using System;
using System.Collections.Generic;
using System.Text;
using Com.Inmage.Tools;
using System.Diagnostics;
using System.Threading;
using System.Windows.Forms;
using System.IO;
using System.Collections;


namespace Com.Inmage
{
    public class PushInstall
    {
        static string Silentinstall = " /VERYSILENT -usesysvolumes -ip ";
        static string Portnumber = " -port ";
        static string Restart = " -restart Y ";
        static string Norestart = " /norestart ";
        static string Setappagentmanual = " -setappagent manual";
        static string Tempdir = "c:";
        internal string Vconinstalpath = WinTools.FxAgentPath() + "\\vContinuum";
        //  static int MAX_WAIT_TIME = 60000;
        private string passPharsePath = WinTools.GetProgramFilesPath() + "\\InMage Systems";
        public const int RETURN_CODE_CERT_TRUST_VERIFICATION_FAILED = 11;
        public const int RETURN_CODE_CERT_ROOT_IS_NOT_MS_CA = 12;

        public int PushAgent(string machineIP, string domain, string userName, string passWord,
                                    string pathOfBinary, string binaryName, string cxIP, string cxPort, 
                                     bool restart,   RemoteWin remote, Host host)
        {
            int returncode;
            string remoteInstallCommand;
            string errorMessage = "";
            string output = "";
            WmiCode errorCode = WmiCode.Unknown;

            try
            {
                Trace.WriteLine("\n Entering PushInstaller: PushInstallToWindows CXIP, PORT = " + cxIP + " " + cxPort);

               

                
                remote = new RemoteWin(machineIP, domain, userName, passWord,host.hostname);
                remote.Connect( errorMessage,  errorCode);

                errorCode = remote.GetWmiCode;
                errorMessage = remote.GetErrorString;

                if (restart == true)
                {

                    remoteInstallCommand = Tempdir + "\\" + binaryName + "  " + Restart + Silentinstall + host.cxIP + Portnumber + host.portNumber + " /SUPPRESSMSGBOXES";

                }
                else
                {
                    remoteInstallCommand = Tempdir + "\\" + binaryName + "  " + Norestart + Silentinstall + host.cxIP + Portnumber + host.portNumber + " /SUPPRESSMSGBOXES";
                }

                if (host.role == Role.Primary)
                {
                    //remoteInstallCommand = remoteInstallCommand + SET_APPAGENT_MANUAL ;

                }
                else
                {
                    remoteInstallCommand = remoteInstallCommand + " /role vconmt";
                }
               

                if (host.natip != null)
                {
                    //Installing Unified agent using nat ip instead actual physical ip.
                    remoteInstallCommand = remoteInstallCommand + " -natip " + host.natip;
                }
                if (host.enableLogging == true)
                {
                    remoteInstallCommand = remoteInstallCommand + " /debuglog /loglevel 7";
                }
                if (host.cacheDir != null)
                {
                    remoteInstallCommand = remoteInstallCommand + " /cachedir " + "\""+  host.cacheDir + "\"";
                }

                if (host.https == true)
                {
                    remoteInstallCommand = remoteInstallCommand + " /CommunicationMode Https";
                }
                else
                {
                    remoteInstallCommand = remoteInstallCommand + " /CommunicationMode Http";
                }

                if (File.Exists(passPharsePath + "\\private\\connection.passphrase"))
                {
                    remoteInstallCommand = remoteInstallCommand + " /PassphrasePath " + "\"" + "C:\\Windows\\Temp\\connection.passphrase" + "\"";
                }


                if (remote.ConnectAndCreateFile(600) == 0)
                {
                    Trace.WriteLine(DateTime.Now + "\t Successfully created file for passpharse");
                }
                else
                {
                    Trace.WriteLine(DateTime.Now + "\t Failed to create passpharse file in remote machine");
                }
                Trace.WriteLine(DateTime.Now+"\t PushInstaller: PushInstallToWindows Executing command " + remoteInstallCommand);            

                if (File.Exists(Vconinstalpath + "\\UnifiedAgent.bat"))
                {
                    File.Delete(Vconinstalpath + "\\UnifiedAgent.bat");
                }



                FileInfo f1 = new FileInfo(Vconinstalpath + "\\UnifiedAgent.bat");
                StreamWriter sw = f1.CreateText();
                sw.WriteLine("echo off");
                sw.WriteLine(remoteInstallCommand);
                if (host.natip != null)
                {
                    sw.WriteLine("set exittcode=%errorlevel%");
                    sw.WriteLine("timeout/T 5");
                    sw.WriteLine("net stop svagents");
                    sw.WriteLine("timeout/T 5");
                    sw.WriteLine("net start svagents");
                }
                sw.WriteLine("exit %errorlevel%");
                sw.Close();

                ArrayList listOfFiles = new ArrayList();
                listOfFiles.Add(binaryName);
                listOfFiles.Add("UnifiedAgent.bat");
                // Verify code signing for the installer before copying it to the remote machine.
                // The following verifies only UnifiedAgent.exe but not UnifiedAgent.bat for each VM to initiate push install.
                string fileToverify = Path.Combine(pathOfBinary, binaryName);                
                if (Com.Inmage.Security.WinTrustWrapper.VerifyTrust(fileToverify))
                {                    
                    if (Com.Inmage.Security.CryptoWrapper.IsCertificateRootMS(fileToverify))
                    {
                        if (host.hostname.Split('.')[0].ToUpper() != Environment.MachineName.ToUpper())
                        {
                            returncode = remote.CopyAndExecute("UnifiedAgent.bat", pathOfBinary, listOfFiles, 600);
                        }
                        else
                        {
                            Trace.WriteLine(DateTime.Now + "\t Came here to install agenrts on local machine ");
                            returncode = remote.CopyAndExecuteInLocalPath("UnifiedAgent.bat", pathOfBinary, listOfFiles, 600);
                        }
                    }
                    else
                    {
                        returncode = RETURN_CODE_CERT_ROOT_IS_NOT_MS_CA;
                    }
                }
                else
                {
                    returncode = RETURN_CODE_CERT_TRUST_VERIFICATION_FAILED;
                }
                
                Trace.WriteLine(DateTime.Now + "\t PushInstaller: Completed with return code with return code:" + returncode);
                
                return returncode;

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
                return -1;
            }
        }

