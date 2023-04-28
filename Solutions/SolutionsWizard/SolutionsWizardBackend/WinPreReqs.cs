using System;
using System.Collections.Generic;
using System.Text;
using System.Net.NetworkInformation;
using System.Diagnostics;
using System.IO;
using System.Xml;
using System.Net;
using System.Windows.Forms;
using System.Collections;
using System.Management;
using System.Threading;
using HttpCommunication;

namespace Com.Inmage.Tools
{//Error Codes Range: 01_003

    public class WinPreReqs
    {
        internal string IP, Username, Password, Domain;
        internal static int Agentheartbeatthreshold = 900;
        internal string Latestpath = WinTools.FxAgentPath() + "\\vContinuum\\Latest";
        internal string Vcontinuumpath = WinTools.FxAgentPath() + "\\vContinuum";
        public static string Mtvxpath = null;
        public static ArrayList refplanList = new ArrayList();
        public static string refError = null;
        public static Host refHost = new Host();
        public static string refIPaddress = null;
        public static string refOutput = null;
        public static string refHostname = null;
        public static HostList duplicateList = new HostList();
        public static string tempXml = "\\temp.xml";
        public WinPreReqs(string inIp, string inDomain, string inUserName, string inPassWord)
        {
           
            IP = inIp;
            Username = inUserName;
            Password = inPassWord;
            Domain = inDomain;

        }

        public static string GetIPaddress
        {
            get
            {
                return refIPaddress;
            }
            set
            {
                value = refIPaddress;
            }
        }
        public static Host GetHost
        {
            get
            {
                return refHost;
            }
            set
            {
                value = refHost;
            }
        }

        public static string GetError
        {
            get
            {
                return refError;
            }
           set
            {
                value = refError;
            }
        }

        public static string GetOutput
       {
           get
           {
               return refOutput;
           }
            set
           {
               value = refOutput;
           }
       }

        public static string GetHostname
        {
            get
            {
                return refHostname;
            }
            set
            {
                value = refHostname;
            }
        }

