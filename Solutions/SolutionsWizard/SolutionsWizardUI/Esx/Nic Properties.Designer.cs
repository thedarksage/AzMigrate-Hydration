namespace Com.Inmage.Wizard
{
    partial class NicPropertiesForm
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
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(NicPropertiesForm));
            this.netWorkComboBox = new System.Windows.Forms.ComboBox();
            this.netWorkLabel = new System.Windows.Forms.Label();
            this.useDhcpRadioButton = new System.Windows.Forms.RadioButton();
            this.cancelButton = new System.Windows.Forms.Button();
            this.addButton = new System.Windows.Forms.Button();
            this.requiredFieldLabel = new System.Windows.Forms.Label();
            this.viewAllIPsPanel = new System.Windows.Forms.Panel();
            this.nonDHCPRadioButton = new System.Windows.Forms.RadioButton();
            this.ipTabControl = new System.Windows.Forms.TabControl();
            this.doubleStarLabel = new System.Windows.Forms.Label();
            this.backgroundWorker = new System.ComponentModel.BackgroundWorker();
            this.refreshPictureBox = new System.Windows.Forms.PictureBox();
            this.progressBar = new System.Windows.Forms.ProgressBar();
            this.secondNotelabel = new System.Windows.Forms.Label();
            this.viewAllIPsPanel.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.refreshPictureBox)).BeginInit();
            this.SuspendLayout();
            // 
            // netWorkComboBox
            // 
            this.netWorkComboBox.FormattingEnabled = true;
            this.netWorkComboBox.Location = new System.Drawing.Point(184, 279);
            this.netWorkComboBox.Name = "netWorkComboBox";
            this.netWorkComboBox.Size = new System.Drawing.Size(115, 21);
            this.netWorkComboBox.TabIndex = 7;
            // 
            // netWorkLabel
            // 
            this.netWorkLabel.AutoSize = true;
            this.netWorkLabel.Location = new System.Drawing.Point(77, 282);
            this.netWorkLabel.Name = "netWorkLabel";
            this.netWorkLabel.Size = new System.Drawing.Size(72, 13);
            this.netWorkLabel.TabIndex = 10;
            this.netWorkLabel.Text = "Network label";
            // 
            // useDhcpRadioButton
            // 
            this.useDhcpRadioButton.AutoSize = true;
            this.useDhcpRadioButton.BackColor = System.Drawing.Color.Transparent;
            this.useDhcpRadioButton.Location = new System.Drawing.Point(36, 12);
            this.useDhcpRadioButton.Name = "useDhcpRadioButton";
            this.useDhcpRadioButton.Size = new System.Drawing.Size(77, 17);
            this.useDhcpRadioButton.TabIndex = 1;
            this.useDhcpRadioButton.Text = "Use DHCP";
            this.useDhcpRadioButton.UseVisualStyleBackColor = false;
            this.useDhcpRadioButton.CheckedChanged += new System.EventHandler(this.useDhcpRadioButton_CheckedChanged);
            // 
            // cancelButton
            // 
            this.cancelButton.Location = new System.Drawing.Point(286, 306);
            this.cancelButton.Name = "cancelButton";
            this.cancelButton.Size = new System.Drawing.Size(75, 23);
            this.cancelButton.TabIndex = 9;
            this.cancelButton.Text = "Cancel";
            this.cancelButton.UseVisualStyleBackColor = true;
            this.cancelButton.Click += new System.EventHandler(this.cancelButton_Click);
            // 
            // addButton
            // 
            this.addButton.Location = new System.Drawing.Point(205, 306);
            this.addButton.Name = "addButton";
            this.addButton.Size = new System.Drawing.Size(75, 23);
            this.addButton.TabIndex = 8;
            this.addButton.Text = "Add";
            this.addButton.UseVisualStyleBackColor = true;
            this.addButton.Click += new System.EventHandler(this.addButton_Click);
            // 
            // requiredFieldLabel
            // 
            this.requiredFieldLabel.ForeColor = System.Drawing.Color.Red;
            this.requiredFieldLabel.Location = new System.Drawing.Point(9, 340);
            this.requiredFieldLabel.Name = "requiredFieldLabel";
            this.requiredFieldLabel.Size = new System.Drawing.Size(361, 79);
            this.requiredFieldLabel.TabIndex = 10;
            this.requiredFieldLabel.Text = resources.GetString("requiredFieldLabel.Text");
            // 
            // viewAllIPsPanel
            // 
            this.viewAllIPsPanel.Controls.Add(this.nonDHCPRadioButton);
            this.viewAllIPsPanel.Controls.Add(this.ipTabControl);
            this.viewAllIPsPanel.Location = new System.Drawing.Point(18, 37);
            this.viewAllIPsPanel.Name = "viewAllIPsPanel";
            this.viewAllIPsPanel.Size = new System.Drawing.Size(352, 236);
            this.viewAllIPsPanel.TabIndex = 11;
            // 
            // nonDHCPRadioButton
            // 
            this.nonDHCPRadioButton.AutoSize = true;
            this.nonDHCPRadioButton.Checked = true;
            this.nonDHCPRadioButton.Location = new System.Drawing.Point(18, 5);
            this.nonDHCPRadioButton.Name = "nonDHCPRadioButton";
            this.nonDHCPRadioButton.Size = new System.Drawing.Size(142, 17);
            this.nonDHCPRadioButton.TabIndex = 1;
            this.nonDHCPRadioButton.TabStop = true;
            this.nonDHCPRadioButton.Text = "Use following IP Address";
            this.nonDHCPRadioButton.UseVisualStyleBackColor = true;
            this.nonDHCPRadioButton.CheckedChanged += new System.EventHandler(this.nonDHCPRadioButton_CheckedChanged);
            // 
            // ipTabControl
            // 
            this.ipTabControl.Location = new System.Drawing.Point(7, 23);
            this.ipTabControl.Name = "ipTabControl";
            this.ipTabControl.SelectedIndex = 0;
            this.ipTabControl.Size = new System.Drawing.Size(330, 210);
            this.ipTabControl.TabIndex = 0;
            this.ipTabControl.SelectedIndexChanged += new System.EventHandler(this.ipTabControl_SelectedIndexChanged);
            this.ipTabControl.TabIndexChanged += new System.EventHandler(this.ipTabControl_TabIndexChanged);
            // 
            // doubleStarLabel
            // 
            this.doubleStarLabel.AutoSize = true;
            this.doubleStarLabel.ForeColor = System.Drawing.Color.Red;
            this.doubleStarLabel.Location = new System.Drawing.Point(343, 283);
            this.doubleStarLabel.Name = "doubleStarLabel";
            this.doubleStarLabel.Size = new System.Drawing.Size(18, 13);
            this.doubleStarLabel.TabIndex = 12;
            this.doubleStarLabel.Text = "* *";
            // 
            // backgroundWorker
            // 
            this.backgroundWorker.DoWork += new System.ComponentModel.DoWorkEventHandler(this.backgroundWorker_DoWork);
            this.backgroundWorker.ProgressChanged += new System.ComponentModel.ProgressChangedEventHandler(this.backgroundWorker_ProgressChanged);
            this.backgroundWorker.RunWorkerCompleted += new System.ComponentModel.RunWorkerCompletedEventHandler(this.backgroundWorker_RunWorkerCompleted);
            // 
            // refreshPictureBox
            // 
            this.refreshPictureBox.Image = global::Com.Inmage.Wizard.Properties.Resources.view_refresh;
            this.refreshPictureBox.Location = new System.Drawing.Point(315, 280);
            this.refreshPictureBox.Name = "refreshPictureBox";
            this.refreshPictureBox.Size = new System.Drawing.Size(15, 17);
            this.refreshPictureBox.TabIndex = 14;
            this.refreshPictureBox.TabStop = false;
            this.refreshPictureBox.Click += new System.EventHandler(this.refreshPictureBox_Click);
            // 
            // progressBar
            // 
            this.progressBar.BackColor = System.Drawing.Color.LightGray;
            this.progressBar.ForeColor = System.Drawing.Color.ForestGreen;
            this.progressBar.Location = new System.Drawing.Point(3, 424);
            this.progressBar.Name = "progressBar";
            this.progressBar.Size = new System.Drawing.Size(374, 10);
            this.progressBar.Style = System.Windows.Forms.ProgressBarStyle.Marquee;
            this.progressBar.TabIndex = 17;
            this.progressBar.Visible = false;
            // 
            // secondNotelabel
            // 
            this.secondNotelabel.AutoSize = true;
            this.secondNotelabel.Location = new System.Drawing.Point(8, 398);
            this.secondNotelabel.Name = "secondNotelabel";
            this.secondNotelabel.Size = new System.Drawing.Size(362, 13);
            this.secondNotelabel.TabIndex = 18;
            this.secondNotelabel.Text = "Please wait for a while. Retriving network information form secondary server";
            this.secondNotelabel.Visible = false;
            // 
            // NicPropertiesForm
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.AutoScroll = true;
            this.BackColor = System.Drawing.Color.White;
            this.ClientSize = new System.Drawing.Size(382, 441);
            this.Controls.Add(this.secondNotelabel);
            this.Controls.Add(this.progressBar);
            this.Controls.Add(this.refreshPictureBox);
            this.Controls.Add(this.doubleStarLabel);
            this.Controls.Add(this.viewAllIPsPanel);
            this.Controls.Add(this.requiredFieldLabel);
            this.Controls.Add(this.addButton);
            this.Controls.Add(this.netWorkComboBox);
            this.Controls.Add(this.cancelButton);
            this.Controls.Add(this.netWorkLabel);
            this.Controls.Add(this.useDhcpRadioButton);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedSingle;
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.MaximizeBox = false;
            this.Name = "NicPropertiesForm";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
            this.Text = "NIC Properties";
            this.viewAllIPsPanel.ResumeLayout(false);
            this.viewAllIPsPanel.PerformLayout();
            ((System.ComponentModel.ISupportInitialize)(this.refreshPictureBox)).EndInit();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Label requiredFieldLabel;
        private System.Windows.Forms.Label doubleStarLabel;
        private System.ComponentModel.BackgroundWorker backgroundWorker;
        private System.Windows.Forms.Label secondNotelabel;
        internal System.Windows.Forms.RadioButton useDhcpRadioButton;
        internal System.Windows.Forms.Button cancelButton;
        internal System.Windows.Forms.Button addButton;
        internal System.Windows.Forms.ComboBox netWorkComboBox;
        internal System.Windows.Forms.Label netWorkLabel;
        internal System.Windows.Forms.Panel viewAllIPsPanel;
        internal System.Windows.Forms.TabControl ipTabControl;
        internal System.Windows.Forms.RadioButton nonDHCPRadioButton;
        internal System.Windows.Forms.PictureBox refreshPictureBox;
        internal System.Windows.Forms.ProgressBar progressBar;
    }
}