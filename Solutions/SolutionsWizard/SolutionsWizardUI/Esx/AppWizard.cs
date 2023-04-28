using System;
using System.Collections.Generic;
using System.Windows.Forms;
using Com.Inmage.Tools;
using System.IO;
using System.Diagnostics;
using Com.Inmage.Esxcalls;
using System.Threading;

namespace Com.Inmage.Wizard
{
    static class Program
    {

        static string LOGFILE_NAME = "logs\\vContinuum.log";
        static string LOGFILE_NAME1 = "logs\\vContinuum_commandline.log";
        static string TEMP_LOG = "logs\\vContinuum_UI.log";
        static string CX_API_LOG = "logs\\vContinuum_CXAPI.log";
        public static TextWriterTraceListener tr3;
        
        /// <summary>
        /// The main entry point for the application.
        /// </summary>
        [STAThread]
        static int Main(string[] args)
        {

            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            bool boolCreatedNewWindow = true;

           
            
            //Checking whether fx is instaled on vcon box or not
            if (WinTools.ServiceExist("frsvc") == false)
            {
                Trace.WriteLine(DateTime.Now + "Fx agent is not installed on this host");
                MessageBox.Show("Fx Agent is not installed on this host            " + Environment.NewLine + "Install Fx agent and proceed again", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return 1;
            }
            else
            {
              //Verifying the logs and latest folders exists or not.. If not we will create...
                string _vConInstallPath = WinTools.FxAgentPath() + "\\vContinuum";
               Directory.SetCurrentDirectory(_vConInstallPath);
               if (!Directory.Exists("logs"))
               {
                   Directory.CreateDirectory("logs");
               }
               if (!Directory.Exists("Latest"))
               {
                   Directory.CreateDirectory("Latest");
               }
                //Make all trace and debug message go console as well as to to a file
                //TextWriterTraceListener tr1 = new TextWriterTraceListener(System.Console.Out);
                // Debug.Listeners.Add(tr1);
                // Debug.AutoFlush = true;
                // Trace.AutoFlush = true;
               try
               {
                   //Here we will
                  
                   if (args.Length == 0)
                   {
                       if (!File.Exists(TEMP_LOG))
                       {
                           tr3 = new TextWriterTraceListener(System.IO.File.CreateText(TEMP_LOG));
                           Debug.Listeners.Add(tr3);
                       }
                       else
                       {
                           File.Delete(TEMP_LOG);
                           tr3 = new TextWriterTraceListener(System.IO.File.AppendText(TEMP_LOG));
                           Debug.Listeners.Add(tr3);
                       }
                       if (!File.Exists(LOGFILE_NAME))
                       {
                           TextWriterTraceListener tr2 = new TextWriterTraceListener(System.IO.File.CreateText(LOGFILE_NAME));
                           Debug.Listeners.Add(tr2);

                       }
                       else
                       {
                           TextWriterTraceListener tr2 = new TextWriterTraceListener(System.IO.File.AppendText(LOGFILE_NAME));
                           Debug.Listeners.Add(tr2);
                       }
                       Debug.AutoFlush = true;
                       Trace.AutoFlush = true;
                       //  Application.Run(new LaunchPreReqsForm());
                       string uniqueNameForMutex = "Global\\d3g2694c-5f57-5g45-9fdd-d26f4e6d99g1";
                       Mutex mutex = new System.Threading.Mutex(true, uniqueNameForMutex, out boolCreatedNewWindow);
                       if (boolCreatedNewWindow)
                       {
                           LaunchPreReqsForm launch = new LaunchPreReqsForm();
                           if (launch.ShowForm() == true)
                           {

                               if (launch.preresPassed == true)
                               {
                                   Application.Run(new ResumeForm());
                               }
                               else
                               {
                                   Application.Exit();
                               }
                           }
                       }
                   }
                   else
                   {
                      
                       bool _cleanUp = false, _updatetocx = false;
                       bool _upgradeFileToCx = false;
                       bool _updatefiletoLatest = false;
                       bool _uploadtocx = false;
                       bool _downloadfromcx = false;
                       string sourceHostName = null;
                       string mtHostname = null;
                       string planname = null;
                       string vconname = null;
                       string inputXml = null;
                       string esxIp = null;
                       string esxUsername = null;
                       string esxPassword = null;
                       string masterConfigFile = null;                     
                       bool cxapi = false;
                       bool getnodes = false;
                       string apiName = null;
                       string hostname = null; string ip = null; string id = null; string requestid = null;
                       string messageForHelp = null;
                       messageForHelp = "- 2014 Microsoft Corp. All rights reserved." + Environment.NewLine +
                                        "Following options are available for vContinuum command line operations" + Environment.NewLine + Environment.NewLine +
                                        "To upload MasterConfigFile.xml from Latest folder" + Environment.NewLine +
                                        "--uploadtocx" + Environment.NewLine +
                                        "To download MasterConfigFile.xml to Latest folder in vContinuum" + Environment.NewLine +
                                        "--downloadfromcx" + Environment.NewLine +
                                        "To update file to MasterConfigFile.xml " + Environment.NewLine +
                                        "--updatetocx <Xml File path to update>" + Environment.NewLine +
                                        "To run GethostInfo API " + Environment.NewLine +
                                            "--cxapi gethostinfo --hostname <hostname> (OR)" + Environment.NewLine +
                                            "--cxapi gethostinfo --ip <IP Address> (OR)" + Environment.NewLine +
                                            "--cxapi gethostinfo --id <UA host id of machine" + Environment.NewLine +
                                        "To run RefreshHost API" + Environment.NewLine +
                                        "--cxapi refreshhost --hostname <hostname> (OR)" + Environment.NewLine +
                                        "--cxapi refreshhost --ip <IP Address> (OR)" + Environment.NewLine +
                                        "--cxapi refreshhost --id <UA host id of machine" + Environment.NewLine +
                                        "To run GetRequestStatus API" + Environment.NewLine +
                                        "--getrequeststatus --requestid <request id>" + Environment.NewLine +
                                        "To run GetNode cxcli call " + Environment.NewLine +
                                         "--getnodes --hostname <hostname> (OR) " + Environment.NewLine +
                                             "--getnodes --ip <IP Address>";
                       //Control comes here if argument count is equal or more than 1.

                    
                       TextWriterTraceListener tr2;
                      TextWriterTraceListener tr4;
                       if (!File.Exists(LOGFILE_NAME1))
                       {
                            tr2 = new TextWriterTraceListener(System.IO.File.CreateText(LOGFILE_NAME1));
                           Debug.Listeners.Add(tr2);
                       }
                       else
                       {
                            tr2 = new TextWriterTraceListener(System.IO.File.AppendText(LOGFILE_NAME1));
                           Debug.Listeners.Add(tr2);
                       }
                    
                       if (!File.Exists(CX_API_LOG))
                       {
                           tr4 = new TextWriterTraceListener(System.IO.File.CreateText(CX_API_LOG));
                           Debug.Listeners.Add(tr4);
                       }
                       else
                       {
                          
                           tr4 = new TextWriterTraceListener(System.IO.File.AppendText(CX_API_LOG));
                           Debug.Listeners.Add(tr4);
                       }
                       
                       Debug.AutoFlush = true;
                       Trace.AutoFlush = true;
                       if (WinTools.GetHttpsOrHttp() == false)
                       {
                           WinTools.Https = false;
                           Trace.WriteLine(DateTime.Now + "\t Https is false");
                       }
                       else
                       {
                           WinTools.Https = true;
                           Trace.WriteLine(DateTime.Now + "\t Https is True");
                       }
                    
                  
                       
                       //This will be called if we find any arguments for vContinuum.exe 

                       Trace.WriteLine(DateTime.Now + "\t\tPrinting command line arguments");
                       string command = "";
                       foreach (string arg in args)
                       {
                           command = command + arg;

                       }
                      
                       Trace.WriteLine(command);
                       int index = 0;
                       foreach (string argument in args)
                       {

                           switch (argument)
                           {
                               case "--cleanup":
                                   _cleanUp = true;
                                   break;
                               case "--hostname":
                                   sourceHostName = args[index + 1].ToString();
                                   hostname = args[index + 1].ToString();
                                   break;
                               case "--mt":
                                   mtHostname = args[index + 1].ToString();
                                   break;
                               case "--planname":
                                   planname = args[index + 1].ToString();
                                   break;
                               case "--vconname":
                                   vconname = args[index + 1].ToString();
                                   break;
                               case "--updatetocx":
                                   //this is for append the entries for esx.xml\                                    
                                   inputXml = args[index + 1].ToString();
                                   _updatetocx = true;
                                   break;
                               case "--uploadtocx":
                                   _uploadtocx = true;
                                   break;
                               case "--downloadfromcx":
                                   _downloadfromcx = true;
                                   break;
                               //this is for append the entries for esx_master.xml
                               case "--updatetolatest":
                                   _updatefiletoLatest = true;
                                   inputXml = args[index + 1].ToString();
                                   break;
                               case "--cxapi":
                                   cxapi = true;                                 
                                   apiName = args[index + 1].ToString();
                                   break;                               
                               case "--ip":
                                   ip = args[index + 1].ToString();
                                   break;
                               case "--id":
                                   id = args[index + 1].ToString();
                                   break;
                               case "--requestid":
                                   requestid = args[index + 1].ToString();
                                   break;
                               case "--getnodes":
                                   Trace.WriteLine(DateTime.Now + "\t Get node");
                                   getnodes = true;
                                   break;
                               case "--help":                                   
                                   Trace.WriteLine(messageForHelp);                                                              
                                   break;
                               case "--h":
                                   Trace.WriteLine(messageForHelp);
                                   break;
                               case "/?":
                                    Trace.WriteLine(messageForHelp);
                                   break;
                               case "-h":
                                   Trace.WriteLine(messageForHelp);
                                   break;


                           }
                           index++;

                       }
                       
                       if (_cleanUp == true)
                       {
                           if (sourceHostName == null || mtHostname == null || planname == null || vconname == null)
                           {
                               Trace.WriteLine(DateTime.Now + "\t Found some of the args are empty ");
                               Trace.WriteLine(DateTime.Now + "\t Unable to proceed with clean up");
                               return 3;
                           }
                           Host h = new Host();
                           h.delete = true;
                           h.hostname = sourceHostName;
                           Remove clean = new Remove();
                           Trace.WriteLine(DateTime.Now + " Printing the options " + sourceHostName + " " + mtHostname + " " + planname + " " + vconname);
                           if (clean.RemoveReplication(h, mtHostname, planname, WinTools.GetcxIPWithPortNumber(), vconname) == true)
                           {
                               h = clean.GetHost;
                               Trace.WriteLine(DateTime.Now + "\t Successfully deleted the consistency and protection jobs for " + h.hostname);
                               return 0;
                           }
                           else
                           {
                               Trace.WriteLine(DateTime.Now + "\t Failed to delete the consistency and protection jobs for " + h.hostname);
                               return 1;
                           }
                       }
                       else if (_updatetocx == true)
                       {
                           MasterConfigFile masterConfig = new MasterConfigFile();
                           if (masterConfig.UpdateMasterConfigFile(inputXml) == true)
                           {
                               Trace.WriteLine(DateTime.Now + "\t Successfully updated the masterconfigfile with the input xml: " + inputXml);
                               return 0;
                           }
                           else
                           {
                               Trace.WriteLine(DateTime.Now + "\t Failed to update the masterconfigfile with the input xml: " + inputXml);
                               return 1;
                           }
                       }
                       else if (_downloadfromcx == true)
                       {
                           Trace.WriteLine(DateTime.Now + " \t\t download of masterConfigFile is called by command line");
                           MasterConfigFile master = new MasterConfigFile();
                           if (master.DownloadMasterConfigFile(WinTools.GetcxIPWithPortNumber()) == true)
                           {
                               Trace.WriteLine(DateTime.Now + "\t \t Successfully downloaded the file from cx");
                           }
                           else
                           {
                               Trace.WriteLine(DateTime.Now + "\t \t Failed to downloaded the file from cx");
                           }
                       }
                       else if (_uploadtocx == true)
                       {
                           Trace.WriteLine(DateTime.Now + " \t\t Upload of masterConfigFile is called by command line");
                           MasterConfigFile master = new MasterConfigFile();
                           if (master.UploadMasterConfigFile(WinTools.GetcxIPWithPortNumber()) == true)
                           {
                               Trace.WriteLine(DateTime.Now + "\t \t Successfully Uploaded the file to  cx");
                           }
                           else
                           {
                               Trace.WriteLine(DateTime.Now + "\t \t Failed upload the file to  cx");
                           }
                       }
                       else if (_updatefiletoLatest == true)
                       {
                           Trace.WriteLine(DateTime.Now + " \t\t updatetolatest of masterConfigFile is called by command line.Input xml file is: " + inputXml);
                           if (inputXml != null)
                           {
                               if (File.Exists(inputXml))
                               {
                                   MasterConfigFile masterConfig = new MasterConfigFile();
                                   masterConfig.UpdateMasterConfigFiletoLatestVersion(inputXml);
                               }
                           }
                       }
                       else if(cxapi == true)
                       {
                           string cxIP = WinTools.GetcxIPWithPortNumber();
                           WinPreReqs win = new WinPreReqs("", "", "", "");
                           Cxapicalls cxAPis = new Cxapicalls();
                           Host h = new Host();
                           if (apiName != null)
                           {
                               if(id != null)
                               {
                                   h.inmage_hostid = id;

                               }
                               else if (hostname != null)
                               {
                                   h.hostname = hostname;
                                  
                                   win.SetHostIPinputhostname(h.hostname, h.ip, cxIP);
                                   h.ip = WinPreReqs.GetIPaddress;
                                   win.GetDetailsFromcxcli(h, cxIP);
                                   h = WinPreReqs.GetHost;
                               }
                               else if(ip != null)
                               {
                                   h.ip = ip;                                   
                                   WinPreReqs.SetHostNameInputIP(h.ip, h.hostname, cxIP);
                                   h.hostname = WinPreReqs.GetHostname;
                                   win.GetDetailsFromcxcli(h, cxIP);
                                   h = WinPreReqs.GetHost;

                               }
                               else if (requestid != null)
                               {
                                   h.requestId = requestid;
                               }

                               if (apiName == "gethostinfo")
                               {
                                   if (cxAPis.Post(h, "GetHostInfo") == true)
                                   {

                                       h = Cxapicalls.GetHost;
                                       h.role = Role.Primary;
                                       h.ip = ip;
                                       h.displayname = h.hostname;
                                   }
                               }
                               else if (apiName == "getrequeststatus")
                               {
                                   Host h1 = new Host();
                                   h1 = h;
                                   if (cxAPis.Post(h1, "GetRequestStatus") == true)
                                   {

                                   }
                               }
                               else if(apiName == "refreshhost")
                               {
                                   Host h1 = new Host();
                                   h1 = h;
                                   if (cxAPis.PostRefreshWithall(h1) == true)
                                   {
                                       h1 = Cxapicalls.GetHost;
                                       h.requestId = h1.requestId;
                                       Trace.WriteLine(DateTime.Now + "\t Successfully request for refresh host info with request id " + h.requestId);
                                   }
                               }
                           }
                       }
                       else if(getnodes == true)
                       {
                           Trace.WriteLine(DateTime.Now + "\t User opted for getnodes");
                           string cxIP = WinTools.GetcxIPWithPortNumber();
                           WinPreReqs win = new WinPreReqs("", "", "", "");
                           Cxapicalls cxAPis = new Cxapicalls();
                           string ipofdummy = null;
                           Host h1 = new Host();

                           if (hostname != null)
                           {
                               Trace.WriteLine(DateTime.Now + "\t User send hostname as argument");
                               h1.hostname = hostname;
                               win.SetHostIPinputhostname(hostname, ipofdummy, cxIP);
                               h1.ip = WinPreReqs.GetIPaddress;
                               win.GetDetailsFromcxcli(h1, cxIP);
                           }
                           else if (ip != null)
                           {
                               h1.ip = ip;
                               Trace.WriteLine(DateTime.Now + "\t Going to set hostname");
                               WinPreReqs.SetHostNameInputIP(ip, hostname, cxIP);
                               Trace.WriteLine(DateTime.Now + "\t Completed serach for hostname");
                               hostname = WinPreReqs.GetHostname;
                               h1.hostname = hostname;
                               h1.displayname = hostname;
                               Trace.WriteLine(DateTime.Now + "\t Hostname " + hostname);
                               if (hostname != null)
                               {
                                   win.GetDetailsFromcxcli(h1, cxIP);
                               }
                               else
                               {
                                   Trace.WriteLine(DateTime.Now + "\t Server details not exist in CX");
                               }
                           }
                           else
                           {
                               Trace.WriteLine(DateTime.Now + "\t all are null");
                               win.SetHostIPinputhostname("test", ipofdummy, cxIP);
                           }
                       }
                       
                   }
               }
               catch (IOException e)
               {
                   System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);
                   Trace.WriteLine("ERROR*******************************************");
                   Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                   Trace.WriteLine("Exception  =  " + e.Message);
                   Trace.WriteLine("ERROR___________________________________________");
                   MessageBox.Show("Another instance of vContinuum is running. Exiting..", "vContinuum", MessageBoxButtons.OK, MessageBoxIcon.Error);
                   return 2;
               }
                return 0;
            }
        }
    }
}
