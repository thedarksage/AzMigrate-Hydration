namespace LinuxCommunicationFramework
{
    /// <summary>
    /// Class specifying linux client constants
    /// </summary>
    internal static class LinuxConstants
    {
        /// <summary>
        /// device file prefix 
        /// </summary>
        public const string DeviceFilePrefix = "/dev";

        /// <summary>
        /// redirects stderr and stdout to specific file
        /// </summary>
        public const string Nulldevice = DeviceFilePrefix + "/null";
        
        /// <summary>
        /// login daemon name
        /// </summary>
        public const string LoginDaemonName = "systemd-logind";

        /// <summary>
        /// Init process is the first process to be started and have process ID 1.
        /// </summary>
        public const long InitProcessId = 1;
    }
}
