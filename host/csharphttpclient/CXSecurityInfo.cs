using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;

namespace HttpCommunication
{
    public enum InMageComponent
    {
        None,
        Agent,
        CX
    }

    public static class CXSecurityInfo
    {

        static InMageComponent installedComponent;
        static string accessKeyID;
        static string passphrase;
        static string fingerprint;       

        /*
         * Get csps installpath
         */

        private static void GetCSPSInstallPath(ref string cspsInstallPath)
        {
            try
            {
                string programDataPath = Environment.GetEnvironmentVariable("ProgramData");
                string installerConf = programDataPath + "\\Microsoft Azure Site Recovery\\Config\\App.conf";
                var data = new Dictionary<string, string>();
                foreach (var row in File.ReadAllLines(installerConf))
                {
                    if (row.Contains("="))
                    {
                        string key = row.Split('=')[0];
                        string value = row.Split('=')[1];
                        data.Add(key.Trim(), value.Trim(new Char[] { ' ', '"' }));
                    }
                }

                cspsInstallPath = data["INSTALLATION_PATH"];

                if (string.IsNullOrEmpty(cspsInstallPath))
                {
                    string error = "Failed to get the csps install path.Error: either the key INSTALLATION_PATH in  " + installerConf + " doesn't exist or it's value is empty.";
                    throw new SecurityInfoNotFoundException(error);
                }
            }
            catch (Exception exc)
            {
                throw new SecurityInfoNotFoundException("Unable to get CS install path", exc);
            }
             
        }

        /*
         * Get installed Component of InMage.
         */
        public static InMageComponent GetInstalledComponent()
        {
            if (installedComponent == InMageComponent.None)
            {
                Microsoft.Win32.RegistryKey hklm = null;
                try
                {
                    hklm = Microsoft.Win32.Registry.LocalMachine.OpenSubKey("SOFTWARE\\Wow6432Node\\InMage Systems\\Installed Products");
                    if (hklm == null)
                    {
                        hklm = Microsoft.Win32.Registry.LocalMachine.OpenSubKey("SOFTWARE\\InMage Systems\\Installed Products");
                        if (hklm == null)
                        {
                            throw new SecurityInfoNotFoundException("InMage Component is not installed");
                        }
                    }

                    if (hklm.OpenSubKey("9") != null)
                    {
                        installedComponent = InMageComponent.CX;
                    }
                    else if(hklm.OpenSubKey("5") != null)
                    {
                        installedComponent = InMageComponent.Agent;
                    }
                }
                catch (SecurityInfoNotFoundException)
                {
                    throw;
                }
                catch (Exception ex)
                {
                    throw new SecurityInfoNotFoundException("Unable to get installed InMage Component", ex);
                }
                finally
                {
                    if (hklm != null)
                    {
                        hklm.Close();
                    }
                }
            }
            return installedComponent;
        }

        /*
         * Get Access Key ID
         */
        public static string GetAccessKeyID()
        {
            InMageComponent component = GetInstalledComponent();
            if (String.IsNullOrEmpty(accessKeyID))
            {
                if (component == InMageComponent.Agent)
                {
                    accessKeyID = GetAgentHostGUID();
                }
                else if (component == InMageComponent.CX)
                {
                    accessKeyID = GetCXHostGUID();
                }
            }
            return accessKeyID;
        }

