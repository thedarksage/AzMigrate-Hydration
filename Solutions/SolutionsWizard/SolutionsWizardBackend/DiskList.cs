using System;
using System.Collections.Generic;
using System.Text;
using Com.Inmage.Tools;
using System.Collections;
using System.Management;
using System.Diagnostics;
using System.Windows.Forms;
using System.IO;
using System.Xml;
using System.Net;
using HttpCommunication;

namespace Com.Inmage
{
    [Serializable]
    public class DiskList
    {

        internal int SLOTS_PER_ADAPTER = 16;
        private int SCSI_SLOT_SKIP_NUMBER = 7;
        public ArrayList list;
        public ArrayList partitionList;
        public static Host refHost;
        internal static string latestPath = WinTools.Vxagentpath() + "\\vContinuum\\Latest";
        public DiskList()
        {
            list = new ArrayList();
            partitionList = new ArrayList();
        }

        public ArrayList GetDiskList
        {
            get
            {
                return list;
            }
            set
            {
                value = list;
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

        public ArrayList GetPartitionList
        {
            get
            {
                return partitionList;
            }
            set
            {
                value = partitionList;
            }
        }

        public bool SelectDisk(int diskNumber)
        {
            try
            {

                // For both p2v and v2v if user selected disk which is unseleted it will come to here yo make selected value as yes...
                if (diskNumber >= list.Count)
                {
                    return false;
                }

                ((Hashtable)list[diskNumber])["Selected"] = "Yes";
                try
                {
                    if (((Hashtable)list[diskNumber])["selectedbyuser"] != null)
                    {
                        ((Hashtable)list[diskNumber])["selectedbyuser"] = "Yes";
                    }
                    else
                    {
                        ((Hashtable)list[diskNumber]).Add("selectedbyuser", "Yes");
                    }
                }
                catch (Exception ex)
                {
                    Trace.WriteLine(DateTime.Now + "\t Faile dto update user input " + ex.Message);
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


        public bool SelectDiskForRemove(int diskNumber)
        {
            try
            {

                // For both p2v and v2v if user selected disk which is unseleted it will come to here yo make selected value as yes...
                if (diskNumber >= list.Count)
                {
                    return false;
                }

                //((Hashtable)list[diskNumber])["Selected"] = "Yes";
                ((Hashtable)list[diskNumber])["remove"] = "false";
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

        public bool UnselectDisk(int diskNumber)
        {
            try
            {// For both p2v and v2v if user unselected disk which is seleted it will come to here yo make selected value as no
                if (diskNumber >= list.Count)
                {
                    return false;
                }
                Debug.WriteLine("Deleting the disk from the host");
                ((Hashtable)list[diskNumber])["Selected"] = "No";
                try
                {
                    if (((Hashtable)list[diskNumber])["selectedbyuser"] != null)
                    {
                        ((Hashtable)list[diskNumber])["selectedbyuser"] = "No";
                    }
                    else
                    {
                        ((Hashtable)list[diskNumber]).Add("selectedbyuser", "No");
                    }
                }
                catch (Exception ex)
                {
                    Trace.WriteLine(DateTime.Now + "\t Faile dto update user input " + ex.Message);
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


        public bool UnselectDiskForRemove(int diskNumber)
        {
            try
            {// For both p2v and v2v if user unselected disk which is seleted it will come to here yo make selected value as no
                if (diskNumber >= list.Count)
                {
                    return false;
                }
                Debug.WriteLine("Deleting the disk from the host");
                //((Hashtable)list[diskNumber])["Selected"] = "No";
                ((Hashtable)list[diskNumber])["remove"] = "True";
               
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

        public bool AddScsiNumbers()
        {
            try
            {
                // Only in case of p2v we will add scsi numbers by getting from the wmi call.
                // In case of v2v scripts will give info in the sourcexml.xml.
                int slotNumber = 0;
                int adapterCount = 0;

                foreach (Hashtable disk in list)
                {


                    if (disk["Selected"].ToString() == "Yes")
                    {
                        if (slotNumber == SLOTS_PER_ADAPTER )
                        {
                            adapterCount++;

                            slotNumber = 0;

                        }
                        

                        //Don't use scsi slot #7. Thta is reserved for SCSI internal use.
                        if (slotNumber == SCSI_SLOT_SKIP_NUMBER)
                            slotNumber++;

                        disk["scsi_mapping_vmx"] = adapterCount.ToString() + ":" + slotNumber.ToString();
                        disk["scsi_adapter_number"] = adapterCount;
                        disk["scsi_slot_number"] = slotNumber;
                        slotNumber++;


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


        public bool AppendScsiForP2vAdditionOfDisk(int index)
        {
            try
            {

                // In case of p2v addition of disk we need to add same scsi numbers to already protected disks
                // So that we have added a new method.
                int slotNumber = index;
                int adapterCount = 0;

                if (index >= 16 && index <= 31)
                {
                    slotNumber = index - 16;
                    adapterCount = 1;
                }
                else if (index >= 32 && index <= 47)
                {
                    slotNumber = index - 32;
                    adapterCount = 2;
                }
                else if (index > 47)
                {
                    slotNumber = index - 48;
                    adapterCount = 3;
                }
               

                foreach (Hashtable disk in list)
                {


                    if (disk["Selected"].ToString() == "Yes")
                    {
                        if (disk["already_map_vmx"].ToString() == "no" )
                        {
                            if (slotNumber == SLOTS_PER_ADAPTER)
                            {
                                adapterCount++;

                                slotNumber = 0;

                            }


                            //Don't use scsi slot #7. Thta is reserved for SCSI internal use.
                            if (slotNumber == SCSI_SLOT_SKIP_NUMBER)
                                slotNumber++;
                            bool allotValue = false;
                            /*foreach (string scsivmx in scsiList)
                            {
                                if (scsivmx == adapterCount.ToString() + ":" + slotNumber.ToString())
                                {
                                    slotNumber++;
                                    allotValue = true;
                                    disk["scsi_mapping_vmx"] = adapterCount.ToString() + ":" + slotNumber.ToString();
                                    disk["scsi_adapter_number"] = adapterCount;
                                    disk["scsi_slot_number"] = slotNumber;
                                    slotNumber++;
                                }

                            }*/

                            try
                            {
                                foreach (Hashtable disks in list)
                                {
                                    if (disks["scsi_mapping_vmx"] != null)
                                    {
                                        if (disks["scsi_mapping_vmx"].ToString() == adapterCount.ToString() + ":" + slotNumber.ToString())
                                        {
                                            if (slotNumber == SLOTS_PER_ADAPTER)
                                            {
                                                adapterCount++;

                                                slotNumber = 0;

                                            }
                                            slotNumber++;
                                            if (slotNumber == SCSI_SLOT_SKIP_NUMBER)
                                            {
                                                slotNumber++;                                                
                                            }                                            
                                        }
                                    }
                                }
                            }
                            catch (Exception ex)
                            {
                                Trace.WriteLine(DateTime.Now + "\t Failed to add scsi VMX " + ex.Message);
                            }
                            if (allotValue == false)
                            {
                                disk["scsi_mapping_vmx"] = adapterCount.ToString() + ":" + slotNumber.ToString();
                                disk["scsi_adapter_number"] = adapterCount;
                                disk["scsi_slot_number"] = slotNumber;
                                Trace.WriteLine(DateTime.Now + "\t scsi mapping vmx " + disk["scsi_mapping_vmx"].ToString() + "\t disk name " + disk["Name"].ToString());
                                slotNumber++;
                                if (slotNumber == SCSI_SLOT_SKIP_NUMBER)
                                {
                                    slotNumber++;
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
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;

            }
            return true;
        }



        public bool Checkscsivmx(Hashtable hash, ArrayList list, ref int slotnumber, ref int adaptornumer)
        {
            foreach (Hashtable hashes in list)
            {
                
                Trace.WriteLine(DateTime.Now + "\t Printing seelcted and protected status of the disk " + hashes["Selected"].ToString() + "\t protected " + hashes["Protected"].ToString());
                if (hashes["Selected"].ToString() == "Yes" || hashes["Protected"].ToString() == "yes")
                {
                    if (hash["scsi_mapping_vmx"] != null && hashes["scsi_mapping_vmx"] != null)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Came here either disk is selected for protection or disk is already protected ");
                        if (hash["disk_signature"] != null && hashes["disk_signature"] != null)
                        {
                            Trace.WriteLine(DateTime.Now + "\t SCSCI mapping vmx " + hash["scsi_mapping_vmx"].ToString() + "\t from list " + hashes["scsi_mapping_vmx"].ToString() + "\t Names " + hash["disk_signature"].ToString() + " from List " + hashes["disk_signature"].ToString());
                            if (hash["scsi_mapping_vmx"].ToString() == hashes["scsi_mapping_vmx"].ToString() && (hash["disk_signature"].ToString() != hashes["disk_signature"].ToString()))
                            {
                                Trace.WriteLine(DateTime.Now + "\t Came here becuae both scsi mapping vms and disk signatures are matched.");
                                if (slotnumber == SLOTS_PER_ADAPTER)
                                {
                                    adaptornumer++;

                                    slotnumber = 0;

                                }
                                if (slotnumber == SCSI_SLOT_SKIP_NUMBER)
                                {
                                    slotnumber++;
                                }
                                hash["scsi_mapping_vmx"] = adaptornumer.ToString() + ":" + slotnumber.ToString();
                                hash["scsi_adapter_number"] = adaptornumer;
                                hash["scsi_slot_number"] = slotnumber;
                                slotnumber++;
                                Trace.WriteLine(DateTime.Now + "\t disk scsi mapping and matched and names didn't matched ");
                                Trace.WriteLine(DateTime.Now + "\t scsi mapping vmx " + hash["scsi_mapping_vmx"].ToString() + "\t disk name " + hash["Name"].ToString());
                                Checkscsivmx(hash, list, ref slotnumber, ref adaptornumer);

                            }
                        }
                        else
                        {
                            Trace.WriteLine(DateTime.Now + "\t SCSCI mapping vmx " + hash["scsi_mapping_vmx"].ToString() + "\t from list " + hashes["scsi_mapping_vmx"].ToString() + "\t Names " + hash["Name"].ToString() + " from ;ist " + hashes["Name"].ToString());
                            if ((hash["scsi_mapping_vmx"].ToString() == hashes["scsi_mapping_vmx"].ToString()) && (hash["Name"].ToString() != hashes["Name"].ToString()))
                            {
                                if (slotnumber == SLOTS_PER_ADAPTER)
                                {
                                    adaptornumer++;

                                    slotnumber = 0;

                                }
                                if (slotnumber == SCSI_SLOT_SKIP_NUMBER)
                                {
                                    slotnumber++;
                                }
                                hash["scsi_mapping_vmx"] = adaptornumer.ToString() + ":" + slotnumber.ToString();
                                hash["scsi_adapter_number"] = adaptornumer;
                                hash["scsi_slot_number"] = slotnumber;
                                slotnumber++;
                                Trace.WriteLine(DateTime.Now + "\t disk scsi mapping and matched and names didn't matched ");
                                Trace.WriteLine(DateTime.Now + "\t scsi mapping vmx " + hash["scsi_mapping_vmx"].ToString() + "\t disk name " + hash["Name"].ToString());
                                Checkscsivmx(hash, list, ref slotnumber, ref adaptornumer);

                            }
                        }
                    }
                }
            }

            return true;
        }


        public bool AppendScsiForP2vAdditionOfDiskFinal(int index)
        {
            try
            {

                // This is also in case of p2v addition of disk only after user selects the disks...and going for the protection..
                int slotNumber = index;
                int adapterCount = 0;


                if (index >= 16 && index <= 31)
                {
                    slotNumber = index - 16;
                    adapterCount = 1;
                }
                else if (index >= 32 && index <= 47)
                {
                    slotNumber = index - 32;
                    adapterCount = 2;
                }
                else if (index > 47)
                {
                    slotNumber = index - 48;
                    adapterCount = 3;
                }



                foreach (Hashtable disk in list)
                {


                    if (disk["Selected"].ToString() == "Yes" && disk["Protected"].ToString() == "no")
                    {


                        if (slotNumber == SLOTS_PER_ADAPTER)
                        {
                            adapterCount++;

                            slotNumber = 0;

                        }


                        //Don't use scsi slot #7. Thta is reserved for SCSI internal use.
                        if (slotNumber == SCSI_SLOT_SKIP_NUMBER)
                            slotNumber++;

                        disk["scsi_mapping_vmx"] = adapterCount.ToString() + ":" + slotNumber.ToString();
                        disk["scsi_adapter_number"] = adapterCount;
                        disk["scsi_slot_number"] = slotNumber;
                        slotNumber++;
                        Checkscsivmx(disk, list, ref slotNumber, ref adapterCount);
                        
                        //try
                        //{
                        //    foreach (Hashtable disks in list)
                        //    {
                        //        if (disks["scsi_mapping_vmx"] != null)
                        //        {
                        //            if (disks["scsi_mapping_vmx"].ToString() == adapterCount.ToString() + ":" + slotNumber.ToString())
                        //            {
                        //                if (slotNumber == SLOTS_PER_ADAPTER)
                        //                {
                        //                    adapterCount++;

                        //                    slotNumber = 0;

                        //                }
                        //                if (slotNumber == SCSI_SLOT_SKIP_NUMBER)
                        //                    slotNumber++;
                        //                disk["scsi_mapping_vmx"] = adapterCount.ToString() + ":" + slotNumber.ToString();
                        //                disk["scsi_adapter_number"] = adapterCount;
                        //                disk["scsi_slot_number"] = slotNumber;
                        //                slotNumber++;
                        //                //CheckSCSIVMX(disk, list, ref slotNumber, ref adapterCount);
                        //            }
                        //        }
                        //    }
                        //}
                        //catch (Exception ex)
                        //{
                        //    Trace.WriteLine(DateTime.Now + "\t Failed to add scsi VMX " + ex.Message);
                        //}
                        Trace.WriteLine(DateTime.Now + " \t printing slot numbers " + adapterCount.ToString() + ":" + slotNumber.ToString());
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
        public bool GetPartitionsFreeSpace(Host inHost)
        {
          
           /* if (WinPreReqs.IpReachable(inHost.ip) == false)
            {
                return false;
            
            }*/
            // Using cxcli call number 48 we will get volumes which can be available for the retention..
            Trace.WriteLine("Entered:GetPartitionsFreeSpace() for collecting info...");
            string _xmlString = "";
            try
            {
               
                String postData1 = "&operation=";

                // xmlData would be send as the value of variable called "xml"
                // This data would then be wrtitten in to a file callied input.xml
                // on CX and then given as argument to the cx_cli.php script.
                int operation = 48;
                String postData2 = "&userfile='input.xml'&xml=" + _xmlString;
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
                    return false;
                }
                string responseFromServer = reply.data;
                
                if(inHost.ip != null)
                {
                    if (inHost.ip.Contains(","))
                    {
                        bool foundIp = false;
                        string[] ips = inHost.ip.Split(',');
                        foreach (string ip in ips)
                        {
                            if (responseFromServer.Contains(ip))
                            {

                                inHost.ip = ip;
                                foundIp = true;
                            }
                        }
                        if (foundIp == false)
                        {
                            Trace.WriteLine(" Cx doesn't have info about " + inHost.displayname + inHost.ip);
                            return false;
                        }
                    }
                    else
                    {
                        if (!responseFromServer.Contains(inHost.ip))
                        {

                            Trace.WriteLine(" Cx doesn't have info about " + inHost.displayname + inHost.ip);
                            return false;

                        }
                    }
                }


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
                XmlNodeList hostNodeList = xDocument.SelectNodes(".//host");

                foreach (XmlNode hostNode in hostNodeList)
                {
                    //if (hostNode.Attributes["id"] != null && hostNode.Attributes["id"].Value.Length != 0)
                    //{
                    //    Trace.WriteLine(DateTime.Now + "\t host id " + hostNode.Attributes["id"].Value.ToString());
                    //}
                    //MessageBox.Show("Host node count" + hostNodeList.Count);
                    //Trace.WriteLine(DateTime.Now + "\t Printing Inner text " + hostNode.ChildNodes.Item(0).InnerText.ToString());
                    if (hostNode.ChildNodes.Item(1).InnerText.ToString() == inHost.ip)
                    {


                        XmlNode logicalVolumesNodeList = hostNode.ChildNodes.Item(2);

                        if (logicalVolumesNodeList.HasChildNodes)
                        {
                            XmlNodeList volumeNodeList = logicalVolumesNodeList.ChildNodes;


                            foreach (XmlNode volumeNode in volumeNodeList)
                            {
                                if (volumeNode.HasChildNodes)
                                {
                                    if (volumeNode.ChildNodes.Item(0).InnerText == "" || volumeNode.ChildNodes.Item(1).InnerText == "")
                                    {
                                        Trace.WriteLine("Unable to get list of drives from cx for host " + inHost.displayname);

                                        //return false;

                                    }


                                    Hashtable partitionHash = new Hashtable();

                                    partitionHash.Add("Name", volumeNode.ChildNodes.Item(0).InnerText);
                                    partitionHash.Add("FreeSpace", volumeNode.ChildNodes.Item(1).InnerText);
                                    //diskHash.Add("DeviceID", queryObj["Caption"]);
                                    Trace.WriteLine("prnting paritions and free spacess....." + volumeNode.ChildNodes.Item(0).InnerText + "   " + volumeNode.ChildNodes.Item(1).InnerText);
                                    // MessageBox.Show("prnting paritions and free spacess....." + queryObj["Caption"].ToString() + queryObj["FreeSpace"].ToString());
                                    partitionList.Add(partitionHash);
                                }

                            }
                        }                  
                    }  
                
                }

            }
            catch( Exception e)
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

                return false;
            }
            refHost = inHost;
            return true;

        }

        internal bool GetDisksAndPartitions(RemoteWin remote)
        {
                   

            try
            {


                //In case of p2v we are getting disks info and the scsi number and bootable disk info using wmi
                ObjectQuery query = new ObjectQuery(
                    "SELECT * FROM Win32_DiskDrive where size > 0");

                ManagementObjectSearcher searcher =
                    new ManagementObjectSearcher(remote.scope, query);
                int diskNum = 1;                
                foreach (ManagementObject queryObj in searcher.Get())
                {
                    
                    Hashtable diskHash = new Hashtable();
                    //diskHash.Add("DeviceID", queryObj["Caption"]);
                    //tring to find out bootable disk.
                    ObjectQuery queryBootdisk = new ObjectQuery(
                      "SELECT * FROM Win32_DiskPartition where bootable!=null ");
                  //  MessageBox.Show("Diskdrive index is:" + queryObj["Index"].ToString());
                    ManagementObjectSearcher searcher1 =
                        new ManagementObjectSearcher(remote.scope, queryBootdisk);

                    bool IsBootable = false;
                    foreach (ManagementObject newqueryObj in searcher1.Get())
                    {
                        
                        if (newqueryObj["Bootable"].ToString().Contains("True"))
                        {
                            if (newqueryObj["diskIndex"].ToString().Equals(queryObj["Index"].ToString()))
                            {
                                Trace.WriteLine(DateTime.Now + "\tBootable disk found at " + queryObj["Index"] + " Name of the disk is" + queryObj["Name"].ToString().Remove(0, 12) + "Size " + queryObj["Size"].ToString());
                                IsBootable = true;
                            }                            
                        }
                    }
                    if (queryObj["SCSIPort"] != null && queryObj["SCSITargetId"] != null && queryObj["SCSILogicalUnit"] != null)
                    {
                        int scsiPort = int.Parse(queryObj["SCSIPort"].ToString());
                        string scsiTargetID = queryObj["SCSITargetId"].ToString();
                        string scsiLogicalUnit = queryObj["SCSILogicalUnit"].ToString();
                        string scsi_mapping_host = scsiPort.ToString() + ":" + scsiTargetID + ":" + scsiLogicalUnit;
                        string totalSectors = queryObj["SectorsPerTrack"].ToString();
                        string totalHeads = queryObj["TotalHeads"].ToString();
                        diskHash.Add("scsi_mapping_host", scsi_mapping_host);
                       // diskHash.Add("totalCylinders", totalCylinders);
                        diskHash.Add("SectorsPerTrack", totalSectors);
                        diskHash.Add("TotalHeads", totalHeads);
                        Trace.WriteLine(DateTime.Now + " \t Printing heads and sectors " +  " " + totalHeads + "  " + totalSectors);
                        //Change size to KBs
                        long sizeBytes = (long)long.Parse(queryObj["Size"].ToString());
                        //  MessageBox.Show(sizeBytes.ToString());
                        string name = queryObj["Name"].ToString();
                        //Cut first 12 chars to get "DRIVE0"  in \\.\PHYSICALDRIVE0
                        name = name.Remove(0, 12);
                        //Round up to next KB if we get decimals
                        long sizeKB = (long)sizeBytes / (1024) + (sizeBytes % 1024 > 0 ? 1 : 0);
                        diskHash.Add("ActualSize", sizeKB);
                        sizeKB = sizeKB + sizeKB / 50;
                        diskHash.Add("Size", sizeKB);
                        //Disks will be named as disk1, disk2 etc
                        // diskHash.Add("Name", "Disk"+diskNum.ToString());
                        diskHash.Add("Name", name);
                        diskHash.Add("Selected", "Yes");
                        diskHash.Add("disk_type", "persistent");
                        diskHash.Add("Protected", "no");
                        diskHash.Add("SelectedDataStore", "null");
                        diskHash.Add("IsThere", "no");
                        diskHash.Add("IsUsedAsSlot", "no");
                        diskHash.Add("port_number", scsiPort.ToString());
                        if (IsBootable == true)
                        {
                            diskHash.Add("IsBootable", "True");
                            
                        }
                        else
                        {
                            diskHash.Add("IsBootable", "False");
                        }
                        diskNum++;
                        list.Add(diskHash);

                    }
                    else
                    {
                        Trace.WriteLine(DateTime.Now + " \t Know that USB disk is there and we skipped that from the protection " + queryObj["Index"] + " Name of the disk is" + queryObj["Name"].ToString().Remove(0, 12));
                    }
                    
                }        
           
            }
            catch (ManagementException err)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(err, true);
                System.FormatException formatException = new FormatException("Inner exception", err);
                Trace.WriteLine(formatException.GetBaseException());
                Trace.WriteLine(formatException.Data);
                Trace.WriteLine(formatException.InnerException);
                Trace.WriteLine(formatException.Message);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + err.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            catch (System.UnauthorizedAccessException unauthorizedException)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(unauthorizedException, true);
                System.FormatException formatException = new FormatException("Inner exception", unauthorizedException);
                Trace.WriteLine(formatException.GetBaseException());
                Trace.WriteLine(formatException.Data);
                Trace.WriteLine(formatException.InnerException);
                Trace.WriteLine(formatException.Message);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + unauthorizedException.Message);
                Trace.WriteLine("ERROR___________________________________________");
                Trace.WriteLine("01_001_007: " + "An error occurred while querying for WMI data: " + unauthorizedException.Message);
                MessageBox.Show("Connection error (user name or password might be incorrect): " + unauthorizedException.Message);
                return false;
            }
            return true;
        }

        internal void Print()
        {
            foreach (Hashtable valHash in list)
            {
                Console.WriteLine("_____________________________________________________");
                foreach (string key in valHash.Keys)
                {
                    Console.WriteLine(key + "\t=\t" + valHash[key]);
                }


            }

            Console.WriteLine("Total Entries = " + list.Count.ToString());
        }

    }
    
}
