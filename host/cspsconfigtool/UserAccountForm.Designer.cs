namespace cspsconfigtool
{
    partial class UserAccountForm
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this.confirmPwdTextBox = new System.Windows.Forms.TextBox();
            this.confirmPwdLabel = new System.Windows.Forms.Label();
            this.pwdTextBox = new System.Windows.Forms.TextBox();
            this.pwdLabel = new System.Windows.Forms.Label();
            this.usernameTextBox = new System.Windows.Forms.TextBox();
            this.unameLabel = new System.Windows.Forms.Label();
            this.friendlyNameTextBox = new System.Windows.Forms.TextBox();
            this.fnameLabel = new System.Windows.Forms.Label();
            this.cancelButton = new System.Windows.Forms.Button();
            this.okButton = new System.Windows.Forms.Button();
            this.validateCredentialsCheckBox = new System.Windows.Forms.CheckBox();
            this.separatorLabel = new System.Windows.Forms.Label();
            this.addBackgroundWorker = new System.ComponentModel.BackgroundWorker();
            this.progressLabel = new System.Windows.Forms.Label();
            this.userNameFormatLabel = new System.Windows.Forms.Label();
            this.SuspendLayout();
            // 
            // confirmPwdTextBox
            // 
            this.confirmPwdTextBox.Location = new System.Drawing.Point(180, 186);
            this.confirmPwdTextBox.Name = "confirmPwdTextBox";
            this.confirmPwdTextBox.Size = new System.Drawing.Size(100, 20);
            this.confirmPwdTextBox.TabIndex = 7;
            this.confirmPwdTextBox.UseSystemPasswordChar = true;
            this.confirmPwdTextBox.TextChanged += new System.EventHandler(this.textBox_TextChanged);
            // 
            // confirmPwdLabel
            // 
            this.confirmPwdLabel.AutoSize = true;
            this.confirmPwdLabel.Location = new System.Drawing.Point(26, 189);
            this.confirmPwdLabel.Name = "confirmPwdLabel";
            this.confirmPwdLabel.Size = new System.Drawing.Size(90, 13);
            this.confirmPwdLabel.TabIndex = 6;
            this.confirmPwdLabel.Text = "Confirm password";
            // 
            // pwdTextBox
            // 
            this.pwdTextBox.Location = new System.Drawing.Point(180, 143);
            this.pwdTextBox.Name = "pwdTextBox";
            this.pwdTextBox.Size = new System.Drawing.Size(100, 20);
            this.pwdTextBox.TabIndex = 5;
            this.pwdTextBox.UseSystemPasswordChar = true;
            this.pwdTextBox.TextChanged += new System.EventHandler(this.textBox_TextChanged);
            // 
            // pwdLabel
            // 
            this.pwdLabel.AutoSize = true;
            this.pwdLabel.Location = new System.Drawing.Point(26, 146);
            this.pwdLabel.Name = "pwdLabel";
            this.pwdLabel.Size = new System.Drawing.Size(53, 13);
            this.pwdLabel.TabIndex = 4;
            this.pwdLabel.Text = "Password";
            // 
            // usernameTextBox
            // 
            this.usernameTextBox.Location = new System.Drawing.Point(180, 89);
            this.usernameTextBox.Name = "usernameTextBox";
            this.usernameTextBox.Size = new System.Drawing.Size(100, 20);
            this.usernameTextBox.TabIndex = 3;
            this.usernameTextBox.TextChanged += new System.EventHandler(this.textBox_TextChanged);
            // 
            // unameLabel
            // 
            this.unameLabel.AutoSize = true;
            this.unameLabel.Location = new System.Drawing.Point(26, 92);
            this.unameLabel.Name = "unameLabel";
            this.unameLabel.Size = new System.Drawing.Size(58, 13);
            this.unameLabel.TabIndex = 2;
            this.unameLabel.Text = "User name";
            // 
            // friendlyNameTextBox
            // 
            this.friendlyNameTextBox.Location = new System.Drawing.Point(180, 45);
            this.friendlyNameTextBox.Name = "friendlyNameTextBox";
            this.friendlyNameTextBox.Size = new System.Drawing.Size(100, 20);
            this.friendlyNameTextBox.TabIndex = 1;
            this.friendlyNameTextBox.TextChanged += new System.EventHandler(this.textBox_TextChanged);
            // 
            // fnameLabel
            // 
            this.fnameLabel.AutoSize = true;
            this.fnameLabel.Location = new System.Drawing.Point(26, 48);
            this.fnameLabel.Name = "fnameLabel";
            this.fnameLabel.Size = new System.Drawing.Size(145, 13);
            this.fnameLabel.TabIndex = 0;
            this.fnameLabel.Text = "Friendly name (used in Azure)";
            // 
            // cancelButton
            // 
            this.cancelButton.DialogResult = System.Windows.Forms.DialogResult.Cancel;
            this.cancelButton.Location = new System.Drawing.Point(271, 290);
            this.cancelButton.Name = "cancelButton";
            this.cancelButton.Size = new System.Drawing.Size(75, 23);
            this.cancelButton.TabIndex = 12;
            this.cancelButton.Text = "Cancel";
            this.cancelButton.UseVisualStyleBackColor = true;
            this.cancelButton.Click += new System.EventHandler(this.cancelButton_Click);
            // 
            // okButton
            // 
            this.okButton.DialogResult = System.Windows.Forms.DialogResult.Cancel;
            this.okButton.Enabled = false;
            this.okButton.Location = new System.Drawing.Point(169, 290);
            this.okButton.Name = "okButton";
            this.okButton.Size = new System.Drawing.Size(75, 23);
            this.okButton.TabIndex = 11;
            this.okButton.Text = "OK";
            this.okButton.UseVisualStyleBackColor = true;
            this.okButton.Click += new System.EventHandler(this.okButton_Click);
            // 
            // validateCredentialsCheckBox
            // 
            this.validateCredentialsCheckBox.Enabled = false;
            this.validateCredentialsCheckBox.Location = new System.Drawing.Point(65, 214);
            this.validateCredentialsCheckBox.Name = "validateCredentialsCheckBox";
            this.validateCredentialsCheckBox.Size = new System.Drawing.Size(279, 32);
            this.validateCredentialsCheckBox.TabIndex = 8;
            this.validateCredentialsCheckBox.Text = "Validate domain credentials";
            this.validateCredentialsCheckBox.UseVisualStyleBackColor = true;
            this.validateCredentialsCheckBox.Visible = false;
            // 
            // separatorLabel
            // 
            this.separatorLabel.BorderStyle = System.Windows.Forms.BorderStyle.Fixed3D;
            this.separatorLabel.Location = new System.Drawing.Point(3, 280);
            this.separatorLabel.Name = "separatorLabel";
            this.separatorLabel.Size = new System.Drawing.Size(365, 3);
            this.separatorLabel.TabIndex = 10;
            // 
            // addBackgroundWorker
            // 
            this.addBackgroundWorker.WorkerReportsProgress = true;
            this.addBackgroundWorker.DoWork += new System.ComponentModel.DoWorkEventHandler(this.addBackgroundWorker_DoWork);
            this.addBackgroundWorker.ProgressChanged += new System.ComponentModel.ProgressChangedEventHandler(this.backgroundWorker_ProgressChanged);
            this.addBackgroundWorker.RunWorkerCompleted += new System.ComponentModel.RunWorkerCompletedEventHandler(this.addBackgroundWorker_RunWorkerCompleted);
            // 
            // progressLabel
            // 
            this.progressLabel.Location = new System.Drawing.Point(26, 253);
            this.progressLabel.Name = "progressLabel";
            this.progressLabel.Size = new System.Drawing.Size(318, 26);
            this.progressLabel.TabIndex = 9;
            this.progressLabel.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
            // 
            // userNameFormatLabel
            // 
            this.userNameFormatLabel.AutoSize = true;
            this.userNameFormatLabel.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.userNameFormatLabel.Location = new System.Drawing.Point(178, 111);
            this.userNameFormatLabel.Name = "userNameFormatLabel";
            this.userNameFormatLabel.Size = new System.Drawing.Size(105, 13);
            this.userNameFormatLabel.TabIndex = 22;
            this.userNameFormatLabel.Text = "(Domain\\User name)";
            // 
            // UserAccountForm
            // 
            this.AcceptButton = this.okButton;
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.BackColor = System.Drawing.SystemColors.Control;
            this.CancelButton = this.cancelButton;
            this.ClientSize = new System.Drawing.Size(374, 323);
            this.Controls.Add(this.userNameFormatLabel);
            this.Controls.Add(this.progressLabel);
            this.Controls.Add(this.separatorLabel);
            this.Controls.Add(this.validateCredentialsCheckBox);
            this.Controls.Add(this.okButton);
            this.Controls.Add(this.cancelButton);
            this.Controls.Add(this.confirmPwdTextBox);
            this.Controls.Add(this.confirmPwdLabel);
            this.Controls.Add(this.pwdTextBox);
            this.Controls.Add(this.pwdLabel);
            this.Controls.Add(this.usernameTextBox);
            this.Controls.Add(this.unameLabel);
            this.Controls.Add(this.friendlyNameTextBox);
            this.Controls.Add(this.fnameLabel);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
            this.MaximizeBox = false;
            this.MinimizeBox = false;
            this.Name = "UserAccountForm";
            this.ShowInTaskbar = false;
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
            this.Text = "Account Details";
            this.FormClosing += new System.Windows.Forms.FormClosingEventHandler(this.UserAccountForm_FormClosing);
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.TextBox confirmPwdTextBox;
        private System.Windows.Forms.Label confirmPwdLabel;
        private System.Windows.Forms.TextBox pwdTextBox;
        private System.Windows.Forms.Label pwdLabel;
        private System.Windows.Forms.TextBox usernameTextBox;
        private System.Windows.Forms.Label unameLabel;
        private System.Windows.Forms.TextBox friendlyNameTextBox;
        private System.Windows.Forms.Label fnameLabel;
        private System.Windows.Forms.Button cancelButton;
        private System.Windows.Forms.Button okButton;
        private System.Windows.Forms.CheckBox validateCredentialsCheckBox;
        private System.Windows.Forms.Label separatorLabel;
        private System.ComponentModel.BackgroundWorker addBackgroundWorker;
        private System.Windows.Forms.Label progressLabel;
        private System.Windows.Forms.Label userNameFormatLabel;
    }
}