using System;
using System.Collections.Generic;
using System.Text;
using System.Windows.Forms;
using System.Xml;
using com.InMage.ESX;
using com.InMage.Tools;


namespace com.InMage.Wizard
{
    class SelectDataStoresPanelHandler : OfflineSyncPanelHandler
    {
        public SelectDataStoresPanelHandler()
        {

        }

        public override bool Initialize(ImportOfflineSyncForm importOfflineSyncForm)
        {
            int rowIndex = 0;


            importOfflineSyncForm.masterTargetTextBox.Text = importOfflineSyncForm.masterHost.displayName;
            foreach (DataStore d in importOfflineSyncForm.masterHost.dataStoreList)
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
                MessageBox.Show("Please select the data store for the target");
                return false;

            }
            else
            {
                importOfflineSyncForm.targetDataStore = importOfflineSyncForm.targetDataStoreComboBox.SelectedItem.ToString();

            }
            if (importOfflineSyncForm.sourceDataStoreComboBox.Text.Length == 0)
            {
                MessageBox.Show("Please select the datastore for the source vmdks");
                return false;
            }
            else
            {
                importOfflineSyncForm.vmdkDataStore = importOfflineSyncForm.sourceDataStoreComboBox.SelectedItem.ToString();

            }
            importOfflineSyncForm._cxIP= WinTools.getCxIp();
            string planName = "";

            importOfflineSyncForm._finalMasterList.AddOrReplaceHost(importOfflineSyncForm.masterHost);

            SolutionConfig config = new SolutionConfig();
            config.WriteXmlFile(importOfflineSyncForm._selectedSourceList, importOfflineSyncForm._finalMasterList,importOfflineSyncForm._esx, importOfflineSyncForm._cxIP, "ESX.xml", planName);

          
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
