using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.Runtime.InteropServices;
using Com.Inmage.Tools;

namespace Com.Inmage.Security
{        
    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    sealed class WinTrustFileInfo
    {
        public WinTrustFileInfo(String filePath)
        {
            this.m_structSize = (UInt32)Marshal.SizeOf(typeof(WinTrustFileInfo));
            this.m_filePath = filePath;
            this.m_fileHandle = IntPtr.Zero;
            this.m_knownSubjectGuidPtr = IntPtr.Zero;
        }

        UInt32 m_structSize;
        [MarshalAs(UnmanagedType.LPWStr)]
        string m_filePath;
        IntPtr m_fileHandle;
        IntPtr m_knownSubjectGuidPtr;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    sealed class WinTrustData : IDisposable
    {
        const uint UI_CHOICE_NONE = 2;

        const uint PROV_FLAG_REVOCATION_CHECK_NONE = 0x00000010;       
        const uint PROV_FLAG_REVOCATION_CHECK_CHAIN_EXCLUDE_ROOT = 0x00000080;
        const uint PROV_FLAG_REVOCATION_CHECK_CACHE_ONLY_URL_RETRIEVAL = 0x00001000;
        
        const uint CERTIFICATE_REVOCATION_CHECKS_NONE = 0x00000000;
        const uint CERTIFICATE_REVOCATION_CHECKS_REVOKE_WHOLECHAIN = 0x00000001;

        const uint VERIFY_TRUST_FILE = 1;
        const uint UI_CONTEXT_EXECUTE = 0;
        const uint STATE_ACTION_VERIFY = 0x00000001;

        public WinTrustData(String filePath)
        {
            this.m_structSize = (UInt32)Marshal.SizeOf(typeof(WinTrustData));
            this.m_policyCallbackDataPtr = IntPtr.Zero;
            this.m_sipClientDataPtr = IntPtr.Zero;
            this.m_uiChoice = UI_CHOICE_NONE;
            this.m_revocationChecks = CERTIFICATE_REVOCATION_CHECKS_REVOKE_WHOLECHAIN;
            this.m_unionChoice = VERIFY_TRUST_FILE;
            this.m_stateAction = STATE_ACTION_VERIFY;
            this.m_stateData = IntPtr.Zero;
            this.m_urlReference = IntPtr.Zero;
            this.m_provFlags = PROV_FLAG_REVOCATION_CHECK_CHAIN_EXCLUDE_ROOT | PROV_FLAG_REVOCATION_CHECK_CACHE_ONLY_URL_RETRIEVAL;
            this.m_uiContext = UI_CONTEXT_EXECUTE;
            WinTrustFileInfo winTrustFileInfo = new WinTrustFileInfo(filePath);
            this.m_winTrustFileInfoHandle = SafeAllocHandle.AllocateHandle(winTrustFileInfo);
        }

        public void Dispose()
        {
            if (!this.m_disposed)
            {
                if(this.m_winTrustFileInfoHandle != null && !this.m_winTrustFileInfoHandle.IsInvalid)
                {
                    this.m_winTrustFileInfoHandle.Close();
                }
                this.m_disposed = true;
            }
            GC.SuppressFinalize(this);
        }

        ~WinTrustData()
        {
            Dispose();
        }

        UInt32 m_structSize;
        IntPtr m_policyCallbackDataPtr;
        IntPtr m_sipClientDataPtr;
        uint m_uiChoice;
        uint m_revocationChecks;
        uint m_unionChoice;
        SafeAllocHandle m_winTrustFileInfoHandle;
        uint m_stateAction;
        IntPtr m_stateData;
        IntPtr m_urlReference;
        uint m_provFlags;
        uint m_uiContext;
        bool m_disposed = false;
    }

    internal static class WinTrustWrapper
    {
        private const string WINTRUST_ACTION_GENERIC_VERIFY_V2 = "{00AAC56B-CD44-11d0-8CC2-00C04FC295EE}";

        private const uint WTD_RETURN_CODE_SUCCESS = 0x00000000;
        private const uint WTD_RETURN_CODE_TRUST_E_NOSIGNATURE = 0x800B0100;
        
        internal static bool VerifyTrust(string filePath)
        {
            //Retruning true for this method as we don't have unifedagent.exe binary is signed.
            // When UA is signed with MS certificate we need to remove this return code.
            if (WinTools.VerifyUnifiedAgentCertificate() == true)
            {
                Trace.WriteLine(DateTime.Now + "\t In registry user selected not to verify UA signing ");
                return true;
            }
            else
            {
                bool bStatus = false;
                using (WinTrustData wtData = new WinTrustData(filePath))
                {
                    Guid wtActionGuid = new Guid(WINTRUST_ACTION_GENERIC_VERIFY_V2);
                    uint retVal = NativeMethods.WinVerifyTrust(IntPtr.Zero, wtActionGuid, wtData);
                    switch (retVal)
                    {
                        case WTD_RETURN_CODE_SUCCESS:
                            Trace.WriteLine(DateTime.Now + "\tTrust verification is successful.");
                            bStatus = true;
                            break;
                        case WTD_RETURN_CODE_TRUST_E_NOSIGNATURE:
                            Trace.WriteLine(DateTime.Now + "\tTrust verification failed. Error: The given file is not signed or has invalid signature.");
                            break;
                        default:
                            Trace.WriteLine(DateTime.Now + "\tTrust verification failed. Error: 0x" + retVal.ToString("x"));
                            break;
                    }
                }

                return bStatus;
            }
        }
    }    
}
