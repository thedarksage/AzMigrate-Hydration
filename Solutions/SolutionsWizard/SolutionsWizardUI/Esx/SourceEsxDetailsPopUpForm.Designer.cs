namespace Com.Inmage.Wizard
{
    partial class SourceEsxDetailsPopUpForm
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
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(SourceEsxDetailsPopUpForm));
            this.cancelButton = new System.Windows.Forms.Button();
            this.addButton = new System.Windows.Forms.Button();
            this.passWordText = new System.Windows.Forms.TextBox();
            this.userNameText = new System.Windows.Forms.TextBox();
            this.sourceEsxIpText = new System.Windows.Forms.TextBox();
            this.userNameLabel = new System.Windows.Forms.Label();
            this.ipLabel = new System.Windows.Forms.Label();
            this.passwordLabel = new System.Windows.Forms.Label();
            this.domainNameLabel = new System.Windows.Forms.Label();
            this.domainText = new System.Windows.Forms.TextBox();
            this.headingPanel = new System.Windows.Forms.Panel();
            this.credentialsHeadingLabel = new System.Windows.Forms.Label();
            this.navaigationPanel = new System.Windows.Forms.Panel();
            this.mandatoryLabel = new System.Windows.Forms.Label();
            this.headingPanel.SuspendLayout();
            this.SuspendLayout();
            // 
            // cancelButton
            // 
            this.cancelButton.Location = new System.Drawing.Point(284, 300);
            this.cancelButton.Name = "cancelButton";
            this.cancelButton.Size = new System.Drawing.Size(75, 23);
            this.cancelButton.TabIndex = 35;
            this.cancelButton.Text = "Close";
            this.cancelButton.UseVisualStyleBackColor = true;
            this.cancelButton.Click += new System.EventHandler(this.cancelButton_Click);
            // 
            // addButton
            // 
            this.addButton.Location = new System.Drawing.Point(203, 300);
            this.addButton.Name = "addButton";
            this.addButton.Size = new System.Drawing.Size(75, 23);
            this.addButton.TabIndex = 34;
            this.addButton.Text = "Add";
            this.addButton.UseVisualStyleBackColor = true;
            this.addButton.Click += new System.EventHandler(this.addButton_Click);
            // 
            // passWordText
            // 
            this.passWordText.Location = new System.Drawing.Point(199, 139);
            this.passWordText.Name = "passWordText";
            this.passWordText.PasswordChar = '*';
            this.passWordText.Size = new System.Drawing.Size(121, 20);
            this.passWordText.TabIndex = 31;
            this.passWordText.UseSystemPasswordChar = true;
            // 
            // userNameText
            // 
            this.userNameText.Location = new System.Drawing.Point(199, 103);
            this.userNameText.Name = "userNameText";
            this.userNameText.Size = new System.Drawing.Size(121, 20);
            this.userNameText.TabIndex = 30;
            // 
            // sourceEsxIpText
            // 
            this.sourceEsxIpText.Location = new System.Drawing.Point(199, 67);
            this.sourceEsxIpText.Name = "sourceEsxIpText";
            this.sourceEsxIpText.ReadOnly = true;
            this.sourceEsxIpText.Size = new System.Drawing.Size(121, 20);
            this.sourceEsxIpText.TabIndex = 29;
            // 
            // userNameLabel
            // 
            this.userNameLabel.AutoSize = true;
            this.userNameLabel.BackColor = System.Drawing.Color.Transparent;
            this.userNameLabel.Location = new System.Drawing.Point(23, 105);
            this.userNameLabel.Name = "userNameLabel";
            this.userNameLabel.Size = new System.Drawing.Size(62, 13);
            this.userNameLabel.TabIndex = 28;
            this.userNameLabel.Text = "Username *";
            // 
            // ipLabel
            // 
            this.ipLabel.AutoSize = true;
            this.ipLabel.BackColor = System.Drawing.Color.Transparent;
            this.ipLabel.Location = new System.Drawing.Point(23, 68);
            this.ipLabel.Name = "ipLabel";
            this.ipLabel.Size = new System.Drawing.Size(111, 13);
            this.ipLabel.TabIndex = 27;
            this.ipLabel.Text = "Source ESX IP/Name";
            // 
            // passwordLabel
            // 
            this.passwordLabel.AutoSize = true;
            this.passwordLabel.BackColor = System.Drawing.Color.Transparent;
            this.passwordLabel.Location = new System.Drawing.Point(24, 145);
            this.passwordLabel.Name = "passwordLabel";
            this.passwordLabel.Size = new System.Drawing.Size(60, 13);
            this.passwordLabel.TabIndex = 36;
            this.passwordLabel.Text = "Password *";
            // 
            // domainNameLabel
            // 
            this.domainNameLabel.AutoSize = true;
            this.domainNameLabel.BackColor = System.Drawing.Color.Transparent;
            this.domainNameLabel.Location = new System.Drawing.Point(27, 184);
            this.domainNameLabel.Name = "domainNameLabel";
            this.domainNameLabel.Size = new System.Drawing.Size(43, 13);
            this.domainNameLabel.TabIndex = 37;
            this.domainNameLabel.Text = "Domain";
            this.domainNameLabel.Visible = false;
            // 
            // domainText
            // 
            this.domainText.Location = new System.Drawing.Point(199, 176);
            this.domainText.Name = "domainText";
            this.domainText.Size = new System.Drawing.Size(121, 20);
            this.domainText.TabIndex = 38;
            this.domainText.Visible = false;
            // 
            // headingPanel
            // 
            this.headingPanel.BackColor = System.Drawing.Color.Transparent;
            this.headingPanel.BackgroundImage = ((System.Drawing.Image)(resources.GetObject("headingPanel.BackgroundImage")));
            this.headingPanel.Controls.Add(this.credentialsHeadingLabel);
            this.headingPanel.Location = new System.Drawing.Point(-1, 0);
            this.headingPanel.Name = "headingPanel";
            this.headingPanel.Size = new System.Drawing.Size(377, 44);
            this.headingPanel.TabIndex = 39;
            // 
            // credentialsHeadingLabel
            // 
            this.credentialsHeadingLabel.AutoSize = true;
            this.credentialsHeadingLabel.Font = new System.Drawing.Font("Microsoft Sans Serif", 9.75F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.credentialsHeadingLabel.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(186)))), ((int)(((byte)(61)))), ((int)(((byte)(69)))));
            this.credentialsHeadingLabel.Location = new System.Drawing.Point(10, 7);
            this.credentialsHeadingLabel.Name = "credentialsHeadingLabel";
            this.credentialsHeadingLabel.Size = new System.Drawing.Size(143, 16);
            this.credentialsHeadingLabel.TabIndex = 1;
            this.credentialsHeadingLabel.Text = "Provide credentials";
            // 
            // navaigationPanel
            // 
            this.navaigationPanel.BackColor = System.Drawing.Color.Transparent;
            this.navaigationPanel.BackgroundImage = global::Com.Inmage.Wizard.Properties.Resources.navBar;
            this.navaigationPanel.Location = new System.Drawing.Point(3, 276);
            this.navaigationPanel.Name = "navaigationPanel";
            this.navaigationPanel.Size = new System.Drawing.Size(365, 10);
            this.navaigationPanel.TabIndex = 40;
            // 
            // mandatoryLabel
            // 
            this.mandatoryLabel.AutoSize = true;
            this.mandatoryLabel.BackColor = System.Drawing.Color.Transparent;
            this.mandatoryLabel.ForeColor = System.Drawing.Color.Black;
            this.mandatoryLabel.Location = new System.Drawing.Point(9, 305);
            this.mandatoryLabel.Name = "mandatoryLabel";
            this.mandatoryLabel.Size = new System.Drawing.Size(103, 13);
            this.mandatoryLabel.TabIndex = 143;
            this.mandatoryLabel.Text = "( * ) Mandatory fields";
            // 
            // SourceEsxDetailsPopUpForm
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.BackColor = System.Drawing.Color.White;
            this.BackgroundImage = global::Com.Inmage.Wizard.Properties.Resources.workarea;
            this.ClientSize = new System.Drawing.Size(371, 335);
            this.ControlBox = false;
            this.Controls.Add(this.mandatoryLabel);
            this.Controls.Add(this.navaigationPanel);
            this.Controls.Add(this.headingPanel);
            this.Controls.Add(this.domainText);
            this.Controls.Add(this.domainNameLabel);
            this.Controls.Add(this.passwordLabel);
            this.Controls.Add(this.cancelButton);
            this.Controls.Add(this.addButton);
            this.Controls.Add(this.passWordText);
            this.Controls.Add(this.userNameText);
            this.Controls.Add(this.sourceEsxIpText);
            this.Controls.Add(this.userNameLabel);
            this.Controls.Add(this.ipLabel);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedSingle;
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.MaximizeBox = false;
            this.MinimizeBox = false;
            this.Name = "SourceEsxDetailsPopUpForm";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
            this.Text = "vContinuum";
            this.headingPanel.ResumeLayout(false);
            this.headingPanel.PerformLayout();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Button cancelButton;
        private System.Windows.Forms.Button addButton;
        private System.Windows.Forms.Label userNameLabel;
        private System.Windows.Forms.Label passwordLabel;
        private System.Windows.Forms.Panel headingPanel;
        private System.Windows.Forms.Label credentialsHeadingLabel;
        private System.Windows.Forms.Panel navaigationPanel;
        internal System.Windows.Forms.TextBox passWordText;
        internal System.Windows.Forms.TextBox userNameText;
        internal System.Windows.Forms.TextBox sourceEsxIpText;
        internal System.Windows.Forms.Label domainNameLabel;
        internal System.Windows.Forms.TextBox domainText;
        internal System.Windows.Forms.Label ipLabel;
        internal System.Windows.Forms.Label mandatoryLabel;
    }
}