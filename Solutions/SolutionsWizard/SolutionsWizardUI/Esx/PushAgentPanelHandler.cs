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

namespace Com.Inmage.Wizard
{
    class PushAgentPanelHandler : PanelHandler
    {
        public static long TIME_OUT_MILLISECONDS = 360000;
        public static int DISPLAY_NAME_COLUMN = 0;
        public static int IP_COLUMN = 1;
        public static int AGENT_STATUS_COLUMN = 2;
        public static int PUSHAGENT_COLUMN = 5;
        public static int VERSION_COLUMN = 3;
        public static int ADVANCED_COLUMN = 4;
        //public static int PUSH_AS_TARGET = 4;
       // public static int REBOOT_COLUMN = 4;
      //  public static int USE_NAT_IP_COLUMN = 4;
       // public static int NAT_IP_COLUMN = 5;
        public long MAX_WAIT_TIME = 60000;
        public string UNIFIED_AGENT_NAME = "UnifiedAgent.exe";
        public int _credentialIndex;
        public bool _wmiErrorCheckCanceled = false;
        public static bool _credentialsCheckPassed = false;
        Host _selectedHost = new Host();
        public static bool _isAgentsInstalled = false;
        string _installPath = WinTools.FxAgentPath() + "\\vContinuum";
        public bool _alreadyHaveNatIp = false;
        public bool _backGroundWorkerIsCompleted = false;
        public bool _selectedCredentialsForOneShot = false;
        public static string _message = null;
        private delegate void TickerDelegate(string s);
        TickerDelegate tickerDelegate;
        TextBox _textBox = new TextBox();
        public bool _installAgents = false;
        public bool _skipAgents = false;
        public int _totalVmsSelectedForPush = 0;
        public int _totalVmsCompletedSuccessfully = 0;
        public int _totalFailed = 0;
        Label _label = new Label();
        public bool _agentUpgrade = false;
        public string _upgradedVMSList = null;
        public PushAgentPanelHandler()
        {
        }

