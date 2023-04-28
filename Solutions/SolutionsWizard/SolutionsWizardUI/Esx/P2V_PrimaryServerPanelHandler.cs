using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.Windows.Forms;
using System.Threading;
using Com.Inmage;
using System.Collections;
using Com.Inmage.Tools;
using System.IO;

using System.Runtime.InteropServices;
using System.Runtime.Serialization;


namespace Com.Inmage.Wizard
{
    class P2V_PrimaryServerPanelHandler : PanelHandler
    {
        internal static int BOTTABLE_ROW = 0;
        internal static int IP_COLUMN = 0;
        internal static int REMOVE_COLUMN = 2;
        internal static int OS_COLUMN = 1;
        internal static int CREDENTIALS_COLUMN = 2;
        internal static int ADDLOGIN_COLUMN = 3;
        internal static int PHYSICAL_MACHINE_DISK_NUMBER_COLUMN = 0;
        internal static int PHYSICAL_MACHINE_DISK_SIZE_COLUMN = 1;
        internal static int PHYSICAL_MACHINE_DISK_SELECT_COLUMN = 2;
        internal static int USE_NAT_IP_COLUMN = 3;
        internal HostList _initialList = new HostList();
        internal int rowIndex = 0;
        Host _selectedHost = new Host();
        ToolTip toolTipforOSDisplay = new ToolTip();
        AllServersForm _allServersForm;
        public ArrayList clusterList = new ArrayList();
        static HostList _tempHostList = new HostList();





        public P2V_PrimaryServerPanelHandler()
        {
            
            
        }