        public static bool WindowsShareEnabled(string ip, string domain, string userName, string passWord, string hostname, string error)
        {
            Trace.WriteLine(DateTime.Now + "\t Entering:WindowsShareEnabled:Using domain:" + domain);
            string mappedDrive = "";
            string wmiError = "";
            WmiCode errorCode = WmiCode.Unknown;
            string originalIP = null;
            originalIP = ip;
           

            Trace.WriteLine(DateTime.Now + "\t Successfully came here after comparing the hostname and domain");

            try
            {
                Trace.WriteLine(DateTime.Now + "\t Printing the value of ip " + ip);
                if (ip != ".")
                {
                    /*if (IpReachable(ip) == false)
                    {
                        //Try once again before failing.
                        if (IpReachable(ip) == false)
                        {
                            error = Environment.NewLine + ip + " is not reachable. Please check that firewall is not blocking ";
                            //MessageBox.Show(ip + " is not reachable. Please check that firewall is not blocking ");
                            errorCode = Wmi_Codes.NOT_REACHABLE;
                            return false;
                        }
                    }*/
                }
                else
                {
                    domain = null;
                    userName = null;
                    passWord = null;
                }

                RemoteWin remote = new RemoteWin(ip, domain, userName, passWord,hostname);

                if (remote.Connect( wmiError,  errorCode) == false)
                {
                    if (remote.Connect( wmiError,  errorCode) == false)
                    {
                        errorCode = remote.GetWmiCode;
                        wmiError = remote.GetErrorString;
                        error = "ERROR: " + wmiError;

                        Trace.WriteLine(" WinPreReqs:WindowsShareEnabled " + "WMI Error code = " + errorCode + " " + error);

                        switch (errorCode)
                        {
                            case WmiCode.RpcError:

                                error += Environment.NewLine + Environment.NewLine + "Try following to resolve this issue" +
                                                Environment.NewLine +
                                                Environment.NewLine + " 1. Ensure that firewall is not blocking " +
                                                Environment.NewLine +
                                                Environment.NewLine + " 2. WMI is functioning properly with following steps" +
                                                Environment.NewLine + "           On " + ip + " go to Computer Management->\"Services and Applications\"->\"WMI Control\", right click" +
                                                Environment.NewLine + "            and select properties." +
                                                Environment.NewLine + "           If you see \"WMI: Initialization failure\", please run below command in case of windows 2003 machines" +
                                                Environment.NewLine + "            \"rundll32 wbemupgd, RepairWMISetup\" ";
                                               
                                if (domain.Length > 0)
                                {

                                    error += Environment.NewLine + Environment.NewLine + "3. Use local user account instead of domain account " +
                                             Environment.NewLine;
                                }

                                break;

                            case WmiCode.Credentialerror:
                                error += Environment.NewLine + Environment.NewLine + "Please verify that " +
                                             Environment.NewLine +
                                             Environment.NewLine + "1. Credentials provided are correct " +
                                             Environment.NewLine + "2. User is part of either local or domain administrators group" +
                                             Environment.NewLine + "3. WMI(Distributed COM) is enabled on remote machine: " + ip;
                                             

                                //In case of domain user, netlogon services is not needed
                                if (domain.Length == 0)
                                {
                                    error += Environment.NewLine + "4. Netlogon, Workstation services are running on remote machine: " + ip;
                                }
                                else
                                {
                                    error += Environment.NewLine + "4. Clock time of system with IP: " + ip + " matches with the clock time of domain controller";
                                }
                                break;
                            case WmiCode.Unknown:
                                break;
                            default:
                                error += Environment.NewLine + " Verify that WMI is functioning properly on: " + ip;
                                break;
                        }

                        refError = error;
                        //MessageBox.Show(messageString);

                        return false;
                    }
                }
                Hashtable sysInfoHash = new Hashtable();
                remote.GetSystemInfo(sysInfoHash);
                Trace.WriteLine(DateTime.Now + "\t\t WMI verification succeeded.Now verifying the network map drive");
                if (originalIP != ".")
                {
                   
                    Trace.WriteLine(DateTime.Now + " \t Came here to map network drive of this machine " + originalIP);
                   
                    if (remote.MapRemoteDirToDrive( mappedDrive) == false)
                    {

                        error = "Could not access access C drive of " + ip + " as remote share " + 
                            Environment.NewLine + "Please check that" + 
                            Environment.NewLine + " 1. Credentials are correct " +
                            Environment.NewLine + " 2. NetLogon,WorkStation and Server services are up and running on remote machine" + 
                            Environment.NewLine + " 3. Check whether remote machine's C$ already mapped to this machine with different drive letter. Please disconnect mapped drive." +
                            Environment.NewLine + " 4. Client for Microsoft Networks is enable for NIC.You can check in NIC properties: " + ip;
                        refError = error;
                        return false;

                    }
                    else
                    {
                        mappedDrive = RemoteWin.GetMapNetworkdrive;
                        RemoteWin.Detach(mappedDrive);
                        Trace.WriteLine(DateTime.Now + "\t\t Able to map the network drive.");
                        Trace.WriteLine(DateTime.Now + "\t Exiting:WindowsShareEnabled:Successfully completed winprereqs windows share enabled");
                        refError = error;
                        return true;
                    }
                }
                else
                {
                    Trace.WriteLine(DateTime.Now + " \t Came here because we had skipped the map network drive because vCon and mt are same");
                    refError = error;
                    return true;
                }



            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                System.FormatException formatException = new FormatException("Inner exception", ex);
                Trace.WriteLine(formatException.GetBaseException());
                Trace.WriteLine(formatException.Data);
                Trace.WriteLine(formatException.InnerException);
                Trace.WriteLine(formatException.Message);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine("Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                refError = error;
                return false;

            }


        }

        public static bool IPreachable(String ipaddress)
        {

            Ping pingSender = new Ping();
            PingOptions options = new PingOptions();

            try
            {
                //waits 1200 milli seconds to get the response from server.
                // Create a buffer of 32 bytes of data to be transmitted.
                string data = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
                byte[] buffer = Encoding.ASCII.GetBytes(data);
                int timeout = 1200;

                PingReply reply = pingSender.Send(ipaddress, timeout, buffer, options);

                if (reply.Status == IPStatus.Success)
                {
                    return true;
                }
                else
                {
                    //Try once again before returning false;
                    reply = pingSender.Send(ipaddress, timeout, buffer, options);

                    if (reply.Status != IPStatus.Success)
                    {
                        return false;
                    }
                    else
                    {
                        return true;
                    }
                }

            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                System.FormatException formatException = new FormatException("Inner exception", ex);
                Trace.WriteLine(formatException.GetBaseException());
                Trace.WriteLine(formatException.Data);
                Trace.WriteLine(formatException.InnerException);
                Trace.WriteLine(formatException.Message);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine("Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }

        }

        //internal static bool IsServiceInstalled(RemoteWin remote , string inServiceName,ref bool inStatus)
        //{
        //    try
        //    {
        //        string errorMessage = "";
        //        WmiCode errorCode = WmiCode.Unknown;

        //        remote.Connect( errorMessage,  errorCode);


        //        errorCode = remote.GetWmiCode;
        //        errorMessage = remote.GetErrorString;
        //        if (remote.ServiceExists(inServiceName, ref inStatus))
        //        {
        //            return true;
        //        }
        //    }
        //    catch (Exception ex)
        //    {

        //        System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
        //        System.FormatException formatException = new FormatException("Inner exception", ex);
        //        Trace.WriteLine(formatException.GetBaseException());
        //        Trace.WriteLine(formatException.Data);
        //        Trace.WriteLine(formatException.InnerException);
        //        Trace.WriteLine(formatException.Message);
        //        Trace.WriteLine("ERROR*******************************************");
        //        Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
        //        Trace.WriteLine("Exception  =  " + ex.Message);
        //        Trace.WriteLine("ERROR___________________________________________");
        //        return false;

        //    }
        //   return false;
        //}

        


        public bool GetDetailsFromcxcli(Host host, string incxIP)
         {
             string inPutString = "<plan> " + Environment.NewLine + "<failover_protection> " + Environment.NewLine + "                <replication> " + Environment.NewLine + "                <server_ip>" + host.ip + "</server_ip>" + Environment.NewLine +
                                    "                </replication>" + Environment.NewLine + " </failover_protection>" + Environment.NewLine + " </plan>";
            
             string[] words = host.hostname.Split('.');
             string hostname;
             string hostnameOriginal = words[0];
             hostname = words[0].ToUpper();
            
             Trace.WriteLine(inPutString);
             try
             {
                
                 String postData1 = "&operation=";
                 // xmlData would be send as the value of variable called "xml"
                 // This data would then be wrtitten in to a file callied input.xml
                 // on CX and then given as argument to the cx_cli.php script.
                 int operation = 3;
                 String postData2 = "&userfile='input.xml'&xml=" + inPutString;
                 String postData = postData1 + operation + postData2;
                 HttpReply reply = ConnectToHttpClient(postData);
                 if (String.IsNullOrEmpty(reply.error))
                 {
                     // process reply
                     if (!String.IsNullOrEmpty(reply.data))
                     {
                         Trace.WriteLine(reply.data);
                     }
                 }
                 else
                 {

                     Trace.WriteLine(reply.error);
                     return false;
                 }
                 string responseFromServer = reply.data;
                 if ((!responseFromServer.Contains(hostname)) && (!responseFromServer.Contains(host.ip)))
                 {
                     host.pointedToCx = false;
                     refHost = host;
                     return false;
                 }
                 // MessageBox.Show(responseFromServer);
                 //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
                 //based on above comment we have added xmlresolver as null
                 XmlDocument xDocument = new XmlDocument();
                 xDocument.XmlResolver = null;
                 //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
                 XmlReaderSettings settings = new XmlReaderSettings();
                 settings.ProhibitDtd = true;
                 settings.XmlResolver = null;
                 using (XmlReader reader1 = XmlReader.Create(Latestpath + tempXml, settings))
                 {
                     xDocument.Load(reader1);
                    // reader1.Close();
                 }
                 //xDocument.Load(Latestpath + tempXml);
               XmlNodeList  hostnode = xDocument.GetElementsByTagName("host");
               foreach (XmlNode node in hostnode)
               {
                   XmlAttributeCollection attribColl = node.Attributes;
                   if (node.Attributes["id"] != null)
                   {
                       host.inmage_hostid = attribColl.GetNamedItem("id").Value.ToString();
                       Trace.WriteLine(DateTime.Now + "\t Printing id " + host.inmage_hostid);
                   }
               }
                 XmlNodeList xPNodeList = xDocument.SelectNodes(".//vx_agent_path");
                 foreach (XmlNode xNode in xPNodeList)
                 {
                     host.vx_agent_path = (xNode.InnerText != null ? xNode.InnerText : "");
                     if (host.vx_agent_path == "")
                     {
                         Trace.WriteLine("ERROR: GetDetailsFromCxCli: " + host.displayname + " ( " + host.ip + " ) :" + " VX agent not installed");
                         host.vxagentInstalled = false;
                     }
                     else
                     {                        
                         host.vxagentInstalled = true;
                     }
                 }
                 xPNodeList = xDocument.SelectNodes(".//fx_agent_path");
                 foreach (XmlNode xNode in xPNodeList)
                 {
                     host.fx_agent_path = (xNode.InnerText != null ? xNode.InnerText : "");
                     if (host.fx_agent_path == "")
                     {
                         Trace.WriteLine("ERROR: GetDetailsFromCxCli: " + host.displayname + " ( " + host.ip + " ) :" + " FX agent not installed");
                         host.fxagentInstalled = false;
                     }
                     else
                     {                         
                         host.fxagentInstalled = true;
                     }
                 }
                 xPNodeList = xDocument.SelectNodes(".//vx_heartbeat_diff_in_seconds");
                 foreach (XmlNode xNode in xPNodeList)
                 {
                     if(xNode.InnerText != null)
                     {
                         host.vxAgentHeartBeat = xNode.InnerText;
                     }
                     if (int.Parse(xNode.InnerText != null ? xNode.InnerText : "") <= Agentheartbeatthreshold)
                     {
                         host.vx_agent_heartbeat = true;
                     }
                     else
                     {                         
                         host.vx_agent_heartbeat = false;
                         Trace.WriteLine("ERROR: GetDetailsFromCxCli: " + host.displayname + " ( " + host.ip + " ) :" + " VX agent heartbeat had exceeded " + Agentheartbeatthreshold + " seconds " + xNode.InnerText.ToString());
                     }
                 }
                 xPNodeList = xDocument.SelectNodes(".//fx_heartbeat_diff_in_seconds");
                 foreach (XmlNode xNode in xPNodeList)
                 {
                     if (xNode.InnerText != null)
                     {
                         host.fxAgentHeartBeat = xNode.InnerText;
                     }
                     if (int.Parse(xNode.InnerText != null ? xNode.InnerText : "") <= Agentheartbeatthreshold)
                     {
                         host.fx_agent_heartbeat = true;
                     }
                     else
                     {
                         host.fx_agent_heartbeat = false;
                         Trace.WriteLine("ERROR: GetDetailsFromCxCli: " + host.displayname + " ( " + host.ip + " ) :" + " FX agent heartbeat had exceeded " + Agentheartbeatthreshold + " seconds " + xNode.InnerText.ToString());
                     }
                 }
                 xPNodeList = xDocument.SelectNodes(".//vx_licensed");
                 foreach (XmlNode xNode in xPNodeList)
                 {
                     string value;
                     value = (xNode.InnerText != null ? xNode.InnerText : "");
                     if (value == "Yes")
                     {
                         host.vxlicensed = true;
                     }
                     else
                     {
                         host.vxlicensed = false;
                         Trace.WriteLine("ERROR: GetDetailsFromCxCli: " + host.displayname + " ( " + host.ip + " ) :" + " VX agent not licensed");
                     }
                 }
                 xPNodeList = xDocument.SelectNodes(".//fx_licensed");
                 foreach (XmlNode xNode in xPNodeList)
                 {
                     string value;
                     value = (xNode.InnerText != null ? xNode.InnerText : "");
                     if (value == "Yes")
                     {
                         host.fxlicensed = true;
                     }
                     else
                     {
                         host.fxlicensed = false;
                         Trace.WriteLine("ERROR: GetDetailsFromCxCli: " + host.displayname + " ( " + host.ip + " ) :" + " FX agent not licensed");
                     }
                 }
                 xPNodeList = xDocument.SelectNodes(".//agent_time_zone");
                 foreach (XmlNode xNode in xPNodeList)
                 {                     
                     host.timeZone = (xNode.InnerText != null ? xNode.InnerText : "");                    
                 }
                 xPNodeList = xDocument.SelectNodes(".//master_target");
                 foreach (XmlNode xNode in xPNodeList)
                 {
                    if(xNode.InnerText != null && xNode.InnerText.ToUpper() == "TRUE")
                    {
                        host.masterTarget = true;
                    }
                 }

               
             }
             //Process_Server_List.Update();
             catch (System.Net.WebException e)
             {
                 System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);
                 System.FormatException formatException = new FormatException("Inner exception", e);
                 Trace.WriteLine(formatException.GetBaseException());
                 Trace.WriteLine(formatException.Data);
                 Trace.WriteLine(formatException.InnerException);
                 Trace.WriteLine(formatException.Message);
                 Trace.WriteLine("ERROR*******************************************");
                 Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                 Trace.WriteLine("Exception  =  " + e.Message);
                 Trace.WriteLine("ERROR___________________________________________");
                 Trace.WriteLine("This error from GetDetailsFromCxCli() method");
                 Trace.WriteLine(e.ToString());
                 //MessageBox.Show("Server is not pointed to the CX.Please veriry whether agents are installed or not.And they are pointing to " + inCXIP);
                 host.pointedToCx = false;
                 return false;
             }
             catch (Exception e)
             {
                 System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);
                 System.FormatException formatException = new FormatException("Inner exception", e);
                 Trace.WriteLine(formatException.GetBaseException());
                 Trace.WriteLine(formatException.Data);
                 Trace.WriteLine(formatException.InnerException);
                 Trace.WriteLine(formatException.Message);
                 Trace.WriteLine("ERROR*******************************************");
                 Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                 Trace.WriteLine("Exception  =  " + e.Message);
                 Trace.WriteLine("ERROR___________________________________________");
                 return false;
             }
             host.pointedToCx = true;
             refHost = host;
             return true;

         }


        public HttpReply ConnectToHttpClient(string content)
        {
             string cxip = WinTools.GetcxIP();
             string portnumber = WinTools.GetcxPortNumber();
             HttpReply reply;
             string vxAgent = WinTools.Vxagentpath();
             string path = null;
            ////if(Directory.Exists(path + "Program"))
             if (8 == IntPtr.Size  || (!String.IsNullOrEmpty(Environment.GetEnvironmentVariable("PROCESSOR_ARCHITEW6432"))))
             {
                 path = Environment.GetEnvironmentVariable("ProgramFiles(x86)");
             }
             else
             {

                 path = Environment.GetEnvironmentVariable("ProgramFiles");
             }

             
             path = path + "\\InMage Systems\\fingerprints";
             string fingerPrintsPath = path;
            if (WinTools.Https == false)
            {
                HttpClient http = new HttpClient(cxip, int.Parse(portnumber), false, 300000, fingerPrintsPath);
                 reply = http.sendHttpRequest("POST", "/cli/cx_cli_no_upload.php", content, "application/x-www-form-urlencoded",null);
          
            }
            else
            {
                Trace.WriteLine(DateTime.Now + "\t Going with Https ");
                HttpClient http = new HttpClient(cxip, int.Parse(portnumber), true, 300000, fingerPrintsPath);
              reply = http.sendHttpRequest("POST", "/cli/cx_cli_no_upload.php", content, "application/x-www-form-urlencoded",null);
               
            }
            Trace.WriteLine(DateTime.Now + "\t Reply " + reply.data);
            try
            {
                if (File.Exists(Latestpath + tempXml))
                {
                    File.Delete(Latestpath + tempXml);
                }
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed to delete temp.xml from latest folder " + ex.Message);
            }

            if (String.IsNullOrEmpty(reply.error))
            {
                // process reply
                if (!String.IsNullOrEmpty(reply.data))
                {
                    StreamWriter sr1 = new StreamWriter(Latestpath + tempXml);
                    sr1.Write(reply.data);
                    sr1.Close();
                }
            }
            else
            {
                Trace.WriteLine(reply.error);               
            }                          
            return reply;
            
        }

        public bool CheckIfcxWorking()
        {



            string _xmlString = string.Empty;
           
            Debug.WriteLine(_xmlString);

            try
            {



                String postData1 = "&operation=";

                // xmlData would be send as the value of variable called "xml"
                // This data would then be wrtitten in to a file callied input.xml
                // on CX and then given as argument to the cx_cli.php script.
                int operation = 2;

                String postData2 = "&userfile='input.xml'&xml=" + _xmlString;
                String postData = postData1 + operation + postData2;
                HttpReply responseFromServer = ConnectToHttpClient(postData);
                if (String.IsNullOrEmpty(responseFromServer.error))
                {
                    // process reply
                    if (!String.IsNullOrEmpty(responseFromServer.data))
                    {
                        Trace.WriteLine(responseFromServer.data);
                    }
                }
                else
                {
                    
                    Trace.WriteLine(responseFromServer.error);
                    return false;
                }

            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\tCX server: " + WinTools.GetcxIPWithPortNumber() + " is not available at port: " + WinTools.GetcxPortNumber() + Environment.NewLine + "Please verify that port number in Scout agent configufation is valid.");


                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                System.FormatException formatException = new FormatException("Inner exception", ex);
                Trace.WriteLine(formatException.GetBaseException());
                Trace.WriteLine(formatException.Data);
                Trace.WriteLine(formatException.InnerException);
                Trace.WriteLine(formatException.Message);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine("Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }

            return true;
        }


        public int CheckAnyDuplicateEntries(string hostname, string ipaddress, string incxIP)
        {
            //to set proper ip for a host if host has multiple ips/multiple nics

            string originalHostname = hostname;
            string[] words = hostname.Split('.');
            string hostnameOriginal = words[0];
            int count = 0;
            hostname = words[0].ToUpper();
            if (hostname.Length > 15)
            {
                hostname = hostname.Substring(0, 15);
            }
            duplicateList = new HostList();
            Trace.WriteLine("Looking for " + hostnameOriginal + "  " + hostname);
            string _xmlString = "";
            

            try
            {

                String postData1 = "&operation=";

                // xmlData would be send as the value of variable called "xml"
                // This data would then be wrtitten in to a file callied input.xml
                // on CX and then given as argument to the cx_cli.php script.
                int operation = 3;

                String postData2 = "&userfile='input.xml'&xml=" + _xmlString;


                String postData = postData1 + operation + postData2;
                //  Debug.WriteLine(postData);
                //String postData = "&operation=2";
                HttpReply reply = ConnectToHttpClient(postData);
                if (String.IsNullOrEmpty(reply.error))
                {
                    // process reply
                    if (!String.IsNullOrEmpty(reply.data))
                    {
                        Trace.WriteLine(reply.data);
                    }
                }
                else
                {

                    Trace.WriteLine(reply.error);
                    return 1;
                }
                string responseFromServer = reply.data;

                //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
                //based on above comment we have added xmlresolver as null
                XmlDocument xDocument = new XmlDocument();
                xDocument.XmlResolver = null;
                //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
                XmlReaderSettings settings = new XmlReaderSettings();
                settings.ProhibitDtd = true;
                settings.XmlResolver = null;
                using (XmlReader reader1 = XmlReader.Create(Latestpath + tempXml, settings))
                {
                    xDocument.Load(reader1);
                    //reader1.Close();
                }
               // xDocument.Load(Latestpath + tempXml);



                //Reading volume replication details from cx

                XmlNodeList xPNodeList = xDocument.SelectNodes(".//host");
              
                foreach (XmlNode node in xPNodeList)
                {
                    Host h = new Host();             
                    if (node.ChildNodes.Item(0).InnerText.ToUpper() == hostnameOriginal.ToUpper() || node.ChildNodes.Item(0).InnerText.ToUpper() == originalHostname.ToUpper())
                    {
                        if (node.Attributes["id"] != null && node.Attributes["id"].Value != null && node.Attributes["id"].Value.Length != 0)
                        {
                            h.inmage_hostid = node.Attributes["id"].Value;
                            Trace.WriteLine(DateTime.Now + "\t Printing host id " + h.inmage_hostid);
                        }

                        XmlNodeList childList = node.ChildNodes;
                        foreach (XmlNode nodes in childList)
                        {                           
                            Trace.WriteLine(DateTime.Now + "\tNodes " + nodes.Name);
                            Trace.WriteLine(DateTime.Now + "\tNodes value " + nodes.InnerText);
                            if(nodes.Name == "creation_time" && nodes.InnerText != null && nodes.InnerText.Length != 0)
                            {
                                h.agentFirstReportedTime = nodes.InnerText;
                                Trace.WriteLine(DateTime.Now + "\t Agent report time " + h.agentFirstReportedTime);
                                break;
                            }
                        }

                        Trace.WriteLine(DateTime.Now + "\t Found hostname and reading required values for the host" + hostname);
                        Trace.WriteLine("*******Found Source IP address***********");
                        ipaddress = node.ChildNodes.Item(1).InnerText;
                        Trace.WriteLine("Hostname:" + hostname);
                        Trace.WriteLine("IpAddress:" + ipaddress);
                        h.ip = ipaddress;
                        Trace.WriteLine(DateTime.Now + "\t Reading required attributes");
                        string ipaddresses = node.ChildNodes.Item(2).InnerText;
                        h.IPlist = ipaddresses.Split(',');
                        h.hostname = hostname;
                        Trace.WriteLine(DateTime.Now + "\t Printing ipaddresses " + ipaddresses);
                        count++;
                        duplicateList._hostList.Add(h);
                        Trace.WriteLine(DateTime.Now + "\t Count of hosts " + duplicateList._hostList.Count.ToString());
                    }
                }

                if (count == 1)
                {
                    refIPaddress = ipaddress;
                    return 0;
                }
                else if (count > 1)
                {
                    Trace.WriteLine(DateTime.Now + "\t CX having multiple entries of this vm " + hostname);
                    refIPaddress = ipaddress;
                    return 1;
                }
                else if (count == 0)
                {
                    Trace.WriteLine(DateTime.Now + "\t Not found VM entry in cx " + hostname);
                    refIPaddress = ipaddress;
                    return 2;
                }
                refIPaddress = ipaddress;
                return 2;
            }
            catch (System.Net.WebException e)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);
                System.FormatException formatException = new FormatException("Inner exception", e);
                Trace.WriteLine(formatException.GetBaseException());
                Trace.WriteLine(formatException.Data);
                Trace.WriteLine(formatException.InnerException);
                Trace.WriteLine(formatException.Message);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + e.Message);
                Trace.WriteLine("ERROR___________________________________________");

                return 1;
            }

            catch (System.Xml.XmlException e)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);
                System.FormatException formatException = new FormatException("Inner exception", e);
                Trace.WriteLine(formatException.GetBaseException());
                Trace.WriteLine(formatException.Data);
                Trace.WriteLine(formatException.InnerException);
                Trace.WriteLine(formatException.Message);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + e.Message);
                Trace.WriteLine("ERROR___________________________________________");


