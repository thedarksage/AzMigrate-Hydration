namespace Com.Inmage.Wizard
{
    partial class NatConFigurationForm
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
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(NatConFigurationForm));
            this.cancelButton = new System.Windows.Forms.Button();
            this.addButton = new System.Windows.Forms.Button();
            this.natConfigurationLinkLabel = new System.Windows.Forms.LinkLabel();
            this.natDataGridView = new System.Windows.Forms.DataGridView();
            this.heading = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.localIp = new System.Windows.Forms.DataGridViewTextBoxColumn();
            this.natIp = new System.Windows.Forms.DataGridViewTextBoxColumn();
            ((System.ComponentModel.ISupportInitialize)(this.natDataGridView)).BeginInit();
            this.SuspendLayout();
            // 
            // cancelButton
            // 
            this.cancelButton.Location = new System.Drawing.Point(403, 176);
            this.cancelButton.Name = "cancelButton";
            this.cancelButton.Size = new System.Drawing.Size(75, 23);
            this.cancelButton.TabIndex = 9;
            this.cancelButton.Text = "Cancel";
            this.cancelButton.UseVisualStyleBackColor = true;
            this.cancelButton.Click += new System.EventHandler(this.cancelButton_Click);
            // 
            // addButton
            // 
            this.addButton.Location = new System.Drawing.Point(322, 176);
            this.addButton.Name = "addButton";
            this.addButton.Size = new System.Drawing.Size(75, 23);
            this.addButton.TabIndex = 8;
            this.addButton.Text = "Add";
            this.addButton.UseVisualStyleBackColor = true;
            this.addButton.Click += new System.EventHandler(this.addButton_Click);
            // 
            // natConfigurationLinkLabel
            // 
            this.natConfigurationLinkLabel.AutoSize = true;
            this.natConfigurationLinkLabel.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.natConfigurationLinkLabel.LinkColor = System.Drawing.Color.Orange;
            this.natConfigurationLinkLabel.Location = new System.Drawing.Point(42, 176);
            this.natConfigurationLinkLabel.Name = "natConfigurationLinkLabel";
            this.natConfigurationLinkLabel.Size = new System.Drawing.Size(174, 13);
            this.natConfigurationLinkLabel.TabIndex = 10;
            this.natConfigurationLinkLabel.TabStop = true;
            this.natConfigurationLinkLabel.Text = "More about Nat Configuration";
            this.natConfigurationLinkLabel.Visible = false;
            this.natConfigurationLinkLabel.LinkClicked += new System.Windows.Forms.LinkLabelLinkClickedEventHandler(this.natConfigurationLinkLabel_LinkClicked);
            // 
            // natDataGridView
            // 
            this.natDataGridView.AllowUserToAddRows = false;
            this.natDataGridView.BackgroundColor = System.Drawing.Color.White;
            this.natDataGridView.ColumnHeadersHeightSizeMode = System.Windows.Forms.DataGridViewColumnHeadersHeightSizeMode.AutoSize;
            this.natDataGridView.Columns.AddRange(new System.Windows.Forms.DataGridViewColumn[] {
            this.heading,
            this.localIp,
            this.natIp});
            this.natDataGridView.Location = new System.Drawing.Point(38, 12);
            this.natDataGridView.Name = "natDataGridView";
            this.natDataGridView.RowHeadersVisible = false;
            this.natDataGridView.Size = new System.Drawing.Size(368, 150);
            this.natDataGridView.TabIndex = 11;
            this.natDataGridView.CellValueChanged += new System.Windows.Forms.DataGridViewCellEventHandler(this.natDataGridView_CellValueChanged);
            this.natDataGridView.CurrentCellDirtyStateChanged += new System.EventHandler(this.natDataGridView_CurrentCellDirtyStateChanged);
            // 
            // heading
            // 
            this.heading.HeaderText = "";
            this.heading.Name = "heading";
            this.heading.ReadOnly = true;
            this.heading.Width = 60;
            // 
            // localIp
            // 
            this.localIp.HeaderText = "Local IP";
            this.localIp.Name = "localIp";
            this.localIp.ReadOnly = true;
            this.localIp.Width = 150;
            // 
            // natIp
            // 
            this.natIp.HeaderText = "NAT IP";
            this.natIp.Name = "natIp";
            this.natIp.Width = 150;
            // 
            // NatConFigurationForm
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.BackColor = System.Drawing.Color.White;
            this.ClientSize = new System.Drawing.Size(500, 211);
            this.ControlBox = false;
            this.Controls.Add(this.natDataGridView);
            this.Controls.Add(this.natConfigurationLinkLabel);
            this.Controls.Add(this.cancelButton);
            this.Controls.Add(this.addButton);
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.Name = "NatConFigurationForm";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
            this.Text = "NAT ConFiguration";
            ((System.ComponentModel.ISupportInitialize)(this.natDataGridView)).EndInit();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Button cancelButton;
        private System.Windows.Forms.Button addButton;
        private System.Windows.Forms.LinkLabel natConfigurationLinkLabel;
        private System.Windows.Forms.DataGridView natDataGridView;
        private System.Windows.Forms.DataGridViewTextBoxColumn heading;
        private System.Windows.Forms.DataGridViewTextBoxColumn localIp;
        private System.Windows.Forms.DataGridViewTextBoxColumn natIp;
    }
}