        internal override bool Initialize(AllServersForm allServersForm)
        {
            try
            {
                // making the bool variables to false when we enters to this screen..

                _agentUpgrade = false;
                _upgradedVMSList = null;
                _selectedCredentialsForOneShot = false;
                _backGroundWorkerIsCompleted = false;
                allServersForm.pushAgentCheckBox.Checked = false;
                _textBox = allServersForm.generalLogTextBox;
                Trace.WriteLine(DateTime.Now + " \t Entered: Initialize of PushAgentPanelHandler");
                allServersForm.generalLogTextBox.Visible = true;
                allServersForm.helpContentLabel.Text = HelpForcx.ProtectScreen3;

                allServersForm.cxIPTextBox.Text = WinTools.GetcxIP();
                allServersForm.portTextBox.Text = WinTools.GetcxPortNumber();
                if (allServersForm.appNameSelected == AppName.Pushagent)
                {
                    allServersForm.previousButton.Visible = true;
                    allServersForm.nextButton.Enabled = false;
                    allServersForm.cxIPTextBox.ReadOnly = true;
                    allServersForm.portTextBox.ReadOnly = true;
                    if (WinTools.Https == true)
                    {
                        allServersForm.useHTTPSRadioButton.Checked = true;
                        allServersForm.useHTTPRadioButton.Checked = false;                        
                    }
                    else
                    {
                        allServersForm.useHTTPRadioButton.Checked = true;
                        allServersForm.useHTTPSRadioButton.Checked = false;
                    }
                    //allServersForm.cxSettingsGroupBox.Enabled = false;
                }

                //if (!File.Exists(_installPath + "\\" + UNIFIED_AGENT_NAME))
                //{
                //    allServersForm.pushAgentFileSelectionGroupBox.Visible = true;
                //}
                //else
                //{
                //    allServersForm.pushAgentFileSelectionGroupBox.Visible = false;
                //}

                // Making all the pushagnet and and rebootafter push variable as false
                // When user selects for push we will make those variables as true..
                // From 1.2 version we don't have rebbot option....

               
                foreach (Host h in allServersForm.selectedMasterListbyUser._hostList)
                {
                    h.pushAgent = false;
                    h.rebootAfterPush = false;
                }
                foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                {
                    h.rebootAfterPush = false;
                    h.pushAgent = false;
                }


                int hostCount = 0;
                int rowIndex = 0;
                hostCount = allServersForm.selectedSourceListByUser._hostList.Count;

                if (allServersForm.appNameSelected == AppName.Bmr)
                {
                    foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                    {
                        h.checkedCredentials = true;
                    }

                }

                // Filling the pusgagnetdatagridview with the master target and source machines.....
                if (hostCount > 0)
                {
                    allServersForm.pushAgentDataGridView.Rows.Clear();

                    //allServersForm.pushAgentDataGridView.RowCount = hostCount;
                    // Beofre filling the datagridview we are checking the count of the iplist and then
                    // We are filling them seperated with &.
                    foreach (Host h1 in allServersForm.selectedMasterListbyUser._hostList)
                    {
                        h1.role = Role.Secondary;
                        allServersForm.pushAgentDataGridView.Rows.Add(1);
                        allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[DISPLAY_NAME_COLUMN].Value = h1.displayname;
                        if (h1.IPlist != null)
                        {
                            if (h1.IPlist.Length > 1)
                            {
                                string ip = null;
                                foreach (string IP in h1.IPlist)
                                {
                                    if (ip == null)
                                    {
                                        ip = IP;
                                    }
                                    else
                                    {
                                        ip = ip + "&" + IP;
                                    }

                                }
                                allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[IP_COLUMN].Value = ip;
                            }
                            else
                            {
                                allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[IP_COLUMN].Value = h1.ip;

                            }
                        }
                        else
                        {
                            allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[IP_COLUMN].Value = h1.ip;
                        }
                        allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[AGENT_STATUS_COLUMN].Value = h1.role;
                        rowIndex++;
                    }

                    // Beofre filling the datagridview we are checking the count of the iplist and then
                    // We are filling them seperated with &.
                    foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                    {
                        h.role = Role.Primary;
                        // h.role = role.SOURCE;
                        allServersForm.pushAgentDataGridView.Rows.Add(1);
                        if (h.IPlist != null)
                        {
                            if (h.IPlist.Length > 1)
                            {
                                string ip = null;
                                foreach (string IP in h.IPlist)
                                {
                                    if (ip == null)
                                    {
                                        ip = IP;
                                    }
                                    else
                                    {
                                        ip = ip + "&" + IP;
                                    }
                                }
                                allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[IP_COLUMN].Value = ip;
                            }
                            else
                            {
                                allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[IP_COLUMN].Value = h.ip;

                            }
                        }
                        else
                        {
                            allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[IP_COLUMN].Value = h.ip;
                        }
                        allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[DISPLAY_NAME_COLUMN].Value = h.displayname;
                        allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[AGENT_STATUS_COLUMN].Value = h.role;
                        rowIndex++;
                    }
                }

                // At the time of seperate push we will direcly display this creen only
                // We will take input from the user to give crentials as based on the esx/p2v 
                // here we will be checking that user has selected p2v push or esx push.....
                if (allServersForm.appNameSelected == AppName.Pushagent)
                {//In case of push agent we are changing the role column into hostname column.....
                    allServersForm.generalLogTextBox.Visible = false;
                    //allServersForm.pushAgentDataGridView.Columns[PushAgentPanelHandler.ROLE_COLUMN].HeaderText = "Hostname";
                    allServersForm.previousButton.Visible = false;
                    allServersForm.nextButton.Visible = false;
                    
                    //allServersForm.pushAgentDataGridView.Columns[PushAgentPanelHandler.PUSHAGENT_COLUMN].Visible = true;
                    allServersForm.addCredentialsLabel.Text = "Install agents";
                    if (allServersForm.selectionByUser == Selection.P2vpush)
                    {
                        allServersForm.pushGetDeatsilsButton.TabIndex = 150;
                        allServersForm.pushDomainTextBox.TabIndex = 149;
                        allServersForm.pushDomainLabel.Visible = true;
                        allServersForm.pushDomainTextBox.PasswordChar = '*';
                        allServersForm.pushDomainTextBox.Visible = true;
                        allServersForm.pushIPLabel.Text = "Server IP Address *";
                        allServersForm.pushPassWordLabel.Text = "Username *";
                        allServersForm.pushUserNameLabel.Text = "Domain";
                        allServersForm.pushAgentCheckBox.Visible = false;
                        allServersForm.pushAgentHeadingLabel.Text = "Select primary servers for the push installation.";
                        allServersForm.pushAgentDataGridView.Columns[PUSHAGENT_COLUMN].HeaderText = "Select";


                    }
                    if (allServersForm.selectionByUser == Selection.Esxpush)
                    {
                        allServersForm.pushIPLabel.Text = "vSphere/vCenter IP/Name *";
                        allServersForm.pushGetDeatsilsButton.Location = new System.Drawing.Point(375, 50);
                        allServersForm.osTypeComboBox.Items.Add("Windows");
                        allServersForm.osTypeComboBox.SelectedItem = "Windows";
                        allServersForm.pushPasswordTextBox.PasswordChar = '*';
                        allServersForm.pushAgentHeadingLabel.Text = "Select vSphere server for the push installation.";
                    }
                    allServersForm.pushIPLabel.Visible = true;
                    allServersForm.pushIPTextBox.Visible = true;
                    allServersForm.pushUserNameLabel.Visible = true;
                    allServersForm.pushUserNAmeTextBox.Visible = true;
                    allServersForm.pushPassWordLabel.Visible = true;
                    allServersForm.pushPasswordTextBox.Visible = true;
                    allServersForm.pushAgentDataGridView.Visible = false;
                    allServersForm.pushAgentsButton.Visible = false;
                    allServersForm.pushUninstallButton.Visible = false;
                    allServersForm.pushAgentCheckBox.Visible = false;
                    allServersForm.pushGetDeatsilsButton.Visible = true;

                }
                else
                {
                    allServersForm.generalLogBoxClearLinkLabel.Visible = true;
                    allServersForm.pushAgentDataGridView.Location = new System.Drawing.Point(24, 53);
                    allServersForm.pushAgentCheckBox.Location = new System.Drawing.Point(380, 55);
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
            Trace.WriteLine(DateTime.Now + " \t Exiting: Initialize of PushAgentPanelHandler");
            //allServersForm.pushAgentHelpLabel.Text
            return true;
        }

        internal bool PushAgentsHandler(AllServersForm allServersForm)
        {
            // This will be called when user selects the install agents 
            // Fisrt we will check for the unifiedagent exists or not if it is not there in vocn folder
            // USer has to slect the path of unified agent....
            // After that we will start installing agnets to the selected machines.....
            Trace.WriteLine(DateTime.Now + " \t Entered: PushAgentsHandler() PushAgentPanelHandler");
            try
            {
                string binaryPath;
                string directory, fileName;
                string cxIp = "";
                string cxPort = "";
                bool masterRebooted = false;
                string masterTargetName = "";
                bool existsNatIPForAnyVm = false;

                _installAgents = true;
                cxIp = allServersForm.cxIPTextBox.Text;
                cxPort = allServersForm.portTextBox.Text;
               


               

                if (File.Exists(_installPath + "\\" + UNIFIED_AGENT_NAME))
                {
                    binaryPath = _installPath + "\\" + UNIFIED_AGENT_NAME;
                }
                else
                {
                   

                        MessageBox.Show("Unified agent is missing in vContinuum installation path", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return false;
                    
                }
                // MessageBox.Show("   " +cxPort);
                directory = Path.GetDirectoryName(binaryPath);
                fileName = Path.GetFileName(binaryPath);
                bool selectedForPush = false;
                _totalVmsSelectedForPush = 0;
                _totalVmsCompletedSuccessfully = 0;
                _totalFailed = 0;

                if (allServersForm.sourceRadioButton.Checked == true)
                {
                    foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                    {
                        h.role = Role.Primary;
                    }
                }
                else
                {
                    foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                    {
                        h.role = Role.Secondary;
                    }
                }

                
                foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                {
                    Trace.WriteLine(DateTime.Now + "\t Pushing agent as  "  + h.role.ToString()+ "  "+  h.displayname);
                }
                
                foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                {
                    if (h.pushAgent == true)
                    {
                        _totalVmsSelectedForPush++;
                        selectedForPush = true;
                    }
                }
                foreach (Host h in allServersForm.selectedMasterListbyUser._hostList)
                {
                    if (h.pushAgent == true)
                    {
                        _totalVmsSelectedForPush++;
                        selectedForPush = true;
                    }
                    if (h.rebootAfterPush == true)
                    {
                        masterRebooted = true;
                        masterTargetName = h.displayname + " IP: " + h.ip + " ";
                    }
                }
                if (selectedForPush == false)
                {
                    MessageBox.Show("Please select VMs to install agents", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return false;
                }



                if (_totalVmsSelectedForPush > 1 && (allServersForm.sourceRadioButton.Checked == false))
                {
                    // You have chosen more than one server as master target agent. Are you sure to proceed?
                  

                    switch (MessageBox.Show("You have chosen more than one server as master target agent. Are you sure you want to proceed?", "Warning",
                                 MessageBoxButtons.YesNo,
                                 MessageBoxIcon.Warning))
                    {
                        case DialogResult.No:
                            return false;
                    }

                }



                if (allServersForm.appNameSelected != AppName.Pushagent)
                {
                    foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                    {
                        h.cxIP = allServersForm.cxIPTextBox.Text.Trim();
                        h.portNumber = allServersForm.portTextBox.Text.Trim();
                    }
                    foreach (Host h in allServersForm.selectedMasterListbyUser._hostList)
                    {
                        h.cxIP = allServersForm.cxIPTextBox.Text.Trim();
                        h.portNumber = allServersForm.portTextBox.Text.Trim();
                    
                    }
                }
                else if (allServersForm.appNameSelected == AppName.Pushagent)
                {
                    foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                    {
                        h.cxIP = allServersForm.cxIPTextBox.Text.Trim();
                        h.portNumber = allServersForm.portTextBox.Text.Trim();
                        h.https = WinTools.Https;
                    }                
                }
                //Push Agents to the Master
                _isAgentsInstalled = true;
                if (allServersForm.selectedMasterListbyUser._hostList != null && allServersForm.selectedMasterListbyUser._hostList.Count != 0)
                {
                    PushAgents(allServersForm, allServersForm.selectedMasterListbyUser._hostList, directory, fileName, allServersForm.generalLogTextBox, cxIp, cxPort);
                }
                PushAgents(allServersForm, allServersForm.selectedSourceListByUser._hostList, directory, fileName, allServersForm.generalLogTextBox, cxIp, cxPort);
               // In case of upgrade agents user has to reboot vms manually so thats why we displaying user to rebbot particular vms.
                if (_agentUpgrade == true)
                {
                    tickerDelegate = new TickerDelegate(SetLeftTicker);
                    _message = "Following servers are upgraded to latest agent. Please reboot following servers" + Environment.NewLine;
                    allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                    _message = _upgradedVMSList + Environment.NewLine;
                    allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                    _message = "NOTE: Replications will not  progress unless you reboot server. But replication pairs will be set." + Environment.NewLine;
                    allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                }
                _agentUpgrade = false;
                _upgradedVMSList = null;
               
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
            Trace.WriteLine(DateTime.Now + " \t Exiting: PushAgentsHandler() PushAgentPanelHandler");
            return true;
        }

        internal bool SelectedHostForAdvancedOptions(AllServersForm allServersForm, int rowIndex)
        {
            try
            {
                if (allServersForm.pushAgentDataGridView.RowCount > 0)
                {
                    Host h1 = new Host();

                    if (allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[IP_COLUMN].Value != null)
                    {
                        h1.ip = allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[IP_COLUMN].Value.ToString();
                        if (h1.ip.Contains("&"))
                        {
                            h1.ip = h1.ip.Replace("&", ",");

                        }
                        else
                        {
                            h1.ip = allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[IP_COLUMN].Value.ToString();
                        }
                    }
                    if (allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[DISPLAY_NAME_COLUMN].Value != null)
                    {
                        h1.displayname = allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[DISPLAY_NAME_COLUMN].Value.ToString();
                    }
                    int listIndex = 0;
                    _credentialIndex = rowIndex;

                    if (allServersForm.selectedSourceListByUser.DoesHostExist(h1, ref listIndex) == true)
                    {
                        _selectedHost = ((Host)allServersForm.selectedSourceListByUser._hostList[listIndex]);
                        AdvancedSettingsforPush push = new AdvancedSettingsforPush(_selectedHost);
                        push.ShowDialog();

                        if (push.canceled == false)
                        {
                            _selectedHost.enableLogging = push.sourceHost.enableLogging;
                            _selectedHost.cacheDir = push.sourceHost.cacheDir;
                            _selectedHost.iptoPush = push.sourceHost.iptoPush;
                            _selectedHost.natip = push.sourceHost.natip;
                            
                            if (push.applyALLForLoggingCheckBox.Checked == true)
                            {
                                foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                                {
                                    h.enableLogging = push.sourceHost.enableLogging;
                                }
                            }
                            if (push.applyAllForCacheDirCheckBox.Checked == true)
                            {
                                foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                                {                                  
                                    h.cacheDir = push.sourceHost.cacheDir;
                                }
                            }                            
                        }
                    }
                    else
                    {
                        Trace.WriteLine(DateTime.Now + "\t Failed to find selected host in list ");
                        return false; 
                    }
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: SelectedHostForAdvancedOptions " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            return true;
        }

        internal bool PushOptionsChanged(AllServersForm allServersForm, int rowIndex)
        {
            try
            {
                // When user selects or unslects the push agnet column this method will be called.....
                // we will check whether this machine is present in our data structure or not and whether is mt or source...
                // and we will check for the credentials if credentials are not there with us we will ask for credentials and
                // we will validate the credentials also 
                // We will check whether vCOn machine and mt is same or not 
                // if same we wont ask for credentials....

                _selectedCredentialsForOneShot = false;
                if (allServersForm.pushAgentDataGridView.RowCount > 0)
                {
                    Host h1 = new Host();

                    if (allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[IP_COLUMN].Value != null)
                    {
                        h1.ip = allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[IP_COLUMN].Value.ToString();
                        if (h1.ip.Contains("&"))
                        {
                            h1.ip = h1.ip.Replace("&", ",");

                        }
                        else
                        {
                            h1.ip = allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[IP_COLUMN].Value.ToString();
                        }
                    }
                    if (allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[DISPLAY_NAME_COLUMN].Value != null)
                        h1.displayname = allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[DISPLAY_NAME_COLUMN].Value.ToString();

                    int listIndex = 0;
                    _credentialIndex = rowIndex;

                    // User has selected to push agent to this host
                    if ((bool)allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[PUSHAGENT_COLUMN].FormattedValue)
                    {
                        if (allServersForm.selectedSourceListByUser.DoesHostExist(h1, ref listIndex) == true)
                        {
                            _selectedHost = ((Host)allServersForm.selectedSourceListByUser._hostList[listIndex]);
                            if (((Host)allServersForm.selectedSourceListByUser._hostList[listIndex]).skipPushAndPreReqs == false)
                            {

                                ((Host)allServersForm.selectedSourceListByUser._hostList[listIndex]).pushAgent = true;
                            }
                            else
                            {
                                MessageBox.Show("Remote agent installation functionality is disabled for this host", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                ((Host)allServersForm.selectedSourceListByUser._hostList[listIndex]).pushAgent = false;
                                allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[PUSHAGENT_COLUMN].Value = false;
                                allServersForm.pushAgentDataGridView.RefreshEdit();
                                return false;
                            }
                            Trace.WriteLine(DateTime.Now + " \t       Ip and display name are matched ");
                            /* if ((bool)allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[REBOOT_COLUMN].FormattedValue)
                             {
                                 if (((Host)allServersForm._selectedSourceList._hostList[listIndex]).rebootAfterPush == false)
                                 {                                   
                                         ((Host)allServersForm._selectedSourceList._hostList[listIndex]).rebootAfterPush = true;
                                 }
                                 Trace.WriteLine(DateTime.Now + " \t     Printing value of push agent " + ((Host)allServersForm._selectedSourceList._hostList[listIndex]).pushAgent + ((Host)allServersForm._selectedSourceList._hostList[listIndex]).rebootAfterPush);
                             }
                             else*/
                            {
                                ((Host)allServersForm.selectedSourceListByUser._hostList[listIndex]).rebootAfterPush = false;
                            }
                            if (_selectedHost.hostname.Split('.')[0].ToUpper() != Environment.MachineName.ToUpper())
                            {

                                if (_selectedHost.userName == null)
                                {
                                    if (GetCredentials(allServersForm, ref _selectedHost.domain, ref _selectedHost.userName, ref _selectedHost.passWord) == false)
                                    {
                                        _selectedHost.pushAgent = false;
                                        Debug.WriteLine("MasterVMSelected: Returning false");
                                        return false;
                                    }
                                }
                            }
                            if (allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[PUSHAGENT_COLUMN].Selected == true)
                            {
                                if (allServersForm.selectedSourceListByUser.DoesHostExist(_selectedHost, ref listIndex) == true)
                                {
                                    allServersForm.progressBar.Visible = true;
                                    allServersForm.p2v_PushAgentPanel.Enabled = false;
                                    allServersForm.nextButton.Enabled = false;
                                    allServersForm.previousButton.Enabled = false;
                                    Trace.WriteLine(DateTime.Now + " \t Printing the current value of backgroundworker " + _backGroundWorkerIsCompleted.ToString());
                                    _backGroundWorkerIsCompleted = true;
                                    allServersForm.checkCredentialsBackgroundWorker.RunWorkerAsync();
                                }
                            }
                        }
                    }
                    else  // Push agent is not selected for this host
                    {
                        if (allServersForm.selectedSourceListByUser.DoesHostExist(h1, ref listIndex))
                        {
                            ((Host)allServersForm.selectedSourceListByUser._hostList[listIndex]).pushAgent = false;
                            //allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[REBOOT_COLUMN].Value = false;
                            allServersForm.pushAgentDataGridView.RefreshEdit();
                            //allServersForm._selectedSourceList.Print();
                        }
                    }
                    // User has selected to push agent  for mastet target
                    if ((bool)allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[PUSHAGENT_COLUMN].FormattedValue)
                    {
                        if (allServersForm.selectedMasterListbyUser.DoesHostExist(h1, ref listIndex) == true)
                        {
                            _selectedHost = ((Host)allServersForm.selectedMasterListbyUser._hostList[listIndex]);
                            ((Host)allServersForm.selectedMasterListbyUser._hostList[listIndex]).pushAgent = true;
                            /* if ((bool)allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[REBOOT_COLUMN].FormattedValue)
                             {
                                 if (((Host)allServersForm._selectedMasterList._hostList[listIndex]).rebootAfterPush == false)
                                 {                                  
                                         ((Host)allServersForm._selectedMasterList._hostList[listIndex]).rebootAfterPush = true;                                    
                                 }
                             }
                             else*/
                            {
                                ((Host)allServersForm.selectedMasterListbyUser._hostList[listIndex]).rebootAfterPush = false;
                            }
                            if (_selectedHost.hostname.Split('.')[0].ToUpper() != Environment.MachineName.ToUpper())
                            {
                                if (_selectedHost.userName == null)
                                {
                                    if (GetCredentials(allServersForm, ref _selectedHost.domain, ref _selectedHost.userName, ref _selectedHost.passWord) == false)
                                    {
                                        _selectedHost.pushAgent = false;
                                        Debug.WriteLine("MasterVMSelected: Returning false");
                                        return false;
                                    }
                                }
                            }
                            if (allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[PUSHAGENT_COLUMN].Selected == true)
                            {
                                if (allServersForm.selectedMasterListbyUser.DoesHostExist(_selectedHost, ref listIndex) == true)
                                {
                                    allServersForm.progressBar.Visible = true;
                                    allServersForm.p2v_PushAgentPanel.Enabled = false;
                                    allServersForm.nextButton.Enabled = false;
                                    allServersForm.previousButton.Enabled = false;
                                    Trace.WriteLine(DateTime.Now + " \t Printing the current value of backgroundworker " + _backGroundWorkerIsCompleted.ToString());

                                    _backGroundWorkerIsCompleted = true;
                                    allServersForm.checkCredentialsBackgroundWorker.RunWorkerAsync();
                                }
                            }
                        }
                    }
                    else  // Push agent is not selected for master target
                    {
                        if (allServersForm.selectedMasterListbyUser.DoesHostExist(h1, ref listIndex))
                        {
                            ((Host)allServersForm.selectedMasterListbyUser._hostList[listIndex]).pushAgent = false;
                            // allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[REBOOT_COLUMN].Value = false;
                            allServersForm.pushAgentDataGridView.RefreshEdit();
                        }
                    }


                   /*  if (allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[USE_NAT_IP_COLUMN].Selected == true)
                     {
                         Host tempHost = new Host();

                         if (allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[IP_COLUMN].Value != null)
                             tempHost.ip = allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[IP_COLUMN].Value.ToString();
                         if (allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[DISPLAY_NAME_COLUMN].Value != null)
                             tempHost.displayname = allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[DISPLAY_NAME_COLUMN].Value.ToString();
                         if ((bool)allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[USE_NAT_IP_COLUMN].FormattedValue)
                         {

                             NATIPForm nat = new NATIPForm();
                             nat.natIPLabel.Text = "Enter NAT IP address for " + tempHost.displayname;
                             nat.ShowDialog();
                             if (nat._cancelled == false)
                             {
                                 if (allServersForm._selectedSourceList.DoesHostExist(tempHost, ref listIndex) == true)
                                 {
                                     ((Host)allServersForm._selectedSourceList._hostList[listIndex]).containsNatIPAddress = true;
                                     ((Host)allServersForm._selectedSourceList._hostList[listIndex]).natIPAddress = nat.natIPTextBox.Text;

                                 }
                                 if (allServersForm._selectedMasterList.DoesHostExist(tempHost, ref listIndex) == true)
                                 {
                                     ((Host)allServersForm._selectedMasterList._hostList[listIndex]).containsNatIPAddress = true;
                                     ((Host)allServersForm._selectedMasterList._hostList[listIndex]).natIPAddress = nat.natIPTextBox.Text;

                                 }
                             }
                             else
                             {
                                 allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[USE_NAT_IP_COLUMN].Value = false;
                                 allServersForm.pushAgentDataGridView.RefreshEdit();
                                 return false;
                             }

                         }
                         else
                         {
                             if (allServersForm._selectedSourceList.DoesHostExist(tempHost, ref listIndex) == true)
                             {
                                 ((Host)allServersForm._selectedSourceList._hostList[listIndex]).containsNatIPAddress = false;
                                 ((Host)allServersForm._selectedSourceList._hostList[listIndex]).natIPAddress = null;
                             }
                             if (allServersForm._selectedMasterList.DoesHostExist(tempHost, ref listIndex) == true)
                             {
                                 ((Host)allServersForm._selectedMasterList._hostList[listIndex]).containsNatIPAddress = false;
                                 ((Host)allServersForm._selectedMasterList._hostList[listIndex]).natIPAddress = null;

                             }
                         }
                        
                     }*/


                }

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





        internal override bool ProcessPanel(AllServersForm allServersForm)
        {
            try
            {

                foreach(Host h in allServersForm.selectedSourceListByUser._hostList)
                {
                Trace.WriteLine(DateTime.Now + "\t Printing the role " +  h.role.ToString());
                }
                bool _selectedForPushAgents = false;

                // Here we will check that if user slects for push 
                // and if he doesn't install agents we will ask conformation message to go for next screen or 
                // he wants to install agnets.......
                foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                {
                    if (h.pushAgent == true)
                    {
                        _selectedForPushAgents = true;
                    }
                }
                foreach (Host h in allServersForm.selectedMasterListbyUser._hostList)
                {
                    if (h.pushAgent == true)
                    {
                        _selectedForPushAgents = true;
                    }
                }
                if (_selectedForPushAgents == true)
                {
                    if (_installAgents == false)
                    {
                        switch (MessageBox.Show("Are you sure you want to skip the push install?", "vContinuum", MessageBoxButtons.YesNo, MessageBoxIcon.Question))
                        {
                            case DialogResult.Yes:
                                break;
                            case DialogResult.No:
                                return false;
                                break;

                        }
                    }
                }      
                Trace.WriteLine(DateTime.Now + " \t Entered: ProcessPanel of PushAgentPanelHandler");
               

                // In next screen we want to display the total size of the machines in space column 
                // So wr are adding all the disks selected for the protection.....
                if (allServersForm.appNameSelected == AppName.Esx || allServersForm.appNameSelected == AppName.Failback || (allServersForm.appNameSelected == AppName.Offlinesync && ResumeForm.selectedActionForPlan == "Esx"))
                {
                    foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                    {


                        ArrayList physicalDisks;
                        physicalDisks = h.disks.list;
                        h.totalSizeOfDisks = 0;

                        //Move this to initialize of sourceToMaster post 5.5

                        foreach (Hashtable disk in physicalDisks)
                        {
                            if (disk["Selected"].ToString() == "Yes")
                            {
                                if (disk["Rdm"].ToString() == "no" || h.convertRdmpDiskToVmdk == true)
                                {
                                    if (disk["Size"] != null)
                                    {
                                        float size = float.Parse(disk["Size"].ToString());

                                        size = size / (1024 * 1024);
                                        h.totalSizeOfDisks = h.totalSizeOfDisks + size;
                                    }
                                }



                            }
                        }
                        if (h.totalSizeOfDisks.ToString().Contains("."))
                        {
                            h.totalSizeOfDisks = (float)Math.Round(h.totalSizeOfDisks, 0);
                        }
                    }
                }
                else if (allServersForm.appNameSelected == AppName.Bmr || (allServersForm.appNameSelected == AppName.Offlinesync && ResumeForm.selectedActionForPlan == "P2v"))
                {
                    // Commented this code because in case of p2v we are calculating size in first screenonly 

                    //foreach (Host h in allServersForm._selectedSourceList._hostList)
                    //{
                    //    ArrayList physicalDisks;

                    //    //h.disks.AddScsiNumbers();

                    //    physicalDisks = h.disks.GetDiskList();
                    //    h.totalSizeOfDisks = 0;
                    //    Trace.WriteLine(DateTime.Now + "\t Priniting the count of disks" + h.disks.GetDiskList().Count.ToString());
                    //    foreach (Hashtable disk in physicalDisks)
                    //    {
                    //        if (disk["Selected"].ToString() == "Yes")
                    //        {
                    //            if (disk["Size"] != null)
                    //            {

                    //                int size = int.Parse(disk["Size"].ToString());

                    //                size = size / (1024 * 1024);
                    //                h.totalSizeOfDisks = h.totalSizeOfDisks + size;
                    //                h.machineType = "PhysicalMachine";
                    //            }
                    //            // Trace.WriteLine("Printing the value of the disk " + h.totalSizeOfDisks);

                    //        }



                    //    }
                    //}
                }

                /* if (allServersForm._osType == OStype.Windows)
                 {
                     Host masterHost = (Host)allServersForm._selectedMasterList._hostList[0];
                     masterHost.disks.partitionList.Clear();
                     // RemoteWin rw = new RemoteWin(masterHost.ip, masterHost.domain, masterHost.userName, masterHost.passWord);
                     //rw.Connect(ref errorMessage, ref errorCode);

                     if (masterHost.disks.GetPartitionsFreeSpace(ref masterHost) == false)
                     {
                         MessageBox.Show("Master target:"+masterHost.displayname + " (" + masterHost.ip+" )"+ " information is not found in the CX server "+
                         Environment.NewLine + "Please verify that" +
                         Environment.NewLine + "1. Agent is installed" +
                         Environment.NewLine + "2. Configured ( in AgentConfiguration ) to point to " + WinTools.GetcxIP() + " with port number: " + WinTools.GetCxPortNumber()() +
                         Environment.NewLine + "3.  InMage Scout services are up and running" , "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);

                         return false;
                     }
                     masterHost.Print();

                 }*/

                // This code is not be in used we can remove this code in the next version..... 
                // BEcause entire nat flow is changed.....
                if (_isAgentsInstalled == false)
                {
                    bool existsNatIPForAnyVm = false;


                    foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                    {
                        if (h.natIPAddress != null)
                        {
                            existsNatIPForAnyVm = true;
                        }
                    }
                    foreach (Host h in allServersForm.selectedMasterListbyUser._hostList)
                    {
                        if (h.natIPAddress != null)
                        {
                            existsNatIPForAnyVm = true;
                        }
                    }
                    
                }

                Trace.WriteLine(DateTime.Now + " \t Exiting: ProcessPanel of PushAgentPanelHandler");
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
        private void SetLeftTicker(string s)
        {
            //This will be used while backgroundworker is running we can update the text box by invoking the control....   
            _textBox.AppendText(s);
        }

        private void SetLabelText(string s)
        {
            _label.Visible = true;
            _label.Text = s;
        }

        private bool PushAgents(AllServersForm allServersForm, ArrayList inHostList, string inDirectory, string inBinaryFile, TextBox inStatusBox, string inCxIp, string inCxPort)
        {
            try
            {
                // This method will be called by the pushagnetshandler() method once all the checks are completed
                // We will call this method to install agents..
                // In this first we will checks for the agent status based on that we will install agnet or 
                // we will update the cx ip only if already agents are installed.......
                Trace.WriteLine(DateTime.Now + " \t   Entered: PushAgents of PushAgentPanelHandler");
                int returnCodeForPush;
                bool pushSucceeded = true;
                string error = "";
                int returCode;
                string currentCXip = WinTools.GetcxIP();
                string cxipWithPortNumber = WinTools.GetcxIPWithPortNumber();


                tickerDelegate = new TickerDelegate(SetLabelText);
                _label = allServersForm.pushAgentStatsLabel;

                _message = "Number of servers selected:" + _totalVmsSelectedForPush;
                allServersForm.pushAgentStatsLabel.Invoke(tickerDelegate, new object[] { _message });

                tickerDelegate = new TickerDelegate(SetLeftTicker);
                _message = "Running readiness checks for the selected machines" + Environment.NewLine;
                allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                foreach(Host h in inHostList)
                {
                    if (h.pushAgent == true && h.skipPushAndPreReqs == false)
                    {
                        Host tempHost = new Host();
                        tempHost.ip = h.ip;
                        tempHost.displayname = h.displayname;
                        tempHost.hostname = h.hostname;
                        WinPreReqs win = new WinPreReqs("", "", "", "");
                        win.GetDetailsFromcxcli(tempHost, cxipWithPortNumber);
                        tempHost = WinPreReqs.GetHost;
                        if (tempHost.vxagentInstalled == true)
                        {
                            if (h.role == Role.Primary && tempHost.masterTarget == true)
                            {
                                MessageBox.Show("Master target agent is already installed on " + h.displayname + ". You can't install Scout Agent on this. Please uninstall master target agent from " + h.displayname + " and try installing Scout Agent.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                return false;
                            }
                            if (h.role == Role.Secondary && h.masterTarget == false)
                            {
                                MessageBox.Show("Source agent is already installed on " + h.displayname + ". You can't install Scout Agent on this. Please uninstall source agent from " + h.displayname + " and try installing Scout Agent.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                return false;
                            }
                        }
                    }
                    
                }
                _message = "Completed readiness checks for the selected machines" + Environment.NewLine;
                allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                foreach (Host h in inHostList)
                {
                    
                    if (h.iptoPush != null)
                    {
                        h.ip = h.iptoPush;
                    }
                    tickerDelegate = new TickerDelegate(SetLabelText);
                    _label = allServersForm.pushagentSuccessLabel;
                    _message = "Success: " + _totalVmsCompletedSuccessfully.ToString();
                    allServersForm.pushagentSuccessLabel.Invoke(tickerDelegate, new object[] { _message });
                    _label = allServersForm.pushAgentFailedLabel;
                    _message = "Failed: " + _totalFailed;
                    allServersForm.pushAgentFailedLabel.Invoke(tickerDelegate, new object[] { _message });
                    tickerDelegate = new TickerDelegate(SetLeftTicker);
                    //skipping the pushinstaller if user select skipWmicheck as true

                    if (h.pushAgent == true && h.skipPushAndPreReqs == false)
                    {
                        Trace.WriteLine(DateTime.Now + " \t       Pushing Agent to " + h.ip + "With account  Domain = " + h.domain);
                        pushSucceeded = true;
                        try
                        {
                            _message = "Checking whether agent is present or not on:" + h.ip;
                            allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                        }
                        catch (Exception ex)
                        {
                            Trace.WriteLine(DateTime.Now + " \appending to text box " + ex.Message);
                        }
                        PushInstall pushObj = new PushInstall();
                        string outPut = null;
                        int returnCode = 0;
                        CheckCXIP(h, ref outPut);
                        string currentCXIP = WinTools.GetcxIP();
                        bool updateCXIp = false;
                        bool installAgent = true;
                        if (outPut != null)
                        {
                            if (outPut.Contains("="))
                            {
                                string[] outPutSplit = outPut.Split('=');
                                string cxip = outPutSplit[0];
                                if (outPutSplit[1] == "2")
                                {
                                    returnCode = 2;
                                    try
                                    {
                                        _message = "  \t ....Already Fx is installed. " + Environment.NewLine;
                                        allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });


                                    }
                                    catch (Exception ex)
                                    {
                                        Trace.WriteLine(DateTime.Now + " \appending to text box " + ex.Message);
                                    }
                                    if (cxip != currentCXip)
                                    {

                                        switch (MessageBox.Show("InMage Scout FX  is already installed and pointed to CX IP: " + cxip + Environment.NewLine + " Do you want to install VX and update CX IP with: " + currentCXip, "Update CX IP", MessageBoxButtons.YesNo,
                                          MessageBoxIcon.Question))
                                        {
                                            case DialogResult.Yes:
                                                updateCXIp = true;
                                                break;
                                            case DialogResult.No:
                                                installAgent = false;
                                                continue;

                                        }

                                    }
                                }
                                else if (outPutSplit[1] == "3")
                                {
                                    returnCode = 3;
                                    try
                                    {
                                        _message = "  \t ....Already Vx is installed. " + Environment.NewLine;
                                        allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });

                                    }
                                    catch (Exception ex)
                                    {
                                        Trace.WriteLine(DateTime.Now + " \appending to text box " + ex.Message);
                                    }
                                    if (cxip != currentCXip)
                                    {
                                        switch (MessageBox.Show("InMage Scout VX  is already installed and pointed to CX IP: " + cxip + Environment.NewLine + " Do you want to install FX and update CX IP with: " + currentCXip, "Update CX IP", MessageBoxButtons.YesNo,
                                          MessageBoxIcon.Question))
                                        {
                                            case DialogResult.Yes:
                                                updateCXIp = true;
                                                break;
                                            case DialogResult.No:
                                                installAgent = false;
                                                continue;

                                        }

                                    }
                                }
                                else if (outPutSplit[1] == "0")
                                {
                                    returnCode = 0;
                                    try
                                    {

                                        _message = "  \t ....Already Installed. " + Environment.NewLine;
                                        allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                                        // Comparing cx ip which is already in vm to current cx ip. 
                                        // If same cx ip is there we are not going to update cx ip.                               

                                    }
                                    catch (Exception ex)
                                    {
                                        Trace.WriteLine(DateTime.Now + " \t appending to text box " + ex.Message);
                                    }
                                    if (cxip != currentCXip)
                                    {

                                        switch (MessageBox.Show("InMage Scout Unified agent is already installed and pointed to CX IP: " + cxip + Environment.NewLine + " Do you want to update CX IP with: " + currentCXip, "Update CX IP", MessageBoxButtons.YesNo,
                                          MessageBoxIcon.Question))
                                        {
                                            case DialogResult.Yes:
                                                updateCXIp = true;
                                                break;
                                            case DialogResult.No:
                                                installAgent = false;
                                                continue;

                                        }

                                    }
                                }
                            }

                            else
                            {
                                returnCode = 1;
                            }
                        }
                        else
                        {
                            returnCode = 1;
                        }
                        if (installAgent == true)
                        {
                            Trace.WriteLine(DateTime.Now + "\t Printing the return code of the agent status " + returnCode.ToString());
                            if (returnCode == 0)
                            {

                                _agentUpgrade = true;
                                if (_upgradedVMSList == null)
                                {
                                    _upgradedVMSList = h.displayname + Environment.NewLine;
                                }
                                else
                                {
                                    _upgradedVMSList = _upgradedVMSList + h.displayname + Environment.NewLine;
                                }
                                
                                _message = "Upgrading agents to latest version on " + h.ip + "(" + h.displayname + ") ";
                                allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                                int returncode = 0;
                               returncode=  pushObj.UpgradeAgents(h.ip, h.domain, h.userName, h.passWord, inDirectory, inBinaryFile,  h);
                                if (returncode == PushInstall.RETURN_CODE_CERT_TRUST_VERIFICATION_FAILED)
                                {
                                    _message = "Found that UnifiedAgent.exe in vContinuum installation path is not trusted binary" + Environment.NewLine;
                                    _agentUpgrade = false;
                                    allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                                    return false;

                                }
                                else if (returncode == PushInstall.RETURN_CODE_CERT_ROOT_IS_NOT_MS_CA)
                                {
                                    _message = "Found that UnifiedAgent.exe in vContinuum installation path is not Microsoft certified binary" + Environment.NewLine;
                                    allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                                    _agentUpgrade = false;
                                    return false;

                                }
                                _message = " \t ....Done. " + Environment.NewLine;
                                allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                                if (updateCXIp == true)
                                {
                                    _message = "Updating the CX IP and port number. ";
                                    allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                                    
                                    pushObj.UpdatecxIPandPortNumber(h.ip, h.domain, h.userName, h.passWord, inDirectory, inBinaryFile, h);
                                    
                                    _message = " \t ....Done." + Environment.NewLine;

                                    allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                                }
                                _totalVmsCompletedSuccessfully++;
                            }
                            else if (returnCode == 2)
                            {

                                _message = "Installing Vx agent..  " ;
                                allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                                int returncode = 0;
                              returncode=   pushObj.UpgradeAgents(h.ip, h.domain, h.userName, h.passWord, inDirectory, inBinaryFile,  h);
                                if (returncode == PushInstall.RETURN_CODE_CERT_TRUST_VERIFICATION_FAILED)
                                {
                                    _message = "Found that UnifiedAgent.exe in vContinuum installation path is not trusted binary" + Environment.NewLine;
                                    allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                                    return false;

                                }
                                else if (returncode == PushInstall.RETURN_CODE_CERT_ROOT_IS_NOT_MS_CA)
                                {
                                    _message = "Found that UnifiedAgent.exe in vContinuum installation path is not Microsoft certified binary" + Environment.NewLine;
                                    allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                                    return false;

                                }
                                _message = "....Done. " + Environment.NewLine;
                                allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                                if (updateCXIp == true)
                                {
                                    _message = "Updating the CX IP and port number. ";
                                    allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                                   
                                     pushObj.UpdatecxIPandPortNumber(h.ip, h.domain, h.userName, h.passWord, inDirectory, inBinaryFile,  h);
                                   
                                    _message = " \t ....Done." + Environment.NewLine;

                                    allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                                }
                                _totalVmsCompletedSuccessfully++;
                                
                            }
                            else if (returnCode == 3)
                            {


                                _message = "Installing Fx agent";
                                allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                                int returncode = 0;
                                returncode = pushObj.UpgradeAgents(h.ip, h.domain, h.userName, h.passWord, inDirectory, inBinaryFile,  h);
                                if (returncode == PushInstall.RETURN_CODE_CERT_TRUST_VERIFICATION_FAILED)
                                {
                                    _message = "Found that UnifiedAgent.exe in vContinuum installation path is not trusted binary" + Environment.NewLine;
                                    allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                                    return false;

                                }
                                else if (returncode == PushInstall.RETURN_CODE_CERT_ROOT_IS_NOT_MS_CA)
                                {
                                    _message = "Found that UnifiedAgent.exe in vContinuum installation path is not Microsoft certified binary" + Environment.NewLine;
                                    allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                                    return false;

                                }
                                _message = "....Done." + Environment.NewLine;
                                allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                                if (updateCXIp == true)
                                {
                                    _message = "Updating the CX IP and port number ";
                                    allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                                   pushObj.UpdatecxIPandPortNumber(h.ip, h.domain, h.userName, h.passWord, inDirectory, inBinaryFile,  h);
                                   
                                    _message = " \t ....Done." + Environment.NewLine;

                                    allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                                }
                                _totalVmsCompletedSuccessfully++;
                                

                            }
                            else
                            {
                                _message = "....Not Installed.  " + Environment.NewLine;
                                allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                                _message = "Pushing agent to " + h.ip + Environment.NewLine;
                                allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                                RemoteWin remote = null;

                                Trace.WriteLine(DateTime.Now + " \t       Printing the reboot Value " + h.rebootAfterPush);
                                Trace.WriteLine(DateTime.Now + " \t      Calling Push agent with CX IP, Port" + inCxIp + inCxPort);
                                string ip = null;
                                if (h.natIPAddress == null)
                                {
                                    ip = h.ip;
                                }
                                else
                                {
                                    ip = h.natIPAddress;
                                }
                                returnCodeForPush = pushObj.PushAgent(ip, h.domain, h.userName, h.passWord, inDirectory, inBinaryFile, inCxIp, inCxPort,  h.rebootAfterPush,   remote, h);

                                if (returnCodeForPush == 0)
                                {
                                    _message = "Agent is installed on machine " + h.ip + " (" + h.displayname + " )." + Environment.NewLine;
                                    allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                                    _totalVmsCompletedSuccessfully++;
                                }
                                else if (returnCodeForPush == PushInstall.RETURN_CODE_CERT_TRUST_VERIFICATION_FAILED)
                                {
                                    _message = "Found that UnifiedAgent.exe in vContinuum installation is not trusted binary" + Environment.NewLine;
                                    allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                                    return false;
                                }
                                else if (returnCodeForPush == PushInstall.RETURN_CODE_CERT_ROOT_IS_NOT_MS_CA)
                                {
                                    _message = "Found that UnifiedAgent.exe in vContinuum installation is not Microsoft certified binary" + Environment.NewLine;
                                    allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                                    return false;

                                }
                                else if (returnCodeForPush == -1)
                                {
                                    if (h.operatingSystem == "Windows_XP_32" || h.operatingSystem.Contains("XP"))
                                    {
                                        _message = "Agent is installed on machine " + h.ip + " (" + h.displayname + " )." + Environment.NewLine;
                                        allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                                        _totalVmsCompletedSuccessfully++;
                                    }
                                    else
                                    {
                                        _message = "Failed to install agent. Please install/upgrade manually on " + h.ip + " (" + h.displayname + " )." + Environment.NewLine;
                                        allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                                        _message = "Re-trying for the same machine.... " + h.ip + " (" + h.displayname + " )." + Environment.NewLine;
                                        allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                                        returnCodeForPush = pushObj.PushAgent(ip, h.domain, h.userName, h.passWord, inDirectory, inBinaryFile, inCxIp, inCxPort, h.rebootAfterPush,   remote, h);
                                        if (returnCodeForPush == 0)
                                        {
                                            _message = "Agent is installed on machine " + h.ip + " (" + h.displayname + " )." + Environment.NewLine;
                                            allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                                            _totalVmsCompletedSuccessfully++;
                                        }
                                        else if (returnCodeForPush == 2)
                                        {
                                            _message = "Found that UnifiedAgent.exe in vContinuum installation is not trusted binary" + Environment.NewLine;
                                            allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                                            return false;
                                        }
                                        else if (returnCodeForPush == 3)
                                        {
                                            _message = "Found that UnifiedAgent.exe in vContinuum installation is not Microsoft certified binary" + Environment.NewLine;
                                            allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                                            return false;

                                        }
                                        else if (returnCodeForPush == -1)
                                        {
                                            if (h.operatingSystem == "Windows_XP_32" || h.operatingSystem.Contains("XP"))
                                            {
                                                _message = "Agent is installed on machine " + h.ip + " (" + h.displayname + " )." + Environment.NewLine;
                                                allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                                                _totalVmsCompletedSuccessfully++;
                                            }
                                            else
                                            {
                                                _message = "Failed to install agent. Please install/upgrade manually on " + h.ip + " (" + h.displayname + " )." + Environment.NewLine;
                                                allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                                                _totalFailed++;
                                            }
                                        }
                                        else
                                        {
                                            _message = "Failed to install agent. Please install/upgrade manually on " + h.ip + " (" + h.displayname + " )." + Environment.NewLine;
                                            allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                                            _totalFailed++;
                                        }
                                    }
                                }
                                else
                                {
                                    _message = "Failed to install agent. Please install/upgrade manually on " + h.ip + " (" + h.displayname + " )." + Environment.NewLine;
                                    allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                                    _message = "Re-trying for the same machine.... " + h.ip + " (" + h.displayname + " )." + Environment.NewLine;
                                    allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                                    returnCodeForPush = pushObj.PushAgent(ip, h.domain, h.userName, h.passWord, inDirectory, inBinaryFile, inCxIp, inCxPort, h.rebootAfterPush,   remote, h);
                                    if (returnCodeForPush == 0)
                                    {
                                        _message = "Agent is installed on machine " + h.ip + " (" + h.displayname + " )." + Environment.NewLine;
                                        allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                                        _totalVmsCompletedSuccessfully++;
                                    }
                                    else if (returnCodeForPush == PushInstall.RETURN_CODE_CERT_TRUST_VERIFICATION_FAILED)
                                    {
                                        _message = "Found that UnifiedAgent.exe in vContinuum installation is not trusted binary" + Environment.NewLine;
                                        allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                                        return false;
                                    }
                                    else if (returnCodeForPush == PushInstall.RETURN_CODE_CERT_ROOT_IS_NOT_MS_CA)
                                    {
                                        _message = "Found that UnifiedAgent.exe in vContinuum installation is not Microsoft certified binary" + Environment.NewLine;
                                        allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                                        return false;

                                    }
                                    else if (returnCodeForPush == -1)
                                    {
                                        if (h.operatingSystem == "Windows_XP_32" || h.operatingSystem.Contains("XP"))
                                        {
                                            _message = "Agent is installed on machine " + h.ip + " (" + h.displayname + " )." + Environment.NewLine;
                                            allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                                            _totalVmsCompletedSuccessfully++;
                                        }
                                        else
                                        {
                                            _message = "Failed to install agent. Please install/upgrade manually on " + h.ip + " (" + h.displayname + " )." + Environment.NewLine;
                                            allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                                            _totalFailed++;
                                        }
                                    }
                                    else
                                    {
                                        _message = "Failed to install agent. Please install/upgrade manually on " + h.ip + " (" + h.displayname + " )." + Environment.NewLine;
                                        allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                                        _totalFailed++;
                                    }
                                }
                            }

                        }

                        tickerDelegate = new TickerDelegate(SetLabelText);
                        _label = allServersForm.pushagentSuccessLabel;
                        _message = "Success: " + _totalVmsCompletedSuccessfully.ToString();
                        allServersForm.pushagentSuccessLabel.Invoke(tickerDelegate, new object[] { _message });
                        _label = allServersForm.pushAgentFailedLabel;
                        _message = "Failed: " + _totalFailed;
                        allServersForm.pushAgentFailedLabel.Invoke(tickerDelegate, new object[] { _message });
                        Trace.WriteLine(DateTime.Now + " \t  Exiting: PushAgents of PushAgentPanelHandler");
                       
                    }
                }
                return pushSucceeded;
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


        private bool CheckCXIP(Host h, ref string outPut)
        {
            outPut = null;
            string currentCXIP = WinTools.GetcxIP();
            string cxipwithPortnumenr = WinTools.GetcxIPWithPortNumber();
            string cxHostName = null;
            WinPreReqs win = new WinPreReqs(h.ip, h.domain, h.userName, h.passWord);
            //RemoteWin remotewin = new RemoteWin(h.ip, h.domain, h.userName, h.passWord, h.hostname);
            string pushOutPut = null;
            win.AgentCheckStatus(h,  pushOutPut);
            pushOutPut = WinPreReqs.GetOutput;
            outPut = pushOutPut;
            return true;
        }

        // Now we are not at all using this method......
        // we will remove this method in the post 6.0 release.....
        private void SleepAndRefresh(AllServersForm allServersForm, long sleepTime)
        {
            try
            {
                Trace.WriteLine(DateTime.Now + " \t Entered: SleepAndRefresh() of PushAgentPanelHandler");
                int sleepInterval = 50, totalSleep = 0;
                int i = 0;


                while (totalSleep < sleepTime)
                {
                    Thread.Sleep(sleepInterval);
                    totalSleep += sleepInterval;

                    allServersForm.progressBar.Value = i++;
                    if (i > 99) i = 0;

                    allServersForm.Refresh();
                }
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");

                //return false;
            }
            Trace.WriteLine(DateTime.Now + " \t Entered: SleepAndRefresh() of PushAgentPanelHandler");

        }


        internal override bool ValidatePanel(AllServersForm allServersForm)
        {
            Trace.WriteLine(DateTime.Now + " \t Reached P2V_PushAgentPanelHandler  ValidatePanel");
            return true;
        }

        internal override bool CanGoToNextPanel(AllServersForm allServersForm)
        {
            Trace.WriteLine(DateTime.Now + " \t Reached P2V_PushAgentPanelHandler  CanGoToNextPanel");
            return true;

        }

        internal override bool CanGoToPreviousPanel(AllServersForm allServersForm)
        {
            // While moving to the previous screens we need to update the help content ....
            try
            {

                if (allServersForm.appNameSelected == AppName.Pushagent)
                {
                    allServersForm.previousButton.Visible = false;
                    allServersForm.nextButton.Enabled = true;
                    allServersForm.helpContentLabel.Text = HelpForcx.ProtectScreen1;
                }
                else
                {
                    allServersForm.helpContentLabel.Text = HelpForcx.ProtectScreen2;
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
            Trace.WriteLine(DateTime.Now + " \t Reached P2V_PushAgentPanelHandler  CanGoToPreviousPanel");
            return true;

        }
        internal bool GetCredentials(AllServersForm allServersForm, ref string inDomain, ref string inUserName, ref string inPassword)
        {
            // This will be called when the credential are null
            // And we will connect to this machines using wmi....
            // Use this credentials for all machines option is there in the popup...... 
            Trace.WriteLine(DateTime.Now + " \t Entered: GetCredentials of Esx_AddSourcePanelHandler   ");
            string hostname = "";
            try
            {
                AddCredentialsPopUpForm addCredentialsForm = new AddCredentialsPopUpForm();

                addCredentialsForm.domainText.Text = allServersForm.cachedDomain;
                addCredentialsForm.userNameText.Text = allServersForm.cachedUsername;
                addCredentialsForm.passWordText.Text = allServersForm.cachedPassword;
                addCredentialsForm.credentialsHeadingLabel.Text = "Provide credentials " + _selectedHost.displayname;
                if (_selectedCredentialsForOneShot == true)
                {
                    addCredentialsForm.useForAllCheckBox.Enabled = false;
                }
                else
                {
                    addCredentialsForm.useForAllCheckBox.Enabled = true;
                }
                addCredentialsForm.ShowDialog();


                if (addCredentialsForm.canceled == true)
                {
                    allServersForm.pushAgentDataGridView.Rows[_credentialIndex].Cells[PUSHAGENT_COLUMN].Value = false;
                    allServersForm.pushAgentDataGridView.RefreshEdit();
                    Debug.WriteLine("GetCredentials: Returning false");
                    return false;
                }
                else
                {
                    if (addCredentialsForm.useForAllCheckBox.Checked == true || _selectedCredentialsForOneShot == true)
                    {

                        foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                        {
                            if (h.checkedCredentials == false)
                            {
                                h.domain = addCredentialsForm.domain;

                                if (h.domain.Length == 0)
                                {
                                    if (h.hostname != null)
                                    {
                                        hostname = h.hostname.Split('.')[0];
                                        if (hostname.Length > 15)
                                        {
                                            hostname = hostname.Substring(0, 15);
                                        }
                                        h.domain = hostname;
                                    }
                                }
                                h.userName = addCredentialsForm.userName;
                                h.passWord = addCredentialsForm.passWord;
                            }
                        }
                        foreach (Host h in allServersForm.selectedMasterListbyUser._hostList)
                        {
                            if (h.checkedCredentials == false)
                            {
                                h.domain = addCredentialsForm.domain;
                                if (h.domain.Length == 0)
                                {
                                    if (h.hostname != null)
                                    {
                                        hostname = h.hostname.Split('.')[0];
                                        if (hostname.Length > 15)
                                        {
                                            hostname = hostname.Substring(0, 15);
                                        }
                                        h.domain = hostname;
                                    }
                                }
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

        internal bool BackGroundWorkerForSourceCredentialCheck(AllServersForm allServersForm)
        {
            // Checking that the given credentials are correct or not using wmi
            // Once wmi checking is done we will connect through the mapnetwork drive....
            // one map network dirve succed we will detach the same drive.....
            // If wmi fails we will try two times and through the pop to skip or re -try with new credentials....
            Trace.WriteLine(DateTime.Now + " \t Entered: BackGroundWorkerForSourceCredentialCheck of the Esx_AddSourcePanelHandler  ");
            string hostname = "";
            try
            {
                string error = "";
                bool ipReachable = false;
                string hostIP = null;
                Trace.WriteLine(DateTime.Now + " \t Reached here to verify the given credentials are correct or not  " + _selectedHost.displayname);
                //MessageBox.Show("Domain : " + _selectedHost.ip + _selectedHost.userName + _selectedHost.passWord);
                Trace.WriteLine(DateTime.Now + " \t Printing the hostname " + _selectedHost.hostname.Split('.')[0] + Environment.NewLine);
                if (_selectedHost.hostname.Split('.')[0].ToUpper() != Environment.MachineName.ToUpper())
                {
                    if (_selectedHost.natIPAddress == null)
                    {
                        if (_selectedHost.iptoPush == null)
                        {
                            if (_selectedHost.ipCount > 1)
                            {
                                //Handling multiple ip addresses. A host can have couple of private of ip addresses and public ip addresses. This is for assigning right ip address to _selectedHost.Ip parameter

                                foreach (string ip in _selectedHost.IPlist)
                                {

                                    Trace.WriteLine(DateTime.Now + " \t Trying to reach " + _selectedHost.displayname + " on " + ip);
                                    if (WinPreReqs.IPreachable(ip) == true)
                                    {
                                        _selectedHost.ip = ip;
                                        hostIP = ip;
                                        Trace.WriteLine(DateTime.Now + " \t Successfully reached " + _selectedHost.displayname + " on " + ip);
                                        ipReachable = true;
                                        break;
                                    }
                                }

                                if (ipReachable == false)
                                {
                                    string ip = "";
                                    if (_selectedHost.ip.Contains(","))
                                    {
                                        ip = _selectedHost.ip.Replace(",", "&");
                                    }
                                    else
                                    {
                                        ip = _selectedHost.ip;
                                    }
                                    error = Environment.NewLine + _selectedHost.displayname + " is not reachable. Please check that guest level firewall is not blocking ";
                                    string message = _selectedHost.displayname + " is not reachable on below IP Addresses" + Environment.NewLine + ip + Environment.NewLine + "Please check that guest level firewall is not blocking ";
                                    MessageBox.Show(message, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                    _wmiErrorCheckCanceled = true;
                                    _selectedHost.pushAgent = false;
                                    return false;

                                }

                            }
                            else
                            {
                                hostIP = _selectedHost.ip;
                            }
                        }
                        else
                        {
                            Trace.WriteLine(DateTime.Now + "\t User selected to push with this ip " + _selectedHost.iptoPush);
                            hostIP = _selectedHost.iptoPush;
                        }

                    }
                    else
                    {
                        hostIP = _selectedHost.natIPAddress;

                    }
                }
                else
                {
                    hostIP = ".";
                }
                _credentialsCheckPassed = false;
                if (WinPreReqs.WindowsShareEnabled(hostIP, _selectedHost.domain, _selectedHost.userName, _selectedHost.passWord,_selectedHost.hostname,  error) == false)
                {

                    //Try once again before failing
                    if (WinPreReqs.WindowsShareEnabled(hostIP, _selectedHost.domain, _selectedHost.userName, _selectedHost.passWord, _selectedHost.hostname, error) == false)
                    {
                        // error = error + Environment.NewLine + Environment.NewLine + "You can still proceed with this error. How ever, automatic remote agent installation and validation checks will be skipped";
                        // Environment.NewLine + Environment.NewLine + "Do you want to proceed ?";
                        error = WinPreReqs.GetError;
                        WmiErrorMessageForm errorMessageForm = new WmiErrorMessageForm(error);
                        errorMessageForm.domainText.Text = allServersForm.cachedDomain;
                        errorMessageForm.userNameText.Text = allServersForm.cachedUsername;
                        errorMessageForm.passWordText.Text = allServersForm.cachedPassword;
                        errorMessageForm.ShowDialog();
                        //errorMessageForm.BringToFront();                


                        if (errorMessageForm.canceled == false)
                        {
                            if (errorMessageForm.continueWithOutValidation == true)
                            {
                                _selectedHost.skipPushAndPreReqs = true;
                                _wmiErrorCheckCanceled = false;
                                _selectedHost.pushAgent = false;
                                // allServersForm._selectedSourceList.AddOrReplaceHost(_selectedHost);
                                _credentialsCheckPassed = true;
                            }
                            else
                            {
                                foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                                {

                                    if (h.checkedCredentials == false)
                                    {
                                        if (h.userName != null)
                                        {
                                            h.userName = errorMessageForm.username;
                                            h.passWord = errorMessageForm.password;
                                            h.domain = errorMessageForm.domain;
                                           
                                            if (h.domain.Length == 0)
                                            {
                                                if (h.hostname != null)
                                                {
                                                    hostname = h.hostname.Split('.')[0];
                                                    if (hostname.Length > 15)
                                                    {
                                                        hostname = hostname.Substring(0, 15);
                                                    }
                                                    h.domain = hostname;
                                                }
                                            }
                                            
                                        }
                                    }
                                    Debug.WriteLine(h.userName);
                                }
                                _wmiErrorCheckCanceled = false;
                                allServersForm.cachedDomain = errorMessageForm.domain;
                                allServersForm.cachedUsername = errorMessageForm.username;
                                allServersForm.cachedPassword = errorMessageForm.password;



                                _selectedHost.domain = errorMessageForm.domain;
                                _selectedHost.userName = errorMessageForm.username;
                                _selectedHost.passWord = errorMessageForm.password;

                                _credentialsCheckPassed = false;
                            }
                        }
                        if (errorMessageForm.canceled == true)
                        {
                            _wmiErrorCheckCanceled = true;
                            _selectedHost.pushAgent = false;
                            foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                            {

                                if (h.checkedCredentials == false)
                                {
                                    h.userName = null;
                                    h.passWord = null;
                                    h.domain = null;
                                }
                                Debug.WriteLine(h.userName);

                            }


                        }

                        Trace.WriteLine(DateTime.Now + " \t Exiting: BackGroundWorkerForSourceCredentialCheck  because of user selecteed cancel the Esx_AddSourcePanelHandler  ");


                        return false;
                    }
                    else
                    {
                        _wmiErrorCheckCanceled = false;
                        _selectedHost.checkedCredentials = true;
                        _credentialsCheckPassed = true;
                        _selectedHost.skipPushAndPreReqs = false;
                    }

                }
                else
                {
                    _wmiErrorCheckCanceled = false;
                    _credentialsCheckPassed = true;
                    _selectedHost.checkedCredentials = true;
                    _selectedHost.skipPushAndPreReqs = false;
                    // allServersForm._selectedSourceList.AddOrReplaceHost(_selectedHost);
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
            Trace.WriteLine(DateTime.Now + " \t Exiting: BackGroundWorkerForSourceCredentialCheck of the Esx_AddSourcePanelHandler  ");

            return true;
        }



        internal bool PostCredentialsCheck(AllServersForm allServersForm)
        {

            // Once the credentials check is completed it will come here we will check here
            // If it is succed or we need to re-run the credentials once again or not....
            Trace.WriteLine(DateTime.Now + " \t Entered: PostCredentialsCheck of the Esx_AddSourcePanelHandler  ");
            try
            {
                _backGroundWorkerIsCompleted = false;
                if (_wmiErrorCheckCanceled == false)
                {
                    if (_credentialsCheckPassed == true)
                    {

                        if (allServersForm.selectedSourceListByUser._hostList.Count > 0)
                        {
                            allServersForm.BringToFront();

                            allServersForm.generalLogTextBox.AppendText("Successfully completed checking credentials for " + _selectedHost.displayname + " (" + _selectedHost.ip + ")" + Environment.NewLine);

                        }
                    }
                    else
                    {
                        //Didn't pass the credentials check. Take credentials again from user.


                        // if (GetCredentials(allServersForm, ref _selectedHost.domain, ref _selectedHost.userName, ref _selectedHost.passWord) == true)
                        {
                            allServersForm.BringToFront();
                            allServersForm.nextButton.Enabled = false;
                            allServersForm.previousButton.Enabled = false;
                            // allServersForm.Enabled = false;
                            allServersForm.progressBar.Visible = true;
                            allServersForm.p2v_PushAgentPanel.Enabled = false;
                            _backGroundWorkerIsCompleted = true;
                            allServersForm.checkCredentialsBackgroundWorker.RunWorkerAsync();

                            return false;
                        }




                    }
                }
                else
                {
                    allServersForm.BringToFront();

                    allServersForm.pushAgentDataGridView.Rows[_credentialIndex].Cells[PUSHAGENT_COLUMN].Value = false;
                    allServersForm.pushAgentDataGridView.RefreshEdit();

                }


                if (_selectedHost.skipPushAndPreReqs == true)
                {
                    allServersForm.pushAgentDataGridView.Rows[_credentialIndex].Cells[PUSHAGENT_COLUMN].Value = false;
                    allServersForm.pushAgentDataGridView.RefreshEdit();
                }


                allServersForm.p2v_PushAgentPanel.Enabled = true;
                if (allServersForm.appNameSelected != AppName.Pushagent)
                {
                    allServersForm.nextButton.Enabled = true;
                }
                allServersForm.previousButton.Enabled = true;

                // In case of selct all at one shot we wil increase the inex and select the pushagent column one by
                // one and check the credentials for all the machines....

                if (_selectedCredentialsForOneShot == true)
                {
                    int i = 0;
                    Trace.WriteLine(DateTime.Now + " \t Printing the count of datagridview " + allServersForm.pushAgentDataGridView.RowCount.ToString() + "  " + _credentialIndex.ToString());
                    if (_credentialIndex < (allServersForm.pushAgentDataGridView.RowCount - 1))
                    {

                        i = _credentialIndex + 1;
                        allServersForm.pushAgentDataGridView.Rows[i].Cells[PUSHAGENT_COLUMN].Value = true;
                        SelectAllForPushOneShot(allServersForm, i);
                    }
                }
                Trace.WriteLine(DateTime.Now + " \t Exiting: PostCredentialsCheck of the Esx_AddSourcePanelHandler  ");
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



        internal bool SelectAllVmsForPush(AllServersForm allServersForm)
        {
            try
            {//This will be called when user  select all for push.......
                int i = 0;
                if (allServersForm.pushAgentCheckBox.Checked == true)
                {
                    Trace.WriteLine(DateTime.Now + " \t Came here to check the value of backgroundworker " + _backGroundWorkerIsCompleted.ToString());
                    _selectedCredentialsForOneShot = true;
                    while (!allServersForm.checkCredentialsBackgroundWorker.IsBusy)
                    {
                        allServersForm.pushAgentDataGridView.Rows[i].Cells[PUSHAGENT_COLUMN].Value = true;
                        if (SelectAllForPushOneShot(allServersForm, i) == false)
                        {
                            return false;
                        }
                    }
                }
                else
                {//This will be called when user unselect all for push.......
                    for (int j = 0; j < allServersForm.pushAgentDataGridView.RowCount; j++)
                    {
                        allServersForm.pushAgentDataGridView.Rows[j].Cells[PUSHAGENT_COLUMN].Value = false;
                        allServersForm.pushAgentDataGridView.RefreshEdit();
                    }
                    foreach (Host h in allServersForm.selectedMasterListbyUser._hostList)
                    {
                        h.pushAgent = false;
                    }
                    foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                    {
                        h.pushAgent = false;
                    }
                }
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

        internal bool SelectAllForPushOneShot(AllServersForm allServersForm, int rowIndex)
        {
            try
            {
                // This will be called by the postcredentailcheck when user selects for select all feature.....

                if (allServersForm.pushAgentDataGridView.RowCount > 0)
                {
                    Host h1 = new Host();
                    if (allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[IP_COLUMN].Value != null)
                    {
                        h1.ip = allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[IP_COLUMN].Value.ToString();
                        if (h1.ip.Contains("&"))
                        {
                            h1.ip = h1.ip.Replace("&", ",");
                            
                        }
                        else
                        {
                            h1.ip = allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[IP_COLUMN].Value.ToString();
                        }
                    }
                    if (allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[DISPLAY_NAME_COLUMN].Value != null)
                        h1.displayname = allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[DISPLAY_NAME_COLUMN].Value.ToString();
                    int listIndex = 0;
                    _credentialIndex = rowIndex;
                    // User has selected to push agent to this host
                    if ((bool)allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[PUSHAGENT_COLUMN].FormattedValue)
                    {
                        if (allServersForm.selectedSourceListByUser.DoesHostExist(h1, ref listIndex) == true)
                        {
                            _selectedHost = ((Host)allServersForm.selectedSourceListByUser._hostList[listIndex]);
                            ((Host)allServersForm.selectedSourceListByUser._hostList[listIndex]).pushAgent = true;
                            if (_selectedHost.hostname.Split('.')[0].ToUpper() != Environment.MachineName.ToUpper())
                            {
                                if (_selectedHost.userName == null)
                                {
                                    if (GetCredentials(allServersForm, ref _selectedHost.domain, ref _selectedHost.userName, ref _selectedHost.passWord) == false)
                                    {
                                        _selectedHost.pushAgent = false;
                                        allServersForm.pushAgentCheckBox.Checked = false;
                                        Debug.WriteLine("MasterVMSelected: Returning false");
                                        return false;
                                    }
                                }
                            }
                            allServersForm.progressBar.Visible = true;
                            allServersForm.p2v_PushAgentPanel.Enabled = false;
                            allServersForm.nextButton.Enabled = false;
                            allServersForm.previousButton.Enabled = false;
                            Trace.WriteLine(DateTime.Now + " \t Printing the current value of backgroundworker " + _backGroundWorkerIsCompleted.ToString());
                            while (_backGroundWorkerIsCompleted == false)
                            {
                                _backGroundWorkerIsCompleted = true;
                                allServersForm.checkCredentialsBackgroundWorker.RunWorkerAsync();
                                // allServersForm.generalLogTextBox.AppendText("Checking credentials for " + _selectedHost.displayname + " (" + _selectedHost.ip + ")" + Environment.NewLine);
                            }
                        }
                    }
                    else  // Push agent is not selected for this host
                    {
                        if (allServersForm.selectedSourceListByUser.DoesHostExist(h1, ref listIndex))
                        {
                            ((Host)allServersForm.selectedSourceListByUser._hostList[listIndex]).pushAgent = false;
                            //allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[REBOOT_COLUMN].Value = false;
                            allServersForm.pushAgentDataGridView.RefreshEdit();
                        }
                    }
                    // User has selected to push agent  for mastet target
                    if ((bool)allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[PUSHAGENT_COLUMN].FormattedValue)
                    {
                        if (allServersForm.selectedMasterListbyUser.DoesHostExist(h1, ref listIndex) == true)
                        {
                            _selectedHost = ((Host)allServersForm.selectedMasterListbyUser._hostList[listIndex]);
                            ((Host)allServersForm.selectedMasterListbyUser._hostList[listIndex]).pushAgent = true;
                            if (_selectedHost.hostname.Split('.')[0].ToUpper() != Environment.MachineName.ToUpper())
                            {
                                if (_selectedHost.userName == null)
                                {
                                    if (GetCredentials(allServersForm, ref _selectedHost.domain, ref _selectedHost.userName, ref _selectedHost.passWord) == false)
                                    {
                                        _selectedHost.pushAgent = false;
                                        allServersForm.pushAgentCheckBox.Checked = false;
                                        Debug.WriteLine("MasterVMSelected: Returning false");
                                        return false;
                                    }
                                }
                            }
                           
                            if (allServersForm.selectedMasterListbyUser.DoesHostExist(_selectedHost, ref listIndex) == true)
                            {
                                allServersForm.progressBar.Visible = true;
                                allServersForm.p2v_PushAgentPanel.Enabled = false;
                                allServersForm.nextButton.Enabled = false;
                                allServersForm.previousButton.Enabled = false;
                                Trace.WriteLine(DateTime.Now + " \t Printing the current value of backgroundworker " + _backGroundWorkerIsCompleted.ToString());
                                while (_backGroundWorkerIsCompleted == false)
                                {
                                    _backGroundWorkerIsCompleted = true;
                                    allServersForm.checkCredentialsBackgroundWorker.RunWorkerAsync();
                                    //allServersForm.generalLogTextBox.AppendText("Checking credentials for " + _selectedHost.displayname + " (" + _selectedHost.ip + ")" + Environment.NewLine);
                                }
                            }
                        }
                    }
                    else  // Push agent is not selected for master target
                    {
                        if (allServersForm.selectedMasterListbyUser.DoesHostExist(h1, ref listIndex))
                        {
                            ((Host)allServersForm.selectedMasterListbyUser._hostList[listIndex]).pushAgent = false;
                            //allServersForm.pushAgentDataGridView.Rows[rowIndex].Cells[REBOOT_COLUMN].Value = false;
                            allServersForm.pushAgentDataGridView.RefreshEdit();
                        }
                    }
                }
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

        internal bool UnInstallAgents(AllServersForm allServersForm)
        {
            try
            {
                // This will called when user selects the uninstall buttons
                // Here we will check whether the agents are installed on that machine by agents status
                // If agents are there we will uninstall agnets......
                string binaryPath;
                bool selectedForPush = false;
                string directory, fileName;
                
                if (File.Exists(_installPath + "\\" + UNIFIED_AGENT_NAME))
                {
                    binaryPath = _installPath + "\\" + UNIFIED_AGENT_NAME;
                }
                else
                {
                    
                        MessageBox.Show("Unified agent is missing in vContinuum installation path", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return false;
                   
                }
                directory = Path.GetDirectoryName(binaryPath);
                fileName = Path.GetFileName(binaryPath);
                PushInstall pushObj = new PushInstall();
                _totalVmsSelectedForPush = 0;
                _totalVmsCompletedSuccessfully = 0;
                _totalFailed = 0;
                foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                {
                    if (h.pushAgent == true)
                    {
                        _totalVmsSelectedForPush++;
                        h.unInstall = true;
                        selectedForPush = true;
                    }
                    else
                    {
                        h.unInstall = false;
                    }
                }
                foreach (Host h in allServersForm.selectedMasterListbyUser._hostList)
                {
                    if (h.pushAgent == true)
                    {
                        _totalVmsSelectedForPush++;
                        h.unInstall = true;
                        selectedForPush = true;
                    }
                    else
                    {
                        h.unInstall = false;
                    }
                }
                if (selectedForPush == false)
                {
                    MessageBox.Show("Please select VMs to install agents", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return false;
                }
                tickerDelegate = new TickerDelegate(SetLabelText);
                _label = allServersForm.pushAgentStatsLabel;
                _message = "Number of servers selected:" + _totalVmsSelectedForPush;
                allServersForm.pushAgentStatsLabel.Invoke(tickerDelegate, new object[] { _message });
                _label = allServersForm.pushagentSuccessLabel;
                _message = "Success: " + _totalVmsCompletedSuccessfully.ToString();
                allServersForm.pushagentSuccessLabel.Invoke(tickerDelegate, new object[] { _message });
                _label = allServersForm.pushAgentFailedLabel;
                _message = "Failed: " + _totalFailed;
                allServersForm.pushAgentFailedLabel.Invoke(tickerDelegate, new object[] { _message });
                foreach (Host h in allServersForm.selectedMasterListbyUser._hostList)
                {
                    tickerDelegate = new TickerDelegate(SetLeftTicker);
                    if (h.unInstall == true)
                    {
                        _message = "Checking whether agent is present or not on:" + h.ip;
                        allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                        if (PushInstall.IsAgentExists(h.ip, h.domain, h.userName, h.passWord,h.hostname) == 1)
                        {
                            _message = "....Not Installed." + Environment.NewLine;
                            allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });

                        }
                        else
                        {
                            _message = ".... AlreadyInstalled." + Environment.NewLine;
                            allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                            _message = "Uninstalling agent from " + h.ip;
                            allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                            int returncode = 0;
                            if (h.hostname.Split('.')[0].ToUpper() != Environment.MachineName.ToUpper())
                            {
                                returncode = pushObj.UninstallAgent(h.ip, h.domain, h.userName, h.passWord, directory, fileName, h);
                            }
                            else
                            {
                                returncode = pushObj.UninstallAgent(".", h.domain, h.userName, h.passWord, directory, fileName, h);
                            }

                           
                            if (returncode == PushInstall.RETURN_CODE_CERT_TRUST_VERIFICATION_FAILED)
                            {
                                if (WinTools.VerifyUnifiedAgentCertificate() == false)
                                {
                                    _message = Environment.NewLine + "Found that UnifiedAgent.exe in vContinuum installation is not trusted binary" + Environment.NewLine;
                                    allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                                    return false;
                                }
                            }
                            else if (returncode == PushInstall.RETURN_CODE_CERT_ROOT_IS_NOT_MS_CA)
                            {
                                if (WinTools.VerifyUnifiedAgentCertificate() == false)
                                {
                                    _message = Environment.NewLine + "Found that UnifiedAgent.exe in vContinuum installation is not Microsoft certified binary" + Environment.NewLine;
                                    allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                                    return false;
                                }

                            }
                            _message = "....Done" + Environment.NewLine;
                            allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                        }
                        _totalVmsCompletedSuccessfully++;
                    }
                }
                foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                {
                    tickerDelegate = new TickerDelegate(SetLabelText);
                    _label = allServersForm.pushagentSuccessLabel;
                    _message = "Success: " + _totalVmsCompletedSuccessfully.ToString();
                    allServersForm.pushagentSuccessLabel.Invoke(tickerDelegate, new object[] { _message });
                    _label = allServersForm.pushAgentFailedLabel;
                    _message = "Failed:" + _totalFailed;
                    allServersForm.pushAgentFailedLabel.Invoke(tickerDelegate, new object[] { _message });
                    tickerDelegate = new TickerDelegate(SetLeftTicker);
                    if (h.unInstall == true)
                    {
                        _message = "Checking whether agent is present or not on:" + h.ip;
                        allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                        if (PushInstall.IsAgentExists(h.ip, h.domain, h.userName, h.passWord,h.hostname) == 1)
                        {
                           _message =  "....Not Installed." + Environment.NewLine;
                           allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                        }
                        else
                        {
                            _message = ".... Already Installed." + Environment.NewLine;
                            allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                            _message = "Uninstalling agent from " + h.ip ;
                            allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                            int returncode = 0;
                            if (h.hostname.Split('.')[0].ToUpper() != Environment.MachineName.ToUpper())
                            {
                                returncode = pushObj.UninstallAgent(h.ip, h.domain, h.userName, h.passWord, directory, fileName, h);
                            }
                            else
                            {
                                returncode = pushObj.UninstallAgent(".", h.domain, h.userName, h.passWord, directory, fileName, h);
                            }

                            if (returncode == PushInstall.RETURN_CODE_CERT_TRUST_VERIFICATION_FAILED)
                            {
                                if (WinTools.VerifyUnifiedAgentCertificate() == false)
                                {
                                    _message = Environment.NewLine + "Found that UnifiedAgent.exe in vContinuum installation is not trusted binary" + Environment.NewLine;
                                    allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                                    return false;
                                }
                            }
                            else if (returncode == PushInstall.RETURN_CODE_CERT_ROOT_IS_NOT_MS_CA)
                            {
                                if (WinTools.VerifyUnifiedAgentCertificate() == false)
                                {
                                    _message = Environment.NewLine + "Found that UnifiedAgent.exe in vContinuum installation is not Microsoft certified binary" + Environment.NewLine;
                                    allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });
                                    return false;
                                }

                            }
                            _message = "....Done" + Environment.NewLine;
                            allServersForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { _message });                            
                        }
                        _totalVmsCompletedSuccessfully++;
                    }
                }
                tickerDelegate = new TickerDelegate(SetLabelText);
                _label = allServersForm.pushagentSuccessLabel;
                _message = "Success " + _totalVmsCompletedSuccessfully.ToString();
                allServersForm.pushagentSuccessLabel.Invoke(tickerDelegate, new object[] { _message });
                _label = allServersForm.pushAgentFailedLabel;
                _message = "Failed " + _totalFailed;
                allServersForm.pushAgentFailedLabel.Invoke(tickerDelegate, new object[] { _message });
                return true;
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
        }
    }
}

