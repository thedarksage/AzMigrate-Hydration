using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.Windows.Forms;
using Com.Inmage.Tools;
using Com.Inmage.Esxcalls;
using System.Collections;


namespace Com.Inmage.Wizard
{
    class DrDrillPanelHandler : PanelHandler
    {
        internal static int SOURCE_DISPLAYNAME_COLUMN = 0;
        internal static int TARGET_DISPLAY_NAME_COLUMN = 1;
        internal static int HOST_NAME_COLUMN = 2;
        internal static int DATASTORE_COLUMN = 4;
        internal static int SELECT_LUNS_COLUMN = 5;
        internal static int OLD_DATASTORE_COLUMN = 3;
        internal bool firstTimeEnterScreen = true;
        internal bool _firstTimeSelection = true;
        HostList masterList = new HostList();
        /*
         * This will fill the table with the datastructue which we alreay have and selected byt eh user for DR Drill.
         * We will get the latest info about the datastore info like free space an used space form the Info.xml.
         * Info.xml values are stroe in _esx.GetHostList();
         * We will get the total vm size by calculating the disk sizes.
         */
        public DrDrillPanelHandler()
        {
           
        }

        internal override bool Initialize(AllServersForm allServersForm)
        {
            try
            {
                if (masterList._hostList.Count == 0)
                {
                    masterList.AddOrReplaceHost(allServersForm.masterHostAdded);
                }
                int datastroeIndex = 0;
                allServersForm.helpContentLabel.Text = HelpForcx.Drdrill3;

                Host masterHostofTarget = (Host)allServersForm.esxSelected.GetHostList[0];
                allServersForm.targetDatastroeDataGridView.Rows.Clear();
                allServersForm.drDrillDatastroeDataGridView.Rows.Clear();
                //At the time of drdrill we need to show datastore and freespaces for targetesx so thats why are filling latest
                //Values to the datastroe list.....
                masterHostofTarget.dataStoreList = allServersForm.esxSelected.dataStoreList;
                 allServersForm.masterHostAdded.dataStoreList = allServersForm.esxSelected.dataStoreList;
                  
                // Filling the datastroe details with the latest values which we had already...
                allServersForm.drillDataStoreColumn.Items.Clear();
              
                foreach (DataStore d in masterHostofTarget.dataStoreList)
                {
                    if (allServersForm.masterHostAdded.vSpherehost != null)
                    {
                        if (d.vSpherehostname == allServersForm.masterHostAdded.vSpherehost)
                        {
                            allServersForm.targetDatastroeDataGridView.Rows.Add(1);
                            allServersForm.targetDatastroeDataGridView.Rows[datastroeIndex].Cells[0].Value = d.name;
                            allServersForm.targetDatastroeDataGridView.Rows[datastroeIndex].Cells[1].Value = d.totalSize;
                            allServersForm.targetDatastroeDataGridView.Rows[datastroeIndex].Cells[2].Value = d.totalSize - d.freeSpace;
                            allServersForm.targetDatastroeDataGridView.Rows[datastroeIndex].Cells[3].Value = d.freeSpace;
                            allServersForm.drillDataStoreColumn.Items.Add(d.name);
                           
                            datastroeIndex++;
                        }
                    }
                    else
                    {
                        allServersForm.targetDatastroeDataGridView.Rows.Add(1);
                        allServersForm.targetDatastroeDataGridView.Rows[datastroeIndex].Cells[0].Value = d.name;
                        allServersForm.targetDatastroeDataGridView.Rows[datastroeIndex].Cells[1].Value = d.totalSize;
                        allServersForm.targetDatastroeDataGridView.Rows[datastroeIndex].Cells[2].Value = d.totalSize - d.freeSpace;
                        allServersForm.targetDatastroeDataGridView.Rows[datastroeIndex].Cells[3].Value = d.freeSpace;
                        allServersForm.drillDataStoreColumn.Items.Add(d.name);
                       
                        datastroeIndex++;

                    }


                }

                //Getting the disk sizes which is having the vmdks......
                foreach (Host h in allServersForm.recoveryChecksPassedList._hostList)
                {


                    ArrayList physicalDisks;
                    physicalDisks = h.disks.GetDiskList;
                    h.totalSizeOfDisks = 0;

                    //Move this to initialize of sourceToMaster post 5.5
                    if (h.machineType == "VirtualMachine")
                    {
                        foreach (Hashtable disk in physicalDisks)
                        {

                            if (disk["Rdm"].ToString() == "no" || h.convertRdmpDiskToVmdk == true)
                            {
                                if (disk["Size"] != null)
                                {
                                    float size = float.Parse(disk["Size"].ToString());

                                    size = size / (1024 * 1024);
                                    h.totalSizeOfDisks = h.totalSizeOfDisks + size;
                                }
                            }
                        }
                    }
                    else
                    {
                        foreach (Hashtable disk in physicalDisks)
                        {
                            if (disk["Rdm"].ToString() == "no" || h.convertRdmpDiskToVmdk == true)
                            {
                                if (disk["Size"] != null)
                                {
                                    float size = float.Parse(disk["Size"].ToString());

                                    size = size / (1024 * 1024);
                                    h.totalSizeOfDisks = h.totalSizeOfDisks + size;
                                }
                            }
                        }
                    }
                    Trace.WriteLine(DateTime.Now + "\t Printing the size of the machine " + h.displayname + " size " + h.totalSizeOfDisks.ToString());
                }

                int rowIndex = 0;
                // Filling the datagridview with the selected vms info
                foreach (Host h in allServersForm.recoveryChecksPassedList._hostList)
                {
                    allServersForm.drDrillDatastroeDataGridView.Rows.Add(1);
                    allServersForm.drDrillDatastroeDataGridView.Rows[rowIndex].Cells[SOURCE_DISPLAYNAME_COLUMN].Value = h.displayname;
                    allServersForm.drDrillDatastroeDataGridView.Rows[rowIndex].Cells[TARGET_DISPLAY_NAME_COLUMN].Value = h.new_displayname;
                    allServersForm.drDrillDatastroeDataGridView.Rows[rowIndex].Cells[HOST_NAME_COLUMN].Value = h.hostname;
                    allServersForm.drDrillDatastroeDataGridView.Rows[rowIndex].Cells[OLD_DATASTORE_COLUMN].Value = h.sourceDataStoreForDrdrill;
                    if (h.rdmpDisk == "TRUE" && h.convertRdmpDiskToVmdk == false)
                    {
                        foreach (Hashtable disk in h.disks.list)
                        {
                            if (disk["Selected"].ToString() == "Yes" && disk["Rdm"].ToString() == "yes")
                            {
                                h.rdmdisk = true;
                            }
                        }
                        if (h.rdmSelected == false && h.rdmdisk == true)
                        {
                            allServersForm.drDrillDatastroeDataGridView.Rows[rowIndex].Cells[SELECT_LUNS_COLUMN].Value = "Select luns";
                        }
                        else if (h.rdmSelected == true)
                        {
                            allServersForm.drDrillDatastroeDataGridView.Rows[rowIndex].Cells[SELECT_LUNS_COLUMN].Value = "Available";
                        }
                        else if (h.rdmdisk == false)
                        {
                            allServersForm.drDrillDatastroeDataGridView.Rows[rowIndex].Cells[SELECT_LUNS_COLUMN].Value = "Not applicable";
                        }


                    }
                    else
                    {
                        allServersForm.drDrillDatastroeDataGridView.Rows[rowIndex].Cells[SELECT_LUNS_COLUMN].Value = "Not applicable";
                    }
                    if (h.targetDataStore != null)
                    {
                        allServersForm.drDrillDatastroeDataGridView.Rows[rowIndex].Cells[DATASTORE_COLUMN].Value = h.targetDataStore;
                    }

                    rowIndex++;
                }
                if (allServersForm.hostBasedRadioButton.Checked == true)
                {
                    allServersForm.drDrillDatastroeSelectionLabel.Text = "Select Datastore for DR Drill";
                    allServersForm.datastoresAvailableOnTargetLabel.Visible = true;
                    allServersForm.targetDatastroeDataGridView.Visible = true;
                    allServersForm.drDrillDatastroeDataGridView.Columns[OLD_DATASTORE_COLUMN].Visible = false;
                }
                else
                {
                    allServersForm.drDrillDatastroeSelectionLabel.Text = "Select Datastore for Array based DR Drill";
                    allServersForm.datastoresAvailableOnTargetLabel.Visible = false;
                    allServersForm.targetDatastroeDataGridView.Visible = false;
                    allServersForm.drDrillDatastroeDataGridView.Columns[OLD_DATASTORE_COLUMN].Visible = true;
                }
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: Initialize of dr dril panelhandler " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
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
            return true;
        }

        /*
         * In this process panel we will check whether user has seleted datastroe or not.
         * If not we will block user for the selection.
         */
        internal override bool ProcessPanel(AllServersForm allServersForm)
        {
            try
            {
                for (int i = 0; i < allServersForm.drDrillDatastroeDataGridView.RowCount; i++)
                {
                    if (allServersForm.drDrillDatastroeDataGridView.Rows[i].Cells[DATASTORE_COLUMN].Value == null)
                    {
                        string displayName = allServersForm.drDrillDatastroeDataGridView.Rows[i].Cells[SOURCE_DISPLAYNAME_COLUMN].Value.ToString();
                        MessageBox.Show("Select datastore for the machine " + displayName + " for DR Drill", "Error",MessageBoxButtons.OK,MessageBoxIcon.Error);
                        return false;
                    }
                }

                foreach (Host h in allServersForm.recoveryChecksPassedList._hostList)
                {
                    Trace.WriteLine(DateTime.Now + "\t Printing the Datastroe selected value " + h.targetDataStore);
                }

                foreach (Host h in allServersForm.recoveryChecksPassedList._hostList)
                {
                    foreach (DataStore d in allServersForm.masterHostAdded.dataStoreList)
                    {
                        if (d.name == h.targetDataStore && d.vSpherehostname == allServersForm.masterHostAdded.vSpherehost)
                        {
                            if (d.type == "vsan")
                            {
                                h.vSan = true;
                                //MessageBox.Show("You have select vSAN datastore for: "+ h.displayName + ". vSAN datastore is not supported." +Environment.NewLine
                                //+ " Please select other datastore", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                //return false;
                            }
                            else
                            {
                                h.vSan = false;
                            }

                        }
                    }

                }

            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: ProcessPanel of dr dril panelhandler " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");

                return false;
            }
            return true;
        }

