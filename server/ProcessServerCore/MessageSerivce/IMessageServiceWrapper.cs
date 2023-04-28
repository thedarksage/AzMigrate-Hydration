using Microsoft.ServiceBus.Messaging;
using System;
using System.Threading;
using System.Threading.Tasks;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.MessageService
{
    public interface IMessageServiceWrapper
    {
        Task PostMessageSync(EventData eventData, CancellationToken cancellationToken);
        Task PostMessageAsync(EventData payload, CancellationToken cancellationToken);
    }
}
