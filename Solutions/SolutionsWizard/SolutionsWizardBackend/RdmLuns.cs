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
     public class RdmLuns
    {
        public string name;
        public string adapter;
        public string lun;
        public double capacity_in_kb;
        public bool alreadyUsed = false;
        public string devicename;
        public string vSpherehostname = null;




        internal void Print()
        {
            Trace.WriteLine("Name = " + name, "Adapter = " + adapter + "Capacity_in_kb " + capacity_in_kb);
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
                    Trace.WriteLine("RDM LUNS:WriteXmlNode Got a null writer  value ");
                    MessageBox.Show("RDM LUNS:WriteXmlNode Got a null writer  value ");
                    return false;

                }

                writer.WriteStartElement("lun");
                writer.WriteAttributeString("name", name);
                writer.WriteAttributeString("adapter", adapter);
                writer.WriteAttributeString("lun", lun);
                writer.WriteAttributeString("devicename", devicename);
                writer.WriteAttributeString("capacity_in_kb", capacity_in_kb.ToString());
                writer.WriteAttributeString("vSpherehostname", vSpherehostname);
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
