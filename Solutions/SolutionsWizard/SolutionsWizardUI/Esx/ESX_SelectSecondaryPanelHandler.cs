using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
//using System.Linq;
using System.Text;
using System.Windows.Forms;
using System.Diagnostics;
using System.Threading;
using System.Collections;
using Com.Inmage.Esxcalls;
using System.IO;
using Com.Inmage.Tools;
using System.Runtime.InteropServices;
using System.Runtime.Serialization;


namespace Com.Inmage.Wizard
{
    class Esx_SelectSecondaryPanelHandler : PanelHandler
    {


        internal string _esxHostIP = null;
        internal string _esxUserName = null;
        internal string _esxPassWord = null;
        Esx  _targetEsxInfo                                 = new Esx();
        int  _rowIndex                                      = 0;
        //public static int ESX_IP_COLUMN                   = 0;
        internal static int DISPLAY_NAME_COLUMN = 0;
        internal static int CHECKBOX_COLUMN = 1;
       // public static int POWER_STATUS_COLUMN             = 1;
        internal static int VMWARE_TOOLS_COLUMN = 2;
        internal static int NAT_IP_COLUMN = 3;
        internal bool powerOn;
        internal string ip = "", username = "", password = "", domain = "";
        Host _h;
        string vmName;
        internal static bool _useVmAsMT = false;
        internal int _powerOnReturnCode = 0;
        int _rowIndexofDataGridView = 0;
        bool _rerunBackGroundWorker = false;
        bool _popUpCancelled = false;
        bool _firstTimeEnteredScreen = false;
        int _getInfoReturnCode = 0;
        internal string _latestFilePath = null;
        internal string _skipData = null;
        public     Esx_SelectSecondaryPanelHandler()
        {
            //_state = 0;
        }

