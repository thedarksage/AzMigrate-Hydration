using System;
using System.Collections.Generic;
using System.Text;
using System.Windows.Forms;
using System.Diagnostics;
using System.Collections;
using Com.Inmage.Esxcalls;
using System.IO;
using Com.Inmage.Tools;
using System.Threading;

namespace Com.Inmage.Wizard
{
    class AdditionOfDiskSelectMachinePanelHandler : PanelHandler
    {
        internal static long TimeOutMilliSeconds = 3600000;
        internal static string targetEsxIPxml;
        internal static string targetUsernameXml;
        internal static string targetPasswordXml;
        internal string MasterFile;
        internal bool downLoadedFile = false;
        internal static HostList sourceListPrepared;
        internal static Host masterHostPrepared;
        string esxIPprepared;
        string cxIPprepared;
        internal static int PlannameColumn = 0;
        internal static int HostnameColumn = 1;
        internal static int SourceesxIPColumn = 2;
        internal static int TargetEsxIPcolumn = 3;
        internal static int MastertargetDisplaynameColumn = 4;
        internal static int AdddiskColumn = 5;
        internal bool alReadyCalled = false;
        string planNamePrepared;
        internal static HostList temperorySelectedSourceList = new HostList();
        internal static Esx esxInfoAdddisk = new Esx();
        internal static string sourceEsxIPxml;
        internal static string sourceUsernamexml;
        internal static string sourcePasswordxml;
        internal static Esx targetEsxAdddisk = new Esx();
        internal static string machineTypexml;
        internal string masterTargetDisplayName;
        internal bool someMtIsUsingFile = false;
        internal HostList tempMasterListAddDisk = new HostList();
        internal string inStallPath = null;
        public AdditionOfDiskSelectMachinePanelHandler()
        {
            inStallPath = WinTools.FxAgentPath() + "\\vContinuum";
        }

