namespace Com.Inmage.Wizard
{
    partial class ImportOfflineSyncForm
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
            System.Windows.Forms.DataGridViewCellStyle dataGridViewCellStyle1 = new System.Windows.Forms.DataGridViewCellStyle();
            System.Windows.Forms.DataGridViewCellStyle dataGridViewCellStyle2 = new System.Windows.Forms.DataGridViewCellStyle();
            System.Windows.Forms.DataGridViewCellStyle dataGridViewCellStyle3 = new System.Windows.Forms.DataGridViewCellStyle();
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(ImportOfflineSyncForm));
            this.generalLogTextBox = new System.Windows.Forms.TextBox();
            this.progressBar = new System.Windows.Forms.ProgressBar();
            this.nextButton = new System.Windows.Forms.Button();
            this.cancelButton = new System.Windows.Forms.Button();
            this.getTargetDetailsBackgroundWorker = new System.ComponentModel.BackgroundWorker();
            this.targetScriptBackgroundWorker = new System.ComponentModel.BackgroundWorker();
            this.doneButton = new System.Windows.Forms.Button();
            this.headingPanel = new System.Windows.Forms.Panel();
            this.patchLabel = new System.Windows.Forms.Label();
            this.versionLabel = new System.Windows.Forms.Label();
            this.navigationPanel = new System.Windows.Forms.Panel();
            this.targetServerDataGridView = new System.Windows.Forms.DataGridView();
            this.esx_ip = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.ipTarget = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.ip = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.hostToImportLabel = new System.Windows.Forms.Label();
            this.targetIpText = new System.Windows.Forms.TextBox();
            this.targetUserNameLabel = new System.Windows.Forms.Label();
            this.targetUserNameText = new System.Windows.Forms.TextBox();
            this.targetPassWordLabel = new System.Windows.Forms.Label();
            this.targetPassWordText = new System.Windows.Forms.TextBox();
            this.secondaryServerPanelAddEsxButton = new System.Windows.Forms.Button();
            this.enterEsxLabel = new System.Windows.Forms.Label();
            this.targetIpLabel = new System.Windows.Forms.Label();
            this.masterTargetTextBox = new System.Windows.Forms.TextBox();
            this.masterTargetLabel = new System.Windows.Forms.Label();
            this.taregtHostLabel = new System.Windows.Forms.Label();
            this.masterHostComboBox = new System.Windows.Forms.ComboBox();
            this.targetDetailsPanel = new System.Windows.Forms.Panel();
            this.masterTargetDataStoreComboBox = new System.Windows.Forms.ComboBox();
            this.masterTargetDataStoreLabel = new System.Windows.Forms.Label();
            this.applicationPanel = new Com.Inmage.Wizard.BufferedPanel();
            this.helpPanel = new System.Windows.Forms.Panel();
            this.helpContentLabel = new System.Windows.Forms.Label();
            this.esxProtectionLinkLabel = new System.Windows.Forms.LinkLabel();
            this.closePictureBox = new System.Windows.Forms.PictureBox();
            this.helpPictureBox = new System.Windows.Forms.PictureBox();
            this.p2vHeadingLabel = new System.Windows.Forms.Label();
            this.addCredentialsLabel = new System.Windows.Forms.Label();
            this.credentialPictureBox = new System.Windows.Forms.PictureBox();
            this.mandatoryLabel = new System.Windows.Forms.Label();
            this.headingPanel.SuspendLayout();
            this.navigationPanel.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.targetServerDataGridView)).BeginInit();
            this.targetDetailsPanel.SuspendLayout();
            this.applicationPanel.SuspendLayout();
            this.helpPanel.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.closePictureBox)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.helpPictureBox)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.credentialPictureBox)).BeginInit();
            this.SuspendLayout();
            // 
            // generalLogTextBox
            // 
            this.generalLogTextBox.BackColor = System.Drawing.Color.SeaShell;
            this.generalLogTextBox.Location = new System.Drawing.Point(170, 458);
            this.generalLogTextBox.Multiline = true;
            this.generalLogTextBox.Name = "generalLogTextBox";
            this.generalLogTextBox.ReadOnly = true;
            this.generalLogTextBox.ScrollBars = System.Windows.Forms.ScrollBars.Both;
            this.generalLogTextBox.Size = new System.Drawing.Size(683, 81);
            this.generalLogTextBox.TabIndex = 115;
            // 
            // progressBar
            // 
            this.progressBar.Location = new System.Drawing.Point(0, 0);
            this.progressBar.Name = "progressBar";
            this.progressBar.Size = new System.Drawing.Size(677, 10);
            this.progressBar.Style = System.Windows.Forms.ProgressBarStyle.Marquee;
            this.progressBar.TabIndex = 112;
            this.progressBar.Visible = false;
            // 
            // nextButton
            // 
            this.nextButton.Location = new System.Drawing.Point(682, 561);
            this.nextButton.Name = "nextButton";
            this.nextButton.Size = new System.Drawing.Size(75, 23);
            this.nextButton.TabIndex = 111;
            this.nextButton.Text = "Next >";
            this.nextButton.UseVisualStyleBackColor = true;
            this.nextButton.Click += new System.EventHandler(this.nextButton_Click);
            // 
            // cancelButton
            // 
            this.cancelButton.DialogResult = System.Windows.Forms.DialogResult.Cancel;
            this.cancelButton.Location = new System.Drawing.Point(763, 561);
            this.cancelButton.Name = "cancelButton";
            this.cancelButton.Size = new System.Drawing.Size(75, 22);
            this.cancelButton.TabIndex = 113;
            this.cancelButton.Text = "Cancel";
            this.cancelButton.UseVisualStyleBackColor = true;
            this.cancelButton.Click += new System.EventHandler(this.cancelButton_Click);
            // 
            // getTargetDetailsBackgroundWorker
            // 
            this.getTargetDetailsBackgroundWorker.DoWork += new System.ComponentModel.DoWorkEventHandler(this.getTargetDetailsBackgroundWorker_DoWork);
            this.getTargetDetailsBackgroundWorker.RunWorkerCompleted += new System.ComponentModel.RunWorkerCompletedEventHandler(this.getTargetDetailsBackgroundWorker_RunWorkerCompleted);
            this.getTargetDetailsBackgroundWorker.ProgressChanged += new System.ComponentModel.ProgressChangedEventHandler(this.getTargetDetailsBackgroundWorker_ProgressChanged);
            // 
            // targetScriptBackgroundWorker
            // 
            this.targetScriptBackgroundWorker.DoWork += new System.ComponentModel.DoWorkEventHandler(this.targetScriptBackgroundWorker_DoWork);
            this.targetScriptBackgroundWorker.RunWorkerCompleted += new System.ComponentModel.RunWorkerCompletedEventHandler(this.targetScriptBackgroundWorker_RunWorkerCompleted);
            this.targetScriptBackgroundWorker.ProgressChanged += new System.ComponentModel.ProgressChangedEventHandler(this.targetScriptBackgroundWorker_ProgressChanged);
            // 
            // doneButton
            // 
            this.doneButton.Location = new System.Drawing.Point(763, 561);
            this.doneButton.Name = "doneButton";
            this.doneButton.Size = new System.Drawing.Size(75, 23);
            this.doneButton.TabIndex = 137;
            this.doneButton.Text = "Done";
            this.doneButton.UseVisualStyleBackColor = true;
            this.doneButton.Visible = false;
            this.doneButton.Click += new System.EventHandler(this.doneButton_Click);
            // 
            // headingPanel
            // 
            this.headingPanel.BackColor = System.Drawing.Color.Transparent;
            this.headingPanel.BackgroundImage = global::Com.Inmage.Wizard.Properties.Resources.heading;
            this.headingPanel.Controls.Add(this.patchLabel);
            this.headingPanel.Controls.Add(this.versionLabel);
            this.headingPanel.Location = new System.Drawing.Point(0, 0);
            this.headingPanel.Name = "headingPanel";
            this.headingPanel.Size = new System.Drawing.Size(853, 44);
            this.headingPanel.TabIndex = 138;
            // 
            // patchLabel
            // 
            this.patchLabel.AutoSize = true;
            this.patchLabel.Location = new System.Drawing.Point(636, 27);
            this.patchLabel.Name = "patchLabel";
            this.patchLabel.Size = new System.Drawing.Size(39, 13);
            this.patchLabel.TabIndex = 5;
            this.patchLabel.Text = "History";
            this.patchLabel.Visible = false;
            this.patchLabel.MouseEnter += new System.EventHandler(this.patchLabel_MouseEnter);
            // 
            // versionLabel
            // 
            this.versionLabel.AutoSize = true;
            this.versionLabel.Font = new System.Drawing.Font("Tahoma", 6.75F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.versionLabel.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(64)))), ((int)(((byte)(64)))), ((int)(((byte)(64)))));
            this.versionLabel.Location = new System.Drawing.Point(531, 27);
            this.versionLabel.Name = "versionLabel";
            this.versionLabel.Size = new System.Drawing.Size(98, 11);
            this.versionLabel.TabIndex = 1;
            this.versionLabel.Text = "5.50.1.GA.2047.1";
            this.versionLabel.MouseEnter += new System.EventHandler(this.versionLabel_MouseEnter);
            // 
            // navigationPanel
            // 
            this.navigationPanel.BackColor = System.Drawing.Color.Transparent;
            this.navigationPanel.BackgroundImage = global::Com.Inmage.Wizard.Properties.Resources.navBar;
            this.navigationPanel.Controls.Add(this.progressBar);
            this.navigationPanel.Location = new System.Drawing.Point(172, 545);
            this.navigationPanel.Name = "navigationPanel";
            this.navigationPanel.Size = new System.Drawing.Size(678, 10);
            this.navigationPanel.TabIndex = 139;
            // 
            // targetServerDataGridView
            // 
            this.targetServerDataGridView.AllowUserToAddRows = false;
            this.targetServerDataGridView.BackgroundColor = System.Drawing.Color.White;
            this.targetServerDataGridView.BorderStyle = System.Windows.Forms.BorderStyle.None;
            dataGridViewCellStyle1.Alignment = System.Windows.Forms.DataGridViewContentAlignment.MiddleLeft;
            dataGridViewCellStyle1.BackColor = System.Drawing.SystemColors.Control;
            dataGridViewCellStyle1.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            dataGridViewCellStyle1.ForeColor = System.Drawing.SystemColors.WindowText;
            dataGridViewCellStyle1.SelectionBackColor = System.Drawing.SystemColors.Highlight;
            dataGridViewCellStyle1.SelectionForeColor = System.Drawing.SystemColors.HighlightText;
            dataGridViewCellStyle1.WrapMode = System.Windows.Forms.DataGridViewTriState.True;
            this.targetServerDataGridView.ColumnHeadersDefaultCellStyle = dataGridViewCellStyle1;
            this.targetServerDataGridView.ColumnHeadersHeightSizeMode = System.Windows.Forms.DataGridViewColumnHeadersHeightSizeMode.AutoSize;
            this.targetServerDataGridView.Columns.AddRange(new System.Windows.Forms.DataGridViewColumn[] {
            this.esx_ip,
            this.ipTarget,
            this.ip});
            dataGridViewCellStyle2.Alignment = System.Windows.Forms.DataGridViewContentAlignment.MiddleLeft;
            dataGridViewCellStyle2.BackColor = System.Drawing.SystemColors.Window;
            dataGridViewCellStyle2.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            dataGridViewCellStyle2.ForeColor = System.Drawing.SystemColors.ControlText;
            dataGridViewCellStyle2.SelectionBackColor = System.Drawing.SystemColors.Highlight;
            dataGridViewCellStyle2.SelectionForeColor = System.Drawing.SystemColors.HighlightText;
            dataGridViewCellStyle2.WrapMode = System.Windows.Forms.DataGridViewTriState.False;
            this.targetServerDataGridView.DefaultCellStyle = dataGridViewCellStyle2;
            this.targetServerDataGridView.Location = new System.Drawing.Point(32, 127);
            this.targetServerDataGridView.Name = "targetServerDataGridView";
            dataGridViewCellStyle3.Alignment = System.Windows.Forms.DataGridViewContentAlignment.MiddleLeft;
            dataGridViewCellStyle3.BackColor = System.Drawing.SystemColors.Control;
            dataGridViewCellStyle3.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            dataGridViewCellStyle3.ForeColor = System.Drawing.SystemColors.WindowText;
            dataGridViewCellStyle3.SelectionBackColor = System.Drawing.SystemColors.Highlight;
            dataGridViewCellStyle3.SelectionForeColor = System.Drawing.SystemColors.HighlightText;
            dataGridViewCellStyle3.WrapMode = System.Windows.Forms.DataGridViewTriState.True;
            this.targetServerDataGridView.RowHeadersDefaultCellStyle = dataGridViewCellStyle3;
            this.targetServerDataGridView.RowHeadersVisible = false;
            this.targetServerDataGridView.Size = new System.Drawing.Size(302, 217);
            this.targetServerDataGridView.TabIndex = 133;
            this.targetServerDataGridView.Visible = false;
            // 
            // esx_ip
            // 
            this.esx_ip.FillWeight = 156.6826F;
            this.esx_ip.HeaderText = "Primary vSphere/vCenter IP";
            this.esx_ip.Name = "esx_ip";
            this.esx_ip.ReadOnly = true;
            // 
            // ipTarget
            // 
            this.ipTarget.FillWeight = 46.08311F;
            this.ipTarget.HeaderText = "Primary VM display name";
            this.ipTarget.Name = "ipTarget";
            this.ipTarget.ReadOnly = true;
            // 
            // ip
            // 
            this.ip.HeaderText = "Primary VM IP";
            this.ip.Name = "ip";
            this.ip.ReadOnly = true;
            // 
            // hostToImportLabel
            // 
            this.hostToImportLabel.AutoSize = true;
            this.hostToImportLabel.Font = new System.Drawing.Font("Arial", 8.25F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.hostToImportLabel.Location = new System.Drawing.Point(31, 106);
            this.hostToImportLabel.Name = "hostToImportLabel";
            this.hostToImportLabel.Size = new System.Drawing.Size(93, 14);
            this.hostToImportLabel.TabIndex = 126;
            this.hostToImportLabel.Text = "Hosts to import";
            this.hostToImportLabel.Visible = false;
            // 
            // targetIpText
            // 
            this.targetIpText.Location = new System.Drawing.Point(31, 60);
            this.targetIpText.Name = "targetIpText";
            this.targetIpText.Size = new System.Drawing.Size(100, 20);
            this.targetIpText.TabIndex = 128;
            // 
            // targetUserNameLabel
            // 
            this.targetUserNameLabel.AutoSize = true;
            this.targetUserNameLabel.Font = new System.Drawing.Font("Arial", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.targetUserNameLabel.Location = new System.Drawing.Point(149, 40);
            this.targetUserNameLabel.Name = "targetUserNameLabel";
            this.targetUserNameLabel.Size = new System.Drawing.Size(63, 14);
            this.targetUserNameLabel.TabIndex = 129;
            this.targetUserNameLabel.Text = "Username *";
            // 
            // targetUserNameText
            // 
            this.targetUserNameText.Location = new System.Drawing.Point(152, 60);
            this.targetUserNameText.Name = "targetUserNameText";
            this.targetUserNameText.Size = new System.Drawing.Size(100, 20);
            this.targetUserNameText.TabIndex = 130;
            // 
            // targetPassWordLabel
            // 
            this.targetPassWordLabel.AutoSize = true;
            this.targetPassWordLabel.Font = new System.Drawing.Font("Arial", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.targetPassWordLabel.Location = new System.Drawing.Point(266, 40);
            this.targetPassWordLabel.Name = "targetPassWordLabel";
            this.targetPassWordLabel.Size = new System.Drawing.Size(64, 14);
            this.targetPassWordLabel.TabIndex = 131;
            this.targetPassWordLabel.Text = "Password *";
            // 
            // targetPassWordText
            // 
            this.targetPassWordText.Location = new System.Drawing.Point(269, 60);
            this.targetPassWordText.Name = "targetPassWordText";
            this.targetPassWordText.PasswordChar = '*';
            this.targetPassWordText.Size = new System.Drawing.Size(100, 20);
            this.targetPassWordText.TabIndex = 132;
            // 
            // secondaryServerPanelAddEsxButton
            // 
            this.secondaryServerPanelAddEsxButton.Location = new System.Drawing.Point(394, 60);
            this.secondaryServerPanelAddEsxButton.Name = "secondaryServerPanelAddEsxButton";
            this.secondaryServerPanelAddEsxButton.Size = new System.Drawing.Size(75, 23);
            this.secondaryServerPanelAddEsxButton.TabIndex = 134;
            this.secondaryServerPanelAddEsxButton.Text = "Get Details";
            this.secondaryServerPanelAddEsxButton.UseVisualStyleBackColor = true;
            this.secondaryServerPanelAddEsxButton.Click += new System.EventHandler(this.secondaryServerPanelAddEsxButton_Click);
            // 
            // enterEsxLabel
            // 
            this.enterEsxLabel.AutoSize = true;
            this.enterEsxLabel.Font = new System.Drawing.Font("Arial", 8.25F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.enterEsxLabel.Location = new System.Drawing.Point(29, 19);
            this.enterEsxLabel.Name = "enterEsxLabel";
            this.enterEsxLabel.Size = new System.Drawing.Size(186, 14);
            this.enterEsxLabel.TabIndex = 135;
            this.enterEsxLabel.Text = "Enter secondary vSphere server";
            // 
            // targetIpLabel
            // 
            this.targetIpLabel.AutoSize = true;
            this.targetIpLabel.Font = new System.Drawing.Font("Arial", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.targetIpLabel.Location = new System.Drawing.Point(28, 40);
            this.targetIpLabel.Name = "targetIpLabel";
            this.targetIpLabel.Size = new System.Drawing.Size(107, 14);
            this.targetIpLabel.TabIndex = 136;
            this.targetIpLabel.Text = "vSphere/vCenter IP *";
            // 
            // masterTargetTextBox
            // 
            this.masterTargetTextBox.Location = new System.Drawing.Point(525, 127);
            this.masterTargetTextBox.Name = "masterTargetTextBox";
            this.masterTargetTextBox.ReadOnly = true;
            this.masterTargetTextBox.Size = new System.Drawing.Size(119, 20);
            this.masterTargetTextBox.TabIndex = 137;
            this.masterTargetTextBox.Visible = false;
            // 
            // masterTargetLabel
            // 
            this.masterTargetLabel.AutoSize = true;
            this.masterTargetLabel.Font = new System.Drawing.Font("Arial", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.masterTargetLabel.Location = new System.Drawing.Point(360, 132);
            this.masterTargetLabel.Name = "masterTargetLabel";
            this.masterTargetLabel.Size = new System.Drawing.Size(73, 14);
            this.masterTargetLabel.TabIndex = 138;
            this.masterTargetLabel.Text = "Master Target";
            this.masterTargetLabel.Visible = false;
            // 
            // taregtHostLabel
            // 
            this.taregtHostLabel.AutoSize = true;
            this.taregtHostLabel.Font = new System.Drawing.Font("Arial", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.taregtHostLabel.Location = new System.Drawing.Point(361, 170);
            this.taregtHostLabel.Name = "taregtHostLabel";
            this.taregtHostLabel.Size = new System.Drawing.Size(80, 14);
            this.taregtHostLabel.TabIndex = 139;
            this.taregtHostLabel.Text = "vSphere Host *";
            this.taregtHostLabel.Visible = false;
            // 
            // masterHostComboBox
            // 
            this.masterHostComboBox.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.masterHostComboBox.FormattingEnabled = true;
            this.masterHostComboBox.Location = new System.Drawing.Point(525, 163);
            this.masterHostComboBox.Name = "masterHostComboBox";
            this.masterHostComboBox.Size = new System.Drawing.Size(121, 21);
            this.masterHostComboBox.TabIndex = 140;
            this.masterHostComboBox.Visible = false;
            this.masterHostComboBox.SelectedValueChanged += new System.EventHandler(this.masterHostComboBox_SelectedValueChanged);
            // 
            // targetDetailsPanel
            // 
            this.targetDetailsPanel.BackColor = System.Drawing.Color.Transparent;
            this.targetDetailsPanel.BackgroundImage = global::Com.Inmage.Wizard.Properties.Resources.workarea;
            this.targetDetailsPanel.Controls.Add(this.masterTargetDataStoreComboBox);
            this.targetDetailsPanel.Controls.Add(this.masterTargetDataStoreLabel);
            this.targetDetailsPanel.Controls.Add(this.masterHostComboBox);
            this.targetDetailsPanel.Controls.Add(this.taregtHostLabel);
            this.targetDetailsPanel.Controls.Add(this.masterTargetLabel);
            this.targetDetailsPanel.Controls.Add(this.masterTargetTextBox);
            this.targetDetailsPanel.Controls.Add(this.targetIpLabel);
            this.targetDetailsPanel.Controls.Add(this.enterEsxLabel);
            this.targetDetailsPanel.Controls.Add(this.secondaryServerPanelAddEsxButton);
            this.targetDetailsPanel.Controls.Add(this.targetPassWordText);
            this.targetDetailsPanel.Controls.Add(this.targetPassWordLabel);
            this.targetDetailsPanel.Controls.Add(this.targetUserNameText);
            this.targetDetailsPanel.Controls.Add(this.targetUserNameLabel);
            this.targetDetailsPanel.Controls.Add(this.targetIpText);
            this.targetDetailsPanel.Controls.Add(this.hostToImportLabel);
            this.targetDetailsPanel.Controls.Add(this.targetServerDataGridView);
            this.targetDetailsPanel.Location = new System.Drawing.Point(170, 45);
            this.targetDetailsPanel.Name = "targetDetailsPanel";
            this.targetDetailsPanel.Size = new System.Drawing.Size(683, 413);
            this.targetDetailsPanel.TabIndex = 116;
            // 
            // masterTargetDataStoreComboBox
            // 
            this.masterTargetDataStoreComboBox.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.masterTargetDataStoreComboBox.FormattingEnabled = true;
            this.masterTargetDataStoreComboBox.Location = new System.Drawing.Point(525, 200);
            this.masterTargetDataStoreComboBox.Name = "masterTargetDataStoreComboBox";
            this.masterTargetDataStoreComboBox.Size = new System.Drawing.Size(123, 21);
            this.masterTargetDataStoreComboBox.TabIndex = 145;
            this.masterTargetDataStoreComboBox.Visible = false;
            this.masterTargetDataStoreComboBox.Click += new System.EventHandler(this.masterTargetDataStoreComboBox_Click);
            // 
            // masterTargetDataStoreLabel
            // 
            this.masterTargetDataStoreLabel.AutoSize = true;
            this.masterTargetDataStoreLabel.Location = new System.Drawing.Point(360, 205);
            this.masterTargetDataStoreLabel.Name = "masterTargetDataStoreLabel";
            this.masterTargetDataStoreLabel.Size = new System.Drawing.Size(136, 13);
            this.masterTargetDataStoreLabel.TabIndex = 144;
            this.masterTargetDataStoreLabel.Text = "Master Target\'s Datastore *";
            this.masterTargetDataStoreLabel.Visible = false;
            // 
            // applicationPanel
            // 
            this.applicationPanel.BackgroundImage = global::Com.Inmage.Wizard.Properties.Resources.leftBg;
            this.applicationPanel.BackgroundImageLayout = System.Windows.Forms.ImageLayout.Stretch;
            this.applicationPanel.Controls.Add(this.helpPanel);
            this.applicationPanel.Controls.Add(this.helpPictureBox);
            this.applicationPanel.Controls.Add(this.p2vHeadingLabel);
            this.applicationPanel.Controls.Add(this.addCredentialsLabel);
            this.applicationPanel.Controls.Add(this.credentialPictureBox);
            this.applicationPanel.Location = new System.Drawing.Point(0, 44);
            this.applicationPanel.Name = "applicationPanel";
            this.applicationPanel.Size = new System.Drawing.Size(168, 550);
            this.applicationPanel.TabIndex = 110;
            // 
            // helpPanel
            // 
            this.helpPanel.BackColor = System.Drawing.Color.Transparent;
            this.helpPanel.BackgroundImage = global::Com.Inmage.Wizard.Properties.Resources.helpOpen2;
            this.helpPanel.Controls.Add(this.helpContentLabel);
            this.helpPanel.Controls.Add(this.esxProtectionLinkLabel);
            this.helpPanel.Controls.Add(this.closePictureBox);
            this.helpPanel.Location = new System.Drawing.Point(0, 251);
            this.helpPanel.Name = "helpPanel";
            this.helpPanel.Size = new System.Drawing.Size(163, 296);
            this.helpPanel.TabIndex = 137;
            this.helpPanel.Visible = false;
            // 
            // helpContentLabel
            // 
            this.helpContentLabel.ForeColor = System.Drawing.Color.Black;
            this.helpContentLabel.Location = new System.Drawing.Point(7, 45);
            this.helpContentLabel.Name = "helpContentLabel";
            this.helpContentLabel.Size = new System.Drawing.Size(148, 215);
            this.helpContentLabel.TabIndex = 5;
            // 
            // esxProtectionLinkLabel
            // 
            this.esxProtectionLinkLabel.AutoSize = true;
            this.esxProtectionLinkLabel.Cursor = System.Windows.Forms.Cursors.Hand;
            this.esxProtectionLinkLabel.LinkColor = System.Drawing.Color.FromArgb(((int)(((byte)(210)))), ((int)(((byte)(0)))), ((int)(((byte)(0)))));
            this.esxProtectionLinkLabel.Location = new System.Drawing.Point(4, 274);
            this.esxProtectionLinkLabel.Name = "esxProtectionLinkLabel";
            this.esxProtectionLinkLabel.Size = new System.Drawing.Size(105, 13);
            this.esxProtectionLinkLabel.TabIndex = 3;
            this.esxProtectionLinkLabel.TabStop = true;
            this.esxProtectionLinkLabel.Text = " Offliune Sync Import";
            this.esxProtectionLinkLabel.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
            this.esxProtectionLinkLabel.LinkClicked += new System.Windows.Forms.LinkLabelLinkClickedEventHandler(this.esxProtectionLinkLabel_LinkClicked);
            // 
            // closePictureBox
            // 
            this.closePictureBox.BackgroundImage = global::Com.Inmage.Wizard.Properties.Resources.closeBtn;
            this.closePictureBox.BackgroundImageLayout = System.Windows.Forms.ImageLayout.None;
            this.closePictureBox.Location = new System.Drawing.Point(141, 11);
            this.closePictureBox.Name = "closePictureBox";
            this.closePictureBox.Size = new System.Drawing.Size(14, 14);
            this.closePictureBox.TabIndex = 0;
            this.closePictureBox.TabStop = false;
            this.closePictureBox.Click += new System.EventHandler(this.closePictureBox_Click);
            // 
            // helpPictureBox
            // 
            this.helpPictureBox.BackgroundImage = global::Com.Inmage.Wizard.Properties.Resources.helpClose1;
            this.helpPictureBox.BackgroundImageLayout = System.Windows.Forms.ImageLayout.None;
            this.helpPictureBox.Location = new System.Drawing.Point(0, 516);
            this.helpPictureBox.Name = "helpPictureBox";
            this.helpPictureBox.Size = new System.Drawing.Size(168, 33);
            this.helpPictureBox.TabIndex = 2;
            this.helpPictureBox.TabStop = false;
            this.helpPictureBox.Click += new System.EventHandler(this.helpPictureBox_Click);
            // 
            // p2vHeadingLabel
            // 
            this.p2vHeadingLabel.AutoSize = true;
            this.p2vHeadingLabel.BackColor = System.Drawing.Color.Transparent;
            this.p2vHeadingLabel.Font = new System.Drawing.Font("Tahoma", 11.25F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.p2vHeadingLabel.ForeColor = System.Drawing.Color.White;
            this.p2vHeadingLabel.Location = new System.Drawing.Point(9, 23);
            this.p2vHeadingLabel.Name = "p2vHeadingLabel";
            this.p2vHeadingLabel.Size = new System.Drawing.Size(155, 18);
            this.p2vHeadingLabel.TabIndex = 88;
            this.p2vHeadingLabel.Text = "Offline Sync Import";
            // 
            // addCredentialsLabel
            // 
            this.addCredentialsLabel.AutoSize = true;
            this.addCredentialsLabel.BackColor = System.Drawing.Color.Transparent;
            this.addCredentialsLabel.Font = new System.Drawing.Font("Tahoma", 8.25F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.addCredentialsLabel.ForeColor = System.Drawing.Color.White;
            this.addCredentialsLabel.Location = new System.Drawing.Point(35, 83);
            this.addCredentialsLabel.Name = "addCredentialsLabel";
            this.addCredentialsLabel.Size = new System.Drawing.Size(96, 13);
            this.addCredentialsLabel.TabIndex = 89;
            this.addCredentialsLabel.Text = "VM(s) to import";
            this.addCredentialsLabel.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
            // 
            // credentialPictureBox
            // 
            this.credentialPictureBox.BackColor = System.Drawing.Color.Transparent;
            this.credentialPictureBox.Image = global::Com.Inmage.Wizard.Properties.Resources.arrow1;
            this.credentialPictureBox.Location = new System.Drawing.Point(3, 78);
            this.credentialPictureBox.Name = "credentialPictureBox";
            this.credentialPictureBox.Size = new System.Drawing.Size(38, 24);
            this.credentialPictureBox.SizeMode = System.Windows.Forms.PictureBoxSizeMode.CenterImage;
            this.credentialPictureBox.TabIndex = 88;
            this.credentialPictureBox.TabStop = false;
            // 
            // mandatoryLabel
            // 
            this.mandatoryLabel.AutoSize = true;
            this.mandatoryLabel.BackColor = System.Drawing.Color.Transparent;
            this.mandatoryLabel.ForeColor = System.Drawing.Color.Black;
            this.mandatoryLabel.Location = new System.Drawing.Point(174, 566);
            this.mandatoryLabel.Name = "mandatoryLabel";
            this.mandatoryLabel.Size = new System.Drawing.Size(103, 13);
            this.mandatoryLabel.TabIndex = 143;
            this.mandatoryLabel.Text = "( * ) Mandatory fields";
            // 
            // ImportOfflineSyncForm
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.AutoScroll = true;
            this.BackColor = System.Drawing.Color.White;
            this.ClientSize = new System.Drawing.Size(854, 594);
            this.Controls.Add(this.mandatoryLabel);
            this.Controls.Add(this.headingPanel);
            this.Controls.Add(this.generalLogTextBox);
            this.Controls.Add(this.nextButton);
            this.Controls.Add(this.cancelButton);
            this.Controls.Add(this.applicationPanel);
            this.Controls.Add(this.navigationPanel);
            this.Controls.Add(this.targetDetailsPanel);
            this.Controls.Add(this.doneButton);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.MaximizeBox = false;
            this.Name = "ImportOfflineSyncForm";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
            this.Text = "Offline Sync Import";
            this.headingPanel.ResumeLayout(false);
            this.headingPanel.PerformLayout();
            this.navigationPanel.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.targetServerDataGridView)).EndInit();
            this.targetDetailsPanel.ResumeLayout(false);
            this.targetDetailsPanel.PerformLayout();
            this.applicationPanel.ResumeLayout(false);
            this.applicationPanel.PerformLayout();
            this.helpPanel.ResumeLayout(false);
            this.helpPanel.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this.closePictureBox)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.helpPictureBox)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.credentialPictureBox)).EndInit();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private BufferedPanel applicationPanel;
        public System.Windows.Forms.Label p2vHeadingLabel;
        private System.Windows.Forms.Label addCredentialsLabel;
        private System.Windows.Forms.PictureBox credentialPictureBox;
        public System.Windows.Forms.Button doneButton;
        private System.Windows.Forms.Panel headingPanel;
        private System.Windows.Forms.Panel navigationPanel;
        private System.Windows.Forms.LinkLabel esxProtectionLinkLabel;
        private System.Windows.Forms.PictureBox closePictureBox;
        private System.Windows.Forms.PictureBox helpPictureBox;
        public System.Windows.Forms.Label hostToImportLabel;
        private System.Windows.Forms.Label targetUserNameLabel;
        private System.Windows.Forms.Label targetPassWordLabel;
        private System.Windows.Forms.Label enterEsxLabel;
        private System.Windows.Forms.Label targetIpLabel;
        public System.Windows.Forms.Label helpContentLabel;
        private System.Windows.Forms.Label versionLabel;
        private System.Windows.Forms.Label patchLabel;
        private System.Windows.Forms.DataGridViewTextBoxColumn esx_ip;
        private System.Windows.Forms.DataGridViewTextBoxColumn ipTarget;
        private System.Windows.Forms.DataGridViewTextBoxColumn ip;
        internal System.Windows.Forms.Label mandatoryLabel;
        internal System.ComponentModel.BackgroundWorker getTargetDetailsBackgroundWorker;
        internal System.ComponentModel.BackgroundWorker targetScriptBackgroundWorker;
        internal System.Windows.Forms.TextBox targetUserNameText;
        internal System.Windows.Forms.TextBox targetPassWordText;
        internal System.Windows.Forms.Button secondaryServerPanelAddEsxButton;
        internal System.Windows.Forms.TextBox masterTargetTextBox;
        internal System.Windows.Forms.ComboBox masterHostComboBox;
        internal System.Windows.Forms.Panel targetDetailsPanel;
        internal System.Windows.Forms.ComboBox masterTargetDataStoreComboBox;
        internal System.Windows.Forms.TextBox generalLogTextBox;
        internal System.Windows.Forms.DataGridView targetServerDataGridView;
        internal System.Windows.Forms.TextBox targetIpText;
        internal System.Windows.Forms.Label taregtHostLabel;
        internal System.Windows.Forms.Label masterTargetLabel;
        internal System.Windows.Forms.Label masterTargetDataStoreLabel;
        internal System.Windows.Forms.ProgressBar progressBar;
        internal System.Windows.Forms.Button nextButton;
        internal System.Windows.Forms.Button cancelButton;
        internal System.Windows.Forms.Panel helpPanel;
    }
}