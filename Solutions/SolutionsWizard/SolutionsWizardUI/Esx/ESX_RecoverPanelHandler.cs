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
using System.Text.RegularExpressions;
using System.IO;

namespace Com.Inmage.Wizard
{
    class Esx_RecoverPanelHandler : PanelHandler
    {
        internal static int HOSTNAME_COLUMN = 0;
        internal static int DISPLAY_NAME_COLUMN = 1;
        internal static int RECOVER_CHECK_BOX_COLUMN = 3;
        internal static int MASTERTARGET_DISPLAYNAME = 2;

        internal static int CHECK_REPORT_SERVER_NAME_COLUMN = 0;
        internal static int CHECK_REPORT_TAG_COLUMN = 1;
        internal static int CHECK_RESULT_COLUMN = 2;
        internal static int CORRECT_ACTION_COLUMN = 3;
        internal static string SKIP_DATA = null;
        internal string _cacheDomain = "";
        internal string _cacheUserName = "";
        internal string _cachePassWord = "";

        internal int _credentialCheckBoxIndex;
        internal bool _reRunCredentials = false;
        internal bool _wmiMessageFormCancel = false;
        internal static string _logMessage;
        internal bool _verifiedTags = false;
        internal bool _skipRecoveryChecks = false;

        internal static int LATEST_TAG_RECOVERY = 1;
        internal static int LATEST_TIME_RECOVERY = 2;
        internal static int SPECIFIC_TAG_AT_TIME_RECOVERY = 3;
        internal static int SPECIFIC_TIME_RECOVERY = 4;
        public static int SPECIFIC_TAG = 5;
        internal string _message = null;
        internal int _rowIndex = 0;
        internal System.Drawing.Bitmap _passed;
        internal System.Drawing.Bitmap _failed;
        private delegate void TickerDelegate(string s);
        internal string _installPath = null;
        TickerDelegate tickerDelegate;
        TextBox _textBox = new TextBox();
        Button _closeButton = new Button();
        Label _label = new Label();
        internal bool _getInfoScriptCalled = false;
        public Esx_RecoverPanelHandler()
        {
          
            _passed = Wizard.Properties.Resources.tick;
            _failed = Wizard.Properties.Resources.cross;
            _installPath = WinTools.FxAgentPath() + "\\vContinuum";
           
        }
        /*
         * After user selecting some vms for recovery and click next we will get this scren first.
         * We will get Info.xml for the target vShere/vCenter.
         * And will the selected vms list in the table.
         * And display the tag selection wondow fro user to select tags.
         */
        internal override bool Initialize(AllServersForm recoveryForm)
        {
            recoveryForm.mandatoryLabel.Visible = false;
            recoveryForm.nextButton.Enabled = false;
            recoveryForm.specificTimeRadioButton.Location = new Point(10, 89);
            recoveryForm.specificTagRadioButton.Location = new Point(10, 112);
            recoveryForm.previousButton.Visible = false;
            recoveryForm.recoveryChecksPassedList = new HostList();
            _textBox = recoveryForm.recoveryText;
            if (recoveryForm.appNameSelected == AppName.Drdrill)
            {
                recoveryForm.drdrillOptionPanel.Visible = true;
                recoveryForm.recoveryTagLabels.Text = "Snapshot based on:";
                recoveryForm.recoveryPlanNameLabel.Text = "DR Drill Planname *";
            }
            if (ResumeForm.selectedVMListForPlan._hostList.Count != 0 && ResumeForm.masterTargetList._hostList.Count != 0)
            {
                //modifiied by srikanth
                Host tempHost = (Host)ResumeForm.selectedVMListForPlan._hostList[0];
                //Host tempHost1 = (Host)ResumeForm._masterList._hostList[0];
                /*Trace.WriteLine(DateTime.Now + "\t\t temphost target esx ip:" + tempHost.targetESXIp);
                Trace.WriteLine(DateTime.Now + "\t\t temphost target username :" + tempHost.targetESXUserName);
                Trace.WriteLine(DateTime.Now + "\t\t temphost target password :" + tempHost.passWord);

                Trace.WriteLine(DateTime.Now + "\t\t temphost1 target esx ip:" + tempHost1.esxIp);
                Trace.WriteLine(DateTime.Now + "\t\t temphost1 target username ip:" + tempHost1.esxUserName);
                Trace.WriteLine(DateTime.Now + "\t\t temphost1 target password ip:" + tempHost1.esxPassword);

                Trace.WriteLine(DateTime.Now + "\t\t recoverymasterhost target esx ip:" + recoveryForm._masterHost.esxIp);
                Trace.WriteLine(DateTime.Now + "\t\t recoverymsterhost target username ip:" + recoveryForm._masterHost.esxUserName);
                Trace.WriteLine(DateTime.Now + "\t\t recoverymasterhost target password ip:" + recoveryForm._masterHost.esxPassword);*/

               // return false;
                //modified by srikanth
                recoveryForm.esxIPSelected = tempHost.targetESXIp;
                

                recoveryForm.tgtEsxUserName = tempHost.targetESXUserName;
                recoveryForm.osTypeSelected = tempHost.os;
                recoveryForm.tgtEsxPassword = tempHost.targetESXPassword;
                

                recoveryForm.sourceListByUser = new HostList();
                recoveryForm.sourceListByUser = ResumeForm.selectedVMListForPlan;
                if (recoveryForm.appNameSelected == AppName.Drdrill)
                {
                    foreach (Host h in recoveryForm.sourceListByUser._hostList)
                    {
                        h.targetDataStore = null;
                    }
                }
                recoveryForm.masterHostAdded = (Host)ResumeForm.masterTargetList._hostList[0];
                if (tempHost.targetvSphereHostName != null)
                {
                    recoveryForm.masterHostAdded.vSpherehost = tempHost.targetvSphereHostName;
                }
                recoveryForm.recoveryTargetESXIPTextBox.Visible = false;
                recoveryForm.targetEsxUserNameTextBox.Visible = false;
                recoveryForm.targetEsxPasswordTextBox.Visible = false;
                recoveryForm.teargetEsxIPLabel.Visible = false;
                recoveryForm.recoveryOsComboBox.Visible = false;
                recoveryForm.recoveryOstypeLabel.Visible = false;
                recoveryForm.targetEsxIPHeadingLabel.Visible = false;
                recoveryForm.targetEsxUserNameLabel.Visible = false;
                recoveryForm.targetEsxPasswordLabel.Visible = false;
                recoveryForm.recoveryGetDetailsButton.Visible = false;

                int rowIndex = 0;
                recoveryForm.recoverDataGridView.Rows.Clear();
                Trace.WriteLine(DateTime.Now + " \t Printing the vm list to recover " + recoveryForm.sourceListByUser._hostList.Count.ToString());
                foreach (Host h in recoveryForm.sourceListByUser._hostList)
                {

                    h.tag = "LATEST";
                    h.recoveryOperation = 1;

                    h.tagType = "FS";
                    
                    recoveryForm.recoverDataGridView.Rows.Add(1);
                    recoveryForm.recoverDataGridView.Rows[rowIndex].Cells[Esx_RecoverPanelHandler.HOSTNAME_COLUMN].Value = h.hostname;
                    recoveryForm.recoverDataGridView.Rows[rowIndex].Cells[Esx_RecoverPanelHandler.DISPLAY_NAME_COLUMN].Value = h.displayname;
                    recoveryForm.recoverDataGridView.Rows[rowIndex].Cells[Esx_RecoverPanelHandler.MASTERTARGET_DISPLAYNAME].Value = h.masterTargetDisplayName;
                    recoveryForm.recoverDataGridView.Rows[rowIndex].Cells[Esx_RecoverPanelHandler.RECOVER_CHECK_BOX_COLUMN].Value = true;
                    rowIndex++;

                }
                Host tempHost1 = (Host)recoveryForm.sourceListByUser._hostList[0];
               



                recoveryForm.recoverDataGridView.Visible = true;
                recoveryForm.tagPanel.Visible = true;

               
                    recoveryForm.recoveryPreReqsButton.Visible = true;
                    try
                    {
                        if (File.Exists(_installPath + "\\Latest\\Info.xml"))
                        {
                            File.Delete(_installPath + "\\Latest\\Info.xml");
                        }
                    }
                    catch (Exception ex)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Failed to delete Info.xml " + ex.Message);
                    }
                    if (tempHost.machineType == "PhysicalMachine" && tempHost.failback == "yes")
                    {
                        AllServersForm.v2pRecoverySelected = true;
                        recoveryForm.progressLabel.Visible = false;
                    }
                    else
                    {
                        AllServersForm.v2pRecoverySelected = false;
                        bool needtoGetInfoOrNot = false;

                        foreach (Host sourceHost in recoveryForm.sourceListByUser._hostList)
                        {
                            foreach (Hashtable sourceHash in sourceHost.nicList.list)
                            {
                                if (sourceHash["new_macaddress"] == null)
                                {
                                    needtoGetInfoOrNot = true;
                                    
                                }
                            }
                        }

                        if (recoveryForm.masterHostAdded.networkNameList.Count != 0)
                        {
                            bool vSpherenamenull = false;
                            foreach (Network net in recoveryForm.masterHostAdded.networkNameList)
                            {
                                if (net.Vspherehostname == null)
                                {
                                    vSpherenamenull = true;
                                }
                            }
                            if (vSpherenamenull == true)
                            {
                                recoveryForm.masterHostAdded.networkNameList = new ArrayList();
                            }
                            vSpherenamenull = false;
                            foreach (Configurations config in recoveryForm.masterHostAdded.configurationList)
                            {
                                if (config.vSpherehostname == null)
                                {
                                    vSpherenamenull = true;
                                }

                            }
                            if (vSpherenamenull == true)
                            {
                                recoveryForm.masterHostAdded.configurationList = new ArrayList();
                                recoveryForm.masterHostAdded.networkNameList = new ArrayList();
                            }
                        }
                        if (needtoGetInfoOrNot == true || recoveryForm.appNameSelected == AppName.Drdrill || recoveryForm.masterHostAdded.networkNameList.Count == 0)
                        {
                            AllServersForm.v2pRecoverySelected = false;
                            recoveryForm.progressBar.Visible = true;
                            recoveryForm.recoverPanel.Enabled = false;
                            _getInfoScriptCalled = true;
                            if (!recoveryForm.recoveryForMasterXmlBackGroundWorker.IsBusy)
                            {
                                recoveryForm.progressLabel.Visible = true;
                                recoveryForm.recoveryForMasterXmlBackGroundWorker.RunWorkerAsync();
                            }
                        }
                        else
                        {
                            if (!recoveryForm.validateBackgroundWorker.IsBusy)
                            {
                                recoveryForm.progressLabel.Text = "Validating target vSphere server Credentials. Please wait for few minutes....";
                                recoveryForm.recoverPanel.Enabled = false;
                                recoveryForm.progressLabel.Visible = true;
                                recoveryForm.progressBar.Visible = true;
                                recoveryForm.validateBackgroundWorker.RunWorkerAsync();
                            }
                            
                        }
                    }
                    
            }
            else
            {
                recoveryForm.progressLabel.Visible = false;
            }

           

            foreach (Host h in ResumeForm.selectedVMListForPlan._hostList)
            {
                Trace.WriteLine(DateTime.Now + "\t Printing the vCOn name " + h.vConName);
            }

              recoveryForm.helpContentLabel.Text = HelpForcx.RecoveryScreen1;
            return true;
        }


        


