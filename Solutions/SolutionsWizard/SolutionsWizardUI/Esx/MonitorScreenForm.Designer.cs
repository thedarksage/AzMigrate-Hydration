namespace com.InMage.Wizard
{
    partial class MonitorScreenForm
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
       // private System.ComponentModel.IContainer components = null;

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
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(MonitorScreenForm));
            this.webBrowserForCx = new System.Windows.Forms.WebBrowser();
            this.SuspendLayout();
            // 
            // webBrowserForCx
            // 
            this.webBrowserForCx.AccessibleRole = System.Windows.Forms.AccessibleRole.MenuBar;
            this.webBrowserForCx.Dock = System.Windows.Forms.DockStyle.Fill;
            this.webBrowserForCx.Location = new System.Drawing.Point(0, 0);
            this.webBrowserForCx.MinimumSize = new System.Drawing.Size(20, 20);
            this.webBrowserForCx.Name = "webBrowserForCx";
            this.webBrowserForCx.Size = new System.Drawing.Size(848, 586);
            this.webBrowserForCx.TabIndex = 0;
            // 
            // MonitorScreenForm
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.BackColor = System.Drawing.Color.White;
            this.ClientSize = new System.Drawing.Size(848, 586);
            this.Controls.Add(this.webBrowserForCx);
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.Name = "MonitorScreenForm";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
            this.Text = "MonitorScreen";
            this.ResumeLayout(false);

        }

        #endregion

        public System.Windows.Forms.WebBrowser webBrowserForCx;

    }
}