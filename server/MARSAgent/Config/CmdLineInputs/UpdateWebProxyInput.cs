using RcmAgentCommonLib.Config.CmdLineInputs;
using RcmContract;

namespace MarsAgent.Config.CmdLineInputs
{
    /// <summary>
    /// Contains input for updating web proxy.
    /// </summary>
    public class UpdateWebProxyInput : UpdateWebProxyInputBase
    {
        /// <summary>
        /// Initializes a new instance of the <see cref="UpdateWebProxyInput"/> class.
        /// </summary>
        public UpdateWebProxyInput()
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