        public static int IsAgentExists(string machineIP, string domain, string userName, string passWord, string hostname)

        { 
              int _returncode;
       
            string errorMessage = "";
            string output = "";
            WmiCode errorCode = WmiCode.Unknown;
            string _installPath = WinTools.FxAgentPath() + "\\vContinuum";
            //string _installPath = Directory.GetCurrentDirectory();
            try 
            {
               
                RemoteWin remote = new RemoteWin(machineIP, domain, userName, passWord,hostname);
               
                remote.Connect( errorMessage,  errorCode);

                errorCode = remote.GetWmiCode;
                errorMessage = remote.GetErrorString;
                ArrayList listOfFiles = new ArrayList();
                listOfFiles.Add("AgentStatus.bat");
                if (userName != null)
                {
                    _returncode = remote.CopyAndExecute("AgentStatus.bat", _installPath, listOfFiles,  600);
                }
                else
                {
                    _returncode = remote.CopyAndExecuteInLocalPath("AgentStatus.bat", _installPath, listOfFiles, 600);

                }

                switch (_returncode)
                {
                    case 0:
                        Trace.WriteLine(DateTime.Now + "VX and FX are installed on " + machineIP);
                        break;
                    case 1:
                        Trace.WriteLine(DateTime.Now + "VX and FX are not installed on " + machineIP);
                        break;
                    case 2:
                        Trace.WriteLine(DateTime.Now + "Only FX is installed on " + machineIP);
                        break;
                    case 3:
                        Trace.WriteLine(DateTime.Now + "Only VX is installed on " + machineIP);
                        break;
                }

                return _returncode;
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
                return -1;
            }
        
        }

