using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Diagnostics;
using System.Windows.Forms;

namespace Com.Inmage.Wizard
{
    public partial class MessageBoxForm : Form
    {
        internal bool canceled = false;
        public MessageBoxForm()
        {
            InitializeComponent();
        }

        private void okButton_Click(object sender, EventArgs e)
        {
            Close();
        }

        private void cancelbutton_Click(object sender, EventArgs e)
        {
            canceled = true;
            Close();
        }

        private void driverLinkLabel_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
        {
            try
            {
                System.Diagnostics.Process.Start("http://www.lsi.com/Search/Pages/results.aspx?k=symmpi_wXP_1201800.ZIP");
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed to open hyperlink " + ex.Message);
            }
        }
    }
}
