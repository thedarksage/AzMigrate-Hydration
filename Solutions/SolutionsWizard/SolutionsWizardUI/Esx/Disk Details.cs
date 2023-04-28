using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;

namespace Com.Inmage.Wizard
{
    public partial class DiskDetails : Form
    {
        internal System.Drawing.Bitmap information;
        internal System.Drawing.Bitmap error;
        internal bool selectedtoContinue = false;
        public DiskDetails()
        {
            InitializeComponent();
            information = Wizard.Properties.Resources.Information;
            pictureBox.Image = information;
           
        }

        private void okButton_Click(object sender, EventArgs e)
        {
            selectedtoContinue = false;

            Close();
        }

        private void yesButton_Click(object sender, EventArgs e)
        {
            selectedtoContinue = true;

            Close();
        }
    }
}
