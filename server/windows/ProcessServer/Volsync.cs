using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Microsoft.Azure.SiteRecovery.ProcessServer;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Config;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Monitoring;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Tracing;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils;

namespace ProcessServer
{
    internal class Volsync : IDisposable
    {
        private readonly int m_volsyncChildInstances;

        private IEnumerable<Task> m_volsyncChildTasks;

        private const string PERF_LOG_FILENAME = @"perf.log";

        private const string PERF_TXT_FILENAME = @"perf.txt";

        private const string SOURCE_FILE_PREFIX = @"completed_diff";

        private const string TARGET_FILE_PREFIX = @"completed_ediff";

        private const string TEMP_FILE_PREFIX = @"_tmp_";

        private const string DIFF_SUBSTR = @"_diff_";

        private const string TAG_SUBSTR = "tag_";

        private const string TSO_SUBSTR = "tso_";

        private const string PERF_PREFIX = "Perf Metric V2";

        private const string GZIP_EXTENSION = ".gz";

        private const string DAT_EXTENSION = ".dat";

        public Volsync()
        {
            m_volsyncChildInstances = PSInstallationInfo.Default.GetMaxTmid();
        }

        /// <summary>
        /// Main volsync parent task
        /// </summary>
        /// <param name="ct">Cancellation token</param>
        /// <returns>A task which can be monitored by Process server</returns>
        internal async Task RunVolsyncParent(CancellationToken ct)
        {
            await TaskUtils.RunTaskForAsyncOperation(
                Tracers.PSVolsyncMain,
                "VolsyncParent",
                ct,
                async (mainCancToken) =>
                {
                    m_volsyncChildTasks = Enumerable.Range(1, m_volsyncChildInstances).Select(tmid =>
                    {
                        return RunVolsyncChild(tmid, mainCancToken);
                    });

                    await Task.WhenAll(m_volsyncChildTasks).ConfigureAwait(false);
                }).ConfigureAwait(false); 
        }

        /// <summary>
        /// Volsync child tasks
        /// These tasks will run continuously, until the service is stopped
        /// </summary>
        /// <param name="tmid">tmid for the volsync child process</param>
        /// <param name="ct">Cancellation token</param>
        /// <returns>Volsync child task, which can be monitored by volsync parent</returns>
        private async Task RunVolsyncChild(int tmid, CancellationToken ct)
        {
            Tracers.PSVolsyncChild.TraceAdminLogV2Message(TraceEventType.Information,
                $"Child {tmid} started");
            try
            {
                Stopwatch volsyncExecutionSW = new Stopwatch();

                while (true)
                {
                    if (ct.IsCancellationRequested)
                        ct.ThrowIfCancellationRequested();
                    volsyncExecutionSW.Restart();
                    Synchronize(tmid, ct);
                    TimeSpan elpsedTime = volsyncExecutionSW.Elapsed;
                    if (elpsedTime < Settings.Default.VosyncRunInterval)
                    {
                        await Task.Delay(Settings.Default.VosyncRunInterval.Subtract(elpsedTime), ct).
                            ConfigureAwait(false);
                    }
                }
            }
            catch (OperationCanceledException) when (ct.IsCancellationRequested)
            {
                Tracers.PSVolsyncChild.TraceAdminLogV2Message(
                    TraceEventType.Error, tmid,
                    "Operation cancelled for volsync child in {0} function",
                    nameof(RunVolsyncChild)
                    );
                throw;
            }
            catch (Exception ex)
            {
                Tracers.PSVolsyncChild.TraceAdminLogV2Message(
                    TraceEventType.Error, tmid,
                    "Function {0} failed with exception {1}{2}",
                    nameof(RunVolsyncChild), Environment.NewLine, ex
                    );
            }
        }

