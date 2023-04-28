using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Web;
using System.Windows.Forms;
using System.Collections;
using System.Diagnostics;
using System.IO;

using System.Text.RegularExpressions;
using System.Xml;
using System.Threading;
using Com.Inmage.Esxcalls;
using Com.Inmage.Tools;
namespace Com.Inmage.Wizard
{
    internal partial class ImportOfflineSyncForm : Form
    {
        internal int indexArray = 0;
        internal ArrayList panelList = new ArrayList();
        internal ArrayList panelHandlerList = new ArrayList();
        internal HostList selSourceList = new HostList();
        ArrayList pictureBoxList = new ArrayList();
        private int taskListIndex = 1;
        internal Host masterHost = new Host();
        internal HostList finMasterList = new HostList();
        internal string cxIPused;
        internal Esx esxglobal = new Esx();
        System.Drawing.Bitmap currentTask;
        System.Drawing.Bitmap completeTask;
        internal string hostWhereMasterTargetRestored = "";
        internal string dataStoreWhereMTisThere = " ";
        internal OStype operatingsysType = OStype.Windows;
        internal string esxIPused = "";
        internal string esxsUserName = "";
        internal string esxsPassword = "";
        internal string tempDataStore = "";
        internal bool slideOpen = false;
        internal string installedPath = "";
        internal bool uploadedFileSuccessfully = false;
        internal string latestPathInstalled = null;
        internal System.Drawing.Bitmap scoutHeading;
        public ImportOfflineSyncForm()
        {
            InitializeComponent();
            try
            {
                scoutHeading = Wizard.Properties.Resources.scoutheading;
                if (HelpForcx.Brandingcode == 1)
                {
                    headingPanel.BackgroundImage = scoutHeading;
                }
                installedPath = WinTools.FxAgentPath() + "\\vContinuum";
                latestPathInstalled = WinTools.FxAgentPath() + "\\vContinuum\\Latest";
                OfflineSync_TargetDetailsPanelHandler targetDetailsPanelHandler = new OfflineSync_TargetDetailsPanelHandler();

                helpContentLabel.Text = HelpForcx.OfflineSyncImport;

                completeTask = Wizard.Properties.Resources.doneIcon;

                panelList.Add(targetDetailsPanel);
                // _panelList.Add(SelectDataStorePanel);
                // _panelList.Add(processPanel);

                panelHandlerList.Add(targetDetailsPanelHandler);
                // _panelHandlerList.Add(selectDataStorePanelHandler);
                //_panelHandlerList.Add(processPanelHandler);

                pictureBoxList.Add(credentialPictureBox);



                startApp();
                // This is only when the patch is installed on the vcon box.
                // We will search for the patch log and then we will display this.
                System.Windows.Forms.ToolTip toolTipToDisplayPatch = new System.Windows.Forms.ToolTip();
                toolTipToDisplayPatch.AutoPopDelay = 5000;
                toolTipToDisplayPatch.InitialDelay = 1000;
                toolTipToDisplayPatch.ReshowDelay = 500;
                toolTipToDisplayPatch.ShowAlways = true;
                toolTipToDisplayPatch.GetLifetimeService();
                toolTipToDisplayPatch.IsBalloon = false;
                toolTipToDisplayPatch.UseAnimation = true;
                toolTipToDisplayPatch.SetToolTip(versionLabel, HelpForcx.BuildDate);
                versionLabel.Text = HelpForcx.VersionNumber;
                if (File.Exists(WinTools.FxAgentPath()+ "\\vContinuum" + "\\patch.log"))
                {
                    patchLabel.Visible = true;
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }
        private void startApp()
        {
            try
            {

                if (panelList.Count != panelHandlerList.Count)
                {
                    Trace.WriteLine(DateTime.Now + "\t ImportOfflineSyncForms: startApp: Panel Handler count it not matching with Panels.");
                }
                OfflineSyncPanelHandler panelHandler = (OfflineSyncPanelHandler)panelHandlerList[indexArray];
                panelHandler.Initialize(this);
                ((System.Windows.Forms.Panel)panelList[indexArray]).BringToFront();
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");

            }

        }

        private void cancelButton_Click(object sender, EventArgs e)
        {
            Close();
        }

        private void nextButton_Click(object sender, EventArgs e)
        {
            try
            {
                if (indexArray < (panelList.Count))
                {

                    {
                        OfflineSyncPanelHandler panelHandler = (OfflineSyncPanelHandler)panelHandlerList[indexArray];

                        if (panelHandler.ValidatePanel(this) == true)
                        {
                            if (panelHandler.ProcessPanel(this) == true)
                            {


                                //Move to next panel if all states of current panel are done
                                if (panelHandler.CanGoToNextPanel(this) == true)
                                {
                                    //Update the task progress on left side
                                    ((System.Windows.Forms.PictureBox)pictureBoxList[taskListIndex - 1]).Visible = true;
                                    if (indexArray < (panelList.Count - 1))
                                    {
                                        ((System.Windows.Forms.PictureBox)pictureBoxList[taskListIndex - 1]).Image = completeTask;
                                        ((System.Windows.Forms.PictureBox)pictureBoxList[taskListIndex]).Visible = true;
                                        ((System.Windows.Forms.PictureBox)pictureBoxList[taskListIndex]).Image = currentTask;

                                        ((System.Windows.Forms.Panel)panelList[indexArray]).SendToBack();
                                        taskListIndex++;
                                        indexArray++;

                                        Console.WriteLine(" current index is :" + indexArray);
                                        panelHandler = (OfflineSyncPanelHandler)panelHandlerList[indexArray];
                                        panelHandler.Initialize(this);
                                        ((System.Windows.Forms.Panel)panelList[indexArray]).BringToFront();
                                    }
                                }

                            }

                        }
                    }
                }

                OfflineSync_TargetDetailsPanelHandler targetDetailspanel = (OfflineSync_TargetDetailsPanelHandler)panelHandlerList[indexArray];
                targetDetailspanel.OfflineSyncImport(this);

            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");


            }

        }

        private void previousButton_Click(object sender, EventArgs e)
        {
            try
            {
                OfflineSyncPanelHandler panelHandler;

                if (indexArray > 0)
                {

                    if (taskListIndex > 0)
                    {

                        panelHandler = (OfflineSyncPanelHandler)panelHandlerList[indexArray];


                        if (panelHandler.CanGoToPreviousPanel(this) == true)
                        {
                            ((System.Windows.Forms.Panel)panelList[indexArray]).SendToBack();

                            indexArray--;
                            taskListIndex--;

                            ((System.Windows.Forms.Panel)panelList[indexArray]).BringToFront();
                            panelHandler = (OfflineSyncPanelHandler)panelHandlerList[indexArray];
                            ((System.Windows.Forms.PictureBox)pictureBoxList[taskListIndex]).Visible = false;
                            ((System.Windows.Forms.PictureBox)pictureBoxList[taskListIndex - 1]).Image = currentTask;

                            Debug.WriteLine(" current index is :" + indexArray);
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


            }
        }

        private void secondaryServerPanelAddEsxButton_Click(object sender, EventArgs e)
        {
            try
            {
                if (File.Exists(latestPathInstalled + "\\Info.xml"))
                {
                    File.Delete(latestPathInstalled + "\\Info.xml");

                }


                OfflineSync_TargetDetailsPanelHandler targetDetailsPanelHandler = (OfflineSync_TargetDetailsPanelHandler)panelHandlerList[indexArray];
                secondaryServerPanelAddEsxButton.Enabled = false;
                nextButton.Enabled = false;
                targetDetailsPanelHandler.GetEsxMasterFile(this);
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");


            }
        }

        private void getTargetDetailsBackgroundWorker_DoWork(object sender, DoWorkEventArgs e)
        {
            try
            {
                OfflineSync_TargetDetailsPanelHandler target = (OfflineSync_TargetDetailsPanelHandler)panelHandlerList[indexArray];

                target.GetOfflineSyncXmlMasterXml(this);
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");


            }

        }

        private void getTargetDetailsBackgroundWorker_ProgressChanged(object sender, ProgressChangedEventArgs e)
        {
            progressBar.Value = e.ProgressPercentage;

        }

        private void getTargetDetailsBackgroundWorker_RunWorkerCompleted(object sender, RunWorkerCompletedEventArgs e)
        {
            try
            {
                OfflineSync_TargetDetailsPanelHandler target = (OfflineSync_TargetDetailsPanelHandler)panelHandlerList[indexArray];
                target.LoadIntoDataGridView(this);
                secondaryServerPanelAddEsxButton.Enabled = true;
                nextButton.Enabled = true;
                progressBar.Visible = false;
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");


            }
          

        }

        private void targetScriptBackgroundWorker_DoWork(object sender, DoWorkEventArgs e)
        {
            OfflineSync_TargetDetailsPanelHandler offline = (OfflineSync_TargetDetailsPanelHandler)panelHandlerList[indexArray];
            offline.TargetScriptForImport(this);
        }

        private void targetScriptBackgroundWorker_ProgressChanged(object sender, ProgressChangedEventArgs e)
        {
            progressBar.Value = e.ProgressPercentage;


        }

        private void targetScriptBackgroundWorker_RunWorkerCompleted(object sender, RunWorkerCompletedEventArgs e)
        {
            if (uploadedFileSuccessfully == true)
            {

                nextButton.Visible = false;
                cancelButton.Visible = false;
                progressBar.Visible = false;
                doneButton.Visible = true;
            }
            else
            {
                nextButton.Enabled = true;
                cancelButton.Enabled = true;
            }
            progressBar.Visible = false;

        }

        private void doneButton_Click(object sender, EventArgs e)
        {
            this.Close();
        }

        private void closePictureBox_Click(object sender, EventArgs e)
        {
            helpPanel.Visible = false;
            slideOpen = false;
        }

        private void helpPictureBox_Click(object sender, EventArgs e)
        {
            try
            {
                if (slideOpen == false)
                {

                    helpPanel.BringToFront();
                    helpPanel.Visible = true;
                    slideOpen = true;
                }
                else
                {
                    slideOpen = false;
                    helpPanel.Visible = false;
                }
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");


            }
        }

        private void esxProtectionLinkLabel_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
        {
            try
            {
                //Process.Start("http://" + WinTools.GetcxIPWithPortNumber() + "/help/Content/ESX Solution/Offline Sync.htm");
                if (File.Exists(installedPath + "\\Manual.chm"))
                {
                    Help.ShowHelp(null, installedPath + "\\Manual.chm", HelpNavigator.TopicId, "72");
                }
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void versionLabel_MouseEnter(object sender, EventArgs e)
        {
            try
            {
                System.Windows.Forms.ToolTip toolTipToDisplaybuildDate = new System.Windows.Forms.ToolTip();
                toolTipToDisplaybuildDate.AutoPopDelay = 5000;
                toolTipToDisplaybuildDate.InitialDelay = 1000;
                toolTipToDisplaybuildDate.ReshowDelay = 500;
                toolTipToDisplaybuildDate.ShowAlways = true;
                toolTipToDisplaybuildDate.GetLifetimeService();
                toolTipToDisplaybuildDate.IsBalloon = false;
                toolTipToDisplaybuildDate.UseAnimation = true;
                toolTipToDisplaybuildDate.SetToolTip(versionLabel, HelpForcx.BuildDate);
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
        }

        private void historyLinkLabel_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
        {
            /*try
            {
                if (File.Exists(Directory.GetCurrentDirectory() + " \\patch.log"))
                {
                    //System.Diagnostics.Process.Start("patch.log");
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }*/
        }

        private void patchLabel_MouseEnter(object sender, EventArgs e)
        {
            if (File.Exists(WinTools.FxAgentPath()+ "\\vContinuum" + " \\patch.log"))
            {
                System.Windows.Forms.ToolTip toolTipToDisplayPatchVersion = new System.Windows.Forms.ToolTip();
                toolTipToDisplayPatchVersion.AutoPopDelay = 5000;
                toolTipToDisplayPatchVersion.InitialDelay = 1000;
                toolTipToDisplayPatchVersion.ReshowDelay = 500;
                toolTipToDisplayPatchVersion.ShowAlways = true;
                toolTipToDisplayPatchVersion.GetLifetimeService();
                toolTipToDisplayPatchVersion.IsBalloon = true;

                toolTipToDisplayPatchVersion.UseAnimation = true;
                StreamReader sr = new StreamReader(WinTools.FxAgentPath()+ "\\vContinuum" + " \\patch.log");

                string s = sr.ReadToEnd().ToString();
                toolTipToDisplayPatchVersion.SetToolTip(patchLabel, s);
                sr.Close();

            }
        }

        private void masterHostComboBox_SelectedValueChanged(object sender, EventArgs e)
        {
            try
            {
                if (masterHostComboBox.SelectedItem != null)
                {
                    string host = masterHostComboBox.SelectedItem.ToString();
                    masterTargetDataStoreComboBox.Items.Clear();
                    if (esxglobal.GetHostList.Count != 0)
                    {
                        // 4750308 [ASR Scout]vContinnum offlinesync import is not listing datastores.
                        foreach (DataStore d in esxglobal.dataStoreList)
                        {
                            if (masterHostComboBox.SelectedItem.ToString() == d.vSpherehostname)
                            {
                                masterTargetDataStoreComboBox.Items.Add(d.name);
                            }
                        }
                    }

                }
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Caught exception at masterHostComboBox_SelectedValueChanged " + ex.Message);
            }
        }

        private void masterTargetDataStoreComboBox_Click(object sender, EventArgs e)
        {
            if (masterHostComboBox.SelectedItem == null)
            {
                MessageBox.Show("Select Master target Host server first", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

       

        
    }
}
