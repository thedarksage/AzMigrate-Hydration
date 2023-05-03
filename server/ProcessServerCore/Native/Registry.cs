using Microsoft.Win32;
using Microsoft.Win32.SafeHandles;
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Security.AccessControl;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.Native
{
    /// <summary>
    /// Native Registry helpers
    /// </summary>
    internal static class Registry
    {
        public enum HKEY : long
        {
            HKEY_CLASSES_ROOT = 0x80000000,
            HKEY_CURRENT_USER = 0x80000001,
            HKEY_LOCAL_MACHINE = 0x80000002,
            HKEY_USERS = 0x80000003,
            HKEY_PERFORMANCE_DATA = 0x80000004,
            HKEY_CURRENT_CONFIG = 0x80000005
        }

        private const int KEY_WOW64_32KEY = 0x0200;
        private const int KEY_WOW64_64KEY = 0x0100;

        /// <summary>
        /// Create or open registry key for transacted operations
        /// </summary>
        /// <param name="hKey">Well-known <see cref="HKEY"/></param>
        /// <param name="subKey">Subkey path</param>
        /// <param name="rights"><see cref="RegistryRights"/></param>
        /// <param name="transactionHandle">Handle to transaction</param>
        /// <param name="use64BitView">If true, accesses 64-bit mode registry.
        /// Otherwise, 32-bit mode registry.</param>
        /// <returns>Registry key object for the created or opened entity</returns>
        public static RegistryKey CreateKeyTransacted(
            HKEY hKey, string subKey, RegistryRights rights, SafeHandle transactionHandle, bool use64BitView)
        {
            rights |= (RegistryRights)(use64BitView ? KEY_WOW64_64KEY : KEY_WOW64_32KEY);

            var error = NativeMethods.RegCreateKeyTransacted(
                new IntPtr(unchecked((int)hKey)), subKey, 0, null, 0, rights,
                IntPtr.Zero, out SafeRegistryHandle regHandle,
                out uint _, transactionHandle, IntPtr.Zero);

            if (error != 0)
                throw new Win32Exception(error);

            return RegistryKey.FromHandle(regHandle, use64BitView ? RegistryView.Registry64 : RegistryView.Registry32);
        }

        /// <summary>
        /// Open registry key for transacted operations
        /// </summary>
        /// <param name="hKey">Well-known <see cref="HKEY"/></param>
        /// <param name="subKey">Subkey path</param>
        /// <param name="rights"><see cref="RegistryRights"/></param>
        /// <param name="transactionHandle">Handle to transaction</param>
        /// <param name="use64BitView">If true, accesses 64-bit mode registry.
        /// Otherwise, 32-bit mode registry.</param>
        /// <returns>Registry key object for the opened entity</returns>
        public static RegistryKey OpenKeyTransacted(
            HKEY hKey, string subKey, RegistryRights rights, SafeHandle transactionHandle, bool use64BitView)
        {
            rights |= (RegistryRights)(use64BitView ? KEY_WOW64_64KEY : KEY_WOW64_32KEY);

            var error = NativeMethods.RegOpenKeyTransacted(
                new IntPtr(unchecked((int)hKey)), subKey, 0, rights,
                out SafeRegistryHandle regHandle, transactionHandle, IntPtr.Zero);

            if (error == 2) // FileNotFound
                return null;

            if (error != 0)
                throw new Win32Exception(error);

            return RegistryKey.FromHandle(regHandle, use64BitView ? RegistryView.Registry64 : RegistryView.Registry32);
        }

        /// <summary>
        /// Delete registry key as part of transacted operations
        /// </summary>
        /// <param name="hKey">Well-known <see cref="HKEY"/></param>
        /// <param name="subKey">Subkey path</param>
        /// <param name="transactionHandle">Handle to transaction</param>
        /// <param name="use64BitView">If true, accesses 64-bit mode registry.
        /// Otherwise, 32-bit mode registry.</param>
        /// <param name="ignoreKeyNotFound">If true, don't fail, if the key isn't found.</param>
        public static void DeleteKeyTransacted(
            HKEY hKey, string subKey, SafeHandle transactionHandle, bool use64BitView, bool ignoreKeyNotFound)
        {
            var rights = (RegistryRights)(use64BitView ? KEY_WOW64_64KEY : KEY_WOW64_32KEY);

            var error = NativeMethods.RegDeleteKeyTransacted(
                new IntPtr(unchecked((int)hKey)), subKey, rights, 0, transactionHandle, IntPtr.Zero);

            if (error == 2 && ignoreKeyNotFound) // FileNotFound
                return;

            if (error != 0)
                throw new Win32Exception(error);
        }

        private static class NativeMethods
        {
            [DllImport("Advapi32.dll",
                CallingConvention = CallingConvention.Winapi,
                CharSet = CharSet.Unicode)]
            public static extern Int32 RegCreateKeyTransacted(
                [In] IntPtr hKey,
                [In] string lpSubKey,
                [In] UInt32 Reserved,
                [In] string lpClass,
                [In] UInt32 dwOptions,
                [In] RegistryRights samDesired,
                [In] IntPtr lpSecurityAttributes,
                [Out] out SafeRegistryHandle phkResult,
                [Out] out UInt32 lpdwDisposition,
                [In] SafeHandle hTransaction,
                [In] IntPtr pExtendedParemeter);

            [DllImport("Advapi32.dll",
                CallingConvention = CallingConvention.Winapi,
                CharSet = CharSet.Unicode)]
            public static extern Int32 RegOpenKeyTransacted(
                [In] IntPtr hKey,
                [In] string lpSubKey,
                [In] UInt32 Reserved,
                [In] RegistryRights samDesired,
                [Out] out SafeRegistryHandle phkResult,
                [In] SafeHandle hTransaction,
                [In] IntPtr pExtendedParemeter);

            [DllImport("Advapi32.dll",
                CallingConvention = CallingConvention.Winapi,
                CharSet = CharSet.Unicode)]
            public static extern Int32 RegDeleteKeyTransacted(
                [In] IntPtr hKey,
                [In] string lpSubKey,
                [In] RegistryRights samDesired,
                [In] UInt32 Reserved,
                [In] SafeHandle hTransaction,
                [In] IntPtr pExtendedParemeter);
        }
    }
}
