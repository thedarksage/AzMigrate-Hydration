using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Threading;
using System.Windows.Forms;
using System.Runtime.InteropServices;
using Logging;
using Microsoft.Win32;
using System.Text.RegularExpressions;
using System.IO;

namespace cspsconfigtool
{
    public partial class psform : Form
    {
        private delegate void HideSvcStopMsg();
        private delegate void DisplaySvcStopMsg();

        private delegate void HideIpPortPassphraseLableBoxes();

        private delegate void HideSvcStartMsg();
        private delegate void DisplaySvcStartMsg();

        private delegate void HideInitialUpdateMsg();
        private delegate void DisplayInitialUpdateMsg();

        private delegate void ReadOnlyTrueTextFeilds();
        private delegate void ReadOnlyFalseTextFeilds();

        private delegate void HideValueUpdateMessage();
        private delegate void DisplayValueUpdateMessage();

        private delegate void EnteredConfigFeildsDisable();
        private delegate void EnteredConfigFeildsEnable();

        public bool stopsvc = false;
        public bool checkupdates = false;

        // Constants
        internal const int SV_OK = 0;
        internal const int SV_FAIL = 1;

        Thread m_SaveButtonThread;
        bool constCretedThreadstarted;
        Thread localSaveButtonThread;
        bool localCreatedThreadstarted;

        Thread cancelButtonThread;
        public Thread initThread;

        bool processingOnClosingTask;


        [DllImport("configtool.dll", EntryPoint = "freeAllocatedBuf")]
        public static extern void freeAllocatedBuf(out IntPtr allocatedDllBuf);

        [DllImport("configtool.dll")]
        public static extern int UpdatePropertyValues([MarshalAs(UnmanagedType.LPStr)] string amethystinput, [MarshalAs(UnmanagedType.LPStr)] string pushinput, [MarshalAs(UnmanagedType.LPStr)] string cxpsstinput, out IntPtr err);




        public void MakeReadOnlyComponent()
        {
            if (groupBox2.InvokeRequired)
            {
                ReadOnlyTrueTextFeilds call = new psform.ReadOnlyTrueTextFeilds(() =>
                {
                    this.textBox1.ReadOnly = true;
                    this.textBox2.ReadOnly = true;
                    this.textBox3.ReadOnly = true;
                    this.textBox4.ReadOnly = true;
                });

                groupBox2.Invoke(call);
            }
            else
            {
                this.textBox1.ReadOnly = true;
                this.textBox2.ReadOnly = true;
                this.textBox3.ReadOnly = true;
                this.textBox4.ReadOnly = true;
            }
        }
        public void MakeReadOnlyFalse()
        {
            if (groupBox2.InvokeRequired)
            {
                ReadOnlyFalseTextFeilds call = new psform.ReadOnlyFalseTextFeilds(() =>
                {
                    this.textBox1.ReadOnly = false;
                    this.textBox2.ReadOnly = false;
                    this.textBox3.ReadOnly = false;
                    this.textBox4.ReadOnly = false;
                });

                groupBox2.Invoke(call);
            }
            else
            {
                this.textBox1.ReadOnly = false;
                this.textBox2.ReadOnly = false;
                this.textBox3.ReadOnly = false;
                this.textBox4.ReadOnly = false;
            }
        }

