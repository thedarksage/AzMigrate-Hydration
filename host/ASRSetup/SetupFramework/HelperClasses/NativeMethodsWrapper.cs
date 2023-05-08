//-----------------------------------------------------------------------
// <copyright file="NativeMethodsWrapper.cs" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
// <summary> Class used to manage open and create service. It records
//           open, close and create handles.
// </summary>
//-----------------------------------------------------------------------

using System;
using System.Collections.Generic;
using System.Diagnostics;

namespace ASRSetupFramework
{
    public class NativeWrapper : NativeMethods
    {
        private static IDictionary<string, int> s_serviceHandleMap = new Dictionary<string, int>();
        private static int s_scMangerHandleCount = 0;

        /// <summary>
        /// Calls into native method CreateService
        /// </summary>
        /// <param name="hSCManager">Handle to service Manager.</param>
        /// <param name="serviceName">Service Name.</param>
        /// <param name="lpDisplayName">Service Display Name.</param>
        /// <param name="dwDesiredAccess">Desired Access.</param>
        /// <param name="dwServiceType">Service Type.</param>
        /// <param name="dwStartType">Service Start Type.</param>
        /// <param name="dwErrorControl">Error Control.</param>
        /// <param name="lpBinaryPathName">Binary Path.</param>
        /// <param name="lpLoadOrderGroup">Loadorder group for driver.</param>
        /// <param name="lpdwTagId">Tag.</param>
        /// <param name="lpDependencies">Service Dependencies.</param>
        /// <param name="lpServiceStartName">Service Start Name.</param>
        /// <param name="lpPassword">Password.</param>
        /// <returns>Returns handle of opened service.</returns>
        public new static IntPtr CreateService(IntPtr hSCManager,
                                                    string serviceName,
                                                    string lpDisplayName,
                                                    NativeMethods.SERVICE_ACCESS dwDesiredAccess,
                                                    NativeMethods.SERVICE_TYPE dwServiceType,
                                                    NativeMethods.SERVICE_START_TYPE dwStartType,
                                                    NativeMethods.SERVICE_ERROR dwErrorControl,
                                                    string lpBinaryPathName,
                                                    string lpLoadOrderGroup,
                                                    IntPtr lpdwTagId,
                                                    string lpDependencies,
                                                    string lpServiceStartName,
                                                    string lpPassword)
        {
            Trc.Log(LogLevel.Debug, "Create service: {0}", serviceName);

            if (!s_serviceHandleMap.ContainsKey(serviceName))
            {
                s_serviceHandleMap[serviceName] = 0;
            }

            var stopWatch = new System.Diagnostics.Stopwatch();

            stopWatch.Start();
            var hService = NativeMethods.CreateService(hSCManager,
                                               serviceName,
                                               lpDisplayName,
                                               dwDesiredAccess,
                                               dwServiceType,
                                               dwStartType,
                                               dwErrorControl,
                                               lpBinaryPathName,
                                               lpLoadOrderGroup,
                                               lpdwTagId,
                                               lpDependencies,
                                               lpServiceStartName,
                                               lpPassword);
            stopWatch.Stop();
            if (IntPtr.Zero != hService)
            {
                ++s_serviceHandleMap[serviceName];
            }

            Trc.Log(LogLevel.Debug, "Time taken by CreateService service: {0} time: {1}ms", serviceName, stopWatch.ElapsedMilliseconds);

            Trc.Log(LogLevel.Debug, "SeviceHandle Info: Service: {0} handle count = {1}",
                                                                serviceName, s_serviceHandleMap[serviceName]);

            return hService;
        }

