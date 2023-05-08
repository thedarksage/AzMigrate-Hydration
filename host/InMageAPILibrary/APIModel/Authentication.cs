using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Text;


namespace InMage.APIModel
{
    public enum AuthMethod
    {
        MessageAuth,
        RXAuth,
        CXAuth,
        ComponentAuth
    }

    /*
     * Class: Authentication - The  class object hierarchy representing InMage Scout API Model 
     */
    public class Authentication : XMLSerialize
    {

        /* AccessKeyID property */
        private String accessKeyID;
        public String AccessKeyID
        {
            get { return accessKeyID; }
            set { accessKeyID = value; }
        }

        /* AccessSignature property */
        private String accessSignature;
        public String AccessSignature
        {
            get { return accessSignature; }
            set { accessSignature = value; }
        }

        /* AuthMethod property */
        private AuthMethod authMethod;
        public AuthMethod AuthMethod
        {
            get { return authMethod; }
            set { authMethod = value; }
        }

        /* UserName property */
        private String userName;
        public String UserName
        {
            get { return userName; }
            set { userName = value; }
        }

        /* Secure Password property */
        private String password;
        public String Password
        {
            get { return password; }
            set { password = value; }
        }

        /* AuthMethodVersion property */
        private String authMethodVersion;
        public String AuthMethodVersion
        {
            get { return authMethodVersion; }
            set { authMethodVersion = value; }
        }

        /* ChildList property - this property contains all the children belonging to this object */
        private ArrayList childList = new ArrayList();
        public ArrayList ChildList
        {
            get { return childList; }
            set { childList = value; }
        }

        /* Adds a child to this object */
        public void addChildObj(Object obj)
        {

            if (obj is ParameterGroup || obj is Parameter)
            {
                this.childList.Add(obj);
            }
            else
            {
                throw new APIModelException("Illegal Object passed as child");
            }
        }


        /* Converts this object to XML String */
        public string ToXML()
        {
            String xmlOutput = "<Authentication>";
            xmlOutput += "<AccessKeyID>";
            xmlOutput += accessKeyID;
            xmlOutput += "</AccessKeyID>";
            xmlOutput += "<AuthMethod>";
            xmlOutput += authMethod;
            if (authMethod == AuthMethod.ComponentAuth)
            {
                xmlOutput += String.Format("_{0}", authMethodVersion);
            }
            xmlOutput += "</AuthMethod>";
            if (authMethod == APIModel.AuthMethod.CXAuth)
            {
                xmlOutput += "<CXUserName>";
                xmlOutput += userName;
                xmlOutput += "</CXUserName>";
                xmlOutput += "<CXPassword>";
                xmlOutput += password;
                xmlOutput += "</CXPassword>";
            }
            else if (authMethod == APIModel.AuthMethod.RXAuth)
            {
                xmlOutput += "<RXUserName>";
                xmlOutput += userName;
                xmlOutput += "</RXUserName>";
                xmlOutput += "<RXPassword>";
                xmlOutput += password;
                xmlOutput += "</RXPassword>";
            }
            else
            {
                xmlOutput += "<AccessSignature>";
                xmlOutput += accessSignature;               
                xmlOutput += "</AccessSignature>";
            }            
            foreach (XMLSerialize obj in childList)
            {
                xmlOutput += obj.ToXML();
            }
            xmlOutput += "</Authentication>";
            return xmlOutput;
        }
    }
}
