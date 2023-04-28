using System;
using System.Collections.Generic;
using System.Collections;
using System.Text;
using System.Diagnostics;
using Com.Inmage.Esxcalls;
using System.IO;
using System.Windows.Forms;
using System.Threading;
using System.Xml;
using Com.Inmage.Tools;
using System.Drawing;


namespace Com.Inmage.Wizard
{ //01_020_XXXX
    class Esx_ProtectionPanelHandler : PanelHandler
    {
        internal static long TIME_OUT_MILLISECONDS = 3600000;
        internal static int SOURCE_COLUMN = 0;
        internal static int SOURCE_HOSTNAME_COLUMN = 1;
        internal static int SOURCEIP_ADDRESS_COLUMN = 2;

        internal static int SOURCE_SELECT_UNSELECT_COLUMN = 3;
        //public static int CX_IP_COLUMN = 3;
        private string      PERL_BINARY         = "perl.exe";
        internal string MASTER_FILE;
        internal string PHYSICAL_MACHINE_LIST_FILE = "Physical_Machine_list.txt";
        string FXSERVICE_NAME = "frsvc";
        string VXSERVICE_NAME = "svagents";
        internal static bool POST_JOBAUTOMATION = false;
        internal string _directoryNameinXmlFile = null;
        static AllServersForm _allServersForm;
        internal string _installPath;
        internal bool _createGuestOnTargetSucceed = false;
        internal bool _runMTCheckOnce = false;
        internal bool _dummyDiskCreation = false;
        internal bool _jobAutomationSucceed = false;
        internal bool _esxutilwinExecution = false;
        internal bool _generatedVmxFiles = false;
        bool _targetESXPreChekCompleted = false;
        internal string _cxIPwithPortNumber = null;
        internal int _rowIndex = 0;
        internal System.Drawing.Bitmap _passed;
        internal System.Drawing.Bitmap _failed;
        internal System.Drawing.Bitmap _warning;
        //these are the columns for checkReportDataGridView.
        internal bool _postJobAutomationResult = true;
        internal static int HOSTNAME_COLUMN = 0;
        internal static int CHECK_COLUMN = 1;
        internal static int RESULT_COLUMN = 2;
        internal static int ACTION_COLUMN = 3;
        internal static bool _hasPlanName = false;
        internal ArrayList _planNameList = new ArrayList();
        internal static bool _usingExistingPlanName = false;
        internal string planName = null;
        internal static string LINX_SCRIPT = "linux_script.sh";
        internal string _fxFailOverDataPath = null;
        internal string _latestFilePath = null;
        internal string _cxInstallationPath = null;
        HostList readinessCheckHostList = new HostList();
        string _reUseVmdksString = null; //This string contains list vms which are found on target esx server. this is populated by reading values after target readiness check where targetuid=!null.
                                         //it means a vm is already found in readiness check itself

        //Thread childThread;
        //int i = 10;  
        Esx _esx;
        Host masterHost ;
        public bool _vconMT = false;
        private delegate void TickerDelegate(string s);
       
        Host _tempHost = new Host();
        internal bool _targetReadinessCheck = false;
        TickerDelegate tickerDelegate;
        TextBox _textBox = new TextBox();
        internal static string _message = null;
        public Esx_ProtectionPanelHandler()
        {
            _installPath = WinTools.FxAgentPath() + "\\vContinuum";
            _cxIPwithPortNumber = WinTools.GetcxIPWithPortNumber();
           // LINX_SCRIPT = _installPath + "\\linux_script.sh";
            _passed = Wizard.Properties.Resources.tick;
            _failed = Wizard.Properties.Resources.cross;
            _warning = Wizard.Properties.Resources.warning;
            _fxFailOverDataPath = WinTools.FxAgentPath() + "\\Failover\\Data";
            _latestFilePath = WinTools.FxAgentPath() + "\\vContinuum\\Latest";
            
        }

