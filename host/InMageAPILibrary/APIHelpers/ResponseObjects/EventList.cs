using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using InMage.APIModel;

namespace InMage.APIHelpers
{
    public class EventList
    {
        public string Token { get; private set; }
        public List<Event> Events { get; private set; }

        public EventList(FunctionResponse listEventsFucntionResponse)
        {
            Token = listEventsFucntionResponse.GetParameterValue("Token");
            Events = new List<Event>();
            foreach (var item in listEventsFucntionResponse.ChildList)
            { 
                if(item is ParameterGroup)
                {
                    Events.Add(new Event(item as ParameterGroup));
                }
            }
        }
    }
}
