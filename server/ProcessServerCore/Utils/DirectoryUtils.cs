using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Tracing;
using System;
using System.Diagnostics;
using System.IO;
using System.Threading;
using System.Threading.Tasks;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils
{
    /// <summary>
    /// Utilities to work with Directories
    /// </summary>
    public static class DirectoryUtils
    {
        /// <summary>
        /// Delete a directory
        /// </summary>
        /// <param name="folderPath">Path of the directory to delete</param>
        /// <param name="recursive">If true, all the contents of the directory are deleted</param>
        /// <param name="useLongPath">If true, the operation uses long path notation</param>
        /// <param name="traceSource">Trace source to use for logging. Pass null, if logging is not required.</param>
        public static void DeleteDirectory(
            string folderPath,
            bool recursive,
            bool useLongPath,
            TraceSource traceSource)
        {
            DeleteDirectory(
                folderPath,
                recursive,
                useLongPath,
                maxRetryCount: 0,
                retryInterval: TimeSpan.Zero,
                traceSource: traceSource);
        }

        /// <summary>
        /// Delete a directory
        /// </summary>
        /// <param name="folderPath">Path of the directory to delete</param>
        /// <param name="recursive">If true, all the contents of the directory are deleted</param>
        /// <param name="useLongPath">If true, the operation uses long path notation</param>
        /// <param name="maxRetryCount">Maximum number of retries to attemp on failure</param>
        /// <param name="retryInterval">Interval between retries</param>
        /// <param name="traceSource">Trace source to use for logging. Pass null, if logging is not required.</param>
        public static void DeleteDirectory(
            string folderPath,
            bool recursive,
            bool useLongPath,
            int maxRetryCount,
            TimeSpan retryInterval,
            TraceSource traceSource)
        {
            DeleteDirectoryAsync(
                folderPath,
                recursive,
                useLongPath,
                maxRetryCount,
                retryInterval,
                traceSource: traceSource,
                ct: CancellationToken.None)
                .GetAwaiter()
                .GetResult();
        }

        /// <summary>
        /// Delete a directory
        /// </summary>
        /// <param name="folderPath">Path of the directory to delete</param>
        /// <param name="recursive">If true, all the contents of the directory are deleted</param>
        /// <param name="useLongPath">If true, the operation uses long path notation</param>
        /// <param name="maxRetryCount">Maximum number of retries to attemp on failure</param>
        /// <param name="retryInterval">Interval between retries</param>
        /// <param name="traceSource">Trace source to use for logging. Pass null, if logging is not required.</param>
        /// <param name="ct">Cancellation token</param>
        /// <returns>Task of the asynchronous operation</returns>
        public static async Task DeleteDirectoryAsync(
            string folderPath,
            bool recursive,
            bool useLongPath,
            int maxRetryCount,
            TimeSpan retryInterval,
            TraceSource traceSource,
            CancellationToken ct)
        {
            if (string.IsNullOrEmpty(folderPath))
                throw new ArgumentNullException(nameof(folderPath));

            var longPath = folderPath;

            if (useLongPath)
            {
                longPath = FSUtils.GetLongPath(folderPath, isFile: false, optimalPath: false);
            }

            traceSource?.TraceAdminLogV2Message(
                TraceEventType.Information,
                "Deleting folder : {0} {1}recursively using path : {2}",
                folderPath, recursive ? "" : "non-", longPath);

            for (int retryCnt = 0; ; retryCnt++)
            {
                try
                {
                    if (Directory.Exists(longPath))
                    {
                        Directory.Delete(longPath, recursive);

                        traceSource?.TraceAdminLogV2Message(
                            TraceEventType.Information,
                            "Successfully {0}recursively deleted the folder : {1}",
                            recursive ? "" : "non-", longPath);
                    }
                    else
                    {
                        traceSource?.TraceAdminLogV2Message(
                            TraceEventType.Information,
                            "Couldn't find the folder : {0} for deletion",
                            longPath);
                    }

                    break;
                }
                catch (Exception ex)
                {
                    traceSource?.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Failed to delete the folder : {0}{1}{2}",
                        longPath, Environment.NewLine, ex);

                    if (retryCnt >= maxRetryCount)
                        throw;

                    traceSource?.TraceAdminLogV2Message(
                        TraceEventType.Information,
                        "Waiting for {0} and retrying ({1}/{2}) the {3}recursive deletion of the folder : {4}",
                        retryInterval,
                        retryCnt + 1,
                        maxRetryCount,
                        recursive ? "" : "non-",
                        longPath);

                    await Task.Delay(retryInterval, ct).ConfigureAwait(false);
                }
            }
        }

        /// <summary>
        /// Creates a directory asynchronously
        /// </summary>
        /// <param name="dir">Directory to be created</param>
        /// <param name="maxRetryCount">Max retries while creating directory</param>
        /// <param name="retryInterval">Interval between two consecutive retries</param>
        /// <param name="ct">Cancellation token</param>
        /// <returns>Task that can be awaited</returns>
        public static async Task CreateDirectoryAsync(string dir, 
            int maxRetryCount,
            TimeSpan retryInterval,
            CancellationToken ct)
        {
            if (string.IsNullOrWhiteSpace(dir))
            {
                throw new ArgumentNullException(nameof(dir));
            }

            DirectoryInfo dirInfo = new DirectoryInfo(dir);
            if (dirInfo.Exists)
            {
                Tracers.Misc.TraceAdminLogV2Message(
                        TraceEventType.Verbose,
                        "The directory {0} already exists",
                        dir
                        );
                return;
            }

            for (int retryCount = 0; ; retryCount++)
            {
                try
                {
                    FSUtils.CreateAdminsAndSystemOnlyDir(dir);
                    Tracers.Misc.TraceAdminLogV2Message(
                            TraceEventType.Information,
                            "Successfully created the directory {0}",
                            dir
                            );
                    break;
                }
                catch (Exception ex)
                {
                    if (retryCount >= maxRetryCount)
                        throw;

                    Tracers.Misc.TraceAdminLogV2Message(
                            TraceEventType.Error,
                            "Failed to create directory {0} after retries {1} with exception {2}{3}",
                            dir,
                            retryCount + 1,
                            Environment.NewLine,
                            ex
                            );
                    retryCount++;

                    await Task.Delay(retryInterval, ct).ConfigureAwait(false);
                }
            }
        }
    }
}
