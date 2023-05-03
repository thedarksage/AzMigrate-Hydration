using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using Kusto.Cloud.Platform.Data;
using Kusto.Data.Common;
using Kusto.Data.Net.Client;
using System.Collections.Concurrent;

namespace Microsoft.Internal.SiteRecovery.ASRDiagnostic
{
    public partial class Main : Form
    {
        public string DirPath = "";
        public Main()
        {
            InitializeComponent();
        }

        private void button1_Click(object sender, EventArgs e)
        {
            if (textBox1.Text == "")
                MessageBox.Show("Please Enter Customer Subscription ID!", "Error", MessageBoxButtons.OK);
            else if(DirPath == "")
                MessageBox.Show("Please select a folder where logs will be dumped!", "Error", MessageBoxButtons.OK);
            else
            {
                string StartTime = dateTimePicker1.Value.ToString("yyyy-MM-dd HH:mm:ss");
                string EndTime = dateTimePicker2.Value.ToString("yyyy-MM-dd HH:mm:ss");
                Progress ProgressForm = new Progress(textBox1.Text,textBox2.Text,textBox3.Text,StartTime,EndTime,DirPath);
                ProgressForm.Show();
            }
        }

        private void Form1_Load(object sender, EventArgs e)
        {
            dateTimePicker1.Value = DateTime.Now.AddDays(-7);
            dateTimePicker2.Value = DateTime.Now;
        }

        private void label2_Click(object sender, EventArgs e)
        {

        }

        private void label1_Click(object sender, EventArgs e)
        {

        }

        private void Form1_FormClosing(object sender, FormClosingEventArgs e)
        {
            DialogResult quit=MessageBox.Show("Do you really want to exit?","Exit",MessageBoxButtons.YesNo);
            if (quit == DialogResult.Yes)
                Application.ExitThread();
            else
                e.Cancel = true;

        }

        private void textBox1_TextChanged(object sender, EventArgs e)
        {

        }

        private void dateTimePicker1_ValueChanged(object sender, EventArgs e)
        {
        }

        private void button2_Click(object sender, EventArgs e)
        {
            FolderBrowserDialog fbd = new FolderBrowserDialog();
            fbd.ShowDialog();
            DirPath = fbd.SelectedPath;
        }
    }
}
