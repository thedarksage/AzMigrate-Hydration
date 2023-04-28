using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.Windows.Forms;
using System.Collections;
using System.IO;

using System.Text.RegularExpressions;
using System.Xml;
using System.Threading;
using Com.Inmage.Tools;
using Com.Inmage;
using System.Drawing;
using System.Net;
using Com.Inmage.Esxcalls;
using HttpCommunication;

namespace Com.Inmage.Wizard
{
    class Esx_SourceToMasterPanelHandler : PanelHandler
    {
        // these are the columns of the datastoredatagridiew...
        internal static int DATASTORE_NAME_COLUMN = 0;
        internal static int DATASTORE_TOTALSPACE_COLUMN = 1;
        internal static int DATASTORE_USEDSPACE_COLUMN = 2;
        internal static int DATASTORE_FREESPACE_COLUMN = 3;
      //  public static int DATASOURCE_SELECTED_COLUMN          = 4;        
        //these are the columns of the souretomasterdatagridview...       
        internal static int SOURCE_SERVER_IP = 0;
        internal static int SIZE_COLUMN = 1;
        internal static int PROCESS_SERVER_IP_COLUMN = 2;
        internal static int RETENTION_SIZE_COLUMN = 3;
        internal static int RETENTION_PATH_COLUMN = 4;
       // public static int RETENTION_PATH_FOR_LINUX              = 5;
        internal static int RETENTION_IN_DAYS_COLUMN = 5;
        internal static int RETENTION_SELECT_COLUMN = 6;
        internal static int CONSISTENCY_TIME_COLUMN = 7;
       // public static int COPYDRIVERS_COLUMN                    = 7;
        internal static int SELECT_DATASTORE_COLUMN = 8;
        internal static int SELECT_LUNS_COLUMN = 9;
        internal static int UNSELECT_MACHINE_COLUMN = 10;
        string enterValue                                       = "Enter value";
        internal string _installPath;
        internal int _dataStore = 0;
        ArrayList diskList                                      = new ArrayList();
        string _pathForDriver = "";
        string _operatingSystem = "";
        int _driverIndex = 0;
        int _cellIndex = 0;
        bool _returnCodeOfDrivers = false;
        internal static string DEFAULT_RETENTION_DIR;
        internal static bool _firstTimeSelection = true;
        internal static string MASTER_CONFIG_FILE = "MasterConfigFile.xml";
        internal string _latestFilePath = null;
        //static AllServersForm _allServersForm;
        
        public Esx_SourceToMasterPanelHandler()          
        {
            _installPath = WinTools.FxAgentPath() + "\\vContinuum";
            _latestFilePath = WinTools.FxAgentPath() + "\\vContinuum\\Latest";
        }
        /*
         * Here we will clear all the comboboxes like datastore and processserver ip comboboxes
         * We will clear the datastore datsgridview and fill with the latest values.
         * we will fil the sourceto masterdatagridview with the latest values we have.
         * we will check for the datastroe values. If they are not null we will fill automatically.
         * At the time of failback/addition of disk. Datastroe combobox is readonly.
         * Using cxcli call getps we will get the ps using the cx ip in vcon box.
         * if already retention/consistency values are there we will fill up that values.
         * We will get the retention drives using cxcli call only.
         * Here we will check for the mt having any fx jobs.
         */
        internal override bool Initialize(AllServersForm allServersForm)
        {
            try
            {
                Trace.WriteLine(DateTime.Now + " \t Entering: P2V_SourceToMasterPanelHandler  initialzie");
                
                // Came here to fill the content in the help panel.....
                allServersForm.helpContentLabel.Text = HelpForcx.ProtectScreen4;

                //Clear the datagrid view 
                allServersForm.SourceToMasterDataGridView.Rows.Clear();
               
                    allServersForm.mandatoryLabel.Visible = true;
                    allServersForm.mandatorydoubleFieldLabel.Visible = true;
                

                allServersForm.previousButton.Visible = true;

                allServersForm.ignoreDatastoreSizeCalculationCheckBox.Checked = true;

                if (allServersForm.osTypeSelected == OStype.Linux || allServersForm.osTypeSelected == OStype.Solaris)
                {
                    DEFAULT_RETENTION_DIR = "/Retention_Logs";
                }
                else if (allServersForm.osTypeSelected == OStype.Windows)
                {
                    DEFAULT_RETENTION_DIR = ":\\Retention_Logs";
                }

                string errorMessage = "";

                WmiCode errorCode = WmiCode.Unknown;

                // Before getting the process server ips and retention volumes we are clearing the combox box........
                allServersForm.processServerComboBox.Items.Clear();
                allServersForm.retentionPath.Items.Clear();
                
                if (allServersForm.osTypeSelected == OStype.Linux)
                {
                    if (!Directory.Exists(_installPath + "\\Linux"))
                    {
                       // Directory.CreateDirectory(_installPath + "\\Linux");
                    }
                }
                allServersForm.retentionPath.Items.Clear();
                allServersForm.datastorecolumn.Items.Clear();
                allServersForm.previousButton.Visible = true;

                //getting cx ip from registrey entries
                allServersForm.SourceToMasterDataGridView.Columns[UNSELECT_MACHINE_COLUMN].Visible = false;

                int _rowIndex = 0;
                ArrayList _dataStoreList = new ArrayList();
                if (allServersForm.dataStoreDataGridView.RowCount > 0)
                {
                    allServersForm.dataStoreDataGridView.Rows.Clear();
                }

                // Populate the datastore values in the datastore datagrid view
                foreach (Host h in allServersForm.selectedMasterListbyUser._hostList)
                {
                   // allServersForm.dataStoreDataGridView.RowCount = h.dataStoreList.Count;
                    Trace.WriteLine("+++++++++++++++ printing datastore count " + h.dataStoreList.Count);
                    foreach (DataStore d in h.dataStoreList)
                    {
                        if (h.vSpherehost == d.vSpherehostname)
                        {
                            Trace.WriteLine(DateTime.Now + "\t Free space " + d.freeSpace);
                            allServersForm.blockSize = d.blockSize * 256;
                            allServersForm.dataStoreDataGridView.Rows.Add(1);
                            allServersForm.dataStoreDataGridView.Rows[_rowIndex].Cells[DATASTORE_NAME_COLUMN].Value = d.name;
                            allServersForm.dataStoreDataGridView.Rows[_rowIndex].Cells[DATASTORE_TOTALSPACE_COLUMN].Value = d.totalSize;
                            allServersForm.dataStoreDataGridView.Rows[_rowIndex].Cells[DATASTORE_FREESPACE_COLUMN].Value = d.freeSpace;
                            allServersForm.dataStoreDataGridView.Rows[_rowIndex].Cells[DATASTORE_USEDSPACE_COLUMN].Value = d.totalSize - d.freeSpace;
                            allServersForm.datastorecolumn.Items.Add(d.name);                            
                            _rowIndex++;
                        }
                    }
                }



                //allServersForm._selectedMasterList.Print();
                // allServersForm._selectedSourceList.Print();
                int hostCount;
                int rowIndex = 0;
                hostCount = allServersForm.selectedSourceListByUser._hostList.Count;

                //Fill the datagrid view
                if (hostCount > 0)
                {
                    allServersForm.SourceToMasterDataGridView.RowCount = hostCount;
                    // In case of p2v support for rdm is not there.....
                    if (allServersForm.appNameSelected == AppName.Bmr || (allServersForm.appNameSelected == AppName.Adddisks && ResumeForm.selectedActionForPlan == "BMR Protection"))
                    {
                           BareMetalLoadGridView(allServersForm, rowIndex);
                        
                       // allServersForm.SourceToMasterDataGridView.Columns[SELECT_LUNS_COLUMN].Visible = false;                        
                    }

                    // In case of esx we are fillimg the datagridview with the selected vms...
                    if (allServersForm.appNameSelected == AppName.Esx )
                    {
                        foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                        {
                            diskList = h.disks.GetDiskList;
                            allServersForm.SourceToMasterDataGridView.Rows[rowIndex].Cells[SOURCE_SERVER_IP].Value = h.displayname;
                            allServersForm.SourceToMasterDataGridView.Rows[rowIndex].Cells[SIZE_COLUMN].Value = h.totalSizeOfDisks;
                           
                            // Checking whether any rdm disks are there if so user asked to convert them as vmdk or rdm.....
                            // If rdms are not there we need to fill the column as not applicable....
                            if (h.rdmpDisk == "TRUE" && h.convertRdmpDiskToVmdk == false)
                            {
                                foreach (Hashtable disk in diskList)
                                {
                                    if (disk["Selected"].ToString() == "Yes" && disk["Rdm"].ToString() == "yes")
                                    {
                                        h.rdmdisk = true;
                                    }
                                }
                                if (h.rdmSelected == false && h.rdmdisk == true)
                                {
                                    allServersForm.SourceToMasterDataGridView.Rows[rowIndex].Cells[SELECT_LUNS_COLUMN].Value = "Select luns";
                                }
                                else if (h.rdmSelected == true)
                                {
                                    allServersForm.SourceToMasterDataGridView.Rows[rowIndex].Cells[SELECT_LUNS_COLUMN].Value = "Available";
                                }
                                else if (h.rdmdisk == false)
                                {
                                    allServersForm.SourceToMasterDataGridView.Rows[rowIndex].Cells[SELECT_LUNS_COLUMN].Value = "Not applicable";
                                }
                            }
                            else
                            {
                                allServersForm.SourceToMasterDataGridView.Rows[rowIndex].Cells[SELECT_LUNS_COLUMN].Value = "Not applicable";
                            }
                            rowIndex++;
                        }
                    }


                    if (allServersForm.appNameSelected == AppName.Failback || (allServersForm.appNameSelected == AppName.Adddisks && ResumeForm.selectedActionForPlan == "ESX"))
                    {
                    
                        //allServersForm.SourceToMasterDataGridView.Columns[SELECT_LUNS_COLUMN].Visible = false;
                        foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                        {
                            allServersForm.SourceToMasterDataGridView.Rows[rowIndex].Cells[SOURCE_SERVER_IP].Value = h.displayname;
                            allServersForm.SourceToMasterDataGridView.Rows[rowIndex].Cells[SIZE_COLUMN].Value = h.totalSizeOfDisks;

                            // In case of failback and addition of disk we need to fill the datastore column......
                            if (h.targetDataStore != null)
                            {
                                // MessageBox.Show("Before showing the datagrid " + h.targetDataStore);
                                allServersForm.SourceToMasterDataGridView.Rows[rowIndex].Cells[SELECT_DATASTORE_COLUMN].Value = h.targetDataStore;
                                allServersForm.SourceToMasterDataGridView.Rows[rowIndex].Cells[SELECT_DATASTORE_COLUMN].ReadOnly = true;
                                // MessageBox.Show(h.targetDataStore);
                                Host masterHost = (Host)allServersForm.selectedMasterListbyUser._hostList[0];
                                if (h.resourcepoolgrpname_src != null)
                                {
                                    bool resourcePoolExists = false;
                                    foreach (ResourcePool resource in masterHost.resourcePoolList)
                                    {
                                        if (masterHost.vSpherehost == resource.host)
                                        {
                                            if (h.resourcepoolgrpname_src == resource.groupId)
                                            {
                                                resourcePoolExists = true;
                                                h.resourcePool = resource.name;
                                                h.resourcepoolgrpname = resource.groupId;
                                            }
                                        }
                                    }
                                    if (resourcePoolExists == false)
                                    {
                                        h.resourcepoolgrpname = masterHost.resourcepoolgrpname;
                                        h.resourcePool = masterHost.resourcePool;
                                    }
                                }                                
                            }
                            else
                            {
                                

                                Host masterHost = (Host)allServersForm.selectedMasterListbyUser._hostList[0];
                                if (h.resourcepoolgrpname_src != null)
                                {
                                    bool resourcePoolExists = false;
                                    foreach (ResourcePool resource in masterHost.resourcePoolList)
                                    {
                                        if (masterHost.vSpherehost == resource.host)
                                        {
                                            if (h.resourcepoolgrpname_src == resource.groupId)
                                            {
                                                resourcePoolExists = true;
                                                h.resourcePool = resource.name;
                                                h.resourcepoolgrpname = resource.groupId;
                                            }
                                        }
                                    }
                                    if (resourcePoolExists == false)
                                    {
                                        h.resourcepoolgrpname = masterHost.resourcepoolgrpname;
                                        h.resourcePool = masterHost.resourcePool; 
                                    }
                                }
                                else
                                {
                                    h.resourcePool = masterHost.resourcePool;
                                    h.resourcepoolgrpname = masterHost.resourcepoolgrpname;
                                }
                            }
                            // In case of addition of disk we are checking for the rdm disks are present or not.....
                            if ((allServersForm.appNameSelected == AppName.Adddisks && ResumeForm.selectedActionForPlan == "ESX"))
                            {
                                diskList = h.disks.GetDiskList;
                                if (h.rdmpDisk == "TRUE" && h.convertRdmpDiskToVmdk == false)
                                {
                                    foreach (Hashtable disk in diskList)
                                    {
                                        if (disk["Selected"].ToString() == "Yes" && disk["Rdm"].ToString() == "yes")
                                        {
                                            h.rdmdisk = true;
                                        }
                                    }
                                    if (h.rdmSelected == false && h.rdmdisk == true)
                                    {
                                        allServersForm.SourceToMasterDataGridView.Rows[rowIndex].Cells[SELECT_LUNS_COLUMN].Value = "Select luns";
                                    }
                                    else if (h.rdmSelected == true)
                                    {
                                        allServersForm.SourceToMasterDataGridView.Rows[rowIndex].Cells[SELECT_LUNS_COLUMN].Value = "Available";
                                    }
                                    else if (h.rdmdisk == false)
                                    {
                                        allServersForm.SourceToMasterDataGridView.Rows[rowIndex].Cells[SELECT_LUNS_COLUMN].Value = "Not applicable";
                                    }
                                }
                                else
                                {
                                    allServersForm.SourceToMasterDataGridView.Rows[rowIndex].Cells[SELECT_LUNS_COLUMN].Value = "Not applicable";
                                }
                            }
                            else if (allServersForm.appNameSelected == AppName.Failback)
                            {
                                allServersForm.SourceToMasterDataGridView.Columns[SELECT_LUNS_COLUMN].Visible = false;
                            }

                            if (allServersForm.appNameSelected == AppName.Adddisks)
                            {                               
                                allServersForm.SourceToMasterDataGridView.Columns[CONSISTENCY_TIME_COLUMN].Visible = false;
                            }
                            rowIndex++;
                        }
                    }

                    // In case of offline sync protection. RDM support is not there....
                    if (allServersForm.appNameSelected == AppName.Offlinesync)
                    {
                        allServersForm.ignoreDatastoreSizeCalculationCheckBox.Visible = false;
                        if (ResumeForm.selectedActionForPlan == "P2v")
                        {
                          //  if (allServersForm._osType == OStype.Windows)
                            {
                                BareMetalLoadGridView(allServersForm, rowIndex);
                            }
                            allServersForm.SourceToMasterDataGridView.Columns[SELECT_LUNS_COLUMN].Visible = false;
                        }
                        else
                        {
                            foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                            {
                                allServersForm.SourceToMasterDataGridView.Rows[rowIndex].Cells[SOURCE_SERVER_IP].Value = h.displayname;
                                allServersForm.SourceToMasterDataGridView.Rows[rowIndex].Cells[SIZE_COLUMN].Value = h.totalSizeOfDisks;
                                rowIndex++;
                            }

                        }
                        allServersForm.SourceToMasterDataGridView.Columns[SELECT_LUNS_COLUMN].Visible = false;
                    }

                    foreach (Host h in allServersForm.selectedMasterListbyUser._hostList)
                    {
                        allServersForm.selectedTargetsByUser.Add(h.displayname);
                    }


                    

                    // Here we are checking that current master target having any fx jobs running and 
                    // Does retention drives are available or not.....
                    // If not we will through one pop up and go to previous screen.....
                    if (MasterTargetChecks(allServersForm) == false)
                    {
                        object o = new object();
                        EventArgs e = new EventArgs();
                        allServersForm.previousButton_Click(o, e);

                    }



                    GetProcessServerIP(allServersForm);
                    Host temp = (Host)allServersForm.selectedSourceListByUser._hostList[0];
                    if (allServersForm.appNameSelected == AppName.Bmr)
                    {
                        //  allServersForm.detailsOfDataStoreGridViewLabel.Text = "Select datastore for " + temp.ip;
                    }
                    else if (allServersForm.appNameSelected == AppName.Offlinesync)
                    {
                        if (ResumeForm.selectedActionForPlan != "Esx")
                        {
                            // allServersForm.detailsOfDataStoreGridViewLabel.Text = "Select datastore for " + temp.ip;
                        }
                    }
                    else
                    {
                        // allServersForm.detailsOfDataStoreGridViewLabel.Text = "Select datastore for " + temp.displayname;

                    }


                    allServersForm.hoursordaysColumn.Items.Clear();
                    allServersForm.hoursordaysColumn.Items.Add("days");
                    allServersForm.hoursordaysColumn.Items.Add("hours");
                    
                    // Filling the datagridview using the datastructures
                    for (int i = 0; i < allServersForm.SourceToMasterDataGridView.RowCount; i++)
                    {
                        foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                        {
                            if (allServersForm.appNameSelected == AppName.Bmr || (allServersForm.appNameSelected == AppName.Adddisks && ResumeForm.selectedActionForPlan == "BMR Protection"))
                            {
                                if (h.ip == allServersForm.SourceToMasterDataGridView.Rows[i].Cells[SOURCE_SERVER_IP].Value.ToString())
                                {
                                    if (h.retentionLogSize == null)
                                    {
                                        allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_SIZE_COLUMN].Value = enterValue;
                                    }
                                    else
                                    {
                                        allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_SIZE_COLUMN].Value = h.retentionLogSize;
                                    }
                                    if (h.consistencyInmins == null)
                                    {
                                        if (allServersForm.appNameSelected == AppName.Adddisks)
                                        {
                                            allServersForm.SourceToMasterDataGridView.Columns[CONSISTENCY_TIME_COLUMN].Visible = false;
                                            allServersForm.SourceToMasterDataGridView.Rows[i].Cells[CONSISTENCY_TIME_COLUMN].Value = 30;
                                        }
                                        else
                                        {
                                            allServersForm.SourceToMasterDataGridView.Rows[i].Cells[CONSISTENCY_TIME_COLUMN].Value = enterValue;
                                        }                                       
                                    }
                                    else
                                    {
                                        allServersForm.SourceToMasterDataGridView.Rows[i].Cells[CONSISTENCY_TIME_COLUMN].Value = h.consistencyInmins;

                                    }
                                    if (h.retentionInDays == null)
                                    {
                                        allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_IN_DAYS_COLUMN].Value = enterValue;
                                    }
                                    else
                                    {
                                        allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_IN_DAYS_COLUMN].Value = h.retentionInDays;
                                        allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_SELECT_COLUMN].Value = "days";
                                    }
                                    if (h.retentionInHours != 0)
                                    {
                                        allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_IN_DAYS_COLUMN].Value = h.retentionInHours;
                                        allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_SELECT_COLUMN].Value = "hours";
                                    }
                                    
