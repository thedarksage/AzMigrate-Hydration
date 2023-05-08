using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using InMage.APIModel;

namespace InMage.APIHelpers
{
    public class Event
    {
        public string EventCode { get; private set; }
        public string EventType { get; private set; }
        public string Category { get; private set; }
        public string Component { get; private set; }
        public string HostId { get; private set; }
        public string HostName { get; private set; }
        public EventSeverity Severity { get; private set; }
        public DateTime? EventTime { get; private set; }
        public string Summary { get; private set; }
        public string Details { get; private set; }
        public string CorrectiveAction { get; private set; }
        public Dictionary<string, string> PlaceHolders { get; private set; }

        public Event(ParameterGroup eventDetails)
        {
            EventCode = eventDetails.GetParameterValue("EventCode");
            EventType = eventDetails.GetParameterValue("EventType");
            Category = eventDetails.GetParameterValue("Category");
            Component = eventDetails.GetParameterValue("Component");
            HostId = eventDetails.GetParameterValue("HostId");
            HostName = eventDetails.GetParameterValue("HostName");
            Severity = Utils.ParseEnum<EventSeverity>(eventDetails.GetParameterValue("Severity"));
            EventTime = Utils.UnixTimeStampToDateTime(eventDetails.GetParameterValue("EventTime"));
            Summary = eventDetails.GetParameterValue("Summary");
            Details = eventDetails.GetParameterValue("Details");
            CorrectiveAction = eventDetails.GetParameterValue("CorrectiveAction");
            PlaceHolders = new Dictionary<string, string>();
            ParameterGroup placeHolders = eventDetails.GetParameterGroup("PlaceHolders");
            if (placeHolders != null)
            {
                foreach (var item in placeHolders.ChildList)
                {
                    if (item is Parameter)
                    {
                        Parameter placeHolder = item as Parameter;
                        PlaceHolders.Add(placeHolder.Name, placeHolder.Value);
                    }
                }
            }
        }
    }
}
