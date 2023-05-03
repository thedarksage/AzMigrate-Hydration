using System;
using System.Collections.Generic;
using System.Collections;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;
using System.Diagnostics;
using System.Threading;

using System.Xml;
using System.IO;
using Com.Inmage.Esxcalls;
using Com.Inmage.Tools;
using System.Management;

namespace Com.Inmage.Wizard
{
    public partial class StatusForm : Form
    {
        private delegate void TickerDelegate(string s);
        TickerDelegate tickerDelegate;
        Button buttonStatus = new Button();
        Label labelStatus = new Label();
        DataGridView datagridViewStatus = new DataGridView();
        internal string powerOffList = null;
        internal bool powerOn = false;
        internal bool dummyDiskCreation = false;
        internal bool createGuestOnTargetSucceed = false;
        internal bool jobAutomationSucceed = false;
        internal bool vconMT = false;
        internal bool esxutilwinExecution = false;
        internal bool uploadfiletocx = false;
        internal bool getplanidapi = false;
        internal bool policyapi = false;
        internal bool statusApi = false;
        internal bool checkingForVmsPowerStatus = false;
        internal bool readinessCheck = false;
        internal bool recoveredVms = false;

        internal bool resizeSourceChecks = false;

        internal bool resizeTargetExtend = false;
        internal bool updateMasterConfigfile = false;
        internal bool executeEsxUtilWin = false;

        internal string planId = null;
        internal string scriptid = null;
        internal bool firsttime = false;
        string latestFilePath = null;
        string fxFailOverDataPath = null;
        string directoryNameinXmlFile = null;
        string logPath = null;
        internal bool postJobAutomationResult = false;
        internal bool calledByProtectButton = false;
        internal bool creatingScsiUnitsFile = false;
        Host _masterHost = new Host();
        Esx _esx = new Esx();
        internal HostList selectedSourceList = new HostList();
        AppName _appName;
        internal System.Drawing.Bitmap currentTask;
        internal System.Drawing.Bitmap completeTask;
        internal System.Drawing.Bitmap pending;
        internal System.Drawing.Bitmap failed;
        internal System.Drawing.Bitmap warning;
        internal int currentRow = 0;

        int row = 0;
        int column = 0;

        public StatusForm(HostList selectedList, Host masterHost, AppName appName, string plannameFolder)
        {
            try
            {
                selectedSourceList = selectedList;

                _appName = appName;
                latestFilePath = WinTools.FxAgentPath() + "\\vContinuum\\Latest\\";
                fxFailOverDataPath = WinTools.FxAgentPath() + "\\Failover\\Data";
                directoryNameinXmlFile = plannameFolder;
                _masterHost = masterHost;
                masterHost.directoryName = plannameFolder;
                currentTask = Wizard.Properties.Resources.PROCESS_PROCESSING;
                completeTask = Wizard.Properties.Resources.tick;
                pending = Wizard.Properties.Resources.pending;
                failed = Wizard.Properties.Resources.cross;
                warning = Wizard.Properties.Resources.warning;
                InitializeComponent();
               
                cancelButton.Enabled = false;
                protectButton.Visible = false;
                if (appName == AppName.Recover)
                {
                    this.Text = "Recovery Status";
                    statusLabel.Text = "Recovery Status";
                    protectButton.Text = "Recover";
                }
                else if (appName == AppName.Drdrill)
                {
                    this.Text = "DR Drill Status";
                    statusLabel.Text = "DR Drill Status";
                    protectButton.Text = "Start";
                }
                else if (appName == AppName.Removedisk)
                {
                    this.Text = "Remove disk from Protection";
                    statusLabel.Text = "Remove disk Status";
                    protectButton.Text = "Remove disk";
                }
                else if (appName == AppName.Removevolume)
                {
                    this.Text = "Remove volume from Protection";
                    statusLabel.Text = "Remove volume";
                    protectButton.Text = "Remove volume";
                }

                datagridViewStatus = statusDataGridView;
                labelStatus = warningLabel;
                buttonStatus = cancelButton;
                //statusDataGridView.BeginEdit(true);

                if (appName == AppName.Recover)
                {
                    statusDataGridView.Rows.Clear();
                    statusDataGridView.Rows.Add(1);
                    statusDataGridView.Rows.Add(1);

                    statusDataGridView.Rows[1].Cells[0].Value = pending;
                    statusDataGridView.Rows[1].Cells[1].Value = "Initializing Recovery Plan";

                    statusDataGridView.Rows.Add(1);
                    statusDataGridView.Rows[2].Cells[1].Value = "Downloading Configuration Files";
                    statusDataGridView.Rows[2].Cells[0].Value = pending;
                    statusDataGridView.Rows.Add(1);
                    statusDataGridView.Rows[3].Cells[1].Value = "Starting Recovery For Selected VM(s)";
                    statusDataGridView.Rows[3].Cells[0].Value = pending;


                    if (AllServersForm.v2pRecoverySelected == false)
                    {
                        statusDataGridView.Rows.Add(1);
                        statusDataGridView.Rows[4].Cells[1].Value = "Powering on the recovered VM(s)";
                        statusDataGridView.Rows[4].Cells[0].Value = pending;
                    }
                    else
                    {
                        statusDataGridView.Rows.Add(1);
                        statusDataGridView.Rows[4].Cells[1].Value = "Updating Configuration Files";
                        statusDataGridView.Rows[4].Cells[0].Value = pending;
                    }
                    statusDataGridView.Rows[0].Cells[0].Value = currentTask;
                    statusDataGridView.Rows[0].Cells[1].Value = "Uploading Configuration Files";

                }
                else if (appName == AppName.Drdrill)
                {
                    statusDataGridView.Rows.Clear();
                    statusDataGridView.Rows.Add(1);
                    statusDataGridView.Rows[0].Cells[0].Value = currentTask;
                    statusDataGridView.Rows[0].Cells[1].Value = "Provisioning recovery VM(s) on secondary server";
                    statusDataGridView.Rows.Add(1);
                    statusDataGridView.Rows[1].Cells[1].Value = "Initializing Dr-Drill Plan";
                    statusDataGridView.Rows[1].Cells[0].Value = pending;
                    statusDataGridView.Rows.Add(1);
                    statusDataGridView.Rows[2].Cells[1].Value = "Downloading Configuration Files";
                    statusDataGridView.Rows[2].Cells[0].Value = pending;
                    statusDataGridView.Rows.Add(1);
                    statusDataGridView.Rows[3].Cells[1].Value = "Preparing Master Target Disks";
                    statusDataGridView.Rows[3].Cells[0].Value = pending;
                    if (masterHost.os == OStype.Windows)
                    {
                        statusDataGridView.Rows.Add(1);
                        statusDataGridView.Rows[4].Cells[1].Value = "Updating Disk Layouts";
                        statusDataGridView.Rows[4].Cells[0].Value = pending;
                        statusDataGridView.Rows.Add(1);
                        statusDataGridView.Rows[5].Cells[1].Value = "Initializing Physical Snapshot Operation";
                        statusDataGridView.Rows[5].Cells[0].Value = pending;
                        statusDataGridView.Rows.Add(1);
                        statusDataGridView.Rows[6].Cells[1].Value = "Starting Physical Snapshot For Selected VM(s)";
                        statusDataGridView.Rows[6].Cells[0].Value = pending;
                        statusDataGridView.Rows.Add(1);
                        statusDataGridView.Rows[7].Cells[1].Value = "Powering on the dr drill VM(s)";
                        statusDataGridView.Rows[7].Cells[0].Value = pending;
                    }
                    else
                    {
                        statusDataGridView.Rows.Add(1);
                        statusDataGridView.Rows[4].Cells[1].Value = "Initializing Physical Snapshot Operation";
                        statusDataGridView.Rows[4].Cells[0].Value = pending;
                        statusDataGridView.Rows.Add(1);
                        statusDataGridView.Rows[5].Cells[1].Value = "Starting Physical Snapshot For Selected VM(s)";
                        statusDataGridView.Rows[5].Cells[0].Value = pending;
                        statusDataGridView.Rows.Add(1);
                        statusDataGridView.Rows[6].Cells[1].Value = "Powering on the dr drill VM(s)";
                        statusDataGridView.Rows[6].Cells[0].Value = pending;
                    }
                    statusDataGridView.Rows[0].Cells[0].Value = currentTask;
                    statusDataGridView.Rows[0].Cells[1].Value = "Provisioning recovery VM(s) on secondary server";
                }
                else if (appName == AppName.Resize)
                {
                    statusDataGridView.Rows.Clear();
                    statusDataGridView.Rows.Add(1);
                    statusDataGridView.Rows[0].Cells[0].Value = currentTask;
                    statusDataGridView.Rows[0].Cells[1].Value = "Comparing source machines and  recovery VM(s)";

                    statusDataGridView.Rows.Add(1);

                    statusDataGridView.Rows[1].Cells[1].Value = "Initializing volume resize protection plan";

                    statusDataGridView.Rows[1].Cells[0].Value = pending;


                    statusDataGridView.Rows.Add(1);
                    statusDataGridView.Rows[2].Cells[1].Value = "Downloading Configuration Files";
                    statusDataGridView.Rows[2].Cells[0].Value = pending;


                    statusDataGridView.Rows.Add(1);
                    statusDataGridView.Rows[3].Cells[1].Value = "Pausing the protection pairs";
                    statusDataGridView.Rows[3].Cells[0].Value = pending;

                    statusDataGridView.Rows.Add(1);
                    statusDataGridView.Rows[4].Cells[1].Value = "Preparing Master Target Disks";
                    statusDataGridView.Rows[4].Cells[0].Value = pending;
                    if (masterHost.os == OStype.Windows)
                    {
                        statusDataGridView.Rows.Add(1);
                        statusDataGridView.Rows[5].Cells[1].Value = "Updating Disk Layouts";
                        statusDataGridView.Rows[5].Cells[0].Value = pending;
                        statusDataGridView.Rows.Add(1);
                        statusDataGridView.Rows[6].Cells[1].Value = "Resuming the protection pairs";
                        statusDataGridView.Rows[6].Cells[0].Value = pending;
                    }
                    else
                    {
                        statusDataGridView.Rows.Add(1);
                        statusDataGridView.Rows[5].Cells[1].Value = "Resuming the protection pairs";
                        statusDataGridView.Rows[5].Cells[0].Value = pending;
                    }

                }
                else if (appName == AppName.Removedisk)
                {
                    statusDataGridView.Rows.Clear();
                    statusDataGridView.Rows.Add(1);
                    

                    statusDataGridView.Rows[0].Cells[0].Value = currentTask;
                    statusDataGridView.Rows[0].Cells[1].Value = "Preparing configuration files";

                    statusDataGridView.Rows.Add(1);
                    statusDataGridView.Rows[1].Cells[0].Value = pending;
                    statusDataGridView.Rows[1].Cells[1].Value = "Initializing remove replication pairs Plan";

                    statusDataGridView.Rows.Add(1);
                    statusDataGridView.Rows[2].Cells[1].Value = "Downloading Configuration Files";
                    statusDataGridView.Rows[2].Cells[0].Value = pending;
                    statusDataGridView.Rows.Add(1);
                    statusDataGridView.Rows[3].Cells[1].Value = "Deleting replication pairs";
                    statusDataGridView.Rows[3].Cells[0].Value = pending;
                    statusDataGridView.Rows.Add(1);
                    statusDataGridView.Rows[4].Cells[1].Value = "Detaching disks from MT and DR VM(s)";
                    statusDataGridView.Rows[4].Cells[0].Value = pending;

                }
                else if (appName == AppName.Removevolume)
                {
                    statusDataGridView.Rows.Clear();
                    statusDataGridView.Rows.Add(1);


                    statusDataGridView.Rows[0].Cells[0].Value = currentTask;
                    statusDataGridView.Rows[0].Cells[1].Value = "Preparing configuration files";

                    statusDataGridView.Rows.Add(1);
                    statusDataGridView.Rows[1].Cells[0].Value = pending;
                    statusDataGridView.Rows[1].Cells[1].Value = "Initializing remove replication pairs Plan";

                    statusDataGridView.Rows.Add(1);
                    statusDataGridView.Rows[2].Cells[1].Value = "Downloading Configuration Files";
                    statusDataGridView.Rows[2].Cells[0].Value = pending;
                    statusDataGridView.Rows.Add(1);
                    statusDataGridView.Rows[3].Cells[1].Value = "Deleting replication pairs";
                    statusDataGridView.Rows[3].Cells[0].Value = pending;
                   
                   
                }
                else
                {
                    statusDataGridView.Rows.Clear();
                    statusDataGridView.Rows.Add(1);
                    if (appName == AppName.V2P)
                    {
                        statusDataGridView.Rows[0].Cells[0].Value = currentTask;
                        statusDataGridView.Rows[0].Cells[1].Value = "Uploading Configuration Files to CX";
                    }
                    else
                    {
                        statusDataGridView.Rows[0].Cells[0].Value = currentTask;
                        statusDataGridView.Rows[0].Cells[1].Value = "Provisioning recovery VM(s) on secondary server";
                    }
                    statusDataGridView.Rows.Add(1);

                    statusDataGridView.Rows[1].Cells[1].Value = "Initializing Protection Plan";

                    statusDataGridView.Rows[1].Cells[0].Value = pending;
                    statusDataGridView.Rows.Add(1);
                    statusDataGridView.Rows[2].Cells[1].Value = "Downloading Configuration Files";
                    statusDataGridView.Rows[2].Cells[0].Value = pending;
                    statusDataGridView.Rows.Add(1);
                    statusDataGridView.Rows[3].Cells[1].Value = "Preparing Master Target Disks";
                    statusDataGridView.Rows[3].Cells[0].Value = pending;
                    if (masterHost.os == OStype.Windows)
                    {
                        statusDataGridView.Rows.Add(1);
                        statusDataGridView.Rows[4].Cells[1].Value = "Updating Disk Layouts";
                        statusDataGridView.Rows[4].Cells[0].Value = pending;
                        statusDataGridView.Rows.Add(1);
                        statusDataGridView.Rows[5].Cells[1].Value = "Activating Protection Plan";
                        statusDataGridView.Rows[5].Cells[0].Value = pending;
                    }
                    else
                    {
                        statusDataGridView.Rows.Add(1);
                        statusDataGridView.Rows[4].Cells[1].Value = "Activating Protection Plan";
                        statusDataGridView.Rows[4].Cells[0].Value = pending;
                    }
                    if (appName != AppName.V2P)
                    {
                        statusDataGridView.Rows[0].Cells[0].Value = currentTask;
                        statusDataGridView.Rows[0].Cells[1].Value = "Provisioning recovery VM(s) on secondary server";
                    }
                    else
                    {
                        statusDataGridView.Rows[0].Cells[0].Value = currentTask;
                        statusDataGridView.Rows[0].Cells[1].Value = "Uploading Configuration Files to CX";
                    }
                }
                firsttime = true;
                AnimateImage();
                ImageAnimator.UpdateFrames();
                try
                {
                    //if (File.Exists("C:\\windows\\temp\\InMage_Recovered_Vms.rollback"))
                    //{
                    //    File.Delete("C:\\windows\\temp\\InMage_Recovered_Vms.rollback");
                    //}
                    if (File.Exists("C:\\windows\\temp\\vCon_Error.log"))
                    {
                        File.Delete("C:\\windows\\temp\\vCon_Error.log");
                    }
                    if (File.Exists(WinTools.FxAgentPath() + "\\vContinuum\\logs\\vCon_Error.log"))
                    {
                        File.Delete(WinTools.FxAgentPath() + "\\vContinuum\\logs\\vCon_Error.log");
                    }
                }
                catch (Exception ex)
                {
                    Trace.WriteLine(DateTime.Now + "\t Failed to delete file form windows temp folder " + ex.Message);
                }
               
                backgroundWorker.RunWorkerAsync();
                this.ShowDialog();
               
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed in do work " + ex.Message);
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");

            }

        }

