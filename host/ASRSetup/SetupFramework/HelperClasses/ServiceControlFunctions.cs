//-----------------------------------------------------------------------
// <copyright file="ServiceControlFunctions.cs" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
// <summary> Class used to control services start/stop.
// </summary>
//-----------------------------------------------------------------------
namespace ASRSetupFramework
{
    using System;
    using System.Collections.Generic;
    using System.ComponentModel;
    using System.Diagnostics;
    using System.Globalization;
    using System.IO;
    using System.Linq;
    using System.Runtime.InteropServices;
    using System.ServiceProcess;
    using System.Threading;

    using Microsoft.Win32;
    using NativeFramework;

    public sealed class ServiceConstants
    {
        public const string KeyNameFormat = @"SYSTEM\CurrentControlSet\Services\{0}";
        public const string ImagePath = @"ImagePath";
    };

    /// <summary>
    /// Class used to control services: Start, stop, etc...
    /// </summary>
    public sealed class ServiceControlFunctions
    {
        public class ServiceControl : IDisposable
        {
            private IntPtr hServiceManager = IntPtr.Zero;

            public IntPtr hServiceControl { get; private set; }

            private string serviceName = null;

            private bool disposed = false;

            public ServiceControl(string svcName)
            {
                serviceName = svcName;
                hServiceControl = IntPtr.Zero;
            }

            public void Dispose()
            {
                Dispose(true);
                GC.SuppressFinalize(this);
            }

            protected virtual void Dispose(bool disposing)
            {
                if (disposed)
                    return;

                //Closing in all cases as we have unmanaged resources.
                Close();

                disposed = true;
            }

            /// <summary>
            /// opens a handle to service.
            /// </summary>
            /// <returns>handle for service</returns>
            public IntPtr Open()
            {
                try
                {
                    Trc.Log(LogLevel.Debug, "opening service {0}", serviceName);
                    uint uiSCMAccess = (uint)NativeMethods.SCM_ACCESS.SC_MANAGER_ALL_ACCESS;

                    uint uiSvcAccess = (uint)NativeMethods.SERVICE_ACCESS.SERVICE_ALL_ACCESS;

                    hServiceManager = NativeWrapper.OpenSCManager(null,
                                                        null,
                                                        uiSCMAccess);
                    if (IntPtr.Zero == hServiceManager)
                    {
                        Trc.Log(LogLevel.Error, "Failed to open Service Control Manager err: {0}", Marshal.GetLastWin32Error());
                        return IntPtr.Zero;
                    }
                    Trc.Log(LogLevel.Always, "Opened SCManager");

                    hServiceControl = NativeWrapper.OpenService(hServiceManager,
                                                        serviceName,
                                                        uiSvcAccess);
                    if (IntPtr.Zero == hServiceControl)
                    {
                        Trc.Log(LogLevel.Error, "Failed to open Service {0} err: {1}",
                                                                    serviceName,
                                                                    Marshal.GetLastWin32Error());
                        return IntPtr.Zero;
                    }

                    Trc.Log(LogLevel.Debug, "opened service {0} successfully", serviceName);
                }
                catch (Exception ex)
                {
                    Trc.Log(LogLevel.Error, "Failed to open Service {0} err: {1}",
                                                                serviceName, ex);
                }
                return hServiceControl;
            }

            /// <summary>
            /// Creates service
            /// </summary>
            /// <param name="displayName">Display Name of Service</param>
            /// <param name="serviceBootFlag">Service Start Type</param>
            /// <param name="serviceAccess">Service Access</param>
            /// <param name="fileName">Binary path for service</param>
            /// <param name="serviceType">Service Type</param>
            /// <param name="LoadOrderGroup">Load Order group for drivers</param>
            /// <returns></returns>
            public IntPtr Create(
                    string displayName,
                    NativeMethods.SERVICE_START_TYPE serviceBootFlag,
                    NativeMethods.SERVICE_ACCESS serviceAccess,
                    string fileName,
                    NativeMethods.SERVICE_TYPE serviceType,
                    string LoadOrderGroup)
            {
                IntPtr tagguid = IntPtr.Zero;

                Trc.Log(LogLevel.Debug, "Creating service {0} start: {1} file: {2}",
                                                    serviceName,
                                                    serviceBootFlag,
                                                    fileName);

                try
                {
                    if (!File.Exists(fileName))
                    {
                        Trc.Log(LogLevel.Error, "ServiceControlFunctions::Create FilePath {0} doesnt exist", fileName);
                        return IntPtr.Zero;
                    }

                    Open();

                    if (IntPtr.Zero == hServiceManager)
                    {
                        Trc.Log(LogLevel.Error, "Failed to open Service Control Manager err: {0}", Marshal.GetLastWin32Error());
                        return IntPtr.Zero;
                    }

                    if (IntPtr.Zero != hServiceControl)
                    {
                        Trc.Log(LogLevel.Always, "Service {0} is already installed.  Skipping installation.", serviceName);
                        return hServiceControl;
                    }

                    Trc.Log(LogLevel.Info, "{0} service doesn't exist already. Installing it.", serviceName);

                    if (null != LoadOrderGroup)
                    {
                        tagguid = Marshal.AllocHGlobal(Marshal.SizeOf(typeof(Int32)));
                        if (IntPtr.Zero == tagguid)
                        {
                            Trc.Log(LogLevel.Error, "ServiceControl Err: Failed to allocate memory for tagguid");
                            return IntPtr.Zero;
                        }

                    }

                    hServiceControl = NativeWrapper.CreateService(
                        hSCManager: hServiceManager,
                        serviceName: serviceName,
                        lpDisplayName: displayName,
                        dwDesiredAccess: serviceAccess,
                        dwServiceType: serviceType,
                        dwStartType: serviceBootFlag,
                        dwErrorControl: NativeMethods.SERVICE_ERROR.SERVICE_ERROR_NORMAL,
                        lpBinaryPathName: fileName,
                        lpLoadOrderGroup: LoadOrderGroup,
                        lpdwTagId: tagguid,
                        lpDependencies: null,
                        lpServiceStartName: null,
                        lpPassword: string.Empty);

                    if (IntPtr.Zero == hServiceControl)
                    {
                        Trc.Log(LogLevel.Error, "Failed to install {0} service err={1}.", serviceName, Marshal.GetLastWin32Error());
                        return IntPtr.Zero;
                    }

                    Trc.Log(LogLevel.Debug, "{0} service has been installed successfully.", serviceName);
                }
                catch (Exception ex)
                {
                    Trc.Log(LogLevel.Error, "CreateService Exception: {0}", ex);
                    return IntPtr.Zero;
                }
                finally
                {
                    if (IntPtr.Zero != tagguid)
                    {
                        Marshal.FreeHGlobal(tagguid);
                    }
                }

                return hServiceControl;
            }

