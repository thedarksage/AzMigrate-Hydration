using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;

namespace Com.Inmage.Wizard
{
    public partial class AddCredentialsPopUpForm : Form
    {
        internal string domain, userName, passWord;
        internal string previousDomain, previousUsername, previousPassword;
        internal bool canceled;
        public AddCredentialsPopUpForm()
        {
            InitializeComponent();

            noteLabel.Text = "Note: Provide local(or)domain administrator user ." + Environment.NewLine;
                
           noteLabel.BringToFront();
           noteLabel.Show();
           useForAllCheckBox.Checked = true;

        }

        private void passWordText_TextChanged(object sender, EventArgs e)
        {
            passWordText.PasswordChar = '*';
        }

        private void addButton_Click(object sender, EventArgs e)
        {
            

            if (addCredentialsFormIsValidated() == true)
            {
                domain = domainText.Text.Trim();
                userName = userNameText.Text.Trim();
                passWord = passWordText.Text;

                CachingCredentialsValues();
                             
                
                Close();
            }
        }
        private bool addCredentialsFormIsValidated()
        {
            bool validated = true;
           
            if (passWordText.Text.Trim().Length < 1 )
            {
                addCredentialsErrorProvider.SetError(passWordText, "Please enter password");

                MessageBox.Show("Please enter valid password", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                validated = false;
            }
            if (userNameText.Text.Trim().Length < 1)
            {
                addCredentialsErrorProvider.SetError(userNameText, "Please enter username ");
                MessageBox.Show("Please enter valid username","Error",MessageBoxButtons.OK,MessageBoxIcon.Error);
                validated = false;

            }
            if (domainText.Text.Trim().Length < 1)
            {
                //addCredentialsErrorProvider.SetError(domainText, "please enter a valid domain ");
               // validated = false;
            }



            return validated;


        }
       

        private void cancelButton_Click(object sender, EventArgs e)
        {
            canceled = true;
            Close();
        }
        private bool CachingCredentialsValues()
        {
           

            previousDomain = domain;
            previousUsername = userName;
            previousPassword = passWord;

            return true;

        }

        private void domainText_TextChanged(object sender, EventArgs e)
        {

        }

        private void userNameText_TextChanged(object sender, EventArgs e)
        {

        }

        private void AddCredentialsPopUpForm_HelpButtonClicked(object sender, CancelEventArgs e)
        {

        }

      
        
    }
}
