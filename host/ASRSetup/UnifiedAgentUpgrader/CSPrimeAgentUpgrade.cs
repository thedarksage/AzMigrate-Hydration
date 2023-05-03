using System;
using System.IO;
using ASRSetupFramework;

namespace UnifiedAgentUpgrader
{
    internal class CSPrimeAgentUpgrade : IAgentUpgrade
    {
        private int upgradeExitCode;

        private string ValidationFilepath => Path.Combine(Path.GetDirectoryName(UnifiedUpgraderConstants.UpgraderLogPath), "InstallerErrors.json");

        public CSPrimeAgentUpgrade()
        {
            //No-op
        }

        public int PostActions()
        {
            int completeJobExitCode = CompleteJobToRcm();
            if(completeJobExitCode == 0)
            {
                Trc.Log(LogLevel.Always, "Post Actions completed successfully");
            }
            else
            {
                Trc.Log(LogLevel.Always, "Post Actions failed.");
            }
            return completeJobExitCode;
        }

        public int PreActions()
        {
            Trc.Log(LogLevel.Always, "Pre Actions completed successfully");
            return 0;
        }

        public bool RunUpgrade()
        {
            if(PreActions() != 0)
            {
                PostActions();
                return false;
            }

            if(UpgradeAction() != 0)
            {
                PostActions();
                return false;
            }

            if (PostActions() == 0)
            {
                return true;
            }

            return false;
        }

        public int UpgradeAction()
        {
            upgradeExitCode = UpgradeMobitlityService();
            if(upgradeExitCode != (int)SetupHelper.UASetupReturnValues.Successful &&
                upgradeExitCode != (int)SetupHelper.UASetupReturnValues.SuccessfulRecommendedReboot &&
                upgradeExitCode != (int)SetupHelper.UASetupReturnValues.SucceededWithWarnings)
            {
                // Upgrade failed
                // so fail the entire upgrade job.
                Trc.Log(LogLevel.Info, "Agent Upgrade failed with exitcode {0}", upgradeExitCode);
                return -1;
            }

            // Proceed with register sourceagent
            int regAgentExitCode = RegisterSourceAgent();
            if (regAgentExitCode != 0)
            {
                Trc.Log(LogLevel.Info, "Register source agent failed with exitcode {0}", regAgentExitCode);
                upgradeExitCode = regAgentExitCode;
                return regAgentExitCode;
            }

            Trc.Log(LogLevel.Always, "Upgrade action completed successfully");
            return 0;
        }

        public int UploadLogs()
        {
            string GetExeFilePath()
            {
                return GetAzureRcmCliPath();
            }

            string GetArgs()
            {
                string args = $"--agentupgradelogsupload " +
                    $"--agentupgradelogspath \"{GetAgentUpgradeLogsDirectory()}\" " +
                    $"--agentupgradejobdetails \"{GetAgentUpgradeJobFile()}\" ";
                return args;
            }

            int exitcode = -1;

            try
            {
                Trc.Log(LogLevel.Always, "Starting Agent AutoUpgrade Logs Upload");

                exitcode = CommandExecutor.ExecuteCommand(
                    filePath: GetExeFilePath(),
                    output: out string output,
                    error: out string error,
                    arguments: GetArgs());

                Trc.Log(LogLevel.Always, "Agent AutoUpgrade Logs Upload exitcode : {0}", exitcode);
            }
            catch (Exception ex)
            {
                Trc.Log(LogLevel.Error, "Failed to execute the command {0} {1} with exception {2}",
                    GetExeFilePath(), GetArgs(), ex);
                exitcode = -1;
            }

            return exitcode;
        }

        private string GetAzureRcmCliPath()
        {
            return Path.Combine(SetupHelper.GetAgentInstalledLocation(),
                UnifiedSetupConstants.AzureRCMCliExeName);
        }

