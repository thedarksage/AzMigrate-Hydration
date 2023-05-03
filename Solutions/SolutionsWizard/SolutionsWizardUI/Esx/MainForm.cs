using System;
using System.Collections.Generic;
using System.Collections;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;
using System.Diagnostics;
using System.IO;
using System.Net;
using com.InMage.ESX;
using Microsoft.Win32;
using com.InMage.Tools;

namespace com.InMage.Wizard
{
    
   
    

    public partial class MainForm : Form
    {
       
        private static int PROTECT_TEXT_LABEL_X = 83;
        private static int PROTECT_TEXT_LABEL_Y = 168;       
        private static int PROTECT_TEXT_LABEL_NEW_Y = 205;       
        private static int PROTECT_MORE_LABEL_X = 380;
       
        private static int PROTECT_MORE_LABEL_NEW_X = 375;
        private static int PROTECT_MORE_LABEL_NEW_Y = 177;
        public static string PROTECTION_ESX = "69";
        public static string RECOVER = "70";
        public static string FAIL_BACK = "71";
        public static string OFFLINE_SYNC = "72";
        public static string REMOVE = "73";
        public static string ADD_DISKS = "74";
        public static string _selectedItemInComboBox;
        public string _installPath;      
        public int returncodeForP2vEnabled = 1;
      //  public static HostList _selectedSourceList = new HostList();
        public  string _plan;
        public MainForm()
        {         
            InitializeComponent();
            try
            {               
                Trace.WriteLine("Os type " + Environment.OSVersion.Platform.ToString());
                Trace.WriteLine("Operearting System" + Environment.OSVersion.VersionString);
                GetIp();
               string osTypeOfVcontinuum =  WinPreReqs.VConBoxOs();

              
                SetStyle(ControlStyles.UserPaint, true);
                SetStyle(ControlStyles.AllPaintingInWmPaint, true);
                _installPath = WinTools.fxAgentPath() + "\\vContinuum";
                if (osTypeOfVcontinuum.Contains("2012"))
                {
                    //WinTools.Execute("cmd.exe",   "compact/C " + "\"" + _installPath + "\\UnifiedAgent.exe"+ "\"" ,600000);
                    PrepareAndExecuteBATFileforUnifiedAgent();
                }
                selectApplicationComboBox.Items.Add("ESX");
                selectApplicationComboBox.Visible = false;
                applicationLabel.Visible = false;
                esxProlectionLabel.Visible = true;
              //  if (WinTools.EnableP2v() == true)
                {
                    selectApplicationComboBox.Visible = true;
                    applicationLabel.Visible = true;
                    esxProlectionLabel.Visible = false;
                    forvSphereLabel.Visible = false;
                    tmLabel.Visible = false;
                    selectApplicationComboBox.Items.Add("P2V");
                }
                selectApplicationComboBox.SelectedIndex = 0;
                protectMoreLabel.Location = new Point(394, 181);
                offlineSyncMoreLabel.Location = new Point(496, 372);
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

        public static int GetIp()
        {
            try
            {
                string hostNameOfLocalMachine = null;
                //MessageBox.Show(PlatformID.Win32Windows.ToString() + osInfo.Version.Minor.ToString());
                hostNameOfLocalMachine = Environment.MachineName;
              
                // Then using host name, get the IP address list..
                IPHostEntry ipToGetByHostName = Dns.GetHostByName(hostNameOfLocalMachine);
              
                IPAddress[] ipAddress = ipToGetByHostName.AddressList;
              
                Trace.WriteLine(" ********************************Printing vCon IP & CX IP *************************");
                Trace.WriteLine(DateTime.Now + "\t Prinitng the vContinuum box name " + hostNameOfLocalMachine);
                for (int i = 0; i < ipAddress.Length; i++)
                {                 
                    Trace.WriteLine("vContinuum box ip: " + ipAddress[i].ToString());
                }
                Trace.WriteLine("Printing CX IP " + WinTools.getCxIp());

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
            return 0;
        }


        private bool PrepareAndExecuteBATFileforUnifiedAgent()
        {
            try
            {
                if (File.Exists(_installPath + "\\compressingFile.bat"))
                {
                    File.Delete(_installPath + "\\compressingFile.bat");
                }
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed to delete existing bat file compressingFile.bat " + ex.Message);
            }

            try
            {
                FileInfo f1 = new FileInfo(_installPath + "\\compressingFile.bat");
                StreamWriter sw = f1.CreateText();
                sw.WriteLine("echo off");
                sw.WriteLine("cd " + "\"" + _installPath + "\"");
                sw.WriteLine("compact/C " + "\"" + "UnifiedAgent.exe" + "\"");
                sw.WriteLine("echo %errorlevel%");
                sw.Close();

                WinTools.Execute("\"" + _installPath + "\\compressingFile.bat" + "\"", "", 6000000);
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed to compress the file " + ex.Message);
            }

            try
            {
                if(File.Exists(_installPath + "\\compressingFile.bat"))
                {
                    File.Delete(_installPath + "\\compressingFile.bat");
                }
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed to delete files " +  ex.Message);
            }


          
            return true;
        }


        public bool CheckPreReqs()
        {
            LaunchPreReqsForm launch = new LaunchPreReqsForm();            
            launch.Show();
            if (launch._preresPassed == false)
            {
                return false;
            }          
            return true;           
        }


        private bool cxReachable()
        {
           /* bool cxReachable = false;
            if (WinPreReqs.IpReachable(WinTools.getCxIp()) == false)
            {
                if (WinPreReqs.IpReachable(WinTools.getCxIp()) == false)
                {
                    cxReachable = false;
                }
                else
                {
                    cxReachable = true;
                }
            }
            else
            {
                cxReachable = true;
            }*/
            return true;
        }


        private void planBasedToolStripMenuItem_Click(object sender, EventArgs e)
        {
                 //calling the recovery form to recover...
                RecoveryForm recoverForm = new RecoveryForm();
                recoverForm.ShowDialog();            
        }

        private void ResetAllButtons()
        {
            newPlanRadioButton.Visible                  = false;
            addDiskToExistingPlanRadioButton.Visible    = false;
            addDiskToExistingPlanRadioButton.Checked    = false;
           // nextButton.Visible                          = false;         
            exportRadioButton.Visible                   = false;
            importRadioButton.Visible                   = false;
            protectGroupLabel.Visible                     = false;
            offlineSyncGroupPanel.Visible                 = false;
            exportRadioButton.Checked                   = false;
            importRadioButton.Checked                   = false;
           
        }       

        private void newPlanRadioButton_CheckedChanged(object sender, EventArgs e)
        {
            if (newPlanRadioButton.Checked == true)
            {
                newPlanRadioButton.Checked = false;
                AllServersForm allServersForm = new AllServersForm(selectApplicationComboBox.SelectedItem.ToString(), _plan);
                // this.Visible = false;
                allServersForm.ShowDialog();               
            }
        }        

        private void addDiskToExistingPlanRadioButton_CheckedChanged(object sender, EventArgs e)
        {
            if (addDiskToExistingPlanRadioButton.Checked == true)
            {

                if (selectApplicationComboBox.SelectedItem.ToString() == "P2V")
                {
                    _selectedItemInComboBox = "BMR Protection";
                }
                else if (selectApplicationComboBox.SelectedItem.ToString() == "ESX")
                {
                    _selectedItemInComboBox = "ESX";
                }
                AllServersForm allServersForm = new AllServersForm("additionOfDisk", _plan);
                addDiskToExistingPlanRadioButton.Checked = false;
                allServersForm.ShowDialog();
            }
        }   

        private void importRadioButton_Click(object sender, EventArgs e)
        {           
                if (importRadioButton.Checked == true)
                {
                    ImportOfflineSyncForm form = new ImportOfflineSyncForm();
                    importRadioButton.Checked = false;
                    form.ShowDialog();
                }           
        }

        private void exportRadioButton_Click(object sender, EventArgs e)
        {
            if (exportRadioButton.Checked == true)
            {
                if (selectApplicationComboBox.SelectedItem.ToString() == "P2V")
                {
                    _selectedItemInComboBox = "P2v";
                }
                else
                {
                    _selectedItemInComboBox = "Esx";
                }
                string appName = "offlineSync";               
                AllServersForm allServersForm = new AllServersForm(appName, _plan);
                allServersForm.p2vHeadingLabel.Text = "Offline Sync Export";
                exportRadioButton.Checked = false;
                allServersForm.ShowDialog();
            }        
        }        

        private void selectApplicationComboBox_SelectedIndexChanged(object sender, EventArgs e)
        {           
            if (selectApplicationComboBox.SelectedItem.ToString() == "P2V")
            {
                addDiskToExistingPlanRadioButton.Visible = false;
                //addDiskToExistingPlanRadioButton.Text = "Protect newly added disks of a protected server";
                protectGroupLabel.Visible = false;
                offlineSyncGroupPanel.Visible = false;
                addDiskToExistingPlanRadioButton.Checked = false;
                //addDiskToExistingPlanRadioButton.Visible    = false;
                newPlanRadioButton.Visible = false;
                newPlanRadioButton.Checked = false;
                importRadioButton.Visible = false;
                exportRadioButton.Visible = false;
                failBackLinkLabel.Enabled = false;
            }
            else
            {
                protectGroupLabel.Visible = false;
                offlineSyncGroupPanel.Visible = false;
                protectGroupLabel.Visible = false;
                failBackLinkLabel.Enabled = true;
                addDiskToExistingPlanRadioButton.Text = "Protect newly added disks of a protected VM";
            }
            protectLinkLabel.Location = new Point(83, 143);
            protectTextLabel.Location = new Point(PROTECT_TEXT_LABEL_X, PROTECT_TEXT_LABEL_Y);
            protectMoreLabel.Location = new Point(394, 181);
            recoverLinkLabel.Location = new Point(83, 218);
            recoverTextLabel.Location = new Point(83, 241);
            recoverMoreLabel.Location = new Point(730, 241);
            failBackLinkLabel.Location = new Point(83, 273);
            failBackTextLabel.Location = new Point(83, 296);
            failbackMoreLabel.Location = new Point(793, 309);
            offlineSyncLinkLabel.Location = new Point(83, 336);
            offlineSyncTextLabel.Location = new Point(83, 359);
            offlineSyncMoreLabel.Location = new Point(496, 372);            
            removeLinkLabel.Location = new Point(83, 421);
            removeTextLabel.Location = new Point(83, 445);
            removeMoreLabel.Location = new Point(330, 445);
            monitorlinkLabel.Location = new Point(83, 474);
            monitorDescriptionLabel.Location = new Point(83, 492);           
        }


        private void protectLinkLabel_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
        {          
           // protectLinkLabel.Location = new Point(83, 131);
            if (newPlanRadioButton.Visible == true && addDiskToExistingPlanRadioButton.Visible == true)
            {
                return;
            }
            ResetAllButtons();
            newPlanRadioButton.Visible = true;
            addDiskToExistingPlanRadioButton.Visible = true;
            protectGroupLabel.Location = new Point(83, 163);
            protectGroupLabel.Visible = true;
            protectTextLabel.Location = new Point(PROTECT_TEXT_LABEL_X, PROTECT_TEXT_LABEL_NEW_Y);
            protectMoreLabel.Location = new Point(381, 218);
            protectMoreLabel.BringToFront();
            protectMoreLabel.Visible = true;            
            protectTextLabel.Refresh();            
            if (selectApplicationComboBox.SelectedItem.ToString() == "P2V")
            {
                // addDiskToExistingPlanRadioButton.Visible = false;
                addDiskToExistingPlanRadioButton.Text = "Protect newly added disks of a protected server";
            }
            else
            {
                addDiskToExistingPlanRadioButton.Text = "Protect newly added disks of a protected VM";
            }                      
            recoverLinkLabel.Location = new Point(83, 243);
            recoverTextLabel.Location = new Point(83, 264);
            recoverMoreLabel.Location = new Point(730, 264);
            failBackLinkLabel.Location = new Point(83, 293);
            failBackTextLabel.Location = new Point(83, 316);
            failbackMoreLabel.Location = new Point(793, 329);
            offlineSyncLinkLabel.Location = new Point(83, 355);
            offlineSyncTextLabel.Location = new Point(83, 380);
            offlineSyncMoreLabel.Location = new Point(499, 393);             
            removeLinkLabel.Location = new Point(83, 421);
            removeTextLabel.Location = new Point(83, 445);
            removeMoreLabel.Location = new Point(330, 445);
            monitorlinkLabel.Location = new Point(83, 474);
            monitorDescriptionLabel.Location = new Point(83, 492);            
        }

        private void recoverLinkLabel_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
        {
            if (selectApplicationComboBox.SelectedItem.ToString() == "P2V")
            {
                _selectedItemInComboBox = "P2V";
            }
            else
            {
                _selectedItemInComboBox = "ESX";
            }
            AllServersForm allServersForm = new AllServersForm("recovery","");
            allServersForm.ShowDialog();
           /* RecoveryForm recovery = new RecoveryForm();    
            recovery.ShowDialog();*/
        }

        private void failBackLinkLabel_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
        {
            //ResetAllButtons();
            if (selectApplicationComboBox.SelectedItem.ToString() == "P2V")
            {
                //bmr protection is not supported by this solution....
                //MessageBox.Show(" BMR Protection is not supported for fail back", " ", MessageBoxButtons.OK, MessageBoxIcon.Stop);
               // failBackRadioButton.Checked = false;
            }
            else
            {
                string appName = "failBack";
                AllServersForm allServersForm = new AllServersForm(appName, _plan);
                allServersForm.ShowDialog();
            }
        }

        private void offlineSyncLinkLabel_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
        {            
            ResetAllButtons();        
            protectLinkLabel.Location = new Point(83, 143);
            protectTextLabel.Location = new Point(PROTECT_TEXT_LABEL_X, PROTECT_TEXT_LABEL_Y);
            protectMoreLabel.Location = new Point(394, 181);                  
            recoverLinkLabel.Location = new Point(83, 218);
            recoverTextLabel.Location = new Point(83, 241);
            recoverMoreLabel.Location = new Point(730, 241);
            failBackLinkLabel.Location = new Point(83, 273);
            failBackTextLabel.Location = new Point(83, 296);
            failbackMoreLabel.Location = new Point(793, 309);
            offlineSyncTextLabel.Location = new Point(83, 385);
            offlineSyncMoreLabel.Location = new Point(493, 399);
            offlineSyncLinkLabel.Location = new Point(83, 320);
            offlineSyncGroupPanel.Location = new Point(83, 340);
            removeLinkLabel.Location = new Point(83, 421);
            removeTextLabel.Location = new Point(83, 445);
            removeMoreLabel.Location = new Point(330, 445);
            monitorlinkLabel.Location = new Point(83, 474);
            monitorDescriptionLabel.Location = new Point(83, 492);
            offlineSyncGroupPanel.Visible = true;
            importRadioButton.Visible = true;
            exportRadioButton.Visible = true;    
            offlineSyncLinkLabel.Refresh();           
        }

        private void removeLinkLabel_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
        {
            RemovePairsForm remove = new RemovePairsForm();
            remove.ShowDialog();
        }

        private void protectMoreLabel_Click(object sender, EventArgs e)
        {
           // Process.Start("http://" + WinTools.getCxIpWithPortNumber() + "/help/Content/ESX Solution/Protect ESX.htm");
            if (File.Exists(_installPath + "\\Manual.chm"))
            {
               Help.ShowHelp(null, _installPath + "\\Manual.chm", HelpNavigator.TopicId, PROTECTION_ESX);
            }            
        }

        private void recoverMoreLabel_Click(object sender, EventArgs e)
        {
           // Process.Start("http://" + WinTools.getCxIpWithPortNumber() + "/help/Content/ESX Solution/Recover ESX.htm");
            if (File.Exists(_installPath + "\\Manual.chm"))
            {
                Help.ShowHelp(null, _installPath + "\\Manual.chm", HelpNavigator.TopicId, RECOVER);
          
            }           
        }

        private void failbackMoreLabel_Click(object sender, EventArgs e)
        {
            //Process.Start("http://" + WinTools.getCxIpWithPortNumber() + "/help/Content/ESX Solution/Failback Protection.htm");
            if (File.Exists(_installPath + "\\Manual.chm"))
            {
                Help.ShowHelp(null, _installPath + "\\Manual.chm", HelpNavigator.TopicId, FAIL_BACK);
            }
        }

        private void offlineSyncMoreLabel_Click(object sender, EventArgs e)
        {
            //Process.Start("http://" + WinTools.getCxIpWithPortNumber() + "/help/Content/ESX Solution/Offline Sync.htm");
            if (File.Exists(_installPath + "\\Manual.chm"))
            {
                Help.ShowHelp(null, _installPath + "\\Manual.chm", HelpNavigator.TopicId, OFFLINE_SYNC);
            }
        }

        private void removeMoreLabel_Click(object sender, EventArgs e)
        {
           // Process.Start("http://" + WinTools.getCxIpWithPortNumber() + "/help/Content/ESX Solution/Remove Protection.htm");
            if (File.Exists(_installPath + "\\Manual.chm"))
            {
               Help.ShowHelp(null, _installPath + "\\Manual.chm", HelpNavigator.TopicId, REMOVE);
            }
        }

        private void monitorlinkLabel_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
        {

            //ResumeForm monitor = new ResumeForm("Monitor");
            //monitor.ShowDialog();
           // Process.Start("http://" + WinTools.getCxIpWithPortNumber() + "/ui");
        }

        private void pushAgentLinkLabel_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
        {
            if (selectApplicationComboBox.SelectedItem.ToString() == "P2V")
            {
                _selectedItemInComboBox = "P2VPUSH";
            }
            else
            {
                _selectedItemInComboBox = "ESXPUSH";
            }
            AllServersForm allServersForm = new AllServersForm(_selectedItemInComboBox, _plan);
            // this.Visible = false;
            allServersForm.ShowDialog(); 
        }

        private void resumeRadioButton_CheckedChanged(object sender, EventArgs e)
        {
            if (resumeRadioButton.Checked == true)
            {
                if (selectApplicationComboBox.SelectedItem.ToString() == "P2V")
                {
                    _selectedItemInComboBox = "P2V";
                }
                else if (selectApplicationComboBox.SelectedItem.ToString() == "ESX")
                {
                    _selectedItemInComboBox = "ESX";
                }
                /*ResumeForm resume = new ResumeForm("Resume");
                resumeRadioButton.Checked = false;
                resume.ShowDialog();*/

            }
        }

        private void mainFormPanel_Paint(object sender, PaintEventArgs e)
        {

        }       
    }
}
