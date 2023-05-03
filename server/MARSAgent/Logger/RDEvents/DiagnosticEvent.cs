using System;
using System.Diagnostics;
using System.Diagnostics.Tracing;
using System.Threading;
using LoggerInterface;

namespace MarsAgent.LoggerInterface
{
    /// <summary>
    /// Defines a diagnostic event to be logged.
    /// </summary>
    [EventData]
    public class DiagnosticEvent : DiagnosticEventBase
    {
        /// <summary>
        /// The diagnostic event version.
        /// </summary>
        /// <remarks>
        /// The event version must be changed when there is any change in event properties.
        /// </remarks>
        private string eventVersion = "1.0";

        /// <summary>
        /// Initializes a new instance of the <see cref="DiagnosticEvent" /> class.
        /// </summary>
        /// <param name="fetchLogContext">Specifies whether it is okay to fetch log context or
        /// not.</param>
        public DiagnosticEvent(bool fetchLogContext = true)
        {
            if (fetchLogContext)
            {
                LogContext logContext = Logger.Instance.GetLogContext();

                this.PreciseTimeStamp = DateTime.UtcNow;
                this.Pid = Process.GetCurrentProcess().Id;
                this.Tid = Thread.CurrentThread.ManagedThreadId;
                if (logContext != null)
                {
                    this.ActivityId = logContext.ActivityId.ToString();
                    this.ApplianceId = logContext.ApplianceId;
                    this.ApplianceComponentName = logContext.ComponentName;
                    this.ServiceVersion = logContext.ServiceVersion;
                    this.StampName = logContext.RcmStampName;
                    this.AgentMachineId = logContext.AgentMachineId;
                    this.DiskId = logContext.DiskId;
                }
            }
        }

        /// <summary>
        /// Gets the event time.
        /// </summary>
        public DateTime PreciseTimeStamp { get; private set; }

        /// <summary>
        /// Gets the activity ID.
        /// </summary>
        public string ActivityId { get; private set; }

        /// <summary>
        /// Gets the process ID.
        /// </summary>
        public int Pid { get; private set; }

        /// <summary>
        /// Gets the thread ID.
        /// </summary>
        public int Tid { get; private set; }

        /// <summary>
        /// Gets identifier of the appliance.
        /// </summary>
        public string ApplianceId { get; private set; }

        /// <summary>
        /// Gets the component name.
        /// </summary>
        public string ApplianceComponentName { get; private set; }

        /// <summary>
        /// Gets the version information of the agent component.
        /// </summary>
        public string ServiceVersion { get; private set; }

        /// <summary>
        /// Gets machine ID of the source agent.
        /// </summary>
        public string AgentMachineId { get; private set; }

        /// <summary>
        /// Gets the disk ID.
        /// </summary>
        public string DiskId { get; private set; }

        /// <summary>
        /// To be overridden by child classes if they are using EventSource for tracing.
        /// </summary>
        /// <param name="eventSource">The event source.</param>
        /// <param name="options">The event source options.</param>
        public override void Write(EventSource eventSource, EventSourceOptions options)
        {
            eventSource.Write(this.GetType().Name, options, this);
        }

        /// <summary>
        /// Method to retrieve the event version.
        /// </summary>
        /// <returns>The event version.</returns>
        public string GetEventVersion()
        {
            return this.eventVersion;
        }
    }
}