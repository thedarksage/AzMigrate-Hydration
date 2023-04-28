using static Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils.MiscUtils;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.MessageService
{
    public interface IMessageService
    {
        IMessageServiceWrapper GetMessageWrapper(EventChannelType eventType);
        IMessageServiceWrapper GetMessageWrapper(EventChannelType eventType, string channelSasUri);
    }
}