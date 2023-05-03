using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace InMage.APIModel
{

    /*
     * Class: Header - The  class object hierarchy representing InMage Scout API Model 
     */
    public class Header:XMLSerialize
    {
        
        /* Property: Authentication */
        private Authentication authentication;
        public Authentication Authentication
        {
            get
            {
                return authentication;
            }
            set
            {
                authentication = value;
            }
        }


        /* Converts this object to XML String */
        public string ToXML()
        {
            String xmlOutput = "<Header>";
            xmlOutput += authentication.ToXML();
            xmlOutput += "</Header>";

            return xmlOutput;
        }
    }
}
