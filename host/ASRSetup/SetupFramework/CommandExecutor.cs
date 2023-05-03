using System;
using System.Diagnostics;

namespace ASRSetupFramework
{
    public class CommandExecutor
    {
        /// <summary>
        /// Runs an executable with given arguments
        /// </summary>
        /// <param name="filePath">File path of the executable</param>
        /// <param name="arguments">Arguments</param>
        /// <returns>Exit code</returns>
        public static int RunCommand(string filePath, string arguments="", int timeout = System.Threading.Timeout.Infinite)
        {
            int exitCode = -1;
            try
            {
                ProcessStartInfo processStartInfo = new ProcessStartInfo();

                //Set the defaults
                processStartInfo.FileName = filePath;
                processStartInfo.Arguments = arguments;
                processStartInfo.CreateNoWindow = true;
                processStartInfo.UseShellExecute = false;
                processStartInfo.Verb = "runas";
                processStartInfo.WindowStyle = ProcessWindowStyle.Hidden;

                Trc.Log(LogLevel.Always, "Executing the following command: {0} {1}", filePath, arguments);

                using (Process process = Process.Start(processStartInfo))
                {
                    Trc.Log(LogLevel.Always, "Process started.");
                    Trc.Log(LogLevel.Always, "Running command {0} with timeout value {1}", filePath, timeout);
                    bool process_completed = process.WaitForExit(timeout);
                    if (process_completed)
                    {
                        Trc.Log(LogLevel.Always, "The command {0} completed within given timeout period {1}", filePath, timeout);
                    }
                    else
                    {
                        Trc.Log(LogLevel.Always, "The command {0} did not complete within given timeout period {1}. Trying to kill the process.", filePath, timeout);
                        try
                        {
                            process.Kill();
                            Trc.Log(LogLevel.Always, "The command {0} did not complete within given timeout period {1}. So process was killed.", filePath, timeout);
                        }
                        catch(Exception ex)
                        {
                            Trc.Log(LogLevel.Always, "Failed to kill the process with exception " + ex);
                        }
                        System.Threading.Thread.Sleep(1000);
                    }
                    exitCode = process.ExitCode;
                    Trc.Log(LogLevel.Always, "Exit code returned by command: {0}", exitCode);
                    return exitCode;
                }
            }
            catch (Exception ex)
            {
                Trc.Log(LogLevel.Always, "Exception occured: {0}", ex.ToString());
                return exitCode;
            }
        }

        /// <summary>
        /// Runs an executable with given arguments and returns the standard output the executables
        /// </summary>
        /// <param name="filePath">File path of the executable</param>
        /// <param name="arguments">Arguments</param>
        /// <param name="output">Standard output of the executable</param>
        /// <returns>Exit code</returns>
        public static int RunCommand(string filePath, string arguments, out string output, int timeout = System.Threading.Timeout.Infinite)
        {
            ProcessStartInfo processStartInfo = new ProcessStartInfo();

            //Set the defaults
            processStartInfo.FileName = filePath;
            processStartInfo.Arguments = arguments;
            processStartInfo.CreateNoWindow = true;
            processStartInfo.Verb = "runas";
            processStartInfo.UseShellExecute = false;
            processStartInfo.RedirectStandardOutput = true;
            processStartInfo.WindowStyle = ProcessWindowStyle.Hidden;

            Trc.Log(LogLevel.Always, "Executing the following command: {0} {1}", filePath, arguments);

            using (Process process = Process.Start(processStartInfo))
            {
                Trc.Log(LogLevel.Always, "Process started.");
                output = process.StandardOutput.ReadToEnd();
                bool process_completed = process.WaitForExit(timeout);
                if (process_completed)
                {
                    Trc.Log(LogLevel.Always, "The command {0} completed within given timeout period {1}", filePath, timeout);
                }
                else
                {
                    Trc.Log(LogLevel.Always, "The command {0} did not complete within given timeout period {1}. Trying to kill the process.", filePath, timeout);
                    try
                    {
                        process.Kill();
                        Trc.Log(LogLevel.Always, "The command {0} did not complete within given timeout period {1}. So process was killed.", filePath, timeout);
                    }
                    catch(Exception ex)
                    {
                        Trc.Log(LogLevel.Always, "Failed to kill the process with exception " + ex);
                    }
                    System.Threading.Thread.Sleep(1000);
                }
                int exitCode = process.ExitCode;
                Trc.Log(LogLevel.Always, "Exit code returned by command: {0}", exitCode);
                return exitCode;
            }
        }

