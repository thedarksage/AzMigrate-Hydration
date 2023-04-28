using System;
using System.Collections.Generic;
using System.Text;
using Com.Inmage;
using System.Windows.Forms;
using System.Threading;
using Com.Inmage.Esxcalls;
using System.Collections;
using Com.Inmage.Tools;
using System.IO;
using System.Drawing;
using System.Diagnostics;

using System.Text.RegularExpressions;


namespace Com.Inmage.Wizard
{
    class ESX_PrimaryServerPanelHandler : PanelHandler
    {

        internal Esx _esxInfo = new Esx();
        //public static int ESX_IP_COLUMN           = 0;
        internal static int DISPLAY_NAME_COLUMN = 0;
        internal static int IP_COLUMN = 1;
        internal static int POWER_STATUS_COLUMN = 2;
        internal static int VMWARE_TOOLS_COLUMN = 3;
        internal static int SOURCE_CHECKBOX_COLUMN = 4;
        internal static int NAT_IP_COLUMN = 5;
        string _vmUUID;
        AddCredentialsPopUpForm addCredentialsForm;
        internal bool powerOn;
        internal string _esxHostIP = null;
        internal string _esxUserName = null;
        internal string _esxPassWord = null;
        internal static bool _credentialsCheckPassed = false;
        internal string _ip = "", _username = "", _password = "", _domain = "";
        internal int _credentialIndex = 0;
        internal bool _wmiErrorCheckCanceled = false;
        internal static string SOURCEXML = "INFO.xml";
        Host _selectedHost;
        int _check = 0;
        internal HostList _powerOnList = new HostList();
        int _getInfoReturnCode = 0;
        int _powerOnReturnCode = 0;
        internal string _latestFilePath = null;
        internal string _skipData = null;
        ToolTip toolTipforOsDisplay = new ToolTip();
        internal TreeNode _currentNode;
        internal bool _vmExistsinCXDataBase = false;
        public static ArrayList _clutserList = new ArrayList();

        public ESX_PrimaryServerPanelHandler()
        {

            _latestFilePath = WinTools.FxAgentPath() + "\\vContinuum\\Latest";
        }

