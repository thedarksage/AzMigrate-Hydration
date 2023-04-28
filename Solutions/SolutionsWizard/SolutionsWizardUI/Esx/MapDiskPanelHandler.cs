using System;
using System.Collections.Generic;
using System.Text;
using Com.Inmage;
using System.Windows.Forms;
using System.Threading;
using Com.Inmage.Esxcalls;
using System.Collections;
using Com.Inmage.Tools;
using System.IO;
using System.Drawing;
using System.Diagnostics;

using System.Xml;
using System.Net;
using System.Text.RegularExpressions;
using HttpCommunication;

namespace Com.Inmage.Wizard
{
    class MapDiskPanelHandler : PanelHandler
    {
        internal string _latestPath = WinTools.FxAgentPath() + "\\vContinuum\\Latest";
        internal int _rowIndex = 0;
        internal static int HOSTNAME_COLUMN = 0;
        internal static int CHECK_COLUMN = 1;
        internal static int RESULT_COLUMN = 2;
        internal static int ACTION_COLUMN = 3;

        internal System.Drawing.Bitmap _passed;
        internal System.Drawing.Bitmap _failed;
        internal System.Drawing.Bitmap _warning;
        internal string _directoryNameinXmlFile = null;
        internal string _fxFailOverDataPath = null;
        internal static string DEFAULT_RETENTION_DIR = null;
        public MapDiskPanelHandler()
        {
            _passed = Wizard.Properties.Resources.tick;
            _failed = Wizard.Properties.Resources.cross;
            _warning = Wizard.Properties.Resources.warning;
            _fxFailOverDataPath = WinTools.FxAgentPath() + "\\Failover\\Data";
            
        }

