using System;
using System.Collections.Generic;
using System.Collections;
using System.ComponentModel;
using System.Diagnostics;
using System.Data;
using System.Drawing;
using System.Text;
using System.IO;
using System.Windows.Forms;
using System.Xml;
using System.Xml.Serialization;
using Com.Inmage.Esxcalls;
using System.Web;
using Com.Inmage.Tools;

namespace Com.Inmage.Wizard
{
    public partial class EditingMasterXmlFile : Form
    {
        internal static string masterConfingFile = "MasterConfigFile.xml";
        internal string targetesxIP = null;
        internal string userName = null;
        internal string password = null;
        internal HostList sourceList = new HostList();
        internal HostList masterList = new HostList();
        
        Esx esxInfo = new Esx();
        internal string cxIP = null;
        internal string esxIP = null;
        ArrayList IPlist = new ArrayList();
        internal int returnValue;
        string masterFileName = null;
        bool uploadMasterConfigFile = false;
        public EditingMasterXmlFile()
        {
            InitializeComponent();
            SolutionConfig config = new SolutionConfig();
           // config.ReadXmlConfigFileWithMasterList(MASTER_CONFIG_FILE, ref _sourceList, ref _masterList,ref _esxIp, ref _cxIp);
           /* chooseESXIPComboBox.Items.Clear();
            foreach (Host h in _sourceList._hostList)
            {
                if (_ipList.Count == 0)
                {
                    _ipList.Add(h.targetESXIp);
                }
                else
                {
                    bool ipExists = false;
                    foreach (string ip in _ipList)
                    {
                        Trace.WriteLine(DateTime.Now + "\t\t Comparing the ips " + h.esxIp + " " + ip);
                        if (h.targetESXIp == ip)
                        {
                            Trace.WriteLine(DateTime.Now + " \t Came here to add ip " + ip);
                            ipExists = true;
                        }
                    }
                    if (ipExists == false)
                    {
                        Trace.WriteLine(DateTime.Now + " \t Came here to add ip " + h.esxIp);
                        _ipList.Add(h.targetESXIp);
                    }
                }
            }
            foreach (string ip in _ipList)
            {
                chooseESXIPComboBox.Items.Add(ip);
            }*/
            string xmlFormatText = null;
            StreamReader reader = new StreamReader(WinTools.FxAgentPath() + "\\vContinuum\\Latest\\" + masterConfingFile);
             xmlFormatText = reader.ReadToEnd();
            if (xmlFormatText != null)
            {
                xmlEditTextBox.AppendText(xmlFormatText + Environment.NewLine);
            }
            reader.Close();
          
        }

        private void cancelButton_Click(object sender, EventArgs e)
        {
            ResumeForm.breifLog.WriteLine(DateTime.Now + " \t Closing the EditMAsterConfigFile wizard");
            ResumeForm.breifLog.Flush();
            Close();
        }

        private void backgroundWorker_DoWork(object sender, DoWorkEventArgs e)
        {
           /* _returnValue = _esxInfo.UploadFile(_esxIp, _userName, _password, masterFileName);
            if (_returnValue == 3)
            {
                SourceEsxDetailsPopUpForm source = new SourceEsxDetailsPopUpForm();
                source.sourceEsxIpText.ReadOnly = false;
                source.sourceEsxIpText.Text = _esxIp;
                if (source.canceled == false)
                {
                    _esxIp = source.sourceEsxIpText.Text;
                    _userName = source.userNameText.Text;
                    _password = source.passWordText.Text;
                    _returnValue = _esxInfo.UploadFile(_esxIp, _userName, _password, masterFileName);
                    if (_returnValue != 0)
                    {
                        MessageBox.Show("Failed to upload master file.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return;
                    }
                }
                else
                {
                    return;
                }
            }*/
            MasterConfigFile master = new MasterConfigFile();
            if (master.UploadMasterConfigFile(WinTools.GetcxIPWithPortNumber()) == true)
            {
                ResumeForm.breifLog.WriteLine(DateTime.Now + " \t Successfully updated the Configuration files");
                ResumeForm.breifLog.Flush();
                uploadMasterConfigFile = true;
            }
            else
            {
                ResumeForm.breifLog.WriteLine(DateTime.Now + " \t Failed to updated the Configuration files");
                ResumeForm.breifLog.Flush();
            }

        }