        /*
         * In case of failback we will get the Info.xml and load the seleted valuyes into the Treeview.
         * In case of normal protection we will just fill the fhelp content.
         */
        internal override bool Initialize(AllServersForm allServersForm)
        {
            try
            {
                Trace.WriteLine(DateTime.Now + " \t Entered: Esx_AddSourcePanelHandler Initialize  ");
                
                _latestFilePath = WinTools.FxAgentPath() + "\\vContinuum\\Latest";
                if (allServersForm.appNameSelected == AppName.Failback)
                {
                    // At the time of failback we are not showing to select the vms
                    // Vms which are selected in first should be failback...
                    allServersForm.mandatoryLabel.Visible = false;
                    allServersForm.addSourceTreeView.CheckBoxes = false;
                    if (ResumeForm.selectedVMListForPlan._hostList.Count != 0)
                    {
                        AdditionOfDiskSelectMachinePanelHandler.esxInfoAdddisk = ResumeForm._esxInfo;
                        AdditionOfDiskSelectMachinePanelHandler.temperorySelectedSourceList = ResumeForm.selectedVMListForPlan;
                        AdditionOfDiskSelectMachinePanelHandler.sourceListPrepared = ResumeForm.sourceHostsList;
                        Host h = (Host)ResumeForm.selectedVMListForPlan._hostList[0];
                        AdditionOfDiskSelectMachinePanelHandler.sourceEsxIPxml = h.esxIp;
                        allServersForm.addSourceRefreshLinkLabel.Visible = true;
                        allServersForm.osTypeSelected = h.os;
                        allServersForm.previousButton.Visible = false;
                        allServersForm.planInput = h.plan;
                        _esxHostIP = h.targetESXIp;
                        _esxUserName = h.targetESXUserName;
                        _esxPassWord = h.targetESXPassword;
                        Cxapicalls cxapi = new Cxapicalls();
                        cxapi.UpdaterCredentialsToCX(h.targetESXIp, _esxUserName, _esxUserName);
                        _esxUserName = Cxapicalls.GetUsername;
                        _esxPassWord = Cxapicalls.GetPassword;
                    }
                }
                else if (allServersForm.appNameSelected == AppName.V2P)
                {
                    allServersForm.previousButton.Visible = false;
                    allServersForm.mandatoryLabel.Visible = false;
                    AdditionOfDiskSelectMachinePanelHandler.temperorySelectedSourceList = ResumeForm.selectedVMListForPlan;
                    AdditionOfDiskSelectMachinePanelHandler.sourceListPrepared = ResumeForm.sourceHostsList;
                    Host host = (Host)ResumeForm.selectedVMListForPlan._hostList[0];

                    Host sourceHost = (Host)ResumeForm.selectedVMListForPlan._hostList[0];
                    //sourceHost = h;
                    ArrayList nicList = sourceHost.nicList.list;
                    allServersForm.addSourceTreeView.CheckBoxes = false;
                    Cxapicalls cxAPi = new Cxapicalls();
                    if (cxAPi.Post( host, "GetHostInfo") == true)
                    {
                        host = Cxapicalls.GetHost;
                        host.role = Role.Primary;
                        host.displayname = host.hostname;
                    }
                    foreach (Hashtable hash in nicList)
                    {
                        Trace.WriteLine(DateTime.Now + "\t hash count " + hash.Count.ToString());
                    }
                    allServersForm.osTypeSelected = host.os;
                    allServersForm.addSourceTreeView.Nodes.Add("Machine for Failback");
                    allServersForm.addSourceTreeView.Nodes[0].ImageIndex = 0;
                    allServersForm.addSourceTreeView.Nodes[0].Nodes.Add(host.displayname).Text = host.displayname;
                    allServersForm.addSourceTreeView.Nodes[0].Nodes[0].ImageIndex = 1;
                    allServersForm.addSourceTreeView.Nodes[0].Nodes[0].SelectedImageIndex = 1;
                    allServersForm.selectedSourceListByUser = new HostList();
                    try
                    {
                        //sourceHost.nicList.list = nicList;
                        //h.nicList.list = sourceHost.nicList.list;
                        foreach (Hashtable hash in host.nicList.list)
                        {
                            foreach (Hashtable actualHash in nicList)
                            {
                                if (actualHash["new_macaddress"] != null && hash["nic_mac"] != null)
                                {

                                    Trace.WriteLine(DateTime.Now + "\t comparing nic mac " + actualHash["new_macaddress"].ToString() + "\t " + hash["nic_mac"].ToString());
                                    if (actualHash["new_macaddress"].ToString().ToUpper() == hash["nic_mac"].ToString().ToUpper())
                                    {
                                        Trace.WriteLine(DateTime.Now + "\t Both new and old mac addresses are not null");
                                        
                                        try
                                        {

                                            if (actualHash["nic_mac"] != null)
                                            {
                                                hash["new_macaddress"] = actualHash["nic_mac"].ToString();
                                                Trace.WriteLine(DateTime.Now + "\t Updated new mac as " + hash["new_macaddress"].ToString());
                                            }
                                            else
                                            {
                                                Trace.WriteLine(DateTime.Now + "\t In Masterconfigfile nic mac is null ");
                                            }
                                            if (actualHash["ip"] != null)
                                            {
                                                hash["new_ip"] = actualHash["ip"].ToString();
                                                Trace.WriteLine(DateTime.Now + "\t Updated new ip as " + actualHash["ip"].ToString());
                                            }

                                            if (hash["mask"] != null)
                                            {
                                                hash["new_mask"] = actualHash["mask"].ToString();
                                                Trace.WriteLine(DateTime.Now + "\t Updated new mask as " + actualHash["mask"].ToString());
                                            }

                                            if (actualHash["dnsip"] != null)
                                            {
                                                hash["new_dnsip"] = actualHash["dnsip"].ToString();
                                                Trace.WriteLine(DateTime.Now + "\t Updated dns ip as " + actualHash["dnsip"].ToString());
                                            }

                                            if (actualHash["gateway"] != null)
                                            {
                                                hash["new_gateway"] = actualHash["gateway"].ToString();
                                                Trace.WriteLine(DateTime.Now + "\t updated new gatway as " + actualHash["gateway"].ToString());
                                            }                                

                                        }
                                        catch (Exception ex)
                                        {
                                            Trace.WriteLine(DateTime.Now + "\t Failed to update ips " + ex.Message);
                                        }
                                        //Trace.WriteLine(DateTime.Now + "\t Actual old mac address " + hash["nic_mac"].ToString() + "\t new mac address " + hash["new_macaddress"].ToString());
                                        
                                    }
                                }
                                else
                                {
                                    Trace.WriteLine(DateTime.Now + "\t Found one of the mac address value as null for the machine " + host.displayname);
                                }
                            }
                        }
                    }
                    catch (Exception ex)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Failed to update the nic macs " + ex.Message);
                    }
                    allServersForm.selectedSourceListByUser.AddOrReplaceHost(host);
                    allServersForm.initialSourceListFromXml = new HostList();
                    allServersForm.initialSourceListFromXml.AddOrReplaceHost(host);
                    allServersForm.addSourceTreeView.Visible = true;
                    allServersForm.addSourceTreeView.ExpandAll();
                    allServersForm.addSourceTreeView.SelectedNode = allServersForm.addSourceTreeView.Nodes[0].Nodes[0];

                }
                else
                {
                    allServersForm.addCredentialsLabel.Text = "Select Primary VM (s)";
                }
                //when user select fail back then we are making the datagridview check box column visible as false...
                //because user can't unselect the machine from the protection....
               
                if (allServersForm.appNameSelected == AppName.Failback)
                {
                    allServersForm.Text = "Failback";
                    allServersForm.helpContentLabel.Text = HelpForcx.ProtectScreen1;
                    if (ResumeForm.selectedVMListForPlan._hostList.Count == 0)
                    {
                        allServersForm.previousButton.Visible = true;
                    }
                    allServersForm.addSourceTreeView.CheckBoxes = false;
                    allServersForm.esxHostIpAddressLabel.Visible = false;
                    allServersForm.esxHostIpText.Visible = false;
                    allServersForm.sourceUserNameLabel.Visible = false;
                    allServersForm.sourceUserNameText.Visible = false;
                    allServersForm.sourcePassWordLabel.Visible = false;
                    allServersForm.sourcePassWordText.Visible = false;
                    allServersForm.addEsxButton.Visible = false;
                    allServersForm.osTypeLabel.Visible = false;
                    allServersForm.osTypeComboBox.Visible = false;
                    allServersForm.addSourcePanelheadingLabel.Visible = false;
                    //GetEsxDetails(allServersForm);
                    allServersForm.diskDetailsHeadingLabel.Visible = false;
                    //allServersForm.esxDiskInfoDatagridView.Enabled = false;
                    ReloadTreeView(allServersForm);

                    foreach (Host h in ResumeForm.selectedVMListForPlan._hostList)
                    {
                        foreach (Host sourceHost in allServersForm.selectedSourceListByUser._hostList)
                        {
                            if (h.target_uuid == sourceHost.source_uuid || h.new_displayname == sourceHost.displayname)
                            {
                                foreach (Hashtable hash in h.disks.list)
                                {
                                    foreach (Hashtable sourceHash in sourceHost.disks.list)
                                    {
                                        if (hash["target_disk_uuid"] != null && sourceHash["source_disk_uuid"] != null)
                                        {
                                            if (hash["target_disk_uuid"].ToString() == sourceHash["source_disk_uuid"].ToString())
                                            {
                                                if (hash["volumelist"] != null)
                                                {
                                                    sourceHash["volumelist"] = hash["volumelist"];
                                                }

                                                if (hash["disk_signature"] != null)
                                                {
                                                    sourceHash["disk_signature"] = hash["disk_signature"];
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                }
                else if (allServersForm.appNameSelected == AppName.V2P)
                {
                    allServersForm.addSourceTreeView.CheckBoxes = false;
                    allServersForm.esxHostIpAddressLabel.Visible = false;
                    allServersForm.esxHostIpText.Visible = false;
                    allServersForm.sourceUserNameLabel.Visible = false;
                    allServersForm.sourceUserNameText.Visible = false;
                    allServersForm.sourcePassWordLabel.Visible = false;
                    allServersForm.sourcePassWordText.Visible = false;
                    allServersForm.addEsxButton.Visible = false;
                    allServersForm.osTypeLabel.Visible = false;
                    allServersForm.osTypeComboBox.Visible = false;
                    allServersForm.addSourcePanelheadingLabel.Visible = false;
                    //GetEsxDetails(allServersForm);
                    allServersForm.diskDetailsHeadingLabel.Visible = false;
                   // allServersForm.esxDiskInfoDatagridView.Enabled = false;

                    foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                    {
                        if (h.cluster == "yes")
                        {
                            allServersForm.esxDiskInfoDatagridView.Enabled = true;
                        }
                    }
                }
                else
                { // WE come here for offliny sync export and normal protection.

                    if (allServersForm.appNameSelected == AppName.Esx)
                    {
                        allServersForm.Text = "Protect";
                        allServersForm.helpContentLabel.Text = HelpForcx.ProtectScreen1;
                    }

                    if (allServersForm.appNameSelected == AppName.Offlinesync)
                    {
                        allServersForm.Text = "Offline Sync Export";
                        allServersForm.helpContentLabel.Text = HelpForcx.OfflinesyncExport;
                    }
                    allServersForm.osTypeComboBox.Items.Add("Windows");
                    allServersForm.osTypeComboBox.Items.Add("Linux");
                    allServersForm.osTypeComboBox.Items.Add("Solaris");
                    allServersForm.osTypeComboBox.SelectedItem = "Windows";
                    if (allServersForm.appNameSelected == AppName.Pushagent)
                    {
                        allServersForm.addSourcePanelheadingLabel.Text = "Select vSphere server to push agents";
                        allServersForm.p2vHeadingLabel.Text = "Push Agent";
                        allServersForm.osTypeComboBox.Enabled = false;
                        //allServersForm.addSourcePanelEsxDataGridView.Columns[NAT_IP_COLUMN].Visible = false;
                        allServersForm.Text = "Push Agent";
                        allServersForm.addCredentialsLabel.Text = "Select vSphere server";
                    }
                    else
                    {
                        allServersForm.p2vHeadingLabel.Text = "Protect";
                    }
                    allServersForm.esxHostIpText.Select();
                    allServersForm.osTypeComboBox.TabIndex = 4;
                    allServersForm.addEsxButton.TabIndex = 5;
                    allServersForm.addSourceTreeView.TabIndex = 6;
                    allServersForm.addDiskDetailsDataGridView.TabIndex = 7;
                    allServersForm.nextButton.TabIndex = 8;
                    allServersForm.cancelButton.TabIndex = 9;
                    allServersForm.cancelButton.TabStop = true;
                    allServersForm.nextButton.Enabled = false;
                    allServersForm.previousButton.Visible = false;
                }

                
               // if(allServersForm._appName == 
                //allServersForm.helpContentRichTextBox.Text = "";
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
           // allServersForm.helpContentLabel.AppendText("After that you can select the primary vms" + Environment.NewLine);
            Trace.WriteLine(DateTime.Now + " \t  Exiting: Esx_AddSourcePanelHandler Initialize  ");
            return true;
        }

        internal override bool CanGoToNextPanel(AllServersForm allServersForm)
        {

            // While user selects next it will come here always when it is in first screen
            Trace.WriteLine(DateTime.Now + " \t Entered: CanGoToNextPanel  " );
            return true;
           
        }

        /*
         * As the flow got chnaged. Now we are not using the method.For the purpose of upgrading we had keep this logic.
         * In futrue we can remove the logic but node method because it is a overide for all panel.
         */
        internal override bool CanGoToPreviousPanel(AllServersForm allServersForm)
        {
            try
            {
                // only in case of old version (5.5) it will come here 
                // From 6.0 we had a new flow...
                Trace.WriteLine(DateTime.Now + " \t Entered: CanGoToPreviousPanel  ");
                if (allServersForm.appNameSelected == AppName.Failback ||  allServersForm.appNameSelected == AppName.V2P)
                {
                    allServersForm.selectMasterTargetTreeView.Nodes.Clear();
                    allServersForm.selectMasterTargetTreeView.Visible = false;
                    allServersForm.initialMasterListFromXml = new HostList();
                    allServersForm.selectedMasterListbyUser = new HostList();
                    allServersForm.initialMasterListFromXml._hostList.Clear();
                    allServersForm.selectedMasterListbyUser._hostList.Clear();
                    allServersForm.selectSecondaryRefreshLinkLabel.Visible = false;
                    allServersForm.powerOffNoteLabel.Visible = false;
                    allServersForm.selectMasterTargetLabel.Visible = false;
                    allServersForm.starLabel.Visible = false;
                    allServersForm.starforLabel.Visible = false;
                    allServersForm.previousButton.Visible = false;
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
            return true ;
        }
        /*
         * In case of failback we will check for the power status, ip, hostname, vm ware tools status.
         * We will calculate the disk sizes for each vm for the datastroe seletion page.
         * One error is also there in this method like
         * For each VM at least one disk should be selected.
         */
        internal override bool ProcessPanel(AllServersForm allServersForm)
        {
            try
            {
                Trace.WriteLine(DateTime.Now + " \t Entered: Esx_AddSourcePanelHandler ProcessPanel  ");


                if (allServersForm.appNameSelected == AppName.Failback || allServersForm.appNameSelected == AppName.V2P)
                {
                    if (allServersForm.addSourceTreeView.Nodes.Count == 0)
                    {
                        MessageBox.Show("There are no VMs available for failback protection", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return false;
                    }
                    
                }


                // At the time of the failback we are asking domain,username,password at the time of the selection....
                //so here we are comparing the sourcelist and assigning the values to the current primary list......
                if (allServersForm.appNameSelected == AppName.Failback)
                {

                    foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                    {
                        foreach (Host h1 in AdditionOfDiskSelectMachinePanelHandler.temperorySelectedSourceList._hostList)
                        {
                            if (h1.displayname == h.displayname)
                            {
                                h.userName = h1.userName;
                                h.passWord = h1.passWord;
                                h.domain = h1.domain;
                            }
                        }
                    }
                    foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                    {
                        if (h.poweredStatus == "OFF")
                        {
                            MessageBox.Show("One or more selected VMs are powered off. Please  power them on to proceed", "vContinuum error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            return false;
                        }
                        if (h.datacenter == null)
                        {
                            MessageBox.Show("VM: " + h.displayname + "has null datacenter value. Unable to proceed.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            return false;
                        }
                        if (h.vmwareTools.ToUpper().ToString() == "NotInstalled".ToUpper().ToString() || h.vmwareTools.ToUpper().ToString() == "NotRunning".ToUpper().ToString())
                        {
                            MessageBox.Show("Please make sure VMware tools are installed and running on VM: " + h.displayname, "vContinuum error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            return false;
                        }
                        if (h.hostname == "NOT SET")
                        {
                            MessageBox.Show("Please make sure hostname is set on VM: " + h.displayname, "vContinuum Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            return false;
                        }
                        if (h.ip == "NOT SET")
                        {
                            MessageBox.Show("Please make sure IP address is set on VM: " + h.displayname, "vContinuum Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            return false;

                        }


                    }
                }
                try
                {
                    if (allServersForm.appNameSelected != AppName.V2P)
                    {
                        foreach (Host h1 in allServersForm.selectedSourceListByUser._hostList)
                        {
                            ArrayList physicalDisks;

                            //at the time of fails back also we are asking the user if there is rdmp disk if there wwe are
                            //asking user that sahll we convert to vmdk or not.....l
                            if (allServersForm.appNameSelected == AppName.Failback)
                            {
                                foreach (Host previousProtectionHost in AdditionOfDiskSelectMachinePanelHandler.sourceListPrepared._hostList)
                                {
                                    //MessageBox.Show("Came to verify displaynames" + h1.displayname + "  " + previousProtectionHost.displayname + " " + previousProtectionHost.convertRdmpDiskToVmdk);

                                    if (h1.displayname == previousProtectionHost.displayname)
                                    {
                                        if (previousProtectionHost.rdmpDisk == "TRUE")
                                        {
                                            h1.rdmpDisk = previousProtectionHost.rdmpDisk;
                                            h1.convertRdmpDiskToVmdk = previousProtectionHost.convertRdmpDiskToVmdk;

                                        }
                                    }




                                }
                                foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                                {
                                    if (CheckIdeDisk(allServersForm, h) == false)
                                    {

                                        return false;
                                    }
                                }
                            }

                            //calculating the total size of the disks......
                            physicalDisks = h1.disks.GetDiskList;
                            h1.totalSizeOfDisks = 0;
                            h1.selectedDisk = false;
                            foreach (Hashtable disk in physicalDisks)
                            {
                                if (disk["Selected"].ToString() == "Yes")
                                {
                                    h1.selectedDisk = true;
                                    if (disk["Rdm"].ToString() == "no" || h1.convertRdmpDiskToVmdk == true)
                                    {
                                        if (disk["Size"] != null)
                                        {
                                            float size = float.Parse(disk["Size"].ToString());
                                            size = size / (1024 * 1024);
                                            h1.totalSizeOfDisks = h1.totalSizeOfDisks + size;
                                        }
                                    }
                                }
                            }
                        }



                        //evry time when user came to the process we are asking the user if any dynamic disks are there are not if so we are not going to support...
                        //if (allServersForm._osType == OStype.Windows)
                        {
                            foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                            {
                                if (h.selectedDisk == false)
                                {
                                    MessageBox.Show("VM: " + h.displayname + " None of the disks are selected for protecion.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                    return false;
                                }
                            }
                        }
                    }
                    /*
                    if (allServersForm._osType == OStype.Windows)
                    {
                        switch (MessageBox.Show("vContinuum does not support protection of VMs with dynamic disks.\n Does the selected VM have dynamic disks? ", " Dynamic disk verification ", MessageBoxButtons.YesNo, MessageBoxIcon.Question))
                        {
                            case DialogResult.Yes:
                                MessageBox.Show("Dynamic disks are not supported. Exiting ...", "vContinuum");
                                allServersForm.Close();
                                return false;





                        }
                    }
                     * */
                    //currentForm._selectedSourceList.GenerateVmxFile(); 

                }
                catch (Exception ex)
                {
                    System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                    Trace.WriteLine("ERROR*******************************************");
                    Trace.WriteLine(DateTime.Now + "Caught exception at Method: checking selected disks " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                    Trace.WriteLine("Exception  =  " + ex.Message);
                    Trace.WriteLine("ERROR___________________________________________");
                    return false;
                }
            
            
                try
                {
                    if (allServersForm.appNameSelected == AppName.Esx)
                    {
                        WinPreReqs win = new WinPreReqs("", "", "", "");
                        string cxip = WinTools.GetcxIPWithPortNumber();
                        foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                        {

                            Host tempHost = new Host();
                            tempHost = h;
                            if (tempHost.inmage_hostid == null)
                            {
                                if (win.GetDetailsFromcxcli( tempHost, cxip) == true)
                                {
                                    tempHost = WinPreReqs.GetHost;
                                }
                            }
                            Cxapicalls cxAPi = new Cxapicalls();
                            cxAPi.PostRefreshWithmbrOnly( tempHost);
                            tempHost = Cxapicalls.GetHost;
                            h.requestId = tempHost.requestId;
                        }
                    }
                }
                catch (Exception ex)
                {
                    Trace.WriteLine(DateTime.Now + "\t Failed to call refresh host " + ex.Message) ;
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

            Trace.WriteLine(DateTime.Now + " \t Exiting: Esx_AddSourcePanelHandler ProcessPanel  " );

            return true;
    

        }

       
        /*
         * This method will checks the Count of the selected VM(s).
         * If count is zero we will block user here only.
         */
        internal override bool ValidatePanel(AllServersForm allServersForm)
        {
            try
            {
                // for every screen we have validate panel this sholud be used as a pre-req check....
                Trace.WriteLine(DateTime.Now + " \t Entered: ValidatePanel Esx_AddSourcePanelHandler.cs  ");
                if (allServersForm.selectedSourceListByUser._hostList.Count == 0)
                {
                    MessageBox.Show("Please select at least one machine ", "No VM selected", MessageBoxButtons.OK, MessageBoxIcon.Error);
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

                return false;
            }
            Trace.WriteLine(DateTime.Now + " \t Exiting: ValidatePanel Esx_AddSourcePanelHandler.cs  ");
            return true;
        }

        internal bool AddCredentials(AllServersForm allServersForm)
        {
            
            return true;
        }
        /*
         * This method will be called by the Get details button in first screen.
         * It will checks the Info.xl in latest folder and delete if exists.
         * After that we wil call the getInfo.pl by using backgroundworker.
         * Backgrounderwoker(esxDetailsBackgroundWorker) do work will call the Getinfo.pl 
         */
        internal bool GetEsxDetails(AllServersForm allServersForm)
        {
            try
            {

                if (File.Exists(_latestFilePath + "\\Info.xml"))
                {
                    File.Delete(_latestFilePath + "\\Info.xml");
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

            Trace.WriteLine(DateTime.Now + " \t Entered: AddSourcePanelHandler:GetSourceEsxDetails ");
            //checking the length of the ip textbox...
            try
            {
                if (allServersForm.appNameSelected == AppName.Esx || allServersForm.appNameSelected == AppName.Offlinesync || allServersForm.appNameSelected == AppName.Pushagent)
                {

                    allServersForm.totalCountLabel.Visible = false;

                    string osType = allServersForm.osTypeComboBox.SelectedItem.ToString();

                    allServersForm.osTypeSelected = (OStype)Enum.Parse(typeof(OStype), osType);

                    if (allServersForm.appNameSelected != AppName.Pushagent)
                    {
                        if (allServersForm.esxHostIpText.Text.Trim().Length <= 0)
                        {
                            MessageBox.Show("Please enter vSphere server IP address", "Credential Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            return false;
                        }

                        //checking the length of the username textbox...
                        if (allServersForm.sourceUserNameText.Text.Trim().Length <= 0)
                        {
                            MessageBox.Show("Please enter username", "Credential Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            return false;
                        }
                        //checking the length of the passwor dtextbox....
                        if (allServersForm.sourcePassWordText.Text.Trim().Length <= 0)
                        {
                            MessageBox.Show("Please enter password ", "Credential Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            return false;
                        }
                    }
                    else
                    {
                        if (allServersForm.pushIPTextBox.Text.Trim().Length <= 0)
                        {
                            allServersForm.pushGetDeatsilsButton.Enabled = true;
                            MessageBox.Show("Please enter vSphere server IP address", "Credential Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            return false;
                        }

                        //checking the length of the username textbox...
                        if (allServersForm.pushUserNAmeTextBox.Text.Trim().Length <= 0)
                        {
                            allServersForm.pushGetDeatsilsButton.Enabled = true;
                            MessageBox.Show("Please enter username", "Credential Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            return false;
                        }
                        //checking the length of the passwor dtextbox....
                        if (allServersForm.pushPasswordTextBox.Text.Trim().Length <= 0)
                        {
                            allServersForm.pushGetDeatsilsButton.Enabled = true;
                            MessageBox.Show("Please enter password ", "Credential Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            return false;
                        }

                    }
                    //clearing the earlier prior selection...
                    if (allServersForm.selectedMasterListbyUser._hostList.Count > 0)
                    {
                        Host masterTargetHost = (Host)allServersForm.selectedMasterListbyUser._hostList[0];
                        foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                        {
                            if (h.targetDataStore != null)
                            {//reassingin the values of disk to the datastore....

                                foreach (DataStore d in masterTargetHost.dataStoreList)
                                {
                                    Trace.WriteLine(DateTime.Now + "\t Verfying the datastore " + h.targetDataStore + " \ttarget " + d.name);
                                    if (d.name == h.targetDataStore && d.vSpherehostname == masterTargetHost.vSpherehost)
                                    {
                                        h.targetDataStore = null;
                                        Trace.WriteLine(DateTime.Now + "\t Adding the datastore size ");
                                        d.freeSpace = d.freeSpace + h.totalSizeOfDisks;
                                        Trace.WriteLine(DateTime.Now + "\t Freesize after adding " + d.freeSpace.ToString());
                                    }
                                }
                            }
                        }
                    }
                    allServersForm.initialSourceListFromXml = new HostList();
                    allServersForm.selectedSourceListByUser = new HostList();

                    // checking that same esx ip is using again ....
                    // if already exixts we are not able to get the details again.....


                    allServersForm.addEsxButton.Enabled = false;
                    allServersForm.addSourceTreeView.Nodes.Clear();
                    allServersForm.esxDiskInfoDatagridView.Rows.Clear();
                    allServersForm.esxDiskInfoDatagridView.Visible = false;
                    allServersForm.addSourceTreeView.Visible = false;
                    allServersForm.addSourceRefreshLinkLabel.Visible = false;
                    allServersForm.addSourceDataGridViewHeadingLabel.Visible = false;
                    allServersForm.diskDetailsHeadingLabel.Visible = false;
                    allServersForm.vmDetailsPanelpanel.Visible = false;

                    if (allServersForm.initialSourceListFromXml._hostList.Count > 0)
                    {
                        foreach (Host h in allServersForm.initialSourceListFromXml._hostList)
                        {
                            if (h.esxIp == allServersForm.esxHostIpText.Text.Trim())
                            {
                                allServersForm.esxHostIpText.Clear();
                                MessageBox.Show("Primary vSphere server already exists in the list. ", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                return false;
                            }
                        }

                    }
                    if (allServersForm.appNameSelected != AppName.Pushagent)
                    {
                        _esxHostIP = allServersForm.esxHostIpText.Text.Trim();
                        _esxUserName = allServersForm.sourceUserNameText.Text.Trim();
                        _esxPassWord = allServersForm.sourcePassWordText.Text;
                    }
                    else
                    {

                        _esxHostIP = allServersForm.pushIPTextBox.Text.Trim();
                        _esxUserName = allServersForm.pushUserNAmeTextBox.Text.Trim();
                        _esxPassWord = allServersForm.pushPasswordTextBox.Text;

                    }

                    allServersForm.sourceEsxIPinput = _esxHostIP;

                    //Debug.WriteLine("Printing _esxIP ", _esxHostIP);
                    // allServersForm.sourceUserNameText.Clear();
                    // allServersForm.sourcePassWordText.Clear();
                }



                _esxInfo.GetHostList.Clear();

                ////UnComment following to remove shortcut

                //assigning the text box values entered by the user...


                allServersForm.progressBar.Visible = true;
                //allServersForm.Enabled                                      = false;   
                allServersForm.warnigLinkLabel.Visible = false;
                allServersForm.esxDetailsBackgroundWorker.RunWorkerAsync();
                allServersForm.loadingPictureBox.BringToFront();
                allServersForm.loadingPictureBox.Visible = true;
                Cursor.Current = Cursors.WaitCursor;
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
            Trace.WriteLine(DateTime.Now + " \t Exiting: AddSourcePanelHandler:GetSourceEsxDetails ");
            return true;
        }

        /*
         * This will called by esxDetailsBackgroundWorker.DoWork.
         * This will call the GetInfo.pl. To get the Info.xml.
         */
        internal bool GetDetailsBackGroundWorker(AllServersForm allServersForm)
         {
             Trace.WriteLine(DateTime.Now + " \t Entered: AddSourcePanelHandler:GetDetailsBackGroundWorker  " );
             if (allServersForm.appNameSelected == AppName.Pushagent)
             {
                 _esxHostIP = allServersForm.pushIPTextBox.Text.Trim();
                 _esxUserName = allServersForm.pushUserNAmeTextBox.Text.Trim();
                 _esxPassWord = allServersForm.pushPasswordTextBox.Text;
             }


             Cxapicalls cxApi = new Cxapicalls();
             if (cxApi. UpdaterCredentialsToCX(_esxHostIP, _esxUserName, _esxPassWord) == true)
             {
                 Trace.WriteLine(DateTime.Now + "\t updated credentials to CX ");
             }

             cxApi.GetHostCredentials(_esxHostIP);
             if (Cxapicalls.GetUsername != null && Cxapicalls.GetPassword != null)
             {
                 Trace.WriteLine(DateTime.Now + "\t Credentials are successfully updated to CX");
             }
           _getInfoReturnCode =   _esxInfo.GetEsxInfo(_esxHostIP, Role.Primary,allServersForm.osTypeSelected);
             if (allServersForm.appNameSelected == AppName.Failback)
             {

                 AdditionOfDiskSelectMachinePanelHandler.esxInfoAdddisk = _esxInfo;
             }
             return true;
         }

        /*
         * Once the GetInfo.pl scripts completed.  esxDetailsBackgroundWorker. Work completed will call this.
         * One the Info.xml is prepared by scripts. We will read that file and load into the tre view.
         * Based on the operation.
         * If Normal protection we need to show all vms.
         * If failback we need to show only selected vms.
         * If push agent we will load the datagridview only of pus agent panel.
         * There we will display only powered on vms.
         */
        internal bool ReloadTreeView(AllServersForm allServersForm)
         {
             try
             { /// checkin got the selection of user if it is push agent 
               /// 
                 allServersForm.loadingPictureBox.Visible = false;
                 if (allServersForm.appNameSelected != AppName.Pushagent)
                 {
                     Trace.WriteLine(DateTime.Now + " \t Entered: ReloadDataGridView method of the Esx_AddSourcePanelHandler  ");
                     int _rowIndex = 0;
                     int hostCount = 0;

                     allServersForm.addSourceTreeView.Nodes.Clear();
                     if (allServersForm.appNameSelected == AppName.Failback)
                     {

                         hostCount = AdditionOfDiskSelectMachinePanelHandler.esxInfoAdddisk.GetHostList.Count;
                     }
                     else
                     {
                         hostCount = _esxInfo.GetHostList.Count;
                     }
                     if (allServersForm.appNameSelected == AppName.Failback)
                     {
                         allServersForm.Update();
                         // Thread.Sleep(200);
                     }
                     foreach (Host h in _esxInfo.GetHostList)
                     {
                         h.new_displayname = h.displayname;
                         h.esxPassword = _esxPassWord;
                         h.esxUserName = _esxUserName;
                     }
                     if (hostCount > 0)
                     {
                         if (allServersForm.appNameSelected == AppName.Failback)
                         {
                             Host masterHost = new Host();
                             if (ResumeForm.masterTargetList._hostList.Count != 0)
                             {
                                  masterHost = (Host)ResumeForm.masterTargetList._hostList[0];
                             }
                             allServersForm.initialSourceListFromXml = new HostList();
                             allServersForm.selectedSourceListByUser = new HostList();
                             foreach (Host source in AdditionOfDiskSelectMachinePanelHandler.temperorySelectedSourceList._hostList)
                             {
                                 foreach (Host h in AdditionOfDiskSelectMachinePanelHandler.esxInfoAdddisk.GetHostList)
                                 {
                                     if (source.target_uuid != null)
                                     {
                                         if (masterHost != null)
                                         {
                                             if (source.target_uuid == h.source_uuid )
                                             {

                                                 h.target_uuid = source.source_uuid;
                                                 h.vmDirectoryPath = source.vmDirectoryPath;
                                                 h.old_vm_folder_path = source.folder_path;
                                                 h.resourcePool_src = source.resourcePool_src;
                                                 h.resourcepoolgrpname_src = source.resourcepoolgrpname_src;

                                                 foreach (Hashtable actualSourceHash in source.disks.list)
                                                 {
                                                     foreach (Hashtable currentSourceHash in h.disks.list)
                                                     {
                                                         if (actualSourceHash["target_disk_uuid"] != null && currentSourceHash["source_disk_uuid"] != null && actualSourceHash["source_disk_uuid"] != null)
                                                         {
                                                             if (actualSourceHash["target_disk_uuid"].ToString() == currentSourceHash["source_disk_uuid"].ToString())
                                                             {
                                                                 currentSourceHash["target_disk_uuid"] = actualSourceHash["source_disk_uuid"].ToString();
                                                             }
                                                         }
                                                     }
                                                 }


                                                allServersForm.initialSourceListFromXml.AddOrReplaceHost(h);
                                                 allServersForm.selectedSourceListByUser.AddOrReplaceHost(h);
												 break;
                                             }
                                             else
                                             {
                                                 if (h.poweredStatus == "ON")
                                                 {
                                                     if (source.targetvSphereHostName != null && source.targetvSphereHostName == h.vSpherehost)
                                                     {
                                                         if (VerifyingWithVmdks(allServersForm, h, source) == true)
                                                         {
                                                             h.vmDirectoryPath = source.vmDirectoryPath;
                                                             h.old_vm_folder_path = source.folder_path;
                                                             h.resourcePool_src = source.resourcePool_src;
                                                             h.resourcepoolgrpname_src = source.resourcepoolgrpname_src;
                                                             h.target_uuid = source.source_uuid;


                                                             foreach (Hashtable actualSourceHash in source.disks.list)
                                                             {
                                                                 foreach (Hashtable currentSourceHash in h.disks.list)
                                                                 {
                                                                     if (actualSourceHash["target_disk_uuid"] != null && currentSourceHash["source_disk_uuid"] != null && actualSourceHash["source_disk_uuid"] != null)
                                                                     {
                                                                         if (actualSourceHash["target_disk_uuid"].ToString() == currentSourceHash["source_disk_uuid"].ToString())
                                                                         {
                                                                             currentSourceHash["target_disk_uuid"] = actualSourceHash["source_disk_uuid"].ToString();
                                                                         }
                                                                     }
                                                                 }
                                                             }

                                                             allServersForm.initialSourceListFromXml.AddOrReplaceHost(h);
                                                 			 allServersForm.selectedSourceListByUser.AddOrReplaceHost(h);

                                                             break;
                                                         }
                                                     }
                                                 }
                                             }
                                         }
                                         else
                                         {
                                             if (source.target_uuid == h.source_uuid || h.displayname == source.new_displayname)
                                             {
                                                h.vmDirectoryPath = source.vmDirectoryPath;
                                                 h.target_uuid = source.source_uuid;
                                                 h.old_vm_folder_path = source.folder_path;
                                                 h.resourcePool_src = source.resourcePool_src;
                                                 h.resourcepoolgrpname_src = source.resourcepoolgrpname_src;

                                                 foreach (Hashtable actualSourceHash in source.disks.list)
                                                 {
                                                     foreach (Hashtable currentSourceHash in h.disks.list)
                                                     {
                                                         if (actualSourceHash["target_disk_uuid"] != null && currentSourceHash["source_disk_uuid"] != null && actualSourceHash["source_disk_uuid"] != null)
                                                         {
                                                             if (actualSourceHash["target_disk_uuid"].ToString() == currentSourceHash["source_disk_uuid"].ToString())
                                                             {
                                                                 currentSourceHash["target_disk_uuid"] = actualSourceHash["source_disk_uuid"].ToString();
                                                             }
                                                         }
                                                     }
                                                 }


                                                 allServersForm.initialSourceListFromXml.AddOrReplaceHost(h);
                                                 allServersForm.selectedSourceListByUser.AddOrReplaceHost(h);
                                                 break;
                                             }
                                             else
                                             {
                                                 if (h.poweredStatus == "ON")
                                                 {
                                                     if (VerifyingWithVmdks(allServersForm, h, source) == true)
                                                     {
                                                        h.vmDirectoryPath = source.vmDirectoryPath;
                                                        h.old_vm_folder_path = source.folder_path;
                                                         h.target_uuid = source.source_uuid;
                                                         h.resourcePool_src = source.resourcePool_src;
                                                         h.resourcepoolgrpname_src = source.resourcepoolgrpname_src;

                                                         foreach (Hashtable actualSourceHash in source.disks.list)
                                                         {
                                                             foreach (Hashtable currentSourceHash in h.disks.list)
                                                             {
                                                                 if (actualSourceHash["target_disk_uuid"] != null && currentSourceHash["source_disk_uuid"] != null && actualSourceHash["source_disk_uuid"] != null)
                                                                 {
                                                                     if (actualSourceHash["target_disk_uuid"].ToString() == currentSourceHash["source_disk_uuid"].ToString())
                                                                     {
                                                                         currentSourceHash["target_disk_uuid"] = actualSourceHash["source_disk_uuid"].ToString();
                                                                     }
                                                                 }
                                                             }
                                                         }


                                                         allServersForm.initialSourceListFromXml.AddOrReplaceHost(h);
                                                 		 allServersForm.selectedSourceListByUser.AddOrReplaceHost(h);
                                                         break;
                                                     }
                                                 }
                                             }
                                         }
                                     }
                                     else
                                     {
                                         if (source.new_displayname == h.displayname)
                                         {

                                             h.vmDirectoryPath = source.vmDirectoryPath;
                                             h.old_vm_folder_path = source.folder_path;
                                             h.target_uuid = source.source_uuid;
                                             h.resourcePool_src = source.resourcePool_src;
                                             h.resourcepoolgrpname_src = source.resourcepoolgrpname_src;

                                             foreach (Hashtable actualSourceHash in source.disks.list)
                                             {
                                                 foreach (Hashtable currentSourceHash in h.disks.list)
                                                 {
                                                     if (actualSourceHash["target_disk_uuid"] != null && currentSourceHash["source_disk_uuid"] != null && actualSourceHash["source_disk_uuid"] != null)
                                                     {
                                                         if (actualSourceHash["target_disk_uuid"].ToString() == currentSourceHash["source_disk_uuid"].ToString())
                                                         {
                                                             currentSourceHash["target_disk_uuid"] = actualSourceHash["source_disk_uuid"].ToString();
                                                         }
                                                     }
                                                 }
                                             }


                                             allServersForm.initialSourceListFromXml.AddOrReplaceHost(h);
                                             allServersForm.selectedSourceListByUser.AddOrReplaceHost(h);
                                             break;
                                         }
                                         else
                                         {
                                             if (h.poweredStatus == "ON")
                                             {
                                                 if (VerifyingWithVmdks(allServersForm, h, source) == true)
                                                 {
                                                     // h.target_uuid = source.source_uuid;
                                                    h.vmDirectoryPath = source.vmDirectoryPath;
                                                    h.old_vm_folder_path = source.folder_path;
                                                     h.target_uuid = source.source_uuid;
                                                     h.resourcePool_src = source.resourcePool_src;
                                                     h.resourcepoolgrpname_src = source.resourcepoolgrpname_src;

                                                     foreach (Hashtable actualSourceHash in source.disks.list)
                                                     {
                                                         foreach (Hashtable currentSourceHash in h.disks.list)
                                                         {
                                                             if (actualSourceHash["target_disk_uuid"] != null && currentSourceHash["source_disk_uuid"] != null && actualSourceHash["source_disk_uuid"] != null)
                                                             {
                                                                 if (actualSourceHash["target_disk_uuid"].ToString() == currentSourceHash["source_disk_uuid"].ToString())
                                                                 {
                                                                     currentSourceHash["target_disk_uuid"] = actualSourceHash["source_disk_uuid"].ToString();
                                                                 }
                                                             }
                                                         }
                                                     }

                                                     allServersForm.initialSourceListFromXml.AddOrReplaceHost(h);
                                                     allServersForm.selectedSourceListByUser.AddOrReplaceHost(h);
                                                     break;
                                                 }
                                             }
                                         }
                                     }
                                 }
                             }

                             LoadTreeView(allServersForm, allServersForm.selectedSourceListByUser);                            
                             if (allServersForm.appNameSelected == AppName.Failback)
                             {
                                 if (allServersForm.addSourceTreeView.Nodes.Count == 0)
                                 {
                                     MessageBox.Show("There are no VMs available for failback protection", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                     return false;
                                 }
                                 else
                                 {
                                     allServersForm.nextButton.Enabled = true;
                                 }
                             }

                             foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                             {
                                 if (h.cluster == "yes")
                                 {
                                     allServersForm.esxDiskInfoDatagridView.Enabled = true;
                                 }
                             }

                         }
                         else
                         {
                             Debug.WriteLine(DateTime.Now + " \t   `       Printing the count of the esx " + _esxInfo.GetHostList.Count);
                             //allServersForm
                             HostList list = new HostList();
                             foreach (Host h in _esxInfo.GetHostList)
                             {
                                 if(h.template == false)
                                 list.AddOrReplaceHost(h);
                             }
                             LoadTreeView(allServersForm, list);
                             allServersForm.addSourceDataGridViewHeadingLabel.Visible = true;
                             _skipData = null;
                             try
                             {
                                 if (allServersForm.appNameSelected != AppName.Failback)
                                 {
                                     if (File.Exists(WinTools.FxAgentPath() + "\\vContinuum\\logs\\vContinuum_ESX.log"))
                                     {
                                         StreamReader sr = new StreamReader(WinTools.FxAgentPath() + "\\vContinuum\\logs\\vContinuum_ESX.log");
                                         string skipData = sr.ReadToEnd();
                                         if (skipData.Length != 0)
                                         {
                                             _skipData = skipData;
                                             allServersForm.warnigLinkLabel.Visible = true;
                                             allServersForm.errorProvider.SetError(allServersForm.warnigLinkLabel, "Skipped data");
                                             //MessageBox.Show("Some of the data has been skipped form the vCenter/vSphere server." + Environment.NewLine +skipData, "Skipped Data for Server", MessageBoxButtons.OK, MessageBoxIcon.Information);
                                         }
                                         sr.Close();


                                     }
                                     //allServersForm.protectCheckBox.Visible = true;
                                 }
                             }
                             catch (Exception ex)
                             {
                                 Trace.WriteLine(DateTime.Now + "\t Failed to read vContinuum_ESx.log " + ex.Message);
                             }
                             allServersForm.addSourceRefreshLinkLabel.Visible = true;
                         }
                         
                         allServersForm.addSourceTreeView.ExpandAll();
                     }
                     else
                     {
                       
                         allServersForm.addSourceRefreshLinkLabel.Visible = false;
                         allServersForm.addSourceTreeView.Visible = false;
                         allServersForm.esxDiskInfoDatagridView.Rows.Clear();
                         allServersForm.esxDiskInfoDatagridView.Visible = false;
                         allServersForm.diskDetailsHeadingLabel.Visible = false;
                         //currentForm.errorProvider.SetError(currentForm.esxLoginControl, "please enter a valid data");
                         if (File.Exists(WinTools.FxAgentPath()+ "\\vContinuum\\Latest\\"+ SOURCEXML))
                         {
                             if (hostCount == 0)
                             {
                                 MessageBox.Show("There are no VMs to protect with this os type " + allServersForm.osTypeSelected, "vContinuum");
                                 allServersForm.addSourceRefreshLinkLabel.Visible = false;
                                 allServersForm.addEsxButton.Enabled = true;
                                 allServersForm.progressBar.Visible = false;
                                 allServersForm.addSourcePanel.Enabled = true;
                                 allServersForm.Enabled = true;
                                 return false;
                             }
                         }
                         else
                         {
                             if (AllServersForm.SuppressMsgBoxes == false)
                             {
                                 if (_getInfoReturnCode == 3)
                                 {
                                     MessageBox.Show("Please check that IP address and credentials are correct.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                 }
                                 else
                                 {
                                     MessageBox.Show("Could not retrieve guest info. Please check that IP address and credentials are correct.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                 }
                             }
                             allServersForm.addEsxButton.Enabled = true;
                             allServersForm.progressBar.Visible = false;
                             allServersForm.addSourceRefreshLinkLabel.Visible = false;
                             allServersForm.addSourcePanel.Enabled = true;
                             allServersForm.Enabled = true;
                             return false;
                         }
                     }
                     allServersForm.addSourcePanel.Enabled = true;
                     allServersForm.Enabled = true;
                     allServersForm.progressBar.Visible = false;
                     allServersForm.addEsxButton.Enabled = true;
                 }
                 else
                 {// If user selcts for push agent for esx it will call this method...
                     //checking for the sourcexml is exists or not...
                     Trace.WriteLine(DateTime.Now + "\t Came here to check file exists or not " + SOURCEXML);
                     if (File.Exists(WinTools.FxAgentPath()+ "\\vContinuum\\Latest\\"+SOURCEXML))
                     {
                         Trace.WriteLine(DateTime.Now + " \t File exists ");
                         int hostCount = 0;
                         //Reading the SourceXml file incase of the pushagents selection.........
                         _esxInfo.ReadEsxConfigFile(SOURCEXML, Role.Primary, _esxHostIP);


                         hostCount = _esxInfo.GetHostList.Count;


                         Trace.WriteLine(DateTime.Now + " \t printing count of hostlist " + _esxInfo.hostList.Count.ToString());
                         if (hostCount == 0)
                         {
                             if (AllServersForm.SuppressMsgBoxes == false)
                             {
                                 MessageBox.Show("There are no VMs to protect with this os type " + allServersForm.osTypeSelected, "vContinuum");
                             }
                             allServersForm.addSourceRefreshLinkLabel.Visible = false;
                             allServersForm.addEsxButton.Enabled = true;
                             allServersForm.progressBar.Visible = false;
                             allServersForm.addSourcePanel.Enabled = true;
                             allServersForm.Enabled = true;
                             allServersForm.pushGetDeatsilsButton.Enabled = true;
                             return false;
                         }
                         else
                         {
                             foreach (Host h in _esxInfo.hostList)
                             {//Validating the hosts with power on status, ip and hostname.....
                                 if (h.poweredStatus == "ON" && h.hostname != "NOT SET" && h.ip != "NOT SET")
                                 {
                                     h.esxUserName = _esxUserName;
                                     h.esxPassword = _esxPassWord;
                                     h.https = WinTools.Https;
                                     h.selectedDisk = true;
                                     allServersForm.selectedSourceListByUser.AddOrReplaceHost(h);
                                 }
                             }
                             int rowIndex = 0;
                             //filling the values in the pushagent screen.......pushagent datagridview.....
                             string cxip = WinTools.GetcxIP();
                             Cxapicalls cxApi = new Cxapicalls();
                             cxApi.PostToGetHostDetails( allServersForm.selectedSourceListByUser);
                             allServersForm.selectedSourceListByUser = Cxapicalls.GetHostlist;
                             //foreach (Host h in allServersForm._selectedSourceList._hostList)
                             //{
                             //    //Host h1 = (Host)h;
                             //    //WinPreReqs win = new WinPreReqs(h.ip, h.domain, h.userName, h.passWord);
                             //    //win.GetDetailsFromcxcli(ref h1, cxip);
                             //    if (h.fxagentInstalled == true && h.vxagentInstalled == true)
                             //    {
                             //        h.unifiedAgent = true;
                             //    }
                             //    else
                             //    {
                             //        h.unifiedAgent = false;
                             //    }
                             //}


                             foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                             {
                                 h.role = Role.Primary;
                                 // h.role = role.SOURCE;
                                 allServersForm.pushAgentDataGridView.Rows.Add(1);
                                 allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[PushAgentPanelHandler.IP_COLUMN].Value = h.ip;
                                 allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[PushAgentPanelHandler.DISPLAY_NAME_COLUMN].Value = h.displayname;
                                 if (h.vxagentInstalled == true && h.fxagentInstalled == true)
                                 {
                                     allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[PushAgentPanelHandler.AGENT_STATUS_COLUMN].Value = "Installed";

                                 }
                                 else if(h.vxagentInstalled == true && h.fxagentInstalled == false)
                                 {
                                     allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[PushAgentPanelHandler.AGENT_STATUS_COLUMN].Value = "VX Installed";
                                 }
                                 else if (h.vxagentInstalled == false && h.fxagentInstalled == true)
                                 {
                                     allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[PushAgentPanelHandler.AGENT_STATUS_COLUMN].Value = "FX Installed";
                                 }
                                 else if (h.vxagentInstalled == false && h.fxagentInstalled == false)
                                 {
                                     allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[PushAgentPanelHandler.AGENT_STATUS_COLUMN].Value = "Not Installed";
                                 }
                                 if (h.unifiedagentVersion == null)
                                 {
                                     allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[PushAgentPanelHandler.VERSION_COLUMN].Value = "N/A";
                                 }
                                 else
                                 {
                                     allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[PushAgentPanelHandler.VERSION_COLUMN].Value = h.unifiedagentVersion;
                                 }
                                 allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[PushAgentPanelHandler.ADVANCED_COLUMN].Value = "Settings";
                                 allServersForm.pushAgentDataGridView.Columns[PushAgentPanelHandler.PUSHAGENT_COLUMN].Visible = true;
                                 
                                 allServersForm.pushNoteLabel.Text = "Note: UA agent installation status is retried from CS server.( " + cxip + " )";
                                 allServersForm.pushNoteLabel.Visible = true;

                                 rowIndex++;
                             }
                             // allServersForm.addEsxButton.Enabled = true;
                             allServersForm.generalLogTextBox.Visible = true;
                             allServersForm.generalLogBoxClearLinkLabel.Visible = true;
                             allServersForm.pushAgentDataGridView.Visible = true;
                             allServersForm.pushAgentsButton.Visible = true;
                             allServersForm.pushUninstallButton.Visible = true;
                             allServersForm.progressBar.Visible = false;
                             allServersForm.pushAgentCheckBox.Visible = true;
                             allServersForm.pushGetDeatsilsButton.Enabled = true;
                             return true;
                         }
                     }
                     else
                     {
                         if (AllServersForm.SuppressMsgBoxes == false)
                         {
                             MessageBox.Show("Could not retrieve guest info from ESX server. Please check that IP address and credentials are correct.", "ESX credential error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                         }
                         allServersForm.addEsxButton.Enabled = true;
                         allServersForm.progressBar.Visible = false;
                         allServersForm.addSourceRefreshLinkLabel.Visible = false;
                         allServersForm.addSourcePanel.Enabled = true;
                         allServersForm.Enabled = true;
                         allServersForm.pushGetDeatsilsButton.Enabled = true;
                         return false;
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
             Trace.WriteLine(DateTime.Now + " \t Exiting: ReloadDataGridView method of the Esx_AddSourcePanelHandler  ");
             return true;
         }


        private bool LoadTreeView(AllServersForm allServersForm, HostList sourceList)
        {
            allServersForm.addSourceTreeView.Nodes.Clear();

            bool clusterExists = false;
            foreach (Host h in sourceList._hostList)
            {
                if (h.cluster == "yes")
                {
                    clusterExists = true;
                    break;
                }
            }

             _clutserList = new ArrayList();
            if (clusterExists == true)
            {
                Cxapicalls cxAPi = new Cxapicalls();
                cxAPi.PostToGetClusterInfo(ref _clutserList);

                if (_clutserList.Count != 0)
                {
                    foreach (Hashtable hash in _clutserList)
                    {
                        if (hash["clusterName"] != null)
                        {
                            string[] clusterhosts = hash["ClusterNodes"].ToString().Split(',');
                            string[] clusterUUIDs = hash["ClusterNodesInmageuuid"].ToString().Split(',');

                            int i = 0;
                            foreach (string s in clusterhosts)
                            {
                                Host h = new Host();
                                h.hostname = s;
                                h.cluster = "yes";
                                h.clusterName = hash["clusterName"].ToString();
                                h.clusterNodes = hash["ClusterNodes"].ToString();
                                h.clusterInmageUUIds = hash["ClusterNodesInmageuuid"].ToString();
                                h.inmage_hostid = clusterUUIDs[i];
                                foreach (Host sourceHost in sourceList._hostList)
                                {
                                    string hostname = sourceHost.hostname;
                                    string[] words = hostname.Split('.');
                                    string hostnameOriginal = words[0];
                                    int count = 0;
                                    hostname = words[0].ToUpper();
                                    if (hostname.Length > 15)
                                    {
                                        hostname = hostname.Substring(0, 15);
                                    }

                                    if (hostname.ToUpper() == s.ToUpper())
                                    {
                                        sourceHost.clusterName = hash["clusterName"].ToString();
                                        sourceHost.clusterNodes = hash["ClusterNodes"].ToString();
                                        sourceHost.clusterInmageUUIds = hash["ClusterNodesInmageuuid"].ToString();
                                        sourceHost.inmage_hostid = clusterUUIDs[i];
                                    }
                                }
                                i++;
                            }
                        }
                    }
                }
            }




            foreach (Host h in sourceList._hostList)
            {
                // h.datastore                                                                                 = null;
                if (h.template == false)
                {
                    if (h.cluster == "no" || h.clusterName == null)
                    {
                        ImageList list = new ImageList();
                        list.Images.Add(allServersForm.hostImage);
                        list.Images.Add(allServersForm.poweronImage);
                        list.Images.Add(allServersForm.powerOffImage);
                        allServersForm.addSourceTreeView.ImageList = list;
                        h.machineType = "VirtualMachine";
                        if (allServersForm.addSourceTreeView.Nodes.Count == 0)
                        {
                            allServersForm.addSourceTreeView.Nodes.Add(h.vSpherehost);
                            allServersForm.addSourceTreeView.Nodes[0].ImageIndex = 0;
                            allServersForm.addSourceTreeView.Nodes[0].Nodes.Add(h.displayname).Text = h.displayname;
                            allServersForm.addSourceTreeView.Refresh();
                            if (h.poweredStatus.ToUpper().ToString() == "OFF")
                            {
                                allServersForm.addSourceTreeView.Nodes[0].Nodes[0].ImageIndex = 2;
                                allServersForm.addSourceTreeView.Nodes[0].Nodes[0].SelectedImageIndex = 2;
                            }
                            else
                            {
                                allServersForm.addSourceTreeView.Nodes[0].Nodes[0].ImageIndex = 1;
                                allServersForm.addSourceTreeView.Nodes[0].Nodes[0].SelectedImageIndex = 1;
                            }
                            allServersForm.addSourceTreeView.SelectedNode = allServersForm.addSourceTreeView.Nodes[0].Nodes[0];
                        }
                        else
                        {
                            bool esxExists = false;
                            int nodeCount = 0;
                            foreach (TreeNode node in allServersForm.addSourceTreeView.Nodes)
                            {
                                nodeCount++;
                                // Trace.WriteLine(DateTime.Now + " \t printing the node names " + node.Name + " plan name " + h.plan);
                                if (node.Text == h.vSpherehost)
                                {
                                    esxExists = true;
                                    node.Nodes.Add(h.displayname);
                                    allServersForm.addSourceTreeView.Refresh();
                                    int count = allServersForm.addSourceTreeView.Nodes.Count;
                                    int childNodesCount = allServersForm.addSourceTreeView.Nodes[nodeCount - 1].Nodes.Count;
                                    if (h.poweredStatus.ToUpper().ToString() == "OFF")
                                    {
                                        allServersForm.addSourceTreeView.Nodes[nodeCount - 1].Nodes[childNodesCount - 1].SelectedImageIndex = 2;
                                        allServersForm.addSourceTreeView.Nodes[nodeCount - 1].Nodes[childNodesCount - 1].ImageIndex = 2;
                                    }
                                    else
                                    {
                                        allServersForm.addSourceTreeView.Nodes[nodeCount - 1].Nodes[childNodesCount - 1].SelectedImageIndex = 1;
                                        allServersForm.addSourceTreeView.Nodes[nodeCount - 1].Nodes[childNodesCount - 1].ImageIndex = 1;
                                    }
                                }
                            }
                            if (esxExists == false)
                            {
                                allServersForm.addSourceTreeView.Nodes.Add(h.vSpherehost);
                                int count = allServersForm.addSourceTreeView.Nodes.Count;
                                allServersForm.addSourceTreeView.Nodes[count - 1].ImageIndex = 0;

                                allServersForm.addSourceTreeView.Nodes[count - 1].Nodes.Add(h.displayname).Text = h.displayname;
                                allServersForm.addSourceTreeView.Refresh();
                                int childNodesCount = allServersForm.addSourceTreeView.Nodes[count - 1].Nodes.Count;
                                if (h.poweredStatus.ToUpper().ToString() == "OFF")
                                {
                                    allServersForm.addSourceTreeView.Nodes[count - 1].Nodes[childNodesCount - 1].ImageIndex = 2;
                                    allServersForm.addSourceTreeView.Nodes[count - 1].Nodes[childNodesCount - 1].SelectedImageIndex = 2;
                                }
                                else
                                {
                                    allServersForm.addSourceTreeView.Nodes[count - 1].Nodes[childNodesCount - 1].ImageIndex = 1;
                                    allServersForm.addSourceTreeView.Nodes[count - 1].Nodes[childNodesCount - 1].SelectedImageIndex = 1;
                                }
                            }
                        }

                        allServersForm.addSourceTreeView.Visible = true;
                        allServersForm.addSourceTreeView.Refresh();
                        allServersForm.addSourceRefreshLinkLabel.Visible = true;
                        if (allServersForm.appNameSelected != AppName.Failback)
                        {
                            allServersForm.addSourceTreeView.CheckBoxes = true;
                            allServersForm.initialSourceListFromXml.AddOrReplaceHost(h);
                            // _rowIndex++;
                        }
                        else
                        {
                            allServersForm.addSourceTreeView.CheckBoxes = false;
                        }
                    }
                }
            }


            foreach (Host h in sourceList._hostList)
            {
                if (h.cluster == "yes" && h.clusterName != null)
                {
                    ImageList list = new ImageList();
                    list.Images.Add(allServersForm.hostImage);
                    list.Images.Add(allServersForm.poweronImage);
                    list.Images.Add(allServersForm.powerOffImage);
                    allServersForm.addSourceTreeView.ImageList = list;
                    h.machineType = "VirtualMachine";
                    if (allServersForm.addSourceTreeView.Nodes.Count == 0)
                    {
                        allServersForm.addSourceTreeView.Nodes.Add("MSCS(" + h.clusterName + ")");
                        allServersForm.addSourceTreeView.Nodes[0].ImageIndex = 0;
                        allServersForm.addSourceTreeView.Nodes[0].Nodes.Add(h.displayname).Text = h.displayname;
                        allServersForm.addSourceTreeView.Refresh();

                        if (h.poweredStatus.ToUpper().ToString() == "OFF")
                        {
                            allServersForm.addSourceTreeView.Nodes[0].Nodes[0].ImageIndex = 2;
                            allServersForm.addSourceTreeView.Nodes[0].Nodes[0].SelectedImageIndex = 2;
                        }
                        else
                        {
                            allServersForm.addSourceTreeView.Nodes[0].Nodes[0].ImageIndex = 1;
                            allServersForm.addSourceTreeView.Nodes[0].Nodes[0].SelectedImageIndex = 1;
                        }
                        allServersForm.addSourceTreeView.SelectedNode = allServersForm.addSourceTreeView.Nodes[0].Nodes[0];
                        allServersForm.addSourceTreeView.Visible = true;
                    }
                    else
                    {
                        bool esxExists = false;
                        int nodeCount = 0;
                        foreach (TreeNode node in allServersForm.addSourceTreeView.Nodes)
                        {
                            nodeCount++;
                            // Trace.WriteLine(DateTime.Now + " \t printing the node names " + node.Name + " plan name " + h.plan);
                            if (node.Text == "MSCS(" + h.clusterName + ")")
                            {
                                esxExists = true;
                                node.Nodes.Add(h.displayname);
                                allServersForm.addSourceTreeView.Refresh();
                                int count = allServersForm.addSourceTreeView.Nodes.Count;
                                int childNodesCount = allServersForm.addSourceTreeView.Nodes[nodeCount - 1].Nodes.Count;
                                if (h.poweredStatus.ToUpper().ToString() == "OFF")
                                {
                                    allServersForm.addSourceTreeView.Nodes[nodeCount - 1].Nodes[childNodesCount - 1].SelectedImageIndex = 2;
                                    allServersForm.addSourceTreeView.Nodes[nodeCount - 1].Nodes[childNodesCount - 1].ImageIndex = 2;
                                }
                                else
                                {
                                    allServersForm.addSourceTreeView.Nodes[nodeCount - 1].Nodes[childNodesCount - 1].SelectedImageIndex = 1;
                                    allServersForm.addSourceTreeView.Nodes[nodeCount - 1].Nodes[childNodesCount - 1].ImageIndex = 1;
                                }
                            }
                        }
                        if (esxExists == false)
                        {
                            allServersForm.addSourceTreeView.Nodes.Add("MSCS(" + h.clusterName + ")");
                            int count = allServersForm.addSourceTreeView.Nodes.Count;
                            allServersForm.addSourceTreeView.Nodes[count - 1].ImageIndex = 0;

                            allServersForm.addSourceTreeView.Nodes[count - 1].Nodes.Add(h.displayname).Text = h.displayname;
                            allServersForm.addSourceTreeView.Refresh();
                            int childNodesCount = allServersForm.addSourceTreeView.Nodes[count - 1].Nodes.Count;
                            if (h.poweredStatus.ToUpper().ToString() == "OFF")
                            {
                                allServersForm.addSourceTreeView.Nodes[count - 1].Nodes[childNodesCount - 1].ImageIndex = 2;
                                allServersForm.addSourceTreeView.Nodes[count - 1].Nodes[childNodesCount - 1].SelectedImageIndex = 2;
                            }
                            else
                            {
                                allServersForm.addSourceTreeView.Nodes[count - 1].Nodes[childNodesCount - 1].ImageIndex = 1;
                                allServersForm.addSourceTreeView.Nodes[count - 1].Nodes[childNodesCount - 1].SelectedImageIndex = 1;
                            }
                        }
                    }
                }

                if (allServersForm.appNameSelected != AppName.Failback)
                {
                    allServersForm.addSourceTreeView.CheckBoxes = true;
                    allServersForm.initialSourceListFromXml.AddOrReplaceHost(h);
                    // _rowIndex++;
                }
                else
                {
                    allServersForm.addSourceTreeView.CheckBoxes = false;
                }
            }
            return true;
        }


        /*
         * In Case of Failback Protection we need to check whether slected vms are powered on and be there on target side.
         * So if any changes in displaynames or uuids didn't matched. we will check with vmdk names.
         * if vmdk are matched then we will assume that  machines are there on actual target.
         */
        internal bool VerifyingWithVmdks(AllServersForm allServersForm, Host h, Host source)
         {
             try
             {
                 ArrayList physicalDisks = new ArrayList();
                 ArrayList sourceDisks = new ArrayList();


                 ArrayList targetSideDiskDetails;
                 ArrayList actualProtection;
                 targetSideDiskDetails = h.disks.GetDiskList;
                 actualProtection = source.disks.GetDiskList;
                 foreach (Hashtable hash in targetSideDiskDetails)
                 {
                     string diskname = hash["Name"].ToString();
                     var pattern = @"\[(.*?)]";

                     var matches = Regex.Matches(diskname, pattern);
                     foreach (Match m in matches)
                     {
                         m.Result("");
                         diskname = diskname.Replace(m.ToString(), "");
                         string[] disksOntargetToCheckForFoldernameOption = diskname.Split('/');
                         // This is to check that user has selected the foldername option at th etime of protection.
                         // If the length of the disksOntargetToCheckForFoldernameOption is equal to three we will check with the last two 
                         if (disksOntargetToCheckForFoldernameOption.Length == 3)
                         {
                             Trace.WriteLine(DateTime.Now + "\t Found that at the time of protection user has selected the fodername option for the machine " + h.displayname);
                             diskname = disksOntargetToCheckForFoldernameOption[1] + "/" + disksOntargetToCheckForFoldernameOption[2];
                         }
                     }
                     foreach (Hashtable diskHash in actualProtection)
                     {
                         string targetDiskName = diskHash["Name"].ToString();
                         var matchesTarget = Regex.Matches(targetDiskName, pattern);
                         foreach (Match m in matchesTarget)
                         {
                             m.Result("");
                             targetDiskName = targetDiskName.Replace(m.ToString(), "");

                         }
                         if (diskname.TrimStart().TrimEnd().ToString() == targetDiskName.TrimStart().TrimEnd().ToString())
                         {
                             Trace.WriteLine(DateTime.Now + "\t Printing the Disknames after comparing successfull " + diskname + " " + targetDiskName);
                             Trace.WriteLine(DateTime.Now + "\t Printing the displaynames " + source.displayname + " \t target side " + h.displayname);
                             return true;
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
                 return false;
             }
             return false;
         }

        //credentials entered by the user are are assigning to the particular host....
        /*
         * This is used when we are ataking credential for each source vm.
         * Now we are not taking credentails at the time of first screen.
         * We had shifted that part to push agent screen.
         */
        internal bool GetCredentials(AllServersForm allServersForm, ref string inDomain, ref string inUserName, ref string inPassword)
         {
             try
             {
                 Trace.WriteLine(DateTime.Now + " \t Entered: GetCredentials of Esx_AddSourcePanelHandler   ");
                 addCredentialsForm = new AddCredentialsPopUpForm();
                 addCredentialsForm.domainText.Text = allServersForm.cachedDomain;
                 addCredentialsForm.userNameText.Text = allServersForm.cachedUsername;
                 addCredentialsForm.passWordText.Text = allServersForm.cachedPassword;
                 addCredentialsForm.credentialsHeadingLabel.Text = "Provide credentials " + _selectedHost.displayname;
                 addCredentialsForm.ShowDialog();
                 if (addCredentialsForm.canceled == true)
                 {
                     //  allServersForm.addSourcePanelEsxDataGridView.Rows[_credentialIndex].Cells[SOURCE_CHECKBOX_COLUMN].Value = false;
                     //  allServersForm.addSourcePanelEsxDataGridView.RefreshEdit();
                     Debug.WriteLine("GetCredentials: Returning false");
                     return false;
                 }
                 else
                 {
                     if (addCredentialsForm.useForAllCheckBox.Checked == true)
                     {
                         foreach (Host h in allServersForm.initialSourceListFromXml._hostList)
                         {
                             if (h.checkedCredentials == false)
                             {
                                 h.domain = addCredentialsForm.domain;
                                 h.userName = addCredentialsForm.userName;
                                 h.passWord = addCredentialsForm.passWord;
                             }
                         }
                     }
                     inDomain = addCredentialsForm.domain;
                     inUserName = addCredentialsForm.userName;
                     inPassword = addCredentialsForm.passWord;
                     allServersForm.cachedDomain = addCredentialsForm.domain;
                     allServersForm.cachedUsername = addCredentialsForm.userName;
                     allServersForm.cachedPassword = addCredentialsForm.passWord;
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
             Trace.WriteLine(DateTime.Now + " \t Exiting: GetCredentials of Esx_AddSourcePanelHandler  ");
             return true;
         }
        
        //when user select the soure machine to protect....
        /*
         * When particular machine is selected we will cal this method.
         * This will checks for the powered on status. If not we will power on the vm by the conformation from user.
         * Along with powered on wewill check for vm ware tools, ide disk,rdm disk, hostname. 
         * If anything went fails we will block user top select particular vm.
         */
        internal bool GetSelectedSourceInfo(AllServersForm allServersForm, string inDisplayName, int index, string vSphereHostName, TreeNode selectedNode)
         {
             try
             {
                 _credentialIndex = index;
                 /* foreach (Host h in allServersForm._initialSourceList._hostList)
                  {
                      Trace.WriteLine(DateTime.Now + "\t Printing display name " + h.displayname);
                  }*/
                 Trace.WriteLine(DateTime.Now + " \t Entered: GetSelectedSourceInfo of the Esx_AddSourcePanelHandler  ");
                 Trace.WriteLine(DateTime.Now + " \t Printing the index value and display name " + inDisplayName + " " + index.ToString());
                 // taking the display name to compare
                 if (inDisplayName.Length <= 0)
                 {
                     Trace.WriteLine(DateTime.Now + " \t P2vSelectSecondaryPanelHandler:MasterVmSelected: Display name is empty  ");
                     return false;
                 }
                 //To know the esx ip we added this logic
                 Host tempHost = new Host();
                 tempHost.displayname = inDisplayName;
                 tempHost.vSpherehost = vSphereHostName;
                 Trace.WriteLine(DateTime.Now + "         \t Printing tempHost display name " + tempHost.displayname);
                 int listIndex = 0;
                 if (allServersForm.initialSourceListFromXml.DoesHostExist(tempHost, ref listIndex) == true)
                 {
                     _selectedHost = (Host)allServersForm.initialSourceListFromXml._hostList[listIndex];
                 }
                 else
                 {
                     Trace.WriteLine(DateTime.Now + "\t Host does not exists in the list " + tempHost.displayname);
                     return false;
                 }
                 if (_selectedHost.poweredStatus == "ON")
                 {
                     if (_selectedHost.datacenter == null)
                     {
                         MessageBox.Show("Selected machine datacenter value is null. Unable to select this VM.","Error",MessageBoxButtons.OK,MessageBoxIcon.Error);
                         return false;
                     }
                     if (_selectedHost.hostname == "NOT SET")
                     {
                         MessageBox.Show("Selected VM's hostname is not set try again by using refresh option", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                         return false;
                     }
                     if (_selectedHost.ip == "NOT SET")
                     {
                         MessageBox.Show("Selected VM's IP is not set", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                         return false;
                     }
                     //Bug 14071 - Check for braces(in UI) in the VMX path names ONLY for 3.5 (
                     /* if (Esx.VERSION_OF_PRIMARY_VSPHERE_SERVER.Contains("3.5"))
                      {
                          string vmxPath = _selectedHost.vmx_path.Split(']')[1];
                          if ((_selectedHost.folder_path.Contains("(") || _selectedHost.folder_path.Contains(")") )||vmxPath.Contains("(") || vmxPath.Contains(")" ))
                          {
                              //MessageBox.Show(vmxPath);
                              MessageBox.Show("Invalid characters like braces ( or ) contained in vmx folder path","Error",MessageBoxButtons.OK,MessageBoxIcon.Error);
                              return false;
                          }
                      }*/
                     //this is for to see whether rdmp disk is there or not if it is there we are showing the below messagebox....

                     //if vmware tool are not installed user can't select the machine....
                     if (_selectedHost.vmwareTools.ToUpper().ToString() == "NotInstalled".ToUpper().ToString())
                     {
                         String message = _selectedHost.displayname + ": VM does not have VMware tools installed" +
                                           Environment.NewLine + "Please install VMware tools on the VM and use refresh to update data.";
                         MessageBox.Show(message, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                         return false;
                     }
                     else if (_selectedHost.vmwareTools.ToUpper().ToString() == "NotRunning".ToUpper().ToString())
                     {
                         MessageBox.Show("VMware tools are not running on this VM. Please make sure VMware tools are running on guest VM and then click on refresh option in vContinuum to update data.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                         return false;
                     }
                     //else if (_selectedHost.vmwareTools.ToUpper().ToString() == "OutOfDate".ToUpper().ToString())
                     //{
                     //    MessageBox.Show("VMware tools are out of date on this VM. Please update VMware tools on this VM: " + _selectedHost.displayname, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                     //    return false;
                     //}
                     if (_selectedHost.operatingSystem == null)
                     {
                         MessageBox.Show("Selected VM opereating system is null. Please check the VM info at vSphere/vCenter level", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                         return false;
                     }
                 }
                 //user can power on if he selects the powered off machine if he want....
                 if (_selectedHost.poweredStatus == "OFF")
                 {
                     _vmUUID = _selectedHost.source_uuid;
                     switch (MessageBox.Show("The " + _selectedHost.displayname + " is powered off. In order to use it as primary, we need to power on the VM. Do you want to power on the VM now ?", "Power on VM",
                                 MessageBoxButtons.YesNo,
                                 MessageBoxIcon.Question))
                     {
                         case DialogResult.Yes:
                             powerOn = true;
                             _esxInfo.GetHostList.Clear();
                             allServersForm.addSourceDataGridViewHeadingLabel.Visible = false;
                             allServersForm.addSourceRefreshLinkLabel.Visible = false;
                             allServersForm.diskDetailsHeadingLabel.Visible = false;
                             allServersForm.esxDiskInfoDatagridView.Visible = false;
                             allServersForm.nextButton.Enabled = false;
                             allServersForm.progressBar.Visible = true;
                             allServersForm.addSourcePanel.Enabled = false;

                             //allServersForm.addSourceTreeView.Visible = false;
                             allServersForm.vmDetailsPanelpanel.Visible = false;
                             Trace.WriteLine(DateTime.Now + " \t Prinitng the count of the list " + allServersForm.selectedSourceListByUser._hostList.Count.ToString());
                             allServersForm.esxDetailsBackgroundWorker.RunWorkerAsync();

                             return false;
                         case DialogResult.No:
                             return false;
                     }
                 }
                 else
                 {
                     if (_selectedHost.nicHash == null)
                     {
                         MessageBox.Show("Selected Vm's network information is disabled", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                         return false;
                     }
                     if (CheckIdeDisk(allServersForm, _selectedHost) == false)
                     {
                         Trace.WriteLine(DateTime.Now + "\t Ide Disk there in the source vm. So it is not supported");
                         return false;
                     }
                     if (_selectedHost.rdmpDisk == "TRUE")
                     {
                        // allServersForm.addSourceTreeView.SelectedNode.Parent.Text = vSphereHostName;
                         //allServersForm.addSourceTreeView.SelectedNode.Text = inDisplayName; 
                         
                         Trace.WriteLine(DateTime.Now + "\t RDM disk is present in VM " + _selectedHost.displayname);
                         switch (MessageBox.Show("This VM: " + _selectedHost.displayname + " has RDM disks. Do you want to convert RDM disks to vmdks?" + Environment.NewLine + Environment.NewLine + "Choose Yes to convert RDM disks to vmdk disks." + Environment.NewLine + "Choose No to keep them as RDM disks.", "", MessageBoxButtons.YesNo, MessageBoxIcon.Question))
                         {
                             case DialogResult.No:
                                 //allServersForm._convertRdmDiskToVmdk = "no";
                                 _selectedHost.convertRdmpDiskToVmdk = false;
                                 //Rdm to rdm support is not supporterd so here we are not allowing.
                                 if (allServersForm.appNameSelected == AppName.Offlinesync)
                                 {
                                     MessageBox.Show("RDM support at the time of offlinesync is not there", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                     return false;
                                 }
                                 break;
                             case DialogResult.Yes:
                              
                                 Debug.WriteLine(DateTime.Now + "\t Printing the Display name " + _selectedHost.displayname);
                                 _selectedHost.convertRdmpDiskToVmdk = true;
                                 break;
                         }
                     }
                     //In case of rdm disk when we show pop up it is going focus to anothe rnode so to fix the 
                     // Bug 20046 we have added this logic
                     if (inDisplayName != _selectedHost.displayname)
                     {
                         Trace.WriteLine(DateTime.Now + "\t we think this is the case where rdm disk is present in the source");
                         _selectedHost = (Host)allServersForm.initialSourceListFromXml._hostList[listIndex];
                     }
                     Host tempHost1 = new Host();
                     tempHost1.hostname = _selectedHost.hostname;
                     if (CheckinVMinCX(allServersForm, ref tempHost1) == true)
                     {
                         foreach (Hashtable hash in _selectedHost.nicList.list)
                         {
                             foreach (Hashtable tempHash in tempHost1.nicList.list)
                             {
                                 if (hash["nic_mac"] != null && tempHash["nic_mac"] != null)
                                 {
                                     if (hash["nic_mac"].ToString().ToUpper() == tempHash["nic_mac"].ToString().ToUpper())
                                     {
                                         tempHash["network_name"] = hash["network_name"];
                                         tempHash["adapter_type"] = hash["adapter_type"];
                                         break;
                                     }
                                 }
                             }
                                
                         }
                         _selectedHost.nicList.list = tempHost1.nicList.list;
                         allServersForm.selectedSourceListByUser.AddOrReplaceHost(_selectedHost);
                     }
                     else
                     {
                         Trace.WriteLine(DateTime.Now + "\t Failed to find vm in  cx" + _selectedHost.displayname);
                         return false;
                     }
                     //foreach (Hashtable hash in _selectedHost.nicList)
                     //{

                     //}
                     _selectedHost.resourcePool_src = _selectedHost.resourcePool;
                     _selectedHost.resourcepoolgrpname_src = _selectedHost.resourcepoolgrpname;
                     allServersForm.selectedSourceListByUser.AddOrReplaceHost(_selectedHost);
                     Trace.WriteLine(DateTime.Now + "\t Priniting the sourcelist count " + allServersForm.selectedSourceListByUser._hostList.Count.ToString());
                     allServersForm.totalCountLabel.Text = "Total number of vms selected for protection is " + allServersForm.selectedSourceListByUser._hostList.Count.ToString();
                     allServersForm.totalCountLabel.Visible = true;
                     EsxDiskDetails(allServersForm, _selectedHost);
                     FillGeneralTableWithHostDetails(allServersForm, _selectedHost);
                     allServersForm.nextButton.Enabled = true;
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
             Trace.WriteLine(DateTime.Now + " \t Exiting: GetSelectedSourceInfo of the Esx_AddSourcePanelHandler  ");
             return true;
         }


        internal bool CheckReturnCodeOfBackGroundWorker(AllServersForm allServersForm)
         {
             allServersForm.addSourcePanel.Enabled = true;
             if (_vmExistsinCXDataBase == true)
             {
                 _selectedHost.resourcePool_src = _selectedHost.resourcePool;
                 _selectedHost.resourcepoolgrpname_src = _selectedHost.resourcepoolgrpname;
                 allServersForm.selectedSourceListByUser.AddOrReplaceHost(_selectedHost);
                 Trace.WriteLine(DateTime.Now + "\t Priniting the sourcelist count " + allServersForm.selectedSourceListByUser._hostList.Count.ToString());
                 allServersForm.totalCountLabel.Text = "Total number of vms selected for protection is " + allServersForm.selectedSourceListByUser._hostList.Count.ToString();
                 allServersForm.totalCountLabel.Visible = true;
                 EsxDiskDetails(allServersForm, _selectedHost);
                 FillGeneralTableWithHostDetails(allServersForm, _selectedHost);
                 allServersForm.nextButton.Enabled = true;
             }
             else
             {
                 _currentNode.Checked = false;
             }
             allServersForm.progressBar.Visible = false;
             return true;
         }

         /*
          * GetSelectedSourceInfowill call this method.
          * To check whether any ide disks are present in the source vm.
          * Ide support is not there so we have this error check.
          */
        internal bool CheckIdeDisk(AllServersForm allServersForm, Host h)
         {
             try
             {
                 Trace.WriteLine(DateTime.Now + " \t Entered: CheckIdeDisk of the Esx_AddSourcePanelHandler  ");
                 ArrayList physicalDisks = new ArrayList();
                 physicalDisks = h.disks.GetDiskList;
                 foreach (Hashtable disk in physicalDisks)
                 {
                     Trace.WriteLine(DateTime.Now + " \t Printing the ide_scsi value " + disk["ide_or_scsi"].ToString());
                     if (disk["ide_or_scsi"].ToString() == "ide")
                     {
                         MessageBox.Show("VM : " + h.displayname + " having IDE disks.VM with IDE disks  is not supported at this time.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                         return false;
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
             Trace.WriteLine(DateTime.Now + " \t Exiting: CheckIdeDisk of the Esx_AddSourcePanelHandler  ");
             return true;
         }

        /*
         * We are not using this code.
         * This is in 5.5 GA which is first release of vcon.
         */
        internal bool PostCredentialsCheck(AllServersForm allServersForm)
         {
             Trace.WriteLine(DateTime.Now + " \t Entered: PostCredentialsCheck of the Esx_AddSourcePanelHandler  ");
             try
             {
                 if (_wmiErrorCheckCanceled == false)
                 {
                     if (_credentialsCheckPassed == true)
                     {
                         //Display disk details in the disk grid
                         if (allServersForm.selectedSourceListByUser._hostList.Count > 0)
                         {
                             allServersForm.BringToFront();
                             EsxDiskDetails(allServersForm, _selectedHost);


                         }
                     }
                     else
                     {
                         //Didn't pass the credentials check. Take credentials again from user.


                         // if (GetCredentials(allServersForm, ref _selectedHost.domain, ref _selectedHost.userName, ref _selectedHost.passWord) == true)
                         {
                             allServersForm.BringToFront();
                             allServersForm.nextButton.Enabled = false;
                             allServersForm.addSourcePanel.Enabled = false;
                             // allServersForm.Enabled = false;
                             allServersForm.progressBar.Visible = true;
                             allServersForm.checkCredentialsBackgroundWorker.RunWorkerAsync();
                         }
                     }
                 }
                 else
                 {
                     allServersForm.BringToFront();

                   //  allServersForm.addSourcePanelEsxDataGridView.Rows[_credentialIndex].Cells[SOURCE_CHECKBOX_COLUMN].Value = false;
                    // allServersForm.addSourcePanelEsxDataGridView.RefreshEdit();

                 }

                 if (_credentialsCheckPassed == false)
                 {

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

             Trace.WriteLine(DateTime.Now + " \t Exiting: PostCredentialsCheck of the Esx_AddSourcePanelHandler  ");

             return true; 
         }

        
        /*
         * If any selected vm is in powered off state we will ask user whether he power to power in throug Wizard or not 
         * If not user can't seelct the vm.
         * If user wants to power on the vm we will power on the vm.
         * we will call this method to power on the vm
         * this was called by the backgroundworker to power on the vm.
         */
        internal bool PowerOnVm(AllServersForm allServersForm)
         {
             try
             {
                 Trace.WriteLine(DateTime.Now + " \t Entered: PowerOnVm of the Esx_AddSourcePanelHandler  ");

                 _powerOnReturnCode = _esxInfo.PowerOnVM(_esxHostIP, "\"" + _vmUUID + "\"");
                 Thread.Sleep(30000);

                 if (_esxInfo.GetEsxInfo(_esxHostIP,  Role.Primary, allServersForm.osTypeSelected) == 1)
                 {
                     Trace.WriteLine(DateTime.Now + " \t Failed to get Source.XML file from  " + _esxHostIP );
                     Trace.WriteLine(DateTime.Now + " \t Retrying after waiting for 20 seconds... " );
                     Thread.Sleep(20000);
                     _esxInfo.GetEsxInfo(_esxHostIP,  Role.Primary, allServersForm.osTypeSelected);
                 }
                 else
                 {
                     foreach (Host h in _esxInfo.GetHostList)
                     {
                         if (h.displayname == _vmUUID)
                         {
                             if (h.nicHash.Count == 0)
                             {
                                 Trace.WriteLine(DateTime.Now + " \t Failed to get Network information for " + _vmUUID);
                                 Trace.WriteLine(DateTime.Now + " \t Retrying after waiting for 20 seconds... ");
                                 Thread.Sleep(20000);
                                 _esxInfo.GetEsxInfo(_esxHostIP, Role.Primary, allServersForm.osTypeSelected);
                             }
                         }
                     }

                 }


                 Trace.WriteLine(DateTime.Now + " \t Exiting: PowerOnVm of the Esx_AddSourcePanelHandler  ");
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

        /*
         * once after power on the vm. we will load the treeview with selected vms already.
         * the machines which are selected early by the user will be selected automatically in this method.
         * using same backgroundworke rfor both power on ang getinfo.pl.
         */
        internal bool ReloadAfterPowerOn(AllServersForm allServersForm)
         {
             Trace.WriteLine(DateTime.Now + " \t Entered: ReloadAfterPowerOn of the Esx_AddSourcePanelHandler  ");
             try
             {
                 
                 allServersForm.addSourceTreeView.Nodes.Clear();

                 allServersForm.initialSourceListFromXml = new HostList();
                 foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                 {
                     _powerOnList.AddOrReplaceHost(h);
                 }
                 int hostCount = _esxInfo.GetHostList.Count;
                 int rowIndex = 0;
                 if (hostCount > 0)
                 {

                     foreach (Host h in _esxInfo.GetHostList)
                     {
                         // h.datastore                                                                                 = null;

                         h.machineType = "VirtualMachine";
                         if (allServersForm.addSourceTreeView.Nodes.Count == 0)
                         {
                             allServersForm.addSourceTreeView.Nodes.Add(h.vSpherehost);
                             allServersForm.addSourceTreeView.Nodes[0].ImageIndex = 0;
                             allServersForm.addSourceTreeView.Nodes[0].Nodes.Add(h.displayname).Text = h.displayname;
                             if (h.poweredStatus.ToUpper().ToString() == "OFF".ToUpper().ToString())
                             {
                                 allServersForm.addSourceTreeView.Nodes[0].Nodes[0].ImageIndex = 2;
                                 allServersForm.addSourceTreeView.Nodes[0].Nodes[0].SelectedImageIndex = 2;
                             }
                             else
                             {
                                 allServersForm.addSourceTreeView.Nodes[0].Nodes[0].ImageIndex = 1;
                                 allServersForm.addSourceTreeView.Nodes[0].Nodes[0].SelectedImageIndex = 1;

                             }
                             allServersForm.addSourceTreeView.SelectedNode = allServersForm.addSourceTreeView.Nodes[0].Nodes[0];
                             allServersForm.addSourceRefreshLinkLabel.Visible = true;
                             allServersForm.addSourceTreeView.Visible = true;

                         }
                         else
                         {
                             bool esxExists = false;
                             foreach (TreeNode node in allServersForm.addSourceTreeView.Nodes)
                             {
                                 // Trace.WriteLine(DateTime.Now + " \t printing the node names " + node.Name + " plan name " + h.plan);
                                 if (node.Text == h.vSpherehost)
                                 {
                                     esxExists = true;
                                     node.Nodes.Add(h.displayname);
                                     int count = allServersForm.addSourceTreeView.Nodes.Count;
                                     int childNodesCount = allServersForm.addSourceTreeView.Nodes[count - 1].Nodes.Count;
                                     if (h.poweredStatus.ToUpper().ToString() == "OFF".ToUpper().ToString())
                                     {
                                         allServersForm.addSourceTreeView.Nodes[count - 1].Nodes[childNodesCount - 1].SelectedImageIndex = 2;
                                         allServersForm.addSourceTreeView.Nodes[count - 1].Nodes[childNodesCount - 1].ImageIndex = 2;
                                     }
                                     else
                                     {
                                         allServersForm.addSourceTreeView.Nodes[count - 1].Nodes[childNodesCount - 1].SelectedImageIndex = 1;
                                         allServersForm.addSourceTreeView.Nodes[count - 1].Nodes[childNodesCount - 1].ImageIndex = 1;
                                     }
                                 }
                             }
                             if (esxExists == false)
                             {
                                 allServersForm.addSourceTreeView.Nodes.Add(h.vSpherehost);
                                 int count = allServersForm.addSourceTreeView.Nodes.Count;
                                 allServersForm.addSourceTreeView.Nodes[count - 1].ImageIndex = 0;

                                 allServersForm.addSourceTreeView.Nodes[count - 1].Nodes.Add(h.displayname).Text = h.displayname;
                                 int childNodesCount = allServersForm.addSourceTreeView.Nodes[count - 1].Nodes.Count;
                                 if (h.poweredStatus.ToUpper().ToString() == "OFF".ToUpper().ToString())
                                 {
                                     allServersForm.addSourceTreeView.Nodes[count - 1].Nodes[childNodesCount - 1].ImageIndex = 2;
                                     allServersForm.addSourceTreeView.Nodes[count - 1].Nodes[childNodesCount - 1].SelectedImageIndex = 2;
                                 }
                                 else
                                 {
                                     allServersForm.addSourceTreeView.Nodes[count - 1].Nodes[childNodesCount - 1].ImageIndex = 1;
                                     allServersForm.addSourceTreeView.Nodes[count - 1].Nodes[childNodesCount - 1].SelectedImageIndex = 1;
                                 }
                             }
                         }
                         allServersForm.addSourceTreeView.CheckBoxes = true;
                        /* for (int i = 0; i < allServersForm.addSourceTreeView.Nodes.Count; i++)
                         {
                             allServersForm.addSourceTreeView.Nodes[i].Checked = false;
                             for (int j = 0; j < allServersForm.addSourceTreeView.Nodes[i].Nodes.Count; j++)
                             {
                                 allServersForm.addSourceTreeView.Nodes[i].Nodes[j].Checked = false;
                             }
                         }*/

                         //allServersForm.addSourceTreeView.Nodes[_rowIndex].Nodes.Add(h.displayname).Text = h.displayname;
                         //allServersForm.addSourceTreeView.Nodes[_rowIndex].Nodes[_rowIndex].Nodes.Add(h.ip).Text = h.ip;

                         //allServersForm.addSourceTreeView.Nodes[_rowIndex].Nodes[_rowIndex].Nodes.Add(h.vmwareTools).Text = h.vmwareTools;

                         allServersForm.addSourceTreeView.Visible = true;

                         allServersForm.initialSourceListFromXml.AddOrReplaceHost(h);

                     }
                     Trace.WriteLine(DateTime.Now + " \t Reloading the vms into the data grid view ");

                     foreach (Host h1 in _esxInfo.GetHostList)
                     {



                         //allServersForm.addSourcePanelEsxDataGridView.Rows[rowIndex].Cells[ESX_IP_COLUMN].Value               = h1.esxIp;
                         h1.plan = allServersForm.planInput;
                         h1.machineType = "VirtualMachine";

                        
                         // allServersForm.addSourcePanelEsxDataGridView.Rows[rowIndex].Cells[ESX_IP_COLUMN].Value               = h1.esxIp;


                         if (allServersForm.initialSourceListFromXml.AddOrReplaceHost(h1) == true)
                         {
                         }
                         else
                         {
                             Trace.WriteLine(DateTime.Now + "\t Failed to add the machine to list " + h1.displayname);
                         }
                         Trace.WriteLine(DateTime.Now + " Priting the count of prevous selected list " + _powerOnList._hostList.Count.ToString());
                         foreach (Host h in _powerOnList._hostList)
                         {
                             if (h.displayname == h1.displayname && h.hostname == h1.hostname)
                             {
                                 allServersForm.nextButton.Enabled = true;
                                 
                                     for (int i = 0; i < allServersForm.addSourceTreeView.Nodes.Count; i++)
                                     {
                                         if (h.displayname == allServersForm.addSourceTreeView.Nodes[i].Text)
                                         {
                                            allServersForm.addSourceTreeView.Nodes[i].Checked = true;
                                             
                                         }
                                         for (int j = 0; j < allServersForm.addSourceTreeView.Nodes[i].Nodes.Count; j++)
                                         {
                                             Trace.WriteLine(DateTime.Now + " \t Came here to check the nodes " + h.displayname + "Node " + allServersForm.addSourceTreeView.Nodes[i].Nodes[j].Text);
                                             if (h.displayname == allServersForm.addSourceTreeView.Nodes[i].Nodes[j].Text)
                                             {
                                                 allServersForm.addSourceTreeView.Nodes[i].Nodes[j].Checked = true;
                                                
                                             }
                                         }
                                     
                                 }
                                 //allServersForm.addSourcePanelEsxDataGridView.Rows[rowIndex].Cells[SOURCE_CHECKBOX_COLUMN].Value = true;
                             }
                         }
                         rowIndex++;
                     }
                 }
                 allServersForm.Enabled = true;
                 allServersForm.addSourcePanel.Enabled = true;
                 //recently it is added for the get details button enabled issue.
                 allServersForm.addEsxButton.Enabled = true;
                 allServersForm.addSourceDataGridViewHeadingLabel.Visible = true;
                 allServersForm.addSourceTreeView.Visible = true;                 
                 allServersForm.addSourceRefreshLinkLabel.Visible = true;             
                 allServersForm.progressBar.Visible = false;
                 if (_powerOnReturnCode != 0)
                 {
                     MessageBox.Show("Unable to power on the VM", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                 }
                 allServersForm.addSourceTreeView.ExpandAll();
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
             Trace.WriteLine(DateTime.Now + " \t Exiting: ReloadAfterPowerOn of the Esx_AddSourcePanelHandler  ");
             return true;
         }

        /*
         * This method will display the disks of selected vm.
         * If user unselcts disks. that will be shown unslect while user selects to see the vm details.
         */


        internal bool EsxDiskDetails(AllServersForm allServersForm, Host h)
         {
             try
             {
                 
                 if (allServersForm.appNameSelected != AppName.Pushagent)
                 {
                     // for the displaying the disk details.....
                     Trace.WriteLine(DateTime.Now + " \t Entered: EsxDiskDetails of the Esx_AddSourcePanelHandler  ");
                     //if (allServersForm._osType == OStype.Windows)
                     {
                         allServersForm.currentDisplayedPhysicalHostIPselected = h.displayname;
                         ArrayList physicalDisks;
                         int i = 0;
                         physicalDisks = h.disks.GetDiskList;
                         allServersForm.esxDiskInfoDatagridView.Rows.Clear();
                         allServersForm.esxDiskInfoDatagridView.RowCount = h.disks.GetDiskList.Count;
                         foreach (Hashtable disk in physicalDisks)
                         {
                             allServersForm.esxDiskInfoDatagridView.Rows[i].Cells[0].Value = "Disk" + i.ToString();
                             Trace.WriteLine(DateTime.Now + "\t Printing the seelcted machine diskcount " + h.disks.GetDiskList.Count.ToString() + "\t name " + h.displayname);
                             if (disk["Selected"].ToString() == "Yes")
                             {
                                 allServersForm.esxDiskInfoDatagridView.Rows[i].Cells[2].Value = true;
                             }
                             else
                             {
                                 allServersForm.esxDiskInfoDatagridView.Rows[i].Cells[2].Value = false;
                             }


                             if (allServersForm.osTypeSelected == OStype.Windows && h.cluster == "no")
                             {
                                 if (h.operatingSystem.Contains("7") || h.operatingSystem.Contains("2003" ))
                                 {

                                 }
                                 else
                                 {
                                     if (disk["volumelist"] == null && allServersForm.appNameSelected != AppName.Failback)
                                     {
                                         allServersForm.esxDiskInfoDatagridView.Rows[i].Cells[2].Value = false;
                                         allServersForm.esxDiskInfoDatagridView.Rows[i].Cells[2].ReadOnly = true;
                                         disk["Selected"] = "No";
                                     }
                                 }
                             }
                             if (disk["IsBootable"] != null)
                             {
                                 Trace.WriteLine(DateTime.Now + "\t Found boot disk value");
                                 if (disk["IsBootable"].ToString().ToUpper() == "TRUE")
                                 {
                                     Trace.WriteLine(DateTime.Now + "\t Made boot disk as not selectable");
                                     allServersForm.esxDiskInfoDatagridView.Rows[i].Cells[2].ReadOnly = true;
                                 }
                                 else
                                 {
                                     allServersForm.esxDiskInfoDatagridView.Rows[i].Cells[2].ReadOnly = false;
                                 }
                             }
                         
                             allServersForm.diskDetailsHeadingLabel.Visible = true;
                             //Adding when the click event done in the primaryDataGridView ip column 
                             // disk["Selected"]        = "Yes";
                             if (disk["Size"] != null)
                             {
                                 float size = float.Parse(disk["Size"].ToString());

                                 size = size / (1024 * 1024);
                                 //size                     = int.Parse(String.Format("{0:##.##}", size));
                                 size = float.Parse(String.Format("{0:00000.000}", size));
                                 allServersForm.esxDiskInfoDatagridView.Rows[i].Cells[1].Value = size;
                             }
                             allServersForm.esxDiskInfoDatagridView.Rows[i].Cells[3].Value = "Details";
                             i++;
                             Debug.WriteLine(DateTime.Now + "\t Physical Disk name = " + disk["Name"] + " Size: = " + disk["Size"] + " Selected: = " + disk["Selected"]);
                         }
                         allServersForm.esxDiskInfoDatagridView.Visible = true;
                         allServersForm.diskDetailsHeadingLabel.Text = "Disk details of " + h.displayname;
                         allServersForm.diskDetailsHeadingLabel.Visible = true;
                         Debug.WriteLine(DateTime.Now + "\t______________________________________    before dispalydriverdetails printing _selected SourceList");
                         //allServersForm._selectedSourceList.Print();
                     }
                     Trace.WriteLine(DateTime.Now + " \t Exiting: EsxDiskDetails of the Esx_AddSourcePanelHandler  ");
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

        /*
         * For Windows protection we willhave the disk selecion.
         * User can unselect the disks which he doesn't want to protect. 
         * 
         */



        internal bool DislpayDiskDetails(AllServersForm allServersForm, int index)
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
                     disk.descriptionLabel.Text = "Details of: " + allServersForm.esxDiskInfoDatagridView.Rows[index].Cells[0].Value.ToString();
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
                     if (hash["source_disk_uuid"] != null)
                     {
                         disk.detailsDataGridView.Rows[1].Cells[1].Value = hash["source_disk_uuid"].ToString();
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
                     if (hash["disk_type_os"] != null)
                     {
                         if (hash["efi"] != null)
                         {
                             disk.detailsDataGridView.Rows[4].Cells[1].Value = hash["disk_type_os"].ToString() + ", Layout ->" + hash["DiskLayout"].ToString() + ", EFI -> " + hash["efi"].ToString();
                         }
                         else
                         {
                             disk.detailsDataGridView.Rows[4].Cells[1].Value = hash["disk_type"].ToString() + ", Layout -> " + hash["DiskLayout"].ToString();
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
             Trace.WriteLine(DateTime.Now + " \t Entered: SelectDiskOrUnSelectDisk of the Esx_AddSourcePanelHandler  ");
             try
             {
                 if ((bool)allServersForm.esxDiskInfoDatagridView.Rows[index].Cells[2].FormattedValue)
                 {
                     Host tempHost = new Host();
                     tempHost.displayname = allServersForm.currentDisplayedPhysicalHostIPselected;
                     int listIndex = 0;
                     if (allServersForm.selectedSourceListByUser.DoesHostExist(tempHost, ref listIndex) == true)
                     {
                         Debug.WriteLine("Host exists in the primary host list ");

                         ((Host)allServersForm.selectedSourceListByUser._hostList[listIndex]).SelectDisk(index);

                         Host selectedHost = (Host)allServersForm.selectedSourceListByUser._hostList[listIndex];

                         if (selectedHost.cluster == "yes" && !selectedHost.operatingSystem.Contains("2003"))
                         {

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
                    // Host selectedHost = (Host)allServersForm.selectedSourceListByUser._hostList[listIndex];
                     
                 }
                 else
                 {

                     Host tempHost = new Host();
                     tempHost.displayname = allServersForm.currentDisplayedPhysicalHostIPselected;
                     int listIndex = 0;
                     if (allServersForm.selectedSourceListByUser.DoesHostExist(tempHost, ref listIndex) == true)
                     {
                         Debug.WriteLine("Host exists in the primary host list ");


                         ((Host)allServersForm.selectedSourceListByUser._hostList[listIndex]).UnselectDisk(index);

                         Host selectedHost = (Host)allServersForm.selectedSourceListByUser._hostList[listIndex];
                         if (selectedHost.cluster == "yes" && !selectedHost.operatingSystem.Contains("2003"))
                         {

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
             Trace.WriteLine(DateTime.Now + " \t Exiting: SelectDiskOrUnSelectDisk of the Esx_AddSourcePanelHandler  ");

             return true;
         }

        /*
         * In this method we will check whether slected host is there in initiallist which is nothing but Info.xml list.
         * As of now we are not using this method.
         */ 
         private bool SourceEsxExists(AllServersForm allServersForm)
         {

             Trace.WriteLine(DateTime.Now + " \t Entered: SourceEsxExists of the Esx_AddSourcePanelHandler  ");

             try
             {
                 // this logic is for when user enters same esx server two times we are not giving option for him to enter....

                 if (allServersForm.initialSourceListFromXml._hostList.Count != 0)
                 {
                     foreach (Host h in allServersForm.initialSourceListFromXml._hostList)
                     {
                         if (h.esxIp == allServersForm.esxHostIpText.Text)
                         {
                             return false;
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
             Trace.WriteLine(DateTime.Now + " \t Exiting: SourceEsxExists of the Esx_AddSourcePanelHandler  ");

             return true;
         }

        /*
         * By clicking the Refresh link label in the first screen of protection.
         * We will get Info.xml with the credentails given by user first time.
         * We will use the existing credentials.
         * In case of failbackl we will get credentails for the MasterConfigFile.xml.
         * If that credentails are wrong we will ask user to give new credentails. That too in main screen only 
         * There we are actually getting the credentails.
         * In this case we won't fail with credential issue with Refresh link lable click.
         */
         internal bool RefreshLinkLabelClickEvent(AllServersForm allServersForm)
         {
             try
             {
                 if (_esxHostIP != null && _esxUserName != null && _esxPassWord != null)
                 {

                                   
                     allServersForm.totalCountLabel.Visible = false;
                     allServersForm.vmDetailsPanelpanel.Visible = false;
                     allServersForm.addEsxButton.Enabled = false;
                     allServersForm.addSourceTreeView.Nodes.Clear();
                     allServersForm.esxDiskInfoDatagridView.Rows.Clear();
                     allServersForm.esxDiskInfoDatagridView.Visible = false;
                     allServersForm.addSourceTreeView.Visible = false;
                     allServersForm.addSourceDataGridViewHeadingLabel.Visible = false;
                     allServersForm.diskDetailsHeadingLabel.Visible = false;
                     allServersForm.addSourceRefreshLinkLabel.Visible = false;
                     allServersForm.progressBar.Visible = true;
                     allServersForm.vmDetailsPanelpanel.Visible = false;
                     if (allServersForm.selectedMasterListbyUser._hostList.Count != 0)
                     {
                         Host masterTargetHost = (Host)allServersForm.selectedMasterListbyUser._hostList[0];
                         foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                         {
                          
                             if (h.targetDataStore != null)
                             {//reassingin the values of disk to the datastore....
                                
                                 foreach (DataStore d in masterTargetHost.dataStoreList)
                                 {   Trace.WriteLine(DateTime.Now + "\t Verfying the datastore " + h.targetDataStore + " \ttarget " +d.name );
                                     if (d.name == h.targetDataStore && d.vSpherehostname == masterTargetHost.vSpherehost)
                                     {
                                         h.targetDataStore = null;
                                         Trace.WriteLine(DateTime.Now + "\t Adding the datastore size ");
                                         d.freeSpace = d.freeSpace + h.totalSizeOfDisks;
                                         Trace.WriteLine(DateTime.Now + "\t Freesize after adding " + d.freeSpace.ToString());                                                                                 
                                     }
                                 }                                 
                             }
                         }

                         
                        
                     }
                     allServersForm.initialSourceListFromXml = new HostList();
                     allServersForm.selectedSourceListByUser = new HostList();
                     _esxInfo.GetHostList.Clear();                   
                     allServersForm.nextButton.Enabled = false;
                     powerOn = false;
                     allServersForm.loadingPictureBox.BringToFront();
                     allServersForm.loadingPictureBox.Visible = true;
                     allServersForm.esxDetailsBackgroundWorker.RunWorkerAsync();
                 }
                 else
                 {
                     MessageBox.Show("We don't have credential to retrive data can you enter credentials and Click Get Details button");
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
       
        /*
         * For 6.2 GA which is 2.0(v6) of vContinuum. We had introduced select all feature.
         * With that user can select all the vm with a single click.
         * this will be called when user clicks on the parent node check box of treeview.
         */
         internal bool SelectAllHostsAtOneShot(AllServersForm allServersForm, string inDisplayName, int index)
         {
             try
             {
                 if (inDisplayName.Length <= 0)
                 {
                     Trace.WriteLine(DateTime.Now + " \t P2vSelectSecondaryPanelHandler:MasterVmSelected: Display name is empty  ");
                     return false;
                 }


                 //To know the esx ip we added this logic

                 Host tempHost = new Host();
                 tempHost.displayname = inDisplayName;
                 Trace.WriteLine(DateTime.Now + "         \t Printing tempHost display name " + tempHost.displayname);
                 int listIndex = 0;

                 allServersForm.initialSourceListFromXml.DoesHostExist(tempHost, ref listIndex);

                 _selectedHost = (Host)allServersForm.initialSourceListFromXml._hostList[listIndex];
                 if (_selectedHost.hostname == "NOT SET")
                 {
                     //MessageBox.Show("Selected VM's hostname is not set try again by using refresh option", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                     return false;
                 }
                 if (_selectedHost.ip == "NOT SET")
                 {
                     //MessageBox.Show("Selected VM's IP is not set", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                     return false;

                 }
                 if (_selectedHost.vmwareTools == "NotInstalled")
                 {
                     String message = _selectedHost.displayname + ": VM does not have VMware tools installed" +
                                       Environment.NewLine + "Please install VMware tools on the VM and use refresh to update data.";

                     //MessageBox.Show(message, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                     return false;
                 }
                 else if (_selectedHost.vmwareTools == "NotRunning")
                 {

                     //MessageBox.Show("VMware tools are not running on this VM. Please make sure VMware tools are running and then refresh to update data.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                     return false;
                 }
                 //else if (_selectedHost.vmwareTools == "OutOfDate")
                 //{
                 //    //MessageBox.Show("VMware tools are out of date on this VM. Please update VMware tools on this VM: " + _selectedHost.displayname, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                 //    return false;
                 //}
                 if (_selectedHost.nicList.list.Count == 0)
                 {
                     return false;
                 }
                 if (_selectedHost.rdmpDisk == "TRUE")
                 {
                     switch (MessageBox.Show("This VM: " + _selectedHost.displayname + " has RDM disks. Do you want to convert RDM disks to vmdks?" + Environment.NewLine + Environment.NewLine + "Choose Yes to convert RDM disks to vmdk disks." + Environment.NewLine + "Choose No to keep them as RDM disks.", "", MessageBoxButtons.YesNo, MessageBoxIcon.Question))
                     {
                         case DialogResult.No:

                             //allServersForm._convertRdmDiskToVmdk = "no";

                             _selectedHost.convertRdmpDiskToVmdk = false;
                             //Rdm to rdm support is not supporterd so here we are not allowing.
                             if (allServersForm.appNameSelected == AppName.Offlinesync)
                             {
                                 MessageBox.Show("RDM support at the time of offlinesync is not there", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                 return false;
                             }

                             break;
                         case DialogResult.Yes:
                             _selectedHost.convertRdmpDiskToVmdk = true;
                             break;
                     }




                 }
                 if (_selectedHost.poweredStatus == "OFF")
                 {
                     return false;
                     
                 }
                 
                 else
                 {
                     if (CheckIdeDisk(allServersForm, _selectedHost) == false)
                     {
                         return false;
                     }
                     allServersForm.selectedSourceListByUser.AddOrReplaceHost(_selectedHost);
                     allServersForm.totalCountLabel.Text = "Total number of vms selected for protection is " + allServersForm.selectedSourceListByUser._hostList.Count.ToString();
                     allServersForm.totalCountLabel.Visible = true;
                     //EsxDiskDetails(allServersForm, _selectedHost);
                     allServersForm.nextButton.Enabled = true;
                 }
                 return true;
             }
             catch (Exception ex)
             {

                 System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                 Trace.WriteLine("ERROR*******************************************");
                 Trace.WriteLine(DateTime.Now + "Caught exception at Method: SelectAllHostsAtOneShot()  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                 Trace.WriteLine("Exception  =  " + ex.Message);
                 Trace.WriteLine("ERROR___________________________________________");

                 return false;
             }
         }

        /*
         * If selected Vm has multiple ips. we will display all ips when user moves mouse over to that ip label or he can click 
         * View all label in the vm details panel.
         * That will call this method.
         */
         internal bool DisplayAllIPs(AllServersForm allServersForm)
         {
             try
             {
                 toolTipforOsDisplay = new ToolTip();
                 toolTipforOsDisplay.AutoPopDelay = 5000;
                 toolTipforOsDisplay.InitialDelay = 100;
                 toolTipforOsDisplay.ReshowDelay = 500;
                 toolTipforOsDisplay.ShowAlways = false;
                 toolTipforOsDisplay.GetLifetimeService();
                 toolTipforOsDisplay.IsBalloon = true;
                 toolTipforOsDisplay.Active = true;
                 toolTipforOsDisplay.UseAnimation = true;
                 toolTipforOsDisplay.RemoveAll();
                 
                 toolTipforOsDisplay.SetToolTip(allServersForm.viewAllIPsLabel, _selectedHost.ip);
                 //toolTip.Show(_selectedHost.ip,allServersForm.viewAllIPsLabel);
                 
                 return true;
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
                 Trace.WriteLine(DateTime.Now + "Caught exception at Method: DisplayAllIPs " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                 Trace.WriteLine("Exception  =  " + ex.Message);
                 Trace.WriteLine("ERROR___________________________________________");
                 return false;
             }
         }


         internal bool UnDisplayAllIPs(AllServersForm allServersForm)
         {
             try
             {
                 toolTipforOsDisplay = new ToolTip();
                 toolTipforOsDisplay.Active = false;
                 toolTipforOsDisplay.Dispose();
                 foreach (Control c in allServersForm.Controls)
                 {
                     if (c.Tag == toolTipforOsDisplay)
                     {
                         c.Visible = false;
                     }
                 }
                 return true;
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
                 Trace.WriteLine(DateTime.Now + "Caught exception at Method: DisplayAllIPs " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                 Trace.WriteLine("Exception  =  " + ex.Message);
                 Trace.WriteLine("ERROR___________________________________________");
                 return false;
             }
         }
        /*
         * At the time of the Selecting VM fopr protection.
         * We will check particular host is in selected list or not.
         * This is for to display the disks details
         * We will display disks only if selected for protection.
         */
         internal bool CheckHostContainsinWhichList(AllServersForm allServersForm, string inDisplayName, string vSphereHostName)
         {
             try
             {
                 Trace.WriteLine(DateTime.Now + " \t Entered CheckHostContainsinWhichList");
                 if (allServersForm.appNameSelected == AppName.V2P)
                 {
                     if (allServersForm.selectedSourceListByUser._hostList.Count != 0)
                     {
                         Host h = new Host();
                         h.displayname = inDisplayName;
                         h = (Host)allServersForm.selectedSourceListByUser._hostList[0];
                         EsxDiskDetails(allServersForm, h);
                         FillGeneralTableWithHostDetails(allServersForm, h);
                     }
                 }
                 else
                 {
                     Host h = new Host();
                     h.displayname = inDisplayName;
                     h.vSpherehost = vSphereHostName;
                     int index = 0;
                     if (allServersForm.selectedSourceListByUser.DoesHostExist(h, ref index) == true)
                     {
                         Trace.WriteLine(DateTime.Now + "\t Host contains in the seectedSourceList");
                         h = (Host)allServersForm.selectedSourceListByUser._hostList[index];
                         FillGeneralTableWithHostDetails(allServersForm, h);
                         _selectedHost = h;
                         EsxDiskDetails(allServersForm, h);


                     }
                     else
                     {
                         Trace.WriteLine(DateTime.Now + "\t Host does not contains in the seectedSourceList");
                         if (allServersForm.initialSourceListFromXml.DoesHostExist(h, ref index) == true)
                         {
                             Trace.WriteLine(DateTime.Now + "\t Host contains in the _initialSourceList");
                             h = (Host)allServersForm.initialSourceListFromXml._hostList[index];
                             FillGeneralTableWithHostDetails(allServersForm, h);
                             _selectedHost = h;
                             allServersForm.esxDiskInfoDatagridView.Visible = false;
                         }
                         else
                         {
                             Trace.WriteLine(DateTime.Now + "\t Host does not contains in the _initialSourceList");
                             allServersForm.esxDiskInfoDatagridView.Visible = false;
                             allServersForm.vmDetailsPanelpanel.Visible = false;
                             Trace.WriteLine(DateTime.Now + " \t Exiting CheckHostContainsinWhichList with return false " + inDisplayName);
                             return false;
                         }
                         allServersForm.diskDetailsHeadingLabel.Visible = false;
                         allServersForm.esxDiskInfoDatagridView.Visible = false;
                         //allServersForm.selectedDiskLabel.Visible = false;
                         //allServersForm.totalNumberOfDisksLabel.Visible = false;
                     }
                 }
                 Trace.WriteLine(DateTime.Now + " \t Exiting CheckHostContainsinWhichList");
                 return true;
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
                 Trace.WriteLine(DateTime.Now + "Caught exception at Method: CheckHostContainsinWhichList " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                 Trace.WriteLine("Exception  =  " + ex.Message);
                 Trace.WriteLine("ERROR___________________________________________");
                 return false;
             }

         }
        /*
         * If user selected any vm we need to fill the VM details in the Vm details panel
         * In this method we will fill all the vm details we have in our datastructures.
         * Like cpu,memory,os system,vm ware tools, powered status etc.....
         */
         internal bool FillGeneralTableWithHostDetails(AllServersForm allServersForm, Host h)
         {
             try
             {
                 if (h.hostname != null)
                 {
                     allServersForm.hostNameLabel.Text = h.hostname;
                     allServersForm.hostNameLabel.Visible = true;
                 }
                 else
                 {
                     allServersForm.hostDetailsLabel.Visible = false;
                 }
                 if (h.ip != null)
                 {
                     string[] ips = h.ip.Split(',');
                     allServersForm.ipAddressEnterLabel.Text = ips[0];
                     allServersForm.ipAddressEnterLabel.Visible = true;
                     if (ips.Length > 1)
                     {
                         allServersForm.viewAllIPsLabel.Visible = true;
                     }
                     else
                     {
                         allServersForm.viewAllIPsLabel.Visible = false;
                     }
                 }
                 else
                 {
                     allServersForm.ipAddressEnterLabel.Visible = false;
                 }
                 if (h.poweredStatus != null)
                 {
                     if (h.poweredStatus == "OFF")
                     {

                         allServersForm.vmStatusEnterLabel.Text = "Powered Off";
                     }
                     else
                     {
                         allServersForm.vmStatusEnterLabel.Text = "Powered On";
                     }
                     allServersForm.vmStatusEnterLabel.Visible = true;
                 }
                 else
                 {
                     allServersForm.vmStatusEnterLabel.Visible = false;
                 }
                 if (h.vmwareTools != null)
                 {
                     allServersForm.vmWareStatusEnterLabel.Text = h.vmwareTools;
                     allServersForm.vmWareStatusEnterLabel.Visible = true;
                 }
                 else
                 {
                     allServersForm.vmWareStatusEnterLabel.Visible = false;
                 }
                 if (h.memSize != 0)
                 {
                     allServersForm.memoryValueLabel.Text = h.memSize.ToString() + " (MB)";
                 }
                 if (h.cpuCount != 0)
                 {
                     allServersForm.cpuValueLabel.Text = h.cpuCount.ToString();
                 }
                 if (h.vmxversion != null)
                 {
                     string[] version = h.vmxversion.Split('-');
                     string vmxversion = version[1];
                     if (vmxversion.Length > 1)
                     {
                         allServersForm.vmVersionValueLabel.Text = vmxversion.Substring(1);
                     }
                     else
                     {
                         allServersForm.vmVersionValueLabel.Text = vmxversion;
                     }
                 }
                 if (h.operatingSystem != null)
                 {
                     allServersForm.guestOSValueLabel.Text = h.operatingSystem;
                 }

                 if (h.cluster == "yes")
                 {
                     allServersForm.v2vClusterValueLabel.Text = "Yes";
                 }
                 else
                 {
                     allServersForm.v2vClusterValueLabel.Text = "No";
                 }
                 allServersForm.vmDetailsPanelpanel.Visible = true;
                 allServersForm.vmDetailsPanelpanel.BringToFront();
                 return true;
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
                 return false;
             }
         }
        /*
         * In vm details panel we can't show entirs os of some vms.
         * So we are displaying in the tool tip.
         * So we have added this method.
         */
         internal bool DisplayEntireTextofOs(AllServersForm allServersForm)
         {
             try
             {
                 toolTipforOsDisplay = new ToolTip();
                 toolTipforOsDisplay.AutoPopDelay = 5000;
                 toolTipforOsDisplay.InitialDelay = 100;
                 toolTipforOsDisplay.ReshowDelay = 500;
                 toolTipforOsDisplay.ShowAlways = true;
                 toolTipforOsDisplay.GetLifetimeService();
                 toolTipforOsDisplay.IsBalloon = true;
                 toolTipforOsDisplay.RemoveAll();
                 toolTipforOsDisplay.UseAnimation = true;
                 toolTipforOsDisplay.SetToolTip(allServersForm.guestOSValueLabel, _selectedHost.operatingSystem);
                 return true;
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
                 return false;
             }
         }
         internal bool UnDisplayEntireTextofOs(AllServersForm allServersForm)
         {
             try
             {
                 toolTipforOsDisplay.Dispose();
                 return true;
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
                 return false;
             }
         }

         internal bool SelectedVmToProtect(AllServersForm allServeraForm, Host h)
         {
             if (h.hostname == "NOT SET")
             {

             }

             return true;
         }

         internal bool ShowSkipData(AllServersForm allServersForm)
         {
             try
             {
                 if (_skipData != null)
                 {
                     MessageBox.Show(_skipData, "Skipped some of the Data", MessageBoxButtons.OK, MessageBoxIcon.Information);
                 }
             }
             catch (Exception ex)
             {
                 System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                 Trace.WriteLine("ERROR*******************************************");
                 Trace.WriteLine(DateTime.Now + "Caught exception at Method: addSourceWarningLinkLabel_LinkClicked " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                 Trace.WriteLine("Exception  =  " + ex.Message);
                 Trace.WriteLine("ERROR___________________________________________");
             }


             return true;
         }


         internal bool CheckinVMinCX(AllServersForm allServersForm, ref  Host h)
         {

             Trace.WriteLine(DateTime.Now + "\t Entered CheckinVMinCX in ESX_PrimaryServerPanelHandler.cs" );
             if (h.displayname == null || h.hostname == null || h.ip == null)
             {
                 if (_selectedHost.hostname != null && _selectedHost.displayname != null)
                 {
                     h.displayname = _selectedHost.displayname;
                     h.hostname = _selectedHost.hostname;
                     h.ip = _selectedHost.ip;
                 }
             }
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
                     if(h.masterTarget == true)
                     {
                         MessageBox.Show("Master target agent is installed on this host. It can't be protected. Uninstall the agent and install as Scout Agent. ", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                         return false;
                     }
                     h = WinPreReqs.GetHost;
                     Trace.WriteLine(DateTime.Now + "\t Successfully got the uuid for the machine " + h.displayname);
                 }
                 else
                 {
                     h = WinPreReqs.GetHost;
                     _vmExistsinCXDataBase = false;
                     Trace.WriteLine(DateTime.Now + "\t Failed to find vm in  cx " + h.displayname);
                     MessageBox.Show("VM:" + displayname + " (" + ip + " )" + " information is not found in the CX server " +
                     Environment.NewLine + Environment.NewLine + "Please verify that" +
                     Environment.NewLine +
                     Environment.NewLine + "1. Agent is installed" +
                     Environment.NewLine + "2. Machine is up and running" +
                     Environment.NewLine + "3. vContinuum wizard is pointing to " + WinTools.GetcxIP() + " with port number " + WinTools.GetcxPortNumber() +
                     Environment.NewLine + "   Make sure that both machine and vContinuum wizard are pointed to same CX." +
                     Environment.NewLine + "4. InMage Scout services are up and running", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);

                     return false;
                 }
                 Host tempHost1 = new Host();
                 tempHost1.inmage_hostid = h.inmage_hostid;
                 Cxapicalls cxApi = new Cxapicalls();
                 if (cxApi.Post( tempHost1, "GetHostInfo") == true)
                 {
                     tempHost1 = Cxapicalls.GetHost;
                     _vmExistsinCXDataBase = true;
                     h.nicList.list = tempHost1.nicList.list;
                     foreach (Hashtable hash in _selectedHost.nicList.list)
                     {
                         foreach (Hashtable tempHash in tempHost1.nicList.list)
                         {
                             if (hash["nic_mac"] != null && tempHash["nic_mac"] != null)
                             {
                                 if (hash["nic_mac"].ToString().ToUpper() == tempHash["nic_mac"].ToString().ToUpper())
                                 {
                                     tempHash["network_name"] = hash["network_name"];
                                     tempHash["adapter_type"] = hash["adapter_type"];
                                 }
                             }
                         }
                     }
                     _selectedHost.nicList.list = tempHost1.nicList.list;

                     
                     _selectedHost.requestId = tempHost1.requestId;


                     foreach (Hashtable selectedHostHash in tempHost1.disks.list)
                     {
                         if (selectedHostHash["DiskScsiId"] != null)
                         {
                             string sourceSCSIId = null;
                             sourceSCSIId = selectedHostHash["DiskScsiId"].ToString();
                             if (sourceSCSIId != null && sourceSCSIId.Length > 0)
                             {
                                 
                                 Trace.WriteLine(DateTime.Now + "\t Printing scsi id before removing " + sourceSCSIId);
                                 sourceSCSIId = sourceSCSIId.Remove(0, 1);
                                 Trace.WriteLine(DateTime.Now + "\t Printing scsi id after removing first char - " + sourceSCSIId);
                                 selectedHostHash.Add("source_disk_uuid_to_use", sourceSCSIId);
                             }
                         }
                     }
                     foreach (Hashtable selectedHostHash in _selectedHost.disks.list)
                     {
                         if (selectedHostHash["source_disk_uuid"] != null)
                         {
                             string sourceSCSIId = null;
                             sourceSCSIId = selectedHostHash["source_disk_uuid"].ToString();
                             if (sourceSCSIId != null && sourceSCSIId.Length > 0)
                             {
                                 Trace.WriteLine(DateTime.Now + "\t Printing scsi id before replacing with - " + sourceSCSIId);
                                 sourceSCSIId = sourceSCSIId.Replace("-", "");
                                 Trace.WriteLine(DateTime.Now + "\t Printing scsi id after replacing  - " + sourceSCSIId);

                                 if (selectedHostHash["disk_id_to_compare"] == null)
                                 {
                                     selectedHostHash.Add("disk_id_to_compare", sourceSCSIId);
                                 }
                                 else
                                 {
                                     selectedHostHash["disk_id_to_compare"] = sourceSCSIId;
                                 }
                             }

                         }
                     }

                     foreach (Hashtable selectedHostHash in _selectedHost.disks.list)
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
                                         if (selectedHostHash["disk_signature"] == null)
                                         {
                                             selectedHostHash.Add("disk_signature", hashofTempHost["disk_signature"].ToString());
                                         }
                                         else
                                         {
                                             selectedHostHash["disk_signature"] = hashofTempHost["disk_signature"].ToString();
                                         }
                                     }
                                     if (hashofTempHost["IsBootable"] != null)
                                     {
                                         if (selectedHostHash["IsBootable"] == null)
                                         {
                                             selectedHostHash.Add("IsBootable", hashofTempHost["IsBootable"].ToString());
                                         }
                                         else
                                         {
                                             selectedHostHash["IsBootable"] = hashofTempHost["IsBootable"].ToString();
                                         }

                                     }
                                     if (hashofTempHost["volumelist"] != null)
                                     {
                                         if (selectedHostHash["volumelist"] == null)
                                         {
                                             selectedHostHash.Add("volumelist", hashofTempHost["volumelist"].ToString());
                                         }
                                         else
                                         {
                                             selectedHostHash["volumelist"] = hashofTempHost["volumelist"].ToString();
                                         }
                                     }
                                     if(hashofTempHost["src_devicename"] != null)
                                     {
                                         if (selectedHostHash["src_devicename"] == null)
                                         {
                                             selectedHostHash.Add("src_devicename", hashofTempHost["src_devicename"].ToString());
                                         }
                                         else
                                         {
                                             selectedHostHash["src_devicename"] = hashofTempHost["src_devicename"].ToString();
                                         }
                                     }
                                     if (hashofTempHost["disk_type_os"] != null)
                                     {
                                         if (selectedHostHash["disk_type_os"] == null)
                                         {
                                             selectedHostHash.Add("disk_type_os", hashofTempHost["disk_type_os"].ToString());
                                         }
                                         else
                                         {
                                             selectedHostHash["disk_type_os"] = hashofTempHost["disk_type_os"].ToString();
                                         }
                                     }
                                     if (hashofTempHost["efi"] != null)
                                     {
                                         if (selectedHostHash["efi"] == null)
                                         {
                                             selectedHostHash.Add("efi", hashofTempHost["efi"].ToString());
                                         }
                                         else
                                         {
                                             selectedHostHash["efi"] = hashofTempHost["efi"].ToString();
                                         }
                                     }
                                     if (hashofTempHost["DiskLayout"] != null)
                                     {
                                         if (selectedHostHash["DiskLayout"] == null)
                                         {
                                             selectedHostHash.Add("DiskLayout", hashofTempHost["DiskLayout"].ToString());
                                         }
                                         else
                                         {
                                             selectedHostHash["DiskLayout"] = hashofTempHost["DiskLayout"].ToString();
                                         }
                                     }
                                     if (hashofTempHost["DiskScsiId"] != null)
                                     {
                                         if (selectedHostHash["DiskScsiId"] == null)
                                         {
                                             selectedHostHash.Add("DiskScsiId", hashofTempHost["DiskScsiId"].ToString());
                                         }
                                         else
                                         {
                                             selectedHostHash["DiskScsiId"] = hashofTempHost["DiskScsiId"].ToString();
                                         }
                                     }
                                     break;
                                 }
                             }
                         }
                     }


                 }
                 else
                 {
                     _vmExistsinCXDataBase = false;
                     Trace.WriteLine(DateTime.Now + "\t Failed to find vm in  cx " + h.displayname);
                     MessageBox.Show("VM:" + displayname + " (" + ip + " )" + " information is not found in the CX server " +
                     Environment.NewLine + Environment.NewLine + "Please verify that" +
                     Environment.NewLine +
                     Environment.NewLine + "1. Agent is installed" +
                     Environment.NewLine + "2. Machine is up and running" +
                     Environment.NewLine + "3. vContinuum wizard is pointing to " + WinTools.GetcxIP() + " with port number " + WinTools.GetcxPortNumber() +
                     Environment.NewLine + "   Make sure that both machine and vContinuum wizard are pointed to same CX." +
                     Environment.NewLine + "4. InMage Scout services are up and running" +
                     Environment.NewLine + "5. Machine is licenced in CX.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);

                     return false;
                 }

             }
             return true;
         }

         internal bool CheckinVMinCXWithSilet(AllServersForm allServersForm, ref  Host h)
         {
             Trace.WriteLine(DateTime.Now + "\t Entered CheckinVMinCXWithSilet in ESX_PrimaryServerPanelHandler.cs");
             WinPreReqs win = new WinPreReqs(h.ip, h.domain, h.userName, h.passWord);
             int returnCode = 0;
             string cxip = WinTools.GetcxIPWithPortNumber();
             if (h.hostname != null)
             {

                 returnCode = win.SetHostIPinputhostname(h.hostname,  h.ip, WinTools.GetcxIPWithPortNumber());
                 h.ip = WinPreReqs.GetIPaddress;

                 if (returnCode != 0)
                 {
                     Trace.WriteLine(DateTime.Now + "\t Failed to get ip of the host " + h.hostname);
                     return false;
                 }
                 if (win.GetDetailsFromcxcli( h, cxip) == true)
                 {
                     h = WinPreReqs.GetHost;
                     if (h.vxagentInstalled == false)
                     {
                         return false;
                     }
                     if (h.vx_agent_heartbeat == false)
                     {
                         return false;
                     }
                     if(h.masterTarget == true)
                     {
                         return false;
                     }
                     Trace.WriteLine(DateTime.Now + "\t Successfully got the uuid for the machine " + h.displayname);
                 }
                 else
                 {
                     h = WinPreReqs.GetHost;
                     Trace.WriteLine(DateTime.Now + "\t Failed to find vm in  cx " + h.displayname);
                    // MessageBox.Show("VM: " + h.displayname + " didn't found in cx database.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                     return false;
                 }
                 Host tempHost1 = new Host();
                 tempHost1.inmage_hostid = h.inmage_hostid;

                 Cxapicalls cxApi = new Cxapicalls();
                 if (cxApi.Post( tempHost1, "GetHostInfo") == true)
                 {
                     tempHost1 = Cxapicalls.GetHost;
                     h.nicList.list = tempHost1.nicList.list;

                     foreach (Hashtable selectedHostHash in tempHost1.disks.list)
                     {
                         if (selectedHostHash["DiskScsiId"] != null)
                         {

                             string sourceSCSIId = null;
                             sourceSCSIId = selectedHostHash["DiskScsiId"].ToString();
                             if (sourceSCSIId != null)
                             {
                                 if (sourceSCSIId != null && sourceSCSIId.Length > 0)
                                 {
                                     Trace.WriteLine(DateTime.Now + "\t Printing scsi id before removing " + sourceSCSIId);
                                     sourceSCSIId = sourceSCSIId.Remove(0, 1);
                                     Trace.WriteLine(DateTime.Now + "\t Printing scsi id after removing first char - " + sourceSCSIId);

                                     if (selectedHostHash["source_disk_uuid_to_use"] == null)
                                     {
                                         selectedHostHash.Add("source_disk_uuid_to_use", sourceSCSIId);
                                     }
                                     else
                                     {
                                         selectedHostHash["source_disk_uuid_to_use"] = sourceSCSIId;
                                     }
                                 }
                             }
                         }
                     }
                     foreach (Hashtable selectedHostHash in h.disks.list)
                     {
                         if (selectedHostHash["source_disk_uuid"] != null)
                         {
                             string sourceSCSIId = null;
                             sourceSCSIId = selectedHostHash["source_disk_uuid"].ToString();
                             if (sourceSCSIId != null && sourceSCSIId.Length > 0)
                             {
                                 Trace.WriteLine(DateTime.Now + "\t Printing scsi id before replacing with - " + sourceSCSIId);
                                 sourceSCSIId = sourceSCSIId.Replace("-", "");
                                 Trace.WriteLine(DateTime.Now + "\t Printing scsi id after replacing  - " + sourceSCSIId);
                                 if (selectedHostHash["disk_id_to_compare"] == null)
                                 {
                                     selectedHostHash.Add("disk_id_to_compare", sourceSCSIId);
                                 }
                                 else
                                 {
                                     selectedHostHash["disk_id_to_compare"] = sourceSCSIId;
                                 }
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
                                         if (selectedHostHash["disk_signature"] == null)
                                         {
                                             selectedHostHash.Add("disk_signature", hashofTempHost["disk_signature"].ToString());
                                         }
                                         else
                                         {
                                             selectedHostHash["disk_signature"] = hashofTempHost["disk_signature"].ToString();
                                         }
                                     }
                                     if (hashofTempHost["volumelist"] != null)
                                     {
                                         if (selectedHostHash["volumelist"] == null)
                                         {
                                             selectedHostHash.Add("volumelist", hashofTempHost["volumelist"].ToString());
                                         }
                                         else
                                         {
                                             selectedHostHash["volumelist"] = hashofTempHost["volumelist"].ToString();
                                         }
                                     }
                                     if (hashofTempHost["IsBootable"] != null)
                                     {
                                         if (selectedHostHash["IsBootable"] == null)
                                         {
                                             selectedHostHash.Add("IsBootable", hashofTempHost["IsBootable"].ToString());
                                         }
                                         else
                                         {
                                             selectedHostHash["IsBootable"] = hashofTempHost["IsBootable"].ToString();
                                         }

                                     }
                                     if(hashofTempHost["src_devicename"] != null)
                                     {
                                         if (selectedHostHash["src_devicename"] == null)
                                         {
                                             selectedHostHash.Add("src_devicename", hashofTempHost["src_devicename"].ToString());
                                         }
                                         else
                                         {
                                             selectedHostHash["src_devicename"] = hashofTempHost["src_devicename"].ToString();
                                         }
                                     }
                                     if (hashofTempHost["disk_type_os"] != null)
                                     {
                                         if (selectedHostHash["disk_type_os"] == null)
                                         {
                                             selectedHostHash.Add("disk_type_os", hashofTempHost["disk_type_os"].ToString());
                                         }
                                         else
                                         {
                                             selectedHostHash["disk_type_os"] = hashofTempHost["disk_type_os"].ToString();
                                         }
                                     }
                                     if (hashofTempHost["efi"] != null)
                                     {
                                         if (selectedHostHash["efi"] == null)
                                         {
                                             selectedHostHash.Add("efi", hashofTempHost["efi"].ToString());
                                         }
                                         else
                                         {
                                             selectedHostHash["efi"] = hashofTempHost["efi"].ToString();
                                         }
                                     }
                                     if (hashofTempHost["DiskLayout"] != null)
                                     {
                                         if (selectedHostHash["DiskLayout"] == null)
                                         {
                                             selectedHostHash.Add("DiskLayout", hashofTempHost["DiskLayout"].ToString());
                                         }
                                         else
                                         {
                                             selectedHostHash["DiskLayout"] = hashofTempHost["DiskLayout"].ToString();
                                         }
                                     }
                                     if (hashofTempHost["DiskScsiId"] != null)
                                     {
                                         if (selectedHostHash["DiskScsiId"] == null)
                                         {
                                             selectedHostHash.Add("DiskScsiId", hashofTempHost["DiskScsiId"].ToString());
                                         }
                                         else
                                         {
                                             selectedHostHash["DiskScsiId"] = hashofTempHost["DiskScsiId"].ToString();
                                         }
                                     }
                                     break;
                                 }
                             }
                         }
                     }
                 }
                 else
                 {
                     Trace.WriteLine(DateTime.Now + "\t Failed to find vm in  cx " + h.displayname);
                     //MessageBox.Show("VM: " + h.displayname + " Didn't found in cx database.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                     return false;
                 }

             }
             return true;
         }
    }
}