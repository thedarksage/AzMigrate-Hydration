namespace EsxLoginControl
{
    partial class EsxLoginControl
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

        #region Component Designer generated code

        /// <summary> 
        /// Required method for Designer support - do not modify 
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this.components = new System.ComponentModel.Container();
            this.esxHostIpLabel = new System.Windows.Forms.Label();
            this.userNameLabel = new System.Windows.Forms.Label();
            this.passWordLabel = new System.Windows.Forms.Label();
            this.esxHostIpText = new System.Windows.Forms.TextBox();
            this.userNameText = new System.Windows.Forms.TextBox();
            this.passWordText = new System.Windows.Forms.TextBox();
            this.addServerErrorProvider = new System.Windows.Forms.ErrorProvider(this.components);
            ((System.ComponentModel.ISupportInitialize)(this.addServerErrorProvider)).BeginInit();
            this.SuspendLayout();
            // 
            // esxHostIpLabel
            // 
            this.esxHostIpLabel.AutoSize = true;
            this.esxHostIpLabel.Location = new System.Drawing.Point(14, 12);
            this.esxHostIpLabel.Name = "esxHostIpLabel";
            this.esxHostIpLabel.Size = new System.Drawing.Size(116, 13);
            this.esxHostIpLabel.TabIndex = 0;
            this.esxHostIpLabel.Text = "ESX Server IP Address";
            // 
            // userNameLabel
            // 
            this.userNameLabel.AutoSize = true;
            this.userNameLabel.Location = new System.Drawing.Point(133, 11);
            this.userNameLabel.Name = "userNameLabel";
            this.userNameLabel.Size = new System.Drawing.Size(58, 13);
            this.userNameLabel.TabIndex = 1;
            this.userNameLabel.Text = "User name";
            // 
            // passWordLabel
            // 
            this.passWordLabel.AutoSize = true;
            this.passWordLabel.Location = new System.Drawing.Point(241, 12);
            this.passWordLabel.Name = "passWordLabel";
            this.passWordLabel.Size = new System.Drawing.Size(53, 13);
            this.passWordLabel.TabIndex = 2;
            this.passWordLabel.Text = "Password";
            // 
            // esxHostIpText
            // 
            this.esxHostIpText.Location = new System.Drawing.Point(19, 28);
            this.esxHostIpText.Name = "esxHostIpText";
            this.esxHostIpText.Size = new System.Drawing.Size(100, 20);
            this.esxHostIpText.TabIndex = 3;
            // 
            // userNameText
            // 
            this.userNameText.Location = new System.Drawing.Point(135, 28);
            this.userNameText.Name = "userNameText";
            this.userNameText.Size = new System.Drawing.Size(100, 20);
            this.userNameText.TabIndex = 4;
            // 
            // passWordText
            // 
            this.passWordText.Location = new System.Drawing.Point(244, 28);
            this.passWordText.Name = "passWordText";
            this.passWordText.Size = new System.Drawing.Size(100, 20);
            this.passWordText.TabIndex = 5;
            this.passWordText.TextChanged += new System.EventHandler(this.passWordText_TextChanged);
            // 
            // addServerErrorProvider
            // 
            this.addServerErrorProvider.ContainerControl = this;
            // 
            // EsxLoginControl
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.BackColor = System.Drawing.Color.White;
            this.Controls.Add(this.passWordText);
            this.Controls.Add(this.userNameText);
            this.Controls.Add(this.esxHostIpText);
            this.Controls.Add(this.passWordLabel);
            this.Controls.Add(this.userNameLabel);
            this.Controls.Add(this.esxHostIpLabel);
            this.Name = "EsxLoginControl";
            this.Size = new System.Drawing.Size(354, 61);
            ((System.ComponentModel.ISupportInitialize)(this.addServerErrorProvider)).EndInit();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Label esxHostIpLabel;
        private System.Windows.Forms.Label userNameLabel;
        private System.Windows.Forms.Label passWordLabel;
        public System.Windows.Forms.TextBox esxHostIpText;
        public System.Windows.Forms.TextBox userNameText;
        public System.Windows.Forms.TextBox passWordText;
        public System.Windows.Forms.ErrorProvider addServerErrorProvider;
    }
}
