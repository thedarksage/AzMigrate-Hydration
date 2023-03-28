using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace InMage.Test.API
{
    public delegate JobStatus FunctionHandler(Section section);
    //public delegate JobStatus AsyncFunctionHandler(Section section);

    public abstract class BaseAPITest
    {        
        protected Dictionary<string, string> placeholders;

        public int AsyncFunctionTimeOut { get; set; }    

        public abstract void SetConfiguration(string ipaddress, int port, string protocol);        

        public void SetPlaceholders(Section placeholdersSection)
        {
            if (placeholdersSection != null && placeholdersSection.Keys.Count > 0)
            {
                this.placeholders = new Dictionary<string, string>();
                foreach (Key key in placeholdersSection.Keys)
                {
                    this.placeholders[key.Name] = key.Value;
                }
            }
        }

        public void ReplacePlaceholders(Section section)
        {
            section.ReplacePlaceholders(this.placeholders);
        }

        public abstract FunctionHandler GetFunctionHandler(Section section);       
    }
}