        /// <summary>
        /// This function fetches the settings for each volsync child,
        /// and then runs diffdync/resync functionality for that pair
        /// as applicable
        /// </summary>
        /// <param name="tmid">tmid for the pair</param>
        /// <param name="ct">Cancellation Token</param>
        private void Synchronize(int tmid, CancellationToken ct)
        {
            if (ct.IsCancellationRequested)
                ct.ThrowIfCancellationRequested();

            //function to handle diffsync for any pair
            void DiffsyncHandler(IProcessServerPairSettings pair)
            {
                TaskUtils.RunAndIgnoreException(() =>
                {
                    if (ProcessDiffsyncPairsAsync(pair, ct).GetAwaiter().GetResult())
                    {
                        return;
                    }

                    if (IsResyncNeeded(pair))
                    {
                        Tracers.PSVolsyncChild.TraceAdminLogV2Message(TraceEventType.Error, tmid,
                            "Failed to process sync files for pair {0} before completing resync",
                            GetPairIdentityString(pair)
                            );
                    }
                    else
                    {
                        Tracers.PSVolsyncChild.TraceAdminLogV2Message(TraceEventType.Error, tmid,
                            "Failed to process sync files for pair {0}",
                            GetPairIdentityString(pair)
                            );
                    }
                }, Tracers.PSVolsyncChild, tmid);
            }

            // function to handle resync for any pair
            void ResyncHandler(IProcessServerPairSettings pair)
            {
                TaskUtils.RunAndIgnoreException(() =>
                {
                    if (!IsResyncNeeded(pair))
                    {
                        return;
                    }

                    if (!ProcessResyncPairsAsync(pair, ct).GetAwaiter().GetResult())
                    {
                        Tracers.PSVolsyncChild.TraceAdminLogV2Message(TraceEventType.Error, tmid,
                            "Failed to process resync files for pair {0}",
                            GetPairIdentityString(pair)
                            );
                    }
                }, Tracers.PSVolsyncChild, tmid);
            }

            TaskUtils.RunAndIgnoreException(() =>
            {
                var psSettings = ProcessServerSettings.GetCachedSettings();

                if (psSettings == null)
                {
                    Tracers.PSVolsyncChild.TraceAdminLogV2Message(TraceEventType.Information,
                        tmid,
                        "Unable to get process server settings.");

                    return;
                }

                if (psSettings.Pairs == null || psSettings.Pairs.Count() == 0)
                {
                    // Always log in debug level, otherwise it will flood the logs
                    Tracers.PSVolsyncChild.DebugAdminLogV2Message(
                        tmid,
                        "Process Server settings does not have pair settings.");

                    return;
                }

                List<IProcessServerPairSettings> diffSyncPairList = new List<IProcessServerPairSettings>();
                List<IProcessServerPairSettings> resyncPairList = new List<IProcessServerPairSettings>();

                psSettings.Pairs.ToList().ForEach(pair =>
                {
                    if ((uint)tmid != pair.tmid)
                    {
                        return;
                    }

                    if (pair.ExecutionState == ExecutionState.Unconfigured ||
                    pair.ExecutionState == ExecutionState.UnconfiguredWaiting)
                    {
                        return;
                    }

                    if (pair.FarlineProtected != FarLineProtected.Protected)
                    {
                        Tracers.PSVolsyncChild.TraceAdminLogV2Message(
                            TraceEventType.Verbose, tmid,
                            "The disk corresponding to pair {0} is not protected",
                            GetPairIdentityString(pair)
                            );
                        return;
                    }

                    diffSyncPairList.Add(pair);
                    if (pair.DoResync == 1)
                    {
                        resyncPairList.Add(pair);
                    }

                    if (ct.IsCancellationRequested)
                        ct.ThrowIfCancellationRequested();
                });

                // if this setting is set, then run all pairs in parallel
                if (Settings.Default.ProcessPairsInParallel)
                {
                    Parallel.ForEach(diffSyncPairList, DiffsyncHandler);
                    Parallel.ForEach(resyncPairList, ResyncHandler);
                }
                else
                {
                    diffSyncPairList.ForEach(DiffsyncHandler);
                    resyncPairList.ForEach(ResyncHandler);
                }
            }, Tracers.PSVolsyncChild, tmid);
        }

        /// <summary>
        /// Main function that handles the diffsync functionality
        /// for all pairs
        /// </summary>
        /// <param name="pair">Process server pair</param>
        /// <param name="ct">Cancellation Token</param>
        /// <returns>True for success, false otherwise</returns>
        private async Task<bool> ProcessDiffsyncPairsAsync(IProcessServerPairSettings pair, CancellationToken ct)
        {
            if (ct.IsCancellationRequested)
                ct.ThrowIfCancellationRequested();

            bool shouldMonitorRPO = false;
            bool rpoValid = true;
            IEnumerable<string> sortedFiles;

            Tracers.PSVolsyncChild.TraceAdminLogV2Message(
                TraceEventType.Verbose, (int)pair.tmid,
                "Source and Target directories are {0} and {1}",
                pair.SourceDiffFolder, pair.TargetDiffFolder
                );

            // if source directory does not exist create it
            if (!Directory.Exists(pair.SourceDiffFolder))
            {
                await DirectoryUtils.CreateDirectoryAsync(pair.SourceDiffFolder,
                    Settings.Default.Volsync_DirectoryCreationRetryCount,
                    Settings.Default.Volsync_DirectoryCreationRetryInterval,
                    ct);
            }

            // if resync directory does not exist create it
            if (!Directory.Exists(pair.ResyncFolder))
            {
                await DirectoryUtils.CreateDirectoryAsync(pair.ResyncFolder,
                    Settings.Default.Volsync_DirectoryCreationRetryCount,
                    Settings.Default.Volsync_DirectoryCreationRetryInterval,
                    ct);
            }

            // if target directory does not exist create it
            if (!Directory.Exists(pair.TargetDiffFolder))
            {
                await DirectoryUtils.CreateDirectoryAsync(pair.TargetDiffFolder,
                    Settings.Default.Volsync_DirectoryCreationRetryCount,
                    Settings.Default.Volsync_DirectoryCreationRetryInterval,
                    ct);
            }

            if (pair.ExecutionState == ExecutionState.Default ||
            pair.ExecutionState == ExecutionState.Active ||
            pair.ExecutionState == ExecutionState.Waiting)
            {
                shouldMonitorRPO = true;
            }

            Tracers.PSVolsyncChild.TraceAdminLogV2Message(
                TraceEventType.Verbose, (int)pair.tmid,
                "{0} value: {1}",
                nameof(shouldMonitorRPO), shouldMonitorRPO
                );

            if (!string.IsNullOrWhiteSpace(pair.LunId) &&
                !pair.LunId.Equals("0", StringComparison.Ordinal)
                && pair.LunState != LunState.Protected)
            {
                rpoValid = false;
            }

            Tracers.PSVolsyncChild.TraceAdminLogV2Message(
                TraceEventType.Verbose, (int)pair.tmid,
                "{0} value: {1}",
                nameof(rpoValid), rpoValid
                );

            if (ct.IsCancellationRequested)
                ct.ThrowIfCancellationRequested();

            // Get sorted list of files to be moved from source folder to target folder
            try
            {
                sortedFiles = GetSortedFiles(pair.SourceDiffFolder);

                Tracers.PSVolsyncChild.TraceAdminLogV2Message(
                        TraceEventType.Verbose, (int)pair.tmid,
                        "Successfully executed diffdatasort on directory {0}",
                        pair.SourceDiffFolder
                        );
            }
            catch (Exception ex)
            {
                Tracers.PSVolsyncChild.TraceAdminLogV2Message(
                        TraceEventType.Error, (int)pair.tmid,
                        "Failed to execute diffdatasort on directory {0} due to exception {1}",
                        pair.SourceDiffFolder, ex
                        );
                throw;
            }

            // Initialise the monitor file
            MonitorRPOFiles(pair, shouldMonitorRPO, rpoValid, sortedFiles.Count() == 0);

            // if there are no files to move, return
            if (sortedFiles.Count() == 0)
            {
                Tracers.PSVolsyncChild.TraceAdminLogV2Message(
                        TraceEventType.Verbose, (int)pair.tmid,
                        "No diff files found for the directory {0}. Exiting.",
                        pair.SourceDiffFolder
                        );

                if (ct.IsCancellationRequested)
                    ct.ThrowIfCancellationRequested();

                return true;
            }

            return await ProcessDiffFilesAsync(pair, ct, sortedFiles, shouldMonitorRPO, rpoValid);
        }

