using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Config;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils;
using Newtonsoft.Json;
using System;
using System.Diagnostics;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.Tracing
{
    /// <summary>
    /// Final table in Kusto
    /// </summary>
    public enum MdsTable
    {
        Unknown,
        InMageAdminLogV2,
        InMageTelemetryPSV2
    }

    /// <summary>
    /// Abstract, base class for all the TraceListeners in the PS
    /// </summary>
    public abstract class PSTraceListenerBase : TraceListener
    {
        /// <summary>
        /// Kusto table destination of this object
        /// </summary>
        public MdsTable MdsTable { get; private set; }

        /// <summary>
        /// System properties to be logged in InMageAdminLogV2
        /// </summary>
        protected SystemProperties SystemProperties { get; private set; }

        private static readonly JsonSerializerSettings s_jsonSerSettings
            = JsonUtils.GetStandardSerializerSettings(
                indent: false,
                converters: JsonUtils.GetAllKnownConverters());

        private readonly CancellationTokenSource m_initSysPropsCts = new CancellationTokenSource();
        private Task m_initSysPropsTask;

        /// <summary>
        /// Must be called from the constructor of the derived class
        /// </summary>
        /// <param name="mdsTable">Kusto table destination of this object</param>
        /// <param name="componentId">Component Id (ProcessServer)</param>
        protected void Initialize(MdsTable mdsTable, string componentId)
        {
            this.MdsTable = mdsTable;

            this.SystemProperties = new SystemProperties()
            {
                ComponentId = componentId
            };

            if (!InitializeSystemProperties())
            {
                m_initSysPropsTask = Task.Run(async () =>
                {
                    do
                    {
                        // TODO-SanKumar-2004: Delay time in Settings
                        await
                            Task.Delay(TimeSpan.FromMinutes(5), m_initSysPropsCts.Token)
                            .ConfigureAwait(false);

                    } while (!InitializeSystemProperties());
                },
                m_initSysPropsCts.Token);
            }
        }

        /// <summary>
        /// Try to initialize the <see cref="SystemProperties"/> member
        /// </summary>
        /// <returns>True, if fully initialized</returns>
        private bool InitializeSystemProperties()
        {
            bool allSuccessful = true;

            if (this.SystemProperties.MachineId == null)
            {
                allSuccessful &= TaskUtils.RunAndIgnoreException(() =>
                {
                    switch (PSInstallationInfo.Default.CSMode)
                    {
                        case CSMode.Rcm:
                        case CSMode.LegacyCS:
                            this.SystemProperties.MachineId = PSInstallationInfo.Default.GetPSHostId();
                            break;

                        case CSMode.Unknown:
                            throw new Exception("Invalid CS mode");

                        default:
                            throw new NotImplementedException(
                                $"CSMode {PSInstallationInfo.Default.CSMode} is not implemented");
                    }
                }, Tracers.Misc);
            }

            if (this.SystemProperties.BiosId == null)
            {
                allSuccessful &= TaskUtils.RunAndIgnoreException(
                    () => this.SystemProperties.BiosId = SystemUtils.GetBiosId(),
                    Tracers.Misc);
            }

            // TODO-SanKumar-2004: Initialize sysProps.FabType

            // ComponentId - Filled once at the Initialize() method
            // SequenceNumber - Auto-initialized to 0

            return allSuccessful;
        }

        // TODO-SanKumar-1903: This call/Dispose doesn't come automatically, when
        // the process is stopping. It will only come when explicitly Close() is
        // called on the TraceSource using it. Currently, this is not an issue,
        // since the data is written to the file immediately during every log.
        // But when the buffering of logs is added to improve performance, we
        // must ensure to call close in both OnStop() of services as well as
        // UnhandledExceptionHandlers to flush out the lines of log within the
        // in-memory buffer.
        // Similarly, the taks runs in the default thread pool, which consists
        // of background threads, so they simply get aborted when the process
        // quits without blocking the service stop.

        /// <inheritdoc/>
        public sealed override void Close()
        {
            Dispose();
        }

        #region Dispose Pattern

        private int m_isDisposed = 0;

        //private void ThrowIfDisposed()
        //{
        //    if (m_isDisposed != 0)
        //        throw new ObjectDisposedException(this.ToString());
        //}

        /// <inheritdoc/>
        protected override void Dispose(bool disposing)
        {
            // Do nothing and return, if already dispose started / completed.
            if (0 != Interlocked.CompareExchange(ref m_isDisposed, 1, 0))
                return;

            if (disposing)
            {
                // Not expected to throw, since no callback is registered.
                m_initSysPropsCts.Cancel();

                TaskUtils.RunAndIgnoreException(() =>
                {
                    try
                    {
                        m_initSysPropsTask?.Wait();
                    }
                    catch (AggregateException ae)
                    {
                        if (!m_initSysPropsTask.IsCanceled ||
                            ae.Flatten().InnerExceptions.Any(ex => !(ex is TaskCanceledException) || (ex as TaskCanceledException).Task != m_initSysPropsTask))
                        {
                            // Rethrowing, so that debugger is broken by ignore exception wrapper
                            throw;
                        }
                    }
                }, Tracers.Misc);

                m_initSysPropsCts.Dispose();
            }

            base.Dispose(disposing);
        }

        #endregion Dispose Pattern

        #region TraceEvent

        /// <inheritdoc/>
        public sealed override void TraceEvent(TraceEventCache eventCache, string source, TraceEventType eventType, int id)
        {
            this.TraceEvent(eventCache, source, eventType, id, null, null);
        }

        /// <inheritdoc/>
        public sealed override void TraceEvent(TraceEventCache eventCache, string source, TraceEventType eventType, int id, string message)
        {
            this.TraceEvent(eventCache, source, eventType, id, message, null);
        }

        /// <summary>
        /// Writes trace information, a formatted array of objects and event 
        /// information to the listener specific output.
        /// </summary>
        /// <param name="eventCache">A System.Diagnostics.TraceEventCache object 
        /// that contains the current process ID, thread ID, and stack trace information.</param>
        /// <param name="source">A name used to identify the output, typically the name of the 
        /// application that generated the trace event.</param>
        /// <param name="eventType">One of the System.Diagnostics.TraceEventType values 
        /// specifying the type of event that has caused the trace.</param>
        /// <param name="id">A numeric identifier for the event.</param>
        /// <param name="format">A format string that contains zero or more format items, 
        /// which correspond to objects in the args array.</param>
        /// <param name="args">An object array containing zero or more objects to format.</param>
        protected abstract void OnTraceEvent(TraceEventCache eventCache, string source, TraceEventType eventType, int id, string format, params object[] args);

        /// <inheritdoc/>
        public sealed override void TraceEvent(TraceEventCache eventCache, string source, TraceEventType eventType, int id, string format, params object[] args)
        {
            if (m_isDisposed != 0)
                return;

            TaskUtils.RunAndIgnoreException(() =>
            {
                if (this.Filter != null && !this.Filter.ShouldTrace(eventCache, source, eventType, id, format, args, data1: null, data: null))
                    return;

                OnTraceEvent(eventCache, source, eventType, id, format, args);
            });
        }

        #endregion TraceEvent

        #region TraceData

        /// <inheritdoc/>
        public sealed override void TraceData(TraceEventCache eventCache, string source, TraceEventType eventType, int id, params object[] data)
        {
            this.TraceData(eventCache, source, eventType, id, new InMageAdminLogV2TracePacket() { ExtendedProperties = data });
        }

        /// <summary>
        /// Writes trace information, a data object and event information to the 
        /// listener specific output.
        /// </summary>
        /// <param name="eventCache">A System.Diagnostics.TraceEventCache object that 
        /// contains the current process ID, thread ID, and stack trace information.</param>
        /// <param name="source">A name used to identify the output, typically 
        /// the name of the application that generated the trace event.</param>
        /// <param name="eventType">One of the System.Diagnostics.TraceEventType 
        /// values specifying the type of event that has caused the trace.</param>
        /// <param name="id">A numeric identifier for the event.</param>
        /// <param name="data1">The trace data to emit.</param>
        protected abstract void OnTraceData(TraceEventCache eventCache, string source, TraceEventType eventType, int id, object data1);

        /// <inheritdoc/>
        public sealed override void TraceData(TraceEventCache eventCache, string source, TraceEventType eventType, int id, object data1)
        {
            if (m_isDisposed != 0)
                return;

            TaskUtils.RunAndIgnoreException(() =>
            {
                object tracePkt;
                string format = null;
                object[] args = null;

                switch (this.MdsTable)
                {
                    case MdsTable.InMageAdminLogV2:
                        tracePkt = data1 as InMageAdminLogV2TracePacket;
                        var adminLogV2TracePkt = (InMageAdminLogV2TracePacket)tracePkt;

                        format = adminLogV2TracePkt?.FormatString;
                        args = adminLogV2TracePkt?.FormatArgs;

                        break;

                    case MdsTable.InMageTelemetryPSV2:
                        tracePkt = data1 as InMageTelemetryPSV2Row;
                        break;

                    default:
                        throw new NotImplementedException(this.MdsTable.ToString());
                }

                if (this.Filter != null && !this.Filter.ShouldTrace(eventCache, source, eventType, id, format, args, data1, data: null))
                    return;

                if (data1 != null && tracePkt == null)
                {
                    // If the passed data is not expected trace packet
                    this.TraceEvent(eventCache, source, eventType, id,
                        message: JsonConvert.SerializeObject(data1, s_jsonSerSettings));

                    return;
                }

                OnTraceData(eventCache, source, eventType, id, data1);
            });
        }

        #endregion TraceData

        #region Write

        /// <inheritdoc/>
        protected sealed override void WriteIndent()
        {
            // NO-OP
        }

        /// <inheritdoc/>
        public sealed override void Write(string message)
        {
            WriteLine(message);
        }

        /// <inheritdoc/>
        public sealed override void WriteLine(string message)
        {
            this.TraceEvent(
                new TraceEventCache(), "Unknown", TraceEventType.Information, 0, message);
        }

        #endregion Write

        #region Force sealed base class implementations

        // Usually the inheritance will automatically do this but explicitly
        // sealing these methods, so that no derived class implements these methods.

        /// <inheritdoc/>
        public sealed override void Fail(string message)
        {
            base.Fail(message);
        }

        /// <inheritdoc/>
        public sealed override void Fail(string message, string detailMessage)
        {
            base.Fail(message, detailMessage);
        }

        /// <inheritdoc/>
        public sealed override string Name { get => base.Name; set => base.Name = value; }

        /// <inheritdoc/>
        public sealed override void TraceTransfer(TraceEventCache eventCache, string source, int id, string message, Guid relatedActivityId)
        {
            base.TraceTransfer(eventCache, source, id, message, relatedActivityId);
        }

        /// <inheritdoc/>
        public sealed override void Write(object o)
        {
            base.Write(o);
        }

        /// <inheritdoc/>
        public sealed override void Write(object o, string category)
        {
            base.Write(o, category);
        }

        /// <inheritdoc/>
        public sealed override void Write(string message, string category)
        {
            base.Write(message, category);
        }

        /// <inheritdoc/>
        public sealed override void WriteLine(object o)
        {
            base.WriteLine(o);
        }

        /// <inheritdoc/>
        public sealed override void WriteLine(object o, string category)
        {
            base.WriteLine(o, category);
        }

        /// <inheritdoc/>
        public sealed override void WriteLine(string message, string category)
        {
            base.WriteLine(message, category);
        }

        #endregion Force sealed base class implementations
    }
}