        /// <summary>
        /// Runs an executable with given arguments and returns the standard output the executables
        /// </summary>
        /// <param name="filePath">File path of the executable</param>
        /// <param name="arguments">Arguments</param>
        /// <param name="output">Standard output of the executable</param>
        /// <param name="error">returns process StandardError</param>
        /// <returns>Exit code</returns>
        public static int RunCommandNoArgsLogging(string filePath, string arguments, out string output, out string error, int timeout = System.Threading.Timeout.Infinite)
        {
            ProcessStartInfo processStartInfo = new ProcessStartInfo();

            //Set the defaults
            processStartInfo.FileName = filePath;
            processStartInfo.Arguments = arguments;
            processStartInfo.CreateNoWindow = true;
            processStartInfo.Verb = "runas";
            processStartInfo.UseShellExecute = false;
            processStartInfo.RedirectStandardOutput = true;
            processStartInfo.RedirectStandardError = true;
            processStartInfo.WindowStyle = ProcessWindowStyle.Hidden;

            Trc.Log(LogLevel.Always, "Executing the following command: {0}", filePath);

            using (Process process = Process.Start(processStartInfo))
            {
                Trc.Log(LogLevel.Always, "Process started.");
                output = process.StandardOutput.ReadToEnd();
                error = process.StandardError.ReadToEnd();
                bool process_completed = process.WaitForExit(timeout);
                if (process_completed)
                {
                    Trc.Log(LogLevel.Always, "The command {0} completed within given timeout period {1}", filePath, timeout);
                }
                else
                {
                    Trc.Log(LogLevel.Always, "The command {0} did not complete within given timeout period {1}. Trying to kill the process.", filePath, timeout);
                    try
                    {
                        process.Kill();
                        Trc.Log(LogLevel.Always, "The command {0} did not complete within given timeout period {1}. So process was killed.", filePath, timeout);
                    }
                    catch(Exception ex)
                    {
                        Trc.Log(LogLevel.Always, "Failed to kill the process with exception " + ex);
                    }
                    System.Threading.Thread.Sleep(1000);
                }
                int exitCode = process.ExitCode;
                Trc.Log(LogLevel.Always, "Exit code returned by command: {0}", exitCode);
                return exitCode;
            }
        }

        /// <summary>
        /// Executes an exe or a file with given arguments
        /// </summary>
        /// <param name="filePath">File path of the executable</param>
        /// <param name="output">returns output</param>
        /// <param name="error">returns process StandardError</param>
        /// <param name="arguments">Arguments</param>
        /// <returns>Exit code</returns>               
        public static int ExecuteCommand(string filePath, out string output, out string error, string arguments = "", int timeout = System.Threading.Timeout.Infinite)
        {            
            ProcessStartInfo processStartInfo = new ProcessStartInfo();

            //Set the defaults
            processStartInfo.FileName = filePath;
            processStartInfo.Arguments = arguments;
            processStartInfo.CreateNoWindow = true;
			processStartInfo.Verb = "runas";
            processStartInfo.UseShellExecute = false;
            processStartInfo.RedirectStandardOutput = true;
            processStartInfo.RedirectStandardError = true;
            processStartInfo.WindowStyle = ProcessWindowStyle.Hidden;

            Trc.Log(LogLevel.Always, "Executing the following command: {0} {1}", filePath, arguments);

            using (Process process = Process.Start(processStartInfo))
            {
                Trc.Log(LogLevel.Always, "Process started.");
                output = process.StandardOutput.ReadToEnd();
                error = process.StandardError.ReadToEnd();
                bool process_completed = process.WaitForExit(timeout);
                if (process_completed)
                {
                    Trc.Log(LogLevel.Always, "The command {0} completed within given timeout period {1}", filePath, timeout);
                }
                else
                {
                    Trc.Log(LogLevel.Always, "The command {0} did not complete within given timeout period {1}. Trying to kill the process.", filePath, timeout);
                    try
                    {
                        process.Kill();
                        Trc.Log(LogLevel.Always, "The command {0} did not complete within given timeout period {1}. So process was killed.", filePath, timeout);
                    }
                    catch(Exception ex)
                    {
                        Trc.Log(LogLevel.Always, "Failed to kill the process with exception " + ex);
                    }
                    System.Threading.Thread.Sleep(1000);
                }
                int exitCode = process.ExitCode;
                Trc.Log(LogLevel.Always, "Exit code returned by command: {0}", exitCode);
                Trc.Log(LogLevel.Always, "Process output: {0}", output);
                Trc.Log(LogLevel.Always, "Process error: {0}", error);
                return exitCode;
            }            
        }

        /// <summary>
        /// Executes an exe or a file with given arguments
        /// </summary>
        /// <param name="filePath">File path of the executable</param>
        /// <param name="output">returns output</param>
        /// <param name="arguments">Arguments</param>
        /// <returns>Exit code</returns>               
        public static int ExecuteCommand(string filePath, out string output, string arguments = "", int timeout = System.Threading.Timeout.Infinite)
        {
            string error;
            return CommandExecutor.ExecuteCommand(
                filePath,
                out output,
                out error,
                arguments,
                timeout);
        }

        /// <summary>
        /// Executes an exe or a file with given arguments
        /// </summary>
        /// <param name="filePath">File path of the executable</param>
        /// <param name="arguments">Arguments</param>
        /// <returns>Exit code</returns>               
        public static int ExecuteCommand(string filePath, string arguments = "", int timeout = System.Threading.Timeout.Infinite)
        {
            string output, error;
            return CommandExecutor.ExecuteCommand(
                filePath,
                out output,
                out error,
                arguments,
                timeout);
        }

        /// <summary>
        /// Runs an executable with given arguments and returns the standard output the executables
        /// </summary>
        /// <param name="filePath">File path of the executable</param>
        /// <param name="arguments">Arguments</param>
        /// <param name="output">Standard output of the executable</param>
        /// <returns>Exit code</returns>
        public static int RunCommandNoArgsLogging(string filePath, string arguments, out string output, int timeout = System.Threading.Timeout.Infinite)
        {
            string error;
            return CommandExecutor.RunCommandNoArgsLogging(
                filePath,
                arguments,
                out output,
                out error,
                timeout);
        }
    }
}