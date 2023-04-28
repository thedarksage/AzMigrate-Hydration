using System;
using System.Collections.Generic;
using System.Text;
using System.Collections;
using System.ComponentModel;
using System.Data;
using System.Management;
using System.Windows.Forms;
using Com.Inmage;
using System.Threading;

using System.Net.Sockets;
using System.Diagnostics;
using System.IO;




namespace Com.Inmage.Tools
{//Error Codes Range: 01_001_

   public enum WmiCode { Suceess = 0, Credentialerror, RpcError, Notreachable, Unknown };

   public class RemoteWin
    {
        //Error codes Range: 01_001_[000 to 003]

        internal  static string Templocalmountdrive = "Q:";
        internal static string Templocalmountdrives = "Q:,R:,W:,X:,P:,U:,T:,S:,U:,V:,Z:,K:,L:,M:,N:,O:,P:";
        internal static string Remotedirtomount = @"C:";
        internal static string Outputfile = "temp_output.txt";
        internal static string Tempdir = "C:\\Windows\\Temp\\";
        internal static string Remotetemppath = "C:\\Windows\\Temp\\";
        internal static string Remotetempdir = "\\Windows\\Temp";
        internal static string Outputfilepath = Tempdir + Outputfile;
        internal int Returncode = -1;
        internal static ArrayList refArrayList;
        internal static string refErrorMessage;
        internal static WmiCode referrorCode;
        internal static Hashtable refSysHash;
        internal ManagementScope scope = null;
        //public string _remoteIP, _remoteDomain, _remoteUserName, _remotePassWord, _localTempDriveLetter = TEMP_LOCAL_MOUNT_DRIVE, _remoteDirToMount = REMOTE_DIR_TO_MOUNT;
        internal string remoteIP, remoteDomain, remoteUserName, remotePassWord, remoteHostName, localTempDriveLetter = Templocalmountdrive, remoteDirToMount = Remotedirtomount;
        internal Hashtable configInfo;
        internal bool connected = false;
        internal bool fileShareEnabledCheck = false;
        internal static string refMapnetworkdrive;
        internal static string refOSedition;
        //private string vxInstallPath = WinTools.Vxagentpath();
        private string passPharsePath = WinTools.GetProgramFilesPath() + "\\InMage Systems";
       private static string passPharse = "\\private\\connection.passphrase";
         ~RemoteWin()
        {
            try
            {
                //Detach(_localTempDriveLetter);
            }
            catch (Exception e)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + e.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

         public RemoteWin(string inMachineIP, string inDomain, string inUserName, string inPassWord, string inHostName)
        {

        //    AppConfig app = new AppConfig();
        //    Debug.WriteLine("Calling GetConfigInfo in RemoteWin");
        //    _configInfo = app.GetConfigInfo("REMOTE_WIN");
       //     app.PrintConfigValues(_configInfo);

            try
            {

                if (configInfo != null)
                {
                    if (configInfo["DriveLetterToUse"] != null)
                    {
                        localTempDriveLetter = configInfo["DriveLetterToUse"].ToString();
                    }
                }
                else
                {
                    localTempDriveLetter = GetLocalTempMountPoint();

                }


                if (inMachineIP == String.Empty)
                {
                    Trace.WriteLine("01_001_001: RemoteWin: IP address is null  ");

                }



                if (configInfo != null)
                {

                    if (configInfo["RemoteTempDirPath"] != null)
                    {
                        remoteDirToMount = configInfo["RemoteTempDirPath"].ToString();
                    }
                    else
                    {
                        remoteDirToMount = Remotedirtomount;
                    }
                }
                else
                {
                    remoteDirToMount = Remotedirtomount;
                }


                remoteIP = inMachineIP;
                remoteDomain = inDomain;
                remoteUserName = inUserName;
                remotePassWord = inPassWord;
                remoteHostName = inHostName;

            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
               

            }

            
           
        }

       internal static string GetLocalTempMountPoint()
       {

           string systemWinDir = "";
           string tempMountPoint = "r:";
           try
           {
               //String[] drivesToCheck = { "l:", "m:", "n:", "o:", "p:", "q:", "r:", "s:", "t:", "u:", "v:", "x:", "y:", "z:", "i:", "j:", "k:" };
               bool driveLeterExists = false;
               DriveInfo[] driveLetters = DriveInfo.GetDrives();
               foreach (string localdrive in RemoteWin.Templocalmountdrives.Split(','))
               {
                   driveLeterExists = false;
                   RemoteWin.Templocalmountdrive = localdrive;
                   foreach (DriveInfo driveLetter in driveLetters)
                   {
                      // Trace.WriteLine(DateTime.Now + "\t Comparing the drives " + driveLetter.ToString() + " \t  local drive " + localdrive);
                       if (driveLetter.ToString() == localdrive + "\\")
                       {
                           Trace.WriteLine(DateTime.Now + "\t Drive exists  " + driveLetter.ToString());
                           driveLeterExists = true;
                           break;


                       }

                   }
                   Trace.WriteLine(DateTime.Now + "\t Priniting the driveletter " + localdrive);
                   if (driveLeterExists == false)
                   {
                       break;
                   }
               }

               bool matched = false;
               /*
                          

               */
               //For 5.5 we will hard code it to 
               Trace.WriteLine(DateTime.Now + "\t For map network drive we are using " + RemoteWin.Templocalmountdrive);
               tempMountPoint = RemoteWin.Templocalmountdrive;

               Debug.WriteLine("RemoteWin::GetLocalTempMountPoint Retrutning the temp mount point" + tempMountPoint);
           }
           catch (Exception ex)
           {

               System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

               Trace.WriteLine("ERROR*******************************************");
               Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
               Trace.WriteLine("Exception  =  " + ex.Message);
               Trace.WriteLine("ERROR___________________________________________");
              

           }
           return tempMountPoint;

       }


       private bool CreatePassphraseFile(string mappedDrive)
       {
           try
           {
               string filepath = passPharsePath + passPharse;
               string fileToCreate = mappedDrive + "\\Windows\\temp\\connection.passphrase";
               if (File.Exists(filepath))
               {
                   StreamReader reader = new StreamReader(filepath);
                   string fileText = reader.ReadToEnd();
                   reader.Close();
                   StreamWriter writer = new StreamWriter(fileToCreate);
                   writer.Write(fileText);
                   writer.Close();
                   Trace.WriteLine(DateTime.Now + "\t Successfully write the file ");
                   WinTools win = new WinTools();
                   win.SetFilePermissions(fileToCreate);
               }
           }
           catch (Exception ex)
           {

               Trace.WriteLine(DateTime.Now + "\t Failed to write fingerprint file " + ex.Message);
               return false;
           }
           return true;
       }


       internal int ConnectAndCreateFile(int inMaxWaitTime)
       {

           Trace.WriteLine(DateTime.Now + "\t Entered the Copy and Execute ");
           string errorMessage = "";
           WmiCode errorCode = WmiCode.Unknown;

           bool timedOut = false;

           string remoteCommand = "";
           string mappedDrive = "";
           int processId = -1;

           try
           {
               Connect(errorMessage, errorCode);


               errorCode = GetWmiCode;
               errorMessage = GetErrorString;
               Trace.WriteLine(DateTime.Now + "\t error code for connect  " + errorCode);
               Trace.WriteLine(DateTime.Now + "\t error string " + errorMessage);

               if (!File.Exists(passPharsePath + passPharse))
               {
                   Trace.WriteLine("RemoteWin:: ConnectAndCreateFile: File: " + passPharsePath + "\\" + passPharse + " doesn't exist");
                   return -1;
               }
               if (MapRemoteDirToDrive(mappedDrive) == false)
               {

                   Trace.WriteLine("RemoteWin:: CopyFilesTo: Couldn't map to local drive");
                   return -1;

               }
               mappedDrive = GetMapNetworkdrive;
               CreatePassphraseFile(mappedDrive);
               Detach(mappedDrive);

               Thread.Sleep(5000);
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
               Trace.WriteLine("Caught exception at Method: ConnectAndCreateFile" + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
               Trace.WriteLine("Exception  =  " + ex.Message);
               Trace.WriteLine("ERROR___________________________________________");
               return -1;
           }
           return 0;
       }

       internal int CopyAndExecute(string fileName, string dirName, ArrayList fileList, int inMaxWaitTime)
       {

           Trace.WriteLine(DateTime.Now + "\t Entered the Copy and Execute ");
           string errorMessage = "";
           WmiCode errorCode = WmiCode.Unknown;

           bool timedOut = false;

           string remoteCommand = "";
           string mappedDrive = "";
           int processId = -1;

           try
           {
               Connect( errorMessage,  errorCode);
           

               errorCode = GetWmiCode;
               errorMessage = GetErrorString;
               Trace.WriteLine(DateTime.Now + "\t error code for connect  " + errorCode);
               Trace.WriteLine(DateTime.Now + "\t error string " + errorMessage);
               if (!File.Exists(dirName + "\\" + fileName))
               {
                   Trace.WriteLine("RemoteWin:: CopyAndExecute: File: " + dirName + "\\" + fileName + " doesn't exist");
                   return -1;
               }

               bool copyStatus = CopyFilesTo(dirName, fileName, fileList, Remotetemppath,  mappedDrive);
               mappedDrive = GetMapNetworkdrive;
               if (copyStatus == true)
               {

                   remoteCommand = Remotetemppath + "\\" + fileName + " > " + Remotetemppath + "\\" + Outputfile;

               }
               else
               {
                   Trace.WriteLine("\t RemoteWin: CopyAndExecute Could not copy file " + dirName + "\\" + fileName + " to remote machine ");
                   return -1;
               }


               Debug.WriteLine("Remote command to be executed is:" + remoteCommand);


               

               processId = Execute(remoteCommand);

               ManagementEventWatcher watcher = new ManagementEventWatcher(scope, new WqlEventQuery("SELECT * FROM Win32_ProcessStopTrace where ProcessId=" + processId));
               if (processId == -1)
               {
                   Debug.WriteLine(" process id is calling.Execute script again... ");
                   processId = Execute(remoteCommand);
                   Debug.WriteLine(" process id is calling.Execute script again... ");
               }
               if (processId == -1)
               {
                   Debug.WriteLine("Unable to run the command on remote machine");
                   return -1;
               }
              
               
               Trace.WriteLine(DateTime.Now + "\tWaiting for next stoptrace event to occur");
            
               watcher.Options.Timeout = new TimeSpan(0, 0, inMaxWaitTime); //Time out of 5 seconds
               int returnCodeInstall = 0;
                   try
                   {
                       ManagementBaseObject e = watcher.WaitForNextEvent();
                       if ((uint)e["ProcessId"] == processId)
                       {
                           Returncode = Int32.Parse(e["ExitStatus"].ToString());
                           returnCodeInstall = Returncode;
                           //Console.WriteLine("{0} Exit Code: {1}", e["ProcessName"], e["ExitStatus"]);
                           Trace.WriteLine(DateTime.Now + "\tprocess id matched with PID of the process event matcher.Pid:" + processId + " completed.Hence returning.");
                           watcher.Stop();
                           
                       }                      
                   }
                   catch (System.Management.ManagementException ex)
                   {
                       watcher.Stop();
                       return -1;
                       //We can ignore this error.This exception will occure whenever timeout of 5 seconds.
                   
                   }
                

               
               

               //Process has not completed in 3mins(TOTALITERATIONS*5 seconds)
                   fileList.Add("agentoutput.txt");
                   fileList.Add("\\windows\\temp\\connection.passphrase");
                   Detach(mappedDrive);

                   Thread.Sleep(5000);
                   DeleteFilesFrom(dirName, fileName, fileList, Remotetemppath, mappedDrive);

                   Detach(mappedDrive);

                   Thread.Sleep(5000);
                   //_returnCode = GetRemoteProcessReturnCode(processId, ref timedOut, inMaxWaitTime); //this is old code.
              
              
               return returnCodeInstall;

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
               Trace.WriteLine("Caught exception at Method: CopyandExecute" + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
               Trace.WriteLine("Exception  =  " + ex.Message);
               Trace.WriteLine("ERROR___________________________________________");
               return -1;
           }

       }

       internal int CopyAndExecuteInLocalPath(string fileName, string dirName, ArrayList fileList, int inMaxWaitTime)
       {


           string errorMessage = "";
           WmiCode errorCode = WmiCode.Unknown;

           bool timedOut = false;

           string remoteCommand = "";
           string mappedDrive = "";
           int processId = -1;

           try
           {
               Connect( errorMessage,  errorCode);


               errorCode = GetWmiCode;
               errorMessage = GetErrorString;

               if (!File.Exists(dirName + "\\" + fileName))
               {
                   Trace.WriteLine("RemoteWin:: CopyAndExecute: File: " + dirName + "\\" + fileName + " doesn't exist");
                   return -1;
               }

               bool copyStatus = CopyFilesToLocalPath(dirName, fileName, fileList, Remotetemppath);
               if (copyStatus == true)
               {

                   remoteCommand = Remotetemppath + "\\" + fileName + " > " + Remotetemppath + "\\" + Outputfile;

               }
               else
               {
                   Trace.WriteLine("\t RemoteWin: CopyAndExecute Could not copy file " + dirName + "\\" + fileName + " to remote machine ");
                   return -1;
               }


               Debug.WriteLine("Remote command to be executed is:" + remoteCommand);




               processId = Execute(remoteCommand);

               ManagementEventWatcher watcher = new ManagementEventWatcher(scope, new WqlEventQuery("SELECT * FROM Win32_ProcessStopTrace where ProcessId=" + processId));
               if (processId == -1)
               {
                   Debug.WriteLine(" process id is calling.Execute script again... ");
                   processId = Execute(remoteCommand);
                   Debug.WriteLine(" process id is calling.Execute script again... ");
               }
               if (processId == -1)
               {
                   Debug.WriteLine("Unable to run the command on remote machine");
                   return -1;
               }


               Trace.WriteLine(DateTime.Now + "\tWaiting for next stoptrace event to occur");
               watcher.Options.Timeout = new TimeSpan(0, 0, inMaxWaitTime); //Time out of 5 seconds

               try
               {
                   ManagementBaseObject e = watcher.WaitForNextEvent();
                   if ((uint)e["ProcessId"] == processId)
                   {
                       Returncode = Int32.Parse(e["ExitStatus"].ToString());
                       //Console.WriteLine("{0} Exit Code: {1}", e["ProcessName"], e["ExitStatus"]);
                       Trace.WriteLine(DateTime.Now + "\tprocess id matched with PID of the process event matcher.Pid:" + processId + " completed.Hence returning.");
                       watcher.Stop();

                   }

               }
               catch (System.Management.ManagementException ex)
               {
                   watcher.Stop();
                   return -1;
                   //We can ignore this error.This exception will occure whenever timeout of 5 seconds.

               }



               DeleteFilesFromLocalPath(fileName, fileList, Remotetemppath);

               //Process has not completed in 3mins(TOTALITERATIONS*5 seconds)



               //_returnCode = GetRemoteProcessReturnCode(processId, ref timedOut, inMaxWaitTime); //this is old code.

               //Detach(mappedDrive);

               Thread.Sleep(5000);
               return Returncode;
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


       private int GetRemoteProcessReturnCode( int pid, ref bool timedOut, int inMaxWaitTime)
        {
            Trace.WriteLine(DateTime.Now + "Entering GetRemoteProcessReturnCode");
            int returnCode = -1;
            //int waitTime = 0;
            //int sleepTime = 100;
            int TIME_OUT_MINS = 5;
            try
            {
                string errorMessage = "";
                WmiCode errorCode = WmiCode.Unknown;

                Connect( errorMessage,  errorCode);



                errorCode = GetWmiCode;
                errorMessage = GetErrorString;


                ManagementEventWatcher watcher = new ManagementEventWatcher(scope, new WqlEventQuery("SELECT * FROM Win32_ProcessStopTrace"));
            
                while (true)
                {
                    ManagementBaseObject e = watcher.WaitForNextEvent();
                    if ((uint)e["ProcessId"] == pid)
                    {
                        returnCode = Int32.Parse(e["ExitStatus"].ToString());
                        //Console.WriteLine("{0} Exit Code: {1}", e["ProcessName"], e["ExitStatus"]);
                        Trace.WriteLine(DateTime.Now + "process id matched with PID of the process event matcher.Pid:" + pid + " completed.Hence returning.");
                        
                        break;
                    }
                    Trace.WriteLine(DateTime.Now + "process id Didn't match with PID of the process event matcher.Waiting for " + pid + " to complete.");

                    /*    Thread.Sleep(sleepTime);

                
                        waitTime = waitTime + sleepTime;
                        Console.WriteLine("Waited {0} milli seconds\n", waitTime);
                        if (waitTime > inMaxWaitTime)
                        {
                            Debug.WriteLine("\t RemoteWin: CopyAndExecute: Could not install agent on to remote machine ");
                   
                            timedOut = true;
                    
                            return -1;
                        }
                        */


                }
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");

                return -1;
            }
            Trace.WriteLine(DateTime.Now + "Returng the process exit value from method:GetRemoteProcessReturnCode ");
            return returnCode;




        }

       internal WmiCode GetWmiCode
       {
           get
           {
               return referrorCode;
           }
           set
           {
               value = referrorCode;
           }
       }

       internal String GetErrorString
       {
           get
           {
               return refErrorMessage;
           }
           set
           {
               value = refErrorMessage;
           }
       }


       internal bool Connect(string errorMessage, WmiCode errorcode)
       {
           
           string hostConnectPath;



           if (connected)
           {
               referrorCode = errorcode;
               refErrorMessage = errorMessage;

               return true;
           }
           try
           {
               try
               {
                   if (remoteDomain.Length == 0)
                   {
                       if ( remoteHostName.Length != 0)
                       {
                           remoteHostName = remoteHostName.Split('.')[0];
                           if (remoteHostName.Length > 15)
                           {
                               remoteHostName = remoteHostName.Substring(0, 15);
                           }
                           remoteDomain = remoteHostName;
                           Trace.WriteLine(DateTime.Now + "\t  Domain name is null so we are assigning the hostname as domain to connect ");
                       }
                   }
               }
               catch (Exception e)
               {
                   Trace.WriteLine(DateTime.Now + " \t Failed in the windows share enabled " + e.Message);
               }


               ConnectionOptions connection = new ConnectionOptions();

               try
               {
                   if (remoteDomain.Trim().Length < 1)
                   { //Domain less local user credentials

                       connection.Authority = "ntlmdomain:localcomputer";
                   }
                   else
                   {
                       connection.Authority = "ntlmdomain:" + remoteDomain;
                   }
               }
               catch (Exception e)
               {
                   Trace.WriteLine(DateTime.Now + " \t Failed to check the domain in the connect function " + e.Message);
               }

               if (remoteIP != ".")
               {
                   connection.Username = remoteUserName;
                   connection.Password = remotePassWord;
               }
               AuthenticationLevel newobj = new AuthenticationLevel();
               newobj = AuthenticationLevel.PacketPrivacy;

               ImpersonationLevel newimersonate = new ImpersonationLevel();
               newimersonate = ImpersonationLevel.Impersonate;
               connection.Authentication = newobj;
               connection.Impersonation = newimersonate;
              
               hostConnectPath = "\\\\" + remoteIP + "\\root\\CIMV2";



               scope = new ManagementScope(
                 hostConnectPath, connection);

               scope.Connect();
             
            


           }
           catch (ManagementException err)
           {
               System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(err, true);

               Trace.WriteLine("ERROR*******************************************");
               Trace.WriteLine(DateTime.Now + "Caught exception at Method: RemoteWin Connect" + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
               Trace.WriteLine("Exception  =  " + err.Message);
               Trace.WriteLine("ERROR___________________________________________");
               Trace.WriteLine("An error occurred while trying to execute the WMI method: " + err.Message);

               scope = null;
               errorMessage = err.Message;
               errorcode = WmiCode.RpcError;

               referrorCode = errorcode;
               refErrorMessage = errorMessage;

               return false;

           }
           catch (System.UnauthorizedAccessException ex)
           {

               System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

               Trace.WriteLine("ERROR*******************************************");
               Trace.WriteLine("Caught exception at Method: Remotewin connect " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
               Trace.WriteLine("Exception  =  " + ex.Message);
               Trace.WriteLine("ERROR___________________________________________");


          
               scope = null;
               connected = false;
               errorMessage = ex.Message;
               errorcode = WmiCode.Credentialerror;
               referrorCode = errorcode;
               refErrorMessage = errorMessage;
               return false;

           }
           catch (System.Runtime.InteropServices.COMException ex)
           {

              

               System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

               Trace.WriteLine("ERROR*******************************************");
               Trace.WriteLine("Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
               Trace.WriteLine("Exception  =  " + ex.Message);
               Trace.WriteLine("ERROR___________________________________________");

               connected = false;
               errorMessage = ex.Message;
               errorcode = WmiCode.RpcError;
               referrorCode = errorcode;
               refErrorMessage = errorMessage;
               return false;
           }
           catch (Exception ex)
           {
               System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

               Trace.WriteLine("ERROR*******************************************");
               Trace.WriteLine("Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
               Trace.WriteLine("Exception  =  " + ex.Message);
               Trace.WriteLine("ERROR___________________________________________");

             
               scope = null;
               connected = false;
               errorMessage = ex.Message;
               errorcode = WmiCode.Unknown;
               referrorCode = errorcode;
               refErrorMessage = errorMessage;
               return false;


           }

           referrorCode = errorcode;
           refErrorMessage = errorMessage;
           connected = true;
           Trace.WriteLine(DateTime.Now + "\t Successfully connected to the remote machine ");
           return true;
       }




       internal static StringBuilder GetOutput(string mappedDrive)
       {


           string outputFilFullPath = mappedDrive + "\\" + Remotetempdir + "\\" + Outputfile;


           StringBuilder output = new StringBuilder();


           try
           {
               if (!File.Exists(outputFilFullPath))
               {
                   Trace.WriteLine("RemoteWin:GetOutput Output file : " + outputFilFullPath + " is not found");
                   return null;
               }
               using (StreamReader sr = File.OpenText(outputFilFullPath))
               {

                   String line;

                   while ((line = sr.ReadLine()) != null)
                   {
                       //Console.WriteLine(line);
                       output.AppendLine(line);
                   }
                   //Console.WriteLine ("The end of the stream has been reached.");

                   sr.Close();
               }
           }
           catch (Exception ex)
           {

               System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

               Trace.WriteLine("ERROR*******************************************");
               Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
               Trace.WriteLine("Exception  =  " + ex.Message);
               Trace.WriteLine("ERROR___________________________________________");
             

           }

           return output;

       }

       /*   Name:         CopyFileFrom
            Description:  Given a remote dir, remote filename, it copies one file to the local directory
        *               Step1:  On remote machine, copies file to c:\
        *                   Step2:  Maps remote machines C: to local temp drive
        *                  Step3:   Copies the file from local temp drive to the local directory
        *   Arguments:  inRemotedir:    Directory on remote machine
        *               inFileName:   File on remote machine
        *               inLocalDir:     Local directory to which file to be moved to
        *   Returns:    True/False
        *   
        *   Uses:       
        * 
        */

       internal StringBuilder GetOutputfromfile()
        {

            string  outputFile          = Outputfile;
            string outputPath = localTempDriveLetter;
            string  outputFilFullPath   = outputPath + outputFile;
           
            StringBuilder output = new StringBuilder();

            try
            {
                //Step1: Copy output file from remote machine's C:\\temp_output.txt to local C:\\
                if (CopyFileFrom("c:\\", Outputfile, "c:\\") == false)
                {
                    Trace.WriteLine("01_001_0011: Copying of output file " + Outputfile + " from " + remoteIP);
                }


                using (StreamReader sr = File.OpenText(outputFilFullPath))
                {

                    String line;

                    while ((line = sr.ReadLine()) != null)
                    {
                        //Console.WriteLine(line);
                        output.AppendLine(line);
                    }
                    //Console.WriteLine ("The end of the stream has been reached.");

                    sr.Close();
                }
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
               

            }
           
            return output;
            
        }

       /*   Name:         CopyFileFrom
            Description:  Given a remote dir, remote filename, it copies one file to the local directory
        *               Step1:  On remote machine, copies file to c:\
        *                   Step2:  Maps remote machines C: to local temp drive
        *                  Step3:   Copies the file from local temp drive to the local directory
        *   Arguments:  inRemotedir:    Directory on remote machine
        *               inFileName:   File on remote machine
        *               inLocalDir:     Local directory to which file to be moved to
        *   Returns:    True/False
        *   
        *   Uses:       
        * 
        */
       internal bool CopyFileFrom(string inRemoteDir, string inFileName, string inLocalDir)
        {
            try
            {

                Trace.WriteLine("\n Entered RemoteWin: CopyFileFrom:   TargetDir  = " + inRemoteDir + " Target File = " + inFileName + " Source Path" + inLocalDir);

                //If WMI connection is not established return.
                if (connected == false)
                {

                    Trace.WriteLine("01_001_013: Can't copy file. Not connected to the remote machine" + remoteIP);
                    return false;
                }

                //string localpath = "r:";

                //Step1: Copy from remote target directory to remote temp dir(c:)
                string targetPath = inRemoteDir + "\\" + inFileName;

                //commenting the backslash for the testing purpose XXXXXXXXXXX


                string remoteCopyCommand = "cmd /c  copy " + targetPath + " " + remoteDirToMount + "\\";

                Trace.WriteLine("\t RemoteWin: CopyFileFrom : \t Remote Copy Command = " + remoteCopyCommand);

                //  need check for return code, once Execute implements the monitor/wait step.
                // RIght now it just returns as soon as it executes
                int processId = Execute(remoteCopyCommand);

                Trace.WriteLine("Process id = " + processId.ToString() + " for command " + remoteCopyCommand);


                //Step2: Map the default remote temp dir(C:) of remote machine to a local temp drive
                //       First detach's any existing drives and then attaches.

                if (MapRemoteDirToDrive() == false)
                {
                    Trace.WriteLine("Can't map network drive c:\\ from " + remoteIP);
                    return false;
                }



                // Step3: Copy file from local mapped drive the the local destination dir
                string tempFile = localTempDriveLetter + "\\windows\\temp\\" + inFileName;
                string localFile = inLocalDir + "\\" + inFileName;
                try
                {
                    FileInfo file = new FileInfo(tempFile);

                    //Console.WriteLine("Copying file {0} to {1}", sourcePath, targetPath);
                    Trace.WriteLine("\tCopyFilesTo: \t Copying file " + localFile + "To " + inLocalDir);

                    file.CopyTo(localFile, true);
                    Detach(RemoteWin.Templocalmountdrive);
                }
                catch (Exception e)
                {
                    System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);

                    Trace.WriteLine("ERROR*******************************************");
                    Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                    Trace.WriteLine("Exception  =  " + e.Message);
                    Trace.WriteLine("ERROR___________________________________________");
                    Trace.WriteLine("\t 01_001_015: RemoteWin: CopyFileFrom: Problem occured while copying " + localFile + " To " + inLocalDir + "  \n" + e.Message);
                    //MessageBox.Show("\t 01_001_015: RemoteWin: CopyFileFrom: Problem occured while copying " + localFile + " To " + inLocalDir + "  \n" + e.Message);
                    return false;
                }


                //Step5: Disconnect the mapped drive

                //Detach(_localTempDriveLetter);

                Trace.WriteLine("\n Exiting: RemoteWin: CopyFilesTo:   ");
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;

            }

            return true;
        }

       internal bool CopyFileFromLocalPathSystem(string inRemoteDir, string inFileName, string inLocalDir)
       {
           try
           {

               Trace.WriteLine("\n Entered RemoteWin: CopyFileFromLocalPathSystem:   TargetDir  = " + inRemoteDir + " Target File = " + inFileName + " Source Path" + inLocalDir);

               //If WMI connection is not established return.
               if (connected == false)
               {

                   Trace.WriteLine("01_001_013: Can't copy file. Not connected to the remote machine" + remoteIP);
                   return false;
               }

               //string localpath = "r:";
               
               //Step1: Copy from remote target directory to remote temp dir(c:)
               string targetPath = inRemoteDir + "\\" + inFileName;
               Trace.WriteLine(DateTime.Now + " \t Printing the target path " + targetPath);
               //commenting the backslash for the testing purpose XXXXXXXXXXX


               string remoteCopyCommand = "cmd /c  copy " + targetPath + " " + remoteDirToMount + "\\";

               Trace.WriteLine("\t RemoteWin: CopyFileFrom : \t Remote Copy Command = " + remoteCopyCommand);

               //  need check for return code, once Execute implements the monitor/wait step.
               // RIght now it just returns as soon as it executes
               int processId = Execute(remoteCopyCommand);

               Trace.WriteLine("Process id = " + processId.ToString() + " for command " + remoteCopyCommand);


               //Step2: Map the default remote temp dir(C:) of remote machine to a local temp drive
               //       First detach's any existing drives and then attaches.

              /* if (mapRemoteDirToDrive() == false)
               {
                   Trace.WriteLine("Can't map network drive c:\\ from " + _remoteIP);
                   return false;
               }*/



               // Step3: Copy file from local mapped drive the the local destination dir
               string tempFile = inRemoteDir + "\\" + inFileName;
               string localFile = inLocalDir + "\\" + inFileName;
               try
               {
                   Trace.WriteLine(DateTime.Now + " \t Printing the tempFile path " + tempFile);
                   FileInfo file = new FileInfo(tempFile);

                   //Console.WriteLine("Copying file {0} to {1}", sourcePath, targetPath);
                   Trace.WriteLine("\tCopyFilesTo: \t Copying file " + localFile + "To " + inLocalDir);

                   file.CopyTo(localFile, true);
                   Trace.WriteLine(DateTime.Now + "\t File copied successfully  " + inFileName);

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
                   Trace.WriteLine(DateTime.Now + "Caught exception at Method: CopyFileFromLocalPathSystem" + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                   Trace.WriteLine("Exception  =  " + e.Message);
                   Trace.WriteLine("ERROR___________________________________________");
                   Trace.WriteLine("\t 01_001_015: RemoteWin: CopyFileFrom: Problem occured while copying " + localFile + " To " + inLocalDir + "  \n" + e.Message);
                   //MessageBox.Show("\t 01_001_015: RemoteWin: CopyFileFrom: Problem occured while copying " + localFile + " To " + inLocalDir + "  \n" + e.Message);
                   return false;
               }


               //Step5: Disconnect the mapped drive

               //Detach(_localTempDriveLetter);

               Trace.WriteLine("\n Exiting: RemoteWin: CopyFilesTo:   ");
           }
           catch (Exception ex)
           {

               System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

               Trace.WriteLine("ERROR*******************************************");
               Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
               Trace.WriteLine("Exception  =  " + ex.Message);
               Trace.WriteLine("ERROR___________________________________________");
               return false;

           }

           return true;
       }

       internal bool CopyFilesTo(string inLocalDir, string inFileName, ArrayList fileList, string inTargetPath, string mappedDrive)
       {

           try
           {
               Trace.WriteLine("\n Entered RemoteWin: CopyFilesTo:   SourceDir = " + inLocalDir + "Source File = " + inFileName + " Target Path " + inTargetPath);

               if (!File.Exists(inLocalDir + "\\" + inFileName))
               {
                   Trace.WriteLine("RemoteWin:: CopyFilesTo: File: " + inLocalDir + "\\" + inFileName + " doesn't exist");
                   return false;
               }


               ProcessStartInfo psi;

               if (scope == null)
               {

                   Trace.WriteLine("01_001_013: Can't copy file. Not connected to the remote machine" + remoteIP);
                   return false;
               }


               //string localpath = "r:";


               if (MapRemoteDirToDrive( mappedDrive) == false)
               {

                   Trace.WriteLine("RemoteWin:: CopyFilesTo: Couldn't map to local drive");
                   return false;

               }
               mappedDrive = GetMapNetworkdrive;
               // Step3: Copy file to the local mounted drive.

               if (inFileName != null)
               {
                   if (copyOneFile(inLocalDir, inFileName, mappedDrive, inTargetPath) == -1)
                   {
                       Trace.WriteLine(DateTime.Now + "\t CopyOneFile method failed. ");
                       return false;
                   }

               }

               if (fileList != null)
               {
                   Debug.WriteLine("Going through tht list of files");
                   foreach (string fileName in fileList)
                   {
                       Debug.WriteLine("Going through tht list of files: FILE name = " + fileName);
                       if (copyOneFile(inLocalDir, fileName, mappedDrive, inTargetPath) == -1)
                       {
                           
                           Trace.WriteLine(DateTime.Now + "\t CopyOneFile method failed. ");
                           return false;

                       }
                       Thread.Sleep(2000);
                   }

               }
               
               Trace.WriteLine("\n Exiting: RemoteWin: CopyFilesTo:   ");
           }
           catch (Exception ex)
           {

               System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

               Trace.WriteLine("ERROR*******************************************");
               Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
               Trace.WriteLine("Exception  =  " + ex.Message);
               Trace.WriteLine("ERROR___________________________________________");
               System.FormatException formatException = new FormatException("Inner exception", ex);
               Trace.WriteLine(formatException.GetBaseException());
               Trace.WriteLine(formatException.Data);
               Trace.WriteLine(formatException.InnerException);
               Trace.WriteLine(formatException.Message);

               return false;

           }
           refMapnetworkdrive = mappedDrive;
           return true;
       }

       // This will delete files which are copied as part of Push install through vContinuum.
       // It will connects c drive of remote machine as local drive and exceute delete file command.
       internal bool DeleteFilesFrom(string inLocalDir, string inFileName, ArrayList fileList, string inTargetPath, string mappedDrive)
       {

           try
           {
               Trace.WriteLine("\n Entered RemoteWin: DeleteFilesFrom:   " + " Target Path " + inTargetPath);




               ProcessStartInfo psi;

               if (scope == null)
               {

                   Trace.WriteLine("01_001_013: Can't copy file. Not connected to the remote machine" + remoteIP);
                   return false;
               }


               //string localpath = "r:";


               if (MapRemoteDirToDrive(mappedDrive) == false)
               {

                   Trace.WriteLine("RemoteWin:: CopyFilesTo: Couldn't map to local drive");
                   return false;

               }
               mappedDrive = GetMapNetworkdrive;
               // Step3: Copy file to the local mounted drive.

               if (inFileName != null)
               {
                   if (DeleteOneFile(inFileName, mappedDrive, inTargetPath) == -1)
                   {
                       Trace.WriteLine(DateTime.Now + "\t Delete method failed. ");
                       // return false;
                   }

               }

               if (fileList != null)
               {
                   Debug.WriteLine("Going through tht list of files");
                   foreach (string fileName in fileList)
                   {
                       Debug.WriteLine("Going through tht list of files: FILE name = " + fileName);
                       if (DeleteOneFile(fileName, mappedDrive, inTargetPath) == -1)
                       {

                           Trace.WriteLine(DateTime.Now + "\t Delete file method failed. ");
                           //return false;

                       }
                       Thread.Sleep(2000);
                   }

               }

               Trace.WriteLine("\n Exiting: RemoteWin: CopyFilesTo:   ");
           }
           catch (Exception ex)
           {

               System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

               Trace.WriteLine("ERROR*******************************************");
               Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
               Trace.WriteLine("Exception  =  " + ex.Message);
               Trace.WriteLine("ERROR___________________________________________");
               System.FormatException formatException = new FormatException("Inner exception", ex);
               Trace.WriteLine(formatException.GetBaseException());
               Trace.WriteLine(formatException.Data);
               Trace.WriteLine(formatException.InnerException);
               Trace.WriteLine(formatException.Message);

               return false;

           }
           refMapnetworkdrive = mappedDrive;
           return true;
       }

       internal bool CopyFilesToLocalPath(string inLocalDir, string inFileName, ArrayList fileList, string inTargetPath)
       {

           try
           {
               Trace.WriteLine("\n Entered RemoteWin: CopyFilesTo:   SourceDir = " + inLocalDir + "Source File = " + inFileName + " Target Path " + inTargetPath);

               if (!File.Exists(inLocalDir + "\\" + inFileName))
               {
                   Trace.WriteLine("RemoteWin:: CopyFilesTo: File: " + inLocalDir + "\\" + inFileName + " doesn't exist");
                   return false;
               }


               ProcessStartInfo psi;

               if (scope == null)
               {

                   Trace.WriteLine("01_001_013: Can't copy file. Not connected to the remote machine" + remoteIP);
                   return false;
               }


               //string localpath = "r:";


              /* if (mapRemoteDirToDrive(ref mappedDrive) == false)
               {

                   Trace.WriteLine("RemoteWin:: CopyFilesTo: Couldn't map to local drive");
                   return false;

               }*/

               // Step3: Copy file to the local mounted drive.

               if (inFileName != null)
               {
                   if (copyOneFile(inLocalDir, inFileName, @"c:", inTargetPath) == -1)
                   {
                       Trace.WriteLine(DateTime.Now + "\t CopyOneFile method failed. ");
                       return false;
                   }

               }

               if (fileList != null)
               {
                   Debug.WriteLine("Going through tht list of files");
                   foreach (string fileName in fileList)
                   {
                       Debug.WriteLine("Going through tht list of files: FILE name = " + fileName);
                       if (copyOneFile(inLocalDir, fileName, @"c:", inTargetPath) == -1)
                       {

                           Trace.WriteLine(DateTime.Now + "\t CopyOneFile method failed. ");
                           return false;

                       }
                       Thread.Sleep(2000);
                   }

               }
               Trace.WriteLine("\n Exiting: RemoteWin: CopyFilesTo:   ");
           }
           catch (Exception ex)
           {

               System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

               Trace.WriteLine("ERROR*******************************************");
               Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
               Trace.WriteLine("Exception  =  " + ex.Message);
               Trace.WriteLine("ERROR___________________________________________");
               System.FormatException formatException = new FormatException("Inner exception", ex);
               Trace.WriteLine(formatException.GetBaseException());
               Trace.WriteLine(formatException.Data);
               Trace.WriteLine(formatException.InnerException);
               Trace.WriteLine(formatException.Message);

               return false;

           }

           return true;
       }
       internal bool DeleteFilesFromLocalPath(string inFileName, ArrayList fileList, string inTargetPath)
       {

           try
           {
               Trace.WriteLine("\n Entered RemoteWin: DeleteFilesFromLocalPath:   SourceDir = " + inFileName + " Target Path " + inTargetPath);



               ProcessStartInfo psi;

               if (scope == null)
               {

                   Trace.WriteLine("01_001_013: Can't copy file. Not connected to the remote machine" + remoteIP);
                   return false;
               }


               //string localpath = "r:";


               /* if (mapRemoteDirToDrive(ref mappedDrive) == false)
                {

                    Trace.WriteLine("RemoteWin:: CopyFilesTo: Couldn't map to local drive");
                    return false;

                }*/

               // Step3: Copy file to the local mounted drive.

               if (inFileName != null)
               {
                   if (DeleteOneFile(inFileName, @"c:", inTargetPath) == -1)
                   {
                       Trace.WriteLine(DateTime.Now + "\t CopyOneFile method failed. ");
                       return false;
                   }

               }

               if (fileList != null)
               {
                   Debug.WriteLine("Going through tht list of files");
                   foreach (string fileName in fileList)
                   {
                       Debug.WriteLine("Going through tht list of files: FILE name = " + fileName);
                       if (DeleteOneFile(fileName, @"c:", inTargetPath) == -1)
                       {

                           Trace.WriteLine(DateTime.Now + "\t CopyOneFile method failed. ");
                           return false;

                       }
                       Thread.Sleep(2000);
                   }

               }
               Trace.WriteLine("\n Exiting: RemoteWin: CopyFilesTo:   ");
           }
           catch (Exception ex)
           {

               System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

               Trace.WriteLine("ERROR*******************************************");
               Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
               Trace.WriteLine("Exception  =  " + ex.Message);
               Trace.WriteLine("ERROR___________________________________________");
               System.FormatException formatException = new FormatException("Inner exception", ex);
               Trace.WriteLine(formatException.GetBaseException());
               Trace.WriteLine(formatException.Data);
               Trace.WriteLine(formatException.InnerException);
               Trace.WriteLine(formatException.Message);

               return false;

           }

           return true;
       }

       private int DeleteOneFile(string inFileName, string mappedDrive, string inTargetPath)
       {


           string targetPath = mappedDrive + "\\" + inFileName;

           try
           {
              
                   if (File.Exists(targetPath))
                   {
                       Trace.WriteLine(DateTime.Now + "\t file exists " + inTargetPath + "\t file name " + inFileName);
                   }
                   else
                   {
                       Trace.WriteLine(DateTime.Now + "\t File not exits " + inFileName);
                       return 0;
                   }
              

               FileInfo file = new FileInfo(targetPath);

               //Console.WriteLine("Copying file {0} to {1}", sourcePath, targetPath);
               Trace.WriteLine("\tDelete files : \t delete file " + targetPath);
               //File.Copy(sourcePath, targetPath, true);

               file.Delete();

           }
           catch (Exception e)
           {
               System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);

               Trace.WriteLine("ERROR*******************************************");
               Trace.WriteLine(DateTime.Now + "Caught exception at Method: Copying to r drive is failing copyOneFile" + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
               Trace.WriteLine("Exception  =  " + e.Message);
               Trace.WriteLine("ERROR___________________________________________");
               Trace.WriteLine("\t 01_001_010: RemoteWin: CopyFilesTo: Problem occured while deleting " + targetPath + "  \n" + e.Message + " Please make sure that \"Net Logon\" service is running on " + remoteIP);
               //MessageBox.Show("01_001_010: Problem occured while copying file to " +  _remoteIP + "  Please make sure that \n \n 1. Login information is corrent  \n 2. Net Logon service is running on " + _remoteIP);
               System.FormatException formatException = new FormatException("Inner exception", e);
               Trace.WriteLine(formatException.GetBaseException());
               Trace.WriteLine(formatException.Data);
               Trace.WriteLine(formatException.InnerException);
               Trace.WriteLine(formatException.Message);
               return -1;
           }





           //Step4: Copy from remote temp directory to remote target directory

           string remoteCopyCommand = "cmd /c  del /q " + remoteDirToMount + "\\" + inFileName + " " + inTargetPath + "\\" + inFileName;
           Trace.WriteLine("\t RemoteWin: CopyFilesTo: \t Remote Copy Command = " + remoteCopyCommand);
           Thread.Sleep(2000);
           //  need check for return code, once Execute implements the monitor/wait step.
           // RIght now it just returns as soon as it executes
           return Execute(remoteCopyCommand);


       }

       private int copyOneFile(string inLocalDir, string inFileName, string mappedDrive, string inTargetPath )
       {

           string sourcePath = inLocalDir + "\\" + inFileName;
           string targetPath = mappedDrive + "\\" + inFileName;

           try
           {
               FileInfo file = new FileInfo(sourcePath);

               //Console.WriteLine("Copying file {0} to {1}", sourcePath, targetPath);
               Debug.WriteLine("\tCopyFilesTo: \t Copying file " + sourcePath + "To " + targetPath);
               //File.Copy(sourcePath, targetPath, true);
               file.CopyTo(targetPath, true);

           }
           catch (Exception e)
           {
               System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);

               Trace.WriteLine("ERROR*******************************************");
               Trace.WriteLine(DateTime.Now + "Caught exception at Method: Copying to r drive is failing copyOneFile" + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
               Trace.WriteLine("Exception  =  " + e.Message);
               Trace.WriteLine("ERROR___________________________________________");
               Trace.WriteLine("\t 01_001_010: RemoteWin: CopyFilesTo: Problem occured while copying " + sourcePath + " To " + targetPath + "  \n" + e.Message + " Please make sure that \"Net Logon\" service is running on " + remoteIP);
               //MessageBox.Show("01_001_010: Problem occured while copying file to " +  _remoteIP + "  Please make sure that \n \n 1. Login information is corrent  \n 2. Net Logon service is running on " + _remoteIP);
               System.FormatException formatException = new FormatException("Inner exception", e);
               Trace.WriteLine(formatException.GetBaseException());
               Trace.WriteLine(formatException.Data);
               Trace.WriteLine(formatException.InnerException);
               Trace.WriteLine(formatException.Message);
               return -1 ;
           }





           //Step4: Copy from remote temp directory to remote target directory

           string remoteCopyCommand = "cmd /c  copy /y " + remoteDirToMount + "\\" + inFileName + " " + inTargetPath;
           Trace.WriteLine("\t RemoteWin: CopyFilesTo: \t Remote Copy Command = " + remoteCopyCommand);
           Thread.Sleep(2000);
           //  need check for return code, once Execute implements the monitor/wait step.
           // RIght now it just returns as soon as it executes
           return Execute(remoteCopyCommand);


       }



       //Maps the _remoteDirToMount(C:\) on the remote system to local TEMP_LOCAL_MOUNT_DRIVE(r:)
       internal bool MapRemoteDirToDrive(string drive)
       {
           try
           {

               string remoteTempMapPath;
               int returnCode;


               if (configInfo != null)
               {
                   Trace.WriteLine(DateTime.Now + "\t Config info value is not null");
                   if (configInfo["DriveLetterToUse"] != null)
                   {
                       localTempDriveLetter = configInfo["DriveLetterToUse"].ToString();
                   }
               }
               else
               {
                   localTempDriveLetter = GetLocalTempMountPoint();
               }

               // Send the drive letter as a return value in the referece string
               drive = localTempDriveLetter;

               //Execute("cmd /c  mkdir " + _remoteDirToMount );

               remoteTempMapPath = remoteDirToMount.Replace(":", "$");

               Debug.WriteLine("\tCopyFilesTo RemoteTempDirPath = " + remoteDirToMount + " RemoteTempMapPath = " + remoteTempMapPath);


               // Step1: Detach if an map is already present here

               Detach(localTempDriveLetter);


               //Step2:  Mount the remote directory to the local machine's drive 

               returnCode = Attach(remoteIP, remoteTempMapPath, localTempDriveLetter, remoteDomain, remoteUserName, remotePassWord);
               Debug.WriteLine("Return code of Attache drive command is " + returnCode);

               if (returnCode != 0)
               {
                   return false;
               }


               /*  Enahacenment to get a dynamic driver letter using * in net use command and parse for the
               *  drive used 
                      
               string netCommand = "use " + " * " + " " + "\\\\" + _remoteIP + "\\" + remoteTempPath + " " + "/user:" + _remoteUserName + "@" + _remoteDomain + " " + _remotePassWord;
               Console.WriteLine("Command = " + netCommand);
               Process myProcess = new Process();
               myProcess.StartInfo.FileName = "net.exe " + netCommand;
               myProcess.StartInfo.CreateNoWindow = true;

               myProcess.Start();
                        
               // Wait up to 30 seconds for the process to finish 
               if (!myProcess.WaitForExit(3000))
                   throw new Exception("Timed out reading from utility.");

               myProcess.Close(); 
             
               */
           }
           catch (Exception ex)
           {

               System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

               Trace.WriteLine("ERROR*******************************************");
               Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
               Trace.WriteLine("Exception  =  " + ex.Message);
               Trace.WriteLine("ERROR___________________________________________");
               System.FormatException formatException = new FormatException("Inner exception", ex);
               Trace.WriteLine(formatException.GetBaseException());
               Trace.WriteLine(formatException.Data);
               Trace.WriteLine(formatException.InnerException);
               Trace.WriteLine(formatException.Message);
               return false;

           }
           refMapnetworkdrive = drive;
           return true;

       }


       internal static string GetMapNetworkdrive
       {
           get
           {
               return refMapnetworkdrive;
           }
           set
           {
               value = refMapnetworkdrive;
           }
       }


       private bool MapRemoteDirToDrive()
        {
            try
            {
                string remoteTempMapPath;
                int returnCode;


                if (configInfo != null)
                {
                    if (configInfo["DriveLetterToUse"] != null)
                    {
                        localTempDriveLetter = configInfo["DriveLetterToUse"].ToString();
                    }
                }
                else
                {
                    localTempDriveLetter = GetLocalTempMountPoint();
                }


                //Execute("cmd /c  mkdir " + _remoteDirToMount );

                remoteTempMapPath = remoteDirToMount.Replace(":", "$");

                Debug.WriteLine("\tCopyFilesTo RemoteTempDirPath = " + remoteDirToMount + " RemoteTempMapPath = " + remoteTempMapPath);


                // Step1: Detach if an map is already present here

                Detach(localTempDriveLetter);



                //Step2:  Mount the remote directory to the local machine's drive



                returnCode = Attach(remoteIP, remoteTempMapPath, localTempDriveLetter, remoteDomain, remoteUserName, remotePassWord);
                Debug.WriteLine("Return code of Attache drive command is " + returnCode);

                if (returnCode != 0)
                {
                    Trace.WriteLine("01_001_017: Couldn't attach network share from " + remoteIP + " to local machine. Please check that Credentials provided are correct");
                    MessageBox.Show("01_001_017: Couldn't attach network share from " + remoteIP + " to local machine. \nPlease check that\n1.Credentials provided are correct \n2. \"NetLogon\" and \"Server\" services are running on remote machine.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return false;
                }


                /* Enahacenment to get a dynamic driver letter using * in net use command and parse for the
                *  drive used 
                      
                string netCommand = "use " + " * " + " " + "\\\\" + _remoteIP + "\\" + remoteTempPath + " " + "/user:" + _remoteUserName + "@" + _remoteDomain + " " + _remotePassWord;
                Console.WriteLine("Command = " + netCommand);
                Process myProcess = new Process();
                myProcess.StartInfo.FileName = "net.exe " + netCommand;
                myProcess.StartInfo.CreateNoWindow = true;

                myProcess.Start();
                        
                // Wait up to 30 seconds for the process to finish 
                if (!myProcess.WaitForExit(3000))
                    throw new Exception("Timed out reading from utility.");

                myProcess.Close(); 
             
                */
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;

            }

            return true;

        }

       internal int RemoteCopyFile(string inSrouceDir, string inFileName, string inTargetDir)
       {
           try
           {
               string remoteCopyCommand = "cmd /c  copy " + inSrouceDir + "\\" + inFileName + " " + inTargetDir;

               Debug.WriteLine("\t RemoteWin: CopyFilesTo: \t Remote Copy Command = " + remoteCopyCommand);

               Execute(remoteCopyCommand);
           }
           catch (Exception ex)
           {

               System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

               Trace.WriteLine("ERROR*******************************************");
               Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
               Trace.WriteLine("Exception  =  " + ex.Message);
               Trace.WriteLine("ERROR___________________________________________");
               return 1;

           }
           return 0;



       }

       internal bool EnableNetworkShare()
       {
           try
           {
               if (fileShareEnabledCheck == false)
               {
                   Execute("net.exe start server");
                   fileShareEnabledCheck = true;
               }
           }
           catch (Exception ex)
           {

               System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

               Trace.WriteLine("ERROR*******************************************");
               Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
               Trace.WriteLine("Exception  =  " + ex.Message);
               Trace.WriteLine("ERROR___________________________________________");
               return false;

           }
           return true;

       }
             
       internal static int Detach(  string inLocalDrive)
       {
           ProcessStartInfo psi;
           Process p = null ;
           int exitCode = -1;

           Trace.WriteLine(DateTime.Now + " \t Entering: RemoteWin:Detach. Deleting any existing mappings at " + inLocalDrive);

           try
           {
               
               string deleteCommand = " use /delete /y " + inLocalDrive;
               psi = new ProcessStartInfo("net.exe ", deleteCommand);
               psi.CreateNoWindow = true;
               psi.UseShellExecute = false;
               p = Process.Start(psi);

               //Wait for the process to end.
               p.WaitForExit();

               exitCode = p.ExitCode;
               Debug.WriteLine("Detach command was " + "net " + deleteCommand);
               Debug.WriteLine("Return code of Detach Network drive command is " + p.ExitCode);
           }
           catch (Exception ex)
           {
               System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

               Trace.WriteLine("ERROR*******************************************");
               Trace.WriteLine("Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
               Trace.WriteLine("Exception  =  " + ex.Message);
               Trace.WriteLine("ERROR___________________________________________");
              
               //If the drive is not used we might get exception. Ignore.
           }



           Trace.WriteLine(DateTime.Now + " \t Exiting: RemoteWin:Detach. Deleting any existing mappings at " + inLocalDrive);
           return exitCode;
               

       }

       internal int Attach(string inRemoteIP, string inRemotePath, string inLocalDrive, string inDomain, string inUserName, string inPassWord)
       {

           ProcessStartInfo psi;
           Trace.WriteLine(DateTime.Now + " \t Entered: RemoteWin:Attach");

           try
           {
               EnableNetworkShare();
               string netCommand = "use " + inLocalDrive + " " + "\\\\" + inRemoteIP + "\\" + inRemotePath + " " + "/user:" + inDomain + "\\" + inUserName + " " + inPassWord;
               string netCommandWithoutCredentials = "use " + inLocalDrive + " " + "\\\\" + remoteIP + "\\" + inRemotePath + " ";


               Debug.WriteLine("Executing Attach command :" + netCommand);

               //Try to attach the network share with a time out our 10000(10 secods)
               Trace.WriteLine(DateTime.Now + " \t Exiting: RemoteWin:Attach ");

               return WinTools.Execute("net.exe", netCommand, 20000);
           }
           catch (Exception ex)
           {
               System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

               Trace.WriteLine("ERROR*******************************************");
               Trace.WriteLine("Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
               Trace.WriteLine("Exception  =  " + ex.Message);
               Trace.WriteLine("ERROR___________________________________________");
               return -1;
           }             

       }




       public int Execute(string command)
        {
         

            int processId = -1;
            Trace.WriteLine(DateTime.Now + "\t Entered RemoteWin execute.Command to run is:" + command);

            //Redirect output to a temp file.
          //  command = command + " > " + DEFAULT_REMOTE_TEMPDIR_PATH + "\\" + OUTPUT_FILE;

          //  Debug.WriteLine("Executing Command *****************************************" + command);

            if ( command == string.Empty )
            {
                Trace.WriteLine("01_001_002: Execute: Command to execute is empty");

                Debug.WriteLine("Remote command is null.. ");
                //throw new ArgumentNullException("Command to execute is null");
            }
            try
            {
                ManagementClass classInstance =
                    new ManagementClass(scope,
                    new ManagementPath("Win32_Process"), null);

                // Obtain in-parameters for the method
                ManagementBaseObject inParams =
                classInstance.GetMethodParameters("Create");

                // Add the input parameters.
                inParams["CommandLine"] = command;

                // Execute the method and obtain the return values.
                ManagementBaseObject outParams =
                    classInstance.InvokeMethod("Create", inParams, null);

                // List outParams
                Trace.WriteLine(DateTime.Now  + "\t RemoteWin Execute: \t Out parameters:");
                Trace.WriteLine(DateTime.Now +  "\t RemoteWin Execute: \t ProcessId: " + outParams["ProcessId"]);
                Trace.WriteLine(DateTime.Now +  "\t RemoteWin Execute: \t ReturnValue: " + outParams["ReturnValue"]);
                Returncode = Convert.ToInt32(outParams["ReturnValue"]);
                //Return Process Id as a number
                if ( outParams["ProcessId"] != null )
                {

                    processId = Int32.Parse(outParams["ProcessId"].ToString());
                }


            }
            catch (ManagementException err)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(err, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + err.Message);
                Trace.WriteLine("ERROR___________________________________________");
                MessageBox.Show("An error occurred while trying to execute the WMI method: " + err.Message,"Remote connection");
            }
            catch (System.UnauthorizedAccessException unauthorizedErr)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(unauthorizedErr, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + unauthorizedErr.Message);
                Trace.WriteLine("ERROR___________________________________________");
                MessageBox.Show("Connection error (user name or password might be incorrect): " + unauthorizedErr.Message,"Remote connection");
            }
            catch ( Exception e)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + e.Message);
                Trace.WriteLine("ERROR___________________________________________");
                MessageBox.Show("An error occurred while trying to execute the WMI method " + e.Message,"Remote connection" );
            }

           
            return processId;
        }



       public StringBuilder ExecuteWithOutput(string command)
        {
            try
            {

                int processId;

                command = command + " > " + RemoteWin.Outputfilepath;


                //         command = " cmd /c del " + RemoteWin.OUTPUT_FILE_PATH + " & " + command + " > " + RemoteWin.OUTPUT_FILE_PATH;

                processId = Execute(command);

                if (processId < 0)
                {
                    Trace.WriteLine("01_001_016: RemoteWin:ExecuteWithOutput: Couldn't execute command: " + command + " on " + remoteIP);
                    return null;
                }

                while (ProcessIsRunning(processId))
                {
                    Debug.WriteLine("Process" + processId + " is still running");
                    Thread.Sleep(100);
                }
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
              

            }

            return GetOutputfromfile();
            
        }


       internal bool ProcessIsRunning(int processId)
        {
           
            Debug.WriteLine("Entering ProcessIsRunning...");

            int resultCount=0;

            if ( processId < 0 )
            {
                Trace.WriteLine("01_001_003: ProcessIsRunning: Invalid ProcessID passed.");
                throw new ArgumentNullException("ProcessID");
            }
            try
            {
                string queryString = "SELECT * FROM Win32_Process where ProcessId = " + processId;

                ObjectQuery query = new ObjectQuery(queryString);

                ManagementObjectSearcher searcher =
                    new ManagementObjectSearcher(scope,query);
                      

                foreach (ManagementObject queryObj in searcher.Get())
                {
                    Trace.WriteLine("-----------------------------------");
                    Trace.WriteLine("Win32_Process instance");
                    Trace.WriteLine("-----------------------------------");
                    Trace.WriteLine("ProcessId:" + queryObj["ProcessId"]);
                    Trace.WriteLine("CommandLine:" + queryObj["CommandLine"]);
                    resultCount++;
                }

                if (resultCount > 0)
                {
                    Debug.WriteLine("Exiting ProcessIsRunning...with value true");
                    return true;
                }
            }
            catch (Exception ex)
            {
               
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine("Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
            Debug.WriteLine("Exiting ProcessIsRunning...with value false");

            return false;
        }


       internal bool ProcessIsRunning2(uint processId)
        {
            try
            {

                int resultCount = 0;

                if (processId < 0)
                {
                    Trace.WriteLine("01_001_003: ProcessIsRunning: Invalid ProcessID passed.");
                    throw new ArgumentNullException("ProcessID");
                }
                try
                {
                    string queryString = "SELECT * FROM Win32_Process where ProcessId = " + processId;

                    ObjectQuery query = new ObjectQuery(queryString);

                    ManagementObjectSearcher searcher =
                        new ManagementObjectSearcher(scope, query);


                    foreach (ManagementObject queryObj in searcher.Get())
                    {
                        
                        Console.WriteLine("Win32_Process instance for current process");                        
                        Console.WriteLine("ProcessId of current process : {0}", queryObj["ProcessId"]);
                        Console.WriteLine("Printing in CommandLine console: {0}", queryObj["CommandLine"]);
                        resultCount++;
                    }

                    if (resultCount > 0)
                        return true;

                }
                catch (ManagementException e)
                {
                    Trace.WriteLine("RemoteWin: ProcessIsRunning2: An error occurred while querying for WMI data: " + e.Message);
                }
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;

            }
            return false;
        }




       internal bool GetSystemInfo(Hashtable sysInfoHash)
        {

            string wmiCaption="";
            string wmiSystemType = "";
            string osArchitecture;
            string OSedition="";

            try
            {

                ObjectQuery query = new ObjectQuery(
                    "SELECT * FROM Win32_ComputerSystem");

                ManagementObjectSearcher searcher =
                    new ManagementObjectSearcher(scope, query);

                Hashtable diskHash = new Hashtable();

                foreach (ManagementObject queryObj in searcher.Get())
                {
                                      
                    sysInfoHash.Add("HostName", queryObj["Name"]);
                    wmiSystemType = queryObj["SystemType"].ToString();
                    sysInfoHash.Add("cpucount", queryObj["NumberOfProcessors"]);
                    sysInfoHash.Add("memsize", queryObj["TotalPhysicalMemory"]);

                }


                 query = new ObjectQuery(
                 "SELECT * FROM Win32_OperatingSystem");

                 searcher =
                    new ManagementObjectSearcher(scope, query);

                foreach (ManagementObject queryObj in searcher.Get())
                {
                    
                    wmiCaption = queryObj["Caption"].ToString();

                    Trace.WriteLine(DateTime.Now + "\t Operating system " + wmiCaption);
                    Trace.WriteLine(DateTime.Now + "\t system drive " + queryObj["SystemDrive"].ToString());
                   
                }

                Debug.WriteLine(DateTime.Now + "\t Printing the wmi caption " + wmiCaption + "\t wmi system type " + wmiSystemType);
                osArchitecture = GetOSArchitectureFromWmiCaptionString(wmiCaption,wmiSystemType, OSedition);
                OSedition = GetOSediton;
                sysInfoHash.Add("OperatingSystem", osArchitecture);
                sysInfoHash.Add("osEdition", OSedition);


            }

            catch (ManagementException err)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(err, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + err.Message);
                Trace.WriteLine("ERROR___________________________________________");

                Trace.WriteLine("01_001_008: " + "An error occurred while querying for WMI data: " + err.Message);
               // MessageBox.Show("An error occurred while querying for WMI data: " + err.Message);
                return false;
            }
            catch (System.UnauthorizedAccessException unauthorizedErr)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(unauthorizedErr, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + unauthorizedErr.Message);
                Trace.WriteLine("ERROR___________________________________________");
                Trace.WriteLine("01_001_009: " + "An error occurred while querying for WMI data: " + unauthorizedErr.Message);
                //MessageBox.Show("Connection error (user name or password might be incorrect): " + unauthorizedErr.Message);
                return false;
            }

            refSysHash = sysInfoHash;
            return true;

        }



       internal Hashtable GetSysInfoHash
        {
           get
            {
                return refSysHash;
            }
            set
            {
                value = refSysHash;
            }
        }

       public static string GetOSArchitectureFromWmiCaptionString(string wmiOScaption, string wmiSystemType, string osedition)
        {



            string OSarchitecture = "Windows";
            try
            {
                if (wmiOScaption.Contains("2003"))
                {
                    OSarchitecture = OSarchitecture + "_2003";
                    if (wmiSystemType.Contains("x64"))
                    {
                        OSarchitecture = OSarchitecture + "_64";
                        if (wmiOScaption.Contains("Standard"))
                        {
                            osedition = "winNetStandard64Guest";
                        }
                        if (wmiOScaption.Contains("Enterprise"))
                        {
                            osedition = "winNetEnterprise64Guest";
                        }
                        if (wmiOScaption.Contains("Datacenter"))
                        {
                            osedition = "winNetDatacenter64Guest";
                        }
                        if (wmiOScaption.Contains("Web"))
                        {
                            osedition = "winNetWeb64Guest";
                        }
                        if (wmiOScaption.Contains("Small Business Server"))
                        {

                            osedition = "winNetBusiness64Guest";
                        }
                    }
                    else
                    {
                        OSarchitecture = OSarchitecture + "_32";
                        if (wmiOScaption.Contains("Standard"))
                        {
                            osedition = "winNetStandardGuest";
                        }
                        if (wmiOScaption.Contains("Enterprise"))
                        {

                            osedition = "winNetEnterpriseGuest";
                        }
                        if (wmiOScaption.Contains("Datacenter"))
                        {
                            osedition = "winNetDatacenterGuest";
                        }
                        if (wmiOScaption.Contains("Web"))
                        {
                            osedition = "winNetWebGuest";
                        }
                        if (wmiOScaption.Contains("Small Business Server"))
                        {

                            osedition = "winNetBusinessGuest";
                        }
                    }

                }
                else if (wmiOScaption.Contains("2008"))
                {
                    OSarchitecture = OSarchitecture + "_2008";

                    if (wmiOScaption.Contains("R2"))
                    {
                        OSarchitecture = OSarchitecture + "_R2";
                        osedition = "windows7Server64Guest";

                    }
                    else if (wmiSystemType.Contains("x64"))
                    {

                        OSarchitecture = OSarchitecture + "_64";
                        osedition = "winLonghorn64Guest";
                    }
                    else
                    {

                        OSarchitecture = OSarchitecture + "_32";
                        osedition = "winLonghornGuest";

                    }


                }
                else if (wmiOScaption.Contains("2012"))
                {
                    // As of now we are writting alt guest id of win2k8r2 becuase
                    // We don;t have guest id of win2k12 officially.
                    // We will update this once officially release
                    OSarchitecture = OSarchitecture + "_2012";
                    osedition = "windows7Server64Guest";
                }
                else if (wmiOScaption.ToUpper().Contains("XP"))
                {
                    OSarchitecture = OSarchitecture + "_XP";
                    if (wmiSystemType.Contains("x64"))
                    {
                        OSarchitecture = OSarchitecture + "_64";
                        osedition = "winXPPro64Guest";
                    }
                    else
                    {
                        OSarchitecture = OSarchitecture + "_32";
                        osedition = "winXPProGuest";
                    }
                }
                else if (wmiOScaption.Contains("7"))
                {
                    OSarchitecture = OSarchitecture + "_7";
                    if (wmiSystemType.Contains("x64"))
                    {
                        OSarchitecture = OSarchitecture + "_64";
                        osedition = "windows7_64Guest";
                    }
                    else
                    {
                        OSarchitecture = OSarchitecture + "_32";
                        osedition = "windows7Guest";
                    }
                }
                else if (wmiOScaption.Contains("8") && !wmiOScaption.Contains("2008"))
                {
                    OSarchitecture = OSarchitecture + "_8";
                    osedition = "windows8_64Guest";
                }
                else if (wmiOScaption.ToUpper().Contains("VISTA"))
                {
                    OSarchitecture = OSarchitecture + "_Vista";
                    if (wmiSystemType.Contains("x64"))
                    {
                        OSarchitecture = OSarchitecture + "_64";
                        osedition = "winVista64Guest";
                    }
                    else
                    {
                        OSarchitecture = OSarchitecture + "_32";
                        osedition = "winVistaGuest";
                    }
                }
                else
                {
                    OSarchitecture = OSarchitecture + "_2000";
                    osedition = "win2000advserv";
                }

            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");


            }

            refOSedition = osedition;
            return OSarchitecture;


        }

       public String GetOSediton
        {
            get
            {
                return refOSedition;
            }
            set
            {
                value = refOSedition;
            }
        }

       public static bool GetOSArchitectureFromWmiCaptionStringForLinux(string wmiOScaption, string wmiSystemType, string osedition)
        {
            if (wmiOScaption.Contains("Red Hat") && wmiOScaption.Contains("4"))
            {
                if (wmiSystemType.Contains("64"))
                {
                    osedition = "rhel4_64Guest";
                    refOSedition = osedition;
                    return true;
                }
                else
                {
                    osedition = "rhel4Guest";
                    refOSedition = osedition;
                    return true;
                }
            }
            if(wmiOScaption.Contains("Red Hat") && wmiOScaption.Contains("5"))
            {
                if (wmiSystemType.Contains("64"))
                {
                    osedition = "rhel5_64Guest";
                    refOSedition = osedition;
                    return true;
                }
                else
                {
                    osedition = "rhel5Guest";
                    refOSedition = osedition;
                    return true;
                }
            }
            if (wmiOScaption.Contains("Red Hat") && wmiOScaption.Contains("6"))
            {
                if (wmiSystemType.Contains("64"))
                {
                    osedition = "rhel6_64Guest";
                    refOSedition = osedition;
                    return true;
                }
                else
                {
                    osedition = "rhel6Guest";
                    refOSedition = osedition;
                    return true;
                }
            }
            if (wmiOScaption.Contains("Suse Linux") && wmiOScaption.Contains("11") || wmiOScaption.Contains("SUSE Linux") && wmiOScaption.Contains("11"))
            {
                if (wmiSystemType.Contains("64"))
                {
                    osedition = "sles11_64Guest";
                    refOSedition = osedition;
                    return true;
                }
                else
                {
                    osedition = "sles11Guest";
                    refOSedition = osedition;
                    return true;
                }
            }
            if (wmiOScaption.Contains("Suse Linux") && wmiOScaption.Contains("10") || wmiOScaption.Contains("SUSE Linux") && wmiOScaption.Contains("10"))
            {
                if (wmiSystemType.Contains("64"))
                {
                    osedition = "sles10_64Guest";
                    refOSedition = osedition;
                    return true;
                }
                else
                {
                    osedition = "sles10Guest";
                    refOSedition = osedition;
                    return true;
                }
            }
            if (wmiOScaption.Contains("Suse Linux") && wmiOScaption.Contains("9") || wmiOScaption.Contains("SUSE Linux") && wmiOScaption.Contains("9"))
            {
                if (wmiSystemType.Contains("64"))
                {
                    osedition = "sles64Guest";
                    refOSedition = osedition;
                    return true;
                }
                else
                {
                    osedition = "slesGuest";
                    refOSedition = osedition;
                    return true;
                }
            }

            if (wmiOScaption.ToUpper().Contains("CENTOS") && wmiSystemType.Contains("64"))
            {
                osedition = "centos64Guest";
                refOSedition = osedition;
                return true;
            }           
            else if (wmiOScaption.ToUpper().Contains("CENTOS"))
            {
                osedition = "centosGuest";
                refOSedition = osedition;
                return true;
            }

            if ((wmiOScaption.ToUpper().Contains("ENTERPRISE LINUX ENTERPRISE LINUX SERVER RELEASE") || wmiOScaption.ToUpper().Contains("ORACLE LINUX SERVER") || wmiOScaption.ToUpper().Contains("LINUX SERVER")) && wmiSystemType.Contains("64"))
            {
                osedition = "oracleLinux64Guest";
                refOSedition = osedition;
                return true;
            }
            else if (wmiOScaption.Contains("ENTERPRISE LINUX ENTERPRISE LINUX SERVER RELEASE") || wmiOScaption.ToUpper().Contains("ORACLE LINUX SERVER") || wmiOScaption.ToUpper().Contains("LINUX SERVER"))
            {
                osedition = "oracleLinuxGuest";
                refOSedition = osedition;
                return true;
            }

            if (wmiOScaption.ToUpper().Contains("UBUNTU") && wmiSystemType.Contains("64"))
            {
                osedition = "ubuntu64Guest";
                refOSedition = osedition;
                return true;
            }
            else if (wmiOScaption.ToUpper().Contains("UBUNTU"))
            {
                osedition = "ubuntuGuest";
                refOSedition = osedition;
                return true;
            }
            return true;
        }
       internal bool GetPartitionInfo(ArrayList diskInfo)
        {
            //Dictionary<string, int> d = new Dictionary<string, int>();
            
            if ( diskInfo == null )
            {
                Trace.WriteLine("01_001_005 GetParitionInfo: Invalid disk list passed");
                throw new ArgumentNullException("DiskList");
           
            }
            
            try
            {
                string queryString = "SELECT * FROM Win32_LogicalDisk";

                ObjectQuery query = new ObjectQuery(queryString);



                ManagementObjectSearcher searcher =
                    new ManagementObjectSearcher(scope, query);




                foreach (ManagementObject queryObj in searcher.Get())
                {
                    Hashtable diskHash = new Hashtable();


                    diskHash.Add("DeviceID", queryObj["DeviceID"]);
                    diskHash.Add("Size", queryObj["Size"]);
                    diskHash.Add("FreeSpace", queryObj["FreeSpace"]);
                    diskHash.Add("FileSystem", queryObj["FileSystem"]);
                    diskInfo.Add(diskHash);


                }
                refArrayList = diskInfo;
                return true;
            }
            catch (ManagementException e)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + e.Message);
                Trace.WriteLine("ERROR___________________________________________");
                //MessageBox.Show("An error occurred while querying for WMI data: " + e.Message);
            }
            refArrayList = diskInfo;
            return true;
        }


       public ArrayList DiskInfo
        {
            get
            {
                return refArrayList;
            }
           set
            {
                value = refArrayList;
            }
        }


        //public bool ServiceExists(string inServiceName, ref bool inStatus)
        //{

        //    try
        //    {
        //        ObjectQuery query = new ObjectQuery(
        //             "SELECT * FROM Win32_Service");

        //        ManagementObjectSearcher searcher =
        //            new ManagementObjectSearcher(scope, query);

        //        foreach (ManagementObject queryObj in searcher.Get())
        //        {


        //            if (queryObj["Name"].ToString() == inServiceName)
        //            {
        //                if (queryObj["Started"].ToString() == "True")
        //                {
        //                    inStatus = true;
        //                }
        //                else
        //                {

        //                    inStatus = false;
        //                }
        //                return true;
        //            }

        //        }

        //    }
        //    catch (ManagementException err)
        //    {
        //        System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(err, true);

        //        Trace.WriteLine("ERROR*******************************************");
        //        Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
        //        Trace.WriteLine("Exception  =  " + err.Message);
        //        Trace.WriteLine("ERROR___________________________________________");
        //        //MessageBox.Show("An error occurred while querying for WMI data: " + err.Message);
        //    }
        //    catch (System.UnauthorizedAccessException unauthorizedErr)
        //    {
        //        System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(unauthorizedErr, true);

        //        Trace.WriteLine("ERROR*******************************************");
        //        Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
        //        Trace.WriteLine("Exception  =  " + unauthorizedErr.Message);
        //        Trace.WriteLine("ERROR___________________________________________");
        //        //MessageBox.Show("Connection error (user name or password might be incorrect): " + unauthorizedErr.Message);
        //    }

        //    return false;
        //}


       public static bool CheckPort(string cxip, int port)
        {
            TcpClient tcpClient = new TcpClient();
            Trace.WriteLine(DateTime.Now + "\t Checkig ports for the ip " + cxip + " of port " + port);
            try
            {
                try
                {
                    tcpClient.Connect(cxip, port);
                    Trace.WriteLine(DateTime.Now + "\t Successfully connected to the cx using the port number " + port.ToString());
                    return true;
                }
                catch (Exception ex)
                {
                    Trace.WriteLine(DateTime.Now + "\t Failed to connect to cx using port " + port.ToString());
                    Trace.WriteLine(DateTime.Now + "\t Exception message " + ex.Message);
                    return false;
                }
                
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Exception message " + ex.Message);
            }
            return true;
        }

   
    }



}
