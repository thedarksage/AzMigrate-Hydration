using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;
using System.Diagnostics;
using com.InMage;
using com.InMage.Tools;
using com.InMage.ESX;
using System.Collections;
using System.Management;
using System.Globalization;
using System.IO;

namespace com.InMage.Wizard
{   
    public partial class RecoveryForm : Form
    {
        public string MASTER_FILE;
        public string RECOVERY_HOSTFILE = "recovery_hostfile.txt";
        public HostList _sourceList;
        public  Host _masterHost;
        
        public string _esxIp ;
        public string _targetEsxUserName;
        public string _targetEsxPassword;
        public string _installPath;
        private int _taskListIndex = 1;
        public string _currentDisplayName;
        public HostList _finalSourceList            = new HostList();
        public HostList _selectedRecoverList = new HostList();
        public HostList _recoverChecksPassedList = new HostList();
         int _listIndex = 0;
         public int _index = 0;
        public string _cxIp ;
        ArrayList _tagList                          = new ArrayList();
        public ArrayList _panelList                 = new ArrayList();
        public ArrayList _panelHandlerList          = new ArrayList();
        ArrayList _pictureBoxList                   = new ArrayList();
        public Esx _esx = new Esx();
        System.Drawing.Bitmap _currentTask;
        System.Drawing.Bitmap _completeTask;
        int _closeForm = 0;
        public OsType _osType;
        public string _planName = "";
        public string masterTargetDisplayName = "";
        public string _universalTime = "";
        public bool _slideOpen = false;
        public bool _someMtUsingFile = false;
        public static bool SUPPRESSMSGBOXES = false;
       
      
       
