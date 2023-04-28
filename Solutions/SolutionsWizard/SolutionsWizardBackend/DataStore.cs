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
    public class DataStore
    {
        public int DATASTORE_MULTIPLIER = 262144;
        public string name;
        public float totalSize;
        public float freeSpace;
        public string vSpherehostname = null;
        public int blockSize = 1;
        public double filesystemversion = 1;
        public string type = null;


        internal void Print()
        {
            Trace.WriteLine("Name = " + name, "TotalSize = " + totalSize + "FreeSpace " + freeSpace);
        }

        public object Clone()
        {
           
            MemoryStream memoryStream = new MemoryStream();

            BinaryFormatter binaryFormatter = new BinaryFormatter();

            binaryFormatter.Serialize(memoryStream, this);

            memoryStream.Position = 0;

            object obj = binaryFormatter.Deserialize(memoryStream);

            memoryStream.Close();

            return obj;

        }

        public long GetMaxDataFileSizeMB
        {
            get
            {
                return DATASTORE_MULTIPLIER * blockSize;
            }
            set
            {
                value = DATASTORE_MULTIPLIER * blockSize;
            }

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

                writer.WriteStartElement("datastore");
                writer.WriteAttributeString("datastore_name", name);
                writer.WriteAttributeString("vSpherehostname", vSpherehostname);
                writer.WriteAttributeString("total_size", totalSize.ToString());
                writer.WriteAttributeString("free_space", freeSpace.ToString());
                writer.WriteAttributeString("datastore_blocksize_mb", blockSize.ToString());
                writer.WriteAttributeString("filesystem_version", filesystemversion.ToString());
                writer.WriteAttributeString("type",type);
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
