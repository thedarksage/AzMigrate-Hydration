using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Tracing;
using Microsoft.Azure.SiteRecovery.ProcessServerMonitor;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Native;
using System;
using System.Diagnostics;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using System.ComponentModel;

namespace ProcessServerMonitor
{

    class ProcessInvoker
    {
        private ProcessStartInfo m_processInfo;

        private string m_procName;

        public ProcessInvoker(string filename, string args, string processName)
        {
            m_processInfo = new ProcessStartInfo();
            m_processInfo.CreateNoWindow = true;
            m_processInfo.UseShellExecute = false;
            m_processInfo.FileName = filename;
            m_processInfo.Arguments = args;
            m_processInfo.RedirectStandardOutput = false;
            m_processInfo.RedirectStandardError = false;
            m_processInfo.WindowStyle = ProcessWindowStyle.Hidden;

            m_procName = processName;
        }

        internal async Task MonitorProcess(CancellationToken token)
        {
            // To do: Can we add a new Trace source
            Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                            TraceEventType.Information, "Started Process Invoker");

            token.ThrowIfCancellationRequested();

            // if process is already running, check the status after every 5 sec
            while (Process.GetProcesses().Any(p => p.ProcessName.Equals(m_procName, StringComparison.OrdinalIgnoreCase)))
            {
                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                                   TraceEventType.Verbose, string.Format("Process {0} is already running", m_procName));
                await Task.Delay(
                    Settings.Default.ProcessPollInterval, token).ConfigureAwait(false);
            }
            // wait for 5 min after process is stopped before restarting
            // if there is a service start we always wait for 5 minutes before starting the process
            await Task.Delay(
                        Settings.Default.ProcessRespawnInterval, token).ConfigureAwait(false);

            // Process is not running. So start the process
            while(true)
            {
                try
                {
                    using (Job job = new Job())
                    {
                        using (Process process = Process.Start(m_processInfo))
                        {
                            try
                            {
                                job.AddProcess(process.Handle);
                            }
                            catch(Win32Exception winex)
                            {
                                Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                                      TraceEventType.Error, "Failed to add process to job object with exception {0}", winex);
                            }
                            Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                                       TraceEventType.Information, "Process started");
                            using (EventWaitHandle eventWaitHandle = new EventWaitHandle(false, EventResetMode.ManualReset, Settings.Default.ProcessEventPrefix + process.Id))
                            {
                                // Check the status after every 5 seconds
                                while (Process.GetProcesses().Any(p => p.ProcessName.Equals(m_procName, StringComparison.OrdinalIgnoreCase)))
                                {
                                    Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                                           TraceEventType.Verbose, string.Format("Process {0} is already running", m_procName));
                                    await Task.Delay(
                                        Settings.Default.ProcessPollInterval).ConfigureAwait(false);
                                    if (token.IsCancellationRequested)
                                    {
                                        // if cancellation of task is requested, then signal the process to quit.
                                        Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                                        TraceEventType.Information, "Cancellation requested in task Monitor Process");
                                        eventWaitHandle.Set();
                                        break;
                                    }
                                }
                                if (token.IsCancellationRequested)
                                {
                                    await Task.Delay(
                                        Settings.Default.ProcessPollInterval).ConfigureAwait(false);
                                    var proc = Process.GetProcessesByName(m_procName);
                                    if (proc != null && proc.Length > 0)
                                    {
                                        Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                                        TraceEventType.Error, "Failed to stop process {0} with pid {1} in {2} seconds. So killing the process", m_procName, proc[0].Id, Settings.Default.ProcessPollInterval.Seconds);
                                        proc[0].Kill();
                                    }
                                }
                                token.ThrowIfCancellationRequested();
                            }
                        }
                    }
                }
                catch (OperationCanceledException)
                {
                    Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                                TraceEventType.Error, "Task cancellation exception is thrown in Process Invoker Task");
                    throw;
                }
                catch (Exception ex)
                {
                    Constants.s_psSysMonTraceSource.TraceAdminLogV2Message(
                                TraceEventType.Error, "Exception thrown in Process Invoker task {0}" + ex);
                }
                await Task.Delay(
                        Settings.Default.ProcessRespawnInterval, token).ConfigureAwait(false);
            }
        }
    }
}
