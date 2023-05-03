namespace ExchangeValidation
{
    using System;
    using System.Collections.Generic;
    using System.Diagnostics;
    using System.Text;
    using System.Xml;
    using System.IO;

    internal class ExchangeXML
    {
        private Exchange exchObj = new Exchange();

        private HashSet<KeyValuePair<string, HashSet<string>>> ConstructSourceLogPath(HashSet<KeyValuePair<string, HashSet<string>>> tgtList, HashSet<KeyValuePair<string, string>> srcList, string evsName)
        {
            Trace.WriteLine("   DEBUG   ENTERED ConstructSourceLogPath()", DateTime.Now.ToString());
            HashSet<KeyValuePair<string, HashSet<string>>> set = new HashSet<KeyValuePair<string, HashSet<string>>>();
            HashSet<KeyValuePair<string, HashSet<string>>> set2 = new HashSet<KeyValuePair<string, HashSet<string>>>(tgtList);
            HashSet<KeyValuePair<string, string>> set3 = new HashSet<KeyValuePair<string, string>>(srcList);
            string str = evsName;
            try
            {
                foreach (KeyValuePair<string, HashSet<string>> pair in set2)
                {
                    HashSet<string> set4 = new HashSet<string>(pair.Value);
                    HashSet<string> set5 = new HashSet<string>();
                    string key = "";
                    foreach (KeyValuePair<string, string> pair2 in set3)
                    {
                        if (string.Compare(pair.Key, pair2.Value, true) == 0)
                        {
                            string str3 = pair2.Key;
                            foreach (string str4 in set4)
                            {
                                string item = str3;
                                key = str3;
                                if (!string.IsNullOrEmpty(str))
                                {
                                    item = @"\\";
                                    item = item + str + @"\" + str3.Replace(":", "$");
                                }
                                item = item + @"\" + str4;
                                set5.Add(item);
                            }
                            continue;
                        }
                    }
                    set.Add(new KeyValuePair<string, HashSet<string>>(key, set5));
                }
            }
            catch (Exception exception)
            {
                Console.WriteLine("Exception in ConstructSourceLogPath(). Message = {0}", exception.Message);
                Console.WriteLine(string.Format("Exception in ConstructSourceLogPath(). Message = {0}", exception.Message));
                set.Clear();
            }
            Trace.WriteLine("   DEBUG   EXITED ConstructSourceLogPath()", DateTime.Now.ToString());
            return set;
        }

        public XmlTextWriter CreateXMLFile(string agentInstallPath, string seq)
        {
            XmlTextWriter writer;
            Trace.WriteLine("   DEBUG   ENTERED CreateXMLFile()", DateTime.Now.ToString());
            string str = seq;
            string str2 = agentInstallPath;
            string str3 = str2 + @"\Failover\ConsistencyValidation";
            try
            {
                if (!Directory.Exists(str3))
                    Directory.CreateDirectory(str3);
                str3 = str3 + @"\XML_" + str + ".xml";
                Console.WriteLine("      XMLfilename = {0}", str3);
                Trace.WriteLine("   " + str3, "XML FilePath        ");
                writer = new XmlTextWriter(str3, Encoding.UTF8);
            }
            catch (Exception exception)
            {
                Console.WriteLine("Exception in CreateXMLFile(). Message = {0}", exception.Message);
                Trace.WriteLine(string.Format("Exception in CreateXMLFile(). Message = {0}", exception.Message));
                writer = null;
            }
            Trace.WriteLine("   DEBUG   EXITED CreateXMLFile()", DateTime.Now.ToString());
            return writer;
        }

        ~ExchangeXML()
        {
        }

        public bool PersistLogFilesIntoXML(HashSet<KeyValuePair<string, HashSet<string>>> validatedLogFiles, string sourceServerName, string evsName, string seq_num, HashSet<KeyValuePair<string, string>> sourceLogPath)
        {
            Trace.WriteLine("   DEBUG   ENTERED PersistLogFilesIntoXML()", DateTime.Now.ToString());
            bool flag = true;
            bool bIsSourceCluster = false;
            HashSet<KeyValuePair<string, HashSet<string>>> logFiles = new HashSet<KeyValuePair<string, HashSet<string>>>();
            Console.WriteLine("\n***** Started Persisting Exchange log files into XML");
            try
            {
                if (!string.IsNullOrEmpty(evsName))
                {
                    bIsSourceCluster = true;
                }
                logFiles = this.ConstructSourceLogPath(validatedLogFiles, sourceLogPath, evsName);
                if ((logFiles.Count != 0) && !this.WriteIntoXML(logFiles, sourceServerName, seq_num, bIsSourceCluster))
                {
                    flag = false;
                }
            }
            catch (Exception exception)
            {
                Console.WriteLine("Exception in PersistLogFilesIntoXML(). Message = {0}", exception.Message);
                Trace.WriteLine("Exception in PersistLogFilesIntoXML(). Message = {0}", exception.Message);
                flag = false;
            }
            Trace.WriteLine("   DEBUG   EXITED PersistLogFilesIntoXML()", DateTime.Now.ToString());
            return flag;
        }

        private bool WriteIntoXML(HashSet<KeyValuePair<string, HashSet<string>>> logFiles, string source, string num, bool bIsSourceCluster)
        {
            Trace.WriteLine("   DEBUG   ENTERED WriteIntoXML()", DateTime.Now.ToString());
            bool flag = true;
            string text = source;
            string seq = num;
            int num2 = 0;
            try
            {
                XmlTextWriter writer = this.CreateXMLFile(this.exchObj.inMageInstallPath, seq);
                writer.Formatting = Formatting.Indented;
                writer.WriteProcessingInstruction("xml", "version='1.0' encoding='UTF-8'");
                writer.WriteStartElement("Root");
                writer.WriteStartElement("Host");
                writer.WriteString(text);
                writer.WriteEndElement();
                if (bIsSourceCluster)
                {
                    writer.WriteStartElement("IsSourceCluster");
                    writer.WriteString("TRUE");
                    writer.WriteEndElement();
                }
                foreach (KeyValuePair<string, HashSet<string>> pair in logFiles)
                {
                    num2++;
                    writer.WriteStartElement(("StorageGroup" + num2.ToString()).ToString());
                    HashSet<string> set = new HashSet<string>(pair.Value);
                    foreach (string str3 in set)
                    {
                        writer.WriteStartElement("FilePath");
                        writer.WriteString(str3);
                        writer.WriteEndElement();
                    }
                    writer.WriteEndElement();
                    if (set.Count != 0)
                    {
                        Console.WriteLine("\nSuccessfully persisted Exchange Logs\n\tSource Exchange Log Path \"{0}\"", pair.Key);
                        Trace.WriteLine("   " + pair.Key, "Persist Exchange Log");
                    }
                    else
                    {
                        Console.WriteLine("\nFound No Exchange logs to persist for \"{0}\"", pair.Key);
                        Trace.WriteLine("   " + pair.Key, "No Exchange Log     ");
                    }
                }
                writer.WriteStartElement("TotalStorageGroup");
                writer.WriteString(num2.ToString());
                writer.WriteEndElement();
                writer.WriteEndElement();
                writer.Close();
            }
            catch (Exception exception)
            {
                Console.WriteLine("Exception in WriteIntoXML(). Message = {0}", exception.Message);
                Trace.WriteLine(string.Format("Exception in WriteIntoXML(). Message = {0}", exception.Message));
                flag = false;
            }
            Trace.WriteLine("   DEBUG   EXITED WriteIntoXML()", DateTime.Now.ToString());
            return flag;
        }
    }
}

