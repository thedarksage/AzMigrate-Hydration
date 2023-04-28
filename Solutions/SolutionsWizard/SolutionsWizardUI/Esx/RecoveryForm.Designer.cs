namespace com.InMage.Wizard
{
    partial class RecoveryForm
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
            System.Windows.Forms.DataGridViewCellStyle dataGridViewCellStyle1 = new System.Windows.Forms.DataGridViewCellStyle();
            System.Windows.Forms.DataGridViewCellStyle dataGridViewCellStyle2 = new System.Windows.Forms.DataGridViewCellStyle();
            System.Windows.Forms.DataGridViewCellStyle dataGridViewCellStyle3 = new System.Windows.Forms.DataGridViewCellStyle();
            System.Windows.Forms.DataGridViewCellStyle dataGridViewCellStyle4 = new System.Windows.Forms.DataGridViewCellStyle();
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(RecoveryForm));
            this.nextButton = new System.Windows.Forms.Button();
            this.cancelButton = new System.Windows.Forms.Button();
            this.doneButton = new System.Windows.Forms.Button();
            this.errorProvider = new System.Windows.Forms.ErrorProvider(this.components);
            this.previousButton = new System.Windows.Forms.Button();
            this.recoveryDetailsBackgroundWorker = new System.ComponentModel.BackgroundWorker();
            this.recoveryBackgroundWorker = new System.ComponentModel.BackgroundWorker();
            this.credentialCheckBackgroundWorker = new System.ComponentModel.BackgroundWorker();
            this.headingPanel = new System.Windows.Forms.Panel();
            this.versionLabel = new System.Windows.Forms.Label();
            this.panel1 = new System.Windows.Forms.Panel();
            this.progressBar = new System.Windows.Forms.ProgressBar();
            this.reviewPanel = new System.Windows.Forms.Panel();
            this.monitiorLinkLabel = new System.Windows.Forms.LinkLabel();
            this.generalLogTextBox = new System.Windows.Forms.TextBox();
            this.selectedMachineListLabel = new System.Windows.Forms.Label();
            this.recoveryReviewDataGridView = new System.Windows.Forms.DataGridView();
            this.displayName = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.IP = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.SubnetMask = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.DefaultGateWay = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.dns = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.tag = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.tagType = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.recoverOrder = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.configurationPanel = new System.Windows.Forms.Panel();
            this.recoverDataGridView = new System.Windows.Forms.DataGridView();
            this.hostName = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.os = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.masterTarget = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.recover = new System.Windows.Forms.DataGridViewCheckBoxColumn();
            this.targetEsxUserNameLabel = new System.Windows.Forms.Label();
            this.targetEsxUserNameTextBox = new System.Windows.Forms.TextBox();
            this.targetEsxPasswordTextBox = new System.Windows.Forms.TextBox();
            this.targetEsxPasswordLabel = new System.Windows.Forms.Label();
            this.teargetEsxIPLabel = new System.Windows.Forms.Label();
            this.targetEsxIPText = new System.Windows.Forms.TextBox();
            this.getDetailsButton = new System.Windows.Forms.Button();
            this.tagPanel = new System.Windows.Forms.Panel();
            this.specificTimeDateTimePicker = new System.Windows.Forms.DateTimePicker();
            this.specificTimeRadioButton = new System.Windows.Forms.RadioButton();
            this.dateTimePicker = new System.Windows.Forms.DateTimePicker();
            this.userDefinedTimeRadioButton = new System.Windows.Forms.RadioButton();
            this.timeBasedRadioButton = new System.Windows.Forms.RadioButton();
            this.tagBasedRadioButton = new System.Windows.Forms.RadioButton();
            this.recoveryTagLabels = new System.Windows.Forms.Label();
            this.label1 = new System.Windows.Forms.Label();
            this.osTypeComboBox = new System.Windows.Forms.ComboBox();
            this.planNameLabel = new System.Windows.Forms.Label();
            this.planNameText = new System.Windows.Forms.TextBox();
            this.osTypeLabel = new System.Windows.Forms.Label();
            this.preReqsButton = new System.Windows.Forms.Button();
            this.recoveryTabControl = new System.Windows.Forms.TabControl();
            this.logTabPage = new System.Windows.Forms.TabPage();
            this.recoveryText = new System.Windows.Forms.TextBox();
            this.clearLogLinkLabel = new System.Windows.Forms.LinkLabel();
            this.runReadinessCheckTabPage = new System.Windows.Forms.TabPage();
            this.checkReportDataGridView = new System.Windows.Forms.DataGridView();
            this.host = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.check = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.result = new System.Windows.Forms.DataGridViewImageColumn();
            this.action = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.selectAllCheckBoxForRecovery = new System.Windows.Forms.CheckBox();
            this.recoverPanel = new System.Windows.Forms.Panel();
            this.applicationPanel = new com.InMage.Wizard.BufferedPanel();
            this.nwConfigurationLabelLabel = new System.Windows.Forms.Label();
            this.configurationPictureBox = new System.Windows.Forms.PictureBox();
            this.helpPanel = new System.Windows.Forms.Panel();
            this.helpContentLabel = new System.Windows.Forms.Label();
            this.esxProtectionLinkLabel = new System.Windows.Forms.LinkLabel();
            this.closePictureBox = new System.Windows.Forms.PictureBox();
            this.recoverLabel = new System.Windows.Forms.Label();
            this.reviewPictureBox = new System.Windows.Forms.PictureBox();
            this.p2vHeadingLabel = new System.Windows.Forms.Label();
            this.addCredentialsLabel = new System.Windows.Forms.Label();
            this.recoverPictureBox = new System.Windows.Forms.PictureBox();
            this.helpPictureBox = new System.Windows.Forms.PictureBox();
            this.configurationScreen = new com.InMage.Wizard.ConfigurationScreen();
            ((System.ComponentModel.ISupportInitialize)(this.errorProvider)).BeginInit();
            this.headingPanel.SuspendLayout();
            this.panel1.SuspendLayout();
            this.reviewPanel.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.recoveryReviewDataGridView)).BeginInit();
            this.configurationPanel.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.recoverDataGridView)).BeginInit();
            this.tagPanel.SuspendLayout();
            this.recoveryTabControl.SuspendLayout();
            this.logTabPage.SuspendLayout();
            this.runReadinessCheckTabPage.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.checkReportDataGridView)).BeginInit();
            this.recoverPanel.SuspendLayout();
            this.applicationPanel.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.configurationPictureBox)).BeginInit();
            this.helpPanel.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.closePictureBox)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.reviewPictureBox)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.recoverPictureBox)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.helpPictureBox)).BeginInit();
            this.SuspendLayout();
            // 
            // nextButton
            // 
            this.nextButton.Enabled = false;
            this.nextButton.Location = new System.Drawing.Point(674, 559);
            this.nextButton.Name = "nextButton";
            this.nextButton.Size = new System.Drawing.Size(75, 23);
            this.nextButton.TabIndex = 110;
            this.nextButton.Text = "Next >";
            this.nextButton.UseVisualStyleBackColor = true;
            this.nextButton.Click += new System.EventHandler(this.nextButton_Click);
            // 
            // cancelButton
            // 
            this.cancelButton.Location = new System.Drawing.Point(766, 559);
            this.cancelButton.Name = "cancelButton";
            this.cancelButton.Size = new System.Drawing.Size(75, 23);
            this.cancelButton.TabIndex = 109;
            this.cancelButton.Text = "Cancel";
            this.cancelButton.UseVisualStyleBackColor = true;
            this.cancelButton.Click += new System.EventHandler(this.cancelButton_Click);
            // 
            // doneButton
            // 
            this.doneButton.Location = new System.Drawing.Point(766, 559);
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
            // previousButton
            // 
            this.previousButton.Location = new System.Drawing.Point(575, 559);
            this.previousButton.Name = "previousButton";
            this.previousButton.Size = new System.Drawing.Size(75, 23);
            this.previousButton.TabIndex = 115;
            this.previousButton.Text = "Previous";
            this.previousButton.UseVisualStyleBackColor = true;
            this.previousButton.Visible = false;
            this.previousButton.Click += new System.EventHandler(this.previousButton_Click);
            // 
            // recoveryDetailsBackgroundWorker
            // 
            this.recoveryDetailsBackgroundWorker.DoWork += new System.ComponentModel.DoWorkEventHandler(this.recoveryDetailsBackgroundWorker_DoWork);
            this.recoveryDetailsBackgroundWorker.RunWorkerCompleted += new System.ComponentModel.RunWorkerCompletedEventHandler(this.recoveryDetailsBackgroundWorker_RunWorkerCompleted);
            this.recoveryDetailsBackgroundWorker.ProgressChanged += new System.ComponentModel.ProgressChangedEventHandler(this.recoveryDetailsBackgroundWorker_ProgressChanged);
            // 
            // recoveryBackgroundWorker
            // 
            this.recoveryBackgroundWorker.DoWork += new System.ComponentModel.DoWorkEventHandler(this.recoveryBackgroundWorker_DoWork);
            this.recoveryBackgroundWorker.RunWorkerCompleted += new System.ComponentModel.RunWorkerCompletedEventHandler(this.recoveryBackgroundWorker_RunWorkerCompleted);
            this.recoveryBackgroundWorker.ProgressChanged += new System.ComponentModel.ProgressChangedEventHandler(this.recoveryBackgroundWorker_ProgressChanged);
            // 
            // credentialCheckBackgroundWorker
            // 
            this.credentialCheckBackgroundWorker.DoWork += new System.ComponentModel.DoWorkEventHandler(this.credentialCheckBackgroundWorker_DoWork);
            this.credentialCheckBackgroundWorker.RunWorkerCompleted += new System.ComponentModel.RunWorkerCompletedEventHandler(this.credentialCheckBackgroundWorker_RunWorkerCompleted);
            this.credentialCheckBackgroundWorker.ProgressChanged += new System.ComponentModel.ProgressChangedEventHandler(this.credentialCheckBackgroundWorker_ProgressChanged);
            // 
            // headingPanel
            // 
            this.headingPanel.BackColor = System.Drawing.Color.Transparent;
            this.headingPanel.BackgroundImage = global::com.InMage.Wizard.Properties.Resources.heading;
            this.headingPanel.Controls.Add(this.versionLabel);
            this.headingPanel.Location = new System.Drawing.Point(0, 0);
            this.headingPanel.Name = "headingPanel";
            this.headingPanel.Size = new System.Drawing.Size(850, 44);
            this.headingPanel.TabIndex = 118;
            // 
            // versionLabel
            // 
            this.versionLabel.AutoSize = true;
            this.versionLabel.Font = new System.Drawing.Font("Tahoma", 6.75F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.versionLabel.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(64)))), ((int)(((byte)(64)))), ((int)(((byte)(64)))));
            this.versionLabel.Location = new System.Drawing.Point(629, 27);
            this.versionLabel.Name = "versionLabel";
            this.versionLabel.Size = new System.Drawing.Size(98, 11);
            this.versionLabel.TabIndex = 1;
            this.versionLabel.Text = "5.50.1.GA.2047.1";
            // 
            // panel1
            // 
            this.panel1.BackgroundImage = global::com.InMage.Wizard.Properties.Resources.navBar;
            this.panel1.Controls.Add(this.progressBar);
            this.panel1.Location = new System.Drawing.Point(169, 544);
            this.panel1.Name = "panel1";
            this.panel1.Size = new System.Drawing.Size(679, 10);
            this.panel1.TabIndex = 117;
            // 
            // progressBar
            // 
            this.progressBar.BackColor = System.Drawing.Color.White;
            this.progressBar.ForeColor = System.Drawing.Color.ForestGreen;
            this.progressBar.Location = new System.Drawing.Point(2, 0);
            this.progressBar.Name = "progressBar";
            this.progressBar.Size = new System.Drawing.Size(676, 10);
            this.progressBar.Style = System.Windows.Forms.ProgressBarStyle.Marquee;
            this.progressBar.TabIndex = 116;
            this.progressBar.Visible = false;
            // 
            // reviewPanel
            // 
            this.reviewPanel.BackColor = System.Drawing.Color.Transparent;
            this.reviewPanel.BackgroundImage = global::com.InMage.Wizard.Properties.Resources.workarea;
            this.reviewPanel.Controls.Add(this.monitiorLinkLabel);
            this.reviewPanel.Controls.Add(this.generalLogTextBox);
            this.reviewPanel.Controls.Add(this.selectedMachineListLabel);
            this.reviewPanel.Controls.Add(this.recoveryReviewDataGridView);
            this.reviewPanel.Location = new System.Drawing.Point(169, 46);
            this.reviewPanel.Name = "reviewPanel";
            this.reviewPanel.Size = new System.Drawing.Size(679, 492);
            this.reviewPanel.TabIndex = 23;
            // 
            // monitiorLinkLabel
            // 
            this.monitiorLinkLabel.AutoSize = true;
            this.monitiorLinkLabel.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.monitiorLinkLabel.LinkColor = System.Drawing.Color.SteelBlue;
            this.monitiorLinkLabel.Location = new System.Drawing.Point(536, 472);
            this.monitiorLinkLabel.Name = "monitiorLinkLabel";
            this.monitiorLinkLabel.Size = new System.Drawing.Size(124, 13);
            this.monitiorLinkLabel.TabIndex = 117;
            this.monitiorLinkLabel.TabStop = true;
            this.monitiorLinkLabel.Text = "Click here to monitor";
            this.monitiorLinkLabel.Visible = false;
            this.monitiorLinkLabel.LinkClicked += new System.Windows.Forms.LinkLabelLinkClickedEventHandler(this.monitiorLinkLabel_LinkClicked);
            // 
            // generalLogTextBox
            // 
            this.generalLogTextBox.BackColor = System.Drawing.Color.Linen;
            this.generalLogTextBox.Location = new System.Drawing.Point(13, 333);
            this.generalLogTextBox.Multiline = true;
            this.generalLogTextBox.Name = "generalLogTextBox";
            this.generalLogTextBox.ReadOnly = true;
            this.generalLogTextBox.ScrollBars = System.Windows.Forms.ScrollBars.Both;
            this.generalLogTextBox.Size = new System.Drawing.Size(647, 129);
            this.generalLogTextBox.TabIndex = 2;
            // 
            // selectedMachineListLabel
            // 
            this.selectedMachineListLabel.AutoSize = true;
            this.selectedMachineListLabel.Font = new System.Drawing.Font("Arial", 9F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.selectedMachineListLabel.Location = new System.Drawing.Point(12, 16);
            this.selectedMachineListLabel.Name = "selectedMachineListLabel";
            this.selectedMachineListLabel.Size = new System.Drawing.Size(219, 15);
            this.selectedMachineListLabel.TabIndex = 1;
            this.selectedMachineListLabel.Text = "Following machines will be recovered";
            this.selectedMachineListLabel.TextAlign = System.Drawing.ContentAlignment.TopCenter;
            // 
            // recoveryReviewDataGridView
            // 
            this.recoveryReviewDataGridView.AllowUserToAddRows = false;
            this.recoveryReviewDataGridView.AllowUserToResizeColumns = false;
            this.recoveryReviewDataGridView.AllowUserToResizeRows = false;
            this.recoveryReviewDataGridView.AutoSizeColumnsMode = System.Windows.Forms.DataGridViewAutoSizeColumnsMode.Fill;
            this.recoveryReviewDataGridView.AutoSizeRowsMode = System.Windows.Forms.DataGridViewAutoSizeRowsMode.AllCells;
            this.recoveryReviewDataGridView.BackgroundColor = System.Drawing.Color.White;
            this.recoveryReviewDataGridView.BorderStyle = System.Windows.Forms.BorderStyle.None;
            this.recoveryReviewDataGridView.ColumnHeadersBorderStyle = System.Windows.Forms.DataGridViewHeaderBorderStyle.None;
            this.recoveryReviewDataGridView.ColumnHeadersHeight = 30;
            this.recoveryReviewDataGridView.ColumnHeadersHeightSizeMode = System.Windows.Forms.DataGridViewColumnHeadersHeightSizeMode.DisableResizing;
            this.recoveryReviewDataGridView.Columns.AddRange(new System.Windows.Forms.DataGridViewColumn[] {
            this.displayName,
            this.IP,
            this.SubnetMask,
            this.DefaultGateWay,
            this.dns,
            this.tag,
            this.tagType,
            this.recoverOrder});
            this.recoveryReviewDataGridView.Location = new System.Drawing.Point(12, 45);
            this.recoveryReviewDataGridView.Name = "recoveryReviewDataGridView";
            this.recoveryReviewDataGridView.RowHeadersBorderStyle = System.Windows.Forms.DataGridViewHeaderBorderStyle.None;
            this.recoveryReviewDataGridView.RowHeadersVisible = false;
            this.recoveryReviewDataGridView.Size = new System.Drawing.Size(635, 198);
            this.recoveryReviewDataGridView.TabIndex = 0;
            this.recoveryReviewDataGridView.CellValidating += new System.Windows.Forms.DataGridViewCellValidatingEventHandler(this.recoveryReviewDataGridView_CellValidating);
            // 
            // displayName
            // 
            dataGridViewCellStyle1.Alignment = System.Windows.Forms.DataGridViewContentAlignment.TopLeft;
            this.displayName.DefaultCellStyle = dataGridViewCellStyle1;
            this.displayName.HeaderText = "Host Name";
            this.displayName.Name = "displayName";
            this.displayName.ReadOnly = true;
            // 
            // IP
            // 
            this.IP.HeaderText = "IP";
            this.IP.Name = "IP";
            this.IP.ReadOnly = true;
            // 
            // SubnetMask
            // 
            this.SubnetMask.HeaderText = "Subnet Mask";
            this.SubnetMask.Name = "SubnetMask";
            this.SubnetMask.ReadOnly = true;
            // 
            // DefaultGateWay
            // 
            this.DefaultGateWay.HeaderText = "Default Gateway";
            this.DefaultGateWay.Name = "DefaultGateWay";
            this.DefaultGateWay.ReadOnly = true;
            // 
            // dns
            // 
            this.dns.HeaderText = "DNS";
            this.dns.Name = "dns";
            this.dns.ReadOnly = true;
            // 
            // tag
            // 
            this.tag.HeaderText = "Tag";
            this.tag.Name = "tag";
            this.tag.ReadOnly = true;
            // 
            // tagType
            // 
            this.tagType.HeaderText = "Tag Type";
            this.tagType.Name = "tagType";
            this.tagType.ReadOnly = true;
            // 
            // recoverOrder
            // 
            this.recoverOrder.AutoSizeMode = System.Windows.Forms.DataGridViewAutoSizeColumnMode.ColumnHeader;
            this.recoverOrder.HeaderText = "Recovery Order";
            this.recoverOrder.Name = "recoverOrder";
            this.recoverOrder.Resizable = System.Windows.Forms.DataGridViewTriState.True;
            this.recoverOrder.Width = 98;
            // 
            // configurationPanel
            // 
            this.configurationPanel.BackgroundImage = global::com.InMage.Wizard.Properties.Resources.workarea;
            this.configurationPanel.Controls.Add(this.configurationScreen);
            this.configurationPanel.Location = new System.Drawing.Point(167, 44);
            this.configurationPanel.Name = "configurationPanel";
            this.configurationPanel.Size = new System.Drawing.Size(679, 494);
            this.configurationPanel.TabIndex = 139;
            // 
            // recoverDataGridView
            // 
            this.recoverDataGridView.AllowUserToAddRows = false;
            this.recoverDataGridView.AllowUserToResizeColumns = false;
            this.recoverDataGridView.BackgroundColor = System.Drawing.Color.White;
            this.recoverDataGridView.BorderStyle = System.Windows.Forms.BorderStyle.None;
            this.recoverDataGridView.ColumnHeadersHeightSizeMode = System.Windows.Forms.DataGridViewColumnHeadersHeightSizeMode.AutoSize;
            this.recoverDataGridView.Columns.AddRange(new System.Windows.Forms.DataGridViewColumn[] {
            this.hostName,
            this.os,
            this.masterTarget,
            this.recover});
            this.recoverDataGridView.Location = new System.Drawing.Point(9, 82);
            this.recoverDataGridView.Name = "recoverDataGridView";
            this.recoverDataGridView.RowHeadersVisible = false;
            this.recoverDataGridView.SelectionMode = System.Windows.Forms.DataGridViewSelectionMode.FullRowSelect;
            this.recoverDataGridView.Size = new System.Drawing.Size(373, 160);
            this.recoverDataGridView.TabIndex = 5;
            this.recoverDataGridView.Visible = false;
            this.recoverDataGridView.CellValueChanged += new System.Windows.Forms.DataGridViewCellEventHandler(this.recoverDataGridView_CellValueChanged);
            this.recoverDataGridView.CellClick += new System.Windows.Forms.DataGridViewCellEventHandler(this.recoverDataGridView_CellClick);
            this.recoverDataGridView.CurrentCellDirtyStateChanged += new System.EventHandler(this.recoverDataGridView_CurrentCellDirtyStateChanged);
            this.recoverDataGridView.CellContentClick += new System.Windows.Forms.DataGridViewCellEventHandler(this.recoverDataGridView_CellValueChanged);
            // 
            // hostName
            // 
            this.hostName.HeaderText = "Host Name";
            this.hostName.Name = "hostName";
            this.hostName.ReadOnly = true;
            this.hostName.Resizable = System.Windows.Forms.DataGridViewTriState.False;
            this.hostName.Width = 90;
            // 
            // os
            // 
            this.os.AutoSizeMode = System.Windows.Forms.DataGridViewAutoSizeColumnMode.None;
            this.os.HeaderText = "Display Name";
            this.os.Name = "os";
            this.os.ReadOnly = true;
            this.os.Resizable = System.Windows.Forms.DataGridViewTriState.False;
            // 
            // masterTarget
            // 
            this.masterTarget.HeaderText = "Master Target";
            this.masterTarget.Name = "masterTarget";
            this.masterTarget.ReadOnly = true;
            this.masterTarget.Resizable = System.Windows.Forms.DataGridViewTriState.False;
            // 
            // recover
            // 
            this.recover.AutoSizeMode = System.Windows.Forms.DataGridViewAutoSizeColumnMode.None;
            this.recover.HeaderText = "Recover";
            this.recover.Name = "recover";
            this.recover.Resizable = System.Windows.Forms.DataGridViewTriState.False;
            this.recover.SortMode = System.Windows.Forms.DataGridViewColumnSortMode.Automatic;
            this.recover.Width = 72;
            // 
            // targetEsxUserNameLabel
            // 
            this.targetEsxUserNameLabel.AutoSize = true;
            this.targetEsxUserNameLabel.Font = new System.Drawing.Font("Arial", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.targetEsxUserNameLabel.Location = new System.Drawing.Point(129, 33);
            this.targetEsxUserNameLabel.Name = "targetEsxUserNameLabel";
            this.targetEsxUserNameLabel.Size = new System.Drawing.Size(56, 14);
            this.targetEsxUserNameLabel.TabIndex = 12;
            this.targetEsxUserNameLabel.Text = "Username";
            // 
            // targetEsxUserNameTextBox
            // 
            this.targetEsxUserNameTextBox.Location = new System.Drawing.Point(132, 50);
            this.targetEsxUserNameTextBox.Name = "targetEsxUserNameTextBox";
            this.targetEsxUserNameTextBox.Size = new System.Drawing.Size(100, 20);
            this.targetEsxUserNameTextBox.TabIndex = 2;
            // 
            // targetEsxPasswordTextBox
            // 
            this.targetEsxPasswordTextBox.Location = new System.Drawing.Point(246, 50);
            this.targetEsxPasswordTextBox.Name = "targetEsxPasswordTextBox";
            this.targetEsxPasswordTextBox.PasswordChar = '*';
            this.targetEsxPasswordTextBox.Size = new System.Drawing.Size(88, 20);
            this.targetEsxPasswordTextBox.TabIndex = 3;
            this.targetEsxPasswordTextBox.Enter += new System.EventHandler(this.targetEsxPasswordTextBox_Enter);
            // 
            // targetEsxPasswordLabel
            // 
            this.targetEsxPasswordLabel.AutoSize = true;
            this.targetEsxPasswordLabel.Font = new System.Drawing.Font("Arial", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.targetEsxPasswordLabel.Location = new System.Drawing.Point(243, 32);
            this.targetEsxPasswordLabel.Name = "targetEsxPasswordLabel";
            this.targetEsxPasswordLabel.Size = new System.Drawing.Size(57, 14);
            this.targetEsxPasswordLabel.TabIndex = 15;
            this.targetEsxPasswordLabel.Text = "Password";
            // 
            // teargetEsxIPLabel
            // 
            this.teargetEsxIPLabel.AutoSize = true;
            this.teargetEsxIPLabel.Font = new System.Drawing.Font("Arial", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.teargetEsxIPLabel.Location = new System.Drawing.Point(10, 33);
            this.teargetEsxIPLabel.Name = "teargetEsxIPLabel";
            this.teargetEsxIPLabel.Size = new System.Drawing.Size(92, 14);
            this.teargetEsxIPLabel.TabIndex = 16;
            this.teargetEsxIPLabel.Text = "Secondary Esx IP";
            // 
            // targetEsxIPText
            // 
            this.targetEsxIPText.Location = new System.Drawing.Point(13, 50);
            this.targetEsxIPText.Name = "targetEsxIPText";
            this.targetEsxIPText.Size = new System.Drawing.Size(100, 20);
            this.targetEsxIPText.TabIndex = 1;
            // 
            // getDetailsButton
            // 
            this.getDetailsButton.Location = new System.Drawing.Point(447, 48);
            this.getDetailsButton.Name = "getDetailsButton";
            this.getDetailsButton.Size = new System.Drawing.Size(75, 23);
            this.getDetailsButton.TabIndex = 5;
            this.getDetailsButton.Text = "Get Details";
            this.getDetailsButton.UseVisualStyleBackColor = true;
            this.getDetailsButton.Click += new System.EventHandler(this.getDetailsButton_Click);
            // 
            // tagPanel
            // 
            this.tagPanel.Controls.Add(this.specificTimeDateTimePicker);
            this.tagPanel.Controls.Add(this.specificTimeRadioButton);
            this.tagPanel.Controls.Add(this.dateTimePicker);
            this.tagPanel.Controls.Add(this.userDefinedTimeRadioButton);
            this.tagPanel.Controls.Add(this.timeBasedRadioButton);
            this.tagPanel.Controls.Add(this.tagBasedRadioButton);
            this.tagPanel.Controls.Add(this.recoveryTagLabels);
            this.tagPanel.Location = new System.Drawing.Point(389, 81);
            this.tagPanel.Name = "tagPanel";
            this.tagPanel.Size = new System.Drawing.Size(284, 266);
            this.tagPanel.TabIndex = 21;
            this.tagPanel.Visible = false;
            // 
            // specificTimeDateTimePicker
            // 
            this.specificTimeDateTimePicker.Location = new System.Drawing.Point(60, 139);
            this.specificTimeDateTimePicker.Name = "specificTimeDateTimePicker";
            this.specificTimeDateTimePicker.Size = new System.Drawing.Size(200, 20);
            this.specificTimeDateTimePicker.TabIndex = 56;
            this.specificTimeDateTimePicker.Visible = false;
            this.specificTimeDateTimePicker.Leave += new System.EventHandler(this.specificTimeDateTimePicker_Leave);
            this.specificTimeDateTimePicker.CloseUp += new System.EventHandler(this.specificTimeDateTimePicker_CloseUp);
            // 
            // specificTimeRadioButton
            // 
            this.specificTimeRadioButton.AutoSize = true;
            this.specificTimeRadioButton.Location = new System.Drawing.Point(10, 116);
            this.specificTimeRadioButton.Name = "specificTimeRadioButton";
            this.specificTimeRadioButton.Size = new System.Drawing.Size(212, 17);
            this.specificTimeRadioButton.TabIndex = 55;
            this.specificTimeRadioButton.TabStop = true;
            this.specificTimeRadioButton.Text = "Specific Time(Local System Time Zone)";
            this.specificTimeRadioButton.UseVisualStyleBackColor = true;
            this.specificTimeRadioButton.CheckedChanged += new System.EventHandler(this.specificTimeRadioButton_CheckedChanged);
            // 
            // dateTimePicker
            // 
            this.dateTimePicker.Location = new System.Drawing.Point(58, 89);
            this.dateTimePicker.Name = "dateTimePicker";
            this.dateTimePicker.Size = new System.Drawing.Size(200, 20);
            this.dateTimePicker.TabIndex = 54;
            this.dateTimePicker.Visible = false;
            this.dateTimePicker.Leave += new System.EventHandler(this.dateTimePicker_Leave);
            this.dateTimePicker.CloseUp += new System.EventHandler(this.dateTimePicker_CloseUp);
            // 
            // userDefinedTimeRadioButton
            // 
            this.userDefinedTimeRadioButton.AutoSize = true;
            this.userDefinedTimeRadioButton.Location = new System.Drawing.Point(13, 66);
            this.userDefinedTimeRadioButton.Name = "userDefinedTimeRadioButton";
            this.userDefinedTimeRadioButton.Size = new System.Drawing.Size(255, 17);
            this.userDefinedTimeRadioButton.TabIndex = 53;
            this.userDefinedTimeRadioButton.TabStop = true;
            this.userDefinedTimeRadioButton.Text = "Tag at Specified Time (Local System Time Zone)";
            this.userDefinedTimeRadioButton.UseVisualStyleBackColor = true;
            this.userDefinedTimeRadioButton.CheckedChanged += new System.EventHandler(this.userDefinedTimeRadioButton_CheckedChanged);
            // 
            // timeBasedRadioButton
            // 
            this.timeBasedRadioButton.AutoSize = true;
            this.timeBasedRadioButton.Location = new System.Drawing.Point(13, 43);
            this.timeBasedRadioButton.Name = "timeBasedRadioButton";
            this.timeBasedRadioButton.Size = new System.Drawing.Size(80, 17);
            this.timeBasedRadioButton.TabIndex = 52;
            this.timeBasedRadioButton.Text = "Latest Time";
            this.timeBasedRadioButton.UseVisualStyleBackColor = true;
            this.timeBasedRadioButton.CheckedChanged += new System.EventHandler(this.timeBasedRadioButton_CheckedChanged);
            // 
            // tagBasedRadioButton
            // 
            this.tagBasedRadioButton.AutoSize = true;
            this.tagBasedRadioButton.Checked = true;
            this.tagBasedRadioButton.Location = new System.Drawing.Point(13, 21);
            this.tagBasedRadioButton.Name = "tagBasedRadioButton";
            this.tagBasedRadioButton.Size = new System.Drawing.Size(76, 17);
            this.tagBasedRadioButton.TabIndex = 51;
            this.tagBasedRadioButton.TabStop = true;
            this.tagBasedRadioButton.Text = "Latest Tag";
            this.tagBasedRadioButton.UseVisualStyleBackColor = true;
            this.tagBasedRadioButton.CheckedChanged += new System.EventHandler(this.tagBasedRadioButton_CheckedChanged);
            // 
            // recoveryTagLabels
            // 
            this.recoveryTagLabels.AutoSize = true;
            this.recoveryTagLabels.Font = new System.Drawing.Font("Arial", 9.75F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.recoveryTagLabels.Location = new System.Drawing.Point(24, 1);
            this.recoveryTagLabels.Name = "recoveryTagLabels";
            this.recoveryTagLabels.Size = new System.Drawing.Size(122, 16);
            this.recoveryTagLabels.TabIndex = 48;
            this.recoveryTagLabels.Text = "Recover based on";
            // 
            // label1
            // 
            this.label1.AutoSize = true;
            this.label1.Font = new System.Drawing.Font("Arial", 8.25F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.label1.Location = new System.Drawing.Point(12, 10);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(226, 14);
            this.label1.TabIndex = 22;
            this.label1.Text = "Provide secondary vSphere information";
            // 
            // osTypeComboBox
            // 
            this.osTypeComboBox.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.osTypeComboBox.FormattingEnabled = true;
            this.osTypeComboBox.Location = new System.Drawing.Point(348, 50);
            this.osTypeComboBox.Name = "osTypeComboBox";
            this.osTypeComboBox.Size = new System.Drawing.Size(80, 21);
            this.osTypeComboBox.TabIndex = 4;
            // 
            // planNameLabel
            // 
            this.planNameLabel.AutoSize = true;
            this.planNameLabel.Font = new System.Drawing.Font("Arial", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.planNameLabel.Location = new System.Drawing.Point(10, 290);
            this.planNameLabel.Name = "planNameLabel";
            this.planNameLabel.Size = new System.Drawing.Size(106, 14);
            this.planNameLabel.TabIndex = 49;
            this.planNameLabel.Text = "Recovery Plan name";
            this.planNameLabel.Visible = false;
            // 
            // planNameText
            // 
            this.planNameText.Location = new System.Drawing.Point(13, 307);
            this.planNameText.Name = "planNameText";
            this.planNameText.Size = new System.Drawing.Size(126, 20);
            this.planNameText.TabIndex = 50;
            this.planNameText.Visible = false;
            this.planNameText.KeyUp += new System.Windows.Forms.KeyEventHandler(this.planNameText_KeyUp);
            // 
            // osTypeLabel
            // 
            this.osTypeLabel.AutoSize = true;
            this.osTypeLabel.Font = new System.Drawing.Font("Arial", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.osTypeLabel.Location = new System.Drawing.Point(345, 30);
            this.osTypeLabel.Name = "osTypeLabel";
            this.osTypeLabel.Size = new System.Drawing.Size(80, 14);
            this.osTypeLabel.TabIndex = 59;
            this.osTypeLabel.Text = "Guest OS Type";
            // 
            // preReqsButton
            // 
            this.preReqsButton.Location = new System.Drawing.Point(10, 256);
            this.preReqsButton.Name = "preReqsButton";
            this.preReqsButton.Size = new System.Drawing.Size(126, 23);
            this.preReqsButton.TabIndex = 60;
            this.preReqsButton.Text = "Run Readiness check";
            this.preReqsButton.UseVisualStyleBackColor = true;
            this.preReqsButton.Visible = false;
            this.preReqsButton.Click += new System.EventHandler(this.preReqsButton_Click);
            // 
            // recoveryTabControl
            // 
            this.recoveryTabControl.Controls.Add(this.logTabPage);
            this.recoveryTabControl.Controls.Add(this.runReadinessCheckTabPage);
            this.recoveryTabControl.Location = new System.Drawing.Point(17, 353);
            this.recoveryTabControl.Name = "recoveryTabControl";
            this.recoveryTabControl.SelectedIndex = 0;
            this.recoveryTabControl.Size = new System.Drawing.Size(642, 138);
            this.recoveryTabControl.TabIndex = 63;
            this.recoveryTabControl.Visible = false;
            // 
            // logTabPage
            // 
            this.logTabPage.Controls.Add(this.recoveryText);
            this.logTabPage.Controls.Add(this.clearLogLinkLabel);
            this.logTabPage.Location = new System.Drawing.Point(4, 22);
            this.logTabPage.Name = "logTabPage";
            this.logTabPage.Padding = new System.Windows.Forms.Padding(3);
            this.logTabPage.Size = new System.Drawing.Size(634, 112);
            this.logTabPage.TabIndex = 0;
            this.logTabPage.Text = "Log";
            this.logTabPage.UseVisualStyleBackColor = true;
            // 
            // recoveryText
            // 
            this.recoveryText.BackColor = System.Drawing.Color.Linen;
            this.recoveryText.Location = new System.Drawing.Point(8, 22);
            this.recoveryText.Multiline = true;
            this.recoveryText.Name = "recoveryText";
            this.recoveryText.ReadOnly = true;
            this.recoveryText.ScrollBars = System.Windows.Forms.ScrollBars.Vertical;
            this.recoveryText.Size = new System.Drawing.Size(626, 84);
            this.recoveryText.TabIndex = 61;
            // 
            // clearLogLinkLabel
            // 
            this.clearLogLinkLabel.AutoSize = true;
            this.clearLogLinkLabel.Location = new System.Drawing.Point(575, 2);
            this.clearLogLinkLabel.Name = "clearLogLinkLabel";
            this.clearLogLinkLabel.Size = new System.Drawing.Size(48, 13);
            this.clearLogLinkLabel.TabIndex = 62;
            this.clearLogLinkLabel.TabStop = true;
            this.clearLogLinkLabel.Text = "Clear log";
            this.clearLogLinkLabel.Visible = false;
            this.clearLogLinkLabel.LinkClicked += new System.Windows.Forms.LinkLabelLinkClickedEventHandler(this.clearLogLinkLabel_LinkClicked);
            // 
            // runReadinessCheckTabPage
            // 
            this.runReadinessCheckTabPage.Controls.Add(this.checkReportDataGridView);
            this.runReadinessCheckTabPage.Location = new System.Drawing.Point(4, 22);
            this.runReadinessCheckTabPage.Name = "runReadinessCheckTabPage";
            this.runReadinessCheckTabPage.Padding = new System.Windows.Forms.Padding(3);
            this.runReadinessCheckTabPage.Size = new System.Drawing.Size(634, 112);
            this.runReadinessCheckTabPage.TabIndex = 1;
            this.runReadinessCheckTabPage.Text = "Run readiness ";
            this.runReadinessCheckTabPage.UseVisualStyleBackColor = true;
            // 
            // checkReportDataGridView
            // 
            this.checkReportDataGridView.AllowUserToAddRows = false;
            this.checkReportDataGridView.AllowUserToResizeColumns = false;
            this.checkReportDataGridView.BackgroundColor = System.Drawing.Color.White;
            dataGridViewCellStyle2.Alignment = System.Windows.Forms.DataGridViewContentAlignment.MiddleLeft;
            dataGridViewCellStyle2.BackColor = System.Drawing.SystemColors.Control;
            dataGridViewCellStyle2.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            dataGridViewCellStyle2.ForeColor = System.Drawing.SystemColors.WindowText;
            dataGridViewCellStyle2.SelectionBackColor = System.Drawing.SystemColors.Highlight;
            dataGridViewCellStyle2.SelectionForeColor = System.Drawing.SystemColors.HighlightText;
            dataGridViewCellStyle2.WrapMode = System.Windows.Forms.DataGridViewTriState.True;
            this.checkReportDataGridView.ColumnHeadersDefaultCellStyle = dataGridViewCellStyle2;
            this.checkReportDataGridView.ColumnHeadersHeightSizeMode = System.Windows.Forms.DataGridViewColumnHeadersHeightSizeMode.AutoSize;
            this.checkReportDataGridView.Columns.AddRange(new System.Windows.Forms.DataGridViewColumn[] {
            this.host,
            this.check,
            this.result,
            this.action});
            dataGridViewCellStyle3.Alignment = System.Windows.Forms.DataGridViewContentAlignment.MiddleLeft;
            dataGridViewCellStyle3.BackColor = System.Drawing.SystemColors.Window;
            dataGridViewCellStyle3.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            dataGridViewCellStyle3.ForeColor = System.Drawing.SystemColors.ControlText;
            dataGridViewCellStyle3.SelectionBackColor = System.Drawing.SystemColors.Highlight;
            dataGridViewCellStyle3.SelectionForeColor = System.Drawing.SystemColors.HighlightText;
            dataGridViewCellStyle3.WrapMode = System.Windows.Forms.DataGridViewTriState.False;
            this.checkReportDataGridView.DefaultCellStyle = dataGridViewCellStyle3;
            this.checkReportDataGridView.Location = new System.Drawing.Point(6, 3);
            this.checkReportDataGridView.Name = "checkReportDataGridView";
            dataGridViewCellStyle4.Alignment = System.Windows.Forms.DataGridViewContentAlignment.MiddleLeft;
            dataGridViewCellStyle4.BackColor = System.Drawing.SystemColors.Control;
            dataGridViewCellStyle4.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            dataGridViewCellStyle4.ForeColor = System.Drawing.SystemColors.WindowText;
            dataGridViewCellStyle4.SelectionBackColor = System.Drawing.SystemColors.Highlight;
            dataGridViewCellStyle4.SelectionForeColor = System.Drawing.SystemColors.HighlightText;
            dataGridViewCellStyle4.WrapMode = System.Windows.Forms.DataGridViewTriState.True;
            this.checkReportDataGridView.RowHeadersDefaultCellStyle = dataGridViewCellStyle4;
            this.checkReportDataGridView.RowHeadersVisible = false;
            this.checkReportDataGridView.Size = new System.Drawing.Size(618, 103);
            this.checkReportDataGridView.TabIndex = 133;
            this.checkReportDataGridView.Visible = false;
            // 
            // host
            // 
            this.host.HeaderText = "Server Name";
            this.host.Name = "host";
            this.host.ReadOnly = true;
            // 
            // check
            // 
            this.check.HeaderText = "Check";
            this.check.Name = "check";
            this.check.ReadOnly = true;
            // 
            // result
            // 
            this.result.HeaderText = "Result";
            this.result.Name = "result";
            this.result.ReadOnly = true;
            this.result.Resizable = System.Windows.Forms.DataGridViewTriState.True;
            this.result.SortMode = System.Windows.Forms.DataGridViewColumnSortMode.Automatic;
            this.result.Width = 60;
            // 
            // action
            // 
            this.action.HeaderText = "Corrective Action";
            this.action.Name = "action";
            this.action.ReadOnly = true;
            this.action.Width = 355;
            // 
            // selectAllCheckBoxForRecovery
            // 
            this.selectAllCheckBoxForRecovery.BackColor = System.Drawing.Color.White;
            this.selectAllCheckBoxForRecovery.BackgroundImageLayout = System.Windows.Forms.ImageLayout.None;
            this.selectAllCheckBoxForRecovery.Location = new System.Drawing.Point(354, 86);
            this.selectAllCheckBoxForRecovery.Name = "selectAllCheckBoxForRecovery";
            this.selectAllCheckBoxForRecovery.Size = new System.Drawing.Size(13, 13);
            this.selectAllCheckBoxForRecovery.TabIndex = 64;
            this.selectAllCheckBoxForRecovery.UseVisualStyleBackColor = false;
            this.selectAllCheckBoxForRecovery.Visible = false;
            this.selectAllCheckBoxForRecovery.CheckedChanged += new System.EventHandler(this.selectAllCheckBoxForRecovery_CheckedChanged);
            // 
            // recoverPanel
            // 
            this.recoverPanel.BackColor = System.Drawing.Color.Transparent;
            this.recoverPanel.BackgroundImage = global::com.InMage.Wizard.Properties.Resources.workarea;
            this.recoverPanel.Controls.Add(this.selectAllCheckBoxForRecovery);
            this.recoverPanel.Controls.Add(this.recoveryTabControl);
            this.recoverPanel.Controls.Add(this.preReqsButton);
            this.recoverPanel.Controls.Add(this.osTypeLabel);
            this.recoverPanel.Controls.Add(this.planNameText);
            this.recoverPanel.Controls.Add(this.planNameLabel);
            this.recoverPanel.Controls.Add(this.osTypeComboBox);
            this.recoverPanel.Controls.Add(this.label1);
            this.recoverPanel.Controls.Add(this.tagPanel);
            this.recoverPanel.Controls.Add(this.getDetailsButton);
            this.recoverPanel.Controls.Add(this.targetEsxIPText);
            this.recoverPanel.Controls.Add(this.teargetEsxIPLabel);
            this.recoverPanel.Controls.Add(this.targetEsxPasswordLabel);
            this.recoverPanel.Controls.Add(this.targetEsxPasswordTextBox);
            this.recoverPanel.Controls.Add(this.targetEsxUserNameTextBox);
            this.recoverPanel.Controls.Add(this.targetEsxUserNameLabel);
            this.recoverPanel.Controls.Add(this.recoverDataGridView);
            this.recoverPanel.Location = new System.Drawing.Point(167, 44);
            this.recoverPanel.Name = "recoverPanel";
            this.recoverPanel.Size = new System.Drawing.Size(679, 494);
            this.recoverPanel.TabIndex = 111;
            // 
            // applicationPanel
            // 
            this.applicationPanel.BackgroundImage = global::com.InMage.Wizard.Properties.Resources.leftBg;
            this.applicationPanel.BackgroundImageLayout = System.Windows.Forms.ImageLayout.Stretch;
            this.applicationPanel.Controls.Add(this.nwConfigurationLabelLabel);
            this.applicationPanel.Controls.Add(this.configurationPictureBox);
            this.applicationPanel.Controls.Add(this.helpPanel);
            this.applicationPanel.Controls.Add(this.recoverLabel);
            this.applicationPanel.Controls.Add(this.reviewPictureBox);
            this.applicationPanel.Controls.Add(this.p2vHeadingLabel);
            this.applicationPanel.Controls.Add(this.addCredentialsLabel);
            this.applicationPanel.Controls.Add(this.recoverPictureBox);
            this.applicationPanel.Controls.Add(this.helpPictureBox);
            this.applicationPanel.Location = new System.Drawing.Point(0, 44);
            this.applicationPanel.Name = "applicationPanel";
            this.applicationPanel.Size = new System.Drawing.Size(168, 548);
            this.applicationPanel.TabIndex = 113;
            // 
            // nwConfigurationLabelLabel
            // 
            this.nwConfigurationLabelLabel.AutoSize = true;
            this.nwConfigurationLabelLabel.BackColor = System.Drawing.Color.Transparent;
            this.nwConfigurationLabelLabel.Font = new System.Drawing.Font("Tahoma", 8.25F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.nwConfigurationLabelLabel.ForeColor = System.Drawing.Color.White;
            this.nwConfigurationLabelLabel.Location = new System.Drawing.Point(39, 122);
            this.nwConfigurationLabelLabel.Name = "nwConfigurationLabelLabel";
            this.nwConfigurationLabelLabel.Size = new System.Drawing.Size(103, 13);
            this.nwConfigurationLabelLabel.TabIndex = 93;
            this.nwConfigurationLabelLabel.Text = "VM Configuration";
            // 
            // configurationPictureBox
            // 
            this.configurationPictureBox.BackColor = System.Drawing.Color.Transparent;
            this.configurationPictureBox.Location = new System.Drawing.Point(3, 115);
            this.configurationPictureBox.Name = "configurationPictureBox";
            this.configurationPictureBox.Size = new System.Drawing.Size(38, 25);
            this.configurationPictureBox.SizeMode = System.Windows.Forms.PictureBoxSizeMode.CenterImage;
            this.configurationPictureBox.TabIndex = 92;
            this.configurationPictureBox.TabStop = false;
            // 
            // helpPanel
            // 
            this.helpPanel.BackColor = System.Drawing.Color.Transparent;
            this.helpPanel.BackgroundImage = global::com.InMage.Wizard.Properties.Resources.helpOpen2;
            this.helpPanel.BackgroundImageLayout = System.Windows.Forms.ImageLayout.None;
            this.helpPanel.Controls.Add(this.helpContentLabel);
            this.helpPanel.Controls.Add(this.esxProtectionLinkLabel);
            this.helpPanel.Controls.Add(this.closePictureBox);
            this.helpPanel.Location = new System.Drawing.Point(2, 249);
            this.helpPanel.Name = "helpPanel";
            this.helpPanel.Size = new System.Drawing.Size(163, 298);
            this.helpPanel.TabIndex = 63;
            this.helpPanel.Visible = false;
            // 
            // helpContentLabel
            // 
            this.helpContentLabel.ForeColor = System.Drawing.Color.Black;
            this.helpContentLabel.Location = new System.Drawing.Point(7, 46);
            this.helpContentLabel.Name = "helpContentLabel";
            this.helpContentLabel.Size = new System.Drawing.Size(148, 217);
            this.helpContentLabel.TabIndex = 5;
            this.helpContentLabel.Text = "label1";
            // 
            // esxProtectionLinkLabel
            // 
            this.esxProtectionLinkLabel.AutoSize = true;
            this.esxProtectionLinkLabel.Cursor = System.Windows.Forms.Cursors.Hand;
            this.esxProtectionLinkLabel.LinkColor = System.Drawing.Color.FromArgb(((int)(((byte)(210)))), ((int)(((byte)(0)))), ((int)(((byte)(0)))));
            this.esxProtectionLinkLabel.Location = new System.Drawing.Point(6, 276);
            this.esxProtectionLinkLabel.Name = "esxProtectionLinkLabel";
            this.esxProtectionLinkLabel.Size = new System.Drawing.Size(134, 13);
            this.esxProtectionLinkLabel.TabIndex = 3;
            this.esxProtectionLinkLabel.TabStop = true;
            this.esxProtectionLinkLabel.Text = "More about ESX Recovery";
            this.esxProtectionLinkLabel.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
            this.esxProtectionLinkLabel.LinkClicked += new System.Windows.Forms.LinkLabelLinkClickedEventHandler(this.esxProtectionLinkLabel_LinkClicked);
            // 
            // closePictureBox
            // 
            this.closePictureBox.BackgroundImage = global::com.InMage.Wizard.Properties.Resources.closeBtn;
            this.closePictureBox.BackgroundImageLayout = System.Windows.Forms.ImageLayout.None;
            this.closePictureBox.Location = new System.Drawing.Point(141, 11);
            this.closePictureBox.Name = "closePictureBox";
            this.closePictureBox.Size = new System.Drawing.Size(14, 14);
            this.closePictureBox.TabIndex = 0;
            this.closePictureBox.TabStop = false;
            this.closePictureBox.Click += new System.EventHandler(this.closePictureBox_Click);
            // 
            // recoverLabel
            // 
            this.recoverLabel.AutoSize = true;
            this.recoverLabel.BackColor = System.Drawing.Color.Transparent;
            this.recoverLabel.Font = new System.Drawing.Font("Tahoma", 8.25F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.recoverLabel.ForeColor = System.Drawing.Color.White;
            this.recoverLabel.Location = new System.Drawing.Point(39, 158);
            this.recoverLabel.Name = "recoverLabel";
            this.recoverLabel.Size = new System.Drawing.Size(54, 13);
            this.recoverLabel.TabIndex = 91;
            this.recoverLabel.Text = "Recover";
            this.recoverLabel.Click += new System.EventHandler(this.recoverLabel_Click);
            // 
            // reviewPictureBox
            // 
            this.reviewPictureBox.BackColor = System.Drawing.Color.Transparent;
            this.reviewPictureBox.Location = new System.Drawing.Point(3, 153);
            this.reviewPictureBox.Name = "reviewPictureBox";
            this.reviewPictureBox.Size = new System.Drawing.Size(38, 25);
            this.reviewPictureBox.SizeMode = System.Windows.Forms.PictureBoxSizeMode.CenterImage;
            this.reviewPictureBox.TabIndex = 90;
            this.reviewPictureBox.TabStop = false;
            // 
            // p2vHeadingLabel
            // 
            this.p2vHeadingLabel.AutoSize = true;
            this.p2vHeadingLabel.BackColor = System.Drawing.Color.Transparent;
            this.p2vHeadingLabel.Font = new System.Drawing.Font("Tahoma", 12F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.p2vHeadingLabel.ForeColor = System.Drawing.Color.White;
            this.p2vHeadingLabel.Location = new System.Drawing.Point(9, 22);
            this.p2vHeadingLabel.Name = "p2vHeadingLabel";
            this.p2vHeadingLabel.Size = new System.Drawing.Size(84, 19);
            this.p2vHeadingLabel.TabIndex = 88;
            this.p2vHeadingLabel.Text = "Recovery";
            // 
            // addCredentialsLabel
            // 
            this.addCredentialsLabel.AutoSize = true;
            this.addCredentialsLabel.BackColor = System.Drawing.Color.Transparent;
            this.addCredentialsLabel.Font = new System.Drawing.Font("Tahoma", 8.25F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.addCredentialsLabel.ForeColor = System.Drawing.Color.White;
            this.addCredentialsLabel.Location = new System.Drawing.Point(39, 84);
            this.addCredentialsLabel.Name = "addCredentialsLabel";
            this.addCredentialsLabel.Size = new System.Drawing.Size(98, 13);
            this.addCredentialsLabel.TabIndex = 89;
            this.addCredentialsLabel.Text = "Select Machines";
            this.addCredentialsLabel.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
            // 
            // recoverPictureBox
            // 
            this.recoverPictureBox.BackColor = System.Drawing.Color.Transparent;
            this.recoverPictureBox.Image = global::com.InMage.Wizard.Properties.Resources.arrow1;
            this.recoverPictureBox.Location = new System.Drawing.Point(3, 78);
            this.recoverPictureBox.Name = "recoverPictureBox";
            this.recoverPictureBox.Size = new System.Drawing.Size(38, 24);
            this.recoverPictureBox.SizeMode = System.Windows.Forms.PictureBoxSizeMode.CenterImage;
            this.recoverPictureBox.TabIndex = 88;
            this.recoverPictureBox.TabStop = false;
            // 
            // helpPictureBox
            // 
            this.helpPictureBox.BackgroundImage = global::com.InMage.Wizard.Properties.Resources.helpClose1;
            this.helpPictureBox.BackgroundImageLayout = System.Windows.Forms.ImageLayout.None;
            this.helpPictureBox.Location = new System.Drawing.Point(0, 516);
            this.helpPictureBox.Name = "helpPictureBox";
            this.helpPictureBox.Size = new System.Drawing.Size(168, 32);
            this.helpPictureBox.TabIndex = 1;
            this.helpPictureBox.TabStop = false;
            this.helpPictureBox.Click += new System.EventHandler(this.helpPictureBox_Click_1);
            // 
            // configurationScreen
            // 
            this.configurationScreen.BackColor = System.Drawing.Color.Transparent;
            this.configurationScreen.BackgroundImage = global::com.InMage.Wizard.Properties.Resources.workarea;
            this.configurationScreen.BackgroundImageLayout = System.Windows.Forms.ImageLayout.None;
            this.configurationScreen.Location = new System.Drawing.Point(2, 3);
            this.configurationScreen.Name = "configurationScreen";
            this.configurationScreen.Size = new System.Drawing.Size(651, 483);
            this.configurationScreen.TabIndex = 0;
            // 
            // RecoveryForm
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.BackColor = System.Drawing.Color.White;
            this.ClientSize = new System.Drawing.Size(850, 588);
            this.Controls.Add(this.headingPanel);
            this.Controls.Add(this.panel1);
            this.Controls.Add(this.applicationPanel);
            this.Controls.Add(this.cancelButton);
            this.Controls.Add(this.previousButton);
            this.Controls.Add(this.nextButton);
            this.Controls.Add(this.doneButton);
            this.Controls.Add(this.reviewPanel);
            this.Controls.Add(this.recoverPanel);
            this.Controls.Add(this.configurationPanel);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedSingle;
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.MaximizeBox = false;
            this.Name = "RecoveryForm";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
            this.Text = "Recovery";
            this.FormClosing += new System.Windows.Forms.FormClosingEventHandler(this.RecoveryForm_FormClosing);
            ((System.ComponentModel.ISupportInitialize)(this.errorProvider)).EndInit();
            this.headingPanel.ResumeLayout(false);
            this.headingPanel.PerformLayout();
            this.panel1.ResumeLayout(false);
            this.reviewPanel.ResumeLayout(false);
            this.reviewPanel.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this.recoveryReviewDataGridView)).EndInit();
            this.configurationPanel.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.recoverDataGridView)).EndInit();
            this.tagPanel.ResumeLayout(false);
            this.tagPanel.PerformLayout();
            this.recoveryTabControl.ResumeLayout(false);
            this.logTabPage.ResumeLayout(false);
            this.logTabPage.PerformLayout();
            this.runReadinessCheckTabPage.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.checkReportDataGridView)).EndInit();
            this.recoverPanel.ResumeLayout(false);
            this.recoverPanel.PerformLayout();
            this.applicationPanel.ResumeLayout(false);
            this.applicationPanel.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this.configurationPictureBox)).EndInit();
            this.helpPanel.ResumeLayout(false);
            this.helpPanel.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this.closePictureBox)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.reviewPictureBox)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.recoverPictureBox)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.helpPictureBox)).EndInit();
            this.ResumeLayout(false);

        }

        #endregion

        public System.Windows.Forms.Button nextButton;
        public System.Windows.Forms.Button cancelButton;
        private BufferedPanel applicationPanel;
        private System.Windows.Forms.Label p2vHeadingLabel;
        private System.Windows.Forms.Label addCredentialsLabel;
        private System.Windows.Forms.PictureBox recoverPictureBox;
        public System.Windows.Forms.ErrorProvider errorProvider;
        private System.Windows.Forms.Panel reviewPanel;
        private System.Windows.Forms.PictureBox reviewPictureBox;
        public System.Windows.Forms.Button doneButton;
        public System.Windows.Forms.DataGridView recoveryReviewDataGridView;
        private System.Windows.Forms.Label recoverLabel;
        public System.Windows.Forms.Button previousButton;
        private System.Windows.Forms.Label selectedMachineListLabel;
        public System.Windows.Forms.TextBox generalLogTextBox;
        public System.ComponentModel.BackgroundWorker recoveryDetailsBackgroundWorker;
        public System.Windows.Forms.ProgressBar progressBar;
        public System.ComponentModel.BackgroundWorker recoveryBackgroundWorker;
        private System.Windows.Forms.Panel panel1;
        public System.ComponentModel.BackgroundWorker credentialCheckBackgroundWorker;
        private System.Windows.Forms.DataGridViewTextBoxColumn displayName;
        private System.Windows.Forms.DataGridViewTextBoxColumn IP;
        private System.Windows.Forms.DataGridViewTextBoxColumn SubnetMask;
        private System.Windows.Forms.DataGridViewTextBoxColumn DefaultGateWay;
        private System.Windows.Forms.DataGridViewTextBoxColumn dns;
        private System.Windows.Forms.DataGridViewTextBoxColumn tag;
        private System.Windows.Forms.DataGridViewTextBoxColumn tagType;
        private System.Windows.Forms.DataGridViewTextBoxColumn recoverOrder;
        public System.Windows.Forms.Panel helpPanel;
        private System.Windows.Forms.LinkLabel esxProtectionLinkLabel;
        private System.Windows.Forms.PictureBox closePictureBox;
        private System.Windows.Forms.Panel headingPanel;
        private System.Windows.Forms.PictureBox helpPictureBox;
        public System.Windows.Forms.Label helpContentLabel;
        private System.Windows.Forms.Label versionLabel;
        public System.Windows.Forms.LinkLabel monitiorLinkLabel;
        public System.Windows.Forms.Panel configurationPanel;
        public ConfigurationScreen configurationScreen;
        private System.Windows.Forms.Label nwConfigurationLabelLabel;
        private System.Windows.Forms.PictureBox configurationPictureBox;
        public System.Windows.Forms.Panel recoverPanel;
        public System.Windows.Forms.CheckBox selectAllCheckBoxForRecovery;
        public System.Windows.Forms.TabControl recoveryTabControl;
        public System.Windows.Forms.TabPage logTabPage;
        public System.Windows.Forms.TextBox recoveryText;
        public System.Windows.Forms.LinkLabel clearLogLinkLabel;
        public System.Windows.Forms.TabPage runReadinessCheckTabPage;
        public System.Windows.Forms.DataGridView checkReportDataGridView;
        private System.Windows.Forms.DataGridViewTextBoxColumn host;
        private System.Windows.Forms.DataGridViewTextBoxColumn check;
        private System.Windows.Forms.DataGridViewImageColumn result;
        private System.Windows.Forms.DataGridViewTextBoxColumn action;
        public System.Windows.Forms.Button preReqsButton;
        public System.Windows.Forms.Label osTypeLabel;
        public System.Windows.Forms.TextBox planNameText;
        public System.Windows.Forms.Label planNameLabel;
        public System.Windows.Forms.ComboBox osTypeComboBox;
        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.Panel tagPanel;
        private System.Windows.Forms.DateTimePicker specificTimeDateTimePicker;
        private System.Windows.Forms.RadioButton specificTimeRadioButton;
        private System.Windows.Forms.DateTimePicker dateTimePicker;
        private System.Windows.Forms.RadioButton userDefinedTimeRadioButton;
        private System.Windows.Forms.RadioButton timeBasedRadioButton;
        private System.Windows.Forms.RadioButton tagBasedRadioButton;
        private System.Windows.Forms.Label recoveryTagLabels;
        public System.Windows.Forms.Button getDetailsButton;
        public System.Windows.Forms.TextBox targetEsxIPText;
        private System.Windows.Forms.Label teargetEsxIPLabel;
        private System.Windows.Forms.Label targetEsxPasswordLabel;
        private System.Windows.Forms.TextBox targetEsxPasswordTextBox;
        private System.Windows.Forms.TextBox targetEsxUserNameTextBox;
        private System.Windows.Forms.Label targetEsxUserNameLabel;
        public System.Windows.Forms.DataGridView recoverDataGridView;
        private System.Windows.Forms.DataGridViewTextBoxColumn hostName;
        private System.Windows.Forms.DataGridViewTextBoxColumn os;
        private System.Windows.Forms.DataGridViewTextBoxColumn masterTarget;
        private System.Windows.Forms.DataGridViewCheckBoxColumn recover;
    }
}