            /// <summary>
            /// Queries service configuration using QueryConfig2
            /// </summary>
            /// <param name="config2Type">Arggument for QueryConfig2</param>
            /// <returns></returns>
            public IntPtr QueryConfig2(NativeMethods.ServiceConfig2Type config2Type)
            {
                // Don't change it if it is already in the desired state (causes exclusive locking in SCM).
                uint dwBytesNeeded = 0;
                IntPtr pQuery2Info = IntPtr.Zero;

                Trc.Log(LogLevel.Debug, "Entering QueryConfig2 service: {0}", serviceName);

                try
                {
                    if (IntPtr.Zero == hServiceControl)
                    {
                        Trc.Log(LogLevel.Error, "QueryConfig2: Calling this function without opening service {0}",
                                            serviceName);
                        return IntPtr.Zero;
                    }

                    if (!NativeMethods.QueryServiceConfig2(hServiceControl,
                                                          config2Type,
                                                          IntPtr.Zero, 0, out dwBytesNeeded))
                    {
                        int error = Marshal.GetLastWin32Error();
                        if (Win32ErrorCodes.ERROR_INSUFFICIENT_BUFFER != error)
                        {
                            Trc.Log(LogLevel.Error, "QueryServiceConfig2 Failed for service {0} error: {1}",
                                                serviceName,
                                                error);
                            return IntPtr.Zero;
                        }

                        Trc.Log(LogLevel.Debug, "QueryConfig2 service: {0} bytes needed: {1}", serviceName, dwBytesNeeded);

                        pQuery2Info = Marshal.AllocHGlobal((int)dwBytesNeeded);
                        if (IntPtr.Zero == pQuery2Info)
                        {
                            Trc.Log(LogLevel.Error, "Failed to allocate memory for QueryServiceConfig2 pStartInfo");
                            return IntPtr.Zero;
                        }

                        if (!NativeMethods.QueryServiceConfig2(hServiceControl,
                                                          config2Type,
                                                          pQuery2Info, (int)dwBytesNeeded, out dwBytesNeeded))
                        {
                            Trc.Log(LogLevel.Error, "QueryServiceConfig2 Failed for service {0} error: {1}",
                                                serviceName,
                                                error);
                            Marshal.FreeHGlobal(pQuery2Info);
                            return IntPtr.Zero;
                        }

                        Trc.Log(LogLevel.Debug, "Successfully retrieved config2 for service {0}", serviceName);
                    }
                }
                catch (Exception ex)
                {
                    Trc.Log(LogLevel.Error, "QueryServiceConfig2 Failed for service {0} exception: {1}",
                                serviceName, ex);
                }
                finally
                {
                    Trc.Log(LogLevel.Debug, "Exiting QueryConfig2 service: {0}", serviceName);
                }

                return pQuery2Info;
            }

            /// <summary>
            /// Closes all open handle for this class
            /// </summary>
            public void Close()
            {
                Trc.Log(LogLevel.Debug, "Entering ServiceControlFunctions::Close service: {0}", serviceName);

                NativeWrapper.CloseService(serviceName, hServiceControl);
                NativeWrapper.CloseSCManager(hServiceManager);

                hServiceControl = IntPtr.Zero;
                hServiceManager = IntPtr.Zero;

                Trc.Log(LogLevel.Debug, "Exiting ServiceControlFunctions::Close");
            }
        };

        /// <summary>
        /// This is the amount of time we will wait for the service
        /// </summary>
        private const int ServiceResponseWaitTimeInMS = 6000;

        /// <summary>
        /// This is the amount of time we will wait for the service
        /// </summary>
        private const int MaxRetryCount = 6;

        /// <summary>
        /// Amount of in between time for the service to Refresh again
        /// </summary>
        private const double ServiceWaitInBetweenTimeSec = 30;

        /// <summary>
        /// Prevents a default instance of the ServiceControlFunctions class from being created.
        /// </summary>
        private ServiceControlFunctions()
        {
            // Do nothing
        }

        /// <summary>
        /// Starts the Driver or, service.
        /// </summary>
        /// <param name="serviceName">Name of the service.</param>
        /// <returns>If service start succeeded.</returns>
        public static bool StartService(string serviceName)
        {
            bool serviceStarted = false;
            bool fatalError = false;
            int loopCount = 0;

            if (!IsInstalled(serviceName))
            {
                Trc.Log(LogLevel.Error, "StartService: Service {0} doesnt exist on this system", serviceName);
                return false;
            }

            using (var svcController = new ServiceController(serviceName))
            {
                try
                {
                    Trc.Log(LogLevel.Debug, "StartService: attempting to start service {0}", serviceName);
                    while (loopCount < MaxRetryCount)
                    {
                        try
                        {
                            if (svcController.Status != ServiceControllerStatus.Running)
                            {
                                Trc.Log(LogLevel.Debug, "Service {0} Status: {1}", serviceName, (uint)svcController.Status);
                                svcController.Start();
                            }

                            break;
                        }
                        catch (InvalidOperationException e)
                        {
                            Trc.Log(LogLevel.Debug, "StartService: Could not get service status. Threw exception Message: {0} StackTrace: {1}.",
                                e.Message,
                                e.StackTrace);
                        }
                        catch (Win32Exception e)
                        {
                            Trc.Log(LogLevel.Debug, "StartService: Could not get service status. Threw exception Message: {0} ErrorCode: {1} StackTrace: {2}.",
                                e.Message,
                                e.ErrorCode,
                                e.StackTrace);
                        }

                        if (loopCount >= MaxRetryCount)
                        {
                            fatalError = true;
                            break;
                        }
                        else
                        {
                            loopCount++;
                            Thread.Sleep(ServiceResponseWaitTimeInMS);
                        }
                    }

                    if (!fatalError)
                    {
                        loopCount = 0;
                        while (svcController.Status != ServiceControllerStatus.Running)
                        {
                            try
                            {
                                if (loopCount >= MaxRetryCount)
                                {
                                    break; // get out of the loop
                                }

                                loopCount++;
                                svcController.Refresh();
                                svcController.WaitForStatus(ServiceControllerStatus.Running, TimeSpan.FromSeconds(ServiceWaitInBetweenTimeSec));
                            }
                            catch (System.ServiceProcess.TimeoutException)
                            {
                                // do nothing
                            }
                            catch (InvalidEnumArgumentException e)
                            {
                                Trc.Log(LogLevel.Error, "StartService: Threw exception Message: {0} StackTrace: {1}.",
                                    e.Message,
                                    e.StackTrace);
                                fatalError = true;
                                break;
                            }
                        }
                    }

                    if (!fatalError)
                    {
                        if (svcController.Status != ServiceControllerStatus.Running)
                        {
                            Trc.Log(LogLevel.Error, "StartService: Unable to start the service {0} after {1} minutes.", serviceName, (loopCount / 2).ToString(CultureInfo.InvariantCulture));
                        }
                        else
                        {
                            Trc.Log(LogLevel.Always, "StartService: Able to start the service {0} after {1} minutes.", serviceName, (loopCount / 2).ToString(CultureInfo.InvariantCulture));
                            serviceStarted = true;
                        }
                    }
                }
                catch (ArgumentException e)
                {
                    Trc.Log(LogLevel.Error, "StartService ArgumentException: Threw exception Message: {0} StackTrace: {1}.",
                        e.Message,
                        e.StackTrace);
                }
                catch (InvalidOperationException e)
                {
                    Trc.Log(LogLevel.Error, "StartService InvalidOperationException: Threw exception Message: {0} StackTrace: {1}.",
                        e.Message,
                        e.StackTrace);
                }
                catch (Win32Exception e)
                {
                    Trc.Log(LogLevel.Error, "StartService Win32Exception: Threw exception Message: {0} StackTrace: {1} ErrorCode: {2}.",
                        e.Message,
                        e.StackTrace,
                        e.ErrorCode);
                }
                catch (Exception e)
                {
                    Trc.Log(LogLevel.Error, "StartService Exception: Threw exception Message: {0} StackTrace: {1}.",
                        e.Message,
                        e.StackTrace);
                }
            }
            return serviceStarted;
        }