        /// <summary>
        /// This function moves the diff files from source to target folder
        /// </summary>
        /// <param name="pair">Process Server pair</param>
        /// <param name="ct">Cancellation token</param>
        /// <param name="sortedFiles">List of files to move</param>
        /// <param name="shouldMonitorRPO">Whether rpo should be monitored for this pair</param>
        /// <param name="rpoValid">Whether rpo is valid for the pair</param>
        /// <returns>True for success</returns>
        private async Task<bool> ProcessDiffFilesAsync(IProcessServerPairSettings pair, 
            CancellationToken ct, IEnumerable<string> sortedFiles, 
            bool shouldMonitorRPO, 
            bool rpoValid)
        {
            if (ct.IsCancellationRequested)
                ct.ThrowIfCancellationRequested();

            Stopwatch psw = new Stopwatch();
            psw.Start();
            uint numOfFilesProcessed = 0;
            string currFileName = string.Empty;

            List<string> perfData = new List<string>();
            List<string> compressionData = new List<string>();

            foreach (var file in sortedFiles)
            {
                if (ct.IsCancellationRequested)
                    ct.ThrowIfCancellationRequested();

                string currFile = file.Trim(Environment.NewLine.ToCharArray());
                currFileName = Path.GetFileName(currFile);

                DateTime lastModifiedTime = DateTime.MinValue;
                long uncompressedSize = 0;
                string outputFileName;
                string outputFileNameWithoutExtension;
                string renOutputFileName;
                FileInfo currFileinfo = new FileInfo(currFile);

                if (!currFileinfo.Exists)
                {
                    // if the file is not found then return
                    // Remaining files will be picked in next iteration
                    Tracers.PSVolsyncChild.TraceAdminLogV2Message(
                        TraceEventType.Error, (int)pair.tmid,
                        "File {0} is missing. Exiting.",
                        currFile
                    );
                    return false;
                }

                lastModifiedTime = currFileinfo.LastWriteTimeUtc;
                // initial assumption is that size of uncompressed file is
                // equal to file size
                uncompressedSize = currFileinfo.Length;

                // if there is host based compression or inline compression,
                // and the file extension is .gz, it means that the file is 
                // already compressed by source.
                // So get the actual uncompressed file size
                if (currFileinfo.Extension.Equals(GZIP_EXTENSION, StringComparison.Ordinal))
                {
                    if((pair.EnableCompression == EnableCompression.HostBasedCompression ||
                    pair.EnableCompression == EnableCompression.InLineCompression))
                    {
                        long sizeBeforeCompression = FSUtils.GetUncompressedFileSize(currFile);
                        if (sizeBeforeCompression != 0)
                        {
                            uncompressedSize = sizeBeforeCompression;
                            Tracers.PSVolsyncChild.TraceAdminLogV2Message(
                                TraceEventType.Verbose, (int)pair.tmid,
                                "Uncompressed size for file {0} is {1}",
                                currFile, uncompressedSize
                                );
                        }
                        else
                        {
                            Tracers.PSVolsyncChild.TraceAdminLogV2Message(
                                TraceEventType.Error, (int)pair.tmid,
                                "Failed to get uncompressed size for file {0}",
                                currFile
                                );
                        }
                    }
                    else
                    {
                        Tracers.PSVolsyncChild.TraceAdminLogV2Message(
                                TraceEventType.Error, (int)pair.tmid,
                                "Compression is not set but received compressed file {0}",
                                currFile
                                );
                    }
                }

                // if current item is a directory/softlink, then skip it
                if (currFileinfo.Attributes.HasFlag(FileAttributes.Directory)
                    || currFileinfo.Attributes.HasFlag(FileAttributes.ReparsePoint))
                {
                    Tracers.PSVolsyncChild.TraceAdminLogV2Message(
                            TraceEventType.Error, (int)pair.tmid,
                            "Current item {0} is a directory or reparse point",
                            currFile
                            );
                    continue;
                }

                outputFileName = Path.Combine(pair.TargetDiffFolder, TARGET_FILE_PREFIX + currFileName);
                outputFileNameWithoutExtension = string.Empty;

                if (lastModifiedTime.Equals(DateTime.MinValue) ||
                lastModifiedTime.Equals(DateTime.MaxValue))
                {
                    Tracers.PSVolsyncChild.TraceAdminLogV2Message(
                            TraceEventType.Error, (int)pair.tmid,
                            "Unable to get the last modified time for the file {0}",
                            currFile
                            );
                    return false;
                }

                // Add the file modified time in output filename
                outputFileName = GetFormattedOutputFileName(outputFileName, lastModifiedTime);
                // this variable is required only when compression fails.
                outputFileNameWithoutExtension = outputFileName;

                renOutputFileName = Path.Combine(pair.SourceDiffFolder, TEMP_FILE_PREFIX + Path.GetFileName(outputFileName));
                outputFileName = GetTrimmedPath(outputFileName);
                renOutputFileName = GetTrimmedPath(renOutputFileName);

                // if CX_COMPRESSION is set, and the file is not compressed, then compress the file
                if (currFileinfo.Extension.Equals(DAT_EXTENSION, StringComparison.Ordinal)
                && pair.EnableCompression == EnableCompression.InLineCompression)
                {
                    if (!Path.GetExtension(outputFileName).Equals(GZIP_EXTENSION))
                    {
                        outputFileName += GZIP_EXTENSION;
                        renOutputFileName += GZIP_EXTENSION;
                    }

                    // perl scripts used to compress the file, only when
                    // there is no file with the same name.
                    // But now, the new compressed file will always be created
                    Tracers.PSVolsyncChild.TraceAdminLogV2Message(
                            TraceEventType.Verbose, (int)pair.tmid,
                            "Compressing the file {0} to {1}",
                            currFile, renOutputFileName
                            );

                    // Perform the actual compression
                    if (!FSUtils.CompressFile(currFile, renOutputFileName))
                    {
                        Tracers.PSVolsyncChild.TraceAdminLogV2Message(
                            TraceEventType.Error, (int)pair.tmid,
                            "Failed to compress file {0} to {1}",
                            currFile, renOutputFileName
                            );

                        // if the file deletion fails then there are two cases
                        // 1. Original file is moved to target. In his case,
                        // this stale file will will be stuck until next resync.
                        // 2. If orignal file move also fails, then resync is triggered 
                        // immediately.
                        // So ignore the errors in file deletion
                        FSUtils.DeleteFile(renOutputFileName);

                        renOutputFileName = currFile;
                        outputFileName = outputFileNameWithoutExtension;
                    }
                }
                else
                {
                    // There is no need for compression
                    // So set intermediate file to the original file
                    renOutputFileName = currFile;
                }

                // get the output file stats from intermediate temp file
                FileInfo outfileinfo = new FileInfo(renOutputFileName);
                long compressedSize = outfileinfo.Length;
                // Do not change to UTC time
                lastModifiedTime = outfileinfo.LastWriteTime;

                bool fileMoveStatus = await CreateTargetDiffFileAsync(renOutputFileName, outputFileName, pair, ct);
                // Move the file from source to target
                if (!fileMoveStatus)
                {
                    return false;
                }

                // Delete the original file (and not intermediate file)
                if(!FSUtils.DeleteFile(currFile))
                {
                    return false;
                }

                // Create the tag file, if the diff file name contains _tag_
                await CreateTagFolderAsync(outputFileName, pair);

                // Update the compression stats as necessary
                if (compressedSize != 0 && uncompressedSize != 0)
                {
                    string perfline = string.Join(", ", new string [] { PERF_PREFIX,
                        lastModifiedTime.ToString("ddd MMM dd HH:MM:ss yyyy"), pair.Hostname.ToUpper(), currFile,
                        pair.DeviceId, uncompressedSize.ToString(), compressedSize.ToString()});
                    perfline = DateTimeUtils.GetSecondsAfterEpoch() + ": " + perfline + ", ";

                    // if there is a resync or any other failure, due to which
                    // all the files are not processed, then this data is dropped.
                    perfData.Add(perfline);
                    compressionData.Add(uncompressedSize + "," + compressedSize);

                    // Flush the data in chunks, otherwise all the data is lost in case of failures
                    if (perfData.Count() > Settings.Default.PerfDataFlushThreshold)
                    {
                        LogPerfData(pair, perfData);
                        Tracers.PSVolsyncChild.TraceAdminLogV2Message(
                                            TraceEventType.Verbose, (int)pair.tmid,
                                            "Successfully logged perf data for the pair {0}",
                                            GetPairIdentityString(pair)
                                            );
                        perfData.Clear();
                    }

                    // Flush the data in chunks, otherwise all the data is lost in case of failures
                    if (compressionData.Count() > Settings.Default.CompressionDataFlushThreshold)
                    {
                        LogCompressionData(pair, compressionData);
                        Tracers.PSVolsyncChild.TraceAdminLogV2Message(
                                            TraceEventType.Verbose, (int)pair.tmid,
                                            "Successfully logged compression data for the pair {0}",
                                            GetPairIdentityString(pair)
                                            );
                        compressionData.Clear();
                    }
                }
                else
                {
                    Tracers.PSVolsyncChild.TraceAdminLogV2Message(
                                TraceEventType.Verbose, (int)pair.tmid,
                                "Zero size file {0} found. So not logging the compression parameters",
                                currFile
                                );
                }

                numOfFilesProcessed++;

                Tracers.PSVolsyncChild.TraceAdminLogV2Message(
                                TraceEventType.Verbose, (int)pair.tmid,
                                "Source file {0} processing complete",
                                currFile
                                );
                    
                // If a pair takes more than configurable time to move the files,
                // stop processing the files for that pair.
                // This prevents starvation of other pairs
                if (psw.Elapsed > Settings.Default.MaxBatchProcessingChunk)
                {
                    Tracers.PSVolsyncChild.TraceAdminLogV2Message(
                                TraceEventType.Verbose, (int)pair.tmid,
                                "Processing of the pair {0} took {1} ms. Moving to next pair",
                                GetPairIdentityString(pair), psw.ElapsedMilliseconds
                                );
                    return true;
                }
            }

            Tracers.PSVolsyncChild.TraceAdminLogV2Message(
                TraceEventType.Verbose, (int)pair.tmid,
                        $"{numOfFilesProcessed} files processed in {psw.ElapsedMilliseconds} ms."
                        );

            psw.Stop();

            if (numOfFilesProcessed > 0)
            {
                long fileTime = -1;
                MonitorRpo(pair.RPOFilePath, fileTime, currFileName, pair.tmid);
            }

            //Flush the remaining data
            // Todo: since we don't care if this succeedes or not
            // can we do it in an async way?
            LogPerfData(pair, perfData);
            Tracers.PSVolsyncChild.TraceAdminLogV2Message(
                                TraceEventType.Verbose, (int)pair.tmid,
                                "Successfully logged perf data for the pair {0}",
                                GetPairIdentityString(pair)
                                );

            LogCompressionData(pair, compressionData);
            Tracers.PSVolsyncChild.TraceAdminLogV2Message(
                                TraceEventType.Verbose, (int)pair.tmid,
                                "Successfully logged compression data for the pair {0}",
                                GetPairIdentityString(pair)
                                );

            return true;
        }

