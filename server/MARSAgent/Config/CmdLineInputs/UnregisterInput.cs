using RcmAgentCommonLib.Config.CmdLineInputs;
using RcmContract;

namespace MarsAgent.Config.CmdLineInputs
{
    /// <summary>
    /// Contains input for service un-registration.
    /// </summary>
    public class UnregisterInput : InputBase
    {
        /// <summary>
        /// Initializes a new instance of the <see cref="UnregisterInput"/> class.
        /// </summary>
        public UnregisterInput()
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
