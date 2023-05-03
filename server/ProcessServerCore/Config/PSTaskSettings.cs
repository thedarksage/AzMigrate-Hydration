using Newtonsoft.Json;
using Newtonsoft.Json.Converters;
using System;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.Config
{
    [JsonConverter(typeof(StringEnumConverter))]
    public enum TaskExecutionState
    {
        ON,
        OFF
    }

    [JsonConverter(typeof(StringEnumConverter))]
    public enum TaskName
    {
        AutoResyncOnResetSet,

        DeleteReplication,

        RegisterPS,

        MonitorLogs,

        RenewCertificates,

        MonitorTelemetry,

        AccumulatePerfData,

        UploadPerfData
    }

    public class TaskAttributes
    {
        [JsonProperty("NameSpaceName")]
        public string NameSpaceName { get; internal set; }

        [JsonProperty("AssemblyName")]
        public string AssemblyName { get; internal set; }

        [JsonProperty("ClassName")]
        public string ClassName { get; internal set; }

        [JsonProperty("MethodName")]
        public string MethodName { get; internal set; }

        public bool IsValidInfo()
        {
            return
                !string.IsNullOrWhiteSpace(AssemblyName) &&
                !string.IsNullOrWhiteSpace(ClassName) &&
                !string.IsNullOrWhiteSpace(MethodName);
        }
    }

    public class PSTask
    {
        [JsonProperty("TaskExecutionState")]
        [JsonConverter(typeof(StringEnumConverter))]
        public TaskExecutionState TaskExecutionState { get; private set; }

        [JsonProperty("TaskInterval")]
        public string TaskInterval { get; private set; }

        [JsonProperty("TaskName")]
        [JsonConverter(typeof(StringEnumConverter))]
        public TaskName TaskName { get; private set; }

        [JsonProperty("TaskAttributes")]
        public TaskAttributes TaskAttributes { get; private set; }

        public bool IsValidInfo()
        {
            return
                Enum.IsDefined(typeof(TaskName), TaskName) &&
                Enum.IsDefined(typeof(TaskExecutionState), TaskExecutionState) &&
                TimeSpan.TryParse(TaskInterval, out TimeSpan interval);
        }
    }
}
