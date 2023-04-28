using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using Microsoft.Win32;
using System.IO;
using System.Net;
using System.Net.NetworkInformation;
using System.Management;
using System.Web.UI.WebControls;
using System.Threading;
using Microsoft.SharePoint.Administration;
using System.Diagnostics;


namespace spapp
{
    class Configuration
    {                      
        public static String SPAPP_TRACE_DIR = "\\Application data\\SPAPPTrace\\";

        public static string getCopyrightFromAssembly()
        {
            string copyrightStr = String.Empty;
            object[] copyrightAtts = System.Reflection.Assembly.GetExecutingAssembly().GetCustomAttributes(typeof(System.Reflection.AssemblyCopyrightAttribute), false);
            if (copyrightAtts.Length != 0)
            {
                copyrightStr = ((System.Reflection.AssemblyCopyrightAttribute)copyrightAtts[0]).Copyright;
            }
            return copyrightStr;
        }

        public static Uri GetCxUri(string absolutePath)
        {
            Object objValue = null;
            string serverName = null;
            string portNumber = null;
            string protocol = "http";
            RegistryKey installationRegKey = null;
            Uri uri = null;
            try
            {
                installationRegKey = Registry.LocalMachine.OpenSubKey("SOFTWARE\\Wow6432Node\\SV Systems\\");
                if (installationRegKey == null)
                {
                    installationRegKey = Registry.LocalMachine.OpenSubKey("SOFTWARE\\SV Systems\\");
                    if (installationRegKey == null)
                    {
                        Trace.WriteLine(DateTime.Now.ToString() + "\tFailed to open registry key.");
                        return null;
                    }
                }

                objValue = installationRegKey.GetValue("ServerName");
                if (objValue == null)
                {
                    Trace.WriteLine(DateTime.Now.ToString() + "\tServer name or IP address of CX not found");
                    return null;
                }
                serverName = objValue.ToString();

                objValue = installationRegKey.GetValue("ServerHttpPort");
                if (objValue == null)
                {
                    Trace.WriteLine(DateTime.Now.ToString() + "\tPort number of CX not found");
                    return null;
                }
                portNumber = objValue.ToString();

                objValue = installationRegKey.GetValue("Https");
                if (objValue != null && (int)objValue == 1)
                {
                    protocol = "https";
                }
                string uriStr = String.Format("{0}://{1}:{2}", protocol, serverName, portNumber);
                if (!String.IsNullOrEmpty(absolutePath))
                {
                    uriStr += absolutePath;
                }
                uri = new Uri(uriStr);
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now.ToString() + "\tFailed to get CX configuration. Error: " + ex.Message);
                return null;
            }
            finally
            {
                if (installationRegKey != null)
                {
                    installationRegKey.Close();
                }
            }
            return uri;
        }

        public static bool IpReachable(String Ip_in)
        {
            Trace.WriteLine(DateTime.Now.ToString() + "\t" +  "Entered Configuration IpReachable");
            Ping pingSender = new Ping();
            PingOptions options = new PingOptions();
            bool Result = false;

            try
            {
                // Create a buffer of 32 bytes of data to be transmitted.
                string data = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
                byte[] buffer = Encoding.ASCII.GetBytes(data);
                int timeout = 120;

                PingReply reply = pingSender.Send(Ip_in, timeout, buffer, options);

                if (reply.Status == IPStatus.Success)
                {
                    Result = true;
                }
                else
                {
                    Result = false;
                }
            }
            catch (Exception e)
            {
                Console.WriteLine("IP is Not reachable:" + e.Message);
                Result = false;
            }

            Trace.WriteLine(DateTime.Now.ToString() + "\t" +  "Exited Configuration IpReachable");
            return Result;
        }

        public static String vxAgentPath()
        {
            Trace.WriteLine(DateTime.Now.ToString() + "\t" +  "Entered Configuration vxAgentPath");
            Object obp;
            RegistryKey hklm = Registry.LocalMachine;
            RegistryKey hklm32 = Registry.LocalMachine;
            String VxDir = "";

            hklm = hklm.OpenSubKey("SOFTWARE\\Wow6432Node\\SV Systems\\VxAgent");
            hklm32 = hklm32.OpenSubKey("SOFTWARE\\SV Systems\\VxAgent");


            if (hklm == null && hklm32 == null)
            {
                return null;
            }

            else
            {
                if (hklm != null)
                {
                    obp = hklm.GetValue("InstallDirectory");
                }
                else
                {
                    obp = hklm32.GetValue("InstallDirectory");
                }
                VxDir = obp.ToString();
             }
            Trace.WriteLine(DateTime.Now.ToString() + "\t" +  "Exited Configuration vxAgentPath");
            return VxDir;
        }

        public static String GetRemoteRegistryValue(String computerName, RegistryHive root, string subKey, string valueName)
        {
            String result = String.Empty;
            try
            {
                RegistryKey hKey = RegistryKey.OpenRemoteBaseKey(root, computerName);
                RegistryKey hSubKey = hKey.OpenSubKey(subKey);
                result = (string)hSubKey.GetValue(valueName);

                hKey.Close();
            }
            catch (Exception ex)
            {
                Trace.WriteLine(String.Format("{0}\tFailed to read registry value \"{1}\" from the host \"{2}\". Exception:{3}", DateTime.Now.ToString(), valueName, computerName, ex.ToString()));
            }
            return result;
        }

