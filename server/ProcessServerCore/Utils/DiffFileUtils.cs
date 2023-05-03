using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Config;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Tracing;
using System;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Threading;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils
{
    public static class DiffFileUtils
    {
        /// <summary>
        /// Get RPO data for the given pair
        /// </summary>
        /// <param name="pair">Pair Data</param>
        /// <param name="ct">Cancellation Token</param>
        /// <returns>RPO Data</returns>
        public static (int, int, string, ulong, ulong) GetRPOData(
            IProcessServerPairSettings pair, TraceSource traceSource, CancellationToken ct)
        {
            int srcToPSFileUploadStuck = 0;
            int monitorTxtFileExists = 0;

            string lastMoveFile = string.Empty;
            ulong lastMoveFileModTime = 0;
            ulong lastMoveFileProcTime = 0;

            ct.ThrowIfCancellationRequested();

            TaskUtils.RunAndIgnoreException(() =>
            {
                if (File.Exists(pair.RPOFilePath))
                {
                    string[] monitorFileContents = File.ReadAllLines(pair.RPOFilePath);
                    if (monitorFileContents.Count() != 2)
                    {
                        return;
                    }
                    lastMoveFile = monitorFileContents[0];

                    string[] fileUploadTimeStamps = monitorFileContents[1].Split(
                        new char[] { ':' }, StringSplitOptions.None);

                    if (fileUploadTimeStamps.Length != 3)
                    {
                        return;
                    }

                    lastMoveFileModTime = !ulong.TryParse(
                        fileUploadTimeStamps[0], NumberStyles.Integer,
                        CultureInfo.InvariantCulture, out ulong lastFileUploadTime) ?
                        0 : lastFileUploadTime;

                    lastMoveFileProcTime = !ulong.TryParse(
                        fileUploadTimeStamps[1], NumberStyles.Integer,
                        CultureInfo.InvariantCulture, out ulong lastProcessTime) ?
                        0 : lastProcessTime;

                    if (lastMoveFileModTime != 0 &&
                    lastMoveFileProcTime - lastMoveFileModTime >
                    (ulong)PSInstallationInfo.Default.GetSourceUploadBlockedDuration().TotalSeconds)
                    {
                        srcToPSFileUploadStuck = 1;
                    }
                    monitorTxtFileExists = 1;
                }
            }, traceSource);

            return (srcToPSFileUploadStuck, monitorTxtFileExists,
                lastMoveFile, lastMoveFileModTime, lastMoveFileProcTime);
        }

        /// <summary>
        /// Populate default values for diff folder input
        /// </summary>
        /// <returns>DiffFolderInput</returns>
        public static DiffFolderInpt GetDefaultDiffFolderInput()
        {
            DiffFolderInpt diffFolderInput = new DiffFolderInpt
            {
                DiffFilesCount = 0,
                DiffFirstFileName = string.Empty,
                DiffFirstFileTimeStamp = 0,
                DiffFirstTagFileName = string.Empty,
                DiffFirstTagFileTimeStamp = 0,
                DiffLastFileName = string.Empty,
                DiffLastFileTimeStamp = 0,
                DiffLastTagFileName = string.Empty,
                DiffLastTagFileTimeStamp = 0,
                DiffTagFilesCount = 0,
                TotalPendingDiffs = 0
            };

            return diffFolderInput;
        }

        /// <summary>
        /// Get Diff files and tag files count under the given directory
        /// </summary>
        /// <param name="diffFolder">Diff folder path</param>
        /// <param name="pattern">File patterns to filter</param>
        /// <returns>DiffFolderInput</returns>
        public static DiffFolderInpt ProcessDiffFolder(string diffFolder, string pattern, TraceSource traceSource)
        {
            string retVal = string.Empty;
            const string TAG_FILES_PATTERN = "completed_ediffcompleted_diff_tag_P";

            DiffFolderInpt diffFolderInput = GetDefaultDiffFolderInput();

            TaskUtils.RunAndIgnoreException(() =>
            {
                if (!Directory.Exists(diffFolder))
                {
                    traceSource?.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "MonitorTelemetry : diffFolder {0} is not yet created",
                        diffFolder);

                    return;
                }

                var dInfo = new DirectoryInfo(diffFolder);
                var diffFiles = dInfo.EnumerateFiles(
                    pattern, SearchOption.TopDirectoryOnly).OrderBy(file => file.Name);

                ulong numOfDiffFiles = 0;
                ulong numOfTagfiles = 0;

                var diffFilesCount = diffFiles.Count();
                if (diffFilesCount > 0)
                {
                    foreach (var diffFile in diffFiles)
                    {
                        var currDiffFile = diffFile.FullName;

                        if (!FSUtils.IsNonEmptyFile(currDiffFile))
                        {
                            continue;
                        }

                        var fileLastWriteTime = (ulong)DateTimeUtils.GetSecondsSinceEpoch(
                            diffFile.LastWriteTimeUtc);
                        if (numOfDiffFiles == 0)
                        {
                            diffFolderInput.DiffFirstFileName = diffFile.Name;
                            diffFolderInput.DiffFirstFileTimeStamp = fileLastWriteTime;
                        }
                        diffFolderInput.DiffLastFileName = diffFile.Name;
                        diffFolderInput.DiffLastFileTimeStamp = fileLastWriteTime;

                        if (diffFile.Name.Contains(TAG_FILES_PATTERN))
                        {
                            if (numOfTagfiles == 0)
                            {
                                diffFolderInput.DiffFirstTagFileName = diffFile.Name;
                                diffFolderInput.DiffFirstTagFileTimeStamp = fileLastWriteTime;
                            }
                            diffFolderInput.DiffFirstTagFileName = diffFile.Name;
                            diffFolderInput.DiffFirstTagFileTimeStamp = fileLastWriteTime;

                            numOfTagfiles++;
                        }

                        diffFolderInput.TotalPendingDiffs += (ulong)diffFile.Length;
                        numOfDiffFiles++;
                    }
                }

                diffFolderInput.DiffTagFilesCount = numOfTagfiles;
                diffFolderInput.DiffFilesCount = diffFilesCount;
            }, traceSource);

            return diffFolderInput;
        }
    }

    public class DiffFolderInpt
    {
        public int DiffFilesCount;

        public string DiffFirstFileName;

        public ulong DiffFirstFileTimeStamp;

        public string DiffLastFileName;

        public ulong DiffLastFileTimeStamp;

        public ulong TotalPendingDiffs;

        public string DiffFirstTagFileName;

        public ulong DiffFirstTagFileTimeStamp;

        public string DiffLastTagFileName;

        public ulong DiffLastTagFileTimeStamp;

        public ulong DiffTagFilesCount;
    }
}
