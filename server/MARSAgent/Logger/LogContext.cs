using System;
using System.Runtime.Serialization;
using RcmAgentCommonLib.LoggerInterface;
using RcmContract;
using MarsAgent.Service;

namespace MarsAgent.LoggerInterface
{
    /// <summary>
    /// The context associated with every logging operation.
    /// </summary>
    [DataContract]
    public class LogContext : RcmAgentCommonLogContext
    {
        /// <summary>
        /// Initializes a new instance of the <see cref="LogContext" /> class.
        /// </summary>
        public LogContext()
            : base(
                  ServiceProperties.Instance.ComponentName,
                  ServiceProperties.Instance.ServiceVersion)
        {
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="LogContext" /> class.
        /// </summary>
        /// <param name="applianceId">Appliance Identifier.</param>
        public LogContext(string applianceId)
            : base(
                  ServiceProperties.Instance.ComponentName,
                  ServiceProperties.Instance.ServiceVersion,
                  applianceId)
        {
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="LogContext" /> class.
        /// </summary>
        /// <param name="applianceId">Appliance identifier.</param>
        /// <param name="activityId">Activity Id to initialize with.</param>
        public LogContext(string applianceId, Guid activityId)
            : base(
                  ServiceProperties.Instance.ComponentName,
                  ServiceProperties.Instance.ServiceVersion,
                  applianceId,
                  activityId)
        {
        }

        /// <summary>
        /// Gets or sets machine ID of the agent.
        /// </summary>
        [DataMember]
        public string AgentMachineId { get; set; }

        /// <summary>
        /// Gets or sets Disk ID.
        /// </summary>
        [DataMember]
        public string DiskId { get; set; }

        /// <summary>
        /// Overrides the base ToString for debugging/logging purposes.
        /// </summary>
        /// <returns>The display string.</returns>
        public override string ToString()
        {
            return this.RcmSafeToString();
        }
    }
}