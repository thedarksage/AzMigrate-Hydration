using System;

namespace MarsAgent.CBEngine.Constants
{
    public sealed class LogTunables
    {
        /// <summary>
        /// Singleton instance of <see cref="LogTunables"/> used by all callers.
        /// </summary>
        public static readonly LogTunables Instance = new LogTunables();

        /// <summary>
        /// Prevents a default instance of the <see cref="LogTunables"/> class from
        /// being created.
        /// </summary>
        private LogTunables()
        {
        }

        /// <summary>
        /// Gets the log cleanup interval.
        /// </summary>
        public TimeSpan LogCleanupInterval
        {
            get
            {
                return TimeSpan.FromMinutes(60);
            }
        }

        /// <summary>
        /// Gets the log process interval.
        /// </summary>
        public TimeSpan LogProcessInterval
        {
            get
            {
                return TimeSpan.FromMinutes(5);
            }
        }

        /// <summary>
        /// Gets the max number of log directories to process.
        /// </summary>
        public int MaxNumberOfLogDirectoriesToProcess
        {
            get
            {
                return 5;
            }
        }

        /// <summary>
        /// Gets the interval (in seconds) between periodic log upload.
        /// </summary>
        public TimeSpan LogUploadInterval
        {
            get
            {
                return TimeSpan.FromSeconds(60);
            }
        }

        /// <summary>
        /// Gets a value indicating whether to enable compression for uploaded log folders or not.
        /// </summary>
        public bool IsCompressionEnabledForUploadedLogs
        {
            get
            {
                return true;
            }
        }

        /// <summary>
        /// Gets the log retention duration.
        /// </summary>
        public TimeSpan LogRetentionDuration
        {
            get
            {
                return TimeSpan.FromDays(7);
            }
        }

        /// <summary>
        /// Gets a value indicating whether to limit log retention on disk based on size or not.
        /// </summary>
        public bool LimitLogRetentionBasedOnSize
        {
            get
            {
                return false;
            }
        }

        /// <summary>
        /// Gets the maximum size (in bytes) of log retention allowed on disk
        /// if <see cref="LimitLogRetentionBasedOnSize"/> flag is set.
        /// </summary>
        public long LogRetentionMaximumSizeInBytes
        {
            get
            {
                return (long)int.MaxValue;
            }
        }

        /// <summary>
        /// Gets the available disk percentage below which
        /// log pruning based on size will be performed.
        /// Applicable if <see cref="LimitLogRetentionBasedOnSize"/> flag is set.
        /// </summary>
        public int LogPruningAvailableDiskSizePercentageThreshold
        {
            get
            {
                return 20;
            }
        }

        /// <summary>
        /// Gets the available disk size in bytes below which
        /// log pruning based on size will be performed.
        /// Applicable if <see cref="LimitLogRetentionBasedOnSize"/> flag is set.
        /// </summary>
        public ulong LogPruningAvailableDiskSizeInBytesThreshold
        {
            get
            {
                // By default 20 GB in bytes.
                return 21474836480UL;
            }
        }
    }
}
