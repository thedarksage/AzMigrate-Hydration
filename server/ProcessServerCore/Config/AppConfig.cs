using System;
using System.IO;
using System.Threading;

using IniParserOutput = System.Collections.Generic.IReadOnlyDictionary<
    string,
    System.Collections.Generic.IReadOnlyDictionary<string, string>>;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.Config
{
    /// <summary>
    /// Wrapper object for App.conf INI file in Legacy CS mode
    /// </summary>
    public class AppConfig
    {
        /// <summary>
        /// Singleton object
        /// </summary>
        public static AppConfig Default { get; } = new AppConfig();

        private Lazy<IniParserOutput> m_parsedData;

        /// <summary>
        /// Constructor. Meant only for internal use.
        /// </summary>
        private AppConfig()
        {
            // Since this constructor is used to only initialize static object,
            // keep it light and reliable (no exceptions).

            Reload();
        }

        /// <summary>
        /// Clears the currently cached settings and loads the values from the
        /// file again. Note that the file is parsed and the values are only
        /// loaded at the first access.
        /// </summary>
        public void Reload()
        {
            IniParserOutput ParseAppConfigFile()
            {
                // Don't perform debug logging in this parsing, since the trace
                // listener might be expecting the install path for placing the log
                // file, which will result in a deadlock.

                return IniParser.ParseAsync(
                    Settings.Default.LegacyCS_FullPath_AppConf,
                    IniParserOptions.RemoveDoubleQuotesInValues)
                    .GetAwaiter().GetResult();
            }

            // TODO-SanKumar-1905: If the service gonna run for a long time,
            // caching the exception that occurred once might not be right.
            // No one is calling Reload() at this point.

            // Any load exception would be cached and replayed by the Lazy object

            Interlocked.Exchange(ref m_parsedData, new Lazy<IniParserOutput>(ParseAppConfigFile));
        }

        // TODO-SanKumar-1903: Failback option could be from the assembly path.
        public string InstallationPath =>
                m_parsedData.Value[string.Empty]["INSTALLATION_PATH"];
    }
}
