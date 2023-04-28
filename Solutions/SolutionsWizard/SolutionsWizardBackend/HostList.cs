using System;
using System.Collections.Generic;
using System.Text;
using System.Collections;
using System.Diagnostics;
using System.Xml;
using System.IO;
using System.Text.RegularExpressions;
using System.Windows.Forms;
using System.Runtime.Serialization.Formatters.Binary;
using Com.Inmage.Tools;




namespace Com.Inmage
{

    [Serializable]
    public class HostList : ICloneable
    {// 01_008_XXX


        public ArrayList _hostList;
        internal Credentials _domainAdminCredentials;
        internal Hashtable _configInfo;
        internal string SUBDIR_USED = "Windows";
        //internal string VMX_TEMPLATE_FILE = "vmx_template.vmx";
        internal int DISKS_PER_ADAPTER = 16;
        internal int SCSI_SLOT_SKIP_NUMBER = 7;
        internal bool domainCredentialsSet = false;
        internal string _installPath;

        internal static Host refHost =  new Host();
        

        public HostList()
        {
            _hostList           = new ArrayList();
            _domainAdminCredentials = new Credentials();

            AppConfig app = new AppConfig();
           // Debug.WriteLine("Calling GetConfigInfo in HostList");
           // _configInfo = app.GetConfigInfo("P2V");
            //app.PrintConfigValues(_configInfo);
            _installPath = WinTools.FxAgentPath() + "\\vContinuum";
        }



        public bool SetDomainCredentials(string inCredentialsName, string inDomain, string inUserName, string inPassWord)
        {

            _domainAdminCredentials.name        = inCredentialsName;
            _domainAdminCredentials.domain      = inDomain;
            _domainAdminCredentials.userName    = inUserName;
            _domainAdminCredentials.password    = inPassWord;
            domainCredentialsSet = true;
            return true;
        }

