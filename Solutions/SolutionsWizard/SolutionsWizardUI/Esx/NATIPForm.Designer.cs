namespace com.InMage.Wizard
{
    partial class NATIPForm
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
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(NATIPForm));
            this.natIPLabel = new System.Windows.Forms.Label();
            this.natIPTextBox = new System.Windows.Forms.TextBox();
            this.addButton = new System.Windows.Forms.Button();
            this.cancelButton = new System.Windows.Forms.Button();
            this.portNumberLabel = new System.Windows.Forms.Label();
            this.portNumberTextBox = new System.Windows.Forms.TextBox();
            this.SuspendLayout();
            // 
            // natIPLabel
            // 
            this.natIPLabel.AutoSize = true;
            this.natIPLabel.BackColor = System.Drawing.Color.Transparent;
            this.natIPLabel.Location = new System.Drawing.Point(8, 15);
            this.natIPLabel.Name = "natIPLabel";
            this.natIPLabel.Size = new System.Drawing.Size(109, 13);
            this.natIPLabel.TabIndex = 0;
            this.natIPLabel.Text = "Enter NAT Ip address";
            // 
            // natIPTextBox
            // 
            this.natIPTextBox.Location = new System.Drawing.Point(223, 12);
            this.natIPTextBox.Name = "natIPTextBox";
            this.natIPTextBox.Size = new System.Drawing.Size(100, 20);
            this.natIPTextBox.TabIndex = 1;
            this.natIPTextBox.KeyPress += new System.Windows.Forms.KeyPressEventHandler(this.natIPTextBox_KeyPress);
            // 
            // addButton
            // 
            this.addButton.Location = new System.Drawing.Point(178, 76);
            this.addButton.Name = "addButton";
            this.addButton.Size = new System.Drawing.Size(75, 23);
            this.addButton.TabIndex = 2;
            this.addButton.Text = "Add";
            this.addButton.UseVisualStyleBackColor = true;
            this.addButton.Click += new System.EventHandler(this.addButton_Click);
            // 
            // cancelButton
            // 
            this.cancelButton.Location = new System.Drawing.Point(259, 76);
            this.cancelButton.Name = "cancelButton";
            this.cancelButton.Size = new System.Drawing.Size(75, 23);
            this.cancelButton.TabIndex = 3;
            this.cancelButton.Text = "Cancel";
            this.cancelButton.UseVisualStyleBackColor = true;
            this.cancelButton.Click += new System.EventHandler(this.cancelButton_Click);
            // 
            // portNumberLabel
            // 
            this.portNumberLabel.AutoSize = true;
            this.portNumberLabel.Location = new System.Drawing.Point(11, 49);
            this.portNumberLabel.Name = "portNumberLabel";
            this.portNumberLabel.Size = new System.Drawing.Size(67, 13);
            this.portNumberLabel.TabIndex = 4;
            this.portNumberLabel.Text = "Port number ";
            this.portNumberLabel.Visible = false;
            // 
            // portNumberTextBox
            // 
            this.portNumberTextBox.Location = new System.Drawing.Point(223, 47);
            this.portNumberTextBox.Name = "portNumberTextBox";
            this.portNumberTextBox.Size = new System.Drawing.Size(100, 20);
            this.portNumberTextBox.TabIndex = 5;
            this.portNumberTextBox.Visible = false;
            this.portNumberTextBox.KeyPress += new System.Windows.Forms.KeyPressEventHandler(this.portNumberTextBox_KeyPress);
            // 
            // NATIPForm
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.BackColor = System.Drawing.Color.White;
            this.ClientSize = new System.Drawing.Size(346, 104);
            this.ControlBox = false;
            this.Controls.Add(this.portNumberTextBox);
            this.Controls.Add(this.portNumberLabel);
            this.Controls.Add(this.cancelButton);
            this.Controls.Add(this.addButton);
            this.Controls.Add(this.natIPTextBox);
            this.Controls.Add(this.natIPLabel);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedSingle;
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.Name = "NATIPForm";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
            this.Text = "NAT IP address";
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        public System.Windows.Forms.TextBox natIPTextBox;
        private System.Windows.Forms.Button addButton;
        private System.Windows.Forms.Button cancelButton;
        public System.Windows.Forms.Label portNumberLabel;
        public System.Windows.Forms.TextBox portNumberTextBox;
        public System.Windows.Forms.Label natIPLabel;
    }
}