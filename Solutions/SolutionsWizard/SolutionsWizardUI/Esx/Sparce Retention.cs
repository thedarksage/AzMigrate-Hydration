using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Diagnostics;
using System.Windows.Forms;
using System.IO;
using Com.Inmage.Esxcalls;
using Com.Inmage.Tools;

namespace Com.Inmage.Wizard
{
    public partial class sparceRetention : Form
    {
        internal int daysRestorePoint = 0;
        internal int weeksRestorePoint = 0;
        internal int monthsRestorePoint = 0;
        internal int selectedWeeks = 0;
        internal int selectedMonths = 0;
        internal int selectedDays = 0;
        internal int totalSelectedDays = 0;
        internal int selectedHours = 0;
        internal bool canceled = false;
        internal Host hostSelected = new Host();
        internal string _installPath = null;
        public sparceRetention(ref Host sourceHost,Host masterHost,AppName appName)
        {
            InitializeComponent();
            try
            {
                _installPath = WinTools.FxAgentPath() + "\\vContinuum";
                versionLabel.Text = HelpForcx.VersionNumber;
                if (File.Exists(_installPath + "\\patch.log"))
                {
                    patchLabel.Visible = true;
                }
                hostSelected = sourceHost;
                headingLabel.Text = "Advanced setting for " + sourceHost.displayname;
                if (appName != AppName.Drdrill)
                {
                    hourComboBox.Items.Add("1");
                    hourComboBox.Items.Add("2");
                    hourComboBox.Items.Add("3");
                    hourComboBox.Items.Add("4");
                    hourComboBox.Items.Add("6");
                    hourComboBox.Items.Add("8");
                    hourComboBox.Items.Add("12");
                    hourComboBox.SelectedItem = hourComboBox.Items[0].ToString();

                    latestComboBox.Items.Add("hours");
                    latestComboBox.Items.Add("days");
                    latestComboBox.Items.Add("weeks");
                    latestComboBox.Items.Add("months");
                    
                   
                    foreach (ResourcePool resource in masterHost.resourcePoolList)
                    {
                        if (resource.host == masterHost.vSpherehost)
                        {
                            Trace.WriteLine(DateTime.Now + "\t Prining the name and type " + resource.name + "\t type " + resource.ownertype);
                            if (resource.name.ToUpper().ToString() == "RESOURCES" && resource.host == masterHost.vSpherehost && (resource.ownertype.ToUpper().ToString() == "COMPUTERESOURCE" || resource.ownertype.ToUpper().ToString() == "CLUSTERCOMPUTERESOURCE"))
                            {
                                resourcePoolComboBox.Items.Add("Default");
                            }
                            else
                            {
                                resourcePoolComboBox.Items.Add(resource.name);
                            }
                        }
                    }
                    if (hostSelected.resourcePool != null)
                    {
                        if (resourcePoolComboBox.Items.Contains(hostSelected.resourcePool))
                        {
                            resourcePoolComboBox.SelectedItem = hostSelected.resourcePool;
                        }
                    }
                    else
                    {
                        if (resourcePoolComboBox.Items.Count != 0)
                        {
                            resourcePoolComboBox.SelectedItem = masterHost.resourcePool;
                        }
                    }
                    if (hostSelected.retentionInDays != null)
                    {
                        latestNumericUpDown.Value = int.Parse(hostSelected.retentionInDays.ToString());
                        latestComboBox.SelectedItem = latestComboBox.Items[1];
                    }
                    else if (hostSelected.retentionInHours != 0)
                    {
                        latestNumericUpDown.Value = int.Parse(hostSelected.retentionInHours.ToString());
                        latestComboBox.SelectedItem = latestComboBox.Items[0];
                    }
                    else
                    {
                        try
                        {
                            latestNumericUpDown.Value = 0;
                            latestComboBox.SelectedItem = latestComboBox.Items[1];
                        }
                        catch (Exception ex)
                        {
                            Trace.WriteLine(DateTime.Now + "\t Failed to add to numeric upand down " + ex.Message);
                        }
                    }
                    advancedRetentionCheckBox.Checked = true;
                    if (sourceHost.sparceDaysSelected == true)
                    {
                        if (sourceHost.unitsofTimeIntervelWindow != 0)
                        {
                            int i = 0;
                            foreach (string s in hourComboBox.Items)
                            {
                                if (s == sourceHost.unitsofTimeIntervelWindow.ToString())
                                {
                                    break;
                                }
                                i++;
                            }
                            Trace.WriteLine(DateTime.Now + "\t Prinitng number of hours selected by user " + sourceHost.unitsofTimeIntervelWindow.ToString());
                            hourComboBox.SelectedItem = hourComboBox.Items[i].ToString();
                        }
                        if (sourceHost.unitsofTimeIntervelWindow1Value != 0)
                        {
                            daysTextBox.Text = sourceHost.unitsofTimeIntervelWindow1Value.ToString();
                            daysNumericUpDown.Value = sourceHost.bookMarkCountWindow1;
                            daysCheckBox.Checked = true;
                            CalculateTotalDays();
                        }
                    }
                    if (sourceHost.sparceWeeksSelected == true)
                    {
                        if (sourceHost.unitsofTimeIntervelWindow2Value != 0)
                        {
                            weeksCheckBox.Checked = true;
                            weeksTextBox.Text = sourceHost.unitsofTimeIntervelWindow2Value.ToString();
                            weeksNumericUpDown.Value = sourceHost.bookMarkCountWindow2;
                            CalculateTotalDays();
                        }
                    }
                    if (sourceHost.sparceMonthsSelected == true)
                    {
                        if (sourceHost.unitsofTimeIntervelWindow3Value != 0)
                        {
                            monthsCheckBox.Checked = true;
                            monthsTextBox.Text = sourceHost.unitsofTimeIntervelWindow3Value.ToString();
                            monthsNumericUpDown.Value = sourceHost.bookMarkCountWindow3;
                            CalculateTotalDays();
                        }
                    }
                    if (sourceHost.cxbasedCompression == true)
                    {
                        cxBasedRadioButton.Checked = true;
                        compressionRadioButton.Checked = true;
                    }
                    if (sourceHost.sourceBasedCompression == true)
                    {
                        sourceBasedRadioButton.Checked = true;
                        compressionRadioButton.Checked = true;
                    }
                    if (sourceHost.noCompression == true)
                    {
                        noCompressionRadioButton.Checked = true;
                    }
                    if (sourceHost.secure_src_to_ps == true)
                    {
                        secureFromSourceToCxCheckBox.Checked = true;
                    }
                    if (sourceHost.secure_ps_to_tgt == true)
                    {
                        secureFromCXtoDestinationCheckBox.Checked = true;
                    }
                    if (hostSelected.thinProvisioning == true)
                    {
                        thinProvisionRadioButton.Checked = true;
                    }
                    else
                    {
                        thickProvisionRadioButton.Checked = true;
                    }


                }
                else
                {
                    naForSpaceLabel.Visible = true;
                    naForEncryptionLabel.Visible = true;
                    nacompressionLabel.Visible = true;
                    naForProvisioningLabel.Visible = true;
                    naForResourcePoolLabel.Visible = true;
                    sparceRetentionGroupBox.Enabled = false;
                    encryptionGroupBox.Enabled = false;
                    compressionGroupBox.Enabled = false;
                    resourcePoolGroupBox.Enabled = false;
                    provisionGroupBox.Enabled = false;
                    daysCheckBox.Checked = false;
                    weeksCheckBox.Checked = false;
                    monthsCheckBox.Checked = false;
                }
                if (sourceHost.vmDirectoryPath != null)
                {
                    keepVMUnderNeathOfDirectoryRadioButton.Checked = true;
                    directoryNameTextBox.Text = sourceHost.vmDirectoryPath;
                }

                if (sourceHost.cluster == "yes")
                {
                    //keepVMinDataStoreDirectoryRadioButton.Checked = false;
                    //keepVMinDataStoreDirectoryRadioButton.Enabled = false;
                    folderNameGroupBox.Enabled = false;
                    keepVMUnderNeathOfDirectoryRadioButton.Enabled = false;
                }

            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed in constructor of Sparce retention form");
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }

        }