        public bool IsdomainCredentialsSet()
        {
            return domainCredentialsSet;
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


        public bool AddOrReplaceHost(Host inHost)
        {
            try
            {

                // Our hostlist contains hosts. 
                // Hosts and hostlist are nothing but arraylist....
                // To add new host or replace host we will call this method for the duplicate entries.....
                // we will check with displayname and targetesxips to add or replace...
                int index = -1;
                Host h = new Host();
                h = (Host)inHost.Clone();
                Host existHost = new Host();
                if (DoesHostExist(inHost, ref index) == true)
                {
                   // Trace.WriteLine(DateTime.Now + "\t Replacing the host " );
                   // inHost.Print();
                    //Trace.WriteLine(DateTime.Now + "\t printing the displayname and hostname " + inHost.displayName + " host " + inHost.hostName);
                  
                        existHost = (Host)_hostList[index];
                        if (existHost.targetvSphereHostName != null)
                        {
                            if (existHost.targetvSphereHostName == inHost.targetvSphereHostName)
                            {
                                _hostList.RemoveAt(index);
                            }
                        }
                        else if (existHost.targetESXIp != null)
                        {
                            if (existHost.targetESXIp == inHost.targetESXIp)
                            {

                                _hostList.RemoveAt(index);
                            }
                        }
                        else
                        {
                            _hostList.RemoveAt(index);
                        }

                        Debug.WriteLine(DateTime.Now + "\t Replacing the existing host " + h.displayname + " \t with displaynmae " + inHost.displayname);
                        _hostList.Add(h);
                    

                }
                else
                {
                    _hostList.Add(h);
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
            return true;
        }





        public bool DoesHostExist(Host inHost, ref int inIndex)
        {
            try
            {
                // Before adding to the hostlist we are checking whether it is already in list or not 
                // using displayname, hostname, ip, target esx ip......
                // Debug.WriteLine("Entered: DoesHostExist");
                int index = 0;
              //  Trace.WriteLine(DateTime.Now + " \t printing hostlist count " + _hostList.Count.ToString());
                // In this case First we are checking whether source_uuid is null or not.
                // If uuids are matched we are going to continue to next step.
                // If not we will go by ips. If ips are null then we will go for hostname and then by displaynames.
                index = 0;
                if (inHost.source_uuid != null)
                {
                    foreach (Host h in _hostList)
                    {
                        if (h.source_uuid != null && h.source_uuid.Length > 0)
                        {
                            if (h.source_uuid == inHost.source_uuid)
                            {
                                inIndex = index;
                                return true;
                            }
                        }
                        index++;
                    }

                }                
                else if (inHost.hostname != null)
                {
                    foreach (Host h in _hostList)
                    {

                        if (inHost.hostname != null && inHost.hostname.Length > 0)
                        {
                            if (h.hostname != null && h.hostname.Length > 0)
                            {
                                if (inHost.hostname != "NOT SET" && h.hostname != "NOT SET")
                                {
                                    if (inHost.targetESXIp != null)
                                    {
                                        if (h.hostname.CompareTo(inHost.hostname) == 0 && h.targetESXIp == inHost.targetESXIp)
                                        {
                                            Debug.WriteLine(DateTime.Now + " \t Printing vm hostname " + inHost.hostname);
                                            inIndex = index;

                                            return true;
                                        }
                                    }
                                    else if (inHost.vSpherehost != null)
                                    {
                                        Debug.WriteLine(DateTime.Now + "\t Pritnting the diaplay name and vSphere names " + h.displayname + " " + inHost.displayname + " host " + h.vSpherehost + " " + inHost.vSpherehost);
                                        if (h.displayname != null)
                                        {
                                            if ((h.hostname == inHost.hostname && h.vSpherehost == inHost.vSpherehost) && (h.displayname != null && h.displayname == inHost.displayname))
                                            {
                                                Debug.WriteLine(DateTime.Now + " \t Matches the display names " + h.displayname + " " + inHost.displayname);
                                                inIndex = index;
                                                return true;
                                            }
                                        }
                                        else
                                        {
                                            if (h.hostname == inHost.hostname && h.vSpherehost == inHost.vSpherehost)
                                            {
                                                Debug.WriteLine(DateTime.Now + " \t Matches the display names " + h.displayname + " " + inHost.displayname);
                                                inIndex = index;
                                                return true;
                                            }

                                        }
                                    }
                                    else
                                    {
                                        Debug.WriteLine(DateTime.Now + "\t Printing the hostnames " + h.hostname + "\t selectedhost name " + inHost.hostname);
                                        if (h.hostname.CompareTo(inHost.hostname) == 0)
                                        {
                                            Debug.WriteLine(DateTime.Now + " \t Printing vm hostname " + inHost.hostname + " esx ip " + h.esxIp);
                                            inIndex = index;

                                            return true;
                                        }
                                    }
                                }
                            }
                        }

                        Debug.WriteLine(DateTime.Now + " \t printing index values " + index.ToString());
                        index++;
                    }
                }
                else if (inHost.displayname != null)
                {
                    foreach (Host h in _hostList)
                    {
                        Debug.WriteLine("\tComparing " + h.displayname + " With " + inHost.displayname + " host name " + h.hostname + " " + inHost.hostname);
                        if (h.displayname != null && h.displayname.Length > 0)
                        {
                            if (inHost.displayname != null && inHost.displayname.Length > 0)
                            {
                                if (inHost.targetESXIp != null)
                                {
                                    if (h.displayname == inHost.displayname && h.targetESXIp == inHost.targetESXIp)
                                    {
                                        Debug.WriteLine(DateTime.Now + " \t Matches the display names ");
                                        inIndex = index;
                                        return true;
                                    }
                                    else
                                    {
                                        Debug.WriteLine(DateTime.Now + "\t display name does not match " + h.displayname + " " + inHost.displayname);
                                    }
                                }
                                else if (inHost.vSpherehost != null)
                                {
                                    Debug.WriteLine(DateTime.Now + "\t Pritnting the diaplay name and vSphere names " + h.displayname + " " + inHost.displayname + " host " + h.vSpherehost + " " + inHost.vSpherehost);
                                    if (h.displayname == inHost.displayname && h.vSpherehost == inHost.vSpherehost)
                                    {
                                        Debug.WriteLine(DateTime.Now + " \t Matches the display names " + h.displayname + " " + inHost.displayname);
                                        inIndex = index;
                                        return true;
                                    }
                                }
                                else
                                {
                                    Debug.WriteLine(DateTime.Now + "\t Printing the hostnames " + h.displayname + "\t selectedhost name " + inHost.displayname);
                                    if (h.displayname == inHost.displayname)
                                    {
                                        Debug.WriteLine(DateTime.Now + " \t Matches the display names " + h.displayname + " " + inHost.displayname);
                                        inIndex = index;
                                        return true;
                                    }
                                }
                            }
                        }
                        index++;
                    }
                }
                else if (inHost.ip != null)
                {
                    foreach (Host h in _hostList)
                    {

                        if (inHost.ip != null && inHost.ip.Length > 0)
                        {
                            if (h.ip != null && h.ip.Length > 0)
                            {
                                if (inHost.ip != "NOT SET" && h.ip != "NOT SET")
                                {
                                    if (inHost.targetESXIp != null)
                                    {
                                        if (h.ip.CompareTo(inHost.ip) == 0 && h.targetESXIp == inHost.targetESXIp)
                                        {
                                            inIndex = index;
                                            Debug.WriteLine(DateTime.Now + " \t Matched IPs " + inHost.ip + "and " + h.ip + "Index = " + index);
                                            return true;
                                        }
                                    }
                                    else if (inHost.vSpherehost != null)
                                    {
                                        Debug.WriteLine(DateTime.Now + "\t Pritnting the diaplay name and vSphere names " + h.displayname + " " + inHost.displayname + " host " + h.vSpherehost + " " + inHost.vSpherehost);
                                        if (h.ip == inHost.ip && h.vSpherehost == inHost.vSpherehost)
                                        {
                                            Debug.WriteLine(DateTime.Now + " \t Matches the display names " + h.displayname + " " + inHost.displayname);
                                            inIndex = index;
                                            return true;
                                        }
                                    }
                                    else
                                    {
                                        if (h.ip.CompareTo(inHost.ip) == 0)
                                        {
                                            inIndex = index;
                                            Debug.WriteLine(DateTime.Now + " \t Matched IPs " + inHost.ip + "and " + h.ip + "Index = " + index);
                                            return true;
                                        }
                                    }
                                }
                            }
                        }
                        index++;
                    }
                }
                //  Debug.WriteLine("Exiting: DoesHostExist");
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
            return false ;
        }
        public bool DoesHostExistInPlanBased(Host inHost, ref int inIndex)
        {
            try
            {
                // For supporting the 1-n we have added this method based on plan name we will check the 
                // Hosts that are exists in the configuaration file.....
                // Debug.WriteLine("Entered: DoesHostExist");
                int index = 0;
                //Trace.WriteLine(DateTime.Now + " \t printing hostlist count " + _hostList.Count.ToString());
                foreach (Host h in _hostList)
                {
                    //   Debug.WriteLine( "\tComparing " +  h.displayName + " With " + inHost.displayName);
                    if (h.displayname != null && h.displayname.Length > 0)
                    {
                        if (inHost.displayname != null && inHost.displayname.Length > 0)
                        {
                               if (h.displayname.CompareTo(inHost.displayname) == 0 && h.plan == inHost.plan)
                                {
                                   // Trace.WriteLine(DateTime.Now + " \t Matches the display names " + h.esxIp + h.displayName + " " + inHost.displayName);
                                    inIndex = index;
                                    return true;
                                }
                                else
                                {
                                   // Trace.WriteLine(DateTime.Now + "\t display name does not match " + h.displayName +" " + inHost.displayName);
                                }                           
                        }
                    }

                    if (inHost.ip != null && inHost.ip.Length > 0)
                    {
                        if (h.ip != null && h.ip.Length > 0)
                        {
                            if (inHost.ip != "NOT SET" && h.ip != "NOT SET")
                            {
                                if (h.ip.CompareTo(inHost.ip) == 0 && h.plan == inHost.plan)
                                {
                                    inIndex = index;
                                    //Trace.WriteLine(DateTime.Now + " \t Matched IPs " + inHost.ip + "and " + h.ip + "Index = " + index);
                                    return true;
                                }
                            }
                        }
                    }

                    if (inHost.hostname != null && inHost.hostname.Length > 0)
                    {
                        if (h.hostname != null && h.hostname.Length > 0)
                        {
                            if (inHost.hostname != "NOT SET" && h.hostname != "NOT SET")
                            {
                                if (h.hostname.CompareTo(inHost.hostname) == 0 && h.plan == inHost.plan)
                                {
                                    Debug.WriteLine(DateTime.Now + " \t Printing vm hostname " + inHost.hostname);
                                    inIndex = index;

                                    return true;
                                }
                            }
                        }
                    }
                    //Trace.WriteLine(DateTime.Now + " \t printing index values " + index.ToString());
                    index++;

                }
                //  Debug.WriteLine("Exiting: DoesHostExist");
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
            return false;
        }


        //internal bool GenerateVmxFile()
        //{
        //    try
        //    {
        //        // In case of p2v ui is going to make the vmx file with info getting from the wmi call..
        //        string templateFile = null;
        //        string hostVmxFile = null;

        //        foreach (Host h in _hostList)
        //        {

        //            Host h1;
        //            h1 = h;
        //            if (h.displayname == null)
        //                h.displayname = h.hostname;

        //            if (_configInfo == null || _configInfo["Vmx_Template"] == null)
        //                templateFile = VMX_TEMPLATE_FILE;
        //            else
        //                templateFile = _configInfo["Vmx_Template"].ToString();

        //            if (h.displayname == null)
        //            {
        //                Trace.WriteLine(DateTime.Now + " \t 01_008_001: hostname is null \n");
        //                MessageBox.Show("01_008_001: hostname is null\n");

        //            }
        //            hostVmxFile = h.displayname + ".vmx";

        //            if (WriteVmxFile( h1, templateFile, hostVmxFile) == false)
        //            {

        //                return false;
        //            }
        //            else
        //            {
        //                h1 = GetHost;
        //                h.disks.list = h1.disks.list;

        //            }
        //            Trace.WriteLine(DateTime.Now + "\t\t For" + h.hostname);
        //            foreach (Hashtable diskhash in h.disks.list)
        //            {

        //                Trace.WriteLine(DateTime.Now + "\t\t" + diskhash["Name"].ToString() + "scsi_mapping_vmx  " + diskhash["scsi_mapping_vmx"]);

        //            }


        //        }
        //    }
        //    catch (Exception ex)
        //    {

        //        System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
        //        System.FormatException formatException = new FormatException("Inner exception", ex);
        //        Trace.WriteLine(formatException.GetBaseException());
        //        Trace.WriteLine(formatException.Data);
        //        Trace.WriteLine(formatException.InnerException);
        //        Trace.WriteLine(formatException.Message);
        //        Trace.WriteLine("ERROR*******************************************");
        //        Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
        //        Trace.WriteLine("Exception  =  " + ex.Message);
        //        Trace.WriteLine("ERROR___________________________________________");
        //        return false;

        //    }

        //    return true;

        //}


        public bool WriteVmxFile(Host host, string templateFile, string hostVmxFile)
        {
            try
            {
                // For each host we need to create directory with with the displayname an dmake vmx in that directoryu
                // That will be done in the vCOn install path....
                Trace.WriteLine(DateTime.Now + " \t Entered: WriteVmxFile of HostList.cs ");
                string currentDir = _installPath;




                if (!Directory.Exists(SUBDIR_USED))
                {
                    Debug.WriteLine(DateTime.Now + " \t Creating Dir " + SUBDIR_USED);
                    Directory.CreateDirectory(SUBDIR_USED);
                }



                string hostDir = host.displayname;
                Trace.WriteLine(DateTime.Now + " \t Prinitng the sub dir name " + hostDir);
                if (Directory.Exists(hostDir))
                {
                    try
                    {
                        string[] files = System.IO.Directory.GetFiles(hostDir);
                        foreach (string s in files)
                        {
                            File.Delete(s);
                        }
                        Directory.Delete(hostDir);
                    }
                    catch (Exception ex)
                    {
                        Trace.WriteLine(DateTime.Now + " \t Failed to delete the directory " + hostDir + ex.Message);
                    }
                }
                if (!Directory.Exists(hostDir))
                {
                    Debug.WriteLine(DateTime.Now + " \t Creating Dir " + hostDir);
                    Directory.CreateDirectory(hostDir);
                }




                templateFile = currentDir + "\\" + templateFile;

                hostVmxFile = hostDir + "\\" + hostVmxFile;

                if (!File.Exists(templateFile))
                {
                    Trace.WriteLine(DateTime.Now + " \t 01_008_002: vmx template file " + templateFile + " doesn't exist ");
                    MessageBox.Show("01_008_002: vmx template file " + templateFile + " doesn't exist ", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return false;

                }
                
                if (File.Exists(hostVmxFile))
                {
                    Trace.WriteLine(DateTime.Now + " \t  deleteing the old exisintg file");
                    File.Delete(hostVmxFile);
                }
                File.Copy(templateFile, hostVmxFile, true);
                // we need to add disk scsi entries and the nic info also.....
                AppendScsiEntries(hostVmxFile,  host);
                host = GetHost;
                AppendNicEntries(hostVmxFile,  host);
                host = GetHost;
                StreamWriter sW = File.AppendText(hostVmxFile);
                sW.WriteLine("guestOS = " + "\"" + host.osEdition + "\"");
                sW.Close();
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

            Trace.WriteLine(DateTime.Now +" \t Exiting: WriteVmxFile of HostLists.cs");
            refHost = host;
            return true;

        }


        public static bool AppendNicEntries(string hostVmxFile, Host host)
        {
            try
            {

             
                // In case of p2v we are getting the nic info using wmi call..
                // we need to add these entries in the vmx file...
                if (host.nicList.list == null)
                {
                    Trace.WriteLine("01_008_003: Host contains an empty NIC list");
                    return false;
                }

                StreamWriter sw = File.AppendText(hostVmxFile);

                int i=0;
                if (host.nicList.list != null)
                {
                    Trace.WriteLine(DateTime.Now + " \t Printing count of nics " + host.nicList.list.Count.ToString());
                    foreach (Hashtable nic in host.nicList.list)
                    {
                        Trace.WriteLine("Added nic" + i.ToString() + " to vmx file");
                        
                        string etherNetnumber="ethernet"+i.ToString();
                      
                            sw.WriteLine(etherNetnumber +".present = \"TRUE\"");
                            sw.WriteLine(etherNetnumber + ".virtualDev = \"e1000\"");
                            sw.WriteLine(etherNetnumber +".networkName = \"VM Network\"");
                            sw.WriteLine(etherNetnumber +".addressType = \"generated\"");
                       
                            // If we reach 16 disks ( 0-15) then we increment adapter counter
                        i++;


                            //Debug.WriteLine("Disk Name after split = " + diskName);
                        }
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

            refHost = host;
            return true;

        }

        public Host GetHost
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

        public bool AppendScsiEntries(string hostVmxFile, Host host)
        {
            try
            {
                // We need to append scsi entries with the bootable disk info first..
                // To boot the machine we need write first bootable disk info first..
                int diskCount, adapterCount, disksNeeded, adaptersNeeded;
                string scsiString, scsiAdapterString, diskName;

                string lastScsiAdapter = "";

               
                diskCount = 0;
                adapterCount = 0;


                if (host.disks.list == null)
                {
                    Trace.WriteLine("01_008_003: Host contains an empty drive list");
                    return false;
                }

                StreamWriter sw = File.AppendText(hostVmxFile);

                disksNeeded = host.disks.list.Count;
                adaptersNeeded = diskCount % DISKS_PER_ADAPTER;


                //Write first one for scsi0

                /* scsiAdapterString = "scsi" + adapterCount;
                 sw.WriteLine(scsiAdapterString + ".present = \"TRUE\"");
                 sw.WriteLine(scsiAdapterString + ".sharedBus = \"none\"");
                 sw.WriteLine(scsiAdapterString + ".virtualDev = \"lsilogic\"");
                 */
                bool isFirstTimeIntoLoop = true;
                bool additionOfDisk = false;
                string firstDiskName = "", bootableDiskName = "", bootableDiskVmxEntries = "", firstDiskVmxEntries = "";
                ArrayList slotList = new ArrayList();
                Trace.WriteLine(DateTime.Now + " \t Printing the count of disks " + host.disks.list.Count.ToString());
                if (host.disks.list != null)
                {
                    foreach (Hashtable drive in host.disks.list)
                    {
                        if (drive["Protected"] != null)
                        {
                            if (drive["Protected"].ToString() == "yes")
                            {
                                additionOfDisk = true;
                            }
                        }
                        
                        Trace.WriteLine(DateTime.Now + " \t name of disk " + drive["Name"].ToString() + "Selected " + drive["Selected"].ToString() + " protected " + drive["Protected"].ToString());
                        if (drive["Selected"].ToString() == "Yes" || drive["Protected"].ToString() == "yes")
                        {

                            //For each SCSI adapter, write following values once.
                            if (!drive["scsi_adapter_number"].ToString().Equals(lastScsiAdapter))
                            {
                                bool slotAlreadyUsed = false;
                              //Checking that slots are not equal to zero
                                if (slotList.Count > 0)
                                {
                                    foreach (string slotNumber in slotList)
                                    {
                                        //comparing the slots numbers in arraylist with present slot.
                                        if (slotNumber == drive["scsi_adapter_number"].ToString())
                                        {                                           
                                            slotAlreadyUsed = true;
                                        }
                                    }
                                }
                                //We will write new slot only if slotAlreadyUsed is false only if not we are getting smae slot two times in vmx file
                                if (slotAlreadyUsed == false)
                                {
                                    slotList.Add(drive["scsi_adapter_number"].ToString());

                                    scsiAdapterString = "scsi" + drive["scsi_adapter_number"];

                                    sw.WriteLine(scsiAdapterString + ".present = \"TRUE\"");
                                    sw.WriteLine(scsiAdapterString + ".sharedBus = \"none\"");
                                    sw.WriteLine(scsiAdapterString + ".virtualDev = \"lsilogic\"");

                                    lastScsiAdapter = drive["scsi_adapter_number"].ToString();
                                }
                               

                            }

                            Debug.WriteLine(DateTime.Now + " \t Disk Name = " + drive["Name"].ToString() + "Size =  " + drive["Size"].ToString());

                            //Remove the datastore name added for Physical machines.
                            diskName = drive["Name"].ToString();
                            diskName = diskName.Split('/')[1];
                           
                            if (isFirstTimeIntoLoop == true)
                            {
                                
                                firstDiskName = diskName;
                                isFirstTimeIntoLoop = false;
                                firstDiskVmxEntries = drive["scsi_mapping_vmx"].ToString();

                            }
                            if (drive["IsBootable"].ToString().Equals("True"))
                            {

                                
                                bootableDiskName = diskName;
                                bootableDiskVmxEntries = drive["scsi_mapping_vmx"].ToString();

                            }


                         

                            scsiString = "scsi" + drive["scsi_adapter_number"] + ":" + drive["scsi_slot_number"];
                            sw.WriteLine(scsiString + ".present  = \"TRUE\" ");
                            sw.WriteLine(scsiString + ".diskType  = \"disk\" ");
                            sw.WriteLine(scsiString + ".fileName  = \"" + diskName + "\"");
                            Trace.WriteLine(DateTime.Now + " \t Printing SCSI numbers " + scsiString);
                            Trace.WriteLine(DateTime.Now + " \t Name=" + diskName);
                            Trace.WriteLine(DateTime.Now + " \t boot=" + drive["IsBootable"]);


                            // If we reach 16 disks ( 0-15) then we increment adapter counter



                            //Debug.WriteLine("Disk Name after split = " + diskName);
                        }
                    }
                }
                sw.Close();
                if (additionOfDisk == false)
                {
                    if (firstDiskName == bootableDiskName)
                    {
                        Trace.WriteLine(DateTime.Now + "\t First disk and bootable disks are same.No modifications required in the vmx file..");
                    }
                    else
                    {
                        //swaping the disknames in vmx file.

                        Trace.WriteLine(DateTime.Now + "\t Modifying the .vmx file.Replacing the first disk name with bootable disk name..");
                        Trace.WriteLine(DateTime.Now + "\t first disk name is:" + firstDiskName + " And Bootable disk Name is:" + bootableDiskName);
                        StreamReader reader = new StreamReader(hostVmxFile);
                        string contentInVMXFile = reader.ReadToEnd();
                        reader.Close();

                        contentInVMXFile = Regex.Replace(contentInVMXFile, bootableDiskName, "bootabledisktemp");
                        contentInVMXFile = Regex.Replace(contentInVMXFile, firstDiskName, bootableDiskName);
                        contentInVMXFile = Regex.Replace(contentInVMXFile, "bootabledisktemp", firstDiskName);


                        StreamWriter writer = new StreamWriter(hostVmxFile);
                        writer.Write(contentInVMXFile);
                        writer.Close();
                        Trace.WriteLine(DateTime.Now + "\t\tUpdation of vmx file completed..");

                        //swaping the controller entries in case if first disk is not bootable disk.


                        foreach (Hashtable diskhash in host.disks.list)
                        {
                        
                            if (diskhash["Name"].ToString().Contains(firstDiskName))
                            {
                                //MessageBox.Show("came here 1"+ bootableDiskVmxEntries);
                                diskhash["scsi_mapping_vmx"] = bootableDiskVmxEntries;

                            }
                            if (diskhash["Name"].ToString().Contains(bootableDiskName))
                            {
                                //MessageBox.Show("came here 2"+ firstDiskVmxEntries);
                                diskhash["scsi_mapping_vmx"] = firstDiskVmxEntries;

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

            refHost = host;
            return true;

        }


        public bool RemoveHost(Host inHost)
        {
            // Using display name and hostname, ip we are able to remove host from the hostlist
            bool foundHost = false;
            int index=0;

            try
            {
                foreach (Host h in _hostList)
                {
                    if (inHost.plan != null)
                    {
                        if ((h.hostname != null && (h.hostname == inHost.hostname)) && h.plan == inHost.plan)
                        {
                            foundHost = true;
                            break;
                        }
                        if ((h.ip != null && (h.ip == inHost.ip)) && h.plan == inHost.plan)
                        {
                            foundHost = true;
                            break;
                        }
                        if ((h.displayname != null && (h.displayname == inHost.displayname)) && h.plan == inHost.plan)
                        {
                            foundHost = true;
                            break;
                        }
                    }
                    else if (inHost.vSpherehost != null)
                    {
                        if ((h.hostname != null && (h.hostname == inHost.hostname) && h.vSpherehost == inHost.vSpherehost))
                        {
                            foundHost = true;
                            break;
                        }
                        if ((h.ip != null && (h.ip == inHost.ip) && h.vSpherehost == inHost.vSpherehost))
                        {
                            foundHost = true;
                            break;
                        }
                        if ((h.displayname != null && (h.displayname == inHost.displayname) && h.vSpherehost == inHost.vSpherehost))
                        {
                            foundHost = true;
                            break;
                        }

                    }
                    else
                    {
                        if ((h.hostname != null && (h.hostname == inHost.hostname)))
                        {
                            foundHost = true;
                            break;
                        }
                        if ((h.ip != null && (h.ip == inHost.ip)))
                        {
                            foundHost = true;
                            break;
                        }
                        if ((h.displayname != null && (h.displayname == inHost.displayname)))
                        {
                            foundHost = true;
                            break;
                        }
                    }

                    index++;
                }

                if (foundHost)
                {
                    _hostList.RemoveAt(index);

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




        public Host GetHostByDisplayName(string inDisplayName)
        {
            try
            {
                foreach (Host h in _hostList)
                {
                    Debug.WriteLine(DateTime.Now + " \t Comparing " + h.displayname + " With " + inDisplayName);


                    if (h.displayname != null)
                    {
                        if (h.displayname.CompareTo(inDisplayName) == 0)
                        {
                            return h;
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
             

            }
            return null;

        }



        public ArrayList GetHostList
        {
            get
            {
                return _hostList;
            }
            set
            {
                value = _hostList;
            }

        }



        internal bool WriteHostsToXml(XmlWriter writer)
        {
            try
            {
                // We will prepare esx.xml and recovery.xml using the hostlist and hosts.
                if (writer == null)
                {
                    Trace.WriteLine("HostList:WriteHostsToXml: Got a null writer  value ");
                    MessageBox.Show("HostList:WriteHostsToXml: Got a null writer  value ");
                    return false;

                }

                foreach (Host h in _hostList)
                {
                    h.WriteXmlNode(writer);

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



        public bool WriteToXmlFile(string inFileName, string parentNode)
        {
            try
            {


                int index = 0;
                XmlWriterSettings settings = new XmlWriterSettings();
                settings.Indent = true;
                settings.ConformanceLevel = ConformanceLevel.Fragment;



                using (XmlWriter writer = XmlWriter.Create(inFileName, settings))
                {
                    writer.WriteStartElement("root");
                    if (parentNode.Length > 0)
                    {
                        writer.WriteStartElement(parentNode);
                    }


                    foreach (Host h in _hostList)
                    {
                        h.WriteXmlNode(writer);
                        index++;
                    }

                    if (parentNode.Length > 0)
                    {
                        writer.WriteEndElement();
                    }

                    writer.WriteEndElement();

                }




                /*


                    XmlDocument xmldoc = new XmlDocument();

                    XmlWriter myXmlWriter = new XmlWriter
                    xmldoc.cre
                    xmldoc.Load( inFileName  );

                    //Create a new node
                    XmlElement newelement = xmldoc.CreateElement("host");
           

                    XmlElement host = xmldoc.CreateElement("host");
            


                    XmlElement xmlTitle = xmldoc.CreateElement("title");
                    XmlElement xmlContent = xmldoc.CreateElement("content");

                    foreach (Host h in _hostList)
                    {


                        index++;
                    }
                */

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

        public bool Print()
        {
            
            foreach (Host h in _hostList)
            {
                h.Print();
            }
      
            return true;
        }
    }
}
