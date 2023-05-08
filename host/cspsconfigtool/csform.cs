using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Net;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Runtime.InteropServices;
using Logging;
using System.Text.RegularExpressions;
using InMage.APIHelpers;
using System.Net.NetworkInformation;

namespace cspsconfigtool
{
    public partial class csform : Form
    {        
        /// <summary>
        /// List of values that the setup can return
        /// </summary>
        public enum DRConfigReturnValues
        {
            /// <summary>
            /// setup was successful
            /// </summary>
            Successful = 0,

            /// <summary>
            /// Setup failed
            /// </summary>
            Failed = 1,

            /// <summary>
            /// Command Line is not valid
            /// </summary>
            InvalidCommandLine = 2,

            /// <summary>
            /// Failed prereq checks
            /// </summary>
            FailedPrerequisiteChecks = 3,

            /// <summary>
            /// Performing invalid operation
            /// </summary>
            InvalidOperation = 4,

            /// <summary>
            /// Setup was successful and we need a reboot
            /// </summary>
            SuccessfulNeedReboot = 3010,
        }

        private enum ProxyConnectionState
        {
            /// <summary>
            /// Not checked yet the connectivity to internet.
            /// </summary>
            NotChecked,
            /// <summary>
            /// Connected to internet.
            /// </summary>
            Connected,

            /// <summary>
            /// Not connected to internet.
            /// </summary>
            NotConnected,

            /// <summary>
            /// Checking for internet connectivity.
            /// </summary>
            Checking,

            /// <summary>
            /// Pending check for internet connectivity.
            /// </summary>
            CheckingPending,

            /// <summary>
            /// Additional data required for checking connectivity to internet.
            /// </summary>
            DataRequired,

            /// <summary>
            /// Proxy password is required.
            /// </summary>
            PasswordRequired
        }

        /// <summary>
        /// Type of proxy.
        /// </summary>
        private enum ProxyType
        {
            /// <summary>
            /// Bypass existing system proxy.
            /// </summary>
            Bypass,

            /// <summary>
            /// Use custom proxy settings.
            /// </summary>
            Custom,

            /// <summary>
            /// Use custom proxy settings with Authentication.
            /// </summary>
            CustomWithAuthentication
        }

        private delegate void HideIpPortPassphraseLableBoxes();

        private delegate void HideSvcStartStopMsg();
        private delegate void DisplaySvcStartStopMsg();

        private delegate void EnteredConfigFeildsDisable();
        private delegate void EnteredConfigFeildsEnable();

        private delegate void ReadOnlyTrueTextFeilds();

        private delegate void HideCustomProxySettingsFeilds();
        private delegate void DisplayCustomProxySettingsFeilds();

        private delegate void DisableCustomProxyGroupBox();
        private delegate void EnableCustomProxyGroupBox();

        private delegate void DisableVaultBrowseTextBox();
        private delegate void EnableVaultBrowseTextBox();

        private delegate void HideRegInProgressLabel();
        private delegate void DisplayRegInProgressLabel();


        Thread m_registerButtonThread;
        bool constCretedThreadstarted;
        Thread localRegButtonThread;
        bool localCreatedRegButtonThreadstarted;

        /// <summary>
        /// Stores current connection state using all different types of proxy.
        /// </summary>
        //private static Dictionary<ProxyType, ProxyConnectionState> proxyTypeConnectionStateMap;

        /// <summary>
        /// Detailed Error of proxy connection failure.
        /// </summary>
        private string lastDetailedError = string.Empty;

        private ProxyType proxy = ProxyType.Bypass;

        Thread m_configSaveButtonThread;
        bool constCretedConfigSaveThreadstarted;
        Thread localConfigSaveThread;
        bool localCreatedConfigThreadstarted;

        public Thread initThread;

        public bool stopsvc = false;

        // Private data used for Manage Accounts
        bool hasUserAccounts = false;
        bool operationInProgress = false;
        bool canSelectTab = true;
        CXClient cxClient = new CXClient();

        // Private data used for localization
        string language;

        // Constants
        internal const int SV_OK = 0;
        internal const int SV_FAIL = 1;

        [DllImport("configtool.dll")]
        public static extern void freeAllocatedBuf(out IntPtr allocatedBuff);
        [DllImport("configtool.dll")]
         public static extern int GetHostName(out IntPtr  hostname,out IntPtr err);
        [DllImport("configtool.dll")]
        public static extern int DoDRRegister([MarshalAs(UnmanagedType.LPStr)] string cmdinput, out IntPtr processexitcode, out IntPtr err);


        public void MakeIpPortPassphraseLableBoxesHide()
        {
            if (groupBox1.InvokeRequired)
            {
                HideIpPortPassphraseLableBoxes call = new csform.HideIpPortPassphraseLableBoxes(() =>
                {
                    this.label1.Visible = false;
                    this.label2.Visible = false;
                    this.label3.Visible = false;

                    this.textBox1.Visible = false;
                    this.textBox2.Visible = false;
                    this.textBox3.Visible = false;
                });

                groupBox1.Invoke(call);
            }
            else
            {
                this.label1.Visible = false;
                this.label2.Visible = false;
                this.label3.Visible = false;

                this.textBox1.Visible = false;
                this.textBox2.Visible = false;
                this.textBox3.Visible = false;
            }
        }

        public void MakeSvcStartStopMsgHide()
        {
            if (groupBox1.InvokeRequired)
            {
                HideSvcStartStopMsg call = new csform.HideSvcStartStopMsg(() =>
                {
                    // do control stuff
                    this.label6.Visible = false;
                });

                groupBox1.Invoke(call);
            }
            else
            {
                this.label6.Visible = false;
            }
        }
        public void MakeSvcStartStopMsgVisible()
        {
            if (groupBox1.InvokeRequired)
            {
                DisplaySvcStartStopMsg call = new csform.DisplaySvcStartStopMsg(() =>
                {
                    // do control stuff
                    this.label6.Visible = true;
                });

                groupBox1.Invoke(call);
            }
            else
            {
                this.label6.Visible = true;
            }
        }

        public void MakeConfigSaveButtonDisable()
        {
            this.configsavebutton.Enabled = false;
            this.canSelectTab = false;
        }
        public void MakeConfigsaveButtonEnable()
        {
            this.configsavebutton.Enabled = true;
            this.canSelectTab = true;
        }
        public void DisableEnteredConfigFeilds()
        {
            if (groupBox1.InvokeRequired)
            {
                EnteredConfigFeildsDisable call = new csform.EnteredConfigFeildsDisable(() =>
                {
                    this.textBox5.Enabled = false;
                    this.textBox6.Enabled = false;
                    this.signatureVerificationCheckBox.Enabled = false;
                });

                groupBox1.Invoke(call);
            }
            else
            {
                this.textBox5.Enabled = false;
                this.textBox6.Enabled = false;
                this.signatureVerificationCheckBox.Enabled = false;
            }
        }

