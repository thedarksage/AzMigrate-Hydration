using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace InMage.APIInterfaces
{
    /*
     *  Interface: APIInterface - API Transport implementation
     */ 
    public interface APIInterface
    {
     
        /* Initialize APIInterface Object */
        void init(Dictionary<String, String> dictionary);
       
        /* Process/post RequestXML */
        String post(String requestXML);
    }
}
