using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Native;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Tracing;
using Microsoft.Win32;
using System;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Security.AccessControl;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils
{
    /// <summary>
    /// Utilties to work with Files
    /// </summary>
    internal static class FileUtils
    {
        /// <summary>
        /// Acquire lock on the lock file corresponding to the file.
        /// </summary>
        /// <param name="actualFilePath">Actual file, whose lock file must be locked</param>
        /// <param name="exclusive">True, if exclusive lock must be acquired</param>
        /// <returns>IDisposable object that must be disposed to release the lock</returns>
        public static FileApi.LockFile AcquireLockFile(string actualFilePath, bool exclusive)
        {
            return
                AcquireLockFileAsync(actualFilePath, exclusive)
                .GetAwaiter()
                .GetResult();
        }

        /// <summary>
        /// Acquire lock on the lock file corresponding to the file.
        /// </summary>
        /// <param name="actualFilePath">Actual file, whose lock file must be locked</param>
        /// <param name="exclusive">True, if exclusive lock must be acquired</param>
        /// <returns>IDisposable object that must be disposed to release the lock</returns>
        public static Task<FileApi.LockFile> AcquireLockFileAsync(string actualFilePath, bool exclusive)
        {
            return AcquireLockFileAsync(actualFilePath, exclusive, CancellationToken.None);
        }

        /// <summary>
        /// Acquire lock on the lock file corresponding to the file.
        /// </summary>
        /// <param name="actualFilePath">Actual file, whose lock file must be locked</param>
        /// <param name="exclusive">True, if exclusive lock must be acquired</param>
        /// <param name="maxWaitTime">Maximum wait time to acquire the lock</param>
        /// <returns>IDisposable object that must be disposed to release the lock</returns>
        public static FileApi.LockFile AcquireLockFile(
            string actualFilePath,
            bool exclusive,
            TimeSpan maxWaitTime)
        {
            return
                AcquireLockFileAsync(actualFilePath, exclusive, maxWaitTime)
                .GetAwaiter()
                .GetResult();
        }

        /// <summary>
        /// Acquire lock on the lock file corresponding to the file.
        /// </summary>
        /// <param name="actualFilePath">Actual file, whose lock file must be locked</param>
        /// <param name="exclusive">True, if exclusive lock must be acquired</param>
        /// <param name="maxWaitTime">Maximum wait time to acquire the lock</param>
        /// <returns>IDisposable object that must be disposed to release the lock</returns>
        public static async Task<FileApi.LockFile> AcquireLockFileAsync(
            string actualFilePath,
            bool exclusive,
            TimeSpan maxWaitTime)
        {
            using (CancellationTokenSource cts = new CancellationTokenSource(maxWaitTime))
            {
                return await
                    AcquireLockFileAsync(actualFilePath, exclusive, cts.Token)
                    .ConfigureAwait(false);
            }
        }

        /// <summary>
        /// Acquire lock on the lock file corresponding to the file.
        /// </summary>
        /// <param name="actualFilePath">Actual file, whose lock file must be locked</param>
        /// <param name="exclusive">True, if exclusive lock must be acquired</param>
        /// <param name="ct">Cancellation token</param>
        /// <returns>IDisposable object that must be disposed to release the lock</returns>
        public static FileApi.LockFile AcquireLockFile(
            string actualFilePath,
            bool exclusive,
            CancellationToken ct)
        {
            return
                AcquireLockFileAsync(actualFilePath, exclusive, ct)
                .GetAwaiter()
                .GetResult();
        }

        /// <summary>
        /// Acquire lock on the lock file.
        /// </summary>
        /// <param name="filePath">File path</param>
        /// <param name="exclusive">True, if exclusive lock must be acquired</param>
        /// <param name="appendLckExt">True, if .lck extension has to be appended</param>
        /// <returns>IDisposable object that must be disposed to release the lock</returns>
        public static FileApi.LockFile AcquireLockFile(string filePath, bool exclusive, bool appendLckExt)
        {
            return
                AcquireLockFileAsync(filePath, exclusive, appendLckExt, CancellationToken.None)
                .GetAwaiter()
                .GetResult();
        }

        /// <summary>
        /// Acquire lock on the lock file corresponding to the file.
        /// </summary>
        /// <param name="actualFilePath">Actual file, whose lock file must be locked</param>
        /// <param name="exclusive">True, if exclusive lock must be acquired</param>
        /// <param name="ct">Cancellation token</param>
        /// <returns>IDisposable object that must be disposed to release the lock</returns>
        public static Task<FileApi.LockFile> AcquireLockFileAsync(
            string actualFilePath,
            bool exclusive,
            CancellationToken ct)
        {
            return AcquireLockFileAsync(actualFilePath, exclusive, appendLckExt: true, ct: ct);
        }

        /// <summary>
        /// Acquire lock on the lock file.
        /// </summary>
        /// <param name="filePath">File path</param>
        /// <param name="exclusive">True, if exclusive lock must be acquired</param>
        /// <param name="appendLckExt">True, if .lck extension has to be appended</param>
        /// <param name="ct">Cancellation token</param>
        /// <returns>IDisposable object that must be disposed to release the lock</returns>
        public static async Task<FileApi.LockFile> AcquireLockFileAsync(
            string filePath,
            bool exclusive,
            bool appendLckExt,
            CancellationToken ct)
        {
            Exception cachedEx = null;
            string lckFilePath = appendLckExt ? (filePath + ".lck") : (filePath);

            for (int retryCnt = 0; retryCnt <= Settings.Default.FileUtils_AcquireLockRetryCnt; retryCnt++)
            {
                if (retryCnt != 0)
                {
                    await Task.Delay(Settings.Default.FileUtils_AcquireLockRetryInterval, ct)
                        .ConfigureAwait(false);
                }

                try
                {
                    string parentDirPath = Path.GetDirectoryName(filePath);
                    if (!Directory.Exists(parentDirPath))
                        FSUtils.CreateAdminsAndSystemOnlyDir(parentDirPath);

                    // Matching the range in boost implementation of file lock that
                    // extends from 0 to -1 (max).
                    return await FileApi.LockFile.LockFileRangeAsync(
                        lckFilePath,
                        offset: 0,
                        length: ulong.MaxValue,
                        waitTillLockAcquired: true,
                        exclusive: exclusive,
                        ct: ct)
                        .ConfigureAwait(false);
                }
                catch (IOException ioEx)
                {
                    Tracers.Misc.TraceAdminLogV2Message(TraceEventType.Warning,
                        "Failed to acquire lock on {0} on attempt {1}.{2}{3}",
                        lckFilePath, retryCnt, Environment.NewLine, ioEx);

                    cachedEx = ioEx;
                }
            }

            throw cachedEx;
        }

        /// <summary>
        /// Reliably save a file along with backup copies of previous and current versions.
        /// </summary>
        /// <param name="filePath">File to be saved</param>
        /// <param name="backupFolderPath">If null, same folder of <paramref name="filePath"/> is used.</param>
        /// <param name="multipleBackups">If true, prefix with ticks to 
        /// individually identify a backup at each instance.</param>
        /// <param name="writeData">Action that generates data to be written</param>
        /// <param name="flush">If true, use FlushFileBuffers() to persist the changes</param>
        public static void ReliableSaveFile(
            string filePath,
            string backupFolderPath,
            bool multipleBackups,
            Action<StreamWriter> writeData,
            bool flush)
        {
            var task = ReliableSaveFileAsync(
                filePath,
                backupFolderPath,
                multipleBackups,
                sw =>
                {
                    writeData?.Invoke(sw);
                    return Task.CompletedTask;
                },
                flush);

            task.GetAwaiter().GetResult();
        }

        public static void ReliableSaveFile(
            string filePath,
            string tmpFilePath,
            string backupFolderPath,
            bool multipleBackups,
            bool flush)
        {
            FSUtils.ParseFilePath(filePath, out string parentDir, out string fileNameWithoutExt, out string fileExt);

            var task = ReliableSaveFileAsync(
                filePath,
                tmpFilePath,
                GetDefaultPrevBakFilePath(backupFolderPath, parentDir, fileNameWithoutExt, fileExt),
                GetDefaultCurrBackupFilePath(backupFolderPath, parentDir, fileNameWithoutExt, fileExt),
                multipleBackups,
                null,
                flush);

            task.GetAwaiter().GetResult();
        }

        /// <summary>
        /// Reliably save a file along with backup copies of previous and current versions.
        /// </summary>
        /// <param name="filePath">File to be saved</param>
        /// <param name="backupFolderPath">If null, same folder of <paramref name="filePath"/> is used.</param>
        /// <param name="multipleBackups">If true, prefix with ticks to 
        /// individually identify a backup at each instance.</param>
        /// <param name="writeDataAsync">Asynchronous action that generates data to be written</param>
        /// <param name="flush">If true, use FlushFileBuffers() to persist the changes</param>
        /// <returns>Asynchronous task</returns>
        public static Task ReliableSaveFileAsync(
            string filePath,
            string backupFolderPath,
            bool multipleBackups,
            Func<StreamWriter, Task> writeDataAsync,
            bool flush)
        {
            FSUtils.ParseFilePath(filePath, out string parentDir, out string fileNameWithoutExt, out string fileExt);

            return ReliableSaveFileAsync(
                filePath,
                GetDefaultTempPath(parentDir, fileNameWithoutExt, fileExt),
                GetDefaultPrevBakFilePath(backupFolderPath, parentDir, fileNameWithoutExt, fileExt),
                GetDefaultCurrBackupFilePath(backupFolderPath, parentDir, fileNameWithoutExt, fileExt),
                multipleBackups,
                writeDataAsync,
                flush);
        }

        public static string GetDefaultTempPath(string parentDir, string fileNameWithoutExt, string fileExt)
            => Path.Combine(parentDir, $"{fileNameWithoutExt}_tmp{fileExt}");

        public static string GetDefaultPrevBakFilePath(string backupFolderPath, string parentDir, string fileNameWithoutExt, string fileExt)
        {
            return string.IsNullOrEmpty(backupFolderPath) ?
                Path.Combine(parentDir, $"{fileNameWithoutExt}_bak_prev{fileExt}") :
                Path.Combine(backupFolderPath, $"{fileNameWithoutExt}_bak_prev{fileExt}");
        }

        public static string GetDefaultCurrBackupFilePath(string backupFolderPath, string parentDir, string fileNameWithoutExt, string fileExt)
        {
            return string.IsNullOrEmpty(backupFolderPath) ?
                Path.Combine(parentDir, $"{fileNameWithoutExt}_bak_curr{fileExt}") :
                Path.Combine(backupFolderPath, $"{fileNameWithoutExt}_bak_curr{fileExt}");
        }

        /// <summary>
        /// Reliably save a file
        /// </summary>
        /// <param name="filePath">File to be saved</param>
        /// <param name="tmpFilePath">Path of temporary file used while writing the data</param>
        /// <param name="prevBakFilePath">Path of the backup file (previous, replaced version). Optional.</param>
        /// <param name="multipleBackups">If true, prefix with ticks to 
        /// individually identify a backup at each instance.</param>
        /// <param name="currBakFilePath">Path of the backup file (current version). Optional.</param>
        /// <param name="writeData">Action that generates data to be written</param>
        /// <param name="flush">If true, use FlushFileBuffers() to persist the changes</param>
        public static void ReliableSaveFile(
            string filePath,
            string tmpFilePath,
            string prevBakFilePath,
            string currBakFilePath,
            bool multipleBackups,
            Action<StreamWriter> writeData,
            bool flush)
        {
            var task = ReliableSaveFileAsync(
                    filePath,
                    tmpFilePath,
                    prevBakFilePath,
                    currBakFilePath,
                    multipleBackups,
                    sw =>
                    {
                        writeData?.Invoke(sw);
                        return Task.CompletedTask;
                    },
                    flush);

            task.GetAwaiter().GetResult();
        }

        /// <summary>
        /// Reliably save a file
        /// </summary>
        /// <param name="filePath">File to be saved</param>
        /// <param name="tmpFilePath">Path of temporary file used while writing the data</param>
        /// <param name="prevBakFilePath">Path of the backup file (previous, replaced version). Optional.</param>
        /// <param name="currBakFilePath">Path of the backup file (current version). Optional.</param>
        /// <param name="multipleBackups">If true, prefix with ticks to 
        /// individually identify a backup at each instance.</param>
        /// <param name="writeDataAsync">Asynchronous action that generates data to be written</param>
        /// <param name="flush">If true, use FlushFileBuffers() to persist the changes</param>
        /// <returns>Asynchronous task</returns>
        public static async Task ReliableSaveFileAsync(
            string filePath,
            string tmpFilePath,
            string prevBakFilePath,
            string currBakFilePath,
            bool multipleBackups,
            Func<StreamWriter, Task> writeDataAsync,
            bool flush)
        {
            if (filePath == null)
            {
                throw new ArgumentNullException(nameof(filePath));
            }
            else if (FSUtils.IsPartiallyQualified(filePath))
            {
                throw new ArgumentException(
                    $"{nameof(filePath)} is partially qualified",
                    nameof(filePath));
            }

            if (tmpFilePath == null)
            {
                throw new ArgumentNullException(nameof(tmpFilePath));
            }
            else if (FSUtils.IsPartiallyQualified(tmpFilePath))
            {
                throw new ArgumentException(
                    $"{nameof(tmpFilePath)} is partially qualified",
                    nameof(tmpFilePath));
            }

            if (prevBakFilePath != null && FSUtils.IsPartiallyQualified(prevBakFilePath))
            {
                throw new ArgumentException(
                    $"{nameof(prevBakFilePath)} is partially qualified",
                    nameof(prevBakFilePath));
            }

            if (currBakFilePath != null && FSUtils.IsPartiallyQualified(currBakFilePath))
            {
                throw new ArgumentException(
                    $"{nameof(currBakFilePath)} is partially qualified",
                    nameof(currBakFilePath));
            }

            // TODO-SanKumar-1909: Move to settings
            const int BufferSize = 4096;

            var fileParent = Directory.GetParent(filePath);
            if (!fileParent.Exists)
                fileParent.Create(FSUtils.GetAdminAndSystemDirSecurity());

            var tmpFileParent = Directory.GetParent(tmpFilePath);
            if (!tmpFileParent.Exists)
                tmpFileParent.Create(FSUtils.GetAdminAndSystemDirSecurity());

            if (writeDataAsync != null)
            {
                // TODO-SanKumar-2004: Handle long paths
                using (var tmpFileStream = new FileStream(
                    path: tmpFilePath,
                    mode: FileMode.Create,
                    rights: FileSystemRights.Write | FileSystemRights.ChangePermissions,
                    share: FileShare.None,
                    bufferSize: BufferSize,
                    options: FileOptions.None, // TODO-SanKumar-2004: Expose FileOptions.WriteThrough as an option?
                    fileSecurity: FSUtils.GetAdminAndSystemFileSecurity()))
                {
                    using (var tmpFileWriter = new FormattingStreamWriter(
                                    tmpFileStream,
                                    CultureInfo.InvariantCulture,
                                    new UTF8Encoding(encoderShouldEmitUTF8Identifier: false, throwOnInvalidBytes: true),
                                    BufferSize,
                                    leaveOpen: true))
                    {
                        await writeDataAsync(tmpFileWriter).ConfigureAwait(false);
                    }

                    // First flush before rename, so all the new data is persisted as
                    // part of this itself.
                    if (flush)
                    {
                        tmpFileStream.Flush(flushToDisk: true);
                    }
                }
            }
            else if (flush)
            {
                FileUtils.FlushFileBuffers(tmpFilePath);
            }

            var ticks = DateTime.UtcNow.Ticks;

            if (currBakFilePath != null)
            {
                var currBakFileParent = Directory.GetParent(currBakFilePath);
                if (!currBakFileParent.Exists)
                {
                    currBakFileParent.Create(FSUtils.GetAdminAndSystemDirSecurity());
                }

                if (multipleBackups)
                {
                    currBakFilePath = Path.Combine(
                        currBakFileParent.FullName,
                        $"{ticks}_{Path.GetFileName(currBakFilePath)}");
                }

                using (var tmpFileStream = File.OpenRead(tmpFilePath))
                using (var currBakFileStream = new FileStream(
                    path: currBakFilePath,
                    mode: FileMode.Create,
                    rights: FileSystemRights.Write | FileSystemRights.ChangePermissions,
                    share: FileShare.None,
                    bufferSize: BufferSize,
                    options: FileOptions.None, // TODO-SanKumar-2004: Expose FileOptions.WriteThrough as an option?
                    fileSecurity: FSUtils.GetAdminAndSystemFileSecurity()))
                {
                    await
                        tmpFileStream.CopyToAsync(currBakFileStream, BufferSize)
                        .ConfigureAwait(false);
                }
            }

            // ReplaceFile() only when it already exists. Otherwise, FileNotFoundException is thrown.
            if (File.Exists(filePath))
            {
                if (prevBakFilePath != null)
                {
                    var prevBakFileParent = Directory.GetParent(prevBakFilePath);
                    if (!prevBakFileParent.Exists)
                    {
                        prevBakFileParent.Create(FSUtils.GetAdminAndSystemDirSecurity());
                    }

                    if (multipleBackups)
                    {
                        prevBakFilePath = Path.Combine(
                            prevBakFileParent.FullName,
                            $"{ticks}_{Path.GetFileNameWithoutExtension(prevBakFilePath)}{Path.GetExtension(prevBakFilePath)}");
                    }
                }

                File.Replace(
                    sourceFileName: tmpFilePath,
                    destinationFileName: filePath,
                    destinationBackupFileName: prevBakFilePath,
                    ignoreMetadataErrors: false);
            }
            else
            {
                File.Move(sourceFileName: tmpFilePath, destFileName: filePath);
            }

            // Second flush is needed, since the metadata is always cached in NTFS.
            if (flush)
            {
                using (var origFileStream = new FileStream(filePath, FileMode.Open, FileAccess.Write, FileShare.None))
                {
                    origFileStream.Flush(flushToDisk: true);
                }
            }
        }

        public static void FlushFileBuffers(string filePath)
        {
            using (FileStream fs = File.Open(filePath, FileMode.Open))
                fs.Flush(flushToDisk: true);
        }
    }
}
