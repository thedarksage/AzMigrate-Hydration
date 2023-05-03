namespace com.InMage.Wizard
{
    partial class ConfigurationScreen
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

        #region Component Designer generated code

        /// <summary> 
        /// Required method for Designer support - do not modify 
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this.sourceConfigurationTreeView = new System.Windows.Forms.TreeView();
            this.netWorkConfiurationGroupBox = new System.Windows.Forms.GroupBox();
            this.networkDataGridView = new System.Windows.Forms.DataGridView();
            this.nicName = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.keepoldColumn = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.buttonColumn = new System.Windows.Forms.DataGridViewButtonColumn();
            this.hardWareGroupBox = new System.Windows.Forms.GroupBox();
            this.hardWareSettingsCheckBox = new System.Windows.Forms.CheckBox();
            this.memoryNumericUpDown = new System.Windows.Forms.NumericUpDown();
            this.memoryValuesComboBox = new System.Windows.Forms.ComboBox();
            this.memoryLabel = new System.Windows.Forms.Label();
            this.cpuComboBox = new System.Windows.Forms.ComboBox();
            this.cpuLabel = new System.Windows.Forms.Label();
            this.netWorkConfiurationGroupBox.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.networkDataGridView)).BeginInit();
            this.hardWareGroupBox.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.memoryNumericUpDown)).BeginInit();
            this.SuspendLayout();
            // 
            // sourceConfigurationTreeView
            // 
            this.sourceConfigurationTreeView.BackColor = System.Drawing.Color.White;
            this.sourceConfigurationTreeView.HideSelection = false;
            this.sourceConfigurationTreeView.Location = new System.Drawing.Point(3, 9);
            this.sourceConfigurationTreeView.Name = "sourceConfigurationTreeView";
            this.sourceConfigurationTreeView.Size = new System.Drawing.Size(167, 443);
            this.sourceConfigurationTreeView.TabIndex = 0;
            this.sourceConfigurationTreeView.AfterSelect += new System.Windows.Forms.TreeViewEventHandler(this.sourceConfigurationTreeView_AfterSelect);
            // 
            // netWorkConfiurationGroupBox
            // 
            this.netWorkConfiurationGroupBox.Controls.Add(this.networkDataGridView);
            this.netWorkConfiurationGroupBox.Location = new System.Drawing.Point(185, 9);
            this.netWorkConfiurationGroupBox.Name = "netWorkConfiurationGroupBox";
            this.netWorkConfiurationGroupBox.Size = new System.Drawing.Size(424, 170);
            this.netWorkConfiurationGroupBox.TabIndex = 1;
            this.netWorkConfiurationGroupBox.TabStop = false;
            this.netWorkConfiurationGroupBox.Text = "Network Configuration";
            // 
            // networkDataGridView
            // 
            this.networkDataGridView.AllowUserToAddRows = false;
            this.networkDataGridView.BackgroundColor = System.Drawing.Color.White;
            this.networkDataGridView.BorderStyle = System.Windows.Forms.BorderStyle.None;
            this.networkDataGridView.ColumnHeadersHeightSizeMode = System.Windows.Forms.DataGridViewColumnHeadersHeightSizeMode.AutoSize;
            this.networkDataGridView.Columns.AddRange(new System.Windows.Forms.DataGridViewColumn[] {
            this.nicName,
            this.keepoldColumn,
            this.buttonColumn});
            this.networkDataGridView.Location = new System.Drawing.Point(6, 22);
            this.networkDataGridView.Name = "networkDataGridView";
            this.networkDataGridView.RowHeadersVisible = false;
            this.networkDataGridView.Size = new System.Drawing.Size(412, 139);
            this.networkDataGridView.TabIndex = 0;
            this.networkDataGridView.CellClick += new System.Windows.Forms.DataGridViewCellEventHandler(this.networkDataGridView_CellClick);
            // 
            // nicName
            // 
            this.nicName.HeaderText = "Nic ";
            this.nicName.Name = "nicName";
            this.nicName.ReadOnly = true;
            // 
            // keepoldColumn
            // 
            this.keepoldColumn.HeaderText = "IP";
            this.keepoldColumn.Name = "keepoldColumn";
            this.keepoldColumn.ReadOnly = true;
            this.keepoldColumn.Resizable = System.Windows.Forms.DataGridViewTriState.True;
            this.keepoldColumn.SortMode = System.Windows.Forms.DataGridViewColumnSortMode.NotSortable;
            this.keepoldColumn.Width = 150;
            // 
            // buttonColumn
            // 
            this.buttonColumn.FlatStyle = System.Windows.Forms.FlatStyle.System;
            this.buttonColumn.HeaderText = "Static";
            this.buttonColumn.Name = "buttonColumn";
            this.buttonColumn.Width = 75;
            // 
            // hardWareGroupBox
            // 
            this.hardWareGroupBox.Controls.Add(this.hardWareSettingsCheckBox);
            this.hardWareGroupBox.Controls.Add(this.memoryNumericUpDown);
            this.hardWareGroupBox.Controls.Add(this.memoryValuesComboBox);
            this.hardWareGroupBox.Controls.Add(this.memoryLabel);
            this.hardWareGroupBox.Controls.Add(this.cpuComboBox);
            this.hardWareGroupBox.Controls.Add(this.cpuLabel);
            this.hardWareGroupBox.Location = new System.Drawing.Point(185, 198);
            this.hardWareGroupBox.Name = "hardWareGroupBox";
            this.hardWareGroupBox.Size = new System.Drawing.Size(205, 126);
            this.hardWareGroupBox.TabIndex = 3;
            this.hardWareGroupBox.TabStop = false;
            this.hardWareGroupBox.Text = "Hardware Configuration";
            // 
            // hardWareSettingsCheckBox
            // 
            this.hardWareSettingsCheckBox.AutoSize = true;
            this.hardWareSettingsCheckBox.Enabled = false;
            this.hardWareSettingsCheckBox.Location = new System.Drawing.Point(23, 92);
            this.hardWareSettingsCheckBox.Name = "hardWareSettingsCheckBox";
            this.hardWareSettingsCheckBox.Size = new System.Drawing.Size(160, 17);
            this.hardWareSettingsCheckBox.TabIndex = 6;
            this.hardWareSettingsCheckBox.Text = "Use these values for all VMs";
            this.hardWareSettingsCheckBox.UseVisualStyleBackColor = true;
            this.hardWareSettingsCheckBox.CheckedChanged += new System.EventHandler(this.hardWareSettingsCheckBox_CheckedChanged);
            // 
            // memoryNumericUpDown
            // 
            this.memoryNumericUpDown.Enabled = false;
            this.memoryNumericUpDown.Location = new System.Drawing.Point(78, 53);
            this.memoryNumericUpDown.Name = "memoryNumericUpDown";
            this.memoryNumericUpDown.Size = new System.Drawing.Size(58, 20);
            this.memoryNumericUpDown.TabIndex = 7;
            this.memoryNumericUpDown.ValueChanged += new System.EventHandler(this.memoryNumericUpDown_ValueChanged);
            // 
            // memoryValuesComboBox
            // 
            this.memoryValuesComboBox.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.memoryValuesComboBox.Enabled = false;
            this.memoryValuesComboBox.FormattingEnabled = true;
            this.memoryValuesComboBox.Location = new System.Drawing.Point(149, 53);
            this.memoryValuesComboBox.Name = "memoryValuesComboBox";
            this.memoryValuesComboBox.Size = new System.Drawing.Size(40, 21);
            this.memoryValuesComboBox.TabIndex = 4;
            // 
            // memoryLabel
            // 
            this.memoryLabel.AutoSize = true;
            this.memoryLabel.Location = new System.Drawing.Point(20, 60);
            this.memoryLabel.Name = "memoryLabel";
            this.memoryLabel.Size = new System.Drawing.Size(44, 13);
            this.memoryLabel.TabIndex = 2;
            this.memoryLabel.Text = "Memory";
            // 
            // cpuComboBox
            // 
            this.cpuComboBox.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.cpuComboBox.Enabled = false;
            this.cpuComboBox.FormattingEnabled = true;
            this.cpuComboBox.Location = new System.Drawing.Point(78, 24);
            this.cpuComboBox.Name = "cpuComboBox";
            this.cpuComboBox.Size = new System.Drawing.Size(58, 21);
            this.cpuComboBox.TabIndex = 1;
            this.cpuComboBox.SelectionChangeCommitted += new System.EventHandler(this.cpuComboBox_SelectionChangeCommitted);
            // 
            // cpuLabel
            // 
            this.cpuLabel.AutoSize = true;
            this.cpuLabel.Location = new System.Drawing.Point(20, 31);
            this.cpuLabel.Name = "cpuLabel";
            this.cpuLabel.Size = new System.Drawing.Size(34, 13);
            this.cpuLabel.TabIndex = 0;
            this.cpuLabel.Text = "CPUs";
            // 
            // ConfigurationScreen
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.BackColor = System.Drawing.Color.Transparent;
            this.BackgroundImage = global::com.InMage.Wizard.Properties.Resources.workarea;
            this.BackgroundImageLayout = System.Windows.Forms.ImageLayout.None;
            this.Controls.Add(this.hardWareGroupBox);
            this.Controls.Add(this.netWorkConfiurationGroupBox);
            this.Controls.Add(this.sourceConfigurationTreeView);
            this.Name = "ConfigurationScreen";
            this.Size = new System.Drawing.Size(677, 466);
            this.netWorkConfiurationGroupBox.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.networkDataGridView)).EndInit();
            this.hardWareGroupBox.ResumeLayout(false);
            this.hardWareGroupBox.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this.memoryNumericUpDown)).EndInit();
            this.ResumeLayout(false);

        }

        #endregion

        public System.Windows.Forms.TreeView sourceConfigurationTreeView;
        public System.Windows.Forms.GroupBox netWorkConfiurationGroupBox;
        public System.Windows.Forms.GroupBox hardWareGroupBox;
        public System.Windows.Forms.Label memoryLabel;
        public System.Windows.Forms.ComboBox cpuComboBox;
        private System.Windows.Forms.Label cpuLabel;
        public System.Windows.Forms.DataGridView networkDataGridView;
        public System.Windows.Forms.ComboBox memoryValuesComboBox;
        public System.Windows.Forms.NumericUpDown memoryNumericUpDown;
        public System.Windows.Forms.CheckBox hardWareSettingsCheckBox;
        private System.Windows.Forms.DataGridViewTextBoxColumn nicName;
        private System.Windows.Forms.DataGridViewTextBoxColumn keepoldColumn;
        private System.Windows.Forms.DataGridViewButtonColumn buttonColumn;

    }
}
