using System;
using System.Collections.Generic;
using System.Collections;
using System.Windows.Forms;
using System.Text;
using System.Deployment;
using System.Diagnostics;
using System.IO;
using Com.Inmage.Tools;
using Com.Inmage.Esxcalls;


namespace Com.Inmage.Wizard
{
    class CreateDataStorePanelHandler : PanelHandler
    {
        public int readinessCheckReturnCode = 0;
        public int protectscriptReturnCode = 0;
        public int executeScript = 0;
        public string _installPath = null;
        public static string RECOVERY_ERROR_LOG_FILE = "\\logs\\vContinuum_ESX.log";
        public static bool CREATED_DATASTORES_USING_VCON = false;
        private delegate void TickerDelegate(string s);
        TickerDelegate tickerDelegate;
        TextBox _textBox = new TextBox();
        string _message = null;
        public CreateDataStorePanelHandler()
        {
            _installPath = WinTools.FxAgentPath() + "\\vContinuum";
        }
        internal override bool Initialize(AllServersForm allServersForm)
        {

            //if (File.Exists(_installPath + "\\test.bat"))
            //{
            //    try
            //    {
            //        File.Delete(_installPath + "\\test.bat");
            //    }
            //    catch (Exception ex)
            //    {
            //        Trace.WriteLine(DateTime.Now + "\t Failed to delete test.bat file " + ex.Message);
            //    }
            //}
            //try
            //{
            //    FileInfo f1 = new FileInfo(_installPath + "\\test.bat");
            //    StreamWriter sw = f1.CreateText();
            //    sw.Write("echo helloworld");
            //    sw.Close();
            //}
            //catch (Exception ex)
            //{
            //    Trace.WriteLine(DateTime.Now + "\t Failed to create test.bat file " + ex.Message);
            //}
            //allServersForm.commandToRunScriptTextBox.Text = _installPath + "\\test.bat";      

            //Trace.WriteLine(DateTime.Now + "\t Entered Initialize of CreateDataStorePanelHandler.cs");
            //Trace.WriteLine(DateTime.Now + "\t Exiting Initialize of CreateDataStorePanelHandler.cs");
            //if (allServersForm.createDatastoresRadioButton.Checked == true)
            //{
            //    allServersForm.runButton.Enabled = true;
            //    allServersForm.nextButton.Enabled = false;
            //}
            //else if (allServersForm.dataStoreAvailableRadioButton.Checked == true)
            //{
            //    allServersForm.runButton.Enabled = false;
            //    allServersForm.nextButton.Enabled = true;
            //}
            return true;
        }

        internal override bool ProcessPanel(AllServersForm allServersForm)
        {
            Trace.WriteLine(DateTime.Now + "\t Entered ProcessPanel of CreateDataStorePanelHandler.cs");
            Trace.WriteLine(DateTime.Now + "\t Exiting ProcessPanel of CreateDataStorePanelHandler.cs");
            return true;
        }

        internal override bool ValidatePanel(AllServersForm allServersForm)
        {
            Trace.WriteLine(DateTime.Now + "\t Entered ValidatePanel of CreateDataStorePanelHandler.cs");
            Trace.WriteLine(DateTime.Now + "\t Exiting ValidatePanel of CreateDataStorePanelHandler.cs");
            return true;
        }

        internal override bool CanGoToNextPanel(AllServersForm allServersForm)
        {
            Trace.WriteLine(DateTime.Now + "\t Entered CanGoToNextPanel of CreateDataStorePanelHandler.cs");
            Trace.WriteLine(DateTime.Now + "\t Exiting CanGoToNextPanel of CreateDataStorePanelHandler.cs");
            return true;
        }

        internal override bool CanGoToPreviousPanel(AllServersForm allServersForm)
        {
            allServersForm.nextButton.Enabled = true;
            Trace.WriteLine(DateTime.Now + "\t Entered CanGoToPreviousPanel of CreateDataStorePanelHandler.cs");
            Trace.WriteLine(DateTime.Now + "\t Exiting CanGoToPreviousPanel of CreateDataStorePanelHandler.cs");
            return true;
        }

