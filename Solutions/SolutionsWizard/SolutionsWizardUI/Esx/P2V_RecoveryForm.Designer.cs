namespace com.InMage.Wizard
{
    partial class P2V_RecoveryForm
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
            this.recoverPanel = new System.Windows.Forms.Panel();
            this.tagComboBox = new System.Windows.Forms.ComboBox();
            this.tagLabel = new System.Windows.Forms.Label();
            this.recoveryTagText = new System.Windows.Forms.TextBox();
            this.recoveryTagLabel = new System.Windows.Forms.Label();
            this.defaultGateWayText = new System.Windows.Forms.TextBox();
            this.subnetMaskText = new System.Windows.Forms.TextBox();
            this.defaultGateWayLabel = new System.Windows.Forms.Label();
            this.subnetMaskLabel = new System.Windows.Forms.Label();
            this.machineConfigurationLabel = new System.Windows.Forms.Label();
            this.ipText = new System.Windows.Forms.TextBox();
            this.ipLabel = new System.Windows.Forms.Label();
            this.targetEsxIPText = new System.Windows.Forms.TextBox();
            this.teargetEsxIPLabel = new System.Windows.Forms.Label();
            this.targetEsxPasswordLabel = new System.Windows.Forms.Label();
            this.targetEsxPasswordTextBox = new System.Windows.Forms.TextBox();
            this.targetEsxUserNameTextBox = new System.Windows.Forms.TextBox();
            this.targetEsxUserNameLabel = new System.Windows.Forms.Label();
            this.p2vRecoverDataGridView = new System.Windows.Forms.DataGridView();
            this.nextButton = new System.Windows.Forms.Button();
            this.cancelButton = new System.Windows.Forms.Button();
            this.linePanel = new System.Windows.Forms.Panel();
            this.rightHeaderPictureBox = new System.Windows.Forms.PictureBox();
            this.headerPictureBox = new System.Windows.Forms.PictureBox();
            this.applicationPanel = new System.Windows.Forms.Panel();
            this.p2vHeadingLabel = new System.Windows.Forms.Label();
            this.addCredentialsLabel = new System.Windows.Forms.Label();
            this.credentialPictureBox = new System.Windows.Forms.PictureBox();
            this.doneButton = new System.Windows.Forms.Button();
            this.errorProvider = new System.Windows.Forms.ErrorProvider(this.components);
            this.getDetailsButton = new System.Windows.Forms.Button();
            this.dnsLabel = new System.Windows.Forms.Label();
            this.dnsText = new System.Windows.Forms.TextBox();
            this.checkBox = new System.Windows.Forms.CheckBox();
            this.hostName = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.os = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.recover = new System.Windows.Forms.DataGridViewCheckBoxColumn();
            this.RecoveryOrder = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.recoverPanel.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.p2vRecoverDataGridView)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.rightHeaderPictureBox)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.headerPictureBox)).BeginInit();
            this.applicationPanel.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.credentialPictureBox)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.errorProvider)).BeginInit();
            this.SuspendLayout();
            // 
            // recoverPanel
            // 
            this.recoverPanel.Controls.Add(this.checkBox);
            this.recoverPanel.Controls.Add(this.dnsText);
            this.recoverPanel.Controls.Add(this.dnsLabel);
            this.recoverPanel.Controls.Add(this.getDetailsButton);
            this.recoverPanel.Controls.Add(this.tagComboBox);
            this.recoverPanel.Controls.Add(this.tagLabel);
            this.recoverPanel.Controls.Add(this.recoveryTagText);
            this.recoverPanel.Controls.Add(this.recoveryTagLabel);
            this.recoverPanel.Controls.Add(this.defaultGateWayText);
            this.recoverPanel.Controls.Add(this.subnetMaskText);
            this.recoverPanel.Controls.Add(this.defaultGateWayLabel);
            this.recoverPanel.Controls.Add(this.subnetMaskLabel);
            this.recoverPanel.Controls.Add(this.machineConfigurationLabel);
            this.recoverPanel.Controls.Add(this.ipText);
            this.recoverPanel.Controls.Add(this.ipLabel);
            this.recoverPanel.Controls.Add(this.targetEsxIPText);
            this.recoverPanel.Controls.Add(this.teargetEsxIPLabel);
            this.recoverPanel.Controls.Add(this.targetEsxPasswordLabel);
            this.recoverPanel.Controls.Add(this.targetEsxPasswordTextBox);
            this.recoverPanel.Controls.Add(this.targetEsxUserNameTextBox);
            this.recoverPanel.Controls.Add(this.targetEsxUserNameLabel);
            this.recoverPanel.Controls.Add(this.p2vRecoverDataGridView);
            this.recoverPanel.Location = new System.Drawing.Point(177, 46);
            this.recoverPanel.Name = "recoverPanel";
            this.recoverPanel.Size = new System.Drawing.Size(673, 397);
            this.recoverPanel.TabIndex = 111;
            // 
            // tagComboBox
            // 
            this.tagComboBox.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.tagComboBox.FormattingEnabled = true;
            this.tagComboBox.Location = new System.Drawing.Point(474, 97);
            this.tagComboBox.Name = "tagComboBox";
            this.tagComboBox.Size = new System.Drawing.Size(121, 21);
            this.tagComboBox.TabIndex = 28;
            // 
            // tagLabel
            // 
            this.tagLabel.AutoSize = true;
            this.tagLabel.Location = new System.Drawing.Point(373, 97);
            this.tagLabel.Name = "tagLabel";
            this.tagLabel.Size = new System.Drawing.Size(53, 13);
            this.tagLabel.TabIndex = 27;
            this.tagLabel.Text = "Tag Type";
            // 
            // recoveryTagText
            // 
            this.recoveryTagText.Location = new System.Drawing.Point(474, 131);
            this.recoveryTagText.Name = "recoveryTagText";
            this.recoveryTagText.Size = new System.Drawing.Size(100, 20);
            this.recoveryTagText.TabIndex = 26;
            this.recoveryTagText.Text = "LATEST";
            // 
            // recoveryTagLabel
            // 
            this.recoveryTagLabel.AutoSize = true;
            this.recoveryTagLabel.Location = new System.Drawing.Point(373, 138);
            this.recoveryTagLabel.Name = "recoveryTagLabel";
            this.recoveryTagLabel.Size = new System.Drawing.Size(26, 13);
            this.recoveryTagLabel.TabIndex = 25;
            this.recoveryTagLabel.Text = "Tag";
            // 
            // defaultGateWayText
            // 
            this.defaultGateWayText.Location = new System.Drawing.Point(474, 291);
            this.defaultGateWayText.Name = "defaultGateWayText";
            this.defaultGateWayText.Size = new System.Drawing.Size(100, 20);
            this.defaultGateWayText.TabIndex = 31;
            this.defaultGateWayText.Visible = false;
            // 
            // subnetMaskText
            // 
            this.subnetMaskText.Location = new System.Drawing.Point(474, 253);
            this.subnetMaskText.Name = "subnetMaskText";
            this.subnetMaskText.Size = new System.Drawing.Size(100, 20);
            this.subnetMaskText.TabIndex = 30;
            this.subnetMaskText.Visible = false;
            // 
            // defaultGateWayLabel
            // 
            this.defaultGateWayLabel.AutoSize = true;
            this.defaultGateWayLabel.Location = new System.Drawing.Point(370, 298);
            this.defaultGateWayLabel.Name = "defaultGateWayLabel";
            this.defaultGateWayLabel.Size = new System.Drawing.Size(86, 13);
            this.defaultGateWayLabel.TabIndex = 22;
            this.defaultGateWayLabel.Text = "Default Gateway";
            this.defaultGateWayLabel.Visible = false;
            // 
            // subnetMaskLabel
            // 
            this.subnetMaskLabel.AutoSize = true;
            this.subnetMaskLabel.Location = new System.Drawing.Point(370, 260);
            this.subnetMaskLabel.Name = "subnetMaskLabel";
            this.subnetMaskLabel.Size = new System.Drawing.Size(70, 13);
            this.subnetMaskLabel.TabIndex = 21;
            this.subnetMaskLabel.Text = "Subnet Mask";
            this.subnetMaskLabel.Visible = false;
            // 
            // machineConfigurationLabel
            // 
            this.machineConfigurationLabel.AutoSize = true;
            this.machineConfigurationLabel.Location = new System.Drawing.Point(370, 193);
            this.machineConfigurationLabel.Name = "machineConfigurationLabel";
            this.machineConfigurationLabel.Size = new System.Drawing.Size(133, 13);
            this.machineConfigurationLabel.TabIndex = 20;
            this.machineConfigurationLabel.Text = "Change Networks Settings";
            this.machineConfigurationLabel.Visible = false;
            // 
            // ipText
            // 
            this.ipText.Location = new System.Drawing.Point(474, 223);
            this.ipText.Name = "ipText";
            this.ipText.Size = new System.Drawing.Size(100, 20);
            this.ipText.TabIndex = 29;
            this.ipText.Visible = false;
            // 
            // ipLabel
            // 
            this.ipLabel.AutoSize = true;
            this.ipLabel.Location = new System.Drawing.Point(370, 226);
            this.ipLabel.Name = "ipLabel";
            this.ipLabel.Size = new System.Drawing.Size(17, 13);
            this.ipLabel.TabIndex = 18;
            this.ipLabel.Text = "IP";
            this.ipLabel.Visible = false;
            // 
            // targetEsxIPText
            // 
            this.targetEsxIPText.Location = new System.Drawing.Point(82, 22);
            this.targetEsxIPText.Name = "targetEsxIPText";
            this.targetEsxIPText.Size = new System.Drawing.Size(100, 20);
            this.targetEsxIPText.TabIndex = 17;
            // 
            // teargetEsxIPLabel
            // 
            this.teargetEsxIPLabel.AutoSize = true;
            this.teargetEsxIPLabel.Location = new System.Drawing.Point(5, 25);
            this.teargetEsxIPLabel.Name = "teargetEsxIPLabel";
            this.teargetEsxIPLabel.Size = new System.Drawing.Size(71, 13);
            this.teargetEsxIPLabel.TabIndex = 16;
            this.teargetEsxIPLabel.Text = "Target Esx IP";
            // 
            // targetEsxPasswordLabel
            // 
            this.targetEsxPasswordLabel.AutoSize = true;
            this.targetEsxPasswordLabel.Location = new System.Drawing.Point(390, 29);
            this.targetEsxPasswordLabel.Name = "targetEsxPasswordLabel";
            this.targetEsxPasswordLabel.Size = new System.Drawing.Size(53, 13);
            this.targetEsxPasswordLabel.TabIndex = 15;
            this.targetEsxPasswordLabel.Text = "Password";
            // 
            // targetEsxPasswordTextBox
            // 
            this.targetEsxPasswordTextBox.Location = new System.Drawing.Point(449, 22);
            this.targetEsxPasswordTextBox.Name = "targetEsxPasswordTextBox";
            this.targetEsxPasswordTextBox.PasswordChar = '*';
            this.targetEsxPasswordTextBox.Size = new System.Drawing.Size(100, 20);
            this.targetEsxPasswordTextBox.TabIndex = 19;
            this.targetEsxPasswordTextBox.Enter += new System.EventHandler(this.targetEsxPasswordTextBox_Enter);
            // 
            // targetEsxUserNameTextBox
            // 
            this.targetEsxUserNameTextBox.Location = new System.Drawing.Point(269, 22);
            this.targetEsxUserNameTextBox.Name = "targetEsxUserNameTextBox";
            this.targetEsxUserNameTextBox.Size = new System.Drawing.Size(100, 20);
            this.targetEsxUserNameTextBox.TabIndex = 18;
            // 
            // targetEsxUserNameLabel
            // 
            this.targetEsxUserNameLabel.AutoSize = true;
            this.targetEsxUserNameLabel.Location = new System.Drawing.Point(208, 29);
            this.targetEsxUserNameLabel.Name = "targetEsxUserNameLabel";
            this.targetEsxUserNameLabel.Size = new System.Drawing.Size(55, 13);
            this.targetEsxUserNameLabel.TabIndex = 12;
            this.targetEsxUserNameLabel.Text = "Username";
            // 
            // p2vRecoverDataGridView
            // 
            this.p2vRecoverDataGridView.AllowUserToAddRows = false;
            this.p2vRecoverDataGridView.BackgroundColor = System.Drawing.Color.White;
            this.p2vRecoverDataGridView.BorderStyle = System.Windows.Forms.BorderStyle.None;
            this.p2vRecoverDataGridView.ColumnHeadersHeightSizeMode = System.Windows.Forms.DataGridViewColumnHeadersHeightSizeMode.AutoSize;
            this.p2vRecoverDataGridView.Columns.AddRange(new System.Windows.Forms.DataGridViewColumn[] {
            this.hostName,
            this.os,
            this.recover,
            this.RecoveryOrder});
            this.p2vRecoverDataGridView.Location = new System.Drawing.Point(5, 122);
            this.p2vRecoverDataGridView.Name = "p2vRecoverDataGridView";
            this.p2vRecoverDataGridView.RowHeadersVisible = false;
            this.p2vRecoverDataGridView.Size = new System.Drawing.Size(349, 189);
            this.p2vRecoverDataGridView.TabIndex = 5;
            this.p2vRecoverDataGridView.CellValueChanged += new System.Windows.Forms.DataGridViewCellEventHandler(this.p2vRecoverDataGridView_CellValueChanged);
            this.p2vRecoverDataGridView.CellContentClick += new System.Windows.Forms.DataGridViewCellEventHandler(this.p2vRecoverDataGridView_CellContentClick);
            // 
            // nextButton
            // 
            this.nextButton.Location = new System.Drawing.Point(662, 449);
            this.nextButton.Name = "nextButton";
            this.nextButton.Size = new System.Drawing.Size(75, 23);
            this.nextButton.TabIndex = 110;
            this.nextButton.Text = "Next >";
            this.nextButton.UseVisualStyleBackColor = true;
            this.nextButton.Click += new System.EventHandler(this.nextButton_Click);
            // 
            // cancelButton
            // 
            this.cancelButton.Location = new System.Drawing.Point(766, 449);
            this.cancelButton.Name = "cancelButton";
            this.cancelButton.Size = new System.Drawing.Size(75, 23);
            this.cancelButton.TabIndex = 109;
            this.cancelButton.Text = "Cancel";
            this.cancelButton.UseVisualStyleBackColor = true;
            this.cancelButton.Click += new System.EventHandler(this.cancelButton_Click);
            // 
            // linePanel
            // 
            this.linePanel.BackColor = System.Drawing.Color.DarkRed;
            this.linePanel.Location = new System.Drawing.Point(-20, 39);
            this.linePanel.Name = "linePanel";
            this.linePanel.Size = new System.Drawing.Size(888, 1);
            this.linePanel.TabIndex = 108;
            // 
            // rightHeaderPictureBox
            // 
            this.rightHeaderPictureBox.Image = global::com.InMage.Wizard.Properties.Resources.logoRight;
            this.rightHeaderPictureBox.Location = new System.Drawing.Point(464, -3);
            this.rightHeaderPictureBox.Name = "rightHeaderPictureBox";
            this.rightHeaderPictureBox.Size = new System.Drawing.Size(335, 39);
            this.rightHeaderPictureBox.TabIndex = 107;
            this.rightHeaderPictureBox.TabStop = false;
            // 
            // headerPictureBox
            // 
            this.headerPictureBox.Image = global::com.InMage.Wizard.Properties.Resources.logo;
            this.headerPictureBox.Location = new System.Drawing.Point(3, 1);
            this.headerPictureBox.Name = "headerPictureBox";
            this.headerPictureBox.Size = new System.Drawing.Size(267, 37);
            this.headerPictureBox.TabIndex = 106;
            this.headerPictureBox.TabStop = false;
            // 
            // applicationPanel
            // 
            this.applicationPanel.BackgroundImage = global::com.InMage.Wizard.Properties.Resources.sourcetarget;
            this.applicationPanel.Controls.Add(this.p2vHeadingLabel);
            this.applicationPanel.Controls.Add(this.addCredentialsLabel);
            this.applicationPanel.Controls.Add(this.credentialPictureBox);
            this.applicationPanel.Location = new System.Drawing.Point(3, 42);
            this.applicationPanel.Name = "applicationPanel";
            this.applicationPanel.Size = new System.Drawing.Size(173, 440);
            this.applicationPanel.TabIndex = 113;
            // 
            // p2vHeadingLabel
            // 
            this.p2vHeadingLabel.AutoSize = true;
            this.p2vHeadingLabel.BackColor = System.Drawing.Color.Transparent;
            this.p2vHeadingLabel.Font = new System.Drawing.Font("Microsoft Sans Serif", 10F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.p2vHeadingLabel.ForeColor = System.Drawing.SystemColors.ActiveCaptionText;
            this.p2vHeadingLabel.Location = new System.Drawing.Point(27, 21);
            this.p2vHeadingLabel.Name = "p2vHeadingLabel";
            this.p2vHeadingLabel.Size = new System.Drawing.Size(76, 17);
            this.p2vHeadingLabel.TabIndex = 88;
            this.p2vHeadingLabel.Text = "Recovery";
            // 
            // addCredentialsLabel
            // 
            this.addCredentialsLabel.AutoSize = true;
            this.addCredentialsLabel.BackColor = System.Drawing.Color.Transparent;
            this.addCredentialsLabel.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.addCredentialsLabel.Location = new System.Drawing.Point(37, 83);
            this.addCredentialsLabel.Name = "addCredentialsLabel";
            this.addCredentialsLabel.Size = new System.Drawing.Size(95, 13);
            this.addCredentialsLabel.TabIndex = 89;
            this.addCredentialsLabel.Text = "Select Machine";
            this.addCredentialsLabel.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
            // 
            // credentialPictureBox
            // 
            this.credentialPictureBox.BackColor = System.Drawing.Color.Transparent;
            this.credentialPictureBox.Image = global::com.InMage.Wizard.Properties.Resources.arrow_right;
            this.credentialPictureBox.Location = new System.Drawing.Point(3, 78);
            this.credentialPictureBox.Name = "credentialPictureBox";
            this.credentialPictureBox.Size = new System.Drawing.Size(38, 24);
            this.credentialPictureBox.SizeMode = System.Windows.Forms.PictureBoxSizeMode.CenterImage;
            this.credentialPictureBox.TabIndex = 88;
            this.credentialPictureBox.TabStop = false;
            // 
            // doneButton
            // 
            this.doneButton.Location = new System.Drawing.Point(766, 449);
            this.doneButton.Name = "doneButton";
            this.doneButton.Size = new System.Drawing.Size(75, 23);
            this.doneButton.TabIndex = 114;
            this.doneButton.Text = "Done";
            this.doneButton.UseVisualStyleBackColor = true;
            this.doneButton.Visible = false;
            this.doneButton.Click += new System.EventHandler(this.doneButton_Click);
            // 
            // errorProvider
            // 
            this.errorProvider.ContainerControl = this;
            // 
            // getDetailsButton
            // 
            this.getDetailsButton.Location = new System.Drawing.Point(555, 19);
            this.getDetailsButton.Name = "getDetailsButton";
            this.getDetailsButton.Size = new System.Drawing.Size(75, 23);
            this.getDetailsButton.TabIndex = 20;
            this.getDetailsButton.Text = "Get Details";
            this.getDetailsButton.UseVisualStyleBackColor = true;
            this.getDetailsButton.Click += new System.EventHandler(this.getDetailsButton_Click);
            // 
            // dnsLabel
            // 
            this.dnsLabel.AutoSize = true;
            this.dnsLabel.Location = new System.Drawing.Point(373, 337);
            this.dnsLabel.Name = "dnsLabel";
            this.dnsLabel.Size = new System.Drawing.Size(30, 13);
            this.dnsLabel.TabIndex = 32;
            this.dnsLabel.Text = "DNS";
            this.dnsLabel.Visible = false;
            // 
            // dnsText
            // 
            this.dnsText.Location = new System.Drawing.Point(474, 329);
            this.dnsText.Name = "dnsText";
            this.dnsText.Size = new System.Drawing.Size(100, 20);
            this.dnsText.TabIndex = 33;
            this.dnsText.Visible = false;
            // 
            // checkBox
            // 
            this.checkBox.AutoSize = true;
            this.checkBox.Location = new System.Drawing.Point(509, 193);
            this.checkBox.Name = "checkBox";
            this.checkBox.Size = new System.Drawing.Size(15, 14);
            this.checkBox.TabIndex = 34;
            this.checkBox.UseVisualStyleBackColor = true;
            this.checkBox.Visible = false;
            this.checkBox.CheckedChanged += new System.EventHandler(this.checkBox_CheckedChanged);
            // 
            // hostName
            // 
            this.hostName.HeaderText = "Host Name";
            this.hostName.Name = "hostName";
            this.hostName.ReadOnly = true;
            this.hostName.Width = 95;
            // 
            // os
            // 
            this.os.AutoSizeMode = System.Windows.Forms.DataGridViewAutoSizeColumnMode.None;
            this.os.HeaderText = "OS";
            this.os.Name = "os";
            // 
            // recover
            // 
            this.recover.AutoSizeMode = System.Windows.Forms.DataGridViewAutoSizeColumnMode.None;
            this.recover.HeaderText = "Recover";
            this.recover.Name = "recover";
            this.recover.Resizable = System.Windows.Forms.DataGridViewTriState.True;
            this.recover.SortMode = System.Windows.Forms.DataGridViewColumnSortMode.Automatic;
            this.recover.Width = 50;
            // 
            // RecoveryOrder
            // 
            this.RecoveryOrder.HeaderText = "Recovery Order";
            this.RecoveryOrder.Name = "RecoveryOrder";
            this.RecoveryOrder.Resizable = System.Windows.Forms.DataGridViewTriState.True;
            this.RecoveryOrder.SortMode = System.Windows.Forms.DataGridViewColumnSortMode.NotSortable;
            // 
            // P2V_RecoveryForm
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.BackColor = System.Drawing.Color.White;
            this.ClientSize = new System.Drawing.Size(862, 483);
            this.Controls.Add(this.applicationPanel);
            this.Controls.Add(this.recoverPanel);
            this.Controls.Add(this.nextButton);
            this.Controls.Add(this.linePanel);
            this.Controls.Add(this.rightHeaderPictureBox);
            this.Controls.Add(this.headerPictureBox);
            this.Controls.Add(this.cancelButton);
            this.Controls.Add(this.doneButton);
            this.Name = "P2V_RecoveryForm";
            this.Text = "Bare Metal Recovery Wizard";
            this.recoverPanel.ResumeLayout(false);
            this.recoverPanel.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this.p2vRecoverDataGridView)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.rightHeaderPictureBox)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.headerPictureBox)).EndInit();
            this.applicationPanel.ResumeLayout(false);
            this.applicationPanel.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this.credentialPictureBox)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.errorProvider)).EndInit();
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.Panel recoverPanel;
        private System.Windows.Forms.DataGridView p2vRecoverDataGridView;
        public System.Windows.Forms.Button nextButton;
        public System.Windows.Forms.Button cancelButton;
        private System.Windows.Forms.Panel linePanel;
        private System.Windows.Forms.PictureBox rightHeaderPictureBox;
        private System.Windows.Forms.PictureBox headerPictureBox;
        private System.Windows.Forms.Panel applicationPanel;
        private System.Windows.Forms.Label p2vHeadingLabel;
        private System.Windows.Forms.Label addCredentialsLabel;
        private System.Windows.Forms.PictureBox credentialPictureBox;
        private System.Windows.Forms.Button doneButton;
        public System.Windows.Forms.ErrorProvider errorProvider;
        public System.Windows.Forms.TextBox targetEsxIPText;
        private System.Windows.Forms.Label teargetEsxIPLabel;
        private System.Windows.Forms.Label targetEsxPasswordLabel;
        private System.Windows.Forms.TextBox targetEsxPasswordTextBox;
        private System.Windows.Forms.TextBox targetEsxUserNameTextBox;
        private System.Windows.Forms.Label targetEsxUserNameLabel;
        private System.Windows.Forms.TextBox ipText;
        private System.Windows.Forms.Label ipLabel;
        private System.Windows.Forms.Label recoveryTagLabel;
        private System.Windows.Forms.TextBox defaultGateWayText;
        private System.Windows.Forms.TextBox subnetMaskText;
        private System.Windows.Forms.Label defaultGateWayLabel;
        private System.Windows.Forms.Label subnetMaskLabel;
        private System.Windows.Forms.Label machineConfigurationLabel;
        private System.Windows.Forms.TextBox recoveryTagText;
        private System.Windows.Forms.ComboBox tagComboBox;
        private System.Windows.Forms.Label tagLabel;
        private System.Windows.Forms.Button getDetailsButton;
        private System.Windows.Forms.CheckBox checkBox;
        private System.Windows.Forms.TextBox dnsText;
        private System.Windows.Forms.Label dnsLabel;
        private System.Windows.Forms.DataGridViewTextBoxColumn hostName;
        private System.Windows.Forms.DataGridViewTextBoxColumn os;
        private System.Windows.Forms.DataGridViewCheckBoxColumn recover;
        private System.Windows.Forms.DataGridViewTextBoxColumn RecoveryOrder;
    }
}