        /*
         * In case of vCenter  protection we will display same source vCenter details for target selection.
         * User can give new info if required.
         * Fo the first time we will fill the data into tree view same as source.
         */
        internal override bool Initialize(AllServersForm allServersForm)
        {
            Trace.WriteLine(DateTime.Now + " \t Entered: ESX_SelectSecondaryPanelHandler  initialzie");
            try
            {
                // While entering to this screen we will fill the help content 
                //if mt is not selected we will disable the next button.
                _latestFilePath = WinTools.FxAgentPath() + "\\vContinuum\\Latest";
                if (allServersForm.appNameSelected != AppName.Failback)
                {
                    if (_firstTimeEnteredScreen == false)
                    {
                        Host sourceHost = (Host)allServersForm.selectedSourceListByUser._hostList[0];
                        if (sourceHost.vCenterProtection == "Yes")
                        {
                            _firstTimeEnteredScreen = true;
                            _targetEsxInfo.ReadEsxConfigFile("Info.xml", Role.Secondary, sourceHost.esxIp);
                            
                            allServersForm.targetIpText.Text = sourceHost.esxIp;
                            allServersForm.targetUserNameText.Text = sourceHost.esxUserName;
                            allServersForm.targetPassWordText.Text = sourceHost.esxPassword;
                            _esxHostIP = sourceHost.esxIp;
                            _esxUserName = sourceHost.esxUserName;
                            _esxPassWord = sourceHost.esxPassword;
                            ReloadTargetTreeViewAfterGetDetails(allServersForm);
                        }
                    }
                }
                allServersForm.helpContentLabel.Text = HelpForcx.ProtectScreen2;
                allServersForm.slideOpenHelp = false;
                allServersForm.targetIpText.Select();
                allServersForm.nextButton.Enabled = false;
                allServersForm.previousButton.Visible = true;
                allServersForm.mandatoryLabel.Visible = true;
                if (allServersForm.selectedMasterListbyUser._hostList.Count != 0)
                {
                    allServersForm.nextButton.Enabled = true;
                }

                if (allServersForm.appNameSelected == AppName.Failback)
                {
                    
                    if (AdditionOfDiskSelectMachinePanelHandler.temperorySelectedSourceList != null && AdditionOfDiskSelectMachinePanelHandler.temperorySelectedSourceList._hostList.Count != 0)
                    {
                        Host h1 = (Host)AdditionOfDiskSelectMachinePanelHandler.temperorySelectedSourceList._hostList[0];
                        allServersForm.targetIpText.Text = h1.esxIp;
                        
                    }
                    else
                    {
                        allServersForm.targetIpText.Text = AdditionOfDiskSelectMachinePanelHandler.sourceEsxIPxml;
                    }
                    //allServersForm.targetIpText.ReadOnly = true;
                }
                //allServersForm.selectSecondaryHelpLabel.Text
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
            Trace.WriteLine(DateTime.Now + " \t Exiting: ESX_SelectSecondaryPanelHandler  initialzie");
            return true;
        }

        /*
         * This method will assign targetesxip,username,password to all the soruce selected machines.
         */
        internal override bool ProcessPanel(AllServersForm allServersForm)
        {
            // If linux protection we need to skip the push install screen so that here we will increase index...
            // we will assign the username and password to the master target which are given by the user to get details...
            Trace.WriteLine(DateTime.Now + " \t Entered: ESX_SelectSecondaryPanelHandler  ProcessPanel");
            try
            {
               
                //if (allServersForm._osType != OStype.Windows)
                //{
                //    ((System.Windows.Forms.PictureBox)allServersForm._pictureBoxList[allServersForm._taskListIndex - 1]).Image = allServersForm._completeTask;
                //    allServersForm._index++;
                //    allServersForm._taskListIndex++;
                //}
               // _allServersForm = allServersForm;
                foreach (Host h in allServersForm.selectedMasterListbyUser._hostList)
                {
                    h.dataStoreList = _targetEsxInfo.dataStoreList;
                    h.lunList = _targetEsxInfo.lunList;
                    h.configurationList = _targetEsxInfo.configurationList;
                    h.networkNameList = _targetEsxInfo.networkList;
                    h.resourcePoolList = _targetEsxInfo.resourcePoolList;
                    foreach (Network n in h.networkNameList)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Printing the values of the network " + n.Networkname);
                    }
                }

                foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                {
                    
                    h.targetESXPassword = _esxPassWord;
                    h.targetESXUserName = _esxUserName;
                }

                // allServersForm._selectedMasterList.Print();
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
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            Trace.WriteLine(DateTime.Now + " \t Exiting: ESX_SelectSecondaryPanelHandler  ProcessPanel");           
            return true;
        }
        
        /*
         * When user selected a Vm for master target we will call this method.
         * This will check for the Vm ware tools, hostname, ip etc set to ok or not.
         * And we will check whether user has selected same source vm as mt or not if so we will block user.
         * Once all the readiness checks are completed we will cek whether vCOn and mt are same of not.
         * If same we won;t ask for credentials. if not we will ask credentais and verify credentials are correct or not.
         * After that we will checks whnether any jobs are running on mt or not
         * In jobs are running we willblock user with messsage box.
         */
        internal bool MasterVmSelected(AllServersForm allServersForm, string displayName, string vSphereHostName)
        {
            try
            {
                // When user selected for the master target we will check for the hostname,ip, vmware tools, displayname
                // if everything is ok we will ask for the credentials.....
                //we will check that the given credentails are correct or not....
                //If not we will through one pop up from to re enter credentials or to skip the push install and the checks...
                // If vCOn hostname and mt hostname are same we wont ask for the credentials...

                Trace.WriteLine(DateTime.Now + " \t Entered: MasterVmSelected() of ESX_SelectSecondaryPanelHandler");
                string error = "";
                // Host h = new Host();
                //_rowIndexofDataGridView = rowIndex;
                if (displayName.Length <= 0)
                {
                    Trace.WriteLine(DateTime.Now + " \t P2vSelectSecondaryPanelHandler:MasterVmSelected: Display name is empty");
                    return false;
                }
                //To know the esx ip we added this logic
                //Trace.WriteLine("Printing the display names +++++++++++++ " + displayName);
                Host tempHost = new Host();
                tempHost.displayname = displayName;
                tempHost.vSpherehost = vSphereHostName;
                Trace.WriteLine("Printing tempHost display name", tempHost.displayname);
                int listIndex = 0;
                  //allServersForm._initialMasterList.Print();
                if (allServersForm.initialMasterListFromXml.DoesHostExist(tempHost, ref listIndex) == true)
                {
                    Trace.WriteLine(DateTime.Now + " \t Came here because selected host exists in initialmasterList");
                    _h = (Host)allServersForm.initialMasterListFromXml._hostList[listIndex];
                    //h.Print();
                    _esxHostIP = _h.esxIp;
                    _esxUserName = _h.esxUserName;
                    _esxPassWord = _h.esxPassword;
                    Debug.WriteLine("+++++++++++++++++++++++++ Printing Vm ware tools " + _h.displayname + _h.vmwareTools);
                    if (allServersForm.appNameSelected == AppName.Failback)
                    {
                        foreach (Host sourceHost in allServersForm.selectedSourceListByUser._hostList)
                        {
                            if (sourceHost.displayname == _h.displayname)
                            {
                                MessageBox.Show("Unable to use primary VM as master target ", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                return false;
                            }
                        }
                    }
                    foreach (Host sourceHost in allServersForm.selectedSourceListByUser._hostList)
                    {
                        if (sourceHost.ip == _h.ip)
                        {
                            MessageBox.Show("Select different master target. This VM cannot be selected as master target." + Environment.NewLine+ Environment.NewLine
                                + "Note:VM(s) choosen for protection cannot be selected as master target.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            return false;
                        }
                    }
                    if (_h.vmwareTools == "NotInstalled")
                    {
                        string message = _h.displayname + ": VM does not have VMware tools installed" +
                                             Environment.NewLine + "Please install VMware tools and try again";
                        MessageBox.Show(message, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return false;
                    }
                    else if (_h.vmwareTools == "NotRunning")
                    {

                        MessageBox.Show("VMware tools are not running on this VM. Please make sure that VMware tools are running and try again.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return false;
                    }
                    //else if (_h.vmwareTools == "OutOfDate")
                    //{
                    //    MessageBox.Show("VMware tools are out of date on this VM. Please update VMware tools on this VM: " + _h.displayname, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    //    return false;
                    //}
                    if (_h.ip == "NOT SET")
                    {
                        MessageBox.Show("Selected VM's IP address is not set", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return false;
                    }
                    if (_h.datacenter == null)
                    {
                        MessageBox.Show("Selected VM's datacenter value is null. Unable to use this VM as Master target", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return false;
                    }
                    if (_h.hostname == "NOT SET" || _h.hostname == null)
                    {
                        MessageBox.Show("Selected VM's hostname is not set try again by using refresh option ", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return false;
                    }
                    else
                    {
                        string[] words = _h.hostname.Split('.');
                        string hostnameOriginal = words[0];
                        if (hostnameOriginal.Length > 15)
                        {
                            MessageBox.Show("Master Target hostname should not be greater than 15 characters." + Environment.NewLine
                                + "Reduce hostname to less than 15 character and use refresh link.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            return false;
                        }
                    }
                    if (allServersForm.appNameSelected == AppName.Offlinesync)
                    {
                        Debug.WriteLine("Reached to verify master target");
                        foreach (Host tempHost1 in allServersForm.selectedSourceListByUser._hostList)
                        {
                            if (tempHost1.esxIp == _h.esxIp && tempHost1.displayname == _h.displayname)
                            {
                                MessageBox.Show("Same VM cannot be selected for as primary and as master target", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                return false;
                            }
                        }
                    }
                    if (_h.poweredStatus == "OFF")
                    {
                        vmName = _h.displayname;
                        switch (MessageBox.Show("The " + _h.displayname + " is powered off. In order to use it as master target, we need to power on the VM. Do you want to power on the VM now ?", "Power on VM",

                                    MessageBoxButtons.YesNo,
                                    MessageBoxIcon.Question))
                        {
                            case DialogResult.Yes:
                                powerOn = true;
                                allServersForm.progressBar.Visible = true;
                                allServersForm.nextButton.Enabled = false;
                                allServersForm.previousButton.Enabled = false;
                                allServersForm.p2v_SelectPrimarySecondaryPanel.Enabled = false;
                                allServersForm.selectSecondaryRefreshLinkLabel.Visible = false;
                                _targetEsxInfo.GetHostList.Clear();
                                allServersForm.targetBackgroundWorker.RunWorkerAsync();
                                break;
                            case DialogResult.No:
                                return false;
                            //break;
                        }
                    }
                    else
                    {
                        if (allServersForm.osTypeSelected == OStype.Windows)
                        {
                            if (!_h.operatingSystem.Contains("2003") && !_h.operatingSystem.Contains("2008") && !_h.operatingSystem.Contains("2012") && !_h.operatingSystem.ToUpper().Contains("WINDOWS 8"))
                            {
                                MessageBox.Show("Master target should be either windows 2003 or windows 2008", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                return false;
                            }
                            Trace.WriteLine(DateTime.Now + "\t Printing the vCon machine and mt hostnames " + Environment.MachineName + " mt " + _h.hostname.Split('0')[0]);
                            if (_h.hostname.Split('.')[0].ToUpper() != Environment.MachineName.ToUpper())
                            {
                               /* if (CommonTools.GetCredentials(allServersForm, ref _h.domain, ref _h.userName, ref _h.passWord) == false)
                                {
                                    Trace.WriteLine("MasterVMSelected: Returning false");
                                    return false;
                                }*/
                            }
                            Debug.WriteLine("Checking credentials of Master host:");
                            //h.Print();
                           // domain = _h.domain;
                           // username = _h.userName;
                           // password = _h.passWord;
                            ip = _h.ip;
                            allServersForm.progressBar.Visible = true;
                            allServersForm.nextButton.Enabled = false;
                            allServersForm.previousButton.Enabled = false;
                            allServersForm.p2v_SelectPrimarySecondaryPanel.Enabled = false;
                            // allServersForm.Enabled              = false;
                            allServersForm.MasterTargetBackgroundWorker.RunWorkerAsync();
                        }
                        else
                        {
                           /* if (CommonTools.GetCredentials(allServersForm, ref _h.domain, ref _h.userName, ref _h.passWord) == false)
                            {
                                Trace.WriteLine("MasterVMSelected: Returning false");
                                return false;
                            }*/
                            allServersForm.progressBar.Visible = true;
                            allServersForm.nextButton.Enabled = false;
                            allServersForm.previousButton.Enabled = false;
                            allServersForm.MasterTargetBackgroundWorker.RunWorkerAsync();
                            //allServersForm._selectedMasterList.AddOrReplaceHost(_h);
                            //allServersForm.nextButton.Enabled = true;
                        }
                    }
                }
                else
                {
                    Trace.WriteLine(DateTime.Now + "\t Came here because selected machine is not there in the initialMasterList " + tempHost.displayname);
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
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            Trace.WriteLine(DateTime.Now + " \t Exiting: MasterVmSelected() of ESX_SelectSecondaryPanelHandler");
            return true;
        }

        /*
         * This will be called by the MasterTargetBackgroundWorker_DoWork.
         * Here we will check that give n credentails by user are correct or not.
         * In this only we will check for the vcon + mt are same machine or not.
         * User may skip the credentials checks if wmi won't works fine or any security issues.
         */

        internal bool BackGroundWorkerForMasterTargetPreReq(AllServersForm allServersForm)
        {
            try
            {
                // Once user gave the credentails we will check that given credentails are corrent or not by using wmi...
                // if it fails we will try  for two times and we will error out....
                // If it contains multiple ips we will check which ip is pingable and we will try to connect with that ip...

                Trace.WriteLine(DateTime.Now + " \t Entered: BackGroundWorkerForMasterTargetPreReq() of ESX_SelectSecondaryPanelHandler");
                bool ipReachable = false;
                string error = "";
                Trace.WriteLine(DateTime.Now + " \t Reached here to verify the given credentials are correct or not  ");
                //MessageBox.Show("Domain : " + _selectedHost.ip + _selectedHost.userName + _selectedHost.passWord);
                _rerunBackGroundWorker = false;
                _popUpCancelled = false;
                string ip = null;
                if (_h.hostname.Split('.')[0].ToUpper() != Environment.MachineName.ToUpper())
                {

                    if (_h.ipCount > 1)
                    {
                        //Handling multiple ip addresses. A host can have couple of private of ip addresses and public ip addresses. This is for assigning right ip address to _selectedHost.Ip parameter

                        foreach (string ips in _h.IPlist)
                        {
                            Trace.WriteLine(DateTime.Now + " \t Trying to reach " + _h.displayname + " on " + ips);
                            if (WinPreReqs.IPreachable(ips) == true)
                            {
                                _h.ip = ips;
                                Trace.WriteLine(DateTime.Now + " \t Successfully reached " + _h.displayname + " on " + ips);
                                ipReachable = true;
                                break;
                            }
                        }
                        if (ipReachable == false)
                        {
                           /* for (int i = 0; i < allServersForm.selectMasterTargetTreeView.Nodes.Count; i++)
                            {
                                allServersForm.secondaryServerDataGridView.Rows[i].Cells[CHECKBOX_COLUMN].Value = false;
                                allServersForm.secondaryServerDataGridView.RefreshEdit();
                            }*/
                            _popUpCancelled = true;
                            error = Environment.NewLine + _h.displayname + " is not reachable. Please check that firewall is not blocking ";
                            string message = _h.displayname + " is not reachable on below IP Addresses" + Environment.NewLine + _h.ip + Environment.NewLine + "Please check that firewall is not blocking ";
                            MessageBox.Show(message, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            return false;
                        }
                    }
                    ip = _h.ip;
                }
                else
                {
                    ip = ".";
                }
                if (allServersForm.osTypeSelected == OStype.Windows)
                {
                    if (WinPreReqs.WindowsShareEnabled(ip, _h.domain, _h.userName, _h.passWord, _h.hostname,  error) == false)
                    {
                        error = WinPreReqs.GetError;
                        Trace.WriteLine(DateTime.Now + "\t came here because wmi fails to connect to mt");
                        //MessageBox.Show(error, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        //MessageBox.Show("Could not access access C drive of " + ip + " as remote share " + Environment.NewLine + "Please check that" + Environment.NewLine + "1. Credentials are correct " + "\n" + " 2. NetLogon and Server services are up and running on remote machine");
                        WmiErrorMessageForm errorMessageForm = new WmiErrorMessageForm(error);
                        errorMessageForm.userNameText.Text = _h.userName;
                        errorMessageForm.passWordText.Text = _h.passWord;
                        errorMessageForm.domainText.Text = _h.domain;
                        errorMessageForm.ShowDialog();
                        if (errorMessageForm.continueWithOutValidation == true)
                        {
                            _popUpCancelled = false;
                            Trace.WriteLine(DateTime.Now + " USer selets for skip of wmi calls");
                            _h.skipPushAndPreReqs = true;
                            allServersForm.selectedMasterListbyUser.AddOrReplaceHost(_h);
                        }
                        else if (errorMessageForm.canceled == true)
                        {
                            _popUpCancelled = true;
                            Trace.WriteLine(DateTime.Now + " User cancelled the wmi pop screen");
                            
                        }
                        else
                        {
                            _popUpCancelled = false;
                            Trace.WriteLine(DateTime.Now + "User selects for retry ");
                            _h.userName = errorMessageForm.username;
                            _h.passWord = errorMessageForm.password;
                            _h.domain = errorMessageForm.domain;
                            _rerunBackGroundWorker = true;
                        }
                        return false;
                    }
                    else
                    {
                        _popUpCancelled = false;
                        //MessageBox.Show("You can proceed by clicking next", "Information", MessageBoxButtons.OK, MessageBoxIcon.Information);
                        allServersForm.selectedMasterListbyUser.AddOrReplaceHost(_h);
                    }
                }
                else
                {
                    Trace.WriteLine(DateTime.Now + "\t came here because wmi fails to connect to mt linux " );
                    //if (WinTools.ConnectToLinux(_h.ip, _h.userName, _h.passWord) == false)
                    //{
                    //    if (WinTools.ConnectToLinux(_h.ip, _h.userName, _h.passWord) == false)
                    //    {
                    //        WmiErrorMessageForm errorMessageForm = new WmiErrorMessageForm("Failed to connect " + _h.ip + "( + " + _h.displayname + ")");
                    //        errorMessageForm.userNameText.Text = _h.userName;
                    //        errorMessageForm.passWordText.Text = _h.passWord;
                    //        errorMessageForm.domainText.Text = _h.domain;
                    //        errorMessageForm.ShowDialog();
                    //        if (errorMessageForm.continueWithOutValidation == true)
                    //        {
                    //            _popUpCancelled = false;
                    //            Trace.WriteLine(DateTime.Now + " USer selets for skip of wmi calls");
                    //            _h.skipPushAndPreReqs = true;
                    //            allServersForm._selectedMasterList.AddOrReplaceHost(_h);
                    //        }
                    //        else if (errorMessageForm._canceled == true)
                    //        {
                    //            _popUpCancelled = true;
                    //            Trace.WriteLine(DateTime.Now + " User cancelled the wmi pop screen");
                                
                    //        }
                    //        else
                    //        {
                    //            _popUpCancelled = false;
                    //            Trace.WriteLine(DateTime.Now + "User selects for retry ");
                    //            _h.userName = errorMessageForm.username;
                    //            _h.passWord = errorMessageForm.password;
                    //            _h.domain = errorMessageForm.domain;
                    //            _rerunBackGroundWorker = true;
                    //        }
                    //        return false;

                    //    }
                    //}
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
            Trace.WriteLine(DateTime.Now + " \t Exiting: BackGroundWorkerForMasterTargetPreReq() of ESX_SelectSecondaryPanelHandler");
            return true;
        }

        // Now this power on future is not there we can remove this logic...
        /*
         * This method is not using now because some times user is going to power on the protected vms
         * So we are displaying only VMs which are powered on.
         */
        internal bool PowerOnVm(AllServersForm allServersForm)
        {
            try
            {
                Trace.WriteLine(DateTime.Now + " \t Entered: PowerOnVm() of ESX_SelectSecondaryPanelHandler");
                _powerOnReturnCode = _targetEsxInfo.PowerOnVM(_esxHostIP, "\"" + vmName + "\"");
                Thread.Sleep(30000);
                _targetEsxInfo.GetEsxInfo(_esxHostIP,  Role.Secondary, allServersForm.osTypeSelected);
                Trace.WriteLine(DateTime.Now + " \t Exiting: PowerOnVm() of ESX_SelectSecondaryPanelHandler");
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

        /*
         * This method will be called when user click on the get details button in the Master target Screen.
         * this will be called by the background worker do_work(targetBackgroundWorker_DoWork).
         */
        internal bool GetEsxInfo(AllServersForm allServersForm)
        {
            try
            {
                //Once the user gave the credentails and asked to get credentials we will call th esx.pl script to generate masterxml.xml

                Trace.WriteLine(DateTime.Now + " \t Entered: GetEsxInfo() of ESX_SelectSecondaryPanelHandler");
                Cxapicalls cxApi = new Cxapicalls();
                if (cxApi. UpdaterCredentialsToCX(_esxHostIP, _esxUserName, _esxPassWord) == true)
                {
                    Trace.WriteLine(DateTime.Now + "\t Updated target esx creds to CX");
                }
                if (allServersForm.osTypeSelected == OStype.Solaris)
                {
                    _getInfoReturnCode = _targetEsxInfo.GetEsxInfo(_esxHostIP,  Role.Secondary, OStype.Linux);
                }
                else
                {
                    _getInfoReturnCode = _targetEsxInfo.GetEsxInfo(_esxHostIP,  Role.Secondary, allServersForm.osTypeSelected);
                }
                Trace.WriteLine(DateTime.Now + " \t Exiting: GetEsxInfo() of ESX_SelectSecondaryPanelHandler");

                if (allServersForm.appNameSelected == AppName.Failback)
                { 
                    string listOfVmsToPowerOff = null;                   
                    foreach (Host source in allServersForm.selectedSourceListByUser._hostList)
                    {
                        foreach (Host target in _targetEsxInfo.GetHostList)
                        {
                            Trace.WriteLine(DateTime.Now + " \t ReloadDataGridViewAfterGetDetails : Esx_SelectsecondaryPanelHandler comparing the displaynames");
                            if (source.target_uuid != null)
                            {
                                if ((source.target_uuid == target.source_uuid))
                                {
                                    source.displayname = target.displayname;
                                    if (target.poweredStatus == "ON")
                                    {
                                        if (listOfVmsToPowerOff == null)
                                        {
                                            listOfVmsToPowerOff = "\"" + target.source_uuid;
                                        }
                                        else
                                        {
                                            listOfVmsToPowerOff = listOfVmsToPowerOff + "!@!@!" + target.source_uuid;
                                        }
                                    }
                                }
                            }
                            else if (source.displayname == target.displayname)
                            {
                                source.displayname = target.displayname;
                                if (target.poweredStatus == "ON")
                                {
                                    if (listOfVmsToPowerOff == null)
                                    {
                                        listOfVmsToPowerOff = "\"" + target.source_uuid;
                                    }
                                    else
                                    {
                                        listOfVmsToPowerOff = listOfVmsToPowerOff + "!@!@!" + target.source_uuid;
                                    }
                                }
                            }
                        }
                    }


                    if (listOfVmsToPowerOff == null)
                    {
                        RefreshHosts(allServersForm.selectedSourceListByUser);


                        foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                        {
                            Host tempHost1 = new Host();
                            tempHost1.hostname = h.hostname;
                            if (CheckinVMinCX(allServersForm, ref tempHost1) == true)
                            {
                                foreach (Hashtable hash in h.nicList.list)
                                {
                                    foreach (Hashtable tempHash in tempHost1.nicList.list)
                                    {
                                        if (hash["nic_mac"] != null && tempHash["nic_mac"] != null)
                                        {
                                            if (hash["nic_mac"].ToString().ToUpper() == tempHash["nic_mac"].ToString().ToUpper())
                                            {
                                                tempHash["network_name"] = hash["network_name"];
                                                break;
                                            }
                                        }
                                    }

                                }
                                h.nicList.list = tempHost1.nicList.list;
                                //allServersForm._selectedSourceList.AddOrReplaceHost(_selectedHost);
                            }
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
                return false;
            }
            return true;
        }



        internal bool CheckinVMinCX(AllServersForm allServersForm, ref  Host h)
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
                else
                {
                    h = WinPreReqs.GetHost;
                    Trace.WriteLine(DateTime.Now + "\t Failed to find vm in  cx " + h.displayname);

                    return false;
                }
                Host tempHost1 = new Host();
                tempHost1.inmage_hostid = h.inmage_hostid;
                Cxapicalls cxApi = new Cxapicalls();
                if (cxApi.Post( tempHost1, "GetHostInfo") == true)
                {
                    tempHost1 = Cxapicalls.GetHost;
                    h.nicList.list = tempHost1.nicList.list;
                    foreach (Hashtable hash in h.nicList.list)
                    {
                        foreach (Hashtable tempHash in tempHost1.nicList.list)
                        {
                            if (hash["nic_mac"] != null && tempHash["nic_mac"] != null)
                            {
                                if (hash["nic_mac"].ToString().ToUpper() == tempHash["nic_mac"].ToString().ToUpper())
                                {
                                    tempHash["network_name"] = hash["network_name"];
                                }
                            }
                        }
                    }
                    h.nicList.list = tempHost1.nicList.list;

                }
                else
                {

                    Trace.WriteLine(DateTime.Now + "\t Failed to find vm in  cx " + h.displayname);

                    return false;
                }

            }
            return true;
        }
        /*
         * This methiod wil check the vmx version and hosty version which are supported.
         * In case of p2v win2k8r2 should not be selected as source if target esx version is 3.5
         * In case of offline sync protection if source and target esx serves are same.
         * if displaynames aresmae for mt and source vms we need to block user here.
         * 
         */
        internal override bool ValidatePanel(AllServersForm allServersForm)
        {
            Trace.WriteLine(DateTime.Now + " \t Entered: ValidatePanel() of ESX_SelectSecondaryPanelHandler");
            try
            {
                // we wil check for the mt is selected or not 
                // In case of offilen sync we will check that both the mt and source are same or not
                // If same we need to error out this.....
                if (allServersForm.selectedMasterListbyUser._hostList.Count < 1)
                {
                    MessageBox.Show("Please select master target", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return false;
                }


                ///Below code is added to get the configured pairs from CX using CX API.
                ///This was the request by customer to check whether the source is protected to same master target
                ///First we will get the master target host id using cx cli call. 
                ///tempMtHost refers to Master target .
                ///For each source machine selected by user will compare with CX API.
                ///For each source mahcien we will check for host id. If it is null we will gethost id using cx cli call.
                ///Bug 29647 
                Host tempMtHost = (Host)allServersForm.selectedMasterListbyUser._hostList[0];
                string cxip = WinTools.GetcxIPWithPortNumber();
                WinPreReqs win = new WinPreReqs("", "", "", "");
                if (tempMtHost.inmage_hostid == null)
                {                    
                    if (tempMtHost.inmage_hostid == null)
                    {
                        win.SetHostIPinputhostname(tempMtHost.hostname,  tempMtHost.ip, cxip);
                        tempMtHost.ip = WinPreReqs.GetIPaddress;
                        if (win.GetDetailsFromcxcli( tempMtHost, cxip) == false)
                        {

                            MessageBox.Show("CX does not contain master target information", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            return false;
                        }
                    }
                    tempMtHost = WinPreReqs.GetHost;

                    if(tempMtHost.masterTarget == false)
                    {
                        MessageBox.Show("Scout Agent is installed on this server. It can't be selected as Master target. Uninstall Scout agent and installed in Master target mode", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return false;
                    }
                }
                else
                {
                   
                      win.SetHostIPinputhostname(tempMtHost.hostname, tempMtHost.ip, cxip);
                        tempMtHost.ip = WinPreReqs.GetIPaddress;

                   

                        if (win.GetDetailsFromcxcli(tempMtHost, cxip) == false)
                        {

                            MessageBox.Show("CX does not contain master target information", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            return false;
                        }
                   
                    tempMtHost = WinPreReqs.GetHost;

                    if (tempMtHost.masterTarget == false)
                    {
                        MessageBox.Show("Scout Agent is installed on this server. It can't be selected as Master target. Uninstall Scout agent and installed in Master target mode", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return false;
                    }
                }


                foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                {
                    Host tempHost = new Host();
                    tempHost.inmage_hostid = h.inmage_hostid;
                    tempHost.displayname = h.displayname;
                    tempHost.hostname = h.hostname;
                    foreach (Host srcHost in allServersForm.selectedSourceListByUser._hostList)
                    {
                        if (h.ip == tempMtHost.ip)
                        {
                            MessageBox.Show("Found same IP address for both master target and following source server: " + Environment.NewLine + srcHost.displayname + Environment.NewLine + "Currently this combination is not supported.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            return false;
                        }
                    }
                    if (tempHost.inmage_hostid == null)
                    {
                       
                        if (tempHost.inmage_hostid == null)
                        {
                            win.SetHostIPinputhostname(tempHost.hostname,  tempHost.ip, cxip);
                            tempHost.ip = WinPreReqs.GetIPaddress;
                            if (win.GetDetailsFromcxcli( tempHost, cxip) == true)
                            {
                                tempHost = WinPreReqs.GetHost;
                                h.inmage_hostid = tempHost.inmage_hostid;
                            }
                        }
                    }


                    Cxapicalls cxApi = new Cxapicalls();
                    cxApi.PostToGetPairsOFSelectedMachine( tempHost, tempMtHost.inmage_hostid);
                    tempHost = Cxapicalls.GetHost;
                    foreach (Hashtable hash in tempHost.disks.partitionList)
                    {

                        if (hash["SourceVolume"] != null && hash["TargetVolume"] != null)
                        {
                            MessageBox.Show("VM: " + tempHost.displayname + " is already protected to same Master Target("+ tempMtHost.displayname +"). You can choose another Master Target.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            return false;
                        }
                    }
                }
                foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                {
                    if (h.efi == true)
                    {
                        if (tempMtHost.hostversion.StartsWith("5"))
                        {
                            Trace.WriteLine(DateTime.Now + "\t Target esx version is 5 or greater than 5");

                        }
                        else
                        {
                            MessageBox.Show("EFI disks are supported from 5.0 or later vSphere version." + Environment.NewLine
                                + "Select secondary vSphere 5.0 or later versions", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            return false;
                        }
                        if (h.vmxversion.Contains("08"))
                        {
                            Trace.WriteLine(DateTime.Now + "\t already source vm is having vmx 08");
                        }
                        else
                        {
                            h.vmxversion = "vmx-08";
                            Trace.WriteLine(DateTime.Now + "\t vmx version is set to 08 for the vm " + h.displayname);
                        }
                    }
                   
                }

                if (allServersForm.appNameSelected == AppName.Offlinesync)
                {
                    foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                    {
                        foreach (Host masterHost in allServersForm.selectedMasterListbyUser._hostList)
                        {
                            if (h.displayname == masterHost.displayname && h.esxIp == masterHost.esxIp)
                            {
                                MessageBox.Show("Source VM and master taregt VM should not be same", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                return false;
                            }
                        }
                    }
                }
                else if (allServersForm.appNameSelected != AppName.Bmr) 
                {
                    foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                    {
                        foreach (Host mtHost in allServersForm.selectedMasterListbyUser._hostList)
                        {

                            if (h.vSpherehost == mtHost.vSpherehost && h.displayname == mtHost.displayname)
                            {
                                MessageBox.Show("Source VM and master taregt VM should not be same", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                return false;
                            }
                        }
                    }
                }
                if (allServersForm.appNameSelected == AppName.Bmr || (allServersForm.appNameSelected == AppName.Offlinesync && ResumeForm.selectedActionForPlan == "P2v"))
                {
                    foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                    {
                        if (h.operatingSystem == "Windows_2008_R2")
                        {
                            foreach (Host mtHost in allServersForm.selectedMasterListbyUser._hostList)
                            {
                                if (mtHost.hostversion.StartsWith("3.5"))
                                {
                                    MessageBox.Show("Source Server " + h.displayname + " is having windows 2008 r2 operating system. Target vSphere is 3.5 version" + Environment.NewLine +
                                        " Windows 2008 r2 is not supported in 3.5 versions", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                    return false;
                                }
                            }
                        }
                        if (h.operatingSystem == "Windows_2012")
                        {
                            //if (tempMtHost.hostversion.Contains("3.5"))
                            //{
                            //    MessageBox.Show("Source Server " + h.displayname + " is having windows 2012 operating system. Target vSphere is less than 5.1 version" + Environment.NewLine +
                            //            " Windows 2012 is supported only in vSphere 5.1 and later version", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            //    return false;
                            //}
                            // In case of P2V W2k12 protection.
                            // Only for 5.1 we are writting original alt guest id fir remaing targets we are writing w2k8r2's alt_guest_id
                            
                                h.alt_guest_name = "windows8Server64Guest";
                           

                            //if (!tempMtHost.hostversion.StartsWith("5.1") && !tempMtHost.hostversion.StartsWith("5.5"))
                            //{
                            //    MessageBox.Show("Source Server " + h.displayname + " is having windows 2012 operating system. Target vSphere is less than 5.1 version" + Environment.NewLine +
                            //           " Windows 2012 is supported only in vSphere 5.1 and later version", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            //    return false;
                            //}
                        }
                        if (h.operatingSystem == "Windows_8")
                        {
                            if (!tempMtHost.hostversion.StartsWith("5"))
                            {
                                MessageBox.Show("Source Server " + h.displayname + " is having windows 8 operating system. Target vSphere is less than 5.0 version" + Environment.NewLine +
                                       " Windows 8 is supported only in vSphere 5.0 and later version", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                return false;
                            }
                        }


                        if (h.operatingSystem.ToUpper().Contains("ENTERPRISE LINUX ENTERPRISE LINUX SERVER RELEASE"))
                        {
                            if (!tempMtHost.hostversion.StartsWith("5"))
                            {
                                MessageBox.Show("Source Server " + h.displayname + " is having OLE 5 operating system. Target vSphere is less than 5.0 version" + Environment.NewLine +
                                       " OLE 5 is supported only in vSphere 5.0 and later version", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                return false;
                            }
                        }

                        if (tempMtHost.hostversion.StartsWith("5.5"))
                        {
                            foreach (Hashtable hash in h.disks.list)
                            {
                                if ((long.Parse(hash["Size"].ToString())) >= 2147483648)
                                {
                                    h.vmxversion = "vmx-10";
                                }
                            }
                        }
                   
                    }
                    foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                    {
                        foreach (Host masterHost in allServersForm.selectedMasterListbyUser._hostList)
                        {
                            h.resourcepoolgrpname = masterHost.resourcepoolgrpname;
                            h.resourcePool = masterHost.resourcePool;
                            if (h.displayname == masterHost.displayname && h.ip == masterHost.ip)
                            {
                                MessageBox.Show("Source VM and master taregt VM should not be same", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                return false;
                            }
                        }
                    }
                    
                    foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                    {
                        if (tempMtHost.hostversion.StartsWith("3.5"))
                        {
                            h.vmxversion = "vmx-04";
                        }
                        else
                        {
                            h.vmxversion = "vmx-07";
                        }


                        if (h.operatingSystem == "Windows_8" || h.operatingSystem == "Windows_2012")
                        {
                            h.vmxversion = "vmx-08";
                        }
                        
                    }
                }
                else
                {
                    foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                    {
                        foreach (Host targetHost in allServersForm.selectedMasterListbyUser._hostList)
                        {
                            if (h.hostversion.StartsWith("4") && targetHost.hostversion.StartsWith("3.5"))
                            {
                                MessageBox.Show("Primary vSphere server version is " + Esx.Sourceversionnumber + " and secondary vSphere server version is " + Esx.Targetversionnumber
                                        + ". You can't protect from 4.0.to 3.5 and 4.1 to 3.5 versions ", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                return false;
                            }
                            if (h.hostversion.StartsWith("3.5") && targetHost.hostversion.StartsWith("5"))
                            {
                                MessageBox.Show("Primary vSphere server version is " + Esx.Sourceversionnumber + " and secondary vSphere server version is " + Esx.Targetversionnumber
                                        + ". You can't protect from 4.0.to 3.5 ,4.1 to 3.5 and 3.5 to 5.0 versions.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                return false;
                            }
                        }
                    }
                    Host mtHost = (Host)allServersForm.selectedMasterListbyUser._hostList[0];
                    foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                    {
                        if (h.vmxversion.Contains("07") && mtHost.hostversion.StartsWith("3.5"))
                        {
                            MessageBox.Show("Select primary and secondary configuration is not supported." +
                                Environment.NewLine + " Pelase select greater than 3.5 vSphere version as secondary", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            return false;
                        }
                        if (h.vmxversion.Contains("10") && (mtHost.hostversion.StartsWith("5.0") || mtHost.hostversion.StartsWith("4.0") || mtHost.hostversion.StartsWith("4.1")))
                        {
                            h.vmxversion = "vmx-07";
                        }
                        else if(h.vmxversion.Contains("10") && (mtHost.hostversion.StartsWith("5.1")))
                        {
                            h.vmxversion = "vmx-08";
                        }
                        if (h.operatingSystem.Contains("Oracle Linux 4/5/6"))
                        {
                            if (!mtHost.hostversion.StartsWith("5"))
                            {
                                MessageBox.Show("Source Server " + h.displayname + " is having OLE 5 operating system. Target vSphere is less than 5.0 version" + Environment.NewLine +
                                       " OLE 5 is supported only in vSphere 5.0 and later version", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                return false;
                            }
                        }      
                       
                    }
                    foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                    {
                        if (h.vmxversion.Contains("08") && mtHost.hostversion.StartsWith("3.5"))
                        {
                            MessageBox.Show("Select primary and secondary configuration is not supported." +
                                   Environment.NewLine + " Pelase select greater than 3.5 vSphere version as secondary server.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            return false;

                        }
                        else  if (h.vmxversion.Contains("09") && mtHost.hostversion.StartsWith("3.5"))
                        {
                            MessageBox.Show("Select primary and secondary configuration is not supported." +
                                   Environment.NewLine + " Pelase select greater than 3.5 vSphere version as secondary server.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            return false;

                        }
                        else if (h.vmxversion.Contains("08") && ( mtHost.hostversion.StartsWith("4.0") || mtHost.hostversion.StartsWith("4.1")))
                        {
                            h.vmxversion = "vmx-07";
                        }
                        else if (h.vmxversion.Contains("09") && (mtHost.hostversion.StartsWith("4.0") || mtHost.hostversion.StartsWith("4.1") || mtHost.hostversion.StartsWith("5.0")))
                        {
                            h.vmxversion = "vmx-07";
                        }

                        if (h.vmxversion.Contains("04") && (mtHost.hostversion.StartsWith("5.0")))
                        {
                            h.vmxversion = "vmx-07";
                        }                        
                    }

                 
                }



                //It will check for machines which are having 2012 os. 
                // after preparing list it will check for the target esx version 5.0
                //If 5.0 vContinuum will ask user for conformation whetehr having u1 installed or not. 
                //Bug 29861 


                Host mtHost1 = (Host)allServersForm.selectedMasterListbyUser._hostList[0];
                string vmList = null;

                foreach (Host h1 in allServersForm.selectedSourceListByUser._hostList)
                {
                    if (h1.operatingSystem.Contains("2012"))
                    {

                        if (vmList == null)
                        {
                            vmList = h1.displayname;
                        }
                        else
                        {
                            vmList = vmList + Environment.NewLine + h1.displayname;
                        }
                    }
                }
                if (mtHost1.hostversion.StartsWith("5.0"))
                {   

                    if (vmList != null)
                    {
                        switch (MessageBox.Show("Following servers contain(s) Windows 2012 operating systems " + vmList + Environment.NewLine +
                            Environment.NewLine + "Windows 2012 OS is supported from 5.0 Update1 onwards. Are you sure target vSphere has 5.0 update1 or later version?"+ Environment.NewLine +
                            Environment.NewLine + "Press Yes to Continue.." + 
                             Environment.NewLine + "Press No to Choose different MT(on different vSphere)", "vSphere Version",
                       MessageBoxButtons.YesNo,
                       MessageBoxIcon.Question))
                        {                           
                            case DialogResult.No:
                                Trace.WriteLine(DateTime.Now + "\t User selected that target 5.0 is not havingu1 installed");
                                return false;

                        }
                    }

                }
                else
                {
                    if (mtHost1.hostversion.StartsWith("3.5") || mtHost1.hostversion.StartsWith("4.0") || mtHost1.hostversion.StartsWith("4.1"))
                    {
                        if (vmList != null)
                        {
                            MessageBox.Show("Source Servers " + vmList + " is having windows 2012 operating system. Target vSphere is less than 5.1 version" + Environment.NewLine +
                                   " Windows 2012 is supported only in vSphere 5.1 and later version", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            return false;
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
                return false;
            }
            Trace.WriteLine(DateTime.Now + " \t Exiting: ValidatePanel() of ESX_SelectSecondaryPanelHandler");
            return true;
        }

        internal override bool CanGoToNextPanel(AllServersForm allServersForm)
        {
            Trace.WriteLine(DateTime.Now + " \t Entered: CanGoToNextPanel() of ESX_SelectSecondaryPanelHandler");
            return true;
        }

        internal override bool CanGoToPreviousPanel(AllServersForm allServersForm)
        {
            try
            {
                // While moving to the previous screen we need to change the help content...
                Trace.WriteLine(DateTime.Now + " \t Entered: CanGoToPreviousPanel() of ESX_SelectSecondaryPanelHandler");
                if (allServersForm.appNameSelected != AppName.Failback)
                {
                    allServersForm.previousButton.Visible = false;
                }
                allServersForm.nextButton.Enabled = true;
                allServersForm.helpContentLabel.Text = HelpForcx.ProtectScreen1;
                Trace.WriteLine(DateTime.Now + " \t Exiting: CanGoToPreviousPanel() of ESX_SelectSecondaryPanelHandler");
                if (allServersForm.appNameSelected == AppName.Failback)
                {
                    if (ResumeForm.selectedVMListForPlan._hostList.Count != 0)
                    {
                        allServersForm.previousButton.Visible = false;
                    }
                }

                if(allServersForm.appNameSelected == AppName.Bmr || allServersForm.appNameSelected == AppName.Failback)
                {
                    allServersForm.mandatoryLabel.Visible = false;
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

        //Gets target vms from the ESX servers and adds to the grid.
        //Called by P2v_AllServersForm::SecondaryServerPanelAddEsxButton_Click() event handler
        internal bool GetEsxGuestVmInfo(AllServersForm allServersForm)
        {

            // When user clicks on the get details button we will first check the ip username and password text box are filled or not 
            // once that is done we will call esx.pl script to generate masterxml.xml...
            Trace.WriteLine(DateTime.Now + " \t Entered: GetEsxGuestVmInfo() of ESX_SelectSecondaryPanelHandler");
            try
            {
                if (File.Exists(_latestFilePath+ "\\Info.xml"))
                {
                    File.Delete(_latestFilePath+ "\\Info.xml");
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
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: PostJobAutomation  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            try
            {
                //Check that IP address is valid
                if (allServersForm.targetIpText.Text.Trim().Length < 4)
                {
                    allServersForm.errorProvider.SetError(allServersForm.targetIpText, "Please enter secondary vSphere server ip ");
                    MessageBox.Show("Please enter secondary vSphere server ip", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return false;
                }
                else
                {
                    allServersForm.errorProvider.Clear();
                }
                if (allServersForm.targetUserNameText.Text.Trim().Length < 1)
                {
                    allServersForm.errorProvider.SetError(allServersForm.targetUserNameText, "Please enter secondary vSphere username ");
                    MessageBox.Show("Please enter secondary vSphere username", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return false;
                }
                else
                {
                    allServersForm.errorProvider.Clear();
                }
                if (allServersForm.targetPassWordText.Text.Trim().Length < 1)
                {
                    allServersForm.errorProvider.SetError(allServersForm.targetPassWordText, "Please enter secondary vSphere host password ");
                    MessageBox.Show("Please enter secondary vSphere host password", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return false;
                }
                else
                {
                    allServersForm.errorProvider.Clear();
                }
                //bug #no 12412 It shoulb be part of next smokianide request.
               /* if (allServersForm._appName == AppName.ESX)
                {
                    if (allServersForm._sourceEsxIP == allServersForm.targetIpText.Text.Trim())
                    {
                        MessageBox.Show(allServersForm._sourceEsxIP + " is selected as primary vSphere host. It cannot be used as Secondary host", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return false;
                    }
                }*/
                allServersForm.initialMasterListFromXml = new HostList();
                allServersForm.selectedMasterListbyUser = new HostList();
                allServersForm.previousButton.Enabled = false;
                allServersForm.nextButton.Enabled = false;
                allServersForm.selectSecondaryRefreshLinkLabel.Visible = false;
                allServersForm.secondaryServerPanelAddEsxButton.Enabled = false;
                allServersForm.selectMasterTargetTreeView.Visible = false;
                allServersForm.selectMasterTargetLabel.Visible = false;
                allServersForm.powerOffNoteLabel.Visible = false;
                allServersForm.starLabel.Visible = false;
                allServersForm.starforLabel.Visible = false;
                allServersForm.selectMasterTargetTreeView.Nodes.Clear();
                if (allServersForm.initialMasterListFromXml._hostList.Count > 0)
                {
                    foreach (Host h in allServersForm.initialMasterListFromXml._hostList)
                    {
                        if (h.esxIp == allServersForm.targetIpText.Text.Trim())
                        {
                            allServersForm.targetIpText.Clear();
                            MessageBox.Show("Secondary ESX server is already exist in the list.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            allServersForm.selectMasterTargetTreeView.Enabled = false;
                            return false;
                        }
                    }
                }
                //Clear prior selection of master target and prior ESX datastore valies. We support only one master target.
                _targetEsxInfo.GetHostList.Clear();
                _targetEsxInfo.dataStoreList.Clear();
                _targetEsxInfo.lunList.Clear();
                _targetEsxInfo.networkList.Clear();
                _targetEsxInfo.configurationList.Clear();
                //Just added clearing of data.
                _targetEsxInfo = new Esx();
                _esxHostIP = allServersForm.targetIpText.Text.Trim();
                _esxUserName = allServersForm.targetUserNameText.Text.Trim();
                _esxPassWord = allServersForm.targetPassWordText.Text;

                if (allServersForm.appNameSelected == AppName.Esx || allServersForm.appNameSelected == AppName.Bmr)
                {
                    foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                    {
                        if (h.targetDataStore != null)
                        {
                            h.targetDataStore = null;
                        }
                    }
                }
                allServersForm.selectSecondaryServerPictureBox.Visible = true;
                allServersForm.progressBar.Visible = true;
                Cursor.Current = Cursors.WaitCursor;
                allServersForm.secondaryPanelLinkLabel.Visible = false;
                allServersForm.targetBackgroundWorker.RunWorkerAsync();
                // allServersForm.progressBar.Visible = false;
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
            Trace.WriteLine(DateTime.Now + " \t Exiting: GetEsxGuestVmInfo() of ESX_SelectSecondaryPanelHandler");
            return true;
        }

        //Now this method is not using......
        /*
         * Power on Target vms itself is not there. So we won't com eto this method any more.
         */

        internal bool ReloadAfterPowerOn(AllServersForm allServersForm)
        {
            Trace.WriteLine(DateTime.Now + " \t Entered: ReloadAfterPowerOn() of ESX_SelectSecondaryPanelHandler");
            try
            {
                int hostCount = _targetEsxInfo.GetHostList.Count;
                _rowIndex = 0;
                if (hostCount > 0)
                {
                    allServersForm.selectMasterTargetTreeView.Nodes.Clear();
                    Debug.WriteLine("Reloading the vms into the data grid view ");
                    foreach (Host h1 in _targetEsxInfo.GetHostList)
                    {
                        if (h1.poweredStatus == "ON")
                        {
                            // h.datastore                                                                                 = null;

                            h1.machineType = "VirtualMachine";
                            if (allServersForm.selectMasterTargetTreeView.Nodes.Count == 0)
                            {
                                allServersForm.selectMasterTargetTreeView.Nodes.Add(h1.vSpherehost);
                                allServersForm.selectMasterTargetTreeView.Nodes[0].Nodes.Add(h1.displayname).Text = h1.displayname;
                                foreach (System.Web.UI.WebControls.TreeNode node in allServersForm.selectMasterTargetTreeView.Nodes)
                                {
                                    if (node.ChildNodes.Count > 0)
                                    {
                                        node.ShowCheckBox = false;
                                    }
                                }
                            }
                            else
                            {
                                bool esxExists = false;
                                foreach (TreeNode node in allServersForm.selectMasterTargetTreeView.Nodes)
                                {
                                    // Trace.WriteLine(DateTime.Now + " \t printing the node names " + node.Name + " plan name " + h.plan);
                                    if (node.Text == h1.vSpherehost)
                                    {
                                        esxExists = true;
                                        node.Nodes.Add(h1.displayname);
                                    }
                                }
                                if (esxExists == false)
                                {
                                    allServersForm.selectMasterTargetTreeView.Nodes.Add(h1.vSpherehost);
                                }
                            }
                            allServersForm.selectMasterTargetTreeView.CheckBoxes = true;
                            allServersForm.selectMasterTargetTreeView.Visible = true;
                            allServersForm.initialSourceListFromXml.AddOrReplaceHost(h1);
                            _rowIndex++;

                        }
                    }
                }
                if (_powerOnReturnCode != 0)
                {
                    MessageBox.Show("Unable to power on the VM", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
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
            Trace.WriteLine(DateTime.Now + " \t Exiting: ReloadAfterPowerOn() of ESX_SelectSecondaryPanelHandler");
            return true;
        }

        /*
         * After gettin details we will come to this method.
         * TargetBackGroundWorker_RunWorkCompleted will call this method.
         * Only powered on vms will be displayed in this treeview.
         * At time of Failback we willcheck whether acutal protected vms are powered on/off state. if they are in powered on we will
         * Ask user to power off the vm. If won't agree we will blcok user to continue.
         */
        internal bool ReloadTargetTreeViewAfterGetDetails(AllServersForm allServersForm)
        {
            try
            {
                // After getting details from the target esx server we will fill in the datagridview
                // we will display only powered on vm only .....
                // In case of failback we will check for the sourcevms and the datastore availability also...
                // We will check whether the source vms are powered on if powered on we will ask user fto shutdown or 
                // we will shutdown the vms by taking conformation from the user......

                Trace.WriteLine(DateTime.Now + " \t Entered: ReloadDataGridViewAfterGetDetails() of ESX_SelectSecondaryPanelHandler");
                int hostCount = _targetEsxInfo.GetHostList.Count;
              
                Debug.WriteLine("Printing the master vms\n");
                //  _targetEsxInfo.PrintVms();
                allServersForm.selectSecondaryServerPictureBox.Visible = false;
                allServersForm.targetEsxInfoXml = _targetEsxInfo;
                // Assign the list of VMS read from Secondary ESX to the initial list.
                // From this list, user would select Master vm
                //  allServersForm._initialMasterList = new HostList();
                //  allServersForm._initialMasterList._hostList = _targetEsxInfo.GetHostList();
                
                _rowIndex = 0;
                //currentForm.secondaryServerDataGridView.RowCount = hostCount;
                if (hostCount > 0)
                {
                    allServersForm.initialMasterListFromXml = new HostList();
                    allServersForm.selectedMasterListbyUser = new HostList();
                    allServersForm.selectMasterTargetTreeView.Nodes.Clear();
                    if (allServersForm.appNameSelected == AppName.Failback)
                    {
                        string listOfVmsToPowerOff = null;
                        Trace.WriteLine(DateTime.Now + " \t Came here to assign values to datagridview for failback : ReloadDataGridViewAfterGetDetails");
                        Esx esxInfo = new Esx();
                        Host masterHost = new Host();
                        if (ResumeForm.masterTargetList._hostList.Count != 0)
                        {
                            masterHost = (Host)ResumeForm.masterTargetList._hostList[0];
                        }
                        foreach (Host source in allServersForm.selectedSourceListByUser._hostList)
                        {
                            foreach (Host target in _targetEsxInfo.GetHostList)
                            {
                                Trace.WriteLine(DateTime.Now + " \t ReloadDataGridViewAfterGetDetails : Esx_SelectsecondaryPanelHandler comparing the displaynames");
                                if (source.target_uuid != null  )
                                {
                                    if (( source.target_uuid == target.source_uuid))
                                    {
                                        source.displayname = target.displayname;
                                        if (target.poweredStatus == "ON")
                                        {
                                            if (listOfVmsToPowerOff == null)
                                            {
                                                listOfVmsToPowerOff = "\"" + target.source_uuid;
                                            }
                                            else
                                            {
                                                listOfVmsToPowerOff = listOfVmsToPowerOff + "!@!@!" + target.source_uuid;
                                            }
                                        }
                                    }
                                }
                                else if ( source.displayname == target.displayname)
                                {
                                    source.displayname = target.displayname;
                                    if (target.poweredStatus == "ON")
                                    {
                                        if (listOfVmsToPowerOff == null)
                                        {
                                            listOfVmsToPowerOff = "\"" + target.source_uuid;
                                        }
                                        else
                                        {
                                            listOfVmsToPowerOff = listOfVmsToPowerOff + "!@!@!" + target.source_uuid;
                                        }
                                    }
                                }
                            }
                        }
                        if (listOfVmsToPowerOff != null)
                        {
                            listOfVmsToPowerOff = listOfVmsToPowerOff + "\"";


                            string message = "Some of the VM(s) selected for Failback protection is/are powered-on state" + Environment.NewLine +" on primary vSphere. Shall we shutdown the VM(s)";

                            MessageBoxForAll messages = new MessageBoxForAll(message, "question", "Power Off");
                            messages.Focus();
                            messages.BringToFront();
                            messages.ShowDialog();
                            if (messages.selectedyes == true)
                            {
                                Host target = (Host)_targetEsxInfo.GetHostList[0];
                                if (esxInfo.PowerOffVM(target.esxIp,  listOfVmsToPowerOff) != 0)
                                {
                                    MessageBox.Show("Unable to power off the VM: " + target.displayname + " shutdown VM and try again", "Power off ", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                    allServersForm.secondaryServerPanelAddEsxButton.Enabled = true;
                                    allServersForm.nextButton.Enabled = false;
                                    allServersForm.previousButton.Enabled = true;
                                    allServersForm.p2v_SelectPrimarySecondaryPanel.Enabled = true;
                                    allServersForm.Enabled = true;
                                    allServersForm.progressBar.Visible = false;
                                    return false;
                                }
                                else
                                {
                                    Thread.Sleep(3000);
                                    allServersForm.progressBar.Visible = true;
                                    Cursor.Current = Cursors.WaitCursor;
                                    _targetEsxInfo.GetHostList.Clear();
                                    allServersForm.targetBackgroundWorker.RunWorkerAsync();
                                    return true;
                                }
                            }
                            else
                            {
                                MessageBox.Show("With out power off source vm we can't proceed Exiting....");
                                //allServersForm._closeFormForcely = true;
                                //allServersForm.Close();
                                return false;
                            }

                           
                        }



                        foreach (Host source in allServersForm.selectedSourceListByUser._hostList)
                        {
                            foreach (Host target in _targetEsxInfo.GetHostList)
                            {
                                Trace.WriteLine(DateTime.Now + " \t ReloadDataGridViewAfterGetDetails : Esx_SelectsecondaryPanelHandler comparing the displaynames");

                                if ((source.target_uuid != null && source.target_uuid == target.source_uuid) || source.displayname == target.displayname)
                                {
                                    target.dataStoreList = _targetEsxInfo.dataStoreList;
                                    if (target.poweredStatus == "OFF")
                                    {
                                        source.displayname = target.displayname;

                                        foreach (Host mainFormHost in AdditionOfDiskSelectMachinePanelHandler.temperorySelectedSourceList._hostList)
                                        {
                                            foreach (DataStore d in target.dataStoreList)
                                            {
                                                if (mainFormHost.target_uuid != null)
                                                {
                                                    if (mainFormHost.target_uuid == source.source_uuid)
                                                    {

                                                        ArrayList physicalDisks;
                                                        ArrayList targetdisklist;
                                                        physicalDisks = mainFormHost.disks.GetDiskList;
                                                        targetdisklist = source.disks.GetDiskList;

                                                        foreach (Hashtable disk in physicalDisks)
                                                        {
                                                            if (disk["Protected"].ToString() == "yes")
                                                            {
                                                                string diskname = disk["Name"].ToString().Split('/')[1];
                                                                foreach (Hashtable diskHash in targetdisklist)
                                                                {
                                                                    string[] targetDisks = diskHash["Name"].ToString().Split('/');
                                                                    // this is to avoid the isses when user selects the foldername options.
                                                                    string targetDiskName = targetDisks[targetDisks.Length - 1];
                                                                    // MessageBox.Show(diskname + " Comparing to " + targetDiskName);
                                                                    if (diskname == targetDiskName)
                                                                    {
                                                                        source.targetDataStore = target.datastore;
                                                                        diskHash["IsThere"] = "yes";
                                                                        diskHash["SelectedDataStore"] = source.datastore;
                                                                    }
                                                                }
                                                            }
                                                        }
                                                        foreach (Hashtable diskHash in targetdisklist)
                                                        {
                                                            //MessageBox.Show("Came here for value os isthere " + diskHash["IsThere"].ToString());
                                                            if (diskHash["IsThere"].ToString() != "yes")
                                                            {
                                                                long sizeBytes = (long)long.Parse(diskHash["Size"].ToString());
                                                                long sizeGB = (long)sizeBytes / (1024 * 1024) + (sizeBytes % 1024 > 0 ? 1 : 0);
                                                                Trace.WriteLine(DateTime.Now + " \t\t Size wanted in datastore for particular disk name  " + diskHash["Name"].ToString() + " size is " + sizeGB.ToString());
                                                                Trace.WriteLine(DateTime.Now + " \t comparing datastore " + d.name + mainFormHost.datastore);
                                                                if (d.name == mainFormHost.datastore)
                                                                {
                                                                    Trace.WriteLine(DateTime.Now + " \t Prining freespace in datastore and wanted space " + d.freeSpace.ToString() + " " + sizeGB.ToString());
                                                                    if (d.freeSpace - sizeGB >= 0)
                                                                    {
                                                                        d.freeSpace = d.freeSpace - sizeGB;
                                                                        diskHash["SelectedDataStore"] = d.name;
                                                                    }
                                                                    else
                                                                    {
                                                                        diskHash["SelectedDataStore"] = d.name;
                                                                        //MessageBox.Show("The selected source does not have the free space on the target");
                                                                        allServersForm.previousButton.Visible = true;
                                                                        allServersForm.previousButton.Enabled = true;
                                                                        //return false;
                                                                    }
                                                                }
                                                            }
                                                        }

                                                    }
                                                }
                                                else if(mainFormHost.new_displayname == source.displayname)
                                                {
                                                    ArrayList physicalDisks;
                                                    ArrayList targetdisklist;
                                                    physicalDisks = mainFormHost.disks.GetDiskList;
                                                    targetdisklist = source.disks.GetDiskList;

                                                    foreach (Hashtable disk in physicalDisks)
                                                    {
                                                        if (disk["Protected"].ToString() == "yes")
                                                        {
                                                            string diskname = disk["Name"].ToString().Split('/')[1];
                                                            foreach (Hashtable diskHash in targetdisklist)
                                                            {
                                                                string[] targetDisks = diskHash["Name"].ToString().Split('/');
                                                                // this is to avoid the isses when user selects the foldername options.
                                                                string targetDiskName = targetDisks[targetDisks.Length - 1];
                                                                // MessageBox.Show(diskname + " Comparing to " + targetDiskName);
                                                                if (diskname == targetDiskName)
                                                                {
                                                                    source.targetDataStore = target.datastore;
                                                                    diskHash["IsThere"] = "yes";
                                                                    diskHash["SelectedDataStore"] = source.datastore;
                                                                }
                                                            }
                                                        }
                                                    }
                                                    foreach (Hashtable diskHash in targetdisklist)
                                                    {
                                                        //MessageBox.Show("Came here for value os isthere " + diskHash["IsThere"].ToString());
                                                        if (diskHash["IsThere"].ToString() != "yes")
                                                        {
                                                            long sizeBytes = (long)long.Parse(diskHash["Size"].ToString());
                                                            long sizeGB = (long)sizeBytes / (1024 * 1024) + (sizeBytes % 1024 > 0 ? 1 : 0);
                                                            Trace.WriteLine(DateTime.Now + " \t\t Size wanted in datastore for particular disk name  " + diskHash["Name"].ToString() + " size is " + sizeGB.ToString());
                                                            Trace.WriteLine(DateTime.Now + " \t comparing datastore " + d.name + mainFormHost.datastore);
                                                            if (d.name == mainFormHost.datastore)
                                                            {
                                                                Trace.WriteLine(DateTime.Now + " \t Prining freespace in datastore and wanted space " + d.freeSpace.ToString() + " " + sizeGB.ToString());
                                                                if (d.freeSpace - sizeGB >= 0)
                                                                {
                                                                    d.freeSpace = d.freeSpace - sizeGB;
                                                                    diskHash["SelectedDataStore"] = d.name;
                                                                }
                                                                else
                                                                {
                                                                    diskHash["SelectedDataStore"] = d.name;
                                                                    //MessageBox.Show("The selected source does not have the free space on the target");
                                                                    allServersForm.previousButton.Visible = true;
                                                                    allServersForm.previousButton.Enabled = true;
                                                                    //return false;
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }

                                        }
                                    }
                                }
                                else
                                {
                                    Trace.WriteLine("The display names are not matched");
                                }
                            }
                        }
                    }                   
                   //Adding icons to the powered on machiens and hosts..
                    foreach (Host h in _targetEsxInfo.GetHostList)
                    {
                        //ImageList list = new ImageList();
                        //list.Images.Add(allServersForm._host);
                        //list.Images.Add(allServersForm._poweron);
                        //list.Images.Add(allServersForm._powerOff);
                        //allServersForm.selectMasterTargetTreeView.ImageList = list;
                        h.esxUserName = _esxUserName;
                        h.esxPassword = _esxPassWord;
                        allServersForm.initialMasterListFromXml.AddOrReplaceHost(h);
                        h.machineType = "VirtualMachine";
                        if (h.poweredStatus == "ON")
                        {
                             // h.datastore                                                                                 = null;

                                
                                if (allServersForm.selectMasterTargetTreeView.Nodes.Count == 0)
                                {
                                    allServersForm.selectMasterTargetTreeView.Nodes.Add(h.vSpherehost);
                                    //allServersForm.selectMasterTargetTreeView.Nodes[0].ImageIndex = 0;
                                    allServersForm.selectMasterTargetTreeView.Nodes[0].Nodes.Add(h.displayname).Text = h.displayname;
                                    //allServersForm.selectMasterTargetTreeView.Nodes[0].Nodes[0].ImageIndex = 1;
                                    //allServersForm.selectMasterTargetTreeView.Nodes[0].Nodes[0].SelectedImageIndex = 1;
                                    HideCheckBoxFromTreeView(allServersForm.selectMasterTargetTreeView, allServersForm.selectMasterTargetTreeView.Nodes[0]);
                                }
                                else
                                {
                                    HideCheckBoxFromTreeView(allServersForm.selectMasterTargetTreeView, allServersForm.selectMasterTargetTreeView.Nodes[0]);
                                    bool esxExists = false;
                                    int nodeCount = 1;
                                    foreach (TreeNode node in allServersForm.selectMasterTargetTreeView.Nodes)
                                    {
                                        // Trace.WriteLine(DateTime.Now + " \t printing the node names " + node.Name + " plan name " + h.plan);
                                        if (node.Text == h.vSpherehost)
                                        {
                                            
                                            esxExists = true;
                                            node.Nodes.Add(h.displayname);
                                            int count = allServersForm.selectMasterTargetTreeView.Nodes.Count;
                                            int childNodesCount = allServersForm.selectMasterTargetTreeView.Nodes[count - 1].Nodes.Count;
                                            //allServersForm.selectMasterTargetTreeView.Nodes[count - 1].Nodes[childNodesCount - 1].SelectedImageIndex = 1;
                                            //allServersForm.selectMasterTargetTreeView.Nodes[count - 1].Nodes[childNodesCount - 1].ImageIndex = 1;
                                        }
                                    }
                                    if (esxExists == false)
                                    {
                                        allServersForm.selectMasterTargetTreeView.Nodes.Add(h.vSpherehost);
                                        int count = allServersForm.selectMasterTargetTreeView.Nodes.Count;
                                        allServersForm.selectMasterTargetTreeView.Nodes[count - 1].ImageIndex = 0;

                                        allServersForm.selectMasterTargetTreeView.Nodes[count - 1].Nodes.Add(h.displayname).Text = h.displayname;                                       
                                        int childNodesCount = allServersForm.selectMasterTargetTreeView.Nodes[count - 1].Nodes.Count;
                                       //allServersForm.selectMasterTargetTreeView.Nodes[count - 1].Nodes[childNodesCount - 1].ImageIndex = 1;
                                        //allServersForm.selectMasterTargetTreeView.Nodes[count - 1].Nodes[childNodesCount - 1].SelectedImageIndex = 1;
                                        
                                        
                                        HideCheckBoxFromTreeView(allServersForm.selectMasterTargetTreeView, allServersForm.selectMasterTargetTreeView.Nodes[count - 1]);
                                        
                                    }
                                }
                                
                                //allServersForm.selectMasterTargetTreeView.CheckBoxes = true;
                                allServersForm.selectMasterTargetTreeView.Visible = true;                                
                                _rowIndex++;
                            
                        }
                    }
                    allServersForm.progressBar.Visible = false;
                    
                }
                else
                {
                    if (File.Exists(_latestFilePath +"\\Info.xml"))
                    {
                        if (hostCount == 0)
                        {
                            if (AllServersForm.SuppressMsgBoxes == false)
                            {
                                MessageBox.Show("There are no vms to protect this os type " + allServersForm.osTypeSelected);
                            }
                            allServersForm.secondaryServerPanelAddEsxButton.Enabled = true;
                            allServersForm.previousButton.Enabled = true;
                            allServersForm.progressBar.Visible = false;
                            allServersForm.nextButton.Enabled = false;
                            allServersForm.previousButton.Enabled = true;
                            allServersForm.p2v_SelectPrimarySecondaryPanel.Enabled = true;
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
                                MessageBox.Show("Please check that IP address and credentials are correct.", " Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            }
                            else
                            {
                                MessageBox.Show("Could not retrieve guest info. Please check that IP address and credentials are correct.", " Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            }
                        }
                        allServersForm.progressBar.Visible = false;
                        allServersForm.secondaryServerPanelAddEsxButton.Enabled = true;
                        allServersForm.previousButton.Enabled = true;
                        allServersForm.p2v_SelectPrimarySecondaryPanel.Enabled = true;
                        allServersForm.Enabled = true;
                        return false;
                    }
                    allServersForm.Enabled = true;
                    allServersForm.p2v_SelectPrimarySecondaryPanel.Enabled = true;
                    Cursor.Current = Cursors.Default;
                    allServersForm.secondaryServerPanelAddEsxButton.Enabled = true;
                    allServersForm.previousButton.Enabled = true;
                    return false;
                }
                _skipData = null;
                try
                {
                    if (File.Exists(WinTools.FxAgentPath() + "\\vContinuum\\logs\\vContinuum_ESX.log"))
                    {
                        StreamReader sr = new StreamReader(WinTools.FxAgentPath() + "\\vContinuum\\logs\\vContinuum_ESX.log");
                        string skipData = sr.ReadToEnd();
                        if (skipData.Length != 0)
                        {
                            _skipData = skipData;
                            allServersForm.secondaryPanelLinkLabel.Visible = true;
                            allServersForm.errorProvider.SetError(allServersForm.secondaryPanelLinkLabel, "Skipped data");
                            //MessageBox.Show("Some of the data has been skipped form the vCenter/vSphere server." + Environment.NewLine +skipData, "Skipped Data for Server", MessageBoxButtons.OK, MessageBoxIcon.Information);
                        }
                        sr.Close();


                    }
                }
                catch (Exception ex)
                {
                    Trace.WriteLine(DateTime.Now + "\t Failed to read vContinuum_ESx.log " + ex.Message);
                }


                allServersForm.selectMasterTargetTreeView.Visible = true;
                allServersForm.selectSecondaryRefreshLinkLabel.Visible = true;
                allServersForm.starLabel.Visible = true;
                allServersForm.starforLabel.Visible = true;
                allServersForm.powerOffNoteLabel.Visible = true;
                allServersForm.secondaryServerPanelAddEsxButton.Enabled = true;
                //allServersForm.targetIpText.Clear(); ;
                //allServersForm.targetUserNameText.Clear(); ;
                //allServersForm.targetPassWordText.Clear();
                allServersForm.selectMasterTargetLabel.Visible = true;
                allServersForm.previousButton.Enabled = true;
                //allServersForm.nextButton.Enabled = true;
                allServersForm.secondaryServerPanelAddEsxButton.Enabled = true;
                allServersForm.Enabled = true;
                allServersForm.p2v_SelectPrimarySecondaryPanel.Enabled = true;
                Cursor.Current = Cursors.Default;
                Trace.WriteLine(DateTime.Now + " \t Exiting: ReloadDataGridViewAfterGetDetails() of ESX_SelectSecondaryPanelHandler");
                foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                {
                    for (int i = 0; i < allServersForm.selectMasterTargetTreeView.Nodes.Count; i++)
                    {
                        if (h.displayname == allServersForm.selectMasterTargetTreeView.Nodes[i].Text)
                        {
                            
                            
                            //allServersForm.selectMasterTargetTreeView.Nodes[i].EndEdit(true);
                            //allServersForm.selectMasterTargetTreeView.Nodes[i].BackColor = Color.Red;


                        }
                        for (int j = 0; j < allServersForm.selectMasterTargetTreeView.Nodes[i].Nodes.Count; j++)
                        {
                            if (h.displayname == allServersForm.selectMasterTargetTreeView.Nodes[i].Nodes[j].Text)
                            {
                                //allServersForm.selectMasterTargetTreeView.Nodes[i].Nodes[j].EndEdit(true) ;
                               // allServersForm.selectMasterTargetTreeView.Nodes[i].Nodes[j].BackColor = Color.Gray;
                               

                            }
                        }
                    }
                }

                allServersForm.selectMasterTargetTreeView.ExpandAll();
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


        // We can remove this method noww....
        /*
         * This code is not going to use any more.
         */ 
        private bool TargetEsxExists(AllServersForm allServersForm)
        {
            try
            {
                Trace.WriteLine(DateTime.Now + " \t Entered: TargetEsxExists() of ESX_SelectSecondaryPanelHandler");
                if (allServersForm.initialMasterListFromXml._hostList.Count != 0)
                {
                    foreach (Host h in allServersForm.initialMasterListFromXml._hostList)
                    {
                        if (h.esxIp == allServersForm.targetIpText.Text)
                        {
                            return false;
                        }
                    }
                }
                Trace.WriteLine(DateTime.Now + " \t Exiting: TargetEsxExists() of ESX_SelectSecondaryPanelHandler");
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
        /*
         * By clicking the Refresh link label in the second screen of protection.
         * We will get Info.xml with the credentails given by user first time.
         * We will use the existing credentials.  
         */
        internal bool SelectSecondaryRefreshLinkLabelClickEvent(AllServersForm allServersForm)
        {
            try
            {

                // If user clicks on the refresh link label we will call this method...
                // we will fetch again the info form the same esx server.....
                if (_esxHostIP != null && _esxUserName != null && _esxPassWord != null)
                {
                    allServersForm.initialMasterListFromXml = new HostList();
                    allServersForm.selectedMasterListbyUser = new HostList();
                    _targetEsxInfo.GetHostList.Clear();
                    allServersForm.previousButton.Enabled = false;
                    allServersForm.nextButton.Enabled = false;
                    allServersForm.selectSecondaryRefreshLinkLabel.Visible = false;
                    allServersForm.secondaryServerPanelAddEsxButton.Enabled = false;
                    allServersForm.selectMasterTargetTreeView.Visible = false;
                    allServersForm.selectMasterTargetLabel.Visible = false;
                    allServersForm.progressBar.Visible = true;
                    powerOn = false;
                    allServersForm.powerOffNoteLabel.Visible = false;
                    allServersForm.starLabel.Visible = false;
                    allServersForm.starforLabel.Visible = false;
                    allServersForm.selectMasterTargetTreeView.Nodes.Clear();
                    allServersForm.selectSecondaryServerPictureBox.Visible = true;
                    allServersForm.targetBackgroundWorker.RunWorkerAsync();
                }
                else
                {
                    MessageBox.Show("We don't have credential to retrive data can you enter credentials and Click Get Details button");
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

        //This method can be removed ...
        // we had changed the flow .....
      
        /*
         * This is the pre req check for the mt whether any fx jobs are running on mt or not 
         * If any jobs are running we will blck user to continue.
         * Using cxcli call we will check the fx jobs status.
         * We wil get true or false in the xml file.
         */
        internal bool MTPreCheck(AllServersForm allServersForm)
        {
            // Here we will check that is there any fx jobs running in this mt
            // If some jobs already running we need to block here......
            // This is to support the parallel esx protection....
            try
            {
                //WinPreReqs win = new WinPreReqs(_h.ip, _h.domain, _h.userName, _h.passWord);
                //if (win.MasterTargetPreCheck(ref _h, WinTools.GetcxIPWithPortNumber()) == true)
                //{
                //    if (_h.machineInUse == false)
                //    {
                //        _useVmAsMT = true;
                //        allServersForm._selectedMasterList.AddOrReplaceHost(_h);
                //    }
                //    else
                //    {
                //        _useVmAsMT = false;
                //        string message = "This VM can't be used as master target. Some Fx jobs are running on this master target." + Environment.NewLine
                //              + "Select another master target";
                //        MessageBox.Show(message, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                //        return false;
                //    }
                //}
                //else
                //{
                //    _useVmAsMT = true;
                //    allServersForm._selectedMasterList.AddOrReplaceHost(_h);
                //}

                _useVmAsMT = true;
                allServersForm.selectedMasterListbyUser.AddOrReplaceHost(_h);
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: MTPreCheck selectsecondaryPanelHandler.cs  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");


            }
            return true;
        }
        /*
         * One after verifying the credentials . MasterTargetBackgroundWorker_RunWorkerCompleted this will call this method.
         * This will check whether credentials worked fine or user may chosse for re tyr or they can
         */
        internal bool PostCredentialsCheck(AllServersForm allServersForm)
        {

            // Once the credentials check is done it will come to here 
            // whether to rerun or to not.....
            Trace.WriteLine(DateTime.Now + " \t Entered: PostCredentialsCheck of the Esx_AddSourcePanelHandler  ");
            try
            {
                if (_rerunBackGroundWorker == true)
                {
                    allServersForm.nextButton.Enabled = false;
                    allServersForm.previousButton.Enabled = false;
                    allServersForm.p2v_SelectPrimarySecondaryPanel.Enabled = false;
                    allServersForm.progressBar.Visible = true;
                    allServersForm.MasterTargetBackgroundWorker.RunWorkerAsync();
                }
                else
                {
                    if (_popUpCancelled == true || _useVmAsMT == false)
                    {
                        for (int i = 0; i < allServersForm.selectMasterTargetTreeView.Nodes.Count; i++)
                        {
                           // allServersForm.selectMasterTargetTreeView.Nodes[i].Checked = false;
                            for (int j = 0; j < allServersForm.selectMasterTargetTreeView.Nodes[i].Nodes.Count; j++)
                            {
                                allServersForm.selectMasterTargetTreeView.Nodes[i].Nodes[j].Checked = false;
                            }
                        }
                    }
                    allServersForm.progressBar.Visible = false;
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
            Trace.WriteLine(DateTime.Now + " \t Exiting: PostCredentialsCheck of the Esx_AddSourcePanelHandler  ");
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
        private bool RefreshHosts(HostList _selectedHostList)
        {
            try
            {



                string cxip = WinTools.GetcxIPWithPortNumber();
                foreach (Host h in _selectedHostList._hostList)
                {Host h1 = h;
                    if (h.inmage_hostid == null)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Found inmage uuid as null");
                        WinPreReqs win = new WinPreReqs("", "", "", "");
                        win.SetHostIPinputhostname(h1.hostname,  h1.ip, cxip);
                        h1.ip = WinPreReqs.GetIPaddress;
                        win.GetDetailsFromcxcli( h1, cxip);
                        h1 = WinPreReqs.GetHost;
                        h.inmage_hostid = h1.inmage_hostid;
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
                Trace.WriteLine(DateTime.Now + "\t Successfully post the request for all the hosts. Now going to sleep for 65 seconds ");
                Thread.Sleep(65000);
                foreach (Host h in _selectedHostList._hostList)
                {

                    Host tempHost = new Host();
                    tempHost.requestId = h.requestId;
                    Cxapicalls cxApis = new Cxapicalls();
                    if (cxApis.Post( tempHost, "GetRequestStatus") == true)
                    {
                        tempHost = Cxapicalls.GetHost;
                        Trace.WriteLine(DateTime.Now + "\t Successfully get request status " + h.displayname + " new displayname " + h.new_displayname);

                    }
                    else
                    {
                        Trace.WriteLine(DateTime.Now + "\t Going to sleep 1 minute for first time");
                        Thread.Sleep(65000);
                        if (cxApis.Post( tempHost, "GetRequestStatus") == true)
                        {
                            tempHost = Cxapicalls.GetHost;
                            Trace.WriteLine(DateTime.Now + "\t Successfully get request status " + h.displayname + " new displayname " + h.new_displayname);

                        }
                        else
                        {
                            Trace.WriteLine(DateTime.Now + "\t Going to sleep 1 minute for second time");
                            Thread.Sleep(65000);
                            if (cxApis.Post( tempHost, "GetRequestStatus") == true)
                            {
                                tempHost = Cxapicalls.GetHost;
                                Trace.WriteLine(DateTime.Now + "\t Successfully get request status " + h.displayname + " new displayname " + h.new_displayname);

                            }
                            else
                            {
                                Trace.WriteLine(DateTime.Now + "\t Going to sleep 1 minute for third time");
                                Thread.Sleep(65000);
                                if (cxApis.Post( tempHost, "GetRequestStatus") == true)
                                {
                                    tempHost = Cxapicalls.GetHost;
                                    Trace.WriteLine(DateTime.Now + "\t Successfully get request status " + h.displayname + " new displayname " + h.new_displayname);

                                }
                                else
                                {
                                    Trace.WriteLine(DateTime.Now + "\t Going to sleep 1 minute for fourth time");
                                    Thread.Sleep(65000);
                                    if (cxApis.Post( tempHost, "GetRequestStatus") == true)
                                    {
                                        tempHost = Cxapicalls.GetHost;
                                        Trace.WriteLine(DateTime.Now + "\t Successfully get request status " + h.displayname + " new displayname " + h.new_displayname);

                                    }
                                    else
                                    {
                                        Trace.WriteLine(DateTime.Now + "\t Going to sleep 1 minute for fifth time");
                                        Thread.Sleep(65000);
                                        if (cxApis.Post( tempHost, "GetRequestStatus") == true)
                                        {
                                            tempHost = Cxapicalls.GetHost;
                                            Trace.WriteLine(DateTime.Now + "\t Successfully get request status " + h.displayname + " new displayname " + h.new_displayname);
                                        }
                                        else
                                        {
                                            Trace.WriteLine(DateTime.Now + "\t Going to sleep 1 minute for sixth time");
                                            Thread.Sleep(65000);
                                            if (cxApis.Post( tempHost, "GetRequestStatus") == true)
                                            {
                                                tempHost = Cxapicalls.GetHost;
                                                Trace.WriteLine(DateTime.Now + "\t Successfully get request status " + h.displayname + " new displayname " + h.new_displayname);
                                            }
                                            else
                                            {
                                                Trace.WriteLine(DateTime.Now + "\t Going to sleep 1 minute for seventh time");
                                                Thread.Sleep(65000);
                                                if (cxApis.Post( tempHost, "GetRequestStatus") == true)
                                                {
                                                    tempHost = Cxapicalls.GetHost;
                                                    Trace.WriteLine(DateTime.Now + "\t Successfully get request status " + h.displayname + " new displayname " + h.new_displayname);
                                                }
                                                else
                                                {
                                                    Trace.WriteLine(DateTime.Now + "\t Going to sleep 1 minute for eighth time");
                                                    Thread.Sleep(65000);
                                                    if (cxApis.Post( tempHost, "GetRequestStatus") == true)
                                                    {
                                                        tempHost = Cxapicalls.GetHost;
                                                        Trace.WriteLine(DateTime.Now + "\t Successfully get request status " + h.displayname + " new displayname " + h.new_displayname);
                                                    }
                                                    else
                                                    {
                                                        Trace.WriteLine(DateTime.Now + "\t Going to sleep 1 minute for ninth time");
                                                        Thread.Sleep(65000);
                                                        if (cxApis.Post( tempHost, "GetRequestStatus") == true)
                                                        {
                                                            tempHost = Cxapicalls.GetHost;
                                                            Trace.WriteLine(DateTime.Now + "\t Successfully get request status " + h.displayname + " new displayname " + h.new_displayname);
                                                        }
                                                        else
                                                        {
                                                            Trace.WriteLine(DateTime.Now + "\t Going to sleep 1 minute for tenth time");
                                                            Thread.Sleep(65000);
                                                            if (cxApis.Post( tempHost, "GetRequestStatus") == true)
                                                            {
                                                                tempHost = Cxapicalls.GetHost;
                                                                Trace.WriteLine(DateTime.Now + "\t Successfully get request status " + h.displayname + " new displayname " + h.new_displayname);
                                                            }
                                                            else
                                                            {
                                                                Trace.WriteLine(DateTime.Now + "\t Continuing with out refresh after waiting for 10 mins");
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
    }
}