        internal bool PrepareXmlForPreReqChecks(AllServersForm allServersForm)
        {
            try
            {
                try
                {
                    //SolutionConfig solution = new SolutionConfig();
                    //foreach (Host h in allServersForm._recoverChecksPassedList._hostList)
                    //{
                    //    foreach (Hashtable hash in h.disks.list)
                    //    {
                    //        hash["Protected"] = "no";
                    //    }
                    //}
                    //HostList masterList = new HostList();
                    //masterList.AddOrReplaceHost(allServersForm._masterHost);
                    //Esx esx = new Esx();
                    //esx._dataStoreList = allServersForm._esx._dataStoreList;
                    //esx._configurationList = allServersForm._esx._configurationList;
                    //esx._networkList = allServersForm._esx._networkList;

                    //foreach (Host h in allServersForm._recoverChecksPassedList._hostList)
                    //{
                    //    if (h.targetDataStore == null)
                    //    {
                    //        h.targetDataStore = h.sourceDataStoreForDrdrill;
                    //    }
                    //}
                    //solution.WriteXmlFile(allServersForm._recoverChecksPassedList, masterList, esx, WinTools.GetcxIP(), "Snapshot.xml", allServersForm._planName, false);
                    
                }
                catch (Exception ex)
                {

                    System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                    Trace.WriteLine("ERROR*******************************************");
                    Trace.WriteLine(DateTime.Now + "Caught exception at Method: Initialize" + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                    Trace.WriteLine("Exception  =  " + ex.Message);
                    Trace.WriteLine("ERROR___________________________________________");

                    return false;
                }

                //if (allServersForm.commandToRunScriptTextBox.Text.Length == 0)
                //{
                //    MessageBox.Show("Please enter command or script to execute.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                //    return false;
                //}
                

                //allServersForm.previousButton.Enabled = false;
                //allServersForm.progressBar.Visible = true;
                //allServersForm.createDatastorepanel.Enabled = false;
                //allServersForm.createDatastoreLogTextBox.AppendText("Running readiness checks on secondary server " + Environment.NewLine);
                //tickerDelegate = new TickerDelegate(SetLeftTicker);
                //_textBox = allServersForm.createDatastoreLogTextBox;
                //allServersForm.createDatastoresBackgroundWorker.RunWorkerAsync();
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: PrepareXmlForPreReqChecks " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            return true;
        }

        private void SetLeftTicker(string s)
        {
            _textBox.AppendText(s);
        }

        internal bool RunReadinesschecks(AllServersForm allServersForm)
        {


            //Esx esx = new Esx();
            //executeScript = WinTools.ExecuteLocally(1000000, "\"" + allServersForm.commandToRunScriptTextBox.Text + "\"");

            //if (executeScript == 0)
            //{
            //    readinessCheckReturnCode = esx.PreReqCheckForDRDrillAtCreatindDataStores(allServersForm._masterHost.esxIp, allServersForm._masterHost.esxUserName, allServersForm._masterHost.esxPassword);
            //    if (readinessCheckReturnCode == 0)
            //    {
            //        try
            //        {
            //            StreamReader sr = new StreamReader(_installPath + RECOVERY_ERROR_LOG_FILE);

            //            string firstLine;
            //            firstLine = sr.ReadToEnd();
            //            _message = firstLine + Environment.NewLine;
            //            allServersForm.createDatastoreLogTextBox.Invoke(tickerDelegate, new object[] { _message });
            //            sr.Close();
            //        }
            //        catch (Exception ex)
            //        {
            //            Trace.WriteLine(DateTime.Now + "\t Failed to rwad log file and failed to update text box as well ");
            //            Trace.WriteLine(DateTime.Now + "\t Failed after readiness checks completed successfully.");
            //            Trace.WriteLine(DateTime.Now + "\t So continuing with protect.pl script ");
            //            Trace.WriteLine(DateTime.Now + "\t Caught exception " + ex.Message);
            //        }
            //        _message = "Completed readiness checks on secondary server." + Environment.NewLine;
            //        allServersForm.createDatastoreLogTextBox.Invoke(tickerDelegate, new object[] { _message });
            //        _message = "Running scripts to create datastores on secondary server." + Environment.NewLine;
            //        allServersForm.createDatastoreLogTextBox.Invoke(tickerDelegate, new object[] { _message });
            //        protectscriptReturnCode = esx.ExecuteDatastoresScriptForArrayDrill(allServersForm._masterHost.esxIp, allServersForm._masterHost.esxUserName, allServersForm._masterHost.esxPassword);

            //        if (protectscriptReturnCode == 0)
            //        {
            //            _message = "Successfully completed creating scripts on secondary server." + Environment.NewLine;
            //            allServersForm.createDatastoreLogTextBox.Invoke(tickerDelegate, new object[] { _message });
            //        }
            //        else
            //        {
            //            _message = "Failed to create datastores on secondary server" + Environment.NewLine;
            //            allServersForm.createDatastoreLogTextBox.Invoke(tickerDelegate, new object[] { _message });
            //        }
            //    }
            //}
            //else
            //{
            //    Trace.WriteLine(DateTime.Now + "\t Failed to execute script given by user");
            //}
            return true;
        }

        internal bool CheckReadinesschecksReturncode(AllServersForm allServersForm)
        {
            try
            {
                //if (readinessCheckReturnCode == 0 && protectscriptReturnCode == 0)
                //{
                //    CREATED_DATASTORES_USING_VCON = true;
                    
                //    allServersForm.nextButton.Enabled = true;
                //    allServersForm.progressBar.Visible = false;
                //    SolutionConfig sol = new SolutionConfig();
                //    HostList sourceList = new HostList();
                //    Host masterHost = new Host();
                //    string esxip = null;
                //    string cxip = null;
                //    sol.ReadXmlConfigFile("Snapshot.xml", ref sourceList, ref masterHost, ref esxip, ref cxip);

                //    foreach (Host h in allServersForm._recoverChecksPassedList._hostList)
                //    {
                //        foreach (Host h1 in sourceList._hostList)
                //        {
                //            if (h1.new_displayname == h.new_displayname && h1.hostname == h.hostname)
                //            {
                //                h.targetDataStore = h1.targetDataStore;
                //                foreach (Host h2 in allServersForm._esx.GetHostList())
                //                {
                //                    DataStore d = new DataStore();
                //                    d.name = h1.targetDataStore;
                //                    d.freeSpace = 2040;
                //                    d.blockSize = 8;
                //                    d.filesystemversion = 5;
                //                    d.totalSize = 2048;
                //                    d.vSpherehostname = allServersForm._masterHost.vSpherehost;
                //                    DataStore d2 = (DataStore)h2.dataStoreList[0];
                //                    d.vendor = d2.vendor;
                //                    d.lun = d2.lun;
                //                    d.display_name = d2.display_name;
                //                    d.disk_name = "vContinuum999#";
                //                    h2.dataStoreList.Add(d);
                //                }
                //            }
                //        }
                //    }
                //}
                //else
                //{
                //    ReadLogFile(allServersForm);
                //    if (readinessCheckReturnCode != 0)
                //    {
                //        allServersForm.createDatastoreLogTextBox.AppendText("Readiness checks got failed on secondary server " + Environment.NewLine);
                //    }
                //    else if (protectscriptReturnCode != 0)
                //    {
                //        allServersForm.createDatastoreLogTextBox.AppendText("Failed to create datastores on secondary server " + Environment.NewLine);
                //    }

                //}
                //allServersForm.previousButton.Enabled = true;
                //allServersForm.previousButton.Visible = true;
                //allServersForm.createDatastorepanel.Enabled = true;
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: CheckReadinesschecksReturncode " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");

                return false;
            }
            return true;
        }

        internal bool ReadLogFile(AllServersForm recoveryForm)
        {
            try
            {
                //StreamReader sr = new StreamReader(_installPath + RECOVERY_ERROR_LOG_FILE);

                //string firstLine;
                //firstLine = sr.ReadToEnd();
                //recoveryForm.createDatastoreLogTextBox.AppendText(firstLine + Environment.NewLine);
                //sr.Close();
                //recoveryForm.progressBar.Visible = false;
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: ReadLogFile " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");

                return false;
            }
            return true;
        }
    }
}
