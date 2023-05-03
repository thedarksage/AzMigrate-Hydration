using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using Com.Inmage.Esxcalls;
using Com.Inmage.Tools;
using System.Windows.Forms;
using System.Text.RegularExpressions;
using System.Xml;


namespace Com.Inmage.Wizard
{
    class DisplayLUNSPanelHandler: PanelHandler
    {
        public DisplayLUNSPanelHandler()
        {

        }

        internal override bool Initialize(Com.Inmage.Wizard.AllServersForm allServersForm)
        {
           
            

           // Host masterHostofTarget = (Host)allServersForm._esx.GetHostList()[0];

           //// allServersForm.displayDatastoresTreeView.Nodes.Clear();
           // //a//llServersForm.displayDatastoresTreeView.Nodes.Add("Datastores");
           // foreach (DataStore d in masterHostofTarget.dataStoreList)
           // {
           //     if (d.vSpherehostname == allServersForm._masterHost.vSpherehost)
           //     {
           //         if (d.disk_name == null)
           //         {
           //             allServersForm.displayDatastoresTreeView.Nodes[0].Nodes.Add(d.name);
           //         }
           //         else if (d.disk_name != null && d.disk_name != "vContinuum999#")
           //         {
           //             allServersForm.displayDatastoresTreeView.Nodes[0].Nodes.Add(d.name);
           //         }
           //     }
           // }

           // if (allServersForm.displayDatastoresTreeView.Nodes.Count != 0)
           // {
           //     allServersForm.displayDatastoresTreeView.ExpandAll();
           //     if (allServersForm.displayDatastoresTreeView.Nodes[0].Nodes.Count != 0)
           //     {

           //         TreeNodeCollection nodes = allServersForm.displayDatastoresTreeView.Nodes[0].Nodes;

           //         allServersForm.displayDatastoresTreeView.SelectedNode = nodes[0];
           //     }
           // }


           // Trace.WriteLine(DateTime.Now + "\t Entered Initialize of DisplayLUNSPanelHandler.cs");
           // Trace.WriteLine(DateTime.Now + "\t Exiting Initialize of DisplayLUNSPanelHandler.cs");
            return true;
        }

        internal bool SelectedDatastore(AllServersForm allServersForm, string selectedDataStore)
        {
            try
            {
                //allServersForm.lunDeatilsDataGridView.Rows.Clear();
                //Host masterHostofTarget = (Host)allServersForm._esx.GetHostList()[0];
                //allServersForm.datastoreDescriptionLabel.Text = "LUNS on datastore: " + selectedDataStore;
                //foreach (DataStore d in masterHostofTarget.dataStoreList)
                //{
                //    if (d.vSpherehostname == allServersForm._masterHost.vSpherehost)
                //    {
                //        if (d.name == selectedDataStore)
                //        {
                //            if (d.disk_name != null)
                //            {
                //                if (d.disk_name.Contains(","))
                //                {
                //                    string[] disknames = d.disk_name.Split(',');
                //                    int i = 0;
                //                    foreach (string s in disknames)
                //                    {
                //                        allServersForm.lunDeatilsDataGridView.Rows.Add(1);
                //                        allServersForm.lunDeatilsDataGridView.Rows[i].Cells[0].Value = s;
                //                        i++;
                //                    }
                //                    if (d.vendor.Contains(","))
                //                    {
                //                        string[] vendors = d.vendor.Split(',');
                //                        if (disknames.Length == vendors.Length)
                //                        {
                //                            for (int j = 0; j < allServersForm.lunDeatilsDataGridView.RowCount; j++)
                //                            {
                //                                allServersForm.lunDeatilsDataGridView.Rows[j].Cells[1].Value = vendors[j].ToString();
                //                            }
                //                        }
                //                    }
                //                    if (d.display_name.Contains(","))
                //                    {
                //                        string[] displaynames = d.display_name.Split(',');
                //                        if (disknames.Length == displaynames.Length)
                //                        {
                //                            for (int j = 0; j < allServersForm.lunDeatilsDataGridView.RowCount; j++)
                //                            {
                //                                allServersForm.lunDeatilsDataGridView.Rows[j].Cells[2].Value = displaynames[j].ToString();
                //                            }
                //                        }
                //                    }
                //                }
                //                else
                //                {
                //                    allServersForm.lunDeatilsDataGridView.Rows.Add(1);
                //                    allServersForm.lunDeatilsDataGridView.Rows[0].Cells[0].Value = d.disk_name;
                //                    allServersForm.lunDeatilsDataGridView.Rows[0].Cells[1].Value = d.vendor;
                //                    allServersForm.lunDeatilsDataGridView.Rows[0].Cells[2].Value = d.display_name;
                //                }
                //            }
                //        }
                //    }
                //}
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: SelectedDatastore of DisplayLUNSPanelHandler.cs " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
            return true;
        }


        internal override bool ProcessPanel(AllServersForm allServersForm)
        {
            Trace.WriteLine(DateTime.Now + "\t Entered ProcessPanel of DisplayLUNSPanelHandler.cs");
            Trace.WriteLine(DateTime.Now + "\t Exiting ProcessPanel of DisplayLUNSPanelHandler.cs");
            return true;
        }

        internal override bool ValidatePanel(AllServersForm allServersForm)
        {
            Trace.WriteLine(DateTime.Now + "\t Entered ValidatePanel of DisplayLUNSPanelHandler.cs");
            Trace.WriteLine(DateTime.Now + "\t Exiting ValidatePanel of DisplayLUNSPanelHandler.cs");
            return true;
        }

        internal override bool CanGoToNextPanel(AllServersForm allServersForm)
        {
            
            Trace.WriteLine(DateTime.Now + "\t Entered CanGoToNextPanel of DisplayLUNSPanelHandler.cs");
            Trace.WriteLine(DateTime.Now + "\t Exiting CanGoToNextPanel of DisplayLUNSPanelHandler.cs");
            return true;
        }

        internal override bool CanGoToPreviousPanel(AllServersForm allServersForm)
        {
            //if (allServersForm.arrayBasedDrDrillRadioButton.Checked == true)
            //{
            //    allServersForm.previousButton.Visible = false;
            //    allServersForm.previousButton.Enabled = true;
            //    allServersForm.nextButton.Enabled = true;
            //    allServersForm.nextButton.Visible = true;
            //}
            Trace.WriteLine(DateTime.Now + "\t Entered CanGoToPreviousPanel of DisplayLUNSPanelHandler.cs");
            Trace.WriteLine(DateTime.Now + "\t Exiting CanGoToPreviousPanel of DisplayLUNSPanelHandler.cs");
            return true;
        }
    }
}