        /// <summary>
        /// Moves the diff file from source to destination
        /// </summary>
        /// <param name="sourceDiffFile">The file to be moved</param>
        /// <param name="targetDiffFile">Target diff file</param>
        /// <param name="pair">Process server pair</param>
        /// <param name="ct">Cancellation token</param>
        /// <returns>False if resync is required, but could not be set,
        /// True for all other cases</returns>
        private async Task<bool> CreateTargetDiffFileAsync(string sourceDiffFile, 
            string targetDiffFile, 
            IProcessServerPairSettings pair, 
            CancellationToken ct)
        {
            DiffFileParser fileParams = new DiffFileParser(targetDiffFile);

            // if the end timestamp is less than resync start tag time, 
            // it means the file was created before the start of resync
            // so ignore this file
            if (fileParams.EndTimestamp < (ulong)pair.ResyncStartTagtime)
            {
                Tracers.PSVolsyncChild.TraceAdminLogV2Message(
                                TraceEventType.Error, (int)pair.tmid,
                                "EndTimestamp {0} is less than Resync start tag time {1} for the file {2}",
                                fileParams.EndTimestamp, pair.ResyncStartTagtime, targetDiffFile
                                );
                return true;
            }

            // if the source diff file is not present
            // then ignore the failure
            if (!File.Exists(sourceDiffFile))
            {
                Tracers.PSVolsyncChild.TraceAdminLogV2Message(
                                TraceEventType.Error, (int)pair.tmid,
                                "Unable to find intermediate file {0} to move to target folder",
                                sourceDiffFile
                                );
                return true;
            }

            // if target diff file already exists, then skip moving the file
            // this might happen in cases where the file was already moved,
            // but original file was not deleted
            if (File.Exists(targetDiffFile))
            {
                Tracers.PSVolsyncChild.TraceAdminLogV2Message(
                                TraceEventType.Error, (int)pair.tmid,
                                "Output file {0} already exists",
                                targetDiffFile
                                );
                return true;
            }

            Tracers.PSVolsyncChild.TraceAdminLogV2Message(
                            TraceEventType.Verbose, (int)pair.tmid,
                            "Moving file from {0} to {1}",
                            sourceDiffFile, targetDiffFile
                            );

            // Actual file move
            bool linkstatus = FSUtils.MoveFile(sourceDiffFile, targetDiffFile);

            // if file move is successful then return
            if (linkstatus)
            {
                Tracers.PSVolsyncChild.TraceAdminLogV2Message(
                                TraceEventType.Verbose, (int)pair.tmid,
                                "Successfully created hard link from {0} to {1}",
                                sourceDiffFile, targetDiffFile
                                );
                return true;
            }

            Tracers.PSVolsyncChild.TraceAdminLogV2Message(
                TraceEventType.Error, (int)pair.tmid,
                "Failed to create hard link from {0} to {1}",
                sourceDiffFile, targetDiffFile
                );

            // if ResyncSetCxtimestamp != 0 , then resync is already set for the pair
            // if AutoResyncStartTime != 0, then the pair will automatically resync
            // without manual intervention, if resync is set
            // So don't set resync again
            if (pair.AutoResyncStartTime != 0 && pair.ResyncSetCxtimestamp != 0)
            {
                return true;
            }

            Tracers.PSVolsyncChild.TraceAdminLogV2Message(
                TraceEventType.Information, (int)pair.tmid,
                "Triggering resync for the pair {0}",
                GetPairIdentityString(pair)
                );

            // Set resync for the pair
            const string hardLinkFailureResyncReasonCode = "ProcessServerDifferentialFileHardLinkFailureError";
            bool resyncStatus = await SetResyncAsync(pair.HostId, pair.DeviceId, pair.TargetHostId, pair.TargetDeviceId, pair.tmid, ct,
                hardLinkFailureResyncReasonCode, DateTimeUtils.GetTimeInSecSinceEpoch1970().ToString(),
                sourceDiffFile, targetDiffFile);

            // if resync is not correctly set, then return
            // the file will be moved again in next iteration
            if (!resyncStatus)
            {
                Tracers.PSVolsyncChild.TraceAdminLogV2Message(
                    TraceEventType.Error, (int)pair.tmid,
                    "Failed to resync for the pair {0}",
                    GetPairIdentityString(pair)
                    );

                return false;
            }
            else
            {
                Tracers.PSVolsyncChild.TraceAdminLogV2Message(
                    TraceEventType.Information, (int)pair.tmid,
                    "Successfully set resync for the pair {0}:{1}",
                    pair.HostId, pair.DeviceId
                    );
            }
            return true;
        }

