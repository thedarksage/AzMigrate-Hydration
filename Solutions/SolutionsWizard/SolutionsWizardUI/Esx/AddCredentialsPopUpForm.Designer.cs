namespace Com.Inmage.Wizard
{
    partial class AddCredentialsPopUpForm
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
            this.components = new System.ComponentModel.Container();
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(AddCredentialsPopUpForm));
            this.domainLabel = new System.Windows.Forms.Label();
            this.userNameLabel = new System.Windows.Forms.Label();
            this.passWordLabel = new System.Windows.Forms.Label();
            this.domainText = new System.Windows.Forms.TextBox();
            this.userNameText = new System.Windows.Forms.TextBox();
            this.passWordText = new System.Windows.Forms.TextBox();
            this.addButton = new System.Windows.Forms.Button();
            this.cancelButton = new System.Windows.Forms.Button();
            this.addCredentialsErrorProvider = new System.Windows.Forms.ErrorProvider(this.components);
            this.useForAllCheckBox = new System.Windows.Forms.CheckBox();
            this.noteLabel = new System.Windows.Forms.Label();
            this.credentialHeading = new System.Windows.Forms.Panel();
            this.credentialsHeadingLabel = new System.Windows.Forms.Label();
            this.navaigationPanel = new System.Windows.Forms.Panel();
            this.mandatoryLabel = new System.Windows.Forms.Label();
            ((System.ComponentModel.ISupportInitialize)(this.addCredentialsErrorProvider)).BeginInit();
            this.credentialHeading.SuspendLayout();
            this.SuspendLayout();
            // 
            // domainLabel
            // 
            this.domainLabel.AutoSize = true;
            this.domainLabel.BackColor = System.Drawing.Color.Transparent;
            this.domainLabel.Location = new System.Drawing.Point(23, 69);
            this.domainLabel.Name = "domainLabel";
            this.domainLabel.Size = new System.Drawing.Size(43, 13);
            this.domainLabel.TabIndex = 12;
            this.domainLabel.Text = "Domain";
            // 
            // userNameLabel
            // 
            this.userNameLabel.AutoSize = true;
            this.userNameLabel.BackColor = System.Drawing.Color.Transparent;
            this.userNameLabel.Location = new System.Drawing.Point(23, 106);
            this.userNameLabel.Name = "userNameLabel";
            this.userNameLabel.Size = new System.Drawing.Size(62, 13);
            this.userNameLabel.TabIndex = 13;
            this.userNameLabel.Text = "Username *";
            // 
            // passWordLabel
            // 
            this.passWordLabel.AutoSize = true;
            this.passWordLabel.BackColor = System.Drawing.Color.Transparent;
            this.passWordLabel.Location = new System.Drawing.Point(23, 143);
            this.passWordLabel.Name = "passWordLabel";
            this.passWordLabel.Size = new System.Drawing.Size(60, 13);
            this.passWordLabel.TabIndex = 14;
            this.passWordLabel.Text = "Password *";
            // 
            // domainText
            // 
            this.domainText.Location = new System.Drawing.Point(126, 68);
            this.domainText.Name = "domainText";
            this.domainText.Size = new System.Drawing.Size(121, 20);
            this.domainText.TabIndex = 1;
            this.domainText.TextChanged += new System.EventHandler(this.domainText_TextChanged);
            // 
            // userNameText
            // 
            this.userNameText.Location = new System.Drawing.Point(126, 104);
            this.userNameText.Name = "userNameText";
            this.userNameText.Size = new System.Drawing.Size(121, 20);
            this.userNameText.TabIndex = 2;
            this.userNameText.TextChanged += new System.EventHandler(this.userNameText_TextChanged);
            // 
            // passWordText
            // 
            this.passWordText.Location = new System.Drawing.Point(126, 140);
            this.passWordText.Name = "passWordText";
            this.passWordText.PasswordChar = '*';
            this.passWordText.Size = new System.Drawing.Size(121, 20);
            this.passWordText.TabIndex = 3;
            this.passWordText.UseSystemPasswordChar = true;
            this.passWordText.TextChanged += new System.EventHandler(this.passWordText_TextChanged);
            // 
            // addButton
            // 
            this.addButton.Location = new System.Drawing.Point(199, 314);
            this.addButton.Name = "addButton";
            this.addButton.Size = new System.Drawing.Size(75, 23);
            this.addButton.TabIndex = 5;
            this.addButton.Text = "Add";
            this.addButton.UseVisualStyleBackColor = true;
            this.addButton.Click += new System.EventHandler(this.addButton_Click);
            // 
            // cancelButton
            // 
            this.cancelButton.Location = new System.Drawing.Point(280, 314);
            this.cancelButton.Name = "cancelButton";
            this.cancelButton.Size = new System.Drawing.Size(75, 23);
            this.cancelButton.TabIndex = 6;
            this.cancelButton.Text = "Close";
            this.cancelButton.UseVisualStyleBackColor = true;
            this.cancelButton.Click += new System.EventHandler(this.cancelButton_Click);
            // 
            // addCredentialsErrorProvider
            // 
            this.addCredentialsErrorProvider.ContainerControl = this;
            // 
            // useForAllCheckBox
            // 
            this.useForAllCheckBox.AutoSize = true;
            this.useForAllCheckBox.BackColor = System.Drawing.Color.Transparent;
            this.useForAllCheckBox.Location = new System.Drawing.Point(126, 174);
            this.useForAllCheckBox.Name = "useForAllCheckBox";
            this.useForAllCheckBox.Size = new System.Drawing.Size(146, 17);
            this.useForAllCheckBox.TabIndex = 4;
            this.useForAllCheckBox.Text = "Use this credentials for all";
            this.useForAllCheckBox.UseVisualStyleBackColor = false;
            // 
            // noteLabel
            // 
            this.noteLabel.BackColor = System.Drawing.Color.Transparent;
            this.noteLabel.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Italic, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.noteLabel.Location = new System.Drawing.Point(27, 210);
            this.noteLabel.Name = "noteLabel";
            this.noteLabel.Size = new System.Drawing.Size(293, 22);
            this.noteLabel.TabIndex = 24;
            // 
            // credentialHeading
            // 
            this.credentialHeading.BackColor = System.Drawing.Color.Transparent;
            this.credentialHeading.BackgroundImage = ((System.Drawing.Image)(resources.GetObject("credentialHeading.BackgroundImage")));
            this.credentialHeading.Controls.Add(this.credentialsHeadingLabel);
            this.credentialHeading.Location = new System.Drawing.Point(0, 0);
            this.credentialHeading.Name = "credentialHeading";
            this.credentialHeading.Size = new System.Drawing.Size(377, 44);
            this.credentialHeading.TabIndex = 28;
            // 
            // credentialsHeadingLabel
            // 
            this.credentialsHeadingLabel.AutoSize = true;
            this.credentialsHeadingLabel.Font = new System.Drawing.Font("Microsoft Sans Serif", 9.75F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.credentialsHeadingLabel.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(186)))), ((int)(((byte)(61)))), ((int)(((byte)(69)))));
            this.credentialsHeadingLabel.Location = new System.Drawing.Point(10, 11);
            this.credentialsHeadingLabel.Name = "credentialsHeadingLabel";
            this.credentialsHeadingLabel.Size = new System.Drawing.Size(143, 16);
            this.credentialsHeadingLabel.TabIndex = 0;
            this.credentialsHeadingLabel.Text = "Provide credentials";
            // 
            // navaigationPanel
            // 
            this.navaigationPanel.BackColor = System.Drawing.Color.Transparent;
            this.navaigationPanel.BackgroundImage = global::Com.Inmage.Wizard.Properties.Resources.navBar;
            this.navaigationPanel.Location = new System.Drawing.Point(0, 300);
            this.navaigationPanel.Name = "navaigationPanel";
            this.navaigationPanel.Size = new System.Drawing.Size(365, 10);
            this.navaigationPanel.TabIndex = 30;
            // 
            // mandatoryLabel
            // 
            this.mandatoryLabel.AutoSize = true;
            this.mandatoryLabel.BackColor = System.Drawing.Color.Transparent;
            this.mandatoryLabel.ForeColor = System.Drawing.Color.Black;
            this.mandatoryLabel.Location = new System.Drawing.Point(23, 319);
            this.mandatoryLabel.Name = "mandatoryLabel";
            this.mandatoryLabel.Size = new System.Drawing.Size(103, 13);
            this.mandatoryLabel.TabIndex = 143;
            this.mandatoryLabel.Text = "( * ) Mandatory fields";
            // 
            // AddCredentialsPopUpForm
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.BackColor = System.Drawing.Color.White;
            this.BackgroundImage = global::Com.Inmage.Wizard.Properties.Resources.workarea;
            this.ClientSize = new System.Drawing.Size(365, 341);
            this.ControlBox = false;
            this.Controls.Add(this.mandatoryLabel);
            this.Controls.Add(this.navaigationPanel);
            this.Controls.Add(this.credentialHeading);
            this.Controls.Add(this.noteLabel);
            this.Controls.Add(this.useForAllCheckBox);
            this.Controls.Add(this.cancelButton);
            this.Controls.Add(this.addButton);
            this.Controls.Add(this.passWordText);
            this.Controls.Add(this.userNameText);
            this.Controls.Add(this.domainText);
            this.Controls.Add(this.passWordLabel);
            this.Controls.Add(this.userNameLabel);
            this.Controls.Add(this.domainLabel);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.MaximizeBox = false;
            this.MinimizeBox = false;
            this.Name = "AddCredentialsPopUpForm";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
            this.Text = "Please Add Credentials";
            this.HelpButtonClicked += new System.ComponentModel.CancelEventHandler(this.AddCredentialsPopUpForm_HelpButtonClicked);
            ((System.ComponentModel.ISupportInitialize)(this.addCredentialsErrorProvider)).EndInit();
            this.credentialHeading.ResumeLayout(false);
            this.credentialHeading.PerformLayout();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Label domainLabel;
        private System.Windows.Forms.Label userNameLabel;
        private System.Windows.Forms.Label passWordLabel;
        private System.Windows.Forms.Button addButton;
        private System.Windows.Forms.Button cancelButton;
        private System.Windows.Forms.ErrorProvider addCredentialsErrorProvider;
        private System.Windows.Forms.Label noteLabel;
        private System.Windows.Forms.Panel credentialHeading;
        private System.Windows.Forms.Panel navaigationPanel;
        internal System.Windows.Forms.TextBox domainText;
        internal System.Windows.Forms.TextBox userNameText;
        internal System.Windows.Forms.TextBox passWordText;
        internal System.Windows.Forms.CheckBox useForAllCheckBox;
        internal System.Windows.Forms.Label mandatoryLabel;
        internal System.Windows.Forms.Label credentialsHeadingLabel;
    }
}