//-----------------------------------------------------------------------
// <copyright file="NativeMethods.cs" company="Microsoft">
//     Copyright (c) Microsoft Corporation.  All rights reserved.
// </copyright>
// <summary> Class to hold all our Native Methods
// </summary>
//-----------------------------------------------------------------------
namespace ASRSetupFramework
{
    using System;
    using System.Diagnostics;
    using System.Diagnostics.CodeAnalysis;
    using System.IO;
    using System.Runtime.InteropServices;
    using System.Text;

    /// <summary>
    /// Native Methods class
    /// </summary>
    [SuppressMessage(
        "Microsoft.StyleCop.CSharp.OrderingRules",
        "SA1201:ElementsMustAppearInTheCorrectOrder",
        Justification = "Native methods being imported.")]
    [SuppressMessage(
        "Microsoft.StyleCop.CSharp.DocumentationRules",
        "SA1600:ElementsMustBeDocumented",
        Justification = "DLL imported methods")]
    [SuppressMessage(
        "Microsoft.StyleCop.CSharp.DocumentationRules",
        "SA1602:EnumerationItemsMustBeDocumented",
        Justification = "DLL imported methods")]
    [SuppressMessage(
        "Microsoft.StyleCop.CSharp.DocumentationRules",
        "SA1630:DocumentationTextMustContainWhitespace",
        Justification = "DLL imported methods")]
    [SuppressMessage(
        "Microsoft.StyleCop.CSharp.DocumentationRules",
        "SA1632:DocumentationTextMustMeetMinimumCharacterLength",
        Justification = "DLL imported methods")]
    [SuppressMessage(
        "Microsoft.StyleCop.CSharp.ReadabilityRules",
        "SA1121:UseBuiltInTypeAlias",
        Justification = "DLL imported methods")]
    public class NativeMethods
    {
        /// <summary>
        /// Prevents a default instance of the NativeMethods class from being created.
        /// </summary>
        protected NativeMethods()
        {
            // Do nothing
        }

        /// <summary>
        /// While updating one service confuration we need to make sure other configurations remain untouched
        /// https://docs.microsoft.com/en-us/windows/win32/api/winsvc/nf-winsvc-changeserviceconfiga
        /// https://docs.microsoft.com/en-us/windows/win32/services/changing-a-service-configuration
        /// Use SERVICE_NO_CHANGE for those case
        /// </summary>
        public static uint SERVICE_NO_CHANGE = uint.MaxValue;

        /// <summary>
        /// Win32 DsEnumerateDomainTrusts() API.
        /// </summary>
        /// <param name="ServerName">The name of the computer</param>
        /// <param name="Flags">Flags that determines which domain trusts to enumerate</param>
        /// <param name="Domains">Output variable that receives an array of DS_DOMAIN_TRUSTS structures</param>
        /// <param name="DomainCount">Output variable that receives the number of elements</param>
        /// <returns>Returns 0 if successful or a Win32 error code otherwise.</returns>
        [DllImport("NetAPI32.dll", CharSet = CharSet.Unicode)]
        public static extern uint DsEnumerateDomainTrusts(
            [MarshalAs(UnmanagedType.LPWStr)] string ServerName,
            [MarshalAs(UnmanagedType.U4)] uint Flags,
            out IntPtr Domains,
            out uint DomainCount);

        /// <summary>
        /// Win32 NetApiBufferFree() API.
        /// </summary>
        /// <param name="buffer">Buffer returned by another network management function</param>
        /// <returns>Returns 0 if successful or a Win32 error code otherwise.</returns>
        [DllImport("NetAPI32.dll", CharSet = CharSet.Unicode)]
        public static extern uint NetApiBufferFree(IntPtr buffer);

        internal const int SC_STATUS_PROCESS_INFO = 0;

        #region Interop for GlobalMemoryStatusEx

