using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Windows.Forms;
using System.Threading;
using System.Text;
using System.Collections;
using Com.Inmage.Tools;
using System.Xml;
using System.IO;
using Com.Inmage.Esxcalls;
using System.Drawing;
using System.Text.RegularExpressions;

namespace Com.Inmage.Wizard
{
    class RecoverConfigurationPanel : PanelHandler
    {
        internal static int NIC_NAME_COLUMN = 0;
        internal static int KEPP_OLD_VALUES_COLUMN = 1;
        internal static int CHANGE_VALUE_COLUMN = 2;
        internal static int SELECT_UNSELECT_NIC_COLUMN = 3;
        internal string _displayName = null;
        internal Host _selectedHost = new Host();
        internal static int MAX_SIZE_IN_MB = 0;
        internal static int MAX_SIZE_IN_GB = 0;
        internal string _cacheSubnetmask = null;
        internal string _cacheGateway = null;
        internal string _cacheDns = null;
        public RecoverConfigurationPanel()
        {
          
            
        }
        /*
         * First we will add nodes to the treeview. By reading Info.xml
         * we will fill the cpu combo box and memory numberic values text box.
         * Set the max and values of numericmemorybox.
         * Fill the datagridview with vlues of first server.
         */
        internal override bool Initialize(AllServersForm allServersForm)
        {

            try
            {
                allServersForm.natConfigGroupBox.Visible = false;
                // Filling the memoryValuesCombobox with gb and mb.
                if (allServersForm.appNameSelected == AppName.Recover)
                {
                    allServersForm.displayNameSettingsGroupBox.Visible = false;
                    
                }
                allServersForm.helpContentLabel.Text = HelpForcx.ProtectScreen6;
                if (AllServersForm.v2pRecoverySelected == false)
                {
                    allServersForm.memoryValuesComboBox.Items.Clear();
                    allServersForm.memoryValuesComboBox.Items.Add("GB");
                    allServersForm.memoryValuesComboBox.Items.Add("MB");

                    allServersForm.memoryNumericUpDown.Enabled = true;
                    allServersForm.cpuComboBox.Enabled = true;
                    allServersForm.memoryValuesComboBox.Enabled = true;
                    allServersForm.hardWareSettingsCheckBox.Enabled = true;
                }
                else
                {
                    allServersForm.hardWareGroupBox.Enabled = false;

                }
                allServersForm.previousButton.Visible = true;
                allServersForm.previousButton.Enabled = true;
                string esxIP = null;
                string masterHostVsphereHostname = null;
                // allServersForm.configurationScreen.sourceConfigurationTreeView.Nodes.Clear();
                foreach (Host h in allServersForm.recoveryChecksPassedList._hostList)
                {
                    esxIP = h.targetESXIp;
                    if(h.targetvSphereHostName != null)
                    {
                        masterHostVsphereHostname = h.targetvSphereHostName;
                    }
                    
                }
                if (AllServersForm.v2pRecoverySelected == false)
                {
                    if (allServersForm.masterHostAdded.networkNameList.Count == 0 || allServersForm.appNameSelected == AppName.Drdrill)
                    {
                        Host masterHostofTarget = (Host)allServersForm.esxSelected.GetHostList[0];
                        allServersForm.masterHostAdded.networkNameList = allServersForm.esxSelected.networkList;
                        allServersForm.masterHostAdded.configurationList = allServersForm.esxSelected.configurationList;
                        allServersForm.masterHostAdded.dataStoreList = allServersForm.esxSelected.dataStoreList;
                    }
                    else
                    {
                        //Host masterHostofTarget = (Host)allServersForm._esx.GetHostList()[0];
                        //foreach (Configuration config in allServersForm._masterHost.configurationList)
                        //{
                        //    if (config.vSpherehostname == null)
                        //    {
                        //        allServersForm._masterHost.configurationList = masterHostofTarget.configurationList;
                        //        allServersForm._masterHost.networkNameList = masterHostofTarget.networkNameList;
                        //        allServersForm._masterHost.dataStoreList = masterHostofTarget.dataStoreList;
                        //    }
                        //}
                    }
                    Trace.WriteLine(DateTime.Now + "\t Printing the vsphere hostname " + allServersForm.masterHostAdded.vSpherehost);
                    allServersForm.helpContentLabel.Text = HelpForcx.ProtectScreen6;
                    //Filling the values in the cpu count combobox
                    if (allServersForm.masterHostAdded.configurationList != null)
                    {
                        foreach (Configurations config in allServersForm.masterHostAdded.configurationList)
                        {
                            if (masterHostVsphereHostname != null)
                            {
                                Trace.WriteLine(DateTime.Now + "\t Came here because vSherehost is not null" + allServersForm.masterHostAdded.vSpherehost);
                                if (config.vSpherehostname == masterHostVsphereHostname)
                                {
                                    if (config.cpucount != 0)
                                    {
                                        allServersForm.cpuComboBox.Items.Clear();
                                        for (int i = 0; i < config.cpucount; i++)
                                        {
                                            allServersForm.cpuComboBox.Items.Add(i + 1);
                                        }
                                    }
                                    if (config.memsize != 0)
                                    {
                                        int memsize = (int)Math.Round(config.memsize);
                                        allServersForm.memoryValuesComboBox.SelectedItem = "MB";
                                        allServersForm.memoryNumericUpDown.Maximum = memsize;
                                        MAX_SIZE_IN_GB = memsize / 1024;
                                        MAX_SIZE_IN_MB = memsize;
                                    }
                                }
                            }
                            if (allServersForm.masterHostAdded.vSpherehost != null)
                            {
                                Trace.WriteLine(DateTime.Now + "\t Came here because vSherehost is not null" + allServersForm.masterHostAdded.vSpherehost);
                                if (config.vSpherehostname == allServersForm.masterHostAdded.vSpherehost)
                                {
                                    if (config.cpucount != 0)
                                    {
                                        allServersForm.cpuComboBox.Items.Clear();
                                        for (int i = 0; i < config.cpucount; i++)
                                        {
                                            allServersForm.cpuComboBox.Items.Add(i + 1);
                                        }
                                    }
                                    if (config.memsize != 0)
                                    {
                                        int memsize = (int)Math.Round(config.memsize);
                                        allServersForm.memoryValuesComboBox.SelectedItem = "MB";
                                        allServersForm.memoryNumericUpDown.Maximum = memsize;
                                        MAX_SIZE_IN_GB = memsize / 1024;
                                        MAX_SIZE_IN_MB = memsize;
                                    }
                                }
                            }
                            else
                            {
                                Trace.WriteLine(DateTime.Now + "\t Came here because user has protected with the 5.5 or 6.2 release builds");
                                if (config.cpucount != 0)
                                {
                                    allServersForm.cpuComboBox.Items.Clear();
                                    for (int i = 0; i < config.cpucount; i++)
                                    {
                                        allServersForm.cpuComboBox.Items.Add(i + 1);
                                    }
                                }
                                if (config.memsize != 0)
                                {
                                    int memsize = (int)Math.Round(config.memsize);
                                    allServersForm.memoryValuesComboBox.SelectedItem = "MB";
                                    allServersForm.memoryNumericUpDown.Maximum = memsize;
                                    MAX_SIZE_IN_GB = memsize / 1024;
                                    MAX_SIZE_IN_MB = memsize;
                                }
                            }


                        }
                    }
                    allServersForm.sourceConfigurationTreeView.Nodes.Clear();
                    allServersForm.sourceConfigurationTreeView.Nodes.Add("Target: " + esxIP);
                    // filling the node to the configurationtreevoew....
                }
                else
                {
                    allServersForm.sourceConfigurationTreeView.Nodes.Clear();
                    allServersForm.sourceConfigurationTreeView.Nodes.Add("Physical Machine");
                }
                foreach (Host h in allServersForm.recoveryChecksPassedList._hostList)
                {
                    allServersForm.sourceConfigurationTreeView.Nodes[0].Nodes.Add(h.displayname).Text = h.displayname;
                }
                allServersForm.sourceConfigurationTreeView.ExpandAll();
                TreeNodeCollection nodes = allServersForm.sourceConfigurationTreeView.Nodes[0].Nodes;

                allServersForm.sourceConfigurationTreeView.SelectedNode = nodes[0];
                Host tempHost = (Host)allServersForm.recoveryChecksPassedList._hostList[0];
                int rowIndex = 0;
                allServersForm.networkDataGridView.Rows.Clear();
                Trace.WriteLine(DateTime.Now + " \t Printing the nic list count " + tempHost.nicList.list.Count.ToString());

                foreach (Hashtable nicHash in tempHost.nicList.list)
                {
                    //Trace.WriteLine(DateTime.Now + " \t printing the values of the nic " + nic.name + " ip " + nic.ip + " mask " + nic.subnetMask);
                    allServersForm.networkDataGridView.Rows.Add(1);                   
                    allServersForm.networkDataGridView.Rows[rowIndex].Cells[NIC_NAME_COLUMN].Value = nicHash["nic_mac"].ToString();
                    if (nicHash["new_ip"] == null)
                    {
                        if (nicHash["ip"] != null)
                        {
                            allServersForm.networkDataGridView.Rows[rowIndex].Cells[KEPP_OLD_VALUES_COLUMN].Value = nicHash["ip"].ToString();
                        }
                        else
                        {
                            allServersForm.networkDataGridView.Rows[rowIndex].Cells[KEPP_OLD_VALUES_COLUMN].Value = "IP not configured";
                        }
                    }
                    else
                    {
                        allServersForm.networkDataGridView.Rows[rowIndex].Cells[KEPP_OLD_VALUES_COLUMN].Value = nicHash["new_ip"].ToString();
                    }
                    if (nicHash["Selected"] != null)
                    {
                        if (nicHash["Selected"].ToString() == "yes")
                        {
                            allServersForm.networkDataGridView.Rows[rowIndex].Cells[SELECT_UNSELECT_NIC_COLUMN].Value = true;
                        }
                        else
                        {
                            allServersForm.networkDataGridView.Rows[rowIndex].Cells[SELECT_UNSELECT_NIC_COLUMN].Value = false;
                        }
                        allServersForm.networkDataGridView.Rows[rowIndex].Cells[SELECT_UNSELECT_NIC_COLUMN].ReadOnly = true;
                    }
                   // allServersForm.networkDataGridView.Rows[rowIndex].Cells[CHANGE_VALUE_COLUMN].Value = "Change";
                    rowIndex++;
                }
                for (int i = 0; i < allServersForm.networkDataGridView.RowCount; i++)
                {
                    allServersForm.networkDataGridView.Rows[i].Cells[CHANGE_VALUE_COLUMN].Value = "Change";
                }

                //Due to issue at the time of the resume recovery we are getting the issue so we are commenting the code.
                //Due to sungard issue with port group we are removing this code JIRA bug 152or 151 
                /*foreach (Host h in allServersForm._recoverChecksPassedList._hostList)
                {
                    foreach (Hashtable hash in h.nicList.list)
                    {
                        if (hash["new_network_name"] != null)
                        {
                            hash["network_name"] = hash["new_network_name"].ToString();
                        }
                    }
                }*/
                if (AllServersForm.v2pRecoverySelected == false)
                {
                    allServersForm.memoryValuesComboBox.SelectedItem = "MB";
                    allServersForm.memoryNumericUpDown.Maximum = MAX_SIZE_IN_MB;
                    if (tempHost.memSize < 512)
                    {
                        allServersForm.memoryNumericUpDown.Value = 512;
                    }
                    else if (tempHost.memSize > MAX_SIZE_IN_MB)
                    {
                        allServersForm.memoryNumericUpDown.Value = MAX_SIZE_IN_MB;
                    }
                    else
                    {
                        allServersForm.memoryNumericUpDown.Value = tempHost.memSize;
                    }
                    if (allServersForm.cpuComboBox.Items.Contains(tempHost.cpuCount))
                    {
                        allServersForm.cpuComboBox.SelectedItem = tempHost.cpuCount;
                    }
                    if (tempHost.cpuCount == 0)
                    {
                        allServersForm.netWorkConfiurationGroupBox.Visible = false;
                        allServersForm.networkDataGridView.Visible = false;
                        allServersForm.hardWareGroupBox.Visible = false;
                        //allServersForm.oldProtectionLabel.Text = "This machine is protected with old vContinuum. This feature doesn't exist for old protection.";
                        // allServersForm.oldProtectionLabel.Visible = true;
                    }
                    else
                    {
                        allServersForm.netWorkConfiurationGroupBox.Visible = true;
                        allServersForm.networkDataGridView.Visible = true;
                        allServersForm.hardWareGroupBox.Visible = true;
                        //allServersForm.oldProtectionLabel.Text = "This machine is protected with old vContinuum. This feature doesn't exist for old protection.";
                        //allServersForm.oldProtectionLabel.Visible = false;
                    }
                }
                if (allServersForm.appNameSelected == AppName.Drdrill)
                {
                    // In case of protection we are giving the option for user to enter the vm folder path;
                    allServersForm.advancedSettingsLinkLabel.Visible = true;
                    if (allServersForm.arrayBasedDrDrillRadioButton.Checked == true)
                    {
                        allServersForm.advancedSettingsLinkLabel.Visible = false;
                        allServersForm.keepSameAsSouceRadioButton.Enabled = false;
                    }
                }
                else
                {
                    allServersForm.advancedSettingsLinkLabel.Visible = false;
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: Initialize of RecoverConfiguration.cs " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }            
            return true;
        }

        /*
         * Here first we will check for the new_network_name on target side.
         * if user does't select any network name we will select first one as default.
         * we will convert all the memory values into mb and we will check that memory values should be divisible by 4.
         * In case of dr drill we had provided an opption to change the displayname.
         * If not we will add _DR_Drill for each mahine. With that displayname we will change the folder name and path also.
         */
        internal override bool ProcessPanel(AllServersForm allServersForm)
        {
            try
            {
                foreach (Host h in allServersForm.recoveryChecksPassedList._hostList)
                {
                    // checking whether the selected n.w for each nic existed in target side or not...
                    foreach (Hashtable nicHash in h.nicList.list)
                    {
                        if (nicHash["new_network_name"] == null)
                        {
                            foreach (Network network in allServersForm.masterHostAdded.networkNameList)
                            {
                                if (allServersForm.masterHostAdded.vSpherehost == network.Vspherehostname)
                                {
                                    if (nicHash["network_name"] != null)
                                    {
                                        if (nicHash["network_name"].ToString() == network.Networkname)
                                        {
                                            nicHash["new_network_name"] = network.Networkname;
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                        try
                        {
                            //Filling old values as new values in case user doesn't enter any values by using nic configuration screen
                            if (nicHash["new_ip"] == null)
                            {
                                if (nicHash["new_ip"] == null && nicHash["ip"] != null)
                                {
                                    nicHash["new_ip"] = nicHash["ip"].ToString();
                                }

                                if (nicHash["new_mask"] == null && nicHash["mask"] != null)
                                {
                                    nicHash["new_mask"] = nicHash["mask"].ToString();
                                }
                                if (nicHash["new_gateway"] == null & nicHash["gateway"] != null)
                                {
                                    nicHash["new_gateway"] = nicHash["gateway"].ToString();
                                }
                                if (nicHash["new_dnsip"] == null && nicHash["dnsip"] != null)
                                {
                                    nicHash["new_dnsip"] = nicHash["dnsip"].ToString();
                                }
                            }
                        }
                        catch (Exception ex)
                        {
                            Trace.WriteLine(DateTime.Now + "\t Failed to update old values with new values " + ex.Message);
                        }

                    }
                    foreach (Hashtable nicHash in h.nicList.list)
                    {
                        if (nicHash["new_network_name"] == null)
                        {
                            foreach (Network network in allServersForm.masterHostAdded.networkNameList)
                            {
                                if (allServersForm.masterHostAdded.vSpherehost == network.Vspherehostname)
                                {
                                    nicHash["new_network_name"] = network.Networkname;
                                    break;
                                }
                            }
                        }
                    }
                }

                foreach (Host h in allServersForm.recoveryChecksPassedList._hostList)
                {
                    foreach (Hashtable nicHash in h.nicList.list)
                    {
                        if (nicHash["new_network_name"] == null)
                        {
                            if (nicHash["network_name"] != null)
                            {
                                nicHash["new_network_name"] = nicHash["network_name"].ToString();
                            }
                        }
                        nicHash["VMNameExists"] = "no";
                    }

                    foreach (Hashtable nicHash in h.nicList.list)
                    {
                        if (nicHash["new_dnsip"] != null && nicHash["dnsip"] != null)
                        {
                            string[] newgateways = nicHash["new_dnsip"].ToString().Split(',');
                            string[] gateways = nicHash["dnsip"].ToString().Split(',');

                            if (newgateways.Length != gateways.Length)
                            {
                                if (gateways.Length > newgateways.Length)
                                {
                                    int newgatewayslength = newgateways.Length;
                                    int oldgatewaylenght = gateways.Length;
                                    while(newgatewayslength  < oldgatewaylenght)
                                    {
                                        nicHash["new_dnsip"] = nicHash["new_dnsip"].ToString() + "," + "notselected";
                                        newgatewayslength++;
                                    }
                                    
                                }
                            }
                        }

                        if (nicHash["new_gateway"] != null && nicHash["gateway"] != null)
                        {
                            string[] newgateways = nicHash["new_gateway"].ToString().Split(',');
                            string[] gateways = nicHash["gateway"].ToString().Split(',');

                            if (newgateways.Length != gateways.Length)
                            {
                                if (gateways.Length > newgateways.Length)
                                {
                                    int newgatewayslength = newgateways.Length;
                                    int oldgatewaylenght = gateways.Length;
                                    while (newgatewayslength < oldgatewaylenght)
                                    {
                                        nicHash["new_gateway"] = nicHash["new_gateway"].ToString() + "," + "notselected";
                                        newgatewayslength++;
                                    }
                                }
                            }
                        }
                    }
                }

                // Checking the ram selected by the user for each vm should be divisible by 4.
                foreach (Host h in allServersForm.recoveryChecksPassedList._hostList)
                {
                    Trace.WriteLine(DateTime.Now + "\t Printing mamsize " + h.memSize.ToString());

                    Trace.WriteLine(DateTime.Now + "\t Printing mamsize  after " + h.memSize.ToString());
                    if (h.memSize == 0)
                    {
                        h.memSize = 4;
                    }
                    if (h.memSize % 4 != 0)
                    {
                        int remainder = h.memSize % 4;
                        h.memSize = h.memSize - remainder;
                        Trace.WriteLine(DateTime.Now + "\t Printing mamsize  afeter remove " + remainder.ToString());
                    }
                    try
                    {
                        foreach (Configurations configs in allServersForm.masterHostAdded.configurationList)
                        {
                            if (allServersForm.masterHostAdded.vSpherehost == configs.vSpherehostname)
                            {
                                if (h.cpuCount > configs.cpucount)
                                {
                                    h.cpuCount = configs.cpucount;
                                }
                                if (allServersForm.masterHostAdded.hostversion != null)
                                {
                                    if (allServersForm.masterHostAdded.hostversion.Contains("3.5") || (h.vmxversion != null && h.vmxversion.Contains("4")))
                                    {
                                        if (h.cpuCount == 3 )
                                        {
                                            Trace.WriteLine(DateTime.Now + "\t Making cpu count as 2 because target esx version is 3.5 and user has selected 3 as cpu count");
                                            h.cpuCount = 2;
                                        }
                                        else if (h.cpuCount > 4 && h.cpuCount < 8)
                                        {
                                            Trace.WriteLine(DateTime.Now + "\t Making cpu count as 4. Because vmx version is 4 and user has selected cpu count in b/w 4 and 8");
                                            h.cpuCount = 4;
                                        }
                                        else if (h.cpuCount > 8 && h.cpuCount < 16)
                                        {
                                            Trace.WriteLine(DateTime.Now + "\t Making cpu count as 8. Because vmx version is 4 and user has selected cpu count in b/w 8 and 16");
                                            h.cpuCount = 8;

                                        }
                                        else if (h.cpuCount > 16 && h.cpuCount < 32)
                                        {
                                            Trace.WriteLine(DateTime.Now + "\t Making cpu count as 16. Because vmx version is 4 and user has selected cpu count in b/w 16 and 32");
                                            h.cpuCount = 16;
                                        }
                                        else if (h.cpuCount > 32 && h.cpuCount < 64)
                                        {
                                            Trace.WriteLine(DateTime.Now + "\t Making cpu count as 32. Because vmx version is 4 and user has selected cpu count in b/w 32 and 64");
                                            h.cpuCount = 32;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    catch (Exception ex)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Faield to update cpus at the time of recovery/Dr drill " + ex.Message);
                    }
                }
                
                foreach (Host h in allServersForm.sourceListByUser._hostList)
                {
                    foreach (Host h1 in allServersForm.recoveryChecksPassedList._hostList)
                    {
                        if (h1.hostname == h.hostname)
                        {
                            h.memSize = h1.memSize;
                            h.cpuCount = h1.cpuCount;
                            h.nicList = h1.nicList;
                            h.keepNicOldValues = h1.keepNicOldValues;
                        }
                    }
                }
                foreach (Host h in allServersForm.vmsSelectedRecovery._hostList)
                {
                    foreach (Host h1 in allServersForm.recoveryChecksPassedList._hostList)
                    {
                        if (h1.hostname == h.hostname)
                        {
                            h.memSize = h1.memSize;
                            h.cpuCount = h1.cpuCount;
                            h.nicList = h1.nicList;
                            h.keepNicOldValues = h1.keepNicOldValues;
                            h.thinProvisioning = false;
                        }
                    }
                }


                foreach (Host h in allServersForm.recoveryChecksPassedList._hostList)
                {
                    if (h.operatingSystem.Contains("2012"))
                    {
                        if (h.memSize < 2000)
                        {
                            MessageBox.Show("You have selected Memory size less than 2GB for Machine: " + h.displayname + " and that machine has 2012 Operating system. Please select more than 2 GB.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            return false;
                        }
                    }
                    try
                    {
                        if (h.new_displayname.Length > 80)
                        {
                            MessageBox.Show("For the machine " + h.displayname + " you have selected DR server name with more than 80 characters which is not supported.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            return false;
                        }
                    }
                    catch (Exception ex)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Failed to verify displayname lenght " + ex.Message);
                    }

                }

                foreach (Host h in allServersForm.recoveryChecksPassedList._hostList)
                {
                    Trace.WriteLine(DateTime.Now + "\t Printing the Memsize " + h.memSize.ToString() + " \t displayname " + h.displayname);
                }
                foreach (Host h in allServersForm.recoveryChecksPassedList._hostList)
                {
                    foreach (Hashtable nicHash in h.nicList.list)
                    {
                        foreach (Network network in allServersForm.masterHostAdded.networkNameList)
                        {
                            if (nicHash["new_network_name"] != null)
                            {
                                if (nicHash["new_network_name"].ToString() == network.Networkname)
                                {
                                    nicHash["VMNameExists"] = "yes";
                                }
                            }
                            else
                            {
                                nicHash["VMNameExists"] = "yes";
                            }
                        }
                    }

                    bool selectedAtleastOneNic = false;
                    foreach (Hashtable nicHash in h.nicList.list)
                    {
                        if (nicHash["Selected"].ToString() == "yes")
                        {
                            selectedAtleastOneNic = true;
                        }
                    }
                    if (selectedAtleastOneNic == false)
                    {
                        MessageBox.Show("You don't have selected even a single nic for the machine " + h.displayname, "Information", MessageBoxButtons.OK, MessageBoxIcon.Information);
                        MessageBox.Show("Select atleast one nic for the VM: " + h.displayname + " to proceed", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return false;
                    }
                }
                //foreach (Host h in allServersForm._recoverChecksPassedList._hostList)
                //{
                //    foreach (Hashtable nicHash in h.nicList.list)
                //    {
                //        if (nicHash["VMNameExists"].ToString() != "yes")
                //        {
                //            MessageBox.Show("Network name selected for the VM: " + h.displayname + " is not present on target." + Environment.NewLine +
                //                " Select another network name for this vm", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                //            return false;
                //        }
                //    }
                //}

                if (allServersForm.appNameSelected == AppName.Drdrill)
                {
                    //If appname is drdrill then recovercheckspassedlist will represent the selected and runreadiness check pass vms list.
                    foreach (Host h in allServersForm.recoveryChecksPassedList._hostList)
                    {
                        foreach (Host h1 in allServersForm.esxSelected.GetHostList)
                        {
                            if (h.new_displayname != null)
                            {

                                if (h.new_displayname == h1.displayname && h.target_uuid == h1.source_uuid)
                                {
                                    if (allServersForm.hostBasedRadioButton.Checked == true)
                                    {
                                        Trace.WriteLine(DateTime.Now + "\t Both the displayname in xml file got matched Master configfile name " + h.new_displayname + " Info displayname " + h1.displayname);
                                        h.new_displayname = h1.displayname + "_DR_Drill";
                                        break;
                                    }
                                    else
                                    {
                                        MessageBox.Show("Same displayname is not supported for Array based snapshot", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                        return false;
                                    }
                                }
                            }         
                            
                        }

                    }
                   
                    // Before going to make the changes of path we need to remove the vm directory path which is existing already.

                   

                    //Changing the disk, folder, vmx paths for the selected vms for dr drill.
                    // We are going to change the paths with new display names.
                    foreach (Host h in allServersForm.recoveryChecksPassedList._hostList)
                    {
                        h.thinProvisioning = false;
                        if (allServersForm.arrayBasedDrDrillRadioButton.Checked == false)
                        {
                            Host h1 = new Host();
                            foreach (Host hostinInfoFile in allServersForm.esxSelected.GetHostList)
                            {
                                if (allServersForm.masterHostAdded.vSpherehost != null)
                                {

                                    if (h.target_uuid == hostinInfoFile.source_uuid)
                                    {
                                        Trace.WriteLine(DateTime.Now + "\t Matched the uuids " + h.displayname + "\t " + hostinInfoFile.displayname);
                                        Trace.WriteLine(DateTime.Now + "\t Printing the new displayname " + h.new_displayname + " \t " + hostinInfoFile.new_displayname);
                                        h1 = new Host();
                                        h1 = h;
                                        ReNameDrDrillPath(allServersForm, ref h1, hostinInfoFile);
                                        h.vmx_path = h1.vmx_path;
                                        h.folder_path = h1.folder_path;
                                        h.disks = h1.disks;
                                        break;
                                    }
                                    else
                                    {
                                        if (VerifyingWithVmdks(allServersForm, h, hostinInfoFile) == true)
                                        {
                                            h1 = new Host();
                                            h1 = h;
                                            ReNameDrDrillPath(allServersForm, ref h1, hostinInfoFile);
                                            h.vmx_path = h1.vmx_path;
                                            h.folder_path = h1.folder_path;
                                            h.disks = h1.disks;
                                            break;
                                        }
                                    }


                                }
                                else if (h.targetESXIp != null)
                                {
                                    if (h.targetESXIp == hostinInfoFile.esxIp)
                                    {
                                        if (h.target_uuid == hostinInfoFile.source_uuid)
                                        {
                                            h1 = new Host();
                                            h1 = h;
                                            ReNameDrDrillPath(allServersForm, ref h1, hostinInfoFile);
                                            h.vmx_path = h1.vmx_path;
                                            h.folder_path = h1.folder_path;
                                            h.disks = h1.disks;
                                            break;
                                        }
                                        else
                                        {
                                            if (VerifyingWithVmdks(allServersForm, h, hostinInfoFile) == true)
                                            {
                                                h1 = new Host();
                                                h1 = h;
                                                ReNameDrDrillPath(allServersForm, ref h1, hostinInfoFile);
                                                h.vmx_path = h1.vmx_path;
                                                h.folder_path = h1.folder_path;
                                                h.disks = h1.disks;
                                                break;
                                            }
                                        }
                                    }

                                }
                            }
                        }
                        foreach (Hashtable diskHash in h.disks.list)
                        {
                            diskHash["Protected"] = "no";
                        }

                    }
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: Process of RecoverConfiguration.cs " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            return true;
        }

        /*
         * In case of DR Drill will come here to change the Path of the disk, vmx_path and folder path.
         * With that scripts will create VM with that path. In respective datastores.
         * In this method first we will split diskname with / .
         * And First part of the splitted strinf will b
         */

        internal bool ReNameDrDrillPath(AllServersForm allServersForm, ref Host h, Host hostinInfoFile)
        {
            try
            {
                Trace.WriteLine(DateTime.Now + "\t Entered to ReNameDrDrillPath in REcovryConfigurationPanelHandler.cs");
                ArrayList physicalDisks;
                ArrayList diskList;
                physicalDisks = h.disks.GetDiskList;
                diskList = hostinInfoFile.disks.GetDiskList;
                h.vmx_path = hostinInfoFile.vmx_path;
                h.folder_path = hostinInfoFile.folder_path;

                if (h.oldVMDirectory != null)
                {
                    h.vmx_path = h.vmx_path.Replace(h.oldVMDirectory + "/", "");
                    Trace.WriteLine(DateTime.Now + "\t Printing the vmx_path " + h.vmx_path);
                }
               
                //Assign new displayname with slash as a folder path.
                 h.folder_path = h.new_displayname + "/";
                //Spliting the vmx path with slash to make two parts.
                 //e.g:vmx_path="[datastore1 (2)] win2k8-32/win2k8-32.vmx"
                string[] vmxPath = h.vmx_path.ToString().Split('/');
                //Again spliting the first part of vmx path with brackets
                string displayname = vmxPath[0];
                string[] pathname = displayname.Split(']');
                
                if (pathname.Length > 1)
                {
                    pathname[1] = h.new_displayname;
                }
                displayname = null;
                for (int i = 0; i < pathname.Length; i++)
                {
                    if (displayname == null)
                    {
                        displayname = pathname[i] + "] ";
                    }
                    else
                    {
                        displayname = displayname + pathname[i];
                    }
                }
                vmxPath[0] = displayname;
                h.vmx_path = null;
                Trace.WriteLine(DateTime.Now + "\t Printing the displayname path" + displayname);
                
                    for (int i = 0; i < vmxPath.Length; i++)
                    {
                        if (h.vmx_path == null)
                        {
                            h.vmx_path = vmxPath[0] + "/";
                        }
                        else
                        {
                            h.vmx_path = h.vmx_path + vmxPath[i];
                        }
                    }
              
                Trace.WriteLine(DateTime.Now + "\t Printing the vmx path of machine " + h.new_displayname + "\t " + h.vmx_path);
                // Assigning the previous disk name sin case if user moves back and forth...
                foreach (Hashtable diskhash in diskList)
                {
                    foreach (Hashtable disk in physicalDisks)
                    {
                        disk["Protected"] = "no";
                        if (disk["target_disk_uuid"] != null)
                        {
                            if (disk["target_disk_uuid"] == disk["source_disk_uuid"])
                            {
                                Trace.WriteLine(DateTime.Now + "\t Printing scsi Mapping disk uuid target " + disk["target_disk_uuid"].ToString() + "\t diskhash uuid " + diskhash["source_disk_uuid"].ToString());
                                Trace.WriteLine(DateTime.Now + "\t Assigning the disknames " + diskhash["Name"].ToString());
                                disk["Name"] = diskhash["Name"].ToString();
                                if (h.oldVMDirectory != null)
                                {
                                    disk["Name"] = disk["Name"].ToString().Replace(h.oldVMDirectory + "/", "");
                                    Trace.WriteLine(DateTime.Now + "\t Printing the path " + h.oldVMDirectory);
                                    Trace.WriteLine(DateTime.Now + "\t Printing the diskname " + disk["Name"].ToString());
                                }
                                break;
                            }
                        }
                        else
                        {
                            if (disk["scsi_mapping_vmx"].ToString() == diskhash["scsi_mapping_vmx"].ToString())
                            {
                                Trace.WriteLine(DateTime.Now + "\t Printing scsi Mapping vmx " + disk["scsi_mapping_vmx"].ToString() + "\t diskhash name " + diskhash["scsi_mapping_vmx"].ToString());
                                Trace.WriteLine(DateTime.Now + "\t Assigning the disknames " + diskhash["Name"].ToString());
                                disk["Name"] = diskhash["Name"].ToString();
                                if (h.oldVMDirectory != null)
                                {
                                    disk["Name"] = disk["Name"].ToString().Replace(h.oldVMDirectory + "/", "");
                                    Trace.WriteLine(DateTime.Now + "\t Printing the path " + h.oldVMDirectory);
                                    Trace.WriteLine(DateTime.Now + "\t Printing the diskname " + disk["Name"].ToString());
                                }
                                break;
                            }
                        }
                        
                        
                    }

                }

                foreach (Hashtable disk in physicalDisks)
                {
                    //First spliting diskname inot two parts with / to diskname array
                    //Againg split first part with ] folderpath array
                    // make folderpath second part as new displayname and combine all the folderpath array
                    // Now combine parts of diskname array.
                    //After that we
                    Trace.WriteLine(DateTime.Now + "\t Printing disk name " + disk["Name"].ToString());
                    string[] diskname = disk["Name"].ToString().Split('/');
                    int splitCount = diskname.Length;
                    string foldername = diskname[0];
                    //Trace.WriteLine(DateTime.Now + "\t Printing the foldername " + foldername);
                    string[] folderpath = foldername.Split(']');
                    if (folderpath.Length > 1)
                    {
                        folderpath[1] = h.new_displayname;
                    }
                    foldername = null;
                    for (int i = 0; i < folderpath.Length; i++)
                    {
                        if (foldername == null)
                        {
                            foldername = folderpath[i] + "] ";
                        }
                        else
                        {
                            foldername = foldername  + folderpath[i];
                        }
                    }
                    diskname[0] = foldername;
                    Trace.WriteLine(DateTime.Now + "\t Printing the foldername " + foldername);
                  /*  // Adding some name to diskname = t1;
                    if (diskname[1] != null)
                    {
                        string[] disknamePart2 = diskname[1].Split('.');
                        diskname[1] = disknamePart2[0] + "_DR_Drill." + disknamePart2[1];
                    }*/
                    disk["Name"] = null;
                   
                        for (int i = 0; i < splitCount; i++)
                        {
                            if (disk["Name"] == null)
                            {
                                disk["Name"] = diskname[i] + "/";
                            }
                            else
                            {
                                disk["Name"] = disk["Name"] + diskname[i];
                                Trace.WriteLine(DateTime.Now + "\t Printing the disknames after splliting for drdrill " + disk["Name"].ToString());
                            }
                        }
                    
                }
                Trace.WriteLine(DateTime.Now + "\t Exiting to ReNameDrDrillPath in REcovryConfigurationPanelHandler.cs");
                return true;
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
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: Process of RecoverConfiguration.cs " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
               

                return false;
            }
        }
        internal bool VerifyingWithVmdks(AllServersForm allServersForm, Host h, Host source)
        {
            try
            {
                ArrayList physicalDisks = new ArrayList();
                ArrayList sourceDisks = new ArrayList();


                ArrayList targetSideDiskDetails;
                ArrayList actualProtection;
                targetSideDiskDetails = h.disks.GetDiskList;
                actualProtection = source.disks.GetDiskList;
                foreach (Hashtable hash in targetSideDiskDetails)
                {
                    string diskname = hash["Name"].ToString();
                    var pattern = @"\[(.*?)]";

                    var matches = Regex.Matches(diskname, pattern);
                    foreach (Match m in matches)
                    {
                        m.Result("");
                        diskname = diskname.Replace(m.ToString(), "");
                        string[] disksOntargetToCheckForFoldernameOption = diskname.Split('/');
                        if (disksOntargetToCheckForFoldernameOption.Length == 3)
                        {
                            diskname = null;
                            for (int i = 1; i < disksOntargetToCheckForFoldernameOption.Length; i++)
                            {
                                Trace.WriteLine(DateTime.Now + "\t Found that at the time of protection user has selected the fodername option for the machine " + h.displayname);
                                if (diskname == null)
                                {
                                    diskname = disksOntargetToCheckForFoldernameOption[i];
                                }
                                else
                                {
                                    diskname = diskname + "/" + disksOntargetToCheckForFoldernameOption[i];
                                }
                            }
                        }
                    }
                    foreach (Hashtable diskHash in actualProtection)
                    {
                        string targetDiskName = diskHash["Name"].ToString();
                        var matchesTarget = Regex.Matches(targetDiskName, pattern);
                        foreach (Match m in matchesTarget)
                        {
                            m.Result("");
                            targetDiskName = targetDiskName.Replace(m.ToString(), "");

                        }


                        if (diskname.TrimStart().TrimEnd().ToString() == targetDiskName.TrimStart().TrimEnd().ToString())
                        {
                            Trace.WriteLine(DateTime.Now + "\t Printing the Disknames after comparing successfull " + diskname + " " + targetDiskName);
                            Trace.WriteLine(DateTime.Now + "\t Printing the displaynames " + source.displayname + " \t target side " + h.displayname);
                            return true;
                        }
                    }
                }
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
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: ReadHostNode " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            return false;
        }

        internal override bool ValidatePanel(AllServersForm allServersForm)
        {

            return true;
        }
        internal override bool CanGoToNextPanel(AllServersForm allServersForm)
        {
            allServersForm.mandatoryLabel.Visible = true;
            return true;
        }

        internal override bool CanGoToPreviousPanel(AllServersForm allServersForm)
        {
            //In case of back and forth we need to save the selected values.
            //Which have been selected by the user.
            //We need to add the nic info also.

            foreach (Host h in allServersForm.sourceListByUser._hostList)
            {
                foreach (Host h1 in allServersForm.recoveryChecksPassedList._hostList)
                {
                    if (h.displayname == h1.displayname || h.hostname == h1.hostname)
                    {
                        h.memSize = h1.memSize;
                        h.cpuCount = h1.cpuCount;
                        h.nicList.list = h1.nicList.list;

                    }
                }
            }
            foreach (Host h in allServersForm.vmsSelectedRecovery._hostList)
            {
                foreach (Host h1 in allServersForm.recoveryChecksPassedList._hostList)
                {
                    if (h.displayname == h1.displayname || h.hostname == h1.hostname)
                    {
                        h.memSize = h1.memSize;
                        h.cpuCount = h1.cpuCount;
                        h.nicList.list = h1.nicList.list;


                    }
                }
            }
            allServersForm.previousButton.Visible = false;
            return true;
        }

        internal bool SelectedVM(AllServersForm allServersForm, string displayName)
        {
            try
            {
                // If user selected particular vm from the configuration tree view we need to 
                // all the details in the cpu and memory and nic info in their respective tables.
                Trace.WriteLine(DateTime.Now + " \t printing the value of node " + displayName);
                Host h = new Host();
                h.displayname = displayName;
                _displayName = displayName;
                int index = 0;
                allServersForm.enterTargetDisplayNameTextBox.Clear();
                if (allServersForm.recoveryChecksPassedList.DoesHostExist(h, ref  index) == true)
                {
                    h = (Host)allServersForm.recoveryChecksPassedList._hostList[index];


                    _selectedHost = h;
                    int rowIndex = 0;
                    allServersForm.networkDataGridView.Rows.Clear();
                    if (h.nicList.list.Count != 0)
                    {
                        Trace.WriteLine(DateTime.Now + " \t printing the count of the niclist " + h.nicList.list.Count.ToString());
                        foreach (Hashtable nicHash in h.nicList.list)
                        {
                            //Trace.WriteLine(DateTime.Now + " \t printing the values of the nic " + nic.name + " ip " + nic.ip + " mask " + nic.subnetMask);
                            allServersForm.networkDataGridView.Rows.Add(1);
                            allServersForm.networkDataGridView.Rows[rowIndex].Cells[NIC_NAME_COLUMN].Value = nicHash["nic_mac"].ToString();
                            if (nicHash["new_ip"] == null)
                            {
                                if (nicHash["ip"] != null)
                                {
                                    allServersForm.networkDataGridView.Rows[rowIndex].Cells[KEPP_OLD_VALUES_COLUMN].Value = nicHash["ip"].ToString();
                                }
                                else
                                {
                                    allServersForm.networkDataGridView.Rows[rowIndex].Cells[KEPP_OLD_VALUES_COLUMN].Value = "IP not configured";
                                }
                            }
                            else
                            {
                                allServersForm.networkDataGridView.Rows[rowIndex].Cells[KEPP_OLD_VALUES_COLUMN].Value = nicHash["new_ip"].ToString();
                            }
                            if (nicHash["Selected"] != null)
                            {
                                if (nicHash["Selected"].ToString() == "yes")
                                {
                                    allServersForm.networkDataGridView.Rows[rowIndex].Cells[SELECT_UNSELECT_NIC_COLUMN].Value = true;
                                }
                                else
                                {
                                    allServersForm.networkDataGridView.Rows[rowIndex].Cells[SELECT_UNSELECT_NIC_COLUMN].Value = false;
                                }
                                allServersForm.networkDataGridView.Rows[rowIndex].Cells[SELECT_UNSELECT_NIC_COLUMN].ReadOnly = true;
                            }
                            allServersForm.networkDataGridView.Rows[rowIndex].Cells[CHANGE_VALUE_COLUMN].Value = "Change";
                            rowIndex++;
                        }
                    }
                    Trace.WriteLine(DateTime.Now + " Printing the memory size " + h.memSize.ToString() + " \t space " + (h.memSize / 1024).ToString());
                    allServersForm.memoryValuesComboBox.SelectedItem = "MB";
                    allServersForm.memoryNumericUpDown.Maximum = MAX_SIZE_IN_MB;
                    if (h.memSize > MAX_SIZE_IN_MB)
                    {
                        allServersForm.memoryNumericUpDown.Value = MAX_SIZE_IN_MB;
                    }
                    else if (h.memSize == 0)
                    {
                        allServersForm.memoryNumericUpDown.Value = 512;
                    }
                    else 
                    {
                        allServersForm.memoryNumericUpDown.Value = h.memSize;
                    }
                    if (allServersForm.cpuComboBox.Items.Contains(h.cpuCount))
                    {
                        allServersForm.cpuComboBox.SelectedItem = h.cpuCount;
                    }
                    if (h.cpuCount == 0)
                    {
                        allServersForm.netWorkConfiurationGroupBox.Visible = false;
                        allServersForm.networkDataGridView.Visible = false;
                        allServersForm.hardWareGroupBox.Visible = false;
                        //allServersForm.oldProtectionLabel.Text = "This machine is protected with old vContinuum. This feature doesn't exist for old protection.";
                        // allServersForm.oldProtectionLabel.Visible = true;
                    }
                    else
                    {
                        allServersForm.netWorkConfiurationGroupBox.Visible = true;
                        allServersForm.networkDataGridView.Visible = true;
                        
                        allServersForm.hardWareGroupBox.Visible = true;
                        //allServersForm.oldProtectionLabel.Text = "This machine is protected with old vContinuum. This feature doesn't exist for old protection.";
                        // allServersForm.oldProtectionLabel.Visible = false;
                        
                    }
                    if (allServersForm.appNameSelected == AppName.Drdrill)
                    {

                        allServersForm.displayNameSettingsGroupBox.Visible = true;
                        allServersForm.enterTargetDisplaynameRadioButton.Checked = true;
                        if (_selectedHost.new_displayname != null)
                        {
                            if (allServersForm.hostBasedRadioButton.Checked == true)
                            {
                                allServersForm.enterTargetDisplayNameTextBox.Text = _selectedHost.new_displayname;
                            }
                            else
                            {
                                foreach (Host selectedHost in allServersForm.recoveryChecksPassedList._hostList)
                                {
                                    foreach (Host h1 in allServersForm.esxSelected.GetHostList)
                                    {
                                        if (selectedHost.new_displayname == h1.displayname && selectedHost.target_uuid == h1.source_uuid)
                                        {
                                            Trace.WriteLine(DateTime.Now + "\t adding _DR drill in text box for the Vm " + selectedHost.new_displayname + " Info displayname " + h1.displayname);
                                            allServersForm.enterTargetDisplayNameTextBox.Text = selectedHost.new_displayname + "_DR_Drill";
                                            //_selectedHost.new_displayname = _selectedHost.new_displayname + "_DR_Drill";
                                            selectedHost.new_displayname = selectedHost.new_displayname + "_DR_Drill";

                                           
                                        }
                                        if (_selectedHost.new_displayname == h1.displayname && _selectedHost.target_uuid == h1.source_uuid)
                                        {
                                            Trace.WriteLine(DateTime.Now + "\t adding _DR drill in text box for the Vm " + _selectedHost.new_displayname + " Info displayname " + h1.displayname);
                                            allServersForm.enterTargetDisplayNameTextBox.Text = _selectedHost.new_displayname + "_DR_Drill";
                                            //_selectedHost.new_displayname = _selectedHost.new_displayname + "_DR_Drill";
                                            _selectedHost.new_displayname = _selectedHost.new_displayname + "_DR_Drill";


                                        }
                                    }
                                }
                                if (allServersForm.enterTargetDisplayNameTextBox.Text.Length == 0)
                                {
                                    allServersForm.enterTargetDisplayNameTextBox.Text = _selectedHost.new_displayname;
                                }
                            }
                            allServersForm.appliedPrefixTextBox.Clear();
                            allServersForm.suffixTextBox.Clear();


                        }
                        else
                        {
                            if (allServersForm.hostBasedRadioButton.Checked == true)
                            {
                                allServersForm.enterTargetDisplayNameTextBox.Text = _selectedHost.displayname;
                            }
                            else
                            {
                                foreach (Host selectedHost in allServersForm.recoveryChecksPassedList._hostList)
                                {
                                    foreach (Host h1 in allServersForm.esxSelected.GetHostList)
                                    {
                                        if (selectedHost.displayname == h1.displayname && selectedHost.target_uuid == h1.source_uuid)
                                        {
                                            Trace.WriteLine(DateTime.Now + "\t adding _DR drill in text box for the Vm " + h.new_displayname + " Info displayname " + h1.displayname);
                                            allServersForm.enterTargetDisplayNameTextBox.Text = _selectedHost.displayname + "_DR_Drill";
                                            _selectedHost.new_displayname = _selectedHost.displayname + "_DR_Drill";
                                            break;
                                        }
                                    }
                                }
                                if (allServersForm.enterTargetDisplayNameTextBox.Text.Length == 0)
                                {
                                    allServersForm.enterTargetDisplayNameTextBox.Text = _selectedHost.displayname;
                                }
                            }
                            allServersForm.appliedPrefixTextBox.Clear();
                            allServersForm.suffixTextBox.Clear();
                        }

                        allServersForm.advancedSettingsLinkLabel.Visible = true;
                    }
                    else
                    {
                        allServersForm.advancedSettingsLinkLabel.Visible = false;
                    }
                }
                else
                { 
                     allServersForm.netWorkConfiurationGroupBox.Visible = false;
                    allServersForm.hardWareGroupBox.Visible = false;
                    allServersForm.displayNameSettingsGroupBox.Visible = false;
                    
                    allServersForm.advancedSettingsLinkLabel.Visible = false;
                }                
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: SelectedVM of RecoverConfiguration.cs " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            return true;
        }

        internal int ipcnt = 1;
        internal int maskcnt = 1;
        internal int gatewayCount = 0;
        internal int dnsipCount = 0;
        internal string ips;
        internal string masks;
        internal string gateways;
        internal string dnsips;
        NicPropertiesForm nicForm;
        internal string _newmask = null;
        internal int _newmask_cnt = 0;
        internal string[] _netmask_ips;
        internal int tbpgcnt = 0;
        string[] mask_Ips = null;
        internal static int tabclrcnt = 0;
        internal string[] _gateway_ips;
        internal string[] _dns_ips;
        internal string[] _ips;

        internal GroupBox[] g;                                             //Added From
        internal RadioButton[] radioButton1;
        internal RadioButton[] radioButton2;
        internal GroupBox[] groupBox1;
        internal Label[] label1;
        internal Label[] label2;
        internal Label[] label3;
        internal Label[] label4;
        internal Label[] label5;
        internal Label[] label6;
        internal Label[] label7;
        internal Label[] label8;
        internal TextBox[] textBox1;
        internal TextBox[] textBox2;
        internal TextBox[] textBox3;
        internal TextBox[] textBox4;
        internal TabPage[] tabPage;

        protected void Checked_Changed1(Object sender, EventArgs e)
        {
            int i = nicForm.ipTabControl.SelectedIndex;

            if (radioButton1[i].Checked == true)
            {
                foreach (Control c1 in groupBox1[i].Controls)
                {
                    if (c1.Name == "ipTextBox1" || c1.Name == "subNetMaskTextBox1" || c1.Name == "defaultGateWayTextBox1" || c1.Name == "dnsTextBox1")
                    {
                        c1.Enabled = false;
                    }
                }
                nicForm.dhcp = true;
            }
            else
            {
                radioButton2[i].Checked = true;
            }
        }

        protected void Checked_Changed2(Object sender, EventArgs e)
        {
            int i = nicForm.ipTabControl.SelectedIndex;

            if (radioButton2[i].Checked == true)
            {
                foreach (Control control in groupBox1[i].Controls)
                {
                    control.Enabled = true;
                }
                nicForm.dhcp = false;
            }
            else if (radioButton1[i].Checked == true)
            {
                foreach (Control control in groupBox1[i].Controls)
                {
                    if (control.Name == "ipTextBox1" || control.Name == "subNetMaskTextBox1" || control.Name == "defaultGateWayTextBox1" || control.Name == "dnsTextBox1")
                        control.Enabled = false;
                }
                nicForm.dhcp = true;
            }
        }
        internal bool SelectedNic(AllServersForm allServersForm, string nicName, int rowindex)
        {
            try
            {
                foreach (Hashtable hash in _selectedHost.nicList.list)
                {
                    if (hash["nic_mac"].ToString() == nicName)
                    {
                        if ((bool)allServersForm.networkDataGridView.Rows[rowindex].Cells[SELECT_UNSELECT_NIC_COLUMN].FormattedValue)
                        {
                            hash["Selected"] = "yes";
                        }
                        else
                        {
                            hash["Selected"] = "no";
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed to updated selected info of nic in recoveryConfiguration panel handler");
                Trace.WriteLine(DateTime.Now + "\t" + ex.Message);
            }
            return true;
        }

        internal bool NetworkSetting(AllServersForm allServersForm, string nicName, int rowindex)
        {
            try
            {
                ipcnt = 1;
                maskcnt = 1;
                ips = null;
                masks = null;
                gateways = null;
                dnsips = null;
                NicPropertiesForm nicForm = new NicPropertiesForm(allServersForm.osTypeSelected);
                bool dhcp = false;
                if (_selectedHost.nicList.list.Count != 0)
                {
                    _selectedHost.keepNicOldValues = false;
                    Host masterHost = (Host)allServersForm.masterHostAdded;
                    foreach (Hashtable hash in _selectedHost.nicList.list)
                    {


                        if (hash["nic_mac"].ToString() == nicName)
                        {

                            if (masterHost.networkNameList.Count != 0)
                            {
                                nicForm.netWorkComboBox.Items.Clear();
                                foreach (Network network in masterHost.networkNameList)
                                {
                                    if (masterHost.vSpherehost == network.Vspherehostname)
                                    {
                                        Trace.WriteLine(DateTime.Now + "\t Printing the value of the networkname " + network.Networkname);
                                        nicForm.netWorkComboBox.Items.Add(network.Networkname);
                                    }
                                }
                            }
                            if (hash["dhcp"].ToString() == "0")
                            {
                                if (hash["new_network_name"] != null)
                                {
                                    nicForm.netWorkComboBox.Text = hash["new_network_name"].ToString();
                                }
                                else
                                {
                                    if (hash["network_name"] != null)
                                    {
                                        for (int i = 0; i < nicForm.netWorkComboBox.Items.Count; i++)
                                        {
                                            if (hash["network_name"].ToString() == nicForm.netWorkComboBox.Items[i].ToString())
                                            {
                                                nicForm.netWorkComboBox.SelectedItem = nicForm.netWorkComboBox.Items[i].ToString();
                                            }
                                        }
                                    }
                                }

                            }
                            else
                            {
                                if (hash["new_network_name"] != null)
                                {
                                    nicForm.netWorkComboBox.Text = hash["new_network_name"].ToString();
                                }
                                else
                                {
                                    if (hash["network_name"] != null)
                                    {
                                        for (int i = 0; i < nicForm.netWorkComboBox.Items.Count; i++)
                                        {
                                            if (hash["network_name"].ToString() == nicForm.netWorkComboBox.Items[i].ToString())
                                            {
                                                nicForm.netWorkComboBox.SelectedItem = nicForm.netWorkComboBox.Items[i].ToString();
                                            }
                                        }
                                    }
                                }
                                nicForm.useDhcpRadioButton.Checked = true;
                                dhcp = true;
                            }



                            if (dhcp == true)
                            {
                                nicForm.useDhcpRadioButton.Checked = true;
                            }
                            _selectedHost.keepNicOldValues = false;

                            //Count the Number Of IPs for the Selected NIC and then create that many number of TabPages in NICFORM and display the NICFORM

                            //ips = allServersForm.networkDataGridView.Rows[rowindex].Cells[1].Value.ToString();
                            if (hash["new_ip"] != null)
                            {
                                ips = hash["new_ip"].ToString();
                            }
                            else if (hash["ip"] != null)
                            {
                                ips = hash["ip"].ToString();

                            }
                            if (hash["new_mask"] != null)
                            {
                                masks = hash["new_mask"].ToString();
                            }
                            else if (hash["mask"] != null)
                            {
                                masks = hash["mask"].ToString();

                            }
                            else
                            {
                                masks = null;
                            }
                            if (hash["new_dnsip"] != null)
                            {
                                dnsips = hash["new_dnsip"].ToString();
                            }
                            else if (hash["dnsip"] != null)
                            {
                                dnsips = hash["dnsip"].ToString();
                            }
                            else
                            {
                                dnsips = null;
                            }
                            if (hash["new_gateway"] != null)
                            {
                                gateways = hash["new_gateway"].ToString();
                            }
                            else if (hash["gateway"] != null)
                            {
                                gateways = hash["gateway"].ToString();
                            }
                            else
                            {
                                gateways = null;
                            }
                            ipcnt = 1;
                            if (ips != null)
                            {
                                int ips_length = ips.Length;
                                if (ips[ips_length - 1].ToString() == ",")
                                {
                                    ips = ips.Substring(0, ips_length - 2);
                                }
                                for (int i = 0; i < ips.Length; i++)
                                {
                                    if (ips[i].ToString() == ",")
                                    {
                                        ipcnt++;
                                    }
                                }
                            }


                            g = new GroupBox[ipcnt];
                            radioButton1 = new RadioButton[ipcnt];
                            radioButton2 = new RadioButton[ipcnt];
                            groupBox1 = new GroupBox[ipcnt];
                            label1 = new Label[ipcnt];
                            label2 = new Label[ipcnt];
                            label3 = new Label[ipcnt];
                            label4 = new Label[ipcnt];
                            label5 = new Label[ipcnt];
                            label6 = new Label[ipcnt];
                            label7 = new Label[ipcnt];
                            label8 = new Label[ipcnt];

                            textBox1 = new TextBox[ipcnt];
                            textBox2 = new TextBox[ipcnt];
                            textBox3 = new TextBox[ipcnt];
                            textBox4 = new TextBox[ipcnt];


                            tabPage = new TabPage[ipcnt];


                            string[] _ips = ip_split(ips, ipcnt);
                            string[] _masks;
                            if (masks != null)
                            {
                                maskcnt = 1;
                                int maskip_length = masks.Length;
                                if (masks[maskip_length - 1].ToString() == ",")
                                {
                                    masks = masks.Substring(0, maskip_length - 2);
                                }
                                for (int i = 0; i < masks.Length; i++)
                                {
                                    if (masks[i].ToString() == ",")
                                    {
                                        maskcnt++;
                                    }
                                }
                                _masks = ip_split(masks, maskcnt);

                            }
                            else
                            {
                                _masks = null;
                            }

                            string[] gatewayslist;
                            gatewayCount = 0;
                            if (gateways != null)
                            {
                                gatewayCount = 1;
                                int gateways_Length = gateways.Length;
                                if (gateways[gateways_Length - 1].ToString() == ",")
                                {
                                    gateways = gateways.Substring(0, gateways_Length - 2);
                                }
                                for (int i = 0; i < gateways.Length; i++)
                                {
                                    if (gateways[i].ToString() == ",")
                                    {
                                        gatewayCount++;
                                    }
                                }
                                gatewayslist = ip_split(gateways, gatewayCount);
                            }
                            else
                            {
                                gatewayslist = null;
                            }
                            dnsipCount = 0;
                            string[] dnsipList;
                            if (dnsips != null)
                            {
                                dnsipCount = 1;
                                int dnsips_length = dnsips.Length;
                                if (dnsips[dnsips_length - 1].ToString() == ",")
                                {
                                    dnsips = dnsips.Substring(0, dnsips_length - 2);
                                }
                                for (int i = 0; i < dnsips.Length; i++)
                                {
                                    if (dnsips[i].ToString() == ",")
                                    {
                                        dnsipCount++;
                                    }
                                }
                                dnsipList = ip_split(dnsips, dnsipCount);
                            }
                            else
                            {
                                dnsipList = null;
                            }
                            if (tabclrcnt == 0)
                            {
                                nicForm.ipTabControl.TabPages.Clear();
                            }
                            for (int i = 0; i < ipcnt; i++)
                            {

                                tabPage[i] = new TabPage();
                                //tp[i].Text = _ips[i].ToString();                            
                                tabPage[i].Text = "IP - " + (i + 1);
                                tabPage[i].BackColor = Color.White;
                                nicForm.ipTabControl.TabPages.Add(tabPage[i]);

                                label1[i] = new Label();
                                label1[i].Name = "ipLabel1";
                                label1[i].Location = new Point(15, 36);
                                label1[i].Width = 60;
                                label1[i].Height = 13;
                                label1[i].Text = "IP Address";
                                tabPage[i].Controls.Add(label1[i]);

                                label6[i] = new Label();
                                label6[i].Name = "ipStarLabel";
                                label6[i].Location = new Point(240, 36);
                                label6[i].Width = 10;
                                label6[i].Height = 13;
                                label6[i].Text = "*";
                                label6[i].ForeColor = Color.Red;
                                tabPage[i].Controls.Add(label6[i]);



                                textBox1[i] = new TextBox();
                                textBox1[i].Name = "ipTextBox1";
                                textBox1[i].Location = new Point(116, 33);
                                textBox1[i].Width = 115;
                                textBox1[i].Height = 20;
                                if (_ips[i] != null)
                                {
                                    textBox1[i].Text = _ips[i].ToString();
                                }
                                tabPage[i].Controls.Add(textBox1[i]);

                                label2[i] = new Label();
                                label2[i].Name = "subNetMaskLabel1";
                                label2[i].Location = new Point(15, 69);
                                label2[i].Width = 72;
                                label2[i].Height = 13;
                                label2[i].Text = "Subnet Mask :";
                                tabPage[i].Controls.Add(label2[i]);

                                textBox2[i] = new TextBox();
                                textBox2[i].Name = "subNetMaskTextBox1";
                                textBox2[i].Location = new Point(116, 66);
                                textBox2[i].Width = 115;
                                textBox2[i].Height = 20;
                                if (ipcnt == maskcnt)
                                {
                                    if (masks != null)
                                    {
                                        textBox2[i].Text = _masks[i].ToString();
                                    }
                                }
                                else
                                {
                                    if (hash["mask"] != null)
                                    {
                                        if (hash["mask"].ToString().Contains(","))
                                        {
                                            textBox2[i].Text = hash["mask"].ToString().Split(',')[0];
                                        }
                                        else
                                        {
                                            textBox2[i].Text = hash["mask"].ToString();
                                        }
                                    }
                                }

                                //if (tabclrcnt != 0)
                                //{
                                //    if (mask_Ips[i].ToString() != null)
                                //    {
                                //        t2[i].Text = mask_Ips[i];
                                //    }
                                //}
                                tabPage[i].Controls.Add(textBox2[i]);
                                label7[i] = new Label();
                                label7[i].Name = "maskStarLabel";
                                label7[i].Location = new Point(240, 69);
                                label7[i].Width = 10;
                                label7[i].Height = 13;
                                label7[i].Text = "*";
                                label7[i].ForeColor = Color.Red;
                                tabPage[i].Controls.Add(label7[i]);


                                label3[i] = new Label();
                                label3[i].Name = "defaultGateWayLabel1";
                                label3[i].Location = new Point(15, 100);
                                label3[i].Width = 87;
                                label3[i].Height = 13;
                                label3[i].Text = "GateWay :";
                                tabPage[i].Controls.Add(label3[i]);

                                textBox3[i] = new TextBox();
                                textBox3[i].Name = "defaultGateWayTextBox1";
                                textBox3[i].Location = new Point(116, 97);
                                textBox3[i].Width = 115;
                                textBox3[i].Height = 20;
                                if (ipcnt == gatewayCount)
                                {
                                    if (gateways != null)
                                    {
                                        if (gatewayslist[i].ToString() != "notselected")
                                        {
                                            textBox3[i].Text = gatewayslist[i].ToString();
                                        }
                                    }
                                }
                                else
                                {
                                    if (hash["gateway"] != null)
                                    {
                                        if (hash["gateway"].ToString().Contains(","))
                                        {
                                            textBox3[i].Text = hash["gateway"].ToString().Split(',')[0];
                                        }
                                        else
                                        {
                                            textBox3[i].Text = hash["gateway"].ToString();
                                        }
                                    }

                                }
                                tabPage[i].Controls.Add(textBox3[i]);

                                label4[i] = new Label();
                                label4[i].Name = "dnsLabel1";
                                label4[i].Location = new Point(15, 129);
                                label4[i].Width = 33;
                                label4[i].Height = 13;
          
                                label4[i].Text = "DNS :";
                                tabPage[i].Controls.Add(label4[i]);

                                textBox4[i] = new TextBox();
                                textBox4[i].Name = "dnsTextBox1";
                                textBox4[i].Location = new Point(116, 126);
                                textBox4[i].Width = 115;
                                textBox4[i].Height = 20;
                                if (ipcnt == dnsipCount)
                                {
                                    if (gateways != null)
                                    {
                                        if (dnsipList[i].ToString() != "notselected")
                                        {
                                            textBox4[i].Text = dnsipList[i].ToString();
                                        }
                                    }
                                }
                                else
                                {
                                    if (hash["new_dnsip"] != null)
                                    {
                                        if (allServersForm.osTypeSelected == OStype.Linux || allServersForm.osTypeSelected == OStype.Solaris)
                                        {
                                            if (hash["new_dnsip"].ToString().Contains(","))
                                            {
                                                textBox4[i].Text = hash["new_dnsip"].ToString().Split(',')[0];
                                            }
                                            else
                                            {
                                                textBox4[i].Text = hash["new_dnsip"].ToString();
                                            }
                                        }
                                        else
                                        {
                                            textBox4[i].Text = hash["new_dnsip"].ToString();
                                        }
                                    }
                                    else
                                    {
                                        if (hash["dnsip"] != null)
                                        {
                                            if (allServersForm.osTypeSelected == OStype.Linux || allServersForm.osTypeSelected == OStype.Solaris)
                                            {
                                                if (hash["dnsip"].ToString().Contains(","))
                                                {
                                                    textBox4[i].Text = hash["dnsip"].ToString().Split(',')[0];
                                                }
                                                else
                                                {
                                                    textBox4[i].Text = hash["dnsip"].ToString();
                                                }
                                            }
                                            else
                                            {
                                                textBox4[i].Text = hash["dnsip"].ToString();
                                            }
                                        }
                                    }
                                }                              
                                tabPage[i].Controls.Add(textBox4[i]);

                                
                                //c1.SelectedIndex = 0;
                            }
                        }
                    }
                }
                nicForm.ipTabControl.SelectedIndex = 0;
                tbpgcnt = nicForm.ipTabControl.TabPages.Count;
                nicForm.addButton.Visible = true;
                nicForm.cancelButton.Visible = true;
                nicForm.ipText = textBox1;
                nicForm.subnetMaskText = textBox2;
                nicForm.dnsText = textBox4;
                nicForm.gateWayText = textBox3;
                try
                {
                    if (nicForm.netWorkComboBox.Text.Length == 0)
                    {
                        nicForm.netWorkComboBox.Text = allServersForm.cacheNetworkNamePrepared;
                    }

                }
                catch (Exception ex)
                {
                    Trace.WriteLine(DateTime.Now + "\t Failed to Update the network combobox " + ex.Message);
                }
                try
                {
                    for (int i = 0; i < nicForm.ipTabControl.TabCount; i++)
                    {
                        if (nicForm.subnetMaskText[i].Text.Length == 0 && _cacheSubnetmask != null)
                        {
                            nicForm.subnetMaskText[i].Text = _cacheSubnetmask;
                        }
                        if (nicForm.gateWayText[i].Text.Length == 0 && _cacheGateway != null)
                        {
                            nicForm.gateWayText[i].Text = _cacheGateway;
                        }
                        if (nicForm.dnsText[i].Text.Length == 0 && _cacheDns != null)
                        {
                            nicForm.dnsText[i].Text = _cacheDns;
                        }
                    }
                }
                catch (Exception ex)
                {
                    Trace.WriteLine(DateTime.Now + "\t Failed to update cache values " + ex.Message);
                }
                  nicForm.refreshPictureBox.Visible = true;
                    
                    nicForm.masterHost = allServersForm.masterHostAdded;
                
                nicForm.ShowDialog();
                // Once the nic form is closed we will read the nic information into the particular vm info like data structures....
                if (nicForm.canceled == false)
                {
                    if (nicForm.masterHost.networkNameList.Count != 0)
                    {
                        Trace.WriteLine(DateTime.Now + "\t master list network count " + nicForm.masterHost.networkNameList.Count.ToString());
                        allServersForm.masterHostAdded.networkNameList = nicForm.masterHost.networkNameList;
                    }
                    foreach (Hashtable hash in _selectedHost.nicList.list)
                    {
                        if (hash["nic_mac"].ToString() == nicName)
                        {
                            if (nicForm.useDhcpRadioButton.Checked == false)
                            {
                                allServersForm.networkDataGridView.Rows[rowindex].Cells[KEPP_OLD_VALUES_COLUMN].Value = "";



                                for (int c = 0; c < ipcnt; c++)
                                {
                                    if (c == 0)
                                    {

                                        if (textBox1[c].Text.Length > 0)
                                        {// When user changed the ip address of particular nic we are going to change the 
                                            // that ip in the datagridview by comoparing the mac addresss.
                                            hash["new_ip"] = textBox1[c].Text;
                                            for (int i = 0; i < allServersForm.networkDataGridView.RowCount; i++)
                                            {
                                                if (allServersForm.networkDataGridView.Rows[i].Cells[NIC_NAME_COLUMN].Value != null)
                                                {
                                                    if (allServersForm.networkDataGridView.Rows[i].Cells[NIC_NAME_COLUMN].Value.ToString() == hash["nic_mac"].ToString())
                                                    {
                                                        if (c != ipcnt - 1)
                                                        {
                                                            allServersForm.networkDataGridView.Rows[i].Cells[KEPP_OLD_VALUES_COLUMN].Value = allServersForm.networkDataGridView.Rows[i].Cells[KEPP_OLD_VALUES_COLUMN].Value + textBox1[c].Text + ",";
                                                        }
                                                        else
                                                        {
                                                            allServersForm.networkDataGridView.Rows[i].Cells[KEPP_OLD_VALUES_COLUMN].Value = allServersForm.networkDataGridView.Rows[i].Cells[KEPP_OLD_VALUES_COLUMN].Value + textBox1[c].Text;
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                        if (textBox2[c].Text.Length > 0)
                                        {
                                            if (c == 0)
                                            {
                                                hash["new_mask"] = textBox2[c].Text;
                                                Trace.WriteLine(DateTime.Now + "\t Printing new mask " + hash["new_mask"].ToString());
                                            }
                                            else
                                            {
                                                if (hash["new_mask"] == null)
                                                {
                                                    hash["new_mask"] = textBox2[c].Text;
                                                }
                                                else
                                                {
                                                    hash["new_mask"] = hash["new_mask"] + "," + textBox2[c].Text;
                                                }
                                                Trace.WriteLine(DateTime.Now + "\t Printing new mask " + hash["new_mask"].ToString());
                                            }
                                        }
                                        if (textBox4[c].Text.Length > 0)
                                        {
                                            if (c == 0)
                                            {
                                                if (textBox4[c].Text.Length > 0)
                                                {
                                                    hash["new_dnsip"] = textBox4[c].Text;
                                                }
                                                else
                                                {
                                                    hash["new_dnsip"] = "notselected";
                                                }

                                            }
                                            else
                                            {
                                                if (textBox4[c].Text.Length > 0)
                                                {
                                                    hash["new_dnsip"] = textBox4[c].Text;
                                                }
                                                else
                                                {
                                                    hash["new_dnsip"] = "notselected";
                                                }

                                            }
                                            Trace.WriteLine(DateTime.Now + "\t Printing new dns " + hash["new_dnsip"].ToString());
                                        }
                                        else
                                        {
                                            hash["new_dnsip"] = "notselected";
                                        }
                                        if (textBox3[c].Text.Length > 0)
                                        {
                                            if (c == 0)
                                            {
                                                if (textBox3[c].Text.Length > 0)
                                                {
                                                    hash["new_gateway"] = textBox3[c].Text;
                                                }
                                                else
                                                {
                                                    hash["new_gateway"] = "notselected";
                                                }
                                                Trace.WriteLine(DateTime.Now + "\t Printing new gateway " + hash["new_gateway"].ToString());
                                            }
                                            else
                                            {

                                                if (textBox3[c].Text.Length > 0)
                                                {
                                                    hash["new_gateway"] = textBox3[c].Text;
                                                }
                                                else
                                                {
                                                    hash["new_gateway"] = "notselected";
                                                }
                                                Trace.WriteLine(DateTime.Now + "\t Printing new gateway " + hash["new_gateway"].ToString());
                                            }
                                        }
                                        else
                                        {
                                            hash["new_gateway"] = "notselected";
                                        }
                                    }
                                    else
                                    {
                                        if (textBox1[c].Text.Length > 0)
                                        {// When user changed the ip address of particular nic we are going to change the 
                                            // that ip in the datagridview by comoparing the mac addresss.
                                            if (c != ipcnt - 1)
                                            {
                                                if (hash["new_ip"] == null)
                                                {
                                                    hash["new_ip"] = textBox1[c].Text;
                                                }
                                                else
                                                {
                                                    hash["new_ip"] = hash["new_ip"] + "," + textBox1[c].Text;
                                                }
                                                Trace.WriteLine(DateTime.Now + "\t new ips " + hash["new_ip"].ToString());
                                            }
                                            else
                                            {
                                                if (hash["new_ip"] == null)
                                                {
                                                    hash["new_ip"] = textBox1[c].Text;
                                                }
                                                else
                                                {
                                                    hash["new_ip"] = hash["new_ip"] + "," + textBox1[c].Text;
                                                }
                                                Trace.WriteLine(DateTime.Now + "\t new ips " + hash["new_ip"].ToString());
                                            }
                                            for (int i = 0; i < allServersForm.networkDataGridView.RowCount; i++)
                                            {
                                                if (allServersForm.networkDataGridView.Rows[i].Cells[NIC_NAME_COLUMN].Value != null)
                                                {
                                                    if (allServersForm.networkDataGridView.Rows[i].Cells[NIC_NAME_COLUMN].Value.ToString() == hash["nic_mac"].ToString())
                                                    {
                                                        if (c != ipcnt - 1)
                                                        {
                                                            //allServersForm.networkDataGridView.Rows[i].Cells[KEPP_OLD_VALUES_COLUMN].Value = allServersForm.networkDataGridView.Rows[i].Cells[KEPP_OLD_VALUES_COLUMN].Value + t1[c].Text + ",";
                                                            allServersForm.networkDataGridView.Rows[i].Cells[KEPP_OLD_VALUES_COLUMN].Value = hash["new_ip"].ToString();
                                                        }
                                                        else
                                                        {
                                                            //allServersForm.networkDataGridView.Rows[i].Cells[KEPP_OLD_VALUES_COLUMN].Value = allServersForm.networkDataGridView.Rows[i].Cells[KEPP_OLD_VALUES_COLUMN].Value + t1[c].Text;
                                                            allServersForm.networkDataGridView.Rows[i].Cells[KEPP_OLD_VALUES_COLUMN].Value = hash["new_ip"].ToString();
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                        if (textBox2[c].Text.Length > 0)
                                        {
                                            if (c != ipcnt - 1)
                                            {

                                                hash["new_mask"] = hash["new_mask"] + "," + textBox2[c].Text;
                                            }
                                            else
                                            {

                                                hash["new_mask"] = hash["new_mask"] + "," + textBox2[c].Text;
                                            }
                                        }
                                        if (textBox4[c].Text.Length > 0)
                                        {
                                            if (c != ipcnt - 1)
                                            {
                                                if (textBox4[c].Text.Length > 0)
                                                {
                                                    hash["new_dnsip"] = hash["new_dnsip"] + "," + textBox4[c].Text;
                                                }
                                                else
                                                {
                                                    hash["new_dnsip"] = hash["new_dnsip"] + "," + "notselected";
                                                }

                                            }
                                            else
                                            {

                                                hash["new_dnsip"] = hash["new_dnsip"] + "," + "notselected";
                                            }
                                            Trace.WriteLine(DateTime.Now + "\t Printing new dns " + hash["new_dnsip"].ToString());
                                        }
                                        else
                                        {
                                            if (hash["new_dnsip"] != null)
                                            {
                                                hash["new_dnsip"] = hash["new_dnsip"] + "," + "notselected";
                                            }
                                            else
                                            {
                                                hash["new_dnsip"] = "notselected";
                                            }
                                        }
                                        if (textBox3[c].Text.Length > 0)
                                        {
                                            if (c != ipcnt - 1)
                                            {
                                                if (textBox3[c].Text.Length > 0)
                                                {
                                                    hash["new_gateway"] = hash["new_gateway"] + "," + textBox3[c].Text;
                                                }
                                                else
                                                {
                                                    hash["new_gateway"] = hash["new_gateway"] + "," + "notselected";
                                                }
                                            }
                                            else
                                            {
                                                if (textBox3[c].Text.Length > 0)
                                                {

                                                    hash["new_gateway"] = hash["new_gateway"] + "," + textBox3[c].Text;
                                                }
                                                else
                                                {
                                                    hash["new_gateway"] = hash["new_gateway"] + "," + "notselected";
                                                }
                                            }
                                            Trace.WriteLine(DateTime.Now + "\t Printing new gateway " + hash["new_gateway"].ToString());
                                        }
                                        else
                                        {
                                            if (hash["new_gateway"] != null)
                                            {
                                                hash["new_gateway"] = hash["new_gateway"] + "," + "notselected";
                                            }
                                            else
                                            {
                                                hash["new_gateway"] = "notselected";
                                            }
                                        }
                                    }
                                }

                                if (nicForm.netWorkComboBox.Text.Length != 0)
                                {
                                    allServersForm.cacheNetworkNamePrepared = nicForm.vmNetWorkLabelSelected;
                                    hash["new_network_name"] = nicForm.vmNetWorkLabelSelected;
                                }
                                _cacheSubnetmask = nicForm.cacheSubNetmask;
                                _cacheGateway = nicForm.cacheGateway;
                                _cacheDns = nicForm.cacheDNS;
                                if (nicForm.dhcp == true)
                                {
                                    hash["dhcp"] = "1";
                                }
                                else
                                {
                                    hash["dhcp"] = "0";
                                }
                            }
                            else
                            {
                                if (nicForm.netWorkComboBox.Text.Length != 0)
                                {
                                    allServersForm.cacheNetworkNamePrepared = nicForm.vmNetWorkLabelSelected;
                                    hash["new_network_name"] = nicForm.vmNetWorkLabelSelected;
                                }
                                hash["dhcp"] = "1";
                            }

                        }
                    }
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: NetWorkingSetting of ConfigurationPanelHandler.cs " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            return true;
        }







        internal string[] ip_split(string ip, int cnt)
        {
            string[] ips = new string[cnt];

            int i = 0, j = 0, k = 0;
            for (i = 0; i < cnt - 1; i++)
            {
                j = 0;
                k = ip.IndexOf(",", 0, ip.Length);
                k = k - 1 - j;
                ips[i] = ip.Substring(j, k + 1);
                j = k + 2;
                ip = ip.Substring(j);
            }
            ips[i] = ip;
            return (ips);
        }   


        /*
        * When user selected Memory Values Combo box we will call this method.
        * Here we will check Whether user selected GB\MB.
        * Based on the selection we will show the user what he has selected memory.
        * If user seleted any wrong values like less than 512mb we will make that as 512 mb
        * In case of gb if user selected more than the max ram size we will assign max size.
        */
        internal bool MemorySelectedinGBOrMB(AllServersForm allServersForm)
        {
            try
            {
                int memsize = 0;
                foreach (Configurations config in allServersForm.masterHostAdded.configurationList)
                {
                    if (config.vSpherehostname == allServersForm.masterHostAdded.vSpherehost)
                    {
                        memsize = (int)Math.Round(config.memsize / 1024);
                        //allServersForm.memoryNumericUpDown.Maximum = memsize;
                        break;
                    }
                }
                if (allServersForm.memoryValuesComboBox.SelectedItem.ToString() == "GB")
                {
                    allServersForm.memoryNumericUpDown.Maximum = memsize;
                    allServersForm.memoryNumericUpDown.Minimum = 1;
                    allServersForm.memoryNumericUpDown.Increment = 1;
                    if (_selectedHost.memSize != 0 && _selectedHost.memSize >= 1024)
                    {
                        allServersForm.memoryNumericUpDown.Value = _selectedHost.memSize / 1024;

                    }
                    else
                    {
                        _selectedHost.memSize = 1;

                        allServersForm.memoryNumericUpDown.Value = 1;
                        _selectedHost.memSize = 1024;
                    }
                }
                else
                {
                    allServersForm.memoryNumericUpDown.Maximum = memsize * 1024;

                    //allServersForm.memoryNumericUpDown.Minimum = 512;
                    allServersForm.memoryNumericUpDown.Increment = 4;

                    if (_selectedHost.memSize != 0)
                    {
                        if (_selectedHost.memSize < 512)
                        {
                            MessageBox.Show("You have selected memory size less than 512MB. Minimum value is 512MB", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            allServersForm.memoryNumericUpDown.Value = 512;
                            _selectedHost.memSize = 512;
                        }
                        else
                        {
                            allServersForm.memoryNumericUpDown.Value = _selectedHost.memSize;
                        }
                    }
                    else
                    {
                        allServersForm.memoryNumericUpDown.Value = 512;
                        _selectedHost.memSize = 512;
                    }
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: MemorySelectedinGBOrMB of RecoverConfiguration.cs " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            return true;
        }
        internal bool SelectCpuCount(AllServersForm allServersForm)
        {
            try
            {
                if (allServersForm.cpuComboBox.SelectedItem != null)
                {
                    _selectedHost.cpuCount = int.Parse(allServersForm.cpuComboBox.SelectedItem.ToString());
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: SelectCpuCount of RecoverConfiguration.cs " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            return true;
        }

        /*
         * IF user seelcted some memory for particular machine. When the control got lost the focus.
         * we will call this method to assign the value for the selected host.
         * We will check the MB/GB combobox and based on that selection we will assign that values.
         * Always we will convert them to the MB in case user has selected the GB also.
         * 
         */
        internal bool SelectedMemory(AllServersForm allServersForm)
        {
            try
            {
                if (allServersForm.memoryNumericUpDown.Value.ToString() != null)
                {
                    // _selectedHost.memSize = int.Parse(allServersForm.memoryNumericUpDown.Value.ToString());
                    if (allServersForm.memoryValuesComboBox.SelectedItem.ToString() == "MB")
                    {
                        if (allServersForm.memoryNumericUpDown.Value < 512)
                        {
                            MessageBox.Show("You have selected memory size less than 512MB. Minimum value is 512MB", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            _selectedHost.memSize = 512;
                            allServersForm.memoryNumericUpDown.Value = 512;
                        }
                        else
                        {
                            _selectedHost.memSize = int.Parse(allServersForm.memoryNumericUpDown.Value.ToString());
                        }
                    }
                    else
                    {
                        if (allServersForm.memoryNumericUpDown.Value > MAX_SIZE_IN_GB)
                        {
                            MessageBox.Show("You have selected memory size greater than " + MAX_SIZE_IN_GB.ToString() + ". Maximum value is " + MAX_SIZE_IN_GB, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            _selectedHost.memSize = MAX_SIZE_IN_GB * 1024;
                            allServersForm.memoryNumericUpDown.Value = MAX_SIZE_IN_GB;
                        }
                        else
                        {

                            _selectedHost.memSize = int.Parse(allServersForm.memoryNumericUpDown.Value.ToString()) * 1024;
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: SelectedMemory of RecoverConfiguration.cs " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            return true;
        }

        /*
        * In case of use for all settings selection for the hardware settigns.
        * Firstw e will check the memvalues and GB/MB drop down list is null or not.
        * If not null we will read that values and assign to all the hosts selected for the protection/recpvery.
        * In case of null we won't add any thing we will leave it.
        */
        internal bool SelectAllCheckBoxOfHardWareSetting(AllServersForm allServersForm)
        {
            try
            {
                if (allServersForm.hardWareSettingsCheckBox.Checked == true)
                {
                    if (allServersForm.memoryNumericUpDown.Value != 0 && allServersForm.cpuComboBox.SelectedItem != null)
                    {
                        foreach (Host h in allServersForm.recoveryChecksPassedList._hostList)
                        {
                            h.cpuCount = int.Parse(allServersForm.cpuComboBox.SelectedItem.ToString());
                            h.memSize = _selectedHost.memSize;

                        }
                    }
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: SelectAllCheckBoxOfHW of RecoverConfiguration.cs " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            return true;
        }


        internal bool UseDisplayNameForSingleVM(AllServersForm allServersForm)
        {
            try
            {
                _selectedHost.new_displayname = allServersForm.enterTargetDisplayNameTextBox.Text;
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: UseDisplayNameForSingleVM of RecoverConfiguration.cs " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            return true;
        }

        internal bool ApplyPreFixToSelectedVM(AllServersForm allServersForm)
        {
            try
            {
                foreach (Host h in allServersForm.vmsSelectedRecovery._hostList)
                {
                    if (_selectedHost.displayname == h.displayname)
                    {
                        _selectedHost.new_displayname = allServersForm.appliedPrefixTextBox.Text + "_" + h.new_displayname;
                    }
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: ApplyPreFixToSelectedVM of RecoverConfiguration.cs " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            return true;
        }

        internal bool ApplySuffixToSelectedVM(AllServersForm allServersForm)
        {
            try
            {
                foreach (Host h in allServersForm.vmsSelectedRecovery._hostList)
                {
                    if (_selectedHost.displayname == h.displayname)
                    {
                        _selectedHost.new_displayname = h.new_displayname + "_" + allServersForm.suffixTextBox.Text;
                    }
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: ApplySuffixToSelectedVM of RecoverConfiguration.cs " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            return true;
        }

        internal bool UseSameDisplayNameOnTarget(AllServersForm allServersForm)
        {
            try
            {
                if (allServersForm.keepSameAsSouceRadioButton.Checked == true)
                {
                    foreach (Host h in allServersForm.vmsSelectedRecovery._hostList)
                    {
                        foreach (Host h1 in allServersForm.recoveryChecksPassedList._hostList)
                        {
                            h1.new_displayname = h.new_displayname;
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: UseSameDisplayNameOnTarget of RecoverConfiguration.cs " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            return true;
        }
        internal bool KeepVMinDataStore(AllServersForm allServersForm)
        {
            _selectedHost.vmDirectoryPath = null;
            return true;
        }






        internal bool ShowAdvancedSettingForm(AllServersForm allServersForm)
        {
            try
            {
                if (allServersForm.appNameSelected == AppName.Drdrill)
                {
                    
                    sparceRetention sparce = new sparceRetention(ref _selectedHost, allServersForm.masterHostAdded,allServersForm.appNameSelected);
                    if (_selectedHost.cluster.ToUpper() == "YES")
                    {
                        sparce.folderNameGroupBox.Enabled = false;
                    }
                    sparce.ShowDialog();
                    if (sparce.canceled == false)
                    {
                        if (sparce.folderNameUseForAllCheckBox.Checked == true)
                        {
                            foreach (Host h in allServersForm.recoveryChecksPassedList._hostList)
                            {
                                h.vmDirectoryPath = sparce.hostSelected.vmDirectoryPath;
                            }
                        }
                        else
                        {
                            _selectedHost.vmDirectoryPath = sparce.hostSelected.vmDirectoryPath;
                        }
                    }
                    else
                    {
                        Trace.WriteLine(DateTime.Now + "\t User has cancelled the advanced pop up ");
                    }
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: ShowadvancedSettingForm " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            return true;
        }

    }
}
