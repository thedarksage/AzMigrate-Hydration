using System;
using System.Collections.Generic;
using System.Text;
using System.Windows.Forms;
using com.InMage.ESX;

namespace com.InMage.Wizard
{
    class TargetDetailsPanelHandler : OfflineSyncPanelHandler
    {
        public string _esxIP = "";
        public string _esxUserName = "";
        public string _esxPassword = "";
        public TargetDetailsPanelHandler()
        {

        }

        public override bool Initialize(ImportOfflineSyncForm importOfflineSyncForm)
        {
            importOfflineSyncForm.previousButton.Visible = false;
            return true;
        }

        public override bool ProcessPanel(ImportOfflineSyncForm importOfflineSyncForm)
        {
            return true;
        }
        public override bool ValidatePanel(ImportOfflineSyncForm importOfflineSyncForm)
        {
            if (importOfflineSyncForm._selectedSourceList._hostList.Count > 0)
            {
            }
            else
            {
                MessageBox.Show("There are no machine to do offline sync on target");
                return false;
            }
            return true;
        }
        public override bool CanGoToNextPanel(ImportOfflineSyncForm importOfflineSyncForm)
        {
            importOfflineSyncForm.previousButton.Visible = true;
            return true;

        }

        public override bool CanGoToPreviousPanel(ImportOfflineSyncForm importOfflineSyncForm)
        {
            
            return true;

        }

        public bool GetEsxMasterFile(ImportOfflineSyncForm importOfflineSyncForm)
        {
            importOfflineSyncForm.progressBar.Visible = true;
            _esxIP = importOfflineSyncForm.targetIpText.Text.Trim();
            _esxUserName = importOfflineSyncForm.targetUserNameText.Text.Trim();
            _esxPassword = importOfflineSyncForm.targetPassWordText.Text.Trim();

            if (importOfflineSyncForm.masterHost.esxIp == _esxIP)
            {
                importOfflineSyncForm.masterHost.esxUserName = _esxUserName;
                importOfflineSyncForm.masterHost.esxPassword = _esxPassword;
            }

            importOfflineSyncForm.getTargetDetailsBackgroundWorker.RunWorkerAsync();
           

            return true;
        }
        public bool RunCommands(ImportOfflineSyncForm importOfflineSyncForm)
        {
            Esx esx = new Esx();
            importOfflineSyncForm._esx = esx;
            esx.DownLoadXmlFileForOfflineSync(_esxIP, _esxUserName, _esxPassword, "ESX_Master_Offlinesync.xml");
            //esx.DownloadFile(_esxIP, _esxUserName, _esxPassword, "ESX_Master.xml");
            SolutionConfig config = new SolutionConfig();
            string cxIp = "";
            esx.GetEsxInfo(_esxIP, _esxUserName, _esxPassword, role.SECONDARY,importOfflineSyncForm._osType);
            config.ReadXmlConfigFile("ESX_Master_Offlinesync.xml", ref importOfflineSyncForm._selectedSourceList, ref importOfflineSyncForm.masterHost, ref _esxIP, ref cxIp);
            if (importOfflineSyncForm.masterHost.esxIp == _esxIP)
            {
                importOfflineSyncForm.masterHost.esxUserName = _esxUserName;
                importOfflineSyncForm.masterHost.esxPassword = _esxPassword;
            }

            //config.ReadXmlConfigFile("ESX_Master.xml", ref importOfflineSyncForm._selectedSourceList, ref importOfflineSyncForm.masterHost, ref _esxIP, ref cxIp);
            return true;
        }
            
        public bool LoadIntoDataGridView(ImportOfflineSyncForm importOfflineSyncForm)
        {
            int rowIndex = 0;

            if (importOfflineSyncForm._selectedSourceList._hostList.Count > 0)
            {

                importOfflineSyncForm.targetServerDataGridView.RowCount = importOfflineSyncForm._selectedSourceList._hostList.Count;
                foreach (Host h in importOfflineSyncForm._selectedSourceList._hostList)
                {
                    //importOfflineSyncForm.targetServerDataGridView.Rows.Add(1);
                    importOfflineSyncForm.targetServerDataGridView.Rows[rowIndex].Cells[0].Value = h.esxIp;
                    importOfflineSyncForm.targetServerDataGridView.Rows[rowIndex].Cells[1].Value = h.displayName;
                    importOfflineSyncForm.targetServerDataGridView.Rows[rowIndex].Cells[2].Value = h.ip;
                    rowIndex++;
                }
                importOfflineSyncForm.targetServerDataGridView.Visible = true;
            }
            else
            {
                MessageBox.Show("There are no machine to do offline sync on target");
                return false;
            }
            return true;
        }

    }
}