        public static string GetRemoteRegistryValue(string servername, String regKeyToGet)
        {

            string keyToRead = "StringValue";

            ConnectionOptions oConn = new ConnectionOptions();
            oConn.Impersonation = ImpersonationLevel.Impersonate;

            ManagementScope scope = new ManagementScope(@"//" + servername + @"/root/default", oConn);
            ManagementClass registry = new ManagementClass(scope, new ManagementPath("StdRegProv"), null);

            // Returns a specific value for a specified key 
            ManagementBaseObject inParams = registry.GetMethodParameters("GetStringValue");
            inParams["sSubKeyName"] = regKeyToGet;
            inParams["sValueName"] = keyToRead;
            ManagementBaseObject outParams = registry.InvokeMethod("GetStringValue", inParams, null);

            return outParams["sValue"].ToString();
        }

        public bool SharePointInstallationCheck()
        {
            Trace.WriteLine(DateTime.Now.ToString() + "\t" +  "Entered Configuration SharePointInstallationCheck");
            bool result = false;
            RegistryKey SP2k7 = Registry.LocalMachine;
            RegistryKey SP2k10 = Registry.LocalMachine;
            RegistryKey SP2k13 = Registry.LocalMachine;
            SP2k7 = SP2k7.OpenSubKey("SOFTWARE\\Microsoft\\Office Server\\12.0");
            SP2k10 = SP2k10.OpenSubKey("SOFTWARE\\Microsoft\\Office Server\\14.0");
            SP2k13 = SP2k13.OpenSubKey("SOFTWARE\\Microsoft\\Office Server\\15.0");
            if (SP2k7 == null && SP2k10 == null && SP2k13 == null)
            {
                result = false;
            }
            else if (SP2k7 != null || SP2k10 != null || SP2k13 != null)
            {
                result = true;
                ////Finding the sharePoint version
                //System.Version SPIstallVersion = SPFarm.Local.BuildVersion;
                //String SharePointVersion = SPIstallVersion.ToString();

                //if (!string.IsNullOrEmpty(SharePointVersion))
                //{
                //    Console.WriteLine("Detected SharePoint Version :" + SharePointVersion + "\n");
                //}
                //else
                //{
                //    Console.WriteLine("No sharePoint Installation detected\nExiting....\n");
                //    Application.Exit();
                //}
            }
            Trace.WriteLine(DateTime.Now.ToString() + "\t" +  "Exited Configuration SharePointInstallationCheck");
            return result;
        }
        public static int GetIISVersion()
        {
            Trace.WriteLine(DateTime.Now.ToString() + "\t" +  "Entered Configuration GetIISVersion");
            int IISversion;
            Object iisregobj;
            RegistryKey hklm = Registry.LocalMachine;
            hklm = hklm.OpenSubKey("SOFTWARE\\Microsoft\\InetStp");
            if (hklm == null)
            {
                Console.WriteLine("Not able to determine the IIS version\n");
                return -1;
            }
            else
            {
                iisregobj = hklm.GetValue("MajorVersion");
                IISversion = int.Parse(iisregobj.ToString());
            }
            Trace.WriteLine(DateTime.Now.ToString() + "\t" +  "Exited Configuration GetIISVersion");
            return IISversion;
        }

        public static int GetSharePointVersion()
        {
            Trace.WriteLine(DateTime.Now.ToString() + "\t" +  "Entered Configuration GetSharePointVersion");
            //int SPVersion;
            //SPVersion = SPFarm.Local.BuildVersion.Major;
            //return SPVersion;
            String[] obp;
            int SPVersion = 0;
            RegistryKey hklm = Registry.LocalMachine;
            String RegKey = "SOFTWARE\\Microsoft\\Office Server";
            hklm = hklm.OpenSubKey(RegKey);

            if (hklm == null)
            {
                System.Console.WriteLine("Not able to open RegistryKey : " + RegKey);
            }
            else
            {
                obp = hklm.GetSubKeyNames();
                foreach (String str in obp)
                {
                    double Num;
                    if (double.TryParse(str, out Num))
                    {
                        if ((int)Num > SPVersion)
                            SPVersion = (int)Num;
                    }
                }
            }
            Trace.WriteLine(DateTime.Now.ToString() + "\t" +  "Exited Configuration GetSharePointVersion");
            return SPVersion;
        }

        //this is to get .NET framework 2.0 root path
        public static String GetDotnetFrameWorkRootpath()
        {
            Trace.WriteLine(DateTime.Now.ToString() + "\t" +  "Entered Configuration GetDotnetFrameWorkRootpath");
            String DotNetRootpath = "";
            Object obp;
            RegistryKey hklm = Registry.LocalMachine;
            hklm = hklm.OpenSubKey("SOFTWARE\\Microsoft\\.NETFramework");
            if (hklm == null)
            {
                Console.WriteLine("Failed to open registry key : HKLM\\SOFTWARE\\Microsoft\\.NETFramework");
                return null;
            }
            else
            {
                obp = hklm.GetValue("InstallRoot");
                DotNetRootpath = obp.ToString();
            }
            Trace.WriteLine(DateTime.Now.ToString() + "\t" +  "Exited Configuration GetDotnetFrameWorkRootpath");
            return DotNetRootpath;
        }
    }
}

