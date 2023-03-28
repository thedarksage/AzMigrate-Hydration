using System;
using System.Diagnostics;
using System.Linq;
using System.Security;
using System.Threading;
using System.Windows.Forms;
using Microsoft.Win32;
using System.Management;
using System.Runtime.InteropServices;
using System.IO;
using ASRResources;
using ASRSetupFramework;

namespace WrapperUnifiedAgent
{
    public static class Program
    {
        /// </summary>
        /// Blocks multiple instances of same exe.
        /// </summary>
        private static Mutex mutex;

        /// <summary>
        /// Main entry point
        /// </summary>
        [System.Diagnostics.CodeAnalysis.SuppressMessage(
            "Microsoft.Design",
            "CA1031:DoNotCatchGeneralExceptionTypes",
            Justification = "Setup should never crash on the user." +
            "By catching the general exception at this level, we keep that from happening.")]
        [SecurityCritical]
        [STAThread]
        public static int Main()
        {
            SetupHelper.UASetupReturnValues returnValue = SetupHelper.UASetupReturnValues.Failed;

            // Decide if we go Console mode or Windows mode
            if (Environment.GetCommandLineArgs().Length > 1)
            {
                ShowConsoleWindow();
            }
            else
            {
                HideConsoleWindow();
            }

            // Create mutex
            bool createdNew;
            mutex = new Mutex(true, "Microsoft Azure Site Recovery Unified Agent Wrapper", out createdNew);
            if (!createdNew)
            {
                if (Environment.GetCommandLineArgs().Length > 1)
                {
                    Console.WriteLine(StringResources.ProcessAlreadyLaunchedUA);
                    return (int)SetupHelper.UASetupReturnValues.AnInstanceIsAlreadyRunning;
                }
                else
                {
                    MessageBox.Show(StringResources.ProcessAlreadyLaunchedUA, StringResources.SetupMessageBoxTitleUA, MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return (int)returnValue;
                }
            }

            // Set log file name and start a logging session
            string LogFile = "WrapperUnifiedAgent.log";
            UnifiedAgentLogs UnifiedAgentLogs = new UnifiedAgentLogs(LogFile);
            UnifiedAgentLogs.LogMessage(TraceEventType.Information, "");
            UnifiedAgentLogs.LogMessage(TraceEventType.Information, "===========================================");
            UnifiedAgentLogs.LogMessage(TraceEventType.Information, "Trace Session Started.");

            try
            {
                int uaReturnCode;
                // Check for .Net 3.5 or .Net 4.0 or .Net 4.5. Please note that .Net 4.5 information under v4 registry hive.
                if ((ValidationHelper.GetDotNetVersionFromRegistry("3.5")) || (ValidationHelper.GetDotNetVersionFromRegistry("4.0")) || (ValidationHelper.GetDotNetVersionFromRegistry("4")))
                {
                    if (Environment.GetCommandLineArgs().Length > 1)
                    {
                        Console.WriteLine(".NET Framework is avaialble. Proceeding ahead with unified agent installation.");
                    }
                    UnifiedAgentLogs.LogMessage(TraceEventType.Information, ".NET Framework installed. Procceeding ahead with unified agent installation.");

                    // Invoke unified agent actuall installer and captrue the return code.
                    uaReturnCode = LaunchUnifiedAgent();
                    if (Environment.GetCommandLineArgs().Length > 1)
                    {
                        if ((uaReturnCode == 0) || (uaReturnCode == 3010))
                        {
                            Console.WriteLine("Unified agent installation has succeeded.");
                            if (uaReturnCode == 3010)
                            {
                                Console.WriteLine(StringResources.PostInstallationRebootRequired);
                            }
                        }
                        else
                        {
                            Console.WriteLine("Unified agent installation has failed with exit code - {0}.", uaReturnCode);
                        }
                        string uaLogFileName = Path.GetPathRoot(Environment.SystemDirectory) + "ProgramData\\ASRSetupLogs\\ASRUnifiedAgentInstaller.log";
                        Console.WriteLine("Please refer installation log for more details - {0}.", uaLogFileName);
                    }

                    return (int)uaReturnCode;
                }
                else
                {
                    // If .NET is not available, show a message box in care of interactive installation and return an exit code in case of silent installation.
                    if (Environment.GetCommandLineArgs().Length == 1)
                    {
                        MessageBox.Show(
                            StringResources.DotNetIsNotAvailable, 
                            StringResources.AppInitError, 
                            MessageBoxButtons.OK, 
                            MessageBoxIcon.Error);
                    }
                    else
                    {
                        Console.WriteLine(StringResources.DotNetIsNotAvailable);
                    }

                    UnifiedAgentLogs.LogMessage(
                        TraceEventType.Error, 
                        ".NET Framework is not available");
                    UnifiedAgentLogs.LogMessage(
                        TraceEventType.Information,
                        "Exit code: {0}({1})", 
                        SetupHelper.UASetupReturnValues.DotNetIsNotAvailable,
                        (int)SetupHelper.UASetupReturnValues.DotNetIsNotAvailable);
                    return (int)SetupHelper.UASetupReturnValues.DotNetIsNotAvailable;
                }
            }
            catch (Exception exception)
            {
                UnifiedAgentLogs.LogMessage(TraceEventType.Information, "Uncaught Exception", exception);
                MessageBox.Show(exception.Message, "Exception", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return (int)SetupHelper.UASetupReturnValues.Failed;
            }
            finally
            {
                UnifiedAgentLogs.LogMessage(TraceEventType.Information, "Trace Session Ended.");
                mutex.ReleaseMutex();
            }
        }

        /// <summary>
        /// Launches UnifiedAgent binary and passes arguments that are passed to this executable.
        /// </summary>
        public static int LaunchUnifiedAgent()
        {
            int returnCode = -1;
            try
            {
	            if (Environment.GetCommandLineArgs().Length > 1)
	            {
	                //Remove the first argument as it is the name of the current exe and wrap each argument in double quotes.
	                string[] arguments = Environment.GetCommandLineArgs().Skip(1).ToArray();
	                for (int i = 0; i < arguments.Length; i++)
	                {
	                    arguments[i] = "\""+arguments[i]+"\"";
	                }
	                returnCode = RunCommand("UnifiedAgentInstaller.exe", string.Join(" ", arguments));
	            }
	            else
	            {
	                returnCode = RunCommand("UnifiedAgentInstaller.exe", "");
	            }
	            return returnCode;
	     	}
            catch (Exception ex)
            {
                UnifiedAgentLogs.LogMessage(TraceEventType.Error, "Exception occured while launching UnifiedAgent: {0}", ex.ToString());
                return returnCode;
            }
        }

        /// <summary>
        /// Runs an executable with given arguments
        /// </summary>
        public static int RunCommand(string filePath, string arguments = "", int timeout = System.Threading.Timeout.Infinite)
        {
            try
            {
                ProcessStartInfo processStartInfo = new ProcessStartInfo();
                int exitCode = -1;

                //Set the defaults
                processStartInfo.FileName = filePath;
                processStartInfo.Arguments = arguments;
                processStartInfo.UseShellExecute = false;
                processStartInfo.RedirectStandardError = true;
                processStartInfo.RedirectStandardOutput = true;
                processStartInfo.Verb = "runas";
                
                UnifiedAgentLogs.LogMessage(TraceEventType.Information, "Executing the following command: {0} {1}", filePath, arguments);
                using (Process proc = new Process())
                {
                    proc.StartInfo = processStartInfo;
                    proc.EnableRaisingEvents = true;

                    // Showing output from Installer on console.
                    proc.ErrorDataReceived += new DataReceivedEventHandler((sender, e) =>
                        {
                            Console.WriteLine(e.Data);
                            UnifiedAgentLogs.LogMessage(TraceEventType.Information, "Error Info : {0}", e.Data);
                        });
                    proc.OutputDataReceived += new DataReceivedEventHandler((sender, e) =>
                        {
                            Console.WriteLine(e.Data);
                            UnifiedAgentLogs.LogMessage(TraceEventType.Information, "Output Info : {0}", e.Data);
                        });

                    proc.Start();
                    proc.BeginOutputReadLine();
                    bool process_completed = proc.WaitForExit(timeout);
                    if (process_completed)
                    {
                        Trc.Log(LogLevel.Always, "The command {0} completed within given timeout period {1}", filePath, timeout);
                    }
                    else
                    {
                        Trc.Log(LogLevel.Always, "The command {0} did not complete within given timeout period {1}. Trying to kill the process.", filePath, timeout);
                        try
                        {
                            proc.Kill();
                            Trc.Log(LogLevel.Always, "The command {0} did not complete within given timeout period {1}. So process was killed.", filePath, timeout);
                        }
                        catch(Exception ex)
                        {
                            Trc.Log(LogLevel.Always, "Failed to kill the process with exception " + ex);
                        }
                        System.Threading.Thread.Sleep(1000);
                    }
                    exitCode = proc.ExitCode;
                    UnifiedAgentLogs.LogMessage(TraceEventType.Information, "Exit code returned by command: {0}", exitCode);
                    return exitCode;
                }                
            }
            catch (Exception ex)
            {
                UnifiedAgentLogs.LogMessage(TraceEventType.Error, "Exception occured: {0}", ex.ToString());
                return (int)SetupHelper.UASetupReturnValues.Failed;
            }
        }

        #region Enable/Disable Console methods

        /// <summary>
        /// Hide console window.
        /// </summary>
        private const int HideConsole = 0;

        /// <summary>
        /// Show console window.
        /// </summary>
        private const int ShowConsole = 5;

        /// <summary>
        /// Attaches console to current application.
        /// </summary>
        public static void ShowConsoleWindow()
        {
            var handle = GetConsoleWindow();

            if (handle == IntPtr.Zero)
            {
                AllocConsole();
            }
            else
            {
                ShowWindow(handle, ShowConsole);
            }
        }

        /// <summary>
        /// Remove console window from current application.
        /// </summary>
        public static void HideConsoleWindow()
        {
            var handle = GetConsoleWindow();

            ShowWindow(handle, HideConsole);
        }

        #region DLL Import Declarations
        [DllImport("kernel32.dll", SetLastError = true)]
        static extern bool AllocConsole();

        [DllImport("kernel32.dll")]
        static extern IntPtr GetConsoleWindow();

        [DllImport("user32.dll")]
        static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
        
        #endregion
        #endregion
    }
}
