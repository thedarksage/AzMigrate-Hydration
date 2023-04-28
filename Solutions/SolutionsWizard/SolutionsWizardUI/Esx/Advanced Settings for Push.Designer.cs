namespace Com.Inmage.Wizard
{
    partial class AdvancedSettingsforPush
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
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(AdvancedSettingsforPush));
            this.advanceSettingsLabel = new System.Windows.Forms.Label();
            this.ipToPushLabel = new System.Windows.Forms.Label();
            this.ipToPushComboBox = new System.Windows.Forms.ComboBox();
            this.UseNatIPLabel = new System.Windows.Forms.Label();
            this.selectNATIPComboBox = new System.Windows.Forms.ComboBox();
            this.addButton = new System.Windows.Forms.Button();
            this.cancelButton = new System.Windows.Forms.Button();
            this.applicationCacheDirectoryLabel = new System.Windows.Forms.Label();
            this.cacheDirTextBox = new System.Windows.Forms.TextBox();
            this.headingPanel = new System.Windows.Forms.Panel();
            this.applyALLForLoggingCheckBox = new System.Windows.Forms.CheckBox();
            this.applyAllForCacheDirCheckBox = new System.Windows.Forms.CheckBox();
            this.doubleStarLabel = new System.Windows.Forms.Label();
            this.loggingGroupBox = new System.Windows.Forms.GroupBox();
            this.disableRadioButton = new System.Windows.Forms.RadioButton();
            this.enableRadioButton = new System.Windows.Forms.RadioButton();
            this.noteLabel = new System.Windows.Forms.Label();
            this.headingPanel.SuspendLayout();
            this.loggingGroupBox.SuspendLayout();
            this.SuspendLayout();
            // 
            // advanceSettingsLabel
            // 
            this.advanceSettingsLabel.AutoSize = true;
            this.advanceSettingsLabel.BackColor = System.Drawing.Color.Transparent;
            this.advanceSettingsLabel.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.advanceSettingsLabel.ForeColor = System.Drawing.Color.White;
            this.advanceSettingsLabel.Location = new System.Drawing.Point(10, 27);
            this.advanceSettingsLabel.Name = "advanceSettingsLabel";
            this.advanceSettingsLabel.Size = new System.Drawing.Size(206, 13);
            this.advanceSettingsLabel.TabIndex = 0;
            this.advanceSettingsLabel.Text = "Advance options to push agent on:";
            // 
            // ipToPushLabel
            // 
            this.ipToPushLabel.AutoSize = true;
            this.ipToPushLabel.Location = new System.Drawing.Point(23, 84);
            this.ipToPushLabel.Name = "ipToPushLabel";
            this.ipToPushLabel.Size = new System.Drawing.Size(183, 13);
            this.ipToPushLabel.TabIndex = 1;
            this.ipToPushLabel.Text = "IP Address to use for push installation";
            // 
            // ipToPushComboBox
            // 
            this.ipToPushComboBox.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.ipToPushComboBox.FormattingEnabled = true;
            this.ipToPushComboBox.Location = new System.Drawing.Point(297, 77);
            this.ipToPushComboBox.Name = "ipToPushComboBox";
            this.ipToPushComboBox.Size = new System.Drawing.Size(121, 21);
            this.ipToPushComboBox.TabIndex = 2;
            // 
            // UseNatIPLabel
            // 
            this.UseNatIPLabel.AutoSize = true;
            this.UseNatIPLabel.Location = new System.Drawing.Point(23, 141);
            this.UseNatIPLabel.Name = "UseNatIPLabel";
            this.UseNatIPLabel.Size = new System.Drawing.Size(253, 13);
            this.UseNatIPLabel.TabIndex = 3;
            this.UseNatIPLabel.Text = "Use this Agent\'s IP address to communicate with CS";
            // 
            // selectNATIPComboBox
            // 
            this.selectNATIPComboBox.FormattingEnabled = true;
            this.selectNATIPComboBox.Location = new System.Drawing.Point(297, 134);
            this.selectNATIPComboBox.Name = "selectNATIPComboBox";
            this.selectNATIPComboBox.Size = new System.Drawing.Size(121, 21);
            this.selectNATIPComboBox.TabIndex = 4;
            // 
            // addButton
            // 
            this.addButton.Location = new System.Drawing.Point(364, 377);
            this.addButton.Name = "addButton";
            this.addButton.Size = new System.Drawing.Size(75, 23);
            this.addButton.TabIndex = 5;
            this.addButton.Text = "Add";
            this.addButton.UseVisualStyleBackColor = true;
            this.addButton.Click += new System.EventHandler(this.addButton_Click);
            // 
            // cancelButton
            // 
            this.cancelButton.Location = new System.Drawing.Point(453, 377);
            this.cancelButton.Name = "cancelButton";
            this.cancelButton.Size = new System.Drawing.Size(75, 23);
            this.cancelButton.TabIndex = 6;
            this.cancelButton.Text = "Cancel";
            this.cancelButton.UseVisualStyleBackColor = true;
            this.cancelButton.Click += new System.EventHandler(this.cancelButton_Click);
            // 
            // applicationCacheDirectoryLabel
            // 
            this.applicationCacheDirectoryLabel.AutoSize = true;
            this.applicationCacheDirectoryLabel.Location = new System.Drawing.Point(23, 189);
            this.applicationCacheDirectoryLabel.Name = "applicationCacheDirectoryLabel";
            this.applicationCacheDirectoryLabel.Size = new System.Drawing.Size(138, 13);
            this.applicationCacheDirectoryLabel.TabIndex = 8;
            this.applicationCacheDirectoryLabel.Text = "Application Cache Directory";
            // 
            // cacheDirTextBox
            // 
            this.cacheDirTextBox.Location = new System.Drawing.Point(167, 186);
            this.cacheDirTextBox.Name = "cacheDirTextBox";
            this.cacheDirTextBox.Size = new System.Drawing.Size(230, 20);
            this.cacheDirTextBox.TabIndex = 9;
            // 
            // headingPanel
            // 
            this.headingPanel.BackColor = System.Drawing.Color.SteelBlue;
            this.headingPanel.Controls.Add(this.advanceSettingsLabel);
            this.headingPanel.Location = new System.Drawing.Point(-2, 1);
            this.headingPanel.Name = "headingPanel";
            this.headingPanel.Size = new System.Drawing.Size(576, 60);
            this.headingPanel.TabIndex = 10;
            // 
            // applyALLForLoggingCheckBox
            // 
            this.applyALLForLoggingCheckBox.AutoSize = true;
            this.applyALLForLoggingCheckBox.Location = new System.Drawing.Point(105, 40);
            this.applyALLForLoggingCheckBox.Name = "applyALLForLoggingCheckBox";
            this.applyALLForLoggingCheckBox.Size = new System.Drawing.Size(171, 17);
            this.applyALLForLoggingCheckBox.TabIndex = 11;
            this.applyALLForLoggingCheckBox.Text = "Apply for all selected machines";
            this.applyALLForLoggingCheckBox.UseVisualStyleBackColor = true;
            // 
            // applyAllForCacheDirCheckBox
            // 
            this.applyAllForCacheDirCheckBox.AutoSize = true;
            this.applyAllForCacheDirCheckBox.Location = new System.Drawing.Point(402, 189);
            this.applyAllForCacheDirCheckBox.Name = "applyAllForCacheDirCheckBox";
            this.applyAllForCacheDirCheckBox.Size = new System.Drawing.Size(171, 17);
            this.applyAllForCacheDirCheckBox.TabIndex = 12;
            this.applyAllForCacheDirCheckBox.Text = "Apply for all selected machines";
            this.applyAllForCacheDirCheckBox.UseVisualStyleBackColor = true;
            // 
            // doubleStarLabel
            // 
            this.doubleStarLabel.AutoSize = true;
            this.doubleStarLabel.ForeColor = System.Drawing.Color.Red;
            this.doubleStarLabel.Location = new System.Drawing.Point(424, 145);
            this.doubleStarLabel.Name = "doubleStarLabel";
            this.doubleStarLabel.Size = new System.Drawing.Size(11, 13);
            this.doubleStarLabel.TabIndex = 14;
            this.doubleStarLabel.Text = "*";
            // 
            // loggingGroupBox
            // 
            this.loggingGroupBox.Controls.Add(this.disableRadioButton);
            this.loggingGroupBox.Controls.Add(this.enableRadioButton);
            this.loggingGroupBox.Controls.Add(this.applyALLForLoggingCheckBox);
            this.loggingGroupBox.Location = new System.Drawing.Point(25, 247);
            this.loggingGroupBox.Name = "loggingGroupBox";
            this.loggingGroupBox.Size = new System.Drawing.Size(328, 85);
            this.loggingGroupBox.TabIndex = 15;
            this.loggingGroupBox.TabStop = false;
            this.loggingGroupBox.Text = "Logging";
            // 
            // disableRadioButton
            // 
            this.disableRadioButton.AutoSize = true;
            this.disableRadioButton.Checked = true;
            this.disableRadioButton.Location = new System.Drawing.Point(6, 52);
            this.disableRadioButton.Name = "disableRadioButton";
            this.disableRadioButton.Size = new System.Drawing.Size(60, 17);
            this.disableRadioButton.TabIndex = 13;
            this.disableRadioButton.TabStop = true;
            this.disableRadioButton.Text = "Disable";
            this.disableRadioButton.UseVisualStyleBackColor = true;
            // 
            // enableRadioButton
            // 
            this.enableRadioButton.AutoSize = true;
            this.enableRadioButton.Location = new System.Drawing.Point(6, 19);
            this.enableRadioButton.Name = "enableRadioButton";
            this.enableRadioButton.Size = new System.Drawing.Size(58, 17);
            this.enableRadioButton.TabIndex = 12;
            this.enableRadioButton.Text = "Enable";
            this.enableRadioButton.UseVisualStyleBackColor = true;
            // 
            // noteLabel
            // 
            this.noteLabel.AutoSize = true;
            this.noteLabel.ForeColor = System.Drawing.Color.Red;
            this.noteLabel.Location = new System.Drawing.Point(23, 355);
            this.noteLabel.Name = "noteLabel";
            this.noteLabel.Size = new System.Drawing.Size(368, 13);
            this.noteLabel.TabIndex = 18;
            this.noteLabel.Text = "Note: * Select IP address from Drop down list or you can enter public NAT IP";
            // 
            // AdvancedSettingsforPush
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.BackColor = System.Drawing.Color.White;
            this.ClientSize = new System.Drawing.Size(577, 411);
            this.ControlBox = false;
            this.Controls.Add(this.noteLabel);
            this.Controls.Add(this.loggingGroupBox);
            this.Controls.Add(this.doubleStarLabel);
            this.Controls.Add(this.applyAllForCacheDirCheckBox);
            this.Controls.Add(this.headingPanel);
            this.Controls.Add(this.cacheDirTextBox);
            this.Controls.Add(this.applicationCacheDirectoryLabel);
            this.Controls.Add(this.cancelButton);
            this.Controls.Add(this.addButton);
            this.Controls.Add(this.selectNATIPComboBox);
            this.Controls.Add(this.UseNatIPLabel);
            this.Controls.Add(this.ipToPushComboBox);
            this.Controls.Add(this.ipToPushLabel);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedSingle;
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.Name = "AdvancedSettingsforPush";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
            this.Text = "Advanced Settings for Push";
            this.headingPanel.ResumeLayout(false);
            this.headingPanel.PerformLayout();
            this.loggingGroupBox.ResumeLayout(false);
            this.loggingGroupBox.PerformLayout();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Label advanceSettingsLabel;
        private System.Windows.Forms.Label ipToPushLabel;
        private System.Windows.Forms.Label UseNatIPLabel;
        private System.Windows.Forms.ComboBox selectNATIPComboBox;
        private System.Windows.Forms.Button addButton;
        private System.Windows.Forms.Button cancelButton;
        private System.Windows.Forms.Label applicationCacheDirectoryLabel;
        private System.Windows.Forms.Panel headingPanel;
        private System.Windows.Forms.Label doubleStarLabel;
        private System.Windows.Forms.GroupBox loggingGroupBox;
        private System.Windows.Forms.Label noteLabel;
        internal System.Windows.Forms.ComboBox ipToPushComboBox;
        internal System.Windows.Forms.TextBox cacheDirTextBox;
        internal System.Windows.Forms.CheckBox applyALLForLoggingCheckBox;
        internal System.Windows.Forms.CheckBox applyAllForCacheDirCheckBox;
        internal System.Windows.Forms.RadioButton enableRadioButton;
        internal System.Windows.Forms.RadioButton disableRadioButton;
    }
}