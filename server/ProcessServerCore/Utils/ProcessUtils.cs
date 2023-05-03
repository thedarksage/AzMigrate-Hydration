using System.Diagnostics;
using System.Threading.Tasks;
using System.Text;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils
{
    /// <summary>
    /// Utilities to work with processes
    /// </summary>
    public static class ProcessUtils
    {
        /// <summary>
        /// Run process and return result
        /// </summary>
        /// <param name="exePath">Executable path</param>
        /// <param name="args">Arguments</param>
        /// <param name="stdOut">Standard output of the execution</param>
        /// <param name="stdErr">Standard error of the execution</param>
        /// <param name="pid">Process Id of the execution</param>
        /// <returns>Exit code of the process execution</returns>
        public static int RunProcess(string exePath, string args, out string stdOut, out string stdErr, out int pid)
        {
            StringBuilder stdOutSB = new StringBuilder(), stdErrSB = new StringBuilder();

            using (var proc = new Process())
            {
                proc.StartInfo.Arguments = args;
                proc.StartInfo.CreateNoWindow = false;
                proc.StartInfo.ErrorDialog = false;
                proc.StartInfo.FileName = exePath;
                proc.StartInfo.RedirectStandardOutput = true;
                proc.StartInfo.RedirectStandardError = true;
                proc.StartInfo.UseShellExecute = false;
                proc.StartInfo.WindowStyle = ProcessWindowStyle.Hidden;

                proc.ErrorDataReceived += (s, e) => stdErrSB.AppendLine(e.Data);
                proc.OutputDataReceived += (s, e) => stdOutSB.AppendLine(e.Data);

                proc.Start();

                // It's cleaner deadlock avoidance to use asynchronous read of
                // the streams as per the documentation
                // https://docs.microsoft.com/en-us/dotnet/api/system.diagnostics.process.standarderror
                proc.BeginErrorReadLine();
                proc.BeginOutputReadLine();

                proc.WaitForExit();

                //bool procExited = proc.WaitForExit(milliseconds: timeoutMs);

                //if (!procExited)
                //    throw new TimeoutException($"Process {exePath} {args} (PID:{proc.Id}) didn't complete in {timeoutMs} ms");

                stdOut = stdOutSB.ToString();
                stdErr = stdErrSB.ToString();
                pid = proc.Id;

                return proc.ExitCode;
            }
        }
    }
}