        private void cancelButton_Click(object sender, EventArgs e)
        {
            try
            {
                bool didIkillAnybody = false;
                //finding current process id...
                System.Diagnostics.Process CurrentPro = new System.Diagnostics.Process();

                CurrentPro = System.Diagnostics.Process.GetCurrentProcess();
                Process[] processes = Process.GetProcesses();

                foreach (Process p in processes)
                {
                    if (GetParentProcess(p.Id) == CurrentPro.Id)
                    {
                        
                        p.Kill();
                        didIkillAnybody = true;

                    }
                }



                if (didIkillAnybody)
                {
                    Trace.WriteLine(DateTime.Now + "\t\t Killed all running child processes...");
                    //CurrentPro.Kill();

                }
                else
                {

                    Trace.WriteLine(DateTime.Now + "\t\t No child processes to kill..");
                }
                this.Close();
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed to close the Wizard " + ex.Message);
                this.Close();
            }
        }
        /*
         * get parent process for a given PID
         */
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
        private void SetLeftTicker(string s)
        {
            try
            {
                if (_appName == AppName.Recover)
                {
                    if (uploadfiletocx == true)
                    {
                        datagridViewStatus.Rows[0].Cells[0].Value = completeTask;
                    }
                    else
                    {
                        datagridViewStatus.Rows[0].Cells[0].Value = failed;
                    }
                    if (calledByProtectButton == true && uploadfiletocx == false)
                    {
                        datagridViewStatus.Rows[0].Cells[column].Value = currentTask;
                        calledByProtectButton = false;
                        currentRow = 0;
                        return;
                    }
                    if (_masterHost.initializingRecoveryPlan == "Completed")
                    {
                        datagridViewStatus.Rows[1].Cells[0].Value = completeTask;
                    }
                    else if (_masterHost.initializingRecoveryPlan == "Failed")
                    {
                        datagridViewStatus.Rows[1].Cells[column].Value = failed;
                    }
                    else if (_masterHost.initializingRecoveryPlan == "Pending")
                    {
                        datagridViewStatus.Rows[1].Cells[column].Value = currentTask;

                    }
                    else if (_masterHost.initializingRecoveryPlan == "Notstarted" || _masterHost.initializingRecoveryPlan == null)
                    {
                        datagridViewStatus.Rows[1].Cells[column].Value = pending;
                    }
                    else if (_masterHost.initializingRecoveryPlan == "Warning")
                    {
                        datagridViewStatus.Rows[1].Cells[column].Value = warning;
                    }



                    if (_masterHost.downloadingConfigurationfiles == "Completed")
                    {

                        datagridViewStatus.Rows[2].Cells[0].Value = completeTask;
                    }
                    else if (_masterHost.downloadingConfigurationfiles == "Failed")
                    {
                        datagridViewStatus.Rows[2].Cells[column].Value = failed;
                    }
                    else if (_masterHost.downloadingConfigurationfiles == "Pending")
                    {

                        datagridViewStatus.Rows[2].Cells[column].Value = currentTask;
                    }
                    else if (_masterHost.downloadingConfigurationfiles == "Notstarted" || _masterHost.downloadingConfigurationfiles == null)
                    {
                        datagridViewStatus.Rows[2].Cells[column].Value = pending;
                    }
                    else if (_masterHost.downloadingConfigurationfiles == "Warning")
                    {
                        datagridViewStatus.Rows[2].Cells[column].Value = warning;
                    }

                    if (_masterHost.startingRecoveryForSelectedVms == "Completed")
                    {

                        datagridViewStatus.Rows[3].Cells[0].Value = completeTask;
                    }
                    else if (_masterHost.startingRecoveryForSelectedVms == "Failed")
                    {
                        datagridViewStatus.Rows[3].Cells[column].Value = failed; ;
                    }
                    else if (_masterHost.startingRecoveryForSelectedVms == "Pending")
                    {

                        datagridViewStatus.Rows[3].Cells[column].Value = currentTask;
                    }
                    else if (_masterHost.startingRecoveryForSelectedVms == "Notstarted" || _masterHost.startingRecoveryForSelectedVms == null)
                    {
                        datagridViewStatus.Rows[3].Cells[column].Value = pending;
                    }
                    else if (_masterHost.startingRecoveryForSelectedVms == "Warning")
                    {
                        datagridViewStatus.Rows[3].Cells[column].Value = warning;
                    }



                    if (_masterHost.powerOnVMs == "Completed")
                    {
                        datagridViewStatus.Rows[4].Cells[0].Value = completeTask;
                    }
                    else if (_masterHost.powerOnVMs == "Failed")
                    {
                        datagridViewStatus.Rows[4].Cells[0].Value = failed;
                    }
                    else if (_masterHost.powerOnVMs == "Pending")
                    {

                        datagridViewStatus.Rows[4].Cells[0].Value = currentTask;
                    }
                    else if (_masterHost.powerOnVMs == "Notstarted" || _masterHost.powerOnVMs == null)
                    {
                        datagridViewStatus.Rows[4].Cells[0].Value = pending;
                    }
                    else if (_masterHost.powerOnVMs == "Warning")
                    {
                        datagridViewStatus.Rows[4].Cells[column].Value = warning;
                    }

                    if (uploadfiletocx == true)
                    {
                        datagridViewStatus.Rows[0].Cells[column].Value = completeTask;
                    }
                    else
                    {
                        datagridViewStatus.Rows[0].Cells[column].Value = failed;


                    }
                    if (_masterHost.initializingRecoveryPlan == "Pending")
                    {
                        labelStatus.Visible = true;
                        buttonStatus.Text = "Close";
                        buttonStatus.Enabled = true;
                        currentRow = 1;
                        datagridViewStatus.Rows[1].Cells[column].Value = currentTask;
                    }
                    else if (_masterHost.downloadingConfigurationfiles == "Pending")
                    {
                        labelStatus.Visible = true;
                        buttonStatus.Text = "Close";
                        buttonStatus.Enabled = true;
                        currentRow = 2;
                        datagridViewStatus.Rows[2].Cells[column].Value = currentTask;
                    }
                    else if (_masterHost.startingRecoveryForSelectedVms == "Pending")
                    {
                        labelStatus.Visible = true;
                        buttonStatus.Text = "Close";
                        buttonStatus.Enabled = true;
                        currentRow = 3;
                        datagridViewStatus.Rows[3].Cells[column].Value = currentTask;
                    }
                    else if (_masterHost.powerOnVMs == "Pending")
                    {
                        labelStatus.Visible = true;
                        buttonStatus.Text = "Close";
                        buttonStatus.Enabled = true;
                        currentRow = 4;
                        datagridViewStatus.Rows[4].Cells[column].Value = currentTask;

                    }
                    else if (uploadfiletocx == true && _masterHost.initializingRecoveryPlan == null)
                    {
                        datagridViewStatus.Rows[1].Cells[column].Value = currentTask;
                        currentRow = 1;
                    }
                    else
                    {
                        currentRow = -1;
                    }

                }
                else if (_appName == AppName.Drdrill)
                {
                    if (createGuestOnTargetSucceed == true)
                    {
                        datagridViewStatus.Rows[0].Cells[column].Value = completeTask;
                    }
                    else
                    {
                        datagridViewStatus.Rows[0].Cells[column].Value = failed;
                    }
                    if (calledByProtectButton == true && createGuestOnTargetSucceed == false)
                    {
                        datagridViewStatus.Rows[0].Cells[column].Value = currentTask;
                        calledByProtectButton = false;
                        currentRow = 0;
                        return;
                    }


                    if (_masterHost.initializingDRDrillPlan == "Completed")
                    {
                        datagridViewStatus.Rows[1].Cells[column].Value = completeTask;
                    }
                    else if (_masterHost.initializingDRDrillPlan == "Pending")
                    {
                        datagridViewStatus.Rows[1].Cells[column].Value = currentTask;
                    }
                    else if (_masterHost.initializingDRDrillPlan == "Notstarted" || _masterHost.initializingDRDrillPlan == null)
                    {
                        datagridViewStatus.Rows[1].Cells[column].Value = pending;
                    }
                    else if (_masterHost.initializingDRDrillPlan == "Failed")
                    {
                        datagridViewStatus.Rows[1].Cells[column].Value = failed;
                    }
                    else if (_masterHost.initializingDRDrillPlan == "Warning")
                    {
                        datagridViewStatus.Rows[1].Cells[column].Value = warning;
                    }

                    if (_masterHost.downloadingConfigurationfiles == "Completed")
                    {
                        datagridViewStatus.Rows[2].Cells[column].Value = completeTask;
                    }
                    else if (_masterHost.downloadingConfigurationfiles == "Pending")
                    {
                        datagridViewStatus.Rows[2].Cells[column].Value = currentTask;
                    }
                    else if (_masterHost.downloadingConfigurationfiles == "Notstarted" || _masterHost.downloadingConfigurationfiles == null)
                    {
                        datagridViewStatus.Rows[2].Cells[column].Value = pending;
                    }
                    else if (_masterHost.downloadingConfigurationfiles == "Failed")
                    {
                        datagridViewStatus.Rows[2].Cells[column].Value = failed;
                    }
                    else if (_masterHost.downloadingConfigurationfiles == "Warning")
                    {
                        datagridViewStatus.Rows[2].Cells[column].Value = warning;
                    }

                    if (_masterHost.preparingMasterTargetDisks == "Completed")
                    {
                        datagridViewStatus.Rows[3].Cells[column].Value = completeTask;
                    }
                    else if (_masterHost.preparingMasterTargetDisks == "Pending")
                    {
                        datagridViewStatus.Rows[3].Cells[column].Value = currentTask;
                    }
                    else if (_masterHost.preparingMasterTargetDisks == "Notstarted" || _masterHost.preparingMasterTargetDisks == null)
                    {
                        datagridViewStatus.Rows[3].Cells[column].Value = pending;
                    }
                    else if (_masterHost.preparingMasterTargetDisks == "Failed")
                    {
                        datagridViewStatus.Rows[3].Cells[column].Value = failed;
                    }
                    else if (_masterHost.preparingMasterTargetDisks == "Warning")
                    {
                        datagridViewStatus.Rows[3].Cells[column].Value = warning;
                    }


                    if (_masterHost.os == OStype.Windows)
                    {
                        if (_masterHost.updatingDiskLayouts == "Completed")
                        {
                            datagridViewStatus.Rows[4].Cells[column].Value = completeTask;
                        }
                        else if (_masterHost.updatingDiskLayouts == "Pending")
                        {
                            datagridViewStatus.Rows[4].Cells[column].Value = currentTask;
                        }
                        else if (_masterHost.updatingDiskLayouts == "Notstarted" || _masterHost.updatingDiskLayouts == null)
                        {
                            datagridViewStatus.Rows[4].Cells[column].Value = pending;
                        }
                        else if (_masterHost.updatingDiskLayouts == "Failed")
                        {
                            datagridViewStatus.Rows[4].Cells[column].Value = failed;
                        }
                        else if (_masterHost.updatingDiskLayouts == "Warning")
                        {
                            datagridViewStatus.Rows[4].Cells[column].Value = warning;
                        }
                    }
                    if (_masterHost.os == OStype.Windows)
                    {
                        if (_masterHost.initializingPhysicalSnapShotOperation == "Completed")
                        {
                            datagridViewStatus.Rows[5].Cells[column].Value = completeTask;
                        }
                        else if (_masterHost.initializingPhysicalSnapShotOperation == "Pending")
                        {
                            datagridViewStatus.Rows[5].Cells[column].Value = currentTask;
                        }
                        else if (_masterHost.initializingPhysicalSnapShotOperation == "Notstarted" || _masterHost.initializingPhysicalSnapShotOperation == null)
                        {
                            datagridViewStatus.Rows[5].Cells[column].Value = pending;
                        }
                        else if (_masterHost.initializingPhysicalSnapShotOperation == "Failed")
                        {
                            datagridViewStatus.Rows[5].Cells[column].Value = failed;
                        }
                        else if (_masterHost.initializingPhysicalSnapShotOperation == "Warning")
                        {
                            datagridViewStatus.Rows[5].Cells[column].Value = warning;
                        }
                    }
                    else
                    {
                        if (_masterHost.initializingPhysicalSnapShotOperation == "Completed")
                        {
                            datagridViewStatus.Rows[4].Cells[column].Value = completeTask;
                        }
                        else if (_masterHost.initializingPhysicalSnapShotOperation == "Pending")
                        {
                            datagridViewStatus.Rows[4].Cells[column].Value = currentTask;
                        }
                        else if (_masterHost.initializingPhysicalSnapShotOperation == "Notstarted" || _masterHost.initializingPhysicalSnapShotOperation == null)
                        {
                            datagridViewStatus.Rows[4].Cells[column].Value = pending;
                        }
                        else if (_masterHost.initializingPhysicalSnapShotOperation == "Failed")
                        {
                            datagridViewStatus.Rows[4].Cells[column].Value = failed;
                        }
                        else if (_masterHost.initializingPhysicalSnapShotOperation == "Warning")
                        {
                            datagridViewStatus.Rows[4].Cells[column].Value = warning;
                        }
                    }
                    if (_masterHost.os == OStype.Windows)
                    {
                        if (_masterHost.startingPhysicalSnapShotforSelectedVMs == "Completed")
                        {
                            datagridViewStatus.Rows[6].Cells[column].Value = completeTask;
                        }
                        else if (_masterHost.startingPhysicalSnapShotforSelectedVMs == "Pending")
                        {
                            datagridViewStatus.Rows[6].Cells[column].Value = currentTask;
                        }
                        else if (_masterHost.startingPhysicalSnapShotforSelectedVMs == "Notstarted" || _masterHost.startingPhysicalSnapShotforSelectedVMs == null)
                        {
                            datagridViewStatus.Rows[6].Cells[column].Value = pending;
                        }
                        else if (_masterHost.startingPhysicalSnapShotforSelectedVMs == "Failed")
                        {
                            datagridViewStatus.Rows[6].Cells[column].Value = failed;
                        }
                        else if (_masterHost.startingPhysicalSnapShotforSelectedVMs == "Warning")
                        {
                            datagridViewStatus.Rows[6].Cells[column].Value = warning;
                        }
                    }
                    else
                    {
                        if (_masterHost.startingPhysicalSnapShotforSelectedVMs == "Completed")
                        {
                            datagridViewStatus.Rows[5].Cells[column].Value = completeTask;
                        }
                        else if (_masterHost.startingPhysicalSnapShotforSelectedVMs == "Pending")
                        {
                            datagridViewStatus.Rows[5].Cells[column].Value = currentTask;
                        }
                        else if (_masterHost.startingPhysicalSnapShotforSelectedVMs == "Notstarted" || _masterHost.startingPhysicalSnapShotforSelectedVMs == null)
                        {
                            datagridViewStatus.Rows[5].Cells[column].Value = pending;
                        }
                        else if (_masterHost.startingPhysicalSnapShotforSelectedVMs == "Failed")
                        {
                            datagridViewStatus.Rows[5].Cells[column].Value = failed;
                        }
                        else if (_masterHost.startingPhysicalSnapShotforSelectedVMs == "Warning")
                        {
                            datagridViewStatus.Rows[5].Cells[column].Value = warning;
                        }
                    }
                    if (_masterHost.os == OStype.Windows)
                    {
                        if (_masterHost.powerOnVMs == "Completed")
                        {
                            datagridViewStatus.Rows[7].Cells[column].Value = completeTask;
                        }
                        else if (_masterHost.powerOnVMs == "Pending")
                        {
                            datagridViewStatus.Rows[7].Cells[column].Value = currentTask;
                        }
                        else if (_masterHost.powerOnVMs == "Notstarted" || _masterHost.powerOnVMs == null)
                        {
                            datagridViewStatus.Rows[7].Cells[column].Value = pending;
                        }
                        else if (_masterHost.powerOnVMs == "Failed")
                        {
                            datagridViewStatus.Rows[7].Cells[column].Value = failed;
                        }
                        else if (_masterHost.powerOnVMs == "Warning")
                        {
                            datagridViewStatus.Rows[7].Cells[column].Value = warning;
                        }
                    }
                    else
                    {
                        if (_masterHost.powerOnVMs == "Completed")
                        {
                            datagridViewStatus.Rows[6].Cells[column].Value = completeTask;
                        }
                        else if (_masterHost.powerOnVMs == "Pending")
                        {
                            datagridViewStatus.Rows[6].Cells[column].Value = currentTask;
                        }
                        else if (_masterHost.powerOnVMs == "Notstarted" || _masterHost.powerOnVMs == null)
                        {
                            datagridViewStatus.Rows[6].Cells[column].Value = pending;
                        }
                        else if (_masterHost.powerOnVMs == "Failed")
                        {
                            datagridViewStatus.Rows[6].Cells[column].Value = failed;
                        }
                        else if (_masterHost.powerOnVMs == "Warning")
                        {
                            datagridViewStatus.Rows[6].Cells[column].Value = warning;
                        }
                    }

                    if (uploadfiletocx == true)
                    {
                        datagridViewStatus.Rows[0].Cells[column].Value = completeTask;
                    }
                    else
                    {
                        datagridViewStatus.Rows[0].Cells[column].Value = failed;


                    }
                    if (_masterHost.initializingDRDrillPlan == "Pending")
                    {
                        labelStatus.Visible = true;
                        buttonStatus.Text = "Close";
                        buttonStatus.Enabled = true;
                        currentRow = 1;
                        datagridViewStatus.Rows[1].Cells[column].Value = currentTask;
                    }
                    else if (_masterHost.downloadingConfigurationfiles == "Pending")
                    {
                        labelStatus.Visible = true;
                        buttonStatus.Text = "Close";
                        buttonStatus.Enabled = true;
                        currentRow = 2;
                        datagridViewStatus.Rows[2].Cells[column].Value = currentTask;
                    }
                    else if (_masterHost.preparingMasterTargetDisks == "Pending")
                    {
                        labelStatus.Visible = true;
                        buttonStatus.Text = "Close";
                        buttonStatus.Enabled = true;
                        currentRow = 3;
                        datagridViewStatus.Rows[3].Cells[column].Value = currentTask;
                    }
                    else if (_masterHost.updatingDiskLayouts == "Pending")
                    {
                        currentRow = 4;
                        datagridViewStatus.Rows[4].Cells[column].Value = currentTask;

                    }
                    else if (_masterHost.initializingPhysicalSnapShotOperation == "Pending")
                    {
                        labelStatus.Visible = true;
                        buttonStatus.Text = "Close";
                        buttonStatus.Enabled = true;
                        if (_masterHost.os == OStype.Windows)
                        {
                            currentRow = 5;
                            datagridViewStatus.Rows[5].Cells[column].Value = currentTask;
                        }
                        else
                        {
                            currentRow = 4;
                            datagridViewStatus.Rows[4].Cells[column].Value = currentTask;
                        }
                    }
                    else if (_masterHost.startingPhysicalSnapShotforSelectedVMs == "Pending")
                    {
                        labelStatus.Visible = true;
                        buttonStatus.Text = "Close";
                        buttonStatus.Enabled = true;
                        if (_masterHost.os == OStype.Windows)
                        {
                            currentRow = 6;
                            datagridViewStatus.Rows[6].Cells[column].Value = currentTask;
                        }
                        else
                        {
                            currentRow = 5;
                            datagridViewStatus.Rows[5].Cells[column].Value = currentTask;
                        }
                    }
                    else if (_masterHost.powerOnVMs == "Pending")
                    {
                        labelStatus.Visible = true;
                        buttonStatus.Text = "Close";
                        buttonStatus.Enabled = true;
                        if (_masterHost.os == OStype.Windows)
                        {
                            currentRow = 7;
                            datagridViewStatus.Rows[7].Cells[column].Value = currentTask;
                        }
                        else
                        {
                            currentRow = 6;
                            datagridViewStatus.Rows[6].Cells[column].Value = currentTask;
                        }

                    }
                    else if (createGuestOnTargetSucceed == true && uploadfiletocx == true && _masterHost.initializingDRDrillPlan == null)
                    {
                        datagridViewStatus.Rows[1].Cells[column].Value = currentTask;
                        currentRow = 1;
                    }
                    else
                    {
                        currentRow = -1;
                    }

                }
                else if (_appName == AppName.Resize)
                {
                    if (resizeSourceChecks == true  && resizeTargetExtend == true)
                    {
                        datagridViewStatus.Rows[0].Cells[column].Value = completeTask;
                    }
                    else
                    {
                        datagridViewStatus.Rows[0].Cells[column].Value = failed;
                    }

                    if (calledByProtectButton == true && resizeSourceChecks == false)
                    {
                        datagridViewStatus.Rows[0].Cells[column].Value = currentTask;
                        calledByProtectButton = false;
                        currentRow = 0;
                        return;
                    }


                    if (_masterHost.initializingResizePlan == "Completed")
                    {
                        datagridViewStatus.Rows[1].Cells[column].Value = completeTask;
                    }
                    else if (_masterHost.initializingResizePlan == "Pending")
                    {
                        datagridViewStatus.Rows[1].Cells[column].Value = currentTask;
                    }
                    else if (_masterHost.initializingResizePlan == "Notstarted" || _masterHost.initializingResizePlan == null)
                    {
                        datagridViewStatus.Rows[1].Cells[column].Value = pending;
                    }
                    else if (_masterHost.initializingResizePlan == "Failed")
                    {
                        datagridViewStatus.Rows[1].Cells[column].Value = failed;
                    }
                    else if (_masterHost.initializingResizePlan == "Warning")
                    {
                        datagridViewStatus.Rows[1].Cells[column].Value = warning;
                    }


                    if (_masterHost.resizedownloadfiles == "Completed")
                    {
                        datagridViewStatus.Rows[2].Cells[column].Value = completeTask;
                    }
                    else if (_masterHost.resizedownloadfiles == "Pending")
                    {
                        datagridViewStatus.Rows[2].Cells[column].Value = currentTask;
                    }
                    else if (_masterHost.resizedownloadfiles == "Notstarted" || _masterHost.resizedownloadfiles == null)
                    {
                        datagridViewStatus.Rows[2].Cells[column].Value = pending;
                    }
                    else if (_masterHost.resizedownloadfiles == "Failed")
                    {
                        datagridViewStatus.Rows[2].Cells[column].Value = failed;
                    }
                    else if (_masterHost.resizedownloadfiles == "Warning")
                    {
                        datagridViewStatus.Rows[2].Cells[column].Value = warning;
                    }

                    if (_masterHost.pauseReplicationPairs == "Completed")
                    {
                        datagridViewStatus.Rows[3].Cells[column].Value = completeTask;
                    }
                    else if (_masterHost.pauseReplicationPairs == "Pending")
                    {
                        datagridViewStatus.Rows[3].Cells[column].Value = currentTask;
                    }
                    else if (_masterHost.pauseReplicationPairs == "Notstarted" || _masterHost.pauseReplicationPairs == null)
                    {
                        datagridViewStatus.Rows[3].Cells[column].Value = pending;
                    }
                    else if (_masterHost.pauseReplicationPairs == "Failed")
                    {
                        datagridViewStatus.Rows[3].Cells[column].Value = failed;
                    }
                    else if (_masterHost.pauseReplicationPairs == "Warning")
                    {
                        datagridViewStatus.Rows[3].Cells[column].Value = warning;
                    }


                    if (_masterHost.resizePreParingMasterTargetDisks == "Completed")
                    {
                        datagridViewStatus.Rows[4].Cells[column].Value = completeTask;
                    }
                    else if (_masterHost.resizePreParingMasterTargetDisks == "Pending")
                    {
                        datagridViewStatus.Rows[4].Cells[column].Value = currentTask;
                    }
                    else if (_masterHost.resizePreParingMasterTargetDisks == "Notstarted" || _masterHost.resizePreParingMasterTargetDisks == null)
                    {
                        datagridViewStatus.Rows[4].Cells[column].Value = pending;
                    }
                    else if (_masterHost.resizePreParingMasterTargetDisks == "Failed")
                    {
                        datagridViewStatus.Rows[4].Cells[column].Value = failed;
                    }
                    else if (_masterHost.resizePreParingMasterTargetDisks == "Warning")
                    {
                        datagridViewStatus.Rows[4].Cells[column].Value = warning;
                    }


                    if (_masterHost.os == OStype.Windows)
                    {
                        if (_masterHost.resizeUpdateDiskLayouts == "Completed")
                        {
                            datagridViewStatus.Rows[5].Cells[column].Value = completeTask;
                        }
                        else if (_masterHost.resizeUpdateDiskLayouts == "Pending")
                        {
                            datagridViewStatus.Rows[5].Cells[column].Value = currentTask;
                        }
                        else if (_masterHost.resizeUpdateDiskLayouts == "Notstarted" || _masterHost.resizeUpdateDiskLayouts == null)
                        {
                            datagridViewStatus.Rows[5].Cells[column].Value = pending;
                        }
                        else if (_masterHost.resizeUpdateDiskLayouts == "Failed")
                        {
                            datagridViewStatus.Rows[5].Cells[column].Value = failed;
                        }
                        else if (_masterHost.resizeUpdateDiskLayouts == "Warning")
                        {
                            datagridViewStatus.Rows[5].Cells[column].Value = warning;
                        }


                        if (_masterHost.resumeReplicationPairs == "Completed")
                        {
                            datagridViewStatus.Rows[6].Cells[column].Value = completeTask;
                        }
                        else if (_masterHost.resumeReplicationPairs == "Pending")
                        {
                            datagridViewStatus.Rows[6].Cells[column].Value = currentTask;
                        }
                        else if (_masterHost.resumeReplicationPairs == "Notstarted" || _masterHost.resumeReplicationPairs == null)
                        {
                            datagridViewStatus.Rows[6].Cells[column].Value = pending;
                        }
                        else if (_masterHost.resumeReplicationPairs == "Failed")
                        {
                            datagridViewStatus.Rows[6].Cells[column].Value = failed;
                        }
                        else if (_masterHost.resumeReplicationPairs == "Warning")
                        {
                            datagridViewStatus.Rows[6].Cells[column].Value = warning;
                        }

                    }
                    else
                    {
                        if (_masterHost.resumeReplicationPairs == "Completed")
                        {
                            datagridViewStatus.Rows[5].Cells[column].Value = completeTask;
                        }
                        else if (_masterHost.resumeReplicationPairs == "Pending")
                        {
                            datagridViewStatus.Rows[5].Cells[column].Value = currentTask;
                        }
                        else if (_masterHost.resumeReplicationPairs == "Notstarted" || _masterHost.resumeReplicationPairs == null)
                        {
                            datagridViewStatus.Rows[5].Cells[column].Value = pending;
                        }
                        else if (_masterHost.resumeReplicationPairs == "Failed")
                        {
                            datagridViewStatus.Rows[5].Cells[column].Value = failed;
                        }
                        else if (_masterHost.resumeReplicationPairs == "Warning")
                        {
                            datagridViewStatus.Rows[5].Cells[column].Value = warning;
                        }
                    }

                    if (_masterHost.initializingResizePlan == "Pending")
                    {
                       // _label.Visible = true;
                        //_button.Text = "Close";
                        //_button.Enabled = true;
                        currentRow = 1;
                        datagridViewStatus.Rows[1].Cells[column].Value = currentTask;
                    }
                    else if (_masterHost.resizedownloadfiles == "Pending")
                    {
                       // _label.Visible = true;
                       // _button.Text = "Close";
                       // _button.Enabled = true;
                        currentRow = 2;
                        datagridViewStatus.Rows[2].Cells[column].Value = currentTask;
                    }
                    else if (_masterHost.pauseReplicationPairs == "Pending")
                    {
                        //_label.Visible = true;
                        //_button.Text = "Close";
                       // _button.Enabled = true;
                        currentRow = 3;
                        datagridViewStatus.Rows[3].Cells[column].Value = currentTask;
                    }
                    else if (_masterHost.resizePreParingMasterTargetDisks == "Pending")
                    {
                        //_label.Visible = true;
                       // _button.Text = "Close";
                       // _button.Enabled = true;
                        currentRow = 4;
                        datagridViewStatus.Rows[4].Cells[column].Value = currentTask;
                    }
                    else if (_masterHost.resizeUpdateDiskLayouts == "Pending")
                    {
                        currentRow = 5;
                        datagridViewStatus.Rows[5].Cells[column].Value = currentTask;

                    }
                    else if (_masterHost.resumeReplicationPairs == "Pending")
                    {
                       // _label.Visible = true;
                       // _button.Text = "Close";
                       // _button.Enabled = true;
                        if (_masterHost.os == OStype.Windows)
                        {
                            currentRow = 6;
                            datagridViewStatus.Rows[6].Cells[column].Value = currentTask;
                        }
                        else
                        {
                            currentRow = 5;
                            datagridViewStatus.Rows[5].Cells[column].Value = currentTask;
                        }
                    }
                    else if (resizeSourceChecks == true && resizeTargetExtend == true && _masterHost.initializingResizePlan == null)
                    {
                        datagridViewStatus.Rows[1].Cells[column].Value = currentTask;
                        currentRow = 1;
                    }
                    else
                    {
                        currentRow = -1;
                    }


                }
                else if (_appName == AppName.Removedisk)
                {
                    if (creatingScsiUnitsFile == true)
                    {
                        datagridViewStatus.Rows[0].Cells[column].Value = completeTask;
                    }
                    else
                    {
                        datagridViewStatus.Rows[0].Cells[column].Value = failed;
                    }

                    if (_masterHost.initializingRemoveDiskPlan == "Completed")
                    {
                        datagridViewStatus.Rows[1].Cells[column].Value = completeTask;
                    }
                    else if (_masterHost.initializingRemoveDiskPlan == "Pending")
                    {
                        datagridViewStatus.Rows[1].Cells[column].Value = currentTask;
                    }
                    else if (_masterHost.initializingRemoveDiskPlan == "Notstarted" || _masterHost.initializingProtectionPlan == null)
                    {
                        datagridViewStatus.Rows[1].Cells[column].Value = pending;
                    }
                    else if (_masterHost.initializingRemoveDiskPlan == "Failed")
                    {
                        datagridViewStatus.Rows[1].Cells[column].Value = failed;
                    }
                    else if (_masterHost.initializingRemoveDiskPlan == "Warning")
                    {
                        datagridViewStatus.Rows[1].Cells[column].Value = warning;
                    }

                    if (_masterHost.downloadRemoveDiskFiles == "Completed")
                    {
                        datagridViewStatus.Rows[2].Cells[column].Value = completeTask;
                    }
                    else if (_masterHost.downloadRemoveDiskFiles == "Pending")
                    {
                        datagridViewStatus.Rows[2].Cells[column].Value = currentTask;
                    }
                    else if (_masterHost.downloadRemoveDiskFiles == "Notstarted" || _masterHost.downloadingConfigurationfiles == null)
                    {
                        datagridViewStatus.Rows[2].Cells[column].Value = pending;
                    }
                    else if (_masterHost.downloadRemoveDiskFiles == "Failed")
                    {
                        datagridViewStatus.Rows[2].Cells[column].Value = failed;
                    }
                    else if (_masterHost.downloadRemoveDiskFiles == "Warning")
                    {
                        datagridViewStatus.Rows[2].Cells[column].Value = warning;
                    }


                    if (_masterHost.deleteReplicationPairs == "Completed")
                    {
                        datagridViewStatus.Rows[3].Cells[column].Value = completeTask;
                    }
                    else if (_masterHost.deleteReplicationPairs == "Pending")
                    {
                        datagridViewStatus.Rows[3].Cells[column].Value = currentTask;
                    }
                    else if (_masterHost.deleteReplicationPairs == "Notstarted" || _masterHost.preparingMasterTargetDisks == null)
                    {
                        datagridViewStatus.Rows[3].Cells[column].Value = pending;
                    }
                    else if (_masterHost.deleteReplicationPairs == "Failed")
                    {
                        datagridViewStatus.Rows[3].Cells[column].Value = failed;
                    }
                    else if (_masterHost.deleteReplicationPairs == "Warning")
                    {
                        datagridViewStatus.Rows[3].Cells[column].Value = warning;
                    }

                    if (_masterHost.removeDisksFromMt == "Completed")
                    {
                        datagridViewStatus.Rows[4].Cells[column].Value = completeTask;
                    }
                    else if (_masterHost.removeDisksFromMt == "Pending")
                    {
                        datagridViewStatus.Rows[4].Cells[column].Value = currentTask;
                    }
                    else if (_masterHost.removeDisksFromMt == "Notstarted" || _masterHost.updatingDiskLayouts == null)
                    {
                        datagridViewStatus.Rows[4].Cells[column].Value = pending;
                    }
                    else if (_masterHost.removeDisksFromMt == "Failed")
                    {
                        datagridViewStatus.Rows[4].Cells[column].Value = failed;
                    }
                    else if (_masterHost.removeDisksFromMt == "Warning")
                    {
                        datagridViewStatus.Rows[4].Cells[column].Value = warning;
                    }


                    if (_masterHost.initializingRemoveDiskPlan == "Pending")
                    {
                        //_label.Visible = true;
                       // _button.Text = "Close";
                      //  _button.Enabled = true;
                        datagridViewStatus.Rows[1].Cells[column].Value = currentTask;
                        currentRow = 1;
                    }
                    else if (_masterHost.downloadRemoveDiskFiles == "Pending")
                    {
                       // _label.Visible = true;
                       // _button.Text = "Close";
                       // _button.Enabled = true;
                        datagridViewStatus.Rows[2].Cells[column].Value = currentTask;
                        currentRow = 2;
                    }
                    else if (_masterHost.deleteReplicationPairs == "Pending")
                    {
                       // _label.Visible = true;
                       // _button.Text = "Close";
                       // _button.Enabled = true;
                        datagridViewStatus.Rows[3].Cells[column].Value = currentTask;
                        currentRow = 3;
                    }
                    else if (_masterHost.removeDisksFromMt == "Pending")
                    {
                       // _label.Visible = true;
                       // _button.Text = "Close";
                      //  _button.Enabled = true;

                        datagridViewStatus.Rows[4].Cells[column].Value = currentTask;
                        currentRow = 4;

                    }
                    else if (creatingScsiUnitsFile == true && _masterHost.initializingRemoveDiskPlan == null)
                    {
                        datagridViewStatus.Rows[1].Cells[column].Value = currentTask;
                        currentRow = 1;
                    }
                    else
                    {
                        currentRow = -1;
                    }
                    AnimateImage();
                    ImageAnimator.UpdateFrames();



                }
                else if (_appName == AppName.Removevolume)
                {
                    if (uploadfiletocx == true)
                    {
                        datagridViewStatus.Rows[0].Cells[column].Value = completeTask;
                    }
                    else
                    {
                        datagridViewStatus.Rows[0].Cells[column].Value = failed;
                    }

                    if (_masterHost.initializingRemoveDiskPlan == "Completed")
                    {
                        datagridViewStatus.Rows[1].Cells[column].Value = completeTask;
                    }
                    else if (_masterHost.initializingRemoveDiskPlan == "Pending")
                    {
                        datagridViewStatus.Rows[1].Cells[column].Value = currentTask;
                    }
                    else if (_masterHost.initializingRemoveDiskPlan == "Notstarted" || _masterHost.initializingProtectionPlan == null)
                    {
                        datagridViewStatus.Rows[1].Cells[column].Value = pending;
                    }
                    else if (_masterHost.initializingRemoveDiskPlan == "Failed")
                    {
                        datagridViewStatus.Rows[1].Cells[column].Value = failed;
                    }
                    else if (_masterHost.initializingRemoveDiskPlan == "Warning")
                    {
                        datagridViewStatus.Rows[1].Cells[column].Value = warning;
                    }

                    if (_masterHost.downloadRemoveDiskFiles == "Completed")
                    {
                        datagridViewStatus.Rows[2].Cells[column].Value = completeTask;
                    }
                    else if (_masterHost.downloadRemoveDiskFiles == "Pending")
                    {
                        datagridViewStatus.Rows[2].Cells[column].Value = currentTask;
                    }
                    else if (_masterHost.downloadRemoveDiskFiles == "Notstarted" || _masterHost.downloadingConfigurationfiles == null)
                    {
                        datagridViewStatus.Rows[2].Cells[column].Value = pending;
                    }
                    else if (_masterHost.downloadRemoveDiskFiles == "Failed")
                    {
                        datagridViewStatus.Rows[2].Cells[column].Value = failed;
                    }
                    else if (_masterHost.downloadRemoveDiskFiles == "Warning")
                    {
                        datagridViewStatus.Rows[2].Cells[column].Value = warning;
                    }


                    if (_masterHost.deleteReplicationPairs == "Completed")
                    {
                        datagridViewStatus.Rows[3].Cells[column].Value = completeTask;
                    }
                    else if (_masterHost.deleteReplicationPairs == "Pending")
                    {
                        datagridViewStatus.Rows[3].Cells[column].Value = currentTask;
                    }
                    else if (_masterHost.deleteReplicationPairs == "Notstarted" || _masterHost.preparingMasterTargetDisks == null)
                    {
                        datagridViewStatus.Rows[3].Cells[column].Value = pending;
                    }
                    else if (_masterHost.deleteReplicationPairs == "Failed")
                    {
                        datagridViewStatus.Rows[3].Cells[column].Value = failed;
                    }
                    else if (_masterHost.deleteReplicationPairs == "Warning")
                    {
                        datagridViewStatus.Rows[3].Cells[column].Value = warning;
                    }

                   


                    if (_masterHost.initializingRemoveDiskPlan == "Pending")
                    {
                       // _label.Visible = true;
                       // _button.Text = "Close";
                      //  _button.Enabled = true;
                        datagridViewStatus.Rows[1].Cells[column].Value = currentTask;
                        currentRow = 1;
                    }
                    else if (_masterHost.downloadRemoveDiskFiles == "Pending")
                    {
                       // _label.Visible = true;
                      //  _button.Text = "Close";
                      //  _button.Enabled = true;
                        datagridViewStatus.Rows[2].Cells[column].Value = currentTask;
                        currentRow = 2;
                    }
                    else if (_masterHost.deleteReplicationPairs == "Pending")
                    {
                       // _label.Visible = true;
                      //  _button.Text = "Close";
                       // _button.Enabled = true;
                        datagridViewStatus.Rows[3].Cells[column].Value = currentTask;
                        currentRow = 3;
                    }
                    
                    else if (uploadfiletocx == true && _masterHost.initializingRemoveDiskPlan == null)
                    {
                        datagridViewStatus.Rows[1].Cells[column].Value = currentTask;
                        currentRow = 1;
                    }
                    else
                    {
                        currentRow = -1;
                    }
                    AnimateImage();
                    ImageAnimator.UpdateFrames();
                }
                else
                {
                    if (createGuestOnTargetSucceed == true && jobAutomationSucceed == true && uploadfiletocx == true)
                    {
                        datagridViewStatus.Rows[0].Cells[column].Value = completeTask;
                    }
                    else
                    {
                        datagridViewStatus.Rows[0].Cells[column].Value = failed;
                    }
                    if (_appName != AppName.V2P)
                    {
                        if (calledByProtectButton == true && createGuestOnTargetSucceed == false)
                        {
                            datagridViewStatus.Rows[0].Cells[column].Value = currentTask;
                            calledByProtectButton = false;
                            currentRow = 0;
                            return;
                        }
                        else if (createGuestOnTargetSucceed == true && jobAutomationSucceed == false)
                        {
                            datagridViewStatus.Rows[0].Cells[column].Value = currentTask;
                            datagridViewStatus.Rows[0].Cells[column].Value = currentTask;
                            calledByProtectButton = false;
                            currentRow = 0;
                            return;
                        }
                    }
                    else
                    {
                        createGuestOnTargetSucceed = true;
                        uploadfiletocx = true;
                        jobAutomationSucceed = true;
                        datagridViewStatus.Rows[0].Cells[column].Value = completeTask;
                    }
                    if (_masterHost.initializingProtectionPlan == "Completed")
                    {
                        datagridViewStatus.Rows[1].Cells[column].Value = completeTask;
                    }
                    else if (_masterHost.initializingProtectionPlan == "Pending")
                    {
                        datagridViewStatus.Rows[1].Cells[column].Value = currentTask;
                    }
                    else if (_masterHost.initializingProtectionPlan == "Notstarted" || _masterHost.initializingProtectionPlan == null)
                    {
                        datagridViewStatus.Rows[1].Cells[column].Value = pending;
                    }
                    else if (_masterHost.initializingProtectionPlan == "Failed")
                    {
                        datagridViewStatus.Rows[1].Cells[column].Value = failed;
                    }
                    else if (_masterHost.initializingProtectionPlan == "Warning")
                    {
                        datagridViewStatus.Rows[1].Cells[column].Value = warning;
                    }



                    if (_masterHost.downloadingConfigurationfiles == "Completed")
                    {
                        datagridViewStatus.Rows[2].Cells[column].Value = completeTask;
                    }
                    else if (_masterHost.downloadingConfigurationfiles == "Pending")
                    {
                        datagridViewStatus.Rows[2].Cells[column].Value = currentTask;
                    }
                    else if (_masterHost.downloadingConfigurationfiles == "Notstarted" || _masterHost.downloadingConfigurationfiles == null)
                    {
                        datagridViewStatus.Rows[2].Cells[column].Value = pending;
                    }
                    else if (_masterHost.downloadingConfigurationfiles == "Failed")
                    {
                        datagridViewStatus.Rows[2].Cells[column].Value = failed;
                    }
                    else if (_masterHost.downloadingConfigurationfiles == "Warning")
                    {
                        datagridViewStatus.Rows[2].Cells[column].Value = warning;
                    }


                    if (_masterHost.preparingMasterTargetDisks == "Completed")
                    {
                        datagridViewStatus.Rows[3].Cells[column].Value = completeTask;
                    }
                    else if (_masterHost.preparingMasterTargetDisks == "Pending")
                    {
                        datagridViewStatus.Rows[3].Cells[column].Value = currentTask;
                    }
                    else if (_masterHost.preparingMasterTargetDisks == "Notstarted" || _masterHost.preparingMasterTargetDisks == null)
                    {
                        datagridViewStatus.Rows[3].Cells[column].Value = pending;
                    }
                    else if (_masterHost.preparingMasterTargetDisks == "Failed")
                    {
                        datagridViewStatus.Rows[3].Cells[column].Value = failed;
                    }
                    else if (_masterHost.preparingMasterTargetDisks == "Warning")
                    {
                        datagridViewStatus.Rows[3].Cells[column].Value = warning;
                    }


                    if (_masterHost.os == OStype.Windows)
                    {
                        if (_masterHost.updatingDiskLayouts == "Completed")
                        {
                            datagridViewStatus.Rows[4].Cells[column].Value = completeTask;
                        }
                        else if (_masterHost.updatingDiskLayouts == "Pending")
                        {
                            datagridViewStatus.Rows[4].Cells[column].Value = currentTask;
                        }
                        else if (_masterHost.updatingDiskLayouts == "Notstarted" || _masterHost.updatingDiskLayouts == null)
                        {
                            datagridViewStatus.Rows[4].Cells[column].Value = pending;
                        }
                        else if (_masterHost.updatingDiskLayouts == "Failed")
                        {
                            datagridViewStatus.Rows[4].Cells[column].Value = failed;
                        }
                        else if (_masterHost.updatingDiskLayouts == "Warning")
                        {
                            datagridViewStatus.Rows[4].Cells[column].Value = warning;
                        }
                    }
                    else
                    {
                        if (_masterHost.activateProtectionPlan == "Completed")
                        {
                            datagridViewStatus.Rows[4].Cells[column].Value = completeTask;
                        }
                        else if (_masterHost.activateProtectionPlan == "Pending")
                        {
                            datagridViewStatus.Rows[4].Cells[column].Value = currentTask;
                        }
                        else if (_masterHost.activateProtectionPlan == "Notstarted" || _masterHost.activateProtectionPlan == null)
                        {
                            datagridViewStatus.Rows[4].Cells[column].Value = pending;
                        }
                        else if (_masterHost.activateProtectionPlan == "Failed")
                        {
                            datagridViewStatus.Rows[4].Cells[column].Value = failed;
                        }
                        else if (_masterHost.activateProtectionPlan == "Warning")
                        {
                            datagridViewStatus.Rows[4].Cells[column].Value = warning;
                        }


                    }

                    if (_masterHost.os == OStype.Windows)
                    {
                        if (_masterHost.activateProtectionPlan == "Completed")
                        {
                            datagridViewStatus.Rows[5].Cells[column].Value = completeTask;
                        }
                        else if (_masterHost.activateProtectionPlan == "Pending")
                        {
                            datagridViewStatus.Rows[5].Cells[column].Value = currentTask;
                        }
                        else if (_masterHost.activateProtectionPlan == "Notstarted" || _masterHost.activateProtectionPlan == null)
                        {
                            datagridViewStatus.Rows[5].Cells[column].Value = pending;
                        }
                        else if (_masterHost.activateProtectionPlan == "Failed")
                        {
                            datagridViewStatus.Rows[5].Cells[column].Value = failed;
                        }
                        else if (_masterHost.activateProtectionPlan == "Warning")
                        {
                            datagridViewStatus.Rows[5].Cells[column].Value = warning;
                        }
                    }


                    if (_masterHost.initializingProtectionPlan == "Pending")
                    {
                        labelStatus.Visible = true;
                        buttonStatus.Text = "Close";
                        buttonStatus.Enabled = true;
                        datagridViewStatus.Rows[1].Cells[column].Value = currentTask;
                        currentRow = 1;
                    }
                    else if (_masterHost.downloadingConfigurationfiles == "Pending")
                    {
                        labelStatus.Visible = true;
                        buttonStatus.Text = "Close";
                        buttonStatus.Enabled = true;
                        datagridViewStatus.Rows[2].Cells[column].Value = currentTask;
                        currentRow = 2;
                    }
                    else if (_masterHost.preparingMasterTargetDisks == "Pending")
                    {
                        labelStatus.Visible = true;
                        buttonStatus.Text = "Close";
                        buttonStatus.Enabled = true;
                        datagridViewStatus.Rows[3].Cells[column].Value = currentTask;
                        currentRow = 3;
                    }
                    else if (_masterHost.updatingDiskLayouts == "Pending")
                    {
                        labelStatus.Visible = true;
                        buttonStatus.Text = "Close";
                        buttonStatus.Enabled = true;
                        if (_masterHost.os == OStype.Windows)
                        {
                            datagridViewStatus.Rows[4].Cells[column].Value = currentTask;
                            currentRow = 4;
                        }
                    }
                    else if (_masterHost.activateProtectionPlan == "Pending")
                    {
                        labelStatus.Visible = true;
                        buttonStatus.Text = "Close";
                        buttonStatus.Enabled = true;
                        if (_masterHost.os == OStype.Windows)
                        {
                            datagridViewStatus.Rows[5].Cells[column].Value = currentTask;
                            currentRow = 5;
                        }
                        else
                        {
                            if (_masterHost.activateProtectionPlan == "Pending")
                            {
                                datagridViewStatus.Rows[4].Cells[column].Value = currentTask;
                                currentRow = 4;
                            }
                        }
                    }
                    else if (createGuestOnTargetSucceed == true && uploadfiletocx == true && jobAutomationSucceed == true && _masterHost.initializingProtectionPlan == null)
                    {
                        datagridViewStatus.Rows[1].Cells[column].Value = currentTask;
                        currentRow = 1;
                    }
                    else
                    {
                        currentRow = -1;
                    }

                }
                AnimateImage();
                ImageAnimator.UpdateFrames();
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

        public bool PreScriptRun(Host masterHost)
        {
            Trace.WriteLine(DateTime.Now + " \t Entered: PreScriptRun: of StatusForm.cs");
            try
            {
                tickerDelegate = new TickerDelegate(SetLeftTicker);



                if (_appName != AppName.V2P)
                {
                    if (dummyDiskCreation == false)
                    {
                        // This metod will create dummy disks add that will attach disk to the mt.
                        ArrayList diskList = new ArrayList();
                        if (masterHost.os == OStype.Linux)
                        {
                            masterHost.requestId = null;
                            RunningScriptInLinuxmt(masterHost, ref diskList);
                        }
                        if (CreateDummydisk(masterHost) == false)
                        {
                            Trace.WriteLine(DateTime.Now + "\t Failed to create dummy disks");
                            statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                            return false;
                        }
                        else
                        {
                            Trace.WriteLine(DateTime.Now + "\t Successfully completed dummy disks creation");
                        }
                    }


                    // This will be called to create the vms on the target side.....
                    if (createGuestOnTargetSucceed == false)
                    {

                        if (CreateVMs(masterHost) == false)
                        {
                            createGuestOnTargetSucceed = false;
                            statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                            return false;
                        }
                        else
                        {
                            Trace.WriteLine(DateTime.Now + "\t Successfully completed creating vms on secondary server");
                            //statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                            // _message = " Creating VM(s) on target server completed successfully. \t Completed:" + DateTime.Now + Environment.NewLine;
                            //allServersForm.protectionText.Invoke(tickerDelegate, new object[] { _message });
                        }

                    }
                }
                else
                {
                    if (selectedSourceList._hostList.Count != 0)
                    {
                        Host sourceHost = (Host)selectedSourceList._hostList[0];
                        if (sourceHost.os == OStype.Windows)
                        {
                            RefreshHosts(selectedSourceList);
                        }
                    }
                    dummyDiskCreation = true;
                    createGuestOnTargetSucceed = true;
                }

                if (createGuestOnTargetSucceed == true && jobAutomationSucceed == false)
                {
                    if (SetFxJobs(masterHost, "yes") == false)
                    {
                        jobAutomationSucceed = false;
                        statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                        return false;
                    }
                    else
                    {
                        Trace.WriteLine(DateTime.Now + "\t Successfully completed setting consistency jobs");
                    }

                }
                
                

                if (jobAutomationSucceed == true)
                {
                    CopyFiletoDirectory();

                    Trace.WriteLine(DateTime.Now + "\t Successfully completed copying files to Planname folder");
                    MasterConfigFile masterCOnfig = new MasterConfigFile();
                    if (masterCOnfig.UpdateMasterConfigFile(latestFilePath + "\\ESX.xml"))
                    {
                        Trace.WriteLine(DateTime.Now + "\t Successfully updated the MasterConfigfile");
                        postJobAutomationResult = true;
                    }
                    else
                    {
                        Trace.WriteLine(DateTime.Now + "\t Failed to update MasterConfigfile.xml");
                        postJobAutomationResult = false;

                    }
                }
               
                if (jobAutomationSucceed == true && esxutilwinExecution == false)
                {
                    if (uploadfiletocx == false)
                    {
                        if (UploadFileAndPrePareInputText(masterHost.os) == true)
                        {
                            uploadfiletocx = true;
                            statusDataGridView.Invoke(tickerDelegate, new object[] { "" });  
                            Trace.WriteLine(DateTime.Now + "\t Successfully made input.txt file and uploaded all required file to cxps");

                        }
                        else
                        {
                            uploadfiletocx = false;
                            statusDataGridView.Invoke(tickerDelegate, new object[] { "" });  
                            Trace.WriteLine(DateTime.Now + "\t Failed to made input.txt and failed to upload required files to cxps");
                            return false;
                        }
                    }
                    try
                    {
                        if (File.Exists(WinTools.FxAgentPath() + "\\Failover\\Data\\" + directoryNameinXmlFile + "\\protection.log"))
                        {
                            File.Delete(WinTools.FxAgentPath() + "\\Failover\\Data\\" + directoryNameinXmlFile + "\\protection.log");
                        }
                    }
                    catch (Exception ex)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Failed to delete the protection.log " + ex.Message);
                    }
                    EsxUtilWinExecute(ref masterHost);
                    //CopyLogs(masterHost);
                    
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


        public bool CopyLogs(Host masterHost)
        {
            try
            {
                if (scriptid != null)
                {
                    Cxapicalls cxApi = new Cxapicalls();
                    if (cxApi.PostRequestStatusForDummyDisk( masterHost, true) == false)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Failed to get script out ");
                    }
                    else
                    {
                        masterHost = Cxapicalls.GetHost;
                        if (File.Exists(WinTools.FxAgentPath() + "\\vContinuum\\logs\\ESXUtilorWin.log"))
                        {
                            if (fxFailOverDataPath != null && directoryNameinXmlFile != null)
                            {
                                File.Copy(WinTools.FxAgentPath() + "\\vContinuum\\logs\\ESXUtilorWin.log", fxFailOverDataPath + "\\" + directoryNameinXmlFile + "\\ESXUtilorWin.log", true);
                            }
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed in do work " + ex.Message);
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");

            }
            return true;
        }

        private bool VerifyCredentials(Host masterHost, AllServersForm allServersForm)
        {
            if(allServersForm.VerfiyEsxCreds(masterHost.esxIp, "target") == true)
            {
                Esx esx = new Esx();
                int returnValue = esx.ValidateEsxCredentails(masterHost.esxIp);
                if(returnValue == 3)
                {
                    SourceEsxDetailsPopUpForm sourceEsx = new SourceEsxDetailsPopUpForm();
                    sourceEsx.ipLabel.Text = "Target vCenter/vSphere IP/Name *";
                    sourceEsx.sourceEsxIpText.ReadOnly = false;
                    sourceEsx.sourceEsxIpText.Text = masterHost.esxIp;
                    sourceEsx.Focus();
                    sourceEsx.ShowDialog();
                    if (sourceEsx.canceled == false)
                    {

                        masterHost.esxIp = sourceEsx.sourceEsxIpText.Text;
                        masterHost.esxUserName = sourceEsx.userNameText.Text;
                        masterHost.esxPassword = sourceEsx.passWordText.Text;
                        Cxapicalls cxAPi = new Cxapicalls();
                        cxAPi.UpdaterCredentialsToCX(masterHost.esxIp, masterHost.esxUserName, masterHost.esxPassword);
                        returnValue = esx.ValidateEsxCredentails(masterHost.esxIp);

                        if (returnValue != 0)
                        {
                            MessageBox.Show("Failed to get the information form target vSphere client", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            return false;
                        }
                        
                    }
                }
                else if (returnValue != 0)
                {
                    MessageBox.Show("Failed to get the information form target vSphere client", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return false;
                }
            }
           
            return true;
        }

        private bool RemoveDiskFromProtection(HostList sourceList, Host masterHost)
        {
            try
            {

                AllServersForm allServersFrom = new AllServersForm("Removedisk", "removedisk");

                if (VerifyCredentials(masterHost, allServersFrom) == false)
                {
                    Trace.WriteLine(DateTime.Now + "\t Failed to validate credentials");
                    return false;
                }
                if (creatingScsiUnitsFile == false)
                {
                    Esx esx = new Esx();
                    int returnCodeOfJobAutomation = 0;
                    returnCodeOfJobAutomation = esx.ExecuteJobAutomationScript(OperationType.Removedisk, "no");
                    if (returnCodeOfJobAutomation == 0)
                    {
                        creatingScsiUnitsFile = true;
                    }
                    else
                    {
                        creatingScsiUnitsFile = false;
                        return false;
                    }

                    UploadFileAndPrePareInputText(OStype.Windows);
                }

                if (esxutilwinExecution == false)
                {
                    EsxUtilWinExecute(ref masterHost);
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

        private bool RemoveVolumeFromProtection(HostList sourceList, Host masterHost)
        {
            if (uploadfiletocx == false)
            {
                UploadFileAndPrePareInputText(OStype.Windows);
                uploadfiletocx = true;
            }
            if (esxutilwinExecution == false)
            {
                EsxUtilWinExecute(ref masterHost);
            }
            return true;
        }

        private bool ResizeOFDiskOrVolume(HostList sourceHostList, Host masterHost)
        {

            Host sourceHost = (Host)sourceHostList._hostList[0];
            Esx esx = new Esx();
            if (resizeSourceChecks == false)
            {
                RefreshHosts(sourceHostList);
                HostList tempList = new HostList();
                Host tempHost1 = new Host();
                foreach (Host h in sourceHostList._hostList)
                {

                    tempHost1 = new Host();
                    tempHost1.inmage_hostid = h.inmage_hostid;
                    tempHost1.hostname = h.hostname;
                    tempHost1.displayname = h.displayname;
                    tempList.AddOrReplaceHost(tempHost1);
                }
                if (sourceHost.machineType == "PhysicalMachine")
                {
                  
                   
                   

                    Cxapicalls cxapi = new Cxapicalls();
                    foreach (Host h in tempList._hostList)
                    {
                        Host temp = new Host();
                        temp.inmage_hostid = h.inmage_hostid;
                        if (cxapi.Post(temp, "GetHostInfo") == false)
                        {
                            Trace.WriteLine(DateTime.Now + "\t Failed to gethostInof");
                        }
                        else
                        {
                            temp = Cxapicalls.GetHost;
                            h.disks.list = temp.disks.list;
                            h.cluster = temp.cluster;
                            h.displayname = temp.displayname;
                            h.clusterMBRFiles = temp.clusterMBRFiles;
                            h.clusterNodes = temp.clusterNodes;
                            h.hostname = temp.hostname;
                        }
                    }

                    foreach (Host h in tempList._hostList)
                    {
                        if (h.cluster == "yes")
                        {
                            foreach (Host sourceHost1 in sourceHostList._hostList)
                            {

                                foreach (Hashtable hash in h.disks.list)
                                {
                                    foreach (Hashtable sourceHash in sourceHost1.disks.list)
                                    {
                                        if (hash["Size"] != null && hash["Size"].ToString() != "0" && hash["disk_signature"] != null && hash["disk_signature"].ToString() != "0" && sourceHash["disk_signature"] != null && !sourceHash["disk_signature"].ToString().ToUpper().Contains("SYSTEM"))
                                        {
                                            if (hash["disk_signature"].ToString() == sourceHash["disk_signature"].ToString())
                                            {
                                                sourceHash["Size"] = hash["Size"];
                                                Trace.WriteLine(DateTime.Now + "\t Updated size for the the disk " + sourceHash["Name"].ToString() + "\t signture" + sourceHash["disk_signature"].ToString() + "\t Host " + sourceHost1.hostname);

                                            }
                                        }
                                    }
                                }

                            }
                        }
                    }

                }

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
                rootNodes1 = document1.GetElementsByTagName("root");
                diskNodes = document1.GetElementsByTagName("disk");

                Cxapicalls cxApi = new Cxapicalls();
                foreach (Host h1 in selectedSourceList._hostList)
                {
                    Host tempHost = new Host();
                    tempHost.inmage_hostid = h1.inmage_hostid;

                    cxApi.Post( tempHost, "GetHostInfo");
                    tempHost = Cxapicalls.GetHost;
                    Trace.WriteLine(DateTime.Now + "\t Old mbr file path " + h1.mbr_path + " \t " + h1.displayname);
                    h1.mbr_path = tempHost.mbr_path;
                    h1.clusterMBRFiles = tempHost.clusterMBRFiles;
                    Trace.WriteLine(DateTime.Now + "\t New mbr file path " + h1.mbr_path + " \t " + h1.displayname);
                    if (h1.displayname == null || h1.displayname.Length == 0)
                    {
                        h1.hostname = h1.hostname;
                    }
                }

                foreach (Host h1 in selectedSourceList._hostList)
                {
                    foreach (XmlNode node in hostNodes1)
                    {
                        if (h1.hostname.ToUpper() == node.Attributes["hostname"].Value.ToUpper() || h1.displayname.ToUpper() == node.Attributes["display_name"].Value.ToUpper())
                        {
                            node.Attributes["mbr_path"].Value = h1.mbr_path;
                            if (h1.cluster == "yes")
                            {
                                node.Attributes["clusternodes_mbrfiles"].Value = h1.clusterMBRFiles;
                            }
                            Trace.WriteLine(DateTime.Now + "\t Updated the mbr path " + h1.mbr_path);
                            if (node.Attributes["inmage_hostid"] != null && node.Attributes["inmage_hostid"].Value.Length == 0)
                            {
                                node.Attributes["inmage_hostid"].Value = h1.inmage_hostid;
                            }
                            else
                            {
                                XmlElement ele = (XmlElement)node;
                                ele.SetAttribute("inmage_hostid", h1.inmage_hostid);

                            }
                            if (node.Attributes["mbr_path"] != null)
                            {
                                node.Attributes["mbr_path"].Value = h1.mbr_path;
                            }
                            else
                            {
                                XmlElement ele = (XmlElement)node;
                                ele.SetAttribute("mbr_path", h1.mbr_path);
                            }
                            if (node.Attributes["clusternodes_mbrfiles"] != null)
                            {
                                node.Attributes["clusternodes_mbrfiles"].Value = h1.clusterMBRFiles;
                            }
                            else
                            {
                                XmlElement ele = (XmlElement)node;
                                ele.SetAttribute("clusternodes_mbrfiles", h1.clusterMBRFiles);
                            }

                        }
                    }

                    if (ResumeForm.selectedActionForPlan != "ESXRESIZE")
                    {
                        foreach (XmlNode node in hostNodes1)
                        {
                            foreach (Host h in tempList._hostList)
                            {
                                if (h.displayname == node.Attributes["display_name"].Value)
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
                                                                //Trace.WriteLine(DateTime.Now + "\t Comparing the disk_signature " + hash["disk_signature"].ToString() + "\t xml value is " + diskNode.Attributes["disk_signature"].Value);
                                                                if (hash["disk_signature"].ToString() == diskNode.Attributes["disk_signature"].Value.ToString())
                                                                {
                                                                    if (hash["Size"] != null && hash["Size"].ToString() != "0")
                                                                    {
                                                                        Trace.WriteLine(DateTime.Now + "\t updated size is " + hash["Size"].ToString() + "\t signature " + hash["disk_signature"].ToString());
                                                                        diskNode.Attributes["size"].Value = hash["Size"].ToString();
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                            catch (Exception ex)
                                            {
                                                Trace.WriteLine(DateTime.Now + "\t Failed to update size " + ex.Message);
                                            }
                                        }
                                    }

                                }
                            }
                        }

                    }
                }
                document1.Save(xmlfile1);
            }
            AllServersForm allServersFrom = new AllServersForm("Resize", sourceHost.plan);
            Cxapicalls cxApis = new Cxapicalls();
            if (ResumeForm.selectedActionForPlan == "ESXRESIZE")
            {
                if (resizeSourceChecks == false)
                {
                    int returncodeofSourceCommand = 0;
                    
                    if (allServersFrom.VerfiyEsxCreds(sourceHost.esxIp, "source") == false)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Failed to update source esx meta data ");
                        return false;
                    }


                    returncodeofSourceCommand = esx.GetEsxInfoForResize(sourceHost.esxIp);
                    if (returncodeofSourceCommand == 3)
                    {
                        SourceEsxDetailsPopUpForm source = new SourceEsxDetailsPopUpForm();
                        source.sourceEsxIpText.ReadOnly = false;
                        source.sourceEsxIpText.Text = sourceHost.esxIp;
                        source.ShowDialog();
                        if (source.canceled == false)
                        {
                            sourceHost.esxIp = source.sourceEsxIpText.Text;
                            sourceHost.esxUserName = source.userNameText.Text;
                            sourceHost.esxPassword = source.passWordText.Text;
                            source.ipLabel.Text = "Source vSphere/vCenter IP/Name *";
                            
                            cxApis. UpdaterCredentialsToCX(sourceHost.esxIp, sourceHost.esxUserName, sourceHost.esxPassword);
                            if (esx.GetEsxInfoForResize(sourceHost.esxIp) != 0)
                            {
                                Trace.WriteLine(DateTime.Now + "\t Failed to get the source info for resize operation");
                                return false;
                            }
                            else
                            {

                                resizeSourceChecks = true;
                            }
                        }
                    }
                    else if (returncodeofSourceCommand != 0)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Failed to get source info for resize operation");
                        return false;
                    }
                    else
                    {
                        resizeSourceChecks = true;
                    }
                }
            }
            else
            {
                resizeSourceChecks = true;
            }
            if (resizeTargetExtend == false)
            {
                int extendDiskSize = 0;
                allServersFrom.VerfiyEsxCreds(masterHost.esxIp, "target");
                extendDiskSize = esx.ExecuteCreateTargetGuestScript(masterHost.esxIp,  OperationType.Resize);
                if (extendDiskSize == 3)
                {
                    SourceEsxDetailsPopUpForm source = new SourceEsxDetailsPopUpForm();
                    source.sourceEsxIpText.ReadOnly = false;
                    source.sourceEsxIpText.Text = sourceHost.targetESXIp;
                    source.ipLabel.Text = "Target vSphere/vCenter IP/Name *";
                    source.ShowDialog();
                    if (source.canceled == false)
                    {
                        sourceHost.targetESXIp = source.sourceEsxIpText.Text;
                        sourceHost.targetESXUserName = source.userNameText.Text;
                        sourceHost.targetESXPassword = source.passWordText.Text;
                        masterHost.esxIp = source.sourceEsxIpText.Text;
                        masterHost.esxUserName = source.userNameText.Text;
                        masterHost.esxPassword = source.passWordText.Text;
                        cxApis. UpdaterCredentialsToCX(masterHost.esxIp, masterHost.esxUserName, masterHost.esxPassword);
                        if (esx.ExecuteCreateTargetGuestScript(masterHost.esxIp, OperationType.Resize) != 0)
                        {
                            Trace.WriteLine(DateTime.Now + "\t Failed to get the target info for resize operation");
                            return false;
                        }
                        else
                        {
                            extendDiskSize = 0;
                            resizeTargetExtend = true;
                        }
                    }
                }
                if (extendDiskSize != 0)
                {
                    resizeTargetExtend = false;
                    Trace.WriteLine(DateTime.Now + "\t Failed to extend the disk size ");
                    return false;
                }
                else
                {
                    resizeTargetExtend = true;
                }
            }
            if (updateMasterConfigfile == false)
            {
                Trace.WriteLine(DateTime.Now + "\t Successfully completed copying files to Planname folder");
                MasterConfigFile masterCOnfig = new MasterConfigFile();
                if (masterCOnfig.UpdateMasterConfigFile(latestFilePath + "\\Resize.xml") == true)
                {
                    Trace.WriteLine(DateTime.Now + "\t Successfully updated the MasterConfigfile");
                    updateMasterConfigfile = true;
                }
                else
                {
                    Trace.WriteLine(DateTime.Now + "\t Failed to update MasterConfigfile.xml");
                    updateMasterConfigfile = false;
                    return false;

                }
            }

            if (File.Exists(WinTools.FxAgentPath() + "\\vContinuum\\Latest\\Resize.xml"))
            {
                File.Copy(WinTools.FxAgentPath() + "\\vContinuum\\Latest\\Resize.xml", WinTools.FxAgentPath() + "\\Failover\\Data\\" + directoryNameinXmlFile + "\\Resize.xml", true);
            }

            UploadFileAndPrePareInputText(_masterHost.os);
            if (esxutilwinExecution == false)
            {
                EsxUtilWinExecute(ref masterHost);
            }

            return true;
        }


        public bool EsxUtilWinExecute(ref Host masterHost)
        {
            try
            {




                string cxIP = WinTools.GetcxIPWithPortNumber();
                string path = WinTools.FxAgentPath() + "\\Failover\\Data\\" + directoryNameinXmlFile;
                Cxapicalls cxapi = new Cxapicalls();
                try
                {
                    foreach (Host h in selectedSourceList._hostList)
                    {
                        if (h.plan != null)
                        {
                            masterHost.plan = h.plan;
                            break;
                        }
                    }

                    
                    WinPreReqs winPreReqs = new WinPreReqs("", "", "", "");
                    ArrayList planNameList = new ArrayList();                   
                    winPreReqs.GetPlanNames( planNameList, WinTools.GetcxIPWithPortNumber());
                    planNameList = WinPreReqs.GetPlanlist;
                    if (_appName == AppName.Adddisks)
                    {
                        
                        Host h1 = new Host();
                        h1.plan = masterHost.plan;
                        h1.operationType = OperationType.Additionofdisk;
                        cxapi.Post( h1, "RemoveExecutionStep");
                        h1 = Cxapicalls.GetHost;
                    }
                    else if(_appName == AppName.Resize)
                    {
                        Host h1 = new Host();
                        h1.plan = masterHost.plan + "_resize";
                        h1.operationType = OperationType.Resize;
                        cxapi.Post( h1, "RemoveExecutionStep");
                        h1 = Cxapicalls.GetHost;
                    }
                    else if (_appName == AppName.Removedisk )
                    {
                        Host h1 = new Host();
                        h1.plan = masterHost.plan + "_Remove";
                        h1.operationType = OperationType.Removedisk;
                        cxapi.Post( h1, "RemoveExecutionStep");
                        h1 = Cxapicalls.GetHost;
                    }
                    else if (_appName == AppName.Removevolume)
                    {
                        Host h1 = new Host();
                        h1.plan = masterHost.plan + "_Remove_volume";
                        h1.operationType = OperationType.Removevolume;
                        cxapi.Post( h1, "RemoveExecutionStep");
                        h1 = Cxapicalls.GetHost;

                    }
                    else if (_appName == AppName.Esx || _appName == AppName.Bmr || _appName == AppName.Offlinesync)
                    {
                        foreach (string s in planNameList)
                        {
                            if (s == masterHost.plan)
                            {
                                Host h1 = new Host();
                                h1.plan = masterHost.plan;
                                h1.operationType = OperationType.Initialprotection;
                                cxapi.Post( h1, "RemoveExecutionStep");
                                h1 = Cxapicalls.GetHost;
                                break;
                            }
                        }
                    }
                    else if (_appName == AppName.Resume)
                    {
                        foreach (string s in planNameList)
                        {
                            if (s == masterHost.plan)
                            {
                                Host h1 = new Host();
                                h1.plan = masterHost.plan;
                                h1.operationType = OperationType.Resume;
                                cxapi.Post( h1, "RemoveExecutionStep");
                                h1 = Cxapicalls.GetHost;
                                break;
                            }
                        }
                    }
                    else if (_appName == AppName.Failback || _appName == AppName.V2P)
                    {
                        foreach (string s in planNameList)
                        {
                            if (s == masterHost.plan)
                            {
                                Host h1 = new Host();
                                h1.plan = masterHost.plan;
                                h1.operationType = OperationType.Failback;
                                cxapi.Post( h1, "RemoveExecutionStep");
                                h1 = Cxapicalls.GetHost;
                                break;
                            }
                        }
                    }                    
                }
                catch (Exception ex)
                {
                    Trace.WriteLine(DateTime.Now + "\t Failed to delete planname database " + ex.Message);
                }
                if (_appName == AppName.Esx || _appName == AppName.Bmr || _appName == AppName.Offlinesync || _appName == AppName.Adddisks || _appName == AppName.Resume || _appName == AppName.Failback || _appName == AppName.V2P || _appName == AppName.Resize || _appName == AppName.Removedisk || _appName == AppName.Removevolume)
                {
                    foreach (Host h in selectedSourceList._hostList)
                    {
                        if (h.plan != null)
                        {
                            masterHost.plan = h.plan;
                            break;
                        }
                    }
                    if (masterHost.vx_agent_path == null)
                    {
                        Host tempHost = new Host();
                        tempHost.ip = masterHost.ip;
                        tempHost.hostname = masterHost.hostname;
                        tempHost.inmage_hostid = masterHost.inmage_hostid;
                        WinPreReqs winpre = new WinPreReqs("", "", "", "");
                        winpre.GetDetailsFromcxcli( tempHost, cxIP);
                        tempHost = WinPreReqs.GetHost;
                        masterHost.vx_agent_path = tempHost.vx_agent_path;
                        // masterHost.inmage_hostid = tempHost.inmage_hostid;
                        masterHost.ip = tempHost.ip;
                        masterHost.inmage_hostid = tempHost.inmage_hostid;
                    }
                    Trace.WriteLine(DateTime.Now + "\t Getting plan id form the cx api ");
                    masterHost.runEsxUtilCommand = true;
                    if (_appName != AppName.Failback && _appName != AppName.V2P && _appName != AppName.Removevolume && _appName != AppName.Removedisk)
                    {
                        if (masterHost.os == OStype.Windows)
                        {
                            masterHost.commandtoExecute = "'" + masterHost.vx_agent_path + "\\EsxUtilWin.exe" + "'" + " -role target -d " + "'" + masterHost.vx_agent_path + "\\FailOver\\Data\\" + directoryNameinXmlFile + "'" + " -cxpath " + "'" + "/home/svsystems/vcon/" + directoryNameinXmlFile + "'";
                        }
                        else
                        {
                            masterHost.commandtoExecute = "'" + masterHost.vx_agent_path + "bin/EsxUtil" + "'" + " -replication linux -d " + "'" + masterHost.vx_agent_path + "failover_data/" + directoryNameinXmlFile + "'" + " -cxpath " + "'" + "/home/svsystems/vcon/" + directoryNameinXmlFile + "'";
                        }
                    }
                    else if (_appName == AppName.Removedisk)
                    {

                        Host sourceHost = new Host();
                        sourceHost = (Host)selectedSourceList._hostList[0];

                        if (masterHost.os == OStype.Windows)
                        {
                            if (sourceHost.machineType == "PhysicalMachine")
                            {
                                masterHost.commandtoExecute = "'" + masterHost.vx_agent_path + "\\EsxUtil.exe" + "'" + " -removepair -d " + "'" + masterHost.vx_agent_path + "\\FailOver\\Data\\" + directoryNameinXmlFile + "'" + " -cxpath " + "'" + "/home/svsystems/vcon/" + directoryNameinXmlFile + "'" + " -op diskremove -p2v";
                            }
                            else
                            {
                                masterHost.commandtoExecute = "'" + masterHost.vx_agent_path + "\\EsxUtil.exe" + "'" + " -removepair -d " + "'" + masterHost.vx_agent_path + "\\FailOver\\Data\\" + directoryNameinXmlFile + "'" + " -cxpath " + "'" + "/home/svsystems/vcon/" + directoryNameinXmlFile + "'" + " -op diskremove";
                            }
                        }
                        else
                        {
                            masterHost.commandtoExecute = "'" + masterHost.vx_agent_path + "bin/EsxUtil" + "'" + " -removepair -d " + "'" + masterHost.vx_agent_path + "failover_data/" + directoryNameinXmlFile + "'" + " -cxpath " + "'" + "/home/svsystems/vcon/" + directoryNameinXmlFile + "'" + " -op volumeremove";
                        }

                    }
                    else if (_appName == AppName.Removevolume)
                    {
                        if (masterHost.os == OStype.Windows)
                        {
                            masterHost.commandtoExecute = "'" + masterHost.vx_agent_path + "\\EsxUtil.exe" + "'" + " -removepair -d " + "'" + masterHost.vx_agent_path + "\\FailOver\\Data\\" + directoryNameinXmlFile + "'" + " -cxpath " + "'" + "/home/svsystems/vcon/" + directoryNameinXmlFile + "'" + " -op volumeremove";
                        }
                        else
                        {
                            masterHost.commandtoExecute = "'" + masterHost.vx_agent_path + "bin/EsxUtil" + "'" + " -removepair -d " + "'" + masterHost.vx_agent_path + "failover_data/" + directoryNameinXmlFile + "'" + " -cxpath " + "'" + "/home/svsystems/vcon/" + directoryNameinXmlFile + "'" + " -op diskremove";
                        }
                    }
                    else
                    {
                        if (masterHost.os == OStype.Windows)
                        {
                            masterHost.commandtoExecute = "'" + masterHost.vx_agent_path + "\\EsxUtilWin.exe" + "'" + " -role failback -d " + "'" + masterHost.vx_agent_path + "\\FailOver\\Data\\" + directoryNameinXmlFile + "'" + " -cxpath " + "'" + "/home/svsystems/vcon/" + directoryNameinXmlFile + "'";
                        }
                        else
                        {
                            masterHost.commandtoExecute = "'" + masterHost.vx_agent_path + "bin/EsxUtil" + "'" + " -replication linux -d " + "'" + masterHost.vx_agent_path + "failover_data/" + directoryNameinXmlFile + "'" + " -cxpath " + "'" + "/home/svsystems/vcon/" + directoryNameinXmlFile + "'";
                        }
                    }
                    if (_appName == AppName.Removedisk)
                    {
                        masterHost.tardir = fxFailOverDataPath + "\\" + directoryNameinXmlFile;
                        if (masterHost.os == OStype.Windows)
                        {
                            masterHost.srcDir = masterHost.vx_agent_path + "\\Failover\\Data\\" + directoryNameinXmlFile;
                        }
                        else
                        {
                            masterHost.srcDir = masterHost.vx_agent_path + "failover_data/" + directoryNameinXmlFile;
                        }
                    }
                    else
                    {
                        if (masterHost.os == OStype.Windows)
                        {
                            masterHost.srcDir = masterHost.vx_agent_path + "\\Failover\\Data\\" + directoryNameinXmlFile;
                            masterHost.tardir = masterHost.vx_agent_path + "\\Failover\\Data\\" + directoryNameinXmlFile;
                        }
                        else
                        {
                            masterHost.srcDir = masterHost.vx_agent_path + "failover_data/" + directoryNameinXmlFile;
                            masterHost.tardir = masterHost.vx_agent_path + "failover_data/" + directoryNameinXmlFile;

                        }
                    }


                    masterHost.initializingProtectionPlan = null;
                    masterHost.downloadingConfigurationfiles = null;
                    masterHost.preparingMasterTargetDisks = null;
                    masterHost.updatingDiskLayouts = null;
                    masterHost.activateProtectionPlan = null;

                    masterHost.initializingRemoveDiskPlan = null;
                    masterHost.downloadRemoveDiskFiles = null;
                    masterHost.removeDisksFromMt = null;
                    masterHost.deleteReplicationPairs = null;

                    try
                    {
                        masterHost.taskList = new ArrayList();
                        // by the time this is initializing the plan......
                        if (statusDataGridView.RowCount != 0)
                        {
                            statusDataGridView.Rows[0].Cells[0].Value = completeTask;
                            statusDataGridView.Rows[1].Cells[0].Value = currentTask;
                            currentRow = 1;
                            for (int i = 2; i <= statusDataGridView.RowCount; i++)
                            {
                                statusDataGridView.Rows[i].Cells[0].Value = pending;
                            }
                        }
                    }
                    catch (Exception ex)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Failed to update datagridview and arraylist " + ex.Message);
                    }
                    if (_appName == AppName.Adddisks)
                    {
                        masterHost.commandtoExecute = masterHost.commandtoExecute + " -op adddisk";
                        getplanidapi = cxapi.PostFXJob( masterHost,  OperationType.Additionofdisk);
                        masterHost = Cxapicalls.GetHost;
                    }
                    else if (_appName == AppName.Resize)
                    {
                        masterHost.commandtoExecute = masterHost.commandtoExecute + " -op volumeresize";
                        getplanidapi = cxapi.PostFXJob( masterHost, OperationType.Resize);
                        masterHost = Cxapicalls.GetHost;
                        try
                        {
                            ResumeForm.breifLog.WriteLine(DateTime.Now + "\t User selected Resize operationa and going to set fx job ");
                            foreach (Host h in selectedSourceList._hostList)
                            {
                                ResumeForm.breifLog.WriteLine(DateTime.Now + " Source display name " + h.displayname + " Mt display name " + h.masterTargetDisplayName);
                            }
                            ResumeForm.breifLog.WriteLine(DateTime.Now + " \t Successfully completed by setting jobs to the above selected vms with plan name " + masterHost.plan);
                            ResumeForm.breifLog.Flush();
                        }
                        catch (Exception ex)
                        {
                            Trace.WriteLine(DateTime.Now + "\t Failed to write in to Brief log " + ex.Message);
                        }


                    }
                    else if (_appName == AppName.Failback || _appName == AppName.V2P)
                    {
                        masterHost.commandtoExecute = masterHost.commandtoExecute + " -op failback";
                        getplanidapi = cxapi.PostFXJob( masterHost,  OperationType.Failback);
                        masterHost = Cxapicalls.GetHost;
                    }
                    else if (_appName == AppName.Resume)
                    {
                        masterHost.commandtoExecute = masterHost.commandtoExecute + " -op resume";
                        getplanidapi = cxapi.PostFXJob( masterHost,  OperationType.Resume);
                        masterHost = Cxapicalls.GetHost;
                    }
                    else if(_appName == AppName.Removedisk )
                    {
                        
                        masterHost.perlCommand = "\"" + WinTools.FxAgentPath() + "\\vContinuum\\Scripts\\Recovery.pl" + "\"" + " --server " + masterHost.esxIp  + " --directory " + "\"" + fxFailOverDataPath + "\\" + directoryNameinXmlFile + "\"" + " --removedisk yes --cxpath " + "\"" + "/home/svsystems/vcon/" + directoryNameinXmlFile + "\"";
                        PrepareBatchScript(masterHost);
                        masterHost.perlCommand = "'" + fxFailOverDataPath + "\\" + directoryNameinXmlFile + "\\InMage_Script.bat" + "'";
                        getplanidapi = cxapi.PostFXJob( masterHost, OperationType.Removedisk);
                        masterHost = Cxapicalls.GetHost;
                        try
                        {
                            ResumeForm.breifLog.WriteLine(DateTime.Now + "\t User selected Remove disk operationa and going to set fx job ");
                            foreach (Host h in selectedSourceList._hostList)
                            {
                                ResumeForm.breifLog.WriteLine(DateTime.Now + " Source display name " + h.displayname + " Mt display name " + h.masterTargetDisplayName);
                            }
                            ResumeForm.breifLog.WriteLine(DateTime.Now + " \t Successfully completed by setting jobs to the above selected vms with plan name " + masterHost.plan);
                            ResumeForm.breifLog.Flush();
                        }
                        catch (Exception ex)
                        {
                            Trace.WriteLine(DateTime.Now + "\t Failed to write in to Brief log " + ex.Message);
                        }
                    }
                    else if (_appName == AppName.Removevolume)
                    {
                        getplanidapi = cxapi.PostFXJob( masterHost,  OperationType.Removevolume);
                        masterHost = Cxapicalls.GetHost;
                        try
                        {
                            ResumeForm.breifLog.WriteLine(DateTime.Now + "\t User selected Remove volume operationa and going to set fx job ");
                            foreach (Host h in selectedSourceList._hostList)
                            {
                                ResumeForm.breifLog.WriteLine(DateTime.Now + " Source display name " + h.displayname + " Mt display name " + h.masterTargetDisplayName);
                            }
                            ResumeForm.breifLog.WriteLine(DateTime.Now + " \t Successfully completed by setting jobs to the above selected vms with plan name " + masterHost.plan);
                            ResumeForm.breifLog.Flush();
                        }
                        catch (Exception ex)
                        {
                            Trace.WriteLine(DateTime.Now + "\t Failed to write in to Brief log " + ex.Message);
                        }
                    }
                    else
                    {
                        getplanidapi = cxapi.PostFXJob( masterHost,  OperationType.Initialprotection);
                        masterHost = Cxapicalls.GetHost;
                    }
                }
                if (getplanidapi == true)
                {


                    planId = masterHost.planid;
                    //if (_appName == AppName.Bmr || _appName == AppName.Esx)
                    //{
                    //    Cxapicalls cxAPi = new Cxapicalls();
                    //    if (cxAPi.PostForTeleMetering(selectedSourceList, masterHost,OperationType.Initialprotection ) == true)
                    //    {
                    //        Trace.WriteLine(DateTime.Now + "\t Successfully updated plan info to CX ");
                    //    }
                    //    else
                    //    {
                    //        Trace.WriteLine(DateTime.Now + "\t Failed to updated plan info to CX ");
                    //    }
                    //}
                    //if(_appName == AppName.Recover )
                    //{
                    //    Cxapicalls cxAPi = new Cxapicalls();
                    //    if (cxAPi.PostForTeleMetering(selectedSourceList, masterHost, OperationType.Recover) == true)
                    //    {
                    //        Trace.WriteLine(DateTime.Now + "\t Successfully updated plan info to CX ");
                    //    }
                    //    else
                    //    {
                    //        Trace.WriteLine(DateTime.Now + "\t Failed to updated plan info to CX ");
                    //    }
                    //}
                    //if (cxapi.Post(ref masterHost, "InsertScriptPolicy") == true)
                    //{
                    //    _policyAPI = true;
                    //    _scriptID = masterHost.requestId;
                    //    Trace.WriteLine(DateTime.Now + "\t Successfully set the script policy and request id is " + _scriptID);
                    //}
                }
                else
                {
                    masterHost.initializingProtectionPlan = "Failed";
                    statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                    Trace.WriteLine(DateTime.Now + "\t Failed to get plan iod using cx api");
                    return false;
                }
                Thread.Sleep(45000);
                while (statusApi == false)
                {
                    bool cxstatusAPI = false;
                    if (_appName == AppName.Adddisks)
                    {
                        masterHost.operationType = OperationType.Additionofdisk;
                    }
                    else if (_appName == AppName.Resize)
                    {
                        masterHost.operationType = OperationType.Resize;
                    }
                    else if (_appName == AppName.Resume)
                    {
                        masterHost.operationType = OperationType.Resume;
                    }
                    else if (_appName == AppName.Failback || _appName == AppName.V2P)
                    {
                        masterHost.operationType = OperationType.Failback;
                    }
                    else if (_appName == AppName.Bmr || _appName == AppName.Esx || _appName == AppName.Offlinesync)
                    {
                        masterHost.operationType = OperationType.Initialprotection;
                    }
                    else if (_appName == AppName.Removedisk)
                    {
                        masterHost.operationType = OperationType.Removedisk;
                    }
                    else if (_appName == AppName.Removevolume)
                    {
                        masterHost.operationType = OperationType.Removevolume;
                    }
                    cxstatusAPI = cxapi.Post( masterHost, "MonitorESXProtectionStatus");
                    masterHost = Cxapicalls.GetHost;

                    if (_appName == AppName.Resize)
                    {
                        foreach (Hashtable hash in masterHost.taskList)
                        {
                            if (hash["Name"] != null)
                            {
                                if (hash["Name"].ToString() == "Initializing volume resize protection plan")
                                {
                                    if (hash["TaskStatus"] != null)
                                    {
                                        if (hash["TaskStatus"].ToString() == "Completed" || hash["TaskStatus"].ToString() == "Complete")
                                        {
                                            _masterHost.initializingResizePlan = "Completed";
                                        }
                                        else if (hash["TaskStatus"].ToString() == "Failed")
                                        {
                                            _masterHost.initializingResizePlan = "Failed";
                                        }
                                        else if (hash["TaskStatus"].ToString() == "Queued")
                                        {
                                            _masterHost.initializingResizePlan = "Notstarted";
                                        }
                                        else if (hash["TaskStatus"].ToString() == "InProgress")
                                        {
                                            _masterHost.initializingResizePlan = "Pending";
                                        }
                                        else if (hash["TaskStatus"].ToString() == "Warning")
                                        {
                                            _masterHost.initializingResizePlan = "Warning";
                                        }
                                    }
                                }
                                if (hash["Name"].ToString() == "Downloading Configuration Files")
                                {
                                    if (hash["TaskStatus"].ToString() == "Completed" || hash["TaskStatus"].ToString() == "Complete")
                                    {
                                        _masterHost.resizedownloadfiles = "Completed";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Failed")
                                    {
                                        _masterHost.resizedownloadfiles = "Failed";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Queued")
                                    {
                                        _masterHost.resizedownloadfiles = "Notstarted";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "InProgress")
                                    {
                                        _masterHost.resizedownloadfiles = "Pending";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Warning")
                                    {
                                        _masterHost.resizedownloadfiles = "Warning";
                                    }
                                }
                                if (hash["Name"].ToString() == "Pausing the protection pairs")
                                {
                                    if (hash["TaskStatus"].ToString() == "Completed" || hash["TaskStatus"].ToString() == "Complete")
                                    {
                                        _masterHost.pauseReplicationPairs = "Completed";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Failed")
                                    {
                                        _masterHost.pauseReplicationPairs = "Failed";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Queued")
                                    {
                                        _masterHost.pauseReplicationPairs = "Notstarted";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "InProgress")
                                    {
                                        _masterHost.pauseReplicationPairs = "Pending";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Warning")
                                    {
                                        _masterHost.pauseReplicationPairs = "Warning";
                                    }
                                }
                                if (hash["Name"].ToString() == "Preparing Master Target Disks")
                                {
                                    if (hash["TaskStatus"].ToString() == "Completed" || hash["TaskStatus"].ToString() == "Complete")
                                    {
                                        _masterHost.resizePreParingMasterTargetDisks = "Completed";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Failed")
                                    {
                                        _masterHost.resizePreParingMasterTargetDisks = "Failed";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Queued")
                                    {
                                        _masterHost.resizePreParingMasterTargetDisks = "Notstarted";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "InProgress")
                                    {
                                        _masterHost.resizePreParingMasterTargetDisks = "Pending";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Warning")
                                    {
                                        _masterHost.resizePreParingMasterTargetDisks = "Warning";
                                    }
                                }
                                if (hash["Name"].ToString() == "Updating Disk Layouts")
                                {
                                    if (hash["TaskStatus"].ToString() == "Completed" || hash["TaskStatus"].ToString() == "Complete")
                                    {
                                        _masterHost.resizeUpdateDiskLayouts = "Completed";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Failed")
                                    {
                                        _masterHost.resizeUpdateDiskLayouts = "Failed";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Queued")
                                    {
                                        _masterHost.resizeUpdateDiskLayouts = "Notstarted";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "InProgress")
                                    {
                                        _masterHost.resizeUpdateDiskLayouts = "Pending";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Warning")
                                    {
                                        _masterHost.resizeUpdateDiskLayouts = "Warning";
                                    }
                                }
                                if (hash["Name"].ToString() == "Resuming the protection pairs")
                                {
                                    if (hash["TaskStatus"].ToString() == "Completed" || hash["TaskStatus"].ToString() == "Complete")
                                    {
                                        _masterHost.resumeReplicationPairs = "Completed";
                                        
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Failed")
                                    {
                                        _masterHost.resumeReplicationPairs = "Failed";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Queued")
                                    {
                                        _masterHost.resumeReplicationPairs = "Notstarted";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "InProgress")
                                    {
                                        _masterHost.resumeReplicationPairs = "Pending";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Warning")
                                    {
                                        _masterHost.resumeReplicationPairs = "Warning";
                                    }
                                }

                            }
                            if (hash["LogPath"] != null && hash["TaskStatus"] != null && hash["TaskStatus"].ToString() == "Failed")
                            {
                                logPath = hash["LogPath"].ToString();
                                DownloadRequiredFiles();
                            }
                        }

                        if (_masterHost.resumeReplicationPairs == "Completed")
                        {
                            statusApi = true;
                            esxutilwinExecution = true;
                            statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                            break;
                        }
                        if (_masterHost.initializingResizePlan == "Failed")
                        {
                            statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                            break;
                        }
                        if (_masterHost.resizedownloadfiles == "Failed")
                        {
                            statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                            break;
                        }
                        if (_masterHost.pauseReplicationPairs == "Failed")
                        {
                            statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                            break;
                        }
                        if (_masterHost.resizePreParingMasterTargetDisks == "Failed")
                        {
                            statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                            break;
                        }
                        if (_masterHost.resizeUpdateDiskLayouts == "Failed")
                        {
                            statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                            break;
                        }
                        if (_masterHost.resumeReplicationPairs == "Failed")
                        {
                            statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                            break;
                        }
                        statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                        Thread.Sleep(35000);
                    }
                    else if (_appName == AppName.Removedisk)
                    {
                        foreach (Hashtable hash in masterHost.taskList)
                        {
                            if (hash["Name"] != null)
                            {
                                if (hash["Name"].ToString() == "Initializing remove replication pairs Plan")
                                {
                                    if (hash["TaskStatus"] != null)
                                    {
                                        if (hash["TaskStatus"].ToString() == "Completed" || hash["TaskStatus"].ToString() == "Complete")
                                        {
                                            _masterHost.initializingRemoveDiskPlan = "Completed";
                                        }
                                        else if (hash["TaskStatus"].ToString() == "Failed")
                                        {
                                            _masterHost.initializingRemoveDiskPlan = "Failed";
                                        }
                                        else if (hash["TaskStatus"].ToString() == "Queued")
                                        {
                                            _masterHost.initializingRemoveDiskPlan = "Notstarted";
                                        }
                                        else if (hash["TaskStatus"].ToString() == "InProgress")
                                        {
                                            _masterHost.initializingRemoveDiskPlan = "Pending";
                                        }
                                        else if (hash["TaskStatus"].ToString() == "Warning")
                                        {
                                            _masterHost.initializingRemoveDiskPlan = "Warning";
                                        }
                                    }
                                }
                                if (hash["Name"].ToString() == "Downloading Configuration Files")
                                {
                                    if (hash["TaskStatus"].ToString() == "Completed" || hash["TaskStatus"].ToString() == "Complete")
                                    {
                                        _masterHost.downloadRemoveDiskFiles = "Completed";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Failed")
                                    {
                                        _masterHost.downloadRemoveDiskFiles = "Failed";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Queued")
                                    {
                                        _masterHost.downloadRemoveDiskFiles = "Notstarted";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "InProgress")
                                    {
                                        _masterHost.downloadRemoveDiskFiles = "Pending";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Warning")
                                    {
                                        _masterHost.downloadRemoveDiskFiles = "Warning";
                                    }
                                }
                                if (hash["Name"].ToString() == "Deleting replication pairs")
                                {
                                    if (hash["TaskStatus"].ToString() == "Completed" || hash["TaskStatus"].ToString() == "Complete")
                                    {
                                        _masterHost.deleteReplicationPairs = "Completed";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Failed")
                                    {
                                        _masterHost.deleteReplicationPairs = "Failed";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Queued")
                                    {
                                        _masterHost.deleteReplicationPairs = "Notstarted";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "InProgress")
                                    {
                                        _masterHost.deleteReplicationPairs = "Pending";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Warning")
                                    {
                                        _masterHost.deleteReplicationPairs = "Warning";
                                    }
                                }
                                if (hash["Name"].ToString() == "Detaching disks from MT and DR VM(s)")
                                {
                                    if (hash["TaskStatus"].ToString() == "Completed" || hash["TaskStatus"].ToString() == "Complete")
                                    {
                                        _masterHost.removeDisksFromMt = "Completed";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Failed")
                                    {
                                        _masterHost.removeDisksFromMt = "Failed";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Queued")
                                    {
                                        _masterHost.removeDisksFromMt = "Notstarted";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "InProgress")
                                    {
                                        _masterHost.removeDisksFromMt = "Pending";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Warning")
                                    {
                                        _masterHost.removeDisksFromMt = "Warning";
                                    }
                                }
                            }
                        }
                        if (_masterHost.removeDisksFromMt == "Completed")
                        {
                            statusApi = true;
                            esxutilwinExecution = true;
                            statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                            break;
                        }
                        if (_masterHost.initializingRemoveDiskPlan == "Failed")
                        {
                            statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                            break;
                        }
                        if (_masterHost.downloadRemoveDiskFiles == "Failed")
                        {
                            statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                            break;
                        }
                        if (_masterHost.deleteReplicationPairs == "Failed")
                        {
                            statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                            break;
                        }
                        if (_masterHost.removeDisksFromMt == "Failed")
                        {
                            statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                            break;
                        }

                        statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                        Thread.Sleep(35000);
                    }
                    else if(_appName == AppName.Removevolume)
                    {
                        foreach (Hashtable hash in masterHost.taskList)
                        {
                            if (hash["Name"] != null)
                            {
                                if (hash["Name"].ToString() == "Initializing remove replication pairs Plan")
                                {
                                    if (hash["TaskStatus"] != null)
                                    {
                                        if (hash["TaskStatus"].ToString() == "Completed" || hash["TaskStatus"].ToString() == "Complete")
                                        {
                                            _masterHost.initializingRemoveDiskPlan = "Completed";
                                        }
                                        else if (hash["TaskStatus"].ToString() == "Failed")
                                        {
                                            _masterHost.initializingRemoveDiskPlan = "Failed";
                                        }
                                        else if (hash["TaskStatus"].ToString() == "Queued")
                                        {
                                            _masterHost.initializingRemoveDiskPlan = "Notstarted";
                                        }
                                        else if (hash["TaskStatus"].ToString() == "InProgress")
                                        {
                                            _masterHost.initializingRemoveDiskPlan = "Pending";
                                        }
                                        else if (hash["TaskStatus"].ToString() == "Warning")
                                        {
                                            _masterHost.initializingRemoveDiskPlan = "Warning";
                                        }
                                    }
                                }
                                if (hash["Name"].ToString() == "Downloading Configuration Files")
                                {
                                    if (hash["TaskStatus"].ToString() == "Completed" || hash["TaskStatus"].ToString() == "Complete")
                                    {
                                        _masterHost.downloadRemoveDiskFiles = "Completed";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Failed")
                                    {
                                        _masterHost.downloadRemoveDiskFiles = "Failed";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Queued")
                                    {
                                        _masterHost.downloadRemoveDiskFiles = "Notstarted";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "InProgress")
                                    {
                                        _masterHost.downloadRemoveDiskFiles = "Pending";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Warning")
                                    {
                                        _masterHost.downloadRemoveDiskFiles = "Warning";
                                    }
                                }
                                if (hash["Name"].ToString() == "Deleting replication pairs")
                                {
                                    if (hash["TaskStatus"].ToString() == "Completed" || hash["TaskStatus"].ToString() == "Complete")
                                    {
                                        _masterHost.deleteReplicationPairs = "Completed";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Failed")
                                    {
                                        _masterHost.deleteReplicationPairs = "Failed";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Queued")
                                    {
                                        _masterHost.deleteReplicationPairs = "Notstarted";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "InProgress")
                                    {
                                        _masterHost.deleteReplicationPairs = "Pending";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Warning")
                                    {
                                        _masterHost.deleteReplicationPairs = "Warning";
                                    }
                                }
                                
                            }
                        }
                        if (_masterHost.deleteReplicationPairs == "Completed")
                        {
                            statusApi = true;
                            esxutilwinExecution = true;
                            statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                            break;
                        }
                        if (_masterHost.initializingRemoveDiskPlan == "Failed")
                        {
                            statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                            break;
                        }
                        if (_masterHost.downloadRemoveDiskFiles == "Failed")
                        {
                            statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                            break;
                        }
                        if (_masterHost.deleteReplicationPairs == "Failed")
                        {
                            statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                            break;
                        }
                       

                        statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                        Thread.Sleep(35000);
                    }
                    else
                    {

                        foreach (Hashtable hash in masterHost.taskList)
                        {
                            if (hash["Name"] != null)
                            {
                                if (hash["Name"].ToString() == "Initializing Protection Plan")
                                {
                                    if (hash["TaskStatus"] != null)
                                    {
                                        if (hash["TaskStatus"].ToString() == "Completed" || hash["TaskStatus"].ToString() == "Complete")
                                        {
                                            _masterHost.initializingProtectionPlan = "Completed";
                                        }
                                        else if (hash["TaskStatus"].ToString() == "Failed")
                                        {
                                            _masterHost.initializingProtectionPlan = "Failed";
                                        }
                                        else if (hash["TaskStatus"].ToString() == "Queued")
                                        {
                                            _masterHost.initializingProtectionPlan = "Notstarted";
                                        }
                                        else if (hash["TaskStatus"].ToString() == "InProgress")
                                        {
                                            _masterHost.initializingProtectionPlan = "Pending";
                                        }
                                        else if (hash["TaskStatus"].ToString() == "Warning")
                                        {
                                            _masterHost.initializingProtectionPlan = "Warning";
                                        }
                                    }
                                }
                                if (hash["Name"].ToString() == "Downloading Configuration Files")
                                {
                                    if (hash["TaskStatus"].ToString() == "Completed" || hash["TaskStatus"].ToString() == "Complete")
                                    {
                                        _masterHost.downloadingConfigurationfiles = "Completed";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Failed")
                                    {
                                        _masterHost.downloadingConfigurationfiles = "Failed";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Queued")
                                    {
                                        _masterHost.downloadingConfigurationfiles = "Notstarted";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "InProgress")
                                    {
                                        _masterHost.downloadingConfigurationfiles = "Pending";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Warning")
                                    {
                                        _masterHost.downloadingConfigurationfiles = "Warning";
                                    }
                                }
                                if (hash["Name"].ToString() == "Preparing Master Target Disks")
                                {
                                    if (hash["TaskStatus"].ToString() == "Completed" || hash["TaskStatus"].ToString() == "Complete")
                                    {
                                        _masterHost.preparingMasterTargetDisks = "Completed";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Failed")
                                    {
                                        _masterHost.preparingMasterTargetDisks = "Failed";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Queued")
                                    {
                                        _masterHost.preparingMasterTargetDisks = "Notstarted";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "InProgress")
                                    {
                                        _masterHost.preparingMasterTargetDisks = "Pending";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Warning")
                                    {
                                        _masterHost.preparingMasterTargetDisks = "Warning";
                                    }
                                }
                                if (hash["Name"].ToString() == "Updating Disk Layouts")
                                {
                                    if (hash["TaskStatus"].ToString() == "Completed" || hash["TaskStatus"].ToString() == "Complete")
                                    {
                                        _masterHost.updatingDiskLayouts = "Completed";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Failed")
                                    {
                                        _masterHost.updatingDiskLayouts = "Failed";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Queued")
                                    {
                                        _masterHost.updatingDiskLayouts = "Notstarted";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "InProgress")
                                    {
                                        _masterHost.updatingDiskLayouts = "Pending";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Warning")
                                    {
                                        _masterHost.updatingDiskLayouts = "Warning";
                                    }
                                }
                                if (hash["Name"].ToString() == "Activating Protection Plan")
                                {
                                    if (hash["TaskStatus"].ToString() == "Completed" || hash["TaskStatus"].ToString() == "Complete")
                                    {
                                        _masterHost.activateProtectionPlan = "Completed";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Failed")
                                    {
                                        _masterHost.activateProtectionPlan = "Failed";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Queued")
                                    {
                                        _masterHost.activateProtectionPlan = "Notstarted";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "InProgress")
                                    {
                                        _masterHost.activateProtectionPlan = "Pending";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Warning")
                                    {
                                        _masterHost.activateProtectionPlan = "Warning";
                                    }
                                }

                            }

                            if (hash["LogPath"] != null && hash["TaskStatus"] != null && hash["TaskStatus"].ToString() == "Failed")
                            {
                                logPath = hash["LogPath"].ToString();
                                DownloadRequiredFiles();
                            }
                        }
                        if (_masterHost.activateProtectionPlan == "Completed")
                        {
                            statusApi = true;
                            esxutilwinExecution = true;
                            statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                            break;
                        }
                        if (_masterHost.initializingProtectionPlan == "Failed")
                        {
                            statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                            break;
                        }
                        if (_masterHost.downloadingConfigurationfiles == "Failed")
                        {
                            statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                            break;
                        }
                        if (_masterHost.preparingMasterTargetDisks == "Failed")
                        {
                            statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                            break;
                        }
                        if (_masterHost.updatingDiskLayouts == "Failed")
                        {
                            statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                            break;
                        }
                        if (_masterHost.activateProtectionPlan == "Failed")
                        {
                            statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                            break;
                        }
                        statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                        Thread.Sleep(35000);

                    }
                }
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed in do work " + ex.Message);
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");

            }
            return true;
        }

      

        internal static bool RunningScriptInLinuxmt(Host masterHost, ref ArrayList diskList)
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
                    int k = 0;
                    while (k < 6)
                    {
                        if (cxapi.PostRequestStatusForDummyDisk(h, false) == false)
                        {
                            Thread.Sleep(60000);
                            k++;
                            Trace.WriteLine(DateTime.Now + "\t Going to sleep for 1 min for "+ k.ToString()+" after executing InsertScriptPolicy");
                        }
                        else
                        {
                            k = 6;
                          
                        }
                    }

                    
                 
                    h = Cxapicalls.GetHost;
                    try
                    {

                        int r= 0;
                        while (r < 4)
                        {
                            if (h.diskInfo == null || h.diskInfo.Length == 0)
                            {
                                Thread.Sleep(5000);
                                cxapi.PostRequestStatusForDummyDisk(h, false);
                                h = Cxapicalls.GetHost;
                                r++;
                            }
                            else
                            {
                                r = 4;
                                break;
                            }
                        }                      

                        h = Cxapicalls.GetHost;
                        if (h.diskInfo != null)
                        {
                            Trace.WriteLine(DateTime.Now + "\t Printing the diskInfo value " + h.diskInfo);
                            string[] diskInfo = h.diskInfo.Split('@');
                            int i = diskInfo.Length;
                            Trace.WriteLine(DateTime.Now + "\t Printing the diskInfo length " + i.ToString());
                            diskList = new ArrayList();
                            // Disklist 0 will hve path of linux_script.sh path hence ignoring it.
                            for (int j = 1; j < diskInfo.Length; j++)
                            {
                                Hashtable hash = new Hashtable();
                                Trace.WriteLine(DateTime.Now + "\t Printing the value of k[j] " + diskInfo[j]);
                                if (diskInfo[j].Contains(","))
                                {
                                    string[] size = diskInfo[j].Split(',');
                                    Trace.WriteLine(DateTime.Now + "\t Length after first split " + size.Length.ToString());

                                    if (size.Length >= 1)
                                    {
                                        hash.Add("Size", size[0]);
                                        hash.Add("Port", size[1]);
                                        hash.Add("ScsiBusNumber", size[1].Split(':')[0]);
                                    }
                                }
                                if (hash["Size"] != null && hash["Port"] != null)
                                {
                                    diskList.Add(hash);
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
                catch (Exception ex)
                {
                    Trace.WriteLine(DateTime.Now + "\t Failed in do work " + ex.Message);
                    System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                    Trace.WriteLine("ERROR*******************************************");
                    Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                    Trace.WriteLine("Exception  =  " + ex.Message);
                    Trace.WriteLine("ERROR___________________________________________");

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

        internal bool CreateDummydisk(Host masterHost)
        {
            try
            {
                int dummyReturncode = 0;
                if (_appName == AppName.Drdrill)
                {
                    dummyReturncode = _esx.ExecuteDummyDisksScriptForDrdrill(masterHost.esxIp);
                }
                else
                {
                    dummyReturncode = _esx.ExecuteDummyDisksScript(masterHost.esxIp);
                }
                if (dummyReturncode == 0)
                {
                    ArrayList diskList = new ArrayList();
                    // For linux we have removed the refresh API and parsing the output of InserScript policy
                    // We are calling script policy two times.
                    // FIrst time with wait of 25 seconds.
                    // Second time with wait of 15 seconds
                    if (masterHost.os == OStype.Linux)
                    {
                        try
                        {
                            masterHost.requestId = null;
                            Thread.Sleep(40000);
                            RunningScriptInLinuxmt(masterHost, ref diskList);

                            //Removed second time logic as we are not gettign any devices in that call
                            //Thread.Sleep(15000);
                            //diskList = new ArrayList();
                            //RunningScriptInLinuxMt(masterHost, ref diskList);
                            //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
                            //based on above comment we have added xmlresolver as null
                            XmlDocument document = new XmlDocument();
                            document.XmlResolver = null;
                            string xmlfile = null;
                            if (_appName == AppName.Drdrill)
                            {
                                xmlfile = latestFilePath + "Snapshot.xml";
                            }
                            else
                            {

                                xmlfile = latestFilePath + "ESX.xml";
                            }
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

                                    foreach (XmlNode node in mtdiskNodes)
                                    {
                                        foreach (Hashtable disk in diskList)
                                        {
                                            Trace.WriteLine(DateTime.Now + " \t comparing both the sizes " + node.Attributes["size"].Value.ToString() + " " + disk["Size"].ToString());
                                            if (node.Attributes["size"].Value.ToString() == disk["Size"].ToString())
                                            {

                                                if (disk["ScsiBusNumber"] != null)
                                                {
                                                    Trace.WriteLine(DateTime.Now + "Printing ScsiBusNumber values " + disk["ScsiBusNumber"].ToString());
                                                    node.Attributes["port_number"].Value = disk["ScsiBusNumber"].ToString();
                                                }


                                            }
                                        }
                                    }
                                    dummyDiskCreation = true;
                                    document.Save(xmlfile);
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
                        try
                        {
                            //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
                            //based on above comment we have added xmlresolver as null
                            XmlDocument document = new XmlDocument();
                            document.XmlResolver = null;
                            string xmlfile = null;
                            if (_appName == AppName.Drdrill)
                            {
                                xmlfile = latestFilePath + "Snapshot.xml";
                            }
                            else
                            {

                                xmlfile = latestFilePath + "ESX.xml";
                            }
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
                                        int i = 0;
                                        while (i < 10)
                                        {
                                            if (cxApi.Post(h, "GetRequestStatus") == true)
                                            {
                                                i = 11;
                                                h = Cxapicalls.GetHost;
                                            }
                                            else
                                            {
                                                Trace.WriteLine(DateTime.Now + "\t Going to sleep for " +  i.ToString());
                                                Thread.Sleep(65000);
                                                i++;
                                                if (i == 10)
                                                {
                                                    cxApi.Post(h, "GetHostInfo");
                                                    h = Cxapicalls.GetHost;
                                                }
                                            }
                                        }
                                        
                                        ArrayList physicalDisks;
                                        physicalDisks = h.disks.GetDiskList;
                                        foreach (XmlNode node in mtdiskNodes)
                                        {
                                            foreach (Hashtable disk in physicalDisks)
                                            {
                                                Trace.WriteLine(DateTime.Now + " \t comparing both the sizes " + node.Attributes["size"].Value.ToString() + " " + disk["ActualSize"].ToString());
                                                if (node.Attributes["size"].Value.ToString() == disk["ActualSize"].ToString())
                                                {
                                                    if (masterHost.os == OStype.Linux)
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

                                        dummyDiskCreation = true;
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
                }
                else
                {
                    statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                    dummyDiskCreation = false;
                    return false;
                }
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed in do work " + ex.Message);
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");

            }

            return true;
        }
        public bool CreateVMs(Host masterHost)
        {
            try
            {// allServersForm.protectionText.Update();
                if (_appName == AppName.Failback)
                {
                    Trace.WriteLine(DateTime.Now + " \t Entered to create guest vms for failback on target vSphere sever");
                    //allServersForm.protectionText.AppendText("Creating guest vms on secondary ESX server" + Environment.NewLine +
                    // "This may take several minutes..." + Environment.NewLine);
                    if (_esx.ExecuteCreateTargetGuestScript(masterHost.esxIp, OperationType.Failback) == 0)
                    {
                        Trace.WriteLine(DateTime.Now + " \t Successfully  created guest vms on target vSphere sever");
                        createGuestOnTargetSucceed = true;
                        Thread.Sleep(5000);

                    }
                    else
                    {
                        Trace.WriteLine(DateTime.Now + " \t Failed to  create guest vms on target vSphere sever");
                        return false;
                    }

                }
                else if (_appName == AppName.Drdrill)
                {
                    Trace.WriteLine(DateTime.Now + "\t Entered to create vms for dr drill ");
                    if (AllServersForm.arrayBasedDrdrillSelected == true)
                    {
                        if (_esx.ExecuteCreateTargetGuestScriptForArrayDrill(masterHost.esxIp) == 0)
                        {
                            Trace.WriteLine(DateTime.Now + " \t Successfully  created guest vms on target vSphere sever");
                            createGuestOnTargetSucceed = true;
                            Thread.Sleep(5000);

                        }
                        else
                        {
                            Trace.WriteLine(DateTime.Now + " \t Failed to  create guest vms on target vSphere sever");
                            return false;
                        }
                    }
                    else
                    {
                        if (_esx.ExecuteCreateTargetGuestScript(masterHost.esxIp, OperationType.Drdrill) == 0)
                        {
                            Trace.WriteLine(DateTime.Now + " \t Successfully  created guest vms on target vSphere sever");
                            createGuestOnTargetSucceed = true;
                            Thread.Sleep(5000);

                        }
                        else
                        {
                            Trace.WriteLine(DateTime.Now + " \t Failed to  create guest vms on target vSphere sever");
                            return false;
                        }
                    }
                }
                else if (_appName != AppName.Adddisks && _appName != AppName.Offlinesync && _appName != AppName.Failback)
                {
                    Trace.WriteLine(DateTime.Now + " \t Entered to create guest vms on target vSphere sever");

                    if (_esx.ExecuteCreateTargetGuestScript(masterHost.esxIp,OperationType.Initialprotection) == 0)
                    {
                        Trace.WriteLine(DateTime.Now + " \t Successfully  created guest vms on target vSphere sever");
                        createGuestOnTargetSucceed = true;
                        Thread.Sleep(5000);

                    }
                    else
                    {
                        Trace.WriteLine(DateTime.Now + " \t Failed to  create guest vms on target vSphere sever");
                        return false;
                    }
                }

                else if (_appName == AppName.Adddisks)
                {
                    //this is for incremental disk.
                    Trace.WriteLine(DateTime.Now + " \t Entered to create guest vms for addition of disk on target vSphere sever");
                    if (_esx.ExecuteCreateTargetGuestScript(masterHost.esxIp, OperationType.Additionofdisk) == 0)
                    {
                        Trace.WriteLine(DateTime.Now + " \t Successfully  created guest vms on target vSphere sever");
                        createGuestOnTargetSucceed = true;
                        Thread.Sleep(5000);
                    }
                    else
                    {
                        Trace.WriteLine(DateTime.Now + " \t Failed to  create guest vms on target vSphere sever");
                        return false;
                    }
                }
                else if (_appName == AppName.Offlinesync)
                {
                    Trace.WriteLine(DateTime.Now + " \t Entered to create guest vms for Offline sync on target vSphere sever");
                    if (_esx.ExecuteCreateTargetGuestScript(masterHost.esxIp,  OperationType.Offlinesync) == 0)
                    {
                        Trace.WriteLine(DateTime.Now + " \t Successfully  created guest vms on target vSphere sever");
                        createGuestOnTargetSucceed = true;
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
        public bool SetFxJobs(Host masterHost, string vconmt)
        {

            // once the delete of the folders are done we will call the script to set fx jobs......
            try
            {
                if (_appName == AppName.Failback)
                {
                    Trace.WriteLine(DateTime.Now + " \t Entered to create fx jobs for the failback protection");
                    if (_esx.ExecuteJobAutomationScript(OperationType.Failback, vconmt) == 0)
                    {
                        ResumeForm.breifLog.WriteLine(DateTime.Now + " \t Successfully completed by setting jobs to the above selected vms with plan name " + masterHost.plan);
                        ResumeForm.breifLog.Flush();
                        Trace.WriteLine(DateTime.Now + " \t Successfully created fx jobs for the protection");
                        jobAutomationSucceed = true;
                    }
                    else
                    {
                        Trace.WriteLine(DateTime.Now + " \t Failed to create fx jobs for the protection");
                        return false;
                    }
                }
                else if (_appName != AppName.Adddisks && _appName != AppName.Failback)
                {
                    //Writing into the vContinuum_breif.log.......... 
                    ResumeForm.breifLog.WriteLine(DateTime.Now + "\t Fx jobs are set for the following vms ");
                    ResumeForm.breifLog.Flush();
                    foreach (Host h in selectedSourceList._hostList)
                    {
                        ResumeForm.breifLog.WriteLine(DateTime.Now + " \t " + h.displayname + " \t mt " + h.masterTargetDisplayName + " \t target esx ip " + h.targetESXIp + "\t source esx ip " + h.esxIp + " \t plan name  " + h.plan);
                    }
                    ResumeForm.breifLog.Flush();
                    Trace.WriteLine(DateTime.Now + " \t Entered to create fx jobs for the protection");

                    if (_esx.ExecuteJobAutomationScript(OperationType.Initialprotection, vconmt) == 0)
                    {
                        Trace.WriteLine(DateTime.Now + " \t Successfully created fx jobs for the protection");
                        jobAutomationSucceed = true;
                    }
                    else
                    {
                        Trace.WriteLine(DateTime.Now + " \t Failed to create fx jobs for the protection");
                        return false;
                    }
                }
                else if (_appName == AppName.Adddisks)
                {
                    jobAutomationSucceed = true;
                    //Trace.WriteLine(DateTime.Now + " \t Entered to create fx jobs for the addition of disk protection");
                    //if (_esx.ExecuteJobAutomationScript(OperationType.Additionofdisk, vconmt) == 0)
                    //{
                    //    Trace.WriteLine(DateTime.Now + " \t Successfully created fx jobs for the protection");
                    //    _jobAutomationSucceed = true;
                    //}
                    //else
                    //{
                    //    Trace.WriteLine(DateTime.Now + " \t Failed to create fx jobs for the protection");
                    //    return false;
                    //}
                    ////Writing into the vContinuum_breif.log.......... 
                    //ResumeForm.breifLog.WriteLine(DateTime.Now + " \t Successfully completed by setting jobs to the above selected vms");
                    //ResumeForm.breifLog.Flush();
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
        public bool CopyFiletoDirectory()
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
                string xmlfile = latestFilePath + "ESX.xml";
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

                //reading esx.xml masternode and writing hostname of mastertaregt to sourcenode.
                if (_appName == AppName.Esx || _appName == AppName.Bmr || _appName == AppName.Failback || _appName == AppName.Adddisks || _appName == AppName.Offlinesync)
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
                        node.Attributes["directoryName"].Value = directoryNameinXmlFile;
                    }

                   
                }
                document.Save(xmlfile);
                foreach (XmlNode node in diskNodes)
                {
                    node.Attributes["protected"].Value = "yes";
                }

                // In case of failback we will make the failback attribute values as yes.....
                if (_appName == AppName.Failback || _appName == AppName.V2P)
                {
                    foreach (XmlNode node in hostNodes)
                    {
                        foreach (Host h in selectedSourceList._hostList)
                        {
                            if (h.displayname == node.Attributes["display_name"].Value)
                            {
                                node.Attributes["failback"].Value = "yes";
                            }
                            if (h.mbr_path != null && h.os == OStype.Windows && _appName == AppName.V2P)
                            {
                                if (node.Attributes["mbr_path"] != null)
                                {
                                    node.Attributes["mbr_path"].Value = h.mbr_path;
                                }
                            }
                        }
                    }
                }
                document.Save(xmlfile);
                //Creating the directory for the parallel protection with plan and mt hostname + random number

                if (File.Exists(latestFilePath + "ESX.xml"))
                {
                    File.Copy(latestFilePath + "ESX.xml", fxFailOverDataPath + "\\" + directoryNameinXmlFile + "\\ESX.xml", true);
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
        public bool UploadFileAndPrePareInputText(OStype ostype)
        {
            ArrayList fileList = new ArrayList();
            try
            {
                if (File.Exists(latestFilePath + "input.txt"))
                {
                    File.Delete(latestFilePath + "input.txt");
                }
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed to delete input.txt file form latest folder" + ex.Message);
            }
            try
            {
                FileInfo file = new FileInfo(latestFilePath + "input.txt");
                StreamWriter writer = file.AppendText();
                if (_appName == AppName.Recover)
                {
                    writer.WriteLine("Recovery.xml");
                    fileList.Add(fxFailOverDataPath + "\\" + directoryNameinXmlFile + "\\Recovery.xml");
                    fileList.Add(fxFailOverDataPath + "\\" + directoryNameinXmlFile + "\\input.txt");
                }
                else if (_appName == AppName.Drdrill)
                {
                    writer.WriteLine("Snapshot.xml");
                    writer.WriteLine("Inmage_scsi_unit_disks.txt");
                    fileList.Add(fxFailOverDataPath + "\\" + directoryNameinXmlFile + "\\Snapshot.xml");fileList.Add(fxFailOverDataPath + "\\" + directoryNameinXmlFile + "\\Inmage_scsi_unit_disks.txt");
                    fileList.Add(fxFailOverDataPath + "\\" + directoryNameinXmlFile + "\\Inmage_scsi_unit_disks.txt");
                    fileList.Add(fxFailOverDataPath + "\\" + directoryNameinXmlFile + "\\input.txt");
                }
                else if (_appName == AppName.Resize)
                {
                    writer.WriteLine("Resize.xml");
                    writer.WriteLine("Inmage_scsi_unit_disks.txt");
                    fileList.Add(fxFailOverDataPath + "\\" + directoryNameinXmlFile + "\\Resize.xml"); fileList.Add(fxFailOverDataPath + "\\" + directoryNameinXmlFile + "\\Inmage_scsi_unit_disks.txt");
                    fileList.Add(fxFailOverDataPath + "\\" + directoryNameinXmlFile + "\\Inmage_scsi_unit_disks.txt");
                    fileList.Add(fxFailOverDataPath + "\\" + directoryNameinXmlFile + "\\input.txt");
                }
                else if (_appName == AppName.Removedisk)
                {
                    writer.WriteLine("Remove.xml");
                    writer.WriteLine("Inmage_scsi_unit_disks.txt");
                    fileList.Add(fxFailOverDataPath + "\\" + directoryNameinXmlFile + "\\Remove.xml"); fileList.Add(fxFailOverDataPath + "\\" + directoryNameinXmlFile + "\\Inmage_scsi_unit_disks.txt");
                    fileList.Add(fxFailOverDataPath + "\\" + directoryNameinXmlFile + "\\Inmage_scsi_unit_disks.txt");
                    fileList.Add(fxFailOverDataPath + "\\" + directoryNameinXmlFile + "\\input.txt");
                }
                else if (_appName == AppName.Removevolume)
                {
                    writer.WriteLine("Inmage_volumepairs.txt");
                    fileList.Add(fxFailOverDataPath + "\\" + directoryNameinXmlFile + "\\Inmage_volumepairs.txt");
                    fileList.Add(fxFailOverDataPath + "\\" + directoryNameinXmlFile + "\\input.txt");
                }
                else
                {
                    writer.WriteLine("ESX.xml");
                    writer.WriteLine("Inmage_scsi_unit_disks.txt");
                    fileList.Add(fxFailOverDataPath + "\\" + directoryNameinXmlFile + "\\ESX.xml");
                    fileList.Add(fxFailOverDataPath + "\\" + directoryNameinXmlFile + "\\Inmage_scsi_unit_disks.txt");
                    fileList.Add(fxFailOverDataPath + "\\" + directoryNameinXmlFile + "\\input.txt");
                    if (_appName == AppName.Bmr || (_appName == AppName.Offlinesync && ResumeForm.selectedActionForPlan == "P2v"))
                    {
                        if (ostype == OStype.Windows)
                        {
                            if (Directory.Exists(fxFailOverDataPath + "\\" + directoryNameinXmlFile + "\\Windows_2003_32"))
                            {
                                if (File.Exists(fxFailOverDataPath + "\\" + directoryNameinXmlFile + "\\Windows_2003_32\\symmpi.sys"))
                                {
                                    writer.WriteLine("Windows_2003_32/symmpi.sys");
                                    fileList.Add(fxFailOverDataPath + "\\" + directoryNameinXmlFile + "\\Windows_2003_32\\symmpi.sys");
                                }
                            }
                            if (Directory.Exists(fxFailOverDataPath + "\\" + directoryNameinXmlFile + "\\Windows_2003_64"))
                            {
                                if (File.Exists(fxFailOverDataPath + "\\" + directoryNameinXmlFile + "\\Windows_2003_64\\symmpi.sys"))
                                {
                                    writer.WriteLine("Windows_2003_64/symmpi.sys");
                                    fileList.Add(fxFailOverDataPath + "\\" + directoryNameinXmlFile + "\\Windows_2003_64\\symmpi.sys");
                                }
                            }
                            if (Directory.Exists(fxFailOverDataPath + "\\" + directoryNameinXmlFile + "\\Windows_XP_64"))
                            {
                                if (File.Exists(fxFailOverDataPath + "\\" + directoryNameinXmlFile + "\\Windows_XP_64\\symmpi.sys"))
                                {
                                    writer.WriteLine("Windows_XP_64/symmpi.sys");
                                    fileList.Add(fxFailOverDataPath + "\\" + directoryNameinXmlFile + "\\Windows_XP_64\\symmpi.sys");
                                }
                            }
                            if (Directory.Exists(fxFailOverDataPath + "\\" + directoryNameinXmlFile + "\\Windows_XP_32"))
                            {
                                if (File.Exists(fxFailOverDataPath + "\\" + directoryNameinXmlFile + "\\Windows_XP_32\\symmpi.sys"))
                                {
                                    writer.WriteLine("Windows_XP_32/symmpi.sys");
                                    fileList.Add(fxFailOverDataPath + "\\" + directoryNameinXmlFile + "\\Windows_XP_32\\symmpi.sys");
                                }
                            }
                        }
                    }
                }
                writer.Close();
                writer.Dispose();
                if (File.Exists(latestFilePath + "input.txt"))
                {
                    File.Copy(latestFilePath + "input.txt", fxFailOverDataPath + "\\" + directoryNameinXmlFile + "\\input.txt", true);

                }
                AllServersForm.SetFolderPermissions(fxFailOverDataPath + "\\" + directoryNameinXmlFile);
                WinTools win = new WinTools();
                win.SetFilePermissions(latestFilePath + "\\input.txt");
                foreach (string filename in fileList)
                {
                    Trace.WriteLine(DateTime.Now + "\t File permissions for file " + filename);
                    win.SetFilePermissions(filename);
                }
                foreach (string filename in fileList)
                {
                    if(filename.Contains("Windows_2003_32"))
                    {
                        if (WinTools.UploadFileToCX("\"" + filename + "\"", "\"" + "/home/svsystems/vcon/" + directoryNameinXmlFile + "/Windows_2003_32" + "\"") == 0)
                        {
                            Trace.WriteLine(DateTime.Now + "\t Successfully upload the file to Cx using cxpsclient.exe " + filename);
                        }
                        else
                        {
                            Trace.WriteLine(DateTime.Now + "\t Failed to  upload the file to Cx using cxpsclient.exe " + filename);
                            return false;
                        }
                    }
                    else if (filename.Contains("Windows_2003_64"))
                    {
                        if (WinTools.UploadFileToCX("\"" + filename + "\"", "\"" + "/home/svsystems/vcon/" + directoryNameinXmlFile + "/Windows_2003_64" + "\"") == 0)
                        {
                            Trace.WriteLine(DateTime.Now + "\t Successfully upload the file to Cx using cxpsclient.exe " + filename);
                        }
                        else
                        {
                            Trace.WriteLine(DateTime.Now + "\t Failed to  upload the file to Cx using cxpsclient.exe " + filename);
                            return false;
                        }
                    }
                    else if (filename.Contains("Windows_XP_64"))
                    {
                        if (WinTools.UploadFileToCX("\"" + filename + "\"", "\"" + "/home/svsystems/vcon/" + directoryNameinXmlFile + "/Windows_XP_64" + "\"") == 0)
                        {
                            Trace.WriteLine(DateTime.Now + "\t Successfully upload the file to Cx using cxpsclient.exe " + filename);
                        }
                        else
                        {
                            Trace.WriteLine(DateTime.Now + "\t Failed to  upload the file to Cx using cxpsclient.exe " + filename);
                            return false;
                        }
                    }
                    else if (filename.Contains("Windows_XP_32"))
                    {
                        if (WinTools.UploadFileToCX("\"" + filename + "\"", "\"" + "/home/svsystems/vcon/" + directoryNameinXmlFile + "/Windows_XP_32" + "\"") == 0)
                        {
                            Trace.WriteLine(DateTime.Now + "\t Successfully upload the file to Cx using cxpsclient.exe " + filename);
                        }
                        else
                        {
                            Trace.WriteLine(DateTime.Now + "\t Failed to  upload the file to Cx using cxpsclient.exe " + filename);
                            return false;
                        }
                    }
                    else
                    {
                        if (WinTools.UploadFileToCX("\"" + filename + "\"", "\"" + "/home/svsystems/vcon/" + directoryNameinXmlFile + "\"") == 0)
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

        private void backgroundWorker_DoWork(object sender, DoWorkEventArgs e)
        {
            try
            {
                
                tickerDelegate = new TickerDelegate(SetLeftTicker);
                if (_appName == AppName.Resume)
                {
                    if (firsttime == false)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Came here for not first time in case of resume");
                        _masterHost.initializingProtectionPlan = null;
                        statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                    }
                    PreScriptRunForResume();
                    firsttime = false;
                }
                else if (_appName == AppName.Recover || _appName == AppName.Drdrill)
                {
                    if (firsttime == false)
                    {
                        _masterHost.initializingRecoveryPlan = null;
                        _masterHost.initializingDRDrillPlan = null;
                        statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                    }
                    RecoverSelectedVms(ref _masterHost);
                    firsttime = false;
                }
                else if (_appName == AppName.Resize)
                {
                    ResizeOFDiskOrVolume(selectedSourceList, _masterHost);
                }
                else if (_appName == AppName.Removedisk)
                {
                    RemoveDiskFromProtection(selectedSourceList, _masterHost);
                }
                else if (_appName == AppName.Removevolume)
                {
                    RemoveVolumeFromProtection(selectedSourceList, _masterHost);
                }
                else
                {
                    if (firsttime == false)
                    {
                        _masterHost.initializingProtectionPlan = null;
                        statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                    }
                    PreScriptRun(_masterHost);
                    firsttime = false;
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

        private void backgroundWorker_RunWorkerCompleted(object sender, RunWorkerCompletedEventArgs e)
        {
            if (_appName == AppName.Recover)
            {
                if (_masterHost.recoveryLater == true)
                {
                    if (recoveredVms == true)
                    {
                        
                        protectButton.Visible = false;
                        protectButton.Enabled = true;
                        cancelButton.Enabled = true;
                        cancelButton.Text = "Done";
                        recoveryLaterLabel.Visible = true;
                        currentRow = -1;
                        statusDataGridView.Visible = false;
                        statusDataGridView.Rows.Clear();

                        if (AllServersForm.v2pRecoverySelected == true)
                        {
                            warningLabel.Text = "Note: Once recovery is done.Power off physical server, Detach USB disk  and Boot physical server.";
                        }
                        
                    }
                }


                if (uploadfiletocx == false || recoveredVms == false)
                {
                    if (recoveredVms == false)
                    {
                        if (File.Exists(WinTools.FxAgentPath() + "\\vContinuum\\logs\\vCon_Error.log"))
                        {
                            logLinkLabel.Visible = true;
                        }
                    }

                    protectButton.Enabled = true;
                    protectButton.Visible = true;
                    cancelButton.Enabled = true;
                }
                else
                {
                    if (AllServersForm.v2pRecoverySelected == true)
                    {
                        warningLabel.Text = "Note: Once recovery is done.Power off physical server, Detach USB disk  and Boot physical server.";
                    }
                    protectButton.Visible = false;
                    protectButton.Enabled = true;
                    cancelButton.Text = "Done";
                    cancelButton.Enabled = true;
                }
            }
            else if (_appName == AppName.Resize)
            {

                if (resizeSourceChecks == false || resizeTargetExtend == false)
                {
                    protectButton.Enabled = true;
                    protectButton.Visible = true;
                    protectButton.Text = "Re-try";
                    cancelButton.Text = "Cancel";
                    cancelButton.Enabled = true;


                    statusDataGridView.Rows[0].Cells[0].Value = failed;
                    if (File.Exists(WinTools.FxAgentPath() + "\\vContinuum\\logs\\vContinuum_ESX.log"))
                    {
                        logLinkLabel.Visible = true;

                    }
                    return;
                }

                if (esxutilwinExecution == false)
                {
                    protectButton.Enabled = true;
                    protectButton.Visible = true;
                    protectButton.Text = "Re-try";
                    cancelButton.Text = "Cancel";
                    cancelButton.Enabled = true;

                    if (File.Exists(WinTools.FxAgentPath() + "\\vContinuum\\logs\\vCon_Error.log"))
                    {
                        logLinkLabel.Visible = true;
                        return;
                    }
                }
                protectButton.Visible = false;
                protectButton.Enabled = true;
                cancelButton.Text = "Done";
                cancelButton.Enabled = true;

            }
            else if (_appName == AppName.Removedisk)
            {
                if (creatingScsiUnitsFile == false)
                {
                    protectButton.Enabled = true;
                    protectButton.Visible = true;
                    protectButton.Text = "Re-try";
                    cancelButton.Text = "Cancel";
                    cancelButton.Enabled = true;


                    statusDataGridView.Rows[0].Cells[0].Value = failed;
                    if (File.Exists(WinTools.FxAgentPath() + "\\vContinuum\\logs\\vContinuum_ESX.log"))
                    {
                        logLinkLabel.Visible = true;

                    }
                    return;
                }

                if (esxutilwinExecution == false)
                {
                    protectButton.Enabled = true;
                    protectButton.Visible = true;
                    protectButton.Text = "Re-try";
                    cancelButton.Text = "Cancel";
                    cancelButton.Enabled = true;

                    if (File.Exists(WinTools.FxAgentPath() + "\\vContinuum\\logs\\vCon_Error.log"))
                    {
                        logLinkLabel.Visible = true;
                        return;
                    }
                }
               // remove
                try
                {
                    if (File.Exists(WinTools.FxAgentPath() + "\\vContinuum\\logs\\vCon_Error.log"))
                    {
                        File.Delete(WinTools.FxAgentPath() + "\\vContinuum\\logs\\vCon_Error.log");
                    }
                }
                catch (Exception ex)
                {
                    Trace.WriteLine(DateTime.Now + "\t Failed to delete existing log file " + ex.Message);
                }
                try
                {



                    FileInfo f1 = new FileInfo(WinTools.FxAgentPath() + "\\vContinuum\\logs\\vCon_Error.log");
                    StreamWriter sw = f1.CreateText();
                    foreach (Host h in selectedSourceList._hostList)
                    {
                        foreach (Hashtable hash in h.disks.list)
                        {
                            if (hash["Name"] != null && hash["remove"] != null && hash["remove"].ToString().ToUpper() == "TRUE")
                            {
                                string prepareTargetDiskname = null;
                                string[] disksplit = hash["Name"].ToString().Split(']');
                                if (disksplit.Length >= 1)
                                {
                                    if (h.vmDirectoryPath == null)
                                    {
                                        prepareTargetDiskname = "[" + h.targetDataStore + "] " + disksplit[1].ToString();
                                    }
                                    else
                                    {
                                        prepareTargetDiskname = "[" + h.targetDataStore + "] " + h.vmDirectoryPath +"\\" +disksplit[1].ToString();
                                    }
                                    sw.WriteLine(prepareTargetDiskname);
                                }
                            }
                        }
                    }                
                    sw.Close();
                    logLinkLabel.Visible = true;
                }
                catch (Exception ex)
                {
                    Trace.WriteLine(DateTime.Now + "\t Failed to update the list in file " + ex.Message);
                }

                warningLabel.Text = "Remove disk option will not delete disks from datastore. They have to be deleted manually.";
                logLinkLabel.Text = "Click here to view disk names to delete";
                protectButton.Visible = false;
                warningLabel.Visible = true;
                protectButton.Enabled = true;
                cancelButton.Text = "Done";
                cancelButton.Enabled = true;
            }
            else if (_appName == AppName.Removevolume)
            {
                if (uploadfiletocx == false)
                {
                    protectButton.Enabled = true;
                    protectButton.Visible = true;
                    protectButton.Text = "Re-try";
                    cancelButton.Text = "Cancel";
                    cancelButton.Enabled = true;


                    statusDataGridView.Rows[0].Cells[0].Value = failed;
                    return;
                }
                if (esxutilwinExecution == false)
                {
                    protectButton.Enabled = true;
                    protectButton.Visible = true;
                    protectButton.Text = "Re-try";
                    cancelButton.Text = "Cancel";
                    cancelButton.Enabled = true;

                    if (File.Exists(WinTools.FxAgentPath() + "\\vContinuum\\logs\\vCon_Error.log"))
                    {
                        logLinkLabel.Visible = true;
                        return;
                    }
                }
                protectButton.Visible = false;
                protectButton.Enabled = true;
                cancelButton.Text = "Done";
                cancelButton.Enabled = true;
            }
            else
            {
                if (_appName == AppName.Resume)
                {
                    if (powerOn == true)
                    {
                        string message = "Some of the VM(s) selected to resume is powered on on target." + Environment.NewLine + "Shall we shutdown the VM(s)";

                        MessageBoxForAll messages = new MessageBoxForAll(message, "question", "Power Off");
                        messages.Focus();
                        messages.BringToFront();
                        messages.Activate();
                        messages.ShowDialog();
                        if (messages.selectedyes == true)
                        {

                            powerOn = true;
                            firsttime = true;
                            backgroundWorker.RunWorkerAsync();
                            return;
                        }
                        else
                        {
                            MessageBox.Show("With out power off target vm we can't proceed Exiting....","Error",MessageBoxButtons.OK,MessageBoxIcon.Error);
                            cancelButton.Enabled = true;
                            return;
                        }


                    }

                    if (checkingForVmsPowerStatus == false || dummyDiskCreation == false || createGuestOnTargetSucceed == false || esxutilwinExecution == false)
                    {
                        protectButton.Enabled = true;
                        protectButton.Visible = true;
                        cancelButton.Text = "Cancel";
                        cancelButton.Enabled = true;

                        if (checkingForVmsPowerStatus == false || dummyDiskCreation == false || createGuestOnTargetSucceed == false)
                        {
                            if (File.Exists(WinTools.FxAgentPath() + "\\vContinuum\\logs\\vContinuum_ESX.log"))
                            {
                                logLinkLabel.Visible = true;

                            }
                        }
                        else if (esxutilwinExecution == false)
                        {
                            if (File.Exists(WinTools.FxAgentPath() + "\\vContinuum\\logs\\vCon_Error.log"))
                            {
                                logLinkLabel.Visible = true;

                            }
                        }
                    }
                }


                if ( dummyDiskCreation == false || createGuestOnTargetSucceed == false || esxutilwinExecution == false)
                {
                    protectButton.Enabled = true;
                    protectButton.Visible = true;
                    cancelButton.Text = "Cancel";
                    cancelButton.Enabled = true;

                    if (dummyDiskCreation == false || createGuestOnTargetSucceed == false)
                    {
                        if (File.Exists(WinTools.FxAgentPath() + "\\vContinuum\\logs\\vContinuum_ESX.log"))
                        {
                            logLinkLabel.Visible = true;

                        }
                    }
                    else if (esxutilwinExecution == false)
                    {
                        if (File.Exists(WinTools.FxAgentPath() + "\\vContinuum\\logs\\vCon_Error.log"))
                        {
                            logLinkLabel.Visible = true;

                        }
                    }
                    if (_appName != AppName.Drdrill)
                    {
                        if (jobAutomationSucceed == false)
                        {
                            if (File.Exists(WinTools.FxAgentPath() + "\\vContinuum\\logs\\vContinuum_ESX.log"))
                            {
                                logLinkLabel.Visible = true;
                            }
                        }
                    }

                }
                else
                {
                    logLinkLabel.Visible = false;
                    protectButton.Visible = false;
                    protectButton.Enabled = true;
                    cancelButton.Text = "Done";
                    cancelButton.Enabled = true;

                }
            }

        }

        private void backgroundWorker_ProgressChanged(object sender, ProgressChangedEventArgs e)
        {

        }

        private void protectButton_Click(object sender, EventArgs e)
        {
            try
            {
                //PreScriptRun(_masterHost);
                logPath = null;
                logLinkLabel.Visible = false;
                protectButton.Enabled = false;
                protectButton.Visible = false;
                calledByProtectButton = true;
                backgroundWorker.RunWorkerAsync();
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed in do work " + ex.Message);
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");

            }
        }

        private void statusDataGridView_CellPainting(object sender, DataGridViewCellPaintingEventArgs e)
        {
            if (statusDataGridView.RowCount != 0)
            {
                ImageAnimator.UpdateFrames();
                e.Graphics.DrawImage(currentTask, e.CellBounds.Location);
            }

        }
        public void AnimateImage()
        {


            try
            {

                Image image = currentTask;

                if (image != null)
                {
                    ImageAnimator.Animate(image, new EventHandler(this.OnFrameChanged));
                }


            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed in do work " + ex.Message);
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");

            }

        }

        private void OnFrameChanged(object o, EventArgs e)
        {
            try
            {
                //Force a call to the Paint event handler.
                if (currentRow != -1)
                {
                    datagridViewStatus.InvalidateCell(datagridViewStatus.Rows[currentRow].Cells[0]);
                }
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed in do work " + ex.Message);
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");

            }
            //_datagrid.Invalidate();
        }

        public bool PreScriptRunForResume()
        {
            try
            {
                //Resume button click will call this method....

                tickerDelegate = new TickerDelegate(SetLeftTicker);
                Host h = (Host)selectedSourceList._hostList[0];
                Host tempMasterHost = new Host();
                tempMasterHost.hostname = h.masterTargetHostName;
                tempMasterHost.displayname = h.masterTargetDisplayName;
                Esx _esxInfo = new Esx();


                if (checkingForVmsPowerStatus == false)
                {


                    //WinPreReqs win = new WinPreReqs("", "", "", "");
                    //Trace.WriteLine(DateTime.Now + "\t Printing mt host name and ip " + tempMasterHost.hostname + tempMasterHost.ip);
                    //if (win.MasterTargetPreCheck(ref tempMasterHost, WinTools.GetcxIPWithPortNumber()) == true)
                    //{
                    //    if (tempMasterHost.machineInUse == true)
                    //    {
                    //        Trace.WriteLine(DateTime.Now + " \t Came here to know that mt is in use");
                    //        string message = "This VM can't be used as master target. Some Fx jobs are running on this master target." + Environment.NewLine
                    //            + "Select another master target";
                    //        MessageBox.Show(message, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    //        statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                    //        return false;
                    //    }
                    //}

                    Host tempHost = (Host)selectedSourceList._hostList[0];
                    try
                    {
                        AllServersForm allServersForm = new AllServersForm("Resume", tempHost.plan);
                        Cxapicalls cxAPis = new Cxapicalls();
                        if (allServersForm.VerfiyEsxCreds(tempHost.targetESXIp, "target") == false)
                        {
                            Trace.WriteLine(DateTime.Now + "\t Failed to update esx meta data in CX");
                            return false;
                        }
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
                                foreach (Host h1 in selectedSourceList._hostList)
                                {
                                    h1.targetESXIp = source.sourceEsxIpText.Text;
                                    h1.targetESXUserName = source.userNameText.Text;
                                    h1.targetESXPassword = source.passWordText.Text;
                                }
                                cxAPis. UpdaterCredentialsToCX(h.targetESXIp, h.targetESXUserName, h.targetESXPassword);
                                returnValue = _esxInfo.GetEsxInfoWithVmdks(h.targetESXIp, Role.Secondary, h.os);
                                if (returnValue != 0)
                                {
                                    statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                                    Trace.WriteLine(DateTime.Now + " \t Failed to get the MasterXMl.xml");
                                    return false;
                                }
                                else
                                {
                                    string listOfVmsToPowerOff = null;
                                    foreach (Host selectedHost in selectedSourceList._hostList)
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

                                        powerOffList = listOfVmsToPowerOff;
                                        powerOn = true;
                                        checkingForVmsPowerStatus = true;
                                        return false;
                                    }                                    
                                       
                                }
                            }
                            else
                            {
                                statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                                powerOn = false;
                                return false;
                            }

                        }
                        else if (returnValue == 0)
                        {
                            string listOfVmsToPowerOff = null;
                            foreach (Host selectedHost in selectedSourceList._hostList)
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

                                powerOffList = listOfVmsToPowerOff;
                                powerOn = true;
                                checkingForVmsPowerStatus = true;
                                return false;
                            }
                            
                                                
                        }
                        else
                        {

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

                if (powerOn == true)
                {
                    powerOn = false;
                    if (_esxInfo.PowerOffVM(h.targetESXIp,  powerOffList) != 0)
                    {
                        checkingForVmsPowerStatus = false;
                        return false;
                    }
                }  
                if (readinessCheck == false)
                {
                    try
                    {
                        Trace.WriteLine(DateTime.Now + "\t Came here to run the readiness checks ");
                        foreach (Host selectedHost in selectedSourceList._hostList)
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



                    foreach (Host targetHost in _esxInfo.GetHostList)
                    {
                        if (_masterHost.source_uuid != null)
                        {
                            if (_masterHost.source_uuid == targetHost.source_uuid)
                            {
                                _masterHost.ide_count = targetHost.ide_count;
                                break;
                            }
                        }
                        if (_masterHost.displayname == targetHost.displayname || _masterHost.hostname == targetHost.hostname)
                        {
                            _masterHost.ide_count = targetHost.ide_count;
                            break;
                        }
                    }

                    try
                    {
                        RefreshHosts(selectedSourceList);
                    }
                    catch (Exception ex)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Failed to refresh hosts  " + ex.Message);
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
                        //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
                       // StreamReader reader = new StreamReader(xmlfile1);
                        //string xmlString = reader.ReadToEnd();
                        XmlReaderSettings settings = new XmlReaderSettings();
                        settings.ProhibitDtd = true;
                        settings.XmlResolver = null;
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
                        rootNodes1 = document1.GetElementsByTagName("root");
                        diskNodes = document1.GetElementsByTagName("disk");
                        foreach (Host h1 in selectedSourceList._hostList)
                        {
                            Trace.WriteLine(DateTime.Now + "\t Machine type " + h1.machineType);
                            string machineType = h1.machineType;
                            ArrayList list = new ArrayList();
                            list = h1.disks.list;
                            Host tempHost1 = new Host();
                            if (h1.inmage_hostid == null)
                            {

                                string cxip = WinTools.GetcxIPWithPortNumber();
                                WinPreReqs win = new WinPreReqs("", "", "", "");
                                if (win.GetDetailsFromcxcli( tempHost1, cxip) == true)
                                {
                                    tempHost1 = WinPreReqs.GetHost;
                                    h1.inmage_hostid = tempHost1.inmage_hostid;
                                }
                               
                               
                            }
                            tempHost1.inmage_hostid = h1.inmage_hostid;

                            Cxapicalls cxApis = new Cxapicalls();
                            cxApis.Post( tempHost1, "GetHostInfo");
                            tempHost1 = Cxapicalls.GetHost;
                            h1.machineType = machineType;
                            if (h1.inmage_hostid == null)
                            {
                                h1.inmage_hostid = tempHost1.inmage_hostid;
                            }
                            if (h1.mbr_path == null)
                            {
                                h1.mbr_path = tempHost1.mbr_path;
                            }
                            if (h1.system_volume == null)
                            {
                                h1.system_volume = tempHost1.system_volume;
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
                                        if (h1.os == OStype.Windows)
                                        {
                                            if (partitionVolume.Contains("C"))
                                            {
                                                h1.system_volume = "C";
                                            }
                                            else
                                            {
                                                h1.system_volume = partitions[0];
                                            }
                                        }
                                        else if (h1.os == OStype.Linux)
                                        {

                                            h1.system_volume = partitionVolume;
                                        }
                                    }
                                    else
                                    {
                                        h1.system_volume = partitionVolume;
                                    }

                                    Trace.WriteLine(DateTime.Now + "\t System volume for the host: " + h1.displayname + " is " + h.system_volume);
                                }
                            }
                            foreach (Hashtable hash in h1.disks.list)
                            {
                                foreach (Hashtable diskHash in tempHost1.disks.list)
                                {
                                    if (hash["src_devicename"] == null)
                                    {
                                        if (hash["Name"] != null && diskHash["Name"] != null)
                                        {
                                            hash["src_devicename"] = diskHash["src_devicename"];
                                        }
                                    }
                                }
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
                        foreach (Host h1 in selectedSourceList._hostList)
                        {
                            foreach (XmlNode node in hostNodes1)
                            {
                                if (h1.hostname.ToUpper() == node.Attributes["hostname"].Value.ToUpper() || h1.displayname.ToUpper() == node.Attributes["display_name"].Value.ToUpper())
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
                                    if (node.Attributes["system_volume"] != null && node.Attributes["system_volume"].Value.Length == 0)
                                    {
                                        node.Attributes["system_volume"].Value = h1.system_volume;
                                    }
                                    else
                                    {
                                        XmlElement ele = (XmlElement)node;
                                        ele.SetAttribute("system_volume", h1.system_volume);

                                    }
                                    // This is to support earlier protection. That too we are only checking for the Windows protection.
                                    //
                                    if (h1.os == OStype.Windows)
                                    {
                                        if (h1.mbr_path != null)
                                        {
                                            if (node.Attributes["mbr_path"] != null && node.Attributes["mbr_path"].Value.Length == 0)
                                            {
                                                //Trace.WriteLine(DateTime.Now + "\t we are writting new mbr path at the time of resume");
                                                Trace.WriteLine(DateTime.Now + "\t It seems mbr is null. Mean to say it is earlier protection.");
                                                Trace.WriteLine(DateTime.Now + "\t MBR value is " + h1.mbr_path);
                                                node.Attributes["mbr_path"].Value = h1.mbr_path;
                                            }
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
                            if (node.Attributes["display_name"].Value == _masterHost.displayname)
                            {
                                if (node.Attributes["ide_count"].Value.Length == 0)
                                {
                                    node.Attributes["ide_count"].Value = _masterHost.ide_count;
                                }
                            }
                        }
                        foreach (XmlNode node in hostNodes1)
                        {
                            foreach (Host h1 in selectedSourceList._hostList)
                            {
                                if (h1.displayname == node.Attributes["display_name"].Value)
                                {
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
                                                            if (diskNode.Attributes["disk_signature"] != null && diskNode.Attributes["disk_signature"].Value.Length != 0 && !diskNode.Attributes["disk_signature"].Value.ToUpper().Contains("SYSTEM"))
                                                            {
                                                                 Trace.WriteLine(DateTime.Now + "\t Printing disk count " + h.disks.list.Count.ToString());
                                                                 foreach (Hashtable hash in h.disks.list)
                                                                 {
                                                                     if (diskNode.Attributes["disk_signature"].Value == hash["disk_signature"].ToString())
                                                                     {
                                                                         if (diskNode.Attributes["src_devicename"] == null)
                                                                         {
                                                                             XmlElement ele = (XmlElement)diskNode;
                                                                             ele.SetAttribute("src_devicename", hash["src_devicename"].ToString());
                                                                             Trace.WriteLine(DateTime.Now + "\t Added device name as " + hash["src_devicename"].ToString());
                                                                         }
                                                                         if (diskNode.Attributes["scsi_mapping_host"] != null)
                                                                         {
                                                                             //diskNode.Attributes["scsi_mapping_host"].Value = hash["scsi_mapping_host"].ToString();
                                                                             //Trace.WriteLine(DateTime.Now + "\t Added scci mapping as " + hash["scsi_mapping_host"].ToString());
                                                                         }
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
                                                                        if (diskNode.Attributes["disk_name"].Value == hash["Name"].ToString())
                                                                        {
                                                                            Trace.WriteLine(DateTime.Now + "\t Matched the disk name s" + diskNode.Attributes["disk_name"].Value + " \t from hash " + hash["Name"].ToString());
                                                                            if (diskNode.Attributes["src_devicename"] == null)
                                                                            {
                                                                                XmlElement ele = (XmlElement)diskNode;
                                                                                ele.SetAttribute("src_devicename", hash["src_devicename"].ToString());
                                                                                Trace.WriteLine(DateTime.Now + "\t Added device name as " + hash["src_devicename"].ToString());
                                                                                if (diskNode.Attributes["scsi_mapping_host"] != null)
                                                                                {
                                                                                    //diskNode.Attributes["scsi_mapping_host"].Value = hash["scsi_mapping_host"].ToString();
                                                                                    //Trace.WriteLine(DateTime.Now + "\t Added scci mapping as " + hash["scsi_mapping_host"].ToString());
                                                                                }
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
                                                    Trace.WriteLine(DateTime.Now + "\t Faile dto update disks" + ex.Message);
                                                }
                                            }
                                        }
                                    }
                                }

                            }
                        }

                        document1.Save(xmlfile1);
                      
                    }
                    catch (Exception ex)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Failed to update esx.xml with some issue " + ex.Message);
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
                    }
                    else if (returnValue == 0)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Completed readiness checks successfully for resume ");
                        readinessCheck = true;
                    }
                    else
                    {
                        statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                        return false;
                    }
                }
                if (dummyDiskCreation == false)
                {// In case of linux we are calling linux script beofore creating the dummy disk calls.
                    // Because using cent os 6.2 we are not getting all the disks.
                    // For that we have changed the scripts also. By calling this before creating the dummy disks.
                    // We are calling this linux script.

                    _masterHost.esxIp = h.targetESXIp;
                    _masterHost.esxUserName = h.targetESXUserName;
                    _masterHost.esxPassword = h.targetESXPassword;
                    string cxip = WinTools.GetcxIPWithPortNumber();

                    if (_masterHost.vx_agent_path == null)
                    {
                        WinPreReqs win = new WinPreReqs("", "", "", "");
                        win.GetDetailsFromcxcli( _masterHost, cxip);
                        _masterHost = WinPreReqs.GetHost;
                    }
                    
                    if (_masterHost.os == OStype.Linux)
                    {
                        _masterHost.requestId = null;
                        if (_masterHost.vx_agent_path == null)
                        {
                            WinPreReqs win = new WinPreReqs("", "", "", "");
                            win.GetDetailsFromcxcli( _masterHost, cxip);
                            _masterHost = WinPreReqs.GetHost;
                        }
                       
                        ArrayList diskList = new ArrayList();
                        RunningScriptInLinuxmt(_masterHost, ref diskList);
                    }
                    if (_masterHost.inmage_hostid == null)
                    {
                        if (_masterHost.vx_agent_path == null)
                        {
                            WinPreReqs win = new WinPreReqs("", "", "", "");
                            win.GetDetailsFromcxcli( _masterHost, cxip);
                             _masterHost= WinPreReqs.GetHost;
                        }
                    }
                    if (CreateDummydisk(_masterHost) == false)
                    {
                        return false;
                    }

                }
                if (createGuestOnTargetSucceed == false)
                {
                    if (_esxInfo.ExecuteCreateTargetGuestScript(h.targetESXIp, OperationType.Resume) == 0)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Completed creating VMs on target ");
                        createGuestOnTargetSucceed = true;
                        statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                       
                    }
                    else
                    {
                        Trace.WriteLine(DateTime.Now + "\t Failed Completed creating VMs on target ");
                        createGuestOnTargetSucceed = false;
                        statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                        return false;
                    }
                }
                try
                {
                    Trace.WriteLine(DateTime.Now + "\t Going to set consistency job ");

                    if (jobAutomationSucceed == false)
                    {



                        if (_esxInfo.ExecuteJobAutomationScript(OperationType.Resume, "yes") == 0)
                        {
                            //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
                            //based on above comment we have added xmlresolver as null
                            XmlDocument document = new XmlDocument();
                            document.XmlResolver = null;
                            string xmlfile = latestFilePath + "\\ESX.xml";

                            //StreamReader reader = new StreamReader(xmlfile);
                            //string xmlString = reader.ReadToEnd();
                            //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
                            XmlReaderSettings settings = new XmlReaderSettings();
                            settings.ProhibitDtd = true;
                            settings.XmlResolver = null;
                            using (XmlReader reader1 = XmlReader.Create(xmlfile, settings))
                            {
                                document.Load(reader1);
                                //reader1.Close();
                            }
                            //document.Load(xmlfile);
                            //reader.Close();
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
                            Trace.WriteLine(DateTime.Now + "\t Printing the directory name " + directoryNameinXmlFile);
                            foreach (XmlNode node in hostNodes)
                            {
                                node.Attributes["directoryName"].Value = directoryNameinXmlFile;
                            }
                            Trace.WriteLine(DateTime.Now + "\t updated dir and disk attribute and going to save file");
                            document.Save(xmlfile);
                            Trace.WriteLine(DateTime.Now + "Completed writing the directory name to all hosts");

                            if (File.Exists(latestFilePath + "\\ESX.xml"))
                            {

                                File.Copy(latestFilePath + "\\ESX.xml", fxFailOverDataPath + "\\" + directoryNameinXmlFile + "\\ESX.xml", true);
                                Trace.WriteLine(DateTime.Now + "\t Successfully copied fixed to the " + fxFailOverDataPath + "\\" + directoryNameinXmlFile + "\\ESX.xml");

                            }
                            jobAutomationSucceed = true;

                            Trace.WriteLine(DateTime.Now + " \t Successfully to set the fx jobs");
                        }
                        else
                        {
                            jobAutomationSucceed = false;
                            Trace.WriteLine(DateTime.Now + " \t Failed to set the fx jobs");
                            statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                            return false;
                        }
                    }
                    if (jobAutomationSucceed == true)
                    {
                        MasterConfigFile masterCOnfig = new MasterConfigFile();
                        if (masterCOnfig.UpdateMasterConfigFile(latestFilePath + "\\ESX.xml"))
                        {
                            Trace.WriteLine(DateTime.Now + "\t Successfully completed the upload of masterconfigfile.xml ");

                            //_uploadfiletocx = true;
                        }
                        else
                        {
                            Trace.WriteLine(DateTime.Now + " \t Failed to upload MasterConfigFile.xml to the target esx box");
                            uploadfiletocx = false;

                            return false;
                        }

                    }
                    if (jobAutomationSucceed == true && esxutilwinExecution == false)
                    {
                        if (uploadfiletocx == false)
                        {
                            if (UploadFileAndPrePareInputText(_masterHost.os) == true)
                            {
                                uploadfiletocx = true;
                                Trace.WriteLine(DateTime.Now + "\t Successfully made input.txt file and uploaded all required file to cxps");

                            }
                            else
                            {
                                uploadfiletocx = false;
                                Trace.WriteLine(DateTime.Now + "\t Failed to made input.txt and failed to upload required files to cxps");
                                return false;
                            }
                        }
                        EsxUtilWinExecute(ref _masterHost);

                    }

                    //if (_jobAutomationSucceed && _esxutilwinExecution == false)
                    //{
                    //    if (_scriptID != null)
                    //    {
                    //        CXAPICALLS cxApi = new CXAPICALLS();
                    //        if (cxApi.PostRequestStatusForDummyDisk(ref _masterHost, false) == false)
                    //        {
                    //            CopyLogs(_masterHost);
                    //            Trace.WriteLine(DateTime.Now + "\t Failed to get script out ");
                    //        }
                    //    }
                    //}
                }
                catch (Exception ex)
                {
                    System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                    Trace.WriteLine("ERROR*******************************************");
                    Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                    Trace.WriteLine("Exception  =  " + ex.Message);
                    Trace.WriteLine("ERROR___________________________________________");
                }
                if (esxutilwinExecution == true)
                {

                    // Preparing MasterConfigFile.xml......
                    
                    try
                    {
                        if (File.Exists(WinTools.FxAgentPath() + "\\vContinuum\\logs\\vContinuum_UI.log"))
                        {
                            if (directoryNameinXmlFile != null)
                            {
                                Program.tr3.Dispose();
                                System.IO.File.Move(WinTools.FxAgentPath() + "\\vContinuum\\logs\\vContinuum_UI.log", fxFailOverDataPath + "\\" + directoryNameinXmlFile + "\\vContinuum_UI.log");
                                Program.tr3.Dispose();
                            }
                        }
                    }
                    catch (Exception ex)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Failed to copy vCOntinuum_UI.log " + ex.Message);
                    }

                }
                //CopyLogs(_masterHost);
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
            try
            {
                if (h.machineType == "PhysicalMachine" && h.os == OStype.Windows)
                {
                    foreach (Hashtable diskHash in h.disks.list)
                    {
                        if (diskHash["scsi_mapping_host"] != null)
                        {
                            string[] mappingHost = diskHash["scsi_mapping_host"].ToString().Split(':');
                            if (diskHash["src_devicename"] == null && mappingHost.Length == 3)
                            {
                                Trace.WriteLine(DateTime.Now + "\t Found it seems protection has been done in earlier version of vContinuum.");
                                foreach (Hashtable tempHash in tempHost.disks.list)
                                {
                                    if (tempHash["ScsiPort"] != null && tempHash["ScsiTargetId"] != null && tempHash["ScsiLogicalUnit"] != null)
                                    {
                                        if (diskHash["scsi_mapping_host"].ToString() == tempHash["ScsiPort"].ToString() + ":" + tempHash["ScsiTargetId"].ToString() + ":" + tempHash["ScsiLogicalUnit"].ToString())
                                        {
                                            diskHash["scsi_mapping_host"] = tempHash["scsi_mapping_host"].ToString();
                                            Trace.WriteLine(DateTime.Now + "\t Update scsci mapping host to " + tempHash["scsi_mapping_host"].ToString());
                                            diskHash["src_devicename"] = tempHash["src_devicename"].ToString();
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
                Trace.WriteLine(DateTime.Now + "\t Failed to update the disk values " + ex.Message);
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
                        if (hash["src_devicename"] != null && tempHash["src_devicename"] != null && tempHash["scsi_mapping_host"] != null)
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
                                if (hash["Name"].ToString() == tempHash["Name"].ToString())
                                {
                                    hash["scsi_mapping_host"] = tempHash["scsi_mapping_host"].ToString();
                                }
                            }
                        }
                    }
                }
            }

            return true;
        }



        private bool RefreshHosts(HostList _selectedHostList)
        {
            try
            {


                if (_appName == AppName.V2P)
                {
                    foreach (Host h in _selectedHostList._hostList)
                    {
                        Host h1 = h;
                        string cxip = WinTools.GetcxIPWithPortNumber();
                        WinPreReqs win = new WinPreReqs("", "", "", "");
                        if (win.GetDetailsFromcxcli(h1, cxip) == true)
                        {
                            h1 = WinPreReqs.GetHost;
                            h1.inmage_hostid = h.inmage_hostid;
                        }
                        Cxapicalls cxApis = new Cxapicalls();
                        if (cxApis.PostRefreshWithall(h1) == true)
                        {
                            h1 = Cxapicalls.GetHost;
                            h.requestId = h1.requestId;
                            Trace.WriteLine(DateTime.Now + "\t Successfully request for refresh host info with request id " + h.requestId);
                        }
                        else
                        {
                            if (cxApis.PostRefreshWithall(h1) == true)
                            {
                                h1 = Cxapicalls.GetHost;
                                h.requestId = h1.requestId;
                                Trace.WriteLine(DateTime.Now + "\t Successfully request for refresh host info with request id " + h.requestId);
                            }
                        }
                    }
                }
                else
                {
                    foreach (Host h in _selectedHostList._hostList)
                    {
                        Host h1 = h;
                        string cxip = WinTools.GetcxIPWithPortNumber();
                        WinPreReqs win = new WinPreReqs("", "", "", "");
                        if (win.GetDetailsFromcxcli(h1, cxip) == true)
                        {
                            h1 = WinPreReqs.GetHost;
                            h1.inmage_hostid = h.inmage_hostid;
                        }
                        Cxapicalls cxApis = new Cxapicalls();
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
                }
                Trace.WriteLine(DateTime.Now + "\t Successfully post the request for all the hosts. Now going to sleep for 65 seconds ");
                Thread.Sleep(65000);

                foreach (Host h in _selectedHostList._hostList)
                {

                    Host tempHost = new Host();
                    tempHost.requestId = h.requestId;
                    Cxapicalls cxApis = new Cxapicalls();

                    int i = 0;
                    while (i < 10)
                    {
                        i++;

                        if (cxApis.Post(tempHost, "GetRequestStatus") == true)
                        {

                            i = 11;
                            tempHost = Cxapicalls.GetHost;
                            Trace.WriteLine(DateTime.Now + "\t Successfully get request status " + h.displayname + " new displayname " + h.new_displayname);
                            Host h1 = h;
                            OutPutOfRefreshHostStatus(ref h1, tempHost);
                            if (h.inmage_hostid == null)
                            {
                                h.inmage_hostid = h1.inmage_hostid;
                            }
                            if (h.mbr_path == null || _appName == AppName.V2P)
                            {
                                h.mbr_path = tempHost.mbr_path;
                                h.clusterMBRFiles = tempHost.clusterMBRFiles;
                            }
                            if (h.system_volume == null)
                            {
                                h.system_volume = h1.system_volume;
                            }
                            if (h.machineType.ToUpper() == "PHYSICALMACHINE" && h.os == OStype.Windows)
                            {
                                h.disks.list = h1.disks.list;
                            }
                        }
                        else
                        {
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



        public bool RecoverSelectedVms(ref Host masterHost)
        {
            try
            {
                if (_appName == AppName.Drdrill)
                {
                    if (dummyDiskCreation == false)
                    {
                        Esx esx = new Esx();
                        // This metod will create dummy disks add that will attach disk to the mt.
                        
                        if (masterHost.os == OStype.Linux)
                        {
                            ArrayList diskList = new ArrayList();
                            masterHost.requestId = null;
                            RunningScriptInLinuxmt(masterHost, ref diskList);
                        }
                        if (CreateDummydisk(masterHost) == false)
                        {
                            Trace.WriteLine(DateTime.Now + "\t Failed to create dummy disks");
                            statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                            return false;
                        }
                        else
                        {
                            Trace.WriteLine(DateTime.Now + "\t Successfully completed dummy disks creation");
                        }
                    }


                    // This will be called to create the vms on the target side.....
                    if (createGuestOnTargetSucceed == false)
                    {

                        if (CreateVMs(masterHost) == false)
                        {
                            createGuestOnTargetSucceed = false;
                            statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                            return false;
                        }
                        else
                        {
                            if (File.Exists(WinTools.FxAgentPath() + "\\vContinuum\\Latest\\Snapshot.xml"))
                            {
                                File.Copy(WinTools.FxAgentPath() + "\\vContinuum\\Latest\\Snapshot.xml", fxFailOverDataPath + "\\" + directoryNameinXmlFile + "\\Snapshot.xml", true);
                            }
                            createGuestOnTargetSucceed = true;
                            Trace.WriteLine(DateTime.Now + "\t Successfully completed creating vms on secondary server");
                            //  statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                            // _message = " Creating VM(s) on target server completed successfully. \t Completed:" + DateTime.Now + Environment.NewLine;
                            //allServersForm.protectionText.Invoke(tickerDelegate, new object[] { _message });
                        }
                    }
                }
                if (recoveredVms == false)
                {
                    string cxIP = WinTools.GetcxIPWithPortNumber();

                    if (masterHost.vx_agent_path == null)
                    {
                        Host tempHost = new Host();
                        tempHost.ip = masterHost.ip;
                        tempHost.hostname = masterHost.hostname;
                        tempHost.inmage_hostid = masterHost.inmage_hostid;
                        WinPreReqs winpre = new WinPreReqs("", "", "", "");
                        winpre.GetDetailsFromcxcli( tempHost, cxIP);
                        tempHost = WinPreReqs.GetHost;
                        masterHost.vx_agent_path = tempHost.vx_agent_path;
                        // masterHost.inmage_hostid = tempHost.inmage_hostid;
                        masterHost.ip = tempHost.ip;

                    }

                    if (UploadFileAndPrePareInputText(masterHost.os) == true)
                    {
                        uploadfiletocx = true;
                        statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                        Trace.WriteLine(DateTime.Now + "\t Uploading file got completed");

                    }
                    else
                    {
                        statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                        Trace.WriteLine(DateTime.Now + "\t Failed to upload config files to cx");
                        uploadfiletocx = false;
                        return false;
                    }
                    if (recoveredVms == false)
                    {
                        if (Recovery(ref masterHost) == true)
                        {
                            Trace.WriteLine(DateTime.Now + "\t Successfully rollbaced the volumnes");

                        }
                        else
                        {
                            //CopyLogs(masterHost);
                            Trace.WriteLine(DateTime.Now + "\t Failed to rollback the vms");
                            return false;
                        }
                    }

                    //if (DownloadRequiredFiles() == true)
                    //{

                    //    statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                    //    PowerOnVMs(masterHost);
                    //}
                    //else
                    //{
                    //    statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                    //    return false;
                    //}

                    //CopyLogs(masterHost);
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

        //This will update Recovery.xml to masterconfigfile when recovery later option is selected.
        private bool UpdatePalnIDAndMergeConfigFile(string planId)
        {
            try
            {
                XmlDocument document = new XmlDocument();
                document.XmlResolver = null;
                string xmlfile = latestFilePath + "\\Recovery.xml";

                //StreamReader reader = new StreamReader(xmlfile);
                //string xmlString = reader.ReadToEnd();
                //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
                XmlReaderSettings settings = new XmlReaderSettings();
                settings.ProhibitDtd = true;
                settings.XmlResolver = null;
                using (XmlReader reader1 = XmlReader.Create(xmlfile, settings))
                {
                    document.Load(reader1);
                    //reader1.Close();
                }
                //document.Load(xmlfile);
                //reader.Close();
                //XmlNodeList hostNodes = null;
                XmlNodeList hostNodes = null;
                XmlNodeList diskNodes = null;
                XmlNodeList rootNodes = null;
                rootNodes = document.GetElementsByTagName("root");
                hostNodes = document.GetElementsByTagName("host");
                diskNodes = document.GetElementsByTagName("disk");

                foreach (XmlNode node in hostNodes)
                {
                    node.Attributes["recovery_plan_id"].Value = planId;
                }

                document.Save(xmlfile);


                MasterConfigFile master = new MasterConfigFile();
                master.UpdateMasterConfigFile(xmlfile);
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

        public bool Recovery(ref Host masterHost)
        {
            try
            {
                Trace.WriteLine(DateTime.Now + "\t Getting plan id form the cx api ");
                planId = null;
                string path = WinTools.FxAgentPath() + "\\Failover\\Data\\" + directoryNameinXmlFile;
                Cxapicalls cxapi = new Cxapicalls();

                try
                {

                    WinPreReqs winPreReqs = new WinPreReqs("", "", "", "");
                    ArrayList planNameList = new ArrayList();
                    winPreReqs.GetPlanNames( planNameList, WinTools.GetcxIPWithPortNumber());
                    planNameList = WinPreReqs.GetPlanlist;
                    if (_appName == AppName.Recover)
                    {
                        masterHost.initializingRecoveryPlan = null;
                        masterHost.downloadingConfigurationfiles = null;
                        masterHost.powerOnVMs = null;
                        masterHost.startingRecoveryForSelectedVms = null;
                        masterHost.powerOnVMs = null;
                        foreach (string s in planNameList)
                        {
                            if (s == masterHost.plan)
                            {
                                Host h1 = new Host();
                                h1.plan = masterHost.plan;
                                h1.operationType = OperationType.Recover;
                                cxapi.Post( h1, "RemoveExecutionStep");
                                h1 = Cxapicalls.GetHost;
                                break;
                            }
                        }
                    }
                    else if (_appName == AppName.Drdrill)
                    {
                        masterHost.initializingDRDrillPlan = null;
                        masterHost.downloadingConfigurationfiles = null;
                        masterHost.preparingMasterTargetDisks = null;
                        masterHost.updatingDiskLayouts = null;
                        masterHost.startingPhysicalSnapShotforSelectedVMs = null;
                        masterHost.initializingPhysicalSnapShotOperation = null;
                        masterHost.powerOnVMs = null;

                        try
                        {
                            masterHost.taskList = new ArrayList();
                        }
                        catch (Exception ex)
                        {
                            Trace.WriteLine(DateTime.Now + "\t Failed to new arraylist " + ex.Message);
                        }
                        foreach (string s in planNameList)
                        {
                            if (s == masterHost.plan)
                            {
                                Host h1 = new Host();
                                h1.plan = masterHost.plan;
                                h1.operationType = OperationType.Drdrill;
                                cxapi.Post( h1, "RemoveExecutionStep");
                                h1 = Cxapicalls.GetHost;
                                break;
                            }
                        }
                    }
                }
                catch (Exception ex)
                {
                    Trace.WriteLine(DateTime.Now + "\t Failed to remove plan from database " + ex.Message);
                }
                masterHost.runEsxUtilCommand = true;
                masterHost.requestId = null;
                if (_appName == AppName.Recover)
                {
                    if (masterHost.os == OStype.Windows)
                    {
                        masterHost.commandtoExecute = "'" + masterHost.vx_agent_path + "\\EsxUtil.exe" + "'" + " -rollback -d " + "'" + masterHost.vx_agent_path + "\\FailOver\\Data\\" + directoryNameinXmlFile + "'" + " -cxpath " + "'" + "/home/svsystems/vcon/" + directoryNameinXmlFile + "'";
                    }
                    else
                    {
                        masterHost.commandtoExecute = "'" + masterHost.vx_agent_path + "bin/EsxUtil" + "'" + " -rollback -d " + "'" + masterHost.vx_agent_path + "/failover_data/" + directoryNameinXmlFile + "'" + " -cxpath " + "'" + "/home/svsystems/vcon/" + directoryNameinXmlFile + "'";
                    }
                }
                else if (_appName == AppName.Drdrill)
                {
                    if (masterHost.os == OStype.Windows)
                    {
                        masterHost.commandtoExecute = "'" + masterHost.vx_agent_path + "\\consistency\\VCon_Physical_Snapshot.bat " + "'" + " " + "'" + masterHost.vx_agent_path + "\\FailOver\\Data\\" + directoryNameinXmlFile + "'" + " " + "'" + "/home/svsystems/vcon/" + directoryNameinXmlFile + "'";
                    }
                    else
                    {
                        masterHost.commandtoExecute = "'" + masterHost.vx_agent_path + "scripts/vCon/VCon_Physical_Snapshot.sh" + "'" + " " + "'" + masterHost.vx_agent_path + "/failover_data/" + directoryNameinXmlFile + "'" + " " + "'" + "/home/svsystems/vcon/" + directoryNameinXmlFile + "'";
                    }
                }
              
                
                if (_appName == AppName.Recover)
                {
                    if (AllServersForm.v2pRecoverySelected == false)
                    {
                        masterHost.perlCommand = "\"" + WinTools.FxAgentPath() + "\\vContinuum\\Scripts\\Recovery.pl" + "\"" + " --server " + masterHost.esxIp + " --directory " + "\"" + fxFailOverDataPath + "\\" + directoryNameinXmlFile + "\"" + " --recovery yes --cxpath " + "\"" + "/home/svsystems/vcon/" + directoryNameinXmlFile + "\"";
                    }
                    else
                    {
                        masterHost.perlCommand = "\"" + WinTools.FxAgentPath() + "\\vContinuum\\Scripts\\Recovery.pl" + "\"" + " --directory " + "\"" + fxFailOverDataPath + "\\" + directoryNameinXmlFile + "\"" + " --recovery_v2p yes --cxpath " + "\"" + "/home/svsystems/vcon/" + directoryNameinXmlFile + "\"" + " --server " + "10.0.1.45" + " --username " + "\"" + "root" + "\"" + " --password " + "\"" + "aeiou+=+0" + "\"";
                    }
                }
                else if (_appName == AppName.Drdrill)
                {
                    masterHost.perlCommand = "\"" + WinTools.FxAgentPath() + "\\vContinuum\\Scripts\\Recovery.pl" + "\"" + " --server " + masterHost.esxIp  + " --directory " + "\"" + fxFailOverDataPath + "\\" + directoryNameinXmlFile + "\"" + " --dr_drill yes --cxpath " + "\"" + "/home/svsystems/vcon/" + directoryNameinXmlFile + "\"";
                }
               
                 PrepareBatchScript(masterHost);
                
                masterHost.tardir = fxFailOverDataPath + "\\" + directoryNameinXmlFile;
                masterHost.perlCommand = "'" + fxFailOverDataPath + "\\"+ directoryNameinXmlFile + "\\InMage_Script.bat" + "'";
                if (masterHost.os == OStype.Windows)
                {
                    masterHost.srcDir = masterHost.vx_agent_path + "\\Failover\\Data\\" + directoryNameinXmlFile;
                }
                else
                {
                    masterHost.srcDir = masterHost.vx_agent_path + "failover_data/" + directoryNameinXmlFile;
                }
                //if (cxapi.Post(ref masterHost, "InsertScriptPolicy") == true)
                //{
                //    _policyAPI = true;
                //    _scriptID = masterHost.requestId;
                //    Trace.WriteLine(DateTime.Now + "\t Successfully set the script policy and request id is " + _scriptID);
                //}
                if (_appName == AppName.Recover)
                {

                    getplanidapi = cxapi.PostFXJob( masterHost,  OperationType.Recover);
                    masterHost = Cxapicalls.GetHost;
                    if (getplanidapi == true)
                    {
                        if (masterHost.recoveryLater == true)
                        {
                            UpdatePalnIDAndMergeConfigFile(masterHost.planid);
                            Trace.WriteLine(DateTime.Now + "\t updated plan id " + masterHost.planid);
                            Trace.WriteLine(DateTime.Now + "\t Successfully set the fx job. User may run the job when ever he wants to recover");
                            recoveredVms = true;
                            return true;
                        }
                    }
                }
                else if (_appName == AppName.Drdrill)
                {
                    getplanidapi = cxapi.PostFXJob( masterHost,  OperationType.Drdrill);
                    masterHost = Cxapicalls.GetHost;
                }
                if (getplanidapi == true)
                {
                    planId = masterHost.planid;

                }
                else
                {
                    masterHost.initializingRecoveryPlan = "Failed";
                    masterHost.initializingDRDrillPlan = "Failed";
                    statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                    Trace.WriteLine(DateTime.Now + "\t Failed to get plan iod using cx api");
                    return false;
                }
                Thread.Sleep(45000);
                while (statusApi == false)
                {
                    bool cxstatusAPI = false;
                    if (_appName == AppName.Recover)
                    {
                        masterHost.operationType = OperationType.Recover;
                    }
                    else if (_appName == AppName.Drdrill)
                    {
                        masterHost.operationType = OperationType.Drdrill;
                    }
                    cxstatusAPI = cxapi.Post( masterHost, "MonitorESXProtectionStatus");
                    masterHost = Cxapicalls.GetHost;
                    foreach (Hashtable hash in masterHost.taskList)
                    {
                        if (hash["Name"] != null)
                        {
                            if (_appName == AppName.Recover)
                            {
                                if (hash["Name"].ToString() == "Initializing Recovery Plan")
                                {
                                    if (hash["TaskStatus"].ToString() == "Completed" || hash["TaskStatus"].ToString() == "Complete")
                                    {
                                        masterHost.initializingRecoveryPlan = "Completed";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Failed")
                                    {
                                        masterHost.initializingRecoveryPlan = "Failed";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Queued")
                                    {
                                        masterHost.initializingRecoveryPlan = "Notstarted";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "InProgress")
                                    {
                                        masterHost.initializingRecoveryPlan = "Pending";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Warning")
                                    {
                                        masterHost.initializingRecoveryPlan = "Warning";
                                    }
                                }
                                if (hash["Name"].ToString() == "Downloading Configuration Files")
                                {
                                    if (hash["TaskStatus"].ToString() == "Completed" || hash["TaskStatus"].ToString() == "Complete")
                                    {
                                        masterHost.downloadingConfigurationfiles = "Completed";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Failed")
                                    {
                                        masterHost.downloadingConfigurationfiles = "Failed";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Queued")
                                    {
                                        masterHost.downloadingConfigurationfiles = "Notstarted";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "InProgress")
                                    {
                                        masterHost.downloadingConfigurationfiles = "Pending";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Warning")
                                    {
                                        masterHost.downloadingConfigurationfiles = "Warning";
                                    }
                                }
                                if (hash["Name"].ToString() == "Starting Recovery For Selected VM(s)")
                                {
                                    if (hash["TaskStatus"].ToString() == "Completed" || hash["TaskStatus"].ToString() == "Complete")
                                    {
                                        masterHost.startingRecoveryForSelectedVms = "Completed";

                                        //masterHost.powerOnVMs = "Completed";

                                    }
                                    else if (hash["TaskStatus"].ToString() == "Failed")
                                    {
                                        masterHost.startingRecoveryForSelectedVms = "Failed";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Queued")
                                    {
                                        masterHost.startingRecoveryForSelectedVms = "Notstarted";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "InProgress")
                                    {
                                        masterHost.startingRecoveryForSelectedVms = "Pending";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Warning")
                                    {
                                        masterHost.startingRecoveryForSelectedVms = "Warning";
                                    }
                                }

                                if (hash["Name"].ToString() == "Powering on the recovered VM(s)")
                                {
                                    if (hash["TaskStatus"].ToString() == "Completed" || hash["TaskStatus"].ToString() == "Complete")
                                    {
                                        masterHost.powerOnVMs = "Completed";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Failed")
                                    {
                                        masterHost.powerOnVMs = "Failed";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Queued")
                                    {
                                        masterHost.powerOnVMs = "Notstarted";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "InProgress")
                                    {
                                        masterHost.powerOnVMs = "Pending";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Warning")
                                    {
                                        masterHost.powerOnVMs = "Warning";
                                    }
                                }

                            }
                            else if (_appName == AppName.Drdrill)
                            {
                                if (hash["Name"].ToString() == "Initializing Dr-Drill Plan")
                                {
                                    if (hash["TaskStatus"].ToString() == "Completed" || hash["TaskStatus"].ToString() == "Complete")
                                    {
                                        masterHost.initializingDRDrillPlan = "Completed";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Failed")
                                    {
                                        masterHost.initializingDRDrillPlan = "Failed";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Queued")
                                    {
                                        masterHost.initializingDRDrillPlan = "Notstarted";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "InProgress")
                                    {
                                        masterHost.initializingDRDrillPlan = "Pending";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Warning")
                                    {
                                        masterHost.initializingDRDrillPlan = "Warning";
                                    }
                                }

                                if (hash["Name"].ToString() == "Downloading Configuration Files")
                                {
                                    if (hash["TaskStatus"].ToString() == "Completed" || hash["TaskStatus"].ToString() == "Complete")
                                    {
                                        masterHost.downloadingConfigurationfiles = "Completed";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Failed")
                                    {
                                        masterHost.downloadingConfigurationfiles = "Failed";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Queued")
                                    {
                                        masterHost.downloadingConfigurationfiles = "Notstarted";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "InProgress")
                                    {
                                        masterHost.downloadingConfigurationfiles = "Pending";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Warning")
                                    {
                                        masterHost.downloadingConfigurationfiles = "Warning";
                                    }
                                }
                                if (hash["Name"].ToString() == "Preparing Master Target Disks")
                                {
                                    if (hash["TaskStatus"].ToString() == "Completed" || hash["TaskStatus"].ToString() == "Complete")
                                    {
                                        masterHost.preparingMasterTargetDisks = "Completed";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Failed")
                                    {
                                        masterHost.preparingMasterTargetDisks = "Failed";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Queued")
                                    {
                                        masterHost.preparingMasterTargetDisks = "Notstarted";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "InProgress")
                                    {
                                        masterHost.preparingMasterTargetDisks = "Pending";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Warning")
                                    {
                                        masterHost.preparingMasterTargetDisks = "Warning";
                                    }
                                }
                                if (hash["Name"].ToString() == "Updating Disk Layouts")
                                {
                                    if (hash["TaskStatus"].ToString() == "Completed" || hash["TaskStatus"].ToString() == "Complete")
                                    {
                                        masterHost.updatingDiskLayouts = "Completed";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Failed")
                                    {
                                        masterHost.updatingDiskLayouts = "Failed";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Queued")
                                    {
                                        masterHost.updatingDiskLayouts = "Notstarted";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "InProgress")
                                    {
                                        masterHost.updatingDiskLayouts = "Pending";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Warning")
                                    {
                                        masterHost.updatingDiskLayouts = "Warning";
                                    }
                                }
                                if (hash["Name"].ToString() == "Initializing Physical Snapshot Operation")
                                {
                                    if (hash["TaskStatus"].ToString() == "Completed" || hash["TaskStatus"].ToString() == "Complete")
                                    {
                                        masterHost.initializingPhysicalSnapShotOperation = "Completed";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Failed")
                                    {
                                        masterHost.initializingPhysicalSnapShotOperation = "Failed";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Queued")
                                    {
                                        masterHost.initializingPhysicalSnapShotOperation = "Notstarted";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "InProgress")
                                    {
                                        masterHost.initializingPhysicalSnapShotOperation = "Pending";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Warning")
                                    {
                                        masterHost.initializingPhysicalSnapShotOperation = "Warning";
                                    }
                                }

                                if (hash["Name"].ToString() == "Starting Physical Snapshot For Selected VM(s)")
                                {
                                    if (hash["TaskStatus"].ToString() == "Completed" || hash["TaskStatus"].ToString() == "Complete")
                                    {
                                        masterHost.startingPhysicalSnapShotforSelectedVMs = "Completed";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Failed")
                                    {
                                        masterHost.startingPhysicalSnapShotforSelectedVMs = "Failed";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Queued")
                                    {
                                        masterHost.startingPhysicalSnapShotforSelectedVMs = "Notstarted";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "InProgress")
                                    {
                                        masterHost.startingPhysicalSnapShotforSelectedVMs = "Pending";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Warning")
                                    {
                                        masterHost.startingPhysicalSnapShotforSelectedVMs = "Warning";
                                    }
                                }
                                if (hash["Name"].ToString() == "Powering on the dr drill VM(s)")
                                {
                                    if (hash["TaskStatus"].ToString() == "Completed" || hash["TaskStatus"].ToString() == "Complete")
                                    {
                                        esxutilwinExecution = true;
                                        masterHost.powerOnVMs = "Completed";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Failed")
                                    {
                                        masterHost.powerOnVMs = "Failed";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Queued")
                                    {
                                        masterHost.powerOnVMs = "Notstarted";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "InProgress")
                                    {
                                        masterHost.powerOnVMs = "Pending";
                                    }
                                    else if (hash["TaskStatus"].ToString() == "Warning")
                                    {
                                        masterHost.powerOnVMs = "Warning";
                                    }
                                }
                            }
                        }
                        if (hash["LogPath"] != null && hash["TaskStatus"] != null && hash["TaskStatus"].ToString() == "Failed")
                        {
                            logPath = hash["LogPath"].ToString();
                            DownloadRequiredFiles();
                        }
                    }
                    if (masterHost.powerOnVMs == "Completed")
                    {
                        recoveredVms = true;
                        statusApi = true;

                        statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                        return true;
                    }
                    else if(masterHost.powerOnVMs == "Failed")
                    {
                        recoveredVms = false;
                        statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                        return false;
                    }
                    if (_appName == AppName.Recover)
                    {
                        if (masterHost.initializingRecoveryPlan == "Failed" || masterHost.downloadingConfigurationfiles == "Failed" || masterHost.startingRecoveryForSelectedVms == "Failed")
                        {
                            statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                            recoveredVms = false;
                            return false;
                        }
                    }
                    else if (_appName == AppName.Drdrill)
                    {
                        if (masterHost.initializingDRDrillPlan == "Failed" || masterHost.downloadingConfigurationfiles == "Failed" || masterHost.preparingMasterTargetDisks == "Failed" || masterHost.updatingDiskLayouts == "Failed" || masterHost.startingPhysicalSnapShotforSelectedVMs == "Failed" || masterHost.initializingPhysicalSnapShotOperation == "Failed")
                        {
                            statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                            recoveredVms = false;
                            return false;
                        }
                    }
                    statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                    Thread.Sleep(25000);
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

        public bool PowerOnVMs(Host masterHost)
        {
            try
            {
                Esx esxInfo = new Esx();
                if (esxInfo.WmiRecovery(masterHost.esxIp, fxFailOverDataPath + "\\" + directoryNameinXmlFile ,"/home/svsystems/vcon/"+ directoryNameinXmlFile, OperationType.Recover) == 0)
                {
                    Trace.WriteLine(DateTime.Now + "\t Successfully recovery the selected vms");
                    //_poweronVmsstring = "Completed";
                    //_poweronVms = true;
                    statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                    return true;
                }
                else
                {
                    // _poweronVmsstring = "Failed";
                    statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                    Trace.WriteLine(DateTime.Now + "\t Failed to power on the vms");
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

        public bool DownloadRequiredFiles()
        {
            try
            {
                if (logPath != null)
                {
                    try
                    {
                        //if (File.Exists("C:\\windows\\temp\\InMage_Recovered_Vms.rollback"))
                        //{
                        //    File.Delete("C:\\windows\\temp\\InMage_Recovered_Vms.rollback");
                        //}
                        if (File.Exists("C:\\windows\\temp\\vCon_Error.log"))
                        {
                            File.Delete("C:\\windows\\temp\\vCon_Error.log");
                        }
                        if (File.Exists(WinTools.FxAgentPath() + "\\vContinuum\\logs\\vCon_Error.log"))
                        {
                            File.Delete(WinTools.FxAgentPath() + "\\vContinuum\\logs\\vCon_Error.log");
                        }
                    }
                    catch (Exception ex)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Failed to delete file form windows temp folder " + ex.Message);
                    }
                    if (WinTools.DownloadFileToCX("\"" + logPath + "\"", "\"" + "C:\\windows\\temp\\vCon_Error.log" + "\"") == 0)
                    {
                        if (File.Exists("C:\\windows\\temp\\vCon_Error.log"))
                        {
                            File.Copy("C:\\windows\\temp\\vCon_Error.log", WinTools.FxAgentPath() + "\\vContinuum\\logs\\vCon_Error.log", true);
                        }

                        Trace.WriteLine(DateTime.Now + "\t Successfully downloaded file form cx vCon_Error.log");
                        return true;
                    }
                    else
                    {

                        Trace.WriteLine(DateTime.Now + "\t Failed to download file from cx");
                        return false;
                    }
                }
                else
                {
                    Trace.WriteLine(DateTime.Now + "\t Log file value is null ");
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

        private void logLinkLabel_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
        {
            try
            {

                if (File.Exists(WinTools.FxAgentPath() + "\\vContinuum\\logs\\vCon_Error.log"))
                {
                    Process.Start("notepad.exe", WinTools.FxAgentPath() + "\\vContinuum\\logs\\vCon_Error.log");
                }

                if (_appName != AppName.Recover && _appName != AppName.Removedisk)
                {
                    if (createGuestOnTargetSucceed == false || dummyDiskCreation == false)
                    {
                        if (File.Exists(WinTools.FxAgentPath() + "\\vContinuum\\logs\\vContinuum_ESX.log"))
                        {
                            Process.Start("notepad.exe", WinTools.FxAgentPath() + "\\vContinuum\\logs\\vContinuum_ESX.log");
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed to open ESXUtilorWin.log " + ex.Message);
            }
        }


        private bool PrepareBatchScript(Host masterHost)
        {
            Trace.WriteLine(DateTime.Now + "\t Entered to prepare batch script");
            try
            {
                if (File.Exists(fxFailOverDataPath + "\\" + directoryNameinXmlFile + "\\InMage_Script.bat"))
                {
                    File.Delete(fxFailOverDataPath + "\\" + directoryNameinXmlFile + "\\InMage_Script.bat");
                }
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed to delete existing inmage script batch file");
            }
            try
            {
                FileInfo f1 = new FileInfo(fxFailOverDataPath + "\\" + directoryNameinXmlFile + "\\InMage_Script.bat");
                StreamWriter sw = f1.CreateText();
                sw.WriteLine("echo off");
                sw.WriteLine("SET PLANID=%2");
                sw.WriteLine("if \"%PROCESSOR_ARCHITECTURE%\"==\"x86\" (");
                sw.WriteLine("set REG_OS=SOFTWARE");
                sw.WriteLine(") else (");
                sw.WriteLine("set REG_OS=SOFTWARE\\Wow6432Node");
                sw.WriteLine(")");
                sw.WriteLine("ver | find \"XP\" > nul");
                sw.WriteLine("if %ERRORLEVEL% == 0 (");
                sw.WriteLine("FOR /F \"tokens=2* \" %%A IN ('REG QUERY \"HKLM\\%REG_OS%\\SV Systems\\vContinuum\" /v vSphereCLI_Path') DO SET vmpath=%%B");
                sw.WriteLine(") else (");
                sw.WriteLine("FOR /F \"tokens=2* delims= \" %%A IN ('REG QUERY \"HKLM\\%REG_OS%\\SV Systems\\vContinuum\" /v vSphereCLI_Path') DO SET vmpath=%%B");
                sw.WriteLine(")");
                sw.WriteLine("set PATH=%vmpath%;%path%;%windir%;%windir%\\system32;%windir%\\system32\\wbem;");
                sw.WriteLine("echo PATH = %PATH%");
                sw.WriteLine("cd " +"\"" +  WinTools.FxAgentPath() + "\\vContinuum\\Scripts" + "\"");
                sw.WriteLine( masterHost.perlCommand + " --planid %PLANID%");
                sw.Close();

                WinTools win = new WinTools();
                win.SetFilePermissions(fxFailOverDataPath + "\\" + directoryNameinXmlFile + "\\InMage_Script.bat");

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

    }
}