        internal override bool ValidatePanel(AllServersForm allServersForm)
        {// Here we are checking for the block size of the target datastroe selected.
            foreach (Host h in allServersForm.recoveryChecksPassedList._hostList)
            {
                ArrayList physicalDisks = new ArrayList();

                physicalDisks = h.disks.GetDiskList;
                foreach (Hashtable disk in physicalDisks)
                {
                    if (h.machineType == "PhysicalMachine")
                    {
                        Trace.WriteLine("Came to check size of disk with block for p2v p2v");
                        if (disk["Selected"].ToString() == "Yes")
                        {
                            float size = float.Parse(disk["Size"].ToString());

                            //Convert the size to MBs
                            size = size / 1024;
                            foreach (DataStore d in allServersForm.masterHostAdded.dataStoreList)
                            {
                                if (h.targetDataStore == d.name && allServersForm.masterHostAdded.vSpherehost == d.vSpherehostname)
                                {
                                    if (d.filesystemversion < 5)
                                    {
                                        if (d.blockSize != 0)
                                        {
                                            if (d.GetMaxDataFileSizeMB < (long)size)
                                            {//Maximum allowed VMDK size on datastore ( datastore name) is max size Please choose another datastore 
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

                                foreach (DataStore d in allServersForm.masterHostAdded.dataStoreList)
                                {

                                    if (h.targetDataStore == d.name && allServersForm.masterHostAdded.vSpherehost == d.vSpherehostname)
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
            return true;
        }

        internal bool SelectedDataStoreForHost(AllServersForm allServersForm, Host h, int index)
        {
            try
            {
                allServersForm.recoveryChecksPassedList.DoesHostExist(h, ref allServersForm.listIndexPrepared);
                Host h1 = (Host)allServersForm.recoveryChecksPassedList._hostList[allServersForm.listIndexPrepared];
                return true;
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: SelectedDataStoreForHost of dr dril panelhandler " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");

                return false;
            }
     
        }
        /*
         * When user selects datastroe we will call this method to assign datastroe 
         * We will check for the freespace available in the datastroe to create vm if not we will block.
         * If already datastroe is selected for particular vm we will re assign that datastroe space for the particular datastore.
         */
        internal bool EsxDataStoreSelectedDataStore(AllServersForm allServersForm, Host h, int index)
        {
            // In case of esx we will go by displayname where as in p2v we will go by ip.....
            Trace.WriteLine(DateTime.Now + "\t Entered: EsxDataStoreSelectedDataStore() in DrDrillPanelHandler");
            try
            {

                Debug.WriteLine("Entered to find the calculation exactly");
                Host host = new Host();
                Host masterHost = (Host)allServersForm.esxSelected.GetHostList[0];

                if (allServersForm.drDrillDatastroeDataGridView.Rows[index].Cells[DATASTORE_COLUMN].Value != null)
                {
                    if (h.targetDataStore == null)
                    {

                        if (allServersForm.drDrillDatastroeDataGridView.Rows[index].Cells[DATASTORE_COLUMN].Value != null)
                        {
                            for (int i = 0; i < allServersForm.targetDatastroeDataGridView.RowCount; i++)
                            {
                                if (allServersForm.targetDatastroeDataGridView.Rows[i].Cells[0].Value.ToString() == allServersForm.drDrillDatastroeDataGridView.Rows[index].Cells[DATASTORE_COLUMN].Value.ToString())
                                {

                                    if (Convert.ToInt32(allServersForm.targetDatastroeDataGridView.Rows[i].Cells[3].Value) > h.totalSizeOfDisks)
                                    {
                                        Trace.WriteLine("Printing total size of the disk " + h.targetDataStore + h.totalSizeOfDisks + " " + allServersForm.targetDatastroeDataGridView.Rows[i].Cells[3].Value.ToString());

                                        //h.datastore = allServersForm.dataStoreDataGridView.Rows[rowIndex].Cells[DATASTORE_NAME_COLUMN].Value.ToString();

                                        h.targetDataStore = allServersForm.drDrillDatastroeDataGridView.Rows[index].Cells[DATASTORE_COLUMN].Value.ToString();
                                       /* //if (h.vSpherehost == masterHost.vSpherehost)
                                        {
                                            //if (h.datastore == h.targetDataStore)
                                            {
                                                h.targetDataStore = null;
                                                allServersForm.drDrillDatastroeDataGridView.Rows[index].Cells[DATASTORE_COLUMN].Value = null;
                                                // allServersForm.dataStoreDataGridView.Rows[index].Cells[DATASOURCE_SELECTED_COLUMN].Value = false;
                                                allServersForm.drDrillDatastroeDataGridView.RefreshEdit();
                                               // MessageBox.Show("Selected machines source and target datastore is same. Seelct some other datastore", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                                return false;
                                            }
                                        }*/
                                        SelectedDataStore(allServersForm, h, i);

                                        return true;
                                    }
                                    else
                                    {
                                        if (allServersForm.arrayBasedDrDrillRadioButton.Checked == true || h.cluster == "yes")
                                        {
                                            h.targetDataStore = allServersForm.drDrillDatastroeDataGridView.Rows[index].Cells[DATASTORE_COLUMN].Value.ToString();
                                        }
                                        else
                                        {
                                            allServersForm.drDrillDatastroeDataGridView.Rows[index].Cells[DATASTORE_COLUMN].Value = null;

                                            // allServersForm.dataStoreDataGridView.Rows[index].Cells[DATASOURCE_SELECTED_COLUMN].Value = false;
                                            allServersForm.drDrillDatastroeDataGridView.RefreshEdit();
                                            MessageBox.Show("Selected datastore does not have enough space ", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);

                                            return false;
                                        }
                                    }
                                }
                            }

                        }

                    }
                    else
                    {
                        if (h.targetDataStore != allServersForm.drDrillDatastroeDataGridView.Rows[index].Cells[DATASTORE_COLUMN].Value.ToString())
                        {
                            for (int i = 0; i < allServersForm.targetDatastroeDataGridView.RowCount; i++)
                            {
                                if (h.targetDataStore == allServersForm.targetDatastroeDataGridView.Rows[i].Cells[0].Value.ToString())
                                {
                                    UnSelectDataStoreCalCulation(allServersForm, h, i);
                                    h.targetDataStore = null;
                                }

                            }



                        }
                        
                            if (allServersForm.drDrillDatastroeDataGridView.Rows[index].Cells[DATASTORE_COLUMN].Value != null)
                            {
                                for (int i = 0; i < allServersForm.targetDatastroeDataGridView.RowCount; i++)
                                {
                                    if (allServersForm.targetDatastroeDataGridView.Rows[i].Cells[0].Value.ToString() == allServersForm.drDrillDatastroeDataGridView.Rows[index].Cells[DATASTORE_COLUMN].Value.ToString())
                                    {

                                        if (Convert.ToInt32(allServersForm.targetDatastroeDataGridView.Rows[i].Cells[3].Value) > h.totalSizeOfDisks)
                                        {
                                            Trace.WriteLine("Printing total size of the disk " + h.targetDataStore + h.totalSizeOfDisks + " " + allServersForm.targetDatastroeDataGridView.Rows[i].Cells[3].Value.ToString());

                                            //h.datastore = allServersForm.dataStoreDataGridView.Rows[rowIndex].Cells[DATASTORE_NAME_COLUMN].Value.ToString();

                                            h.targetDataStore = allServersForm.drDrillDatastroeDataGridView.Rows[index].Cells[DATASTORE_COLUMN].Value.ToString();
                                            SelectedDataStore(allServersForm, h, i);

                                            return true;
                                        }
                                        else
                                        {
                                            if (allServersForm.arrayBasedDrDrillRadioButton.Checked == true || h.cluster == "yes")
                                            {
                                                h.targetDataStore = allServersForm.drDrillDatastroeDataGridView.Rows[index].Cells[DATASTORE_COLUMN].Value.ToString();
                                            }
                                            else
                                            {
                                                allServersForm.drDrillDatastroeDataGridView.Rows[index].Cells[DATASTORE_COLUMN].Value = null;
                                                // allServersForm.dataStoreDataGridView.Rows[index].Cells[DATASOURCE_SELECTED_COLUMN].Value = false;
                                                allServersForm.drDrillDatastroeDataGridView.RefreshEdit();
                                                MessageBox.Show("Selected datastore does not have enough space ", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);

                                                return false;
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
                Trace.WriteLine(DateTime.Now + "\t Exiting: EsxDataStoreSelectedDataStore() in DrDrillPanelHandler");

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
         * Here we will check for the free space available in datastroe and then assin value for the oarticular host 
         * and we will reomove the space from the available datastore.
         */
        internal bool SelectedDataStore(AllServersForm allServersForm, Host h, int index)
        {
            try
            {
                // This method will be called after all the error checks are done for the pariticular machines...
                // Error checks are like is there enough space in the particular datastore or not and 
                // Did they select any datastore previously.....
                // We will remove that space from the datastore free space.....

                Trace.WriteLine(DateTime.Now + "\t Entered: SelectedDataStore() in DrDrillPanelHandler");
                float a = Convert.ToInt32(allServersForm.targetDatastroeDataGridView.Rows[index].Cells[3].Value);
                //Host h1 = (Host)allServersForm._selectedMasterList._hostList[0];
                Host masterHostofTarget = (Host)allServersForm.esxSelected.GetHostList[0];
                foreach (DataStore d in masterHostofTarget.dataStoreList)
                {
                    if (d.vSpherehostname == allServersForm.masterHostAdded.vSpherehost)
                    {
                        if (d.name == allServersForm.targetDatastroeDataGridView.Rows[index].Cells[0].Value.ToString())
                        {
                            d.freeSpace = a - h.totalSizeOfDisks;

                        }
                    }
                }


                Debug.WriteLine("entered to calculate the datastore sizes " + h.totalSizeOfDisks + a + "   " + (a - h.totalSizeOfDisks));

                allServersForm.targetDatastroeDataGridView.Rows[index].Cells[3].Value = a - h.totalSizeOfDisks;
                allServersForm.targetDatastroeDataGridView.Rows[index].Cells[2].Value = h.totalSizeOfDisks + Convert.ToInt32(allServersForm.targetDatastroeDataGridView.Rows[index].Cells[2].Value);
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
            Trace.WriteLine(DateTime.Now + "\t Exiting: SelectedDataStore() in DrDrillPanelHandler");
            return true;
        }

        /*
         * If user selects another datastore after selecting one datastroe
         * There we will call this method. This method is  going to add free space to the old datstroe 
         * seelcted by user earlier.
         */
        internal bool UnSelectDataStoreCalCulation(AllServersForm allServersForm, Host h, int index)
        {

            // Unslect datastore meand when user trying ot select another datastore we check whether it is already having datastore selected 
            // If so we will add this space to the previously selected datastore available space.....
            Trace.WriteLine(DateTime.Now + "\t Entered: UnSelectDataStoreCalCulation() in DrDrillPanelHandler");
            try
            {
                float a = Convert.ToInt32(allServersForm.targetDatastroeDataGridView.Rows[index].Cells[3].Value);
                Host masterHostofTarget = (Host)allServersForm.esxSelected.GetHostList[0];
                foreach (DataStore d in masterHostofTarget.dataStoreList)
                {
                    if (d.name == allServersForm.targetDatastroeDataGridView.Rows[index].Cells[0].Value.ToString())
                    {
                        d.freeSpace = a + h.totalSizeOfDisks;

                    }
                }

                Debug.WriteLine("entered to calculate the datastore sizes " + h.totalSizeOfDisks + a + "   " + (a - h.totalSizeOfDisks));

                allServersForm.targetDatastroeDataGridView.Rows[index].Cells[3].Value = a + h.totalSizeOfDisks;
                allServersForm.targetDatastroeDataGridView.Rows[index].Cells[2].Value = Convert.ToInt32(allServersForm.targetDatastroeDataGridView.Rows[index].Cells[2].Value) - h.totalSizeOfDisks;

                Trace.WriteLine(DateTime.Now + "\t Exiting: UnSelectDataStoreCalCulation() in DrDrillPanelHandler");

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
         * If user selecting datastore for the first time in dr drill option after loading wizard
         * we will select that datastroe all te vms. defaultly for the first time.
         */
        internal bool SelectDataStoreForFirstTime(AllServersForm allServersForm, int index)
        {
            try
            {
                // when user selects datastore foe the first time 
                //We will select that datastore for all vms if free space is available on the datastore.....
                if (_firstTimeSelection == true)
                {
                    _firstTimeSelection = false;
                    for (int i = 0; i < allServersForm.drDrillDatastroeDataGridView.RowCount; i++)
                    {
                        allServersForm.drDrillDatastroeDataGridView.Rows[i].Cells[DATASTORE_COLUMN].Value = allServersForm.drDrillDatastroeDataGridView.Rows[index].Cells[DATASTORE_COLUMN].Value.ToString();
                    }
                    Trace.WriteLine(DateTime.Now + "\t Entered: SelectDataStoreForFirstTime() in DrDrillPanelHandler");

                    foreach (Host h in allServersForm.recoveryChecksPassedList._hostList)
                    {
                        for (int i = 0; i < allServersForm.targetDatastroeDataGridView.RowCount; i++)
                        {
                            if (allServersForm.targetDatastroeDataGridView.Rows[i].Cells[0].Value.ToString() == allServersForm.drDrillDatastroeDataGridView.Rows[index].Cells[DATASTORE_COLUMN].Value.ToString())
                            {
                                Trace.WriteLine(DateTime.Now + "\t Printing the datastore " + h.targetDataStore);
                                if (h.targetDataStore == null)
                                {
                                    int a = Convert.ToInt32(allServersForm.targetDatastroeDataGridView.Rows[i].Cells[3].Value);
                                    Trace.WriteLine("entered to calculate the datastore sizes " + h.totalSizeOfDisks + a + "   " + (a - h.totalSizeOfDisks));
                                    //MessageBox.Show("Printing a "+a.ToString() + "usedspace"+ allServersForm.dataStoreDataGridView.Rows[i].Cells[DATASTORE_USEDSPACE_COLUMN].Value.ToString());
                                    if (a - h.totalSizeOfDisks >= 0)
                                    {
                                        allServersForm.targetDatastroeDataGridView.Rows[i].Cells[3].Value = a - h.totalSizeOfDisks;

                                        // MessageBox.Show("value is "+ h.totalSizeOfDisks + Convert.ToInt32(allServersForm.dataStoreDataGridView.Rows[i].Cells[DATASTORE_USEDSPACE_COLUMN].Value));
                                        allServersForm.targetDatastroeDataGridView.Rows[i].Cells[2].Value = h.totalSizeOfDisks + Convert.ToInt32(allServersForm.targetDatastroeDataGridView.Rows[i].Cells[2].Value);
                                        allServersForm.targetDatastroeDataGridView.RefreshEdit();
                                        h.targetDataStore = allServersForm.drDrillDatastroeDataGridView.Rows[index].Cells[DATASTORE_COLUMN].Value.ToString();
                                        Host h1 = (Host)allServersForm.esxSelected.GetHostList[0];
                                        foreach (DataStore d in h1.dataStoreList)
                                        {
                                            if (d.name == h.targetDataStore)
                                            {
                                                d.freeSpace = float.Parse(allServersForm.targetDatastroeDataGridView.Rows[i].Cells[3].Value.ToString());

                                            }
                                        }
                                    }
                                    else
                                    {
                                        if (allServersForm.arrayBasedDrDrillRadioButton.Checked == true)
                                        {
                                            allServersForm.targetDatastroeDataGridView.Rows[i].Cells[2].Value = h.totalSizeOfDisks + Convert.ToInt32(allServersForm.targetDatastroeDataGridView.Rows[i].Cells[2].Value);
                                            allServersForm.targetDatastroeDataGridView.RefreshEdit();
                                            h.targetDataStore = allServersForm.drDrillDatastroeDataGridView.Rows[index].Cells[DATASTORE_COLUMN].Value.ToString();
                                        }
                                        else
                                        {
                                            if (h.cluster == "no")
                                            {
                                                for (int j = 0; j < allServersForm.drDrillDatastroeDataGridView.RowCount; j++)
                                                {

                                                    if (h.displayname == allServersForm.drDrillDatastroeDataGridView.Rows[j].Cells[SOURCE_DISPLAYNAME_COLUMN].Value.ToString())
                                                    {
                                                        Trace.WriteLine(DateTime.Now + "\t Printing index values " + i.ToString());
                                                        allServersForm.drDrillDatastroeDataGridView.Rows[j].Cells[DATASTORE_COLUMN].Value = null;
                                                    }
                                                }
                                                //MessageBox.Show("The selected datastore does not have enough space to export machine(s)", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                                allServersForm.drDrillDatastroeDataGridView.RefreshEdit();
                                            }
                                            // return false;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    Trace.WriteLine(DateTime.Now + "\t Exiting: SelectDataStoreForFirstTime() in DrDrillPanelHandler");
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: SelectDataStoreForFirstTime DrDrillPanelHandler " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            return true;
        }


        internal bool SelectRDMLun(AllServersForm allServersForm, int index)
        {
            if (allServersForm.drDrillDatastroeDataGridView.Rows[index].Cells[SELECT_LUNS_COLUMN].Value.ToString() == "Select luns" || allServersForm.drDrillDatastroeDataGridView.Rows[index].Cells[SELECT_LUNS_COLUMN].Value.ToString() == "Available")
            {
                Host h = new Host();
                h.displayname = allServersForm.drDrillDatastroeDataGridView.Rows[index].Cells[SOURCE_DISPLAYNAME_COLUMN].Value.ToString();
                h.hostname = allServersForm.drDrillDatastroeDataGridView.Rows[index].Cells[HOST_NAME_COLUMN].Value.ToString();
                h.new_displayname = allServersForm.drDrillDatastroeDataGridView.Rows[index].Cells[TARGET_DISPLAY_NAME_COLUMN].Value.ToString();
                foreach (Host selectedHost in allServersForm.recoveryChecksPassedList._hostList)
                {
                    if (selectedHost.hostname == h.hostname && selectedHost.displayname == h.displayname && selectedHost.new_displayname == h.new_displayname)
                    {
                        h = selectedHost;
                    }
                }


                EsxRdmSelectionForm rdm = new EsxRdmSelectionForm(h, masterList);
                rdm.ShowDialog();
                if (rdm.canceled == false)
                {
                    allServersForm.selectedMasterListbyUser = rdm.masterSelectedHostList;

                    foreach (Host h1 in allServersForm.recoveryChecksPassedList._hostList)
                    {
                        if (h1.displayname == rdm.lunsSelectedHost.displayname)
                        {
                            h1.disks = rdm.lunsSelectedHost.disks;
                            allServersForm.drDrillDatastroeDataGridView.Rows[index].Cells[SELECT_LUNS_COLUMN].Value = "Available";
                        }
                    }
                }
            }
            return true;
        }

    }
}
