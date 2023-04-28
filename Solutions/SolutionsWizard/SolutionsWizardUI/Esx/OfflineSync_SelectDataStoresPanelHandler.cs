using System;
using System.Collections.Generic;
using System.Text;
using System.Windows.Forms;
using System.Xml;
using com.InMage.ESX;
using com.InMage.Tools;


namespace com.InMage.Wizard
{
    class OfflineSync_SelectDataStoresPanelHandler : OfflineSyncPanelHandler
    {
        public OfflineSync_SelectDataStoresPanelHandler()
        {

        }

        public override bool Initialize(ImportOfflineSyncForm importOfflineSyncForm)
        {
            importOfflineSyncForm.targetDataStoreComboBox.Items.Clear();
            importOfflineSyncForm.sourceDataStoreComboBox.Items.Clear();
            int rowIndex = 0;

            Host h1 = (Host)importOfflineSyncForm._esx.getHostList()[0];
            importOfflineSyncForm.masterTargetTextBox.Text = importOfflineSyncForm.masterHost.displayName;
            foreach (DataStore d in h1.dataStoreList)
            {
                importOfflineSyncForm.targetDataStoreComboBox.Items.Add(d.name);
                importOfflineSyncForm.sourceDataStoreComboBox.Items.Add(d.name);

            }

            importOfflineSyncForm.sourceVmsDataGridView.Rows.Clear();
            foreach (Host h in importOfflineSyncForm._selectedSourceList._hostList)
            {
                importOfflineSyncForm.sourceVmsDataGridView.Rows.Add(1);
                //importOfflineSyncForm.sourceVmsDataGridView.Rows[rowIndex].Cells[0].Value = h.esxIp;
                importOfflineSyncForm.sourceVmsDataGridView.Rows[rowIndex].Cells[0].Value = h.ip;
                importOfflineSyncForm.sourceVmsDataGridView.Rows[rowIndex].Cells[1].Value = h.displayName;
                rowIndex++;
            }

            return true;
        }

        public override bool ProcessPanel(ImportOfflineSyncForm importOfflineSyncForm)
        {
           

            
            if (importOfflineSyncForm.targetDataStoreComboBox.Text.Length == 0)
            {
                MessageBox.Show("Please select a data store where master target is copied to", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return false;

            }
            else
            {
                importOfflineSyncForm.targetDataStore = importOfflineSyncForm.targetDataStoreComboBox.SelectedItem.ToString();

            }
            if (importOfflineSyncForm.sourceDataStoreComboBox.Text.Length == 0)
            {
                MessageBox.Show("Please select the datastore where primary VMS are copied to", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return false;
            }
            else
            {
                importOfflineSyncForm.vmdkDataStore = importOfflineSyncForm.sourceDataStoreComboBox.SelectedItem.ToString();

            }
            importOfflineSyncForm._cxIP= WinTools.getCxIp();
            string planName = "";


            foreach (Host h in importOfflineSyncForm._selectedSourceList._hostList)
            {
                h.targetDataStore = importOfflineSyncForm.targetDataStore;
            }

            importOfflineSyncForm.masterHost.datastore = importOfflineSyncForm.targetDataStore;
            string s = importOfflineSyncForm.masterHost.vmx_path.Replace( importOfflineSyncForm._tempDataStore ,   importOfflineSyncForm.targetDataStore );
          
            importOfflineSyncForm.masterHost.vmx_path = s;
            importOfflineSyncForm._finalMasterList.AddOrReplaceHost(importOfflineSyncForm.masterHost);

            SolutionConfig config = new SolutionConfig();
            config.WriteXmlFile(importOfflineSyncForm._selectedSourceList, importOfflineSyncForm._finalMasterList,importOfflineSyncForm._esx, importOfflineSyncForm._cxIP, "ESX.xml", planName,false);

          
            return true;
        }
        public override bool ValidatePanel(ImportOfflineSyncForm importOfflineSyncForm)
        {
            
            return true;
        }
        public override bool CanGoToNextPanel(ImportOfflineSyncForm importOfflineSyncForm)
        {
            return true;

        }

        public override bool CanGoToPreviousPanel(ImportOfflineSyncForm importOfflineSyncForm)
        {
            importOfflineSyncForm.previousButton.Visible = false;

            return true;

        }
    }
}
