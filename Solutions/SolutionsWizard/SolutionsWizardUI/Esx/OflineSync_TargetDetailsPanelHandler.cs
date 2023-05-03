using System;
using System.Collections.Generic;
using System.Collections;
using System.Text;
using System.Diagnostics;
using Com.Inmage.Esxcalls;
using System.IO;
using System.Windows.Forms;
using System.Threading;
using System.Xml;
using Com.Inmage.Tools;
using System.Drawing;


namespace Com.Inmage.Wizard
{
    class OfflineSync_TargetDetailsPanelHandler : OfflineSyncPanelHandler
    {
        internal static int returnCode = 0;
        internal static bool returnValue = false;
        internal string vmsRestoredDataStorName = "";
        internal static long TimeOutMilliSeconds = 360000;
        bool targetESXPreChekCompleted = false;
        internal bool successFullyMadeFile = false;
        string message = null;
       // public int _returnCode = 0;
        private delegate void TickerDelegate(string s);
       
        TickerDelegate tickerDelegate;
        TextBox textBoxForBackGround = new TextBox();
        public OfflineSync_TargetDetailsPanelHandler()
        {

        }

        internal override bool Initialize(ImportOfflineSyncForm importOfflineSyncForm)
        {           
            return true;
        }

        internal override bool ProcessPanel(ImportOfflineSyncForm importOfflineSyncForm)
        {
            return true;
        }
        internal override bool ValidatePanel(ImportOfflineSyncForm importOfflineSyncForm)
        {
            try
            {
                if (importOfflineSyncForm.selSourceList._hostList.Count > 0)
                {

                }
                else
                {
                    MessageBox.Show("There are no VMs to import", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return false;
                }
                if (importOfflineSyncForm.masterHostComboBox.SelectedItem == null)
                {
                    MessageBox.Show("Select master targets host and datastore.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return false;
                }
                if (importOfflineSyncForm.masterTargetDataStoreComboBox.SelectedItem == null)
                {
                    MessageBox.Show("Select master targets datastore form drop downlist", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
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
        internal override bool CanGoToNextPanel(ImportOfflineSyncForm importOfflineSyncForm)
        {
           
            return true;

        }

        internal override bool CanGoToPreviousPanel(ImportOfflineSyncForm importOfflineSyncForm)
        {
            
            return true;

        }

        /*
         * When user clicks on get details we will validate the text boxes whether they are empty or fill by values.
         */
        internal bool GetEsxMasterFile(ImportOfflineSyncForm importOfflineSyncForm)
        {
            try
            {
                importOfflineSyncForm.progressBar.Visible = true;

                if (importOfflineSyncForm.targetIpText.Text.Length == 0)
                {
                    MessageBox.Show("Please enter secondary vSphere server IP", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return false;
                }
                if (importOfflineSyncForm.targetUserNameText.Text.Length == 0)
                {
                    MessageBox.Show("Please enter secondary vSphere server username", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return false;
                }
                if (importOfflineSyncForm.targetPassWordText.Text.Length == 0)
                {
                    MessageBox.Show("Please enter secondary vSphere server password", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return false;
                }
                successFullyMadeFile = false;

                importOfflineSyncForm.masterTargetDataStoreComboBox.Enabled = true;
                importOfflineSyncForm.masterHostComboBox.Enabled = true;
                try
                {
                    importOfflineSyncForm.esxIPused = importOfflineSyncForm.targetIpText.Text.Trim();
                    importOfflineSyncForm.esxsUserName = importOfflineSyncForm.targetUserNameText.Text;
                    importOfflineSyncForm.esxsPassword = importOfflineSyncForm.targetPassWordText.Text;
                    importOfflineSyncForm.masterHostComboBox.Items.Clear();
                    

                    if (importOfflineSyncForm.masterHost.esxIp == importOfflineSyncForm.esxIPused)
                    {
                        importOfflineSyncForm.masterHost.esxUserName = importOfflineSyncForm.esxsUserName;
                        importOfflineSyncForm.masterHost.esxPassword = importOfflineSyncForm.esxsPassword;
                    }

                    importOfflineSyncForm.getTargetDetailsBackgroundWorker.RunWorkerAsync();
                }
                catch (Exception ex)
                {
                    System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                    Trace.WriteLine("ERROR*******************************************");
                    Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                    Trace.WriteLine("Exception  =  " + ex.Message);
                    Trace.WriteLine("ERROR___________________________________________");

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

        /*
         * This method will call the GetInfo.pl and will get the Info.xml to laest folder.
         */
        internal bool GetOfflineSyncXmlMasterXml(ImportOfflineSyncForm importOfflineSyncForm)
        {
            try
            {
                Esx esx = new Esx();                
                importOfflineSyncForm.esxglobal = esx;
                //Gettting OffLineSyncXml file from secondary ESX box
                Host h = (Host)ResumeForm.selectedVMListForPlan._hostList[0];
                
                importOfflineSyncForm.masterHost = (Host)ResumeForm.masterTargetList._hostList[0];
                importOfflineSyncForm.selSourceList = ResumeForm.selectedVMListForPlan;

                returnCode = importOfflineSyncForm.esxglobal.GetEsxInfo(importOfflineSyncForm.esxIPused, Role.Primary, h.os);
                Trace.WriteLine(DateTime.Now + " \t Came here after successfully havinfg return code " + returnCode.ToString());
                importOfflineSyncForm.masterHost.esxIp = importOfflineSyncForm.esxIPused;
                importOfflineSyncForm.masterHost.esxUserName = importOfflineSyncForm.esxsUserName;
                importOfflineSyncForm.masterHost.esxPassword = importOfflineSyncForm.esxsPassword;
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
           
        /*
         * This is for Loading the values into datagridview and need to 
         * Fill the vSphere host in the Combobox to display user to selet the target host.
         */
        internal bool LoadIntoDataGridView(ImportOfflineSyncForm importOfflineSyncForm)
        {
            try
            {
                int rowIndex = 0;
                Trace.WriteLine(DateTime.Now + " \t Entered : LoadIntoDataGridView of OfflineSync_TargetPanelHandler:");
                Trace.WriteLine(DateTime.Now + " \t Printing the count of SourceList " + importOfflineSyncForm.selSourceList._hostList.Count);
                if (importOfflineSyncForm.selSourceList._hostList.Count > 0)
                {
                   
                    importOfflineSyncForm.targetServerDataGridView.RowCount = importOfflineSyncForm.selSourceList._hostList.Count;
                    foreach (Host h in importOfflineSyncForm.selSourceList._hostList)
                    {
                        h.targetESXUserName = importOfflineSyncForm.esxsUserName;
                        h.targetESXPassword = importOfflineSyncForm.esxsPassword;
                        if (h.esxIp != null)
                        {
                            importOfflineSyncForm.targetServerDataGridView.Rows[rowIndex].Cells[0].Value = h.esxIp;
                        }
                        else
                        {
                            importOfflineSyncForm.targetServerDataGridView.Rows[rowIndex].Cells[0].Value = "N/A";
                        }
                        importOfflineSyncForm.targetServerDataGridView.Rows[rowIndex].Cells[1].Value = h.displayname;
                        importOfflineSyncForm.targetServerDataGridView.Rows[rowIndex].Cells[2].Value = h.ip;
                        rowIndex++;
                    }
                    importOfflineSyncForm.targetServerDataGridView.Visible = true;
                    importOfflineSyncForm.hostToImportLabel.Visible = true;
                    importOfflineSyncForm.nextButton.Text = "Import";
                }
                else
                {
                    MessageBox.Show("There are no VMs to import", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return false;
                }
                //Updating MasterHostname textbox and otehr vaues.
                importOfflineSyncForm.masterTargetTextBox.Text = importOfflineSyncForm.masterHost.displayname;
                importOfflineSyncForm.masterTargetTextBox.Visible = true;
                importOfflineSyncForm.masterTargetLabel.Visible = true;
                importOfflineSyncForm.masterHostComboBox.Visible = true;
                importOfflineSyncForm.masterHostComboBox.Items.Clear();
                importOfflineSyncForm.masterTargetDataStoreComboBox.Visible = true;
                importOfflineSyncForm.masterTargetDataStoreLabel.Visible = true;
                
                // importOfflineSyncForm.sourceVMsRestoredDataStoreComboBox.Items.Clear();
                
                importOfflineSyncForm.taregtHostLabel.Visible = true;
                if (returnCode == 0)
                {
                    Trace.WriteLine(DateTime.Now + " \t Printing the count of importOfflineSyncForm._esx.GetHostList() " + importOfflineSyncForm.esxglobal.GetHostList.Count);
                    //Get datastore informarion for any of the host in the hostlist. Every host has datastore list in it.
                    if (importOfflineSyncForm.esxglobal.GetHostList.Count > 0)
                    {
                        Host h1 = (Host)importOfflineSyncForm.esxglobal.GetHostList[0];
                        importOfflineSyncForm.masterTargetTextBox.Text = importOfflineSyncForm.masterHost.displayname;

                        importOfflineSyncForm.masterHostComboBox.Items.Clear();
                        if (h1.vCenterProtection == "No")
                        {
                            importOfflineSyncForm.masterHostComboBox.Items.Add(h1.vSpherehost);
                            importOfflineSyncForm.masterHostComboBox.SelectedItem = h1.vSpherehost;

                        }
                        else
                        {
                            ArrayList hostList = new ArrayList();
                            foreach (DataStore h in importOfflineSyncForm.esxglobal.dataStoreList)
                            {
                                if (hostList.Count == 0)
                                {
                                    hostList.Add(h.vSpherehostname);
                                }
                                else
                                {
                                    bool hostExists = false;
                                    foreach (string s in hostList)
                                    {
                                        if (s == h.vSpherehostname)
                                        {
                                            hostExists = true;
                                        }
                                    }
                                    if (hostExists == false)
                                    {
                                        hostList.Add(h.vSpherehostname);
                                    }
                                }
                            }
                            foreach (string s in hostList)
                            {
                                importOfflineSyncForm.masterHostComboBox.Items.Add(s);
                            }
                            if (importOfflineSyncForm.masterHostComboBox.Items.Count == 1)
                            {
                                importOfflineSyncForm.masterHostComboBox.SelectedItem = h1.vSpherehost;
                            }
                        }
                    }
                    else
                    {
                        ArrayList hostList = new ArrayList();
                        foreach (DataStore h in importOfflineSyncForm.esxglobal.dataStoreList)
                        {
                            if (hostList.Count == 0)
                            {
                                hostList.Add(h.vSpherehostname);
                            }
                            else
                            {
                                bool hostExists = false;
                                foreach (string s in hostList)
                                {
                                    if (s == h.vSpherehostname)
                                    {
                                        hostExists = true;
                                    }
                                }
                                if (hostExists == false)
                                {
                                    hostList.Add(h.vSpherehostname);
                                }
                            }
                        }
                        foreach (string s in hostList)
                        {
                            importOfflineSyncForm.masterHostComboBox.Items.Add(s);
                        }
                    }                   
                }
                else
                {
                    if (returnCode == 3)
                    {
                        MessageBox.Show("Could not retrieve guest info from vSphere server. Please check that IP address and credentials are correct.", "ESX credential error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        //    MessageBox.Show("There are no VMs found for import", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    }
                    else
                    {
                        MessageBox.Show("Failed to fetch the information from the secondary vSphere server", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    }
                    return false;
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }

            return true;
        }


        /*
         * As of now we removed the logic of where The mt is stored in the target vSPhere/vCenter server.
         */
        internal bool readMasterTargetDataStore(ImportOfflineSyncForm importOfflineSyncForm)
        {
            Trace.WriteLine(DateTime.Now + " \t Entered: readMasterTargetDataStore of OfflineSync_TargetPanelHandler");

            try
            {
                //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
                //based on above comment we have added xmlresolver as null
                XmlDocument document = new XmlDocument();
                document.XmlResolver = null;
                XmlNodeList esxNodes = null;

                if (File.Exists(importOfflineSyncForm.installedPath + "\\ESX_Master_Offlinesync.xml"))
                {
                    //StreamReader reader = new StreamReader(importOfflineSyncForm.installedPath + "\\ESX_Master_Offlinesync.xml");
                    //string xmlString = reader.ReadToEnd();
                    XmlReaderSettings settings = new XmlReaderSettings();
                    settings.ProhibitDtd = true;
                    settings.XmlResolver = null;
                    //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
                    using (XmlReader reader1 = XmlReader.Create(importOfflineSyncForm.installedPath + "\\ESX_Master_Offlinesync.xml", settings))
                    {
                        document.Load(reader1);
                       // reader1.Close();
                    }
                    //document.Load(importOfflineSyncForm.installedPath + "\\ESX_Master_Offlinesync.xml");
                    //reader.Close();
                    esxNodes = document.GetElementsByTagName("root");
                    foreach (XmlNode node in esxNodes)
                    {
                        XmlAttributeCollection attribColl = node.Attributes;
                        if (node.Attributes["VMsImportedTo"] != null)
                        {
                            vmsRestoredDataStorName = attribColl.GetNamedItem("VMsImportedTo").Value.ToString().Replace("[", "").Replace("]", "");
                        }
                        else if (node.Attributes["VMsImportedTo"].ToString() == "")
                        {
                            return false;
                        }
                        else
                        {
                            return false;
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
                return false;

            }

            Trace.WriteLine(DateTime.Now + " \t Exiting: readMasterTargetDataStore of OfflineSync_TargetPanelHandler");
            return true;
        }


        /*
         * When user clicks on the Next button this will be called First.
         * This will crate esx.xml in Latest folder.
         * Followed by that readiness checks, Import Script and merging the file ESX.xml to MasterConfigFile.xml.
         */
        internal bool OfflineSyncImport(ImportOfflineSyncForm importOfflineSyncForm)
        {



            SolutionConfig config1 = new SolutionConfig();
            string targetEsxIP="", cxIp="";

            try
            {

                if (successFullyMadeFile == false)
                {

                    if (importOfflineSyncForm.masterHostComboBox.Text.Length == 0)
                    {

                        MessageBox.Show("Please select the datastore where primary VMS are copied to", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return false;

                    }
                    else
                    {

                        importOfflineSyncForm.hostWhereMasterTargetRestored = importOfflineSyncForm.masterHostComboBox.SelectedItem.ToString();

                    }
                    if (importOfflineSyncForm.masterTargetDataStoreComboBox.Text.Length == 0)
                    {
                        MessageBox.Show("Please select a data store where master target is copied to", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return false;
                    }
                    else
                    {

                        importOfflineSyncForm.dataStoreWhereMTisThere = importOfflineSyncForm.masterTargetDataStoreComboBox.SelectedItem.ToString();


                    }
                    importOfflineSyncForm.cxIPused = WinTools.GetcxIP();
                    string planName = "";


                    /* foreach (Host h in importOfflineSyncForm._selectedSourceList._hostList)
                     {
                         h.targetDataStore = importOfflineSyncForm.dataStoreWhereVMsRestored;
                     }

                     importOfflineSyncForm.masterHost.datastore = importOfflineSyncForm.hostWhereMasterTargetRestored;
                     string s = importOfflineSyncForm.masterHost.vmx_path.Replace(importOfflineSyncForm._tempDataStore, importOfflineSyncForm.hostWhereMasterTargetRestored);
                     *
                     importOfflineSyncForm.masterHost.vmx_path = s;*/
                    importOfflineSyncForm.masterHost.esxIp = importOfflineSyncForm.esxIPused;
                    importOfflineSyncForm.masterHost.esxUserName = importOfflineSyncForm.esxsUserName;
                    importOfflineSyncForm.masterHost.esxPassword = importOfflineSyncForm.esxsPassword;
                    importOfflineSyncForm.masterHost.vSpherehost = importOfflineSyncForm.hostWhereMasterTargetRestored;
                    importOfflineSyncForm.masterHost.datastore = importOfflineSyncForm.dataStoreWhereMTisThere;
                    importOfflineSyncForm.masterHost.machineType = "VirtualMachine";
                    importOfflineSyncForm.finMasterList.AddOrReplaceHost(importOfflineSyncForm.masterHost);

                    foreach (Host h in importOfflineSyncForm.selSourceList._hostList)
                    {
                        h.targetESXIp = importOfflineSyncForm.esxIPused;
                        h.targetESXUserName = importOfflineSyncForm.esxsUserName;
                        h.targetESXPassword = importOfflineSyncForm.esxsPassword;
                        h.offlineSync = false;

                        try
                        {
                            Trace.WriteLine(DateTime.Now + "\t Printing memsize  " + h.memSize.ToString());
                            if (h.memSize == 0)
                            {
                                h.memSize = 4;
                            }
                            if (h.memSize % 4 != 0)
                            {
                                int remainder = h.memSize % 4;
                                h.memSize = h.memSize - remainder;
                                Trace.WriteLine(DateTime.Now + "\t Printing memsize  afeter remove " + remainder.ToString());
                            }
                        }
                        catch (Exception ex)
                        {
                            Trace.WriteLine(DateTime.Now + "\t Failed to update ram size " + ex.Message);
                        }
                    }


                    SolutionConfig config = new SolutionConfig();
                    config.WriteXmlFile(importOfflineSyncForm.selSourceList, importOfflineSyncForm.finMasterList, importOfflineSyncForm.esxglobal, importOfflineSyncForm.cxIPused, "ESX.xml", planName, false);



                    //Modifying the entries in esx.xml file to proceed for setting up the jobs
                    //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
                    //based on above comment we have added xmlresolver as null
                    XmlDocument document = new XmlDocument();
                    document.XmlResolver = null;
                    string xmlfile = WinTools.FxAgentPath() + "\\vContinuum\\Latest" + "\\ESX.xml";

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
                   // document.Load(xmlfile);
                    //reader.Close();
                    //XmlNodeList hostNodes = null;
                    XmlNodeList hostNodes = null;
                    XmlNodeList diskNodes = null;
                    hostNodes = document.GetElementsByTagName("host");
                    diskNodes = document.GetElementsByTagName("disk");
                    foreach (XmlNode node in diskNodes)
                    {
                        node.Attributes["protected"].Value = "no";
                    }

                    document.Save(xmlfile);
                    successFullyMadeFile = true;

                }
                runReadynesscheckAndTargetScript(importOfflineSyncForm);
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            
            return true;
        
        
        
        
        }
        /*
         * This  method will be called by tge method OfflineSyncImport after creating the ESX.xml in latest folder.
         * This will run the readinesscheks for the selected vms on targte side.
         * Followed by the readiness checks it will call the target script for the Import for the selected vms.
         * Finally it will merge ESX.xml to MasterConfigfile.xml.
         */


        internal bool runReadynesscheckAndTargetScript(ImportOfflineSyncForm importOfflineSyncForm)
        {
            try
            {
                // importOfflineSyncForm.progressBar.Visible = true;
                Esx esx = new Esx();
                Process p = null;
                importOfflineSyncForm.nextButton.Enabled = false;
                importOfflineSyncForm.cancelButton.Enabled = false;
                if (targetESXPreChekCompleted == false)
                {
                    importOfflineSyncForm.generalLogTextBox.AppendText("Running pre-req checks on secondary ESX... " + Environment.NewLine);
                    if (TargetReadynessCheck(importOfflineSyncForm) == false)
                    {
                        successFullyMadeFile = false;
                        importOfflineSyncForm.nextButton.Enabled = true;
                        importOfflineSyncForm.cancelButton.Enabled = true;
                        return false;
                    }
                    else
                    {
                        importOfflineSyncForm.masterTargetDataStoreComboBox.Enabled = false;
                        importOfflineSyncForm.masterHostComboBox.Enabled = false;
                        targetESXPreChekCompleted = true;
                        importOfflineSyncForm.generalLogTextBox.AppendText("Pre-req checks completed on target server.. " + Environment.NewLine);                        
                    }
                }
                if (targetESXPreChekCompleted == true)
                {
                    importOfflineSyncForm.nextButton.Enabled = false;
                    importOfflineSyncForm.cancelButton.Enabled = false;
                    importOfflineSyncForm.generalLogTextBox.AppendText("Running target scripts on secondary server... " + Environment.NewLine);
                    importOfflineSyncForm.targetScriptBackgroundWorker.RunWorkerAsync();
                    importOfflineSyncForm.progressBar.Visible = true;
                }
                // importOfflineSyncForm.targetScriptBackgroundWorker.RunWorkerAsync();
                //  Esx esx = new Esx();
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

        /*
         * this will be called by targetScriptBackgroundWorker dowork.
         * First this will cal the import script and then it will merge the esx.xml to MasterConfigFile.xml.
         * 
         */
        private bool VerfiyCxhasCreds(ImportOfflineSyncForm importOfflineSyncForm)
        {
            Cxapicalls cxAPi = new Cxapicalls();
            cxAPi.GetHostCredentials(importOfflineSyncForm.esxIPused);
            if (Cxapicalls.GetUsername != null && Cxapicalls.GetPassword != null)
            {
                Trace.WriteLine(DateTime.Now + "\t CX has target esx creds ");
            }
            else
            {
                if (cxAPi. UpdaterCredentialsToCX(importOfflineSyncForm.esxIPused, importOfflineSyncForm.esxsUserName, importOfflineSyncForm.esxsPassword) == true)
                {
                    Trace.WriteLine(DateTime.Now + "\t Updated creds to cx");
                    cxAPi.GetHostCredentials(importOfflineSyncForm.esxIPused);
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
            return true;
        }
        internal bool TargetScriptForImport(ImportOfflineSyncForm importOfflineSyncForm)
        {
            try
            {
                tickerDelegate = new TickerDelegate(SetLeftTicker);
                textBoxForBackGround = importOfflineSyncForm.generalLogTextBox;
                if (importOfflineSyncForm.esxglobal.TargetScriptForOffliceSyncImport(importOfflineSyncForm.esxIPused) == 0)
                {
                    ResumeForm.breifLog.WriteLine(DateTime.Now + "\t Successfully completed the import operation for the following vms");
                    foreach (Host h in importOfflineSyncForm.selSourceList._hostList)
                    {
                        ResumeForm.breifLog.WriteLine(DateTime.Now + " \t" + h.displayname + " \t target esx ip " + importOfflineSyncForm.esxIPused + " \t master target " + h.masterTargetDisplayName + " \t plan name " + h.plan);
                    }
                    ResumeForm.breifLog.Flush();
                    SolutionConfig config = new SolutionConfig();
                    //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
                    //based on above comment we have added xmlresolver as null
                    //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
                    XmlDocument document = new XmlDocument();
                    document.XmlResolver = null;
                    string xmlfile = WinTools.FxAgentPath() + "\\vContinuum\\Latest" + "\\ESX.xml";

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
                   // document.Load(xmlfile);
                    //reader.Close();
                    //XmlNodeList hostNodes = null;
                    XmlNodeList hostNodes = null;
                    XmlNodeList diskNodes = null;
                    hostNodes = document.GetElementsByTagName("host");
                    diskNodes = document.GetElementsByTagName("disk");
                    foreach (XmlNode node in diskNodes)
                    {
                        node.Attributes["protected"].Value = "yes";
                    }

                    try
                    {
                        Esx esx = new Esx();

                        VerfiyCxhasCreds(importOfflineSyncForm);
                        if (esx.GetEsxInfo(importOfflineSyncForm.esxIPused, Role.Secondary, importOfflineSyncForm.operatingsysType) == 0)
                        {
                            foreach (Host targetHost in importOfflineSyncForm.finMasterList._hostList)
                            {
                                foreach (Host h in esx.GetHostList)
                                {
                                    if (h.displayname == targetHost.displayname || h.hostname == targetHost.hostname || h.source_uuid == targetHost.source_uuid)
                                    {
                                        targetHost.vSpherehost = h.vSpherehost;
                                    }
                                }
                            }
                        }

                        Host target = (Host)importOfflineSyncForm.finMasterList._hostList[0];
                        foreach (XmlNode node in hostNodes)
                        {
                            if (node.Attributes["targetvSphereHostName"] != null)
                            {
                                node.Attributes["targetvSphereHostName"].Value = target.vSpherehost;
                            }
                            if (node.Attributes["display_name"] != null)
                            {
                                if (node.Attributes["display_name"].Value == target.displayname)
                                {
                                    node.Attributes["vSpherehostname"].Value = target.vSpherehost;
                                }
                            }
                        }
                        document.Save(xmlfile);
                    }
                    catch (Exception ex)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Failed to update esx.xml with proper vSphere host of mt " + ex.Message);

                    }
                    document.Save(xmlfile);
                    
                    MasterConfigFile master = new MasterConfigFile();
                    if (master.UpdateMasterConfigFile(WinTools.FxAgentPath() + "\\vContinuum\\Latest\\ESX.xml") == true)
                    {
                        importOfflineSyncForm.uploadedFileSuccessfully = true;
                        
                        message = "Import operation is completed" + Environment.NewLine;
                        importOfflineSyncForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { message });
                    
                    }
                    else
                    {
                        message = "Failed to upload MasterCOnfigFile.xml" + Environment.NewLine;
                        importOfflineSyncForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { message });                        
                    }
                }
                else
                {
                    try
                    {
                        StreamReader sr1 = new StreamReader(WinTools.FxAgentPath() + "\\vContinuum" + "\\logs\\vContinuum_ESX.log");
                        string firstLine1;
                        firstLine1 = sr1.ReadToEnd();
                         message = firstLine1;
                        importOfflineSyncForm.generalLogTextBox.Invoke(tickerDelegate, new object[] { message });
                        sr1.Close();
                        sr1.Dispose();
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

        /*
         * This will be caled by runReadynesscheckAndTargetScript.
         * This is using because we have splitting calls into different methods for understanding.
         */

        internal bool TargetReadynessCheck(ImportOfflineSyncForm importOfflineSyncForm)
        {
            bool timedOut = false;
            Process p;
            Esx esx = new Esx();
            int exitCode;

            try
            {

                for (int j = 0; j < importOfflineSyncForm.finMasterList._hostList.Count; j++)
                {
                    Host masterHost = (Host)importOfflineSyncForm.finMasterList._hostList[j];

                    // here we are checking that vCon and mt are same or not..... 
                    if (masterHost.hostname.Split('.')[0].ToUpper() == Environment.MachineName.ToUpper())
                    {
                        MessageBox.Show("Import operation can't be done for the same vCon and Master Target", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return false;
                    }
                    if (VerfiyCxhasCreds(importOfflineSyncForm) == false)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Failed to update esx creds to CX");
                        return false;
                    }
                    p = esx.PreReqCheckForOfflineSync(importOfflineSyncForm.esxIPused);

                    exitCode = WaitForProcess(p, importOfflineSyncForm, importOfflineSyncForm.progressBar, TimeOutMilliSeconds, ref timedOut);

                    if (exitCode != 0)
                    {
                        updateLogTextBox(importOfflineSyncForm.generalLogTextBox);
                        return false;
                    }
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


        /*
         * Our Readiness checks will be run by process. 
         * At first we don't know about backgroundworkers so that we introduced the Process for waiting till the readiness checks completed.
         * 
         */
        internal int WaitForProcess(Process p, ImportOfflineSyncForm importOfflineSyncForm, ProgressBar prgoressBar, long maxWaitTime, ref bool timedOut)
        {
           
                long waited = 0;
                int increasedWait = 100;
                int exitCode = 1;
                int i = 10;
                //90 Seconds
                try
                {
                    importOfflineSyncForm.targetDetailsPanel.Enabled = false;
                    //importOfflineSyncForm.Enabled = false;
                    importOfflineSyncForm.progressBar.Visible = true;
                    importOfflineSyncForm.progressBar.Value = i;

                    waited = 0;
                    while (p.HasExited == false)
                    {

                        if (waited <= TimeOutMilliSeconds)
                        {
                            p.WaitForExit(increasedWait);
                            waited = waited + increasedWait;
                            importOfflineSyncForm.progressBar.Value = i;
                            importOfflineSyncForm.progressBar.Update();
                            importOfflineSyncForm.progressBar.Refresh();
                            importOfflineSyncForm.Refresh();

                        }
                        else
                        {
                            if (p.HasExited == false)
                            {
                                Trace.WriteLine("Pre req checks failed ");
                                timedOut = true;
                                exitCode = 1;
                                break;


                            }
                        }
                    }


                    if (timedOut == false)
                    {
                        exitCode = p.ExitCode;
                    }



                    importOfflineSyncForm.progressBar.Visible = false;

                    importOfflineSyncForm.Enabled = true;
                    importOfflineSyncForm.targetDetailsPanel.Enabled = true;
                    Cursor.Current = Cursors.Default;
                }
                catch (Exception ex)
                {

                    System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                    Trace.WriteLine("ERROR*******************************************");
                    Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                    Trace.WriteLine("Exception  =  " + ex.Message);
                    Trace.WriteLine("ERROR___________________________________________");

                    return 1;
                }
            return exitCode;
        }

        /*
         * This is to updatelog when ever sripts failed.
         * With this method we will show logs in text box.
         */ 
        private bool updateLogTextBox(TextBox generalLogTextBox)
        {
            try
            {
                StreamReader sr1 = new StreamReader(WinTools.FxAgentPath()+ "\\vContinuum" + "\\logs\\vContinuum_ESX.log");
                string firstLine1;
                firstLine1 = sr1.ReadToEnd();
                generalLogTextBox.AppendText(firstLine1);
                sr1.Close();
                sr1.Dispose();
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
        /*
         * As of now we are not using this method.
         * Earlier ui has to read the datastroe from the ESX_Master_offline.xml
         * Now we removed entire that flow.
         */ 
        private DataStore ReadDataStoreNode(XmlNode node)
        {
            DataStore dataStore = new DataStore();
            Trace.WriteLine(DateTime.Now + " \t Entered: ReadDataStoreNode() of OfflineSync_TargetPanelHandler ");
            try
            {
                XmlNodeReader reader = null;
                //DataStore dataStore = new DataStore();
                reader = new XmlNodeReader(node);
                reader.Read();
                dataStore.name = reader.GetAttribute("datastore_name");
                /* dataStore.totalSize = float.Parse(reader.GetAttribute("total_size"));
                 dataStore.freeSpace = float.Parse(reader.GetAttribute("free_space"));
                 dataStore.blockSize = int.Parse(reader.GetAttribute("datastore_blocksize_mb"));*/
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
            Trace.WriteLine(DateTime.Now + " \t Exiting: ReadDataStoreNode() ofOfflineSync_TargetPanelHandler ");
            return dataStore;
        }
        private void SetLeftTicker(string s)
        {
            textBoxForBackGround.AppendText(s);
        }

    }
}
