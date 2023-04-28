namespace MarsAgent.Constants
{
    /// <summary>
    /// Defines well known activity IDs used by the system when not running in the context of an
    /// end to end flow.
    /// </summary>
    public static class WellKnownActivityId
    {
        /// <summary>
        /// Activity ID to be used for logging actions that are running during service
        /// OnStart(), OnStop() and Run() calls.
        /// </summary>
        public const string MarsAgentServiceActivityId =
            "D5F55F8E-8CC8-4C94-98A5-F33B4DC6CD12";

        /// <summary>
        /// Activity ID to be used for cbengine operations
        /// </summary>
        public const string CBEngineServiceActivityId =
            "E0115B67-2094-4F59-8203-B76A86391072";

        /// <summary>
        /// Activity ID to be used for monitoring cbengine logs
        /// </summary>
        public const string CBEngineMonitorLogActivityId =
            "ABF4BEA7-62E7-40B5-BCFC-BDA651E55281";

        /// <summary>
        /// Activity ID to be used for uploading cbengine logs
        /// </summary>
        public const string CBEngineLogUploadActivityId =
            "A2C4B25C-B340-4F8F-AFF5-D22C57A1B030";
    }
}
