using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Text;
using System.Net.NetworkInformation;
using System.Diagnostics;
using System.IO;
using System.Xml;
using System.Net;
using System.Globalization;
using System.Windows.Forms;
using System.Collections;
using System.Management;
using System.Threading;
using System.Security.Cryptography;
using Com.Inmage.Tools;
using InMage.APIHelpers;
using InMage.APIInterfaces;
using InMage.APIModel;
using HttpCommunication;
using Com.Inmage.Esxcalls;


namespace Com.Inmage
{
    public class Cxapicalls
    {
        internal static string Gethostinfo = "GetHostInfo";
        internal static string Mbrpath = "GetHostMBRPath";
        internal static Host refHost;
        internal static HostList refHostList;
        internal static ArrayList refArraylist;
        internal static string volumesinString;
        internal static string Username = null;
        internal static string Password = null;
        internal static string IP = null;
        internal static string latestPath = WinTools.Vxagentpath() + "\\vContinuum\\Latest";

        public Cxapicalls()
        {

        }

        public static string GetUsername
        {
            get
            {
                return Username;
            }
            set
            {
                value = Username;
            }            
        }

        public static string GetPassword
        {
            get
            {
                return Password;
            }
            set
            {
                value = Password;
            }
        }
        public static string GetIP
        {
            get
            {
                return IP;
            }
            set
            {
                value = IP;
            }
        }

        public static Host GetHost
        {
            get
            {
                return refHost;
            }
            set
            {
                value = refHost;
            }
        }

        public static HostList GetHostlist
        {
            get
            {
                return refHostList;
            }
            set
            {
                value = refHostList;
            }
        }


        public static string Getvolumesstring
        {
            get
            {
                return volumesinString;
            }
            set
            {
                value = volumesinString;
            }
        }

        public static ArrayList GetArrayList
        {
            get
            {
                return refArraylist;
            }
            set
            {
                value = refArraylist;
            }
        }