        public RecoveryForm()
        {
            InitializeComponent();
            try
            {
               
                _tagList.Add("FS");
                _tagList.Add("UserDefined");

                dateTimePicker.Format = DateTimePickerFormat.Custom;
                specificTimeDateTimePicker.Format = DateTimePickerFormat.Custom;
                DateTime dt = DateTime.Now;


                dateTimePicker.CustomFormat = "yyyy/MM/dd H:mm:ss";
                specificTimeDateTimePicker.CustomFormat = "yyyy/MM/dd H:mm:ss";
                if (ResumeForm._selectedAction == "ESX")
                {
                    osTypeComboBox.Items.Add("Windows");
                    osTypeComboBox.Items.Add("Linux");

                }

                else
                {
                    osTypeComboBox.Items.Add("Windows");

                }
                osTypeComboBox.SelectedItem = osTypeComboBox.Items[0];
                // tagComboBox.DataSource = _tagList;

                _installPath = WinTools.fxAgentPath() + "\\vContinuum";

                _panelList.Add(recoverPanel);
                _panelList.Add(configurationPanel);
                _panelList.Add(reviewPanel);

                Esx_RecoverPanelHandler recoveryPanelHandler = new Esx_RecoverPanelHandler();
                ReviewPanelHandler reviewPanelHandler = new ReviewPanelHandler();
                RecoverConfigurationPanel configurationPanelHandler = new RecoverConfigurationPanel();

                _panelHandlerList.Add(recoveryPanelHandler);
                _panelHandlerList.Add(configurationPanelHandler);
                _panelHandlerList.Add(reviewPanelHandler);

                _pictureBoxList.Add(recoverPictureBox);
                _pictureBoxList.Add(configurationPictureBox);
                _pictureBoxList.Add(reviewPictureBox);

                _currentTask = Wizard.Properties.Resources.arrow;
                _completeTask = Wizard.Properties.Resources.doneIcon;
                startApp();
                versionLabel.Text = HelpForCx.VERSION_NUMBER;
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

        private void startApp()
        {
           
            if (_panelList.Count != _panelHandlerList.Count)
            {
                Trace.WriteLine(DateTime.Now + "\t ImportOfflineSyncForms: startApp: Panel Handler count it not matching with Panels.");
         
            }

            RecoveryPanelHandler recoveryPanelHandler = (RecoveryPanelHandler)_panelHandlerList[_index];

            recoveryPanelHandler.Initialize(this);

           


            ((System.Windows.Forms.Panel)_panelList[_index]).BringToFront();


        }

        private void cancelButton_Click(object sender, EventArgs e)
        {

            this.Close();
           
        }

        private void getCredentialsButton_Click(object sender, EventArgs e)
        {
            AddCredentialsPopUpForm addCredentials = new AddCredentialsPopUpForm();

            addCredentials.ShowDialog();
        }

       

        private void nextButton_Click(object sender, EventArgs e)
        {
            try
            {
                if (_index < (_panelList.Count))
                {

                    {
                        RecoveryPanelHandler recoveryPanelHandler = (RecoveryPanelHandler)_panelHandlerList[_index];

                        if (recoveryPanelHandler.ValidatePanel(this) == true)
                        {
                            if (recoveryPanelHandler.ProcessPanel(this) == true)
                            {


                                //Move to next panel if all states of current panel are done
                                if (recoveryPanelHandler.CanGoToNextPanel(this) == true)
                                {
                                    //Update the task progress on left side
                                    ((System.Windows.Forms.PictureBox)_pictureBoxList[_taskListIndex - 1]).Visible = true;
                                    if (_index < (_panelList.Count - 1))
                                    {
                                        ((System.Windows.Forms.PictureBox)_pictureBoxList[_taskListIndex - 1]).Image = _completeTask;
                                        ((System.Windows.Forms.PictureBox)_pictureBoxList[_taskListIndex]).Visible = true;
                                        ((System.Windows.Forms.PictureBox)_pictureBoxList[_taskListIndex]).Image = _currentTask;

                                        ((System.Windows.Forms.Panel)_panelList[_index]).SendToBack();
                                        _taskListIndex++;
                                        _index++;

                                        Console.WriteLine(" current index is :" + _index);
                                        recoveryPanelHandler = (RecoveryPanelHandler)_panelHandlerList[_index];
                                        recoveryPanelHandler.Initialize(this);
                                        ((System.Windows.Forms.Panel)_panelList[_index]).BringToFront();
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

      
        }

        private void doneButton_Click(object sender, EventArgs e)
        {
          
            this.Close();
        }


        public bool Validate1()
        {

            try
            {
                if (targetEsxIPText.Text.Trim().Length < 5)
                {
                    MessageBox.Show("Please enter IP", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return false;
                }
                if (targetEsxUserNameTextBox.Text.Trim().Length < 1)
                {
                    errorProvider.SetError(targetEsxUserNameTextBox, "Please enter username");
                    MessageBox.Show("Please enter Username", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return false;
                }
                else
                {
                    errorProvider.Clear();
                }

                if (targetEsxPasswordTextBox.Text.Trim().Length < 1)
                {
                    errorProvider.SetError(targetEsxPasswordTextBox, "Please enter Password");
                    MessageBox.Show("Please Enter Password", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return false;
                }
                else
                {
                    errorProvider.Clear();
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

        private void targetEsxPasswordTextBox_Enter(object sender, EventArgs e)
        {
            

        }

        private void getDetailsButton_Click(object sender, EventArgs e)
        {
           
            
        }

      
        private void recoverDataGridView_CellValueChanged(object sender, DataGridViewCellEventArgs e)
        {
           
            
        }

        private void dnsText_TextChanged(object sender, EventArgs e)
        {

           // Console.WriteLine("printing the text of iptext box ");

            
        }



   

        private void recoverDataGridView_CellClick(object sender, DataGridViewCellEventArgs e)
        {
           
               

        }

        private void dnsText_KeyUp(object sender, KeyEventArgs e)
        {
          //  RecoveryChanges();
            

        }

        private void defaultGateWayText_KeyUp(object sender, KeyEventArgs e)
        {
           // RecoveryChanges();
        }

        private void subnetMaskText_KeyUp(object sender, KeyEventArgs e)
        {
           // RecoveryChanges();
        }

        private void ipText_KeyUp(object sender, KeyEventArgs e)
        {
           // RecoveryChanges();
        }

        private void recoveryTagText_KeyUp(object sender, KeyEventArgs e)
        {
           // RecoveryChanges();
        }

        private void tagComboBox_SelectionChangeCommitted(object sender, EventArgs e)
        {
           // RecoveryChanges();
        }

       

       

        private void previousButton_Click(object sender, EventArgs e)
        {
            try
            {
                RecoveryPanelHandler panelHandler;
                if (_index > 0)
                {

                    if (_taskListIndex > 0)
                    {

                        panelHandler = (RecoveryPanelHandler)_panelHandlerList[_index];


                        if (panelHandler.CanGoToPreviousPanel(this) == true)
                        {
                            ((System.Windows.Forms.Panel)_panelList[_index]).SendToBack();

                            _index--;
                            _taskListIndex--;

                            ((System.Windows.Forms.Panel)_panelList[_index]).BringToFront();
                            panelHandler = (RecoveryPanelHandler)_panelHandlerList[_index];
                            ((System.Windows.Forms.PictureBox)_pictureBoxList[_taskListIndex]).Visible = false;
                            ((System.Windows.Forms.PictureBox)_pictureBoxList[_taskListIndex - 1]).Image = _currentTask;

                            Debug.WriteLine(" current index is :" + _index);
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

        private void p2vRecoverDataGridView_CurrentCellDirtyStateChanged(object sender, EventArgs e)
        {

        }

        private void RecoveryForm_FormClosing(object sender, FormClosingEventArgs e)
        {
            try
            {
                if (_closeForm == 0)
                {

                    if (cancelButton.Visible == true)
                    {
                        switch (MessageBox.Show("Are you sure you want to exit?", "vContinuum", MessageBoxButtons.YesNo, MessageBoxIcon.Question))
                        {
                            case DialogResult.Yes:
                                FindAndKillProcess();
                                _closeForm = 1;
                                this.Close();

                                break;
                            case DialogResult.No:
                                e.Cancel = true;

                                break;

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

        private void recoveryDetailsBackgroundWorker_DoWork(object sender, DoWorkEventArgs e)
        {
            
        }

        private void recoveryDetailsBackgroundWorker_ProgressChanged(object sender, ProgressChangedEventArgs e)
        {
           
        }

        private void recoveryDetailsBackgroundWorker_RunWorkerCompleted(object sender, RunWorkerCompletedEventArgs e)
        {
            
        }

       
        
        private void recoveryReviewDataGridView_CellValidating(object sender, DataGridViewCellValidatingEventArgs e)
        {
            try
            {
                if (e.RowIndex > 0)
                {
                    if (e.ColumnIndex == recoveryReviewDataGridView.Columns[ReviewPanelHandler.RECOVERY_ORDER_COLUMN].Index) //this is our numeric column
                    {
                        int i;
                        if (!int.TryParse(Convert.ToString(e.FormattedValue), out i))
                        {
                            e.Cancel = true;
                            MessageBox.Show("Recovery order should be in numeric.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
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

        private void recoveryBackgroundWorker_DoWork(object sender, DoWorkEventArgs e)
        {
            
        }

        private void recoveryBackgroundWorker_ProgressChanged(object sender, ProgressChangedEventArgs e)
        {
            
        }

        private void recoveryBackgroundWorker_RunWorkerCompleted(object sender, RunWorkerCompletedEventArgs e)
        {
            

        }

        private void recoverLabel_Click(object sender, EventArgs e)
        {

        }

        private void credentialCheckBackgroundWorker_DoWork(object sender, DoWorkEventArgs e)
        {
            
        }

        private void credentialCheckBackgroundWorker_ProgressChanged(object sender, ProgressChangedEventArgs e)
        {
          
            
        }

        private void credentialCheckBackgroundWorker_RunWorkerCompleted(object sender, RunWorkerCompletedEventArgs e)
        {
           
            
        }

        private void recoverDataGridView_CurrentCellDirtyStateChanged(object sender, EventArgs e)
        {
            if (recoverDataGridView.RowCount > 0)
            {

                recoverDataGridView.CommitEdit(DataGridViewDataErrorContexts.Commit);
            }

        }

        private void preReqsButton_Click(object sender, EventArgs e)
        {
        }

        private void planNameText_KeyUp(object sender, KeyEventArgs e)
        {
            if (planNameText.Text.Length > 0)
            {
                errorProvider.Clear();
            }
        }

        private void tagBasedRadioButton_CheckedChanged(object sender, EventArgs e)
        {
           
        }

        private void timeBasedRadioButton_CheckedChanged(object sender, EventArgs e)
        {
            
        }

        private void userDefinedTimeRadioButton_CheckedChanged(object sender, EventArgs e)
        {
           
        }

      

        private void dateTimePicker_CloseUp(object sender, EventArgs e)
        {
        
           

        }

        private void dateTimePicker_Leave(object sender, EventArgs e)
        {
           
          

        }

        private void clearLogLinkLabel_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
        {
            recoveryText.Text = null;
        }

        private void helpPictureBox_Click(object sender, EventArgs e)
        {
           
        }

        private void closePictureBox_Click(object sender, EventArgs e)
        {
            try
            {
                helpPanel.Visible = false;
                _slideOpen = false;
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

        private void esxProtectionLinkLabel_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
        {
            try
            {
                //Process.Start("http://" + WinTools.getCxIpWithPortNumber() + "/help/Content/ESX Solution/Recover ESX.htm");
                if (File.Exists(_installPath + "\\Manual.chm"))
                {
                    Help.ShowHelp(null, _installPath + "\\Manual.chm", HelpNavigator.TopicId, "70");
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

        private void helpPictureBox_Click_1(object sender, EventArgs e)
        {
            try
            {
                
                
                if (_slideOpen == false)
                {

                    helpPanel.BringToFront();
                    helpPanel.Visible = true;
                    _slideOpen = true;
                }
                else
                {
                    _slideOpen = false;
                    helpPanel.Visible = false;
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

        private void monitiorLinkLabel_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
        {
            try
            {
                //MonitorScreenForm monitorScreenForm = new MonitorScreenForm();
                // monitorScreenForm.ShowDialog();
                Process.Start("http://" + WinTools.getCxIpWithPortNumber() + "/ui");
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

        private void selectAllCheckBoxForRecovery_CheckedChanged(object sender, EventArgs e)
        {
            
        }

        private void sourceConfigurationTreeView_AfterSelect(object sender, TreeViewEventArgs e)
        {
            if (e.Node.IsSelected == true)
            {
                Trace.WriteLine(DateTime.Now + " \t printing the values of the displayname " + e.Node.Text);
                RecoverConfigurationPanel configurationPanel = (RecoverConfigurationPanel)_panelHandlerList[_index];
                //configurationPanel.SelectedVM(this, e.Node.Text);
            }
        }

        private void networkDataGridView_CellClick(object sender, DataGridViewCellEventArgs e)
        {
            if (e.RowIndex >= 0)
            {
                if (configurationScreen.networkDataGridView.Rows[e.RowIndex].Cells[ConfigurationPanelHandler.CHANGE_VALUE_COLUMN].Selected == true)
                {
                    if (configurationScreen.networkDataGridView.Rows[e.RowIndex].Cells[ConfigurationPanelHandler.NIC_NAME_COLUMN].Value != null)
                    {
                        string nicName = configurationScreen.networkDataGridView.Rows[e.RowIndex].Cells[ConfigurationPanelHandler.NIC_NAME_COLUMN].Value.ToString();
                        RecoverConfigurationPanel config = (RecoverConfigurationPanel)_panelHandlerList[_index];
                        
                       // config.NetworkSetting(this, nicName);
                    }
                }
            }
        }
        private void memoryValuesComboBox_SelectionChangeCommitted(object sender, EventArgs e)
        {
            RecoverConfigurationPanel config = (RecoverConfigurationPanel)_panelHandlerList[_index];
            //config.MemorySelectedinGBOrMB(this);
        }
        private void cpuCountComboBox_SelectionChangeCommitted(object sender, EventArgs e)
        {
            RecoverConfigurationPanel config = (RecoverConfigurationPanel)_panelHandlerList[_index];
            //config.SelectCpuCount(this);
        }
        private void memoryNumericUpDown_ValueChanged(object sender, EventArgs e)
        {
            RecoverConfigurationPanel config = (RecoverConfigurationPanel)_panelHandlerList[_index];
            //config.SelectedMemory(this);
        }
        private void hardWareSettingsCheckBox_CheckedChanged(object sender, EventArgs e)
        {
            RecoverConfigurationPanel config = (RecoverConfigurationPanel)_panelHandlerList[_index];
           // config.SelectAllCheckBoxOfHardWareSetting(this);
        }

        private void specificTimeRadioButton_CheckedChanged(object sender, EventArgs e)
        {
           
        }

        private void specificTimeDateTimePicker_CloseUp(object sender, EventArgs e)
        {
           
        }

        private void specificTimeDateTimePicker_Leave(object sender, EventArgs e)
        {
            
        }
        public bool FindAndKillProcess()
        {
            bool didIkillAnybody = false;
            //finding current process id...
            System.Diagnostics.Process currentProcessOfvContinuum = new System.Diagnostics.Process();

            currentProcessOfvContinuum = System.Diagnostics.Process.GetCurrentProcess();
            Process[] processes = Process.GetProcesses();

            foreach (Process process in processes)
            {
                if (GetParentProcess(process.Id) == currentProcessOfvContinuum.Id)
                {
                    SUPPRESSMSGBOXES = true;
                    process.Kill();
                    didIkillAnybody = true;

                }
            }



            if (didIkillAnybody)
            {
                Trace.WriteLine(DateTime.Now + "\t\t Killed running child processes...");
                //CurrentPro.Kill();

            }
            else
            {

                Trace.WriteLine(DateTime.Now + "\t\t No child processes to kill..");
            }
            return true;
        }
        private int GetParentProcess(int Id)
        {
            int parentPid = 0;
            using (ManagementObject mo = new ManagementObject("win32_process.handle='" + Id.ToString() + "'"))
            {
                mo.Get();
                parentPid = Convert.ToInt32(mo["ParentProcessId"]);
            }
            return parentPid;
        }
    }
}
