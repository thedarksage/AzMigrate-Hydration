using System;
using System.Collections.Generic;
using System.Text;
using Com.Inmage.Tools;
using System.Diagnostics;
using System.Net.NetworkInformation;
using System.IO;
using System.Xml;
using System.Net;
using System.Threading;
using System.Windows.Forms;
using HttpCommunication;

namespace Com.Inmage
{
    public class Remove
    {

        public string xmlstring;
        string SourcehostIP, MastertargethostIP,_vConName="";
        string Sourcehostname, Mastertargethostname, IncxIP, Originalsourcehostname, Originalmastertargethostname, Originalvconhostname;
        public static Host refHost = new Host();
        internal static string latestPath = WinTools.Vxagentpath() + "\\vContinuum\\Latest";
        public Remove()
        {

         
            Trace.WriteLine("Cleanup Constructor called");
        
        }

        public Host GetHost
        {
            get
            {
                return refHost;
            }
            set
            {
                value = refHost;
            }
        }

        public bool RemoveReplication(Host host, string inMasterHostName, string planName, string cxIP, string inVConName)
        {
            try
            {
                // From UI we will call this method  with source info and master targte name and planname
                string[] words = host.hostname.Split('.');
                
                

                //Trimming hostname,master target name and vCon name in case if they are more than 15 characters.
                Sourcehostname = words[0].ToUpper();
                Originalsourcehostname = host.hostname;
               

               /* if (_sourceHostName.Length > 15)
                {
                    _sourceHostName = _sourceHostName.Substring(0, 15).ToUpper();
                }*/
                words = inMasterHostName.Split('.');

                Mastertargethostname = words[0].ToUpper();
                Originalmastertargethostname = inMasterHostName;

                /*if (_master_target_HostName.Length > 15)
                {
                    _master_target_HostName = _master_target_HostName.Substring(0, 15).ToUpper();
                }*/

                // Incase if vCOn name is null we are taking the machine name 
                //Only in case of 5.5 to 6.0 upgrade we will see one issue in remove operation so that we are 
                // taking machine name as vCOn name.
                // This will fix bug # 17797.
                if (inVConName == null)
                {
                    inVConName = Environment.MachineName;
                }
                    words = inVConName.Split('.');
                    _vConName = words[0].ToUpper();
                    Originalvconhostname = inVConName;
                
               /* if (_vConName.Length > 15)
                {
                    _vConName = _vConName.Substring(0, 15).ToUpper();
                
                }*/


                IncxIP = cxIP;

                Trace.WriteLine(DateTime.Now + "\t Called Cleanup of fx and vx jobs for " + host.hostname + "(" + host.ip + ")");

                if (host.delete == true)
                {

                    //set _sourceHostIP,_target_HostIP by using cxcli query.
                    // Here we will get ips from cx for mt and source machines.
                    if (SetSourceAndTargetIPaddress() == false)
                    {

                        Trace.Write(DateTime.Now + "\tUnable to find source target Machines from cxcli query");
                        refHost = host;
                        return false;

                    }



                    //call cxcli to get replication pair associated with host based on plan name
                    Trace.WriteLine("Source host name:" + Sourcehostname + " And IP:" + SourcehostIP);
                    Trace.WriteLine("Target host name:" + Mastertargethostname + " And IP:" + MastertargethostIP);

                    // We will get replication paris from cx and compare with planname.
                    if (GetReplicatinPairsStatus(planName) == true)
                    {
                        //if replication pair status received successfully, then delete the replication pair.

                        if (RemovePairs() == true)
                        {
                            // Trace.WriteLine("Successfully deleted replication pairs from CX");
                            Trace.WriteLine(DateTime.Now + "\t Completed Cleanup of fx and vx jobs for " + host.hostname + "(" + host.ip + ")");
                            refHost = host;
                            return true;
                        }
                        else
                        {
                            //Unable to delete the replication pairs.

                            Trace.WriteLine(DateTime.Now + "\t removing fx and vx jobs failed(but finding the jobs on cx succeeded) on cx for " + host.hostname + "(" + host.ip + ")");
                            refHost = host;
                            return false;
                        }

                    }
                    else
                    {
                        //Return false in case, if we are unable to fetch information from cx.

                        Trace.WriteLine(DateTime.Now + "\t Unable to find fx and vx jobs on cx for " + host.hostname + "(" + host.ip + ")");
                        refHost = host;
                        return false;


                    }

                    //call remove cxcli to remove the replicatin pairs.




                }
                else
                {
                    refHost = host;
                    return false;

                }

            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                System.FormatException formatException = new FormatException("Inner exception", ex);
                Trace.WriteLine(formatException.GetBaseException());
                Trace.WriteLine(formatException.Data);
                Trace.WriteLine(formatException.InnerException);
                Trace.WriteLine(formatException.Message);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
               // return false;

            }

            Trace.WriteLine(DateTime.Now + "\t Completed Cleanup of fx and vx jobs for " + host.hostname + "(" + host.ip + ")");
            refHost = host;
            return true;
        
        }

