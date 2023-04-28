using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.Security;
using System.Security.Cryptography;
using System.Security.Cryptography.X509Certificates;
using System.Runtime.InteropServices;
using Com.Inmage.Tools;

namespace Com.Inmage.Security
{
    internal enum CertUsageMatchType : uint
    {
        UseAndLogic = 0x00000000,
        UseOrLogic = 0x00000001
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    internal struct CertChainPara
    {
        internal uint structSize;
        internal CertUsageMatch requestedUsage;
        internal CertUsageMatch requestedIssuancePolicy;
        internal uint urlRetrievalTimeout;
        internal bool checkRevocationFreshnessTime;
        internal uint revocationFreshnessTime;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    internal struct CertUsageMatch
    {
        internal CertUsageMatchType usageMatchType;
        internal CertEnhkeyUsage enhkeyUsage;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    internal struct CertEnhkeyUsage
    {
        internal uint usageIdentifierCount;
        internal IntPtr usageIdentifierPtr;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    internal struct CertChainPolicyPara
    {
        internal CertChainPolicyPara(int size)
        {
            structSize = (uint)size;
            flags = 0;
            extraPolicyParaPtr = IntPtr.Zero;
        }
        internal uint structSize;
        internal uint flags;
        internal IntPtr extraPolicyParaPtr;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    internal struct CertChainPolicyStatus
    {
        internal CertChainPolicyStatus(int size)
        {
            structSize = (uint)size;
            errorCode = 0;
            chainIndex = 0;
            elementIndex = 0;
            extraPolicyStatusPtr = IntPtr.Zero;
        }
        internal uint structSize;
        internal uint errorCode;
        internal Int32 chainIndex;
        internal Int32 elementIndex;
        internal IntPtr extraPolicyStatusPtr;
    }

    internal static class CryptoWrapper
    {        
        internal const uint CERT_CHAIN_ENABLE_PEER_TRUST = 0x00000400;
        
        // Predefined certificate chain policy to verify MS root
        internal const uint CERT_CHAIN_POLICY_MICROSOFT_ROOT = 7;

        // Policy status code of CertVerifyCertificateChainPolicy
        internal const uint POLICY_STATUS_CODE_CERT_E_UNTRUSTEDROOT = 0x800b0109;

        internal static X509Certificate GetCertificate(string filePath)
        {
            try
            {
                X509Certificate x509Certificate = X509Certificate.CreateFromSignedFile(filePath);
                if (x509Certificate.GetHashCode() != 0)
                {
                    return x509Certificate;
                }
            }
            catch (COMException ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine("Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine(DateTime.Now + "\tFailed to get certificate.");
                Trace.WriteLine("ERROR___________________________________________");
            }
            catch (CryptographicException ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine("Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine(DateTime.Now + "\tUnable to extract certificate.");
                Trace.WriteLine("ERROR___________________________________________");
            }
            return null;
        }

        //
        // Get the certificate chain.
        //        
        internal static bool GetCertificateChain(IntPtr chainEngineHandle, IntPtr certContextHandle, TimeSpan timeout, ref IntPtr chainContextHandle)
        {
            if (null == certContextHandle)
            {               
                Trace.WriteLine(DateTime.Now + "\tCertificate context handle is invalid.");
                return false;
            }
            CertChainPara certChainPara = new CertChainPara();
            certChainPara.structSize = (uint)Marshal.SizeOf(certChainPara);

            CertEnhkeyUsage certEnhkeyUsage = new CertEnhkeyUsage();
            certEnhkeyUsage.usageIdentifierCount = 0;
            certEnhkeyUsage.usageIdentifierPtr = IntPtr.Zero;

            CertUsageMatch certUsageMatch = new CertUsageMatch();
            certUsageMatch.usageMatchType = CertUsageMatchType.UseAndLogic;
            certUsageMatch.enhkeyUsage = certEnhkeyUsage;

            certChainPara.requestedUsage = certUsageMatch;
            certChainPara.urlRetrievalTimeout = (uint)timeout.Milliseconds;

            uint flags = CERT_CHAIN_ENABLE_PEER_TRUST;

            if (!NativeMethods.CertGetCertificateChain(chainEngineHandle,
                                                       certContextHandle,
                                                       IntPtr.Zero,
                                                       IntPtr.Zero,
                                                       ref certChainPara,
                                                       flags,
                                                       IntPtr.Zero,
                                                       ref chainContextHandle))
            {
                int hResult = Marshal.GetHRForLastWin32Error();
                Trace.WriteLine(DateTime.Now + "\tFailed to build certificate chain with the error: " + hResult);
                return false;
            }
            return true;
        }

        //
        // Checks whether the Root of the Certificate chain is Microsoft Certificate Authority
        //        
        internal static bool IsCertificateRootMS(string filePath)
        {
            // Retruning true for this method as we don't have unifedagent.exe binary is signed.
            // When UA is signed with MS certificate we need to remove this return code.
            if (WinTools.VerifyUnifiedAgentCertificate() == true)
            {
                Trace.WriteLine(DateTime.Now + "\t In registry user selected not to verify UA signing ");
                return true;
            }
            else
            {

                bool bStatus = true;
                // Get the certificate that is embedded with the file.
                X509Certificate x509Cert = GetCertificate(filePath);
                if (null == x509Cert)
                {
                    Trace.WriteLine(DateTime.Now + "\tCertificate context handle is invalid.");
                    return false;
                }
                IntPtr certContextHandle = x509Cert.Handle;
                IntPtr chainPolicyOID = new IntPtr(CERT_CHAIN_POLICY_MICROSOFT_ROOT);
                CertChainPolicyPara chainPolicyPara = new CertChainPolicyPara(Marshal.SizeOf(typeof(CertChainPolicyPara)));
                CertChainPolicyStatus chainPolicyStatus = new CertChainPolicyStatus(Marshal.SizeOf(typeof(CertChainPolicyStatus)));

                // Build the certificate chain.
                IntPtr chainContextHandle = IntPtr.Zero;
                try
                {
                    if (!GetCertificateChain(IntPtr.Zero, certContextHandle, new TimeSpan(0, 1, 0), ref chainContextHandle))
                    {
                        Trace.WriteLine(DateTime.Now + "\tUnable to find the root CA of the certificate.");
                        return false;
                    }

                    // Verify the chain using the pre-defined MS root chain policy.
                    if (NativeMethods.CertVerifyCertificateChainPolicy(chainPolicyOID, chainContextHandle, ref chainPolicyPara, ref chainPolicyStatus))
                    {
                        // Check for any error in the policy status.
                        if (chainPolicyStatus.errorCode == 0)
                        {
                            Trace.WriteLine(DateTime.Now + "\tCertificate chain policy verification succeeded, so the root CA of the certificate is MS.");
                        }
                        else
                        {
                            if (POLICY_STATUS_CODE_CERT_E_UNTRUSTEDROOT == chainPolicyStatus.errorCode)
                            {
                                Trace.WriteLine(DateTime.Now + "\tCertificate chain policy verification failed. Error: Couldn't find the trusted root.");
                            }
                            else
                            {
                                Trace.WriteLine(DateTime.Now + "\tCertificate chain policy verification failed. Policy status error: " + chainPolicyStatus.errorCode.ToString("x"));
                            }
                            bStatus = false;
                        }
                    }
                    else
                    {
                        // Call GetLastError() if the CertVerifyCertificateChainPolicy() method fails.
                        int hResult = Marshal.GetHRForLastWin32Error();
                        Trace.WriteLine(DateTime.Now + "\tFailed to verify certificate chain policy with error: " + hResult);
                        bStatus = false;
                    }
                }
                finally
                {
                    // Free the chain context handle
                    if (chainContextHandle != IntPtr.Zero)
                    {
                        NativeMethods.CertFreeCertificateChain(chainContextHandle);
                        Trace.WriteLine(DateTime.Now + "\tCleanup done.");
                        certContextHandle = IntPtr.Zero;
                    }                    
                    x509Cert = null;
                }
                return bStatus;
            }
        }
    }
}