        /// <summary>
        /// Create the tag files if the file moved is a tag file
        /// </summary>
        /// <param name="outputFileName">Diff file</param>
        /// <param name="pair">Process server pair</param>
        /// <returns>true, if the tag file is successfully created, false otherwise</returns>
        private async Task CreateTagFolderAsync(string outputFileName, IProcessServerPairSettings pair)
        {
            string baseFilename = Path.GetFileName(outputFileName);
            // if filename does not contain tag substring, return
            if (!baseFilename.Contains(TAG_SUBSTR))
            {
                return;
            }

            string tagFileName = Path.Combine(pair.TagFolder, baseFilename);
            // create the tag directory if it does not exist
            if (!Directory.Exists(pair.TagFolder))
            {
                await DirectoryUtils.CreateDirectoryAsync(pair.TagFolder,
                    Settings.Default.Volsync_DirectoryCreationRetryCount,
                    Settings.Default.Volsync_DirectoryCreationRetryInterval,
                    CancellationToken.None);
            }

            // Create the tag file.
            FSUtils.CreateAdminsAndSystemOnlyFile(tagFileName);

            Tracers.PSVolsyncChild.TraceAdminLogV2Message(
                    TraceEventType.Verbose, (int)pair.tmid,
                    "Created tag file {0}",
                    tagFileName
                    );
        }