        internal bool SetSourceAndTargetIPaddress()
        {
            try
            {
                Trace.WriteLine("Entered RemovePairs()...");
              
                xmlstring = "";
                try
                {
                   
                    String postData1 = "&operation=";

                    // xmlData would be send as the value of variable called "xml"
                    // This data would then be wrtitten in to a file callied input.xml
                    // on CX and then given as argument to the cx_cli.php script.
                    int operation = 3;

                    String postData2 = "&userfile='input.xml'&xml=" + xmlstring;


                    String postData = postData1 + operation + postData2;
                    //  Trace.WriteLine(postData);
                    //String postData = "&operation=2";
                    WinPreReqs win = new WinPreReqs("", "", "", "");
                    HttpReply reply = win.ConnectToHttpClient(postData);
                    if (String.IsNullOrEmpty(reply.error))
                    {
                        // process reply
                        if (!String.IsNullOrEmpty(reply.data))
                        {
                            Trace.WriteLine(reply.data);
                        }
                    }
                    else
                    {

                        Trace.WriteLine(reply.error);
                        return false;
                    }
                    string responseFromServer = reply.data;

                  
                    Trace.WriteLine("Printing Response from CX...");
                    Trace.WriteLine(responseFromServer);
                    Trace.WriteLine("proceeding ...");
                    //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
                    //based on above comment we have added xmlresolver as null
                    XmlDocument xDocument = new XmlDocument();
                    xDocument.XmlResolver = null;
                    //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
                    XmlReaderSettings settings = new XmlReaderSettings();
                    settings.ProhibitDtd = true;
                    settings.XmlResolver = null;
                    using (XmlReader reader1 = XmlReader.Create(latestPath + WinPreReqs.tempXml, settings))
                    {
                        xDocument.Load(reader1);
                        //reader1.Close();
                    }

                    //Reading volume replication details from cx

                    XmlNodeList xPNodeList = xDocument.SelectNodes(".//host");

                    foreach (XmlNode node in xPNodeList)
                    {


                        if (node.ChildNodes.Item(0).InnerText == Sourcehostname)
                        {

                            Trace.WriteLine("*******Found Source IP address***********");
                            SourcehostIP = node.ChildNodes.Item(1).InnerText;

                        }
                        if (node.ChildNodes.Item(0).InnerText == Mastertargethostname)
                        {

                            Trace.WriteLine("*******Found Master Target IP address***********");
                            MastertargethostIP = node.ChildNodes.Item(1).InnerText;

                        }



                    }

                    Trace.WriteLine(SourcehostIP + "   " + MastertargethostIP);
                    return true;

                }
                catch (System.Net.WebException e)
                {

                    System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);
                    System.FormatException formatException = new FormatException("Inner exception", e);
                    Trace.WriteLine(formatException.GetBaseException());
                    Trace.WriteLine(formatException.Data);
                    Trace.WriteLine(formatException.InnerException);
                    Trace.WriteLine(formatException.Message);
                    Trace.WriteLine("ERROR*******************************************");
                    Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                    Trace.WriteLine("Exception  =  " + e.Message);
                    Trace.WriteLine("ERROR___________________________________________");

                    //Trace.WriteLine("Faied to delete jobs from CX." + _in_cx_IP);
                    return false;
                }

                catch (System.Xml.XmlException e)
                {


                    System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);
                    System.FormatException formatException = new FormatException("Inner exception", e);
                    Trace.WriteLine(formatException.GetBaseException());
                    Trace.WriteLine(formatException.Data);
                    Trace.WriteLine(formatException.InnerException);
                    Trace.WriteLine(formatException.Message);
                    Trace.WriteLine("ERROR*******************************************");
                    Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                    Trace.WriteLine("Exception  =  " + e.Message);
                    Trace.WriteLine("ERROR___________________________________________");

                    return false;


                }
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                System.FormatException formatException = new FormatException("Inner exception", ex);
                Trace.WriteLine(formatException.GetBaseException());
                Trace.WriteLine(formatException.Data);
                Trace.WriteLine(formatException.InnerException);
                Trace.WriteLine(formatException.Message);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");

