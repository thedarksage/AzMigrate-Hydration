using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Windows.Forms;
using InMage.APIHelpers;
using Logging;

namespace cspsconfigtool
{
    public partial class UserAccountForm : Form
    {
        private bool canAllowAccountValidation;
        private bool canCreateAccount;
        private bool operationInProgress;
        private CXClient cxClient;

        public UserAccount SelectedAccount { get; private set; }        

        public UserAccountForm(CXClient client)
        {
            InitializeComponent();
            this.canCreateAccount = true;
            this.cxClient = client;
        }

        public UserAccountForm(CXClient client, UserAccount userAccount)
        {
            InitializeComponent();
            this.cxClient = client;
            SelectedAccount = new UserAccount()
            {
                AccountId = userAccount.AccountId,
                AccountName = userAccount.AccountName,
                Domain = userAccount.Domain,
                UserName = userAccount.UserName
            };
            this.friendlyNameTextBox.Text =  userAccount.AccountName;            
            this.usernameTextBox.Text = String.IsNullOrEmpty(userAccount.Domain) ? userAccount.UserName : String.Format("{0}\\{1}", userAccount.Domain, userAccount.UserName);
            this.pwdTextBox.Text = userAccount.Password;
            this.confirmPwdTextBox.Text = userAccount.Password;
        }

        private void GetResources()
        {
            this.Text = ResourceHelper.GetStringResourceValue("accountdetails");
            this.fnameLabel.Text = ResourceHelper.GetStringResourceValue("friendlyname");
            this.unameLabel.Text = ResourceHelper.GetStringResourceValue("username");
            this.userNameFormatLabel.Text = ResourceHelper.GetStringResourceValue("usernameformat");
            this.pwdLabel.Text = ResourceHelper.GetStringResourceValue("pwd");
            this.confirmPwdLabel.Text = ResourceHelper.GetStringResourceValue("confirmpwd");
            this.validateCredentialsCheckBox.Text = ResourceHelper.GetStringResourceValue("validatedomaincred");
            this.okButton.Text = ResourceHelper.GetStringResourceValue("ok");
            this.cancelButton.Text = ResourceHelper.GetStringResourceValue("cancel");
        }

