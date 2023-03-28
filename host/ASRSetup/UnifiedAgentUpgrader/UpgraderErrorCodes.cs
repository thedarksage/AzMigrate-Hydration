namespace UnifiedAgentUpgrader
{
    internal enum UnifiedAgentUpgraderExitCodes
    {
        Unknown = 0,
        Success = 1,
        Warning = 2,
        Failure = 3,
        UpdateAlreadyRunning = 4,
        OutOfMemory = 5,
        ProductNotInstalled = 6,
        InvalidArguments = 7
    }
}
