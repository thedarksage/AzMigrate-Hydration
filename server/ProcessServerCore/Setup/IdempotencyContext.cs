using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Config;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Native;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Tracing;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils;
using Microsoft.Win32;
using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using System.Security.AccessControl;
using System.Threading;
using System.Threading.Tasks;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.Setup
{
    public sealed class IdempotencyContext : IDisposable
    {
        public string Identifier { get; }

        private readonly SafeHandle m_transaction;
        private readonly FileApi.LockFile m_idempLockFile;

        private readonly Lazy<PSConfiguration> m_psConfiguration;
        private readonly Lazy<CxpsConfig> m_cxpsConfig;

        private readonly ConcurrentBag<string> m_createdFoldersSubKeys = new ConcurrentBag<string>();

        private static class ValueNames
        {
            public const string FullPath = "Full Path";
            public const string LatestFilePath = "Latest File Path";
            public const string BackupFolderPath = "Backup Folder Path";
            public const string LockFilePath = "Lock File Path";
        }

        public IdempotencyContext()
        {
            string idempLckFilePath = PSInstallationInfo.Default.GetIdempotencyLockFilePath();
            m_idempLockFile =
                FileUtils.AcquireLockFileAsync(idempLckFilePath, exclusive: true, appendLckExt: false, ct: CancellationToken.None)
                .GetAwaiter().GetResult();

            RestoreTransactedSavedFilesAsync(exclLockAcquired: true, ct: CancellationToken.None)
                .GetAwaiter().GetResult();

            int randVal = RandUtils.GetNextIntAsync(int.MaxValue, CancellationToken.None)
                .GetAwaiter().GetResult();

            this.Identifier = $"{DateTime.UtcNow.Ticks} - {randVal}";

            // TODO: Make this a setting? Should this be some finite number?
            var timeout = TimeSpan.Zero; // Infinite

            m_transaction = KernelTransactionManager.CreateTransaction(
                timeout, $"Rcm PS Idempotency - {this.Identifier}");

            m_psConfiguration = new Lazy<PSConfiguration>(() => new PSConfiguration(this));
            m_cxpsConfig = new Lazy<CxpsConfig>(() => new CxpsConfig(this));
        }

        public PSConfiguration PSConfiguration => m_psConfiguration.Value;

        public CxpsConfig CxpsConfig => m_cxpsConfig.Value;

        public void AddCreatedFolders(string folderPath, string lockFilePath)
        {
            AddFoldersToDeleteRegKey(folderPath, lockFilePath, revertOnCommit: true);
        }

        public void AddToDeleteFolders(string folderPath, string lockFilePath)
        {
            AddFoldersToDeleteRegKey(folderPath, lockFilePath, revertOnCommit: false);
        }

        private void AddFoldersToDeleteRegKey(string folderPath, string lockFilePath, bool revertOnCommit)
        {
            ThrowIfDisposed();

            if (string.IsNullOrEmpty(folderPath))
                throw new ArgumentNullException(nameof(folderPath), "Folder path is invalid");

            bool use64BitView = Settings.Default.Registry_Use64BitRegView;
            string idempKeySubPath =
                Path.Combine(
                    Settings.Default.Registry_IdempotencyKeySubPath,
                    Settings.Default.Registry_IdempotencyFoldersToDeleteKeySubPath,
                    Path.GetFileName(folderPath));

            // TODO-SanKumar-2006: Should this be a setting? Should this be a finite value?
            TimeSpan timeout = TimeSpan.Zero; // Infinite
            string description = $"Add folder - {this.Identifier} - {Path.GetFileName(folderPath)}";

            using (var addFolderToDelTrans =
                revertOnCommit ? KernelTransactionManager.CreateTransaction(timeout, description) : null)
            {
                using (var reg = Native.Registry.CreateKeyTransacted(
                            Native.Registry.HKEY.HKEY_LOCAL_MACHINE,
                            idempKeySubPath,
                            RegistryRights.WriteKey,
                            addFolderToDelTrans ?? m_transaction,
                            use64BitView))
                {
                    if (revertOnCommit)
                    {
                        m_createdFoldersSubKeys.Add(idempKeySubPath);
                    }

                    reg.SetValue(ValueNames.FullPath, folderPath);

                    if (lockFilePath != null)
                    {
                        reg.SetValue(ValueNames.LockFilePath, lockFilePath);
                    }
                }

                if (addFolderToDelTrans != null)
                {
                    KernelTransactionManager.CommitTransaction(addFolderToDelTrans);
                }
            }
        }

        public RegistryKey GetPSRegistryKeyTransacted(RegistryRights rights)
        {
            ThrowIfDisposed();

            return Native.Registry.OpenKeyTransacted(
                Native.Registry.HKEY.HKEY_LOCAL_MACHINE,
                Settings.Default.Registry_ProcessServerKeySubPath,
                rights,
                m_transaction,
                Settings.Default.Registry_Use64BitRegView);
        }

        public static void PersistFileToBeDeleted(string filePath, string lockFilePath, string backupFolderPath)
        {
            // TODO-SanKumar-2006: Should this be a setting? Should this be a finite value?
            TimeSpan timeout = TimeSpan.Zero; // Infinite
            string description = $"Persist deletion of file - {Path.GetFileName(filePath)}";

            using (var delFileTrans = KernelTransactionManager.CreateTransaction(timeout, description))
            {
                AddFileToBeReplacedInternal(delFileTrans, filePath, null, lockFilePath, backupFolderPath);
                KernelTransactionManager.CommitTransaction(delFileTrans);
            }
        }

        public void AddFileToBeDeleted(string filePath, string lockFilePath, string backupFolderPath)
        {
            ThrowIfDisposed();

            AddFileToBeReplacedInternal(m_transaction, filePath, null, lockFilePath, backupFolderPath);
        }

        public void AddFileToBeReplaced(
            string filePath, string latestFilePath, string lockFilePath, string backupFolderPath, bool revertLatestFileCleanupGuard)
        {
            ThrowIfDisposed();

            if (string.IsNullOrEmpty(latestFilePath))
                throw new ArgumentNullException(nameof(latestFilePath));

            if (revertLatestFileCleanupGuard)
            {
                bool use64BitView = Settings.Default.Registry_Use64BitRegView;
                string latestFileKeySubPath =
                    Path.Combine(
                        Settings.Default.Registry_IdempotencyKeySubPath,
                        Settings.Default.Registry_IdempotencyFilesToReplaceKeySubPath,
                        Path.GetFileName(latestFilePath));

                using (var reg = Native.Registry.OpenKeyTransacted(
                            Native.Registry.HKEY.HKEY_LOCAL_MACHINE,
                            latestFileKeySubPath,
                            RegistryRights.ReadKey,
                            m_transaction,
                            use64BitView))
                {
                    if (reg != null)
                    {
                        // If the key is present, ensure that it's previously marked
                        // for deleting the temp file and if so, delete the registry key.
                        if ((string)reg.GetValue(ValueNames.FullPath) == latestFilePath &&
                            string.IsNullOrEmpty((string)reg.GetValue(ValueNames.LatestFilePath)))
                        {
                            Native.Registry.DeleteKeyTransacted(
                                Native.Registry.HKEY.HKEY_LOCAL_MACHINE,
                                latestFileKeySubPath,
                                m_transaction,
                                use64BitView,
                                ignoreKeyNotFound: false);
                        }
                    }
                }
            }

            AddFileToBeReplacedInternal(m_transaction, filePath, latestFilePath, lockFilePath, backupFolderPath);
        }

        private static void AddFileToBeReplacedInternal(
            SafeHandle transaction,
            string filePath,
            string latestFilePath,
            string lockFilePath,
            string backupFolderPath)
        {
            if (string.IsNullOrEmpty(filePath))
                throw new ArgumentNullException(nameof(filePath));

            bool use64BitView = Settings.Default.Registry_Use64BitRegView;
            string idempKeySubPath =
                Path.Combine(
                    Settings.Default.Registry_IdempotencyKeySubPath,
                    Settings.Default.Registry_IdempotencyFilesToReplaceKeySubPath,
                    Path.GetFileName(filePath));

            using (var reg = Native.Registry.CreateKeyTransacted(
                        Native.Registry.HKEY.HKEY_LOCAL_MACHINE,
                        idempKeySubPath,
                        RegistryRights.WriteKey,
                        transaction,
                        use64BitView))
            {
                reg.SetValue(ValueNames.FullPath, filePath);

                if (latestFilePath != null)
                {
                    reg.SetValue(ValueNames.LatestFilePath, latestFilePath);
                }

                if (lockFilePath != null)
                {
                    reg.SetValue(ValueNames.LockFilePath, lockFilePath);
                }

                if (backupFolderPath != null)
                {
                    reg.SetValue(ValueNames.BackupFolderPath, backupFolderPath);
                }
            }
        }

        private bool m_committed = false;

        public bool Commit()
        {
            ThrowIfDisposed();

            if (m_committed)
            {
                throw new InvalidOperationException(
                    $"{nameof(IdempotencyContext)} ({this.Identifier}) is already committed");
            }

            bool use64BitView = Settings.Default.Registry_Use64BitRegView;

            foreach (var currSubKeyPath in m_createdFoldersSubKeys)
            {
                Native.Registry.DeleteKeyTransacted(
                    Native.Registry.HKEY.HKEY_LOCAL_MACHINE,
                    currSubKeyPath,
                    m_transaction,
                    use64BitView,
                    ignoreKeyNotFound: true); // Previous failed call to commit could've deleted the key.
            }

            using (m_transaction)
            {
                KernelTransactionManager.CommitTransaction(m_transaction);
            }

            m_committed = true;

            // If there's any issue, this idempotent restore operation would be attempted before
            // any other operation starts until it succeeds.
            return TaskUtils.RunAndIgnoreException(
                () => RestoreTransactedSavedFilesAsync(exclLockAcquired: true, ct: CancellationToken.None).GetAwaiter().GetResult(),
                Tracers.Misc);
        }

        public void Rollback()
        {
            if (m_committed)
            {
                throw new InvalidOperationException(
                    $"{nameof(IdempotencyContext)} ({this.Identifier}) is already committed");
            }

            Dispose();
        }

        public override string ToString()
        {
            return (this.Identifier == null) ?
                nameof(IdempotencyContext) :
                $"{nameof(IdempotencyContext)} - {this.Identifier}";
        }

        #region Dispose Pattern

        ~IdempotencyContext()
        {
            Dispose(false);
        }

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        private int m_isDisposed = 0;

        private void ThrowIfDisposed()
        {
            if (m_isDisposed != 0)
                throw new ObjectDisposedException(this.ToString());
        }

        private void Dispose(bool disposing)
        {
            if (Interlocked.CompareExchange(ref m_isDisposed, 1, 0) == 1)
            {
                return;
            }

            if (disposing)
            {
                // Dispose managed resources

                // TODO-SanKumar-2006: These are critical operations that
                // shouldn't fail at any cost. Should we do a failfast here?

                if (!m_committed)
                {
                    TaskUtils.RunAndIgnoreException(
                        () => KernelTransactionManager.RollbackTransaction(m_transaction), Tracers.Misc);
                    DisposeUtils.TryDispose(m_transaction, Tracers.Misc);

                    // If there's any issue, this idempotent restore operation would be attempted before
                    // any other operation starts until it succeeds.
                    TaskUtils.RunAndIgnoreException(
                        () => RestoreTransactedSavedFilesAsync(exclLockAcquired: true, ct: CancellationToken.None).GetAwaiter().GetResult(),
                        Tracers.Misc);
                }

                DisposeUtils.TryDispose(m_idempLockFile, Tracers.Misc);
            }

            // Dispose unmanaged resources
        }

        #endregion Dispose Pattern

        #region Restore

        private static async Task ExecuteRestoreActionAsync(
            SafeHandle transaction,
            string keySubPath,
            Func<RegistryKey, Task> asyncAction)
        {
            bool use64BitView = Settings.Default.Registry_Use64BitRegView;
            string idempKeySubPath =
                Path.Combine(Settings.Default.Registry_IdempotencyKeySubPath, keySubPath);

            using (var idempRegKey = Native.Registry.OpenKeyTransacted(
                Native.Registry.HKEY.HKEY_LOCAL_MACHINE, idempKeySubPath,
                RegistryRights.ReadKey, transaction, use64BitView))
            {
                if (idempRegKey == null)
                    return;

                var subKeyNames = idempRegKey.GetSubKeyNames();

                foreach (var currSubKeyName in subKeyNames)
                {
                    var currSubKeyPath = Path.Combine(idempKeySubPath, currSubKeyName);

                    using (var currSubKey = Native.Registry.OpenKeyTransacted(
                        Native.Registry.HKEY.HKEY_LOCAL_MACHINE, currSubKeyPath,
                        RegistryRights.ReadKey, transaction, use64BitView))
                    {
                        await asyncAction(currSubKey).ConfigureAwait(false);
                    }

                    Native.Registry.DeleteKeyTransacted(
                        Native.Registry.HKEY.HKEY_LOCAL_MACHINE,
                        currSubKeyPath,
                        transaction,
                        use64BitView,
                        ignoreKeyNotFound: false);
                }
            }
        }

        public static async Task RestoreTransactedSavedFilesAsync(bool exclLockAcquired, CancellationToken ct)
        {
            var postCommitMoveTuples = new List<(string latestFilePath, string currBakFilePath)>();

            string idempLckFilePath = PSInstallationInfo.Default.GetIdempotencyLockFilePath();
            using (var idempLock =
                exclLockAcquired ?
                null :
                await FileUtils.AcquireLockFileAsync(idempLckFilePath, exclusive: true, appendLckExt: false, ct: ct).ConfigureAwait(false))
            {
                // TODO: Should we set this to some finite value?
                var timeout = TimeSpan.Zero; // Infinite
                const string identifier = "IdempotentRestoreManagedTransaction";
                using (var transaction = KernelTransactionManager.CreateTransaction(timeout, identifier))
                {
                    await ExecuteRestoreActionAsync(
                        transaction, Settings.Default.Registry_IdempotencyFilesToReplaceKeySubPath,
                        async (RegistryKey subKey) =>
                        {
                            var originalFilePath = (string)subKey.GetValue(ValueNames.FullPath);
                            var latestFilePath = (string)subKey.GetValue(ValueNames.LatestFilePath);
                            var backupFolderPath = (string)subKey.GetValue(ValueNames.BackupFolderPath);
                            var lockFilePath = (string)subKey.GetValue(ValueNames.LockFilePath);

                            if (string.IsNullOrEmpty(originalFilePath))
                                throw new Exception($"{ValueNames.FullPath} can't be empty");

                            using (var lockFile =
                                string.IsNullOrEmpty(lockFilePath) ?
                                null :
                                await FileUtils.AcquireLockFileAsync(lockFilePath, exclusive: true, appendLckExt: false, ct: ct).ConfigureAwait(false))
                            {
                                FSUtils.ParseFilePath(originalFilePath, out string parentDir, out string fileNameWithoutExt, out string fileExt);

                                string prevBakFilePath = FileUtils.GetDefaultPrevBakFilePath(backupFolderPath, parentDir, fileNameWithoutExt, fileExt);
                                string currBakFilePath = FileUtils.GetDefaultCurrBackupFilePath(backupFolderPath, parentDir, fileNameWithoutExt, fileExt);

                                var bakFilesParent = Directory.GetParent(currBakFilePath);
                                if (!bakFilesParent.Exists)
                                {
                                    bakFilesParent.Create(FSUtils.GetAdminAndSystemDirSecurity());
                                }

                                var ticks = DateTime.UtcNow.Ticks;

                                // TODO: Make util
                                prevBakFilePath = Path.Combine(
                                    bakFilesParent.FullName,
                                    $"{ticks}_{Path.GetFileName(prevBakFilePath)}");

                                currBakFilePath = Path.Combine(
                                    bakFilesParent.FullName,
                                    $"{ticks}_{Path.GetFileName(currBakFilePath)}");

                                if (File.Exists(originalFilePath))
                                {
                                    File.Copy(sourceFileName: originalFilePath, destFileName: prevBakFilePath, overwrite: true);
                                    FileUtils.FlushFileBuffers(prevBakFilePath);
                                }

                                if (string.IsNullOrEmpty(latestFilePath))
                                {
                                    // TODO-SanKumar-2006: Should we do, handle open,
                                    // delete (marks for delete), flush and then close handle?
                                    // Would that ensure that the file deletion is completely
                                    // persisted to the file system?
                                    if (File.Exists(originalFilePath))
                                        File.Delete(originalFilePath);
                                }
                                else if (File.Exists(latestFilePath))
                                {
                                    File.Copy(sourceFileName: latestFilePath, destFileName: originalFilePath, overwrite: true);
                                    FileUtils.FlushFileBuffers(originalFilePath);

                                    postCommitMoveTuples.Add((latestFilePath, currBakFilePath));
                                }
                            }
                        }).ConfigureAwait(false);

                    await ExecuteRestoreActionAsync(
                        transaction, Settings.Default.Registry_IdempotencyFoldersToDeleteKeySubPath,
                        async (RegistryKey subKey) =>
                        {
                            var folderPath = (string)subKey.GetValue(ValueNames.FullPath);
                            var lockFilePath = (string)subKey.GetValue(ValueNames.LockFilePath);

                            using (var lockFile =
                                string.IsNullOrEmpty(lockFilePath) ?
                                null :
                                await FileUtils.AcquireLockFileAsync(lockFilePath, exclusive: true, appendLckExt: false, ct: ct).ConfigureAwait(false))
                            {
                                if (Directory.Exists(folderPath))
                                    Directory.Delete(folderPath, recursive: true);
                            }
                        }).ConfigureAwait(false);

                    KernelTransactionManager.CommitTransaction(transaction);
                }

                foreach (var (latestFilePath, currBakFilePath) in postCommitMoveTuples)
                {
                    TaskUtils.RunAndIgnoreException(
                        () =>
                        {
                            if (!File.Exists(latestFilePath))
                                return;

                            File.Move(
                                sourceFileName: latestFilePath, destFileName: currBakFilePath);

                            FileUtils.FlushFileBuffers(currBakFilePath);
                        }, Tracers.Misc);
                }
            }
        }

        #endregion Restore
    }
}
