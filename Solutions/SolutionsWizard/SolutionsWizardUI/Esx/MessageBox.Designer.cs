namespace Com.Inmage.Wizard
{
    partial class MessageBoxForm
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
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(MessageBoxForm));
            this.description1Label = new System.Windows.Forms.Label();
            this.followDescriptionLabel = new System.Windows.Forms.Label();
            this.step1Label = new System.Windows.Forms.Label();
            this.driverLinkLabel = new System.Windows.Forms.LinkLabel();
            this.step2Label = new System.Windows.Forms.Label();
            this.step3Label = new System.Windows.Forms.Label();
            this.noteLabel = new System.Windows.Forms.Label();
            this.okButton = new System.Windows.Forms.Button();
            this.cancelbutton = new System.Windows.Forms.Button();
            this.SuspendLayout();
            // 
            // description1Label
            // 
            this.description1Label.AutoSize = true;
            this.description1Label.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.description1Label.Location = new System.Drawing.Point(25, 18);
            this.description1Label.Name = "description1Label";
            this.description1Label.Size = new System.Drawing.Size(282, 13);
            this.description1Label.TabIndex = 0;
            this.description1Label.Text = "Windows XP CD/iso will not contain required SCSI drivers.";
            // 
            // followDescriptionLabel
            // 
            this.followDescriptionLabel.AutoSize = true;
            this.followDescriptionLabel.Location = new System.Drawing.Point(25, 42);
            this.followDescriptionLabel.Name = "followDescriptionLabel";
            this.followDescriptionLabel.Size = new System.Drawing.Size(99, 13);
            this.followDescriptionLabel.TabIndex = 1;
            this.followDescriptionLabel.Text = "Follow below steps:";
            // 
            // step1Label
            // 
            this.step1Label.AutoSize = true;
            this.step1Label.Location = new System.Drawing.Point(39, 64);
            this.step1Label.Name = "step1Label";
            this.step1Label.Size = new System.Drawing.Size(204, 13);
            this.step1Label.TabIndex = 2;
            this.step1Label.Text = "1. Download drivers by clicking below link";
            // 
            // driverLinkLabel
            // 
            this.driverLinkLabel.AutoSize = true;
            this.driverLinkLabel.Location = new System.Drawing.Point(79, 86);
            this.driverLinkLabel.Name = "driverLinkLabel";
            this.driverLinkLabel.Size = new System.Drawing.Size(379, 13);
            this.driverLinkLabel.TabIndex = 3;
            this.driverLinkLabel.TabStop = true;
            this.driverLinkLabel.Text = "http://www.lsi.com/Search/Pages/results.aspx?k=symmpi_wXP_1201800.ZIP";
            this.driverLinkLabel.LinkClicked += new System.Windows.Forms.LinkLabelLinkClickedEventHandler(this.driverLinkLabel_LinkClicked);
            // 
            // step2Label
            // 
            this.step2Label.AutoSize = true;
            this.step2Label.Location = new System.Drawing.Point(39, 109);
            this.step2Label.Name = "step2Label";
            this.step2Label.Size = new System.Drawing.Size(87, 13);
            this.step2Label.TabIndex = 4;
            this.step2Label.Text = "2. Extract .zip file";
            // 
            // step3Label
            // 
            this.step3Label.AutoSize = true;
            this.step3Label.Location = new System.Drawing.Point(39, 141);
            this.step3Label.Name = "step3Label";
            this.step3Label.Size = new System.Drawing.Size(302, 13);
            this.step3Label.TabIndex = 5;
            this.step3Label.Text = "3. Point to File Browser to symmpi.sys file in extracted directory.";
            // 
            // noteLabel
            // 
            this.noteLabel.AutoSize = true;
            this.noteLabel.Location = new System.Drawing.Point(12, 173);
            this.noteLabel.Name = "noteLabel";
            this.noteLabel.Size = new System.Drawing.Size(256, 13);
            this.noteLabel.TabIndex = 6;
            this.noteLabel.Text = "Note: File Browser will be opened once you click Ok.";
            // 
            // okButton
            // 
            this.okButton.Location = new System.Drawing.Point(277, 196);
            this.okButton.Name = "okButton";
            this.okButton.Size = new System.Drawing.Size(75, 23);
            this.okButton.TabIndex = 7;
            this.okButton.Text = "Ok";
            this.okButton.UseVisualStyleBackColor = true;
            this.okButton.Click += new System.EventHandler(this.okButton_Click);
            // 
            // cancelbutton
            // 
            this.cancelbutton.Location = new System.Drawing.Point(368, 196);
            this.cancelbutton.Name = "cancelbutton";
            this.cancelbutton.Size = new System.Drawing.Size(75, 23);
            this.cancelbutton.TabIndex = 8;
            this.cancelbutton.Text = "Cancel";
            this.cancelbutton.UseVisualStyleBackColor = true;
            this.cancelbutton.Click += new System.EventHandler(this.cancelbutton_Click);
            // 
            // MessageBoxForm
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.BackColor = System.Drawing.Color.White;
            this.ClientSize = new System.Drawing.Size(489, 229);
            this.ControlBox = false;
            this.Controls.Add(this.cancelbutton);
            this.Controls.Add(this.okButton);
            this.Controls.Add(this.noteLabel);
            this.Controls.Add(this.step3Label);
            this.Controls.Add(this.step2Label);
            this.Controls.Add(this.driverLinkLabel);
            this.Controls.Add(this.step1Label);
            this.Controls.Add(this.followDescriptionLabel);
            this.Controls.Add(this.description1Label);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedSingle;
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.Name = "MessageBoxForm";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
            this.Text = "Windows XP 32 bit scsi drivers..";
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Label description1Label;
        private System.Windows.Forms.Label followDescriptionLabel;
        private System.Windows.Forms.Label step1Label;
        private System.Windows.Forms.LinkLabel driverLinkLabel;
        private System.Windows.Forms.Label step2Label;
        private System.Windows.Forms.Label step3Label;
        private System.Windows.Forms.Label noteLabel;
        private System.Windows.Forms.Button okButton;
        private System.Windows.Forms.Button cancelbutton;
    }
}