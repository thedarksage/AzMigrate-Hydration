using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.Xml;
using System.Windows.Forms;
using System.IO;
using System.Runtime.Serialization.Formatters.Binary;

namespace Com.Inmage.Esxcalls
{
    [Serializable]
    public class ResourcePool
    {
        public string host = null;
        public string name = null;
        public string groupId = null;
        public string type = null;
        public string owner = null;
        public string ownertype = null;
        public void Print()
        {
            Trace.WriteLine("Name = " + name, "networktype = " + host);
        }
        internal object Clone()
        {

            MemoryStream memoryStream = new MemoryStream();

            BinaryFormatter binaryFormatter = new BinaryFormatter();

            binaryFormatter.Serialize(memoryStream, this);

            memoryStream.Position = 0;

            object obj = binaryFormatter.Deserialize(memoryStream);

            memoryStream.Close();

            return obj;

        }
        internal bool WriteXmlNode(XmlWriter writer)
        {
            try
            {
                if (writer == null)
                {
                    Trace.WriteLine(DateTime.Now + "\t DataStore:WriteXmlNode Got a null writer  value ");
                    MessageBox.Show("DataStore:WriteXmlNode Got a null writer  value ");
                    return false;

                }
                writer.WriteStartElement("resourcepool");
                writer.WriteAttributeString("host", host);
                writer.WriteAttributeString("name", name);
                writer.WriteAttributeString("groupId", groupId);
                writer.WriteAttributeString("type", type);
                writer.WriteAttributeString("owner", owner);
                writer.WriteAttributeString("owner_type", ownertype);
                writer.WriteEndElement();
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;

            }

            return true;


        }

    }
}