        internal override bool Initialize(AllServersForm allServersForm)
        {
            try
            {
                allServersForm.previousButton.Visible = false;
                allServersForm.nextButton.Enabled = false;
                if (ResumeForm.selectedActionForPlan == "ESX")
                {
                    allServersForm.p2vHeadingLabel.Text = "Protect";
                }
                else
                {
                    allServersForm.p2vHeadingLabel.Text = "P2V Protection";
                }


                if (allServersForm.appNameSelected == AppName.Failback)
                {
                    allServersForm.p2vHeadingLabel.Text = "Failback protection";
                    allServersForm.Text = "Failback";
                    allServersForm.helpContentLabel.Text = HelpForcx.Failback;
                }
                else if (allServersForm.appNameSelected == AppName.Adddisks)
                {
                    allServersForm.Text = "Add Disks";
                    allServersForm.helpContentLabel.Text = HelpForcx.AdditionofDisk1;

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
                bool timedOut = false;
                Process p;
                int exitCode;
                ArrayList scsiVmxList = new ArrayList();
                Cursor.Current = Cursors.WaitCursor;
                // allServersForm.Enabled = false;
                allServersForm.additionOfDiskPanel.Enabled = false;
                //taking because for the usage in the allserversform
                if (allServersForm.appNameSelected == AppName.Adddisks)
                {
                    allServersForm.Text = "Add Disks";
                    string nodeList = null;
                    //here we are taking the selected item for the combobox whether it is bmr or esx..
                    if (ResumeForm.selectedActionForPlan == "BMR Protection")
                    {
                        //the below string is the public static string ....

                        //this is for the esx.xml file attribute...
                        if (temperorySelectedSourceList._hostList.Count != 0)
                        {
                            foreach (Host h in temperorySelectedSourceList._hostList)
                            {
                                foreach (Host h1 in sourceListPrepared._hostList)
                                {
                                    Trace.WriteLine(DateTime.Now + " \t Came here to compare displaynames " + h.displayname + "  " + h1.displayname);
                                    if (h.displayname == h1.displayname)
                                    {

                                        ArrayList diskList = new ArrayList();
                                        ArrayList physicalList = new ArrayList();
                                        diskList = h.disks.GetDiskList;
                                        physicalList = h1.disks.GetDiskList;
                                        foreach (Hashtable diskHash in diskList)
                                        {
                                            diskHash["already_map_vmx"] = "no";
                                        }

                                        foreach (Hashtable hash in physicalList)
                                        {
                                            foreach (Hashtable diskHash in diskList)
                                            {
                                                Trace.WriteLine(DateTime.Now + " \t Came here to compare disknames  " + hash["Name"].ToString() + "  " + diskHash["Name"].ToString());
                                                if (hash["Name"].ToString() == diskHash["Name"].ToString())
                                                {
                                                    Trace.WriteLine(DateTime.Now + " \t Came here to assing scsi mapping host values" + hash["scsi_mapping_vmx"].ToString());
                                                    diskHash["scsi_mapping_vmx"] = hash["scsi_mapping_vmx"].ToString();
                                                    scsiVmxList.Add(hash["scsi_mapping_vmx"].ToString());
                                                    diskHash["scsi_adapter_number"] = hash["scsi_mapping_vmx"].ToString().Split(':')[0];
                                                    diskHash["scsi_slot_number"] = hash["scsi_mapping_vmx"].ToString().Split(':')[1];
                                                    diskHash["Protected"] = "yes";
                                                    diskHash["already_map_vmx"] = "yes";
                                                }

                                            }
                                        }

                                    }

                                }
                            }
                            foreach (Host h1 in temperorySelectedSourceList._hostList)
                            {
                                ArrayList diskListOfHost = new ArrayList();
                                diskListOfHost = h1.disks.GetDiskList;
                                foreach (Hashtable disk in diskListOfHost)
                                {
                                    disk["Selected"] = "Yes";
                                }
                            }
                            foreach (Host h1 in temperorySelectedSourceList._hostList)
                            {
                                foreach (Host tempHost in sourceListPrepared._hostList)
                                {
                                    if (h1.displayname == tempHost.displayname)
                                    {
                                        ArrayList diskListOfHost = new ArrayList();
                                        diskListOfHost = tempHost.disks.GetDiskList;
                                        int i = 0;
                                        foreach (Hashtable disk in diskListOfHost)
                                        {
                                            if (disk["Protected"].ToString() == "yes")
                                            {
                                                i++;
                                            }
                                        }
                                        //tempHost.alreadyP2VDisksProtected = i;
                                        //h1.alreadyP2VDisksProtected = i;

                                        if ((i >= 8 && i < 23))
                                        {
                                            tempHost.alreadyP2VDisksProtected = i + 1;
                                            h1.alreadyP2VDisksProtected = i + 1;
                                        }
                                        else if (i >= 23 && i < 38)
                                        {
                                            tempHost.alreadyP2VDisksProtected = i + 2;
                                            h1.alreadyP2VDisksProtected = i + 2;

                                        }
                                        else if (i >= 38 && i < 53)
                                        {
                                            tempHost.alreadyP2VDisksProtected = i + 3;
                                            h1.alreadyP2VDisksProtected = i + 3;
                                        }
                                        else if (i >= 53)
                                        {
                                            tempHost.alreadyP2VDisksProtected = i + 4;
                                            h1.alreadyP2VDisksProtected = i + 4;
                                        }
                                        else
                                        {
                                            tempHost.alreadyP2VDisksProtected = i;
                                            h1.alreadyP2VDisksProtected = i;

                                        }
                                    }
                                }

                                foreach (Host tempHost1 in temperorySelectedSourceList._hostList)
                                {
                                    Trace.WriteLine(DateTime.Now + " \t Printing protected disk count" + tempHost1.alreadyP2VDisksProtected.ToString());
                                    tempHost1.disks.AppendScsiForP2vAdditionOfDisk(tempHost1.alreadyP2VDisksProtected);
                                    ArrayList physicalDisks = tempHost1.disks.GetDiskList;
                                }
                            }
                            //h.disks.AddScsiNumbers();

                            //generating sourcexml for the scripts at the time of the incremental disk...

                            SolutionConfig config = new SolutionConfig();
                           // config.WriteSourceXml(_tempSelectedSourceList, "SourceXml.xml");
                            Host sourceHost = (Host)temperorySelectedSourceList._hostList[0];
                           masterHostPrepared.displayname = sourceHost.masterTargetDisplayName;
                            masterHostPrepared.hostname = sourceHost.masterTargetHostName;
                            HostList masterlist = new HostList();
                            masterlist.AddOrReplaceHost(masterHostPrepared);
                            
                            Esx e1 = new Esx();
                            //checking the esx.xml exist and if it is there we are deleting the file...
                            try
                            {
                                if (File.Exists("ESX.xml"))
                                {
                                    File.Delete("ESX.xml");

                                }

                            }
                            catch (Exception ex)
                            {

                                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                                Trace.WriteLine("ERROR*******************************************");
                                Trace.WriteLine(DateTime.Now + "Caught exception at Method: PostJobAutomation  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                                Trace.WriteLine("Exception  =  " + ex.Message);
                                Trace.WriteLine("ERROR___________________________________________");

                                return false;
                            }
                            config.WriteXmlFile(temperorySelectedSourceList, masterlist, targetEsxAdddisk, WinTools.GetcxIP(), "ESX.xml", "", true);
                            //_tempSelectedSourceList.GenerateVmxFile();

                            //downloading the esx.xml from the scripts for the p2v incremental disk....
                            p = e1.DownLoadXmlFileForP2v(targetEsxIPxml);
                            exitCode = WaitForProcess(p, allServersForm, allServersForm.progressBar, TimeOutMilliSeconds, ref timedOut);


                            if (exitCode == 0)
                            {
                                allServersForm.additionOfDiskPanel.Enabled = true;
                                allServersForm.Enabled = true;

                                Cursor.Current = Cursors.Default;
                                allServersForm.Enabled = true;

                            }
                            else if (exitCode == 3)
                            {
                                allServersForm.additionOfDiskPanel.Enabled = true;
                                // allServersForm.Enabled = true;
                                Cursor.Current = Cursors.Default;
                                allServersForm.Enabled = true;

                                MessageBox.Show("Please check credentials provided", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);


                                return false;
                            }
                            else
                            {
                                Cursor.Current = Cursors.Default;
                                allServersForm.additionOfDiskPanel.Enabled = true;
                                allServersForm.Enabled = true;
                                MessageBox.Show("Failed to get information from the primary vSphere server.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);


                                return false;
                            }

                           
                        }
                        else
                        {
                            Cursor.Current = Cursors.Default;
                            allServersForm.additionOfDiskPanel.Enabled = true;
                            allServersForm.Enabled = true;
                            MessageBox.Show("There are no vms found", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            return false;
                        }
                    }
                   
                    //this is for the esx incremental disk solution....
                    else if (ResumeForm.selectedActionForPlan == "ESX")
                    {
                        //the below string is the public static string ....


                        //this is for the esx.xml file attribute...
                        //  _machineType = "VirtualMachine";




                        

                        if (temperorySelectedSourceList._hostList.Count != 0)
                        {
                            Host h1 = (Host)temperorySelectedSourceList._hostList[0];
                            // Debug.WriteLine("Printing command" + _sourceEsxIp + "\"" + _sourceUsername + "\"" + "\"" + _sourcePassword + "\"" + _targetEsxIp + "\"" + _targetUsername + "\"" + _targetPassword + nodeList + h1.masterTargetDisplayName + _plan);

                            //downloading the esx.xml file for the esx solution.....
                            try
                            {
                                if (File.Exists(inStallPath + " \\Latest\\ESX.xml"))
                                {
                                    File.Delete(inStallPath + " \\Latest\\ESX.xml");
                                }
                            }
                            catch (Exception ex)
                            {
                                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                                Trace.WriteLine("ERROR*******************************************");
                                Trace.WriteLine(DateTime.Now + "Caught exception at Method: Deletion ESX.xml failed in ProcessPanel " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                                Trace.WriteLine("Exception  =  " + ex.Message);
                                Trace.WriteLine("ERROR___________________________________________");                               
                            }
                            SolutionConfig config = new SolutionConfig();
                            Esx esx = new Esx();
                            Host sourceHost = (Host)temperorySelectedSourceList._hostList[0];
                            masterHostPrepared.displayname = sourceHost.masterTargetDisplayName;
                            masterHostPrepared.hostname = sourceHost.masterTargetHostName;
                            masterHostPrepared.source_uuid = sourceHost.mt_uuid;
                            tempMasterListAddDisk.AddOrReplaceHost(masterHostPrepared);
                            esx.configurationList = masterHostPrepared.configurationList;
                            esx.lunList = masterHostPrepared.lunList;
                            esx.networkList = masterHostPrepared.networkNameList;
                            esx.dataStoreList = masterHostPrepared.dataStoreList;
                            config.WriteXmlFile(temperorySelectedSourceList, tempMasterListAddDisk, esx, WinTools.GetcxIP(), "ESX.xml", "", false);
                            p = esxInfoAdddisk.DownLoadXmlFile(sourceEsxIPxml);

                            exitCode = WaitForProcess(p, allServersForm, allServersForm.progressBar, TimeOutMilliSeconds, ref timedOut);


                            if (exitCode == 0)
                            {
                                allServersForm.additionOfDiskPanel.Enabled = true;
                                allServersForm.Enabled = true;
                                Cursor.Current = Cursors.Default;
                                allServersForm.Enabled = true;

                            }
                            else if (exitCode == 3)
                            {
                                allServersForm.additionOfDiskPanel.Enabled = true;
                                // allServersForm.Enabled = true;
                                Cursor.Current = Cursors.Default;
                                allServersForm.Enabled = true;

                                MessageBox.Show("Please check credentials provided", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);


                                return false;
                            }
                            else
                            {
                                Cursor.Current = Cursors.Default;
                                allServersForm.additionOfDiskPanel.Enabled = true;
                                allServersForm.Enabled = true;
                                MessageBox.Show("Failed to get information from the primary vSphere server.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);


                                return false;
                            }

                        }
                        else
                        {
                            Cursor.Current = Cursors.Default;
                            allServersForm.additionOfDiskPanel.Enabled = true;
                            allServersForm.Enabled = true;
                            MessageBox.Show("There are no vms found", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            return false;
                        }
                    }

                }
                else if (allServersForm.appNameSelected == AppName.Failback)
                {
                    if (temperorySelectedSourceList._hostList.Count == 0)
                    {
                        Cursor.Current = Cursors.Default;
                        allServersForm.additionOfDiskPanel.Enabled = true;
                        allServersForm.Enabled = true;
                        MessageBox.Show("There are no vms selected for the failback protection", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);

                        return false;
                    }
                    else
                    {
                        esxInfoAdddisk.GetHostList.Clear();
                        if (esxInfoAdddisk.GetEsxInfo(targetEsxIPxml, Role.Primary, OStype.Windows) == 0)
                        {
                            Cursor.Current = Cursors.Default;
                            allServersForm.additionOfDiskPanel.Enabled = true;
                            allServersForm.Enabled = true;
                        }
                        else
                        {
                            MessageBox.Show("Failed to fetch the information from " + targetEsxIPxml, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            Cursor.Current = Cursors.Default;
                            allServersForm.additionOfDiskPanel.Enabled = true;
                            allServersForm.Enabled = true;
                            return false;

                        }

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

        internal int WaitForProcess(Process p, AllServersForm allServersForm, ProgressBar prgoressBar, long maxWaitTime, ref bool timedOut)
        {

 
            long waited = 0;
            int incWait = 100;
            int exitCode = 1;
            int i = 10;
            //90 Seconds

            try
            {
                // allServersForm.Enabled = false;
                allServersForm.additionOfDiskPanel.Enabled = false;
                allServersForm.progressBar.Visible = true;
                allServersForm.progressBar.Value = i;

                waited = 0;
                while (p.HasExited == false)
                {

                    if (waited <= TimeOutMilliSeconds)
                    {
                        p.WaitForExit(incWait);
                        waited = waited + incWait;
                        allServersForm.progressBar.Value = i;
                        allServersForm.progressBar.Update();
                        allServersForm.progressBar.Refresh();
                        allServersForm.Refresh();

                    }
                    else
                    {
                        if (p.HasExited == false)
                        {
                            Trace.WriteLine("Pre req checks failed " + DateTime.Now);
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

                allServersForm.progressBar.Visible = false;
                allServersForm.additionOfDiskPanel.Enabled = true;
                allServersForm.Enabled = true;
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
        internal override bool CanGoToNextPanel(AllServersForm allServersForm)
        {
            return true;

        }
        internal override bool CanGoToPreviousPanel(AllServersForm allServersForm)
        {
            return true;

        }
        internal bool GetDetailsOfProtectedVms(AllServersForm allServersForm)
        {
            try
            {
                if (allServersForm.addDiskTargetESXIPText.Text.Length == 0)
                {
                    MessageBox.Show("Please enter secondary vSphere IP address ", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return false;
                }
                if (allServersForm.addDiskUserNameText.Text.Length == 0)
                {
                    MessageBox.Show("Please enter secondary vSphere username ", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return false;

                }
                if (allServersForm.addDiskPasswordText.Text.Length == 0)
                {
                    MessageBox.Show("Please enter secondary vSphere password ", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return false;
                }
                targetEsxIPxml = allServersForm.addDiskTargetESXIPText.Text.Trim();
                targetUsernameXml = allServersForm.addDiskUserNameText.Text.Trim();
                targetPasswordXml = allServersForm.addDiskPasswordText.Text;
                allServersForm.getDetailsButton.Enabled = false;
                allServersForm.nextButton.Enabled = false;
                allServersForm.progressBar.Visible = true;
                allServersForm.detailsOfAdditionOfDiskDataGridView.Visible = false;
                allServersForm.detailsOfAdditionOfDiskDataGridView.Rows.Clear();
                MasterFile = "ESX_Master_" + targetEsxIPxml + ".xml";
                try
                {
                    if (File.Exists(MasterFile))
                    {
                        File.Delete(MasterFile);
                    }
                }
                catch (Exception ex)
                {

                    System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                    Trace.WriteLine("ERROR*******************************************");
                    Trace.WriteLine(DateTime.Now + "Caught exception at Method: PostJobAutomation  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                    Trace.WriteLine("Exception  =  " + ex.Message);
                    Trace.WriteLine("ERROR___________________________________________");

                    return false;
                }
                allServersForm.additionOfDiskBackGroundWorker.RunWorkerAsync();

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


        internal bool DownLoadEsxMasterXmlFile(AllServersForm allServersForm)
        {
            try
            {
                int returnValue = 1;
            returnValue =esxInfoAdddisk.DownloadFile(targetEsxIPxml, MasterFile);
               if(returnValue == 0)
                {
                    downLoadedFile = true;

                }
               else if (returnValue == 2)
               {
                   if (File.Exists(inStallPath + "\\ESX_Master.lck"))
                   {
                       StreamReader sr1 = new StreamReader(inStallPath + "\\ESX_Master.lck");
                       string firstLine1;
                       firstLine1 = sr1.ReadToEnd();                       
                       WinPreReqs win = new WinPreReqs("", "", "", "");
                       Host h = new Host();
                       h.hostname = firstLine1.Split('=')[0];
                       string cxIP = firstLine1.Split('=')[1];
                       win.MasterTargetPreCheck( h, cxIP);
                       h = WinPreReqs.GetHost;
                       if (h.machineInUse == true)
                       {
                           someMtIsUsingFile = true;
                           MessageBox.Show("Master target:" + h.hostname + " jobs are running on CX:" + cxIP, "Error", MessageBoxButtons.OK, MessageBoxIcon.Stop);

                           downLoadedFile = false;
                       }
                       else
                       {

                       }
                   }
                   else
                   {
                       return false;
                   }
                  
               }
               else
               {
                   downLoadedFile = false;
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

        internal bool LoadDataGridViewAfterDownLoadingEsx_MasterXml(AllServersForm allServersForm)
        {
            try
            {
                if (downLoadedFile == true)
                {
                    sourceListPrepared = new HostList();
                    temperorySelectedSourceList = new HostList();
                    alReadyCalled = false;
                    masterHostPrepared = new Host();
                    string machine = "";
                    SolutionConfig cfg = new SolutionConfig();
                    targetEsxAdddisk = esxInfoAdddisk;
                    cfg.ReadXmlConfigFile(MasterFile,  sourceListPrepared,  masterHostPrepared,  esxIPprepared,  cxIPprepared);
                    sourceListPrepared = SolutionConfig.list;
                    masterHostPrepared = SolutionConfig.masterHosts;
                    esxIPprepared = SolutionConfig.EsxIP;
                    cxIPprepared = SolutionConfig.csIP;
                    if (ResumeForm.selectedActionForPlan == "BMR Protection")
                    {
                        machine = "PhysicalMachine";
                        machineTypexml = machine;
                    }
                    else if (ResumeForm.selectedActionForPlan == "ESX")
                    {
                        machine = "VirtualMachine";
                        machineTypexml = machine;
                    }

                    int hostCount = sourceListPrepared._hostList.Count;

                    Debug.WriteLine("Printing hostcount");
                    int rowIndex = 0;
                    if (hostCount > 0)
                    {

                        allServersForm.detailsOfAdditionOfDiskDataGridView.Rows.Clear();
                        foreach (Host h in sourceListPrepared._hostList)
                        {

                            Debug.WriteLine("Printing plan name " + h.plan);
                            if (allServersForm.appNameSelected == AppName.Adddisks)
                            {
                                if (h.os != OStype.Linux)
                                {
                                    if (h.machineType == machine)
                                    {
                                        if (h.failOver != "yes")
                                        {
                                            //MessageBox.Show("datastore" + h.targetDataStore  + h.displayname);
                                            allServersForm.detailsOfAdditionOfDiskDataGridView.Rows.Add(1);
                                            allServersForm.detailsOfAdditionOfDiskDataGridView.Rows[rowIndex].Cells[PlannameColumn].Value = h.plan;

                                            allServersForm.detailsOfAdditionOfDiskDataGridView.Rows[rowIndex].Cells[HostnameColumn].Value = h.displayname;
                                            if (allServersForm.appNameSelected == AppName.Adddisks && ResumeForm.selectedActionForPlan == "ESX")
                                            {
                                                allServersForm.detailsOfAdditionOfDiskDataGridView.Rows[rowIndex].Cells[SourceesxIPColumn].Value = h.esxIp;                                                
                                            }
                                            else
                                            {
                                                allServersForm.detailsOfAdditionOfDiskDataGridView.Rows[rowIndex].Cells[SourceesxIPColumn].Value = "N/A";
                                            }
                                            allServersForm.detailsOfAdditionOfDiskDataGridView.Rows[rowIndex].Cells[TargetEsxIPcolumn].Value = h.targetESXIp;
                                            allServersForm.detailsOfAdditionOfDiskDataGridView.Rows[rowIndex].Cells[MastertargetDisplaynameColumn].Value = h.masterTargetDisplayName;
                                            // detailsDataGridView.Rows[rowIndex].Cells[2].Value = true;
                                            rowIndex++;
                                        }
                                    }
                                }
                            }
                            if (allServersForm.appNameSelected == AppName.Failback)
                            {
                                allServersForm.detailsOfAdditionOfDiskDataGridView.Columns[AdddiskColumn].HeaderText = "Fail back";
                                machine = "VirtualMachine";
                                if (h.os != OStype.Linux)
                                {
                                    if (h.machineType == machine)
                                    {
                                        if (h.failOver == "yes")
                                        {
                                            allServersForm.detailsOfAdditionOfDiskDataGridView.Rows.Add(1);
                                            allServersForm.detailsOfAdditionOfDiskDataGridView.Rows[rowIndex].Cells[PlannameColumn].Value = h.plan;
                                            allServersForm.detailsOfAdditionOfDiskDataGridView.Rows[rowIndex].Cells[HostnameColumn].Value = h.displayname;
                                            if (h.esxIp.Length > 0)
                                            {
                                                allServersForm.detailsOfAdditionOfDiskDataGridView.Rows[rowIndex].Cells[SourceesxIPColumn].Value = h.esxIp;
                                            }
                                            else
                                            {
                                                allServersForm.detailsOfAdditionOfDiskDataGridView.Rows[rowIndex].Cells[SourceesxIPColumn].Value = "N/A";
                                            }
                                            allServersForm.detailsOfAdditionOfDiskDataGridView.Rows[rowIndex].Cells[TargetEsxIPcolumn].Value = h.targetESXIp;
                                            allServersForm.detailsOfAdditionOfDiskDataGridView.Rows[rowIndex].Cells[MastertargetDisplaynameColumn].Value = h.masterTargetDisplayName;
                                            // detailsDataGridView.Rows[rowIndex].Cells[2].Value = true;
                                            rowIndex++;
                                        }
                                        ArrayList physicalDisks = h.disks.GetDiskList;
                                        foreach (Hashtable disk in physicalDisks)
                                        {
                                            if (disk["Selected"].ToString() == "Yes")
                                            {
                                                if (disk["Size"] != null)
                                                {
                                                    int size = int.Parse(disk["Size"].ToString());
                                                    size = size / (1024 * 1024);
                                                    h.totalSizeOfDisks = h.totalSizeOfDisks + size;
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        //allServersForm.detailsOfAdditionOfDiskDataGridView.Visible = true;
                    }
                    if (allServersForm.detailsOfAdditionOfDiskDataGridView.RowCount > 0)
                    {
                        allServersForm.detailsLabel.Visible = true;
                        allServersForm.detailsOfAdditionOfDiskDataGridView.Visible = true;
                    }
                    else
                    {
                        allServersForm.detailsLabel.Visible = false;
                        allServersForm.detailsOfAdditionOfDiskDataGridView.Visible = false;
                        if (allServersForm.appNameSelected == AppName.Adddisks)
                        {
                            MessageBox.Show("There are no VMs available to add disk.", "No VMs available", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        }
                        if (allServersForm.appNameSelected == AppName.Failback)

                            MessageBox.Show("There are no VMs available for failback.", "Failback: No VMs found", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    }
                    allServersForm.getDetailsButton.Enabled = true;
                    allServersForm.progressBar.Visible = false;
                    return false;
                }
                else if (someMtIsUsingFile == true)
                {

                }
                else
                {
                    MessageBox.Show("Could not login to the ESX server: " + targetEsxIPxml + " Please check IP address and credentials provided.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);


                }
                allServersForm.getDetailsButton.Enabled = true;
                allServersForm.progressBar.Visible = false;
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


        internal bool SelectedMachineForAdditionOfDisk(AllServersForm allServersForm, int index)
        {
            try
            {
                Host h = new Host();

                if (allServersForm.detailsOfAdditionOfDiskDataGridView.RowCount > 0)
                {
                    if (allServersForm.detailsOfAdditionOfDiskDataGridView.Rows[index].Cells[AdddiskColumn].Selected == true)
                    {

                        h.displayname = allServersForm.detailsOfAdditionOfDiskDataGridView.Rows[index].Cells[HostnameColumn].Value.ToString();

                        Debug.WriteLine("printing the checkbox value " + (bool)allServersForm.detailsOfAdditionOfDiskDataGridView.Rows[index].Cells[AdddiskColumn].FormattedValue);

                        if ((bool)allServersForm.detailsOfAdditionOfDiskDataGridView.Rows[index].Cells[AdddiskColumn].FormattedValue)
                        {
                            if (allServersForm.appNameSelected == AppName.Adddisks)
                            {


                                if (alReadyCalled == false)
                                {
                                    planNamePrepared = allServersForm.detailsOfAdditionOfDiskDataGridView.Rows[index].Cells[PlannameColumn].Value.ToString();
                                    allServersForm.planInput = planNamePrepared;

                                    SourceEsxDetailsPopUpForm sourceEsxDetailsPopUPForm = new SourceEsxDetailsPopUpForm();
                                    if (ResumeForm.selectedActionForPlan == "BMR Protection")
                                    {
                                        foreach (Host h1 in sourceListPrepared._hostList)
                                        {
                                            if (h.displayname == h1.displayname)
                                            {
                                                sourceEsxDetailsPopUPForm.ipLabel.Text = "Source IP";
                                                sourceEsxDetailsPopUPForm.sourceEsxIpText.Text = h1.ip;
                                                sourceEsxDetailsPopUPForm.domainNameLabel.Visible = true;
                                                sourceEsxDetailsPopUPForm.domainText.Visible = true;
                                                break;
                                            }
                                        }
                                    }
                                    if (ResumeForm.selectedActionForPlan == "ESX")
                                    {
                                        sourceEsxDetailsPopUPForm.sourceEsxIpText.Text = allServersForm.detailsOfAdditionOfDiskDataGridView.Rows[index].Cells[SourceesxIPColumn].Value.ToString();
                                    }
                                    sourceEsxDetailsPopUPForm.userNameText.Select();
                                    sourceEsxDetailsPopUPForm.ShowDialog();
                                    if (sourceEsxDetailsPopUPForm.canceled == true)
                                    {
                                        allServersForm.detailsOfAdditionOfDiskDataGridView.Rows[index].Cells[AdddiskColumn].Value = false;
                                        allServersForm.detailsOfAdditionOfDiskDataGridView.RefreshEdit();
                                    }
                                    else
                                    {
                                        masterTargetDisplayName = allServersForm.detailsOfAdditionOfDiskDataGridView.Rows[index].Cells[MastertargetDisplaynameColumn].Value.ToString();
                                        allServersForm.nextButton.Visible = true;
                                        alReadyCalled = true;
                                        foreach (Host h1 in sourceListPrepared._hostList)
                                        {
                                            
                                            Host h2 = new Host();

                                            if (h1.displayname == h.displayname)
                                            {

                                                if (ResumeForm.selectedActionForPlan == "BMR Protection")
                                                {
                                                    alReadyCalled = false;
                                                    h1.userName = sourceEsxDetailsPopUPForm.userNameText.Text.Trim();
                                                    h1.passWord = sourceEsxDetailsPopUPForm.passWordText.Text.Trim();
                                                    h1.domain = sourceEsxDetailsPopUPForm.domainText.Text.Trim();
                                                 
                                                    PhysicalMachine p = new PhysicalMachine(h1.ip, h1.domain, h1.userName, h1.passWord);
                                                    string error ="";
                                                    //Here capturing the wmi output to show the error message if user gives wrong credentials.
                                                    if (WinPreReqs.WindowsShareEnabled(h1.ip, h1.domain, h1.userName, h1.passWord,"", error) == false)
                                                    {
                                                        error = WinPreReqs.GetError;
                                                        MessageBox.Show(error,"Error",MessageBoxButtons.OK,MessageBoxIcon.Error);
                                                       
                                                        alReadyCalled = false;
                                                        allServersForm.detailsOfAdditionOfDiskDataGridView.Rows[index].Cells[AdddiskColumn].Value = false;
                                                        allServersForm.detailsOfAdditionOfDiskDataGridView.RefreshEdit();
                                                        return false;

                                                    }
                                                    if (p.GetHostInfo() == true)
                                                    {
                                                        h2 = p.GetHost;
                                                    }
                                                    if (h2 == null)
                                                    {                                                      
                                                        allServersForm.detailsOfAdditionOfDiskDataGridView.Rows[index].Cells[AdddiskColumn].Value = false;
                                                        allServersForm.detailsOfAdditionOfDiskDataGridView.RefreshEdit();
                                                        return false;
                                                    }                                             
                                                    h2.targetDataStore = h1.targetDataStore;
                                                    h2.masterTargetDisplayName = h1.masterTargetDisplayName;
                                                    h2.masterTargetHostName = h1.masterTargetHostName;
                                                    temperorySelectedSourceList.AddOrReplaceHost(h2);
                                                }
                                                else
                                                {
                                                    temperorySelectedSourceList.AddOrReplaceHost(h1);
                                                    Debug.WriteLine("printing the count of datastroe list" + h.dataStoreList.Count);
                                                }
                                                // _tempSelectedSourceList.Print();
                                                // Debug.WriteLine("Adding to the hostlist");
                                                // _tempSelectedSourceList.Print();
                                            }

                                        }


                                        sourceEsxIPxml = sourceEsxDetailsPopUPForm.sourceEsxIpText.Text.Trim();
                                        sourceUsernamexml = sourceEsxDetailsPopUPForm.userNameText.Text.Trim();
                                        sourcePasswordxml = sourceEsxDetailsPopUPForm.passWordText.Text.Trim();

                                    }
                                }
                                else
                                {
                                    if (ResumeForm.selectedActionForPlan == "ESX")
                                    {

                                        if (SelectedMachine(allServersForm, index) == false)
                                        {
                                            return false;
                                        }
                                    }
                                }
                            }
                            else if (allServersForm.appNameSelected == AppName.Failback)
                            {

                                string esxIP = "";
                                foreach (Host h1 in sourceListPrepared._hostList)
                                {
                                    if (h1.displayname == allServersForm.detailsOfAdditionOfDiskDataGridView.Rows[index].Cells[HostnameColumn].Value.ToString())
                                    {
                                        if (temperorySelectedSourceList._hostList.Count == 0)
                                        {

                                            planNamePrepared = allServersForm.detailsOfAdditionOfDiskDataGridView.Rows[index].Cells[PlannameColumn].Value.ToString();
                                            allServersForm.planInput = planNamePrepared;


                                            sourceEsxIPxml = allServersForm.detailsOfAdditionOfDiskDataGridView.Rows[index].Cells[SourceesxIPColumn].Value.ToString();
                                            allServersForm.nextButton.Visible = true;
                                            esxIP = allServersForm.detailsOfAdditionOfDiskDataGridView.Rows[index].Cells[SourceesxIPColumn].Value.ToString();


                                            /*  AddCredentialsPopUpForm add = new AddCredentialsPopUpForm();
                                              add.domainText.Text = allServersForm.cachedDomain;
                                              add.userNameText.Text = allServersForm.cachedUsername;
                                              add.passWordText.Text = allServersForm.cachedPassword;
                                              add.useForAllCheckBox.Visible = true;
                                              add.credentialsHeadingLabel.Text = "Provide credentials for " + h1.displayname;
                                              add.ShowDialog();

                                              if (add.canceled == false)
                                              {

                                                  h1.userName = add.userName;
                                                  h1.passWord = add.passWord;
                                                  h1.domain = add.domain;
                                                  allServersForm.cachedDomain = add.domain;
                                                  allServersForm.cachedUsername = add.userName;
                                                  allServersForm.cachedPassword = add.passWord;
                                                  if (add.useForAllCheckBox.Checked == true)
                                                  {
                                                      foreach (Host tempHost in _sourceList._hostList)
                                                      {
                                                          if (tempHost.userName == null)
                                                          {
                                                              tempHost.userName = add.userName;
                                                              tempHost.passWord = add.passWord;
                                                              tempHost.domain = add.domain;
                                                          }
                                                      }

                                                  }
                                                  _tempSelectedSourceList.AddOrReplaceHost(h1);
                                              }
                                              else
                                              {
                                                  allServersForm.detailsOfAdditionOfDiskDataGridView.Rows[index].Cells[ADD_DISK_COLUMN].Value = false;
                                                  allServersForm.detailsOfAdditionOfDiskDataGridView.RefreshEdit();

                                              }
                                          */
                                            temperorySelectedSourceList.AddOrReplaceHost(h1);
                                        }
                                        else
                                        {
                                            if (planNamePrepared != allServersForm.detailsOfAdditionOfDiskDataGridView.Rows[index].Cells[PlannameColumn].Value.ToString())
                                            {
                                                MessageBox.Show("Kindly choose machines from same plan name.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                                allServersForm.detailsOfAdditionOfDiskDataGridView.Rows[index].Cells[AdddiskColumn].Value = false;

                                                allServersForm.detailsOfAdditionOfDiskDataGridView.RefreshEdit();
                                                return false;
                                            }
                                            if (h1.esxIp != sourceEsxIPxml) //|| _masterTargetDisplayName != allServersForm.detailsOfAdditionOfDiskDataGridView.Rows[index].Cells[MASTERTARGET_DISPLAYNAME_COLUMN].Value.ToString())
                                            {
                                                allServersForm.detailsOfAdditionOfDiskDataGridView.Rows[index].Cells[AdddiskColumn].Value = false;
                                                
                                                allServersForm.detailsOfAdditionOfDiskDataGridView.RefreshEdit();
                                                MessageBox.Show("This VM does not belong to the  same source ESX. Please choose another VM.", "Select VM from same ESX ", MessageBoxButtons.OK, MessageBoxIcon.Warning);

                                                return false;
                                            }
                                            else
                                            {
                                                planNamePrepared = allServersForm.detailsOfAdditionOfDiskDataGridView.Rows[index].Cells[PlannameColumn].Value.ToString();
                                                allServersForm.planInput = planNamePrepared;
                                                if (h1.userName == null)
                                                {
                                                    /* AddCredentialsPopUpForm add = new AddCredentialsPopUpForm();
                                                     add.domainText.Text = allServersForm.cachedDomain;
                                                     add.userNameText.Text = allServersForm.cachedUsername;
                                                     add.passWordText.Text = allServersForm.cachedPassword;
                                                     add.ShowDialog();

                                                     if (add.canceled == false)
                                                     {

                                                         h1.userName = add.userName;
                                                         h1.passWord = add.passWord;
                                                         h1.domain = add.domain;
                                                         if (add.useForAllCheckBox.Checked == true)
                                                         {
                                                             foreach (Host tempHost in _sourceList._hostList)
                                                             {
                                                                 if (tempHost.userName == null)
                                                                 {
                                                                     tempHost.userName = add.userName;
                                                                     tempHost.passWord = add.passWord;
                                                                     tempHost.domain = add.domain;
                                                                 }
                                                             }

                                                         }
                                                         _tempSelectedSourceList.AddOrReplaceHost(h1);
                                                     }
                                                     else
                                                     {
                                                         allServersForm.detailsOfAdditionOfDiskDataGridView.Rows[index].Cells[ADD_DISK_COLUMN].Value = false;
                                                         allServersForm.detailsOfAdditionOfDiskDataGridView.RefreshEdit();

                                                     }*/
                                                    temperorySelectedSourceList.AddOrReplaceHost(h1);
                                                }
                                                else
                                                {
                                                    temperorySelectedSourceList.AddOrReplaceHost(h1);

                                                }

                                                //  _tempSelectedSourceList.AddOrReplaceHost(h1);
                                            }
                                        }

                                    }

                                }
                            }

                        }


                        else
                        {
                            bool CheckedTrueForOneVM = false;
                            for (int i = 0; i < allServersForm.detailsOfAdditionOfDiskDataGridView.RowCount; i++)
                            {
                                if ((bool)allServersForm.detailsOfAdditionOfDiskDataGridView.Rows[i].Cells[AdddiskColumn].FormattedValue)
                                {
                                    CheckedTrueForOneVM = true;

                                }
                            }
                            if (CheckedTrueForOneVM == false)
                            {
                                alReadyCalled = false;
                                masterTargetDisplayName = null;
                            }
                            h.displayname = allServersForm.detailsOfAdditionOfDiskDataGridView.Rows[index].Cells[HostnameColumn].Value.ToString();

                            foreach (Host tempHost in temperorySelectedSourceList._hostList)
                            {
                                if (h.displayname == tempHost.displayname)
                                {
                                    h = tempHost;
                                }
                            }
                            temperorySelectedSourceList.RemoveHost(h);
                        }
                    }


                    allServersForm.nextButton.Enabled = true;
                    return true;
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
        private bool SelectedMachine(AllServersForm allServersForm,int listIndex)
        {
            try
            {
               if (planNamePrepared != allServersForm.detailsOfAdditionOfDiskDataGridView.Rows[listIndex].Cells[PlannameColumn].Value.ToString())
                {

                    MessageBox.Show("Kindly choose machines from same plan name.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        allServersForm.detailsOfAdditionOfDiskDataGridView.Rows[listIndex].Cells[AdddiskColumn].Value = false;
                        allServersForm.detailsOfAdditionOfDiskDataGridView.RefreshEdit();
                        return false;
                }
                if ((bool)allServersForm.detailsOfAdditionOfDiskDataGridView.Rows[listIndex].Cells[AdddiskColumn].FormattedValue)
                {
                    if (allServersForm.detailsOfAdditionOfDiskDataGridView.Rows[listIndex].Cells[SourceesxIPColumn].Value.ToString() == sourceEsxIPxml && masterTargetDisplayName == allServersForm.detailsOfAdditionOfDiskDataGridView.Rows[listIndex].Cells[MastertargetDisplaynameColumn].Value.ToString())
                    {
                        foreach (Host h in sourceListPrepared._hostList)
                        {
                            if (allServersForm.detailsOfAdditionOfDiskDataGridView.Rows[listIndex].Cells[HostnameColumn].Value.ToString() == h.displayname)
                            {
                                Debug.WriteLine("Adding to the source host list");
                                temperorySelectedSourceList.AddOrReplaceHost(h);

                                temperorySelectedSourceList.Print();
                            }

                        }

                    }
                    
                    else
                    {
                        allServersForm.detailsOfAdditionOfDiskDataGridView.Rows[listIndex].Cells[AdddiskColumn].Value = false;
                        allServersForm.detailsOfAdditionOfDiskDataGridView.RefreshEdit();
                        MessageBox.Show("Please choose VMs from a single vShpere server and a single master target", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
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
    }
}
