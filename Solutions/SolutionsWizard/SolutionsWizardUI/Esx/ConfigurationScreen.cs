using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Data;
using System.Text;
using System.Windows.Forms;

namespace com.InMage.Wizard
{
    public partial class ConfigurationScreen : UserControl
    {
        public ConfigurationScreen()
        {
            InitializeComponent();
            memoryValuesComboBox.Items.Add("GB");
            memoryValuesComboBox.Items.Add("MB");
            //memoryValuesComboBox.SelectedText = "MB";
            buttonColumn.Text = "Properties";
        }

        private void sourceConfigurationTreeView_AfterSelect(object sender, TreeViewEventArgs e)
        {
            if (e.Node.IsSelected == true)
            {
               // foreach(Host h in all
                
               
            }
        }

        private void networkDataGridView_CellClick(object sender, DataGridViewCellEventArgs e)
        {

        }

        private void cpuComboBox_SelectionChangeCommitted(object sender, EventArgs e)
        {

        }

        private void memoryNumericUpDown_ValueChanged(object sender, EventArgs e)
        {

        }

        private void hardWareSettingsCheckBox_CheckedChanged(object sender, EventArgs e)
        {

        }
    }
}