        internal override bool Initialize(AllServersForm allServersForm)
        {
            try
            {

                //if (allServersForm.appNameSelected != AppName.ADDDISKS)
                //{
                //    allServersForm.mandatoryLabel.Visible = true;
                //    allServersForm.mandatorydoubleFieldLabel.Visible = true;
                //}
                _textBox = allServersForm.protectionText;
                allServersForm.helpContentLabel.Text = HelpForcx.ProtectScreen5;
                allServersForm.protectionText.Visible = true;
                allServersForm.preReqsbutton.Select();
                allServersForm.preReqsbutton.Enabled = true;
                _targetESXPreChekCompleted = false;
                allServersForm.nextButton.Enabled = false;
                allServersForm.planNameTableLayoutPanel.Visible = false;
                masterHost = (Host)allServersForm.finalMasterListPrepared._hostList[0];
                allServersForm.reviewDataGridView.RowCount = allServersForm.selectedSourceListByUser._hostList.Count;
                int rowIndex = 0;
                // Loading into  the datagridview in case p2v protection bmr is nothing but p2v
                if (allServersForm.appNameSelected == AppName.Bmr)
                {
                    allServersForm.reviewDataGridView.Columns[SOURCE_COLUMN].HeaderText = "Source server name";
                    foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                    {                        
                        allServersForm.reviewDataGridView.Rows[rowIndex].Cells[SOURCE_COLUMN].Value = h.displayname;
                        allServersForm.reviewDataGridView.Rows[rowIndex].Cells[SOURCE_HOSTNAME_COLUMN].Value = h.hostname;
                        allServersForm.reviewDataGridView.Rows[rowIndex].Cells[SOURCEIP_ADDRESS_COLUMN].Value = h.ip;
                        allServersForm.reviewDataGridView.Rows[rowIndex].Cells[SOURCE_SELECT_UNSELECT_COLUMN].Value = "Unselect";
                       
                        rowIndex++;
                    }
                }
                //Clearing the items before filling the dropdownlist.....
               
                
               
                // Loading into the datagrid view in case of esx protection, failback protection and offline sync export..
                
                if (allServersForm.appNameSelected == AppName.Esx || allServersForm.appNameSelected == AppName.Failback || allServersForm.appNameSelected == AppName.Offlinesync)
                {
                    allServersForm.reviewDataGridView.Columns[SOURCE_COLUMN].HeaderText = "Source Server Name";
                    foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                    {                        
                        allServersForm.reviewDataGridView.Rows[rowIndex].Cells[SOURCE_COLUMN].Value = h.displayname;
                        allServersForm.reviewDataGridView.Rows[rowIndex].Cells[SOURCE_HOSTNAME_COLUMN].Value = h.hostname;
                        allServersForm.reviewDataGridView.Rows[rowIndex].Cells[SOURCEIP_ADDRESS_COLUMN].Value = h.ip;
                        allServersForm.reviewDataGridView.Rows[rowIndex].Cells[SOURCE_SELECT_UNSELECT_COLUMN].Value = "Unselect";
                        
                       
                        rowIndex++;
                    }
                }
                // If resourcepools are no there on secondary esx server we are filling the grid with default....
                

                // Loading into the datagrid view in case of addition of disks....
                if (allServersForm.appNameSelected == AppName.Adddisks)
                {
                    
                    allServersForm.reviewDataGridView.Columns[SOURCE_COLUMN].HeaderText = "Source Server Name";
                    foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                    {
                        allServersForm.reviewDataGridView.Rows[rowIndex].Cells[SOURCE_COLUMN].Value = h.displayname;
                        allServersForm.reviewDataGridView.Rows[rowIndex].Cells[SOURCE_HOSTNAME_COLUMN].Value = h.hostname;
                        allServersForm.reviewDataGridView.Rows[rowIndex].Cells[SOURCEIP_ADDRESS_COLUMN].Value = h.ip;
                        allServersForm.reviewDataGridView.Rows[rowIndex].Cells[SOURCE_SELECT_UNSELECT_COLUMN].Value = "Unselect";
                        rowIndex++;
                    }
                }
                Host tempHost = (Host)allServersForm.finalMasterListPrepared._hostList[0];
                if (tempHost.vCenterProtection == "Yes")
                {
                    allServersForm.secondaryVCenterTextBox.Text = tempHost.esxIp;
                    allServersForm.targetESXIPTextBox.Text = tempHost.vSpherehost;

                }
                else
                {
                    allServersForm.secondaryVCenterTextBox.Text = "N/A";
                    allServersForm.targetESXIPTextBox.Text = tempHost.esxIp;
                }
                allServersForm.masterTargetTextBox.Text = tempHost.displayname;
                allServersForm.nextButton.Text = "Protect";
                //allServersForm.logTextBox.Visible = false;
                allServersForm.previousButton.Visible = true;
                Debug.WriteLine("Printing Temp list");
                // In case of addition of disk we are not displaying the option to select the batch resync values so disabling.....
                if (allServersForm.appNameSelected == AppName.Adddisks )
                {
                    allServersForm.reviewDataGridView.Columns[SOURCE_SELECT_UNSELECT_COLUMN].Visible = false;
                    allServersForm.limitResyncCheckBox.Visible = false;
                    allServersForm.limitResyncTextBox.Visible = false;
                    allServersForm.batchResyncDescriptionLabel.Visible = false;
                }

               


                Trace.WriteLine("Reached P2V_ProtectionPanelHandler  initialzie");
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

        internal bool TargetReadynessCheck(AllServersForm allServersForm)
        {
            try
            {
                bool timedOut = false;
                Process p;
                Esx esx = new Esx();
                int exitCode;

              
                
                // Calling the readiness checks for the scripts.......
                for (int j = 0; j < allServersForm.finalMasterListPrepared._hostList.Count; j++)
                {
                    Host masterHost = (Host)allServersForm.finalMasterListPrepared._hostList[j];
                    Cxapicalls cxAPi = new Cxapicalls();
                    cxAPi.GetHostCredentials(masterHost.esxIp);
                    if (Cxapicalls.GetUsername != null && Cxapicalls.GetPassword != null)
                    {
                        Trace.WriteLine(DateTime.Now + "\t CX has target esx creds ");
                    }
                    else
                    {
                        if (cxAPi. UpdaterCredentialsToCX(masterHost.esxIp, masterHost.esxUserName, masterHost.esxPassword) == true)
                        {
                            Trace.WriteLine(DateTime.Now + "\t Updated creds to cx");
                            cxAPi.GetHostCredentials(masterHost.esxIp);
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
                    //MessageBox.Show("Username " + masterHost.esxUserName  + masterHost.esxPassword);
                    if (allServersForm.appNameSelected == AppName.Failback)
                    {
                        Debug.WriteLine("     Running prereqs for rdm failback");
                        p = esx.PreReqCheck(masterHost.esxIp, "yes");
                    }
                    else if (allServersForm.appNameSelected == AppName.Adddisks)
                    {
                        p = esx.PreReqCheckForIncrementalDisk(masterHost.esxIp);
                    }
                    else if (allServersForm.appNameSelected == AppName.Offlinesync)
                    {
                        Trace.WriteLine(DateTime.Now + " \t Came here to check the prereqs");

                        p = esx.PreReqCheckForOfflineSyncExport(masterHost.esxIp);
                    }
                    else
                    {
                        p = esx.PreReqCheck(masterHost.esxIp, "no");
                    }

                    exitCode = WaitForProcess(p, allServersForm, allServersForm.progressBar, TIME_OUT_MILLISECONDS, ref timedOut);

                    if (exitCode != 0)
                    {
                        _targetReadinessCheck = false;
                        Trace.WriteLine(DateTime.Now + " \t Came here to check exitcode " + exitCode.ToString());                       
                        return false;
                    }
                    else
                    {
                        _targetReadinessCheck = true;
                        if (allServersForm.appNameSelected == AppName.Esx && allServersForm.appNameSelected == AppName.Bmr)
                        {
                            Trace.WriteLine(DateTime.Now + "\t Target readiness checks completed successfully.");
                            Trace.WriteLine(DateTime.Now + "\t Trying to read esx.xml file in the latest folder");
                            // after readiness checks we are going to check that vm already exists on target side. 
                            // If it is already exists we will show to user in tab control.
                            _reUseVmdksString = null;
                            SolutionConfig sol = new SolutionConfig();
                            Host tempMasterHost = new Host();
                            string esxip = null, cxip = null;
                            if (sol.ReadXmlConfigFile("ESX.xml",  readinessCheckHostList,  tempMasterHost,  esxip,  cxip) == true)
                                readinessCheckHostList = SolutionConfig.list;
                            tempMasterHost = SolutionConfig.masterHosts;
                            esxip = SolutionConfig.EsxIP;
                            cxip = SolutionConfig.csIP;
                            {
                                Trace.WriteLine(DateTime.Now + "\t Successfully read the esx.xml file after completion of target readiness check");
                                foreach (Host h in readinessCheckHostList._hostList)
                                {
                                    if (h.target_uuid != null)
                                    {
                                        Trace.WriteLine(DateTime.Now + "\t Found this VM on target side " + h.displayname + "( " + h.ip + ")");
                                        if (_reUseVmdksString == null)
                                        {
                                            _reUseVmdksString = h.displayname + Environment.NewLine;
                                        }
                                        else
                                        {
                                            _reUseVmdksString = _reUseVmdksString + h.displayname + Environment.NewLine;
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
          

            return true;
        }

        // Waiting for the readiness checks to completed.. for this readiness we are not using back groundworker.....
        internal int WaitForProcess(Process p, AllServersForm allServersForm, ProgressBar prgoressBar, long maxWaitTime, ref bool timedOut)
        {
           
            int exitCode = 1;
            try
            {

                long waited = 0;
                int incWait = 100;
               // int exitCode = 1;
                int i = 10;
                //90 Seconds


                // allServersForm.Enabled = false;
             

                waited = 0;
                while (p.HasExited == false)
                {

                    if (waited <= TIME_OUT_MILLISECONDS)
                    {
                        p.WaitForExit(incWait);
                        waited = waited + incWait;
                       
                    }
                    else
                    {
                        if (p.HasExited == false)
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
                    exitCode = p.ExitCode;
                }

                
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

        private bool freezeUI(AllServersForm allServersForm)
        {
            try
            {
                allServersForm.progressBar.Visible = true;
                allServersForm.progressBar.Value = 1;

                // allServersForm.p2v_ProtectionPanel.Enabled = false;
                //allServersForm.Enabled = false;
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
            return true;

        }

        private bool UnFreezeUI(AllServersForm allServersForm)
        {
           // allServersForm.progressBar.Visible = false;
            try
            {
                allServersForm.p2v_ProtectionPanel.Enabled = true;
                allServersForm.Enabled = true;
                Cursor.Current = Cursors.Default;
                updateLogTextBox(allServersForm.protectionText);
                allServersForm.protectionText.Update();
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: unFreezeUI of ESX_ProtectionPanelHandler.cs " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            return true;
        }


        internal override bool ProcessPanel(AllServersForm allServersForm)
        {

            try
            {
                // First we are checking for the dummy disk creation only if it is false we will modify  the xml file and
                // re-create the directory....
                if (_dummyDiskCreation == false)
                {
                    


                    if (allServersForm.appNameSelected != AppName.Adddisks)
                    {
                        if (_usingExistingPlanName == false)
                        {
                            if (allServersForm.planNameText.Text.Trim().Length == 0)
                            {
                                MessageBox.Show("Enter planname to proceed", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                return false;
                            }
                            else
                            {
                                if (allServersForm.planNameText.Text.Length > 40)
                                {
                                    MessageBox.Show("Plan name should not exceed 40 characters. Enter less than 40 characters", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                    return false;
                                }
                                planName = allServersForm.planNameText.Text;
                            }
                        }
                        else
                        {
                            if (allServersForm.existingPlanNameComboBox.SelectedItem != null)
                            {
                                planName = allServersForm.existingPlanNameComboBox.SelectedItem.ToString();
                            }
                            else
                            {
                                MessageBox.Show("Select planname to proceed", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                return false;
                            }
                        }                      
                    
                    }
                    Host masterHost = (Host)allServersForm.selectedMasterListbyUser._hostList[0];
                    allServersForm.protectionTabControl.Visible = true;
                    // allServersForm.checkReportDataGridView.Visible = false;
                    allServersForm.protectionText.Visible = true;
                    // Changing the tab of the readiness checlk table... 
                    allServersForm.protectionTabControl.SelectTab(allServersForm.logsTabPage);
                    allServersForm.errorProvider.Clear();
                    allServersForm.previousButton.Visible = false;
                    

                    // Generating some random number for the directory creation.....
                    Random randomlyGenerateNumber = new Random();
                    int randomNumber = randomlyGenerateNumber.Next(999999);
                    if (allServersForm.limitResyncCheckBox.Checked == true)
                    {
                        if (allServersForm.limitResyncTextBox.Text.Length == 0)
                        {
                            MessageBox.Show("Please enter batch resync value.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            return false;
                        }
                    }
                    allServersForm.reviewDataGridView.Enabled = false;

                    // Trying to modify the esx.xml file for the directory name, plan name and batch re sync......
                    //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
                    //based on above comment we have added xmlresolver as null
                    XmlDocument document = new XmlDocument();
                    document.XmlResolver = null;
                    string xmlfile = _latestFilePath + "\\ESX.xml";

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
                   // document.Load(xmlfile);
                    //reader.Close();
                    //XmlNodeList hostNodes = null;
                    XmlNodeList rootNodes = null, hostNodes = null, diskNodes = null;
                    rootNodes = document.GetElementsByTagName("root");
                    hostNodes = document.GetElementsByTagName("host");
                    diskNodes = document.GetElementsByTagName("disk");
                    if (allServersForm.limitResyncCheckBox.Checked == true)
                    {
                        if (allServersForm.limitResyncTextBox.Text == "0")
                        {
                            MessageBox.Show("Batch resync value should be greater than 0.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                            return false;
                        }
                        Trace.WriteLine("reached to change the root value for batch resync" + allServersForm.limitResyncTextBox.Text.Trim());
                        foreach (XmlNode node in rootNodes)
                        {
                            //esxnode.ParentNode["batchresync"].Value = allServersForm.limitResyncTextBox.Text.Trim();
                            node.Attributes["batchresync"].Value = allServersForm.limitResyncTextBox.Text.Trim();
                            if (allServersForm.appNameSelected != AppName.Adddisks)
                            {
                                node.Attributes["plan"].Value = planName;
                                allServersForm.planInput = planName;
                                
                            }
                           
                        }
                        if (allServersForm.appNameSelected != AppName.Adddisks)
                        {
                            foreach (XmlNode node in hostNodes)
                            {
                                node.Attributes["batchresync"].Value = allServersForm.limitResyncTextBox.Text.Trim(); 
                            }
                        }
                    }
                    else
                    {
                        foreach (XmlNode node in rootNodes)
                        {
                            //node.ParentNode["batchresync"].Value = "0";
                            node.Attributes["batchresync"].Value = "0";
                            if (allServersForm.appNameSelected != AppName.Adddisks)
                            {
                                node.Attributes["plan"].Value = planName;
                                allServersForm.planInput = planName;
                            }
                          
                        }
                        if (allServersForm.appNameSelected != AppName.Adddisks)
                        {
                            foreach (XmlNode node in hostNodes)
                            {
                                node.Attributes["batchresync"].Value = "0";
                            }
                        }
                    }

                    // Checking the plan name character because some of the chars are not allowed in teh plan name.....
                    if (allServersForm.appNameSelected != AppName.Adddisks)
                    {
                        if (_usingExistingPlanName == false)
                        {
                            if (allServersForm.osTypeSelected == OStype.Linux)
                            {
                                if (allServersForm.planNameText.Text.Contains(" "))
                                {
                                    MessageBox.Show("Plan name with spaces does not support spaces", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                    return false;
                                }
                            }
                            if (allServersForm.planNameText.Text.Contains("&") || allServersForm.planNameText.Text.Contains("\"") || allServersForm.planNameText.Text.Contains("\'") || allServersForm.planNameText.Text.Contains("<") || allServersForm.planNameText.Text.Contains(">") || allServersForm.planNameText.Text.Contains(":") || allServersForm.planNameText.Text.Contains("\\") || allServersForm.planNameText.Text.Contains("/") || allServersForm.planNameText.Text.Contains("|") || allServersForm.planNameText.Text.Contains("*") || allServersForm.planNameText.Text.Contains("="))
                            {
                                MessageBox.Show("Plan name does not accept character * : = ? < > / \\ | & \' \"", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                return false;
                            }
                            WinPreReqs win = new WinPreReqs("", "", "", "");
                            if (WinPreReqs.IsThisPlanUsesSameMT(allServersForm.planNameText.Text.Trim().ToString(), masterHost.hostname, WinTools.GetcxIPWithPortNumber()) == false)
                            {
                                MessageBox.Show("Select another planname for the protection", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                return false;
                            }
                        }
                        else
                        {
                            WinPreReqs win = new WinPreReqs("", "", "", "");
                            if (WinPreReqs.IsThisPlanUsesSameMT(allServersForm.existingPlanNameComboBox.SelectedItem.ToString(), masterHost.hostname, WinTools.GetcxIPWithPortNumber()) == false)
                            {
                                MessageBox.Show("Select another planname for the protection", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                return false;
                            }
                        }

                        // Writing the time zone and plan name for all the hosts int eh esx.xml.... comparing the displayname..
                        
                    }
                    foreach (XmlNode node in hostNodes)
                    {
                        foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                        {
                            if (h.displayname == node.Attributes["display_name"].Value)
                            {
                                Trace.WriteLine(DateTime.Now + "\t Compared successfully and display name are same ");
                                if (allServersForm.appNameSelected != AppName.Adddisks)
                                {
                                    node.Attributes["plan"].Value = planName;
                                    Trace.WriteLine(DateTime.Now + "\t Updated plan name ");
                                }
                                node.Attributes["timezone"].Value = h.timeZone;
                                node.Attributes["ip_address"].Value = h.ip;                               
                                node.Attributes["inmage_hostid"].Value = h.inmage_hostid;
                                node.Attributes["mbr_path"].Value = h.mbr_path;
                                node.Attributes["clusternodes_inmageguids"].Value = h.clusterInmageUUIds;
                                node.Attributes["cluster_name"].Value = h.clusterName;
                                node.Attributes["clusternodes_mbrfiles"].Value = h.clusterMBRFiles;
                                node.Attributes["cluster_nodes"].Value = h.clusterNodes;
                                Trace.WriteLine(DateTime.Now + "\t updated all values in host");
                                if (h.clusterMBRFiles != null)
                                {
                                    node.Attributes["clusternodes_mbrfiles"].Value = h.clusterMBRFiles;
                                }
                                node.Attributes["system_volume"].Value = h.system_volume;
                                Trace.WriteLine(DateTime.Now + "\t Printing the machine type " + h.machineType);
                                if (h.machineType.ToUpper() == "PHYSICALMACHINE")
                                {
                                    if (node.HasChildNodes)
                                    {
                                        foreach (XmlNode childs in node.ChildNodes)
                                        {
                                            try
                                            {
                                                if (childs.Name == "disk")
                                                {
                                                    XmlNodeList diskNodesOFVM = node.ChildNodes;
                                                    Trace.WriteLine(DateTime.Now + "\t printing the child names " + childs.Name);
                                                    foreach (XmlNode diskNode in diskNodesOFVM)
                                                    {
                                                        if (diskNode.Attributes["disk_signature"] != null && diskNode.Attributes["disk_signature"].Value.Length != 0)
                                                        {
                                                            
                                                                Trace.WriteLine(DateTime.Now + "\t Printing disk count " + h.disks.list.Count.ToString());
                                                                foreach (Hashtable hash in h.disks.list)
                                                                {
                                                                    Trace.WriteLine(DateTime.Now + "\t Comparing the disk_signature " + hash["disk_signature"].ToString() + "\t xml value is " + diskNode.Attributes["disk_signature"].Value);
                                                                    if (hash["disk_signature"].ToString() == diskNode.Attributes["disk_signature"].Value.ToString())
                                                                    {
                                                                        if (hash["Size"] != null && hash["Size"].ToString() != "0")
                                                                        {
                                                                            Trace.WriteLine(DateTime.Now + "\t updated size is " + hash["Size"].ToString());
                                                                            diskNode.Attributes["size"].Value = hash["Size"].ToString();
                                                                        }
                                                                        else
                                                                        {
                                                                            Trace.WriteLine(DateTime.Now + "\t Found disk size as null");
                                                                        }
                                                                        if (allServersForm.osTypeSelected == OStype.Windows)
                                                                        {
                                                                            if (hash["scsi_mapping_host"] != null)
                                                                            {
                                                                                Trace.WriteLine(DateTime.Now + "\t Updated disk mapping hosts " + hash["scsi_mapping_host"].ToString());
                                                                                diskNode.Attributes["scsi_mapping_host"].Value = hash["scsi_mapping_host"].ToString();
                                                                            }
                                                                            else
                                                                            {
                                                                                Trace.WriteLine(DateTime.Now + "\t Found NULL SCSI values");
                                                                            }
                                                                        }
                                                                        break;
                                                                    }


                                                                }
                                                            
                                                        }
                                                        else
                                                        {
                                                            if (diskNode.Attributes["disk_name"] != null && diskNode.Attributes["disk_name"].Value.Length != 0)
                                                            {
                                                                Trace.WriteLine(DateTime.Now + "\t Printing disk count " + h.disks.list.Count.ToString());
                                                                foreach (Hashtable hash in h.disks.list)
                                                                {
                                                                    Trace.WriteLine(DateTime.Now + "\t Comparing the names " + hash["Name"].ToString() + "\t xml value is " + diskNode.Attributes["disk_name"].Value);
                                                                    if (hash["Name"].ToString() == diskNode.Attributes["disk_name"].Value.ToString())
                                                                    {
                                                                        if (hash["Size"] != null && hash["Size"].ToString() != "0")
                                                                        {
                                                                            Trace.WriteLine(DateTime.Now + "\t updated size is " + hash["Size"].ToString());
                                                                            diskNode.Attributes["size"].Value = hash["Size"].ToString();
                                                                        }
                                                                        else
                                                                        {
                                                                            Trace.WriteLine(DateTime.Now + "\t Found disk size as null");
                                                                        }
                                                                        if (allServersForm.osTypeSelected == OStype.Windows)
                                                                        {
                                                                            if (hash["scsi_mapping_host"] != null)
                                                                            {
                                                                                Trace.WriteLine(DateTime.Now + "\t Updated disk mapping hosts " + hash["scsi_mapping_host"].ToString());
                                                                                diskNode.Attributes["scsi_mapping_host"].Value = hash["scsi_mapping_host"].ToString();
                                                                            }
                                                                            else
                                                                            {
                                                                                Trace.WriteLine(DateTime.Now + "\t Found NULL SCSI values");
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
                                                Trace.WriteLine(DateTime.Now + "\t Faield to update esx.xml with sizes " + ex.Message);
                                            }
                                        }
                                    }
                                }
                             
                            }
                        }
                        foreach (Host h in allServersForm.finalMasterListPrepared._hostList)
                        {
                            if (h.displayname == node.Attributes["display_name"].Value)
                            {
                                node.Attributes["inmage_hostid"].Value = h.inmage_hostid;
                            }
                        }
                     
                    }
                    // getting the plan name for the esx.xml 
                    // This is because to create a directory withe esx.xml for the protrection
                    // we are reading thsi from the root nodes because in case of addition of disk we didn't ask user to enter plan name....
                    string planname = null;
                    foreach (XmlNode node in rootNodes)
                    {
                        XmlAttributeCollection attribColl = node.Attributes;
                        if (node.Attributes["plan"] != null)
                        {
                            planname = attribColl.GetNamedItem("plan").Value.ToString();
                        }
                    }
                    // Creating the directory to save esx.xml file for the protection.....

                    if (allServersForm.appNameSelected == AppName.Bmr || allServersForm.appNameSelected == AppName.Esx)
                    {
                        _directoryNameinXmlFile = planname + "_" + masterHost.hostname + "_Protection" + "_" + randomNumber.ToString();
                    }
                    else if (allServersForm.appNameSelected == AppName.Adddisks)
                    {
                        _directoryNameinXmlFile = planname + "_" + masterHost.hostname + "_Add_Disk" + "_" + randomNumber.ToString();
                    }
                    else if(allServersForm.appNameSelected == AppName.Failback)
                    {
                        _directoryNameinXmlFile = planname + "_" + masterHost.hostname + "_FAILback" + "_" + randomNumber.ToString();
                    }
                    else if (allServersForm.appNameSelected == AppName.Offlinesync)
                    {
                        _directoryNameinXmlFile = planname + "_" + masterHost.hostname + "_Offlinesync" + "_" + randomNumber.ToString();
                    }
                    else
                    {
                        _directoryNameinXmlFile = planname + "_" + masterHost.hostname + "_" + randomNumber.ToString();
                    }
                    if (Directory.Exists(_fxFailOverDataPath + "\\" + _directoryNameinXmlFile))
                    {
                        randomNumber = randomlyGenerateNumber.Next(999999);
                        _directoryNameinXmlFile = planname + "_" + masterHost.hostname + "_" + randomNumber.ToString();
                    }
                    Trace.WriteLine(DateTime.Now + " \t printing foldername " + _directoryNameinXmlFile);
                    foreach (XmlNode node in rootNodes)
                    {
                        node.Attributes["xmlDirectoryName"].Value = _directoryNameinXmlFile;
                    }

                    // After modifying the esx.xml we are saving the esx.xml....
                    document.Save(xmlfile);

                    // Here  we are creating the directory usoing plan name, mt hostname and the random number.....
                    if (File.Exists(_latestFilePath + "\\ESX.xml"))
                    {
                        FileInfo destFile = new FileInfo(_fxFailOverDataPath + "\\" + _directoryNameinXmlFile + "\\ESX.xml");
                        if (destFile.Directory != null && !destFile.Directory.Exists)
                        {
                            destFile.Directory.Create();
                            //WinTools.RestrictPermissions(_fxFailOverDataPath + "\\" + _directoryNameinXmlFile);
                        }
                        File.Copy(_latestFilePath + "\\ESX.xml", destFile.ToString(), true);
                        WinTools win = new WinTools();
                        win.SetFilePermissions(_fxFailOverDataPath + "\\" + _directoryNameinXmlFile + "\\ESX.xml");
                        // FileInfo destFile1 = new FileInfo(Directory.GetCurrentDirectory() + "\\" + _directoryNameinXmlFile + "\\" + PHYSICAL_MACHINE_LIST_FILE);
                        if (allServersForm.appNameSelected == AppName.Bmr || (allServersForm.appNameSelected == AppName.Offlinesync && ResumeForm.selectedActionForPlan == "P2v"))
                        {
                            if (allServersForm.osTypeSelected == OStype.Windows)
                            {

                                if (Directory.Exists(_installPath + "\\Windows"))
                                {
                                    string directoryString = _installPath + "\\Windows";
                                    string[] directories = Directory.GetDirectories(directoryString);
                                    foreach (string directory in directories)
                                    {
                                        string[] path = directory.Split('\\');
                                        Trace.WriteLine("Printing path count " + path.Length.ToString());
                                        string directoryonly = null;
                                        foreach (string s in path)
                                        {
                                            Trace.WriteLine(s);
                                            directoryonly = s;
                                        }
                                        Trace.WriteLine("Printing path count " + directoryonly);
                                        try
                                        {
                                            //Directory.Move(directory, Directory.GetCurrentDirectory() + "\\" + _directoryNameinXmlFile + "\\" + directoryonly);
                                            Directory.CreateDirectory(_fxFailOverDataPath + "\\" + _directoryNameinXmlFile + "\\" + directoryonly);
                                            File.Copy(directory + "\\symmpi.sys", _fxFailOverDataPath + "\\" + _directoryNameinXmlFile + "\\" + directoryonly + "\\symmpi.sys");
                                        }
                                        catch (Exception e)
                                        {
                                            Trace.WriteLine(DateTime.Now + " \t Caught an exception for Creating the folder " + e.Message.ToString());
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                if (AllServersForm.SetFolderPermissions(_fxFailOverDataPath + "\\" + _directoryNameinXmlFile) == true)
                {
                    Trace.WriteLine(DateTime.Now + "\t Successfully set the file permission for the folder " + _fxFailOverDataPath + "\\"  + _directoryNameinXmlFile);
                }
                else
                {
                    Trace.WriteLine(DateTime.Now + "\t Failed to set folder permission for the folder ");
                }
                Process p = null;
                long waited = 0;
                int incWait = 100;
              
                Debug.WriteLine("Reached P2V_ProtectionPanelHandler  processPanel");
                _allServersForm = allServersForm;
                _esx = allServersForm.targetEsxInfoXml;
                //90 Seconds
                freezeUI(allServersForm);
                Debug.WriteLine("Came to generate vmx file " + _generatedVmxFiles);              
                allServersForm.nextButton.Enabled = false;
                allServersForm.preReqsbutton.Enabled = false;
                allServersForm.planNameTableLayoutPanel.Enabled = false;
                allServersForm.limitResyncCheckBox.Enabled = false;
                allServersForm.limitResyncTextBox.Enabled = false;
                if (_esxutilwinExecution == false)
                {
                    // Running the backgrounder worker asyncronusly.....
                    // Thsi is call the do work of the scriptBackgroundWorker 
                    // i.e PreScriptRun in the same file....

                   // allServersForm.scriptBackgroundWorker.RunWorkerAsync();
                    if (_cxInstallationPath != null)
                    {

                    }
                    masterHost.plan = allServersForm.planInput;
                    StatusForm status = new StatusForm(allServersForm.selectedSourceListByUser, masterHost, allServersForm.appNameSelected, _directoryNameinXmlFile);
                    allServersForm.closeFormForcelyByUser = true;
                    allServersForm.Close();
                    //status.ShowDialog();
                    // In case of addition of disks we are showing the below message in the log box...
                    //if (allServersForm._appName == AppName.ADDDISKS)
                    //{
                    //    if (_createGuestOnTargetSucceed == false)
                    //    {
                    //        allServersForm.protectionText.AppendText("Adding newly added disk to the VMs on secondary ESX server. \t Started:" + DateTime.Now + Environment.NewLine +
                    //         "This may take several minutes..." + Environment.NewLine);
                    //        //allServersForm.protectionText.AppendText
                    //    }
                    //}
                    //else
                    //{// In other cases we will display this message int eh log box.....
                    //    if (_createGuestOnTargetSucceed == false)
                    //    {
                    //        allServersForm.protectionText.AppendText(" Creating guest vms on secondary vSphere server may take few minutes. \t Started:" + DateTime.Now   + Environment.NewLine);                            allServersForm.protectionText.AppendText("Please wait till the process is completed" + Environment.NewLine);
                    //        if (_reUseVmdksString != null)
                    //        {
                    //            string message = "Note:Following Virtual Machines discovered on secondary vSphere server.Hence re-using existing vmdks." + Environment.NewLine +
                    //                             "     However CPU/Memory/NICs will be configured according to latest selection" + Environment.NewLine;
                    //            Trace.WriteLine(DateTime.Now+"\t\t"+message);
                    //            Trace.WriteLine(DateTime.Now+"\t\t"+_reUseVmdksString);
                    //            allServersForm.protectionText.AppendText(message);
                    //            allServersForm.protectionText.AppendText(_reUseVmdksString);
                    //            _reUseVmdksString = null;
                    //        }
                    //    }
                    //}
                }
                else
                {  
                    // This will call once the jobs are set and it failes to download or upload this 
                    //will be called when user clicks next.....    
                    allServersForm.postJobAutomationBackgroundWorker.RunWorkerAsync();
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


        private bool GenerateVmXFiles(AllServersForm allServersForm)
        {  //Downloading vmx files from corresponding esx box to current directory

            Debug.WriteLine("Entered to generate vmx files ");
            Esx _esxInfo = new Esx();
            //allServersForm._selectedSourceList.Print();         
            return true;
        }



       private bool updateLogTextBox(TextBox generalLogTextBox)
       {
           try
           {
               // This will update the log box when some scripts calls are failed.....
               StreamReader sr1 = new StreamReader(WinTools.FxAgentPath()+"\\vContinuum\\logs\\vContinuum_ESX.log");
               string firstLine1;
               firstLine1 = sr1.ReadToEnd();

               generalLogTextBox.AppendText(firstLine1);

               sr1.Close();
               sr1.Dispose();
           }
           catch (Exception e)
           {
               generalLogTextBox.AppendText("Some other process is accessing the file logs\\vContinuum_ESX.log");
               System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);
               Trace.WriteLine("ERROR*******************************************");
               Trace.WriteLine(DateTime.Now + "Caught exception at Method: updateLogTextBox " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
               Trace.WriteLine("Exception  =  " + e.Message);
               Trace.WriteLine("ERROR___________________________________________");               
               return false;
           }

           return true;

       }

       internal bool CopyFiletoDirectory(AllServersForm allServersForm)
       {
           try
           {
               SolutionConfig config = new SolutionConfig();
               HostList sourceList = new HostList();
               Host masterTempHost = new Host();

               string esxIP = ""; ;
               string cxIP = "";
               string planname = null;
               //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
               //based on above comment we have added xmlresolver as null
               XmlDocument document = new XmlDocument();
               document.XmlResolver = null;
               string xmlfile = _latestFilePath + "\\ESX.xml";

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
              // document.Load(xmlfile);
               //reader.Close();
               //XmlNodeList hostNodes = null;
               XmlNodeList hostNodes = null;
               XmlNodeList diskNodes = null;
               XmlNodeList rootNodes = null;
               rootNodes = document.GetElementsByTagName("root");
               hostNodes = document.GetElementsByTagName("host");
               diskNodes = document.GetElementsByTagName("disk");

               //reading esx.xml masternode and writing hostname of mastertaregt to sourcenode.
               if (allServersForm.appNameSelected == AppName.Esx || allServersForm.appNameSelected == AppName.Bmr || allServersForm.appNameSelected == AppName.Failback || allServersForm.appNameSelected == AppName.Adddisks || allServersForm.appNameSelected == AppName.Offlinesync)
               {
                   config.ReadXmlConfigFile("ESX.xml",   sourceList,  masterTempHost,  esxIP,  cxIP);
                   sourceList = SolutionConfig.list;
                   masterTempHost = SolutionConfig.masterHosts;
                   esxIP = SolutionConfig.EsxIP;
                   cxIP = SolutionConfig.csIP;
                   foreach (XmlNode node in rootNodes)
                   {
                       XmlAttributeCollection attribColl = node.Attributes;
                       if (node.Attributes["plan"] != null)
                       {
                           planname = attribColl.GetNamedItem("plan").Value.ToString();
                       }
                   }
                   foreach (XmlNode node in hostNodes)
                   {
                       node.Attributes["directoryName"].Value = _directoryNameinXmlFile;
                   }
               }
               document.Save(xmlfile);
               foreach (XmlNode node in diskNodes)
               {
                   node.Attributes["protected"].Value = "yes";
               }

               // In case of failback we will make the failback attribute values as yes.....
               if (allServersForm.appNameSelected == AppName.Failback)
               {
                   foreach (XmlNode node in hostNodes)
                   {
                       foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                       {
                           if (h.displayname == node.Attributes["display_name"].Value)
                           {
                               node.Attributes["failback"].Value = "yes";
                           }
                       }
                   }


               }
               document.Save(xmlfile);
               //Creating the directory for the parallel protection with plan and mt hostname + random number

               if (File.Exists(_latestFilePath + "\\ESX.xml"))
               {
                   File.Copy(_latestFilePath + "\\ESX.xml", _fxFailOverDataPath + "\\" + _directoryNameinXmlFile + "\\ESX.xml", true);
               }
           }
           catch (Exception ex)
           {
               System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
               Trace.WriteLine("ERROR*******************************************");
               Trace.WriteLine(DateTime.Now + "Caught exception at Method:   " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
               Trace.WriteLine("Exception  =  " + ex.Message);
               Trace.WriteLine("ERROR___________________________________________");
               return false;
           }
           return true;
       }

       internal bool PostJobAutomation(AllServersForm allServersForm)
        {
            // This will be called when fx jobs are set and we need to modifty the disks protection status as yes....
            // and for each host entery we will wirte master target hostname and displayname
            try
            {
                
                if (allServersForm.appNameSelected == AppName.Bmr || (allServersForm.appNameSelected == AppName.Offlinesync && ResumeForm.selectedActionForPlan == "P2v"))
                {
                   // _esx.UploadFile(masterHost.esxIp, masterHost.esxUserName, masterHost.esxPassword, PHYSICAL_MACHINE_LIST_FILE);
                }
                //Check for return code of the jobs and do following if jobs were successfull
                //Update MasterConfigFile.xml file

                //File.Copy(Directory.GetCurrentDirectory() + "\\ESX.xml", Directory.GetCurrentDirectory() + "\\" + MASTER_FILE, true);

                //esx.UploadFile(masterHost.esxIp, masterHost.esxUserName, masterHost.esxPassword, MASTER_FILE);

                // In case of offlinesync export we will prepare ESX_Master_Offlinesync.xml.......

                 
                    // Preparing MasterConfigFile.xml......
                    MasterConfigFile masterCOnfig = new MasterConfigFile();
                    if(masterCOnfig.UpdateMasterConfigFile(_latestFilePath + "\\ESX.xml"))                    
                    {
                        _postJobAutomationResult = true;
                    }
                    else
                    {
                        _postJobAutomationResult = false;
                        return false;
                    }                                   
                
            }
            catch ( Exception ex )
            {               
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:   " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            return true;
        }
        //This will called once the masterconfigfile.xml is uploadto cx......
       internal bool PostJobAutomationResultAction(AllServersForm allServersForm)
        {
            try
            {
                if (POST_JOBAUTOMATION == true)
                {
                    if (_postJobAutomationResult == true)
                    {


                        ResumeForm.breifLog.WriteLine(DateTime.Now + " \t Successfully completed updating the configuration files");
                        ResumeForm.breifLog.Flush();
                        if (_vconMT == false)
                        {
                            allServersForm.protectionText.AppendText("Go to http://" + WinTools.GetcxIPWithPortNumber() + "/ui  " + " and access Monitor-> " + allServersForm.planInput + Environment.NewLine);
                            allServersForm.protectionText.AppendText("Note:" + Environment.NewLine);
                            allServersForm.protectionText.AppendText("1. Make sure that vContinuum machine(local machine) is up and running till volume replication pairs are set." + Environment.NewLine);
                            //allServersForm.protectionText.AppendText("2. Please don't start any new protection/recovery operations till volume replication pairs are set." + Environment.NewLine + Environment.NewLine);
                        }
                        allServersForm.protectionText.AppendText("Completed protection" + Environment.NewLine);
                        // allServersForm.protectionText.AppendText("Preparing configuration of files may take few minutes" + Environment.NewLine);
                        //Uploading Physical_machineslist.txt file to secondary ESX Box   
                        allServersForm.doneButton.Visible = true;
                        allServersForm.doneButton.Enabled = true;
                        allServersForm.progressBar.Visible = false;
                        allServersForm.nextButton.Visible = false;
                        allServersForm.previousButton.Visible = false;
                        allServersForm.cancelButton.Visible = false;
                        try
                        {
                            if (File.Exists(_installPath + "\\logs\\vContinuum_UI.log"))
                            {
                                if (_directoryNameinXmlFile != null)
                                {
                                    Program.tr3.Dispose();
                                    System.IO.File.Move(WinTools.FxAgentPath() + "\\vContinuum\\logs\\vContinuum_UI.log", _fxFailOverDataPath + "\\" + _directoryNameinXmlFile + "\\vContinuum_UI.log");
                                    Program.tr3.Dispose();
                                }
                            }
                        }
                        catch (Exception ex)
                        {
                            Trace.WriteLine(DateTime.Now + "\t Failed to copy vCOntinuum_UI.log " + ex.Message);
                        }
                    }
                    else
                    {
                        allServersForm.progressBar.Visible = false;
                        allServersForm.nextButton.Visible = true;
                        allServersForm.nextButton.Enabled = true;
                        allServersForm.cancelButton.Visible = true;
                        allServersForm.cancelButton.Enabled = true;
                        allServersForm.doneButton.Visible = false;
                        allServersForm.protectionText.AppendText("Uploading Master file failed." + Environment.NewLine);
                        return false;
                    }
                }
                else
                {
                    allServersForm.progressBar.Visible = false;
                    allServersForm.nextButton.Visible = true;
                    allServersForm.nextButton.Enabled = true;
                    allServersForm.cancelButton.Visible = true;
                    allServersForm.cancelButton.Enabled = true;
                    allServersForm.doneButton.Visible = false;
                    allServersForm.protectionText.AppendText("Uploading Master file failed." + Environment.NewLine);
                    return false;                  
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:   " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            return true;
        }

       internal override bool ValidatePanel(AllServersForm allServersForm)
        {
            Debug.WriteLine("Reached P2V_ProtectionPanelHandler  ValidatePanel");
            return true;
        }

       internal bool RefreshHost(AllServersForm allServersForm, ref Host tempHost1)
        {
            try
            {
                Trace.WriteLine(DateTime.Now + "\t Refreshing hosts " + tempHost1.displayname);
                Cxapicalls cxApi = new Cxapicalls();
                if (cxApi.Post( tempHost1, "RefreshHostInfo") == true)
                {
                    tempHost1 = Cxapicalls.GetHost;
                    Trace.WriteLine(DateTime.Now + "\t Printing the request id" + tempHost1.requestId);
                    Trace.WriteLine(DateTime.Now + "\t Going to sleep for 65 sec for first time");
                    Thread.Sleep(65000);

                    int i = 1;
                    while (i < 10)
                    {
                        if (cxApi.Post(tempHost1, "GetRequestStatus") == true)
                        {
                            i = 11;
                            break;
                        }
                        else
                        {
                            i++;
                            Trace.WriteLine(DateTime.Now + "\t Going to sleep for 65 sec for time : " + i.ToString());
                            Thread.Sleep(65000);
                        }
                    
                    }
                   

                    tempHost1 = Cxapicalls.GetHost;
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:   " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            return true;
        }

       internal bool RefreshAllHostsAtATime(AllServersForm allServersForm)
        {
            try
            {
                Cxapicalls cxRefreshHostForAllHost = new Cxapicalls();
                foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                {
                    
                    string cxIp = WinTools.GetcxIPWithPortNumber();
                    if (h.inmage_hostid == null)
                    {
                        
                        Host tempHost = h;
                        WinPreReqs win = new WinPreReqs(h.ip, "", "", "");
                        if (h.hostname != null)
                        {
                            win.SetHostIPinputhostname(h.hostname,  h.ip, WinTools.GetcxIPWithPortNumber());
                            h.ip = WinPreReqs.GetIPaddress;
                        }
                        win.GetDetailsFromcxcli( tempHost, cxIp);
                        tempHost = WinPreReqs.GetHost;
                        h.inmage_hostid = tempHost.inmage_hostid;
                        if (h.inmage_hostid == null)
                        {
                            Trace.WriteLine(DateTime.Now + "\t Found inmage uuid is null for the machine " + h.displayname);
                        }
                    }
                }

                if (cxRefreshHostForAllHost.PostForRefreshAllHosts( allServersForm.selectedSourceListByUser) == true)
                {
                    allServersForm.selectedSourceListByUser = Cxapicalls.GetHostlist;
                    string requestId = null;
                    Host sourceHost = (Host)allServersForm.selectedSourceListByUser._hostList[0];
                    requestId = sourceHost.requestId;
                    Trace.WriteLine(DateTime.Now + "\t Going to sleep for 3 minutes after refreshing all the hosts");
                    Thread.Sleep(180000);
                    Host source = new Host();
                    source.requestId = requestId;
                    if (cxRefreshHostForAllHost.Post( source, "RefreshHostInfoStatus") == false)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Going to sleep for another minute(1) to get the status for all hosts");
                        Thread.Sleep(60000);
                        if (cxRefreshHostForAllHost.Post( source, "RefreshHostInfoStatus") == false)
                        {
                            Trace.WriteLine(DateTime.Now + "\t Going to sleep for another minute(2) to get the status for all hosts");
                            Thread.Sleep(60000);
                            if (cxRefreshHostForAllHost.Post( source, "RefreshHostInfoStatus") == false)
                            {
                                Trace.WriteLine(DateTime.Now + "\t Going to sleep for another minute(3) to get the status for all hosts");
                                Thread.Sleep(60000);
                                if (cxRefreshHostForAllHost.Post( source, "RefreshHostInfoStatus") == false)
                                {
                                    Trace.WriteLine(DateTime.Now + "\t Going to sleep for another minute(4) to get the status for all hosts");
                                    Thread.Sleep(60000);
                                    if (cxRefreshHostForAllHost.Post( source, "RefreshHostInfoStatus") == false)
                                    {
                                        Trace.WriteLine(DateTime.Now + "\t Going to sleep for another minute(5) to get the status for all hosts");
                                        Thread.Sleep(60000); if (cxRefreshHostForAllHost.Post( source, "RefreshHostInfoStatus") == false)
                                        {
                                            Trace.WriteLine(DateTime.Now + "\t Going to sleep for another minute(6) to get the status for all hosts");
                                            Thread.Sleep(60000);
                                            if (cxRefreshHostForAllHost.Post( source, "RefreshHostInfoStatus") == false)
                                            {
                                                Trace.WriteLine(DateTime.Now + "\t We have waited for 10 mins to get the refresh status of all hosts. Still we are getting pending status");
                                                Trace.WriteLine(DateTime.Now + "\t So we are continuing this step");
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    source = Cxapicalls.GetHost;

                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:   " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            return true;
        }

       internal bool PreReqCheck(AllServersForm allServersForm)
        {
            try
            {
                // When user clicks on the readiness check button in the last screen this method will be called......
                tickerDelegate = new TickerDelegate(SetLeftTicker);
                foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                {
                    h.vx_agent_heartbeat = false;
                    h.vxagentInstalled = false;
                    h.fx_agent_heartbeat = false;
                    h.fxagentInstalled = false;
                    if (h.efi == true)
                    {
                        // Efi is automated now.....
                       // _message = "Warning: Source server contains EFI Partition. After protection is done, you need to follow manual steps." + Environment.NewLine
                       // +"Please follow EFI vContinuum Solution document to copy EFI partition manually." + Environment.NewLine;
                        //_message = "EFI OS Installation detected, please consult the manual for additional steps required for protection"+Environment.NewLine;

                        //allServersForm.protectionText.Invoke(tickerDelegate, new object[] { _message });
                        //break;
                    }
                }

                foreach(Host h in allServersForm.finalMasterListPrepared._hostList)
                {
                    h.vx_agent_heartbeat = false;
                    h.vxagentInstalled = false;
                    h.fx_agent_heartbeat = false;
                    h.fxagentInstalled = false;
                }
                

                //allServersForm.protectionTabControl.BringToFront();
                _cxIPwithPortNumber = WinTools.GetcxIPWithPortNumber();
                //Step1: Prereq checks for secondary ESX host             
                if (_targetESXPreChekCompleted == false)
                {
                    _message = "Running readiness checks on secondary vSphere... \t Started:" + DateTime.Now + Environment.NewLine;
                    allServersForm.protectionText.Invoke(tickerDelegate, new object[] { _message });
                   
                    if (TargetReadynessCheck(allServersForm) == false)
                    {
                        _targetReadinessCheck = false;
                        Trace.Write(DateTime.Now + " \t  Came here because of readiness check is failed");
                        return false;
                    }
                    else
                    {
                        _targetReadinessCheck = true;
                        _targetESXPreChekCompleted = true;
                        _message = "Readiness checks completed on secondary vSphere... \t Completed:" + DateTime.Now + Environment.NewLine;
                        allServersForm.protectionText.Invoke(tickerDelegate, new object[] { _message });
                        
                    }
                    //  allServersForm.protectionText.AppendText("Readiness checks completed on secondary Esx... " + Environment.NewLine);
                }

                //Host selectedHost = (Host)allServersForm._selectedSourceList._hostList[0];
                //if (selectedHost.machineType.ToUpper() != "PHYSICALMACHINE" && selectedHost.os == OStype.Windows)
                //{
                //    RefreshAllHostsAtATime(allServersForm);
                //}
              

                if (allServersForm.appNameSelected == AppName.Esx)
                {
                    foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                    {
                        if (h.requestId != null)
                        {
                            Host tempHost1 = new Host();
                            tempHost1.requestId = h.requestId;
                            Trace.WriteLine(DateTime.Now + "\t Printing displayname " + h.displayname);
                            Cxapicalls cxApi = new Cxapicalls();
                            if (cxApi.Post( tempHost1, "GetRequestStatus") == false)
                            {
                                Trace.WriteLine(DateTime.Now + "\t Continuing with old MBR file means refresh host for mbr not completed in time ");
                            }
                            else
                            {
                                tempHost1 = Cxapicalls.GetHost;
                                Trace.WriteLine(DateTime.Now + "\t Got the new mbr from the host which is triggered in first screen ");

                            }
                            Trace.WriteLine(DateTime.Now + "\t Printing displayname " + h.displayname);
                        }
                        else
                        {
                            Trace.WriteLine(DateTime.Now + "\t It seems Request id is null ");
                        }
                    }
                }
                //From here we are checking for the source, master target, vCon and cx license and heartbeat.....
                Host h1 = new Host();
                //Step2: Prereq checks for source host list
                string cxIP = WinTools.GetcxIPWithPortNumber();
                foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                {
                    h1 = new Host();
                    h1 = h;
                    //skiping prereqs for the machines which are selected skipwmicheck as true by user.
                    if (h.ranPrecheck == false)
                    {
                        bool preReqPassed = true;
                        _message = "Running readiness checks for " + h.displayname + "(" + h.ip + ") ......" + Environment.NewLine;
                        allServersForm.protectionText.Invoke(tickerDelegate, new object[] { _message });
                        
                        WinPreReqs win = new WinPreReqs(h.ip, h.domain, h.userName, h.passWord);
                        int returnCode = 0;
                        if (h.hostname != null)
                        {

                            returnCode = win.SetHostIPinputhostname(h.hostname,  h.ip, WinTools.GetcxIPWithPortNumber());
                            h.ip = WinPreReqs.GetIPaddress;
                            //MessageBox.Show(h.ip);

                        }
                        if (returnCode == 0)
                        {
                            h.numberOfEntriesInCX = 1;
                        }
                        else if (returnCode == 1)
                        {
                            h.numberOfEntriesInCX = 2;
                        }
                        else if (returnCode == 2)
                        {
                            h.numberOfEntriesInCX = 0;
                        }
                        preReqPassed = CheckWinPreReqs(allServersForm, ref h1);
                        h.pointedToCx = h1.pointedToCx;
                        h.fxagentInstalled = h1.fxagentInstalled;
                        h.vxagentInstalled = h1.vxagentInstalled;
                        h.fxlicensed = h1.fxlicensed;
                        h.vxlicensed = h1.vxlicensed;
                        h.fx_agent_path = h1.fx_agent_path;
                        h.vx_agent_path = h1.vx_agent_path;
                        h.fx_agent_heartbeat = h1.fx_agent_heartbeat;
                        h.vx_agent_heartbeat = h1.vx_agent_heartbeat;
                        Host tempHost1 = new Host();
                        tempHost1.inmage_hostid = h1.inmage_hostid;
                        Cxapicalls cxApi = new Cxapicalls();
                        if (h.machineType.ToUpper() == "PHYSICALMACHINE" && h.os == OStype.Windows)
                        {
                            RefreshHost(allServersForm, ref tempHost1);
                            try
                            {
                                foreach (Hashtable hash in h.disks.list)
                                {
                                    foreach (Hashtable tempHash in tempHost1.disks.list)
                                    {
                                        if (hash["disk_signature"] != null && tempHash["disk_signature"] != null)
                                        {
                                            if (hash["disk_signature"].ToString() == tempHash["disk_signature"].ToString())
                                            {
                                                hash["scsi_mapping_host"] = tempHash["scsi_mapping_host"].ToString();
                                            }
                                        }
                                        else if (hash["src_devicename"] != null && tempHash["src_devicename"] != null && tempHash["scsi_mapping_host"] != null)
                                        {
                                            if (hash["src_devicename"].ToString() == tempHash["src_devicename"].ToString())
                                            {
                                                hash["scsi_mapping_host"] = tempHash["scsi_mapping_host"].ToString();
                                            }
                                        }
                                        else
                                        {
                                            if (hash["Name"] != null && tempHash["Name"] != null && tempHash["scsi_mapping_host"] != null)
                                            {
                                                if (hash["Name"].ToString() == tempHash["Name"].ToString() )
                                                {
                                                    hash["scsi_mapping_host"] = tempHash["scsi_mapping_host"].ToString();
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                            catch (Exception ex)
                            {
                                Trace.WriteLine(DateTime.Now + "\t Caught an exception in this case " + ex.Message);
                            }
                        }
                        else if (h.machineType.ToUpper() == "VIRTUALMACHINE" && h.cluster == "yes")
                        {
                            Trace.WriteLine(DateTime.Now + "\t Refreshing host becuase this is a cluster machine");
                            if (cxApi.PostRefreshWithmbrOnly(tempHost1) == true)
                            {
                                tempHost1 = Cxapicalls.GetHost;
                                _message = "Trying to refresh host to get new mbr value" + h.displayname + "(" + h.ip + ") ......" + Environment.NewLine;
                                allServersForm.protectionText.Invoke(tickerDelegate, new object[] { _message });
                                Thread.Sleep(65000);
                                if (cxApi.Post( tempHost1, "GetRequestStatus") == false)
                                {
                                    tempHost1 = Cxapicalls.GetHost;
                                    _message = "Trying to refresh host to get new mbr value" + h.displayname + "(" + h.ip + ") ......" + Environment.NewLine;
                                    allServersForm.protectionText.Invoke(tickerDelegate, new object[] { _message });
                                    Trace.WriteLine(DateTime.Now + "\t Failed to get the request status after 1 min");
                                    Thread.Sleep(65000);
                                    if (cxApi.Post( tempHost1, "GetRequestStatus") == false)
                                    {
                                        tempHost1 = Cxapicalls.GetHost;
                                        _message = "Trying to refresh host to get new mbr value" + h.displayname + "(" + h.ip + ") ......" + Environment.NewLine;
                                        allServersForm.protectionText.Invoke(tickerDelegate, new object[] { _message });
                                        Trace.WriteLine(DateTime.Now + "\t Failed to get the request status after 2 mins");
                                        Thread.Sleep(65000);
                                        if (cxApi.Post( tempHost1, "GetRequestStatus") == false)
                                        {
                                            tempHost1 = Cxapicalls.GetHost;
                                            Trace.WriteLine(DateTime.Now + "\t Failed to get the request status after 3 mins");
                                            Thread.Sleep(65000);
                                            if (cxApi.Post( tempHost1, "GetRequestStatus") == false)
                                            {
                                                tempHost1 = Cxapicalls.GetHost;
                                                Trace.WriteLine(DateTime.Now + "\t Failed to get the request status after 3 mins");
                                            }
                                            
                                        }
                                    }
                                }

                            }
                        }
                        //tempHost1 = Cxapicalls.GetHost;
                        if (cxApi.Post( tempHost1, "GetHostInfo") == true)
                        {
                            tempHost1 = Cxapicalls.GetHost;
                            int dynamicDiskCount = 0;
                            foreach (Hashtable newDisks in tempHost1.disks.list)
                            {
                                if (newDisks["disk_type"] != null)
                                {

                                    if (newDisks["disk_type"].ToString() == "dynamic")
                                    {
                                        dynamicDiskCount++;
                                    }
                                }
                                if (h.machineType.ToUpper() == "PHYSICALMACHINE")
                                {
                                    foreach (Hashtable hash in h.disks.list)
                                    {
                                       
                                        Trace.WriteLine(DateTime.Now + "\t Printing disknames original " + newDisks["Name"].ToString() + "\t splitted " + hash["Name"].ToString());
                                        if (hash["disk_signature"] != null && newDisks["disk_signature"] != null)
                                        {
                                            if (hash["disk_signature"].ToString() == newDisks["disk_signature"].ToString())
                                            {
                                                if (newDisks["Size"] != null)
                                                {
                                                    Trace.WriteLine(DateTime.Now + "\t Printing the sizes wmi " + hash["Size"].ToString() + " Cx " + newDisks["Size"].ToString());
                                                    hash["Size"] = newDisks["Size"];
                                                }
                                            }
                                        }
                                        else
                                        {
                                            if (hash["Name"].ToString() == newDisks["Name"].ToString())
                                            {
                                                if (newDisks["Size"] != null)
                                                {
                                                    Trace.WriteLine(DateTime.Now + "\t Printing the sizes wmi " + hash["Size"].ToString() + " Cx " + newDisks["Size"].ToString());
                                                    hash["Size"] = newDisks["Size"];
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                            int selectedDynamicCount = 0;
                            foreach (Hashtable selectedDisks in h.disks.list)
                            {
                                foreach (Hashtable newDisks in tempHost1.disks.GetDiskList)
                                {
                                    if (selectedDisks["Name"].ToString() == newDisks["Name"].ToString())
                                    {
                                        if (newDisks["disk_type"] != null)
                                        {
                                            if (newDisks["disk_type"].ToString() == "dynamic")
                                            {
                                                if (selectedDisks["Selected"].ToString() == "Yes" || selectedDisks["Protected"].ToString() == "yes")
                                                {
                                                    selectedDynamicCount++;
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                            if (h.machineType == "PhysicalMachine")
                            {
                                if (dynamicDiskCount != selectedDynamicCount && selectedDynamicCount != 0)
                                {
                                    MessageBox.Show("Machine " + h.displayname + " has more than one dynamic disk." + Environment.NewLine
                                        + " Please go back and select all dynamics disks for this machine.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                    return false;
                                }
                            }
                            //h.disks.partitionList = h1.disks.partitionList;
                            if (h.os == OStype.Windows)
                            {

                                h.mbr_path = tempHost1.mbr_path;
                                h.clusterMBRFiles = tempHost1.clusterMBRFiles;
                                if (h.mbr_path == null)
                                {

                                    _message = "Didn't found MBR path for " + h.displayname + "(" + h.ip + ") ......" + Environment.NewLine;
                                    allServersForm.protectionText.Invoke(tickerDelegate, new object[] { _message });
                                    tempHost1.operatingSystem = null;
                                    if (cxApi.PostRefreshWithmbrOnly(tempHost1) == true)
                                    {
                                        tempHost1 = Cxapicalls.GetHost;
                                        _message = "Trying to refresh host to get mbr value" + h.displayname + "(" + h.ip + ") ......" + Environment.NewLine;
                                        allServersForm.protectionText.Invoke(tickerDelegate, new object[] { _message });
                                        Thread.Sleep(65000);
                                        if (cxApi.Post( tempHost1, "GetRequestStatus") == false)
                                        {
                                            _message = "Trying to refresh host to get mbr value" + h.displayname + "(" + h.ip + ") ......" + Environment.NewLine;
                                            allServersForm.protectionText.Invoke(tickerDelegate, new object[] { _message });
                                            Trace.WriteLine(DateTime.Now + "\t Failed to get the request status after 1 min");
                                            Thread.Sleep(65000);
                                            if (cxApi.Post( tempHost1, "GetRequestStatus") == false)
                                            {
                                                _message = "Trying to refresh host to get mbr value" + h.displayname + "(" + h.ip + ") ......" + Environment.NewLine;
                                                allServersForm.protectionText.Invoke(tickerDelegate, new object[] { _message });
                                                Trace.WriteLine(DateTime.Now + "\t Failed to get the request status after 2 mins");
                                                Thread.Sleep(65000);
                                                if (cxApi.Post( tempHost1, "GetRequestStatus") == false)
                                                {
                                                    Trace.WriteLine(DateTime.Now + "\t Failed to get the request status after 3 mins");
                                                }
                                            }
                                        }
                                        tempHost1 = Cxapicalls.GetHost;

                                    }
                                    h.mbr_path = tempHost1.mbr_path;
                                    h.clusterMBRFiles = tempHost1.clusterMBRFiles;
                                    if (h.mbr_path == null)
                                    {

                                        _message = "Didn't found MBR path for " + h.displayname + "(" + h.ip + ") ......" + Environment.NewLine;
                                        allServersForm.protectionText.Invoke(tickerDelegate, new object[] { _message });
                                    }
                                }
                                Trace.WriteLine(DateTime.Now + "\t mbr path for the host: " + h.displayname + " is " + h.mbr_path);

                            }
                            string partitionVolume = null;
                            foreach (Hashtable partition in tempHost1.disks.partitionList)
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
                            else
                            {
                                Trace.WriteLine(DateTime.Now + "\t System volume got null for the CX database.");
                            }
                        }
                       
                        if (preReqPassed == false)
                        {
                            _message = "Readiness checks failed for " + h1.displayname + "(" + h.ip + ") ..." + Environment.NewLine;
                            allServersForm.protectionText.Invoke(tickerDelegate, new object[] { _message });
                           
                            //return false;
                        }
                        else
                        {
                            _message = "Readiness checks completed for " + h1.displayname + "(" + h.ip + ") ..." + Environment.NewLine;
                            allServersForm.protectionText.Invoke(tickerDelegate, new object[] { _message });
                            
                            //h.ranPrecheck = true;
                        }
                        
                    }
                }
                //Step3: Prereq checks for master target
                foreach (Host h in allServersForm.finalMasterListPrepared._hostList)
                {
                    h1 = new Host();
                    h1 = h;
                    if (h.ranPrecheck == false)
                    {
                        bool preReqPassed = true;
                        _message = "Running readiness checks for " + h.displayname + "(" + h.ip + ") ......" + Environment.NewLine;
                        allServersForm.protectionText.Invoke(tickerDelegate, new object[] { _message });
                        
                        WinPreReqs win = new WinPreReqs(h.ip, h.domain, h.userName, h.passWord);
                        int returnCode = 0;
                        if (h.hostname != null)
                        {

                            returnCode = win.SetHostIPinputhostname(h.hostname,  h.ip, WinTools.GetcxIPWithPortNumber());
                            h.ip = WinPreReqs.GetIPaddress;
                            //MessageBox.Show(h.ip);

                        }
                        if (returnCode == 0)
                        {
                            h.numberOfEntriesInCX = 1;
                        }
                        else if (returnCode == 1)
                        {
                            h.numberOfEntriesInCX = 2;
                        }
                        else if (returnCode == 2)
                        {
                            h.numberOfEntriesInCX = 0;
                        }
                        preReqPassed = CheckWinPreReqs(allServersForm, ref h1);
                        h.pointedToCx = h1.pointedToCx;
                        h.fxagentInstalled = h1.fxagentInstalled;
                        h.vxagentInstalled = h1.vxagentInstalled;
                        h.fxlicensed = h1.fxlicensed;
                        h.vxlicensed = h1.vxlicensed;
                        h.fx_agent_path = h1.fx_agent_path;
                        h.vx_agent_path = h1.vx_agent_path;
                        h.fx_agent_heartbeat = h1.fx_agent_heartbeat;
                        h.vx_agent_heartbeat = h1.vx_agent_heartbeat;
                        if (h.pointedToCx == false)
                        {
                            _runMTCheckOnce = true;
                        }
                        if (preReqPassed == false)
                        {
                            _message = "Readiness checks failed for " + h1.displayname + "(" + h.ip + ") ..." + Environment.NewLine;
                            allServersForm.protectionText.Invoke(tickerDelegate, new object[] { _message });
                            
                            //return false;
                        }
                        else
                        {
                            _message = "Readiness checks completed for " + h1.displayname + "(" + h.ip + ") ..." + Environment.NewLine;
                            allServersForm.protectionText.Invoke(tickerDelegate, new object[] { _message });                            
                            //h.ranPrecheck = true;
                        }                        
                    }
                }
                // ProgressReportOfPreReqsForm form = new ProgressReportOfPreReqsForm(allServersForm._selectedSourceList, allServersForm._finalMasterList);
                // form.ShowDialog();
               
                // allServersForm.checkTabPage.Show();
                //Prereq checks for Wizard/RCLI box
                 _tempHost = new Host();
                _tempHost.hostname = Environment.MachineName;
                _tempHost.displayname = Environment.MachineName;                
               
                WinPreReqs wp = new WinPreReqs("", "", "", "");
                string ipaddress = "";
                _message = "Running readiness checks for " + _tempHost.hostname + "(local host)" + Environment.NewLine;
                allServersForm.protectionText.Invoke(tickerDelegate, new object[] { _message }); 
                
                int returnCodeofTempHost = wp.SetHostIPinputhostname(_tempHost.hostname,  ipaddress, _cxIPwithPortNumber);
                ipaddress = WinPreReqs.GetIPaddress;
                if (returnCodeofTempHost == 0)
                {
                    _tempHost.ip = ipaddress;

                    _tempHost.numberOfEntriesInCX = 1;
                    ///checking vCon box fx licensed or not
                    if (wp.GetDetailsFromcxcli( _tempHost, _cxIPwithPortNumber) == true)
                    {
                        _tempHost = WinPreReqs.GetHost;
                        //_message = "Checking for agent license" ;
                        //allServersForm.protectionText.Invoke(tickerDelegate, new object[] { _message }); 
                            
                        
                        //if (_tempHost.fxlicensed == false)
                        //{
                        //    _message = "Error: Fx is not licensed for " + _tempHost.displayname + Environment.NewLine;
                        //    allServersForm.protectionText.Invoke(tickerDelegate, new object[] { _message }); 
                            
                        //    allServersForm.Refresh();
                        //}
                        //else
                        //{
                        //    _message = "\t\t  : PASSED" + Environment.NewLine;
                        //    allServersForm.protectionText.Invoke(tickerDelegate, new object[] { _message }); 
                            
                        //    _tempHost.ranPrecheck = true;
                        //}
                        if (_tempHost.fx_agent_heartbeat == false)
                        {
                            _message = "Agents are not reporting status to CX for last 15 mins. Please verify it on " + _tempHost.displayname + Environment.NewLine;
                            allServersForm.protectionText.Invoke(tickerDelegate, new object[] { _message }); 
                            
                        }
                    }
                }
                else if (returnCodeofTempHost == 1)
                {
                    _tempHost.numberOfEntriesInCX = 2;

                }
                else
                {
                    _tempHost.numberOfEntriesInCX = 0;
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


       internal bool PostReadinessCheck(AllServersForm allServersForm)
        {
            try
            {
               
                if (_targetReadinessCheck == false)
                {
                    try
                    {
                        updateLogTextBox(allServersForm.protectionText);
                        return false;
                    }
                    catch (Exception ex)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Failed to update log text box " + ex.Message);
                        return false;
                    }
                }
                // Checking for the ports of both cs and ps.
                allServersForm.checkReportDataGridView.Rows.Clear();
                
                
                allServersForm.checkReportDataGridView.Visible = true;
                _rowIndex = 0;
                foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                {
                    ReloadDataGridViewAfterRunReadinessCheck(allServersForm, h);
                    if (h.os == OStype.Windows)
                    {
                        if (h.mbr_path == null)
                        {
                            allServersForm.checkReportDataGridView.Rows.Add(1);
                            allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[HOSTNAME_COLUMN].Value = h.displayname;
                            allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[CHECK_COLUMN].Value = "MBR path";
                            allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[RESULT_COLUMN].Value = _failed;
                            allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[ACTION_COLUMN].Value = "MBR is not found on CS server for " + h.displayname+ ". Please check whether connectivity between source and CS server. Contact support for further information.";
                            _rowIndex++;
                        }                       
                        
                    }
                }
                foreach (Host h in allServersForm.finalMasterListPrepared._hostList)
                {
                    ReloadDataGridViewAfterRunReadinessCheck(allServersForm, h);
                }
                allServersForm.protectionTabControl.SelectTab(allServersForm.checkTabPage);

                if (_tempHost.numberOfEntriesInCX == 2)
                {
                    allServersForm.checkReportDataGridView.Rows.Add(1);
                    allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[HOSTNAME_COLUMN].Value = _tempHost.displayname;
                    allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[CHECK_COLUMN].Value = "Machine is pointing to cs";
                    allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[RESULT_COLUMN].Value = _failed;
                    allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[ACTION_COLUMN].Value = "Multiple entries found in CS database." + WinTools.GetcxIPWithPortNumber();
                    _rowIndex++;

                }
                else
                {
                    if ( _tempHost.fx_agent_heartbeat == true)
                    {
                        allServersForm.checkReportDataGridView.Rows.Add(1);
                        allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[HOSTNAME_COLUMN].Value = _tempHost.displayname;
                        allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[CHECK_COLUMN].Value = "All checks";
                        allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[RESULT_COLUMN].Value = _passed;
                        allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[ACTION_COLUMN].Value = "";
                        _rowIndex++;
                    }
                    else if (_tempHost.fxagentInstalled == false)
                    {
                        allServersForm.checkReportDataGridView.Rows.Add(1);
                        allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[HOSTNAME_COLUMN].Value = _tempHost.displayname;
                        allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[CHECK_COLUMN].Value = "Fx installed";
                        allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[RESULT_COLUMN].Value = _failed;
                        allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[ACTION_COLUMN].Value = "Install Fx agent";
                        _rowIndex++;
                        // return false;
                    }
                    else if (_tempHost.fx_agent_heartbeat == false)
                    {
                        allServersForm.checkReportDataGridView.Rows.Add(1);
                        allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[HOSTNAME_COLUMN].Value = _tempHost.displayname;
                        allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[CHECK_COLUMN].Value = "Fx running";
                        allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[RESULT_COLUMN].Value = _failed;
                        allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[ACTION_COLUMN].Value = "Fx service is not reported status to CS for more than 15 minutes.";
                        _rowIndex++;
                    }
                    else if (_tempHost.fxlicensed == false)
                    {
                        //allServersForm.checkReportDataGridView.Rows.Add(1);
                        //allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[HOSTNAME_COLUMN].Value = _tempHost.displayname;
                        //allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[CHECK_COLUMN].Value = "Fx licensed";
                        //allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[RESULT_COLUMN].Value = _failed;
                        //allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[ACTION_COLUMN].Value = "Add FX license in CX UI->Settings->LicenseManagement";
                        //_rowIndex++;
                    }
                }
                //Checking for cx 
                foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                {
                    if (h.efi == true)
                    {
                        // Now we have automated EFI Support
                        //Trace.WriteLine(DateTime.Now + "Found efi partation in " + h.displayname);
                        //allServersForm.checkReportDataGridView.Rows.Add(1);
                        //allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[HOSTNAME_COLUMN].Value = h.displayname;
                        //allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[CHECK_COLUMN].Value = "EFI";
                        //allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[RESULT_COLUMN].Value = _warning;
                        //allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[ACTION_COLUMN].Value = "EFI OS Installation detected, please consult the manual for additional steps required for protection";
                        //_rowIndex++;
                    }
                    else
                    {
                        Trace.WriteLine(DateTime.Now + "Does notFound efi partation in " + h.displayname);
                    }
                }
                if (CheckPortsForCSAndPS(allServersForm) == false)
                {
                    return false;
                }
                foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                {
                    if (h.numberOfEntriesInCX == 2)
                    {
                        allServersForm.planNameLabel.Visible = false;
                        allServersForm.planNameText.Visible = false;
                        allServersForm.nextButton.Enabled = false;
                        return false;
                    }
                    if (allServersForm.osTypeSelected == OStype.Windows)
                    {
                        if (h.pointedToCx == false ||  h.vxagentInstalled == false || h.fxagentInstalled == false || h.vx_agent_heartbeat == false || h.fx_agent_heartbeat == false || h.mbr_path == null)
                        {
                            allServersForm.planNameLabel.Visible = false;
                            allServersForm.planNameText.Visible = false;
                            allServersForm.nextButton.Enabled = false;
                            return false;
                        }
                    }
                    else if (allServersForm.osTypeSelected == OStype.Linux || allServersForm.osTypeSelected == OStype.Solaris)
                    {
                        if (h.pointedToCx == false ||  h.vxagentInstalled == false || h.fxagentInstalled == false || h.vx_agent_heartbeat == false || h.fx_agent_heartbeat == false)
                        {
                            allServersForm.planNameLabel.Visible = false;
                            allServersForm.planNameText.Visible = false;
                            allServersForm.nextButton.Enabled = false;
                            return false;
                        }
                    }
                }
                foreach (Host h in allServersForm.finalMasterListPrepared._hostList)
                {
                    if (h.numberOfEntriesInCX == 2)
                    {
                        allServersForm.planNameLabel.Visible = false;
                        allServersForm.planNameText.Visible = false;
                        allServersForm.nextButton.Enabled = false;
                        return false;
                    }
                    if (h.pointedToCx == false || h.vxagentInstalled == false || h.fxagentInstalled == false || h.vx_agent_heartbeat == false || h.fx_agent_heartbeat == false)
                    {
                        allServersForm.planNameLabel.Visible = false;
                        allServersForm.planNameText.Visible = false;
                        allServersForm.nextButton.Enabled = false;
                        return false;
                    }
                }
                if ( _tempHost.fx_agent_heartbeat == false || _tempHost.fxagentInstalled == false)
                {
                    if (_tempHost.numberOfEntriesInCX == 2)
                    {
                        allServersForm.planNameLabel.Visible = false;
                        allServersForm.planNameText.Visible = false;
                        allServersForm.nextButton.Enabled = false;
                        return false;
                    }
                    allServersForm.planNameLabel.Visible = false;
                    allServersForm.planNameText.Visible = false;
                    allServersForm.nextButton.Enabled = false;
                    return false;
                }
                
               

                WinPreReqs winPreReqs = new WinPreReqs("", "", "", "");
                _planNameList = new ArrayList();
                allServersForm.existingPlanNameComboBox.Items.Clear();
                winPreReqs.GetPlanNames( _planNameList, WinTools.GetcxIPWithPortNumber());
                _planNameList = WinPreReqs.GetPlanlist;
                if (_planNameList.Count != 0)
                {
                    foreach (string s in _planNameList)
                    {
                        allServersForm.existingPlanNameComboBox.Items.Add(s);
                        _hasPlanName = true;
                        allServersForm.existingPlanNameComboBox.Visible = true;
                        allServersForm.existingPlanNameLabel.Visible = true;

                        allServersForm.mandatorydoubleFieldLabel.Visible = true;
                        allServersForm.mandatoryLabel.Visible = true;
                        allServersForm.planNameLabel.Text = "Create New Plan **";
                        allServersForm.existingPlanNameLabel.Text = "Existing Plan Name **";
                    }
                }
                else
                {
                    allServersForm.existingPlanNameLabel.Visible = false;
                    allServersForm.existingPlanNameComboBox.Visible = false;
                    allServersForm.mandatorydoubleFieldLabel.Visible = false;
                    allServersForm.planNameLabel.Text = "Create New Plan *";
                    allServersForm.mandatoryLabel.Visible = true;
                    _hasPlanName = false;
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

       internal bool ReloadDataGridViewAfterRunReadinessCheck(AllServersForm allServersForm, Host h)
        {
            try
            {
                if (h.ip == "127.0.0.1")
                {
                    allServersForm.checkReportDataGridView.Rows.Add(1);
                    allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[HOSTNAME_COLUMN].Value = h.displayname;
                    allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[CHECK_COLUMN].Value = "IP:127.0.0.1";
                    allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[RESULT_COLUMN].Value = _warning;
                    allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[ACTION_COLUMN].Value = "Found 127.0.0.1 ip for this vm. Pairs won't progress in this case.";
                    _rowIndex++;
                }
                if (h.numberOfEntriesInCX == 2)
                {
                    allServersForm.checkReportDataGridView.Rows.Add(1);
                    allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[HOSTNAME_COLUMN].Value = h.displayname;
                    allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[CHECK_COLUMN].Value = "Machine is pointing to cx";
                    allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[RESULT_COLUMN].Value = _failed;
                    allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[ACTION_COLUMN].Value = "Multiple entries found in CX database." + WinTools.GetcxIPWithPortNumber();
                    _rowIndex++;
                    return false;

                }
                
                // Once all the licenses verification is done we will fill in to thus datagridview....... whether they are passed or failed....
                if (h.pointedToCx == true &&  h.vxagentInstalled == true && h.fxagentInstalled == true && h.fx_agent_heartbeat == true && h.vx_agent_heartbeat == true )
                {
                    allServersForm.checkReportDataGridView.Rows.Add(1);
                    allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[HOSTNAME_COLUMN].Value = h.displayname;
                    allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[CHECK_COLUMN].Value = "All checks";
                    allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[RESULT_COLUMN].Value = _passed;
                    allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[ACTION_COLUMN].Value = " ";
                    _rowIndex++;
                    return true;

                }
               
                if (h.pointedToCx == false)
                {
                    allServersForm.checkReportDataGridView.Rows.Add(1);
                    allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[HOSTNAME_COLUMN].Value = h.displayname;
                    allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[CHECK_COLUMN].Value = "Machine is pointing to cx";
                    allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[RESULT_COLUMN].Value = _failed;
                    allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[ACTION_COLUMN].Value = "Either unified agent is not installed or agent is not pointing to CX IP:" + WinTools.GetcxIPWithPortNumber();
                    _rowIndex++;
                    return false;
                }
                if (h.vxagentInstalled == false)
                {
                    allServersForm.checkReportDataGridView.Rows.Add(1);
                    allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[HOSTNAME_COLUMN].Value = h.displayname;
                    allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[CHECK_COLUMN].Value = "VX Installed";
                    allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[RESULT_COLUMN].Value = _failed;
                    allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[ACTION_COLUMN].Value = "Install Unified agents";
                    _rowIndex++;

                }
                if (h.fxagentInstalled == false)
                {
                    allServersForm.checkReportDataGridView.Rows.Add(1);
                    allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[HOSTNAME_COLUMN].Value = h.displayname;
                    allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[CHECK_COLUMN].Value = "FX Installed";
                    allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[RESULT_COLUMN].Value = _failed;
                    allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[ACTION_COLUMN].Value = "Install Unified agents";
                    _rowIndex++;

                }
                //if (h.fxlicensed == false && h.fxagentInstalled == true)
                //{
                //    allServersForm.checkReportDataGridView.Rows.Add(1);
                //    allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[HOSTNAME_COLUMN].Value = h.displayname;
                //    allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[CHECK_COLUMN].Value = "FX License";
                //    allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[RESULT_COLUMN].Value = _failed;
                //    allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[ACTION_COLUMN].Value = "Add FX license in CX UI->Settings->LicenseManagement";
                //    _rowIndex++;

                //}
                //if (h.vxlicensed == false && h.vxagentInstalled == true)
                //{
                //    allServersForm.checkReportDataGridView.Rows.Add(1);
                //    allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[HOSTNAME_COLUMN].Value = h.displayname;
                //    allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[CHECK_COLUMN].Value = "VX License";
                //    allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[RESULT_COLUMN].Value = _failed;
                //    allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[ACTION_COLUMN].Value = "Add Vx license in  CX UI->Settings->LicenseManagement";
                //    _rowIndex++;

                //}

                if (h.fx_agent_heartbeat == false && h.fxagentInstalled == true)
                {
                    allServersForm.checkReportDataGridView.Rows.Add(1);
                    allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[HOSTNAME_COLUMN].Value = h.displayname;
                    allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[CHECK_COLUMN].Value = "Fx running";
                    allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[RESULT_COLUMN].Value = _failed;
                    allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[ACTION_COLUMN].Value = "Fx service is not reported status to CX for more than 15 minutes.";
                    _rowIndex++;
                }
                if (h.vx_agent_heartbeat == false && h.vxagentInstalled == true)
                {
                    allServersForm.checkReportDataGridView.Rows.Add(1);
                    allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[HOSTNAME_COLUMN].Value = h.displayname;
                    allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[CHECK_COLUMN].Value = "Vx running";
                    allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[RESULT_COLUMN].Value = _failed;
                    allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[ACTION_COLUMN].Value = "Vx service is not reported status to CX for more than 15 minutes.";
                    _rowIndex++;

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

       internal bool CheckWinPreReqs(AllServersForm allServersForm, ref Host h)
        {
            try
            {
                // Getting the information form the cx to check the license and heart beat of the source, master target , vcon and cx.....
                Host h1 = new Host();
                bool preReqPassed = true;
                string errorMessage = "";
                WmiCode errorCode = WmiCode.Unknown;
              /*  if (WinPreReqs.IpReachable(h.ip) == false)
                {
                    allServersForm.protectionText.AppendText("Error: Cant reach " + h.ip + Environment.NewLine);
                    preReqPassed = false;
                    return false;
                }*/
                WinPreReqs win = new WinPreReqs(h.ip, h.domain, h.userName, h.passWord);
                h.fx_agent_heartbeat = false;
                h.vx_agent_heartbeat = false;
                if (win.GetDetailsFromcxcli(h, _cxIPwithPortNumber) == true)
                {
                    h = WinPreReqs.GetHost;
                    return true;
                }
                else
                {
                    h = WinPreReqs.GetHost;
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
            
        }


       internal override bool CanGoToNextPanel(AllServersForm allServersForm)
        {
            return true;

        }
       internal override bool CanGoToPreviousPanel(AllServersForm allServersForm)
        {
            try
            {
                //While going back to the previous screen we need to change the button text as next from the protect......
                Trace.WriteLine(DateTime.Now + " \t Entered the CanGoToPreviousPanel");
                allServersForm.helpContentLabel.Text = HelpForcx.ProtectScreen4;
                if (allServersForm.SourceToMasterDataGridView.RowCount > 0)
                {
                    allServersForm.nextButton.Enabled = true;
                }
                int index = 0;              
                allServersForm.nextButton.Text = "Next >";
                allServersForm.mandatoryLabel.Visible = false;
                if(allServersForm.appNameSelected == AppName.Failback)
                {
                    allServersForm.mandatoryLabel.Visible = true;
                    allServersForm.mandatorydoubleFieldLabel.Visible = true;
                }

                if (allServersForm.appNameSelected == AppName.Adddisks)
                {
                    allServersForm.mandatoryLabel.Visible = true;
                    allServersForm.mandatorydoubleFieldLabel.Visible = true;
                }
                else
                {
                    allServersForm.mandatorydoubleFieldLabel.Visible = false;
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
            Trace.WriteLine(DateTime.Now + " \t Exiting the CanGoToPreviousPanel");
            return true;
        }

       internal bool RunningScriptInLinuxMt(AllServersForm allServersFrom, Host masterHost)
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


       internal bool PreScriptRun(AllServersForm allServersForm)
        {
            Trace.WriteLine(DateTime.Now + " \t Entered: PreScriptRun: of Esx_ProtectionPanelHandler.cs");
            try
            {
                tickerDelegate = new TickerDelegate(SetLeftTicker);
                //check for the newly added scsi id fix that 
                //UI will call the script to create disks and scrips will appen a node to the targetesx
                //UI need to check the existence of that node and need to use wmi call to fill port id.
                for (int k = 0; k < allServersForm.finalMasterListPrepared._hostList.Count; k++)
                {
                    masterHost = (Host)allServersForm.finalMasterListPrepared._hostList[k];
                    MASTER_FILE = "ESX_Master_" + masterHost.esxIp + ".xml";
                }



                if (_dummyDiskCreation == false)
                {
                    // This metod will create dummy disks add that will attach disk to the mt.

                    if (masterHost.os == OStype.Linux)
                    {
                        masterHost.requestId = null;
                        RunningScriptInLinuxMt(allServersForm, masterHost);
                    }
                    if (CreateDummydisk(allServersForm, masterHost) == false)
                    {
                       
                        return false;
                    }
                    

                }          

                          
                // This will be called to create the vms on the target side.....
                if (_createGuestOnTargetSucceed == false)
                {

                    if (CreateVMs(allServersForm, masterHost) == false)
                    {
                        _createGuestOnTargetSucceed = false;
                        return false;
                    }
                    else
                    {
                        _message = " Creating VM(s) on target server completed successfully. \t Completed:" + DateTime.Now+ Environment.NewLine;
                        allServersForm.protectionText.Invoke(tickerDelegate, new object[] { _message });
                    }

                }

                // Check every second


                // once the creation of source vm on target side is compled we iwll sleep for 30 seconds and called the folder_delete.bat
                // to delete the folders created by the scrips...
                string vconmt = "no";
                if (masterHost.hostname.Split('.')[0].ToUpper() != Environment.MachineName.ToUpper())
                {
                    vconmt = "no";
                    _vconMT = false;
                }
                else
                {
                    vconmt = "yes";
                    _vconMT = true;
                }
               
                if (_createGuestOnTargetSucceed == true && _jobAutomationSucceed == false)
                {
                    if (SetFxJobs(allServersForm, masterHost, vconmt) == false)
                    {
                        _jobAutomationSucceed = false;
                        return false;
                    }
                    else
                    {
                        if (vconmt == "no")
                        {
                            _message =  "Setting up jobs completed sucessfully. \t Completed:"+ DateTime.Now + Environment.NewLine;
                            allServersForm.protectionText.Invoke(tickerDelegate, new object[] { _message });
                        }
                        else
                        {
                            _message = "Setting up consistency completed sucessfully. \t Completed:"+ DateTime.Now + Environment.NewLine;
                            allServersForm.protectionText.Invoke(tickerDelegate, new object[] { _message });
                        }
                    }
                   
                }
                if (_jobAutomationSucceed == true)
                {
                    CopyFiletoDirectory(allServersForm);
                   
                    // Preparing input.txt file                  

                }
                if (_jobAutomationSucceed == true && vconmt == "yes")
                {
                    if (UploadFileAndPrePareInputText(allServersForm) == true)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Successfully made input.txt file and uploaded all required file to cxps");

                    }
                    else
                    {
                        Trace.WriteLine(DateTime.Now + "\t Failed to made input.txt and failed to upload required files to cxps");
                        return false;
                    }
                    int returncode = 0;
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
                    string path = WinTools.FxAgentPath() + "\\Failover\\Data\\" + _directoryNameinXmlFile;
                    _message =  "Creating volume replication pairs. \t Started:" + DateTime.Now + Environment.NewLine;
                    allServersForm.protectionText.Invoke(tickerDelegate, new object[] { _message });
                    if (allServersForm.appNameSelected == AppName.Failback)
                    {
                        returncode = WinTools.ExecuteEsxUtilWinLocally(100000, "failback", "\"" +  path + "\"" , path + "\\protection.log" );
                        //returncode = _esx.ExecuteEsxutilWin("failback", WinTools.FxAgentPath() + "\\Failover\\Data\\" + _directoryNameinXmlFile);
                    }
                    else
                    {
                        returncode = WinTools.ExecuteEsxUtilWinLocally(100000, "target", "\"" + path + "\"",  path + "\\protection.log");
                        //returncode = _esx.ExecuteEsxutilWin("target", WinTools.FxAgentPath() + "\\Failover\\Data\\" + _directoryNameinXmlFile);
                    }

                    if (returncode == 0)
                    {
                        _message = "Setting up volume replication pairs completed sucessfully. \t Completed:" + DateTime.Now + Environment.NewLine;
                        allServersForm.protectionText.Invoke(tickerDelegate, new object[] { _message });
                        _esxutilwinExecution = true;
                    }
                    else
                    {
                        _message =  "Setting up volume replication pairs got failed. Check EsxUtilWin.log for exact error" + Environment.NewLine
                                    +"Path of log. " + WinTools.FxAgentPath() + "\\EsxUtlWin.log" + "Failed at " + DateTime.Now  + Environment.NewLine;
                        allServersForm.protectionText.Invoke(tickerDelegate, new object[] { _message });
                        _esxutilwinExecution = false;
                    }
                }
                else
                {
                    _esxutilwinExecution = true;
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



       internal bool UploadFileAndPrePareInputText(AllServersForm allServersForm)
        {
            ArrayList fileList = new ArrayList();
            try
            {
                if (File.Exists(_installPath + "\\Latest\\input.txt"))
                {
                    File.Delete(_installPath + "\\Latest\\input.txt");
                }
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed to delete input.txt file form latest folder" + ex.Message);
            }
            try
            {
                FileInfo file = new FileInfo(_installPath + "\\Latest\\input.txt");
                StreamWriter writer = file.AppendText();
                writer.WriteLine("ESX.xml");
                writer.WriteLine("Inmage_scsi_unit_disks.txt");
                fileList.Add("ESX.xml");
                fileList.Add("Inmage_scsi_unit_disks.txt");
                if (allServersForm.appNameSelected == AppName.Bmr || (allServersForm.appNameSelected == AppName.Offlinesync && ResumeForm.selectedActionForPlan == "P2v"))
                {
                    if (allServersForm.osTypeSelected == OStype.Windows)
                    {
                        if (Directory.Exists(_fxFailOverDataPath + "\\" + _directoryNameinXmlFile + "\\Windows_2003_32"))
                        {
                            if (File.Exists(_fxFailOverDataPath + "\\" + _directoryNameinXmlFile + "\\Windows_2003_32\\symmpi.sys"))
                            {
                                writer.WriteLine("Windows_2003_32/symmpi.sys");
                                fileList.Add(_fxFailOverDataPath + "\\" + _directoryNameinXmlFile + "\\Windows_2003_32\\symmpi.sys");
                            }
                        } if (Directory.Exists(_fxFailOverDataPath + "\\" + _directoryNameinXmlFile + "\\Windows_2003_64"))
                        {
                            if (File.Exists(_fxFailOverDataPath + "\\" + _directoryNameinXmlFile + "\\Windows_2003_64\\symmpi.sys"))
                            {
                                writer.WriteLine("Windows_2003_64/symmpi.sys");
                                fileList.Add(_fxFailOverDataPath + "\\" + _directoryNameinXmlFile + "\\Windows_2003_64\\symmpi.sys");
                            }
                        }
                    }
                }
                writer.Close();
                writer.Dispose();
                if (File.Exists(_installPath + "\\Latest\\input.txt"))
                {
                    File.Copy(_installPath + "\\Latest\\input.txt", _fxFailOverDataPath + "\\" + _directoryNameinXmlFile + "\\input.txt", true);
                }
                foreach (string filename in fileList)
                {
                    if (WinTools.UploadFileToCX("\"" + filename + "\"", "\"" + "/usr/local/InMage/Vx/failover_data/" + _directoryNameinXmlFile + "\"") == 0)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Successfully upload the file to Cx using cxpsclient.exe " + filename);
                    }
                    else
                    {
                        Trace.WriteLine(DateTime.Now + "\t Failed to  upload the file to Cx using cxpsclient.exe " + filename);
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


       internal bool CreateDummydisk(AllServersForm allServersForm, Host masterHost)
        {
            if (_esx.ExecuteDummyDisksScript(masterHost.esxIp) == 0)
            {
                if (masterHost.os == OStype.Linux)
                {
                    masterHost.requestId = null;
                    RunningScriptInLinuxMt(allServersForm, masterHost);
                }
                try
                {
                    //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
                    //based on above comment we have added xmlresolver as null
                    XmlDocument document = new XmlDocument();
                    document.XmlResolver = null;
                    string xmlfile = _latestFilePath + "\\ESX.xml";


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
                   // document.Load(xmlfile);
                   // reader.Close();
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
                            Host h = new Host();
                            h.inmage_hostid = masterHost.inmage_hostid;
                            Trace.WriteLine(DateTime.Now + "\t Printing the mt host id " + h.inmage_hostid);
                            h.ip = masterHost.ip;
                            h.userName = masterHost.userName;
                            h.passWord = masterHost.passWord;
                            h.domain = masterHost.domain;
                            h.hostname = masterHost.hostname;

                            Cxapicalls cxApi = new Cxapicalls();
                            if (cxApi.Post( h, "RefreshHostInfo") == true)
                            {
                                h = Cxapicalls.GetHost;
                                Trace.WriteLine(DateTime.Now + "\t Printing the request id " + h.requestId);
                                Trace.WriteLine(DateTime.Now + "\t Going to sleep for 1 min  first time");
                                Thread.Sleep(65000);

                                if (cxApi.Post( h, "GetRequestStatus") == false)
                                {
                                    Trace.WriteLine(DateTime.Now + "\t Going to sleep for 1 min  second time");
                                    Thread.Sleep(65000);
                                    if (cxApi.Post( h, "GetRequestStatus") == false)
                                    {
                                        Trace.WriteLine(DateTime.Now + "\t Going to sleep for 1 min  third time");
                                        Thread.Sleep(65000);
                                        if (cxApi.Post( h, "GetRequestStatus") == false)
                                        {
                                            Trace.WriteLine(DateTime.Now + "\t Going to sleep for 1 min  fourth time");
                                            Thread.Sleep(65000);
                                            if (cxApi.Post( h, "GetRequestStatus") == false)
                                            {
                                                Trace.WriteLine(DateTime.Now + "\t Going to sleep for 1 min  fifth time");
                                                Thread.Sleep(65000);
                                                if (cxApi.Post( h, "GetRequestStatus") == false)
                                                {

                                                }
                                            }
                                        }

                                    }
                                }
                                h = Cxapicalls.GetHost;
                                ArrayList physicalDisks;
                                physicalDisks = h.disks.GetDiskList;
                                foreach (XmlNode node in mtdiskNodes)
                                {
                                    foreach (Hashtable disk in physicalDisks)
                                    {
                                        Trace.WriteLine(DateTime.Now + " \t comparing both the sizes " + node.Attributes["size"].Value.ToString() + " " + disk["ActualSize"].ToString());
                                        if (node.Attributes["size"].Value.ToString() == disk["ActualSize"].ToString())
                                        {
                                            if (allServersForm.osTypeSelected == OStype.Linux || allServersForm.osTypeSelected == OStype.Solaris)
                                            {
                                                if (disk["ScsiBusNumber"] != null)
                                                {
                                                    Trace.WriteLine(DateTime.Now + "Printing ScsiBusNumber values " + disk["ScsiBusNumber"].ToString());
                                                    node.Attributes["port_number"].Value = disk["ScsiBusNumber"].ToString();
                                                }
                                            }
                                            else if(allServersForm.osTypeSelected == OStype.Windows)
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
                                _dummyDiskCreation = true;
                                document.Save(xmlfile);

                            }

                        }
                        else
                        {
                            Trace.WriteLine(DateTime.Now + "\t There are no dummy disks present in esx.xml");
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
            else
            {
                _dummyDiskCreation = false;
                return false;
            }

            return true;
        }


       internal bool CreateVMs(AllServersForm allServersForm, Host masterHost)
        {
            try
            {// allServersForm.protectionText.Update();
                if (allServersForm.appNameSelected == AppName.Failback)
                {
                    Trace.WriteLine(DateTime.Now + " \t Entered to create guest vms for failback on target vSphere sever");
                    //allServersForm.protectionText.AppendText("Creating guest vms on secondary ESX server" + Environment.NewLine +
                    // "This may take several minutes..." + Environment.NewLine);
                    if (_esx.ExecuteCreateTargetGuestScript(masterHost.esxIp, OperationType.Failback) == 0)
                    {
                        Trace.WriteLine(DateTime.Now + " \t Successfully  created guest vms on target vSphere sever");
                        _createGuestOnTargetSucceed = true;
                        Thread.Sleep(5000);

                    }
                    else
                    {
                        Trace.WriteLine(DateTime.Now + " \t Failed to  create guest vms on target vSphere sever");
                        return false;
                    }

                }

                else if (allServersForm.appNameSelected != AppName.Adddisks && allServersForm.appNameSelected != AppName.Offlinesync && allServersForm.appNameSelected != AppName.Failback)
                {
                    Trace.WriteLine(DateTime.Now + " \t Entered to create guest vms on target vSphere sever");

                    if (_esx.ExecuteCreateTargetGuestScript(masterHost.esxIp,  OperationType.Initialprotection) == 0)
                    {
                        Trace.WriteLine(DateTime.Now + " \t Successfully  created guest vms on target vSphere sever");
                        _createGuestOnTargetSucceed = true;
                        Thread.Sleep(5000);

                    }
                    else
                    {
                        Trace.WriteLine(DateTime.Now + " \t Failed to  create guest vms on target vSphere sever");
                        return false;
                    }
                }

                else if (allServersForm.appNameSelected == AppName.Adddisks)
                {
                    //this is for incremental disk.
                    Trace.WriteLine(DateTime.Now + " \t Entered to create guest vms for addition of disk on target vSphere sever");
                    if (_esx.ExecuteCreateTargetGuestScript(masterHost.esxIp, OperationType.Additionofdisk) == 0)
                    {
                        Trace.WriteLine(DateTime.Now + " \t Successfully  created guest vms on target vSphere sever");
                        _createGuestOnTargetSucceed = true;
                        Thread.Sleep(5000);
                    }
                    else
                    {
                        Trace.WriteLine(DateTime.Now + " \t Failed to  create guest vms on target vSphere sever");
                        return false;
                    }
                }
                else if (allServersForm.appNameSelected == AppName.Offlinesync)
                {
                    Trace.WriteLine(DateTime.Now + " \t Entered to create guest vms for Offline sync on target vSphere sever");
                    if (_esx.ExecuteCreateTargetGuestScript(masterHost.esxIp, OperationType.Offlinesync) == 0)
                    {
                        Trace.WriteLine(DateTime.Now + " \t Successfully  created guest vms on target vSphere sever");
                        _createGuestOnTargetSucceed = true;
                        Thread.Sleep(5000);
                    }
                    else
                    {
                        Trace.WriteLine(DateTime.Now + " \t Failed to  create guest vms on target vSphere sever");
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

            return true;
        }


       internal bool SetFxJobs(AllServersForm allServersForm, Host masterHost, string vconmt)
        {

            // once the delete of the folders are done we will call the script to set fx jobs......
            try
            {
                if (allServersForm.appNameSelected == AppName.Failback)
                {
                    Trace.WriteLine(DateTime.Now + " \t Entered to create fx jobs for the failback protection");
                    if (_esx.ExecuteJobAutomationScript(OperationType.Failback, vconmt) == 0)
                    {
                        ResumeForm.breifLog.WriteLine(DateTime.Now + " \t Successfully completed by setting jobs to the above selected vms with plan name " + planName);
                        ResumeForm.breifLog.Flush();
                        Trace.WriteLine(DateTime.Now + " \t Successfully created fx jobs for the protection");
                        _jobAutomationSucceed = true;
                    }
                    else
                    {
                        Trace.WriteLine(DateTime.Now + " \t Failed to create fx jobs for the protection");
                        return false;
                    }
                }
                else if (allServersForm.appNameSelected != AppName.Adddisks && allServersForm.appNameSelected != AppName.Failback)
                {
                    //Writing into the vContinuum_breif.log.......... 
                    ResumeForm.breifLog.WriteLine(DateTime.Now + "\t Fx jobs are set for the following vms ");
                    ResumeForm.breifLog.Flush();
                    foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                    {
                        ResumeForm.breifLog.WriteLine(DateTime.Now + " \t " + h.displayname + " \t mt " + h.masterTargetDisplayName + " \t target esx ip " + h.targetESXIp + "\t source esx ip " + h.esxIp + " \t plan name  " + planName);
                    }
                    ResumeForm.breifLog.Flush();
                    Trace.WriteLine(DateTime.Now + " \t Entered to create fx jobs for the protection");

                    if (_esx.ExecuteJobAutomationScript(OperationType.Initialprotection, vconmt) == 0)
                    {
                        Trace.WriteLine(DateTime.Now + " \t Successfully created fx jobs for the protection");
                        _jobAutomationSucceed = true;
                    }
                    else
                    {
                        Trace.WriteLine(DateTime.Now + " \t Failed to create fx jobs for the protection");
                        return false;
                    }
                }
                else if (allServersForm.appNameSelected == AppName.Adddisks)
                {
                    Trace.WriteLine(DateTime.Now + " \t Entered to create fx jobs for the addition of disk protection");
                    if (_esx.ExecuteJobAutomationScript(OperationType.Additionofdisk, vconmt) == 0)
                    {
                        Trace.WriteLine(DateTime.Now + " \t Successfully created fx jobs for the protection");
                        _jobAutomationSucceed = true;
                    }
                    else
                    {
                        Trace.WriteLine(DateTime.Now + " \t Failed to create fx jobs for the protection");
                        return false;
                    }
                    //Writing into the vContinuum_breif.log.......... 
                    ResumeForm.breifLog.WriteLine(DateTime.Now + " \t Successfully completed by setting jobs to the above selected vms");
                    ResumeForm.breifLog.Flush();
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

       internal bool PostScriptRunAfetrJobAutomation(AllServersForm allServersForm)
        {
            try
            {
                // Once the jobs are set it will called this function and then we will check all the calls went fine or not.....
                allServersForm.nextButton.Enabled = true;
                for (int k = 0; k < allServersForm.finalMasterListPrepared._hostList.Count; k++)
                {
                    masterHost = (Host)allServersForm.finalMasterListPrepared._hostList[k];
                    MASTER_FILE = "ESX_Master_" + masterHost.esxIp + ".xml";
                }
                string vconmt = "no";
                if (masterHost.hostname.Split('.')[0].ToUpper() != Environment.MachineName.ToUpper())
                {
                    vconmt = "no";
                }
                else
                {
                    vconmt = "yes";
                }
                if (_esxutilwinExecution == true )
                {
                    if (vconmt == "no")
                    {
                        //allServersForm.doneButton.Visible = true;
                        
                        //allServersForm.previousButton.Visible = false;
                        //allServersForm.cancelButton.Visible = false;

                        allServersForm.protectionText.AppendText(Environment.NewLine + "Added FX jobs to create protection." + Environment.NewLine);
                        allServersForm.protectionText.AppendText("Volume replication pairs will be set after file replication jobs are completed." + Environment.NewLine);
                    }
                    else
                    {
                        allServersForm.protectionText.AppendText("Volume replication pairs are set" + Environment.NewLine);
                    }
                    allServersForm.nextButton.Enabled = false;
                    allServersForm.postJobAutomationBackgroundWorker.RunWorkerAsync();


                }
                else
                {
                    if (_dummyDiskCreation == false)
                    {
                        allServersForm.reviewDataGridView.Enabled = true;
                        allServersForm.progressBar.Visible = false;
                        updateLogTextBox(allServersForm.protectionText);
                        allServersForm.protectionText.AppendText("Creating dummy disks failed" + Environment.NewLine);
                        return false;
                    }
                    if (allServersForm.appNameSelected == AppName.Bmr || (allServersForm.appNameSelected == AppName.Adddisks && ResumeForm.selectedActionForPlan == "BMR Protection"))
                    {

                    }
                    else
                    {
                        /*if (_generatedVmxFiles == false)
                        {
                            updateLogTextBox(allServersForm.protectionText);
                            allServersForm.protectionText.AppendText("Generating vmx file failed" + Environment.NewLine);
                            Trace.WriteLine("Generating vmx file failed " + DateTime.Now);
                            allServersForm.reviewDataGridView.Enabled = true;
                            allServersForm.progressBar.Visible = false;
                            return false;
                        }*/
                    }
                    if (_createGuestOnTargetSucceed == false)
                    {
                        allServersForm.reviewDataGridView.Enabled = true;
                        updateLogTextBox(allServersForm.protectionText);
                        if (AllServersForm.SuppressMsgBoxes == false)
                        {
                            MessageBox.Show("01_020_001: Could not create guest VMs on the secondary vSphere server.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        }

                        allServersForm.progressBar.Visible = false;
                        return false;
                    }                    
                    if (_jobAutomationSucceed == false)
                    {
                        allServersForm.reviewDataGridView.Enabled = false;
                        updateLogTextBox(allServersForm.protectionText);
                        if (AllServersForm.SuppressMsgBoxes == false)
                        {
                            MessageBox.Show("01_020_002: Could not create Fx jobs on CX ", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        }                        
                        allServersForm.progressBar.Visible = false;
                        return false;
                    }
                    if (_esxutilwinExecution == false)
                    {
                        MessageBox.Show("Failed to set volume replication pairs", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        allServersForm.reviewDataGridView.Enabled = false;
                        allServersForm.progressBar.Visible = false;
                        return false;
                    }
                }
                //unFreezeUI(allServersForm);
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
       internal bool UnselectMachine(AllServersForm allServersForm, int index)
        {
            try
            {
                // If user unseles any machine form the table it will call this method to delete that machine form
                // the table,esx.xml and from the datastructures....
                // we need to remove from all the screens...

                Host masterTargetHost = (Host)allServersForm.finalMasterListPrepared._hostList[0];
                Host h = new Host();
                h.displayname = allServersForm.reviewDataGridView.Rows[index].Cells[SOURCE_COLUMN].Value.ToString();
                h.hostname = allServersForm.reviewDataGridView.Rows[index].Cells[SOURCE_HOSTNAME_COLUMN].Value.ToString();
                h.ip = allServersForm.reviewDataGridView.Rows[index].Cells[SOURCEIP_ADDRESS_COLUMN].Value.ToString();
                //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
                //based on above comment we have added xmlresolver as null
                XmlDocument documentEsx = new XmlDocument();
                documentEsx.XmlResolver = null;
                string esxXmlFile = _latestFilePath + "\\ESX.xml";


                //StreamReader reader = new StreamReader(esxXmlFile);
                //string xmlString = reader.ReadToEnd();
                XmlReaderSettings settings = new XmlReaderSettings();
                settings.ProhibitDtd = true;
                settings.XmlResolver = null;
                //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
                using (XmlReader reader1 = XmlReader.Create(esxXmlFile, settings))
                {
                    documentEsx.Load(reader1);
                    //reader1.Close();
                }
                //documentEsx.Load(esxXmlFile);
                //reader.Close();
                XmlNodeList hostNodesEsxXml = null;
                hostNodesEsxXml = documentEsx.GetElementsByTagName("host");
                foreach (XmlNode esxnode in hostNodesEsxXml)
                {//searchine for the node in the xml file and then deleting that node form xml.
                    Trace.WriteLine(DateTime.Now + " \t printing xml value of displayname  " + esxnode.Attributes["display_name"].Value.ToString() + "\t" + h.displayname + "\t hostname " + esxnode.Attributes["hostname"].Value.ToString() +"\t" + h.hostname);
                    if (esxnode.Attributes["display_name"].Value.ToString() == h.displayname && esxnode.Attributes["hostname"].Value.ToString() == h.hostname)
                    {
                        
                        if (esxnode.ParentNode.Name == "SRC_ESX")
                        {
                            XmlNode parentNode = esxnode.ParentNode;
                            esxnode.ParentNode.RemoveChild(esxnode);
                            break;
                        }
                    }
                }
                documentEsx.Save(esxXmlFile);
                allServersForm.reviewDataGridView.Rows.RemoveAt(index);
                int sourceIndex = 0;
                if (allServersForm.selectedSourceListByUser.DoesHostExist(h, ref sourceIndex))
                {// cheking the machine in the sourcelist......
                    h = (Host)allServersForm.selectedSourceListByUser._hostList[sourceIndex];
                }
                if (h.targetDataStore != null)
                {//reassingin the values of disk to the datastore....
                    if (allServersForm.ignoreDatastoreSizeCalculationCheckBox.Checked == false)
                    {
                        foreach (DataStore d in masterTargetHost.dataStoreList)
                        {
                            if (d.name == h.targetDataStore && d.vSpherehostname == masterTargetHost.vSpherehost)
                            {
                                d.freeSpace = d.freeSpace + h.totalSizeOfDisks;
                                h.targetDataStore = null;
                            }
                        }
                        allServersForm.selectedMasterListbyUser.AddOrReplaceHost(masterTargetHost);
                    }
                }

                if (allServersForm.appNameSelected == AppName.Esx || (allServersForm.appNameSelected == AppName.Offlinesync && ResumeForm.selectedActionForPlan == "Esx"))
                { //checking the datagridview values of the first screen and then un checking that machine in the datagridview...
                    for (int i = 0; i < allServersForm.addSourceTreeView.Nodes.Count; i++)
                    {
                        for (int j = 0; j < allServersForm.addSourceTreeView.Nodes[i].Nodes.Count; j++)
                        {
                            if (allServersForm.addSourceTreeView.Nodes[i].Nodes[j].Text == h.displayname && allServersForm.addSourceTreeView.Nodes[i].Text == h.vSpherehost)
                            {
                                allServersForm.addSourceTreeView.Nodes[i].Nodes[j].Checked = false;
                              
                            }
                        }
                    }
                }

                if (allServersForm.appNameSelected == AppName.Esx || allServersForm.appNameSelected == AppName.Bmr)
                {
                    if (allServersForm.sourceConfigurationTreeView.Nodes.Count != 0)
                    {
                        for (int i = 0; i < allServersForm.sourceConfigurationTreeView.Nodes[0].Nodes.Count; i++)
                        {
                            if (allServersForm.sourceConfigurationTreeView.Nodes[0].Nodes[i].Text == h.displayname)
                            {
                                try
                                {                                   
                                    allServersForm.sourceConfigurationTreeView.Nodes[0].Nodes[i].Remove();
                                }
                                catch (Exception ex)
                                {
                                    Trace.WriteLine(DateTime.Now + "\t Failed to remove the node " + ex.Message);
                                }
                            }
                        }
                    }
                }
                 if(allServersForm.appNameSelected == AppName.Bmr || (allServersForm.appNameSelected == AppName.Offlinesync && ResumeForm.selectedActionForPlan == "P2v"))
                {//checking the datagridview values of the first screen and then deleting that machine in the datagridview...for p2v case
                    
                }
                for (int i = 0; i < allServersForm.SourceToMasterDataGridView.RowCount; i++)
                {//Removing the row in sourcetomasterdatagridview......
                    if (allServersForm.appNameSelected == AppName.Esx || allServersForm.appNameSelected == AppName.Failback || (allServersForm.appNameSelected == AppName.Offlinesync && ResumeForm.selectedActionForPlan == "Esx") || (allServersForm.appNameSelected == AppName.Adddisks && ResumeForm.selectedActionForPlan == "ESX"))
                    {
                        if (h.displayname == allServersForm.SourceToMasterDataGridView.Rows[i].Cells[Esx_SourceToMasterPanelHandler.SOURCE_SERVER_IP].Value.ToString())
                        {
                            Trace.WriteLine(DateTime.Now + " \t Came here to delete row in sourceto master target grid " + i.ToString());
                            allServersForm.SourceToMasterDataGridView.Rows.RemoveAt(i);
                            allServersForm.SourceToMasterDataGridView.RefreshEdit();
                            Trace.WriteLine(DateTime.Now + " \t Came here to delete row in sourceto master  target grid completed " + i.ToString());
                        }
                    }
                    else
                    {
                        if (h.ip == allServersForm.SourceToMasterDataGridView.Rows[i].Cells[Esx_SourceToMasterPanelHandler.SOURCE_SERVER_IP].Value.ToString())
                        {
                            Trace.WriteLine(DateTime.Now + " \t Came here to delete row in sourceto master target grid " + i.ToString());
                            allServersForm.SourceToMasterDataGridView.Rows.RemoveAt(i);
                            allServersForm.SourceToMasterDataGridView.RefreshEdit();
                            Trace.WriteLine(DateTime.Now + " \t Came here to delete row in sourceto master  target grid completed " + i.ToString());
                        }
                    }
                }
                 //checking the datagridview values of the push agent screen and then deleting the row in the datagridview...
                for (int i = 0; i < allServersForm.pushAgentDataGridView.RowCount; i++)
                {
                    if (h.displayname == allServersForm.pushAgentDataGridView.Rows[i].Cells[PushAgentPanelHandler.DISPLAY_NAME_COLUMN].Value.ToString())
                    {
                        Trace.WriteLine(DateTime.Now + " \t Came here to delete row in pushagent target grid " + i.ToString());
                        allServersForm.pushAgentDataGridView.Rows.RemoveAt(i);
                        allServersForm.pushAgentDataGridView.RefreshEdit();
                    }
                }
                if (allServersForm.appNameSelected == AppName.Failback)
                {//checking thevalues in second screen and removing the row from the table.
                    /*for (int i = 0; i < allServersForm.addSourcePanelEsxDataGridView.RowCount; i++)
                    {
                        if (h.displayname == allServersForm.addSourcePanelEsxDataGridView.Rows[i].Cells[ESX_PrimaryServerPanelHandler.DISPLAY_NAME_COLUMN].Value.ToString())
                        {
                            allServersForm.addSourcePanelEsxDataGridView.Rows.RemoveAt(i);
                            allServersForm.addSourcePanelEsxDataGridView.RefreshEdit();
                        }
                    }*/
                    //checking thevalues in first screen and unchecking the machine 
                    for(int i =0; i<allServersForm.detailsOfAdditionOfDiskDataGridView.RowCount; i++)
                    {
                        if (h.displayname == allServersForm.detailsOfAdditionOfDiskDataGridView.Rows[i].Cells[AdditionOfDiskSelectMachinePanelHandler.HostnameColumn].Value.ToString())
                        {
                            allServersForm.detailsOfAdditionOfDiskDataGridView.Rows[i].Cells[AdditionOfDiskSelectMachinePanelHandler.AdddiskColumn].Value = false;
                            allServersForm.detailsOfAdditionOfDiskDataGridView.RefreshEdit();
                        }
                    }
                }
                if (allServersForm.appNameSelected == AppName.Adddisks)
                {//checking thevalues in first screen and unchecking the machine 
                    for (int i = 0; i < allServersForm.detailsOfAdditionOfDiskDataGridView.RowCount; i++)
                    {
                        if (h.displayname == allServersForm.detailsOfAdditionOfDiskDataGridView.Rows[i].Cells[AdditionOfDiskSelectMachinePanelHandler.HostnameColumn].Value.ToString())
                        {
                            allServersForm.detailsOfAdditionOfDiskDataGridView.Rows[i].Cells[AdditionOfDiskSelectMachinePanelHandler.AdddiskColumn].Value = false;
                            allServersForm.detailsOfAdditionOfDiskDataGridView.RefreshEdit();
                        }
                    }
                    //checking thevalues in second screen and removing the row from the table.
                    for (int i = 0; i < allServersForm.addDiskDetailsDataGridView.RowCount; i++)
                    {
                        if (h.displayname == allServersForm.addDiskDetailsDataGridView.Rows[i].Cells[AddDiskPanelHandler.SourceDisplaynameColumn].Value.ToString())
                        {
                            allServersForm.addDiskDetailsDataGridView.Rows.RemoveAt(i);
                            allServersForm.addDiskDetailsDataGridView.RefreshEdit();
                        }
                    }
                    AdditionOfDiskSelectMachinePanelHandler.temperorySelectedSourceList.RemoveHost(h);
                    allServersForm.newDiskDetailsDataGridView.Rows.Clear();
                    allServersForm.newDiskDetailsDataGridView.Visible = false;
                }
                allServersForm.selectedSourceListByUser.RemoveHost(h);
                if (allServersForm.selectedSourceListByUser._hostList.Count == 0)
                {
                    MessageBox.Show("There are no machines to protect.", "Information", MessageBoxButtons.OK, MessageBoxIcon.Information);
                    allServersForm.nextButton.Enabled = false;
                    allServersForm.preReqsbutton.Enabled = false;
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

       internal bool CheckPortsForCSAndPS(AllServersForm allServersForm)
        {
            try
            {
                bool checksResult = true;
                //bool required9443 = false;
                //bool required9080 = false;

                //foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                //{
                //    if(h.secure_ps_to_tgt == true || h.secure_src_to_ps == true)
                //    {
                //        required9443 = true;
                //    }
                //    if(h.secure_ps_to_tgt == false || h.secure_src_to_ps == false)
                //    {
                //        required9080 = true;
                //    }
                //}
                string cxIP = WinTools.GetcxIP();
                if (WinTools.Https == false)
                {
                    allServersForm.protectionText.AppendText("Checking for port 9080 for CS(" + cxIP + ")");
                    if (RemoteWin.CheckPort(cxIP, 9080) == true)
                    {
                        allServersForm.protectionText.AppendText(":Passed" + Environment.NewLine);
                    }
                    else
                    {
                        checksResult = false;
                        allServersForm.checkReportDataGridView.Rows.Add(1);
                        allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[HOSTNAME_COLUMN].Value = cxIP + " (CS)";
                        allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[CHECK_COLUMN].Value = "Port 9080";
                        allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[RESULT_COLUMN].Value = _failed;
                        allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[ACTION_COLUMN].Value = "Please open port number 9080 for CS";
                        _rowIndex++;
                        allServersForm.protectionText.AppendText(":Failed" + Environment.NewLine);
                    }
                }
                if (WinTools.Https == true)
                {
                    allServersForm.protectionText.AppendText("Checking for port 9443 for CS(" + cxIP + ")");
                    if (RemoteWin.CheckPort(cxIP, 9443) == true)
                    {
                        allServersForm.protectionText.AppendText(":Passed" + Environment.NewLine);
                    }
                    else
                    {
                        checksResult = false;
                        allServersForm.checkReportDataGridView.Rows.Add(1);
                        allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[HOSTNAME_COLUMN].Value = cxIP + " (CS)";
                        allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[CHECK_COLUMN].Value = "Port 9443";
                        allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[RESULT_COLUMN].Value = _failed;
                        allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[ACTION_COLUMN].Value = "Please open port number 9443 for CS";
                        _rowIndex++;
                        allServersForm.protectionText.AppendText(":Closed" + Environment.NewLine);
                    }
                }

                //ArrayList processServerList = new ArrayList();
                //processServerList.Add(cxIP);
                //foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                //{
                //    bool exists = false;
                //    if (cxIP != h.processServerIp)
                //    {
                //        if (processServerList.Count != 0)
                //        {
                //            foreach (string psIp in processServerList)
                //            {
                //                if (psIp == h.processServerIp)
                //                {
                //                    exists = true;
                //                }
                //            }
                //            if (exists == false)
                //            {
                //                processServerList.Add(h.processServerIp);
                //            }
                //        }
                //        else
                //        {
                //            processServerList.Add(h.processServerIp);
                //        }
                //    }

                //}
                //if (processServerList.Count != 0)
                //{
                //    foreach (string ip in processServerList)
                //    {
                //        if (required9080 == true)
                //        {
                //            if (cxIP == ip && WinTools.Https == false)
                //            {
                //                Trace.WriteLine(DateTime.Now + "\t Both cs and ps ips are same and already 9080 is verfied for cs");
                //            }
                //            else 
                //            {
                //                allServersForm.protectionText.AppendText("Checking for port 9080 for PS(" + ip + ")");
                //                if (RemoteWin.CheckPort(ip, 9080) == true)
                //                {
                //                    allServersForm.protectionText.AppendText(":Passed" + Environment.NewLine);
                //                }
                //                else
                //                {
                //                    checksResult = false;
                //                    allServersForm.checkReportDataGridView.Rows.Add(1);
                //                    allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[HOSTNAME_COLUMN].Value = ip + " (PS)";
                //                    allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[CHECK_COLUMN].Value = "Port 9080";
                //                    allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[RESULT_COLUMN].Value = _failed;
                //                    allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[ACTION_COLUMN].Value = "Please open port number 9080 for PS";
                //                    _rowIndex++;

                //                    allServersForm.protectionText.AppendText(":Closed" + Environment.NewLine);
                //                }
                //            }
                //        }
                //        if (required9443 == true)
                //        {
                //            if (cxIP == ip && WinTools.Https == true)
                //            {
                //                Trace.WriteLine(DateTime.Now + "\t Both cs and ps ips are same and already 9443 is verfied for cs");

                //            }
                //            else
                //            {
                //                allServersForm.protectionText.AppendText("Checking for port 9443 for PS(" + ip + ")");
                //                if (RemoteWin.CheckPort(ip, 9443) == true)
                //                {
                //                    allServersForm.protectionText.AppendText(":Passed" + Environment.NewLine);
                //                }
                //                else
                //                {
                //                    checksResult = false;
                //                    allServersForm.protectionText.AppendText(":Closed" + Environment.NewLine);
                //                    allServersForm.checkReportDataGridView.Rows.Add(1);
                //                    allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[HOSTNAME_COLUMN].Value = ip + " (PS)";
                //                    allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[CHECK_COLUMN].Value = "Port 9443";
                //                    allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[RESULT_COLUMN].Value = _failed;
                //                    allServersForm.checkReportDataGridView.Rows[_rowIndex].Cells[ACTION_COLUMN].Value = "Please open port number 9443 for PS";
                //                    _rowIndex++;
                //                }
                //            }
                //        }

                //    }
                //}
                return checksResult;
                
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

    }
}

