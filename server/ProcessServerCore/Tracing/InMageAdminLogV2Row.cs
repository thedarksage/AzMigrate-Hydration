using Microsoft.Azure.SiteRecovery.ProcessServer.Core.RcmApi;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils;
using Newtonsoft.Json;
using Newtonsoft.Json.Converters;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.Threading;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.Tracing
{
    public class SystemProperties
    {
        public string ComponentId;
        public long SequenceNumber;
        public string MachineId;
        public string BiosId;
        public string FabType;
    }

    /// <summary>
    /// Represents a row in the InMageAdminLogV2 MDS table. When serialized in
    /// JSON, it will be consumed by the parser in the MARS for upload.
    /// </summary>
    internal class InMageAdminLogV2Row
    {
        private static readonly JsonSerializerSettings s_jsonSerSettings =
            JsonUtils.GetStandardSerializerSettings(
                indent: false,
                converters: JsonUtils.GetAllKnownConverters());

        /// <summary>Mandatory. The value must be passed to Telemetry Service.</summary>
        [JsonConverter(typeof(StringEnumConverter))]
        public TraceEventType AgentLogLevel;
        public long AgentPid;
        public string AgentTid;
        /// <summary>Mandatory. The value must be passed to Telemetry Service.</summary>
        public DateTime AgentTimeStamp;
        /// <summary>Mandatory. The value must be passed to Telemetry Service.</summary>
        public string BiosId;
        /// <summary>Mandatory. The value must be passed to Telemetry Service.</summary>
        public string ComponentId;
        public string DiskId;
        public string DiskNumber;
        public string ErrorCode;

        [JsonIgnore]
        public Dictionary<string, string> ExtendedPropertiesDict;
        public string ExtendedProperties;

        /// <summary>Mandatory. The value must be passed to Telemetry Service.</summary>
        public string FabType;
        /// <summary>Mandatory. The value must be passed to Telemetry Service.</summary>
        public string MachineId;
        public string MemberName;
        /// <summary>Mandatory. The value must be passed to Telemetry Service.</summary>
        public string Message;
        public long SequenceNumber;
        public string SourceEventId;
        public string SourceFilePath;
        public string SourceLineNumber;
        public string SubComponent;

        public string SerializeExtendedProperties()
        {
            // Extended properties are stored in a dictionary and 
            // serialized into a JSON on demand.

            if ((this.ExtendedPropertiesDict?.Count).GetValueOrDefault() > 0)
                ExtendedProperties = JsonConvert.SerializeObject(ExtendedPropertiesDict, s_jsonSerSettings);

            return ExtendedProperties;
        }

        public static InMageAdminLogV2Row Build(
            TraceEventCache eventCache, SystemProperties sysProps, string source,
            TraceEventType eventType, RcmOperationContext opContext,
            int id, string format, object[] args)
        {
            if (sysProps == null)
                throw new ArgumentNullException("sysProps");

            var biosId = sysProps.BiosId;
            var componentId = sysProps.ComponentId;
            var fabType = sysProps.FabType;
            var machineId = sysProps.MachineId;

            Dictionary<string, string> extProps = null;
            if (opContext?.ClientRequestId != null || opContext?.ActivityId != null)
            {
                extProps = new Dictionary<string, string>();

                if (opContext?.ClientRequestId != null)
                    extProps.Add(nameof(opContext.ClientRequestId), opContext.ClientRequestId);

                if (opContext?.ActivityId != null)
                    extProps.Add(nameof(opContext.ActivityId), opContext.ActivityId);
            }

            var toRet = new InMageAdminLogV2Row()
            {
                AgentLogLevel = eventType, // TelemetrySvc will fail writing this log, without this value
                //AgentPid, // Filled below
                //AgentTid, // Filled below
                //AgentTimestamp, // TelemetrySvc will fail writing this log, without this value
                BiosId = biosId ?? "<empty>", // TelemetrySvc will fail writing this log, without this value
                ComponentId = componentId ?? "<empty>", // TelemetrySvc will fail writing this log, without this value
                DiskId = null,
                DiskNumber = null,
                ErrorCode = null,
                ExtendedPropertiesDict = extProps,
                FabType = fabType ?? "<empty>", // TelemetrySvc will fail writing this log, without this value
                MachineId = machineId ?? "<empty>", // TelemetrySvc will fail writing this log, without this value
                MemberName = null, // TODO-SanKumar-1906: Could be infered from the eventCache.CallStack
                //Message, // Filled below (TelemetrySvc will fail writing this log, without this value)
                SequenceNumber = Interlocked.Increment(ref sysProps.SequenceNumber),
                SourceEventId = id.ToString(CultureInfo.InvariantCulture),
                SourceFilePath = null, // TODO-SanKumar-1906: Could be infered from the eventCache.CallStack
                SourceLineNumber = null, // TODO-SanKumar-1906: Could be infered from the eventCache.CallStack
                SubComponent = source
            };

            if (format != null && args != null && args.Length > 0)
                toRet.Message = string.Format(CultureInfo.InvariantCulture, format, args);
            else
                toRet.Message = format ?? string.Empty;

            if (eventCache != null)
            {
                toRet.AgentPid = eventCache.ProcessId;
                toRet.AgentTid = eventCache.ThreadId;
                toRet.AgentTimeStamp = eventCache.DateTime.ToUniversalTime();
            }
            else
            {
                // TODO-SanKumar-1903: We could fill in the current process and current thread id.
                //currLog.AgentPid
                //currLog.AgentTid
                toRet.AgentTimeStamp = DateTime.UtcNow;
            }

            return toRet;
        }
    }

    /// <summary>
    /// Wrapper class around the MDS rows to simulate the JSON objects from source
    /// </summary>
    internal class InMageLogUnit
    {
        public object Map;
    }
}
