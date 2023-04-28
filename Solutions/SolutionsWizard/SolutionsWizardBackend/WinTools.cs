using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.IO;
using System.Net.NetworkInformation;
using System.Collections;
using System.Windows.Forms;
using System.Management;
using Microsoft.Win32;
using System.Web;
using System.Threading;
using System.Xml;
using System.Net;
using System.Security.AccessControl;

namespace Com.Inmage.Tools
{
   
    public class WinTools
    {//Error Codes Range: 01_006_

        public static string Output = "";

        public static bool Https = false;
        public static Host refHost = new Host();
        internal bool DeleteFile(string inFileName)           
        {
          File.Delete(inFileName);
          return true;
        }

        public static bool SetHostTimeinGMT(Host host)
        {
            // At the time of recovery we will call this method when user selects the user defined or specfic tag at time ...
            // We will send source time and this method will convert into the gmt....
            Trace.WriteLine(DateTime.Now + "\t\t Entered method for setting GMT time for " + host.hostname + " Display name is: " + host.displayname);
            if (host.timeZone == null || host.timeZone == "")
            {
                Trace.WriteLine(DateTime.Now + "\t\t Can't set GMT time for the " + host.hostname + " as timezone is null");
                return false;
                            
            }
           
            if (host.timeZone.StartsWith("-"))
            { 
                  int hours=int.Parse(host.timeZone.Substring(1,2));
                  int mins=int.Parse(host.timeZone.Substring(3,2));
                  TimeSpan timeSpan=new TimeSpan(hours,mins,0);
                DateTime datetime= DateTime.Parse(host.selectedTimeByUser);
                Trace.WriteLine("\t\t Time before Modification:" + host.selectedTimeByUser + timeSpan.ToString());
                datetime=datetime.Add(timeSpan);
                host.userDefinedTime = datetime.ToString("yyyy/MM/dd HH:mm:ss");
                Trace.WriteLine("\t\t Time after Modification:"+ host.userDefinedTime);
            
            }
            else if (host.timeZone.StartsWith("+"))
            {
                int hours = int.Parse(host.timeZone.Substring(1, 2));
                int mins = int.Parse(host.timeZone.Substring(3, 2));
                TimeSpan timeSpan = new TimeSpan(hours, mins, 0);
                DateTime dateTime = DateTime.Parse(host.selectedTimeByUser);
                Trace.WriteLine("\t\t Time before Modification:" + host.selectedTimeByUser);
                dateTime=dateTime.Subtract(timeSpan);
                host.userDefinedTime = dateTime.ToString("yyyy/MM/dd HH:mm:ss");
                Trace.WriteLine("\t\t Time after Modification:" + host.userDefinedTime);

            }
            else
            {
                Trace.WriteLine(DateTime.Now + "\t\t timezone value in host is wrong. Timezone should start with either + or -. But value in h is " + host.timeZone);
                return false;
            
            }

            refHost = host;
            return true;
        
        }


