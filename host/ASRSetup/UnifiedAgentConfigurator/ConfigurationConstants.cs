using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace UnifiedAgentConfigurator
{
    /// <summary>
    /// Constants use by configurator.
    /// </summary>
    internal static class ConfigurationConstants
    {
        /// <summary>
        /// Temp passphrase path.
        /// </summary>
        public const string TempPassphrasePath = @"Temp\ASRSetup\Passphrase.txt";

        /// <summary>
        /// HTTP Url scheme.
        /// </summary>
        public const string UrlHttpScheme = "http";

        /// <summary>
        /// No of retries for A2A network operations
        /// registermachine/unregistermachine/registersourceagent/unregistersourceagent
        /// </summary>
        public const int NetOpsRetryCount = 2;
    }
}
