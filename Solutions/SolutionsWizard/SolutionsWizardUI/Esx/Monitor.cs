using System;
using System.Collections.Generic;
using System.Collections;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;

using System.Runtime.InteropServices;
using System.Runtime.Serialization;
using System.Diagnostics;
using System.Threading;
using System.IO;
using Com.Inmage.Tools;

namespace Com.Inmage.Wizard
{
    public partial class Monitor : Form
    {
        private delegate void TickerDelegate(string s);
        TickerDelegate tickerDelegate;
        ArrayList planList = new ArrayList();
        Host selectedPlanHost = new Host();
        internal System.Drawing.Bitmap currentTask;
        internal System.Drawing.Bitmap completeTask;
        internal System.Drawing.Bitmap pending;
        internal System.Drawing.Bitmap failed;
        internal System.Drawing.Bitmap warning;
        internal bool isInProgress = false;
        internal DataGridView statusGrid = new DataGridView();
        internal Label labelforBackground = new Label();
        internal string logPathFromApi = null;
        internal bool makeDatastoreNotVisible = false;
        internal string labelMessage = null;
        internal string planIdFromApi = null;
        int currentRowofDatagrid = 0;
        public Monitor()
        {
            InitializeComponent();
            statusGrid = statusDataGridView;
            labelforBackground = statusLabel;
            currentTask = Wizard.Properties.Resources.PROCESS_PROCESSING;
            completeTask = Wizard.Properties.Resources.tick;
            pending = Wizard.Properties.Resources.pending;
            failed = Wizard.Properties.Resources.cross;
            warning = Wizard.Properties.Resources.warning;
            planList = new ArrayList();
            Cxapicalls cxApi = new Cxapicalls();
            cxApi.PostToGetAllPlans( planList);
            planList = Cxapicalls.GetArrayList;
            int i = 0;

            foreach (Hashtable hash in planList)
            {
                if (hash["PlanName"] != null && hash["PlanId"] != null && hash["PlanType"] != null)
                {
                    if (planNamesTreeView.Nodes.Count == 0)
                    { 
                        
                        planNamesTreeView.Nodes.Add(hash["PlanType"].ToString());
                        planNamesTreeView.Nodes[0].Nodes.Add(hash["PlanName"].ToString());
                        i++;
                        //HideCheckBox(planNamesTreeView, planNamesTreeView.Nodes[0]);
                    }
                    else
                    {
                        bool planExists = false;
                        foreach (TreeNode node in planNamesTreeView.Nodes)
                        {
                            // Trace.WriteLine(DateTime.Now + " \t printing the node names " + node.Name + " plan name " + h.plan);
                            if (node.Text == hash["PlanType"].ToString())
                            {
                                planExists = true;
                                node.Nodes.Add(hash["PlanName"].ToString());
                                
                            }
                        }
                        if (planExists == false)
                        {planNamesTreeView.Nodes.Add(hash["PlanType"].ToString());
                            int count = planNamesTreeView.Nodes.Count;
                            
                            planNamesTreeView.Nodes[i].Nodes.Add(hash["PlanName"].ToString());
                            //HideCheckBox(planNamesTreeView, planNamesTreeView.Nodes[count-1]);
                            i++;
                        }
                    }
                }
            }

            if (planNamesTreeView.Nodes.Count == 0)
            {
                planNamesTreeView.Nodes.Add("No plans found");
            }
            else
            {
                planNamesTreeView.ExpandAll();
            }
            try
            {
                //if (File.Exists("C:\\windows\\temp\\InMage_Recovered_Vms.rollback"))
                //{
                //    File.Delete("C:\\windows\\temp\\InMage_Recovered_Vms.rollback");
                //}
                if (File.Exists("C:\\windows\\temp\\vCon_Error.log"))
                {
                    File.Delete("C:\\windows\\temp\\vCon_Error.log");
                }
                if (File.Exists(WinTools.FxAgentPath() + "\\vContinuum\\logs\\vCon_Error.log"))
                {
                    File.Delete(WinTools.FxAgentPath() + "\\vContinuum\\logs\\vCon_Error.log");
                }
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed to delete file form windows temp folder " + ex.Message);
            }

        }