        public void EnableEnteredConfigFeilds()
        {
            if (groupBox1.InvokeRequired)
            {
                EnteredConfigFeildsEnable call = new csform.EnteredConfigFeildsEnable(() =>
                {
                    this.textBox5.Enabled = true;
                    this.textBox6.Enabled = true;
                    this.signatureVerificationCheckBox.Enabled = true;
                });

                groupBox1.Invoke(call);
            }
            else
            {
                this.textBox5.Enabled = true;
                this.textBox6.Enabled = true;
                this.signatureVerificationCheckBox.Enabled = true;
            }
        }

        public void MakeHideCustomProxySettings()
        {
            if (this.CustomProxyGroupBox.InvokeRequired)
            {
                HideCustomProxySettingsFeilds call = new csform.HideCustomProxySettingsFeilds(() =>
                {
                    this.CustomProxyGroupBox.Visible = false;
                });

                this.CustomProxyGroupBox.Invoke(call);
            }
            else
            {
                this.CustomProxyGroupBox.Visible = false;
            }
        }

        public void MakeVisibleCustomProxySettings()
        {
            if (this.CustomProxyGroupBox.InvokeRequired)
            {
                DisplayCustomProxySettingsFeilds call = new csform.DisplayCustomProxySettingsFeilds(() =>
                {
                    this.CustomProxyGroupBox.Visible = true;
                    this.ProxyUserNameTextBox.Enabled = false;
                    this.ProxyPasswordTextBox.Enabled = false;
                });

                this.CustomProxyGroupBox.Invoke(call);
            }
            else
            {
                this.CustomProxyGroupBox.Visible = true;
                this.ProxyUserNameTextBox.Enabled = false;
                this.ProxyPasswordTextBox.Enabled = false;
            }
        }

        public void MakeDisableCustomProxyGroupBox()
        {
            if (this.CustomProxyGroupBox.InvokeRequired)
            {
                DisableCustomProxyGroupBox call = new csform.DisableCustomProxyGroupBox(() =>
                {
                    this.CustomProxyGroupBox.Enabled = false;
                });

                this.CustomProxyGroupBox.Invoke(call);
            }
            else
            {
                this.CustomProxyGroupBox.Enabled = false;
            }
        }

        public void MakeEnableCustomProxyGroupBox()
        {
            if (this.CustomProxyGroupBox.InvokeRequired)
            {
                EnableCustomProxyGroupBox call = new csform.EnableCustomProxyGroupBox(() =>
                {
                    this.CustomProxyGroupBox.Enabled = true;
                });

                this.CustomProxyGroupBox.Invoke(call);
            }
            else
            {
                this.CustomProxyGroupBox.Enabled = true;
            }
        }

        public void MakeDisableVaultBrowseTextBox()
        {
            if (this.textBox1.InvokeRequired)
            {
                DisableVaultBrowseTextBox call = new csform.DisableVaultBrowseTextBox(() =>
                {
                    this.textBox1.Enabled = false;
                });

                this.textBox1.Invoke(call);
            }
            else
            {
                this.textBox1.Enabled = false;
            }
        }

        public void MakeEnableVaultBrowseTextBox()
        {
            if (this.textBox1.InvokeRequired)
            {
                EnableVaultBrowseTextBox call = new csform.EnableVaultBrowseTextBox(() =>
                {
                    this.textBox1.Enabled = true;
                });

                this.textBox1.Invoke(call);
            }
            else
            {
                this.textBox1.Enabled = true;
            }
        }

        public void MakeHideRegInProgressLabel()
        {
            if (this.ASRRegInProgressLabel.InvokeRequired)
            {
                HideRegInProgressLabel call = new csform.HideRegInProgressLabel(() =>
                {
                    this.ASRRegInProgressLabel.Visible = false;
                });

                this.ASRRegInProgressLabel.Invoke(call);
            }
            else
            {
                this.ASRRegInProgressLabel.Visible = false;
            }
        }

        public void MakeVisibleRegInProgressLabel()
        {
            if (this.ASRRegInProgressLabel.InvokeRequired)
            {
                DisplayRegInProgressLabel call = new csform.DisplayRegInProgressLabel(() =>
                {
                    this.ASRRegInProgressLabel.Visible = true;
                });

                this.ASRRegInProgressLabel.Invoke(call);
            }
            else
            {
                this.ASRRegInProgressLabel.Visible = true;
            }
        }

        public void setCsPsIp(string pscsip, string pscsport)
        {
            this.onPremCsIpText.Text = pscsip;
            this.textBox2.Text = pscsport;
        }
        public void setCsDVACPPort(string dvacpport)
        {
            this.textBox6.Text = dvacpport;
        }
        public void setPassphrase(string pphrase)
        {
            this.textBox3.Text = pphrase;
        }
        public void setSSLPort(string sslport)
        {
            this.textBox5.Text = sslport;
        }

        public void GetProxySettingsValues(ref string proxyAddress, ref string proxyPort, ref string proxyUsername, ref string proxyPassword)
        {
            if (this.CustomProxyConnectRadiobutton.Checked == true)
            {
                string addrs = this.ProxyAddressTextBox.Text;
                string port = this.ProxyPortTextBox.Text;
                string uname = this.ProxyUserNameTextBox.Text;
                string passwd = this.ProxyPasswordTextBox.Text;

                if (!string.IsNullOrEmpty(addrs))
                {
                    proxyAddress = addrs;
                }

                if (!string.IsNullOrEmpty(port))
                {
                    proxyPort = port;
                }

                if (this.ProxyAuthCheckBox.Checked == true)
                {
                    if (!string.IsNullOrEmpty(uname))
                    {
                        proxyUsername = uname;
                    }

                    if (!string.IsNullOrEmpty(passwd))
                    {
                        proxyPassword = passwd;
                    }
                }
            }
        }

