using System;
using System.Collections.Generic;
using System.Collections;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;
using System.Diagnostics;
using System.Threading;

namespace Com.Inmage.Wizard
{
    public partial class RemoveDuplicateEntries : Form
    {
        public bool canceledForm = false;
        Host selectedHost = new Host();
       public bool unregisteredHost = false;
       public bool skipUnregisterHost = false;
        int selectedIndex = 0;
        public RemoveDuplicateEntries(HostList list)
        {

            
           
            InitializeComponent();
            removeDuplicateEntriesDataGridView.Rows.Clear();
            int index = 0;
            foreach (Host h in list._hostList)
            {
                removeDuplicateEntriesDataGridView.Rows.Add(1);
                removeDuplicateEntriesDataGridView.Rows[index].Cells[0].Value = h.hostname;
                removeDuplicateEntriesDataGridView.Rows[index].Cells[1].Value = h.ip;
                string ipAddress = null;
                if (h.IPlist != null)
                {
                    foreach (string s in h.IPlist)
                    {
                        if (ipAddress == null)
                        {
                            ipAddress = s;
                        }
                        else
                        {
                            ipAddress = ipAddress + "," + s;
                        }
                             
                    }
                }
                removeDuplicateEntriesDataGridView.Rows[index].Cells[2].Value = ipAddress;
                removeDuplicateEntriesDataGridView.Rows[index].Cells[3].Value = h.inmage_hostid;
                removeDuplicateEntriesDataGridView.Rows[index].Cells[4].Value = h.agentFirstReportedTime;
                removeDuplicateEntriesDataGridView.Rows[index].Cells[5].Value = "Remove";
                index++;
            }
        }

        private void cancelButton_Click(object sender, EventArgs e)
        {
            if(cancelButton.Text == "Cancel")
            {
                canceledForm = true;
            }
            Close();
        }

        private void removeDuplicateEntriesDataGridView_CellContentClick(object sender, DataGridViewCellEventArgs e)
        {
            if(e.RowIndex >=0)
            {
                if(removeDuplicateEntriesDataGridView.Rows[e.RowIndex].Cells[5].Selected == true)
                {
                    selectedHost = new Host();
                    if (removeDuplicateEntriesDataGridView.Rows[e.RowIndex].Cells[3].Value != null)
                    {
                        selectedHost.inmage_hostid = removeDuplicateEntriesDataGridView.Rows[e.RowIndex].Cells[3].Value.ToString();
                        //Now we have added this line because when user tried remove for the machine which is in protection. Wizard need to display message with name
                        try
                        {
                            selectedHost.hostname = removeDuplicateEntriesDataGridView.Rows[e.RowIndex].Cells[0].Value.ToString();
                        }
                        catch (Exception ex)
                        {
                            Trace.WriteLine(DateTime.Now + "\t Failed to read hostname " + ex.Message);
                        }
                        selectedIndex = e.RowIndex;
                        cancelButton.Enabled = false;
                        progressBar.Visible = true;
                        backgroundWorker.RunWorkerAsync();
                    }
                }
            }
        }

        private void backgroundWorker_DoWork(object sender, DoWorkEventArgs e)
        {
            Cxapicalls cxApi = new Cxapicalls();
            // Added below code to check if already any pairs exists in CS to the selected server.
            // In case if we found any pairs in CS we will ask show pop to user whether to continue or not.
            try
            {
                Host tempHost = new Host();
                tempHost.inmage_hostid = selectedHost.inmage_hostid;
                Host tempMtHost = new Host();
                tempMtHost.inmage_hostid = null;
                cxApi.PostToGetPairsOFSelectedMachine(tempHost, tempMtHost.inmage_hostid);
                tempHost = Cxapicalls.GetHost;
                skipUnregisterHost = false;
                foreach (Hashtable hash in tempHost.disks.partitionList)
                {

                    if (hash["SourceVolume"] != null && hash["TargetVolume"] != null)
                    {
                        switch (MessageBox.Show("Selected machine: " + selectedHost.hostname + " is under protection now. Do you want to still remove it?", "Remove",
                                      MessageBoxButtons.YesNo,
                                      MessageBoxIcon.Question))
                        {
                            case DialogResult.Yes:
                                Trace.WriteLine(DateTime.Now + "\t User selected to continue with remove");
                                break;
                            case DialogResult.No:
                                skipUnregisterHost = true;
                                return;
                        }
                        break;

                    }
                }
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed to check pair info in CS " + ex.Message);
            }
            Trace.WriteLine(DateTime.Now + "\t Going to unregister host " + selectedHost.displayname);
            if (cxApi.Post(selectedHost, "UnregisterAgent") == true)
            {
                unregisteredHost = true;
                Thread.Sleep(15000);
            }
            else
            {
                unregisteredHost = false;
                Trace.WriteLine(DateTime.Now + "\t Failed to unregister host from CX database");
            }
        }

        private void backgroundWorker_RunWorkerCompleted(object sender, RunWorkerCompletedEventArgs e)
        {
            cancelButton.Enabled = true;
            progressBar.Visible = false;
            if(unregisteredHost == true)
            {
                cancelButton.Text = "Close";
                removeDuplicateEntriesDataGridView.Columns[4].ReadOnly = true;
                removeDuplicateEntriesDataGridView.Rows.RemoveAt(selectedIndex);
                removeDuplicateEntriesDataGridView.RefreshEdit();
            }
            else
            {
                //This skipunregisterhost bool value is added becuase when user select no to contine defualt wizard think that it has failed ans throughs messagebox
                //To avoid that we have added this check 
                if (skipUnregisterHost == false)
                {
                    MessageBox.Show("Failed to unregister host from CX", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                }
            }
        }

        private void backgroundWorker_ProgressChanged(object sender, ProgressChangedEventArgs e)
        {
            progressBar.Value = e.ProgressPercentage;
        }
    }
}
