using Microsoft.ServiceBus.Messaging;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.MessageService
{
    class EventHubClientFactory
    {   
       
    /// <summary>
    ///  Returns new event hub client object every time it is called.
    /// </summary>
    /// <param name="EventHubConnectionString">SAS uri for event hub connection.</param>
    /// <returns>New EventHubClient object from current PS Settings.</returns>
    public static EventHubSender GetEventHubClient(string EventHubConnectionString)
        {
            return EventHubSender.CreateFromConnectionString(EventHubConnectionString);
        }
    }
}
