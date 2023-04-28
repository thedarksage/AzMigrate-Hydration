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


namespace Com.Inmage.Wizard
{
    class ConfigurationPanelHandler:PanelHandler
    {
        internal static int NIC_NAME_COLUMN = 0;
        internal static int KEPP_OLD_VALUES_COLUMN = 1;
        internal static int CHANGE_VALUE_COLUMN = 2;
        internal static int SELECT_UNSELECT_NIC_COLUMN = 3;
        internal string _displayname = null;
        internal Host _selectedHost = new Host();
        internal Host _masterHost = new Host();
        internal static int MAX_SIZE_IN_MB = 0; // This is the variable that indicates the maximum size of the memory in MBs.
        internal static int MAX_SIZE_IN_GB = 0; // This is the variable that indicates the maximum size of the memory in GBs.
        internal string _latestFilePath = null;
        internal string _cacheSubnetmask = null;
        internal string _cacheGateway = null;
        internal string _cacheDns = null;
        internal override bool Initialize(AllServersForm allServersForm)
        {
            try
            {
                _latestFilePath = WinTools.FxAgentPath() + "\\vContinuum\\Latest";
                allServersForm.helpContentLabel.Text = HelpForcx.ProtectScreen6;
                //Filling the values into the memory and cpu comboboxes...
                Host masterHost = (Host)allServersForm.finalMasterListPrepared._hostList[0];
                _masterHost = masterHost;
               allServersForm.memoryValuesComboBox.Items.Clear();
               allServersForm.memoryValuesComboBox.Items.Add("GB");
               allServersForm.memoryValuesComboBox.Items.Add("MB");
                allServersForm.memoryNumericUpDown.Enabled = true;
                allServersForm.cpuComboBox.Enabled = true;
                allServersForm.memoryValuesComboBox.Enabled = true;
                allServersForm.hardWareSettingsCheckBox.Enabled = true;
                
                    foreach (Configurations config in masterHost.configurationList)
                    {
                        if (masterHost.vSpherehost != null)
                        {
                            if (config.vSpherehostname == masterHost.vSpherehost)
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
                        
                    
                }
                string esxIP = null;

                foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                {
                    esxIP = h.esxIp;
                }
                ////Making the nodes of tree view as clear for the fisrt time....
                allServersForm.sourceConfigurationTreeView.Nodes.Clear();
                ///writing the esx ip of the hosts as a parent nodes.
                ///In case of p2v esxip is not there so that we will check this....
                if (esxIP != null)
                {
                    allServersForm.sourceConfigurationTreeView.Nodes.Add("Source: " + esxIP);
                }
                else
                {
                    allServersForm.sourceConfigurationTreeView.Nodes.Add("Physical Machines");
                }
                ////writing the nodes to the parent nodes....
                foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                {
                    allServersForm.sourceConfigurationTreeView.Nodes[0].Nodes.Add(h.displayname).Text = h.displayname;
                }
                //expanding the tree view node....
                allServersForm.sourceConfigurationTreeView.ExpandAll();
                TreeNodeCollection nodes = allServersForm.sourceConfigurationTreeView.Nodes[0].Nodes;
                allServersForm.sourceConfigurationTreeView.SelectedNode = nodes[0];
                allServersForm.networkDataGridView.Rows.Clear();
                ////Selecting the first node as default and then displaying the nic into the datagridview.....
                int rowIndex = 0;
                Host tempHost = (Host)allServersForm.selectedSourceListByUser._hostList[0];
                _selectedHost = tempHost;
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
                        if (nicHash["new_ip"] != null)
                        {
                            allServersForm.networkDataGridView.Rows[rowIndex].Cells[KEPP_OLD_VALUES_COLUMN].Value = nicHash["new_ip"].ToString();
                        }
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
                    }
                    //allServersForm.networkDataGridView.Rows[rowIndex].Cells[CHANGE_VALUE_COLUMN].Value = "Change";
                    rowIndex++;
                }

                for (int i = 0; i < allServersForm.networkDataGridView.RowCount; i++)
                {
                    allServersForm.networkDataGridView.Rows[i].Cells[CHANGE_VALUE_COLUMN].Value = "Change";
                }
                //Filling the values of the cpu and memory of the first vm in the contorls of their fields...
                allServersForm.memoryValuesComboBox.SelectedItem = "MB";
                allServersForm.memoryNumericUpDown.Maximum = MAX_SIZE_IN_MB;
                if (tempHost.memSize < MAX_SIZE_IN_MB)
                {
                    allServersForm.memoryNumericUpDown.Value = tempHost.memSize;
                }
                else
                {
                    allServersForm.memoryNumericUpDown.Value = MAX_SIZE_IN_MB;
                }


                if (allServersForm.cpuComboBox.Items.Contains(tempHost.cpuCount))
                {
                    allServersForm.cpuComboBox.SelectedItem = tempHost.cpuCount;
                }

               
                

            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: Initialize of ConfigurationPanelHandler.cs " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            return true;
        }
        internal override bool ProcessPanel(AllServersForm allServersForm)
        {
            try
            {                
                if ( allServersForm.appNameSelected != AppName.Bmr && allServersForm.appNameSelected != AppName.Offlinesync)
                {
                    foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                    {
                        foreach (Host h1 in allServersForm.finalMasterListPrepared._hostList)
                        {
                            Trace.WriteLine(DateTime.Now + "\t Printing the vsphere names " + h.vSpherehost + "\t vsphere host " + h1.vSpherehost);
                            if (h.vSpherehost == h1.vSpherehost)
                            {
                                h.local_backup = true;
                                Trace.WriteLine(DateTime.Now + "\t Printing the displaynames " + h.new_displayname + "\t displayname " + h.displayname);
                                if (h.displayname == h.new_displayname || h.new_displayname == null)
                                {
                                    h.new_displayname = h.displayname + "_Local";
                                }
                            }
                        }
                    }               
                    //foreach (Host h in allServersForm._selectedSourceList._hostList)
                    //{
                    //    foreach (Host h1 in allServersForm._initialSourceList._hostList)
                    //    {
                    //        if (h.displayname != h1.displayname)
                    //        {
                    //            if (h.new_displayname == h1.new_displayname)
                    //            {
                    //                MessageBox.Show("Selected target displayname for VM: " + h.displayname + " is already there on target side.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    //                return false;
                    //            }
                    //        }
                    //    }
                    //}
                    foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                    {                        
                        foreach (Host masterHost in allServersForm.finalMasterListPrepared._hostList)
                        {
                            foreach (Host hostinInfoFile in allServersForm.initialSourceListFromXml._hostList)
                            {
                                if (h.vSpherehost == masterHost.vSpherehost)
                                {
                                    if (h.vSpherehost == hostinInfoFile.vSpherehost && h.displayname == hostinInfoFile.displayname)
                                    {
                                        //First assigning the old values because user may go back and forth for few times.
                                        ArrayList physicalDisks;
                                        ArrayList diskList;
                                        physicalDisks = h.disks.GetDiskList;
                                        diskList = hostinInfoFile.disks.GetDiskList;
                                        h.vmx_path = hostinInfoFile.vmx_path;
                                        h.folder_path = hostinInfoFile.folder_path;

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
                                                h.vmx_path = vmxPath[i] + "/";
                                            }
                                            else
                                            {
                                                h.vmx_path = h.vmx_path + vmxPath[i];
                                            }
                                        }
                                        foreach (Hashtable diskhash in diskList)
                                        {
                                            foreach (Hashtable disk in physicalDisks)
                                            {
                                                if (disk["scsi_mapping_host"].ToString() == diskhash["scsi_mapping_host"].ToString())
                                                {
                                                    disk["Name"] = diskhash["Name"].ToString();
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
                                                    foldername = foldername + folderpath[i];
                                                }
                                            }
                                            diskname[0] = foldername;
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
                                                }
                                            }
                                        }
                                    }

                                }
                            }
                        }
                    }
                }


                Host masterTargetHost = (Host)allServersForm.finalMasterListPrepared._hostList[0];
                foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                {
                    if (h.new_displayname == null)
                    {
                        h.new_displayname = h.displayname;
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
                    bool resourcePoolSelected = false;
                    foreach (ResourcePool resource in masterTargetHost.resourcePoolList)
                    {
                        if (resource.host == masterTargetHost.vSpherehost)
                        {
                            if (h.resourcePool != null)
                            {
                                if (h.resourcePool == resource.name && h.resourcepoolgrpname == resource.groupId)
                                {
                                    resourcePoolSelected = true;
                                    break;
                                }
                                else
                                {
                                    resourcePoolSelected = false;
                                }
                            }
                            else
                            {
                                h.resourcepoolgrpname = masterTargetHost.resourcepoolgrpname;
                                h.resourcePool = masterTargetHost.resourcePool;

                            }
                        }
                    }
                    if (resourcePoolSelected == false)
                    {
                        h.resourcepoolgrpname = masterTargetHost.resourcepoolgrpname;
                        h.resourcePool = masterTargetHost.resourcePool;
                    }

                }
                

                // When user clicks on the next button it will come here and checks for the memory values 
                // Does that values are divided by 4 or not if not we will remove that remainder to divisible by 4....
                foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
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
                    Host masterHost = (Host)allServersForm.finalMasterListPrepared._hostList[0];
                    try
                    {
                        foreach (Configurations configs in masterHost.configurationList)
                        {
                            if (h.cpuCount > configs.cpucount)
                            {
                                h.cpuCount = configs.cpucount;
                            }
                            if (masterHost.hostversion != null)
                            {
                                if (masterHost.hostversion.Contains("3.5") || (h.vmxversion != null && h.vmxversion.Contains("4")))
                                {
                                    if (h.cpuCount == 3)
                                    {
                                        Trace.WriteLine(DateTime.Now + "\t Making cpu count as 2 becuase target esx version is 3.5 and user has selected 3 as cpu count");
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
                    catch (Exception ex)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Faield to update cpus at the time of protection " + ex.Message);
                    }
                    foreach (Hashtable nicHash in h.nicList.list)
                    {
                        if (nicHash["new_network_name"] == null)
                        {
                            foreach (Network network in masterHost.networkNameList)
                            {
                                if (masterHost.vSpherehost == network.Vspherehostname)
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
                    }
                    foreach (Hashtable nicHash in h.nicList.list)
                    {
                        if (nicHash["new_network_name"] == null)
                        {
                            foreach (Network network in masterHost.networkNameList)
                            {
                                if (masterHost.vSpherehost == network.Vspherehostname)
                                {
                                    nicHash["new_network_name"] = network.Networkname;
                                    break;
                                }
                            }
                        }                       
                    }
                    bool selectedAtleastOneNic = false;
                    int i = 0;
                    foreach (Hashtable nicHash in h.nicList.list)
                    {
                        if (nicHash["Selected"].ToString() == "yes")
                        {
                            selectedAtleastOneNic = true;
                            i++;
                        }
                    }
                    if (selectedAtleastOneNic == false)
                    {
                        MessageBox.Show("Select atleast one nic for the VM: " + h.displayname + " to proceed", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return false;
                    }


                    if (i > 10)
                    {
                        MessageBox.Show("Machine:" + h.displayname + " has more than 10 NIC's. You need to select less than 10 NIC's for protection for each machine", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return false;
                    }

                }

                foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                {
                    if (h.cluster == "yes")
                    {
                        h.thinProvisioning = false;
                        Host sourceHost = new Host();
                        sourceHost = h;
                        CreateVirtualNicInHash(allServersForm, ref sourceHost);
                        h.nicList.list = sourceHost.nicList.list;
                    }
                }

                // Creating the esx.xml with the latest update values......like network connfiguration selected by user.....
                SolutionConfig config = new SolutionConfig();
                config.WriteXmlFile(allServersForm.selectedSourceListByUser, allServersForm.finalMasterListPrepared, allServersForm.targetEsxInfoXml, allServersForm.cxIPretrived, "ESX.xml", allServersForm.planInput, allServersForm.thinProvisionSelected);
                //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
                //based on above comment we have added xmlresolver as null
                XmlDocument document = new XmlDocument();
                document.XmlResolver = null;
                string xmlfile =_latestFilePath + "\\ESX.xml";

                //StreamReader reader = new StreamReader(xmlfile);
                //string xmlString = reader.ReadToEnd();

                XmlReaderSettings settings = new XmlReaderSettings();
                settings.ProhibitDtd = true;
                settings.XmlResolver = null;
                //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
                using (XmlReader reader1 = XmlReader.Create(xmlfile, settings))
                {
                    document.Load(reader1);
                    //reader1.Close();
                }
                //document.Load(xmlfile);
                //reader.Close();
                //XmlNodeList hostNodes = null;
                XmlNodeList rootNodes = null, hostNodes = null;
                rootNodes = document.GetElementsByTagName("root");
                bool isSourceNatted = false;
                bool isTargetNatted = false;


                foreach (XmlNode node in rootNodes)
                {                    
                    node.Attributes["cx_portnumber"].Value = WinTools.GetcxPortNumber();
                    if (Nat.CxnatIP != null)
                    {
                        node.Attributes["cx_nat_ip"].Value = Nat.CxnatIP;
                        node.Attributes["cx_ip"].Value = Nat.CxlocalIP;
                    }
                }
                document.Save(xmlfile);
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: Process of ConfigurationPanelHandler.cs " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            return true;
        }
        internal override bool ValidatePanel(AllServersForm allServersForm)
        {

            //If user selected any NAT configuration we are going to verify all selected process servers are having NAT IP or not.
            // If nat ip is not configured we are going to block in VM config screen.
            ArrayList processserverList = new ArrayList();
            Nat nat = new Nat();
            Nat.GetPSLocalNatIP(ref processserverList);
            foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
            {
                foreach (Hashtable hash in processserverList)
                {
                    if(hash["localip"] != null && h.processServerIp == hash["localip"].ToString())
                    {
                        Trace.WriteLine(DateTime.Now + "\t Comparing process server ips and local ip " + h.processServerIp + "\t local ip " + hash["localip"].ToString());
                        if (h.isSourceNatted == true || h.isTargetNatted == true)
                        {
                            if (hash["natip"] == null)
                            {
                                MessageBox.Show("Process server (" +h.processServerIp+ ") selected for the source server " + h.displayname + " is not having NAT IP configured. Please configure NAT IP using vContinuum to proceed","Error",MessageBoxButtons.OK,MessageBoxIcon.Error);
                                return false;
                            }
                            break;
                        }
                    }

                }
            }
            // This is to block user when selected same displaynames for Multiple DR VMS.
            //
            foreach(Host h in allServersForm.selectedSourceListByUser._hostList)
            {
                foreach (Host sourceHost in allServersForm.selectedSourceListByUser._hostList)
                {
                    //Need to check only when new_display name value is not null only.
                    if (h.new_displayname != null && sourceHost.new_displayname != null)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Comparing displaynames h" + h.displayname + "\t sourcehost " + sourceHost.displayname + "\t new displaynames " + h.new_displayname + "\t Source " + sourceHost.new_displayname);
                        if (h.displayname != sourceHost.displayname && h.new_displayname == sourceHost.new_displayname)
                        {
                            MessageBox.Show("You have chosen same display name on target for following machines" + Environment.NewLine + h.displayname + Environment.NewLine + sourceHost.displayname + Environment.NewLine +
                                             "Change different display name for one of the server and proceed.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            return false;
                        }
                    }
                }
            }

            return true;
        }

        internal override bool CanGoToNextPanel(AllServersForm allServersForm)
        {
            return true;
        }

        internal override bool CanGoToPreviousPanel(AllServersForm allServersForm)
        {
            allServersForm.mandatorydoubleFieldLabel.Visible = true;
            allServersForm.mandatoryLabel.Visible = true;
            return true;
        }

        internal bool SelectedVM(AllServersForm allServersForm, string displayname)
        {
            try
            {
                // This will be called when user selects particular vm we will show all the valuesin the datagridview and in
                // the memory values and cpu count values in the  corresponding gropu boxes....
                Trace.WriteLine(DateTime.Now + " \t printing the value of node " + displayname);
                Host h = new Host();
                h.displayname = displayname;
                _displayname = displayname;
                int index = 0;
                if (allServersForm.selectedSourceListByUser.DoesHostExist(h, ref  index) == true)
                {
                    h = (Host)allServersForm.selectedSourceListByUser._hostList[index];


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
                            allServersForm.networkDataGridView.Rows[rowIndex].Cells[CHANGE_VALUE_COLUMN].Value = "Change";
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
                            }
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
                    else
                    {
                        allServersForm.memoryNumericUpDown.Value = h.memSize;
                    }

                    if (allServersForm.cpuComboBox.Items.Contains(h.cpuCount))
                    {
                        allServersForm.cpuComboBox.SelectedItem = h.cpuCount;

                    }
                    allServersForm.netWorkConfiurationGroupBox.Visible = true;
                    allServersForm.hardWareGroupBox.Visible = true;
                    allServersForm.displayNameSettingsGroupBox.Visible = true;
                    allServersForm.enterTargetDisplaynameRadioButton.Checked = true;
                    allServersForm.natConfigGroupBox.Visible = true;
                    allServersForm.enterTargetDisplayNameTextBox.Text = h.new_displayname;
                    if (allServersForm.appNameSelected == AppName.Esx || allServersForm.appNameSelected == AppName.Bmr)
                    {
                        allServersForm.enterTargetDisplaynameRadioButton.Checked = true;
                        if (h.new_displayname != null)
                        {
                            allServersForm.enterTargetDisplayNameTextBox.Text = h.new_displayname;
                            allServersForm.appliedPrefixTextBox.Clear();
                            allServersForm.suffixTextBox.Clear();
                        }
                        else
                        {
                            allServersForm.enterTargetDisplayNameTextBox.Text = h.displayname;
                            allServersForm.appliedPrefixTextBox.Clear();
                            allServersForm.suffixTextBox.Clear();
                        }
                        
                        allServersForm.advancedSettingsLinkLabel.Visible = true;   

                    }
                    if (h.isTargetNatted == true)
                    {
                        allServersForm.psToTgtCheckBox.Checked = true;
                    }
                    else
                    {
                        allServersForm.psToTgtCheckBox.Checked = false;
                    }
                    if(h.isSourceNatted == true)
                    {
                        allServersForm.srcToPSCommunicationCheckBox.Checked = true;
                    }
                    else
                    {
                        allServersForm.srcToPSCommunicationCheckBox.Checked = false;
                    }
                    
                }
                else
                {
                    allServersForm.advancedSettingsLinkLabel.Visible = false;
                    allServersForm.netWorkConfiurationGroupBox.Visible = false;
                    allServersForm.hardWareGroupBox.Visible = false;
                    allServersForm.displayNameSettingsGroupBox.Visible = false;
                    allServersForm.natConfigGroupBox.Visible = false;
                   
                }
                
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: SelectedVM of ConfigurationPanelHandler.cs " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
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

        internal GroupBox[] groupBox;                                             //Added From
        internal RadioButton[] radioButton1;
        internal RadioButton[] radiobutton2;
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
                radiobutton2[i].Checked = true;
            }
        }

        protected void Checked_Changed2(Object sender, EventArgs e)
        {
            int i = nicForm.ipTabControl.SelectedIndex;

            if (radiobutton2[i].Checked == true)
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
                    Host masterHost = (Host)allServersForm.selectedMasterListbyUser._hostList[0];


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
                            if(hash["new_gateway"] != null)
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


                            groupBox = new GroupBox[ipcnt];
                            radioButton1 = new RadioButton[ipcnt];
                            radiobutton2 = new RadioButton[ipcnt];
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
                                gatewayslist = ip_split(gateways,gatewayCount);
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
                nicForm.ShowDialog();
                // Once the nic form is closed we will read the nic information into the particular vm info like data structures....
                if (nicForm.canceled == false)
                {
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
                                            hash["new_ip"] =  textBox1[c].Text ;
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
                                            if (c ==0)
                                            {
                                                hash["new_mask"] = textBox2[c].Text ;
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
                                            if (c ==0)
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

                                                hash["new_mask"] = hash["new_mask"] +","+ textBox2[c].Text;
                                            }
                                            else
                                            {

                                                hash["new_mask"] = hash["new_mask"] +","+ textBox2[c].Text;
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
                                if (nicForm.dhcp == true)
                                {
                                    hash["dhcp"] = "1";
                                }
                                else
                                {
                                    hash["dhcp"] = "0";
                                }
                                _cacheSubnetmask = nicForm.cacheSubNetmask;
                                _cacheGateway = nicForm.cacheGateway;
                                _cacheDns = nicForm.cacheDNS;
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
                foreach (Configurations config in _masterHost.configurationList)
                {
                    if (config.vSpherehostname == _masterHost.vSpherehost)
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
                // This will be called when the user selects the cpu values.....
                if (allServersForm.cpuComboBox.SelectedItem != null)
                {
                    _selectedHost.cpuCount = int.Parse(allServersForm.cpuComboBox.SelectedItem.ToString());
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: SelectCpuCount of ConfigurationPanelHandler.cs " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
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
                        foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
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


        internal bool UseSameDisplayNameOnTarget(AllServersForm allServersForm)
        {
            if (allServersForm.keepSameAsSouceRadioButton.Checked == true)
            {
                foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                {
                    h.new_displayname = h.displayname;
                }
            }
            return true;
        }

        internal bool UseDisplayNameForSingleVM(AllServersForm allServersForm)
        {
            _selectedHost.new_displayname = allServersForm.enterTargetDisplayNameTextBox.Text;
            return true;
        }

        internal bool ApplyPreFixToSelectedVM(AllServersForm allServersForm)
        {
            _selectedHost.new_displayname = allServersForm.appliedPrefixTextBox.Text + "_" + _selectedHost.displayname;
            return true;
        }

        internal bool ApplySuffixToSelectedVM(AllServersForm allServersForm)
        {
            _selectedHost.new_displayname = _selectedHost.displayname + "_" + allServersForm.suffixTextBox.Text;
            return true;
        }





        internal bool ShowAdvancedSettingForm(AllServersForm allServersForm)
        {
            try
            {
                if (allServersForm.appNameSelected == AppName.Esx || allServersForm.appNameSelected == AppName.Bmr || allServersForm.appNameSelected == AppName.Failback)
                {
                    Host masterHost = (Host)allServersForm.finalMasterListPrepared._hostList[0];
                    sparceRetention sparce = new sparceRetention(ref _selectedHost,masterHost,allServersForm.appNameSelected);
                    if (_selectedHost.vSan == true)
                    {
                        sparce.folderNameGroupBox.Enabled = false;
                    }
                    else
                    {
                        sparce.folderNameGroupBox.Enabled = true;
                    }
                    if (_selectedHost.cluster.ToUpper() == "YES")
                    {
                        sparce.folderNameGroupBox.Enabled = false;
                    }
                    sparce.ShowDialog();
                    if (sparce.canceled == false)
                    {
                        if (sparce.applyForProvisioningCheckBox.Checked == true)
                        {
                            foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                            {
                                if (sparce.thinProvisionRadioButton.Checked == true)
                                {
                                    h.thinProvisioning = true;
                                }
                                else
                                {
                                    h.thinProvisioning = false;
                                }
                            }
                        }
                        else
                        {
                            if (sparce.thinProvisionRadioButton.Checked == true)
                            {
                                _selectedHost.thinProvisioning = true;
                            }
                            else
                            {
                                _selectedHost.thinProvisioning = false;
                            }

                        }

                        if (sparce.compressionApplyAllcheckBox.Checked == true)
                        {
                            foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                            {
                                if (sparce.compressionRadioButton.Checked == true)
                                {
                                    if (sparce.cxBasedRadioButton.Checked == true)
                                    {
                                        h.cxbasedCompression = true;
                                        h.sourceBasedCompression = false;
                                        h.noCompression = false;
                                    }
                                    else if (sparce.sourceBasedRadioButton.Checked == true)
                                    {
                                        h.sourceBasedCompression = true;
                                        h.cxbasedCompression = false;
                                        h.noCompression = false;
                                    }
                                }
                                if (sparce.noCompressionRadioButton.Checked == true)
                                {
                                    h.sourceBasedCompression = false;
                                    h.cxbasedCompression = false;
                                    h.noCompression = true;
                                }
                            }
                        }
                        else
                        {
                            if (sparce.compressionRadioButton.Checked == true)
                            {
                                if (sparce.cxBasedRadioButton.Checked == true)
                                {
                                    _selectedHost.cxbasedCompression = true;
                                    _selectedHost.sourceBasedCompression = false;
                                    _selectedHost.noCompression = false;
                                }
                                else if (sparce.sourceBasedRadioButton.Checked == true)
                                {
                                    _selectedHost.sourceBasedCompression = true;
                                    _selectedHost.cxbasedCompression = false;
                                    _selectedHost.noCompression = false;
                                }
                            }
                            else if (sparce.noCompressionRadioButton.Checked == true)
                            {
                                _selectedHost.sourceBasedCompression = false;
                                _selectedHost.cxbasedCompression = false;
                                _selectedHost.noCompression = true;
                            }

                        }

                        if (sparce.applyResourcePoolForAllHostsCheckBox.Checked == true)
                        {
                            foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                            {                           

                                h.resourcePool = sparce.resourcePoolComboBox.Text.ToString();
                                foreach (ResourcePool resource in masterHost.resourcePoolList)
                                {
                                    if (h.resourcePool != "Default")
                                    {
                                        if (h.resourcePool == resource.name && masterHost.vSpherehost == resource.host)
                                        {
                                            h.resourcepoolgrpname = resource.groupId;
                                        }
                                    }
                                    else
                                    {
                                        h.resourcePool = "Resources";
                                        if (h.resourcePool == resource.name && masterHost.vSpherehost == resource.host)
                                        {
                                            h.resourcepoolgrpname = resource.groupId;
                                            break;
                                        }
                                    }
                                } 
                            }
                        }
                        else
                        {
                            _selectedHost.resourcePool = sparce.resourcePoolComboBox.Text.ToString();
                            foreach (ResourcePool resource in masterHost.resourcePoolList)
                            {
                                if (_selectedHost.resourcePool != "Default")
                                {
                                    if (_selectedHost.resourcePool == resource.name && masterHost.vSpherehost == resource.host)
                                    {
                                        _selectedHost.resourcepoolgrpname = resource.groupId;
                                    }
                                }
                                else
                                {
                                    _selectedHost.resourcePool = "Resources";
                                    if (_selectedHost.resourcePool == resource.name && masterHost.vSpherehost == resource.host)
                                    {
                                        _selectedHost.resourcepoolgrpname = resource.groupId;
                                        break;
                                    }
                                }
                            }
                        }
                        
                        if (sparce.applyEncryptionForAllHostsCheckBox.Checked == true)
                        {
                            foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                            {
                                if (sparce.secureFromSourceToCxCheckBox.Checked == true)
                                {
                                    h.secure_src_to_ps = true;
                                }
                                else
                                {
                                    h.secure_src_to_ps = false;
                                }
                                if (sparce.secureFromCXtoDestinationCheckBox.Checked == true)
                                {
                                    h.secure_ps_to_tgt = true;
                                }
                                else
                                {
                                    h.secure_ps_to_tgt = false;
                                }
                            }
                        }
                        else
                        {
                            if (sparce.secureFromSourceToCxCheckBox.Checked == true)
                            {
                                _selectedHost.secure_src_to_ps = true;
                            }
                            else
                            {
                                _selectedHost.secure_src_to_ps = false;
                            }
                            if (sparce.secureFromCXtoDestinationCheckBox.Checked == true)
                            {
                                _selectedHost.secure_ps_to_tgt = true;

                            }
                            else
                            {
                                _selectedHost.secure_ps_to_tgt = false;
                            }
                        }
                        if (sparce.folderNameUseForAllCheckBox.Checked == true)
                        {
                            foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                            {
                                if (h.cluster == "no")
                                {
                                    h.vmDirectoryPath = sparce.hostSelected.vmDirectoryPath;
                                }
                            }
                        }
                        else
                        {
                            _selectedHost.vmDirectoryPath = sparce.hostSelected.vmDirectoryPath;
                        }

                        if (sparce.sparceApplyForAllCheckBox.Checked == true)
                        {
                            foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                            {
                                h.unitsofTimeIntervelWindow = sparce.hostSelected.unitsofTimeIntervelWindow;
                                h.unitsofTimeIntervelWindow1Value = sparce.hostSelected.unitsofTimeIntervelWindow1Value;
                                h.unitsofTimeIntervelWindow2Value = sparce.hostSelected.unitsofTimeIntervelWindow2Value;
                                h.unitsofTimeIntervelWindow3Value = sparce.hostSelected.unitsofTimeIntervelWindow3Value;
                                h.bookMarkCountWindow1 = sparce.hostSelected.bookMarkCountWindow1;
                                h.bookMarkCountWindow2 = sparce.hostSelected.bookMarkCountWindow2;
                                h.bookMarkCountWindow3 = sparce.hostSelected.bookMarkCountWindow3;
                                h.retentionInDays = sparce.hostSelected.retentionInDays;
                                h.selectedSparce = sparce.hostSelected.selectedSparce;
                                h.sparceDaysSelected = sparce.hostSelected.sparceDaysSelected;
                                h.sparceWeeksSelected = sparce.hostSelected.sparceWeeksSelected;
                                h.sparceMonthsSelected = sparce.hostSelected.sparceMonthsSelected;
                                if (sparce.hostSelected.retentionInHours != 0)
                                {
                                    h.retentionInHours = sparce.hostSelected.retentionInHours;

                                    Trace.WriteLine(DateTime.Now + "\t selected retention in hours " + h.retentionInHours.ToString());

                                }
                            }

                        }
                        else
                        {
                            _selectedHost.selectedSparce = sparce.hostSelected.selectedSparce;
                            _selectedHost.sparceDaysSelected = sparce.hostSelected.sparceDaysSelected;
                            _selectedHost.sparceWeeksSelected = sparce.hostSelected.sparceWeeksSelected;
                            _selectedHost.sparceMonthsSelected = sparce.hostSelected.sparceMonthsSelected;
                            _selectedHost.unitsofTimeIntervelWindow = sparce.hostSelected.unitsofTimeIntervelWindow;
                            _selectedHost.unitsofTimeIntervelWindow1Value = sparce.hostSelected.unitsofTimeIntervelWindow1Value;
                            _selectedHost.unitsofTimeIntervelWindow2Value = sparce.hostSelected.unitsofTimeIntervelWindow2Value;
                            _selectedHost.unitsofTimeIntervelWindow3Value = sparce.hostSelected.unitsofTimeIntervelWindow3Value;
                            _selectedHost.bookMarkCountWindow1 = sparce.hostSelected.bookMarkCountWindow1;
                            _selectedHost.bookMarkCountWindow2 = sparce.hostSelected.bookMarkCountWindow2;
                            _selectedHost.bookMarkCountWindow3 = sparce.hostSelected.bookMarkCountWindow3;
                            _selectedHost.retentionInDays = sparce.hostSelected.retentionInDays;
                            if (sparce.hostSelected.retentionInHours != 0)
                            {
                                _selectedHost.retentionInHours = sparce.hostSelected.retentionInHours;
                                Trace.WriteLine(DateTime.Now + "\t selected retention in hours " + _selectedHost.retentionInHours.ToString());
                            }
                            

                        }
                    }
                    else
                    {
                        Trace.WriteLine(DateTime.Now + "\t User has canceled the advanced settings form");
                    }
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: ShowFolderNameGroupBox of ConfigurationPanelHandler.cs " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
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
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: CreateVirtualNicInHash of ConfigurationPanelHandler.cs " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            return true;
        }



        internal bool SrcToPSCommunication(AllServersForm allServersFrom)
        {
            if (allServersFrom.srcToPSCommunicationCheckBox.Checked == true)
            {
                _selectedHost.isSourceNatted = true;
            }
            else
            {
                _selectedHost.isSourceNatted = false;
            }
            return true;
        }

        internal bool PsToTargetCommunication(AllServersForm allServersFrom)
        {
            if (allServersFrom.psToTgtCheckBox.Checked == true)
            {
                _selectedHost.isTargetNatted = true;
            }
            else
            {
                _selectedHost.isTargetNatted = false;
            }
            return true;
        }

        internal bool NatSettingsUseForAll(AllServersForm allServersform)
        {
            foreach (Host h in allServersform.selectedSourceListByUser._hostList)
            {
                if (allServersform.srcToPSCommunicationCheckBox.Checked == true)
                {
                    h.isSourceNatted = true;
                }
                else
                {
                    h.isSourceNatted = false;
                }

                if (allServersform.psToTgtCheckBox.Checked == true)
                {
                    h.isTargetNatted = true;
                }
                else
                {
                    h.isTargetNatted = false;
                }
            }
            return true;
        }
    }
}
