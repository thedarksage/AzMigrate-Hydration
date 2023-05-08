//using System;
//using System.Collections.Generic;
//using System.Linq;
using System.Text;
using System.Security;
using System.Runtime.InteropServices;

//[assembly: AllowPartiallyTrustedCallers]

internal static class NativeMethods
{
    [SecurityCritical]
    [DllImport("kernel32.dll", EntryPoint = "GetPrivateProfileString")]
    private static extern int ReadConfFile(string section, string key, string def, StringBuilder retVal, int size, string filePath);

    [SecuritySafeCritical]
    internal static int GetPrivateProfileString(string section, string key, string def, StringBuilder retVal, int size, string filePath)
    {
        return ReadConfFile(section, key, def, retVal, size, filePath);
    }

    [SecurityCritical]
    [DllImport("kernel32.dll", EntryPoint = "WritePrivateProfileString")]
    private static extern bool WriteConfFile(string lpAppName, string lpKeyName, string lpString, string lpFileName);

    [SecuritySafeCritical]
    internal static bool WritePrivateProfileString(string lpAppName, string lpKeyName, string lpString, string lpFileName)
    {
        return WriteConfFile(lpAppName, lpKeyName, lpString, lpFileName);
    }

    [SecurityCritical]
    [DllImport("kernel32.dll", EntryPoint = "GetVolumePathName")]
    private static extern bool GetVolumeMountPoint(string lpszFileName, [Out] StringBuilder lpszVolumePathName, uint cchBufferLength);

    [SecuritySafeCritical]
    internal static bool GetVolumePathName(string lpszFileName, [Out] StringBuilder lpszVolumePathName, uint cchBufferLength)
    {
        return GetVolumeMountPoint(lpszFileName, lpszVolumePathName, cchBufferLength);
    }
}