        public csform(bool enableInstallerMode)
        {
            InitializeComponent();
            if (enableInstallerMode)
            {
                this.tabControl1.TabPages.Remove(this.vaultRegistrationTabPage);
                this.tabControl1.TabPages.Remove(this.localizationTabPage);                
            }            
            m_registerButtonThread = new Thread(OnClickRegisterButtonTask);
            initThread = new Thread(InitProcessFunction);
            constCretedThreadstarted = false;
            localCreatedRegButtonThreadstarted = false;

            m_configSaveButtonThread = new Thread(OnClickConfigSaveButtonTask);
            constCretedConfigSaveThreadstarted = false;
            localCreatedConfigThreadstarted = false;

            this.textBox2.Enabled = false;
            this.textBox3.Enabled = false;
            this.onPremCsIpText.Enabled = false;
            //this.textBox5.Enabled = false;
            this.textBox3.PasswordChar = '*';
            //MakeConfigsaveButtonEnable();
            MakeHideRegInProgressLabel();
            MakeSvcStartStopMsgHide();
            this.ProxyPasswordTextBox.PasswordChar = '*';
            this.signatureVerificationCheckBox.Checked = false;
            MakeHideCustomProxySettings();
        }

        private void GetResources()
        {
            this.Text = ResourceHelper.GetStringResourceValue("cs");
            this.manageAccountsTabPage.Text = ResourceHelper.GetStringResourceValue("manageaccounts");
            this.vaultRegistrationTabPage.Text = ResourceHelper.GetStringResourceValue("vaultreg");
            this.headingLabel.Text = ResourceHelper.GetStringResourceValue("manageaccountsdesc");
            this.addButton.Text = ResourceHelper.GetStringResourceValue("addaccount");
            this.editButton.Text = ResourceHelper.GetStringResourceValue("edit");
            this.deleteButton.Text = ResourceHelper.GetStringResourceValue("delete");
            this.ColumnFriendlyName.HeaderText = ResourceHelper.GetStringResourceValue("name").ToUpper(ResourceHelper.CurrentCulture);
            this.ColumnUsername.HeaderText = ResourceHelper.GetStringResourceValue("username").ToUpper(ResourceHelper.CurrentCulture);
            this.closeButton.Text = ResourceHelper.GetStringResourceValue("close");
            this.label3.Text = ResourceHelper.GetStringResourceValue("vaultregdesc");
            this.label1.Text = ResourceHelper.GetStringResourceValue("vaultcredfile");
            this.regbutton.Text = ResourceHelper.GetStringResourceValue("register");
            this.browsebutton.Text = ResourceHelper.GetStringResourceValue("browse");
        }

        public void MakeRegCancelButtonDisable()
        {
            this.regbutton.Enabled = false;
            this.closeButton.Enabled = false;
            this.browsebutton.Enabled = false;
            this.canSelectTab = false;
        }

        public void MakeRegCancelButtonEnable()
        {
            this.regbutton.Enabled = true;
            this.closeButton.Enabled = true;
            this.browsebutton.Enabled = true;
            this.canSelectTab = true;
        }

        public void MakeProxyButtonDisable()
        {
            this.BypassProxyConnectionRadiobutton.Enabled = false;
            this.CustomProxyConnectRadiobutton.Enabled = false;
        }

        public void MakeProxyButtonEnable()
        {
            this.BypassProxyConnectionRadiobutton.Enabled = true;
            this.CustomProxyConnectRadiobutton.Enabled = true;
        }

        public void MakeRegButtonEnable()
        {
            this.regbutton.Enabled = true;
        }
        public void MakeBrowsebuttonDisable()
        {
            this.browsebutton.Enabled = false;
        }
        public void MakeCredentialBoxRedOnly()
        {
            if (this.textBox1.InvokeRequired)
            {
                ReadOnlyTrueTextFeilds call = new csform.ReadOnlyTrueTextFeilds(() =>
                {
                    this.textBox1.ReadOnly = true;
                });

                this.textBox1.Invoke(call);
            }
            else
            {
                this.textBox1.ReadOnly = true;

            }
        }

        public void MakeConfigButtonDisable()
        {
            this.configsavebutton.Enabled = false;
            this.closeButton.Enabled = false;
        }
        public void MakeConfigButtonEnable()
        {
            this.configsavebutton.Enabled = true;
            this.closeButton.Enabled = true;
        }

