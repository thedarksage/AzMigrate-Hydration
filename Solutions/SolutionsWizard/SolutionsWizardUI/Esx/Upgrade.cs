using System;
using System.Collections.Generic;
using System.Collections;
using System.IO;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;
using System.Diagnostics;
using Com.Inmage.Esxcalls;
using Com.Inmage.Tools;
using System.Xml;

namespace Com.Inmage.Wizard
{
    public partial class Upgrade : Form
    {
        HostList sourceList = new HostList();
        HostList masterList = new HostList();
        ArrayList sourceEsxIPList = new ArrayList();
        ArrayList targetEsxIPList = new ArrayList();
        ArrayList sourceUpdatedList = new ArrayList();
        ArrayList targetUpdatedList = new ArrayList();
        HostList listFromInfoFile = new HostList();
        string esxip = null;
        string cxip = null;
        string ip = null;
        string username = null;
        string password = null;
        HostList sourceESXInfoList = new HostList();
        HostList targetESXInfoList = new HostList();
        bool _sourceUpdate = false;
        bool _targetUpdate = false;
        string selectedSoruceIP = null;
        string selectedTargetIP = null;
        internal bool slideOpen = false;
        internal string Upgraded = "11";
        Esx esxInfo = new Esx();       
        string latestPath = null;
        string vConVersionNumber = null;
        string masterConfigFile = "MasterConfigFile.xml";
        internal string installedPath = null;
        public Upgrade()
        {
            InitializeComponent();
            versionLabel.Text = HelpForcx.VersionNumber;
            helpContentLabel.Text = HelpForcx.Upgrade;
            if (File.Exists(WinTools.FxAgentPath() + "\\vContinuum\\patch.log"))
            {
                patchLabel.Visible = true;
            }
            latestPath = WinTools.FxAgentPath() + "\\vContinuum\\Latest";
            installedPath = WinTools.FxAgentPath() + "\\vContinuum";
           /* MasterConfigFile master = new MasterConfigFile();
            master.DownloadMasterConfigFile(WinTools.GetcxIPWithPortNumber());*/

            if (File.Exists(WinTools.FxAgentPath() + "\\vContinuum\\Latest\\MasterConfigFile.xml"))
            {
                SolutionConfig sol = new SolutionConfig();
                sol.ReadXmlConfigFileWithMasterList("MasterConfigFile.xml",  sourceList,  masterList,  esxip,  cxip);
                sourceList = SolutionConfig.GetHostList;
                masterList = SolutionConfig.GetMasterList;
                esxip = sol.GetEsxIP;
                cxip = sol.GetcxIP;
            }
            else
            {
                MessageBox.Show("There are no plan in cx configuration file", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);              
            }
            //Checking for the source esx ips and then we are filling the drop down list.
            foreach (Host h in sourceList._hostList)
            {
                if (h.vCenterProtection == "No")
                {
                    if (sourceEsxIPList.Count == 0)
                    {
                        sourceEsxIPList.Add(h.esxIp);
                    }
                    else
                    {
                        bool ipExists = false;
                        foreach (string s in sourceEsxIPList)
                        {
                            if (s == h.esxIp)
                            {
                                ipExists = true;
                                break;
                            }
                        }
                        if (ipExists == false)
                        {
                            sourceEsxIPList.Add(h.esxIp);
                        }
                    }
                }
            }
            if (sourceEsxIPList.Count != 0)
            {
                foreach (string s in sourceEsxIPList)
                {
                    sourceServersComboBox.Items.Add(s);

                }
                sourceServersComboBox.SelectedItem = sourceServersComboBox.Items[0].ToString();

                sourcevCenterIPTextBox.Text = sourceServersComboBox.Items[0].ToString();
                sourcevCenterIPTextBox.Enabled = false;
            }
            Trace.WriteLine(DateTime.Now + "\t Printing the count of the masterlist " + masterList._hostList.Count.ToString());
            foreach (Host h in masterList._hostList)
            {
                if (h.vCenterProtection == "No")
                {
                    if (targetEsxIPList.Count == 0)
                    {
                        targetEsxIPList.Add(h.esxIp);
                    }
                    else
                    {
                        bool ipExists = false;
                        foreach (string s in targetEsxIPList)
                        {
                            if (s == h.esxIp)
                            {
                                ipExists = true;
                                break;
                            }
                        }
                        if (ipExists == false)
                        {
                            targetEsxIPList.Add(h.esxIp);
                        }
                    }
                }
            }
            if (targetEsxIPList.Count != 0)
            {
                foreach (string s in targetEsxIPList)
                {

                    selectTargetvSphereComboBox.Items.Add(s);

                }
                selectTargetvSphereComboBox.SelectedItem = selectTargetvSphereComboBox.Items[0].ToString();
                enterTargetIPTextBox.Enabled = false;
                enterTargetIPTextBox.Text = selectTargetvSphereComboBox.Items[0].ToString();
            }
            string versionNumber = WinTools.VersionNumber();

           vConVersionNumber =   versionNumber.Substring(0, 3);
            
        }