        /// <summary>
        /// contains information about the current state of both physical and virtual memory, 
        /// including extended memory
        /// </summary>
        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Auto)]
        public class MemoryStatus
        {
            [return: MarshalAs(UnmanagedType.Bool)]
            [DllImport("kernel32.dll", CharSet = CharSet.Auto, SetLastError = true)]
            public static extern bool GlobalMemoryStatusEx([In, Out] MemoryStatus lpBuffer);

            /// <summary>
            /// Size of the data structure, in bytes. 
            /// Set at construction.
            /// </summary>
            public uint Length;

            /// <summary>
            /// Number between 0 and 100 that specifies the approximate percentage of 
            /// physical memory that is in use 
            /// (0 indicates no memory use and 100 indicates full memory use).
            /// </summary>
            public uint MemoryLoad;

            /// <summary>
            /// Total size of physical memory, in bytes.
            /// </summary>
            public ulong TotalPhysical;

            /// <summary>
            /// Size of physical memory available, in bytes.
            /// </summary>
            public ulong AvailPhysical;

            /// <summary>
            /// Size of the committed memory limit, in bytes. This is physical memory plus 
            /// the size of the page file, minus a small overhead.
            /// </summary>
            public ulong TotalPageFile;

            /// <summary>
            /// Size of available memory to commit, in bytes. The limit is ullTotalPageFile.
            /// </summary>
            public ulong AvailPageFile;

            /// <summary>
            /// Total size of the user mode portion of the virtual address space of the 
            /// calling process, in bytes.
            /// </summary>
            public ulong TotalVirtual;

            /// <summary>
            /// Size of unreserved and uncommitted memory in the user mode portion of the virtual 
            /// address space of the calling process, in bytes.
            /// </summary>
            public ulong AvailVirtual;

            /// <summary>
            /// Size of unreserved and uncommitted memory in the extended portion of the virtual 
            /// address space of the calling process, in bytes.
            /// </summary>
            public ulong AvailExtendedVirtual;

            /// <summary>
            /// Size of the class, computed once statically to prevent FxCop Error
            /// </summary>
            private static int length = Marshal.SizeOf(typeof(MemoryStatus));

            /// <summary>
            /// Initializes a new instance of the <see cref="T:MemoryStatus"/> class.
            /// </summary>
            public MemoryStatus()
            {
                this.Length = (uint)length;
                GlobalMemoryStatusEx(this);
            }
        }

        #endregion // Interop for GlobalMemoryStatusEx

        #region Interop for Service Control

        [Flags]
        public enum SCM_ACCESS : uint
        {
            STANDARD_RIGHTS_REQUIRED = 0xF0000,
            SC_MANAGER_CONNECT = 0x00001,
            SC_MANAGER_CREATE_SERVICE = 0x00002,
            SC_MANAGER_ENUMERATE_SERVICE = 0x00004,
            SC_MANAGER_LOCK = 0x00008,
            SC_MANAGER_QUERY_LOCK_STATUS = 0x00010,
            SC_MANAGER_MODIFY_BOOT_CONFIG = 0x00020,
            SC_MANAGER_ALL_ACCESS = STANDARD_RIGHTS_REQUIRED |
                             SC_MANAGER_CONNECT |
                             SC_MANAGER_CREATE_SERVICE |
                             SC_MANAGER_ENUMERATE_SERVICE |
                             SC_MANAGER_LOCK |
                             SC_MANAGER_QUERY_LOCK_STATUS |
                             SC_MANAGER_MODIFY_BOOT_CONFIG
        }

        [Flags]
        public enum SERVICE_ACCESS : int
        {
            STANDARD_RIGHTS_REQUIRED = 0xF0000,
            SERVICE_QUERY_CONFIG = 0x00001,
            SERVICE_CHANGE_CONFIG = 0x00002,
            SERVICE_QUERY_STATUS = 0x00004,
            SERVICE_ENUMERATE_DEPENDENTS = 0x00008,
            SERVICE_START = 0x00010,
            SERVICE_STOP = 0x00020,
            SERVICE_PAUSE_CONTINUE = 0x00040,
            SERVICE_INTERROGATE = 0x00080,
            SERVICE_USER_DEFINED_CONTROL = 0x00100,
            SERVICE_ALL_ACCESS = (STANDARD_RIGHTS_REQUIRED |
                              SERVICE_QUERY_CONFIG |
                              SERVICE_CHANGE_CONFIG |
                              SERVICE_QUERY_STATUS |
                              SERVICE_ENUMERATE_DEPENDENTS |
                              SERVICE_START |
                              SERVICE_STOP |
                              SERVICE_PAUSE_CONTINUE |
                              SERVICE_INTERROGATE |
                              SERVICE_USER_DEFINED_CONTROL)
        }

        public enum ServiceConfig2Type : uint
        {
            Description = 1,    // SERVICE_CONFIG_DESCRIPTION
            FailureActions = 2,    // SERVICE_CONFIG_FAILURE_ACTIONS
            DelayedAutoStartInfo = 3,    // SERVICE_CONFIG_DELAYED_AUTO_START_INFO
            FailureActionsFlag = 4,    // SERVICE_CONFIG_FAILURE_ACTIONS_FLAG
            ServiceSidInfo = 5,    // SERVICE_CONFIG_SERVICE_SID_INFO
            RequiredPrivilegesInfo = 6,    // SERVICE_CONFIG_REQUIRED_PRIVILEGES_INFO
            PreShutdownInfo = 7,    // SERVICE_CONFIG_PRESHUTDOWN_INFO
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct SERVICE_DELAYED_AUTO_START_INFO
        {
            [MarshalAs(UnmanagedType.Bool)]
            public bool DelayedAutostart;
        }

        public enum SC_STATUS_TYPE : int
        {
            SC_STATUS_PROCESS_INFO = 0
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct SC_ACTION
        {
            public SC_ACTION_TYPE Type;
            public uint Delay;
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct SERVICE_DESCRIPTION_INFO
        {
            [MarshalAs(UnmanagedType.LPWStr)]
            public string ServiceDescription;
        }

        [StructLayout(LayoutKind.Sequential)]
        public class SERVICE_STATUS_PROCESS
        {
            public int dwServiceType;
            public int dwCurrentState;
            public int dwControlsAccepted;
            public int dwWin32ExitCode;
            public int dwServiceSpecificExitCode;
            public int dwCheckPoint;
            public int dwWaitHint;
            public int dwProcessId;
            public uint dwServiceFlags;
        }

        // Service type.
        [Flags]
        public enum SERVICE_TYPE : uint
        {
            SERVICE_KERNEL_DRIVER = 0x00000001,
            SERVICE_FILE_SYSTEM_DRIVER = 0x00000002,
            SERVICE_ADAPTER = 0x00000004,
            SERVICE_RECOGNIZER_DRIVER = 0x00000008,
            SERVICE_WIN32_OWN_PROCESS = 0x00000010,
            SERVICE_WIN32_SHARE_PROCESS = 0x00000020,
            SERVICE_USER_OWN_PROCESS = 0x00000050,
            SERVICE_USER_SHARE_PROCESS = 0x00000060,
            SERVICE_INTERACTIVE_PROCESS = 0x00000100
        }

        // Service start options.
        public enum SERVICE_START_TYPE : uint
        {
            SERVICE_BOOT_START = 0x00000000,
            SERVICE_SYSTEM_START = 0x00000001,
            SERVICE_AUTO_START = 0x00000002,
            SERVICE_DEMAND_START = 0x00000003,
            SERVICE_DISABLED = 0x00000004
        }

        // Severity of the service error.
        public enum SERVICE_ERROR : uint
        {
            SERVICE_ERROR_IGNORE = 0x00000000,
            SERVICE_ERROR_NORMAL = 0x00000001,
            SERVICE_ERROR_SEVERE = 0x00000002,
            SERVICE_ERROR_CRITICAL = 0x00000003
        }

        public enum SC_ACTION_TYPE : uint
        {
            SC_ACTION_NONE = 0,
            SC_ACTION_RESTART = 1,
            SC_ACTION_REBOOT = 2,
            SC_ACTION_RUN_COMMAND = 3
        }



        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Auto)]
        public class SERVICE_FAILURE_ACTIONS
        {
            internal uint ResetPeriod;
            internal string RebootMsg;
            internal string Command;
            internal uint NumActions;
            internal IntPtr Actions;

            internal SERVICE_FAILURE_ACTIONS() { }
            internal SERVICE_FAILURE_ACTIONS(uint resetPeriod,
                                              string rebootMsg,
                                              string command,
                                              uint numActions,
                                              IntPtr actions)
            {
                ResetPeriod = resetPeriod;
                RebootMsg = rebootMsg;
                Command = command;
                NumActions = numActions;
                Actions = actions;
            } // end constructor
        } // end class SERVICE_FAILURE_ACTIONS


        [DllImport("advapi32.dll", CharSet = CharSet.Auto, SetLastError = true)]
        protected static extern IntPtr OpenSCManager(string machineName, string databaseName, uint access);

        [DllImport("advapi32.dll", SetLastError = true, CharSet = CharSet.Auto)]
        protected static extern IntPtr CreateService(IntPtr hSCManager,
                                                    string lpServiceName,
                                                    string lpDisplayName,
                                                    SERVICE_ACCESS dwDesiredAccess,
                                                    SERVICE_TYPE dwServiceType,
                                                    SERVICE_START_TYPE dwStartType,
                                                    SERVICE_ERROR dwErrorControl,
                                                    string lpBinaryPathName,
                                                    string lpLoadOrderGroup,
                                                    IntPtr lpdwTagId,
                                                    string lpDependencies,
                                                    string lpServiceStartName,
                                                    string lpPassword);

        [DllImport("advapi32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool DeleteService(IntPtr hService);

        [DllImport("advapi32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
        protected static extern IntPtr OpenService(IntPtr scManager, string serviceName, uint desiredAccess);

        [DllImport("advapi32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        protected static extern bool CloseServiceHandle(IntPtr scObject);

        [DllImport("advapi32.dll", SetLastError = true, CharSet = CharSet.Auto)]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool ChangeServiceConfig2(
               IntPtr hService,
               int dwInfoLevel,
               IntPtr lpInfo);

        [DllImport("advapi32.dll", CharSet = CharSet.Auto, SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool ChangeServiceConfig(IntPtr hService, UInt32 dwServiceType, UInt32 dwStartType, UInt32 dwErrorControl, IntPtr lpBinaryPathName, IntPtr lpLoadOrderGroup, IntPtr lpdwTagId, IntPtr lpDependencies, IntPtr lpServiceStartName, IntPtr lpPassword, IntPtr lpDisplayName);

        [DllImport("advapi32.dll", CharSet = CharSet.Auto, SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool QueryServiceStatusEx(IntPtr hService,
                                                       SC_STATUS_TYPE InfoLevel,
                                                       IntPtr lpBuffer,
                                                       UInt32 cbBufSize,
                                                       out UInt32 pcbBytesNeeded);

        [DllImport("advapi32.dll", CharSet = CharSet.Auto, SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool QueryServiceConfig2(IntPtr serviceHandle, ServiceConfig2Type infoLevel, IntPtr info, int bufferSize, out uint bytesNeeded);

        [StructLayout(LayoutKind.Sequential)]
        public class ServiceConfiguration
        {
            [MarshalAs(System.Runtime.InteropServices.UnmanagedType.U4)]
            public uint ServiceType;
            [MarshalAs(System.Runtime.InteropServices.UnmanagedType.U4)]
            public int StartType;
            [MarshalAs(System.Runtime.InteropServices.UnmanagedType.U4)]
            public uint ErrorControl;
            [MarshalAs(System.Runtime.InteropServices.UnmanagedType.LPWStr)]
            public string BinaryPathName;
            [MarshalAs(System.Runtime.InteropServices.UnmanagedType.LPWStr)]
            public string LoadOrderGroup;
            [MarshalAs(System.Runtime.InteropServices.UnmanagedType.U4)]
            public uint TagID;
            [MarshalAs(System.Runtime.InteropServices.UnmanagedType.LPWStr)]
            public string Dependencies;
            [MarshalAs(System.Runtime.InteropServices.UnmanagedType.LPWStr)]
            public string ServiceStartName;
            [MarshalAs(System.Runtime.InteropServices.UnmanagedType.LPWStr)]
            public string DisplayName;
        }

        public enum ServiceStartType
        {
            BOOT_START = 0, // A device driver started by the system loader. This value is valid only for driver services.
            SYSTEM_START = 1,
            AUTO_START = 2, // A service started automatically by the service control manager during system startup.
            DEMAND_START = 3, // A service started by the service control manager when a process calls the StartService function.
            DISABLED = 4, // A service that cannot be started. Attempts to start the service result in the error code ERROR_SERVICE_DISABLED. 
        }

        [DllImport("advapi32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        public static extern bool QueryServiceConfig(IntPtr serviceHandle, IntPtr queryConfig, uint bufferSize, out uint bytesNeeded);


        #endregion

        #region Interop for MSI

        // MsiSetExternalUI message types (see INSTALLMESSAGE_ in MSDN)
        public const int mtActionData = 0x08000000;
        public const int mtActionStart = 0x09000000;
        public const int mtProgress = 0x0A000000;
        public const int mtFatalExit = 0x00000000; // premature termination, possibly fatal OOM
        public const int mtError = 0x01000000; // formatted error message
        public const int mtWarning = 0x02000000; // formatted warning message
        public const int mtUser = 0x03000000; // user request message
        public const int mtInfo = 0x04000000; // informative message for log
        public const int mtFilesInUse = 0x05000000; // list of files in use that need to be replaced
        public const int mtResolveSource = 0x06000000; // request to determine a valid source location
        public const int mtOutOfDiskSpace = 0x07000000; // insufficient disk space message
        public const int mtCommonData = 0x0B000000; // product info for dialog: language Id, dialog caption
        public const int mtInitialize = 0x0C000000; // sent prior to UI initialization, no string data
        public const int mtTerminate = 0x0D000000; // sent after UI termination, no string data
        public const int mtShowDialog = 0x0E000000; // sent prior to display or authored dialog or wizard

        /// <summary>
        /// Msi InstatllState values
        /// </summary>
        public const int InstallState_Unknown = -1;
        public const int InstallState_Local = 3;

        /// <summary>
        /// InstallUIHandler is needed for the MsiSetExternalUI import
        /// </summary>
        /// <param name="context">context for the message</param>
        /// <param name="messageType">Type of the message</param>
        /// <param name="message">message text</param>
        /// <returns>int return value</returns>
        public delegate int InstallUIHandler(
                                                IntPtr context,
                                                Int32 messageType,
                                                [MarshalAs(UnmanagedType.LPTStr)] string message);

        /// <summary>
        /// Install modes
        /// </summary>
        public enum InstallLogModes : int
        {
            /// <summary>
            /// No install mode.
            /// </summary>
            None = 0,

            /// <summary>
            /// Fatal Exit encountered.
            /// </summary>
            FatalExit = (1 << ((int)0x00000000 >> 24)),

            /// <summary>
            /// Error encountered.
            /// </summary>
            Error = (1 << ((int)0x01000000 >> 24)),

            /// <summary>
            /// Warning encountered.
            /// </summary>
            Warning = (1 << ((int)0x02000000 >> 24)),

            /// <summary>
            /// User
            /// </summary>
            User = (1 << ((int)0x03000000 >> 24)),

            /// <summary>
            /// Info
            /// </summary>
            Info = (1 << ((int)0x04000000 >> 24)),

            /// <summary>
            /// Files In Use
            /// </summary>
            FilesInUse = (1 << ((int)0x05000000 >> 24)),

            /// <summary>
            /// Resolve Source
            /// </summary>
            ResolveSource = (1 << ((int)0x06000000 >> 24)),

            /// <summary>
            /// Out Of Disk Space
            /// </summary>
            OutOfDiskSpace = (1 << ((int)0x07000000 >> 24)),

            /// <summary>
            /// Action Start
            /// </summary>
            ActionStart = (1 << ((int)0x08000000 >> 24)),

            /// <summary>
            /// Action Data
            /// </summary>
            ActionData = (1 << ((int)0x09000000 >> 24)),

            /// <summary>
            /// CommonData
            /// </summary>
            CommonData = (1 << ((int)0x0B000000 >> 24)),

            /// <summary>
            /// Property Dump
            /// </summary>
            PropertyDump = (1 << ((int)0x0A000000 >> 24)), // log only

            /// <summary>
            /// Verbose
            /// </summary>
            Verbose = (1 << ((int)0x0C000000 >> 24)), // log only

            /// <summary>
            /// LogOnError
            /// </summary>
            LogOnError = (1 << ((int)0x0E000000 >> 24)), // log only

            /// <summary>
            /// Progress
            /// </summary>
            Progress = (1 << ((int)0x0A000000 >> 24)), // external handler only

            /// <summary>
            /// Initialize
            /// </summary>
            Initialize = (1 << ((int)0x0C000000 >> 24)), // external handler only

            /// <summary>
            /// Terminate
            /// </summary>
            Terminate = (1 << ((int)0x0D000000 >> 24)), // external handler only

            /// <summary>
            /// ShowDialog
            /// </summary>
            ShowDialog = (1 << ((int)0x0E000000 >> 24)), // external handler only
        }

        /// <summary>
        /// Attributes to be passed to the installer
        /// </summary>
        public enum InstallLogAttributes // flag attributes for MsiEnableLog
        {
            /// <summary>
            /// Append to the log
            /// </summary>
            Append = (1 << 0),

            /// <summary>
            /// flush eash line of the log
            /// </summary>
            FlushEachLine = (1 << 1),
        }

        /// <summary>
        /// InstallUiLevel flags
        /// </summary>
        public enum InstallUiLevel : int
        {
            /// <summary>
            /// NoChange
            /// </summary>
            NoChange = 0,    // UI level is unchanged

            /// <summary>
            /// Default
            /// </summary>
            Default = 1,    // default UI is used

            /// <summary>
            /// None
            /// </summary>
            None = 2,    // completely silent installation

            /// <summary>
            /// Basic
            /// </summary>
            Basic = 3,    // simple progress and error handling

            /// <summary>
            /// Reduced
            /// </summary>
            Reduced = 4,    // authored UI, wizard dialogs suppressed

            /// <summary>
            /// Full
            /// </summary>
            Full = 5,    // authored UI with wizards, progress, errors

            /// <summary>
            /// EndDialog
            /// </summary>
            EndDialog = 0x80, // display success/failure dialog at end of install

            /// <summary>
            /// ProgressOnly
            /// </summary>
            ProgressOnly = 0x40, // display only progress dialog
        }

        /// <summary>
        /// InstallErrorLevel return values
        /// </summary>
        public enum InstallErrorLevel : long
        {
            /// <summary>
            /// Error Success
            /// </summary>
            Error_Success = 0,

            /// <summary>
            /// Error Install UserExit
            /// </summary>
            Error_Install_UserExit = 1602,

            /// <summary>
            /// Error Install Failed
            /// </summary>
            Error_Install_Failed = 1603,

            /// <summary>
            /// Error Success Reboot Initiated
            /// </summary>
            Error_Success_Reboot_Initiated = 1641,

            /// <summary>
            /// Error Success Reboot Required
            /// </summary>
            Error_Success_Reboot_Required = 3010,
        }

        [DllImport("msi.dll", CharSet = CharSet.Auto)]
        public static extern int MsiQueryProductState(
                                                [MarshalAs(UnmanagedType.LPWStr)] string product);   // Product GUID

        [DllImport("msi.dll", CharSet = CharSet.Auto)]
        public static extern int MsiConfigureProductEx(
                                                [MarshalAs(UnmanagedType.LPWStr)] string productCode,    // product code
                                                int installLevel,    // install level
                                                int installState,    // install state
                                                [MarshalAs(UnmanagedType.LPWStr)] string CommandLine);   // command line

        /// <summary>
        /// Msis the install product.
        /// </summary>
        /// <param name="PackagePath">The package path.</param>
        /// <param name="CommandLine">The command line.</param>
        /// <returns>int return</returns>
        [DllImport("msi.dll", CharSet = CharSet.Auto)]
        public static extern int MsiInstallProduct(
                                                [MarshalAs(UnmanagedType.LPWStr)] string PackagePath,    // package to install
                                                [MarshalAs(UnmanagedType.LPWStr)] string CommandLine);   // command line

        /// <summary>
        /// Msis the enable log.
        /// </summary>
        /// <param name="LogMode">The log mode.</param>
        /// <param name="LogFile">The log file.</param>
        /// <param name="LogAttributes">The log attributes.</param>
        /// <returns>int return</returns>
        [DllImport("msi.dll", CharSet = CharSet.Auto)]
        public static extern int MsiEnableLog(
                                                int LogMode,           // logging options
                                                [MarshalAs(UnmanagedType.LPWStr)] string LogFile,           // log file name
                                                int LogAttributes);    // Flush attribute

        [DllImport("msi.dll", CharSet = CharSet.Auto)]
        public static extern int MsiGetProductCode(
                                                [MarshalAs(UnmanagedType.LPWStr)] string ComponentID,           // In: Component ID
                                                [MarshalAs(UnmanagedType.LPWStr)] System.Text.StringBuilder ProductCode);    // Out: Product Code

        [DllImport("msi.dll", CharSet = CharSet.Auto)]
        public static extern Int32 MsiGetProductInfo(
            [MarshalAs(UnmanagedType.LPWStr)] string product,
            [MarshalAs(UnmanagedType.LPWStr)] string property,
            [MarshalAs(UnmanagedType.LPWStr)]StringBuilder valueBuf,
            ref Int32 len);

        [DllImport("msi.dll", CharSet = CharSet.Auto)]
        public static extern Int32 MsiLocateComponent(
            [MarshalAs(UnmanagedType.LPWStr)] string component,
            [MarshalAs(UnmanagedType.LPWStr)] System.Text.StringBuilder pathBuffer,
            ref Int32 numberOfCharInBuffer);

        [DllImport("msi.dll", CharSet = CharSet.Auto)]
        public static extern UInt32 MsiGetFileVersion(
            [MarshalAs(UnmanagedType.LPWStr)] string filePath,
            [MarshalAs(UnmanagedType.LPWStr)] StringBuilder version,
            ref int versionLength,
            [MarshalAs(UnmanagedType.LPWStr)] StringBuilder language,
            ref int languageLength);

        /// <summary>
        /// Returns MSI Version 
        /// </summary>
        /// <returns>MSI Version </returns>
        public static string MsiGetVersion()
        {
            // Per MSDN: 
            // Call the MsiGetFileVersion function with the szFilePath parameter set to the path to the file Msi.dll.
            // You can call the SHGetKnownFolderPath function with the CSIDL_SYSTEM constant to get the path to Msi.dll. 
            string filePath =
                Path.Combine(
                    Environment.GetFolderPath(Environment.SpecialFolder.System),
                    "Msi.Dll");
            Debug.Assert(File.Exists(filePath), "NativeMethods.MsiGetVersion: Bad MSI path");
            int versionLength = 24;
            StringBuilder version = new StringBuilder(versionLength);
            int languageLength = 0;
            uint hr = MsiGetFileVersion(filePath, version, ref versionLength, null, ref languageLength);
            return hr >= 0 ? version.ToString() : null;
        }

        /// <summary>
        /// Msis the set internal UI.
        /// </summary>
        /// <param name="dwUILevel">The dw UI level.</param>
        /// <param name="winhandle">The winhandle.</param>
        /// <returns>int return</returns>
        [DllImport("msi.dll", CharSet = CharSet.Auto)]
        public static extern InstallUiLevel MsiSetInternalUI(int dwUILevel, ref IntPtr winhandle);

        /// <summary>
        /// Safe Handel
        /// </summary>
        public class MsiSafeHandle : Microsoft.Win32.SafeHandles.SafeHandleZeroOrMinusOneIsInvalid
        {
            /// <summary>
            /// Prevents a default instance of the MsiSafeHandle class from being created.
            /// </summary>
            private MsiSafeHandle()
                : base(true)
            {
            }

            /// <summary>
            /// When overridden in a derived class, executes the code required to free the handle.
            /// </summary>
            /// <returns>
            /// true if the handle is released successfully; otherwise, in the event of a catastrophic failure, false. In this case, it generates a ReleaseHandleFailed Managed Debugging Assistant.
            /// </returns>
            protected override bool ReleaseHandle()
            {
                this.handle = IntPtr.Zero;
                return true;
            }
        }

        /// <summary>
        /// As defined in SDK msi.h.
        /// </summary>
        public class MsiAttribute
        {
            public const string INSTALLPROPERTY_INSTALLEDPRODUCTNAME = "InstalledProductName";
            public const string INSTALLPROPERTY_VERSIONSTRING = "VersionString";
            public const string INSTALLPROPERTY_HELPLINK = "HelpLink";
            public const string INSTALLPROPERTY_HELPTELEPHONE = "HelpTelephone";
            public const string INSTALLPROPERTY_INSTALLLOCATION = "InstallLocation";
            public const string INSTALLPROPERTY_INSTALLSOURCE = "InstallSource";
            public const string INSTALLPROPERTY_INSTALLDATE = "InstallDate";
            public const string INSTALLPROPERTY_PUBLISHER = "Publisher";
            public const string INSTALLPROPERTY_LOCALPACKAGE = "LocalPackage";
            public const string INSTALLPROPERTY_URLINFOABOUT = "URLInfoAbout";
            public const string INSTALLPROPERTY_URLUPDATEINFO = "URLUpdateInfo";
            public const string INSTALLPROPERTY_VERSIONMINOR = "VersionMinor";
            public const string INSTALLPROPERTY_VERSIONMAJOR = "VersionMajor";
            public const string INSTALLPROPERTY_PRODUCTID = "ProductID";
            public const string INSTALLPROPERTY_REGCOMPANY = "RegCompany";
            public const string INSTALLPROPERTY_REGOWNER = "RegOwner";
        };
        #endregion // Interop for MSI

        #region Symbolic Link

        /// <summary>
        /// Symbolic link flags.
        /// </summary>
        [Flags]
        public enum SymbolicLinkFlags : uint
        {
            SYMBOLIC_LINK_FLAG_FILE = 0x00000000,
            SYMBOLIC_LINK_FLAG_DIRECTORY = 0x00000001,
            SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE = 0x00000002
        }

        [DllImport("kernel32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool CreateSymbolicLink(string lpSymlinkFileName, string lpTargetFileName, SymbolicLinkFlags dwFlags);

        #endregion // Symbolic Link

        #region Native APIs

        /// <summary>
        /// Win32 API IsWow64Process determines whether the specified process is running under WOW64 or an Intel64 of x64 processor.
        /// Used to verify whether current operating system is 64-bit.
        /// </summary>
        /// <param name="hProcess">A handle to the process.</param>
        /// <param name="bIsWow64Process">Output variable that is set to TRUE if the process is running under WOW64 on an Intel64 or x64 processor.</param>
        /// <returns>
        /// true if succeeds otherwise false. If failed use GetLastWin32Error to get an error code.
        /// </returns>
        [DllImport("kernel32.dll", SetLastError = true, CallingConvention = CallingConvention.Winapi)]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool IsWow64Process([In] IntPtr hProcess, [Out] out bool bIsWow64Process);

        #endregion // Native APIs
    }
}