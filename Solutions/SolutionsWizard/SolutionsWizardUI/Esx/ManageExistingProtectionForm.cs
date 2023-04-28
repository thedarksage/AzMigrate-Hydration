using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using Com.Inmage;
using System.IO;
using System.Windows.Forms;
using System.Collections;
using Com.Inmage.Esxcalls;
using Com.Inmage.Tools;
using System.Threading;
using System.Management;
using System.Xml;
using System.Runtime.InteropServices;
using System.Runtime.Serialization;
using System.Xml.Schema;


namespace Com.Inmage.Wizard
{
    internal partial class ResumeForm : Form
    { 
        internal static long TimeOutMilliseconds = 3600000;
        internal static string SampleMasterConfigXSDFile = "SampleMasterConfigFile.xsd";
        internal System.Drawing.Bitmap currentTask;
        internal static HostList sourceHostsList = new HostList();
        HostList mastertargetsHostList = new HostList();
        string esxIPinMain = null;
        string cxIPinMain = null;
        internal static HostList selectedVMListForPlan = new HostList();
        internal static HostList masterTargetList = new HostList();
        internal HostList p2vResumeList = new HostList();
        Host _masterHost = new Host();
        string masterConfigFile = "MasterConfigFile.xml";
        internal string _planName = null;
        internal bool checkingForVmsPowerStatus = false;
        internal bool createGuestOnTarget = false;
        internal bool readinessCheck = false;
        internal bool jobautomation = false;
        internal bool esxutilwinExecution = false;
        internal bool upLoadFile = false;
        internal bool upDateMasterFile = false;
        internal bool v2pRefesh = false;
        internal string _directoryNameinXmlFile = null;
        internal static Esx _esxInfo = new Esx();
        private delegate void TickerDelegate(string s);
        internal static string selectedActionForPlan = null;
        TickerDelegate tickerDelegate;       
        TextBox _textBox = new TextBox();
        internal static string messages = null;
        string nodesList = null;
        int returnCodeofDetach = 10;
        internal bool dummyDiskscreated = false;
        internal string applicationName = "Monitor";
        string Masterfile = null;
        internal bool passedAdditionOfDisk = false;
        internal static string ProtectionEsx = "1";
        internal static string Recover = "2";
        internal static string Failback = "3";
        internal static string Offlinesync = "4";
        internal static string Remove = "5";
        internal static string Drdrill = "8";
        internal static string Adddisks = "6";
        internal static string ProtectionP2v = "10";
        internal static string ResumeProtection = "7";
        internal static string Administrativetasks = "9";
        internal static string Pushagent = "12";
        internal static string Upgrade = "11";
        internal bool xmlFileMadeSuccessfully = false;
        internal static string LinuxScript = "linux_script.sh";
        private static Mutex mutex = new Mutex();
         FileInfo f1 = new FileInfo(WinTools.FxAgentPath() +"\\vContinuum\\logs\\vContinuum_brief.log");
         internal static StreamWriter breifLog;
         internal string fxInstallPath = null;
         internal string latestFilePath = null;
         internal string installPath = null;
         internal bool getplans = false;
         internal string currentDisplayedPhysicalHostIp = null;
         internal string planSelected = null;
         internal System.Drawing.Bitmap scoutHeading;
         internal int nErrors = 0;
         internal string strErrorMsg = null;
        public ResumeForm()
        {
            InitializeComponent();

            scoutHeading = Wizard.Properties.Resources.scoutheading;
            if (HelpForcx.Brandingcode == 1)
            {
                headingPanel.BackgroundImage = scoutHeading;
            }
          
            latestFilePath = WinTools.FxAgentPath() + "\\vContinuum\\Latest";
            installPath = WinTools.FxAgentPath() + "\\vContinuum";
            breifLog = f1.AppendText();
            breifLog.WriteLine(DateTime.Now + " \t Launched the Vontinuum");
            breifLog.Flush();
           //Using tool tip for the version label thing to show the date...
            System.Windows.Forms.ToolTip toolTipToDisplayPatchVersion = new System.Windows.Forms.ToolTip();
            toolTipToDisplayPatchVersion.AutoPopDelay = 5000;
            toolTipToDisplayPatchVersion.InitialDelay = 1000;
            toolTipToDisplayPatchVersion.ReshowDelay = 500;
            toolTipToDisplayPatchVersion.ShowAlways = true;
            toolTipToDisplayPatchVersion.GetLifetimeService();
            toolTipToDisplayPatchVersion.IsBalloon = false;
            toolTipToDisplayPatchVersion.UseAnimation = true;            
            toolTipToDisplayPatchVersion.SetToolTip(versionLabel, HelpForcx.BuildDate);
            versionLabel.Text = HelpForcx.VersionNumber;
            if (File.Exists( installPath+ "\\patch.log"))
            {
                patchLabel.Visible = true;
            }

            //Getting the current directory path....
           
            fxInstallPath = WinTools.FxAgentPath() + "\\Failover\\Data";
            chooseApplicationComboBox.Items.Add("ESX");
            chooseApplicationComboBox.Items.Add("P2V");
            chooseApplicationComboBox.SelectedItem = "ESX";
            _textBox = logTextBox;
            if (WinTools.EnableEditMasterConfigFile() == true)
            {
                editMasterConfigfileRadioButton.Visible = true;
            }
            else
            {
                editMasterConfigfileRadioButton.Visible = false;
            }

            cxIPTextBox.Text = WinTools.GetcxIP();
            portNumberTextBox.Text = WinTools.GetcxPortNumber();
            currentTask = Wizard.Properties.Resources.arrow;
            cancelButton.Visible = true;
            cancelButton.Text = "Close";
            offlineSyncExportRadioButton.Checked = false;
            newProtectionRadioButton.Checked = false;
            pushAgentRadioButton.Checked = false;
            manageExistingPlanRadioButton.Checked = false;
            _directoryNameinXmlFile = null;

            planNamesTreeView.Nodes.Clear();
            getPlansButton.Enabled = false;
            resumeButton.Enabled = false;
            cancelButton.Enabled = false;
            progressBar.Visible = true;
            mainFormPanel.BringToFront();
            VMDeatilsPanel.SendToBack();
            taskGroupBox.Enabled = false;
            
            
           
            getPlansBackgroundWorker.RunWorkerAsync();
        }