        private void closeButton_Click(object sender, EventArgs e)
        {
            if (backgroundWorker.IsBusy)
            {
                backgroundWorker.CancelAsync();
            }
            Close();
        }

         // These are the local  variable for masking the check box for the parent node...
        private const int TREEVIEW_STATE = 0x8;
        private const int TREEVIEW_STATEIMAGEMASK = 0xF000;
        private const int TREEVIEW_FIRST = 0x1100;
        private const int TREEVIEWMODE_SETITEM = TREEVIEW_FIRST + 63;

        [StructLayout(LayoutKind.Sequential, Pack = 8, CharSet = CharSet.Auto)]
        private struct TVITEM
        {
            public int mask;
            public IntPtr hideItem;
            public int state;
            public int stateMask;
            [MarshalAs(UnmanagedType.LPTStr)]
            public string indexNodeText;
            public int childCheckBoxTextMax;
            public int indexImage;
            public int indexSelectedImage;
            public int checkChildren;
            public IntPtr interParam;
        }

        [DllImport("user32.dll", CharSet = CharSet.Auto)]
        private static extern IntPtr SendMessage(IntPtr hideWindow, int message, IntPtr windowParameter,
                                                 ref TVITEM lParam);

        /// <summary>
        /// Hides the checkbox for the specified node on a TreeView control.
        /// </summary>
        public void HideCheckBox(TreeView treeViewWindow, TreeNode node)
        {
            try
            {
                TVITEM treeViewItem = new TVITEM();
                treeViewItem.hideItem = node.Handle;
                treeViewItem.mask = TREEVIEW_STATE;
                treeViewItem.stateMask = TREEVIEW_STATEIMAGEMASK;
                treeViewItem.state = 0;
                // Trace.WriteLine(DateTime.Now + " \t Printing the node text  " + node.Text);
                SendMessage(treeViewWindow.Handle, TREEVIEWMODE_SETITEM, IntPtr.Zero, ref treeViewItem);
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: HideCheckBox  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");


            }
        }