                return false;
            }



        }


        private bool RemovePairs()
        {
        
            Trace.WriteLine("Entered RemovePairs()...");
             String url = "";
            

            try
            {
              
                String postData1 = "&operation=";

                // xmlData would be send as the value of variable called "xml"
                // This data would then be wrtitten in to a file callied input.xml
                // on CX and then given as argument to the cx_cli.php script.
                int operation = 45;

                String postData2 = "&userfile='input.xml'&xml=" + xmlstring;


                String postData = postData1 + operation + postData2;
                //  Trace.WriteLine(postData);
                WinPreReqs win = new WinPreReqs("", "", "", "");
                HttpReply reply = win.ConnectToHttpClient(postData);
                if (String.IsNullOrEmpty(reply.error))
                {
                    // process reply
                    if (!String.IsNullOrEmpty(reply.data))
                    {
                        Trace.WriteLine(reply.data);
                    }
                }
                else
                {

                    Trace.WriteLine(reply.error);
                    return false;
                }
                string responseFromServer = reply.data;
                Trace.WriteLine("Printing Response from CX...");
                Trace.WriteLine(responseFromServer.ToString());
                Trace.WriteLine("proceeding ...");
                
                if(responseFromServer.ToString().Trim().Length == 0)
                {
                    Trace.WriteLine("Unable to delete pairs..Cx reponse is blank after the removing replication");

                    return false;
                
                }
               


            }
            catch (System.Net.WebException e)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);
                System.FormatException formatException = new FormatException("Inner exception", e);
                Trace.WriteLine(formatException.GetBaseException());
                Trace.WriteLine(formatException.Data);
                Trace.WriteLine(formatException.InnerException);
                Trace.WriteLine(formatException.Message);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + e.Message);
                Trace.WriteLine("ERROR___________________________________________");

               
                Trace.WriteLine("Faied to delete jobs from CX." + IncxIP);
                return false;
            }

             catch (System.Xml.XmlException e)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);
                System.FormatException formatException = new FormatException("Inner exception", e);
                Trace.WriteLine(formatException.GetBaseException());
                Trace.WriteLine(formatException.Data);
                Trace.WriteLine(formatException.InnerException);
                Trace.WriteLine(formatException.Message);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + e.Message);
                Trace.WriteLine("ERROR___________________________________________");

                
              

                return false;
         
            
            }

            Thread.Sleep(10000);
        
        return true;
        }



        internal bool GetReplicatinPairsStatus(string inPlanName)
        {

            try
            {
                // We will get repliaction pairs with this plan name using cxcli call
                Trace.WriteLine(DateTime.Now + "Printing the plan name " + inPlanName);
                bool replicationExist = false;

                string inPutString = "<plan> " + Environment.NewLine +
                                     "  <header> " + Environment.NewLine +
                                     "                <parameters> " + Environment.NewLine +
                                     "                      <param name='name'>" + inPlanName + "</param>" + Environment.NewLine +
                                     "                      <param name='type'>" + "DR Protection" + "</param>" + Environment.NewLine +
                                     "                      <param name='version'>1.1</param>" + Environment.NewLine +
                                     "                </parameters> " + Environment.NewLine +
                                     "  </header> " + Environment.NewLine +
                                     " </plan>";



               
                Trace.WriteLine(inPutString);

                try
                {
                    
                    String postData1 = "&operation=";

                    // xmlData would be send as the value of variable called "xml"
                    // This data would then be wrtitten in to a file callied input.xml
                    // on CX and then given as argument to the cx_cli.php script.
                    int operation = 43;

                    String postData2 = "&userfile='input.xml'&xml=" + inPutString;


                    String postData = postData1 + operation + postData2;
                    //  Trace.WriteLine(postData);
                    //String postData = "&operation=2";
                    WinPreReqs win = new WinPreReqs("", "", "", "");
                    HttpReply reply = win.ConnectToHttpClient(postData);
                    if (String.IsNullOrEmpty(reply.error))
                    {
                        // process reply
                        if (!String.IsNullOrEmpty(reply.data))
                        {
                            Trace.WriteLine(reply.data);
                        }
                    }
                    else
                    {

                        Trace.WriteLine(reply.error);
                        return false;
                    }
                    string responseFromServer = reply.data;
                    Trace.WriteLine("Printing Response from CX...");
                    Trace.WriteLine(responseFromServer);
                    Trace.WriteLine("proceeding ...");
                    if (responseFromServer.Contains("<error_code>FAIL</error_code>"))
                    {

                        Trace.WriteLine(DateTime.Now + "\t Specified Plan name doesn't exist on CX");

                        return false;
                    }

                    if (!responseFromServer.Contains(Sourcehostname))
                    {

                        Trace.WriteLine(DateTime.Now + "\t" + Sourcehostname + " is not part of given Plan");
                        return false;

                    }


                    // Trace.WriteLine(responseFromServer);

                    xmlstring = "<pair_details>" + Environment.NewLine;
                    //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
                    //based on above comment we have added xmlresolver as null
                    XmlDocument xDocument = new XmlDocument();
                    xDocument.XmlResolver = null;
                    Trace.WriteLine(DateTime.Now + " \t  responce from cxcli " + responseFromServer.ToString());
                    //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
                    XmlReaderSettings settings = new XmlReaderSettings();
                    settings.ProhibitDtd = true;
                    settings.XmlResolver = null;
                    using (XmlReader reader1 = XmlReader.Create(latestPath + WinPreReqs.tempXml, settings))
                    {
                        xDocument.Load(reader1);
                        //reader1.Close();
                    }

                    //Reading volume replication details from cx

                    XmlNodeList xPNodeList = xDocument.SelectNodes(".//volume_protection");


                    foreach (XmlNode xNode in xPNodeList)
                    {


                        //Comparing source host name with cxcli reponse and target hostname.If both matches, then only proceed for preparing xml for deletion.

                        if ((xNode.ChildNodes.Item(0).InnerText.ToString().ToUpper() == Sourcehostname.ToUpper() || xNode.ChildNodes.Item(0).InnerText.ToString().ToUpper() == Originalsourcehostname.ToUpper()) && (xNode.ChildNodes.Item(2).InnerText.ToString().ToUpper() == Mastertargethostname.ToUpper()|| xNode.ChildNodes.Item(2).InnerText.ToString().ToUpper() == Originalmastertargethostname.ToUpper()))
                        {

                            replicationExist = true;
                            Trace.WriteLine("Vx replication exist between:" + Sourcehostname + " and " + Mastertargethostname);

                            xmlstring = xmlstring +
                                      "        <volume_replication>" + Environment.NewLine +
                                      "                <source_ip>" + SourcehostIP + "</source_ip>" + Environment.NewLine +
                                      "                <target_ip>" + MastertargethostIP + "</target_ip>" + Environment.NewLine +
                                      "                <source_device>" + xNode.ChildNodes.Item(1).InnerText.ToString().Replace("\\", "\\\\") + "</source_device>" + Environment.NewLine +
                                      "                <target_device>" + xNode.ChildNodes.Item(3).InnerText.ToString().Replace("\\", "\\\\") + "</target_device>" + Environment.NewLine +
                                      "                <delete_retention>True</delete_retention>" + Environment.NewLine + 
                                      "        </volume_replication>" + Environment.NewLine;
                        }

                    }


                    //Getting CX hostname for preparing the xml to remove fx jobs.

                    string _cxHostName = null;
                    string _vconIP = "";
                    WinPreReqs.SetHostNameInputIP(WinTools.GetcxIP(),  _cxHostName, IncxIP);
                    _cxHostName = WinPreReqs.GetHostname;
                    WinPreReqs WP = new WinPreReqs("", "", "", "");
                    WP.SetHostIPinputhostname(_vConName,  _vconIP, IncxIP);

                    _vconIP = WinPreReqs.GetIPaddress;
                    //Reading File replication details from cx
                    //preparing the file replication 
                    xPNodeList = xDocument.SelectNodes(".//file_protection");


                    foreach (XmlNode xNode in xPNodeList)
                    {

                    //Job 0 from vcon to cx job 1 from sources to cx and job2 from cx to target
                    //sourceIP will be set accordingly. 


                        if (xNode.ChildNodes.Item(0).InnerText.ToString().ToUpper() == Sourcehostname.ToUpper()||xNode.ChildNodes.Item(0).InnerText.ToString().ToUpper() == Originalsourcehostname.ToUpper() || xNode.ChildNodes.Item(0).InnerText.ToString().ToUpper() == _vConName.ToUpper()|| xNode.ChildNodes.Item(0).InnerText.ToString().ToUpper() == Originalvconhostname.ToUpper() || xNode.ChildNodes.Item(0).InnerText.ToString().ToUpper() == _cxHostName)
                        {

                            replicationExist = true;
                            Trace.WriteLine(xNode.ChildNodes.Item(0).InnerText.ToString());
                            Trace.WriteLine(xNode.ChildNodes.Item(1).InnerText.ToString());
                            Trace.WriteLine(xNode.ChildNodes.Item(2).InnerText.ToString());
                            Trace.WriteLine(xNode.ChildNodes.Item(3).InnerText.ToString());

                            string sourceIp = "";

                            if( xNode.ChildNodes.Item(0).InnerText.ToString().ToUpper() == _vConName.ToUpper() || xNode.ChildNodes.Item(0).InnerText.ToString().ToUpper() == Originalvconhostname.ToUpper())
                            {
                                sourceIp=_vconIP;
                            
                            }
                            else if( xNode.ChildNodes.Item(0).InnerText.ToString().ToUpper() == _cxHostName)
                                
                            {
                                sourceIp = WinTools.GetcxIP();
                            }
                            else
                             {
                                 sourceIp = SourcehostIP;
                             }

                            string targetIp = "";
                            if (xNode.ChildNodes.Item(2).InnerText.ToString().ToUpper() == Mastertargethostname.ToUpper()||xNode.ChildNodes.Item(2).InnerText.ToString().ToUpper() == Originalmastertargethostname.ToUpper())
                            {

                                targetIp = MastertargethostIP;


                            }
                            else if (xNode.ChildNodes.Item(2).InnerText.ToString().ToUpper() == Sourcehostname.ToUpper() || xNode.ChildNodes.Item(2).InnerText.ToString().ToUpper() == Originalsourcehostname.ToUpper())
                            {

                                targetIp = SourcehostIP;

                            }
                            else if(xNode.ChildNodes.Item(2).InnerText.ToString().ToUpper() == _cxHostName.ToUpper())
                            {
                                targetIp = WinTools.GetcxIP();
                            }
                            else
                            {

                                continue;
                            }


                            Trace.WriteLine("Fx replication exist between:" + Sourcehostname + " and " + Mastertargethostname);

                            xmlstring = xmlstring +
                                      "     <file_replication>" + Environment.NewLine +
                                      "                <job_name>" + xNode.ChildNodes.Item(5).InnerText.ToString() + "</job_name>" + Environment.NewLine +
                                      "                <app_grp_name>" + xNode.ChildNodes.Item(4).InnerText.ToString() + "</app_grp_name>" + Environment.NewLine +
                                      "                <source_ip>" + sourceIp + "</source_ip>" + Environment.NewLine +
                                      "                <target_ip>" + targetIp + "</target_ip>" + Environment.NewLine +
                                      "                <source_dir>" + xNode.ChildNodes.Item(1).InnerText.ToString() + "</source_dir>" + Environment.NewLine +
                                      "                <target_dir>" + xNode.ChildNodes.Item(3).InnerText.ToString() + "</target_dir>" + Environment.NewLine +
                                      "                <job_id>" + xNode.ChildNodes.Item(6).InnerText.ToString() + "</job_id>" + Environment.NewLine +
                                      "                <group_id>" + xNode.ChildNodes.Item(7).InnerText.ToString() + "</group_id>" + Environment.NewLine +
                                      "        </file_replication>" + Environment.NewLine;

                        }

                    }







                    xmlstring = xmlstring + "</pair_details>";

                    Trace.WriteLine("output string is:" + Environment.NewLine + xmlstring);

                }
                catch (System.Net.WebException e)
                {

                    System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);
                    System.FormatException formatException = new FormatException("Inner exception", e);
                    Trace.WriteLine(formatException.GetBaseException());
                    Trace.WriteLine(formatException.Data);
                    Trace.WriteLine(formatException.InnerException);
                    Trace.WriteLine(formatException.Message);
                    Trace.WriteLine("ERROR*******************************************");
                    Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                    Trace.WriteLine("Exception  =  " + e.Message);
                    Trace.WriteLine("ERROR___________________________________________");

                    Trace.WriteLine("\t This Server is not pointed to the CX.Please verify whether agents are installed or not.And are pointing to " + IncxIP);
                    return false;
                }
                catch (System.Xml.XmlException e)
                {
                    System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);
                    System.FormatException formatException = new FormatException("Inner exception", e);
                    Trace.WriteLine(formatException.GetBaseException());
                    Trace.WriteLine(formatException.Data);
                    Trace.WriteLine(formatException.InnerException);
                    Trace.WriteLine(formatException.Message);
                    Trace.WriteLine("ERROR*******************************************");
                    Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                    Trace.WriteLine("Exception  =  " + e.Message);
                    Trace.WriteLine("ERROR___________________________________________");
                    Trace.WriteLine("Cxcli reponse is not formatted correctly");
                    return false;


                }

                if (replicationExist == false)
                {

                    Trace.WriteLine("There is no replication between source and  master target exist");
                    return false;

                }
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                System.FormatException formatException = new FormatException("Inner exception", ex);
                Trace.WriteLine(formatException.GetBaseException());
                Trace.WriteLine(formatException.Data);
                Trace.WriteLine(formatException.InnerException);
                Trace.WriteLine(formatException.Message);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;

            }

          
            return true;

        }




        //internal void RemoveReplication()
        //{
        //    throw new NotImplementedException();
        //}
    }
}
