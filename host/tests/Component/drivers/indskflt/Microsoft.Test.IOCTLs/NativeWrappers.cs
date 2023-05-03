//-----------------------------------------------------------------------
// <copyright file="NativeMacros.cs" company="Microsoft">
//     Copyright (c) Microsoft Corporation. All rights reserved.
// </copyright>
//-----------------------------------------------------------------------
namespace Microsoft.Test.Utils
{
    using System;
    using System.Runtime.InteropServices;
    using System.Threading;
    using Microsoft.Win32.SafeHandles;
    using System.ComponentModel;
    using Microsoft.Test.Utils.NativeMacros;

    namespace NativeMacros
    {
        #region Enums

        [Flags]
        public enum ShareModes : uint
        {
            None = 0,
            Read = 1 << 0,
            Write = 1 << 1,
            Delete = 1 << 2,
        }

        [Flags]
        public enum AccessMasks : uint
        {
            All = 1 << 28,
            Execute = 1 << 29,
            Write = 1 << 30,
            Read = 1U << 31
        }

        public enum CreationDisposition : uint
        {
            CreateNew = 1,
            CreateAlways = 2,
            OpenExisting = 3,
            OpenAlways = 4,
            TruncateExisting = 5
        }

        [Flags]
        public enum FileFlagsAndAttributes : uint
        {
            // File flags
            WriteThrough = 0x80000000,
            Overlapped = 0x40000000,
            NoBuffering = 0x20000000,
            RandomAccess = 0x10000000,
            SequentialScan = 0x08000000,
            DeleteOnClose = 0x04000000,
            BackupSemantics = 0x02000000,
            PosixSemantics = 0x01000000,
            OpenReparsePoint = 0x00200000,
            OpenNoRecall = 0x00100000,
            FirstPipeInstance = 0x00080000,
            SessionAware = 0x00800000,

            // File attributes
            Archive = 0x20,
            Encrypted = 0x4000,
            Hidden = 0x2,
            Normal = 0x80,
            Offline = 0x1000,
            Readonly = 0x1,
            System = 0x4,
            Temporary = 0x100
        }

        public enum MoveMethod : uint
        {
            FileBegin = 0,
            FileCurrent = 1,
            FileEnd = 2
        }
        #endregion
    }

    public static class NativeWrapper
    {
       [DllImport("Kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        public static extern SafeFileHandle CreateFile(
            [In, MarshalAs(UnmanagedType.LPWStr)] string lpFileName,
            [In] AccessMasks dwDesiredAccess,
            [In] ShareModes dwShareMode,
            [In, Optional] IntPtr lpSecurityAttributes,
            [In] CreationDisposition dwCreationDisposition,
            [In] FileFlagsAndAttributes dwFlagsAndAttributes,
            [In, Optional] SafeFileHandle hTemplateFile
        );

        [DllImport("Kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        unsafe public static extern bool WriteFile(
            [In] SafeFileHandle hFile,
            [In] byte[] buffer,
            [In] UInt32 nNumberOfBytesToWrite,
            [Out, Optional] UInt32* lpNumberOfBytesWritten,
            [In, Out, Optional] NativeOverlapped* lpOverlapped
        );

        [DllImport("Kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        unsafe public static extern bool ReadFile(
            [In] SafeFileHandle hFile,
            [Out] byte[] buffer,
            [In] UInt32 nNumberOfBytesToRead,
            [Out, Optional] UInt32* lpNumberOfBytesRead,
            [In, Out, Optional] NativeOverlapped* lpOverlapped
        );

        [DllImport("Kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        unsafe public static extern IntPtr CreateEvent(IntPtr lpEventAttributes, bool bManualReset, bool bInitialState, string lpName);

        [DllImport("Kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        unsafe public static extern bool CloseHandle(SafeFileHandle hFile);



        /// <summary>          
        /// As defined in winbase.h. See MSDN documentation.          
        /// </summary>          
        /// <param name="hDevice"> Obtained through CreateFile</param>          
        /// <param name="dwIoControlCode">See NativeConstants.IoControlCodes</param>          
        /// <param name="lpInBuffer">Use Marshal.AllocHGlobal to allocate.</param>          
        /// <param name="nInBufferSize"></param>          
        /// <param name="lpOutBuffer">Use Marshal.AllocHGlobal to allocate.</param>          
        /// <param name="nOutBufferSize"></param>          
        /// <param name="lpBytesReturned"></param>          
        /// <param name="lpOverlapped"></param>          
        /// <returns>True on success. False otherwise.  Use Marshal.GetLastWin32Error()          
        /// to get error status.</returns>  
        [DllImport("kernel32.dll", SetLastError = true)]
        unsafe public static extern bool DeviceIoControl(
                [In] SafeFileHandle hDevice,
                [In] UInt32 dwIoControlCode,
      [Optional][In] IntPtr lpInBuffer,
                [In] uint nInBufferSize,
 [Optional][In][Out] IntPtr lpOutBuffer,
                [In] UInt32 nOutBufferSize,
     [Optional][Out] out UInt32 lpBytesReturned,
 [Optional][In][Out] NativeOverlapped* lpOverlapped
            );
    }
}