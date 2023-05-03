namespace com.InMage.Wizard
{
    partial class RemovePairsForm
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
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(RemovePairsForm));
            this.headingPanel = new System.Windows.Forms.Panel();
            this.versionLabel = new System.Windows.Forms.Label();
            this.recoverPanel = new System.Windows.Forms.Panel();
            this.clearLogsLinkLabel = new System.Windows.Forms.LinkLabel();
            this.logTextBox = new System.Windows.Forms.TextBox();
            this.label1 = new System.Windows.Forms.Label();
            this.getDetailsButton = new System.Windows.Forms.Button();
            this.targetEsxIPText = new System.Windows.Forms.TextBox();
            this.teargetEsxIPLabel = new System.Windows.Forms.Label();
            this.targetEsxPasswordLabel = new System.Windows.Forms.Label();
            this.targetEsxPasswordTextBox = new System.Windows.Forms.TextBox();
            this.targetEsxUserNameTextBox = new System.Windows.Forms.TextBox();
            this.targetEsxUserNameLabel = new System.Windows.Forms.Label();
            this.removeDataGridView = new System.Windows.Forms.DataGridView();
            this.progressBar = new System.Windows.Forms.ProgressBar();
            this.nextButton = new System.Windows.Forms.Button();
            this.doneButton = new System.Windows.Forms.Button();
            this.cancelButton = new System.Windows.Forms.Button();
            this.removeDetailsBackgroundWorker = new System.ComponentModel.BackgroundWorker();
            this.navigationPanel = new System.Windows.Forms.Panel();
            this.detachBackgroundWorker = new System.ComponentModel.BackgroundWorker();
            this.applicationPanel = new com.InMage.Wizard.BufferedPanel();
            this.p2vHeadingLabel = new System.Windows.Forms.Label();
            this.addCredentialsLabel = new System.Windows.Forms.Label();
            this.recoverPictureBox = new System.Windows.Forms.PictureBox();
            this.helpPanel = new System.Windows.Forms.Panel();
            this.helpContentLabel = new System.Windows.Forms.Label();
            this.esxProtectionLinkLabel = new System.Windows.Forms.LinkLabel();
            this.closePictureBox = new System.Windows.Forms.PictureBox();
            this.helpPictureBox = new System.Windows.Forms.PictureBox();
            this.planName = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.hostName = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.displayName = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.masterTarget = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.remove = new System.Windows.Forms.DataGridViewCheckBoxColumn();
            this.selectAllCheckBox = new System.Windows.Forms.CheckBox();
            this.headingPanel.SuspendLayout();
            this.recoverPanel.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.removeDataGridView)).BeginInit();
            this.navigationPanel.SuspendLayout();
            this.applicationPanel.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.recoverPictureBox)).BeginInit();
            this.helpPanel.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.closePictureBox)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.helpPictureBox)).BeginInit();
            this.SuspendLayout();
            // 
            // headingPanel
            // 
            this.headingPanel.BackColor = System.Drawing.Color.Transparent;
            this.headingPanel.BackgroundImage = global::com.InMage.Wizard.Properties.Resources.heading;
            this.headingPanel.Controls.Add(this.versionLabel);
            this.headingPanel.Location = new System.Drawing.Point(0, 0);
            this.headingPanel.Name = "headingPanel";
            this.headingPanel.Size = new System.Drawing.Size(850, 44);
            this.headingPanel.TabIndex = 115;
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
            // recoverPanel
            // 
            this.recoverPanel.BackColor = System.Drawing.Color.Transparent;
            this.recoverPanel.BackgroundImage = global::com.InMage.Wizard.Properties.Resources.workarea;
            this.recoverPanel.Controls.Add(this.selectAllCheckBox);
            this.recoverPanel.Controls.Add(this.clearLogsLinkLabel);
            this.recoverPanel.Controls.Add(this.logTextBox);
            this.recoverPanel.Controls.Add(this.label1);
            this.recoverPanel.Controls.Add(this.getDetailsButton);
            this.recoverPanel.Controls.Add(this.targetEsxIPText);
            this.recoverPanel.Controls.Add(this.teargetEsxIPLabel);
            this.recoverPanel.Controls.Add(this.targetEsxPasswordLabel);
            this.recoverPanel.Controls.Add(this.targetEsxPasswordTextBox);
            this.recoverPanel.Controls.Add(this.targetEsxUserNameTextBox);
            this.recoverPanel.Controls.Add(this.targetEsxUserNameLabel);
            this.recoverPanel.Controls.Add(this.removeDataGridView);
            this.recoverPanel.Location = new System.Drawing.Point(168, 44);
            this.recoverPanel.Name = "recoverPanel";
            this.recoverPanel.Size = new System.Drawing.Size(679, 494);
            this.recoverPanel.TabIndex = 117;
            // 
            // clearLogsLinkLabel
            // 
            this.clearLogsLinkLabel.AutoSize = true;
            this.clearLogsLinkLabel.Location = new System.Drawing.Point(605, 334);
            this.clearLogsLinkLabel.Name = "clearLogsLinkLabel";
            this.clearLogsLinkLabel.Size = new System.Drawing.Size(48, 13);
            this.clearLogsLinkLabel.TabIndex = 24;
            this.clearLogsLinkLabel.TabStop = true;
            this.clearLogsLinkLabel.Text = "Clear log";
            this.clearLogsLinkLabel.LinkClicked += new System.Windows.Forms.LinkLabelLinkClickedEventHandler(this.clearLogsLinkLabel_LinkClicked);
            // 
            // logTextBox
            // 
            this.logTextBox.BackColor = System.Drawing.Color.SeaShell;
            this.logTextBox.Location = new System.Drawing.Point(5, 356);
            this.logTextBox.Multiline = true;
            this.logTextBox.Name = "logTextBox";
            this.logTextBox.ReadOnly = true;
            this.logTextBox.ScrollBars = System.Windows.Forms.ScrollBars.Both;
            this.logTextBox.Size = new System.Drawing.Size(649, 130);
            this.logTextBox.TabIndex = 23;
            // 
            // label1
            // 
            this.label1.AutoSize = true;
            this.label1.Font = new System.Drawing.Font("Arial", 8.25F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.label1.Location = new System.Drawing.Point(9, 10);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(226, 14);
            this.label1.TabIndex = 22;
            this.label1.Text = "Provide secondary vSphere information";
            // 
            // getDetailsButton
            // 
            this.getDetailsButton.Location = new System.Drawing.Point(360, 47);
            this.getDetailsButton.Name = "getDetailsButton";
            this.getDetailsButton.Size = new System.Drawing.Size(75, 23);
            this.getDetailsButton.TabIndex = 5;
            this.getDetailsButton.Text = "Get Details";
            this.getDetailsButton.UseVisualStyleBackColor = true;
            this.getDetailsButton.Click += new System.EventHandler(this.getDetailsButton_Click);
            // 
            // targetEsxIPText
            // 
            this.targetEsxIPText.Location = new System.Drawing.Point(13, 50);
            this.targetEsxIPText.Name = "targetEsxIPText";
            this.targetEsxIPText.Size = new System.Drawing.Size(100, 20);
            this.targetEsxIPText.TabIndex = 1;
            // 
            // teargetEsxIPLabel
            // 
            this.teargetEsxIPLabel.AutoSize = true;
            this.teargetEsxIPLabel.Font = new System.Drawing.Font("Arial", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.teargetEsxIPLabel.Location = new System.Drawing.Point(10, 33);
            this.teargetEsxIPLabel.Name = "teargetEsxIPLabel";
            this.teargetEsxIPLabel.Size = new System.Drawing.Size(94, 14);
            this.teargetEsxIPLabel.TabIndex = 16;
            this.teargetEsxIPLabel.Text = "Secondary ESX IP";
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
            // targetEsxPasswordTextBox
            // 
            this.targetEsxPasswordTextBox.Location = new System.Drawing.Point(246, 50);
            this.targetEsxPasswordTextBox.Name = "targetEsxPasswordTextBox";
            this.targetEsxPasswordTextBox.PasswordChar = '*';
            this.targetEsxPasswordTextBox.Size = new System.Drawing.Size(88, 20);
            this.targetEsxPasswordTextBox.TabIndex = 3;
            // 
            // targetEsxUserNameTextBox
            // 
            this.targetEsxUserNameTextBox.Location = new System.Drawing.Point(132, 50);
            this.targetEsxUserNameTextBox.Name = "targetEsxUserNameTextBox";
            this.targetEsxUserNameTextBox.Size = new System.Drawing.Size(100, 20);
            this.targetEsxUserNameTextBox.TabIndex = 2;
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
            // removeDataGridView
            // 
            this.removeDataGridView.AllowUserToAddRows = false;
            this.removeDataGridView.BackgroundColor = System.Drawing.Color.White;
            this.removeDataGridView.BorderStyle = System.Windows.Forms.BorderStyle.None;
            dataGridViewCellStyle1.Alignment = System.Windows.Forms.DataGridViewContentAlignment.MiddleLeft;
            dataGridViewCellStyle1.BackColor = System.Drawing.SystemColors.Control;
            dataGridViewCellStyle1.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            dataGridViewCellStyle1.ForeColor = System.Drawing.SystemColors.WindowText;
            dataGridViewCellStyle1.SelectionBackColor = System.Drawing.SystemColors.Highlight;
            dataGridViewCellStyle1.SelectionForeColor = System.Drawing.SystemColors.HighlightText;
            dataGridViewCellStyle1.WrapMode = System.Windows.Forms.DataGridViewTriState.True;
            this.removeDataGridView.ColumnHeadersDefaultCellStyle = dataGridViewCellStyle1;
            this.removeDataGridView.ColumnHeadersHeightSizeMode = System.Windows.Forms.DataGridViewColumnHeadersHeightSizeMode.AutoSize;
            this.removeDataGridView.Columns.AddRange(new System.Windows.Forms.DataGridViewColumn[] {
            this.planName,
            this.hostName,
            this.displayName,
            this.masterTarget,
            this.remove});
            dataGridViewCellStyle2.Alignment = System.Windows.Forms.DataGridViewContentAlignment.MiddleLeft;
            dataGridViewCellStyle2.BackColor = System.Drawing.SystemColors.Window;
            dataGridViewCellStyle2.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            dataGridViewCellStyle2.ForeColor = System.Drawing.SystemColors.ControlText;
            dataGridViewCellStyle2.SelectionBackColor = System.Drawing.SystemColors.Highlight;
            dataGridViewCellStyle2.SelectionForeColor = System.Drawing.SystemColors.HighlightText;
            dataGridViewCellStyle2.WrapMode = System.Windows.Forms.DataGridViewTriState.False;
            this.removeDataGridView.DefaultCellStyle = dataGridViewCellStyle2;
            this.removeDataGridView.Location = new System.Drawing.Point(5, 122);
            this.removeDataGridView.Name = "removeDataGridView";
            dataGridViewCellStyle3.Alignment = System.Windows.Forms.DataGridViewContentAlignment.MiddleLeft;
            dataGridViewCellStyle3.BackColor = System.Drawing.SystemColors.Control;
            dataGridViewCellStyle3.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            dataGridViewCellStyle3.ForeColor = System.Drawing.SystemColors.WindowText;
            dataGridViewCellStyle3.SelectionBackColor = System.Drawing.SystemColors.Highlight;
            dataGridViewCellStyle3.SelectionForeColor = System.Drawing.SystemColors.HighlightText;
            dataGridViewCellStyle3.WrapMode = System.Windows.Forms.DataGridViewTriState.True;
            this.removeDataGridView.RowHeadersDefaultCellStyle = dataGridViewCellStyle3;
            this.removeDataGridView.RowHeadersVisible = false;
            this.removeDataGridView.Size = new System.Drawing.Size(474, 190);
            this.removeDataGridView.TabIndex = 5;
            this.removeDataGridView.Visible = false;
            this.removeDataGridView.CellValueChanged += new System.Windows.Forms.DataGridViewCellEventHandler(this.removeDataGridView_CellValueChanged);
            this.removeDataGridView.CurrentCellDirtyStateChanged += new System.EventHandler(this.removeDataGridView_CurrentCellDirtyStateChanged);
            // 
            // progressBar
            // 
            this.progressBar.BackColor = System.Drawing.Color.White;
            this.progressBar.Location = new System.Drawing.Point(0, 0);
            this.progressBar.Name = "progressBar";
            this.progressBar.Size = new System.Drawing.Size(683, 10);
            this.progressBar.Style = System.Windows.Forms.ProgressBarStyle.Marquee;
            this.progressBar.TabIndex = 124;
            this.progressBar.Visible = false;
            // 
            // nextButton
            // 
            this.nextButton.Enabled = false;
            this.nextButton.Location = new System.Drawing.Point(666, 559);
            this.nextButton.Name = "nextButton";
            this.nextButton.Size = new System.Drawing.Size(75, 23);
            this.nextButton.TabIndex = 121;
            this.nextButton.Text = "Next >";
            this.nextButton.UseVisualStyleBackColor = true;
            this.nextButton.Click += new System.EventHandler(this.nextButton_Click);
            // 
            // doneButton
            // 
            this.doneButton.Location = new System.Drawing.Point(760, 559);
            this.doneButton.Name = "doneButton";
            this.doneButton.Size = new System.Drawing.Size(75, 23);
            this.doneButton.TabIndex = 122;
            this.doneButton.Text = "Done";
            this.doneButton.UseVisualStyleBackColor = true;
            this.doneButton.Visible = false;
            this.doneButton.Click += new System.EventHandler(this.doneButton_Click);
            // 
            // cancelButton
            // 
            this.cancelButton.Location = new System.Drawing.Point(760, 559);
            this.cancelButton.Name = "cancelButton";
            this.cancelButton.Size = new System.Drawing.Size(75, 23);
            this.cancelButton.TabIndex = 125;
            this.cancelButton.Text = "Cancel";
            this.cancelButton.UseVisualStyleBackColor = true;
            this.cancelButton.Click += new System.EventHandler(this.cancelButton_Click);
            // 
            // removeDetailsBackgroundWorker
            // 
            this.removeDetailsBackgroundWorker.DoWork += new System.ComponentModel.DoWorkEventHandler(this.removeDetailsBackgroundWorker_DoWork);
            this.removeDetailsBackgroundWorker.RunWorkerCompleted += new System.ComponentModel.RunWorkerCompletedEventHandler(this.removeDetailsBackgroundWorker_RunWorkerCompleted);
            this.removeDetailsBackgroundWorker.ProgressChanged += new System.ComponentModel.ProgressChangedEventHandler(this.removeDetailsBackgroundWorker_ProgressChanged);
            // 
            // navigationPanel
            // 
            this.navigationPanel.BackColor = System.Drawing.Color.Transparent;
            this.navigationPanel.BackgroundImage = global::com.InMage.Wizard.Properties.Resources.navBar;
            this.navigationPanel.Controls.Add(this.progressBar);
            this.navigationPanel.Location = new System.Drawing.Point(166, 544);
            this.navigationPanel.Name = "navigationPanel";
            this.navigationPanel.Size = new System.Drawing.Size(686, 10);
            this.navigationPanel.TabIndex = 23;
            // 
            // detachBackgroundWorker
            // 
            this.detachBackgroundWorker.DoWork += new System.ComponentModel.DoWorkEventHandler(this.detachBackgroundWorker_DoWork);
            this.detachBackgroundWorker.RunWorkerCompleted += new System.ComponentModel.RunWorkerCompletedEventHandler(this.detachBackgroundWorker_RunWorkerCompleted);
            this.detachBackgroundWorker.ProgressChanged += new System.ComponentModel.ProgressChangedEventHandler(this.detachBackgroundWorker_ProgressChanged);
            // 
            // applicationPanel
            // 
            this.applicationPanel.BackgroundImage = global::com.InMage.Wizard.Properties.Resources.leftBg;
            this.applicationPanel.BackgroundImageLayout = System.Windows.Forms.ImageLayout.Stretch;
            this.applicationPanel.Controls.Add(this.p2vHeadingLabel);
            this.applicationPanel.Controls.Add(this.addCredentialsLabel);
            this.applicationPanel.Controls.Add(this.recoverPictureBox);
            this.applicationPanel.Controls.Add(this.helpPanel);
            this.applicationPanel.Controls.Add(this.helpPictureBox);
            this.applicationPanel.Location = new System.Drawing.Point(0, 44);
            this.applicationPanel.Name = "applicationPanel";
            this.applicationPanel.Size = new System.Drawing.Size(168, 544);
            this.applicationPanel.TabIndex = 114;
            // 
            // p2vHeadingLabel
            // 
            this.p2vHeadingLabel.AutoSize = true;
            this.p2vHeadingLabel.BackColor = System.Drawing.Color.Transparent;
            this.p2vHeadingLabel.Font = new System.Drawing.Font("Arial", 12F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.p2vHeadingLabel.ForeColor = System.Drawing.Color.White;
            this.p2vHeadingLabel.Location = new System.Drawing.Point(7, 21);
            this.p2vHeadingLabel.Name = "p2vHeadingLabel";
            this.p2vHeadingLabel.Size = new System.Drawing.Size(155, 19);
            this.p2vHeadingLabel.TabIndex = 88;
            this.p2vHeadingLabel.Text = "Remove Protection";
            // 
            // addCredentialsLabel
            // 
            this.addCredentialsLabel.AutoSize = true;
            this.addCredentialsLabel.BackColor = System.Drawing.Color.Transparent;
            this.addCredentialsLabel.Font = new System.Drawing.Font("Tahoma", 8.25F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.addCredentialsLabel.ForeColor = System.Drawing.Color.White;
            this.addCredentialsLabel.Location = new System.Drawing.Point(31, 84);
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
            // helpPanel
            // 
            this.helpPanel.BackColor = System.Drawing.Color.Transparent;
            this.helpPanel.BackgroundImage = global::com.InMage.Wizard.Properties.Resources.helpOpen2;
            this.helpPanel.Controls.Add(this.helpContentLabel);
            this.helpPanel.Controls.Add(this.esxProtectionLinkLabel);
            this.helpPanel.Controls.Add(this.closePictureBox);
            this.helpPanel.Location = new System.Drawing.Point(0, 248);
            this.helpPanel.Name = "helpPanel";
            this.helpPanel.Size = new System.Drawing.Size(163, 296);
            this.helpPanel.TabIndex = 64;
            this.helpPanel.Visible = false;
            // 
            // helpContentLabel
            // 
            this.helpContentLabel.ForeColor = System.Drawing.Color.Black;
            this.helpContentLabel.Location = new System.Drawing.Point(7, 45);
            this.helpContentLabel.Name = "helpContentLabel";
            this.helpContentLabel.Size = new System.Drawing.Size(148, 206);
            this.helpContentLabel.TabIndex = 5;
            this.helpContentLabel.Text = "label1";
            // 
            // esxProtectionLinkLabel
            // 
            this.esxProtectionLinkLabel.AutoSize = true;
            this.esxProtectionLinkLabel.Cursor = System.Windows.Forms.Cursors.Hand;
            this.esxProtectionLinkLabel.LinkColor = System.Drawing.Color.FromArgb(((int)(((byte)(210)))), ((int)(((byte)(0)))), ((int)(((byte)(0)))));
            this.esxProtectionLinkLabel.Location = new System.Drawing.Point(5, 275);
            this.esxProtectionLinkLabel.Name = "esxProtectionLinkLabel";
            this.esxProtectionLinkLabel.Size = new System.Drawing.Size(104, 13);
            this.esxProtectionLinkLabel.TabIndex = 3;
            this.esxProtectionLinkLabel.TabStop = true;
            this.esxProtectionLinkLabel.Text = "More about Remove";
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
            // helpPictureBox
            // 
            this.helpPictureBox.BackgroundImage = global::com.InMage.Wizard.Properties.Resources.helpClose1;
            this.helpPictureBox.BackgroundImageLayout = System.Windows.Forms.ImageLayout.None;
            this.helpPictureBox.Location = new System.Drawing.Point(0, 514);
            this.helpPictureBox.Name = "helpPictureBox";
            this.helpPictureBox.Size = new System.Drawing.Size(168, 33);
            this.helpPictureBox.TabIndex = 2;
            this.helpPictureBox.TabStop = false;
            this.helpPictureBox.Click += new System.EventHandler(this.helpPictureBox_Click);
            // 
            // planName
            // 
            this.planName.HeaderText = "Planname";
            this.planName.Name = "planName";
            this.planName.ReadOnly = true;
            this.planName.Width = 95;
            // 
            // hostName
            // 
            this.hostName.AutoSizeMode = System.Windows.Forms.DataGridViewAutoSizeColumnMode.None;
            this.hostName.HeaderText = "Host Name";
            this.hostName.Name = "hostName";
            // 
            // displayName
            // 
            this.displayName.HeaderText = "Display name";
            this.displayName.Name = "displayName";
            this.displayName.ReadOnly = true;
            // 
            // masterTarget
            // 
            this.masterTarget.HeaderText = "Master Target";
            this.masterTarget.Name = "masterTarget";
            // 
            // remove
            // 
            this.remove.AutoSizeMode = System.Windows.Forms.DataGridViewAutoSizeColumnMode.None;
            this.remove.HeaderText = "Remove";
            this.remove.Name = "remove";
            this.remove.Resizable = System.Windows.Forms.DataGridViewTriState.True;
            this.remove.SortMode = System.Windows.Forms.DataGridViewColumnSortMode.Automatic;
            this.remove.Width = 70;
            // 
            // selectAllCheckBox
            // 
            this.selectAllCheckBox.Location = new System.Drawing.Point(452, 126);
            this.selectAllCheckBox.Name = "selectAllCheckBox";
            this.selectAllCheckBox.Size = new System.Drawing.Size(13, 13);
            this.selectAllCheckBox.TabIndex = 25;
            this.selectAllCheckBox.UseVisualStyleBackColor = true;
            this.selectAllCheckBox.Visible = false;
            this.selectAllCheckBox.CheckedChanged += new System.EventHandler(this.selectAllCheckBox_CheckedChanged);
            // 
            // RemovePairsForm
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.BackColor = System.Drawing.Color.White;
            this.ClientSize = new System.Drawing.Size(848, 586);
            this.Controls.Add(this.navigationPanel);
            this.Controls.Add(this.nextButton);
            this.Controls.Add(this.doneButton);
            this.Controls.Add(this.headingPanel);
            this.Controls.Add(this.applicationPanel);
            this.Controls.Add(this.cancelButton);
            this.Controls.Add(this.recoverPanel);
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.Name = "RemovePairsForm";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
            this.Text = "Remove Protection";
            this.headingPanel.ResumeLayout(false);
            this.headingPanel.PerformLayout();
            this.recoverPanel.ResumeLayout(false);
            this.recoverPanel.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this.removeDataGridView)).EndInit();
            this.navigationPanel.ResumeLayout(false);
            this.applicationPanel.ResumeLayout(false);
            this.applicationPanel.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this.recoverPictureBox)).EndInit();
            this.helpPanel.ResumeLayout(false);
            this.helpPanel.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this.closePictureBox)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.helpPictureBox)).EndInit();
            this.ResumeLayout(false);

        }

        #endregion

        private BufferedPanel applicationPanel;
        private System.Windows.Forms.Label p2vHeadingLabel;
        private System.Windows.Forms.Label addCredentialsLabel;
        private System.Windows.Forms.PictureBox recoverPictureBox;
        private System.Windows.Forms.Panel headingPanel;
        private System.Windows.Forms.Panel recoverPanel;
        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.Button getDetailsButton;
        public System.Windows.Forms.TextBox targetEsxIPText;
        private System.Windows.Forms.Label teargetEsxIPLabel;
        private System.Windows.Forms.Label targetEsxPasswordLabel;
        private System.Windows.Forms.TextBox targetEsxPasswordTextBox;
        private System.Windows.Forms.TextBox targetEsxUserNameTextBox;
        private System.Windows.Forms.Label targetEsxUserNameLabel;
        public System.Windows.Forms.DataGridView removeDataGridView;
        public System.Windows.Forms.ProgressBar progressBar;
        public System.Windows.Forms.Button nextButton;
        public System.Windows.Forms.Button doneButton;
        public System.Windows.Forms.Button cancelButton;
        public System.ComponentModel.BackgroundWorker removeDetailsBackgroundWorker;
        private System.Windows.Forms.Panel navigationPanel;
        public System.Windows.Forms.TextBox logTextBox;
        private System.Windows.Forms.LinkLabel clearLogsLinkLabel;
        public System.ComponentModel.BackgroundWorker detachBackgroundWorker;
        public System.Windows.Forms.Panel helpPanel;
        private System.Windows.Forms.LinkLabel esxProtectionLinkLabel;
        private System.Windows.Forms.PictureBox closePictureBox;
        private System.Windows.Forms.PictureBox helpPictureBox;
        public System.Windows.Forms.Label helpContentLabel;
        private System.Windows.Forms.Label versionLabel;
        public System.Windows.Forms.CheckBox selectAllCheckBox;
        private System.Windows.Forms.DataGridViewTextBoxColumn planName;
        private System.Windows.Forms.DataGridViewTextBoxColumn hostName;
        private System.Windows.Forms.DataGridViewTextBoxColumn displayName;
        private System.Windows.Forms.DataGridViewTextBoxColumn masterTarget;
        private System.Windows.Forms.DataGridViewCheckBoxColumn remove;
    }
}