        /*
         * Get CX Host GUID
         */
        public static string GetCXHostGUID()
        {
            string hostGUID = String.Empty;
            try
            {
                string csInstallPath ="";
                GetCSPSInstallPath(ref csInstallPath);
                string  CxConfigFilePath = csInstallPath + "\\home\\svsystems\\etc\\amethyst.conf";

                string[] formattedNameValues = System.IO.File.ReadAllLines(CxConfigFilePath);
                foreach (string nameValue in formattedNameValues)
                {
                    if (!string.IsNullOrEmpty(nameValue) && nameValue.StartsWith("HOST_GUID", StringComparison.OrdinalIgnoreCase))
                    {
                        string[] nameValueTokens = nameValue.Split('=');
                        if (nameValueTokens.Length == 2)
                        {
                            hostGUID = nameValueTokens[1].Trim(new char[] { ' ', '\"', '\'' });
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                throw new SecurityInfoNotFoundException("Unable to get CX Host GUID", ex);
            }
            return hostGUID;
        }

        /*
         * Get Agent Host GUID
         */
        public static string GetAgentHostGUID()
        {
            Microsoft.Win32.RegistryKey hklm = null;
            try
            {
                hklm = Microsoft.Win32.Registry.LocalMachine.OpenSubKey("SOFTWARE\\Wow6432Node\\SV Systems");
                if (hklm == null)
                {
                    hklm = Microsoft.Win32.Registry.LocalMachine.OpenSubKey("SOFTWARE\\SV Systems");
                    if (hklm == null)
                    {
                        throw new SecurityInfoNotFoundException("Agent Host GUID not found");
                    }
                }
                Object hostGuid = hklm.GetValue("HostId");
                return hostGuid.ToString();
            }
            catch (Exception ex)
            {
                throw new SecurityInfoNotFoundException("Unable to get Agent Host GUID", ex);
            }
            finally
            {
                if (hklm != null)
                {
                    hklm.Close();
                }
            }
        }

        /*
         * Get passphrase string
         */
        public static string ReadPassphrase(string passphraseDir, bool readNewPassphrase = false)
        {
            if (String.IsNullOrEmpty(passphrase) || readNewPassphrase)
            {
                try
                {
                    string filePath;
                    if (String.IsNullOrEmpty(passphraseDir) || !System.IO.Directory.Exists(passphraseDir))
                    {
                        passphraseDir = Environment.ExpandEnvironmentVariables("%ProgramData%\\Microsoft Azure Site Recovery\\private");
                        if (!System.IO.Directory.Exists(passphraseDir))
                        {
                            throw new SecurityInfoNotFoundException("CS Passphrase not found");
                        }

                    }
                    filePath = String.Format("{0}\\connection.passphrase", passphraseDir);
                    passphrase = System.IO.File.ReadAllText(filePath);
                    if (String.IsNullOrEmpty(passphrase))
                    {
                        throw new SecurityInfoNotFoundException("Passphrase is empty.");
                    }
                }
                catch (Exception ex)
                {
                    throw new SecurityInfoNotFoundException("Unable to load CS passphrase", ex);
                }
            }
            return passphrase;
        }

        /*
         * Get fingerprint string
         */
        public static string ReadFingerprint(string fingerprintDir, string cxIP, int port, bool readNewFingerPrint = false)
        {
            if (String.IsNullOrEmpty(fingerprint) || readNewFingerPrint)
            {
                try
                {
                    string filePath;
                    if (String.IsNullOrEmpty(fingerprintDir) || !System.IO.Directory.Exists(fingerprintDir))
                    {
                        fingerprintDir = Environment.ExpandEnvironmentVariables("%ProgramData%\\Microsoft Azure Site Recovery\\fingerprints");
                        if (!System.IO.Directory.Exists(fingerprintDir))
                        {
                            throw new SecurityInfoNotFoundException("CS fingerprint not found");
                        }
                    }
                    filePath = String.Format("{0}\\cs.fingerprint", fingerprintDir);
                    if (!System.IO.File.Exists(filePath))
                    {
                        filePath = String.Format("{0}\\{1}_{2}.fingerprint", fingerprintDir, cxIP, port);
                        if (!System.IO.File.Exists(filePath))
                        {
                            throw new SecurityInfoNotFoundException("CS fingerprint not found");
                        }
                    }
                    fingerprint = System.IO.File.ReadAllText(filePath);
                }
                catch (SecurityInfoNotFoundException ex)
                {
                    throw ex;
                }
                catch (Exception ex)
                {
                    throw new SecurityInfoNotFoundException("Unable to load CS fingerprint", ex);
                }
            }
            return fingerprint;
        }
    }
}
