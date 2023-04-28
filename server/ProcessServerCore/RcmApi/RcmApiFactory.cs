using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Config;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.RcmApi
{
    public static class RcmApiFactory
    {
        public static IProcessServerRcmApiStubs GetProcessServerRcmApiStubs(PSConfiguration psConfig)
        {
            // TODO-SanKumar-1904: Check for required params instead of failing
            // with null here; instead of inside impl constructor

            return new ProcessServerRcmApiStubsImpl(
                rcmUri: psConfig.RcmUri,
                aadDetails: psConfig.RcmAzureADSpn,
                proxy: psConfig.Proxy,
                machineId: psConfig.HostId,
                disableRcmSslCertCheck: psConfig.DisableRcmSslCertCheck);
        }
    }
}