        public int UpdatecxIPandPortNumber(string machineIP, string domain, string userName, string passWord,
                                    string pathOfBinary, string binaryName, Host host)
        {   
            int returncode;
            string errorMessage = "";
            string output = "";
            WmiCode errorCode = WmiCode.Unknown;
            string remoteInstallCommand = Tempdir + "\\" + binaryName + "  " + Silentinstall + " " + host.cxIP + Portnumber + host.portNumber + " /UpdateConfig Y /SUPPRESSMSGBOXES";

            try
            {
                
                if (File.Exists(Vconinstalpath + "\\UnifiedAgent.bat"))
                {
                    File.Delete(Vconinstalpath + "\\UnifiedAgent.bat");
                }


                FileInfo f1 = new FileInfo(Vconinstalpath + "\\UnifiedAgent.bat");
                StreamWriter sw = f1.CreateText();
                sw.WriteLine("echo off");
                sw.WriteLine(remoteInstallCommand);
                sw.WriteLine("exit %errorlevel%");
                sw.Close();

                ArrayList listOfFiles = new ArrayList();
                listOfFiles.Add(binaryName);
                listOfFiles.Add("UnifiedAgent.bat");
                
                RemoteWin remote = new RemoteWin(machineIP, domain, userName, passWord,host.hostname);
                remote.Connect( errorMessage,  errorCode);

                errorCode = remote.GetWmiCode;
                errorMessage = remote.GetErrorString;
                listOfFiles.Add(binaryName);
                if (host.hostname.Split('.')[0].ToUpper() != Environment.MachineName.ToUpper())
                {
                    returncode = remote.CopyAndExecute("UnifiedAgent.bat", pathOfBinary, listOfFiles,  600);
                }
                else
                {
                    returncode = remote.CopyAndExecuteInLocalPath("UnifiedAgent.bat", pathOfBinary, listOfFiles, 600);
                }

              
                return returncode;
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
                return -1;
            }

        }
        public int UninstallAgent(string machineIP, string domain, string userName, string passWord,
                                        string pathOfBinary, string binaryName,Host host)
        {
            int _returncode;
            string errorMessage = "";
            string output = "";
            WmiCode errorCode = WmiCode.Unknown;
            string remoteInstallCommand = Tempdir + "\\" + binaryName + " /VERYSILENT  /Uninstall Y /norestart";
            try
            {
               
                RemoteWin remote = new RemoteWin(machineIP, domain, userName, passWord, host.hostname);

                remote.Connect( errorMessage,  errorCode);

                errorCode = remote.GetWmiCode;
                errorMessage = remote.GetErrorString;
                if (File.Exists(Vconinstalpath + "\\UnifiedAgent.bat"))
                {
                    File.Delete(Vconinstalpath + "\\UnifiedAgent.bat");
                }


                if (File.Exists(passPharsePath + "\\private\\connection.passphrase"))
                {
                    remoteInstallCommand = remoteInstallCommand + " /PassphrasePath " + "\"" + "C:\\Windows\\Temp\\connection.passphrase" + "\"";
                }


                if (remote.ConnectAndCreateFile(600) == 0)
                {
                    Trace.WriteLine(DateTime.Now + "\t Successfully created file for passpharse");
                }
                else
                {
                    Trace.WriteLine(DateTime.Now + "\t Failed to create passpharse file in remote machine");
                }

                FileInfo f1 = new FileInfo(Vconinstalpath + "\\UnifiedAgent.bat");
                StreamWriter sw = f1.CreateText();
                sw.WriteLine("echo off");
                sw.WriteLine(remoteInstallCommand);
                sw.WriteLine("exit %errorlevel%");
                sw.Close();

                ArrayList listOfFiles = new ArrayList();
                listOfFiles.Add(binaryName);
                listOfFiles.Add("UnifiedAgent.bat");


                listOfFiles.Add(binaryName);
                string fileToverify = Path.Combine(pathOfBinary, binaryName);
                if (Com.Inmage.Security.WinTrustWrapper.VerifyTrust(fileToverify))
                {
                    if (Com.Inmage.Security.CryptoWrapper.IsCertificateRootMS(fileToverify))
                    {
                        if (host.hostname.Split('.')[0].ToUpper() != Environment.MachineName.ToUpper())
                        {
                            _returncode = remote.CopyAndExecute("UnifiedAgent.bat", pathOfBinary, listOfFiles, 600);
                        }
                        else
                        {
                            _returncode = remote.CopyAndExecuteInLocalPath("UnifiedAgent.bat", pathOfBinary, listOfFiles, 600);
                        }
                    }
                    else
                    {
                        _returncode = RETURN_CODE_CERT_ROOT_IS_NOT_MS_CA;
                    }
                }
                else
                {
                    _returncode = RETURN_CODE_CERT_TRUST_VERIFICATION_FAILED;
                }
               // _returncode = remote.Execute(remoteInstallCommand);
                return _returncode;
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
                return -1;
            }
        
        
        
        }

