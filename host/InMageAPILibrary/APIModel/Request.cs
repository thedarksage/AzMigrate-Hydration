using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace InMage.APIModel
{
    /*
     * Class: Request - The root class object hierarchy representing InMage Scout API Model 
     */
    
    public class Request: XMLSerialize
    {
        private String id;
        private String version;
        private Header header;
        
        /* Header Property */
        public Header Header
        {
            get
            {
                return header;
            }
            set
            {
                header = value;
            }
        }

        /* Id Property */
        public String Id
        {
            get
            {
                return id;
            }
            set
            {
                id = value;
            }
        }

        /* Version Property */
        public String Version
        {
            get
            {
                return version;
            }
            set
            {
                version = value;
            }
        }
        /* Body Property */
        private Body body;
        public Body Body
        {
            get { return body; }
            set { body = value; }
        }

        /* Serializes the current object to equivalent XML representation */
        public string ToXML()
        {
            String xmlOutput = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Request Id=\"{0}\" Version=\"{1}\">";
            xmlOutput =  String.Format(xmlOutput, id, version);
            xmlOutput += header.ToXML();
            xmlOutput += body.ToXML();
            xmlOutput += "</Request>";
            return xmlOutput;
        }
    }
}
