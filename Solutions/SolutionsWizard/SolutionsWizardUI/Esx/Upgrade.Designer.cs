namespace Com.Inmage.Wizard
{
    partial class Upgrade
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
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(Upgrade));
            this.upgadePanel = new System.Windows.Forms.Panel();
            this.targetUpgradeButton = new System.Windows.Forms.Button();
            this.targetPasswordTextBox = new System.Windows.Forms.TextBox();
            this.targetPasswordLabel = new System.Windows.Forms.Label();
            this.targetUsernameTextBox = new System.Windows.Forms.TextBox();
            this.targetUsernameLabel = new System.Windows.Forms.Label();
            this.enterTargetIPTextBox = new System.Windows.Forms.TextBox();
            this.enterTargetvCenterIPLabel = new System.Windows.Forms.Label();
            this.doesTargetESXConnectedTovCenterCheckBox = new System.Windows.Forms.CheckBox();
            this.selectTargetvSphereIPLabel = new System.Windows.Forms.Label();
            this.selectTargetvSphereComboBox = new System.Windows.Forms.ComboBox();
            this.sourceUpdateToLatestButton = new System.Windows.Forms.Button();
            this.sourcePasswordTextBox = new System.Windows.Forms.TextBox();
            this.sourcePasswordLabel = new System.Windows.Forms.Label();
            this.sourceUsernameTextBox = new System.Windows.Forms.TextBox();
            this.sourceUsernameLabel = new System.Windows.Forms.Label();
            this.sourcevCenterIPTextBox = new System.Windows.Forms.TextBox();
            this.enterSourcevCenterIPLabel = new System.Windows.Forms.Label();
            this.sourcevCenterCheckBox = new System.Windows.Forms.CheckBox();
            this.selectSourceServerIPLabel = new System.Windows.Forms.Label();
            this.sourceServersComboBox = new System.Windows.Forms.ComboBox();
            this.upgradeDescriptionLabel = new System.Windows.Forms.Label();
            this.cancelButton = new System.Windows.Forms.Button();
            this.getDetailsBackgroundWorker = new System.ComponentModel.BackgroundWorker();
            this.progressBar = new System.Windows.Forms.ProgressBar();
            this.haedingPanel = new Com.Inmage.Wizard.BufferedPanel();
            this.versionLabel = new System.Windows.Forms.Label();
            this.patchLabel = new System.Windows.Forms.Label();
            this.applicationPanel = new Com.Inmage.Wizard.BufferedPanel();
            this.helpPanel = new System.Windows.Forms.Panel();
            this.closePictureBox = new System.Windows.Forms.PictureBox();
            this.helpContentLabel = new System.Windows.Forms.Label();
            this.upgradeLinkLabel = new System.Windows.Forms.LinkLabel();
            this.helpPictureBox = new System.Windows.Forms.PictureBox();
            this.upgradeSideLabel = new System.Windows.Forms.Label();
            this.addCredentialsLabel = new System.Windows.Forms.Label();
            this.upgradePictureBox = new System.Windows.Forms.PictureBox();
            this.upgadePanel.SuspendLayout();
            this.haedingPanel.SuspendLayout();
            this.applicationPanel.SuspendLayout();
            this.helpPanel.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.closePictureBox)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.helpPictureBox)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.upgradePictureBox)).BeginInit();
            this.SuspendLayout();
            // 
            // upgadePanel
            // 
            this.upgadePanel.BackColor = System.Drawing.Color.White;
            this.upgadePanel.BackgroundImage = global::Com.Inmage.Wizard.Properties.Resources.workarea;
            this.upgadePanel.Controls.Add(this.targetUpgradeButton);
            this.upgadePanel.Controls.Add(this.targetPasswordTextBox);
            this.upgadePanel.Controls.Add(this.targetPasswordLabel);
            this.upgadePanel.Controls.Add(this.targetUsernameTextBox);
            this.upgadePanel.Controls.Add(this.targetUsernameLabel);
            this.upgadePanel.Controls.Add(this.enterTargetIPTextBox);
            this.upgadePanel.Controls.Add(this.enterTargetvCenterIPLabel);
            this.upgadePanel.Controls.Add(this.doesTargetESXConnectedTovCenterCheckBox);
            this.upgadePanel.Controls.Add(this.selectTargetvSphereIPLabel);
            this.upgadePanel.Controls.Add(this.selectTargetvSphereComboBox);
            this.upgadePanel.Controls.Add(this.sourceUpdateToLatestButton);
            this.upgadePanel.Controls.Add(this.sourcePasswordTextBox);
            this.upgadePanel.Controls.Add(this.sourcePasswordLabel);
            this.upgadePanel.Controls.Add(this.sourceUsernameTextBox);
            this.upgadePanel.Controls.Add(this.sourceUsernameLabel);
            this.upgadePanel.Controls.Add(this.sourcevCenterIPTextBox);
            this.upgadePanel.Controls.Add(this.enterSourcevCenterIPLabel);
            this.upgadePanel.Controls.Add(this.sourcevCenterCheckBox);
            this.upgadePanel.Controls.Add(this.selectSourceServerIPLabel);
            this.upgadePanel.Controls.Add(this.sourceServersComboBox);
            this.upgadePanel.Controls.Add(this.upgradeDescriptionLabel);
            this.upgadePanel.Location = new System.Drawing.Point(169, 47);
            this.upgadePanel.Name = "upgadePanel";
            this.upgadePanel.Size = new System.Drawing.Size(677, 481);
            this.upgadePanel.TabIndex = 132;
            // 
            // targetUpgradeButton
            // 
            this.targetUpgradeButton.Location = new System.Drawing.Point(392, 241);
            this.targetUpgradeButton.Name = "targetUpgradeButton";
            this.targetUpgradeButton.Size = new System.Drawing.Size(109, 23);
            this.targetUpgradeButton.TabIndex = 150;
            this.targetUpgradeButton.Text = "OK";
            this.targetUpgradeButton.UseVisualStyleBackColor = true;
            this.targetUpgradeButton.Click += new System.EventHandler(this.targetUpgradeButton_Click);
            // 
            // targetPasswordTextBox
            // 
            this.targetPasswordTextBox.Location = new System.Drawing.Point(276, 242);
            this.targetPasswordTextBox.Name = "targetPasswordTextBox";
            this.targetPasswordTextBox.PasswordChar = '*';
            this.targetPasswordTextBox.Size = new System.Drawing.Size(100, 20);
            this.targetPasswordTextBox.TabIndex = 149;
            // 
            // targetPasswordLabel
            // 
            this.targetPasswordLabel.AutoSize = true;
            this.targetPasswordLabel.BackColor = System.Drawing.Color.Transparent;
            this.targetPasswordLabel.Location = new System.Drawing.Point(273, 223);
            this.targetPasswordLabel.Name = "targetPasswordLabel";
            this.targetPasswordLabel.Size = new System.Drawing.Size(53, 13);
            this.targetPasswordLabel.TabIndex = 153;
            this.targetPasswordLabel.Text = "Password";
            // 
            // targetUsernameTextBox
            // 
            this.targetUsernameTextBox.Location = new System.Drawing.Point(157, 242);
            this.targetUsernameTextBox.Name = "targetUsernameTextBox";
            this.targetUsernameTextBox.Size = new System.Drawing.Size(100, 20);
            this.targetUsernameTextBox.TabIndex = 148;
            // 
            // targetUsernameLabel
            // 
            this.targetUsernameLabel.AutoSize = true;
            this.targetUsernameLabel.BackColor = System.Drawing.Color.Transparent;
            this.targetUsernameLabel.Location = new System.Drawing.Point(154, 223);
            this.targetUsernameLabel.Name = "targetUsernameLabel";
            this.targetUsernameLabel.Size = new System.Drawing.Size(55, 13);
            this.targetUsernameLabel.TabIndex = 152;
            this.targetUsernameLabel.Text = "Username";
            // 
            // enterTargetIPTextBox
            // 
            this.enterTargetIPTextBox.Location = new System.Drawing.Point(17, 243);
            this.enterTargetIPTextBox.Name = "enterTargetIPTextBox";
            this.enterTargetIPTextBox.Size = new System.Drawing.Size(100, 20);
            this.enterTargetIPTextBox.TabIndex = 147;
            // 
            // enterTargetvCenterIPLabel
            // 
            this.enterTargetvCenterIPLabel.AutoSize = true;
            this.enterTargetvCenterIPLabel.BackColor = System.Drawing.Color.Transparent;
            this.enterTargetvCenterIPLabel.Location = new System.Drawing.Point(14, 223);
            this.enterTargetvCenterIPLabel.Name = "enterTargetvCenterIPLabel";
            this.enterTargetvCenterIPLabel.Size = new System.Drawing.Size(119, 13);
            this.enterTargetvCenterIPLabel.TabIndex = 151;
            this.enterTargetvCenterIPLabel.Text = "Enter Target vCenter IP";
            // 
            // doesTargetESXConnectedTovCenterCheckBox
            // 
            this.doesTargetESXConnectedTovCenterCheckBox.AutoSize = true;
            this.doesTargetESXConnectedTovCenterCheckBox.BackColor = System.Drawing.Color.Transparent;
            this.doesTargetESXConnectedTovCenterCheckBox.Location = new System.Drawing.Point(157, 188);
            this.doesTargetESXConnectedTovCenterCheckBox.Name = "doesTargetESXConnectedTovCenterCheckBox";
            this.doesTargetESXConnectedTovCenterCheckBox.Size = new System.Drawing.Size(205, 17);
            this.doesTargetESXConnectedTovCenterCheckBox.TabIndex = 146;
            this.doesTargetESXConnectedTovCenterCheckBox.Text = "Is this vSphere connected to vCenter.";
            this.doesTargetESXConnectedTovCenterCheckBox.UseVisualStyleBackColor = false;
            this.doesTargetESXConnectedTovCenterCheckBox.CheckedChanged += new System.EventHandler(this.doesTargetESXConnectedTovCenterCheckBox_CheckedChanged);
            // 
            // selectTargetvSphereIPLabel
            // 
            this.selectTargetvSphereIPLabel.AutoSize = true;
            this.selectTargetvSphereIPLabel.BackColor = System.Drawing.Color.Transparent;
            this.selectTargetvSphereIPLabel.Location = new System.Drawing.Point(14, 163);
            this.selectTargetvSphereIPLabel.Name = "selectTargetvSphereIPLabel";
            this.selectTargetvSphereIPLabel.Size = new System.Drawing.Size(127, 13);
            this.selectTargetvSphereIPLabel.TabIndex = 145;
            this.selectTargetvSphereIPLabel.Text = "Select Target vSphere IP";
            // 
            // selectTargetvSphereComboBox
            // 
            this.selectTargetvSphereComboBox.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.selectTargetvSphereComboBox.FormattingEnabled = true;
            this.selectTargetvSphereComboBox.Location = new System.Drawing.Point(17, 185);
            this.selectTargetvSphereComboBox.Name = "selectTargetvSphereComboBox";
            this.selectTargetvSphereComboBox.Size = new System.Drawing.Size(121, 21);
            this.selectTargetvSphereComboBox.TabIndex = 145;
            this.selectTargetvSphereComboBox.SelectedIndexChanged += new System.EventHandler(this.selectTargetvSphereComboBox_SelectedIndexChanged);
            // 
            // sourceUpdateToLatestButton
            // 
            this.sourceUpdateToLatestButton.Location = new System.Drawing.Point(392, 121);
            this.sourceUpdateToLatestButton.Name = "sourceUpdateToLatestButton";
            this.sourceUpdateToLatestButton.Size = new System.Drawing.Size(109, 23);
            this.sourceUpdateToLatestButton.TabIndex = 140;
            this.sourceUpdateToLatestButton.Text = "OK";
            this.sourceUpdateToLatestButton.UseVisualStyleBackColor = true;
            this.sourceUpdateToLatestButton.Click += new System.EventHandler(this.sourceUpdateToLatestButton_Click);
            // 
            // sourcePasswordTextBox
            // 
            this.sourcePasswordTextBox.Location = new System.Drawing.Point(276, 122);
            this.sourcePasswordTextBox.Name = "sourcePasswordTextBox";
            this.sourcePasswordTextBox.PasswordChar = '*';
            this.sourcePasswordTextBox.Size = new System.Drawing.Size(100, 20);
            this.sourcePasswordTextBox.TabIndex = 139;
            // 
            // sourcePasswordLabel
            // 
            this.sourcePasswordLabel.AutoSize = true;
            this.sourcePasswordLabel.BackColor = System.Drawing.Color.Transparent;
            this.sourcePasswordLabel.Location = new System.Drawing.Point(273, 103);
            this.sourcePasswordLabel.Name = "sourcePasswordLabel";
            this.sourcePasswordLabel.Size = new System.Drawing.Size(53, 13);
            this.sourcePasswordLabel.TabIndex = 143;
            this.sourcePasswordLabel.Text = "Password";
            // 
            // sourceUsernameTextBox
            // 
            this.sourceUsernameTextBox.Location = new System.Drawing.Point(157, 122);
            this.sourceUsernameTextBox.Name = "sourceUsernameTextBox";
            this.sourceUsernameTextBox.Size = new System.Drawing.Size(100, 20);
            this.sourceUsernameTextBox.TabIndex = 138;
            // 
            // sourceUsernameLabel
            // 
            this.sourceUsernameLabel.AutoSize = true;
            this.sourceUsernameLabel.BackColor = System.Drawing.Color.Transparent;
            this.sourceUsernameLabel.Location = new System.Drawing.Point(154, 103);
            this.sourceUsernameLabel.Name = "sourceUsernameLabel";
            this.sourceUsernameLabel.Size = new System.Drawing.Size(55, 13);
            this.sourceUsernameLabel.TabIndex = 142;
            this.sourceUsernameLabel.Text = "Username";
            // 
            // sourcevCenterIPTextBox
            // 
            this.sourcevCenterIPTextBox.Location = new System.Drawing.Point(17, 123);
            this.sourcevCenterIPTextBox.Name = "sourcevCenterIPTextBox";
            this.sourcevCenterIPTextBox.Size = new System.Drawing.Size(100, 20);
            this.sourcevCenterIPTextBox.TabIndex = 137;
            // 
            // enterSourcevCenterIPLabel
            // 
            this.enterSourcevCenterIPLabel.AutoSize = true;
            this.enterSourcevCenterIPLabel.BackColor = System.Drawing.Color.Transparent;
            this.enterSourcevCenterIPLabel.Location = new System.Drawing.Point(14, 103);
            this.enterSourcevCenterIPLabel.Name = "enterSourcevCenterIPLabel";
            this.enterSourcevCenterIPLabel.Size = new System.Drawing.Size(122, 13);
            this.enterSourcevCenterIPLabel.TabIndex = 141;
            this.enterSourcevCenterIPLabel.Text = "Enter Source vCenter IP";
            // 
            // sourcevCenterCheckBox
            // 
            this.sourcevCenterCheckBox.AutoSize = true;
            this.sourcevCenterCheckBox.BackColor = System.Drawing.Color.Transparent;
            this.sourcevCenterCheckBox.Location = new System.Drawing.Point(157, 68);
            this.sourcevCenterCheckBox.Name = "sourcevCenterCheckBox";
            this.sourcevCenterCheckBox.Size = new System.Drawing.Size(205, 17);
            this.sourcevCenterCheckBox.TabIndex = 136;
            this.sourcevCenterCheckBox.Text = "Is this vSphere connected to vCenter.";
            this.sourcevCenterCheckBox.UseVisualStyleBackColor = false;
            this.sourcevCenterCheckBox.CheckedChanged += new System.EventHandler(this.sourcevCenterCheckBox_CheckedChanged);
            // 
            // selectSourceServerIPLabel
            // 
            this.selectSourceServerIPLabel.AutoSize = true;
            this.selectSourceServerIPLabel.BackColor = System.Drawing.Color.Transparent;
            this.selectSourceServerIPLabel.Location = new System.Drawing.Point(14, 43);
            this.selectSourceServerIPLabel.Name = "selectSourceServerIPLabel";
            this.selectSourceServerIPLabel.Size = new System.Drawing.Size(130, 13);
            this.selectSourceServerIPLabel.TabIndex = 134;
            this.selectSourceServerIPLabel.Text = "Select Source vSphere IP";
            // 
            // sourceServersComboBox
            // 
            this.sourceServersComboBox.BackColor = System.Drawing.Color.White;
            this.sourceServersComboBox.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.sourceServersComboBox.FormattingEnabled = true;
            this.sourceServersComboBox.Location = new System.Drawing.Point(17, 65);
            this.sourceServersComboBox.Name = "sourceServersComboBox";
            this.sourceServersComboBox.Size = new System.Drawing.Size(121, 21);
            this.sourceServersComboBox.TabIndex = 135;
            this.sourceServersComboBox.SelectedIndexChanged += new System.EventHandler(this.sourceServersComboBox_SelectedIndexChanged);
            // 
            // upgradeDescriptionLabel
            // 
            this.upgradeDescriptionLabel.AutoSize = true;
            this.upgradeDescriptionLabel.BackColor = System.Drawing.Color.Transparent;
            this.upgradeDescriptionLabel.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.upgradeDescriptionLabel.Location = new System.Drawing.Point(3, 17);
            this.upgradeDescriptionLabel.Name = "upgradeDescriptionLabel";
            this.upgradeDescriptionLabel.Size = new System.Drawing.Size(334, 13);
            this.upgradeDescriptionLabel.TabIndex = 133;
            this.upgradeDescriptionLabel.Text = "Select Source and Target servers to Migrate (or) Upgrade";
            // 
            // cancelButton
            // 
            this.cancelButton.Location = new System.Drawing.Point(744, 557);
            this.cancelButton.Name = "cancelButton";
            this.cancelButton.Size = new System.Drawing.Size(75, 23);
            this.cancelButton.TabIndex = 133;
            this.cancelButton.Text = "Close";
            this.cancelButton.UseVisualStyleBackColor = true;
            this.cancelButton.Click += new System.EventHandler(this.cancelButton_Click);
            // 
            // getDetailsBackgroundWorker
            // 
            this.getDetailsBackgroundWorker.DoWork += new System.ComponentModel.DoWorkEventHandler(this.getDetailsBackgroundWorker_DoWork);
            this.getDetailsBackgroundWorker.RunWorkerCompleted += new System.ComponentModel.RunWorkerCompletedEventHandler(this.getDetailsBackgroundWorker_RunWorkerCompleted);
            this.getDetailsBackgroundWorker.ProgressChanged += new System.ComponentModel.ProgressChangedEventHandler(this.getDetailsBackgroundWorker_ProgressChanged);
            // 
            // progressBar
            // 
            this.progressBar.BackColor = System.Drawing.Color.LightGray;
            this.progressBar.ForeColor = System.Drawing.Color.ForestGreen;
            this.progressBar.Location = new System.Drawing.Point(169, 534);
            this.progressBar.Name = "progressBar";
            this.progressBar.Size = new System.Drawing.Size(677, 10);
            this.progressBar.Style = System.Windows.Forms.ProgressBarStyle.Marquee;
            this.progressBar.TabIndex = 134;
            this.progressBar.Visible = false;
            // 
            // haedingPanel
            // 
            this.haedingPanel.BackColor = System.Drawing.Color.Transparent;
            this.haedingPanel.BackgroundImage = global::Com.Inmage.Wizard.Properties.Resources.heading;
            this.haedingPanel.BackgroundImageLayout = System.Windows.Forms.ImageLayout.None;
            this.haedingPanel.Controls.Add(this.versionLabel);
            this.haedingPanel.Controls.Add(this.patchLabel);
            this.haedingPanel.Location = new System.Drawing.Point(-2, 0);
            this.haedingPanel.Name = "haedingPanel";
            this.haedingPanel.Size = new System.Drawing.Size(848, 44);
            this.haedingPanel.TabIndex = 130;
            // 
            // versionLabel
            // 
            this.versionLabel.AutoSize = true;
            this.versionLabel.Font = new System.Drawing.Font("Tahoma", 6.75F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.versionLabel.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(64)))), ((int)(((byte)(64)))), ((int)(((byte)(64)))));
            this.versionLabel.Location = new System.Drawing.Point(629, 27);
            this.versionLabel.Name = "versionLabel";
            this.versionLabel.Size = new System.Drawing.Size(98, 11);
            this.versionLabel.TabIndex = 3;
            this.versionLabel.Text = "5.50.1.GA.2047.1";
            // 
            // patchLabel
            // 
            this.patchLabel.AutoSize = true;
            this.patchLabel.Location = new System.Drawing.Point(734, 27);
            this.patchLabel.Name = "patchLabel";
            this.patchLabel.Size = new System.Drawing.Size(39, 13);
            this.patchLabel.TabIndex = 2;
            this.patchLabel.Text = "History";
            this.patchLabel.Visible = false;
            // 
            // applicationPanel
            // 
            this.applicationPanel.BackgroundImage = ((System.Drawing.Image)(resources.GetObject("applicationPanel.BackgroundImage")));
            this.applicationPanel.BackgroundImageLayout = System.Windows.Forms.ImageLayout.None;
            this.applicationPanel.Controls.Add(this.helpPanel);
            this.applicationPanel.Controls.Add(this.helpPictureBox);
            this.applicationPanel.Controls.Add(this.upgradeSideLabel);
            this.applicationPanel.Controls.Add(this.addCredentialsLabel);
            this.applicationPanel.Controls.Add(this.upgradePictureBox);
            this.applicationPanel.Location = new System.Drawing.Point(0, 50);
            this.applicationPanel.Name = "applicationPanel";
            this.applicationPanel.Size = new System.Drawing.Size(168, 550);
            this.applicationPanel.TabIndex = 131;
            // 
            // helpPanel
            // 
            this.helpPanel.BackColor = System.Drawing.Color.Transparent;
            this.helpPanel.BackgroundImage = ((System.Drawing.Image)(resources.GetObject("helpPanel.BackgroundImage")));
            this.helpPanel.Controls.Add(this.closePictureBox);
            this.helpPanel.Controls.Add(this.helpContentLabel);
            this.helpPanel.Controls.Add(this.upgradeLinkLabel);
            this.helpPanel.Location = new System.Drawing.Point(0, 250);
            this.helpPanel.Name = "helpPanel";
            this.helpPanel.Size = new System.Drawing.Size(163, 296);
            this.helpPanel.TabIndex = 90;
            this.helpPanel.Visible = false;
            // 
            // closePictureBox
            // 
            this.closePictureBox.BackgroundImage = ((System.Drawing.Image)(resources.GetObject("closePictureBox.BackgroundImage")));
            this.closePictureBox.BackgroundImageLayout = System.Windows.Forms.ImageLayout.None;
            this.closePictureBox.Location = new System.Drawing.Point(141, 11);
            this.closePictureBox.Name = "closePictureBox";
            this.closePictureBox.Size = new System.Drawing.Size(14, 14);
            this.closePictureBox.TabIndex = 5;
            this.closePictureBox.TabStop = false;
            this.closePictureBox.Click += new System.EventHandler(this.closePictureBox_Click);
            // 
            // helpContentLabel
            // 
            this.helpContentLabel.Font = new System.Drawing.Font("Tahoma", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.helpContentLabel.ForeColor = System.Drawing.Color.Black;
            this.helpContentLabel.Location = new System.Drawing.Point(9, 40);
            this.helpContentLabel.Name = "helpContentLabel";
            this.helpContentLabel.Size = new System.Drawing.Size(148, 222);
            this.helpContentLabel.TabIndex = 4;
            this.helpContentLabel.Text = "label1";
            // 
            // upgradeLinkLabel
            // 
            this.upgradeLinkLabel.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom)
                        | System.Windows.Forms.AnchorStyles.Left)
                        | System.Windows.Forms.AnchorStyles.Right)));
            this.upgradeLinkLabel.AutoSize = true;
            this.upgradeLinkLabel.Cursor = System.Windows.Forms.Cursors.Hand;
            this.upgradeLinkLabel.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.upgradeLinkLabel.LinkColor = System.Drawing.Color.FromArgb(((int)(((byte)(210)))), ((int)(((byte)(0)))), ((int)(((byte)(0)))));
            this.upgradeLinkLabel.Location = new System.Drawing.Point(20, 272);
            this.upgradeLinkLabel.Name = "upgradeLinkLabel";
            this.upgradeLinkLabel.Size = new System.Drawing.Size(121, 13);
            this.upgradeLinkLabel.TabIndex = 3;
            this.upgradeLinkLabel.TabStop = true;
            this.upgradeLinkLabel.Text = "Click here to know more";
            this.upgradeLinkLabel.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
            this.upgradeLinkLabel.LinkClicked += new System.Windows.Forms.LinkLabelLinkClickedEventHandler(this.esxProtectionLinkLabel_LinkClicked);
            // 
            // helpPictureBox
            // 
            this.helpPictureBox.BackgroundImage = ((System.Drawing.Image)(resources.GetObject("helpPictureBox.BackgroundImage")));
            this.helpPictureBox.BackgroundImageLayout = System.Windows.Forms.ImageLayout.None;
            this.helpPictureBox.Location = new System.Drawing.Point(-2, 513);
            this.helpPictureBox.Name = "helpPictureBox";
            this.helpPictureBox.Size = new System.Drawing.Size(168, 33);
            this.helpPictureBox.TabIndex = 91;
            this.helpPictureBox.TabStop = false;
            this.helpPictureBox.Click += new System.EventHandler(this.helpPictureBox_Click);
            // 
            // upgradeSideLabel
            // 
            this.upgradeSideLabel.BackColor = System.Drawing.Color.Transparent;
            this.upgradeSideLabel.Font = new System.Drawing.Font("Tahoma", 8.25F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.upgradeSideLabel.ForeColor = System.Drawing.Color.White;
            this.upgradeSideLabel.Location = new System.Drawing.Point(31, 68);
            this.upgradeSideLabel.Name = "upgradeSideLabel";
            this.upgradeSideLabel.Size = new System.Drawing.Size(129, 88);
            this.upgradeSideLabel.TabIndex = 89;
            this.upgradeSideLabel.Text = "Migrate Protections from vSphere  to vCenter (or)          Upgrade older plans to" +
                " latest version";
            this.upgradeSideLabel.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            // 
            // addCredentialsLabel
            // 
            this.addCredentialsLabel.AutoSize = true;
            this.addCredentialsLabel.BackColor = System.Drawing.Color.Transparent;
            this.addCredentialsLabel.Font = new System.Drawing.Font("Tahoma", 8.25F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.addCredentialsLabel.ForeColor = System.Drawing.Color.White;
            this.addCredentialsLabel.Location = new System.Drawing.Point(31, 68);
            this.addCredentialsLabel.Name = "addCredentialsLabel";
            this.addCredentialsLabel.Size = new System.Drawing.Size(129, 13);
            this.addCredentialsLabel.TabIndex = 89;
            this.addCredentialsLabel.Text = "Select Primary VM (s)";
            this.addCredentialsLabel.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
            // 
            // upgradePictureBox
            // 
            this.upgradePictureBox.BackColor = System.Drawing.Color.Transparent;
            this.upgradePictureBox.Image = ((System.Drawing.Image)(resources.GetObject("upgradePictureBox.Image")));
            this.upgradePictureBox.Location = new System.Drawing.Point(6, 68);
            this.upgradePictureBox.Name = "upgradePictureBox";
            this.upgradePictureBox.Size = new System.Drawing.Size(38, 24);
            this.upgradePictureBox.SizeMode = System.Windows.Forms.PictureBoxSizeMode.CenterImage;
            this.upgradePictureBox.TabIndex = 88;
            this.upgradePictureBox.TabStop = false;
            // 
            // Upgrade
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.BackColor = System.Drawing.Color.White;
            this.ClientSize = new System.Drawing.Size(847, 592);
            this.Controls.Add(this.progressBar);
            this.Controls.Add(this.cancelButton);
            this.Controls.Add(this.upgadePanel);
            this.Controls.Add(this.haedingPanel);
            this.Controls.Add(this.applicationPanel);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.MaximizeBox = false;
            this.Name = "Upgrade";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
            this.Text = "Upgrade";
            this.upgadePanel.ResumeLayout(false);
            this.upgadePanel.PerformLayout();
            this.haedingPanel.ResumeLayout(false);
            this.haedingPanel.PerformLayout();
            this.applicationPanel.ResumeLayout(false);
            this.applicationPanel.PerformLayout();
            this.helpPanel.ResumeLayout(false);
            this.helpPanel.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this.closePictureBox)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.helpPictureBox)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.upgradePictureBox)).EndInit();
            this.ResumeLayout(false);

        }

        #endregion

        private BufferedPanel haedingPanel;
        private System.Windows.Forms.Label patchLabel;
        private BufferedPanel applicationPanel;
        public System.Windows.Forms.Label addCredentialsLabel;
        private System.Windows.Forms.PictureBox upgradePictureBox;
        private System.Windows.Forms.Panel upgadePanel;
        private System.Windows.Forms.Label upgradeDescriptionLabel;
        private System.Windows.Forms.Label selectSourceServerIPLabel;
        private System.Windows.Forms.ComboBox sourceServersComboBox;
        private System.Windows.Forms.CheckBox sourcevCenterCheckBox;
        private System.Windows.Forms.TextBox sourceUsernameTextBox;
        private System.Windows.Forms.Label sourceUsernameLabel;
        private System.Windows.Forms.TextBox sourcevCenterIPTextBox;
        private System.Windows.Forms.Label enterSourcevCenterIPLabel;
        private System.Windows.Forms.Button sourceUpdateToLatestButton;
        private System.Windows.Forms.TextBox sourcePasswordTextBox;
        private System.Windows.Forms.Label sourcePasswordLabel;
        private System.Windows.Forms.Button targetUpgradeButton;
        private System.Windows.Forms.TextBox targetPasswordTextBox;
        private System.Windows.Forms.Label targetPasswordLabel;
        private System.Windows.Forms.TextBox targetUsernameTextBox;
        private System.Windows.Forms.Label targetUsernameLabel;
        private System.Windows.Forms.TextBox enterTargetIPTextBox;
        private System.Windows.Forms.Label enterTargetvCenterIPLabel;
        private System.Windows.Forms.CheckBox doesTargetESXConnectedTovCenterCheckBox;
        private System.Windows.Forms.Label selectTargetvSphereIPLabel;
        private System.Windows.Forms.ComboBox selectTargetvSphereComboBox;
        private System.Windows.Forms.Button cancelButton;
        private System.ComponentModel.BackgroundWorker getDetailsBackgroundWorker;
        private System.Windows.Forms.Label versionLabel;
        private System.Windows.Forms.PictureBox closePictureBox;
        public System.Windows.Forms.Label helpContentLabel;
        private System.Windows.Forms.LinkLabel upgradeLinkLabel;
        private System.Windows.Forms.PictureBox helpPictureBox;
        internal System.Windows.Forms.Label upgradeSideLabel;
        internal System.Windows.Forms.Panel helpPanel;
        internal System.Windows.Forms.ProgressBar progressBar;
    }
}