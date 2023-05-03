using Microsoft.Win32.SafeHandles;
using System;
using System.ComponentModel;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.Native
{
    namespace Common
    {
        /// <summary>
        /// Windows API LARGE_INTEGER
        /// </summary>
        [StructLayout(LayoutKind.Explicit)]
        public struct LARGE_INTEGER
        {
            [FieldOffset(0)]
            public UInt32 LowPart;
            [FieldOffset(4)]
            public UInt32 HighPart;

            [FieldOffset(0)]
            public UInt64 QuadPart;
        }

        /// <summary>
        /// Native windows error codes
        /// </summary>
        public enum Win32Error
        {
            /// <summary>
            /// The operation completed successfully.
            /// </summary>
            ERROR_SUCCESS = 0,

            /// <summary>
            /// The I/O operation has been aborted because of either a thread
            /// exit or an application request.
            /// </summary>
            ERROR_OPERATION_ABORTED = 995,

            /// <summary>
            /// Overlapped I/O operation is in progress.
            /// </summary>
            ERROR_IO_PENDING = 997,

            /// <summary>
            /// Element not found.
            /// </summary>
            ERROR_NOT_FOUND = 1168,

            /// <summary>
            /// An internal error occurred.
            /// </summary>
            ERROR_INTERNAL_ERROR = 1359
        }

        /// <summary>
        /// Object that represents a successfully completed overlapped IO
        /// </summary>
        public class OverlappedIO
        {
            /// <summary>
            /// Offset returned
            /// </summary>
            public ulong Offset { get; private set; }

            /// <summary>
            /// Number of bytes returned
            /// </summary>
            public uint NumOfBytes { get; private set; }

            /// <summary>
            /// Error code returned
            /// </summary>
            public uint ErrorCode { get; private set; }

            public static void PrepareHandleForOverlappedIO(SafeFileHandle handle, bool isAlreadyTPBound)
            {
                // If the file handle used is already open for async operation
                // via FileStream, there's no need to set this again, which would
                // actually fail.
                if (!isAlreadyTPBound && !ThreadPool.BindHandle(handle))
                    throw new Exception("Failed to bind handle to threadpool");

                // Need this call to clearly perform the completion of the task
                // just once.
                SetFileCompletionNotificationModes(handle, skipCompletionPortOnSuccess: true, skipSetEventOnHandle: false);
            }

            public unsafe delegate bool Win32ApiCall(SafeFileHandle handle, out uint numOfBytesProcessed, NativeOverlapped* nativeOverlapped);

            // TODO-SanKumar-2002: The right way to ensure that the handle is
            // bound to the .Net ThreadPool for callback invocation is to use
            // ThreadPoolBoundHandle. The problem is that it's not available in
            // .Net 4.6.2, which is our current target version.
            /// <summary>
            /// Start asynchronous Win32 call
            /// </summary>
            /// <param name="handle">File handle</param>
            /// <param name="offset">Starting offset</param>
            /// <param name="win32ApiCall">Win32 p/invoke wrapper</param>
            /// <param name="ct">Cancellation token</param>
            /// <returns>OverlappedIO object on successful completion</returns>
            public static unsafe Task<OverlappedIO> StartOverlappedIOAsync(
                SafeFileHandle handle,
                ulong offset,
                Win32ApiCall win32ApiCall,
                CancellationToken ct)
            {
                var tcs = new TaskCompletionSource<OverlappedIO>();
                var resultObj = new OverlappedIO();

                LARGE_INTEGER offsetLI = new LARGE_INTEGER
                {
                    QuadPart = offset
                };

                Overlapped overlapped = new Overlapped(
                    offsetLo: (int)offsetLI.LowPart,
                    offsetHi: (int)offsetLI.HighPart,
                    hEvent: IntPtr.Zero,
                    ar: null);

                // Prevents race condition between registering for
                // cancellation and an asynchronous completion. Otherwise,
                // there could be an edge case, where the asynchronous completion
                // would end up freeing the unmanaged overlap and then the
                // cancellation callback would be registered with bad
                // ctRegistration and natOvlpdPtr objects.
                // [Synchronizes access to ctRegistration, natOvlpdPtr]
                object asyncComplRaceLock = new object();
                CancellationTokenRegistration ctRegistration = new CancellationTokenRegistration();
                IntPtr natOvlpdPtr = IntPtr.Zero;
                int ioCompleted = 0;

                void CompleteTask(uint errorCode, uint numBytes, NativeOverlapped* pOVERLAP)
                {
                    if (1 == Interlocked.CompareExchange(ref ioCompleted, 1, 0))
                    {
                        Debug.Assert(false);
                        return;
                    }

                    try
                    {
                        if (errorCode == 0)
                        {
                            LARGE_INTEGER resOffsetLI = new LARGE_INTEGER
                            {
                                LowPart = (uint)pOVERLAP->OffsetLow,
                                HighPart = (uint)pOVERLAP->OffsetHigh
                            };

                            tcs.SetResult(new OverlappedIO() { Offset = resOffsetLI.QuadPart, NumOfBytes = numBytes, ErrorCode = errorCode });
                        }
                        else if (errorCode == (int)Win32Error.ERROR_OPERATION_ABORTED)
                        {
                            tcs.TrySetCanceled(ct.IsCancellationRequested ? ct : new CancellationToken(true));
                        }
                        else
                        {
                            tcs.SetException(new Win32Exception((int)errorCode));
                        }
                    }
                    finally
                    {
                        // Cancel registration. Note that this method waits for
                        // any active registered cancellation actions to complete.
                        // Due to this, we don't need a separate synchronization
                        // between the callback executing cancellation of the
                        // overlapped pointer and this thread freeing the same
                        // native overlapped pointer.
                        ctRegistration.Dispose();

                        if (pOVERLAP != null)
                        {
                            Overlapped.Unpack(pOVERLAP);
                            Overlapped.Free(pOVERLAP);
                        }
                    }
                }

                void IOComplCallbackImpl(uint winError, uint numBytes, NativeOverlapped* pOVERLAP)
                {
                    lock (asyncComplRaceLock)
                    {
                        Debug.Assert(pOVERLAP == natOvlpdPtr.ToPointer());

                        CompleteTask(winError, numBytes, pOVERLAP);
                    }
                }

                NativeOverlapped* packedOvlpdRawPtr = null;
                uint error = (uint)Win32Error.ERROR_INTERNAL_ERROR;
                uint numOfBytesProcessed = 0;
                bool completeSynchronously = true;

                try
                {
                    if (ct.IsCancellationRequested)
                    {
                        error = (uint)Win32Error.ERROR_OPERATION_ABORTED;
                    }
                    else
                    {
                        packedOvlpdRawPtr = overlapped.Pack(IOComplCallbackImpl, null);
                        Interlocked.Exchange(ref natOvlpdPtr, new IntPtr(packedOvlpdRawPtr));

                        bool success = win32ApiCall(handle, out numOfBytesProcessed, packedOvlpdRawPtr);

                        if (!success && Marshal.GetLastWin32Error() == (int)Win32Error.ERROR_IO_PENDING)
                        {
                            completeSynchronously = false;

                            lock (asyncComplRaceLock)
                            {
                                if (ioCompleted == 0 && ct.CanBeCanceled)
                                {
                                    ctRegistration = ct.Register(() =>
                                    {
                                        if (ioCompleted == 0)
                                        {
                                            CancelIO(handle, (NativeOverlapped*)natOvlpdPtr.ToPointer());
                                        }

                                        // NOTE-SanKumar-2001: Any failure in this callback would
                                        // show up on issuing cancellation of this token in another
                                        // thread but it's designed to not throw except in unforeseen
                                        // bad cases.
                                    });
                                }
                            }
                        }
                        else
                        {
                            error = success ? (uint)Win32Error.ERROR_SUCCESS : (uint)Marshal.GetLastWin32Error();
                        }
                    }
                }
                finally
                {
                    if (completeSynchronously)
                    {
                        CompleteTask(error, numOfBytesProcessed, packedOvlpdRawPtr);
                    }
                }

                return tcs.Task;
            }

            /// <summary>
            /// Cancel overlapped IO on the file handle
            /// </summary>
            /// <param name="handle">File handle</param>
            /// <param name="natOvlpdPtr">Native overlapped IO pointer</param>
            private static unsafe void CancelIO(SafeFileHandle handle, NativeOverlapped* natOvlpdPtr)
            {
                Debug.Assert(!handle.IsClosed && !handle.IsInvalid);
                Debug.Assert(natOvlpdPtr != null);

                if (!NativeMethods.CancelIoEx(handle, natOvlpdPtr))
                {
                    // It could be possible that the IO completed and so it may
                    // not be found. So, ignoring that failure.
                    if (Marshal.GetLastWin32Error() != (int)Win32Error.ERROR_NOT_FOUND)
                        throw new Win32Exception();
                }
            }

            /// <summary>
            /// Set notification modes for I/O completion ports of the file
            /// </summary>
            /// <param name="hFile">File handle</param>
            /// <param name="skipCompletionPortOnSuccess">
            /// Skip notification for an I/O completion, unless explicitly pended by the win32 call
            /// </param>
            /// <param name="skipSetEventOnHandle">
            /// Skip setting the event in the file handle on I/O completion
            /// </param>
            private static void SetFileCompletionNotificationModes(SafeFileHandle hFile, bool skipCompletionPortOnSuccess, bool skipSetEventOnHandle)
            {
                const byte FILE_SKIP_COMPLETION_PORT_ON_SUCCESS = 1;
                const byte FILE_SKIP_SET_EVENT_ON_HANDLE = 2;

                int flags = 0;
                flags |= skipCompletionPortOnSuccess ? FILE_SKIP_COMPLETION_PORT_ON_SUCCESS : 0;
                flags |= skipSetEventOnHandle ? FILE_SKIP_SET_EVENT_ON_HANDLE : 0;

                if (!NativeMethods.SetFileCompletionNotificationModes(hFile, (byte)flags))
                    throw new Win32Exception();
            }

            private static class NativeMethods
            {
                [DllImport("Kernel32.dll", CallingConvention = CallingConvention.Winapi, SetLastError = true)]
                [return: MarshalAs(UnmanagedType.Bool)]
                public static extern unsafe bool CancelIoEx(
                    [In]    SafeFileHandle hFile,
                    [In][Optional] NativeOverlapped* lpOverlapped
                    );

                [DllImport("Kernel32.dll", CallingConvention = CallingConvention.Winapi, SetLastError = true)]
                [return: MarshalAs(UnmanagedType.Bool)]
                public static extern unsafe bool SetFileCompletionNotificationModes(
                    [In]    SafeFileHandle hFile,
                    [In]    byte Flags
                    );
            }
        }
    }
}