                return 1;


            }


        }

        public int SetHostIPinputhostname(string hostname, string ipaddress, string incxIP)
         {
             //to set proper ip for a host if host has multiple ips/multiple nics

             string originalHostname = hostname;
             string[] words = hostname.Split('.');
             string hostnameOriginal = words[0];
             int count = 0;
             hostname = words[0].ToUpper();
             if (hostname.Length > 15)
             {
                 hostname = hostname.Substring(0, 15);
             }

             Trace.WriteLine("Looking for " + hostnameOriginal + "  " + hostname);
             string _xmlString="";
             
           
             try
             {
                 
                 String postData1 = "&operation=";

                 // xmlData would be send as the value of variable called "xml"
                 // This data would then be wrtitten in to a file callied input.xml
                 // on CX and then given as argument to the cx_cli.php script.
                 int operation = 3;

                 String postData2 = "&userfile='input.xml'&xml=" + _xmlString;


                 String postData = postData1 + operation + postData2;
                 //  Debug.WriteLine(postData);
                 //String postData = "&operation=2";
                 HttpReply reply = ConnectToHttpClient(postData);
                 if (String.IsNullOrEmpty(reply.error))
                 {
                     // process reply
                     if (!String.IsNullOrEmpty(reply.data))
                     {
                         Trace.WriteLine(reply.data);
                     }
                 }
                 else
                 {

                     Trace.WriteLine(reply.error);
                     return 1;
                 }
                 string responseFromServer = reply.data;

                 //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
                 //based on above comment we have added xmlresolver as null
                 XmlDocument xDocument = new XmlDocument();
                 xDocument.XmlResolver = null;
                 //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
                 XmlReaderSettings settings = new XmlReaderSettings();
                 settings.ProhibitDtd = true;
                 settings.XmlResolver = null;
                 using (XmlReader reader1 = XmlReader.Create(Latestpath + tempXml, settings))
                 {
                     xDocument.Load(reader1);
                    // reader1.Close();
                 }
                // xDocument.Load(Latestpath + tempXml);



                 //Reading volume replication details from cx

                 XmlNodeList xPNodeList = xDocument.SelectNodes(".//host");

                 foreach (XmlNode node in xPNodeList)
                 {
                    
                     Host h = new Host();
                    
                     if (node.ChildNodes.Item(0).InnerText.ToUpper() == hostnameOriginal.ToUpper() || node.ChildNodes.Item(0).InnerText.ToUpper() == originalHostname.ToUpper())
                     {
                         if (node.Attributes["id"] != null && node.Attributes["id"].Value != null && node.Attributes["id"].Value.Length != 0)
                         {
                             h.inmage_hostid = node.Attributes["id"].Value;
                             Trace.WriteLine(DateTime.Now + "\t Printing host id " + h.inmage_hostid);
                         }
                         Trace.WriteLine(DateTime.Now + "\t Found hostname and reading required values for the host" + hostname);
                         Trace.WriteLine("*******Found Source IP address***********");
                         ipaddress = node.ChildNodes.Item(1).InnerText;
                         Trace.WriteLine("Hostname:" + hostname);
                         Trace.WriteLine("IpAddress:" + ipaddress);
                         h.ip = ipaddress;
                        
                         count++;
                         
                     }
                



                 }

                 if (count == 1)
                 {
                     refIPaddress = ipaddress;
                     return 0;
                 }
                 else if (count >1)
                 {
                     Trace.WriteLine(DateTime.Now + "\t CX having multiple entries of this vm " + hostname);
                     refIPaddress = ipaddress;
                     return 1;
                 }
                 else if (count == 0)
                 {
                     Trace.WriteLine(DateTime.Now + "\t Not found VM entry in cx " + hostname);
                     refIPaddress = ipaddress;
                     return 2;
                 }
                 refIPaddress = ipaddress;
                 return 2;
             }
             catch (System.Net.WebException e)
             {
                 System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);
                 System.FormatException formatException = new FormatException("Inner exception", e);
                 Trace.WriteLine(formatException.GetBaseException());
                 Trace.WriteLine(formatException.Data);
                 Trace.WriteLine(formatException.InnerException);
                 Trace.WriteLine(formatException.Message);
                 Trace.WriteLine("ERROR*******************************************");
                 Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                 Trace.WriteLine("Exception  =  " + e.Message);
                 Trace.WriteLine("ERROR___________________________________________");
                
                 return 1;
             }

             catch (System.Xml.XmlException e)
             {

                 System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);
                 System.FormatException formatException = new FormatException("Inner exception", e);
                 Trace.WriteLine(formatException.GetBaseException());
                 Trace.WriteLine(formatException.Data);
                 Trace.WriteLine(formatException.InnerException);
                 Trace.WriteLine(formatException.Message);
                 Trace.WriteLine("ERROR*******************************************");
                 Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                 Trace.WriteLine("Exception  =  " + e.Message);
                 Trace.WriteLine("ERROR___________________________________________");
                 

                 return 1;


             }


         }

        public bool GetPlanNames(ArrayList planList, string incxIP)
         {
             planList.Clear();
             string _xmlString = "";
            

             try
             {
                 
                 String postData1 = "&operation=";

                 // xmlData would be send as the value of variable called "xml"
                 // This data would then be wrtitten in to a file callied input.xml
                 // on CX and then given as argument to the cx_cli.php script.
                 int operation = 44;

                 String postData2 = "&userfile='input.xml'&xml=" + _xmlString;


                 String postData = postData1 + operation + postData2;
                 //  Debug.WriteLine(postData);
                 HttpReply reply = ConnectToHttpClient(postData);
                 if (String.IsNullOrEmpty(reply.error))
                 {
                     // process reply
                     if (!String.IsNullOrEmpty(reply.data))
                     {
                         Trace.WriteLine(reply.data);
                     }
                 }
                 else
                 {

                     Trace.WriteLine(reply.error);
                     return false;
                 }
                 string responseFromServer = reply.data;

                 //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
                 //based on above comment we have added xmlresolver as null
                 XmlDocument xDocument = new XmlDocument();
                 xDocument.XmlResolver = null;
                 //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
                 XmlReaderSettings settings = new XmlReaderSettings();
                 settings.ProhibitDtd = true;
                 settings.XmlResolver = null;
                 using (XmlReader reader1 = XmlReader.Create(Latestpath + tempXml, settings))
                 {
                     xDocument.Load(reader1);
                     //xDocument.Load(Latestpath + tempXml);
                     //reader1.Close();
                 }

                 //Reading volume replication details from cx

                 XmlNodeList xPNodeList = xDocument.SelectNodes(".//plan");

                 foreach (XmlNode node in xPNodeList)
                 {


                     if (node.ChildNodes.Item(0).InnerText.ToString()!= null)
                     {
                         planList.Add(node.ChildNodes.Item(0).InnerText.ToString());
                         
                        

                     }




                 }
                 refplanList = planList;
                 return true;

             }
             catch (System.Net.WebException e)
             {
                 System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);
                 System.FormatException formatException = new FormatException("Inner exception", e);
                 Trace.WriteLine(formatException.GetBaseException());
                 Trace.WriteLine(formatException.Data);
                 Trace.WriteLine(formatException.InnerException);
                 Trace.WriteLine(formatException.Message);
                 Trace.WriteLine("ERROR*******************************************");
                 Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                 Trace.WriteLine("Exception  =  " + e.Message);
                 Trace.WriteLine("ERROR___________________________________________");

                 return false;
             }

             catch (System.Xml.XmlException e)
             {

                 System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);
                 System.FormatException formatException = new FormatException("Inner exception", e);
                 Trace.WriteLine(formatException.GetBaseException());
                 Trace.WriteLine(formatException.Data);
                 Trace.WriteLine(formatException.InnerException);
                 Trace.WriteLine(formatException.Message);
                 Trace.WriteLine("ERROR*******************************************");
                 Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                 Trace.WriteLine("Exception  =  " + e.Message);
                 Trace.WriteLine("ERROR___________________________________________");


                 return false;


             }


         }

        public static ArrayList GetPlanlist
         {
             get
             {
                 return refplanList;
             }
            set
             {
                 value = refplanList;
             }
         }


        public static bool IsThisPlanUsesSameMT(string inPlanName, string inMTHostName, string incxIP)
         {

             string[] words = inMTHostName.Split('.');

             
             inMTHostName = words[0].ToUpper();
             if (inMTHostName.Length > 15)
             {
                 inMTHostName = inMTHostName.Substring(0, 15);
             }
            
             try
            {


                bool isMTExist = false;

                string inPutString = "<plan> " + Environment.NewLine +
                                     "  <header> " + Environment.NewLine +
                                     "                <parameters> " + Environment.NewLine +
                                     "                      <param name='name'>" + inPlanName + "</param>" + Environment.NewLine +
                                     "                      <param name='type'>" + "DR Protection" + "</param>" + Environment.NewLine +
                                     "                      <param name='version'>1.1</param>" + Environment.NewLine +
                                     "                </parameters> " + Environment.NewLine +
                                     "  </header> " + Environment.NewLine +
                                     " </plan>";



                String url = "";
                

                try
                {
                    
                    String postData1 = "&operation=";

                    // xmlData would be send as the value of variable called "xml"
                    // This data would then be wrtitten in to a file callied input.xml
                    // on CX and then given as argument to the cx_cli.php script.
                    int operation = 43;

                    String postData2 = "&userfile='input.xml'&xml=" + inPutString;


                    String postData = postData1 + operation + postData2;
                    //  Debug.WriteLine(postData);
                    //String postData = "&operation=2";
                    WinPreReqs win = new WinPreReqs("", "", "", "");
                    HttpReply reply = win.ConnectToHttpClient(postData);
                    if (String.IsNullOrEmpty(reply.error))
                    {
                        // process reply
                        if (!String.IsNullOrEmpty(reply.data))
                        {
                            Trace.WriteLine(reply.data);
                        }
                    }
                    else
                    {

                        Trace.WriteLine(reply.error);
                        return false;
                    }
                    string responseFromServer = reply.data;
                    Trace.WriteLine(DateTime.Now + "\t Response " + responseFromServer);
                    if (responseFromServer.Contains("<error_code>FAIL</error_code>") || responseFromServer.Contains("<error_code >FAIL</error_code>"))
                    {
                        Trace.WriteLine(DateTime.Now + "\t Specified Plan name doesn't exist on CX");
                        return true;
                    }

                    if (!responseFromServer.Contains(inMTHostName))
                    {

                        Trace.WriteLine(DateTime.Now + "\t" + inMTHostName + " is not part of given Plan");
                        return false;

                    }


                    // Debug.WriteLine(responseFromServer);
                    //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
                    //based on above comment we have added xmlresolver as null
                    XmlDocument xDocument = new XmlDocument();
                    xDocument.XmlResolver = null;
                    Trace.WriteLine(DateTime.Now + " \t Printing the responce from cxcli " + responseFromServer.ToString());
                    string latest = WinTools.Vxagentpath() + "\\vContinuum\\Latest";
                    //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
                    XmlReaderSettings settings = new XmlReaderSettings();
                    settings.ProhibitDtd = true;
                    settings.XmlResolver = null;
                    using (XmlReader reader1 = XmlReader.Create(latest + tempXml, settings))
                    {
                        xDocument.Load(reader1);
                        //xDocument.Load(latest + tempXml);
                       // reader1.Close();
                    }
                    //Reading volume replication details from cx

                    XmlNodeList xPNodeList = xDocument.SelectNodes(".//volume_protection");


                    foreach (XmlNode xNode in xPNodeList)
                    {


                        //Comparing source host name with cxcli reponse and target hostname.If both matches, then only proceed for preparing xml for deletion.

                        if ( xNode.ChildNodes.Item(2).InnerText.ToString().Split('.')[0].ToUpper() == inMTHostName.ToString().ToUpper())
                        {

                            isMTExist = true;
                            break;
                        }

                    }


                    if (isMTExist == false)
                    {
                        xPNodeList = xDocument.SelectNodes(".//file_protection");


                        foreach (XmlNode xNode in xPNodeList)
                        {

                            if (xNode.ChildNodes.Item(2).InnerText.ToString().Split('.')[0].ToUpper() == inMTHostName.ToString().ToUpper())
                            {

                                isMTExist = true;

                                break;
                            }

                        }
                    }
                    else

                    {
                        return true;
                    
                    
                    }

                    if (isMTExist == true)
                    {
                        return true;
                    }
                    else
                    {
                        return false;
                    
                    }

                }
                catch (System.Net.WebException e)
                {

                    System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);
                    System.FormatException formatException = new FormatException("Inner exception", e);
                    Trace.WriteLine(formatException.GetBaseException());
                    Trace.WriteLine(formatException.Data);
                    Trace.WriteLine(formatException.InnerException);
                    Trace.WriteLine(formatException.Message);
                    Trace.WriteLine("ERROR*******************************************");
                    Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                    Trace.WriteLine("Exception  =  " + e.Message);
                    Trace.WriteLine("ERROR___________________________________________");

                    Trace.WriteLine("\t This Server is not pointed to the CX.Please verify whether agents are installed or not.And are pointing to " + incxIP);
                    return false;
                }
                catch (System.Xml.XmlException e)
                {
                    System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);
                    System.FormatException formatException = new FormatException("Inner exception", e);
                    Trace.WriteLine(formatException.GetBaseException());
                    Trace.WriteLine(formatException.Data);
                    Trace.WriteLine(formatException.InnerException);
                    Trace.WriteLine(formatException.Message);
                    Trace.WriteLine("ERROR*******************************************");
                    Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                    Trace.WriteLine("Exception  =  " + e.Message);
                    Trace.WriteLine("ERROR___________________________________________");
                    Trace.WriteLine("Cxcli reponse is not formatted correctly");
                    return false;


                }

               
            }
             catch (Exception ex)
             {

                 System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                 System.FormatException formatException = new FormatException("Inner exception", ex);
                 Trace.WriteLine(formatException.GetBaseException());
                 Trace.WriteLine(formatException.Data);
                 Trace.WriteLine(formatException.InnerException);
                 Trace.WriteLine(formatException.Message);
                 Trace.WriteLine("ERROR*******************************************");
                 Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                 Trace.WriteLine("Exception  =  " + ex.Message);
                 Trace.WriteLine("ERROR___________________________________________");
                 return false;

             }


             return true;


         
         }


        public static bool SetHostNameInputIP(string ipvalue, string hostname, string incxIP)
         {
             

             Trace.WriteLine("Looking for " +  "  " + ipvalue);
             string _xmlString = "";
            

             try
             {
                 
                 String postData1 = "&operation=";
                 // xmlData would be send as the value of variable called "xml"
                 // This data would then be wrtitten in to a file callied input.xml
                 // on CX and then given as argument to the cx_cli.php script.
                 int operation = 3;
                 String postData2 = "&userfile='input.xml'&xml=" + _xmlString;
                 String postData = postData1 + operation + postData2;
                 WinPreReqs win = new WinPreReqs("", "", "", "");
                 HttpReply reply = win.ConnectToHttpClient(postData);
                 if (String.IsNullOrEmpty(reply.error))
                 {
                     // process reply
                     if (!String.IsNullOrEmpty(reply.data))
                     {
                         Trace.WriteLine(reply.data);
                     }
                 }
                 else
                 {

                     Trace.WriteLine(reply.error);
                     return false;
                 }
                 string responseFromServer = reply.data;
                 //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
                 //based on above comment we have added xmlresolver as null
                 XmlDocument xDocument = new XmlDocument();
                 xDocument.XmlResolver = null;
                 string latest = WinTools.Vxagentpath() + "\\vContinuum\\Latest";

                 //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
                 XmlReaderSettings settings = new XmlReaderSettings();
                 settings.ProhibitDtd = true;
                 settings.XmlResolver = null;
                 using (XmlReader reader1 = XmlReader.Create(latest + tempXml, settings))
                 {
                     xDocument.Load(reader1);
                     //reader1.Close();
                 }
                 //xDocument.Load(latest + tempXml);
                 //Reading volume replication details from cx
                 XmlNodeList xPNodeList = xDocument.SelectNodes(".//host");
                 foreach (XmlNode node in xPNodeList)
                 {
                     string[] ips = node.ChildNodes.Item(2).InnerText.Split(',');
                     foreach (string ip in ips)
                     {
                         if (ip == ip)
                         {
                             hostname = node.ChildNodes.Item(0).InnerText;
                             Trace.WriteLine("Hostname:" + hostname);
                             Trace.WriteLine("IpAddress:" + ip);
                             refHostname = hostname;
                         
                             return true;
                         }
                     }   
                 }
                 return false;
             }
             catch (System.Net.WebException e)
             {
                 System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);
                 System.FormatException formatException = new FormatException("Inner exception", e);
                 Trace.WriteLine(formatException.GetBaseException());
                 Trace.WriteLine(formatException.Data);
                 Trace.WriteLine(formatException.InnerException);
                 Trace.WriteLine(formatException.Message);
                 Trace.WriteLine("ERROR*******************************************");
                 Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                 Trace.WriteLine("Exception  =  " + e.Message);
                 Trace.WriteLine("ERROR___________________________________________");
                 return false;
             }

             catch (System.Xml.XmlException e)
             {
                 System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);
                 System.FormatException formatException = new FormatException("Inner exception", e);
                 Trace.WriteLine(formatException.GetBaseException());
                 Trace.WriteLine(formatException.Data);
                 Trace.WriteLine(formatException.InnerException);
                 Trace.WriteLine(formatException.Message);
                 Trace.WriteLine("ERROR*******************************************");
                 Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                 Trace.WriteLine("Exception  =  " + e.Message);
                 Trace.WriteLine("ERROR___________________________________________");
                 return false;
             }
         }

        public bool SetvxAgentInstallationPath(string hostname, ref string vxAgentPath, string incxIP)
         {
             string[] words = hostname.Split('.');
             string hostnameOriginal = words[0];
             hostname = words[0].ToUpper();
             if (hostname.Length > 15)
             {
                 hostname = hostname.Substring(0, 15);
             }

             string _xmlString = "";
            
             try
             {
                 
                 String postData1 = "&operation=";

                 // xmlData would be send as the value of variable called "xml"
                 // This data would then be wrtitten in to a file callied input.xml
                 // on CX and then given as argument to the cx_cli.php script.
                 int operation = 3;

                 String postData2 = "&userfile='input.xml'&xml=" + _xmlString;


                 String postData = postData1 + operation + postData2;
                 HttpReply reply = ConnectToHttpClient(postData);
                 if (String.IsNullOrEmpty(reply.error))
                 {
                     // process reply
                     if (!String.IsNullOrEmpty(reply.data))
                     {
                         Trace.WriteLine(reply.data);
                     }
                 }
                 else
                 {

                     Trace.WriteLine(reply.error);
                     return false;
                 }
                 string responseFromServer = reply.data;
                 //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
                 //based on above comment we have added xmlresolver as null
                 XmlDocument xDocument = new XmlDocument();
                 xDocument.XmlResolver = null;
                 //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
                 XmlReaderSettings settings = new XmlReaderSettings();
                 settings.ProhibitDtd = true;
                 settings.XmlResolver = null;
                 using (XmlReader reader1 = XmlReader.Create(Latestpath + tempXml, settings))
                 {
                     xDocument.Load(reader1);
                     //xDocument.Load(Latestpath + tempXml);

                   //  reader1.Close();
                 }
                 //Reading volume replication details from cx

                 XmlNodeList xPNodeList = xDocument.SelectNodes(".//host");

                 foreach (XmlNode node in xPNodeList)
                 {

                     
                     if (node.ChildNodes.Item(0).InnerText == hostname.ToUpper() || node.ChildNodes.Item(0).InnerText == hostnameOriginal)
                     {

                         Debug.WriteLine("*******Found Source IP address***********");
                         vxAgentPath = node.ChildNodes.Item(4).InnerText;
                         Debug.WriteLine("Hostname:" + hostname);
                         Debug.WriteLine("VxAgentInstallationPath:" + vxAgentPath);
                         return true;

                     }




                 }

                 return false;

             }
             catch (System.Net.WebException e)
             {
                 System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);
                 System.FormatException formatException = new FormatException("Inner exception", e);
                 Trace.WriteLine(formatException.GetBaseException());
                 Trace.WriteLine(formatException.Data);
                 Trace.WriteLine(formatException.InnerException);
                 Trace.WriteLine(formatException.Message);
                 Trace.WriteLine("ERROR*******************************************");
                 Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                 Trace.WriteLine("Exception  =  " + e.Message);
                 Trace.WriteLine("ERROR___________________________________________");
                 
                 
                 return false;
             }

             catch (System.Xml.XmlException e)
             {

                 System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);
                 System.FormatException formatException = new FormatException("Inner exception", e);
                 Trace.WriteLine(formatException.GetBaseException());
                 Trace.WriteLine(formatException.Data);
                 Trace.WriteLine(formatException.InnerException);
                 Trace.WriteLine(formatException.Message);
                 Trace.WriteLine("ERROR*******************************************");
                 Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                 Trace.WriteLine("Exception  =  " + e.Message);
                 Trace.WriteLine("ERROR___________________________________________");
                 
                 return false;


             }


         }


        public int PreReqCheckForRecovery(string inMasterHostname, Host inHost, string incxIPwithportNumber)
         {

             string masterTargetIP = null, masterTargetVxPath=null;

             try
             {
                 //Checking whether cx is up or not
                /*if (IpReachable(in_cx_ip) == false)
                 {
                     Trace.WriteLine(DateTime.Now +"\tUnable to reach " + in_cx_ip);
                     Trace.WriteLine(DateTime.Now +"Please check whether Cx is available or not");
                     return 1;

                 }*/
                 
                 //Setting MasterTargets IP address
                 if (SetHostIPinputhostname(inMasterHostname,  masterTargetIP, incxIPwithportNumber) != 0 || masterTargetIP == null)
                 {
                     masterTargetIP =  WinPreReqs.GetIPaddress;
                     Trace.WriteLine(DateTime.Now + "Unable to Fetch Master TargetIP from cx ");
                     Trace.WriteLine(DateTime.Now + "Can't proceeed without Master TargetIP");
                     return 2;

                 }
                 if (SetvxAgentInstallationPath(inMasterHostname, ref masterTargetVxPath, incxIPwithportNumber) == false || masterTargetVxPath == null)
                 {

                     Trace.WriteLine(DateTime.Now + "Unable to Fetch Master Target's VX pathfrom cx ");
                     Trace.WriteLine(DateTime.Now + "Can't proceeed without Master Target VX path");
                     return 3;

                 }
                 masterTargetIP = WinPreReqs.GetIPaddress;
                 string hostnames = null;
                 // For vcon 4.0 release we are going to use uuid. which we will get from cx.
                 
                     hostnames = inHost.hostname;
                     //Trace.WriteLine(DateTime.Now + "\t In this case inmage_host uuid is  null means vcon 3.0 pre release and before " + inHost.hostName);
                



                 //Creating batch file to run cscript
                 if (File.Exists(Vcontinuumpath + "\\prereqcheck.bat"))
                 {
                     File.Delete(Vcontinuumpath +"\\prereqcheck.bat");

                 }

                 if (File.Exists(Vcontinuumpath + "\\cdpoutput.txt"))
                 {
                     File.Delete("cdpoutput.txt");
                 }


                 if (inHost.userDefinedTime != null)
                 {
                     DateTime convertTime = DateTime.Parse(inHost.userDefinedTime);
                     DateTime addOnesecond = convertTime.AddSeconds(1);
                     DateTime gmt = DateTime.Parse(addOnesecond.ToString());
                     inHost.userDefinedTime = gmt.ToString("yyyy/MM/dd HH:mm:ss");
                 
                 }

                 FileInfo f1 = new FileInfo(Vcontinuumpath +"\\prereqcheck.bat");
                 StreamWriter sw = f1.CreateText();
                 sw.WriteLine("echo off");

                 //  sw.WriteLine("\"" + masterTargetVxPath + "\"" + "\\" + "drvutil.exe --gdv|find \"Error\"");
                 sw.WriteLine("cscript " + "c:\\windows\\temp\\prereqcheck.vbs " + "  " + inHost.recoveryOperation + " " + hostnames + "  " + "\"" + masterTargetVxPath + "\"" + "  " + "\"" + inHost.userDefinedTime + "\"" + "  " + "\"" + inHost.userDefinedTime + "\"");
                 sw.WriteLine("exit %errorlevel%");
                 sw.Close();

                 if (!File.Exists(Vcontinuumpath  + "\\prereqcheck.vbs"))
                 {

                     Trace.WriteLine(DateTime.Now + "\t prereqcheck.vbs doesn't exist. Can't run pre-req check if prereqcheck.vbs doesn't exist");
                     refHost = inHost;
                     return 4;

                 }
                 if (inMasterHostname.Split('.')[0].ToUpper() == Environment.MachineName.ToUpper())
                 {
                     masterTargetIP = ".";
                     Domain = null;
                     Username = null;
                     Password = null;
                 }
             
                 RemoteWin remoteWin1 = new RemoteWin(masterTargetIP, Domain, Username, Password,inMasterHostname);

                 ArrayList filenames = new ArrayList();
                 filenames.Add("prereqcheck.vbs");
                 filenames.Add("prereqcheck.bat");
                 string output = "";
                 int value;


                 Trace.WriteLine("Running copy and execute command for " + inHost.hostname + "Display name:" + inHost.displayname + " ip:" + inHost.ip);
                 if (inMasterHostname.Split('.')[0].ToUpper() == Environment.MachineName.ToUpper())
                 {
                     value = remoteWin1.CopyAndExecuteInLocalPath("prereqcheck.bat", Vcontinuumpath, filenames, 300);
                 }
                 else
                 {
                     value = remoteWin1.CopyAndExecute("prereqcheck.bat", Vcontinuumpath, filenames,  300);
                 }
                 if (value == 1)
                 {
                     Trace.WriteLine(DateTime.Now + "\t "+ inHost.hostname +".pinfo file doesn't exit in VX_AGENT_INSTALLATION_PATH\\failover\\data folder" );
                     refHost = inHost;
                     return 5;
                 }
                 if (inMasterHostname.Split('.')[0].ToUpper() == Environment.MachineName.ToUpper())
                 {
                     if (remoteWin1.CopyFileFromLocalPathSystem("c:\\windows\\temp", "cdpoutput.txt", Vcontinuumpath) == false)
                     {
                         Trace.WriteLine(DateTime.Now + "\t Unable to copy cdpoutput.txt from remote machine to local machine's vContinuum folder");
                         refHost = inHost;
                         return 6;
                     }

                 }
                 else
                 {
                     if (remoteWin1.CopyFileFrom("c:\\windows\\temp", "cdpoutput.txt", Vcontinuumpath) == false)
                     {
                         Trace.WriteLine(DateTime.Now + "\t Unable to copy cdpoutput.txt from remote machine to local machine's vContinuum folder");
                         refHost = inHost;
                         return 6;
                     }
                 }

                 if (File.Exists(Vcontinuumpath +"\\cdpoutput.txt"))
                 {

                     f1 = new FileInfo(Vcontinuumpath +"\\cdpoutput.txt");
                     FileInfo f2 = new FileInfo(Vcontinuumpath + "\\cdpoutput.txt");
                     StreamReader sr = f1.OpenText();
                     StreamReader sr1 = f2.OpenText();
                     Trace.WriteLine(DateTime.Now + "\t Printing cdpoutput.txt and optin is choosens for pre req is: " + inHost.recoveryOperation.ToString());
                     Trace.WriteLine(DateTime.Now + "\t" + sr1.ReadToEnd().ToString());


                     if (sr.ReadLine().Contains(hostnames))
                     {
                         if (!sr.EndOfStream)
                         {
                             if (sr.ReadLine().Contains("0"))
                             {
                                 if (inHost.recoveryOperation == 1)
                                 {
                                     inHost.commonTagAvaiable = true;

                                 }
                                 else if (inHost.recoveryOperation == 2)
                                 {
                                     inHost.commonTimeAvaiable = true;

                                 }
                                 else if (inHost.recoveryOperation == 3)
                                 {

                                     inHost.commonUserDefinedTimeAvailable = true;
                                     if (!sr.EndOfStream)
                                     {
                                         sr.ReadLine();


                                     }
                                     if (!sr.EndOfStream)
                                     {
                                         string timeValue = sr.ReadLine();
                                         string[] firstArray = timeValue.Split('=');
                                         string nextString = firstArray[1];
                                         string[] finalStrings = nextString.Split(':');
                                         string actualString = finalStrings[0] + ":" + finalStrings[1] + ":" + finalStrings[2];
                                         inHost.userDefinedTime = actualString;

                                     }

                                     if (!sr.EndOfStream)
                                     {
                                         string[] tagValues = sr.ReadLine().Split('=');

                                         inHost.tag = tagValues[1].Trim();


                                     }

                                 }
                                 else if (inHost.recoveryOperation == 4)
                                 {
                                     inHost.commonSpecificTimeAvaiable = true;
                                 
                                 }
                             }

                         }
                     }
                     else
                     {
                         //If cdpoutput.txt does not contain hostname it cam eto here to return false
                         sr.Close();
                         sr1.Close();
                         refHost = inHost;
                         return 8;
                     }

                     sr.Close();
                     sr1.Close();

                 }
                 else
                 {
                     refHost = inHost;
                     return 7;

                 }
             }
             catch (Exception e)
             {
                 System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);
                 System.FormatException formatException = new FormatException("Inner exception", e);
                 Trace.WriteLine(formatException.GetBaseException());
                 Trace.WriteLine(formatException.Data);
                 Trace.WriteLine(formatException.InnerException);
                 Trace.WriteLine(formatException.Message);
                 Trace.WriteLine("ERROR*******************************************");
                 Trace.WriteLine("Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                 Trace.WriteLine("Exception  =  " + e.Message);
                 Trace.WriteLine("ERROR___________________________________________");
                 return 6;
             
             }
             refHost = inHost;
            
             return 0;
         
         }

        public int AgentCheckStatus(Host inHost, string outPut)
         {

             

             try
             {

                 string hostname = inHost.hostname;
                 string machineIp = inHost.ip;
                 if (inHost.hostname.Split('.')[0].ToUpper() == Environment.MachineName.ToUpper())
                 {
                     machineIp = ".";
                     Domain = null;
                     Username = null;
                     Password = null;
                 }
                 

                 //Creating batch file to run cscript
                 if (File.Exists(Vcontinuumpath + "\\Agentcheck.bat"))
                 {
                     File.Delete(Vcontinuumpath + "\\Agentcheck.bat");

                 }

                 if (File.Exists(Vcontinuumpath + "\\agentoutput.txt"))
                 {
                     File.Delete("agentoutput.txt");
                 }




                 FileInfo f1 = new FileInfo(Vcontinuumpath + "\\Agentcheck.bat");
                 StreamWriter sw = f1.CreateText();
                 sw.WriteLine("echo off");

                 //  sw.WriteLine("\"" + masterTargetVxPath + "\"" + "\\" + "drvutil.exe --gdv|find \"Error\"");
                 
                 sw.WriteLine("del/Q " + "c:\\windows\\temp\\UnifiedAgent.exe " +"c:\\UnifiedAgent.exe" );
                 sw.WriteLine("cscript " + "c:\\windows\\temp\\Agentcheck.vbs ");
                 sw.WriteLine("exit %errorlevel%");
                 sw.Close();

                 if (!File.Exists(Vcontinuumpath + "\\Agentcheck.vbs"))
                 {

                     Trace.WriteLine(DateTime.Now + "\t Agentcheck.vbs doesn't exist. Can't run Agentcheck  if Agentcheck.vbs doesn't exist");
                     refOutput = outPut;
                     return 4;

                 }
                 

                 RemoteWin remoteWin1 = new RemoteWin(machineIp, Domain, Username, Password, inHost.hostname);

                 ArrayList filenames = new ArrayList();
                 filenames.Add("Agentcheck.vbs");
                 filenames.Add("Agentcheck.bat");
                 string output = "";
                 int value;


                 Trace.WriteLine("Running copy and execute command for " + inHost.hostname + "Display name:" + inHost.displayname + " ip:" + inHost.ip);
                 if (inHost.hostname.Split('.')[0].ToUpper() == Environment.MachineName.ToUpper())
                 {
                     value = remoteWin1.CopyAndExecuteInLocalPath("Agentcheck.bat", Vcontinuumpath, filenames,  300);
                 }
                 else
                 {
                     value = remoteWin1.CopyAndExecute("Agentcheck.bat", Vcontinuumpath, filenames,  300);
                 }
                 if (inHost.hostname.Split('.')[0].ToUpper() == Environment.MachineName.ToUpper())
                 {
                     if (remoteWin1.CopyFileFromLocalPathSystem("c:\\windows\\temp", "agentoutput.txt", Vcontinuumpath) == false)
                     {
                         Trace.WriteLine(DateTime.Now + "\t Unable to copy agentoutput.txt from remote machine to local machine's vContinuum folder");
                         refOutput = outPut;
                         return 6;
                     }

                 }
                 else
                 {
                     if (remoteWin1.CopyFileFrom("c:\\windows\\temp", "agentoutput.txt", Vcontinuumpath) == false)
                     {
                         Trace.WriteLine(DateTime.Now + "\t Unable to copy agentoutput.txt from remote machine to local machine's vContinuum folder");
                         refOutput = outPut;
                         return 6;
                     }
                 }

                 if (File.Exists(Vcontinuumpath + "\\agentoutput.txt"))
                 {
                     StreamReader sr1 = new StreamReader(Vcontinuumpath + "\\agentoutput.txt");
                     TextReader tr = sr1;
                     string outputs = null;
                     outputs = tr.ReadLine();
                     Trace.WriteLine(DateTime.Now + "\t" + sr1.ReadLine());
                     outPut = outputs;                   
                     Trace.WriteLine(DateTime.Now + "\t CXIP " + outputs);
                     sr1.Close();                   

                 }                 
             }
             catch (Exception e)
             {
                 System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);
                 System.FormatException formatException = new FormatException("Inner exception", e);
                 Trace.WriteLine(formatException.GetBaseException());
                 Trace.WriteLine(formatException.Data);
                 Trace.WriteLine(formatException.InnerException);
                 Trace.WriteLine(formatException.Message);
                 Trace.WriteLine("ERROR*******************************************");
                 Trace.WriteLine("Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                 Trace.WriteLine("Exception  =  " + e.Message);
                 Trace.WriteLine("ERROR___________________________________________");
                 return 1;

             }

             refOutput = outPut;
             return 0;

         }


        public static string VconboxOS()
         {
             string osType = "";
           
             ManagementObjectSearcher searcher = new ManagementObjectSearcher("select * from win32_OperatingSystem");
             foreach (ManagementObject queryObj in searcher.Get())
             {
                 osType = Convert.ToString(queryObj["Caption"]);
             }
             Trace.WriteLine("Operating system of vCon box " + osType);
             return osType;
         }




        public bool MasterTargetPreCheck(Host host, string incxIP)
         {
             
             try
             {
                 Trace.WriteLine(DateTime.Now + " ptinting the host name of mt " + host.hostname + " ip " + host.ip);
                 SetHostIPinputhostname(host.hostname,  host.ip, incxIP);
                 host.ip = WinPreReqs.GetIPaddress;
                 string hostname = host.hostname;
                 string[] words =hostname.Split('.');
                 string hostnameOriginal = words[0];
                 hostname = words[0].ToUpper();
                 if (hostname.Length > 15)
                 {
                     hostname = hostname.Substring(0, 15);
                 }

                 string inPutString = "<plan>" + Environment.NewLine + "<host_ip>" + host.ip + "</host_ip>" + Environment.NewLine +
                                    "<host_name>" +  hostname +"</host_name>"+Environment.NewLine+                  "</plan>";
                 
                 String postData1 = "&operation=";

                 // xmlData would be send as the value of variable called "xml"
                 // This data would then be wrtitten in to a file callied input.xml
                 // on CX and then given as argument to the cx_cli.php script.
                 int operation = 51;

                 String postData2 = "&userfile='input.xml'&xml=" + inPutString;


                 String postData = postData1 + operation + postData2;
                 HttpReply reply = ConnectToHttpClient(postData);
                 if (String.IsNullOrEmpty(reply.error))
                 {
                     // process reply
                     if (!String.IsNullOrEmpty(reply.data))
                     {
                         Trace.WriteLine(reply.data);
                     }
                 }
                 else
                 {

                     Trace.WriteLine(reply.error);
                     return false;
                 }
                 string responseFromServer = reply.data;
                 if (responseFromServer.Length != 0)
                 {
                     Trace.WriteLine(responseFromServer.ToString());
                 }
                 else
                 {
                     Trace.WriteLine(DateTime.Now + " \t didn't find the machine in cx");
                     Trace.WriteLine(DateTime.Now + " \t Make sure that mt is pointing to this cx");
                     refHost = host;
                     return false;
                 }
                 //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
                 //based on above comment we have added xmlresolver as null
                 XmlDocument xDocument = new XmlDocument();
                 xDocument.XmlResolver = null;
                 //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
                 XmlReaderSettings settings = new XmlReaderSettings();
                 settings.ProhibitDtd = true;
                 settings.XmlResolver = null;
                 using (XmlReader reader1 = XmlReader.Create(Latestpath + tempXml, settings))
                 {
                     xDocument.Load(reader1);
                     //reader1.Close();
                 }
                 //xDocument.Load(Latestpath + tempXml);
                 XmlNodeList xPNodeList = xDocument.SelectNodes(".//Protection_status");
                 foreach (XmlNode node in xPNodeList)
                 {
                     host.machineInUse = bool.Parse(node.ChildNodes.Item(2).InnerText.ToString());
                     Trace.WriteLine("Is machine is using :" + host.machineInUse.ToString());
                     Trace.WriteLine("IpAddress:" + host.ip);
                     refHost = host;
                     return true;
                 }
                 refHost = host;
                 return false;
             }
             catch (Exception ex)
             {
                 Trace.WriteLine(DateTime.Now + "\t Couldn't find the mt machine in cx " + host.hostname);
                 System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                 System.FormatException formatException = new FormatException("Inner exception", ex);
                 Trace.WriteLine(formatException.GetBaseException());
                 Trace.WriteLine(formatException.Data);
                 Trace.WriteLine(formatException.InnerException);
                 Trace.WriteLine(formatException.Message);
                 Trace.WriteLine("ERROR*******************************************");
                 Trace.WriteLine("Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                 Trace.WriteLine("Exception  =  " + ex.Message);
                 Trace.WriteLine("ERROR___________________________________________");
             }
             refHost = host;
             return true;
         }
        public int WmiBasedRecovery(string inMasterHostname, string incxIPwithportNumber, string directoryonmt)
         {

             string masterTargetIP = null, masterTargetVxPath = null;

             try
             {
                 //Checking whether cx is up or not
                 /*if (IpReachable(in_cx_ip) == false)
                 {
                     Trace.WriteLine(DateTime.Now + "\tUnable to reach " + in_cx_ip);
                     Trace.WriteLine(DateTime.Now + "Please check whether Cx is available or not");
                     return 1;

                 }*/

                 //Setting MasterTargets IP address
                 if (SetHostIPinputhostname(inMasterHostname,  masterTargetIP, incxIPwithportNumber) != 0)
                 {
                     masterTargetIP = WinPreReqs.GetIPaddress; ;
                     Trace.WriteLine(DateTime.Now + "Unable to Fetch Master TargetIP from cx " + masterTargetIP);
                     Trace.WriteLine(DateTime.Now + "Can't proceeed without Master TargetIP" + masterTargetIP);
                     return 2;

                 }
                 else
                 {
                     masterTargetIP = WinPreReqs.GetIPaddress;
                 }
                 if (SetvxAgentInstallationPath(inMasterHostname, ref masterTargetVxPath, incxIPwithportNumber) == false || masterTargetVxPath == null)
                 {

                     Trace.WriteLine(DateTime.Now + "Unable to Fetch Master Target's VX pathfrom cx ");
                     Trace.WriteLine(DateTime.Now + "Can't proceeed without Master Target VX path");
                     return 3;

                 }


                 masterTargetIP = WinPreReqs.GetIPaddress; ;
                 Mtvxpath = masterTargetVxPath;

                 //Creating batch file to run cscript
                 if (File.Exists(Latestpath + "\\Recovery.bat"))
                 {
                     File.Delete(Latestpath + "\\Recovery.bat");

                 }

                 if (File.Exists(Vcontinuumpath + "\\InMage_Recovered_Vms.rollback"))
                 {
                     File.Delete(Vcontinuumpath + "\\InMage_Recovered_Vms.rollback");
                 }


                 FileInfo f1 = new FileInfo(Latestpath + "\\Recovery.bat");
                 StreamWriter sw = f1.CreateText();
                 sw.WriteLine("echo off");

                 //  sw.WriteLine("\"" + masterTargetVxPath + "\"" + "\\" + "drvutil.exe --gdv|find \"Error\"");
                 sw.WriteLine("DEL "+ "\"" + masterTargetVxPath + "\\Failover\\Data\\Recovery.xml" + "\"" + " /Q 2>>c:\\windows\\messageonly");
                 //sw.WriteLine("MKDIR " + masterTargetVxPath + "\\Failover\\Data\\" + directory);
                 sw.WriteLine("cd " + "\"" + masterTargetVxPath + "\\Failover\\Data" + "\"");
                 sw.WriteLine("mkdir " + directoryonmt);
                 sw.WriteLine("copy/y" + " c:\\windows\\temp\\Recovery.xml " + "\"" + masterTargetVxPath + "\\Failover\\Data\\" + directoryonmt+ "\"");
                 
                 sw.WriteLine("\"" + masterTargetVxPath + "\"" + "\\Esxutil.exe -rollback -d " + "\"" + masterTargetVxPath + "\\Failover\\Data\\" + directoryonmt+ "\""  + " -cxpath "   + "\"" + "/home/svsystems/vcon/" + directoryonmt + "\"");
                 sw.WriteLine("exit %errorlevel%");
                 sw.Close();

                 if (inMasterHostname.Split('.')[0].ToUpper() == Environment.MachineName.ToUpper())
                 {
                     masterTargetIP = ".";
                     Domain = null;
                     Username = null;
                     Password = null;
                 }

                 RemoteWin remoteWin1 = new RemoteWin(masterTargetIP, Domain, Username, Password,inMasterHostname);

                 ArrayList filenames = new ArrayList();
                 filenames.Add("Recovery.xml");
                 filenames.Add("Recovery.bat");
                 string output = "";
                 int value;


                 Trace.WriteLine("Running copy and execute command on master target for recovery");
                 if (inMasterHostname.Split('.')[0].ToUpper() == Environment.MachineName.ToUpper())
                 {
                     value = remoteWin1.CopyAndExecuteInLocalPath("Recovery.bat", Latestpath, filenames, 28800);
                 }
                 else
                 {
                     value = remoteWin1.CopyAndExecute("Recovery.bat", Latestpath, filenames,  28800);
                 }


                 if (value != 0)
                 {
                     Trace.WriteLine(DateTime.Now + "\t Failed to rollback pairs using WMI");
                     return 1;
                 }

                 Thread.Sleep(5000);
                 if (inMasterHostname.Split('.')[0].ToUpper() == Environment.MachineName.ToUpper())
                 {
                     if (remoteWin1.CopyFileFromLocalPathSystem("c:\\windows\\temp", "InMage_Recovered_Vms.rollback", Vcontinuumpath) == false)
                     {
                         Trace.WriteLine(DateTime.Now + "\t Unable to copy InMage_Recovered_Vms.rollback from remote machine to local machine's vContinuum folder");
                         return 6;
                     }

                 }
                 else
                 {
                     if (remoteWin1.CopyFileFrom("c:\\windows\\temp", "InMage_Recovered_Vms.rollback",Vcontinuumpath) == false)
                     {
                         Trace.WriteLine(DateTime.Now + "\t Unable to copy InMage_Recovered_Vms.rollback from remote machine to local machine's vContinuum folder");
                         return 6;
                     }
                 }


             }
             catch (Exception e)
             {
                 System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);
                 System.FormatException formatException = new FormatException("Inner exception", e);
                 Trace.WriteLine(formatException.GetBaseException());
                 Trace.WriteLine(formatException.Data);
                 Trace.WriteLine(formatException.InnerException);
                 Trace.WriteLine(formatException.Message);
                 Trace.WriteLine("ERROR*******************************************");
                 Trace.WriteLine("Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                 Trace.WriteLine("Exception  =  " + e.Message);
                 Trace.WriteLine("ERROR___________________________________________");
                 return 6;

             }


             return 0;

         }

        public int WmiBaseddrdrill(string inMasterHostname, string incxIPwithportNumber)
         {

             string masterTargetIP = null, masterTargetVxPath = null;

             try
             {
                 //Checking whether cx is up or not
                 /*if (IpReachable(in_cx_ip) == false)
                 {
                     Trace.WriteLine(DateTime.Now + "\tUnable to reach " + in_cx_ip);
                     Trace.WriteLine(DateTime.Now + "Please check whether Cx is available or not");
                     return 1;

                 }*/

                 //Setting MasterTargets IP address
                 if (SetHostIPinputhostname(inMasterHostname,  masterTargetIP, incxIPwithportNumber) != 0 || masterTargetIP == null)
                 {
                     masterTargetIP = WinPreReqs.GetIPaddress;

                     Trace.WriteLine(DateTime.Now + "Unable to Fetch Master TargetIP from cx ");
                     Trace.WriteLine(DateTime.Now + "Can't proceeed without Master TargetIP");
                     return 2;

                 }
                 if (SetvxAgentInstallationPath(inMasterHostname, ref masterTargetVxPath, incxIPwithportNumber) == false || masterTargetVxPath == null)
                 {

                     Trace.WriteLine(DateTime.Now + "Unable to Fetch Master Target's VX pathfrom cx ");
                     Trace.WriteLine(DateTime.Now + "Can't proceeed without Master Target VX path");
                     return 3;

                 }

                 masterTargetIP = WinPreReqs.GetIPaddress; 

                 Mtvxpath = masterTargetVxPath;

                 //Creating batch file to run cscript
                 if (File.Exists(Latestpath + "\\DrDrill.bat"))
                 {
                     File.Delete(Latestpath + "\\DrDrill.bat");

                 }




                 FileInfo f1 = new FileInfo(Latestpath + "\\DrDrill.bat");
                 StreamWriter sw = f1.CreateText();
                 sw.WriteLine("echo off");

                 //  sw.WriteLine("\"" + masterTargetVxPath + "\"" + "\\" + "drvutil.exe --gdv|find \"Error\"");
                 sw.WriteLine("DEL " + "\"" + masterTargetVxPath + "\\Failover\\Data\\Inmage_scsi_unit_disks.txt" + "\"" + " /Q 2>>c:\\windows\\messageonly");
                 sw.WriteLine("DEL " + "\"" + masterTargetVxPath + "\\Failover\\Data\\Snapshot.xml" + "\"" + " /Q 2>>c:\\windows\\messageonly"); 
                 sw.WriteLine("copy/y" + " c:\\windows\\temp\\Snapshot.xml " + "\"" + masterTargetVxPath + "\\Failover\\Data" + "\"");
                 sw.WriteLine("copy/y" + " c:\\windows\\temp\\Inmage_scsi_unit_disks.txt " + "\"" + masterTargetVxPath + "\\Failover\\Data" + "\"");
                 sw.WriteLine("\"" + masterTargetVxPath + "\"" + "\\Consistency\\VCon_Physical_Snapshot.bat ");
                 sw.WriteLine("exit %errorlevel%");
                 sw.Close();

                 if (inMasterHostname.Split('.')[0].ToUpper() == Environment.MachineName.ToUpper())
                 {
                     masterTargetIP = ".";
                     Domain = null;
                     Username = null;
                     Password = null;
                 }

                 RemoteWin remoteWin1 = new RemoteWin(masterTargetIP, Domain, Username, Password, inMasterHostname);

                 ArrayList filenames = new ArrayList();
                 filenames.Add("DrDrill.bat");
                 filenames.Add("Snapshot.xml");
                 filenames.Add("Inmage_scsi_unit_disks.txt");
                 string output = "";
                 int value;


                 Trace.WriteLine("Running copy and execute command on master target for recovery");
                 if (inMasterHostname.Split('.')[0].ToUpper() == Environment.MachineName.ToUpper())
                 {
                     value = remoteWin1.CopyAndExecuteInLocalPath("DrDrill.bat", Latestpath, filenames,  300000);
                 }
                 else
                 {
                     value = remoteWin1.CopyAndExecute("DrDrill.bat", Latestpath, filenames, 300000);
                 }

                 Thread.Sleep(5000);
                 
                 if (inMasterHostname.Split('.')[0].ToUpper() == Environment.MachineName.ToUpper())
                 {
                     if (remoteWin1.CopyFileFromLocalPathSystem("c:\\windows\\temp", "InMage_Recovered_Vms.snapshot", Vcontinuumpath) == false)
                     {
                         Trace.WriteLine(DateTime.Now + "\t Unable to copy InMage_Recovered_Vms.rollback from remote machine to local machine's vContinuum folder");
                         return 6;
                     }

                 }
                 else
                 {
                     if (remoteWin1.CopyFileFrom("c:\\windows\\temp", "InMage_Recovered_Vms.snapshot", Vcontinuumpath) == false)
                     {
                         Trace.WriteLine(DateTime.Now + "\t Unable to copy InMage_Recovered_Vms.rollback from remote machine to local machine's vContinuum folder");
                         return 6;
                     }
                 }


             }
             catch (Exception e)
             {
                 System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);
                 System.FormatException nht = new FormatException("Inner exception", e);
                 Trace.WriteLine(nht.GetBaseException());
                 Trace.WriteLine(nht.Data);
                 Trace.WriteLine(nht.InnerException);
                 Trace.WriteLine(nht.Message);
                 Trace.WriteLine("ERROR*******************************************");
                 Trace.WriteLine("Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                 Trace.WriteLine("Exception  =  " + e.Message);
                 Trace.WriteLine("ERROR___________________________________________");
                 return 6;

             }


             return 0;

         }
        // This method will create linuxinfo.sh
        public int DiscoverLinuxInfo(string inIP, string inFileName)
         {
             try
             {
                 //Creating sh file to run cscript
                 if (File.Exists(Latestpath + "\\linuxinfo.sh"))
                 {
                     File.Delete(Latestpath + "\\linuxinfo.sh");

                 }
                 string hostname = null, vxInstallationPath = null;
                 if (SetHostNameInputIP(inIP,  hostname, WinTools.GetcxIPWithPortNumber()) == false || hostname == null)
                 {
                     hostname = WinPreReqs.GetHostname;
                     Trace.WriteLine(DateTime.Now + "Unable to Fetch SourceIP from cx ");
                     Trace.WriteLine(DateTime.Now + "Can't proceeed without source server IP");
                     return 1;

                 }
                 if (SetvxAgentInstallationPath(hostname, ref vxInstallationPath, WinTools.GetcxIPWithPortNumber()) == false || vxInstallationPath == null)
                 {

                     Trace.WriteLine(DateTime.Now + "Unable to Fetch source server's VX path from cx ");
                     Trace.WriteLine(DateTime.Now + "Can't proceeed without source server's VX path");
                     return 2;

                 }

               hostname  = WinPreReqs.GetHostname;
                 FileInfo f1 = new FileInfo(Latestpath + "\\linuxinfo.sh");
                 StreamWriter sw = f1.CreateText();
                 sw.WriteLine("#!/bin/sh");
                 sw.WriteLine("#Running binary to fetch info");
                 sw.WriteLine( vxInstallationPath + "bin/EsxUtil -nw linux -noregister " + "-file " + inFileName );
                 //sw.WriteLine("sleep 5");
                 sw.Close();
             }
             catch (Exception Ex)
             {
                 Trace.WriteLine(DateTime.Now + "\t Exception message " + Ex.Message);
             }
             //copy file to Linux machine

             WinTools win = new WinTools();
             
             //if (win.CopyFileToLinuxMT(inIP, inUserName, inPassword,  WinTools.fxAgentPath() + "\\vContinuum\\Latest\\linuxinfo.sh") == false)
             //{
             //    Trace.WriteLine(DateTime.Now + "\t Failed to copy file to linux server " + inIP);
             //}
             //else
             //{
             //    if (win.ExecuteLinuxForP2VLinux(inIP, inUserName, inPassword, "linuxinfo.sh") == false)
             //    {
             //        Trace.WriteLine(DateTime.Now + "\t Failed to execute the sh script " + inIP);
             //    }
             //    else
             //    {
             //        Trace.WriteLine(DateTime.Now + "\t Successfully completed the execution ");
             //    }
             //}
             //if (win.CopyFileFromLinux(inIP, inUserName, inPassword, inFileName) == true)
             //{
             //    return 0;
             //}

             return 3;
         }


    }
}