        public int UpgradeAgents(string machineIP, string domain, string userName, string passWord,
                                       string pathOfBinary, string binaryName,  Host host)
        {
            int _returncode;
            string errorMessage = ""; 
            string output = "";
            WmiCode errorCode = WmiCode.Unknown;
            string remoteInstallCommand = null;
            if (host.role == Role.Secondary)
            {
                remoteInstallCommand = Tempdir + "\\" + binaryName + "  -a u " + " /VERYSILENT " + "/SUPPRESSMSGBOXES" + " /NORESTART /role vconmt";
            }
            else
            {
                remoteInstallCommand = Tempdir + "\\" + binaryName + "  -a u " + " /VERYSILENT " + "/SUPPRESSMSGBOXES" + " /NORESTART " ;
            }
            try
            {

                if (host.natip != null)
                {
                    //Installing Unified agent using nat ip instead actual physical ip.
                    remoteInstallCommand = remoteInstallCommand + " -natip " + host.natip;
                }
                if (host.enableLogging == true)
                {
                    remoteInstallCommand = remoteInstallCommand + " /debuglog /loglevel 7";
                }
                if (host.cacheDir != null)
                {
                    remoteInstallCommand = remoteInstallCommand + " /cachedir " + "\"" + host.cacheDir + "\"";
                }
                if (File.Exists(passPharsePath + "\\private\\connection.passphrase"))
                {
                    remoteInstallCommand = remoteInstallCommand + " /PassphrasePath " + "\"" + "C:\\Windows\\Temp\\connection.passphrase" + "\"";
                }
                //if (h.https == true)
                //{
                //    remoteInstallCommand = remoteInstallCommand + " /CommunicationMode https";
                //}
                //else
                //{
                //    remoteInstallCommand = remoteInstallCommand + " /CommunicationMode http";
                //}
                FileInfo f1 = new FileInfo(Vconinstalpath + "\\UnifiedAgent.bat");
                StreamWriter sw = f1.CreateText();
                sw.WriteLine("echo off");
                sw.WriteLine(remoteInstallCommand);
                if (host.natip != null)
                {
                    sw.WriteLine("set exittcode=%errorlevel%");
                    sw.WriteLine("net stop svagents");
                    sw.WriteLine("net start svagents");
                }
                sw.WriteLine("sc config " + "\"" + "InMage Scout Application Service" + "\"" + " start= auto");
                sw.WriteLine("exit %errorlevel%");
                sw.Close();

                ArrayList listOfFiles = new ArrayList();
                listOfFiles.Add(binaryName);
                listOfFiles.Add("UnifiedAgent.bat");

                RemoteWin remote = new RemoteWin(machineIP, domain, userName, passWord,host.hostname);
                if (remote.ConnectAndCreateFile(600) == 0)
                {
                    Trace.WriteLine(DateTime.Now + "\t Successfully created file for passpharse");
                }
                else
                {
                    Trace.WriteLine(DateTime.Now + "\t Failed to create passpharse file in remote machine");
                }
                remote.Connect( errorMessage,  errorCode);

                errorCode = remote.GetWmiCode;
                errorMessage = remote.GetErrorString;
                listOfFiles.Add(binaryName);

                // Verify code signing for the installer before copying it to the remote machine.
                // The following verifies only UnifiedAgent.exe but not UnifiedAgent.bat for each VM to intiate push upgrade.
                string fileToverify = Path.Combine(pathOfBinary, binaryName);
                if (Com.Inmage.Security.WinTrustWrapper.VerifyTrust(fileToverify))
                {
                    if (Com.Inmage.Security.CryptoWrapper.IsCertificateRootMS(fileToverify))
                    {
                        if (host.hostname.Split('.')[0].ToUpper() != Environment.MachineName.ToUpper())
                        {
                            _returncode = remote.CopyAndExecute("UnifiedAgent.bat", pathOfBinary, listOfFiles, 600);
                        }
                        else
                        {
                            Trace.WriteLine(DateTime.Now + "\t Came here to upgrade agents on local machine ");
                            _returncode = remote.CopyAndExecuteInLocalPath("UnifiedAgent.bat", pathOfBinary, listOfFiles, 600);
                        }
                    }
                    else
                    {
                        _returncode = RETURN_CODE_CERT_ROOT_IS_NOT_MS_CA;
                    }
                }
                else
                {
                    _returncode = RETURN_CODE_CERT_TRUST_VERIFICATION_FAILED;
                }
                Trace.WriteLine(DateTime.Now + "\t Priniting the return code of upgrade " + _returncode.ToString());
                return _returncode;
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
                return -1;
            }

        }
    
    
    } 
}
