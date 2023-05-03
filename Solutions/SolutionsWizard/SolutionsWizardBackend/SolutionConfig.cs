using System;
using System.Collections.Generic;
using System.Text;
using System.Xml;
using System.Diagnostics;
using Com.Inmage.Esxcalls;
using System.IO;
using System.Collections;
using Com.Inmage.Tools;
using System.Windows.Forms;
using System.Threading;
using System.Security.AccessControl;
namespace Com.Inmage
{
    public class SolutionConfig
    {
        internal string Windowsdir = "Windows";
        internal string Processserverfile = "ProcessSvr.txt";
        internal string Physicalmachinelistfile = "Physical_Machine_list.txt";
        internal string Physicalmachinelistfiletemp = "Physical_Machine_list_Temp.txt";
        internal string Installpath;
        XmlNodeList Datastorenodes = null;
        XmlNodeList Nicnodelist = null;
        XmlNodeList Lunnodes = null;
        XmlNodeList Networknodes = null;
        XmlNodeList Resourcepoolnodes = null;
        XmlNodeList Configurationnodes = null;
        XmlNodeList Processservernodes = null;
        string Latestfilepath = null;
        internal static bool Isremovexml = false;
        internal static HostList sourceList = new HostList();
        internal static HostList masterList= new HostList();
        internal static Host masterHost = new Host();
        internal static string esxIP= null;
        internal static string cxIP = null;
        internal static string altGuestName = null;
        internal static Host linHost = new Host();
       
        internal string Masterconfigfile = "MasterConfigFile.xml";
        public SolutionConfig()
        {
            Installpath = WinTools.FxAgentPath() + "\\vContinuum";
            Latestfilepath = WinTools.FxAgentPath() + "\\vContinuum\\Latest";
        }
		public class HostCompare : IComparer
        {

            // Calls CaseInsensitiveComparer.Compare with the parameters reversed.
            int IComparer.Compare(Object x, Object y)
            {
                Host host1 = (Host)x;
                Host host2 = (Host)y;

                string host1Name = host1.hostname;
                string host2Name = host2.hostname;

                return host1Name.CompareTo(host2Name);
            }
        }
        

        public bool WriteXmlFile(HostList inSourceHostList, HostList inMasterTargetList, Esx inTargetEsx, string incxIP, string inFileName, string inPlanName, bool thinProvision)
        {

            try
            {
                // this is to prepare the esx.xmla nd recover.xml for both the xml we are maintaining only one method.
                // int index = 0;

                try
                {
                    IComparer myComparer = new HostCompare();
                
                    inSourceHostList._hostList.Sort(myComparer);
                }
                catch (Exception ex)
                {
                    Trace.WriteLine(DateTime.Now + "\t Failed to sort the list " + ex.Message);
                }

                XmlWriterSettings settings = new XmlWriterSettings();
                settings.Indent = true;
                settings.ConformanceLevel = ConformanceLevel.Fragment;
                if (inFileName == "Remove.xml" && inPlanName == "Remove")
                {
                    Isremovexml = true;

                }
                else
                {
                    Isremovexml = false;
                }

                using (XmlWriter writer = XmlWriter.Create(Latestfilepath +"\\"+ inFileName, settings))
                {
                    writer.WriteStartElement("root");
                    //writer.WriteStartAttribute("CX_IP","");
                    //writer.WriteString( inCxIP );
                    string versionNumber = WinTools.VersionNumber();

                    writer.WriteAttributeString("ConfigFileVersion",versionNumber.Substring(0,3));
                    writer.WriteAttributeString("cx_ip", incxIP);
                    writer.WriteAttributeString("cx_portnumber", "80");
                    writer.WriteAttributeString("inmage_cx_uuid", "");
                    writer.WriteAttributeString("cx_nat_ip", "null");
                    writer.WriteAttributeString("plan", inPlanName);
                    writer.WriteAttributeString("batchresync", "0");
                    writer.WriteAttributeString("AdditionOfDisk", "false");
                    writer.WriteAttributeString("ArrayBasedDrDrill", "false");
                    writer.WriteAttributeString("V2P", "false");
                    writer.WriteAttributeString("thin_disk", thinProvision.ToString());                    
                    writer.WriteAttributeString("xmlDirectoryName", "null");                   
                    writer.WriteAttributeString("ParallelCdpcli", "12");
                    
                    writer.WriteAttributeString("vConVersion", WinTools.VersionNumber().ToString());
                    //writer.WriteEndAttribute();


                    Debug.WriteLine("enter the cx ip =================\t" + incxIP);


                    writer.WriteStartElement("SRC_ESX");
                    inSourceHostList.WriteHostsToXml(writer);
                    writer.WriteEndElement();

                    writer.WriteStartElement("TARGET_ESX");
                    writer.WriteStartAttribute("ip", "");
                    //writer.WriteString(inTargetEsx.ip);
                    writer.WriteEndAttribute();

                    inMasterTargetList.WriteHostsToXml(writer);
                    /*Commented below lines as it is getting bulky with vcenter support as vcenter contains lot of datastores/luns/resource pools
                    we can enable this whenever required.
                    protection and recovery are not using datastore/luns/resource pool which are under TARGET_ESX tag in esx.xml */


                    //inTargetEsx.WriteDataStoreXmlNodes(writer);
                    //inTargetEsx.WriteConfigurationNodes(writer);
                    //inTargetEsx.WriteNetworkNodes(writer);
                   // inTargetEsx.WriteLunXmlNodes(writer);
                   // inTargetEsx.WriteResourcePollNodes(writer);
                    writer.WriteEndElement();
                    writer.Close();
                    WinTools win = new WinTools();
                    if (win.SetFilePermissions(Latestfilepath + "\\" + inFileName) == true)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Succesfully set the file permissions ");
                    }
                    
                }
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                System.FormatException formatException = new FormatException("Inner exception", ex);
                Trace.WriteLine(formatException.GetBaseException());
                Trace.WriteLine(formatException.Data);
                Trace.WriteLine(formatException.InnerException);
                Trace.WriteLine(formatException.Message);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;

            }

            return true;
        }


    

        internal bool AddOrReplaceMasterXmlFile(string localXml)
        {
            try
            {
                // With this method we are going to prepare masterConfigFile.xml
                // First download any existing file in the cx and then we will check 
                // any existing vms protected if so we will remove that node..
                // and we will append entire esx.xml to that xml file.
                // lastly we will upload the file to the cx.......
                Trace.WriteLine(DateTime.Now + " \t  Entered: AddOrReplaceMasterXmlFile() in SolutionConfig.cs");             
                                             
                if (!File.Exists(Latestfilepath + "\\" + Masterconfigfile))
                {  //Create New file .xml//
                   
                    XmlWriterSettings settings = new XmlWriterSettings();
                    settings.Indent = true;
                    settings.ConformanceLevel = ConformanceLevel.Fragment;
                    using (XmlWriter writer = XmlWriter.Create(Latestfilepath + "\\" + Masterconfigfile, settings))
                    {
                        writer.WriteStartElement("start");
                        writer.WriteEndElement();

                        writer.Close();
                        AppendXml(localXml);
                    }
                }
                else
                {


                    RemoveEntryIfAlreadyExists(localXml);
                    //Append to existing file
                    AppendXml(localXml);
                }
                
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                System.FormatException formatException = new FormatException("Inner exception", ex);
                Trace.WriteLine(formatException.GetBaseException());
                Trace.WriteLine(formatException.Data);
                Trace.WriteLine(formatException.InnerException);
                Trace.WriteLine(formatException.Message);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;

            }
            Trace.WriteLine(DateTime.Now + " \t  Exiting: AddOrReplaceMasterXmlFile() in SolutionConfig.cs");
            return true;
        
        }