        private void planNamesTreeView_AfterSelect(object sender, TreeViewEventArgs e)
        {
            try
            {
             
                tickerDelegate = new TickerDelegate(SetLeftTicker);
                if (backgroundWorker.IsBusy)
                {
                    try
                    {
                       
                        logLinkLabel.Visible = false;
                        backgroundWorker.CancelAsync();
                        statusDataGridView.Rows.Clear();
                        statusDataGridView.Visible = false;
                        Cursor = Cursors.WaitCursor;
                        //Thread.Sleep(5000);
                    }
                    catch (Exception ex)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Failed to kill back ground worker " + ex.Message);
                    }
                }
               startButton.Visible = false;
                string planname = null;
                string operation = null;
                if (e.Node.IsSelected)
                {
                    if (e.Node.Parent == null)
                    {
                        statusDataGridView.Visible = false;
                        statusLabel.Visible = false;
                        return;
                    }
                    else
                    {
                        operation = e.Node.Parent.Text;
                        planname = e.Node.Text;
                        if (e.Node.Parent.Text == "Protection")
                        {
                            statusLabel.Text = "Protection status for plan: " + planname;
                           
                        }
                        else if (e.Node.Parent.Text == "Recovery")
                        {
                            statusLabel.Text = "Recovery status for plan: " + planname;
                        }
                        else if (e.Node.Parent.Text == "Resume")
                        {
                            statusLabel.Text = "Resume status for plan: " + planname;
                        }
                        else if (e.Node.Parent.Text == "Failback")
                        {
                            statusLabel.Text = "Failback status for plan: " + planname;
                        }
                        else if (e.Node.Parent.Text == "DrDrill")
                        {
                            statusLabel.Text = "DR Drill status for plan: " + planname;
                        }
                        else if (e.Node.Parent.Text == "Adddisk")
                        {
                            statusLabel.Text = "Adddisk status for plan: " + planname;
                        }
                        else if (e.Node.Parent.Text == "Resize")
                        {
                            statusLabel.Text = "Resize status for the plan: " + planname;
                        }
                        else if (e.Node.Parent.Text == "Remove volume")
                        {
                            statusLabel.Text = "Remove volume status for the plan: " + planname;
                        }
                        else if (e.Node.Parent.Text == "Remove Disk")
                        {
                            statusLabel.Text = "Remove disk status fort the plan: " + planname;
                        }


                        labelMessage = statusLabel.Text;
                        statusLabel.Visible = true;
                    }

                    foreach (Hashtable hash in planList)
                    {
                        if (hash["PlanName"] != null)
                        {
                            if (hash["PlanName"].ToString() == planname)
                            {
                                selectedPlanHost.planid = hash["PlanId"].ToString();
                                selectedPlanHost.plan = hash["PlanName"].ToString();
                                if (operation == "Protection")
                                {
                                    selectedPlanHost.operationType = Com.Inmage.Esxcalls.OperationType.Initialprotection;
                                }
                                else if (operation == "Recovery")
                                {
                                    selectedPlanHost.operationType = Com.Inmage.Esxcalls.OperationType.Recover;
                                }
                                else if (operation == "Resume")
                                {
                                    selectedPlanHost.operationType = Com.Inmage.Esxcalls.OperationType.Resume;
                                }
                                else if (operation == "DrDrill")
                                {
                                    selectedPlanHost.operationType = Com.Inmage.Esxcalls.OperationType.Drdrill;
                                }
                                else if (operation == "Failback")
                                {
                                    selectedPlanHost.operationType = Com.Inmage.Esxcalls.OperationType.Failback;
                                }
                                else if (operation == "Adddisk")
                                {
                                    selectedPlanHost.operationType = Com.Inmage.Esxcalls.OperationType.Additionofdisk;
                                }
                                else if (operation == "Resize")
                                {
                                    selectedPlanHost.operationType = Com.Inmage.Esxcalls.OperationType.Resize; 
                                }
                                else if (operation == "Remove volume")
                                {
                                    selectedPlanHost.operationType = Com.Inmage.Esxcalls.OperationType.Removevolume;
                                }
                                else if (operation == "Remove Disk")
                                {
                                    selectedPlanHost.operationType = Com.Inmage.Esxcalls.OperationType.Removedisk;
                                }

                            }
                        }
                    }
                    //this.Enabled = false;
                    Trace.WriteLine(DateTime.Now + "\t printing backgroundworker value " + backgroundWorker.IsBusy.ToString());
                    if (!backgroundWorker.IsBusy)
                    {
                        Cursor = Cursors.WaitCursor;
                        backgroundWorker.RunWorkerAsync();
                    }
                }
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed in do work " + ex.Message);
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: planNamesTreeView_AfterSelect " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");

            }
        }

        private void backgroundWorker_DoWork(object sender, DoWorkEventArgs e)
        {

            try
            {
                Cxapicalls cxApi = new Cxapicalls();
               
                selectedPlanHost.taskList = new ArrayList();
                cxApi.Post( selectedPlanHost, "MonitorESXProtectionStatus");
                selectedPlanHost = Cxapicalls.GetHost;
                while(selectedPlanHost.taskList.Count == 0)
                {
                    
                    statusLabel.Invoke(tickerDelegate, new object[] { "" });
                    Thread.Sleep(6000);
                    if (backgroundWorker.CancellationPending)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Canceled the backgroundworker");

                        e.Cancel = true;
                        currentRowofDatagrid = -1;
                    }
                    cxApi.Post( selectedPlanHost, "MonitorESXProtectionStatus");
                    selectedPlanHost = Cxapicalls.GetHost;
                }
                if (selectedPlanHost.taskList.Count == 1)
                {
                    foreach (Hashtable hash in selectedPlanHost.taskList)
                    {
                        if (hash["TaskStatus"] != null)
                        {
                            if (hash["TaskStatus"].ToString() == "Queued")
                            {
                                return;
                            }
                        }
                    }                    
                }
                foreach (Hashtable hash in selectedPlanHost.taskList)
                {
                    if (hash["TaskStatus"] != null)
                    {
                        if (hash["TaskStatus"].ToString() == "InProgress")
                        {
                            isInProgress = true;
                            while (isInProgress == true)
                            {
                                statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                                selectedPlanHost.taskList = new ArrayList();

                                if (backgroundWorker.CancellationPending)
                                {
                                    Trace.WriteLine(DateTime.Now + "\t Canceled the backgroundworker");
                                    e.Cancel = true;

                                }
                                Thread.Sleep(5000);
                                if (backgroundWorker.CancellationPending)
                                {
                                    Trace.WriteLine(DateTime.Now + "\t Canceled the backgroundworker");

                                    e.Cancel = true;

                                }
                                isInProgress = false;
                                currentRowofDatagrid = -1;

                                cxApi.Post( selectedPlanHost, "MonitorESXProtectionStatus");
                                selectedPlanHost = Cxapicalls.GetHost;
                                if (backgroundWorker.CancellationPending)
                                {
                                    Trace.WriteLine(DateTime.Now + "\t Canceled the backgroundworker");
                                    e.Cancel = true;
                                }
                                foreach (Hashtable hash1 in selectedPlanHost.taskList)
                                {
                                    if (hash1["TaskStatus"] != null)
                                    {
                                        if (hash1["TaskStatus"].ToString() == "InProgress")
                                        {
                                            statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                                            isInProgress = true;
                                        }
                                        else if (hash1["TaskStatus"].ToString() == "Failed")
                                        {

                                            if (hash1["LogPath"] != null && hash1["TaskStatus"].ToString() == "Failed")
                                            {
                                                logPathFromApi = hash1["LogPath"].ToString();
                                                DownloadRequiredFiles();
                                                statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                                                return;
                                            }
                                            return;
                                        }
                                        statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                                    }

                                }
                            }
                        }
                        else
                        {
                            if (hash["TaskStatus"].ToString() == "Failed")
                            {

                                if (hash["LogPath"] != null && hash["TaskStatus"].ToString() == "Failed")
                                {
                                    logPathFromApi = hash["LogPath"].ToString();
                                    DownloadRequiredFiles();
                                    statusDataGridView.Invoke(tickerDelegate, new object[] { "" });
                                    return;
                                }
                                return;
                            }
                          

                        }

                    }
                }
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed in do work " + ex.Message);
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: backgroundWorker_DoWork " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");

            }

        }

        private void backgroundWorker_ProgressChanged(object sender, ProgressChangedEventArgs e)
        {
            this.Enabled = true;
        }

        private void backgroundWorker_RunWorkerCompleted(object sender, RunWorkerCompletedEventArgs e)
        {
            try
            {
                statusDataGridView.Rows.Clear();
                int rowIndex = 0;
                if (selectedPlanHost.taskList.Count > 1)
                {
                    foreach (Hashtable hash in selectedPlanHost.taskList)
                    {
                        if (hash["Name"] != null)
                        {
                            if (hash["Name"].ToString() != "FX job Status")
                            {
                                statusDataGridView.Rows.Add(1);
                                statusDataGridView.Rows[rowIndex].Cells[1].Value = hash["Name"].ToString();
                                if (hash["TaskStatus"].ToString() == "Completed" || hash["TaskStatus"].ToString() == "Complete")
                                {
                                    statusDataGridView.Rows[rowIndex].Cells[0].Value = completeTask;
                                }
                                else if (hash["TaskStatus"].ToString() == "Failed")
                                {
                                    statusDataGridView.Rows[rowIndex].Cells[0].Value = failed;
                                }
                                else if (hash["TaskStatus"].ToString() == "Queued")
                                {
                                    statusDataGridView.Rows[rowIndex].Cells[0].Value = pending;
                                }
                                else if (hash["TaskStatus"].ToString() == "InProgress")
                                {
                                    statusDataGridView.Rows[rowIndex].Cells[0].Value = currentTask;
                                    currentRowofDatagrid = rowIndex;
                                }
                                else if (hash["TaskStatus"].ToString() == "Warning")
                                {
                                    statusDataGridView.Rows[rowIndex].Cells[0].Value = warning;
                                }
                                rowIndex++;
                            }
                            
                        }
                    }
                }

                if (selectedPlanHost.taskList.Count == 1)
                {
                    foreach (Hashtable hash in selectedPlanHost.taskList)
                    {
                        if (hash["Name"] != null )
                        {
                            if (hash["Name"].ToString() == "FX job Status")
                            {
                                if (hash["TaskStatus"].ToString() == "Queued")
                                {
                                    statusLabel.Text = labelMessage + " : Not Started";
                                    startButton.Visible = true;
                                }
                                else if (hash["TaskStatus"].ToString() == "InProgress")
                                {
                                    statusLabel.Text = labelMessage + " : Started";
                                    startButton.Visible = false;
                                }
                                
                            }
                        }
                    }
                }
                else
                {
                    startButton.Visible = false;
                }
                if (statusDataGridView.RowCount != 0)
                {
                    AnimateImage();
                    ImageAnimator.UpdateFrames();
                    
                    statusDataGridView.Visible = true;
                }
                else
                {
                    currentRowofDatagrid = -1;
                }

                Cursor = Cursors.Default;
                if(logPathFromApi != null)
                {
                    logLinkLabel.Visible = true;
                }
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed in do work " + ex.Message);
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");

            }

        }

        public void AnimateImage()
        {
            try
            {
                Image img = currentTask;
                if (img != null)
                {
                    ImageAnimator.Animate(img, new EventHandler(this.OnFrameChanged));
                }
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed in do work " + ex.Message);
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: AnimateImage " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");

            }
        }

        private void OnFrameChanged(object o, EventArgs e)
        {
            try
            {
                //Force a call to the Paint event handler.
                if (statusDataGridView.RowCount >= currentRowofDatagrid && statusDataGridView.RowCount != 0)
                {
                    if (currentRowofDatagrid != -1)
                    {
                        statusDataGridView.InvalidateCell(statusDataGridView.Rows[currentRowofDatagrid].Cells[0]);
                    }
                }
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed in do work " + ex.Message);
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: OnFrameChanged " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");

            }
            //_datagrid.Invalidate();
        }

        private void statusDataGridView_CellPainting(object sender, DataGridViewCellPaintingEventArgs e)
        {
            try
            {
             
                ImageAnimator.UpdateFrames();
                e.Graphics.DrawImage(currentTask, e.CellBounds.Location);
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed in do work " + ex.Message);
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: statusDataGridView_CellPainting " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");

            }
        }

        private void SetLeftTicker(string s)
        {
            try
            {
                int rowIndex = 0;
                statusGrid.Rows.Clear();
                Trace.WriteLine(DateTime.Now + "\t Count of hash " + selectedPlanHost.taskList.Count.ToString());
                foreach (Hashtable hash in selectedPlanHost.taskList)
                {

                    if (hash["Name"] != null )
                    {
                       
                        if (hash["Name"].ToString() != "FX job Status")
                        {
                            statusGrid.Rows.Add(1);
                            statusGrid.Rows[rowIndex].Cells[1].Value = hash["Name"].ToString();
                            if (hash["TaskStatus"].ToString() == "Completed" || hash["TaskStatus"].ToString() == "Complete")
                            {
                                statusGrid.Rows[rowIndex].Cells[0].Value = completeTask;
                            }
                            else if (hash["TaskStatus"].ToString() == "Failed")
                            {
                                statusGrid.Rows[rowIndex].Cells[0].Value = failed;
                            }
                            else if (hash["TaskStatus"].ToString() == "Queued")
                            {
                                statusGrid.Rows[rowIndex].Cells[0].Value = pending;
                            }
                            else if (hash["TaskStatus"].ToString() == "InProgress")
                            {
                                statusGrid.Rows[rowIndex].Cells[0].Value = currentTask;
                                currentRowofDatagrid = rowIndex;
                            }
                            else if (hash["TaskStatus"].ToString() == "Warning")
                            {
                                statusGrid.Rows[rowIndex].Cells[0].Value = warning;
                            }
                            rowIndex++;
                        }
                        
                    }
                    
                }
                if (statusGrid.RowCount != 0)
                {
                    statusGrid.Visible = true;
                    AnimateImage();
                    ImageAnimator.UpdateFrames();
                    Cursor = Cursors.Default;
                }
                else
                {
                    currentRowofDatagrid = -1;
                }
                if (selectedPlanHost.taskList.Count == 0)
                {
                    statusLabel.Text = labelMessage +" : Not Started";
                }
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed in do work " + ex.Message);
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: SetLeftTicker " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");

            }
        }
        public bool DownloadRequiredFiles()
        {
            try
            {
                if (logPathFromApi != null)
                {
                    try
                    {

                        if (File.Exists("C:\\windows\\temp\\vCon_Error.log"))
                        {
                            File.Delete("C:\\windows\\temp\\vCon_Error.log");
                        }
                        if (File.Exists(WinTools.FxAgentPath() + "\\vContinuum\\logs\\vCon_Error.log"))
                        {
                            File.Delete(WinTools.FxAgentPath() + "\\vContinuum\\logs\\vCon_Error.log");
                        }
                    }
                    catch (Exception ex)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Failed to delete file form windows temp folder " + ex.Message);
                    }
                    if (WinTools.DownloadFileToCX("\"" + logPathFromApi + "\"", "\"" + "C:\\windows\\temp\\vCon_Error.log" + "\"") == 0)
                    {
                        if (File.Exists("C:\\windows\\temp\\vCon_Error.log"))
                        {
                            File.Copy("C:\\windows\\temp\\vCon_Error.log", WinTools.FxAgentPath() + "\\vContinuum\\logs\\vCon_Error.log", true);
                        }
                        
                        Trace.WriteLine(DateTime.Now + "\t Successfully downloaded file form cx vCon_Error.log");
                        return true;
                    }
                    else
                    {

                        Trace.WriteLine(DateTime.Now + "\t Failed to download file from cx");
                        return false;
                    }
                }
                else
                {
                    Trace.WriteLine(DateTime.Now + "\t Log file value is null ");
                }
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed in do work " + ex.Message);
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");

            }
            return true;
        }

       
        private void logLinkLabel_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
        {
            if (File.Exists(WinTools.FxAgentPath() + "\\vContinuum\\logs\\vCon_Error.log"))
            {
                Process.Start("notepad.exe", WinTools.FxAgentPath() + "\\vContinuum\\logs\\vCon_Error.log");
            }
            
        }

       

        private void startButton_Click(object sender, EventArgs e)
        {
            Cxapicalls cxapi = new Cxapicalls();
            Host h = new Host();
            h.planid = selectedPlanHost.planid;
            cxapi.Post( h, "StartFXJob");
            h = Cxapicalls.GetHost;
            startButton.Visible = false;
            statusLabel.Text = labelMessage + " :Started";
            Cursor = Cursors.WaitCursor;
            backgroundWorker.RunWorkerAsync();
        }

    }
}