        /// <summary>
        /// Stops the service.
        /// </summary>
        /// <param name="serviceName">Name of the service.</param>
        public static bool StopService(string serviceName)
        {
            bool serviceStopped = false;
            int loopCount = 0;

            if (!IsInstalled(serviceName))
            {
                Trc.Log(LogLevel.Error, "StopService: Service {0} doesnt exist on this system", serviceName);
                return false;
            }

            using (ServiceController service = IsServiceInstalled(serviceName))
            {
                if (null == service)
                {
                    return false;
                }

                try
                {
                    Trc.Log(LogLevel.Debug, "StopService: attempting to stop service {0}", serviceName);
                    while (loopCount < MaxRetryCount)
                    {
                        try
                        {
                            if (service.Status != ServiceControllerStatus.Stopped)
                            {
                                service.Stop();
                            }

                            break;
                        }
                        catch (InvalidOperationException e)
                        {
                            if (loopCount >= MaxRetryCount)
                            {
                                Trc.Log(LogLevel.Debug, "InvalidOperationException StopService: Could not get service status. Threw exception Message: {0} StackTrace: {1}.",
                                e.Message,
                                e.StackTrace);
                                return false;
                            }
                            loopCount++;
                            Thread.Sleep(ServiceResponseWaitTimeInMS);
                        }
                    }

                    loopCount = 0;
                    while (service.Status != ServiceControllerStatus.Stopped)
                    {
                        try
                        {
                            if (loopCount >= MaxRetryCount)
                            {
                                break; // get out of the loop
                            }

                            loopCount++;
                            service.Refresh();
                            service.WaitForStatus(ServiceControllerStatus.Stopped, TimeSpan.FromSeconds(ServiceWaitInBetweenTimeSec));
                        }
                        catch (System.ServiceProcess.TimeoutException)
                        {
                            // do nothing
                        }
                        catch (Exception e)
                        {
                            Trc.Log(LogLevel.Debug, "StopService: Threw exception Message: {0} StackTrace: {1}.",
                                e.Message,
                                e.StackTrace);
                        }
                    }

                    Trc.Log(LogLevel.Always, "{0} service status - {1} ", serviceName, service.Status);

                    if (service.Status != ServiceControllerStatus.Stopped)
                    {
                        Trc.Log(LogLevel.Debug, "StopService: Unable to stop the service {0} after {1} minutes.", serviceName, (loopCount / 2).ToString(CultureInfo.InvariantCulture));
                    }
                    else
                    {
                        Trc.Log(LogLevel.Debug, "StopService: Able to stop the service {0} after {1} minutes.", serviceName, (loopCount / 2).ToString(CultureInfo.InvariantCulture));
                        serviceStopped = true;
                    }
                }
                catch (Exception ex)
                {
                    Trc.Log(LogLevel.Error, "StopService: Failed to stop service exception: {0}", ex);
                }
                finally
                {
                    Trc.Log(LogLevel.Debug, "Service {0} {1}", 
                                        serviceName,
                                        serviceStopped? "Stopped Sucessfully" : "Failed to Stop");
                }
            }
            return serviceStopped;
        }

        /// <summary>
        /// This function sets default action of 2 restarts after 5 mins for given service
        /// </summary>
        /// <param name="serviceName">Name of the service</param>
        public static bool SetServiceDefaultFailureActions(string serviceName)
        {
            Trc.Log(LogLevel.Debug, "Entering SetServiceDefaultFailureActions service: {0}", serviceName);
            return SetServiceFailureActions(serviceName, 2, 300000);
        }

        /// <summary>
        /// This function sets service failure actions to given number of restarts with given interval
        /// If number of actions is less than 3, it adds one more action of no Action
        /// </summary>
        /// <param name="serviceName">Name of the Service</param>
        /// <param name="numRestarts">Number of service restarts in case of service crash</param>
        /// <param name="delayInMS">Interval after which service will be restarted</param>
        public static bool SetServiceFailureActions(string serviceName, uint numRestarts, uint delayInMS)
        {
            Trc.Log(LogLevel.Debug, "Entering SetServiceFailureActions service: {0} numRestarts: {1} delayInMS: {2}",
                                                serviceName,
                                                numRestarts,
                                                delayInMS);

            IList<NativeMethods.SC_ACTION> actions = new List<NativeMethods.SC_ACTION>();
            for (int idx = 0; idx < Math.Min(numRestarts, 3); idx++)
            {
                actions.Add(new NativeMethods.SC_ACTION()
                { Type = NativeMethods.SC_ACTION_TYPE.SC_ACTION_RESTART, Delay = delayInMS });
            }

            if (numRestarts < 3)
            {
                actions.Add(new NativeMethods.SC_ACTION() { Type = NativeMethods.SC_ACTION_TYPE.SC_ACTION_NONE, Delay = 0 });
            }

            return SetServiceFailureActions(serviceName, actions);
        }

