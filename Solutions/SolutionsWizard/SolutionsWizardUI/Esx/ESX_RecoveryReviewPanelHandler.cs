using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;
using System.Diagnostics;
using Com.Inmage;
using Com.Inmage.Tools;
using Com.Inmage.Esxcalls;
using System.Collections;
using System.IO;
using System.Xml;

using System.Threading;
namespace Com.Inmage.Wizard
{
    class ReviewPanelHandler : PanelHandler
    {
        internal static int RECOVERY_ORDER_COLUMN = 4;
        internal static int POWER_ON_VM = 5;
        internal static string RECOVERY_ERROR_LOG_FILE = "\\logs\\vContinuum_ESX.log";
        string recoveryDbPath = "";
        internal bool recoverCompleted = false;
        internal string _directoryNameinXmlFile = null;
        internal static int HOST_NAME_COLUMN = 0;
        internal static int IP_COLUMN = 1;
       // public static int SUB_NET_MASK_COLUMN = 2;
       // public static int GATE_WAY_COLUMN = 3;
       // public static int DNS_COLUMN = 4;
        internal static int TAG_COLUMN = 2;
        internal static int TAG_TYPE_COLUMN = 3;
        internal string _latestFilePah = null;
        internal string _planFolderPath = null;
        internal string _installPath = null;
        internal bool _wmirollback = false;
        internal bool _scriptsCall = false;
        internal bool _dummyDiskCreation = false;
        private delegate void TickerDelegate(string s);
        internal static bool _fileMadeandCopiedSuccessfully = false;
        internal bool _createdVm = false;
        internal static string LINX_SCRIPT = "linux_script.sh";
        internal bool _readinessForDrDrill = false;
        internal bool _setFxjobs = false;
        TickerDelegate tickerDelegate;
        TextBox _textBox = new TextBox();
        internal bool _wmiMessageFormCancel = false;
        internal bool _reRunCredentials = false;
        internal bool _wmiWorkedfine = false;
        internal string _message = null;
        internal int _readinesschecksReturnCode = 1;
        /*
         * In Initialize we will fill the table with the datastructure which we ahve thre info
         * about the selected machine and we will fil the recovery order with 1 for all the machines.
         * User can change the recovery order.
         * we will fill the paths like latestpath and planname path with the path we knows for the regedit.
         */
        internal override bool Initialize(AllServersForm recoveryForm)
        {
            _readinesschecksReturnCode = 1;
            Trace.WriteLine("Entered: Initialize of ESX_recoveryReviewPanelHandler");
           /* for (int i = 0; i < recoveryForm._recoverChecksPassedList._hostList.Count; i++)
            {

                recoveryForm.recoverOrder.Items.Add(i+1);
               
            }*/
            try
            {
                recoveryForm.helpContentLabel.Text = HelpForcx.RecoveryScreen1;
                _fileMadeandCopiedSuccessfully = false;
                if (recoveryForm.appNameSelected == AppName.Drdrill)
                {                    
                    recoveryForm.recoveryReviewDataGridView.Enabled = true;
                    recoveryForm.drDrillReadinessCheckButton.Enabled = true;
                    recoveryForm.drDrillReadinessCheckButton.Visible = true;
                    recoveryForm.recoveryPlanNameLabel.Visible = false;
                    recoveryForm.recoveryPlanNameTextBox.Visible = false;
                    recoveryForm.nextButton.Enabled = false;
                    recoveryForm.selectedMachineListLabel.Text = "DR Drill will be performed on following VM(s)";
                    recoveryForm.recvoeryParallelLabel.Text = "DR Drill";
                    foreach (Host h in recoveryForm.recoveryChecksPassedList._hostList)
                    {
                        h.target_uuid = null;
                        h.vsan_folder = null;
                        foreach (Hashtable hash in h.disks.list)
                        {
                            hash["target_disk_uuid"] = null;
                            if (hash["scsi_id_onmastertarget"] != null)
                            {
                                hash["scsi_id_onmastertarget"] = null;
                            }
                            if (h.machineType == "PhysicalMachine")
                            {
                                if (hash["selected_for_protection"] != null && hash["selected_for_protection"].ToString() == "no")
                                {
                                    hash["Selected"] = "No";
                                    hash["Protected"] = "no";
                                }
                                if (hash["disk_signature"] != null && hash["disk_signature"].ToString() == "0")
                                {
                                    hash["Selected"] = "No";
                                    hash["Protected"] = "no";
                                }
                            }
                            else
                            {
                                if (hash["selected_for_protection"] != null && hash["selected_for_protection"].ToString() == "no")
                                {
                                    hash["selected_for_protection"] = null;
                                }
                            }
                        }
                    }
                    recoveryForm.recoveryGroupBox.Visible = false;
                    if (recoveryForm.hostBasedRadioButton.Checked == true)
                    {
                        recoveryForm.provisionGroupBox.Visible = true;
                        recoveryForm.provisionGroupBox.BringToFront();
                    }
                    else
                    {
                        recoveryForm.provisionGroupBox.Visible = false;
                    }

                }
                else
                {
                    // In case where user selected wmi radio button and clicked on previous button we are displyaing planname text box which should not be the case. to avoid that we are verifying this check
                    //For WMI recovery we are not asking planname
                    if (recoveryForm.fxJobRadioButton.Checked == true)
                    {
                        recoveryForm.recoveryPlanNameLabel.Visible = true;
                        recoveryForm.recoveryPlanNameTextBox.Visible = true;
                    }
                    recoveryForm.drDrillReadinessCheckButton.Visible = false;
                    recoveryForm.recoveryGroupBox.Visible = true;
                }

                if (recoveryForm.recoveryChecksPassedList._hostList.Count < 5)
                {
                    recoveryForm.parallelVolumeTextBox.Text = recoveryForm.recoveryChecksPassedList._hostList.Count.ToString();
                }
                _latestFilePah = WinTools.FxAgentPath() + "\\vContinuum\\Latest";
                _planFolderPath = WinTools.FxAgentPath() + "\\Failover\\Data";
                recoveryForm.recoveryReviewDataGridView.Rows.Clear();
                recoveryDbPath = WinTools.FxAgentPath()+ "\\vContinuum";
                _installPath = WinTools.FxAgentPath() + "\\vContinuum";
                if (recoveryForm.appNameSelected == AppName.Recover)
                {
                    recoveryForm.nextButton.Text = "Recover";

                    recoveryForm.recoveryThroughGroupBox.Visible = true;
                }
                else
                {
                    recoveryForm.recoveryThroughGroupBox.Visible = false;
                    recoveryForm.nextButton.Text = "Start";
                }
                recoveryForm.previousButton.Visible = true;
                _textBox = recoveryForm.recoveryLogTextBox;
                int rowIndex = 0;

                // Now in 6.0 we had changed the entire flow for the n/w changes so we are not going to display 
                // the n/w info in this screen.
               // recoveryForm.recoveryReviewDataGridView.Columns[SUB_NET_MASK_COLUMN].Visible = false;
               // recoveryForm.recoveryReviewDataGridView.Columns[GATE_WAY_COLUMN].Visible = false;
               // recoveryForm.recoveryReviewDataGridView.Columns[DNS_COLUMN].Visible = false;
                int hostCount = recoveryForm.recoveryChecksPassedList._hostList.Count;
                //Creating folder for the parallel recovery.......
                // Filling the datagridview with the info we have in our data structures.

                bool selectAllForPowerOn = false;

                foreach (Host h in recoveryForm.recoveryChecksPassedList._hostList)
                {
                    h.recoveryPlanId = null;
                    if (h.powerOnVM == true)
                    {
                        selectAllForPowerOn = true;
                    }
                    else
                    {
                        selectAllForPowerOn = false;
                    }
                }
                if (selectAllForPowerOn == true)
                {
                    recoveryForm.powerOnALLVMSCheckBox.Checked = true;
                }
                else
                {
                    recoveryForm.powerOnALLVMSCheckBox.Checked = false;
                }
                recoveryForm.recoverOrder.Items.Clear();
                foreach (Host h in recoveryForm.recoveryChecksPassedList._hostList)
                {
                    recoveryForm.recoverOrder.Items.Add((rowIndex + 1).ToString());
                    recoveryForm.recoveryReviewDataGridView.Rows.Add(1);
                    recoveryForm.recoveryReviewDataGridView.Rows[rowIndex].Cells[HOST_NAME_COLUMN].Value = h.hostname;
                    if (h.newIP != null)
                    {
                        recoveryForm.recoveryReviewDataGridView.Rows[rowIndex].Cells[IP_COLUMN].Value = h.newIP;
                    }
                    else
                    {
                        recoveryForm.recoveryReviewDataGridView.Rows[rowIndex].Cells[IP_COLUMN].Value = h.ip;
                    }
                    
                    recoveryForm.recoveryReviewDataGridView.Rows[rowIndex].Cells[TAG_COLUMN].Value = h.tag;
                    recoveryForm.recoveryReviewDataGridView.Rows[rowIndex].Cells[TAG_TYPE_COLUMN].Value = h.tagType;
                    recoveryForm.recoveryReviewDataGridView.Rows[rowIndex].Cells[RECOVERY_ORDER_COLUMN].Value = recoveryForm.recoverOrder.Items[0];
                    if (h.powerOnVM == true)
                    {
                        recoveryForm.recoveryReviewDataGridView.Rows[rowIndex].Cells[POWER_ON_VM].Value = true;
                    }
                    else
                    {
                        recoveryForm.recoveryReviewDataGridView.Rows[rowIndex].Cells[POWER_ON_VM].Value = false;
                    }
                    rowIndex++;
                }

               

                if (recoveryForm.osTypeSelected == OStype.Linux)
                {
                    
                    recoveryForm.recoveryThroughGroupBox.Visible = false;
                }
                recoveryForm.recoveryPlanNameTextBox.Enabled = true;
                if (AllServersForm.v2pRecoverySelected == true)
                {
                    recoveryForm.recoveryReviewDataGridView.Columns[RECOVERY_ORDER_COLUMN].Visible = false;
                    recoveryForm.parallelVolumeTextBox.Text = "1";
                    recoveryForm.parallelVolumeTextBox.Enabled = false;
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
            Trace.WriteLine("Exiting: Initialize of ESX_recoveryReviewPanelHandler");    
            return true;
        }


        private bool CheckPowerSelectedOption(AllServersForm allServersFrom)
        {
            try
            {
                for (int i = 0; i < allServersFrom.recoveryReviewDataGridView.RowCount; i++)
                {
                    foreach (Host h in allServersFrom.recoveryChecksPassedList._hostList)
                    {
                        if (h.hostname == allServersFrom.recoveryReviewDataGridView.Rows[i].Cells[HOST_NAME_COLUMN].Value.ToString())
                        {
                            if ((bool)allServersFrom.recoveryReviewDataGridView.Rows[i].Cells[POWER_ON_VM].FormattedValue == true)
                            {
                                h.powerOnVM = true;
                            }
                            else
                            {
                                h.powerOnVM = false;
                            }
                            break;
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: CheckPowerSelectedOption " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            return true;
        }


        /*
         * First we will check fo the recovery order given by the user is right or worng. it sholud be in order.
         * We should not accept negative values and alphabets.
         * Numbers should me in order. They can't miss the sequence. 
         * After that we will create Snapshot.xml/Recovery.xml and create a folder name along with random number and copied the files 
         * to that folder and then we will call the recovery oprerations.
         */




        internal override bool ProcessPanel(AllServersForm recoveryForm)
        {
            try
            {

                if (_fileMadeandCopiedSuccessfully == false)
                {
                    try
                    {
                        if (recoveryForm.parallelVolumeTextBox.Text.Length == 0)
                        {
                            MessageBox.Show("Please enter value for parallel VM(s) to recover", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            return false;
                        }
                        if (int.Parse(recoveryForm.parallelVolumeTextBox.Text.ToString()) > recoveryForm.recoveryChecksPassedList._hostList.Count)
                        {
                            MessageBox.Show("Parallel recovery option should be less than or equal to VM(s) selected for recovery", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            return false;
                        }
                        if (int.Parse(recoveryForm.parallelVolumeTextBox.Text.ToString()) <= 0)
                        {
                            MessageBox.Show("Parallel recovery option should be greater than or equal to 1.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            return false;
                        }
                    }
                    catch (Exception ex)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Failed to validate parallel recovery option " + ex.Message);
                    }

                    
                    if (recoveryForm.appNameSelected == AppName.Recover && recoveryForm.wmiBasedRadioButton.Checked == true)
                    {
                        Random _r1 = new Random();
                        int n1 = _r1.Next(9999);

                        recoveryForm.planNameSelected = "Recovery_" + n1.ToString();
                    }
                    else
                    {
                        if (recoveryForm.recoveryPlanNameTextBox.Text.Trim().Length == 0)
                        {
                            recoveryForm.errorProvider.SetError(recoveryForm.planNameText, "Please enter plan name for the recovery.");
                            MessageBox.Show("Please enter plan name for the recovery.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            return false;
                        }
                        else
                        {
                            if (recoveryForm.osTypeSelected == OStype.Linux)
                            {
                                if (recoveryForm.recoveryPlanNameTextBox.Text.Contains(" "))
                                {
                                    MessageBox.Show("Plan name with spaces does not support spaces", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                    return false;
                                }
                            }
                            if (recoveryForm.recoveryPlanNameTextBox.Text.Contains("&") || recoveryForm.recoveryPlanNameTextBox.Text.Contains("\"") || recoveryForm.recoveryPlanNameTextBox.Text.Contains("\'") || recoveryForm.recoveryPlanNameTextBox.Text.Contains(">") || recoveryForm.recoveryPlanNameTextBox.Text.Contains(":") || recoveryForm.recoveryPlanNameTextBox.Text.Contains("\\") || recoveryForm.recoveryPlanNameTextBox.Text.Contains("/") || recoveryForm.recoveryPlanNameTextBox.Text.Contains("|") || recoveryForm.recoveryPlanNameTextBox.Text.Contains("*") || recoveryForm.recoveryPlanNameTextBox.Text.Contains("="))
                            {
                                MessageBox.Show("Plan name does not accept character * = : ? < > / \\ | & \' \"", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                return false;
                            }
                            else
                            {
                                recoveryForm.errorProvider.Clear();
                                WinPreReqs winPreReqs = new WinPreReqs("", "", "", "");
                                ArrayList _planNameList = new ArrayList();
                                winPreReqs.GetPlanNames( _planNameList, WinTools.GetcxIPWithPortNumber());
                                _planNameList = WinPreReqs.GetPlanlist;
                                foreach (string plan in _planNameList)
                                {
                                    if (plan == recoveryForm.recoveryPlanNameTextBox.Text.Trim())
                                    {
                                        MessageBox.Show("This plan is already exists on CX", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                        return false;
                                    }
                                }

                                if (recoveryForm.recoveryPlanNameTextBox.Text.Length > 40)
                                {
                                    MessageBox.Show("Plan name should not exceed 40 characters. Enter less than 40 characters", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                    return false;
                                }

                                recoveryForm.planNameSelected = recoveryForm.recoveryPlanNameTextBox.Text.Trim();
                            }
                        }
                    }

                    CheckPowerSelectedOption(recoveryForm);
                    if (recoveryForm.appNameSelected == AppName.Recover)
                    {                       

                        string physicalMachinesList = null;
                        foreach (Host h in recoveryForm.recoveryChecksPassedList._hostList)
                        {
                            if (h.powerOnVM == false && h.machineType == "PhysicalMachine")
                            {
                                physicalMachinesList = h.displayname + Environment.NewLine;
                            }
                        }

                        if (physicalMachinesList != null)
                        {
                            string message = "Note: You have selected not to Power on following Virtual Machines " + Environment.NewLine
                                            + physicalMachinesList + Environment.NewLine
                                            + "Vmware tools will not be installed on those VMs. As these are original physical server," + Environment.NewLine
                                            + "you need to install vmware tools manually on secondary esx server";
                            MessageBox.Show(message, "Information", MessageBoxButtons.OK, MessageBoxIcon.Information);
                        }

                    }

                   // Need to call RecoveryOrder
                    if (recoveryForm.appNameSelected == AppName.Recover)
                    {
                        if (CheckRecoveryOrder(recoveryForm) == false)
                        {
                            Trace.WriteLine(DateTime.Now + "\t Found an error in checkrecoveryorder ");
                            return false;
                        }
                        if (File.Exists(_latestFilePah + "\\Recovery.xml"))
                        {
                            File.Delete(_latestFilePah + "\\Recovery.xml");
                        }
                    }
                    Trace.WriteLine("Entered: ProcessPanel of ESX_recoveryReviewPanelHandler");
                   
                    
                    
                    // Creating the dir for the support of the parallel recovery 
                    // we will create one dir with mt hostname and with random number
                    // And we will copy rcovery.xml to that folder.
                    Random _r = new Random();
                    int n = _r.Next(99999);

                    if (recoveryForm.appNameSelected == AppName.Recover)
                    {
                        _directoryNameinXmlFile = recoveryForm.planNameSelected +  "_Recovery_" + n.ToString();
                    }
                    else if (recoveryForm.appNameSelected == AppName.Drdrill)
                    {
                        _directoryNameinXmlFile = recoveryForm.planNameSelected + "_DR_Drill_" + n.ToString();
                    }
                    else
                    {

                        _directoryNameinXmlFile = recoveryForm.planNameSelected + "_" + n.ToString();
                    }

                    if (Directory.Exists(_planFolderPath + "\\" + _directoryNameinXmlFile))
                    {
                        n = _r.Next(99999);
                        if (recoveryForm.appNameSelected == AppName.Recover)
                        {
                            _directoryNameinXmlFile = recoveryForm.planNameSelected + "_Recovery_" + n.ToString();
                        }
                        else if (recoveryForm.appNameSelected == AppName.Drdrill)
                        {
                            _directoryNameinXmlFile = recoveryForm.planNameSelected + "_DR_Drill_" + n.ToString();
                        }
                        else
                        {

                            _directoryNameinXmlFile = recoveryForm.planNameSelected + "_" + n.ToString();
                        }
                    }
                    Trace.WriteLine(DateTime.Now + " \t printing foldername " + _directoryNameinXmlFile);

                    FileInfo destFile = new FileInfo(_planFolderPath + "\\" + _directoryNameinXmlFile + "\\Recovery.xml");
                    WinTools win = new WinTools();
                  
                    if (destFile.Directory != null && !destFile.Directory.Exists)
                    {
                        destFile.Directory.Create();
                    }

                    foreach (Host h in recoveryForm.recoveryChecksPassedList._hostList)
                    {
                        h.directoryName = _directoryNameinXmlFile;
                    }
                    if (AllServersForm.SetFolderPermissions(_planFolderPath + "\\" + _directoryNameinXmlFile) == true)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Successfully set the p[ermissions for the folder ");
                    }
                    else
                    {
                        Trace.WriteLine(DateTime.Now + "\t Failed to set permissions for the folder ");
                    }
                    HostList masterList = new HostList();
                    masterList.AddOrReplaceHost(recoveryForm.masterHostAdded);
                    Esx esx = new Esx();
                    esx.configurationList = recoveryForm.masterHostAdded.configurationList;
                    esx.dataStoreList = recoveryForm.masterHostAdded.dataStoreList;
                    esx.lunList = recoveryForm.masterHostAdded.lunList;
                    esx.networkList = recoveryForm.masterHostAdded.networkNameList;
                    foreach (Host h in recoveryForm.recoveryChecksPassedList._hostList)
                    {
                        if (h.new_displayname == null)
                        {
                            h.new_displayname = h.displayname;
                        }

                        if(h.recoveryOperation == 4)
                        {
                            DateTime dt = DateTime.Now;
                            dt = DateTime.Parse(h.selectedTimeByUser);
                            DateTime gmt = DateTime.Parse(dt.ToString());

                            h.selectedTimeByUser = gmt.ToString("yyyy/MM/dd HH:mm:ss");
                        }
                    }
                    SolutionConfig solution = new SolutionConfig();
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
                        {cxHost =   WinPreReqs.GetHost;

                        }
                    }
                    if (recoveryForm.appNameSelected == AppName.Recover)
                    {
                        try
                        {
                            if (File.Exists(WinTools.FxAgentPath() + "\\vContinuum\\InMage_Recovered_Vms.rollback"))
                            {
                                File.Delete(WinTools.FxAgentPath() + "\\vContinuum\\InMage_Recovered_Vms.rollback");
                            }
                        }
                        catch (Exception ex)
                        {
                            Trace.WriteLine(DateTime.Now + "\t Failed to deleted snapshot file " + ex.Message);
                        }
                        solution.WriteXmlFile(recoveryForm.recoveryChecksPassedList, masterList, esx, WinTools.GetcxIP(), "Recovery.xml", recoveryForm.planNameSelected, true);
                        //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
                        //based on above comment we have added xmlresolver as null
                        XmlDocument document = new XmlDocument();
                        document.XmlResolver = null;
                        string xmlfile = _latestFilePah + "\\Recovery.xml";

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
                        XmlNodeList rootNodes = null, hostNodes = null;
                        rootNodes = document.GetElementsByTagName("root");
                        Host h1 = (Host)recoveryForm.recoveryChecksPassedList._hostList[0];
                        foreach (XmlNode node in rootNodes)
                        {
                            node.Attributes["xmlDirectoryName"].Value = _directoryNameinXmlFile;
                            if (node.Attributes["inmage_cx_uuid"] != null)
                            {
                                node.Attributes["inmage_cx_uuid"].Value = cxHost.inmage_hostid;
                                Trace.WriteLine(DateTime.Now + "\t Updated the cx uuid " + cxHost.inmage_hostid);
                            }
                            try
                            {
                                if (node.Attributes["ParallelCdpcli"] != null)
                                {
                                    node.Attributes["ParallelCdpcli"].Value = recoveryForm.parallelVolumeTextBox.Text.Trim().ToString();
                                    Trace.WriteLine(DateTime.Now + "\t parallel volumes selected by user " + recoveryForm.parallelVolumeTextBox.Text.Trim().ToString());
                                }
                            }
                            catch (Exception ex)
                            {
                                Trace.WriteLine(DateTime.Now + "\t Failed to add parallel cdpcli value " + ex.Message);
                            }

                            if (AllServersForm.v2pRecoverySelected == true)
                            {
                                if (node.Attributes["V2P"] != null)
                                {
                                    node.Attributes["V2P"].Value = "True";
                                }

                            }
                           
                        }
                        document.Save(xmlfile);
                        if (File.Exists(_latestFilePah + "\\Recovery.xml"))
                        {
                            File.Copy(_latestFilePah + "\\Recovery.xml", _planFolderPath + "\\" + _directoryNameinXmlFile + "\\Recovery.xml", true);
                            Trace.WriteLine(DateTime.Now + "\t Successfully copied recovery.xml to the plan name folder " + _planFolderPath + "\\" + _directoryNameinXmlFile);

                            if (!File.Exists(_planFolderPath + "\\" + _directoryNameinXmlFile + "\\Recovery.xml"))
                            {
                                Trace.WriteLine(DateTime.Now + "\t Recovery.xml is not found in the directory " + _planFolderPath + "\\" + _directoryNameinXmlFile + "\\Recovery.xml");
                                Trace.WriteLine(DateTime.Now + "\t hence copying it again to the same directory");
                                File.Copy(_latestFilePah + "\\Recovery.xml", _planFolderPath + "\\" + _directoryNameinXmlFile + "\\Recovery.xml", true);
                                Trace.WriteLine(DateTime.Now + "\t Successfully copied recovery.xml to the plan name folder " + _planFolderPath + "\\" + _directoryNameinXmlFile);
                            }
                            if (!File.Exists(_planFolderPath + "\\" + _directoryNameinXmlFile + "\\Recovery.xml"))
                            {
                                Trace.WriteLine(DateTime.Now + "\t Recovery.xml doesnot exists in the folder " + _planFolderPath + "\\" + _directoryNameinXmlFile);
                                Trace.WriteLine(DateTime.Now + "\t Cannot proceed with the recovey.");
                                return false;
                            }

                            win.SetFilePermissions(_planFolderPath + "\\" + _directoryNameinXmlFile + "\\Recovery.xml");
                        }
                    }
                    else
                    {
                        try
                        {
                            if (File.Exists(WinTools.FxAgentPath() + "\\vContinuum\\InMage_Recovered_Vms.snapshot"))
                            {
                                File.Delete(WinTools.FxAgentPath() + "\\vContinuum\\InMage_Recovered_Vms.snapshot");
                            }
                        }
                        catch (Exception ex)
                        {
                            Trace.WriteLine(DateTime.Now + "\t Failed to deleted snapshot file " + ex.Message);
                        }
                        //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
                        //based on above comment we have added xmlresolver as null
                        XmlDocument document = new XmlDocument();
                        document.XmlResolver = null;
                        string xmlfile = _latestFilePah + "\\Snapshot.xml";
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
                        XmlNodeList rootNodes = null, hostNodes = null, diskNodes = null;
                        rootNodes = document.GetElementsByTagName("root");
                        diskNodes = document.GetElementsByTagName("disk");
                        Host h1 = (Host)recoveryForm.recoveryChecksPassedList._hostList[0];
                        foreach (XmlNode node in rootNodes)
                        {
                            node.Attributes["xmlDirectoryName"].Value = _directoryNameinXmlFile;
                            if (node.Attributes["inmage_cx_uuid"] != null)
                            {
                                node.Attributes["inmage_cx_uuid"].Value = cxHost.inmage_hostid;
                                Trace.WriteLine(DateTime.Now + "\t Updated the cx uuid " + cxHost.inmage_hostid);
                            }
                            if (node.Attributes["plan"] != null)
                            {
                                node.Attributes["plan"].Value = recoveryForm.planNameSelected;
                            }
                            if (recoveryForm.arrayBasedDrDrillRadioButton.Checked == true)
                            {
                                if (node.Attributes["ArrayBasedDrDrill"] != null)
                                {
                                    node.Attributes["ArrayBasedDrDrill"].Value = "True";
                                }
                            }
                            try
                            {
                                if (node.Attributes["ParallelCdpcli"] != null)
                                {
                                    node.Attributes["ParallelCdpcli"].Value = recoveryForm.parallelVolumeTextBox.Text.Trim().ToString();
                                    Trace.WriteLine(DateTime.Now + "\t parallel volumes selected by user " + recoveryForm.parallelVolumeTextBox.Text.Trim().ToString());
                                }
                            }
                            catch (Exception ex)
                            {
                                Trace.WriteLine(DateTime.Now + "\t Failed to add parallel cdpcli value " + ex.Message);
                            }
                        }
                        foreach (XmlNode node in diskNodes)
                        {
                            if (node.Attributes["thin_disk"] != null)
                            {
                                if (recoveryForm.thinProvisionRadiobutton.Checked == true)
                                {
                                    node.Attributes["thin_disk"].Value = "True";
                                }
                                else
                                {
                                    node.Attributes["thin_disk"].Value = "False";
                                }
                            }
                        }
                        document.Save(xmlfile);
                        if (File.Exists(_latestFilePah + "\\Snapshot.xml"))
                        {

                            File.Copy(_latestFilePah + "\\Snapshot.xml", _planFolderPath + "\\" + _directoryNameinXmlFile + "\\Snapshot.xml", true);
                            Trace.WriteLine(DateTime.Now + "\t Successfully copied Snapshot.xml to the plan name folder " + _planFolderPath + "\\" + _directoryNameinXmlFile);
                            if (!File.Exists(_planFolderPath + "\\" + _directoryNameinXmlFile + "\\Snapshot.xml"))
                            {
                                Trace.WriteLine(DateTime.Now + "\t Snapshot.xml is not found in the directory " + _planFolderPath + "\\" + _directoryNameinXmlFile + "\\Snapshot.xml");
                                Trace.WriteLine(DateTime.Now + "\t hence copying it again to the same directory");
                                File.Copy(_latestFilePah + "\\Snapshot.xml", _planFolderPath + "\\" + _directoryNameinXmlFile + "\\Snapshot.xml", true);
                                Trace.WriteLine(DateTime.Now + "\t Successfully copied Snapshot.xml to the plan name folder " + _planFolderPath + "\\" + _directoryNameinXmlFile);
                            }
                            if (!File.Exists(_planFolderPath + "\\" + _directoryNameinXmlFile + "\\Snapshot.xml"))
                            {
                                Trace.WriteLine(DateTime.Now + "\t Snapshot.xml doesnot exists in the folder " + _planFolderPath + "\\" + _directoryNameinXmlFile);
                                Trace.WriteLine(DateTime.Now + "\t Cannot proceed with the recovey.");
                                return false;
                            }
                            win.SetFilePermissions(_planFolderPath + "\\" + _directoryNameinXmlFile + "\\Snapshot.xml");
                        }
                    }
                    recoveryForm.recoveryPlanNameTextBox.Enabled = false;
                    _fileMadeandCopiedSuccessfully = true;
                    if (recoveryForm.appNameSelected == AppName.Recover)
                    {
                        if (recoveryForm.wmiBasedRecvoerySelected == false)
                        {
                           // recoveryForm.recoveryLogTextBox.AppendText("Setting jobs for the recovery. This may take few minutes...." + Environment.NewLine);
                        }
                    }

                }
                //need to remove while checkin the code

                recoveryForm.progressBar.Visible = true;
                recoveryForm.previousButton.Enabled = false;
                recoveryForm.nextButton.Enabled = false;

                recoveryForm.recoveryReviewDataGridView.Enabled = false;

                if (recoveryForm.esxIPSelected != null)
                {
                    recoveryForm.masterHostAdded.esxIp = recoveryForm.esxIPSelected;
                }
                if (recoveryForm.tgtEsxUserName != null)
                {
                    recoveryForm.masterHostAdded.esxUserName = recoveryForm.tgtEsxUserName;
                }
                if (recoveryForm.tgtEsxPassword != null)
                {
                    recoveryForm.masterHostAdded.esxPassword = recoveryForm.tgtEsxPassword;
                }

                if (recoveryForm.recoveryLaterRadioButton.Checked == true)
                {
                    recoveryForm.masterHostAdded.recoveryLater = true;
                }
                else
                {
                    recoveryForm.masterHostAdded.recoveryLater = false;
                }
                recoveryForm.masterHostAdded.plan = recoveryForm.planNameSelected;
                if (recoveryForm.wmiBasedRadioButton.Checked == true && recoveryForm.appNameSelected == AppName.Recover)
                {
                    recoveryForm.recoveryGroupBox.Enabled = false;
                    recoveryForm.recoveryThroughGroupBox.Enabled = false;                   
                    recoveryForm.recoveryBackgroundWorker.RunWorkerAsync();
                }
                else
                {
                    StatusForm status = new StatusForm(recoveryForm.recoveryChecksPassedList, recoveryForm.masterHostAdded, recoveryForm.appNameSelected, _directoryNameinXmlFile);
                    recoveryForm.closeFormForcelyByUser = true;
                    recoveryForm.Close();
                }
                Trace.WriteLine("Exiting: ProcessPanel of ESX_recoveryReviewPanelHandler");

            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: ProcessPanel Of RecvoeruyReviewPanelHandler " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");

                return false;
            }
            return true;
        }

        internal override bool ValidatePanel(AllServersForm recoveryForm)
        {
            return true;
        }
        internal override bool CanGoToNextPanel(AllServersForm recoveryForm)
        {
            return true;
        }

        internal override bool CanGoToPreviousPanel(AllServersForm recoveryForm)
        {
            recoveryForm.nextButton.Text = "Next >";
            recoveryForm.nextButton.Enabled = true;
            if (recoveryForm.appNameSelected == AppName.Recover)
            {
                recoveryForm.mandatoryLabel.Visible = false;
            }
            if (recoveryForm.appNameSelected == AppName.Recover)
            {
                recoveryForm.mandatoryLabel.Visible = false;
            }
           // recoveryForm.previousButton.Visible = false;
            return true;
        }

        /*
         * In case of Recovery and DR Drill Next button will call this method
         * This will check first wmi work or fx work
         * if fx job means for recovey it will set the fx jobs and done with wizard.
         * If not it will create recovery.bat and and copy both recovery.xm and recovey.bat file to mt and execute the recovery commands and
         * copies Rollback file to vcponfoder and from vconfolder we will copy to planname folder.
         * then we will call detach and power on command for the recovered vms.
         * In case of dr drill first it willc all the readiness checks followed by creation of vm and then 
         * it will check for wmi of fx based. If fx based it wil set the jobs and done. If wmi based it will createdrdrill.bt file
         * and copies to mt and execure the drdrill comaands after that it will copy Snapshot file to vcon folder and frm that location to plan name folder
         * and call the scripts to detach and power on the VM.
         */


        internal bool RecoverMachines(AllServersForm recoveryForm)
        {
            // recoveryBackGroundWorker will call this when user clicks on the recover button.

            int exitCode = 0;
            Esx esx = new Esx();
            try
            {
                if (recoveryForm.wmiBasedRadioButton.Checked == true)
                {
                    if (recoveryForm.masterHostAdded.hostname.Split('.')[0].ToUpper() != Environment.MachineName)
                    {
                        if (recoveryForm.masterHostAdded.userName == null)
                        {

                            if (recoveryForm.masterHostAdded.userName == null)
                            {
                                AddCredentialsPopUpForm addCredentialsForm = new AddCredentialsPopUpForm();
                                addCredentialsForm.credentialsHeadingLabel.Text = "Provide credentials for master target";
                                addCredentialsForm.useForAllCheckBox.Visible = false;
                                addCredentialsForm.ShowDialog();
                                addCredentialsForm.BringToFront();
                                if (addCredentialsForm.canceled == false)
                                {
                                    recoveryForm.masterHostAdded.domain = addCredentialsForm.domain;
                                    recoveryForm.masterHostAdded.userName = addCredentialsForm.userName;
                                    recoveryForm.masterHostAdded.passWord = addCredentialsForm.passWord;
                                }
                                else
                                {
                                    _fileMadeandCopiedSuccessfully = false;
                                    Trace.WriteLine(DateTime.Now + "\t User has cancelled the credentials form");
                                    return false;
                                }

                            }

                        }
                        string error = null;
                        if (WinPreReqs.WindowsShareEnabled(recoveryForm.masterHostAdded.ip, recoveryForm.masterHostAdded.domain, recoveryForm.masterHostAdded.userName, recoveryForm.masterHostAdded.passWord, recoveryForm.masterHostAdded.hostname,  error) == false)
                        {
                            error = WinPreReqs.GetError;
                            _wmiWorkedfine = false;
                            
                            WmiErrorMessageForm errorMessageForm = new WmiErrorMessageForm(error);
                            errorMessageForm.domainText.Text = recoveryForm.masterHostAdded.domain;
                            errorMessageForm.userNameText.Text = recoveryForm.masterHostAdded.userName;
                            errorMessageForm.passWordText.Text = recoveryForm.masterHostAdded.passWord;
                            errorMessageForm.skipWmiValidationsRadioButton.Text = "Do you want to proceed with FX based recovery";
                            errorMessageForm.chooseLabel.Text = "You can proceed to recover by skipping the checks for availability of consistency points";
                            errorMessageForm.ShowDialog();
                            errorMessageForm.BringToFront();
                            //User didn't click the cancel button in the WMI error form
                            if (errorMessageForm.canceled == false)
                            {
                                _wmiMessageFormCancel = false;
                                if (errorMessageForm.continueWithOutValidation == true)
                                {
                                    _reRunCredentials = false;
                                    recoveryForm.masterHostAdded.skipPushAndPreReqs = true;
                                    return false;

                                }
                                else
                                {
                                    _reRunCredentials = true;
                                    //Debug.WriteLine(" Domain " + errorMessageForm.domain + errorMessageForm.username + errorMessageForm.password);
                                    recoveryForm.masterHostAdded.domain = errorMessageForm.domain;
                                    recoveryForm.masterHostAdded.userName = errorMessageForm.username;
                                    recoveryForm.masterHostAdded.passWord = errorMessageForm.password;

                                    //   MessageBox.Show("domain " + errorMessageForm.domain + errorMessageForm.username + errorMessageForm.password);
                                    return false;
                                }
                            }
                            else
                            {
                                _reRunCredentials = false;
                                _wmiMessageFormCancel = true;
                                return false;
                            }

                        }
                        else
                        {
                            recoveryForm.masterHostAdded.skipPushAndPreReqs = false;
                            _wmiWorkedfine = true;
                        }
                    }
                }
             
                if (recoveryForm.appNameSelected == AppName.Recover)
                {
                    tickerDelegate = new TickerDelegate(SetLeftTicker);
                    string mtIp = null;
                    if (recoveryForm.masterHostAdded.hostname.Split('.')[0].ToUpper() == Environment.MachineName.ToUpper())
                    {
                        mtIp = ".";
                    }
                    else
                    {
                        mtIp = recoveryForm.masterHostAdded.ip;
                    }
                    if (_wmirollback == false)
                    {

                        WinPreReqs win = new WinPreReqs(mtIp, recoveryForm.masterHostAdded.domain, recoveryForm.masterHostAdded.userName, recoveryForm.masterHostAdded.passWord);
                        string displaynames = null;
                        foreach (Host h in recoveryForm.recoveryChecksPassedList._hostList)
                        {
                            if (displaynames == null)
                            {
                                displaynames = h.new_displayname;
                            }
                            else
                            {
                                displaynames = displaynames + " , " + h.new_displayname;
                            }
                        }
                        _message = "Executing rollback for the VM:" + Environment.NewLine + displaynames + Environment.NewLine;

                        recoveryForm.recoveryText.Invoke(tickerDelegate, new object[] { _message });
                        if (mtIp == ".")
                        {
                            if (WinTools.RecoveryLocally(100000, "\"" + _planFolderPath + "\\" + _directoryNameinXmlFile + "\"", _planFolderPath + "\\vCon_Error.log",_directoryNameinXmlFile) == 0)
                            {
                                if (File.Exists(_planFolderPath + "\\" + _directoryNameinXmlFile + "\\InMage_Recovered_Vms.rollback"))
                                {
                                    _message = "Recovery completed and now powering on the following machines: " + Environment.NewLine + displaynames + Environment.NewLine;
                                    recoveryForm.recoveryText.Invoke(tickerDelegate, new object[] { _message });
                                    Trace.WriteLine(DateTime.Now + "\t Came here to copy file to the direcotry " + _directoryNameinXmlFile);
                                    //File.Copy(WinTools.FxAgentPath() + "\\vContinuum\\InMage_Recovered_Vms.snapshot", _planFolderPath + "\\" + _directoryNameinXmlFile + "\\InMage_Recovered_Vms.snapshot", true);
                                    _wmirollback = true;
                                }
                                else
                                {
                                    Trace.WriteLine(DateTime.Now + "\t InMage_Recovered_Vms.rollback file does not exist in the planname folder ");
                                    _message = "Recovery failed for the selected VM(s). " + Environment.NewLine;
                                    recoveryForm.recoveryText.Invoke(tickerDelegate, new object[] { _message });
                                    _wmirollback = false;
                                    return false;
                                }
                            }
                            else
                            {
                                _message = "Recovery failed for the selected VM(s). " + Environment.NewLine;
                                recoveryForm.recoveryText.Invoke(tickerDelegate, new object[] { _message });
                                _wmirollback = false;
                                return false;
                            }
                        }
                        else if (win.WmiBasedRecovery(recoveryForm.masterHostAdded.hostname,  WinTools.GetcxIPWithPortNumber(),_directoryNameinXmlFile) == 0)
                        {
                            if (AllServersForm.v2pRecoverySelected == true)
                            {
                                _message = "Rollback is completed and now uploading configuaration files. " + Environment.NewLine + displaynames + Environment.NewLine;
                            }
                            else
                            {
                                _message = "Rollback is completed and now powering on the following machines " + Environment.NewLine + displaynames + Environment.NewLine;
                            }
                            recoveryForm.recoveryText.Invoke(tickerDelegate, new object[] { _message });
                            if (File.Exists(WinTools.FxAgentPath() + "\\vContinuum\\InMage_Recovered_Vms.rollback"))
                            {
                                Trace.WriteLine(DateTime.Now + "\t Came here to copy file to the direcotry " + _directoryNameinXmlFile);
                                File.Copy(WinTools.FxAgentPath() + "\\vContinuum\\InMage_Recovered_Vms.rollback", _planFolderPath + "\\" + _directoryNameinXmlFile + "\\InMage_Recovered_Vms.rollback", true);
                                _wmirollback = true;
                            }
                            else
                            {
                                _message = "Rollback failed for the selected VM(s). " + Environment.NewLine;
                                recoveryForm.recoveryText.Invoke(tickerDelegate, new object[] { _message });
                                Trace.WriteLine(DateTime.Now + "\t InMage_Recovered_Vms.rollback file does not exist in the vContinuum folder ");
                                return false;
                            }

                        }
                        else
                        {
                            _message = "Rollback failed for the following vms: " + displaynames + Environment.NewLine;
                            recoveryForm.recoveryText.Invoke(tickerDelegate, new object[] { _message });
                            _wmirollback = false;
                            return false;
                        }
                    }
                    Trace.WriteLine(DateTime.Now + "\t Came here because successfully completed the rollback.");

                    Esx esxInfo = new Esx();
                    
                    if (_scriptsCall == false)
                    {
                        if (AllServersForm.v2pRecoverySelected == false)
                        {
                            _message = "Powering on the recovered machines " + Environment.NewLine;
                            recoveryForm.recoveryText.Invoke(tickerDelegate, new object[] { _message });

                            if (esxInfo.WmiRecovery(recoveryForm.esxIPSelected, _planFolderPath + "\\" + _directoryNameinXmlFile,"/home/svsystems/vcon/" + _directoryNameinXmlFile, OperationType.Recover) == 0)
                            {
                                _message = "Powering on the machines completed successfully" + Environment.NewLine;
                                recoveryForm.recoveryText.Invoke(tickerDelegate, new object[] { _message });
                                _scriptsCall = true;
                                recoverCompleted = true;
                            }
                            else
                            {
                                _message = "Detaching and powerng on failed for the selected machines. " + Environment.NewLine;
                                recoveryForm.recoveryText.Invoke(tickerDelegate, new object[] { _message });
                                _scriptsCall = false;
                                recoverCompleted = false;
                            }
                        }
                        else
                        {
                            _message = "Updating Configuration Files. " + Environment.NewLine;
                            recoveryForm.recoveryText.Invoke(tickerDelegate, new object[] { _message });

                            if (esxInfo.WmiRecoveryForV2p("10.0.1.45", _planFolderPath + "\\" + _directoryNameinXmlFile, OperationType.Recover) == 0)
                            {
                                _message = "Uploading files completed successfully." + Environment.NewLine +
                                           "Detach USB disk  and Boot physical server.";
                                recoveryForm.recoveryText.Invoke(tickerDelegate, new object[] { _message });
                                _scriptsCall = true;
                                recoverCompleted = true;
                            }
                            else
                            {
                                _message = "Failed to upload Configuration files. " + Environment.NewLine;
                                recoveryForm.recoveryText.Invoke(tickerDelegate, new object[] { _message });
                                _scriptsCall = false;
                                recoverCompleted = false;
                            }
                        }
                    }
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

            return true;
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
                    masterHost = Cxapicalls.GetHost;
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
        private void SetLeftTicker(string s)
        {
            _textBox.AppendText(s);
        }

        /*
         * After comption of the recover or dr drill steps it will come  here.
         * Here we wil check whether all the operation done successflly or any failures. 
         * Any failures we will display to user by log text box.
         * if all the steps went fine we will show the done button here.
         */
        internal bool PostActionAfterRecoveryCompleted(AllServersForm recoveryForm)
        {
            try
            {
                recoveryForm.recoveryPlanNameTextBox.Enabled = true;

                recoveryForm.progressBar.Visible = false;
                recoveryForm.recoveryGroupBox.Enabled = true;
                recoveryForm.recoveryThroughGroupBox.Enabled = true;
                if (recoveryForm.wmiBasedRadioButton.Checked == true)
                {
                    if (_wmiWorkedfine == false && _reRunCredentials == true)
                    {
                        recoveryForm.progressBar.Visible = true;
                        recoveryForm.previousButton.Enabled = false;
                        recoveryForm.nextButton.Enabled = false;

                        recoveryForm.recoveryReviewDataGridView.Enabled = false;
                        recoveryForm.recoveryBackgroundWorker.RunWorkerAsync();
                        return false;

                    }
                    if (recoveryForm.masterHostAdded.skipPushAndPreReqs == true)
                    {
                        recoveryForm.masterHostAdded.skipPushAndPreReqs = false;
                        recoveryForm.previousButton.Enabled = true;
                        recoveryForm.nextButton.Enabled = true;
                        recoveryForm.recoveryReviewDataGridView.Enabled = true;
                        recoveryForm.recoveryLogTextBox.AppendText("Select FX based recovery and continue." + Environment.NewLine);
                        return false;
                    }
                    if(_wmiMessageFormCancel == true)
                    {
                        recoveryForm.masterHostAdded.skipPushAndPreReqs = false;
                        recoveryForm.previousButton.Enabled = true;
                        recoveryForm.nextButton.Enabled = true;
                        recoveryForm.recoveryReviewDataGridView.Enabled = true;
                        return false;
                    }
                }
                if (recoveryForm.appNameSelected == AppName.Recover)                
                {
                    //After setting up the fx jobs or any failures we will display in the method.
                    if (recoveryForm.wmiBasedRecvoerySelected == false)
                    {

                        if (recoverCompleted == true)
                        {
                            recoveryForm.previousButton.Enabled = true;
                            recoveryForm.nextButton.Enabled = true;
                            //recoveryForm.recoveryLogTextBox.AppendText("Created FX jobs necessary for recovery." + Environment.NewLine + "To monitor jobs visit");
                           // recoveryForm.recoveryLogTextBox.AppendText("http://" + WinTools.GetcxIPWithPortNumber() + "/ui" + " and access Monitor->" + recoveryForm._planName + Environment.NewLine);
                           // recoveryForm.recoveryLogTextBox.AppendText(Environment.NewLine + "Note:");
                            //recoveryForm.recoveryLogTextBox.AppendText("Make sure that vContinuum machine(local system) is up and running till machines are recovered." + Environment.NewLine);
                            recoveryForm.nextButton.Visible = false;
                            recoveryForm.cancelButton.Visible = false;
                            recoveryForm.previousButton.Visible = false;
                            recoveryForm.doneButton.Visible = true;
                            
                            
                               // recoveryForm.recoverylinkLabel.Visible = true;
                            
                        }
                        else
                        {
                            recoveryForm.previousButton.Enabled = true;
                            recoveryForm.nextButton.Enabled = true;
                            try
                            {
                                
                                StreamReader sr = new StreamReader(_installPath + RECOVERY_ERROR_LOG_FILE);

                                string firstLine;
                                firstLine = sr.ReadToEnd();
                                recoveryForm.recoveryLogTextBox.AppendText(firstLine);
                                sr.Close();
                                recoveryForm.progressBar.Visible = false;
                            }
                            catch (Exception ex)
                            {

                                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                                Trace.WriteLine("ERROR*******************************************");
                                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                                Trace.WriteLine("Exception  =  " + ex.Message);
                                Trace.WriteLine("ERROR___________________________________________");

                                return false;
                            }
                            // recoveryForm._index--;
                            return false;
                        }
                    }
                    else
                    {
                        if (recoverCompleted == true)
                        {

                            if (_setFxjobs == true)
                            {
                                recoveryForm.recoveryLogTextBox.AppendText("Created FX jobs necessary for Snapshot." + Environment.NewLine + "To montior jobs visit");
                                recoveryForm.recoveryLogTextBox.AppendText("http://" + WinTools.GetcxIPWithPortNumber() + "/ui" + " and access Monitor->" + recoveryForm.planNameSelected + Environment.NewLine);
                                recoveryForm.recoveryLogTextBox.AppendText(Environment.NewLine + "Note:");
                                recoveryForm.recoveryLogTextBox.AppendText("Make sure that vContinuum machine(local system) is up and running till machines are recovered." + Environment.NewLine);
                                recoveryForm.nextButton.Visible = false;
                            }
                            recoveryForm.previousButton.Enabled = true;
                            recoveryForm.nextButton.Visible = false;
                            recoveryForm.cancelButton.Visible = false;
                            recoveryForm.previousButton.Visible = false;
                            recoveryForm.doneButton.Visible = true;
                            recoveryForm.recoverylinkLabel.Visible = true;
                        }
                        else
                        {
                            recoveryForm.previousButton.Enabled = true;
                            recoveryForm.nextButton.Enabled = true;
                            try
                            {
                                StreamReader sr = new StreamReader(_installPath + RECOVERY_ERROR_LOG_FILE);

                                string firstLine;
                                firstLine = sr.ReadToEnd();
                                recoveryForm.recoveryLogTextBox.AppendText(firstLine);
                                sr.Close();
                                recoveryForm.progressBar.Visible = false;
                            }
                            catch (Exception ex)
                            {

                                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                                Trace.WriteLine("ERROR*******************************************");
                                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                                Trace.WriteLine("Exception  =  " + ex.Message);
                                Trace.WriteLine("ERROR___________________________________________");

                                return false;
                            }
                            // recoveryForm._index--;
                            return false;
                        }
                       
                    }
                }
                else if (recoveryForm.appNameSelected == AppName.Drdrill)
                {
                    if (_readinessForDrDrill == false)
                    {
                        recoveryForm.previousButton.Enabled = true;
                    }
                    if (_dummyDiskCreation == false)
                    {
                        recoveryForm.nextButton.Enabled = true;
                        recoveryForm.recoveryLogTextBox.AppendText("Failed to create dummy disks." + Environment.NewLine);
                        ReadLogFile(recoveryForm);
                        return false;
                    }
                    if (_createdVm == false || _setFxjobs == false)
                    {
                        recoveryForm.nextButton.Enabled = true;
                        ReadLogFile(recoveryForm);
                        return false;

                    }
                    if (_setFxjobs == true)
                    {
                        recoveryForm.recoveryLogTextBox.AppendText("Created FX jobs necessary for Snapshot." + Environment.NewLine + "To montior jobs visit");
                        recoveryForm.recoveryLogTextBox.AppendText("http://" + WinTools.GetcxIPWithPortNumber() + "/ui" + " and access Monitor->" + recoveryForm.planNameSelected + Environment.NewLine);
                        recoveryForm.recoveryLogTextBox.AppendText(Environment.NewLine + "Note:");
                        recoveryForm.recoveryLogTextBox.AppendText("Make sure that vContinuum machine(local system) is up and running till machines are recovered." + Environment.NewLine);
                        recoveryForm.nextButton.Visible = false;
                    }
                    if (recoverCompleted == true)
                    {
                        recoveryForm.previousButton.Enabled = true;
                        recoveryForm.nextButton.Visible = false;
                        recoveryForm.cancelButton.Visible = false;
                        recoveryForm.previousButton.Visible = false;
                        recoveryForm.doneButton.Visible = true;
                        recoveryForm.recoverylinkLabel.Visible = true;
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
            try
            {
                if (File.Exists(_installPath + "\\logs\\vContinuum_UI.log"))
                {
                    if (_directoryNameinXmlFile != null)
                    {
                        Program.tr3.Dispose();
                        System.IO.File.Move(WinTools.FxAgentPath() + "\\vContinuum\\logs\\vContinuum_UI.log", _planFolderPath + "\\" + _directoryNameinXmlFile + "\\vContinuum_UI.log");
                        Program.tr3.Dispose();
                    }
                }
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed to copy vCOntinuum_UI.log " + ex.Message);
            }
            recoveryForm.progressBar.Visible = false;
            return true;
        }

        /*
         * When ever we saw a failure from scripts we will call this method.
         * This will read the vContinuum_ESX.log for the error and will display in the log box.
         * This is to display error message to user.
         */

        internal bool ReadLogFile(AllServersForm recoveryForm)
        {
            try
            {
                StreamReader sr = new StreamReader(_installPath + RECOVERY_ERROR_LOG_FILE);

                string firstLine;
                firstLine = sr.ReadToEnd();
                recoveryForm.recoveryLogTextBox.AppendText(firstLine);
                sr.Close();
                recoveryForm.progressBar.Visible = false;
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");

                return false;
            }
            return true;
        }


        internal bool ReadinessChecksForDrDrill(AllServersForm allServersForm)
        {
            if (File.Exists(_latestFilePah + "\\Snapshot.xml"))
            {
                File.Delete(_latestFilePah + "\\Snapshot.xml");
            }
            HostList masterList = new HostList();
            masterList.AddOrReplaceHost(allServersForm.masterHostAdded);
            return true;
        }

        private bool CheckRecoveryOrder(AllServersForm recoveryForm)
        {
            try
            {
                int totalCount = 0;
                totalCount = recoveryForm.recoveryReviewDataGridView.RowCount;
                for (int i = 0; i < recoveryForm.recoveryReviewDataGridView.RowCount; i++)
                {

                    for (int j = 1; j < recoveryForm.recoveryReviewDataGridView.RowCount; j++)
                    {
                        //Trace.WriteLine(DateTime.Now + "\t Printing the i and j values " + i.ToString() +"  "+  j.ToString());
                        if (i != j && i < j)
                        {
                            //Trace.WriteLine(DateTime.Now + "\t Printing the i and j values " + i.ToString() + "  " + j.ToString()+" "  + totalCount.ToString());
                            if (int.Parse(recoveryForm.recoveryReviewDataGridView.Rows[i].Cells[RECOVERY_ORDER_COLUMN].Value.ToString()) == int.Parse(recoveryForm.recoveryReviewDataGridView.Rows[j].Cells[RECOVERY_ORDER_COLUMN].Value.ToString()))
                            {
                                totalCount = totalCount - 1;

                                break;
                                //MessageBox.Show("Two machines have same recovery order number. Please select different recovery order", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                //return false;
                            }
                        }
                    }

                }
                Trace.WriteLine(DateTime.Now + "\t Printing the totl cout " + totalCount.ToString());
                for (int i = 0; i < recoveryForm.recoveryReviewDataGridView.RowCount; i++)
                {
                    if (int.Parse(recoveryForm.recoveryReviewDataGridView.Rows[i].Cells[RECOVERY_ORDER_COLUMN].Value.ToString()) > totalCount)
                    {
                        MessageBox.Show("Please check the recovery order.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return false;
                    }
                    if (int.Parse(recoveryForm.recoveryReviewDataGridView.Rows[i].Cells[RECOVERY_ORDER_COLUMN].Value.ToString()) < 1)
                    {
                        MessageBox.Show("The recovery order cannot be less than 1", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return false;
                    }
                }

                for (int i = 1; i < recoveryForm.recoveryReviewDataGridView.RowCount + 1; i++)
                {

                    foreach (Host h in recoveryForm.recoveryChecksPassedList._hostList)
                    {
                        if (h.hostname == recoveryForm.recoveryReviewDataGridView.Rows[i - 1].Cells[0].Value.ToString())
                        {
                            h.recoveryOrder = int.Parse(recoveryForm.recoveryReviewDataGridView.Rows[i - 1].Cells[RECOVERY_ORDER_COLUMN].Value.ToString());
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: CheckRecoveryOrder " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            return true;
        }

        internal bool CheckRecoveryOrderandPrePareXMLfile(AllServersForm recoveryForm)
        {
            try
            {
                // Checking for the recovery order selected by the user...
                if (CheckRecoveryOrder(recoveryForm) == false)
                {
                    Trace.WriteLine(DateTime.Now + "\t Found an error in checkrecoveryorder ");
                    return false;
                }


                CheckPowerSelectedOption(recoveryForm);
                string physicalMachinesList = null;
                foreach (Host h in recoveryForm.recoveryChecksPassedList._hostList)
                {
                    if (h.powerOnVM == false && h.machineType == "PhysicalMachine")
                    {
                        physicalMachinesList = h.displayname + Environment.NewLine;
                    }
                }

                if (physicalMachinesList != null)
                {
                    string message = "Note: You have selected not to Power on following Virtual Machines " + Environment.NewLine
                                    + physicalMachinesList + Environment.NewLine
                                    + "Vmware tools will not be installed on those VMs. As these are original physical server," + Environment.NewLine
                                    + "you need to install vmware tools manually on secondary esx server";
                    MessageBox.Show(message, "Information", MessageBoxButtons.OK, MessageBoxIcon.Information);
                }

                foreach (Host h in recoveryForm.recoveryChecksPassedList._hostList)
                {
                    //Debug.WriteLine(":++++++++++++++++++++ Printing recovery order" + h.recoveryOrder);
                    if (recoveryForm.appNameSelected == AppName.Drdrill)
                    {
                        h.target_uuid = null;
                    }
                    recoveryForm.finalSelectedList.AddOrReplaceHost(h);
                }

                




                HostList masterList = new HostList();
                masterList.AddOrReplaceHost(recoveryForm.masterHostAdded);
                Esx esx = new Esx();
                esx.configurationList = recoveryForm.masterHostAdded.configurationList;
                esx.dataStoreList = recoveryForm.masterHostAdded.dataStoreList;
                esx.lunList = recoveryForm.masterHostAdded.lunList;
                esx.networkList = recoveryForm.masterHostAdded.networkNameList;
                SolutionConfig solution = new SolutionConfig();
                foreach (Host h in recoveryForm.recoveryChecksPassedList._hostList)
                {
                    foreach (Hashtable hash in h.disks.list)
                    {
                        hash["Protected"] = "no";
                    }
                    if (h.recoveryOperation == 4)
                    {
                        DateTime dt = DateTime.Now;
                        dt = DateTime.Parse(h.selectedTimeByUser);
                        DateTime gmt = DateTime.Parse(dt.ToString());

                        h.selectedTimeByUser = gmt.ToString("yyyy/MM/dd HH:mm:ss");
                    }
                }
                solution.WriteXmlFile(recoveryForm.recoveryChecksPassedList, masterList, esx, WinTools.GetcxIP(), "Snapshot.xml", recoveryForm.planNameSelected, false);
                recoveryForm.progressBar.Visible = true;
                recoveryForm.recoveryReviewDataGridView.Enabled = false;
                recoveryForm.recoveryLogTextBox.AppendText("Running readiness checks on secondary server"+ Environment.NewLine);
                recoveryForm.drDrillReadinessCheckButton.Enabled = false;
                recoveryForm.previousButton.Enabled = false;
                recoveryForm.nextButton.Enabled = false;
                recoveryForm.drDrillReadinessCheckBackgroundWorker.RunWorkerAsync();
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: CheckRecoveryOrderandPrePareXMLfile " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");

                return false;
            }
            return true;
        }


        internal bool ReadinessChecks(AllServersForm allServersFrom)
        {
            try
            {
                Esx esx = new Esx();
                if (AllServersForm.arrayBasedDrdrillSelected == true)
                {
                    _readinesschecksReturnCode = esx.PreReqCheckForDRDrill(allServersFrom.masterHostAdded.esxIp, "dr_drill_array");


                }
                else
                {

                    _readinesschecksReturnCode = esx.PreReqCheckForDRDrill(allServersFrom.masterHostAdded.esxIp, "dr_drill");

                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: ReadinessChecks " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");

                return false;
            }
            return true;
        }

        internal bool PostReadinessChecks(AllServersForm allServersForm)
        {
            try
            {
                allServersForm.progressBar.Visible = false;
                allServersForm.previousButton.Enabled = true;
                if (_readinesschecksReturnCode == 0)
                {
                    allServersForm.recoveryPlanNameTextBox.Visible = true;
                    allServersForm.recoveryPlanNameLabel.Visible = true;
                    allServersForm.nextButton.Enabled = true;
                    allServersForm.recoveryLogTextBox.AppendText("Completed readiness checks on secondary server. Enter planname and click on start" + Environment.NewLine);
                    allServersForm.errorProvider.SetError(allServersForm.recoveryPlanNameTextBox, "Enter Planame");
                }
                else
                {
                    allServersForm.recoveryReviewDataGridView.Enabled = true;
                    allServersForm.recoveryLogTextBox.AppendText("Failed to complete readiness checks on secondar server" + Environment.NewLine);
                    try
                    {
                        // This will update the log box when some scripts calls are failed.....
                        StreamReader sr1 = new StreamReader(WinTools.FxAgentPath() + "\\vContinuum\\logs\\vContinuum_ESX.log");
                        string firstLine1;
                        firstLine1 = sr1.ReadToEnd();

                        allServersForm.recoveryLogTextBox.AppendText(firstLine1 + Environment.NewLine);

                        sr1.Close();
                        sr1.Dispose();
                    }
                    catch (Exception e)
                    {
                        allServersForm.recoveryLogTextBox.AppendText("Some other process is accessing the file logs\\vContinuum_ESX.log" + Environment.NewLine);
                        System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);
                        Trace.WriteLine("ERROR*******************************************");
                        Trace.WriteLine(DateTime.Now + "Caught exception at Method: PostReadinessChecks updateLogTextBox " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                        Trace.WriteLine("Exception  =  " + e.Message);
                        Trace.WriteLine("ERROR___________________________________________");
                        return false;
                    }
                }
                allServersForm.drDrillReadinessCheckButton.Enabled = true;
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: PostReadinessChecks " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");

                return false;
            }
            return true;
        }

    }
}