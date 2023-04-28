namespace Com.Inmage.Wizard
{
    partial class EditingMasterXmlFile
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
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(EditingMasterXmlFile));
            this.xmlEditTextBox = new System.Windows.Forms.TextBox();
            this.saveButton = new System.Windows.Forms.Button();
            this.cancelButton = new System.Windows.Forms.Button();
            this.backgroundWorker = new System.ComponentModel.BackgroundWorker();
            this.progressBar = new System.Windows.Forms.ProgressBar();
            this.chooseESXIPLabel = new System.Windows.Forms.Label();
            this.chooseESXIPComboBox = new System.Windows.Forms.ComboBox();
            this.SuspendLayout();
            // 
            // xmlEditTextBox
            // 
            this.xmlEditTextBox.Font = new System.Drawing.Font("Microsoft Sans Serif", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.xmlEditTextBox.Location = new System.Drawing.Point(3, 41);
            this.xmlEditTextBox.MaxLength = 999999999;
            this.xmlEditTextBox.Multiline = true;
            this.xmlEditTextBox.Name = "xmlEditTextBox";
            this.xmlEditTextBox.ScrollBars = System.Windows.Forms.ScrollBars.Both;
            this.xmlEditTextBox.Size = new System.Drawing.Size(833, 485);
            this.xmlEditTextBox.TabIndex = 0;
            this.xmlEditTextBox.WordWrap = false;
            // 
            // saveButton
            // 
            this.saveButton.Location = new System.Drawing.Point(675, 551);
            this.saveButton.Name = "saveButton";
            this.saveButton.RightToLeft = System.Windows.Forms.RightToLeft.No;
            this.saveButton.Size = new System.Drawing.Size(75, 23);
            this.saveButton.TabIndex = 1;
            this.saveButton.Text = "Save";
            this.saveButton.UseVisualStyleBackColor = true;
            this.saveButton.Click += new System.EventHandler(this.saveButton_Click);
            // 
            // cancelButton
            // 
            this.cancelButton.Location = new System.Drawing.Point(756, 551);
            this.cancelButton.Name = "cancelButton";
            this.cancelButton.RightToLeft = System.Windows.Forms.RightToLeft.No;
            this.cancelButton.Size = new System.Drawing.Size(75, 23);
            this.cancelButton.TabIndex = 2;
            this.cancelButton.Text = "Cancel";
            this.cancelButton.UseVisualStyleBackColor = true;
            this.cancelButton.Click += new System.EventHandler(this.cancelButton_Click);
            // 
            // backgroundWorker
            // 
            this.backgroundWorker.DoWork += new System.ComponentModel.DoWorkEventHandler(this.backgroundWorker_DoWork);
            this.backgroundWorker.RunWorkerCompleted += new System.ComponentModel.RunWorkerCompletedEventHandler(this.backgroundWorker_RunWorkerCompleted);
            this.backgroundWorker.ProgressChanged += new System.ComponentModel.ProgressChangedEventHandler(this.backgroundWorker_ProgressChanged);
            // 
            // progressBar
            // 
            this.progressBar.BackColor = System.Drawing.Color.LightGray;
            this.progressBar.ForeColor = System.Drawing.Color.ForestGreen;
            this.progressBar.Location = new System.Drawing.Point(1, 532);
            this.progressBar.Name = "progressBar";
            this.progressBar.Size = new System.Drawing.Size(845, 10);
            this.progressBar.Style = System.Windows.Forms.ProgressBarStyle.Marquee;
            this.progressBar.TabIndex = 31;
            this.progressBar.Visible = false;
            // 
            // chooseESXIPLabel
            // 
            this.chooseESXIPLabel.AutoSize = true;
            this.chooseESXIPLabel.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.chooseESXIPLabel.Location = new System.Drawing.Point(7, 14);
            this.chooseESXIPLabel.Name = "chooseESXIPLabel";
            this.chooseESXIPLabel.Size = new System.Drawing.Size(134, 13);
            this.chooseESXIPLabel.TabIndex = 32;
            this.chooseESXIPLabel.Text = "Choose Target ESX IP";
            // 
            // chooseESXIPComboBox
            // 
            this.chooseESXIPComboBox.FormattingEnabled = true;
            this.chooseESXIPComboBox.Location = new System.Drawing.Point(155, 9);
            this.chooseESXIPComboBox.Name = "chooseESXIPComboBox";
            this.chooseESXIPComboBox.Size = new System.Drawing.Size(121, 21);
            this.chooseESXIPComboBox.TabIndex = 33;
            this.chooseESXIPComboBox.SelectedValueChanged += new System.EventHandler(this.chooseESXIPComboBox_SelectedValueChanged);
            // 
            // EditingMasterXmlFile
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.BackColor = System.Drawing.Color.White;
            this.ClientSize = new System.Drawing.Size(848, 586);
            this.Controls.Add(this.chooseESXIPComboBox);
            this.Controls.Add(this.chooseESXIPLabel);
            this.Controls.Add(this.progressBar);
            this.Controls.Add(this.cancelButton);
            this.Controls.Add(this.saveButton);
            this.Controls.Add(this.xmlEditTextBox);
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.Name = "EditingMasterXmlFile";
            this.Text = "EditingMasterXmlFile";
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Button saveButton;
        private System.Windows.Forms.Button cancelButton;
        private System.ComponentModel.BackgroundWorker backgroundWorker;
        private System.Windows.Forms.Label chooseESXIPLabel;
        private System.Windows.Forms.ComboBox chooseESXIPComboBox;
        internal System.Windows.Forms.TextBox xmlEditTextBox;
        internal System.Windows.Forms.ProgressBar progressBar;
    }
}