        /// <summary>
        /// This function sets service failure actions as requested by caller
        /// </summary>
        /// <param name="serviceName">Name of the service</param>
        /// <param name="actions">Actions to be performed in case of service crash</param>
        public static bool SetServiceFailureActions(string serviceName,
                    IList<NativeMethods.SC_ACTION> actions)
        {
            IntPtr lpActions = IntPtr.Zero;
            IntPtr hService = IntPtr.Zero;
            IntPtr lpInfo = IntPtr.Zero;

            Trc.Log(LogLevel.Debug, "Entering SetServiceFailureActions service: {0}", serviceName);
            if (!IsInstalled(serviceName))
            {
                Trc.Log(LogLevel.Error, "SetServiceFailureActions: Service {0} is not installed", serviceName);
                return false;
            }

            bool isSuccessful = false;

            using (ServiceControl serviceControl = new ServiceControl(serviceName))
            {
                try
                {
                    hService = serviceControl.Open();

                    if (hService == IntPtr.Zero)
                    {
                        Trc.Log(LogLevel.Error, "Failed to open service {0} Error: {1}", serviceName, Marshal.GetLastWin32Error());
                        return false;
                    }

                    lpActions = Marshal.AllocHGlobal(Marshal.SizeOf(actions[0]) * actions.Count);
                    if (IntPtr.Zero == lpActions)
                    {
                        Trc.Log(LogLevel.Error, "Failed to allocate memory for service {0} actions", serviceName);
                        return false;
                    }

                    // Set all actions
                    int actionIdx = 0;
                    foreach (var action in actions)
                    {
                        IntPtr nextAction = (IntPtr)(lpActions.ToInt64() + actionIdx * Marshal.SizeOf(action));
                        Marshal.StructureToPtr(action, nextAction, false);
                        actionIdx++;
                    }

                    NativeMethods.SERVICE_FAILURE_ACTIONS failureActions =
                        new NativeMethods.SERVICE_FAILURE_ACTIONS(
                                            0,
                                            "",
                                            "",
                                            (uint)actions.Count,
                                            lpActions);

                    lpInfo = Marshal.AllocHGlobal(Marshal.SizeOf(failureActions));
                    if (IntPtr.Zero == lpInfo)
                    {
                        Trc.Log(LogLevel.Error, "Failed to allocate memory for service {0} failure actions", serviceName);
                        return false;
                    }

                    Marshal.StructureToPtr(failureActions, lpInfo, false);

                    if (!NativeMethods.ChangeServiceConfig2(hService, 2, lpInfo))
                    {
                        Trc.Log(LogLevel.Error, "Failed to change service {0} config.. error: {1}",
                                                            serviceName,
                                                            Marshal.GetLastWin32Error());
                        return false;
                    }
                    Trc.Log(LogLevel.Always, "Service {0} Config Changed Successfully", serviceName);
                    isSuccessful = true;
                }
                catch (Exception ex)
                {
                    Trc.Log(LogLevel.Error, "Exception: {0}", ex);
                }
                finally
                {
                    if (IntPtr.Zero != lpActions)
                    {
                        Marshal.FreeHGlobal(lpActions);
                    }

                    if (IntPtr.Zero == lpInfo)
                    {
                        Marshal.FreeHGlobal(lpInfo);
                    }
                    Trc.Log(LogLevel.Debug, "Exiting SetServiceFailureActions service: {0}", serviceName);
                }
            }
            return isSuccessful;
        }

        /// <summary>
        /// Change the service startup type.
        /// </summary>
        /// <param name="serviceName">Name of the service.</param>
        public static bool ChangeServiceStartupType(string serviceName, NativeMethods.SERVICE_START_TYPE startupType)
        {
            Trc.Log(LogLevel.Debug, "ChangeServiceStartupType: Changing service startup type {0} - {1}", serviceName, startupType.ToString());

            IntPtr hService = IntPtr.Zero;

            if (!IsInstalled(serviceName))
            {
                Trc.Log(LogLevel.Error, "ChangeServiceStartupType: Service {0} is not installed", serviceName);
                return false;
            }

            using (ServiceControl serviceControl = new ServiceControl(serviceName))
            {
                try
                {
                    hService = serviceControl.Open();
                    if (IntPtr.Zero == hService)
                    {
                        Trc.Log(LogLevel.Error, "Failed to open service {0} err={1}", serviceName, Marshal.GetLastWin32Error());
                        return false;
                    }

                    if (!NativeMethods.ChangeServiceConfig(hService, NativeMethods.SERVICE_NO_CHANGE, (uint)startupType, NativeMethods.SERVICE_NO_CHANGE, IntPtr.Zero, IntPtr.Zero, IntPtr.Zero, IntPtr.Zero, IntPtr.Zero, IntPtr.Zero, IntPtr.Zero))
                    {
                        Trc.Log(LogLevel.Error, "Failed to change service {0} to type {1} err={2}", serviceName, startupType.ToString(), Marshal.GetLastWin32Error());
                        return false;
                    }

                    Trc.Log(LogLevel.Always, "Changed service {0} startup type to {1}", serviceName, startupType.ToString());
                }
                catch (Exception ex)
                {
                    Trc.Log(LogLevel.Error,
                        "Exception at service startup type change: {0} {1}",
                        ex.Message,
                        ex.StackTrace);
                    return false;
                }
                finally
                {
                    Trc.Log(LogLevel.Debug, "Exit: Changing service startup type {0} - {1}", serviceName, startupType.ToString());
                }
            }
            return true;
        }

        /// <summary>
        /// Get the process id of the running service.
        /// </summary>
        /// <param name="serviceName">Name of the service.</param>
        public static uint GetProcessID(string serviceName)
        {
            Trc.Log(LogLevel.Debug, "Entering GetProcessID for service {0}", serviceName);

            uint procId = 0;

            IntPtr hService = IntPtr.Zero;
            UInt32 dwBytes = 0;
            IntPtr lpBuffer = IntPtr.Zero;
            NativeMethods.SERVICE_STATUS_PROCESS serviceStatus = new NativeMethods.SERVICE_STATUS_PROCESS();

            using (ServiceControl serviceControl = new ServiceControl(serviceName))
            {
                try
                {
                    hService = serviceControl.Open();
                    if (IntPtr.Zero == hService)
                    {
                        Trc.Log(LogLevel.Debug, "GetProcessID: Failed to open service {0}", serviceName);
                        return 0;
                    }

                    if (!NativeMethods.QueryServiceStatusEx(hService, NativeMethods.SC_STATUS_TYPE.SC_STATUS_PROCESS_INFO,
                                                        IntPtr.Zero, 0, out dwBytes))
                    {
                        int error = Marshal.GetLastWin32Error();
                        if (Win32ErrorCodes.ERROR_INSUFFICIENT_BUFFER != error)
                        {
                            Trc.Log(LogLevel.Error, "QueryServiceStatusEx failed for service: {0} error: {1}",
                                    serviceName,
                                    error);
                            return procId;
                        }

                        Trc.Log(LogLevel.Debug, "GetProcessID service {0}: Allocating {1} bytes for lpBuffer", serviceName, dwBytes);

                        lpBuffer = Marshal.AllocHGlobal((int)dwBytes);
                        if (IntPtr.Zero == lpBuffer)
                        {
                            Trc.Log(LogLevel.Error, "GetProcessID: Failed to allocate lpBuffer");
                            return procId;
                        }

                        if (!NativeMethods.QueryServiceStatusEx(hService, NativeMethods.SC_STATUS_TYPE.SC_STATUS_PROCESS_INFO,
                                                            lpBuffer, dwBytes, out dwBytes))
                        {
                            Trc.Log(LogLevel.Error, "GetProcessID: QueryServiceStatusEx for service {0} failed with error={1}",
                                                                            serviceName, Marshal.GetLastWin32Error());
                            return procId;
                        }

                        Trc.Log(LogLevel.Debug, "GetProcessID service {0}: received {1} bytes for serviceStatusProcess", serviceName, dwBytes);
                        Marshal.PtrToStructure(lpBuffer, serviceStatus);

                        procId = (uint)serviceStatus.dwProcessId;
                        Trc.Log(LogLevel.Info, "Service: {0} ProcessId: {1}", serviceName, procId);
                    }
                }
                catch (Exception ex)
                {
                    Trc.Log(LogLevel.Error, "GetProcessID Exception: {0}", ex);
                }
                finally
                {
                    if (IntPtr.Zero != lpBuffer)
                    {
                        Marshal.FreeHGlobal(lpBuffer);
                    }
                    Trc.Log(LogLevel.Debug, "Exiting GetProcessID for service {0}", serviceName);
                }
            }

            return procId;
        }