        internal override bool Initialize(AllServersForm allServersForm)
        {
            try
            {
                Trace.WriteLine(DateTime.Now + " \t Entered: Initialize Of the P2V_PrimaryServerPanelHandler");
                allServersForm.mandatoryLabel.Visible = false;
                if (allServersForm.appNameSelected == AppName.Pushagent)
                {
                    allServersForm.addSourcePanelheadingLabel.Text = "Select machines to push agent";
                    allServersForm.p2vHeadingLabel.Text = "Push Agent";
                    allServersForm.Text = "Push Agent";
                    
                }
                else if (allServersForm.appNameSelected == AppName.V2P)
                {
                    try
                    {
                        if (allServersForm.p2vOSComboBox.Items.Count != 0)
                        {
                            allServersForm.p2vOSComboBox.Items.Clear();
                        }
                    }
                    catch (Exception ex)
                    {
                        Trace.WriteLine(DateTime.Now + "\t failed to delete items in combo box " + ex.Message);
                    }
                    if (allServersForm.osTypeSelected == OStype.Windows)
                    {
                        allServersForm.p2vOSComboBox.Items.Add("Windows");
                        allServersForm.vepNoteLabel.Text = "Note:  Select target physical server booted with Windows 2 Go OS";
                    }
                    else if(allServersForm.osTypeSelected == OStype.Linux)
                    {
                        allServersForm.vepNoteLabel.Text = "Note:  Select target physical server booted with Linux Live CD";
                        allServersForm.p2vOSComboBox.Items.Add("Linux");
                    }
                   
                    allServersForm.p2vOSComboBox.SelectedIndex = 0;
                }
                else
                {
                    allServersForm.p2vOSComboBox.Items.Add("Windows");
                    allServersForm.p2vOSComboBox.Items.Add("Linux");
                    allServersForm.p2vOSComboBox.SelectedIndex = 0;
                    allServersForm.p2vHeadingLabel.Text = "P2V Protection";
                }
                allServersForm.primaryServerPanelIpText.Text = WinTools.GetcxIP();
                allServersForm.primaryServerPanelIpText.Enabled = false;
                allServersForm.helpContentLabel.Text = HelpForcx.P2vProtectScreen1;
                allServersForm.primaryServerPanelIpText.Select();
                //allServersForm.primaryDataGridView.Columns[USE_NAT_IP_COLUMN].Visible = false;   
                if (allServersForm.appNameSelected != AppName.V2P)
                {
                    allServersForm.addCredentialsLabel.Text = "Select Source server";
                }
                allServersForm.nextButton.Enabled = false;
                allServersForm.previousButton.Visible = false;
                if (allServersForm.appNameSelected == AppName.V2P)
                {
                    allServersForm.previousButton.Visible = true;
                    allServersForm.previousButton.Enabled = true;
                    if (allServersForm.selectedMasterListbyUser._hostList.Count == 0)
                    {
                        allServersForm.p2vLoadingBackgroundWorker.RunWorkerAsync();
                    }
                    else
                    {
                        allServersForm.nextButton.Enabled = true;
                    }
                }
                else
                {
                    allServersForm.primaryServerAddButton.Enabled = false;
                    allServersForm.progressBar.Visible = true;
                    allServersForm.p2vLoadingBackgroundWorker.RunWorkerAsync();
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
            Trace.WriteLine(DateTime.Now + " \t Exiting: Initialize Of the P2V_PrimaryServerPanelHandler");
            return true;
        }
        internal override bool ProcessPanel(AllServersForm allServersForm)
        {
            Trace.WriteLine(DateTime.Now + " \t Entered: ProcessPanel Of the P2V_PrimaryServerPanelHandler");
            try
            {           
                foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                {
                    bool selectedDiskForProtection = false;
                    ArrayList physicalDisks;
                    //h.disks.AddScsiNumbers();
                    physicalDisks = h.disks.GetDiskList;
                    h.totalSizeOfDisks = 0;
                    int diskCount = 0;
                    foreach (Hashtable disk in physicalDisks)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Printing the disk selected value " + disk["Selected"].ToString());
                        if (disk["Selected"].ToString() == "Yes")
                        {
                            if (disk["Size"] != null)
                            {
                                diskCount++;
                                float size = float.Parse(disk["Size"].ToString());
                                h.selectedDisk = true;
                                selectedDiskForProtection = true;
                                size = size / (1024 * 1024);
                                h.totalSizeOfDisks = h.totalSizeOfDisks + size;
                                h.machineType = "PhysicalMachine";
                                // Trace.WriteLine("Printing the value of the disk " + h.totalSizeOfDisks);
                            }                            
                        }                        
                    }


                    if (h.totalSizeOfDisks.ToString().Contains("."))
                    {
                        h.totalSizeOfDisks = (float)Math.Round(h.totalSizeOfDisks, 0);
                    }
                    //checking that user selects bootable disk or not if not we are n ot allowing to proceed.
                    h.selectedDisk = selectedDiskForProtection;
                    bool bootableDiskSelected = false;
                    foreach (Hashtable disk in physicalDisks)
                    {
                        //MessageBox.Show(disk["IsBootable"].ToString());
                        if (disk["Selected"].ToString() == "Yes")
                        {
                            if (disk["IsBootable"] != null)
                            {
                                if (disk["IsBootable"].ToString().ToUpper() == "TRUE")
                                {
                                    bootableDiskSelected = true;
                                }
                            }
                        }
                    }
                    if (allServersForm.appNameSelected != AppName.V2P)
                    {
                        if (allServersForm.osTypeSelected == OStype.Windows)
                        {
                            if (bootableDiskSelected == false)
                            {
                                MessageBox.Show("You need to select bootable disk for the machine " + h.displayname + " It is mandidatory");
                                return false;
                            }
                        }
                    }
                }
                if (allServersForm.osTypeSelected == OStype.Windows)
                {
                    foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                    {
                        if (h.selectedDisk == false)
                        {
                            MessageBox.Show("Machine: " + h.displayname + " None of the disks are selected for protecion.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            return false;
                        }
                    }
                }
                Debug.WriteLine("Printing Source List after get total size of disks ");
                // Earlier we use to check boot diks only for windows. As now we are getting boot valeu for linux also we have added check for linux machines as well.
                // In case of boot disk found on second disk we will sort the disk order and write disk2 as one.
                if ( allServersForm.appNameSelected != AppName.V2P)
                {
                    foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                    {
                        Host h1 = h;
                        Trace.WriteLine(DateTime.Now + "\t Disk count of machine " + h1.displayname + "\t count is " + h1.disks.list.Count.ToString());
                        RearrangeBootableDisk(ref h1);
                        h.disks.list = h1.disks.list;
                        Trace.WriteLine(DateTime.Now + "\t Disk count of machine " + h.displayname + "\t count is " + h.disks.list.Count.ToString());
                    }
                    
                }
                foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                {
                    h.disks.AddScsiNumbers();
                }
                /*if (allServersForm._osType == OStype.Windows)
                {
                    switch (MessageBox.Show("vContinuum does not support protection of machines with dynamic disks.\n Does the selected machines have dynamic disks? ", " Dynamic disk verification ", MessageBoxButtons.YesNo, MessageBoxIcon.Question))
                    {
                        case DialogResult.Yes:
                            MessageBox.Show("Dynamic disks are not supported. Exiting ...", "vContinuum");
                            allServersForm.Close();
                            return false;
                    }
                }*/
                //  allServersForm._selectedSourceList.Print();
               
               // This is vCon 2.1 dev. From this release we are not going to writevmx files for p2v we will 
                // write every thing in esx.xml
                /*if (allServersForm._selectedSourceList.GenerateVmxFile() == false)
                {
                    return false;
                }*/
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
            Trace.WriteLine(DateTime.Now + " \t Exiting: ProcessPanel Of the P2V_PrimaryServerPanelHandler");
            return true;            
        }
               
        //Method takes each selected host in the _allServersForm._selectedSourceList and generates
        // 1. A combines XML files ( SourceXml.xml) 
        // 2. A VMX file in a subdirectory with the same name as hostname
        internal bool GeneratePhysicalMachineConfigurations(ref HostList selectedHosts)
        {
            Trace.WriteLine(DateTime.Now + " \t Entered: GeneratePhysicalMachineConfigurations of P2V_PrimaryServerPanelHandler.cs");    
           // Debug.WriteLine("Count at the beining of GeneratePhysicalMachineConfigurations =" + _tempHostList._hostList.Count);
            //Debug.WriteLine("Going to collect info for following machines ");
           // selectedHosts.Print();
            try
            {
                HostList tempHostList = selectedHosts;
                foreach (Host selectedHost in tempHostList._hostList)
                {
                    if (selectedHost.ip == null)
                    {
                        MessageBox.Show("No IP addres is set for host" + selectedHost.hostname);
                        Debug.WriteLine(DateTime.Now + " \t No IP addres is set for host" + selectedHost.hostname);
                        // return false;
                    }
                    if (selectedHost.collectedDetails == false)
                    {
                        //Debug.WriteLine("\t Calling Remote.connect for " + selectedHost.ip +" With username = " + selectedHost.userName + " Password = " + selectedHost.passWord);
                        PhysicalMachine p = new PhysicalMachine(selectedHost.ip, selectedHost.domain, selectedHost.userName, selectedHost.passWord);
                        if (p.GetHostInfo() == true)
                        {
                            Host h = p.GetHost;
                            if (h == null)
                            {
                                return false;
                            }
                            h.role = Role.Primary;
                            selectedHosts.AddOrReplaceHost(h);
                        }
                        else
                        {
                            Trace.WriteLine(DateTime.Now + " \t Skipping Remote.connect for " + selectedHost.ip);
                        }
                    }
                }
                // WriteToXml takes two arguments. Xml file and parent tag to embedd it in
                selectedHosts.WriteToXmlFile("SourceXML.xml", "SRC_ESX");
                // Generates a VMX file from a template
                //selectedHosts.GenerateVmxFile();
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
          Trace.WriteLine(DateTime.Now + " \t Exiting: GeneratePhysicalMachineConfigurations of P2V_PrimaryServerPanelHandler.cs");
           return true;
        }

        internal override bool ValidatePanel(AllServersForm allServersForm)
        {
            try
            {
                Trace.WriteLine(DateTime.Now + " \t Entered: P2V_PrimaryServerPanelHandler  ValidatePanel");
                if (allServersForm.appNameSelected != AppName.V2P)
                {
                    if (allServersForm.selectedSourceListByUser._hostList.Count == 0)
                    {
                        MessageBox.Show("Please Select Source Server to Protect", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
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
            Trace.WriteLine(DateTime.Now + " \t Exiting: P2V_PrimaryServerPanelHandler  ValidatePanel");
            return true;
        }

        internal override bool CanGoToNextPanel(AllServersForm allServersForm)
        {
            
            return true;

        }
        internal override bool CanGoToPreviousPanel(AllServersForm allServersForm)
        {
            if (allServersForm.appNameSelected == AppName.V2P)
            {
                allServersForm.previousButton.Visible = false;
                allServersForm.nextButton.Enabled = true;
                allServersForm.nextButton.Visible = true;
            }
            return true;
        }



        private bool CheckWMiConnection(AllServersForm allServersForm,string ip,string domain,string username, string password)
        {
            if (allServersForm.primaryServerPanelIpText.Text.Trim().Length == 0)
            {
                MessageBox.Show("please enter ip", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return false;
            }
            
            allServersForm.primaryServerAddButton.Enabled = false;




            string error = "";
            if (allServersForm.osTypeSelected == OStype.Windows)
            {
                if (WinPreReqs.WindowsShareEnabled(ip, domain, username, password, "",  error) == false)
                {
                    error = WinPreReqs.GetError;
                    allServersForm.primaryServerAddButton.Enabled = true;
                    MessageBox.Show(error, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return false;
                }
            }
            return true;
        }


        internal bool AddIp(AllServersForm allServersForm)
        {
            Trace.WriteLine(DateTime.Now + " \t Entering: AddIp of P2V_PrimaryServerPanelHandler.cs");
            try
            {
                string ip, domain = null, username= null, password= null, message = "";
                if (allServersForm.appNameSelected != AppName.Pushagent)
                {
                    ip = allServersForm.primaryServerPanelIpText.Text.Trim();                    
                }
                else
                {
                    ip = allServersForm.pushIPTextBox.Text.Trim();
                    domain = allServersForm.pushUserNAmeTextBox.Text.Trim();
                    username = allServersForm.pushPasswordTextBox.Text.Trim();
                    password = allServersForm.pushDomainTextBox.Text;
                }
                //In case the IP is blank return false
                if (allServersForm.appNameSelected != AppName.Pushagent)
                {
                    if (allServersForm.p2vOSComboBox.SelectedItem.ToString() == "Windows")
                    {
                        
                        allServersForm.osTypeSelected = OStype.Windows;
                    }
                    else if (allServersForm.p2vOSComboBox.SelectedItem.ToString() == "Linux")
                    {

                        allServersForm.osTypeSelected = OStype.Linux;
                    }
                    else if(allServersForm.p2vOSComboBox.SelectedItem.ToString() == "Solaris")
                    {
                        allServersForm.osTypeSelected = OStype.Solaris;
                    }
                    allServersForm.primaryServerAddButton.Enabled = true;
                    allServersForm.p2vLoadingBackgroundWorker.RunWorkerAsync();

                }
                else
                {
                    if (PushChecksCredentail(allServersForm, ip, domain, username, password) == false)
                    {
                        Trace.WriteLine("Failed to connect to Windows machine");
                        return false;
                    }
                }
                if (allServersForm.selectedSourceListByUser._hostList.Count > 0)
                {
                    foreach (Host h2 in allServersForm.selectedSourceListByUser._hostList)
                    {
                        if (h2.ip == allServersForm.primaryServerPanelIpText.Text.Trim())
                        {
                            //MessageBox.Show(h2.ip + " is selected for protection");
                            allServersForm.primaryServerPanelIpText.Clear();
                            return false;
                        }
                    }
                }
                allServersForm.Enabled = false;
                Cursor.Current = Cursors.WaitCursor;
                // Debug.WriteLine("IP = " + ip + " domain =" + domain + " username = " + username + " Password = " + password);
                Host h = new Host();
                //For p2v we are going to support linux as well so now we are adding that checks.
                // For windows we will use wmi to connect to machine.
                //For linux we will use ssh to connect to machine.
                if (allServersForm.osTypeSelected == OStype.Windows && allServersForm.appNameSelected == AppName.Pushagent)
                {
                    if (allServersForm.selectedSourceListByUser._hostList.Count  != 0)
                    {
                        foreach(Host temp in allServersForm.selectedSourceListByUser._hostList)
                        {
                            if(temp.ip == ip)
                            {
                                Trace.WriteLine(DateTime.Now + "\t Already this host exists in list");
                                allServersForm.Enabled = true;
                                allServersForm.nextButton.Enabled = true;
                                allServersForm.primaryServerPanelIpText.Clear();
                                allServersForm.pushNoteLabel.Visible = true;
                                allServersForm.pushAgentDataGridView.Visible = true;
                                allServersForm.pushUninstallButton.Visible = true;
                                allServersForm.pushAgentsButton.Visible = true;
                                allServersForm.pushIPTextBox.Clear();
                                return true;
                            }
                        }
                    }
                    h = new Host();
                    if (ConnectToWindowsMachineToGetDetails(allServersForm, ip, domain, username, password, ref h) == false)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Failed to get information with machine ");
                        return false;
                    }

                }

                HostList tempList = new HostList();
                tempList.AddOrReplaceHost(h);
                Cxapicalls cxapi = new Cxapicalls();
                string cxip = WinTools.GetcxIP();
                cxapi.PostToGetHostDetails( tempList);
                tempList = Cxapicalls.GetHostlist;
                foreach (Host h2 in tempList._hostList)
                {
                    if (h.hostname.ToUpper() == h2.hostname.ToUpper())
                    {
                        h.fxagentInstalled = h2.fxagentInstalled;
                        h.vxagentInstalled = h2.vxagentInstalled;
                        h.unifiedagentVersion = h2.unifiedagentVersion;
                        h.inmage_hostid = h2.inmage_hostid;
                    }
                }

                if (allServersForm.appNameSelected == AppName.Pushagent)
                {
                    //Trace.WriteLine(DateTime.Now + "\t printing the memsize and cpu count " + h.memSize.ToString() + " cpu " + h.cpuCount.ToString());
                    h.https = WinTools.Https;
                    allServersForm.selectedSourceListByUser.AddOrReplaceHost(h);
                    _initialList.AddOrReplaceHost(h);
                    allServersForm.p2vOSComboBox.Enabled = false;                    
                    allServersForm.sourceServerLabel.Visible = true;
                    allServersForm.rowIndexPrepared = allServersForm.selectedSourceListByUser._hostList.Count - 1;
                  
                    //If user selcts for P2v push agents only it will comes here....
                        //filling the values into pushagent datagridview in pushagent screen.....
                        allServersForm.generalLogTextBox.Visible = true;
                        allServersForm.generalLogBoxClearLinkLabel.Visible = true;
                        //h.pushAgent = true;
                        allServersForm.pushAgentDataGridView.Rows.Add(1);
                        allServersForm.pushAgentDataGridView.Rows[allServersForm.rowIndexPrepared].Cells[PushAgentPanelHandler.DISPLAY_NAME_COLUMN].Value = h.displayname;
                        allServersForm.pushAgentDataGridView.Rows[allServersForm.rowIndexPrepared].Cells[PushAgentPanelHandler.IP_COLUMN].Value = h.ip;
                       // allServersForm.pushAgentDataGridView.Rows[allServersForm._rowIndex].Cells[PushAgentPanelHandler.ROLE_COLUMN].Value = h.hostname;
                        allServersForm.pushAgentDataGridView.Rows[allServersForm.rowIndexPrepared].Cells[PushAgentPanelHandler.PUSHAGENT_COLUMN].Value = true;
                        if (h.vxagentInstalled == true && h.fxagentInstalled == true)
                        {
                            allServersForm.pushAgentDataGridView.Rows[allServersForm.rowIndexPrepared].Cells[PushAgentPanelHandler.AGENT_STATUS_COLUMN].Value = "Installed";

                        }
                        else if (h.vxagentInstalled == true && h.fxagentInstalled == false)
                        {
                            allServersForm.pushAgentDataGridView.Rows[allServersForm.rowIndexPrepared].Cells[PushAgentPanelHandler.AGENT_STATUS_COLUMN].Value = "VX Installed";
                        }
                        else if (h.vxagentInstalled == false && h.fxagentInstalled == true)
                        {
                            allServersForm.pushAgentDataGridView.Rows[allServersForm.rowIndexPrepared].Cells[PushAgentPanelHandler.AGENT_STATUS_COLUMN].Value = "FX Installed";
                        }
                        else if (h.vxagentInstalled == false && h.fxagentInstalled == false)
                        {
                            allServersForm.pushAgentDataGridView.Rows[allServersForm.rowIndexPrepared].Cells[PushAgentPanelHandler.AGENT_STATUS_COLUMN].Value = "Not Installed";
                        }
                        if (h.unifiedagentVersion == null)
                        {
                            allServersForm.pushAgentDataGridView.Rows[allServersForm.rowIndexPrepared].Cells[PushAgentPanelHandler.VERSION_COLUMN].Value = "N/A";
                        }
                        else
                        {
                            allServersForm.pushAgentDataGridView.Rows[allServersForm.rowIndexPrepared].Cells[PushAgentPanelHandler.VERSION_COLUMN].Value = h.unifiedagentVersion;
                        }
                        allServersForm.pushAgentDataGridView.Rows[allServersForm.rowIndexPrepared].Cells[PushAgentPanelHandler.ADVANCED_COLUMN].Value = "Settings";
                        allServersForm.pushNoteLabel.Text = "Note: UA agent installation status is retried from CS server.( " +cxip + " )";
                        allServersForm.pushNoteLabel.Visible = true;
                        allServersForm.pushAgentDataGridView.Visible = true;
                        allServersForm.pushUninstallButton.Visible = true;
                        allServersForm.pushAgentsButton.Visible = true;
                        allServersForm.pushIPTextBox.Clear();
                        
                    
                }
                
               
                Host h1 = (Host)allServersForm.selectedSourceListByUser._hostList[0];
                //DisplayDriveDetails(allServersForm, h1);
                
                allServersForm.rowIndexPrepared++;
                allServersForm.Enabled = true; 
                allServersForm.nextButton.Enabled = true;
                allServersForm.primaryServerPanelIpText.Clear();
                Trace.WriteLine(DateTime.Now + " \t Exiting: AddIp of P2V_PrimaryServerPanelHandler.cs");
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                allServersForm.Enabled = true;
                return false;
            }
            return true; 
        }

        /*
         * Connect to windows machine with wmi to get the details of the machine
         * We are getitng nic,disk and machine info.
         */ 
        private bool ConnectToWindowsMachineToGetDetails(AllServersForm allServersForm, string ip, string domain, string username, string password,ref Host h)
        {
            try
            {
                PhysicalMachine p = new PhysicalMachine(ip, domain, username, password);
                if (p.Connected == false)
                {
                    allServersForm.Enabled = true; ;
                    allServersForm.nextButton.Enabled = true;
                    return false;
                }

                if( p.GetHostInfo() == true)
                {
                    h = p.GetHost;
                }
                if (h == null)
                {
                    allServersForm.Enabled = true;
                    allServersForm.nextButton.Enabled = true;
                    return false;
                }
            
                h.role = Role.Primary;
                // h.Print();
                if (allServersForm.appNameSelected == AppName.Pushagent)
                {
                    h.pushAgent = true;
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                allServersForm.Enabled = true;
                return false;
            }
            return true; 
        }


        //step1: verify whether credentails are working fine or not
        //step2: check whether it is registered with cx or not.
        //step3: Prepare shell script and copy to linux server.
        //step4: Execute shell script
        //step5: copy the file to latest folder
        //step6: read the xml example ip is LinuxInfo.xml then the file from linux machine is LinuxInfo.xml
        private bool ConnectToLinuxGetInfo(AllServersForm allServersForm,string ip, string username,string password,ref Host h)
        {
            try
            {
                //if (WinTools.ConnectToLinux(ip, username, password) == true)
                //{
                //    Trace.WriteLine(DateTime.Now + "\t Successfully verified the credentials given by user");
                //}
                //else
                //{
                //    allServersForm.Enabled = true;
                //    allServersForm.nextButton.Enabled = true;
                //    MessageBox.Show("Failed to connect to linux server. Check the gvien credentials", "Credential Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                //    Trace.WriteLine(DateTime.Now + "\t Failed to connect to linux server credentials given by user ");
                //    return false;
                //}

                if (File.Exists(WinTools.FxAgentPath() + "\\vContinuum\\Latest\\linuxinfo.xml"))
                {
                    try
                    {
                        File.Delete(WinTools.FxAgentPath() + "\\vContinuum\\Latest\\linuxinfo.xml");
                    }
                    catch (Exception ex)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Failed to delete file from the vConinuum\\Latest folder" + ex.Message);
                    }
                }
                WinTools win = new WinTools();

                WinPreReqs wp = new WinPreReqs("", "", "", "");
                if (wp.DiscoverLinuxInfo(ip,  "linuxinfo.xml") != 0)
                {
                    //write proper error messages.
                    Trace.WriteLine(DateTime.Now + "\t Failed to get the discovery info");
                    MessageBox.Show("Failed to fetch the information form the linux server ", "Discovery Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    allServersForm.Enabled = true;
                    allServersForm.nextButton.Enabled = true;
                    return false;

                }
                else
                {
                    SolutionConfig confg = new SolutionConfig();
                    confg.ReadLinuxXmlFile( h);
                    h = SolutionConfig.LinuxHost;
                    h.ip = ip;
                    h.userName = username;
                    h.passWord = password;
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                allServersForm.Enabled = true;
                return false;
            }
            return true;
        }


        private bool PushChecksCredentail(AllServersForm allServersForm, string ip, string domain, string username, string password)
        {
            try
            {
                if (allServersForm.pushIPTextBox.Text.Trim().Length == 0)
                {
                    MessageBox.Show("please enter ip", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return false;
                }
                if (allServersForm.pushUserNAmeTextBox.Text.Trim().Length == 0)
                {
                    //MessageBox.Show("Please enter domain ");
                    // return false;
                }
                if (allServersForm.pushPasswordTextBox.Text.Trim().Length == 0)
                {
                    MessageBox.Show("Please enter user name ", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return false;
                }
                if (allServersForm.pushDomainTextBox.Text.Trim().Length == 0)
                {
                    MessageBox.Show("Please enter password ", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return false;
                }
                string error = "";
                allServersForm.pushGetDeatsilsButton.Enabled = false;
                if (WinPreReqs.WindowsShareEnabled(ip, domain, username, password, "",  error) == false)
                {
                    error = WinPreReqs.GetError;
                    MessageBox.Show(error, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    allServersForm.pushGetDeatsilsButton.Enabled = true;
                    return false;

                }
                allServersForm.pushGetDeatsilsButton.Enabled = true;
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                allServersForm.Enabled = true;
                return false;
            }
            return true;
        }

        private bool GetHostsDetailsandFillTreeView(AllServersForm allServersForm)
        {
            try
            {
                _initialList = new HostList();
                Cxapicalls cxcliCallTogetMachiensList = new Cxapicalls();
                //Trace.WriteLine(DateTime.Now + "\t Printing ostyep value " + allServersForm.p2vOSComboBox.SelectedItem.ToString());

                
                  
                    clusterList = new ArrayList();
                    if (allServersForm.osTypeSelected == OStype.Windows)
                    {
                        cxcliCallTogetMachiensList.PostToGetClusterInfo(ref clusterList);
                    }
               

                Cxapicalls.GetAllHostFromCX(_initialList, WinTools.GetcxIPWithPortNumber(), allServersForm.osTypeSelected);
                _initialList = Cxapicalls.GetHostlist;
                
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                allServersForm.Enabled = true;
                return false;
            }
            return true;
        }


        /* Name:
         * Input:
         * Description:
         * CalledBy:
         * Calls:
         * Returns:
         * 
         * */
        internal bool DisplayDriveDetails(AllServersForm allServersForm, Host h)
        {
            Trace.WriteLine(DateTime.Now + " \t Entered:To display the disk details in of physical machines of " + h.ip);
            try
            {
                if (allServersForm.appNameSelected != AppName.Pushagent)
                {
                    allServersForm.currentDisplayedPhysicalHostIPselected = h.displayname;
                    _selectedHost = h;
                    ArrayList physicalDisks;
                    int i = 0;
                    physicalDisks = h.disks.GetDiskList;
                    allServersForm.physicalMachineDiskSizeDataGridView.Rows.Clear();
                    Trace.WriteLine(DateTime.Now + "\t Count of disks " + h.disks.list.Count.ToString() + " for the machine " + h.displayname);
                    allServersForm.physicalMachineDiskSizeDataGridView.RowCount = h.disks.list.Count;

                    foreach (Hashtable disk in physicalDisks)
                    {
                        
                            allServersForm.physicalMachineDiskSizeDataGridView.Rows[i].Cells[PHYSICAL_MACHINE_DISK_NUMBER_COLUMN].Value = "Disk" + i.ToString();
                            if (disk["Selected"].ToString() == "Yes")
                            {
                                
                        
                                allServersForm.physicalMachineDiskSizeDataGridView.Rows[i].Cells[PHYSICAL_MACHINE_DISK_SELECT_COLUMN].Value = true;
                                Trace.WriteLine(DateTime.Now + "\t Printing the boot value of disk " + disk["IsBootable"].ToString());
                                if (disk["IsBootable"].ToString().ToUpper() == "TRUE")
                                {
                                    allServersForm.physicalMachineDiskSizeDataGridView.Rows[i].Cells[PHYSICAL_MACHINE_DISK_SELECT_COLUMN].ReadOnly = true;
                                    Trace.WriteLine(DateTime.Now + "\t disk is boot disk so making this cell as read only ");
                                }
                                if (allServersForm.osTypeSelected == OStype.Linux && disk["src_devicename"] != null)
                                {
                                    foreach (Hashtable hash in physicalDisks)
                                    {
                                        if (disk["DiskScsiId"] != null && disk["DiskScsiId"].ToString().Length > 1)
                                        {
                                            if (hash["DiskScsiId"] != null && hash["DiskScsiId"].ToString().Length > 1)
                                            {
                                                if (disk["DiskScsiId"].ToString() == hash["DiskScsiId"].ToString() && hash["src_devicename"].ToString() != disk["src_devicename"].ToString())
                                                {
                                                    if (disk["src_devicename"].ToString().Contains("/dev/mapper"))
                                                    {
                                                        allServersForm.physicalMachineDiskSizeDataGridView.Rows[i].Cells[PHYSICAL_MACHINE_DISK_SELECT_COLUMN].Value = true;
                                                        allServersForm.physicalMachineDiskSizeDataGridView.RefreshEdit();
                                                        disk["Selected"] = "Yes";
                                                    }
                                                    else
                                                    {

                                                        if (disk["selectedbyuser"] != null && disk["selectedbyuser"].ToString() == "Yes")
                                                        {
                                                            allServersForm.physicalMachineDiskSizeDataGridView.Rows[i].Cells[PHYSICAL_MACHINE_DISK_SELECT_COLUMN].Value = true;
                                                            allServersForm.physicalMachineDiskSizeDataGridView.RefreshEdit();
                                                            disk["Selected"] = "Yes";
                                                        }
                                                        else
                                                        {
                                                            allServersForm.physicalMachineDiskSizeDataGridView.Rows[i].Cells[PHYSICAL_MACHINE_DISK_SELECT_COLUMN].Value = false;
                                                            allServersForm.physicalMachineDiskSizeDataGridView.RefreshEdit();
                                                            disk["Selected"] = "No";
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    // allServersForm.physicalMachineDiskSizeDataGridView.Rows[i].Cells[PHYSICAL_MACHINE_DISK_SELECT_COLUMN].Value = false;
                                    // disk["Selected"] = "No";
                                }
                            }
                            else
                            {
                                allServersForm.physicalMachineDiskSizeDataGridView.Rows[i].Cells[PHYSICAL_MACHINE_DISK_SELECT_COLUMN].Value = false;
                            }

                            if (allServersForm.osTypeSelected == OStype.Windows)
                            {
                                if (disk["volumelist"] == null && disk["IsBootable"].ToString().ToUpper() == "FALSE")
                                {
                                    Trace.WriteLine(DateTime.Now + "\t Disk is not a bootable disk ");
                                    allServersForm.physicalMachineDiskSizeDataGridView.Rows[i].Cells[PHYSICAL_MACHINE_DISK_SELECT_COLUMN].Value = false;
                                    allServersForm.physicalMachineDiskSizeDataGridView.Rows[i].Cells[PHYSICAL_MACHINE_DISK_SELECT_COLUMN].ReadOnly = true;
                                    disk["Selected"] = "No";
                                }
                                else if(disk["IsBootable"].ToString().ToUpper() == "TRUE")
                                {
                                    allServersForm.physicalMachineDiskSizeDataGridView.Rows[i].Cells[PHYSICAL_MACHINE_DISK_SELECT_COLUMN].ReadOnly = true;

                                }
                                else
                                {
                                    allServersForm.physicalMachineDiskSizeDataGridView.Rows[i].Cells[PHYSICAL_MACHINE_DISK_SELECT_COLUMN].ReadOnly = false;
                                }
                                //if (disk["Size"].ToString() == "0" && h.cluster == "yes" && h.operatingSystem.Contains("2003"))
                                //{
                                //    allServersForm.physicalMachineDiskSizeDataGridView.Rows[i].Cells[PHYSICAL_MACHINE_DISK_SELECT_COLUMN].Value = false;
                                //    allServersForm.physicalMachineDiskSizeDataGridView.Rows[i].Cells[PHYSICAL_MACHINE_DISK_SELECT_COLUMN].ReadOnly = true;
                                //    disk["Selected"] = "No";
                                //}
                            }
                            if (disk["Size"] != null)
                            {
                                float size = float.Parse(disk["Size"].ToString());
                                size = size / (1024 * 1024);
                                //size = int.Parse(String.Format("{0:##.##}", size));
                                size = float.Parse(String.Format("{0:00000.000}", size));
                                allServersForm.physicalMachineDiskSizeDataGridView.Rows[i].Cells[PHYSICAL_MACHINE_DISK_SIZE_COLUMN].Value = size;
                            }
                            allServersForm.physicalMachineDiskSizeDataGridView.Rows[i].Cells[3].Value = "Details";
                            i++;
                            Debug.WriteLine(DateTime.Now + " \t \t Physical Disk name = " + disk["Name"]);
                            Debug.WriteLine(DateTime.Now + " \t \t Physical Disk Size = " + disk["Size"]);
                            Debug.WriteLine(DateTime.Now + " \t \t Physical Disk Selected = " + disk["Selected"]);
                        }

                    allServersForm.totalNumberOfDisksLabel.Text = "Total number of disks: " + h.disks.list.Count.ToString();
                    allServersForm.totalNumberOfDisksLabel.Visible = true;

                    int count = 0;
                    foreach (Hashtable hash in h.disks.list)
                    {
                        if (hash["Selected"].ToString().ToUpper() == "YES")
                        {
                            count++;
                        }
                    }
                    
                    allServersForm.physicalMachineDiskSizeDataGridView.Visible = true;
                    allServersForm.hostDetailsLabel.Visible = true;
                    allServersForm.selectedDiskLabel.Text = "Total number of disks selected: " + count.ToString();
                    allServersForm.selectedDiskLabel.Visible = true;
                    allServersForm.physicalMachineDiskSizeDataGridView.RefreshEdit();
                    Trace.WriteLine(DateTime.Now + " \t Exiting:After displaying the disk details in of physical machines of " + h.ip);
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

        internal bool GetSourceServerDetails(AllServersForm allServersForm)
        {
            try
            {
                Trace.WriteLine(DateTime.Now + " \t Entered: GetSourceServerDetails() of P2V_PrimaryServerPanelHandler.cs ");
                Host selectedHost = new Host();
                _tempHostList = allServersForm.selectedSourceListByUser;
                GeneratePhysicalMachineConfigurations(ref allServersForm.selectedSourceListByUser);
                //allServersForm._selectedSourceList.Print();
                //allServersForm._selectedSourceList = (HostList)_tempHostList.Clone();
                Host h = (Host)allServersForm.selectedSourceListByUser._hostList[0];
                DisplayDriveDetails(allServersForm, h);
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
            Trace.WriteLine(DateTime.Now + " \t Exiting: GetSourceServerDetails() of P2V_PrimaryServerPanelHandler.cs ");
            return true;
        }



        internal static bool RearrangeBootableDisk(ref Host h)
        {
            try
            {
                ArrayList diskList = new ArrayList();
                Hashtable diskHash = new Hashtable();
                ArrayList physicalDisks;
                
                physicalDisks = h.disks.GetDiskList;
                foreach (Hashtable disk in physicalDisks)
                {
                    //MessageBox.Show(disk["IsBootable"].ToString());
                    if (disk["Selected"].ToString() == "Yes")
                    {
                        if (disk["IsBootable"].ToString().ToUpper() == "TRUE")
                        {
                            Trace.WriteLine(DateTime.Now + "\t Found bootable disk at " + disk["Name"].ToString());
                            //h.disks.list.Add(disk);
                            diskList.Add(disk);
                        }
                    }
                }
                foreach (Hashtable disk in physicalDisks)
                {
                    //MessageBox.Show(disk["IsBootable"].ToString());
                    if (disk["Selected"].ToString() == "Yes")
                    {
                        if (disk["IsBootable"].ToString().ToUpper() == "FALSE")
                        {
                            Trace.WriteLine(DateTime.Now + "\t Not bootable disk at " + disk["Name"].ToString());
                            diskList.Add(disk);
                            //h.disks.list.Add(disk);
                        }
                    }
                }
                foreach (Hashtable disk in physicalDisks)
                {
                    //MessageBox.Show(disk["IsBootable"].ToString());
                    if (disk["Selected"].ToString() == "No")
                    {
                        if (disk["IsBootable"].ToString().ToUpper() == "FALSE")
                        {
                            Trace.WriteLine(DateTime.Now + "\t Not bootable disk at " + disk["Name"].ToString());
                            Trace.WriteLine(DateTime.Now + "\t Not selected for protection " + disk["Name"].ToString());
                            diskList.Add(disk);
                            //h.disks.list.Add(disk);
                        }
                    }
                }
                h.disks.list = diskList;
            }
            catch (Exception err)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(err, true);
                System.FormatException formatException = new FormatException("Inner exception", err);
                Trace.WriteLine(formatException.GetBaseException());
                Trace.WriteLine(formatException.Data);
                Trace.WriteLine(formatException.InnerException);
                Trace.WriteLine(formatException.Message);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: RearrangeBootableDisk " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + err.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
           // h.disks.list.Add(diskList);
            return true;
        }


        // These are the local  variable for masking the check box for the parent node...
        private const int TREEVIEW_STATE = 0x8;
        private const int TREEVIEW_MASKFORCHECKBOX = 0xF000;
        private const int TREEVIEW_FIRST = 0x1100;
        private const int TREEVIEW_SETITEM = TREEVIEW_FIRST + 63;

        [StructLayout(LayoutKind.Sequential, Pack = 8, CharSet = CharSet.Auto)]
        private struct TREEVIEWITEM
        {
            internal int mask;
            internal IntPtr hideItem;
            internal int state;
            internal int stateMask;
            [MarshalAs(UnmanagedType.LPTStr)]
            internal string nodeText;
            internal int cacheText;
            internal int indexImage;
            internal int indexSelectedImage;
            internal int checkChildren;
            internal IntPtr lineParam;
        }

        [DllImport("user32.dll", CharSet = CharSet.Auto)]
        private static extern IntPtr SendMessage(IntPtr hideWindow, int Message, IntPtr withParam,
                                                 ref TREEVIEWITEM lineParam);

        /// <summary>
        /// Hides the checkbox for the specified node on a TreeView control.
        /// </summary>
        internal void HideCheckBoxFromTreeView(TreeView treeview, TreeNode node)
        {
            try
            {
                TREEVIEWITEM treeView = new TREEVIEWITEM();
                treeView.hideItem = node.Handle;
                treeView.mask = TREEVIEW_STATE;
                treeView.stateMask = TREEVIEW_MASKFORCHECKBOX;
                treeView.state = 0;
                // Trace.WriteLine(DateTime.Now + " \t Printing the node text  " + node.Text);
                SendMessage(treeview.Handle, TREEVIEW_SETITEM, IntPtr.Zero, ref treeView);
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: HideCheckBox  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");


            }
        }

        internal bool UnslectMachine(AllServersForm allServersForm, string hostname)
        {
            try
            {
                Host h = new Host();
                h.hostname = hostname;
                int index = 0;
                if (allServersForm.selectedSourceListByUser.DoesHostExist(h, ref index) == true)
                {
                    h = (Host)allServersForm.selectedSourceListByUser._hostList[index];
                    allServersForm.selectedSourceListByUser.RemoveHost(h);
                    allServersForm.selectedDiskLabel.Visible = false;
                    allServersForm.totalNumberOfDisksLabel.Visible = false;
                    Trace.WriteLine(DateTime.Now + "\t Successfully removed host form the _selectedSourceList");
                    Trace.WriteLine(DateTime.Now + "\t Removed host name is " + h.hostname);
                }
                else
                {
                    Trace.WriteLine(DateTime.Now + "\t Host does not exist in _selectedSourceList in case of p2v " + h.hostname);
                }
           
                
            }
            catch (Exception err)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(err, true);
                System.FormatException formatException = new FormatException("Inner exception", err);
                Trace.WriteLine(formatException.GetBaseException());
                Trace.WriteLine(formatException.Data);
                Trace.WriteLine(formatException.InnerException);
                Trace.WriteLine(formatException.Message);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: UnslectMachine P2V PrimaryPanelHandler " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + err.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            return true;
        }

        internal bool AddexistingMachinetoWmiBasedProtection(AllServersForm allServersForm, string hostname)
        {
            try
            {
                Host h = new Host();
                h.hostname = hostname;
                int index = 0;
                if (_initialList.DoesHostExist(h, ref index) == true)
                {
                    h = (Host)_initialList._hostList[index];
                    allServersForm.selectedSourceListByUser.AddOrReplaceHost(h);
                    Trace.WriteLine(DateTime.Now + "\t Successfully add this machine for protection " + hostname);
                    return true;
                }
                else
                {
                    Trace.WriteLine(DateTime.Now + "\t Failed to find this machine in existing list " + hostname);
                    return false;
                }
            }
            catch (Exception err)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(err, true);
                System.FormatException formatException = new FormatException("Inner exception", err);
                Trace.WriteLine(formatException.GetBaseException());
                Trace.WriteLine(formatException.Data);
                Trace.WriteLine(formatException.InnerException);
                Trace.WriteLine(formatException.Message);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: UnslectMachine P2V PrimaryPanelHandler " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + err.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            
        }

        internal bool GetResponceFromCXAPI(AllServersForm allServerForm, string hostname)
        {
            try
            {
                Host h = new Host();
                h.hostname = hostname;
                int index = 0;
                if (_initialList.DoesHostExist(h, ref index) == true)
                {

                    h = (Host)_initialList._hostList[index];

                    string ip = h.ip;
                    Cxapicalls cxApi = new Cxapicalls();
                    string cxIP = WinTools.GetcxIPWithPortNumber();
                    h.os = allServerForm.osTypeSelected;
                    if (cxApi.Post( h, "GetHostInfo") == true)
                    {

                        h = Cxapicalls.GetHost;
                        h.role = Role.Primary;
                        h.ip = ip;
                        h.displayname = h.hostname;
                       
                        if (h.machineType == null)
                        {
                            MessageBox.Show("Selected machine info is not found in CX database", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            return false;
                        }

                        //if (h.operatingSystem.ToUpper().Contains("UBUNTU"))
                        //{
                        //    MessageBox.Show("You have selected Ubuntu server. Ubuntu is not supported", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        //    return false;
                        //}
                        if (allServerForm.appNameSelected != AppName.V2P)
                        {
                            if (h.machineType.ToUpper() == "VIRTUALMACHINE")
                            {
                                MessageBox.Show("You have selected virtual machine as physical machine ", "Information", MessageBoxButtons.OK, MessageBoxIcon.Information);
                            }
                            h.machineType = "PhysicalMachine";
                            _selectedHost = h;
                           
                            allServerForm.selectedSourceListByUser.AddOrReplaceHost(h);
                            allServerForm.currentDisplayedPhysicalHostIPselected = h.displayname;
                        }
                        else
                        {
                           
                           
                            allServerForm.selectedMasterListbyUser = new HostList();
                            allServerForm.selectedMasterListbyUser.AddOrReplaceHost(h);
                        }
                        
                       
                        
                    }
                    else
                    {
                        Trace.WriteLine(DateTime.Now + "\t Failed to get the responce form the server");
                        return false;
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


        // While adding background worker we can't fill all the values in dowork so calling this method in runworker completed.
        internal bool DisplayAllDetails(AllServersForm allServerForm)
        {
            Host h = _selectedHost;
            DisplayDriveDetails(allServerForm, h);
           
            allServerForm.p2vIPValueLabel.Text = h.ip;
            allServerForm.p2vHostnameValueLabel.Text = h.hostname;
            allServerForm.p2vOSValueLabel.Text = h.operatingSystem;
            allServerForm.p2vRamValueLabel.Text = h.memSize.ToString() + " (MB)";
            allServerForm.p2vCpuValueLabel.Text = h.cpuCount.ToString();
            allServerForm.p2vPanel.Visible = true;
            allServerForm.machineTypeValueLabel.Text = h.machineType;
            if (h.cluster == "yes")
            {
                allServerForm.p2vClusterValueLabel.Text = "Yes";
            }
            else
            {
                allServerForm.p2vClusterValueLabel.Text = "No";
            }
            allServerForm.nextButton.Enabled = true;
            return true;
        }

        internal bool DisplayOSDetails(AllServersForm allServersForm)
        {
            try
            {
                toolTipforOSDisplay = new System.Windows.Forms.ToolTip();
                toolTipforOSDisplay.AutoPopDelay = 5000;
                toolTipforOSDisplay.InitialDelay = 100;
                toolTipforOSDisplay.ReshowDelay = 500;
                toolTipforOSDisplay.ShowAlways = true;
                toolTipforOSDisplay.GetLifetimeService();
                toolTipforOSDisplay.IsBalloon = true;

                toolTipforOSDisplay.UseAnimation = true;
                toolTipforOSDisplay.SetToolTip(allServersForm.p2vOSValueLabel, _selectedHost.operatingSystem );
                return true;
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
                return false;
            }
            
        }


        internal bool UnDisplayOSDetails(AllServersForm allServersForm)
        {
            try
            {
                toolTipforOSDisplay.Dispose();
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
        internal bool GetDetailsUsingBackGroundWorker(AllServersForm allServersForm)
        {
            if (GetHostsDetailsandFillTreeView(allServersForm) == true)
            {
                Trace.WriteLine(DateTime.Now + "\t Successfully got hte details for the cx and laod the tree view");
                return true;
            }
            else
            {
                Trace.WriteLine(DateTime.Now + "\t Failed to get machines from cx");
                
                return false;
            }
           
        }

        internal bool LoadTreeViewAfterGettingDetails(AllServersForm allServersForm)
        {
            try
            {// making note label as visible..... V2P Case
                if (allServersForm.appNameSelected == AppName.V2P)
                {
                    allServersForm.vepNoteLabel.Visible = true;
                }


                if (clusterList.Count != 0)
                {
                    foreach (Hashtable hash in clusterList)
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
                                int index =0;
                                string ip = null;
                                if (_initialList.DoesHostExist(h, ref index) == true)
                                {
                                    Host tempHost = (Host)_initialList._hostList[index];
                                     ip = tempHost.ip;
                                }
                                if (ip != null)
                                {
                                    h.ip = ip;
                                }
                                _initialList.AddOrReplaceHost(h);
                                i++;
                            }
                        }
                    }
                }


                if (_initialList._hostList.Count != 0)
                {
                    allServersForm.p2VPrimaryTreeView.Nodes.Clear();
                    string cxip = WinTools.GetcxIP();
                    allServersForm.p2VPrimaryTreeView.Nodes.Add(cxip);
                    foreach (Host h in _initialList._hostList)
                    {
                        if (h.masterTarget == false || allServersForm.appNameSelected == AppName.V2P)
                        {
                            allServersForm.p2VPrimaryTreeView.Nodes[0].Nodes.Add(h.hostname);
                            allServersForm.p2VPrimaryTreeView.Visible = true;
                        }                        

                    }

                    foreach (Host h in _initialList._hostList)
                    {
                        if (h.cluster == "yes")
                        {
                            if (h.clusterName != null)
                            {
                                bool esxExists = false;
                                int nodeCount = 0;
                                foreach (TreeNode node in allServersForm.p2VPrimaryTreeView.Nodes)
                                {
                                    nodeCount++;
                                    if (node.Text == "MSCS("+h.clusterName + ")")
                                    {
                                        esxExists = true;
                                        node.Nodes.Add(h.hostname);
                                        allServersForm.p2VPrimaryTreeView.Refresh();
                                    }
                                }
                                if (esxExists == false)
                                {
                                    allServersForm.p2VPrimaryTreeView.Nodes.Add("MSCS(" + h.clusterName + ")");
                                    int count = allServersForm.p2VPrimaryTreeView.Nodes.Count;
                                    allServersForm.p2VPrimaryTreeView.Nodes[count - 1].Nodes.Add(h.hostname);
                                    allServersForm.p2VPrimaryTreeView.Refresh();
                                }
                            }
                        }
                    }

                    allServersForm.p2VPrimaryTreeView.ExpandAll();
                    HideCheckBoxFromTreeView(allServersForm.p2VPrimaryTreeView, allServersForm.p2VPrimaryTreeView.Nodes[0]);
                }
                else
                {
                    MessageBox.Show("There are no machines found in CX.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
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
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: LoadTreeViewAfterGettingDetails " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            return true;
        }
    }
}

