namespace Com.Inmage.Wizard
{
    partial class EsxRdmSelectionForm
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
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(EsxRdmSelectionForm));
            this.topLinePanel = new System.Windows.Forms.Panel();
            this.credentialsPictureBox = new System.Windows.Forms.PictureBox();
            this.panel1 = new System.Windows.Forms.Panel();
            this.rdmLunDetailsDataGridView = new System.Windows.Forms.DataGridView();
            this.diskName = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.size = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.selectLunCombobox = new System.Windows.Forms.DataGridViewComboBoxColumn();
            this.cancelButton = new System.Windows.Forms.Button();
            this.addButton = new System.Windows.Forms.Button();
            this.dispalyNameLabel = new System.Windows.Forms.Label();
            this.rdmNoteLabel = new System.Windows.Forms.Label();
            this.provideLunsLabel = new System.Windows.Forms.Label();
            this.headingPictureBox = new System.Windows.Forms.PictureBox();
            this.loadPictureBox = new System.Windows.Forms.PictureBox();
            this.backgroundWorker = new System.ComponentModel.BackgroundWorker();
            this.mandatoryLabel = new System.Windows.Forms.Label();
            ((System.ComponentModel.ISupportInitialize)(this.credentialsPictureBox)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.rdmLunDetailsDataGridView)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.headingPictureBox)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.loadPictureBox)).BeginInit();
            this.SuspendLayout();
            // 
            // topLinePanel
            // 
            this.topLinePanel.BackColor = System.Drawing.Color.Maroon;
            this.topLinePanel.Location = new System.Drawing.Point(2, 40);
            this.topLinePanel.Name = "topLinePanel";
            this.topLinePanel.Size = new System.Drawing.Size(650, 1);
            this.topLinePanel.TabIndex = 24;
            // 
            // credentialsPictureBox
            // 
            this.credentialsPictureBox.BackgroundImageLayout = System.Windows.Forms.ImageLayout.Stretch;
            this.credentialsPictureBox.Location = new System.Drawing.Point(3, 2);
            this.credentialsPictureBox.Name = "credentialsPictureBox";
            this.credentialsPictureBox.Size = new System.Drawing.Size(646, 38);
            this.credentialsPictureBox.TabIndex = 23;
            this.credentialsPictureBox.TabStop = false;
            // 
            // panel1
            // 
            this.panel1.BackColor = System.Drawing.Color.Maroon;
            this.panel1.Location = new System.Drawing.Point(0, 285);
            this.panel1.Name = "panel1";
            this.panel1.Size = new System.Drawing.Size(650, 1);
            this.panel1.TabIndex = 25;
            // 
            // rdmLunDetailsDataGridView
            // 
            this.rdmLunDetailsDataGridView.AllowUserToAddRows = false;
            this.rdmLunDetailsDataGridView.BackgroundColor = System.Drawing.Color.White;
            this.rdmLunDetailsDataGridView.BorderStyle = System.Windows.Forms.BorderStyle.None;
            this.rdmLunDetailsDataGridView.ColumnHeadersHeightSizeMode = System.Windows.Forms.DataGridViewColumnHeadersHeightSizeMode.AutoSize;
            this.rdmLunDetailsDataGridView.Columns.AddRange(new System.Windows.Forms.DataGridViewColumn[] {
            this.diskName,
            this.size,
            this.selectLunCombobox});
            this.rdmLunDetailsDataGridView.EditMode = System.Windows.Forms.DataGridViewEditMode.EditOnEnter;
            this.rdmLunDetailsDataGridView.Location = new System.Drawing.Point(12, 79);
            this.rdmLunDetailsDataGridView.Name = "rdmLunDetailsDataGridView";
            this.rdmLunDetailsDataGridView.RowHeadersVisible = false;
            this.rdmLunDetailsDataGridView.Size = new System.Drawing.Size(622, 177);
            this.rdmLunDetailsDataGridView.TabIndex = 26;
            this.rdmLunDetailsDataGridView.CellValueChanged += new System.Windows.Forms.DataGridViewCellEventHandler(this.rdmLunDetailsDataGridView_CellValueChanged);
            this.rdmLunDetailsDataGridView.CurrentCellDirtyStateChanged += new System.EventHandler(this.rdmLunDetailsDataGridView_CurrentCellDirtyStateChanged);
            // 
            // diskName
            // 
            this.diskName.FillWeight = 80F;
            this.diskName.HeaderText = "Disk Name";
            this.diskName.Name = "diskName";
            this.diskName.ReadOnly = true;
            // 
            // size
            // 
            this.size.FillWeight = 82.39521F;
            this.size.HeaderText = "Size of Rdm disk(in KBs)";
            this.size.Name = "size";
            this.size.ReadOnly = true;
            this.size.Width = 103;
            // 
            // selectLunCombobox
            // 
            this.selectLunCombobox.FillWeight = 168.6228F;
            this.selectLunCombobox.HeaderText = "Select Lun *";
            this.selectLunCombobox.Name = "selectLunCombobox";
            this.selectLunCombobox.Width = 430;
            // 
            // cancelButton
            // 
            this.cancelButton.Location = new System.Drawing.Point(557, 294);
            this.cancelButton.Name = "cancelButton";
            this.cancelButton.Size = new System.Drawing.Size(75, 23);
            this.cancelButton.TabIndex = 27;
            this.cancelButton.Text = "Cancel";
            this.cancelButton.UseVisualStyleBackColor = true;
            this.cancelButton.Click += new System.EventHandler(this.cancelButton_Click);
            // 
            // addButton
            // 
            this.addButton.Location = new System.Drawing.Point(463, 294);
            this.addButton.Name = "addButton";
            this.addButton.Size = new System.Drawing.Size(75, 23);
            this.addButton.TabIndex = 28;
            this.addButton.Text = "Add";
            this.addButton.UseVisualStyleBackColor = true;
            this.addButton.Click += new System.EventHandler(this.addButton_Click);
            // 
            // dispalyNameLabel
            // 
            this.dispalyNameLabel.AutoSize = true;
            this.dispalyNameLabel.BackColor = System.Drawing.Color.Transparent;
            this.dispalyNameLabel.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.dispalyNameLabel.Location = new System.Drawing.Point(12, 54);
            this.dispalyNameLabel.Name = "dispalyNameLabel";
            this.dispalyNameLabel.Size = new System.Drawing.Size(41, 13);
            this.dispalyNameLabel.TabIndex = 29;
            this.dispalyNameLabel.Text = "label1";
            // 
            // rdmNoteLabel
            // 
            this.rdmNoteLabel.AutoSize = true;
            this.rdmNoteLabel.ForeColor = System.Drawing.Color.Red;
            this.rdmNoteLabel.Location = new System.Drawing.Point(18, 264);
            this.rdmNoteLabel.Name = "rdmNoteLabel";
            this.rdmNoteLabel.Size = new System.Drawing.Size(360, 13);
            this.rdmNoteLabel.TabIndex = 30;
            this.rdmNoteLabel.Text = "Note : Make sure that selected lun should not be used for another machine";
            // 
            // provideLunsLabel
            // 
            this.provideLunsLabel.AutoSize = true;
            this.provideLunsLabel.BackColor = System.Drawing.Color.White;
            this.provideLunsLabel.Font = new System.Drawing.Font("Microsoft Sans Serif", 9.75F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.provideLunsLabel.ForeColor = System.Drawing.Color.FromArgb(((int)(((byte)(186)))), ((int)(((byte)(61)))), ((int)(((byte)(69)))));
            this.provideLunsLabel.Location = new System.Drawing.Point(21, 13);
            this.provideLunsLabel.Name = "provideLunsLabel";
            this.provideLunsLabel.Size = new System.Drawing.Size(128, 16);
            this.provideLunsLabel.TabIndex = 31;
            this.provideLunsLabel.Text = "Select target luns";
            // 
            // headingPictureBox
            // 
            this.headingPictureBox.BackgroundImage = global::Com.Inmage.Wizard.Properties.Resources.popupheader;
            this.headingPictureBox.BackgroundImageLayout = System.Windows.Forms.ImageLayout.Stretch;
            this.headingPictureBox.Location = new System.Drawing.Point(247, 2);
            this.headingPictureBox.Name = "headingPictureBox";
            this.headingPictureBox.Size = new System.Drawing.Size(399, 38);
            this.headingPictureBox.TabIndex = 32;
            this.headingPictureBox.TabStop = false;
            // 
            // loadPictureBox
            // 
            this.loadPictureBox.BackColor = System.Drawing.Color.Transparent;
            this.loadPictureBox.Image = ((System.Drawing.Image)(resources.GetObject("loadPictureBox.Image")));
            this.loadPictureBox.Location = new System.Drawing.Point(232, 73);
            this.loadPictureBox.Name = "loadPictureBox";
            this.loadPictureBox.Size = new System.Drawing.Size(183, 201);
            this.loadPictureBox.SizeMode = System.Windows.Forms.PictureBoxSizeMode.CenterImage;
            this.loadPictureBox.TabIndex = 97;
            this.loadPictureBox.TabStop = false;
            this.loadPictureBox.Visible = false;
            // 
            // backgroundWorker
            // 
            this.backgroundWorker.DoWork += new System.ComponentModel.DoWorkEventHandler(this.backgroundWorker_DoWork);
            this.backgroundWorker.RunWorkerCompleted += new System.ComponentModel.RunWorkerCompletedEventHandler(this.backgroundWorker_RunWorkerCompleted);
            // 
            // mandatoryLabel
            // 
            this.mandatoryLabel.AutoSize = true;
            this.mandatoryLabel.BackColor = System.Drawing.Color.Transparent;
            this.mandatoryLabel.ForeColor = System.Drawing.Color.Black;
            this.mandatoryLabel.Location = new System.Drawing.Point(18, 299);
            this.mandatoryLabel.Name = "mandatoryLabel";
            this.mandatoryLabel.Size = new System.Drawing.Size(103, 13);
            this.mandatoryLabel.TabIndex = 143;
            this.mandatoryLabel.Text = "( * ) Mandatory fields";
            // 
            // ESX_RdmSelectionForm
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.BackColor = System.Drawing.Color.White;
            this.BackgroundImage = global::Com.Inmage.Wizard.Properties.Resources.workarea;
            this.ClientSize = new System.Drawing.Size(646, 347);
            this.ControlBox = false;
            this.Controls.Add(this.mandatoryLabel);
            this.Controls.Add(this.loadPictureBox);
            this.Controls.Add(this.headingPictureBox);
            this.Controls.Add(this.provideLunsLabel);
            this.Controls.Add(this.rdmNoteLabel);
            this.Controls.Add(this.dispalyNameLabel);
            this.Controls.Add(this.addButton);
            this.Controls.Add(this.cancelButton);
            this.Controls.Add(this.rdmLunDetailsDataGridView);
            this.Controls.Add(this.panel1);
            this.Controls.Add(this.topLinePanel);
            this.Controls.Add(this.credentialsPictureBox);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.MaximizeBox = false;
            this.Name = "ESX_RdmSelectionForm";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
            this.Text = "Select RDM";
            ((System.ComponentModel.ISupportInitialize)(this.credentialsPictureBox)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.rdmLunDetailsDataGridView)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.headingPictureBox)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.loadPictureBox)).EndInit();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Panel topLinePanel;
        private System.Windows.Forms.PictureBox credentialsPictureBox;
        private System.Windows.Forms.Panel panel1;
        private System.Windows.Forms.Button cancelButton;
        private System.Windows.Forms.Label dispalyNameLabel;
        private System.Windows.Forms.Label rdmNoteLabel;
        private System.Windows.Forms.Label provideLunsLabel;
        private System.Windows.Forms.PictureBox headingPictureBox;
        private System.ComponentModel.BackgroundWorker backgroundWorker;
        private System.Windows.Forms.DataGridViewTextBoxColumn diskName;
        private System.Windows.Forms.DataGridViewTextBoxColumn size;
        private System.Windows.Forms.DataGridViewComboBoxColumn selectLunCombobox;
        internal System.Windows.Forms.DataGridView rdmLunDetailsDataGridView;
        internal System.Windows.Forms.Button addButton;
        internal System.Windows.Forms.PictureBox loadPictureBox;
        internal System.Windows.Forms.Label mandatoryLabel;
    }
}