        /// <summary>
        /// Kill the process associated with pid.
        /// </summary>
        /// <param name="procId">Pid of the process to be terminated</param>
        /// <returns>true on sucessfull termination of process, else returns false</returns>
        public static bool KillProcess(uint procId)
        {
            Process process = null;

            // Get process id.
            try
            {
                process = Process.GetProcessById((int)procId);
                Trc.Log(LogLevel.Always, "process: {0}", process.ToString());
                Trc.Log(LogLevel.Always, "process.Id: {0}", process.Id);
            }
            catch (Exception ex)
            {
                Trc.LogException(LogLevel.Error,
                    "Exception at KillProcess during GetProcessById: {0}",
                    ex);
                return false;
            }

            // Kill the process.
            try
            {
                if (process != null && process.Id > 0)
                {
                    process.Kill();
                }
            }
            catch (Exception ex)
            {
                Trc.LogException(LogLevel.Error,
                    "Exception at KillProcess during Kill: {0}",
                    ex);
                return false;
            }

            return true;
        }

        /// <summary>
        /// Kills the service process.
        /// </summary>
        /// <param name="serviceName">Name of the service.</param>
        /// <returns>true on sucessfull termination of process, else returns false</returns>
        public static bool KillServiceProcess(string serviceName)
        {
            Trc.Log(LogLevel.Always, "Begin KillServiceProcess for service : {0}", serviceName);

            bool status = false;

            int retryCount = 3;
            int loopCount = 0;
            while (loopCount < retryCount)
            {
                uint pid = GetProcessID(serviceName);
                if (pid != 0)
                {
                    Trc.Log(LogLevel.Always, "Kill {0} service count : {1}", serviceName, loopCount);
                    KillProcess(pid);
                    loopCount++;
                    Thread.Sleep(ServiceResponseWaitTimeInMS);
                }
                else
                {
                    Trc.Log(LogLevel.Always, "Successfully terminated {0} service", serviceName);
                    status = true;
                    break;
                }
            }

            uint procId = GetProcessID(serviceName);
            if (procId != 0)
            {
                Trc.Log(LogLevel.Error, "Unable to kill {0} service with PID : {1}", serviceName, procId);
                status = false;
            }

            return status;
        }

        #region IsInstalled & IsEnabled Methods

        /// <summary>
        /// Is the specified service installed on the local machine?
        /// </summary>
        /// <param name="serviceName">Name of the service.</param>

        /// <returns>
        /// <c>true</c> if the specified service/driver name is installed; otherwise, <c>false</c>.
        /// </returns>
        public static bool IsInstalled(string serviceName)
        {
            ServiceController scm = IsServiceInstalled(serviceName);
            bool isInstalled = (null != scm);

            if (null != scm)
            {
                scm.Dispose();
            }

            return isInstalled;
        }

        /// <summary>
        /// Is the specified service installed on the local machine?
        /// </summary>
        /// <param name="serviceName">Name of the service.</param>
        /// <returns>
        /// <c>true</c> if the specified service name is installed; otherwise, <c>false</c>.
        /// </returns>
        public static ServiceController IsServiceInstalled(string serviceName)
        {
            ServiceController scm = null;
            try
            {
                scm = new ServiceController(serviceName);
                Trc.Log(LogLevel.Always, "service {0} Status: {1}", serviceName, scm.Status);
                return scm;
            }
            // Ideally if service doesn't exist it should get Invalid Operation exception
            catch (System.InvalidOperationException ex)
            {
                Trc.Log(LogLevel.Debug, "InvalidOperationException: Message: {0} StackTrace: {1} ErrorCode: {2}.",
                     ex.Message,
                     ex.StackTrace,
                     ex.InnerException);
                var w32ex = ex.InnerException as System.ComponentModel.Win32Exception;
                if (null != w32ex)
                {
                    Trc.Log(LogLevel.Debug, "Win32Exception ErrorCode: {0}", Marshal.GetLastWin32Error());
                }
            }
            // Rest of exceptions are handled as service doesnt exist
            catch (System.ComponentModel.Win32Exception ex)
            {
                Trc.Log(LogLevel.Error, "Win32Exception: Message: {0} StackTrace: {1} ErrorCode: {2}.",
                     ex.Message,
                     ex.StackTrace,
                     ex.ErrorCode);
            }
            catch (Exception ex)
            {
                Trc.Log(LogLevel.Error, "Exception Message: {0} Stack Trace: {1}", ex.Message, ex.StackTrace);
                Trc.Log(LogLevel.Error, "\nexception: " + ex);
            }

            return null;
        }

        /// <summary>
        /// Tells if the service is enabled and in desired state
        /// </summary>
        /// <param name="serviceName">Name of service</param>
        /// <param name="state">Service state (Running/Stopped).</param>
        /// <returns>true is service is enabled and running, false otherwise.</returns>
        public static bool IsEnabledAndInDesiredState(string serviceName, ServiceControllerStatus state)
        {
            bool isEnabledAndDesiredState = false;
            ServiceController service = IsServiceInstalled(serviceName);

            if (null != service)
            {
                Trc.Log(LogLevel.Always,
                    "{0} service status - {1}",
                    service.ServiceName,
                    service.Status);
                if (IsEnabled(serviceName) &&
                    service.Status == state)
                {
                    isEnabledAndDesiredState = true;
                }

                service.Close();
            }

            return isEnabledAndDesiredState;
        }

        /// <summary>
        /// Tells if the service is enabled and stopped
        /// </summary>
        /// <param name="serviceName">Name of service</param>
        /// <returns>trre is service is enabled and running, false otherwise.</returns>
        public static bool IsEnabledAndStopped(string serviceName)
        {
            return IsEnabledAndInDesiredState(serviceName, ServiceControllerStatus.Stopped);
        }

        /// <summary>
        /// Tells if the service is enabled and running
        /// </summary>
        /// <param name="serviceName">Name of service</param>
        /// <returns>trre is service is enabled and running, false otherwise.</returns>
        public static bool IsEnabledAndRunning(string serviceName)
        {
            return IsEnabledAndInDesiredState(serviceName, ServiceControllerStatus.Running);
        }