        internal override bool Initialize(AllServersForm allServersForm)
        {
            try
            {

                try
                {
                    allServersForm.v2pProcessServercomboBox.Items.Clear();
                    allServersForm.mandatoryLabel.Visible = true;
                }
                catch (Exception ex)
                {
                    Trace.WriteLine(DateTime.Now + "\t Failed to clear process servers " + ex.Message);
                }
                if (allServersForm.osTypeSelected == OStype.Linux)
                {
                    DEFAULT_RETENTION_DIR = "/Retention_Logs";
                }
                else if (allServersForm.osTypeSelected == OStype.Windows)
                {
                    DEFAULT_RETENTION_DIR = ":\\Retention_Logs";
                }
                Host masterHost1 = (Host)allServersForm.selectedMasterListbyUser._hostList[0];
                masterHost1.disks.partitionList = new ArrayList();
                if (allServersForm.osTypeSelected == OStype.Windows)
                {
                    allServersForm.v2pNoteLabel.Text = "Note: Please don't select WindowsToGo USB disk as target disk";

                }
                else
                {
                    allServersForm.v2pNoteLabel.Text = "Note: Please don't select target USB disk where Unified agent is installed as target";
                }
                if (masterHost1.disks.GetPartitionsFreeSpace( masterHost1) == false)
                {
                    masterHost1 = DiskList.GetHost;
                    Thread.Sleep(1000);
                    if (masterHost1.disks.GetPartitionsFreeSpace( masterHost1) == false)
                    {
                        masterHost1 = DiskList.GetHost;
                        Trace.WriteLine(DateTime.Now + " \t Came here to rerun  GetPartitionsFreeSpace() in Initialize of ESX_SourceToMasterTargetPanelHandler");
                        MessageBox.Show("Master target:" + masterHost1.displayname + " (" + masterHost1.ip + " )" + " information is not found in the CX server " +
                      Environment.NewLine + Environment.NewLine + "Please verify that" +
                      Environment.NewLine +
                      Environment.NewLine + "1. Agent is installed" +
                      Environment.NewLine + "2. Master target is up and running" +
                      Environment.NewLine + "3. vContinuum wizard is pointing to " + WinTools.GetcxIP() + " with port number " + WinTools.GetcxPortNumber() +
                      Environment.NewLine + "     Make sure that both master target and vContinuum wizard are pointed to same CX." +
                      Environment.NewLine + "4. InMage Scout services are up and running" +
                        Environment.NewLine + "5. Master target is licenced in CX.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        object o = new object();
                        EventArgs e = new EventArgs();
                        allServersForm.previousButton_Click(o, e);
                        return false;
                    }
                }
                masterHost1 = DiskList.GetHost;
               // masterHost1.Print();

                //Filling the retention drives in to the combobox.....
                ComboBox box = new ComboBox();
                allServersForm.retentionDriveComboBox.Items.Clear();
                foreach (Host masterHost in allServersForm.selectedMasterListbyUser._hostList)
                {
                    if (masterHost.disks.partitionList.Count > 0)
                    {
                        foreach (Hashtable hash in masterHost.disks.partitionList)
                        {
                            if (!hash["Name"].ToString().Contains("/dev"))
                            {//skipping /dev/sdb /dev/sdc like partition for linux
                                if (!(hash["Name"].ToString() == "/"))
                                {
                                    //skipping "/" as retetnion drive and some times CXCLI sends proper retention but while posting to CX, it might fail to setup  VX pairs

                                    allServersForm.retentionDriveComboBox.Items.Add(hash["Name"]);
                                    box.Items.Add(hash["Name"]);
                                }
                            }
                        }
                    }
                }

                if (allServersForm.retentionDriveComboBox.Items.Count != 0)
                {
                    allServersForm.retentionDriveComboBox.SelectedItem = allServersForm.retentionDriveComboBox.Items[0];
                }
                else
                {
                    MessageBox.Show("There are no partitions available for retention. Create partitions on master target and try again.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    object obj = new object();
                    EventArgs eventArgs = new EventArgs();
                    allServersForm.previousButton_Click(obj, eventArgs);
                    return false;
                }
                allServersForm.nextButton.Enabled = false;
                Host h = (Host)allServersForm.selectedSourceListByUser._hostList[0];
                Host targetHost = (Host)allServersForm.selectedMasterListbyUser._hostList[0];
                allServersForm.sourceServerDisplayNameLabel.Text = "Source VM Name: " + h.displayname;
                allServersForm.sourceServerIPLabel.Text = "Source VM IP: " + h.ip;
                allServersForm.targetServerDisplayNameLabel.Text = "Target Physical Server Name: " + targetHost.displayname;
                allServersForm.targetServerIPLabel.Text = "Target Physical Server IP: " + targetHost.ip;
                int rowIndex = 0;
                allServersForm.mapDiskDataGridView.Rows.Clear();
                foreach (Hashtable diskHash in h.disks.list)
                {
                    if (diskHash["Selected"].ToString() == "Yes")
                    {
                        allServersForm.mapDiskDataGridView.Rows.Add(1);
                        allServersForm.mapDiskDataGridView.Rows[rowIndex].Cells[0].Value = diskHash["src_devicename"];
                        allServersForm.mapDiskDataGridView.Rows[rowIndex].Cells[1].Value = diskHash["Size"];
                        rowIndex++;
                    }
                }

                allServersForm.seletTargetDisk.Items.Clear();

                foreach (Hashtable diskHash in targetHost.disks.list)
                {
                    if (diskHash["media_type"] != null && diskHash["ide_or_scsi"] != null)
                    {
                        if (diskHash["media_type"].ToString().ToUpper().Contains("USB") || diskHash["ide_or_scsi"].ToString().ToUpper().Contains("USB"))
                        {
                            Trace.WriteLine(DateTime.Now + "\t Media contains USB ");
                        }
                        else
                        {
                            allServersForm.seletTargetDisk.Items.Add(diskHash["src_devicename"] + "(" + diskHash["Size"] + ")");
                        }
                    }
                    else
                    {
                        Trace.WriteLine(DateTime.Now + "\t Media type got null ");
                        allServersForm.seletTargetDisk.Items.Add(diskHash["src_devicename"] + "(" + diskHash["Size"] + ")");
                    }
                }
                allServersForm.nextButton.Text = "Protect";
                GetProcessServerIP(allServersForm);

                Host sourceHost = (Host)allServersForm.selectedSourceListByUser._hostList[0];

                if (sourceHost.consistencyInmins == null)
                {
                    allServersForm.consistencyTextBox.Text = "15";
                }

                if (sourceHost.retentionInDays == null)
                {
                    allServersForm.retentionInDaysTextBox.Text = "3";
                }
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Initialize of MapDiskPanelHandler.cs " + ex.Message);
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            return true;
        }


        private bool GetProcessServerIP(AllServersForm allServersForm)
        {
            try
            {
                // Getting process server ip form the cxcli call.

               
                
                String postData = "&operation=2";
                WinPreReqs win = new WinPreReqs("", "", "", "");
                HttpReply reply = win.ConnectToHttpClient(postData);
                if (String.IsNullOrEmpty(reply.error))
                {
                    // process reply
                    if (!String.IsNullOrEmpty(reply.data))
                    {
                        Trace.WriteLine(reply.data);
                    }
                }
                else
                {

                    Trace.WriteLine(reply.error);
                    return false;
                }
                string responseFromServer = reply.data;
                //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
                //based on above comment we have added xmlresolver as null
                XmlDocument xDocument = new XmlDocument();
                xDocument.XmlResolver = null;
                //MessageBox.Show(responseFromServer);
                try
                {
                    XmlReaderSettings settings = new XmlReaderSettings();
                    settings.ProhibitDtd = true;
                    settings.XmlResolver = null;
                    //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
                    using (XmlReader reader1 = XmlReader.Create(_latestPath + WinPreReqs.tempXml, settings))
                    {
                        xDocument.Load(reader1);
                        //reader1.Close();
                    }
                    //xDocument.Load(_latestPath + WinPreReqs.tempXml);
                }
                catch (Exception e)
                {
                    MessageBox.Show("CX server: " + allServersForm.cxIPretrived + " is not responding ", "CX CLI error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    Trace.WriteLine(DateTime.Now + " \t CX server: " + allServersForm.cxIPretrived + "is not responding to the cli");
                    return false;
                }
                XmlNodeList xPNodeList = xDocument.SelectNodes(".//ip");
                ComboBox box = new ComboBox();
                foreach (XmlNode xNode in xPNodeList)
                {
                    //MessageBox.Show(xNode.InnerText != null ? xNode.InnerText : "");
                    box.Items.Add(xNode.InnerText != null ? xNode.InnerText : "");
                    allServersForm.v2pProcessServercomboBox.Items.Add(xNode.InnerText != null ? xNode.InnerText : "");
                }

                try
                {
                    if (allServersForm.v2pProcessServercomboBox.Items.Count != 0)
                    {
                        allServersForm.v2pProcessServercomboBox.SelectedItem = allServersForm.v2pProcessServercomboBox.Items[0];
                    }
                }
                catch (Exception e)
                {
                    MessageBox.Show("Process server ip is not reachable", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return false;
                }

            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            return true;
        }

        internal override bool ValidatePanel(AllServersForm allServersForm)
        {
            
            return true;
        }

        internal override bool ProcessPanel(AllServersForm allServersForm)
        {
            try
            {
                _rowIndex = 0;



                Host sourceHost = (Host)allServersForm.selectedSourceListByUser._hostList[0];
                Host masterHost = (Host)allServersForm.selectedMasterListbyUser._hostList[0];
                string planName = null;
                planName = allServersForm.v2pPlanNameTextBox.Text;
                string cxip = WinTools.GetcxIP();
                foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                {
                    h.machineType = "PhysicalMachine";
                    h.failback = "yes";
                    h.plan = planName;
                    h.masterTargetDisplayName = masterHost.displayname;
                    h.masterTargetHostName = masterHost.hostname;
                    h.targetESXIp = null;
                    h.targetESXUserName = null;
                    h.targetESXPassword = null;
                    h.esxIp = null;
                    h.esxUserName = null;
                    h.esxPassword = null;
                    h.targetvSphereHostName = null;
                    h.vSpherehost = null;
                    h.mt_uuid = null;
                    h.source_uuid = null;

                }

                FileInfo f1 = new FileInfo(_latestPath + "\\Inmage_scsi_unit_disks.txt");
                StreamWriter sw = f1.CreateText();
                sw.WriteLine(sourceHost.inmage_hostid);
                int diskCOunt = 0;
                foreach (Hashtable hash in sourceHost.disks.list)
                {
                    if (hash["Selected"].ToString() == "Yes")
                    {
                        diskCOunt++;
                    }
                }

                sw.WriteLine(diskCOunt.ToString());
                foreach (Hashtable sourceHash in sourceHost.disks.list)
                {
                    foreach (Hashtable masterHash in masterHost.disks.list)
                    {
                        if (sourceHash["targetDisk"] != null && masterHash["src_devicename"] != null)
                        {
                            Trace.WriteLine(DateTime.Now + "\t Printing the disknames selected target " + sourceHash["targetDisk"] + " target " + masterHash["src_devicename"]);
                            if (sourceHash["targetDisk"].ToString() == masterHash["src_devicename"].ToString())
                            {
                                if (sourceHost.os == OStype.Windows)
                                {
                                    if (sourceHash["scsi_mapping_host"] != null && masterHash["scsi_mapping_host"] != null )
                                    {
                                        sw.WriteLine(sourceHash["scsi_mapping_host"] + "!@!@!" + masterHash["scsi_mapping_host"] );
                                    }
                                    else if (sourceHash["scsi_mapping_host"] == null && masterHash["scsi_mapping_host"] != null)
                                    {
                                        sw.WriteLine(sourceHash["src_devicename"] + "!@!@!" + masterHash["scsi_mapping_host"]);
                                    }
                                    else
                                    {
                                        sw.WriteLine(sourceHash["src_devicename"] + "!@!@!" + masterHash["src_devicename"]);
                                    }
                                    break;
                                }
                                else
                                {
                                    //if (sourceHash["scsi_mapping_host"] != null && masterHash["scsi_mapping_host"] != null )
                                    //{
                                    //    sw.WriteLine(sourceHash["scsi_mapping_host"] + "!@!@!" + masterHash["scsi_mapping_host"]);
                                    //}
                                    //else if (sourceHash["scsi_mapping_host"] == null && masterHash["scsi_mapping_host"] != null)
                                    //{
                                    //    sw.WriteLine(sourceHash["src_devicename"] + "!@!@!" + masterHash["scsi_mapping_host"]);
                                    //}
                                    //else
                                    //{
                                        sw.WriteLine(sourceHash["src_devicename"] + "!@!@!" + masterHash["src_devicename"]);
                                    //}
                                    break;

                                }
                            }
                        }
                    }
                }
                sw.Close();

                if (allServersForm.v2pProcessServercomboBox.SelectedItem != null)
                {
                    foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                    {
                        h.processServerIp = allServersForm.v2pProcessServercomboBox.SelectedItem.ToString();
                    }
                }
                if (allServersForm.retentionDriveComboBox.SelectedItem != null)
                {
                    foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                    {
                        h.retentionPath = allServersForm.retentionDriveComboBox.SelectedItem.ToString() + DEFAULT_RETENTION_DIR;
                        if (allServersForm.retentionDriveComboBox.SelectedItem.ToString().Contains(":"))
                        {
                            h.retentionPath = allServersForm.retentionDriveComboBox.SelectedItem.ToString() + "\\Retention_Logs";
                        }
                        else
                        {
                            h.retentionPath = allServersForm.retentionDriveComboBox.SelectedItem.ToString() + DEFAULT_RETENTION_DIR;
                        }
                        h.retention_drive = allServersForm.retentionDriveComboBox.SelectedItem.ToString();
                     
                    }
                }

                if (allServersForm.consistencyTextBox.Text.Length != 0)
                {
                    foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                    {
                        h.consistencyInmins = allServersForm.consistencyTextBox.Text;
                    }
                }

                if (allServersForm.retentionInDaysTextBox.Text.Length != 0)
                {
                    foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                    {
                        h.retentionInDays = allServersForm.retentionInDaysTextBox.Text;
                    }
                }
                foreach (Host h in allServersForm.selectedMasterListbyUser._hostList)
                {
                    h.role = Role.Secondary;
                }
                SolutionConfig solution = new SolutionConfig();
                Esx esx = new Esx();
                solution.WriteXmlFile(allServersForm.selectedSourceListByUser, allServersForm.selectedMasterListbyUser, esx, cxip, "ESX.xml", planName, true);


                Random randomlyGenerateNumber = new Random();
                int randomNumber = randomlyGenerateNumber.Next(999999);
                _directoryNameinXmlFile = planName + "_" + masterHost.hostname + "_" + randomNumber.ToString();
                if (Directory.Exists(_fxFailOverDataPath + "\\" + _directoryNameinXmlFile))
                {
                    randomNumber = randomlyGenerateNumber.Next(999999);
                    _directoryNameinXmlFile = planName + "_" + masterHost.hostname + "_" + randomNumber.ToString();
                }


                if (AllServersForm.SetFolderPermissions(_fxFailOverDataPath + "\\" + _directoryNameinXmlFile) == true)
                {
                    Trace.WriteLine(DateTime.Now + "\t Successfully set the permissions for the folder ");
                }
                else
                {
                    Trace.WriteLine(DateTime.Now + "\t Failed to set permissions for the folder ");
                }
                //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
                //based on above comment we have added xmlresolver as null
                XmlDocument document = new XmlDocument();
                document.XmlResolver = null;
                string xmlfile = _latestPath + "\\ESX.xml";
                //StreamReader reader = new StreamReader(xmlfile);
                //string xmlString = reader.ReadToEnd();
                XmlReaderSettings settings = new XmlReaderSettings();
                settings.ProhibitDtd = true;
                settings.XmlResolver = null;
                //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
                using (XmlReader reader1 = XmlReader.Create(xmlfile, settings))
                {
                    document.Load(reader1);
                   // reader1.Close();
                }
               // document.Load(xmlfile);
               // reader.Close();
                //XmlNodeList hostNodes = null;
                XmlNodeList rootNodes = null, hostNodes = null, diskNodes = null;
                rootNodes = document.GetElementsByTagName("root");
                hostNodes = document.GetElementsByTagName("host");
                diskNodes = document.GetElementsByTagName("disk");

                foreach (XmlNode node in rootNodes)
                {
                    //node.ParentNode["batchresync"].Value = "0";
                    node.Attributes["batchresync"].Value = "3";
                    node.Attributes["plan"].Value = planName;
                    allServersForm.planInput = planName;
                    node.Attributes["xmlDirectoryName"].Value = _directoryNameinXmlFile;
                    if (node.Attributes["V2P"] != null)
                    {
                        node.Attributes["V2P"].Value = "True";
                    }
                }
                foreach (XmlNode node in hostNodes)
                {
                    foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                    {
                        if (h.displayname == node.Attributes["display_name"].Value)
                        {
                            node.Attributes["inmage_hostid"].Value = h.inmage_hostid;
                            node.Attributes["mbr_path"].Value = h.mbr_path;
                            node.Attributes["system_volume"].Value = h.system_volume;
                        }
                    }
                }
                Trace.WriteLine(DateTime.Now + " \t printing foldername " + _directoryNameinXmlFile);
                document.Save(xmlfile);
                if (File.Exists(_latestPath + "\\ESX.xml"))
                {
                    FileInfo destFile = new FileInfo(_fxFailOverDataPath + "\\" + _directoryNameinXmlFile + "\\ESX.xml");
                    if (destFile.Directory != null && !destFile.Directory.Exists)
                    {
                        destFile.Directory.Create();
                    }
                    File.Copy(_latestPath + "\\ESX.xml", destFile.ToString(), true);
                }
                if (File.Exists(_latestPath + "\\Inmage_scsi_unit_disks.txt"))
                {
                    FileInfo destFile = new FileInfo(_fxFailOverDataPath + "\\" + _directoryNameinXmlFile + "\\Inmage_scsi_unit_disks.txt");
                    if (destFile.Directory != null && !destFile.Directory.Exists)
                    {
                        destFile.Directory.Create();
                    }
                    File.Copy(_latestPath + "\\Inmage_scsi_unit_disks.txt", destFile.ToString(), true);
                    AllServersForm.SetFolderPermissions(_fxFailOverDataPath + "\\" + _directoryNameinXmlFile);
                }

                StatusForm form = new StatusForm(allServersForm.selectedSourceListByUser, masterHost, allServersForm.appNameSelected, _directoryNameinXmlFile);
                //form.ShowDialog();
                allServersForm.closeFormForcelyByUser = true;
                allServersForm.Close();
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Process Panel of MapDiskPanelHandler.cs " + ex.Message);
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            return true;
        }

        internal override bool CanGoToNextPanel(AllServersForm allServersForm)
        {
            return true;
        }

        internal override bool CanGoToPreviousPanel(AllServersForm allServersForm)
        {
            allServersForm.mandatoryLabel.Visible = false;
            allServersForm.nextButton.Enabled = true;
            allServersForm.nextButton.Text = "Next";
            return true;
        }


        internal bool SelectedDisk(AllServersForm allServersFrom, int index)
        {
            try
            {
                Host maserTargetHost = (Host)allServersFrom.selectedMasterListbyUser._hostList[0];
                Host sourceHost = (Host)allServersFrom.selectedSourceListByUser._hostList[0];
                string sourceDiskName = allServersFrom.mapDiskDataGridView.Rows[index].Cells[0].Value.ToString();
                long sizeOfSourceDisk = long.Parse(allServersFrom.mapDiskDataGridView.Rows[index].Cells[1].Value.ToString());
                Trace.WriteLine(DateTime.Now + "\t size of source disk " + sizeOfSourceDisk.ToString());
                string selectedDisk = allServersFrom.mapDiskDataGridView.Rows[index].Cells[2].Value.ToString();
                for (int i = 0; i < allServersFrom.mapDiskDataGridView.RowCount; i++)
                {
                    if (i != index)
                    {
                        if (allServersFrom.mapDiskDataGridView.Rows[i].Cells[2].Value != null)
                        {
                            if (selectedDisk == allServersFrom.mapDiskDataGridView.Rows[i].Cells[2].Value.ToString())
                            {
                                MessageBox.Show("This disk is already selected for another disk. Select another disk", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                allServersFrom.mapDiskDataGridView.Rows[index].Cells[2].Value = null;
                                allServersFrom.mapDiskDataGridView.RefreshEdit();
                                return false;
                            }
                        }
                    }
                }
                string[] selectedDiskValues = selectedDisk.Split('(');
                if (selectedDiskValues.Length >= 2)
                {
                    long selectedDiskSise = long.Parse(selectedDiskValues[1].ToString().Split(')')[0]);
                    string diskName = selectedDiskValues[0].ToString();
                    foreach (Hashtable sourceHash in sourceHost.disks.list)
                    {
                        if (sourceHash["src_devicename"].ToString() == sourceDiskName)
                        {
                            Trace.WriteLine(DateTime.Now + "\t Printing the values of disks " + sourceHash["Size"].ToString() + " selected one " + selectedDiskSise.ToString());
                            if (long.Parse(sourceHash["Size"].ToString()) > selectedDiskSise)
                            {
                                allServersFrom.mapDiskDataGridView.Rows[index].Cells[2].Value = null;
                                allServersFrom.mapDiskDataGridView.RefreshEdit();
                                MessageBox.Show("Selected disk size is less than the source disk. Select another disk to proceed", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                return false;
                            }
                            else
                            {
                                sourceHash["targetDisk"] = selectedDiskValues[0];
                                Trace.WriteLine(DateTime.Now + "\t Selected disk : " + sourceHash["targetDisk"] + "for the source disk " + sourceHash["src_devicename"]);
                                allServersFrom.selectedSourceListByUser.AddOrReplaceHost(sourceHost);
                            }
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t SelectedDisk of MapDiskPanelHandler.cs " + ex.Message);
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            return true;
        }

        internal bool CheckReadniessChecks(AllServersForm allServersForm)
        {
            try
            {
                foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                {
                    if (h.numberOfEntriesInCX == 2)
                    {
                        allServersForm.planNameLabel.Visible = false;
                        allServersForm.planNameText.Visible = false;
                        allServersForm.nextButton.Enabled = false;
                        return false;
                    }
                    if (allServersForm.osTypeSelected == OStype.Windows)
                    {
                        if (h.pointedToCx == false || h.vxagentInstalled == false || h.fxagentInstalled == false || h.vx_agent_heartbeat == false || h.fx_agent_heartbeat == false || h.mbr_path == null)
                        {
                            allServersForm.planNameLabel.Visible = false;
                            allServersForm.planNameText.Visible = false;
                            allServersForm.nextButton.Enabled = false;
                            return false;
                        }
                    }
                    else if (allServersForm.osTypeSelected == OStype.Linux || allServersForm.osTypeSelected == OStype.Solaris)
                    {
                        if (h.pointedToCx == false ||  h.vxagentInstalled == false || h.fxagentInstalled == false || h.vx_agent_heartbeat == false || h.fx_agent_heartbeat == false)
                        {
                            allServersForm.planNameLabel.Visible = false;
                            allServersForm.planNameText.Visible = false;
                            allServersForm.nextButton.Enabled = false;
                            return false;
                        }
                    }
                }
                foreach (Host h in allServersForm.selectedMasterListbyUser._hostList)
                {
                    if (h.numberOfEntriesInCX == 2)
                    {
                        allServersForm.planNameLabel.Visible = false;
                        allServersForm.planNameText.Visible = false;
                        allServersForm.nextButton.Enabled = false;
                        return false;
                    }
                    if (h.pointedToCx == false ||  h.vxagentInstalled == false || h.fxagentInstalled == false || h.vx_agent_heartbeat == false || h.fx_agent_heartbeat == false)
                    {
                        allServersForm.planNameLabel.Visible = false;
                        allServersForm.planNameText.Visible = false;
                        allServersForm.nextButton.Enabled = false;
                        return false;
                    }
                }
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t CheckReadinessCheck of MapDiskPanelHandler.cs " + ex.Message);
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            return true;
        }


        internal bool PreReadinesschecks(AllServersForm allServersForm)
        {
            try
            {
                Host masterHost = (Host)allServersForm.selectedMasterListbyUser._hostList[0];
                for (int i = 0; i < allServersForm.mapDiskDataGridView.RowCount; i++)
                {
                    if (allServersForm.mapDiskDataGridView.Rows[i].Cells[2].Value == null)
                    {
                        MessageBox.Show("Select target disk for all the source disk to proceed", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return false;
                    }
                }
                try
                {

                    if (allServersForm.retentionInDaysTextBox.Text.Length == 0)
                    {
                        MessageBox.Show("Enter retention in days to proceed.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return false;
                    }


                    if (int.Parse(allServersForm.retentionInDaysTextBox.Text.ToString()) == 0 || allServersForm.retentionInDaysTextBox.Text.Contains("-"))
                    {
                        MessageBox.Show("Retention in days should be greater than or equal to 1", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return false;
                    }


                    if (allServersForm.consistencyTextBox.Text.Length == 0)
                    {
                        MessageBox.Show("Enter consistency inteval to proceed", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return false;
                    }
                    string planName = null;
                    if (allServersForm.v2pPlanNameTextBox.Text.Trim().Length == 0)
                    {
                        MessageBox.Show("Enter planname to proceed", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return false;
                    }
                    else
                    {
                        planName = allServersForm.v2pPlanNameTextBox.Text;
                    }
                    if (allServersForm.v2pPlanNameTextBox.Text.Contains("&") || allServersForm.v2pPlanNameTextBox.Text.Contains("\"") || allServersForm.v2pPlanNameTextBox.Text.Contains("\'") || allServersForm.v2pPlanNameTextBox.Text.Contains("<") || allServersForm.v2pPlanNameTextBox.Text.Contains(">") || allServersForm.v2pPlanNameTextBox.Text.Contains(":") || allServersForm.v2pPlanNameTextBox.Text.Contains("\\") || allServersForm.v2pPlanNameTextBox.Text.Contains("//") || allServersForm.v2pPlanNameTextBox.Text.Contains("|") || allServersForm.v2pPlanNameTextBox.Text.Contains("*"))
                    {
                        MessageBox.Show("Plan name does not accept character * : ? < > / \\ | & \' \"", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return false;
                    }
                    WinPreReqs win = new WinPreReqs("", "", "", "");
                    if (WinPreReqs.IsThisPlanUsesSameMT(allServersForm.planNameText.Text.Trim().ToString(), masterHost.hostname, WinTools.GetcxIPWithPortNumber()) == false)
                    {
                        MessageBox.Show("Select another planname for the protection", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return false;
                    }
                }
                catch (Exception ex)
                {
                    Trace.WriteLine(DateTime.Now + "\t In of PreReadiness Checks of MapDiskPanelHandler.cs " + ex.Message);
                    System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                    Trace.WriteLine("ERROR*******************************************");
                    Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                    Trace.WriteLine("Exception  =  " + ex.Message);
                    Trace.WriteLine("ERROR___________________________________________");
                    return false;
                }
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t PreReadiness of MapDiskPanelHandler.cs " + ex.Message);
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            return true;
        }

        internal bool ReadinessChecks(AllServersForm allServersForm)
        {
            try
            {
                foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                {
                    allServersForm.v2pLogTextBox.AppendText("Running readiness checks for " + h.displayname + "(" + h.ip + ") ......" + Environment.NewLine);
                    Host h1 = new Host();
                    h1 = h;
                    if (h.ranPrecheck == false)
                    {
                        bool preReqPassed = true;


                        WinPreReqs win = new WinPreReqs(h.ip, h.domain, h.userName, h.passWord);
                        int returnCode = 0;
                        if (h.hostname != null)
                        {

                            returnCode = win.SetHostIPinputhostname(h.hostname,  h.ip, WinTools.GetcxIPWithPortNumber());
                            h.ip = WinPreReqs.GetIPaddress;

                            //MessageBox.Show(h.ip);

                        }
                        if (returnCode == 0)
                        {
                            h.numberOfEntriesInCX = 1;
                        }
                        else if (returnCode == 1)
                        {
                            h.numberOfEntriesInCX = 2;
                        }
                        else if (returnCode == 2)
                        {
                            h.numberOfEntriesInCX = 0;
                        }
                        preReqPassed = CheckWinPreReqs(allServersForm, ref h1);
                        h.pointedToCx = h1.pointedToCx;
                        h.fxagentInstalled = h1.fxagentInstalled;
                        h.vxagentInstalled = h1.vxagentInstalled;
                        h.fxlicensed = h1.fxlicensed;
                        h.vxlicensed = h1.vxlicensed;
                        h.fx_agent_path = h1.fx_agent_path;
                        h.vx_agent_path = h1.vx_agent_path;
                        h.fx_agent_heartbeat = h1.fx_agent_heartbeat;
                        h.vx_agent_heartbeat = h1.vx_agent_heartbeat;
                        if (preReqPassed == true)
                        {
                            allServersForm.v2pLogTextBox.AppendText("Readiness checks completed for " + h1.displayname + "(" + h.ip + ") ..." + Environment.NewLine);
                        }
                        else
                        {
                            allServersForm.v2pLogTextBox.AppendText("Readiness checks failed for " + h1.displayname + "(" + h.ip + ") ..." + Environment.NewLine);
                        }
                        Host tempHost1 = new Host();
                        tempHost1.inmage_hostid = h1.inmage_hostid;
                        Cxapicalls cxApi = new Cxapicalls();
                        if (cxApi.Post( tempHost1, "GetHostInfo") == true)
                        {
                            tempHost1 = Cxapicalls.GetHost;
                            string partitionVolume = null;
                            foreach (Hashtable partition in tempHost1.disks.partitionList)
                            {
                                if (partition["IsSystemVolume"] != null && partition["Name"] != null)
                                {
                                    if (bool.Parse(partition["IsSystemVolume"].ToString()))
                                    {
                                        if (partitionVolume == null)
                                        {
                                            partitionVolume = partition["Name"].ToString();
                                        }
                                        else
                                        {
                                            partitionVolume = partitionVolume + "," + partition["Name"].ToString();
                                        }
                                    }
                                }
                            }
                            if (partitionVolume != null)
                            {
                                string[] partitions = partitionVolume.Split(',');
                                if (partitions.Length >= 1)
                                {
                                    if (h.os == OStype.Windows)
                                    {
                                        if (partitionVolume.Contains("C"))
                                        {
                                            h.system_volume = "C";
                                        }
                                        else
                                        {
                                            h.system_volume = partitions[0];
                                        }
                                    }
                                    else if (h.os == OStype.Linux)
                                    {

                                        h.system_volume = partitionVolume;
                                    }
                                }
                                else
                                {
                                    h.system_volume = partitionVolume;
                                }

                                Trace.WriteLine(DateTime.Now + "\t System volume for the host: " + h.displayname + " is " + h.system_volume);
                            }
                            else
                            {
                                Trace.WriteLine(DateTime.Now + "\t System volume got null for the CX database.");
                            }
                        }
                    }
                }
                foreach (Host h in allServersForm.selectedMasterListbyUser._hostList)
                {
                    allServersForm.v2pLogTextBox.AppendText("Running readiness checks for " + h.displayname + "(" + h.ip + ") ......" + Environment.NewLine);
                    Host h1 = new Host();
                    h1 = h;
                    if (h.ranPrecheck == false)
                    {
                        bool preReqPassed = true;


                        WinPreReqs win = new WinPreReqs(h.ip, h.domain, h.userName, h.passWord);
                        int returnCode = 0;
                        if (h.hostname != null)
                        {

                            returnCode = win.SetHostIPinputhostname(h.hostname,  h.ip, WinTools.GetcxIPWithPortNumber());
                            h.ip= WinPreReqs.GetIPaddress;

                            //MessageBox.Show(h.ip);

                        }
                        if (returnCode == 0)
                        {
                            h.numberOfEntriesInCX = 1;
                        }
                        else if (returnCode == 1)
                        {
                            h.numberOfEntriesInCX = 2;
                        }
                        else if (returnCode == 2)
                        {
                            h.numberOfEntriesInCX = 0;
                        }
                        preReqPassed = CheckWinPreReqs(allServersForm, ref h1);
                        h.pointedToCx = h1.pointedToCx;
                        h.fxagentInstalled = h1.fxagentInstalled;
                        h.vxagentInstalled = h1.vxagentInstalled;
                        h.fxlicensed = h1.fxlicensed;
                        h.vxlicensed = h1.vxlicensed;
                        h.fx_agent_path = h1.fx_agent_path;
                        h.vx_agent_path = h1.vx_agent_path;
                        h.fx_agent_heartbeat = h1.fx_agent_heartbeat;
                        h.vx_agent_heartbeat = h1.vx_agent_heartbeat;
                        if (preReqPassed == true)
                        {
                            allServersForm.v2pLogTextBox.AppendText("Readiness checks completed for " + h1.displayname + "(" + h.ip + ") ..." + Environment.NewLine);
                        }
                        else
                        {
                            allServersForm.v2pLogTextBox.AppendText("Readiness checks failed for " + h1.displayname + "(" + h.ip + ") ..." + Environment.NewLine);
                        }

                    }
                }
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Readiness Checks of MapDiskPanelHandler.cs " + ex.Message);
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            return true;
        }
        internal bool CheckWinPreReqs(AllServersForm allServersForm, ref Host h)
        {
            try
            {
                string cxipwithPortNumber = WinTools.GetcxIPWithPortNumber();
                // Getting the information form the cx to check the license and heart beat of the source, master target , vcon and cx.....
                Host h1 = new Host();
                bool preReqPassed = true;
                string errorMessage = "";
                WmiCode errorCode = WmiCode.Unknown;
                /*  if (WinPreReqs.IpReachable(h.ip) == false)
                  {
                      allServersForm.protectionText.AppendText("Error: Cant reach " + h.ip + Environment.NewLine);
                      preReqPassed = false;
                      return false;
                  }*/
                WinPreReqs win = new WinPreReqs(h.ip, h.domain, h.userName, h.passWord);
                if (win.GetDetailsFromcxcli( h,cxipwithPortNumber ) == true)
                {
                    h = WinPreReqs.GetHost;
                    return true;
                }
                else
                {
                    h = WinPreReqs.GetHost;
                    return false;
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }

        }


        internal bool ReloadDataGridViewAfterRunReadinessCheck(AllServersForm allServersForm, Host h)
        {
            try
            {
                if (h.ip == "127.0.0.1")
                {
                    allServersForm.v2pCheckReportDataGridView.Rows.Add(1);
                    allServersForm.v2pCheckReportDataGridView.Rows[_rowIndex].Cells[HOSTNAME_COLUMN].Value = h.displayname;
                    allServersForm.v2pCheckReportDataGridView.Rows[_rowIndex].Cells[CHECK_COLUMN].Value = "IP:127.0.0.1";
                    allServersForm.v2pCheckReportDataGridView.Rows[_rowIndex].Cells[RESULT_COLUMN].Value = _warning;
                    allServersForm.v2pCheckReportDataGridView.Rows[_rowIndex].Cells[ACTION_COLUMN].Value = "Found 127.0.0.1 ip for this vm. Pairs won't progress in this case.";
                    _rowIndex++;
                }
                if (h.numberOfEntriesInCX == 2)
                {
                    allServersForm.v2pCheckReportDataGridView.Rows.Add(1);
                    allServersForm.v2pCheckReportDataGridView.Rows[_rowIndex].Cells[HOSTNAME_COLUMN].Value = h.displayname;
                    allServersForm.v2pCheckReportDataGridView.Rows[_rowIndex].Cells[CHECK_COLUMN].Value = "Machine is pointing to cx";
                    allServersForm.v2pCheckReportDataGridView.Rows[_rowIndex].Cells[RESULT_COLUMN].Value = _failed;
                    allServersForm.v2pCheckReportDataGridView.Rows[_rowIndex].Cells[ACTION_COLUMN].Value = "Multiple entries found in CX database." + WinTools.GetcxIPWithPortNumber();
                    _rowIndex++;
                    return false;

                }

                // Once all the licenses verification is done we will fill in to thus datagridview....... whether they are passed or failed....
                if (h.pointedToCx == true &&  h.vxagentInstalled == true && h.fxagentInstalled == true && h.fx_agent_heartbeat == true && h.vx_agent_heartbeat == true)
                {
                    allServersForm.v2pCheckReportDataGridView.Rows.Add(1);
                    allServersForm.v2pCheckReportDataGridView.Rows[_rowIndex].Cells[HOSTNAME_COLUMN].Value = h.displayname;
                    allServersForm.v2pCheckReportDataGridView.Rows[_rowIndex].Cells[CHECK_COLUMN].Value = "All checks";
                    allServersForm.v2pCheckReportDataGridView.Rows[_rowIndex].Cells[RESULT_COLUMN].Value = _passed;
                    allServersForm.v2pCheckReportDataGridView.Rows[_rowIndex].Cells[ACTION_COLUMN].Value = " ";
                    _rowIndex++;
                    return true;

                }

                if (h.pointedToCx == false)
                {
                    allServersForm.v2pCheckReportDataGridView.Rows.Add(1);
                    allServersForm.v2pCheckReportDataGridView.Rows[_rowIndex].Cells[HOSTNAME_COLUMN].Value = h.displayname;
                    allServersForm.v2pCheckReportDataGridView.Rows[_rowIndex].Cells[CHECK_COLUMN].Value = "Machine is pointing to cx";
                    allServersForm.v2pCheckReportDataGridView.Rows[_rowIndex].Cells[RESULT_COLUMN].Value = _failed;
                    allServersForm.v2pCheckReportDataGridView.Rows[_rowIndex].Cells[ACTION_COLUMN].Value = "Either unified agent is not installed or agent is not pointing to CX IP:" + WinTools.GetcxIPWithPortNumber();
                    _rowIndex++;
                    return false;
                }
                if (h.vxagentInstalled == false)
                {
                    allServersForm.v2pCheckReportDataGridView.Rows.Add(1);
                    allServersForm.v2pCheckReportDataGridView.Rows[_rowIndex].Cells[HOSTNAME_COLUMN].Value = h.displayname;
                    allServersForm.v2pCheckReportDataGridView.Rows[_rowIndex].Cells[CHECK_COLUMN].Value = "VX Installed";
                    allServersForm.v2pCheckReportDataGridView.Rows[_rowIndex].Cells[RESULT_COLUMN].Value = _failed;
                    allServersForm.v2pCheckReportDataGridView.Rows[_rowIndex].Cells[ACTION_COLUMN].Value = "Install Unified agents";
                    _rowIndex++;

                }
                if (h.fxagentInstalled == false)
                {
                    allServersForm.v2pCheckReportDataGridView.Rows.Add(1);
                    allServersForm.v2pCheckReportDataGridView.Rows[_rowIndex].Cells[HOSTNAME_COLUMN].Value = h.displayname;
                    allServersForm.v2pCheckReportDataGridView.Rows[_rowIndex].Cells[CHECK_COLUMN].Value = "FX Installed";
                    allServersForm.v2pCheckReportDataGridView.Rows[_rowIndex].Cells[RESULT_COLUMN].Value = _failed;
                    allServersForm.v2pCheckReportDataGridView.Rows[_rowIndex].Cells[ACTION_COLUMN].Value = "Install Unified agents";
                    _rowIndex++;

                }
                if (h.fxlicensed == false && h.fxagentInstalled == true)
                {
                    allServersForm.v2pCheckReportDataGridView.Rows.Add(1);
                    allServersForm.v2pCheckReportDataGridView.Rows[_rowIndex].Cells[HOSTNAME_COLUMN].Value = h.displayname;
                    allServersForm.v2pCheckReportDataGridView.Rows[_rowIndex].Cells[CHECK_COLUMN].Value = "FX License";
                    allServersForm.v2pCheckReportDataGridView.Rows[_rowIndex].Cells[RESULT_COLUMN].Value = _failed;
                    allServersForm.v2pCheckReportDataGridView.Rows[_rowIndex].Cells[ACTION_COLUMN].Value = "Add FX license in CX UI->Settings->LicenseManagement";
                    _rowIndex++;

                }
                if (h.vxlicensed == false && h.vxagentInstalled == true)
                {
                    allServersForm.v2pCheckReportDataGridView.Rows.Add(1);
                    allServersForm.v2pCheckReportDataGridView.Rows[_rowIndex].Cells[HOSTNAME_COLUMN].Value = h.displayname;
                    allServersForm.v2pCheckReportDataGridView.Rows[_rowIndex].Cells[CHECK_COLUMN].Value = "VX License";
                    allServersForm.v2pCheckReportDataGridView.Rows[_rowIndex].Cells[RESULT_COLUMN].Value = _failed;
                    allServersForm.v2pCheckReportDataGridView.Rows[_rowIndex].Cells[ACTION_COLUMN].Value = "Add Vx license in  CX UI->Settings->LicenseManagement";
                    _rowIndex++;

                }

                if (h.fx_agent_heartbeat == false && h.fxagentInstalled == true)
                {
                    allServersForm.v2pCheckReportDataGridView.Rows.Add(1);
                    allServersForm.v2pCheckReportDataGridView.Rows[_rowIndex].Cells[HOSTNAME_COLUMN].Value = h.displayname;
                    allServersForm.v2pCheckReportDataGridView.Rows[_rowIndex].Cells[CHECK_COLUMN].Value = "Fx running";
                    allServersForm.v2pCheckReportDataGridView.Rows[_rowIndex].Cells[RESULT_COLUMN].Value = _failed;
                    allServersForm.v2pCheckReportDataGridView.Rows[_rowIndex].Cells[ACTION_COLUMN].Value = "Fx service is not reported status to CX for more than 15 minutes.";
                    _rowIndex++;
                }
                if (h.vx_agent_heartbeat == false && h.vxagentInstalled == true)
                {
                    allServersForm.v2pCheckReportDataGridView.Rows.Add(1);
                    allServersForm.v2pCheckReportDataGridView.Rows[_rowIndex].Cells[HOSTNAME_COLUMN].Value = h.displayname;
                    allServersForm.v2pCheckReportDataGridView.Rows[_rowIndex].Cells[CHECK_COLUMN].Value = "Vx running";
                    allServersForm.v2pCheckReportDataGridView.Rows[_rowIndex].Cells[RESULT_COLUMN].Value = _failed;
                    allServersForm.v2pCheckReportDataGridView.Rows[_rowIndex].Cells[ACTION_COLUMN].Value = "Vx service is not reported status to CX for more than 15 minutes.";
                    _rowIndex++;

                }

            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            return true;
        }

    }
}