        public void MakeAllRegRelatedButton_TextBoxEnable()
        {
            MakeRegCancelButtonEnable();
            MakeProxyButtonEnable();
            MakeEnableCustomProxyGroupBox();
            MakeEnableVaultBrowseTextBox();
        }
       
        
        private void regButton_Click(object sender, EventArgs e)
        {
            if (textBox1.Text.Length == 0)
            {
                MessageBox.Show("Provide valid path of Recovery Services Vault Credentials File to proceed", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }
            MakeRegCancelButtonDisable();
            MakeProxyButtonDisable();
            MakeDisableCustomProxyGroupBox();
            MakeDisableVaultBrowseTextBox();
            if (!constCretedThreadstarted)
            {
                m_registerButtonThread.Start(this);
                constCretedThreadstarted = true;
            }
            else
            {
                localRegButtonThread = new Thread(OnClickRegisterButtonTask);
                localRegButtonThread.Start(this);
                localCreatedRegButtonThreadstarted = true;
            }
        }

        private void OnClickRegisterButtonTask(object obj)
        {

            Logger.Debug("Entered csform:OnClickRegisterButtonTask");

            if (this.BypassProxyConnectionRadiobutton.Checked == true)
            {
                proxy = ProxyType.Bypass;
            }

            if (this.CustomProxyConnectRadiobutton.Checked == true)
            {
                proxy = ProxyType.Custom;
                if (MissingFeildsInCustomProxySettings())
                {
                    MakeAllRegRelatedButton_TextBoxEnable();
                    return;
                }
            }

            MakeVisibleRegInProgressLabel();

            Services.Helpers hs = new Services.Helpers();
            string hostname = "hostname";
            IntPtr hostPtr = IntPtr.Zero;
            IntPtr errPtr = IntPtr.Zero;
            try
            {
                int ret = csform.SV_OK;
                ret = GetHostName(out hostPtr, out errPtr);
                if (ret != csform.SV_OK)
                {
                    Logger.Error("Failed to Get hostname.");
                    string errmsg = Marshal.PtrToStringAnsi(errPtr);
                    freeAllocatedBuf(out errPtr);
                    MessageBox.Show(this, errmsg, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1);
                    Logger.Close();
                    System.Environment.Exit(1);
                }
                else
                {
                    hostname += "=";
                    hostname += Marshal.PtrToStringAnsi(hostPtr);
                    freeAllocatedBuf(out hostPtr);
                }

            }
            catch (Exception exc)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(exc, true);
                string err = trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber() + " Exception: " + exc.Message;
                Logger.Error("Caught Exception... ExcMsg: " + err);
                MessageBox.Show(this, "Caught Exception... ExcMsg: " + err, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1);
                Logger.Close();
                System.Environment.Exit(1);
            }


            string credentialsboxvalue = this.textBox1.Text;
            var ifMatched = Regex.Match(credentialsboxvalue, @"[^ \a\t\n\b\r]");

            int found = ifMatched.Success ? ifMatched.Index : -1;

            if (found == -1)
            {
                MessageBox.Show(this, "Wrong CredentialFile path.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1);
                MakeAllRegRelatedButton_TextBoxEnable();
                MakeHideRegInProgressLabel();
                return;
            }
            if (credentialsboxvalue.Length > 256)
            {
                MessageBox.Show(this, "Wrong CredentialFile path.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1);
                MakeAllRegRelatedButton_TextBoxEnable();
                MakeHideRegInProgressLabel();
                return;
            }

            string credentialsFile = "credentialsfile";
            credentialsFile += "=";
            credentialsFile += credentialsboxvalue;

            string proxytype = "proxytype";
            string proxyaddress = "proxyaddress";
            string proxyport = "proxyport";
            string proxyusername = "proxyusername";
            string proxypassword = "proxypassword";
           

            string cmdinput = proxytype;
            cmdinput += "=";

            if (this.BypassProxyConnectionRadiobutton.Checked == true)
            {
                cmdinput += ProxyType.Bypass;
            }

            if (this.CustomProxyConnectRadiobutton.Checked == true)
            {
                if (this.ProxyAuthCheckBox.Checked == true)
                {
                    cmdinput += ProxyType.CustomWithAuthentication;
                }
                else
                {
                    cmdinput += ProxyType.Custom;
                }

                cmdinput += ";";
                cmdinput += proxyaddress + "=";
                cmdinput += this.ProxyAddressTextBox.Text;
                cmdinput += ";";
                cmdinput += proxyport + "=";
                cmdinput += this.ProxyPortTextBox.Text;

                if (this.ProxyAuthCheckBox.Checked == true)
                {
                    cmdinput += ";";
                    cmdinput += proxyusername + "=";
                    cmdinput += this.ProxyUserNameTextBox.Text;
                    cmdinput += ";";
                    cmdinput += proxypassword + "=";
                    cmdinput += this.ProxyPasswordTextBox.Text;
                }

            }

            cmdinput += ";" + hostname + ";" + credentialsFile;

            string regErrmsg = string.Empty;
            string psExitCode = string.Empty;

            if (hs.drregister(ref cmdinput, ref psExitCode, ref regErrmsg))
            {
                MakeHideRegInProgressLabel();
                Logger.Debug("DLL DrRegister API execution succeeded.");
                int exitCode = Convert.ToInt32(psExitCode);
                if (exitCode == (int)DRConfigReturnValues.Successful)
                {
                    Logger.Debug("DRA registration succeeded.");
                    MessageBox.Show(this, "DRA registration succeeded.", "Information", MessageBoxButtons.OK, MessageBoxIcon.Information, MessageBoxDefaultButton.Button1);
                }
                else if (exitCode == (int)DRConfigReturnValues.SuccessfulNeedReboot)
                {
                    Logger.Debug("DRA registration succeeded but requires a reboot.");
                    MessageBox.Show(this, "DRA registration succeeded but requires a reboot.", "Information", MessageBoxButtons.OK, MessageBoxIcon.Information, MessageBoxDefaultButton.Button1);
                }
                else
                {
                    string drmsg = "Failed to register DRA. Error code: " + exitCode + ". Error logs are available at <SYSTEMDRIVE>\\ProgramData\\ASRLogs";
                    Logger.Error(drmsg);
                    MessageBox.Show(this, drmsg, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1);
                    Logger.Close();
                    System.Environment.Exit(1);

                }
                MakeCredentialBoxRedOnly();
                MakeBrowsebuttonDisable();
                Logger.Debug("Exited csform:OnClickRegisterButtonTask");
                Logger.Debug("Exiting Application");
                Logger.Close();
                System.Environment.Exit(0);
            }
            else
            {
                MakeHideRegInProgressLabel();
                string err = "Registration could not be completed successfully.\n\nERROR MESSAGE:\n " + regErrmsg + "\nRECOMENDATION:\nResolve any errors and try to register again using the CSPSConfigtool.exe. If this issue persists contact Microsoft Support.";
                Logger.Error(err);
                MessageBox.Show(this, err, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1);
                Logger.Close();
                System.Environment.Exit(1);
            }

            Logger.Debug("Exited csform:OnClickRegisterButtonTask"); 
            
        }

        private void browseButton_Click(object sender, EventArgs e)
        {
            OpenFileDialog openFileDialog1 = new OpenFileDialog(); 
            openFileDialog1.InitialDirectory = "c:\\";
            openFileDialog1.Filter = "All files (*.*)|*.*";
            openFileDialog1.FilterIndex = 2;
            openFileDialog1.RestoreDirectory = true;

            if (openFileDialog1.ShowDialog() == DialogResult.OK)
            {
                textBox1.Text = openFileDialog1.FileName;
            }

        }

        private void closeButton_Click(object sender, EventArgs e)
        {
            Logger.Debug("Clicked on Cancel Button");
            Logger.Debug("Exiting Application");
            Logger.Close();
            System.Environment.Exit(0);
        }

        private void OnCSForm_Closing(object sender, System.ComponentModel.CancelEventArgs e)
        {
            bool isBackGroundProcessRunning = false;
            if (this.m_registerButtonThread.IsAlive || this.operationInProgress || this.initThread.IsAlive || this.m_configSaveButtonThread.IsAlive)
            {
                isBackGroundProcessRunning = true;
            }
            else if (localCreatedConfigThreadstarted && this.localConfigSaveThread.IsAlive)
            {
                isBackGroundProcessRunning = true;
            }
            else if (localCreatedRegButtonThreadstarted && this.localRegButtonThread.IsAlive)
            {
                isBackGroundProcessRunning = true;
            }
            if (isBackGroundProcessRunning)
            {
                MessageBox.Show(this, "Background process is going on, please try after some time");
                // Cancel the Closing event from closing the form.
                e.Cancel = true;
            }

        }

        private void InitProcessFunction(object obj)
        {
            Logger.Debug("Entered Program:InitProcessFunction.");
            Services.Helpers hs = new Services.Helpers();
            csform cs = (csform)obj;
            bool isValRetirevedProperly = true;

            string pscsip = string.Empty;
            string pscsport = string.Empty;
            string pphrase = string.Empty;
            string sslport = string.Empty;
            string dvacpPort = string.Empty;
            bool isCS = true;
            string errmsg = string.Empty;
            bool isSigVerificationReq = true;
            string azurecsip = string.Empty;

            cs.MakeConfigButtonDisable();
            Thread.Sleep(1000);

            if (hs.initprocessfunction(cs, ref  pscsip, ref pscsport, ref pphrase, ref sslport, ref dvacpPort, ref errmsg, ref isCS, ref azurecsip))
            {
                if (!pscsip.Equals("\"\"", StringComparison.Ordinal) || !pscsport.Equals("\"\"", StringComparison.Ordinal) || !dvacpPort.Equals("\"\"", StringComparison.Ordinal))
                {
                    Logger.Debug("Exist PS_CS_IP : " + pscsip + " , PS_CS_PORT : " + pscsport + " , DVACP_PORT : " + dvacpPort);

                    if ((pscsip[0] == '"' && pscsip[pscsip.Length - 1] == '"'))
                        pscsip = pscsip.Replace("\"", "");
                    if ((pscsport[0] == '"' && pscsport[pscsport.Length - 1] == '"'))
                        pscsport = pscsport.Replace("\"", "");
                    if ((dvacpPort[0] == '"' && dvacpPort[dvacpPort.Length - 1] == '"'))
                        dvacpPort = dvacpPort.Replace("\"", "");

                    cs.setCsPsIp(pscsip, pscsport);
                    cs.setCsDVACPPort(dvacpPort);
                    cs.azureCsIpComboBox_Load(pscsip, azurecsip);
                }
                else if (pscsip[0] == '"' && pscsip[pscsip.Length - 1] == '"')
                {
                    Logger.Debug("Quoted value");
                }
                else
                {
                    Logger.Debug("Either PS_CS_IP and/or PS_CS_PORT is Empty");
                }

                if (pphrase.Equals("Empty", StringComparison.Ordinal))
                {
                    Logger.Debug("Passphrase is Empty");
                }
                else
                {
                    cs.setPassphrase(pphrase);
                }

                if (sslport.Equals("Empty", StringComparison.Ordinal))
                {
                    Logger.Debug("ssl_port is Empty");
                }
                else
                {
                    cs.setSSLPort(sslport);
                }

                if (hs.GetSignatureVerificationChecksConfig(ref isSigVerificationReq, ref errmsg))
                {
                    this.signatureVerificationCheckBox.Checked = isSigVerificationReq;
                }
                else
                {
                    isValRetirevedProperly = false;
                }

                cs.MakeConfigButtonEnable();
                Thread.Sleep(1000);
            }
            else 
            {
                isValRetirevedProperly = false;
            }


            if(!isValRetirevedProperly)
            {
                Logger.Error("Failed Program:InitProcessFunction, Error: " + errmsg);
                MessageBox.Show(this, errmsg, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1);
                Logger.Close();
                System.Environment.Exit(1);
            }
            Logger.Debug("Exited Program:InitProcessFunction.");
        }

        #region Manage Accounts Tab Event Handlers

        private void csform_Load(object sender, EventArgs e)
        {
            EnableManageAccountsTabControls(false);
            this.operationInProgress = true;
            loadBackgroundWorker.RunWorkerAsync();
        }

        private void PopulateUserAccounts(List<UserAccount> userAccounts)
        {
            if (userAccounts != null)
            {
                foreach (UserAccount userAccount in userAccounts)
                {
                    AddRow(userAccount);
                }
            }
        }

        private void EnableManageAccountsTabControls(bool enabled)
        {            
            this.canSelectTab = enabled;
            this.addButton.Enabled = this.closeButton.Enabled = enabled;
            this.accountsDataGridView.Enabled = enabled;
            if (this.hasUserAccounts)
            {
                this.editButton.Enabled = this.deleteButton.Enabled = enabled;
            }
        }

        private void EnableLocalizationTabControls(bool enabled)
        {
            this.canSelectTab = enabled;
            this.saveButton.Enabled = this.closeButton.Enabled = enabled;      
        }

        private void ShowTaskProgress(bool showMessage, string message)
        {            
            this.progressLabel.Visible = showMessage;
            this.progressLabel.Text = showMessage ? message : null;
        }

        private void AddRow(UserAccount userAccount)
        {
            if (userAccount != null)
            {
                DataGridViewRow row = (DataGridViewRow)this.accountsDataGridView.RowTemplate.Clone();
                row.CreateCells(accountsDataGridView);
                row.Tag = userAccount.AccountId;
                row.Cells[0].Value = userAccount.AccountName;
                row.Cells[1].Value = String.IsNullOrEmpty(userAccount.Domain) ? userAccount.UserName : String.Format("{0}\\{1}", userAccount.Domain, userAccount.UserName);
                row.Cells[1].Tag = userAccount.Password;
                this.accountsDataGridView.Rows.Add(row);
                if (!this.hasUserAccounts)
                {
                    this.accountsDataGridView.Visible = true;
                    this.messageLabel.Visible = false;
                    this.editButton.Enabled = true;
                    this.deleteButton.Enabled = true;
                    hasUserAccounts = true;
                }
            }
        }

        private void UpdateSelectedRow(UserAccount userAccount)
        {
            if (userAccount != null)
            {
                DataGridViewSelectedRowCollection dataGridViewSelectedRowCollection = accountsDataGridView.SelectedRows;
                if (dataGridViewSelectedRowCollection != null && dataGridViewSelectedRowCollection.Count > 0 &&
                    dataGridViewSelectedRowCollection[0].Tag != null && dataGridViewSelectedRowCollection[0].Tag.ToString() == userAccount.AccountId)
                {
                    dataGridViewSelectedRowCollection[0].Cells[0].Value = userAccount.AccountName;
                    dataGridViewSelectedRowCollection[0].Cells[1].Value = String.IsNullOrEmpty(userAccount.Domain) ? userAccount.UserName : String.Format("{0}\\{1}", userAccount.Domain, userAccount.UserName);
                    dataGridViewSelectedRowCollection[0].Cells[1].Tag = userAccount.Password;
                }
                else
                {
                    Logger.Error("Selected account not found");
                }
            }
        }

        private int GetSelectRowIndex()
        {
            int rowIndex = -1;
            if (accountsDataGridView.RowCount > 0)
            {
                DataGridViewSelectedRowCollection dataGridViewSelectedRowCollection = accountsDataGridView.SelectedRows;
                if (dataGridViewSelectedRowCollection != null && dataGridViewSelectedRowCollection.Count > 0)
                {
                    rowIndex = dataGridViewSelectedRowCollection[0].Index;                    
                }
            }  

            return rowIndex;
        }

        private void DeleteRow(int rowIndex)
        {
            if (rowIndex >= 0 && rowIndex < accountsDataGridView.RowCount)
            {
                this.accountsDataGridView.Rows.RemoveAt(rowIndex);
                if (accountsDataGridView.RowCount == 0)
                {
                    hasUserAccounts = false;
                    this.accountsDataGridView.Visible = false;
                    Logger.Debug("Last Account was deleted");
                }
            }
        }

        private UserAccount GetSelectedUserAccount()
        {
            UserAccount userAccount = null;
            if (accountsDataGridView.RowCount > 0)
            {
                DataGridViewSelectedRowCollection dataGridViewSelectedRowCollection = accountsDataGridView.SelectedRows;
                if (dataGridViewSelectedRowCollection != null && dataGridViewSelectedRowCollection.Count > 0)
                {
                    userAccount = new UserAccount();
                    object value = dataGridViewSelectedRowCollection[0].Tag;
                    userAccount.AccountId = (value == null) ? String.Empty : value.ToString();
                    value = dataGridViewSelectedRowCollection[0].Cells[0].Value;
                    userAccount.AccountName = (value == null) ? String.Empty : value.ToString();
                    value = dataGridViewSelectedRowCollection[0].Cells[1].Value;
                    userAccount.SetDomainAndUserName( (value ==null) ? String.Empty : value.ToString());
                    value = dataGridViewSelectedRowCollection[0].Cells[1].Tag;
                    userAccount.Password = (value == null) ? String.Empty : value.ToString();
                }
            }
            return userAccount;
        }

        private void loadBackgroundWorker_DoWork(object sender, DoWorkEventArgs e)
        {
            string errorMessage;
            this.language = Localization.GetLanguage(Localization.ReadLocale(out errorMessage));

            ResponseStatus responseStatus = new ResponseStatus();
            loadBackgroundWorker.ReportProgress(10, "Loading accounts.. Please wait for few minutes.");
            List<UserAccount> userAccounts = this.cxClient.ListAccounts(responseStatus);            
            if (responseStatus.ReturnCode != 0)
            {
                e.Result = responseStatus;
            }
            else
            {
                e.Result = userAccounts;
            }
        }

        private void backgroundWorker_ProgressChanged(object sender, ProgressChangedEventArgs e)
        {
            ShowTaskProgress(true, e.UserState.ToString());
        }

        private void loadBackgroundWorker_RunWorkerCompleted(object sender, RunWorkerCompletedEventArgs e)
        {
            this.langComboBox.SelectedItem = String.IsNullOrEmpty(this.language) ? "English" : this.language;

            this.accountsDataGridView.Rows.Clear();
            string operation = "load Accounts.";
            if (e.Error != null)
            {
                Logger.Error(String.Format("Failed to {0}. Error: {1}", operation, e.Error));
                ShowStatus(Services.Helpers.GetCXAPIErrorMessage(operation, e.Error));
            }
            else if (e.Result != null)
            {
                if (e.Result is ResponseStatus)
                {                    
                    string errorMessage = Services.Helpers.GetCXAPIErrorMessage(operation, e.Result as ResponseStatus);
                    ShowStatus(errorMessage);
                    Logger.Error(errorMessage);
                }
                else if (e.Result is List<UserAccount> && (e.Result as List<UserAccount>).Count > 0)
                {
                    PopulateUserAccounts(e.Result as List<UserAccount>);                    
                }
                else
                {
                    Logger.Debug("No accounts available.");
                }
            }
            else
            {
                Logger.Debug("No accounts available.");
            }

            this.operationInProgress = false;
            ShowTaskProgress(false, null);
            EnableManageAccountsTabControls(true);
        }

        private void addButton_Click(object sender, EventArgs e)
        {
            UserAccountForm form = new UserAccountForm(this.cxClient);
            if (form.ShowDialog(this) == System.Windows.Forms.DialogResult.OK)
            {
                EnableManageAccountsTabControls(false);
                this.operationInProgress = true;
                loadBackgroundWorker.RunWorkerAsync();
            }
            form.Dispose();
        }

        private void editButton_Click(object sender, EventArgs e)
        {
            UserAccountForm form = new UserAccountForm(this.cxClient, GetSelectedUserAccount());
            if (form.ShowDialog(this) == System.Windows.Forms.DialogResult.OK)
            {
                UpdateSelectedRow(form.SelectedAccount);
            }
            form.Dispose();
        }

        private void deleteButton_Click(object sender, EventArgs e)
        {
            UserAccount userAccount = GetSelectedUserAccount();
            if (userAccount != null)
            {
                if (MessageBox.Show(this, String.Format("Are you sure you want to delete \"{0}\" Account?", userAccount.AccountName), "Confirmation", MessageBoxButtons.YesNo, MessageBoxIcon.Question) == System.Windows.Forms.DialogResult.Yes)
                {
                    EnableManageAccountsTabControls(false);
                    this.operationInProgress = true;
                    this.deleteBackgroundWorker.RunWorkerAsync(userAccount);
                }
            }
        }

        private void deleteBackgroundWorker_DoWork(object sender, DoWorkEventArgs e)
        {
            bool result = false;
            ResponseStatus responseStatus = new ResponseStatus();
            deleteBackgroundWorker.ReportProgress(10, "Deleting account.. Please wait for few minutes.");
            if (e.Argument is UserAccount)
            {
                UserAccount userAccount = e.Argument as UserAccount;
                List<string> userAccountList = new List<string>();
                userAccountList.Add(userAccount.AccountId);
                result = this.cxClient.RemoveAccounts(userAccountList, responseStatus);
            }
            e.Result = responseStatus as object;
        }

        private void deleteBackgroundWorker_RunWorkerCompleted(object sender, RunWorkerCompletedEventArgs e)
        {
            ShowTaskProgress(false, null);
            int rowIndex = GetSelectRowIndex();
            object cellValue = accountsDataGridView.Rows[rowIndex].Cells[0].Value;
            string accountName = (cellValue == null) ? String.Empty : cellValue.ToString();
            string message;
            string operation = String.Format("delete \"{0}\" Account", accountName);
            if (e.Error != null)
            {
                Logger.Error(String.Format("Failed to {0}. Error: {1}", operation, e.Error));
                MessageBox.Show(this,
                       Services.Helpers.GetCXAPIErrorMessage(operation, e.Error),
                       "Error",
                       MessageBoxButtons.OK,
                       MessageBoxIcon.Error);
            }
            else if (e.Result != null && e.Result is ResponseStatus)
            {
                ResponseStatus responseStatus = e.Result as ResponseStatus;                
                if (responseStatus.ReturnCode == 0)
                {
                    DeleteRow(rowIndex);
                    message = String.Format("Successfully deleted \"{0}\" Account.", accountName);
                    MessageBox.Show(this,
                         message,
                         "Information",
                         MessageBoxButtons.OK,
                         MessageBoxIcon.Information);
                    Logger.Debug(message);
                }
                else
                {
                    message = Services.Helpers.GetCXAPIErrorMessage(operation, responseStatus);
                    MessageBox.Show(this,
                        message,
                        "Error",
                        MessageBoxButtons.OK,
                        MessageBoxIcon.Error);
                    Logger.Error(message);
                }
            }
            else
            {
                message = String.Format("Failed to {0} with internal error.", operation);
                MessageBox.Show(this,
                       message,
                       "Error",
                       MessageBoxButtons.OK,
                       MessageBoxIcon.Error);
                Logger.Error(message);
            }
            this.operationInProgress = false;
            EnableManageAccountsTabControls(true);
        }

        private void ShowStatus(string message)
        {
            this.accountsDataGridView.Visible = false;
            this.messageLabel.Text = message;            
            this.messageLabel.Visible = true;
        }
        

        #endregion               

        private void tabControl1_Selecting(object sender, TabControlCancelEventArgs e)
        {
            if(!this.canSelectTab)
            {
                e.Cancel = true;
            }
        }

        private void saveButton_Click(object sender, EventArgs e)
        {
            try
            {
                string message;
                EnableLocalizationTabControls(false);
                if (this.langComboBox.SelectedItem == null)
                {
                    MessageBox.Show(this,
                         "Please select a language for Provider error messages.",
                         "Error",
                         MessageBoxButtons.OK,
                         MessageBoxIcon.Error);
                    return;
                }
                if (Localization.UpdateLocale(this.langComboBox.SelectedItem.ToString(), out message))
                {
                    message = String.Format("Successfully updated localization setting.");
                    Logger.Debug(message);
                    MessageBox.Show(this,
                         message,
                         "Information",
                         MessageBoxButtons.OK,
                         MessageBoxIcon.Information);
                }
                else
                {                    
                    MessageBox.Show(this,
                       message,
                       "Error",
                       MessageBoxButtons.OK,
                       MessageBoxIcon.Error);
                }
            }
            catch(Exception ex)
            {
                Logger.Error("Failed to update localization setting. Error: " + ex);
                MessageBox.Show(this,
                       "Failed to update localization setting with internal error.",
                       "Error",
                       MessageBoxButtons.OK,
                       MessageBoxIcon.Error);
            }
            EnableLocalizationTabControls(true);
        }

        private void configSaveButton_Click(object sender, EventArgs e)
        {
            MakeConfigSaveButtonDisable();
            this.closeButton.Enabled = false;
            DisableEnteredConfigFeilds();
            if (!constCretedConfigSaveThreadstarted)
            {
                m_configSaveButtonThread.Start(this);
                constCretedConfigSaveThreadstarted = true;
            }
            else
            {
                localConfigSaveThread = new Thread(OnClickConfigSaveButtonTask);
                localConfigSaveThread.Start(this);
                localCreatedConfigThreadstarted = true;
            }
        }

        private void OnClickConfigSaveButtonTask(object obj)
        {

            Logger.Debug("Entered csform:OnClickConfigSaveButtonTask");
            string errmsg = string.Empty;
            Services.Helpers hs = new Services.Helpers();

            this.label6.Text = "Stopping Services...";
            MakeSvcStartStopMsgVisible();
            if (hs.stopservices(ref errmsg))
            {
                Logger.Debug("Successfully stopped services.");
                MakeSvcStartStopMsgHide();
                Thread.Sleep(1000);
                stopsvc = true;
            }
            else
            {
                Logger.Error("Failed to stop services, error: " + errmsg);
                MessageBox.Show(this, errmsg, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1);
                Logger.Debug("Exiting Application");
                Logger.Close();
                System.Environment.Exit(1);

            }

            bool disableSignatureVerification = !signatureVerificationCheckBox.Checked;
            if (hs.UpdatePushInstallConfigFile(disableSignatureVerification) == false)
            {
                MessageBox.Show("Failed to modify values in PushInstaller.CONF for signature validation.", "Warning", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }

            string dvacpportvalue = this.textBox6.Text;
            string dvacpport = "DVACP_PORT";
            dvacpport += "=";
            if (string.IsNullOrEmpty(dvacpportvalue))
            {
                dvacpport += "20004";
            }
            else
            {
                dvacpport += dvacpportvalue;
            }

            string areq = dvacpport;

            string dataTransFerSecurePort = "sslport";
            dataTransFerSecurePort += "=";
            if (string.IsNullOrEmpty(this.textBox5.Text))
            {
                dataTransFerSecurePort += "9443";
            }
            else
            {
                dataTransFerSecurePort += this.textBox5.Text;
            }

            string creq = dataTransFerSecurePort;

            string azurecsvalue = this.azureCsIpComboBox.SelectedItem.ToString();
            string azurecs = "IP_ADDRESS_FOR_AZURE_COMPONENTS";
            azurecs += "=";
            if (string.IsNullOrEmpty(azurecsvalue))
            {
                Logger.Error("IP_ADDRESS_FOR_AZURE_COMPONENTS value expected.");
                MessageBox.Show(this, "Failed to UpdateConfFilesInCS", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1);
            }
            else
            {
                azurecs += azurecsvalue;
            }

            string azurecsreq = azurecs;

            string regErrmsg = string.Empty;
            bool isUpdated = true;
            if (hs.UpdateConfFilesInCS(ref areq, ref creq, ref azurecsreq, ref regErrmsg))
            {
                Logger.Debug("Successfully updated UpdateConfFilesInCS.");
                 Thread.Sleep(1000);
            }
            else
            {
                isUpdated = false;
                Logger.Error("Failed to update UpdateConfFilesInCS, error: " + regErrmsg);
                MessageBox.Show(this, "Failed to update UpdateConfFilesInCS, error: " + regErrmsg, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1);
            }

            try
            {
                errmsg = string.Empty;
                Logger.Debug("Starting the Services");
                this.label6.Text = "Starting Services...";
                MakeSvcStartStopMsgVisible();
                Thread.Sleep(1000);
                if (hs.startservices(ref errmsg))
                {
                    MakeSvcStartStopMsgHide();
                    Logger.Debug("Successfully started services.");
                    Thread.Sleep(1000);
                    stopsvc = false;

                }
                else
                {
                    Logger.Error("Failed to start services, error: " + errmsg);
                    string err = string.Empty;
                    if (isUpdated)
                    {
                        err = "Changes have been saved successfully,but " + errmsg;
                    }
                    else
                    {
                        err = "Unable to save all changes and " + errmsg + "And re-try the operation.";
                    }
                    MessageBox.Show(this, err, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1);
                }
            }
            catch (Exception exc)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(exc, true);
                MessageBox.Show(this, trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber() + " Exception: " + exc.Message,
                     "Error", MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1);
            }
            MakeSvcStartStopMsgHide();
            MakeConfigsaveButtonEnable();
            this.closeButton.Enabled = true;
            EnableEnteredConfigFeilds();
            if (isUpdated && !stopsvc)
            {
                MessageBox.Show(this, "Updated Successfully.", "Information", MessageBoxButtons.OK, MessageBoxIcon.Information, MessageBoxDefaultButton.Button1);
            }

            Logger.Debug("Exited csform:OnClickConfigSaveButtonTask");

        }

        private void dvacplinkLabel_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
        {
            // Specify that the link was visited.
            this.dvacplinkLabel.LinkVisited = true;

            // Navigate to a URL.
            System.Diagnostics.Process.Start("https://azure.microsoft.com/en-us/documentation/articles/site-recovery-vmware-to-azure/");
        }


        private void CustomProxyConnectRadiobutton_CheckedChanged(object sender, EventArgs e)
        {
            if (this.CustomProxyGroupBox.Visible == true)
            {
                //this.CustomProxyGroupBox.Visible = false;
                MakeHideCustomProxySettings();
            }
            else
            {
                //this.CustomProxyGroupBox.Visible = true;
                MakeVisibleCustomProxySettings();
            }
        }

        private void ProxyAuthCheckBox_CheckedChanged(Object sender, EventArgs e)
        {
            if (this.ProxyAuthCheckBox.Checked == true)
            {
                this.ProxyUserNameTextBox.Enabled = true;
                this.ProxyPasswordTextBox.Enabled = true;
            }
            else
            {
                this.ProxyUserNameTextBox.Enabled = false;
                this.ProxyPasswordTextBox.Enabled = false;
            }
        }

        private bool MissingFeildsInCustomProxySettings()
        {
            bool ret = true;
            string errmsg = string.Empty;
            if (string.IsNullOrEmpty(this.ProxyAddressTextBox.Text))
            {
                errmsg = "Proxy server address is required.";
            }
            else if (string.IsNullOrEmpty(this.ProxyPortTextBox.Text))
            {
                errmsg = "Proxy server port is required.";
            }
            else
            {
                if (!Services.Helpers.validateport(this.ProxyPortTextBox.Text))
                {
                    errmsg = "Invalid proxy server port.";
                }
                else if (this.ProxyAuthCheckBox.Checked == true)
                {
                    if (string.IsNullOrEmpty(this.ProxyUserNameTextBox.Text))
                    {
                        errmsg = "Proxy server Username is required.";
                    }
                    else if (string.IsNullOrEmpty(this.ProxyPasswordTextBox.Text))
                    {
                        errmsg = "Proxy server Password is required.";
                    }
                    else
                    {
                        ret = false;
                    }
                }
                else
                {
                    ret = false;
                }
            }

            if (ret == true)
            {
                MessageBox.Show(this, errmsg, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1);
            }

            return ret;
        }

        private void azureCsIpComboBox_Load(string onpremcsip, string azurecsip)
        {
            if (!onpremcsip.Equals(azurecsip))
            {
                this.azureCsIpComboBox.Items.Add(azurecsip);
                this.azureCsIpComboBox.SelectedIndex = 0;
                this.azureCsIpComboBox.Enabled = false;
                return;
            }

            this.azureCsIpComboBox.Items.AddRange(this.getNetworkInterfaces());

            int index = this.azureCsIpComboBox.Items.IndexOf(azurecsip);
            if (index < 0)
            {
                string errmsg = "Azure CS ip: " + azurecsip + " is not in the NIC list.";
                Logger.Debug("azureCsIpComboBox_Load: " + errmsg);
                this.azureCsIpComboBox.Items.Add(azurecsip);
                index = this.azureCsIpComboBox.Items.IndexOf(azurecsip);
            }

            this.azureCsIpComboBox.SelectedIndex = index;
        }

        private object[] getNetworkInterfaces()
        {
            Logger.Debug("Entered csform:getNetworkInterfaces");
            List<object> Comboboxnics = new List<object>();
            try
            {
                foreach (NetworkInterface nic in NetworkInterface.GetAllNetworkInterfaces())
                {
                    if (isValidNIC(nic))
                    {
                        IPInterfaceProperties ipProperties = nic.GetIPProperties();

                        //For each of the IPProperties process the informaiton 
                        foreach (IPAddressInformation ipAddr in ipProperties.UnicastAddresses)
                        {

                            string ipAddress = ipAddr.Address.ToString();
                            if (!string.IsNullOrEmpty(ipAddress))
                            {
                                //Only list Valid IPV4 address using regular expression comparision
                                string IPV4_PATTERN =
                                        "^([01]?\\d\\d?|2[0-4]\\d|25[0-5])\\.([01]?\\d\\d?|2[0-4]\\d|25[0-5])\\.([01]?\\d\\d?|2[0-4]\\d|25[0-5])\\.([01]?\\d\\d?|2[0-4]\\d|25[0-5])$";
                                if (Regex.IsMatch(ipAddress, IPV4_PATTERN))
                                {
                                    Comboboxnics.Add(ipAddress);
                                    Logger.Debug("nic/ip: " + nic.Name + " [" + ipAddress + "]");
                                }
                            }

                        }
                    }
                }
            }
            catch (Exception ex)
            {
                string errmsg = "Unexptected error occurred while fetching the networks.";
                Logger.Debug("Exception occurred in getNetworkInterfaces" + ex.ToString());
                MessageBox.Show(this, errmsg, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1);
            }
            Logger.Debug("Exited csform:getNetworkInterfaces");

            return Comboboxnics.ToArray();
        }

        private Boolean isValidNIC(NetworkInterface nic)
        {
            Boolean result = false;

            //Check if the NIC supports IPV4 protocol
            if (nic.Supports(NetworkInterfaceComponent.IPv4) == true)
            {
                //Check if NIC interface type is either Ethernet based or Wireless(WIFI) based.
                switch (nic.NetworkInterfaceType)
                {
                    case NetworkInterfaceType.Ethernet:
                    case NetworkInterfaceType.Ethernet3Megabit:
                    case NetworkInterfaceType.FastEthernetFx:
                    case NetworkInterfaceType.FastEthernetT:
                    case NetworkInterfaceType.GigabitEthernet:
                    case NetworkInterfaceType.Wireless80211:
                        {
                            // Only show NIC cards, which are in operational state i.e. LAN Wire or WIFI is already connected.
                            if (nic.OperationalStatus == OperationalStatus.Up)
                            {
                                result = true;
                            }
                            break;
                        }
                }
            }
            return result;
        }
    }
}
