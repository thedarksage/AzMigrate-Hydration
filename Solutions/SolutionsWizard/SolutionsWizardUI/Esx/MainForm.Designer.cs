namespace com.InMage.Wizard
{
    partial class MainForm
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
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(MainForm));
            this.mainFormPanel = new com.InMage.Wizard.BufferedPanel();
            this.pushAgentTextLabel = new System.Windows.Forms.Label();
            this.pushAgentLinkLabel = new System.Windows.Forms.LinkLabel();
            this.monitorDescriptionLabel = new System.Windows.Forms.Label();
            this.protectMoreLabel = new System.Windows.Forms.Label();
            this.monitorlinkLabel = new System.Windows.Forms.LinkLabel();
            this.forvSphereLabel = new System.Windows.Forms.Label();
            this.tmLabel = new System.Windows.Forms.Label();
            this.esxProlectionLabel = new System.Windows.Forms.Label();
            this.removeMoreLabel = new System.Windows.Forms.Label();
            this.removeTextLabel = new System.Windows.Forms.Label();
            this.removeLinkLabel = new System.Windows.Forms.LinkLabel();
            this.offlineSyncMoreLabel = new System.Windows.Forms.Label();
            this.failbackMoreLabel = new System.Windows.Forms.Label();
            this.recoverMoreLabel = new System.Windows.Forms.Label();
            this.failBackTextLabel = new System.Windows.Forms.Label();
            this.recoverTextLabel = new System.Windows.Forms.Label();
            this.selectApplicationComboBox = new System.Windows.Forms.ComboBox();
            this.applicationLabel = new System.Windows.Forms.Label();
            this.offlineSyncLinkLabel = new System.Windows.Forms.LinkLabel();
            this.protectLinkLabel = new System.Windows.Forms.LinkLabel();
            this.failBackLinkLabel = new System.Windows.Forms.LinkLabel();
            this.recoverLinkLabel = new System.Windows.Forms.LinkLabel();
            this.protectTextLabel = new System.Windows.Forms.Label();
            this.protectGroupLabel = new com.InMage.Wizard.BufferedPanel();
            this.resumeRadioButton = new System.Windows.Forms.RadioButton();
            this.newPlanRadioButton = new System.Windows.Forms.RadioButton();
            this.addDiskToExistingPlanRadioButton = new System.Windows.Forms.RadioButton();
            this.offlineSyncGroupPanel = new com.InMage.Wizard.BufferedPanel();
            this.importRadioButton = new System.Windows.Forms.RadioButton();
            this.exportRadioButton = new System.Windows.Forms.RadioButton();
            this.offlineSyncTextLabel = new System.Windows.Forms.Label();
            this.haedingPanel = new com.InMage.Wizard.BufferedPanel();
            this.versionLabel = new System.Windows.Forms.Label();
            this.mainFormPanel.SuspendLayout();
            this.protectGroupLabel.SuspendLayout();
            this.offlineSyncGroupPanel.SuspendLayout();
            this.haedingPanel.SuspendLayout();
            this.SuspendLayout();
            // 
            // mainFormPanel
            // 
            this.mainFormPanel.BackColor = System.Drawing.Color.Transparent;
            this.mainFormPanel.BackgroundImage = global::com.InMage.Wizard.Properties.Resources.mainBg;
            this.mainFormPanel.Controls.Add(this.pushAgentTextLabel);
            this.mainFormPanel.Controls.Add(this.pushAgentLinkLabel);
            this.mainFormPanel.Controls.Add(this.monitorDescriptionLabel);
            this.mainFormPanel.Controls.Add(this.protectMoreLabel);
            this.mainFormPanel.Controls.Add(this.monitorlinkLabel);
            this.mainFormPanel.Controls.Add(this.forvSphereLabel);
            this.mainFormPanel.Controls.Add(this.tmLabel);
            this.mainFormPanel.Controls.Add(this.esxProlectionLabel);
            this.mainFormPanel.Controls.Add(this.removeMoreLabel);
            this.mainFormPanel.Controls.Add(this.removeTextLabel);
            this.mainFormPanel.Controls.Add(this.removeLinkLabel);
            this.mainFormPanel.Controls.Add(this.offlineSyncMoreLabel);
            this.mainFormPanel.Controls.Add(this.failbackMoreLabel);
            this.mainFormPanel.Controls.Add(this.recoverMoreLabel);
            this.mainFormPanel.Controls.Add(this.failBackTextLabel);
            this.mainFormPanel.Controls.Add(this.recoverTextLabel);
            this.mainFormPanel.Controls.Add(this.selectApplicationComboBox);
            this.mainFormPanel.Controls.Add(this.applicationLabel);
            this.mainFormPanel.Controls.Add(this.offlineSyncLinkLabel);
            this.mainFormPanel.Controls.Add(this.protectLinkLabel);
            this.mainFormPanel.Controls.Add(this.failBackLinkLabel);
            this.mainFormPanel.Controls.Add(this.recoverLinkLabel);
            this.mainFormPanel.Controls.Add(this.protectTextLabel);
            this.mainFormPanel.Controls.Add(this.protectGroupLabel);
            this.mainFormPanel.Controls.Add(this.offlineSyncGroupPanel);
            this.mainFormPanel.Controls.Add(this.offlineSyncTextLabel);
            this.mainFormPanel.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(64)))), ((int)(((byte)(64)))), ((int)(((byte)(64)))));
            this.mainFormPanel.Location = new System.Drawing.Point(0, 44);
            this.mainFormPanel.Name = "mainFormPanel";
            this.mainFormPanel.Size = new System.Drawing.Size(848, 532);
            this.mainFormPanel.TabIndex = 129;
            this.mainFormPanel.Paint += new System.Windows.Forms.PaintEventHandler(this.mainFormPanel_Paint);
            // 
            // pushAgentTextLabel
            // 
            this.pushAgentTextLabel.AutoSize = true;
            this.pushAgentTextLabel.Location = new System.Drawing.Point(83, 115);
            this.pushAgentTextLabel.Name = "pushAgentTextLabel";
            this.pushAgentTextLabel.Size = new System.Drawing.Size(187, 13);
            this.pushAgentTextLabel.TabIndex = 149;
            this.pushAgentTextLabel.Text = "Select VMs to push and install agents.";
            // 
            // pushAgentLinkLabel
            // 
            this.pushAgentLinkLabel.AutoSize = true;
            this.pushAgentLinkLabel.Font = new System.Drawing.Font("Microsoft Sans Serif", 9.75F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.pushAgentLinkLabel.LinkColor = System.Drawing.Color.SteelBlue;
            this.pushAgentLinkLabel.Location = new System.Drawing.Point(83, 95);
            this.pushAgentLinkLabel.Name = "pushAgentLinkLabel";
            this.pushAgentLinkLabel.Size = new System.Drawing.Size(86, 16);
            this.pushAgentLinkLabel.TabIndex = 148;
            this.pushAgentLinkLabel.TabStop = true;
            this.pushAgentLinkLabel.Text = "Push Agent";
            this.pushAgentLinkLabel.LinkClicked += new System.Windows.Forms.LinkLabelLinkClickedEventHandler(this.pushAgentLinkLabel_LinkClicked);
            // 
            // monitorDescriptionLabel
            // 
            this.monitorDescriptionLabel.AutoSize = true;
            this.monitorDescriptionLabel.Location = new System.Drawing.Point(83, 492);
            this.monitorDescriptionLabel.Name = "monitorDescriptionLabel";
            this.monitorDescriptionLabel.Size = new System.Drawing.Size(381, 13);
            this.monitorDescriptionLabel.TabIndex = 147;
            this.monitorDescriptionLabel.Text = "Click here to show the Cx Interface for monitoring existing plans and jobs status" +
                ".";
            // 
            // protectMoreLabel
            // 
            this.protectMoreLabel.AutoSize = true;
            this.protectMoreLabel.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(255)))), ((int)(((byte)(128)))), ((int)(((byte)(0)))));
            this.protectMoreLabel.Location = new System.Drawing.Point(705, 189);
            this.protectMoreLabel.Name = "protectMoreLabel";
            this.protectMoreLabel.Size = new System.Drawing.Size(31, 13);
            this.protectMoreLabel.TabIndex = 134;
            this.protectMoreLabel.Text = "More";
            this.protectMoreLabel.Click += new System.EventHandler(this.protectMoreLabel_Click);
            // 
            // monitorlinkLabel
            // 
            this.monitorlinkLabel.AutoSize = true;
            this.monitorlinkLabel.Font = new System.Drawing.Font("Microsoft Sans Serif", 9.75F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.monitorlinkLabel.LinkColor = System.Drawing.Color.SteelBlue;
            this.monitorlinkLabel.Location = new System.Drawing.Point(83, 474);
            this.monitorlinkLabel.Name = "monitorlinkLabel";
            this.monitorlinkLabel.Size = new System.Drawing.Size(59, 16);
            this.monitorlinkLabel.TabIndex = 146;
            this.monitorlinkLabel.TabStop = true;
            this.monitorlinkLabel.Text = "Monitor";
            this.monitorlinkLabel.LinkClicked += new System.Windows.Forms.LinkLabelLinkClickedEventHandler(this.monitorlinkLabel_LinkClicked);
            // 
            // forvSphereLabel
            // 
            this.forvSphereLabel.AutoSize = true;
            this.forvSphereLabel.Font = new System.Drawing.Font("Microsoft Sans Serif", 12F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.forvSphereLabel.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(64)))), ((int)(((byte)(64)))), ((int)(((byte)(64)))));
            this.forvSphereLabel.Location = new System.Drawing.Point(187, 42);
            this.forvSphereLabel.Name = "forvSphereLabel";
            this.forvSphereLabel.Size = new System.Drawing.Size(102, 20);
            this.forvSphereLabel.TabIndex = 145;
            this.forvSphereLabel.Text = "for vSphere";
            // 
            // tmLabel
            // 
            this.tmLabel.AutoSize = true;
            this.tmLabel.Font = new System.Drawing.Font("Microsoft Sans Serif", 6F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.tmLabel.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(64)))), ((int)(((byte)(64)))), ((int)(((byte)(64)))));
            this.tmLabel.Location = new System.Drawing.Point(286, 39);
            this.tmLabel.Name = "tmLabel";
            this.tmLabel.Size = new System.Drawing.Size(27, 9);
            this.tmLabel.TabIndex = 144;
            this.tmLabel.Text = "(TM)";
            // 
            // esxProlectionLabel
            // 
            this.esxProlectionLabel.AutoSize = true;
            this.esxProlectionLabel.Font = new System.Drawing.Font("Tahoma", 12F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.esxProlectionLabel.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(179)))), ((int)(((byte)(48)))), ((int)(((byte)(54)))));
            this.esxProlectionLabel.Location = new System.Drawing.Point(83, 42);
            this.esxProlectionLabel.Name = "esxProlectionLabel";
            this.esxProlectionLabel.Size = new System.Drawing.Size(111, 19);
            this.esxProlectionLabel.TabIndex = 143;
            this.esxProlectionLabel.Text = "vContinuum ";
            // 
            // removeMoreLabel
            // 
            this.removeMoreLabel.AutoSize = true;
            this.removeMoreLabel.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(255)))), ((int)(((byte)(128)))), ((int)(((byte)(0)))));
            this.removeMoreLabel.Location = new System.Drawing.Point(330, 445);
            this.removeMoreLabel.Name = "removeMoreLabel";
            this.removeMoreLabel.Size = new System.Drawing.Size(31, 13);
            this.removeMoreLabel.TabIndex = 142;
            this.removeMoreLabel.Text = "More";
            this.removeMoreLabel.Click += new System.EventHandler(this.removeMoreLabel_Click);
            // 
            // removeTextLabel
            // 
            this.removeTextLabel.AutoSize = true;
            this.removeTextLabel.Font = new System.Drawing.Font("Tahoma", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.removeTextLabel.Location = new System.Drawing.Point(83, 445);
            this.removeTextLabel.Name = "removeTextLabel";
            this.removeTextLabel.Size = new System.Drawing.Size(247, 13);
            this.removeTextLabel.TabIndex = 141;
            this.removeTextLabel.Text = "Remove the protection and clean up stale entries.";
            // 
            // removeLinkLabel
            // 
            this.removeLinkLabel.AutoSize = true;
            this.removeLinkLabel.Font = new System.Drawing.Font("Tahoma", 9.75F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.removeLinkLabel.LinkColor = System.Drawing.Color.SteelBlue;
            this.removeLinkLabel.Location = new System.Drawing.Point(83, 421);
            this.removeLinkLabel.Name = "removeLinkLabel";
            this.removeLinkLabel.Size = new System.Drawing.Size(60, 16);
            this.removeLinkLabel.TabIndex = 140;
            this.removeLinkLabel.TabStop = true;
            this.removeLinkLabel.Text = "Remove";
            this.removeLinkLabel.LinkClicked += new System.Windows.Forms.LinkLabelLinkClickedEventHandler(this.removeLinkLabel_LinkClicked);
            // 
            // offlineSyncMoreLabel
            // 
            this.offlineSyncMoreLabel.AutoSize = true;
            this.offlineSyncMoreLabel.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(255)))), ((int)(((byte)(128)))), ((int)(((byte)(0)))));
            this.offlineSyncMoreLabel.Location = new System.Drawing.Point(494, 370);
            this.offlineSyncMoreLabel.Name = "offlineSyncMoreLabel";
            this.offlineSyncMoreLabel.Size = new System.Drawing.Size(31, 13);
            this.offlineSyncMoreLabel.TabIndex = 137;
            this.offlineSyncMoreLabel.Text = "More";
            this.offlineSyncMoreLabel.Click += new System.EventHandler(this.offlineSyncMoreLabel_Click);
            // 
            // failbackMoreLabel
            // 
            this.failbackMoreLabel.AutoSize = true;
            this.failbackMoreLabel.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(255)))), ((int)(((byte)(128)))), ((int)(((byte)(0)))));
            this.failbackMoreLabel.Location = new System.Drawing.Point(793, 309);
            this.failbackMoreLabel.Name = "failbackMoreLabel";
            this.failbackMoreLabel.Size = new System.Drawing.Size(31, 13);
            this.failbackMoreLabel.TabIndex = 136;
            this.failbackMoreLabel.Text = "More";
            this.failbackMoreLabel.Click += new System.EventHandler(this.failbackMoreLabel_Click);
            // 
            // recoverMoreLabel
            // 
            this.recoverMoreLabel.AutoSize = true;
            this.recoverMoreLabel.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(255)))), ((int)(((byte)(128)))), ((int)(((byte)(0)))));
            this.recoverMoreLabel.Location = new System.Drawing.Point(705, 241);
            this.recoverMoreLabel.Name = "recoverMoreLabel";
            this.recoverMoreLabel.Size = new System.Drawing.Size(31, 13);
            this.recoverMoreLabel.TabIndex = 135;
            this.recoverMoreLabel.Text = "More";
            this.recoverMoreLabel.Click += new System.EventHandler(this.recoverMoreLabel_Click);
            // 
            // failBackTextLabel
            // 
            this.failBackTextLabel.AutoSize = true;
            this.failBackTextLabel.Font = new System.Drawing.Font("Tahoma", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.failBackTextLabel.Location = new System.Drawing.Point(81, 296);
            this.failBackTextLabel.Name = "failBackTextLabel";
            this.failBackTextLabel.Size = new System.Drawing.Size(752, 13);
            this.failBackTextLabel.TabIndex = 132;
            this.failBackTextLabel.Text = "After a machine is failed over to the target site, any changes made on to that ma" +
                "chine at target side can be synced back to the source site by failing back. ";
            // 
            // recoverTextLabel
            // 
            this.recoverTextLabel.AutoSize = true;
            this.recoverTextLabel.Font = new System.Drawing.Font("Tahoma", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.recoverTextLabel.Location = new System.Drawing.Point(83, 241);
            this.recoverTextLabel.Name = "recoverTextLabel";
            this.recoverTextLabel.Size = new System.Drawing.Size(644, 13);
            this.recoverTextLabel.TabIndex = 131;
            this.recoverTextLabel.Text = "Failover to VMs on secondary ESX server.  Data replication from source to target " +
                "is stopped and VM on target machine is brought up.";
            // 
            // selectApplicationComboBox
            // 
            this.selectApplicationComboBox.BackColor = System.Drawing.Color.AliceBlue;
            this.selectApplicationComboBox.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.selectApplicationComboBox.Location = new System.Drawing.Point(220, 42);
            this.selectApplicationComboBox.Name = "selectApplicationComboBox";
            this.selectApplicationComboBox.Size = new System.Drawing.Size(111, 21);
            this.selectApplicationComboBox.TabIndex = 106;
            this.selectApplicationComboBox.SelectedIndexChanged += new System.EventHandler(this.selectApplicationComboBox_SelectedIndexChanged);
            // 
            // applicationLabel
            // 
            this.applicationLabel.AutoSize = true;
            this.applicationLabel.BackColor = System.Drawing.Color.Transparent;
            this.applicationLabel.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Bold);
            this.applicationLabel.ForeColor = System.Drawing.SystemColors.ControlText;
            this.applicationLabel.ImeMode = System.Windows.Forms.ImeMode.NoControl;
            this.applicationLabel.Location = new System.Drawing.Point(81, 48);
            this.applicationLabel.Name = "applicationLabel";
            this.applicationLabel.Size = new System.Drawing.Size(116, 13);
            this.applicationLabel.TabIndex = 107;
            this.applicationLabel.Text = "Choose Application";
            // 
            // offlineSyncLinkLabel
            // 
            this.offlineSyncLinkLabel.AutoSize = true;
            this.offlineSyncLinkLabel.BackColor = System.Drawing.Color.Transparent;
            this.offlineSyncLinkLabel.Font = new System.Drawing.Font("Tahoma", 9.75F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.offlineSyncLinkLabel.LinkColor = System.Drawing.Color.SteelBlue;
            this.offlineSyncLinkLabel.Location = new System.Drawing.Point(83, 336);
            this.offlineSyncLinkLabel.Name = "offlineSyncLinkLabel";
            this.offlineSyncLinkLabel.Size = new System.Drawing.Size(84, 16);
            this.offlineSyncLinkLabel.TabIndex = 127;
            this.offlineSyncLinkLabel.TabStop = true;
            this.offlineSyncLinkLabel.Text = "Offline Sync";
            this.offlineSyncLinkLabel.LinkClicked += new System.Windows.Forms.LinkLabelLinkClickedEventHandler(this.offlineSyncLinkLabel_LinkClicked);
            // 
            // protectLinkLabel
            // 
            this.protectLinkLabel.AutoSize = true;
            this.protectLinkLabel.BackColor = System.Drawing.Color.Transparent;
            this.protectLinkLabel.Font = new System.Drawing.Font("Tahoma", 9.75F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.protectLinkLabel.LinkColor = System.Drawing.Color.SteelBlue;
            this.protectLinkLabel.Location = new System.Drawing.Point(83, 143);
            this.protectLinkLabel.Name = "protectLinkLabel";
            this.protectLinkLabel.Size = new System.Drawing.Size(57, 16);
            this.protectLinkLabel.TabIndex = 124;
            this.protectLinkLabel.TabStop = true;
            this.protectLinkLabel.Text = "Protect";
            this.protectLinkLabel.LinkClicked += new System.Windows.Forms.LinkLabelLinkClickedEventHandler(this.protectLinkLabel_LinkClicked);
            // 
            // failBackLinkLabel
            // 
            this.failBackLinkLabel.AutoSize = true;
            this.failBackLinkLabel.BackColor = System.Drawing.Color.Transparent;
            this.failBackLinkLabel.Font = new System.Drawing.Font("Tahoma", 9.75F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.failBackLinkLabel.LinkColor = System.Drawing.Color.SteelBlue;
            this.failBackLinkLabel.Location = new System.Drawing.Point(83, 273);
            this.failBackLinkLabel.Name = "failBackLinkLabel";
            this.failBackLinkLabel.Size = new System.Drawing.Size(130, 16);
            this.failBackLinkLabel.TabIndex = 126;
            this.failBackLinkLabel.TabStop = true;
            this.failBackLinkLabel.Text = "Failback Protection";
            this.failBackLinkLabel.LinkClicked += new System.Windows.Forms.LinkLabelLinkClickedEventHandler(this.failBackLinkLabel_LinkClicked);
            // 
            // recoverLinkLabel
            // 
            this.recoverLinkLabel.AutoSize = true;
            this.recoverLinkLabel.BackColor = System.Drawing.Color.Transparent;
            this.recoverLinkLabel.Font = new System.Drawing.Font("Tahoma", 9.75F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.recoverLinkLabel.LinkColor = System.Drawing.Color.SteelBlue;
            this.recoverLinkLabel.Location = new System.Drawing.Point(83, 218);
            this.recoverLinkLabel.Name = "recoverLinkLabel";
            this.recoverLinkLabel.Size = new System.Drawing.Size(62, 16);
            this.recoverLinkLabel.TabIndex = 125;
            this.recoverLinkLabel.TabStop = true;
            this.recoverLinkLabel.Text = "Recover";
            this.recoverLinkLabel.LinkClicked += new System.Windows.Forms.LinkLabelLinkClickedEventHandler(this.recoverLinkLabel_LinkClicked);
            // 
            // protectTextLabel
            // 
            this.protectTextLabel.AutoSize = true;
            this.protectTextLabel.Font = new System.Drawing.Font("Tahoma", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.protectTextLabel.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(64)))), ((int)(((byte)(64)))), ((int)(((byte)(64)))));
            this.protectTextLabel.Location = new System.Drawing.Point(83, 168);
            this.protectTextLabel.Name = "protectTextLabel";
            this.protectTextLabel.Size = new System.Drawing.Size(305, 26);
            this.protectTextLabel.TabIndex = 130;
            this.protectTextLabel.Text = "Start  new protection of one or more VMS \r\nStart protecting newly added disk of a" +
                "n already protected VM \r\n";
            // 
            // protectGroupLabel
            // 
            this.protectGroupLabel.BackgroundImage = global::com.InMage.Wizard.Properties.Resources.options5;
            this.protectGroupLabel.BackgroundImageLayout = System.Windows.Forms.ImageLayout.Stretch;
            this.protectGroupLabel.Controls.Add(this.resumeRadioButton);
            this.protectGroupLabel.Controls.Add(this.newPlanRadioButton);
            this.protectGroupLabel.Controls.Add(this.addDiskToExistingPlanRadioButton);
            this.protectGroupLabel.Location = new System.Drawing.Point(93, 162);
            this.protectGroupLabel.Name = "protectGroupLabel";
            this.protectGroupLabel.Size = new System.Drawing.Size(599, 38);
            this.protectGroupLabel.TabIndex = 138;
            this.protectGroupLabel.Visible = false;
            // 
            // resumeRadioButton
            // 
            this.resumeRadioButton.AutoSize = true;
            this.resumeRadioButton.Location = new System.Drawing.Point(143, 10);
            this.resumeRadioButton.Name = "resumeRadioButton";
            this.resumeRadioButton.Size = new System.Drawing.Size(152, 17);
            this.resumeRadioButton.TabIndex = 127;
            this.resumeRadioButton.TabStop = true;
            this.resumeRadioButton.Text = "Resume existing protection";
            this.resumeRadioButton.UseVisualStyleBackColor = true;
            this.resumeRadioButton.CheckedChanged += new System.EventHandler(this.resumeRadioButton_CheckedChanged);
            // 
            // newPlanRadioButton
            // 
            this.newPlanRadioButton.AutoSize = true;
            this.newPlanRadioButton.BackColor = System.Drawing.Color.Transparent;
            this.newPlanRadioButton.Location = new System.Drawing.Point(12, 10);
            this.newPlanRadioButton.Name = "newPlanRadioButton";
            this.newPlanRadioButton.Size = new System.Drawing.Size(98, 17);
            this.newPlanRadioButton.TabIndex = 125;
            this.newPlanRadioButton.TabStop = true;
            this.newPlanRadioButton.Text = "New Protection";
            this.newPlanRadioButton.UseVisualStyleBackColor = false;
            this.newPlanRadioButton.Visible = false;
            this.newPlanRadioButton.CheckedChanged += new System.EventHandler(this.newPlanRadioButton_CheckedChanged);
            // 
            // addDiskToExistingPlanRadioButton
            // 
            this.addDiskToExistingPlanRadioButton.AutoSize = true;
            this.addDiskToExistingPlanRadioButton.BackColor = System.Drawing.Color.Transparent;
            this.addDiskToExistingPlanRadioButton.Font = new System.Drawing.Font("Tahoma", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.addDiskToExistingPlanRadioButton.Location = new System.Drawing.Point(338, 10);
            this.addDiskToExistingPlanRadioButton.Name = "addDiskToExistingPlanRadioButton";
            this.addDiskToExistingPlanRadioButton.Size = new System.Drawing.Size(239, 17);
            this.addDiskToExistingPlanRadioButton.TabIndex = 126;
            this.addDiskToExistingPlanRadioButton.TabStop = true;
            this.addDiskToExistingPlanRadioButton.Text = "Protect newly added disks of a protected VM";
            this.addDiskToExistingPlanRadioButton.UseVisualStyleBackColor = false;
            this.addDiskToExistingPlanRadioButton.Visible = false;
            this.addDiskToExistingPlanRadioButton.CheckedChanged += new System.EventHandler(this.addDiskToExistingPlanRadioButton_CheckedChanged);
            // 
            // offlineSyncGroupPanel
            // 
            this.offlineSyncGroupPanel.BackgroundImage = global::com.InMage.Wizard.Properties.Resources.options5;
            this.offlineSyncGroupPanel.BackgroundImageLayout = System.Windows.Forms.ImageLayout.None;
            this.offlineSyncGroupPanel.Controls.Add(this.importRadioButton);
            this.offlineSyncGroupPanel.Controls.Add(this.exportRadioButton);
            this.offlineSyncGroupPanel.Location = new System.Drawing.Point(79, 361);
            this.offlineSyncGroupPanel.Name = "offlineSyncGroupPanel";
            this.offlineSyncGroupPanel.Size = new System.Drawing.Size(475, 41);
            this.offlineSyncGroupPanel.TabIndex = 139;
            this.offlineSyncGroupPanel.Visible = false;
            // 
            // importRadioButton
            // 
            this.importRadioButton.AutoSize = true;
            this.importRadioButton.BackColor = System.Drawing.Color.Transparent;
            this.importRadioButton.Font = new System.Drawing.Font("Tahoma", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.importRadioButton.Location = new System.Drawing.Point(89, 10);
            this.importRadioButton.Name = "importRadioButton";
            this.importRadioButton.Size = new System.Drawing.Size(57, 17);
            this.importRadioButton.TabIndex = 122;
            this.importRadioButton.TabStop = true;
            this.importRadioButton.Text = "Import";
            this.importRadioButton.UseVisualStyleBackColor = false;
            this.importRadioButton.Visible = false;
            this.importRadioButton.CheckedChanged += new System.EventHandler(this.importRadioButton_Click);
            // 
            // exportRadioButton
            // 
            this.exportRadioButton.AutoSize = true;
            this.exportRadioButton.BackColor = System.Drawing.Color.Transparent;
            this.exportRadioButton.Font = new System.Drawing.Font("Tahoma", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.exportRadioButton.Location = new System.Drawing.Point(14, 10);
            this.exportRadioButton.Name = "exportRadioButton";
            this.exportRadioButton.Size = new System.Drawing.Size(57, 17);
            this.exportRadioButton.TabIndex = 123;
            this.exportRadioButton.TabStop = true;
            this.exportRadioButton.Text = "Export";
            this.exportRadioButton.UseVisualStyleBackColor = false;
            this.exportRadioButton.Visible = false;
            this.exportRadioButton.CheckedChanged += new System.EventHandler(this.exportRadioButton_Click);
            // 
            // offlineSyncTextLabel
            // 
            this.offlineSyncTextLabel.AutoSize = true;
            this.offlineSyncTextLabel.Location = new System.Drawing.Point(81, 359);
            this.offlineSyncTextLabel.Name = "offlineSyncTextLabel";
            this.offlineSyncTextLabel.Size = new System.Drawing.Size(435, 26);
            this.offlineSyncTextLabel.TabIndex = 133;
            this.offlineSyncTextLabel.Text = "Data from primary server can be first exported to a removable media which is then" +
                " imported \r\nback to secondary server. This option is used in case where WAN band" +
                "width is limited ";
            // 
            // haedingPanel
            // 
            this.haedingPanel.BackColor = System.Drawing.Color.Transparent;
            this.haedingPanel.BackgroundImage = global::com.InMage.Wizard.Properties.Resources.heading;
            this.haedingPanel.BackgroundImageLayout = System.Windows.Forms.ImageLayout.None;
            this.haedingPanel.Controls.Add(this.versionLabel);
            this.haedingPanel.Location = new System.Drawing.Point(0, 0);
            this.haedingPanel.Name = "haedingPanel";
            this.haedingPanel.Size = new System.Drawing.Size(848, 44);
            this.haedingPanel.TabIndex = 128;
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
            // MainForm
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.BackColor = System.Drawing.Color.White;
            this.BackgroundImageLayout = System.Windows.Forms.ImageLayout.Stretch;
            this.ClientSize = new System.Drawing.Size(850, 588);
            this.Controls.Add(this.mainFormPanel);
            this.Controls.Add(this.haedingPanel);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedSingle;
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.MaximizeBox = false;
            this.Name = "MainForm";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
            this.Text = "vContinuum";
            this.mainFormPanel.ResumeLayout(false);
            this.mainFormPanel.PerformLayout();
            this.protectGroupLabel.ResumeLayout(false);
            this.protectGroupLabel.PerformLayout();
            this.offlineSyncGroupPanel.ResumeLayout(false);
            this.offlineSyncGroupPanel.PerformLayout();
            this.haedingPanel.ResumeLayout(false);
            this.haedingPanel.PerformLayout();
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.Label applicationLabel;
        public System.Windows.Forms.ComboBox selectApplicationComboBox;
        public System.Windows.Forms.RadioButton importRadioButton;
        private System.Windows.Forms.RadioButton exportRadioButton;
        private System.Windows.Forms.LinkLabel protectLinkLabel;
        private System.Windows.Forms.LinkLabel recoverLinkLabel;
        private System.Windows.Forms.LinkLabel failBackLinkLabel;
        private System.Windows.Forms.LinkLabel offlineSyncLinkLabel;
        private BufferedPanel haedingPanel;
        private BufferedPanel mainFormPanel;
        private System.Windows.Forms.Label protectTextLabel;
        private System.Windows.Forms.Label failBackTextLabel;
        private System.Windows.Forms.Label recoverTextLabel;
        private System.Windows.Forms.Label offlineSyncTextLabel;
        private System.Windows.Forms.Label offlineSyncMoreLabel;
        private System.Windows.Forms.Label failbackMoreLabel;
        private System.Windows.Forms.Label recoverMoreLabel;
        private System.Windows.Forms.Label protectMoreLabel;
        private BufferedPanel offlineSyncGroupPanel;
        private System.Windows.Forms.LinkLabel removeLinkLabel;
        private System.Windows.Forms.Label removeMoreLabel;
        private System.Windows.Forms.Label removeTextLabel;
        private BufferedPanel protectGroupLabel;
        private System.Windows.Forms.RadioButton newPlanRadioButton;
        private System.Windows.Forms.RadioButton addDiskToExistingPlanRadioButton;
        private System.Windows.Forms.Label esxProlectionLabel;
        private System.Windows.Forms.Label forvSphereLabel;
        private System.Windows.Forms.Label tmLabel;
        private System.Windows.Forms.Label versionLabel;
        private System.Windows.Forms.LinkLabel monitorlinkLabel;
        private System.Windows.Forms.Label monitorDescriptionLabel;
        private System.Windows.Forms.LinkLabel pushAgentLinkLabel;
        private System.Windows.Forms.Label pushAgentTextLabel;
        private System.Windows.Forms.RadioButton resumeRadioButton;

    }
}