using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.MessageService
{
    public static class MessageServiceFactory
    {
        public static IMessageService GetMessageService()
        {
            return new EventHubService();
        }
    }
}
