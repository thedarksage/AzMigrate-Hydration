using System;
using System.Collections.Generic;
using System.Collections;
using System.Linq;
using System.Text;

namespace InMage.APIModel
{

    /*
     * Class: Body - The  class object hierarchy representing InMage Scout API Model
     *        Shared by both Request and Response Hierarchy  
     */
    public class Body : XMLSerialize
    {

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
            if (obj is FunctionRequest || obj is Function)
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
            String xmlOutput = String.Format("<Body>");
            foreach (XMLSerialize obj in childList)
            {
                xmlOutput += obj.ToXML();
            }
            xmlOutput += "</Body>";
            return xmlOutput;
        }
    }
}
