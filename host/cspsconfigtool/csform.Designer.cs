namespace cspsconfigtool
{
    partial class csform
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
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(csform));
            System.Windows.Forms.DataGridViewCellStyle dataGridViewCellStyle1 = new System.Windows.Forms.DataGridViewCellStyle();
            this.closeButton = new System.Windows.Forms.Button();
            this.tabControl1 = new System.Windows.Forms.TabControl();
            this.manageAccountsTabPage = new System.Windows.Forms.TabPage();
            this.progressLabel = new System.Windows.Forms.Label();
            this.headingLabel = new System.Windows.Forms.Label();
            this.messageLabel = new System.Windows.Forms.Label();
            this.accountsDataGridView = new System.Windows.Forms.DataGridView();
            this.ColumnFriendlyName = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.ColumnUsername = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.editButton = new System.Windows.Forms.Button();
            this.addButton = new System.Windows.Forms.Button();
            this.deleteButton = new System.Windows.Forms.Button();
            this.vaultRegistrationTabPage = new System.Windows.Forms.TabPage();
            this.CustomProxyGroupBox = new System.Windows.Forms.GroupBox();
            this.ProxyAddressTextBox = new System.Windows.Forms.TextBox();
            this.ProxyAuthCheckBox = new System.Windows.Forms.CheckBox();
            this.ProxyPasswordTextBox = new System.Windows.Forms.TextBox();
            this.ProxyPortTextBox = new System.Windows.Forms.TextBox();
            this.ProxyUserNameLabel = new System.Windows.Forms.Label();
            this.ProxyPasswordLabel = new System.Windows.Forms.Label();
            this.ProxyPortLabel = new System.Windows.Forms.Label();
            this.ProxyAddressLabel = new System.Windows.Forms.Label();
            this.ProxyUserNameTextBox = new System.Windows.Forms.TextBox();
            this.CustomProxyConnectRadiobutton = new System.Windows.Forms.RadioButton();
            this.BypassProxyConnectionRadiobutton = new System.Windows.Forms.RadioButton();
            this.label3 = new System.Windows.Forms.Label();
            this.browsebutton = new System.Windows.Forms.Button();
            this.regbutton = new System.Windows.Forms.Button();
            this.textBox1 = new System.Windows.Forms.TextBox();
            this.label1 = new System.Windows.Forms.Label();
            this.localizationTabPage = new System.Windows.Forms.TabPage();
            this.saveButton = new System.Windows.Forms.Button();
            this.langComboBox = new System.Windows.Forms.ComboBox();
            this.label4 = new System.Windows.Forms.Label();
            this.label2 = new System.Windows.Forms.Label();
            this.configurationDetailsTabPage = new System.Windows.Forms.TabPage();
            this.groupBox1 = new System.Windows.Forms.GroupBox();
            this.label9 = new System.Windows.Forms.Label();
            this.label8 = new System.Windows.Forms.Label();
            this.label7 = new System.Windows.Forms.Label();
            this.azureCsIpComboBox = new System.Windows.Forms.ComboBox();
            this.dvacplinkLabel = new System.Windows.Forms.LinkLabel();
            this.label6 = new System.Windows.Forms.Label();
            this.textBox6 = new System.Windows.Forms.TextBox();
            this.label5 = new System.Windows.Forms.Label();
            this.configsavebutton = new System.Windows.Forms.Button();
            this.label16 = new System.Windows.Forms.Label();
            this.textBox5 = new System.Windows.Forms.TextBox();
            this.label15 = new System.Windows.Forms.Label();
            this.signatureVerificationNoteLabel = new System.Windows.Forms.Label();
            this.signatureVerificationCheckBox = new System.Windows.Forms.CheckBox();
            this.label11 = new System.Windows.Forms.Label();
            this.label10 = new System.Windows.Forms.Label();
            this.textBox3 = new System.Windows.Forms.TextBox();
            this.label12 = new System.Windows.Forms.Label();
            this.textBox2 = new System.Windows.Forms.TextBox();
            this.label13 = new System.Windows.Forms.Label();
            this.onPremCsIpText = new System.Windows.Forms.TextBox();
            this.label14 = new System.Windows.Forms.Label();
            this.deleteBackgroundWorker = new System.ComponentModel.BackgroundWorker();
            this.loadBackgroundWorker = new System.ComponentModel.BackgroundWorker();
            this.ASRRegInProgressLabel = new System.Windows.Forms.Label();
            this.tabControl1.SuspendLayout();
            this.manageAccountsTabPage.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.accountsDataGridView)).BeginInit();
            this.vaultRegistrationTabPage.SuspendLayout();
            this.CustomProxyGroupBox.SuspendLayout();
            this.localizationTabPage.SuspendLayout();
            this.configurationDetailsTabPage.SuspendLayout();
            this.groupBox1.SuspendLayout();
            this.SuspendLayout();
            // 
            // closeButton
            // 
            this.closeButton.Location = new System.Drawing.Point(508, 410);
            this.closeButton.Name = "closeButton";
            this.closeButton.Size = new System.Drawing.Size(75, 25);
            this.closeButton.TabIndex = 5;
            this.closeButton.Text = "Close";
            this.closeButton.UseVisualStyleBackColor = true;
            this.closeButton.Click += new System.EventHandler(this.closeButton_Click);
            // 
            // tabControl1
            // 
            this.tabControl1.Controls.Add(this.manageAccountsTabPage);
            this.tabControl1.Controls.Add(this.vaultRegistrationTabPage);
            this.tabControl1.Controls.Add(this.localizationTabPage);
            this.tabControl1.Controls.Add(this.configurationDetailsTabPage);
            this.tabControl1.Dock = System.Windows.Forms.DockStyle.Top;
            this.tabControl1.Location = new System.Drawing.Point(0, 0);
            this.tabControl1.Name = "tabControl1";
            this.tabControl1.SelectedIndex = 0;
            this.tabControl1.Size = new System.Drawing.Size(606, 378);
            this.tabControl1.TabIndex = 5;
            this.tabControl1.Selecting += new System.Windows.Forms.TabControlCancelEventHandler(this.tabControl1_Selecting);
            // 
            // manageAccountsTabPage
            // 
            this.manageAccountsTabPage.BackgroundImage = global::cspsconfigtool.Properties.Resources.InmBackground;
            this.manageAccountsTabPage.Controls.Add(this.progressLabel);
            this.manageAccountsTabPage.Controls.Add(this.headingLabel);
            this.manageAccountsTabPage.Controls.Add(this.messageLabel);
            this.manageAccountsTabPage.Controls.Add(this.accountsDataGridView);
            this.manageAccountsTabPage.Controls.Add(this.editButton);
            this.manageAccountsTabPage.Controls.Add(this.addButton);
            this.manageAccountsTabPage.Controls.Add(this.deleteButton);
            this.manageAccountsTabPage.Location = new System.Drawing.Point(4, 22);
            this.manageAccountsTabPage.Name = "manageAccountsTabPage";
            this.manageAccountsTabPage.Padding = new System.Windows.Forms.Padding(3);
            this.manageAccountsTabPage.Size = new System.Drawing.Size(598, 352);
            this.manageAccountsTabPage.TabIndex = 0;
            this.manageAccountsTabPage.Text = "Manage Accounts";
            this.manageAccountsTabPage.UseVisualStyleBackColor = true;
            // 
            // progressLabel
            // 
            this.progressLabel.Location = new System.Drawing.Point(17, 315);
            this.progressLabel.Name = "progressLabel";
            this.progressLabel.Size = new System.Drawing.Size(562, 26);
            this.progressLabel.TabIndex = 13;
            this.progressLabel.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
            // 
            // headingLabel
            // 
            this.headingLabel.BackColor = System.Drawing.Color.Transparent;
            this.headingLabel.FlatStyle = System.Windows.Forms.FlatStyle.System;
            this.headingLabel.Font = new System.Drawing.Font("Arial", 9.75F);
            this.headingLabel.ForeColor = System.Drawing.Color.Black;
            this.headingLabel.Location = new System.Drawing.Point(3, 13);
            this.headingLabel.Name = "headingLabel";
            this.headingLabel.Padding = new System.Windows.Forms.Padding(2, 2, 0, 0);
            this.headingLabel.Size = new System.Drawing.Size(572, 53);
            this.headingLabel.TabIndex = 12;
            this.headingLabel.Text = resources.GetString("headingLabel.Text");
            // 
            // messageLabel
            // 
            this.messageLabel.Location = new System.Drawing.Point(17, 121);
            this.messageLabel.Name = "messageLabel";
            this.messageLabel.Size = new System.Drawing.Size(562, 191);
            this.messageLabel.TabIndex = 11;
            this.messageLabel.Visible = false;
            // 
            // accountsDataGridView
            // 
            this.accountsDataGridView.AllowUserToAddRows = false;
            this.accountsDataGridView.AllowUserToDeleteRows = false;
            this.accountsDataGridView.AllowUserToResizeRows = false;
            this.accountsDataGridView.AutoSizeRowsMode = System.Windows.Forms.DataGridViewAutoSizeRowsMode.AllCells;
            this.accountsDataGridView.BackgroundColor = System.Drawing.Color.White;
            this.accountsDataGridView.ColumnHeadersHeightSizeMode = System.Windows.Forms.DataGridViewColumnHeadersHeightSizeMode.AutoSize;
            this.accountsDataGridView.Columns.AddRange(new System.Windows.Forms.DataGridViewColumn[] {
            this.ColumnFriendlyName,
            this.ColumnUsername});
            dataGridViewCellStyle1.Alignment = System.Windows.Forms.DataGridViewContentAlignment.MiddleLeft;
            dataGridViewCellStyle1.BackColor = System.Drawing.SystemColors.Window;
            dataGridViewCellStyle1.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            dataGridViewCellStyle1.ForeColor = System.Drawing.SystemColors.ControlText;
            dataGridViewCellStyle1.SelectionBackColor = System.Drawing.Color.FromArgb(((int)(((byte)(0)))), ((int)(((byte)(88)))), ((int)(((byte)(173)))));
            dataGridViewCellStyle1.SelectionForeColor = System.Drawing.SystemColors.HighlightText;
            dataGridViewCellStyle1.WrapMode = System.Windows.Forms.DataGridViewTriState.False;
            this.accountsDataGridView.DefaultCellStyle = dataGridViewCellStyle1;
            this.accountsDataGridView.Location = new System.Drawing.Point(20, 121);
            this.accountsDataGridView.MultiSelect = false;
            this.accountsDataGridView.Name = "accountsDataGridView";
            this.accountsDataGridView.RowHeadersVisible = false;
            this.accountsDataGridView.SelectionMode = System.Windows.Forms.DataGridViewSelectionMode.FullRowSelect;
            this.accountsDataGridView.Size = new System.Drawing.Size(559, 191);
            this.accountsDataGridView.TabIndex = 10;
            this.accountsDataGridView.Visible = false;
            // 
            // ColumnFriendlyName
            // 
            this.ColumnFriendlyName.AutoSizeMode = System.Windows.Forms.DataGridViewAutoSizeColumnMode.Fill;
            this.ColumnFriendlyName.HeaderText = "NAME";
            this.ColumnFriendlyName.Name = "ColumnFriendlyName";
            this.ColumnFriendlyName.ReadOnly = true;
            // 
            // ColumnUsername
            // 
            this.ColumnUsername.HeaderText = "USER NAME";
            this.ColumnUsername.Name = "ColumnUsername";
            this.ColumnUsername.ReadOnly = true;
            this.ColumnUsername.Width = 300;
            // 
            // editButton
            // 
            this.editButton.Enabled = false;
            this.editButton.Location = new System.Drawing.Point(140, 83);
            this.editButton.Name = "editButton";
            this.editButton.Size = new System.Drawing.Size(75, 23);
            this.editButton.TabIndex = 7;
            this.editButton.Text = "Edit";
            this.editButton.UseVisualStyleBackColor = true;
            this.editButton.Click += new System.EventHandler(this.editButton_Click);
            // 
            // addButton
            // 
            this.addButton.Location = new System.Drawing.Point(16, 83);
            this.addButton.Name = "addButton";
            this.addButton.Size = new System.Drawing.Size(95, 23);
            this.addButton.TabIndex = 6;
            this.addButton.Text = "Add Account";
            this.addButton.UseVisualStyleBackColor = true;
            this.addButton.Click += new System.EventHandler(this.addButton_Click);
            // 
            // deleteButton
            // 
            this.deleteButton.Enabled = false;
            this.deleteButton.Location = new System.Drawing.Point(246, 83);
            this.deleteButton.Name = "deleteButton";
            this.deleteButton.Size = new System.Drawing.Size(75, 23);
            this.deleteButton.TabIndex = 8;
            this.deleteButton.Text = "Delete";
            this.deleteButton.UseVisualStyleBackColor = true;
            this.deleteButton.Click += new System.EventHandler(this.deleteButton_Click);
            // 
            // vaultRegistrationTabPage
            // 
            this.vaultRegistrationTabPage.BackgroundImage = global::cspsconfigtool.Properties.Resources.InmBackground;
            this.vaultRegistrationTabPage.Controls.Add(this.CustomProxyGroupBox);
            this.vaultRegistrationTabPage.Controls.Add(this.CustomProxyConnectRadiobutton);
            this.vaultRegistrationTabPage.Controls.Add(this.BypassProxyConnectionRadiobutton);
            this.vaultRegistrationTabPage.Controls.Add(this.label3);
            this.vaultRegistrationTabPage.Controls.Add(this.browsebutton);
            this.vaultRegistrationTabPage.Controls.Add(this.regbutton);
            this.vaultRegistrationTabPage.Controls.Add(this.textBox1);
            this.vaultRegistrationTabPage.Controls.Add(this.label1);
            this.vaultRegistrationTabPage.Location = new System.Drawing.Point(4, 22);
            this.vaultRegistrationTabPage.Name = "vaultRegistrationTabPage";
            this.vaultRegistrationTabPage.Padding = new System.Windows.Forms.Padding(3);
            this.vaultRegistrationTabPage.Size = new System.Drawing.Size(598, 352);
            this.vaultRegistrationTabPage.TabIndex = 1;
            this.vaultRegistrationTabPage.Text = "Vault Registration";
            this.vaultRegistrationTabPage.UseVisualStyleBackColor = true;
            // 
            // CustomProxyGroupBox
            // 
            this.CustomProxyGroupBox.Controls.Add(this.ProxyAddressTextBox);
            this.CustomProxyGroupBox.Controls.Add(this.ProxyAuthCheckBox);
            this.CustomProxyGroupBox.Controls.Add(this.ProxyPasswordTextBox);
            this.CustomProxyGroupBox.Controls.Add(this.ProxyPortTextBox);
            this.CustomProxyGroupBox.Controls.Add(this.ProxyUserNameLabel);
            this.CustomProxyGroupBox.Controls.Add(this.ProxyPasswordLabel);
            this.CustomProxyGroupBox.Controls.Add(this.ProxyPortLabel);
            this.CustomProxyGroupBox.Controls.Add(this.ProxyAddressLabel);
            this.CustomProxyGroupBox.Controls.Add(this.ProxyUserNameTextBox);
            this.CustomProxyGroupBox.Location = new System.Drawing.Point(31, 174);
            this.CustomProxyGroupBox.Name = "CustomProxyGroupBox";
            this.CustomProxyGroupBox.Size = new System.Drawing.Size(405, 114);
            this.CustomProxyGroupBox.TabIndex = 21;
            this.CustomProxyGroupBox.TabStop = false;
            // 
            // ProxyAddressTextBox
            // 
            this.ProxyAddressTextBox.Location = new System.Drawing.Point(62, 11);
            this.ProxyAddressTextBox.Name = "ProxyAddressTextBox";
            this.ProxyAddressTextBox.Size = new System.Drawing.Size(161, 20);
            this.ProxyAddressTextBox.TabIndex = 11;
            // 
            // ProxyAuthCheckBox
            // 
            this.ProxyAuthCheckBox.AutoSize = true;
            this.ProxyAuthCheckBox.Location = new System.Drawing.Point(62, 40);
            this.ProxyAuthCheckBox.Name = "ProxyAuthCheckBox";
            this.ProxyAuthCheckBox.Size = new System.Drawing.Size(216, 17);
            this.ProxyAuthCheckBox.TabIndex = 14;
            this.ProxyAuthCheckBox.Text = "This proxy server requires authentication";
            this.ProxyAuthCheckBox.UseVisualStyleBackColor = true;
            this.ProxyAuthCheckBox.CheckedChanged += new System.EventHandler(this.ProxyAuthCheckBox_CheckedChanged);
            // 
            // ProxyPasswordTextBox
            // 
            this.ProxyPasswordTextBox.Location = new System.Drawing.Point(145, 87);
            this.ProxyPasswordTextBox.Name = "ProxyPasswordTextBox";
            this.ProxyPasswordTextBox.Size = new System.Drawing.Size(133, 20);
            this.ProxyPasswordTextBox.TabIndex = 18;
            // 
            // ProxyPortTextBox
            // 
            this.ProxyPortTextBox.Location = new System.Drawing.Point(284, 14);
            this.ProxyPortTextBox.Name = "ProxyPortTextBox";
            this.ProxyPortTextBox.Size = new System.Drawing.Size(100, 20);
            this.ProxyPortTextBox.TabIndex = 13;
            // 
            // ProxyUserNameLabel
            // 
            this.ProxyUserNameLabel.AutoSize = true;
            this.ProxyUserNameLabel.Location = new System.Drawing.Point(79, 60);
            this.ProxyUserNameLabel.Name = "ProxyUserNameLabel";
            this.ProxyUserNameLabel.Size = new System.Drawing.Size(58, 13);
            this.ProxyUserNameLabel.TabIndex = 15;
            this.ProxyUserNameLabel.Text = "Username:";
            // 
            // ProxyPasswordLabel
            // 
            this.ProxyPasswordLabel.AutoSize = true;
            this.ProxyPasswordLabel.Location = new System.Drawing.Point(81, 85);
            this.ProxyPasswordLabel.Name = "ProxyPasswordLabel";
            this.ProxyPasswordLabel.Size = new System.Drawing.Size(56, 13);
            this.ProxyPasswordLabel.TabIndex = 17;
            this.ProxyPasswordLabel.Text = "Password:";
            // 
            // ProxyPortLabel
            // 
            this.ProxyPortLabel.AutoSize = true;
            this.ProxyPortLabel.Location = new System.Drawing.Point(249, 14);
            this.ProxyPortLabel.Name = "ProxyPortLabel";
            this.ProxyPortLabel.Size = new System.Drawing.Size(29, 13);
            this.ProxyPortLabel.TabIndex = 12;
            this.ProxyPortLabel.Text = "Port:";
            // 
            // ProxyAddressLabel
            // 
            this.ProxyAddressLabel.AutoSize = true;
            this.ProxyAddressLabel.Location = new System.Drawing.Point(6, 11);
            this.ProxyAddressLabel.Name = "ProxyAddressLabel";
            this.ProxyAddressLabel.Size = new System.Drawing.Size(48, 13);
            this.ProxyAddressLabel.TabIndex = 10;
            this.ProxyAddressLabel.Text = "Address:";
            // 
            // ProxyUserNameTextBox
            // 
            this.ProxyUserNameTextBox.Location = new System.Drawing.Point(145, 60);
            this.ProxyUserNameTextBox.Name = "ProxyUserNameTextBox";
            this.ProxyUserNameTextBox.Size = new System.Drawing.Size(133, 20);
            this.ProxyUserNameTextBox.TabIndex = 16;
            // 
            // CustomProxyConnectRadiobutton
            // 
            this.CustomProxyConnectRadiobutton.AutoSize = true;
            this.CustomProxyConnectRadiobutton.Location = new System.Drawing.Point(12, 151);
            this.CustomProxyConnectRadiobutton.Name = "CustomProxyConnectRadiobutton";
            this.CustomProxyConnectRadiobutton.Size = new System.Drawing.Size(191, 17);
            this.CustomProxyConnectRadiobutton.TabIndex = 9;
            this.CustomProxyConnectRadiobutton.Text = "Connect with custom proxy settings";
            this.CustomProxyConnectRadiobutton.UseVisualStyleBackColor = true;
            this.CustomProxyConnectRadiobutton.CheckedChanged += new System.EventHandler(this.CustomProxyConnectRadiobutton_CheckedChanged);
            // 
            // BypassProxyConnectionRadiobutton
            // 
            this.BypassProxyConnectionRadiobutton.AutoSize = true;
            this.BypassProxyConnectionRadiobutton.Checked = true;
            this.BypassProxyConnectionRadiobutton.Location = new System.Drawing.Point(12, 128);
            this.BypassProxyConnectionRadiobutton.Name = "BypassProxyConnectionRadiobutton";
            this.BypassProxyConnectionRadiobutton.Size = new System.Drawing.Size(175, 17);
            this.BypassProxyConnectionRadiobutton.TabIndex = 8;
            this.BypassProxyConnectionRadiobutton.TabStop = true;
            this.BypassProxyConnectionRadiobutton.Text = "Connect directly without a proxy";
            this.BypassProxyConnectionRadiobutton.UseVisualStyleBackColor = true;
            // 
            // label3
            // 
            this.label3.AutoSize = true;
            this.label3.BackColor = System.Drawing.Color.Transparent;
            this.label3.Font = new System.Drawing.Font("Arial", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.label3.ForeColor = System.Drawing.SystemColors.ControlText;
            this.label3.Location = new System.Drawing.Point(9, 17);
            this.label3.Name = "label3";
            this.label3.Size = new System.Drawing.Size(199, 16);
            this.label3.TabIndex = 0;
            this.label3.Text = "Azure Site Recovery Registration";
            // 
            // browsebutton
            // 
            this.browsebutton.Location = new System.Drawing.Point(504, 92);
            this.browsebutton.Name = "browsebutton";
            this.browsebutton.Size = new System.Drawing.Size(75, 23);
            this.browsebutton.TabIndex = 4;
            this.browsebutton.Text = "Browse";
            this.browsebutton.UseVisualStyleBackColor = true;
            this.browsebutton.Click += new System.EventHandler(this.browseButton_Click);
            // 
            // regbutton
            // 
            this.regbutton.Location = new System.Drawing.Point(504, 319);
            this.regbutton.Name = "regbutton";
            this.regbutton.Size = new System.Drawing.Size(75, 25);
            this.regbutton.TabIndex = 4;
            this.regbutton.Text = "Register";
            this.regbutton.UseVisualStyleBackColor = true;
            this.regbutton.Click += new System.EventHandler(this.regButton_Click);
            // 
            // textBox1
            // 
            this.textBox1.Location = new System.Drawing.Point(9, 94);
            this.textBox1.Name = "textBox1";
            this.textBox1.Size = new System.Drawing.Size(447, 20);
            this.textBox1.TabIndex = 1;
            // 
            // label1
            // 
            this.label1.AutoSize = true;
            this.label1.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.label1.Location = new System.Drawing.Point(6, 78);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(198, 13);
            this.label1.TabIndex = 0;
            this.label1.Text = "Recovery Services Vault Credentials File";
            // 
            // localizationTabPage
            // 
            this.localizationTabPage.BackgroundImage = global::cspsconfigtool.Properties.Resources.InmBackground;
            this.localizationTabPage.Controls.Add(this.saveButton);
            this.localizationTabPage.Controls.Add(this.langComboBox);
            this.localizationTabPage.Controls.Add(this.label4);
            this.localizationTabPage.Controls.Add(this.label2);
            this.localizationTabPage.Location = new System.Drawing.Point(4, 22);
            this.localizationTabPage.Name = "localizationTabPage";
            this.localizationTabPage.Padding = new System.Windows.Forms.Padding(3);
            this.localizationTabPage.Size = new System.Drawing.Size(598, 352);
            this.localizationTabPage.TabIndex = 2;
            this.localizationTabPage.Text = "Localization";
            this.localizationTabPage.UseVisualStyleBackColor = true;
            // 
            // saveButton
            // 
            this.saveButton.Location = new System.Drawing.Point(349, 137);
            this.saveButton.Name = "saveButton";
            this.saveButton.Size = new System.Drawing.Size(75, 23);
            this.saveButton.TabIndex = 4;
            this.saveButton.Text = "Save";
            this.saveButton.UseVisualStyleBackColor = true;
            this.saveButton.Click += new System.EventHandler(this.saveButton_Click);
            // 
            // langComboBox
            // 
            this.langComboBox.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.langComboBox.FormattingEnabled = true;
            this.langComboBox.Items.AddRange(new object[] {
            "Portuguese",
            "German",
            "English",
            "Spanish",
            "French",
            "Hungarian (Hungary)",
            "Italian",
            "Japanese",
            "Korean",
            "Russian (Russia)",
            "Polish (Poland)",
            "Portuguese (Brazil)",
            "Simplified_Chinese",
            "Traditional_Chinese"});
            this.langComboBox.Location = new System.Drawing.Point(19, 139);
            this.langComboBox.Name = "langComboBox";
            this.langComboBox.Size = new System.Drawing.Size(265, 21);
            this.langComboBox.TabIndex = 3;
            // 
            // label4
            // 
            this.label4.AutoSize = true;
            this.label4.BackColor = System.Drawing.Color.Transparent;
            this.label4.Font = new System.Drawing.Font("Arial", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.label4.ForeColor = System.Drawing.SystemColors.ControlText;
            this.label4.Location = new System.Drawing.Point(16, 99);
            this.label4.Name = "label4";
            this.label4.Size = new System.Drawing.Size(408, 16);
            this.label4.TabIndex = 2;
            this.label4.Text = "Select a language for error messages to be shown on the ASR portal.";
            // 
            // label2
            // 
            this.label2.AutoSize = true;
            this.label2.BackColor = System.Drawing.Color.Transparent;
            this.label2.Font = new System.Drawing.Font("Arial", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.label2.ForeColor = System.Drawing.SystemColors.ControlText;
            this.label2.Location = new System.Drawing.Point(9, 17);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(268, 16);
            this.label2.TabIndex = 1;
            this.label2.Text = "Provider Error Message Localization Settings";
            // 
            // configurationDetailsTabPage
            // 
            this.configurationDetailsTabPage.BackgroundImage = global::cspsconfigtool.Properties.Resources.InmBackground;
            this.configurationDetailsTabPage.Controls.Add(this.groupBox1);
            this.configurationDetailsTabPage.Location = new System.Drawing.Point(4, 22);
            this.configurationDetailsTabPage.Name = "configurationDetailsTabPage";
            this.configurationDetailsTabPage.Padding = new System.Windows.Forms.Padding(3);
            this.configurationDetailsTabPage.Size = new System.Drawing.Size(598, 352);
            this.configurationDetailsTabPage.TabIndex = 3;
            this.configurationDetailsTabPage.Text = "ConfigurationDetails";
            this.configurationDetailsTabPage.UseVisualStyleBackColor = true;
            // 
            // groupBox1
            // 
            this.groupBox1.BackColor = System.Drawing.Color.Transparent;
            this.groupBox1.Controls.Add(this.label9);
            this.groupBox1.Controls.Add(this.label8);
            this.groupBox1.Controls.Add(this.label7);
            this.groupBox1.Controls.Add(this.azureCsIpComboBox);
            this.groupBox1.Controls.Add(this.dvacplinkLabel);
            this.groupBox1.Controls.Add(this.label6);
            this.groupBox1.Controls.Add(this.textBox6);
            this.groupBox1.Controls.Add(this.label5);
            this.groupBox1.Controls.Add(this.configsavebutton);
            this.groupBox1.Controls.Add(this.label16);
            this.groupBox1.Controls.Add(this.textBox5);
            this.groupBox1.Controls.Add(this.label15);
            this.groupBox1.Controls.Add(this.signatureVerificationNoteLabel);
            this.groupBox1.Controls.Add(this.signatureVerificationCheckBox);
            this.groupBox1.Controls.Add(this.label11);
            this.groupBox1.Controls.Add(this.label10);
            this.groupBox1.Controls.Add(this.textBox3);
            this.groupBox1.Controls.Add(this.label12);
            this.groupBox1.Controls.Add(this.textBox2);
            this.groupBox1.Controls.Add(this.label13);
            this.groupBox1.Controls.Add(this.onPremCsIpText);
            this.groupBox1.Controls.Add(this.label14);
            this.groupBox1.Location = new System.Drawing.Point(-2, -10);
            this.groupBox1.Name = "groupBox1";
            this.groupBox1.Size = new System.Drawing.Size(646, 355);
            this.groupBox1.TabIndex = 12;
            this.groupBox1.TabStop = false;
            // 
            // label9
            // 
            this.label9.AutoSize = true;
            this.label9.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.label9.Location = new System.Drawing.Point(230, 125);
            this.label9.Name = "label9";
            this.label9.Size = new System.Drawing.Size(151, 13);
            this.label9.TabIndex = 27;
            this.label9.Text = "(Azure connection for failback)";
            // 
            // label8
            // 
            this.label8.AutoSize = true;
            this.label8.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.label8.Location = new System.Drawing.Point(20, 125);
            this.label8.Name = "label8";
            this.label8.Size = new System.Drawing.Size(109, 13);
            this.label8.TabIndex = 26;
            this.label8.Text = "(On-prem connection)";
            // 
            // label7
            // 
            this.label7.AutoSize = true;
            this.label7.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.label7.Location = new System.Drawing.Point(230, 110);
            this.label7.Name = "label7";
            this.label7.Size = new System.Drawing.Size(139, 13);
            this.label7.TabIndex = 25;
            this.label7.Text = "Configuration Server IP";
            // 
            // azureCsIpComboBox
            // 
            this.azureCsIpComboBox.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.azureCsIpComboBox.Location = new System.Drawing.Point(233, 140);
            this.azureCsIpComboBox.Name = "azureCsIpComboBox";
            this.azureCsIpComboBox.Size = new System.Drawing.Size(110, 21);
            this.azureCsIpComboBox.TabIndex = 28;
            // 
            // dvacplinkLabel
            // 
            this.dvacplinkLabel.AutoSize = true;
            this.dvacplinkLabel.Location = new System.Drawing.Point(441, 206);
            this.dvacplinkLabel.Name = "dvacplinkLabel";
            this.dvacplinkLabel.Size = new System.Drawing.Size(59, 13);
            this.dvacplinkLabel.TabIndex = 23;
            this.dvacplinkLabel.TabStop = true;
            this.dvacplinkLabel.Text = "Read more";
            this.dvacplinkLabel.LinkClicked += new System.Windows.Forms.LinkLabelLinkClickedEventHandler(this.dvacplinkLabel_LinkClicked);
            // 
            // label6
            // 
            this.label6.AutoSize = true;
            this.label6.Location = new System.Drawing.Point(252, 85);
            this.label6.Name = "label6";
            this.label6.Size = new System.Drawing.Size(102, 13);
            this.label6.TabIndex = 22;
            this.label6.Text = "Stopping Services...";
            // 
            // textBox6
            // 
            this.textBox6.Location = new System.Drawing.Point(230, 224);
            this.textBox6.Name = "textBox6";
            this.textBox6.Size = new System.Drawing.Size(100, 20);
            this.textBox6.TabIndex = 21;
            // 
            // label5
            // 
            this.label5.AutoSize = true;
            this.label5.Location = new System.Drawing.Point(230, 206);
            this.label5.Name = "label5";
            this.label5.Size = new System.Drawing.Size(279, 13);
            this.label5.TabIndex = 20;
            this.label5.Text = "Application Consistency Orchestration Port  (                    )";
            // 
            // configsavebutton
            // 
            this.configsavebutton.Location = new System.Drawing.Point(506, 261);
            this.configsavebutton.Name = "configsavebutton";
            this.configsavebutton.Size = new System.Drawing.Size(75, 26);
            this.configsavebutton.TabIndex = 7;
            this.configsavebutton.Text = "Save";
            this.configsavebutton.UseVisualStyleBackColor = true;
            this.configsavebutton.Click += new System.EventHandler(this.configSaveButton_Click);
            // 
            // label16
            // 
            this.label16.AutoSize = true;
            this.label16.BackColor = System.Drawing.Color.White;
            this.label16.FlatStyle = System.Windows.Forms.FlatStyle.System;
            this.label16.Font = new System.Drawing.Font("Microsoft Sans Serif", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.label16.ForeColor = System.Drawing.Color.Black;
            this.label16.Location = new System.Drawing.Point(19, 32);
            this.label16.Name = "label16";
            this.label16.Size = new System.Drawing.Size(228, 16);
            this.label16.TabIndex = 19;
            this.label16.Text = "Configuration/Process Server Details";
            // 
            // textBox5
            // 
            this.textBox5.Location = new System.Drawing.Point(20, 224);
            this.textBox5.Name = "textBox5";
            this.textBox5.Size = new System.Drawing.Size(100, 20);
            this.textBox5.TabIndex = 18;
            // 
            // label15
            // 
            this.label15.AutoSize = true;
            this.label15.Location = new System.Drawing.Point(20, 206);
            this.label15.Name = "label15";
            this.label15.Size = new System.Drawing.Size(137, 13);
            this.label15.TabIndex = 17;
            this.label15.Text = "Data Transfer Port (Secure)";
            // 
            // signatureVerificationNoteLabel
            // 
            this.signatureVerificationNoteLabel.AutoSize = true;
            this.signatureVerificationNoteLabel.Location = new System.Drawing.Point(21, 290);
            this.signatureVerificationNoteLabel.Name = "signatureVerificationNoteLabel";
            this.signatureVerificationNoteLabel.Size = new System.Drawing.Size(496, 26);
            this.signatureVerificationNoteLabel.TabIndex = 16;
            this.signatureVerificationNoteLabel.Text = "Note: Disable this check if the Process Server does not have internet connectivit" +
    "y. \r\nThis would result in Mobility Service software installed on source machines" +
    " without signature verification.";
            // 
            // signatureVerificationCheckBox
            // 
            this.signatureVerificationCheckBox.AutoSize = true;
            this.signatureVerificationCheckBox.Checked = true;
            this.signatureVerificationCheckBox.CheckState = System.Windows.Forms.CheckState.Checked;
            this.signatureVerificationCheckBox.Location = new System.Drawing.Point(21, 272);
            this.signatureVerificationCheckBox.Name = "signatureVerificationCheckBox";
            this.signatureVerificationCheckBox.Size = new System.Drawing.Size(392, 17);
            this.signatureVerificationCheckBox.TabIndex = 15;
            this.signatureVerificationCheckBox.Text = "Verify Mobility Service software signature before installing on source machines";
            this.signatureVerificationCheckBox.UseVisualStyleBackColor = true;
            // 
            // label11
            // 
            this.label11.AutoSize = true;
            this.label11.Location = new System.Drawing.Point(20, 338);
            this.label11.Name = "label11";
            this.label11.Size = new System.Drawing.Size(330, 13);
            this.label11.TabIndex = 14;
            this.label11.Text = "\"[ASR Install directory]\\home\\svsystems\\bin\\genpassphrase.exe -n\"";
            // 
            // label10
            // 
            this.label10.AutoSize = true;
            this.label10.Location = new System.Drawing.Point(20, 325);
            this.label10.Name = "label10";
            this.label10.Size = new System.Drawing.Size(363, 13);
            this.label10.TabIndex = 13;
            this.label10.Text = "Passphrase value can be obtained from the Configuration Server by running";
            // 
            // textBox3
            // 
            this.textBox3.Location = new System.Drawing.Point(230, 180);
            this.textBox3.Name = "textBox3";
            this.textBox3.Size = new System.Drawing.Size(154, 20);
            this.textBox3.TabIndex = 5;
            // 
            // label12
            // 
            this.label12.AutoSize = true;
            this.label12.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.label12.Location = new System.Drawing.Point(20, 110);
            this.label12.Name = "label12";
            this.label12.Size = new System.Drawing.Size(139, 13);
            this.label12.TabIndex = 0;
            this.label12.Text = "Configuration Server IP";
            // 
            // textBox2
            // 
            this.textBox2.Location = new System.Drawing.Point(20, 180);
            this.textBox2.Name = "textBox2";
            this.textBox2.Size = new System.Drawing.Size(100, 20);
            this.textBox2.TabIndex = 4;
            // 
            // label13
            // 
            this.label13.AutoSize = true;
            this.label13.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.label13.Location = new System.Drawing.Point(20, 165);
            this.label13.Name = "label13";
            this.label13.Size = new System.Drawing.Size(125, 13);
            this.label13.TabIndex = 1;
            this.label13.Text = "Configuration Server Port";
            // 
            // onPremCsIpText
            // 
            this.onPremCsIpText.Location = new System.Drawing.Point(20, 140);
            this.onPremCsIpText.Name = "onPremCsIpText";
            this.onPremCsIpText.Size = new System.Drawing.Size(110, 20);
            this.onPremCsIpText.TabIndex = 3;
            // 
            // label14
            // 
            this.label14.AutoSize = true;
            this.label14.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.label14.Location = new System.Drawing.Point(230, 165);
            this.label14.Name = "label14";
            this.label14.Size = new System.Drawing.Size(119, 13);
            this.label14.TabIndex = 2;
            this.label14.Text = "Connection Passphrase";
            // 
            // deleteBackgroundWorker
            // 
            this.deleteBackgroundWorker.WorkerReportsProgress = true;
            this.deleteBackgroundWorker.WorkerSupportsCancellation = true;
            this.deleteBackgroundWorker.DoWork += new System.ComponentModel.DoWorkEventHandler(this.deleteBackgroundWorker_DoWork);
            this.deleteBackgroundWorker.ProgressChanged += new System.ComponentModel.ProgressChangedEventHandler(this.backgroundWorker_ProgressChanged);
            this.deleteBackgroundWorker.RunWorkerCompleted += new System.ComponentModel.RunWorkerCompletedEventHandler(this.deleteBackgroundWorker_RunWorkerCompleted);
            // 
            // loadBackgroundWorker
            // 
            this.loadBackgroundWorker.WorkerReportsProgress = true;
            this.loadBackgroundWorker.WorkerSupportsCancellation = true;
            this.loadBackgroundWorker.DoWork += new System.ComponentModel.DoWorkEventHandler(this.loadBackgroundWorker_DoWork);
            this.loadBackgroundWorker.ProgressChanged += new System.ComponentModel.ProgressChangedEventHandler(this.backgroundWorker_ProgressChanged);
            this.loadBackgroundWorker.RunWorkerCompleted += new System.ComponentModel.RunWorkerCompletedEventHandler(this.loadBackgroundWorker_RunWorkerCompleted);
            // 
            // ASRRegInProgressLabel
            // 
            this.ASRRegInProgressLabel.Image = global::cspsconfigtool.Properties.Resources.anm_process_img;
            this.ASRRegInProgressLabel.ImageAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.ASRRegInProgressLabel.Location = new System.Drawing.Point(104, 394);
            this.ASRRegInProgressLabel.Name = "ASRRegInProgressLabel";
            this.ASRRegInProgressLabel.Size = new System.Drawing.Size(385, 16);
            this.ASRRegInProgressLabel.TabIndex = 6;
            this.ASRRegInProgressLabel.Text = "      Azure Site Recovery Registration in progress...  Please wait for few minute" +
    "s.";
            // 
            // csform
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(606, 447);
            this.Controls.Add(this.ASRRegInProgressLabel);
            this.Controls.Add(this.tabControl1);
            this.Controls.Add(this.closeButton);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedSingle;
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.MaximizeBox = false;
            this.Name = "csform";
            this.ShowIcon = false;
            this.Text = "Microsoft Azure Site Recovery Configuration Server";
            this.FormClosing += new System.Windows.Forms.FormClosingEventHandler(this.OnCSForm_Closing);
            this.Load += new System.EventHandler(this.csform_Load);
            this.tabControl1.ResumeLayout(false);
            this.manageAccountsTabPage.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.accountsDataGridView)).EndInit();
            this.vaultRegistrationTabPage.ResumeLayout(false);
            this.vaultRegistrationTabPage.PerformLayout();
            this.CustomProxyGroupBox.ResumeLayout(false);
            this.CustomProxyGroupBox.PerformLayout();
            this.localizationTabPage.ResumeLayout(false);
            this.localizationTabPage.PerformLayout();
            this.configurationDetailsTabPage.ResumeLayout(false);
            this.groupBox1.ResumeLayout(false);
            this.groupBox1.PerformLayout();
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.Button closeButton;
        private System.Windows.Forms.Button regbutton;
        private System.Windows.Forms.TabControl tabControl1;
        private System.Windows.Forms.TabPage manageAccountsTabPage;
        private System.Windows.Forms.TabPage vaultRegistrationTabPage;
        private System.Windows.Forms.Label label3;
        private System.Windows.Forms.Button browsebutton;
        private System.Windows.Forms.TextBox textBox1;
        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.Button editButton;
        private System.Windows.Forms.Button addButton;
        private System.Windows.Forms.Button deleteButton;
        private System.Windows.Forms.DataGridView accountsDataGridView;
        private System.Windows.Forms.Label messageLabel;
        private System.Windows.Forms.Label headingLabel;
        private System.Windows.Forms.Label progressLabel;
        private System.ComponentModel.BackgroundWorker deleteBackgroundWorker;
        private System.ComponentModel.BackgroundWorker loadBackgroundWorker;
        private System.Windows.Forms.TabPage localizationTabPage;
        private System.Windows.Forms.Label label2;
        private System.Windows.Forms.Label label4;
        private System.Windows.Forms.ComboBox langComboBox;
        private System.Windows.Forms.Button saveButton;
        private System.Windows.Forms.TabPage configurationDetailsTabPage;
        private System.Windows.Forms.GroupBox groupBox1;
        private System.Windows.Forms.Label signatureVerificationNoteLabel;
        private System.Windows.Forms.CheckBox signatureVerificationCheckBox;
        private System.Windows.Forms.Label label11;
        private System.Windows.Forms.Label label10;
        private System.Windows.Forms.TextBox textBox3;
        private System.Windows.Forms.Label label12;
        private System.Windows.Forms.TextBox textBox2;
        private System.Windows.Forms.Label label13;
        private System.Windows.Forms.TextBox onPremCsIpText;
        private System.Windows.Forms.Label label14;
        private System.Windows.Forms.TextBox textBox5;
        private System.Windows.Forms.Label label15;
        private System.Windows.Forms.Button configsavebutton;
        private System.Windows.Forms.Label label16;
        private System.Windows.Forms.Label label6;
        private System.Windows.Forms.TextBox textBox6;
        private System.Windows.Forms.Label label5;
        private System.Windows.Forms.LinkLabel dvacplinkLabel;
        private System.Windows.Forms.TextBox ProxyPasswordTextBox;
        private System.Windows.Forms.Label ProxyPasswordLabel;
        private System.Windows.Forms.TextBox ProxyUserNameTextBox;
        private System.Windows.Forms.Label ProxyUserNameLabel;
        private System.Windows.Forms.CheckBox ProxyAuthCheckBox;
        private System.Windows.Forms.TextBox ProxyPortTextBox;
        private System.Windows.Forms.Label ProxyPortLabel;
        private System.Windows.Forms.TextBox ProxyAddressTextBox;
        private System.Windows.Forms.Label ProxyAddressLabel;
        private System.Windows.Forms.RadioButton CustomProxyConnectRadiobutton;
        private System.Windows.Forms.RadioButton BypassProxyConnectionRadiobutton;
        private System.Windows.Forms.GroupBox CustomProxyGroupBox;
        private System.Windows.Forms.DataGridViewTextBoxColumn ColumnFriendlyName;
        private System.Windows.Forms.DataGridViewTextBoxColumn ColumnUsername;
        private System.Windows.Forms.Label ASRRegInProgressLabel;
        private System.Windows.Forms.Label label7;
        private System.Windows.Forms.Label label9;
        private System.Windows.Forms.Label label8;
        private System.Windows.Forms.ComboBox azureCsIpComboBox;
    }
}

