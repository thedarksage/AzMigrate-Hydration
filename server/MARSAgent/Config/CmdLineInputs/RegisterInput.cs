using System;
using RcmAgentCommonLib.Config.CmdLineInputs;
using RcmContract;
using MarsAgent.LoggerInterface;
using MarsAgent.Service;

namespace MarsAgent.Config.CmdLineInputs
{
    /// <summary>
    /// Contains input for service registration.
    /// </summary>
    public class RegisterInput : RegisterInputBase<LogContext>
    {
        /// <summary>
        /// Initializes a new instance of the <see cref="RegisterInput"/> class.
        /// </summary>
        public RegisterInput()
            : base(Logger.Instance, ServiceProperties.Instance)
        {
        }

        /// <summary>
        /// Gets or sets the protection service URI.
        /// </summary>
        [Required]
        public string ProtectionServiceUri { get; set; }

        /// <summary>
        /// Overrides the base ToString for debugging/logging purposes.
        /// </summary>
        /// <returns>The display string.</returns>
        public override string ToString()
        {
            return this.RcmSafeToString();
        }

        /// <summary>
        /// Performs custom validation specific to the input.
        /// </summary>
        protected override void CustomValidate()
        {
            if (!Uri.IsWellFormedUriString(this.RcmServiceUri, UriKind.Absolute))
            {
                this.ReportBadInput(
                    Logger.Instance,
                    ServiceProperties.Instance,
                    this.Action,
                    this.RcmServiceUri,
                    nameof(this.RcmServiceUri),
                    typeof(Uri).ToString());
            }

            if (!Uri.IsWellFormedUriString(this.ProtectionServiceUri, UriKind.Absolute))
            {
                this.ReportBadInput(
                    Logger.Instance,
                    ServiceProperties.Instance,
                    this.Action,
                    this.ProtectionServiceUri,
                    nameof(this.ProtectionServiceUri),
                    typeof(Uri).ToString());
            }
        }
    }
}
