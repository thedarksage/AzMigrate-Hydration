using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace InMage.APIModel
{

    /*
     * Class: Parameter - The  class object hierarchy representing InMage Scout API Model 
     */
    public class Parameter : XMLSerialize
    {

        /* Property: Name */
        private String name;
        public String Name
        {
            get { return name; }
            set { name = value; }
        }

        /* Property: Value */
        private String value;
        public String Value
        {
            get { return this.value; }
            set { this.value = value; }
        }

        public Parameter() { }

        public Parameter(string name, string value)
        {
            this.name = name;
            this.value = value;
        }

        /* Returns unique key representing this object, i.e. Name */
        public String getKey()
        {
            return Name;
        }

        /* Converts this object to XML String */
        public string ToXML()
        {
            // Replace invalid characters in a Parameter Value with their XML equivalents and then convert to XML.
            String xmlOutput = String.Format("<Parameter Name=\"{0}\" Value=\"{1}\"/>", name, System.Security.SecurityElement.Escape(value));
            return xmlOutput;
        }
    }
}
