namespace Com.Inmage.Wizard
{
    partial class LaunchPreReqsForm
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
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(LaunchPreReqsForm));
            this.launchBackgroundWorker = new System.ComponentModel.BackgroundWorker();
            this.SuspendLayout();
            // 
            // launchBackgroundWorker
            // 
            this.launchBackgroundWorker.DoWork += new System.ComponentModel.DoWorkEventHandler(this.launchBackgroundWorker_DoWork);
            this.launchBackgroundWorker.RunWorkerCompleted += new System.ComponentModel.RunWorkerCompletedEventHandler(this.launchBackgroundWorker_RunWorkerCompleted);
            this.launchBackgroundWorker.ProgressChanged += new System.ComponentModel.ProgressChangedEventHandler(this.launchBackgroundWorker_ProgressChanged);
            // 
            // LaunchPreReqsForm
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.BackColor = System.Drawing.Color.White;
            this.BackgroundImage = global::Com.Inmage.Wizard.Properties.Resources.Launching;
            this.BackgroundImageLayout = System.Windows.Forms.ImageLayout.Center;
            this.ClientSize = new System.Drawing.Size(223, 110);
            this.ControlBox = false;
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.None;
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.Name = "LaunchPreReqsForm";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterScreen;
            this.Text = "vContinuum";
            this.Load += new System.EventHandler(this.LaunchPreReqsForm_Load);
            this.ResumeLayout(false);

        }

        #endregion

        internal System.ComponentModel.BackgroundWorker launchBackgroundWorker;

    }
}