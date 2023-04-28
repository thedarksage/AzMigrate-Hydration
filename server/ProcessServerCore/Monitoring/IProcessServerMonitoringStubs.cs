using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.Monitoring
{
    public interface IProcessServerMonitoringStubs
    {
        Tuple<long, double> GetUploadedData();
    }
}
