namespace Com.Inmage.Wizard
{
    partial class StatusForm
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
            System.Windows.Forms.DataGridViewCellStyle dataGridViewCellStyle2 = new System.Windows.Forms.DataGridViewCellStyle();
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(StatusForm));
            this.statusDataGridView = new System.Windows.Forms.DataGridView();
            this.statusColumn = new System.Windows.Forms.DataGridViewImageColumn();
            this.taskColumn = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.cancelButton = new System.Windows.Forms.Button();
            this.backgroundWorker = new System.ComponentModel.BackgroundWorker();
            this.protectButton = new System.Windows.Forms.Button();
            this.statusLabel = new System.Windows.Forms.Label();
            this.logLinkLabel = new System.Windows.Forms.LinkLabel();
            this.warningLabel = new System.Windows.Forms.Label();
            this.headingPanelpanel = new System.Windows.Forms.Panel();
            this.recoveryLaterLabel = new System.Windows.Forms.Label();
            ((System.ComponentModel.ISupportInitialize)(this.statusDataGridView)).BeginInit();
            this.headingPanelpanel.SuspendLayout();
            this.SuspendLayout();
            // 
            // statusDataGridView
            // 
            this.statusDataGridView.AllowUserToAddRows = false;
            this.statusDataGridView.AllowUserToResizeColumns = false;
            this.statusDataGridView.AllowUserToResizeRows = false;
            this.statusDataGridView.BackgroundColor = System.Drawing.Color.White;
            this.statusDataGridView.BorderStyle = System.Windows.Forms.BorderStyle.None;
            this.statusDataGridView.CellBorderStyle = System.Windows.Forms.DataGridViewCellBorderStyle.None;
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
            this.statusDataGridView.Location = new System.Drawing.Point(18, 74);
            this.statusDataGridView.Name = "statusDataGridView";
            this.statusDataGridView.ReadOnly = true;
            this.statusDataGridView.RowHeadersVisible = false;
            this.statusDataGridView.Size = new System.Drawing.Size(309, 222);
            this.statusDataGridView.TabIndex = 0;
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
            // cancelButton
            // 
            this.cancelButton.Location = new System.Drawing.Point(508, 309);
            this.cancelButton.Name = "cancelButton";
            this.cancelButton.Size = new System.Drawing.Size(75, 23);
            this.cancelButton.TabIndex = 1;
            this.cancelButton.Text = "Close";
            this.cancelButton.UseVisualStyleBackColor = true;
            this.cancelButton.Click += new System.EventHandler(this.cancelButton_Click);
            // 
            // backgroundWorker
            // 
            this.backgroundWorker.DoWork += new System.ComponentModel.DoWorkEventHandler(this.backgroundWorker_DoWork);
            this.backgroundWorker.RunWorkerCompleted += new System.ComponentModel.RunWorkerCompletedEventHandler(this.backgroundWorker_RunWorkerCompleted);
            this.backgroundWorker.ProgressChanged += new System.ComponentModel.ProgressChangedEventHandler(this.backgroundWorker_ProgressChanged);
            // 
            // protectButton
            // 
            this.protectButton.Enabled = false;
            this.protectButton.Location = new System.Drawing.Point(427, 309);
            this.protectButton.Name = "protectButton";
            this.protectButton.Size = new System.Drawing.Size(75, 23);
            this.protectButton.TabIndex = 2;
            this.protectButton.Text = "Protect";
            this.protectButton.UseVisualStyleBackColor = true;
            this.protectButton.Click += new System.EventHandler(this.protectButton_Click);
            // 
            // statusLabel
            // 
            this.statusLabel.AutoSize = true;
            this.statusLabel.Font = new System.Drawing.Font("Microsoft Sans Serif", 9F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.statusLabel.Location = new System.Drawing.Point(9, 16);
            this.statusLabel.Name = "statusLabel";
            this.statusLabel.Size = new System.Drawing.Size(116, 15);
            this.statusLabel.TabIndex = 3;
            this.statusLabel.Text = "Protection Status";
            // 
            // logLinkLabel
            // 
            this.logLinkLabel.AutoSize = true;
            this.logLinkLabel.Location = new System.Drawing.Point(12, 326);
            this.logLinkLabel.Name = "logLinkLabel";
            this.logLinkLabel.Size = new System.Drawing.Size(51, 13);
            this.logLinkLabel.TabIndex = 4;
            this.logLinkLabel.TabStop = true;
            this.logLinkLabel.Text = "View Log";
            this.logLinkLabel.Visible = false;
            this.logLinkLabel.LinkClicked += new System.Windows.Forms.LinkLabelLinkClickedEventHandler(this.logLinkLabel_LinkClicked);
            // 
            // warningLabel
            // 
            this.warningLabel.Location = new System.Drawing.Point(9, 299);
            this.warningLabel.Name = "warningLabel";
            this.warningLabel.Size = new System.Drawing.Size(412, 27);
            this.warningLabel.TabIndex = 5;
            this.warningLabel.Text = "Note:\r\n         You can close wizard and can monitor status using monitor option " +
                "in first screen";
            this.warningLabel.Visible = false;
            // 
            // headingPanelpanel
            // 
            this.headingPanelpanel.BackColor = System.Drawing.Color.SteelBlue;
            this.headingPanelpanel.Controls.Add(this.statusLabel);
            this.headingPanelpanel.Location = new System.Drawing.Point(0, 0);
            this.headingPanelpanel.Name = "headingPanelpanel";
            this.headingPanelpanel.Size = new System.Drawing.Size(597, 43);
            this.headingPanelpanel.TabIndex = 6;
            // 
            // recoveryLaterLabel
            // 
            this.recoveryLaterLabel.AutoSize = true;
            this.recoveryLaterLabel.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.recoveryLaterLabel.Location = new System.Drawing.Point(132, 160);
            this.recoveryLaterLabel.Name = "recoveryLaterLabel";
            this.recoveryLaterLabel.Size = new System.Drawing.Size(385, 13);
            this.recoveryLaterLabel.TabIndex = 7;
            this.recoveryLaterLabel.Text = "Fx job has been configured. You can recover when ever you want.";
            this.recoveryLaterLabel.Visible = false;
            // 
            // StatusForm
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.BackColor = System.Drawing.Color.White;
            this.ClientSize = new System.Drawing.Size(592, 351);
            this.ControlBox = false;
            this.Controls.Add(this.recoveryLaterLabel);
            this.Controls.Add(this.headingPanelpanel);
            this.Controls.Add(this.warningLabel);
            this.Controls.Add(this.logLinkLabel);
            this.Controls.Add(this.protectButton);
            this.Controls.Add(this.cancelButton);
            this.Controls.Add(this.statusDataGridView);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedSingle;
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.MaximizeBox = false;
            this.Name = "StatusForm";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
            this.Text = "Protection Status";
            ((System.ComponentModel.ISupportInitialize)(this.statusDataGridView)).EndInit();
            this.headingPanelpanel.ResumeLayout(false);
            this.headingPanelpanel.PerformLayout();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.DataGridView statusDataGridView;
        private System.Windows.Forms.Button cancelButton;
        private System.Windows.Forms.DataGridViewImageColumn statusColumn;
        private System.Windows.Forms.DataGridViewTextBoxColumn taskColumn;
        private System.Windows.Forms.Label statusLabel;
        private System.Windows.Forms.LinkLabel logLinkLabel;
        private System.Windows.Forms.Label warningLabel;
        private System.Windows.Forms.Panel headingPanelpanel;
        private System.Windows.Forms.Label recoveryLaterLabel;
        internal System.Windows.Forms.Button protectButton;
        internal System.ComponentModel.BackgroundWorker backgroundWorker;
    }
}