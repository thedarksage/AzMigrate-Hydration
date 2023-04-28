using System;
using System.Collections.Generic;
using System.Collections;
using Com.Inmage.Esxcalls;
using System.Windows.Forms;
using System.Diagnostics;
using System.IO;
using System.Text;
using Com.Inmage.Tools;


namespace Com.Inmage.Wizard
{
    class AddDiskPanelHandler : PanelHandler
    {

        string ReadXmlFile = "ESX.xml";
        internal string esxIP = "";
        internal string cxIp;
        internal static int SourceDisplaynameColumn = 0;
        internal static int MasterTargetDisplaynameColumn = 1;
        internal static int TargetEsxIPcolumn = 2;
       // public static int REMOVE_COLUMN = 3;
        internal static int AdditionDiskCheckBoxColumn = 3;
        internal static int ProtectedDiskColumn = 2;
        internal bool credentialPopup = true;
        ArrayList dataStoreListToCompare = new ArrayList();
        
        public AddDiskPanelHandler()
        {
        }
        internal override bool Initialize(AllServersForm allServersForm)
        {
            try
            {
                // Here we had changed the flow from 5.5 to 6.0 so that we are checking th elist prepared form the main screen.
                // And we will check whether user has selected p2v or esx.
                allServersForm.mandatoryLabel.Visible = false;
                if (ResumeForm.selectedVMListForPlan._hostList.Count != 0)
                {
                    if (ResumeForm.selectedActionForPlan == "ESX")
                    {
                        allServersForm.p2vHeadingLabel.Text = "Protect";
                    }
                    else
                    {
                        allServersForm.p2vHeadingLabel.Text = "P2V Protection";
                    }
                    AdditionOfDiskSelectMachinePanelHandler.temperorySelectedSourceList = new HostList();
                    AdditionOfDiskSelectMachinePanelHandler.temperorySelectedSourceList = ResumeForm.selectedVMListForPlan;
                    AdditionOfDiskSelectMachinePanelHandler.masterHostPrepared = (Host)ResumeForm.masterTargetList._hostList[0];
                    AdditionOfDiskSelectMachinePanelHandler.sourceListPrepared = ResumeForm.sourceHostsList;
                    allServersForm.previousButton.Visible = false;                    
                }
                else
                {
                    allServersForm.previousButton.Visible = true;
                }



                allServersForm.helpContentLabel.Text = HelpForcx.AdditionofDisk2;
                credentialPopup = true;
                //allServersForm.nextButton.Enabled = false;
                //allServersForm.previousButton.Visible = false;
               
                SolutionConfig config = new SolutionConfig();
                allServersForm.selectedSourceListByUser = new HostList();

                //Trying to read the esx.xml which is made by scrips. and we will show in the datagridview.
                config.ReadXmlConfigFile(ReadXmlFile,  allServersForm.selectedSourceListByUser,  allServersForm.masterHostAdded,  esxIP,  cxIp);
                allServersForm.selectedSourceListByUser = SolutionConfig.list;
                allServersForm.masterHostAdded = SolutionConfig.masterHosts;
                esxIP = SolutionConfig.EsxIP;
                cxIp = SolutionConfig.csIP;

                if (ResumeForm.selectedActionForPlan == "ESX")
                {
                    foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                    {

                        Host tempHost = new Host();
                        tempHost = h;

                        CheckinVMinCX(allServersForm, ref tempHost);

                        h.disks.list = tempHost.disks.list;
                    }
                }




                int hostCount = allServersForm.selectedSourceListByUser._hostList.Count;
                if (allServersForm.masterHostAdded.vmwareTools == "NotInstalled" || allServersForm.masterHostAdded.vmwareTools == "NotRunning" )
                {
                    MessageBox.Show("Please make sure VMware tools are installed and running on master target VM: " + allServersForm.masterHostAdded.displayname, "vContinuum error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return false;
                }
                dataStoreListToCompare = allServersForm.masterHostAdded.dataStoreList;
                Debug.WriteLine("Printing the master host");
                // allServersForm._masterHost.Print();
                int rowIndex = 0;
                if (hostCount > 0)
                {
                    allServersForm.addDiskDetailsDataGridView.Rows.Clear();
                    foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                    {
                        allServersForm.addDiskDetailsDataGridView.Rows.Add(1);
                        allServersForm.addDiskDetailsDataGridView.Rows[rowIndex].Cells[SourceDisplaynameColumn].Value = h.displayname;
                        allServersForm.osTypeSelected = h.os;
                        // allServersForm._selectedSourceList.AddOrReplaceHost(h);
                        rowIndex++;
                        // AddingDiskDetails(allServersForm, h);ss
                    }
                    for (int i = 0; i < allServersForm.addDiskDetailsDataGridView.RowCount; i++)
                    {
                        Debug.WriteLine("Printing esx ip " + esxIP + " Cx ip " + cxIp);
                        allServersForm.addDiskDetailsDataGridView.Rows[i].Cells[MasterTargetDisplaynameColumn].Value = allServersForm.masterHostAdded.displayname;

                        allServersForm.addDiskDetailsDataGridView.Rows[i].Cells[TargetEsxIPcolumn].Value = allServersForm.masterHostAdded.esxIp;
                    }
                    if (ResumeForm.selectedVMListForPlan._hostList.Count != 0)
                    {
                        Host h = (Host)ResumeForm.selectedVMListForPlan._hostList[0];
                        allServersForm.masterHostAdded.esxIp = h.targetESXIp;
                        allServersForm.masterHostAdded.esxUserName = h.targetESXUserName;
                        allServersForm.masterHostAdded.esxPassword = h.targetESXPassword;
                        foreach (Host h1 in allServersForm.selectedSourceListByUser._hostList)
                        {
                            h1.esxIp = h.esxIp;
                            h1.esxUserName = h.esxUserName;
                            h1.esxPassword = h.esxPassword;
                            h1.targetESXIp = h.targetESXIp;
                            h1.targetESXUserName = h.targetESXUserName;
                            h1.targetESXPassword = h.targetESXPassword;
                            h1.plan = h.plan;
                            AdditionOfDiskSelectMachinePanelHandler.machineTypexml = h.machineType;
                            
                            AdditionOfDiskSelectMachinePanelHandler.sourceEsxIPxml = h.esxIp;
                            AdditionOfDiskSelectMachinePanelHandler.sourceUsernamexml = h.esxUserName;
                            AdditionOfDiskSelectMachinePanelHandler.sourcePasswordxml = h.esxPassword;
                            AdditionOfDiskSelectMachinePanelHandler.targetEsxIPxml = h.targetESXIp;
                            AdditionOfDiskSelectMachinePanelHandler.targetUsernameXml = h.targetESXUserName;
                            AdditionOfDiskSelectMachinePanelHandler.targetPasswordXml = h.targetESXPassword;
                            allServersForm.planInput = h.plan;
                            allServersForm.masterHostAdded.esxIp = h.targetESXIp;
                            allServersForm.masterHostAdded.esxUserName = h.targetESXUserName;
                            allServersForm.masterHostAdded.esxPassword = h.targetESXPassword;
                            //MessageBox.Show(allServersForm._masterHost.esxUserName);
                        }
                    }
                    else
                    {
                        allServersForm.masterHostAdded.esxIp = AdditionOfDiskSelectMachinePanelHandler.targetEsxIPxml;
                        allServersForm.masterHostAdded.esxUserName = AdditionOfDiskSelectMachinePanelHandler.targetUsernameXml;
                        //MessageBox.Show("   " + AdditionOfDiskSelectMachinePanelHandler._targetPassword);
                        allServersForm.masterHostAdded.esxPassword = AdditionOfDiskSelectMachinePanelHandler.targetPasswordXml;
                    }
                    allServersForm.finalMasterListPrepared = new HostList();
                    allServersForm.selectedMasterListbyUser.AddOrReplaceHost(allServersForm.masterHostAdded);
                    Debug.WriteLine("Printing source list afetr displaying disks in the datagrid view  +++++++++++++++++++++++++++");
                    // allServersForm._selectedSourceList.Print();
                    Trace.WriteLine(DateTime.Now + " \t Printing the count of two lists " + allServersForm.selectedSourceListByUser._hostList.Count.ToString() + AdditionOfDiskSelectMachinePanelHandler.sourceListPrepared._hostList.Count.ToString());
                    foreach (Host h1 in allServersForm.selectedSourceListByUser._hostList)
                    {
                        foreach (Host previousProtectionHost in AdditionOfDiskSelectMachinePanelHandler.sourceListPrepared._hostList)
                        {
                            if (h1.displayname == previousProtectionHost.displayname)
                            {
                                if (h1.rdmpDisk == "TRUE" && previousProtectionHost.rdmpDisk == "TRUE")
                                {
                                    h1.convertRdmpDiskToVmdk = previousProtectionHost.convertRdmpDiskToVmdk;
                                }
                                else if (h1.rdmpDisk == "TRUE" && previousProtectionHost.rdmpDisk == "FALSE")
                                {
                                    switch (MessageBox.Show("Do you want to convert RDM disks to vmdk files for machine " + h1.displayname + " ?", "RDM", MessageBoxButtons.YesNo, MessageBoxIcon.Question))
                                    {
                                        case DialogResult.No:
                                            // allServersForm._convertRdmDiskToVmdk = "no";
                                            h1.convertRdmpDiskToVmdk = false;
                                            break;
                                        case DialogResult.Yes:
                                            h1.convertRdmpDiskToVmdk = true;
                                            break;
                                    }
                                }
                            }
                        }
                    }
                }
                Trace.WriteLine(allServersForm.selectedSourceListByUser._hostList.Count.ToString());
                if (allServersForm.selectedSourceListByUser._hostList.Count > 0)
                {                    
                    foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                    {
                        ArrayList physicalDisks;
                        physicalDisks = h.disks.GetDiskList;
                        foreach (Hashtable disk in physicalDisks)
                        {
                            if (disk["Protected"].ToString() == "no")
                            {
                                disk["Selected"] = "Yes";
                            }
                            else
                            {
                                disk["Selected"] = "No";
                            }
                        }
                    }
                    Host tempHost = (Host)allServersForm.selectedSourceListByUser._hostList[0];
					 foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                    {
                        if (ResumeForm.selectedActionForPlan == "BMR Protection" && h.os  == OStype.Windows)
                        {
                            foreach (Hashtable hash in h.disks.list)
                            {
                                if (hash["volumelist"] == null )
                                {
                                    hash["Selected"] = "No";
                                }
                            }
                        }
                    }
					AddingDiskDetails(allServersForm, tempHost);
                    AddingDiskDetails(allServersForm, tempHost);
                }
                if (ResumeForm.selectedVMListForPlan._hostList.Count != 0)
                {
                    if (ResumeForm.selectedActionForPlan == "BMR Protection")
                    {
                        foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                        {
                            foreach (Host h1 in ResumeForm.selectedVMListForPlan._hostList)
                            {
                                if (h1.hostname == h.hostname)
                                {                                  
                                   h.cpuCount = h1.cpuCount;                              
                                   h.memSize = h1.memSize;
                                   h.batchResync = h1.batchResync;
                                   
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

                return false;
            }
            return true;
        }



        internal bool CheckinVMinCX(AllServersForm allServersForm, ref  Host h)
        {
            try
            {
                WinPreReqs win = new WinPreReqs(h.ip, h.domain, h.userName, h.passWord);
                int returnCode = 0;
                string cxip = WinTools.GetcxIPWithPortNumber();
                string hostname, displayname, ip = null;
                hostname = h.hostname;
                displayname = h.displayname;
                ip = h.ip;
                if (h.hostname != null)
                {

                    returnCode = win.SetHostIPinputhostname(h.hostname,  h.ip, WinTools.GetcxIPWithPortNumber());
                    h.ip = WinPreReqs.GetIPaddress;
                    if (win.GetDetailsFromcxcli( h, cxip) == true)
                    {
                        h = WinPreReqs.GetHost;
                        Trace.WriteLine(DateTime.Now + "\t Successfully got the uuid for the machine " + h.displayname);
                    }

                    Host tempHost1 = new Host();
                    tempHost1.inmage_hostid = h.inmage_hostid;
                    Cxapicalls cxApi = new Cxapicalls();
                    if (cxApi.Post( tempHost1, "GetHostInfo") == true)
                    {
                        tempHost1 = Cxapicalls.GetHost;

                        foreach (Hashtable selectedHostHash in tempHost1.disks.list)
                        {
                            if (selectedHostHash["DiskScsiId"] != null)
                            {
                                string sourceSCSIId = null;
                                sourceSCSIId = selectedHostHash["DiskScsiId"].ToString();
                                if (selectedHostHash["DiskScsiId"].ToString().Length > 0)
                                {
                                    Trace.WriteLine(DateTime.Now + "\t Printing scsi id before removing " + sourceSCSIId);
                                    sourceSCSIId = sourceSCSIId.Remove(0, 1);
                                    Trace.WriteLine(DateTime.Now + "\t Printing scsi id after removing first char - " + sourceSCSIId);
                                    selectedHostHash.Add("source_disk_uuid_to_use", sourceSCSIId);
                                }
                            }
                        }
                        foreach (Hashtable selectedHostHash in h.disks.list)
                        {
                            if (selectedHostHash["source_disk_uuid"] != null)
                            {
                                string sourceSCSIId = null;
                                sourceSCSIId = selectedHostHash["source_disk_uuid"].ToString();

                                if (sourceSCSIId != null)
                                {
                                    Trace.WriteLine(DateTime.Now + "\t Printing scsi id before replacing with - " + sourceSCSIId);
                                    sourceSCSIId = sourceSCSIId.Replace("-", "");
                                    Trace.WriteLine(DateTime.Now + "\t Printing scsi id after replacing  - " + sourceSCSIId);
                                    selectedHostHash.Add("disk_id_to_compare", sourceSCSIId);
                                }

                            }
                        }

                        foreach (Hashtable selectedHostHash in h.disks.list)
                        {
                            foreach (Hashtable hashofTempHost in tempHost1.disks.list)
                            {
                                if (selectedHostHash["disk_id_to_compare"] != null && hashofTempHost["source_disk_uuid_to_use"] != null)
                                {
                                    Trace.WriteLine(DateTime.Now + "\t comparing the disk uuids source from Info.xml  " + selectedHostHash["disk_id_to_compare"].ToString() + "\t From cx " + hashofTempHost["source_disk_uuid_to_use"].ToString());
                                    if (selectedHostHash["disk_id_to_compare"].ToString().ToUpper() == hashofTempHost["source_disk_uuid_to_use"].ToString().ToUpper())
                                    {
                                        Trace.WriteLine(DateTime.Now + "\t disk uuids matched " + selectedHostHash["disk_id_to_compare"].ToString());
                                        if (hashofTempHost["disk_signature"] != null)
                                        {
                                            if (selectedHostHash["disk_signature"] != null)
                                            {
                                                selectedHostHash["disk_signature"] = hashofTempHost["disk_signature"].ToString();
                                            }
                                            else
                                            {
                                                selectedHostHash.Add("disk_signature", hashofTempHost["disk_signature"].ToString());
                                            }
                                        }
                                        if (hashofTempHost["volumelist"] != null)
                                        {
                                            if (selectedHostHash["volumelist"] != null)
                                            {
                                                selectedHostHash["volumelist"] = hashofTempHost["volumelist"].ToString();
                                            }
                                            else
                                            {
                                                selectedHostHash.Add("volumelist", hashofTempHost["volumelist"].ToString());
                                            }
                                        }
                                        if (hashofTempHost["src_devicename"] != null)
                                        {
                                            if (selectedHostHash["src_devicename"] != null)
                                            {
                                                selectedHostHash["src_devicename"] = hashofTempHost["src_devicename"].ToString();
                                            }
                                            else
                                            {
                                                selectedHostHash.Add("src_devicename", hashofTempHost["src_devicename"].ToString());
                                            }
                                        }
                                        if (hashofTempHost["disk_type_os"] != null)
                                        {
                                            if (selectedHostHash["disk_type_os"] != null)
                                            {
                                                selectedHostHash["disk_type_os"] = hashofTempHost["disk_type_os"].ToString();
                                            }
                                            else
                                            {
                                                selectedHostHash.Add("disk_type_os", hashofTempHost["disk_type_os"].ToString());
                                            }
                                        }
                                        if (hashofTempHost["efi"] != null)
                                        {
                                            if (selectedHostHash["efi"] != null)
                                            {
                                                selectedHostHash["efi"] = hashofTempHost["efi"].ToString();
                                            }
                                            else
                                            {
                                                selectedHostHash.Add("efi", hashofTempHost["efi"].ToString());
                                            }
                                        }
                                        if (hashofTempHost["DiskLayout"] != null)
                                        {
                                            if (selectedHostHash["DiskLayout"] != null)
                                            {
                                                selectedHostHash["DiskLayout"] = hashofTempHost["DiskLayout"].ToString();
                                            }
                                            else
                                            {
                                                selectedHostHash.Add("DiskLayout", hashofTempHost["DiskLayout"].ToString());
                                            }
                                        }
                                        if (hashofTempHost["DiskScsiId"] != null)
                                        {
                                            if (selectedHostHash["DiskScsiId"] != null)
                                            {
                                                selectedHostHash["DiskScsiId"] = hashofTempHost["DiskScsiId"].ToString();
                                            }
                                            else
                                            {
                                                selectedHostHash.Add("DiskScsiId", hashofTempHost["DiskScsiId"].ToString());
                                            }
                                        }
                                        break;
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
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: CheckinVMinCX " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");

                return false;
            }
            return true;
        }




        internal override bool ValidatePanel(AllServersForm allServersForm)
        {
            

            return true;
           
        }
        internal override bool ProcessPanel(AllServersForm allServersForm)
        {
            try
            {
                // In this case we need to check what are the disk selected for additon of disk and their sizes 
                // at least one disk should be selected for the vm which is selected to addition of disk.
                //In this process panel only we are creating the vmx file in case of p2v. by comparing the 
                //previous values stroed in the MasterConfigFile.xml.

                allServersForm.cxIPretrived = WinTools.GetcxIP();
                ArrayList physicalDisks = new ArrayList();
                Host tempHostForMaster = new Host();
                string errorMessage = "";
                WmiCode errorCode = WmiCode.Unknown;
                if (credentialPopup == true)
                {
                    allServersForm.selectedMasterListbyUser.AddOrReplaceHost(allServersForm.masterHostAdded);
                }
                foreach (Host h1 in allServersForm.selectedMasterListbyUser._hostList)
                {
                    foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                    {
                        h.machineType = AdditionOfDiskSelectMachinePanelHandler.machineTypexml;
                        h.esxIp = AdditionOfDiskSelectMachinePanelHandler.sourceEsxIPxml;
                        h.esxUserName = AdditionOfDiskSelectMachinePanelHandler.sourceUsernamexml;
                        h.esxPassword = AdditionOfDiskSelectMachinePanelHandler.sourcePasswordxml;
                        h.targetESXIp = h1.esxIp;
                        h.masterTargetDisplayName = h1.displayname;
                        h.mt_uuid = h1.source_uuid;
                        h.masterTargetHostName = h1.hostname;
                        h.plan = allServersForm.planInput;
                        physicalDisks = h.disks.GetDiskList;
                        h.totalSizeOfDisks = 0;
                        h.incrementalDisk = false;
                        foreach (Hashtable disk in physicalDisks)
                        {
                            Trace.WriteLine(DateTime.Now + " \t Printing name and size ans selected values " + h.displayname + " " + disk["Selected"].ToString() + disk["Size"].ToString());
                            if (disk["Selected"].ToString() == "Yes")
                            {
                                h.incrementalDisk = true;
                                if (disk["Rdm"].ToString() == "no" || h.convertRdmpDiskToVmdk == true)
                                {
                                    float size = float.Parse(disk["Size"].ToString());
                                    size = size / (1024 * 1024);
                                    h.totalSizeOfDisks = h.totalSizeOfDisks + size;
                                }


                            }
                        }
                        if (h.totalSizeOfDisks.ToString().Contains("."))
                        {
                            h.totalSizeOfDisks = (float)Math.Round(h.totalSizeOfDisks, 0);
                        }
                    }
                }

                //In case of addition  of disk we will automatically select the datastore which is selected by the user
                // at the time of protection we will check the free space in datastroe and then we will allot 
                // Particular space to the selected disk in this addition of disk.
                foreach (Host h in AdditionOfDiskSelectMachinePanelHandler.temperorySelectedSourceList._hostList)
                {
                    foreach (Host h1 in allServersForm.selectedSourceListByUser._hostList)
                    {
                        foreach (Host h2 in allServersForm.selectedMasterListbyUser._hostList)
                        {
                            foreach (DataStore d in h2.dataStoreList)
                            {
                                if (h.hostname == h1.hostname)
                                {
                                    if (h.targetDataStore == d.name)
                                    {
                                        Trace.WriteLine(DateTime.Now + " \t got the data store " + d.name);
                                        d.freeSpace = d.freeSpace - h1.totalSizeOfDisks;
                                        if (d.freeSpace >= 0)
                                        {

                                            h1.targetDataStore = h.targetDataStore;
                                        }
                                        else
                                        {
                                            if (h.machineType.ToUpper() == "PHYSICALMACHINE")
                                            {
                                                Trace.WriteLine(DateTime.Now + "\t Avoiding size calculation of disk in case of p2v " + h.machineType);
                                                h1.targetDataStore = h.targetDataStore;
                                            }
                                            else
                                            {
                                                d.freeSpace = d.freeSpace + h1.totalSizeOfDisks;
                                                //MessageBox.Show("The datastore does't have enough space to protect new disk of the machine " + h1.displayname);
                                                // return false;
                                            }
                                        }
                                    }
                                }
                                else if (h.source_uuid != null)
                                {
                                    if (h.source_uuid == h1.source_uuid)
                                    {
                                        if (h.targetDataStore == d.name)
                                        {
                                            Trace.WriteLine(DateTime.Now + " \t got the data store " + d.name);
                                            d.freeSpace = d.freeSpace - h1.totalSizeOfDisks;
                                            if (d.freeSpace >= 0)
                                            {
                                                h1.targetDataStore = h.targetDataStore;
                                            }
                                            else
                                            {
                                                d.freeSpace = d.freeSpace + h1.totalSizeOfDisks;
                                                // MessageBox.Show("The datastore does't have enough space to protect new disk of the machine " + h1.displayname);
                                                //return false;
                                            }
                                        }
                                    }
                                }
                                else if (h.displayname == h1.displayname)
                                {
                                    if (h.targetDataStore == d.name)
                                    {
                                        Trace.WriteLine(DateTime.Now + " \t got the data store " + d.name);
                                        d.freeSpace = d.freeSpace - h1.totalSizeOfDisks;
                                        if (d.freeSpace >= 0)
                                        {

                                            h1.targetDataStore = h.targetDataStore;
                                        }
                                        else
                                        {
                                            if (h.machineType.ToUpper() == "PHYSICALMACHINE")
                                            {
                                                Trace.WriteLine(DateTime.Now + "\t Avoiding size calculation of disk in case of p2v " + h.machineType);
                                                h1.targetDataStore = h.targetDataStore;
                                            }
                                            else
                                            {
                                                d.freeSpace = d.freeSpace + h1.totalSizeOfDisks;
                                                //MessageBox.Show("The datastore does't have enough space to protect new disk of the machine " + h1.displayname);
                                                //return false;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                {
                    if (h.incrementalDisk == false && h.cluster == "no")
                    {
                        MessageBox.Show(" No disk is  selected for protection in " + h.displayname, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return false;
                    }


                    Host mtHost = (Host)allServersForm.selectedMasterListbyUser._hostList[0];
                    foreach (DataStore d in mtHost.dataStoreList)
                    {
                        if (d.name == h.targetDataStore && d.vSpherehostname == mtHost.vSpherehost)
                        {
                            if (d.type == "vsan")
                            {
                                h.vSan = true;
                            }
                            else
                            {
                                h.vSan = false;
                            }
                        }
                    }

                }
                // This is only in case of local back up.
                // We need to add the folderpath what ever user seelcted at the time of normal protection to the newly added disk.
                // That has been taking care in the method.
                foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                {
                    if (h.local_backup == true)
                    {
                        ArrayList newlyAddedDisks = new ArrayList();
                        newlyAddedDisks = h.disks.GetDiskList;
                        foreach (Hashtable disk in newlyAddedDisks)
                        {
                            //First spliting diskname inot two parts with / to diskname array
                            //Againg split first part with ] folderpath array
                            // make folderpath second part as new displayname and combine all the folderpath array
                            // Now combine parts of diskname array.
                            //After that we

                            string[] diskname = disk["Name"].ToString().Split('/');
                            int splitCount = diskname.Length;
                            string foldername = diskname[0];
                            //Trace.WriteLine(DateTime.Now + "\t Printing the foldername " + foldername);
                            string[] folderpath = foldername.Split(']');
                            if (folderpath.Length > 1)
                            {
                                folderpath[1] = h.new_displayname;
                            }
                            foldername = null;
                            for (int i = 0; i < folderpath.Length; i++)
                            {
                                if (foldername == null)
                                {
                                    foldername = folderpath[i] + "] ";
                                }
                                else
                                {
                                    foldername = foldername + folderpath[i];
                                }
                            }
                            diskname[0] = foldername;
                            disk["Name"] = null;
                            for (int i = 0; i < splitCount; i++)
                            {
                                if (disk["Name"] == null)
                                {
                                    disk["Name"] = diskname[i] + "/";
                                }
                                else
                                {
                                    disk["Name"] = disk["Name"] + diskname[i];
                                }
                            }
                        }
                    }
                }


                //this is for to generate the vmx file fort p2v .
                if (ResumeForm.selectedActionForPlan == "BMR Protection")
                {
                    foreach (Host tempHost in allServersForm.selectedSourceListByUser._hostList)
                    {
                        ArrayList diskListOfHost = new ArrayList();
                        diskListOfHost = tempHost.disks.GetDiskList;
                        int i = 0;
                        foreach (Hashtable disk in diskListOfHost)
                        {
                            if (disk["Protected"].ToString() == "yes")
                            {
                                i++;
                            }
                            tempHost.alreadyP2VDisksProtected = i;
                            if ((i >= 8 && i < 23))
                            {
                                tempHost.alreadyP2VDisksProtected = i + 1;
                            }
                            else if (i >= 23 && i < 38)
                            {
                                tempHost.alreadyP2VDisksProtected = i + 2;
                            }
                            else if (i >= 38 && i < 53)
                            {
                                tempHost.alreadyP2VDisksProtected = i + 3;
                            }
                            else if (i >= 53)
                            {
                                tempHost.alreadyP2VDisksProtected = i + 4;
                            }
                            else
                            {
                                tempHost.alreadyP2VDisksProtected = i;
                            }
                        }
                    }


                    HostList sourceList = new HostList();
                    HostList masterList = new HostList();
                    string cxip = null; string esxip = null;
                    foreach (Host h2 in allServersForm.selectedSourceListByUser._hostList)
                    {
                        if (h2.cluster == "yes" && h2.operatingSystem.Contains("2003"))
                        {
                            string masterConfigFile = "MasterConfigFile.xml";
                            string latestFilePath = WinTools.FxAgentPath() + "\\vContinuum\\Latest";
                            SolutionConfig sol = new SolutionConfig();
                            if (File.Exists(latestFilePath + "\\" + masterConfigFile))
                            {
                                sol.ReadXmlConfigFileWithMasterList(masterConfigFile,  sourceList,  masterList,  esxip,  cxip);
                                sourceList = SolutionConfig.GetHostList;
                                masterList = SolutionConfig.GetMasterList;
                                esxip = SolutionConfig.EsxIP;
                                cxip = SolutionConfig.csIP;
                            }
                            else
                            {
                                string cxipWithPort = WinTools.GetcxIPWithPortNumber();
                                MasterConfigFile master = new MasterConfigFile();
                                master.DownloadMasterConfigFile(cxipWithPort);
                                sol.ReadXmlConfigFileWithMasterList(masterConfigFile,  sourceList,  masterList,  esxip,  cxip);
                                sourceList = SolutionConfig.GetHostList;
                                masterList = SolutionConfig.GetMasterList;
                                esxip = SolutionConfig.EsxIP;
                                cxip = SolutionConfig.csIP;
                            }
                            break;
                        }
                    }
                    foreach (Host tempHost1 in allServersForm.selectedSourceListByUser._hostList)
                    {
                        foreach (Hashtable hash in tempHost1.disks.list)
                        {
                            if (hash["Protected"].ToString() == "no")
                            {
                                hash["scsi_mapping_vmx"] = null;
                            }
                        }
                        if (tempHost1.cluster == "yes" && tempHost1.operatingSystem.Contains("2003"))
                        {
                            Trace.WriteLine(DateTime.Now + " \t Printing protected disk count" + tempHost1.alreadyP2VDisksProtected.ToString());
                            Trace.WriteLine(DateTime.Now + "\t Came here to assign scsi values for 2003 cluser ");
                            foreach (Host h2 in sourceList._hostList)
                            {
                                if (tempHost1.displayname == h2.displayname || tempHost1.hostname == h2.hostname)
                                {
                                    tempHost1.disks.AppendScsiForP2vAdditionOfDiskFinal(h2.disks.list.Count);
                                }
                            }
                        }
                        else
                        {
                            Trace.WriteLine(DateTime.Now + " \t Printing protected disk count" + tempHost1.alreadyP2VDisksProtected.ToString());
                            tempHost1.disks.AppendScsiForP2vAdditionOfDiskFinal(tempHost1.alreadyP2VDisksProtected);
                        }
                        //ArrayList physicalDisks = tempHost1.disks.GetDiskList();
                    }
                    foreach (Host h in AdditionOfDiskSelectMachinePanelHandler.temperorySelectedSourceList._hostList)
                    {
                        foreach (Host tempHost in allServersForm.selectedSourceListByUser._hostList)
                        {
                            ArrayList diskList = new ArrayList();
                            diskList = h.disks.GetDiskList;
                            ArrayList diskHash = new ArrayList();
                            diskHash = tempHost.disks.GetDiskList;
                            foreach (Hashtable hash in diskList)
                            {
                                foreach (Hashtable disk in diskHash)
                                {//MessageBox.Show("Printing names  " + disk["Name"].ToString
                                    if (disk["Selected"].ToString() == "Yes" || disk["Protected"].ToString() == "yes")
                                    {
                                        if ((disk["disk_signature"] != null && !(disk["disk_signature"].ToString().ToUpper().Contains("SYSTEM"))))
                                        {
                                            if (disk["disk_signature"].ToString() == hash["disk_signature"].ToString())
                                            {
                                                hash["Protected"] = disk["Protected"].ToString();
                                                hash["Selected"] = disk["Selected"].ToString();
                                                hash["IsUsedAsSlot"] = "no";
                                                hash["scsi_mapping_vmx"] = disk["scsi_mapping_vmx"].ToString();
                                                hash["scsi_adapter_number"] = disk["scsi_mapping_vmx"].ToString().Split(':')[0];
                                                hash["scsi_slot_number"] = disk["scsi_mapping_vmx"].ToString().Split(':')[1];

                                                //Trace.WriteLine(DateTime.Now + " \t " + hash["Selected"].ToString() + hash["scsi_mapping_vmx"].ToString() + hash["scsi_adapter_number"].ToString());
                                            }
                                        }
                                        else if (disk["disk_signature"] == null || (disk["disk_signature"] != null && disk["disk_signature"].ToString().ToUpper().Contains("SYSTEM")))
                                        {
                                            if (disk["Name"].ToString() == hash["Name"].ToString())
                                            {
                                                hash["Protected"] = disk["Protected"].ToString();
                                                hash["Selected"] = disk["Selected"].ToString();
                                                hash["IsUsedAsSlot"] = "no";
                                                hash["scsi_mapping_vmx"] = disk["scsi_mapping_vmx"].ToString();
                                                hash["scsi_adapter_number"] = disk["scsi_mapping_vmx"].ToString().Split(':')[0];
                                                hash["scsi_slot_number"] = disk["scsi_mapping_vmx"].ToString().Split(':')[1];

                                                //Trace.WriteLine(DateTime.Now + " \t " + hash["Selected"].ToString() + hash["scsi_mapping_vmx"].ToString() + hash["scsi_adapter_number"].ToString());
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        /* if (AdditionOfDiskSelectMachinePanelHandler._tempSelectedSourceList.GenerateVmxFile() == false)
                         {
                             MessageBox.Show("Generating vmx file failed","Error",MessageBoxButtons.OK,MessageBoxIcon.Error);
                             return false;
                         }*/
                    }

                    //Checking for the add disk dulpcate names.

                    foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                    {
                        foreach (Hashtable diskHash in h.disks.list)
                        {
                            foreach (Hashtable hash in h.disks.list)
                            {
                                if (diskHash["scsi_mapping_vmx"] != null && hash["scsi_mapping_vmx"] != null)
                                {
                                    if (diskHash["scsi_mapping_vmx"].ToString() != hash["scsi_mapping_vmx"].ToString())
                                    {
                                        if (diskHash["Name"].ToString() == hash["Name"].ToString())
                                        {
                                            if (diskHash["Protected"].ToString().ToUpper() == "NO")
                                            {
                                                string name = diskHash["Name"].ToString();
                                                string vmxValue = diskHash["scsi_mapping_vmx"].ToString().Replace(":", "");
                                                string duplicateName = name.Replace(".vmdk", "");
                                                duplicateName = duplicateName + "_" + vmxValue + ".vmdk";
                                                diskHash["Name"] = duplicateName;

                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }


                    Esx _esxInfo = new Esx();
                    allServersForm.selectedSourceListByUser.Print();
                    ArrayList generatedEsxIps = new ArrayList();
                    bool generated = false;
                    string error = null;
                    bool ipReachable = false;
                    foreach (Host h in allServersForm.selectedMasterListbyUser._hostList)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Printing ip of mt " + h.ip);
                        if (h.ipCount > 1)
                        {
                            //Handling multiple ip addresses. A host can have couple of private of ip addresses and public ip addresses. This is for assigning right ip address to _selectedHost.Ip parameter

                            foreach (string ips in h.IPlist)
                            {
                                Trace.WriteLine(DateTime.Now + " \t Trying to reach " + h.displayname + " on " + ips);
                                if (WinPreReqs.IPreachable(ips) == true)
                                {
                                    h.ip = ips;
                                    Trace.WriteLine(DateTime.Now + " \t Successfully reached " + h.displayname + " on " + ips);
                                    ipReachable = true;
                                    break;
                                }
                            }
                            if (ipReachable == false)
                            {
                                error = Environment.NewLine + h.displayname + " is not reachable. Please check that firewall is not blocking ";
                                string message = h.displayname + " is not reachable on below IP Addresses" + Environment.NewLine + h.ip + Environment.NewLine + "Please check that firewall is not blocking ";
                                MessageBox.Show(message, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                return false;
                            }
                        }

                        //From vcon 4.0 release we are not going to dependent on wmi so we have commented code for asking credentials.
                        //if (_credentialPopup == true)
                        //{
                        //    if (h.hostname.Split('.')[0].ToUpper() != Environment.MachineName.ToUpper())
                        //    {
                        //        if (CommonTools.GetCredentials(allServersForm, ref h.domain, ref h.userName, ref h.passWord) == true)
                        //        {

                        //            allServersForm.Refresh();
                        //            Host h1 = new Host();
                        //            if (WinPreReqs.WindowsShareEnabled(h.ip, h.domain, h.userName, h.passWord, h.hostname, ref error) == false)
                        //            {
                        //                WmiErrorMessageForm errorMessageForm = new WmiErrorMessageForm(error);
                        //                errorMessageForm.domainText.Text = allServersForm.cachedDomain;
                        //                errorMessageForm.userNameText.Text = allServersForm.cachedUsername;
                        //                errorMessageForm.passWordText.Text = allServersForm.cachedPassword;
                        //                errorMessageForm.ShowDialog();


                        //                if (errorMessageForm.continueWithOutValidation == true)
                        //                {
                        //                    Trace.WriteLine(DateTime.Now + " USer selets for skip of wmi calls");
                        //                    h.skipPushAndPreReqs = true;
                        //                    _credentialPopup = false;
                        //                    return true;

                        //                }
                        //                else if (errorMessageForm._canceled == true)
                        //                {
                        //                    return false;

                        //                }
                        //                else
                        //                {
                        //                    Trace.WriteLine(DateTime.Now + "User selects for retry ");
                        //                    h.userName = errorMessageForm.username;
                        //                    h.passWord = errorMessageForm.password;
                        //                    h.domain = errorMessageForm.domain;
                        //                    h1 = h;
                        //                    RerunCredentials(allServersForm, ref  h1);
                        //                    allServersForm._selectedMasterList.AddOrReplaceHost(h1);

                        //                }
                        //            }
                        //            else
                        //            {
                        //                _credentialPopup = false;
                        //            }
                        //        }
                        //        else
                        //        {
                        //            return false;
                        //        }
                        //    }

                        //}
                    }
                    foreach (Host h in AdditionOfDiskSelectMachinePanelHandler.temperorySelectedSourceList._hostList)
                    {
                        foreach (Host h1 in allServersForm.selectedSourceListByUser._hostList)
                        {
                            if (h.hostname == h1.hostname)
                            {
                                h1.nicHash = h.nicHash;
                                h1.nicList = h.nicList;
                            }
                        }
                    }

                    // In case of addition of disk We need to add scsi_id_onmastertarget to the disk hash which is 
                    // Written by scripts at the time of issue. With out this we got one issue 
                    // TOsolve  [Bug 19129] New: 6.20.1>vContinuum>P2V>Recovered vm powered on failed due to Unable to access VMDK file
                    // So we have added this logic.

                    /* foreach (Host h in AdditionOfDiskSelectMachinePanelHandler._tempSelectedSourceList._hostList)
                     {
                         foreach (Host h1 in allServersForm._selectedSourceList._hostList)
                         {
                             ArrayList diskList = new ArrayList();
                             diskList = h.disks.GetDiskList();
                             ArrayList diskHash = new ArrayList();
                             diskHash = h1.disks.GetDiskList();
                             foreach (Hashtable hash in diskList)
                             {
                                 foreach (Hashtable disk in diskHash)
                                 {
                                     if(hash["scsi_mapping_vmx"] != null &&disk["scsi_mapping_vmx"] != null)
                                     if (hash["scsi_mapping_vmx"].ToString() == disk["scsi_mapping_vmx"].ToString() || hash["Name"].ToString() == disk["Name"].ToString())
                                     {
                                         if (hash["scsi_id_onmastertarget"] != null)
                                         {
                                             disk["scsi_id_onmastertarget"] = hash["scsi_id_onmastertarget"].ToString();
                                         }

                                     }

                                 }
                             }
                         }
                     }*/
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: ProcessPanel Of AddDiskPanelHandler " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            return true;
        }

        internal bool RerunCredentials(AllServersForm allServersForm, ref  Host h)
        {
            try
            { 
                //At the time of addition of disk we will ask mt credentials form the user.
                //we will check that credentials through wmi.
                Trace.WriteLine(DateTime.Now + " Came here to rerun the credentials in this process");
                string error = null;
                if (WinPreReqs.WindowsShareEnabled(h.ip, h.domain, h.userName, h.passWord,h.hostname,  error) == false)
                {
                    error = WinPreReqs.GetError;
                    WmiErrorMessageForm errorMessageForm = new WmiErrorMessageForm(error);
                    errorMessageForm.domainText.Text = allServersForm.cachedDomain;
                    errorMessageForm.userNameText.Text = allServersForm.cachedUsername;
                    errorMessageForm.passWordText.Text = allServersForm.cachedPassword;
                    errorMessageForm.ShowDialog();
                    if (errorMessageForm.continueWithOutValidation == true)
                    {
                        Trace.WriteLine(DateTime.Now + " USer selets for skip of wmi calls");
                        h.skipPushAndPreReqs = true;
                        credentialPopup = false;
                        object o = new object();
                        EventArgs e = new EventArgs();
                        allServersForm.nextButton_Click(0, e);
                        return true;
                        // allServersForm._selectedMasterList.AddOrReplaceHost(h);
                    }
                    else if (errorMessageForm.canceled == true)
                    {
                        return false;
                    }
                    else
                    {
                        Trace.WriteLine(DateTime.Now + "User selects for retry ");
                        h.userName = errorMessageForm.username;
                        h.passWord = errorMessageForm.password;
                        h.domain = errorMessageForm.domain;
                        allServersForm.cachedDomain = errorMessageForm.domain;
                        allServersForm.cachedUsername = errorMessageForm.username;
                        allServersForm.cachedPassword = errorMessageForm.password;
                        if (RerunCredentials(allServersForm, ref  h) == true)
                        {

                            allServersForm.selectedMasterListbyUser.AddOrReplaceHost(h);
                            credentialPopup = false;
                            object o = new object();
                            EventArgs e = new EventArgs();
                            allServersForm.nextButton_Click(0, e);
                            return true;
                        }
                    }
                }
                else
                {

                }
                
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: RerunCredentials method " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            return true;
        }

        internal override bool CanGoToNextPanel(AllServersForm allServersForm)
        {
            return true;            
        }
        internal override bool CanGoToPreviousPanel(AllServersForm allServersForm)
        {
            allServersForm.previousButton.Visible = false;
            return true;            
        }

        internal bool AddingDiskDetails(AllServersForm allServersForm, Host h)
        {
            try
            {
                //This will be callled when whne user selects particular vm to show the disk details. 
                ArrayList physicalDisks;
                int i = 0;
                h.Print();
                //h.displayname = allServersForm._currentDisplayedPhysicalHostIp;
                physicalDisks = h.disks.GetDiskList;
                allServersForm.newDiskDetailsDataGridView.Rows.Clear();
                allServersForm.newDiskDetailsDataGridView.RowCount = h.disks.GetDiskList.Count;
                Debug.WriteLine("Printing count of the disks  " + h.disks.GetDiskList.Count);
                foreach (Hashtable disk in physicalDisks)
                {
                    allServersForm.newDiskDetailsDataGridView.Rows[i].Cells[0].Value = "Disk" + i.ToString();
                    //allServersForm.newDiskDetailsDataGridView.Rows[i].Cells[2].Value = disk["Protected"];
                    if (disk["Protected"].ToString() == "no")
                    {
                        allServersForm.newDiskDetailsDataGridView.Rows[i].Cells[2].Value = "No";
                        // SelectDisk(allServersForm, i);
                        if (disk["Selected"].ToString() == "Yes")
                        {

                            if (disk["volumelist"] == null )
                            {
                                if (ResumeForm.selectedActionForPlan == "ESX")
                                {
                                    allServersForm.newDiskDetailsDataGridView.Rows[i].Cells[3].Value = true;
                                }
                                else
                                {
                                    if (ResumeForm.selectedActionForPlan != "ESX" && h.os == OStype.Windows)
                                    {
                                        disk["Selected"] = "No";
                                        allServersForm.newDiskDetailsDataGridView.Rows[i].Cells[3].Value = false;
                                        allServersForm.newDiskDetailsDataGridView.Rows[i].Cells[3].ReadOnly = true;
                                    }
                                    else
                                    {
                                        allServersForm.newDiskDetailsDataGridView.Rows[i].Cells[3].Value = true;
                                        allServersForm.newDiskDetailsDataGridView.Rows[i].Cells[3].ReadOnly = false;
                                    }
                                }
                            }
                            else
                            {
                                allServersForm.newDiskDetailsDataGridView.Rows[i].Cells[3].Value = true;
                                allServersForm.newDiskDetailsDataGridView.Rows[i].Cells[3].ReadOnly = false;
                            }
                           


                           
                        }
                        else
                        {
                            allServersForm.newDiskDetailsDataGridView.Rows[i].Cells[3].Value = false;
                            if (disk["volumelist"] == null)
                            {
                                if (ResumeForm.selectedActionForPlan == "ESX")
                                {
                                    //allServersForm.newDiskDetailsDataGridView.Rows[i].Cells[3].Value = true;
                                }
                                else
                                {
                                    if (ResumeForm.selectedActionForPlan != "ESX")
                                    {
                                        disk["Selected"] = "No";
                                        allServersForm.newDiskDetailsDataGridView.Rows[i].Cells[3].Value = false;
                                        allServersForm.newDiskDetailsDataGridView.Rows[i].Cells[3].ReadOnly = true;
                                    }
                                    else
                                    {
                                        allServersForm.newDiskDetailsDataGridView.Rows[i].Cells[3].Value = true;
                                        allServersForm.newDiskDetailsDataGridView.Rows[i].Cells[3].ReadOnly = false;
                                    }
                                }
                            }
                            allServersForm.newDiskDetailsDataGridView.RefreshEdit();
                        }
                    }
                    else
                    {
                        UnselectDisk(allServersForm, i);
                        allServersForm.newDiskDetailsDataGridView.Rows[i].Cells[2].Value = "Yes";
                        // h.targetDataStore = disk["datastore_selected"].ToString();
                        allServersForm.newDiskDetailsDataGridView.Rows[i].Cells[3].ReadOnly = true;
                        allServersForm.newDiskDetailsDataGridView.Rows[i].Cells[3].Value = false;
                        disk["Selected"] = "No";
                    }
                    allServersForm.newDiskDetailsDataGridView.Rows[i].Cells[4].Value = "Details";
                    //Adding when the click event done in the primaryDataGridView ip column 
                    // 
                    if (disk["Size"] != null)
                    {
                        float size = float.Parse(disk["Size"].ToString());
                        size = size / (1024 * 1024);
                        //size = int.Parse(String.Format("{0:##.##}", size));
                        size = float.Parse(String.Format("{0:00000.000}", size));
                        allServersForm.newDiskDetailsDataGridView.Rows[i].Cells[1].Value = size;
                        i++;
                        Debug.WriteLine("************Physical Disk name = " + disk["Name"]);
                        Debug.WriteLine("************Physical Disk Size = " + disk["Size"]);
                        Debug.WriteLine("************Physical Disk Selected = " + disk["Selected"]);
                    }
                }
                allServersForm.currentDisplayedPhysicalHostIPselected = h.displayname;
                allServersForm.newDiskDetailsDataGridView.Visible = true;
                Debug.WriteLine("______________________________________    before dispalydriverdetails printing _selected SourceList");
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
        internal bool SelectDisk(AllServersForm allServersForm, int index)
        {
            try
            {
                // this will be called when user selects the disks
                Host tempHost = new Host();
                tempHost.displayname = allServersForm.currentDisplayedPhysicalHostIPselected;
                int listIndex = 0;
                if (allServersForm.selectedSourceListByUser.DoesHostExist(tempHost, ref listIndex) == true)
                {
                    Debug.WriteLine("Host exists in the source host list ");
                    ((Host)allServersForm.selectedSourceListByUser._hostList[listIndex]).SelectDisk(index);
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

        internal bool UnselectDisk(AllServersForm allServersForm, int index)
        {
            try
            {
                //This will be called when user unselects the disks for a particular vm.
                Host tempHost = new Host();
                tempHost.displayname = allServersForm.currentDisplayedPhysicalHostIPselected;
                int listIndex = 0;
                if (allServersForm.selectedSourceListByUser.DoesHostExist(tempHost, ref listIndex) == true)
                {
                    Debug.WriteLine("Host exists in the source host list ");
                    ((Host)allServersForm.selectedSourceListByUser._hostList[listIndex]).UnselectDisk(index);
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


        internal bool DisplayDiskDetails(AllServersForm allServersForm, int index)
        {
            try
            {
                Host tempHost = new Host();
                tempHost.displayname = allServersForm.currentDisplayedPhysicalHostIPselected;
                int listIndex = 0;
                if (allServersForm.selectedSourceListByUser.DoesHostExist(tempHost, ref listIndex) == true)
                {
                    tempHost = (Host)allServersForm.selectedSourceListByUser._hostList[listIndex];
                    Hashtable hash = (Hashtable)tempHost.disks.list[index];

                    DiskDetails disk = new DiskDetails();
                    disk.descriptionLabel.Text = "Details of: " + allServersForm.newDiskDetailsDataGridView.Rows[index].Cells[0].Value.ToString();
                    disk.detailsDataGridView.Rows.Clear();

                    disk.detailsDataGridView.Rows.Add(1);
                    disk.detailsDataGridView.Rows[0].Cells[0].Value = "VMDK name";
                    string[] disks = hash["Name"].ToString().Split(']');
                    if (disks.Length == 2)
                    {
                        disk.detailsDataGridView.Rows[0].Cells[1].Value = disks[1].ToString();
                    }
                    else
                    {
                        disk.detailsDataGridView.Rows[0].Cells[1].Value = hash["Name"].ToString();
                    }

                    disk.detailsDataGridView.Rows.Add(1);

                    disk.detailsDataGridView.Rows[1].Cells[0].Value = "SCSI ID";
                    if (ResumeForm.selectedActionForPlan == "ESX")
                    {
                        if (hash["source_disk_uuid"] != null)
                        {
                            disk.detailsDataGridView.Rows[1].Cells[1].Value = hash["source_disk_uuid"].ToString();
                        }
                        else
                        {
                            disk.detailsDataGridView.Rows[1].Cells[1].Value = "Not Available";
                        }
                    }
                    else
                    {
                        if (hash["DiskScsiId"] != null)
                        {
                            disk.detailsDataGridView.Rows[1].Cells[1].Value = hash["DiskScsiId"].ToString();
                        }
                        else
                        {
                            disk.detailsDataGridView.Rows[1].Cells[1].Value = "Not Available";
                        }
                    }




                    disk.detailsDataGridView.Rows.Add(1);
                    if (hash["src_devicename"] != null)
                    {
                        disk.detailsDataGridView.Rows[2].Cells[0].Value = "Name";
                        disk.detailsDataGridView.Rows[2].Cells[1].Value = hash["src_devicename"].ToString();
                    }
                    else
                    {
                        disk.detailsDataGridView.Rows[2].Cells[0].Value = "Name";
                        disk.detailsDataGridView.Rows[2].Cells[1].Value = "Not Available";
                    }


                    disk.detailsDataGridView.Rows.Add(1);
                    disk.detailsDataGridView.Rows[3].Cells[0].Value = "Size in KB(s)";
                    disk.detailsDataGridView.Rows[3].Cells[1].Value = hash["Size"].ToString();


                    disk.detailsDataGridView.Rows.Add(1);
                    disk.detailsDataGridView.Rows[4].Cells[0].Value = "Type";
                    try
                    {
                        if (hash["disk_type_os"] == null && hash["disk_type"] != null)
                        {
                            hash.Add("disk_type_os", hash["disk_type"].ToString());
                        }
                    }
                    catch (Exception ex)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Failed to update values of hash " + ex.Message);
                    }
                    if (hash["disk_type_os"] != null && hash["DiskLayout"] != null)
                    {
                        if (hash["efi"] != null)
                        {
                            disk.detailsDataGridView.Rows[4].Cells[1].Value = hash["disk_type_os"].ToString() + ", Layout ->" + hash["DiskLayout"].ToString() + ", EFI -> " + hash["efi"].ToString();
                        }
                        else
                        {
                            disk.detailsDataGridView.Rows[4].Cells[1].Value = hash["disk_type_os"].ToString() + ", Layout -> " + hash["DiskLayout"].ToString();
                        }
                    }
                    else
                    {
                        disk.detailsDataGridView.Rows[4].Cells[1].Value = "Not Available";
                    }



                    disk.detailsDataGridView.Rows.Add(1);
                    if (hash["disk_signature"] != null)
                    {
                        disk.detailsDataGridView.Rows[5].Cells[0].Value = "Disk Signature";
                        disk.detailsDataGridView.Rows[5].Cells[1].Value = hash["disk_signature"].ToString();
                    }
                    else
                    {
                        disk.detailsDataGridView.Rows[5].Cells[0].Value = "Disk Signature";
                        disk.detailsDataGridView.Rows[5].Cells[1].Value = "Not Available";
                    }
                    disk.detailsDataGridView.Rows.Add(1);
                    if (hash["volumelist"] != null)
                    {
                        disk.detailsDataGridView.Rows[6].Cells[0].Value = "Volumes";
                        disk.detailsDataGridView.Rows[6].Cells[1].Value = hash["volumelist"].ToString();
                    }
                    else
                    {
                        disk.detailsDataGridView.Rows[6].Cells[0].Value = "Volumes";
                        disk.detailsDataGridView.Rows[6].Cells[1].Value = "Not Available";
                    }
                    disk.yesButton.Visible = false;
                    disk.ShowDialog();
                }

            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: DislpayDiskDetails " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");

                return false;
            }
            return true;
        }



        internal bool SelectDiskOrUnSelectDisk(AllServersForm allServersForm, int index)
        {
            //if user want to unselect the disk which he does not want to protect particular disk....
            try
            {
                if ((bool)allServersForm.newDiskDetailsDataGridView.Rows[index].Cells[AdditionDiskCheckBoxColumn].FormattedValue)
                {
                    Host tempHost = new Host();
                    tempHost.displayname = allServersForm.currentDisplayedPhysicalHostIPselected;
                    int listIndex = 0;
                    if (allServersForm.selectedSourceListByUser.DoesHostExist(tempHost, ref listIndex) == true)
                    {
                        Debug.WriteLine("Host exists in the source host list ");
                        ((Host)allServersForm.selectedSourceListByUser._hostList[listIndex]).SelectDisk(index);


                        Host selectedHost = (Host)allServersForm.selectedSourceListByUser._hostList[listIndex];

                        Hashtable hash = (Hashtable)selectedHost.disks.list[index];

                        if (hash["disk_signature"] != null && hash["cluster_disk"] != null && hash["cluster_disk"].ToString() == "yes")
                        {
                            foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                            {
                                foreach (Hashtable sourceHash in h.disks.list)
                                {
                                    if (sourceHash["disk_signature"] != null && hash["disk_signature"].ToString() == sourceHash["disk_signature"].ToString())
                                    {
                                        sourceHash["Selected"] = "Yes";
                                    }
                                }
                            }
                        }

                    }
                }
                else
                {
                    Host tempHost = new Host();
                    tempHost.displayname = allServersForm.currentDisplayedPhysicalHostIPselected;
                    int listIndex = 0;
                    if (allServersForm.selectedSourceListByUser.DoesHostExist(tempHost, ref listIndex) == true)
                    {
                        Debug.WriteLine("Host exists in the source host list ");
                        ((Host)allServersForm.selectedSourceListByUser._hostList[listIndex]).UnselectDisk(index);

                        Host selectedHost = (Host)allServersForm.selectedSourceListByUser._hostList[listIndex];
                        Hashtable hash = (Hashtable)selectedHost.disks.list[index];
                        if (hash["disk_signature"] != null && hash["cluster_disk"] != null && hash["cluster_disk"].ToString() == "yes")
                        {
                            foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                            {
                                foreach (Hashtable sourceHash in h.disks.list)
                                {
                                    if (sourceHash["disk_signature"] != null && hash["disk_signature"].ToString() == sourceHash["disk_signature"].ToString())
                                    {
                                        sourceHash["Selected"] = "No";
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
                return false;
            }
            return true;
        }
    }
}
