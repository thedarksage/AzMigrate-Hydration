using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;
using System.Net.Sockets;
using System.Diagnostics;
using System.Text.RegularExpressions;
using System.Collections;
using Com.Inmage.Esxcalls;
namespace Com.Inmage.Wizard
{
    public partial class NicPropertiesForm : Form
    {
        internal bool canceled = true;
        internal bool dhcp = false;
        internal string ipSelected = null;
        internal string dnsSelected = null;
        internal string subNetMaskSelected = null;
        internal string defaultGateWaySelected = null;
        internal string vmNetWorkLabelSelected = null;
        internal OStype osTypeSelected = OStype.Windows;
        internal TextBox[] ipText;
        internal TextBox[] subnetMaskText;
        internal TextBox[] dnsText;
        internal TextBox[] gateWayText;
        internal string defaultGateWay = null;
        internal string defaultdns = null;
        internal string cacheNetworkName = null;
        internal string cacheSubNetmask = null;
        internal string cacheGateway = null;
        internal string cacheDNS = null;
        internal int returnCode = 0;
        internal Host masterHost = new Host();
        int tabIndex = 0;
        public NicPropertiesForm(OStype osType)
        {

            InitializeComponent();
            osTypeSelected = osType;
            try
            {
                if (gateWayText != null)
                {
                    if (gateWayText[0].Text.Length != 0)
                    {
                        defaultGateWay = gateWayText[0].Text;
                    }
                }
                if (dnsText != null)
                {

                    if (dnsText[0].Text.Length != 0)
                    {
                        defaultdns = dnsText[0].Text;
                    }
                }
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed to read the values from gateway and dns text boxes " + ex.Message);
            }
            
            Trace.WriteLine(DateTime.Now + "\t Printing the tab count " + ipTabControl.TabCount.ToString());
           
            
        }

        private void cancelButton_Click(object sender, EventArgs e)
        {
           

            canceled = true;
            Close();
        }

