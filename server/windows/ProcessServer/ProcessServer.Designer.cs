namespace ProcessServer
{
    partial class ProcessServer
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
            if (disposing)
            {
                if (m_volsync != null)
                {
                    m_volsync.Dispose();
                }

                if (m_cts != null)
                {
                    m_cts.Dispose();
                }

                if (components != null)
                {
                    components.Dispose();
                }

                // Task disposal may not be needed. Refer https://blogs.msdn.microsoft.com/pfxteam/2012/03/25/do-i-need-to-dispose-of-tasks/
                /*
                if(m_psTask != null)
                {
                    m_psTask.Dispose();
                }
                 * */
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
            components = new System.ComponentModel.Container();
            this.ServiceName = Resources.ServiceName;
            this.AutoLog = true;
            this.CanHandlePowerEvent = false;
            this.CanHandleSessionChangeEvent = false;
            this.CanPauseAndContinue = false;
            this.CanShutdown = true;
            this.CanStop = true;
        }

        #endregion
    }
}
