using System;
using System.IO;
using ASRSetupFramework;

namespace UnifiedAgentUpgrader
{
    public partial class Program
    {
        public static void InitTrcLog()
        {
            string logFolderPath = Path.GetDirectoryName(UnifiedUpgraderConstants.UpgraderLogPath);

            if (!Directory.Exists(logFolderPath))
            {
                Directory.CreateDirectory(logFolderPath);
            }

            Trc.Initialize(
                (LogLevel.Always | LogLevel.Debug | LogLevel.Error | LogLevel.Warn | LogLevel.Info),
                UnifiedUpgraderConstants.UpgraderLogPath);
            Trc.Log(LogLevel.Always, "Application Started");
        }

        private static int Prechecks()
        {
            if (!IsProductInstalled())
            {
                return (int)UnifiedAgentUpgraderExitCodes.ProductNotInstalled;
            }
            return 0;
        }

        private static bool IsProductInstalled()
        {
            if (PrechecksSetupHelper.QueryInstallationStatus(
                    UnifiedSetupConstants.MS_MTProductGUID,
                    out string version,
                    out bool isInstalled) &&
                    !isInstalled)
            {
                Trc.Log(LogLevel.Error, "Microsoft Azure Site Recovery Agent is not installed");
                return false;
            }
            return true;
        }

        private static bool IsValidArgs(string[] args)
        {
            if (args.Length != 2)
            {
                Trc.Log(LogLevel.Error, "Invalid number of command line arguments");
                return false;
            }

            if (!args[0].Equals(UnifiedUpgraderConstants.UpgradeJobFilePath, StringComparison.Ordinal))
            {
                Trc.Log(LogLevel.Error, "The first argument is invalid : {0}", args[0]);
                return false;
            }

            if (args[1].IsNullOrWhiteSpace())
            {
                Trc.Log(LogLevel.Error, "The argument is null or empty : {0}", args[1]);
                return false;
            }
            PropertyBagDictionary.Instance.SafeAdd(UnifiedUpgraderConstants.UpgradeJobFilePath, args[1]);
            return true;
        }
    }
}
