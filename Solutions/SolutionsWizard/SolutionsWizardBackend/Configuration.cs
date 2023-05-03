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
    public class Configurations
    {
        public int cpucount = 0;
        public float memsize = 0;
        public string vSpherehostname = null;
        internal void Print()
        {
            Trace.WriteLine("Cpu count = " + cpucount.ToString(), "networktype = " + memsize.ToString());
        }
        public object Clone()
        {

            MemoryStream ms = new MemoryStream();

            BinaryFormatter bf = new BinaryFormatter();

            bf.Serialize(ms, this);

            ms.Position = 0;

            object obj = bf.Deserialize(ms);

            ms.Close();

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
                writer.WriteStartElement("config");
                writer.WriteAttributeString("vSpherehostname", vSpherehostname);
                writer.WriteAttributeString("cpucount", cpucount.ToString());
                writer.WriteAttributeString("memsize", memsize.ToString());
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