                                    if (h.processServerIp != null)
                                    {
                                        allServersForm.SourceToMasterDataGridView.Rows[i].Cells[PROCESS_SERVER_IP_COLUMN].Value = h.processServerIp;
                                    }
                                    if (h.targetDataStore != null)
                                    {
                                        allServersForm.SourceToMasterDataGridView.Rows[i].Cells[SELECT_DATASTORE_COLUMN].Value = h.targetDataStore;
                                        if (allServersForm.appNameSelected == AppName.Adddisks)
                                        {
                                            allServersForm.SourceToMasterDataGridView.Rows[i].Cells[SELECT_DATASTORE_COLUMN].ReadOnly = true;
                                        }
                                    }
                                    else
                                    {
                                        allServersForm.SourceToMasterDataGridView.Rows[i].Cells[SELECT_DATASTORE_COLUMN].Value = null;
                                        allServersForm.SourceToMasterDataGridView.RefreshEdit();
                                    }
                                }
                            }
                            else
                            {
                                if ((h.displayname == allServersForm.SourceToMasterDataGridView.Rows[i].Cells[SOURCE_SERVER_IP].Value.ToString()) || (h.ip == allServersForm.SourceToMasterDataGridView.Rows[i].Cells[SOURCE_SERVER_IP].Value.ToString()))
                                {
                                    if (h.retentionLogSize == null)
                                    {
                                        allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_SIZE_COLUMN].Value = enterValue;
                                    }
                                    else
                                    {
                                        allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_SIZE_COLUMN].Value = h.retentionLogSize;
                                    }
                                    if (h.consistencyInmins == null)
                                    {
                                        if (allServersForm.appNameSelected == AppName.Adddisks)
                                        {
                                            allServersForm.SourceToMasterDataGridView.Rows[i].Cells[CONSISTENCY_TIME_COLUMN].Value = "30";
                                        }
                                        else
                                        {   
                                            allServersForm.SourceToMasterDataGridView.Rows[i].Cells[CONSISTENCY_TIME_COLUMN].Value = enterValue;
                                        }
                                    }
                                    else
                                    {
                                        allServersForm.SourceToMasterDataGridView.Rows[i].Cells[CONSISTENCY_TIME_COLUMN].Value = h.consistencyInmins;
                                    }
                                    if (h.retentionInDays == null)
                                    {
                                        allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_IN_DAYS_COLUMN].Value = enterValue;
                                    }
                                    else
                                    {
                                        allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_IN_DAYS_COLUMN].Value = h.retentionInDays;
                                        allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_SELECT_COLUMN].Value = "days";
                                    }
                                    if (h.retentionInHours != 0)
                                    {
                                        allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_IN_DAYS_COLUMN].Value = h.retentionInHours;
                                        allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_SELECT_COLUMN].Value = "hours";
                                    }
                                    if (h.processServerIp != null)
                                    {
                                        allServersForm.SourceToMasterDataGridView.Rows[i].Cells[PROCESS_SERVER_IP_COLUMN].Value = h.processServerIp;
                                    }
                                    if (h.targetDataStore != null)
                                    {
                                        allServersForm.SourceToMasterDataGridView.Rows[i].Cells[SELECT_DATASTORE_COLUMN].Value = h.targetDataStore;
                                    }
                                    else
                                    {
                                        allServersForm.SourceToMasterDataGridView.Rows[i].Cells[SELECT_DATASTORE_COLUMN].Value = null;
                                        allServersForm.SourceToMasterDataGridView.RefreshEdit();
                                    }
                                }

                            }
                        }
                    }
                    // Getting the process servers from the cx using cxcli....                   

                    // allServersForm._selectedSourceList.Print();
                    for (int i = 0; i < allServersForm.SourceToMasterDataGridView.RowCount; i++)
                    {
                        if (allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_SELECT_COLUMN].Value == null)
                        {
                            allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_SELECT_COLUMN].Value = allServersForm.hoursordaysColumn.Items[0];
                        }
                    }
                    Trace.WriteLine(DateTime.Now + "\t Exiting: P2V_SourceToMasterPanelHandler  initialzie  ");
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
            for (int i = 0; i < allServersForm.SourceToMasterDataGridView.RowCount; i++)
            {
                allServersForm.SourceToMasterDataGridView.Rows[i].Cells[UNSELECT_MACHINE_COLUMN].Value = "Unselect";
                allServersForm.SourceToMasterDataGridView.RefreshEdit();
            }
            if (allServersForm.appNameSelected == AppName.Failback)
            {               
                allServersForm.thinProvisionSelected = false;
            }

            return true;
        }

        /*
         * This will be called by initialze of this panel handler.
         * here we will use cxcli call to check if any fx jobs are there for the master target.
         * after that we will get the retention drive using cxcli call.
         */ 
        private bool MasterTargetChecks(AllServersForm allServersForm)
        {

            //Getting the drive letters from the master target
            Host masterHostForRetentionDrive = (Host)allServersForm.selectedMasterListbyUser._hostList[0];
            masterHostForRetentionDrive.disks.partitionList.Clear();


            // RemoteWin rw = new RemoteWin(masterHost.ip, masterHost.domain, masterHost.userName, masterHost.passWord);
            //rw.Connect(ref errorMessage, ref errorCode);
            Trace.WriteLine(DateTime.Now + " \t Came here to check whether the mt is in use or not");
            WinPreReqs win = new WinPreReqs("", "", "", "");
            Trace.WriteLine(DateTime.Now + "\t Printing mt host name and ip " + masterHostForRetentionDrive.hostname + masterHostForRetentionDrive.ip);

            // Checking whether the master target having any fx jobs a=or not 
            // If any fx jobs are there we need to block here...
            // That is not supported...
            //if (win.MasterTargetPreCheck(ref masterHostForRetentionDrive, WinTools.GetcxIPWithPortNumber()) == true)
            //{
            //    if (masterHostForRetentionDrive.machineInUse == true)
            //    {
            //        Trace.WriteLine(DateTime.Now + " \t Came here to know that mt is in use");
            //        string message = "This VM can't be used as master target. Some Fx jobs are running on this master target." + Environment.NewLine
            //            + "Select another master target";
            //        MessageBox.Show(message, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            //        allServersForm.planNameLabel.Visible = false;
            //        allServersForm.planNameText.Visible = false;
            //        allServersForm.nextButton.Enabled = true;

            //        return false;
            //    }
            //}
            
            // Getting retention drives from the master target using cxcli call if it fail
            // We will sleep for 1 secong and again we will try ......
            //string cxIP= WinTools.GetcxIPWithPortNumber();
            //Host temp = new Host();
            //temp.hostname = masterHostForRetentionDrive.hostname;
            //temp.ip = masterHostForRetentionDrive.ip;
            //win.GetDetailsFromcxcli(ref temp, cxIP);
            //if (temp.inmage_hostid == null)
            //{
            //    Trace.WriteLine(DateTime.Now + " \t Came here because inmage uuid got null ");
            //    MessageBox.Show("Master target:" + masterHostForRetentionDrive.displayname + " (" + masterHostForRetentionDrive.ip + " )" + " information is not found in the CX server " +
            //  Environment.NewLine + Environment.NewLine + "Please verify that" +
            //  Environment.NewLine +
            //  Environment.NewLine + "1. Agent is installed" +
            //  Environment.NewLine + "2. Master target is up and running" +
            //  Environment.NewLine + "3. vContinuum wizard is pointing to " + WinTools.GetcxIP() + " with port number " + WinTools.GetCxPortNumber()() +
            //  Environment.NewLine + "     Make sure that both master target and vContinuum wizard are pointed to same CX." +
            //  Environment.NewLine + "4. InMage Scout services are up and running" +
            //    Environment.NewLine + "5. Master target is licenced in CX.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);

            //    return false;
            //}
            //else
            //{
            //    Trace.WriteLine(DateTime.Now + "\t Inmage uuid " + temp.inmage_hostid + "\t hostname " + temp.hostname);
            //    masterHostForRetentionDrive.inmage_hostid = temp.inmage_hostid;
            //}
            if (masterHostForRetentionDrive.disks.GetPartitionsFreeSpace( masterHostForRetentionDrive) == false)
            {
                Thread.Sleep(1000);
                if (masterHostForRetentionDrive.disks.GetPartitionsFreeSpace( masterHostForRetentionDrive) == false)
                {
                    Trace.WriteLine(DateTime.Now + " \t Came here to rerun  GetPartitionsFreeSpace() in Initialize of ESX_SourceToMasterTargetPanelHandler");
                    MessageBox.Show("Master target:" + masterHostForRetentionDrive.displayname + " (" + masterHostForRetentionDrive.ip + " )" + " information is not found in the CX server " +
                  Environment.NewLine + Environment.NewLine + "Please verify that" +
                  Environment.NewLine +
                  Environment.NewLine + "1. Agent is installed" +
                  Environment.NewLine + "2. Master target is up and running" +
                  Environment.NewLine + "3. vContinuum wizard is pointing to " + WinTools.GetcxIP() + " with port number " + WinTools.GetcxPortNumber() +
                  Environment.NewLine + "     Make sure that both master target and vContinuum wizard are pointed to same CX." +
                  Environment.NewLine + "4. InMage Scout services are up and running" +
                    //Environment.NewLine + "5. Master target is licenced in CX.", "Error" +
                    Environment.NewLine + "5. Check whether proxy server configured in the web browser (IE&Firefox).", "Error",MessageBoxButtons.OK, MessageBoxIcon.Error);

                   
                    return false;
                }
            }
            masterHostForRetentionDrive = DiskList.GetHost;
            masterHostForRetentionDrive.Print();

            //Filling the retention drives in to the combobox.....
            ComboBox box = new ComboBox();
            allServersForm.retentionPath.Items.Clear();
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

                                allServersForm.retentionPath.Items.Add(hash["Name"]);
                                box.Items.Add(hash["Name"]);
                            }
                        }
                    }
                }
            }

            // Checking for the count of the retention drives.....
            if (box.Items.Count > 0)
            {
                for (int i = 0; i < allServersForm.SourceToMasterDataGridView.RowCount; i++)
                {
                    allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_PATH_COLUMN].Value = box.Items[0];
                }
            }
            else
            {
                MessageBox.Show("There are no partitions available for retention. Create partitions on master target and try again.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);           
                return false;
            }
            return true;
        }

        /*
         * Using cxcli call we will get the processserver ips.
         * If there are no ps ips we will block user to go for next screens.
         * First one will be selected defaultly.
         */
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
                    using (XmlReader reader1 = XmlReader.Create(_latestFilePath + WinPreReqs.tempXml, settings))
                    {
                        xDocument.Load(reader1);
                       // reader1.Close();
                    }
                    //xDocument.Load(_latestFilePath + WinPreReqs.tempXml);
                    
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
                    allServersForm.processServerComboBox.Items.Add(xNode.InnerText != null ? xNode.InnerText : "");
                }
                for (int i = 0; i < allServersForm.SourceToMasterDataGridView.RowCount; i++)
                {
                    try
                    {
                        allServersForm.SourceToMasterDataGridView.Rows[i].Cells[PROCESS_SERVER_IP_COLUMN].Value = allServersForm.processServerComboBox.Items[0];
                    }
                    catch (Exception e)
                    {
                        MessageBox.Show("Process server ip is not reachable", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
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
         * In case of p2v we will call this method. TO fill the datagridview.
         * In this case we will show ip in the first column of the datagridview.
         * because we don't know the exact displayname of the physical machine.
         * In case of win2k3 32/64 machine we need to get the symmpi.sys.
         * That file will be used to boot the machine.
         */
        internal bool BareMetalLoadGridView(AllServersForm allServersForm, int rowIndex)
        {
            try
            {
                // In case of p2v we will check whether the machines are 2003 or 2008
                // if 2003 machines user has to select driver file from the boot cd or folder..
                // we will copy that file into the windows folder with the 32 or 64 sub folders....
                // while loading inot the datagrid view we will check the folder and we wont ask user to re select 
                // we will reuse the same file if exists....
                foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                {
                    //allServersForm.configureSourceServerDataGridView.Rows.Add(1);                    
                    allServersForm.SourceToMasterDataGridView.Rows[rowIndex].Cells[SIZE_COLUMN].Value = h.totalSizeOfDisks;
                    allServersForm.SourceToMasterDataGridView.Rows[rowIndex].Cells[SOURCE_SERVER_IP].Value = h.ip;
                    //if (allServersForm._appName != AppName.ADDDISKS && allServersForm._osType != OStype.Linux)
                    //{
                    //    //if (h.operatingSystem.Contains("2003"))
                    //    //{
                    //    //    if (File.Exists(_installPath + "\\windows" + "\\" + h.operatingSystem + "\\symmpi.sys"))
                    //    //    {
                    //    //        allServersForm.SourceToMasterDataGridView.Rows[rowIndex].Cells[COPYDRIVERS_COLUMN].Value = "Available";
                    //    //    }
                    //    //    else
                    //    //    {
                    //    //        allServersForm.SourceToMasterDataGridView.Rows[rowIndex].Cells[COPYDRIVERS_COLUMN].Value = "Provide OS CD";
                    //    //    }
                    //    //}
                    //    //else if (h.operatingSystem.Contains("XP"))
                    //    //{
                    //    //    if (File.Exists(_installPath + "\\windows" + "\\" + h.operatingSystem + "\\symmpi.sys"))
                    //    //    {
                    //    //        allServersForm.SourceToMasterDataGridView.Rows[rowIndex].Cells[COPYDRIVERS_COLUMN].Value = "Available";
                    //    //    }
                    //    //    else
                    //    //    {
                    //    //        allServersForm.SourceToMasterDataGridView.Rows[rowIndex].Cells[COPYDRIVERS_COLUMN].Value = "Provide OS CD";
                    //    //    }
                    //    //}
                    //    //else if (h.operatingSystem.Contains("2008"))
                    //    //{
                    //    //    // allServersForm.SourceToMasterDataGridView.Rows[rowIndex].Cells[COPYDRIVERS_COLUMN].Value     = "";
                    //    //    allServersForm.SourceToMasterDataGridView.Rows[rowIndex].Cells[COPYDRIVERS_COLUMN].Value = "Available";
                    //    //}
                    //    //else
                    //    //{
                    //    //    allServersForm.SourceToMasterDataGridView.Rows[rowIndex].Cells[COPYDRIVERS_COLUMN].Value = "Available";
                    //    //}
                    //}
                    //else
                    //{
                    //    allServersForm.SourceToMasterDataGridView.Columns[COPYDRIVERS_COLUMN].Visible = false;
                    //}
                    allServersForm.SourceToMasterDataGridView.Rows[rowIndex].Cells[SELECT_LUNS_COLUMN].Value = "Select luns";
                    rowIndex++;
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
         * HEre we will assign the values of the retention/consistency givenby the user.
         * In this method we will check for the block size of the target datstore.
         * If it is not sufficient we will ask user to increase of to select another datastore.
         * For the retention drive we will assingn REtention_log folderin case of windows and linux.
         */
        internal override bool ProcessPanel(AllServersForm allServersForm)
        {
            Trace.WriteLine(DateTime.Now + " \t Entered: ProcessPanel() in ESX_SourceToMasterTargetPanelHandler  ");
            try
            {
                for (int i = 0; i < allServersForm.SourceToMasterDataGridView.RowCount; i++)
                {
                    allServersForm.SourceToMasterDataGridView.Rows[i].Cells[CONSISTENCY_TIME_COLUMN].Selected = false;
                    allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_IN_DAYS_COLUMN].Selected = false;
                    allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_SIZE_COLUMN].Selected = false;
                }
               

                // For each source machines we need to assign retention log size, consisteny time interval
                // process servers ip , data store and all the inputs given by the user.....
                for (int i = 0; i < allServersForm.SourceToMasterDataGridView.RowCount; i++)
                {
                    foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                    {
                        if (allServersForm.appNameSelected == AppName.Esx || (allServersForm.appNameSelected == AppName.Adddisks || ResumeForm.selectedActionForPlan == "ESX") || allServersForm.appNameSelected == AppName.Failback)
                        {
                            if (allServersForm.SourceToMasterDataGridView.Rows[i].Cells[SOURCE_SERVER_IP].Value.ToString() == h.displayname)
                            {
                                //In case of any monut point selected for rentention drive we are checking for : and we are writing retetnion logs only for the drive. 
                                if (allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_PATH_COLUMN].Value.ToString().Contains(":"))
                                {
                                    h.retentionPath = allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_PATH_COLUMN].Value.ToString() + "\\Retention_Logs";
                                }
                                else
                                {
                                    h.retentionPath = allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_PATH_COLUMN].Value.ToString() + DEFAULT_RETENTION_DIR;
                                }
                                h.retention_drive = allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_PATH_COLUMN].Value.ToString();
                                if (allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_SIZE_COLUMN].Value != null)
                                {
                                    if (allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_SIZE_COLUMN].Value.ToString() != "Enter value")
                                    {

                                        h.retentionLogSize = allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_SIZE_COLUMN].Value.ToString();
                                    }
                                }
                                if (allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_IN_DAYS_COLUMN].Value != null)
                                {
                                    if (allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_IN_DAYS_COLUMN].Value.ToString() != "Enter value" && allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_SELECT_COLUMN].Value.ToString() == "days")
                                    {
                                        h.retentionInDays = allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_IN_DAYS_COLUMN].Value.ToString();
                                        h.retentionInHours = 0;
                                    }
                                    else if (allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_IN_DAYS_COLUMN].Value.ToString() != "Enter value" && allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_SELECT_COLUMN].Value.ToString() == "hours")
                                    {
                                        h.retentionInHours = int.Parse(allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_IN_DAYS_COLUMN].Value.ToString());
                                        h.retentionInDays = null;
                                    }
                                }
                                if (allServersForm.SourceToMasterDataGridView.Rows[i].Cells[PROCESS_SERVER_IP_COLUMN].Value != null)
                                {
                                    h.processServerIp = allServersForm.SourceToMasterDataGridView.Rows[i].Cells[PROCESS_SERVER_IP_COLUMN].Value.ToString();
                                }
                                else
                                {
                                    MessageBox.Show("Process server IP cell is empty make sure that should have IP value", "No IP value", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                    Trace.WriteLine(DateTime.Now + "\t ESX_SourceToMasterPanelHandler: ProcessPanel: Process server IP cell is empty make sure that should have IP value");
                                    return false;
                                }
                                if (allServersForm.SourceToMasterDataGridView.Rows[i].Cells[CONSISTENCY_TIME_COLUMN].Value.ToString() != "Enter value")
                                {
                                    h.consistencyInmins = allServersForm.SourceToMasterDataGridView.Rows[i].Cells[CONSISTENCY_TIME_COLUMN].Value.ToString();
                                }
                                h.consistencyInDays = "0";
                                h.consistencyInHours = "0";
                            }
                        }
                        if (allServersForm.appNameSelected == AppName.Bmr || (allServersForm.appNameSelected == AppName.Adddisks && ResumeForm.selectedActionForPlan == "BMR Protection"))
                        {
                            if (allServersForm.SourceToMasterDataGridView.Rows[i].Cells[SOURCE_SERVER_IP].Value.ToString() == h.ip)
                            {
                                if (allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_PATH_COLUMN].Value.ToString().Contains(":"))
                                {
                                    h.retentionPath = allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_PATH_COLUMN].Value.ToString() + "\\Retention_Logs";
                                }
                                else
                                {
                                    h.retentionPath = allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_PATH_COLUMN].Value.ToString() + DEFAULT_RETENTION_DIR;
                                }
                                h.retention_drive = allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_PATH_COLUMN].Value.ToString();
                                if (allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_SIZE_COLUMN].Value != null )
                                {
                                    if (allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_SIZE_COLUMN].Value.ToString() != "Enter value")
                                    {
                                        h.retentionLogSize = allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_SIZE_COLUMN].Value.ToString();
                                    }
                                }
                                if (allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_IN_DAYS_COLUMN].Value != null)
                                {
                                    if (allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_IN_DAYS_COLUMN].Value.ToString() != "Enter value" && allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_SELECT_COLUMN].Value.ToString() == "days")
                                    {
                                        h.retentionInDays = allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_IN_DAYS_COLUMN].Value.ToString();
                                        h.retentionInHours = 0;
                                    }
                                    else if (allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_IN_DAYS_COLUMN].Value.ToString() != "Enter value" && allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_SELECT_COLUMN].Value.ToString() == "hours")
                                    {
                                        h.retentionInHours = int.Parse(allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_IN_DAYS_COLUMN].Value.ToString());
                                        h.retentionInDays = null;
                                    }
                                }
                                if (allServersForm.SourceToMasterDataGridView.Rows[i].Cells[PROCESS_SERVER_IP_COLUMN].Value != null)
                                {
                                    h.processServerIp = allServersForm.SourceToMasterDataGridView.Rows[i].Cells[PROCESS_SERVER_IP_COLUMN].Value.ToString();
                                }
                                else
                                {
                                    MessageBox.Show("Process server IP cell is empty make sure that should have IP value", "No IP value", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                    Trace.WriteLine(DateTime.Now + "\t ESX_SourceToMasterPanelHandler: ProcessPanel: Process server IP cell is empty make sure that should have IP value");
                                    return false;
                                }
                                if (allServersForm.SourceToMasterDataGridView.Rows[i].Cells[CONSISTENCY_TIME_COLUMN].Value.ToString() != "Enter value")
                                {
                                    h.consistencyInmins = allServersForm.SourceToMasterDataGridView.Rows[i].Cells[CONSISTENCY_TIME_COLUMN].Value.ToString();
                                }
                                h.consistencyInDays = "0";
                                h.consistencyInHours = "0";
                            }
                        }
                        if (allServersForm.appNameSelected == AppName.Offlinesync)
                        {
                            //  if (MainForm._selectedItemInComboBox == "Esx")
                            {
                                if ((allServersForm.SourceToMasterDataGridView.Rows[i].Cells[SOURCE_SERVER_IP].Value.ToString() == h.displayname) || (allServersForm.SourceToMasterDataGridView.Rows[i].Cells[SOURCE_SERVER_IP].Value.ToString() == h.ip))
                                {

                                    if (allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_PATH_COLUMN].Value.ToString().Contains(":"))
                                    {
                                        h.retentionPath = allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_PATH_COLUMN].Value.ToString() + "\\Retention_Logs";
                                    }
                                    else
                                    {
                                        h.retentionPath = allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_PATH_COLUMN].Value.ToString() + DEFAULT_RETENTION_DIR;
                                    }
                                    h.retention_drive = allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_PATH_COLUMN].Value.ToString();
                                    if (allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_SIZE_COLUMN].Value.ToString() != "Enter value")
                                    {
                                        h.retentionLogSize = allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_SIZE_COLUMN].Value.ToString();
                                    }
                                    if (allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_IN_DAYS_COLUMN].Value.ToString() != "Enter value" && allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_SELECT_COLUMN].Value.ToString() == "days")
                                    {
                                        h.retentionInDays = allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_IN_DAYS_COLUMN].Value.ToString();
                                        h.retentionInHours = 0;
                                    }
                                    else if (allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_IN_DAYS_COLUMN].Value.ToString() != "Enter value" && allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_SELECT_COLUMN].Value.ToString() == "hours")
                                    {
                                        h.retentionInHours = int.Parse(allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_IN_DAYS_COLUMN].Value.ToString());
                                        h.retentionInDays = null;
                                    }
                                    if (allServersForm.SourceToMasterDataGridView.Rows[i].Cells[PROCESS_SERVER_IP_COLUMN].Value != null)
                                    {
                                        h.processServerIp = allServersForm.SourceToMasterDataGridView.Rows[i].Cells[PROCESS_SERVER_IP_COLUMN].Value.ToString();
                                    }
                                    else
                                    {
                                        MessageBox.Show("Process server IP cell is empty make sure that should have IP value", "No IP value", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                        Trace.WriteLine(DateTime.Now + "\t ESX_SourceToMasterPanelHandler: ProcessPanel: Process server IP cell is empty make sure that should have IP value");
                                        return false;
                                    }
                                    if (allServersForm.SourceToMasterDataGridView.Rows[i].Cells[CONSISTENCY_TIME_COLUMN].Value.ToString() != "Enter value")
                                    {
                                        h.consistencyInmins = allServersForm.SourceToMasterDataGridView.Rows[i].Cells[CONSISTENCY_TIME_COLUMN].Value.ToString();
                                    }
                                    h.consistencyInDays = "0";
                                    h.consistencyInHours = "0";
                                }
                            }
                        }
                    }
                }

                // Minimum value of the retentionlogsize is 256 if user gave input less than 256
                // we need to block here.....
                // Minimum days of retention day is 1 we need to check that also 
                // Even one is fine we need to procced with single values......
                foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                {
                    if (h.retentionInDays != null && h.retentionInDays.StartsWith("-"))
                    {
                        MessageBox.Show("Minimum value of retention days is 1. Please enter a value greater than 1", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return false;
                    }
                    if (h.retentionLogSize != null && h.retentionLogSize.StartsWith("-"))
                    {
                        MessageBox.Show("Minimum value of retention Size is 256. Please enter a value greater than 256", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return false;
                    }
                    if (h.retentionInHours < 0)
                    {
                        MessageBox.Show("Minimum value of retention hours is 1. Please enter a value greater than 0", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return false;
                    }
                    if (h.retentionLogSize != null)
                    {
                        if (Convert.ToInt32(h.retentionLogSize) < 256 && h.retentionInHours == 0)
                        {
                            if (h.retentionInDays == null || h.retentionInDays == "0"  || Convert.ToInt32(h.retentionInDays) < 0)
                            {
                                MessageBox.Show("Minimum value of retention store size is 256 MB. Please enter a value greater than 256MB", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                return false;
                            }
                            else
                            {
                                h.retentionLogSize = null;
                            }
                        }
                    }
                    if (h.retentionLogSize == null || h.retentionLogSize == "0")
                    {
                        if (h.retentionInDays != null)
                        {
                            if (Convert.ToInt32(h.retentionInDays) == 0 || Convert.ToInt32(h.retentionInDays) < 0)
                            {
                                MessageBox.Show("Minimum value of retention days is 1. Please enter a value greater than 1", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                return false;
                            }
                        }
                    }

                    if (h.retentionInHours == 0 && h.retentionInDays == null && h.retentionLogSize == null)
                    {
                        MessageBox.Show("Enter valid retention values to proceed.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return false;
                    }
                    if ((h.retentionInHours > 23 || h.retentionInHours < 0) && h.retentionInDays == null)
                    {
                        MessageBox.Show("Minimum value of hours is 1. Please enter a value greater than 0 less than 24", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return false;
                    }
                
                    // Minimum consistency interval is 3 mins if user enters less than 3 we need to block here....
                    if (h.consistencyInmins != null)
                    {
                        if (Convert.ToInt32(h.consistencyInmins) < 3)
                        {
                            MessageBox.Show("Consistency time should be greater than 2 mins", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            return false;
                        }
                    }

                    Host mtHost = (Host)allServersForm.selectedMasterListbyUser._hostList[0];
                    foreach (DataStore d in mtHost.dataStoreList)
                    {
                        if (d.name == h.targetDataStore && d.vSpherehostname == mtHost.vSpherehost)
                        {
                            if (d.type == "vsan")
                            {
                                h.vSan = true;
                                MessageBox.Show("You have select vSAN datastore for: " + h.displayname + ". vSAN datastore is not supported." + Environment.NewLine
                                + " Please select other datastore", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                return false;
                            }
                            else
                            {
                                h.vSan = false;
                            }
                        }
                    }

                }
                foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                {
                    //h.isSourceNatted = Nat.IssourceIsnated;
                    //h.isTargetNatted = Nat.IstargetIsnated;

                    if (allServersForm.appNameSelected == AppName.Failback)
                    {
                        h.new_displayname = h.displayname;
                    }
                }
               
                allServersForm.finalMasterListPrepared = new HostList();
                for (int i = 0; i < allServersForm.SourceToMasterDataGridView.RowCount; i++)
                {


                    foreach (Host h1 in allServersForm.selectedMasterListbyUser._hostList)
                    {

                        //h1.datastore = allServersForm.SourceToMasterDataGridView.Rows[i].Cells[DATASTORE_COLUMN].Value.ToString();
                        //h1.retentionPath = allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTIONPATH_COLUMN].Value.ToString();
                        h1.role = Role.Secondary;
                        h1.machineType = "VirtualMachine";
                        allServersForm.finalMasterListPrepared.AddOrReplaceHost(h1);
                        // allServersForm._finalMasterList.Print();

                        // Assigning master target hostname,displayname and esxip to each source machine hosts....

                        foreach (Host tempHost in allServersForm.selectedSourceListByUser._hostList)
                        {
                            tempHost.masterTargetDisplayName = h1.displayname;
                            tempHost.masterTargetHostName = h1.hostname;
                            tempHost.targetESXIp = h1.esxIp;
                            tempHost.mt_uuid = h1.source_uuid;
                            if (allServersForm.appNameSelected != AppName.Failback)
                            {
                                tempHost.resourcePool = h1.resourcePool;
                            }
                            tempHost.targetvSphereHostName = h1.vSpherehost;

                            // tempHost.Print();
                        }
                    }

                }

                // Downloading the masterconfigfile from the cx check whether the same 
                // Vms are in protetion if so user has to select the same process server which is used for the previos protection
                // this check is added to support 1-n protection....
                MasterConfigFile master = new MasterConfigFile();
                master.DownloadMasterConfigFile(WinTools.GetcxIPWithPortNumber());
                if (File.Exists(_latestFilePath + "\\" + MASTER_CONFIG_FILE))
                {
                    Trace.WriteLine(DateTime.Now + " \t Came here to download the MasterConfigFile.xml");
                    FileInfo fInfo = new FileInfo(_latestFilePath + "\\"+MASTER_CONFIG_FILE);
                    long size = fInfo.Length;
                    if (size != 0)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Came hete because size of the MasterConfigFile is greater than zero");
                        HostList sourceList = new HostList();
                        HostList masterList = new HostList();
                        string esxIp = null;
                        string cxIp = null;
                        SolutionConfig configFile = new SolutionConfig();
                        configFile.ReadXmlConfigFileWithMasterList(MASTER_CONFIG_FILE,  sourceList,  masterList,  esxIp,  cxIp);
                        sourceList = SolutionConfig.GetHostList;
                        masterList = SolutionConfig.GetMasterList;
                        esxIp = configFile.GetEsxIP;
                        cxIp = configFile.GetcxIP;
                        if (sourceList._hostList.Count != 0)
                        {
                            foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                            {
                                foreach (Host h1 in sourceList._hostList)
                                {
                                    if (h.displayname == h1.displayname && h.targetESXIp != h1.targetESXIp && h1.failOver == "no")
                                    {
                                        Trace.WriteLine(DateTime.Now + " \t Came here because user is going to do the 1-n protection and he selected the different process servers ip");
                                        if (h.processServerIp != h1.processServerIp)
                                        {
                                            MessageBox.Show("You are trying to protect 1-n protection. You need to select same process server IP: " + h1.processServerIp, "Selction of Process server IP", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                            return false;
                                        }
                                    }
                                }
                            }
                        }

                    }
                    else
                    {
                        Trace.WriteLine(DateTime.Now + "\t Came here because MasterConfigFile.xml size is zero in vcon folder");
                    }
                }
                else
                {
                    Trace.WriteLine(DateTime.Now + "\t Came here because MasterConfigFile.xml doesnot exists in vcon folder");
                }
                

                SolutionConfig config = new SolutionConfig();
                foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                {
                    h.thinProvisioning = allServersForm.thinProvisionSelected;

                    if (h.cluster == "yes")
                    {
                        h.thinProvisioning = true;
                    }
                }


                if (allServersForm.appNameSelected == AppName.Adddisks)
                {
                    allServersForm.targetEsxInfoXml = AdditionOfDiskSelectMachinePanelHandler.targetEsxAdddisk;
                    Trace.WriteLine(DateTime.Now + "\t ********************************* " + AdditionOfDiskSelectMachinePanelHandler.targetEsxIPxml);
                    allServersForm.targetEsxInfoXml.ip = AdditionOfDiskSelectMachinePanelHandler.targetEsxIPxml;
                    allServersForm.targetEsxInfoXml.userName = AdditionOfDiskSelectMachinePanelHandler.targetUsernameXml;
                    allServersForm.targetEsxInfoXml.passWord = AdditionOfDiskSelectMachinePanelHandler.targetPasswordXml;
                    foreach (Host h1 in allServersForm.finalMasterListPrepared._hostList)
                    {
                        Trace.WriteLine("   entered to print the datastore values  " + DateTime.Now);
                        allServersForm.targetEsxInfoXml.dataStoreList = h1.dataStoreList;
                        foreach (DataStore d in h1.dataStoreList)
                        {
                            Trace.WriteLine(DateTime.Now + " \t Printing datastore values   " + d.name + d.freeSpace + d.totalSize);
                        }
                        //  h1.Print();
                    }
                    Host masterHost = (Host)allServersForm.finalMasterListPrepared._hostList[0];
                    allServersForm.targetEsxInfoXml.configurationList = masterHost.configurationList;
                    allServersForm.targetEsxInfoXml.networkList = masterHost.networkNameList;
                    allServersForm.targetEsxInfoXml.dataStoreList = masterHost.dataStoreList;
                    config.WriteXmlFile(allServersForm.selectedSourceListByUser, allServersForm.finalMasterListPrepared, allServersForm.targetEsxInfoXml, allServersForm.cxIPretrived, "ESX.xml", allServersForm.planInput, allServersForm.thinProvisionSelected);
                    if (allServersForm.appNameSelected == AppName.Adddisks)
                    {
                        //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
                        //based on above comment we have added xmlresolver as null
                        XmlDocument document = new XmlDocument();
                        document.XmlResolver = null;
                        string xmlfile = _latestFilePath + "\\ESX.xml";

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
                        XmlNodeList rootNodes = null;
                        rootNodes = document.GetElementsByTagName("root");

                        // writing the addition of disk values as tru in case of addition of disk......
                        foreach (XmlNode node in rootNodes)
                        {
                            node.Attributes["AdditionOfDisk"].Value = "true";
                        }
                        
                        document.Save(xmlfile);
                    }
                }
                else
                {
                    /*foreach (Host h in allServersForm._selectedSourceList._hostList)
                    {
                        MessageBox.Show(h.targetDataStore);
                    }*/
                    if (allServersForm.appNameSelected == AppName.Offlinesync)
                    {
                        foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                        {
                            h.offlineSync = true;
                        }
                    }
                    config.WriteXmlFile(allServersForm.selectedSourceListByUser, allServersForm.finalMasterListPrepared, allServersForm.targetEsxInfoXml, allServersForm.cxIPretrived, "ESX.xml", allServersForm.planInput, allServersForm.thinProvisionSelected);
                    //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
                    //based on above comment we have added xmlresolver as null
                    XmlDocument document = new XmlDocument();
                    document.XmlResolver = null;
                    string xmlfile = _latestFilePath + "\\ESX.xml";

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
                    //document.Load(xmlfile);
                    //reader.Close();
                    //XmlNodeList hostNodes = null;
                    XmlNodeList rootNodes = null, hostNodes = null;
                    rootNodes = document.GetElementsByTagName("root");
                    hostNodes = document.GetElementsByTagName("host");
                    bool isSourceNatted = false;
                    bool isTargetNatted = false;
                   /* foreach (XmlNode node in rootNodes)
                    {
                        node.Attributes["isSourceNatted"].Value = NAT._isSourceIsNated.ToString();
                        node.Attributes["isTargetNatted"].Value = NAT._isTargetIsNated.ToString();
                        node.Attributes["cx_portnumber"].Value = WinTools.GetCxPortNumber()();
                        if (NAT._cxNATIP != null)
                        {
                            node.Attributes["cx_nat_ip"].Value = NAT._cxNATIP;
                            node.Attributes["cx_ip"].Value = NAT._cxLocalIP;
                        }
                    }
                    foreach (XmlNode node in hostNodes)
                    {
                        node.Attributes["sourcenatted"].Value = NAT._isSourceIsNated.ToString();
                        node.Attributes["targetnatted"].Value = NAT._isTargetIsNated.ToString();
                    }*/
                    document.Save(xmlfile);

                }

                /*if (allServersForm._appName == AppName.BMR || (allServersForm._appName == AppName.OFFICELINESYNC && MainForm._selectedItemInComboBox == "P2v"))
                {
                    Host tempHost = new Host();
                    config.PreparePhysicalOsInfoFile(allServersForm._selectedSourceList, allServersForm._finalMasterList);
                }*/
                //config.PrePareProcessServerFile(allServersForm._selectedSourceList, allServersForm._cxIp);
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
            Trace.WriteLine(DateTime.Now + "\t Exiting: ProcessPanel() in ESX_SourceToMasterTargetPanelHandler  ");
            return true;
        }

        /*
         * In this method we will check for the cx is reachable or not.
         * And we will check if user has selected datastore for all the machines or not.
         * Not only datastroe but also reention/consistency policises also.
         * In case of p2v win2k3 32/64 user has to provide the driver file symmpi.sys fiel.
         * If not we will block here.
         */
        internal override bool ValidatePanel(AllServersForm allServersForm)
        {
            try
            {
                Trace.WriteLine(DateTime.Now + " \t Entered: ValidatePanel() in ESX_SourceToMasterTargetPanelHandler  ");
                //Checking whether the cx ip is reachable or not.....
               /* if (WinPreReqs.IpReachable(allServersForm.cxIpText.Text) == false)
                {
                    allServersForm.errorProvider.SetError(allServersForm.cxIpText, "CX ip is not reachable");
                    MessageBox.Show("CX IP is not reachable", "IP error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return false;
                }
                else
                {
                    allServersForm.errorProvider.Clear();
                }*/

                // Checking wether user has selected datastore or not if not we need to block user
                // to select the datastore.....
                foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                {
                    if (allServersForm.appNameSelected == AppName.Bmr || (allServersForm.appNameSelected == AppName.Adddisks && ResumeForm.selectedActionForPlan == "BMR Protection"))
                    {
                        if (h.targetDataStore == null)
                        {
                            foreach (Hashtable hash in h.disks.list)
                            {
                                if (hash["lun_name"] != null)
                                {

                                }
                                else
                                {
                                    MessageBox.Show("Please select datastore for machine " + h.displayname, "Datastore Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                    return false;
                                }

                            }
                        }
                    }
                    else
                    {
                        if (h.targetDataStore == null)
                        {
                            MessageBox.Show("Please select datastore for machine " + h.displayname, "Datastore Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            return false;
                        }
                        else
                        {
                            // MessageBox.Show(h.targetDataStore);
                        }
                    }
                }

                if (allServersForm.appNameSelected == AppName.Bmr || (allServersForm.appNameSelected == AppName.Offlinesync && ResumeForm.selectedActionForPlan == "P2v"))
                {

                    foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                    {
                        if (h.operatingSystem == "Windows_XP_32")
                        {
                            if (h.operatingSystem.Contains("XP"))
                            {
                                if (File.Exists(_installPath + "\\windows" + "\\" + h.operatingSystem + "\\symmpi.sys"))
                                {
                                    Trace.WriteLine(DateTime.Now + "\t Already symmpi.sys file is available in vContinuum box");
                                }
                                else
                                {
                                    if (CopyDriversFromOsCD(h.ip, allServersForm, h.operatingSystem, 0) == true)
                                    {
                                        Trace.WriteLine(DateTime.Now + "\t Got the driver file ");
                                    }
                                    else
                                    {
                                        MessageBox.Show("Failed to get driver file", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                        return false;
                                    }
                                }
                            }
                        }
                    }
                }


                // Checking for process server selection,retention size,consistency interval 
                // wherther user has selected or not.....
                for (int i = 0; i < allServersForm.SourceToMasterDataGridView.RowCount; i++)
                {
                    if (allServersForm.SourceToMasterDataGridView.Rows[i].Cells[PROCESS_SERVER_IP_COLUMN].Value == null)
                    {
                        MessageBox.Show("Please select process server IP address from the drop down list.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return false;
                    }
                    if (allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_SIZE_COLUMN].Value != null && allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_IN_DAYS_COLUMN].Value != null)
                    {
                        if (allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_SIZE_COLUMN].Value.ToString() == "Enter value" && allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_IN_DAYS_COLUMN].Value.ToString() == "Enter value")
                        {

                            MessageBox.Show("Please enter retention size or retention in terms of days.  ", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            return false;
                        }

                    }
                    else if (allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_SIZE_COLUMN].Value == null && allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_IN_DAYS_COLUMN].Value == null)
                    {
                        MessageBox.Show("Please enter retention size or retention in terms of days.  ", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return false;
                    }
                    if (allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_SIZE_COLUMN].Value != null && allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_IN_DAYS_COLUMN].Value == null)
                    {
                        if (allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_SIZE_COLUMN].Value.ToString() == "Enter value")
                        {
                            MessageBox.Show("Please enter retention size or retention in terms of days.  ", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            return false;
                        }

                    }
                    else if (allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_SIZE_COLUMN].Value == null && allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_IN_DAYS_COLUMN].Value != null)
                    {
                        if (allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_IN_DAYS_COLUMN].Value.ToString() == "Enter value")
                        {
                            MessageBox.Show("Please enter retention size or retention in terms of days.  ", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            return false;
                        }

                    }



                    if (allServersForm.SourceToMasterDataGridView.Rows[i].Cells[CONSISTENCY_TIME_COLUMN].Value != null)
                    {
                        if (allServersForm.SourceToMasterDataGridView.Rows[i].Cells[CONSISTENCY_TIME_COLUMN].Value.ToString() == "Enter value")
                        {
                            MessageBox.Show("Please enter consistency time in minutes.  ", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                            return false;

                        }
                    }
                    else
                    {
                        MessageBox.Show("Please enter consistency time in minutes.  ", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return false;

                    }
                }
                // In case of p2v windows 2003 servers user has to provide cd or path of the driver file ("Symmpi.sys").
                // If not we need to ask user to provide cd...
                //for (int i = 0; i < allServersForm.SourceToMasterDataGridView.RowCount; i++)
                //{

                //    if (allServersForm._appName == AppName.BMR && allServersForm._osType == OStype.Windows)
                //    {
                //        foreach (Host h in allServersForm._selectedSourceList._hostList)
                //        {
                //            Host tempH = new Host();
                //            tempH.ip = allServersForm.SourceToMasterDataGridView.Rows[i].Cells[SOURCE_SERVER_IP].Value.ToString();
                //            if (tempH.ip == h.ip)
                //            {
                //                if (allServersForm.SourceToMasterDataGridView.Rows[i].Cells[COPYDRIVERS_COLUMN].Value.ToString() != "Available")
                //                {
                //                    MessageBox.Show("Please Provide " + h.operatingSystem + " CD to copy drivers", "Error ", MessageBoxButtons.OK, MessageBoxIcon.Error);
                //                    return false;

                //                }
                //            }

                //        }
                //    }
                //}


                //For each disk check that the disksize doesn't exceed the maximum file size of a datastore.
                // Maximum size of a file depends upon the blocksize used in creating of datastore
                // Multiply blocksize of datastore with 256000 to get the max size.
                // with Blocksize of  1MB max size of file can be  256GB
                foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                {
                    foreach (Host h1 in allServersForm.selectedMasterListbyUser._hostList)
                    {
                        ArrayList physicalDisks = new ArrayList();

                        physicalDisks = h.disks.GetDiskList;
                        foreach (Hashtable disk in physicalDisks)
                        {
                            if (allServersForm.appNameSelected == AppName.Bmr || (allServersForm.appNameSelected == AppName.Offlinesync && ResumeForm.selectedActionForPlan == "P2v") || (allServersForm.appNameSelected == AppName.Adddisks && ResumeForm.selectedActionForPlan == "BMR Protection"))
                            {
                                Trace.WriteLine("Came to check size of disk with block for p2v p2v");
                                if (disk["Selected"].ToString() == "Yes")
                                {
                                    float size = float.Parse(disk["Size"].ToString());

                                    //Convert the size to MBs
                                    size = size / 1024;
                                    foreach (DataStore d in h1.dataStoreList)
                                    {
                                        if (h.targetDataStore == d.name && h1.vSpherehost == d.vSpherehostname)
                                        {
                                            if (d.filesystemversion < 5)
                                            {

                                                if (d.blockSize != 0)
                                                {
                                                    if (d.GetMaxDataFileSizeMB < (long)size)
                                                    {
                                                        MessageBox.Show("Maximum allowed VMDK size on datastroe ( " + h.targetDataStore + " ) is " + d.GetMaxDataFileSizeMB/ 1024 + "(GB). Please select another datastore for the VM " + h.displayname, " Datastore limit exceeded", MessageBoxButtons.OK, MessageBoxIcon.Stop);
                                                        return false;
                                                    }
                                                }
                                                else
                                                {
                                                    // return false;
                                                }
                                            }
                                        }

                                    }
                                }

                            }
                            else
                            {
                                Trace.WriteLine("Came to check size of disk with block size other than p2v");
                                if (disk["Selected"].ToString() == "Yes")
                                {
                                    if (disk["Rdm"].ToString() == "yes" && h.convertRdmpDiskToVmdk == false)
                                    {    //Ignoring if a disk is RDM and RDM-->RDM replication is selected.
                                        // break;

                                    }
                                    else
                                    {//Checking the size of the disk for normal vmdk and RDM-->vmdk replication is selected.
                                        float size = float.Parse(disk["Size"].ToString());

                                        //Convert the size to MBs
                                        size = size / 1024;

                                        foreach (DataStore d in h1.dataStoreList)
                                        {

                                            if (h.targetDataStore == d.name && h1.vSpherehost == d.vSpherehostname)
                                            {
                                                if (d.filesystemversion < 5)
                                                {
                                                    if (d.blockSize != 0)
                                                    {
                                                        if (d.GetMaxDataFileSizeMB < (long)size)
                                                        {
                                                            MessageBox.Show("Maximum allowed VMDK size on datastroe ( " + h.targetDataStore + " ) is " + d.GetMaxDataFileSizeMB / 1024 + "(GB). Please select another datastore for the VM " + h.displayname, " Datastore limit exceeded", MessageBoxButtons.OK, MessageBoxIcon.Stop);
                                                            return false;
                                                        }
                                                    }
                                                    else
                                                    {
                                                        // return false;
                                                    }
                                                }
                                            }

                                        }


                                    }
                                }

                            }
                        }
                    }
                }
                // In case of rdm disk persent in the soure vms and user want to keep those disks as rdm only we need
                // to ask user to select rdm luns for the particular rdms disks.....
                //if (allServersForm.ignoreDatastoreSizeCalculationCheckBox.Checked == false)
                {
                    if (allServersForm.appNameSelected == AppName.Esx || (allServersForm.appNameSelected == AppName.Adddisks && ResumeForm.selectedActionForPlan == "ESX"))
                    {
                        for (int i = 0; i < allServersForm.SourceToMasterDataGridView.RowCount; i++)
                        {
                            if (allServersForm.SourceToMasterDataGridView.Rows[i].Cells[SELECT_LUNS_COLUMN].Value.ToString() == "Select luns")
                            {
                                MessageBox.Show("Please Select luns for the rdm disks", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                return false;
                            }
                        }
                    }
                }


                Trace.WriteLine(DateTime.Now + " \t Exiting: ValidatePanel() in ESX_SourceToMasterTargetPanelHandler  ");
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


        internal override bool CanGoToNextPanel(AllServersForm allServersForm)
        {
            allServersForm.helpContentLabel.Text = HelpForcx.ProtectScreen3;
            allServersForm.mandatoryLabel.Visible = false;
            allServersForm.mandatorydoubleFieldLabel.Visible = false;
            Trace.WriteLine(DateTime.Now + " \t Entered: CanGoToNextPanel() in ESX_SourceToMasterTargetPanelHandler  " );
            return true;

        }

        /*
         * In case of linux we shouldn't show the push agent scren while moving back and forth.
         * So here we will decrese index and taskListIndex.
         * In case of addition of disk we will add the disksize to the selected datastroe because when user wants 
         * to change his decision and don't want to protect particular disk. So we will add to datastro when we come to 
         * this screen again.
         */
        internal override bool CanGoToPreviousPanel(AllServersForm allServersForm)
        {
            try
            {
                Trace.WriteLine(DateTime.Now + " \t Entered: CanGoToPreviousPanel() in ESX_SourceToMasterTargetPanelHandler  ");
                allServersForm.helpContentLabel.Text = HelpForcx.ProtectScreen3;
                allServersForm.mandatorydoubleFieldLabel.Visible = false;
                if(allServersForm.appNameSelected == AppName.Adddisks)
                {
                    allServersForm.mandatoryLabel.Visible = false;
                    
                }
                //We will skip push agent panel handler screen for Linux
                // By reducing the index of the panel and the side _tasklistIndex.....
                //if (allServersForm._osType != OStype.Windows && allServersForm._appName != AppName.ADDDISKS )
                //{
                //    Trace.WriteLine(DateTime.Now + "\t Came here because this is linux protection ");
                //    ((System.Windows.Forms.PictureBox)allServersForm._pictureBoxList[allServersForm._taskListIndex]).Visible = false;
                //    ((System.Windows.Forms.PictureBox)allServersForm._pictureBoxList[allServersForm._taskListIndex - 1]).Visible = false;
                //    allServersForm._index--;

                //    allServersForm._taskListIndex--;

                //}

                //When we go back to previous panel, sourcetoMasterDataGridView is generating lost focus event
                // causing exceptions. By making selected = false, we will disable all event handlers associated with 
                // that data grid view. 
                for (int i = 0; i < allServersForm.SourceToMasterDataGridView.RowCount; i++)
                {

                    allServersForm.SourceToMasterDataGridView.Rows[i].Selected = false;


                }

                // While moving back we are going to add the disksize to the corresponding datastore because 
                // While coming to this screen we will again add the size
                // To avoid the the datastore calculation issue we had added this.....
                if (allServersForm.appNameSelected == AppName.Adddisks)
                {
                    Host h1 = (Host)allServersForm.selectedMasterListbyUser._hostList[0];
                    foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                    {
                        foreach (DataStore d in h1.dataStoreList)
                        {
                            if (h.targetDataStore == d.name)
                            {
                                d.freeSpace = d.freeSpace + h.totalSizeOfDisks;
                            }

                        }
                    }
                    // we are checking for the count of the se;lected vms list in the first screen
                    // this will show us whether we can make previous button is visible or not....
                    if (ResumeForm.selectedVMListForPlan._hostList.Count != 0)
                    {
                        allServersForm.previousButton.Visible = false;
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
            Trace.WriteLine(DateTime.Now + " \t Exiting: CanGoToPreviousPanel() in ESX_SourceToMasterTargetPanelHandler  ");
            return true;

        }

        /*
         * In case of P2V win2k3 32/64. User has to provide cd location or os location if mounted.
         * From that path we will checkthe readme.htm and then tittle form tittle we will get the 
         * eaxct os given by the user.If os given by user is matched.
         * then we will search forthe symmpi.sys file and will copy that file to the windnows\osname\symmipi.sys file 
         * when user clicks protect we willc opy allthe file s from windows folder to planname folder which is in failovee\data folder.
         */
        internal bool CopyDriversFromOsCD(string ip, AllServersForm allServersForm, string inOperatingSystem, int index)
        {
            try
            {
                // When user selects the linklabel in the particular cd driver column 
                // we will show browserdialog to select path of the cd or os folder...
                // From that we will get the symmpi.sys file and make a folder for the particular folder in the windows folder....

                Trace.WriteLine(DateTime.Now + " \t Entered: CopyDriversFromOsCD() in ESX_SourceToMasterPanelHandler ");
                Host tempHost = new Host();
                tempHost.ip = ip;

                int listIndex = 0;
                allServersForm.selectedSourceListByUser.DoesHostExist(tempHost, ref listIndex);

                //  string operatingSystem = ((Host)allServersForm._selectedSourceList._hostList[listIndex]).operatingSystem;


               
                _cellIndex = index;

                string selectedPath = "";
                allServersForm.folderBrowserDialog.Description = "Please select folder/drive containg OS cd of " + inOperatingSystem;
                if (inOperatingSystem == "Windows_XP_32")
                {
                    MessageBoxForm message = new MessageBoxForm();
                    message.ShowDialog();
                    if (message.canceled == true)
                    {
                        return false;
                    }
                    else
                    {
                        allServersForm.openFileDialog.ShowDialog();

                        if (allServersForm.openFileDialog.FileName.Length > 0)
                        {



                            selectedPath = allServersForm.openFileDialog.FileName;
                            if (selectedPath.ToLower().Contains("symmpi.sys"))
                            {
                                if (!Directory.Exists(_installPath + "\\windows\\Windows_XP_32"))
                                {
                                    Directory.CreateDirectory(_installPath + "\\windows" + "\\" + "Windows_XP_32");
                                }
                                File.Copy(selectedPath, _installPath + "\\windows\\Windows_XP_32\\symmpi.sys",true);
                                //allServersForm.SourceToMasterDataGridView.Rows[index].Cells[Esx_SourceToMasterPanelHandler.COPYDRIVERS_COLUMN].Value = "Available";
                                return true;
                            }
                        }
                        else
                        {
                            return false;
                        }

                    }
                }
                else
                {
                    string messages = "Please make sure that you point to right Windows CD or extracted directory(from iso). " + Environment.NewLine
                                   + "InMage doesn't validate service pack levels of CD/extracted ISO. " + Environment.NewLine
                                   + "Use same versions/service pack level that of source physical server.";
                    MessageBox.Show(messages, "Information", MessageBoxButtons.OK, MessageBoxIcon.Information);
                    DialogResult dlgResult = allServersForm.folderBrowserDialog.ShowDialog();
                    if (dlgResult == DialogResult.Cancel)
                    {
                        return false;

                    }
                    if (allServersForm.folderBrowserDialog.SelectedPath.Length <= 0)
                    {
                        return false;
                    }
                    else
                    {
                        selectedPath = allServersForm.folderBrowserDialog.SelectedPath.ToString();
                    }
                    allServersForm.Refresh();

                    _pathForDriver = selectedPath;
                    _operatingSystem = inOperatingSystem;
                    _driverIndex = listIndex;
                    allServersForm.progressBar.Visible = true;
                    allServersForm.nextButton.Enabled = false;
                    allServersForm.copyDriversBackgroundWorker.RunWorkerAsync();
                }
                Trace.WriteLine(DateTime.Now + " \t Exiting: CopyDriversFromOsCD() in ESX_SourceToMasterPanelHandler ");
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
            return false;
        }
        /*
         * After user seelcted the os path then everything will be done under the backgroundworker process.
         */
        internal bool CopyDriversUsingBackGroundWorker(AllServersForm allServersForm)
        {
            try
            {
                //  copyDriversBackgroundWorker_dowork event will call this method...
                //Here it will search the symmpi.sys file in the selected path and copied to the windows folder
                // With respective os folder.....
                ((Host)allServersForm.selectedSourceListByUser._hostList[_driverIndex]).CopyDriversFromOScd(_pathForDriver, _operatingSystem);

                if (File.Exists(_installPath + "\\windows" + "\\" + _operatingSystem + "\\symmpi.sys"))
                {
                    _returnCodeOfDrivers = true;
                    return true;
                }
                else
                {
                    _returnCodeOfDrivers = false;
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
         * For that backgroundworker we will have the return cdoe here and make the column value as available.
         */
        internal bool FindDriverFileReturnCode(AllServersForm allServersForm)
        {
            try
            {
                // copyDriversBackgroundWorker_Completed will call this once the file copy is done.....
                // Here we will check for the returncode whetehr the file is copied or not....
                if (_returnCodeOfDrivers == true)
                {
                   // allServersForm.SourceToMasterDataGridView.Rows[_cellIndex].Cells[Esx_SourceToMasterPanelHandler.COPYDRIVERS_COLUMN].Value = "Available";
                }
                allServersForm.nextButton.Enabled = true;
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
         * In caseof offline sync user has to select one datastore for all the machines.
         * if user unselects for one machien also we will unselet for all themachines.
         */
        internal bool DataSroteSelectionOrUnselectionForOfficeLineSync(AllServersForm allServersForm, int index)
        {
            try
            {
                // This will be called while the selection of datastore in case of offline sync export
                // For offlinesync export if user elects one datastore we will select the same datasotre for all vms
                // This is the limitation for the offlinesync export....
                Trace.WriteLine(DateTime.Now + " \t Entered: DataSroteSelectionOrUnselectionForOfficeLineSync() in ESX_SourceToMasterTargetPanelHandler");

                if (allServersForm.SourceToMasterDataGridView.Rows[index].Cells[SELECT_DATASTORE_COLUMN].Value.ToString() != null)
                {

                    SelectDataStoreForOffLineSync(allServersForm, index);

                }
                else
                {
                    UnselectDataStoreForOffLineSync(allServersForm, index);
                }
                Trace.WriteLine(DateTime.Now + " \t Exiting: DataSroteSelectionOrUnselectionForOfficeLineSync() in ESX_SourceToMasterTargetPanelHandler");
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
         * When ever user selects the datastore column that event will call this method.
         * This method will check whether is is p2v/v2v and already having datastore selected or not.
         * 
         */
        internal bool DataStoreChanges(AllServersForm allServersForm)
        {
            Trace.WriteLine(DateTime.Now + "\t Entered: DataStoreChanges() in ESX_SourceToMasterTargetPanelHandler");
            try
            {
                // When user selects the datastore this method will be called first 
                // We will get displayname or ip  for the selected machine......

                Host host = new Host();
                if (allServersForm.appNameSelected == AppName.Bmr && (allServersForm.appNameSelected == AppName.Adddisks && ResumeForm.selectedActionForPlan == "BMR Protection"))
                {
                    if (allServersForm.listIndexPrepared == 0)
                    {
                        host.ip = allServersForm.SourceToMasterDataGridView.Rows[0].Cells[Esx_SourceToMasterPanelHandler.SOURCE_SERVER_IP].Value.ToString();
                    }
                    else
                    {
                        host.ip = allServersForm.dataStoreCaptureDisplayNamePrepared;
                    }
                }
                if (allServersForm.appNameSelected == AppName.Esx || allServersForm.appNameSelected == AppName.Failback || (allServersForm.appNameSelected == AppName.Adddisks && ResumeForm.selectedActionForPlan == "ESX"))
                {
                    if (allServersForm.listIndexPrepared == 0)
                    {
                        host.displayname = allServersForm.SourceToMasterDataGridView.Rows[0].Cells[Esx_SourceToMasterPanelHandler.SOURCE_SERVER_IP].Value.ToString();
                    }
                    else
                    {
                        host.displayname = allServersForm.dataStoreCaptureDisplayNamePrepared;
                    }
                }

                if (allServersForm.appNameSelected == AppName.Offlinesync)
                {
                    if (ResumeForm.selectedActionForPlan == "Esx")
                    {


                        if (allServersForm.listIndexPrepared == 0)
                        {

                            host.displayname = allServersForm.SourceToMasterDataGridView.Rows[0].Cells[Esx_SourceToMasterPanelHandler.SOURCE_SERVER_IP].Value.ToString();
                        }
                        else
                        {
                            host.displayname = allServersForm.dataStoreCaptureDisplayNamePrepared;
                        }
                    }
                    else
                    {
                        if (allServersForm.listIndexPrepared == 0)
                        {
                            host.ip = allServersForm.SourceToMasterDataGridView.Rows[0].Cells[Esx_SourceToMasterPanelHandler.SOURCE_SERVER_IP].Value.ToString();
                        }
                        else
                        {
                            host.ip = allServersForm.dataStoreCaptureDisplayNamePrepared;
                        }

                    }
                }
                allServersForm.selectedSourceListByUser.DoesHostExist(host, ref allServersForm.listIndexPrepared);


                if (allServersForm.appNameSelected == AppName.Bmr)
                {

                    foreach (Host tempHost in allServersForm.selectedSourceListByUser._hostList)
                    {
                        if (tempHost.ip == host.ip)
                        {
                            /* if (ValidateValues(allServersForm) == true)
                             {
                                 AssignValuesToHost(allServersForm, tempHost);
                             }*/



                        }
                    }
                }
                if (allServersForm.appNameSelected == AppName.Esx || allServersForm.appNameSelected == AppName.Adddisks || allServersForm.appNameSelected == AppName.Failback)
                {

                    foreach (Host tempHost in allServersForm.selectedSourceListByUser._hostList)
                    {
                        if (tempHost.displayname == host.displayname)
                        {

                            /*if (ValidateValues(allServersForm) == true)
                            {
                                AssignValuesToHost(allServersForm, tempHost);
                            }*/



                        }
                    }
                }
                if (allServersForm.appNameSelected == AppName.Offlinesync)
                {
                    foreach (Host tempHost in allServersForm.selectedSourceListByUser._hostList)
                    {
                        if (ResumeForm.selectedActionForPlan == "Esx")
                        {

                            if (tempHost.displayname == host.displayname)
                            {

                                /* if (ValidateValues(allServersForm) == true)
                                 {
                                     AssignValuesToHost(allServersForm, tempHost);
                                 }*/



                            }
                        }
                        else
                        {
                            if (tempHost.ip == host.ip)
                            {
                                /* if (ValidateValues(allServersForm) == true)
                                 {
                                     AssignValuesToHost(allServersForm, tempHost);
                                 }*/
                            }
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
            Trace.WriteLine(DateTime.Now + "\t Exiting: DataStoreChanges() in ESX_SourceToMasterTargetPanelHandler");
            
            return true;
        }

        /*
         *This method will be called after all the error checks are done for the pariticular machines...
         *Error checks are like is there enough space in the particular datastore or not and 
         *Did they select any datastore previously.....
         *We will remove that space from the datastore free space.....
         */

        internal bool SelectedDataStore(AllServersForm allServersForm, Host h, int index)
        {
            try
            {
                // This method will be called after all the error checks are done for the pariticular machines...
                // Error checks are like is there enough space in the particular datastore or not and 
                // Did they select any datastore previously.....
                // We will remove that space from the datastore free space.....

                Trace.WriteLine(DateTime.Now + "\t Entered: SelectedDataStore() in ESX_SourceToMasterTargetPanelHandler");
                float a = Convert.ToInt32(allServersForm.dataStoreDataGridView.Rows[index].Cells[DATASTORE_FREESPACE_COLUMN].Value);
                Host h1 = (Host)allServersForm.selectedMasterListbyUser._hostList[0];

                foreach (DataStore d in h1.dataStoreList)
                {
                    if (d.name == allServersForm.dataStoreDataGridView.Rows[index].Cells[DATASTORE_NAME_COLUMN].Value.ToString())
                    {
                        d.freeSpace = a - h.totalSizeOfDisks;

                        if (d.type == "vsan")
                        {
                            h.vSan = true;
                        }
                        else
                        {
                            h.vSan = false;
                        }

                    }
                }


                Debug.WriteLine("entered to calculate the datastore sizes " + h.totalSizeOfDisks + a + "   " + (a - h.totalSizeOfDisks));

                allServersForm.dataStoreDataGridView.Rows[index].Cells[DATASTORE_FREESPACE_COLUMN].Value = a - h.totalSizeOfDisks;
                allServersForm.dataStoreDataGridView.Rows[index].Cells[DATASTORE_USEDSPACE_COLUMN].Value = h.totalSizeOfDisks + Convert.ToInt32(allServersForm.dataStoreDataGridView.Rows[index].Cells[DATASTORE_USEDSPACE_COLUMN].Value);
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
            Trace.WriteLine(DateTime.Now + "\t Exiting: SelectedDataStore() in ESX_SourceToMasterTargetPanelHandler");
            return true;
        }

    
        /*
         * Before assign datastroe selected to particular machine. we will check for the whether that machine is having the
         * datstore. If so we will call this method to assgin total isze to the previous seelcted datastore.
         */
        internal bool UnSelectDataStoreCalCulation(AllServersForm allServersForm, Host h, int index)
        {

            // Unslect datastore meand when user trying ot select another datastore we check whether it is already having datastore selected 
            // If so we will add this space to the previously selected datastore available space.....
            Trace.WriteLine(DateTime.Now + "\t Entered: UnSelectDataStoreCalCulation() in ESX_SourceToMasterTargetPanelHandler");
            try
            {
                if (allServersForm.ignoreDatastoreSizeCalculationCheckBox.Checked == false)
                {
                    float a = Convert.ToInt32(allServersForm.dataStoreDataGridView.Rows[index].Cells[DATASTORE_FREESPACE_COLUMN].Value);
                    Host h1 = (Host)allServersForm.selectedMasterListbyUser._hostList[0];
                    foreach (DataStore d in h1.dataStoreList)
                    {
                        if (d.name == allServersForm.dataStoreDataGridView.Rows[index].Cells[DATASTORE_NAME_COLUMN].Value.ToString())
                        {
                            d.freeSpace = a + h.totalSizeOfDisks;

                        }
                    }

                    Debug.WriteLine("entered to calculate the datastore sizes " + h.totalSizeOfDisks + a + "   " + (a - h.totalSizeOfDisks));

                    allServersForm.dataStoreDataGridView.Rows[index].Cells[DATASTORE_FREESPACE_COLUMN].Value = a + h.totalSizeOfDisks;
                    allServersForm.dataStoreDataGridView.Rows[index].Cells[DATASTORE_USEDSPACE_COLUMN].Value = Convert.ToInt32(allServersForm.dataStoreDataGridView.Rows[index].Cells[DATASTORE_USEDSPACE_COLUMN].Value) - h.totalSizeOfDisks;
                }
                Trace.WriteLine(DateTime.Now + "\t Exiting: UnSelectDataStoreCalCulation() in ESX_SourceToMasterTargetPanelHandler");


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
         * In case offline sync user has to select same datastroe for all the machines.
         * If space is not enugh in selected datstore. we will ask user to select new datastroe for this protetion.
         */
        internal bool SelectDataStoreForOffLineSync(AllServersForm allServersForm, int index)
        {
            try
            {
                Trace.WriteLine(DateTime.Now + "\t Entered: SelectDataStoreForOffLineSync() in ESX_SourceToMasterTargetPanelHandler");
                // In case of offline sync all the machines should have same datastore..
                // we will select defaultly when user selects the first datastore
                // If user is trying to changes also we will change for all the vms....

                for (int i = 0; i < allServersForm.dataStoreDataGridView.RowCount; i++)
                {
                    foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                    {
                        if (allServersForm.dataStoreDataGridView.Rows[i].Cells[DATASTORE_NAME_COLUMN].Value.ToString() == allServersForm.SourceToMasterDataGridView.Rows[index].Cells[SELECT_DATASTORE_COLUMN].Value.ToString())
                        {



                            if (h.targetDataStore != null)
                            {
                                UnSelectPreviousDataStore(allServersForm);
                            }
                            int a = Convert.ToInt32(allServersForm.dataStoreDataGridView.Rows[i].Cells[DATASTORE_FREESPACE_COLUMN].Value);
                            Debug.WriteLine("entered to calculate the datastore sizes " + h.totalSizeOfDisks + a + "   " + (a - h.totalSizeOfDisks));
                            //MessageBox.Show("Printing a "+a.ToString() + "usedspace"+ allServersForm.dataStoreDataGridView.Rows[i].Cells[DATASTORE_USEDSPACE_COLUMN].Value.ToString());
                            if (a - h.totalSizeOfDisks >= 0)
                            {
                                allServersForm.dataStoreDataGridView.Rows[i].Cells[DATASTORE_FREESPACE_COLUMN].Value = a - h.totalSizeOfDisks;

                                // MessageBox.Show("value is "+ h.totalSizeOfDisks + Convert.ToInt32(allServersForm.dataStoreDataGridView.Rows[i].Cells[DATASTORE_USEDSPACE_COLUMN].Value));
                                allServersForm.dataStoreDataGridView.Rows[i].Cells[DATASTORE_USEDSPACE_COLUMN].Value = h.totalSizeOfDisks + Convert.ToInt32(allServersForm.dataStoreDataGridView.Rows[i].Cells[DATASTORE_USEDSPACE_COLUMN].Value);
                                allServersForm.dataStoreDataGridView.RefreshEdit();
                                h.targetDataStore = allServersForm.SourceToMasterDataGridView.Rows[index].Cells[SELECT_DATASTORE_COLUMN].Value.ToString();
                            }
                            else
                            {
                                UnselectDataStoreForOffLineSync(allServersForm, i);
                                for (int j = 0; j < allServersForm.SourceToMasterDataGridView.RowCount; j++)
                                {
                                    allServersForm.SourceToMasterDataGridView.Rows[j].Cells[SELECT_DATASTORE_COLUMN].Value = null;
                                }
                                MessageBox.Show("The selected datastore does not have enough space to export machine(s)", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);

                                allServersForm.SourceToMasterDataGridView.RefreshEdit();
                                return false;
                            }
                        }
                    }
                }

                Trace.WriteLine(DateTime.Now + "\t Exiting: SelectDataStoreForOffLineSync() in ESX_SourceToMasterTargetPanelHandler");
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
         * In case of machien is having datastore already seelcted we will assign back the freespace to the previous datastore.
         */
        internal bool UnSelectPreviousDataStore(AllServersForm allServersForm)
        {
            Trace.WriteLine(DateTime.Now + "\t Entered: UnSelectPreviousDataStore() in ESX_SourceToMasterTargetPanelHandler");
            // If user is trying to select another datastore first we will uselect the previous datastore
            // That means we need to add the tatal machine size to the free space of the previous datastore....
            try
            {
                foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                {
                    for (int i = 0; i < allServersForm.dataStoreDataGridView.RowCount; i++)
                    {
                        if (h.targetDataStore == allServersForm.dataStoreDataGridView.Rows[i].Cells[DATASTORE_NAME_COLUMN].Value.ToString())
                        {

                            h.targetDataStore = null;
                            int a = Convert.ToInt32(allServersForm.dataStoreDataGridView.Rows[i].Cells[DATASTORE_FREESPACE_COLUMN].Value);
                            Debug.WriteLine("entered to calculate the datastore sizes " + h.totalSizeOfDisks + a + "   " + (a + h.totalSizeOfDisks));
                            allServersForm.dataStoreDataGridView.Rows[i].Cells[DATASTORE_FREESPACE_COLUMN].Value = a + h.totalSizeOfDisks;
                            allServersForm.dataStoreDataGridView.Rows[i].Cells[DATASTORE_USEDSPACE_COLUMN].Value = Convert.ToInt32(allServersForm.dataStoreDataGridView.Rows[i].Cells[DATASTORE_USEDSPACE_COLUMN].Value) - h.totalSizeOfDisks;
                            allServersForm.dataStoreDataGridView.RefreshEdit();

                        }
                    }
                }

                Trace.WriteLine(DateTime.Now + "\t Exiting: UnSelectPreviousDataStore() in ESX_SourceToMasterTargetPanelHandler");
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
         * For offline sync protection also we user has already seelcted one datastroe and want to change the datastroe.
         * We will assign back freespace to the previus datastroe freespace of all the machines.
         */
        internal bool UnselectDataStoreForOffLineSync(AllServersForm allServersForm, int index)
        {
            //In case of offline sync export we need to unselect for all machines so that we had choosen seperate method for this.......
            Trace.WriteLine(DateTime.Now + "\t Entered: UnselectDataStoreForOffLineSync() in ESX_SourceToMasterTargetPanelHandler");
            try
            {

                foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                {
                    if (h.targetDataStore != null)
                    {
                        h.targetDataStore = null;
                        int a = Convert.ToInt32(allServersForm.dataStoreDataGridView.Rows[index].Cells[DATASTORE_FREESPACE_COLUMN].Value);
                        Debug.WriteLine("entered to calculate the datastore sizes " + h.totalSizeOfDisks + a + "   " + (a + h.totalSizeOfDisks));
                        allServersForm.dataStoreDataGridView.Rows[index].Cells[DATASTORE_FREESPACE_COLUMN].Value = a + h.totalSizeOfDisks;
                        allServersForm.dataStoreDataGridView.Rows[index].Cells[DATASTORE_USEDSPACE_COLUMN].Value = Convert.ToInt32(allServersForm.dataStoreDataGridView.Rows[index].Cells[DATASTORE_USEDSPACE_COLUMN].Value) - h.totalSizeOfDisks;
                        allServersForm.dataStoreDataGridView.RefreshEdit();
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
            Trace.WriteLine(DateTime.Now + "\t Exiting: UnselectDataStoreForOffLineSync() in ESX_SourceToMasterTargetPanelHandler");

            return true;
        }

        /*
         * In case of p2v we need to check machien based on the ip.
         * 
         */
        internal bool BareMetalDataStoreSelection(AllServersForm allServersForm, Host h, int index)
        {

            // In case of P2V we will select datastore using the ip for esx we will select by displayname
            // In case of p2v user will give us ip for esx we will get details from the esx level 

            Trace.WriteLine(DateTime.Now + "\t Entered: BareMetalDataStoreSelection() in ESX_SourceToMasterTargetPanelHandler");
            try
            {

                Host host = new Host();
                if (allServersForm.appNameSelected == AppName.Bmr)
                {

                    if (allServersForm.listIndexPrepared == 0)
                    {
                        host.ip = allServersForm.SourceToMasterDataGridView.Rows[0].Cells[Esx_SourceToMasterPanelHandler.SOURCE_SERVER_IP].Value.ToString();
                    }
                    else
                    {
                        host.ip = allServersForm.dataStoreCaptureDisplayNamePrepared;
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

            Trace.WriteLine(DateTime.Now + "\t Exiting: BareMetalDataStoreSelection() in ESX_SourceToMasterTargetPanelHandler");


            return true;
        }
        internal bool IncrementalDiskDataStoreSelection(AllServersForm allServersForm, Host h, int index)
        {
            // For Addition of disk we had to select the previous datastore at the time of the protection
            // If enough space is not there in that  datastore we need to allow user to select datastore
            // If not datastore column is readonly mode....
            Trace.WriteLine(DateTime.Now + "\t Entered: IncrementalDiskDataStoreSelection() in ESX_SourceToMasterTargetPanelHandler");
            try
            {

                Host host = new Host();
                if (allServersForm.appNameSelected == AppName.Adddisks)
                {
                    if (allServersForm.listIndexPrepared == 0)
                    {
                        host.displayname = allServersForm.SourceToMasterDataGridView.Rows[0].Cells[Esx_SourceToMasterPanelHandler.SOURCE_SERVER_IP].Value.ToString();
                    }
                    else
                    {
                        host.displayname = allServersForm.dataStoreCaptureDisplayNamePrepared;
                    }

                }

                if (allServersForm.SourceToMasterDataGridView.Rows[index].Cells[Esx_SourceToMasterPanelHandler.SELECT_DATASTORE_COLUMN].Value != null)
                {
                    if (h.targetDataStore == null)
                    {
                        if (host.displayname == h.displayname)
                        {
                            if (allServersForm.SourceToMasterDataGridView.Rows[index].Cells[SELECT_DATASTORE_COLUMN].Value != null)
                            {
                                for (int i = 0; i < allServersForm.dataStoreDataGridView.RowCount; i++)
                                {
                                    if (allServersForm.dataStoreDataGridView.Rows[i].Cells[DATASTORE_NAME_COLUMN].Value.ToString() == allServersForm.SourceToMasterDataGridView.Rows[index].Cells[SELECT_DATASTORE_COLUMN].Value.ToString())
                                    {
                                        if (allServersForm.ignoreDatastoreSizeCalculationCheckBox.Checked == false)
                                        {
                                            if (Convert.ToInt32(allServersForm.dataStoreDataGridView.Rows[i].Cells[DATASTORE_FREESPACE_COLUMN].Value) > h.totalSizeOfDisks)
                                            {
                                                Debug.WriteLine("Printing total size of the disk " + h.targetDataStore + h.totalSizeOfDisks + " " + allServersForm.dataStoreDataGridView.Rows[i].Cells[DATASTORE_FREESPACE_COLUMN].Value.ToString());

                                                //h.datastore = allServersForm.dataStoreDataGridView.Rows[rowIndex].Cells[DATASTORE_NAME_COLUMN].Value.ToString();

                                                h.targetDataStore = allServersForm.SourceToMasterDataGridView.Rows[index].Cells[SELECT_DATASTORE_COLUMN].Value.ToString();
                                                SelectedDataStore(allServersForm, h, i);

                                                return true;
                                            }
                                            else
                                            {
                                                allServersForm.SourceToMasterDataGridView.Rows[index].Cells[SELECT_DATASTORE_COLUMN].Value = null;
                                                // allServersForm.dataStoreDataGridView.Rows[index].Cells[DATASOURCE_SELECTED_COLUMN].Value = false;
                                                allServersForm.SourceToMasterDataGridView.RefreshEdit();
                                                MessageBox.Show("Selected datastore does not have enough space ", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);

                                                return false;
                                            }
                                        }
                                        else
                                        {
                                            Trace.WriteLine(DateTime.Now + "\t User has selected to ignore the datastroe calculation");
                                            h.targetDataStore = allServersForm.SourceToMasterDataGridView.Rows[index].Cells[SELECT_DATASTORE_COLUMN].Value.ToString();
                                            return true;
                                        }
                                    }
                                }

                            }

                        }
                    }
                    else
                    {
                        if (h.targetDataStore != allServersForm.SourceToMasterDataGridView.Rows[index].Cells[SELECT_DATASTORE_COLUMN].Value.ToString())
                        {
                            for (int i = 0; i < allServersForm.dataStoreDataGridView.RowCount; i++)
                            {
                                if (h.targetDataStore == allServersForm.dataStoreDataGridView.Rows[i].Cells[DATASTORE_NAME_COLUMN].Value.ToString())
                                {
                                    UnSelectDataStoreCalCulation(allServersForm, h, i);
                                    h.targetDataStore = null;
                                }

                            }



                        }
                        if (host.displayname == h.displayname)
                        {
                            if (allServersForm.SourceToMasterDataGridView.Rows[index].Cells[SELECT_DATASTORE_COLUMN].Value != null)
                            {
                                for (int i = 0; i < allServersForm.dataStoreDataGridView.RowCount; i++)
                                {
                                    if (allServersForm.dataStoreDataGridView.Rows[i].Cells[DATASTORE_NAME_COLUMN].Value.ToString() == allServersForm.SourceToMasterDataGridView.Rows[index].Cells[SELECT_DATASTORE_COLUMN].Value.ToString())
                                    {
                                        if (allServersForm.ignoreDatastoreSizeCalculationCheckBox.Checked == false)
                                        {
                                            if (Convert.ToInt32(allServersForm.dataStoreDataGridView.Rows[i].Cells[DATASTORE_FREESPACE_COLUMN].Value) > h.totalSizeOfDisks)
                                            {
                                                Debug.WriteLine("Printing total size of the disk " + h.targetDataStore + h.totalSizeOfDisks + " " + allServersForm.dataStoreDataGridView.Rows[i].Cells[DATASTORE_FREESPACE_COLUMN].Value.ToString());

                                                //h.datastore = allServersForm.dataStoreDataGridView.Rows[rowIndex].Cells[DATASTORE_NAME_COLUMN].Value.ToString();

                                                h.targetDataStore = allServersForm.SourceToMasterDataGridView.Rows[index].Cells[SELECT_DATASTORE_COLUMN].Value.ToString();
                                                SelectedDataStore(allServersForm, h, i);

                                                return true;
                                            }
                                            else
                                            {
                                                allServersForm.SourceToMasterDataGridView.Rows[index].Cells[SELECT_DATASTORE_COLUMN].Value = null;
                                                // allServersForm.dataStoreDataGridView.Rows[index].Cells[DATASOURCE_SELECTED_COLUMN].Value = false;
                                                allServersForm.SourceToMasterDataGridView.RefreshEdit();
                                                MessageBox.Show("Selected datastore does not have enough space ", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);

                                                return false;

                                            }
                                        }
                                        else
                                        {
                                            Trace.WriteLine(DateTime.Now + "\t User has selected to ignore the datastroe calculation");
                                            h.targetDataStore = allServersForm.SourceToMasterDataGridView.Rows[index].Cells[SELECT_DATASTORE_COLUMN].Value.ToString();
                                            return true;
                                        }
                                    }
                                }

                            }

                        }
                    }


                }

                Trace.WriteLine(DateTime.Now + "\t Exiting: IncrementalDiskDataStoreSelection() in ESX_SourceToMasterTargetPanelHandler");
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

        internal bool DataSotreDataGridViewValues(AllServersForm allServersForm, int rowIndex)
        {
            try
            {
                // When datastore columns values got changes we will call this method and we will check whther it is 
                // First time selection or not if first time selection we will sleect the same datastore for all the machines...
                // If not we will select only for that vm only.....
                Trace.WriteLine(DateTime.Now + "\t Entered: DataSotreDataGridViewValues() in ESX_SourceToMasterTargetPanelHandler");


                Host host = new Host();

                if (allServersForm.appNameSelected == AppName.Bmr)
                {

                    if (allServersForm.listIndexPrepared == 0)
                    {
                        host.ip = allServersForm.SourceToMasterDataGridView.Rows[0].Cells[Esx_SourceToMasterPanelHandler.SOURCE_SERVER_IP].Value.ToString();
                    }
                    else
                    {
                        host.ip = allServersForm.dataStoreCaptureDisplayNamePrepared;
                    }
                }

                if (allServersForm.appNameSelected == AppName.Esx || allServersForm.appNameSelected == AppName.Adddisks || allServersForm.appNameSelected == AppName.Failback)
                {
                    if (allServersForm.listIndexPrepared == 0)
                    {
                        host.displayname = allServersForm.SourceToMasterDataGridView.Rows[0].Cells[Esx_SourceToMasterPanelHandler.SOURCE_SERVER_IP].Value.ToString();
                    }
                    else
                    {
                        host.displayname = allServersForm.dataStoreCaptureDisplayNamePrepared;
                    }
                }


                allServersForm.selectedSourceListByUser.DoesHostExist(host, ref allServersForm.listIndexPrepared);
                Host h1 = (Host)allServersForm.selectedSourceListByUser._hostList[allServersForm.listIndexPrepared];

                if (allServersForm.appNameSelected == AppName.Bmr)
                {
                    if (_firstTimeSelection == true)
                    {
                        BareMetalDataStoreSelectionDataStore(allServersForm, h1, rowIndex);
                        SelectDataStoreForFirstTime(allServersForm, rowIndex);
                        _firstTimeSelection = false;
                    }
                    else
                    {

                        BareMetalDataStoreSelectionDataStore(allServersForm, h1, rowIndex);
                    }
                }
                if (allServersForm.appNameSelected == AppName.Esx || allServersForm.appNameSelected == AppName.Failback)
                {
                    Trace.WriteLine(DateTime.Now + " \t Came here to print the firsttime selection value " + _firstTimeSelection.ToString());
                    if (_firstTimeSelection == true)
                    {
                        Trace.WriteLine(DateTime.Now + " \t Came here to select all targetdatastores");
                        EsxDataStoreSelectedDataStore(allServersForm, h1, rowIndex);
                        SelectDataStoreForFirstTime(allServersForm, rowIndex);
                        _firstTimeSelection = false;
                    }
                    else
                    {
                        EsxDataStoreSelectedDataStore(allServersForm, h1, rowIndex);
                    }

                }

                if (allServersForm.appNameSelected == AppName.Adddisks)
                {
                    
                        IncrementalDiskDataStoreSelection(allServersForm, h1, rowIndex);
                   
                }

                if (allServersForm.appNameSelected == AppName.Offlinesync)
                {
                    SelectDataStoreForOffLineSync(allServersForm, rowIndex);
                    //DataSroteSelectionOrUnselectionForOfficeLineSync(allServersForm, rowIndex);
                    allServersForm.SourceToMasterDataGridView.RefreshEdit();
                    //SelectDataStoreForOffLineSync(allServersForm, rowIndex);
                }
                Trace.WriteLine(DateTime.Now + "\t Exiting: DataSotreDataGridViewValues() in ESX_SourceToMasterTargetPanelHandler");
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
        internal bool EsxDataStoreSelectedDataStore(AllServersForm allServersForm, Host h, int index)
        {
            // In case of esx we will go by displayname where as in p2v we will go by ip.....
            Trace.WriteLine(DateTime.Now + "\t Entered: EsxDataStoreSelectedDataStore() in ESX_SourceToMasterTargetPanelHandler");
            try
            {

                Debug.WriteLine("Entered to find the calculation exactly");
                Host host = new Host();
                Host masterHost = (Host)allServersForm.selectedMasterListbyUser._hostList[0];
                if (allServersForm.appNameSelected == AppName.Esx || allServersForm.appNameSelected == AppName.Failback)
                {
                    if (allServersForm.listIndexPrepared == 0)
                    {
                        host.displayname = allServersForm.SourceToMasterDataGridView.Rows[0].Cells[Esx_SourceToMasterPanelHandler.SOURCE_SERVER_IP].Value.ToString();
                    }
                    else
                    {
                        host.displayname = allServersForm.dataStoreCaptureDisplayNamePrepared;
                    }
                }
                if (allServersForm.SourceToMasterDataGridView.Rows[index].Cells[Esx_SourceToMasterPanelHandler.SELECT_DATASTORE_COLUMN].Value != null)
                {
                    if (h.targetDataStore == null)
                    {
                        if (host.displayname == h.displayname)
                        {
                            if (allServersForm.SourceToMasterDataGridView.Rows[index].Cells[SELECT_DATASTORE_COLUMN].Value != null)
                            {
                                for (int i = 0; i < allServersForm.dataStoreDataGridView.RowCount; i++)
                                {
                                    if (allServersForm.dataStoreDataGridView.Rows[i].Cells[DATASTORE_NAME_COLUMN].Value.ToString() == allServersForm.SourceToMasterDataGridView.Rows[index].Cells[SELECT_DATASTORE_COLUMN].Value.ToString())
                                    {
                                        if (allServersForm.ignoreDatastoreSizeCalculationCheckBox.Checked == false)
                                        {
                                            if (Convert.ToInt32(allServersForm.dataStoreDataGridView.Rows[i].Cells[DATASTORE_FREESPACE_COLUMN].Value) > h.totalSizeOfDisks)
                                            {
                                                Debug.WriteLine("Printing total size of the disk " + h.targetDataStore + h.totalSizeOfDisks + " " + allServersForm.dataStoreDataGridView.Rows[i].Cells[DATASTORE_FREESPACE_COLUMN].Value.ToString());

                                                //h.datastore = allServersForm.dataStoreDataGridView.Rows[rowIndex].Cells[DATASTORE_NAME_COLUMN].Value.ToString();

                                                h.targetDataStore = allServersForm.SourceToMasterDataGridView.Rows[index].Cells[SELECT_DATASTORE_COLUMN].Value.ToString();
                                                /* if (h.vSpherehost == masterHost.vSpherehost)
                                                 {
                                                     //if (h.datastore == h.targetDataStore)
                                                     {
                                                         h.targetDataStore = null;
                                                         allServersForm.SourceToMasterDataGridView.Rows[index].Cells[SELECT_DATASTORE_COLUMN].Value = null;
                                                         // allServersForm.dataStoreDataGridView.Rows[index].Cells[DATASOURCE_SELECTED_COLUMN].Value = false;
                                                         allServersForm.SourceToMasterDataGridView.RefreshEdit();
                                                        // MessageBox.Show("Selected machines source and target datastore is same. Seelct some other datastore", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                                         //return false;
                                                     }
                                                 }*/
                                                SelectedDataStore(allServersForm, h, i);

                                                return true;
                                            }
                                            else
                                            {
                                                allServersForm.SourceToMasterDataGridView.Rows[index].Cells[SELECT_DATASTORE_COLUMN].Value = null;

                                                // allServersForm.dataStoreDataGridView.Rows[index].Cells[DATASOURCE_SELECTED_COLUMN].Value = false;
                                                allServersForm.SourceToMasterDataGridView.RefreshEdit();
                                                MessageBox.Show("Selected datastore does not have enough space ", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);

                                                return false;
                                            }
                                        }
                                        else
                                        {
                                            Trace.WriteLine(DateTime.Now + "\t User has selected to ignore the datastore size calculation");
                                            h.targetDataStore = allServersForm.SourceToMasterDataGridView.Rows[index].Cells[SELECT_DATASTORE_COLUMN].Value.ToString();
                                            return true;
                                        }
                                    }
                                }

                            }

                        }
                    }
                    else
                    {
                        if (h.targetDataStore != allServersForm.SourceToMasterDataGridView.Rows[index].Cells[SELECT_DATASTORE_COLUMN].Value.ToString())
                        {
                            for (int i = 0; i < allServersForm.dataStoreDataGridView.RowCount; i++)
                            {
                                if (h.targetDataStore == allServersForm.dataStoreDataGridView.Rows[i].Cells[DATASTORE_NAME_COLUMN].Value.ToString())
                                {
                                    UnSelectDataStoreCalCulation(allServersForm, h, i);
                                    h.targetDataStore = null;
                                }

                            }



                        }
                        if (host.displayname == h.displayname)
                        {
                            if (allServersForm.SourceToMasterDataGridView.Rows[index].Cells[SELECT_DATASTORE_COLUMN].Value != null)
                            {
                                for (int i = 0; i < allServersForm.dataStoreDataGridView.RowCount; i++)
                                {
                                    if (allServersForm.dataStoreDataGridView.Rows[i].Cells[DATASTORE_NAME_COLUMN].Value.ToString() == allServersForm.SourceToMasterDataGridView.Rows[index].Cells[SELECT_DATASTORE_COLUMN].Value.ToString())
                                    {
                                        if (allServersForm.ignoreDatastoreSizeCalculationCheckBox.Checked == false)
                                        {
                                            if (Convert.ToInt32(allServersForm.dataStoreDataGridView.Rows[i].Cells[DATASTORE_FREESPACE_COLUMN].Value) > h.totalSizeOfDisks)
                                            {
                                                Debug.WriteLine("Printing total size of the disk " + h.targetDataStore + h.totalSizeOfDisks + " " + allServersForm.dataStoreDataGridView.Rows[i].Cells[DATASTORE_FREESPACE_COLUMN].Value.ToString());

                                                //h.datastore = allServersForm.dataStoreDataGridView.Rows[rowIndex].Cells[DATASTORE_NAME_COLUMN].Value.ToString();

                                                h.targetDataStore = allServersForm.SourceToMasterDataGridView.Rows[index].Cells[SELECT_DATASTORE_COLUMN].Value.ToString();
                                                SelectedDataStore(allServersForm, h, i);

                                                return true;
                                            }
                                            else
                                            {
                                                allServersForm.SourceToMasterDataGridView.Rows[index].Cells[SELECT_DATASTORE_COLUMN].Value = null;
                                                // allServersForm.dataStoreDataGridView.Rows[index].Cells[DATASOURCE_SELECTED_COLUMN].Value = false;
                                                allServersForm.SourceToMasterDataGridView.RefreshEdit();
                                                MessageBox.Show("Selected datastore does not have enough space ", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);

                                                return false;

                                            }
                                            
                                        }
                                        else
                                        {
                                            Trace.WriteLine(DateTime.Now + "\t User has selected to ignore datastroe size calculation");
                                            h.targetDataStore = allServersForm.SourceToMasterDataGridView.Rows[index].Cells[SELECT_DATASTORE_COLUMN].Value.ToString();
                                            return true;
                                        }
                                    }
                                }

                            }

                        }
                    }


                }
                else
                {


                }
                Trace.WriteLine(DateTime.Now + "\t Exiting: EsxDataStoreSelectedDataStore() in ESX_SourceToMasterTargetPanelHandler");

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


        internal bool BareMetalDataStoreSelectionDataStore(AllServersForm allServersForm, Host h, int index)
        {
            Trace.WriteLine(DateTime.Now + "\t Entered: BareMetalDataStoreSelectionDataStore() in ESX_SourceToMasterTargetPanelHandler");
            try
            {

                Host host = new Host();
                if (allServersForm.appNameSelected == AppName.Bmr)
                {

                    if (allServersForm.listIndexPrepared == 0)
                    {
                        host.ip = allServersForm.SourceToMasterDataGridView.Rows[0].Cells[Esx_SourceToMasterPanelHandler.SOURCE_SERVER_IP].Value.ToString();
                    }
                    else
                    {
                        host.ip = allServersForm.dataStoreCaptureDisplayNamePrepared;
                    }
                }

                if (allServersForm.SourceToMasterDataGridView.Rows[index].Cells[Esx_SourceToMasterPanelHandler.SELECT_DATASTORE_COLUMN].Value != null)
                {
                    if (h.targetDataStore == null)
                    {
                        if (host.ip == h.ip)
                        {
                            if (allServersForm.SourceToMasterDataGridView.Rows[index].Cells[SELECT_DATASTORE_COLUMN].Value != null)
                            {
                                for (int i = 0; i < allServersForm.dataStoreDataGridView.RowCount; i++)
                                {
                                    if (allServersForm.dataStoreDataGridView.Rows[i].Cells[DATASTORE_NAME_COLUMN].Value.ToString() == allServersForm.SourceToMasterDataGridView.Rows[index].Cells[SELECT_DATASTORE_COLUMN].Value.ToString())
                                    {
                                        if (allServersForm.ignoreDatastoreSizeCalculationCheckBox.Checked == false)
                                        {
                                            if (Convert.ToInt32(allServersForm.dataStoreDataGridView.Rows[i].Cells[DATASTORE_FREESPACE_COLUMN].Value) > h.totalSizeOfDisks)
                                            {
                                                Debug.WriteLine("Printing total size of the disk " + h.targetDataStore + h.totalSizeOfDisks + " " + allServersForm.dataStoreDataGridView.Rows[i].Cells[DATASTORE_FREESPACE_COLUMN].Value.ToString());

                                                //h.datastore = allServersForm.dataStoreDataGridView.Rows[rowIndex].Cells[DATASTORE_NAME_COLUMN].Value.ToString();

                                                h.targetDataStore = allServersForm.SourceToMasterDataGridView.Rows[index].Cells[SELECT_DATASTORE_COLUMN].Value.ToString();
                                                SelectedDataStore(allServersForm, h, i);

                                                return true;
                                            }
                                            else
                                            {
                                                allServersForm.SourceToMasterDataGridView.Rows[index].Cells[SELECT_DATASTORE_COLUMN].Value = null;
                                                // allServersForm.dataStoreDataGridView.Rows[index].Cells[DATASOURCE_SELECTED_COLUMN].Value = false;
                                                allServersForm.SourceToMasterDataGridView.RefreshEdit();
                                                MessageBox.Show("Selected datastore does not have enough space ", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);

                                                return false;

                                            }
                                        }
                                        else
                                        {
                                            Trace.WriteLine(DateTime.Now + "\t User has selected to ignore the datastore size calculation");
                                            h.targetDataStore = allServersForm.SourceToMasterDataGridView.Rows[index].Cells[SELECT_DATASTORE_COLUMN].Value.ToString();
                                            return true;
                                        }
                                    }
                                }

                            }


                        }
                    }
                    else
                    {
                        if (h.targetDataStore != allServersForm.SourceToMasterDataGridView.Rows[index].Cells[SELECT_DATASTORE_COLUMN].Value.ToString())
                        {
                            for (int i = 0; i < allServersForm.dataStoreDataGridView.RowCount; i++)
                            {
                                if (h.targetDataStore == allServersForm.dataStoreDataGridView.Rows[i].Cells[DATASTORE_NAME_COLUMN].Value.ToString())
                                {
                                    UnSelectDataStoreCalCulation(allServersForm, h, i);
                                    h.targetDataStore = null;
                                }

                            }



                        }
                        if (host.ip == h.ip)
                        {
                            if (allServersForm.SourceToMasterDataGridView.Rows[index].Cells[SELECT_DATASTORE_COLUMN].Value != null)
                            {
                                for (int i = 0; i < allServersForm.dataStoreDataGridView.RowCount; i++)
                                {
                                    if (allServersForm.dataStoreDataGridView.Rows[i].Cells[DATASTORE_NAME_COLUMN].Value.ToString() == allServersForm.SourceToMasterDataGridView.Rows[index].Cells[SELECT_DATASTORE_COLUMN].Value.ToString())
                                    {
                                        if (allServersForm.ignoreDatastoreSizeCalculationCheckBox.Checked == false)
                                        {
                                            if (Convert.ToInt32(allServersForm.dataStoreDataGridView.Rows[i].Cells[DATASTORE_FREESPACE_COLUMN].Value) > h.totalSizeOfDisks)
                                            {
                                                Debug.WriteLine("Printing total size of the disk " + h.targetDataStore + h.totalSizeOfDisks + " " + allServersForm.dataStoreDataGridView.Rows[i].Cells[DATASTORE_FREESPACE_COLUMN].Value.ToString());

                                                //h.datastore = allServersForm.dataStoreDataGridView.Rows[rowIndex].Cells[DATASTORE_NAME_COLUMN].Value.ToString();

                                                h.targetDataStore = allServersForm.SourceToMasterDataGridView.Rows[index].Cells[SELECT_DATASTORE_COLUMN].Value.ToString();
                                                SelectedDataStore(allServersForm, h, i);

                                                return true;
                                            }
                                            else
                                            {
                                                allServersForm.SourceToMasterDataGridView.Rows[index].Cells[SELECT_DATASTORE_COLUMN].Value = null;
                                                // allServersForm.dataStoreDataGridView.Rows[index].Cells[DATASOURCE_SELECTED_COLUMN].Value = false;
                                                allServersForm.SourceToMasterDataGridView.RefreshEdit();

                                                MessageBox.Show("Selected datastore does not have enough space ", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);

                                                return false;

                                            }
                                        }
                                        else
                                        {
                                            Trace.WriteLine(DateTime.Now + "\t User has selected to ignore data store size calculation");
                                            h.targetDataStore = allServersForm.SourceToMasterDataGridView.Rows[index].Cells[SELECT_DATASTORE_COLUMN].Value.ToString();
                                            return true;
                                        }
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
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");

                return false;
            }
            return true;
        }

        internal bool RetentionPath(AllServersForm allServersForm, int index)
        {
           
            Trace.WriteLine(DateTime.Now + "\t Exiting: BareMetalDataStoreSelectionDataStore() in ESX_SourceToMasterTargetPanelHandler");

            return true;
        }

        internal bool RetentionInDays(AllServersForm allServersForm, int index)
        {
            Trace.WriteLine(DateTime.Now + "\t Entered: RetentionInDays() in ESX_SourceToMasterTargetPanelHandler");

            try
            {
                // when user enter retention days value for first time we will take the same value forallthe machines...
                // If it is not first time we will take only for that machine

                for (int i = 0; i < allServersForm.SourceToMasterDataGridView.RowCount; i++)
                {
                    Debug.WriteLine("Reached to enter the same value for all rows123");
                    if (allServersForm.SourceToMasterDataGridView.Rows[index].Cells[RETENTION_IN_DAYS_COLUMN].Value != null)
                    {
                        if (i != index)
                        {
                            Debug.WriteLine("Reached to enter the same value for all rows");
                            if (allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_IN_DAYS_COLUMN].Value != null)
                            {
                                if (allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_IN_DAYS_COLUMN].Value.ToString() == enterValue)
                                {
                                    allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_IN_DAYS_COLUMN].Value = allServersForm.SourceToMasterDataGridView.Rows[index].Cells[RETENTION_IN_DAYS_COLUMN].Value.ToString();
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
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");

                return false;
            }
            Trace.WriteLine(DateTime.Now + "\t Exiting: RetentionInDays() in ESX_SourceToMasterTargetPanelHandler");

            return true;
        }

        internal bool ConsistencyTime(AllServersForm allServersForm, int index)
        {
            Trace.WriteLine(DateTime.Now + "\t Entered: ConsistencyTime() in ESX_SourceToMasterTargetPanelHandler");

            try
            {
                // Consistency interval time should be same for all the machines 
                // If user changes for one machine we iwll change for all the machines.....

                Debug.WriteLine("Reached to enter the same value for all rows123");
                for (int i = 0; i < allServersForm.SourceToMasterDataGridView.RowCount; i++)
                {
                    if (allServersForm.SourceToMasterDataGridView.Rows[index].Cells[CONSISTENCY_TIME_COLUMN].Value != null)
                    {
                        if (i != index)
                        {
                            Debug.WriteLine("Reached to enter the same value for all rows");
                            if (allServersForm.SourceToMasterDataGridView.Rows[i].Cells[CONSISTENCY_TIME_COLUMN].Value != null)
                            {
                               // if (allServersForm.SourceToMasterDataGridView.Rows[i].Cells[CONSISTENCY_TIME_COLUMN].Value.ToString() == enterValue)
                                {
                                    allServersForm.SourceToMasterDataGridView.Rows[i].Cells[CONSISTENCY_TIME_COLUMN].Value = allServersForm.SourceToMasterDataGridView.Rows[index].Cells[CONSISTENCY_TIME_COLUMN].Value.ToString();
                                }
                            }

                        }
                    }

                }

                Trace.WriteLine(DateTime.Now + "\t Exiting: ConsistencyTime() in ESX_SourceToMasterTargetPanelHandler");

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
        internal bool RetentionSize(AllServersForm allServersForm, int index)
        {

            // when user enter retention size value for first time we will take the same value forallthe machines...
            // If it is not first time we will take only for that machine
            Trace.WriteLine(DateTime.Now + "\t Entered: RetentionSize() in ESX_SourceToMasterTargetPanelHandler");
            try
            {
                Debug.WriteLine("Reached to enter the same value for all rows123");
                for (int i = 0; i < allServersForm.SourceToMasterDataGridView.RowCount; i++)
                {
                    if (allServersForm.SourceToMasterDataGridView.Rows[index].Cells[RETENTION_SIZE_COLUMN].Value != null)
                    {
                        if (i != index)
                        {
                            Debug.WriteLine("Reached to enter the same value for all rows");
                            if (allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_SIZE_COLUMN].Value != null)
                            {
                                if (allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_SIZE_COLUMN].Value.ToString() == enterValue)
                                {
                                    allServersForm.SourceToMasterDataGridView.Rows[i].Cells[RETENTION_SIZE_COLUMN].Value = allServersForm.SourceToMasterDataGridView.Rows[index].Cells[RETENTION_SIZE_COLUMN].Value.ToString();
                                }
                            }
                        }
                    }
                }
                Trace.WriteLine(DateTime.Now + "\t Exiting: RetentionSize() in ESX_SourceToMasterTargetPanelHandler");
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


        internal bool SelectLunForRDMDisk(AllServersForm allServersForm, int index)
        {
            try
            {
                // When user slects the rdm lun column and if rdm luns are present in that vm 
                // we will popup one form. From that user has to select lun for each lun....
                // User has to select free luns only..
                // After select one lun we wont show that lun for the next vm.....

                Trace.WriteLine(DateTime.Now + "\t Entered: SelectLunForRDMDisk() in ESX_SourceToMasterTargetPanelHandler");
                if (allServersForm.appNameSelected == AppName.Bmr || (allServersForm.appNameSelected == AppName.Adddisks && ResumeForm.selectedActionForPlan == "BMR Protection"))
                {
                    if (allServersForm.SourceToMasterDataGridView.Rows[index].Cells[SELECT_LUNS_COLUMN].Value.ToString() == "Select luns" || allServersForm.SourceToMasterDataGridView.Rows[index].Cells[SELECT_LUNS_COLUMN].Value.ToString() == "Available")
                    {
                        foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                        {
                            if (h.ip == allServersForm.SourceToMasterDataGridView.Rows[index].Cells[SOURCE_SERVER_IP].Value.ToString())
                            {
                                h.rdmdisk = true;
                                h.rdmpDisk = "TRUE";
                                foreach (Hashtable hash in h.disks.list)
                                {
                                    hash["Rdm"] = "yes";
                                }
                                EsxRdmSelectionForm rdm = new EsxRdmSelectionForm(h, allServersForm.selectedMasterListbyUser);
                                rdm.ShowDialog();
                                if (rdm.canceled == false)
                                {
                                    allServersForm.selectedMasterListbyUser = rdm.masterSelectedHostList;

                                    foreach (Host h1 in allServersForm.selectedSourceListByUser._hostList)
                                    {
                                        if (h1.displayname == rdm.lunsSelectedHost.displayname)
                                        {
                                            h1.disks = rdm.lunsSelectedHost.disks;
                                            h1.convertRdmpDiskToVmdk = false;
                                            allServersForm.SourceToMasterDataGridView.Rows[index].Cells[SELECT_LUNS_COLUMN].Value = "Available";
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                else
                {
                    if (allServersForm.SourceToMasterDataGridView.Rows[index].Cells[SELECT_LUNS_COLUMN].Value.ToString() == "Select luns" || allServersForm.SourceToMasterDataGridView.Rows[index].Cells[SELECT_LUNS_COLUMN].Value.ToString() == "Available")
                    {
                        foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                        {
                            if (h.displayname == allServersForm.SourceToMasterDataGridView.Rows[index].Cells[SOURCE_SERVER_IP].Value.ToString())
                            {
                                EsxRdmSelectionForm rdm = new EsxRdmSelectionForm(h, allServersForm.selectedMasterListbyUser);
                                rdm.ShowDialog();
                                if (rdm.canceled == false)
                                {
                                    allServersForm.selectedMasterListbyUser = rdm.masterSelectedHostList;

                                    foreach (Host h1 in allServersForm.selectedSourceListByUser._hostList)
                                    {
                                        if (h1.displayname == rdm.lunsSelectedHost.displayname)
                                        {
                                            h1.disks = rdm.lunsSelectedHost.disks;
                                            allServersForm.SourceToMasterDataGridView.Rows[index].Cells[SELECT_LUNS_COLUMN].Value = "Available";
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                Trace.WriteLine(DateTime.Now + "\t Exiting: SelectLunForRDMDisk() in ESX_SourceToMasterTargetPanelHandler");
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
        internal bool SelectDataStoreForFirstTime(AllServersForm allServersForm, int index)
        {
            try
            {
                // when user selects datastore foe the first time 
                //We will select that datastore for all vms if free space is available on the datastore.....

                for (int i = 0; i < allServersForm.SourceToMasterDataGridView.RowCount; i++)
                {
                    allServersForm.SourceToMasterDataGridView.Rows[i].Cells[Esx_SourceToMasterPanelHandler.SELECT_DATASTORE_COLUMN].Value = allServersForm.SourceToMasterDataGridView.Rows[index].Cells[SELECT_DATASTORE_COLUMN].Value.ToString();
                }
                Trace.WriteLine(DateTime.Now + "\t Entered: SelectDataStoreForFirstTime() in ESX_SourceToMasterTargetPanelHandler");

                foreach (Host h in allServersForm.selectedSourceListByUser._hostList)
                {
                    for (int i = 0; i < allServersForm.dataStoreDataGridView.RowCount; i++)
                    {
                        if (allServersForm.dataStoreDataGridView.Rows[i].Cells[DATASTORE_NAME_COLUMN].Value.ToString() == allServersForm.SourceToMasterDataGridView.Rows[index].Cells[SELECT_DATASTORE_COLUMN].Value.ToString())
                        {
                            if (h.targetDataStore == null)
                            {
                                

                                int a = Convert.ToInt32(allServersForm.dataStoreDataGridView.Rows[i].Cells[DATASTORE_FREESPACE_COLUMN].Value);
                                Trace.WriteLine("entered to calculate the datastore sizes " + h.totalSizeOfDisks + a + "   " + (a - h.totalSizeOfDisks));
                                //MessageBox.Show("Printing a "+a.ToString() + "usedspace"+ allServersForm.dataStoreDataGridView.Rows[i].Cells[DATASTORE_USEDSPACE_COLUMN].Value.ToString());
                                if (allServersForm.ignoreDatastoreSizeCalculationCheckBox.Checked == false)
                                {
                                    if (a - h.totalSizeOfDisks >= 0)
                                    {
                                        allServersForm.dataStoreDataGridView.Rows[i].Cells[DATASTORE_FREESPACE_COLUMN].Value = a - h.totalSizeOfDisks;

                                        // MessageBox.Show("value is "+ h.totalSizeOfDisks + Convert.ToInt32(allServersForm.dataStoreDataGridView.Rows[i].Cells[DATASTORE_USEDSPACE_COLUMN].Value));
                                        allServersForm.dataStoreDataGridView.Rows[i].Cells[DATASTORE_USEDSPACE_COLUMN].Value = h.totalSizeOfDisks + Convert.ToInt32(allServersForm.dataStoreDataGridView.Rows[i].Cells[DATASTORE_USEDSPACE_COLUMN].Value);
                                        allServersForm.dataStoreDataGridView.RefreshEdit();
                                        h.targetDataStore = allServersForm.SourceToMasterDataGridView.Rows[index].Cells[SELECT_DATASTORE_COLUMN].Value.ToString();
                                        Host h1 = (Host)allServersForm.selectedMasterListbyUser._hostList[0];
                                        foreach (DataStore d in h1.dataStoreList)
                                        {
                                            if (d.name == h.targetDataStore)
                                            {
                                                d.freeSpace = int.Parse(allServersForm.dataStoreDataGridView.Rows[i].Cells[DATASTORE_FREESPACE_COLUMN].Value.ToString());

                                            }
                                        }
                                    }
                                    else
                                    {
                                        for (int j = 0; j < allServersForm.SourceToMasterDataGridView.RowCount; j++)
                                        {
                                            if (allServersForm.appNameSelected == AppName.Esx || (allServersForm.appNameSelected == AppName.Offlinesync && ResumeForm.selectedActionForPlan == "Esx"))
                                            {
                                                if (h.displayname == allServersForm.SourceToMasterDataGridView.Rows[j].Cells[SOURCE_SERVER_IP].Value.ToString())
                                                {
                                                    Trace.WriteLine(DateTime.Now + "\t Printing index values " + i.ToString());
                                                    allServersForm.SourceToMasterDataGridView.Rows[j].Cells[SELECT_DATASTORE_COLUMN].Value = null;
                                                }
                                            }
                                            if (allServersForm.appNameSelected == AppName.Bmr || (allServersForm.appNameSelected == AppName.Offlinesync && ResumeForm.selectedActionForPlan == "P2v"))
                                            {
                                                if (h.ip == allServersForm.SourceToMasterDataGridView.Rows[j].Cells[SOURCE_SERVER_IP].Value.ToString())
                                                {
                                                    Trace.WriteLine(DateTime.Now + "\t Printing index values " + i.ToString());
                                                    allServersForm.SourceToMasterDataGridView.Rows[j].Cells[SELECT_DATASTORE_COLUMN].Value = null;
                                                }

                                            }
                                        }
                                        //MessageBox.Show("The selected datastore does not have enough space to export machine(s)", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                        allServersForm.SourceToMasterDataGridView.RefreshEdit();
                                        // return false;
                                    }
                                }
                                else
                                {                                

                                    // MessageBox.Show("value is "+ h.totalSizeOfDisks + Convert.ToInt32(allServersForm.dataStoreDataGridView.Rows[i].Cells[DATASTORE_USEDSPACE_COLUMN].Value));
                                    //allServersForm.dataStoreDataGridView.Rows[i].Cells[DATASTORE_USEDSPACE_COLUMN].Value = h.totalSizeOfDisks + Convert.ToInt32(allServersForm.dataStoreDataGridView.Rows[i].Cells[DATASTORE_USEDSPACE_COLUMN].Value);
                                    allServersForm.dataStoreDataGridView.RefreshEdit();
                                    h.targetDataStore = allServersForm.SourceToMasterDataGridView.Rows[index].Cells[SELECT_DATASTORE_COLUMN].Value.ToString();                         

                                }
                            }
                        }
                    }
                }
                Trace.WriteLine(DateTime.Now + "\t Exiting: SelectDataStoreForFirstTime() in ESX_SourceToMasterTargetPanelHandler");
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
        internal bool UnselctMachine(AllServersForm allServersForm, int index)
        {
            try
            {
                // When user wants to unselect machine we will remove that machine form our data structures like _selectedSourceList
                // Nothing but an arraylist.....
                // We need to remove the same form the psuhagent screen and from the first screen also....

                Host h = new Host();
                //assigning the mastertarget values to masterhost.
                Host masterHost = (Host)allServersForm.selectedMasterListbyUser._hostList[0];
                int sourceMachineIndex = 0;
                if (allServersForm.appNameSelected == AppName.Bmr || (allServersForm.appNameSelected == AppName.Offlinesync && ResumeForm.selectedActionForPlan == "P2v") || (allServersForm.appNameSelected == AppName.Adddisks && ResumeForm.selectedActionForPlan == "BMR Protection"))
                {//reading the values for the datagridview columns.
                    h.ip = allServersForm.SourceToMasterDataGridView.Rows[index].Cells[SOURCE_SERVER_IP].Value.ToString();
                }
                else
                {//reading the values for the datagridview columns.
                    h.displayname = allServersForm.SourceToMasterDataGridView.Rows[index].Cells[SOURCE_SERVER_IP].Value.ToString();
                }
                if (allServersForm.selectedSourceListByUser.DoesHostExist(h, ref sourceMachineIndex))
                {//checking the host exists in the sourcelist or not 
                    h = (Host)allServersForm.selectedSourceListByUser._hostList[sourceMachineIndex];
                }
                else
                {
                    Trace.WriteLine(DateTime.Now + " \t host is not present in source list");
                    return false;
                }
               // checking whether the datastore is selected for the particular machine if so we need to re assign the total size to datastore freespace.
                    if (h.targetDataStore != null)
                    {
                        if (allServersForm.ignoreDatastoreSizeCalculationCheckBox.Checked == false)
                        {
                            foreach (DataStore d in masterHost.dataStoreList)
                            {
                                if (d.name == h.targetDataStore && d.vSpherehostname == masterHost.vSpherehost)
                                {
                                    d.freeSpace = d.freeSpace + h.totalSizeOfDisks;
                                    h.targetDataStore = null;

                                    for (int i = 0; i < allServersForm.dataStoreDataGridView.RowCount; i++)
                                    {
                                        if (d.name == allServersForm.dataStoreDataGridView.Rows[i].Cells[DATASTORE_NAME_COLUMN].Value.ToString())
                                        {
                                            allServersForm.dataStoreDataGridView.Rows[i].Cells[DATASTORE_FREESPACE_COLUMN].Value = d.freeSpace;
                                        }
                                    }
                                }
                            }
                        }
                        else
                        {
                            h.targetDataStore = null;
                        }
                        
                    }
                    if (allServersForm.appNameSelected == AppName.Esx || (allServersForm.appNameSelected == AppName.Offlinesync && ResumeForm.selectedActionForPlan == "Esx"))
                    {
                        
                        //checking the datagridview values of the first screen and then un checking that machine in the datagridview...
                        for (int i = 0; i < allServersForm.addSourceTreeView.Nodes.Count; i++)
                        {
                            for (int j = 0; j < allServersForm.addSourceTreeView.Nodes[i].Nodes.Count; j++)
                            {
                                if (allServersForm.addSourceTreeView.Nodes[i].Nodes[j].Text == h.displayname && allServersForm.addSourceTreeView.Nodes[i].Text == h.vSpherehost)
                                {
                                    allServersForm.addSourceTreeView.Nodes[i].Nodes[j].Checked = false;
                                }
                            }
                        }
                        /*for (int i = 0; i < allServersForm.addSourcePanelEsxDataGridView.RowCount; i++)
                        {
                            if (h.displayname == allServersForm.addSourcePanelEsxDataGridView.Rows[i].Cells[ESX_PrimaryServerPanelHandler.DISPLAY_NAME_COLUMN].Value.ToString())
                            {
                                Trace.WriteLine(DateTime.Now + " \t Came here to delete row in first screen target grid " + i.ToString());
                                allServersForm.addSourcePanelEsxDataGridView.Rows[i].Cells[ESX_PrimaryServerPanelHandler.SOURCE_CHECKBOX_COLUMN].Value = false;
                                allServersForm.addSourcePanelEsxDataGridView.RefreshEdit();
                            }
                        }*/
                    }
                    if (allServersForm.appNameSelected == AppName.Bmr || (allServersForm.appNameSelected == AppName.Offlinesync && ResumeForm.selectedActionForPlan == "P2v"))
                    {//checking the datagridview values of the first screen and then un checking that machine in the datagridview...for p2v case
                        
                    }
                    //checking the datagridview values of the push agent screen and then deleting the row in the datagridview...
                    for (int i = 0; i < allServersForm.pushAgentDataGridView.RowCount; i++)
                    {
                        if (h.displayname == allServersForm.pushAgentDataGridView.Rows[i].Cells[PushAgentPanelHandler.DISPLAY_NAME_COLUMN].Value.ToString())
                        {
                            if (h.ip == allServersForm.pushAgentDataGridView.Rows[i].Cells[PushAgentPanelHandler.IP_COLUMN].Value.ToString())
                            {
                                allServersForm.pushAgentDataGridView.Rows.RemoveAt(i);
                                allServersForm.pushAgentDataGridView.RefreshEdit();
                            }
                        }
                    }
                    if (allServersForm.appNameSelected == AppName.Failback)
                    {//checking thevalues in second screen and removing the row from the table.
                        /*for (int i = 0; i < allServersForm.addSourcePanelEsxDataGridView.RowCount; i++)
                        {
                            if (h.displayname == allServersForm.addSourcePanelEsxDataGridView.Rows[i].Cells[ESX_PrimaryServerPanelHandler.DISPLAY_NAME_COLUMN].Value.ToString())
                            {
                                allServersForm.addSourcePanelEsxDataGridView.Rows.RemoveAt(i);
                                allServersForm.addSourcePanelEsxDataGridView.RefreshEdit();
                            }
                        }*/
                        for (int i = 0; i < allServersForm.detailsOfAdditionOfDiskDataGridView.RowCount; i++)
                        {//checking thevalues in first screen and unchecking the machine 
                            if (h.displayname == allServersForm.detailsOfAdditionOfDiskDataGridView.Rows[i].Cells[AdditionOfDiskSelectMachinePanelHandler.HostnameColumn].Value.ToString())
                            {
                                allServersForm.detailsOfAdditionOfDiskDataGridView.Rows[i].Cells[AdditionOfDiskSelectMachinePanelHandler.AdddiskColumn].Value = false;
                                allServersForm.detailsOfAdditionOfDiskDataGridView.RefreshEdit();
                            }
                        }
                    }
                    if (allServersForm.appNameSelected == AppName.Adddisks)
                    {//checking thevalues in first screen and unchecking the machine 
                        for (int i = 0; i < allServersForm.detailsOfAdditionOfDiskDataGridView.RowCount; i++)
                        {
                            if (h.displayname == allServersForm.detailsOfAdditionOfDiskDataGridView.Rows[i].Cells[AdditionOfDiskSelectMachinePanelHandler.HostnameColumn].Value.ToString())
                            {
                                allServersForm.detailsOfAdditionOfDiskDataGridView.Rows[i].Cells[AdditionOfDiskSelectMachinePanelHandler.AdddiskColumn].Value = false;
                                allServersForm.detailsOfAdditionOfDiskDataGridView.RefreshEdit();
                            }
                        }
                        for (int i = 0; i < allServersForm.addDiskDetailsDataGridView.RowCount; i++)
                        {//checking thevalues in second screen and removing the row from the table.
                            if (h.displayname == allServersForm.addDiskDetailsDataGridView.Rows[i].Cells[AddDiskPanelHandler.SourceDisplaynameColumn].Value.ToString())
                            {
                                allServersForm.addDiskDetailsDataGridView.Rows.RemoveAt(i);
                                allServersForm.addDiskDetailsDataGridView.RefreshEdit();
                            }
                        }
                        AdditionOfDiskSelectMachinePanelHandler.temperorySelectedSourceList.RemoveHost(h);
                        allServersForm.newDiskDetailsDataGridView.Rows.Clear();
                        allServersForm.newDiskDetailsDataGridView.Visible = false;
                    }
                    //removing the machine form the protected list.
                    allServersForm.selectedSourceListByUser.RemoveHost(h);
                    //removing the row from the current screen datagridview(SourceToMasterTargetPanel)
                    allServersForm.SourceToMasterDataGridView.Rows.RemoveAt(index);
                    Trace.WriteLine(DateTime.Now + " \t Printing sourcelist count " + allServersForm.selectedSourceListByUser._hostList.Count.ToString());
                    if (allServersForm.selectedSourceListByUser._hostList.Count == 0)
                    {
                        MessageBox.Show("There are no machines to protect.", "Information", MessageBoxButtons.OK, MessageBoxIcon.Information);
                        allServersForm.nextButton.Enabled = false;
                    }
             
                return true;
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
}

        