        /// <summary>
        /// Is the specified service enabled on the local machine?
        /// </summary>
        /// <param name="serviceName">Name of the service.</param>
        /// <returns>
        /// <c>true</c> if the specified service name is enabled; otherwise, <c>false</c>.
        /// </returns>
        public static bool IsEnabled(string serviceName)
        {
            bool isEnabled = false;

            Trc.Log(LogLevel.Debug, "Entering IsEnabled service: {0}", serviceName);

            if (!IsInstalled(serviceName))
            {
                Trc.Log(LogLevel.Info, "service: {0} is not installed", serviceName);
                return false;
            }

            IntPtr hServiceControl = IntPtr.Zero;
            NativeMethods.ServiceConfiguration serviceConfiguration = null;

            using (ServiceControl serviceControl = new ServiceControl(serviceName))
            {
                try
                {
                    hServiceControl = serviceControl.Open();
                    if (IntPtr.Zero == hServiceControl)
                    {
                        Trc.Log(LogLevel.Error, "SetDescription: Failed to open service {0} with err={1}",
                                        serviceName, Marshal.GetLastWin32Error());
                        return false;
                    }
                    serviceConfiguration = GetConfiguration(hServiceControl);
                    Trc.Log(LogLevel.Always,
                            "start type - {0}",
                            serviceConfiguration.StartType);

                    isEnabled = ((int)NativeMethods.SERVICE_START_TYPE.SERVICE_DISABLED != serviceConfiguration.StartType);
                }
                catch (Exception ex)
                {
                    Trc.Log(LogLevel.Error, "IsEnabled: serviceName: {0} exception: {1}", serviceName,
                                ex);
                }
                finally
                {
                    Trc.Log(LogLevel.Debug, "Exiting IsEnabled for service {0}", serviceName);
                }
            }
            return isEnabled;
        }

        #endregion

        #region Service Install/Uninstall/Fixup Methods.

        /// <summary>
        /// Method to fix registry image path.
        /// Security team has raised concerns that ImagePath of inmage service
        /// dont start with quotes. This is a security concern.
        /// For new installation, this is already taken care
        /// For existing installation we need to update path with
        /// proper quotes.
        /// This function achieves that goal.
        /// </summary>
        /// <param name="serviceName"></param>
        internal static void FixImagePath(string serviceName)
        {
            try
            {
                if (string.IsNullOrEmpty(serviceName) || (0 == serviceName.Trim().Length))
                {
                    Trc.Log(LogLevel.Error, "FixImagePath: Service name is null or empty");
                    return;
                }

                string servicePath = string.Format(ServiceConstants.KeyNameFormat, serviceName);
                var reg = Registry.LocalMachine.OpenSubKey(servicePath, true);
                if (null == reg)
                {
                    Trc.Log(LogLevel.Error, "Service {0} doesn't exist", servicePath);
                    return;
                }

                string imagePath = (string)reg.GetValue(ServiceConstants.ImagePath);
                if (string.IsNullOrEmpty(imagePath) || (0 == imagePath.Trim().Length))
                {
                    Trc.Log(LogLevel.Error, "FixImagePath: ImagePath is null or empty");
                    return;
                }

                Trc.Log(LogLevel.Debug,
                        string.Format("FixImagePath: Imagepath for service {0} is {1}", serviceName, imagePath));

                if (!imagePath.StartsWith("\""))
                {
                    string path = "\"" + imagePath + "\"";
                    Trc.Log(LogLevel.Always,
                            string.Format("FixImagePath: Updating Imagepath for service {0} with {1}", serviceName, path));

                    reg.SetValue(ServiceConstants.ImagePath, path);
                }
            }
            catch (Exception ex)
            {
                Trc.LogException(LogLevel.Error, "FixImagePath failed with exception: ", ex);
            }
            return;
        }

        /// <summary>
        /// Installs the Driver.
        /// </summary>
        /// <param name="driverName">Name of the Driver.</param>
        /// <param name="displayName">Display name of the service.</param>
        /// <param name="serviceBootFlag">Service start type.</param>
        /// <param name="serviceAccess">Desired access to the service.</param>
        /// <param name="fileName">Service executable absolute path.</param>
        /// <param name="loadOrderGroup">Loader order Group for driver</param>
        /// <returns>true when it is able to create the service, false otherwise.</returns>
        public static bool InstallDriver(
            string driverName,
            string displayName,
            NativeMethods.SERVICE_START_TYPE serviceBootFlag,
            string fileName,
            string loadOrderGroup)
        {
            Trc.Log(LogLevel.Debug, "Entering InstallDriver");
            ServiceController svcController = null;
            try
            {
                svcController = ServiceControlFunctions.IsServiceInstalled("indskflt");
                if (null != svcController)
                {
                    Trc.Log(LogLevel.Always, "{0} is already installed..Exiting", driverName);
                    return true;
                }
            }
            catch (Exception ex)
            {
                Trc.Log(LogLevel.Error, "IsServiceInstalled failed with Exception = {0}", ex);
            }
            finally
            {
                if (null != svcController)
                {
                    svcController.Dispose();
                }
            }

            Trc.Log(LogLevel.Always, "Installating Driver {0} Start: {1} Path: {2}",
                                                                        driverName,
                                                                        serviceBootFlag.ToString(),
                                                                        fileName);
            return InstallService(driverName,
                                displayName,
                                serviceBootFlag,
                                NativeMethods.SERVICE_ACCESS.SERVICE_ALL_ACCESS,
                                fileName,
                                NativeMethods.SERVICE_TYPE.SERVICE_KERNEL_DRIVER,
                                loadOrderGroup);
        }

        /// <summary>
        /// Installs the service.
        /// </summary>
        /// <param name="serviceName">Name of the Service.</param>
        /// <param name="displayName">Display name of the service.</param>
        /// <param name="serviceBootFlag">Service start type.</param>
        /// <param name="serviceAccess">Desired access to the service.</param>
        /// <param name="fileName">Service executable absolute path.</param>
        /// <returns>true when it is able to create the service, false otherwise.</returns>
        public static bool InstallService(
            string serviceName,
            string displayName,
            NativeMethods.SERVICE_START_TYPE serviceBootFlag,
            string fileName)
        {
            Trc.Log(LogLevel.Debug, "Entering InstallService");

            if (IsInstalled(serviceName))
            {
                Trc.Log(LogLevel.Always, "{0} is already installed", serviceName);
                return true;
            }

            Trc.Log(LogLevel.Always, "Installating Service {0} Start: {1} Path: {2}",
                                                                                serviceName,
                                                                                serviceBootFlag.ToString(),
                                                                                fileName);

            return InstallService(serviceName,
                                displayName,
                                serviceBootFlag,
                                NativeMethods.SERVICE_ACCESS.SERVICE_ALL_ACCESS,
                                fileName,
                                NativeMethods.SERVICE_TYPE.SERVICE_WIN32_OWN_PROCESS,
                                null);
        }

