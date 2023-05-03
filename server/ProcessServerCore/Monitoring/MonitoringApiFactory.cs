using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Config;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.Monitoring
{
    public static class MonitoringApiFactory
    {
        public static IProcessServerCSApiStubs GetPSMonitoringApiStubs()
        {
            if (PSInstallationInfo.Default.CSMode == CSMode.LegacyCS)
            {
                return new ProcessServerCSApiStubsImpl();
            }
            else if (PSInstallationInfo.Default.CSMode == CSMode.Rcm)
            {
                return new ProcessServerRCMMonitorStubsImpl();
            }
            else
            {
                throw new NotSupportedException(nameof(PSInstallationInfo.Default.CSMode));
            }
        }

    }
}
