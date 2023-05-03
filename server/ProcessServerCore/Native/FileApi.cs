using Microsoft.Win32.SafeHandles;
using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.Native
{
    using Common;

    /// <summary>
    /// Win32 File API p/invoke wrappers
    /// </summary>
    internal static class FileApi
    {
        /// <summary>
        /// Object that represents a locked file range until its unlocked or disposed.
        /// </summary>
        public class LockFile : IDisposable
        {
            private readonly FileStream m_fileStream;
            private readonly ulong m_offset;
            private readonly ulong m_length;
            private int m_unlocked = 0;

            private LockFile(FileStream fs, ulong offset, ulong length)
            {
                m_fileStream = fs;
                m_offset = offset;
                m_length = length;
            }

            /// <summary>
            /// Lock the given range in the file.
            /// </summary>
            /// <param name="filePath">Path of the file to be locked</param>
            /// <param name="offset">Starting offset</param>
            /// <param name="length">Length to lock</param>
            /// <param name="waitTillLockAcquired">
            /// Wait until the desired lock could be acquired. If false and not able to acquire the
            /// lock, exception will be thrown.
            /// </param>
            /// <param name="exclusive">True - Exclusive lock. False - Shared lock.</param>
            /// <param name="ct">Cancellation Token</param>
            /// <returns>Object representing the locked range on success</returns>
            public static async Task<LockFile> LockFileRangeAsync(
                string filePath,
                ulong offset,
                ulong length,
                bool waitTillLockAcquired,
                bool exclusive,
                CancellationToken ct)
            {
                FileStream fs = null;

                try
                {
                    // Matching the boost implementation in opening the lock file,
                    //apart from creating the file, if not found.
                    fs = new FileStream(
                        filePath,
                        FileMode.OpenOrCreate,
                        FileAccess.ReadWrite,
                        FileShare.ReadWrite,
                        bufferSize: 4096,
                        useAsync: true);

                    // No need to bind the file handle to threadpool, since
                    // FileStream takes care of that, as useAsync was set to true.
                    OverlappedIO.PrepareHandleForOverlappedIO(fs.SafeFileHandle, isAlreadyTPBound: true);

                    NativeMethods.LockFileFlags flags = NativeMethods.LockFileFlags.None;
                    flags |= waitTillLockAcquired ? 0 : NativeMethods.LockFileFlags.FailImmediately;
                    flags |= exclusive ? NativeMethods.LockFileFlags.ExclusiveLock : 0;

                    LARGE_INTEGER lengthLI = new LARGE_INTEGER
                    {
                        QuadPart = length
                    };

                    OverlappedIO.Win32ApiCall win32ApiCall;

                    unsafe
                    {
                        win32ApiCall = (SafeFileHandle handle, out uint numOfBytesProcessed, NativeOverlapped* nativeOverlapped) =>
                        {
                            bool success = NativeMethods.LockFileEx(
                                hFile: handle,
                                dwFlags: flags,
                                dwReserved: 0,
                                nNumberOfBytesToLockLow: lengthLI.LowPart,
                                nNumberOfBytesToLockHigh: lengthLI.HighPart,
                                lpOverlapped: nativeOverlapped
                                );

                            numOfBytesProcessed = success ? lengthLI.LowPart : 0;

                            return success;
                        };
                    }

                    var completedIO = await OverlappedIO.StartOverlappedIOAsync(
                        fs.SafeFileHandle, offset, win32ApiCall, ct)
                        .ConfigureAwait(false);

                    return new LockFile(fs, offset, length);
                }
                catch
                {
                    fs?.Dispose();

                    throw;
                }
            }

            public void Dispose()
            {
                // Although the SafeFileHandle close will guarantee that the
                // range will be unlocked, it would be better to explicitly let
                // the user perform unlock with scoping. Also, this would prevent
                // the load on the GC thread performing finalization of SafeFileHandle.

                try
                {
                    if (Interlocked.CompareExchange(ref m_unlocked, 1, 0) == 0)
                        this.UnlockFileAsync(CancellationToken.None).GetAwaiter().GetResult();
                }
                catch (Exception)
                {
                    // Ignore any excpetion
                }

                try
                {
                    m_fileStream?.Dispose();
                }
                catch (Exception)
                {
                    // Ignore any excpetion
                }

                GC.SuppressFinalize(this);
            }

            /// <summary>
            /// Unlock the locked offset range in the corresponding file
            /// </summary>
            /// <param name="ct">Cancellation token</param>
            /// <returns>None</returns>
            public async Task UnlockFileAsync(CancellationToken ct)
            {
                LARGE_INTEGER lengthLI = new LARGE_INTEGER
                {
                    QuadPart = m_length
                };

                OverlappedIO.Win32ApiCall win32ApiCall;

                unsafe
                {
                    win32ApiCall = (SafeFileHandle handle, out uint numOfBytesProcessed, NativeOverlapped* nativeOverlapped) =>
                    {
                        bool success = NativeMethods.UnlockFileEx(
                            hFile: handle,
                            dwReserved: 0,
                            nNumberOfBytesToUnlockLow: lengthLI.LowPart,
                            nNumberOfBytesToUnlockHigh: lengthLI.HighPart,
                            lpOverlapped: nativeOverlapped
                            );

                        numOfBytesProcessed = success ? lengthLI.LowPart : 0;

                        return success;
                    };
                }

                // TODO: Should CT be supported?!
                var completedIO = await OverlappedIO.StartOverlappedIOAsync(
                    m_fileStream.SafeFileHandle, m_offset, win32ApiCall, ct)
                    .ConfigureAwait(false);

                // Successfully unlocked, thus mustn't be attempted again during Dispose.
                Interlocked.Exchange(ref m_unlocked, 1);
            }
        }

        private static class NativeMethods
        {
            [Flags]
            public enum LockFileFlags : UInt32
            {
                /// <summary>
                /// None.
                /// </summary>
                None = 0,

                /// <summary>
                /// The function returns immediately if it is unable to acquire the
                /// requested lock. Otherwise, it waits.
                /// LOCKFILE_FAIL_IMMEDIATELY
                /// </summary>
                FailImmediately = 0x00000001,

                /// <summary>
                /// The function requests an exclusive lock. Otherwise, it requests a shared lock.
                /// LOCKFILE_EXCLUSIVE_LOCK
                /// </summary>
                ExclusiveLock = 0x00000002
            }

            [DllImport("Kernel32.dll", CallingConvention = CallingConvention.Winapi, SetLastError = true)]
            [return: MarshalAs(UnmanagedType.Bool)]
            public static extern unsafe bool LockFileEx(
                    [In]    SafeFileHandle hFile,
                    [MarshalAs(UnmanagedType.U4)]
                    [In]    LockFileFlags dwFlags,
                    [In]    UInt32 dwReserved,
                    [In]    UInt32 nNumberOfBytesToLockLow,
                    [In]    UInt32 nNumberOfBytesToLockHigh,
                    [In][Out] NativeOverlapped* lpOverlapped
                    );

            [DllImport("Kernel32.dll", CallingConvention = CallingConvention.Winapi, SetLastError = true)]
            [return: MarshalAs(UnmanagedType.Bool)]
            public static extern unsafe bool UnlockFileEx(
                [In]    SafeFileHandle hFile,
                [In]    UInt32 dwReserved,
                [In]    UInt32 nNumberOfBytesToUnlockLow,
                [In]    UInt32 nNumberOfBytesToUnlockHigh,
                [In][Out] NativeOverlapped* lpOverlapped
                );
        }
    }
}