        private void cancelButton_Click(object sender, EventArgs e)
        {    
            Close();
        }

        private void sourcevCenterCheckBox_CheckedChanged(object sender, EventArgs e)
        {
            if (sourcevCenterCheckBox.Checked == true)
            {
                if (sourcevCenterIPTextBox.Text.Length != 0)
                {
                    foreach (string s in sourceEsxIPList)
                    {
                        if (s == sourcevCenterIPTextBox.Text)
                        {
                            sourcevCenterIPTextBox.Text = "";
                            sourcevCenterIPTextBox.Enabled = true;
                        }
                    }
                }
            }
            else
            {
                if (sourceServersComboBox.Items.Count != 0 && sourceServersComboBox.SelectedItem != null)
                {
                    sourcevCenterIPTextBox.Text = sourceServersComboBox.SelectedItem.ToString();
                    sourcevCenterIPTextBox.Enabled = false;
                }
            }
        }

        private void doesTargetESXConnectedTovCenterCheckBox_CheckedChanged(object sender, EventArgs e)
        {
            if (doesTargetESXConnectedTovCenterCheckBox.Checked == true)
            {
                if (enterTargetIPTextBox.Text.Length != 0)
                {
                    foreach (string s in targetEsxIPList)
                    {
                        enterTargetIPTextBox.Enabled = true;
                        enterTargetIPTextBox.Text = "";
                    }
                }
            }
            else
            {
                if (selectTargetvSphereComboBox.Items.Count != 0 && selectTargetvSphereComboBox.SelectedItem != null)
                {
                    enterTargetIPTextBox.Text = selectTargetvSphereComboBox.SelectedItem.ToString();
                    enterTargetIPTextBox.Enabled = false;
                }
            }
        }