        /// <summary>
        /// Installs the service.
        /// </summary>
        /// <param name="serviceName">Name of the service.</param>
        /// <param name="displayName">Display name of the service.</param>
        /// <param name="serviceBootFlag">Service start type.</param>
        /// <param name="serviceAccess">Desired access to the service.</param>
        /// <param name="fileName">Service executable absolute path.</param>
        /// <param name="LoadOrderGroup">The names of the load ordering group of which this service is a member</param>
        /// <param name="serviceType">service type like kernel driver</param>
        ///                             
        /// <returns>true when it is able to create the service, false otherwise.</returns>
        internal static bool InstallService(
                    string serviceName,
                    string displayName,
                    NativeMethods.SERVICE_START_TYPE serviceBootFlag,
                    NativeMethods.SERVICE_ACCESS serviceAccess,
                    string fileName,
                    NativeMethods.SERVICE_TYPE serviceType,
                    string LoadOrderGroup)
        {
            bool serviceInstalled = false;
            IntPtr hService = IntPtr.Zero;
            IntPtr tagguid = IntPtr.Zero;

            Trc.Log(LogLevel.Debug, "Entering InstallService: Service {0} displayName {1} bootFlag {2} access {3} file: {4} type: {5} loadOrder: {6}",
                                    serviceName, displayName, (int)serviceBootFlag,
                                    (int)serviceAccess, fileName, serviceType, LoadOrderGroup);

            if (!File.Exists(fileName))
            {
                Trc.Log(LogLevel.Error, "InstallService: File {0} doesnt exist.. exiting", fileName);
                return false;
            }

            using (ServiceControl serviceControl = new ServiceControl(serviceName))
            {

                try
                {
                    hService = serviceControl.Create(
                            displayName,
                            serviceBootFlag,
                            serviceAccess,
                            fileName,
                            serviceType,
                            LoadOrderGroup);

                    serviceInstalled = (IntPtr.Zero != hService);
                }
                catch (Exception ex)
                {
                    Trc.Log(LogLevel.Debug, "Exception occured while installing {0} service.", serviceName);
                    Trc.Log(LogLevel.Debug, "Message: {0} StackTrace: {1}.", ex.Message, ex.StackTrace);
                }
                finally
                {
                    Trc.Log(LogLevel.Debug, "Exiting InstallService: Service {0} displayName {1} bootFlag {2} access {3} file: {4} type: {5} loadOrder: {6}",
                                            serviceName, displayName, (int)serviceBootFlag, (int)serviceAccess, fileName, serviceType, LoadOrderGroup);
                }
            }
            return serviceInstalled;
        }

        // <summary>
        // Uninstalls service.
        // </summary>
        // <param name="serviceName">Service name.</param>/// 
        // <returns>true if it is able uninstall the service, false otherwise.</returns>
        public static bool UninstallService(string serviceName)
        {
            Trc.Log(LogLevel.Debug, "Entering UninstallService service: {0}", serviceName);

            bool serviceUninstalled = false;

            if (!IsInstalled(serviceName))
            {
                Trc.Log(LogLevel.Info, "Service {0} is not installed", serviceName);
                return true;
            }

            IntPtr hService = IntPtr.Zero;

            using (ServiceControl serviceControl = new ServiceControl(serviceName))
            {
                try
                {
                    hService = serviceControl.Open();

                    if (IntPtr.Zero == hService)
                    {
                        Trc.Log(LogLevel.Always, "{0} service doesn't exist, skipping uninstallation.", serviceName);
                        return true;
                    }

                    if (!NativeMethods.DeleteService(hService))
                    {
                        Trc.Log(LogLevel.Error, "Failed to uninstall {0} service err: {1}", serviceName,
                                                                Marshal.GetLastWin32Error());
                        return false;
                    }

                    Trc.Log(LogLevel.Always, "Successfully uninstalled {0} service.", serviceName);
                    serviceUninstalled = true;
                }
                catch (Exception ex)
                {
                    Trc.Log(LogLevel.Debug, "Exception occured while uninstalling {0} service.", serviceName);
                    Trc.Log(LogLevel.Debug, "Message: {0} StackTrace: {1}.", ex.Message, ex.StackTrace);
                }
                finally
                {
                    Trc.Log(LogLevel.Debug, "Exiting UninstallService service: {0}", serviceName);
                }
            }
            return serviceUninstalled;
        }

        #endregion 

        #region Service Settings Methods

        /// <summary>
        /// Sets the delayed auto start property of the given service.
        /// </summary>
        /// <param name="serviceName">Name of the service.</param>
        /// <param name="delayed">if set to <c>true</c> [delayed].</param>
        /// <returns>true when it is able to set delayed auto start property, false otherwise.</returns>
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Microsoft.Security", "CA2122:DoNotIndirectlyExposeMethodsWithLinkDemands", Justification = "Native methods being imported."), System.Diagnostics.CodeAnalysis.SuppressMessage("Microsoft.Interoperability", "CA1404:CallGetLastErrorImmediatelyAfterPInvoke", Justification = "Native methods being imported.")]
        public static bool SetDelayedAutoStart(string serviceName, bool delayed)
        {
            Debug.Assert(string.IsNullOrEmpty(serviceName) == false, "string.IsNullOrEmpty(serviceName) == false");

            Trc.Log(LogLevel.Debug, "Entering SetDelayedAutoStart service {0} delayed {1}", serviceName, delayed);

            if (!IsInstalled(serviceName))
            {
                Trc.Log(LogLevel.Error, "Service {0} is not installed", serviceName);
                return false;
            }

            bool changeConfigResult = false;

            IntPtr hServiceControl = IntPtr.Zero;
            NativeMethods.SERVICE_DELAYED_AUTO_START_INFO startInfo = new NativeMethods.SERVICE_DELAYED_AUTO_START_INFO();
            IntPtr pStartInfo = IntPtr.Zero;
            using (ServiceControl serviceControl = new ServiceControl(serviceName))
            {
                try
                {
                    hServiceControl = serviceControl.Open();
                    if (IntPtr.Zero == hServiceControl)
                    {
                        Trc.Log(LogLevel.Error, "SetDelayedAutoStart: Failed to open service {0} err={1}",
                                serviceName, Marshal.GetLastWin32Error());
                        return false;
                    }

                    var config = GetConfiguration(hServiceControl);
                    if (null == config)
                    {
                        Trc.Log(LogLevel.Error, "Failed to get service {0} configuration err={1}", serviceName, Marshal.GetLastWin32Error());
                        return false;
                    }

                    if (config.ServiceType != (uint)NativeMethods.SERVICE_START_TYPE.SERVICE_AUTO_START)
                    {
                        Trc.Log(LogLevel.Info, "Service startup type is not automatic. Changing it to automatic");

                        if (!NativeMethods.ChangeServiceConfig(hServiceControl,
                                                NativeMethods.SERVICE_NO_CHANGE,
                                                (uint)NativeMethods.SERVICE_START_TYPE.SERVICE_AUTO_START,
                                                NativeMethods.SERVICE_NO_CHANGE, IntPtr.Zero, IntPtr.Zero, IntPtr.Zero, IntPtr.Zero, IntPtr.Zero, IntPtr.Zero, IntPtr.Zero))
                        {
                            Trc.Log(LogLevel.Error, "Failed to change service {0} to type automatic err={1}", serviceName, Marshal.GetLastWin32Error());
                            return false;
                        }
                    }


                    pStartInfo = serviceControl.QueryConfig2(NativeMethods.ServiceConfig2Type.DelayedAutoStartInfo);
                    if (IntPtr.Zero == pStartInfo)
                    {
                        Trc.Log(LogLevel.Error, "SetDelayedAutoStart: Failed to query service config2 for service {0}",
                                    serviceName);
                        return false;
                    }

                    Marshal.PtrToStructure(pStartInfo, typeof(NativeMethods.SERVICE_DELAYED_AUTO_START_INFO));

                    if (delayed == startInfo.DelayedAutostart)
                    {
                        Trc.Log(LogLevel.Always, "SetDelayedAutoStart: DelayedAutostart is already set to {0} for service {1}. Skipping configuration.",
                                        delayed, serviceName);
                        return true;
                    }

                    startInfo.DelayedAutostart = delayed;
                    Marshal.StructureToPtr(startInfo, pStartInfo, false);
                    if (!NativeMethods.ChangeServiceConfig2(hServiceControl, 3, pStartInfo))
                    {
                        Trc.Log(LogLevel.Error, "SetDelayedAutoStart: Failed to set DelayedAutostart property for {0} service err={1}",
                                                        serviceName, Marshal.GetLastWin32Error());
                        return false;
                    }

                    Trc.Log(LogLevel.Debug, "Successfully set DelayedAutostart property for {0} service.", serviceName);
                    changeConfigResult = true;
                }
                catch (Win32Exception ex)
                {
                    Trc.Log(LogLevel.Debug, "Exception occured while changing configuration of {0} service.", serviceName);
                    Trc.Log(LogLevel.Debug, "Message: {0} StackTrace: {1} ErrorCode: {2}.",
                        ex.Message,
                        ex.StackTrace,
                        ex.ErrorCode);
                }
                catch (Exception ex)
                {
                    Trc.Log(LogLevel.Debug, "Exception occured while changing configuration of {0} service.", serviceName);
                    Trc.Log(LogLevel.Debug, "Message: {0} StackTrace: {1}.", ex.Message, ex.StackTrace);
                }
                finally
                {
                    if (IntPtr.Zero != pStartInfo)
                    {
                        Marshal.FreeHGlobal(pStartInfo);
                    }
                    Trc.Log(LogLevel.Debug, "Exiting SetDelayedAutoStart service {0} delayed {1}", serviceName, delayed);
                }
            }
            return changeConfigResult;
        }

