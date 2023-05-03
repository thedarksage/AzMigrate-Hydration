using Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi;
using System;
using System.Threading;
using System.Threading.Tasks;
using static Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils.MiscUtils;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.MessageService
{
    internal class EventHubService : IMessageService
    {
        public IMessageServiceWrapper GetMessageWrapper(EventChannelType eventType)
        {
            try
            {
                // Can throw exception in case the psSettings are not initialised.
                return new EventHubWrapper(eventType);
            }
            catch (NullReferenceException)
            {
                return null;
            }
            
        }

        public IMessageServiceWrapper GetMessageWrapper(EventChannelType eventType, string channelSasUri)
        {
            try
            {
                // Can throw exception in case the uri is null.
                return new EventHubWrapper(eventType, channelSasUri);
            }
            catch (NullReferenceException)
            {
                return null;
            }
        }
    }
}