using ASRSetupFramework;
using System;
using System.IO;

namespace UnifiedAgentUpgrader
{
    public class UnifiedUpgraderConstants
    {
        public static string UpgraderLogPath => Path.Combine(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData),
            UnifiedSetupConstants.LogFolder), "UnifiedAgentUpgrader.log");

        public const string UpgradeJobFilePath = "UpgradeJobFilePath";

        public const string UnifiedAgentExeName = "UnifiedAgent.exe";

        public const string RegisterSourceAgentCmd = "--registersourceagent";

        // To do: sadewang:
        // Have distinct retry count for different operations.
        public const int UpgradeOpsRetryCount = 3;
    }
}
