namespace Com.Inmage.Wizard
{
    partial class DiskDetails
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
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(DiskDetails));
            this.okButton = new System.Windows.Forms.Button();
            this.yesButton = new System.Windows.Forms.Button();
            this.pictureBox = new System.Windows.Forms.PictureBox();
            this.descriptionLabel = new System.Windows.Forms.Label();
            this.detailsDataGridView = new System.Windows.Forms.DataGridView();
            this.pimaryValue = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.secondary = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.noteLabel = new System.Windows.Forms.Label();
            ((System.ComponentModel.ISupportInitialize)(this.pictureBox)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.detailsDataGridView)).BeginInit();
            this.SuspendLayout();
            // 
            // okButton
            // 
            this.okButton.Location = new System.Drawing.Point(347, 234);
            this.okButton.Name = "okButton";
            this.okButton.Size = new System.Drawing.Size(75, 23);
            this.okButton.TabIndex = 0;
            this.okButton.Text = "Ok";
            this.okButton.UseVisualStyleBackColor = true;
            this.okButton.Click += new System.EventHandler(this.okButton_Click);
            // 
            // yesButton
            // 
            this.yesButton.Location = new System.Drawing.Point(266, 234);
            this.yesButton.Name = "yesButton";
            this.yesButton.Size = new System.Drawing.Size(75, 23);
            this.yesButton.TabIndex = 1;
            this.yesButton.Text = "Yes";
            this.yesButton.UseVisualStyleBackColor = true;
            this.yesButton.Click += new System.EventHandler(this.yesButton_Click);
            // 
            // pictureBox
            // 
            this.pictureBox.Location = new System.Drawing.Point(12, 20);
            this.pictureBox.Name = "pictureBox";
            this.pictureBox.Size = new System.Drawing.Size(45, 44);
            this.pictureBox.TabIndex = 3;
            this.pictureBox.TabStop = false;
            // 
            // descriptionLabel
            // 
            this.descriptionLabel.Location = new System.Drawing.Point(73, 15);
            this.descriptionLabel.Name = "descriptionLabel";
            this.descriptionLabel.Size = new System.Drawing.Size(342, 65);
            this.descriptionLabel.TabIndex = 2;
            this.descriptionLabel.Text = "label1";
            // 
            // detailsDataGridView
            // 
            this.detailsDataGridView.AllowUserToAddRows = false;
            this.detailsDataGridView.AllowUserToDeleteRows = false;
            this.detailsDataGridView.AllowUserToResizeColumns = false;
            this.detailsDataGridView.AllowUserToResizeRows = false;
            this.detailsDataGridView.BackgroundColor = System.Drawing.Color.White;
            this.detailsDataGridView.BorderStyle = System.Windows.Forms.BorderStyle.None;
            this.detailsDataGridView.ColumnHeadersBorderStyle = System.Windows.Forms.DataGridViewHeaderBorderStyle.None;
            this.detailsDataGridView.ColumnHeadersHeightSizeMode = System.Windows.Forms.DataGridViewColumnHeadersHeightSizeMode.AutoSize;
            this.detailsDataGridView.ColumnHeadersVisible = false;
            this.detailsDataGridView.Columns.AddRange(new System.Windows.Forms.DataGridViewColumn[] {
            this.pimaryValue,
            this.secondary});
            this.detailsDataGridView.Location = new System.Drawing.Point(78, 35);
            this.detailsDataGridView.Name = "detailsDataGridView";
            this.detailsDataGridView.RowHeadersBorderStyle = System.Windows.Forms.DataGridViewHeaderBorderStyle.None;
            this.detailsDataGridView.RowHeadersVisible = false;
            this.detailsDataGridView.Size = new System.Drawing.Size(323, 169);
            this.detailsDataGridView.TabIndex = 5;
            // 
            // pimaryValue
            // 
            this.pimaryValue.HeaderText = "Primary";
            this.pimaryValue.Name = "pimaryValue";
            this.pimaryValue.ReadOnly = true;
            // 
            // secondary
            // 
            this.secondary.HeaderText = "Secondary";
            this.secondary.Name = "secondary";
            this.secondary.ReadOnly = true;
            this.secondary.Width = 220;
            // 
            // noteLabel
            // 
            this.noteLabel.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.noteLabel.Location = new System.Drawing.Point(12, 219);
            this.noteLabel.Name = "noteLabel";
            this.noteLabel.Size = new System.Drawing.Size(248, 38);
            this.noteLabel.TabIndex = 6;
            this.noteLabel.Text = "Note: If device contains multipath then multipath device will be selected for pro" +
                "tection.";
            this.noteLabel.Visible = false;
            // 
            // DiskDetails
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.BackColor = System.Drawing.Color.White;
            this.ClientSize = new System.Drawing.Size(427, 259);
            this.ControlBox = false;
            this.Controls.Add(this.noteLabel);
            this.Controls.Add(this.detailsDataGridView);
            this.Controls.Add(this.pictureBox);
            this.Controls.Add(this.descriptionLabel);
            this.Controls.Add(this.yesButton);
            this.Controls.Add(this.okButton);
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.MaximizeBox = false;
            this.MinimizeBox = false;
            this.Name = "DiskDetails";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
            this.Text = "Disk Details";
            ((System.ComponentModel.ISupportInitialize)(this.pictureBox)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.detailsDataGridView)).EndInit();
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.DataGridViewTextBoxColumn pimaryValue;
        private System.Windows.Forms.DataGridViewTextBoxColumn secondary;
        internal System.Windows.Forms.Button okButton;
        internal System.Windows.Forms.Button yesButton;
        internal System.Windows.Forms.PictureBox pictureBox;
        internal System.Windows.Forms.Label descriptionLabel;
        internal System.Windows.Forms.DataGridView detailsDataGridView;
        internal System.Windows.Forms.Label noteLabel;
    }
}