using System;
using System.ComponentModel;
using System.Linq;
using System.Text;

namespace MarsAgent.CBEngine.Model
{
    /// <summary>
    /// Define log entity for the revised cbening logs
    /// </summary>
    public class LogEntity
    {
        public LogEntity() { }

        [Order(0)]
        public string PreciseTimeStamp { get; set; }

        [Order(1)]
        public string ActivityId { get; set; }

        [Order(2)]
        public string Pid { get; set; }

        [Order(3)]
        public string Tid { get; set; }

        [Order(4)]
        public string ApplianceId { get; set; }

        [Order(5)]
        public string ApplianceComponentName { get; set; }

        [Order(6)]
        public string ServiceVersion { get; set; }

        [Order(7)]
        public string AgentMachineId { get; set; }

        [Order(8)]
        public string DiskId { get; set; }

        [Order(9)]
        public string ServiceEventId { get; set; }

        [Order(10)]
        public string ServiceEventName { get; set; }

        [Order(11)]
        public string ServiceActivityId { get; set; }

        [Order(12)]
        public string SRSActivityId { get; set; }

        [Order(13)]
        public string ClientRequestId { get; set; }

        [Order(14)]
        public string Message { get; set; }

        [Order(15)]
        public string ComponentName { get; set; }

        [Order(16)]
        public string CallerInfo { get; set; }

        [Order(17)]
        public string SubscriptionId { get; set; }

        [Order(18)]
        public string ResourceId { get; set; }

        [Order(19)]
        public string ContainerId { get; set; }

        [Order(20)]
        public string ResourceLocation { get; set; }

        [Order(21)]
        public string ServiceName { get; set; }

        [Order(22)]
        public string DeploymentName { get; set; }

        [Order(23)]
        public string StampName { get; set; }

        [Order(24)]
        public string Region { get; set; }

        [Order(25)]
        public string LogLevel { get; set; }

        [Order(26)]
        public string DeploymentMode { get; set; }

        [Order(27)]
        public string ProcessId { get; set; }

        [Order(28)]
        public string ThreadId { get; set; }

        [Order(29)]
        public string LogTime { get; set; }

        [Order(30)]
        public string ComponentCode { get; set; }

        [Order(31)]
        public string FileNameLineNumber { get; set; }

        [Order(32)]
        public string This { get; set; }

        [Order(33)]
        public string TaskId { get; set; }

        [Order(34)]
        public string VmId { get; set; }

        [Order(35)]
        public string MarsVersion { get; set; }

        [Order(36)]
        public string BiosId { get; set; }

        /// <summary>
        /// Convert the object to string 
        /// </summary>
        /// <param name="separator"> seperator to convert log properties in string </param>
        public string ToCsv(string separator)
        {
            StringBuilder csvdata = new StringBuilder();

            Type t = typeof(LogEntity);
            var props = t
                .GetProperties()
                .OrderBy(p => ((Order)p.GetCustomAttributes(typeof(Order), false)[0]).order);

            foreach (var f in props)
            {
                if (csvdata.Length > 0)
                    csvdata.Append(separator);

                var x = f.GetValue(this);

                if (x != null)
                    csvdata.Append(x);
                else
                    csvdata.Append(string.Empty);
            }

            return csvdata.ToString();
        }
    }

    /// <summary>
    /// Attribute to order the position the property based on kusto table structure 
    /// </summary>
    [AttributeUsage(AttributeTargets.Property, Inherited = false, AllowMultiple = false)]
    public sealed class Order : Attribute
    {
        public int order { get; }
        public Order(int _order = 0)
        {
            order = _order;
        }
    }
}