        private void sourceUpdateToLatestButton_Click(object sender, EventArgs e)
        {
            if (sourcevCenterIPTextBox.Text.Length == 0)
            {
                MessageBox.Show("Enter vCenter/vSphere IP to continue", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }
            if (sourceUsernameTextBox.Text.Length == 0)
            {
                MessageBox.Show("Enter username to continue", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }
            if (sourcePasswordTextBox.Text.Length == 0)
            {
                MessageBox.Show("Enter password to continue", "Error",MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }

            ip = sourcevCenterIPTextBox.Text.Trim().ToString();
            username = sourceUsernameTextBox.Text.Trim().ToString();
            password = sourcePasswordTextBox.Text.Trim().ToString();
           
            _sourceUpdate = true;
            _targetUpdate = false;
            if (sourceServersComboBox.Items.Count != 0)
            {
                progressBar.Visible = true;
                selectedSoruceIP = sourceServersComboBox.SelectedItem.ToString();
                upgadePanel.Enabled = false;
                getDetailsBackgroundWorker.RunWorkerAsync();
            }

        }

        private void targetUpgradeButton_Click(object sender, EventArgs e)
        {
            if (enterTargetIPTextBox.Text.Length == 0)
            {
                MessageBox.Show("Enter vCenter/vSphere IP to continue", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }
            if (targetUsernameTextBox.Text.Length == 0)
            {
                MessageBox.Show("Enter username to continue", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }
            if (targetPasswordTextBox.Text.Length == 0)
            {
                MessageBox.Show("Enter password to continue", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }
            ip = enterTargetIPTextBox.Text.Trim().ToString();
            username = targetUsernameTextBox.Text.Trim().ToString();
            password = targetPasswordTextBox.Text.Trim().ToString();
           
            if (selectTargetvSphereComboBox.Items.Count != 0)
            {
                progressBar.Visible = true;
                selectedTargetIP = selectTargetvSphereComboBox.SelectedItem.ToString();
                _targetUpdate = true;
                _sourceUpdate = false;
                upgadePanel.Enabled = false;
                getDetailsBackgroundWorker.RunWorkerAsync();
            }
            
        }
        // first we will get the info.xml by the credentials given by the user..
        // For both windows and linux..
        //after that we will compre whether they are in vcenterprotection or not
        // after that we will check for the values uuid is null or not
        //If uuid is null we will go by displaynames and hostnames...
        // for each primary host we will add the nodes like vspherehostname, target uuid, mt uuid and what ever required.
        private void getDetailsBackgroundWorker_DoWork(object sender, DoWorkEventArgs e)
        {
            try
            {
                sourceESXInfoList = new HostList();
                targetESXInfoList = new HostList();
                esxInfo = new Esx();
                Cxapicalls cxAPi = new Cxapicalls();
                cxAPi. UpdaterCredentialsToCX(ip, username, password);
                if (esxInfo.GetEsxInfo(ip, Role.Primary, OStype.Windows) == 0)
                {
                    foreach (Host h in esxInfo.GetHostList)
                    {
                        sourceESXInfoList.AddOrReplaceHost(h);
                        targetESXInfoList.AddOrReplaceHost(h);
                    }
                    if (esxInfo.GetEsxInfo(ip, Role.Primary, OStype.Linux) == 0)
                    {
                        foreach (Host h in esxInfo.GetHostList)
                        {
                            sourceESXInfoList.AddOrReplaceHost(h);
                            targetESXInfoList.AddOrReplaceHost(h);
                        }
                    }
                    //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
                    //based on above comment we have added xmlresolver as null
                    XmlDocument documentMasterEsx = new XmlDocument();
                    documentMasterEsx.XmlResolver = null;
                    string esxMasterXmlFile = latestPath + "\\" + masterConfigFile;
                 //   StreamReader reader = new StreamReader(esxMasterXmlFile);
                   // string xmlString = reader.ReadToEnd();
                    //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
                    XmlReaderSettings settings = new XmlReaderSettings();
                    settings.ProhibitDtd = true;
                    settings.XmlResolver = null;
                    using (XmlReader reader1 = XmlReader.Create(esxMasterXmlFile, settings))
                    {
                        documentMasterEsx.Load(reader1);
                        //reader1.Close();
                    }
                   // documentMasterEsx.Load(esxMasterXmlFile);
                    //reader.Close();
                    XmlNodeList hostNodesEsxMasterXml = null;
                    hostNodesEsxMasterXml = documentMasterEsx.GetElementsByTagName("host");
                    foreach (XmlNode esxMasternode in hostNodesEsxMasterXml)
                    {
                        foreach (Host h1 in sourceESXInfoList._hostList)
                        {
                            if (h1.vCenterProtection == "Yes" && h1.vSpherehost == esxMasternode.Attributes["esxIp"].Value && esxMasternode.Attributes["role"].Value == Role.Primary.ToString())
                            {
                                if (esxMasternode.Attributes["source_uuid"] != null && esxMasternode.Attributes["source_uuid"].Value.Length != 0)
                                {
                                    if (esxMasternode.Attributes["source_uuid"].Value == h1.source_uuid)
                                    {
                                        if (_targetUpdate == true && esxMasternode.Attributes["role"].Value == Role.Secondary.ToString())
                                        {
                                            CheckVersionOfHostNodes(esxMasternode, h1);
                                        }
                                        else if (_sourceUpdate == true && esxMasternode.Attributes["role"].Value == Role.Primary.ToString())
                                        {
                                            CheckVersionOfHostNodes(esxMasternode, h1);
                                        }
                                    }
                                }
                                else if (esxMasternode.Attributes["display_name"].Value == h1.displayname || esxMasternode.Attributes["hostname"].Value == h1.hostname)
                                {
                                    if (_targetUpdate == true && esxMasternode.Attributes["role"].Value == Role.Secondary.ToString())
                                    {
                                        CheckVersionOfHostNodes(esxMasternode, h1);
                                    }
                                    else if (_sourceUpdate == true && esxMasternode.Attributes["role"].Value == Role.Primary.ToString())
                                    {
                                        CheckVersionOfHostNodes(esxMasternode, h1);
                                    }
                                }
                            }
                            else
                            {
                                if (esxMasternode.Attributes["source_uuid"] != null && esxMasternode.Attributes["source_uuid"].Value.Length != 0)
                                {
                                    if (esxMasternode.Attributes["source_uuid"].Value == h1.source_uuid)
                                    {
                                        if (_targetUpdate == true && esxMasternode.Attributes["role"].Value == Role.Secondary.ToString())
                                        {
                                            CheckVersionOfHostNodes(esxMasternode, h1);
                                        }
                                        else if (_sourceUpdate == true && esxMasternode.Attributes["role"].Value == Role.Primary.ToString())
                                        {
                                            CheckVersionOfHostNodes(esxMasternode, h1);
                                        }
                                    }
                                }
                                else if (esxMasternode.Attributes["display_name"].Value == h1.displayname || esxMasternode.Attributes["hostname"].Value == h1.hostname)
                                {
                                    if (_targetUpdate == true && esxMasternode.Attributes["role"].Value == Role.Secondary.ToString())
                                    {
                                        CheckVersionOfHostNodes(esxMasternode, h1);
                                    }
                                    else if (_sourceUpdate == true && esxMasternode.Attributes["role"].Value == Role.Primary.ToString())
                                    {
                                        CheckVersionOfHostNodes(esxMasternode, h1);
                                        try
                                        {
                                            bool nicsExistsForVm = false;
                                            if (esxMasternode.HasChildNodes)
                                            {
                                                // CHILD NODES  can be disks, nics, consistency, processerver.
                                                XmlNodeList childNodes = esxMasternode.ChildNodes;
                                                foreach (XmlNode childNode in childNodes)
                                                {
                                                    if (childNode.Name == "nic")
                                                    {
                                                        nicsExistsForVm = true;
                                                    }
                                                }
                                            }

                                            if (nicsExistsForVm == false)
                                            {


                                                if (h1.nicHash.Count != 0)
                                                {

                                                    foreach (Hashtable nic in h1.nicList.list)
                                                    {

                                                        XmlElement elmnic = documentMasterEsx.DocumentElement;
                                                        XmlElement elmNew = documentMasterEsx.CreateElement("nic");
                                                        if (nic["network_label"] != null)
                                                        {
                                                            elmNew.SetAttribute("network_label", nic["network_label"].ToString());
                                                        }
                                                        else
                                                        {
                                                            elmNew.SetAttribute("network_label", null);
                                                        }
                                                        if (nic["network_name"] != null)
                                                        {
                                                            elmNew.SetAttribute("network_name", nic["network_name"].ToString());
                                                        }
                                                        else
                                                        {
                                                            elmNew.SetAttribute("network_name", null);
                                                        }
                                                        if (nic["nic_mac"] != null)
                                                        {
                                                            elmNew.SetAttribute("nic_mac", nic["nic_mac"].ToString());
                                                        }
                                                        else
                                                        {
                                                            elmNew.SetAttribute("nic_mac", null);
                                                        }
                                                        if (nic["ip"] != null)
                                                        {
                                                            elmNew.SetAttribute("ip", nic["ip"].ToString());
                                                        }
                                                        else
                                                        {
                                                            elmNew.SetAttribute("ip", null);
                                                        }
                                                        if (nic["dhcp"] != null)
                                                        {
                                                            elmNew.SetAttribute("dhcp", nic["dhcp"].ToString());
                                                        }
                                                        else
                                                        {
                                                            elmNew.SetAttribute("dhcp", null);
                                                        }
                                                        if (nic["mask"] != null)
                                                        {
                                                            elmNew.SetAttribute("mask", nic["mask"].ToString());
                                                        }
                                                        else
                                                        {
                                                            elmNew.SetAttribute("mask", null);
                                                        }
                                                        if (nic["gateway"] != null)
                                                        {
                                                            elmNew.SetAttribute("gateway", nic["gateway"].ToString());
                                                        }
                                                        else
                                                        {
                                                            elmNew.SetAttribute("gateway", null);
                                                        }
                                                        if (nic["dnsip"] != null)
                                                        {
                                                            elmNew.SetAttribute("dnsip", nic["dnsip"].ToString());
                                                        }
                                                        else
                                                        {
                                                            elmNew.SetAttribute("dnsip", null);
                                                        }
                                                        if (nic["address_type"] != null)
                                                        {
                                                            elmNew.SetAttribute("address_type", nic["address_type"].ToString());
                                                        }
                                                        else
                                                        {
                                                            elmNew.SetAttribute("address_type", "generated");
                                                        }
                                                        if (nic["adapter_type"] != null)
                                                        {
                                                            elmNew.SetAttribute("adapter_type", nic["adapter_type"].ToString());

                                                        }
                                                        else
                                                        {
                                                            elmNew.SetAttribute("adapter_type", "VirtualE1000");
                                                        }
                                                        elmNew.SetAttribute("new_ip", null);
                                                        elmNew.SetAttribute("new_mask", null);
                                                        elmNew.SetAttribute("new_dnsip", null);
                                                        elmNew.SetAttribute("new_gateway", null);
                                                        elmNew.SetAttribute("new_network_name", null);
                                                        elmNew.SetAttribute("new_macaddress", null);
                                                        esxMasternode.AppendChild(elmNew);
                                                    }
                                                }
                                            }
                                        }
                                        catch (Exception ex)
                                        {
                                            Trace.WriteLine(DateTime.Now + "\t While updating the nics we got exception " + ex.Message);
                                        }
                                    }

                                }
                            }



                            if (esxMasternode.Attributes["role"].Value == Role.Primary.ToString())
                            {
                                if (esxMasternode.Attributes["mastertargetdisplayname"].Value == h1.displayname || esxMasternode.Attributes["mastertargethostname"].Value == h1.hostname.Split('.')[0])
                                {
                                    esxMasternode.Attributes["targetesxip"].Value = ip;
                                    
                                    if (esxMasternode.Attributes["targetvSphereHostName"] == null)
                                    {
                                        XmlElement ele = (XmlElement)esxMasternode;
                                        ele.SetAttribute("targetvSphereHostName", h1.vSpherehost);
                                    }
                                    else
                                    {
                                        esxMasternode.Attributes["targetvSphereHostName"].Value = h1.vSpherehost;
                                    }

                                    if (esxMasternode.Attributes["mt_uuid"] == null)
                                    {
                                        XmlElement ele = (XmlElement)esxMasternode;
                                        ele.SetAttribute("mt_uuid", h1.source_uuid);
                                    }
                                    else
                                    {
                                        esxMasternode.Attributes["mt_uuid"].Value = h1.source_uuid;
                                    }
                                }
                                if (_targetUpdate == true)
                                {
                                    if (esxMasternode.Attributes["target_uuid"] == null || esxMasternode.Attributes["target_uuid"].Value.Length == 0)
                                    {
                                        if (esxMasternode.Attributes["display_name"] != null && esxMasternode.Attributes["display_name"].Value == h1.displayname)
                                        {
                                            if (esxMasternode.Attributes["target_uuid"] == null)
                                            {
                                                XmlElement ele = (XmlElement)esxMasternode;
                                                ele.SetAttribute("target_uuid", h1.source_uuid);
                                            }
                                            else
                                            {
                                                esxMasternode.Attributes["target_uuid"].Value = h1.source_uuid;
                                            }
                                        }
                                    }
                                }


                            }
                        }
                    }


                    documentMasterEsx.Save(esxMasterXmlFile);
                    // MasterConfigFile master = new MasterConfigFile();
                    //master.UploadMasterConfigFile(WinTools.GetcxIPWithPortNumber());
                    if (selectedSoruceIP != null)
                    {
                        sourceUpdatedList.Add(selectedSoruceIP);

                    }
                    if (selectedTargetIP != null)
                    {
                        targetUpdatedList.Add(selectedTargetIP);
                        sourceUpdatedList.Add(selectedSoruceIP);
                    }

                }
                else
                {
                    MessageBox.Show("Failed to get the details from the select ip ", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return;
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

        // This will check the vconversion in the hostnode and if it is 3.0 protection. And we will check whether it is vCneter Protection or not
        // If it is vCenterProtection then we are not going edit file.
        // If vCenterProtection is No in xml file and now user selected to upgrade to vCenter we will upgrade to vCenter.
        // If vConversion is null we will upgrade to lastest with out checking any thing.
        private bool CheckVersionOfHostNodes(XmlNode esxMasternode, Host h1)
        {
            try
            {
                if (esxMasternode.Attributes["vConversion"] != null && esxMasternode.Attributes["vConversion"].Value.Length != 0 && esxMasternode.Attributes["vCenterProtection"] != null && esxMasternode.Attributes["vCenterProtection"].Value.Length != 0)
                {
                    if (h1.vCenterProtection == "Yes" && esxMasternode.Attributes["vCenterProtection"].Value == "No")
                    {
                        SetNodes(esxMasternode, h1);
                    }
                }
                else
                {
                    SetNodes(esxMasternode, h1);
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
            return true;
        }


        private void getDetailsBackgroundWorker_ProgressChanged(object sender, ProgressChangedEventArgs e)
        {
            progressBar.Value = e.ProgressPercentage;
        }

        private void getDetailsBackgroundWorker_RunWorkerCompleted(object sender, RunWorkerCompletedEventArgs e)
        {
            sourceServersComboBox.Items.Clear();
            selectTargetvSphereComboBox.Items.Clear();
            foreach (string s in sourceUpdatedList)
            {
                sourceEsxIPList.Remove(s);
            }
            if (_targetUpdate == true)
            {
                foreach (string s in targetUpdatedList)
                {
                    targetEsxIPList.Remove(s);
                }
            }
            if (sourceEsxIPList.Count != 0)
            {
                foreach (string s in sourceEsxIPList)
                {
                    sourceServersComboBox.Items.Add(s);

                }
                sourceServersComboBox.SelectedItem = sourceServersComboBox.Items[0].ToString();
                sourcevCenterIPTextBox.Text = sourceServersComboBox.Items[0].ToString();
                sourceUpdateToLatestButton.Enabled = true;
                sourceServersComboBox.Enabled = true;
            }
            else
            {
                sourceServersComboBox.Enabled = false;
                sourceUpdateToLatestButton.Enabled = false;                
            }
            if (targetEsxIPList.Count != 0)
            {
                foreach (string s in targetEsxIPList)
                {
                    selectTargetvSphereComboBox.Items.Add(s);
                }
                selectTargetvSphereComboBox.SelectedItem = selectTargetvSphereComboBox.Items[0].ToString();
                enterTargetIPTextBox.Text = selectTargetvSphereComboBox.Items[0].ToString();
                targetUpgradeButton.Enabled = true;
                selectTargetvSphereComboBox.Enabled = true;
            }
            else
            {
                selectTargetvSphereComboBox.Enabled = false;
                targetUpgradeButton.Enabled = false;               
            }
            _sourceUpdate = false;
            _targetUpdate = false;
            selectedTargetIP = null;
            selectedSoruceIP = null;
            sourceUpdatedList = new ArrayList();
            targetUpdatedList = new ArrayList();
            upgadePanel.Enabled = true;
            progressBar.Visible = false;
            MasterConfigFile masterconfig = new MasterConfigFile();
            masterconfig.UploadMasterConfigFile(WinTools.GetcxIPWithPortNumber());

        }

        private void sourceServersComboBox_SelectedIndexChanged(object sender, EventArgs e)
        {
            try
            {
                if (sourceServersComboBox.SelectedItem != null)
                {
                    sourcevCenterIPTextBox.Text = sourceServersComboBox.SelectedItem.ToString();
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

        private void selectTargetvSphereComboBox_SelectedIndexChanged(object sender, EventArgs e)
        {
            try
            {
                if (selectTargetvSphereComboBox.SelectedItem != null)
                {
                    enterTargetIPTextBox.Text = selectTargetvSphereComboBox.SelectedItem.ToString();
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

   

        
        private void closePictureBox_Click(object sender, EventArgs e)
        {
            try
            {
                helpPanel.Visible = false;
                slideOpen = false;
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: closePictureBox_Click_1 " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void helpPictureBox_Click(object sender, EventArgs e)
        {
            try
            {
                if (slideOpen == false)
                {
                    helpPanel.BringToFront();
                    helpPanel.Visible = true;
                    slideOpen = true;
                }
                else
                {
                    slideOpen = false;
                    helpPanel.Visible = false;
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: helpPictureBox_Click " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void esxProtectionLinkLabel_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
        {
            try
            {
                if (File.Exists(installedPath + "\\Manual.chm"))
                {
                    Help.ShowHelp(null, installedPath + "\\Manual.chm", HelpNavigator.TopicId, Upgraded);
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

        // Backgroung worked do_work will call this method.. 
        //Based on the comparision....
        // We will update all the attribute required for the vcon 3.0
        // We are checking for the power on status also because of in case of failback 
        // Both on targetside and sourceside we will have same displayname and hostname so we are taking care incase of failback...
        public bool SetNodes(XmlNode esxMasternode, Host h1)
        {
            try
            {
                //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
                //based on above comment we have added xmlresolver as null
                XmlDocument documentMasterEsx = new XmlDocument();
                documentMasterEsx.XmlResolver = null;
                string esxMasterXmlFile = latestPath + "\\" + masterConfigFile;
                //StreamReader reader = new StreamReader(esxMasterXmlFile);
               // string xmlString = reader.ReadToEnd();
                //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
                XmlReaderSettings settings = new XmlReaderSettings();
                settings.ProhibitDtd = true;
                settings.XmlResolver = null;
                using (XmlReader reader1 = XmlReader.Create(esxMasterXmlFile, settings))
                {
                    documentMasterEsx.Load(reader1);
                    //reader1.Close();
                }
                //documentMasterEsx.Load(esxMasterXmlFile);
                //reader.Close();
                XmlNodeList hostNodesEsxMasterXml = null;
                hostNodesEsxMasterXml = documentMasterEsx.GetElementsByTagName("host");
                foreach (XmlNode node in hostNodesEsxMasterXml)
                {
                    if (node.Attributes["role"].Value == esxMasternode.Attributes["role"].Value && node.Attributes["ip_address"].Value == esxMasternode.Attributes["ip_address"].Value && node.Attributes["hostname"].Value == esxMasternode.Attributes["hostname"].Value && node.Attributes["display_name"].Value == esxMasternode.Attributes["display_name"].Value && h1.poweredStatus == "ON")
                    {
                        if (esxMasternode.Attributes["vConversion"] == null)
                        {
                            XmlElement ele = (XmlElement)esxMasternode;
                            ele.SetAttribute("vConversion", vConVersionNumber);
                        }
                        else
                        {
                            esxMasternode.Attributes["vConversion"].Value = vConVersionNumber;
                        }
                        if (esxMasternode.Attributes["alt_guest_name"] == null)
                        {
                            XmlElement ele = (XmlElement)esxMasternode;
                            ele.SetAttribute("alt_guest_name", h1.alt_guest_name);
                        }
                        else if (esxMasternode.Attributes["alt_guest_name"].Value.Length == 0)
                        {
                            esxMasternode.Attributes["alt_guest_name"].Value = h1.alt_guest_name;
                        }
                        if (esxMasternode.Attributes["vmx_version"] == null)
                        {
                            XmlElement ele = (XmlElement)esxMasternode;
                            ele.SetAttribute("vmx_version", h1.vmxversion);
                        }
                        else if (esxMasternode.Attributes["vmx_version"].Value.Length == 0)
                        {
                            esxMasternode.Attributes["vmx_version"].Value = h1.vmxversion;
                        }
                        if (esxMasternode.Attributes["floppy_device_count"] == null)
                        {
                            XmlElement ele = (XmlElement)esxMasternode;
                            ele.SetAttribute("floppy_device_count", h1.floppy_device_count);
                        }
                        else 
                        {
                            esxMasternode.Attributes["floppy_device_count"].Value = h1.floppy_device_count;
                        }
                        if (esxMasternode.Attributes["hostversion"] == null)
                        {
                            XmlElement ele = (XmlElement)esxMasternode;
                            ele.SetAttribute("hostversion", h1.hostversion);
                        }
                        else 
                        {
                            esxMasternode.Attributes["hostversion"].Value = h1.hostversion;
                        }
                        if (esxMasternode.Attributes["vSpherehostname"] == null)
                        {
                            XmlElement ele = (XmlElement)esxMasternode;
                            ele.SetAttribute("vSpherehostname", h1.vSpherehost);
                        }
                        else 
                        {
                            esxMasternode.Attributes["vSpherehostname"].Value = h1.vSpherehost;
                        }
                        if (esxMasternode.Attributes["resourcepoolgrpname"] == null)
                        {
                            XmlElement ele = (XmlElement)esxMasternode;
                            ele.SetAttribute("resourcepoolgrpname", h1.resourcepoolgrpname);
                        }
                        else 
                        {
                            esxMasternode.Attributes["resourcepoolgrpname"].Value = h1.resourcepoolgrpname;
                        }
                        if (esxMasternode.Attributes["datacenter"] == null)
                        {
                            XmlElement ele = (XmlElement)esxMasternode;
                            ele.SetAttribute("datacenter", h1.datacenter);
                        }
                        else 
                        {
                            esxMasternode.Attributes["datacenter"].Value = h1.datacenter;
                        }
                        if (esxMasternode.Attributes["vCenterProtection"] == null)
                        {
                            XmlElement ele = (XmlElement)esxMasternode;
                            ele.SetAttribute("vCenterProtection", h1.vCenterProtection);
                        }
                        else 
                        {
                            esxMasternode.Attributes["vCenterProtection"].Value = h1.vCenterProtection;
                        }
                        if (esxMasternode.Attributes["source_uuid"] == null)
                        {
                            XmlElement ele = (XmlElement)esxMasternode;
                            ele.SetAttribute("source_uuid", h1.source_uuid);
                        }
                        else
                        {
                            esxMasternode.Attributes["source_uuid"].Value = h1.source_uuid;
                        }
                        if (esxMasternode.Attributes["cpucount"] == null)
                        {
                            XmlElement ele = (XmlElement)esxMasternode;
                            ele.SetAttribute("cpucount", h1.cpuCount.ToString());
                        }
                        else if (esxMasternode.Attributes["cpucount"].Value.Length == 0)
                        {
                            esxMasternode.Attributes["cpucount"].Value = h1.cpuCount.ToString();
                        }
                        if (esxMasternode.Attributes["memsize"] == null)
                        {
                            XmlElement ele = (XmlElement)esxMasternode;
                            ele.SetAttribute("memsize", h1.memSize.ToString());
                        }
                        else if (esxMasternode.Attributes["memsize"].Value.Length == 0)
                        {
                            esxMasternode.Attributes["memsize"].Value = h1.memSize.ToString();
                        }
                        esxMasternode.Attributes["esxIp"].Value = ip;
                        
                        break;
                    }
                }
                documentMasterEsx.Save(esxMasterXmlFile);
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

        
     

    

      
        
    }
}
