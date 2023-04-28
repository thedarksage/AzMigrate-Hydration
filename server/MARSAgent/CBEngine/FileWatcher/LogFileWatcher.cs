using MarsAgent.CBEngine.Constants;
using System;
using System.IO;
using System.Text.RegularExpressions;
using MarsAgent.LoggerInterface;
using LoggerInterface;
using RcmAgentCommonLib.LoggerInterface;
using MarsAgent.Utilities;
using MarsAgent.CBEngine.Utilities;
using System.Linq;
using System.Threading;

namespace MarsAgent.CBEngine.FileWatcher
{
    /// <summary>
    /// Defines implementation class for CBEngine Logs Filewatcher. 
    /// Responsible for uploading logs to revised dir path to upload to kusto.
    /// </summary>
    public sealed class LogFileWatcher
    {
        /// <summary>
        /// The fileout to look-out for watching on rename event changes.
        /// </summary>
        private const string lookoutfile = "CBEngineCurr.errlog";

        /// <summary>
        /// Wait time in seconds to move file on rename event
        /// </summary>
        private const int waitTimeOnRenameEvent = 10;

        /// <summary>
        /// static file watcher instance.
        /// </summary>
        private static FileSystemWatcher watcher;

        /// <summary>
        /// Used in signaling stop.
        /// </summary>
        private static CancellationToken m_ct;

        /// <summary>
        /// Initializes a new instance of the <see cref="LogFileWatcher"/> class.
        /// </summary>
        public LogFileWatcher(Action heartBeatUpdateCallback,
            RcmAgentCommonLogger<LogContext> LoggerInstance,
            CancellationToken ct)
        {
            m_ct = ct;

            Logger.Instance.LogInfo(
                CallInfo.Site(),
                $"Enabling FileWatcher for Rename Event on CBEngine log file : {lookoutfile}");

            watcher = new FileSystemWatcher(CBEngineConstants.TempLogDirPath);
            watcher.NotifyFilter = NotifyFilters.DirectoryName
                                 | NotifyFilters.FileName;

            watcher.Renamed += OnRenamed;
            watcher.Error += OnError;

            watcher.Filter = lookoutfile;
            watcher.IncludeSubdirectories = false;
            watcher.EnableRaisingEvents = true;
        }

        /// <summary>
        /// Occurs when a file or directory in the specified Path is renamed.
        /// </summary>
        /// <param name="evt"> Provides information about the renaming operation.</param>
        private static void OnRenamed(object sender, RenamedEventArgs evt)
        {
            if (evt != null && evt.OldName == lookoutfile)
            {
                // Waiting for some time to make sure that the lock on CBEngine log file acquired by other processes would be released
                Thread.Sleep(TimeSpan.FromSeconds(waitTimeOnRenameEvent));

                RcmAgentCommonLogger<LogContext> LoggerInstance = TaskLogContext.GetLogContext();
                LoggerInstance.LogVerbose(
                        CallInfo.Site(),
                        $"CBEngine RenameEvent, Old: {evt.OldFullPath} ,  New: {evt.FullPath} ");

                MoveFile(LoggerInstance, evt.Name);
            }
        }

        /// <summary>
        /// This method executes when a stop command is sent to the service by the
        /// MarsAgent Service. 
        /// Responsible for stopping any associated events on file watcher.
        /// </summary>
        public void StopEvents()
        {
            if (watcher != null)
            {
                watcher.EnableRaisingEvents = false;
            }
            else
            {
                Logger.Instance.LogError(
                    CallInfo.Site(),
                    $"CBEngine stop filewatcher event on invalid instatiation of filewatcher.");
            }
        }

        /// <summary>
        /// Occurs in case of any occur during event handling.
        /// </summary>
        /// <param name="e"> Provides information about erroe event.</param>
        private static void OnError(object sender, ErrorEventArgs e) => PrintException(e.GetException());

