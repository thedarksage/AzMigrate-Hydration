using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace InMage.APIInterfaces
{
    /*
     * Class: APIInterfaceFactory returns APIIntefaceObject
     */
    public class APIInterfaceFactory
    {
        
        /*
         *  Returns appropriate handler based on the values
         */ 
        public static APIInterface getAPIInterface(Dictionary<String, String> dictionary)
        {
            APIInterface apiInterface;

            // Create Appropriate APIInterface based on type
            //if (dictionary.ContainsKey("Type")) // True
            //{
            //    String type = dictionary["Type"];
            //    if (String.Compare(type, "CX", true) == 0)
            //    {
            //        apiInterface = new CXAPIInterface();
            //    }
            //    else
            //    {                    
            //        apiInterface = new LocalAPIInterface();
            //    }
            //}
            //else
            //{
            //    apiInterface = new LocalAPIInterface();                
            //}

            apiInterface = new CXAPIInterface();

            // Initialize APIInterface object
            apiInterface.init(dictionary);

            // Return the APIInterface

            return apiInterface;
        }

    }
}
