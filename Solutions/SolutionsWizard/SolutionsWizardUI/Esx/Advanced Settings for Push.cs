using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;
using System.Diagnostics;

namespace Com.Inmage.Wizard
{
    public partial class AdvancedSettingsforPush : Form
    {
        internal bool canceled = false;
        internal  Host sourceHost = new Host();
        public AdvancedSettingsforPush(Host h)
        {
            InitializeComponent();
            ipToPushComboBox.Items.Add("Select IP");
            selectNATIPComboBox.Items.Add("Select IP");
            advanceSettingsLabel.Text = "Advanced options to push agent on: " + h.hostname.Split('.')[0] + "( " + h.ip.Split(',')[0] + " ) ";
            if (h.IPlist != null)
            {
                foreach (string s in h.IPlist)
                {

                    ipToPushComboBox.Items.Add(s);
                    selectNATIPComboBox.Items.Add(s);
                    
                }
            }
            else
            {
                ipToPushComboBox.Items.Add(h.ip);
                selectNATIPComboBox.Items.Add(h.ip);

                
            }
            selectNATIPComboBox.SelectedItem = selectNATIPComboBox.Items[0];
            ipToPushComboBox.SelectedItem = ipToPushComboBox.Items[0];
            try
            {
                if (h.iptoPush != null)
                {
                    ipToPushComboBox.SelectedItem = h.iptoPush;
                }
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed to select IP to push " + ex.Message);
            }
            try
            {
                if (h.natip != null)
                {
                    bool natIpsExistsinList = false;
                    foreach (string s in selectNATIPComboBox.Items)
                    {
                        if (s == h.natip)
                        {
                            natIpsExistsinList = true;
                            selectNATIPComboBox.SelectedItem = s;
                            break;
                        }
                    }

                    if (natIpsExistsinList == false)
                    {
                        selectNATIPComboBox.Items.Add(h.natip);
                        selectNATIPComboBox.SelectedItem = h.natip;
                    }
                }
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed to select NAT IP " + ex.Message);
            }
            if (h.operatingSystem.Contains("64"))
            {
                cacheDirTextBox.Text = @"C:\Program Files (x86)\InMage Systems\Application Data";
            }
            else
            {
                cacheDirTextBox.Text = @"C:\Program Files\InMage Systems\Application Data";
            }

            if (h.cacheDir != null)
            {
                cacheDirTextBox.Text = h.cacheDir;
            }
            if (h.enableLogging == true)
            {
                enableRadioButton.Checked = true;
            }
            else
            {
                disableRadioButton.Checked = true;
            }

            
            sourceHost = h;
        }

        private void cancelButton_Click(object sender, EventArgs e)
        {
            canceled = true;
            Close();
        }

        private void addButton_Click(object sender, EventArgs e)
        {
            if (ipToPushComboBox.Text.Length > 0 && ipToPushComboBox.SelectedItem.ToString() != "Select IP")
            {
                sourceHost.iptoPush = ipToPushComboBox.Text.ToString();
            }
            if (selectNATIPComboBox.Text.Length > 0 && selectNATIPComboBox.Text != "Select IP")
            {

                sourceHost.natip = selectNATIPComboBox.Text.ToString();
                if (IPRangeValiation(sourceHost.natip) == false)
                {
                    return;
                }

            }
            if (enableRadioButton.Checked == true)
            {
                sourceHost.enableLogging = true;
            }
            else
            {
                sourceHost.enableLogging = false;
            }
            if (cacheDirTextBox.Text.Length > 0)
            {
                sourceHost.cacheDir = cacheDirTextBox.Text.Trim().ToString();
            }
            
            canceled = false;
            Close();
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
                Trace.WriteLine(DateTime.Now + "\t Failed to check the ip address validation in IPRangeValiation() " + ex.Message);
            }
            return true;
        }

        
        
    }
}
