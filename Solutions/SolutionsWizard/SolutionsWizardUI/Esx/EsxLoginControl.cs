using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Data;
using System.Text;
using System.Windows.Forms;

namespace EsxLoginControl
{
    public partial class EsxLoginControl : UserControl
    {
        public EsxLoginControl()
        {
            InitializeComponent();
            
        }

        private void passWordText_TextChanged(object sender, EventArgs e)
        {
            passWordText.PasswordChar = '*';
        }
    }
}