        private bool ValidateUserData()
        {
            bool isValid = false;
            if (String.IsNullOrEmpty(this.friendlyNameTextBox.Text.Trim()))
            {
                MessageBox.Show(this, "The friendly name has only whitespaces. Please enter a valid friendly name.", "Invalid Friendly Name", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
            else if (String.IsNullOrEmpty(this.usernameTextBox.Text.Trim()))
            {
                MessageBox.Show(this, "The user name has only whitespaces. Please enter a valid user name.", "Invalid User Name", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
            else if (String.IsNullOrEmpty(this.pwdTextBox.Text.Trim()))
            {
                MessageBox.Show(this, "The password has only whitespaces. Please enter a valid password.", "Invalid Password", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
            else if (String.IsNullOrEmpty(this.confirmPwdTextBox.Text.Trim()))
            {
                MessageBox.Show(this, "The value entered in the confirm password field has only whitespaces. Please re-enter the password correctly.", "Invalid Password", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
            else if (String.Compare(this.pwdTextBox.Text, this.confirmPwdTextBox.Text, StringComparison.OrdinalIgnoreCase) != 0)
            {
                MessageBox.Show(this, "The passwords don't match.", "Incorrect Password", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
            else if(!string.IsNullOrEmpty(this.usernameTextBox.Text))
            {
                int i = 0;
                bool userErrorFlag = false;
                string[] userNameSeperater = Regex.Split(this.usernameTextBox.Text, "\\\\");
                foreach (string seperater in userNameSeperater)
                {
                    if ((i == 0) && string.IsNullOrEmpty(seperater))
                    {
                        MessageBox.Show(this, "The user name format is not valid. Please enter a valid user name.", "Invalid User Name", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        userErrorFlag = true;
                    }
                    i++;
                }
                if(!userErrorFlag)
                {
                    isValid = true;
                }
            }
            else
            {
                isValid = true;
            }

            return isValid;
        }

        private void EnableControls(bool enabled)
        {
            this.friendlyNameTextBox.Enabled = this.usernameTextBox.Enabled = this.pwdTextBox.Enabled = this.confirmPwdTextBox.Enabled = enabled;
            this.okButton.Enabled = this.cancelButton.Enabled = enabled;
            if(canAllowAccountValidation)
            {
                this.validateCredentialsCheckBox.Enabled = enabled;
            }
        }

        private void okButton_Click(object sender, EventArgs e)
        {
            if (ValidateUserData())
            {
                EnableControls(false);
                UserAccount userAccount = new UserAccount();
                if(SelectedAccount != null)
                {
                    userAccount.AccountId = SelectedAccount.AccountId;
                }
                userAccount.AccountName = friendlyNameTextBox.Text.Trim();
                userAccount.SetDomainAndUserName(usernameTextBox.Text.Trim());
                userAccount.Password = pwdTextBox.Text.Trim();
                this.operationInProgress = true;
                this.addBackgroundWorker.RunWorkerAsync(userAccount);
                this.DialogResult = System.Windows.Forms.DialogResult.None;
            }
            else
            {
                EnableControls(true);
                this.DialogResult = System.Windows.Forms.DialogResult.None;
            }
        }

        private void cancelButton_Click(object sender, EventArgs e)
        {
            this.DialogResult = System.Windows.Forms.DialogResult.Cancel;
        }

        private void addBackgroundWorker_DoWork(object sender, DoWorkEventArgs e)
        {
            ResponseStatus responseStatus = null;
            this.addBackgroundWorker.ReportProgress(10, this.canCreateAccount ? "Adding Account.. Please wait for few minutes." : "Updating Account details.. Please wait for few minutes.");
            if (e.Argument is UserAccount)
            {
                UserAccount userAccount = e.Argument as UserAccount;
                //if (this.validateCredentialsCheckBox.Checked)
                //{
                //    if (!userAccount.ValidateDomainCredentials())
                //    {
                //        throw new Exception("Failed to validate domain credentials");
                //    }
                //}
                List<UserAccount> userAccountList = new List<UserAccount>();
                userAccountList.Add(userAccount);
                responseStatus = new ResponseStatus();
                if (this.canCreateAccount)
                {
                    this.cxClient.CreateAccounts(userAccountList, responseStatus);
                }
                else
                {
                    this.cxClient.UpdateAccounts(userAccountList, responseStatus);
                }
            }
            e.Result = responseStatus;
        }

        private void backgroundWorker_ProgressChanged(object sender, ProgressChangedEventArgs e)
        {
            ShowProgress(true, e.UserState.ToString());
        }

        private void addBackgroundWorker_RunWorkerCompleted(object sender, RunWorkerCompletedEventArgs e)
        {
            ShowProgress(false, null);
            string accountName = this.friendlyNameTextBox.Text.Trim();
            string operation = String.Format("{0} \"{1}\" Account", this.canCreateAccount ? "add" : "update", accountName);
            string message;
            if(e.Error != null)
            {                
                Logger.Error(String.Format("Failed to {0}. Error: {1}", operation, e.Error));                
                MessageBox.Show(this,
                       Services.Helpers.GetCXAPIErrorMessage(operation, e.Error),
                       "Error",
                       MessageBoxButtons.OK,
                       MessageBoxIcon.Error);
                this.operationInProgress = false;
            }
            else if (e.Result != null && e.Result is ResponseStatus)
            {
                ResponseStatus responseStatus = e.Result as ResponseStatus;                
                if (responseStatus.ReturnCode == 0)
                {
                    if (SelectedAccount != null)
                    {
                        this.SelectedAccount.AccountName = accountName;
                        this.SelectedAccount.SetDomainAndUserName(usernameTextBox.Text.Trim());
                        this.SelectedAccount.Password = this.pwdTextBox.Text.Trim();
                    }
                    message = String.Format("Successfully {0} \"{1}\" Account", this.canCreateAccount ? "added" : "updated", accountName);
                    Logger.Debug(message);
                    MessageBox.Show(this,
                         message,
                         "Information",
                         MessageBoxButtons.OK,
                         MessageBoxIcon.Information);
                    this.operationInProgress = false;
                    this.DialogResult = System.Windows.Forms.DialogResult.OK;
                }
                else
                {
                    message = Services.Helpers.GetCXAPIErrorMessage(operation, responseStatus);
                    Logger.Error(message);
                    MessageBox.Show(this,
                        message,
                        "Error",
                        MessageBoxButtons.OK,
                        MessageBoxIcon.Error);
                    this.operationInProgress = false;                    
                }
            }
            else
            {
                message = String.Format("Failed to {0} with internal error.", operation);
                Logger.Error(message);
                MessageBox.Show(this,
                       message,
                       "Error",
                       MessageBoxButtons.OK,
                       MessageBoxIcon.Error);
                this.operationInProgress = false;
            }
            
            EnableControls(true);
        }

        private void ShowProgress(bool showMessage, string message)
        {            
            this.progressLabel.Visible = showMessage;
            this.progressLabel.Text = showMessage ? message : null;
        }

        private void textBox_TextChanged(object sender, EventArgs e)
        {
            TextBox textBox = sender as TextBox;            
            if(textBox != null && textBox.Name == this.usernameTextBox.Name && !String.IsNullOrEmpty(textBox.Text))
            {
                string[] tokens = textBox.Text.Split(new char[]{'\\'}, StringSplitOptions.RemoveEmptyEntries);                
                if(tokens.Length == 2)
                {
                    this.validateCredentialsCheckBox.Enabled = true;
                    canAllowAccountValidation = true;
                }
                else
                {
                    this.validateCredentialsCheckBox.Enabled = false;
                    canAllowAccountValidation = false;
                }
            }

            if (String.IsNullOrEmpty(this.friendlyNameTextBox.Text) || String.IsNullOrEmpty(this.usernameTextBox.Text) ||
                String.IsNullOrEmpty(this.pwdTextBox.Text) || String.IsNullOrEmpty(this.confirmPwdTextBox.Text))
            {
                this.okButton.Enabled = false;
            }
            else
            {
                this.okButton.Enabled = true;
            }
        }

        private void UserAccountForm_FormClosing(object sender, FormClosingEventArgs e)
        {
            if(this.operationInProgress)
            {
                MessageBox.Show(this, "Saving the changes. Please wait for few minutes.", "Closing", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                e.Cancel = true;
            }
        }
    }
}