        private void cancelButton_Click(object sender, EventArgs e)
        {
            canceled = true;
            Close();
        }

        private void daysCheckBox_CheckedChanged(object sender, EventArgs e)
        {
            try
            {
                if (daysCheckBox.Checked == true)
                {
                    hourComboBox.Enabled = true;
                    daysTextBox.Enabled = true;
                    daysNumericUpDown.Enabled = true;
                }
                else
                {
                    hourComboBox.Enabled = false;
                    daysTextBox.Enabled = false;
                    daysNumericUpDown.Enabled = false;
                    daysTextBox.Clear();
                    daysCheckBox.Refresh();

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

        private void weeksCheckBox_CheckedChanged(object sender, EventArgs e)
        {
            try
            {
                if (weeksCheckBox.Checked == true)
                {
                    weeksNumericUpDown.Enabled = true;
                    weeksTextBox.Enabled = true;
                }
                else
                {
                    weeksNumericUpDown.Enabled = false;
                    weeksTextBox.Enabled = false;
                    weeksTextBox.Clear();
                    CalculateTotalDays();
                    weeksCheckBox.Refresh();
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

        private void monthsCheckBox_CheckedChanged(object sender, EventArgs e)
        {
            try
            {
                if (monthsCheckBox.Checked == true)
                {
                    monthsNumericUpDown.Enabled = true;
                    monthsTextBox.Enabled = true;
                }
                else
                {
                    monthsTextBox.Enabled = false;
                    monthsNumericUpDown.Enabled = false;
                    monthsTextBox.Clear();
                    monthsCheckBox.Refresh();
                    CalculateTotalDays();
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

        private void advancedRetentionCheckBox_CheckedChanged(object sender, EventArgs e)
        {
            try
            {
                if (advancedRetentionCheckBox.Checked == true)
                {
                    retentionPanel.Enabled = true;
                }
                else
                {
                    retentionPanel.Enabled = false;
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

        private void latestNumericUpDown_ValueChanged(object sender, EventArgs e)
        {
            try
            {
                CalculateTotalDays();
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

        private void latestComboBox_SelectionChangeCommitted(object sender, EventArgs e)
        {
            try
            {
                if (latestComboBox.SelectedItem.ToString() == "hours")
                {
                    latestNumericUpDown.Maximum = 23;
                    latestNumericUpDown.Minimum = 1;
                }
                else  if (latestComboBox.SelectedItem.ToString() == "days")
                {
                    latestNumericUpDown.Maximum = 90;
                    latestNumericUpDown.Minimum = 0;
                }
                else if (latestComboBox.SelectedItem.ToString() == "weeks")
                {
                    latestNumericUpDown.Maximum = 12;
                    latestNumericUpDown.Minimum = 0;
                }
                else if (latestComboBox.SelectedItem.ToString() == "months")
                {
                    latestNumericUpDown.Maximum = 3;
                    latestNumericUpDown.Minimum = 0;
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

        private void daysTextBox_KeyPress(object sender, KeyPressEventArgs e)
        {
            e.Handled = !Char.IsNumber(e.KeyChar) && (e.KeyChar != '\b');
        }

        private void weeksTextBox_KeyPress(object sender, KeyPressEventArgs e)
        {
            e.Handled = !Char.IsNumber(e.KeyChar) && (e.KeyChar != '\b');
        }

        private void monthsTextBox_KeyPress(object sender, KeyPressEventArgs e)
        {
            e.Handled = !Char.IsNumber(e.KeyChar) && (e.KeyChar != '\b');
        }

        private void daysTextBox_MouseLeave(object sender, EventArgs e)
        {
            CalculateTotalDays();

        }

        private void weeksTextBox_MouseLeave(object sender, EventArgs e)
        {
            CalculateTotalDays();
            
        }

        private void monthsTextBox_MouseLeave(object sender, EventArgs e)
        {
            CalculateTotalDays();
        }



        private bool CalculateTotalDays()
        {
            try
            {
                int totalDays = 0;
                if (latestComboBox.SelectedItem != null)
                {
                    if (latestComboBox.SelectedItem.ToString() == "hours")
                    {
                        hostSelected.retentionInHours = int.Parse(latestNumericUpDown.Value.ToString());
                    }

                    if (latestComboBox.SelectedItem.ToString() == "days")
                    {
                        totalDays = int.Parse(latestNumericUpDown.Value.ToString());
                        hostSelected.retentionInDays = totalDays.ToString();
                    }
                    else if (latestComboBox.SelectedItem.ToString() == "weeks")
                    {
                        totalDays = int.Parse(latestNumericUpDown.Value.ToString()) * 7;
                        hostSelected.retentionInDays = totalDays.ToString();
                    }
                    else if (latestComboBox.SelectedItem.ToString() == "months")
                    {
                        totalDays = int.Parse(latestNumericUpDown.Value.ToString()) * 30;
                        hostSelected.retentionInDays = totalDays.ToString();
                    }
                }
                daysLabel.Text = "From " + totalDays + " days onwards.";
                if (daysTextBox.Text.Length != 0)
                {
                    totalDays = totalDays + int.Parse(daysTextBox.Text.ToString());


                }
                weeksLabel.Text = "From " + totalDays + " days onwards.";
                if (weeksTextBox.Text.Length != 0)
                {
                    totalDays = totalDays + (int.Parse(weeksTextBox.Text.ToString()) * 7);


                }
                monthsLabel.Text = "From " + totalDays + " days onwards.";
                if (monthsTextBox.Text.Length != 0)
                {
                    totalDays = totalDays + (int.Parse(monthsTextBox.Text.ToString()) * 30);

                }
                if (latestComboBox.SelectedItem.ToString() == "hours")
                {
                    totalDaysLabel.Text = "Total days selected " + totalDays + " " + latestNumericUpDown.Value.ToString() + " hours ";
                }
                totalDaysLabel.Text = "Total days selected " + totalDays;


                totalSelectedDays = totalDays;
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

        private void OkButton_Click(object sender, EventArgs e)
        {
            try
            {
                if (latestComboBox.SelectedItem != null)
                {
                    if (latestComboBox.SelectedItem.ToString() == "hours")
                    {
                        hostSelected.retentionInHours = int.Parse(latestNumericUpDown.Value.ToString());
                    }
                }
                if (daysCheckBox.Checked == true)
                {
                    if (daysTextBox.Text.Length == 0)
                    {
                        MessageBox.Show("Select some number of days to issue bookmark", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return;
                    }

                }
                if (weeksCheckBox.Checked == true)
                {
                    if (weeksTextBox.Text.Length == 0)
                    {
                        MessageBox.Show("Select some number of weeks to issue bookmark", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return;
                    }
                }
                if (monthsCheckBox.Checked == true)
                {
                    if (monthsTextBox.Text.Length == 0)
                    {
                        MessageBox.Show("Select some number of months to issue bookmark", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return;
                    }
                }
                if (daysNumericUpDown.Value != 0 && daysCheckBox.Checked == true)
                {
                    daysRestorePoint = int.Parse(daysNumericUpDown.Value.ToString());
                    hostSelected.bookMarkCountWindow1 = int.Parse(daysNumericUpDown.Value.ToString());
                    hostSelected.unitsofTimeIntervelWindow = int.Parse(hourComboBox.Text.ToString());
                }
                if (weeksNumericUpDown.Value != 0 && weeksCheckBox.Checked == true)
                {
                    weeksRestorePoint = int.Parse(weeksNumericUpDown.Value.ToString());
                    hostSelected.bookMarkCountWindow2 = int.Parse(weeksNumericUpDown.Value.ToString());
                }
                if (monthsNumericUpDown.Value != 0 && monthsCheckBox.Checked == true)
                {
                    monthsRestorePoint = int.Parse(monthsNumericUpDown.Value.ToString());
                    hostSelected.bookMarkCountWindow3 = int.Parse(monthsNumericUpDown.Value.ToString());
                }
                if (daysTextBox.Text.Length != 0)
                {
                    selectedDays = int.Parse(daysTextBox.Text.ToString());
                    hostSelected.unitsofTimeIntervelWindow1Value = int.Parse(daysTextBox.Text.ToString());
                }
                if (weeksTextBox.Text.Length != 0)
                {
                    selectedWeeks = int.Parse(weeksTextBox.Text.ToString());
                    hostSelected.unitsofTimeIntervelWindow2Value = int.Parse(weeksTextBox.Text.ToString());
                }
                if (monthsTextBox.Text.Length != 0)
                {
                    selectedMonths = int.Parse(monthsTextBox.Text.ToString());
                    hostSelected.unitsofTimeIntervelWindow3Value = int.Parse(monthsTextBox.Text.ToString());
                }
                CalculateTotalDays();
                canceled = false;
                if (keepVMUnderNeathOfDirectoryRadioButton.Checked == true)
                {
                    if (directoryNameTextBox.Text.Length != 0)
                    {
                        if (directoryNameTextBox.Text.Contains("&") || directoryNameTextBox.Text.Contains("\"") || directoryNameTextBox.Text.Contains("\'") || directoryNameTextBox.Text.Contains(">") || directoryNameTextBox.Text.Contains(":") || directoryNameTextBox.Text.Contains("\\") || directoryNameTextBox.Text.Contains("//") || directoryNameTextBox.Text.Contains("|") || directoryNameTextBox.Text.Contains("*") || directoryNameTextBox.Text.Contains("/"))
                        {
                            MessageBox.Show("Folder name does not accept character * : ? < > / \\ | & \' \"", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            directoryNameTextBox.Text = null;
                            return;
                        }
                        if (hostSelected.displayname.ToUpper().Contains(directoryNameTextBox.Text.ToUpper().TrimEnd()))
                        {
                            MessageBox.Show("Selected sub directory for VM creation is not supported. Sub directory name should not be part of displayname of machine." + Environment.NewLine + 
                            "Displayname = " + hostSelected.displayname + Environment.NewLine +
                            "Sub fodler = " + directoryNameTextBox.Text + Environment.NewLine + 
                            "Choose different sub directory" , "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            return;
                        }
                        hostSelected.vmDirectoryPath = directoryNameTextBox.Text.TrimEnd();
                    }
                    else
                    {
                        hostSelected.vmDirectoryPath = null;
                    }
                }
                else
                {
                    hostSelected.vmDirectoryPath = null;
                }
                if (daysCheckBox.Checked == true || weeksCheckBox.Checked == true || monthsCheckBox.Checked == true)
                {
                    Trace.WriteLine(DateTime.Now + "\t User has selected sparce retention for this protection");
                    hostSelected.selectedSparce = true;
                }
                else
                {
                    hostSelected.selectedSparce = false;
                    Trace.WriteLine(DateTime.Now + "\t User has not selected sparce retention ");
                }
                if (daysCheckBox.Checked == true)
                {
                    hostSelected.sparceDaysSelected = true;
                }
                else
                {
                    hostSelected.sparceDaysSelected = false;

                }
                if (weeksCheckBox.Checked == true)
                {
                    hostSelected.sparceWeeksSelected = true;
                }
                else
                {
                    hostSelected.sparceWeeksSelected = false;
                }
                if (monthsCheckBox.Checked == true)
                {
                    hostSelected.sparceMonthsSelected = true;
                }
                else
                {
                    hostSelected.sparceMonthsSelected = false;
                }
                if ((daysCheckBox.Checked == true) && (daysTextBox.Text == "0"))
                {
                    MessageBox.Show("Please enter days value more than zero to proceed", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return;
                }
                if ((weeksCheckBox.Checked == true) && (weeksTextBox.Text == "0"))
                {
                    MessageBox.Show("Please enter weeks value more than zero to proceed", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return;
                }
                if ((monthsCheckBox.Checked == true) && (monthsTextBox.Text == "0"))
                {
                    MessageBox.Show("Please enter months value more than zero to proceed", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return;
                }
                Close();
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

        private void keepVMinDataStoreDirectoryRadioButton_CheckedChanged(object sender, EventArgs e)
        {
            try
            {
                if (keepVMinDataStoreDirectoryRadioButton.Checked == true)
                {
                    noteSpecialCharLabel.Visible = false;
                    enterDirectoryLabel.Visible = false;
                    directoryNameTextBox.Visible = false;
                    hostSelected.vmDirectoryPath = null;
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

        private void keepVMUnderNeathOfDirectoryRadioButton_CheckedChanged(object sender, EventArgs e)
        {
            try
            {
                if (keepVMUnderNeathOfDirectoryRadioButton.Checked == true)
                {
                    noteSpecialCharLabel.Visible = true;
                    enterDirectoryLabel.Visible = true;
                    directoryNameTextBox.Visible = true;
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

        private void noCompressionRadioButton_CheckedChanged(object sender, EventArgs e)
        {
            if (noCompressionRadioButton.Checked == true)
            {
                cxBasedRadioButton.Visible = false;
                sourceBasedRadioButton.Visible = false;
                cxBasedRadioButton.Checked = false;
                sourceBasedRadioButton.Checked = false;
            }
        }

        private void compressionRadioButton_CheckedChanged(object sender, EventArgs e)
        {
            if (compressionRadioButton.Checked == true)
            {
                cxBasedRadioButton.Visible = true;
                sourceBasedRadioButton.Visible = true;
                cxBasedRadioButton.Checked = true;
            }
        }

        private void folderNameGroupBox_Enter(object sender, EventArgs e)
        {

        }

        
       

       
       


       
    }
}
