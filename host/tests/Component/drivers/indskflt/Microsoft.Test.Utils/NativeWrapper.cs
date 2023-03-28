using Microsoft.Test.Utils.NativeMacros;
using Microsoft.Win32.SafeHandles;
using System;
using System.Runtime.InteropServices;
using System.Threading;

namespace Microsoft.Test.Utils
{
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

        public enum Win32Error : int
        {
            Success = 0,
            IOPending = 0x000003e5,
        }

        #endregion
    }

    public static class NativeWrapper
    {
        public static Win32Error GetLastWin32Error()
        {
            return (Win32Error)Marshal.GetLastWin32Error();
        }

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
        public static extern bool SetEndOfFile(
            [In] SafeFileHandle hFile
        );

        [DllImport("Kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        public static extern bool SetFilePointerEx(
            [In] SafeFileHandle hFile,
            [In] long liDistanceToMove,
            [Out, Optional] out long newFilePointer,
            [In] MoveMethod moveMethod
        );

        [DllImport("Kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        public static extern bool SetFileValidData(
            [In] SafeFileHandle hFile,
            [In] long validDataLength
        );

        [DllImport("Kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        unsafe public extern static bool DeviceIoControl(
            SafeFileHandle hDevice, uint dwIoControlCode, IntPtr lpInBuffer, uint nInBufferSize, IntPtr lpOutBuffer, uint nOutBufferSize, ref uint lpBytesReturned, NativeOverlapped* lpOverlapped);

    }
}