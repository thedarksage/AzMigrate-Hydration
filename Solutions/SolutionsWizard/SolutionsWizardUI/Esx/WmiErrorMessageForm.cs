using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;

namespace Com.Inmage.Wizard
{
    public partial class WmiErrorMessageForm : Form
    {

        internal bool continueWithOutValidation = false;
        internal string domain = "", username = "", password = "";
        internal bool canceled = false;
        public WmiErrorMessageForm(string message)
        {

            InitializeComponent();
            continueButton.Enabled = false;
            credentialsGroupBox.Visible = false;
            messageTextBox.AppendText(message);

        }

       
        private void continueButton_Click(object sender, EventArgs e)
        {
            if (skipWmiValidationsRadioButton.Checked == true)
            {
                continueWithOutValidation = true;
                Close();
            }
            else if(retryRadioButton.Checked == true)
            {
                
                    if (Validate() == true)
                    {
                        domain = domainText.Text;
                        username = userNameText.Text;
                        password = passWordText.Text;
                        Close();
                    }


                
            }

        }


        
        private bool Validate()
        {
            if (userNameText.Text.Length == 0)
            {
                MessageBox.Show("Please enter username", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return false;
            }
            if (passWordText.Text.Length == 0)
            {
                MessageBox.Show("Please enter password", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return false;
            }
            return true;
        }

        private void skipWmiValidationsRadioButton_CheckedChanged(object sender, EventArgs e)
        {
            if (skipWmiValidationsRadioButton.Checked == true)
            {
                continueButton.Enabled = true;
               
                credentialsGroupBox.Visible = false;
            }
          
        }

        private void retryRadioButton_CheckedChanged(object sender, EventArgs e)
        {
            if (retryRadioButton.Checked == true)
           
            {
                credentialsGroupBox.Visible = true;
                continueButton.Enabled = true;

            }

        }

        private void cancelButton_Click(object sender, EventArgs e)
        {
            canceled = true;
            Close();
        }
    }
}
