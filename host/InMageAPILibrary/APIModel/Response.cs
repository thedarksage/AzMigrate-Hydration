using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace InMage.APIModel
{

    /*
     * Class:Response - Root class of Response Object hierarchy
     */
    public class Response:XMLSerialize
    {

        /* Property: Id */
        private String id;
        public String Id
        {
            get { return id; }
            set { id = value; }
        }

        /* Property: Version */
        private String version;
        public String Version
        {
            get { return version; }
            set { version = value; }
        }

        /* Property: returncode */
        private String returncode;
        public String Returncode
        {
            get { return returncode; }
            set { returncode = value; }
        }


        /* Property: Message */
        private String message;
        public String Message
        {
            get { return message; }
            set { message = value; }
        }

        /* Property: Body */
        private Body body;
        public Body Body
        {
            get { return body; }
            set { body = value; }
        }


        /* Converts this object to XML */
        public string ToXML()
        {
            String xmlOutput = String.Format("<Response ID=\"{0}\" Version=\"{1}\" Returncode=\"{2}\" Message=\"{3}\">", id,version,returncode,message);
            if (body != null)
            {
                xmlOutput += body.ToXML();
            }
            xmlOutput += "</Response>";
            return xmlOutput;
        }
    }
}
