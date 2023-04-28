using System;
using System.Collections.Generic;
using System.Text;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Security.Cryptography.X509Certificates;

namespace Com.Inmage
{
    internal static class NativeMethods
    {
        internal const String DLL_CRYPT32 = "crypt32.dll";
        internal const String DLL_WINTRUST = "wintrust.dll";
  
        #region WIN32_functions

        [DllImport(DLL_WINTRUST, SetLastError = false, CharSet = CharSet.Unicode)]
        internal static extern uint WinVerifyTrust(
             [In] IntPtr callerWindowHandle,
             [In] [MarshalAs(UnmanagedType.LPStruct)] Guid actionGuid,
             [In] Com.Inmage.Security.WinTrustData winTrustData);

        [DllImport(DLL_CRYPT32, CharSet = CharSet.Auto, SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        internal extern static bool CertGetCertificateChain(
            [In] IntPtr chainEngineHandle,
            [In] IntPtr certContextHandle,
            [In] IntPtr timePtr,
            [In] IntPtr additionalStoreHandle,
            [In] ref Com.Inmage.Security.CertChainPara chainPara,
            [In] uint flags,
            [In] IntPtr reservedPtr,
            [In, Out] ref IntPtr chainContextHandle);

        [DllImport(DLL_CRYPT32, CharSet = CharSet.Auto, SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        internal extern static bool CertVerifyCertificateChainPolicy(
            [In] IntPtr chainPolicyOID,
            [In] IntPtr chainContextHandle,
            [In] ref Com.Inmage.Security.CertChainPolicyPara policyPara,
            [In, Out] ref Com.Inmage.Security.CertChainPolicyStatus policyStatus);

        [DllImport(DLL_CRYPT32, SetLastError = true)]
        internal static extern void CertFreeCertificateChain(
            [In] IntPtr certChainHandle);
 
        #endregion
    }
}