        /// <summary>
        /// Function that handles resync functionality
        /// </summary>
        /// <param name="pair">Process Server pair</param>
        /// <param name="ct">Cancellation token</param>
        /// <returns>True for success</returns>
        private async Task<bool> ProcessResyncPairsAsync(IProcessServerPairSettings pair, CancellationToken ct)
        {
            if (ct.IsCancellationRequested)
                ct.ThrowIfCancellationRequested();

            // this directory is created by diffsync function
            // if the directory is not present then return
            if (!Directory.Exists(pair.ResyncFolder))
            {
                Tracers.PSVolsyncChild.TraceAdminLogV2Message(
                        TraceEventType.Information, (int)pair.tmid,
                        "The resync directory {0} is not created",
                        pair.ResyncFolder
                        );

                return false;
            }

            // jobid is already set
            if (pair.JobId != 0)
            {
                return true;
            }

            // if jobid is zero, then fetch jobid from db
            long dbJobId = await GetJobIdAsync(pair, ct);
            // if db jobid is non zero, then the jobid is already set,
            // but not reflected in settings
            // Wait for the jobid to reflect in settings
            if (dbJobId != 0)
            {
                return true;
            }

            // Clear the source folder
            ClearSourceDirectory(pair.SourceDiffFolder);
            // Clear target and resync folder
            ClearTargetDirectory(pair.ResyncFolder, pair.TargetDiffFolder);

            if (pair.CompatibilityNumber < 550000 || pair.TargetCompatibilityNumber < 550000)
            {
                return true;
            }

            // Invalid case
            if (pair.RestartResyncCounter < 0)
            {
                Tracers.PSVolsyncChild.TraceAdminLogV2Message(
                TraceEventType.Error, (int)pair.tmid,
                        "Invalid restart resync counter {0}",
                        pair.RestartResyncCounter);
                return false;
            }
            // Initial replication
            else if (pair.RestartResyncCounter == 0)
            {
                await UpdateJobIdAsync(pair, ct);
            }
            // Resync
            else
            {
                if (!pair.ReplicationCleanupOptions.Equals("0", StringComparison.OrdinalIgnoreCase)         // Default cleanup option
                    && pair.ReplicationStatus == 42                                                         // restart resync cleanup
                    && (ulong)pair.TargetLastHostUpdateTime > pair.ResyncStartTime)
                {
                    await UpdateJobIdAsync(pair, ct);
                }
                else
                {
                    return true;
                }
            }

            // resync files with prefix tmp_ are no more used
            // if these files are present, then cxps verion is very old
            // just log these files and continue
            var unsupportedFileInfo = FSUtils.GetFileList(pair.ResyncFolder,
                "tmp_completed_sync_*.dat",
                SearchOption.TopDirectoryOnly);
            if (unsupportedFileInfo != null && unsupportedFileInfo.Count() > 0)
            {
                // Log only one filename to reduce excessive logging
                string files = unsupportedFileInfo.First().FullName;
                Tracers.PSVolsyncChild.TraceAdminLogV2Message(
                TraceEventType.Information, (int)pair.tmid,
                        "Found {0} unsupported files during resync. Sample name : {1}",
                        unsupportedFileInfo.Count(), files
                        );
            }

            return true;
        }

        /// <summary>
        /// Get the job id from cs db for a pair
        /// </summary>
        /// <param name="pair">Process server pair</param>
        /// <param name="ct">Cancellation token</param>
        /// <returns>jobid for the pair</returns>
        private async Task<long> GetJobIdAsync(IProcessServerPairSettings pair, CancellationToken ct)
        {
            // create a new object every time, to prevent race conditions
            // between different threads.
            using (var csApiStubs = MonitoringApiFactory.GetPSMonitoringApiStubs())
            {
                string result = string.Empty;
                try
                {
                    result = await csApiStubs.GetPsConfigurationAsync(pair.PairId.ToString(), ct);
                }
                catch (Exception ex)
                {
                    Tracers.PSVolsyncChild.TraceAdminLogV2Message(
                        TraceEventType.Error, (int)pair.tmid,
                        "Failed to get jobid for the pair {0} with exception {1}{2}",
                        GetPairIdentityString(pair),
                        Environment.NewLine,
                        ex);
                    return 0;
                }
                if (string.IsNullOrWhiteSpace(result))
                    return 0;
                if (!long.TryParse(result, out long dbJobId))
                    return 0;
                return dbJobId;
            }
        }

