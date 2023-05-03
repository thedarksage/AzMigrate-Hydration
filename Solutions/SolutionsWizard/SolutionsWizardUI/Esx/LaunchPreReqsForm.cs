using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;
using System.Diagnostics;
using Com.Inmage.Tools;
using Com.Inmage.Esxcalls;
using System.Threading;
using System.Globalization;

namespace Com.Inmage.Wizard
{
    public partial class LaunchPreReqsForm : Form
    {
        internal System.Drawing.Bitmap scoutLaunching;
        internal bool preresPassed = false;
        public LaunchPreReqsForm()
        {
            InitializeComponent();
            scoutLaunching = Wizard.Properties.Resources.scoutLaunching;
            if (HelpForcx.Brandingcode == 1)
            {
                this.BackgroundImage = scoutLaunching;
            }
           // PreReqs();
            
        }

        public bool ShowForm()
        {
            try
            {
                
                //this.ShowDialog();
                launchBackgroundWorker.RunWorkerAsync();
                this.ShowDialog();
                return true;
            }
            catch (Exception e)
            {
                Trace.WriteLine(DateTime.Now + " \t Caught an exception at Launch " + e.Message);
            }
            return true;
        }


        public bool PreReqs()
        {
            Trace.WriteLine("**********************************Launching vContinuum***********************************");
            int rt = Esx.PerlInstalled();
            if (rt != 0)
            {
                MessageBox.Show("VMWare VSphere CLI is not installed on this system....exiting", "vContinuum error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                preresPassed = false;
                return false;
            }
            Esx e = new Esx();
           
            e.GetRcliVersion();
            string versionOutput = WinTools.Output;
            Trace.WriteLine(DateTime.Now + "\t Printing the out put " + versionOutput);
            if (versionOutput.Contains("5."))
            {
                Trace.WriteLine(DateTime.Now + " \t the esx.pl version is 5.0");
            }
            else
            {
                MessageBox.Show("vSphere cli 5.0 or later version is not installed on this host. Please install vSphere cli 5.0 and try again.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                preresPassed = false;
                return false;
            }
            if (WinTools.ServiceExist("frsvc") == false)
            {
                Trace.WriteLine(DateTime.Now + "Fx agent is not installed on this host");
                MessageBox.Show("Fx Agent is not installed on this host" + Environment.NewLine + "Install Fx agent and proceed again", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                preresPassed = false;
                return false;
            }


            // while launching vContinuum Wizard. As part of pre req checks wizard verifies whether to use http or https for cx apis and cxcli calls
            if (WinTools.GetHttpsOrHttp() == false)
            {
                WinTools.Https = false;
                Trace.WriteLine(DateTime.Now + "\t Https is false");
            }
            else
            {
                WinTools.Https = true;
                Trace.WriteLine(DateTime.Now + "\t Https is True");
            }

            if(WinTools.Https == false)
            {
                string message = "You have choosen non secure communication(http) between vContinuum and CS server. We always recommend to use secure communication(https).Would you like to continue with Http";
                string title = "Http";
                DialogResult result = MessageBox.Show(message,title, MessageBoxButtons.YesNo, MessageBoxIcon.Error);
                if(result == System.Windows.Forms.DialogResult.No)
                {
                    return false;
                }
               
            }

            if (WinTools.AddFingerPathToRegistry() == true)
            {
                Trace.WriteLine(DateTime.Now + "\t Fingerprints path is there in registry");
            }
            //This will modify CXPS.conf file to allow directories
            WinTools.ModifycxpsConfig();

            if (CxprereqChecks() == false)
            {
                return false;
            } 
            try
            {
                string language = CultureInfo.CurrentCulture.Name;
                Trace.WriteLine(DateTime.Now + "\t Language name of vContinumm box " + language);
                if (language.Contains("en-US") || language.Contains("en-GB") || language.ToUpper().Contains("EN-GB") || language.ToUpper().Contains("EN-AU") || language.Contains("en-AU") || language.Contains("en") || language.ToUpper().Contains("EN")) 
                {

                }
                else
                {
                    MessageBox.Show("This vContinuum server is having different language other than English. Please select any English language to proceed." + Environment.NewLine
                        + "Please change to any one of them to proceed." + Environment.NewLine + " Steps to change: " + Environment.NewLine + "Control Panel-> Region and Language ->  Format(tab) -> Change the format to English(US)", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return false;
                }
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed to get the language name " + ex.Message);
            }
           // _preresPassed = true;
           // this.Close();
            return true;
        }

        public bool CxprereqChecks()
        {
            Trace.WriteLine(DateTime.Now + "\t Entered to check the CX is working or not by pinging cx and getting ps ips");
            WinPreReqs winPrereq = new WinPreReqs("", "", "", "");
            if (CxReachable() == true)
            {

                if (winPrereq.CheckIfcxWorking() == true)
                {
                    Trace.WriteLine(DateTime.Now + "\t Completed CX prereqchecks. Result is Success");
                    preresPassed = true;
                    return true;
                }
                else
                {
                    Trace.WriteLine(DateTime.Now + "\t Completed CX prereqchecks. Result is Failed to get ps ips");
                    string message = Environment.NewLine + "Coudn't contact CX server: " + WinTools.GetcxIP() + " at port number: " + WinTools.GetcxPortNumber() +
                    Environment.NewLine +
                    Environment.NewLine + "1. Make sure that agent configuration settings are set to point to the correct CX and port number using the vContinuum->Agent Configuration wizard." +
                    Environment.NewLine + 
                      Environment.NewLine+  "2. Check whether proxy server configured in the web browser (IE&Firefox)." ;

                    MessageBox.Show(message, "vContinuum", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    preresPassed = false;
                    return false;

                }


            }
            else
            {
                Trace.WriteLine(DateTime.Now + "\t Completed CX prereqchecks. Result is Failed to ping cx");
                MessageBox.Show("Unable to ping CX server:" + WinTools.GetcxIP() + Environment.NewLine + "Make sure that agent configuration settings are set to point to the correct CX and port number using the vContinuum->Agent Configuration wizard.", "vContinuum", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return false;

            }
          
        }


        private bool CxReachable()
        {
           /* bool cxReachable = false;

            if (WinPreReqs.IpReachable(WinTools.GetcxIP()) == false)
            {

                if (WinPreReqs.IpReachable(WinTools.GetcxIP()) == false)
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
            }

            return cxReachable;
            */
            return true;
        }

        private void LaunchPreReqsForm_Load(object sender, EventArgs e)
        {
           // this.Close();
        }

        private void launchBackgroundWorker_DoWork(object sender, DoWorkEventArgs e)
        {
            PreReqs();
        }

        private void launchBackgroundWorker_ProgressChanged(object sender, ProgressChangedEventArgs e)
        {
           // textLabel.Text = "Checking for rcli version";

        }

        private void launchBackgroundWorker_RunWorkerCompleted(object sender, RunWorkerCompletedEventArgs e)
        {
            this.Close();
        }


    }
}