        internal bool ValidateCredentials(AllServersForm recoveryForm)
        {
            try
            {
                Cxapicalls cxAPi = new Cxapicalls();
                int returnValue = 0;
                if (recoveryForm.VerfiyEsxCreds(recoveryForm.esxIPSelected, "target") == false)
                {
                    Trace.WriteLine(DateTime.Now + "\t failed to Update esx meta data ");
                    return false;
                }
                returnValue = recoveryForm.esxSelected.ValidateEsxCredentails(recoveryForm.esxIPSelected);
                if (returnValue == 3)
                {
                    SourceEsxDetailsPopUpForm sourceEsx = new SourceEsxDetailsPopUpForm();
                    sourceEsx.ipLabel.Text = "Target vCenter/vSphere IP/Name *";
                    sourceEsx.sourceEsxIpText.ReadOnly = false;
                    sourceEsx.sourceEsxIpText.Text = recoveryForm.esxIPSelected;
                    sourceEsx.ShowDialog();
                    if (sourceEsx.canceled == false)
                    {
                        recoveryForm.esxIPSelected = sourceEsx.sourceEsxIpText.Text;
                        recoveryForm.tgtEsxUserName = sourceEsx.userNameText.Text;
                        recoveryForm.tgtEsxPassword = sourceEsx.passWordText.Text;
                        recoveryForm.masterHostAdded.esxIp = sourceEsx.sourceEsxIpText.Text;
                        recoveryForm.masterHostAdded.esxUserName = sourceEsx.userNameText.Text;
                        recoveryForm.masterHostAdded.esxPassword = sourceEsx.passWordText.Text;
                        cxAPi. UpdaterCredentialsToCX(recoveryForm.esxIPSelected, recoveryForm.tgtEsxUserName, recoveryForm.tgtEsxPassword);
                        returnValue = recoveryForm.esxSelected.ValidateEsxCredentails(recoveryForm.esxIPSelected);

                        if (returnValue == 3)
                        {
                            MessageBox.Show("You have entered wrong credentials. Failed to fetch information from target vSphere client. Can't proceed further."+ Environment.NewLine+" Please close vContinuum wizard and launch again.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            recoveryForm.recoveryPreReqsButton.Enabled = false;
                            return false;
                        }
                        else if(returnValue != 0)
                        {
                            MessageBox.Show("Failed to get the information from target vSphere client. Please close vContinuum wizard and launch again", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            recoveryForm.recoveryPreReqsButton.Enabled = false;
                            return false;
                        }

                        foreach (Host h in recoveryForm.sourceListByUser._hostList)
                        {
                            h.targetESXIp = recoveryForm.esxIPSelected;
                            h.targetESXUserName = recoveryForm.tgtEsxUserName;
                            h.targetESXPassword = recoveryForm.tgtEsxPassword;
                        }
                    }
                }

                foreach (Host h in recoveryForm.sourceListByUser._hostList)
                {
                    if (h.cluster == "yes")
                    {
                        Host sourceHost = new Host();
                        sourceHost.hostname = h.hostname;
                        sourceHost.ip = h.ip;
                        sourceHost.inmage_hostid = h.inmage_hostid;
                        sourceHost.displayname = h.displayname;

                        if (sourceHost.inmage_hostid == null)
                        {
                            Host tempHost = h;
                            WinPreReqs win = new WinPreReqs(sourceHost.ip, "", "", "");
                            string cxIp = WinTools.GetcxIPWithPortNumber();
                            if (h.hostname != null)
                            {
                                win.SetHostIPinputhostname(sourceHost.hostname,  sourceHost.ip, cxIp);
                                sourceHost.ip = WinPreReqs.GetIPaddress;
                            }
                            win.GetDetailsFromcxcli( sourceHost, cxIp);
                            sourceHost = WinPreReqs.GetHost;
                            h.inmage_hostid = tempHost.inmage_hostid;
                        }
                        foreach (Hashtable source in h.nicList.list)
                        {
                            if (source["virtual_ip"] == null)
                            {
                                
                                if (cxAPi.Post( sourceHost, "GetHostInfo") == true)
                                {
                                    sourceHost = Cxapicalls.GetHost;
                                    CreateVirtualNicInHash(recoveryForm, ref sourceHost);
                                    foreach (Hashtable hash in h.nicList.list)
                                    {
                                        foreach (Hashtable sourceHash in sourceHost.nicList.list)
                                        {
                                            if (hash["nic_mac"] != null && sourceHash["nic_mac"] != null)
                                            {
                                                if (hash["nic_mac"].ToString() == sourceHash["nic_mac"].ToString())
                                                {
                                                    if (hash["virtual_ips"] == null && sourceHash["virtual_ips"] != null)
                                                    {
                                                        hash["virtual_ips"] = sourceHash["virtual_ips"];
                                                        try
                                                        {
                                                            Trace.WriteLine(DateTime.Now + "\t Printing the virtual ips " + hash["virtual_ips"].ToString());
                                                        }
                                                        catch (Exception ex)
                                                        {
                                                            Trace.WriteLine(DateTime.Now + "\t Printing virtual ips failed " + ex.Message);
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

            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: ValidateCredentials of ESX_RecoveryPanelHandler.cs " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            return true;
        }

        /*
         * Initailize will call this method.
         * This will get the Info.xml. If it fail to connect with crentials already having in MasterConFigFile.xml
         * We will ask user to re-enter to get info.
         * If it fail again we will dislaly failure message box to user.
         * If it fails with return code 255 we will re-try once agin with same credentails.
         */

        internal bool RunMasterScriptToGetMaSterXMl(AllServersForm recoveryForm)
        {
            int returnValue = 0;
            if (recoveryForm.masterHostAdded.networkNameList.Count != 0)
            {
                bool vSpherenamenull = false;
                foreach (Network net in recoveryForm.masterHostAdded.networkNameList)
                {
                    if (net.Vspherehostname == null)
                    {
                        vSpherenamenull = true;
                    }
                }
                if (vSpherenamenull == true)
                {
                    recoveryForm.masterHostAdded.networkNameList = new ArrayList();
                }
                vSpherenamenull = false;
                foreach (Configurations config in recoveryForm.masterHostAdded.configurationList)
                {
                    if (config.vSpherehostname == null)
                    {
                        vSpherenamenull = true;
                    }
                    
                }
                if (vSpherenamenull == true)
                {
                    recoveryForm.masterHostAdded.configurationList = new ArrayList();
                    recoveryForm.masterHostAdded.networkNameList = new ArrayList();
                }
            }
            recoveryForm.VerfiyEsxCreds(recoveryForm.esxIPSelected, "target");
            if (recoveryForm.appNameSelected == AppName.Drdrill || recoveryForm.masterHostAdded.vCenterProtection == "No" || recoveryForm.masterHostAdded.networkNameList.Count == 0)
            {
                returnValue = recoveryForm.esxSelected.GetEsxInfoWithVmdksForAllOS(recoveryForm.esxIPSelected, Role.Secondary);

            }
            else
            {
                returnValue = recoveryForm.esxSelected.GetEsxInfoWithVmdksWithOptionsWithoutOS(recoveryForm.esxIPSelected, Role.Secondary, "hosts");
            }
            if (returnValue == 3)
            {
                SourceEsxDetailsPopUpForm sourceEsx = new SourceEsxDetailsPopUpForm();
                sourceEsx.ipLabel.Text = "Target vCenter/vSphere IP/Name *";
                sourceEsx.sourceEsxIpText.ReadOnly = false;
                sourceEsx.sourceEsxIpText.Text = recoveryForm.esxIPSelected;
                sourceEsx.ShowDialog();
                recoveryForm.esxIPSelected = sourceEsx.sourceEsxIpText.Text;
                recoveryForm.tgtEsxUserName = sourceEsx.userNameText.Text;
                recoveryForm.tgtEsxPassword = sourceEsx.passWordText.Text;
                recoveryForm.masterHostAdded.esxIp = sourceEsx.sourceEsxIpText.Text;
                recoveryForm.masterHostAdded.esxUserName = sourceEsx.userNameText.Text;
                recoveryForm.masterHostAdded.esxPassword = sourceEsx.passWordText.Text;
                recoveryForm.VerfiyEsxCreds(recoveryForm.esxIPSelected, "target");
                Cxapicalls cxapi = new Cxapicalls();
                cxapi. UpdaterCredentialsToCX(recoveryForm.masterHostAdded.esxIp, recoveryForm.masterHostAdded.esxUserName, recoveryForm.masterHostAdded.esxPassword);
                if (recoveryForm.appNameSelected == AppName.Drdrill || recoveryForm.masterHostAdded.vCenterProtection == "No" || recoveryForm.masterHostAdded.networkNameList.Count == 0)
                {
                    returnValue = recoveryForm.esxSelected.GetEsxInfoWithVmdksForAllOS(recoveryForm.esxIPSelected, Role.Secondary);

                }
                else
                {
                    returnValue = recoveryForm.esxSelected.GetEsxInfoWithVmdksWithOptionsWithoutOS(recoveryForm.esxIPSelected, Role.Secondary, "hosts");
                }
                if (returnValue != 0)
                {
                    MessageBox.Show("Failed to get the information form target vSphere client", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return false;
                }
                foreach (Host h in recoveryForm.sourceListByUser._hostList)
                {
                    h.targetESXIp = recoveryForm.esxIPSelected;
                    h.targetESXUserName = recoveryForm.tgtEsxUserName;
                    h.targetESXPassword = recoveryForm.tgtEsxPassword;
                }
            }
            else if (returnValue == 255)
            {
                Trace.WriteLine(DateTime.Now + "\t\t Getinfo call failed with error code with 255.It is possibily the connection time out. Hence retrying the getinfo ");
                if (recoveryForm.VerfiyEsxCreds(recoveryForm.esxIPSelected, "target") == false)
                {
                    Trace.WriteLine(DateTime.Now + "\t Failed to update esx meta data to cx");
                }
                if (recoveryForm.appNameSelected == AppName.Drdrill || recoveryForm.masterHostAdded.vCenterProtection == "No" || recoveryForm.masterHostAdded.networkNameList.Count == 0)
                {
                    returnValue = recoveryForm.esxSelected.GetEsxInfoWithVmdksForAllOS(recoveryForm.esxIPSelected, Role.Secondary);

                }
                else
                {
                    returnValue = recoveryForm.esxSelected.GetEsxInfoWithVmdksWithOptionsWithoutOS(recoveryForm.esxIPSelected, Role.Secondary, "hosts");
                }

            }
            else if (returnValue != 0)
            {
                MessageBox.Show("Failed to get the information form target vSphere client", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return false;
            }
            if (returnValue != 0)
            {
                MessageBox.Show("Failed to get the information form target vSphere client", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return false;

            }
            if (returnValue == 0)
            {
                foreach (Host h in recoveryForm.esxSelected.GetHostList)
                {
                    Trace.WriteLine(DateTime.Now + "\t Found os type as null in Host " + h.displayname);
                    h.os = recoveryForm.osTypeSelected;
                }
            }
            foreach (Host h in recoveryForm.sourceListByUser._hostList)
            {
                if (h.cluster == "yes")
                {
                    Host sourceHost = new Host();
                    sourceHost.hostname = h.hostname;
                    sourceHost.ip = h.ip;
                    sourceHost.inmage_hostid = h.inmage_hostid;
                    sourceHost.displayname = h.displayname;

                    if (sourceHost.inmage_hostid == null)
                    {
                        Host tempHost = h;
                        WinPreReqs win = new WinPreReqs(sourceHost.ip, "", "", "");
                        string cxIp = WinTools.GetcxIPWithPortNumber();
                        if (h.hostname != null)
                        {
                            win.SetHostIPinputhostname(sourceHost.hostname,  sourceHost.ip, cxIp);
                            sourceHost.ip = WinPreReqs.GetIPaddress;
                        }
                        win.GetDetailsFromcxcli( sourceHost, cxIp);
                        sourceHost = WinPreReqs.GetHost;
                        h.inmage_hostid = tempHost.inmage_hostid;
                    }
                    foreach (Hashtable source in h.nicList.list)
                    {
                        if (source["virtual_ip"] == null)
                        {
                            Cxapicalls cxAPi = new Cxapicalls();
                            if (cxAPi.Post( sourceHost, "GetHostInfo") == true)
                            {
                                sourceHost = Cxapicalls.GetHost;
                                CreateVirtualNicInHash(recoveryForm, ref sourceHost);
                                foreach (Hashtable hash in h.nicList.list)
                                {
                                    foreach (Hashtable sourceHash in sourceHost.nicList.list)
                                    {
                                        if (hash["nic_mac"] != null && sourceHash["nic_mac"] != null)
                                        {
                                            if (hash["nic_mac"].ToString() == sourceHash["nic_mac"].ToString())
                                            {
                                                if (hash["virtual_ips"] == null && sourceHash["virtual_ips"] != null)
                                                {
                                                    hash["virtual_ips"] = sourceHash["virtual_ips"];
                                                    try
                                                    {
                                                        Trace.WriteLine(DateTime.Now + "\t Printing the virtual ips " + hash["virtual_ips"].ToString());
                                                    }
                                                    catch (Exception ex)
                                                    {
                                                        Trace.WriteLine(DateTime.Now + "\t Printing virtual ips failed " + ex.Message);
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

            return true;
        }

        private bool CreateVirtualNicInHash(AllServersForm allserversFrom, ref  Host h)
        {
            try
            {
                if (h.cluster == "yes")
                {
                    foreach (Hashtable nicHash in h.nicList.list)
                    {
                        if (nicHash["virtual_ips"] == null)
                        {
                            if (nicHash["iptype"] != null && nicHash["ip"] != null)
                            {
                                string[] ipTypes = nicHash["iptype"].ToString().Split(',');
                                string[] ips = nicHash["ip"].ToString().Split(',');

                                if (ips.Length == ipTypes.Length)
                                {
                                    int i = 0;
                                    string virtual_ips = null;
                                    foreach (string s in ipTypes)
                                    {

                                        if (s != "physical")
                                        {
                                            Trace.WriteLine(DateTime.Now + "\t Not a Physical ip " + s);

                                            if (virtual_ips == null)
                                            {
                                                virtual_ips = ips[i];
                                            }
                                            else
                                            {
                                                virtual_ips = virtual_ips + "," + ips[i];
                                            }

                                        }
                                        i++;

                                    }
                                    if (virtual_ips != null)
                                    {
                                        nicHash["virtual_ips"] = virtual_ips;
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
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: CreateVirtualNicInHash of ESX_RecoveryPanelHandler.cs " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            return true;
        }



        /*
        * At the time of recovery we are checking for the vContinuum_ESX.log.
        * If that log is not null we will read and store that data into the SKIP_DATA string and make the warning label as visibile true
        * once user clicks on linklabel we will show that data in message box.
        */
        internal bool ShowSkipData(AllServersForm allServersForm)
        {
            try
            {
                if (SKIP_DATA != null)
                {
                    MessageBox.Show(SKIP_DATA, "Skipped some of the Data", MessageBoxButtons.OK, MessageBoxIcon.Information);
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

        /*
         * In this method we will check for the ostype.
         * If winodos we will add all the selected vms into_recovertyCheckedList directly.
         * If linux we will check wheter mt is having any jobs running or not.
         * If so we will block the user.
         * We will check whether mt exists on targt on not also.
         */
        internal override bool ProcessPanel(AllServersForm recoveryForm)
        {
            try
            {

                if (recoveryForm.arrayBasedDrDrillRadioButton.Checked == true)
                {
                    AllServersForm.arrayBasedDrdrillSelected = true;
                }
                else
                {
                    AllServersForm.arrayBasedDrdrillSelected = false;
                }

                if (recoveryForm.vmsSelectedRecovery._hostList.Count == 0)
                {
                    MessageBox.Show("Please select VMs to recover", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return false;
                }
                else
                {
                    int listIndex = 0;
                    Host tempHost = new Host();
                    try
                    {
                        if (recoveryForm.recoveryChecksPassedList._hostList.Count != 0)
                        {
                            if (recoveryForm.vmsSelectedRecovery._hostList.Count != 0)
                            {
                                foreach (Host h in recoveryForm.vmsSelectedRecovery._hostList)
                                {
                                    foreach (Host h1 in recoveryForm.recoveryChecksPassedList._hostList)
                                    {
                                        if (h1.displayname == h.displayname || h.hostname == h1.hostname)
                                        {
                                            if (h1.targetDataStore != null)
                                            {
                                                h.targetDataStore = h1.targetDataStore;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    catch (Exception ex)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Faile dto assign datastroe values " + ex.Message);
                    }
                    recoveryForm.recoveryChecksPassedList = new HostList();
                    for (int i = 0; i < recoveryForm.recoverDataGridView.RowCount; i++)
                    {
                        tempHost.hostname = recoveryForm.recoverDataGridView.Rows[i].Cells[HOSTNAME_COLUMN].Value.ToString();
                        tempHost.displayname = recoveryForm.recoverDataGridView.Rows[i].Cells[DISPLAY_NAME_COLUMN].Value.ToString();
                        if ((bool)recoveryForm.recoverDataGridView.Rows[i].Cells[RECOVER_CHECK_BOX_COLUMN].FormattedValue)
                        {
                            if (recoveryForm.vmsSelectedRecovery.DoesHostExist(tempHost, ref listIndex) == true)
                            {
                                foreach (Host h1 in recoveryForm.sourceListByUser._hostList)
                                {
                                    foreach (Host h in recoveryForm.vmsSelectedRecovery._hostList)
                                    {
                                        if (h1.hostname == tempHost.hostname)
                                        {
                                            if (h.hostname == tempHost.hostname)
                                            {
                                                h.ip = h1.ip;
                                                Trace.WriteLine(DateTime.Now + " \t Prinitng the vcon name " + h.vConName);
                                                if (h.recoveryPrereqsPassed == true)
                                                {
                                                    recoveryForm.recoveryChecksPassedList.AddOrReplaceHost(h);
                                                }
                                                else
                                                {
                                                    // MessageBox.Show("The selected VMs has to pass readiness check", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                                    // return false;
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                            else
                            {
                                MessageBox.Show("Selected VMs have not passed readiness checks", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                recoveryForm.recoveryChecksPassedList.RemoveHost(tempHost);
                                return false;
                            }
                        }
                    }
                }
                if (recoveryForm.recoveryChecksPassedList._hostList.Count == 0)
                {
                    MessageBox.Show("Please select VMs to recover", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return false;
                }


                if (File.Exists(recoveryForm.installationPath + "\\" + recoveryForm.RecoveryHostfile))
                {
                    File.Delete(recoveryForm.installationPath + "\\" + recoveryForm.RecoveryHostfile);
                }
                
                if (recoveryForm.osTypeSelected == OStype.Windows)
                {
                    foreach (Host h in recoveryForm.recoveryChecksPassedList._hostList)
                    {
                        if (h.newIP != null && h.newIP.Length < 4)
                        {
                            h.newIP = null;
                        }
                        if (h.defaultGateWay != null && h.defaultGateWay.Length < 4)
                        {
                            h.defaultGateWay = null;
                        }
                        if (h.dns != null && h.dns.Length < 4)
                        {
                            h.dns = null;
                        }
                        if (h.subNetMask != null && h.subNetMask.Length < 4)
                        {
                            h.subNetMask = null;
                        }
                        if ((h.newIP != null && h.newIP != h.ip) && h.subNetMask == null)
                        {
                            MessageBox.Show("Please enter subnet mask for following VM: " + h.displayname + h.newIP, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            return false;
                        }
                        if (h.subNetMask != null && h.newIP == null)
                        {
                            MessageBox.Show("Please enter new IP for following VM: " + h.displayname, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            return false;
                        }
                        if (h.defaultGateWay != null && (h.newIP == null || h.subNetMask == null))
                        {
                            MessageBox.Show("Please enter new IP: and subbnet mask " + h.displayname, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            return false;
                        }
                    }
                }
                foreach (Host h in recoveryForm.recoveryChecksPassedList._hostList)
                {
                    //  recoveryForm._selectedRecoverList.Print();
                    if (h.ip == null)
                    {
                        h.ip = "NULL";
                    }
                    if (h.subNetMask == null)
                    {
                        h.subNetMask = "NULL";
                    }
                    if (h.defaultGateWay == null)
                    {
                        h.defaultGateWay = "NULL";
                    }
                    if (h.dns == null)
                    {
                        h.dns = "NULL";
                    }

                    if (h.tag == null)
                    {
                        h.tag = "LATEST";
                    }

                    if (h.tagType == null)
                    {
                        h.tagType = "FS";
                    }
                }
                if (recoveryForm.appNameSelected == AppName.Drdrill)
                {
                    if (recoveryForm.hostBasedRadioButton.Checked == true)
                    {
                        foreach (Host h in recoveryForm.recoveryChecksPassedList._hostList)
                        {
                            //In case of Dr-Drill we are not asking user to select lun. so we are 
                            // coverting rdm to vmdk defaultly.......
                            h.convertRdmpDiskToVmdk = true;
                        }
                    }


                    foreach (Host h in recoveryForm.recoveryChecksPassedList._hostList)
                    {
                        foreach (Host mtHost in recoveryForm.esxSelected.GetHostList)
                        {
                            if (recoveryForm.masterHostAdded.source_uuid == mtHost.source_uuid || recoveryForm.masterHostAdded.hostname == mtHost.hostname || recoveryForm.masterHostAdded.displayname == mtHost.displayname)
                            {
                                recoveryForm.masterHostAdded.resourcePool = mtHost.resourcePool;
                                recoveryForm.masterHostAdded.resourcepoolgrpname = mtHost.resourcepoolgrpname;
                            }
                        }
                        h.resourcepoolgrpname = recoveryForm.masterHostAdded.resourcepoolgrpname;
                        h.resourcePool = recoveryForm.masterHostAdded.resourcePool;
                    }
                    Trace.WriteLine(DateTime.Now + "\t Comparing the controller check");
                    ComparingDiskForControllerCheck(recoveryForm);
                    Trace.WriteLine(DateTime.Now + "\t Completed the comaring the controller check");
                }

                if (recoveryForm.appNameSelected == AppName.Drdrill)
                {
                    foreach (Host h in recoveryForm.recoveryChecksPassedList._hostList)
                    {
                        if (recoveryForm.masterHostAdded.hostversion != null && h.vmxversion != null)
                        {
                            if (h.vmxversion.Contains("04") && (recoveryForm.masterHostAdded.hostversion.StartsWith("4.0") || recoveryForm.masterHostAdded.hostversion.StartsWith("4.1") || recoveryForm.masterHostAdded.hostversion.StartsWith("5.0")))
                            {
                                h.vmxversion = "vmx-07";
                            }
                        }
                        else
                        {
                            Trace.WriteLine(DateTime.Now + "\t one of the vms vmx version is null");
                            return false;
                        }

                        if (h.machineType == "PhysicalMachine" && h.operatingSystem == "Windows_2012")
                        {                            
                            h.vmxversion = "vmx-08";
                        }                        
                    }
                }
                

                return true;
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: ProcessPanel " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
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
            return true;
        }

        /*
         * Once User clicks on the Readinesschecks. First we will prepare selectedvms list and then we will come to this method.
         * In this first we will check whether Master target exists on target vSphere/vCenter Server.
         * If not we will block here. We will check for the vm created on target als. 
         * If not we will block user with out continuing.
         * Then we will check the licences of the vCon,MT and CX.
         * If we have crendentails we won't ask user. If not we will ask for credentails.
         * If credentials are wrong we will ask for credentails or user can skip the readiness checks.
         * If everthing wnet fine we will checks for the tags are available or not....
         */
        internal bool SelectedVmMasterTargetCredentials(AllServersForm recoveryForm)
        {
            try
            {
                if (AllServersForm.v2pRecoverySelected == false && _getInfoScriptCalled == true)
                {
                    //Checking for whether the master target and protected source vms are there at target side or vmotioned.
                    bool masterTargetIsThere = false;
                    foreach (Host h in recoveryForm.esxSelected.GetHostList)
                    {//spliting the hostname which is in MasterConfigFile.xml

                        string hostname = recoveryForm.masterHostAdded.hostname.Split('.')[0];
                        Debug.WriteLine(DateTime.Now + "\t Printing the mt hostname " + recoveryForm.masterHostAdded.hostname);
                        string infoHostName = null;
                        if (h.hostname != null)
                        {
                            infoHostName = h.hostname.Split('.')[0];
                        }
                        Debug.WriteLine(DateTime.Now + " \t Printing all the values of the hosts displayname " + h.displayname + "master host " + recoveryForm.masterHostAdded.displayname);
                        Debug.WriteLine(DateTime.Now + " \t Printing all the values of the hosts hosts " + h.hostname + "master host " + recoveryForm.masterHostAdded.hostname);
                        //In this assigning the inof.xml values to the masterhost.
                        if (recoveryForm.masterHostAdded.source_uuid == null)
                        {
                            if (h.displayname == recoveryForm.masterHostAdded.displayname || infoHostName == hostname)
                            {
                                if (h.poweredStatus == "ON")
                                {

                                    Trace.WriteLine(DateTime.Now + "\t Master target is present on target esx box");
                                    //h.sourceIsThere = true;
                                    masterTargetIsThere = true;
                                    recoveryForm.masterHostAdded.sourceIsThere = true;
                                    //h.esxUserName = recoveryForm._masterHost.esxUserName;
                                    //h.esxPassword = recoveryForm._masterHost.esxPassword;
                                    // h.userName = recoveryForm._masterHost.userName;
                                    // h.passWord = recoveryForm._masterHost.passWord;
                                    // h.domain = recoveryForm._masterHost.domain;
                                    // h.machineType = "VirtualMachine";
                                    h.role = Role.Secondary;
                                    recoveryForm.masterHostAdded.machineType = "VirtualMachine";
                                    recoveryForm.masterHostAdded.vSpherehost = h.vSpherehost;
                                    //recoveryForm._masterHost = h;
                                    break;
                                }
                            }
                        }
                        else
                        {
                            if (h.source_uuid == recoveryForm.masterHostAdded.source_uuid)
                            {
                                if (h.poweredStatus == "ON")
                                {
                                    Trace.WriteLine(DateTime.Now + "\t Master target is present on target esx box");
                                    //h.esxUserName = recoveryForm._masterHost.esxUserName;
                                    //h.esxPassword = recoveryForm._masterHost.esxPassword;
                                    //h.userName = recoveryForm._masterHost.userName;
                                    //h.passWord = recoveryForm._masterHost.passWord;
                                    //h.domain = recoveryForm._masterHost.domain;
                                    //h.machineType = "VirtualMachine";
                                    //h.role = Role.Secondary;
                                    //recoveryForm._masterHost = h;
                                    //h.sourceIsThere = true;
                                    recoveryForm.masterHostAdded.sourceIsThere = true;
                                    recoveryForm.masterHostAdded.machineType = "VirtualMachine";
                                    recoveryForm.masterHostAdded.vSpherehost = h.vSpherehost;
                                    masterTargetIsThere = true;
                                    break;
                                }
                            }
                        }
                    }
                    //checking for master target is there or not.
                    if (masterTargetIsThere == false)
                    {
                        string message = "\t\t Please check following on target vCenter/ESXi" + Environment.NewLine +
                                         "\t\t 1. Make sure master target is up and running" + Environment.NewLine +
                                         "\t\t 2. Either vMotioned to other ESX (Applicable only if protection done at ESX level) " + Environment.NewLine +
                                         "\t\t 3. Master target UUID changed. UUID should be: " + recoveryForm.masterHostAdded.source_uuid + Environment.NewLine + Environment.NewLine +
                                         "Please contact customer support for further assistance";

                        MessageBox.Show(message, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        recoveryForm.recoveryPreReqsButton.Enabled = true;
                        return false;
                    }
                    //checking for the source to vms which are protected whether they are in power off or not also
                    foreach (Host h in recoveryForm.vmsSelectedRecovery._hostList)
                    {
                        foreach (Host tempH in recoveryForm.esxSelected.GetHostList)
                        {
                            if (h.target_uuid != null && h.target_uuid.Length != 0)
                            {
                                if (h.target_uuid == tempH.source_uuid && tempH.poweredStatus == "OFF")
                                {
                                    if (recoveryForm.appNameSelected == AppName.Recover)
                                    {
                                        h.new_displayname = tempH.displayname;
                                    }
                                    if (h.alt_guest_name == null)
                                    {
                                        h.alt_guest_name = tempH.alt_guest_name;

                                    }
                                    if (h.vmxversion == null)
                                    {
                                        h.vmxversion = tempH.vmxversion;
                                    }
                                    try
                                    {
                                        if (h.vmxversion.Length == 0)
                                        {
                                            h.vmxversion = tempH.vmxversion;
                                        }
                                    }
                                    catch (Exception ex)
                                    {
                                        h.vmxversion = tempH.vmxversion;
                                        Trace.WriteLine(DateTime.Now + "\t Failed to update vmxversion " + ex.Message);
                                    }
                                    h.targetvSphereHostName = tempH.vSpherehost;
                                    // In case of the null new_macaddress we are checking by network_label
                                    // First we will check both the network label are equal and then we will 
                                    // Check the new_macaddress is null if null we will assign new_macaddress from target side.
                                    foreach (Hashtable nicHash in h.nicList.list)
                                    {
                                        foreach (Hashtable nics in tempH.nicList.list)
                                        {
                                            if (nicHash["network_label"] != null && nics["network_label"] != null)
                                            {
                                                if (nicHash["network_label"].ToString() == nics["network_label"].ToString())
                                                {
                                                    if (nicHash["new_macaddress"] == null)
                                                    {
                                                        Trace.WriteLine(DateTime.Now + "\t came here to assign nicac of target to protected one " + h.displayname);
                                                        nicHash["new_macaddress"] = nics["nic_mac"];
                                                    }
                                                    if (nicHash["new_network_name"] == null)
                                                    {
                                                        nicHash["new_network_name"] = nics["network_name"];
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    h.sourceIsThere = true;
                                    break;
                                }
                            }
                            else
                            {
                                if (h.new_displayname == tempH.displayname && tempH.poweredStatus == "OFF")
                                {
                                    // with out this we willget all uuids same at target side for p2v protexction 
                                    // So when we uprade 5.5 sp1 to 6.0 we will get uuid as empty 
                                    // So we need not witre these values in the recovey.xml so we are not writing in case of p2v.
                                    if (h.machineType != "PhysicalMachine")
                                    {
                                        h.target_uuid = tempH.source_uuid;
                                    }
                                    // In case of the null new_macaddress we are checking by network_label
                                    // First we will check both the network label are equal and then we will 
                                    // Check the new_macaddress is null if null we will assign new_macaddress from target side.
                                    foreach (Hashtable nicHash in h.nicList.list)
                                    {
                                        foreach (Hashtable nics in tempH.nicList.list)
                                        {
                                            if (nicHash["network_label"] != null && nics["network_label"] != null)
                                            {
                                                if (nicHash["network_label"].ToString() == nics["network_label"].ToString())
                                                {
                                                    if (nicHash["new_macaddress"] == null)
                                                    {
                                                        Trace.WriteLine(DateTime.Now + "\t came here to assign nicac of target to protected one " + h.displayname);
                                                        nicHash["new_macaddress"] = nics["nic_mac"];
                                                    }
                                                    if (nicHash["new_network_name"] == null)
                                                    {
                                                        nicHash["new_network_name"] = nics["network_name"];
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    h.sourceIsThere = true;
                                    if (h.alt_guest_name == null)
                                    {
                                        h.alt_guest_name = tempH.alt_guest_name;

                                    }
                                    h.targetvSphereHostName = tempH.vSpherehost;
                                    if (h.vmxversion == null)
                                    {
                                        h.vmxversion = tempH.vmxversion;
                                    }
                                    try
                                    {
                                        if (h.vmxversion.Length == 0)
                                        {
                                            h.vmxversion = tempH.vmxversion;
                                        }
                                    }
                                    catch (Exception ex)
                                    {
                                        h.vmxversion = tempH.vmxversion;
                                        Trace.WriteLine(DateTime.Now + "\t Failed to update vmxversion " + ex.Message);
                                    }
                                    // this is to fix the upgrade issue from 5.5 sp1 to 6.2..
                                    break;
                                }
                            }
                        }
                    }
                    foreach (Host h in recoveryForm.vmsSelectedRecovery._hostList)
                    {

                        if (h.sourceIsThere == false)
                        {
                            Trace.WriteLine(DateTime.Now + "\t VM not found with uuid. So going for vmdk comparision");
                            VerifyingWithVmdks(recoveryForm);
                        }
                    }

                    //if it is dr drill operation, get the sizes from info.xml instead.This is required to support any kind of volumes resies
                    //if customer has done it manually.

                    if (recoveryForm.appNameSelected == AppName.Drdrill)
                    {
                        try
                        {
                            foreach (Host h in recoveryForm.vmsSelectedRecovery._hostList)
                            {

                                foreach (Host getinfoHost in recoveryForm.esxSelected.GetHostList)
                                {

                                    if (getinfoHost.poweredStatus == "OFF" && h.target_uuid == getinfoHost.source_uuid)
                                    {
                                        foreach (Hashtable recoveryDiskHash in h.disks.list)
                                        {
                                            foreach (Hashtable getInfoDiskHash in getinfoHost.disks.list)
                                            {
                                                if (recoveryDiskHash["target_disk_uuid"] != null && getInfoDiskHash["source_disk_uuid"] != null)
                                                {
                                                    if (recoveryDiskHash["target_disk_uuid"].ToString() == getInfoDiskHash["source_disk_uuid"].ToString())
                                                    {
                                                        if (getInfoDiskHash["Size"] != null)
                                                        {
                                                            try
                                                            {
                                                                Trace.WriteLine(DateTime.Now + "\t Updating the disk sizes in case of dr drill " + getInfoDiskHash["Size"].ToString() + " for the disk " + recoveryDiskHash["Name"].ToString());
                                                            }
                                                            catch (Exception ex)
                                                            {
                                                                Trace.WriteLine(DateTime.Now + "\t Failed to print the sizes adn names " + ex.Message);
                                                            }
                                                            recoveryDiskHash["Size"] = getInfoDiskHash["Size"].ToString();
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
                            System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                            Trace.WriteLine("ERROR*******************************************");
                            Trace.WriteLine(DateTime.Now + "Caught exception at Method: SelectedVmMasterTargetCredentials " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                            Trace.WriteLine("Exception  =  " + ex.Message);
                            Trace.WriteLine("ERROR___________________________________________");

                        }

                    }




                    foreach (Host h1 in recoveryForm.esxSelected.GetHostList)
                    {
                        if (h1.vCenterProtection == "No")
                        {
                            foreach (Host h in recoveryForm.vmsSelectedRecovery._hostList)
                            {

                                if (h.sourceIsThere == false)
                                {
                                    Trace.WriteLine(DateTime.Now + "VM: " + h.displayname + " is not available on secondary server ");
                                    MessageBox.Show("VM: " + h.displayname + " is not available on secondary server ", "VM not found on target", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                    recoveryForm.recoveryPreReqsButton.Enabled = true;

                                    return false;
                                }
                            }
                        }
                        else
                        {
                            foreach (Host h in recoveryForm.vmsSelectedRecovery._hostList)
                            {

                                if (h.sourceIsThere == false)
                                {
                                    Trace.WriteLine(DateTime.Now + "VM: " + h.displayname + " is not available on secondary server ");
                                    //MessageBox.Show("VM: " + h.displayname + " is not available on secondary server ", "VM not found on target", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                    recoveryForm.recoveryPreReqsButton.Enabled = true;

                                    //return false;
                                }
                            }
                        }
                    }

                    //if (recoveryForm._masterHost.hostname.Split('.')[0].ToUpper() != Environment.MachineName)
                    //{
                    //    if (recoveryForm._masterHost.userName == null)
                    //    {
                    //        AddCredentialsPopUpForm addCredentialsForm = new AddCredentialsPopUpForm();
                    //        addCredentialsForm.credentialsHeadingLabel.Text = "Provide credentials for master target";
                    //        addCredentialsForm.domainText.Text = _cacheDomain;
                    //        addCredentialsForm.userNameText.Text = _cacheUserName;
                    //        addCredentialsForm.passWordText.Text = _cachePassWord;
                    //        addCredentialsForm.useForAllCheckBox.Visible = false;
                    //        addCredentialsForm.ShowDialog();
                    //        recoveryForm.getDetailsButton.Enabled = false;
                    //        if (addCredentialsForm.canceled == false)
                    //        {
                    //            recoveryForm._masterHost.domain = addCredentialsForm.domain;
                    //            recoveryForm._masterHost.userName = addCredentialsForm.userName;
                    //            recoveryForm._masterHost.passWord = addCredentialsForm.passWord;
                    //            _cacheDomain = addCredentialsForm.domain;
                    //            _cacheUserName = addCredentialsForm.userName;
                    //            _cachePassWord = addCredentialsForm.passWord;
                    //        }
                    //        else
                    //        {
                    //            recoveryForm.getDetailsButton.Enabled = true;
                    //            recoveryForm.recoveryPreReqsButton.Enabled = true;
                    //            return false;
                    //        }
                    //    }
                    //}
                }
                recoveryForm.progressBar.Visible = true;
                recoveryForm.Refresh();
                recoveryForm.Update();
                WinPreReqs winPrereqs = new WinPreReqs("", "", "", "");
                recoveryForm.masterHostAdded.ip = null;
                //MessageBox.Show("host name "+ recoveryForm._masterHost.hostname);
                Trace.WriteLine(DateTime.Now + "\t MT hostname before get ip " + recoveryForm.masterHostAdded.hostname);
                winPrereqs.SetHostIPinputhostname(recoveryForm.masterHostAdded.hostname,  recoveryForm.masterHostAdded.ip, WinTools.GetcxIPWithPortNumber());
                recoveryForm.masterHostAdded.ip = WinPreReqs.GetIPaddress;
                //MessageBox.Show("   " + recoveryForm._masterHost.ip);
                recoveryForm.recoveryTabControl.Visible = true;
                if (recoveryForm.masterHostAdded.ip == null)
                {
                    Trace.WriteLine("Unable to determine Master target's ip address");
                    MessageBox.Show("Could not retrieve information of master target " + recoveryForm.masterHostAdded.hostname + "( " + recoveryForm.masterHostAdded.displayname + " )" + " from the CX: " + WinTools.GetcxIPWithPortNumber() +
                                           Environment.NewLine + "Please make sure that machine running this wizard is pointing to the same CX used by the master target" +
                                            Environment.NewLine + Environment.NewLine + "To point to correct CX go to Programs->InMage Systems->vContinuum->Agent Congiuration" +
                                            Environment.NewLine +"Check whether proxy server configured in the web browser (IE&Firefox).", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    recoveryForm.progressBar.Visible = false;
                    recoveryForm.getDetailsButton.Enabled = true;
                    recoveryForm.recoveryPreReqsButton.Enabled = true;
                    return false;
                }
                recoveryForm.recoveryText.AppendText("Checking for consistency points to recover. This may take few minutes..." + Environment.NewLine);
                recoveryForm.clearLogLinkLabel.Visible = true;
                recoveryForm.recoveryText.Visible = true;
                recoveryForm.Refresh();
                Trace.WriteLine(DateTime.Now + "\t Checking whether vContinuum wizard has fx agent installed and licensed or not");
                if (CheckForLicenses(recoveryForm) == true)
                {
                    /*if (CheckForMasterFxJobs(recoveryForm) == false)
                    {
                        if (CheckForMasterFxJobs(recoveryForm) == false)
                        {
                            string message = recoveryForm._masterHost.displayname + " has active Fx jobs " + Environment.NewLine
                          + "Please wait for few minutes, until they are complete, and retry." + Environment.NewLine;
                          MessageBox.Show(message, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                          recoveryForm.progressBar.Visible = false;
                          recoveryForm.getDetailsButton.Enabled = true;
                          recoveryForm.recoveryPreReqsButton.Enabled = true;
                          return false;
                        }
                    }*/
                    Cxapicalls cxApis = new Cxapicalls();
                    cxApis.PostTOFindAnyvoluesAreResized(recoveryForm.vmsSelectedRecovery, recoveryForm.masterHostAdded);
                    string vmsList = null;
                    foreach (Host h in recoveryForm.vmsSelectedRecovery._hostList)
                    {
                        if (h.resizedOrPaused == true)
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
                    }

                    if (vmsList != null)
                    {
                        MessageBox.Show("Some of the volumes in following machines are in paused/resized state." + Environment.NewLine +  vmsList +Environment.NewLine +" Wait untill all the pairs come to diff sync", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        recoveryForm.progressBar.Visible = false;
                        recoveryForm.getDetailsButton.Enabled = true;
                        recoveryForm.recoveryPreReqsButton.Enabled = true;
                        return false;
                    }

                    recoveryForm.recoveryTabControl.SelectTab(recoveryForm.logTabPage);
                    recoveryForm.recoverDataGridView.Enabled = false;
                    recoveryForm.selectAllCheckBoxForRecovery.Enabled = false;
                    recoveryForm.recoveryPreReqsButton.Enabled = false;
                    recoveryForm.tagPanel.Enabled = false;
                    recoveryForm.getDetailsButton.Enabled = false;
                    recoveryForm.progressLabel.Text = "Total number of VM(s) Selected for Readiness Checks:" + recoveryForm.vmsSelectedRecovery._hostList.Count.ToString() + "    Success: 0    Failed: 0";
                    recoveryForm.progressLabel.Visible = true;
                    recoveryForm.credentialCheckBackgroundWorker.RunWorkerAsync();
                    Trace.WriteLine(DateTime.Now + "\t Pre reqs completed on vContinuum wizard");
                }
                else
                {
                    Trace.WriteLine(DateTime.Now + "\t***FAILED**** Retrying to check vContinuum wizard has fx agent installed and licensed or not");
                    recoveryForm.progressBar.Visible = false;
                    recoveryForm.getDetailsButton.Enabled = true;
                    recoveryForm.recoveryPreReqsButton.Enabled = true;
                    Trace.WriteLine(DateTime.Now + "\t Pre reqs failed on vContinuum wizard.(Already Tried twice)");
                    return false;
                }

                return true;
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: SelectedVmMasterTargetCredentials " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
        }

        /*
         * If Displayname and uuids are not matched. We will check with the VMDK names.
         * This will be called only if uuids and display names are not matched only.
         */

        internal bool VerifyingWithVmdks(AllServersForm recoveryForm)
        {
            try
            {
                ArrayList physicalDisks = new ArrayList();
                ArrayList sourceDisks = new ArrayList();
                foreach (Host h in recoveryForm.esxSelected.GetHostList)
                {
                    string hostname = recoveryForm.masterHostAdded.hostname.Split('.')[0];
                    Debug.WriteLine(DateTime.Now + " \t Printing hostnames " + recoveryForm.masterHostAdded.hostname + " target  " + h.hostname);
                    if (hostname == h.hostname || recoveryForm.masterHostAdded.source_uuid == h.source_uuid)
                    {
                        physicalDisks = h.disks.GetDiskList;
                        Debug.WriteLine(DateTime.Now + "\t Prinitng the hash names ");
                        foreach (Hashtable hash in physicalDisks)
                        {
                            Trace.WriteLine(DateTime.Now + " \t Printing the name " + hash["Name"].ToString());
                        }
                    }
                }                 
                //Searching for the vmdk ....
                if (recoveryForm.osTypeSelected == OStype.Windows)
                {
                    foreach (Host h in recoveryForm.vmsSelectedRecovery._hostList)
                    {
                        if (h.sourceIsThere == false)
                        {
                            sourceDisks = h.disks.GetDiskList;
                            foreach (Hashtable hash in sourceDisks)
                            {
                                string diskname = hash["Name"].ToString();
                                var pattern = @"\[(.*?)]";
                                var matches = Regex.Matches(diskname, pattern);
                                foreach (Match m in matches)
                                {
                                    m.Result("");
                                    diskname = diskname.Replace(m.ToString(), "");
                                    Debug.WriteLine(DateTime.Now + " \t Printing vmdk names source side " + diskname);
                                }
                                foreach (Hashtable diskHash in physicalDisks)
                                {
                                    string targetDiskName = diskHash["Name"].ToString();
                                    var matchesTarget = Regex.Matches(targetDiskName, pattern);
                                    foreach (Match m in matchesTarget)
                                    {
                                        m.Result("");
                                        targetDiskName = targetDiskName.Replace(m.ToString(), "");
                                        Trace.WriteLine(DateTime.Now + " \t Printing vmdk names target side " + targetDiskName);
                                    }
                                    Debug.WriteLine(DateTime.Now + " \t Printing the disk names " + diskname + " " + targetDiskName);

                                    //if (diskname == targetDiskName)
                                    {

                                        foreach (Host targetHost in recoveryForm.esxSelected.GetHostList)
                                        {
                                            string hostname = recoveryForm.masterHostAdded.hostname.Split('.')[0];
                                            if (targetHost.hostname != hostname)
                                            {
                                                ArrayList targetDiskHashList = new ArrayList();
                                                targetDiskHashList = targetHost.disks.GetDiskList;
                                                foreach (Hashtable targetDiskHash in targetDiskHashList)
                                                {
                                                    string diskNameOnTarget = targetDiskHash["Name"].ToString();
                                                    var patternbrackets = @"\[(.*?)]";
                                                    var searchMatches = Regex.Matches(diskNameOnTarget, patternbrackets);
                                                    foreach (Match m in searchMatches)
                                                    {
                                                        m.Result("");
                                                        diskNameOnTarget = diskNameOnTarget.Replace(m.ToString(), "");
                                                        // This is to check that user has selected the foldername option at th etime of protection.
                                                        // If the length of the disksOntargetToCheckForFoldernameOption is equal to three we will check with the last two 
                                                        string[] disksOntargetToCheckForFoldernameOption = diskNameOnTarget.Split('/');
                                                        if (disksOntargetToCheckForFoldernameOption.Length == 3)
                                                        {
                                                            diskNameOnTarget = null;
                                                            for (int i = 1; i < disksOntargetToCheckForFoldernameOption.Length; i++)
                                                            {
                                                                Trace.WriteLine(DateTime.Now + "\t Found that at the time of protection user has selected the fodername option for the machine " + targetHost.displayname);
                                                                if (diskNameOnTarget == null)
                                                                {
                                                                    diskNameOnTarget = disksOntargetToCheckForFoldernameOption[i];
                                                                }
                                                                else
                                                                {
                                                                    diskNameOnTarget = diskNameOnTarget + "/" + disksOntargetToCheckForFoldernameOption[i];
                                                                }
                                                            }
                                                        }
                                                        
                                                        Trace.WriteLine(DateTime.Now + " \t Printing vmdk names target side " + diskNameOnTarget);
                                                    }
                                                    Trace.WriteLine(DateTime.Now + "\t Comparing the disks " + diskname + " target " + diskNameOnTarget);
                                                   
                                                    if (diskname.TrimStart().TrimEnd().ToString() == diskNameOnTarget.TrimStart().TrimEnd().ToString())
                                                    {
                                                        if (h.source_uuid != targetHost.source_uuid)
                                                        {
                                                            h.sourceIsThere = true;
                                                            // In case of the null new_macaddress we are checking by network_label
                                                            // First we will check both the network label are equal and then we will 
                                                            // Check the new_macaddress is null if null we will assign new_macaddress from target side.
                                                            foreach (Hashtable nicHash in h.nicList.list)
                                                            {
                                                                foreach (Hashtable nics in targetHost.nicList.list)
                                                                {
                                                                    if (nicHash["network_label"] != null && nics["network_label"] != null)
                                                                    {
                                                                        if (nicHash["network_label"].ToString() == nics["network_label"].ToString())
                                                                        {
                                                                            if (nicHash["new_macaddress"] == null)
                                                                            {
                                                                                Trace.WriteLine(DateTime.Now + "\t came here to assign nicac of target to protected one " + h.displayname);
                                                                                nicHash["new_macaddress"] = nics["nic_mac"];
                                                                            }
                                                                            if (nicHash["new_network_name"] == null)
                                                                            {
                                                                                nicHash["new_network_name"] = nics["network_name"];
                                                                            }
                                                                        }
                                                                    }
                                                                }
                                                            }
                                                            if (recoveryForm.appNameSelected == AppName.Recover)
                                                            {
                                                                h.new_displayname = targetHost.displayname;
                                                            }
                                                            // earlier we are not adding this of physical machines. After discussing with chandra we removed this.
                                                                h.target_uuid = targetHost.source_uuid;
                                                           
                                                            if (h.alt_guest_name == null)
                                                            {
                                                                h.alt_guest_name = targetHost.alt_guest_name;

                                                            }
                                                            h.targetvSphereHostName = targetHost.vSpherehost;
                                                            if (h.vmxversion == null)
                                                            {
                                                                h.vmxversion = targetHost.vmxversion;
                                                            }
                                                            try
                                                            {
                                                                if (h.vmxversion.Length == 0)
                                                                {
                                                                    h.vmxversion = targetHost.vmxversion;
                                                                }
                                                            }
                                                            catch (Exception ex)
                                                            {
                                                                h.vmxversion = targetHost.vmxversion;
                                                                Trace.WriteLine(DateTime.Now + "\t Failed to update vmxversion " + ex.Message);
                                                            }
                                                            Trace.WriteLine(DateTime.Now + "\t Came here to print the sourceuuid and displayname " + h.target_uuid + h.displayname);
                                                            break;
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
                else if(recoveryForm.osTypeSelected == OStype.Linux)
                {
                    foreach (Host h in recoveryForm.recoveryChecksPassedList._hostList)
                    {
                        if (h.sourceIsThere == false)
                        {
                            sourceDisks = h.disks.GetDiskList;
                            foreach (Hashtable hash in sourceDisks)
                            {
                                string diskname = hash["Name"].ToString();
                                var pattern = @"\[(.*?)]";
                                var matches = Regex.Matches(diskname, pattern);
                                foreach (Match m in matches)
                                {
                                    m.Result("");
                                    diskname = diskname.Replace(m.ToString(), "");
                                    Debug.WriteLine(DateTime.Now + " \t Printing vmdk names source side " + diskname);
                                }
                                foreach (Hashtable diskHash in physicalDisks)
                                {
                                    string targetDiskName = diskHash["Name"].ToString();
                                    var matchesTarget = Regex.Matches(targetDiskName, pattern);
                                    foreach (Match m in matchesTarget)
                                    {
                                        m.Result("");
                                        targetDiskName = targetDiskName.Replace(m.ToString(), "");
                                        Trace.WriteLine(DateTime.Now + " \t Printing vmdk names target side " + targetDiskName);
                                    }
                                    Trace.WriteLine(DateTime.Now + " \t Printing the disk names " + diskname + " " + targetDiskName);

                                    //if (diskname == targetDiskName)
                                    {

                                        foreach (Host targetHost in recoveryForm.esxSelected.GetHostList)
                                        {
                                            string hostname = recoveryForm.masterHostAdded.hostname.Split('.')[0];
                                            if (targetHost.hostname != hostname)
                                            {
                                                ArrayList targetDiskHashList = new ArrayList();
                                                targetDiskHashList = targetHost.disks.GetDiskList;
                                                foreach (Hashtable targetDiskHash in targetDiskHashList)
                                                {
                                                    string diskNameOnTarget = targetDiskHash["Name"].ToString();

                                                    var patternbrackets = @"\[(.*?)]";
                                                    var searchMatches = Regex.Matches(diskNameOnTarget, patternbrackets);
                                                    foreach (Match m in searchMatches)
                                                    {
                                                        m.Result("");
                                                        diskNameOnTarget = diskNameOnTarget.Replace(m.ToString(), "");
                                                        // This is to check that user has selected the foldername option at th etime of protection.
                                                        // If the length of the disksOntargetToCheckForFoldernameOption is equal to three we will check with the last two 
                                                        string[] disksOntargetToCheckForFoldernameOption = diskNameOnTarget.Split('/');
                                                        if (disksOntargetToCheckForFoldernameOption.Length == 3)
                                                        {
                                                            Trace.WriteLine(DateTime.Now + "\t Found that at the time of protection user has selected the fodername option for the machine " + targetHost.displayname);
                                                            diskNameOnTarget = disksOntargetToCheckForFoldernameOption[1] + "/" + disksOntargetToCheckForFoldernameOption[2];
                                                        }
                                                        Trace.WriteLine(DateTime.Now + " \t Printing vmdk names target side " + diskNameOnTarget);
                                                    }
                                                    Trace.WriteLine(DateTime.Now + "\t Comparing the disks " + diskname + " target " + diskNameOnTarget);
                                                    if (diskname == diskNameOnTarget)
                                                    {
                                                        if (h.source_uuid != targetHost.source_uuid)
                                                        {
                                                            h.sourceIsThere = true;
                                                            // In case of the null new_macaddress we are checking by network_label
                                                            // First we will check both the network label are equal and then we will 
                                                            // Check the new_macaddress is null if null we will assign new_macaddress from target side.
                                                            foreach (Hashtable nicHash in h.nicList.list)
                                                            {
                                                                foreach (Hashtable nics in targetHost.nicList.list)
                                                                {
                                                                    if (nicHash["network_label"] != null && nics["network_label"] != null)
                                                                    {
                                                                        if (nicHash["network_label"].ToString() == nics["network_label"].ToString())
                                                                        {
                                                                            if (nicHash["new_macaddress"] == null)
                                                                            {
                                                                                Trace.WriteLine(DateTime.Now + "\t came here to assign nicac of target to protected one " + h.displayname);
                                                                                nicHash["new_macaddress"] = nics["nic_mac"];
                                                                            }
                                                                            if (nicHash["new_network_name"] == null)
                                                                            {
                                                                                nicHash["new_network_name"] = nics["network_name"];
                                                                            }
                                                                        }
                                                                    }
                                                                }
                                                            }
                                                            if (recoveryForm.appNameSelected == AppName.Recover)
                                                            {
                                                                h.new_displayname = targetHost.displayname;
                                                            }

                                                            h.target_uuid = targetHost.source_uuid;

                                                            if (h.alt_guest_name == null)
                                                            {
                                                                h.alt_guest_name = targetHost.alt_guest_name;
                                                                h.vmxversion = targetHost.vmxversion;
                                                            }
                                                            Trace.WriteLine(DateTime.Now + "\t Came here to print the sourceuuid and displayname " + h.target_uuid + h.displayname);
                                                            break;
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
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: SelectedVmMasterTargetCredentials " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            return true;
        }

        //This will verify if already recovery option later is opted for the selected machines.
        private bool CheckIfRecoveryLaterOpted(AllServersForm allServersForm)
        {
            try
            {
                Cxapicalls cxApi = new Cxapicalls();
                foreach (Host h in allServersForm.vmsSelectedRecovery._hostList)
                {
                    if (h.recoveryPlanId != null)
                    {
                        Host h1 = new Host();
                        h1.planid = h.recoveryPlanId;
                        h1.operationType = OperationType.Recover;
                        cxApi.Post(h1, "MonitorESXProtectionStatus");
                        h1 = Cxapicalls.GetHost;
                        foreach (Hashtable hash in h1.taskList)
                        {
                            if (hash["TaskStatus"] != null)
                            {

                                if (hash["TaskStatus"].ToString() == "Queued")
                                {
                                    MessageBox.Show("Recovery later FX job is set for the VM: " + h.displayname + ". Go to monitor screen and use start to run FX job.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
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
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: CheckIfRecoveryLaterOpted " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            return true;
        }


        internal bool RecoveryReadinessChecks(AllServersForm allServersForm)
        {


            Com.Inmage.Cxapicalls cxApis = new Com.Inmage.Cxapicalls();
           
           
            tickerDelegate = new TickerDelegate(SetLeftTicker);
            if(CheckIfRecoveryLaterOpted(allServersForm) == false)
            {
                Trace.WriteLine(DateTime.Now + "\t Already recovery later option is selected for machine");
                return false;
            }
            foreach (Host h in allServersForm.vmsSelectedRecovery._hostList)
            {
                Host h1 = h;
                _message = "Running readiness check for recovery of " + h1.displayname;
                allServersForm.recoveryText.Invoke(tickerDelegate, new object[] { _message });
                bool verifiedTags = cxApis.RecoveryReadinessChecks( h1, allServersForm.masterHostAdded);
                h1 = Cxapicalls.GetHost;
                if (verifiedTags == true)
                {
                    _verifiedTags = true;
                    h.commonTagAvaiable = h1.commonTagAvaiable;
                    h.commonTimeAvaiable = h1.commonTimeAvaiable;
                    h.commonUserDefinedTimeAvailable = h1.commonUserDefinedTimeAvailable;
                    h.tag = h1.tag;
                    h.commonSpecificTimeAvaiable = h1.commonSpecificTimeAvaiable;
                    h.userDefinedTime = h1.userDefinedTime;
                    if (h.recoveryOperation == LATEST_TAG_RECOVERY && h.commonTagAvaiable == true)
                    {
                        h.recoveryPrereqsPassed = true;

                    }
                    else if (h.recoveryOperation == LATEST_TIME_RECOVERY && h.commonTimeAvaiable == true)
                    {
                        h.recoveryPrereqsPassed = true;
                    }
                    else if (h.recoveryOperation == SPECIFIC_TAG_AT_TIME_RECOVERY && h.commonUserDefinedTimeAvailable == true)
                    {
                        h.recoveryPrereqsPassed = true;
                    }
                    else if (h.recoveryOperation == SPECIFIC_TIME_RECOVERY && h.commonSpecificTimeAvaiable == true)
                    {
                        h.recoveryPrereqsPassed = true;
                    }
                    else if (h.recoveryOperation == SPECIFIC_TAG && h.commonTagAvaiable == true)
                    {
                        h.recoveryPrereqsPassed = true;
                    }

                    else
                    {
                        h.recoveryPrereqsPassed = false;
                        //MessageBox.Show("Common Tag and common time not Available for recovery of " + h.hostname+ Environment.NewLine+ "Can't proceed with recovery" );
                    }

                }
                else
                {
                    h.recoveryPrereqsPassed = false;

                    Trace.WriteLine(DateTime.Now + "\t Failed to verify the tags");

                    MessageBox.Show("Unable to verify consisteny tags for host: " + h.hostname, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);


                    // return false;

                }
                _message = ".... Done" + Environment.NewLine;
                allServersForm.recoveryText.Invoke(tickerDelegate, new object[] { _message });
                Trace.WriteLine(DateTime.Now + " \t" + "-----------------------------------------------------------------------------------------------");
                Trace.WriteLine(DateTime.Now + " \t" + "Completed running of readiness check for recovery of " + h1.displayname + "hostname:" + h1.hostname + "IP:" + h1.ip);
                Trace.WriteLine(DateTime.Now + " \t" + "-----------------------------------------------------------------------------------------------");

            }
            string message = "";
            Trace.WriteLine(DateTime.Now + " \t Printing the ount of recovery list " + allServersForm.vmsSelectedRecovery._hostList.Count.ToString());
            foreach (Host h in allServersForm.vmsSelectedRecovery._hostList)
            {
                if (h.recoveryPrereqsPassed == true)
                {

                    message = message + " " + h.displayname + "  can be recovered." + Environment.NewLine;
                    Trace.WriteLine(DateTime.Now + " \t " + message);
                }
                else
                {
                    if (h.recoveryOperation == LATEST_TAG_RECOVERY)
                    {
                        message = message + " " + h.displayname + " can't be recovered. Error:There is no common recovery tag available" + Environment.NewLine;

                        Trace.WriteLine(DateTime.Now + " \t " + message);

                    }

                    if (h.recoveryOperation == LATEST_TIME_RECOVERY)
                    {
                        message = message + " " + h.displayname + " can't be recovered. Error:There is no common recovery point available." + Environment.NewLine;
                        Trace.WriteLine(DateTime.Now + " \t " + message);
                    }

                    if (h.recoveryOperation == SPECIFIC_TAG_AT_TIME_RECOVERY)
                    {
                        DateTime convertedDate = DateTime.Parse(h.userDefinedTime);

                        DateTime dateTime = TimeZone.CurrentTimeZone.ToLocalTime(convertedDate);
                        h.userDefinedTime = dateTime.ToString();
                        message = message + " " + h.displayname + " can't be recovered. Error:There is no common recovery tag available prior to " + h.userDefinedTime + Environment.NewLine;
                        Trace.WriteLine(DateTime.Now + " \t " + message);
                    }
                    if (h.recoveryOperation == SPECIFIC_TAG)
                    {
                         message = message + " " + h.displayname + " can't be recovered. Tag specifed by user is not available" + Environment.NewLine;

                         Trace.WriteLine(DateTime.Now + " \t " + message);
                    }
                }
            }


            _logMessage = message;


            return true;
        }


        /*
         * This is to verify given credentails are working fine or not.
         * Once we conformed tht credentials are working we will verify the tags which are selected by user.
         * If tags are not there we will block Recvoery/DR Drill for that machine
         */

        internal bool BackGroundWorkerRunReadinessChecks(AllServersForm allServersForm)
        {

            string error = "";
            Trace.WriteLine(" Reached here to verify the given credentials are correct or not");
            try
            {
                // MessageBox.Show("    " + recoveryForm._masterHost.ip + recoveryForm._masterHost.domain + recoveryForm._masterHost.userName + recoveryForm._masterHost.passWord);
                _wmiMessageFormCancel = false;
                _skipRecoveryChecks = false;
                tickerDelegate = new TickerDelegate(SetLeftTicker);
                string mtIp = null;
                if (allServersForm.masterHostAdded.hostname.Split('.')[0].ToUpper() == Environment.MachineName.ToUpper())
                {
                    mtIp = ".";
                }
                else
                {
                    mtIp = allServersForm.masterHostAdded.ip;
                }
                if (WinPreReqs.WindowsShareEnabled(mtIp, allServersForm.masterHostAdded.domain, allServersForm.masterHostAdded.userName, allServersForm.masterHostAdded.passWord, allServersForm.masterHostAdded.hostname,  error) == false)
                {
                    // error = error + Environment.NewLine + Environment.NewLine + "You can still proceed with this error. How ever, automatic remote agent installation and validation checks will be skipped";
                    // Environment.NewLine + Environment.NewLine + "Do you want to proceed ?";
                    //here we are showing popup with options.
                    error = WinPreReqs.GetError;
                    WmiErrorMessageForm errorMessageForm = new WmiErrorMessageForm(error);
                    errorMessageForm.domainText.Text = allServersForm.masterHostAdded.domain;
                    errorMessageForm.userNameText.Text = allServersForm.masterHostAdded.userName;
                    errorMessageForm.passWordText.Text = allServersForm.masterHostAdded.passWord;
                    errorMessageForm.skipWmiValidationsRadioButton.Text = "Skip checks for consistency points and proceed";
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
                            _skipRecoveryChecks = true;
                            allServersForm.wmiWorkedFineForMt = false;
                            allServersForm.masterHostAdded.skipPushAndPreReqs = true;
                        }
                        else
                        {
                            _reRunCredentials = true;
                            //Debug.WriteLine(" Domain " + errorMessageForm.domain + errorMessageForm.username + errorMessageForm.password);
                            allServersForm.masterHostAdded.domain = errorMessageForm.domain;
                            allServersForm.masterHostAdded.userName = errorMessageForm.username;
                            allServersForm.masterHostAdded.passWord = errorMessageForm.password;
                            _cacheDomain = errorMessageForm.domain;
                            _cacheUserName = errorMessageForm.username;
                            _cachePassWord = errorMessageForm.password;
                            //   MessageBox.Show("domain " + errorMessageForm.domain + errorMessageForm.username + errorMessageForm.password);
                            return false;
                        }
                    }
                    else
                    {
                        //User had clicked the cancel button in the WMI error form. 
                        //We fail the 
                        allServersForm.wmiWorkedFineForMt = false;
                        _wmiMessageFormCancel = true;
                        return false;
                    }
                }
                else
                {
                    allServersForm.wmiWorkedFineForMt = true;
                }
                Trace.WriteLine(DateTime.Now +  "\t BackGroundWorkerRunReadinessChecks: Completed the WMI checks.");
                if (_wmiMessageFormCancel == false)
                {
                    if (allServersForm.masterHostAdded.skipPushAndPreReqs == false)
                    {
                        allServersForm.wmiBasedRecvoerySelected = true;
                        Host h1;
                        WinPreReqs wp = new WinPreReqs("", allServersForm.masterHostAdded.domain, allServersForm.masterHostAdded.userName, allServersForm.masterHostAdded.passWord);
                        Trace.WriteLine(DateTime.Now + " \t Count of the VMs to recover " + allServersForm.vmsSelectedRecovery._hostList.Count.ToString());
                        if (allServersForm.vmsSelectedRecovery._hostList.Count > 0)
                        {
                            Trace.WriteLine(DateTime.Now + " \t Going to start the readiness check for selected machines ");                            
                            foreach (Host h in allServersForm.vmsSelectedRecovery._hostList)
                            {
                                _verifiedTags = true;
                                h1 = h;
                                Trace.WriteLine(DateTime.Now + " \t" + "-----------------------------------------------------------------------------------------------");
                                Trace.WriteLine(DateTime.Now + " \t" + "Running readiness check for recovery of " + h1.displayname + "hostname:" + h1.hostname + "IP:" + h1.ip);
                                Trace.WriteLine(DateTime.Now + " \t" + "-----------------------------------------------------------------------------------------------");
                                int returnValue = 1;
                                _message = "Running readiness check for recovery of " + h1.displayname;
                                allServersForm.recoveryText.Invoke(tickerDelegate, new object[] { _message });
                                if (h.recoveryOperation == 3)
                                {
                                    Trace.WriteLine(DateTime.Now + "\t Recovery operation selected 3 and adding 1 min to given time");
                                    Trace.WriteLine(DateTime.Now + "\t Actual selected time:" + h.userDefinedTime);
                                    //h.userDefinedTime = DateTime.Parse(h.userDefinedTime).AddMinutes(1).ToString();
                                    Trace.WriteLine(DateTime.Now + "\t increased time(by adding 1 min) time:" + h.userDefinedTime);
                                }
                                returnValue = wp.PreReqCheckForRecovery(allServersForm.masterHostAdded.hostname,  h1,  WinTools.GetcxIPWithPortNumber());
                                if (returnValue != 0)
                                {
                                    Trace.WriteLine(DateTime.Now + "\t" + "*********Rerunning the readiness check for recovery of " + h1.displayname + "hostname:" + h1.hostname + "IP:" + h1.ip + "***********************");
                                    returnValue = wp.PreReqCheckForRecovery(allServersForm.masterHostAdded.hostname,  h1,  WinTools.GetcxIPWithPortNumber());
                                    h1 = WinPreReqs.GetHost;
                                }
                               
                               
                                if (returnValue == 0)
                                {
                                    _reRunCredentials = false;
                                    h.commonTagAvaiable = h1.commonTagAvaiable;
                                    h.commonTimeAvaiable = h1.commonTimeAvaiable;
                                    h.commonUserDefinedTimeAvailable = h1.commonUserDefinedTimeAvailable;
                                    h.tag = h1.tag;
                                    h.userDefinedTime = h1.userDefinedTime;
                                    if (h.recoveryOperation == LATEST_TAG_RECOVERY && h.commonTagAvaiable == true)
                                    {
                                        h.recoveryPrereqsPassed = true;

                                    }
                                    else if (h.recoveryOperation == LATEST_TIME_RECOVERY && h.commonTimeAvaiable == true)
                                    {
                                        h.recoveryPrereqsPassed = true;
                                    }
                                    else if (h.recoveryOperation == SPECIFIC_TAG_AT_TIME_RECOVERY && h.commonUserDefinedTimeAvailable == true)
                                    {
                                        h.recoveryPrereqsPassed = true;
                                    }
                                    else if (h.recoveryOperation == SPECIFIC_TIME_RECOVERY && h.commonSpecificTimeAvaiable == true)
                                    {
                                        h.recoveryPrereqsPassed = true;
                                    }
                                    else
                                    {
                                        h.recoveryPrereqsPassed = false;
                                        //MessageBox.Show("Common Tag and common time not Available for recovery of " + h.hostname+ Environment.NewLine+ "Can't proceed with recovery" );
                                    }
                                    
                                }
                                else
                                {
                                    h.recoveryPrereqsPassed = false;



                                    MessageBox.Show("Unable to verify consisteny tags for host: " + h.hostname, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);


                                    // return false;

                                }
                                
                                
                                _message = ".... Done" + Environment.NewLine ;
                                allServersForm.recoveryText.Invoke(tickerDelegate, new object[] { _message });
                                Trace.WriteLine(DateTime.Now + " \t" + "-----------------------------------------------------------------------------------------------");
                                Trace.WriteLine(DateTime.Now + " \t" + "Completed running of readiness check for recovery of " + h1.displayname + "hostname:" + h1.hostname + "IP:" + h1.ip);
                                Trace.WriteLine(DateTime.Now + " \t" + "-----------------------------------------------------------------------------------------------");
                            }
                        }
                        else
                        {
                            Trace.WriteLine(DateTime.Now + " \t Came here that recovery Machines list is null ");
                            return false;
                        }
                        string message = "";
                        Trace.WriteLine(DateTime.Now + " \t Printing the ount of recovery list " + allServersForm.vmsSelectedRecovery._hostList.Count.ToString());
                        foreach (Host h in allServersForm.vmsSelectedRecovery._hostList)
                        {                            
                            if (h.recoveryPrereqsPassed == true)
                            {

                                message = message + " " + h.displayname + "  can be recovered." + Environment.NewLine;
                                Trace.WriteLine(DateTime.Now + " \t " + message);
                            }
                            else
                            {
                                if (h.recoveryOperation == LATEST_TAG_RECOVERY)
                                {
                                    message = message + " " + h.displayname + " can't be recovered. Error:There is no common recovery tag available" + Environment.NewLine;

                                    Trace.WriteLine(DateTime.Now + " \t " + message);

                                }

                                if (h.recoveryOperation == LATEST_TIME_RECOVERY)
                                {
                                    message = message + " " + h.displayname + " can't be recovered. Error:There is no common recovery point available." + Environment.NewLine;
                                    Trace.WriteLine(DateTime.Now + " \t " + message);
                                }

                                if (h.recoveryOperation == SPECIFIC_TAG_AT_TIME_RECOVERY)
                                {
                                    DateTime convertedDate = DateTime.Parse(h.userDefinedTime);

                                    DateTime dt = TimeZone.CurrentTimeZone.ToLocalTime(convertedDate);
                                    h.userDefinedTime = dt.ToString();
                                    message = message + " " + h.displayname + " can't be recovered. Error:There is no common recovery tag available prior to " + h.userDefinedTime + Environment.NewLine;
                                    Trace.WriteLine(DateTime.Now + " \t " + message);
                                }
                            }
                        }


                        _logMessage = message;

                        // MessageBox.Show(message, "Information", MessageBoxButtons.OK, MessageBoxIcon.Information);
                        // allServersForm.addSourcePanelEsxDataGridView.Rows[allServersForm._credentialIndex].Cells[SOURCE_CHECKBOX_COLUMN].Value = false;

                        // credentialsCheckPassed = false;
                        return true;
                    }
                    else
                    {
                        //User chose to skip the pre-req in the WMI popup box, in master target
                        foreach (Host h in allServersForm.vmsSelectedRecovery._hostList)
                        {
                            h.recoveryPrereqsPassed = true;
                        }

                    }
                }



                return true;
            }
            catch (Exception ex)
            {
                MessageBox.Show("Failed to find the tags.");
                allServersForm.nextButton.Enabled = false;
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine("Caught exception at Method: BackGroundWorkerRunReadinessChecks " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
        }

        /*
         * This will check for the fx license for the MT, vCon and CX servers.
         * With out fx license we can do the recovey/DR Drill with fx based.
         */
        internal bool CheckForLicenses(AllServersForm recoveryForm)
        {
            try
            {
                //Checking whether RCLI is licensed or not.

                //Prereq checks for Wizard/RCLI box
                Host tempHost = new Host();
                tempHost.hostname = Environment.MachineName;
                tempHost.displayname = Environment.MachineName;
                //Host cxHost = new Host();
                //cxHost.ip = WinTools.GetcxIP();
                //string cxHostName = null;
                WinPreReqs wp = new WinPreReqs("", "", "", "");
                string ipaddress = "";
                recoveryForm.clearLogLinkLabel.Visible = true;
                recoveryForm.recoveryText.Visible = true;

                int returnCodeofvCon = wp.SetHostIPinputhostname(tempHost.hostname,  ipaddress, WinTools.GetcxIPWithPortNumber());
                ipaddress = WinPreReqs.GetIPaddress;
                if (returnCodeofvCon == 0)
                {
                    tempHost.ip = ipaddress;
                    if (wp.GetDetailsFromcxcli( tempHost, WinTools.GetcxIPWithPortNumber()) == true)
                    {
                        tempHost = WinPreReqs.GetHost;
                        if (tempHost.fxagentInstalled == false )
                        {
                            recoveryForm.recoveryText.AppendText("ERROR:Make sure that Fx agent is installed on local machine(" + tempHost.hostname + ")" + Environment.NewLine);
                            return false;
                        }
                        if (tempHost.fx_agent_heartbeat == false)
                        {
                            recoveryForm.recoveryText.AppendText("ERROR:Make sure that Fx service is running on local machine(" + tempHost.hostname + ")" + Environment.NewLine);
                            return false;
                        }
                    }
                }
                else if (returnCodeofvCon == 1)
                {
                    tempHost = WinPreReqs.GetHost;
                    recoveryForm.recoveryText.AppendText("ERROR:Found multiple entries in CX for machine(" + tempHost.hostname + ")" + Environment.NewLine);
                    return false;
                }
                else if (returnCodeofvCon == 2)
                {
                    tempHost = WinPreReqs.GetHost;
                    recoveryForm.recoveryText.AppendText("ERROR:Dis not found entry in CX for machine(" + tempHost.hostname + ")" + Environment.NewLine);
                    return false;
                }
                
                //if (WinPreReqs.SetHostNameInputIP(cxHost.ip, ref cxHostName, WinTools.GetcxIPWithPortNumber()) == true)
                //{
                //    int returnCode = 0;
                //   returnCode= wp.SetHostIPinputhostname(cxHostName, ref cxHost.ip, WinTools.GetcxIPWithPortNumber());
                //    cxHost.hostname = cxHostName;
                //    if (returnCode == 1)
                //    {
                //        recoveryForm.recoveryText.AppendText("ERROR:Found multiple entries in CX for machine(" + cxHost.hostname + ")" + Environment.NewLine);
                //        return false;
                //    }

                //    if (wp.GetDetailsFromcxcli(ref cxHost, WinTools.GetcxIPWithPortNumber()) == true)
                //    {
                //        recoveryForm.recoveryText.AppendText("Checking for agent license");
                //        if (cxHost.fxlicensed == false)
                //        {
                //            recoveryForm.recoveryText.AppendText("Error: Fx is not licensed for " + cxHost.displayname + Environment.NewLine);
                //            recoveryForm.Refresh();
                //        }
                //        else
                //        {
                //            recoveryForm.recoveryText.AppendText("\t\t  : PASSED" + Environment.NewLine);
                //            cxHost.ranPrecheck = true;
                //        }
                //        if (cxHost.fx_agent_heartbeat == false)
                //        {
                //            recoveryForm.recoveryText.AppendText("Agents are not reporting status to CX for last 15 mins. Please verify it on " + cxHost.displayname + Environment.NewLine);
                //        }
                //    }
                //}
               
                

                //Checking whether Master target Licensed or not


                Trace.WriteLine(DateTime.Now + "\t Printing the hostname " + recoveryForm.masterHostAdded.hostname);
                string hostname = recoveryForm.masterHostAdded.hostname;               
                if (wp.GetDetailsFromcxcli( recoveryForm.masterHostAdded, WinTools.GetcxIPWithPortNumber()) == true)
                {
                    recoveryForm.masterHostAdded = WinPreReqs.GetHost;

                    if (recoveryForm.masterHostAdded.fxagentInstalled == false )
                    {

                        recoveryForm.recoveryText.AppendText("ERROR:Make sure that Fx agent is installed on Master target(" + recoveryForm.masterHostAdded.hostname + ")" + Environment.NewLine);

                        return false;
                    }

                    if (recoveryForm.masterHostAdded.fx_agent_heartbeat == false)
                    {
                        recoveryForm.recoveryText.AppendText("ERROR:Make sure that Fx service is running on Master target(" + recoveryForm.masterHostAdded.hostname + ")" + Environment.NewLine);
                        return false;
                    }

                    recoveryForm.masterHostAdded.hostname = hostname;
                    Trace.WriteLine(DateTime.Now + "\t Printing the hostname " + recoveryForm.masterHostAdded.hostname);
                }

            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: CheckForLicenses " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");

                return false;
            }
            return true;

        }


        /*
         * This will checkl for the FX jobs. whether any Fx jobs are running on this mt. 
         * If so we will block all the operations.
         * Parallelism is not there ofr mt.
         */
        internal bool CheckForMasterFxJobs(AllServersForm recoveryForm)
        {
            WinPreReqs wp = new WinPreReqs("", "", "", "");
            string hostname = recoveryForm.masterHostAdded.hostname;
            if (wp.MasterTargetPreCheck( recoveryForm.masterHostAdded, WinTools.GetcxIPWithPortNumber()) == true)
            {
                recoveryForm.masterHostAdded =  WinPreReqs.GetHost;
                recoveryForm.masterHostAdded.hostname = hostname;
                if (recoveryForm.masterHostAdded.machineInUse == true)
                {                   
                    return false;
                }

            }
            return true;
        }

        /*
         * Once the verification of Tags are completed we will come to this method. TO display user 
         * Whether that Machine can be recovered or not. 
         * VMs with out tags  can be unslected by the Wizard automatically.
         */
        internal bool PostReadinessCheck(AllServersForm recoveryForm)
        {


            try
            {

                Host removeFromList = new Host();
                Trace.WriteLine(DateTime.Now + " \t printing recovery list count before unselecting" + recoveryForm.vmsSelectedRecovery._hostList.Count.ToString());
                

                foreach (Host h in recoveryForm.vmsSelectedRecovery._hostList)
                {

                    if (h.recoveryPrereqsPassed == false)
                    {
                        for (int i = 0; i < recoveryForm.recoverDataGridView.RowCount; i++)
                        {
                            if (recoveryForm.recoverDataGridView.Rows[i].Cells[HOSTNAME_COLUMN].Value != null)
                            {
                                if (h.hostname == recoveryForm.recoverDataGridView.Rows[i].Cells[HOSTNAME_COLUMN].Value.ToString())
                                {
                                    recoveryForm.recoverDataGridView.Rows[i].Cells[RECOVER_CHECK_BOX_COLUMN].Value = false;
                                    recoveryForm.recoverDataGridView.RefreshEdit();
                                }
                            }
                        }
                    }
                }
                Trace.WriteLine(DateTime.Now + " \t printing recovery list count afetr selecting" + recoveryForm.vmsSelectedRecovery._hostList.Count.ToString());
                recoveryForm.progressBar.Visible = false;
                recoveryForm.getDetailsButton.Enabled = true;

                recoveryForm.recoveryPreReqsButton.Enabled = true;





                if (_verifiedTags == true)
                {
                    Host h1 = new Host();

                    foreach (Host h in recoveryForm.vmsSelectedRecovery._hostList)
                    {
                        if (h.recoveryOperation == SPECIFIC_TAG_AT_TIME_RECOVERY && h.recoveryPrereqsPassed == true)
                        {

                            h1 = h;
                            DateTime convertedDate = DateTime.Parse(h.userDefinedTime);


                            WinTools.SetHostTimeinitslocal( h1);
                            h1 = WinTools.GetHost;
                            System.Globalization.CultureInfo culture = System.Globalization.CultureInfo.CurrentCulture;
                            System.Globalization.DateTimeFormatInfo info = culture.DateTimeFormat;


                            Trace.WriteLine(DateTime.Now + "\t Culture name " + culture.Name);
                            string datePattern = info.ShortDatePattern;

                            string timePattern = info.ShortTimePattern;
                            DateTime dt = DateTime.Now;
                            dt = DateTime.Parse(h1.selectedTimeByUser);
                            DateTime gmt = DateTime.Parse(dt.ToString());

                            h1.selectedTimeByUser = gmt.ToString(datePattern +" "+ timePattern);
                            switch (MessageBox.Show("You can recover VM:" + h.hostname + " to a consisteny point available at " + h1.selectedTimeByUser
                               + Environment.NewLine + "Do you want to recover to that point? ", "Information", MessageBoxButtons.YesNo, MessageBoxIcon.Question))
                            {
                                case DialogResult.No:

                                    for (int i = 0; i < recoveryForm.recoverDataGridView.RowCount; i++)
                                    {
                                        // MessageBox.Show("Came here to verify");
                                        if (h1.hostname == recoveryForm.recoverDataGridView.Rows[i].Cells[HOSTNAME_COLUMN].Value.ToString())
                                        {
                                            //MessageBox.Show("Came here");
                                            recoveryForm.recoverDataGridView.Rows[i].Cells[RECOVER_CHECK_BOX_COLUMN].Value = false;
                                            recoveryForm.recoverDataGridView.RefreshEdit();
                                            h.recoveryPrereqsPassed = false;
                                            break;

                                        }
                                    }

                                    break;


                            }



                        }
                        if (h.recoveryPrereqsPassed == true)
                        {
                            recoveryForm.nextButton.Enabled = true;
                            recoveryForm.recoveryPlanNameLabel.Visible = true;
                            recoveryForm.recoveryPlanNameTextBox.Visible = true;
                            recoveryForm.recoverDataGridView.Enabled = true;
                            recoveryForm.selectAllCheckBoxForRecovery.Enabled = true;
                            recoveryForm.recoveryPreReqsButton.Enabled = true;
                            recoveryForm.tagPanel.Enabled = true;
                            recoveryForm.getDetailsButton.Enabled = true;
                        }


                        /* recoveryForm._selectedRecoverList.Print();
                         if (recoveryForm._selectedRecoverList.DoesHostExist(h1, ref listIndex) == true)
                         {
                             MessageBox.Show("To remove");
                             recoveryForm._selectedRecoverList.RemoveHost(h1);
                         }*/
                    }

                    int success = 0;
                    int failed = 0;

                    foreach (Host h in recoveryForm.vmsSelectedRecovery._hostList)
                    {
                        if (h.recoveryPrereqsPassed == true)
                        {
                            success++;
                        }
                        else
                        {
                            failed++;
                        }
                    }

                    recoveryForm.progressLabel.Text = "Total number of VM(s) selected for Readiness check: " + recoveryForm.vmsSelectedRecovery._hostList.Count.ToString() + "    Success: " + success.ToString() + "    Failed: " + failed.ToString();
                    _rowIndex = 0;
                    recoveryForm.recoveryCheckReportDataGridView.Rows.Clear();
                    Trace.WriteLine(DateTime.Now + " \t printing recovery list count before checkreport table" + recoveryForm.vmsSelectedRecovery._hostList.Count.ToString());
                    CheckReportTable(recoveryForm);
                    recoveryForm.recoverPanel.Enabled = true;
                    recoveryForm.recoveryTabControl.SelectTab(recoveryForm.runReadinessCheckTabPage);
                    recoveryForm.recoveryText.AppendText(Esx_RecoverPanelHandler._logMessage);
                    recoveryForm.recoveryText.Visible = true;
                    recoveryForm.recoverDataGridView.Enabled = true;
                    recoveryForm.selectAllCheckBoxForRecovery.Enabled = true;
                    recoveryForm.recoveryPreReqsButton.Enabled = true;
                    recoveryForm.tagPanel.Enabled = true;
                    recoveryForm.getDetailsButton.Enabled = true;
                    recoveryForm.recoverPanel.Enabled = true;
                    recoveryForm.clearLogLinkLabel.Visible = true;
                    recoveryForm.recoveryCheckReportDataGridView.Visible = true;
                    return true;

                }
                else
                {
                    recoveryForm.recoveryText.AppendText("Failed to fetch the tags" + Environment.NewLine);
                }               
            }
            catch (Exception ex)
            {
                MessageBox.Show("Failed to find the tags.");
                recoveryForm.nextButton.Enabled = false;
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: PostReadinessCheck " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");

                // recoveryForm.recoveryText = null;


                return false;
            }
            return true;
        }

        /*
         * This will prepare HostList for the selected vm by the user and selected type of tags
         * Follwoed by this we will check for the credentails are correct or not.
         */
        internal bool PrePareHostListForRecovery(AllServersForm recoveryForm)
        {

            try
            {
                Trace.WriteLine(DateTime.Now + "\t Entered PrePareHostListForRecovery in the ESX_RecoveryPanelHandler.cs");
                Host h1 = new Host();
                recoveryForm.recoveryPreReqsButton.Enabled = false;
                for (int i = 0; i < recoveryForm.recoverDataGridView.RowCount; i++)
                {


                    h1.hostname = recoveryForm.recoverDataGridView.Rows[i].Cells[HOSTNAME_COLUMN].Value.ToString();
                    h1.displayname = recoveryForm.recoverDataGridView.Rows[i].Cells[DISPLAY_NAME_COLUMN].Value.ToString();
                    foreach (Host h in recoveryForm.sourceListByUser._hostList)
                    {
                       // Trace.WriteLine(DateTime.Now + "\t Printing the comparing values " + h.hostname + " second one " + h1.hostname);
                        if (h.hostname == h1.hostname)
                        {

                            if ((bool)recoveryForm.recoverDataGridView.Rows[i].Cells[RECOVER_CHECK_BOX_COLUMN].FormattedValue)
                            {
                                recoveryForm.masterHostAdded.source_uuid = h.mt_uuid;
                                recoveryForm.masterHostAdded.displayname = h.masterTargetDisplayName;
                                recoveryForm.masterHostAdded.hostname = h.masterTargetHostName;
                                Trace.WriteLine(DateTime.Now + "\t MT hostname " + recoveryForm.masterHostAdded.hostname);
                                recoveryForm.vmsSelectedRecovery.AddOrReplaceHost(h);
                            }
                            else
                            {
                                recoveryForm.vmsSelectedRecovery.RemoveHost(h);
                            }
                        }
                    }
                }
                if (recoveryForm.vmsSelectedRecovery._hostList.Count == 0)
                {
                    recoveryForm.recoveryPreReqsButton.Enabled = true;
                    MessageBox.Show("Please select one or more machines for recovery ", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return false;
                }
                foreach (Host h in recoveryForm.vmsSelectedRecovery._hostList)
                {
                    if (h.recoveryOperation == 5 && h.tag == null)
                    {
                        recoveryForm.recoveryPreReqsButton.Enabled = true;
                        MessageBox.Show("You have selected recovery option Specify Tag for the machine: " + h.displayname + " and not specifed the tag. Plese specify tag name to continue", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return false;
                    }
                }
                if (SelectedVmMasterTargetCredentials(recoveryForm) == false)
                {
                    return false;
                }
                Trace.WriteLine(DateTime.Now + "\t Entered PrePareHostListForRecovery in the ESX_RecoveryPanelHandler.cs");
                return true;
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: PrePareHostListForRecovery " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");

                return false;
                 
            }
        }
        /*
         * Once the verification of tags are compelted. We will display whether vms can be recovered or not in the Table which is in logs tab.
         * There we will give breif description also for the user. Why he is unable to Recover/DR Drill the machine.
         */

        internal bool CheckReportTable(AllServersForm recoveryForm)
        {
            try
            {
                Trace.WriteLine(DateTime.Now + "\t Printing count of recovery list before showing to checkreport table " + recoveryForm.vmsSelectedRecovery._hostList.Count.ToString());
                foreach (Host h in recoveryForm.vmsSelectedRecovery._hostList)
                {
                    Trace.WriteLine(DateTime.Now + " \t Printing the host and display name of the machines " + h.displayname + " " + h.hostname);
                }
                foreach (Host h in recoveryForm.vmsSelectedRecovery._hostList)
                {
                    if (h.recoveryPrereqsPassed == true)
                    {
                        recoveryForm.recoveryCheckReportDataGridView.Rows.Add(1);
                        recoveryForm.recoveryCheckReportDataGridView.Rows[_rowIndex].Cells[CHECK_REPORT_SERVER_NAME_COLUMN].Value = h.displayname;
                        if (h.recoveryOperation == LATEST_TIME_RECOVERY || h.recoveryOperation == SPECIFIC_TIME_RECOVERY)
                        {
                            recoveryForm.recoveryCheckReportDataGridView.Rows[_rowIndex].Cells[CHECK_REPORT_TAG_COLUMN].Value = "Verifying time";
                        }
                        else
                        {
                            recoveryForm.recoveryCheckReportDataGridView.Rows[_rowIndex].Cells[CHECK_REPORT_TAG_COLUMN].Value = "Verifying tag";
                        }
                        recoveryForm.recoveryCheckReportDataGridView.Rows[_rowIndex].Cells[CHECK_RESULT_COLUMN].Value = _passed;
                        _rowIndex++;
                    }
                    else
                    {
                        Trace.WriteLine(DateTime.Now + " \t Printing the host and recovery option " + h.recoveryOperation.ToString());
                        string message;
                        if (h.recoveryOperation == LATEST_TAG_RECOVERY)
                        {
                            Trace.WriteLine(DateTime.Now + " \t Printing the host and display name " + h.hostname + " " + h.displayname);
                            message = "A common consistency tag is not available to perform recovery operation." + Environment.NewLine
                                          + "Use Latest Time option to recover." + Environment.NewLine;
                            // + "2. Run consistency job from CX UI(if source is available)";
                            recoveryForm.recoveryCheckReportDataGridView.Rows.Add(1);
                            recoveryForm.recoveryCheckReportDataGridView.Rows[_rowIndex].Cells[CHECK_REPORT_SERVER_NAME_COLUMN].Value = h.displayname;
                            recoveryForm.recoveryCheckReportDataGridView.Rows[_rowIndex].Cells[CHECK_REPORT_TAG_COLUMN].Value = "Verifying tag";
                            recoveryForm.recoveryCheckReportDataGridView.Rows[_rowIndex].Cells[CHECK_RESULT_COLUMN].Value = _failed;
                            recoveryForm.recoveryCheckReportDataGridView.Rows[_rowIndex].Cells[CORRECT_ACTION_COLUMN].Value = message;
                            _rowIndex++;
                        }
                        if (h.recoveryOperation == LATEST_TIME_RECOVERY)
                        {
                            Trace.WriteLine(DateTime.Now + " \t Printing the host and display name " + h.hostname + " " + h.displayname);
                            message = "This VM can't be recovered." + Environment.NewLine
                                      + "Make sure replications are in progress and volumes are in data mode" + Environment.NewLine
                                      + "Contact customer support for further assistance";
                            recoveryForm.recoveryCheckReportDataGridView.Rows.Add(1);
                            recoveryForm.recoveryCheckReportDataGridView.Rows[_rowIndex].Cells[CHECK_REPORT_SERVER_NAME_COLUMN].Value = h.displayname;
                            recoveryForm.recoveryCheckReportDataGridView.Rows[_rowIndex].Cells[CHECK_REPORT_TAG_COLUMN].Value = "Verifying time";
                            recoveryForm.recoveryCheckReportDataGridView.Rows[_rowIndex].Cells[CHECK_RESULT_COLUMN].Value = _failed;
                            recoveryForm.recoveryCheckReportDataGridView.Rows[_rowIndex].Cells[CORRECT_ACTION_COLUMN].Value = message;
                            _rowIndex++;
                        }
                        if (h.recoveryOperation == SPECIFIC_TAG_AT_TIME_RECOVERY)
                        {
                            Trace.WriteLine(DateTime.Now + " \t Printing the host and display name " + h.hostname + " " + h.displayname);
                            message = "A common consistency tag is unavailable at specific time.Please choose any one of the following options." + Environment.NewLine
                                    + "1. Use latest tag option for this vm from the wizard and try to recover" + Environment.NewLine
                                    + "2. Use latest time option for this vm from the wizard and try to recover";
                            recoveryForm.recoveryCheckReportDataGridView.Rows.Add(1);
                            recoveryForm.recoveryCheckReportDataGridView.Rows[_rowIndex].Cells[CHECK_REPORT_SERVER_NAME_COLUMN].Value = h.displayname;
                            recoveryForm.recoveryCheckReportDataGridView.Rows[_rowIndex].Cells[CHECK_REPORT_TAG_COLUMN].Value = "Verifying tag";
                            recoveryForm.recoveryCheckReportDataGridView.Rows[_rowIndex].Cells[CHECK_RESULT_COLUMN].Value = _failed;
                            recoveryForm.recoveryCheckReportDataGridView.Rows[_rowIndex].Cells[CORRECT_ACTION_COLUMN].Value = message;
                            _rowIndex++;
                        }
                        if (h.recoveryOperation == SPECIFIC_TIME_RECOVERY)
                        {
                            Trace.WriteLine(DateTime.Now + " \t Printing the host and display name " + h.hostname + " " + h.displayname);
                            message = "This VM can't be recovered." + Environment.NewLine
                                        + " Selected time range is not available for recovery.";
                                      
                            recoveryForm.recoveryCheckReportDataGridView.Rows.Add(1);
                            recoveryForm.recoveryCheckReportDataGridView.Rows[_rowIndex].Cells[CHECK_REPORT_SERVER_NAME_COLUMN].Value = h.displayname;
                            recoveryForm.recoveryCheckReportDataGridView.Rows[_rowIndex].Cells[CHECK_REPORT_TAG_COLUMN].Value = "Verifying time";
                            recoveryForm.recoveryCheckReportDataGridView.Rows[_rowIndex].Cells[CHECK_RESULT_COLUMN].Value = _failed;
                            recoveryForm.recoveryCheckReportDataGridView.Rows[_rowIndex].Cells[CORRECT_ACTION_COLUMN].Value = message;
                            _rowIndex++;
                        }
                        if (h.recoveryOperation == SPECIFIC_TAG)
                        {
                            Trace.WriteLine(DateTime.Now + " \t Printing the host and display name " + h.hostname + " " + h.displayname);
                            message = "Specified Tag is unavailable choose any one of the following options." + Environment.NewLine
                                    + "1. Use latest tag option for this vm from the wizard and try to recover" + Environment.NewLine
                                    + "2. Use latest time option for this vm from the wizard and try to recover";
                            recoveryForm.recoveryCheckReportDataGridView.Rows.Add(1);
                            recoveryForm.recoveryCheckReportDataGridView.Rows[_rowIndex].Cells[CHECK_REPORT_SERVER_NAME_COLUMN].Value = h.displayname;
                            recoveryForm.recoveryCheckReportDataGridView.Rows[_rowIndex].Cells[CHECK_REPORT_TAG_COLUMN].Value = "Verifying tag";
                            recoveryForm.recoveryCheckReportDataGridView.Rows[_rowIndex].Cells[CHECK_RESULT_COLUMN].Value = _failed;
                            recoveryForm.recoveryCheckReportDataGridView.Rows[_rowIndex].Cells[CORRECT_ACTION_COLUMN].Value = message;
                            _rowIndex++;
                        }
                    }
                }
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: CheckReportTable " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            return true;
        }
        /*
         * this is the code for the old flow.Now we are not going to use this code.
         * With this code we can get the Daetaisl form the Target vSphere host.
         */
        internal bool GetDetailsButtonClickEventForRecovery(AllServersForm allServersForm)
        {
            allServersForm.selectAllCheckBoxForRecovery.Visible = false;
            allServersForm.recoverDataGridView.Visible = false;
            allServersForm.recoveryGetDetailsButton.Enabled = false;
            allServersForm.tagPanel.Visible = false;
            allServersForm.recoveryPlanNameTextBox.Visible = false;
            allServersForm.recoveryPlanNameLabel.Visible = false;
            allServersForm.recoveryPreReqsButton.Visible = false;
            allServersForm.vmsSelectedRecovery = new HostList();
            allServersForm.recoveryChecksPassedList = new HostList();
            allServersForm.esxIPSelected = allServersForm.recoveryTargetESXIPTextBox.Text.Trim();
            allServersForm.tgtEsxUserName = allServersForm.targetEsxUserNameTextBox.Text.Trim();
            allServersForm.tgtEsxPassword = allServersForm.targetEsxPasswordTextBox.Text.Trim();
            string osType = allServersForm.recoveryOsComboBox.SelectedItem.ToString();
            allServersForm.osTypeSelected = (OStype)Enum.Parse(typeof(OStype), osType);
            allServersForm.progressBar.Visible = true;
            allServersForm.nextButton.Enabled = false;

            allServersForm.MasterFile = "ESX_Master_" + allServersForm.esxIPSelected + ".xml";

            try
            {
                if (File.Exists(allServersForm.installationPath + "\\" + allServersForm.MasterFile))
                {
                    File.Delete(allServersForm.installationPath + "\\" + allServersForm.MasterFile);
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: PostJobAutomation  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
            allServersForm.selectAllCheckBoxForRecovery.Checked = false;
            allServersForm.recoverDataGridView.Rows.Clear();
            allServersForm.recoveryDetailsBackgroundWorker.RunWorkerAsync();
            return true;
        }

        /*
         * this is the code for the old flow.Now we are not going to use this code.
         * With this code we can get the Daetaisl form the Target vSphere host.
         */
        internal bool DoWorkOfRecoveryDetailsBackGroundWorker(AllServersForm allServersForm)
        {
            try
            {
                int returnValue = 1;
                if (allServersForm.VerfiyEsxCreds(allServersForm.esxIPSelected,"target") == false)
                {
                    Trace.WriteLine(DateTime.Now + "\t Failed to update esx meta data in CX ");
                    return false;
                }
                returnValue = allServersForm.esxSelected.DownloadFile(allServersForm.esxIPSelected, allServersForm.MasterFile);
                if (returnValue == 0)
                {
                    allServersForm.esxSelected.GetEsxInfo(allServersForm.esxIPSelected, Role.Secondary, allServersForm.osTypeSelected);
                }
                else if (returnValue == 2)
                {
                    if (File.Exists(_installPath+ "\\ESX_Master.lck"))
                    {
                        try
                        {
                            File.Delete(_installPath + "\\ESX_Master.lck");
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
                    if (allServersForm.VerfiyEsxCreds(allServersForm.esxIPSelected, "target") == false)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Failed to update esx meta data in CX");
                        return false;
                    }
                    returnValue = allServersForm.esxSelected.DownloadFile(allServersForm.esxIPSelected, "ESX_Master.lck");

                    if (returnValue == 0)
                    {
                        StreamReader sr1 = new StreamReader(_installPath + "\\ESX_Master.lck");
                        string firstLine1;
                        firstLine1 = sr1.ReadToEnd();
                        WinPreReqs win = new WinPreReqs("", "", "", "");
                        Host h = new Host();
                        h.hostname = firstLine1.Split('=')[0];
                        string cxIP = firstLine1.Split('=')[1];
                        win.MasterTargetPreCheck( h, cxIP);
                        h = WinPreReqs.GetHost;
                        if (h.machineInUse == true)
                        {
                            allServersForm.someMtUsingFile = true;
                            MessageBox.Show("Master target:" + h.hostname + " jobs are running on CX:" + cxIP, "Error", MessageBoxButtons.OK, MessageBoxIcon.Stop);
                        }
                        else
                        {
                            allServersForm.VerfiyEsxCreds(allServersForm.esxIPSelected, "target");
                            returnValue = allServersForm.esxSelected.DownloadFile(allServersForm.esxIPSelected, allServersForm.MasterFile);
                            if (returnValue == 0)
                            {
                                allServersForm.someMtUsingFile = false;
                                allServersForm.esxSelected.GetEsxInfo(allServersForm.esxIPSelected, Role.Primary, allServersForm.osTypeSelected);
                            }
                            else if (returnValue == 2)
                            {
                                allServersForm.someMtUsingFile = true;
                            }
                        }
                    }
                    else
                    {
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
        /*
         * this is the code for the old flow.Now we are not going to use this code.
         * With this code we will read the xml file which is in target vSphere Host.
         */
        internal bool RunWorkerCompletedOfRecoveryDetailsBackGroundWorker(AllServersForm allServersForm)
        {
            try
            {
                if (allServersForm.someMtUsingFile == false)
                {
                    if (File.Exists(allServersForm.installationPath + "\\" + allServersForm.MasterFile))
                    {
                        Debug.WriteLine("Came here  to read xml file");
                        allServersForm.recoveryGetDetailsButton.Enabled = true;
                        SolutionConfig cfg = new SolutionConfig();
                        allServersForm.sourceListByUser = new HostList();
                        allServersForm.masterHostAdded = new Host();
                        allServersForm.esxIPSelected = "";
                        allServersForm.cxIPretrived = "";
                        cfg.ReadXmlConfigFile(allServersForm.MasterFile,  allServersForm.sourceListByUser,  allServersForm.masterHostAdded,  allServersForm.esxIPSelected,  allServersForm.cxIPretrived);
                        Trace.WriteLine("reading file is completed");
                        allServersForm.sourceListByUser = SolutionConfig.list;
                        allServersForm.masterHostAdded = SolutionConfig.masterHosts;
                        allServersForm.esxIPSelected = SolutionConfig.EsxIP;
                        allServersForm.cxIPretrived = SolutionConfig.csIP;
                        int rowIndex = 0;
                        int hostCount = allServersForm.sourceListByUser._hostList.Count;
                        //p2vRecoverDataGridView.RowCount = _sourceList._hostList.Count;
                        if (hostCount > 0)
                        {
                            foreach (Host h in allServersForm.sourceListByUser._hostList)
                            {
                                //MessageBox.Show("Printning the os " + h.os + "   " + _osType);
                                if (h.os == allServersForm.osTypeSelected)
                                {
                                    if (h.failOver != "yes")
                                    {
                                        allServersForm.recoverDataGridView.Rows.Add(1);
                                        allServersForm.recoverDataGridView.Rows[rowIndex].Cells[Esx_RecoverPanelHandler.HOSTNAME_COLUMN].Value = h.hostname;
                                        allServersForm.recoverDataGridView.Rows[rowIndex].Cells[Esx_RecoverPanelHandler.DISPLAY_NAME_COLUMN].Value = h.displayname;
                                        // p2vRecoverDataGridView.Rows[rowIndex].Cells[2].Value = true;
                                        allServersForm.recoverDataGridView.Rows[rowIndex].Cells[Esx_RecoverPanelHandler.MASTERTARGET_DISPLAYNAME].Value = h.masterTargetDisplayName;
                                        rowIndex++;
                                    }
                                }
                            }
                            if (allServersForm.recoverDataGridView.RowCount == 0)
                            {
                                MessageBox.Show("No VMs found on " + allServersForm.esxIPSelected + " to recover ", "Recover", MessageBoxButtons.OK, MessageBoxIcon.Information);
                                allServersForm.selectAllCheckBoxForRecovery.Visible = false;
                                allServersForm.recoverDataGridView.Visible = false;
                                allServersForm.nextButton.Enabled = false;
                                allServersForm.tagPanel.Visible = false;
                                allServersForm.progressBar.Visible = false;
                            }
                            else
                            {
                                //selectAllCheckBoxForRecovery.Visible = true;
                                allServersForm.recoverDataGridView.Visible = true;
                                allServersForm.progressBar.Visible = false;
                                //getDetailsButton.Visible = false;
                                allServersForm.tagPanel.Visible = false;
                                // nextButton.Enabled = true;
                            }
                        }
                        else
                        {
                            MessageBox.Show("No VMs found on " + allServersForm.esxIPSelected + " to recover ", "Recover", MessageBoxButtons.OK, MessageBoxIcon.Information);
                            allServersForm.selectAllCheckBoxForRecovery.Visible = false;
                            allServersForm.recoverDataGridView.Visible = false;
                            allServersForm.nextButton.Enabled = false;
                            allServersForm.tagPanel.Visible = false;
                            allServersForm.progressBar.Visible = false;
                        }

                        if (allServersForm.osTypeSelected == OStype.Linux)
                        {
                            if (allServersForm.recoverDataGridView.RowCount > 0)
                            {
                                allServersForm.tagPanel.Visible = true;
                                // machineConfigurationLabel.Visible = false;
                                // ipText.Visible = false;
                                // ipLabel.Visible = false;
                                // subnetMaskLabel.Visible = false;
                                // subnetMaskText.Visible = false;
                                // defaultGateWayLabel.Visible = false;
                                // defaultGateWayText.Visible = false;
                                // dnsText.Visible = false;
                                // dnsLabel.Visible = false;
                                allServersForm.recoveryPreReqsButton.Visible = false;
                                allServersForm.userDefinedTimeRadioButton.Visible = false;
                            }
                        }
                    }
                    else
                    {
                        allServersForm.recoveryGetDetailsButton.Enabled = true;
                        allServersForm.progressBar.Visible = false;
                        if (AllServersForm.SuppressMsgBoxes == false)
                        {
                            MessageBox.Show("The given credentials might be wrong or unable to retrive information from the secondary vSphere server. ", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        }
                    }
                }
                else
                {
                    allServersForm.recoveryGetDetailsButton.Enabled = true;
                    allServersForm.progressBar.Visible = false;
                    MessageBox.Show("Master target jobs are running on CX:" + WinTools.GetcxIP(), "Error", MessageBoxButtons.OK, MessageBoxIcon.Stop);
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

        /*
         * Recovery changes means user will select the different typs of tags for the Recovery.
         * When user selectins any option UI will call thismethod. Toassign particular option for 
         * Recovery/DR Drill.
         */
        internal bool RecoveryChanges(AllServersForm allServersForm)
        {
            try
            {
                // Debug.WriteLine("Printing ListIndex "+ listIndex);

                Host host = new Host();
                if (allServersForm.listIndexPrepared == 0)
                {
                    host.hostname = allServersForm.recoverDataGridView.Rows[0].Cells[Esx_RecoverPanelHandler.HOSTNAME_COLUMN].Value.ToString();
                    // Debug.WriteLine("Came here ");
                }
                else
                {
                    // Debug.WriteLine("came here");
                    host.hostname = allServersForm.currentDispName;
                }
                //Console.WriteLine("Printing list index" + listIndex);
                allServersForm.sourceListByUser.DoesHostExist(host, ref allServersForm.listIndexPrepared);
                //Debug.WriteLine("Printing list index " + listIndex + " " + host.hostname);
               
                foreach (Host tempHost in allServersForm.sourceListByUser._hostList)
                {
                    Host h1 = new Host();
                    h1 = tempHost;
                    if (tempHost.hostname == host.hostname)
                    {

                        if (allServersForm.tagBasedRadioButton.Checked == true)
                        {
                            tempHost.tag = "LATEST";
                            tempHost.recoveryOperation = 1;
                            tempHost.tagType = "FS";
                        }
                        else if (allServersForm.timeBasedRadioButton.Checked == true)
                        {
                            tempHost.tag = "LATESTTIME";
                            tempHost.recoveryOperation = 2;
                            tempHost.tagType = "FS";

                        }
                        else if (allServersForm.userDefinedTimeRadioButton.Checked == true)
                        {
                            tempHost.userDefinedTime = allServersForm.universalTime;
                            tempHost.recoveryOperation = 3;
                            Trace.WriteLine(DateTime.Now + " \t printing universal time " + allServersForm.universalTime);
                            tempHost.tagType = "FS";
                            tempHost.selectedTimeByUser = allServersForm.universalTime;
                            WinTools.SetHostTimeinGMT( h1);
                            h1 = WinTools.GetHost;
                            tempHost.userDefinedTime = h1.userDefinedTime;
                            
                        }
                        else if (allServersForm.specificTimeRadioButton.Checked == true)
                        {
                            
                            tempHost.userDefinedTime = allServersForm.universalTime;
                            tempHost.recoveryOperation = 4;
                            tempHost.tagType = "TIME";
                            tempHost.selectedTimeByUser = allServersForm.universalTime;
                            WinTools.SetHostTimeinGMT( h1);
                            h1 = WinTools.GetHost;
                            tempHost.userDefinedTime = h1.userDefinedTime;
                            tempHost.tag = h1.userDefinedTime;
                        }

                    }
                    else
                    {
                        Trace.WriteLine(DateTime.Now + " \t HostName is not matched");
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

        /*
         * Here the case is user has to select a machine form the table and 
         * They can select option for the recovery.
         * Here we will assign tye of operation seelcted by the user.
         */
        internal bool RevoverDataCellClickEvent(AllServersForm allServersForm, int rowIndex)
        {
            try
            {
                Host tempHost = new Host();
                //int listIndex = 0;
                if (rowIndex >= 0)
                {
                    if ((bool)allServersForm.recoverDataGridView.Rows[rowIndex].Cells[Esx_RecoverPanelHandler.RECOVER_CHECK_BOX_COLUMN].FormattedValue)
                    {

                    }

                    tempHost.hostname = allServersForm.recoverDataGridView.Rows[rowIndex].Cells[Esx_RecoverPanelHandler.HOSTNAME_COLUMN].Value.ToString();

                    allServersForm.sourceListByUser.DoesHostExist(tempHost, ref allServersForm.listIndexPrepared);
                    Host h = (Host)allServersForm.sourceListByUser._hostList[allServersForm.listIndexPrepared];
                    if (h.timeZone != null)
                    {
                        allServersForm.specificTimeRadioButton.Text = "Specific Time ( Source Time Zone: " + h.timeZone.Substring(0, 3) + ":" + h.timeZone.Substring(3, 2) + " hours" + ")";
                        allServersForm.userDefinedTimeRadioButton.Text = "Tag at Specified Time ( Source Time Zone: " + h.timeZone.Substring(0, 3) + ":" + h.timeZone.Substring(3, 2) + " hours" + ")";
                    }
                    allServersForm.listIndexPrepared = rowIndex;
                    // Debug.WriteLine("List Index" + _listIndex    + " " + h.hostname);
                    allServersForm.currentDispName = h.hostname;



                    //ipText.Clear();
                    //defaultGateWayText.Clear();
                    //subnetMaskText.Clear();
                    //dnsText.Clear();
                    // recoveryTagText.Text = h.tag;
                    // tagComboBox.SelectedItem = h.tagType;
                    if (h.newIP == null)
                    {

                        // ipText.Text = h.ip;
                    }
                    else
                    {
                        // ipText.Text = h.newIP;
                    }

                    if (h.recoveryOperation != 0)
                    {
                        if (h.recoveryOperation == 1)
                        {
                            allServersForm.tagBasedRadioButton.Checked = true;
                        }
                        else if (h.recoveryOperation == 2)
                        {
                            allServersForm.timeBasedRadioButton.Checked = true;
                        }
                        else if (h.recoveryOperation == 3)
                        {
                            allServersForm.userDefinedTimeRadioButton.Checked = true;
                            // dateTimePicker.Value = DateTime.Parse(h.userDefinedTime);
                            // DateTime dt = DateTime.Now;
                            // DateTime gmt = DateTime.Parse(dt.ToUniversalTime().ToString());
                            //dateTimePicker.Value = DateTime.Parse(gmt.ToString("yyyy/mm/dd HH:mm:ss"));
                            //recoveryTagText.Text = dt.ToUniversalTime().ToString();
                            // _universalTime = gmt.ToString("yyyy/MM/dd HH:mm:ss");
                        }
                        else if (h.recoveryOperation == 4)
                        {
                            allServersForm.specificTimeRadioButton.Checked = true;
                        }
                        else if (h.recoveryOperation == 5)
                        {
                            allServersForm.specificTagRadioButton.Checked = true;
                            if (h.tag != null)
                            {
                                allServersForm.specificTagTextBox.Text = h.tag;
                            }
                        }
                    }                   
                    if (allServersForm.userDefinedTimeRadioButton.Checked == true)
                    {
                        h.recoveryOperation = 3;
                        DateTime dateTime = DateTime.Now;
                        //dateTimePicker.Value = DateTime.Parse();
                        if (h.userDefinedTime != null)
                        {
                            DateTime convertedDate = DateTime.Parse(h.userDefinedTime);
                            Host h1 = new Host();
                            h1 = h;
                            // dt = TimeZone.CurrentTimeZone.ToLocalTime(convertedDate);
                            // h.userDefinedTime = dt.ToString();
                            WinTools.SetHostTimeinitslocal(h1);
                            h1 = WinTools.GetHost;
                            allServersForm.dateTimePicker.Text = h.selectedTimeByUser;
                            // allServersForm.dateTimePicker.Text = TimeZone.CurrentTimeZone.ToLocalTime(convertedDate).ToString();
                        }
                        else
                        {
                            DateTime gmt = DateTime.Parse(dateTime.ToString());
                            h.userDefinedTime = gmt.ToString("yyyy/MM/dd HH:mm:ss");
                            Trace.WriteLine(DateTime.Now + " \t  Printing datetime value " + h.userDefinedTime + " for the vm " + h.hostname);
                        }
                        h.tag = null;
                    }
                    else if (allServersForm.specificTimeRadioButton.Checked == true)
                    {
                        h.recoveryOperation = 4;
                        DateTime dt = DateTime.Now;
                        //dateTimePicker.Value = DateTime.Parse();
                        if (h.userDefinedTime != null)
                        {
                            DateTime convertedDate = DateTime.Parse(h.userDefinedTime);
                            Host h1 = new Host();
                            h1.timeZone = h.timeZone;
                            h1.userDefinedTime = h.userDefinedTime;
                            // dt = TimeZone.CurrentTimeZone.ToLocalTime(convertedDate);
                            // h.userDefinedTime = dt.ToString();
                            WinTools.SetHostTimeinitslocal(h1);
                            h1 = WinTools.GetHost;
                            allServersForm.specificTimeDateTimePicker.Text = h.selectedTimeByUser;
                            //allServersForm.specificTimeDateTimePicker.Text = TimeZone.CurrentTimeZone.ToLocalTime(convertedDate).ToString();
                        }
                        else
                        {
                            DateTime gmt = DateTime.Parse(dt.ToString());
                            h.userDefinedTime = gmt.ToString("yyyy/MM/dd HH:mm:ss");
                            Trace.WriteLine(DateTime.Now + " \t  Printing datetime value " + h.userDefinedTime + " for the vm " + h.hostname);
                        }
                        h.tag = h.userDefinedTime;
                    }
                    else if (allServersForm.tagBasedRadioButton.Checked == true)
                    {
                        h.recoveryOperation = 1;
                        h.userDefinedTime = null;
                        h.tag = "LATEST";
                    }
                    else if (allServersForm.timeBasedRadioButton.Checked == true)
                    {
                        h.recoveryOperation = 2;
                        h.userDefinedTime = null;
                        h.tag = "LATESTTIME";
                    }
                    else if (allServersForm.specificTagRadioButton.Checked == true)
                    {
                        h.recoveryOperation = 5;
                        h.userDefinedTime = null;
                        allServersForm.specificTagTextBox.Text = h.tag;
                    }
                    if (h.tagType == null)
                    {
                        // tagComboBox.Text = "FS";
                    }
                    //Trace.WriteLine(DateTime.Now + " \t Printing the values of " + h.defaultGateWay + " " + h.subNetMask + " " + h.dns);
                    if (h.defaultGateWay != null && h.defaultGateWay != "NULL")
                    {
                        if (h.defaultGateWay.Length > 3)
                        {
                            // defaultGateWayText.Text = h.defaultGateWay;
                        }
                        else
                        {
                            h.defaultGateWay = null;
                        }
                    }
                    else
                    {
                        //defaultGateWayText.Text = null;
                    }
                    if (h.subNetMask != null && h.subNetMask != "NULL")
                    {
                        if (h.subNetMask.Length > 3)
                        {
                            //subnetMaskText.Text = h.subNetMask;
                        }
                        else
                        {
                            h.subNetMask = null;
                        }
                    }
                    else
                    {
                        // subnetMaskText.Text = null;
                    }
                    if (h.dns != null && h.dns != "NULL")
                    {
                        if (h.dns.Length > 3)
                        {
                            // dnsText.Text = h.dns;
                        }
                        else
                        {
                            h.dns = null;
                        }
                    }
                    else
                    {
                        //dnsText.Text = null;
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


        internal bool RecoverDataGridViewCellValueChangesEvent(AllServersForm allServersForm, int rowIndex)
        {
            try
            {

                if (rowIndex >= 0)
                {
                    if (allServersForm.recoverDataGridView.RowCount > 0)
                    {
                        bool selectedOneVm = false;
                        for (int i = 0; i < allServersForm.recoverDataGridView.RowCount; i++)
                        {
                            if (!(bool)allServersForm.recoverDataGridView.Rows[i].Cells[Esx_RecoverPanelHandler.RECOVER_CHECK_BOX_COLUMN].FormattedValue)
                            {
                                selectedOneVm = false;
                            }
                            else
                            {
                                //allServersForm.recoveryPlanNameLabel.Visible = false;
                               // allServersForm.recoveryPlanNameTextBox.Visible = false;
                                //allServersForm.recoveryPlanNameTextBox.Clear();
                                allServersForm.nextButton.Enabled = false;
                                selectedOneVm = true;
                                break;
                            }
                        }
                        if (selectedOneVm == false)
                        {
                            
                            allServersForm.selectAllCheckBoxForRecovery.Visible = false;
                        }
                        for (int i = 0; i < allServersForm.recoverDataGridView.RowCount; i++)
                        {
                            if ((bool)allServersForm.recoverDataGridView.Rows[i].Cells[Esx_RecoverPanelHandler.RECOVER_CHECK_BOX_COLUMN].FormattedValue)
                            {
                                allServersForm.selectAllCheckBoxForRecovery.Visible = true;
                            }
                        }
                        if (allServersForm.recoverDataGridView.Rows[rowIndex].Cells[Esx_RecoverPanelHandler.RECOVER_CHECK_BOX_COLUMN].Selected == true)
                        {
                            if ((bool)allServersForm.recoverDataGridView.Rows[rowIndex].Cells[Esx_RecoverPanelHandler.RECOVER_CHECK_BOX_COLUMN].FormattedValue)
                            {
                               

                                    allServersForm.recoveryPreReqsButton.Visible = true;
                                    allServersForm.tagPanel.Visible = true;
                                
                                for (int i = 0; i < allServersForm.recoverDataGridView.RowCount; i++)
                                {
                                    if ((bool)allServersForm.recoverDataGridView.Rows[i].Cells[Esx_RecoverPanelHandler.RECOVER_CHECK_BOX_COLUMN].FormattedValue)
                                    {


                                        //e.RowIndex is the selected One. So will skip comparing with itself.
                                        if (i != rowIndex)
                                        {
                                            if (allServersForm.recoverDataGridView.Rows[i].Cells[Esx_RecoverPanelHandler.MASTERTARGET_DISPLAYNAME].Value.ToString() != allServersForm.recoverDataGridView.Rows[rowIndex].Cells[Esx_RecoverPanelHandler.MASTERTARGET_DISPLAYNAME].Value.ToString())
                                            {
                                                allServersForm.recoverDataGridView.Rows[rowIndex].Cells[Esx_RecoverPanelHandler.RECOVER_CHECK_BOX_COLUMN].Value = false;
                                                allServersForm.recoverDataGridView.RefreshEdit();
                                                MessageBox.Show("Machines from only one master target can be recovered at a time" +
                                                      Environment.NewLine + Environment.NewLine + "Please select machines from either " + "\"" + allServersForm.recoverDataGridView.Rows[i].Cells[Esx_RecoverPanelHandler.MASTERTARGET_DISPLAYNAME].Value.ToString() + "\"" + " or " + "\"" + allServersForm.recoverDataGridView.Rows[rowIndex].Cells[Esx_RecoverPanelHandler.MASTERTARGET_DISPLAYNAME].Value.ToString() + "\"", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);


                                                break;

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
         * In case of Dr drillwe will compare the diskname and check whether the controller types are null.
         * If null only we will update the controller type which is in the Info.xml. 
         */
        internal bool ComparingDiskForControllerCheck(AllServersForm recoveryForm)
        {
            try
            {
                
                ArrayList sourceDisks = new ArrayList();
               
                //Searching for the vmdk ....
                foreach (Host h in recoveryForm.recoveryChecksPassedList._hostList)
                {
                    sourceDisks = h.disks.GetDiskList;
                    foreach (Hashtable hash in sourceDisks)
                    {
                        string diskname = hash["Name"].ToString();
                        var pattern = @"\[(.*?)]";
                        var matches = Regex.Matches(diskname, pattern);
                        foreach (Match m in matches)
                        {
                            m.Result("");
                            diskname = diskname.Replace(m.ToString(), "");
                            Debug.WriteLine(DateTime.Now + " \t Printing vmdk names source side " + diskname);
                        }

                        h.sourceIsThere = true;
                        foreach (Host targetHost in recoveryForm.esxSelected.GetHostList)
                        {
                            if (recoveryForm.masterHostAdded.hostname != targetHost.hostname)
                            {
                                string hostname = recoveryForm.masterHostAdded.hostname.Split('.')[0];
                                ArrayList targetDiskHashList = new ArrayList();
                                targetDiskHashList = targetHost.disks.GetDiskList;
                                foreach (Hashtable targetDiskHash in targetDiskHashList)
                                {
                                    string diskNameOnTarget = targetDiskHash["Name"].ToString();
                                    var patternbrackets = @"\[(.*?)]";
                                    var searchMatches = Regex.Matches(diskNameOnTarget, patternbrackets);
                                    foreach (Match m in searchMatches)
                                    {
                                        m.Result("");
                                        diskNameOnTarget = diskNameOnTarget.Replace(m.ToString(), "");
                                        //Trace.WriteLine(DateTime.Now + " \t Printing vmdk names target side " + diskNameOnTarget);
                                    }
                                    if (diskname == diskNameOnTarget)
                                    {
                                        if (hash["controller_type"] == null )
                                        {
                                            hash["controller_type"] = targetDiskHash["controller_type"];
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
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: SelectedVmMasterTargetCredentials " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            return true;
        }




    }
}
