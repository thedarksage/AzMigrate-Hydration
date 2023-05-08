using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace InMage.APIModel
{
    /*
    * Interface: XMLSerialize: The object implementing this interface should provide Object->XML serialization
    */
    public interface XMLSerialize
    {
        /* Converts this object to XML String */
        String ToXML();
    }
}
