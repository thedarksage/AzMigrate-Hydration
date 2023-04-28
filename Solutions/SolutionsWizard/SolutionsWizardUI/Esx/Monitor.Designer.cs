namespace Com.Inmage.Wizard
{
    partial class Monitor
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
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(Monitor));
            this.planNamesTreeView = new System.Windows.Forms.TreeView();
            this.closeButton = new System.Windows.Forms.Button();
            this.backgroundWorker = new System.ComponentModel.BackgroundWorker();
            this.statusDataGridView = new System.Windows.Forms.DataGridView();
            this.statusColumn = new System.Windows.Forms.DataGridViewImageColumn();
            this.taskColumn = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.statusLabel = new System.Windows.Forms.Label();
            this.logLinkLabel = new System.Windows.Forms.LinkLabel();
            this.startButton = new System.Windows.Forms.Button();
            this.haedingPanel = new Com.Inmage.Wizard.BufferedPanel();
            this.headingLanel = new System.Windows.Forms.Label();
            ((System.ComponentModel.ISupportInitialize)(this.statusDataGridView)).BeginInit();
            this.haedingPanel.SuspendLayout();
            this.SuspendLayout();
            // 
            // planNamesTreeView
            // 
            this.planNamesTreeView.BackColor = System.Drawing.Color.White;
            this.planNamesTreeView.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
            this.planNamesTreeView.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.planNamesTreeView.Location = new System.Drawing.Point(6, 51);
            this.planNamesTreeView.Name = "planNamesTreeView";
            this.planNamesTreeView.Size = new System.Drawing.Size(171, 334);
            this.planNamesTreeView.TabIndex = 131;
            this.planNamesTreeView.AfterSelect += new System.Windows.Forms.TreeViewEventHandler(this.planNamesTreeView_AfterSelect);
            // 
            // closeButton
            // 
            this.closeButton.Location = new System.Drawing.Point(508, 391);
            this.closeButton.Name = "closeButton";
            this.closeButton.Size = new System.Drawing.Size(75, 23);
            this.closeButton.TabIndex = 132;
            this.closeButton.Text = "Close";
            this.closeButton.UseVisualStyleBackColor = true;
            this.closeButton.Click += new System.EventHandler(this.closeButton_Click);
            // 
            // backgroundWorker
            // 
            this.backgroundWorker.WorkerReportsProgress = true;
            this.backgroundWorker.WorkerSupportsCancellation = true;
            this.backgroundWorker.DoWork += new System.ComponentModel.DoWorkEventHandler(this.backgroundWorker_DoWork);
            this.backgroundWorker.RunWorkerCompleted += new System.ComponentModel.RunWorkerCompletedEventHandler(this.backgroundWorker_RunWorkerCompleted);
            this.backgroundWorker.ProgressChanged += new System.ComponentModel.ProgressChangedEventHandler(this.backgroundWorker_ProgressChanged);
            // 
            // statusDataGridView
            // 
            this.statusDataGridView.AllowUserToAddRows = false;
            this.statusDataGridView.AllowUserToResizeColumns = false;
            this.statusDataGridView.AllowUserToResizeRows = false;
            this.statusDataGridView.BackgroundColor = System.Drawing.Color.White;
            this.statusDataGridView.BorderStyle = System.Windows.Forms.BorderStyle.None;
            this.statusDataGridView.CellBorderStyle = System.Windows.Forms.DataGridViewCellBorderStyle.None;
            dataGridViewCellStyle1.Alignment = System.Windows.Forms.DataGridViewContentAlignment.MiddleLeft;
            dataGridViewCellStyle1.BackColor = System.Drawing.SystemColors.Control;
            dataGridViewCellStyle1.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            dataGridViewCellStyle1.ForeColor = System.Drawing.SystemColors.WindowText;
            dataGridViewCellStyle1.SelectionBackColor = System.Drawing.SystemColors.Highlight;
            dataGridViewCellStyle1.SelectionForeColor = System.Drawing.SystemColors.HighlightText;
            dataGridViewCellStyle1.WrapMode = System.Windows.Forms.DataGridViewTriState.True;
            this.statusDataGridView.ColumnHeadersDefaultCellStyle = dataGridViewCellStyle1;
            this.statusDataGridView.ColumnHeadersHeightSizeMode = System.Windows.Forms.DataGridViewColumnHeadersHeightSizeMode.AutoSize;
            this.statusDataGridView.ColumnHeadersVisible = false;
            this.statusDataGridView.Columns.AddRange(new System.Windows.Forms.DataGridViewColumn[] {
            this.statusColumn,
            this.taskColumn});
            dataGridViewCellStyle2.Alignment = System.Windows.Forms.DataGridViewContentAlignment.MiddleLeft;
            dataGridViewCellStyle2.BackColor = System.Drawing.SystemColors.Window;
            dataGridViewCellStyle2.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            dataGridViewCellStyle2.ForeColor = System.Drawing.SystemColors.ControlText;
            dataGridViewCellStyle2.SelectionBackColor = System.Drawing.Color.White;
            dataGridViewCellStyle2.SelectionForeColor = System.Drawing.Color.Black;
            dataGridViewCellStyle2.WrapMode = System.Windows.Forms.DataGridViewTriState.False;
            this.statusDataGridView.DefaultCellStyle = dataGridViewCellStyle2;
            this.statusDataGridView.EditMode = System.Windows.Forms.DataGridViewEditMode.EditProgrammatically;
            this.statusDataGridView.Location = new System.Drawing.Point(197, 103);
            this.statusDataGridView.Name = "statusDataGridView";
            this.statusDataGridView.ReadOnly = true;
            dataGridViewCellStyle3.Alignment = System.Windows.Forms.DataGridViewContentAlignment.MiddleLeft;
            dataGridViewCellStyle3.BackColor = System.Drawing.SystemColors.Control;
            dataGridViewCellStyle3.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            dataGridViewCellStyle3.ForeColor = System.Drawing.SystemColors.WindowText;
            dataGridViewCellStyle3.SelectionBackColor = System.Drawing.SystemColors.Highlight;
            dataGridViewCellStyle3.SelectionForeColor = System.Drawing.SystemColors.HighlightText;
            dataGridViewCellStyle3.WrapMode = System.Windows.Forms.DataGridViewTriState.True;
            this.statusDataGridView.RowHeadersDefaultCellStyle = dataGridViewCellStyle3;
            this.statusDataGridView.RowHeadersVisible = false;
            this.statusDataGridView.Size = new System.Drawing.Size(309, 222);
            this.statusDataGridView.TabIndex = 133;
            this.statusDataGridView.CellPainting += new System.Windows.Forms.DataGridViewCellPaintingEventHandler(this.statusDataGridView_CellPainting);
            // 
            // statusColumn
            // 
            this.statusColumn.HeaderText = "Status";
            this.statusColumn.Name = "statusColumn";
            this.statusColumn.ReadOnly = true;
            this.statusColumn.Width = 50;
            // 
            // taskColumn
            // 
            this.taskColumn.HeaderText = "Task";
            this.taskColumn.Name = "taskColumn";
            this.taskColumn.ReadOnly = true;
            this.taskColumn.Width = 243;
            // 
            // statusLabel
            // 
            this.statusLabel.AutoSize = true;
            this.statusLabel.Font = new System.Drawing.Font("Arial", 9.75F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.statusLabel.Location = new System.Drawing.Point(194, 62);
            this.statusLabel.Name = "statusLabel";
            this.statusLabel.Size = new System.Drawing.Size(173, 16);
            this.statusLabel.TabIndex = 134;
            this.statusLabel.Text = "Protection Status for plan:";
            this.statusLabel.Visible = false;
            // 
            // logLinkLabel
            // 
            this.logLinkLabel.AutoSize = true;
            this.logLinkLabel.Location = new System.Drawing.Point(267, 396);
            this.logLinkLabel.Name = "logLinkLabel";
            this.logLinkLabel.Size = new System.Drawing.Size(51, 13);
            this.logLinkLabel.TabIndex = 135;
            this.logLinkLabel.TabStop = true;
            this.logLinkLabel.Text = "View Log";
            this.logLinkLabel.Visible = false;
            this.logLinkLabel.LinkClicked += new System.Windows.Forms.LinkLabelLinkClickedEventHandler(this.logLinkLabel_LinkClicked);
            // 
            // startButton
            // 
            this.startButton.Location = new System.Drawing.Point(427, 391);
            this.startButton.Name = "startButton";
            this.startButton.Size = new System.Drawing.Size(75, 23);
            this.startButton.TabIndex = 137;
            this.startButton.Text = "Start";
            this.startButton.UseVisualStyleBackColor = true;
            this.startButton.Visible = false;
            this.startButton.Click += new System.EventHandler(this.startButton_Click);
            // 
            // haedingPanel
            // 
            this.haedingPanel.BackColor = System.Drawing.Color.SteelBlue;
            this.haedingPanel.BackgroundImageLayout = System.Windows.Forms.ImageLayout.None;
            this.haedingPanel.Controls.Add(this.headingLanel);
            this.haedingPanel.Location = new System.Drawing.Point(0, 0);
            this.haedingPanel.Name = "haedingPanel";
            this.haedingPanel.Size = new System.Drawing.Size(632, 44);
            this.haedingPanel.TabIndex = 130;
            // 
            // headingLanel
            // 
            this.headingLanel.AutoSize = true;
            this.headingLanel.Font = new System.Drawing.Font("Arial Rounded MT Bold", 11.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.headingLanel.ForeColor = System.Drawing.Color.White;
            this.headingLanel.Location = new System.Drawing.Point(12, 18);
            this.headingLanel.Name = "headingLanel";
            this.headingLanel.Size = new System.Drawing.Size(63, 17);
            this.headingLanel.TabIndex = 0;
            this.headingLanel.Text = "Monitor";
            // 
            // Monitor
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.BackColor = System.Drawing.Color.White;
            this.ClientSize = new System.Drawing.Size(631, 429);
            this.Controls.Add(this.startButton);
            this.Controls.Add(this.logLinkLabel);
            this.Controls.Add(this.statusLabel);
            this.Controls.Add(this.statusDataGridView);
            this.Controls.Add(this.closeButton);
            this.Controls.Add(this.planNamesTreeView);
            this.Controls.Add(this.haedingPanel);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedSingle;
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.MaximizeBox = false;
            this.Name = "Monitor";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
            this.Text = "Monitor";
            ((System.ComponentModel.ISupportInitialize)(this.statusDataGridView)).EndInit();
            this.haedingPanel.ResumeLayout(false);
            this.haedingPanel.PerformLayout();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private BufferedPanel haedingPanel;
        private System.Windows.Forms.TreeView planNamesTreeView;
        private System.Windows.Forms.Button closeButton;
        private System.ComponentModel.BackgroundWorker backgroundWorker;
        private System.Windows.Forms.DataGridView statusDataGridView;
        private System.Windows.Forms.DataGridViewImageColumn statusColumn;
        private System.Windows.Forms.DataGridViewTextBoxColumn taskColumn;
        private System.Windows.Forms.Label headingLanel;
        private System.Windows.Forms.Label statusLabel;
        private System.Windows.Forms.LinkLabel logLinkLabel;
        private System.Windows.Forms.Button startButton;
    }
}