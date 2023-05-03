using RcmAgentCommonLib.Config.CmdLineInputs;
using RcmContract;

namespace MarsAgent.Config.CmdLineInputs
{
    /// <summary>
    /// Contains input for service un-configuration.
    /// </summary>
    public class UnconfigureInput : InputBase
    {
        /// <summary>
        /// Initializes a new instance of the <see cref="UnconfigureInput"/> class.
        /// </summary>
        public UnconfigureInput()
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
