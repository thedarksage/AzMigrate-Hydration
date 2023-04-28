using Microsoft.Win32.SafeHandles;
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.Native
{
    /// <summary>
    /// Helpers to use Windows Kernel Transaction Manager
    /// </summary>
    internal static class KernelTransactionManager
    {
        /// <summary>
        /// Create a transaction
        /// </summary>
        /// <param name="timeout">Timeout beyond which the transaction will be rolled back</param>
        /// <param name="description">Description of the transaction</param>
        /// <returns></returns>
        public static SafeHandle CreateTransaction(TimeSpan timeout, string description)
        {
            UInt32 timeoutMS;
            if (timeout.TotalMilliseconds > UInt32.MaxValue)
                timeoutMS = UInt32.MaxValue;
            else if (timeout < TimeSpan.Zero)
                timeoutMS = 0;
            else
                timeoutMS = (UInt32)timeout.Milliseconds;

            const int MAX_TRANSACTION_DESCRIPTION_LENGTH = 64;

            if (!string.IsNullOrEmpty(description) && description.Length > MAX_TRANSACTION_DESCRIPTION_LENGTH)
                description = description.Substring(0, MAX_TRANSACTION_DESCRIPTION_LENGTH);

            var transactionHandle = NativeMethods.CreateTransaction(
                IntPtr.Zero, IntPtr.Zero,
                NativeMethods.CreateOptions.TRANSACTION_DO_NOT_PROMOTE,
                NativeMethods.IsolationLevel.Reserved, NativeMethods.IsolationFlags.Reserved,
                timeoutMS, description);

            if (transactionHandle.IsInvalid)
                throw new Win32Exception();

            return transactionHandle;
        }

        // TODO-SanKumar-1909: We could actually implement async.

        /// <summary>
        /// Commit a transaction
        /// </summary>
        /// <param name="transactionHandle">Handle to transaction</param>
        public static void CommitTransaction(SafeHandle transactionHandle)
        {
            if (!NativeMethods.CommitTransaction(transactionHandle))
                throw new Win32Exception();
        }

        /// <summary>
        /// Rollback a transaction
        /// </summary>
        /// <param name="transactionHandle">Handle to transaction</param>
        public static void RollbackTransaction(SafeHandle transactionHandle)
        {
            if (!NativeMethods.RollbackTransaction(transactionHandle))
                throw new Win32Exception();
        }

        private static class NativeMethods
        {

            public enum CreateOptions : UInt32
            {
                // The transaction cannot be distributed
                TRANSACTION_DO_NOT_PROMOTE = 1
            }

            public enum IsolationLevel : UInt32
            {
                Reserved = 0
            }

            public enum IsolationFlags : UInt32
            {
                Reserved = 0
            }

            // NOTE-SanKumar-1904: Using SafeFileHandle, since it provides != 0 check in IsInvalid.
            [DllImport("Ktmw32.dll",
                CallingConvention = CallingConvention.Winapi,
                CharSet = CharSet.Unicode, SetLastError = true)]
            public static extern SafeFileHandle CreateTransaction(
                [In] IntPtr lpTransactionAttributes,
                [In] IntPtr UOW,
                [In] CreateOptions CreateOptions,
                [In] IsolationLevel IsolationLevel,
                [In] IsolationFlags IsolationFlags,
                [In] UInt32 Timeout,
                [In] string Description);

            [DllImport("Ktmw32.dll",
                CallingConvention = CallingConvention.Winapi,
                CharSet = CharSet.Unicode, SetLastError = true)]
            [return: MarshalAs(UnmanagedType.Bool)]
            public static extern bool CommitTransaction([In] SafeHandle TransactionHandle);

            [DllImport("Ktmw32.dll",
                CallingConvention = CallingConvention.Winapi,
                CharSet = CharSet.Unicode, SetLastError = true)]
            [return: MarshalAs(UnmanagedType.Bool)]
            public static extern bool RollbackTransaction([In] SafeHandle TransactionHandle);
        }
    }
}
