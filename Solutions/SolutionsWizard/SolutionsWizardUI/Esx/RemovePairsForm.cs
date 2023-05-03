using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.IO;
using System.Windows.Forms;
using com.InMage.ESX;
using com.InMage.Tools;
using System.Diagnostics;
using System.Xml;

namespace com.InMage.Wizard
{
    
    public partial class RemovePairsForm : Form
    {
        string _esxIP = "";
        string _esxUserName = "";
        string _esxPassword = "";
        Esx _esxInfo = new Esx();
        public static int PLAN_NAME_COLUMN               = 0;
        public static int HOST_NAME_COLUMN               = 1;
        public static int DISPLAYNAME_COLUMN             = 2;
        public static int MASTER_TARGET_HOST_NAME_COLUMN = 3;
        public static int REMOVE_COLUMN                  = 4;
        bool _downLoadFile = false;
        public HostList _sourceHostList = new HostList();
        public HostList _removeList = new HostList();
        public Host _masterHost = new Host();
        string _cxIP = "";
        bool _firstTimeCheckingMasterTarget               = false;
        public string MASTER_FILE;
        public int _returnValue = 0;
        string _nodeList = null;
        public int _returnCodeofDetach = 0;
        public bool _slideOpen = false;
        public string _installPath = "";
        public string _mtName = null;
        public RemovePairsForm()
        {
            InitializeComponent();
            try
            {
                _installPath = WinTools.fxAgentPath() + "\\vContinuum";

                nextButton.Text = "Remove";
                helpContentLabel.Text = HelpForCx.REMOVE_SCREEN_1;
                versionLabel.Text = HelpForCx.VERSION_NUMBER;
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

        private void cancelButton_Click(object sender, EventArgs e)
        {
            this.Close();
        }

        private void getDetailsButton_Click(object sender, EventArgs e)
        {
            try
            {
                if (Validate() == true)
                {
                    _esxIP = targetEsxIPText.Text;
                    _esxUserName = targetEsxUserNameTextBox.Text;
                    _esxPassword = targetEsxPasswordTextBox.Text;
                    progressBar.Visible = true;
                    selectAllCheckBox.Visible = false;
                    getDetailsButton.Enabled = false;
                    _firstTimeCheckingMasterTarget = false;
                    _removeList = new HostList();
                    selectAllCheckBox.Checked = false;
                    nextButton.Enabled = false;
                    MASTER_FILE = "ESX_Master_" + _esxIP + ".xml";
                    try
                    {
                        if (File.Exists(MASTER_FILE))
                        {
                            File.Delete(MASTER_FILE);
                        }
                    }
                    catch (Exception ex)
                    {

                        System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                        Trace.WriteLine("ERROR*******************************************");
                        Trace.WriteLine(DateTime.Now + "Caught exception at Method: PostJobAutomation  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                        Trace.WriteLine("Exception  =  " + ex.Message);
                        Trace.WriteLine("ERROR___________________________________________");

                        //return false;
                    }
                    removeDetailsBackgroundWorker.RunWorkerAsync();


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

        public bool Validate()
        {
            if (targetEsxIPText.Text.Length == 0)
            {
                MessageBox.Show("Please enter IP address", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return false;

            }
            if (targetEsxUserNameTextBox.Text.Length == 0)
            {
                MessageBox.Show("Please enter username", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return false;
            }
            if (targetEsxPasswordTextBox.Text.Length == 0)
            {
                MessageBox.Show("Please enter password", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return false;
            }
            
            return true;

        }

        private void removeDetailsBackgroundWorker_DoWork(object sender, DoWorkEventArgs e)
        {
            try
            {
                _returnValue = _esxInfo.DownloadFile(_esxIP, _esxUserName, _esxPassword, MASTER_FILE);
                if (_returnValue == 0)
                {
                    _downLoadFile = true;

                }
                else
                {
                    _downLoadFile = false;
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

        private void removeDetailsBackgroundWorker_ProgressChanged(object sender, ProgressChangedEventArgs e)
        {
            progressBar.Value = e.ProgressPercentage;
        }

        private void removeDetailsBackgroundWorker_RunWorkerCompleted(object sender, RunWorkerCompletedEventArgs e)
        {
            try
            {

                if (_downLoadFile == true)
                {
                    SolutionConfig config = new SolutionConfig();
                    _sourceHostList = new HostList();
                    _masterHost = new Host();
                    config.ReadXmlConfigFile(MASTER_FILE, ref _sourceHostList, ref _masterHost, ref _esxIP, ref _cxIP);


                    int hostcount = _sourceHostList._hostList.Count;
                    removeDataGridView.Rows.Clear();
                    int rowIndex = 0;
                    if (hostcount > 0)
                    {
                        foreach (Host h in _sourceHostList._hostList)
                        {
                            removeDataGridView.Rows.Add(1);
                            removeDataGridView.Rows[rowIndex].Cells[PLAN_NAME_COLUMN].Value = h.plan;
                            removeDataGridView.Rows[rowIndex].Cells[HOST_NAME_COLUMN].Value = h.hostName;
                            removeDataGridView.Rows[rowIndex].Cells[DISPLAYNAME_COLUMN].Value = h.displayName;
                            removeDataGridView.Rows[rowIndex].Cells[MASTER_TARGET_HOST_NAME_COLUMN].Value = h.masterTargetHostName;

                            rowIndex++;
                        }
                        progressBar.Visible = false;
                        getDetailsButton.Enabled = true;
                        removeDataGridView.Visible = true;
                    }
                    else
                    {
                        Debug.WriteLine("There are no nodes found in MasterConfigFile.xml.");
                        MessageBox.Show("There are no VMS found on " + _esxIP + " to remove", "Remove", MessageBoxButtons.OK, MessageBoxIcon.Information);
                        progressBar.Visible = false;
                        getDetailsButton.Enabled = true;


                    }


                }
                else
                {
                    if (_returnValue == 3)
                    {
                        MessageBox.Show("Could not connect to the ESX server. Please check IP and credentials", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    }
                    else
                    {

                        Debug.WriteLine("Unable to download MasterConfigFile.xml from secondary ESX");
                        MessageBox.Show("No protected VMs are available on ESX: " + _esxIP, "Error",MessageBoxButtons.OK, MessageBoxIcon.Error);
                    }
                    progressBar.Visible = false;
                    getDetailsButton.Enabled = true;

                }
            }
            catch (Exception ex)
            {

                
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");


            }
        }

        private void removeDataGridView_CurrentCellDirtyStateChanged(object sender, EventArgs e)
        {
            if (removeDataGridView.RowCount > 0)
            {
                //WE need this commit so that CellValueChanged event handler to do the actual handling
                // This commit is essential for CellValue changed event handler to work. 
                removeDataGridView.CommitEdit(DataGridViewDataErrorContexts.Commit);
            }

        }

        private void removeDataGridView_CellValueChanged(object sender, DataGridViewCellEventArgs e)
        {
            try
            {
                Host h = new Host();
                if (e.RowIndex >= 0)
                {
                    if (removeDataGridView.RowCount > 0)
                    {
                        if (removeDataGridView.Rows[e.RowIndex].Cells[REMOVE_COLUMN].Selected == true)
                        {
                            h.hostName = removeDataGridView.Rows[e.RowIndex].Cells[HOST_NAME_COLUMN].Value.ToString();
                            int index = 0;
                            _sourceHostList.DoesHostExist(h, ref index);
                            if ((bool)removeDataGridView.Rows[e.RowIndex].Cells[REMOVE_COLUMN].FormattedValue)
                            {
                                if (_removeList._hostList.Count == 0)
                                {
                                    _firstTimeCheckingMasterTarget = false;
                                }
                                if (_firstTimeCheckingMasterTarget == false)
                                {
                                    selectAllCheckBox.Visible = true;
                                    _mtName = removeDataGridView.Rows[e.RowIndex].Cells[MASTER_TARGET_HOST_NAME_COLUMN].Value.ToString();
                                    _firstTimeCheckingMasterTarget = true;
                                    _removeList.AddOrReplaceHost((Host)_sourceHostList._hostList[index]);
                                    nextButton.Enabled = true;
                                    // _removeList.Print();
                                }
                                for (int i = 0; i < removeDataGridView.RowCount; i++)
                                {
                                    if ((bool)removeDataGridView.Rows[i].Cells[REMOVE_COLUMN].FormattedValue)
                                    {
                                        if (removeDataGridView.Rows[i].Cells[MASTER_TARGET_HOST_NAME_COLUMN].Value.ToString() != removeDataGridView.Rows[e.RowIndex].Cells[MASTER_TARGET_HOST_NAME_COLUMN].Value.ToString())
                                        {
                                            removeDataGridView.Rows[e.RowIndex].Cells[REMOVE_COLUMN].Value = false;
                                            removeDataGridView.RefreshEdit();
                                            MessageBox.Show("Choose VMs from only one Master Target ", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                            return;
                                        }
                                        else
                                        {
                                            _removeList.AddOrReplaceHost((Host)_sourceHostList._hostList[index]);
                                            //_removeList.Print();
                                        }
                                    }
                                }
                                nextButton.Enabled = true;
                            }
                            else
                            {
                                _removeList.RemoveHost((Host)_sourceHostList._hostList[index]);
                                if (_removeList._hostList.Count == 0)
                                {
                                    _firstTimeCheckingMasterTarget = false;
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
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");


            }
            
        }

        private void nextButton_Click(object sender, EventArgs e)
        {

            try
            {
                Remove clean = new Remove();
                Host h1 = new Host();
                bool ranAtleastOnce = false;
                _nodeList = null;
                foreach (Host h in _removeList._hostList)
                {
                    h1 = h;
                    h1.delete = true;
                    if (h1.masterTargetHostName != null)
                    {
                        //  if (h1.failOver != "yes")
                        {
                            switch (MessageBox.Show("Are you sure that you want to remove protection for " + h.hostName + "? You will not be able to recover VM if you remove protection.", "Remove protection", MessageBoxButtons.YesNo, MessageBoxIcon.Question))
                            {
                                case DialogResult.Yes:
                                    this.Refresh();
                                    logTextBox.AppendText("Removing the replication pairs of " + h1.displayName + Environment.NewLine);
                                    // Step1.Removing FX and VX pairs from the CX
                                    if (clean.RemoveReplication(ref h1, h1.masterTargetHostName, h.plan, WinTools.getCxIpWithPortNumber(),h.vConName) == false)
                                    {
                                        logTextBox.AppendText("Failed to delete replication pairs" + Environment.NewLine);
                                    }
                                    else
                                    {
                                        logTextBox.AppendText("Removed the replication pairs of " + h1.displayName + Environment.NewLine);
                                        //Step2: Remove entries from Master.XML file
                                        XmlDocument documentEsx = new XmlDocument();
                                        XmlDocument documentMasterEsx = new XmlDocument();
                                        string esxXmlFile = _installPath + "\\" + MASTER_FILE;
                                        documentEsx.Load(esxXmlFile);
                                        XmlNodeList hostNodesEsxXml = null;
                                        hostNodesEsxXml = documentEsx.GetElementsByTagName("host");
                                        foreach (XmlNode esxnode in hostNodesEsxXml)
                                        {
                                            if (esxnode.Attributes["display_name"].Value == h1.displayName)
                                            {
                                                if (esxnode.ParentNode.Name == "SRC_ESX")
                                                {
                                                    ranAtleastOnce = true;
                                                    XmlNode parentNode = esxnode.ParentNode;
                                                    esxnode.ParentNode.RemoveChild(esxnode);
                                                    if (h.failOver != "yes")
                                                    {
                                                        if (_nodeList == null)
                                                        {
                                                            h.Print();
                                                            Debug.WriteLine("Printing display name" + h.displayName);
                                                            _nodeList = h.hostName;
                                                        }
                                                        else
                                                        {
                                                            Debug.WriteLine("Printing display name" + h.displayName);
                                                            _nodeList = _nodeList + "!@!@!" + h.hostName;
                                                        }
                                                    }
                                                    if (!parentNode.HasChildNodes)
                                                    {
                                                        parentNode.ParentNode.ParentNode.RemoveChild(parentNode.ParentNode);
                                                    }
                                                    break;
                                                }
                                            }

                                        }
                                        // Debug.WriteLine(esxXmlFile);
                                        documentEsx.Save(esxXmlFile);
                                    }

                                    /*  else
                                      {
                                          logTextBox.AppendText("Failed to removed the replication pairs of " + h1.displayName + Environment.NewLine);
                                          MessageBox.Show("Failed to delete pairs from the Cx ", "No paris found", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                          this.Refresh();
                                      }*/
                                    break;
                                case DialogResult.No:
                                    break;
                            }
                        }
                        /* Else if(h1.failOver == "yes")

                          {
                              logTextBox.AppendText("Removing the replication pairs of " + h1.displayName + Environment.NewLine);
                              if (clean.RemoveReplication(ref h1, h1.masterTargetHostName, h.plan, WinTools.getCxIpWithPortNumber()) == true)
                              {
                                  logTextBox.AppendText("Removed the replication pairs of " + h1.displayName + Environment.NewLine);

                              }

                          }
                         */

                    }
                }
                //Step3: Run detach command to detach disks from master target.
                //       If atlease one machine is removed from master xml( where ranAtleastOnce is set to true), we will call detach
                //      Detach script depends upon the recovery database file. Entried from recovery database are removed during failover.
                if (ranAtleastOnce == true)
                {
                    Debug.WriteLine("Uploading MasterConfigFile.xml to ESX host");
                    logTextBox.AppendText("Modifying necessary files.This may take few seconds" + Environment.NewLine);
                    Esx esxObj = new Esx();
                    //logTextBox.AppendText("Modifying necessary files:Completed" + Environment.NewLine);
                    // Debug.WriteLine("Modifying necessary files:Completed");

                    // logTextBox.AppendText("Detaching Vmdk(s) from Master target.This may take few seconds"+ Environment.NewLine);
                    // Debug.WriteLine("Detaching Vmdk(s) from Master target.This may take few seconds");
                    progressBar.Visible = true;
                    nextButton.Enabled = false;
                    detachBackgroundWorker.RunWorkerAsync();
                }
            }
            catch (Exception ex)
            {               
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
            // _removeList.Print();
        }

        private void doneButton_Click(object sender, EventArgs e)
        {
            Close();
        }

        private void clearLogsLinkLabel_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
        {
            logTextBox.Text = null;
        }

        private void detachBackgroundWorker_DoWork(object sender, DoWorkEventArgs e)
        {
            try
            {
                Esx esxObj = new Esx();
                esxObj.UploadFile(_esxIP, _esxUserName, _esxPassword, MASTER_FILE);
                SolutionConfig solution = new SolutionConfig();
                Host h1 = (Host)_removeList._hostList[0];
                _masterHost.displayName = h1.displayName;
                _masterHost.hostName = h1.hostName;
                HostList masterList = new HostList();
                masterList.AddOrReplaceHost(_masterHost);
                if (solution.WriteXmlFile(_removeList, masterList, esxObj, WinTools.getCxIp(), "Remove.xml", "Remove", false) == true)
                {
                    Trace.WriteLine(DateTime.Now + " \t Preparing the Recovery.xml for remove operation is succreefully done");
                }
                else
                {
                    //_message = "Detach the disks manually from master target. " + Environment.NewLine;
                    // logTextBox.Invoke(tickerDelegate, new object[] { _message });
                    Trace.WriteLine(DateTime.Now + "\t Failed to prepare the Recovery.xml ");
                    return;
                }
                if (_nodeList != null)
                {
                    _returnCodeofDetach = esxObj.DetachDisksFromMasterTarget(_esxIP, _esxUserName, _esxPassword);
                }
                else
                {
                    Trace.WriteLine(DateTime.Now + "\t detachBackgroundWorker_DoWork: No VMs available to detach from master target");
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

        private void detachBackgroundWorker_ProgressChanged(object sender, ProgressChangedEventArgs e)
        {
            progressBar.Value = e.ProgressPercentage;
        }

        private void detachBackgroundWorker_RunWorkerCompleted(object sender, RunWorkerCompletedEventArgs e)
        {
            // logTextBox.AppendText("Successfully DetachedVM(s) from " + h1.masterTargetHostName + Environment.NewLine);
            //Debug.WriteLine("Successfully DetachedVM(s) from " + h1.masterTargetHostName);

            try
            {
                Trace.WriteLine(DateTime.Now + "Entered: RemovePairsForms::detachBackgroundWorker_RunWorkerCompleted Node List = " + _nodeList);                
                if (_returnCodeofDetach == 0)
                {
                    if (_nodeList != null)
                    {
                        logTextBox.AppendText("Delete following VM(s) from ESX:" + _esxIP + Environment.NewLine);
                        Trace.WriteLine("You can delete following VM(s) from ESX:" + _esxIP);
                        string[] hostnameList = _nodeList.Split(new string[] { "!@!@!" }, StringSplitOptions.None);
                        foreach (string value in hostnameList)
                        {
                            foreach (Host h in _removeList._hostList)
                            {
                                if (value == h.hostName)
                                {
                                    logTextBox.AppendText(h.displayName + Environment.NewLine);
                                    Debug.WriteLine(h.new_displayname);
                                }
                            }
                        }
                    }
                }
                else
                {
                    logTextBox.AppendText("Unable to Detach disks from Master target" + Environment.NewLine);
                    Trace.WriteLine(DateTime.Now + "\t Unable to Detach disks from Master target. Return code = " + _returnCodeofDetach + Environment.NewLine);
                    //logTextBox.AppendText("Remove operation is completed" + Environment.NewLine);
                }
                logTextBox.AppendText("Remove operation is completed" + Environment.NewLine);
                progressBar.Visible = false;
                nextButton.Enabled = true;
                nextButton.Visible = false;
                cancelButton.Visible = false;
                doneButton.Visible = true;
            }
            catch (Exception ex)
            {               
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void helpPictureBox_Click(object sender, EventArgs e)
        {
            try
            {
                if (_slideOpen == false)
                {
                    helpPanel.BringToFront();
                    helpPanel.Visible = true;
                    _slideOpen = true;
                }
                else
                {
                    _slideOpen = false;
                    helpPanel.Visible = false;
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
            helpPanel.Visible = false;
            _slideOpen = false;
        }

        private void esxProtectionLinkLabel_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
        {
           // Process.Start("http://" + WinTools.getCxIpWithPortNumber() + "/help/Content/ESX Solution/Remove Protection.htm");
            if (File.Exists(_installPath + "\\Manual.chm"))
            {
                Help.ShowHelp(null, _installPath + "\\Manual.chm", HelpNavigator.TopicId, "73");
            }
        }

        private void selectAllCheckBox_CheckedChanged(object sender, EventArgs e)
        {
            try
            {
                Host h = new Host();
                if (_removeList._hostList.Count != 0)
                {
                    if (selectAllCheckBox.Checked == true)
                    {
                        if (_mtName != null)
                        {
                            for (int i = 0; i < removeDataGridView.RowCount; i++)
                            {
                                if (_mtName == removeDataGridView.Rows[i].Cells[MASTER_TARGET_HOST_NAME_COLUMN].Value.ToString())
                                {
                                    h.hostName = removeDataGridView.Rows[i].Cells[HOST_NAME_COLUMN].Value.ToString();
                                    int index = 0;
                                    _sourceHostList.DoesHostExist(h, ref index);
                                    removeDataGridView.Rows[i].Cells[REMOVE_COLUMN].Value = true;
                                    removeDataGridView.RefreshEdit();
                                    _removeList.AddOrReplaceHost((Host)_sourceHostList._hostList[index]);
                                }
                            }
                        }
                    }
                    else
                    {
                        for (int i = 0; i < removeDataGridView.RowCount; i++)
                        {
                            h.hostName = removeDataGridView.Rows[i].Cells[HOST_NAME_COLUMN].Value.ToString();
                            int index = 0;
                            _sourceHostList.DoesHostExist(h, ref index);
                            removeDataGridView.Rows[i].Cells[REMOVE_COLUMN].Value = false;
                            removeDataGridView.RefreshEdit();
                            _removeList.AddOrReplaceHost((Host)_sourceHostList._hostList[index]);
                        }
                        _firstTimeCheckingMasterTarget = false;
                        selectAllCheckBox.Visible = false;
                        _mtName = null;
                    }
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: selectAllCheckBox_CheckedChanged " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }        
    }
}
