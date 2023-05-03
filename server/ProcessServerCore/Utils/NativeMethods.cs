using System.Runtime.InteropServices;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core
{
    public static class NativeMethods
    {
        [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
        [return: MarshalAs(UnmanagedType.Bool)]
        internal static extern bool GetDiskFreeSpaceEx(string lpDirectoryName,
           out ulong lpFreeBytesAvailableToCaller,
           out ulong lpTotalNumberOfBytesToCaller,
           out ulong lpTotalNumberOfFreeBytes);
    }
}