        public static bool SetHostTimeinitslocal(Host host)
        {
            // this is used to show the user that what he has selected for the recvoery when user clicks on the particular machine.
            Trace.WriteLine(DateTime.Now + "\t\t Entered method for setting GMT time for " + host.hostname + "Display name is:" + host.displayname);
            if (host.timeZone == null || host.timeZone == "")
            {
                Trace.WriteLine(DateTime.Now + "\t\t Can't set GMT time for the " + host.hostname + " as timezone is null");
                return false;

            }

            if (host.timeZone.StartsWith("-"))
            {
                int hours = int.Parse(host.timeZone.Substring(1, 2));
                int mins = int.Parse(host.timeZone.Substring(3, 2));
                TimeSpan timespan = new TimeSpan(hours, mins, 0);
                DateTime dateTime = DateTime.Parse(host.userDefinedTime);
                Trace.WriteLine("\t\t Time before Modification to make local converted (gmt) :" + host.userDefinedTime + timespan.ToString());
                dateTime = dateTime.Subtract(timespan);
                host.selectedTimeByUser = dateTime.ToString("yyyy/MM/dd HH:mm:ss");
                Trace.WriteLine("\t\t Time after Modification to make local user selected :" + host.selectedTimeByUser);

            }
            else if (host.timeZone.StartsWith("+"))
            {
                int hours = int.Parse(host.timeZone.Substring(1, 2));
                int mins = int.Parse(host.timeZone.Substring(3, 2));
                TimeSpan timeSpan = new TimeSpan(hours, mins, 0);
                DateTime dateTime = DateTime.Parse(host.userDefinedTime);
                Trace.WriteLine("\t\t Time before Modification to make local converted (gmt) :" + host.userDefinedTime);
                dateTime = dateTime.Add(timeSpan);
                host.selectedTimeByUser = dateTime.ToString("yyyy/MM/dd HH:mm:ss");
                Trace.WriteLine("\t\t Time after Modification to make local user selected :" + host.selectedTimeByUser);

            }
            else
            {
                Trace.WriteLine(DateTime.Now + "\t\t timezone value in host is wrong. Timezone should start with either + or -. But value in h is " + host.timeZone);
                return false;

            }

            refHost = host;
            return true;

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
        public static int ExpandFile(string source, string destination)
        {
           
            string cmdOptions = null;
            //cmdOptions = @"c:\Windows\system32";
            //cmdOptions = @"c:\temp" + "\"" + "\\" + DRIVER_FILE;
            //  string expandFileCommand = string.Format(DRIVERSCD_SCRIPT_COMMAND, function, source, fileName, destination);

            // cmdOptions = cmdOptions + " "; //+ expandFileCommand;
            //cmdOptions = string.Format(function, source, fileName, destination);
            string command = "expand.exe";
            try
            {
                cmdOptions = source + destination;
                Debug.WriteLine("Executing below command command " + command + " " + cmdOptions);
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
                
            }
              
            return Execute(command, cmdOptions, 60000);
        }


        public static Process ExecuteNoWait(string inCommand, string inOptions)
        {
           // Debug.WriteLine("1Execute: Command = " + inCommand + "Options = " + inOptions);
            // This will be called only when ui is not using the backgroundworker...
            ProcessStartInfo psi = new ProcessStartInfo(inCommand, inOptions);
            psi.CreateNoWindow = true;
            psi.UseShellExecute = false;

           Debug.WriteLine("Execute: Command = " + inCommand + "Options = " + inOptions);

            Process p = Process.Start(psi);



            return p;
        }


        public static int Execute(string inCommand, string inOptions, int inTimeOut)
        {

            //Trace.WriteLine(DateTime.Now + " \t Entered: Execute() of Wintools.cs");

            try
            {
                // This will execute the command in the process state.....

                

                //Debug.WriteLine("Execute: Command = " + inCommand + "Options = " + inOptions);

                ProcessStartInfo psi = new ProcessStartInfo(inCommand, inOptions); 
               
                psi.CreateNoWindow = true;
                psi.UseShellExecute = false;
                psi.RedirectStandardOutput = true;
                psi.WindowStyle = ProcessWindowStyle.Hidden;
              
               // Trace.WriteLine("Printing command " + inCommand + inOptions);
                Process p = Process.Start(psi);
                
                int waited = 0;

                //We will check 4 times before the total time 
                //int incWait = inTimeOut / 4;

                // Check every second
                int incWait = 1000;

                while (p.HasExited == false)
                {
                    if (waited <= inTimeOut)
                    {
                        Output += p.StandardOutput.ReadToEnd();
                        //Trace.WriteLine(DateTime.Now + " \t Output " + _output);
                        p.WaitForExit(incWait);
                        waited = waited + incWait;
                    }
                    else
                    {
                        if (p.HasExited == false)
                        {
                            //Trace.WriteLine(DateTime.Now+ "\t 01_006_001:  Process" + inCommand   + " has not completed in the given timeout period of" + inTimeOut);
                            // Console.WriteLine("01_006_001:  Process" + inCommand + inOptions + " has not completed in the given timeout period of" + inTimeOut);
                           // MessageBox.Show("01_006_001: " + inCommand + "   has timed out");
                            p.Kill();
                            return 1;
                        }
                    }
                }         

                Trace.WriteLine(DateTime.Now + " \t WinTools:Execute Completed in " + waited / 1000 + " seconds ExitCode = " + p.ExitCode);

                if (p.HasExited == true)
                {

                    Output += p.StandardOutput.ReadToEnd();
                    //Trace.WriteLine("Writing out put of command " + _output);
                }

                
                Trace.WriteLine(DateTime.Now + " \t Exiting: Execute() of Wintools.cs");
                return p.ExitCode;
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
                return 1;
            }
        }

        public static int Execute1(string inCommand, int inTimeOut)
        {

            
            try
            {
                ProcessStartInfo psi = new ProcessStartInfo("Perl.exe", "GetInfo.pl --version");

                psi.CreateNoWindow = true;
                psi.UseShellExecute = false;
                psi.RedirectStandardOutput = true;

                // Trace.WriteLine("Printing command " + inCommand + inOptions);
                Process p = Process.Start(psi);

                int waited = 0;

                //We will check 4 times before the total time 
                //int incWait = inTimeOut / 4;

                // Check every second
                int incWait = 1000;

                while (p.HasExited == false)
                {
                    if (waited <= inTimeOut)
                    {
                        Output += p.StandardOutput.ReadToEnd();
                        Trace.WriteLine(DateTime.Now + " \t Output " + Output);
                        p.WaitForExit(incWait);
                        waited = waited + incWait;
                    }
                    else
                    {
                        if (p.HasExited == false)
                        {
                            Trace.WriteLine(DateTime.Now + "\t 01_006_001:  Process" + inCommand + " has not completed in the given timeout period of" + inTimeOut);
                            // Console.WriteLine("01_006_001:  Process" + inCommand + inOptions + " has not completed in the given timeout period of" + inTimeOut);
                            // MessageBox.Show("01_006_001: " + inCommand + "   has timed out");
                            p.Kill();
                            return 1;
                        }
                    }
                }

                Trace.WriteLine(DateTime.Now + " \t WinTools:Execute Completed in " + waited / 1000 + " seconds ExitCode = " + p.ExitCode);

                if (p.HasExited == true)
                {



                    Output += p.StandardOutput.ReadToEnd();
                    Trace.WriteLine("Writing out put of command " + Output);
                }
                Trace.WriteLine(DateTime.Now + " \t Exiting: Execute() of Wintools.cs");
                return p.ExitCode;
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
                return 1;
            }
        }


        //using cxpsclient.exe we are uploading somefile

        public static int UploadFileToCX(string filename, string targetdirectory)
        {
            try
            {
                int inTimeOut = 60000;
                Trace.WriteLine(DateTime.Now + "\t entered to upload file to cx using cxpsclient.exe");
                string cxIP = WinTools.GetcxIP();
                string vxAgentPath = WinTools.Vxagentpath();
                ProcessStartInfo psi = new ProcessStartInfo(vxAgentPath + "\\cxpsclient");

                psi.CreateNoWindow = true;
                psi.UseShellExecute = false;
                psi.RedirectStandardOutput = true;
                psi.WindowStyle = ProcessWindowStyle.Hidden;
                psi.Arguments = "-i " + cxIP + "   --cscreds --put " + filename + " -d " + targetdirectory + " -C --secure -c " + "\"" + vxAgentPath + "\"" + "\\transport\\client.pem";


                Trace.WriteLine(DateTime.Now + "\t Printing the commands " + psi.Arguments.ToString());
                Process p = Process.Start(psi);

                int waited = 0;

                //We will check 4 times before the total time 
                //int incWait = inTimeOut / 4;

                // Check every second
                int incWait = 1000;

                while (p.HasExited == false)
                {
                    if (waited <= inTimeOut)
                    {
                        Output += p.StandardOutput.ReadToEnd();
                        //Trace.WriteLine(DateTime.Now + " \t Output " + _output);
                        p.WaitForExit(incWait);
                        waited = waited + incWait;
                    }
                    else
                    {
                        if (p.HasExited == false)
                        {
                            //Trace.WriteLine(DateTime.Now+ "\t 01_006_001:  Process" + inCommand   + " has not completed in the given timeout period of" + inTimeOut);
                            // Console.WriteLine("01_006_001:  Process" + inCommand + inOptions + " has not completed in the given timeout period of" + inTimeOut);
                            // MessageBox.Show("01_006_001: " + inCommand + "   has timed out");
                            p.Kill();
                            return 1;
                        }
                    }
                }

                Trace.WriteLine(DateTime.Now + " \t WinTools:uploadtocx()  Completed in " + waited / 1000 + " seconds ExitCode = " + p.ExitCode);

                if (p.HasExited == true)
                {

                    Output += p.StandardOutput.ReadToEnd();

                    Trace.WriteLine("Writing out put of command " + Output);
                }


                Trace.WriteLine(DateTime.Now + " \t Exiting: uploadtocx() of Wintools.cs");
                return p.ExitCode;
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
                return 1;
            }
        }




        public static int DownloadFileToCX(string filename, string targetdirectory)
        {
            try
            {
                int inTimeOut = 60000;
                Trace.WriteLine(DateTime.Now + "\t entered to download file to cx using cxpsclient.exe");
                string cxIP = WinTools.GetcxIP();

                string vxAgentPath = WinTools.Vxagentpath();

                ProcessStartInfo psi = new ProcessStartInfo(vxAgentPath + "\\cxpsclient");

                psi.CreateNoWindow = true;
                psi.UseShellExecute = false;
                psi.RedirectStandardOutput = true;
                psi.WindowStyle = ProcessWindowStyle.Hidden;
                psi.Arguments = "-i " + cxIP + "  --cscreds --get " + filename + " -L " + targetdirectory + " -C --secure -c " + "\"" +vxAgentPath + "\"" + "\\transport\\client.pem";


                Trace.WriteLine(DateTime.Now + "\t Printing the commands " + psi.Arguments.ToString());
                Process p = Process.Start(psi);

                int waited = 0;

                //We will check 4 times before the total time 
                //int incWait = inTimeOut / 4;

                // Check every second
                int incWait = 1000;

                while (p.HasExited == false)
                {
                    if (waited <= inTimeOut)
                    {
                        Output += p.StandardOutput.ReadToEnd();
                        //Trace.WriteLine(DateTime.Now + " \t Output " + _output);
                        p.WaitForExit(incWait);
                        waited = waited + incWait;
                    }
                    else
                    {
                        if (p.HasExited == false)
                        {
                            //Trace.WriteLine(DateTime.Now+ "\t 01_006_001:  Process" + inCommand   + " has not completed in the given timeout period of" + inTimeOut);
                            // Console.WriteLine("01_006_001:  Process" + inCommand + inOptions + " has not completed in the given timeout period of" + inTimeOut);
                            // MessageBox.Show("01_006_001: " + inCommand + "   has timed out");
                            p.Kill();
                            return 1;
                        }
                    }
                }

                Trace.WriteLine(DateTime.Now + " \t WinTools: download Completed in " + waited / 1000 + " seconds ExitCode = " + p.ExitCode);

                if (p.HasExited == true)
                {

                    Output += p.StandardOutput.ReadToEnd();

                    Trace.WriteLine("Writing out put of command " + Output);
                }


                Trace.WriteLine(DateTime.Now + " \t Exiting: download() of Wintools.cs");
                return p.ExitCode;
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
                return 1;
            }
        }
        public static int ExecuteEsxUtilWinLocally(int inTimeOut, string role, string directory, string logpath)
        {

            //Trace.WriteLine(DateTime.Now + " \t Entered: Execute() of Wintools.cs");

            try
            {
                // This will execute the command in the process state.....

                

               //From now onwards 
                ProcessStartInfo psi = new ProcessStartInfo(WinTools.FxAgentPath() + "\\EsxUtilWin.exe");

                psi.CreateNoWindow = true;
                psi.UseShellExecute = false;
                psi.RedirectStandardOutput = true;
                psi.WindowStyle = ProcessWindowStyle.Hidden;
                psi.Arguments = "-role " + role + " -d " + directory;
                Trace.WriteLine(DateTime.Now + "\t Printing the commands " + psi.Arguments.ToString());
                Process p = Process.Start(psi);
                
                int waited = 0;

                //We will check 4 times before the total time 
                //int incWait = inTimeOut / 4;

                // Check every second
                int incWait = 1000;

                while (p.HasExited == false)
                {
                    if (waited <= inTimeOut)
                    {
                        Output += p.StandardOutput.ReadToEnd();
                        //Trace.WriteLine(DateTime.Now + " \t Output " + _output);
                        p.WaitForExit(incWait);
                        waited = waited + incWait;
                    }
                    else
                    {
                        if (p.HasExited == false)
                        {
                            //Trace.WriteLine(DateTime.Now+ "\t 01_006_001:  Process" + inCommand   + " has not completed in the given timeout period of" + inTimeOut);
                            // Console.WriteLine("01_006_001:  Process" + inCommand + inOptions + " has not completed in the given timeout period of" + inTimeOut);
                            // MessageBox.Show("01_006_001: " + inCommand + "   has timed out");
                            p.Kill();
                            return 1;
                        }
                    }
                }

                Trace.WriteLine(DateTime.Now + " \t WinTools:ExecuteEsxUtilWinLocally Completed in " + waited / 1000 + " seconds ExitCode = " + p.ExitCode);

                if (p.HasExited == true)
                {

                    Output += p.StandardOutput.ReadToEnd();
                    FileInfo f1 = new FileInfo(logpath);
                    StreamWriter sw = f1.AppendText();

                    sw.Write(Output);
                    sw.Close();
                    sw.Dispose();
                    //Trace.WriteLine("Writing out put of command " + _output);
                }


                Trace.WriteLine(DateTime.Now + " \t Exiting: ExecuteEsxUtilWinLocally() of Wintools.cs");
                return p.ExitCode;
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
                return 1;
            }
        }

        public static int ExecuteRescueusb(int inTimeOut)
        {

            Trace.WriteLine(DateTime.Now + " \t Entered: ExecuteRescueUSB() of Wintools.cs");

            try
            {
                // This will execute the command in the process state.....

              
                Trace.WriteLine(DateTime.Now + "\t Current Directory " + Directory.GetCurrentDirectory().ToString());

                //From now onwards 
                ProcessStartInfo psi = new ProcessStartInfo(WinTools.FxAgentPath() + "\\vContinuum\\RescueUSB.exe");

                psi.CreateNoWindow = true;
                psi.UseShellExecute = false;
                psi.RedirectStandardOutput = true;
                psi.WindowStyle = ProcessWindowStyle.Hidden;
                psi.WorkingDirectory = WinTools.FxAgentPath() + "\\vContinuum";
                psi.Arguments = "--app vCon --logfilepath " + "\"" + WinTools.FxAgentPath() + "\\vContinuum\\logs\\RescueUSB.log" + "\"" ;
                //Trace.WriteLine(DateTime.Now + "\t Printing the commands " + psi.Arguments.ToString());
                Process p = Process.Start(psi);

                int waited = 0;

                //We will check 4 times before the total time 
                //int incWait = inTimeOut / 4;

                // Check every second
                int incWait = 1000;

                while (p.HasExited == false)
                {
                    if (waited <= inTimeOut)
                    {
                        Output += p.StandardOutput.ReadToEnd();
                        //Trace.WriteLine(DateTime.Now + " \t Output " + _output);
                        p.WaitForExit(incWait);
                        waited = waited + incWait;
                    }
                    else
                    {
                        if (p.HasExited == false)
                        {
                            //Trace.WriteLine(DateTime.Now+ "\t 01_006_001:  Process" + inCommand   + " has not completed in the given timeout period of" + inTimeOut);
                            // Console.WriteLine("01_006_001:  Process" + inCommand + inOptions + " has not completed in the given timeout period of" + inTimeOut);
                            // MessageBox.Show("01_006_001: " + inCommand + "   has timed out");
                            p.Kill();
                            return 1;
                        }
                    }
                }

                Trace.WriteLine(DateTime.Now + " \t WinTools:ExecuteRescueUSB Completed in " + waited / 1000 + " seconds ExitCode = " + p.ExitCode);

                if (p.HasExited == true)
                {

                    
                    
                }


                Trace.WriteLine(DateTime.Now + " \t Exiting: ExecuteRescueUSB() of Wintools.cs");
                return p.ExitCode;
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
                return 1;
            }
        }

        public static int DRdrillLocally(int inTimeOut, string directory, string logpath)
        {

            //Trace.WriteLine(DateTime.Now + " \t Entered: Execute() of Wintools.cs");

            try
            {
                // This will execute the command in the process state.....




                ProcessStartInfo psi = new ProcessStartInfo(WinTools.FxAgentPath() + "\\consistency\\VCon_Physical_Snapshot.bat");

                psi.CreateNoWindow = true;
                psi.UseShellExecute = false;
                psi.RedirectStandardOutput = true;
                psi.WindowStyle = ProcessWindowStyle.Hidden;
                psi.Arguments =  directory;
                Trace.WriteLine(DateTime.Now + "\t Printing the commands " + psi.Arguments.ToString());
                Process p = Process.Start(psi);

                int waited = 0;

                //We will check 4 times before the total time 
                //int incWait = inTimeOut / 4;

                // Check every second
                int incWait = 1000;

                while (p.HasExited == false)
                {
                    if (waited <= inTimeOut)
                    {
                        Output += p.StandardOutput.ReadToEnd();
                        //Trace.WriteLine(DateTime.Now + " \t Output " + _output);
                        p.WaitForExit(incWait);
                        waited = waited + incWait;
                    }
                    else
                    {
                        if (p.HasExited == false)
                        {
                            //Trace.WriteLine(DateTime.Now+ "\t 01_006_001:  Process" + inCommand   + " has not completed in the given timeout period of" + inTimeOut);
                            // Console.WriteLine("01_006_001:  Process" + inCommand + inOptions + " has not completed in the given timeout period of" + inTimeOut);
                            // MessageBox.Show("01_006_001: " + inCommand + "   has timed out");
                            p.Kill();
                            return 1;
                        }
                    }
                }

                Trace.WriteLine(DateTime.Now + " \t WinTools:DrDrillLocally Completed in " + waited / 1000 + " seconds ExitCode = " + p.ExitCode);

                if (p.HasExited == true)
                {

                    Output += p.StandardOutput.ReadToEnd();
                    Trace.WriteLine(DateTime.Now + "\t Printing the log path " + logpath);
                    FileInfo f1 = new FileInfo(logpath);
                    StreamWriter sw = f1.AppendText();

                    sw.Write(Output);
                    sw.Close();
                    sw.Dispose();
                    //Trace.WriteLine("Writing out put of command " + _output);
                }


                Trace.WriteLine(DateTime.Now + " \t Exiting: DrDrillLocally() of Wintools.cs");
                return p.ExitCode;
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
                return 1;
            }
        }
        public static int RecoveryLocally(int inTimeOut, string directory, string recoverylogpath, string cxPath)
        {

            //Trace.WriteLine(DateTime.Now + " \t Entered: Execute() of Wintools.cs");

            try
            {
                // This will execute the command in the process state.....




                ProcessStartInfo psi = new ProcessStartInfo(WinTools.FxAgentPath() + "\\Esxutil.exe");

                psi.CreateNoWindow = true;
                psi.UseShellExecute = false;
                psi.RedirectStandardOutput = true;
                psi.WindowStyle = ProcessWindowStyle.Hidden;
                psi.Arguments = "-rollback -d " + directory + " -cxpath " + " /home/svsystems/vcon/" + cxPath;
                Trace.WriteLine(DateTime.Now + "\t Printing the commands " + psi.Arguments.ToString());
                Process p = Process.Start(psi);

                int waited = 0;

                //We will check 4 times before the total time 
                //int incWait = inTimeOut / 4;

                // Check every second
                int incWait = 1000;

                while (p.HasExited == false)
                {
                    if (waited <= inTimeOut)
                    {
                        Output += p.StandardOutput.ReadToEnd();
                        //Trace.WriteLine(DateTime.Now + " \t Output " + _output);
                        p.WaitForExit(incWait);
                        waited = waited + incWait;
                    }
                    else
                    {
                        if (p.HasExited == false)
                        {
                            //Trace.WriteLine(DateTime.Now+ "\t 01_006_001:  Process" + inCommand   + " has not completed in the given timeout period of" + inTimeOut);
                            // Console.WriteLine("01_006_001:  Process" + inCommand + inOptions + " has not completed in the given timeout period of" + inTimeOut);
                            // MessageBox.Show("01_006_001: " + inCommand + "   has timed out");
                            p.Kill();
                            return 1;
                        }
                    }
                }

                Trace.WriteLine(DateTime.Now + " \t WinTools:Execute Completed in " + waited / 1000 + " seconds ExitCode = " + p.ExitCode);

                if (p.HasExited == true)
                {

                    Output += p.StandardOutput.ReadToEnd();
                    Trace.WriteLine(DateTime.Now + "\t Printing the log path " + recoverylogpath);
                    FileInfo f1 = new FileInfo(recoverylogpath);
                    StreamWriter sw = f1.AppendText();

                    sw.Write(Output);
                    sw.Close();
                    sw.Dispose();
                 
                    //Trace.WriteLine("Writing out put of command " + _output);
                }


                Trace.WriteLine(DateTime.Now + " \t Exiting: Execute() of Wintools.cs");
                return p.ExitCode;
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
                return 1;
            }
        }
        public static bool ServiceExist(string inserviceName)
        {

            // With this we can check any service that exists in the remote machine...
            // As of now we are not using this method. 
           // MessageBox.Show("came here");
            try
            {
                ManagementObjectSearcher searcher =
                    new ManagementObjectSearcher("root\\CIMV2",
                    "SELECT * FROM Win32_Service");

                foreach (ManagementObject queryObj in searcher.Get())
                {
                   // MessageBox.Show(queryObj["Name"].ToString());
                    if (queryObj["Name"].ToString().ToUpper() == inserviceName.ToString().ToUpper())
                    {
                        Debug.WriteLine(queryObj["Name"].ToString());
                        return true;

                    }
                
                }
                return false;
            }
            catch (ManagementException e)
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
                Trace.WriteLine("An error occurred while querying for WMI data: " + e.Message);
                return false;
            }



            return false;
        
        
        
        }



