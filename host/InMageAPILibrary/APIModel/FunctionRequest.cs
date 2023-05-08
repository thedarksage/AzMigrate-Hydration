using System;
using System.Collections.Generic;
using System.Collections;
using System.Linq;
using System.Text;

namespace InMage.APIModel
{

    /*
     * Class: FunctionRequest - The  class object hierarchy representing InMage Scout API Model 
     */
    public class FunctionRequest : XMLSerialize
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

        /* Property: Include request in the response, default value is false */
        private Boolean include = false;
        public Boolean Include
        {
            get { return include; }
            set { include = value; }
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

        public void AddParameter(string name, string value)
        {
            addChildObj(new Parameter(name, value));
        }

        public void AddParameterGroup(ParameterGroup parameterGroup)
        {
            addChildObj(parameterGroup);
        }

        public void RemoveParameter(string name)
        {
            if(childList != null)
            {
                Parameter param;
                foreach(var item in childList)
                {
                    if(item is Parameter)
                    {
                        param = item as Parameter;
                        if (String.Compare(param.Name, name, StringComparison.OrdinalIgnoreCase) == 0)
                        {
                            childList.Remove(param);
                            break;
                        }
                    }
                }
            }
        }

        /* Converts this object to XML String */
        public string ToXML()
        {
            String incString = "No";
            if (include) { incString = "Yes"; }
            String xmlOutput = String.Format("<FunctionRequest Name=\"{0}\" Id=\"{1}\" include=\"{2}\">", name, id, incString);
            foreach (XMLSerialize obj in childList)
            {
                xmlOutput += obj.ToXML();
            }
            xmlOutput += "</FunctionRequest>";
            return xmlOutput;
        }
    }
}