        private void planNamesTreeView_AfterSelect(object sender, TreeViewEventArgs e)
        {
            try
            {
                //Displaying the details of each machine for the when selecting the machine....
                if (e.Node.IsSelected)
                {

                    int index = 0;
                    Host h = new Host();
                    h.displayname = e.Node.Text;
                    if (e.Node.Parent != null)
                    {
                        h.plan = e.Node.Parent.Text;
                    }
                    if (sourceHostsList.DoesHostExistInPlanBased(h, ref index) == true)
                    {
                        if (removeDiskRadioButton.Checked == true || removeVolumeRadioButton.Checked == true)
                        {
                            currentDisplayedPhysicalHostIp = h.displayname;
                            planSelected = h.plan;
                        }

                        h = (Host)sourceHostsList._hostList[index];
                        Host masterHost = new Host();
                        masterHost.hostname = h.masterTargetHostName;
                        masterHost.displayname = h.masterTargetDisplayName;
                        int masterIndex = 0;
                        Trace.WriteLine(DateTime.Now + " printing the countof master list " + masterTargetList._hostList.Count.ToString());
                        Trace.WriteLine(DateTime.Now + "\t Printing the count of the MasterHostList " + mastertargetsHostList._hostList.Count.ToString());
                        if (mastertargetsHostList.DoesHostExist(masterHost, ref masterIndex) == true)
                        {
                            masterHost = (Host)mastertargetsHostList._hostList[masterIndex];
                            masterTargetNameTextBox.Text = masterHost.displayname;
                            mtOSTextBox.Text = masterHost.operatingSystem;
                            if (h.new_displayname != null)
                            {
                                newDisplayNameTextBox.Text = h.new_displayname;
                            }
                            else
                            {
                                newDisplayNameTextBox.Text = h.displayname;
                                h.new_displayname = h.displayname;

                            }
                            sourceDisplayNameTextBox.Text = h.displayname;
                            sourceIPAddressTextBox.Text = h.ip;
                            targetESXIPAddressTextBox.Text = h.targetESXIp;
                            masterTargetDisplayNameTextBox.Text = masterHost.displayname;
                            masterTargetIPTextBox.Text = masterHost.ip;
                            masterTargetIPTextBox.Text = masterHost.ip;
                            if (masterHost.vCenterProtection == "Yes")
                            {
                                vcenterProtectionTextBox.Text = masterHost.esxIp;
                                targetESXTextBox.Text = masterHost.vSpherehost;
                            }
                            else
                            {
                                vcenterProtectionTextBox.Text = "N/A";
                                targetESXTextBox.Text = masterHost.esxIp;
                            }
                        }
                        else
                        {
                            Trace.WriteLine(DateTime.Now + "\t host does not exist in the master list ");
                            mastertargetsHostList.Print();
                            Trace.WriteLine(DateTime.Now + "\t printing the host values");
                            masterHost.Print();
                        }
                        sourceDetailsGroupBox.Visible = true;
                        sourceVMDiskDetailsgroupBox.Visible = true;
                        targetDetailsGroupBox.Visible = true;
                        if (removeDiskRadioButton.Checked == true || removeVolumeRadioButton.Checked == true)
                        {
                            removeDiskPanel.BringToFront();
                            if (removeDiskRadioButton.Checked == true)
                            {
                                mandatoryLabel.Text = "( * ) Select atleast one disk to remove";
                            }
                            else
                            {
                                mandatoryLabel.Text = "( * ) Select atleast one volume to remove";
                            }
                        }
                        else
                        {
                            VMDeatilsPanel.BringToFront();
                        }

                        if (h.machineType == "VirtualMachine")
                        {
                            sourceESXTextBox.Text = h.esxIp;
                        }
                        else
                        {
                            sourceESXTextBox.Text = "N/A";
                        }
                        int hostIndex = 0;
                        if (selectedVMListForPlan.DoesHostExist(h, ref hostIndex) == true)
                        {
                            h = (Host)selectedVMListForPlan._hostList[hostIndex];
                            Trace.WriteLine(DateTime.Now + "\t Found the host in selected vm list");
                          
                           
                        }

                        if (removeVolumeRadioButton.Checked == true)
                        {
                            FillDataGridWithVolumes(h, masterHost);
                            removeDiskPanel.BringToFront();
                           
                        }
                        else
                        {
                            EsxDiskDetails(h);
                            
                        }
                         hostIndex = 0;
                         if (selectedVMListForPlan.DoesHostExist(h, ref hostIndex) == true)
                         {
                             
                             removeDisksDataGridView.Enabled = true;
                         }
                         else
                         {
                             
                             removeDisksDataGridView.Enabled = false;
                         }
                        
                        sourceIPTextBox.Text = h.ip;
                        sourceHostNameTextBox.Text = h.hostname;
                        osInfoTextBox.Text = h.operatingSystem;
                        processServerTextBox.Text = h.processServerIp;
                        machineTypeTextBox.Text = h.machineType;
                        //targetESXTextBox.Text = h.targetESXIp;
                        masterTargetNameTextBox.Text = h.masterTargetDisplayName;
                        if (h.failback == "yes")
                        {
                            machineStausTextBox.Text = "Failback Protected";
                        }
                        else if (h.failOver == "no")
                        {
                            machineStausTextBox.Text = "Protected";
                        }
                        else if (h.failOver == "yes")
                        {
                            machineStausTextBox.Text = "Recovered";
                        }
                        
                    }
                    else
                    {
                        sourceDetailsGroupBox.Visible = false;
                        sourceVMDiskDetailsgroupBox.Visible = false;
                        targetDetailsGroupBox.Visible = false;
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
        }


        private bool FillDataGridWithVolumes(Host host, Host masterHost)
        {

            
            if (host.disks.partitionList.Count != 0)
            {
                Trace.WriteLine(DateTime.Now + "\t Found the host in selected vm list");
                removeDiskPanel.BringToFront();
                removeDisksDataGridView.Rows.Clear();
                removeDisksDataGridView.Columns[3].Visible = false;
                int i = 0;
                removeDisksDataGridView.Columns[0].HeaderText = "Source Volume";
                removeDisksDataGridView.Columns[1].HeaderText = "Target Volume";
                foreach (Hashtable hash in host.disks.partitionList)
                {
                    removeDisksDataGridView.Rows.Add(1);
                    if (hash["SourceVolume"] != null && hash["TargetVolume"] != null)
                    {
                        removeDisksDataGridView.Rows[i].Cells[0].Value = hash["SourceVolume"].ToString();
                        removeDisksDataGridView.Rows[i].Cells[1].Value = hash["TargetVolume"].ToString();
                        if (hash["remove"].ToString() == "True")
                        {
                            removeDisksDataGridView.Rows[i].Cells[2].Value = true;
                        }
                        //removeDisksDataGridView.Rows[i].Cells[3].Value = "N/A";
                        i++;
                    }
                }
                removeDisksDataGridView.Columns[3].Visible = false;
            }
            else
            {

                Trace.WriteLine(DateTime.Now + "\t Not found the host in selected vm list");
                Cxapicalls cxApi = new Cxapicalls();
                cxApi.PostToGetPairsOFSelectedMachine( host, masterHost.inmage_hostid);
                host = Cxapicalls.GetHost;
                removeDiskPanel.BringToFront();
                removeDisksDataGridView.Rows.Clear();
                removeDisksDataGridView.Columns[3].Visible = false;
                int i = 0;
                removeDisksDataGridView.Columns[0].HeaderText = "Source Volume";
                removeDisksDataGridView.Columns[1].HeaderText = "Target Volume";
                foreach (Hashtable hash in host.disks.partitionList)
                {
                    removeDisksDataGridView.Rows.Add(1);
                    if (hash["SourceVolume"] != null && hash["TargetVolume"] != null)
                    {
                        removeDisksDataGridView.Rows[i].Cells[0].Value = hash["SourceVolume"].ToString();
                        removeDisksDataGridView.Rows[i].Cells[1].Value = hash["TargetVolume"].ToString();
                        i++;
                    }
                }
            }

            return true;
        }

        private void planNamesTreeView_AfterCheck(object sender, TreeViewEventArgs e)
        {
            try
            {
                //Selecting the machine when user select the check box......
                xmlFileMadeSuccessfully = false;

                readinessCheck = false;
                checkingForVmsPowerStatus = false;
                dummyDiskscreated = false;
                createGuestOnTarget = false;
                jobautomation = false;
                upDateMasterFile = false;
                upLoadFile = false;
                Host h = new Host();
                Host masterHost = new Host();
                int index = 0;
                int nodeIndex = e.Node.Index;
                //In caseof Offline sync import user as to select entire plan for the Import.
                //So we added this error check for the import case.
                if (offLineSyncImportRadioButton.Checked == true)
                {
                    if (e.Node.Nodes.Count == 0)
                    {
                        if (e.Node.Parent.Checked == false)
                        {
                            if (e.Node.Parent.Nodes.Count != 1)
                            {
                                if (e.Node.Checked == true)
                                {
                                    MessageBox.Show("This is offline sync import. Please select entire plan", "Error", MessageBoxButtons.OK, MessageBoxIcon.Information);
                                    e.Node.Checked = false;
                                    return;
                                }
                            }
                        }
                        if (e.Node.Checked == false)
                        {
                            if (e.Node.Nodes.Count == 0)
                            {
                               
                                    foreach (TreeNode node in planNamesTreeView.Nodes)
                                    {
                                        node.Checked = false;
                                    }
                                }
                            }
                        }
                    }


                if (failBackRadioButton.Checked == true || recoverRadioButton.Checked == true || drDrillRadioButton.Checked == true || addDiskRadioButton.Checked == true || volumeReSizeRadioButton.Checked == true || removeDiskRadioButton.Checked == true || removeVolumeRadioButton.Checked == true || removeRadioButton.Checked == true || resumeProtectionRadioButton.Checked == true)
                {

                    if (failBackRadioButton.Checked == true && selectedActionForPlan == "V2P")
                    {
                        Trace.WriteLine(DateTime.Now + "\t User selected for Failback of physical machine ");
                    }
                    else
                    {
                        if (e.Node.Nodes.Count == 0)
                        {
                            if (e.Node.Checked == true)
                            {                               

                                h = new Host();
                                h.displayname = e.Node.Text;
                                h.plan = e.Node.Parent.Text;
                                TreeNode parentNode = e.Node.Parent;
                                int indexes = 0;
                                if (sourceHostsList.DoesHostExist(h, ref indexes) == true)
                                {
                                    h = (Host)sourceHostsList._hostList[indexes];
                                    if (h.cluster == "yes")
                                    {
                                       // e.Node.Parent.Checked = true;
                                        int indexcount = 0;
                                        foreach (TreeNode node in e.Node.Parent.Nodes)
                                        {
                                            Trace.WriteLine(DateTime.Now + "\t Count " + indexcount.ToString());
                                            indexcount++;
                                            Host h1 = new Host();
                                            h1.displayname = node.Text;
                                            h1.plan = e.Node.Parent.Text;
                                            if (sourceHostsList.DoesHostExist(h1, ref indexes) == true)
                                            {
                                                h1 = (Host)sourceHostsList._hostList[indexes];
                                                if (h1.cluster == "yes" && h.clusterName == h1.clusterName)
                                                {
                                                    if (SelectedNode(h) == false)
                                                    {

                                                    }
                                                    else
                                                    {
                                                        if (node.Checked == false)
                                                        {
                                                            node.Checked = true;

                                                        }

                                                       // node.Checked = true;
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
                if (selectedVMListForPlan._hostList.Count == 0)
                {


                    Trace.WriteLine(DateTime.Now + "\t Came here because the selected vm list is zero");
                    masterTargetList = new HostList();
                    if (e.Node.Checked == true)
                    {
                        if (e.Node.Nodes.Count != 0)
                        {
                            _planName = e.Node.Text;

                            foreach (TreeNode node in e.Node.Nodes)
                            {
                                h = new Host();
                                h.plan = e.Node.Text;

                                Trace.WriteLine(DateTime.Now + " \t printing the node name " + node.Text);
                                h.displayname = node.Text;
                                
                                    if (SelectedNode(h) == false)
                                    {
                                        node.Checked = false;
                                        if (drDrillRadioButton.Checked == true)
                                        {
                                            return;
                                        }
                                    }
                                    else
                                    {
                                        node.Checked = true;
                                    }
                               
                                planNamesTreeView.SelectedNode = node;
                            }
                        }
                        else
                        {
                            h.displayname = e.Node.Text;
                            try
                            {
                                _planName = e.Node.Parent.Text;
                                h.plan = e.Node.Parent.Text;
                            }
                            catch (Exception ex)
                            {
                                Trace.WriteLine(DateTime.Now + "\t there are no vms to resumes");
                                // MessageBox.Show("There are no vms to resume", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                //return ;
                            }

                            if (SelectedNode(h) == false)
                            {
                                e.Node.Checked = false;
                                if (drDrillRadioButton.Checked == true)
                                {
                                    return;
                                }
                            }
                            planNamesTreeView.SelectedNode = e.Node;
                        }

                    }
                    else
                    {
                        //if (Console._hostList.Count == 0)
                        //{
                        //    try
                        //    {
                        //        if (e.Node.Nodes.Count != 0)
                        //        {
                        //            e.Node.Parent.Checked = false;
                        //        }
                        //    }
                        //    catch (Exception ex)
                        //    {
                        //        Trace.WriteLine(DateTime.Now + "\t There is no parent node for the existing node " + e.Node.Text + ex.ToString());
                        //    }
                        //}

                    }

                }
                else
                {
                    if (e.Node.Checked == false)
                    {
                        if (e.Node.Nodes.Count != 0)
                        {
                            
                            foreach (TreeNode node in e.Node.Nodes)
                            {
                                
                                h.displayname = node.Text;
                                h.plan = node.Parent.Text;
                                if (sourceHostsList.DoesHostExistInPlanBased(h, ref index) == true)
                                {
                                    //h = (Host)sourceHostsList._hostList[index];

                                    selectedVMListForPlan.RemoveHost(h);
                                    if (selectedVMListForPlan._hostList.Count == 0)
                                    {
                                        try
                                        {
                                            if (e.Node.Nodes.Count != 0)
                                            {
                                                if (e.Node.Parent.Checked == true)
                                                {
                                                    e.Node.Parent.Checked = false;
                                                    break;
                                                }
                                            }
                                        }
                                        catch (Exception ex)
                                        {
                                            Trace.WriteLine(DateTime.Now + "\t There is no parent node for the existing node " + e.Node.Text);
                                        }
                                    }
                                    Trace.WriteLine(DateTime.Now + "\t printing the count of the list " + selectedVMListForPlan._hostList.Count.ToString() + " " + mastertargetsHostList._hostList.Count.ToString());
                                }
                                node.Checked = false;
                            }
                        }
                        else
                        {


                           
                        }
                        if (e.Node.Nodes.Count == 0)
                        {
                            h.displayname = e.Node.Text;
                            h.plan = e.Node.Parent.Text;
                            if (sourceHostsList.DoesHostExistInPlanBased(h, ref index) == true)
                            {
                                h = (Host)sourceHostsList._hostList[index];
                                //if (e.Node.Parent.Checked == true && h.cluster == "yes" )
                                //{
                                //    MessageBox.Show("For cluster protections you need to select all machines for Recovery\\Resume\\DR-Drill\\Add disk.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                //    e.Node.Checked = true;

                                //}
                                //else
                                //{

                                selectedVMListForPlan.RemoveHost(h);
                                //}
                                Trace.WriteLine(DateTime.Now + "\t printing the count of the list " + sourceHostsList._hostList.Count.ToString() + " " + mastertargetsHostList._hostList.Count.ToString());
                            }
                        }
                        if (selectedVMListForPlan._hostList.Count == 0)
                        {
                            e.Node.Checked = false;
                            _planName = null;
                        }
                    }
                    if (e.Node.Checked == true)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Printing the plan name " + _planName);
                        if (e.Node.Nodes.Count != 0)
                        {

                            if (recoverRadioButton.Checked == false)
                            {
                                if (_planName != null && _planName .Length > 0)
                                {
                                    if (_planName != e.Node.Text)
                                    {

                                        e.Node.Checked = false;
                                        MessageBox.Show("Machines should be selected only from one plan at a time", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                        return;

                                    }
                                }
                            }
                            foreach (TreeNode node in e.Node.Nodes)
                            {
                                h = new Host();


                                Trace.WriteLine(DateTime.Now + " \t printing the node name " + node.Text);
                                h.displayname = node.Text;
                                h.plan = node.Parent.Text;
                                if (recoverRadioButton.Checked == true)
                                {


                                    int indexes = 0;
                                    if (sourceHostsList.DoesHostExist(h, ref indexes) == true)
                                    {
                                        Host tempHost = (Host)sourceHostsList._hostList[indexes];
                                        Host selectedHost = (Host)sourceHostsList._hostList[0];
                                        
                                        if (selectedHost.masterTargetDisplayName == tempHost.masterTargetDisplayName || selectedHost.masterTargetHostName == tempHost.masterTargetHostName || selectedHost.mt_uuid == tempHost.mt_uuid)
                                        {
                                            h.displayname = node.Text;
                                            h.plan = node.Parent.Text;


                                            if (SelectedNode(h) == false)
                                            {
                                                node.Checked = false;
                                            }
                                            planNamesTreeView.SelectedNode = e.Node;
                                        }
                                        else
                                        {
                                            node.Checked = false;
                                            try
                                            {
                                                if (node.Parent.Checked == true)
                                                {
                                                    node.Parent.Checked = false;
                                                }
                                            }
                                            catch (Exception ex)
                                            {
                                                Trace.WriteLine(DateTime.Now + "\t Failed to unselect parent " + ex.Message);
                                            }
                                            MessageBox.Show("If Master target is same only vContinuum will allow to selects machines across the plans", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                            return;
                                        }
                                    }

                                }
                                else
                                {
                                    if (SelectedNode(h) == false)
                                    {
                                        node.Checked = false;
                                        if (drDrillRadioButton.Checked == true)
                                        {
                                            return;
                                        }
                                    }
                                    else
                                    {
                                        node.Checked = true;
                                    }
                                }
                                planNamesTreeView.SelectedNode = node;
                            }
                        }
                        else
                        {
                            if (e.Node.Parent.Text == _planName || _planName == null || _planName.Length == 0)
                            {
                                h.displayname = e.Node.Text;
                                h.plan = e.Node.Parent.Text;
                                

                                if (SelectedNode(h) == false)
                                {
                                    e.Node.Checked = false;
                                    if (drDrillRadioButton.Checked == true)
                                    {
                                        return;
                                    }
                                }
                                planNamesTreeView.SelectedNode = e.Node;
                            }
                            else
                            {
                                if (recoverRadioButton.Checked == true)
                                {
                                    
                                        h.displayname = e.Node.Text;
                                        h.plan = e.Node.Parent.Text;
                                    
                                    
                                    int indexes = 0;
                                    if (sourceHostsList.DoesHostExist(h, ref indexes) == true)
                                    {
                                        Host tempHost = (Host)sourceHostsList._hostList[indexes];
                                        Host selectedHost = (Host)selectedVMListForPlan._hostList[0];
                                        Trace.WriteLine(DateTime.Now + "\t comparing display names of MT " + selectedHost.masterTargetDisplayName + "\t temphost "+ tempHost.masterTargetDisplayName);
                                        if (selectedHost.masterTargetDisplayName == tempHost.masterTargetDisplayName || selectedHost.masterTargetHostName == tempHost.masterTargetHostName || selectedHost.mt_uuid == tempHost.mt_uuid)
                                        {
                                            h.displayname = e.Node.Text;
                                            h.plan = e.Node.Parent.Text;


                                            if (SelectedNode(h) == false)
                                            {
                                                e.Node.Checked = false;                                                
                                            }
                                            planNamesTreeView.SelectedNode = e.Node;
                                        }
                                        else
                                        {
                                            e.Node.Checked = false;
                                            try
                                            {
                                                if (e.Node.Parent.Checked == true)
                                                {
                                                    e.Node.Parent.Checked = false;
                                                }
                                            }
                                            catch (Exception ex)
                                            {
                                                Trace.WriteLine(DateTime.Now + "\t Failed to unselect parent " + ex.Message);
                                            }
                                            MessageBox.Show("If Master target is same only vContinuum will allow to selects machines across the plans", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                            return;
                                        }
                                    }

                                }
                                else
                                {

                                    e.Node.Checked = false;
                                    MessageBox.Show("Machines should be selected only from one plan at a time", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                    return;
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
        }

        private bool SelectedNode(Host h)
        {
            try
            {
                //When user selects the check box we wil call this method to select the vms...
                int index = 0;
                Host masterHost = new Host();
                if (sourceHostsList.DoesHostExistInPlanBased(h, ref index) == true)
                {
                    
                    Host h2 = new Host();
                    h = (Host)sourceHostsList._hostList[index];
                    if (chooseApplicationComboBox.SelectedItem.ToString() == "P2V")
                    {
                        if (addDiskRadioButton.Checked == true || volumeReSizeRadioButton .Checked == true)
                        {
                            //if (h.userName != null && h.passWord != null)
                            //{
                            //    Trace.WriteLine(DateTime.Now + "Checking credentials for the physical machine ");
                            //    PhysicalMachine p = new PhysicalMachine(h.ip, h.domain, h.userName, h.passWord);
                            //    string error = "";
                            //    //Here capturing the wmi output to show the error message if user gives wrong credentials.

                            //    if (WinPreReqs.WindowsShareEnabled(h.ip, h.domain, h.userName, h.passWord, "", ref error) == false)
                            //    {
                            //        WmiErrorMessageForm errorpopup = new WmiErrorMessageForm(error);
                            //        errorpopup.domainText.Text = h.domain;
                            //        errorpopup.userNameText.Text = h.userName;
                            //        errorpopup.passWordText.Text = h.passWord;
                            //        errorpopup.ShowDialog();
                            //        if (errorpopup._canceled == false)
                            //        {
                            //            h.domain = errorpopup.domain;
                            //            h.userName = errorpopup.username;
                            //            h.passWord = errorpopup.password;
                            //            if (WinPreReqs.WindowsShareEnabled(h.ip, h.domain, h.userName, h.passWord, "", ref error) == false)
                            //            {
                            //                MessageBox.Show(error, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            //                return false;
                            //            }
                            //        }
                            //        else
                            //        {
                            //            // MessageBox.Show(error, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            //            return false; ;
                            //        }
                            //    }
                            //    h2 = p.GetHostInfo();
                            //}
                            //else
                            {
                                if (h.inmage_hostid == null)
                                {
                                    string cxip = WinTools.GetcxIPWithPortNumber();
                                    WinPreReqs win = new WinPreReqs("", "", "", "");
                                    h2.hostname = h.hostname;
                                    h2.os = h.os;
                                    h2.ip = h.ip;
                                    Trace.WriteLine(DateTime.Now + "\t Printing the Hostname " + h2.hostname);
                                    if (win.GetDetailsFromcxcli( h2, cxip) == true)
                                    {
                                        h2 = WinPreReqs.GetHost;
                                        h.inmage_hostid = h2.inmage_hostid;
                                        Trace.WriteLine(DateTime.Now + "\t Printing the Host isd " + h2.inmage_hostid);
                                    }
                                }
                                else
                                {
                                    h2.os = h.os;
                                    h2.inmage_hostid = h.inmage_hostid;
                                }
                                Cxapicalls cxcli = new Cxapicalls();
                                if (cxcli.Post( h2, "GetHostInfo") == false)
                                {
                                    MessageBox.Show("Failed to fetch infomation form CX", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                    return false;
                                }

                                h2 = Cxapicalls.GetHost;
                            }
                            if (h2 == null)
                            {
                                return false;
                            }
                            else
                            {
                                h2.targetDataStore = h.targetDataStore;
                                h2.masterTargetDisplayName = h.masterTargetDisplayName;
                                h2.masterTargetHostName = h.masterTargetHostName;
                                h2.plan = h.plan;
                                h2.targetESXIp = h.targetESXIp;
                                h2.targetESXUserName = h.targetESXUserName;
                                h2.targetESXPassword = h.targetESXPassword;
                                h2.targetvSphereHostName = h.targetvSphereHostName;
                                h2.system_volume = h.system_volume;
                                h2.ip = h.ip;
                                h2.processServerIp = h.processServerIp;
                                h2.failOver = h.failOver;
                                // h2.os = h.os;
                                h2.passWord = h.passWord;
                                h2.domain = h.domain;
                                h2.userName = h.userName;
                                h2.retentionInDays = h.retentionInDays;
                                h2.retentionLogSize = h.retentionLogSize;
                                h2.new_displayname = h.new_displayname;
                                h2.resourcePool = h.resourcePool;
                                h2.resourcepoolgrpname = h.resourcepoolgrpname;
                                h2.target_uuid = h.target_uuid;
                                h2.mt_uuid = h.mt_uuid;
                                h2.vmDirectoryPath = h.vmDirectoryPath;
                                h2.machineType = "PhysicalMachine";
                                h2.mbr_path = h.mbr_path;
                                if (h.clusterMBRFiles != null)
                                {
                                    h2.clusterMBRFiles = h.clusterMBRFiles;
                                }
                                if (h.clusterNodes != null)
                                {
                                    h2.clusterNodes = h.clusterNodes;
                                }
                                if (h.clusterInmageUUIds != null)
                                {
                                    h2.clusterInmageUUIds = h.clusterInmageUUIds;
                                }
                                if (h.clusterName != null)
                                {
                                    h2.clusterName = h.clusterName;
                                }
                                h2.vmxversion = h.vmxversion;
                                h2.secure_src_to_ps = h.secure_src_to_ps;
                                h2.secure_ps_to_tgt = h.secure_ps_to_tgt;
                                h2.cxbasedCompression = h.cxbasedCompression;
                                h2.sourceBasedCompression = h.sourceBasedCompression;
                                h2.unitsofTimeIntervelWindow = h.unitsofTimeIntervelWindow;
                                h2.unitsofTimeIntervelWindow1Value = h.unitsofTimeIntervelWindow1Value;
                                h2.unitsofTimeIntervelWindow2Value = h.unitsofTimeIntervelWindow2Value;
                                h2.unitsofTimeIntervelWindow3Value = h.unitsofTimeIntervelWindow3Value;
                                h2.bookMarkCountWindow1 = h.bookMarkCountWindow1;
                                h2.bookMarkCountWindow2 = h.bookMarkCountWindow2;
                                h2.bookMarkCountWindow3 = h.bookMarkCountWindow3;
                                h2.selectedSparce = h.selectedSparce;
                                h2.sparceDaysSelected = h.sparceDaysSelected;
                                h2.sparceWeeksSelected = h.sparceWeeksSelected;
                                h2.sparceMonthsSelected = h.sparceMonthsSelected;
                                h2.nicList.list = h.nicList.list;
                                h2.isSourceNatted = h.isSourceNatted;
                                h2.isTargetNatted = h.isTargetNatted;
                                if (addDiskRadioButton.Checked == true || volumeReSizeRadioButton.Checked == true)
                                {
                                    if (h.cluster == "yes" && addDiskRadioButton.Checked == true)
                                    {
                                        foreach (Hashtable newList in h2.disks.list)
                                        {
                                            bool diskExists = false;
                                            foreach (Hashtable actualProtection in h.disks.list)
                                            {
                                                if (newList["disk_signature"] != null && newList["disk_signature"].ToString() != "0" && actualProtection["disk_signature"] != null && !actualProtection["disk_signature"].ToString().ToUpper().Contains("SYSTEM"))
                                                {
                                                    if (newList["disk_signature"].ToString() == actualProtection["disk_signature"].ToString())
                                                    {
                                                        diskExists = true;
                                                        break;
                                                    }
                                                }
                                            }

                                            if (diskExists == false && newList["disk_signature"] != null && newList["disk_signature"].ToString() != "0")
                                            {
                                                h.disks.list.Add(newList);
                                                try
                                                {
                                                    Trace.WriteLine(DateTime.Now + "\t Added this disk to list " + newList["Name"].ToString() + "\t signature " + newList["disk_signature"].ToString());
                                                }
                                                catch (Exception ex)
                                                {
                                                    Trace.WriteLine(DateTime.Now + "\t Failed to print " + ex.Message);
                                                }
                                            }
                                        }

                                        h2.disks.list = h.disks.list;


                                    }
                                    else
                                    {
                                        ArrayList physicalDisks;
                                        physicalDisks = h2.disks.GetDiskList;
                                        ArrayList actualProtectedDisk;
                                        actualProtectedDisk = h.disks.GetDiskList;
                                        Trace.WriteLine(DateTime.Now + "\t Going to compare the disks for the machine " + h.displayname);
                                        foreach (Hashtable hash in physicalDisks)
                                        {
                                            foreach (Hashtable diskHash in actualProtectedDisk)
                                            {

                                                if (diskHash["disk_signature"] != null && hash["disk_signature"] != null && (!hash["disk_signature"].ToString().ToUpper().Contains("SYSTEM")) && (!diskHash["disk_signature"].ToString().ToUpper().Contains("SYSTEM")))
                                                {
                                                    Trace.WriteLine(DateTime.Now + "\t Comparing disk signatures  " + hash["disk_signature"].ToString() + "\t actual protection " + diskHash["disk_signature"].ToString());
                                                    if (hash["disk_signature"].ToString() == diskHash["disk_signature"].ToString())
                                                    {
                                                        Trace.WriteLine(DateTime.Now + " Came here to make disks as selected with disk signatures");

                                                        // This is to fix the bootable disk issue at the time of resume 
                                                        // the protected disks wiil be having the same name at the time of resume also....
                                                        if (volumeReSizeRadioButton.Checked == true)
                                                        {
                                                            hash["Size"] = diskHash["Size"];
                                                            if (diskHash["target_disk_uuid"] != null)
                                                            {
                                                                hash["target_disk_uuid"] = diskHash["target_disk_uuid"];
                                                            }
                                                            if (diskHash["selected_for_protection"] != null)
                                                            {
                                                                hash["selected_for_protection"] = diskHash["selected_for_protection"];
                                                            }
                                                            hash["Name"] = diskHash["Name"];
                                                        }
                                                        else
                                                        {
                                                            hash["Name"] = diskHash["Name"];
                                                            hash["scsi_id_onmastertarget"] = diskHash["scsi_id_onmastertarget"];
                                                            if (diskHash["target_disk_uuid"] != null)
                                                            {
                                                                hash["target_disk_uuid"] = diskHash["target_disk_uuid"];
                                                            }
                                                            if (diskHash["selected_for_protection"] != null)
                                                            {
                                                                hash["selected_for_protection"] = diskHash["selected_for_protection"];
                                                            }
                                                        }
                                                    }
                                                }
                                                else
                                                {

                                                    if (hash["src_devicename"] != null && diskHash["src_devicename"] != null)
                                                    {
                                                        if (hash["src_devicename"].ToString() == diskHash["src_devicename"].ToString())
                                                        {
                                                            Trace.WriteLine(DateTime.Now + " Came here to make disks as selected");

                                                            // This is to fix the bootable disk issue at the time of resume 
                                                            // the protected disks wiil be having the same name at the time of resume also....
                                                            //hash["Name"] = diskHash["Name"];
                                                            if (volumeReSizeRadioButton.Checked == true)
                                                            {
                                                                hash["Size"] = diskHash["Size"];
                                                                if (diskHash["target_disk_uuid"] != null)
                                                                {
                                                                    hash["target_disk_uuid"] = diskHash["target_disk_uuid"];
                                                                }
                                                                if (diskHash["selected_for_protection"] != null)
                                                                {
                                                                    hash["selected_for_protection"] = diskHash["selected_for_protection"];
                                                                }
                                                                hash["Name"] = diskHash["Name"];
                                                            }
                                                            else
                                                            {
                                                                hash["Name"] = diskHash["Name"];
                                                                hash["scsi_id_onmastertarget"] = diskHash["scsi_id_onmastertarget"];
                                                                if (diskHash["target_disk_uuid"] != null)
                                                                {
                                                                    hash["target_disk_uuid"] = diskHash["target_disk_uuid"];
                                                                }
                                                                if (diskHash["selected_for_protection"] != null)
                                                                {
                                                                    hash["selected_for_protection"] = diskHash["selected_for_protection"];
                                                                }

                                                            }
                                                            //Trace.WriteLine(DateTime.Now + "\t prnting the disk on mt " + diskHash["scsi_id_onmastertarget"].ToString());
                                                        }
                                                    }
                                                    else
                                                    {
                                                        if (hash["Name"] != null && diskHash["Name"] != null)
                                                        {
                                                            Trace.WriteLine(DateTime.Now + "\t Printing the scsi mapping " + hash["Name"].ToString() + " ori  " + diskHash["Name"].ToString());
                                                            if (hash["Name"].ToString() == diskHash["Name"].ToString())
                                                            {
                                                                Trace.WriteLine(DateTime.Now + " Came here to make disks as selected");

                                                                // This is to fix the bootable disk issue at the time of resume 
                                                                // the protected disks wiil be having the same name at the time of resume also....
                                                                //hash["Name"] = diskHash["Name"];


                                                                hash["src_devicename"] = diskHash["src_devicename"];
                                                                Trace.WriteLine(DateTime.Now + "\t Assigning src_devicename becuase src_device name not exists");
                                                                if (volumeReSizeRadioButton.Checked == true)
                                                                {
                                                                    diskHash["Size"] = hash["Size"];
                                                                    if (diskHash["target_disk_uuid"] != null)
                                                                    {
                                                                        hash["target_disk_uuid"] = diskHash["target_disk_uuid"];
                                                                    }
                                                                    if (diskHash["selected_for_protection"] != null)
                                                                    {
                                                                        hash["selected_for_protection"] = diskHash["selected_for_protection"];
                                                                    }
                                                                }
                                                                else
                                                                {
                                                                    hash["scsi_id_onmastertarget"] = diskHash["scsi_id_onmastertarget"];
                                                                    if (diskHash["target_disk_uuid"] != null)
                                                                    {
                                                                        hash["target_disk_uuid"] = diskHash["target_disk_uuid"];
                                                                    }
                                                                    if (diskHash["selected_for_protection"] != null)
                                                                    {
                                                                        hash["selected_for_protection"] = diskHash["selected_for_protection"];
                                                                    }
                                                                }
                                                                //Trace.WriteLine(DateTime.Now + "\t prnting the disk on mt " + diskHash["scsi_id_onmastertarget"].ToString());
                                                            }
                                                        }

                                                        //Trace.WriteLine(DateTime.Now + "\t Printing the scsi mapping " + hash["scsi_mapping_host"].ToString() + " ori  " + diskHash["scsi_mapping_host"].ToString());
                                                        //if (hash["scsi_mapping_host"].ToString() == diskHash["scsi_mapping_host"].ToString())
                                                        //{
                                                        //    Trace.WriteLine(DateTime.Now + " Came here to make disks as selected");

                                                        //    // This is to fix the bootable disk issue at the time of resume 
                                                        //    // the protected disks wiil be having the same name at the time of resume also....
                                                        //    hash["Name"] = diskHash["Name"];
                                                        //    hash["scsi_id_onmastertarget"] = diskHash["scsi_id_onmastertarget"];
                                                        //    //Trace.WriteLine(DateTime.Now + "\t prnting the disk on mt " + diskHash["scsi_id_onmastertarget"].ToString());
                                                        //}
                                                    }
                                                }
                                            }
                                        }
                                    }
                          //          Trace.WriteLine(DateTime.Now + "\t\t Node from master config file host type" + h.os);
                            //        Trace.WriteLine(DateTime.Now + "\t\t Node from cx API             host type" + h2.os);
                                    if (volumeReSizeRadioButton.Checked == true)
                                    {
                                        selectedVMListForPlan.AddOrReplaceHost(h);
                                    }
                                    else
                                    {
                                        selectedVMListForPlan.AddOrReplaceHost(h2);
                                    }
                                }
                                else
                                {
                                    ArrayList physicalDisks;
                                    physicalDisks = h2.disks.GetDiskList;
                                    ArrayList actualProtectedDisk;
                                    actualProtectedDisk = h.disks.GetDiskList;
                                    foreach (Hashtable hash in physicalDisks)
                                    {
                                        hash["Selected"] = "No";
                                    }
                                    foreach (Hashtable hash in physicalDisks)
                                    {
                                        foreach (Hashtable diskHash in actualProtectedDisk)
                                        {
                                            Trace.WriteLine(DateTime.Now + "\t Printing the scsi mapping " + hash["scsi_mapping_host"].ToString() + " ori  " + diskHash["scsi_mapping_host"].ToString());
                                            if (hash["scsi_mapping_host"].ToString() == diskHash["scsi_mapping_host"].ToString())
                                            {
                                                Trace.WriteLine(DateTime.Now + " Came here to make disks as selected");
                                                hash["Selected"] = "Yes";
                                                // This is to fix the bootable disk issue at the time of resume 
                                                // the protected disks wiil be having the same name at the time of resume also....
                                                hash["Name"] = diskHash["Name"];
                                                hash["scsi_id_onmastertarget"] = diskHash["scsi_id_onmastertarget"];
                                                //Trace.WriteLine(DateTime.Now + "\t prnting the disk on mt " + diskHash["scsi_id_onmastertarget"].ToString());
                                            }
                                        }
                                    }
                                    h2.disks.AddScsiNumbers();
                                    if (h.memSize == 0 || h.cpuCount == 0)
                                    {
                                        h.memSize = h2.memSize;
                                        h.cpuCount = h2.cpuCount;
                                        h.nicHash = h2.nicHash;
                                        h.nicList.list = h2.nicList.list;
                                    }
                                    selectedVMListForPlan.AddOrReplaceHost(h);
                                    p2vResumeList.AddOrReplaceHost(h2);
                                }
                            }
                        }
                        else if (failBackRadioButton.Checked == true)
                        {
                            if (selectedVMListForPlan._hostList.Count != 0)
                            {
                                foreach (Host h1 in selectedVMListForPlan._hostList)
                                {
                                    if (h.hostname != h1.hostname)
                                    {
                                        MessageBox.Show("P2V Failback protection can be done only one machine at a time", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                        return false;
                                    }
                                }
                                
                            }
                            else
                            {
                                selectedVMListForPlan.AddOrReplaceHost(h);

                            }
                        }
                        else
                        {
                            if (drDrillRadioButton.Checked == true)
                            {
                                if (selectedVMListForPlan._hostList.Count == 0)
                                {
                                    selectedVMListForPlan.AddOrReplaceHost(h);
                                }
                                else
                                {
                                    Host h1 = (Host)selectedVMListForPlan._hostList[0];
                                   
                                    if (h.vConversion != null && h1.vConversion != null)
                                    {
                                        if (h.vConversion != h1.vConversion)
                                        {
                                            if (h.vConversion.Contains("3.") || h1.vConversion.Contains("3."))
                                            {
                                                MessageBox.Show("You are trying to select VM(s) from two different protections of vContinuum versions. Only one version of protections are supported for DR Drill at a time.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                                return false;
                                            }
                                            else
                                            {
                                                selectedVMListForPlan.AddOrReplaceHost(h);
                                            }
                                        }
                                        else
                                        {
                                            selectedVMListForPlan.AddOrReplaceHost(h);
                                        }
                                    }
                                    else
                                    {
                                        if (h.vConversion != null && h1.vConversion == null)
                                        {
                                            MessageBox.Show("You are trying to select VM(s) from two different protections of vContinuum versions. Only one version of protections are supported for DR Drill at a time.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                            return false;
                                        }
                                        else if (h.vConversion == null && h1.vConversion != null)
                                        {
                                            MessageBox.Show("You are trying to select VM(s) from two different protections of vContinuum versions. Only one version of protections are supported for DR Drill at a time.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                            return false;
                                        }
                                        else if(h.vConversion == null && h1.vConversion == null)
                                        {
                                            selectedVMListForPlan.AddOrReplaceHost(h);
                                        }
                                    }

                                    
                                }
                            }
                            else
                            {
                                selectedVMListForPlan.AddOrReplaceHost(h);
                                if (removeDiskRadioButton.Checked == true || removeVolumeRadioButton.Checked == true)
                                {
                                    currentDisplayedPhysicalHostIp = h.displayname;
                                    planSelected = h.plan;
                                    removeDisksDataGridView.Enabled = true;
                                    
                                }
                            }
                            
                        }
                    }
                    else
                    {
                        if (selectedVMListForPlan._hostList.Count == 0)
                        {
                            selectedVMListForPlan.AddOrReplaceHost(h);
                            if (removeDiskRadioButton.Checked == true || removeVolumeRadioButton.Checked == true)
                            {
                                currentDisplayedPhysicalHostIp = h.displayname;
                                planSelected = h.plan;
                                removeDisksDataGridView.Enabled = true;
                                
                            }
                        }
                        else
                        {
                            Host tempHost = (Host)selectedVMListForPlan._hostList[0];
                            if (failBackRadioButton.Checked == true || addDiskRadioButton.Checked == true || volumeReSizeRadioButton.Checked == true)
                            {
                                if (h.esxIp != null && h.esxIp != tempHost.esxIp)
                                {
                                    MessageBox.Show("You are trying to select machines from different esx servers. You cannot choose this VM.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                    return false;
                                }
                                else
                                {
                                    selectedVMListForPlan.AddOrReplaceHost(h);
                                    if (removeDiskRadioButton.Checked == true || removeVolumeRadioButton.Checked == true)
                                    {
                                        currentDisplayedPhysicalHostIp = h.displayname;
                                        planSelected = h.plan;
                                        removeDisksDataGridView.Enabled = true;
                                        
                                    }
                                }
                            }
                            else if (drDrillRadioButton.Checked == true)
                            {
                                if (selectedVMListForPlan._hostList.Count == 0)
                                {
                                    selectedVMListForPlan.AddOrReplaceHost(h);
                                }
                                else
                                {
                                    Host h1 = (Host)selectedVMListForPlan._hostList[0];
                                    if (h.vConversion != null && h1.vConversion != null)
                                    {
                                        if (h.vConversion.Contains("3.") || h1.vConversion.Contains("3."))
                                        {
                                            MessageBox.Show("You are trying to select VM(s) from two different protections of vContinuum versions. Only one version of protections are supported for DR Drill at a time.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                            return false;
                                        }
                                        else
                                        {
                                            selectedVMListForPlan.AddOrReplaceHost(h);
                                        }
                                    }
                                    else
                                    {
                                        if (h.vConversion != null && h1.vConversion == null)
                                        {
                                            MessageBox.Show("You are trying to select VM(s) from two different protections of vContinuum versions. Only one version of protections are supported for DR Drill at a time.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                            return false;
                                        }
                                        else if (h.vConversion == null && h1.vConversion != null)
                                        {
                                            MessageBox.Show("You are trying to select VM(s) from two different protections of vContinuum versions. Only one version of protections are supported for DR Drill at a time.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                            return false;
                                        }
                                        else if (h.vConversion == null && h1.vConversion == null)
                                        {
                                            selectedVMListForPlan.AddOrReplaceHost(h);
                                        }
                                    }


                                }
                            }
                            else
                            {
                                selectedVMListForPlan.AddOrReplaceHost(h);
                                if (removeDiskRadioButton.Checked == true || removeVolumeRadioButton.Checked == true)
                                {
                                    currentDisplayedPhysicalHostIp = h.displayname;
                                    planSelected = h.plan;
                                    removeDisksDataGridView.Enabled = true;
                                }
                            }
                        }
                    }
                    masterHost.displayname = h.masterTargetDisplayName;
                    masterHost.hostname = h.masterTargetHostName;
                    masterHost.source_uuid = h.mt_uuid;
                    masterHost.esxIp = h.targetESXIp;
                    if (h.targetvSphereHostName != null)
                    {
                        masterHost.vSpherehost = h.targetvSphereHostName;
                    }
                    Trace.WriteLine(DateTime.Now + "\t Prinitng the master target name " + masterHost.displayname + " hostname " + masterHost.hostname);
                    int masterIndex = 0;
                    if (mastertargetsHostList.DoesHostExist(masterHost, ref masterIndex) == true)
                    {
                        foreach (Host h1 in mastertargetsHostList._hostList)
                        {
                            if (h1.hostname == masterHost.hostname && h1.esxIp == masterHost.esxIp && h1.displayname == masterHost.displayname)
                            {
                                masterHost = h1;
                            }

                        }
                        masterHost = (Host)mastertargetsHostList._hostList[masterIndex];
                        //MessageBox.Show(masterHost.configurationList.Count.ToString() + " " + masterHost.networkNameList.Count.ToString());
                        Trace.WriteLine(DateTime.Now + " \t Came here to print the existing esxip " + masterHost.esxIp);
                    }
                    masterHost.machineType = "VirtualMachine";
                    masterTargetList.AddOrReplaceHost(masterHost);

                    Trace.WriteLine(DateTime.Now + "\t printing the count of the list " + selectedVMListForPlan._hostList.Count.ToString() + " " + masterTargetList._hostList.Count.ToString());
                }
                else
                {
                    Trace.WriteLine(DateTime.Now + " \t This VM " + h.displayname + " is not found is sourceList ");
                }
                return true;
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
        }



        internal bool EsxDiskDetails(Host hDisks)
        {
            try
            {
                // for the displaying the disk details of selected vm......
                //Trace.WriteLine(DateTime.Now + " \t Entered: EsxDiskDetails of the Esx_AddSourcePanelHandler  ");         
                ArrayList physicalDisks;
                int i = 0;
                physicalDisks = hDisks.disks.GetDiskList;
                removeDisksDataGridView.Rows.Clear();
                removeDisksDataGridView.Columns[0].HeaderText = "Name";
                removeDisksDataGridView.Columns[1].HeaderText = "Size";
                esxDiskInfoDatagridView.RowCount = hDisks.disks.GetDiskList.Count;
                foreach (Hashtable disk in physicalDisks)
                {
                    esxDiskInfoDatagridView.Rows[i].Cells[0].Value = "Disk" + i.ToString();
                    
                    //Adding when the click event done in the primaryDataGridView ip column 
                    // disk["Selected"]        = "Yes";
                    if (disk["Size"] != null)
                    {
                        float size = float.Parse(disk["Size"].ToString());
                        size = size / (1024 * 1024);
                        //size                     = int.Parse(String.Format("{0:##.##}", size));
                        size = float.Parse(String.Format("{0:00000.000}", size));
                        esxDiskInfoDatagridView.Rows[i].Cells[1].Value = size;
                    }
                    i++;
                    Debug.WriteLine(DateTime.Now + "\t Physical Disk name = " + disk["Name"] + " Size: = " + disk["Size"] + " Selected: = " + disk["Selected"]);
                    esxDiskInfoDatagridView.Visible = true;
                    Debug.WriteLine(DateTime.Now + "\t______________________________________    before dispalydriverdetails printing _selected SourceList");
                    //allServersForm._selectedSourceList.Print();

                    // Trace.WriteLine(DateTime.Now + " \t Exiting: EsxDiskDetails of the Esx_AddSourcePanelHandler  ");
                }


                if (removeDiskRadioButton.Checked == true)
                {
                    removeDisksDataGridView.Columns[3].Visible = true;
                    removeDisksDataGridView.Rows.Clear();
                    ArrayList physicalDisksForRemove;
                    int j = 0;
                    physicalDisksForRemove = hDisks.disks.GetDiskList;

                    removeDisksDataGridView.RowCount = hDisks.disks.GetDiskList.Count;
                    foreach (Hashtable disk in physicalDisksForRemove)
                    {
                        removeDisksDataGridView.Rows[j].Cells[0].Value = "Disk" + j.ToString();
                        if (disk["remove"] != null)
                        {
                            if (disk["remove"].ToString() == "True")
                            {
                                removeDisksDataGridView.Rows[j].Cells[2].Value = true;
                            }
                            else
                            {
                                removeDisksDataGridView.Rows[j].Cells[2].Value = false;
                            }
                        }
                        //Adding when the click event done in the primaryDataGridView ip column 
                        // disk["Selected"]        = "Yes";
                        if (disk["Size"] != null)
                        {
                            float size = float.Parse(disk["Size"].ToString());
                            size = size / (1024 * 1024);
                            //size                     = int.Parse(String.Format("{0:##.##}", size));
                            size = float.Parse(String.Format("{0:00000.000}", size));
                            removeDisksDataGridView.Rows[j].Cells[1].Value = size;
                        }
                        removeDisksDataGridView.Rows[j].Cells[3].Value = "Details";
                        j++;
                        Debug.WriteLine(DateTime.Now + "\t Physical Disk name = " + disk["Name"] + " Size: = " + disk["Size"] + " Selected: = " + disk["Selected"]);


                        removeDisksDataGridView.Visible = true;
                        Debug.WriteLine(DateTime.Now + "\t______________________________________    before dispalydriverdetails printing _selected SourceList");
                        //allServersForm._selectedSourceList.Print();

                        // Trace.WriteLine(DateTime.Now + " \t Exiting: EsxDiskDetails of the Esx_AddSourcePanelHandler  ");
                    }
                }
                else
                {
                    removeDisksDataGridView.Columns[3].Visible = false;
                }

                esxDiskInfoDatagridView.Enabled = true;

                
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

        private void newProtectionRadioButton_CheckedChanged(object sender, EventArgs e)
        {
            try
            {
                // This is when user selects the newprotection.....
                if (newProtectionRadioButton.Checked == true)
                {
                    resumeButton.Visible = false;
                    if (chooseApplicationComboBox.SelectedItem.ToString() == "P2V")
                    { //Writing into the vContinuum_breif.log.......... 
                        breifLog.WriteLine(DateTime.Now + " \t User selected for the P2V new protection ");
                        selectedActionForPlan = "P2V";
                    }
                    else
                    { //Writing into the vContinuum_breif.log.......... 
                        breifLog.WriteLine(DateTime.Now + " \t User selected for the ESX new protection ");
                        selectedActionForPlan = "ESX";
                    }
                    breifLog.Flush();
                    if (newProtectionRadioButton.Checked == true)
                    {
                        selectedVMListForPlan = new HostList();
                        masterTargetList = new HostList();
                        AllServersForm allServersForm = new AllServersForm(selectedActionForPlan, selectedActionForPlan);
                        allServersForm.ShowDialog();
                        newProtectionRadioButton.Checked = false;
                        if (allServersForm.calledCancelByUser == false)
                        {
                            cancelButton.Visible = true;
                            cancelButton.Text = "Close";
                            offlineSyncExportRadioButton.Checked = false;
                            newProtectionRadioButton.Checked = false;
                            pushAgentRadioButton.Checked = false;
                            manageExistingPlanRadioButton.Checked = false;
                            _directoryNameinXmlFile = null;
                            taskGroupBox.Enabled = false;
                            planNamesTreeView.Nodes.Clear();
                            progressBar.Visible = true;
                            getPlansBackgroundWorker.RunWorkerAsync();
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
        }

       
        private void resumeButton_Click(object sender, EventArgs e)
        {
            try
            {
                //Resume buttons means this is a current action button:
                //When user call the resume this will called......            
                if (resumeProtectionRadioButton.Checked == true)
                { //Writing into the vContinuum_breif.log.......... 

                    bool clutserMachine = false;
                    foreach (Host h in sourceHostsList._hostList)
                    {
                        if(h.cluster == "yes")
                        {
                           clutserMachine = true;
                        }
                    }

                    if(clutserMachine == true)
                    {
                        if(ClusterReadinessChecks() == false)
                        {
                            Trace.WriteLine(DateTime.Now + "\t User has not selected all clutser nodes ");
                            return;
                        }
                    }


                    breifLog.WriteLine(DateTime.Now + " \t User selected for resume for these vms");
                    foreach (Host h in selectedVMListForPlan._hostList)
                    {
                        breifLog.WriteLine(DateTime.Now + " \t" + h.displayname + " \t plan " + h.plan + " \t target esx ip " + h.targetESXIp + "\t Source esx ip " + h.esxIp + " \t Master target " + h.masterTargetDisplayName);
                    }

                    try
                    {
                        Host masterHostForRetentionDrive = (Host)masterTargetList._hostList[0];
                        masterHostForRetentionDrive.disks.partitionList.Clear();
                        if (masterHostForRetentionDrive.disks.GetPartitionsFreeSpace( masterHostForRetentionDrive) == false)
                        {

                        }
                        masterHostForRetentionDrive = DiskList.GetHost;
                        masterHostForRetentionDrive.Print();
                        if (masterHostForRetentionDrive.disks.partitionList.Count > 0)
                        {
                            foreach (Host h in selectedVMListForPlan._hostList)
                            {
                                bool driveExists = false;
                                foreach (Hashtable hash in masterHostForRetentionDrive.disks.partitionList)
                                {
                                    if (h.retention_drive != null)
                                    {
                                        if (h.retention_drive.ToUpper() == hash["Name"].ToString().ToUpper())
                                        {
                                            Trace.WriteLine(DateTime.Now + "\t Found the retention drive" + h.retention_drive);
                                            driveExists = true;
                                        }
                                    }
                                    else
                                    {
                                        if (h.retentionPath != null)
                                        {
                                            if (h.retentionPath.ToUpper().StartsWith(hash["Name"].ToString().ToUpper()))
                                            {
                                                Trace.WriteLine(DateTime.Now + "\t Found the retention drive" + h.retentionPath);
                                                driveExists = true;
                                            }
                                        }
                                        else
                                        {
                                            driveExists = true;
                                        }
                                    }
                                }

                                if (driveExists == false)
                                {
                                    logTextBox.AppendText("Not able to find retention drive " + h.retention_drive + " which was selected at the time of protection. If drive exists increase size or add new disk and assign same drive letter." + Environment.NewLine);
                                    return;
                                }

                            }
                        }


                        // RemoteWin rw = new RemoteWin(masterHost.ip, masterHost.domain, masterHost.userName, masterHost.passWord);
                        //rw.Connect(ref errorMessage, ref errorCode);
                        Trace.WriteLine(DateTime.Now + " \t Came here to check whether the mt is in use or not");
                        WinPreReqs win = new WinPreReqs("", "", "", "");
                        Trace.WriteLine(DateTime.Now + "\t Printing mt host name and ip " + masterHostForRetentionDrive.hostname + masterHostForRetentionDrive.ip);
                    }
                    catch (Exception ex)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Failed to check retention path exists or not " + ex.Message);
                    }

                    breifLog.Flush();
                    if (selectedVMListForPlan._hostList.Count != 0)
                    {
                        if (xmlFileMadeSuccessfully == false)
                        {
                            string planname = null;
                            Esx target = new Esx();
                            foreach (Host h in selectedVMListForPlan._hostList)
                            {
                                ArrayList physicalDisks;
                                int i = 0;
                                physicalDisks = h.disks.GetDiskList;
                                foreach (Hashtable hash in physicalDisks)
                                {
                                    hash["Protected"] = "no";
                                    if (hash["scsi_id_onmastertarget"] != null)
                                    {
                                        hash["scsi_id_onmastertarget"] = null;
                                    }
                                }
                            }
                            SolutionConfig solution = new SolutionConfig();
                            foreach (Host h in masterTargetList._hostList)
                            {
                                target.configurationList = h.configurationList;
                                target.dataStoreList = h.dataStoreList;
                                target.lunList = h.lunList;
                                target.networkList = h.networkNameList;

                            }
                            foreach (Host h in selectedVMListForPlan._hostList)
                            {
                                h.failOver = "no";
                            }
                            Host h1 = (Host)selectedVMListForPlan._hostList[0];
                            Host masterHost = new Host();
                            masterHost.displayname = h1.masterTargetDisplayName;
                            masterHost.hostname = h1.masterTargetHostName;
                            int index = 0;
                            if (masterTargetList._hostList.Count != 0)
                            {
                                masterHost = (Host)masterTargetList._hostList[0];
                            }
                            planname = h1.plan;
                            if (_directoryNameinXmlFile == null)
                            {
                                Random _r = new Random();
                                int n = _r.Next(999999);
                                _directoryNameinXmlFile = planname + "_" + masterHost.hostname + "_Resume_" + n.ToString();

                            }
                            foreach (Host h in selectedVMListForPlan._hostList)
                            {
                                h.directoryName = _directoryNameinXmlFile;
                            }
                            solution.WriteXmlFile(selectedVMListForPlan, masterTargetList, target, WinTools.GetcxIP(), "ESX.xml", planname, false);
                            //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
                            //based on above comment we have added xmlresolver as null
                            XmlDocument document = new XmlDocument();
                            document.XmlResolver = null;
                            string xmlfile = latestFilePath + "\\ESX.xml";
                            //StreamReader reader = new StreamReader(xmlfile);
                            //string xmlString = reader.ReadToEnd();
                            XmlReaderSettings settings = new XmlReaderSettings();
                            settings.ProhibitDtd = true;
                            settings.XmlResolver = null;
                            //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
                            using (XmlReader reader1 = XmlReader.Create(xmlfile, settings))
                            {
                                document.Load(reader1);
                                //reader1.Close();
                            }
                            //document.Load(xmlfile);
                            //reader.Close();
                            //XmlNodeList hostNodes = null;
                            XmlNodeList rootNodes = null, hostNodes = null;
                            rootNodes = document.GetElementsByTagName("root");

                            foreach (XmlNode node in rootNodes)
                            {
                                node.Attributes["xmlDirectoryName"].Value = _directoryNameinXmlFile;
                                node.Attributes["batchresync"].Value = h1.batchResync.ToString();
                                //node.Attributes["isSourceNatted"].Value = h1.isSourceNatted.ToString();
                                //node.Attributes["isTargetNatted"].Value = h1.isTargetNatted.ToString();
                            }
                            document.Save(xmlfile);
                            if (File.Exists(latestFilePath + "\\ESX.xml"))
                            {
                                FileInfo destFile = new FileInfo(fxInstallPath + "\\" + _directoryNameinXmlFile + "\\ESX.xml");
                                if (destFile.Directory != null && !destFile.Directory.Exists)
                                {
                                    destFile.Directory.Create();
                                }
                                File.Copy(latestFilePath + "\\ESX.xml", destFile.ToString(), true);
                                WinTools win = new WinTools();
                                win.SetFilePermissions(fxInstallPath + "\\" + _directoryNameinXmlFile + "\\ESX.xml");
                            }
                            xmlFileMadeSuccessfully = true;
                            if (AllServersForm.SetFolderPermissions(fxInstallPath + "\\" + _directoryNameinXmlFile) == true)
                            {
                                Trace.WriteLine(DateTime.Now + "\t Successfully set permissions for the folder ");
                            }
                            else
                            {
                                Trace.WriteLine(DateTime.Now + "\t Failed to Set permissions for the folder ");
                            }

                        }
                        progressBar.Visible = true;
                        resumeButton.Enabled = false;
                        cancelButton.Enabled = false;
                        planNamesTreeView.Enabled = false;
                        //this.Enabled = false;
                        //backgroundWorker.RunWorkerAsync();
                        Host tempMasterHost = (Host)masterTargetList._hostList[0];
                        StatusForm status = new StatusForm(selectedVMListForPlan, tempMasterHost, AppName.Resume, _directoryNameinXmlFile);
                        progressBar.Visible = false;
                        resumeButton.Visible = false;
                        resumeButton.Enabled = true;
                        cancelButton.Enabled = true;                        
                        planNamesTreeView.Enabled = true;
                        cancelButton.Visible = true;
                        cancelButton.Text = "Close";
                        offlineSyncExportRadioButton.Checked = false;
                        newProtectionRadioButton.Checked = false;
                        pushAgentRadioButton.Checked = false;
                        manageExistingPlanRadioButton.Checked = false;
                        _directoryNameinXmlFile = null;
                        taskGroupBox.Enabled = false;
                        planNamesTreeView.Nodes.Clear();
                        progressBar.Visible = true;
                        getPlansBackgroundWorker.RunWorkerAsync();
                        
                    }
                    else
                    {
                        MessageBox.Show("Select machines to continue", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    }
                }

               
                //When user selects recover machines and clicks next we will call this...
                if (recoverRadioButton.Checked == true)
                {

                     bool clutserMachine = false;
                     foreach (Host h in sourceHostsList._hostList)
                    {
                        if(h.cluster == "yes")
                        {
                           clutserMachine = true;
                        }
                    }

                    if(clutserMachine == true)
                    {
                        if(ClusterReadinessChecks() == false)
                        {
                            Trace.WriteLine(DateTime.Now + "\t User has not selected all clutser nodes ");
                            return;
                        }
                    }

                    breifLog.WriteLine(DateTime.Now + " \t User selected the operation for the recovery");
                    breifLog.Flush();
                    if (selectedVMListForPlan._hostList.Count != 0)
                    {
                        AllServersForm allServersFrom = new AllServersForm("recovery", "");
                        allServersFrom.ShowDialog();
                        recoverRadioButton.Checked = false;
                        manageExistingPlanRadioButton.Checked = false;
                        planNamesTreeView.CheckBoxes = false;
                        recoverRadioButton.Checked = false;
                        cancelButton.Visible = true;
                        cancelButton.Text = "Close";
                        offlineSyncExportRadioButton.Checked = false;
                        newProtectionRadioButton.Checked = false;
                        pushAgentRadioButton.Checked = false;
                        manageExistingPlanRadioButton.Checked = false;
                        _directoryNameinXmlFile = null;

                        planNamesTreeView.Nodes.Clear();
                        progressBar.Visible = true;
                        taskGroupBox.Enabled = false;
                        getPlansBackgroundWorker.RunWorkerAsync();
                    }
                    else
                    {
                        MessageBox.Show("Select VMs to recover", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    }
                }
                //DrDrill 
                if (drDrillRadioButton.Checked == true)
                {
                     bool clutserMachine = false;
                     foreach (Host h in sourceHostsList._hostList)
                    {
                        if(h.cluster == "yes")
                        {
                           clutserMachine = true;
                        }
                    }

                    if(clutserMachine == true)
                    {
                        if(ClusterReadinessChecks() == false)
                        {
                            Trace.WriteLine(DateTime.Now + "\t User has not selected all clutser nodes ");
                            return;
                        }
                    }

                    breifLog.WriteLine(DateTime.Now + " \t User selected the operation for the Dr Drill");
                    breifLog.Flush();
                    if (selectedVMListForPlan._hostList.Count != 0)
                    {
                        AllServersForm allServersFrom = new AllServersForm("drdrill", "");
                        allServersFrom.ShowDialog();
                        recoverRadioButton.Checked = false;
                        manageExistingPlanRadioButton.Checked = false;
                        planNamesTreeView.CheckBoxes = false;
                        recoverRadioButton.Checked = false;
                        cancelButton.Visible = true;
                        cancelButton.Text = "Close";
                        offlineSyncExportRadioButton.Checked = false;
                        newProtectionRadioButton.Checked = false;
                        pushAgentRadioButton.Checked = false;
                        manageExistingPlanRadioButton.Checked = false;
                        _directoryNameinXmlFile = null;
                        taskGroupBox.Enabled = false;
                        planNamesTreeView.Nodes.Clear();
                        progressBar.Visible = true;
                        getPlansBackgroundWorker.RunWorkerAsync();
                    }
                    else
                    {
                        MessageBox.Show("Select VMs to Dr Drill", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    }
                }
                //Remove will call this 
                if (removeRadioButton.Checked == true)
                {
                    try
                    {

                        if (selectedVMListForPlan._hostList.Count != 0)
                        {
                            //Writing into the vContinuum_breif.log.......... 
                            breifLog.WriteLine(DateTime.Now + " \t User selected for Removee operation for these vms ");
                            bool cluster = false;
                            string vmList = null;
                            string clusterGroup = null;

                            foreach (Host h in selectedVMListForPlan._hostList)
                            {
                                if (h.cluster == "yes")
                                {
                                    if (vmList == null)
                                    {
                                        vmList = h.displayname;

                                    }
                                    else
                                    {
                                        vmList = vmList + "," + h.displayname;
                                    }
                                    if (clusterGroup == null)
                                    {
                                        clusterGroup = h.clusterName;
                                    }                                    
                                }
                            }
                            if (vmList != null)
                            {
                                switch (MessageBox.Show("You are trying to remove " + vmList + " from protection. It belongs " + clusterGroup + " cluster. It may results into recovery issues. Do you want to continue?", "Warning", MessageBoxButtons.YesNo, MessageBoxIcon.Warning))
                                {
                                    case DialogResult.No:
                                        return;
                                        break;

                                }
                            }

                            foreach (Host h in selectedVMListForPlan._hostList)
                            {
                                breifLog.WriteLine(h.displayname + " \t plan name " + h.plan + " \t master target " + h.masterTargetDisplayName + " \t target esx " + h.targetESXIp + " \t source esx " + h.esxIp);
                            }
                            breifLog.Flush();
                            switch (MessageBox.Show("Are you sure that you want remove protection for all the selected machines ? You will not be able to recover selected machines. ", "Remove protection", MessageBoxButtons.YesNo, MessageBoxIcon.Question))
                            {
                                case DialogResult.Yes:
                                    progressBar.Visible = true;
                                    resumeButton.Enabled = false;
                                    this.Enabled = false;
                                    detachBackgroundWorker.RunWorkerAsync();
                                    break;
                                case DialogResult.No:
                                    break;

                            }
                           
                        }
                        else
                        {
                            MessageBox.Show("Select Machines to remove", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        }
                    }
                    catch (Exception ex)
                    {
                        System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                        Trace.WriteLine("ERROR*******************************************");
                        Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                        Trace.WriteLine("Exception  =  " + ex.Message);
                        Trace.WriteLine("ERROR___________________________________________");
                    }
                }

                if(resyncRadioButton.Checked == true)
                {
                    progressBar.Visible = true;
                    resyncBackgroundWorker.RunWorkerAsync();
                }
                if (removeDiskRadioButton.Checked == true)
                {
                    if (selectedVMListForPlan._hostList.Count != 0)
                    {
                        bool clutserMachine = false;
                        foreach (Host h in sourceHostsList._hostList)
                        {
                            if (h.cluster == "yes")
                            {
                                clutserMachine = true;
                            }
                        }

                        if (clutserMachine == true)
                        {
                            if (ClusterReadinessChecks() == false)
                            {
                                Trace.WriteLine(DateTime.Now + "\t User has not selected all clutser nodes ");
                                return;
                            }
                        }
                        string vmsList = null;
                        foreach (Host h in selectedVMListForPlan._hostList)
                        {
                            if (vmsList == null)
                            {
                                vmsList = h.displayname + Environment.NewLine;
                            }
                            else
                            {
                                vmsList = vmsList + h.displayname + Environment.NewLine;
                            }
                        }

                        string message = null;
                        message = "Are you sure to remove selected disks from the Protection for the following server(s)" + Environment.NewLine +
                                  vmsList;

                        switch (MessageBox.Show(message, "Remove disk(s) from protection", MessageBoxButtons.YesNo, MessageBoxIcon.Question))
                        {
                            case DialogResult.Yes:                                
                                RemoveDisk();
                                break;
                            case DialogResult.No:
                                break;

                        }
                       
                    }
                    else
                    {
                        MessageBox.Show("You have not selected machines for Remove disk operation. Select machine(s) to proceed.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return;
                    }
                }

                if (removeVolumeRadioButton.Checked == true)
                {
                    if (selectedVMListForPlan._hostList.Count != 0)
                    {
                        bool clutserMachine = false;
                        foreach (Host h in sourceHostsList._hostList)
                        {
                            if (h.cluster == "yes")
                            {
                                clutserMachine = true;
                            }
                        }

                        if (clutserMachine == true)
                        {
                            if (ClusterReadinessChecks() == false)
                            {
                                Trace.WriteLine(DateTime.Now + "\t User has not selected all clutser nodes ");
                                return;
                            }
                        }
                        string vmsList = null;
                        foreach (Host h in selectedVMListForPlan._hostList)
                        {
                            if (vmsList == null)
                            {
                                vmsList = h.displayname + Environment.NewLine;
                            }
                            else
                            {
                                vmsList = vmsList + h.displayname + Environment.NewLine;
                            }
                        }

                        string message = null;
                        message = "Are you sure to remove selected volumes from the Protection for the following server(s)" + Environment.NewLine +
                                  vmsList;

                        switch (MessageBox.Show(message, "Remove volume(s) from protection", MessageBoxButtons.YesNo, MessageBoxIcon.Question))
                        {
                            case DialogResult.Yes:
                                RemoveVolumesFromSelectedList();
                                break;
                            case DialogResult.No:
                                break;
                        }

                    }
                    else
                    {
                        MessageBox.Show("You have not selected machines for Remove volume operation. Select machine(s) to proceed.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return;
                    }
                }

                //Failback protection will come here.....
                if (failBackRadioButton.Checked == true)
                {
                    if (selectedActionForPlan == "V2P")
                    {

                        string message = null;
                        Host h = (Host)selectedVMListForPlan._hostList[0];
                        if (h.os == OStype.Windows)
                        {
                            message = "Note:" + Environment.NewLine +
                                       "1. Make sure that reocvery VM hostname and Win2Go machine hostname is different." + Environment.NewLine +
                                       "2. Make sure that recovery VM is reporting to CX." + Environment.NewLine +
                                       "3. Disable if any multipath software is enable on Win2Go." + Environment.NewLine + "Click Ok to continue";

                        }
                        else
                        {
                            message = "Note:" + Environment.NewLine +
                                          "1. Make sure that reocvery VM hostname and Live CD machine hostname is different." + Environment.NewLine +
                                          "2. Make sure that recovery VM is reporting to CX." + Environment.NewLine +
                                          "3. Disable if any multipath software is enable on Live CD." + Environment.NewLine + "Click Ok to continue";


                        }
                        MessageBox.Show(message, "Information", MessageBoxButtons.OK, MessageBoxIcon.Information);
                        breifLog.WriteLine(DateTime.Now + " \t User selected for failback for these vms V2P ");
                        progressBar.Visible = true;
                        resumeButton.Enabled = false;
                        cancelButton.Enabled = false;
                        planNamesTreeView.Enabled = false;
                        
                        WinPreReqs win = new WinPreReqs(h.ip, h.domain, h.userName, h.passWord);
                       string cxip = WinTools.GetcxIPWithPortNumber();
                       win.SetHostIPinputhostname(h.hostname,  h.ip, cxip);
                       h.ip = WinPreReqs.GetIPaddress;

                       win.GetDetailsFromcxcli( h, cxip);
                       h = WinPreReqs.GetHost;

                       if (h.vx_agent_heartbeat == false || h.fx_agent_heartbeat == false )
                       {
                           if (h.vx_agent_heartbeat == false)
                           {
                               logTextBox.AppendText("VX agent heart is not reporting to CX for the machine: " + h.displayname + Environment.NewLine);

                           }
                           if (h.fx_agent_heartbeat == false)
                           {
                               logTextBox.AppendText("FX agent heart is not reporting to CX for the machine: " + h.displayname + Environment.NewLine);
                           }
                           //if (h.vxlicensed == false)
                           //{
                           //    logTextBox.AppendText("VX agent is not licensed for the machine: " + h.displayname + Environment.NewLine);
                           //}
                           //if (h.fxlicensed == false)
                           //{
                           //    logTextBox.AppendText("FX agent is not licensed for the machine: " + h.displayname + Environment.NewLine);
                           //}
                           return;
                       }

                        logTextBox.AppendText("Retrieving information from recovered machine." + Environment.NewLine);
                        v2pBackgroundWorker.RunWorkerAsync();
                    }
                    else
                    {

                        bool clutserMachine = false;
                        foreach (Host h in sourceHostsList._hostList)
                        {
                            if (h.cluster == "yes")
                            {
                                clutserMachine = true;
                            }
                        }

                        if (clutserMachine == true)
                        {
                            if (ClusterReadinessChecks() == false)
                            {
                                Trace.WriteLine(DateTime.Now + "\t User has not selected all clutser nodes ");
                                return;
                            }
                        }
                        breifLog.WriteLine(DateTime.Now + " \t User selected for failback for these vms ");
                        foreach (Host h in selectedVMListForPlan._hostList)
                        {
                            breifLog.WriteLine(h.displayname + " \t planname " + h.plan + " \t target esx ip " + h.targetESXIp + " \t source esx ip " + h.esxIp);
                        }
                        breifLog.Flush();
                        if (selectedVMListForPlan._hostList.Count != 0)
                        {
                            if (File.Exists(latestFilePath + "\\SourceXML.xml"))
                            {
                                File.Delete(latestFilePath + "\\SourceXML.xml");
                            }
                            progressBar.Visible = true;
                            resumeButton.Enabled = false;
                            this.Enabled = false;
                            getFailBackDetailsBackgroundWorker.RunWorkerAsync();
                        }
                    }
                }
                //Addition of disk will come here....
                if (addDiskRadioButton.Checked == true)
                {
                    if (selectedVMListForPlan._hostList.Count == 0)
                    {
                        MessageBox.Show("Select machines to perform Addition of Disk.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return;
                    }
                    if (chooseApplicationComboBox.SelectedItem.ToString() == "P2V")
                    {
                        selectedActionForPlan = "BMR Protection";
                    }
                    else
                    {
                        selectedActionForPlan = "ESX";
                    }

                    bool clutserMachine = false;
                    foreach (Host h in sourceHostsList._hostList)
                    {
                        if (h.cluster == "yes")
                        {
                            clutserMachine = true;
                        }
                    }

                    if (clutserMachine == true)
                    {
                        if (ClusterReadinessChecks() == false)
                        {
                            Trace.WriteLine(DateTime.Now + "\t User has not selected all clutser nodes ");
                            return;
                        }
                    }
                    //Writing into the vContinuum_breif.log.......... 
                    breifLog.WriteLine(DateTime.Now + "\t User selected for the addition of disks for these vms ");
                    foreach (Host h in selectedVMListForPlan._hostList)
                    {
                        breifLog.WriteLine(DateTime.Now + "\t " + h.displayname + " \t planname " + h.plan + " \t target esx ip " + h.targetESXIp + " \t source esx ip " + h.esxIp + " \t master target " + h.masterTargetDisplayName);
                    }
                    breifLog.Flush();
                    this.Enabled = false;
                    progressBar.Visible = true;
                    resumeButton.Enabled = false;
                    cancelButton.Enabled = false;
                    additionOfDiskBackgroundWorker.RunWorkerAsync();
                }
                if (offLineSyncImportRadioButton.Checked == true)
                {
                    if (selectedVMListForPlan._hostList.Count != 0)
                    {
                        ImportOfflineSyncForm import = new ImportOfflineSyncForm();
                        import.ShowDialog();
                        offLineSyncImportRadioButton.Checked = false;
                        cancelButton.Visible = true;
                        cancelButton.Text = "Close";
                        offlineSyncExportRadioButton.Checked = false;
                        newProtectionRadioButton.Checked = false;
                        pushAgentRadioButton.Checked = false;
                        manageExistingPlanRadioButton.Checked = false;
                        _directoryNameinXmlFile = null;
                        cancelButton.Visible = true;
                        cancelButton.Text = "Close";
                        offlineSyncExportRadioButton.Checked = false;
                        newProtectionRadioButton.Checked = false;
                        pushAgentRadioButton.Checked = false;
                        manageExistingPlanRadioButton.Checked = false;
                        _directoryNameinXmlFile = null;
                        taskGroupBox.Enabled = false;
                        planNamesTreeView.Nodes.Clear();
                        planNamesTreeView.Nodes.Clear();
                        progressBar.Visible = true;
                        getPlansBackgroundWorker.RunWorkerAsync();
                    }
                    else
                    {
                        MessageBox.Show("There are no VM(s) to perform Offlinesync Import", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    }
                }

                if (volumeReSizeRadioButton.Checked == true)
                {
                    bool clutserMachine = false;
                    foreach (Host h in sourceHostsList._hostList)
                    {
                        if (h.cluster == "yes")
                        {
                            clutserMachine = true;
                        }
                    }

                    if (clutserMachine == true)
                    {
                        if (ClusterReadinessChecks() == false)
                        {
                            Trace.WriteLine(DateTime.Now + "\t User has not selected all clutser nodes ");
                            return;
                        }
                    }
                    ResizeOperation();
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
        }


        private bool ResizeOperation()
        {

            if (selectedVMListForPlan._hostList.Count == 0)
            {
                MessageBox.Show("You have not selected machines for Resize option", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return true;
            }

            _directoryNameinXmlFile = null;
            string cxip = WinTools.GetcxIP();
            SolutionConfig sol = new SolutionConfig();
            Esx target = new Esx();
            foreach (Host h in masterTargetList._hostList)
            {
                target.configurationList = h.configurationList;
                target.dataStoreList = h.dataStoreList;
                target.lunList = h.lunList;
                target.networkList = h.networkNameList;

            }
            sol.WriteXmlFile(selectedVMListForPlan, masterTargetList, target, cxip, "Resize.xml", "Resize", true);
            Trace.WriteLine(DateTime.Now + "\t Cam ehere to modify the xml file ");
            //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
            //based on above comment we have added xmlresolver as null
            XmlDocument document1 = new XmlDocument();
            document1.XmlResolver = null;
            string xmlfile1 = latestFilePath + "\\Resize.xml";

            //StreamReader reader = new StreamReader(xmlfile1);
            //string xmlString = reader.ReadToEnd();
            XmlReaderSettings settings = new XmlReaderSettings();
            settings.ProhibitDtd = true;
            settings.XmlResolver = null;
            //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
            using (XmlReader reader1 = XmlReader.Create(xmlfile1, settings))
            {
                document1.Load(reader1);
               // reader1.Close();
            }
            //document1.Load(xmlfile1);
            //reader.Close();
            //XmlNodeList hostNodes = null;
            XmlNodeList rootNodes1 = null, hostNodes1 = null, diskNodes = null;
            hostNodes1 = document1.GetElementsByTagName("host");
            
            diskNodes = document1.GetElementsByTagName("disk");
            XmlNodeList rootNodes = null, hostNodes = null;
            rootNodes = document1.GetElementsByTagName("root");
            Host h1 = (Host)selectedVMListForPlan._hostList[0];
            string planname = null;
            planname = h1.plan;
            if (_directoryNameinXmlFile == null)
            {
                Random _r = new Random();
                int n = _r.Next(999999);
                _directoryNameinXmlFile = planname+"_" + "Resize" + "_" + n.ToString();

            }
            foreach (XmlNode node in rootNodes)
            {
                node.Attributes["xmlDirectoryName"].Value = _directoryNameinXmlFile;
                node.Attributes["batchresync"].Value = h1.batchResync.ToString();                
            }
            document1.Save(xmlfile1);
            if (File.Exists(latestFilePath + "\\Resize.xml"))
            {
                FileInfo destFile = new FileInfo(fxInstallPath + "\\" + _directoryNameinXmlFile + "\\Resize.xml");
                if (destFile.Directory != null && !destFile.Directory.Exists)
                {
                    destFile.Directory.Create();
                }
                WinTools win = new WinTools();
                File.Copy(latestFilePath + "\\Resize.xml", destFile.ToString(), true);
                win.SetFilePermissions(fxInstallPath + "\\" + _directoryNameinXmlFile + "\\Resize.xml");
            }
            if (AllServersForm.SetFolderPermissions(fxInstallPath + "\\" + _directoryNameinXmlFile) == true)
            {
                Trace.WriteLine(DateTime.Now + "\t Successfully set permissions for the folder ");
            }
            else
            {
                Trace.WriteLine(DateTime.Now + "\t Failed to set permission for the folder ");
            }
            Host tempMasterHost = (Host)masterTargetList._hostList[0];
            StatusForm status = new StatusForm(selectedVMListForPlan, tempMasterHost, AppName.Resize, _directoryNameinXmlFile);
            progressBar.Visible = false;
            resumeButton.Visible = false;
            resumeButton.Enabled = true;
            cancelButton.Enabled = true;
            planNamesTreeView.Enabled = true;
            cancelButton.Visible = true;
            cancelButton.Text = "Close";
            offlineSyncExportRadioButton.Checked = false;
            newProtectionRadioButton.Checked = false;
            pushAgentRadioButton.Checked = false;
            manageExistingPlanRadioButton.Checked = false;
            _directoryNameinXmlFile = null;
            taskGroupBox.Enabled = false;
            planNamesTreeView.Nodes.Clear();
            progressBar.Visible = true;
            getPlansBackgroundWorker.RunWorkerAsync();
            return true;
        }

        private void SetLeftTicker(string s)
        {
            try
            {
                _textBox.AppendText(s);
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }
        internal bool RunningScriptInLinuxMt(Host masterHost)
        {
            try
            {
                Cxapicalls cxapi = new Cxapicalls();
                cxapi.Post( masterHost, "InsertScriptPolicy");
                masterHost = Cxapicalls.GetHost;
                if (masterHost.requestId == null)
                {
                    Trace.WriteLine(DateTime.Now + "\t Failed to run the scrips linux_script.sh");
                    return false;
                }
                else
                {
                    Trace.WriteLine(DateTime.Now + "\t Printing the request id " + masterHost.requestId);
                }
                Trace.WriteLine(DateTime.Now + "\t Going to sleep for 1 min for first time after executing InsertScriptPolicy");
                Thread.Sleep(60000);
                Host h = new Host();
                h.requestId = masterHost.requestId;
                try
                {
                    if (cxapi.PostRequestStatusForDummyDisk( h,false) == false)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Going to sleep for 1 min for second time after executing InsertScriptPolicy");
                        Thread.Sleep(60000);
                        if (cxapi.PostRequestStatusForDummyDisk( h,false) == false)
                        {
                            Trace.WriteLine(DateTime.Now + "\t Going to sleep for 1 min for third time after executing InsertScriptPolicy");
                            Thread.Sleep(60000);
                            if (cxapi.PostRequestStatusForDummyDisk( h,false) == false)
                            {
                                Trace.WriteLine(DateTime.Now + "\t Going to sleep for 1 min for fourth time after executing InsertScriptPolicy");
                                Thread.Sleep(60000);
                                if (cxapi.PostRequestStatusForDummyDisk( h,false) == false)
                                {
                                    Trace.WriteLine(DateTime.Now + "\t Going to sleep for 1 min for fifth time after executing InsertScriptPolicy");
                                    Thread.Sleep(60000);
                                    if (cxapi.PostRequestStatusForDummyDisk( h,false) == false)
                                    {
                                        Trace.WriteLine(DateTime.Now + "\t Failed to get the status after waiting 5 mins so continuing with out scanning");
                                    }
                                }
                            }
                        }
                    }
                    h = Cxapicalls.GetHost;
                }
                catch (Exception ex)
                {
                    Trace.WriteLine(DateTime.Now + "\t Failed to get the status ");

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

        private bool VerfiyCxhasCreds(string esxIP, string esxUsername, string esxPassword, string type)
        {
            Cxapicalls cxAPi = new Cxapicalls();
            cxAPi.GetHostCredentials(esxIP);
            if (Cxapicalls.GetUsername != null && Cxapicalls.GetPassword != null)
            {
                Trace.WriteLine(DateTime.Now + "\t CX has target esx creds ");
            }
            else
            {
                if (esxUsername == null && esxPassword == null)
                {
                    SourceEsxDetailsPopUpForm source = new SourceEsxDetailsPopUpForm();
                    if (type == "source")
                    {
                        source.ipLabel.Text = "Source vCenter/vSphere IP/Name *";
                    }
                    else
                    {
                        source.ipLabel.Text = "Target vCenter/vSphere IP/Name *";
                    }
                    source.sourceEsxIpText.ReadOnly = false;
                    source.ShowDialog();
                    if (source.canceled == false)
                    {
                        esxUsername = source.userNameText.Text;
                        esxPassword = source.passWordText.Text;
                        if (type != "source")
                        {
                            foreach (Host h1 in selectedVMListForPlan._hostList)
                            {
                                h1.targetESXIp = source.sourceEsxIpText.Text;
                                h1.targetESXUserName = source.userNameText.Text;
                                h1.targetESXPassword = source.passWordText.Text;
                            }
                            foreach (Host targetHost in masterTargetList._hostList)
                            {
                                targetHost.esxIp = source.sourceEsxIpText.Text;
                                targetHost.esxUserName = source.userNameText.Text;
                                targetHost.esxPassword = source.passWordText.Text;
                            }
                        }
                        else
                        {
                            foreach (Host h1 in selectedVMListForPlan._hostList)
                            {
                                h1.esxIp = source.sourceEsxIpText.Text;
                                h1.esxUserName = source.userNameText.Text;
                                h1.esxPassword = source.passWordText.Text;
                            }
                        }
                    }
                }
                if (cxAPi. UpdaterCredentialsToCX(esxIP, esxUsername, esxPassword) == true)
                {
                    Trace.WriteLine(DateTime.Now + "\t Updated creds to cx");
                    cxAPi.GetHostCredentials(esxIP);
                    if (Cxapicalls.GetUsername != null && Cxapicalls.GetPassword != null)
                    {
                        Trace.WriteLine(DateTime.Now + "\t CX has target esx creds ");
                    }
                    else
                    {
                        Trace.WriteLine(DateTime.Now + "\t CX don't have esx creds");
                        return false;
                    }
                }

            }
            return true;
        }


        internal bool PreScriptRun()
        {
            try
            {
                //Resume button click will call this method....
                if (resumeProtectionRadioButton.Checked == true)
                {
                    tickerDelegate = new TickerDelegate(SetLeftTicker);
                    Host h = (Host)selectedVMListForPlan._hostList[0];
                    Host tempMasterHost = new Host();
                    tempMasterHost.hostname = h.masterTargetHostName;
                    tempMasterHost.displayname = h.displayname;
                    Esx _esxInfo = new Esx();
                    messages = "Resume protection will take few minutes. Started: "+ DateTime.Now + Environment.NewLine;

                    if (checkingForVmsPowerStatus == false)
                    {
                        logTextBox.Invoke(tickerDelegate, new object[] { messages });
                        {
                            WinPreReqs win = new WinPreReqs("", "", "", "");
                            Trace.WriteLine(DateTime.Now + "\t Printing mt host name and ip " + tempMasterHost.hostname + tempMasterHost.ip);
                            if (win.MasterTargetPreCheck( tempMasterHost, WinTools.GetcxIPWithPortNumber()) == true)
                            {
                                tempMasterHost = WinPreReqs.GetHost;
                                if (tempMasterHost.machineInUse == true)
                                {
                                    Trace.WriteLine(DateTime.Now + " \t Came here to know that mt is in use");
                                    string message = "This VM can't be used as master target. Some Fx jobs are running on this master target." + Environment.NewLine
                                        + "Select another master target";
                                    MessageBox.Show(message, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                    return false;
                                }
                            }
                        }
                        Host tempHost = (Host)selectedVMListForPlan._hostList[0];
                        try
                        {
                            VerfiyCxhasCreds(tempHost.targetESXIp, tempHost.targetESXUserName, tempHost.targetESXPassword, "target");
                            int returnValue = _esxInfo.GetEsxInfoWithVmdks(tempHost.targetESXIp, Role.Secondary, tempHost.os);
                            if (returnValue == 3)
                            {
                                SourceEsxDetailsPopUpForm source = new SourceEsxDetailsPopUpForm();
                                source.ipLabel.Text = "Target vCenter/vSphere IP/Name *";
                                source.sourceEsxIpText.ReadOnly = false;
                                source.ShowDialog();
                                if (source.canceled == false)
                                {
                                    h.targetESXIp = source.sourceEsxIpText.Text;
                                    h.targetESXUserName = source.userNameText.Text;
                                    h.targetESXPassword = source.passWordText.Text;
                                    foreach (Host h1 in selectedVMListForPlan._hostList)
                                    {
                                        h1.targetESXIp = source.sourceEsxIpText.Text;
                                        h1.targetESXUserName = source.userNameText.Text;
                                        h1.targetESXPassword = source.passWordText.Text;
                                    }
                                    foreach (Host targetHost in masterTargetList._hostList)
                                    {
                                        targetHost.esxIp = source.sourceEsxIpText.Text;
                                        targetHost.esxUserName = source.userNameText.Text;
                                        targetHost.esxPassword = source.passWordText.Text;
                                    }
                                    Cxapicalls cxapi = new Cxapicalls();
                                    cxapi. UpdaterCredentialsToCX(h.targetESXIp, h.targetESXUserName, h.targetESXPassword);
                                    returnValue = _esxInfo.GetEsxInfoWithVmdks(h.targetESXIp, Role.Secondary, h.os);
                                    if (returnValue != 0)
                                    {
                                        messages = "Failed to fetch the information from target vSphere server. Failed:"+ DateTime.Now + Environment.NewLine;
                                        logTextBox.Invoke(tickerDelegate, new object[] { messages });
                                        Trace.WriteLine(DateTime.Now + " \t Failed to get the MasterXMl.xml");
                                        return false;
                                    }
                                    else
                                    {
                                        string listOfVmsToPowerOff = null;
                                        foreach (Host selectedHost in selectedVMListForPlan._hostList)
                                        {
                                            foreach (Host targetHost in _esxInfo.GetHostList)
                                            {
                                                if (selectedHost.target_uuid != null)
                                                {

                                                    if (selectedHost.target_uuid == targetHost.source_uuid)
                                                    {
                                                        if (selectedHost.alt_guest_name == null)
                                                        {
                                                            selectedHost.alt_guest_name = targetHost.alt_guest_name;

                                                        }
                                                        if (selectedHost.vmxversion == null)
                                                        {
                                                            selectedHost.vmxversion = targetHost.vmxversion;
                                                        }
                                                        if (targetHost.poweredStatus == "ON")
                                                        {
                                                            if (listOfVmsToPowerOff == null || listOfVmsToPowerOff.Length == 0)
                                                            {
                                                                listOfVmsToPowerOff = "\"" + targetHost.source_uuid;
                                                            }
                                                            else
                                                            {
                                                                listOfVmsToPowerOff = listOfVmsToPowerOff + "!@!@!" + targetHost.source_uuid;
                                                            }
                                                        }
                                                    }
                                                }
                                                else
                                                {
                                                    if (selectedHost.new_displayname == targetHost.displayname)
                                                    {
                                                        if (selectedHost.alt_guest_name == null)
                                                        {
                                                            selectedHost.alt_guest_name = targetHost.alt_guest_name;

                                                        }
                                                        if (selectedHost.vmxversion == null)
                                                        {
                                                            selectedHost.vmxversion = targetHost.vmxversion;
                                                        }
                                                        if (targetHost.poweredStatus == "ON")
                                                        {
                                                            if (listOfVmsToPowerOff == null || listOfVmsToPowerOff.Length == 0)
                                                            {
                                                                listOfVmsToPowerOff = "\"" + targetHost.source_uuid;
                                                            }
                                                            else
                                                            {
                                                                listOfVmsToPowerOff = listOfVmsToPowerOff + "!@!@!" + targetHost.source_uuid;
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                        if (listOfVmsToPowerOff != null)
                                        {
                                            listOfVmsToPowerOff = listOfVmsToPowerOff + "\"";
                                            switch (MessageBox.Show("Some of the VM(s) selected to resume is powered on on target. Shall we shutdown the VM(s)", "Power Off ", MessageBoxButtons.YesNo, MessageBoxIcon.Warning))
                                            {
                                                case DialogResult.No:
                                                    MessageBox.Show("With out power off target vm we can't proceed Exiting....");
                                                    break;
                                                case DialogResult.Yes:
                                                    if (_esxInfo.PowerOffVM(h.targetESXIp, listOfVmsToPowerOff) != 0)
                                                    {
                                                        updateLogTextBox(logTextBox);
                                                        return false;
                                                    }
                                                    else
                                                    {
                                                        checkingForVmsPowerStatus = true;
                                                    }
                                                    break;
                                            }
                                        }
                                        else
                                        {
                                            checkingForVmsPowerStatus = true;
                                        }
                                    }
                                }
                                else
                                {
                                    return false;
                                }

                            }
                            else if (returnValue == 0)
                            {
                                string listOfVmsToPowerOff = null;
                                foreach (Host selectedHost in selectedVMListForPlan._hostList)
                                {
                                    foreach (Host targetHost in _esxInfo.GetHostList)
                                    {
                                        if (selectedHost.target_uuid != null)
                                        {
                                            if (selectedHost.target_uuid == targetHost.source_uuid)
                                            {
                                                if (selectedHost.alt_guest_name == null)
                                                {
                                                    selectedHost.alt_guest_name = targetHost.alt_guest_name;
                                                }

                                                if (selectedHost.vmxversion == null)
                                                {
                                                    selectedHost.vmxversion = targetHost.vmxversion;
                                                }
                                                if (targetHost.poweredStatus == "ON")
                                                {
                                                    if (listOfVmsToPowerOff == null || listOfVmsToPowerOff.Length == 0)
                                                    {
                                                        listOfVmsToPowerOff = "\"" + targetHost.source_uuid;
                                                    }
                                                    else
                                                    {
                                                        listOfVmsToPowerOff = listOfVmsToPowerOff + "!@!@!" + targetHost.source_uuid;
                                                    }
                                                }
                                            }
                                        }
                                        else
                                        {
                                            if (selectedHost.new_displayname == targetHost.displayname)
                                            {
                                                if (selectedHost.alt_guest_name == null)
                                                {
                                                    selectedHost.alt_guest_name = targetHost.alt_guest_name;

                                                }
                                                if (selectedHost.vmxversion == null)
                                                {
                                                    selectedHost.vmxversion = targetHost.vmxversion;
                                                }
                                                if (targetHost.poweredStatus == "ON")
                                                {
                                                    if (listOfVmsToPowerOff == null || listOfVmsToPowerOff.Length == 0)
                                                    {
                                                        listOfVmsToPowerOff = "\"" + targetHost.source_uuid;
                                                    }
                                                    else
                                                    {
                                                        listOfVmsToPowerOff = listOfVmsToPowerOff + "!@!@!" + targetHost.source_uuid;
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }

                                if (listOfVmsToPowerOff != null)
                                {
                                    listOfVmsToPowerOff = listOfVmsToPowerOff + "\"";
                                    switch (MessageBox.Show("Some of the VM(s) selected to resume is powered on on target. Shall we shutdown the VM(s)", "Power Off ", MessageBoxButtons.YesNo, MessageBoxIcon.Warning))
                                    {
                                        case DialogResult.No:
                                            MessageBox.Show("With out power off target vm we can't proceed Exiting....");
                                            return false;                                           
                                        case DialogResult.Yes:
                                            if (_esxInfo.PowerOffVM(h.targetESXIp, listOfVmsToPowerOff) != 0)
                                            {
                                                updateLogTextBox(logTextBox);
                                                return false;
                                            }
                                            else
                                            {
                                                checkingForVmsPowerStatus = true;
                                            }
                                            break;
                                    }
                                }
                                else
                                {
                                    checkingForVmsPowerStatus = true;
                                }
                            }
                            else
                            {
                                messages = "Failed to get the target information. Failed:"+ DateTime.Now + Environment.NewLine;
                                logTextBox.Invoke(tickerDelegate, new object[] { messages });
                                updateLogTextBox(logTextBox);
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
                    }

                    try
                    {
                        foreach (Host selectedHost in selectedVMListForPlan._hostList)
                        {
                            foreach (Host targetHost in _esxInfo.GetHostList)
                            {
                                if (selectedHost.target_uuid != null)
                                {
                                    if (selectedHost.target_uuid == targetHost.source_uuid)
                                    {
                                        if (selectedHost.alt_guest_name == null)
                                        {
                                            selectedHost.alt_guest_name = targetHost.alt_guest_name;
                                        }
                                        if (selectedHost.vmxversion == null)
                                        {
                                            selectedHost.vmxversion = targetHost.vmxversion;
                                        }
                                    }
                                    else
                                    {
                                        if (selectedHost.displayname == targetHost.displayname)
                                        {
                                            if (selectedHost.alt_guest_name == null)
                                            {
                                                selectedHost.alt_guest_name = targetHost.alt_guest_name;
                                            }
                                            if (selectedHost.vmxversion == null)
                                            {
                                                selectedHost.vmxversion = targetHost.vmxversion;
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


                    Host masterHost = (Host)masterTargetList._hostList[0];
                    foreach (Host targetHost in _esxInfo.GetHostList)
                    {
                        if (masterHost.source_uuid != null)
                        {
                            if (masterHost.source_uuid == targetHost.source_uuid)
                            {
                                masterHost.ide_count = targetHost.ide_count;
                                break;
                            }
                        }
                        if (masterHost.displayname == targetHost.displayname || masterHost.hostname == targetHost.hostname)
                        {
                            masterHost.ide_count = targetHost.ide_count;
                            break;
                        }
                    }


                    // This is added  because of due to support of upgrade.
                    //In case from upgrade from earlier versions to 3.0 we need to fill alt_guest name and vmx version.
                    try
                    {
                        Trace.WriteLine(DateTime.Now + "\t Cam ehere to modify the xml file ");
                        //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
                        //based on above comment we have added xmlresolver as null
                        XmlDocument document1 = new XmlDocument();
                        document1.XmlResolver = null;
                        string xmlfile1 = latestFilePath + "\\ESX.xml";

                        //StreamReader reader = new StreamReader(xmlfile1);
                        //string xmlString = reader.ReadToEnd();
                        XmlReaderSettings settings = new XmlReaderSettings();
                        settings.ProhibitDtd = true;
                        settings.XmlResolver = null;
                        //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
                        using (XmlReader reader1 = XmlReader.Create(xmlfile1, settings))
                        {
                            document1.Load(reader1);
                           // reader1.Close();
                        }
                        //document1.Load(xmlfile1);
                        //reader.Close();
                        //XmlNodeList hostNodes = null;
                        XmlNodeList rootNodes1 = null, hostNodes1 = null;
                        hostNodes1 = document1.GetElementsByTagName("host");
                        rootNodes1 = document1.GetElementsByTagName("root");
                        foreach (Host h1 in selectedVMListForPlan._hostList)
                        {
                            if (h1.inmage_hostid == null)
                            {
                                Host tempHost1 = h1;
                                string cxip = WinTools.GetcxIPWithPortNumber();
                                WinPreReqs win = new WinPreReqs("", "", "", "");
                                if (win.GetDetailsFromcxcli( tempHost1, cxip) == true)
                                {
                                    tempHost1 = WinPreReqs.GetHost;
                                    h1.inmage_hostid = tempHost1.inmage_hostid;
                                }

                            }
                            Host tempHost = h1;
                            Cxapicalls cxApis = new Cxapicalls();
                            cxApis.Post( h, "GetHostInfo");
                            h = Cxapicalls.GetHost;
                            if (tempHost.mbr_path != null)
                            {
                                h1.mbr_path = tempHost.mbr_path;
                                h1.clusterMBRFiles = tempHost.clusterMBRFiles;
                            }
                        }
                       Host cxHost = new Host();
                        cxHost.ip = WinTools.GetcxIP();
                        string cxIPwithPortNumber = WinTools.GetcxIPWithPortNumber();
                        if (WinPreReqs.SetHostNameInputIP(cxHost.ip,  cxHost.hostname, WinTools.GetcxIPWithPortNumber()) == true)
                        {
                            cxHost.hostname = WinPreReqs.GetHostname;
                            WinPreReqs wp = new WinPreReqs("", "", "", "");
                            int returnCode = wp.SetHostIPinputhostname(cxHost.hostname,  cxHost.ip, WinTools.GetcxIPWithPortNumber());
                            cxHost.ip = WinPreReqs.GetIPaddress;
                            if (wp.GetDetailsFromcxcli( cxHost, cxIPwithPortNumber) == true)
                            {
                                cxHost = WinPreReqs.GetHost;
                            }
                        }
                        try
                        {
                            foreach (XmlNode node in rootNodes1)
                            {
                                if (node.Attributes["inmage_cx_uuid"] != null)
                                {
                                    node.Attributes["inmage_cx_uuid"].Value = cxHost.inmage_hostid;
                                }
                                else
                                {
                                    XmlElement ele = (XmlElement)node;
                                    ele.SetAttribute("inmage_cx_uuid", cxHost.inmage_hostid);
                                }
                                Trace.WriteLine(DateTime.Now + "\t Updated the cx uuid " + cxHost.inmage_hostid);
                            }
                        }
                        catch (Exception ex)
                        {
                            Trace.WriteLine(DateTime.Now + "\t Failed to add cx uuid in esx.xml " + ex.Message); 
                        }
                        foreach (Host h1 in selectedVMListForPlan._hostList)
                        {
                            foreach (XmlNode node in hostNodes1)
                            {
                                if (h1.displayname == node.Attributes["display_name"].Value)
                                {
                                    if (node.Attributes["alt_guest_name"].Value.Length < 3)
                                    {
                                        node.Attributes["alt_guest_name"].Value = h1.alt_guest_name;
                                        Trace.WriteLine(DateTime.Now + "\t  Prinitng the alt guest values " + h1.alt_guest_name + " \t vmx version " + h1.vmxversion);
                                    }
                                    if (node.Attributes["vmx_version"].Value.Length < 2)
                                    {
                                        node.Attributes["vmx_version"].Value = h1.vmxversion;
                                    }
                                    if (node.Attributes["inmage_hostid"] != null && node.Attributes["inmage_hostid"].Value.Length == 0)
                                    {
                                        node.Attributes["inmage_hostid"].Value = h1.inmage_hostid;
                                    }
                                    else
                                    {
                                        XmlElement ele = (XmlElement)node;
                                        ele.SetAttribute("inmage_hostid", h1.inmage_hostid);

                                    }

                                    // This is to support earlier protection. That too we are only checking for the Windows protection.
                                    //
                                    if (h1.os == OStype.Windows)
                                    {
                                        if (node.Attributes["mbr_path"] != null && node.Attributes["mbr_path"].Value.Length == 0)
                                        {
                                            Trace.WriteLine(DateTime.Now + "\t It seems mbr is null. Mean to say it is earlier protection.");
                                            Trace.WriteLine(DateTime.Now + "\t MBR value is " + h1.mbr_path);
                                            node.Attributes["mbr_path"].Value = h1.mbr_path;
                                        }
                                    }
                                    //try
                                    //{

                                    //    Trace.WriteLine(DateTime.Now + "\t Printing old mbr path " + node.Attributes["mbr_path"].Value);
                                    //    node.Attributes["mbr_path"].Value = h1.mbr_path;
                                    //    Trace.WriteLine(DateTime.Now + "\t Updated latest mbr path " + h1.mbr_path);
                                    //}
                                    //catch (Exception ex)
                                    //{
                                    //    Trace.WriteLine(DateTime.Now + "\t Failed to update latest mbr " + ex.Message);
                                    //}
                                }
                            }
                        }
                        foreach (XmlNode node in hostNodes1)
                        {
                            if (node.Attributes["display_name"].Value == masterHost.displayname)
                            {
                                if (node.Attributes["ide_count"].Value.Length == 0)
                                {
                                    node.Attributes["ide_count"].Value = masterHost.ide_count;
                                }
                                if (node.Attributes["inmage_hostid"] != null && node.Attributes["inmage_hostid"].Value.Length == 0)
                                {
                                    node.Attributes["inmage_hostid"].Value = masterHost.inmage_hostid;
                                }
                            }
                        }
                        document1.Save(xmlfile1);
                    }
                    catch (Exception ex)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Failed to update esx.xml with some issue " + ex.Message);
                    }


                    try
                    {
                        RefreshHosts(selectedVMListForPlan);
                    }
                    catch (Exception ex)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Failed to refresh hosts  " + ex.Message);
                    }


                    if (readinessCheck == false)
                    {
                        if (VerfiyCxhasCreds(h.targetESXIp, h.targetESXUserName, h.targetESXPassword, "target") == false)
                        {
                            MessageBox.Show("Failed to update secondary vSphere\vCenter server credentials to CX Database", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error); ;
                            return false;
                        }
                        int returnValue = _esxInfo.PreReqCheckForResume(h.targetESXIp);
                        if (returnValue == 3)
                        {
                            SourceEsxDetailsPopUpForm source = new SourceEsxDetailsPopUpForm();
                            source.ipLabel.Text = "Target vCenter/vSphere IP/Name *";
                            source.sourceEsxIpText.ReadOnly = false;
                            source.ShowDialog();
                            h.targetESXIp = source.sourceEsxIpText.Text;
                            h.targetESXUserName = source.userNameText.Text;
                            h.targetESXPassword = source.passWordText.Text;
                            Cxapicalls cxapis = new Cxapicalls();
                            cxapis. UpdaterCredentialsToCX(h.targetESXIp, h.targetESXUserName, h.targetESXPassword);
                             returnValue = _esxInfo.PreReqCheckForResume(h.targetESXIp);
                             if (returnValue != 0)
                             {
                                 messages = "Readiness check failed. " + DateTime.Now + Environment.NewLine;
                                 logTextBox.Invoke(tickerDelegate, new object[] { messages });
                                 readinessCheck = false;
                                 return false;

                             }
                        }
                        else if (returnValue == 0)
                        {
                            messages = "Successfully completed pre-req checks. Completed:" +DateTime.Now + Environment.NewLine;
                            logTextBox.Invoke(tickerDelegate, new object[] { messages });
                            readinessCheck = true;
                        }
                        else
                        {
                            messages = "Readiness check failed. "+ DateTime.Now + Environment.NewLine;
                            logTextBox.Invoke(tickerDelegate, new object[] { messages });
                            readinessCheck = false;
                            return false;
                        }
                    }
                    if (dummyDiskscreated == false)
                    {// In case of linux we are calling linux script beofore creating the dummy disk calls.
                        // Because using cent os 6.2 we are not getting all the disks.
                        // For that we have changed the scripts also. By calling this before creating the dummy disks.
                        // We are calling this linux script.
                        string cxip = WinTools.GetcxIPWithPortNumber();
                        _masterHost = (Host)masterTargetList._hostList[0];
                        if (_masterHost.os == OStype.Linux)
                        {
                            _masterHost.requestId = null;
                            if (_masterHost.vx_agent_path == null )
                            {
                                WinPreReqs win = new WinPreReqs("", "", "", "");
                                win.GetDetailsFromcxcli( _masterHost, cxip);
                                _masterHost = WinPreReqs.GetHost;
                            }
                            RunningScriptInLinuxMt(_masterHost);
                        }
                        if (_esxInfo.ExecuteDummyDisksScript(h.targetESXIp) == 0)
                        {
                            if (_masterHost.os == OStype.Linux)
                            {
                                if (_masterHost.vx_agent_path == null )
                                {
                                    WinPreReqs win = new WinPreReqs("", "", "", "");
                                    win.GetDetailsFromcxcli( _masterHost, cxip);
                                    _masterHost = WinPreReqs.GetHost;
                                }
                                masterHost.requestId = null;
                                RunningScriptInLinuxMt(_masterHost);
                            }
                            dummyDiskscreated = true;
                            try
                            {
                                _masterHost = (Host)masterTargetList._hostList[0];
                               
                                //if (_masterHost.userName != null)
                                {
                                    //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
                                    //based on above comment we have added xmlresolver as null
                                    XmlDocument document = new XmlDocument();
                                    document.XmlResolver = null;
                                    string xmlfile = latestFilePath + "\\ESX.xml";
                                    //StreamReader reader = new StreamReader(xmlfile);
                                    //string xmlString = reader.ReadToEnd();
                                    XmlReaderSettings settings = new XmlReaderSettings();
                                    settings.ProhibitDtd = true;
                                    settings.XmlResolver = null;
                                    //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
                                    using (XmlReader reader1 = XmlReader.Create(xmlfile, settings))
                                    {
                                        document.Load(reader1);
                                        //reader1.Close();
                                    }
                                    //document.Load(xmlfile);
                                    //reader.Close();
                                    //XmlNodeList hostNodes = null;
                                    XmlNodeList targetNodes = null;
                                    XmlNodeList dummyNodes = null;
                                    XmlNodeList mtdiskNodes = null;
                                    targetNodes = document.GetElementsByTagName("TARGET_ESX");
                                    dummyNodes = document.GetElementsByTagName("dummy_disk");
                                    mtdiskNodes = document.GetElementsByTagName("MT_Disk");
                                    foreach (XmlNode nodesofTarget in targetNodes)
                                    {
                                        if (nodesofTarget.SelectSingleNode("dummy_disk") != null)
                                        {
                                            Host tempHost = new Host();
                                            tempHost.inmage_hostid = _masterHost.inmage_hostid;
                                            Trace.WriteLine(DateTime.Now + "\t Printing the mt host id " + h.inmage_hostid);
                                            tempHost.ip = _masterHost.ip;
                                            tempHost.userName = _masterHost.userName;
                                            tempHost.passWord = _masterHost.passWord;
                                            tempHost.domain = _masterHost.domain;
                                            tempHost.hostname = _masterHost.hostname;

                                            Cxapicalls cxApi = new Cxapicalls();
                                            if (cxApi.Post( tempHost, "RefreshHostInfo") == true)
                                            {
                                                tempHost = Cxapicalls.GetHost;
                                                Trace.WriteLine(DateTime.Now + "\t Printing the request id " + tempHost.requestId);
                                                Trace.WriteLine(DateTime.Now + "\t Going to sleep for 1 min  first time");
                                                Thread.Sleep(65000);

                                                if (cxApi.Post( tempHost, "GetRequestStatus") == false)
                                                {
                                                    Trace.WriteLine(DateTime.Now + "\t Going to sleep for 1 min  second time");
                                                    Thread.Sleep(65000);
                                                    if (cxApi.Post( tempHost, "GetRequestStatus") == false)
                                                    {
                                                        Trace.WriteLine(DateTime.Now + "\t Going to sleep for 1 min  third time");
                                                        Thread.Sleep(65000);
                                                        if (cxApi.Post( tempHost, "GetRequestStatus") == false)
                                                        {
                                                            Trace.WriteLine(DateTime.Now + "\t Going to sleep for 1 min  fourth time");
                                                            Thread.Sleep(65000);
                                                            if (cxApi.Post( tempHost, "GetRequestStatus") == false)
                                                            {
                                                                Trace.WriteLine(DateTime.Now + "\t Going to sleep for 1 min  fifth time");
                                                                Thread.Sleep(65000);
                                                                if (cxApi.Post( tempHost, "GetRequestStatus") == false)
                                                                {
                                                                    Trace.WriteLine(DateTime.Now + "\t after waiting 5 mins we didn't get the refresh host status ");
                                                                }
                                                            }
                                                        }

                                                    }
                                                }
                                                tempHost = Cxapicalls.GetHost;
                                                ArrayList physicalDisks;
                                                physicalDisks = tempHost.disks.GetDiskList;
                                                foreach (XmlNode node in mtdiskNodes)
                                                {
                                                    foreach (Hashtable disk in physicalDisks)
                                                    {
                                                        Trace.WriteLine(DateTime.Now + " \t comparing both the sizes " + node.Attributes["size"].Value.ToString() + " " + disk["ActualSize"].ToString());
                                                        if (node.Attributes["size"].Value.ToString() == disk["ActualSize"].ToString())
                                                        {
                                                            if (_masterHost.os == OStype.Linux)
                                                            {

                                                                if (disk["ScsiBusNumber"] != null)
                                                                {
                                                                    Trace.WriteLine(DateTime.Now + "Printing ScsiBusNumber values " + disk["ScsiBusNumber"].ToString());
                                                                    node.Attributes["port_number"].Value = disk["ScsiBusNumber"].ToString();
                                                                }
                                                            }
                                                            else
                                                            {
                                                                if (disk["port_number"] != null)
                                                                {
                                                                    Trace.WriteLine(DateTime.Now + "Printing port number values " + disk["port_number"].ToString());
                                                                    node.Attributes["port_number"].Value = disk["port_number"].ToString();
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                                dummyDiskscreated = true;
                                                document.Save(xmlfile);

                                            }

                                        }
                                        else
                                        {
                                            Trace.WriteLine(DateTime.Now + "\t There are no dummy disks present in esx.xml");
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
                        }
                        else
                        {
                            Trace.WriteLine(DateTime.Now + "Failed to add dummydisks to the master target");
                            dummyDiskscreated = false;
                            return false;
                        }
                    }
                    if (createGuestOnTarget == false)
                    {
                        if (_esxInfo.ExecuteCreateTargetGuestScript(h.targetESXIp, OperationType.Resume) == 0)
                        {
                            createGuestOnTarget = true;
                            if (File.Exists(installPath + "\\folder_delete.bat"))
                            {
                                //Executing the batch script to delete the folders in vCon folder
                                if (WinTools.Execute(installPath + "\\folder_delete.bat", "", 20000) == 0)
                                {
                                    Trace.WriteLine(DateTime.Now + " \t Successfully completed the deleting folder ");
                                }
                                else
                                {
                                    Trace.WriteLine(DateTime.Now + " \t Failed to delete the folders in 20 seconds.Still continuing...");
                                }
                            }
                            messages = "Successfully completed creating source vm creation on target side. Completed:" + DateTime.Now + Environment.NewLine;
                            logTextBox.Invoke(tickerDelegate, new object[] { messages });
                        }
                        else
                        {
                            createGuestOnTarget = false;
                            return false;
                        }
                    }
                    try
                    {
                        string vconmt = "no";
                        if (_masterHost.hostname.Split('.')[0].ToUpper() != Environment.MachineName.ToUpper())
                        {
                            vconmt = "no";
                        }
                        else
                        {
                            vconmt = "yes";
                        }

                        if (jobautomation == false)
                        {
                            

                        
                            if (_esxInfo.ExecuteJobAutomationScript(OperationType.Resume,vconmt) == 0)
                            {
                                //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
                                //based on above comment we have added xmlresolver as null
                                XmlDocument document = new XmlDocument();
                                document.XmlResolver = null;
                                string xmlfile = latestFilePath + "\\ESX.xml";

                                //StreamReader reader = new StreamReader(xmlfile);
                                //string xmlString = reader.ReadToEnd();
                                XmlReaderSettings settings = new XmlReaderSettings();
                                settings.ProhibitDtd = true;
                                settings.XmlResolver = null;
                                //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
                                using (XmlReader reader1 = XmlReader.Create(xmlfile, settings))
                                {
                                    document.Load(reader1);
                                   // reader1.Close();
                                }
                                //document.Load(xmlfile);
                               // reader.Close();
                                //XmlNodeList hostNodes = null;
                                XmlNodeList hostNodes = null;
                                XmlNodeList diskNodes = null;
                                XmlNodeList rootNodes = null;
                                rootNodes = document.GetElementsByTagName("root");
                                hostNodes = document.GetElementsByTagName("host");
                                diskNodes = document.GetElementsByTagName("disk");
                                HostList sourceList = new HostList();
                                Host masterTempHost = new Host();
                                string esxIP = null;
                                string cxIP = null;
                                SolutionConfig config = new SolutionConfig();
                                config.ReadXmlConfigFile("ESX.xml",   sourceList,  masterTempHost,  esxIP,  cxIP);
                                sourceList = SolutionConfig.list;
                                masterTempHost = SolutionConfig.masterHosts;
                                esxIP = SolutionConfig.EsxIP;
                                cxIP = SolutionConfig.csIP;
                                Trace.WriteLine(DateTime.Now + " \t Completed reading file ");
                                foreach (XmlNode node in diskNodes)
                                {
                                    node.Attributes["protected"].Value = "yes";
                                }
                                Trace.WriteLine(DateTime.Now + " \t Completed making protected disks to yes");
                                Trace.WriteLine(DateTime.Now + "\t Printing the directory name " + _directoryNameinXmlFile);
                                foreach (XmlNode node in hostNodes)
                                {
                                    node.Attributes["directoryName"].Value = _directoryNameinXmlFile;
                                }
                                document.Save(xmlfile);
                                Trace.WriteLine(DateTime.Now + "Completed writing the directory name to all hosts");

                                if (File.Exists(latestFilePath + "\\ESX.xml"))
                                {

                                    File.Copy(latestFilePath + "\\ESX.xml", fxInstallPath + "\\" + _directoryNameinXmlFile + "\\ESX.xml", true);
                                    Trace.WriteLine(DateTime.Now + "\t Successfully copied fixed to the " + fxInstallPath + "\\" + _directoryNameinXmlFile + "\\ESX.xml");

                                }
                                jobautomation = true;
                                if (vconmt == "yes")
                                {
                                    messages = "Successfully completed setting up the consistency jobs. Completed:" + DateTime.Now + Environment.NewLine;
                                    logTextBox.Invoke(tickerDelegate, new object[] { messages });
                                }
                                else
                                {
                                    messages = "Successfully completed setting up the fx jobs. Completed:" + DateTime.Now + Environment.NewLine;
                                    logTextBox.Invoke(tickerDelegate, new object[] { messages });
                                }
                                
                                Trace.WriteLine(DateTime.Now + " \t Successfully to set the fx jobs");
                            }
                            else
                            {
                                jobautomation = false;
                                Trace.WriteLine(DateTime.Now + " \t Failed to set the fx jobs");
                                return false;
                            }
                        }
                        if (jobautomation == true && vconmt == "yes")
                        {

                            try
                            {
                                if (File.Exists(WinTools.FxAgentPath() + "\\Failover\\Data\\" + _directoryNameinXmlFile + "\\protection.log"))
                                {
                                    File.Delete(WinTools.FxAgentPath() + "\\Failover\\Data\\" + _directoryNameinXmlFile + "\\protection.log");
                                }
                            }
                            catch (Exception ex)
                            {
                                Trace.WriteLine(DateTime.Now + "\t Failed to delete the protection.log " + ex.Message);
                            }
                            int returncode = 0;
                           
                            string path = WinTools.FxAgentPath() + "\\Failover\\Data\\" + _directoryNameinXmlFile;
                                returncode = WinTools.ExecuteEsxUtilWinLocally(100000, "target", "\"" +path + "\""  ,  path + "\\protection.log");
                                //returncode = _esx.ExecuteEsxutilWin("target", WinTools.FxAgentPath() + "\\Failover\\Data\\" + _directoryNameinXmlFile);
                           
                            if (returncode == 0)
                            {
                                jobautomation = true;
                                messages = "Successfully completed setting up the volume replication pairs. Completed:"+ DateTime.Now + Environment.NewLine;
                                logTextBox.Invoke(tickerDelegate, new object[] { messages });
                                esxutilwinExecution = true;
                            }
                            else
                            {
                                messages = "Setting up volume repliacation pairs got failed. Check EsxUtilWin.log for exact error" + Environment.NewLine
                                    + "Path of log. " + WinTools.FxAgentPath() + "\\EsxUtlWin.log";
                                
                                logTextBox.Invoke(tickerDelegate, new object[] { messages });
                                esxutilwinExecution = false;
                            }
                        }
                        else
                        {
                            esxutilwinExecution = true;
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
                    if (upLoadFile == false)
                    {

                        // Preparing MasterConfigFile.xml......
                        MasterConfigFile masterCOnfig = new MasterConfigFile();
                        if (masterCOnfig.UpdateMasterConfigFile(latestFilePath + "\\ESX.xml"))
                        {
                            Trace.WriteLine(DateTime.Now + "\t Successfully completed the upload of masterconfigfile.xml ");
                            upLoadFile = true;
                        }
                        else
                        {
                            Trace.WriteLine(DateTime.Now + " \t Failed to upload MasterConfigFile.xml to the target esx box");
                            upLoadFile = false;
                            return false;
                        }

                        try
                        {
                            if (File.Exists(installPath + "\\logs\\vContinuum_UI.log"))
                            {
                                if (_directoryNameinXmlFile != null)
                                {
                                    Program.tr3.Dispose();
                                    System.IO.File.Move(WinTools.FxAgentPath() + "\\vContinuum\\logs\\vContinuum_UI.log", fxInstallPath + "\\" + _directoryNameinXmlFile + "\\vContinuum_UI.log");
                                    Program.tr3.Dispose();
                                }
                            }
                        }
                        catch (Exception ex)
                        {
                            Trace.WriteLine(DateTime.Now + "\t Failed to copy vCOntinuum_UI.log " + ex.Message);
                        }
                        /*if (_upDateMasterFile == false)
                        {
                            MasterConfigFile masterCongif = new MasterConfigFile();
                            if (masterCongif.UpdateMasterConfigFile(MASTER_FILE) == false)
                            {
                                Trace.WriteLine(DateTime.Now + " \t Failed to upload MasterConfig.xml to CX");
                                _upDateMasterFile = false;
                                return false;
                            }
                            else
                            {
                                _upDateMasterFile = true;
                            }
                        }*/
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

        private void backgroundWorker_DoWork(object sender, System.ComponentModel.DoWorkEventArgs e)
        {
            try
            {
                PreScriptRun();
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void backgroundWorker_ProgressChanged(object sender, System.ComponentModel.ProgressChangedEventArgs e)
        {
            progressBar.Value = e.ProgressPercentage;
        }

        private void backgroundWorker_RunWorkerCompleted(object sender, System.ComponentModel.RunWorkerCompletedEventArgs e)
        {
            try
            {
                progressBar.Visible = false;
                planNamesTreeView.Enabled = true;

                if (checkingForVmsPowerStatus == false)
                {
                    logTextBox.AppendText("Failed to power off the VM(s) on target side " + Environment.NewLine);
                    Trace.WriteLine(DateTime.Now + "\t failed to Power  of vms.");
                    cancelButton.Enabled = true;
                    resumeButton.Enabled = true;
                    this.Enabled = true;
                    updateLogTextBox(logTextBox);
                    return;
                }


                if (readinessCheck == false)
                {
                    Trace.WriteLine(DateTime.Now + "\tPre-req checks failed");
                    cancelButton.Enabled = true;
                    resumeButton.Enabled = true;
                    this.Enabled = true;
                    updateLogTextBox(logTextBox);
                    return;
                }
                if (dummyDiskscreated == false)
                {
                    Trace.WriteLine(DateTime.Now + "\tCreation of dummy disks failed.");
                    cancelButton.Enabled = true;
                    resumeButton.Enabled = true;
                    this.Enabled = true;
                    updateLogTextBox(logTextBox);
                    return;

                }
                if (createGuestOnTarget == false)
                {

                    Trace.WriteLine(DateTime.Now + "\t create guest on target failed");
                    cancelButton.Enabled = true;
                    resumeButton.Enabled = true;
                    this.Enabled = true;
                    updateLogTextBox(logTextBox);
                    return;
                }
                if (jobautomation == false)
                {
                    Trace.WriteLine(DateTime.Now + "\t setting up jobs failed");
                    cancelButton.Enabled = true;
                    resumeButton.Enabled = true;
                    this.Enabled = true;
                    updateLogTextBox(logTextBox);
                    return;
                }
                if (esxutilwinExecution == false)
                {
                    Trace.WriteLine(DateTime.Now + "\t setting up volume pairs failed");
                    cancelButton.Enabled = true;
                    resumeButton.Enabled = true;
                    this.Enabled = true;
                    
                    return;

                }
                if (upLoadFile == false)
                {

                    Trace.WriteLine(DateTime.Now + "\t uploading master xml file failed");
                    cancelButton.Enabled = true;
                    resumeButton.Enabled = true;
                    this.Enabled = true;
                    updateLogTextBox(logTextBox);
                    return;
                }
                /* if (_upDateMasterFile == false)
                 {
                     Trace.WriteLine(DateTime.Now + "\t uploading masterConfigFile xml file failed");
                     cancelButton.Enabled = true;
                     resumeButton.Enabled = true;
                     this.Enabled = true;
                     updateLogTextBox(logTextBox);
                     return;
                 }*/

                breifLog.WriteLine(DateTime.Now + "\t SuccessFully completed resume operation for the above selected machines");
                breifLog.Flush();
                logTextBox.AppendText("Successfully completed the resume protection");
                cancelButton.Text = "Done";
                this.Enabled = true;
                resumeButton.Enabled = true;
                resumeButton.Visible = false;
                cancelButton.Enabled = true;
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private bool updateLogTextBox(TextBox generalLogTextBox)
        {
            try
            {
                StreamReader sr1 = new StreamReader(installPath + "\\logs\\vContinuum_ESX.log");
                string firstLine1;
                firstLine1 = sr1.ReadToEnd();

                generalLogTextBox.AppendText(firstLine1);

                sr1.Close();
                sr1.Dispose();
            }
            catch (Exception e)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: updateLogTextBox " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + e.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }

            return true;

        }

        private void cancelButton_Click(object sender, EventArgs e)
        {
            try
            {
                if (cancelButton.Text != "Done")
                {
                    /*Process[] processlist = Process.GetProcesses();

                    foreach (Process theprocess in processlist)
                    {
                        if (theprocess.ProcessName.Contains("vContinuum.vshost"))
                        {
                            Trace.WriteLine("Process: {0} ID: {1}" + theprocess.ProcessName + theprocess.Id);
                            theprocess.Kill();
                        }
                   
                    }*/
                    //FindAndKillProcess();
                    //Close();
                    //Application.Exit();

                    // In some case vContinuum.exe not getting terminated. So we are disposing the base
                    // This will kill all the processes belongs to vcontinuum.

                    try
                    {
                        
                        if (components != null)
                        {
                            components.Dispose();
                        }
                    }
                    catch (Exception ex)
                    {
                        Trace.WriteLine(DateTime.Now + "\t disposing the compents got failed " + ex.Message);
                    }
                    base.Dispose(true);
                    Application.Exit();
                    Close();
                    Environment.Exit(0);

                }
                else
                {
                    logTextBox.Clear();
                    cancelButton.Visible = true;
                    cancelButton.Text = "Close";
                    offlineSyncExportRadioButton.Checked = false;
                    newProtectionRadioButton.Checked = false;
                    pushAgentRadioButton.Checked = false;
                    manageExistingPlanRadioButton.Checked = false;
                    _directoryNameinXmlFile = null;

                    planNamesTreeView.Nodes.Clear();
                    progressBar.Visible = true;
                    taskGroupBox.Enabled = false;
                    getPlansBackgroundWorker.RunWorkerAsync();
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
        }
        private Process GetaProcess(string processname)
        {
            Process[] aProc = Process.GetProcessesByName(processname);

            if (aProc.Length > 0)
                return aProc[0];

            else return null;
        }

        internal bool FindAndKillProcess()
        {
            try
            {
                bool didIkillAnybody = false;
                //finding current process id...
                System.Diagnostics.Process CurrentProcessByvContinuum = new System.Diagnostics.Process();

                CurrentProcessByvContinuum = System.Diagnostics.Process.GetCurrentProcess();
                Process[] processRunningCurrently = Process.GetProcesses();

                foreach (Process p in processRunningCurrently)
                {
                    if (GetParentProcess(p.Id) == CurrentProcessByvContinuum.Id)
                    {

                        p.Kill();
                        didIkillAnybody = true;

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
        private int GetParentProcess(int Id)
        {
            int parentPid = 0;
            using (ManagementObject managementObject = new ManagementObject("win32_process.handle='" + Id.ToString() + "'"))
            {
                managementObject.Get();
                parentPid = Convert.ToInt32(managementObject["ParentProcessId"]);
            }
            return parentPid;
        }

        private void ResumeForm_FormClosing(object sender, FormClosingEventArgs e)
        {
            try
            {
                // this will call when ever user clicks on the clickbutton or on the control box....

                breifLog.WriteLine(DateTime.Now + " \t Closing the vCon Wizard");
                breifLog.Flush();
                breifLog.Close();
                try
                {
                    Trace.WriteLine(DateTime.Now + "\t Came here when user clicks on the control box");
                    if (components != null)
                    {
                        components.Dispose();
                    }
                }
                catch (Exception ex)
                {
                    Trace.WriteLine(DateTime.Now + "\t disposing the compents got failed " + ex.Message);
                }
                base.Dispose(true);
                Application.Exit();

                
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void pushAgentRadioButton_CheckedChanged(object sender, EventArgs e)
        {
            try
            {
                if (chooseApplicationComboBox.SelectedItem.ToString() == "P2V")
                {
                    selectedActionForPlan = "P2VPUSH";
                }
                else
                {
                    selectedActionForPlan = "ESXPUSH";
                }
                if (pushAgentRadioButton.Checked == true)
                {
                    AllServersForm allServersForm = new AllServersForm(selectedActionForPlan, "");
                    // this.Visible = false;
                    allServersForm.ShowDialog();

                    pushAgentRadioButton.Checked = false;
                    if (allServersForm.calledCancelByUser == false)
                    {
                        cancelButton.Visible = true;
                        cancelButton.Text = "Close";
                        offlineSyncExportRadioButton.Checked = false;
                        newProtectionRadioButton.Checked = false;
                        pushAgentRadioButton.Checked = false;
                        manageExistingPlanRadioButton.Checked = false;
                        _directoryNameinXmlFile = null;

                        planNamesTreeView.Nodes.Clear();
                        progressBar.Visible = true;
                        taskGroupBox.Enabled = false;
                        getPlansBackgroundWorker.RunWorkerAsync();
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
        }

        private void detachBackgroundWorker_DoWork(object sender, System.ComponentModel.DoWorkEventArgs e)
        {
           try
            {
               // After selecting the Clean up option and when user clicks the remove then it will call this backgroung worker 
               //This is will modify the esx_master_target esxip and master config file and call the detach script...
               // This will clean up the selected vms.
               //This will call the run work completed once the detach process completed
                tickerDelegate = new TickerDelegate(SetLeftTicker);
                messages = "Remove operation is started." + Environment.NewLine;
                logTextBox.Invoke(tickerDelegate, new object[] { messages });
                Host tempHost = (Host)selectedVMListForPlan._hostList[0];
               
                Remove clean = new Remove();
                Host h1 = new Host();
                bool ranAtleastOnce = false;
                bool modifiedMasterconfig = false;
                nodesList = null;
                //Checking whether masterconfigFile exists or not
                if (File.Exists(latestFilePath + "\\" + masterConfigFile))
                {
                    try
                    {
                        File.Delete(latestFilePath + "\\" + masterConfigFile);
                    }
                    catch (Exception ex)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Failed to delete the masterconfigfile " + ex.Message);
                    }
                }


                if (!File.Exists(latestFilePath + "\\" + masterConfigFile))
                {
                   
                    Trace.WriteLine(DateTime.Now + "\t\t MasterConfigFile is not exists in vContinuum\\Latest Folder ");
                    MasterConfigFile master = new MasterConfigFile();
                    master.DownloadMasterConfigFile(WinTools.GetcxIPWithPortNumber());
                }

                bool haveALLUUIDs = true;
                foreach (Host h in selectedVMListForPlan._hostList)
                {
                    if (h.inmage_hostid == null)
                    {
                        haveALLUUIDs = false;
                    }
                }
                if (haveALLUUIDs == true)
                {
                    Host masterHost = (Host)masterTargetList._hostList[0];
                    if (masterHost.inmage_hostid != null)
                    {
                       
                        Cxapicalls cxapi = new Cxapicalls();
                        messages = "Removing the replication pairs of selected machine(s)" + Environment.NewLine;
                        logTextBox.Invoke(tickerDelegate, new object[] { messages });
                        string requestId = null;
                        if (cxapi.PostRemove(selectedVMListForPlan, masterHost, ref requestId) == true)
                        {
                            Trace.WriteLine(DateTime.Now + "\t Successfully posted the remove option and request id is " + requestId);
                            if (requestId != null)
                            {
                                int i = 0;
                                while (i < 30)
                                {
                                    Trace.WriteLine(DateTime.Now + "\t Going to sleep for the time " + i.ToString());
                                    Thread.Sleep(60000);
                                    i++;
                                    if (cxapi.PostRemoveRetentionStatus(requestId) == true)
                                    {
                                        i = 30;
                                        Trace.WriteLine(DateTime.Now + "\t Successfully removed the retention ");
                                        break;
                                    }
                                }
                            }
                            messages = "Removed the replication pairs of selected machine(s)" + Environment.NewLine;
                            logTextBox.Invoke(tickerDelegate, new object[] { messages });
                            Trace.WriteLine(DateTime.Now + "\t Successfullt deleted the plan from the cx");
                        }
                        else
                        {
                            messages = "Failed to delete replication pairs" + Environment.NewLine;
                            logTextBox.Invoke(tickerDelegate, new object[] { messages });
                            Trace.WriteLine(DateTime.Now + "\t Failed to delete plan from cx");
                        }
                    }
                }

                foreach (Host h in selectedVMListForPlan._hostList)
                {
                    h1 = h;
                    h1.delete = true;
                    Trace.WriteLine(DateTime.Now + "\t Deleting the paris for the vm: " + h1.displayname);
                    if (h1.masterTargetHostName != null)
                    {
                        if (haveALLUUIDs == false)
                        {
                            messages = "Removing the replication pairs of " + h1.displayname + Environment.NewLine;
                            logTextBox.Invoke(tickerDelegate, new object[] { messages });
                            Trace.WriteLine(DateTime.Now + " \t came here to delete the pairs ");
                            // Step1.Removing FX and VX pairs from the CX

                            if (clean.RemoveReplication( h1, h1.masterTargetHostName, h.plan, WinTools.GetcxIPWithPortNumber(), h.vConName) == false)
                            {
                                h1 = clean.GetHost;
                                Trace.WriteLine(DateTime.Now + " \t came here to failed  ");
                                messages = "Failed to delete replication pairs" + Environment.NewLine;
                                logTextBox.Invoke(tickerDelegate, new object[] { messages });
                            }
                            else
                            {
                                messages = "Removed the replication pairs of " + h1.displayname + Environment.NewLine;
                                logTextBox.Invoke(tickerDelegate, new object[] { messages });
                                //Step2: Remove entries from Master.XML file
                            }
                        }
                    
                        Trace.WriteLine(DateTime.Now + "\t Entered to deleted existing node in the masterconfigfile");
                        //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
                        //based on above comment we have added xmlresolver as null
                        XmlDocument documentMasterEsx = new XmlDocument();
                        documentMasterEsx.XmlResolver = null;
                        XmlNodeList hostNodesInMasterXml = null;
                        Trace.WriteLine(DateTime.Now + "\t Prinitng the path of the file " + latestFilePath + "\\" + masterConfigFile);
                        string esxMasterXmlFile = latestFilePath + "\\" + masterConfigFile;

                        //StreamReader reader = new StreamReader(esxMasterXmlFile);
                        //string xmlString = reader.ReadToEnd();
                        //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
                        XmlReaderSettings settings = new XmlReaderSettings();
                        settings.ProhibitDtd = true;
                        settings.XmlResolver = null;
                        using (XmlReader reader3 = XmlReader.Create(esxMasterXmlFile, settings))
                        {
                            documentMasterEsx.Load(reader3);
                           // reader3.Close();
                        }
                        //documentMasterEsx.Load(esxMasterXmlFile);

                       // reader.Close();

                        hostNodesInMasterXml = documentMasterEsx.GetElementsByTagName("host");
                        foreach (XmlNode esxnode in hostNodesInMasterXml)
                        {
                            Trace.WriteLine(DateTime.Now + "\t Comparing displaynames " + esxnode.Attributes["display_name"].Value + "\t" + h1.displayname + "\t and target esx ips " + esxnode.Attributes["targetesxip"].Value + "\t " + h1.targetESXIp);

                            if (esxnode.Attributes["display_name"].Value == h1.displayname && esxnode.Attributes["targetesxip"].Value == h1.targetESXIp)
                            {
                                if (esxnode.ParentNode.Name == "SRC_ESX")
                                {
                                    ranAtleastOnce = true;
                                    XmlNode parentNode = esxnode.ParentNode;
                                    Trace.WriteLine(DateTime.Now + "\t Deleting the child node");
                                    esxnode.ParentNode.RemoveChild(esxnode);
                                    modifiedMasterconfig = true;
                                    if (!parentNode.HasChildNodes)
                                    {
                                        Trace.WriteLine(DateTime.Now + "\t Deleting the parent node");
                                        parentNode.ParentNode.ParentNode.RemoveChild(parentNode.ParentNode);
                                        break;
                                    }
                                    Trace.WriteLine(DateTime.Now + "\t Removed the node for the masterconfig file " + h1.displayname);
                                    break;
                                }
                            }
                        }
                        Trace.WriteLine(DateTime.Now + "\t saving the configuration file in this location " + latestFilePath + "\\" + masterConfigFile);
                        documentMasterEsx.Save(esxMasterXmlFile);
                        // Debug.WriteLine(esxXmlFile);                                                         
                    }
                }                    
           

                if (ranAtleastOnce == true)
                {
                    ranAtleastOnce = false;
                    Trace.WriteLine("Uploading MasterConfigFile.xml to ESX host");
                    messages = "Modifying necessary files.This may take few seconds" + Environment.NewLine;
                    logTextBox.Invoke(tickerDelegate, new object[] { messages });
                    Host sourceHost = (Host)selectedVMListForPlan._hostList[0];
                    if (sourceHost.machineType == "PhysicalMachine" && sourceHost.failback == "yes")
                    {
                        returnCodeofDetach = 0;
                        Trace.WriteLine(DateTime.Now + "\t Tried to use remove option for v2p");
                    }
                    else
                    {
                        //Preparing recovery.xml to detach the disks.....
                        SolutionConfig solution = new SolutionConfig();
                        Esx esx = new Esx();
                        if (solution.WriteXmlFile(selectedVMListForPlan, masterTargetList, esx, WinTools.GetcxIP(), "Remove.xml", "Remove", false) == true)
                        {
                            Trace.WriteLine(DateTime.Now + " \t Preparing the Remove.xml for remove operation is succreefully done");
                        }
                        else
                        {
                            messages = "Detach the disks manually from master target. " + Environment.NewLine;
                            logTextBox.Invoke(tickerDelegate, new object[] { messages });
                            Trace.WriteLine(DateTime.Now + "\t Failed to prepare the Recovery.xml ");
                            return;
                        }
                        messages = "Detaching the disks from the master target " + Environment.NewLine;
                        esxIPinMain = tempHost.targetESXIp;
                        logTextBox.Invoke(tickerDelegate, new object[] { messages });
                        Esx esxObj = new Esx();
                        VerfiyCxhasCreds(tempHost.targetESXIp, tempHost.targetESXUserName, tempHost.targetESXPassword, "target");
                        returnCodeofDetach = esxObj.DetachDisksFromMasterTarget(tempHost.targetESXIp);
                        Trace.WriteLine(DateTime.Now + "\t Retrun code of detach " + returnCodeofDetach);
                        if (returnCodeofDetach == 3)
                        {
                            Trace.WriteLine(DateTime.Now + "\t Failed with old credentials ");
                            SourceEsxDetailsPopUpForm sourceEsx = new SourceEsxDetailsPopUpForm();
                            sourceEsx.ipLabel.Text = "Target vCenter/vSphere IP/Name *";
                            sourceEsx.sourceEsxIpText.ReadOnly = false;
                            sourceEsx.sourceEsxIpText.Text = tempHost.targetESXIp;
                            sourceEsx.ShowDialog();
                            if (sourceEsx.canceled == false)
                            {
                                tempHost.targetESXIp = sourceEsx.sourceEsxIpText.Text;
                                tempHost.targetESXUserName = sourceEsx.userNameText.Text;
                                tempHost.targetESXPassword = sourceEsx.passWordText.Text;
                                Cxapicalls cxAp = new Cxapicalls();
                                cxAp. UpdaterCredentialsToCX(tempHost.targetESXIp, tempHost.targetESXUserName, tempHost.targetESXPassword);
                                returnCodeofDetach = esxObj.DetachDisksFromMasterTarget(tempHost.targetESXIp);
                            }
                        }
                    }

                }
                if (modifiedMasterconfig == true)
                {
                    modifiedMasterconfig = false;
                    MasterConfigFile master = new MasterConfigFile();
                    if (master.UploadMasterConfigFile(WinTools.GetcxIPWithPortNumber()) == true)
                    {
                        Trace.WriteLine(DateTime.Now + " \t Uploaded MasterConfigFile.xml successfully to the cx");
                    }
                    else
                    {
                        Trace.WriteLine(DateTime.Now + "\t Failed to upload MasterConfigFile.xml to cx");
                    }
                }
                else
                {
                    Trace.WriteLine(DateTime.Now + "\t\t No modifications in ESX_Master. Hence masterconfig file not uploaded to cx");                
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: DoWork  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void detachBackgroundWorker_ProgressChanged(object sender, System.ComponentModel.ProgressChangedEventArgs e)
        {
            progressBar.Value = e.ProgressPercentage;

        }

        private void detachBackgroundWorker_RunWorkerCompleted(object sender, System.ComponentModel.RunWorkerCompletedEventArgs e)
        {
            try
            {
                Trace.WriteLine(DateTime.Now + "Entered: RemovePairsForms::detachBackgroundWorker_RunWorkerCompleted Node List = " + nodesList);
                if (returnCodeofDetach == 0)
                {
                    Host sourceHost = (Host)selectedVMListForPlan._hostList[0];
                    if (sourceHost.machineType == "PhysicalMachine" && sourceHost.failback == "yes")
                    {
                        Trace.WriteLine(DateTime.Now + "\t Tried remove for v2p protection");
                    }
                    else
                    {
                        logTextBox.AppendText("Delete following VM(s) from ESX/vCenter:" + esxIPinMain + Environment.NewLine);
                       // logTextBox.AppendText("Please delete retention data from Master Target." + Environment.NewLine);
                        Trace.WriteLine("You can delete following VM(s) from ESX/vCenter:" + esxIPinMain);
                        /* string[] hostnameList = _nodeList.Split(new string[] { "!@!@!" }, StringSplitOptions.None);
                         foreach (string value in hostnameList)
                         {
                             foreach (Host h in Console._hostList)
                             {
                                 if (value == h.hostname)
                                 {
                                     logTextBox.AppendText(h.displayname + Environment.NewLine);
                                     Debug.WriteLine(h.displayname);
                                 }
                             }
                         }*/
                        foreach (Host h in selectedVMListForPlan._hostList)
                        {
                            logTextBox.AppendText(h.displayname + " (" + h.new_displayname + ") " + Environment.NewLine);

                        }
                    }
                    logTextBox.AppendText("Remove operation is completed" + Environment.NewLine);
                }
                else if (returnCodeofDetach == 10)
                {
                    logTextBox.AppendText("Remove operation is cancelled." + Environment.NewLine);
                }
                else
                {
                    logTextBox.AppendText("Unable to Detach disks from Master target" + Environment.NewLine);
                    Trace.WriteLine(DateTime.Now + "\t Unable to Detach disks from Master target. Return code = " + returnCodeofDetach + Environment.NewLine);
                    //logTextBox.AppendText("Remove operation is completed" + Environment.NewLine);
                }
                
                this.Enabled = true;
                progressBar.Visible = false;
                resumeButton.Enabled = true;
                resumeButton.Visible = false;
                cancelButton.Visible = true;
                returnCodeofDetach = 10;
                cancelButton.Text = "Done";
                              
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private bool GetPlans()
        {
            try
            {
                if (File.Exists(latestFilePath + "\\" + masterConfigFile))
                {
                    try
                    {
                        File.Delete(latestFilePath + "\\" + masterConfigFile);
                    }
                    catch (Exception ex)
                    {
                        this.Enabled = true;
                        System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                        Trace.WriteLine("ERROR*******************************************");
                        Trace.WriteLine(DateTime.Now + "Caught exception at Method:Monitor Constructor  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                        Trace.WriteLine("Exception  =  " + ex.Message);
                        Trace.WriteLine("ERROR___________________________________________");
                    }
                }
                MasterConfigFile masterConfig = new MasterConfigFile();
                try
                {
                    masterConfig.DownloadMasterConfigFile(WinTools.GetcxIPWithPortNumber());
                }
                catch (Exception ex)
                {
                    this.Enabled = true;
                    System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                    Trace.WriteLine("ERROR*******************************************");
                    Trace.WriteLine(DateTime.Now + "Caught exception at Method:Monitor Constructor  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                    Trace.WriteLine("Exception  =  " + ex.Message);
                    Trace.WriteLine("ERROR___________________________________________");
                }
                sourceHostsList = new HostList();
                mastertargetsHostList = new HostList();
                if (File.Exists(latestFilePath + "\\" + masterConfigFile))
                {
                    SolutionConfig readMasterXml = new SolutionConfig();
                    readMasterXml.ReadXmlConfigFileWithMasterList(masterConfigFile,  sourceHostsList,  mastertargetsHostList,  esxIPinMain,  cxIPinMain);
                    sourceHostsList = SolutionConfig.GetHostList;
                    mastertargetsHostList = SolutionConfig.GetMasterList;
                    esxIPinMain = readMasterXml.GetEsxIP;
                    cxIPinMain = readMasterXml.GetcxIP;
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

        private void homePicturePictureBox_Click(object sender, EventArgs e)
        {
            try
            {
                xmlFileMadeSuccessfully = false;

                readinessCheck = false;
                checkingForVmsPowerStatus = false;
                dummyDiskscreated = false;
                createGuestOnTarget = false;
                jobautomation = false;
                upDateMasterFile = false;
                upLoadFile = false;

                LaunchPreReqsForm launch = new LaunchPreReqsForm();
                if (launch.CxprereqChecks() == false)
                {
                    return;
                }
                if (File.Exists(latestFilePath + "\\" + masterConfigFile))
                {
                    try
                    {
                        File.Delete(latestFilePath + "\\" + masterConfigFile);
                    }
                    catch (Exception ex)
                    {
                        this.Enabled = true;
                        System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                        Trace.WriteLine("ERROR*******************************************");
                        Trace.WriteLine(DateTime.Now + "Caught exception at Method:Monitor Constructor  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                        Trace.WriteLine("Exception  =  " + ex.Message);
                        Trace.WriteLine("ERROR___________________________________________");
                    }
                }
                MasterConfigFile masterConfig = new MasterConfigFile();
                try
                {
                    masterConfig.DownloadMasterConfigFile(WinTools.GetcxIPWithPortNumber());
                }
                catch (Exception ex)
                {
                    this.Enabled = true;
                    System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                    Trace.WriteLine("ERROR*******************************************");
                    Trace.WriteLine(DateTime.Now + "Caught exception at Method:Monitor Constructor  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                    Trace.WriteLine("Exception  =  " + ex.Message);
                    Trace.WriteLine("ERROR___________________________________________");
                }

                cxIPTextBox.Text = WinTools.GetcxIP();
                currentTask = Wizard.Properties.Resources.arrow;
                SolutionConfig readMasterXml = new SolutionConfig();
                if (File.Exists(latestFilePath + "\\" + masterConfigFile))
                {


                    FileInfo f = new FileInfo(latestFilePath + "\\" + masterConfigFile);
                    long length = f.Length;

                    if (length > 100)
                    {
                        if (!ValidateMasterConfigFile())
                        {
                            MessageBox.Show(strErrorMsg, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            return;
                        }
                        else
                        {
                            Trace.WriteLine(DateTime.Now + "\t Validation completed successfully");
                        }
                        sourceHostsList = new HostList();
                        mastertargetsHostList = new HostList();
                        readMasterXml.ReadXmlConfigFileWithMasterList(masterConfigFile, sourceHostsList, mastertargetsHostList, esxIPinMain, cxIPinMain);
                        sourceHostsList = SolutionConfig.GetHostList;
                        mastertargetsHostList = SolutionConfig.GetMasterList;
                        esxIPinMain = readMasterXml.GetEsxIP;
                        cxIPinMain = readMasterXml.GetcxIP;

                    }
                    else
                    {
                        Trace.WriteLine(DateTime.Now + "\t Found empty values in ConfigFile");
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
                     
        }




        private bool ValidateMasterConfigFile()
        {
            bool bStatus = false;
             nErrors = 0;
             strErrorMsg = null;
            try
            {

                string xsdPath = installPath + "\\" + SampleMasterConfigXSDFile;
                string xmlPath = latestFilePath + "\\" + masterConfigFile;
                // Declare local objects
                XmlReaderSettings rs = new XmlReaderSettings();
                rs.ValidationType = ValidationType.Schema;
                rs.ValidationFlags |= XmlSchemaValidationFlags.ProcessSchemaLocation |
                XmlSchemaValidationFlags.ReportValidationWarnings;
                // Event Handler for handling exception & 
                // this will be called whenever any mismatch between XML & XSD
                rs.ValidationEventHandler +=
            new ValidationEventHandler(ValidationEventHandler);
                rs.Schemas.Add(null, XmlReader.Create(xsdPath));


                // reading xml
                using (XmlReader xmlValidatingReader = XmlReader.Create(xmlPath, rs))
                { while (xmlValidatingReader.Read()) { } }

                ////Exception if error.
                if (nErrors > 0) { throw new Exception(strErrorMsg); }
                else { bStatus = true; }//Success
            }
            catch (Exception error)
            {
                Console.WriteLine(error.Message);
                bStatus = false;
            }


            return bStatus;
           
        }

        private void ValidationEventHandler(object sender, ValidationEventArgs e)
        {
            if (e.Severity == XmlSeverityType.Warning) strErrorMsg += "WARNING: ";
            else strErrorMsg += "ERROR: ";
            nErrors++;

            if (e.Exception.Message.Contains("'Email' element is invalid"))
            {
                strErrorMsg = strErrorMsg + getErrorString("Provided Email data is Invalid", "CAPIEMAIL007") + "\r\n";
            }
            if (e.Exception.Message.Contains
            ("The element 'Person' has invalid child element"))
            {
                strErrorMsg = strErrorMsg + getErrorString
            ("Provided XML contains invalid child element",
            "CAPINVALID005") + "\r\n";
            }
            else
            {
                strErrorMsg = strErrorMsg + e.Exception.Message + "\r\n";
            }
        }
        private string getErrorString(string erroString, string errorCode)
        {
            StringBuilder errXMl = new StringBuilder();
            errXMl.Append("<MyError> <errorString> ERROR_STRING </errorString> 	<errorCode> ERROR_CODE </errorCode> </MyError>");
            errXMl.Replace("ERROR_STRING", erroString);
            errXMl.Replace("ERROR_CODE", errorCode);
            return errXMl.ToString();
        }

        private bool LoadTreeView()
        {
            try
            {
                int i = 0;

                foreach (Host h in sourceHostsList._hostList)
                {
                    //Trace.WriteLine(DateTime.Now + " \t Printing the plan name  " + h.plan);
                    if (h.role == Role.Primary && (h.failback == "no" || h.failOver == "no"))
                    {
                        // Trace.WriteLine(DateTime.Now + "\t Prinitng the plan name and the ip " + h.plan + h.ip + " vconname " +h.vConName ) ;
                        // Trace.WriteLine(DateTime.Now + "\t Printing the display name " + h.displayname);
                        if (planNamesTreeView.Nodes.Count == 0)
                        {

                            planNamesTreeView.Nodes.Add(h.plan);
                            bool sourceVMExists = false;
                            //planNamesTreeView.Nodes[i].Nodes.Add(h.displayname);
                            i++;
                        }

                        bool planExists = false;
                        foreach (TreeNode node in planNamesTreeView.Nodes)
                        {
                            // Trace.WriteLine(DateTime.Now + " \t printing the node names " + node.Name + " plan name " + h.plan);
                            if (node.Text == h.plan && h.role == Role.Primary)
                            {
                                planExists = true;
                                node.Nodes.Add(h.displayname);

                            }
                        }
                        if (planExists == false)
                        {
                            if (h.role == Role.Primary)
                            {
                                planNamesTreeView.Nodes.Add(h.plan);
                                planNamesTreeView.Nodes[i].Nodes.Add(h.displayname);
                                i++;
                            }
                        }
                    }
                }
                // displayAllRadioButton.Checked = true;
                _planName = "";
                selectedVMListForPlan = new HostList();
                masterTargetList = new HostList();
                planNamesTreeView.CheckBoxes = false;
                planNamesTreeView.ExpandAll();
                versionLabel.Text = HelpForcx.VersionNumber;
                resumeButton.Visible = false;
                mainFormPanel.BringToFront();
                VMDeatilsPanel.SendToBack();
                if (planNamesTreeView.Nodes.Count == 0)
                {
                    planNamesTreeView.Nodes.Add("No plans found");
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

        private void getFailBackDetailsBackgroundWorker_DoWork(object sender, System.ComponentModel.DoWorkEventArgs e)
        {
            try
            {
                //Esx esx = new Esx();
                Host h = (Host)selectedVMListForPlan._hostList[0];
                _esxInfo = new Esx();
                VerfiyCxhasCreds(h.targetESXIp, h.targetESXUserName, h.targetESXPassword, "target");
                if (_esxInfo.GetEsxInfo(h.targetESXIp, Role.Primary, h.os) == 3)
                {
                    SourceEsxDetailsPopUpForm source = new SourceEsxDetailsPopUpForm();
                    source.ipLabel.Text = "Target vCenter/vSphere IP/Name *";
                    source.sourceEsxIpText.ReadOnly = false;
                    source.sourceEsxIpText.Text = h.targetESXIp;
                    source.ShowDialog();
                    h.targetESXIp = source.sourceEsxIpText.Text;
                    h.targetESXUserName = source.userNameText.Text;
                    h.targetESXPassword = source.passWordText.Text;
                    Cxapicalls cxapi = new Cxapicalls();
                    cxapi. UpdaterCredentialsToCX(h.targetESXIp, h.targetESXUserName, h.targetESXPassword);
                    if (_esxInfo.GetEsxInfo(h.targetESXIp, Role.Primary, h.os) != 0)
                    {
                        MessageBox.Show("Failed to get the information form the target vSphere client", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return;
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

        }

        private void getFailBackDetailsBackgroundWorker_ProgressChanged(object sender, System.ComponentModel.ProgressChangedEventArgs e)
        {
            progressBar.Value = e.ProgressPercentage;
        }

        private void getFailBackDetailsBackgroundWorker_RunWorkerCompleted(object sender, System.ComponentModel.RunWorkerCompletedEventArgs e)
        {
            try
            {
                progressBar.Visible = false;
                resumeButton.Enabled = true;
                this.Enabled = true;
                if (File.Exists(latestFilePath + "\\Info.xml"))
                {
                    AllServersForm allServersForm = new AllServersForm("failBack", "");
                    allServersForm.ShowDialog();
                    manageExistingPlanRadioButton.Checked = false;
                    planNamesTreeView.CheckBoxes = false;
                    //failBackRadioButton.Checked = false;
                    failBackRadioButton.Checked = false;
                    if (allServersForm.calledCancelByUser == false)
                    {
                        cancelButton.Visible = true;
                        cancelButton.Text = "Close";
                        offlineSyncExportRadioButton.Checked = false;
                        newProtectionRadioButton.Checked = false;
                        pushAgentRadioButton.Checked = false;
                        manageExistingPlanRadioButton.Checked = false;
                        _directoryNameinXmlFile = null;
                        cancelButton.Visible = true;
                        cancelButton.Text = "Close";

                        offlineSyncExportRadioButton.Checked = false;
                        offLineSyncImportRadioButton.Checked = false;

                        newProtectionRadioButton.Checked = false;
                        pushAgentRadioButton.Checked = false;
                        manageExistingPlanRadioButton.Checked = false;
                        _directoryNameinXmlFile = null;

                        planNamesTreeView.Nodes.Clear();
                        planNamesTreeView.Nodes.Clear();
                        taskGroupBox.Enabled = false;
                        progressBar.Visible = true;
                        getPlansBackgroundWorker.RunWorkerAsync();
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
        }

        internal bool CreatingEsxXmlForAdditionOfDisk()
        {
            try
            {
                if (selectedVMListForPlan._hostList.Count != 0)
                {
                    Process p;
                    int exitCode;
                    bool timedOut = false;
                    Host h1 = (Host)selectedVMListForPlan._hostList[0];
                    _masterHost = (Host)masterTargetList._hostList[0];
                    // Debug.WriteLine("Printing command" + _sourceEsxIp + "\"" + _sourceUsername + "\"" + "\"" + _sourcePassword + "\"" + _targetEsxIp + "\"" + _targetUsername + "\"" + _targetPassword + nodeList + h1.masterTargetDisplayName + _plan);

                    //downloading the esx.xml file for the esx solution.....
                    try
                    {
                        if (File.Exists(latestFilePath + " \\ESX.xml"))
                        {
                            File.Delete(latestFilePath + " \\ESX.xml");
                        }
                    }
                    catch (Exception ex)
                    {
                        System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                        Trace.WriteLine("ERROR*******************************************");
                        Trace.WriteLine(DateTime.Now + "Caught exception at Method: Deletion ESX.xml failed in ProcessPanel " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                        Trace.WriteLine("Exception  =  " + ex.Message);
                        Trace.WriteLine("ERROR___________________________________________");
                    }
                    SolutionConfig config = new SolutionConfig();
                    Esx esx = new Esx();
                    Cxapicalls cxAPi = new Cxapicalls();
                    // MessageBox.Show(_masterHost.configurationList.Count.ToString() + " " + _masterHost.networkNameList.Count.ToString());


                    esx.configurationList = _masterHost.configurationList;
                    esx.lunList = _masterHost.lunList;
                    esx.networkList = _masterHost.networkNameList;
                    esx.dataStoreList = _masterHost.dataStoreList;
                    config.WriteXmlFile(selectedVMListForPlan, masterTargetList, esx, WinTools.GetcxIP(), "ESX.xml", "", false);
                    Esx _esxInfo = new Esx();
                    VerfiyCxhasCreds(h1.esxIp, h1.esxUserName, h1.esxPassword,"source");
                    p = _esxInfo.DownLoadXmlFile(h1.esxIp);

                    exitCode = WaitForProcess(p, ref timedOut);
                    Trace.WriteLine(DateTime.Now + " \t Printing the return of addition of disk " + exitCode.ToString());
                    if (exitCode == 0)
                    {
                        TargetReadinessOfIncrementalDisk(h1.targetESXIp, h1.targetESXUserName, h1.targetESXPassword);

                    }
                    else if (exitCode == 3)
                    {
                        SourceEsxDetailsPopUpForm source = new SourceEsxDetailsPopUpForm();
                        source.sourceEsxIpText.ReadOnly = false;
                        source.sourceEsxIpText.Text = h1.esxIp;
                        source.ShowDialog();
                        if (source.canceled == false)
                        {
                            h1.esxIp = source.sourceEsxIpText.Text;
                            h1.esxUserName = source.userNameText.Text;
                            h1.esxPassword = source.passWordText.Text;
                        }
                        source.ipLabel.Text = "Source vSphere/vCenter IP/Name *";
                        source.sourceEsxIpText.Text = h1.targetESXIp;
                        source.ShowDialog();
                        if (source.canceled == false)
                        {
                            h1.targetESXIp = source.sourceEsxIpText.Text;
                            h1.targetESXUserName = source.userNameText.Text;
                            h1.targetESXPassword = source.passWordText.Text;
                        }
                        foreach (Host h in selectedVMListForPlan._hostList)
                        {
                            h.targetESXIp = h1.targetESXIp;
                            h.targetESXUserName = h1.targetESXUserName;
                            h.targetESXPassword = h1.targetESXPassword;
                            h.esxIp = h1.esxIp;
                            h.esxUserName = h1.esxUserName;
                            h.esxPassword = h1.esxPassword;

                        }
                        cxAPi. UpdaterCredentialsToCX(h1.esxIp, h1.esxUserName, h1.esxPassword);
                        p = esx.DownLoadXmlFile(h1.esxIp);

                        exitCode = WaitForProcess(p,  ref timedOut);
                        if (exitCode == 0)
                        {
                            TargetReadinessOfIncrementalDisk(h1.targetESXIp, h1.targetESXUserName, h1.targetESXPassword);
                            passedAdditionOfDisk = true;
                        }
                        else if (exitCode != 0)
                        {
                            passedAdditionOfDisk = false;
                            MessageBox.Show("Failed to get information from the primary vSphere server.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            return false;
                        }
                        return false;
                    }
                    else
                    {
                        Cursor.Current = Cursors.Default;
                        passedAdditionOfDisk = false;
                        MessageBox.Show("Failed to get information from the primary vSphere server.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return false;
                    }
                }
                else
                {
                    Cursor.Current = Cursors.Default;
                    MessageBox.Show("There are no vms found", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
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

        internal int TargetReadinessOfIncrementalDisk(string esxIP, string esxUsername, string esxPassword)
        {
            try
            {
                Esx esx = new Esx();
                Cxapicalls cxapi = new Cxapicalls();
                VerfiyCxhasCreds(esxIP, esxUsername, esxPassword, "target");
                int returnCode = esx.PreReqCheckForAdditionOfDiskTarget(esxIP);
                if (returnCode == 0)
                {
                    passedAdditionOfDisk = true;
                }
                else if (returnCode == 3)
                {
                    SourceEsxDetailsPopUpForm source = new SourceEsxDetailsPopUpForm();
                    source.sourceEsxIpText.ReadOnly = false;
                    source.sourceEsxIpText.Text = esxIP;
                    source.ShowDialog();
                    if (source.canceled == false)
                    {
                        esxIP = source.sourceEsxIpText.Text;
                        esxUsername = source.userNameText.Text;
                        esxPassword = source.passWordText.Text;
                    }
                    source.ipLabel.Text = "Target vSphere/vCenter IP/Name *";
                    source.sourceEsxIpText.Text = esxIP;
                    source.ShowDialog();
                    if (source.canceled == false)
                    {
                        esxIP = source.sourceEsxIpText.Text;
                        esxUsername = source.userNameText.Text;
                        esxPassword = source.passWordText.Text;
                    }
                    foreach (Host h in selectedVMListForPlan._hostList)
                    {
                        h.targetESXIp = esxIP;
                        h.targetESXUserName = esxUsername;
                        h.targetESXPassword = esxPassword;

                    }
                    cxapi. UpdaterCredentialsToCX(esxIP, esxUsername, esxPassword);
                    returnCode = esx.PreReqCheckForAdditionOfDiskTarget(esxIP);


                    if (returnCode == 0)
                    {
                        passedAdditionOfDisk = true;
                        return returnCode;
                    }
                    else if (returnCode != 0)
                    {
                        passedAdditionOfDisk = false;
                        MessageBox.Show("Failed to get information from the primary vSphere/vCenter server.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    }
                }
                else
                {
                    MessageBox.Show("Failed to get information from the secondary vSphere/vCenter server.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);

                }

                return returnCode;

            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return 1;
            }
            
        }

        internal int WaitForProcess(Process process, ref bool timedOut)
        {
            long waited = 0;
            int increasedWait = 100;
            int exitCode = 1;
            int i = 10;
            //90 Seconds
            try
            {
                // allServersForm.Enabled = false;
              
               
                waited = 0;
                while (process.HasExited == false)
                {

                    if (waited <= TimeOutMilliseconds)
                    {
                        process.WaitForExit(increasedWait);
                        waited = waited + increasedWait;
                       
                       

                    }
                    else
                    {
                        if (process.HasExited == false)
                        {
                            Trace.WriteLine("Pre req checks failed " + DateTime.Now);
                            timedOut = true;
                            exitCode = 1;
                            break;
                        }
                    }
                }
                if (timedOut == false)
                {
                    exitCode = process.ExitCode;
                }                
                Cursor.Current = Cursors.Default;
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");

                return 1;
            }
            return exitCode;
        }
        private void additionOfDiskBackgroundWorker_DoWork(object sender, System.ComponentModel.DoWorkEventArgs e)
        {
            try
            {
                if (selectedVMListForPlan._hostList.Count != 0)
                {
                    try
                    {

                        if (GetResizeList() == true)
                        {
                            // HostList selectedList = Console;
                            RefreshHosts(selectedVMListForPlan);
                        }
                        else
                        {
                            Trace.WriteLine(DateTime.Now + "\t Resize operation is pending fro some of the machines");
                            return;
                        }
                        //foreach (Host h in sourceHostsList._hostList)
                        //{
                        //    foreach (Host h1 in Console._hostList)
                        //    {
                        //        if (h.displayname.ToUpper() == h1.displayname.ToUpper())
                        //        {
                        //            h1.disks.list = h.disks.list;
                        //        }
                        //    }
                        //}
                    }
                    catch (Exception ex)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Failed to refresh hosts " + ex.Message);
                    }
                    if (selectedActionForPlan == "ESX")
                    {
                        CreatingEsxXmlForAdditionOfDisk();
                    }
                    else
                    {
                        P2VAdditionOfDiskCall();
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
        }

        private void additionOfDiskBackgroundWorker_ProgressChanged(object sender, System.ComponentModel.ProgressChangedEventArgs e)
        {
            progressBar.Value = e.ProgressPercentage;

        }

        private void additionOfDiskBackgroundWorker_RunWorkerCompleted(object sender, System.ComponentModel.RunWorkerCompletedEventArgs e)
        {
            try
            {
                progressBar.Visible = false;
                resumeButton.Enabled = true;
                cancelButton.Enabled = true;
                this.Enabled = true;
                if (passedAdditionOfDisk == true)
                {
                    AllServersForm allServersForm = new AllServersForm("additionOfDisk", "");
                    allServersForm.ShowDialog();
                    //addDiskRadioButton.Checked = false;
                    manageExistingPlanRadioButton.Checked = false;
                    planNamesTreeView.CheckBoxes = false;
                    addDiskRadioButton.Checked = false;
                    if (allServersForm.calledCancelByUser == false)
                    {
                        cancelButton.Visible = true;
                        cancelButton.Text = "Close";
                        offlineSyncExportRadioButton.Checked = false;
                        newProtectionRadioButton.Checked = false;
                        pushAgentRadioButton.Checked = false;
                        manageExistingPlanRadioButton.Checked = false;
                        _directoryNameinXmlFile = null;
                        taskGroupBox.Enabled = false;
                        planNamesTreeView.Nodes.Clear();
                        progressBar.Visible = true;
                        getPlansBackgroundWorker.RunWorkerAsync();
                    }

                }
                else
                {
                    updateLogTextBox(logTextBox);
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


        }

        internal bool P2VAdditionOfDiskCall()
        {
            try
            {
                if (ResumeForm.selectedActionForPlan == "BMR Protection")
                {
                    bool timedOut = false;
                    int exitCode;
                    Process p;
                    ArrayList scsiVmxList = new ArrayList();
                    //the below string is the public static string ....

                    //this is for the esx.xml file attribute...
                    if (selectedVMListForPlan._hostList.Count != 0)
                    {
                        foreach (Host h in selectedVMListForPlan._hostList)
                        {
                            foreach (Host h1 in sourceHostsList._hostList)
                            {
                                Trace.WriteLine(DateTime.Now + " \t Came here to compare displaynames " + h.displayname + "  " + h1.displayname);
                                if (h.new_displayname == h1.new_displayname || h.hostname == h1.hostname)
                                {

                                    ArrayList diskList = new ArrayList();
                                    ArrayList physicalList = new ArrayList();
                                    diskList = h.disks.GetDiskList;
                                    physicalList = h1.disks.GetDiskList;
                                    foreach (Hashtable diskHash in diskList)
                                    {
                                        diskHash["already_map_vmx"] = "no";
                                    }

                                    foreach (Hashtable hash in physicalList)
                                    {
                                        foreach (Hashtable diskHash in diskList)
                                        {
                                            if (hash["disk_signature"] != null && diskHash["disk_signature"] != null)
                                            {
                                                Trace.WriteLine(DateTime.Now + " \t Came here to compare disknames  " + hash["disk_signature"].ToString() + "  " + diskHash["disk_signature"].ToString());
                                                if (hash["disk_signature"].ToString() == diskHash["disk_signature"].ToString())
                                                {
                                                    if (hash["scsi_mapping_vmx"] != null)
                                                    {
                                                        Trace.WriteLine(DateTime.Now + " \t Came here to assing scsi mapping host values" + hash["scsi_mapping_vmx"].ToString());
                                                        diskHash["scsi_mapping_vmx"] = hash["scsi_mapping_vmx"].ToString();
                                                        scsiVmxList.Add(hash["scsi_mapping_vmx"].ToString());

                                                        diskHash["scsi_adapter_number"] = hash["scsi_mapping_vmx"].ToString().Split(':')[0];
                                                        diskHash["scsi_slot_number"] = hash["scsi_mapping_vmx"].ToString().Split(':')[1];
                                                        diskHash["Protected"] = "yes";
                                                        diskHash["already_map_vmx"] = "yes";
                                                    }
                                                }
                                            }
                                            else
                                            {
                                                Trace.WriteLine(DateTime.Now + " \t Came here to compare disknames  " + hash["Name"].ToString() + "  " + diskHash["Name"].ToString());
                                                if (hash["Name"].ToString() == diskHash["Name"].ToString())
                                                {
                                                    if (hash["scsi_mapping_vmx"] != null)
                                                    {
                                                        Trace.WriteLine(DateTime.Now + " \t Came here to assing scsi mapping host values" + hash["scsi_mapping_vmx"].ToString());
                                                        diskHash["scsi_mapping_vmx"] = hash["scsi_mapping_vmx"].ToString();
                                                        scsiVmxList.Add(hash["scsi_mapping_vmx"].ToString());

                                                        diskHash["scsi_adapter_number"] = hash["scsi_mapping_vmx"].ToString().Split(':')[0];
                                                        diskHash["scsi_slot_number"] = hash["scsi_mapping_vmx"].ToString().Split(':')[1];
                                                        diskHash["Protected"] = "yes";
                                                        diskHash["already_map_vmx"] = "yes";
                                                    }
                                                }
                                            }

                                        }
                                    }

                                }

                            }
                        }
                        foreach (Host h1 in selectedVMListForPlan._hostList)
                        {
                            ArrayList diskListOfHost = new ArrayList();
                            diskListOfHost = h1.disks.GetDiskList;
                            foreach (Hashtable disk in diskListOfHost)
                            {
                                disk["Selected"] = "Yes";
                            }
                        }

                        foreach (Host h1 in selectedVMListForPlan._hostList)
                        {
                            foreach (Host tempHost in sourceHostsList._hostList)
                            {
                                if (h1.displayname == tempHost.displayname)
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
                                    }
                                    //tempHost.alreadyP2VDisksProtected = i;
                                    //h1.alreadyP2VDisksProtected = i;

                                    if ((i >= 8 && i < 23))
                                    {
                                        tempHost.alreadyP2VDisksProtected = i + 1;
                                        h1.alreadyP2VDisksProtected = i + 1;
                                    }
                                    else if (i >= 23 && i < 38)
                                    {
                                        tempHost.alreadyP2VDisksProtected = i + 2;
                                        h1.alreadyP2VDisksProtected = i + 2;

                                    }
                                    else if (i >= 38 && i < 53)
                                    {
                                        tempHost.alreadyP2VDisksProtected = i + 3;
                                        h1.alreadyP2VDisksProtected = i + 3;
                                    }
                                    else if (i >= 53)
                                    {
                                        tempHost.alreadyP2VDisksProtected = i + 4;
                                        h1.alreadyP2VDisksProtected = i + 4;
                                    }
                                    else
                                    {
                                        tempHost.alreadyP2VDisksProtected = i;
                                        h1.alreadyP2VDisksProtected = i;

                                    }
                                }
                            }

                            foreach (Host tempHost1 in selectedVMListForPlan._hostList)
                            {
                                tempHost1.machineType = "PhysicalMachine";
                                Trace.WriteLine(DateTime.Now + " \t Printing protected disk count" + tempHost1.alreadyP2VDisksProtected.ToString());
                                tempHost1.disks.AppendScsiForP2vAdditionOfDisk(tempHost1.alreadyP2VDisksProtected);
                                ArrayList physicalDisks = tempHost1.disks.GetDiskList;
                            }
                        }
                        //h.disks.AddScsiNumbers();

                        //generating sourcexml for the scripts at the time of the incremental disk...

                        SolutionConfig config = new SolutionConfig();
                        // config.WriteSourceXml(_tempSelectedSourceList, "SourceXml.xml");
                        Host sourceHost = (Host)selectedVMListForPlan._hostList[0];
                        Host masterHost = (Host)masterTargetList._hostList[0];


                        Esx e1 = new Esx();
                        //checking the esx.xml exist and if it is there we are deleting the file...
                        try
                        {
                            if (File.Exists("ESX.xml"))
                            {
                                File.Delete("ESX.xml");

                            }

                        }
                        catch (Exception ex)
                        {
                            System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                            Trace.WriteLine("ERROR*******************************************");
                            Trace.WriteLine(DateTime.Now + "Caught exception at Method: PostJobAutomation  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                            Trace.WriteLine("Exception  =  " + ex.Message);
                            Trace.WriteLine("ERROR___________________________________________");
                            return false;
                        }
                        Esx _targetESX = new Esx();
                        _targetESX.dataStoreList = masterHost.dataStoreList;
                        _targetESX.configurationList = masterHost.configurationList;
                        _targetESX.networkList = masterHost.networkNameList;
                        _targetESX.lunList = masterHost.lunList;
                        foreach (Host h in masterTargetList._hostList)
                        {
                            if (h.ide_count == null || h.ide_count.Length == 0)
                            {
                                h.ide_count = "0";
                            }
                        }
                        config.WriteXmlFile(selectedVMListForPlan, masterTargetList, _targetESX, WinTools.GetcxIP(), "ESX.xml", "", true);
                        //Console.GenerateVmxFile();

                        //downloading the esx.xml from the scripts for the p2v incremental disk....
                        VerfiyCxhasCreds(sourceHost.targetESXIp, sourceHost.targetESXUserName, sourceHost.targetESXPassword, "target");
                        exitCode = e1.PreReqCheckForAdditionOfDiskTarget(sourceHost.targetESXIp);



                        if (exitCode == 0)
                        {
                            passedAdditionOfDisk = true;
                        }
                        else if (exitCode == 3)
                        {
                            SourceEsxDetailsPopUpForm source = new SourceEsxDetailsPopUpForm();
                            source.ipLabel.Text = "Target vCenter/vSphere IP/Name *";
                            source.sourceEsxIpText.ReadOnly = false;
                            source.ShowDialog();
                            sourceHost.targetESXIp = source.sourceEsxIpText.Text;
                            sourceHost.targetESXUserName = source.userNameText.Text;
                            sourceHost.targetESXPassword = source.passWordText.Text;
                            foreach (Host h in selectedVMListForPlan._hostList)
                            {
                                h.targetESXIp = sourceHost.targetESXIp;
                                h.targetESXUserName = sourceHost.targetESXUserName;
                                h.targetESXPassword = sourceHost.targetESXPassword;
                            }
                            Cxapicalls cxAPi = new Cxapicalls();
                            cxAPi. UpdaterCredentialsToCX(sourceHost.targetESXIp, sourceHost.targetESXUserName, sourceHost.targetESXPassword);
                            exitCode = e1.PreReqCheckForAdditionOfDiskTarget(sourceHost.targetESXIp);

                            passedAdditionOfDisk = false;

                            if (exitCode != 0)
                            {
                                MessageBox.Show("Failed to fetch the information form the target vSphere client", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                return false;
                            }
                            //MessageBox.Show("Please check credentials provided", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);


                            return false;
                        }
                        else
                        {

                            MessageBox.Show("Failed to get information from the primary vSphere server.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);


                            return false;
                        }


                    }
                    else
                    {

                        MessageBox.Show("There are no vms found", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
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
            }
            return true;
        }

        private void chooseApplicationComboBox_SelectedIndexChanged(object sender, EventArgs e)
        {
            try
            {
                //if (chooseApplicationComboBox.SelectedItem == "P2V".ToString())
                //{
                //    failBackRadioButton.Enabled = false;
                //}
                //else
                //{
                //    failBackRadioButton.Enabled = true;
                //}
                newProtectionRadioButton.Checked = false;
                pushAgentRadioButton.Checked = false;
                manageExistingPlanRadioButton.Checked = false;
                offlineSyncExportRadioButton.Checked = false;
                planNamesTreeView.CheckBoxes = false;
                /*addDiskRadioButton.Checked = false;
                resumeRadioButton.Checked = false;
                recoverRadioButton.Checked = false;
                failBackRadioButton.Checked = false;
                 */

                //  object o = new object();
                //homePicturePictureBox_Click(o, e);
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void offlineSyncImportRadioButton_CheckedChanged(object sender, EventArgs e)
        {
           // if (offlineSyncImportRadioButton.Checked == true)
            {
                
               // offlineSyncImportRadioButton.Checked = false;
            }
        }

        private void offlineSyncExportRadioButton_CheckedChanged(object sender, EventArgs e)
        {
            try
            {
                if (offlineSyncExportRadioButton.Checked == true)
                {
                    if (chooseApplicationComboBox.SelectedItem == "ESX")
                    {
                        selectedActionForPlan = "Esx";
                    }
                    else
                    {
                        selectedActionForPlan = "P2v";
                    }
                    if (offlineSyncExportRadioButton.Checked == true)
                    {
                        removeRadioButton.Visible = false;
                        recoverRadioButton.Visible = false;
                        failBackRadioButton.Visible = false;
                        addDiskRadioButton.Visible = false;
                        drDrillRadioButton.Visible = false;
                        resumeProtectionRadioButton.Visible = false;
                        //editMasterConfigfileRadioButton.Visible = false;
                        offlineSyncExportRadioButton.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
                        exportRadioButton.Visible = true;
                        offLineSyncImportRadioButton.Visible = true;
                        manageExistingPlansPanel.Visible = true;
                    }
                    else
                    {
                        offlineSyncExportRadioButton.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
                        manageExistingPlansPanel.Visible = true;
                    }
                }
                else
                {
                    manageExistingPlansPanel.Visible = false;
                    offlineSyncExportRadioButton.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
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
        }

        private void recoveryMoreLabel_Click(object sender, EventArgs e)
        {
            if (File.Exists(installPath + "\\Manual.chm"))
            {
                Help.ShowHelp(null, installPath + "\\Manual.chm", HelpNavigator.TopicId, Recover);
            } 
        }

        private void additionOfDiskMoreLabel_Click(object sender, EventArgs e)
        {
            if (File.Exists(installPath + "\\Manual.chm"))
            {
                Help.ShowHelp(null, installPath + "\\Manual.chm", HelpNavigator.TopicId, Adddisks);
            } 
        }

        private void failBackProtectionMoreLabel_Click(object sender, EventArgs e)
        {
            if (File.Exists(installPath + "\\Manual.chm"))
            {
                Help.ShowHelp(null, installPath + "\\Manual.chm", HelpNavigator.TopicId, Failback);
            } 
        }

        private void offlineSyncMoreLabel_Click(object sender, EventArgs e)
        {
            if (File.Exists(installPath + "\\Manual.chm"))
            {
                Help.ShowHelp(null, installPath + "\\Manual.chm", HelpNavigator.TopicId, Offlinesync);
            } 
        }

        private void newProtectionMoreLabel_Click(object sender, EventArgs e)
        {
            if (File.Exists(installPath + "\\Manual.chm"))
            {
                if (chooseApplicationComboBox.SelectedItem == "ESX")
                {
                    Help.ShowHelp(null, installPath + "\\Manual.chm", HelpNavigator.TopicId, ProtectionEsx);
                }
                else
                {
                    Help.ShowHelp(null, installPath + "\\Manual.chm", HelpNavigator.TopicId, ProtectionP2v);
                }
            } 
        }

        private void removeMoreLabel_Click(object sender, EventArgs e)
        {
            if (File.Exists(installPath + "\\Manual.chm"))
            {
                Help.ShowHelp(null, installPath + "\\Manual.chm", HelpNavigator.TopicId, Remove);
            } 
        }

        private bool GetMasterConfigFileAndReloadTreeView()
        {
            try
            {
                this.Enabled = false;
                if (File.Exists(latestFilePath + "\\" + masterConfigFile))
                {
                    try
                    {
                        File.Delete(latestFilePath + "\\" + masterConfigFile);
                    }
                    catch (Exception ex)
                    {
                        this.Enabled = true;
                        System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                        Trace.WriteLine("ERROR*******************************************");
                        Trace.WriteLine(DateTime.Now + "Caught exception at Method:Monitor Constructor  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                        Trace.WriteLine("Exception  =  " + ex.Message);
                        Trace.WriteLine("ERROR___________________________________________");
                    }
                }
                MasterConfigFile masterConfig = new MasterConfigFile();
                masterConfig.DownloadMasterConfigFile(WinTools.GetcxIPWithPortNumber());
                cxIPTextBox.Text = WinTools.GetcxIP();
                currentTask = Wizard.Properties.Resources.arrow;
                sourceHostsList = new HostList();
                SolutionConfig readMasterXml = new SolutionConfig();
                if (File.Exists(latestFilePath+"\\"+masterConfigFile))
                {
                    readMasterXml.ReadXmlConfigFileWithMasterList(masterConfigFile,  sourceHostsList,  mastertargetsHostList,  esxIPinMain,  cxIPinMain);
                    sourceHostsList = SolutionConfig.GetHostList;
                    mastertargetsHostList = SolutionConfig.GetMasterList;
                    esxIPinMain = readMasterXml.GetEsxIP;
                    cxIPinMain = readMasterXml.GetcxIP;
                }
            }
            catch (Exception ex)
            {
                this.Enabled = true;
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:Monitor Constructor  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
            this.Enabled = true;
            return true;
        }

        private void manageExistingPlanRadioButton_CheckedChanged(object sender, EventArgs e)
        {
            try
            {
                if (manageExistingPlanRadioButton.Checked == true)
                {
                    _directoryNameinXmlFile = null;
                    removeRadioButton.Visible = true;
                    recoverRadioButton.Visible = true;
                    failBackRadioButton.Visible = true;
                    addDiskRadioButton.Visible = true;
                    resumeProtectionRadioButton.Visible = true;
                    drDrillRadioButton.Visible = true;
                    drDrillRadioButton.BringToFront();
                    resumeLabel.Visible = true;
                    exportRadioButton.Visible = false;
                    offLineSyncImportRadioButton.Visible = false;
                    manageExistingPlansPanel.Visible = true;

                    manageExistingPlanRadioButton.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
                    //pushAgentRadioButton.Location = new System.Drawing.Point(217, 13);
                    // offlineSyncExportRadioButton.Location = new System.Drawing.Point(302, 13);
                    manageExistingPlansPanel.Visible = true;
                }
                else
                {
                    //pushAgentRadioButton.Location = new System.Drawing.Point(207, 13);
                    //offlineSyncExportRadioButton.Location = new System.Drawing.Point(292, 13);
                    //editMasterConfigfileRadioButton.Visible = false;
                    manageExistingPlanRadioButton.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
                    manageExistingPlansPanel.Visible = false;
                }
                addDiskRadioButton.Checked = false;
                drDrillRadioButton.Checked = false;
                resumeProtectionRadioButton.Checked = false;
                recoverRadioButton.Checked = false;
                removeRadioButton.Checked = false;
                failBackRadioButton.Checked = false;
                editMasterConfigfileRadioButton.Checked = false;
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }

        }      

       
       
        private void getPlansButton_Click(object sender, EventArgs e)
        {
            try
            {
                getplans = true;
                Trace.WriteLine(DateTime.Now + "\t getPlansButton_Click to get the plans from cx ");
                if (cxIPTextBox.Text.Length != 0)
                {
                    System.Net.IPAddress ipAddress = null;
                    bool isValidIp = System.Net.IPAddress.TryParse(cxIPTextBox.Text, out ipAddress);
                    if (isValidIp == false)
                    {
                        MessageBox.Show("Enter a valid CX IP address", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return;
                    }
                }
                else
                {
                    MessageBox.Show("Enter CX IP address", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return;
                }
                if (portNumberTextBox.Text.Length != 0)
                {
                    string number = portNumberTextBox.Text.Trim();

                    double Num;

                    bool isNum = double.TryParse(number, out Num);
                    if (isNum == false)
                    {
                        MessageBox.Show("Please enter a numeric port number", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return;
                    }
                }
                else
                {
                    MessageBox.Show("Enter CX port number", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return;
                }
                this.Enabled = false;
                progressBar.Visible = true;
                taskGroupBox.Enabled = false;
                getPlansBackgroundWorker.RunWorkerAsync();
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }  
        
        private void recoverRadioButton_CheckedChanged(object sender, EventArgs e)
        {
            try
            {
                if (recoverRadioButton.Checked == true)
                {
                    progressBar.Visible = true;
                    GetMasterConfigFileAndReloadTreeView();
                    ReloadTreeViewForRecovery();
                    progressBar.Visible = false;
                    if (chooseApplicationComboBox.SelectedItem.ToString() == "P2V")
                    {
                        selectedActionForPlan = "P2V";
                    }
                    else
                    {
                        selectedActionForPlan = "ESX";
                    }

                    selectedVMListForPlan = new HostList();
                    masterTargetList = new HostList();

                    planNamesTreeView.CheckBoxes = true;
                    planNamesTreeView.Nodes.Clear();
                    int i = 0;
                    foreach (Host h in sourceHostsList._hostList)
                    {
                        if (h.offlineSync == false)
                        {
                            if (h.failOver == "no" && h.role == Role.Primary)
                            {
                                // Trace.WriteLine(DateTime.Now + "\t Prinitng the vcon name " + h.vConName);
                                if (planNamesTreeView.Nodes.Count == 0)
                                {
                                    planNamesTreeView.Nodes.Add(h.plan);
                                    // planNamesTreeView.Nodes[i].Nodes.Add(h.displayname);
                                    i++;
                                }
                                bool planExists = false;
                                foreach (TreeNode node in planNamesTreeView.Nodes)
                                {
                                    //Trace.WriteLine(DateTime.Now + " \t printing the node names " + node.Name + " plan name " + h.plan);
                                    if (node.Text == h.plan)
                                    {
                                        planExists = true;
                                        node.Nodes.Add(h.displayname);

                                    }
                                }
                                if (planExists == false)
                                {
                                    planNamesTreeView.Nodes.Add(h.plan);
                                    planNamesTreeView.Nodes[i].Nodes.Add(h.displayname);
                                    i++;
                                }
                            }
                        }
                    }
                    if (planNamesTreeView.Nodes.Count == 0)
                    {
                        GetMasterConfigFileAndReloadTreeView();
                        ReloadTreeViewForRecovery();
                        if (planNamesTreeView.Nodes.Count == 0)
                        {
                            planNamesTreeView.Nodes.Add("No plans to recover");
                            planNamesTreeView.CheckBoxes = false;
                            /*AllServersForm allServersForm = new AllServersForm("recovery", "");
                            allServersForm.ShowDialog();
                            recoverRadioButton.Checked = false;*/
                        }
                        else
                        {
                            _planName = "";
                            selectedVMListForPlan = new HostList();
                            masterTargetList = new HostList();
                            planNamesTreeView.CheckBoxes = true;
                            planNamesTreeView.ExpandAll();
                            if (removeDiskRadioButton.Checked == true)
                            {
                                removeDiskPanel.BringToFront();
                            }
                            else
                            {
                                VMDeatilsPanel.BringToFront();
                            }
                            
                        }
                    }
                    else
                    {
                        _planName = "";
                        selectedVMListForPlan = new HostList();
                        masterTargetList = new HostList();
                        planNamesTreeView.CheckBoxes = true;
                        planNamesTreeView.ExpandAll();
                        // VMDeatilsPanel.BringToFront();
                        mainFormPanel.BringToFront();
                    }
                    cancelButton.Text = "Close";
                    resumeButton.Text = "Next";
                    resumeButton.Visible = true;
                    //VMDeatilsPanel.BringToFront();
                    mainFormPanel.BringToFront();
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
        }

        private bool ReloadTreeViewForRecovery()
        {
            try
            {
                planNamesTreeView.CheckBoxes = true;
                planNamesTreeView.Nodes.Clear();
                int i = 0;
                foreach (Host h in sourceHostsList._hostList)
                {
                    if (h.offlineSync == false)
                    {
                        if (h.failOver == "no" && h.role == Role.Primary)
                        {
                            if (drDrillRadioButton.Checked == true && h.machineType == "PhysicalMachine" && h.failback == "yes")
                            {
                            }
                            else
                            {
                                if (planNamesTreeView.Nodes.Count == 0)
                                {
                                    planNamesTreeView.Nodes.Add(h.plan);
                                    // planNamesTreeView.Nodes[i].Nodes.Add(h.displayname);
                                    i++;
                                }
                                bool planExists = false;
                                foreach (TreeNode node in planNamesTreeView.Nodes)
                                {
                                    //Trace.WriteLine(DateTime.Now + " \t printing the node names " + node.Name + " plan name " + h.plan);
                                    if (node.Text == h.plan)
                                    {
                                        planExists = true;
                                        node.Nodes.Add(h.displayname);
                                    }
                                }
                                if (planExists == false)
                                {
                                    planNamesTreeView.Nodes.Add(h.plan);
                                    planNamesTreeView.Nodes[i].Nodes.Add(h.displayname);
                                    i++;
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
            return true;
        }

        private void resumeProtectionRadioButton_CheckedChanged(object sender, EventArgs e)
        {
            try
            {
                if (resumeProtectionRadioButton.Checked == true)
                {
                    _directoryNameinXmlFile = null;
                    string machine = null;
                    if (chooseApplicationComboBox.SelectedItem.ToString() == "P2V")
                    {
                        machine = "PhysicalMachine";
                        selectedActionForPlan = "P2V";
                    }
                    else
                    {
                        machine = "VirtualMachine";
                        selectedActionForPlan = "ESX";
                    }
                    if (removeDiskRadioButton.Checked == true)
                    {
                        removeDiskPanel.BringToFront();
                    }
                    else
                    {
                        VMDeatilsPanel.BringToFront();
                    }
                    // GetMasterConfigFileAndReloadTreeView();
                    selectedVMListForPlan = new HostList();
                    masterTargetList = new HostList();
                    int i = 0;
                    planNamesTreeView.Nodes.Clear();
                    xmlFileMadeSuccessfully = false;
                    readinessCheck = false;
                    checkingForVmsPowerStatus = false;
                    dummyDiskscreated = false;
                    createGuestOnTarget = false;
                    jobautomation = false;
                    upDateMasterFile = false;
                    upLoadFile = false;

                    foreach (Host h in sourceHostsList._hostList)
                    {
                        if (h.offlineSync == false)
                        {
                            if (h.failOver == "yes" && h.failback == "no" && h.role == Role.Primary)
                            {
                                //Now we are going to resume all p2v or v2v vms at a time....
                                //if (h.machineType == machine)
                                {
                                    if (planNamesTreeView.Nodes.Count == 0)
                                    {
                                        planNamesTreeView.Nodes.Add(h.plan);
                                        // planNamesTreeView.Nodes[i].Nodes.Add(h.displayname);

                                        i++;
                                    }
                                    bool planExists = false;
                                    foreach (TreeNode node in planNamesTreeView.Nodes)
                                    {
                                        //Trace.WriteLine(DateTime.Now + " \t printing the node names " + node.Name + " plan name " + h.plan);
                                        if (node.Text == h.plan)
                                        {
                                            planExists = true;
                                            node.Nodes.Add(h.displayname);
                                        }
                                    }
                                    if (planExists == false)
                                    {
                                        planNamesTreeView.Nodes.Add(h.plan);
                                        planNamesTreeView.Nodes[i].Nodes.Add(h.displayname);
                                        i++;
                                    }
                                }
                            }
                        }

                    }
                    Trace.WriteLine(DateTime.Now + "\t printing the count of the nodes" + planNamesTreeView.Nodes.Count.ToString());
                    if (planNamesTreeView.Nodes.Count == 0)
                    {
                        GetPlans();
                        AddPlansForTreeViewForResume();
                        if (planNamesTreeView.Nodes.Count == 0)
                        {
                            planNamesTreeView.Nodes.Add("No plans to resume");
                            planNamesTreeView.CheckBoxes = false;
                        }
                        else
                        {
                            planNamesTreeView.CheckBoxes = true;
                            _planName = "";
                            selectedVMListForPlan = new HostList();
                            masterTargetList = new HostList();
                            planNamesTreeView.ExpandAll();
                            mainFormPanel.BringToFront();
                        }
                        //MessageBox.Show("There are no VMs to Resume ", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    }
                    else
                    {
                        planNamesTreeView.CheckBoxes = true;
                        _planName = "";
                        selectedVMListForPlan = new HostList();
                        masterTargetList = new HostList();
                        planNamesTreeView.ExpandAll();
                        mainFormPanel.BringToFront();
                    }
                    mainFormPanel.BringToFront();
                    resumeButton.Text = "Resume";
                    resumeButton.Visible = true;
                    resumeButton.Enabled = true;
                    cancelButton.Text = "Close";
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
           
        }

        private bool AddPlansForTreeViewForResume()
        {
            try
            {
                string machine = null;

                if (chooseApplicationComboBox.SelectedItem.ToString() == "P2V")
                {
                    machine = "PhysicalMachine";
                    selectedActionForPlan = "P2V";
                }
                else
                {
                    machine = "VirtualMachine";
                    selectedActionForPlan = "ESX";
                }
                selectedVMListForPlan = new HostList();
                masterTargetList = new HostList();
                int i = 0;
                planNamesTreeView.Nodes.Clear();


                foreach (Host h in sourceHostsList._hostList)
                {
                    if (h.offlineSync == false)
                    {
                        if (h.failOver == "yes" && h.failback == "no" && h.role == Role.Primary)
                        {

                            if (h.machineType == machine)
                            {
                                if (planNamesTreeView.Nodes.Count == 0)
                                {
                                    planNamesTreeView.Nodes.Add(h.plan);
                                    // planNamesTreeView.Nodes[i].Nodes.Add(h.displayname);

                                    i++;
                                }
                                bool planExists = false;
                                foreach (TreeNode node in planNamesTreeView.Nodes)
                                {
                                    //Trace.WriteLine(DateTime.Now + " \t printing the node names " + node.Name + " plan name " + h.plan);
                                    if (node.Text == h.plan)
                                    {
                                        planExists = true;
                                        node.Nodes.Add(h.displayname);
                                    }
                                }
                                if (planExists == false)
                                {
                                    planNamesTreeView.Nodes.Add(h.plan);
                                    planNamesTreeView.Nodes[i].Nodes.Add(h.displayname);
                                    i++;
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

        private void addDiskRadioButton_CheckedChanged(object sender, EventArgs e)
        {
            try
            {
                if (addDiskRadioButton.Checked == true)
                {
                    string machineType = null;

                    // GetMasterConfigFileAndReloadTreeView();
                    if (chooseApplicationComboBox.SelectedItem == "ESX".ToString())
                    {
                        machineType = "VirtualMachine";
                    }
                    else
                    {
                        machineType = "PhysicalMachine";
                    }

                    VMDeatilsPanel.BringToFront();
                    int i = 0;
                    planNamesTreeView.Nodes.Clear();
                    foreach (Host h in sourceHostsList._hostList)
                    {
                        if (h.offlineSync == false)
                        {
                            if (h.machineType == machineType && h.role == Role.Primary )
                            {
                                if (h.failOver == "no" && h.failback == "no" )
                                {
                                    if (planNamesTreeView.Nodes.Count == 0)
                                    {
                                        planNamesTreeView.Nodes.Add(h.plan);
                                        //planNamesTreeView.Nodes[i].Nodes.Add(h.displayname);
                                        i++;
                                    }
                                    bool planExists = false;
                                    foreach (TreeNode node in planNamesTreeView.Nodes)
                                    {
                                        // Trace.WriteLine(DateTime.Now + " \t printing the node names " + node.Name + " plan name " + h.plan);
                                        if (node.Text == h.plan)
                                        {
                                            planExists = true;
                                            node.Nodes.Add(h.displayname);
                                        }
                                    }
                                    if (planExists == false)
                                    {
                                        planNamesTreeView.Nodes.Add(h.plan);
                                        planNamesTreeView.Nodes[i].Nodes.Add(h.displayname);
                                        i++;
                                    }
                                }
                            }
                        }
                    }

                    if (planNamesTreeView.Nodes.Count == 0)
                    {
                        if (chooseApplicationComboBox.SelectedItem.ToString() == "P2V")
                        {
                            selectedActionForPlan = "BMR Protection";
                        }
                        else
                        {
                            selectedActionForPlan = "ESX";
                        }
                        planNamesTreeView.Nodes.Add("No plans to add disks");
                        planNamesTreeView.CheckBoxes = false;
                        //AllServersForm allServersForm = new AllServersForm("additionOfDisk", "");
                        //allServersForm.ShowDialog();
                        selectedVMListForPlan = new HostList();
                        masterTargetList = new HostList();
                        //addDiskRadioButton.Checked = false;

                        // MessageBox.Show("There are no VMs to  add disk", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    }
                    else
                    {
                        _planName = "";
                        selectedVMListForPlan = new HostList();
                        masterTargetList = new HostList();
                        planNamesTreeView.CheckBoxes = true;
                        planNamesTreeView.ExpandAll();
                        mainFormPanel.BringToFront();
                        //VMDeatilsPanel.BringToFront();

                    }
                    resumeButton.Text = "Next";
                    resumeButton.Visible = true;
                    cancelButton.Text = "Close";
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
        }

        private void failBackRadioButton_CheckedChanged(object sender, EventArgs e)
        {
            try
            {
                if (failBackRadioButton.Checked == true)
                {
                    int i = 0;
                    selectedVMListForPlan = new HostList();
                    masterTargetList = new HostList();
                    string machine = "";

                    if (chooseApplicationComboBox.SelectedItem.ToString() == "P2V")
                    {
                        machine = "PhysicalMachine";
                        selectedActionForPlan = "V2P";
                    }
                    else
                    {
                        machine = "VirtualMachine";
                        selectedActionForPlan = "ESX";
                    }

                    // GetMasterConfigFileAndReloadTreeView();
                    planNamesTreeView.Nodes.Clear();
                    foreach (Host h in sourceHostsList._hostList)
                    {
                        if (h.offlineSync == false)
                        {
                            if (h.failOver == "yes" && h.failback == "no" && h.role == Role.Primary)
                            {
                                if (h.machineType == machine)
                                {
                                    if (planNamesTreeView.Nodes.Count == 0)
                                    {
                                        planNamesTreeView.Nodes.Add(h.plan);
                                        // planNamesTreeView.Nodes[i].Nodes.Add(h.displayname); 
                                        i++;
                                    }
                                    bool planExists = false;
                                    foreach (TreeNode node in planNamesTreeView.Nodes)
                                    {
                                        //Trace.WriteLine(DateTime.Now + " \t printing the node names " + node.Name + " plan name " + h.plan);
                                        if (node.Text == h.plan)
                                        {
                                            planExists = true;
                                            node.Nodes.Add(h.displayname);
                                        }
                                    }
                                    if (planExists == false)
                                    {
                                        planNamesTreeView.Nodes.Add(h.plan);
                                        planNamesTreeView.Nodes[i].Nodes.Add(h.displayname);
                                        i++;
                                    }
                                }
                            }
                        }
                    }
                    if (planNamesTreeView.Nodes.Count == 0)
                    {
                        GetPlans();
                        AddNodesForTreeViewForFailBack();
                        if (planNamesTreeView.Nodes.Count == 0)
                        {
                            planNamesTreeView.Nodes.Add("No plans to Failback");
                            planNamesTreeView.CheckBoxes = false;
                            //AllServersForm allServersForm = new AllServersForm("failBack", "");
                            //allServersForm.ShowDialog();
                            selectedVMListForPlan = new HostList();
                            masterTargetList = new HostList();
                            resumeButton.Text = "Next";
                            //failBackRadioButton.Checked = false;
                        }
                        else
                        {
                            _planName = "";
                            cancelButton.Text = "Close";
                            resumeButton.Text = "Next";
                            resumeButton.Visible = true;
                            resumeButton.Enabled = true;
                            planNamesTreeView.CheckBoxes = true;
                            selectedVMListForPlan = new HostList();
                            masterTargetList = new HostList();
                            planNamesTreeView.ExpandAll();
                            mainFormPanel.BringToFront();
                            // VMDeatilsPanel.BringToFront();
                        }
                    }
                    else
                    {
                        _planName = "";
                        cancelButton.Text = "Close";
                        resumeButton.Text = "Next";
                        resumeButton.Visible = true;
                        resumeButton.Enabled = true;
                        planNamesTreeView.CheckBoxes = true;
                        selectedVMListForPlan = new HostList();
                        masterTargetList = new HostList();
                        planNamesTreeView.ExpandAll();
                        VMDeatilsPanel.BringToFront();
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
        }


        private bool AddNodesForTreeViewForFailBack()
        {
            try
            {
                string machine = null;
                int i = 0;
                if (chooseApplicationComboBox.SelectedItem.ToString() == "P2V")
                {
                    selectedActionForPlan = "P2V";
                }
                else
                {
                    machine = "VirtualMachine";
                    selectedActionForPlan = "ESX";
                }
                planNamesTreeView.Nodes.Clear();
                foreach (Host h in sourceHostsList._hostList)
                {
                    if (h.offlineSync == false)
                    {

                        if (h.failOver == "yes" && h.failback == "no" && h.role == Role.Primary)
                        {
                            if (h.machineType == machine)
                            {
                                if (planNamesTreeView.Nodes.Count == 0)
                                {
                                    planNamesTreeView.Nodes.Add(h.plan);
                                    // planNamesTreeView.Nodes[i].Nodes.Add(h.displayname);         

                                    i++;
                                }
                                bool planExists = false;
                                foreach (TreeNode node in planNamesTreeView.Nodes)
                                {
                                    //Trace.WriteLine(DateTime.Now + " \t printing the node names " + node.Name + " plan name " + h.plan);
                                    if (node.Text == h.plan)
                                    {
                                        planExists = true;
                                        node.Nodes.Add(h.displayname);
                                    }
                                }
                                if (planExists == false)
                                {
                                    planNamesTreeView.Nodes.Add(h.plan);
                                    planNamesTreeView.Nodes[i].Nodes.Add(h.displayname);
                                    i++;
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

        private void removeRadioButton_CheckedChanged(object sender, EventArgs e)
        {
            try
            {
                if (removeRadioButton.Checked == true)
                {
                    selectedVMListForPlan = new HostList();
                    masterTargetList = new HostList();
                    int i = 0;
                    planNamesTreeView.Nodes.Clear();

                    foreach (Host h in sourceHostsList._hostList)
                    {
                        if (h.offlineSync == false)
                        {
                            if (h.role == Role.Primary)
                            {
                                if (planNamesTreeView.Nodes.Count == 0)
                                {
                                    planNamesTreeView.Nodes.Add(h.plan);
                                    bool sourceVMExists = false;
                                    // planNamesTreeView.Nodes[i].Nodes.Add(h.displayname);
                                    i++;
                                }

                                bool planExists = false;
                                foreach (TreeNode node in planNamesTreeView.Nodes)
                                {
                                    //Trace.WriteLine(DateTime.Now + " \t printing the node names " + node.Name + " plan name " + h.plan);
                                    if (node.Text == h.plan)
                                    {
                                        planExists = true;
                                        node.Nodes.Add(h.displayname);
                                    }
                                }
                                if (planExists == false)
                                {
                                    planNamesTreeView.Nodes.Add(h.plan);
                                    planNamesTreeView.Nodes[i].Nodes.Add(h.displayname);
                                    i++;
                                }
                            }
                        }
                    }
                    if (planNamesTreeView.Nodes.Count == 0)
                    {
                        planNamesTreeView.Nodes.Add("No plans to remove");
                        planNamesTreeView.CheckBoxes = false;
                       /* RemovePairsForm remove = new RemovePairsForm();
                        remove.ShowDialog();
                        removeRadioButton.Checked = false;*/
                    }
                    else
                    {
                        _planName = "";
                        selectedVMListForPlan = new HostList();
                        masterTargetList = new HostList();
                        planNamesTreeView.CheckBoxes = true;
                        planNamesTreeView.ExpandAll();
                        // VMDeatilsPanel.BringToFront();
                        mainFormPanel.BringToFront();
                    }
                    resumeButton.Text = "Remove";
                    resumeButton.Visible = true;
                    cancelButton.Text = "Close";
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
        }

        private void exportRadioButton_CheckedChanged(object sender, EventArgs e)
        {
            try
            {
                if (exportRadioButton.Checked == true)
                {
                    if (selectedActionForPlan == "Esx")
                    {
                        breifLog.WriteLine(DateTime.Now + "\t User selected Offline sync Export operation  for ESX");
                    }
                    else
                    {
                        //_selectedAction = "P2v";
                        breifLog.WriteLine(DateTime.Now + "\t User selected Offline sync Export operation  for P2V");
                    }
                    breifLog.Flush();
                    AllServersForm allServersForm = new AllServersForm("offlineSync", "");
                    allServersForm.ShowDialog();
                    exportRadioButton.Checked = false;
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
        }

        private void offLineSyncImportRadioButton_CheckedChanged_1(object sender, EventArgs e)
        {
            try
            {
                if (offLineSyncImportRadioButton.Checked == true)
                {
                    string machineType = null;

                    // GetMasterConfigFileAndReloadTreeView();
                    if (chooseApplicationComboBox.SelectedItem == "ESX".ToString())
                    {
                        machineType = "VirtualMachine";
                    }
                    else
                    {
                        machineType = "PhysicalMachine";
                    }


                    int i = 0;
                    planNamesTreeView.Nodes.Clear();
                    foreach (Host h in sourceHostsList._hostList)
                    {
                        if (h.offlineSync == true)
                        {
                            //VMDeatilsPanel.BringToFront();
                            if (h.machineType == machineType && h.role == Role.Primary)
                            {
                                if (h.failOver == "no" && h.failback == "no")
                                {
                                    planNamesTreeView.CheckBoxes = true;

                                    if (planNamesTreeView.Nodes.Count == 0)
                                    {
                                        planNamesTreeView.Nodes.Add(h.plan);
                                        //planNamesTreeView.Nodes[i].Nodes.Add(h.displayname);
                                        i++;
                                    }

                                    bool planExists = false;
                                    foreach (TreeNode node in planNamesTreeView.Nodes)
                                    {
                                        // Trace.WriteLine(DateTime.Now + " \t printing the node names " + node.Name + " plan name " + h.plan);
                                        if (node.Text == h.plan)
                                        {
                                            planExists = true;
                                            node.Nodes.Add(h.displayname);                                    
                                        }
                                    }
                                    if (planExists == false)
                                    {
                                        planNamesTreeView.Nodes.Add(h.plan);
                                        planNamesTreeView.Nodes[i].Nodes.Add(h.displayname);
                                        
                                        i++;
                                    }
                                }
                            }
                        }
                    }
                    planNamesTreeView.ExpandAll();
                    resumeButton.Text = "Next";
                    resumeButton.Visible = true;
                    cancelButton.Text = "Close";
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
            
        }
        
        private void editMasterConfigfileRadioButton_CheckedChanged(object sender, EventArgs e)
        {
            try
            {
                if (editMasterConfigfileRadioButton.Checked == true)
                {
                    breifLog.WriteLine(DateTime.Now + " \t User has seelcted the edit master configFile option");
                    breifLog.Flush();
                    EditingMasterXmlFile edit = new EditingMasterXmlFile();
                    edit.ShowDialog();
                    editMasterConfigfileRadioButton.Checked = false;
                    cancelButton.Visible = true;
                    cancelButton.Text = "Close";
                    offlineSyncExportRadioButton.Checked = false;
                    newProtectionRadioButton.Checked = false;
                    pushAgentRadioButton.Checked = false;
                    manageExistingPlanRadioButton.Checked = false;
                    _directoryNameinXmlFile = null;
                    taskGroupBox.Enabled = false;
                    planNamesTreeView.Nodes.Clear();
                    progressBar.Visible = true;
                    getPlansBackgroundWorker.RunWorkerAsync();
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
        }

        private void clearLogsLinkLabel_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
        {
            logTextBox.Clear();
        }

        private void versionLabel_MouseEnter(object sender, EventArgs e)
        {
            try
            {
                System.Windows.Forms.ToolTip toolTipTodisplaybuildDate = new System.Windows.Forms.ToolTip();
                toolTipTodisplaybuildDate.AutoPopDelay = 5000;
                toolTipTodisplaybuildDate.InitialDelay = 1000;
                toolTipTodisplaybuildDate.ReshowDelay = 500;
                toolTipTodisplaybuildDate.ShowAlways = true;
                toolTipTodisplaybuildDate.GetLifetimeService();
                toolTipTodisplaybuildDate.IsBalloon = false;
                toolTipTodisplaybuildDate.UseAnimation = true;
                toolTipTodisplaybuildDate.SetToolTip(versionLabel, HelpForcx.BuildDate);
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");

            }

        }

        private void resumeProtectionMoreLabel_Click(object sender, EventArgs e)
        {
            if (File.Exists(installPath + "\\Manual.chm"))
            {
                Help.ShowHelp(null, installPath + "\\Manual.chm", HelpNavigator.TopicId, ResumeProtection);
            } 
        }

        private void pushAgentMoreLabel_Click(object sender, EventArgs e)
        {
            if (File.Exists(installPath + "\\Manual.chm"))
            {
                Help.ShowHelp(null, installPath + "\\Manual.chm", HelpNavigator.TopicId, Pushagent);
            } 

        }   
       

        
        

        private void patchLabel_MouseEnter(object sender, EventArgs e)
        {
            try
            {
                if (File.Exists(installPath + " \\patch.log"))
                {
                    System.Windows.Forms.ToolTip toolTip = new System.Windows.Forms.ToolTip();
                    toolTip.AutoPopDelay = 5000;
                    toolTip.InitialDelay = 1000;
                    toolTip.ReshowDelay = 500;
                    toolTip.ShowAlways = true;
                    toolTip.GetLifetimeService();
                    toolTip.IsBalloon = true;

                    toolTip.UseAnimation = true;
                    StreamReader sr = new StreamReader(installPath + " \\patch.log");

                    string s = sr.ReadToEnd().ToString();
                    toolTip.SetToolTip(patchLabel, s);
                    sr.Close();

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
        }

        private void drDrillRadioButton_CheckedChanged(object sender, EventArgs e)
        {
            try
            {
                if (drDrillRadioButton.Checked == true)
                {
                    progressBar.Visible = true;
                    GetMasterConfigFileAndReloadTreeView();
                    ReloadTreeViewForRecovery();
                    progressBar.Visible = false;
                    if (chooseApplicationComboBox.SelectedItem.ToString() == "P2V")
                    {
                        selectedActionForPlan = "P2V";
                    }
                    else
                    {
                        selectedActionForPlan = "ESX";
                    }

                    selectedVMListForPlan = new HostList();
                    masterTargetList = new HostList();

                    planNamesTreeView.CheckBoxes = true;
                    planNamesTreeView.Nodes.Clear();
                    int i = 0;
                    foreach (Host h in sourceHostsList._hostList)
                    {
                        if (h.offlineSync == false)
                        {
                            if (h.failOver == "no" && h.role == Role.Primary)
                            {
                                if (h.machineType == "PhysicalMachine" && h.failback == "yes")
                                {
                                }
                                else
                                {
                                    // Trace.WriteLine(DateTime.Now + "\t Prinitng the vcon name " + h.vConName);
                                    if (planNamesTreeView.Nodes.Count == 0)
                                    {
                                        planNamesTreeView.Nodes.Add(h.plan);
                                        // planNamesTreeView.Nodes[i].Nodes.Add(h.displayname);
                                        i++;
                                    }
                                    bool planExists = false;
                                    foreach (TreeNode node in planNamesTreeView.Nodes)
                                    {
                                        //Trace.WriteLine(DateTime.Now + " \t printing the node names " + node.Name + " plan name " + h.plan);
                                        if (node.Text == h.plan)
                                        {
                                            planExists = true;
                                            node.Nodes.Add(h.displayname);
                                        }
                                    }
                                    if (planExists == false)
                                    {
                                        planNamesTreeView.Nodes.Add(h.plan);
                                        planNamesTreeView.Nodes[i].Nodes.Add(h.displayname);
                                        i++;
                                    }
                                }
                            }
                        }
                    }
                    if (planNamesTreeView.Nodes.Count == 0)
                    {
                        GetMasterConfigFileAndReloadTreeView();
                        ReloadTreeViewForRecovery();
                        if (planNamesTreeView.Nodes.Count == 0)
                        {

                        }
                        else
                        {
                            _planName = "";
                            selectedVMListForPlan = new HostList();
                            masterTargetList = new HostList();
                            planNamesTreeView.CheckBoxes = true;
                            planNamesTreeView.ExpandAll();
                            VMDeatilsPanel.BringToFront();
                        }
                    }
                    else
                    {
                        _planName = "";
                        selectedVMListForPlan = new HostList();
                        masterTargetList = new HostList();
                        planNamesTreeView.CheckBoxes = true;
                        planNamesTreeView.ExpandAll();
                        // VMDeatilsPanel.BringToFront();
                        mainFormPanel.BringToFront();
                    }
                    cancelButton.Text = "Close";
                    resumeButton.Text = "Next";
                    resumeButton.Visible = true;
                    //VMDeatilsPanel.BringToFront();
                    mainFormPanel.BringToFront();
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
        }

        private void getPlansBackgroundWorker_DoWork(object sender, System.ComponentModel.DoWorkEventArgs e)
        {
            try
            {
                if (getplans == true)
                {
                    getplans = false;

                    if (cxIPTextBox.Text != WinTools.GetcxIP())
                    {
                        WinPreReqs win = new WinPreReqs("", "", "", "");
                        Host vConHost = new Host();
                        vConHost.hostname = Environment.MachineName;
                        win.SetHostIPinputhostname(vConHost.hostname,  vConHost.ip, WinTools.GetcxIPWithPortNumber());
                        vConHost.ip = WinPreReqs.GetIPaddress;
                        win.MasterTargetPreCheck( vConHost, WinTools.GetcxIPWithPortNumber());
                        vConHost = WinPreReqs.GetHost;
                        if (vConHost.machineInUse == true)
                        {

                            MessageBox.Show("vContinuum box has some fx jobs running. Try after some time... ", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            return;

                        }
                        else
                        {
                            WinTools.UpdatecxIPandPortNumber(cxIPTextBox.Text, portNumberTextBox.Text);
                        }
                    }
                }


                xmlFileMadeSuccessfully = false;
                readinessCheck = false;
                checkingForVmsPowerStatus = false;
                dummyDiskscreated = false;
                createGuestOnTarget = false;
                jobautomation = false;
                upDateMasterFile = false;
                upLoadFile = false;

                sourceHostsList = new HostList();
                _masterHost = new Host();
                selectedVMListForPlan = new HostList();
                mastertargetsHostList = new HostList();
                masterTargetList = new HostList();
                xmlFileMadeSuccessfully = false;
                object o = new object();
                EventArgs ex = new EventArgs();
                homePicturePictureBox_Click(o, ex);
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");

              
            }

        }

        private void getPlansBackgroundWorker_ProgressChanged(object sender, System.ComponentModel.ProgressChangedEventArgs e)
        {

        }

        private void getPlansBackgroundWorker_RunWorkerCompleted(object sender, System.ComponentModel.RunWorkerCompletedEventArgs e)
        {
            try
            {
                logTextBox.Clear();
                taskGroupBox.Enabled = true;
                this.Enabled = true;
                planNamesTreeView.Nodes.Clear();
                offlineSyncExportRadioButton.Checked = false;
                manageExistingPlanRadioButton.Checked = false;
                newProtectionRadioButton.Checked = false;
                pushAgentRadioButton.Checked = false;
                cancelButton.Visible = true;
                cancelButton.Text = "Close";
                resumeButton.Text = "Next";
                offlineSyncExportRadioButton.Checked = false;
                offLineSyncImportRadioButton.Checked = false;
                newProtectionRadioButton.Checked = false;
                pushAgentRadioButton.Checked = false;
                manageExistingPlanRadioButton.Checked = false;
                administrativeTaksRadioButton.Checked = false;
                volumeReSizeRadioButton.Checked = false;
                removeDiskRadioButton.Checked = false;
                removeVolumeRadioButton.Checked = false;
                pushAgentRadioButton.Checked = false;
                upgradeRadioButton.Checked = false;
                rescueUSBRadioButton.Checked = false;

                _directoryNameinXmlFile = null;

                planNamesTreeView.Nodes.Clear();

                this.Enabled = true;
                if (sourceHostsList._hostList.Count != 0)
                {
                    LoadTreeView();
                }
                else
                {
                    mainFormPanel.BringToFront();
                    VMDeatilsPanel.SendToBack();
                    planNamesTreeView.CheckBoxes = false;
                    planNamesTreeView.Nodes.Add("No plans found");
                }
                resumeButton.Enabled = true;
                cancelButton.Enabled = true;
                getPlansButton.Enabled = true;
                progressBar.Visible = false;
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");

                
            }
        }

        private void drDrillMoreLabel_Click(object sender, EventArgs e)
        {
            try
            {
                if (File.Exists(installPath + "\\Manual.chm"))
                {
                    Help.ShowHelp(null, installPath + "\\Manual.chm", HelpNavigator.TopicId, Drdrill);
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
        }

        private void upgradeRadioButton_CheckedChanged(object sender, EventArgs e)
        {
            try
            {
                if (upgradeRadioButton.Checked == true)
                {
                    Upgrade upgrade = new Upgrade();
                    upgrade.ShowDialog();
                    upgradeRadioButton.Checked = false;
                    cancelButton.Visible = true;
                    cancelButton.Text = "Close";
                    offlineSyncExportRadioButton.Checked = false;
                    newProtectionRadioButton.Checked = false;
                    pushAgentRadioButton.Checked = false;
                    manageExistingPlanRadioButton.Checked = false;
                    _directoryNameinXmlFile = null;
                    taskGroupBox.Enabled = false;
                    planNamesTreeView.Nodes.Clear();
                    progressBar.Visible = true;
                    getPlansBackgroundWorker.RunWorkerAsync();
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
        }       

        private void upgradeMoreLabel_Click(object sender, EventArgs e)
        {
            try
            {
                if (File.Exists(installPath + "\\Manual.chm"))
                {
                    Help.ShowHelp(null, installPath + "\\Manual.chm", HelpNavigator.TopicId, Upgrade);
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
        }


        private bool RefreshHosts(HostList _selectedHostList)
        {
            try
            {




                foreach (Host h in _selectedHostList._hostList)
                {
                    Host h1 = h;
                    string cxip = WinTools.GetcxIPWithPortNumber();
                    WinPreReqs win = new WinPreReqs("", "", "", "");
                    if (h1.inmage_hostid == null)
                    {
                        if (win.GetDetailsFromcxcli( h1, cxip) == true)
                        {
                            h1 = WinPreReqs.GetHost;
                            h1.inmage_hostid = h.inmage_hostid;
                        }
                    }
                    Cxapicalls cxApis = new Cxapicalls();
                    if (h.os == OStype.Windows)
                    {
                        if (cxApis.PostRefreshWithmbrOnly(h1) == true)
                        {
                            h1 = Cxapicalls.GetHost;
                            h.requestId = h1.requestId;
                            Trace.WriteLine(DateTime.Now + "\t Successfully request for refresh host info with request id " + h.requestId);
                        }
                        else
                        {
                            if (cxApis.PostRefreshWithmbrOnly(h1) == true)
                            {
                                h1 = Cxapicalls.GetHost;
                                h.requestId = h1.requestId;
                                Trace.WriteLine(DateTime.Now + "\t Successfully request for refresh host info with request id " + h.requestId);
                            }
                        }
                    }
                    else
                    {
                        if (cxApis.Post( h1, "RefreshHostInfo") == true)
                        {
                            h1 = Cxapicalls.GetHost;
                            h.requestId = h1.requestId;
                            Trace.WriteLine(DateTime.Now + "\t Successfully request for refresh host info with request id " + h.requestId);
                        }
                        else
                        {
                            if (cxApis.Post( h1,"RefreshHostInfo") == true)
                            {
                                h1 = Cxapicalls.GetHost;
                                h.requestId = h1.requestId;
                                Trace.WriteLine(DateTime.Now + "\t Successfully request for refresh host info with request id " + h.requestId);
                            }
                        }
                    }
                }


                if (selectedActionForPlan == "V2P")
                {
                    Host h = (Host)_selectedHostList._hostList[0];
                    Cxapicalls cxApis = new Cxapicalls();
                    if (h.clusterInmageUUIds != null)
                    {
                        string[] inmageUUIdsList = h.clusterInmageUUIds.Split(',');
                        foreach (string s in inmageUUIdsList)
                        {
                            if (h.inmage_hostid != s)
                            {
                                Host h1 = new Host();
                                h1.inmage_hostid = s;
                                cxApis.PostRefreshWithmbrOnly(h1);
                                h1=Cxapicalls.GetHost;
                                Trace.WriteLine(DateTime.Now + "\t after refreshing we are waiting for 15 seconds sleep for each machine in case of V2P ");
                                Thread.Sleep(15000);
                            }
                        }
                    }
                }

                Trace.WriteLine(DateTime.Now + "\t Successfully post the request for all the hosts. Now going to sleep for 65 seconds ");
                Thread.Sleep(65000);

                foreach (Host h in _selectedHostList._hostList)
                {

                    Host tempHost = new Host();
                    tempHost.requestId = h.requestId;
                    Cxapicalls cxApis = new Cxapicalls();

                    int i =1;

                    while (i < 10)
                    {
                        if (cxApis.Post(tempHost, "GetRequestStatus") == true)
                        {
                            tempHost = Cxapicalls.GetHost;
                            Trace.WriteLine(DateTime.Now + "\t Successfully get request status " + h.displayname + " new displayname " + h.new_displayname);
                            Host h1 = h;
                            OutPutOfRefreshHostStatus(ref h1, tempHost);
                            if (h.inmage_hostid == null)
                            {
                                h.inmage_hostid = h1.inmage_hostid;
                            }
                            if (h.mbr_path == null)
                            {
                                h.mbr_path = h1.mbr_path;
                            }
                            if (h.system_volume == null)
                            {
                                h.system_volume = h1.system_volume;
                            }
                            if (h.machineType.ToUpper() == "PHYSICALMACHINE" && h.os == OStype.Windows)
                            {
                                h.disks.list = h1.disks.list;
                            }
						if (h.clusterMBRFiles == null)
                        {
                            h.clusterMBRFiles = h1.clusterMBRFiles;
                        }
                        if (h.clusterNodes == null)
                        {
                            h.clusterNodes = h1.clusterNodes;
                        }
                        if (h.clusterInmageUUIds == null)
                        {
                            h.clusterInmageUUIds = h1.clusterInmageUUIds;
                        }
                        if (h.clusterName == null)
                        {
                            h.clusterName = h1.clusterName;
                        }
                            i = 11;
                            break;
                        }
                        else
                        {
                            i++;
                            Trace.WriteLine(DateTime.Now + "\t going to sleep for " + i.ToString() + " time");
                            Thread.Sleep(65000);
                            
                        }
                    }

                  
                    


                }
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: RefreshHosts " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");


            }
            return true;
        }
        private bool OutPutOfRefreshHostStatus(ref Host h, Host tempHost)
        {
            if (h.inmage_hostid == null)
            {
                h.inmage_hostid = tempHost.inmage_hostid;
            }
            if (h.mbr_path == null)
            {
                h.mbr_path = tempHost.mbr_path;
            }
            if (h.system_volume == null)
            {
                h.system_volume = tempHost.system_volume;
                string partitionVolume = null;
                foreach (Hashtable partition in tempHost.disks.partitionList)
                {
                    if (partition["IsSystemVolume"] != null && partition["Name"] != null)
                    {
                        if (bool.Parse(partition["IsSystemVolume"].ToString()))
                        {
                            if (partitionVolume == null)
                            {
                                partitionVolume = partition["Name"].ToString();
                            }
                            else
                            {
                                partitionVolume = partitionVolume + "," + partition["Name"].ToString();
                            }
                        }
                    }
                    
                }
                if (partitionVolume != null)
                {
                    string[] partitions = partitionVolume.Split(',');
                    if (partitions.Length >= 1)
                    {
                        if (h.os == OStype.Windows)
                        {
                            if (partitionVolume.Contains("C"))
                            {
                                h.system_volume = "C";
                            }
                            else
                            {
                                h.system_volume = partitions[0];
                            }
                        }
                        else if (h.os == OStype.Linux)
                        {

                            h.system_volume = partitionVolume;
                        }
                    }
                    else
                    {
                        h.system_volume = partitionVolume;
                    }

                    Trace.WriteLine(DateTime.Now + "\t System volume for the host: " + h.displayname + " is " + h.system_volume);
                }
            }

            if (h.machineType.ToUpper() == "PHYSICALMACHINE" && h.os == OStype.Windows)
            {

                foreach (Hashtable hash in h.disks.list)
                {
                    foreach (Hashtable tempHash in tempHost.disks.list)
                    {
                        if (hash["src_devicename"] != null && tempHash["src_devicename"] != null)
                        {
                            if (hash["src_devicename"].ToString() == tempHash["src_devicename"].ToString())
                            {
                                hash["scsi_mapping_host"] = tempHash["scsi_mapping_host"].ToString();
                            }
                        }
                        else
                        {
                            if (hash["Name"] != null && tempHash["Name"] != null)
                            {
                                if (hash["Name"].ToString() == tempHash["Name"].ToString())
                                {
                                    hash["scsi_mapping_host"] = tempHash["scsi_mapping_host"].ToString();
                                    hash["src_devicename"] = tempHash["src_devicename"].ToString();
                                }
                            }
                        }
                    }
                }
            }
            return true;
        }

        private void monitorRadioButton_CheckedChanged(object sender, EventArgs e)
        {
            if (monitorRadioButton.Checked == true)
            {
                Monitor monitor = new Monitor();
                monitor.ShowDialog();
                monitorRadioButton.Checked = false;
            }
        }

        private void v2pBackgroundWorker_DoWork(object sender, System.ComponentModel.DoWorkEventArgs e)
        {
            try
            {
                if (RefreshHosts(selectedVMListForPlan) == false)
                {
                    v2pRefesh = false;
                }
                else
                {
                    v2pRefesh = true;
                }
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed to refresh hosts v2p backgroundworker do work" + ex.Message);
            }
           
        }

        private void v2pBackgroundWorker_RunWorkerCompleted(object sender, System.ComponentModel.RunWorkerCompletedEventArgs e)
        {
            try
            {
                if (v2pRefesh == true)
                {
                    planNamesTreeView.Enabled = true;

                    AllServersForm allServersFrom = new AllServersForm("v2p", "");
                    allServersFrom.ShowDialog();
                    getPlansBackgroundWorker.RunWorkerAsync();
                }
                else
                {
                    logTextBox.AppendText("Failed to fetch information from recovered machine" + Environment.NewLine);
                }
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed at v2p backgroundworker completed " + ex.Message);
            }
           
        }

        private void rescueUSBRadioButton_CheckedChanged(object sender, EventArgs e)
        {
            try
            {
                if (File.Exists(installPath + "\\RescueUSB.exe"))
                {
                    if (rescueUSBRadioButton.Checked == true)
                    {
                        progressBar.Visible = true;
                        logTextBox.AppendText("Launching Rescue USB Wizard" + Environment.NewLine);
                        rescueUSBBackgroundWorker.RunWorkerAsync();
                       
                    }
                }
                else
                {
                    Trace.WriteLine(DateTime.Now + "\t Rescue operation can't be done. RescueUSB.exe is missing in vContinuum folder");
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: rescueUSBRadioButton_CheckedChanged " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");

            }
        }

        private void rescueUSBBackgroundWorker_DoWork(object sender, System.ComponentModel.DoWorkEventArgs e)
        {
            try
            {
                WinTools.ExecuteRescueusb(6000000);
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: rescueUSBBackgroundWorker_DoWork " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");

            }
        }

        private void rescueUSBBackgroundWorker_ProgressChanged(object sender, System.ComponentModel.ProgressChangedEventArgs e)
        {

        }

        private void rescueUSBBackgroundWorker_RunWorkerCompleted(object sender, System.ComponentModel.RunWorkerCompletedEventArgs e)
        {
            progressBar.Visible = false;
            rescueUSBRadioButton.Checked = false;
        }

        private void administrativeTaksRadioButton_CheckedChanged(object sender, EventArgs e)
        {
            if (administrativeTaksRadioButton.Checked == true)
            {
                administrativeTasksPanel.Location = new System.Drawing.Point(6, 30);
                administrativeTasksPanel.Visible = true;
               

            }
            else
            {
                administrativeTasksPanel.Visible = false;
                administrativeTasksPanel.Location = new System.Drawing.Point(18, 72);
                volumeReSizeRadioButton.Checked = false;
                rescueUSBRadioButton.Checked = false;
                upgradeRadioButton.Checked = false;
                pushAgentRadioButton.Checked = false;
                editMasterConfigfileRadioButton.Checked = false;
                volumeReSizeRadioButton.Checked = false;
                removeDiskRadioButton.Checked = false;
                removeVolumeRadioButton.Checked = false;
                pushAgentRadioButton.Checked = false;
                upgradeRadioButton.Checked = false;
                rescueUSBRadioButton.Checked = false;
                resyncRadioButton.Checked = false;

            }
        }

        private void volumeReSizeRadioButton_CheckedChanged(object sender, EventArgs e)
        {

            if (volumeReSizeRadioButton.Checked == true)
            {
                try
                {
                    removeDisksDataGridView.Rows.Clear();
                }
                catch (Exception ex)
                {
                    Trace.WriteLine(DateTime.Now + "\t Failed to clear the rows " + ex.Message);
                }
                selectedVMListForPlan = new HostList();
                mainFormPanel.BringToFront();
                resumeButton.Enabled = true;
                resumeButton.Visible = true;
                GetPlansAndVolumeResizeHosts();
                clusterNoteLabel.Visible = true;

                logTextBox.Text = "Note: " +
                                  "1.For Windows MSCS cluster servers user need to use manual resize procedure." + Environment.NewLine +
                                  "2.Resizing the volume to new windows dynamic disk, User needs to use Add disk option." + Environment.NewLine +
                                  "3.Volume resize for both windows and linux is not supported for V2P plans from vContinuum wizard" + Environment.NewLine + 
                                  "4.Resize of volumes with older Scout agent i.e 7.1 U1 or earlier, user need to perform source side activites manually for windows and linux(Need to run drvutil command. Further refer to online help Administrative tasks->Volume resize section.)";



            }
            else
            {
                logTextBox.Text = "";
            }
        }


        private bool GetResizeList()
        {
            try
            {
                ArrayList _planList = new ArrayList();
                Cxapicalls cxApi = new Cxapicalls();
                cxApi.PostToGetAllPlans( _planList);
                _planList = Cxapicalls.GetArrayList;

                Trace.WriteLine(DateTime.Now + "\t Count of plans " + _planList.Count.ToString());
                int i = 0;
                ArrayList planIdList = new ArrayList();
                foreach (Hashtable hash in _planList)
                {
                    if (hash["PlanName"] != null && hash["PlanId"] != null && hash["PlanType"] != null)
                    {
                        if (hash["PlanType"].ToString() == "Adddisk" || hash["PlanType"].ToString() == "Failback" || hash["PlanType"].ToString() == "Resume" || hash["PlanType"].ToString() == "Protection" || hash["PlanType"].ToString() == "Resize")
                        {
                            if (planIdList.Count == 0)
                            {
                                planIdList.Add(hash["PlanId"].ToString());
                            }
                            else
                            {
                                bool planIDExists = false;
                                foreach (string s in planIdList)
                                {
                                    if (s == hash["PlanId"].ToString())
                                    {
                                        planIDExists = true;
                                    }
                                }
                                if (planIDExists == false)
                                {
                                    planIdList.Add(hash["PlanId"].ToString());
                                }
                            }
                        }
                    }
                }

                ArrayList sourceList = new ArrayList();
                Trace.WriteLine(DateTime.Now + "\t Plan id count " + planIdList.Count.ToString());
                if (planIdList.Count != 0)
                {
                    cxApi.PostToGetVolumeResizeInfo( sourceList, planIdList);
                    sourceList = Cxapicalls.GetArrayList;
                }
                if (sourceList.Count != 0)
                {
                    foreach (Host h in sourceHostsList._hostList)
                    {
                        foreach (Hashtable hash in sourceList)
                        {
                            if (hash["HostId"] != null)
                            {
                                Trace.WriteLine(DateTime.Now + "\t printing the host ids  " + h.inmage_hostid + "\t hash " + hash["HostId"].ToString());
                                if (h.inmage_hostid.ToUpper() == hash["HostId"].ToString().ToUpper())
                                {
                                    MessageBox.Show("Volume re-size is pending for the machine " + h.displayname + ". First perfrom re-szire operation and then perfrom Add disk operation.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                    return false;
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
            return true;
        }


        private bool GetPlansAndVolumeResizeHosts()
        {
            planNamesTreeView.Nodes.Clear();

            ArrayList _planList = new ArrayList();
            Cxapicalls cxApi = new Cxapicalls();
            cxApi.PostToGetAllPlans( _planList);
            _planList = Cxapicalls.GetArrayList;
            Trace.WriteLine(DateTime.Now + "\t Count of plans " + _planList.Count.ToString());
            int i = 0;
            ArrayList planIdList = new ArrayList();
            foreach (Hashtable hash in _planList)
            {
                if (hash["PlanName"] != null && hash["PlanId"] != null && hash["PlanType"] != null)
                {
                    if (hash["PlanType"].ToString() == "Adddisk" || hash["PlanType"].ToString() == "Failback" || hash["PlanType"].ToString() == "Resume" || hash["PlanType"].ToString() == "Protection" || hash["PlanType"].ToString() == "Resize")
                    {
                        if (planIdList.Count == 0)
                        {
                            planIdList.Add(hash["PlanId"].ToString());
                        }
                        else
                        {
                            bool planIDExists = false;
                            foreach (string s in planIdList)
                            {
                                if (s == hash["PlanId"].ToString())
                                {
                                    planIDExists = true;
                                }
                            }
                            if (planIDExists == false)
                            {
                                planIdList.Add(hash["PlanId"].ToString());
                            }
                        }
                    }
                }
            }

            ArrayList sourceList = new ArrayList();
            Trace.WriteLine(DateTime.Now + "\t Plan id count " + planIdList.Count.ToString());
            if (planIdList.Count != 0)
            {
                cxApi.PostToGetVolumeResizeInfo( sourceList, planIdList);
                sourceList = Cxapicalls.GetArrayList;
                Trace.WriteLine(DateTime.Now + "\t Count of list  " + sourceList.Count.ToString());
            }
            string machineType = null;
            if (chooseApplicationComboBox.SelectedItem.ToString() == "P2V")
            {
                selectedActionForPlan = "P2VRESIZE";
                machineType = "PhysicalMachine";
            }
            else
            {
                selectedActionForPlan = "ESXRESIZE";
                machineType = "VirtualMachine";
            }

            int j = 0;
            if (sourceList.Count != 0)
            {
                foreach (Host h in sourceHostsList._hostList)
                {
                    foreach (Hashtable hash in sourceList)
                    {
                        if (hash["HostId"] != null)
                        {
                            Trace.WriteLine(DateTime.Now + "\t printing the host ids  " + h.inmage_hostid + "\t hash " + hash["HostId"].ToString());

                            if(hash["planname"]!= null)
                            {
                                Trace.WriteLine(DateTime.Now + "\t Printing plan name hash " + hash["planname"].ToString());
                            }
                            Trace.WriteLine(DateTime.Now + "\t Printing plan name " + h.plan );
                            if (hash["planname"] != null && h.inmage_hostid != null && h.plan != null)
                            {
                                Trace.WriteLine(DateTime.Now + "\t Comparing uuids and plan name " + h.inmage_hostid + "\t hash " + hash["HostId"].ToString() + "\t plan " + h.plan + "\t hash plan " + hash["planname"].ToString());
                                if (h.inmage_hostid.ToUpper() == hash["HostId"].ToString().ToUpper() && h.plan == hash["planname"].ToString())
                                {
                                    Trace.WriteLine(DateTime.Now + "\t Ids and plan names matched ");
                                    planNamesTreeView.CheckBoxes = true;
                                    planNamesTreeView.ExpandAll();
                                    if (h.offlineSync == false)
                                    {

                                        if (h.machineType == "PhysicalMachine" && h.failback == "yes")
                                        {
                                            Trace.WriteLine(DateTime.Now + "\t It seems this machine is physical and v2p protection " + h.displayname);
                                        }
                                        else
                                        {
                                            if (h.failOver == "no" && h.role == Role.Primary && machineType == h.machineType )
                                            {
                                                // Trace.WriteLine(DateTime.Now + "\t Prinitng the vcon name " + h.vConName);
                                                if (planNamesTreeView.Nodes.Count == 0)
                                                {
                                                    planNamesTreeView.Nodes.Add(h.plan);
                                                    // planNamesTreeView.Nodes[i].Nodes.Add(h.displayname);
                                                    j++;
                                                }
                                                bool planExists = false;
                                                foreach (TreeNode node in planNamesTreeView.Nodes)
                                                {
                                                    //Trace.WriteLine(DateTime.Now + " \t printing the node names " + node.Name + " plan name " + h.plan);
                                                    if (node.Text == h.plan)
                                                    {
                                                        planExists = true;
                                                        node.Nodes.Add(h.displayname);

                                                    }
                                                }
                                                if (planExists == false)
                                                {
                                                    planNamesTreeView.Nodes.Add(h.plan);
                                                    planNamesTreeView.Nodes[j].Nodes.Add(h.displayname);
                                                    j++;
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                            else
                            {
                                if (h.inmage_hostid != null)
                                {
                                    if (h.inmage_hostid.ToUpper() == hash["HostId"].ToString().ToUpper())
                                    {

                                        planNamesTreeView.CheckBoxes = true;
                                        planNamesTreeView.ExpandAll();
                                        if (h.offlineSync == false)
                                        {
                                            if (h.machineType == "PhysicalMachine" && h.failback == "yes")
                                            {
                                                Trace.WriteLine(DateTime.Now + "\t It seems this machine is physical and v2p protection " + h.displayname);
                                            }
                                            else
                                            {
                                                if (h.failOver == "no" && h.role == Role.Primary && machineType == h.machineType && h.cluster == "no")
                                                {
                                                    // Trace.WriteLine(DateTime.Now + "\t Prinitng the vcon name " + h.vConName);
                                                    if (planNamesTreeView.Nodes.Count == 0)
                                                    {
                                                        planNamesTreeView.Nodes.Add(h.plan);
                                                        // planNamesTreeView.Nodes[i].Nodes.Add(h.displayname);
                                                        j++;
                                                    }
                                                    bool planExists = false;
                                                    foreach (TreeNode node in planNamesTreeView.Nodes)
                                                    {
                                                        //Trace.WriteLine(DateTime.Now + " \t printing the node names " + node.Name + " plan name " + h.plan);
                                                        if (node.Text == h.plan)
                                                        {
                                                            planExists = true;
                                                            node.Nodes.Add(h.displayname);

                                                        }
                                                    }
                                                    if (planExists == false)
                                                    {
                                                        planNamesTreeView.Nodes.Add(h.plan);
                                                        planNamesTreeView.Nodes[j].Nodes.Add(h.displayname);
                                                        j++;
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
            }

            if (planNamesTreeView.Nodes.Count == 0)
            {
                planNamesTreeView.Nodes.Add("No plans to Resize");
                planNamesTreeView.CheckBoxes = false;
            }
            else
            {
                planNamesTreeView.ExpandAll();
            }
            return true;
        }

        private void removeDiskRadioButton_CheckedChanged(object sender, EventArgs e)
        {
            try
            {
                if (removeDiskRadioButton.Checked == true)
                {
                    progressBar.Visible = true;
                    GetMasterConfigFileAndReloadTreeView();
                    ReloadTreeViewForRecovery();
                    progressBar.Visible = false;
                    if (chooseApplicationComboBox.SelectedItem.ToString() == "P2V")
                    {
                        selectedActionForPlan = "P2V";
                    }
                    else
                    {
                        selectedActionForPlan = "ESX";
                    }

                    selectedVMListForPlan = new HostList();
                    masterTargetList = new HostList();

                    planNamesTreeView.CheckBoxes = true;
                    planNamesTreeView.Nodes.Clear();
                    int i = 0;
                    foreach (Host h in sourceHostsList._hostList)
                    {
                        if (h.offlineSync == false)
                        {
                            if (h.failOver == "no" && h.role == Role.Primary)
                            {
                                // Trace.WriteLine(DateTime.Now + "\t Prinitng the vcon name " + h.vConName);

                                foreach (Hashtable disk in h.disks.list)
                                {
                                    if (disk["remove"] == null)
                                    {
                                        disk.Add("remove", "false");
                                    }
                                }
                                if (planNamesTreeView.Nodes.Count == 0)
                                {
                                    if (h.machineType == "PhysicalMachine" && h.failback == "yes")
                                    {
                                        Trace.WriteLine(DateTime.Now + "\t It seems this machine is physical and v2p protection " + h.displayname);
                                    }
                                    else
                                    {
                                        planNamesTreeView.Nodes.Add(h.plan);
                                    }
                                    // planNamesTreeView.Nodes[i].Nodes.Add(h.displayname);
                                    i++;
                                }
                                bool planExists = false;
                                foreach (TreeNode node in planNamesTreeView.Nodes)
                                {
                                    //Trace.WriteLine(DateTime.Now + " \t printing the node names " + node.Name + " plan name " + h.plan);
                                    if (node.Text == h.plan)
                                    {

                                        planExists = true;
                                        if (h.machineType == "PhysicalMachine" && h.failback == "yes")
                                        {
                                            Trace.WriteLine(DateTime.Now + "\t It seems this machine is physical and v2p protection " + h.displayname);
                                        }
                                        else
                                        {
                                            node.Nodes.Add(h.displayname);
                                        }

                                    }
                                }
                                if (planExists == false)
                                {
                                    if (h.machineType == "PhysicalMachine" && h.failback == "yes")
                                    {
                                        Trace.WriteLine(DateTime.Now + "\t It seems this machine is physical and v2p protection " + h.displayname);
                                    }
                                    else
                                    {
                                        planNamesTreeView.Nodes.Add(h.plan);
                                        planNamesTreeView.Nodes[i].Nodes.Add(h.displayname);
                                        i++;
                                    }
                                }
                            }
                        }
                    }
                    if (planNamesTreeView.Nodes.Count == 0)
                    {
                        GetMasterConfigFileAndReloadTreeView();
                        ReloadTreeViewForRecovery();
                        if (planNamesTreeView.Nodes.Count == 0)
                        {
                            planNamesTreeView.Nodes.Add("No plans to remove disks");
                            planNamesTreeView.CheckBoxes = false;
                            /*AllServersForm allServersForm = new AllServersForm("recovery", "");
                            allServersForm.ShowDialog();
                            recoverRadioButton.Checked = false;*/
                        }
                        else
                        {
                            _planName = "";
                            selectedVMListForPlan = new HostList();
                            masterTargetList = new HostList();
                            planNamesTreeView.CheckBoxes = true;
                            planNamesTreeView.ExpandAll();
                            if (removeDiskRadioButton.Checked == true)
                            {
                                removeDiskPanel.BringToFront();
                            }
                            else
                            {
                                VMDeatilsPanel.BringToFront();
                            }
                        }
                    }
                    else
                    {
                        _planName = "";
                        selectedVMListForPlan = new HostList();
                        masterTargetList = new HostList();
                        planNamesTreeView.CheckBoxes = true;
                        planNamesTreeView.ExpandAll();
                        // VMDeatilsPanel.BringToFront();
                        mainFormPanel.BringToFront();
                    }
                    cancelButton.Text = "Close";
                    resumeButton.Text = "Next";
                    resumeButton.Visible = true;
                    //VMDeatilsPanel.BringToFront();
                    mainFormPanel.BringToFront();
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
        }

        private void removeVolumeRadioButton_CheckedChanged(object sender, EventArgs e)
        {
            try
            {
                if (removeVolumeRadioButton.Checked == true)
                {
                    removeVolumeNoteLabel.Visible = true;
                    progressBar.Visible = true;
                    GetMasterConfigFileAndReloadTreeView();
                    ReloadTreeViewForRecovery();
                    progressBar.Visible = false;
                    if (chooseApplicationComboBox.SelectedItem.ToString() == "P2V")
                    {
                        selectedActionForPlan = "P2V";
                    }
                    else
                    {
                        selectedActionForPlan = "ESX";
                    }

                    selectedVMListForPlan = new HostList();
                    masterTargetList = new HostList();

                    planNamesTreeView.CheckBoxes = true;
                    planNamesTreeView.Nodes.Clear();
                    int i = 0;
                    foreach (Host h in sourceHostsList._hostList)
                    {
                        if (h.offlineSync == false)
                        {
                            if (h.failOver == "no" && h.role == Role.Primary && h.os == OStype.Windows)
                            {
                                // Trace.WriteLine(DateTime.Now + "\t Prinitng the vcon name " + h.vConName);
                                if (planNamesTreeView.Nodes.Count == 0)
                                {
                                    planNamesTreeView.Nodes.Add(h.plan);
                                    // planNamesTreeView.Nodes[i].Nodes.Add(h.displayname);
                                    i++;
                                }
                                bool planExists = false;
                                foreach (TreeNode node in planNamesTreeView.Nodes)
                                {
                                    //Trace.WriteLine(DateTime.Now + " \t printing the node names " + node.Name + " plan name " + h.plan);
                                    if (node.Text == h.plan)
                                    {
                                        planExists = true;
                                        node.Nodes.Add(h.displayname);

                                    }
                                }
                                if (planExists == false)
                                {
                                    planNamesTreeView.Nodes.Add(h.plan);
                                    planNamesTreeView.Nodes[i].Nodes.Add(h.displayname);
                                    i++;
                                }
                            }
                        }
                    }
                    if (planNamesTreeView.Nodes.Count == 0)
                    {

                        planNamesTreeView.Nodes.Add("No plans to Remove volume");
                        planNamesTreeView.CheckBoxes = false;
                        //GetMasterConfigFileAndReloadTreeView();
                        //ReloadTreeViewForRecovery();
                        //if (planNamesTreeView.Nodes.Count == 0)
                        //{
                        //    planNamesTreeView.Nodes.Add("No plans to remove volume");
                        //    planNamesTreeView.CheckBoxes = false;
                        //    /*AllServersForm allServersForm = new AllServersForm("recovery", "");
                        //    allServersForm.ShowDialog();
                        //    recoverRadioButton.Checked = false;*/
                        //}
                        //else
                        //{
                        //    _planName = "";
                        //    Console = new HostList();
                        //    _masterList = new HostList();
                        //    planNamesTreeView.CheckBoxes = true;
                        //    planNamesTreeView.ExpandAll();
                        //    if (removeDiskRadioButton.Checked == true)
                        //    {
                        //        removeDiskPanel.BringToFront();
                        //    }
                        //    else
                        //    {
                        //        VMDeatilsPanel.BringToFront();
                        //    }
                        //}
                    }
                    else
                    {
                        _planName = "";
                        selectedVMListForPlan = new HostList();
                        masterTargetList = new HostList();
                        planNamesTreeView.CheckBoxes = true;
                        planNamesTreeView.ExpandAll();
                        // VMDeatilsPanel.BringToFront();
                        mainFormPanel.BringToFront();
                    }
                    cancelButton.Text = "Close";
                    resumeButton.Text = "Next";
                    resumeButton.Visible = true;
                    //VMDeatilsPanel.BringToFront();
                    mainFormPanel.BringToFront();
                }
                else
                {
                    removeVolumeNoteLabel.Visible = false;
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

        }

        private void esxDiskInfoDatagridView_CurrentCellDirtyStateChanged(object sender, EventArgs e)
        {
            if (esxDiskInfoDatagridView.RowCount > 0)
            {

                esxDiskInfoDatagridView.CommitEdit(DataGridViewDataErrorContexts.Commit);
            }

        }

        private void esxDiskInfoDatagridView_CellValueChanged(object sender, DataGridViewCellEventArgs e)
        {
            if (esxDiskInfoDatagridView.RowCount > 0)
            {
               // if (esxDiskInfoDatagridView.Rows[e.RowIndex].Cells[2].Selected == true)
                {
                    //esxDiskInfoDatagridView.Rows[e.RowIndex].Cells[SELECTDISK_COLUMN].Value = true;

                    // esxDiskInfoDatagridView.RefreshEdit();
                    // this comment will be removed whenthe selection of disks is supported.

                    
                    //SelectDiskOrUnSelectDisk(e.RowIndex);

                }


            }
        }

        internal bool SelectDiskOrUnselectDisk(int index)
         {
             //if user want to unselect the disk which he does not want to protect particular disk....
             Trace.WriteLine(DateTime.Now + " \t Entered: SelectDiskOrUnSelectDisk of the Esx_AddSourcePanelHandler  ");
             try
             {
                 if ((bool)removeDisksDataGridView.Rows[index].Cells[2].FormattedValue)
                 {
                     Host tempHost = new Host();
                     tempHost.displayname = currentDisplayedPhysicalHostIp;
                     tempHost.plan = planSelected;
                     int listIndex = 0;
                     if (selectedVMListForPlan.DoesHostExist(tempHost, ref listIndex) == true)
                     {
                         Debug.WriteLine("Host exists in the primary host list ");
                         Trace.WriteLine(DateTime.Now + "\t added the disk ");
                        
                         Host h = (Host)selectedVMListForPlan._hostList[listIndex];

                         Hashtable hash = (Hashtable)h.disks.list[index];

                         if (hash["disk_type_os"] != null)
                         {
                             if (hash["disk_type_os"].ToString().ToUpper() == "DYNAMIC")
                             {
                                 MessageBox.Show("Selected disk is Dynamic disk. Removing dynamic disk is not supported", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                 removeDisksDataGridView.Rows[index].Cells[2].Value = false;
                                 removeDisksDataGridView.RefreshEdit();
                                 return false;
                             }
                         }
                         ((Host)selectedVMListForPlan._hostList[listIndex]).UnselectDiskForRemove(index);
if (h.cluster == "yes" && !h.operatingSystem.Contains("2003"))
                         {

                             Hashtable hashes = (Hashtable)h.disks.list[index];

                             if (hashes["disk_signature"] != null && hashes["cluster_disk"] != null && hashes["cluster_disk"].ToString() == "yes")
                             {
                                 foreach (Host h1 in sourceHostsList._hostList)
                                 {
                                     foreach (Hashtable sourceHash in h1.disks.list)
                                     {
                                         if (sourceHash["disk_signature"] != null && hashes["disk_signature"].ToString() == sourceHash["disk_signature"].ToString())
                                         {
                                             sourceHash["remove"] = "True";
                                         }
                                     }
                                 }
                             }
                         }

                     }
                     else
                     {
                         if (sourceHostsList.DoesHostExist(tempHost, ref listIndex) == true)
                         {
                             Debug.WriteLine("Host exists in the primary host list ");
                             Trace.WriteLine(DateTime.Now + "\t added the disk ");
                             Host h = (Host)sourceHostsList._hostList[listIndex];

                             Hashtable hash = (Hashtable)h.disks.list[index];

                             if (hash["disk_type_os"] != null)
                             {
                                 if (hash["disk_type_os"].ToString().ToUpper() == "DYNAMIC")
                                 {
                                     MessageBox.Show("Selected disk is Dynamic disk. Removing dynamic disk is not supported", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                     removeDisksDataGridView.Rows[index].Cells[2].Value = false;
                                     removeDisksDataGridView.RefreshEdit();
                                     return false;
                                 }
                             }

                             ((Host)sourceHostsList._hostList[listIndex]).UnselectDiskForRemove(index);
                         }
                     }


                 }
                 else
                 {

                     Host tempHost = new Host();
                     tempHost.displayname = currentDisplayedPhysicalHostIp;
                     tempHost.plan = planSelected;
                     int listIndex = 0;
                     if (selectedVMListForPlan.DoesHostExist(tempHost, ref listIndex) == true)
                     {
                         Debug.WriteLine("Host exists in the primary host list ");

                         Trace.WriteLine(DateTime.Now + "\t Removed the disk ");
                         ((Host)sourceHostsList._hostList[listIndex]).SelectDiskForRemove(index);
                         Host h = (Host)sourceHostsList._hostList[listIndex];
                         if (h.cluster == "yes" && !h.operatingSystem.Contains("2003"))
                         {

                             Hashtable hashes = (Hashtable)h.disks.list[index];

                             if (hashes["disk_signature"] != null && hashes["cluster_disk"] != null && hashes["cluster_disk"].ToString() == "yes")
                             {
                                 foreach (Host h1 in sourceHostsList._hostList)
                                 {
                                     foreach (Hashtable sourceHash in h1.disks.list)
                                     {
                                         if (sourceHash["disk_signature"] != null && hashes["disk_signature"].ToString() == sourceHash["disk_signature"].ToString())
                                         {
                                             sourceHash["remove"] = "false";
                                         }
                                     }
                                 }
                             }
                         }
                     }
                     else
                     {
                         if (sourceHostsList.DoesHostExist(tempHost, ref listIndex) == true)
                         {
                             Debug.WriteLine("Host exists in the primary host list ");
                             Trace.WriteLine(DateTime.Now + "\t added the disk ");
                             ((Host)sourceHostsList._hostList[listIndex]).SelectDiskForRemove(index);
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



        internal bool RemoveVolumesFromSelectedList()
         {
             try
             {
                 if (File.Exists(latestFilePath + "\\Inmage_volumepairs.txt"))
                 {
                     File.Delete(latestFilePath + "\\Inmage_volumepairs.txt");
                 }
             }
             catch (Exception ex)
             {
                 Trace.WriteLine(DateTime.Now + "\t Failed to delete the file Inmage_volumepairs.txt " + ex.Message);
             }
			 try
             {
                 if (File.Exists(latestFilePath + "\\Remove.xml"))
                 {
                     File.Delete(latestFilePath + "\\Remove.xml");
                 }
             }
             catch (Exception ex)
             {
                 Trace.WriteLine(DateTime.Now + "\t Failed to delete the file Remove.xml " + ex.Message);
             }
             foreach (Host h in selectedVMListForPlan._hostList)
             {
                 foreach (Hashtable hash in h.disks.partitionList)
                 {
                     if (hash["remove"] != null && hash["remove"].ToString().ToUpper() == "TRUE")
                     {
                         if (hash["SourceVolume"] != null && hash["SourceVolume"].ToString().ToUpper() == "C" ||hash["SourceVolume"].ToString().ToUpper() == "C:\\SRV" || hash["SourceVolume"].ToString().ToUpper() == "C:" )
                         {
                             switch (MessageBox.Show("Selected list contains volume: C or C:\\srv. VM will not come up if you remove C or C:\\srv." + Environment.NewLine + "Do you want to continue?", "Question", MessageBoxButtons.YesNo, MessageBoxIcon.Question))
                             {
                                 case DialogResult.No:
                                     return false;

                             }

                         }
                     }
                 }
             }
             string planname = null;
             FileInfo f1 = new FileInfo(latestFilePath + "\\Inmage_volumepairs.txt");
             StreamWriter sw = f1.CreateText();
             foreach (Host h in selectedVMListForPlan._hostList)
             {
                 planname = h.plan;
                 
                 sw.WriteLine(h.inmage_hostid);
                 int volumeCount = 0;
                 foreach (Hashtable hash in h.disks.partitionList)
                 {
                     if (hash["remove"] != null)
                     {
                         if (hash["remove"].ToString() == "True")
                         {
                             volumeCount++;
                         }
                     }
                 }


                 if (h.disks.partitionList.Count == volumeCount)
                 {
                     MessageBox.Show("You have selected to remove all volumes from the machine " + h.displayname + Environment.NewLine + "Use remove option to remove VM.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                     sw.Close();
                     return false;
                 }
                 if (volumeCount == 0)
                 {
                     MessageBox.Show("You have not selected single volume to remove from protection for the server " + h.displayname + ". Select volume to proceed", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                     sw.Close();
                     return false;
                 }
                 sw.WriteLine(volumeCount.ToString());
                 foreach (Hashtable hash in h.disks.partitionList)
                 {
                     if (hash["remove"] != null)
                     {
                         if (hash["remove"].ToString() == "True")
                         {
                             sw.WriteLine(hash["SourceVolume"].ToString() + "!@!@!" + hash["TargetVolume"].ToString());
                         }
                     }
                 }
                
             }
             sw.Close();

             SolutionConfig sol = new SolutionConfig();
             Esx target = new Esx();
             string cxip = WinTools.GetcxIPWithPortNumber();
             foreach (Host h in mastertargetsHostList._hostList)
             {
                 target.configurationList = h.configurationList;
                 target.dataStoreList = h.dataStoreList;
                 target.lunList = h.lunList;
                 target.networkList = h.networkNameList;

             }
             sol.WriteXmlFile(sourceHostsList, mastertargetsHostList, target, cxip, "Remove.xml", "Remove", true);


             if (_directoryNameinXmlFile == null)
             {
                 Random randomToGenerateNumber = new Random();
                 int randomNumber = randomToGenerateNumber.Next(999999);
                 _directoryNameinXmlFile = planname + "_" + "Remove_volume" + "_" + randomNumber.ToString();

             }
             if (File.Exists(latestFilePath + "\\Inmage_volumepairs.txt"))
             {
                 FileInfo destFile = new FileInfo(fxInstallPath + "\\" + _directoryNameinXmlFile + "\\Inmage_volumepairs.txt");
                 if (destFile.Directory != null && !destFile.Directory.Exists)
                 {
                     destFile.Directory.Create();
                 }
                 File.Copy(latestFilePath + "\\Inmage_volumepairs.txt", destFile.ToString(), true);
             if (File.Exists(latestFilePath + "\\Remove.xml"))
             {
                  destFile = new FileInfo(fxInstallPath + "\\" + _directoryNameinXmlFile + "\\Remove.xml");
                 if (destFile.Directory != null && !destFile.Directory.Exists)
                 {
                     destFile.Directory.Create();
                 }
                 File.Copy(latestFilePath + "\\Remove.xml", destFile.ToString(), true);
             }
                 WinTools win = new WinTools();
                 win.SetFilePermissions(fxInstallPath + "\\" + _directoryNameinXmlFile + "\\Inmage_volumepairs.txt");
                 win.SetFilePermissions(latestFilePath + "\\Inmage_volumepairs.txt");
                 if (AllServersForm.SetFolderPermissions(fxInstallPath + "\\" + _directoryNameinXmlFile) == true)
                 {
                     Trace.WriteLine(DateTime.Now + "\t Successfully set the p[ermissions for the folder ");
                 }
                 else
                 {
                     Trace.WriteLine(DateTime.Now + "\t Failed to set permissions for the folder ");
                 }
             }

             Host tempMasterHost = (Host)masterTargetList._hostList[0];
             StatusForm status = new StatusForm(selectedVMListForPlan, tempMasterHost, AppName.Removevolume, _directoryNameinXmlFile);
             progressBar.Visible = false;
             resumeButton.Visible = false;
             resumeButton.Enabled = true;
             cancelButton.Enabled = true;
             planNamesTreeView.Enabled = true;
             cancelButton.Visible = true;
             cancelButton.Text = "Close";
             offlineSyncExportRadioButton.Checked = false;
             newProtectionRadioButton.Checked = false;
             pushAgentRadioButton.Checked = false;
             manageExistingPlanRadioButton.Checked = false;
             _directoryNameinXmlFile = null;
             taskGroupBox.Enabled = false;
             planNamesTreeView.Nodes.Clear();
             progressBar.Visible = true;
             getPlansBackgroundWorker.RunWorkerAsync();
             return true;
         }

        internal bool RemoveDisk()
         {
             try
             {
                 if (File.Exists(latestFilePath + "\\Remove.xml"))
                 {
                     File.Delete(latestFilePath + "\\Remove.xml");
                 }


             }
             catch (Exception ex)
             {
                 Trace.WriteLine(DateTime.Now + "\t Failed to delete remove.xml " + ex.Message);
             }


             foreach (Host h in selectedVMListForPlan._hostList)
             {
                 int diskCount = 0;
                 foreach (Hashtable hash in h.disks.list)
                 {
                     if (hash["remove"] != null)
                     {
                         if (hash["remove"].ToString() == "True")
                         {
                             diskCount++;
                         }
                     }
                 }

                 if (diskCount == h.disks.list.Count)
                 {
                     MessageBox.Show("You have selected remove disk option for all the disks in the machine " + h.displayname + Environment.NewLine + 
                         "Use remove option to remove entire machine from protection", "Error",MessageBoxButtons.OK, MessageBoxIcon.Error);
                     return false;

                 }
                 if (diskCount == 0)
                 {
                     MessageBox.Show("You have not selected single disk to remove from protection for the server " + h.displayname + ". Select disk to proceed", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                     return false;
                 }
             }

             SolutionConfig sol = new SolutionConfig();
             Esx target = new Esx();
             string cxip = WinTools.GetcxIPWithPortNumber();
             foreach (Host h in masterTargetList._hostList)
             {
                 target.configurationList = h.configurationList;
                 target.dataStoreList = h.dataStoreList;
                 target.lunList = h.lunList;
                 target.networkList = h.networkNameList;

             }
             sol.WriteXmlFile(selectedVMListForPlan, masterTargetList, target, cxip, "Remove.xml", "Resize", true);
             Trace.WriteLine(DateTime.Now + "\t Cam ehere to modify the xml file ");
             //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
             //based on above comment we have added xmlresolver as null
             XmlDocument document1 = new XmlDocument();
             document1.XmlResolver = null;
             string xmlfile1 = latestFilePath + "\\Remove.xml";

             //StreamReader reader = new StreamReader(xmlfile1);
             //string xmlString = reader.ReadToEnd();
             XmlReaderSettings settings = new XmlReaderSettings();
             settings.ProhibitDtd = true;
             settings.XmlResolver = null;
             //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
             using (XmlReader reader1 = XmlReader.Create(xmlfile1, settings))
             {
                 document1.Load(reader1);
                 //reader1.Close();
             }
             //document1.Load(xmlfile1);
            // reader.Close();
             //XmlNodeList hostNodes = null;
             XmlNodeList rootNodes1 = null, hostNodes1 = null, diskNodes = null;
             hostNodes1 = document1.GetElementsByTagName("host");

             diskNodes = document1.GetElementsByTagName("disk");
             XmlNodeList rootNodes = null, hostNodes = null;
             rootNodes = document1.GetElementsByTagName("root");
             Host h1 = (Host)selectedVMListForPlan._hostList[0];
             string planname = null;
             planname = h1.plan;
             if (_directoryNameinXmlFile == null)
             {
                 Random randomlyGenerateNumber = new Random();
                 int randomNumber = randomlyGenerateNumber.Next(999999);
                 _directoryNameinXmlFile = planname + "_" + "Remove_Disk" + "_" + randomNumber.ToString();

             }
             foreach (XmlNode node in rootNodes)
             {
                 node.Attributes["xmlDirectoryName"].Value = _directoryNameinXmlFile;
                 //node.Attributes["batchresync"].Value = h1.batchResync.ToString();
             }
             document1.Save(xmlfile1);

             if (File.Exists(latestFilePath + "\\Remove.xml"))
             {
                 FileInfo destFile = new FileInfo(fxInstallPath + "\\" + _directoryNameinXmlFile + "\\Remove.xml");
                 if (destFile.Directory != null && !destFile.Directory.Exists)
                 {
                     destFile.Directory.Create();
                 }
                 File.Copy(latestFilePath + "\\Remove.xml", destFile.ToString(), true);
                 if (AllServersForm.SetFolderPermissions(fxInstallPath + "\\" + _directoryNameinXmlFile) == true)
                 {
                     Trace.WriteLine(DateTime.Now + "\t Successfully set the p[ermissions for the folder ");
                 }
                 else
                 {
                     Trace.WriteLine(DateTime.Now + "\t Failed to set permissions for the folder " + fxInstallPath + "\\" + _directoryNameinXmlFile);
                 }

             }
             Host tempMasterHost = (Host)masterTargetList._hostList[0];
             StatusForm status = new StatusForm(selectedVMListForPlan, tempMasterHost, AppName.Removedisk, _directoryNameinXmlFile);
             progressBar.Visible = false;
             resumeButton.Visible = false;
             resumeButton.Enabled = true;
             cancelButton.Enabled = true;
             planNamesTreeView.Enabled = true;
             cancelButton.Visible = true;
             cancelButton.Text = "Close";
             offlineSyncExportRadioButton.Checked = false;
             newProtectionRadioButton.Checked = false;
             pushAgentRadioButton.Checked = false;
             manageExistingPlanRadioButton.Checked = false;
             _directoryNameinXmlFile = null;
             taskGroupBox.Enabled = false;
             planNamesTreeView.Nodes.Clear();
             progressBar.Visible = true;
             getPlansBackgroundWorker.RunWorkerAsync();
             return true;
         }

         private void removeDisksDataGridView_CellValueChanged(object sender, DataGridViewCellEventArgs e)
         {
            
             if (removeDisksDataGridView.RowCount > 0)
             {
                 if (removeDisksDataGridView.Rows[e.RowIndex].Cells[2].Selected == true)
                 {
                     //esxDiskInfoDatagridView.Rows[e.RowIndex].Cells[SELECTDISK_COLUMN].Value = true;

                     // esxDiskInfoDatagridView.RefreshEdit();
                     // this comment will be removed whenthe selection of disks is supported.
                     if (removeVolumeRadioButton.Checked == true)
                     {
                         UnselectOrSelectVolume(e.RowIndex);
                     }
                     else if(removeDiskRadioButton.Checked == true)
                     {
                         SelectDiskOrUnselectDisk(e.RowIndex);
                     }

                 }


             }
         }


        private bool UnselectOrSelectVolume(int index)
        {

            Host h = new Host();
            int hostIndex = 0;
            h.displayname = currentDisplayedPhysicalHostIp;
            h.plan =  planSelected;
            if (selectedVMListForPlan.DoesHostExist(h, ref hostIndex) == true)
            {
                 h = (Host)selectedVMListForPlan._hostList[hostIndex];
                 string sourceVolume = removeDisksDataGridView.Rows[index].Cells[0].Value.ToString();
                 string targetVolume = removeDisksDataGridView.Rows[index].Cells[1].Value.ToString();

                 foreach (Hashtable hash in h.disks.partitionList)
                 {
                     if (hash["SourceVolume"] != null && hash["TargetVolume"] != null)
                     {
                         if (hash["SourceVolume"].ToString() == sourceVolume && hash["TargetVolume"].ToString() == targetVolume)
                         {
                             if ((bool)removeDisksDataGridView.Rows[index].Cells[2].FormattedValue)
                             {
                                 hash["remove"] = "True";


                                 foreach (Host h1 in sourceHostsList._hostList)
                                 {
                                     if (h1.cluster == "yes")
                                     {
                                         foreach (Hashtable hashes in h1.disks.partitionList)
                                         {
                                             if (hashes["SourceVolume"] != null && hashes["TargetVolume"] != null)
                                             {
                                                 if (hashes["SourceVolume"].ToString().ToUpper() == hash["SourceVolume"].ToString().ToUpper() && hashes["TargetVolume"].ToString().ToUpper() == hashes["TargetVolume"].ToString().ToUpper())
                                                 {
                                                     hashes["remove"] = "True";

                                                 }
                                             }
                                         }
                                     }
                                 }

                             }
                             else
                             {
                                 hash["remove"] = "false";

                                 foreach (Host h1 in sourceHostsList._hostList)
                                 {
                                     if (h1.cluster == "yes")
                                     {
                                         foreach (Hashtable hashes in h1.disks.partitionList)
                                         {
                                             if (hashes["SourceVolume"] != null && hashes["TargetVolume"] != null)
                                             {
                                                 if (hashes["SourceVolume"].ToString().ToUpper() == hash["SourceVolume"].ToString().ToUpper() && hashes["TargetVolume"].ToString().ToUpper() == hashes["TargetVolume"].ToString().ToUpper())
                                                 {
                                                     hashes["remove"] = "false";

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


            return true;
        }

         private void removeDisksDataGridView_CurrentCellDirtyStateChanged(object sender, EventArgs e)
         {
             if (removeDisksDataGridView.RowCount > 0)
             {

                 removeDisksDataGridView.CommitEdit(DataGridViewDataErrorContexts.Commit);
             }
         }

         private void removeDisksDataGridView_CellContentClick(object sender, DataGridViewCellEventArgs e)
         {
             if (e.RowIndex >= 0)
             {
                 if (removeDisksDataGridView.Rows[e.RowIndex].Cells[3].Selected == true)
                 {
                     DisplayDiskDetails(e.RowIndex);
                 }
                 if (removeDisksDataGridView.RowCount > 0)
                 {
                     if (removeDisksDataGridView.Rows[e.RowIndex].Cells[2].Selected == true)
                     {
                         //esxDiskInfoDatagridView.Rows[e.RowIndex].Cells[SELECTDISK_COLUMN].Value = true;

                         // esxDiskInfoDatagridView.RefreshEdit();
                         // this comment will be removed whenthe selection of disks is supported.
                         if (removeVolumeRadioButton.Checked == true)
                         {
                             UnselectOrSelectVolume(e.RowIndex);
                         }
                         else if (removeDiskRadioButton.Checked == true)
                         {
                             SelectDiskOrUnselectDisk(e.RowIndex);
                         }

                     }
                 }
             }
         }


         internal bool DisplayDiskDetails(int index)
         {
             try
             {
                 Host tempHost = new Host();
                 tempHost.displayname = currentDisplayedPhysicalHostIp;
                 int listIndex = 0;
                 if (sourceHostsList.DoesHostExist(tempHost, ref listIndex) == true)
                 {
                     tempHost = (Host)sourceHostsList._hostList[listIndex];
                     Hashtable hash = (Hashtable)tempHost.disks.list[index];

                     DiskDetails disk = new DiskDetails();
                     disk.descriptionLabel.Text = "Details of: " + removeDisksDataGridView.Rows[index].Cells[0].Value.ToString();
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
                     if (hash["disk_signature"] != null && (! hash["disk_signature"].ToString().ToUpper().Contains("SYSTEM")))
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

         private void administrativeTaksLinkLabel_Click(object sender, EventArgs e)
         {
             if (File.Exists(installPath + "\\Manual.chm"))
             {
                 Help.ShowHelp(null, installPath + "\\Manual.chm", HelpNavigator.TopicId, Administrativetasks);
             } 

         }

         private void planNamesTreeView_NodeMouseClick(object sender, TreeNodeMouseClickEventArgs e)
         {
             //if (e.Node.Nodes.Count == 0)
             //{
             //    Host h = new Host();
             //    int index = 0;
             //    if (e.Node.Checked == false)
             //    {
             //        if (e.Node.Nodes.Count != 0)
             //        {
             //            foreach (TreeNode node in e.Node.Nodes)
             //            {
             //                node.Checked = false;
             //                h.displayName = node.Text;
             //                h.plan = node.Parent.Text;
             //                if (sourceHostsList.DoesHostExistInPlanBased(h, ref index) == true)
             //                {
             //                    h = (Host)sourceHostsList._hostList[index];
             //                    if (addDiskRadioButton.Checked == true || recoverRadioButton.Checked == true || drDrillRadioButton.Checked == true || resumeProtectionRadioButton.Checked == true || removeDiskRadioButton.Checked == true || removeRadioButton.Checked == true || failBackRadioButton.Checked == true)
             //                    {
             //                        if (h.cluster == "yes")
             //                        {
             //                            if (node.Nodes.Count == 0)
             //                            {
             //                                MessageBox.Show("For cluster protections you need to select all machines for Recovery\\Resume\\DR-Drill\\Add disk.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
             //                                node.Checked = true;
             //                                break;
             //                            }
             //                        }
             //                    }
             //                    Console.RemoveHost(h);
             //                    if (Console._hostList.Count == 0)
             //                    {
             //                        try
             //                        {
             //                            e.Node.Parent.Checked = false;
             //                        }
             //                        catch (Exception ex)
             //                        {
             //                            Trace.WriteLine(DateTime.Now + "\t There is no parent node for the existing node " + e.Node.Text);
             //                        }
             //                    }
             //                    Trace.WriteLine(DateTime.Now + "\t printing the count of the list " + Console._hostList.Count.ToString() + " " + _masterList._hostList.Count.ToString());
             //                }
             //            }
             //        }
             //        if (e.Node.Nodes.Count == 0)
             //        {
             //            h.displayName = e.Node.Text;
             //            h.plan = e.Node.Parent.Text;
             //            if (sourceHostsList.DoesHostExistInPlanBased(h, ref index) == true)
             //            {
             //                h = (Host)sourceHostsList._hostList[index];
             //                if (e.Node.Parent.Checked == true && h.cluster == "yes")
             //                {
             //                    if (addDiskRadioButton.Checked == true || recoverRadioButton.Checked == true || drDrillRadioButton.Checked == true || resumeProtectionRadioButton.Checked == true || removeDiskRadioButton.Checked == true || removeRadioButton.Checked == true)
             //                    {
             //                        MessageBox.Show("For cluster protections you need to select all machines for Recovery\\Resume\\DR-Drill\\Add disk.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
             //                        e.Node.Checked = true;
             //                    }

             //                }
             //                else
             //                {
             //                    Console.RemoveHost(h);
             //                }
             //                Trace.WriteLine(DateTime.Now + "\t printing the count of the list " + Console._hostList.Count.ToString() + " " + _masterList._hostList.Count.ToString());
             //            }
             //        }

             //    }
             //}
         }


         private bool ClusterReadinessChecks()
         {


             string[] clusterUUIDS;
             string mtName = null;
             foreach (Host h in sourceHostsList._hostList)
             {
                 if (h.cluster == "yes" && h.clusterInmageUUIds != null)
                 {
                     mtName = h.masterTargetHostName;
                     clusterUUIDS = h.clusterInmageUUIds.Split(',');
                     bool hostExists = false;
                     if (clusterUUIDS.Length != 0)
                     {
                         foreach (string s in clusterUUIDS)
                         {
                             hostExists = false;
                             foreach (Host h1 in sourceHostsList._hostList)
                             {
                                 if (s == h1.inmage_hostid)
                                 {
                                     hostExists = true;
                                     break;
                                 }
                             }
                             if (hostExists == false)
                             {
                                 bool hostProtectedAlready = false;
                                 foreach (Host sourceHost in sourceHostsList._hostList)
                                 {
                                     if (s == sourceHost.inmage_hostid && sourceHost.masterTargetHostName == mtName)
                                     {
                                         hostProtectedAlready = true;
                                         break;
                                     }

                                 }
                                 if (hostProtectedAlready == true)
                                 {
                                     MessageBox.Show("All protected clutser machines under cluser node has to be selected for the following operations like Add disk\\Recover\\DR-Drill\\Fail back\\Remove disk\\Remove volume", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                     return false;
                                 }
                              
                             }
                         }

                         
                     }


                 }
             }
             return true;
         }


        //This Method event will get the resync required hosts and compares the machines in MasterConfingFile and adds to Treeview
         private void resyncRadioButton_CheckedChanged(object sender, EventArgs e)
         {
             if (resyncRadioButton.Checked == true)
             {
                 planNamesTreeView.Nodes.Clear();
                 resumeButton.Text = "Start";

                 Host h1 = new Host();
                 Cxapicalls cxAPi = new Cxapicalls();
                 if (cxAPi.Post(h1, "GetResyncRequiredHosts") == true)
                 {
                     Trace.WriteLine(DateTime.Now + "\t Successfully posted to get Resync hosts info");

                     ArrayList resyncHostList = new ArrayList();
                     try
                     {
                         if(Cxapicalls.GetArrayList != null)
                         resyncHostList = Cxapicalls.GetArrayList;
                     }
                     catch (Exception ex)
                     {
                         Trace.WriteLine(DateTime.Now + "\t Failed to get list " + ex.Message);
                     }
                     if (resyncHostList.Count != 0)
                     {
                         resumeButton.Visible = true;
                         foreach (Host h in sourceHostsList._hostList)
                         {
                             foreach (Hashtable hash in resyncHostList)
                             {
                                 if (hash["SourceHostId"] != null)
                                 {
                                     if (h.inmage_hostid == hash["SourceHostId"].ToString())
                                     {
                                         h.restartResync = true;
                                     }
                                 }
                             }
                         }


                         int i = 0;
                         foreach (Host h in sourceHostsList._hostList)
                         {

                             if (h.restartResync == true && h.role == Role.Primary)
                             {
                                 // Trace.WriteLine(DateTime.Now + "\t Prinitng the vcon name " + h.vConName);
                                 if (planNamesTreeView.Nodes.Count == 0)
                                 {
                                     planNamesTreeView.Nodes.Add(h.plan);
                                     // planNamesTreeView.Nodes[i].Nodes.Add(h.displayname);
                                     i++;
                                 }
                                 bool planExists = false;
                                 foreach (TreeNode node in planNamesTreeView.Nodes)
                                 {
                                     //Trace.WriteLine(DateTime.Now + " \t printing the node names " + node.Name + " plan name " + h.plan);
                                     if (node.Text == h.plan)
                                     {
                                         planExists = true;
                                         node.Nodes.Add(h.displayname);
                                         

                                     }
                                 }
                                 if (planExists == false)
                                 {
                                     planNamesTreeView.Nodes.Add(h.plan);
                                     planNamesTreeView.Nodes[i].Nodes.Add(h.displayname);
                                     i++;
                                 }
                             }
                         }
                         planNamesTreeView.CheckBoxes = true;
                         planNamesTreeView.ExpandAll();
                     }
                     else
                     {
                         planNamesTreeView.Nodes.Add("No Machines to Re-Start Resync");
                     }
                 }
             }
             else
             {
                 resumeButton.Text = "Next";
                
             }
         }
        //This method will calls Restartresync API to start replciations for the selected machines.
         private bool StartResync()
         {
             try
             {
                 Cxapicalls cxAPi = new Cxapicalls();
                 Host mtHost = (Host)masterTargetList._hostList[0];
                 tickerDelegate = new TickerDelegate(SetLeftTicker);

                 foreach (Host h in selectedVMListForPlan._hostList)
                 {
                     messages = "Restarting the resync operation for server: " + h.displayname;
                     logTextBox.Invoke(tickerDelegate, new object[] { messages });
                     if (cxAPi.PostToRestartReplications(h, mtHost.inmage_hostid) == true)
                     {
                         messages = " : Done" + Environment.NewLine;
                         logTextBox.Invoke(tickerDelegate, new object[] { messages });
                         Trace.WriteLine(DateTime.Now + "\t Successfully completed resync for machine " + h.displayname);
                     }
                     else
                     {
                         messages = " : Failed" + Environment.NewLine;
                         logTextBox.Invoke(tickerDelegate, new object[] { messages });
                         Trace.WriteLine(DateTime.Now + "\t Failed to restart resyn paris for the machine " + h.displayname);
                     }
                 }
             }
             catch (Exception ex)
             {
                 System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                 Trace.WriteLine("ERROR*******************************************");
                 Trace.WriteLine(DateTime.Now + "Caught exception at Method: StartResync " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                 Trace.WriteLine("Exception  =  " + ex.Message);
                 Trace.WriteLine("ERROR___________________________________________");

                 return false;
             }
             return true;
         }

         private void resyncBackgroundWorker_DoWork(object sender, System.ComponentModel.DoWorkEventArgs e)
         {
             StartResync();
         }

         private void resyncBackgroundWorker_RunWorkerCompleted(object sender, System.ComponentModel.RunWorkerCompletedEventArgs e)
         {
             progressBar.Visible = false;
             resumeButton.Visible = false;
             cancelButton.Text = "Done";
         }

    }
}
