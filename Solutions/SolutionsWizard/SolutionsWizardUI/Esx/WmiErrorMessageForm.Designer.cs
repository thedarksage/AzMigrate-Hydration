namespace Com.Inmage.Wizard
{
    partial class WmiErrorMessageForm
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
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(WmiErrorMessageForm));
            this.messageTextBox = new System.Windows.Forms.TextBox();
            this.continueButton = new System.Windows.Forms.Button();
            this.credentialsGroupBox = new System.Windows.Forms.GroupBox();
            this.passWordText = new System.Windows.Forms.TextBox();
            this.passWordLabel = new System.Windows.Forms.Label();
            this.userNameText = new System.Windows.Forms.TextBox();
            this.userNameLabel = new System.Windows.Forms.Label();
            this.domainText = new System.Windows.Forms.TextBox();
            this.domainLabel = new System.Windows.Forms.Label();
            this.chooseLabel = new System.Windows.Forms.Label();
            this.skipWmiValidationsRadioButton = new System.Windows.Forms.RadioButton();
            this.retryRadioButton = new System.Windows.Forms.RadioButton();
            this.cancelButton = new System.Windows.Forms.Button();
            this.credentialsGroupBox.SuspendLayout();
            this.SuspendLayout();
            // 
            // messageTextBox
            // 
            this.messageTextBox.BackColor = System.Drawing.Color.AliceBlue;
            this.messageTextBox.Location = new System.Drawing.Point(52, 7);
            this.messageTextBox.Multiline = true;
            this.messageTextBox.Name = "messageTextBox";
            this.messageTextBox.ReadOnly = true;
            this.messageTextBox.ScrollBars = System.Windows.Forms.ScrollBars.Vertical;
            this.messageTextBox.Size = new System.Drawing.Size(533, 159);
            this.messageTextBox.TabIndex = 25;
            // 
            // continueButton
            // 
            this.continueButton.Enabled = false;
            this.continueButton.Location = new System.Drawing.Point(458, 321);
            this.continueButton.Name = "continueButton";
            this.continueButton.Size = new System.Drawing.Size(75, 23);
            this.continueButton.TabIndex = 11;
            this.continueButton.Text = "Continue";
            this.continueButton.UseVisualStyleBackColor = true;
            this.continueButton.Click += new System.EventHandler(this.continueButton_Click);
            // 
            // credentialsGroupBox
            // 
            this.credentialsGroupBox.Controls.Add(this.passWordText);
            this.credentialsGroupBox.Controls.Add(this.passWordLabel);
            this.credentialsGroupBox.Controls.Add(this.userNameText);
            this.credentialsGroupBox.Controls.Add(this.userNameLabel);
            this.credentialsGroupBox.Controls.Add(this.domainText);
            this.credentialsGroupBox.Controls.Add(this.domainLabel);
            this.credentialsGroupBox.Location = new System.Drawing.Point(122, 233);
            this.credentialsGroupBox.Name = "credentialsGroupBox";
            this.credentialsGroupBox.Size = new System.Drawing.Size(363, 82);
            this.credentialsGroupBox.TabIndex = 7;
            this.credentialsGroupBox.TabStop = false;
            // 
            // passWordText
            // 
            this.passWordText.Location = new System.Drawing.Point(258, 32);
            this.passWordText.Name = "passWordText";
            this.passWordText.PasswordChar = '*';
            this.passWordText.Size = new System.Drawing.Size(99, 20);
            this.passWordText.TabIndex = 10;
            this.passWordText.UseSystemPasswordChar = true;
            // 
            // passWordLabel
            // 
            this.passWordLabel.AutoSize = true;
            this.passWordLabel.BackColor = System.Drawing.Color.Transparent;
            this.passWordLabel.Location = new System.Drawing.Point(259, 14);
            this.passWordLabel.Name = "passWordLabel";
            this.passWordLabel.Size = new System.Drawing.Size(60, 13);
            this.passWordLabel.TabIndex = 18;
            this.passWordLabel.Text = "Password *";
            // 
            // userNameText
            // 
            this.userNameText.Location = new System.Drawing.Point(137, 32);
            this.userNameText.Name = "userNameText";
            this.userNameText.Size = new System.Drawing.Size(115, 20);
            this.userNameText.TabIndex = 9;
            // 
            // userNameLabel
            // 
            this.userNameLabel.AutoSize = true;
            this.userNameLabel.BackColor = System.Drawing.Color.Transparent;
            this.userNameLabel.Location = new System.Drawing.Point(134, 14);
            this.userNameLabel.Name = "userNameLabel";
            this.userNameLabel.Size = new System.Drawing.Size(62, 13);
            this.userNameLabel.TabIndex = 16;
            this.userNameLabel.Text = "Username *";
            // 
            // domainText
            // 
            this.domainText.Location = new System.Drawing.Point(18, 32);
            this.domainText.Name = "domainText";
            this.domainText.Size = new System.Drawing.Size(107, 20);
            this.domainText.TabIndex = 8;
            // 
            // domainLabel
            // 
            this.domainLabel.AutoSize = true;
            this.domainLabel.BackColor = System.Drawing.Color.Transparent;
            this.domainLabel.Location = new System.Drawing.Point(15, 16);
            this.domainLabel.Name = "domainLabel";
            this.domainLabel.Size = new System.Drawing.Size(43, 13);
            this.domainLabel.TabIndex = 14;
            this.domainLabel.Text = "Domain";
            // 
            // chooseLabel
            // 
            this.chooseLabel.AutoSize = true;
            this.chooseLabel.BackColor = System.Drawing.Color.Transparent;
            this.chooseLabel.Location = new System.Drawing.Point(50, 182);
            this.chooseLabel.Name = "chooseLabel";
            this.chooseLabel.Size = new System.Drawing.Size(471, 26);
            this.chooseLabel.TabIndex = 19;
            this.chooseLabel.Text = "You can still proceed with this error. However  \"Remote agent installation\" featu" +
    "re will be disabled. \r\nChoose one of the options below:";
            // 
            // skipWmiValidationsRadioButton
            // 
            this.skipWmiValidationsRadioButton.AutoSize = true;
            this.skipWmiValidationsRadioButton.BackColor = System.Drawing.Color.Transparent;
            this.skipWmiValidationsRadioButton.Location = new System.Drawing.Point(52, 214);
            this.skipWmiValidationsRadioButton.Name = "skipWmiValidationsRadioButton";
            this.skipWmiValidationsRadioButton.Size = new System.Drawing.Size(270, 17);
            this.skipWmiValidationsRadioButton.TabIndex = 5;
            this.skipWmiValidationsRadioButton.TabStop = true;
            this.skipWmiValidationsRadioButton.Text = "Disable \"Remote agent install\" feature and continue\r\n";
            this.skipWmiValidationsRadioButton.UseVisualStyleBackColor = false;
            this.skipWmiValidationsRadioButton.CheckedChanged += new System.EventHandler(this.skipWmiValidationsRadioButton_CheckedChanged);
            // 
            // retryRadioButton
            // 
            this.retryRadioButton.AutoSize = true;
            this.retryRadioButton.BackColor = System.Drawing.Color.Transparent;
            this.retryRadioButton.Location = new System.Drawing.Point(52, 244);
            this.retryRadioButton.Name = "retryRadioButton";
            this.retryRadioButton.Size = new System.Drawing.Size(53, 17);
            this.retryRadioButton.TabIndex = 6;
            this.retryRadioButton.TabStop = true;
            this.retryRadioButton.Text = "Retry ";
            this.retryRadioButton.UseVisualStyleBackColor = false;
            this.retryRadioButton.CheckedChanged += new System.EventHandler(this.retryRadioButton_CheckedChanged);
            // 
            // cancelButton
            // 
            this.cancelButton.Location = new System.Drawing.Point(539, 321);
            this.cancelButton.Name = "cancelButton";
            this.cancelButton.Size = new System.Drawing.Size(75, 23);
            this.cancelButton.TabIndex = 12;
            this.cancelButton.Text = "Cancel";
            this.cancelButton.UseVisualStyleBackColor = true;
            this.cancelButton.Click += new System.EventHandler(this.cancelButton_Click);
            // 
            // WmiErrorMessageForm
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.BackColor = System.Drawing.Color.White;
            this.BackgroundImage = global::Com.Inmage.Wizard.Properties.Resources.workarea;
            this.ClientSize = new System.Drawing.Size(627, 348);
            this.ControlBox = false;
            this.Controls.Add(this.cancelButton);
            this.Controls.Add(this.retryRadioButton);
            this.Controls.Add(this.skipWmiValidationsRadioButton);
            this.Controls.Add(this.chooseLabel);
            this.Controls.Add(this.credentialsGroupBox);
            this.Controls.Add(this.continueButton);
            this.Controls.Add(this.messageTextBox);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedSingle;
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.Name = "WmiErrorMessageForm";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
            this.Text = "Error";
            this.credentialsGroupBox.ResumeLayout(false);
            this.credentialsGroupBox.PerformLayout();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.TextBox messageTextBox;
        private System.Windows.Forms.GroupBox credentialsGroupBox;
        private System.Windows.Forms.Label domainLabel;
        private System.Windows.Forms.Label userNameLabel;
        private System.Windows.Forms.Label passWordLabel;
        internal System.Windows.Forms.Button continueButton;
        internal System.Windows.Forms.TextBox domainText;
        internal System.Windows.Forms.TextBox userNameText;
        internal System.Windows.Forms.TextBox passWordText;
        internal System.Windows.Forms.Button cancelButton;
        internal System.Windows.Forms.Label chooseLabel;
        internal System.Windows.Forms.RadioButton skipWmiValidationsRadioButton;
        internal System.Windows.Forms.RadioButton retryRadioButton;
    }
}