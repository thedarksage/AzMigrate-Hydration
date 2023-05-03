using System;
using System.Collections.Generic;
using System.Collections;
using System.Text;
using System.Diagnostics;
using com.InMage.ESX;
using System.IO;
using System.Windows.Forms;
using System.Threading;
using System.Xml;
using com.InMage.Tools;
using System.Drawing;
namespace com.InMage.Wizard
{
    class ProcessPanelHandler : OfflineSyncPanelHandler
    {
        public static long TIME_OUT_MILLISECONDS = 360000;
        bool _targetESXPreChekCompleted = false;
        
        public ProcessPanelHandler()
        {

        }

        public override bool Initialize(ImportOfflineSyncForm importOfflineSyncForm)
        {
            XmlDocument document = new XmlDocument();
            string xmlfile = Directory.GetCurrentDirectory() + "\\ESX.xml";
            document.Load(xmlfile);
            //XmlNodeList hostNodes = null;
            XmlNodeList hostNodes = null;
            XmlNodeList diskNodes = null;
            hostNodes = document.GetElementsByTagName("host");
            diskNodes = document.GetElementsByTagName("disk");
            foreach (XmlNode node in diskNodes)
            {
                node.Attributes["protected"].Value = "no";
            }

            document.Save(Directory.GetCurrentDirectory() + "\\ESX.xml");
            return true;
        }

        public override bool ProcessPanel(ImportOfflineSyncForm importOfflineSyncForm)
        {
           // importOfflineSyncForm.progressBar.Visible = true;
            Esx esx = new Esx();
            Process p = null;
            importOfflineSyncForm.generalLogTextBox.AppendText("Running pre-req checks on Target Esx... " + Environment.NewLine);
            if (_targetESXPreChekCompleted == false)
            {
                if (TargetReadynessCheck(importOfflineSyncForm) == false)
                {

                    return false;
                }
                else
                {
                    _targetESXPreChekCompleted = true;
                    importOfflineSyncForm.generalLogTextBox.AppendText("Pre-req checks completed on Target Esx... " + Environment.NewLine);
                }

            }
            if (_targetESXPreChekCompleted == true)
            {
                if (esx.TargetScriptForOffliceSyncImport(importOfflineSyncForm.masterHost.esxIp, importOfflineSyncForm.masterHost.esxUserName, importOfflineSyncForm.masterHost.esxPassword, "\"" + "[" + importOfflineSyncForm.targetDataStore + "]" + "\"", "\"" + "[" + importOfflineSyncForm.vmdkDataStore + "]" + "\"") == 0)
                {
                    SolutionConfig config = new SolutionConfig();

                    XmlDocument document = new XmlDocument();
                    string xmlfile = Directory.GetCurrentDirectory() + "\\ESX.xml";
                    document.Load(xmlfile);
                    //XmlNodeList hostNodes = null;
                    XmlNodeList hostNodes = null;
                    XmlNodeList diskNodes = null;
                    hostNodes = document.GetElementsByTagName("host");
                    diskNodes = document.GetElementsByTagName("disk");
                    foreach (XmlNode node in diskNodes)
                    {
                        node.Attributes["protected"].Value = "yes";
                    }

                    document.Save(Directory.GetCurrentDirectory() + "\\ESX.xml");
                    config.AddOrReplaceMasterXmlFile(importOfflineSyncForm._selectedSourceList, importOfflineSyncForm.masterHost, importOfflineSyncForm._cxIP, "ESX_Master.xml", "yes");
                    esx.UploadFileForOfflineSync(importOfflineSyncForm.masterHost.esxIp, importOfflineSyncForm.masterHost.esxUserName, importOfflineSyncForm.masterHost.esxPassword, "ESX_Master.xml", "\"" + "[" + importOfflineSyncForm.targetDataStore + "]" + "\"");
                    importOfflineSyncForm.nextButton.Visible = false;
                    importOfflineSyncForm.previousButton.Visible = false;
                    importOfflineSyncForm.cancelButton.Visible = false;
                    importOfflineSyncForm.progressBar.Visible = false;
                    importOfflineSyncForm.doneButton.Visible = true;

                }
            }
           // importOfflineSyncForm.targetScriptBackgroundWorker.RunWorkerAsync();
          //  Esx esx = new Esx();

           
            
            return true;
        }
        public bool TargetReadynessCheck(ImportOfflineSyncForm importOfflineSyncForm)
        {
            bool timedOut = false;
            Process p;
            Esx esx = new Esx();
            int exitCode;




            for (int j = 0; j < importOfflineSyncForm._finalMasterList._hostList.Count; j++)
            {
                Host masterHost = (Host)importOfflineSyncForm._finalMasterList._hostList[j];

                p = esx.PreReqCheck(masterHost.esxIp, masterHost.esxUserName, masterHost.esxPassword);

                exitCode = WaitForProcess(p, importOfflineSyncForm, importOfflineSyncForm.progressBar, TIME_OUT_MILLISECONDS, ref timedOut);

                if (exitCode != 0)
                {
                    updateLogTextBox(importOfflineSyncForm.generalLogTextBox);
                    return false;
                }
            }



            return true;
        }

        public int WaitForProcess(Process p, ImportOfflineSyncForm importOfflineSyncForm, ProgressBar prgoressBar, long maxWaitTime, ref bool timedOut)
        {


            long waited = 0;
            int incWait = 100;
            int exitCode = 1;
            int i = 10;
            //90 Seconds


            importOfflineSyncForm.Enabled = false;
            importOfflineSyncForm.progressBar.Visible = true;
            importOfflineSyncForm.progressBar.Value = i;

            waited = 0;
            while (p.HasExited == false)
            {

                if (waited <= TIME_OUT_MILLISECONDS)
                {
                    p.WaitForExit(incWait);
                    waited = waited + incWait;
                    importOfflineSyncForm.progressBar.Value = i;
                    importOfflineSyncForm.progressBar.Update();
                    importOfflineSyncForm.progressBar.Refresh();
                    importOfflineSyncForm.Refresh();

                }
                else
                {
                    if (p.HasExited == false)
                    {
                        Trace.WriteLine("Pre req checks failed ");
                        timedOut = true;
                        exitCode = 1;
                        break;


                    }
                }
            }


            if (timedOut == false)
            {
                exitCode = p.ExitCode;
            }



            importOfflineSyncForm.progressBar.Visible = false;

            importOfflineSyncForm.Enabled = true;
            Cursor.Current = Cursors.Default;

            return exitCode;
        }
        private bool updateLogTextBox(TextBox generalLogTextBox)
        {
            StreamReader sr1 = new StreamReader(Directory.GetCurrentDirectory() + "\\Esx_Protect_Log.txt");

            //StreamReader sr = new StreamReader(_installPath+"\\Apm" + "\\JobAutomation.txt");
            string firstLine1;
            firstLine1 = sr1.ReadToEnd();

            generalLogTextBox.AppendText(firstLine1);

            sr1.Close();
            sr1.Dispose();

            return true;

        }
        public bool RunTargetScript(ImportOfflineSyncForm importOfflineSyncForm)
        {
           
             
            return true;
        }

        public override bool ValidatePanel(ImportOfflineSyncForm importOfflineSyncForm)
        {
            return true;
        }
        public override bool CanGoToNextPanel(ImportOfflineSyncForm importOfflineSyncForm)
        {
            return true;

        }

        public override bool CanGoToPreviousPanel(ImportOfflineSyncForm importOfflineSyncForm)
        {

            return true;

        }
    }
}