        /// <summary>
        /// Sets a new jobid in csdb for a pair
        /// </summary>
        /// <param name="pair">Process Server pair</param>
        /// <param name="ct">Cancellation token</param>
        private async Task UpdateJobIdAsync(IProcessServerPairSettings pair, CancellationToken ct)
        {
            // Always use formatted device names, otherwise csapi fails
            string srcVolume = FSUtils.FormatDeviceName(pair.DeviceId);
            string destVolume = FSUtils.FormatDeviceName(pair.TargetDeviceId);

            JobIdUpdateItem jobIdUpdateItem = new JobIdUpdateItem()
            {
                srcHostId = pair.HostId,
                destHostId = pair.TargetHostId,
                srcHostVolume = srcVolume,
                destHostVolume = destVolume,
                tmid = pair.tmid,
                restartResyncCounter = (long)pair.RestartResyncCounter
            };

            // always create a new object, since this function can be called 
            // simultaneously from multiple threads.
            using (var csApiStubs = MonitoringApiFactory.GetPSMonitoringApiStubs())
            {
                // this throws an exception for failures
                await csApiStubs.UpdateJobIdAsync(jobIdUpdateItem, ct);
            }
        }

        /// <summary>
        /// Set resync for a pair
        /// </summary>
        /// <param name="srcHostId">Hostid of the pair</param>
        /// <param name="srcDeviceId">DeviceId for the pair</param>
        /// <param name="tarHostId">Target Hostid for the pair</param>
        /// <param name="tarDeviceId">TargetDeviceId for the pair</param>
        /// <param name="tmid">tmid for the pair</param>
        /// <param name="ct">Cancellation token</param>
        /// <returns>true if the api succeedes, false otherwise</returns>
        private async Task<bool> SetResyncAsync(string srcHostId, string srcDeviceId, string tarHostId, string tarDeviceId, uint tmid, CancellationToken ct,
            string resyncReasonCode, string detectionTime, string hardlinkFromFile, string hardlinkToFile)
        {
            // always create a new object, since this function can be called 
            // simultaneously from multiple threads.
            using (var csApiStubs = MonitoringApiFactory.GetPSMonitoringApiStubs())
            {
                try
                {
                    await csApiStubs.SetResyncFlagAsync(srcHostId, srcDeviceId, tarHostId, tarDeviceId,
                        resyncReasonCode, detectionTime, hardlinkFromFile, hardlinkToFile, ct);
                }
                catch (Exception ex)
                {
                    Tracers.PSVolsyncChild.TraceAdminLogV2Message(
                        TraceEventType.Error, (int)tmid,
                        "Failed to trigger resync for pair {0}:{1} with exception {2}{3}",
                        srcHostId, srcDeviceId, Environment.NewLine, ex);
                    return false;
                }
                return true;
            }
        }

        /// <summary>
        /// Clear the required files in source directory during resync
        /// </summary>
        /// <param name="sourceDiffDir">Directory path</param>
        private void ClearSourceDirectory(string sourceDiffDir)
        {
            if(!Directory.Exists(sourceDiffDir))
            {
                return;
            }

            FSUtils.DeleteFileList(sourceDiffDir, "*.*", SearchOption.TopDirectoryOnly);
        }

        /// <summary>
        /// Clear the required files in target and resync directory during resync
        /// </summary>
        /// <param name="resyncDir">Resync directory</param>
        /// <param name="targetDiffDir">Target directory</param>
        private void ClearTargetDirectory(string resyncDir, string targetDiffDir)
        {
            if (Directory.Exists(targetDiffDir))
            {
                FSUtils.DeleteFileList(targetDiffDir, "*.dat*", SearchOption.TopDirectoryOnly);
            }

            if (Directory.Exists(resyncDir))
            {
                FSUtils.DeleteFileList(resyncDir, "*.dat*", SearchOption.TopDirectoryOnly);

                FSUtils.DeleteFileList(resyncDir, "*.Map", SearchOption.TopDirectoryOnly);
            }
        }

        /// <summary>
        /// Checks if resync is required for any pair
        /// </summary>
        /// <param name="pair">Process server pair</param>
        /// <returns>true, if resync is required for the pair
        /// false otherwise</returns>
        private bool IsResyncNeeded(IProcessServerPairSettings pair)
        {
            if (pair.ResyncStartTime == 0 || pair.ResyncEndTime == 0)
                return true;
            else return false;
        }

        /// <summary>
        /// Gets sorted list of diff files for a given directory
        /// by running diffdatasort
        /// </summary>
        /// <param name="directory">Source diff directory</param>
        /// <returns>List of sorted diff files</returns>
        private IEnumerable<string> GetSortedFiles(string directory)
        {
            DiffDataSort diffDataSortObj = new DiffDataSort(directory, SOURCE_FILE_PREFIX);
            return diffDataSortObj.SortFiles();
        }

        /// <summary>
        /// Initializes the monitor file before moving diff files
        /// </summary>
        /// <param name="pair">Process server pairs</param>
        /// <param name="shouldMonitorRPO">flag to check if file should be updated</param>
        /// <param name="rpoValid">Flag to check if file should be updated</param>
        /// <param name="sortedFilesFlag">Flag to check if there are any files to be moved for the pair</param>
        private void MonitorRPOFiles(IProcessServerPairSettings pair, bool shouldMonitorRPO, bool rpoValid, bool sortedFilesFlag)
        {
            if (!shouldMonitorRPO)
                return;

            string monitorFilePath = pair.RPOFilePath;
            long preFileTimestamp = 0;

            if(File.Exists(monitorFilePath))
            {
                try
                {
                    if (sortedFilesFlag)
                    {
                        string[] file = File.ReadAllLines(monitorFilePath).Where(str => !string.IsNullOrWhiteSpace(str)).ToArray();
                        string fileName = file[0].TrimEnd(Environment.NewLine.ToCharArray());
                        string[] timestamps = file[1].Split(new char[] { ':' });
                        preFileTimestamp = long.Parse(timestamps[0]);

                        if (preFileTimestamp != 0 && rpoValid)
                        {
                            MonitorRpo(monitorFilePath, preFileTimestamp, fileName, pair.tmid);
                        }
                    }
                }
                catch(Exception ex)
                {
                    Tracers.PSVolsyncChild.TraceAdminLogV2Message(
                       TraceEventType.Error, (int)pair.tmid,
                       "Failed to update monitor file {0} with exception {1}",
                       monitorFilePath, ex
                       );
                }
            }
            else
            {
                string fileName = "No File";
                preFileTimestamp = -1;

                if(rpoValid)
                {
                    MonitorRpo(monitorFilePath, preFileTimestamp, fileName, pair.tmid);
                }
            }
        }

