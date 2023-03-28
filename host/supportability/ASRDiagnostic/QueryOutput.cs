using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Internal.SiteRecovery.ASRDiagnostic
{
    public class QueryOutput
    {
        public string Provider,ObjectName, LastClientRequestId, ScenarioName, State, OSName, OSType, OSVersion, ErrorCodeName,Logs,ClusterName;
        public double AgentVersion;

        public QueryOutput(string Provider, string ObjectName, string LastClientRequestId, string ScenarioName, string State, double AgentVersion, string OSName, string OSType, string OSVersion, string ErrorCodeName)
        {
            this.Provider = Provider;
            this.ObjectName = ObjectName;
            this.LastClientRequestId = LastClientRequestId;
            this.ScenarioName = ScenarioName;
            this.State = State;
            this.OSName = OSName;
            this.OSType = OSType;
            this.OSVersion = OSVersion;
            this.ErrorCodeName = ErrorCodeName;
            this.AgentVersion = AgentVersion;
            this.Logs = "";
            this.ClusterName = "";
        }
    }
}
