using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;

namespace Com.Inmage.Wizard
{
    public partial class MessageBoxForAll : Form
    {
        internal System.Drawing.Bitmap errorIcon;
        internal System.Drawing.Bitmap question;
        internal bool selectedyes = false;
        public MessageBoxForAll(string message, string iconmessage, string heading)
        {
            
            InitializeComponent();
            messageLabel.Text = message;
            errorIcon = Wizard.Properties.Resources.errorIcon;
            question = Wizard.Properties.Resources.question;
          
            if (iconmessage == "question")
            {
                pictureBox.Image = question;
            }
            else if(iconmessage  == "error" )
            {
                pictureBox.Image = errorIcon;
            }
            this.Text = heading;
            this.BringToFront();
            this.Focus();
            
          

        }

        private void yesButton_Click(object sender, EventArgs e)
        {
            selectedyes = true;
            Close();
        }

        private void noButton_Click(object sender, EventArgs e)
        {
            selectedyes = false;
            Close();
        }
    }
}
