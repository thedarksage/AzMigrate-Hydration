using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;
using System.Diagnostics;
using com.InMage;
using com.InMage.ESX;
using System.Collections;
using System.IO;

namespace com.InMage.Wizard
{
    public partial class P2V_RecoveryForm : Form
    {
        public HostList _sourceList;
        public  Host _masterHost;
        public string _esxIp ;
        public string _targetEsxUserName;
        public string _targetEsxPassword;
     
        public string _cxIp ;
        ArrayList _tagList = new ArrayList();

        public P2V_RecoveryForm()
        {
            InitializeComponent();
            _tagList.Add("FS");
            _tagList.Add("User Defined");
         
            tagComboBox.DataSource = _tagList;

            //InitializeForm();


        }

        private void cancelButton_Click(object sender, EventArgs e)
        {
            Close();
        }

        private void getCredentialsButton_Click(object sender, EventArgs e)
        {
            AddCredentialsPopUpForm addCredentials = new AddCredentialsPopUpForm();

            addCredentials.ShowDialog();
        }

        private bool InitializeForm()
        {

          


            int rowIndex = 0;
            SolutionConfig  cfg = new SolutionConfig();
            _sourceList = new HostList();
            _masterHost = new Host();
            _esxIp = "";
            _cxIp = "";

            cfg.ReadXmlConfigFile("ESX_Master.xml", ref _sourceList, ref _masterHost, ref _esxIp, ref _cxIp);

            targetEsxIPText.Text = _esxIp;
           
            p2vRecoverDataGridView.RowCount = _sourceList._hostList.Count;

            foreach (Host h in _sourceList._hostList)
            {
                 // p2vRecoverDataGridView.Rows.Add(1);
                  p2vRecoverDataGridView.Rows[rowIndex].Cells[0].Value = h.hostName;

                  p2vRecoverDataGridView.Rows[rowIndex].Cells[1].Value = h.os;
                  p2vRecoverDataGridView.Rows[rowIndex].Cells[2].Value = true;




                
                
                  rowIndex++;

              
                
            }

            return true;

        }

        private void nextButton_Click(object sender, EventArgs e)
        {

            if (Validate() == true)
            {
                Esx esx = new Esx();


                if (File.Exists(Directory.GetCurrentDirectory() + "\\recovery_hostfile.txt"))
                {
                    File.Delete(Directory.GetCurrentDirectory() + "\\recovery_hostfile.txt");

                }

                FileInfo f1= new FileInfo(Directory.GetCurrentDirectory() + "\\recovery_hostfile.txt");
                StreamWriter sw = f1.AppendText();
               
              

                string cwd = Directory.GetCurrentDirectory();

                string recoveryDbPath = "\"" + cwd + "\"";

                foreach (Host h in _sourceList._hostList)
                {
                    string line = h.hostName + "," + recoveryTagText.Text + "," + tagComboBox.SelectedItem.ToString() + "," + "null" + "," + "null" + "," + "null" + "," + "null";
                    sw.WriteLine(line);
                }
                sw.Close();
                
                esx.ExecuteRecoveryScript(recoveryDbPath, _esxIp, _targetEsxUserName, _targetEsxPassword);
                
                nextButton.Visible = false;
                cancelButton.Visible = false;

            } doneButton.Visible = true;
        }

        private void doneButton_Click(object sender, EventArgs e)
        {
            Close();
        }


        public bool Validate()
        {
            if (targetEsxUserNameTextBox.Text.Trim().Length < 1)
            {
                errorProvider.SetError(targetEsxUserNameTextBox, "Please enter username");
                MessageBox.Show("Please Enter UserName");
                return false;
            }
            else
            {
                errorProvider.Clear();
            }

            if (targetEsxPasswordTextBox.Text.Trim().Length < 1)
            {
                errorProvider.SetError(targetEsxPasswordTextBox, "Please enter PassWord");
                MessageBox.Show("Please Enter PassWord");
                return false;
            }
            else
            {
                errorProvider.Clear();
            }
            return true;
        }

