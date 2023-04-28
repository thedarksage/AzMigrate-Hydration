using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Config;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils;
using Newtonsoft.Json;
using System;
using System.Collections.Generic;
using System.Data;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.Tracing
{
    /// <summary>
    /// Custom trace listener implementation that persists logs in the format
    /// that would be consumed by MARS and uploaded to MDS.
    /// </summary>
    public sealed class MdsLogCutterTraceListener : PSTraceListenerBase
    {
        // Serializer for MDS row consumed by MARS
        private static readonly JsonSerializer s_mdsRowJsonSer =
            JsonUtils.GetStandardSerializer(
                indent: false,
                converters: JsonUtils.GetAllKnownConverters());

        // Initialize Data passed from the Config
        public readonly string FolderPath;
        public readonly string WellKnownRoot;
        public readonly string AbsoluteFolderPath;
        public readonly uint MaxCompletedFilesCount;
        public readonly ulong MaxFileSizeKB;
        public readonly TimeSpan CutInterval;
        public readonly bool CompleteOnClose;

        /// <summary>
        /// Stream writer that writes to the current log file
        /// </summary>
        /// <remarks>
        /// Note that this object is volatile, since it could be set to new
        /// object on another thread
        /// </remarks>
        private volatile FormattingStreamWriter m_currLogSW;

        // TODO-SanKumar-1903: Move to config
        private const int StreamWriterBufferSize = 4096;

        private readonly string m_currFilePath;

        /// <summary>
        /// Synchronizing object for m_currLogSW
        /// </summary>
        private readonly object m_fileWriterLock = new object();

        private readonly CancellationTokenSource m_logCutterCts = new CancellationTokenSource();
        private readonly Task m_logCutterTask;

        /// <summary>
        /// Constructor
        /// </summary>
        /// <param name="initializeData">JSON containing the following info:
        /// 1. FolderPath - Folder to write the traces
        /// 2. MdsTableName - Prefix of the file name and name of the MDS table
        /// 3. WellKnownRoot (optional, if FolderPath is absolute) - Predefined
        /// well-known path (Ex: home_svsystems)
        /// 4. ComponentId - ComponentId passed to MDS (Ex: ProcessServer)
        /// 5. MaxCompletedFilesCount (optional) - Read from settings, if not given
        /// [MdsLogCutterTraceListener_MaxCompletedFilesCount]
        /// 6. MaxFileSizeKB (optional) - Read from settings, if not given
        /// [MdsLogCutterTraceListener_MaxFileSizeKB]
        /// 7. CutInterval (optional) - Read from settings, if not given
        /// [MdsLogCutterTraceListener_CutInterval]
        /// 8. CompleteOnClose (optional) - Complete the current file on close,
        /// if possible and valid within other limits. Default false.
        /// </param>
        /// <remarks>Not implementing the default constructor, this class can't
        /// operate without valid initializeData</remarks>
        public MdsLogCutterTraceListener(string initializeData)
        {
            const string MdsTableNameKey = "MdsTableName";
            const string WellKnownRootKey = "WellKnownRoot";
            const string FolderPathKey = "FolderPath";
            const string ComponentIdKey = "ComponentId";
            const string MaxCompletedFilesCountKey = "MaxCompletedFilesCount";
            const string MaxFileSizeKBKey = "MaxFileSizeKB";
            const string CutIntervalKey = "CutInterval";
            const string CompleteOnCloseKey = "CompleteOnClose";

            #region InitializeData parsing and validation

            var initDict = JsonConvert.DeserializeObject<Dictionary<string, string>>(initializeData);

            initDict.TryGetValue(MdsTableNameKey, out string mdsTableName);
            initDict.TryGetValue(WellKnownRootKey, out this.WellKnownRoot);
            initDict.TryGetValue(FolderPathKey, out this.FolderPath);
            initDict.TryGetValue(ComponentIdKey, out string componentId);

            if (!initDict.TryGetValue(MaxCompletedFilesCountKey, out string tempStr) ||
                !uint.TryParse(tempStr, NumberStyles.Integer, CultureInfo.InvariantCulture, out this.MaxCompletedFilesCount))
            {
                this.MaxCompletedFilesCount =
                    Settings.Default.MdsLogCutterTraceListener_MaxCompletedFilesCount;
            }

            if (!initDict.TryGetValue(MaxFileSizeKBKey, out tempStr) ||
                !ulong.TryParse(tempStr, NumberStyles.Integer, CultureInfo.InvariantCulture, out this.MaxFileSizeKB))
            {
                this.MaxFileSizeKB =
                    Settings.Default.MdsLogCutterTraceListener_MaxFileSizeKB;
            }

            if (!initDict.TryGetValue(CutIntervalKey, out tempStr) ||
                !TimeSpan.TryParse(tempStr, CultureInfo.InvariantCulture, out this.CutInterval))
            {
                this.CutInterval =
                    Settings.Default.MdsLogCutterTraceListener_CutInterval;
            }

            if (!initDict.TryGetValue(CompleteOnCloseKey, out tempStr) ||
                !bool.TryParse(tempStr, out this.CompleteOnClose))
            {
                this.CompleteOnClose = false;
            }

            if (this.MaxFileSizeKB == 0)
                throw new ArgumentException("MaxFileSizeKB must be non-zero", "initializeData");

            if (this.MaxFileSizeKB > long.MaxValue / 1024)
                throw new ArgumentOutOfRangeException("MaxFileSizeKB must not be more than " + long.MaxValue / 1024, "initializeData");

            if (this.MaxCompletedFilesCount > int.MaxValue)
                throw new ArgumentOutOfRangeException("MaxCompletedFilesCount must not be more than " + int.MaxValue, "initializeData");

            if (this.CutInterval <= TimeSpan.Zero)
                throw new ArgumentException("CutInterval must be a non-zero positive interval", "initializeData");

            if ((string.IsNullOrWhiteSpace(componentId)) ||
                (string.IsNullOrWhiteSpace(mdsTableName)) ||
                (string.IsNullOrWhiteSpace(this.WellKnownRoot) &&
                 string.IsNullOrWhiteSpace(this.FolderPath)))
            {
                throw new ArgumentException(
                    initializeData + " doesn't contain required keys with non-empty values",
                    "initializeData");
            }

            MdsTable mdsTable = (MdsTable)Enum.Parse(typeof(MdsTable), mdsTableName, ignoreCase: true);

            base.Initialize(mdsTable, componentId);

            if (!string.IsNullOrWhiteSpace(this.FolderPath))
                this.FolderPath = Environment.ExpandEnvironmentVariables(this.FolderPath);

            if (!string.IsNullOrWhiteSpace(this.WellKnownRoot) &&
                !string.IsNullOrWhiteSpace(this.FolderPath) &&
                Path.IsPathRooted(this.FolderPath))
            {
                throw new ArgumentException(
                    "FolderPath " + this.FolderPath + " must be relative, since " +
                    this.WellKnownRoot + " is declared as WellKnownRoot",
                    "initializeData");
            }

            if (string.IsNullOrWhiteSpace(this.WellKnownRoot) &&
                !Path.IsPathRooted(this.FolderPath))
            {
                throw new ArgumentException(
                    "FolderPath " + this.FolderPath + " must be absolute, " +
                    "since WellKnownRoot is not declared",
                    "initializeData");
            }

            if (string.IsNullOrWhiteSpace(this.WellKnownRoot))
            {
                this.AbsoluteFolderPath = this.FolderPath;
            }
            else
            {
                string wellKnownRootPath;

                switch (this.WellKnownRoot.ToLowerInvariant())
                {
                    case "var":
                        wellKnownRootPath = PSInstallationInfo.Default.GetVarFolderPath();
                        break;

                    default:
                        throw new ArgumentException(
                            "WellKnownRoot: " + this.WellKnownRoot + " - Unknown");
                }

                if (!Directory.Exists(wellKnownRootPath))
                {
                    throw new DirectoryNotFoundException(
                        "Directory " + wellKnownRootPath + " set for WellKnownRoot "
                        + this.WellKnownRoot + " couldn't be found");
                }

                this.AbsoluteFolderPath = string.IsNullOrWhiteSpace(this.FolderPath) ?
                    wellKnownRootPath : Path.Combine(wellKnownRootPath, this.FolderPath);
            }

            m_currFilePath = Path.Combine(this.AbsoluteFolderPath, this.MdsTable + "_current.json");

            #endregion InitializeData parsing and validation

            // Complete log file from previous execution as well as initialize members

            // Swallow the exception, which otherwise would fail the start
            // of the process. Attempt the initialization and cutting at the
            // next interval.
            TaskUtils.RunAndIgnoreException(() => RotateOrTruncateCurrFile(false), Tracers.Misc);

            m_logCutterTask = Task.Run(async () =>
                {
                    for (; ; )
                    {
                        await Task.Delay(this.CutInterval, m_logCutterCts.Token).ConfigureAwait(false);

                        // Swallow the exception and attempt the cutting at the next interval
                        TaskUtils.RunAndIgnoreException(() => RotateOrTruncateCurrFile(false), Tracers.Misc);
                    }
                }, m_logCutterCts.Token);
        }

        private static readonly DateTime UnixEpochBeginTime = new DateTime(1970, 1, 1);

        private static ulong GetUnixEpochTimeUtc()
        {
            return Convert.ToUInt64((DateTime.UtcNow - UnixEpochBeginTime).Ticks);
        }

        private void RotateOrTruncateCurrFile(bool isDisposing)
        {
            if (!isDisposing)
                ThrowIfDisposed();

            lock (m_fileWriterLock)
            {
                FileStream newFileStream = null;

                try
                {
                    // As soon as the rotate call comes in, close the existing
                    // stream writer (if any). This is done to reliably log, as
                    // defined by one example below:
                    // If the stream writer reaches max log size, this method is
                    // invoked. Say there's some trouble in querying the completed
                    // files temporarily and that would mean every line of log
                    // invoking this method, which is intensive of file operations.
                    // Instead resetting the current stream writer, irrespective
                    // of the final result would mean that the log writes would
                    // rather wait for the log cutter task to come in and initialize
                    // the new stream writer object.
                    m_currLogSW?.TryDispose(Tracers.Misc);
                    // Swallow any exception in flushing the remaining
                    // data in this stream writer object. Otherwise,
                    // the cutting logic would be forever stuck here.

                    m_currLogSW = null;

                    // Recursively create the directories
                    Directory.CreateDirectory(this.AbsoluteFolderPath);

                    var completedFilesCount = Directory.EnumerateFiles(
                        this.AbsoluteFolderPath,
                        this.MdsTable + "_completed_*.json",
                        SearchOption.TopDirectoryOnly)
                        .Count();

                    var currFileInfo = new FileInfo(m_currFilePath);
                    FileMode openMode;

                    if (completedFilesCount >= checked((int)this.MaxCompletedFilesCount))
                    {
                        if (currFileInfo.Exists &&
                            currFileInfo.Length > checked((long)this.MaxFileSizeKB * 1024))
                        {
                            openMode = FileMode.Truncate;

                            // TODO-SanKumar-1903: Add a truncation log to the first
                            // line of the file along with parameters from truncated
                            // range of logs such as first and last time.
                        }
                        else
                        {
                            openMode = FileMode.Append;
                        }
                    }
                    else
                    {
                        if (currFileInfo.Exists && currFileInfo.Length > 0)
                        {
                            var completedFilePath = Path.Combine(
                                                this.AbsoluteFolderPath,
                                                string.Format(CultureInfo.InvariantCulture,
                                                    "{0}_completed_{1}.json",
                                                    this.MdsTable, GetUnixEpochTimeUtc()));

                            // TODO-SanKumar-1903: Handle long path
                            File.Move(m_currFilePath, completedFilePath);

                            openMode = FileMode.CreateNew;
                        }
                        else
                        {
                            openMode = FileMode.Append;
                        }
                    }

                    // NOTE-SanKumar-1902: If appending due to max number of files
                    // completed, it might seem unnecessary that we are closing
                    // and opending the same file for append (even the filestream
                    // would be in the exact position that new object seeks to).
                    // But this helps in initializing either failed file creation
                    // or reinitializing botched file stream object (external
                    // handle close).

                    var fs = new FileStream(m_currFilePath, openMode, FileAccess.Write, FileShare.Read);

                    // Auto flush is always set to false, since the flushing (note:
                    // not file system buffers but the Stream writer buffer flushing)
                    // is hanlded by this class.
                    m_currLogSW = new FormattingStreamWriter(
                        fs,
                        CultureInfo.InvariantCulture,
                        new UTF8Encoding(encoderShouldEmitUTF8Identifier: false, throwOnInvalidBytes: true),
                        StreamWriterBufferSize,
                        leaveOpen: false)
                    {
                        AutoFlush = false
                    };
                }
                catch
                {
                    m_currLogSW?.TryDispose(Tracers.Misc);
                    m_currLogSW = null;

                    newFileStream?.TryDispose(Tracers.Misc);

                    throw;
                }
            }
        }

        // This class is thread safe, since it's syncrhonized using the lock object.
        // Note that the TraceSource will still use the global lock, even if the
        // listener is thread safe. To obtain high performance, set UseGlobalLock
        // to false in <system.diagnostics><trace> in the App config xml.
        public override bool IsThreadSafe { get { return true; } }

        public override string ToString()
        {
            return $"{nameof(MdsLogCutterTraceListener)}: {this.Name}";
        }

        #region Dispose Pattern

        private int m_isDisposed = 0;

        private void ThrowIfDisposed()
        {
            if (m_isDisposed != 0)
                throw new ObjectDisposedException(this.ToString());
        }

        protected override void Dispose(bool disposing)
        {
            // Do nothing and return, if already dispose started / completed.
            if (0 != Interlocked.CompareExchange(ref m_isDisposed, 1, 0))
                return;

            if (disposing)
            {
                // Not expected to throw, since no callback is registered.
                m_logCutterCts.Cancel();

                TaskUtils.RunAndIgnoreException(() =>
                {
                    try
                    {
                        m_logCutterTask.Wait();
                    }
                    catch (AggregateException ae)
                    {
                        if (!m_logCutterTask.IsCanceled ||
                            ae.Flatten().InnerExceptions.Any(ex => !(ex is TaskCanceledException) || (ex as TaskCanceledException).Task != m_logCutterTask))
                        {
                            // Rethrowing, so that debugger is broken by ignore exception wrapper
                            throw;
                        }
                    }
                }, Tracers.Misc);

                m_logCutterCts.Dispose();

                bool completionOnCloseSuccessful = false;
                if (this.CompleteOnClose)
                {
                    TaskUtils.RunAndIgnoreException(() =>
                    {
                        RotateOrTruncateCurrFile(true);
                        completionOnCloseSuccessful = true;
                    }, Tracers.Misc);
                }

                lock (m_fileWriterLock)
                {
                    m_currLogSW?.TryDispose(Tracers.Misc);
                }

                // If the rotation above was successful, then delete the
                // current file as it would be empty.
                if (this.CompleteOnClose && completionOnCloseSuccessful)
                {
                    TaskUtils.RunAndIgnoreException(() =>
                    {
                        if (File.Exists(m_currFilePath))
                            File.Delete(m_currFilePath);
                    }, Tracers.Misc);
                }
            }

            base.Dispose(disposing);
        }

        #endregion Dispose Pattern

        protected override void OnTraceEvent(TraceEventCache eventCache, string source, TraceEventType eventType, int id, string format, params object[] args)
        {
            if (m_isDisposed != 0)
                return;

            object rowObj;

            switch (this.MdsTable)
            {
                case MdsTable.InMageAdminLogV2:
                    var adminV2RowObj = InMageAdminLogV2Row.Build(
                        eventCache, this.SystemProperties, source, eventType, null, id, format, args);

                    // Extended properties are stored in a dictionary and 
                    // serialized into a JSON on demand.
                    adminV2RowObj.SerializeExtendedProperties();

                    rowObj = adminV2RowObj;
                    break;

                default:
                    throw new NotImplementedException(this.MdsTable.ToString());
            }

            // TODO-SanKumar-1903: this.TraceOutputOptions.HasFlag(TraceOptions.Callstack)
            // Support logging the stack trace in the extended properties for
            // debugging critical issues without debugger.

            this.WriteJsonLineToFile(rowObj);
        }

        protected override void OnTraceData(TraceEventCache eventCache, string source, TraceEventType eventType, int id, object data1)
        {
            if (m_isDisposed != 0)
                return;

            object rowObj;

            switch (this.MdsTable)
            {
                case MdsTable.InMageAdminLogV2:
                    var adminLogV2TracePkt = data1 as InMageAdminLogV2TracePacket;

                    var adminV2RowObj = InMageAdminLogV2Row.Build(
                        eventCache, this.SystemProperties, source,
                        eventType, adminLogV2TracePkt?.OpContext,
                        id, adminLogV2TracePkt?.FormatString, adminLogV2TracePkt?.FormatArgs);

                    if (adminLogV2TracePkt != null)
                    {
                        adminV2RowObj.DiskId = adminLogV2TracePkt.DiskId;
                        adminV2RowObj.DiskNumber = adminLogV2TracePkt.DiskNumber;
                        adminV2RowObj.ErrorCode = adminLogV2TracePkt.ErrorCode;
                    }

                    // Extended properties are stored in a dictionary and 
                    // serialized into a JSON on demand.
                    adminV2RowObj.SerializeExtendedProperties();

                    rowObj = adminV2RowObj;
                    break;

                case MdsTable.InMageTelemetryPSV2:
                    rowObj = data1 as InMageTelemetryPSV2Row;
                    break;

                default:
                    throw new NotImplementedException(this.MdsTable.ToString());
            }

            // TODO-SanKumar-1903: this.TraceOutputOptions.HasFlag(TraceOptions.Callstack)
            // Support logging the stack trace in the extended properties for
            // debugging critical issues without debugger.

            this.WriteJsonLineToFile(rowObj);
        }

        private void WriteJsonLineToFile(object obj)
        {
            // TODO-SanKumar-1903: Currently all the heavy lifting is done within
            // the lock. The final high performant version should be nearly
            // lock free, where all the serialization and formatting is taken
            // care outside the critical section, which will only cache the
            // final value. Another task must be responsible to flush that buffer
            // on to the current log file, while the second buffer will be rotated
            // to cache the future logs.

            lock (this.m_fileWriterLock)
            {
                // This object would be initialized in the next cut interval.
                if (m_currLogSW == null)
                    return;

                if (m_currLogSW.BaseStream.Position + StreamWriterBufferSize >= checked((long)this.MaxFileSizeKB * 1024))
                    RotateOrTruncateCurrFile(false);

                // Add a newline before writing a log, so that any abortion/bug
                // in writing the previous line doesn't affect the current line.
                m_currLogSW.WriteLine();

                // TODO-SanKumar-1903: This serialization inside the lock increases
                // wait time of other threads and prolongates the reference of
                // obj. Instead we could serialize into a string outside the lock,
                // which could consume additional throw away memory as well as
                // alloc and dealloc that is worsened by parallelism. Take a call.
                if (PSInstallationInfo.Default.CSMode == CSMode.Rcm)
                {
                    s_mdsRowJsonSer.Serialize(m_currLogSW, obj);
                }
                else if (PSInstallationInfo.Default.CSMode == CSMode.LegacyCS)
                {
                    s_mdsRowJsonSer.Serialize(m_currLogSW, new InMageLogUnit() { Map = obj });
                }

                // NOTE-SanKumar-1902: This "writes" the buffer of StreamWriter
                // to the cache file. File system buffers of the file aren't flushed.
                m_currLogSW.Flush();
            }
        }

        public override void Flush()
        {
            // NO-OP

            // The default value of AutoFlush is true and that would lead to this
            // method getting called after every line of log is traced. Since we
            // have our own flushing (note: not file system buffers but this
            // object maintained buffers), we treat this method as NO-OP.
        }
    }
}
