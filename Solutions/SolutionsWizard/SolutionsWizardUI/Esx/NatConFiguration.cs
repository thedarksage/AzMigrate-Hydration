using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Collections;
using System.Data;
using System.IO;
using System.Drawing;
using System.Text;
using System.Windows.Forms;
using Com.Inmage.Tools;
using System.Net;
using System.Diagnostics;

namespace Com.Inmage.Wizard
{
    public partial class NatConFigurationForm : Form
    {
        internal bool canceled = false;
        internal static int HeadingColumn = 0;
       
        internal static int LocalIPcolumn = 1;
        internal static int NatIPcolumn = 2;
        string ip = null;
        internal static ArrayList processserverList = new ArrayList();
        internal HostList selectedList = new HostList();
        internal string installedPath;
        internal static string NatHelpid = "13";
        ArrayList selectedProcessServers = new ArrayList();
        public NatConFigurationForm(HostList selectedSourceList)
        {

            selectedList = selectedSourceList;
            InitializeComponent();
            installedPath = WinTools.FxAgentPath() + "\\vContinuum";
           // tableLayoutPanel.RowStyles.Add(new RowStyle(SizeType.Absolute,70f));
            processserverList = new ArrayList();
            Nat nat = new Nat();       
            Nat.GetPSLocalNatIP(ref processserverList);

            selectedProcessServers = new ArrayList();
            Trace.WriteLine(DateTime.Now + "\t Printing count of list " + selectedList._hostList.Count.ToString());
            foreach (Host h in selectedList._hostList)
            {
                if (h.processServerIp != null)
                {
                    if (selectedProcessServers.Count == 0)
                    {
                        selectedProcessServers.Add(h.processServerIp);
                    }
                    else
                    {
                        bool processServerAlreadyExists = false;
                        foreach(string s in selectedProcessServers)
                        {
                            if(s== h.processServerIp)
                            {
                                processServerAlreadyExists = true;
                                break;
                            }
                        }
                        if(processServerAlreadyExists == false)
                        {
                            selectedProcessServers.Add(h.processServerIp);
                        }
                    }
                }
            }
            Trace.WriteLine(DateTime.Now + "\t Selected process servers list " + selectedProcessServers.Count.ToString());
            //tableLayoutPanel.Scroll 
            
            //WinPreReqs win = new WinPreReqs("", "", "", "");
            //win.SetHostIPinputhostname(Environment.MachineName,  ip, WinTools.GetcxIPWithPortNumber());
            //ip = WinPreReqs.GetIPaddress;
            //natDataGridView.Rows.Add(1);
            //natDataGridView.Rows[0].Cells[HeadingColumn].Value = "vCon";
            //natDataGridView.Rows[0].Cells[LocalIPcolumn].Value = ip;
            //natDataGridView.Rows[0].Cells[NatIPcolumn].ReadOnly = true;

            // Before filling all the values in datagrid view first we are verifying selected process servers in list 
         
            int index =0;
            foreach (Hashtable h  in processserverList)
            {
                foreach (string s in selectedProcessServers)
                {
                    if (h["localip"] != null && s == h["localip"].ToString())
                    {
                        natDataGridView.Rows.Add(1);
                        natDataGridView.Rows[index].Cells[HeadingColumn].Value = "PS";
                        natDataGridView.Rows[index].Cells[LocalIPcolumn].Value = h["localip"].ToString();
                        if (h["natip"] != null)
                        {
                            h["needNatIP"] = false;
                            natDataGridView.Rows[index].Cells[NatIPcolumn].Value = h["natip"].ToString();
                            //natDataGridView.Rows[index].Cells[NatIPcolumn].ReadOnly = true;
                        }
                        else
                        {
                            h["needNatIP"] = true;
                        }
                        index++;
                    }
                   
                }
                
            }
        }

        private void cancelButton_Click(object sender, EventArgs e)
        {
            canceled = true;
            Close();
        }

        private void addButton_Click(object sender, EventArgs e)
        {
            if (PreReqs() == true)
            {
                Close();
            }
        }

        private bool PreReqs()
        {
            try
            {             
              
                foreach(Hashtable h in processserverList)
                {
                    for (int i = 0; i < natDataGridView.RowCount;i++ )
                    {
                        Trace.WriteLine(DateTime.Now + " \t Came here to compare the ps " + h["localip"].ToString() + " datagridvalues " + natDataGridView.Rows[i].Cells[LocalIPcolumn].Value.ToString());
                        if (h["localip"].ToString() == natDataGridView.Rows[i].Cells[LocalIPcolumn].Value.ToString())
                        {
                            Trace.WriteLine(DateTime.Now + "\t Cane here that both the ps ips are same " + h["localip"].ToString());
                            if (natDataGridView.Rows[i].Cells[NatIPcolumn].Value != null)
                            {
                                Trace.WriteLine(DateTime.Now + "\t Came here to assing the vlaues of nat ip for the ps " + natDataGridView.Rows[i].Cells[NatIPcolumn].Value.ToString());
                                h["natip"] = natDataGridView.Rows[i].Cells[NatIPcolumn].Value.ToString();
                            }
                            else
                            {
                                MessageBox.Show("Enter NAT IP for the following PS: " + h["localip"].ToString(), "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                return false;
                            }
                        }
                    }
                }
                foreach (Hashtable h in processserverList)
                {
                    if (selectedProcessServers.Count != 0)
                    {
                        foreach (string s in selectedProcessServers)
                        {
                            if (h["localip"] != null && s == h["localip"].ToString())
                            {
                                if (h["natip"] == null)
                                {
                                    MessageBox.Show("Enter NAT IP for the following PS: " + h["localip"].ToString(), "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                    return false;
                                }
                                System.Net.IPAddress ipAddress = null;
                                bool isValidIp = System.Net.IPAddress.TryParse(h["natip"].ToString(), out ipAddress);
                                if (isValidIp == false)
                                {
                                    MessageBox.Show("Enter a valid IP address", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                                    return false;
                                }
                            }
                        }
                    }
                }
                

                foreach (Hashtable h in processserverList)
                {
                  //  if (bool.Parse(h["needNatIP"].ToString()) == true)
                    {
                       // Nat nat = new Nat();
                        //Before updating ips to cx just verifying both the ips are not null.
                        if (h["localip"] != null && h["natip"] != null)
                        {
                            Nat.SetPSNatIP(h["localip"].ToString(), h["natip"].ToString());
                        }
                    }
                }

                    
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            return true;
        }

       

       
        

       
        private void natConfigurationLinkLabel_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
        {
            if(File.Exists(installedPath + "\\Manual.chm"))
            Help.ShowHelp(null, installedPath + "\\Manual.chm", HelpNavigator.TopicId, NatHelpid);
        }

        private void natDataGridView_CurrentCellDirtyStateChanged(object sender, EventArgs e)
        {
            if (natDataGridView.RowCount > 0)
            {
                natDataGridView.CommitEdit(DataGridViewDataErrorContexts.Commit);
            }
        }

        private void natDataGridView_CellValueChanged(object sender, DataGridViewCellEventArgs e)
        {
            
        }      
    }
}
