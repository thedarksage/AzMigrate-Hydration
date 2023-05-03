using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;
using System.Diagnostics;
using System.Collections;
using Com.Inmage.Esxcalls;

namespace Com.Inmage.Wizard
{
    public partial class EsxRdmSelectionForm : Form
    {
        internal static int DiskNameColumn = 0;
        internal static int DiskSizeColumn = 1;
        internal static int SelectLunColumn = 2;
       // public static int LUN_SIZE_COLUMN = 3;
        internal bool canceled = false;
        internal HostList masterSelectedHostList = new HostList();
        internal Host lunsSelectedHost = new Host();
        internal string targetEsxIP = null;
        internal string tgtesxUserName = null;
        internal string tgtesxPassword = null;
        Esx _esx = new Esx();
     
        public EsxRdmSelectionForm(Host sourceHost, HostList masterList)
        {
            try
            {
                lunsSelectedHost = sourceHost;
                masterSelectedHostList = masterList;
                InitializeComponent();

                

                foreach (Host masterHost in masterList._hostList)
                {
                    targetEsxIP = masterHost.esxIp;
                    tgtesxUserName = masterHost.esxUserName;
                    tgtesxPassword = masterHost.esxPassword;
                }
                Host targetHost = (Host)masterList._hostList[0];
                if (targetHost.lunList.Count == 0)
                {
                    rdmLunDetailsDataGridView.Visible = false;
                    dispalyNameLabel.Visible = false;
                    rdmNoteLabel.Visible = false;
                    addButton.Enabled = false;
                    cancelButton.Enabled = false;
                    loadPictureBox.Visible = true;
                    backgroundWorker.RunWorkerAsync();
                }
                else
                {
                    ArrayList physicalDisks;
                    int i = 0;
                    dispalyNameLabel.Text = "Select luns for " + lunsSelectedHost.displayname;

                    rdmLunDetailsDataGridView.Rows.Clear();
                    physicalDisks = lunsSelectedHost.disks.GetDiskList;
                    if (lunsSelectedHost.rdmpDisk == "TRUE")
                    {
                        foreach (Hashtable disk in physicalDisks)
                        {
                            if (disk["Selected"].ToString() == "Yes" && disk["Rdm"].ToString() == "yes")
                            {
                                rdmLunDetailsDataGridView.Rows.Add(1);
                                rdmLunDetailsDataGridView.Rows[i].Cells[DiskNameColumn].Value = disk["Name"].ToString();
                                rdmLunDetailsDataGridView.Rows[i].Cells[DiskSizeColumn].Value = disk["Size"].ToString();
                                i++;
                            }
                        }
                        Host h1 = (Host)masterSelectedHostList._hostList[0];
                        foreach (RdmLuns luns in h1.lunList)
                        {
                            bool rdmCanbeAdded = false;
                            foreach (Hashtable diskCamparision in physicalDisks)
                            {
                                if (diskCamparision["Size"] != null)
                                {
                                    if (h1.vSpherehost == luns.vSpherehostname)
                                    {
                                        float size = float.Parse(diskCamparision["Size"].ToString());
                                        if (luns.capacity_in_kb >= size && luns.alreadyUsed == false && diskCamparision["Selected"].ToString() == "Yes")
                                        {
                                            rdmCanbeAdded = true;
                                        }
                                    }
                                }
                            }
                            if (rdmCanbeAdded == true)
                            {
                                selectLunCombobox.Items.Add(luns.name + ":" + "  Size: " + luns.capacity_in_kb + ":" + "  Lun id: " + luns.lun + ":" + "  Adapter: " + luns.adapter);
                                Trace.WriteLine(DateTime.Now + " \t Printing the added value in the combobox " + luns.name);
                               
                            }
                        }
                    }
                    for (int j = 0; j < rdmLunDetailsDataGridView.RowCount; j++)
                    {
                        foreach (Hashtable disk in physicalDisks)
                        {
                            if (rdmLunDetailsDataGridView.Rows[j].Cells[DiskNameColumn].Value.ToString() == disk["Name"].ToString())
                            {
                                if (disk["lun_name"] != null)
                                {
                                    selectLunCombobox.Items.Add(disk["lun_name"].ToString() + ":" + "    Size: " + disk["capacity_in_kb"].ToString() + ":" + "   Lun id:  " + disk["lun"].ToString() + ":" + "  Adapter: " + disk["adapter"].ToString());
                                    rdmLunDetailsDataGridView.Rows[j].Cells[SelectLunColumn].Value = disk["lun_name"].ToString() + ":" + "    Size: " + disk["capacity_in_kb"].ToString() + ":" + "   Lun id:  " + disk["lun"].ToString() + ":" + "  Adapter: " + disk["adapter"].ToString();
                                    // rdmLunDetailsDataGridView.Rows[j].Cells[LUN_SIZE_COLUMN].Value = disk["capacity_in_kb"].ToString();
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

                
            }



        }

        private void cancelButton_Click(object sender, EventArgs e)
        {
            canceled = true;
            Close();
        }

        private void rdmLunDetailsDataGridView_CurrentCellDirtyStateChanged(object sender, EventArgs e)
        {
            if (rdmLunDetailsDataGridView.RowCount > 0)
            {

                rdmLunDetailsDataGridView.CommitEdit(DataGridViewDataErrorContexts.Commit);
            }
        }

        private void rdmLunDetailsDataGridView_CellValueChanged(object sender, DataGridViewCellEventArgs e)
        {
            try
            {
                if (e.RowIndex >= 0)
                {
                    if (rdmLunDetailsDataGridView.RowCount > 0)
                    {
                        if (rdmLunDetailsDataGridView.Rows[e.RowIndex].Cells[SelectLunColumn].Selected == true && rdmLunDetailsDataGridView.Rows[e.RowIndex].Cells[SelectLunColumn].Value != null)
                        {

                            if (Validate(e.RowIndex) == true)
                            {

                                foreach (Host h in masterSelectedHostList._hostList)
                                {
                                    foreach (RdmLuns luns in h.lunList)
                                    {
                                        if (rdmLunDetailsDataGridView.Rows[e.RowIndex].Cells[SelectLunColumn].Value != null)
                                        {
                                            if (rdmLunDetailsDataGridView.Rows[e.RowIndex].Cells[SelectLunColumn].Value.ToString().Contains(luns.name))
                                            {
                                                if (float.Parse(rdmLunDetailsDataGridView.Rows[e.RowIndex].Cells[DiskSizeColumn].Value.ToString()) <= luns.capacity_in_kb)
                                                {

                                                    // rdmLunDetailsDataGridView.Rows[e.RowIndex].Cells[LUN_SIZE_COLUMN].Value = luns.capacity_in_kb;
                                                }
                                                else
                                                {
                                                    MessageBox.Show("The size of target LUN should be bigger than the size of source RDM disk", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);

                                                    rdmLunDetailsDataGridView.Rows[e.RowIndex].Cells[SelectLunColumn].Value = null;
                                                    //rdmLunDetailsDataGridView.Rows[e.RowIndex].Cells[LUN_SIZE_COLUMN].Value = null;
                                                    rdmLunDetailsDataGridView.RefreshEdit();
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
            catch (Exception ex)
            {              
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }










        private void addButton_Click(object sender, EventArgs e)
        {
            try
            {
                if (AddClickValidate() == true)
                {
                    Host h1 = (Host)masterSelectedHostList._hostList[0];

                    foreach (Hashtable disk in lunsSelectedHost.disks.GetDiskList)
                    {
                        foreach (RdmLuns lun in h1.lunList)
                        {
                            if (disk["lun"] == lun.lun)
                            {
                                lun.alreadyUsed = false;

                            }
                        }
                    }


                    for (int i = 0; i < rdmLunDetailsDataGridView.RowCount; i++)
                    {
                        foreach (Hashtable disk in lunsSelectedHost.disks.GetDiskList)
                        {
                            if (rdmLunDetailsDataGridView.Rows[i].Cells[SelectLunColumn].Value != null)
                            {
                                if (disk["Name"].ToString() == rdmLunDetailsDataGridView.Rows[i].Cells[DiskNameColumn].Value.ToString())
                                {
                                    string[] words = rdmLunDetailsDataGridView.Rows[i].Cells[SelectLunColumn].Value.ToString().Split(':');






                                    foreach (RdmLuns luns in h1.lunList)
                                    {
                                        foreach (string word in words)
                                        {

                                            if (luns.name == word)
                                            {

                                                disk["lun_name"] = word;
                                                luns.alreadyUsed = true;
                                                disk["adapter"] = luns.adapter;
                                                disk["lun"] = luns.lun;
                                                disk["devicename"] = luns.devicename;
                                                disk["capacity_in_kb"] = luns.capacity_in_kb;
                                                if (lunsSelectedHost.machineType == "PhysicalMachine")
                                                {
                                                    disk["disk_type"] = "physicalMode";
                                                }
                                                disk["disk_mode"] = "Mapped Raw LUN";
                                                lunsSelectedHost.rdmSelected = true;
                                                lunsSelectedHost.convertRdmpDiskToVmdk = false;
                                            }
                                        }
                                    }
                                    /* disk["lun_name"] = rdmLunDetailsDataGridView.Rows[i].Cells[SELECT_LUN_COLUMN].Value.ToString();
                               
                                     foreach (RdmLuns luns in h1.lunList)
                                     {

                                         if (disk["lun_name"].ToString() == luns.name)
                                         {
                                             luns.alreadyUsed = true;
                                             disk["adapter"] = luns.adapter;
                                             disk["lun"] = luns.lun;
                                             disk["devicename"] = luns.devicename;
                                             disk["capacity_in_kb"] = luns.capacity_in_kb;
                                             _lunSelectedHost.rdmSelected = true; 
                                       
                                         }

                                   

                                     }*/
                                }


                            }
                        }
                    }
                    Close();




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


        private bool Validate(int index)
        {
            try
            {

                for (int i = 0; i < rdmLunDetailsDataGridView.RowCount; i++)
                {
                    if (i != index)
                    {
                        if (rdmLunDetailsDataGridView.Rows[i].Cells[SelectLunColumn].Value != null)
                        {

                            if (rdmLunDetailsDataGridView.Rows[i].Cells[SelectLunColumn].Value.ToString() == rdmLunDetailsDataGridView.Rows[index].Cells[SelectLunColumn].Value.ToString())
                            {
                                MessageBox.Show("This lun is already in use", "RDM in use", MessageBoxButtons.OK, MessageBoxIcon.Information);

                                rdmLunDetailsDataGridView.Rows[index].Cells[SelectLunColumn].Value = null;
                                rdmLunDetailsDataGridView.RefreshEdit();
                                return false;



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
        private bool AddClickValidate()
        {
            try
            {
                bool emptyCell = false;
                int index = 0;
                for (int i = 0; i < rdmLunDetailsDataGridView.RowCount; i++)
                {
                    if (rdmLunDetailsDataGridView.Rows[i].Cells[SelectLunColumn].Value == null)
                    {
                        emptyCell = true;

                        index++;

                    }
                }
                if (emptyCell == true && lunsSelectedHost.machineType.ToUpper() == "VIRTUALMACHINE")
                {
                    MessageBox.Show("Please select LUNs for all the disks", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return false;
                }
                if (index == rdmLunDetailsDataGridView.RowCount && lunsSelectedHost.machineType.ToUpper() == "PHYSICALMACHINE")
                {
                    MessageBox.Show("Select lun for at least one disk", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return false;
                }

                for (int i = 0; i < (rdmLunDetailsDataGridView.RowCount) - 1; i++)
                {
                    for (int j = i + 1; j < rdmLunDetailsDataGridView.RowCount; j++)
                    {
                        if (rdmLunDetailsDataGridView.Rows[i].Cells[SelectLunColumn].Value != null)
                        {
                            if (rdmLunDetailsDataGridView.Rows[j].Cells[SelectLunColumn].Value != null)
                            {
                                if (rdmLunDetailsDataGridView.Rows[i].Cells[SelectLunColumn].Value.ToString() == rdmLunDetailsDataGridView.Rows[j].Cells[SelectLunColumn].Value.ToString())
                                {
                                    MessageBox.Show("You have selected same LUN for two disks. Please choose different LUNs ", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                    rdmLunDetailsDataGridView.Rows[j].Cells[SelectLunColumn].Value = null;
                                    rdmLunDetailsDataGridView.RefreshEdit();


                                    return false;
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

        private void backgroundWorker_DoWork(object sender, DoWorkEventArgs e)
        {
          
            Cxapicalls cxAPi = new Cxapicalls();
            cxAPi.GetHostCredentials(targetEsxIP);
            if (Cxapicalls.GetUsername != null && Cxapicalls.GetPassword != null)
            {
                Trace.WriteLine(DateTime.Now + "\t CX has target esx creds ");
            }
            else
            {
                if (cxAPi. UpdaterCredentialsToCX(targetEsxIP,tgtesxUserName,tgtesxPassword) == true)
                {
                    Trace.WriteLine(DateTime.Now + "\t Updated creds to cx");
                    cxAPi.GetHostCredentials(targetEsxIP);
                    if (Cxapicalls.GetUsername != null && Cxapicalls.GetPassword != null)
                    {
                        Trace.WriteLine(DateTime.Now + "\t CX has target esx creds ");
                    }
                    else
                    {
                        Trace.WriteLine(DateTime.Now + "\t CX don't have esx creds");
                        return;
                    }
                }

            }
            _esx.GetOnlyLuns(targetEsxIP, "luns");
            if (_esx.lunList.Count == 0)
            {
                MessageBox.Show("Failed to get Luns form Secondary server", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void backgroundWorker_RunWorkerCompleted(object sender, RunWorkerCompletedEventArgs e)
        {
            loadPictureBox.Visible = false;
            dispalyNameLabel.Visible = true;
            rdmNoteLabel.Visible = true;
            addButton.Enabled = true;
            cancelButton.Enabled = true;
            if (_esx.lunList.Count != 0)
            {
                rdmLunDetailsDataGridView.Visible = true;
                foreach (Host h in masterSelectedHostList._hostList)
                {
                    h.lunList = _esx.lunList;
                }
            }
            else
            {
                return;
            }
            ArrayList physicalDisks;
            int i = 0;
            dispalyNameLabel.Text = "Select luns for " + lunsSelectedHost.displayname;

            rdmLunDetailsDataGridView.Rows.Clear();
            physicalDisks = lunsSelectedHost.disks.GetDiskList;
            if (lunsSelectedHost.rdmpDisk == "TRUE")
            {
                foreach (Hashtable disk in physicalDisks)
                {
                    if (disk["Selected"].ToString() == "Yes" && disk["Rdm"].ToString() == "yes")
                    {
                        rdmLunDetailsDataGridView.Rows.Add(1);
                        rdmLunDetailsDataGridView.Rows[i].Cells[DiskNameColumn].Value = disk["Name"].ToString();
                        rdmLunDetailsDataGridView.Rows[i].Cells[DiskSizeColumn].Value = disk["Size"].ToString();
                        i++;
                    }
                }
                Host h1 = (Host)masterSelectedHostList._hostList[0];
                foreach (RdmLuns luns in h1.lunList)
                {
                    bool rdmCanbeAdded = false;
                    foreach (Hashtable diskCamparision in physicalDisks)
                    {
                        if (diskCamparision["Size"] != null)
                        {
                            if (h1.vSpherehost == luns.vSpherehostname)
                            {
                                float size = float.Parse(diskCamparision["Size"].ToString());
                                if (luns.capacity_in_kb >= size && luns.alreadyUsed == false && diskCamparision["Selected"].ToString() == "Yes")
                                {
                                    rdmCanbeAdded = true;
                                }
                            }
                        }
                    }
                    if (rdmCanbeAdded == true)
                    {
                        selectLunCombobox.Items.Add(luns.name + ":" + "  Size: " + luns.capacity_in_kb + ":" + "  Lun id: " + luns.lun + ":" + "  Adapter: " + luns.adapter);
                        Trace.WriteLine(DateTime.Now + " \t Printing the added value in the combobox " + luns.name);
                    }
                }
            }
            for (int j = 0; j < rdmLunDetailsDataGridView.RowCount; j++)
            {
                foreach (Hashtable disk in physicalDisks)
                {
                    if (rdmLunDetailsDataGridView.Rows[j].Cells[DiskNameColumn].Value.ToString() == disk["Name"].ToString())
                    {
                        if (disk["lun_name"] != null)
                        {
                            selectLunCombobox.Items.Add(disk["lun_name"].ToString() + ":" + "    Size: " + disk["capacity_in_kb"].ToString() + ":" + "   Lun id:  " + disk["lun"].ToString() + ":" + "  Adapter: " + disk["adapter"].ToString());
                            rdmLunDetailsDataGridView.Rows[j].Cells[SelectLunColumn].Value = disk["lun_name"].ToString() + ":" + "    Size: " + disk["capacity_in_kb"].ToString() + ":" + "   Lun id:  " + disk["lun"].ToString() + ":" + "  Adapter: " + disk["adapter"].ToString();
                            // rdmLunDetailsDataGridView.Rows[j].Cells[LUN_SIZE_COLUMN].Value = disk["capacity_in_kb"].ToString();
                        }
                    }
                }
            }
        }

    }
}