        private void backgroundWorker_ProgressChanged(object sender, ProgressChangedEventArgs e)
        {
            progressBar.Value = e.ProgressPercentage;
        }

        private void backgroundWorker_RunWorkerCompleted(object sender, RunWorkerCompletedEventArgs e)
        {
            progressBar.Visible = false;
            this.Enabled = true;
            if (uploadMasterConfigFile == true)
            {
                
                saveButton.Visible = false;
                cancelButton.Text = "Done";
            }
        }

        private void getDetailsButton_Click(object sender, EventArgs e)
        {
           /* if (esxIPTextBox.Text.Length != 0)
            {
                _targetESXIp = esxIPTextBox.Text;
            }
            else
            {
                MessageBox.Show("Enter target ESX IP", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }
            if (esxUserNameTextBox.Text.Length != 0)
            {
                _userName = esxUserNameTextBox.Text;
            }
            else
            {
                MessageBox.Show("Enter target ESX username", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }
            if (esxPasswordTextBox.Text.Length != 0)
            {
                _password = esxPasswordTextBox.Text;
            }
            else
            {
                MessageBox.Show("Enter target ESX password", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }

            progressBar.Visible = true;
            backgroundWorker.RunWorkerAsync();*/

        }

        private void chooseESXIPComboBox_SelectedValueChanged(object sender, EventArgs e)
        {
            xmlEditTextBox.Clear();
            xmlEditTextBox.WordWrap = true;
            xmlEditTextBox.AppendText("<start>" + Environment.NewLine);
            //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
            //based on above comment we have added xmlresolver as null
            XmlDocument documentMasterEsx = new XmlDocument();
            documentMasterEsx.XmlResolver = null;
            //StreamReader reader = new StreamReader(WinTools.FxAgentPath() + "\\vContinuum\\Latest\\" + masterConfingFile);
            //string xmlString = reader.ReadToEnd();
            //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
            XmlReaderSettings settings = new XmlReaderSettings();
            settings.ProhibitDtd = true;
            settings.XmlResolver = null;
            using (XmlReader reader2 = XmlReader.Create((WinTools.FxAgentPath() + "\\vContinuum\\Latest\\" + masterConfingFile), settings))
            {
                documentMasterEsx.Load(reader2);
               // reader2.Close();
            }
           // documentMasterEsx.Load(WinTools.FxAgentPath() + "\\vContinuum\\Latest\\" + masterConfingFile);
            //reader.Close();
            XmlNodeList targetESXMasterXml = null, hostnodes = null;
            targetESXMasterXml = documentMasterEsx.GetElementsByTagName("TARGET_ESX");
            hostnodes = documentMasterEsx.GetElementsByTagName("host");
           
             foreach (XmlNode esxnode in hostnodes)
             {
                 esxIP = chooseESXIPComboBox.SelectedItem.ToString();

                 if (esxnode.Attributes["esxIp"].Value == chooseESXIPComboBox.SelectedItem.ToString())
                 {
                     if (esxnode.ParentNode.Name == "TARGET_ESX" )
                     {
                         if (esxnode.ParentNode.ParentNode.Name == "root")
                         {
                             Trace.WriteLine(DateTime.Now + " \t Came here to add text to textbox box");
                             string node = esxnode.ParentNode.ParentNode.OuterXml.ToString();
                             if (File.Exists(WinTools.FxAgentPath() + "\\vContinuum\\Latest\\temp.xml"))
                             {
                                 try
                                 {
                                     File.Delete(WinTools.FxAgentPath() + "\\vContinuum\\Latest\\temp.xml");
                                 }
                                 catch (Exception ex)
                                 {
                                     Trace.WriteLine(DateTime.Now + "\t Failed to delete existing file");
                                 }
                             }
                             //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
                             //based on above comment we have added xmlresolver as null
                             //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
                             System.Xml.XmlDocument doc = new System.Xml.XmlDocument();
                             doc.XmlResolver = null;
                             using (XmlReader reader3 = XmlReader.Create(node, settings))
                             {
                                 documentMasterEsx.Load(reader3);
                               //  reader3.Close();
                             }
                             //doc.Load(node);
                             doc.Save(WinTools.FxAgentPath() + "\\vContinuum\\Latest\\temp.xml");
                             StreamReader reader1 = new StreamReader(WinTools.FxAgentPath() + "\\vContinuum\\Latest\\temp.xml");
                             string xmlFormat = reader1.ReadToEnd();
                             xmlEditTextBox.AppendText(xmlFormat + Environment.NewLine);
                             reader1.Close();
                         }
                     }
                 }
             }            
             xmlEditTextBox.AppendText("</start>");
            

        }
        internal bool AppendXml(string tempFile)
        {
            try
            {


               // XmlTextReader xmlTextReader = new XmlTextReader(masterConfingFile);
                // Load the source of the XML file into an XmlDocument
                //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
                //based on above comment we have added xmlresolver as null
                XmlDocument mySourceDocumnetFile = new XmlDocument();
                mySourceDocumnetFile.XmlResolver = null;
                // Load the source XML file into the first document
                //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
                XmlReaderSettings settings = new XmlReaderSettings();
                settings.ProhibitDtd = true;
                settings.XmlResolver = null;
                using (XmlReader reader1 = XmlReader.Create(masterConfingFile + WinPreReqs.tempXml, settings))
                {
                    mySourceDocumnetFile.Load(reader1);
                    //reader1.Close();
                }

               // mySourceDocumnetFile.Load(masterConfingFile);
                // Close the reader
                //xmlTextReader.Close();
               // xmlTextReader = new XmlTextReader(masterConfingFile);

                // Load the source of the XML file into an XmlDocument
                //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
                XmlDocument myDestinationDocument = new XmlDocument();
                myDestinationDocument.XmlResolver = null;
                // Load the destination XML file into the first document
                using (XmlReader reader2 = XmlReader.Create(masterConfingFile, settings))
                {
                    myDestinationDocument.Load(reader2);
                    //reader2.Close();
                }
                //myDestinationDocument.Load(masterConfingFile);

                // Close the reader

               // xmlTextReader.Close();

                XmlNode rootNodeDest = myDestinationDocument["start"];


                // Store the node to be copied into an XmlNode

                XmlNodeList nodeOrigList = mySourceDocumnetFile["start"].ChildNodes;
                foreach (XmlNode nodeOrig in nodeOrigList)
                {

                    // Store the copy of the original node into an XmlNode

                    XmlNode nodeDest = myDestinationDocument.ImportNode(nodeOrig, true);

                    // Append the node being copied to the root of the destination document

                    rootNodeDest.AppendChild(nodeDest);
                }

                XmlTextWriter xmlTextWritter = new XmlTextWriter(tempFile, Encoding.UTF8);

                // Indented for easy reading

                xmlTextWritter.Formatting = Formatting.Indented;

                // Write the file

                myDestinationDocument.WriteTo(xmlTextWritter);

                // Close the writer

                xmlTextWritter.Close();


               
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
        private void saveButton_Click(object sender, EventArgs e)
        {
            if (File.Exists(WinTools.FxAgentPath() + "\\vcontinuum\\Latest\\" + masterConfingFile))
            {
                File.Delete(WinTools.FxAgentPath() + "\\vContinuum\\Latest\\" + masterConfingFile);
            }

            FileInfo f1 = new FileInfo(WinTools.FxAgentPath() + "\\vContinuum\\Latest\\" + masterConfingFile);
            StreamWriter sw = f1.AppendText();
            
            string line = xmlEditTextBox.Text;
            sw.WriteLine(line);
            sw.Close();
            
            this.Enabled = false;
            progressBar.Visible = true;
            backgroundWorker.RunWorkerAsync();        
        }        
    }
}
