using System.Diagnostics.CodeAnalysis;

namespace MarsAgent.Config.CmdLineInputs
{
    /// <summary>
    /// Enumeration for all supported actions.
    /// </summary>
    [SuppressMessage(
        "Microsoft.StyleCop.CSharp.NamingRules",
        "SA1300:ElementMustBeginWithUpperCaseLetter",
        Justification = "Cannot modify as it will break existing clients.")]
    public enum SupportedActions
    {
        /// <summary>
        /// Run the service in interactive mode.
        /// </summary>
        runinteractive,

        /// <summary>
        /// Install the service.
        /// </summary>
        install,

        /// <summary>
        /// Configure the service.
        /// </summary>
        configure,

        /// <summary>
        /// Register the service with RCM.
        /// </summary>
        register,

        /// <summary>
        /// Update web proxy information.
        /// </summary>
        updatewebproxy,

        /// <summary>
        /// Test RCM connection.
        /// </summary>
        testrcmconnection,

        /// <summary>
        /// Test protection service connection.
        /// </summary>
        testprotsvcconnection,

        /// <summary>
        /// Unregister the service with RCM.
        /// </summary>
        unregister,

        /// <summary>
        /// Un-configure the service.
        /// </summary>
        unconfigure,

        /// <summary>
        /// Uninstall the service.
        /// </summary>
        uninstall
    }
}