        /// <summary>
        /// Updates the statistics for moved files in perf.log
        /// </summary>
        /// <param name="pair">Process Server pair</param>
        /// <param name="perflines">Statictics for moved fiels</param>
        private void LogPerfData(IProcessServerPairSettings pair, IEnumerable<string> perflines)
        {
            if (perflines.Count() > 0)
            {
                TaskUtils.RunAndIgnoreException(() =>
                {
                    string perfFile = Path.Combine(pair.PerfFolder, PERF_LOG_FILENAME);
                    File.AppendAllLines(perfFile, perflines);
                }, Tracers.PSVolsyncChild, (int)pair.tmid);
            }
        }

        /// <summary>
        /// Update the compression stats to perf.txt file
        /// </summary>
        /// <param name="pair">Process server pair</param>
        /// <param name="compressionData">Compression stats for moved files</param>
        private void LogCompressionData(IProcessServerPairSettings pair, IEnumerable<string> compressionData)
        {
            if (compressionData.Count() > 0)
            {
                TaskUtils.RunAndIgnoreException(() =>
                {
                    string perfFile = Path.Combine(pair.PerfFolder, PERF_TXT_FILENAME);
                    File.AppendAllLines(perfFile, compressionData);
                }, Tracers.PSVolsyncChild, (int)pair.tmid);
            }
        }

        /// <summary>
        /// Updates the monitor file with required parameters
        /// </summary>
        /// <param name="monitorFilePath">Monitor file path</param>
        /// <param name="preFileTimestamp">File timestamp</param>
        /// <param name="filename">File Name</param>
        /// <param name="tmid">tmid for the pair</param>
        private void MonitorRpo(string monitorFilePath, long preFileTimestamp, string filename, long tmid)
        {
            List<string> filetypes = new List<string>()
            {
                 "_WE", "_WC", "_ME", "_MC"
            };

            string fileType = string.Empty;

            foreach (var ftype in filetypes)
            {
                if (filename.Contains(ftype))
                {
                    fileType = ftype.Substring(1, 2);
                    break;
                }
            }

            long currTime = DateTimeUtils.GetSecondsAfterEpoch();

            if(preFileTimestamp == -1)
            {
                preFileTimestamp = currTime;
            }

            TaskUtils.RunAndIgnoreException(() =>
            {
                File.WriteAllLines(monitorFilePath,
                    new string[] { filename, string.Join(":", new object[] { preFileTimestamp, currTime, fileType }) });
            }, Tracers.PSVolsyncChild, (int)tmid);
        }

        /// <summary>
        /// Add last modified time to file name
        /// </summary>
        /// <param name="outputFileName">The filename to be modofied</param>
        /// <param name="lastModifiedTime">last modofied time of the fiel</param>
        /// <returns>Filename with modified time</returns>
        private string GetFormattedOutputFileName(string outputFileName, DateTime lastModifiedTime)
        {
            // this check can be done based on extension as well
            if (outputFileName.EndsWith(DAT_EXTENSION, StringComparison.Ordinal))
            {
                outputFileName = outputFileName.Substring(0, outputFileName.Length - DAT_EXTENSION.Length)
                + $"_{DateTimeUtils.GetSecondsSinceEpoch(lastModifiedTime)}{DAT_EXTENSION}";
            }
            else if (outputFileName.EndsWith(DAT_EXTENSION + GZIP_EXTENSION, StringComparison.Ordinal))
            {
                outputFileName = outputFileName.Substring(0, outputFileName.Length -
                    (DAT_EXTENSION.Length + GZIP_EXTENSION.Length))
                    + $"_{DateTimeUtils.GetSecondsSinceEpoch(lastModifiedTime)}{DAT_EXTENSION}{GZIP_EXTENSION}";
            }
            return outputFileName;
        }

        /// <summary>
        /// Removes the path separators from end of the path
        /// </summary>
        /// <param name="path">File/Directory path</param>
        /// <returns>Trimmed paths, without / or \\ at the end</returns>
        private static string GetTrimmedPath(string path)
        {
            char[] PATH_DELIMITER = new char[] { '\\', '/' };
            if (string.IsNullOrWhiteSpace(path))
                return path;
            return path.TrimEnd(PATH_DELIMITER);
        }

        private static string GetPairIdentityString(IProcessServerPairSettings pair)
        {
            return pair.HostId + ":" + pair.DeviceId;
        }

        #region IDisposable Support
        private bool disposedValue = false; // To detect redundant calls

        protected virtual void Dispose(bool disposing)
        {
            if (!disposedValue)
            {
                if (disposing)
                {
                    // TODO: dispose managed state (managed objects).
                }

                // TODO: free unmanaged resources (unmanaged objects) and override a finalizer below.
                // TODO: set large fields to null.

                disposedValue = true;
            }
        }

        // TODO: override a finalizer only if Dispose(bool disposing) above has code to free unmanaged resources.

        ~Volsync()
        {
           Dispose(false);
        }

        // This code added to correctly implement the disposable pattern.
        public void Dispose()
        {
            // Do not change this code. Put cleanup code in Dispose(bool disposing) above.
            Dispose(true);
            // TODO: uncomment the following line if the finalizer is overridden above.
            GC.SuppressFinalize(this);
        }
        #endregion

    }
}
