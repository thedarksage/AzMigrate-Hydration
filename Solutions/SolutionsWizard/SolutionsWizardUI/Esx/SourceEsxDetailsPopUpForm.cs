using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;

namespace Com.Inmage.Wizard
{
    public partial class SourceEsxDetailsPopUpForm : Form
    {
        internal bool canceled;
        public SourceEsxDetailsPopUpForm()
        {
            InitializeComponent();
        }

        private void cancelButton_Click(object sender, EventArgs e)
        {
            canceled = true;
            Close();
        }

        private void addButton_Click(object sender, EventArgs e)
        {
            if (sourceEsxIpText.Text.Trim().Length < 1)
            { MessageBox.Show("Please enter primary vSphere server ip", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
              return;
            }
            if (userNameText.Text.Trim().Length < 1)
            {
                MessageBox.Show("Please enter username", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }
            if (passWordText.Text.Trim().Length < 1)
            {
                MessageBox.Show("Please enter password", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                 return;

            }
            Close();
        }
    }
}
