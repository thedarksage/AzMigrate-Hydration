using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;

namespace com.InMage.Wizard
{
    public partial class NATIPForm : Form
    {
        public bool _cancelled = false;
        public NATIPForm()
        {
            InitializeComponent();
        }

        private void addButton_Click(object sender, EventArgs e)
        {
            if (AddIp() == true)
            {
                Close();
            }
        }

        private void cancelButton_Click(object sender, EventArgs e)
        {
            _cancelled = true;
            Close();
        }
        private bool AddIp()
        {
            if (natIPTextBox.Text.Length < 4)
            {
                MessageBox.Show("Please enter NAT IP address");
                return false;
            }
            if(portNumberTextBox.Visible == true)
            {
                if (portNumberTextBox.Text.Length == 0)
                {
                    MessageBox.Show("Enter CX port number");
                    return false;
                }
            }
                
            return true;
        }

        private void natIPTextBox_KeyPress(object sender, KeyPressEventArgs e)
        {
            e.Handled = !Char.IsNumber(e.KeyChar) && (e.KeyChar != '\b')&& (e.KeyChar != '.');
        }

        private void portNumberTextBox_KeyPress(object sender, KeyPressEventArgs e)
        {
            e.Handled = !Char.IsNumber(e.KeyChar) && (e.KeyChar != '\b')&& (e.KeyChar != '.');
        }
    }
}