        public void MakeIpPortPassphraseLableBoxesHide()
        {
            if (groupBox2.InvokeRequired)
            {
                HideIpPortPassphraseLableBoxes call = new psform.HideIpPortPassphraseLableBoxes(() =>
                {
                    this.label1.Visible = false;
                    this.label2.Visible = false;
                    this.label3.Visible = false;

                    this.textBox1.Visible = false;
                    this.textBox2.Visible = false;
                    this.textBox3.Visible = false;
                });

                groupBox2.Invoke(call);
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

        public void MakeInitialUpdateMsgHide()
        {
            if (groupBox2.InvokeRequired)
            {
                HideValueUpdateMessage call = new psform.HideValueUpdateMessage(() =>
                {
                    this.label5.Visible = false;
                });

                groupBox2.Invoke(call);
            }
            else
            {
                this.label5.Visible = false;
            }
        }
        public void MakeInitialUpdateMsgVisible()
        {
            if (groupBox2.InvokeRequired)
            {
                DisplayValueUpdateMessage call = new psform.DisplayValueUpdateMessage(() =>
                {
                    this.label5.Visible = true;
                });

                groupBox2.Invoke(call);
            }
            else
            {
                this.label5.Visible = true;
            }
        }
        public void MakeSvcStopMsgHide()
        {
            if (groupBox2.InvokeRequired)
            {
                HideSvcStopMsg call = new psform.HideSvcStopMsg(() =>
                {
                    // do control stuff
                    this.label4.Visible = false;
                });

                groupBox2.Invoke(call);
            }
            else
            {
                this.label4.Visible = false;
            }
        }
        public void MakeSvcStopMsgVisible()
        {
            if (groupBox2.InvokeRequired)
            {
                DisplaySvcStopMsg call = new psform.DisplaySvcStopMsg(() =>
                {
                    // do control stuff
                    this.label4.Visible = true;
                });

                groupBox2.Invoke(call);
            }
            else
            {
                this.label4.Visible = true;
            }
        }

        public void MakeSvcStartMessageHide()
        {
            if (groupBox2.InvokeRequired)
            {
                HideSvcStartMsg call = new psform.HideSvcStartMsg(() =>
                {
                    this.label6.Visible = false;
                });

                groupBox2.Invoke(call);
            }
            else
            {
                this.label6.Visible = false;
            }
        }
        public void MakeSvcStartMessageVisible()
        {
            if (groupBox2.InvokeRequired)
            {
                DisplaySvcStartMsg call = new psform.DisplaySvcStartMsg(() =>
                {
                    this.label6.Visible = true;
                });

                groupBox2.Invoke(call);
            }
            else
            {
                this.label6.Visible = true;
            }
        }
        public void MakeValueUpdateMessageHide()
        {
            if (groupBox2.InvokeRequired)
            {
                HideValueUpdateMessage call = new psform.HideValueUpdateMessage(() =>
                {
                    this.label8.Visible = false;
                });

                groupBox2.Invoke(call);
            }
            else
            {
                this.label8.Visible = false;
            }
        }
        public void MakeValueUpdateMessageVisible()
        {
            if (groupBox2.InvokeRequired)
            {
                DisplayValueUpdateMessage call = new psform.DisplayValueUpdateMessage(() =>
                {
                    this.label8.Visible = true;
                });

                groupBox2.Invoke(call);
            }
            else
            {
                this.label8.Visible = true;
            }
        }

        public void DisableEnteredConfigFeilds()
        {
            if (groupBox1.InvokeRequired)
            {
                EnteredConfigFeildsDisable call = new psform.EnteredConfigFeildsDisable(() =>
                {
                    this.textBox1.Enabled = false;
                    this.textBox2.Enabled = false;
                    this.textBox3.Enabled = false;
                    this.textBox4.Enabled = false;
                    this.signatureVerificationCheckBox.Enabled = false;
                });

                groupBox1.Invoke(call);
            }
            else
            {
                this.textBox1.Enabled = false;
                this.textBox2.Enabled = false;
                this.textBox3.Enabled = false;
                this.textBox4.Enabled = false;
                this.signatureVerificationCheckBox.Enabled = false;
            }
        }

        public void EnableEnteredConfigFeilds()
        {
            if (groupBox1.InvokeRequired)
            {
                EnteredConfigFeildsEnable call = new psform.EnteredConfigFeildsEnable(() =>
                {
                    this.textBox1.Enabled = true;
                    this.textBox2.Enabled = true;
                    this.textBox3.Enabled = true;
                    this.textBox4.Enabled = true;
                    this.signatureVerificationCheckBox.Enabled = true;
                });

                groupBox1.Invoke(call);
            }
            else
            {
                this.textBox1.Enabled = true;
                this.textBox2.Enabled = true;
                this.textBox3.Enabled = true;
                this.textBox4.Enabled = true;
                this.signatureVerificationCheckBox.Enabled = true;
            }
        }

        public void MakeButtonDisable()
        {
            this.savebutton.Enabled = false;
            this.cancelbutton2.Enabled = false;
        }
        public void MakeButtonEnable()
        {
            this.savebutton.Enabled = true;
            this.cancelbutton2.Enabled = true;
        }

        public string getEnteredIp()
        {
            return this.textBox1.Text;
        }
        public string getEnteredPort()
        {
            return this.textBox2.Text;
        }

        public void setCsPsIp(string pscsip, string pscsport)
        {
            this.textBox1.Text = pscsip;
            this.textBox2.Text = pscsport;
        }
        public void setPassphrase(string pphrase)
        {
            this.textBox3.Text = pphrase;
        }
        public void setSSLPort(string sslport)
        {
            this.textBox4.Text = sslport;
        }
        public psform()
        {
            InitializeComponent();
            this.MakeInitialUpdateMsgHide();
            this.MakeSvcStopMsgHide();
            this.MakeSvcStartMessageHide();
            this.MakeValueUpdateMessageHide();
            this.textBox3.PasswordChar = '*';
            initThread = new Thread(InitProcessFunction);
            m_SaveButtonThread = new Thread(OnClickSaveButtonTask);
            cancelButtonThread = new Thread(OnClickCancelButtonTask);
            localSaveButtonThread = null;
            constCretedThreadstarted = false;
            localCreatedThreadstarted = false;
            processingOnClosingTask = false;
            this.signatureVerificationCheckBox.Checked = false;
        }

        private void GetResources()
        {
            this.Text = ResourceHelper.GetStringResourceValue("ps");
            this.label7.Text = ResourceHelper.GetStringResourceValue("csdetails");
            this.label9.Text = ResourceHelper.GetStringResourceValue("psreguseraction");
            this.label1.Text = ResourceHelper.GetStringResourceValue("csip");
            this.label2.Text = ResourceHelper.GetStringResourceValue("csport");
            this.label3.Text = ResourceHelper.GetStringResourceValue("connectionpassphrase");
            this.savebutton.Text = ResourceHelper.GetStringResourceValue("save");
            this.cancelbutton2.Text = ResourceHelper.GetStringResourceValue("cancel");
        }

        private void configSaveButton_Click(object sender, EventArgs e)
        {
            MakeButtonDisable();
            DisableEnteredConfigFeilds();
            if (!constCretedThreadstarted)
            {
                m_SaveButtonThread.Start(this);
                constCretedThreadstarted = true; 
            }
            else
            {
                localSaveButtonThread = new Thread(OnClickSaveButtonTask);
                localSaveButtonThread.Start(this);
                localCreatedThreadstarted = true;
            }
        }

        private void OnClickSaveButtonTask(object obj)
        {

            Logger.Debug("Entered psform:OnClickSaveButtonTask.");

            Services.Helpers hs = new Services.Helpers();
            psform ps = (psform)obj;

            string errmsg = string.Empty;

            if (!hs.ValidateUserInput(ps))
            {
                MakeButtonEnable();
                EnableEnteredConfigFeilds();
                return;
            }
            
                string passphrase = textBox3.Text;
                var ifMatched = Regex.Match(passphrase, @"[^ \a\t\n\b\r]");

                int found = ifMatched.Success ? ifMatched.Index : -1;
                if (found == -1)
                {
                    MessageBox.Show(this, "Invalid Connection Passphrase.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1);
                    MakeButtonEnable();
                    EnableEnteredConfigFeilds();
                    return;
                }
                if (passphrase.Length < 16)
                {
                    MessageBox.Show(this, "Passphrase length should be atleast 16 characters.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1);
                    MakeButtonEnable();
                    EnableEnteredConfigFeilds();
                    return;
                }
           
            MakeSvcStopMsgVisible();
            MakeReadOnlyComponent();
            Thread.Sleep(10000);
            if (hs.stopservices(ref errmsg))
            {
                Logger.Debug("Successfully stopped services.");
                MakeSvcStopMsgHide();
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

            if (!checkupdates)
            {
                MakeInitialUpdateMsgVisible();
                Thread.Sleep(1000);
                if (hs.processinitUpdates(ref errmsg))
                {
                    Logger.Debug("Successfully Processed Initial Conf Updates.");

                    Thread.Sleep(1000);
                    checkupdates = true;
                }
                else
                {
                    Logger.Error("Failed to Process Initial Conf Updates error: " + errmsg);
                    MessageBox.Show(this, "Failed Process Initial Conf Updates error: " + errmsg, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1);
                }
                MakeInitialUpdateMsgHide();

            }
            bool updateProperties = true;
            bool isUpdatedEnteredValue = true;

            if (checkupdates)
            {

                Logger.Debug("updating Entered cx ip,port .");

                int ret = SV_OK;

                try
                {
                    string pscsip = "PS_CS_IP";
                    pscsip += "=";
                    pscsip += this.textBox1.Text;
                    string dbhost = "DB_HOST";
                    dbhost += "=";
                    dbhost += this.textBox1.Text;
                    string pscsport = "PS_CS_PORT";
                    pscsport += "=";
                    pscsport += this.textBox2.Text;

                    string areq = pscsip + ";" + dbhost + ";" + pscsport;

                    string pushhostname = "Hostname";
                    pushhostname += "=";
                    pushhostname += this.textBox1.Text;

                    string pushport = "Port";
                    pushport += "=";
                    pushport += this.textBox2.Text;

                    string preq = pushhostname + ";" + pushport;

                    string dataTransFerSecurePort = "sslport";
                    dataTransFerSecurePort += "=";
                    if (string.IsNullOrEmpty(this.textBox4.Text))
                    {
                        dataTransFerSecurePort += "9443";
                    }
                    else
                    {
                        dataTransFerSecurePort += this.textBox4.Text;
                    }

                    string creq = dataTransFerSecurePort;

                    IntPtr responsePtr = IntPtr.Zero;

                    MakeValueUpdateMessageVisible();
                    Thread.Sleep(1000);
                    ret = UpdatePropertyValues(areq, preq, creq, out responsePtr);
                    if (ret != SV_OK)
                    {
                        Logger.Error("Failed : updating Entered cx ip,port and passphrase.");
                        String response = Marshal.PtrToStringAnsi(responsePtr);
                        freeAllocatedBuf(out responsePtr);
                        MessageBox.Show(this, response, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1);
                        updateProperties = false;
                        isUpdatedEnteredValue = false;

                    }
                    else
                    {
                        Logger.Debug("Updated successfully the entered cx ip,port and passphrase.");
                    }
                    MakeValueUpdateMessageHide();
                }
                catch (Exception exc)
                {
                    System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(exc, true);
                    MessageBox.Show(this, trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber() + " Exception: " + exc.Message,
                         "Error", MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1);
                    updateProperties = false;
                    isUpdatedEnteredValue = false;
                    MakeValueUpdateMessageHide();
                }



                if (updateProperties)
                {
                    try
                    {
                        MakeValueUpdateMessageVisible();
                        Thread.Sleep(1000);
                        Logger.Debug("updating Entered passphrase");

                        string enteredip = "PS_CS_IP";
                        enteredip += "=";
                        enteredip += this.textBox1.Text;

                        string enteredport = "PS_CS_PORT";
                        enteredport += "=";
                        enteredport += this.textBox2.Text;

                        string pphrase = "passphrase";
                        pphrase += "=";
                        pphrase += this.textBox3.Text;

                        string request = enteredip + ";" + enteredport + ";" + pphrase;

                        Logger.Debug(enteredip + " ; " + enteredport );

                        if (hs.processpassphrase(request, ref errmsg))
                        {
                            Logger.Debug("Successfully Processed passphrase.");
                        }
                        else
                        {
                            isUpdatedEnteredValue = false;
                            Logger.Error("Failed to Processed passphrase.: " + errmsg); 
							string erms = "Error:\n" + errmsg + "\n\nIf you forgot passphrase, follow below steps to get the current passphrase:\n1. Login to Config Server and open command prompt with admin privileges.\n2. Change directory to CS Installation directory\nEg: C:\\ProgramData\\ASR\\home\\svsystems\\bin\n3. Run the following command \"genpassphrase.exe -n\"";
                            MessageBox.Show(this, erms, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1);
                            MakeValueUpdateMessageHide();
                            MakeButtonEnable();
                            EnableEnteredConfigFeilds();
                            MakeReadOnlyFalse();
                            return;
                        }
                        MakeValueUpdateMessageHide();
                        Thread.Sleep(1000);
                    }
                    catch (Exception exc)
                    {
                        System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(exc, true);
                        MessageBox.Show(this, trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber() + " Exception: " + exc.Message,
                             "Error", MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1);
                    }
                }
            }
            try
            {
                errmsg = string.Empty;
                Logger.Debug("Starting the Services");

                MakeSvcStartMessageVisible();
                Thread.Sleep(1000);
                if (hs.startservices(ref errmsg))
                {
                    Logger.Debug("Successfully started services.");
                    Thread.Sleep(1000);
                    stopsvc = false;

                }
                else
                {
                    Logger.Error("Failed to start services, error: " + errmsg);
                    string err = string.Empty;
                    if (checkupdates && updateProperties && isUpdatedEnteredValue)
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
            MakeSvcStartMessageHide();
            MakeReadOnlyFalse();
            MakeButtonEnable();
            EnableEnteredConfigFeilds();
            if (!checkupdates || !updateProperties || !isUpdatedEnteredValue)
            {
                Logger.Debug("Exiting Application");
                Logger.Close();
                System.Environment.Exit(1);
            }
            else
            {
                MakeButtonDisable();
                MakeReadOnlyComponent();
                if(!stopsvc)
                {
                    MessageBox.Show(this, "Updated Successfully.", "Information", MessageBoxButtons.OK, MessageBoxIcon.Information, MessageBoxDefaultButton.Button1);
                }
                Logger.Debug("Exited psform:OnClickSaveButtonTask.");
                Logger.Debug("Exiting Application");
                Logger.Close();
                System.Environment.Exit(0);
            }

            Logger.Debug("Exited psform:OnClickSaveButtonTask.");
        }

        private void label4_Click(object sender, EventArgs e)
        {

        }

        private void tabPage1_Click(object sender, EventArgs e)
        {

        }

        private void configCancelbutton_Click(object sender, EventArgs e)
        {
            MakeButtonDisable();
            DisableEnteredConfigFeilds();
            cancelButtonThread.Start(this);
        }

        private void OnClickCancelButtonTask(object obj)
        {
            Services.Helpers hs = new Services.Helpers();
            if (stopsvc)
            {
                try
                {
                    string err = string.Empty;
                    Logger.Debug("Starting the Services");

                    MakeSvcStartMessageVisible();
                    Thread.Sleep(1000);
                    if (hs.startservices(ref err))
                    {
                        Logger.Debug("Successfully started services.");
                        MakeSvcStartMessageHide();
                        Thread.Sleep(1000);
                        stopsvc = false;

                    }
                    else
                    {
                        Logger.Error("Failed to start services, error: " + err);
                        MessageBox.Show(this, err, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1);
                    }
                }
                catch (Exception exc)
                {
                    System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(exc, true);
                    MessageBox.Show(this, trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber() + " Exception: " + exc.Message,
                         "Error", MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1);
                }

            }
            Logger.Debug("Clicked on Cancel Button");
            Logger.Debug("Exiting Application");
            Logger.Close();
            System.Environment.Exit(0);
        }

        private void InitProcessFunction(object obj)
        {
            Logger.Debug("Entered Program:InitProcessFunction.");
            Services.Helpers hs = new Services.Helpers();
            psform ps = (psform)obj;
            bool isValRetirevedProperly = true;

            string pscsip = string.Empty;
            string pscsport = string.Empty;
            string pphrase = string.Empty;
            string sslport = string.Empty;
            string dvacpPort = string.Empty;
            bool isCS = false;
            string errmsg = string.Empty;
            bool isSigVerificationReq = true;
            string azurecsip = string.Empty;

            ps.MakeButtonDisable();
            Thread.Sleep(1000);

            if (hs.initprocessfunction(ps, ref  pscsip, ref pscsport, ref pphrase, ref sslport, ref dvacpPort, ref errmsg, ref isCS, ref azurecsip))
            {
                if (!pscsip.Equals("\"\"", StringComparison.Ordinal) && !pscsport.Equals("\"\"", StringComparison.Ordinal))
                {
                    Logger.Debug("Exist PS_CS_IP : " + pscsip + " and PS_CS_PORT : " + pscsport);

                    if ((pscsip[0] == '"' && pscsip[pscsip.Length - 1] == '"'))
                        pscsip = pscsip.Replace("\"", "");
                    if ((pscsport[0] == '"' && pscsport[pscsport.Length - 1] == '"'))
                        pscsport = pscsport.Replace("\"", "");

                    ps.setCsPsIp(pscsip, pscsport);
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
                    ps.setPassphrase(pphrase);
                }

                if (sslport.Equals("Empty", StringComparison.Ordinal))
                {
                    Logger.Debug("ssl_port is Empty");
                }
                else
                {
                    ps.setSSLPort(sslport);
                }

                if (hs.GetSignatureVerificationChecksConfig(ref isSigVerificationReq, ref errmsg))
                {
                    this.signatureVerificationCheckBox.Checked = isSigVerificationReq;
                }
                else
                {
                    isValRetirevedProperly = false;
                }

                ps.MakeButtonEnable();
                Thread.Sleep(1000);
            }
            else
            {
                isValRetirevedProperly = false;
            }

            if (!isValRetirevedProperly)
            {
                Logger.Error("Failed Program:InitProcessFunction, Error: " + errmsg);
                MessageBox.Show(this, errmsg, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1);
                
                Logger.Close();
                System.Environment.Exit(1);
            }
            Logger.Debug("Exited Program:InitProcessFunction.");
        }

        private void OnPSForm_Closing(object sender, System.ComponentModel.CancelEventArgs e)
        {
            Logger.Debug("Entered OnPSForm_Closing ");

            if (!processingOnClosingTask)
            {
                processingOnClosingTask = true;
                bool isBackgroundProcRunning = false;
                // Display a MsgBox asking the user to save changes or abort. 
                if (initThread.IsAlive)
                {
                    Logger.Debug("initThread is running ");
                    isBackgroundProcRunning = true;
                }
                else if (constCretedThreadstarted && this.m_SaveButtonThread.IsAlive)
                {
                    Logger.Debug("constCretedThreadstarted is running ");
                    isBackgroundProcRunning = true;
                }
                else if (localCreatedThreadstarted && localSaveButtonThread.IsAlive)
                {
                    Logger.Debug("localCreatedThreadstarted is running ");
                    isBackgroundProcRunning = true;
                }
                else if (cancelButtonThread.IsAlive)
                {
                    Logger.Debug("cancelButtonThread is running ");
                    isBackgroundProcRunning = true;
                }
                if (isBackgroundProcRunning)
                {
                    MessageBox.Show(this, "Backgroud process is going on, please try after some time");
                    // Cancel the Closing event from closing the form.
                    e.Cancel = true;
                }
                else if (stopsvc)
                {
                    MakeButtonDisable();
                    Services.Helpers hs = new Services.Helpers();
                    try
                    {
                        string err = string.Empty;
                        Logger.Debug("Starting the Services");

                        //MakeSvcStartMessageVisible();
                        //Thread.Sleep(1000);
                        if (hs.startservices(ref err))
                        {
                            Logger.Debug("Successfully started services.");
                            //MakeSvcStartMessageHide();
                            stopsvc = false;
                        }
                        else
                        {
                            Logger.Error("Failed to start services, error: " + err);
                            MessageBox.Show(this, err, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1);
                            
                        }
                    }
                    catch (Exception exc)
                    {
                        System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(exc, true);
                        MessageBox.Show(this, trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber() + " Exception: " + exc.Message,
                             "Error", MessageBoxButtons.OK, MessageBoxIcon.Error, MessageBoxDefaultButton.Button1);
                    }
                }
                processingOnClosingTask = false;   
            }
            
            Logger.Debug("Exited OnPSForm_Closing ");
            Logger.Close();
        }
       
    }
}