        public bool RequestXmlForGetHostInfo(string cxIPWithPortnumber, string functionName, Host host)
        {
            try
            {
                string inputxml = null;
                string accessSignature = null;
                string inputString = null;
                string passphrase = "470D91907A7D460718C47AEE6293A62EE995857D";
                inputString = "ID-1" + "1.0" + functionName + "990F9FA2FF55337D1486406F07E91290F6BD6F73" + "scout";

                string inputsignature = CreateHMACSHA1(inputString, passphrase).ToLower();
                byte[] encData_byte = new byte[inputsignature.Length];
                encData_byte = System.Text.ASCIIEncoding.ASCII.GetBytes(inputsignature);
                Trace.WriteLine(DateTime.Now + "\t printing the hmac value in string " + inputsignature);
                Trace.WriteLine(DateTime.Now + "\t printing the hmac value in byres " + encData_byte);
                inputsignature = Convert.ToBase64String(encData_byte);
                Debug.WriteLine(inputsignature);


                accessSignature = inputsignature;


                inputxml = "<Request Id=\"ID-1\" Version=\"1.0\">" + Environment.NewLine
                      + "<Header>" + Environment.NewLine
                              + "<Authentication>" + Environment.NewLine
                                  + "<AccessKeyID>990F9FA2FF55337D1486406F07E91290F6BD6F73</AccessKeyID>" + Environment.NewLine
                                  + "<AccessSignature>" + accessSignature + "</AccessSignature>" + Environment.NewLine
                              + "</Authentication>" + Environment.NewLine

                      + "</Header>" + Environment.NewLine
                      + "<Body>" + Environment.NewLine
                              + "<FunctionRequest Name=" + "\"" + functionName + "\"" + " Include=\"Yes\" >" + Environment.NewLine
                                      + "<Parameter Name=\"HostGUID\" Value=" + "\"" + host.inmage_hostid + "\"" + "/>" + Environment.NewLine
                                      + "<Parameter Name=\"InformationType\" Value=\"All\"/>" + Environment.NewLine
                    //+ "<ParameterGroup Id=\"PG-ID-1\">" + Environment.NewLine
                    //      + "<Parameter Name=\"Name\" Value=\"Value\"/>" + Environment.NewLine
                    // + "</ParameterGroup>" + Environment.NewLine
                              + "</FunctionRequest>" + Environment.NewLine
                      + "</Body>" + Environment.NewLine
                 + "</Request>";

                Trace.WriteLine(DateTime.Now + "\t Priniting the request xml " + inputxml);


                //preparation is done..now calling cx api
                string responseFromServer = null;

                try
                {

                    string url = "http://" + cxIPWithPortnumber + "/ScoutAPI/CXAPI.php";

                    WebRequest request = WebRequest.Create(url);
                    request.Method = "POST";
                   // String postData1 = "&operation=";
                    // xmlData would be send as the value of variable called "xml"
                    // This data would then be wrtitten in to a file callied input.xml
                    // on CX and then given as argument to the cx_cli.php script.
                    //int operation = null;
                   
                    String postData = inputxml;
                    //  MessageBox.Show(postData);
                    //String postData = "&operation=2";
                    byte[] byteArray = Encoding.UTF8.GetBytes(postData);
                    // Set the ContentType property of the WebRequest.
                    request.ContentType = "application/x-www-form-urlencoded";
                    // Set the ContentLength property of the WebRequest.
                    request.ContentLength = byteArray.Length;
                    Stream dataStream = request.GetRequestStream();
                    // Write the data to the request stream.
                    dataStream.Write(byteArray, 0, byteArray.Length);
                    // Get the response.
                    WebResponse response = request.GetResponse();
                    // Display the status.
                    // Get the stream containing content returned by the server.
                    dataStream = response.GetResponseStream();
                    // Open the stream using a StreamReader for easy access.
                    StreamReader reader = new StreamReader(dataStream);
                    // Read the content.
                    responseFromServer = reader.ReadToEnd().Trim();
                    reader.Close();
                    // Close the Stream object.
                    dataStream.Close();
                    response.Close();

                    Trace.WriteLine("input xml is :" + Environment.NewLine + inputxml);
                    Trace.WriteLine(" response xml is :" + Environment.NewLine + responseFromServer);

                    if (functionName == Gethostinfo)
                    {
                        ReadResponseFormCXAPI( host, responseFromServer);
                        host = GetHost;
                        if (host.os == OStype.Windows)
                        {
                            IComparer myComparer = new MyDiskOrderCompare();

                            host.disks.list.Sort(myComparer);
                        }
                        // Trying to sort with network lable using icomparer
                        try
                        {
                            IComparer comparer = new MyNicOrderCompare();
                            host.nicList.list.Sort(comparer);
                        }
                        catch (Exception ex)
                        {
                            Trace.WriteLine(DateTime.Now + "\t Failed to sort nics");
                        }
                    }
                    else if (functionName == Mbrpath)
                    {
                        ReadResponseFormCXAPIFOrMBRPATH( host, responseFromServer);
                        host = GetHost;
                    }
                }

                catch (Exception e)
                {
                    Console.WriteLine(e.Message);

                }

            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                System.FormatException nht = new FormatException("Inner exception", ex);
                Trace.WriteLine(nht.GetBaseException());
                Trace.WriteLine(nht.Data);
                Trace.WriteLine(nht.InnerException);
                Trace.WriteLine(nht.Message);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            refHost = host;
            return true;

        }


        /*
         * Creating HMACSHA1 signature code.
         */

        private static String CreateHMACSHA1(String message, String key)
        {
            System.Text.ASCIIEncoding encoding = new System.Text.ASCIIEncoding();
            byte[] keyByte = encoding.GetBytes(key);
            HMACSHA1 hmacsha1 = new HMACSHA1(keyByte);
            byte[] messageBytes = encoding.GetBytes(message);
            byte[] hashmessage = hmacsha1.ComputeHash(messageBytes);
            String HMACString = ByteToString(hashmessage);
            return HMACString;
        }

        /*
         *  Create Bytes to String
         */
        private static string ByteToString(byte[] buff)
        {
            string sbinary = "";
            for (int i = 0; i < buff.Length; i++)
            {
                sbinary += buff[i].ToString("X2"); // hex format
            }
            return (sbinary);
        }


        public class MyDiskOrderCompare : IComparer
        {

            // Calls CaseInsensitiveComparer.Compare with the parameters reversed.
            int IComparer.Compare(Object x, Object y)
            {
                Hashtable disk1 = (Hashtable)x;
                Hashtable disk2 = (Hashtable)y;
                if (disk1["disknumber"] != null && disk2["disknumber"] != null)
                {
                    int disk1Name = int.Parse(disk1["disknumber"].ToString());
                    int disk2Name = int.Parse(disk2["disknumber"].ToString());
                    return disk1Name.CompareTo(disk2Name);
                }
                else
                {
                    return 0;
                }
            }
        }

        internal class MyNicOrderCompare : IComparer
        {

            // Calls CaseInsensitiveComparer.Compare with the parameters reversed.
            int IComparer.Compare(Object x, Object y)
            {
                Hashtable nic1 = (Hashtable)x;
                Hashtable nic2 = (Hashtable)y;

                string nic11Name = nic1["network_label"].ToString();
                string nic2Name = nic2["network_label"].ToString();

                return nic11Name.CompareTo(nic2Name);

            }
        }

        private bool ReadResponseFormCXAPI( Host host, string responseFromServer)
        {
            try
            {
                host = new Host();
                //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
                //based on above comment we have added xmlresolver as null
                XmlDocument xDocument = new XmlDocument();
                xDocument.XmlResolver = null;
                string fileFullPath = WinTools.FxAgentPath() + "\\vContinuum\\Responce.xml";

                StreamWriter sr1 = new StreamWriter(latestPath +WinPreReqs.tempXml);
                sr1.Write(responseFromServer);
                sr1.Close();

                //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
                XmlReaderSettings settings = new XmlReaderSettings();
                settings.ProhibitDtd = true;
                settings.XmlResolver = null;
                using (XmlReader reader1 = XmlReader.Create(latestPath + WinPreReqs.tempXml, settings))
                {
                    xDocument.Load(reader1);
                    //reader1.Close();
                }
                XmlNodeList responsenode = xDocument.GetElementsByTagName("Response");
                foreach (XmlNode node in responsenode)
                {
                    XmlAttributeCollection attribColl = node.Attributes;
                    if (node.Attributes["Message"] != null)
                    {
                        string result = attribColl.GetNamedItem("Message").Value.ToString();
                        Trace.WriteLine(DateTime.Now + "\t Printing the result of the cx api call " + result);
                        if (result.ToUpper() == "SUCCESS")
                        {
                            Trace.WriteLine(DateTime.Now + "\t Result id success");
                        }


                    }
                }
                XmlNodeList functionResponse = xDocument.GetElementsByTagName("FunctionResponse");
                foreach (XmlNode node in functionResponse)
                {
                    if (node.HasChildNodes)
                    {
                        XmlNodeList hostNodes = node.ChildNodes;
                        foreach (XmlNode hostnodes in hostNodes)
                        {
                            if (hostnodes.Attributes["Name"] != null && hostnodes.Attributes["Name"].Value == "HostId")
                            {
                                if (hostnodes.Attributes["Value"] != null && hostnodes.Attributes["Value"].Value.Length != 0)
                                {
                                    host.inmage_hostid = hostnodes.Attributes["Value"].Value;
                                    Trace.WriteLine(DateTime.Now + "\t Printing the host id value " + host.inmage_hostid);
                                }
                            }
                            if (hostnodes.Attributes["Name"] != null && hostnodes.Attributes["Name"].Value == "HostName")
                            {
                                if (hostnodes.Attributes["Value"] != null && hostnodes.Attributes["Value"].Value.Length != 0)
                                {
                                    host.hostname = hostnodes.Attributes["Value"].Value;
                                    host.displayname = host.hostname;
                                    Trace.WriteLine(DateTime.Now + "\t Printing the hostName " + host.hostname);
                                }
                            }
                            if (hostnodes.Attributes["Name"] != null && hostnodes.Attributes["Name"].Value == "MemSize")
                            {
                                if (hostnodes.Attributes["Value"] != null && hostnodes.Attributes["Value"].Value.Length != 0)
                                {
                                    long memsize = long.Parse(hostnodes.Attributes["Value"].Value.ToString());
                                    memsize = memsize / (1024 * 1024);
                                    host.memSize = int.Parse(memsize.ToString());
                                    Trace.WriteLine(DateTime.Now + "\t Printing the memsize " + host.memSize.ToString());
                                }
                            }
                            if (hostnodes.Attributes["Name"] != null && hostnodes.Attributes["Name"].Value == "CPUCount")
                            {
                                if (hostnodes.Attributes["Value"] != null && hostnodes.Attributes["Value"].Value.Length != 0)
                                {
                                    host.cpuCount = int.Parse(hostnodes.Attributes["Value"].Value.ToString());

                                    Trace.WriteLine(DateTime.Now + "\t Printing the cpu count " + host.cpuCount.ToString());
                                }
                            }
                            if (hostnodes.Attributes["Name"] != null && hostnodes.Attributes["Name"].Value == "MachineType")
                            {
                                if (hostnodes.Attributes["Value"] != null && hostnodes.Attributes["Value"].Value.Length != 0)
                                {
                                    host.machineType = hostnodes.Attributes["Value"].Value;

                                    Trace.WriteLine(DateTime.Now + "\t Printing the cpu count " + host.machineType);
                                }
                            }
                            if (hostnodes.Attributes["Name"] != null && hostnodes.Attributes["Name"].Value == "Caption")
                            {
                                if (hostnodes.Attributes["Value"] != null && hostnodes.Attributes["Value"].Value.Length != 0)
                                {
                                    host.operatingSystem = hostnodes.Attributes["Value"].Value;

                                    Trace.WriteLine(DateTime.Now + "\t Printing the operating system " + host.operatingSystem);
                                }
                            }
                            if (hostnodes.Attributes["Name"] != null && hostnodes.Attributes["Name"].Value == "CPUArchitecture")
                            {
                                if (hostnodes.Attributes["Value"] != null && hostnodes.Attributes["Value"].Value.Length != 0)
                                {
                                    host.cpuVersion = host.operatingSystem + " " +  hostnodes.Attributes["Value"].Value;

                                    Trace.WriteLine(DateTime.Now + "\t Printing the cpu version " + host.cpuVersion);
                                }
                            }
                            if (hostnodes.Attributes["Name"] != null && hostnodes.Attributes["Name"].Value == "NumberOfDisks")
                            {
                                if (hostnodes.Attributes["Value"] != null && hostnodes.Attributes["Value"].Value.Length != 0)
                                {
                                    host.number_of_disks = int.Parse(hostnodes.Attributes["Value"].Value.ToString());
                                    Trace.WriteLine(DateTime.Now + "\t Printing the number of disks  " + host.number_of_disks.ToString());
                                }
                            }
                            if (hostnodes.Attributes["Name"] != null && hostnodes.Attributes["Name"].Value == "Cluster")
                            {
                                if (hostnodes.Attributes["Value"] != null && hostnodes.Attributes["Value"].Value.Length != 0)
                                {
                                    bool cluster = bool.Parse(hostnodes.Attributes["Value"].Value.ToString());
                                    if (cluster == false)
                                    {
                                        host.cluster = "no";

                                    }
                                    else
                                    {
                                        host.cluster = "yes";
                                    }
                                    Trace.WriteLine(DateTime.Now + "\t Printing the cluster   " + host.cluster.ToString());
                                }
                            }
                            
                            ReadDiskNodes(xDocument, hostnodes,  host);
                           
                            host = GetHost;
                            ReadNicNodes(xDocument, hostnodes,  host);
                            host = GetHost;
                        }
                        if (host.os == OStype.Windows)
                        {
                            IComparer myComparer = new MyDiskOrderCompare();

                            host.disks.list.Sort(myComparer);
                        }
                        Trace.WriteLine(DateTime.Now + "\t Disk count " + host.disks.list.Count.ToString());
                        for (int k = 0; k < host.disks.list.Count; k++)
                        {
                            ((Hashtable)host.disks.list[k])["Name"] = "[DATASTORE_NAME] " + host.hostname + "/" + ((Hashtable)host.disks.list[k])["Name"] + ".vmdk";

                        }
                        host.folder_path = host.hostname + "/";
                        host.vmx_path = "[DATASTORE_NAME] " + host.folder_path + host.hostname + ".vmx";
                        Trace.WriteLine(DateTime.Now + "\t Printing the disk count " + host.disks.list.Count.ToString());
                        Trace.WriteLine(DateTime.Now + "\t Printing the nic count " + host.nicList.list.Count.ToString());
                        RemoteWin remote = new RemoteWin("", "", "", "", "");
                        if (host.os == OStype.Windows)
                        {
                            if (host.operatingSystem != null)
                            {
                                RemoteWin.GetOSArchitectureFromWmiCaptionString(host.operatingSystem, host.cpuVersion, host.alt_guest_name);
                                host.alt_guest_name = remote.GetOSediton;
                            }
                            else
                            {
                                Trace.WriteLine(DateTime.Now + "\t Got the Operating system value as null ");
                            }
                        }
                        else if (host.os == OStype.Linux)
                        {
                            RemoteWin.GetOSArchitectureFromWmiCaptionStringForLinux(host.operatingSystem, host.cpuVersion,  host.alt_guest_name);
                            host.alt_guest_name = remote.GetOSediton;
                        }
                        host.operatingSystem = host.operatingSystem + " " + host.cpuVersion;
                        Trace.WriteLine(DateTime.Now + "\t alt guest id " + host.alt_guest_name);
                    }
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                System.FormatException nht = new FormatException("Inner exception", ex);
                Trace.WriteLine(nht.GetBaseException());
                Trace.WriteLine(nht.Data);
                Trace.WriteLine(nht.InnerException);
                Trace.WriteLine(nht.Message);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            refHost = host;
            return true;
        }


        private bool ReadDiskNodes(XmlDocument xDocument, XmlNode hostnodes,  Host host)
        {
            if (hostnodes.Name == "ParameterGroup" && hostnodes.Attributes["Id"] != null && hostnodes.Attributes["Id"].Value == "DiskList")
            {
                XmlNodeList diskNodes = xDocument.GetElementsByTagName("ParameterGroup");

                foreach (XmlNode disks in diskNodes)
                {
                    if (disks.HasChildNodes)
                    {
                        XmlNodeList diskChildNode = disks.ChildNodes;
                        foreach (XmlNode disksNodes in diskChildNode)
                        {
                            for (int i = 1; i <= host.number_of_disks; i++)
                            {
                                
                                if (disksNodes.Attributes["Id"] != null && disksNodes.Attributes["Id"].Value == "Disk" + i.ToString())
                                {
                                    int partition = disksNodes.ChildNodes.Count;
                                    Trace.WriteLine(DateTime.Now + "\t Printing the partition count " + partition.ToString());
                                    Trace.WriteLine(DateTime.Now + "\t Printing the disknodes name " + disksNodes.Attributes["Id"].Value);
                                    if (disksNodes.HasChildNodes)
                                    {
                                        XmlNodeList diskChildChildNode = disksNodes.ChildNodes;
                                        Hashtable diskHash = new Hashtable();
                                        host.diskHash = new Hashtable();
                                        foreach (XmlNode nodeOfEachDisk in diskChildChildNode)
                                        {
                                            XmlNodeReader reader = new XmlNodeReader(nodeOfEachDisk);
                                            XmlAttributeCollection attribCollforDisk = nodeOfEachDisk.Attributes;
                                            reader = new XmlNodeReader(nodeOfEachDisk);
                                            reader.Read();
                                            if (nodeOfEachDisk.Attributes["Name"] != null && nodeOfEachDisk.Attributes["Name"].Value == "DiskName")
                                            {
                                                if (nodeOfEachDisk.Attributes["Value"] != null && nodeOfEachDisk.Attributes["Value"].Value.Length > 0)
                                                {
                                                    string name = nodeOfEachDisk.Attributes["Value"].Value;
                                                    //Cut first 12 chars to get "DRIVE0"  in \\.\PHYSICALDRIVE0
                                                    name = name.Remove(0, 12);
                                                    Trace.WriteLine(DateTime.Now + "\t Printing the name of the disk " + name);
                                                    host.diskHash.Add("Name", name);
                                                }

                                            }
                                            if (nodeOfEachDisk.Attributes["Name"] != null && nodeOfEachDisk.Attributes["Name"].Value == "Size")
                                            {
                                                if (nodeOfEachDisk.Attributes["Value"] != null && nodeOfEachDisk.Attributes["Value"].Value.Length > 0)
                                                {
                                                    long size = long.Parse(nodeOfEachDisk.Attributes["Value"].Value.ToString());
                                                    size = size / 1024;
                                                    Trace.WriteLine(DateTime.Now + "\t Printing the size of the disk " + size);
                                                    host.diskHash.Add("Size", size.ToString());
                                                }

                                            }
                                            if (nodeOfEachDisk.Attributes["Name"] != null && nodeOfEachDisk.Attributes["Name"].Value == "IdeOrScsi")
                                            {
                                                if (nodeOfEachDisk.Attributes["Value"] != null && nodeOfEachDisk.Attributes["Value"].Value.Length > 0)
                                                {
                                                    host.diskHash.Add("IdeOrScsi", nodeOfEachDisk.Attributes["Value"].Value);
                                                }
                                            }
                                            if (nodeOfEachDisk.Attributes["Name"] != null && nodeOfEachDisk.Attributes["Name"].Value == "ScsiPort")
                                            {
                                                if (nodeOfEachDisk.Attributes["Value"] != null && nodeOfEachDisk.Attributes["Value"].Value.Length > 0)
                                                {
                                                    host.diskHash.Add("ScsiPort", nodeOfEachDisk.Attributes["Value"].Value);
                                                }
                                            }
                                            if (nodeOfEachDisk.Attributes["Name"] != null && nodeOfEachDisk.Attributes["Name"].Value == "ScsiTargetId")
                                            {
                                                if (nodeOfEachDisk.Attributes["Value"] != null && nodeOfEachDisk.Attributes["Value"].Value.Length > 0)
                                                {
                                                    host.diskHash.Add("ScsiTargetId", nodeOfEachDisk.Attributes["Value"].Value);
                                                }
                                            }
                                            if (nodeOfEachDisk.Attributes["Name"] != null && nodeOfEachDisk.Attributes["Name"].Value == "ScsiLogicalUnit")
                                            {
                                                if (nodeOfEachDisk.Attributes["Value"] != null && nodeOfEachDisk.Attributes["Value"].Value.Length > 0)
                                                {
                                                    host.diskHash.Add("ScsiLogicalUnit", nodeOfEachDisk.Attributes["Value"].Value);
                                                }
                                            }
                                            if (nodeOfEachDisk.Attributes["Name"] != null && nodeOfEachDisk.Attributes["Name"].Value == "IsIscsiDevice")
                                            {
                                                if (nodeOfEachDisk.Attributes["Value"] != null && nodeOfEachDisk.Attributes["Value"].Value.Length > 0)
                                                {
                                                    host.diskHash.Add("IsIscsiDevice", nodeOfEachDisk.Attributes["Value"].Value);
                                                }
                                            }
                                            if (nodeOfEachDisk.Attributes["Name"] != null && nodeOfEachDisk.Attributes["Name"].Value == "DiskType")
                                            {
                                                if (nodeOfEachDisk.Attributes["Value"] != null && nodeOfEachDisk.Attributes["Value"].Value.Length > 0)
                                                {
                                                    host.diskHash.Add("disk_type", nodeOfEachDisk.Attributes["Value"].Value);
                                                }
                                            }
                                            if (nodeOfEachDisk.Attributes["Name"] != null && nodeOfEachDisk.Attributes["Name"].Value == "DiskLayout")
                                            {
                                                host.diskHash.Add("disk_mode", nodeOfEachDisk.Attributes["Value"].Value);
                                            }
                                            if (nodeOfEachDisk.Attributes["Name"] != null && nodeOfEachDisk.Attributes["Name"].Value == "Heads")
                                            {
                                                if (nodeOfEachDisk.Attributes["Value"] != null && nodeOfEachDisk.Attributes["Value"].Value.Length > 0)
                                                {
                                                    host.diskHash.Add("Heads", nodeOfEachDisk.Attributes["Value"].Value);
                                                }
                                            }
                                            if (nodeOfEachDisk.Attributes["Name"] != null && nodeOfEachDisk.Attributes["Name"].Value == "Sectors")
                                            {
                                                if (nodeOfEachDisk.Attributes["Value"] != null && nodeOfEachDisk.Attributes["Value"].Value.Length > 0)
                                                {
                                                    host.diskHash.Add("Sectors", nodeOfEachDisk.Attributes["Value"].Value);
                                                }
                                            }
                                            if (nodeOfEachDisk.Attributes["Name"] != null && nodeOfEachDisk.Attributes["Name"].Value == "Bootable")
                                            {
                                                if (nodeOfEachDisk.Attributes["Value"] != null && nodeOfEachDisk.Attributes["Value"].Value.Length > 0)
                                                {
                                                    host.diskHash.Add("IsBootable", nodeOfEachDisk.Attributes["Value"].Value);
                                                }
                                            }
                                            if (nodeOfEachDisk.Attributes["Name"] != null && nodeOfEachDisk.Attributes["Name"].Value == "ScsiControllerModel")
                                            {
                                                if (nodeOfEachDisk.Attributes["Value"] != null && nodeOfEachDisk.Attributes["Value"].Value.Length > 0)
                                                {
                                                    host.diskHash.Add("controller_type", nodeOfEachDisk.Attributes["Value"].Value);
                                                }
                                            }


                                           
                                                
                                            
                                        }

                                        string scsimappingHost = host.diskHash["ScsiPort"].ToString() +":" + host.diskHash["ScsiTargetId"].ToString()+":" + host.diskHash["ScsiLogicalUnit"].ToString();
                                        host.diskHash.Add("scsi_mapping_host", scsimappingHost);
                                        host.diskHash.Add("Selected", "Yes");
                                       
                                        host.disks.list.Add(host.diskHash);
                                        
                                        Trace.WriteLine(DateTime.Now + "\t Printing the count of the disks " + host.disks.list.Count.ToString());
                                    }



                                }
                            }
                        }
                        
                    }

                }
            }
            refHost = host;
            return true;
        }


        private bool ReadNicNodes(XmlDocument xDocument, XmlNode hostnodes,  Host host)
        {
            if (hostnodes.Name == "ParameterGroup" && hostnodes.Attributes["Id"] != null && hostnodes.Attributes["Id"].Value == "MacAddressList")
            {
                Trace.WriteLine(DateTime.Now + "\t Printing the nic count " + hostnodes.ChildNodes.Count.ToString());
                int numberOfNics = hostnodes.ChildNodes.Count;
                XmlNodeList nicNodes = xDocument.GetElementsByTagName("ParameterGroup");
                foreach (XmlNode nics in nicNodes)
                {
                    if (nics.HasChildNodes)
                    {
                        XmlNodeList nicChildNode = nics.ChildNodes;
                        foreach (XmlNode macNodes in nicChildNode)
                        {
                            for (int i = 1; i <= numberOfNics; i++)
                            {
                                
                                if (macNodes.Attributes["Id"] != null && macNodes.Attributes["Id"].Value == "MacAddress" + i.ToString())
                                {

                                    int ipsCount = macNodes.ChildNodes.Count;
                                   
                                    Trace.WriteLine(DateTime.Now + "\t Printing the child nodes of macaddress  " + macNodes.ChildNodes.Count.ToString());
                                    Trace.WriteLine(DateTime.Now + "\t Printing the mac name " + macNodes.Attributes["Id"].Value);
                                    if (macNodes.HasChildNodes)
                                    {
                                        Hashtable nicHash = new Hashtable();
                                        XmlNodeList nicChildChildNode = macNodes.ChildNodes;

                                        foreach (XmlNode nodeOfEachMac in nicChildChildNode)
                                        {
                                            XmlNodeReader reader = new XmlNodeReader(nodeOfEachMac);
                                            XmlAttributeCollection attribCollforDisk = nodeOfEachMac.Attributes;
                                            reader = new XmlNodeReader(nodeOfEachMac);
                                            reader.Read();
                                            if (nodeOfEachMac.Attributes["Name"] != null && nodeOfEachMac.Attributes["Name"].Value == "MacAddress")
                                            {

                                                if (nodeOfEachMac.Attributes["Value"] != null && nodeOfEachMac.Attributes["Value"].Value.Length > 0)
                                                {
                                                    nicHash.Add("nic_mac", nodeOfEachMac.Attributes["Value"].Value);
                                                    Trace.WriteLine(DateTime.Now + " \t Printing the mac values " + nodeOfEachMac.Attributes["Value"].Value);
                                                }                                              
                                            }
                                        }
                                        string ip = null;
                                        string subnetmask = null;
                                        string gateway = null;
                                        string dnsip = null;
                                        bool dhcp = false;
                                        for (int j = 0; j < ipsCount; j++)
                                        {
                                           
                                            foreach (XmlNode ips in macNodes.ChildNodes[j])
                                            {
                                                if (ips.Attributes["Name"] != null && ips.Attributes["Name"].Value == "Ip")
                                                {

                                                    if (ips.Attributes["Value"] != null && ips.Attributes["Value"].Value.Length > 0)
                                                    {
                                                        if (ip == null)
                                                        {
                                                            ip = ips.Attributes["Value"].Value;
                                                           
                                                        }
                                                        else
                                                        {
                                                            ip =  ip + "," +ips.Attributes["Value"].Value;
                                                        }

                                                        Trace.WriteLine(DateTime.Now + "\t Printing the ips " + ip);
                                                    }
                                                }
                                                if (ips.Attributes["Name"] != null && ips.Attributes["Name"].Value == "subnetmask")
                                                {

                                                    if (ips.Attributes["Value"] != null && ips.Attributes["Value"].Value.Length > 0)
                                                    {
                                                        if (subnetmask == null)
                                                        {
                                                            subnetmask = ips.Attributes["Value"].Value;

                                                        }
                                                        else
                                                        {
                                                            subnetmask = subnetmask + "," + ips.Attributes["Value"].Value;
                                                        }

                                                        Trace.WriteLine(DateTime.Now + "\t Printing the subnetmask " + subnetmask);
                                                    }
                                                }
                                                if (ips.Attributes["Name"] != null && ips.Attributes["Name"].Value == "gateway")
                                                {

                                                    if (ips.Attributes["Value"] != null && ips.Attributes["Value"].Value.Length > 0)
                                                    {
                                                        if (gateway == null)
                                                        {
                                                            gateway = ips.Attributes["Value"].Value;

                                                        }
                                                        else
                                                        {
                                                            gateway = gateway + "," + ips.Attributes["Value"].Value;
                                                        }

                                                        Trace.WriteLine(DateTime.Now + "\t Printing the gateway " + gateway);
                                                    }
                                                }
                                                if (ips.Attributes["Name"] != null && ips.Attributes["Name"].Value == "dnsip")
                                                {

                                                    if (ips.Attributes["Value"] != null && ips.Attributes["Value"].Value.Length > 0)
                                                    {
                                                        if (gateway == null)
                                                        {
                                                            dnsip = ips.Attributes["Value"].Value;

                                                        }
                                                        else
                                                        {
                                                            dnsip = dnsip + "," + ips.Attributes["Value"].Value;
                                                        }

                                                        Trace.WriteLine(DateTime.Now + "\t Printing the gateway " + gateway);
                                                    }
                                                    
                                                }
                                                if (ips.Attributes["Name"] != null && ips.Attributes["Name"].Value == "isdhcp")
                                                {

                                                    if (ips.Attributes["Value"] != null && ips.Attributes["Value"].Value.Length > 0)
                                                    {
                                                        dhcp = bool.Parse(ips.Attributes["Value"].Value.ToString());
                                                        Trace.WriteLine(DateTime.Now + "\t Printing the dhcp " + ips.Attributes["Value"].Value);
                                                    }
                                                }
                                            }
                                            if (j == ipsCount - 1)
                                            {
                                                nicHash.Add("ip", ip);
                                                nicHash.Add("mask", subnetmask);
                                                nicHash.Add("gateway", gateway);
                                                nicHash.Add("dnsip", dnsip);
                                                if (dhcp == true)
                                                {
                                                    nicHash.Add("dhcp", "1");
                                                }
                                                else
                                                {
                                                    nicHash.Add("dhcp", "0");
                                                }
                                                nicHash.Add("network_label", "Network adapter " + (j).ToString());
                                                host.nicList.list.Add(nicHash);

                                                Trace.WriteLine(DateTime.Now + "\t Printing all the info of nic " + ip + " subnet mask " + subnetmask + "\t gateway " + gateway + "\t dns " + dnsip + "\t nic mac " + nicHash["nic_mac"].ToString());
                                                nicHash = new Hashtable();
                                            }
                                        }
                                    }

                                }
                              
                            }
                        }

                    }

                }
            }
            refHost = host;
            return true;
        }

       


        public static int GetAllHostFromCX(HostList hostList, string cxIPWithPortNumber, OStype OStype)
        {
            string inPutString = null;
           

           
            try
            {
               
                String postData1 = "&operation=";
                // xmlData would be send as the value of variable called "xml"
                // This data would then be wrtitten in to a file callied input.xml
                // on CX and then given as argument to the cx_cli.php script.
                int operation = 3;
                String postData2 = "&userfile='input.xml'&xml=" + inPutString;
                String postData = postData1 + operation + postData2;
                WinPreReqs win = new WinPreReqs("", "", "", "");
                HttpReply reply = win.ConnectToHttpClient(postData);
                if (String.IsNullOrEmpty(reply.error))
                {
                    // process reply
                    if (!String.IsNullOrEmpty(reply.data))
                    {
                        Trace.WriteLine(reply.data);
                    }
                }
                else
                {

                    Trace.WriteLine(reply.error);
                    return 1;
                }
                string responseFromServer = reply.data;
                StreamWriter sr1 = new StreamWriter(latestPath + WinPreReqs.tempXml);
                sr1.Write(responseFromServer);
                sr1.Close();
                //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
                //based on above comment we have added xmlresolver as null
                XmlDocument xDocument = new XmlDocument();
                xDocument.XmlResolver = null;

                //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
                 XmlReaderSettings settings = new XmlReaderSettings();
                settings.ProhibitDtd = true;
                settings.XmlResolver = null;
                using (XmlReader reader1 = XmlReader.Create(latestPath + WinPreReqs.tempXml, settings))
                {
                    xDocument.Load(reader1);
                    //reader1.Close();
                }
                XmlNodeList hostnode = xDocument.GetElementsByTagName("host");

                foreach (XmlNode node in hostnode)
                {
                    Host host = new Host();
                    XmlAttributeCollection attribColl = node.Attributes;
                    if (node.Attributes["id"] != null)
                    {
                        host.inmage_hostid = attribColl.GetNamedItem("id").Value.ToString();
                        Trace.WriteLine(DateTime.Now + "\t Printing id " + host.inmage_hostid );
                    }
                    if (node.HasChildNodes)
                    {
                        foreach (XmlNode xNode in node)
                        {
                            if (xNode.Name == "ip_address")
                            {
                                host.ip = (xNode.InnerText != null ? xNode.InnerText : "");
                            }
                            if (xNode.Name == "os_type")
                            {
                                if (xNode.InnerText.ToUpper().ToString() == "WINDOWS")
                                {
                                    host.os = OStype.Windows;
                                }
                                else if (xNode.InnerText.ToUpper().ToString().Contains("LINUX"))
                                {
                                    host.os = OStype.Linux;
                                }
                                else
                                {
                                    host.os = OStype.Solaris;
                                }
                            }
                            if (xNode.Name == "name")
                            {
                                host.hostname = (xNode.InnerText != null ? xNode.InnerText : "");
                                Trace.WriteLine(DateTime.Now + "\t Hostname " + host.hostname);
                            }
                            if (xNode.Name == "master_target")
                            {
                                if (xNode.InnerText != null && xNode.InnerText.ToUpper() == "TRUE")
                                {
                                    host.masterTarget = true;
                                }
                            }
                        }
                    }
                    if (OStype == host.os)
                    {
                        hostList.AddOrReplaceHost(host);
                    }
                }

            }
            //Process_Server_List.Update();
            catch (System.Net.WebException e)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);
                System.FormatException nht = new FormatException("Inner exception", e);
                Trace.WriteLine(nht.GetBaseException());
                Trace.WriteLine(nht.Data);
                Trace.WriteLine(nht.InnerException);
                Trace.WriteLine(nht.Message);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + e.Message);
                Trace.WriteLine("ERROR___________________________________________");
                Trace.WriteLine("This error from GetDetailsFromCxCli() method");
                Trace.WriteLine(e.ToString());
                //MessageBox.Show("Server is not pointed to the CX.Please veriry whether agents are installed or not.And they are pointing to " + inCXIP);

                return 1;
            }
            catch (Exception e)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);
                System.FormatException nht = new FormatException("Inner exception", e);
                Trace.WriteLine(nht.GetBaseException());
                Trace.WriteLine(nht.Data);
                Trace.WriteLine(nht.InnerException);
                Trace.WriteLine(nht.Message);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + e.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return 1;
            }
            refHostList = hostList;
            return 0;

        }

        private bool ReadResponseFormCXAPIFOrMBRPATH(Host host, string responseFromServer)
        {
            try
            {
                host = new Host();
                //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
                //based on above comment we have added xmlresolver as null
                XmlDocument xDocument = new XmlDocument();
                xDocument.XmlResolver = null; 

                StreamWriter sr1 = new StreamWriter(latestPath + WinPreReqs.tempXml);
                sr1.Write(responseFromServer);
                sr1.Close();
                //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
                XmlReaderSettings settings = new XmlReaderSettings();
                settings.ProhibitDtd = true;
                settings.XmlResolver = null;
                using (XmlReader reader1 = XmlReader.Create(latestPath + WinPreReqs.tempXml, settings))
                {
                    xDocument.Load(reader1);
                    //reader1.Close();
                }
                XmlNodeList responsenode = xDocument.GetElementsByTagName("Response");
                foreach (XmlNode node in responsenode)
                {
                    XmlAttributeCollection attribColl = node.Attributes;
                    if (node.Attributes["Message"] != null)
                    {
                        string result = attribColl.GetNamedItem("Message").Value.ToString();
                        Trace.WriteLine(DateTime.Now + "\t Printing the result of the cx api call " + result);
                        if (result.ToUpper() == "SUCCESS")
                        {
                            Trace.WriteLine(DateTime.Now + "\t Result id success");
                        }


                    }
                }
                XmlNodeList functionResponse = xDocument.GetElementsByTagName("FunctionResponse");
                foreach (XmlNode node in functionResponse)
                {
                    if (node.HasChildNodes)
                    {
                        XmlNodeList hostNodes = node.ChildNodes;
                        foreach (XmlNode hostnodes in hostNodes)
                        {
                            if (hostnodes.Attributes["Name"] != null && hostnodes.Attributes["Name"].Value == "MBRPath")
                            {
                                if (hostnodes.Attributes["Value"] != null && hostnodes.Attributes["Value"].Value.Length != 0)
                                {
                                    host.mbr_path = hostnodes.Attributes["Value"].Value;
                                    Trace.WriteLine(DateTime.Now + "\t Printing the host mbr path value " + host.mbr_path);
                                }
                            }
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                System.FormatException nht = new FormatException("Inner exception", ex);
                Trace.WriteLine(nht.GetBaseException());
                Trace.WriteLine(nht.Data);
                Trace.WriteLine(nht.InnerException);
                Trace.WriteLine(nht.Message);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
           
            refHost = host;
            return true;
        }

        public bool PostFXJob(Host masterHost, Inmage.Esxcalls.OperationType operationType)
        {
            try
            {
                string cxip = WinTools.GetcxIPWithPortNumber();
                Response response = new Response();
                ArrayList list = new ArrayList();
                Parameter param = new Parameter();
                ParameterGroup paraGroup = new ParameterGroup();
                FunctionRequest functionRequest = new FunctionRequest();
                functionRequest.Name = "ProtectESX";
                param = new Parameter();
                param.Name = "HostIdentification";
                param.Value = masterHost.inmage_hostid;
                functionRequest.addChildObj(param);
                param = new Parameter();
                param.Name = "PlanName";
                if (operationType == Com.Inmage.Esxcalls.OperationType.Resize)
                {
                    param.Value = masterHost.plan + "_resize";
                }
                else if(operationType == Com.Inmage.Esxcalls.OperationType.Removedisk)
                {
                    param.Value = masterHost.plan + "_Remove";
                }
                else if (operationType == Com.Inmage.Esxcalls.OperationType.Removevolume)
                {
                    param.Value = masterHost.plan + "_Remove_volume";
                }
                else
                {
                    param.Value = masterHost.plan;
                }
                functionRequest.addChildObj(param);
                if (operationType == Com.Inmage.Esxcalls.OperationType.Recover)
                {
                    if (masterHost.recoveryLater == true)
                    {
                        param = new Parameter();
                        param.Name = "ScheduleType";
                        param.Value = "run_on_demand";
                        functionRequest.addChildObj(param);
                    }
                }
                param = new Parameter();
                param.Name = "JobType";
                if (operationType == Com.Inmage.Esxcalls.OperationType.Initialprotection)
                {
                    param.Value = "Protection";
                }
                else if (operationType == Com.Inmage.Esxcalls.OperationType.Recover)
                {
                    param.Value = "Recovery";
                }
                else if (operationType == Com.Inmage.Esxcalls.OperationType.Drdrill)
                {
                    param.Value = "DrDrill";
                }
                else if (operationType == Com.Inmage.Esxcalls.OperationType.Failback || operationType == Com.Inmage.Esxcalls.OperationType.V2P)
                {
                    param.Value = "Failback";
                }
                else if (operationType == Com.Inmage.Esxcalls.OperationType.Additionofdisk)
                {
                    param.Value = "Adddisk";
                }
                else if (operationType == Com.Inmage.Esxcalls.OperationType.Resume)
                {
                    param.Value = "Resume";
                }
                else if (operationType == Com.Inmage.Esxcalls.OperationType.Resize)
                {
                    param.Value = "Resize";
                }
                else if (operationType == Com.Inmage.Esxcalls.OperationType.Removedisk)
                {
                    param.Value = "Remove Disk";
                }
                else if (operationType == Com.Inmage.Esxcalls.OperationType.Removevolume)
                {
                    param.Value = "Remove volume";
                }

                functionRequest.addChildObj(param);
                param = new Parameter();
                param.Name = "ConfigureFXJob";
                param.Value = "Yes";
                functionRequest.addChildObj(param);

                param = new Parameter();
                if (operationType == Com.Inmage.Esxcalls.OperationType.Recover || operationType == Com.Inmage.Esxcalls.OperationType.Drdrill || operationType == Com.Inmage.Esxcalls.OperationType.Removedisk)
                {
                    paraGroup.Id = "JobOptions";
                    param.Name = "JobName";
                    param.Value = "Master target - vContinuum";
                    paraGroup.addChildObj(param);
                }
                else
                {
                    paraGroup.Id = "JobOptions";
                    param.Name = "JobName";
                    param.Value = "Master Target - Master Target";
                    paraGroup.addChildObj(param);
                }
                if (operationType == Com.Inmage.Esxcalls.OperationType.Recover || operationType == Com.Inmage.Esxcalls.OperationType.Drdrill || operationType == Com.Inmage.Esxcalls.OperationType.Removedisk)
                {
                    Host tempHost = new Host();
                    tempHost.hostname = Environment.MachineName;
                    tempHost.displayname = Environment.MachineName;

                    WinPreReqs wp = new WinPreReqs("", "", "", "");
                    string ipaddress = "";


                    int returnCodeofTempHost = wp.SetHostIPinputhostname(tempHost.hostname,  ipaddress, cxip);
                    ipaddress = WinPreReqs.GetIPaddress;
                    if (returnCodeofTempHost == 0)
                    {
                        tempHost.ip = ipaddress;

                        tempHost.numberOfEntriesInCX = 1;
                        ///checking vCon box fx licensed or not
                        if (wp.GetDetailsFromcxcli( tempHost, cxip) == true)
                        {
                            tempHost = WinPreReqs.GetHost;
                            if (tempHost.inmage_hostid != null)
                            {
                                Trace.WriteLine(DateTime.Now + "\t Successfully got the vcon uuid");
                            }
                            else
                            {
                                Trace.WriteLine(DateTime.Now + "\t Failed to get the vcon uuid");
                            }
                        }


                        param = new Parameter();
                        param.Name = "TargetHostId";
                        param.Value = tempHost.inmage_hostid;
                        paraGroup.addChildObj(param);
                    }
                }
                else
                {
                    if (masterHost.inmage_hostid == null)
                    {
                        WinPreReqs wp = new WinPreReqs("", "", "", "");
                        string ipaddress = "";


                        int returnCodeofTempHost = wp.SetHostIPinputhostname(masterHost.hostname,  ipaddress, cxip);
                        ipaddress = WinPreReqs.GetIPaddress;
                        if (returnCodeofTempHost == 0)
                        {
                            masterHost.ip = ipaddress;

                            masterHost.numberOfEntriesInCX = 1;
                            ///checking vCon box fx licensed or not
                            if (wp.GetDetailsFromcxcli( masterHost, cxip) == true)
                            {
                                masterHost = WinPreReqs.GetHost;
                                if (masterHost.inmage_hostid != null)
                                {
                                    Trace.WriteLine(DateTime.Now + "\t Successfully got the masterHost uuid");
                                }
                                else
                                {
                                    Trace.WriteLine(DateTime.Now + "\t Failed to get the masterHost uuid");
                                }
                            }
                        }
                    }
                    param = new Parameter();
                    param.Name = "SourceHostId";
                    param.Value = masterHost.inmage_hostid;
                    paraGroup.addChildObj(param);
                }
                if (operationType == Com.Inmage.Esxcalls.OperationType.Recover || operationType == Com.Inmage.Esxcalls.OperationType.Drdrill || operationType == Com.Inmage.Esxcalls.OperationType.Removedisk)
                {
                    param = new Parameter();
                    param.Name = "SourceHostId";
                    param.Value = masterHost.inmage_hostid;
                    paraGroup.addChildObj(param);
                }
                else
                {
                    param = new Parameter();
                    param.Name = "TargetHostId";
                    param.Value = masterHost.inmage_hostid;
                    paraGroup.addChildObj(param);
                }


                param = new Parameter();
                param.Name = "SourceDir";
                param.Value = masterHost.srcDir;
                paraGroup.addChildObj(param);


                param = new Parameter();
                param.Name = "TargetDir";                
                param.Value = masterHost.tardir;      
                paraGroup.addChildObj(param);


                param = new Parameter();
                param.Name = "ExcludeFiles";
                param.Value = "*";
                paraGroup.addChildObj(param);


                if (operationType == Com.Inmage.Esxcalls.OperationType.Recover )
                {
                    param = new Parameter();
                    param.Name = "IncludeFiles";
                    param.Value = "";
                    paraGroup.addChildObj(param);
                }
                else if (operationType == Com.Inmage.Esxcalls.OperationType.Drdrill)
                {
                    param = new Parameter();
                    param.Name = "IncludeFiles";
                    param.Value = "";
                    paraGroup.addChildObj(param);
                }
                
                param = new Parameter();
                param.Name = "TransferMode";
                param.Value = "Push";
                paraGroup.addChildObj(param);
                if (operationType == Com.Inmage.Esxcalls.OperationType.Recover || operationType == Com.Inmage.Esxcalls.OperationType.Drdrill || operationType == Com.Inmage.Esxcalls.OperationType.Removedisk)
                {
                    param = new Parameter();
                    param.Name = "SourcePreScript";
                    param.Value = masterHost.commandtoExecute;
                    paraGroup.addChildObj(param);
                }
                
                if (operationType == Com.Inmage.Esxcalls.OperationType.Recover || operationType == Com.Inmage.Esxcalls.OperationType.Drdrill || operationType == Com.Inmage.Esxcalls.OperationType.Removedisk)
                {
                    if (masterHost.perlCommand != null)
                    {
                        param = new Parameter();
                        param.Name = "TargetPostScript";
                        param.Value = masterHost.perlCommand;
                        paraGroup.addChildObj(param);
                    }

                }
                else
                {
                    param = new Parameter();
                    param.Name = "TargetPostScript";
                    param.Value = masterHost.commandtoExecute;
                    paraGroup.addChildObj(param);

                }
                functionRequest.addChildObj(paraGroup);
                Trace.WriteLine(DateTime.Now + "\t xml " + functionRequest.ToXML());
                PostFXJobRequest(functionRequest,  masterHost);
                masterHost = GetHost;
                if (masterHost.planid == null)
                {
                    return false;
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                System.FormatException nht = new FormatException("Inner exception", ex);
                Trace.WriteLine(nht.GetBaseException());
                Trace.WriteLine(nht.Data);
                Trace.WriteLine(nht.InnerException);
                Trace.WriteLine(nht.Message);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            refHost = masterHost;
            return true;
        }

        internal Response PostFXJobRequest(FunctionRequest functionRequest, Host host)
        {
            Response response = new Response();
            ArrayList functionList = new ArrayList();

            try
            {
                List<Parameter> funcParameterGroups = new List<Parameter>();
                functionList.Add(functionRequest);
                response = PostRequest(functionList);
                string message = response.Message;
                foreach (object obj1 in response.Body.ChildList)
                {
                    if (obj1 is Function)
                    {
                        Function func = (Function)obj1;
                        if (String.Compare(func.Name, functionRequest.Name, true) != 0 || func.FunctionResponse == null)
                            continue;
                        Trace.WriteLine(DateTime.Now + "\t Count of parametergroup " + func.FunctionResponse.ChildList.Count.ToString());

                        foreach (object obj2 in func.FunctionResponse.ChildList)
                        {
                            if (obj2 is Parameter)
                            {
                                Parameter para = obj2 as Parameter;
                                if (para.getKey().ToUpper() == "PLANID")
                                {
                                  
                                    host.planid = para.Value;
                                    Trace.WriteLine(DateTime.Now + "\t Plan id " + host.planid);
                                    refHost = host;
                                    return response;
                                }
                            }
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed to get the planid " + ex.Message);
            }
            refHost = host;
            return response;
        }

        internal bool PostRequestForRemoveDataBaseapi(FunctionRequest functionRequest, Host host)
        {
            Response response = new Response();
            ArrayList functionList = new ArrayList();

            try
            {
                List<ParameterGroup> funcParameterGroups = new List<ParameterGroup>();
                functionList.Add(functionRequest);
                response = PostRequest(functionList);
                string message = response.Message;
                Trace.WriteLine(DateTime.Now + "\t Printing the response of remove execution step " + response.ToXML());
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed to remove the plan " + ex.Message);
            }
            refHost = host;
            return true;
        }

        internal bool PostRequestForMonitorapi(FunctionRequest functionRequest, Host host)
        {
            Response response = new Response();
            ArrayList functionList = new ArrayList();

            try
            {
                List<ParameterGroup> funcParameterGroups = new List<ParameterGroup>();
                functionList.Add(functionRequest);
                response = PostRequest(functionList);
                string message = response.Message;
                foreach (object obj1 in response.Body.ChildList)
                {
                    if (obj1 is Function)
                    {
                        Function func = (Function)obj1;
                        if (String.Compare(func.Name, functionRequest.Name, true) != 0 || func.FunctionResponse == null)
                            continue;
                        Trace.WriteLine(DateTime.Now + "\t Count of parametergroup " + func.FunctionResponse.ChildList.Count.ToString());
                        foreach (object obj2 in func.FunctionResponse.ChildList)
                        {
                        
                            if (obj2 is ParameterGroup)
                            {

                                ParameterGroup parameterGroup = obj2 as ParameterGroup;
                                funcParameterGroups.Add(parameterGroup);
                                Trace.WriteLine(DateTime.Now + "\t parameterGroup " + parameterGroup.ToXML());
                                foreach (ParameterGroup group in funcParameterGroups)
                                {
                                    foreach (object obj3 in group.ChildList)
                                    {
                                        ParameterGroup childOfSteps = obj3 as ParameterGroup;
                                        if (childOfSteps is ParameterGroup)
                                        {
                                            List<ParameterGroup> listParameterGroup = new List<ParameterGroup>();
                                            listParameterGroup.Add(childOfSteps);
                                            foreach (ParameterGroup childDiskListgroup in listParameterGroup)
                                            {
                                                Trace.WriteLine(DateTime.Now + "\t Printing the id value " + childDiskListgroup.Id.ToString());
                                                if (childDiskListgroup.Id != null && childDiskListgroup.Id.ToString() == "TaskList")
                                                {
                                                    Trace.WriteLine(DateTime.Now + "\t Printing the count of the TaskList" + childOfSteps.ChildList.Count.ToString());
                                                    ReadFunctionGroupForTaskList(listParameterGroup,  host);
                                                    host = GetHost;
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            catch (Exception ex)
            {

            }
            refHost = host;
            return true;
        }


        internal static bool ReadFunctionGroupForPlans(List<ParameterGroup> funcParameterGroups, ArrayList list)
        {
            try
            {
                foreach (ParameterGroup group in funcParameterGroups)
                {
                    Hashtable planHash = new Hashtable();
                    if (group.Id.ToString().Contains("Plan"))
                    {
                        Trace.WriteLine(DateTime.Now + "\t Printing the group id " + group.Id.ToString());
                        foreach (object planList in group.ChildList)
                        {
                            if (planList is Parameter)
                            {



                                Parameter paraMeterTasks = planList as Parameter;
                                if (paraMeterTasks.Name == "PlanName")
                                {

                                    planHash.Add("PlanName", paraMeterTasks.Value);
                                    Trace.WriteLine(DateTime.Now + "\t PlanName ", paraMeterTasks.Value);

                                }
                                if (paraMeterTasks.Name == "PlanId")
                                {
                                    if (paraMeterTasks.Value.Length != 0)
                                    {
                                        planHash.Add("PlanId", paraMeterTasks.Value);
                                        Trace.WriteLine(DateTime.Now + "\t PlanId ", paraMeterTasks.Value);
                                    }
                                }
                                if (paraMeterTasks.Name == "PlanType")
                                {
                                    if (paraMeterTasks.Value.Length != 0)
                                    {
                                        planHash.Add("PlanType", paraMeterTasks.Value);
                                        Trace.WriteLine(DateTime.Now + "\t PlanType ", paraMeterTasks.Value);
                                    }
                                }


                            }
                        }
                        list.Add(planHash);

                    }                    
                }
            }
            catch (Exception ex)
            {

            }
            refArraylist = list;
            return true;
        }
        internal static bool ReadFunctionGroupForTaskList(List<ParameterGroup> funcParameterGroups, Host host)
        {
            try
            {
                foreach (ParameterGroup group in funcParameterGroups)
                {

                    if (group.Id.ToString() == "TaskList")
                    {
                        Trace.WriteLine(DateTime.Now + "\t Printing the group id " + group.Id.ToString());
                        foreach (object diskList in group.ChildList)
                        {
                            if (diskList is ParameterGroup)
                            {                                
                                ParameterGroup childDisks = diskList as ParameterGroup;

                                List<ParameterGroup> listParameterGroup = new List<ParameterGroup>();
                                listParameterGroup.Add(childDisks);
                                host.taskHash = new Hashtable();
                                foreach (ParameterGroup childDiskListgroup in listParameterGroup)
                                {
                                    Trace.WriteLine(DateTime.Now + "\t Parameter grp id " + childDiskListgroup.Id.ToString());
                                    
                                    foreach (object taskObj in childDiskListgroup.ChildList)
                                    {

                                        if (taskObj is Parameter)
                                        {

                                            Parameter paraMeterTasks = taskObj as Parameter;
                                            if (paraMeterTasks.Name == "TaskName")
                                            {

                                                host.taskHash.Add("Name", paraMeterTasks.Value);
                                                Trace.WriteLine(DateTime.Now + "\t Name ", paraMeterTasks.Value);

                                            }
                                            if (paraMeterTasks.Name == "Description")
                                            {
                                                if (paraMeterTasks.Value.Length != 0)
                                                {
                                                    host.taskHash.Add("Description", paraMeterTasks.Value);
                                                    Trace.WriteLine(DateTime.Now + "\t Description ", paraMeterTasks.Value);
                                                }
                                            }
                                            if (paraMeterTasks.Name == "TaskStatus")
                                            {
                                                if (paraMeterTasks.Value.Length != 0)
                                                {
                                                    host.taskHash.Add("TaskStatus", paraMeterTasks.Value);
                                                    Trace.WriteLine(DateTime.Now + "\t TaskStatus ", paraMeterTasks.Value);
                                                }
                                            }
                                            if (paraMeterTasks.Name == "ErrorMessage")
                                            {
                                                if (paraMeterTasks.Value.Length != 0)
                                                {

                                                    host.taskHash.Add("ErrorMessage", paraMeterTasks.Value);
                                                    Trace.WriteLine(DateTime.Now + "\t ErrorMessage ", paraMeterTasks.Value);
                                                }
                                            }
                                            if (paraMeterTasks.Name == "FixSteps")
                                            {
                                                if (paraMeterTasks.Value.Length != 0)
                                                {
                                                    host.taskHash.Add("FixSteps", paraMeterTasks.Value);
                                                    Trace.WriteLine(DateTime.Now + "\t Fxi steps ", paraMeterTasks.Value);
                                                }
                                            }
                                            if (paraMeterTasks.Name.ToUpper() == "LOGPATH")
                                            {
                                                if (paraMeterTasks.Value.Length != 0)
                                                {
                                                    host.taskHash.Add("LogPath", paraMeterTasks.Value);
                                                    Trace.WriteLine(DateTime.Now + "\t Logpath " + paraMeterTasks.Value);
                                                }
                                            }
                                            
                                        }
                                    }
                                    host.taskList.Add(host.taskHash);
                                }
                            }
                        }
                    }
                }
            }
            catch (Exception ex)
            {

            }
            refHost = host;
            return true;
        }

        internal bool GetRequestStatusForAllHosts(FunctionRequest functionRequest)
        {
            Response response = new Response();
            ArrayList functionList = new ArrayList();

            try
            {
                List<ParameterGroup> funcParameterGroups = new List<ParameterGroup>();
                functionList.Add(functionRequest);
                response = PostRequest(functionList);
                string message = response.Message;
                foreach (object obj1 in response.Body.ChildList)
                {
                    if (obj1 is Function)
                    {
                        Function func = (Function)obj1;
                        if (String.Compare(func.Name, functionRequest.Name, true) != 0 || func.FunctionResponse == null)
                            continue;
                        Trace.WriteLine(DateTime.Now + "\t Count of parametergroup " + func.FunctionResponse.ChildList.Count.ToString());

                        foreach (object obj2 in func.FunctionResponse.ChildList)
                        {
                            if (obj2 is Parameter)
                            {
                                Parameter para = obj2 as Parameter;
                                if (para.getKey().ToUpper() == "STATUSOFALLHOSTS")
                                {
                                    if (para.Value.ToString().ToUpper() == "PENDING")
                                    {
                                        Trace.WriteLine(DateTime.Now + "\t Status is pending");
                                        return false;
                                    }
                                    else if (para.Value.ToString().ToUpper() == "INPROGRESS")
                                    {
                                        Trace.WriteLine(DateTime.Now + "\t Status is In progress");
                                        return false;
                                    }
                                    else
                                    {
                                        Trace.WriteLine(DateTime.Now + "\t Status is " + para.Value);
                                    }
                                }
                            }
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed to get the status of all reuest hosts " + ex.Message);
            }
            return true;
        }


        internal bool PostRequestForGetHostDetails(FunctionRequest functionRequest, HostList sourceList)
        {
            Response response = new Response();
            ArrayList functionList = new ArrayList();

            try
            {
                List<ParameterGroup> funcParameterGroups = new List<ParameterGroup>();
                functionList.Add(functionRequest);
                response = PostRequest(functionList);
                string message = response.Message;
                HostList tempSourceList = new HostList();

                foreach (object obj1 in response.Body.ChildList)
                {
                    if (obj1 is Function)
                    {
                        Function func = (Function)obj1;
                        if (String.Compare(func.Name, functionRequest.Name, true) != 0 || func.FunctionResponse == null)
                            continue;
                        Trace.WriteLine(DateTime.Now + "\t Count of parametergroup " + func.FunctionResponse.ChildList.Count.ToString());


                        foreach (object obj2 in func.FunctionResponse.ChildList)
                        {
                            if (obj2 is ParameterGroup)
                            {
                                ParameterGroup parameterGroup = obj2 as ParameterGroup;
                                funcParameterGroups.Add(parameterGroup);
                                Trace.WriteLine(DateTime.Now + "\t parameterGroup " + parameterGroup.ToXML());

                                if (parameterGroup.Id != null && parameterGroup.Id.ToString().ToUpper().Contains("HOST"))
                                {


                                    foreach (ParameterGroup group in funcParameterGroups)
                                    {
                                        Host h = new Host();
                                        if (group.Id.ToUpper().Contains("HOST"))
                                        {

                                            foreach (object hostlist in group.ChildList)
                                            {

                                                if (hostlist is Parameter)
                                                {

                                                    Parameter paraMeterHosts = hostlist as Parameter;
                                                    if (paraMeterHosts.getKey().ToUpper() == "HOSTNAME")
                                                    {
                                                        if (paraMeterHosts.Value.Length > 0)
                                                        {
                                                            h.hostname = paraMeterHosts.Value;
                                                        }
                                                    }
                                                    if (paraMeterHosts.getKey().ToUpper() == "IPLIST")
                                                    {
                                                        if (paraMeterHosts.Value.Length > 0)
                                                        {
                                                            h.ip = paraMeterHosts.Value;
                                                        }
                                                    }
                                                    if (paraMeterHosts.getKey().ToUpper() == "VXAGENTSTATUS")
                                                    {
                                                        if (paraMeterHosts.Value.Length > 0)
                                                        {
                                                            if (paraMeterHosts.Value.ToUpper() == "INSTALLED")
                                                            {
                                                                h.vxagentInstalled = true;
                                                            }
                                                            else
                                                            {
                                                                h.vxagentInstalled = false;
                                                            }
                                                        }
                                                    }
                                                    if (paraMeterHosts.getKey().ToUpper() == "FXAGENTSTATUS")
                                                    {
                                                        if (paraMeterHosts.Value.Length > 0)
                                                        {
                                                            if (paraMeterHosts.Value.ToUpper() == "INSTALLED")
                                                            {
                                                                h.fxagentInstalled = true;
                                                            }
                                                            else
                                                            {
                                                                h.fxagentInstalled = false;
                                                            }
                                                        }
                                                    }
                                                    if (paraMeterHosts.getKey().ToUpper() == "HOSTGUID")
                                                    {
                                                        if (paraMeterHosts.Value.Length > 0)
                                                        {
                                                            h.inmage_hostid = paraMeterHosts.Value;
                                                        }
                                                    }
                                                    if (paraMeterHosts.getKey().ToUpper() == "AGENTVERSION")
                                                    {
                                                        if (paraMeterHosts.Value.Length > 0)
                                                        {
                                                            h.unifiedagentVersion = paraMeterHosts.Value;
                                                        }
                                                    }
                                                }
                                            }

                                            tempSourceList.AddOrReplaceHost(h);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }


                foreach (Host h in sourceList._hostList)
                {
                    foreach (Host h1 in tempSourceList._hostList)
                    {
                        if (h.hostname.ToUpper().Split('.')[0] == h1.hostname.ToUpper())
                        {
                            h.inmage_hostid = h1.inmage_hostid;
                            h.fxagentInstalled = h1.fxagentInstalled;
                            h.vxagentInstalled = h1.vxagentInstalled;
                            h.unifiedagentVersion = h1.unifiedagentVersion;
                        }
                    }
                }

            }
            catch (Exception ex)
            {
            }
            refHostList = sourceList;
            return true;
        }


        internal bool PostRequest(FunctionRequest functionRequest, Host host)
        {
            Response response = new Response();
            ArrayList functionList = new ArrayList();

            try
            {
                List<ParameterGroup> funcParameterGroups = new List<ParameterGroup>();
                functionList.Add(functionRequest);
                response = PostRequest(functionList);
                string message = response.Message;
            foreach (object obj1 in response.Body.ChildList)
                {
                    if (obj1 is Function)
                    {
                        Function func = (Function)obj1;
                        if (String.Compare(func.Name, functionRequest.Name, true) != 0 || func.FunctionResponse == null)
                            continue;
                        Trace.WriteLine(DateTime.Now + "\t Count of parametergroup " + func.FunctionResponse.ChildList.Count.ToString());

                        foreach (object obj2 in func.FunctionResponse.ChildList)
                        {
                            if (obj2 is Parameter)
                            {
                                Parameter para = obj2 as Parameter;
                                if (para.getKey().ToUpper() == "STATUS")
                                {
                                    if (para.Value.ToString().ToUpper() == "PENDING")
                                    {
                                        Trace.WriteLine(DateTime.Now + "\t Status is pending");
                                        return false;
                                    }
                                    else if (para.Value.ToString().ToUpper() == "INPROGRESS")
                                    {
                                        Trace.WriteLine(DateTime.Now + "\t Status is In progress");
                                        return false;
                                    }
                                    else
                                    {                                       
                                        Trace.WriteLine(DateTime.Now + "\t Status is " + para.Value);
                                    }
                                }
                            }
                        }
                    }
                }
                foreach (object obj1 in response.Body.ChildList)
                {
                    if (obj1 is Function)
                    {
                        Function func = (Function)obj1;
                        if (String.Compare(func.Name, functionRequest.Name, true) != 0 || func.FunctionResponse == null)
                            continue;
                        Trace.WriteLine(DateTime.Now + "\t Count of parametergroup " + func.FunctionResponse.ChildList.Count.ToString());

                        foreach (object obj2 in func.FunctionResponse.ChildList)
                        {
                            if (obj2 is Parameter)
                            {
                                Parameter para = obj2 as Parameter;

                                if (para.getKey().ToUpper() == "MEMSIZE")
                                {
                                    long memsize = long.Parse(para.Value.ToString());
                                    memsize = memsize / (1024 * 1024);
                                    Trace.WriteLine(DateTime.Now + "\t Memsize " + memsize);
                                    host.memSize = int.Parse(memsize.ToString());
                                }
                                if (para.getKey().ToUpper() == "CPUCOUNT")
                                {
                                    string cpuCount = para.Value;
                                    Trace.WriteLine(DateTime.Now + "\t Cpu Count " + cpuCount);
                                    host.cpuCount = int.Parse(cpuCount.ToString());
                                }
                                if (para.getKey().ToUpper() == "HOSTNAME")
                                {
                                    host.hostname = para.Value;
                                    host.displayname = para.Value;
                                    Trace.WriteLine(DateTime.Now + "\t Printing hostname " + host.hostname);
                                }
                                if (para.getKey().ToUpper() == "HOSTGUID")
                                {
                                    host.inmage_hostid = para.Value;
                                    Trace.WriteLine(DateTime.Now + "\t Printing host id " + host.inmage_hostid);
                                }
                                if (para.getKey().ToUpper() == "MACHINETYPE")
                                {
                                    host.machineType = para.Value;
                                    Trace.WriteLine(DateTime.Now + "\t Printing Machine type value " + host.machineType);
                                }
                                if (para.getKey().ToUpper() == "CAPTION")
                                {
                                    host.operatingSystem = para.Value;
                                    Trace.WriteLine(DateTime.Now + "\t Printing os with out version " + host.operatingSystem);
                                }
                                if (para.getKey().ToUpper() == "CPUARCHITECTURE")
                                {
                                    host.cpuVersion = para.Value;
                                }
                                if (para.getKey().ToUpper() == "SYSTEMTYPE")
                                {
                                    host.osEdition = para.Value;
                                    Trace.WriteLine(DateTime.Now + "\t Os edition (system type) " + host.osEdition);
                                }
                                if (para.getKey().ToUpper() == "NUMBEROFDISKS")
                                {
                                    host.number_of_disks = int.Parse(para.Value.ToString());
                                    Trace.WriteLine(DateTime.Now + "\t Number of disks " + host.number_of_disks.ToString());
                                }
                                if (para.getKey().ToUpper() == "CLUSTER")
                                {
                                    bool isCluster = bool.Parse(para.Value);
                                    if (isCluster == false)
                                    {
                                        host.cluster = "no";
                                    }
                                    else
                                    {
                                        host.cluster = "yes";
                                    }
                                }
                                if (para.getKey().ToUpper() == "MBRPATH")
                                {
                                    if (para.Value.Length != 0)
                                    {
                                        host.mbr_path = para.Value;
                                        Trace.WriteLine(DateTime.Now + "\t MBR path " + host.mbr_path);
                                    }
                                    else
                                    {
                                        host.mbr_path = null;
                                        Trace.WriteLine(DateTime.Now + "\t Got the mbr value is null from cx ");
                                    }
                                }

                                if (para.getKey().ToUpper() == "CLUSTERNAME")
                                {
                                    if (para.Value.Length != 0)
                                    {
                                        host.clusterName = para.Value;
                                        Trace.WriteLine(DateTime.Now + "\t clusterName " + host.clusterName);
                                    }
                                    else
                                    {
                                        host.clusterName = null;
                                        Trace.WriteLine(DateTime.Now + "\t Got the clusterName value is null from cx ");
                                    }
                                }

                                if (para.getKey().ToUpper() == "CLUSTERNODESINMAGEUUID")
                                {
                                    if (para.Value.Length != 0)
                                    {
                                        host.clusterInmageUUIds = para.Value;
                                        Trace.WriteLine(DateTime.Now + "\t clusterInmageUUIds " + host.clusterInmageUUIds);
                                    }
                                    else
                                    {
                                        host.clusterInmageUUIds = null;
                                        Trace.WriteLine(DateTime.Now + "\t Got the clusterInmageUUIds value is null from cx ");
                                    }
                                }

                                if (para.getKey().ToUpper() == "CLUSTERNODES")
                                {
                                    if (para.Value.Length != 0)
                                    {
                                        host.clusterNodes = para.Value;
                                        Trace.WriteLine(DateTime.Now + "\t clusterNodes " + host.clusterNodes);
                                    }
                                    else
                                    {
                                        host.clusterNodes = null;
                                        Trace.WriteLine(DateTime.Now + "\t Got the clusterNodes value is null from cx ");
                                    }
                                }

                               
                                if (para.getKey().ToUpper() == "CLUSTERNODESMBRFILES")
                                {
                                    if (para.Value.Length != 0)
                                    {
                                        host.clusterMBRFiles = para.Value;
                                        Trace.WriteLine(DateTime.Now + "\t clusterMBRFiles " + host.clusterMBRFiles);
                                    }
                                    else
                                    {
                                        host.clusterMBRFiles = null;
                                        Trace.WriteLine(DateTime.Now + "\t Got the clusterMBRFiles value is null from cx ");
                                    }
                                }


                            }
                            if (obj2 is ParameterGroup)
                            {

                                ParameterGroup parameterGroup = obj2 as ParameterGroup;
                                funcParameterGroups.Add(parameterGroup);
                                Trace.WriteLine(DateTime.Now + "\t parameterGroup " + parameterGroup.ToXML());
                                if (parameterGroup.Id != null && parameterGroup.Id.ToString().ToUpper() == "DISKLIST")
                                {
                                    host.disks = new DiskList();
                                    ReadFunctionGroupForDisks(funcParameterGroups,  host);
                                    host = GetHost;
                                    Trace.WriteLine(DateTime.Now + "\t Printing the count of disks " + host.disks.list.Count.ToString());
                                    ArrayList disklist = new ArrayList();
                                    ArrayList tempList = new ArrayList();
                                    disklist = host.disks.list;

                                    int i = 0;
                                    foreach (Hashtable hash in disklist)
                                    {

                                        if (hash["Name"] != null)
                                        {
                                            if (!hash["src_devicename"].ToString().Contains("mapper") && !hash["src_devicename"].ToString().Contains("emcpower"))
                                            {
                                                tempList.Add(hash);
                                                disklist.Remove(i);
                                            }
                                        }
                                        i++;
                                    }
                                    Trace.WriteLine(DateTime.Now + "\t later Printing the count disks " + tempList.Count.ToString());

                                    foreach (Hashtable hash in disklist)
                                    {
                                        if (hash["src_devicename"].ToString().Contains("mapper") || hash["src_devicename"].ToString().Contains("emcpower"))
                                        {
                                            tempList.Add(hash);
                                        }
                                    }


                                    host.disks.list = tempList;

                                    Trace.WriteLine(DateTime.Now + "\t later Printing the count disks " + host.disks.list.Count.ToString());
                                }
                                else
                                {
                                    host.nicList.list = new ArrayList(); 
                                    ReadFunctionGroupForNics(funcParameterGroups,  host);
                                    host = GetHost;
                                }

                            }
                        }
                    }
                }
                if (host.os == OStype.Windows)
                {
                    Trace.WriteLine(DateTime.Now + "\t Came here to sort the disks");
                    IComparer myComparer = new MyDiskOrderCompare();

                    host.disks.list.Sort(myComparer);
                }
                Trace.WriteLine(DateTime.Now + "\t Disk count " + host.disks.list.Count.ToString());
               
                for (int k = 0; k < host.disks.list.Count; k++)
                {
                    if (host.os == OStype.Windows)
                    {
                        ((Hashtable)host.disks.list[k])["Name"] = "[DATASTORE_NAME] " + host.hostname + "/" + ((Hashtable)host.disks.list[k])["Name"] + ".vmdk";
                        Trace.WriteLine(DateTime.Now + "\t DIskname " + ((Hashtable)host.disks.list[k])["Name"].ToString());
                    }
                    else if (host.os == OStype.Linux)
                    {
                        ((Hashtable)host.disks.list[k])["Name"] = ((Hashtable)host.disks.list[k])["Name"].ToString().Replace("/","_");
                        if (((Hashtable)host.disks.list[k])["Name"].ToString().StartsWith("_"))
                        {
                            ((Hashtable)host.disks.list[k])["Name"] = ((Hashtable)host.disks.list[k])["Name"].ToString().Remove(0,1);
                        }
                        ((Hashtable)host.disks.list[k])["Name"] = "[DATASTORE_NAME] " + host.hostname + "/"+   ((Hashtable)host.disks.list[k])["Name"] + ".vmdk";
                        Trace.WriteLine(DateTime.Now + "\t DIskname " + ((Hashtable)host.disks.list[k])["Name"].ToString());
                    }

                }
                host.ide_count = "1";
                host.floppy_device_count = "0";
                host.folder_path = host.hostname + "/";
                host.snapshot = "SNAPFALSE";
                host.vmxversion = "vmx-07";
                host.resourcePool = "dummy";
                host.vmx_path = "[DATASTORE_NAME] " + host.folder_path + host.hostname + ".vmx";
                Trace.WriteLine(DateTime.Now + "\t Printing the disk count " + host.disks.list.Count.ToString());
                Trace.WriteLine(DateTime.Now + "\t Printing the nic count " + host.nicList.list.Count.ToString());
                RemoteWin remote = new RemoteWin("", "", "", "", "");
                if (host.os == OStype.Windows)
                {
                    if (host.operatingSystem != null)
                    {
                        host.operatingSystem = RemoteWin.GetOSArchitectureFromWmiCaptionString(host.operatingSystem, host.osEdition, host.alt_guest_name);
                        host.alt_guest_name = remote.GetOSediton;
                        Trace.WriteLine(DateTime.Now + "\t Operating sysytem " + host.operatingSystem);
                        Trace.WriteLine(DateTime.Now + "\t Os edition (system type) " + host.osEdition);
                    }
                    else
                    {
                        Trace.WriteLine(DateTime.Now + "\t Got os names as null");
                    }
                }
                else if (host.os == OStype.Linux)
                {
                    if (host.operatingSystem != null)
                    {
                        RemoteWin.GetOSArchitectureFromWmiCaptionStringForLinux(host.operatingSystem, host.osEdition, host.alt_guest_name);
                        host.alt_guest_name = remote.GetOSediton;
                        // h.operatingSystem = h.operatingSystem ;
                    }
                    else
                    {
                        Trace.WriteLine(DateTime.Now + "\t Got os names as null");
                    }
                }
               // h.operatingSystem = h.operatingSystem + " " + h.cpuVersion;
                Trace.WriteLine(DateTime.Now + "\t alt guest id " + host.alt_guest_name);

            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

            }
            refHost = host;
            return true;
 
        }

        public bool RecoveryReadinessChecks(Host host, Host masterHost)
        {

            try
            {
                string cxip = WinTools.GetcxIPWithPortNumber();
              
                if (host.inmage_hostid == null)
                {
                    WinPreReqs win = new WinPreReqs("","","","");
                    win.GetDetailsFromcxcli( host, cxip);
                    host = WinPreReqs.GetHost;

                }
                if (masterHost.inmage_hostid == null)
                {
                    WinPreReqs win = new WinPreReqs("", "", "", "");
                    win.GetDetailsFromcxcli( masterHost, cxip);
                    masterHost = WinPreReqs.GetHost;
                }

                Response response = new Response();
                ArrayList list = new ArrayList();

                Parameter sourceId = new Parameter();

                Parameter targetId = new Parameter();
                Parameter tagName = new Parameter();
                Parameter option = new Parameter();
                Parameter time = new Parameter();
                ParameterGroup paraGroup = new ParameterGroup();

                FunctionRequest functionRequest = new FunctionRequest();
                if (host.recoveryOperation == 5)
                {
                    paraGroup.Id = "VerifyTag";
                }
                else
                {
                    paraGroup.Id = "GetCommonRecoveryPoint";
                }
                sourceId.Name = "SourceHostID";
                sourceId.Value = host.inmage_hostid;
                functionRequest.addChildObj(sourceId);
                targetId.Name = "TargetHostID";
                targetId.Value = masterHost.inmage_hostid;
                functionRequest.addChildObj(targetId);
                if (host.recoveryOperation == 5)
                {
                    tagName.Name = "TagName";
                    tagName.Value = host.tag;
                    functionRequest.addChildObj(tagName);
                }
                option.Name = "Option";
                if (host.recoveryOperation == 1)
                {
                    option.Value = "LATEST_TAG";
                    functionRequest.addChildObj(option);
                }
                else if (host.recoveryOperation == 2)
                {
                    option.Value = "LATEST_TIME";
                    functionRequest.addChildObj(option);
                }
                else if (host.recoveryOperation == 3)
                {
                    option.Value = "TAG_AT_SPECIFIC_TIME";
                    functionRequest.addChildObj(option);
                    string userDefinedTime = host.userDefinedTime;
                    DateTime gmt = DateTime.Parse(userDefinedTime);
                    //userDefinedTime = gmt.ToString("dd/MM/yyyy HH:mm:ss");
                    Trace.WriteLine(DateTime.Now + "\t Passing this time to cx api " + userDefinedTime);
                    // gmt = DateTime.Parse(userDefinedTime);
                    long baseTicks = 621355968000000000;
                    long tickResolution = 10000000;
                    long epoch;
                    epoch = (gmt.Ticks - baseTicks) / tickResolution;

                    time.Name = "Time";
                    time.Value = epoch.ToString();
                    functionRequest.addChildObj(time);
                }
                else if (host.recoveryOperation == 4)
                {
                    option.Value = "SPECIFIC_TIME";
                    functionRequest.addChildObj(option);
                    string userDefinedTime = host.userDefinedTime;
                    DateTime gmt = DateTime.Parse(userDefinedTime);
                    //userDefinedTime = gmt.ToString("dd/MM/yyyy HH:mm:ss");
                    Trace.WriteLine(DateTime.Now + "\t Passing this time to cx api " + userDefinedTime);
                    // gmt = DateTime.Parse(userDefinedTime);
                    long baseTicks = 621355968000000000;
                    long tickResolution = 10000000;
                    long epoch;
                    epoch = (gmt.Ticks - baseTicks) / tickResolution;

                    time.Name = "Time";
                    time.Value = epoch.ToString();
                    functionRequest.addChildObj(time);
                }


                Trace.WriteLine(DateTime.Now + "\t Paramaer group " + paraGroup.ToXML());
                if (host.recoveryOperation == 5)
                {
                    functionRequest.Name = "VerifyTag";
                }
                else
                {
                    
                    functionRequest.Name = "GetCommonRecoveryPoint";
                }
                //functionRequest.Name = "GetHostMBRPath";

                Trace.WriteLine(DateTime.Now + "\t Function request " + functionRequest.ToXML());
                response = PostReadinessRequest(functionRequest,  host);
                host = GetHost;
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                System.FormatException nht = new FormatException("Inner exception", ex);
                Trace.WriteLine(nht.GetBaseException());
                Trace.WriteLine(nht.Data);
                Trace.WriteLine(nht.InnerException);
                Trace.WriteLine(nht.Message);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            refHost = host;
            return true;
        }


        public bool PostRequestStatusForDummyDisk(Host host, bool logRequired)
        {
            try
            {
                Response response = new Response();
                ArrayList list = new ArrayList();
                Parameter param = new Parameter();
                ParameterGroup paraGroup = new ParameterGroup();
                FunctionRequest functionRequest = new FunctionRequest();



                paraGroup.Id = "RequestIdList";
                param.Name = "RequestId";
                param.Value = host.requestId;
                paraGroup.ChildList.Add(param);
                functionRequest.addChildObj(paraGroup);

                functionRequest.Name = "GetRequestStatus";
                Trace.WriteLine(DateTime.Now + "\t paragroup " + functionRequest.ToXML());
                //functionRequest.Name = "GetHostMBRPath";
                FunctionRequest fun = new FunctionRequest();
                //fun.Name = "GetHostMBRPath";

                if (PostRequestOfDummyDisk(functionRequest,  host, logRequired) == true)
                {
                    host = GetHost;
                    refHost = host;
                    return true;
                }
                else
                {
                    host = GetHost;
                    refHost = host;
                    return false;
                }

            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed to post request status for the master target " + ex.Message);
                return false;
            }
            
        }



        public bool PostToGetAllPlans(ArrayList planList)
        {
            Response response = new Response();
            ArrayList list = new ArrayList();
            FunctionRequest functionRequest = new FunctionRequest();
            functionRequest.Name = "GetConfiguredPlans";
            PostRequestToGetPlans(functionRequest,  planList);
            planList = refArraylist;
            refArraylist = planList;
            return true;
        }

        internal bool PostRequestToGetPlans(FunctionRequest functionRequest, ArrayList list)
        {
            Response response = new Response();
            ArrayList functionList = new ArrayList();

            try
            {
                List<ParameterGroup> funcParameterGroups = new List<ParameterGroup>();
                functionList.Add(functionRequest);
                response = PostRequest(functionList);
                Trace.WriteLine(DateTime.Now + "\t Printing output of plans " + response.ToXML());
                string message = response.Message;
                foreach (object obj1 in response.Body.ChildList)
                {
                    if (obj1 is Function)
                    {
                        Function func = (Function)obj1;
                        if (String.Compare(func.Name, functionRequest.Name, true) != 0 || func.FunctionResponse == null)
                            continue;
                        Trace.WriteLine(DateTime.Now + "\t Count of parametergroup " + func.FunctionResponse.ChildList.Count.ToString());
                        foreach (object obj2 in func.FunctionResponse.ChildList)
                        {
                            if (obj2 is ParameterGroup)
                            {
                                ParameterGroup parameterGroup = obj2 as ParameterGroup;
                                List<ParameterGroup> listParameterGroup = new List<ParameterGroup>();
                                listParameterGroup.Add(parameterGroup);
                                ReadFunctionGroupForPlans(listParameterGroup,  list);
                                list = refArraylist;
                               
                            }
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
                Trace.WriteLine("Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
            refArraylist = list;
            return true;
        }


        public bool PostForRefreshAllHosts(HostList selectedHost)
        {
            Response response = new Response();
            ArrayList list = new ArrayList();
            Parameter param = new Parameter();
            ParameterGroup paraGroup = new ParameterGroup();
            FunctionRequest functionRequest = new FunctionRequest();
            int i = 1;
            foreach (Host h in selectedHost._hostList)
            {
                if (h.inmage_hostid != null)
                {
                    param = new Parameter();
                    param.Name = "HostGUID" + i.ToString();
                    param.Value = h.inmage_hostid;
                    functionRequest.addChildObj(param);
                    i++;
                }
            }
            functionRequest.Name = "RefreshHostInfo";
            Host sourceHost = (Host)selectedHost._hostList[0];
            response = PostRequestForRefreshHost(functionRequest,  sourceHost);
            sourceHost = GetHost;
            if (sourceHost.requestId != null)
            {
                foreach (Host h1 in selectedHost._hostList)
                {
                    h1.requestId = sourceHost.requestId;
                }
            }
            else
            {
                Trace.WriteLine(DateTime.Now + "\t Found the request id as null for refreshing all hosts at a time ");
                return false;
            }
            refHostList = selectedHost;
            return true;
        }

        public bool PostRefreshWithmbrOnly(Host host)
        {
            Response response = new Response();
            ArrayList list = new ArrayList();
            Parameter param = new Parameter();
            ParameterGroup paraGroup = new ParameterGroup();
         
            FunctionRequest functionRequest = new FunctionRequest();

            paraGroup.Id = "Host1";
            param.Name = "HostGUID";
            param.Value = host.inmage_hostid;
            paraGroup.addChildObj(param);
            param = new Parameter();
            param.Name = "Option";
            param.Value = "dlm_mbronly";
            paraGroup.addChildObj(param);
            functionRequest.addChildObj(paraGroup);

            functionRequest.Name = "RefreshHostInfo";
            Trace.WriteLine(DateTime.Now + "\t paragroup " + functionRequest.ToXML());
            response = PostRequestForRefreshHost(functionRequest,  host);
            host = GetHost;


            refHost = host;
            return true;
        }
        public bool PostRefreshWithall(Host host)
        {
            Response response = new Response();
            ArrayList list = new ArrayList();
            Parameter param = new Parameter();
            ParameterGroup paraGroup = new ParameterGroup();

            FunctionRequest functionRequest = new FunctionRequest();

            paraGroup.Id = "Host1";
            param.Name = "HostGUID";
            param.Value = host.inmage_hostid;
            paraGroup.addChildObj(param);
            param = new Parameter();
            param.Name = "Option";
            param.Value = "dlm_all";
            paraGroup.addChildObj(param);
            functionRequest.addChildObj(paraGroup);

            functionRequest.Name = "RefreshHostInfo";
            Trace.WriteLine(DateTime.Now + "\t paragroup " + functionRequest.ToXML());
            response = PostRequestForRefreshHost(functionRequest,  host);
            host = refHost;
            refHost = host;

            return true;
        }

        public bool PostToGetHostDetails(HostList sourceList)
        {
            Response response = new Response();
            ArrayList list = new ArrayList();
            Parameter param = new Parameter();
            ParameterGroup paraGroup = new ParameterGroup();
            FunctionRequest functionRequest = new FunctionRequest();
            int i = 1;
            foreach (Host h in sourceList._hostList)
            {
                paraGroup = new ParameterGroup();
                param = new Parameter();
                paraGroup.Id = "Host" + i.ToString();
                param.Name = "HostName";
                string hostname = h.hostname.Split('.')[0];
                param.Value = hostname;
                paraGroup.addChildObj(param);
                param = new Parameter();
                param.Name = "IPList";
                string ip = null;
                if (h.IPlist != null)
                {
                    foreach (string s in h.IPlist)
                    {
                        if (ip != null)
                        {
                            ip = ip + "," + s;
                        }
                        else
                        {
                            ip = s;
                        }
                    }
                }
                else
                {
                    ip = h.ip;
                }
                param.Value = ip;
                paraGroup.addChildObj(param);
                functionRequest.addChildObj(paraGroup);
                i++;
            }
            functionRequest.Name = "GetHostAgentDetails";

            
            Trace.WriteLine(DateTime.Now + "\t paragroup " + functionRequest.ToXML());
            //functionRequest.Name = "GetHostMBRPath";
            FunctionRequest fun = new FunctionRequest();
            if (PostRequestForGetHostDetails(functionRequest,  sourceList) == true)
            {
                
                Trace.WriteLine(DateTime.Now + "\t Successfully got the response");
            }
            sourceList = refHostList;
            refHostList = sourceList;
            return true;
        }


        public bool PostToGetPairsOFSelectedMachine(Host host, string mtInmageuuid)
        {
            Response response = new Response();
            ArrayList list = new ArrayList();
            Parameter param = new Parameter();
            ParameterGroup paraGroup = new ParameterGroup();
            ParameterGroup paraGroupHost = new ParameterGroup();
            FunctionRequest functionRequest = new FunctionRequest();

            paraGroupHost.Id = "HostList_1";
            paraGroup.Id = "HostLists";
            param.Name = "SourceHostGUID";
            param.Value = host.inmage_hostid;
            paraGroupHost.addChildObj(param);
            //Now we have made master target uuid as an optional parameter. 
            //Becuase during unregister host we don't know mastar target uuid. So we made this change
            if (mtInmageuuid != null)
            {
                param = new Parameter();
                param.Name = "TargetHostGUID";
                param.Value = mtInmageuuid;
                paraGroupHost.addChildObj(param);
            }
            paraGroup.addChildObj(paraGroupHost);
            functionRequest.addChildObj(paraGroup);

            functionRequest.Name = "GetConfiguredPairs";
            Trace.WriteLine(DateTime.Now + "\t paragroup " + functionRequest.ToXML());
            //functionRequest.Name = "GetHostMBRPath";
            FunctionRequest fun = new FunctionRequest();

            PostRequestForGetPairs(functionRequest,  host);
            host = refHost;
            refHost = host;
            return true;
        }

        internal bool PostRequestForGetPairs(FunctionRequest functionRequest, Host host)
        {
            Response response = new Response();
            ArrayList functionList = new ArrayList();
            host.disks.partitionList = new ArrayList();
            try
            {
                List<ParameterGroup> funcParameterGroups = new List<ParameterGroup>();
                functionList.Add(functionRequest);
                response = PostRequest(functionList);
                string message = response.Message;
                foreach (object obj1 in response.Body.ChildList)
                {
                    if (obj1 is Function)
                    {
                        Function func = (Function)obj1;
                        if (String.Compare(func.Name, functionRequest.Name, true) != 0 || func.FunctionResponse == null)
                            continue;
                        Trace.WriteLine(DateTime.Now + "\t Count of parametergroup " + func.FunctionResponse.ChildList.Count.ToString());
                        foreach (object obj2 in func.FunctionResponse.ChildList)
                        {

                            if (obj2 is ParameterGroup)
                            {
                                 ParameterGroup parameterGroup = obj2 as ParameterGroup;
                                funcParameterGroups.Add(parameterGroup);
                                Trace.WriteLine(DateTime.Now + "\t parameterGroup " + parameterGroup.ToXML());
                                if (obj2 is ParameterGroup)
                                {
                                    ParameterGroup parameterGroups = obj2 as ParameterGroup;
                                    List<ParameterGroup> listParameterGroup = new List<ParameterGroup>();
                                    listParameterGroup.Add(parameterGroups);
                                    ReadFunctionGroupForPairsInfo(listParameterGroup,  host);
                                    host = GetHost;
                                }
                            }
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
                Trace.WriteLine("Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }

            refHost = host;
            return true;
        }



        internal static bool ReadFunctionGroupForPairsInfo(List<ParameterGroup> funcParameterGroups, Host host)
        {
            try
            {
                foreach (ParameterGroup group in funcParameterGroups)
                {
                    Hashtable planHash = new Hashtable();

                    Trace.WriteLine(DateTime.Now + "\t Printing the group id " + group.Id.ToString());
                    foreach (object planList in group.ChildList)
                    {
                        if (planList is ParameterGroup)
                        {
                            ParameterGroup parameterGroups = planList as ParameterGroup;
                            List<ParameterGroup> listParameterGroup = new List<ParameterGroup>();
                            listParameterGroup.Add(parameterGroups);
                            ReadFunctionGroupForSourceTargetVolume(listParameterGroup,  host);
                            host = GetHost;

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
                Trace.WriteLine("Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
            refHost = host;
            return true;
        }


        internal static bool ReadFunctionGroupForSourceTargetVolume(List<ParameterGroup> funcParameterGroups, Host host)
        {
            try
            {
              

                foreach (ParameterGroup group in funcParameterGroups)
                {
                    Hashtable planHash = new Hashtable();

                    Trace.WriteLine(DateTime.Now + "\t Printing the group id " + group.Id.ToString());
                    foreach (object planList in group.ChildList)
                    {
                        if (planList is Parameter)
                        {



                            Parameter paraMeterhostid = planList as Parameter;
                            if (paraMeterhostid.Name.ToUpper() == "SOURCEDEVICENAME")
                            {

                                planHash.Add("SourceVolume", paraMeterhostid.Value);
                                Trace.WriteLine(DateTime.Now + "\t Source Volume ", paraMeterhostid.Value);

                            }
                            if (paraMeterhostid.Name.ToUpper() == "DESTDEVICENAME")
                            {

                                planHash.Add("TargetVolume", paraMeterhostid.Value);
                                Trace.WriteLine(DateTime.Now + "\t target volume ", paraMeterhostid.Value);

                            }
                         


                        }
                    }
                    planHash.Add("remove", "false");
                    host.disks.partitionList.Add(planHash);

                    Trace.WriteLine(DateTime.Now + "\t Count of listt " + host.disks.partitionList.Count.ToString());


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
                Trace.WriteLine("Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
            refHost = host;
            return true;
        }

        public bool GetHostCredentials(string ip)
        {
            Response response = new Response();
            Parameter param = new Parameter();
            FunctionRequest request = new FunctionRequest();

            param.Name = "MachineId";
            param.Value = ip;
            request.addChildObj(param);
            request.Name = "GetHostCredentials";
            Trace.WriteLine(DateTime.Now + "\t Request xml " + request.ToXML().ToString());
            PostRequestforGetCredentials(request);
            return true;
        }
        internal bool PostRequestforGetCredentials(FunctionRequest functionRequest)
        {
            Response response = new Response();
            ArrayList functionList = new ArrayList();

            try
            {
                List<ParameterGroup> funcParameterGroups = new List<ParameterGroup>();
                functionList.Add(functionRequest);
                response = PostRequest(functionList);
                string message = response.Message;
                
                Trace.WriteLine(DateTime.Now + "\t Message " + message);
                foreach (object obj1 in response.Body.ChildList)
                {
                    if (obj1 is Function)
                    {
                        Function func = (Function)obj1;
                        if (String.Compare(func.Name, functionRequest.Name, true) != 0 || func.FunctionResponse == null)
                            continue;

                        Trace.WriteLine(DateTime.Now + "\t Count of parametergroup " + func.FunctionResponse.ChildList.Count.ToString());


                        foreach (object obj2 in func.FunctionResponse.ChildList)
                        {
                            if (obj2 is Parameter)
                            {
                                Parameter para = obj2 as Parameter;

                                if (para.getKey().ToUpper() == "MACHINEID" && para.Value.Length != 0)
                                {
                                    IP = para.Value;
                                }
                                if (para.getKey().ToUpper() == "USERNAME" && para.Value.Length != 0)
                                {
                                    Username = para.Value;
                                }
                                if (para.getKey().ToUpper() == "PASSWORD" && para.Value.Length != 0)
                                {
                                    Password = para.Value;
                                }
                            }
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
                Trace.WriteLine("Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
            return true;
        }
        public bool UpdaterCredentialsToCX(string ip, string username, string password)
        {
            Response response = new Response();
            Parameter param = new Parameter();
            FunctionRequest request = new FunctionRequest();


            param.Name = "MachineId";
            param.Value = ip;
            request.addChildObj(param);
            param = new Parameter();
            param.Name = "UserName";
            param.Value = username;
            request.addChildObj(param);
            param = new Parameter();
            param.Name = "Password";
            param.Value = password;

            request.addChildObj(param);

            request.Name = "InsertHostCredentials";

            return PostRequestforCredentialUpdation(request);
            
        }


        internal bool PostRequestforCredentialUpdation(FunctionRequest functionRequest)
        {
            Response response = new Response();
            ArrayList functionList = new ArrayList();

            try
            {
                List<ParameterGroup> funcParameterGroups = new List<ParameterGroup>();
                functionList.Add(functionRequest);
                response = PostRequest(functionList);
                string message = response.Message;
                Trace.WriteLine(DateTime.Now + "\t Response " + response.ToXML().ToString());
                Trace.WriteLine(DateTime.Now + "\t Message " + message);
                if (message.ToUpper() == "SUCCESS")
                {
                    
                    return true;
                }
                else
                {
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
                Trace.WriteLine("Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
            return true;
        }
        // This method will restart replication pairs for each host. 
        //input for this method is source host and target inmage id.
        public bool PostToRestartReplications(Host host, string mtInmageHostID)
        {
            Response response = new Response();
            ArrayList list = new ArrayList();
            Parameter param = new Parameter();
            
            FunctionRequest functionRequest = new FunctionRequest();

            functionRequest.Name = "RestartResyncReplicationPairs";
            param.Name = "ResyncRequired";
            param.Value = "Yes";
            functionRequest.addChildObj(param);

            param = new Parameter();
            param.Name = "SourceHostGUID";
            param.Value = host.inmage_hostid;
            functionRequest.addChildObj(param);

            param = new Parameter();
            param.Name = "TargetHostGUID";
            param.Value = mtInmageHostID;
            functionRequest.addChildObj(param);
            Trace.WriteLine(DateTime.Now + "\t Printing xml to start replications " + functionRequest.ToXML().ToString());
            return PostRequestforRestart(functionRequest);
        }

        //This will call acutal cx api to start resyc pairs.
        internal bool PostRequestforRestart(FunctionRequest functionRequest)
        {
            Response response = new Response();
            ArrayList functionList = new ArrayList();
            
            try
            {
                List<ParameterGroup> funcParameterGroups = new List<ParameterGroup>();
                functionList.Add(functionRequest);
                response = PostRequest(functionList);
                string message = response.Message;
                foreach (object obj1 in response.Body.ChildList)
                {
                    if (obj1 is Function)
                    {
                        Function func = (Function)obj1;
                        if (String.Compare(func.Name, functionRequest.Name, true) != 0 || func.FunctionResponse == null)
                            continue;
                        Trace.WriteLine(DateTime.Now + "\t Count of parametergroup " + func.FunctionResponse.ChildList.Count.ToString());
                        Trace.WriteLine(func.FunctionResponse.ToXML().ToString());
                        foreach (object obj2 in func.FunctionResponse.ChildList)
                        {
                            if (obj2 is Parameter)
                            {
                                Parameter para = obj2 as Parameter;

                                if (para.getKey().ToUpper() == "RESYNCSTATUS" && para.Value.Length != 0)
                                {
                                    if (para.Value.ToUpper() == "SUCCESS")
                                    {
                                        Trace.WriteLine(DateTime.Now + "\t Successfully completed restart resync " + para.Value);
                                    }
                                }
                            }

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
                Trace.WriteLine("Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }

            return true;
        }


        // This method will post the request to get the resync hostlist.
        //This is check the response and read the paramaetergroup and calls the ReadFunctionGroupForResyncHosts
        internal bool PostRequestToGetResyncHosts(FunctionRequest functionRequest)
        {
            Response response = new Response();
            ArrayList functionList = new ArrayList();

            try
            {
                List<ParameterGroup> funcParameterGroups = new List<ParameterGroup>();
                functionList.Add(functionRequest);
                response = PostRequest(functionList);
                string message = response.Message;

                Trace.WriteLine(DateTime.Now + "\t Message " + message);
                foreach (object obj1 in response.Body.ChildList)
                {
                    if (obj1 is Function)
                    {
                        Function func = (Function)obj1;
                        if (String.Compare(func.Name, functionRequest.Name, true) != 0 || func.FunctionResponse == null)
                            continue;

                        Trace.WriteLine(DateTime.Now + "\t Count of parametergroup " + func.FunctionResponse.ChildList.Count.ToString());


                        foreach (object obj2 in func.FunctionResponse.ChildList)
                        {
                            if (obj2 is ParameterGroup)
                            {
                                ParameterGroup parameterGroups = obj2 as ParameterGroup;
                                List<ParameterGroup> listParameterGroup = new List<ParameterGroup>();
                                listParameterGroup.Add(parameterGroups);
                                ReadFunctionGroupForResyncHosts(listParameterGroup);
                            }
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
                Trace.WriteLine("Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
            return true;
        }

        // This method will read the parameter group and calls the actual method which is going to read host ids of resync pairs.
        internal static bool ReadFunctionGroupForResyncHosts(List<ParameterGroup> funcParameterGroups)
        {
            try
            {
                refArraylist = new ArrayList();
                foreach (ParameterGroup group in funcParameterGroups)
                {
                    Hashtable planHash = new Hashtable();

                    Trace.WriteLine(DateTime.Now + "\t Printing the group id " + group.Id.ToString());
                    foreach (object planList in group.ChildList)
                    {
                        if (planList is ParameterGroup)
                        {
                            ParameterGroup parameterGroups = planList as ParameterGroup;
                            List<ParameterGroup> listParameterGroup = new List<ParameterGroup>();
                            listParameterGroup.Add(parameterGroups);
                            ReadFunctionGroupTOReadSourceTargetIDS(listParameterGroup);
                           

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
                Trace.WriteLine("Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
           
            return true;
        }

        // This method will derives parameter and reads all the nodes from this method.
        internal static bool ReadFunctionGroupTOReadSourceTargetIDS(List<ParameterGroup> funcParameterGroups)
        {
            try
            {


                foreach (ParameterGroup group in funcParameterGroups)
                {
                    Hashtable planHash = new Hashtable();

                    Trace.WriteLine(DateTime.Now + "\t Printing the group id " + group.Id.ToString());
                    foreach (object planList in group.ChildList)
                    {
                        if (planList is Parameter)
                        {



                            Parameter paraMeterhostid = planList as Parameter;
                            if (paraMeterhostid.Name.ToUpper() == "SOURCEHOSTGUID")
                            {

                                planHash.Add("SourceHostId", paraMeterhostid.Value);
                                Trace.WriteLine(DateTime.Now + "\t Source host id "+  paraMeterhostid.Value);

                            }
                            if (paraMeterhostid.Name.ToUpper() == "TARGETHOSTGUID")
                            {

                                planHash.Add("TargetHostId", paraMeterhostid.Value);
                                Trace.WriteLine(DateTime.Now + "\t target host id "+ paraMeterhostid.Value);

                            }
                        }
                       
                    }
                    if (planHash != null)
                    {
                        refArraylist.Add(planHash);
                    }
                   // Trace.WriteLine(DateTime.Now + "\t Count of listt " + refArraylist.Count.ToString());


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
                Trace.WriteLine("Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
           
            return true;
        }

        public bool Post(Host host, string functionname)
        {
            Response response = new Response();
            ArrayList list = new ArrayList();
            Parameter param = new Parameter();
            ParameterGroup paraGroup = new ParameterGroup();
            FunctionRequest functionRequest = new FunctionRequest();
            if (functionname == "GetRequestStatus")
            {
               
                paraGroup.Id = "RequestIdList";
                param.Name = "RequestId";
                param.Value = host.requestId;
                paraGroup.ChildList.Add(param);
                functionRequest.addChildObj(paraGroup);
            }
            else if (functionname == "RefreshHostInfoStatus")
            {
               // paraGroup.Id = "RefreshHostInfoStatus";
                param.Name = "RequestId";
                param.Value = host.requestId;
                //paraGroup.ChildList.Add(param);
                functionRequest.addChildObj(param);
            }
            else if (functionname == "UnregisterAgent")
            {
                param.Name = "HostGUID";
                param.Value = host.inmage_hostid;
                functionRequest.addChildObj(param);
            }
            else if (functionname == "StartFXJob")
            {
                param = new Parameter();
                param.Name = "PlanId";
                param.Value = host.planid;
                functionRequest.addChildObj(param);
            }
            else if (functionname == "InsertScriptPolicy")
            {
               
                param = new Parameter();
                param.Name = "HostID";
                param.Value = host.inmage_hostid;
                functionRequest.addChildObj(param);
                param = new Parameter();
                param.Name = "ScriptPath";
                if (host.runEsxUtilCommand == false)
                {
                    param.Value = host.vx_agent_path + "scripts/vCon/linux_script.sh";
                }
                else
                {
                    param.Value = host.commandtoExecute;
                }
                functionRequest.addChildObj(param);


            }
            else if (functionname == "MonitorESXProtectionStatus")
            {

                if (host.inmage_hostid != null)
                {
                    param = new Parameter();
                    param.Name = "HostIdentification";
                    param.Value = host.inmage_hostid;
                    functionRequest.addChildObj(param);
                }
                param = new Parameter();
                if (host.operationType == Com.Inmage.Esxcalls.OperationType.Initialprotection)
                {
                    param.Name = "StepName";
                    param.Value = "Protection";
                    functionRequest.addChildObj(param);
                }
                else if (host.operationType == Com.Inmage.Esxcalls.OperationType.Additionofdisk)
                {
                    param.Name = "StepName";
                    param.Value = "Adddisk";
                    functionRequest.addChildObj(param);
                }
                else if (host.operationType == Com.Inmage.Esxcalls.OperationType.Resize)
                {
                    param.Name = "StepName";
                    param.Value = "volumeresize";
                    functionRequest.addChildObj(param);
                }
                else if (host.operationType == Com.Inmage.Esxcalls.OperationType.Removevolume)
                {
                    param.Name = "StepName";
                    param.Value = "volumeremove";
                    functionRequest.addChildObj(param);
                }
                else if (host.operationType == Com.Inmage.Esxcalls.OperationType.Removedisk)
                {
                    param.Name = "StepName";
                    param.Value = "diskremove";
                    functionRequest.addChildObj(param);
                }
                else if (host.operationType == Com.Inmage.Esxcalls.OperationType.Failback)
                {
                    param.Name = "StepName";
                    param.Value = "Failback";
                    functionRequest.addChildObj(param);
                }
                else if (host.operationType == Com.Inmage.Esxcalls.OperationType.Recover)
                {
                    param.Name = "StepName";
                    param.Value = "Recovery";
                    functionRequest.addChildObj(param);
                }
                else if (host.operationType == Com.Inmage.Esxcalls.OperationType.Drdrill)
                {
                    param.Name = "StepName";
                    param.Value = "DrDrill";
                    functionRequest.addChildObj(param);
                }
                else if (host.operationType == Com.Inmage.Esxcalls.OperationType.Resume)
                {
                    param.Name = "StepName";
                    param.Value = "Resume";
                    functionRequest.addChildObj(param);
                }
                param = new Parameter();
                param.Name = "PlanId";
                param.Value = host.planid;
                functionRequest.addChildObj(param);
            }
            else if (functionname == "RemoveExecutionStep")
            {
                param = new Parameter();
                if (host.operationType == Com.Inmage.Esxcalls.OperationType.Initialprotection)
                {
                    param.Name = "StepName";
                    param.Value = "Protection";
                    functionRequest.addChildObj(param);
                }
                else if (host.operationType == Com.Inmage.Esxcalls.OperationType.Additionofdisk)
                {
                    param.Name = "StepName";
                    param.Value = "Adddisk";
                    functionRequest.addChildObj(param);
                }
                else if (host.operationType == Com.Inmage.Esxcalls.OperationType.Failback)
                {
                    param.Name = "StepName";
                    param.Value = "Failback";
                    functionRequest.addChildObj(param);
                }
                else if (host.operationType == Com.Inmage.Esxcalls.OperationType.Recover)
                {
                    param.Name = "StepName";
                    param.Value = "Recovery";
                    functionRequest.addChildObj(param);
                }
                else if (host.operationType == Com.Inmage.Esxcalls.OperationType.Drdrill)
                {
                    param.Name = "StepName";
                    param.Value = "DrDrill";
                    functionRequest.addChildObj(param);
                }
                else if (host.operationType == Com.Inmage.Esxcalls.OperationType.Resume)
                {
                    param.Name = "StepName";
                    param.Value = "Resume";
                    functionRequest.addChildObj(param);
                }
                else if (host.operationType == Com.Inmage.Esxcalls.OperationType.Resize)
                {
                    param.Name = "StepName";
                    param.Value = "volumeresize";
                    functionRequest.addChildObj(param);
                }
                param = new Parameter();
                param.Name = "PlanName";
                param.Value = host.plan;
                functionRequest.addChildObj(param);
            }
            else if (functionname == "RefreshHostInfo")
            {
                paraGroup.Id = "Host1";
                param.Name = "HostGUID";
                param.Value = host.inmage_hostid;
                paraGroup.addChildObj(param);
                param = new Parameter();
                param.Name = "Option";
                param.Value = "dlm_normal";
                paraGroup.addChildObj(param);
                functionRequest.addChildObj(paraGroup);
            }
            else if (functionname == "GetResyncRequiredHosts")
            {
                functionRequest.Name = functionname;
            }
            else
            {
                param.Name = "HostGUID";
                param.Value = host.inmage_hostid;
                functionRequest.addChildObj(param);
               
                
            }
            
           
            //functionRequest.addChildObj(param);
            functionRequest.Name = functionname;
            Trace.WriteLine(DateTime.Now + "\t paragroup " + functionRequest.ToXML());
            //functionRequest.Name = "GetHostMBRPath";
            FunctionRequest fun = new FunctionRequest();
            //fun.Name = "GetHostMBRPath";
            if (functionRequest.Name == "GetHostInfo" || functionRequest.Name == "GetRequestStatus")
            {
                if (PostRequest(functionRequest,  host) == true)
                {
                    host = GetHost;
                }
                else
                {
                    host = GetHost;
                    Trace.WriteLine(DateTime.Now + "\t Failed to complete the task ");
                    return false;
                }
                
                if (host.operatingSystem == null)
                {
                    host = GetHost;
                    return false;
                }
            }
            else if (functionRequest.Name == "RefreshHostInfoStatus")
            {
                if (GetRequestStatusForAllHosts(functionRequest) == true)
                {
                    Trace.WriteLine(DateTime.Now + "\t Completed refreshing all hosts ");
                }
                else
                {
                    Trace.WriteLine(DateTime.Now + "\t It seems it has been in pending state");
                    return false;
                }
            }
            else if (functionRequest.Name == "StartFXJob")
            {
                PostTorunFXJob(functionRequest,  host);
                host = GetHost;
                
            }
            else if (functionRequest.Name == "RefreshHostInfo")
            {

                response = PostRequestForRefreshHost(functionRequest,  host);
                host = GetHost; 

            }
            else if (functionRequest.Name == "InsertScriptPolicy")
            {
                response = PostRequestForRefreshHost(functionRequest,  host);
                host = GetHost;
            }
            else if (functionRequest.Name == "MonitorESXProtectionStatus")
            {
                if (PostRequestForMonitorapi(functionRequest,  host) == false)
                {
                    return false;
                }
                host = GetHost;
            }
            else if (functionRequest.Name == "RemoveExecutionStep")
            {
                PostRequestForRemoveDataBaseapi(functionRequest,  host);
                host = GetHost;
            }
            else if(functionRequest.Name == "UnregisterAgent")
            {
                if (PostRequestForRemoveHost(functionRequest) == true)
                {
                    return true;
                }
                else
                {
                    Trace.WriteLine(DateTime.Now + "\t Failed to unregister host");
                    return false;
                }
            }
            else if (functionRequest.Name == "GetResyncRequiredHosts")
            {
                if (PostRequestToGetResyncHosts(functionRequest) == true)
                {
                    Trace.WriteLine(DateTime.Now + "\t Successfully got the resyn list");
                    return true;
                }
                else
                {
                    Trace.WriteLine(DateTime.Now + "\t Failed to get the resync host list");
                    return false;
                }
            }
            else
            {
                response = PostRequestMbrpath(functionRequest,  host);
                host = GetHost;
            }

            refHost = host;
            return true;
        }


        internal Response PostReadinessRequest(FunctionRequest functionRequest, Host host)
        {
            Response response = new Response();
            ArrayList functionList = new ArrayList();
            try
            {
                List<ParameterGroup> funcParameterGroups = new List<ParameterGroup>();
                functionList.Add(functionRequest);
                response = PostRequest(functionList);
                string message = response.Message;
                Trace.WriteLine(DateTime.Now + "\t Message " + message);
                string xml = response.ToXML();
                Trace.WriteLine(DateTime.Now + "\t Printing xml " + xml);
                Trace.WriteLine(DateTime.Now + "\t body of child " + response.Body.ToXML());
                foreach (object obj1 in response.Body.ChildList)
                {
                    if (obj1 is Function)
                    {
                        Function func = (Function)obj1;
                        foreach (object obj2 in func.FunctionResponse.ChildList)
                        {
                            Parameter p = obj2 as Parameter;
                            if (host.recoveryOperation == 1)
                            {
                                if (p.Name != null && p.Name.ToUpper() == "COMMONTAGEXISTS")
                                {
                                    if (p.Value.ToUpper() == "TRUE")
                                    {
                                        host.commonTagAvaiable = true;
                                    }
                                    else
                                    {
                                        host.commonTagAvaiable = false;
                                    }
                                }
                            }
                            else if (host.recoveryOperation == 2)
                            {
                                if (p.Name != null &&  p.Name.ToUpper() == "COMMONTIMEEXISTS")
                                {
                                    if (p.Value.ToUpper() == "TRUE")
                                    {
                                        host.commonTimeAvaiable = true;
                                    }
                                    else
                                    {
                                        host.commonTimeAvaiable = false;
                                    }
                                }
                            }
                            else if (host.recoveryOperation == 3)
                            {
                                if (p.Name.ToUpper() == "COMMONTAGEXISTS")
                                {
                                    if (p.Value.ToUpper() == "TRUE")
                                    {
                                        host.commonUserDefinedTimeAvailable = true;
                                    }
                                    else
                                    {
                                        host.commonUserDefinedTimeAvailable = false;
                                    }
                                    
                                }
                                if (p.Name.ToUpper() == "BEFORETAGNAME")
                                {
                                    host.tag = p.Value;
                                }
                                if (p.Name.ToUpper() == "BEFORETAGTIME")
                                {
                                    if (p.Value.Length != 0)
                                    {
                                        host.commonUserDefinedTimeAvailable = true;
                                        string tagavailable = p.Value;
                                        System.DateTime dtDateTime = new DateTime(1970, 1, 1, 0, 0, 0, 0);
                                        dtDateTime = dtDateTime.AddSeconds(long.Parse(p.Value.ToString()));
                                        host.userDefinedTime = dtDateTime.ToString();
                                        Trace.WriteLine(DateTime.Now + "\t Tag available before the selected time " + host.userDefinedTime);
                                    }
                                }
                            }
                            else if (host.recoveryOperation == 4)
                            {
                                Trace.WriteLine(DateTime.Now + "\t Printing the name " + p.Name);
                                if (p.Name.ToUpper() == "COMMONTIMEEXISTS")
                                {
                                    if (p.Value.ToUpper() == "TRUE")
                                    {
                                        Trace.WriteLine(DateTime.Now + "\t Printing the value " + p.Value);
                                        host.commonSpecificTimeAvaiable = true;
                                    }
                                    else
                                    {
                                        host.commonSpecificTimeAvaiable = false;
                                    }

                                }
                            }
                            else if (host.recoveryOperation == 5)
                            {
                                Trace.WriteLine(DateTime.Now + "\t Printing the name " + p.Name);
                                if (p.Name.ToUpper() == "TAGEXISTS")
                                {
                                    if (p.Value.ToUpper() == "TRUE")
                                    {
                                        Trace.WriteLine(DateTime.Now + "\t Printing the value " + p.Value);
                                        host.commonTagAvaiable = true;
                                    }
                                    else
                                    {
                                        Trace.WriteLine(DateTime.Now + "\t Printing the value of verify tag " + p.Value);
                                        host.commonTagAvaiable = false;
                                    }

                                }
                            }
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
            refHost = host;
            return response;
        }

        internal bool PostRequestOfDummyDisk(FunctionRequest functionRequest, Host host, bool logRequired)
        {
             Response response = new Response();
            ArrayList functionList = new ArrayList();

            try
            {
                List<ParameterGroup> funcParameterGroups = new List<ParameterGroup>();
                functionList.Add(functionRequest);
                response = PostRequest(functionList);
                string message = response.Message;
                Trace.WriteLine(DateTime.Now + "\t Message " + message);
                string xml = null;
                try
                {
                     xml = response.Body.ToXML();
                    foreach (Object o in response.Body.ChildList)
                    {
                        Trace.WriteLine(o.ToString());
                    }
                }
                catch (Exception ex)
                {
                    Trace.WriteLine(DateTime.Now + "\t Failed to read output of response");
                }
                if (logRequired == false)
                {
                    Trace.WriteLine(DateTime.Now + "\t Printing xml " + xml);
                }
                else
                {
                    try
                    {
                        FileInfo info = new FileInfo(WinTools.FxAgentPath() + "\\vContinuum\\logs\\ESXUtilorWin.log");
                        StreamWriter writer = info.AppendText();
                        writer.WriteLine(xml);
                        writer.Close();
                        writer.Dispose();
                    }
                    catch (Exception ex)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Failed to write log for faild case of esxutilwin " + ex.Message);
                    }
                }
                foreach (object obj1 in response.Body.ChildList)
                {
                    if (obj1 is Function)
                    {
                        Function func = (Function)obj1;
                        if (String.Compare(func.Name, functionRequest.Name, true) != 0 || func.FunctionResponse == null)
                            continue;
                        
                            Trace.WriteLine(DateTime.Now + "\t Count of parametergroup " + func.FunctionResponse.ChildList.Count.ToString());
                        

                        foreach (object obj2 in func.FunctionResponse.ChildList)
                        {
                            if (obj2 is Parameter)
                            {
                                Parameter para = obj2 as Parameter;
                                if (para.getKey().ToUpper() == "STATUS")
                                {
                                    if (para.Value.ToString().ToUpper() == "PENDING")
                                    {
                                        Trace.WriteLine(DateTime.Now + "\t Status is pending");
                                        return false;
                                    }
                                    else if (para.Value.ToString().ToUpper() == "INPROGRESS" || para.Value.ToString().ToUpper() == "INPROGESS")
                                    {
                                        Trace.WriteLine(DateTime.Now + "\t Status is In progress");
                                        return false;
                                    }
                                    else
                                    {
                                        Trace.WriteLine(DateTime.Now + "\t Status is " + para.Value);
                                        //return true;
                                    }
                                }
                                // Reading error message into diskIfo string. 
                                // In ui we are going to splict and use that sring.
                                if (para.getKey().ToUpper() == "ERRORMESSAGE")
                                {
                                    if (para.Value.Length != 0)
                                    {
                                        Trace.WriteLine(DateTime.Now + "\t Disk Values " + para.Value);
                                        host.diskInfo = para.Value;
                                        refHost = host;
                                        return true;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed to get the request status");
            }
            refHost = host;
            return true;
        }


        internal Response PostRequestForFxJob(FunctionRequest functionRequest, Host host)
        {
            Response response = new Response();
            ArrayList functionList = new ArrayList();

            try
            {
                List<ParameterGroup> funcParameterGroups = new List<ParameterGroup>();
                functionList.Add(functionRequest);
                response = PostRequest(functionList);
                string message = response.Message;
                Trace.WriteLine(DateTime.Now + "\t Message " + message);
                string xml = response.ToXML();
                Trace.WriteLine(DateTime.Now + "\t Printing xml " + xml);
                foreach (object obj1 in response.Body.ChildList)
                {
                    if (obj1 is Function)
                    {
                        Function func = (Function)obj1;
                        if (String.Compare(func.Name, functionRequest.Name, true) != 0 || func.FunctionResponse == null)
                            continue;
                        Trace.WriteLine(DateTime.Now + "\t Count of parametergroup " + func.FunctionResponse.ChildList.Count.ToString());

                        foreach (object obj2 in func.FunctionResponse.ChildList)
                        {
                            if (obj2 is Parameter)
                            {
                                Parameter para = obj2 as Parameter;
                                if (para.getKey().ToUpper() == "STATUS")
                                {
                                    if (para.Value.ToString().ToUpper() == "PENDING")
                                    {
                                        Trace.WriteLine(DateTime.Now + "\t Status is pending");
                                        return response;
                                    }
                                    else if (para.Value.ToString().ToUpper() == "INPROGRESS")
                                    {
                                        Trace.WriteLine(DateTime.Now + "\t Status is In progress");
                                        return response;
                                    }
                                    else
                                    {
                                        Trace.WriteLine(DateTime.Now + "\t Status is " + para.Value);
                                    }
                                }
                            }
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed to post the api " + ex.Message);
            }
            refHost = host;
            return response;
        }



        internal Response ReadRequest(FunctionRequest functionRequest, Host host)
        {
            Response response = new Response();
            ArrayList functionList = new ArrayList();
            try
            {
                List<ParameterGroup> funcParameterGroups = new List<ParameterGroup>();
                functionList.Add(functionRequest);
                response = PostRequest(functionList);
               // string message = response.Message;
               
                string xml = response.ToXML();
               // Trace.WriteLine(DateTime.Now + "\t Printing xml " + xml);
               
                foreach (object obj1 in response.Body.ChildList)
                {
                    if (obj1 is Function)
                    {
                        Function func = (Function)obj1;
                        if (String.Compare(func.Name, functionRequest.Name, true) != 0 || func.FunctionResponse == null)
                            continue;
                        Trace.WriteLine(DateTime.Now + "\t Count of parametergroup " + func.FunctionResponse.ChildList.Count.ToString());

                        foreach (object obj2 in func.FunctionResponse.ChildList)
                        {
                            if (obj2 is Parameter)
                            {
                                Parameter para = obj2 as Parameter;
                                if (para.getKey() == "RequestId")
                                {
                                   host.requestId = para.Value;
                                   Trace.WriteLine(DateTime.Now + "\t Printing the id " + host.requestId);
                                }
                            }
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
            refHost = host;
            return response;
        }





        internal static bool ReadFunctionGroupForDisks(List<ParameterGroup> funcParameterGroups, Host host)
        {
            try
            {
                foreach (ParameterGroup group in funcParameterGroups)
                {

                    if (group.Id.ToUpper().ToString() == "DISKLIST")
                    {

                        foreach (object diskList in group.ChildList)
                        {

                            if (diskList is ParameterGroup)
                            {
                                ParameterGroup childDisks = diskList as ParameterGroup;

                                List<ParameterGroup> listParameterGroup = new List<ParameterGroup>();
                                listParameterGroup.Add(childDisks);
                                string volumesInDisk = null;
                                foreach (ParameterGroup childDiskListgroup in listParameterGroup)
                                {
                                    Trace.WriteLine(DateTime.Now + "\t Parameter grp id " + childDiskListgroup.Id.ToString());
                                    host.diskHash = new Hashtable();
                                    foreach (object diskObj in childDiskListgroup.ChildList)
                                    {
                                        
                                        if (diskObj is Parameter)
                                        {
                                            Parameter paraMeterDisks = diskObj as Parameter;
                                            if (paraMeterDisks.getKey().ToUpper() == "DISKNAME")
                                            {
                                                // h.diskHash["Name"] = paraMeterDisks.Value;
                                                if (paraMeterDisks.Value.Length != 0)
                                                {
                                                    host.diskHash.Add("src_devicename", paraMeterDisks.Value);
                                                    string name = paraMeterDisks.Value;
                                                    string disknumber = null;
                                                    Trace.WriteLine(DateTime.Now + "\t printing the length " + name.Length.ToString());
                                                    if (host.os == OStype.Windows && name.Length > 17)
                                                    {
                                                        disknumber = name.Remove(0, 17);
                                                        Trace.WriteLine(DateTime.Now + "\t Disk number " + disknumber);
                                                    }
                                                    
                                                    if (host.os == OStype.Windows && name.Length > 12)
                                                    {
                                                        name = name.Remove(0, 12);
                                                    }
                                                    
                                                    if (disknumber != null)
                                                    {
                                                        host.diskHash.Add("disknumber", disknumber);
                                                    }
                                                    host.diskHash.Add("Name", name);
                                                    if (paraMeterDisks.Value.Length != 0)
                                                    {
                                                        Trace.WriteLine(DateTime.Now + "\t DiskName " + host.diskHash["Name"].ToString());
                                                    }
                                                }
                                            }
                                            if (paraMeterDisks.getKey().ToUpper() == "SIZE")
                                            {
                                                if (paraMeterDisks.Value != null)
                                                {

                                                    if (paraMeterDisks.Value.Length != 0)
                                                    {
                                                        try
                                                        {
                                                            long size = long.Parse(paraMeterDisks.Value.ToString());

                                                            size = size / 1024;
                                                           
                                                            host.diskHash.Add("ActualSize", size);
                                                            // Adding 2% for each disk.....
                                                            //size = size + size / 50;  //Commenting this line. Now CX reports actual disk size instead of WMI size. Hence commenting only the CX based p2v conversion
                                                            //only


                                                            // h.diskHash["Size"] = size.ToString();
                                                            host.diskHash.Add("Size", size.ToString());
                                                            Trace.WriteLine(DateTime.Now + "\t Disk Size " + host.diskHash["Size"].ToString());
                                                        }
                                                        catch (Exception ex)
                                                        {
                                                            Trace.WriteLine(DateTime.Now + "\t We got wrong disk size " + ex.Message);
                                                        }
                                                    }
                                                    else
                                                    {
                                                        Trace.WriteLine(DateTime.Now + "\t Disk size got null from cx ");
                                                    }
                                                }
                                            }
                                            if (paraMeterDisks.getKey().ToUpper() == "IDEORSCSI")
                                            {
                                                //h.diskHash["IdeorScsi"] = paraMeterDisks.Value;

                                                if (paraMeterDisks.Value.Length != 0)
                                                {
                                                    host.diskHash.Add("ide_or_scsi", paraMeterDisks.Value);
                                                    Trace.WriteLine(DateTime.Now + "\t Disk Ide or scsi " + host.diskHash["ide_or_scsi"].ToString());
                                                }
                                                else
                                                {
                                                    Trace.WriteLine(DateTime.Now + "\t Disk Ide or scsi value is null form cx response");
                                                }
                                            }
                                            if (paraMeterDisks.getKey().ToUpper() == "SCSIPORT")
                                            {
                                                //h.diskHash["ScsiPort"] = paraMeterDisks.Value;
                                               
                                                if (paraMeterDisks.Value.Length != 0)
                                                {
                                                    host.diskHash.Add("ScsiPort", paraMeterDisks.Value);
                                                    host.diskHash.Add("port_number", paraMeterDisks.Value);
                                                    Trace.WriteLine(DateTime.Now + "\t Disk scsi port " + host.diskHash["ScsiPort"].ToString());
                                                }
                                                else
                                                {
                                                    Trace.WriteLine(DateTime.Now + "\t Disk scsi port value is null form cx response");
                                                }
                                            }
                                            if (paraMeterDisks.getKey().ToUpper() == "SCSITARGETID")
                                            {
                                                //h.diskHash["ScsiTargetId"] = paraMeterDisks.Value;
                                               
                                                if (paraMeterDisks.Value.Length != 0)
                                                {
                                                    host.diskHash.Add("ScsiTargetId", paraMeterDisks.Value);
                                                    Trace.WriteLine(DateTime.Now + "\t Disk ScsiTargetId " + host.diskHash["ScsiTargetId"].ToString());
                                                }
                                                else
                                                {
                                                    Trace.WriteLine(DateTime.Now + "\t Disk ScsiTargetId value is null form cx response");
                                                }
                                            }
                                            if (paraMeterDisks.getKey().ToUpper() == "SCSILOGICALUNIT")
                                            {
                                                //h.diskHash["ScsiLogicalUnit"] = paraMeterDisks.Value;
                                                if (paraMeterDisks.Value.Length != 0)
                                                {
                                                    host.diskHash.Add("ScsiLogicalUnit", paraMeterDisks.Value);
                                                    Trace.WriteLine(DateTime.Now + "\t Disk ScsiLogicalUnit " + host.diskHash["ScsiLogicalUnit"].ToString());
                                                }
                                                else
                                                {
                                                    Trace.WriteLine(DateTime.Now + "\t Disk ScsiLogicalUnit value is null form cx response");
                                                }

                                            }
                                            if (paraMeterDisks.getKey().ToUpper() == "SCSIBUSNUMBER")
                                            {
                                                if (paraMeterDisks.Value.Length != 0)
                                                {
                                                    host.diskHash.Add("ScsiBusNumber", paraMeterDisks.Value);
                                                }
                                                else
                                                {
                                                    Trace.WriteLine(DateTime.Now + "\t Disk ScsiBusNumber value is null form cx response");
                                                }
                                            }
                                            if (paraMeterDisks.getKey().ToUpper() == "ISISCSIDEVICE")
                                            {
                                                // h.diskHash["IsIscsiDevice"] = paraMeterDisks.Value;
                                                
                                                if (paraMeterDisks.Value.Length != 0)
                                                {
                                                    host.diskHash.Add("IsIscsiDevice", paraMeterDisks.Value);
                                                    Trace.WriteLine(DateTime.Now + "\t Disk IsIscsiDevice " + host.diskHash["IsIscsiDevice"].ToString());
                                                }
                                                else
                                                {
                                                    
                                                    Trace.WriteLine(DateTime.Now + "\t Disk IsIscsiDevice value is null form cx response");
                                                }
                                            }
                                            if (paraMeterDisks.getKey().ToUpper() == "DISKTYPE")
                                            {
                                                // h.diskHash["DiskType"] = paraMeterDisks.Value;

                                                if (paraMeterDisks.Value.Length != 0)
                                                {
                                                    host.diskHash.Add("disk_type", paraMeterDisks.Value);
                                                    host.diskHash.Add("disk_type_os", paraMeterDisks.Value);
                                                    Trace.WriteLine(DateTime.Now + "\t Disk DiskType " + host.diskHash["disk_type"].ToString());
                                                }
                                                else
                                                {
                                                    host.diskHash.Add("disk_type", "persistent");
                                                }
                                            }
                                            
                                            if (paraMeterDisks.getKey().ToUpper() == "DISKLAYOUT")
                                            {
                                                //h.diskHash["DiskLayout"] = paraMeterDisks.Value;
                                               
                                                if (paraMeterDisks.Value.Length != 0)
                                                {
                                                    host.diskHash.Add("DiskLayout", paraMeterDisks.Value);
                                                    
                                                    Trace.WriteLine(DateTime.Now + "\t Disk DiskLayout " + host.diskHash["DiskLayout"].ToString());
                                                }
                                                else
                                                {
                                                    Trace.WriteLine(DateTime.Now + "\t Disk DiskLayout value is null");
                                                }

                                            }
                                            if (paraMeterDisks.getKey().ToUpper() == "HEADS")
                                            {
                                                //h.diskHash["TotalHeads"] = paraMeterDisks.Value;
                                                
                                                if (paraMeterDisks.Value.Length != 0)
                                                {
                                                    host.diskHash.Add("TotalHeads", paraMeterDisks.Value);
                                                    Trace.WriteLine(DateTime.Now + "\t Disk Heads " + host.diskHash["TotalHeads"].ToString());
                                                }
                                            }


                                            if (paraMeterDisks.getKey().ToUpper() == "ISSHARED")
                                            {
                                                //h.diskHash["TotalHeads"] = paraMeterDisks.Value;

                                                if (paraMeterDisks.Value.Length != 0)
                                                {
                                                    if (paraMeterDisks.Value.ToUpper() == "TRUE")
                                                    {
                                                        host.diskHash.Add("cluster_disk", "yes");
                                                    }
                                                    else
                                                    {
                                                        host.diskHash.Add("cluster_disk", "no");
                                                    }
                                                    Trace.WriteLine(DateTime.Now + "\t IsShared " + host.diskHash["cluster_disk"].ToString());
                                                }
                                            }


                                            if (paraMeterDisks.getKey().ToUpper() == "SECTORSPERTRACK")
                                            {
                                                //h.diskHash["SectorsPerTrack"] = paraMeterDisks.Value;
                                               
                                                if (paraMeterDisks.Value.Length != 0)
                                                {
                                                    host.diskHash.Add("SectorsPerTrack", paraMeterDisks.Value);
                                                    Trace.WriteLine(DateTime.Now + "\t SectorsPerTrack " + host.diskHash["SectorsPerTrack"].ToString());
                                                }
                                            }
                                            if (paraMeterDisks.getKey().ToUpper() == "BOOTABLE")
                                            {
                                                //h.diskHash["IsBootable"] = paraMeterDisks.Value;

                                                if (paraMeterDisks.Value.Length != 0)
                                                {
                                                    host.diskHash.Add("IsBootable", paraMeterDisks.Value);
                                                    Trace.WriteLine(DateTime.Now + "\t Disk Bootable " + host.diskHash["IsBootable"].ToString());
                                                }
                                                else
                                                {
                                                    host.diskHash.Add("IsBootable", "false");
                                                    Trace.WriteLine(DateTime.Now + "\t Disk Bootable  value is null sp taking as false");
                                                }
                                            }
                                            //if (paraMeterDisks.getKey().ToUpper() == "SCSICONTROLLERMODEL")
                                            //{
                                            //    //h.diskHash["controller_type"] = paraMeterDisks.Value;
                                            //    h.diskHash.Add("controller_type", paraMeterDisks.Value);
                                            //}
                                            if (paraMeterDisks.getKey().ToUpper() == "DISKGROUP")
                                            {
                                                if (paraMeterDisks.Value.Length != 0)
                                                {
                                                    host.diskHash.Add("DiskGroup", paraMeterDisks.Value);
                                                }
                                                else
                                                {
                                                    host.diskHash.Add("DiskGroup", "");
                                                }
                                            }
                                            if (paraMeterDisks.getKey().ToUpper() == "EFI")
                                            {
                                                if (paraMeterDisks.Value.Length != 0)
                                                {
                                                    if (Convert.ToBoolean(paraMeterDisks.Value.ToString()) == true)
                                                    {
                                                        host.efi = true;
                                                    }
                                                    host.diskHash.Add("efi", paraMeterDisks.Value);
                                                }
                                            }
                                            if (paraMeterDisks.getKey().ToUpper() == "SCSIID")
                                            {
                                                if (paraMeterDisks.Value.Length != 0)
                                                {
                                                    host.diskHash.Add("DiskScsiId", paraMeterDisks.Value);
                                                }
                                                else
                                                {
                                                    host.diskHash.Add("DiskScsiId", "");
                                                }
                                            }
                                            if (paraMeterDisks.getKey().ToUpper() == "VENDORINFO")
                                            {
                                                if (paraMeterDisks.Value.Length != 0)
                                                {
                                                    host.diskHash.Add("VendorInfo", paraMeterDisks.Value);
                                                }
                                                else
                                                {
                                                    host.diskHash.Add("VendorInfo", "");
                                                }
                                            }
                                            if (paraMeterDisks.getKey().ToUpper() == "MEDIATYPE")
                                            {
                                                if (paraMeterDisks.Value.Length != 0)
                                                {
                                                    host.diskHash.Add("media_type", paraMeterDisks.Value);
                                                }
                                                else
                                                {
                                                    host.diskHash.Add("media_type", "");
                                                }
                                            }

                                            if (paraMeterDisks.getKey().ToUpper() == "DISKGUID")
                                            {
                                                string disk_guuid = null;
                                                if (paraMeterDisks.Value.Length != 0)
                                                {
                                                    host.diskHash.Add("disk_guuid", paraMeterDisks.Value);
                                                    disk_guuid = paraMeterDisks.Value;
                                                    Trace.WriteLine(DateTime.Now + " \t Disk GUID " + disk_guuid);
                                                }
                                                else
                                                {
                                                    host.diskHash.Add("disk_guuid", "");
                                                }

                                                try
                                                {
                                                    if (disk_guuid != null && (host.diskHash["DiskLayout"] != null && host.diskHash["DiskLayout"].ToString() == "MBR"))
                                                    {
                                                        try
                                                        {
                                                            long num = long.Parse(disk_guuid);
                                                            string myHex = num.ToString("X");
                                                            Trace.WriteLine(DateTime.Now + " \t Disk signature " + myHex);
                                                            host.diskHash.Add("disk_signature", myHex);
                                                        }
                                                        catch (Exception ex)
                                                        {
                                                            Trace.WriteLine(DateTime.Now + "\t Failed to convert to Hex ");

                                                        }
                                                    }
                                                    else
                                                    {
                                                        host.diskHash.Add("disk_signature", disk_guuid);
                                                        Trace.WriteLine(DateTime.Now + "\t This disk is not a MBR disk ");
                                                    }
                                                }
                                                catch (Exception ex)
                                                {
                                                    Trace.WriteLine(DateTime.Now + "\t Failed to convert the signature into hexa " + ex.Message);
                                                }

                                            }
                                        }

                                        volumesInDisk = null;
                                        ReadPartitionsFromParameterGroup(diskObj,  host, volumesInDisk);
                                        host = GetHost;
                                        volumesInDisk = Getvolumesstring;

                                    }
                                    string scsimappingHost = null;
                                    if (host.os == OStype.Windows)
                                    {
                                        if (host.diskHash["ScsiPort"] != null && host.diskHash["ScsiTargetId"] != null && host.diskHash["ScsiLogicalUnit"] != null && host.diskHash["ScsiBusNumber"] != null)
                                        {
                                            scsimappingHost = host.diskHash["ScsiPort"].ToString() + ":" + host.diskHash["ScsiTargetId"].ToString() + ":" + host.diskHash["ScsiLogicalUnit"].ToString() + ":" + host.diskHash["ScsiBusNumber"].ToString();
                                        }
                                        else
                                        {
                                            Trace.WriteLine(DateTime.Now + "\t Some of the values in disk entry are null");
                                        }
                                    }
                                    else if(host.os == OStype.Linux)
                                    {
                                        if (host.diskHash["ScsiBusNumber"] != null && host.diskHash["ScsiTargetId"] != null && host.diskHash["ScsiLogicalUnit"] != null && host.diskHash["ScsiPort"] != null)
                                        {
                                            scsimappingHost = host.diskHash["ScsiBusNumber"].ToString() + ":" + host.diskHash["ScsiTargetId"].ToString() + ":" + host.diskHash["ScsiLogicalUnit"].ToString() + ":" + host.diskHash["ScsiPort"].ToString();
                                        }
                                        else
                                        {
                                            Trace.WriteLine(DateTime.Now + "\t Some of the values in disk entry are null");
                                        }
                                    }
                                    
                                    host.diskHash.Add("scsi_mapping_host", scsimappingHost);
                                    host.diskHash.Add("disk_mode", "Virtual Disk");
                                    host.diskHash.Add("Selected", "Yes");
                                    host.diskHash.Add("SelectedDataStore", "null");
                                    host.diskHash.Add("IsThere", "no");
                                    host.diskHash.Add("IsUsedAsSlot", "no");
                                    host.diskHash.Add("Protected", "no");
                                    if (volumesInDisk != null)
                                    {
                                        host.diskHash.Add("volumelist", volumesInDisk);
                                    }
                                    host.disks.list.Add(host.diskHash);
                                    Trace.WriteLine(DateTime.Now + "\t Printing the diskcount " + host.disks.list.Count.ToString());
                                }
                            }                            
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                System.FormatException nht = new FormatException("Inner exception", ex);
                Trace.WriteLine(nht.GetBaseException());
                Trace.WriteLine(nht.Data);
                Trace.WriteLine(nht.InnerException);
                Trace.WriteLine(nht.Message);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: Read disk parameters " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }

            refHost = host;
            return true;
        }


        private static bool ReadPartitionsFromParameterGroup(Object diskObj,  Host host,  string volumesindisk)
        {
            if (diskObj is ParameterGroup)
            {
                host.partitionHash = new Hashtable();
                ParameterGroup childPartitionsDisk = diskObj as ParameterGroup;
                List<ParameterGroup> partitionsGroup = new List<ParameterGroup>();
                partitionsGroup.Add(childPartitionsDisk);
                foreach (Object partitionObj in partitionsGroup)
                {
                    if (partitionObj is ParameterGroup)
                    {

                        if (childPartitionsDisk.Id.Contains("Partition"))
                        {
                            Trace.WriteLine(DateTime.Now + "\t Parttition " + childPartitionsDisk.Id);
                            foreach (object logicalObj in childPartitionsDisk.ChildList)
                            {
                                if (logicalObj is ParameterGroup)
                                {
                                    ParameterGroup logicalGroup = logicalObj as ParameterGroup;
                                    if (logicalGroup.Id.Contains("LogicalVolume"))
                                    {
                                        host.partitionHash = new Hashtable();
                                        foreach (Object partObj in logicalGroup.ChildList)
                                        {
                                            if (partObj is Parameter)
                                            {
                                                Parameter p = partObj as Parameter;
                                                if (p.getKey().ToUpper() == "NAME")
                                                {
                                                    host.partitionHash.Add("Name", p.Value);
                                                    if (volumesindisk == null)
                                                    {
                                                        volumesindisk = p.Value;
                                                    }
                                                    else
                                                    {
                                                        volumesindisk = volumesindisk + "," + p.Value;
                                                    }
                                                    Trace.WriteLine(DateTime.Now + "\t Name of volume " + p.Value);
                                                }
                                                if (p.getKey().ToUpper() == "SIZE")
                                                {
                                                    host.partitionHash.Add("Size", p.Value);
                                                    Trace.WriteLine(DateTime.Now + "\t Size of volume " + p.Value);
                                                }
                                                if (p.getKey().ToUpper() == "ISSYSTEMVOLUME")
                                                {
                                                    host.partitionHash.Add("IsSystemVolume", p.Value);
                                                    Trace.WriteLine(DateTime.Now + "\t IS it system volume " + p.Value);

                                                }

                                                if (p.getKey().ToUpper() == "SCSIID")
                                                {
                                                    host.partitionHash.Add("PartitionScsiId", p.Value);
                                                    Trace.WriteLine(DateTime.Now + "\t SCSI ID of partition " + p.Value);

                                                }
                                                if (p.getKey().ToUpper() == "VENDORINFO")
                                                {
                                                    host.partitionHash.Add("VendorInfo", p.Value);
                                                    Trace.WriteLine(DateTime.Now + "\t VendorInfo of Partition " + p.Value);

                                                }
                                            }
                                        }
                                        host.disks.partitionList.Add(host.partitionHash);
                                        Trace.WriteLine(DateTime.Now + "\t Count of partitions " + host.disks.partitionList.Count.ToString());
                                    }

                                }
                            }
                        }
                    }
                }
            }
            refHost = host;
            volumesinString = volumesindisk;
            return true;
        }


        internal static bool ReadFunctionGroupForNics(List<ParameterGroup> funcParameterGroups, Host host)
        {
            try
            {
                int nicCount = 1;
                foreach (ParameterGroup group in funcParameterGroups)
                {
                    if (group.Id.ToUpper().ToString() == "MACADDRESSLIST")
                    {

                        foreach (object nicList in group.ChildList)
                        {

                            if (nicList is ParameterGroup)
                            {
                                ParameterGroup childNics = nicList as ParameterGroup;

                                List<ParameterGroup> listParameterGroup = new List<ParameterGroup>();
                                listParameterGroup.Add(childNics);
                                  
                                foreach (ParameterGroup childNicListgroup in listParameterGroup)
                                {
                                    Trace.WriteLine(DateTime.Now + "\t Parameter grp id " + childNicListgroup.Id.ToString());
                                    host.nicHash = new Hashtable();
                                    int i = 0; int j = 0; int k = 0; int l = 0; int m = 0; int n = 0;
                                    string ip = null, subnetmask = null, gateway = null, dnsip = null, dhcp = null, ipType = null; ;
                                    foreach (object nicObj in childNicListgroup.ChildList)
                                    {
                                        int childCount = childNicListgroup.ChildList.Count;
                                        Trace.WriteLine(DateTime.Now + "\t Child mac count " + childNicListgroup.ChildList.Count.ToString());

                                        if (nicObj is Parameter)
                                        {
                                            Parameter paraMeterNic = nicObj as Parameter;
                                            if (paraMeterNic.getKey().ToUpper() == "MACADDRESS")
                                            {
                                                if (paraMeterNic.Value.Length != 0)
                                                {                                                   
                                                    host.nicHash.Add("nic_mac", paraMeterNic.Value);
                                                    Trace.WriteLine(DateTime.Now + "\t Mac address of nic " + host.nicHash["nic_mac"].ToString());
                                                }
                                                else
                                                {
                                                    Trace.WriteLine(DateTime.Now + "\t Mac address is null ");
                                                }
                                            }
                                            if (paraMeterNic.getKey().ToUpper() == "VENDORNAME")
                                            {

                                                host.nicHash.Add("vendor", paraMeterNic.Value);
                                                Trace.WriteLine(DateTime.Now + "\t Vendor " + host.nicHash["vendor"].ToString());
                                            }
                                            
                                        }
                                        if (nicObj is ParameterGroup)
                                        {
                                            ParameterGroup childNicsofMac = nicObj as ParameterGroup;

                                            List<ParameterGroup> listParameterGroupOfMAC = new List<ParameterGroup>();
                                            listParameterGroupOfMAC.Add(childNicsofMac);
                                            if (childNicsofMac.ChildList.Count != 0)
                                            {
                                                foreach (object ipObj in childNicsofMac.ChildList)
                                                {

                                                    Parameter ipParameter = ipObj as Parameter;

                                                    if (ipParameter.getKey().ToUpper() == "IP")
                                                    {
                                                        if (ipParameter.Value.Length != 0)
                                                        {
                                                            bool ipv6 = false;
                                                            IPAddress address;
                                                            if (IPAddress.TryParse(ipParameter.Value, out address))
                                                            {
                                                                
                                                                switch (address.AddressFamily)
                                                                {
                                                                    case System.Net.Sockets.AddressFamily.InterNetwork:
                                                                        // we have IPv4
                                                                        break;
                                                                    case System.Net.Sockets.AddressFamily.InterNetworkV6:
                                                                        // we have IPv6
                                                                        ipv6 = true;
                                                                        break;
                                                                   
                                                                }
                                                            }
                                                            if (ipv6 == true)
                                                            {
                                                                break;
                                                            }
                                                         
                                                        }
                                                        if (i == 0)
                                                        {
                                                            if (ipParameter.Value.Length != 0)
                                                                ip = ipParameter.Value;
                                                            i++;
                                                        }
                                                        else
                                                        {
                                                            if (ip != null)
                                                            {
                                                                if (ipParameter.Value.Length != 0)
                                                                   ip = ip + "," + ipParameter.Value;
                                                            }
                                                            else
                                                            {
                                                                if (ipParameter.Value.Length != 0)
                                                                    ip = ipParameter.Value; ;
                                                            }
                                                        }

                                                    }
                                                    if (ipParameter.getKey().ToUpper() == "SUBNETMASK")
                                                    {
                                                        if (j == 0)
                                                        {
                                                            if (ipParameter.Value.Length != 0)
                                                                subnetmask = ipParameter.Value;
                                                            j++;
                                                        }
                                                        else
                                                        {
                                                            if (subnetmask != null)
                                                            {
                                                                if (ipParameter.Value.Length != 0)
                                                                    subnetmask = subnetmask + ","+ipParameter.Value;
                                                            }
                                                            else
                                                            {
                                                                if (ipParameter.Value.Length != 0)
                                                                    subnetmask = ipParameter.Value;
                                                            }
                                                        }
                                                    }
                                                    if (ipParameter.getKey().ToUpper() == "GATEWAY")
                                                    {
                                                        if (k == 0)
                                                        {
                                                            if (ipParameter.Value.Length != 0)
                                                                gateway = ipParameter.Value;
                                                        }
                                                        else
                                                        {
                                                            if (gateway != null)
                                                            {
                                                                if (ipParameter.Value.Length != 0)
                                                                    gateway = gateway + "," + ipParameter.Value;
                                                            }
                                                            else
                                                            {
                                                                if (ipParameter.Value.Length != 0)
                                                                    gateway = ipParameter.Value;
                                                            }
                                                        }
                                                    }
                                                    if (ipParameter.getKey().ToUpper() == "DNSIP")
                                                    {
                                                        if (l == 0)
                                                        {
                                                            if (ipParameter.Value.Length != 0)
                                                                dnsip = ipParameter.Value;
                                                        }
                                                        else
                                                        {
                                                            if (dnsip != null)
                                                            {
                                                                if (ipParameter.Value.Length != 0)
                                                                    dnsip = dnsip + "," + ipParameter.Value;
                                                            }
                                                            else
                                                            {
                                                                if (ipParameter.Value.Length != 0)
                                                                    dnsip = ipParameter.Value;
                                                            }
                                                        }

                                                    }
                                                    if (ipParameter.getKey().ToUpper() == "ISDHCP")
                                                    {
                                                        if (bool.Parse(ipParameter.Value.ToString()) == true)
                                                        {
                                                             dhcp = "1";
                                                        }
                                                        else
                                                        {
                                                            dhcp = "0";
                                                        }
                                                    }
                                                    if (ipParameter.getKey().ToUpper() == "DEVICENAME")
                                                    {
                                                        if (host.nicHash["devicename"] == null)
                                                        {
                                                            host.nicHash.Add("devicename", ipParameter.Value);
                                                            Trace.WriteLine(DateTime.Now + " \t Printing device name " + ipParameter.Value);
                                                        }
                                                    }
                                                    if (ipParameter.getKey().ToUpper() == "IPTYPE")
                                                    {
                                                        Trace.WriteLine(DateTime.Now + "\t reading IPTYPE values and n value " + n.ToString());
                                                        if (n == 0)
                                                        {
                                                            if (ipParameter.Value.Length != 0)
                                                            {
                                                                ipType = ipParameter.Value;
                                                                Trace.WriteLine(DateTime.Now + "\t Printing the values of iptye " + ipParameter.Value);
                                                                n++;
                                                            }
                                                        }
                                                        else
                                                        {
                                                            if (ipType != null)
                                                            {
                                                                if (ipParameter.Value.Length != 0)
                                                                {
                                                                    ipType = ipType + "," + ipParameter.Value;
                                                                    Trace.WriteLine(DateTime.Now + "\t Printing the values of iptye " + ipParameter.Value);
                                                                }
                                                            }
                                                            else
                                                            {
                                                                if (ipParameter.Value.Length != 0)
                                                                {
                                                                    ipType = ipParameter.Value;
                                                                }
                                                            }
                                                        }
                                                    }
                                                    Trace.WriteLine(DateTime.Now + "\t key ipaddress child nodes " + ipParameter.getKey() + " value " + ipParameter.Value);
                                                }

                                            }

                                        }
                                    }
                                    if (host.os == OStype.Windows)
                                    {
                                        if (host.nicHash["nic_mac"] != null && host.nicHash["vendor"] != null)
                                        {
                                            Trace.WriteLine(DateTime.Now + "\t vendor name " + host.nicHash["vendor"].ToString());
                                            if (host.nicHash["vendor"].ToString().ToUpper().Contains("MICROSOFT"))
                                            {
                                                if (host.nicHash["devicename"] != null)
                                                {
                                                    Trace.WriteLine(DateTime.Now + "\t Printing the name of device " + host.nicHash["devicename"].ToString());
                                                    if (host.nicHash["devicename"].ToString().ToUpper().Contains("NETWORK"))
                                                    {
                                                        Trace.WriteLine(DateTime.Now + "\t Added nic to hash ");
                                                        host.nicHash.Add("ip", ip);
                                                        host.nicHash.Add("mask", subnetmask);
                                                        host.nicHash.Add("gateway", gateway);
                                                        host.nicHash.Add("dnsip", dnsip);
                                                        host.nicHash.Add("dhcp", dhcp);
                                                        host.nicHash.Add("Selected", "yes");
                                                        host.nicHash.Add("network_label", "Network adapter " + nicCount.ToString());
                                                        host.nicHash.Add("iptype", ipType);
                                                        Trace.WriteLine(DateTime.Now + "\t Printing the iptype " + ipType);
                                                        host.nicList.list.Add(host.nicHash);
                                                        nicCount++;
                                                    }
                                                }
                                            }
                                            else
                                            {
                                                host.nicHash.Add("ip", ip);
                                                host.nicHash.Add("mask", subnetmask);
                                                host.nicHash.Add("gateway", gateway);
                                                host.nicHash.Add("dnsip", dnsip);
                                                host.nicHash.Add("dhcp", dhcp);
                                                host.nicHash.Add("Selected", "yes");
                                                host.nicHash.Add("iptype", ipType);
                                                Trace.WriteLine(DateTime.Now + "\t Printing the iptype " + ipType);
                                                host.nicHash.Add("network_label", "Network adapter " + nicCount.ToString());
                                                host.nicList.list.Add(host.nicHash);
                                                nicCount++;
                                            }
                                        }
                                    }
                                    else
                                    {
                                        if (host.nicHash["nic_mac"] != null )
                                        {
                                            host.nicHash.Add("ip", ip);
                                            host.nicHash.Add("mask", subnetmask);
                                            host.nicHash.Add("gateway", gateway);
                                            host.nicHash.Add("dnsip", dnsip);
                                            host.nicHash.Add("dhcp", dhcp);
                                            host.nicHash.Add("Selected", "yes");
                                            host.nicHash.Add("network_label", "Network adapter " +nicCount.ToString() );
                                            host.nicList.list.Add(host.nicHash);
                                            nicCount++;
                                        }
                                    }
                                    Trace.WriteLine(DateTime.Now + "\t Count of nic hash " + host.nicList.list.Count.ToString());
                                }
                            }
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                System.FormatException nht = new FormatException("Inner exception", ex);
                Trace.WriteLine(nht.GetBaseException());
                Trace.WriteLine(nht.Data);
                Trace.WriteLine(nht.InnerException);
                Trace.WriteLine(nht.Message);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: ReadHostNode " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
            refHost = host;
            return true;
        }


        internal bool PostTorunFXJob(FunctionRequest functionRequest, Host host)
        {
            Response response = new Response();
            ArrayList functionList = new ArrayList();
            try
            {
                List<ParameterGroup> funcParameterGroups = new List<ParameterGroup>();
                functionList.Add(functionRequest);
                response = PostRequest(functionList);
                string message = response.Message;
                Trace.WriteLine(DateTime.Now + "\t Message " + message);
                string xml = response.ToXML();
                Trace.WriteLine(DateTime.Now + "\t Printing xml " + xml);
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
                Trace.WriteLine("Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
            refHost = host;
            return true;
        }

        internal Response PostRequestMbrpath(FunctionRequest functionRequest, Host host)
        {
            Response response = new Response();
            ArrayList functionList = new ArrayList();
            try
            {
                List<ParameterGroup> funcParameterGroups = new List<ParameterGroup>();
                functionList.Add(functionRequest);
                response = PostRequest(functionList);
                string message = response.Message;
                Trace.WriteLine(DateTime.Now + "\t Message " + message);
                string xml = response.ToXML();
                Trace.WriteLine(DateTime.Now + "\t Printing xml " + xml);
               
                foreach (object obj1 in response.Body.ChildList)
                {
                    if (obj1 is Function)
                    {
                        Function func = (Function)obj1;
                        if (String.Compare(func.Name, functionRequest.Name, true) != 0 || func.FunctionResponse == null)
                            continue;
                        Trace.WriteLine(DateTime.Now + "\t Count of parametergroup " + func.FunctionResponse.ChildList.Count.ToString());

                        foreach (object obj2 in func.FunctionResponse.ChildList)
                        {
                            if (obj2 is Parameter)
                            {

                                Parameter para = obj2 as Parameter;

                                if (para.getKey().ToUpper() == "MBRPATH")
                                {
                                    host.mbr_path = para.Value;
                                    Trace.WriteLine(DateTime.Now + "\t Printing the Mbr path " + para.Value);
                                }
                            }
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Caught exception " + ex.Message);
            }
            refHost = host;
            return response;
        }

        private bool PostRequestForRemoveHost(FunctionRequest functionRequest)
        {
            Response response = new Response();
            ArrayList functionList = new ArrayList();
            try
            {
                List<ParameterGroup> funcParameterGroups = new List<ParameterGroup>();
                functionList.Add(functionRequest);
                response = PostRequest(functionList);
                string message = response.Message;
                Trace.WriteLine(DateTime.Now + "\t Message at response " + message);
                foreach (object obj1 in response.Body.ChildList)
                {
                    if (obj1 is Function)
                    {
                        Function func = (Function)obj1;
                        if(func.Message != null && func.Message.Length != 0)
                        {
                            Trace.WriteLine(DateTime.Now + "\t Message at body " + func.Message);
                            if (func.Message.ToUpper() == "SUCCESS")
                            {
                                return true;
                            }
                            else
                            {
                                return false;
                            }
                        }
                    }
                }
                //if (message.ToUpper() == "SUCCESS")
                //{
                 
                //    return true;
                //}
                //else
                //{
                //    return false;
                //}

            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Caught exception " + ex.Message);
            }
            
            return true;
        }

        internal Response PostRequestForRefreshHost(FunctionRequest functionRequest, Host host)
        {
            Response response = new Response();
            ArrayList functionList = new ArrayList();
            try
            {
                List<ParameterGroup> funcParameterGroups = new List<ParameterGroup>();
                functionList.Add(functionRequest);
                response = PostRequest(functionList);
                string message = response.Message;
                Trace.WriteLine(DateTime.Now + "\t Message " + message);
                string xml = response.ToXML();
                Trace.WriteLine(DateTime.Now + "\t Printing xml " + xml);
                foreach (object obj1 in response.Body.ChildList)
                {
                    if (obj1 is Function)
                    {
                        Function func = (Function)obj1;
                        if (String.Compare(func.Name, functionRequest.Name, true) != 0 || func.FunctionResponse == null)
                            continue;
                        Trace.WriteLine(DateTime.Now + "\t Count of parametergroup " + func.FunctionResponse.ChildList.Count.ToString());

                        foreach (object obj2 in func.FunctionResponse.ChildList)
                        {
                            if (obj2 is Parameter)
                            {

                                Parameter para = obj2 as Parameter;

                                if (para.getKey().ToUpper() == "REQUESTID")
                                {
                                    host.requestId = para.Value;
                                    Trace.WriteLine(DateTime.Now + "\t Printing the request id " + para.Value);
                                }
                            }
                        }
                    }
                }
                
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Caught exception " + ex.Message);
            }
            refHost = host;
            return response;
        }

        internal static Response PostRequest(ArrayList functionList)
        {
            Response response = new Response();
            int id = 1;
            try
            {

                foreach (FunctionRequest funcRequest in functionList)
                {
                    funcRequest.Id = id.ToString();
                    id++;
                }
                if (WinTools.Https == false)
                {
                    Trace.WriteLine(DateTime.Now + "\t Going with Http");
                    response = CXAPI.post(functionList, WinTools.GetcxIP(), int.Parse(WinTools.GetcxPortNumber().ToString()), "http");
                }
                else
                {
                    Trace.WriteLine(DateTime.Now + "\t Going with Https");
                    response = CXAPI.post(functionList, WinTools.GetcxIP(), int.Parse(WinTools.GetcxPortNumber().ToString()), "https");
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

            }

            return response;
        }



        public bool PostTOFindAnyvoluesAreResized(HostList selectedList, Host masterHost)
        {
            try
            {
                Response response = new Response();
                ArrayList list = new ArrayList();
                Parameter param = new Parameter();
                ParameterGroup paraGroup = new ParameterGroup();
                ParameterGroup hostlistGroup = new ParameterGroup();
                
                ParameterGroup hostGroup = new ParameterGroup();
                FunctionRequest functionRequest = new FunctionRequest();
                Host h = (Host)selectedList._hostList[0];
                functionRequest.Name = "GetResizeOrPauseStatus";
                paraGroup.Id = "HostLists";
               
                int i = 1;
                foreach (Host sourceHost in selectedList._hostList)
                {
                    hostGroup = new ParameterGroup();
                    hostGroup.Id = "HostList_" + i.ToString();
                    param = new Parameter();
                    param.Name = "SourceHostGUID";
                    param.Value = sourceHost.inmage_hostid;
                    hostGroup.addChildObj(param);
                    param = new Parameter();
                    param.Name = "TargetHostGUID";
                    param.Value = masterHost.inmage_hostid;
                    hostGroup.addChildObj(param);
                    paraGroup.addChildObj(hostGroup);
                    i++;
                }
                
                
                
                

                functionRequest.addChildObj(paraGroup);
                Trace.Write(DateTime.Now + "\t Prinitng the remove xml " + functionRequest.ToXML());
                PostRequestForStatusOfVolumeResizePaused(functionRequest,  selectedList);
                selectedList = GetHostlist;

            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");

            }
            return true;
        }

        internal bool PostRequestForStatusOfVolumeResizePaused(FunctionRequest functionRequest, HostList selectedSourceList)
        {
            Response response = new Response();
            ArrayList functionList = new ArrayList();

            try
            {
                List<ParameterGroup> funcParameterGroups = new List<ParameterGroup>();
                functionList.Add(functionRequest);
                response = PostRequest(functionList);
                string message = response.Message;
                foreach (object obj1 in response.Body.ChildList)
                {
                    if (obj1 is Function)
                    {
                        Function func = (Function)obj1;
                        if (String.Compare(func.Name, functionRequest.Name, true) != 0 || func.FunctionResponse == null)
                            continue;
                        Trace.WriteLine(DateTime.Now + "\t Count of parametergroup " + func.FunctionResponse.ChildList.Count.ToString());
                        Trace.WriteLine(DateTime.Now + "\t output " + func.FunctionResponse.ToXML());
                        foreach (object obj2 in func.FunctionResponse.ChildList)
                        {


                            ParameterGroup parameterGroup = obj2 as ParameterGroup;
                            funcParameterGroups.Add(parameterGroup);
                            Trace.WriteLine(DateTime.Now + "\t parameterGroup " + parameterGroup.ToXML());
                            if (obj2 is ParameterGroup)
                            {
                                ParameterGroup parameterGroups = obj2 as ParameterGroup;
                                List<ParameterGroup> listParameterGroup = new List<ParameterGroup>();
                                listParameterGroup.Add(parameterGroups);
                                ReadFunctionToGetStatusOfVolumeResizeOrPausedPairs(listParameterGroup,  selectedSourceList);
                                selectedSourceList = GetHostlist;
                            }
                        }
                    }


                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
            refHostList = selectedSourceList;
            return true;
        }

        internal bool PostRequestForTelemetering(FunctionRequest functionRequest)
        {
            Response response = new Response();
            ArrayList functionList = new ArrayList();
            try
            {
                List<ParameterGroup> funcParameterGroups = new List<ParameterGroup>();
                functionList.Add(functionRequest);
                response = PostRequest(functionList);
                string message = response.Message;
                Trace.WriteLine(DateTime.Now + "\t Response " + response.ToXML().ToString());
                Trace.WriteLine(DateTime.Now + "\t Message " + message);
                if (message.ToUpper() == "SUCCESS")
                {

                    return true;
                }
                else
                {
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
                Trace.WriteLine("Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
            return true;
        }
        //public bool PostForTeleMetering(HostList selectedList, Host masterHost,OperationType operationType)
        //{
        //    Response response = new Response();
        //    Parameter param = new Parameter();
        //    ParameterGroup paraGroup = new ParameterGroup();
        //    ParameterGroup sourceGroup = new ParameterGroup();
        //    FunctionRequest functionRequest = new FunctionRequest();
        //    functionRequest.Name = "UpdateStatusToCX";
        //    if(operationType == OperationType.Recover)
        //    {
        //        param = new Parameter();
        //        param.Name = "PlanType";
        //        param.Value = "Recovery";                
        //    }
        //    else
        //    {
        //        param = new Parameter();
        //        param.Name = "PlanType";
        //        param.Value = "Protection";
        //    }
        //    functionRequest.addChildObj(param);
        //    param = new Parameter();
        //    param.Name = "PlanId";
        //    param.Value = masterHost.planid;
        //    functionRequest.addChildObj(param);


        //    if(operationType == OperationType.Recover)
        //    {
        //        paraGroup.Id = "RecoveryServers";
        //    }
        //    else
        //    {
        //        paraGroup.Id = "ProtectedServers";
        //    }
            
        //    param = new Parameter();
        //    param.Name = "TargetHostId";
        //    param.Value = masterHost.inmage_hostid;
        //    paraGroup.addChildObj(param);
        //    foreach (Host h in selectedList._hostList)
        //    {
        //        sourceGroup = new ParameterGroup();
        //        sourceGroup.Id = h.inmage_hostid;
        //        Parameter sourceParam = new Parameter();
        //        sourceParam.Name = "SourceHostId";
        //        sourceParam.Value = h.inmage_hostid;
        //        sourceGroup.addChildObj(sourceParam);
                
        //        sourceParam = new Parameter();
        //        sourceParam.Name = "Status";
        //        sourceParam.Value = "Intiated";
        //        sourceGroup.addChildObj(sourceParam);

        //        sourceParam = new Parameter();
        //        sourceParam.Name = "ErrorStep";
        //        sourceParam.Value = "";
        //        sourceGroup.addChildObj(sourceParam);

        //        sourceParam = new Parameter();
        //        sourceParam.Name = "ErrorMessage";
        //        sourceParam.Value = "";
        //        sourceGroup.addChildObj(sourceParam);


        //        paraGroup.addChildObj(sourceGroup);

        //    }

        //    functionRequest.addChildObj(paraGroup);

        //    return PostRequestForTelemetering(functionRequest);          
            
        //}

        public bool PostRemove(HostList selectedList, Host masterHost, ref string requestId)
        {
            try
            {
                Response response = new Response();
                ArrayList list = new ArrayList();
                Parameter param = new Parameter();
                ParameterGroup paraGroup = new ParameterGroup();
                ParameterGroup vxGroup = new ParameterGroup();
                ParameterGroup fxGroup = new ParameterGroup();
                ParameterGroup hostGroup = new ParameterGroup();
                FunctionRequest functionRequest = new FunctionRequest();
                Host h = (Host)selectedList._hostList[0];
                functionRequest.Name = "RemoveProtection";
                paraGroup.Id = "Plan1";
                param.Name = "PlanName";
                param.Value = h.plan;
                paraGroup.addChildObj(param);
                vxGroup.Id = "vx";
                int i = 1;
                foreach (Host sourceHost in selectedList._hostList)
                {
                    hostGroup = new ParameterGroup();
                    hostGroup.Id = "host" + i.ToString();
                    param = new Parameter();
                    param.Name = "SourceHostId";
                    param.Value = sourceHost.inmage_hostid;
                    hostGroup.addChildObj(param);
                    param = new Parameter();
                    param.Name = "TargetHostId";
                    param.Value = masterHost.inmage_hostid;
                    hostGroup.addChildObj(param);
                    param = new Parameter();
                    param.Name = "DeleteRetention";
                    param.Value = "True";
                    hostGroup.addChildObj(param);
                    vxGroup.addChildObj(hostGroup);
                    i++;
                }
                paraGroup.addChildObj(vxGroup);
                i = 1;
                fxGroup.Id = "fx";
                foreach (Host sourceHost in selectedList._hostList)
                {
                    hostGroup = new ParameterGroup();
                    hostGroup.Id = "host" + i.ToString();
                    param = new Parameter();
                    param.Name = "SourceHostId";
                    param.Value = sourceHost.inmage_hostid;
                    hostGroup.addChildObj(param);
                    param = new Parameter();
                    param.Name = "TargetHostId";
                    param.Value = sourceHost.inmage_hostid;
                    hostGroup.addChildObj(param);
                    fxGroup.addChildObj(hostGroup);
                    i++;
                }
                hostGroup = new ParameterGroup();
                hostGroup.Id = "host" + i.ToString();
                param = new Parameter();
                param.Name = "SourceHostId";
                param.Value = masterHost.inmage_hostid;
                hostGroup.addChildObj(param);
                param = new Parameter();
                param.Name = "TargetHostId";
                param.Value = masterHost.inmage_hostid;
                hostGroup.addChildObj(param);
                fxGroup.addChildObj(hostGroup);
                paraGroup.addChildObj(fxGroup);

                functionRequest.addChildObj(paraGroup);
                Trace.Write(DateTime.Now + "\t Prinitng the remove xml " + functionRequest.ToXML());
                if (PostRemoveRequest(functionRequest, ref requestId) == true)
                {
                    Trace.WriteLine(DateTime.Now + "\t Successfully deleted the pairs");
                    return true;
                }
                else
                {
                    Trace.WriteLine(DateTime.Now + "\t Failed to delet pairs");
                    return false;
                }
               
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");

            }
            return true;
        }
        public bool PostRemoveRequest(FunctionRequest functionRequest, ref string requestId)
        {
            Response response = new Response();
            ArrayList functionList = new ArrayList();

            try
            {
                List<Parameter> funcParameterGroups = new List<Parameter>();
                functionList.Add(functionRequest);
                response = PostRequest(functionList);
                string message = response.Message;
                foreach (object obj1 in response.Body.ChildList)
                {
                    if (obj1 is Function)
                    {
                        Function func = (Function)obj1;
                        if (String.Compare(func.Name, functionRequest.Name, true) != 0 || func.FunctionResponse == null)
                            continue;
                        Trace.WriteLine(DateTime.Now + "\t Count of parametergroup " + func.FunctionResponse.ChildList.Count.ToString());
                        if (func.Message != null && func.Message.Length != 0)
                        {
                            if (func.Message == "Success")
                            {
                                Trace.WriteLine(DateTime.Now + "\t Successfully deleted the pairs " + func.Message);

                                foreach (object obj2 in func.FunctionResponse.ChildList)
                                {


                                    if (obj2 is Parameter)
                                    {
                                        Parameter paraMeterhostid = obj2 as Parameter;
                                        if (paraMeterhostid.Name.ToUpper() == "REQUESTID" && paraMeterhostid.Value.Length != 0)
                                        {
                                            requestId = paraMeterhostid.Value;
                                            Trace.WriteLine(DateTime.Now + "\t Request id is " + paraMeterhostid.Value);
                                        }
                                    }
                                }
                                return true;
                            }
                            else
                            {
                                Trace.WriteLine(DateTime.Now + "\t Failed to delete the pairs " + func.Message);
                                return false;
                            }
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: PostRemoveRequest " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");

            }
            return true;
        }


        public bool PostRemoveRetentionStatus(string requestId)
        {
            Response response = new Response();
            ArrayList list = new ArrayList();
            Parameter param = new Parameter();
            ParameterGroup paraGroup = new ParameterGroup();
            FunctionRequest functionRequest = new FunctionRequest();

            functionRequest.Name = "GetRequestStatus";
            paraGroup.Id = "RequestIdList";
            param.Name = "RequestId";
            param.Value = requestId;
            paraGroup.ChildList.Add(param);
            functionRequest.addChildObj(paraGroup);
            Trace.WriteLine(DateTime.Now + "\t paragroup " + functionRequest.ToXML());
            if (PostRequestforStatus(functionRequest) == true)
            {
                Trace.WriteLine(DateTime.Now + "\t Status is completed ");
                return true;
            }
            else
            {
                Trace.WriteLine(DateTime.Now + "\t Status is failed");
                return false;
            }
        }

        public bool PostRequestforStatus(FunctionRequest functionRequest)
        {
            Response response = new Response();
            ArrayList functionList = new ArrayList();

            try
            {
                List<ParameterGroup> funcParameterGroups = new List<ParameterGroup>();
                functionList.Add(functionRequest);
                Trace.WriteLine(DateTime.Now + "\t Request " + functionRequest.ToXML().ToString());
                response = PostRequest(functionList);
                string message = response.Message;
                Trace.WriteLine(DateTime.Now + "\t response " + response.ToXML().ToString());
                foreach (object obj1 in response.Body.ChildList)
                {
                    if (obj1 is Function)
                    {
                        Function func = (Function)obj1;
                        if (String.Compare(func.Name, functionRequest.Name, true) != 0 || func.FunctionResponse == null)
                            continue;
                        Trace.WriteLine(DateTime.Now + "\t Count of parametergroup " + func.FunctionResponse.ChildList.Count.ToString());
                        Trace.WriteLine(DateTime.Now + "\t child list " + func.FunctionResponse.ToXML().ToString());
                        foreach (object obj2 in func.FunctionResponse.ChildList)
                        {
                            if (obj2 is Parameter)
                            {
                                Parameter para = obj2 as Parameter;
                                Trace.WriteLine(DateTime.Now + "\t printing name " + para.Name + " \t value  " + para.Value);
                                if (para.getKey().ToUpper() == "STATUS")
                                {
                                    if (para.Value.ToString().ToUpper() == "PENDING")
                                    {
                                        Trace.WriteLine(DateTime.Now + "\t Status is pending");
                                        return false;
                                    }
                                    else if (para.Value.ToString().ToUpper() == "INPROGRESS")
                                    {
                                        Trace.WriteLine(DateTime.Now + "\t Status is In progress");
                                        return false;
                                    }
                                    else
                                    {
                                        Trace.WriteLine(DateTime.Now + "\t Status is " + para.Value);
                                    }
                                }
                            }
                        }
                    }
                }
            }
            catch (Exception ex)
            {
            }
            return true;
        }


        public bool PostToGetVolumeResizeInfo(ArrayList sourceList, ArrayList list)
        {
            Response response = new Response();
            Parameter param = new Parameter();
            FunctionRequest functionRequest = new FunctionRequest();
            functionRequest.Name = "GetResyncPendingVolumeResizePairs";
            int i = 1;
            foreach (string s in list)
            {
                param = new Parameter();
                param.Name = "PlanId" + i.ToString();
                param.Value = s;
                functionRequest.addChildObj(param);
                i++;
            }

            Trace.WriteLine(DateTime.Now + "\t paragroup " + functionRequest.ToXML());
            FunctionRequest fun = new FunctionRequest();
            PostRequestForVolumeResizePairs(functionRequest,  sourceList);
            sourceList = GetArrayList;
            refArraylist = sourceList;
            return true;
        }

        internal bool PostRequestForVolumeResizePairs(FunctionRequest functionRequest, ArrayList sourceList)
        {
            Response response = new Response();
            ArrayList functionList = new ArrayList();

            try
            {
                List<ParameterGroup> funcParameterGroups = new List<ParameterGroup>();
                functionList.Add(functionRequest);
                response = PostRequest(functionList);
                string message = response.Message;
                foreach (object obj1 in response.Body.ChildList)
                {
                    if (obj1 is Function)
                    {
                        Function func = (Function)obj1;
                        if (String.Compare(func.Name, functionRequest.Name, true) != 0 || func.FunctionResponse == null)
                            continue;
                        Trace.WriteLine(DateTime.Now + "\t Count of parametergroup " + func.FunctionResponse.ChildList.Count.ToString());
                        Trace.WriteLine(DateTime.Now  + "\t output " + func.FunctionResponse.ToXML());
                        foreach (object obj2 in func.FunctionResponse.ChildList)
                        {

                        
                            if (obj2 is ParameterGroup)
                            {

                                ParameterGroup parameterGroup = obj2 as ParameterGroup;
                                funcParameterGroups.Add(parameterGroup);
                                Trace.WriteLine(DateTime.Now + "\t parameterGroup " + parameterGroup.ToXML());
                                if (obj2 is ParameterGroup)
                                {
                                    ParameterGroup parameterGroups = obj2 as ParameterGroup;
                                    List<ParameterGroup> listParameterGroup = new List<ParameterGroup>();
                                    listParameterGroup.Add(parameterGroups);
                                    ReadFunctionGroupForPlanid(listParameterGroup,  sourceList);
                                    sourceList = GetArrayList;
                                }
                                
                            }
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
                Trace.WriteLine("Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
            refArraylist = sourceList;
            return true;
        }

        internal static bool ReadFunctionToGetStatusOfVolumeResizeOrPausedPairs(List<ParameterGroup> funcParameterGroups, HostList list)
        {
            try
            {
                string id = null;
                try
                {


                    foreach (ParameterGroup group in funcParameterGroups)
                    {


                        Trace.WriteLine(DateTime.Now + "\t Printing the group id " + group.Id.ToString());
                        foreach (object planList in group.ChildList)
                        {
                            if (planList is Parameter)
                            {



                                Parameter paraMeterhostid = planList as Parameter;
                                if (paraMeterhostid.Name.ToUpper() == "SOURCEHOSTID")
                                {

                                    id = paraMeterhostid.Value;

                                }

                                if (paraMeterhostid.Name.ToUpper() == "STATUS")
                                {
                                    foreach (Host h in list._hostList)
                                    {
                                        if (h.inmage_hostid.ToUpper() == id.ToUpper())
                                        {
                                            if (paraMeterhostid.Value == "TRUE")
                                            {
                                                h.resizedOrPaused = true;
                                            }
                                            else
                                            {
                                                h.resizedOrPaused = false;
                                            }

                                        }
                                    }
                                }


                            }
                        }

                    }

                }
                catch (Exception ex)
                {
                    Trace.WriteLine(DateTime.Now + "\t Failed to read status name " + ex.Message);
                }
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed to read status name " + ex.Message);
            }
            refHostList = list;
            
            return true;
        }


        internal static bool ReadFunctionGroupForPlanid(List<ParameterGroup> funcParameterGroups, ArrayList list)
        {
            try
            {
                string planname = null;
                try
                {
                    

                    foreach (ParameterGroup group in funcParameterGroups)
                    {


                        Trace.WriteLine(DateTime.Now + "\t Printing the group id " + group.Id.ToString());
                        foreach (object planList in group.ChildList)
                        {
                            if (planList is Parameter)
                            {



                                Parameter paraMeterhostid = planList as Parameter;
                                if (paraMeterhostid.Name.ToUpper() == "PLANNAME")
                                {

                                    planname = paraMeterhostid.Value;
                                    Trace.WriteLine(DateTime.Now + "\t planname  ", paraMeterhostid.Value);

                                }


                            }
                        }

                    }

                }
                catch (Exception ex)
                {
                    Trace.WriteLine(DateTime.Now + "\t Failed to read plan name " + ex.Message);
                }
                foreach (ParameterGroup group in funcParameterGroups)
                {
                    Hashtable planHash = new Hashtable();

                    Trace.WriteLine(DateTime.Now + "\t Printing the group id " + group.Id.ToString());
                    foreach (object planList in group.ChildList)
                    {
                        if (planList is ParameterGroup)
                        {
                            ParameterGroup parameterGroups = planList as ParameterGroup;


                        


                            List<ParameterGroup> listParameterGroup = new List<ParameterGroup>();
                            listParameterGroup.Add(parameterGroups);





                            ReadFunctionGroupForHostid(listParameterGroup,  list, planname);
                            list = GetArrayList;

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
                Trace.WriteLine("Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
            refArraylist = list;
            return true;
        }




        internal static bool ReadFunctionGroupForHostid(List<ParameterGroup> funcParameterGroups, ArrayList list, string plananme)
        {
            try
            {
                foreach (ParameterGroup group in funcParameterGroups)
                {
                    Hashtable planHash = new Hashtable();
                    
                        Trace.WriteLine(DateTime.Now + "\t Printing the group id " + group.Id.ToString());
                        foreach (object planList in group.ChildList)
                        {
                            if (planList is Parameter)
                            {



                                Parameter paraMeterhostid= planList as Parameter;
                                if (paraMeterhostid.Name.ToUpper() == "HOSTID")
                                {

                                    planHash.Add("HostId", paraMeterhostid.Value);
                                    Trace.WriteLine(DateTime.Now + "\t hostid ", paraMeterhostid.Value);
                                    planHash.Add("planname", plananme);
                                }
                                

                            }
                        }
                        list.Add(planHash);

                        Trace.WriteLine(DateTime.Now + "\t Count of listt " + list.Count.ToString());

                  
                }
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Exception message " + ex.Message);
            }
            refArraylist = list;
            return true;
        }

        public bool PostToGetClusterInfo(ref ArrayList arrayList)
        {
            Response response = new Response();
            arrayList = new ArrayList();
            FunctionRequest functionRequest = new FunctionRequest();
            functionRequest.Name = "GetClusterInfo";

            Trace.WriteLine(DateTime.Now + "\t paragroup " + functionRequest.ToXML());
            //functionRequest.Name = "GetHostMBRPath";           

            PostRequestOfClusterInfo(functionRequest, ref arrayList);

            return true;
        }
        public bool PostRequestOfClusterInfo(FunctionRequest functionRequest, ref ArrayList array)
        {
            Response response = new Response();
            ArrayList functionList = new ArrayList();

            try
            {
                List<ParameterGroup> funcParameterGroups = new List<ParameterGroup>();
                functionList.Add(functionRequest);
                response = PostRequest(functionList);
                
                string message = response.Message;
                foreach (object obj1 in response.Body.ChildList)
                {
                    if (obj1 is Function)
                    {
                        Function func = (Function)obj1;
                        if (String.Compare(func.Name, functionRequest.Name, true) != 0 || func.FunctionResponse == null)
                            continue;
                        Trace.WriteLine(DateTime.Now + "\t Count of parametergroup " + func.FunctionResponse.ChildList.Count.ToString());

                        foreach (object obj2 in func.FunctionResponse.ChildList)
                        {

                            if (obj2 is ParameterGroup)
                            {
                                ParameterGroup parameterGroup = obj2 as ParameterGroup;
                                funcParameterGroups.Add(parameterGroup);
                                Trace.WriteLine(DateTime.Now + "\t parameterGroup " + parameterGroup.ToXML());
                                if (obj2 is ParameterGroup)
                                {
                                    ParameterGroup parameterGroups = obj2 as ParameterGroup;
                                    List<ParameterGroup> listParameterGroup = new List<ParameterGroup>();
                                    listParameterGroup.Add(parameterGroups);
                                    ReadParameterGropuForClusterInfo(listParameterGroup, ref array);
                                }
                            }
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed to get cluster info");
            }

            return true;
        }

        public static bool ReadParameterGropuForClusterInfo(List<ParameterGroup> funcParameterGroups, ref ArrayList arrayList)
        {
            try
            {


                foreach (ParameterGroup group in funcParameterGroups)
                {
                    Hashtable planHash = new Hashtable();

                    Trace.WriteLine(DateTime.Now + "\t Printing the group id " + group.Id.ToString());
                    foreach (object planList in group.ChildList)
                    {
                        if (planList is Parameter)
                        {



                            Parameter paraMeterhostid = planList as Parameter;
                            if (paraMeterhostid.Name.ToUpper() == "CLUSTERNAME")
                            {

                                planHash.Add("clusterName", paraMeterhostid.Value);
                                Trace.WriteLine(DateTime.Now + "\t clusterName ", paraMeterhostid.Value);

                            }
                            if (paraMeterhostid.Name.ToUpper() == "CLUSTERNODESINMAGEUUID")
                            {

                                planHash.Add("ClusterNodesInmageuuid", paraMeterhostid.Value);
                                Trace.WriteLine(DateTime.Now + "\t ClusterNodesInmageuuid ", paraMeterhostid.Value);

                            }
                            if (paraMeterhostid.Name.ToUpper() == "CLUSTERNODES")
                            {
                                planHash.Add("ClusterNodes", paraMeterhostid.Value);
                                Trace.WriteLine(DateTime.Now + "\t ClusterNodes ", paraMeterhostid.Value);
                            }


                        }
                    }

                    arrayList.Add(planHash);

                    Trace.WriteLine(DateTime.Now + "\t Count of listt " + arrayList.Count.ToString());


                }
            }
            catch (Exception ex)
            {

            }
            return true;
        }
    }
}
