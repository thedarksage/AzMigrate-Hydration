using RcmAgentCommonLib.Config.CmdLineInputs;
using RcmContract;

namespace MarsAgent.Config.CmdLineInputs
{
    /// <summary>
    /// Contains input for service configuration.
    /// </summary>
    public class ConfigureInput : ConfigureInputBase
    {
        /// <summary>
        /// Initializes a new instance of the <see cref="ConfigureInput"/> class.
        /// </summary>
        public ConfigureInput()
        {
        }

        /// <summary>
        /// Overrides the base ToString for debugging/logging purposes.
        /// </summary>
        /// <returns>The display string.</returns>
        public override string ToString()
        {
            return this.RcmSafeToString();
        }
    }
}
