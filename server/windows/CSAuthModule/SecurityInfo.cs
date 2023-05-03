using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;

namespace CSAuthModuleNS
{
    internal static class SecurityInfo
    {
        static String m_passphrase;
        static String m_thumbprint;
        static bool m_thumbprintLoaded = false;
        static bool m_passphraseLoaded = false;
        static object lockObject = new Object();

        internal static void ReadPassphrase(ref String passphrase)
        {
            if (!m_passphraseLoaded)
            {
                lock (lockObject)
                {
                    if (!m_passphraseLoaded)
                    {
                        String filepath;
                        try
                        {
                            String dir = Environment.ExpandEnvironmentVariables("%ProgramData%\\Microsoft Azure Site Recovery\\private");
                            if (!System.IO.Directory.Exists(dir))
                            {
                                throw new Exception(String.Format("Passphrase file not found at {0}", dir));
                            }
                            filepath = String.Format("{0}\\connection.passphrase", dir);
                            m_passphrase = System.IO.File.ReadAllText(filepath);
                          }
                        catch (Exception ex)
                        {
                            throw new Exception("Unable to load passphrase. identity: " + System.Security.Principal.WindowsIdentity.GetCurrent().Name, ex);
                        }
                    }
                    m_passphraseLoaded = true;
                }
            }

            passphrase = m_passphrase;
        }


        internal static void  ReadFingerprint(ref String thumbprint)
        {
            if (!m_thumbprintLoaded)
            {
                lock (lockObject)
                {
                    if (!m_thumbprintLoaded)
                    {
                        try
                        {
                            String filePath;
                            String dir = Environment.ExpandEnvironmentVariables("%ProgramData%\\Microsoft Azure Site Recovery\\fingerprints");
                            if (!System.IO.Directory.Exists(dir))
                            {
                                throw new Exception(String.Format("Fingerprint not found at {0}", dir));
                            }
                            filePath = String.Format("{0}\\cs.fingerprint", dir);
                            m_thumbprint = System.IO.File.ReadAllText(filePath);
                        }
                        catch (Exception ex)
                        {
                            throw new Exception("Unable to load fingerprint", ex);
                        }
                    }
                    m_thumbprintLoaded = true;
                }
            }

            thumbprint = m_thumbprint;
        }

    }
}
