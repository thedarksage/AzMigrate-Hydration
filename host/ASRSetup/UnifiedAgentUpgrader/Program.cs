using System;
using System.Threading;
using ASRSetupFramework;

namespace UnifiedAgentUpgrader
{
    public partial class Program
    {
        public static int Main(string[] args)
        {
            UnifiedAgentUpgraderExitCodes exitcode = UnifiedAgentUpgraderExitCodes.Unknown;
            using (var mutex = new Mutex(true, "Microsoft Azure Site Recovery Upgrader", out bool created))
            {
                InitTrcLog();

                if (!created)
                {
                    Trc.Log(LogLevel.Error, "Another Instance of installer is already running.");
                    return (int)UnifiedAgentUpgraderExitCodes.UpdateAlreadyRunning;
                }

                try
                {
                    int prechecksExitCode = Prechecks();
                    if (prechecksExitCode != 0)
                    {
                        return prechecksExitCode;
                    }

                    if (!IsValidArgs(args))
                    {
                        return (int)UnifiedAgentUpgraderExitCodes.InvalidArguments;
                    }

                    IAgentUpgrade agentUpgrade = new CSPrimeAgentUpgrade();
                    if(agentUpgrade.RunUpgrade())
                    {
                        Trc.Log(LogLevel.Always, "CSPrime agent upgrade completed successfully.");
                    }
                    else
                    {
                        Trc.Log(LogLevel.Error, "CSPrime agent upgrade failed.");
                    }
                    exitcode = UnifiedAgentUpgraderExitCodes.Success;

                    int logsUploadExitCode = agentUpgrade.UploadLogs();
                    if(logsUploadExitCode != 0)
                    {
                        Trc.Log(LogLevel.Error,
                            "CSPrime Agent upgrade Logs Upload failed with exitcode {0}",
                            logsUploadExitCode);
                    }
                    else
                    {
                        Trc.Log(LogLevel.Always,
                            "CSPrime Agent upgrade Logs Upload completed successfully.");
                    }
                }
                catch (OutOfMemoryException)
                {
                    exitcode = UnifiedAgentUpgraderExitCodes.OutOfMemory;
                }
                catch (Exception ex)
                {
                    Trc.Log(LogLevel.Error, "Upgrader failed with exception {0}{1}",
                        Environment.NewLine, ex);
                    exitcode = UnifiedAgentUpgraderExitCodes.Failure;
                }
                finally
                {
                    mutex.ReleaseMutex();
                }
            }

            return (int)exitcode;
        }
    }
}