        private int UpgradeMobitlityService()
        {
            string GetArgs()
            {
                string args = "/Role \"MS\" " +
                    "/Platform VmWare " +
                    "/Silent " +
                    "/InstallationType Upgrade " +
                    "/CSType CSPrime " +
                    $"/ValidationsOutputJsonFilePath \"{ValidationFilepath}\"";
                return args;
            }

            string GetFilename()
            {
                return UnifiedUpgraderConstants.UnifiedAgentExeName;
            }

            int exitcode;
            try
            {
                Trc.Log(LogLevel.Always, "Starting Agent upgrade");

                exitcode = CommandExecutor.ExecuteCommand(
                    filePath: GetFilename(),
                    output: out string output,
                    error: out string error,
                    arguments: GetArgs());

                Trc.Log(LogLevel.Always, "Agent upgrade exitcode : {0}", exitcode);
            }
            catch (Exception ex)
            {
                Trc.Log(LogLevel.Error, "Failed to execute the command {0} {1} with exception {2}",
                    GetFilename(), GetArgs(), ex);
                exitcode = -1;
            }

            return exitcode;
        }

        private int RegisterSourceAgent()
        {
            string GetArgs()
            {
                return UnifiedUpgraderConstants.RegisterSourceAgentCmd;
            }

            string GetFilename()
            {
                return GetAzureRcmCliPath();
            }

            int exitcode = -1;

            int retryCount = 0;
            while (retryCount <= UnifiedUpgraderConstants.UpgradeOpsRetryCount)
            {
                Trc.Log(LogLevel.Always, "Register source agent retry count : {0}", retryCount);
                try
                {
                    Trc.Log(LogLevel.Always, "Starting Register source agent");

                    exitcode = CommandExecutor.ExecuteCommand(
                        filePath: GetFilename(),
                        output: out string output,
                        error: out string error,
                        arguments: GetArgs());

                    Trc.Log(LogLevel.Always, "Register sourceagent exitcode : {0}", exitcode);
                }
                catch (Exception ex)
                {
                    Trc.Log(LogLevel.Error, "Failed to execute the command {0} {1} with exception {2}",
                        GetFilename(), GetArgs(), ex);
                    exitcode = -1;
                }

                if (exitcode == 0) break;
                retryCount++;
            }

            return exitcode;
        }

        private int CompleteJobToRcm()
        {
            string GetFilename()
            {
                return GetAzureRcmCliPath();
            }

            string GetArgs()
            {
                string args = $"--completeagentupgrade --agentupgradeexitcode {upgradeExitCode} " +
                    $"--agentupgradejobdetails \"{GetAgentUpgradeJobFile()}\" " +
                    $"--errorfilepath \"{ValidationFilepath}\"";
                return args;
            }

            int exitcode = -1;

            int retryCount = 0;
            while (retryCount <= UnifiedUpgraderConstants.UpgradeOpsRetryCount)
            {
                Trc.Log(LogLevel.Always, "Rcm job completion retry count : {0}", retryCount);
                try
                {
                    Trc.Log(LogLevel.Always, "Starting Job completion");

                    exitcode = CommandExecutor.ExecuteCommand(
                        filePath: GetFilename(),
                        output: out string output,
                        error: out string error,
                        arguments: GetArgs());

                    Trc.Log(LogLevel.Always, "Complete job exitcode : {0}", exitcode);
                }
                catch (Exception ex)
                {
                    Trc.Log(LogLevel.Error, "Failed to execute the command {0} {1} with exception {2}",
                        GetFilename(), GetArgs(), ex);
                    exitcode = -1;
                }

                if (exitcode == 0) break;
                retryCount++;
            }

            return exitcode;
        }

        private string GetAgentUpgradeJobFile()
        {
            return PropertyBagDictionary.Instance.GetProperty<string>(UnifiedUpgraderConstants.UpgradeJobFilePath);
        }

        private string GetAgentUpgradeLogsDirectory()
        {
            return Path.GetDirectoryName(UnifiedUpgraderConstants.UpgraderLogPath);
        }
    }
}