        public static String GetvContinuumguuid()
        {
            // With this we will get the fx path in the vCon machine..
            // Just we are checking the regedit...(Registry entry)....
            Object obp;
            RegistryKey hklm = Registry.LocalMachine;
            RegistryKey hklm32 = Registry.LocalMachine;

            try
            {

                hklm = hklm.OpenSubKey("SOFTWARE\\Wow6432Node\\SV Systems");
                hklm32 = hklm32.OpenSubKey("SOFTWARE\\SV Systems");

                if (hklm == null && hklm32 == null)
                {
                    return null;
                }
                else
                {

                    if (hklm != null)
                    {
                        obp = hklm.GetValue("HostId");

                    }
                    else
                    {
                        obp = hklm32.GetValue("HostId");
                    }


                    String sp_dir = obp.ToString();
                    return sp_dir;
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
                return null;
            }

        }



        /*
         * This methods gives output true or false.
         * False means Wizard needs to use http.
         * True means wizard need to use https.
         * As part of first step this method will read regisrty of sv system and get he valeu of https
         * If exists will continue to read the registry. 
         */
        public static bool GetHttpsOrHttp()
        { 
            bool httpsResult = false;
           
               // With this we will get the https entry..
            // Just we are checking the regedit...(Registry entry)....
            Object obp;
            RegistryKey hklm = Registry.LocalMachine;
            RegistryKey hklm32 = Registry.LocalMachine;

            try
            {

                hklm = hklm.OpenSubKey("SOFTWARE\\Wow6432Node\\SV Systems");
                hklm32 = hklm32.OpenSubKey("SOFTWARE\\SV Systems");

                if (hklm == null && hklm32 == null)
                {
                    return false;
                }
                else
                {

                    if (hklm != null)
                    {
                        obp = hklm.GetValue("Https");

                    }
                    else
                    {
                        obp = hklm32.GetValue("Https");
                    }

                    if (obp != null)
                    {
                        String sp_dir = obp.ToString();

                        if (sp_dir == "0")
                        {
                            httpsResult = false;
                            Trace.WriteLine(DateTime.Now + "\t Https is disabled");
                        }
                        else if (sp_dir == "1")
                        {
                            httpsResult = true;
                            Trace.WriteLine(DateTime.Now + "\t Https is enabled");
                        }
                    }
                    else
                    {
                        Trace.WriteLine(DateTime.Now + "\t Registry value for https is not found");
                    }                    
                }

            }
            catch (Exception except)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(except, true);
                System.FormatException formatException = new FormatException("Inner exception", except);
                Trace.WriteLine(formatException.GetBaseException());
                Trace.WriteLine(formatException.Data);
                Trace.WriteLine(formatException.InnerException);
                Trace.WriteLine(formatException.Message);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine("Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + except.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return httpsResult;
            }
            if (httpsResult == false)
            {
                Trace.WriteLine(DateTime.Now + "\t It seems http value not found in registry or it may be diaabled state");
            }
            return httpsResult;
        }


        public static String Vxagentpath()
        {
            // With this we will get the fx path in the vCon machine..
            // Just we are checking the regedit...(Registry entry)....
            Object obp;
            RegistryKey hklm = Registry.LocalMachine;
            RegistryKey hklm32 = Registry.LocalMachine;

            try
            {

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


                    String sp_dir = obp.ToString();
                    return sp_dir;
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
                return null;
            }

        }

        public static String FxAgentPath()
        {
            // With this we will get the fx path in the vCon machine..
            // Just we are checking the regedit...(Registry entry)....
            Object obp;
            RegistryKey hklm = Registry.LocalMachine;
            RegistryKey hklm32 = Registry.LocalMachine;

            try
            {

                hklm = hklm.OpenSubKey("SOFTWARE\\Wow6432Node\\SV Systems\\FileReplicationAgent");
                hklm32 = hklm32.OpenSubKey("SOFTWARE\\SV Systems\\FileReplicationAgent");

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


                    String sp_dir = obp.ToString();
                    return sp_dir;
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
                return null;
            }

        }

        public static bool UpdatecxIPandPortNumber(string incxIP, string incxport)
        {

            // User can change the cx ip and port number in the cx using the get plans buttons.
            // we willchange the cx ip and port number in the regedit....
            Object obp;
            RegistryKey hklm = Registry.LocalMachine;
            RegistryKey hklm32 = Registry.LocalMachine;

            try
            {

                hklm = hklm.OpenSubKey("SOFTWARE\\Wow6432Node\\SV Systems",true);
                hklm32 = hklm32.OpenSubKey("SOFTWARE\\SV Systems",true);

                if (hklm == null && hklm32 == null)
                {
                    return false;
                }
                else
                {

                    if (hklm != null)
                    {
                        Trace.WriteLine(hklm.GetValue("ServerName"));
                        hklm.SetValue("ServerName", incxIP);
                        hklm.SetValue("ServerHttpPort", incxport,RegistryValueKind.DWord);    
                        
                    }
                    else
                    {
                        Trace.WriteLine(hklm32.GetValue("ServerName"));
                        hklm32.SetValue("ServerName", incxIP);
                        hklm32.SetValue("ServerHttpPort", incxport,RegistryValueKind.DWord);

                       
                    }



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
                return false;
            }


            return true;
        }

        public static String GetcxIP()
        {

            // With this we will read cx ip in the registry configuration....
            Object obp;
            RegistryKey hklm = Registry.LocalMachine;
            RegistryKey hklm32 = Registry.LocalMachine;

            hklm = hklm.OpenSubKey("SOFTWARE\\Wow6432Node\\SV Systems");
            hklm32 = hklm32.OpenSubKey("SOFTWARE\\SV Systems");
            try
            {
                if (hklm == null && hklm32 == null)
                {
                    return null;
                }
                else
                {

                    if (hklm != null)
                    {
                        obp = hklm.GetValue("ServerName");
                    }
                    else
                    {
                        obp = hklm32.GetValue("ServerName");
                    }


                    String sp_dir = obp.ToString();
                    return sp_dir;


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
                return null;

            }


        }
        public static String GetcxPortNumber()
        {
            try
            {// With this we will read cx portnumebr in the registry configuration....
                Object obp;
                String portNumber = "";
                RegistryKey hklm = Registry.LocalMachine;
                RegistryKey hklm32 = Registry.LocalMachine;

                hklm = hklm.OpenSubKey("SOFTWARE\\Wow6432Node\\SV Systems");
                hklm32 = hklm32.OpenSubKey("SOFTWARE\\SV Systems");

                if (hklm == null && hklm32 == null)
                {
                    return null;
                }
                else
                {

                    if (hklm != null)
                    {

                        obp = hklm.GetValue("ServerHttpPort");
                    }
                    else
                    {

                        obp = hklm32.GetValue("ServerHttpPort");
                    }


                    portNumber = obp.ToString();
                    return portNumber;


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
                return null;

            }


        }

        public static bool EnableP2v()
        {
            try
            {//Now we are not using this.
                //In 5.5 we are not showing p2v directly they need to enable p2v with regisrty changes.
                int valueOfEnableP2v = 0;
                Object obp = null;
                RegistryKey hklm = Registry.LocalMachine;
                RegistryKey hklm32 = Registry.LocalMachine;

                hklm = hklm.OpenSubKey("SOFTWARE\\Wow6432Node\\SV Systems\\vContinuum");

                hklm32 = hklm32.OpenSubKey("SOFTWARE\\SV Systems\\vContinuum");
                if (hklm == null && hklm32 == null)
                {
                    return false;
                }
                else
                {

                    if (hklm != null)
                    {
                        obp = hklm.GetValue("Enable_P2V");



                    }
                    else if (hklm32 != null)
                    {
                        obp = hklm32.GetValue("Enable_P2V");

                    }
                    else
                    {
                        return true;


                    }
                    if (obp == null)
                    {
                        return false;
                    }

                    if (obp.ToString() != null)
                    {


                        if (obp.ToString() == "1")
                        {
                            return true;
                        }
                        else
                        {
                            return false;
                        }
                    }
                    else
                    {
                        return false; ;
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
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;

            }

            //return true;
             
        }
        public static String GetcxIPWithPortNumber()
        {
            try
            {// With this we will read cx ip and port number in the registry configuration....

                String cxIp = "", portNumber = "";

                cxIp = GetcxIP();

                portNumber = GetcxPortNumber();

                String CxIPwithPort = (cxIp + ":" + portNumber);

                return CxIPwithPort.TrimEnd();
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
                return null;

            }
            

        }

        public static String VersionNumber()
        {
            // For display purpose in UI we are reading both verson from the reistry...
            Object obp;
            RegistryKey hklm = Registry.LocalMachine;
            RegistryKey hklm32 = Registry.LocalMachine;

            try
            {

                hklm = hklm.OpenSubKey("SOFTWARE\\Wow6432Node\\SV Systems\\vContinuum");
                hklm32 = hklm32.OpenSubKey("SOFTWARE\\SV Systems\\vContinuum");
                
                if (hklm == null && hklm32 == null)
                {
                    return null;
                }
                else
                {
                    string patchVersion = null;
                    if (hklm != null)
                    {
                        string[] nameList = hklm.GetValueNames();
                       
                        foreach (string name in nameList)
                        {
                            if (name == "PROD_VERSION")
                            {
                                patchVersion = hklm.GetValue("PROD_VERSION").ToString();
                            }
                        }
                       
                        obp = hklm.GetValue("vContinuumVersion");
                    }
                    else
                    {
                        string[] nameList = hklm32.GetValueNames();
                        foreach (string name in nameList)
                        {
                            if (name == "PROD_VERSION")
                            {
                                patchVersion = hklm32.GetValue("PROD_VERSION").ToString();
                            }
                        }                      
                        obp = hklm32.GetValue("vContinuumVersion");
                    }
                    string sp_dir;
                    if(patchVersion == null)
                    {
                         sp_dir = obp.ToString();
                    }
                    else
                    {
                        sp_dir = obp.ToString() + " (" + patchVersion + ")";

                    }                  
                    return sp_dir;
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
                return null;
            }
        }
        public static int BrandingCode()
        {
            try
            {//Now we are not using this.
                //In 5.5 we are not showing p2v directly they need to enable p2v with regisrty changes.
                int valueOfEnableP2v = 0;
                Object obp = null;
                RegistryKey hklm = Registry.LocalMachine;
                RegistryKey hklm32 = Registry.LocalMachine;

                hklm = hklm.OpenSubKey("SOFTWARE\\Wow6432Node\\SV Systems\\vContinuum");

                hklm32 = hklm32.OpenSubKey("SOFTWARE\\SV Systems\\vContinuum");
                if (hklm == null && hklm32 == null)
                {
                    return 0;
                }
                else
                {

                    if (hklm != null)
                    {
                        obp = hklm.GetValue("Branding");
                        try
                        {
                            return int.Parse(obp.ToString());
                        }
                        catch (Exception ex)
                        {
                            return 0;
                        }

                    }
                    else if (hklm32 != null)
                    {
                        obp = hklm32.GetValue("Branding");
                        try
                        {
                            return int.Parse(obp.ToString());
                        }
                        catch (Exception ex)
                        {
                            return 0;
                        }
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
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return 0;

            }

            return 0;

        }
        public static String BuildDateOfVcontinuum()
        {// For display purpose in UI we are reading both verson from the reistry...
            Object obp;
           // Object obp;
            RegistryKey hklm = Registry.LocalMachine;
            RegistryKey hklm32 = Registry.LocalMachine;

            try
            {

                hklm = hklm.OpenSubKey("SOFTWARE\\Wow6432Node\\SV Systems\\vContinuum");
                hklm32 = hklm32.OpenSubKey("SOFTWARE\\SV Systems\\vContinuum");

                if (hklm == null && hklm32 == null)
                {
                    return null;
                }
                else
                {
                    string patchVersion = null;
                    if (hklm != null)
                    {
                        string[] nameList = hklm.GetValueNames();

                        foreach (string name in nameList)
                        {
                            if (name == "PROD_VERSION")
                            {
                                patchVersion = hklm.GetValue("PROD_VERSION").ToString();
                            }
                        }

                        obp = hklm.GetValue("vContinuumBuildVersion");
                    }
                    else
                    {
                        string[] nameList = hklm32.GetValueNames();
                        foreach (string name in nameList)
                        {
                            if (name == "PROD_VERSION")
                            {
                                patchVersion = hklm32.GetValue("PROD_VERSION").ToString();
                            }
                        }

                        obp = hklm32.GetValue("vContinuumBuildVersion");
                    }
                    string sp_dir;
                    if (patchVersion == null)
                    {
                        sp_dir = obp.ToString();
                    }
                    else
                    {
                        sp_dir = obp.ToString() + " (" + patchVersion + ")";

                    }

                    return sp_dir;
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
                return null;
            }

        }


        public static bool EnableEditMasterConfigFile()
        {
            try
            {

                // From 6.0 we are giving option to edit masterConfigfile.xml using registry entry with the test value one
                // So that we are checking whether it is exists or not..
                // This will be used only by the expert center and ps guys.
                //We are not directly showing to customer...
                Trace.WriteLine(DateTime.Now + "\t Came here to read the test registry entry ");
                Object obp = null;
                RegistryKey hklm = Registry.LocalMachine;
                RegistryKey hklm32 = Registry.LocalMachine;

                hklm = hklm.OpenSubKey("SOFTWARE\\Wow6432Node\\SV Systems\\vContinuum");

                hklm32 = hklm32.OpenSubKey("SOFTWARE\\SV Systems\\vContinuum");
                if (hklm == null && hklm32 == null)
                {
                    Trace.WriteLine(DateTime.Now + "\t Both the hklm and hklm32 are having null values");
                    return false;
                }
                else
                {

                    if (hklm != null)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Printingh the path " + hklm.ToString());
                        obp = hklm.GetValue("Test");



                    }
                    else if (hklm32 != null)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Printingh the path " + hklm32.ToString());
                        // Trace.WriteLine(DateTime.Now + "\t printing the path values " + hklm32.GetValue("Test").ToString());
                        obp = hklm32.GetValue("Test");

                    }
                    else
                    {
                        return true;


                    }
                    if (obp == null)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Registry entry value is null");
                        return false;
                    }

                    if (obp.ToString() != null)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Printing the test registry entry value " + obp.ToString());

                        if (obp.ToString() == "1")
                        {
                            return true;
                        }
                        else
                        {
                            return false;
                        }
                    }
                    else
                    {
                        return false; ;
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
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;

            }

           // return true;

        }

        /*
         * this method will read CXPS.conf file from vx installation path + \\transport folder.
         * search for the allowed_dirs line in file. 
         * modifies line with vx agent path + failover 
         * to modify that line first we will split it with = 
         * if length is 2 then we will check second string for current vx agent path + transport folder is alreay part of it.
         * if not we will add that foldder to that string
         * Once modification is done we will move current file to cxps_old.conf
         * and write new file with new modifications
         * */

        public static bool ModifycxpsConfig()
        {           
            try
            {               
                string vxAgentPath = Vxagentpath();
                if (vxAgentPath != null)
                {
                    if (File.Exists(vxAgentPath + "\\transport\\cxps.conf"))
                    {
                        var lines = File.ReadAllLines(vxAgentPath + "\\transport\\cxps.conf");
                        int lineNumber = 0;
                        string modifiedLine = null;
                        foreach (string linetoEdit in lines)
                        {
                            if (linetoEdit != null && (linetoEdit.StartsWith("allowed_dirs =") || linetoEdit.StartsWith("allowed_dirs=")))
                            {
                                string[] lineEditsplit = linetoEdit.Split('=');

                                if (lineEditsplit.Length == 2)
                                {
                                    string path = lineEditsplit[1];
                                    if (path.Contains(vxAgentPath + "\\Failover\\Data"))
                                    {
                                        Trace.WriteLine(DateTime.Now + "\t Already vxagent folder exist in file " + path);
                                        break;
                                    }
                                    else
                                    {
                                        path = path.TrimEnd();
                                        if (!path.EndsWith(";") && path.Length != 0)
                                        {
                                            path = path + ";" + vxAgentPath + "\\Failover\\Data";
                                            modifiedLine = lineEditsplit[0]+ "=" + path;
                                            break;
                                        }
                                        else
                                        {
                                            path = path + vxAgentPath + "\\Failover\\Data";
                                            modifiedLine = lineEditsplit[0] + "=" + path;
                                            break;
                                        }
                                    }
                                }
                                else
                                {
                                    string path = linetoEdit;
                                    path = path + vxAgentPath + "Failover\\Data";
                                    modifiedLine = lineEditsplit[0]+ "=" + path;
                                    break;
                                }
                            }
                            lineNumber++;
                        }

                        if (modifiedLine != null)
                        {
                            lines[lineNumber] = modifiedLine;
                            try
                            {
                                if (File.Exists(vxAgentPath + "\\transport\\cxps_old.conf"))
                                {
                                    File.Delete(vxAgentPath + "\\transport\\cxps_old.conf");
                                }
                            }
                            catch (Exception ex)
                            {
                                Trace.WriteLine(DateTime.Now + "\t Failed to delete old back up file " + ex.Message);
                            }
                            try
                            {
                                File.Move(vxAgentPath + "\\transport\\cxps.conf", vxAgentPath + "\\transport\\cxps_old.conf");
                            }
                            catch (Exception ex)
                            {
                                Trace.WriteLine(DateTime.Now + "\t Failed to Move file as old " + ex.Message);
                            }
                            try
                            {
                                File.WriteAllLines(vxAgentPath + "\\transport\\cxps.conf", lines);
                            }
                            catch (Exception ex)
                            {
                                Trace.WriteLine(DateTime.Now + "\t Failed to Write a file " + ex.Message);
                            }
                            Trace.WriteLine(DateTime.Now + "\t Modified line " + modifiedLine);
                        }                       
                    }
                    else
                    {
                        Trace.WriteLine(DateTime.Now + "\t csps.conf file not found ");                        
                    }
                }
            }
            catch (Exception except)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(except, true);
                System.FormatException formatException = new FormatException("Inner exception", except);
                Trace.WriteLine(formatException.GetBaseException());
                Trace.WriteLine(formatException.Data);
                Trace.WriteLine(formatException.InnerException);
                Trace.WriteLine(formatException.Message);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine("Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + except.Message);
                Trace.WriteLine("ERROR___________________________________________");
                
            }
           
            return true;
        }

        public static bool VerifyUnifiedAgentCertificate()
        {
            try
            {

                // From 6.0 we are giving option to edit masterConfigfile.xml using registry entry with the test value one
                // So that we are checking whether it is exists or not..
                // This will be used only by the expert center and ps guys.
                //We are not directly showing to customer...
                Trace.WriteLine(DateTime.Now + "\t Came here to read the test registry entry ");
                Object obp = null;
                RegistryKey hklm = Registry.LocalMachine;
                RegistryKey hklm32 = Registry.LocalMachine;

                hklm = hklm.OpenSubKey("SOFTWARE\\Wow6432Node\\SV Systems\\vContinuum");

                hklm32 = hklm32.OpenSubKey("SOFTWARE\\SV Systems\\vContinuum");
                if (hklm == null && hklm32 == null)
                {
                    Trace.WriteLine(DateTime.Now + "\t Both the hklm and hklm32 are having null values");
                    return false;
                }
                else
                {

                    if (hklm != null)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Printingh the path " + hklm.ToString());
                        obp = hklm.GetValue("VerifySign");
                   


                    }
                    else if (hklm32 != null)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Printingh the path " + hklm32.ToString());
                        // Trace.WriteLine(DateTime.Now + "\t printing the path values " + hklm32.GetValue("Test").ToString());
                        obp = hklm32.GetValue("VerifySign");

                    }
                    else
                    {
                        return true;


                    }
                    if (obp == null)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Registry entry value is null");
                        return false;
                    }

                    if (obp.ToString() != null)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Printing the test registry entry value " + obp.ToString());

                        if (obp.ToString() == "0")
                        {
                            return true;
                        }
                        else
                        {
                            return false;
                        }
                    }
                    else
                    {
                        return false; ;
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
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;

            }

            // return true;

        }

        public bool SetFilePermissions(string file)
        {
            try
            {
                //get security access
                FileInfo fi = new FileInfo(file);
                FileSecurity fs = fi.GetAccessControl();

                //remove any inherited access
                fs.SetAccessRuleProtection(true, false);

                //get any special user access
                AuthorizationRuleCollection rules = fs.GetAccessRules(true, true, typeof(System.Security.Principal.NTAccount));

                //remove any special access
                foreach (FileSystemAccessRule rule in rules)
                    fs.RemoveAccessRule(rule);
                //string currentUser = System.Security.Principal.WindowsIdentity.GetCurrent().Name.ToString();
                //add current user with full control.
                fs.AddAccessRule(new FileSystemAccessRule("BUILTIN\\Administrators", FileSystemRights.FullControl, AccessControlType.Allow));
                fs.AddAccessRule(new FileSystemAccessRule("NT AUTHORITY\\SYSTEM", FileSystemRights.FullControl, AccessControlType.Allow));
                //flush security access.
                File.SetAccessControl(file, fs);
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed to set permissions for the file " + ex.Message);
            }
            return true;
        }

        internal static string GetProgramFilesPath()
        {
            if (8 == IntPtr.Size
              || (!String.IsNullOrEmpty(Environment.GetEnvironmentVariable("PROCESSOR_ARCHITEW6432"))))
            {
                return Environment.GetEnvironmentVariable("ProgramFiles(x86)");
            }
            else
            {

                return Environment.GetEnvironmentVariable("ProgramFiles");
            }
        }


        private static bool CheckPassPharsePathExists()
        {
            try
            {
                Object obp;
                RegistryKey hklm = Registry.LocalMachine;

                hklm = hklm.OpenSubKey("SOFTWARE\\Wow6432Node\\SV Systems\\vContinuum");

                if (hklm == null)
                {
                    hklm = hklm.OpenSubKey("SOFTWARE\\SV Systems\\vContinuum");
                    if (hklm == null)
                    {                        
                        return false;
                    }
                }
                obp = hklm.GetValue("PathForFiles");
                if (obp != null)
                {
                    Trace.WriteLine(DateTime.Now + "\t Already finger print path is there in Registry");
                    return true;
                }
                else
                {
                    Trace.WriteLine(DateTime.Now + "\t Need to add registry path for Fingerprints");
                    return false;
                }
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed to read registry for fingerprintpath " + ex.Message);
                return false;
            }
        }

        public static bool AddFingerPathToRegistry()
        {
            if (CheckPassPharsePathExists() == false)
            {
                RegistryKey hklm = Registry.LocalMachine.OpenSubKey("SOFTWARE\\Wow6432Node\\SV Systems\\vContinuum",true);

                if (hklm == null)
                {
                    hklm = Registry.LocalMachine.OpenSubKey("SOFTWARE\\SV Systems\\vContinuum",true);
                    if (hklm == null)
                        return false;
                }

                
                
                Trace.WriteLine(DateTime.Now + "\t Path to create in registry key" + hklm.ToString());
                //Registry registry = Microsoft.Win32.Registry.LocalMachine.CreateSubKey(obp);
                //registry.
                try
                {

                    //Microsoft.Win32.RegistryKey key = Microsoft.Win32.Registry.LocalMachine.OpenSubKey("\""+ path+ "\"");

                    //Trace.WriteLine(DateTime.Now + "\t OPen sub key successfully");
                    string frameFingerPath = GetProgramFilesPath() + "\\InMage Systems";
                    Trace.WriteLine(DateTime.Now + "\t PathForFiles  " + frameFingerPath);

                    //key.CreateSubKey("FingerPrintPath");
                   // Object obj = new Object();

                    hklm.SetValue("PathForFiles", frameFingerPath, RegistryValueKind.String);
                    
                    hklm.Close();
                }
                catch (Exception ex)
                {
                    Trace.WriteLine(DateTime.Now + "\t Failed to add registry key " + ex.Message);
                }
            }

            return true;
        }
        public static String ServicesToManual()
        {
            // For display purpose in UI we are reading both verson from the reistry...
            Object obp;
            RegistryKey hklm = Registry.LocalMachine;
            RegistryKey hklm32 = Registry.LocalMachine;

            try
            {

                hklm = hklm.OpenSubKey("SOFTWARE\\Wow6432Node\\SV Systems\\vContinuum");
                hklm32 = hklm32.OpenSubKey("SOFTWARE\\SV Systems\\vContinuum");

                if (hklm == null && hklm32 == null)
                {
                    return null;
                }
                else
                {
                    string patchVersion = null;
                    if (hklm != null)
                    {

                        obp = hklm.GetValue("ServicesToManual");
                    }
                    else
                    {

                        obp = hklm32.GetValue("ServicesToManual");
                    }
                    string services = null;
                    if (obp != null)
                    {
                        services = obp.ToString();
                        Trace.WriteLine(DateTime.Now + "\t Service to manaul " + services);

                    }
                    else
                    {
                        Trace.WriteLine(DateTime.Now + "\t Service to manaul null");
                    }

                    return services;
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
                return null;
            }
        }



    }
}
