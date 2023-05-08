using System;
using System.Collections.Generic;
using System.Collections;
using System.Linq;
using System.Text;
using InMage.Logging;

namespace InMage.APIModel
{
    /*
     * Class: ParameterGroup - The  class object hierarchy representing InMage Scout API Model 
     */
    public class ParameterGroup : XMLSerialize
    {

        /* Default Parameter */

        public ParameterGroup() { }
        
        /* Constructor: Id as parameter */
        public ParameterGroup(String id)
        {
            this.id = id;
        }


        /* Property: Id */
        private String id;
        public String Id
        {
            get { return id; }
            set { id = value; }
        }

        /* Returns a unique id or key representing thi object i.e. Id */
        public String getKey()
        {
            return Id;
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

        /* Converts this object to XML String */
        public string ToXML()
        {
            String xmlOutput = String.Format("<ParameterGroup Id=\"{0}\">", id);
            foreach (XMLSerialize obj in childList)
            {
                xmlOutput += obj.ToXML();
            }
            xmlOutput += "</ParameterGroup>";
            return xmlOutput;
        }

        public String GetParameterValue(String key)
        {
            String value = null;
            Parameter pm;
            foreach (object obj in childList)
            {
                if (obj is Parameter)
                {
                    pm = obj as Parameter;
                    if (pm.Name.Equals(key, StringComparison.OrdinalIgnoreCase))
                    {
                        value = pm.Value;
                        break;
                    }
                }
            }
            if (value == null)
            {
                Logger.Error("Parameter:" + key + " has not found");
                value = "";
            }
            return value;
        }

        public ParameterGroup GetParameterGroup(String Id)
        {
            ParameterGroup paramGroup;
            foreach (object obj in childList)
            {
                if (obj is ParameterGroup)
                {
                    paramGroup = obj as ParameterGroup;
                    if (paramGroup.Id.Equals(Id, StringComparison.OrdinalIgnoreCase))
                    {
                        return paramGroup;
                    }
                }
            }

            Logger.Error("ParameterGroup:" + Id + " has not found");
            return null;
        }
    }
}