        private void targetEsxPasswordTextBox_Enter(object sender, EventArgs e)
        {
            

        }

        private void getDetailsButton_Click(object sender, EventArgs e)
        {
            _esxIp = targetEsxIPText.Text.Trim();
            _targetEsxUserName = targetEsxUserNameTextBox.Text.Trim();
            _targetEsxPassword = targetEsxPasswordTextBox.Text.Trim();


            Esx esx = new Esx();
            if (File.Exists(Directory.GetCurrentDirectory() + "\\ESX_Master.xml"))
            {

                File.Delete(Directory.GetCurrentDirectory() + "\\ESX_Master.xml");
            }

            esx.DownloadEsxMasterXml(_esxIp, _targetEsxUserName, _targetEsxPassword, "ESX_Master.xml");
          
            if (File.Exists(Directory.GetCurrentDirectory() + "\\ESX_Master.xml"))
            {
                SolutionConfig cfg = new SolutionConfig();
                _sourceList = new HostList();
                _masterHost = new Host();
                _esxIp = "";
                _cxIp = "";
                cfg.ReadXmlConfigFile("ESX_Master.xml", ref _sourceList, ref _masterHost, ref _esxIp, ref _cxIp);

                targetEsxIPText.Text = _esxIp;

                p2vRecoverDataGridView.RowCount = _sourceList._hostList.Count;

                int rowIndex = 0;

                foreach (Host h in _sourceList._hostList)
                {
                    // p2vRecoverDataGridView.Rows.Add(1);
                    p2vRecoverDataGridView.Rows[rowIndex].Cells[0].Value = h.displayName;

                    p2vRecoverDataGridView.Rows[rowIndex].Cells[1].Value = h.operatingSystem;
                    p2vRecoverDataGridView.Rows[rowIndex].Cells[2].Value = true;
                    p2vRecoverDataGridView.Rows[rowIndex].Cells[3].Value = rowIndex + 1;






                    rowIndex++;



                }
            }
            else
            {


                MessageBox.Show("Unable to retrieve information from " + targetEsxIPText.Text);
            
            
            }
        }

        private void p2vRecoverDataGridView_CellContentClick(object sender, DataGridViewCellEventArgs e)
        {
            if (e.RowIndex >= 0)
            {
                if (p2vRecoverDataGridView.Rows[e.RowIndex].Cells[0].Selected == true)
                {
                    machineConfigurationLabel.Visible = true;
                    checkBox.Visible = true;
                }
            }
        }

        private void checkBox_CheckedChanged(object sender, EventArgs e)
        {
            if (checkBox.Checked == true)
            {
                ipLabel.Visible = true;
                ipText.Visible = true;
                dnsLabel.Visible = true;
                dnsText.Visible = true;
                defaultGateWayLabel.Visible = true;
                defaultGateWayText.Visible = true;
                subnetMaskLabel.Visible = true;
                subnetMaskText.Visible = true;

            }
            if (checkBox.Checked == false)
            {
                ipLabel.Visible = false;
                ipText.Visible = false;
                dnsLabel.Visible = false;
                dnsText.Visible = false;
                defaultGateWayLabel.Visible = false;
                defaultGateWayText.Visible = false;
                subnetMaskLabel.Visible = false;
                subnetMaskText.Visible = false;
            }
        }

        private void p2vRecoverDataGridView_CellValueChanged(object sender, DataGridViewCellEventArgs e)
        {
            Host h = new Host();
            if (e.RowIndex >= 0)
            {

                h.displayName = p2vRecoverDataGridView.Rows[e.RowIndex].Cells[0].Value.ToString();
                if ((bool)p2vRecoverDataGridView.Rows[e.RowIndex].Cells[2].FormattedValue)
                {
                }
                else
                {
                    //_sourceList.RemoveHost(h);
                }

            }
        }

      
      

       
    }
}