        private void addButton_Click(object sender, EventArgs e)
        {
            try
            {
                for (int i = 0; i < ipTabControl.TabCount; i++)
                {
                    if (i != tabIndex)
                    {
                        if (gateWayText[tabIndex].Text.Length != 0)
                        {
                            if (gateWayText[i].Text.Length == 0)
                            {
                                gateWayText[i].Text = gateWayText[tabIndex].Text;
                            }
                        }
                        if (dnsText[tabIndex].Text.Length != 0 )
                        {
                            if (dnsText[i].Text.Length == 0)
                            {
                                dnsText[i].Text = dnsText[tabIndex].Text;
                            }
                            else
                            {

                                if (osTypeSelected == OStype.Linux || osTypeSelected == OStype.Solaris)
                                {
                                    if (dnsText[tabIndex].Text.Contains(","))
                                    {
                                        MessageBox.Show("Multiple DNS IP for Linux platforms is not supported. Please enter single IP", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                        return;
                                    }
                                }
                            }
                        }
                        if (subnetMaskText[tabIndex].Text.Length != 0)
                        {
                            if (subnetMaskText[i].Text.Length == 0)
                            {
                                subnetMaskText[i].Text = subnetMaskText[tabIndex].Text;
                            }
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed to update the null values " + ex.Message);
            }


            if (PreReqs() == true)
            {
                if (dhcp == false)
                { 
                 
                    
                    vmNetWorkLabelSelected = netWorkComboBox.Text.ToString();
                    Trace.WriteLine(DateTime.Now + "\t Came here to print network value " + netWorkComboBox.Text);
                }
                else
                {
                    vmNetWorkLabelSelected = netWorkComboBox.Text;
                }
                cacheNetworkName = vmNetWorkLabelSelected;
                canceled = false;
                Close();
            }

        }

        private bool PreReqs()
        {
            if (useDhcpRadioButton.Checked == false)
            {
                for (int i = 0; i < ipTabControl.TabCount; i++)
                {
                    try
                    {
                        if (ipText[i].Text.Length == 0)
                        {
                            MessageBox.Show("Enter IP address for the IP-Address:"+ (i+1).ToString(), "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            return false;
                        }
                        else
                        {
                            System.Net.IPAddress ipAddress = null;
                            bool isValidIp = System.Net.IPAddress.TryParse(ipText[i].Text, out ipAddress);
                            if (isValidIp == false)
                            {
                                MessageBox.Show("Enter a valid IP address for IP-Address:" + (i+1).ToString(), "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                return false;
                            }
                            if (IPRangeValiation(ipText[i].Text) == false)
                            {
                                MessageBox.Show("Enter a valid IP address for IP-Address:" + (i + 1).ToString(), "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                return false;
                            }
                        }
                    }
                    catch (Exception ex)
                    {
                        System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                        Trace.WriteLine("ERROR*******************************************");
                        Trace.WriteLine(DateTime.Now + "Caught exception at Method: PreReqs of Nic properties form " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                        Trace.WriteLine("Exception  =  " + ex.Message);
                        Trace.WriteLine("ERROR___________________________________________");
                    }
                   
                    try
                    {
                        if (subnetMaskText[i].Text.Length == 0)
                        {
                            MessageBox.Show("Enter Subnet mask IP address for IP-Address:"+(i+1).ToString(), "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            return false;
                        }
                        else
                        {
                            System.Net.IPAddress ipAddress = null;
                          
                            bool isValidIp = System.Net.IPAddress.TryParse(subnetMaskText[i].Text, out ipAddress);
                            if (isValidIp == false)
                            {
                                MessageBox.Show("Enter a valid subnet mask IP address for IP-Address:"+ (i+1).ToString(), "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                return false;
                            }
                            if (IPRangeValiation(subnetMaskText[i].Text) == false)
                            {
                                MessageBox.Show("Enter a valid subnet mask IP address for IP-Address:" + (i + 1).ToString(), "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                return false;
                            }
                            cacheSubNetmask = subnetMaskText[i].Text;
                        }
                    }
                    catch (Exception ex)
                    {
                        System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                        Trace.WriteLine("ERROR*******************************************");
                        Trace.WriteLine(DateTime.Now + "Caught exception at Method: PreReqs of Nic properties form " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                        Trace.WriteLine("Exception  =  " + ex.Message);
                        Trace.WriteLine("ERROR___________________________________________");
                    }

                    try
                    {
                        if (gateWayText[i].Text.Length != 0)
                        {
                            System.Net.IPAddress ipAddress = null;
                            bool isValidIp = System.Net.IPAddress.TryParse(gateWayText[i].Text, out ipAddress);
                            if (isValidIp == false)
                            {
                                MessageBox.Show("Enter a valid gateway  IP address for IP-Address:" + (i + 1).ToString(), "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                return false;
                            }
                            if (IPRangeValiation(gateWayText[i].Text) == false)
                            {
                                MessageBox.Show("Enter a valid gateway  IP address for IP-Address:" + (i + 1).ToString(), "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                return false;
                            }
                            cacheGateway = gateWayText[i].Text;
                        }
                    }
                    catch (Exception ex)
                    {
                        System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                        Trace.WriteLine("ERROR*******************************************");
                        Trace.WriteLine(DateTime.Now + "Caught exception at Method: PreReqs of Nic properties form " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                        Trace.WriteLine("Exception  =  " + ex.Message);
                        Trace.WriteLine("ERROR___________________________________________");
                    }
                    try
                    {
                        if (dnsText[i].Text.Length != 0)
                        {
                            string[] dnsips = dnsText[i].Text.Split(',');

                            foreach (string dnsip in dnsips)
                            {
                                System.Net.IPAddress ipAddress = null;
                                bool isValidIp = System.Net.IPAddress.TryParse(dnsip, out ipAddress);
                                if (isValidIp == false)
                                {
                                    MessageBox.Show("Enter a valid dns  IP address for IP-Address:" + (i + 1).ToString(), "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                    return false;
                                }
                                if (IPRangeValiation(dnsText[i].Text) == false)
                                {
                                    MessageBox.Show("Enter a valid dns  IP address for IP-Address:" + (i + 1).ToString(), "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                    return false;
                                }
                                cacheDNS = dnsText[i].Text;
                            }
                            //System.Net.IPAddress ipAddress = null;
                            //bool isValidIp = System.Net.IPAddress.TryParse(dnsText[i].Text, out ipAddress);
                            //if (isValidIp == false)
                            //{
                            //    MessageBox.Show("Enter a valid dns  IP address for IP-Address:" + (i + 1).ToString(), "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            //    return false;
                            //}
                            //if (IPRangeValiation(dnsText[i].Text) == false)
                            //{
                            //    MessageBox.Show("Enter a valid dns  IP address for IP-Address:" + (i + 1).ToString(), "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            //    return false;
                            //}
                            //_cacheDNS = dnsText[i].Text;
                        }
                    }
                    catch (Exception ex)
                    {
                        System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                        Trace.WriteLine("ERROR*******************************************");
                        Trace.WriteLine(DateTime.Now + "Caught exception at Method: PreReqs of Nic properties form " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                        Trace.WriteLine("Exception  =  " + ex.Message);
                        Trace.WriteLine("ERROR___________________________________________");
                    }
                }
            }
            return true;

        }


        private bool IPRangeValiation(string ip)
        {
            try
            {
                string[] splitIP = ip.Split('.');
                if (splitIP.Length < 4)
                {
                    return false;
                }
                if (splitIP[0] == "0" || splitIP[0] == "00" || splitIP[0] == "000")
                {
                    return false;
                }

            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed to check the ip address validation in IPRangeValiation() " + ex.Message ); 
            }
            return true;
        }

        private void useDhcpRadioButton_CheckedChanged(object sender, EventArgs e)
        {
            if (useDhcpRadioButton.Checked == true)
            {
                ipTabControl.Enabled = false;
                nonDHCPRadioButton.Checked = false;
               
            }
        }

        private void newIPAddressRadioButton_CheckedChanged(object sender, EventArgs e)
        {
            //if (newIPAddressRadioButton.Checked == true)
            //{
            //    ipTextBox.ReadOnly = false;
            //    subnetMaskTextBox.ReadOnly = false;
            //    dnsTextBox.ReadOnly = false;
            //    defaultGateWayTextBox.ReadOnly = false;
            //    useDhcpRadioButton.Checked = false;
            //    _dhcp = false;
            //}
        }

        private void nonDHCPRadioButton_CheckedChanged(object sender, EventArgs e)
        {
            if (nonDHCPRadioButton.Checked == true)
            {
                ipTabControl.Enabled = true;
                useDhcpRadioButton.Checked = false;
            }
        }

        private void ipTabControl_TabIndexChanged(object sender, EventArgs e)
        {
            
            //for (int i = 0; i < ipTabControl.TabCount; i++)
            //{
            //    if (i != tabIndex)
            //    {
            //        if (gateWayText[tabIndex].Text.Length != 0)
            //        {
            //            if (gateWayText[i].Text.Length == 0)
            //            {
            //                //gateWayText[i].Text = gateWayText[tabIndex].Text;
            //            }
            //        }
            //        if (dnsText[tabIndex].Text.Length != 0 && (defaultdns == null || dnsText[tabIndex].Text != defaultdns))
            //        {
            //            dnsText[i].Text = gateWayText[tabIndex].Text;
            //        }
            //        if (subnetMaskText[tabIndex].Text.Length != 0)
            //        {
            //            if (subnetMaskText[i].Text.Length == 0)
            //            {
            //                subnetMaskText[i].Text = subnetMaskText[tabIndex].Text;
            //            }
            //        }
            //    }
            //}
            //tabIndex = ipTabControl.SelectedIndex;
        }

        private void ipTabControl_SelectedIndexChanged(object sender, EventArgs e)
        {
            //tabIndex = ipTabControl.TabIndex;

            try
            {
                for (int i = 0; i < ipTabControl.TabCount; i++)
                {
                    if (i != tabIndex)
                    {
                        if (gateWayText[tabIndex].Text.Length != 0)
                        {
                            if (gateWayText[i].Text.Length == 0)
                            {
                                gateWayText[i].Text = gateWayText[tabIndex].Text;
                            }
                        }
                        if (dnsText[tabIndex].Text.Length != 0)
                        {
                            dnsText[i].Text = dnsText[tabIndex].Text;
                        }
                        if (subnetMaskText[tabIndex].Text.Length != 0)
                        {
                            if (subnetMaskText[i].Text.Length == 0)
                            {
                                subnetMaskText[i].Text = subnetMaskText[tabIndex].Text;
                            }
                        }
                    }
                }
                tabIndex = ipTabControl.SelectedIndex;
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: ipTabControl_SelectedIndexChanged of Nic properties form " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }

        }

        private void refreshPictureBox_Click(object sender, EventArgs e)
        {
            this.Enabled = false;
            progressBar.Visible = true;
            requiredFieldLabel.Visible = false;
            secondNotelabel.Visible = true;
            backgroundWorker.RunWorkerAsync();
        }

        private bool VerfiyCxhasCreds()
        {
            Cxapicalls cxAPi = new Cxapicalls();
            cxAPi.GetHostCredentials(masterHost.esxIp);
            Trace.WriteLine(DateTime.Now + "\t target Esx ip " + masterHost.esxIp);
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
            Trace.WriteLine(DateTime.Now + "\t target Esx ip " + masterHost.esxIp);
            return true;
        }
        private void backgroundWorker_DoWork(object sender, DoWorkEventArgs e)
        {
            Esx esx = new Esx();
            if (VerfiyCxhasCreds() == false)
            {
                Trace.WriteLine(DateTime.Now + "\t CX doesn't have esx creds");
                return;
            }
            Trace.WriteLine(DateTime.Now + "\t target Esx ip before runnign perl script" + masterHost.esxIp);
            returnCode = esx.GetEsxInfoWithVmdksWithOptions(masterHost.esxIp, Role.Secondary, OStype.Windows, "networks");
            if (returnCode == 0)
            {
                masterHost.networkNameList = esx.networkList;
                Trace.WriteLine(DateTime.Now + "\t Printing the network list : count " + esx.networkList.Count.ToString());
            }

        }

        private void backgroundWorker_ProgressChanged(object sender, ProgressChangedEventArgs e)
        {

        }

        private void backgroundWorker_RunWorkerCompleted(object sender, RunWorkerCompletedEventArgs e)
        {
            progressBar.Visible = false;
            this.Enabled = true;
            requiredFieldLabel.Visible = true;
            secondNotelabel.Visible = false;
            if (returnCode != 0)
            {
                MessageBox.Show("Failed to fetch the network information from secondary server", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);

            }
            else
            {
                netWorkComboBox.Items.Clear();
                netWorkComboBox.SelectedText = null;
                foreach (Network n in masterHost.networkNameList)
                {
                    if (masterHost.vSpherehost == n.Vspherehostname)
                    {
                        netWorkComboBox.Items.Add(n.Networkname);
                        if (netWorkComboBox.Items.Count == 1)
                        {
                            netWorkComboBox.SelectedItem = netWorkComboBox.Items[0].ToString();
                        }
                    }
                }                
            }
            
        }
        
    }
}