        /// <summary>
        /// Wrapper for Native OpenService Method.
        /// </summary>
        /// <param name="scManager">Handle to Service Manager.</param>
        /// <param name="serviceName">Service Name.</param>
        /// <param name="desiredAccess">Access for service open.</param>
        /// <returns>Opens handle of service.</returns>
        public new static IntPtr OpenService(IntPtr scManager, string serviceName, uint desiredAccess)
        {
            Trc.Log(LogLevel.Debug, "Open service: {0}", serviceName);
            if (!s_serviceHandleMap.ContainsKey(serviceName))
            {
                s_serviceHandleMap[serviceName] = 0;
            }

            var stopWatch = new System.Diagnostics.Stopwatch();

            stopWatch.Start();

            var hService = NativeMethods.OpenService(scManager, serviceName, desiredAccess);
            if (IntPtr.Zero != hService)
            {
                ++s_serviceHandleMap[serviceName];
            }
            stopWatch.Stop();
            Trc.Log(LogLevel.Debug, "Time taken by OpenService service: {0} time: {1}ms", serviceName, stopWatch.ElapsedMilliseconds);

            Trc.Log(LogLevel.Debug, "SeviceHandle Info: Service: {0} handle count = {1}",
                                                                serviceName, s_serviceHandleMap[serviceName]);

            return hService;
        }

        /// <summary>
        /// Opens handle of service manager.
        /// </summary>
        /// <param name="machineName">Machine Name.</param>
        /// <param name="databaseName">Database Name.</param>
        /// <param name="access">Access needed.</param>
        /// <returns>Handle to service control manager.</returns>
        public new static IntPtr OpenSCManager(string machineName, string databaseName, uint access)
        {
            var hSCManager = NativeMethods.OpenSCManager(machineName, databaseName, access);

            if (IntPtr.Zero != hSCManager)
            {
                ++s_scMangerHandleCount;
            }

            Trc.Log(LogLevel.Debug, "SCMHandle Info: SCM handle count = {0}", s_scMangerHandleCount);
            return hSCManager;
        }

        /// <summary>
        /// Closes handle of service.
        /// </summary>
        /// <param name="serviceName">Service Name.</param>
        /// <param name="hService">Service Handle.</param>
        /// <returns>Success/Failure.</returns>
        public static bool CloseService(string serviceName, IntPtr hService)
        {
            if (!s_serviceHandleMap.ContainsKey(serviceName))
            {
                s_serviceHandleMap[serviceName] = 0;

                Trc.Log(LogLevel.Debug, "CloseService: Service: {0} not opened. handle count = {1}", 
                    serviceName, s_serviceHandleMap[serviceName]);
            }

            Debug.Assert(0 != s_serviceHandleMap[serviceName]);

            if (IntPtr.Zero == hService)
            {
                Trc.Log(LogLevel.Debug, "Service {0} handle is already closed", serviceName);
                Trc.Log(LogLevel.Debug, "SeviceHandle Info: Service: {0} handle count = {1}",
                                                                    serviceName, s_serviceHandleMap[serviceName]);

                return true;
            }

            --s_serviceHandleMap[serviceName];
            Trc.Log(LogLevel.Debug, "SeviceHandle Info: Service: {0} handle count = {1}", 
                                                                serviceName, s_serviceHandleMap[serviceName]);

            return NativeMethods.CloseServiceHandle(hService);
        }

        /// <summary>
        /// Closes Handle of service manager.
        /// </summary>
        /// <param name="hSCManager">Handle of service Manager.</param>
        /// <returns>success/failure.</returns>
        public static bool CloseSCManager(IntPtr hSCManager)
        {
            if (IntPtr.Zero == hSCManager)
            {
                Trc.Log(LogLevel.Debug, "SCM handle is already closed");
                Trc.Log(LogLevel.Debug, "SCMHandle Info: SCM handle count = {0}", s_scMangerHandleCount);
                return true;
            }

            Debug.Assert(0 != s_scMangerHandleCount);

            --s_scMangerHandleCount;
            Trc.Log(LogLevel.Debug, "SCMHandle Info: SCM handle count = {0}", s_scMangerHandleCount);

            return NativeMethods.CloseServiceHandle(hSCManager);
        }
    }
}