        private static void PrintException(Exception ex)
        {
            if (ex != null)
            {
                RcmAgentCommonLogger<LogContext> LoggerInstance = TaskLogContext.GetLogContext();

                LoggerInstance.LogException(CallInfo.Site(), ex);
            }
        }

        /// <summary>
        /// Move a cbengine log file from Temp directory to ReadyForUpload directory.
        /// </summary>
        /// <param name="LoggerInstance"> Logging Instance</param>
        /// <param name="srcfilename"> File to move</param>
        private static void MoveFile(RcmAgentCommonLogger<LogContext> LoggerInstance, string srcFileName)
        {
            try
            {
                if (!string.IsNullOrWhiteSpace(srcFileName))
                {
                    m_ct.ThrowIfCancellationRequested();
                    object lockObject = new object();

                    lock (lockObject)
                    {
                        string srcFilePath = Path.Combine(CBEngineConstants.TempLogDirPath, srcFileName);

                        if (File.Exists(srcFilePath))
                        {
                            m_ct.ThrowIfCancellationRequested();
                            var srcFileInfo = new FileInfo(srcFilePath);
                            string currtimeStamp = DateTimeOffset.Now.ToUnixTimeMilliseconds().ToString();
                            string destfilename = string.Concat(srcFileName.Split('.')[0], CBEngineConstants.LogFileNameSeperator, currtimeStamp, Constants.CBEngineConstants.LogFileExtension);
                            string destdirpath = DirectoryUtil.GetLogDirectory(CBEngineConstants.LogDirPath, CBEngineConstants.ReadyForUploadDir);
                            string destfilepath = Path.Combine(destdirpath, destfilename);

                            m_ct.ThrowIfCancellationRequested();
                            File.Move(srcFilePath, destfilepath);
                        }
                    }
                }
            }
            catch (OperationCanceledException) when (m_ct.IsCancellationRequested)
            {
                // Rethrow operation canceled exception if the service is stopped.
                LoggerInstance.LogVerbose(
                    CallInfo.Site(),
                    "Task cancellation exception is throwed");
                throw;
            }
            catch (Exception ex)
            {
                LoggerInstance.LogException(CallInfo.Site(), ex);
            }
        }

        /// <summary>
        /// Responsible for moving already existed files on Temp to ReadyForUpload directory.
        /// </summary>
        /// <param name="LoggerInstance"> Provides information about the current logging context</param>
        public static void MoveExistingFiles(RcmAgentCommonLogger<LogContext> LoggerInstance)
        {
            try
            {
                m_ct.ThrowIfCancellationRequested();
                if (Directory.Exists(CBEngineConstants.TempLogDirPath))
                {
                    Regex rgx = new Regex(@"^CBEngine([0-9]|[1][0-9])\.errlog");
                    DirectoryInfo dirinfo = new DirectoryInfo(CBEngineConstants.TempLogDirPath);
                    var files = dirinfo.EnumerateFiles()
                        .OrderBy(fi => fi.CreationTime);

                    m_ct.ThrowIfCancellationRequested();
                    foreach (var fi in files)
                    {
                        m_ct.ThrowIfCancellationRequested();
                        try
                        {
                            if (rgx != null && rgx.IsMatch(fi.Name))
                            {
                                MoveFile(LoggerInstance, Path.GetFileName(fi.FullName));
                            }
                        }
                        catch (Exception ex)
                        {
                            LoggerInstance.LogException(CallInfo.Site(), ex);
                        }
                    }
                }
            }
            catch (OperationCanceledException) when (m_ct.IsCancellationRequested)
            {
                // Rethrow operation canceled exception if the service is stopped.
                LoggerInstance.LogVerbose(
                    CallInfo.Site(),
                    "Task cancellation exception is throwed");
                throw;
            }
            catch (Exception ex)
            {
                LoggerInstance.LogException(CallInfo.Site(), ex);
            }
        }
    }
}