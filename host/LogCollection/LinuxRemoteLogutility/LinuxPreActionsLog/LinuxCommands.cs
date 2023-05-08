namespace LinuxCommunicationFramework
{
    /// <summary>
    /// Class specifying various linux commands
    /// </summary>
    internal static class LinuxCommands
    {
        /// <summary>
        ///  print name of current/working directory
        /// </summary>
        public const string Pwd = "pwd";

        /// <summary>
        /// recursively creates a directory
        /// </summary>
        public const string RecursiveMakeDir = "mkdir -p";

        /// <summary>
        /// remove files or directories
        /// </summary>
        public const string Remove = "rm";

        /// <summary>
        /// remove empty directories
        /// </summary>
        public const string RemoveDir = "rmdir";

        /// <summary>
        /// display a line of text
        /// -e options honours formatting like newline
        /// </summary>
        public const string Echo = "echo -e";

        /// <summary>
        /// check file types and compare values
        /// </summary>
        public const string Test = "test";

        /// <summary>
        ///  report a snapshot of the current processes.
        /// </summary>
        public const string Ps = "ps";

        /// <summary>
        /// redirects stderr and stdout to specific file
        /// </summary>
        public const string RedirectStdOutAndErr = "2>{0} 1>&2";

        /// <summary>
        /// returns the exit code of last command
        /// </summary>
        public const string GetLastExitCode = "echo $?";

        /// <summary>
        /// returns process id of last background process
        /// </summary>
        public const string GetLastBgProcessId = "echo $!";

        /// <summary>
        /// check filesystem command
        /// </summary>
        public const string Fsck = "fsck";

        /// <summary>
        /// mount a filesystem
        /// </summary>
        public const string Mount = "mount";

        /// <summary>
        /// unmount a filesystem
        /// </summary>
        public const string Umount = "umount";

        /// <summary>
        /// list block devices command
        /// </summary>
        public const string Lsblk = "lsblk";

        /// <summary>
        /// Create file system command
        /// </summary>
        public const string Mkfs = "mkfs";

        /// <summary>
        /// Shutdown command
        /// </summary>
        public const string ShutDown = "halt";

        /// <summary>
        /// Reboot command
        /// </summary>
        public const string Reboot = "reboot";

        /// <summary>
        /// Dos2Unix command
        /// Converts file from DOS CRLF format to Unix LF.
        /// </summary>
        public const string Dos2Unix = "dos2unix";
    }
}