        private bool RemoveEntryIfAlreadyExists(string localXml)
        {
            try
            {
                Trace.WriteLine(DateTime.Now + " \t  Entered: RemoveEntryIfAlreadyExists() in SolutionConfig.cs");

                //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
                //based on above comment we have added xmlresolver as null
                // after dowloading the xml we will check any existing node are there we will remove that nodes and append the new nodes.
                XmlDocument documentEsx = new XmlDocument();
                XmlDocument documentMasterEsx = new XmlDocument();
                documentEsx.XmlResolver = null;
                documentMasterEsx.XmlResolver = null;
                string esxXmlFile = localXml;
                string esxMasterXmlFile = Latestfilepath + "\\" + Masterconfigfile;

                //StreamReader reader = new StreamReader(esxXmlFile);
                //string xmlString = reader.ReadToEnd();


                //StreamReader reader1 = new StreamReader(esxMasterXmlFile);
                //string xmlMasterString = reader1.ReadToEnd();
                //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
                XmlReaderSettings settings = new XmlReaderSettings();
                settings.ProhibitDtd = true;
                settings.XmlResolver = null;
                using (XmlReader reader2 = XmlReader.Create(esxXmlFile, settings))
                {
                   // reader2.Close();
                    documentEsx.Load(reader2);
                }
                using (XmlReader reader3 = XmlReader.Create(esxMasterXmlFile, settings))
                {
                    documentMasterEsx.Load(reader3);

                    //reader3.Close();
                }
                //reader.Close();
                //reader1.Close();
                XmlNodeList hostNodesEsxXml = null, hostNodesEsxMasterXml = null;
                hostNodesEsxXml = documentEsx.GetElementsByTagName("host");
                hostNodesEsxMasterXml = documentMasterEsx.GetElementsByTagName("host");
                foreach (XmlNode esxnode in hostNodesEsxXml)
                {
                    foreach (XmlNode esxMasternode in hostNodesEsxMasterXml)
                    {
                        if (esxMasternode.Attributes["offlineSync"] != null && esxMasternode.Attributes["offlineSync"].Value.Length != 0 && esxMasternode.Attributes["offlineSync"].Value.ToUpper().ToString() == "True".ToUpper().ToString())
                        {
                            Trace.WriteLine(DateTime.Now + "\t This is Offlice sync protection ");
                            if (esxMasternode.Attributes["source_uuid"] != null && esxMasternode.Attributes["source_uuid"].Value.Length != 0 && esxnode.Attributes["source_uuid"] != null && esxnode.Attributes["source_uuid"].Value.Length != 0)
                            {
                                Trace.WriteLine(DateTime.Now + "\t Comparing with uuids beacuse targetvSphereHost value is null");
                                Debug.WriteLine(DateTime.Now + "\t Comparing the source uuids " + esxnode.Attributes["source_uuid"].Value + " " + esxMasternode.Attributes["source_uuid"].Value + " \t source vm uuid " + esxnode.Attributes["source_uuid"].Value + "\t  master node target vm  uuid " + esxMasternode.Attributes["target_uuid"].Value);
                                if (((esxnode.Attributes["source_uuid"].Value == esxMasternode.Attributes["source_uuid"].Value) || (esxnode.Attributes["source_uuid"].Value == esxMasternode.Attributes["target_uuid"].Value)) )
                                {

                                    Debug.WriteLine(DateTime.Now + "\t Came here because uuids  are matched");
                                    if (esxMasternode.ParentNode.Name == "SRC_ESX")
                                    {
                                        if (RemoveParticularNode(esxMasternode, esxnode) == true)
                                        {
                                            break;
                                        }

                                    }
                                    //break;
                                }
                            }
                            else
                            {
                                // For p2v case source uuid is null it will come here to compare the displaynames. For older versions like vcon 1.0 and 1.2 
                                //V2v protection also can come here while sourceuuid is null......
                                Trace.WriteLine(DateTime.Now + "\t Source uuids are null assuming that it is a old protection and checking with the display names");
                                Trace.WriteLine(DateTime.Now + "\t Comparing the display names " + esxnode.Attributes["display_name"].Value + " " + esxMasternode.Attributes["display_name"].Value);
                                if ((esxnode.Attributes["display_name"].Value == esxMasternode.Attributes["display_name"].Value || esxnode.Attributes["hostname"].Value == esxMasternode.Attributes["hostname"].Value) )
                                {
                                    Trace.WriteLine(DateTime.Now + "\t Came here because displaynames are matched");
                                    if (esxMasternode.ParentNode.Name == "SRC_ESX")
                                    {
                                        if (RemoveParticularNode(esxMasternode, esxnode) == true)
                                        {
                                            break;
                                        }
                                    }
                                    //break;
                                }
                            }
                        }
                        else
                        {
                            if (esxMasternode.Attributes["source_uuid"] != null && esxMasternode.Attributes["source_uuid"].Value.Length != 0 && esxnode.Attributes["source_uuid"] != null && esxnode.Attributes["source_uuid"].Value.Length != 0)
                            {
                                Trace.WriteLine(DateTime.Now + "\t Comparing with uuids");
                                Trace.WriteLine(DateTime.Now + "\t Comparing the source uuids " + esxnode.Attributes["source_uuid"].Value + " " + esxMasternode.Attributes["source_uuid"].Value + " \t source vm uuid " + esxnode.Attributes["source_uuid"].Value + "\t  master node target vm  uuid " + esxMasternode.Attributes["target_uuid"].Value);

                                if (esxMasternode.Attributes["targetvSphereHostName"] != null && esxMasternode.Attributes["targetvSphereHostName"].Value.Length != 0 && esxMasternode.Attributes["vSpherehostname"] != null && esxMasternode.Attributes["vSpherehostname"].Value.Length != 0 && esxnode.Attributes["vSpherehostname"] !=null && esxnode.Attributes["vSpherehostname"].Value.Length != 0)
                                {
                                    Trace.WriteLine(DateTime.Now + "\t target vSpherehost nodes in mc file " +  esxMasternode.Attributes["targetvSphereHostName"].Value +" esx.xml value "  + esxnode.Attributes["targetvSphereHostName"].Value);
                                    Trace.WriteLine(DateTime.Now + "\t Source vSphereHost nodes " + esxMasternode.Attributes["vSpherehostname"].Value + "\t Source vSphere host in esx.xml " + esxnode.Attributes["vSpherehostname"].Value);
                                    if (((esxnode.Attributes["source_uuid"].Value == esxMasternode.Attributes["source_uuid"].Value) || (esxnode.Attributes["source_uuid"].Value == esxMasternode.Attributes["target_uuid"].Value)) && ((esxMasternode.Attributes["targetvSphereHostName"].Value == esxnode.Attributes["targetvSphereHostName"].Value) || (esxMasternode.Attributes["vSpherehostname"].Value == esxnode.Attributes["targetvSphereHostName"].Value)))
                                    {
                                        Trace.WriteLine(DateTime.Now + "\t Came here because uuids  are matched");
                                        if (esxMasternode.ParentNode.Name == "SRC_ESX")
                                        {
                                            if (RemoveParticularNode(esxMasternode, esxnode) == true)
                                            {
                                                break;
                                            }
                                        }
                                        //break;
                                    }
                                    else
                                    {
                                        if (esxMasternode.Attributes["mt_uuid"] != null && esxMasternode.Attributes["mt_uuid"].Value.Length > 0)
                                        {
                                            if ((esxnode.Attributes["source_uuid"].Value == esxMasternode.Attributes["source_uuid"].Value) && (esxnode.Attributes["mt_uuid"].Value == esxMasternode.Attributes["mt_uuid"].Value))
                                            {
                                                Trace.WriteLine(DateTime.Now + "\t Came here because uuids  are matched");
                                                if (esxMasternode.ParentNode.Name == "SRC_ESX")
                                                {
                                                    if (RemoveParticularNode(esxMasternode, esxnode) == true)
                                                    {
                                                        break;
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    
                                }
                                else  
                                {
                                    if (((esxnode.Attributes["source_uuid"].Value == esxMasternode.Attributes["source_uuid"].Value) || (esxnode.Attributes["source_uuid"].Value == esxMasternode.Attributes["target_uuid"].Value)) && ((esxMasternode.Attributes["targetesxip"].Value == esxnode.Attributes["targetesxip"].Value) || (esxMasternode.Attributes["esxIp"].Value == esxnode.Attributes["targetesxip"].Value)))
                                    {
                                        Debug.WriteLine(DateTime.Now + "\t Came here because uuids  are matched");
                                        if (esxMasternode.ParentNode.Name == "SRC_ESX")
                                        {
                                            if (RemoveParticularNode(esxMasternode, esxnode) == true)
                                            {
                                                break;
                                            }
                                        }
                                        else
                                        {
                                            if (esxMasternode.Attributes["mt_uuid"] != null && esxMasternode.Attributes["mt_uuid"].Value.Length > 0)
                                            {
                                                if ((esxnode.Attributes["source_uuid"].Value == esxMasternode.Attributes["source_uuid"].Value) && (esxnode.Attributes["mt_uuid"].Value == esxMasternode.Attributes["mt_uuid"].Value))
                                                {
                                                    Trace.WriteLine(DateTime.Now + "\t Came here because uuids  are matched");
                                                    if (esxMasternode.ParentNode.Name == "SRC_ESX")
                                                    {
                                                        if (RemoveParticularNode(esxMasternode, esxnode) == true)
                                                        {
                                                            break;
                                                        }
                                                    }
                                                }
                                            }
                                        }

                                        //break;
                                    }
                                    
                                }
                            }
                            else
                            {
                                // For p2v case source uuid is null it will come here to compare the displaynames. For older versions like vcon 1.0 and 1.2 
                                //V2v protection also can come here while sourceuuid is null......
                                Trace.WriteLine(DateTime.Now + "\t Source uuids are null assuming that it is a old protection and checking with the display names");
                                Trace.WriteLine(DateTime.Now + "\t Comparing the display names " + esxnode.Attributes["display_name"].Value + " " + esxMasternode.Attributes["display_name"].Value);
                                if (esxMasternode.Attributes["targetvSphereHostName"] != null && esxMasternode.Attributes["targetvSphereHostName"].Value.Length != 0 && esxMasternode.Attributes["vSpherehostname"] != null && esxMasternode.Attributes["vSpherehostname"].Value.Length != 0 && esxnode.Attributes["vSpherehostname"] != null && esxnode.Attributes["vSpherehostname"].Value.Length != 0)
                                {
                                    if (((esxnode.Attributes["display_name"].Value == esxMasternode.Attributes["display_name"].Value) || (esxnode.Attributes["hostname"].Value == esxMasternode.Attributes["hostname"].Value)) && ((esxMasternode.Attributes["targetvSphereHostName"].Value == esxnode.Attributes["targetvSphereHostName"].Value) || (esxMasternode.Attributes["vSpherehostname"].Value == esxnode.Attributes["targetvSphereHostName"].Value)))
                                    {
                                        Debug.WriteLine(DateTime.Now + "\t Came here because uuids  are matched");
                                        if (esxMasternode.ParentNode.Name == "SRC_ESX")
                                        {
                                            if (RemoveParticularNode(esxMasternode, esxnode) == true)
                                            {
                                                break;
                                            }
                                        }
                                        //break;
                                    }
                                    
                                }
                                else if (esxnode.Attributes["targetesxip"].Value != null)
                                {
                                    if ((esxnode.Attributes["display_name"].Value == esxMasternode.Attributes["display_name"].Value || esxnode.Attributes["hostname"].Value == esxMasternode.Attributes["hostname"].Value) && ((esxMasternode.Attributes["targetesxip"].Value == esxnode.Attributes["targetesxip"].Value) || (esxMasternode.Attributes["esxIp"].Value == esxnode.Attributes["targetesxip"].Value)))
                                    {
                                        Trace.WriteLine(DateTime.Now + "\t Came here because displaynames are matched");
                                        if (esxMasternode.ParentNode.Name == "SRC_ESX")
                                        {
                                            if (RemoveParticularNode(esxMasternode, esxnode) == true)
                                            {
                                                break;
                                            }

                                        }
                                        //break;
                                    }

                                }
                                else
                                {
                                    if ((esxnode.Attributes["display_name"].Value == esxMasternode.Attributes["display_name"].Value || esxnode.Attributes["hostname"].Value == esxMasternode.Attributes["hostname"].Value) )
                                    {
                                        Trace.WriteLine(DateTime.Now + "\t Came here because displaynames are matched");
                                        if (esxMasternode.ParentNode.Name == "SRC_ESX")
                                        {
                                            if (RemoveParticularNode(esxMasternode, esxnode) == true)
                                            {
                                                break;
                                            }

                                        }
                                        //break;
                                    }
                                }
                            }
                        }
                    }
                }
                try
                {
                    documentEsx.Save(esxXmlFile);
                }
                catch (Exception ex)
                {
                    Trace.WriteLine("\t Exception at " + esxXmlFile);
                    System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                    System.FormatException formatException = new FormatException("Inner exception", ex);
                    Trace.WriteLine(formatException.GetBaseException());
                    Trace.WriteLine(formatException.Data);
                    Trace.WriteLine(formatException.InnerException);
                    Trace.WriteLine(formatException.Message);
                    Trace.WriteLine("ERROR*******************************************");
                    Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                    Trace.WriteLine("Exception  =  " + ex.Message);
                    Trace.WriteLine("ERROR___________________________________________");
                    return false;

                }
                try
                {
                    documentMasterEsx.Save(esxMasterXmlFile);
                }
                catch (Exception ex)
                {
                    Trace.WriteLine("\t Exception at " + esxMasterXmlFile);
                    System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                    System.FormatException formatException = new FormatException("Inner exception", ex);
                    Trace.WriteLine(formatException.GetBaseException());
                    Trace.WriteLine(formatException.Data);
                    Trace.WriteLine(formatException.InnerException);
                    Trace.WriteLine(formatException.Message);
                    Trace.WriteLine("ERROR*******************************************");
                    Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                    Trace.WriteLine("Exception  =  " + ex.Message);
                    Trace.WriteLine("ERROR___________________________________________");
                    return false;

                }
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                System.FormatException formatException = new FormatException("Inner exception", ex);
                Trace.WriteLine(formatException.GetBaseException());
                Trace.WriteLine(formatException.Data);
                Trace.WriteLine(formatException.InnerException);
                Trace.WriteLine(formatException.Message);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;

            }
            Trace.WriteLine(DateTime.Now + " \t  Exiting: RemoveEntryIfAlreadyExists() in SolutionConfig.cs");
            return true;
        }


        private static bool RemoveParticularNode(XmlNode esxMasterNode, XmlNode esxnode)
        {
            if (esxMasterNode.Attributes["local_backup"] != null && esxMasterNode.Attributes["local_backup"].Value.Length != 0)
            {
                // In case of local back up we need not to remvove thr host from the masterconfigfile.xml 
                //So we added local_backup node to configaration files.

                //if (Convert.ToBoolean(esxMasterNode.Attributes["local_backup"].Value.ToString()) == false)
                //{
                //    Trace.WriteLine(DateTime.Now + " Removing the existing node " + esxMasterNode.Attributes["display_name"].Value);
                //    XmlNode parentNode = esxMasterNode.ParentNode;
                //    esxMasterNode.ParentNode.RemoveChild(esxMasterNode);
                //    if (!parentNode.HasChildNodes)
                //    {
                //        Trace.WriteLine(DateTime.Now + "\t Came here to remove entire root nodes");
                //        parentNode.ParentNode.ParentNode.RemoveChild(parentNode.ParentNode);
                //    }
                //}
                //else
                //{
                if (esxMasterNode.Attributes["local_backup"].Value == esxnode.Attributes["local_backup"].Value)
                {
                    Trace.WriteLine(DateTime.Now + " Removing the existing node " + esxMasterNode.Attributes["display_name"].Value);
                    XmlNode parentNode = esxMasterNode.ParentNode;
                    esxMasterNode.ParentNode.RemoveChild(esxMasterNode);
                    if (!parentNode.HasChildNodes)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Came here to remove entire root nodes");
                        parentNode.ParentNode.ParentNode.RemoveChild(parentNode.ParentNode);
                    }
                }
                else
                {
                    Trace.WriteLine(DateTime.Now + "\t Doesn't match to remove node");
                    return false;
                }
                //}
            }
            else
            {
                Trace.WriteLine(DateTime.Now + " Removing the existing node " + esxMasterNode.Attributes["display_name"].Value);
                XmlNode parentNode = esxMasterNode.ParentNode;
                esxMasterNode.ParentNode.RemoveChild(esxMasterNode);
                if (!parentNode.HasChildNodes)
                {
                    Trace.WriteLine(DateTime.Now + "\t Came here to remove entire root nodes");
                    parentNode.ParentNode.ParentNode.RemoveChild(parentNode.ParentNode);
                }
            }
            return true;
        }


        internal bool AppendXml(string localXml)
        {
            try
            {// We will append entire esx.xml to the esx_master_targetesxip.xml....
                Trace.WriteLine(DateTime.Now + " \t  Entered: AppendXml() in SolutionConfig.cs");
                //Reading esx.xml for further use.

              //  XmlTextReader xmlReader = new XmlTextReader(localXml);

                // Load the source of the XML file into an XmlDocument
                //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
                //based on above comment we have added xmlresolver as null
                XmlDocument originalfile = new XmlDocument();
                originalfile.XmlResolver = null;
                // Load the source XML file into the first document
                //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
                XmlReaderSettings settings = new XmlReaderSettings();
                settings.ProhibitDtd = true;
                settings.XmlResolver = null;
                using (XmlReader reader1 = XmlReader.Create(localXml, settings))
                {
                    originalfile.Load(reader1);
                    //reader1.Close();
                }
               // originalfile.Load(localXml);

                // Close the reader

                //xmlReader.Close();


                Debug.WriteLine("prepared initial master xml file");

                // Open the reader with the destination XML file

                //xmlReader = new XmlTextReader(Latestfilepath + "\\" + Masterconfigfile);

                // Load the source of the XML file into an XmlDocument
                //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
                //based on above comment we have added xmlresolver as null
                XmlDocument filetoMerge = new XmlDocument();
                filetoMerge.XmlResolver = null;
                // Load the destination XML file into the first document
                //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
                using (XmlReader reader2 = XmlReader.Create(Latestfilepath + "\\" + Masterconfigfile, settings))
                {
                    filetoMerge.Load(reader2);
                    //reader2.Close();
                }
                //filetoMerge.Load(Latestfilepath + "\\" + Masterconfigfile);

                // Close the reader

               // xmlReader.Close();

                XmlNode rootNodeOfSourcefile = filetoMerge["start"];


                // Store the node to be copied into an XmlNode

                XmlNode nodeOrig = originalfile["root"];

                // Store the copy of the original node into an XmlNode

                XmlNode nodeDest = filetoMerge.ImportNode(nodeOrig, true);

                // Append the node being copied to the root of the destination document

                rootNodeOfSourcefile.AppendChild(nodeDest);


                XmlTextWriter myWriter = new XmlTextWriter(Latestfilepath + "\\" + Masterconfigfile, Encoding.UTF8);

                // Indented for easy reading

                myWriter.Formatting = Formatting.Indented;

                // Write the file

                filetoMerge.WriteTo(myWriter);

                // Close the writer

                myWriter.Close();
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                System.FormatException formatException = new FormatException("Inner exception", ex);
                Trace.WriteLine(formatException.GetBaseException());
                Trace.WriteLine(formatException.Data);
                Trace.WriteLine(formatException.InnerException);
                Trace.WriteLine(formatException.Message);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;

            }
            Trace.WriteLine(DateTime.Now + " \t  Exiting: AppendXml() in SolutionConfig.cs");
            return true;
        
        }
        public bool ReadXmlConfigFileWithMasterList(string inFileName, HostList hostList, HostList masterHostList, string targetesxIP, string csIP)
        {
            try
            {
                // this will read the cml file with all master targets list which are in esx_master_targetesxip.xml...
                Trace.WriteLine(DateTime.Now + " \t Entered: ReadXmlConfigFile to read xml file ");
                // Debug.WriteLine("This method is called");
                //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
                //based on above comment we have added xmlresolver as null
                XmlDocument document = new XmlDocument();
                document.XmlResolver = null;
                //string ip = "";
                string sourceHostVersion = null;

                XmlNodeList hostNodes = null;
                XmlNodeList esxNodes = null;


                string fileFullPath = Latestfilepath + "\\" + inFileName;
               

                if (!File.Exists(fileFullPath))
                {
                    File.Create(fileFullPath);
                    Trace.WriteLine("01_002_001: The file " + fileFullPath + " could not be located" + "in dir " + Installpath);

                    // Console.WriteLine("01_002_001: The file " + fileFullPath + " could not be located" + "in dir " + _installPath);
                    return false;
                }

                try
                {
                    //StreamReader reader = new StreamReader(fileFullPath);
                    //string xmlString = reader.ReadToEnd();
                    //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
                    XmlReaderSettings settings = new XmlReaderSettings();
                    settings.ProhibitDtd = true;
                    settings.XmlResolver = null;
                    using (XmlReader reader1 = XmlReader.Create(fileFullPath, settings))
                    {
                        document.Load(reader1);
                        //reader1.Close();
                    }
                    //document.Load(fileFullPath);
                  
                    //reader.Close();
                }
                catch (XmlException xmle)
                {

                    System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(xmle, true);
                    System.FormatException formatException = new FormatException("Inner exception", xmle);
                    Trace.WriteLine(formatException.GetBaseException());
                    Trace.WriteLine(formatException.Data);
                    Trace.WriteLine(formatException.InnerException);
                    Trace.WriteLine(formatException.Message);
                    Trace.WriteLine("ERROR*******************************************");
                    Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                    Trace.WriteLine("Exception  =  " + xmle.Message);
                    Trace.WriteLine("ERROR___________________________________________");
                    Console.WriteLine(xmle.Message);
                    return false;
                }

                esxNodes = document.GetElementsByTagName("root");
                string versionNumebr = null;

                //Get IP address of ESX 
                foreach (XmlNode node in esxNodes)
                {

                    XmlAttributeCollection attribColl = node.Attributes;
                    if (node.Attributes["cx_ip"] != null)
                    {
                        csIP = attribColl.GetNamedItem("cx_ip").Value.ToString();
                    }
                    

                }





                esxNodes = document.GetElementsByTagName("SRC_ESX");

                //Get IP address of ESX 
                foreach (XmlNode node in esxNodes)
                {
                    XmlAttributeCollection attribColl = node.Attributes;
                    /*if (attribColl.GetNamedItem("ip") != null && attribColl.GetNamedItem("ip").Value.Length > 0)
                    {
                        ip = attribColl.GetNamedItem("ip").Value.ToString();
                    }*/
                    try
                    {// Getting hostversion of the source esx in case less than 2.1 versions.
                        if (attribColl.GetNamedItem("Source_ESX_version") != null && attribColl.GetNamedItem("Source_ESX_version").Value.Length > 0)
                        {
                            sourceHostVersion = attribColl.GetNamedItem("Source_ESX_version").Value.ToString();
                        }
                    }
                    catch (Exception ex)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Failed ot read the sourcehostversion " + ex.Message);
                    }
                }

                esxNodes = document.GetElementsByTagName("TARGET_ESX");

                //Get IP address of ESX 
                foreach (XmlNode node in esxNodes)
                {
                    // Debug.WriteLine("Enter to read the target information");

                    XmlAttributeCollection attribColl = node.Attributes;
                    if (attribColl.GetNamedItem("ip") == null)
                    {
                        Debug.WriteLine("Enter to read the target esx ip as null");
                    }
                    else
                    {

                        targetesxIP = attribColl.GetNamedItem("ip").Value.ToString();
                        // Debug.WriteLine("Reading IP " + esxIp);
                    }
                    



                }

                // MessageBox.Show("Entered here");
                hostNodes = document.GetElementsByTagName("host");
                Datastorenodes = document.GetElementsByTagName("datastore");
                Lunnodes = document.GetElementsByTagName("lun");
                Networknodes = document.GetElementsByTagName("network");
                Resourcepoolnodes = document.GetElementsByTagName("resourcepool");
                Nicnodelist = document.GetElementsByTagName("nic");
                Configurationnodes = document.GetElementsByTagName("config");
                Processservernodes = document.GetElementsByTagName("process_server");
                // Console.WriteLine("Values = {0}, {1}, {2}, {3} \n", host.hostName, host.displayName, host.ip, host.os);
                foreach (XmlNode node in hostNodes)
                {
                    Host h = new Host();

                    h = ReadHostNode(node,inFileName);

                    if (h.hostversion == null)
                    {
                        Debug.WriteLine(DateTime.Now + "\t Came here to read hostversion and comparing source_esx " + node.ParentNode.Name + "\t for the host " + h.displayname);
                        if (node.ParentNode.Name == "SRC_ESX")
                        {
                            XmlAttributeCollection attribColl = node.Attributes;
                            if (attribColl.GetNamedItem("Source_ESX_version") != null && attribColl.GetNamedItem("Source_ESX_version").Value.Length > 0)
                            {
                                sourceHostVersion = attribColl.GetNamedItem("Source_ESX_version").Value.ToString();
                            }
                            Trace.WriteLine(DateTime.Now + "\t Assigning the value for thehost " + sourceHostVersion);
                            h.hostversion = sourceHostVersion;
                        }
                    }
                    if (h.new_displayname == null)
                    {
                        h.new_displayname = h.displayname;
                    }
                    //MessageBox.Show("Entered here1");
                    Debug.WriteLine(DateTime.Now + "\t Printing the host and displaynames " + h.hostname + "\t displayname " + h.displayname);
                    if (h != null)
                    {
                        if (h.role == Role.Secondary)
                        {
                            //MessageBox.Show("Entered here2");
                            //h.Print();
                            masterHostList.AddOrReplaceHost(h);

                        }
                        else
                        {
                            //Trace.WriteLine(DateTime.Now + " \t Printing vcon name  = " + h.vConName);
                            hostList.AddOrReplaceHost(h);

                        }

                    }

                }
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                System.FormatException formatException = new FormatException("Inner exception", ex);
                Trace.WriteLine(formatException.GetBaseException());
                Trace.WriteLine(formatException.Data);
                Trace.WriteLine(formatException.InnerException);
                Trace.WriteLine(formatException.Message);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;

            }
            Trace.WriteLine(DateTime.Now + " \t Exiting: ReadXmlConfigFile to read xml file ");
            sourceList = hostList;
            masterList = masterHostList;
            esxIP = targetesxIP;
            cxIP = csIP;
            Trace.WriteLine(DateTime.Now + "\t Printing host count " + sourceList._hostList.Count.ToString());
            Trace.WriteLine(DateTime.Now + "\t Hostlist count " + hostList._hostList.Count.ToString());
            return true;

        }


        public static HostList GetHostList
        {
            get
            {
                return sourceList;
            }
            set
            {
                value = sourceList;
            }
        }

        public static HostList GetMasterList
        {
            get
            {
                return masterList;
            }
            set
            {
                value = masterList;
            }
        }

        public string GetEsxIP
        {
            get
            {
                return esxIP;
            }
            set
            {
                value = esxIP;
            }
        }

        public string GetcxIP
        {
            get
            {
                return cxIP;
            }
            set
            {
                value = cxIP;
            }
        }

        public bool ReadXmlConfigFile(string inFileName, HostList hostList, Host masHost, string targetesxIP, string xmlcxIP)
        {
            try
            {
                // This will read the xml file with the single mt in the 5.5 releae...
                Trace.WriteLine(DateTime.Now + " \t Entered: ReadXmlConfigFile to read xml file ");
                // Debug.WriteLine("This method is called");
                //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
                //based on above comment we have added xmlresolver as null
                XmlDocument document = new XmlDocument();
                document.XmlResolver = null;
                string ip = "";

                string sourceHostVersion = null;

                XmlNodeList hostNodes = null;
                XmlNodeList esxNodes = null;


                string fileFullPath = Latestfilepath + "\\" + inFileName;
               


                if (!File.Exists(fileFullPath))
                {
                    File.Create(fileFullPath);
                    Trace.WriteLine("01_002_001: The file " + fileFullPath + " could not be located" + "in dir " + Installpath);

                    // Console.WriteLine("01_002_001: The file " + fileFullPath + " could not be located" + "in dir " + _installPath);
                    return false;


                }

                try
                {
                    //StreamReader reader = new StreamReader(fileFullPath);
                    //string xmlString = reader.ReadToEnd();
                    //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
                    XmlReaderSettings settings = new XmlReaderSettings();
                    settings.ProhibitDtd = true;
                    settings.XmlResolver = null;
                    using (XmlReader reader1 = XmlReader.Create(fileFullPath, settings))
                    {
                        document.Load(reader1);
                        //reader1.Close();
                    }
                   // reader.Close();
                }
                catch (XmlException xmle)
                {

                    System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(xmle, true);
                    System.FormatException formatException = new FormatException("Inner exception", xmle);
                    Trace.WriteLine(formatException.GetBaseException());
                    Trace.WriteLine(formatException.Data);
                    Trace.WriteLine(formatException.InnerException);
                    Trace.WriteLine(formatException.Message);
                    Trace.WriteLine("ERROR*******************************************");
                    Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                    Trace.WriteLine("Exception  =  " + xmle.Message);
                    Trace.WriteLine("ERROR___________________________________________");
                    Console.WriteLine(xmle.Message);
                    return false;
                }

                esxNodes = document.GetElementsByTagName("root");


                //Get IP address of ESX 
                foreach (XmlNode node in esxNodes)
                {

                    XmlAttributeCollection attribColl = node.Attributes;
                    if (node.Attributes["cx_ip"] != null)
                    {
                        xmlcxIP = attribColl.GetNamedItem("cx_ip").Value.ToString();
                    }
                }





                esxNodes = document.GetElementsByTagName("SRC_ESX");

                //Get IP address of ESX 
                foreach (XmlNode node in esxNodes)
                {
                    XmlAttributeCollection attribColl = node.Attributes;
                   /* if (attribColl.GetNamedItem("ip") != null && attribColl.GetNamedItem("ip").Value.Length > 0)
                    {
                        ip = attribColl.GetNamedItem("ip").Value.ToString();
                    }*/
                    try
                    {// Getting hostversion of the source esx in case less than 2.1 versions.
                        if (attribColl.GetNamedItem("Source_ESX_version") != null && attribColl.GetNamedItem("Source_ESX_version").Value.Length > 0)
                        {
                            sourceHostVersion = attribColl.GetNamedItem("Source_ESX_version").Value.ToString();
                        }
                    }
                    catch (Exception ex)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Failed ot read the sourcehostversion " + ex.Message);
                    }
                }

                esxNodes = document.GetElementsByTagName("TARGET_ESX");

                //Get IP address of ESX 
                foreach (XmlNode node in esxNodes)
                {
                    // Debug.WriteLine("Enter to read the target information");

                    XmlAttributeCollection attribColl = node.Attributes;
                    if (attribColl.GetNamedItem("ip") == null)
                    {
                        Debug.WriteLine("Enter to read the target esx ip as null");
                    }
                    else
                    {

                        targetesxIP = attribColl.GetNamedItem("ip").Value.ToString();
                        // Debug.WriteLine("Reading IP " + esxIp);
                    }
                }
                // MessageBox.Show("Entered here");
                
                hostNodes = document.GetElementsByTagName("host");
                Datastorenodes = document.GetElementsByTagName("datastore");
                Lunnodes = document.GetElementsByTagName("lun");
                Resourcepoolnodes = document.GetElementsByTagName("resourcepool");
                Networknodes = document.GetElementsByTagName("network");
                Nicnodelist = document.GetElementsByTagName("nic");
                Configurationnodes = document.GetElementsByTagName("config");
                Processservernodes = document.GetElementsByTagName("process_server");
                // Console.WriteLine("Values = {0}, {1}, {2}, {3} \n", host.hostName, host.displayName, host.ip, host.os);
                foreach (XmlNode node in hostNodes)
                {
                    Host h = new Host();

                    h = ReadHostNode(node,inFileName);
                    if (h.hostversion == null)
                    {
                        Debug.WriteLine(DateTime.Now + "\t Came here to read hostversion and comparing source_esx " + node.ParentNode.Name + "\t for the host "+ h.displayname);
                        if (node.ParentNode.Name == "SRC_ESX")
                        {
                            XmlAttributeCollection attribColl = node.Attributes;
                            if (attribColl.GetNamedItem("Source_ESX_version") != null && attribColl.GetNamedItem("Source_ESX_version").Value.Length > 0)
                            {
                                sourceHostVersion = attribColl.GetNamedItem("Source_ESX_version").Value.ToString();
                            }
                            Trace.WriteLine(DateTime.Now + "\t Assigning the value for thehost " + sourceHostVersion);
                            h.hostversion = sourceHostVersion;
                        }
                    }
                    if (h.new_displayname == null)
                    {
                        h.new_displayname = h.displayname;
                    }
                    //MessageBox.Show("Entered here1");
                    if (h != null)
                    {
                        if (h.role == Role.Secondary)
                        {
                            //MessageBox.Show("Entered here2");

                            masHost = h;

                        }
                        else
                        {
                            //Trace.WriteLine(DateTime.Now + "\t prinitng the vconname = " + h.vConName);
                            hostList.AddOrReplaceHost(h);

                        }

                    }

                }
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                System.FormatException formatException = new FormatException("Inner exception", ex);
                Trace.WriteLine(formatException.GetBaseException());
                Trace.WriteLine(formatException.Data);
                Trace.WriteLine(formatException.InnerException);
                Trace.WriteLine(formatException.Message);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;

            }
            Trace.WriteLine(DateTime.Now + " \t Exiting: ReadXmlConfigFile to read xml file ");
            sourceList = hostList;
            masterHost = masHost;
            esxIP = targetesxIP;
            cxIP = xmlcxIP;
            return true;

        }

        public static HostList list
        {
            get
            {
                return sourceList;
            }
            set
            {
                value = sourceList;
            }
        }

        public static HostList masterHostlist
        {
            get
            {
                return masterList;
            }
            set
            {
                value = masterList;
            }
        }

        public static Host masterHosts
        {
            get
            {
                return masterHost;
            }
            set
            {
                value = masterHost;
            }
        }

        public static string EsxIP
        {
            get
            {
                return esxIP;
            }
            set
            {
                value = esxIP;
            }
        }

        public static string csIP
        {
            get
            {
                return cxIP;
            }
            set
            {
                value = cxIP;
            }
        }

        private Host ReadHostNode(XmlNode node, string xml)
        {   
            // this will read particular host node in the xml file
            XmlNodeReader reader = null;
            string numOfDisksString;
            XmlAttributeCollection attribColl = node.Attributes;
            Host host = new Host();
            reader = new XmlNodeReader(node);
            reader.Read();
            try
            {
                //Trace.WriteLine("   Entered Read Host Node" + reader.GetAttribute("display_name"));
                host.hostname = reader.GetAttribute("hostname");
                host.displayname = reader.GetAttribute("display_name");
                host.ip = reader.GetAttribute("ip_address");
                if (host.ip != null)
                {
                    host.IPlist = host.ip.Split(',');
                    host.ipCount = host.IPlist.Length;
                }
                string osType = reader.GetAttribute("os_info");
                if (reader.GetAttribute("target_uuid") != null && attribColl.GetNamedItem("target_uuid").Value.Length != 0)
                {
                    host.target_uuid = reader.GetAttribute("target_uuid");
                }
                if (reader.GetAttribute("source_uuid") != null && attribColl.GetNamedItem("source_uuid").Value.Length != 0)
                {
                    host.source_uuid = reader.GetAttribute("source_uuid");
                }
                if (reader.GetAttribute("mt_uuid") != null && attribColl.GetNamedItem("mt_uuid").Value.Length != 0)
                {
                    host.mt_uuid = reader.GetAttribute("mt_uuid");
                }
                if (reader.GetAttribute("vconname") != null && attribColl.GetNamedItem("vconname").Value.Length != 0)
                {
                    host.vConName = reader.GetAttribute("vconname");
                   // //Trace.WriteLine(DateTime.Now + " \t Printing the vCon name at thetime of protection " + host.vConName);
                }
                if (reader.GetAttribute("diskenableuuid") != null && attribColl.GetNamedItem("diskenableuuid").Value.Length != 0)
                {
                    host.diskenableuuid = reader.GetAttribute("diskenableuuid");
                    // //Trace.WriteLine(DateTime.Now + " \t Printing the vCon name at thetime of protection " + host.vConName);
                }
                if (reader.GetAttribute("new_displayname") != null && attribColl.GetNamedItem("new_displayname").Value.Length != 0)
                {
                    host.new_displayname = reader.GetAttribute("new_displayname");
                    // //Trace.WriteLine(DateTime.Now + " \t Printing the vCon name at thetime of protection " + host.vConName);
                }
                if (reader.GetAttribute("targetvSphereHostName") != null && attribColl.GetNamedItem("targetvSphereHostName").Value.Length != 0)
                {
                    host.targetvSphereHostName = reader.GetAttribute("targetvSphereHostName");
                    // //Trace.WriteLine(DateTime.Now + " \t Printing the vCon name at thetime of protection " + host.vConName);
                }
                if (reader.GetAttribute("recovery_plan_id") != null && attribColl.GetNamedItem("recovery_plan_id").Value.Length != 0)
                {
                    host.recoveryPlanId = reader.GetAttribute("recovery_plan_id");                   
                }
                if (reader.GetAttribute("resourcePool_src") != null && attribColl.GetNamedItem("resourcePool_src").Value.Length != 0)
                {
                    host.resourcePool_src = reader.GetAttribute("resourcePool_src");
                    // //Trace.WriteLine(DateTime.Now + " \t Printing the vCon name at thetime of protection " + host.vConName);
                }
                if (reader.GetAttribute("resourcepoolgrpname_src") != null && attribColl.GetNamedItem("resourcepoolgrpname_src").Value.Length != 0)
                {
                    host.resourcepoolgrpname_src = reader.GetAttribute("resourcepoolgrpname_src");
                    // //Trace.WriteLine(DateTime.Now + " \t Printing the vCon name at thetime of protection " + host.vConName);
                }
                if (reader.GetAttribute("offlineSync") != null && attribColl.GetNamedItem("offlineSync").Value.Length != 0)
                {
                    host.offlineSync = Convert.ToBoolean(reader.GetAttribute("offlineSync"));
                    // //Trace.WriteLine(DateTime.Now + " \t Printing the vCon name at thetime of protection " + host.vConName);
                }
                if (reader.GetAttribute("efi") != null && attribColl.GetNamedItem("efi").Value.Length != 0)
                {
                    host.efi = Convert.ToBoolean(reader.GetAttribute("efi"));
                    // //Trace.WriteLine(DateTime.Now + " \t Printing the vCon name at thetime of protection " + host.vConName);
                }
                host.os = (OStype)Enum.Parse(typeof(OStype), osType);
                host.vmx_path = reader.GetAttribute("vmx_path");
                host.machineType = reader.GetAttribute("machinetype");
                host.operatingSystem = reader.GetAttribute("operatingsystem");
                if (attribColl.GetNamedItem("alt_guest_name") != null && attribColl.GetNamedItem("alt_guest_name").Value != null)
                {
                    host.alt_guest_name = attribColl.GetNamedItem("alt_guest_name").Value.ToString();
                }
                else
                {
                    if (host.operatingSystem != null)
                    {
                        SetAlgGuestName(host.operatingSystem,  host.alt_guest_name);

                        host.alt_guest_name = GetAltGuestname;
                    }
                }   
                host.datastore = reader.GetAttribute("datastore");
                host.plan = reader.GetAttribute("plan");
                //Trace.WriteLine(DateTime.Now + "\t " + host.plan);
                host.esxIp = reader.GetAttribute("esxIp");
                host.masterTargetHostName = reader.GetAttribute("mastertargethostname");
                host.masterTargetDisplayName = reader.GetAttribute("mastertargetdisplayname");
                host.targetESXIp = reader.GetAttribute("targetesxip");
                host.folder_path = reader.GetAttribute("folder_path");
                host.poweredStatus = reader.GetAttribute("powered_status");
                host.snapshot = reader.GetAttribute("snapshot");
                host.resourcePool = reader.GetAttribute("resourcepool");
                host.failOver = reader.GetAttribute("failover");
                host.failback = reader.GetAttribute("failback");
                host.rdmpDisk = reader.GetAttribute("rdm");
                host.convertRdmpDiskToVmdk = Convert.ToBoolean(reader.GetAttribute("convert_rdm_to_vmdk"));
                if (attribColl.GetNamedItem("local_backup") != null && attribColl.GetNamedItem("local_backup").Value.Length != 0)
                {
                    host.local_backup = Convert.ToBoolean(attribColl.GetNamedItem("local_backup").Value.ToString());
                }
                if (attribColl.GetNamedItem("vsan") != null && attribColl.GetNamedItem("vsan").Value.Length != 0)
                {
                    host.vSan = Convert.ToBoolean(attribColl.GetNamedItem("vsan").Value.ToString());
                }
                if (attribColl.GetNamedItem("vsan_folder") != null && attribColl.GetNamedItem("vsan_folder").Value.Length != 0)
                {
                    host.vsan_folder = attribColl.GetNamedItem("vsan_folder").Value.ToString();
                }
                if (attribColl.GetNamedItem("old_vm_folder_path") != null && attribColl.GetNamedItem("old_vm_folder_path").Value.Length != 0)
                {
                    host.old_vm_folder_path = attribColl.GetNamedItem("old_vm_folder_path").Value.ToString();
                }
                if (reader.GetAttribute("cluster") != null)
                {
                    host.cluster = reader.GetAttribute("cluster");
                }
                if (reader.GetAttribute("ide_count") != null)
                {
                    host.ide_count = reader.GetAttribute("ide_count");
                }
                else
                {
                    host.ide_count = null;
                }
                if (attribColl.GetNamedItem("cpucount") != null && attribColl.GetNamedItem("cpucount").Value.Length != 0)
                {
                    host.cpuCount = int.Parse(attribColl.GetNamedItem("cpucount").Value.ToString());
                }
                if (attribColl.GetNamedItem("mbr_path") != null && attribColl.GetNamedItem("mbr_path").Value.Length != 0)
                {
                    host.mbr_path = attribColl.GetNamedItem("mbr_path").Value.ToString();
                }
                if (attribColl.GetNamedItem("system_volume") != null && attribColl.GetNamedItem("system_volume").Value.Length != 0)
                {
                    host.system_volume = attribColl.GetNamedItem("system_volume").Value.ToString();
                }
                if (attribColl.GetNamedItem("inmage_hostid") != null && attribColl.GetNamedItem("inmage_hostid").Value.Length !=0 )
                {
                    host.inmage_hostid = attribColl.GetNamedItem("inmage_hostid").Value.ToString();
                }
                if (attribColl.GetNamedItem("vmx_version") != null && attribColl.GetNamedItem("vmx_version").Value.Length != 0)
                {
                    host.vmxversion = attribColl.GetNamedItem("vmx_version").Value.ToString();
                }
                if (attribColl.GetNamedItem("cluster_nodes") != null && attribColl.GetNamedItem("cluster_nodes").Value.Length != 0)
                {
                    host.clusterNodes = attribColl.GetNamedItem("cluster_nodes").Value.ToString();
                }
                if (attribColl.GetNamedItem("clusternodes_inmageguids") != null && attribColl.GetNamedItem("clusternodes_inmageguids").Value.Length != 0)
                {
                    host.clusterInmageUUIds = attribColl.GetNamedItem("clusternodes_inmageguids").Value.ToString();
                }
                if (attribColl.GetNamedItem("cluster_name") != null && attribColl.GetNamedItem("cluster_name").Value.Length != 0)
                {
                    host.clusterName = attribColl.GetNamedItem("cluster_name").Value.ToString();
                }
                if (attribColl.GetNamedItem("clusternodes_mbrfiles") != null && attribColl.GetNamedItem("clusternodes_mbrfiles").Value.Length != 0)
                {
                    host.clusterMBRFiles = attribColl.GetNamedItem("clusternodes_mbrfiles").Value.ToString();
                }
                if (attribColl.GetNamedItem("cluster_nodes") != null && attribColl.GetNamedItem("cluster_nodes").Value.Length != 0)
                {
                    host.clusterNodes = attribColl.GetNamedItem("cluster_nodes").Value.ToString();
                }
                if (attribColl.GetNamedItem("floppy_device_count") != null && attribColl.GetNamedItem("floppy_device_count").Value.Length != 0)
                {
                    host.floppy_device_count = attribColl.GetNamedItem("floppy_device_count").Value.ToString();
                }
                             
                if (attribColl.GetNamedItem("memsize") != null && attribColl.GetNamedItem("memsize").Value.Length != 0)
                {
                    host.memSize = int.Parse(attribColl.GetNamedItem("memsize").Value.ToString());
                }
               
                if (attribColl.GetNamedItem("resourcepoolgrpname") != null && attribColl.GetNamedItem("resourcepoolgrpname").Value.Length != 0)
                {
                    host.resourcepoolgrpname = attribColl.GetNamedItem("resourcepoolgrpname").Value.ToString();
                }
               
                if (attribColl.GetNamedItem("timezone") != null && attribColl.GetNamedItem("timezone").Value.Length != 0)
                {
                    host.timeZone = attribColl.GetNamedItem("timezone").Value.ToString();
                }
                if (attribColl.GetNamedItem("isSourceNatted") != null && attribColl.GetNamedItem("isSourceNatted").Value.Length != 0)
                {
                    host.isSourceNatted = Convert.ToBoolean(attribColl.GetNamedItem("isSourceNatted").Value.ToString());
                }
                if (attribColl.GetNamedItem("isTargetNatted") != null && attribColl.GetNamedItem("isTargetNatted").Value.Length != 0)
                {
                    host.isTargetNatted = Convert.ToBoolean(attribColl.GetNamedItem("isTargetNatted").Value.ToString());
                }
                if (attribColl.GetNamedItem("batchresync") != null && attribColl.GetNamedItem("batchresync").Value.Length != 0)
                {
                    host.batchResync = int.Parse(attribColl.GetNamedItem("batchresync").Value.ToString());
                }
                if (attribColl.GetNamedItem("vSpherehostname") != null && attribColl.GetNamedItem("vSpherehostname").Value.Length != 0)
                {
                    host.vSpherehost = attribColl.GetNamedItem("vSpherehostname").Value.ToString();
                }
                if (attribColl.GetNamedItem("hostversion") != null && attribColl.GetNamedItem("hostversion").Value.Length != 0)
                {
                  
                    host.hostversion = attribColl.GetNamedItem("hostversion").Value.ToString();
                    //Trace.WriteLine(DateTime.Now + "\t Hotversion " + host.hostversion);
                }
                if (attribColl.GetNamedItem("datacenter") != null && attribColl.GetNamedItem("datacenter").Value.Length != 0)
                {
                    host.datacenter = attribColl.GetNamedItem("datacenter").Value.ToString();
                }
                if (attribColl.GetNamedItem("role") != null && attribColl.GetNamedItem("role").Value.Length != 0)
                {
                    if (attribColl.GetNamedItem("role").Value.ToString().ToUpper() == "PRIMARY")
                    {
                        host.role = Role.Primary;
                    }
                    else
                    {
                        host.role = Role.Secondary;
                    }
                }
                if (attribColl.GetNamedItem("cxIP") != null && attribColl.GetNamedItem("cxIP").Value.Length != 0)
                {
                    host.cxIP = attribColl.GetNamedItem("cxIP").Value.ToString();
                }
                if (attribColl.GetNamedItem("vCenterProtection") != null && attribColl.GetNamedItem("vCenterProtection").Value.Length != 0)
                {
                    host.vCenterProtection = attribColl.GetNamedItem("vCenterProtection").Value.ToString();
                }
                if (attribColl.GetNamedItem("vConversion") != null && attribColl.GetNamedItem("vConversion").Value.Length != 0)
                {
                    host.vConversion = attribColl.GetNamedItem("vConversion").Value.ToString();
                }
                if (reader.GetAttribute("vmDirectoryPath") != null && attribColl.GetNamedItem("vmDirectoryPath").Value.Length != 0)
                {
                    host.vmDirectoryPath = reader.GetAttribute("vmDirectoryPath");
                    host.oldVMDirectory = reader.GetAttribute("vmDirectoryPath");
                    // //Trace.WriteLine(DateTime.Now + " \t Printing the vCon name at thetime of protection " + host.vConName);
                }
                if (reader.GetAttribute("compression") != null && attribColl.GetNamedItem("compression").Value.Length != 0)
                {
                    if (reader.GetAttribute("compression") == "source_base")
                    {
                        host.sourceBasedCompression = true;
                    }
                    else
                    {
                        host.cxbasedCompression = true;
                    }
                    // //Trace.WriteLine(DateTime.Now + " \t Printing the vCon name at thetime of protection " + host.vConName);
                }
                else
                {
                    host.cxbasedCompression = true;
                }
                if (reader.GetAttribute("secure_ps_to_tgt") != null && attribColl.GetNamedItem("secure_ps_to_tgt").Value.Length != 0)
                {
                    host.secure_ps_to_tgt = bool.Parse(reader.GetAttribute("secure_ps_to_tgt")); 
                    
                    // //Trace.WriteLine(DateTime.Now + " \t Printing the vCon name at thetime of protection " + host.vConName);
                }
                if (reader.GetAttribute("secure_src_to_ps") != null && attribColl.GetNamedItem("secure_src_to_ps").Value.Length != 0)
                {
                    host.secure_src_to_ps = bool.Parse(reader.GetAttribute("secure_src_to_ps"));

                    // //Trace.WriteLine(DateTime.Now + " \t Printing the vCon name at thetime of protection " + host.vConName);
                }
                if (reader.GetAttribute("selectedSparce") != null && attribColl.GetNamedItem("selectedSparce").Value.Length != 0)
                {
                    host.selectedSparce = bool.Parse(reader.GetAttribute("selectedSparce"));

                   
                }
                if (reader.GetAttribute("sparceDaysSelected") != null && attribColl.GetNamedItem("sparceDaysSelected").Value.Length != 0)
                {
                    host.sparceDaysSelected = bool.Parse(reader.GetAttribute("sparceDaysSelected"));                   
                }
                if (reader.GetAttribute("sparceWeeksSelected") != null && attribColl.GetNamedItem("sparceWeeksSelected").Value.Length != 0)
                {
                    host.sparceWeeksSelected = bool.Parse(reader.GetAttribute("sparceWeeksSelected"));
                }
                if (reader.GetAttribute("sparceMonthsSelected") != null && attribColl.GetNamedItem("sparceMonthsSelected").Value.Length != 0)
                {
                    host.sparceMonthsSelected = bool.Parse(reader.GetAttribute("sparceMonthsSelected"));
                }
                //Debug.WriteLine(host.hostName + " " + host.ip + " " + host.displayName + host.operatingSystem );
                // Console.WriteLine("Values = {0}, {1}, {2}, {3} \n", host.hostName, host.displayName, host.ip, host.os);
                numOfDisksString = reader.GetAttribute("number_of_disks");

                if ((numOfDisksString != null) && (numOfDisksString != string.Empty))
                {
                    host.numOfDisks = Int32.Parse(numOfDisksString);
                }

                if (host.role == Role.Secondary)
                {
                    //host.role = role.SECONDARY;
                    if (node.HasChildNodes)
                    {
                        XmlNodeList networknodes = node.ChildNodes;
                        foreach (XmlNode network in networknodes)
                        {
                            if (network.Name == "network")
                            {
                                XmlAttributeCollection attribCollAttribute = network.Attributes;
                                reader = new XmlNodeReader(network);
                                Network networkList = new Network();
                                reader.Read();
                                if (attribCollAttribute.GetNamedItem("name") != null && attribCollAttribute.GetNamedItem("name").Value.Length > 0)
                                {
                                    networkList.Networkname = reader.GetAttribute("name");
                                }
                                if (attribCollAttribute.GetNamedItem("Type") != null && attribCollAttribute.GetNamedItem("Type").Value.Length > 0)
                                {
                                    networkList.Networktype = reader.GetAttribute("Type");
                                }
                                if (attribCollAttribute.GetNamedItem("vSpherehostname") != null && attribCollAttribute.GetNamedItem("vSpherehostname").Value.Length > 0)
                                {
                                    networkList.Vspherehostname = reader.GetAttribute("vSpherehostname");
                                }
                                if (attribCollAttribute.GetNamedItem("switchUuid") != null && attribCollAttribute.GetNamedItem("switchUuid").Value.Length > 0)
                                {
                                    networkList.Switchuuid = reader.GetAttribute("switchUuid");
                                }
                                if (attribCollAttribute.GetNamedItem("portgroupKey") != null && attribCollAttribute.GetNamedItem("portgroupKey").Value.Length > 0)
                                {
                                    networkList.PortgroupKey = reader.GetAttribute("portgroupKey");
                                }
                                host.networkNameList.Add(networkList);

                            }

                        }
                    }
                    if (node.HasChildNodes)
                    {
                        XmlNodeList configNodes = node.ChildNodes;
                        foreach (XmlNode config in configNodes)
                        {
                            if (config.Name == "config")
                            {
                                XmlAttributeCollection attribCollAttribute = config.Attributes;
                                reader = new XmlNodeReader(config);
                                Configurations configuration = new Configurations();
                                reader.Read();
                                if (attribCollAttribute.GetNamedItem("memsize") != null && attribCollAttribute.GetNamedItem("memsize").Value.Length > 0)
                                {
                                    configuration.memsize = float.Parse(reader.GetAttribute("memsize").ToString());
                                }
                                if (attribCollAttribute.GetNamedItem("cpucount") != null && attribCollAttribute.GetNamedItem("cpucount").Value.Length > 0)
                                {
                                    configuration.cpucount = int.Parse(reader.GetAttribute("cpucount").ToString());
                                }
                                if (attribCollAttribute.GetNamedItem("vSpherehostname") != null && attribCollAttribute.GetNamedItem("vSpherehostname").Value.Length > 0)
                                {
                                    configuration.vSpherehostname = reader.GetAttribute("vSpherehostname");
                                }
                                host.configurationList.Add(configuration);

                            }

                        }
                    }
                    foreach (XmlNode nodes in Datastorenodes)
                    {
                        // Debug.WriteLine("Reaches to read the datastore nodes");
                        DataStore dataStore = ReadDataStoreNode(nodes);
                        //   _dataStoreList.Add(dataStore);

                        host.dataStoreList.Add(dataStore);
                    }
                    ArrayList _dataStoreList = new ArrayList();
                    foreach (XmlNode nodes in Datastorenodes)
                    {
                        //Debug.WriteLine("Reaches to read the datastore nodes");
                        DataStore dataStore = ReadDataStoreNode(nodes);

                        foreach (DataStore d in _dataStoreList)
                        {
                            if (d.name == dataStore.name && d.vSpherehostname == dataStore.vSpherehostname)
                            {
                                host.dataStoreList.Remove(d);
                            }
                        }

                        _dataStoreList.Add(dataStore);
                    }
                    Trace.WriteLine(DateTime.Now + "\t Prinitng the name of xml " + xml);
                    if (xml.Contains("MasterConfigFile.xml"))
                    {
                        Trace.WriteLine(DateTime.Now + "\t Reading Masterconfigfile");
                    }
                    else
                    {
                        Trace.WriteLine(DateTime.Now + "\t REading esx.xml ");
                        if (host.configurationList.Count == 0)
                        {
                            foreach (XmlNode nodes in Configurationnodes)
                            {
                                //Trace.WriteLine(DateTime.Now + " \t Came here to readthe config nodes");
                                Configurations config = ReadConfigurationNodes(nodes);
                                host.configurationList.Add(config);
                            }
                        }
                        if (host.networkNameList.Count == 0)
                        {
                            foreach (XmlNode nodes in Networknodes)
                            {
                                Network network = ReadNetWorkNode(nodes);
                                //host.networkNameList = new ArrayList();
                                host.networkNameList.Add(network);
                            }
                        }
                    }
                    foreach (XmlNode nodes in Resourcepoolnodes)
                    {
                        ResourcePool resourcePool = ReadResourcePoolNode(node);
                        host.resourcePoolList.Add(resourcePool);
                    }
                    foreach (XmlNode lunNode in Lunnodes)
                    {
                        // MessageBox.Show("Came to read lunNode");
                        RdmLuns lun = ReadLunNodes(lunNode);
                        /*if (host.lunList.Count > 0)
                        {
                            int index = 0;
                            foreach (RdmLuns luns in host.lunList)
                            {
                                index++;
                                if (lun.name == luns.name)
                                {
                                    host.lunList.RemoveAt(index);
                                }
                            }
                            
                        }*/
                        host.lunList.Add(lun);
                    }

                    // XmlNodeReader reader = null;
                }
                else
                {
                    host.role = Role.Primary;
                    if (node.HasChildNodes)
                    {
                        XmlNodeList processServer = node.ChildNodes;
                        foreach (XmlNode psIP in processServer)
                        {
                            if (psIP.Name == "process_server")
                            {
                                //XmlAttributeCollection attribCollforDisk = psIP.Attributes;
                                XmlAttributeCollection attribCollAttribute = psIP.Attributes;
                                reader = new XmlNodeReader(psIP);
                                reader.Read();
                                if (attribCollAttribute.GetNamedItem("IP") != null && attribCollAttribute.GetNamedItem("IP").Value.Length > 0)
                                {
                                    host.processServerIp = reader.GetAttribute("IP");
                                }
                            }                        
                        }
                    }
                    if (node.HasChildNodes)
                    {
                        XmlNodeList retentionPath = node.ChildNodes;
                        foreach(XmlNode retention  in retentionPath)
                        {
                            if (retention.Name == "retention")
                            {
                                XmlAttributeCollection attribCollAttribute = retention.Attributes;
                                reader = new XmlNodeReader(retention);
                                reader.Read();
                                if (attribCollAttribute.GetNamedItem("log_data_dir") != null && attribCollAttribute.GetNamedItem("log_data_dir").Value.Length > 0)
                                {
                                    host.retentionPath = reader.GetAttribute("log_data_dir");
                                }
                                if (attribCollAttribute.GetNamedItem("retention_drive") != null && attribCollAttribute.GetNamedItem("retention_drive").Value.Length > 0)
                                {
                                    host.retention_drive = reader.GetAttribute("retention_drive");
                                }
                                if (attribCollAttribute.GetNamedItem("retain_change_days") != null && attribCollAttribute.GetNamedItem("retain_change_days").Value.Length > 0)
                                {
                                    host.retentionInDays = reader.GetAttribute("retain_change_days");
                                }
                                if (attribCollAttribute.GetNamedItem("retain_change_MB") != null && attribCollAttribute.GetNamedItem("retain_change_MB").Value.Length > 0)
                                {
                                    host.retentionLogSize = reader.GetAttribute("retain_change_MB");
                                }
                                if (attribCollAttribute.GetNamedItem("retain_change_hours") != null && attribCollAttribute.GetNamedItem("retain_change_hours").Value.Length > 0)
                                {
                                    try
                                    {
                                        host.retentionInHours = int.Parse(reader.GetAttribute("retain_change_hours").ToString());
                                    }
                                    catch (Exception ex)
                                    {
                                        Trace.WriteLine(DateTime.Now + "\t Failed to read in hour retention " + ex.Message);
                                    }
                                }
                            }
                        }
                    }
                    
                    if (node.HasChildNodes)
                    {
                        XmlNodeList consistency = node.ChildNodes;
                        foreach (XmlNode time in consistency)
                        {
                            if (time.Name == "consistency_schedule")
                            {
                                XmlAttributeCollection attribCollAttribute = time.Attributes;
                                reader = new XmlNodeReader(time);
                                reader.Read();
                                if (attribCollAttribute.GetNamedItem("mins") != null && attribCollAttribute.GetNamedItem("mins").Value.Length > 0)
                                {
                                    host.consistencyInmins = reader.GetAttribute("mins");
                                }

                            }
                        }
                    }
                    if (node.HasChildNodes)
                    {
                        XmlNodeList sparcetime = node.ChildNodes;
                        foreach (XmlNode time in sparcetime)
                        {
                            //Trace.WriteLine(DateTime.Now + "\t name " + time.Name);
                            if (time.Name == "time")
                            {
                                if (time.HasChildNodes)
                                {
                                    XmlNodeList timeWindow1 = time.ChildNodes;
                                    foreach (XmlNode timesparce in timeWindow1)
                                    {
                                        if (timesparce.Name == "time_interval_window1")
                                        {
                                            XmlAttributeCollection attribCollAttribute = timesparce.Attributes;
                                            reader = new XmlNodeReader(timesparce);
                                            reader.Read();
                                            if (attribCollAttribute.GetNamedItem("value") != null && attribCollAttribute.GetNamedItem("value").Value.Length > 0)
                                            {
                                                host.unitsofTimeIntervelWindow1Value = int.Parse(reader.GetAttribute("value").ToString());
                                                Trace.WriteLine(DateTime.Now + "\t days  Value " + host.unitsofTimeIntervelWindow1Value.ToString());
                                            }
                                            if (attribCollAttribute.GetNamedItem("bookmark_count") != null && attribCollAttribute.GetNamedItem("bookmark_count").Value.Length > 0)
                                            {
                                                host.bookMarkCountWindow1 = int.Parse(reader.GetAttribute("bookmark_count").ToString());
                                                Trace.WriteLine(DateTime.Now + "\t bookmark count " + host.bookMarkCountWindow1.ToString());
                                            }
                                            if (attribCollAttribute.GetNamedItem("range") != null && attribCollAttribute.GetNamedItem("range").Value.Length > 0)
                                            {
                                                host.unitsofTimeIntervelWindow = int.Parse(reader.GetAttribute("range").ToString());
                                            }

                                        }
                                        if (timesparce.Name == "time_interval_window2")
                                        {
                                            XmlAttributeCollection attribCollAttribute = timesparce.Attributes;
                                            reader = new XmlNodeReader(timesparce);
                                            reader.Read();
                                            if (attribCollAttribute.GetNamedItem("value") != null && attribCollAttribute.GetNamedItem("value").Value.Length > 0)
                                            {
                                                host.unitsofTimeIntervelWindow2Value = int.Parse(reader.GetAttribute("value").ToString());
                                                Trace.WriteLine(DateTime.Now + "\t  weeks Value " + host.unitsofTimeIntervelWindow2Value.ToString());
                                            }
                                            if (attribCollAttribute.GetNamedItem("bookmark_count") != null && attribCollAttribute.GetNamedItem("bookmark_count").Value.Length > 0)
                                            {
                                                host.bookMarkCountWindow2 = int.Parse(reader.GetAttribute("bookmark_count").ToString());
                                                Trace.WriteLine(DateTime.Now + "\t bookmark count " + host.bookMarkCountWindow2.ToString());
                                            }
                                        }
                                        if (timesparce.Name == "time_interval_window3")
                                        {
                                            XmlAttributeCollection attribCollAttribute = timesparce.Attributes;
                                            reader = new XmlNodeReader(timesparce);
                                            reader.Read();
                                            if (attribCollAttribute.GetNamedItem("value") != null && attribCollAttribute.GetNamedItem("value").Value.Length > 0)
                                            {
                                                host.unitsofTimeIntervelWindow3Value = int.Parse(reader.GetAttribute("value").ToString());
                                                Trace.WriteLine(DateTime.Now + "\t months Value " + host.unitsofTimeIntervelWindow3Value.ToString());
                                            }
                                            if (attribCollAttribute.GetNamedItem("bookmark_count") != null && attribCollAttribute.GetNamedItem("bookmark_count").Value.Length > 0)
                                            {
                                                host.bookMarkCountWindow3 = int.Parse(reader.GetAttribute("bookmark_count").ToString());
                                                Trace.WriteLine(DateTime.Now + "\t bookmark count " + host.bookMarkCountWindow3.ToString());
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    
                }
                //Get host type. It can be either ESX, P2V, Xen, Hyperv , Amazon etc
                switch (node.ParentNode.Name)
                {
                    case "ESX":
                        host.hostType = Hosttype.Vmwareguest;
                        break;
                    case "P2V":
                        host.hostType = Hosttype.Physical;
                        break;
                    default:
                        host.hostType = Hosttype.Vmwareguest;
                        break;
                }


                /* if (node.HasChildNodes)
                 {


                     XmlNodeList diskList = node.ChildNodes;
                     host.vmdkHash = new Hashtable();

                     foreach (XmlNode diskNode in diskList)
                     {
                         reader = new XmlNodeReader(diskNode);
                         reader.Read();

                         if (diskNode.Name == "disk")
                         {


                             reader = new XmlNodeReader(diskNode);
                             reader.Read();

                             host.vmdkHash[reader.GetAttribute("disk_name")] = reader.GetAttribute("size");


                         }
                     }
                 }*/
                if (node.HasChildNodes)
                {
                    XmlNodeList diskList = node.ChildNodes;
                    XmlNodeList nicList = node.ChildNodes;
                    host.vmdkHash = new Hashtable();
                    foreach (XmlNode diskNode in diskList)
                    {
                        reader = new XmlNodeReader(diskNode);
                        XmlAttributeCollection attribCollAttribute = diskNode.Attributes;
                        reader.Read();
                        if (diskNode.Name == "disk")
                        {
                            XmlAttributeCollection attribCollforDisk = diskNode.Attributes;
                            reader = new XmlNodeReader(diskNode);
                            reader.Read();
                            //Nandeesh
                            Hashtable diskHash = new Hashtable();
                            host.diskHash = new Hashtable();
                            //diskHash.Add("Name", reader.GetAttribute("disk_name"));
                            // diskHash.Add("Size", reader.GetAttribute("size"));
                            // diskHash.Add("Selected", "Yes");
                            host.diskHash.Add("Name", reader.GetAttribute("disk_name"));
                            host.diskHash.Add("Size", reader.GetAttribute("size"));
                            host.diskHash.Add("disk_type", reader.GetAttribute("disk_type"));
                            host.diskHash.Add("Protected", reader.GetAttribute("protected"));
                            host.diskHash.Add("datastore_selected", reader.GetAttribute("datastore_selected"));
                            host.diskHash.Add("Selected", "Yes");
                            host.diskHash.Add("scsi_mapping_host", reader.GetAttribute("scsi_mapping_host"));
                            host.diskHash.Add("scsi_mapping_vmx", reader.GetAttribute("scsi_mapping_vmx"));
                            host.targetDataStore = reader.GetAttribute("datastore_selected");
                            host.sourceDataStoreForDrdrill = reader.GetAttribute("datastore_selected");
                            host.diskHash.Add("disk_mode", reader.GetAttribute("disk_mode"));
                            if (attribCollforDisk.GetNamedItem("disk_mode") != null)
                            {
                                if (reader.GetAttribute("disk_mode").ToString() == "Mapped Raw LUN")
                                {
                                    host.diskHash.Add("Rdm", "yes");
                                }
                                else
                                {
                                    host.diskHash.Add("Rdm", "no");
                                }
                            }
                            else
                            {
                                host.diskHash.Add("Rdm", "no");

                            }
                            if (attribCollforDisk.GetNamedItem("sectors") != null)
                            {
                                host.diskHash.Add("SectorsPerTrack", reader.GetAttribute("sectors").ToString());
                            }
                            if (attribCollforDisk.GetNamedItem("heads") != null)
                            {
                                host.diskHash.Add("TotalHeads", reader.GetAttribute("heads").ToString());
                            }
                            if (attribCollforDisk.GetNamedItem("src_devicename") != null && attribCollforDisk.GetNamedItem("src_devicename").Value.Length != 0)
                            {
                                host.diskHash.Add("src_devicename", reader.GetAttribute("src_devicename").ToString());
                            }
                            if (attribCollforDisk.GetNamedItem("volumelist") != null && attribCollforDisk.GetNamedItem("volumelist").Value.Length != 0)
                            {
                                host.diskHash.Add("volumelist", reader.GetAttribute("volumelist").ToString());
                            }
                            if (attribCollforDisk.GetNamedItem("volume_count") != null && attribCollforDisk.GetNamedItem("volume_count").Value.Length != 0)
                            {
                                host.diskHash.Add("volume_count", reader.GetAttribute("volume_count").ToString());
                            }
                            if (attribCollforDisk.GetNamedItem("disk_scsi_id") != null && attribCollforDisk.GetNamedItem("disk_scsi_id").Value.Length != 0)
                            {
                                host.diskHash.Add("DiskScsiId", reader.GetAttribute("disk_scsi_id").ToString());
                            }
                            if (attribCollAttribute.GetNamedItem("disk_signature") != null && attribCollAttribute.GetNamedItem("disk_signature").Value.Length != 0)
                            {
                                host.diskHash.Add("disk_signature", reader.GetAttribute("disk_signature").ToString());
                            }
                            if (attribCollAttribute.GetNamedItem("disk_type_os") != null && attribCollAttribute.GetNamedItem("disk_type_os").Value.Length != 0)
                            {
                                host.diskHash.Add("disk_type_os", reader.GetAttribute("disk_type_os").ToString());
                            }
                            if (attribCollAttribute.GetNamedItem("disk_layout") != null && attribCollAttribute.GetNamedItem("disk_layout").Value.Length != 0)
                            {
                                host.diskHash.Add("DiskLayout", reader.GetAttribute("disk_layout").ToString());
                            }
                            if (attribCollAttribute.GetNamedItem("media_type") != null && attribCollAttribute.GetNamedItem("media_type").Value.Length != 0)
                            {
                                host.diskHash.Add("media_type", reader.GetAttribute("media_type").ToString());
                            }
                            if (reader.GetAttribute("controller_type") != null)
                            {
                                host.diskHash.Add("controller_type", reader.GetAttribute("controller_type"));
                            }
                            if (reader.GetAttribute("scsi_id_onmastertarget") != null)
                            {
                                host.diskHash.Add("scsi_id_onmastertarget", reader.GetAttribute("scsi_id_onmastertarget"));
                            }
                            if (reader.GetAttribute("source_disk_uuid") != null)
                            {
                                host.diskHash.Add("source_disk_uuid", reader.GetAttribute("source_disk_uuid"));
                            }
                            if (reader.GetAttribute("target_disk_uuid") != null)
                            {
                                host.diskHash.Add("target_disk_uuid", reader.GetAttribute("target_disk_uuid"));
                            }
                            if (reader.GetAttribute("selected") != null)
                            {
                                //Trace.WriteLine(DateTime.Now +"\t Came to read selected value"  );
                                host.diskHash.Add("selected_for_protection", reader.GetAttribute("selected"));

                            }
                            //  MessageBox.Show("Came here" +reader.GetAttribute("ide_or_scsi").ToString() );
                            host.diskHash.Add("ide_or_scsi", reader.GetAttribute("ide_or_scsi"));
                            if (attribCollAttribute.GetNamedItem("independent_persistent") != null && attribCollAttribute.GetNamedItem("independent_persistent").Value.Length > 3)
                            {

                                host.diskHash.Add("independent_persistent", reader.GetAttribute("independent_persistent"));
                            }
                            if (attribCollAttribute.GetNamedItem("controller_mode") != null && attribCollAttribute.GetNamedItem("controller_mode").Value.Length != 0)
                            {
                                host.diskHash.Add("controller_mode", reader.GetAttribute("controller_mode"));

                            }
                            if (attribCollAttribute.GetNamedItem("cluster_disk") != null && attribCollAttribute.GetNamedItem("cluster_disk").Value.Length != 0)
                            {
                                host.diskHash.Add("cluster_disk", reader.GetAttribute("cluster_disk"));
                            }
                            //host.totalSizeOfDisks = host.totalSizeOfDisks + int.Parse(reader.GetAttribute("size").ToString());
                            // host.disks.list.Add(diskHash);
                            host.disks.list.Add(host.diskHash);
                            // Debug.WriteLine("++++++++++++++++++++++++++++++++++++++++++++ printing disks");
                            // host.disks.Print();
                            //host.vmdkHash[reader.GetAttribute("disk_name")] = reader.GetAttribute("size");
                        }
                    }

                    foreach (XmlNode nic in nicList)
                    {
                        reader = new XmlNodeReader(nic);
                        XmlAttributeCollection attribCollAttribute = nic.Attributes;

                        reader.Read();

                        if (nic.Name == "nic")
                        {
                            Hashtable nicHash = new Hashtable();
                            host.nicHash = new Hashtable();
                            if (attribCollAttribute.GetNamedItem("network_label") != null && attribCollAttribute.GetNamedItem("network_label").Value.Length > 0)
                            {
                                host.nicHash.Add("network_label", reader.GetAttribute("network_label"));
                            }
                            if (attribCollAttribute.GetNamedItem("network_name") != null && attribCollAttribute.GetNamedItem("network_name").Value.Length > 0)
                            {
                                host.nicHash.Add("network_name", reader.GetAttribute("network_name"));
                            }
                            if (attribCollAttribute.GetNamedItem("nic_mac") != null && attribCollAttribute.GetNamedItem("nic_mac").Value.Length > 0)
                            {
                                host.nicHash.Add("nic_mac", reader.GetAttribute("nic_mac"));
                            }
                            if (attribCollAttribute.GetNamedItem("ip") != null && attribCollAttribute.GetNamedItem("ip").Value.Length > 0)
                            {
                                host.nicHash.Add("ip", reader.GetAttribute("ip"));
                            }
                            if (attribCollAttribute.GetNamedItem("dhcp") != null && attribCollAttribute.GetNamedItem("dhcp").Value.Length > 0)
                            {
                                host.nicHash.Add("dhcp", int.Parse(reader.GetAttribute("dhcp").ToString()));
                            }
                            if (attribCollAttribute.GetNamedItem("mask") != null && attribCollAttribute.GetNamedItem("mask").Value.Length > 0)
                            {
                                host.nicHash.Add("mask", reader.GetAttribute("mask"));
                            }
                            if (attribCollAttribute.GetNamedItem("gateway") != null && attribCollAttribute.GetNamedItem("gateway").Value.Length > 0)
                            {
                                host.nicHash.Add("gateway", reader.GetAttribute("gateway"));
                            }
                            if (attribCollAttribute.GetNamedItem("dnsip") != null && attribCollAttribute.GetNamedItem("dnsip").Value.Length > 0)
                            {
                                host.nicHash.Add("dnsip", reader.GetAttribute("dnsip"));
                            }
                            if (attribCollAttribute.GetNamedItem("new_ip") != null && attribCollAttribute.GetNamedItem("new_ip").Value.Length > 0)
                            {
                                host.nicHash.Add("new_ip", reader.GetAttribute("new_ip"));
                            }
                            else
                            {
                                host.nicHash.Add("new_ip", null);
                            }
                            if (attribCollAttribute.GetNamedItem("new_mask") != null && attribCollAttribute.GetNamedItem("new_mask").Value.Length > 0)
                            {
                                host.nicHash.Add("new_mask", reader.GetAttribute("new_mask"));
                            }
                            else
                            {
                                host.nicHash.Add("new_mask", null);
                            }
                            if (attribCollAttribute.GetNamedItem("new_dnsip") != null && attribCollAttribute.GetNamedItem("new_dnsip").Value.Length > 0)
                            {
                                host.nicHash.Add("new_dnsip", reader.GetAttribute("new_dnsip"));
                            }
                            else
                            {
                                host.nicHash.Add("new_dnsip", null);
                            }
                            if (attribCollAttribute.GetNamedItem("new_gateway") != null && attribCollAttribute.GetNamedItem("new_gateway").Value.Length > 0)
                            {
                                host.nicHash.Add("new_gateway", reader.GetAttribute("new_gateway"));
                            }
                            else
                            {
                                host.nicHash.Add("new_gateway", null);
                            }
                            if (attribCollAttribute.GetNamedItem("new_network_name") != null && attribCollAttribute.GetNamedItem("new_network_name").Value.Length > 0)
                            {
                                host.nicHash.Add("new_network_name", reader.GetAttribute("new_network_name"));
                            }
                            else
                            {
                                host.nicHash.Add("new_network_name", null);
                            }
                            if (attribCollAttribute.GetNamedItem("new_macaddress") != null && attribCollAttribute.GetNamedItem("new_macaddress").Value.Length > 0)
                            {
                                host.nicHash.Add("new_macaddress", reader.GetAttribute("new_macaddress"));
                            }
                            else
                            {
                                host.nicHash.Add("new_macaddress", null);
                            }
                            if (attribCollAttribute.GetNamedItem("address_type") != null && attribCollAttribute.GetNamedItem("address_type").Value.Length > 0)
                            {
                                host.nicHash.Add("address_type", reader.GetAttribute("address_type"));
                            }
                            else
                            {
                                host.nicHash.Add("address_type", null);
                            }
                            if (attribCollAttribute.GetNamedItem("adapter_type") != null && attribCollAttribute.GetNamedItem("adapter_type").Value.Length > 0)
                            {
                                host.nicHash.Add("adapter_type", reader.GetAttribute("adapter_type"));
                            }
                            else
                            {
                                host.nicHash.Add("adapter_type", null);
                            }
                            if (attribCollAttribute.GetNamedItem("virtual_ips") != null && attribCollAttribute.GetNamedItem("virtual_ips").Value.Length > 0)
                            {
                                host.nicHash.Add("virtual_ips", reader.GetAttribute("virtual_ips"));
                            }
                            else
                            {
                                host.nicHash.Add("virtual_ips", null);
                            }
                            host.nicHash.Add("Selected", "yes");
                            host.nicList.list.Add(host.nicHash);
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                System.FormatException formatException = new FormatException("Inner exception", ex);
                Trace.WriteLine(formatException.GetBaseException());
                Trace.WriteLine(formatException.Data);
                Trace.WriteLine(formatException.InnerException);
                Trace.WriteLine(formatException.Message);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: ReadHostNode " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
            // host.Print();
            return host;
        }

        private string ProcessServerIP(XmlNode node)
        {
            // this is not used.
            string ip = null;
            XmlNodeReader reader = null;

            XmlAttributeCollection attribColl = node.Attributes;
            try
            {
                //Debug.WriteLine("enter to read the information to read ");
                reader = new XmlNodeReader(node);
                reader.Read();
                if (attribColl.GetNamedItem("datastore_name").Value.Length > 0)
                {
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                System.FormatException formatException = new FormatException("Inner exception", ex);
                Trace.WriteLine(formatException.GetBaseException());
                Trace.WriteLine(formatException.Data);
                Trace.WriteLine(formatException.InnerException);
                Trace.WriteLine(formatException.Message);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
            return ip;
        }

        private DataStore ReadDataStoreNode(XmlNode node)
        {
            // this will read all the datastore nodes in the xml file under the target_esx node.
            XmlNodeReader reader = null;
            DataStore dataStore = new DataStore();
            XmlAttributeCollection attribColl = node.Attributes;
            try
            {
                //Debug.WriteLine("enter to read the information to read ");
                reader = new XmlNodeReader(node);
                reader.Read();
                if (attribColl.GetNamedItem("datastore_name").Value.Length > 0)
                {
                    dataStore.name = reader.GetAttribute("datastore_name");

                }
                if (attribColl.GetNamedItem("vSpherehostname") != null && attribColl.GetNamedItem("vSpherehostname").Value.Length > 0)
                {
                    dataStore.vSpherehostname = reader.GetAttribute("vSpherehostname");
                }
                if (attribColl.GetNamedItem("total_size").Value.Length > 0)
                {
                    dataStore.totalSize = float.Parse(reader.GetAttribute("total_size"));
                }
                if (attribColl.GetNamedItem("free_space").Value.Length > 0)
                {
                    dataStore.freeSpace = float.Parse(reader.GetAttribute("free_space"));
                }
                if (attribColl.GetNamedItem("datastore_blocksize_mb").Value.Length > 0)
                {
                    dataStore.blockSize = int.Parse(reader.GetAttribute("datastore_blocksize_mb"));
                }
                else
                {
                    dataStore.blockSize = 0;
                }
                if (attribColl.GetNamedItem("filesystem_version") != null && attribColl.GetNamedItem("filesystem_version").Value.Length > 0)
                {
                    dataStore.filesystemversion = double.Parse(reader.GetAttribute("filesystem_version"));
                    //Trace.WriteLine(DateTime.Now + " \t Printitng the file system version " + dataStore.filesystemversion.ToString());
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                System.FormatException formatException = new FormatException("Inner exception", ex);
                Trace.WriteLine(formatException.GetBaseException());
                Trace.WriteLine(formatException.Data);
                Trace.WriteLine(formatException.InnerException);
                Trace.WriteLine(formatException.Message);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
            return dataStore;
        }
        private Configurations ReadConfigurationNodes(XmlNode node)
        {
            // this will read the config info in the xml file under the target_esx node....
           // Trace.WriteLine(DateTime.Now + " \t Entered: ReadConfigurationNodes() of SolutionConfig.cs.cs ");

            XmlNodeReader reader = null;
            Configurations configuration = new Configurations();
            XmlAttributeCollection attribColl = node.Attributes;
            try
            {

                reader = new XmlNodeReader(node);
                reader.Read();
                //Trace.WriteLine(DateTime.Now + " \t Came to read attributes ");
                if (attribColl.GetNamedItem("memsize") != null && attribColl.GetNamedItem("memsize").Value.Length > 0)
                {
                    //Trace.WriteLine(DateTime.Now + " \t printing the memsize  " + reader.GetAttribute("memsize").ToString());
                    configuration.memsize = float.Parse(reader.GetAttribute("memsize").ToString());
                }
                if (attribColl.GetNamedItem("cpucount") != null && attribColl.GetNamedItem("cpucount").Value.Length > 0)
                {
                    //Trace.WriteLine(DateTime.Now + " \t printing the cpucount  " + reader.GetAttribute("cpucount").ToString());
                    configuration.cpucount = int.Parse(reader.GetAttribute("cpucount").ToString());
                }
                if (attribColl.GetNamedItem("vSpherehostname") != null && attribColl.GetNamedItem("vSpherehostname").Value.Length > 0)
                {
                    configuration.vSpherehostname = reader.GetAttribute("vSpherehostname");
                }

            }
            catch (Exception e)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);
                System.FormatException formatException = new FormatException("Inner exception", e);
                Trace.WriteLine(formatException.GetBaseException());
                Trace.WriteLine(formatException.Data);
                Trace.WriteLine(formatException.InnerException);
                Trace.WriteLine(formatException.Message);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: ReadConfigurationNodes " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + e.Message);
                Trace.WriteLine("ERROR___________________________________________");
                Trace.WriteLine(DateTime.Now + "ReadConfigurationNodes Esx.cs:  Caught an exception " + e.Message);

            }

           // Trace.WriteLine(DateTime.Now + " \t Exiting: ReadConfigurationNodes() of SolutionConfig.cs ");
            return configuration;

        }
        private RdmLuns ReadLunNodes(XmlNode node)
        {
            // This will read the lun nodes in the esx_master_targetesxip.xml ....
            //Trace.WriteLine(DateTime.Now + " \t Entered ReadLunNodes of solutionconfig.cs");
                XmlNodeReader reader = null;
                XmlAttributeCollection attribColl = node.Attributes;
                RdmLuns lun = new RdmLuns();
                reader = new XmlNodeReader(node);
                try
                {
                    reader.Read();
                    lun.name = reader.GetAttribute("name");
                    lun.adapter = reader.GetAttribute("adapter");
                    lun.lun = reader.GetAttribute("lun");
                    if (attribColl.GetNamedItem("vSpherehostname") != null && attribColl.GetNamedItem("vSpherehostname").Value.Length > 0)
                    {
                        lun.vSpherehostname = reader.GetAttribute("vSpherehostname");
                    }
                    lun.devicename = reader.GetAttribute("devicename");
                    try
                    {
                        lun.capacity_in_kb = double.Parse(reader.GetAttribute("capacity_in_kb"));
                    }
                    catch (Exception e)
                    {
                        System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);
                        System.FormatException formatException = new FormatException("Inner exception", e);
                        Trace.WriteLine(formatException.GetBaseException());
                        Trace.WriteLine(formatException.Data);
                        Trace.WriteLine(formatException.InnerException);
                        Trace.WriteLine(formatException.Message);
                        Trace.WriteLine("ERROR*******************************************");
                        Trace.WriteLine(DateTime.Now + "Caught exception at Method:ReadLunNodes " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                        Trace.WriteLine("Exception  =  " + e.Message);
                        Trace.WriteLine("ERROR___________________________________________");
                        Trace.WriteLine(e.ToString());
                    }

                }
                catch (Exception ex)
                {

                    System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                    System.FormatException formatException = new FormatException("Inner exception", ex);
                    Trace.WriteLine(formatException.GetBaseException());
                    Trace.WriteLine(formatException.Data);
                    Trace.WriteLine(formatException.InnerException);
                    Trace.WriteLine(formatException.Message);
                    Trace.WriteLine("ERROR*******************************************");
                    Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                    Trace.WriteLine("Exception  =  " + ex.Message);
                    Trace.WriteLine("ERROR___________________________________________");


                }
            //MessageBox.Show("Printing f"+ lun.capacity_in_kb);

            return lun;

        }
        private Network ReadNetWorkNode(XmlNode node)
        {

            //Trace.WriteLine(DateTime.Now + " \t Entered: ReadNetWorkNode() of ESX.cs ");

            XmlNodeReader reader = null;
            Network network = new Network();
            XmlAttributeCollection attribColl = node.Attributes;
            try
            {

                reader = new XmlNodeReader(node);
                reader.Read();
                if (attribColl.GetNamedItem("name") != null && attribColl.GetNamedItem("name").Value.Length > 0)
                {
                    network.Networkname = reader.GetAttribute("name");
                }
                if (attribColl.GetNamedItem("Type") != null && attribColl.GetNamedItem("Type").Value.Length > 0)
                {
                    network.Networktype = reader.GetAttribute("Type");
                }
                if (attribColl.GetNamedItem("vSpherehostname") != null && attribColl.GetNamedItem("vSpherehostname").Value.Length > 0)
                {
                    network.Vspherehostname = reader.GetAttribute("vSpherehostname");
                }
                if (attribColl.GetNamedItem("switchUuid") != null && attribColl.GetNamedItem("switchUuid").Value.Length > 0)
                {
                    network.Switchuuid = reader.GetAttribute("switchUuid");
                }
                if (attribColl.GetNamedItem("portgroupKey") != null && attribColl.GetNamedItem("portgroupKey").Value.Length > 0)
                {
                    network.PortgroupKey = reader.GetAttribute("portgroupKey");
                }
            }
            catch (Exception e)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);
                System.FormatException formatException = new FormatException("Inner exception", e);
                Trace.WriteLine(formatException.GetBaseException());
                Trace.WriteLine(formatException.Data);
                Trace.WriteLine(formatException.InnerException);
                Trace.WriteLine(formatException.Message);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + e.Message);
                Trace.WriteLine("ERROR___________________________________________");
                Trace.WriteLine(DateTime.Now + "Esx.cs:  Caught an exception " + e.Message);
            }

            //Trace.WriteLine(DateTime.Now + " \t Exiting: ReadNetWorkNode() of ESX.cs ");
            return network;

        }
        private ResourcePool ReadResourcePoolNode(XmlNode node)
        {

            //Trace.WriteLine(DateTime.Now + " \t Entered: ReadNetWorkNode() of ESX.cs ");

            XmlNodeReader reader = null;
            ResourcePool resourcePool = new ResourcePool();
            XmlAttributeCollection attribColl = node.Attributes;
            try
            {

                reader = new XmlNodeReader(node);
                reader.Read();
                if (attribColl.GetNamedItem("name") != null && attribColl.GetNamedItem("name").Value.Length > 0)
                {
                    resourcePool.name = reader.GetAttribute("name");
                }
                if (attribColl.GetNamedItem("type") != null && attribColl.GetNamedItem("type").Value.Length > 0)
                {
                    resourcePool.type = reader.GetAttribute("type");
                }
                if (attribColl.GetNamedItem("host") != null && attribColl.GetNamedItem("host").Value.Length > 0)
                {
                    resourcePool.host = reader.GetAttribute("host");
                }
                if (attribColl.GetNamedItem("groupId") != null && attribColl.GetNamedItem("groupId").Value.Length > 0)
                {
                    resourcePool.groupId = reader.GetAttribute("groupId");
                }
                if (attribColl.GetNamedItem("owner") != null && attribColl.GetNamedItem("owner").Value.Length > 0)
                {
                    resourcePool.owner = reader.GetAttribute("owner");
                }
                if (attribColl.GetNamedItem("owner_type") != null && attribColl.GetNamedItem("owner_type").Value.Length > 0)
                {
                    resourcePool.ownertype = reader.GetAttribute("owner_type");
                }

            }
            catch (Exception e)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);
                System.FormatException formatException = new FormatException("Inner exception", e);
                Trace.WriteLine(formatException.GetBaseException());
                Trace.WriteLine(formatException.Data);
                Trace.WriteLine(formatException.InnerException);
                Trace.WriteLine(formatException.Message);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + e.Message);
                Trace.WriteLine("ERROR___________________________________________");
                Trace.WriteLine(DateTime.Now + "Esx.cs:  Caught an exception " + e.Message);
            }

            //Trace.WriteLine(DateTime.Now + " \t Exiting: ReadNetWorkNode() of ESX.cs ");
            return resourcePool;

        }

        internal bool PreparePhysicalOSInfoFile(HostList inSourceHostList, HostList inTargetList)
        {
            try
            {
                // Now we are not preparing the file.
                // Every thing will be done in the esx.xml only.
                // it is in 5.5 only.
                if (!Directory.Exists(Windowsdir))
                {
                    Debug.WriteLine("Creating Dir " + Windowsdir);
                    Directory.CreateDirectory(Windowsdir);

                }

                //Removing the file if already exist///



                //Cleanup earlier files.
                if (File.Exists(Windowsdir + "\\" + Physicalmachinelistfile))
                {
                    File.Delete(Windowsdir + "\\" + Physicalmachinelistfile);
                }
                if (File.Exists(Physicalmachinelistfile))
                {
                    File.Delete(Physicalmachinelistfile);
                }
                if (File.Exists(Windowsdir + "\\" + Physicalmachinelistfiletemp))
                {
                    File.Delete(Windowsdir + "\\" + Physicalmachinelistfiletemp);
                }

                //Get the file from esx server and keep it in Windows folder//
                Esx esxInfo = new Esx();

                //Download the physical machine list from the target ESX machine
                foreach (Host tempH in inTargetList._hostList)
                {
                    esxInfo.DownloadFile(tempH.esxIp,  Physicalmachinelistfile);
                }

                //Move the downloaded file to the windows folder from vContinuum foloder
                if (File.Exists(Physicalmachinelistfile))
                {

                    File.Move(Physicalmachinelistfile, Windowsdir + "\\" + Physicalmachinelistfile);
                }



                FileInfo f1 = new FileInfo(Installpath + "\\" + Windowsdir + "\\" + Physicalmachinelistfile);
                
                //A temp file is used. It will created if it doesn't exist
                FileInfo f2 = new FileInfo(Installpath + "\\" + Windowsdir + "\\" + Physicalmachinelistfiletemp);


                if (File.Exists(Windowsdir + "\\" + Physicalmachinelistfile))
                {

                    string oldHostEntry;
                    int index = 0;

                    StreamReader sr = new StreamReader(Installpath + "\\" + Windowsdir + "\\" + Physicalmachinelistfile);
                  
                   // FileStream fileStream = new FileStream(_installPath + "\\" + WINDOWS_DIR + "\\" + PHYSICAL_MACHINE_LIST_FILE, FileMode.OpenOrCreate, FileAccess.ReadWrite);
                    while ((oldHostEntry = sr.ReadLine()) != null)
                    {
                        bool hostMatched = false;

                        Trace.WriteLine(DateTime.Now + " \t Came here to read the line " + oldHostEntry);
                       
                        string  existingHostName = oldHostEntry.Split('=')[0];

                        hostMatched = false;

                        foreach (Host newHost in inSourceHostList._hostList)
                        {
                            if (newHost.hostname == existingHostName)
                            {
                                hostMatched = true;

                            }
                        }

                        // Take only old entries that are not matching to the new/current host that we are trying to protect.
                        // In case it matched with host that is under protection, then we will consider as duplicate entry.
                        // WE will use only the OS mentioned in the current/new host
                        if( hostMatched == false)
                        {

                                Trace.WriteLine(DateTime.Now + " \t Going to write a line in tempfile " + oldHostEntry);
                                StreamWriter writeToTempFile = f2.AppendText();
                                writeToTempFile.WriteLine(oldHostEntry);
                                writeToTempFile.Close();
                               
                        }
                            
                        
                        index++;
                    }   
                    
                
                    sr.Close();
                    if (File.Exists(Installpath + "\\" + Windowsdir + "\\" + Physicalmachinelistfiletemp))
                    {
                        File.Delete(Installpath + "\\" + Windowsdir + "\\" + Physicalmachinelistfile);
                        File.Move(Installpath + "\\" + Windowsdir + "\\" + Physicalmachinelistfiletemp, Installpath + "\\" + Windowsdir + "\\" + Physicalmachinelistfile);

                    }
                }
              
                StreamWriter sw = f1.AppendText();
                
                foreach (Host h in inSourceHostList._hostList)
                {
                    sw.WriteLine(h.displayname + "=" + h.operatingSystem);
                    
                }
                sw.Close();
              
               
                //Copy one file to current folder so that it will be uploaded
                f1.CopyTo(Installpath + "\\" + Physicalmachinelistfile);
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                System.FormatException formatException = new FormatException("Inner exception", ex);
                Trace.WriteLine(formatException.GetBaseException());
                Trace.WriteLine(formatException.Data);
                Trace.WriteLine(formatException.InnerException);
                Trace.WriteLine(formatException.Message);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;

            }
            return true;
        }



        internal bool PrePareProcessServerFile(HostList inSourceHostList, string incxIP)
        {
            try
            {
                // We are not preparing the processerserver.txt also 
                // it is in 5.5 only.
                if (!Directory.Exists(Windowsdir))
                {
                    Debug.WriteLine("Creating Dir " + Windowsdir);
                    Directory.CreateDirectory(Windowsdir);

                }

                FileInfo f1 = new FileInfo(Windowsdir + "\\" + Processserverfile);
                StreamWriter sw = f1.CreateText();
                foreach (Host h in inSourceHostList._hostList)
                {
                    sw.WriteLine(h.hostname + "=" + incxIP);
                }
                sw.Close();
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                System.FormatException formatException = new FormatException("Inner exception", ex);
                Trace.WriteLine(formatException.GetBaseException());
                Trace.WriteLine(formatException.Data);
                Trace.WriteLine(formatException.InnerException);
                Trace.WriteLine(formatException.Message);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;

            }
            return true;
        }

        public bool WriteSourceXml(HostList inSourceList, string inFileName)
        {
            try
            {
                XmlWriterSettings settings = new XmlWriterSettings();
                settings.Indent = true;
                settings.ConformanceLevel = ConformanceLevel.Fragment;
                using (XmlWriter writer = XmlWriter.Create(Latestfilepath + "\\"+ inFileName, settings))
                {

                    writer.WriteStartElement("SRC_ESX");
                    inSourceList.WriteHostsToXml(writer);
                    writer.WriteEndElement();

                    writer.Close();
                }
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                System.FormatException formatException = new FormatException("Inner exception", ex);
                Trace.WriteLine(formatException.GetBaseException());
                Trace.WriteLine(formatException.Data);
                Trace.WriteLine(formatException.InnerException);
                Trace.WriteLine(formatException.Message);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;

            }
            return true;
        }


        internal bool AddOrReplaceMasterXmlFileOfOfflineSyncXml(Host inMasterHost)
        {
            try
            {
                Trace.WriteLine(DateTime.Now + " \t  Entered: AddOrReplaceMasterXmlFile() in SolutionConfig.cs");

                Esx esx = new Esx();
                //esx.DownloadEsxMasterXml

                //Cleanup earlier files.

                if (File.Exists(Latestfilepath+"\\ESX_Master_Offlinesync.xml"))
                {
                    File.Delete(Latestfilepath   +"\\ESX_Master_Offlinesync.xml");
                }


                int returnValue = 1;

               
               
                    returnValue = esx.DownLoadOfflineSyncXmlFile(inMasterHost.esxIp, "ESX_Master_Offlinesync.xml");
                    if (returnValue == 2)
                    {
                        if (File.Exists(Directory.GetCurrentDirectory() + "\\ESX_Master.lck"))
                        {

                            StreamReader sr1 = new StreamReader(Directory.GetCurrentDirectory() + "\\ESX_Master.lck");
                            string firstLine1;
                            firstLine1 = sr1.ReadToEnd();
                            firstLine1.Split(' ');
                            WinPreReqs win = new WinPreReqs("", "", "", "");
                            Host h = new Host();
                            h.hostname = firstLine1.Split('=')[0];
                            string cxIP = firstLine1.Split('=')[1];
                            win.MasterTargetPreCheck( h, cxIP);
                            h = WinPreReqs.GetHost;
                            if (h.machineInUse == true)
                            {
                                Thread.Sleep(300000);
                                win.MasterTargetPreCheck( h, cxIP);
                                h = WinPreReqs.GetHost;
                                if (h.machineInUse == true)
                                {
                                    MessageBox.Show("Some fx jobs are running by this master target (" + h.hostname + ")" + " in CX:" + cxIP);
                                    return false;
                                }
                                else
                                {
                                    returnValue = esx.DownloadFile(inMasterHost.esxIp, "ESX_Master_Offlinesync.xml");

                                }
                            }
                            else
                            {
                                returnValue = esx.DownloadFile(inMasterHost.esxIp,  "ESX_Master_Offlinesync.xml");

                            }

                        }
                        else
                        {
                            return false;
                        }
                    }
                    if (returnValue != 0)
                    {
                        Trace.WriteLine(DateTime.Now + " \t Downloading the master xml  failed at the time of offlinesync export ESX_Master_OfflineSync.xml");
                        return false;
                    }
              
                //it should be called only if it is addition of disks
                    if (!File.Exists(Latestfilepath + "\\ESX_Master_Offlinesync.xml"))
                    {  //Create New file .xml//
                        XmlWriterSettings settings = new XmlWriterSettings();
                        settings.Indent = true;
                        settings.ConformanceLevel = ConformanceLevel.Fragment;
                        using (XmlWriter writer = XmlWriter.Create(Latestfilepath + "\\ESX_Master_Offlinesync.xml", settings))
                        {
                            writer.WriteStartElement("start");
                            writer.WriteEndElement();

                            writer.Close();
                            AppendXml(Latestfilepath + "\\ESX_Master_Offlinesync.xml");
                        }
                    }
                    else
                    {
                        RemoveEntryIfAlreadyExistsinOfflineSyncXml(Latestfilepath + "\\ESX_Master_Offlinesync.xml");
                        AppendXmlOfflineSyncXml(Latestfilepath + "\\ESX_Master_Offlinesync.xml");
                        //Append to existing file
                       
                    }
                
                
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
            Trace.WriteLine(DateTime.Now + " \t  Exiting: AddOrReplaceMasterXmlFile() in SolutionConfig.cs");
            return true;

        }
        internal bool RemoveEntryIfAlreadyExistsinOfflineSyncXml(string inEsxMasterFile)
        {
            try
            {
                Trace.WriteLine(DateTime.Now + " \t  Entered: RemoveEntryIfAlreadyExists() in SolutionConfig.cs");
                //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
                //based on above comment we have added xmlresolver as null
                XmlDocument documentEsx = new XmlDocument();
                XmlDocument documentMasterEsx = new XmlDocument();
                documentEsx.XmlResolver = null;
                documentMasterEsx.XmlResolver = null;
                string esxXmlFile = Latestfilepath   + "\\ESX.xml";
                string esxMasterXmlFile = inEsxMasterFile;
                //StreamReader reader = new StreamReader(esxXmlFile);
                //string xmlString = reader.ReadToEnd();


                //StreamReader reader1 = new StreamReader(esxMasterXmlFile);
                //string xmlMasterString = reader1.ReadToEnd();
                //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
                XmlReaderSettings settings = new XmlReaderSettings();
                settings.ProhibitDtd = true;
                settings.XmlResolver = null;

                using (XmlReader reader2 = XmlReader.Create(esxXmlFile, settings))
                {
                    documentEsx.Load(reader2);
                   // reader2.Close();
                }
                using (XmlReader reader3 = XmlReader.Create(esxMasterXmlFile, settings))
                {
                    documentMasterEsx.Load(reader3);
                    //documentEsx.Load(esxXmlFile);
                    //documentMasterEsx.Load(esxMasterXmlFile);
                    //reader1.Close();
                    //reader.Close();

                   // reader3.Close();
                }
                XmlNodeList hostNodesEsxXml = null, hostNodesEsxMasterXml = null;
                hostNodesEsxXml = documentEsx.GetElementsByTagName("host");
                hostNodesEsxMasterXml = documentMasterEsx.GetElementsByTagName("host");
                foreach (XmlNode esxnode in hostNodesEsxXml)
                {
                    foreach (XmlNode esxMasternode in hostNodesEsxMasterXml)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Comparing the display names " + esxnode.Attributes["display_name"].Value.ToString() + " " + esxMasternode.Attributes["display_name"].Value.ToString());
                        if (esxnode.Attributes["display_name"].Value == esxMasternode.Attributes["display_name"].Value)
                        {
                            Trace.WriteLine(DateTime.Now + "\t Came here because displaynames are matched");
                            if (esxMasternode.ParentNode.Name == "SRC_ESX")
                            {
                                Trace.WriteLine(DateTime.Now + " Removing the existing node");
                                XmlNode parentNode = esxMasternode.ParentNode;
                                esxMasternode.ParentNode.RemoveChild(esxMasternode);
                                if (!parentNode.HasChildNodes)
                                {
                                    Trace.WriteLine(DateTime.Now + "\t Came here to remove entire root nodes");
                                    parentNode.ParentNode.ParentNode.RemoveChild(parentNode.ParentNode);
                                }
                            }
                            break;
                        }
                    }
                }
                documentEsx.Save(esxXmlFile);
                documentMasterEsx.Save(esxMasterXmlFile);
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
            Trace.WriteLine(DateTime.Now + " \t  Exiting: RemoveEntryIfAlreadyExists() in SolutionConfig.cs");
            return true;
        }
        internal bool AppendXmlOfflineSyncXml(string inEsxMasterFile)
        {
            try
            {
                Trace.WriteLine(DateTime.Now + " \t  Entered: AppendXml() in SolutionConfig.cs");
                //Reading esx.xml for further use.

               // XmlTextReader myReader = new XmlTextReader(Latestfilepath + "\\ESX.xml");

                // Load the source of the XML file into an XmlDocument
                //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
                //based on above comment we have added xmlresolver as null
                XmlDocument originalfile = new XmlDocument();
                originalfile.XmlResolver = null;
                // Load the source XML file into the first document
                //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
                XmlReaderSettings settings = new XmlReaderSettings();
                settings.ProhibitDtd = true;
                settings.XmlResolver = null;
                using (XmlReader reader1 = XmlReader.Create(Latestfilepath + "\\ESX.xml", settings))
                {
                    originalfile.Load(reader1);
                    //reader1.Close();
                }
                //originalfile.Load(Latestfilepath + "\\ESX.xml");

                // Close the reader

               // myReader.Close();


                Debug.WriteLine("prepared initial master xml file");

                // Open the reader with the destination XML file

               // myReader = new XmlTextReader(inEsxMasterFile);

                // Load the source of the XML file into an XmlDocument

                XmlDocument doctoMerge = new XmlDocument();
                doctoMerge.XmlResolver = null;
                // Load the destination XML file into the first document
                //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
                using (XmlReader reader2 = XmlReader.Create(inEsxMasterFile, settings))
                {
                    originalfile.Load(reader2);
                    //doctoMerge.Load(inEsxMasterFile);
                   // reader2.Close();
                    // Close the reader
                }

                //myReader.Close();

                XmlNode rootOfFileToMerge = doctoMerge["start"];


                // Store the node to be copied into an XmlNode

                XmlNode nodeOrig = originalfile["root"];

                // Store the copy of the original node into an XmlNode

                XmlNode nodeDest = doctoMerge.ImportNode(nodeOrig, true);

                // Append the node being copied to the root of the destination document

                rootOfFileToMerge.AppendChild(nodeDest);


                XmlTextWriter xmlWriter = new XmlTextWriter(inEsxMasterFile, Encoding.UTF8);

                // Indented for easy reading

                xmlWriter.Formatting = Formatting.Indented;

                // Write the file

                doctoMerge.WriteTo(xmlWriter);

                // Close the writer

                xmlWriter.Close();
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
            Trace.WriteLine(DateTime.Now + " \t  Exiting: AppendXml() in SolutionConfig.cs");
            return true;

        }


        public static string GetAltGuestname
        {
            get
            {
                return altGuestName;
            }
            set
            {
                value = altGuestName;
            }
        }

        public static string SetAlgGuestName(string operatingSystem, string osEdition)
        {



            string osArchitecture = "Windows";
            try
            {
                if (operatingSystem.Contains("2003"))
                {
                    osArchitecture = osArchitecture + "_2003";
                    if (operatingSystem.Contains("64"))
                    {
                        osArchitecture = osArchitecture + "_64";
                        if (operatingSystem.Contains("Standard"))
                        {
                            osEdition = "winNetStandard64Guest";
                        }
                        if (operatingSystem.Contains("Enterprise"))
                        {
                            osEdition = "winNetEnterprise64Guest";
                        }
                        if (operatingSystem.Contains("Datacenter"))
                        {
                            osEdition = "winNetDatacenter64Guest";
                        }
                        if (operatingSystem.Contains("Web"))
                        {
                            osEdition = "winNetWeb64Guest";
                        }
                        if (operatingSystem.Contains("Small Business Server"))
                        {

                            osEdition = "winNetBusiness64Guest";
                        }
                    }
                    else
                    {
                        osArchitecture = osArchitecture + "_32";
                        if (operatingSystem.Contains("Standard"))
                        {
                            osEdition = "winNetStandardGuest";
                        }
                        if (operatingSystem.Contains("Enterprise"))
                        {

                            osEdition = "winNetEnterpriseGuest";
                        }
                        if (operatingSystem.Contains("Datacenter"))
                        {
                            osEdition = "winNetDatacenterGuest";
                        }
                        if (operatingSystem.Contains("Web"))
                        {
                            osEdition = "winNetWebGuest";
                        }
                        if (operatingSystem.Contains("Small Business Server"))
                        {

                            osEdition = "winNetBusinessGuest";
                        }
                    }

                }

                else if (operatingSystem.Contains("2008"))
                {
                    osArchitecture = osArchitecture + "_2008";

                    if (operatingSystem.Contains("R2"))
                    {
                        osArchitecture = osArchitecture + "_R2";
                        osEdition = "windows7Server64Guest";

                    }
                    else if (operatingSystem.Contains("64"))
                    {

                        osArchitecture = osArchitecture + "_64";
                        osEdition = "winLonghorn64Guest";
                    }
                    else
                    {

                        osArchitecture = osArchitecture + "_32";
                        osEdition = "winLonghornGuest";

                    }

                }
                else
                {
                    osArchitecture = osArchitecture + "_2000";
                }

                altGuestName = osEdition;

            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");


            }

            altGuestName = osEdition;
            return osArchitecture;
        }

        public bool ReadLinuxXmlFile(Host host)
        {
            Trace.WriteLine(DateTime.Now + " \t Entered: Read linuxInfo.xml to read xml file ");
            // Debug.WriteLine("This method is called");
            //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
            //based on above comment we have added xmlresolver as null
            XmlDocument document = new XmlDocument();
            document.XmlResolver = null;
            string ip = "";


            XmlNodeList hostNodes = null;
            XmlNodeList diskNodes = null;


            string fileFullPath = Latestfilepath + "\\linuxinfo.xml";
          

            

            if (!File.Exists(fileFullPath))
            {
                File.Create(fileFullPath);
                Trace.WriteLine("01_002_001: The file " + fileFullPath + " could not be located" + "in dir " + Installpath);

                // Console.WriteLine("01_002_001: The file " + fileFullPath + " could not be located" + "in dir " + _installPath);
                return false;


            }
            try
            {
                //StreamReader reader1 = new StreamReader(fileFullPath);
                //string xmlString = reader1.ReadToEnd();
                //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
                XmlReaderSettings settings = new XmlReaderSettings();
                settings.ProhibitDtd = true;
                settings.XmlResolver = null;
                using (XmlReader reader1 = XmlReader.Create(fileFullPath, settings))
                {
                    document.Load(reader1);
                    //reader1.Close();
                }
               // document.Load(fileFullPath);
                //reader1.Close();
            }


            catch (XmlException xmle)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(xmle, true);
                System.FormatException formatException = new FormatException("Inner exception", xmle);
                Trace.WriteLine(formatException.GetBaseException());
                Trace.WriteLine(formatException.Data);
                Trace.WriteLine(formatException.InnerException);
                Trace.WriteLine(formatException.Message);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + xmle.Message);
                Trace.WriteLine("ERROR___________________________________________");
                Console.WriteLine(xmle.Message);
                return false;
            }
            hostNodes = document.GetElementsByTagName("Host");
            foreach (XmlNode node in hostNodes)
            {
              

                XmlNodeReader reader = null;
                string numOfDisksString;
                XmlAttributeCollection attribColl = node.Attributes;
                host = new Host();
                reader = new XmlNodeReader(node);
                reader.Read();
                try
                {
                    //Trace.WriteLine("   Entered Read Host Node" + reader.GetAttribute("display_name"));
                    host.hostname = reader.GetAttribute("HostName");
                    host.displayname = host.hostname;
                    string osType = OStype.Linux.ToString();
                    host.os = (OStype)Enum.Parse(typeof(OStype), osType);
                    host.vmx_path = reader.GetAttribute("vmx_path");
                    host.machineType = "PhysicalMachine";
                    host.operatingSystem = reader.GetAttribute("Os");
                    if (attribColl.GetNamedItem("AltGuestName") != null && attribColl.GetNamedItem("AltGuestName").Value != null)
                    {
                        host.alt_guest_name = attribColl.GetNamedItem("AltGuestName").Value.ToString();
                    }
                    if (attribColl.GetNamedItem("CpuCount") != null && attribColl.GetNamedItem("CpuCount").Value != null)
                    {
                        host.cpuCount = int.Parse(attribColl.GetNamedItem("CpuCount").Value.ToString());
                    }
                    if (attribColl.GetNamedItem("MemSize") != null && attribColl.GetNamedItem("MemSize").Value != null)
                    {
                        long memsize = long.Parse(attribColl.GetNamedItem("MemSize").Value.ToString());
                        memsize = memsize / 1024;
                        host.memSize = int.Parse(memsize.ToString());
                        host.memSize = int.Parse(memsize.ToString().Split('.')[0]);

                    }
                    if (node.HasChildNodes)
                    {
                        XmlNodeList diskList = node.ChildNodes;
                        XmlNodeList nicList = node.ChildNodes;
                        host.vmdkHash = new Hashtable();
                  
                        foreach (XmlNode diskNode in diskList)
                        {
                            reader = new XmlNodeReader(diskNode);
                            XmlAttributeCollection attribCollAttribute = diskNode.Attributes;
                            reader.Read();
                            if (diskNode.Name == "DisksInfo")
                            {
                                if (diskNode.HasChildNodes)
                                {
                                    XmlNodeList diskInfo = diskNode.ChildNodes;
                                    foreach (XmlNode diskInfoNodes in diskInfo)
                                    {
                                        Trace.WriteLine(DateTime.Now + "\t Printing the node name  " + diskInfoNodes.Name);
                                        if (diskInfoNodes.Name == "Disk")
                                        {
                                            XmlAttributeCollection attribCollforDisk = diskInfoNodes.Attributes;
                                            reader = new XmlNodeReader(diskInfoNodes);
                                            reader.Read();
                                            //Nandeesh
                                            Hashtable diskHash = new Hashtable();
                                            host.diskHash = new Hashtable();
                                            if (attribCollforDisk.GetNamedItem("DeviceName") != null && attribCollforDisk.GetNamedItem("DeviceName").Value.Length != 0)
                                            {
                                                Trace.WriteLine(DateTime.Now + "\t Printing the disk anem " + attribCollforDisk.GetNamedItem("DeviceName").Value);
                                                host.diskHash.Add("Name", attribCollforDisk.GetNamedItem("DeviceName").Value);
                                            }
                                            if (attribCollforDisk.GetNamedItem("DiskSize") != null && attribCollforDisk.GetNamedItem("DiskSize").Value.Length != 0)
                                            {
                                                long size = long.Parse(attribCollforDisk.GetNamedItem("DiskSize").Value.ToString());
                                                size = size / 1024;
                                                host.diskHash.Add("Size", size.ToString());
                                            }
                                            if (attribCollforDisk.GetNamedItem("ScsiMapHostID") != null && attribCollforDisk.GetNamedItem("ScsiMapHostID").Value.Length != 0)
                                            {
                                                host.diskHash.Add("scsi_mapping_host", attribCollforDisk.GetNamedItem("ScsiMapHostID").Value.ToString());
                                            }
                                            if (attribCollforDisk.GetNamedItem("ScsiID") != null && attribCollforDisk.GetNamedItem("ScsiID").Value.Length != 0)
                                            {
                                                host.diskHash.Add("scsi_mapping_vmx", attribCollforDisk.GetNamedItem("ScsiID").Value.ToString());
                                            }
                                            if (attribCollforDisk.GetNamedItem("BootDisk") != null && attribCollforDisk.GetNamedItem("BootDisk").Value.Length != 0)
                                            {
                                                host.diskHash.Add("IsBootable", attribCollforDisk.GetNamedItem("BootDisk").Value.ToString());
                                            }
                                            else
                                            {
                                                host.diskHash.Add("IsBootable", "True");
                                            }
                                            host.diskHash.Add("Selected", "Yes");
                                            host.diskHash.Add("disk_type", "persistent");
                                            host.diskHash.Add("Protected", "no");
                                            host.diskHash.Add("SelectedDataStore", "null");
                                            host.diskHash.Add("IsThere", "no");
                                            host.diskHash.Add("IsUsedAsSlot", "no");

                                            if (reader.GetAttribute("ControllerType") != null)
                                            {
                                                host.diskHash.Add("controller_type", reader.GetAttribute("ControllerType"));
                                            }

                                            //  MessageBox.Show("Came here" +reader.GetAttribute("ide_or_scsi").ToString() );
                                            host.diskHash.Add("ide_or_scsi", "scsi");
                                            host.diskHash.Add("independent_persistent", "persistent");
                                            host.disks.list.Add(host.diskHash);
                                        }
                                        
                                    }
                                }
                            }
                        }
                        for (int i = 0; i < host.disks.list.Count; i++)
                        {
                            ((Hashtable)host.disks.list[i])["Name"] = "[DATASTORE_NAME] " + host.hostname + "/"+ "Drive" + i.ToString() + ".vmdk";
                            Trace.WriteLine(DateTime.Now + "\t Printing the diskname " + ((Hashtable)host.disks.list[i])["Name"].ToString());

                        }
                        host.folder_path = host.hostname + "/";
                        host.vmx_path = "[DATASTORE_NAME] " + host.folder_path + host.hostname + ".vmx";
                        foreach (XmlNode nic in nicList)
                        {
                            reader = new XmlNodeReader(nic);
                            XmlAttributeCollection attribCollAttribute = nic.Attributes;

                            reader.Read();

                            if (nic.Name == "Network")
                            {
                                if ( nic.HasChildNodes)
                                {
                                    XmlNodeList nicInfo = nic.ChildNodes;
                                    Hashtable nicHash = new Hashtable();
                                    foreach (XmlNode nicInfoNodes in nicInfo)
                                    {
                                        if (nicInfoNodes.Name == "Nic")
                                        {
                                            XmlAttributeCollection attribCollforNic = nicInfoNodes.Attributes;
                                            reader = new XmlNodeReader(nicInfoNodes);
                                            reader.Read();
                                            host.nicHash = new Hashtable();
                                            if (attribCollforNic.GetNamedItem("Name") != null && attribCollforNic.GetNamedItem("Name").Value.Length != 0)
                                            {
                                                host.nicHash.Add("network_label", attribCollforNic.GetNamedItem("Name").Value.ToString());
                                            }

                                            if (attribCollforNic.GetNamedItem("Macaddress") != null && attribCollforNic.GetNamedItem("Macaddress").Value.Length != 0)
                                            {
                                                host.nicHash.Add("nic_mac", attribCollforNic.GetNamedItem("Macaddress").Value.ToString());
                                            }
                                            if (attribCollforNic.GetNamedItem("Ipaddress") != null && attribCollforNic.GetNamedItem("Ipaddress").Value.Length != 0)
                                            {
                                                host.nicHash.Add("ip", attribCollforNic.GetNamedItem("Ipaddress").Value.ToString());
                                            }
                                            if (attribCollforNic.GetNamedItem("IsDHCP") != null && attribCollforNic.GetNamedItem("IsDHCP").Value.Length != 0)
                                            {
                                                host.nicHash.Add("dhcp", int.Parse(attribCollforNic.GetNamedItem("IsDHCP").Value.ToString()));
                                            }
                                            if (attribCollforNic.GetNamedItem("Netmask") != null && attribCollforNic.GetNamedItem("Netmask").Value.Length != 0)
                                            {
                                                host.nicHash.Add("mask", attribCollforNic.GetNamedItem("Netmask").Value.ToString());
                                            }
                                            if (attribCollforNic.GetNamedItem("gateway") != null && attribCollforNic.GetNamedItem("gateway").Value.Length != 0)
                                            {
                                                host.nicHash.Add("gateway", attribCollforNic.GetNamedItem("gateway").Value.ToString());
                                            }
                                            if (attribCollforNic.GetNamedItem("dnsip") != null && attribCollforNic.GetNamedItem("dnsip").Value.Length != 0)
                                            {
                                                host.nicHash.Add("dnsip", attribCollforNic.GetNamedItem("dnsip").Value.ToString());
                                            }
                                            host.nicList.list.Add(host.nicHash);
                                        }
                                    }
                                }
                            }
                        }
                    }

                   
                }
                catch (Exception xmle)
                {
                    System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(xmle, true);
                    System.FormatException formatException = new FormatException("Inner exception", xmle);
                    Trace.WriteLine(formatException.GetBaseException());
                    Trace.WriteLine(formatException.Data);
                    Trace.WriteLine(formatException.InnerException);
                    Trace.WriteLine(formatException.Message);
                    Trace.WriteLine("ERROR*******************************************");
                    Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                    Trace.WriteLine("Exception  =  " + xmle.Message);
                    Trace.WriteLine("ERROR___________________________________________");
                    Console.WriteLine(xmle.Message);
                    return false;
                }
                
            }
            linHost = host;
            return true;
        }



        public static Host LinuxHost
        {
            get
            {
                return linHost;
            }
            set
            {
                value = linHost;
            }
        }

      
    }
}
