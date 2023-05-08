using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace InMage.APIModel
{

    /*
     * Class: Function - Object in Response Hierarchy
     */
    public class Function:XMLSerialize
    {

        /* Property: Name */
        private String name;
        public String Name
        {
            get { return name; }
            set { name = value; }
        }

        /* Property: Id */
        private String id;
        public String Id
        {
            get { return id; }
            set { id = value; }
        }

        /* Property: ReturnCode */
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

        
        /* Property: FunctionRequest Object */
        private FunctionRequest functionRequest;
        public FunctionRequest FunctionRequest
        {
            get { return functionRequest; }
            set { functionRequest = value; }
        }

        
        /* Property: FunctionResponse Object */
        private FunctionResponse functionResponse;
        public FunctionResponse FunctionResponse
        {
            get { return functionResponse; }
            set { functionResponse = value; }
        }

        /* Property: ErrorDetails ParameterGroup */
        private ParameterGroup errorDetails;
        public ParameterGroup ErrorDetails
        {
            get { return errorDetails; }
            set { errorDetails = value; }
        }

        /* Converts this object XML String */
        public string ToXML()
        {
            String xmlOutput = String.Format("<Function Name=\"{0}\" Id=\"{1}\" Returncode=\"{2}\" Message=\"{3}\">", name,id,returncode,message);
            if (functionRequest != null)
            {
                xmlOutput += functionRequest.ToXML();
            }
            if (functionResponse != null)
            {
                xmlOutput += functionResponse.ToXML();
            }
            if(errorDetails != null)
            {
                xmlOutput += errorDetails.ToXML();
            }
            xmlOutput += "</Function>";
            return xmlOutput;
        }
    }
}