        /// <summary>
        /// Sets the description of the given service.
        /// </summary>
        /// <param name="serviceName">Name of the service.</param>
        /// <param name="description">Description of the service.</param>
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Microsoft.Security", "CA2122:DoNotIndirectlyExposeMethodsWithLinkDemands", Justification = "Native methods being imported."), System.Diagnostics.CodeAnalysis.SuppressMessage("Microsoft.Interoperability", "CA1404:CallGetLastErrorImmediatelyAfterPInvoke", Justification = "Native methods being imported.")]
        public static bool SetDescription(string serviceName, string description)
        {
            Debug.Assert(string.IsNullOrEmpty(serviceName) == false, "string.IsNullOrEmpty(serviceName) == false");

            Trc.Log(LogLevel.Debug, "Entering SetDescription service: {0} Desc: {1}", serviceName, description);

            if (!IsInstalled(serviceName))
            {
                Trc.Log(LogLevel.Error, "Service {0} is not installed", serviceName);
                return false;
            }

            bool changeDescResult = false;
            IntPtr hServiceControl = IntPtr.Zero;
            NativeMethods.SERVICE_DESCRIPTION_INFO descInfo = new NativeMethods.SERVICE_DESCRIPTION_INFO();
            IntPtr pDescInfo = IntPtr.Zero;
            using (ServiceControl serviceControl = new ServiceControl(serviceName))
            {
                try
                {
                    hServiceControl = serviceControl.Open();
                    if (IntPtr.Zero == hServiceControl)
                    {
                        Trc.Log(LogLevel.Error, "SetDescription: Failed to open service {0} with err={1}",
                                        serviceName, Marshal.GetLastWin32Error());
                        return false;
                    }

                    pDescInfo = serviceControl.QueryConfig2(NativeMethods.ServiceConfig2Type.Description);
                    if (IntPtr.Zero == pDescInfo)
                    {
                        Trc.Log(LogLevel.Error, "SetDescription: Failed to query config for service {0}", serviceName);
                        return false;
                    }

                    Marshal.PtrToStructure(pDescInfo, typeof(NativeMethods.SERVICE_DESCRIPTION_INFO));
                    descInfo.ServiceDescription = description;

                    Marshal.StructureToPtr(descInfo, pDescInfo, false);

                    if (!NativeMethods.ChangeServiceConfig2(hServiceControl,
                                    (int)NativeMethods.ServiceConfig2Type.Description,
                                    pDescInfo))
                    {
                        Trc.Log(LogLevel.Debug, "SetDescription: Failed to change description of {0} service err={1}.",
                                serviceName, Marshal.GetLastWin32Error());
                        return false;
                    }
                    Trc.Log(LogLevel.Always, "SetDescription: Successfully changed description of {0} service.", serviceName);
                    changeDescResult = true;
                }
                catch (Exception ex)
                {
                    Trc.Log(LogLevel.Debug, "Exception occured while changing description of {0} service.", serviceName);
                    Trc.Log(LogLevel.Debug, "Message: {0} StackTrace: {1}.", ex.Message, ex.StackTrace);
                }
                finally
                {
                    if (IntPtr.Zero != pDescInfo)
                    {
                        Marshal.FreeHGlobal(pDescInfo);
                    }
                    Trc.Log(LogLevel.Debug, "Exiting SetDescription service: {0} Desc: {1}", serviceName, description);
                }
            }
            return changeDescResult;
        }


        /// <summary>
        /// Get service configuration.
        /// </summary>
        /// <param name="serviceHandle">Service handle.</param>
        /// <returns>Service configuration.</returns>
        [System.Diagnostics.CodeAnalysis.SuppressMessage("Microsoft.Security", "CA2122:DoNotIndirectlyExposeMethodsWithLinkDemands", Justification = "Native methods being imported.")]
        private static NativeMethods.ServiceConfiguration GetConfiguration(IntPtr serviceHandle)
        {
            // Allocate memory for returned struct.
            IntPtr memoryPtr = IntPtr.Zero;
            try
            {
                memoryPtr = Marshal.AllocHGlobal(4096);

                uint bytesNeeded = 0;
                if (NativeMethods.QueryServiceConfig(serviceHandle, memoryPtr, 4096, out bytesNeeded))
                {
                    NativeMethods.ServiceConfiguration serviceConfiguration = new NativeMethods.ServiceConfiguration();
                    Marshal.PtrToStructure(memoryPtr, serviceConfiguration);
                    Trc.Log(LogLevel.Debug, "Service: {0} StartType: {1}", serviceConfiguration.ServiceStartName, serviceConfiguration.StartType);
                    return serviceConfiguration;
                }

                return null;
            }
            finally
            {
                if (IntPtr.Zero != memoryPtr)
                {
                    Marshal.FreeHGlobal(memoryPtr);
                }
            }
        }

        #endregion
    }
}
