namespace Com.Inmage.Wizard
{
    partial class RemoveDuplicateEntries
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
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(RemoveDuplicateEntries));
            this.headingPanel = new System.Windows.Forms.Panel();
            this.headingLabel = new System.Windows.Forms.Label();
            this.cancelButton = new System.Windows.Forms.Button();
            this.removeDuplicateEntriesDataGridView = new System.Windows.Forms.DataGridView();
            this.noteLabel = new System.Windows.Forms.Label();
            this.backgroundWorker = new System.ComponentModel.BackgroundWorker();
            this.progressBar = new System.Windows.Forms.ProgressBar();
            this.hostname = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.ipAddress = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.ipaddresses = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.inmageuuid = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.agentReportedTime = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.removeLink = new System.Windows.Forms.DataGridViewLinkColumn();
            this.headingPanel.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.removeDuplicateEntriesDataGridView)).BeginInit();
            this.SuspendLayout();
            // 
            // headingPanel
            // 
            this.headingPanel.BackColor = System.Drawing.Color.SteelBlue;
            this.headingPanel.Controls.Add(this.headingLabel);
            this.headingPanel.Location = new System.Drawing.Point(-2, 0);
            this.headingPanel.Name = "headingPanel";
            this.headingPanel.Size = new System.Drawing.Size(786, 48);
            this.headingPanel.TabIndex = 0;
            // 
            // headingLabel
            // 
            this.headingLabel.AutoSize = true;
            this.headingLabel.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.headingLabel.Location = new System.Drawing.Point(11, 17);
            this.headingLabel.Name = "headingLabel";
            this.headingLabel.Size = new System.Drawing.Size(561, 13);
            this.headingLabel.TabIndex = 1;
            this.headingLabel.Text = "Following machine(s) have duplicate entries. Please click on remove link to delet" +
    "e duplicate entry";
            // 
            // cancelButton
            // 
            this.cancelButton.Location = new System.Drawing.Point(645, 310);
            this.cancelButton.Name = "cancelButton";
            this.cancelButton.Size = new System.Drawing.Size(75, 23);
            this.cancelButton.TabIndex = 1;
            this.cancelButton.Text = "Cancel";
            this.cancelButton.UseVisualStyleBackColor = true;
            this.cancelButton.Click += new System.EventHandler(this.cancelButton_Click);
            // 
            // removeDuplicateEntriesDataGridView
            // 
            this.removeDuplicateEntriesDataGridView.AllowUserToAddRows = false;
            this.removeDuplicateEntriesDataGridView.AllowUserToDeleteRows = false;
            this.removeDuplicateEntriesDataGridView.BackgroundColor = System.Drawing.Color.White;
            this.removeDuplicateEntriesDataGridView.ColumnHeadersHeightSizeMode = System.Windows.Forms.DataGridViewColumnHeadersHeightSizeMode.AutoSize;
            this.removeDuplicateEntriesDataGridView.Columns.AddRange(new System.Windows.Forms.DataGridViewColumn[] {
            this.hostname,
            this.ipAddress,
            this.ipaddresses,
            this.inmageuuid,
            this.agentReportedTime,
            this.removeLink});
            this.removeDuplicateEntriesDataGridView.Location = new System.Drawing.Point(12, 54);
            this.removeDuplicateEntriesDataGridView.Name = "removeDuplicateEntriesDataGridView";
            this.removeDuplicateEntriesDataGridView.RowHeadersVisible = false;
            this.removeDuplicateEntriesDataGridView.Size = new System.Drawing.Size(708, 180);
            this.removeDuplicateEntriesDataGridView.TabIndex = 2;
            this.removeDuplicateEntriesDataGridView.CellContentClick += new System.Windows.Forms.DataGridViewCellEventHandler(this.removeDuplicateEntriesDataGridView_CellContentClick);
            // 
            // noteLabel
            // 
            this.noteLabel.AutoSize = true;
            this.noteLabel.Location = new System.Drawing.Point(12, 249);
            this.noteLabel.Name = "noteLabel";
            this.noteLabel.Size = new System.Drawing.Size(605, 26);
            this.noteLabel.TabIndex = 3;
            this.noteLabel.Text = "NOTE: 1. In case of failback from ASR to on-premise, make sure that you are remov" +
    "ing the entry of on-premise Virtual Machine.\r\n            2. Make sure that on-p" +
    "remise Virtual Machines are shutdown.\r\n";
            // 
            // backgroundWorker
            // 
            this.backgroundWorker.DoWork += new System.ComponentModel.DoWorkEventHandler(this.backgroundWorker_DoWork);
            this.backgroundWorker.ProgressChanged += new System.ComponentModel.ProgressChangedEventHandler(this.backgroundWorker_ProgressChanged);
            this.backgroundWorker.RunWorkerCompleted += new System.ComponentModel.RunWorkerCompletedEventHandler(this.backgroundWorker_RunWorkerCompleted);
            // 
            // progressBar
            // 
            this.progressBar.BackColor = System.Drawing.Color.LightGray;
            this.progressBar.ForeColor = System.Drawing.Color.ForestGreen;
            this.progressBar.Location = new System.Drawing.Point(-2, 291);
            this.progressBar.Name = "progressBar";
            this.progressBar.Size = new System.Drawing.Size(722, 13);
            this.progressBar.Style = System.Windows.Forms.ProgressBarStyle.Marquee;
            this.progressBar.TabIndex = 17;
            this.progressBar.Visible = false;
            // 
            // hostname
            // 
            this.hostname.HeaderText = "Hostname";
            this.hostname.Name = "hostname";
            this.hostname.ReadOnly = true;
            this.hostname.Width = 85;
            // 
            // ipAddress
            // 
            this.ipAddress.HeaderText = "IP";
            this.ipAddress.Name = "ipAddress";
            this.ipAddress.ReadOnly = true;
            this.ipAddress.Width = 80;
            // 
            // ipaddresses
            // 
            this.ipaddresses.HeaderText = "IP addresses";
            this.ipaddresses.Name = "ipaddresses";
            this.ipaddresses.ReadOnly = true;
            this.ipaddresses.Width = 110;
            // 
            // inmageuuid
            // 
            this.inmageuuid.HeaderText = "Host ID";
            this.inmageuuid.Name = "inmageuuid";
            this.inmageuuid.ReadOnly = true;
            this.inmageuuid.Width = 235;
            // 
            // agentReportedTime
            // 
            this.agentReportedTime.HeaderText = "First reported time";
            this.agentReportedTime.Name = "agentReportedTime";
            this.agentReportedTime.ReadOnly = true;
            this.agentReportedTime.Width = 125;
            // 
            // removeLink
            // 
            this.removeLink.HeaderText = "Remove ";
            this.removeLink.Name = "removeLink";
            this.removeLink.Width = 70;
            // 
            // RemoveDuplicateEntries
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.BackColor = System.Drawing.Color.White;
            this.BackgroundImage = global::Com.Inmage.Wizard.Properties.Resources.workarea;
            this.BackgroundImageLayout = System.Windows.Forms.ImageLayout.Stretch;
            this.ClientSize = new System.Drawing.Size(723, 345);
            this.ControlBox = false;
            this.Controls.Add(this.progressBar);
            this.Controls.Add(this.noteLabel);
            this.Controls.Add(this.removeDuplicateEntriesDataGridView);
            this.Controls.Add(this.cancelButton);
            this.Controls.Add(this.headingPanel);
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.Name = "RemoveDuplicateEntries";
            this.RightToLeft = System.Windows.Forms.RightToLeft.No;
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
            this.Text = "Remove duplicate entries";
            this.headingPanel.ResumeLayout(false);
            this.headingPanel.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this.removeDuplicateEntriesDataGridView)).EndInit();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Panel headingPanel;
        private System.Windows.Forms.Label headingLabel;
        private System.Windows.Forms.Button cancelButton;
        private System.Windows.Forms.DataGridView removeDuplicateEntriesDataGridView;
        private System.Windows.Forms.Label noteLabel;
        private System.ComponentModel.BackgroundWorker backgroundWorker;
        public System.Windows.Forms.ProgressBar progressBar;
        private System.Windows.Forms.DataGridViewTextBoxColumn hostname;
        private System.Windows.Forms.DataGridViewTextBoxColumn ipAddress;
        private System.Windows.Forms.DataGridViewTextBoxColumn ipaddresses;
        private System.Windows.Forms.DataGridViewTextBoxColumn inmageuuid;
        private System.Windows.Forms.DataGridViewTextBoxColumn agentReportedTime;
        private System.Windows.Forms.DataGridViewLinkColumn removeLink;
    }
}