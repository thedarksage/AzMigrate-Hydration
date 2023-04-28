using System;
using System.Collections.Generic;
using System.Text;
using System.Collections;
using System.Net.NetworkInformation;
using System.Diagnostics;
using System.IO;
using System.Xml;
using System.Web;
using System.Net;
using System.Windows.Forms;
using System.Threading;
using System.Management;
using HttpCommunication;

namespace Com.Inmage.Tools
{
    public class MasterConfigFile
    {
        internal String Xmlstring = "";
        internal string Masterconfigfile = "MasterConfigFile.xml";
        string cxIP= null;
        internal string EsxtargetFile = null;
        internal bool removedTheNode = false;
        internal string latestfilepath = null;
        public MasterConfigFile()
        {

            latestfilepath = WinTools.FxAgentPath() + "\\vContinuum\\Latest";
            Trace.WriteLine(DateTime.Now+"\tMasterConfigFile Constructor called.");
        
        }


        



        public bool DownloadMasterConfigFile(string incxIP)
        {
            // We will download MasterConfig.xml form the using using cxcli call number 53.
            Debug.WriteLine("Entered DownloadMasterConfigFile()...");
            

            try
            {
                
                string postData1 = "&operation=";

                // xmlData would be send as the value of variable called "xml"
                // This data would then be wrtitten in to a file callied input.xml
                // on CX and then given as argument to the cx_cli.php script.
                int operation = 53;

             
                string postData2 = "&userfile='input.xml'&xml=" + Xmlstring;

                // 
                string vCon_guuid = WinTools.GetvContinuumguuid();

                Trace.WriteLine(DateTime.Now + "\t vcon id " + vCon_guuid);
                string postData = postData1 + operation + "&hostguid=" + vCon_guuid + postData2;
                WinPreReqs win = new WinPreReqs("", "", "", "");
                HttpReply reply = win.ConnectToHttpClient(postData);
                if (String.IsNullOrEmpty(reply.error))
                {
                    // process reply
                    if (!String.IsNullOrEmpty(reply.data))
                    {
                        Debug.WriteLine(reply.data);
                    }
                }
                else
                {

                    Trace.WriteLine(reply.error);
                    return false;
                }
                 Xmlstring = reply.data; 
                if (Xmlstring.Contains("ERROR IN DOWNLOADING FILE"))
                {
                    Trace.WriteLine(DateTime.Now + "\t Unable to Download file from " + incxIP);
                    Trace.WriteLine(DateTime.Now + "\t" + Xmlstring);
                    return false;

                }
                else
                {
                    try
                    {
                        if (Xmlstring.Contains("[InMagespecialChar]"))
                        {
                            Trace.WriteLine(DateTime.Now + "\t Found [InMagespecialChar] in master config file");
                            Trace.WriteLine(DateTime.Now + "\t Making that special char as ' (single inverted comma )");
                            Xmlstring = Xmlstring.Replace("[InMagespecialChar]", "'");

                        }
                    }
                    catch (Exception ex)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Failed to check for special char " + ex.Message);
                    }

                    if (WriteConfigFile() == true)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Successfully saved configuration in" + latestfilepath);
                        return true;
                    }
                    else
                    {
                        Trace.WriteLine(DateTime.Now + "\t Failed to save configuration in" + latestfilepath);
                        return false;

                    }

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


                Trace.WriteLine("Faied to delete jobs from CX." + incxIP);
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


            return true;


          
                        
        }

        internal string EncodeString(string inputString)
        {
            string xmlstring = "";
            try
            {
                // TO upload and downlaod a file from http we need to encode if the length of the string exceeds more than 32000
              //  string xmlstring = "";
                int totalLenght = 0;
                int i = 0;
                int MAX_LENGTH = 10000;

                if (inputString.Length > 32000)
                {
                    for (i = 0; i < (inputString.Length - MAX_LENGTH); i = i + MAX_LENGTH)
                    {

                        xmlstring += Uri.EscapeDataString(inputString.Substring(i, MAX_LENGTH));

                    }

                    xmlstring += Uri.EscapeDataString(inputString.Substring(i, inputString.Length - i));
                }
                else
                {

                    xmlstring = Uri.EscapeDataString(inputString);

                }
               // Debug.WriteLine(DateTime.Now + "\t string " + xmlstring);
            }
            catch (System.UriFormatException e)
            {
                System.FormatException formatException = new FormatException("Inner exception", e);
                Trace.WriteLine(formatException.GetBaseException());
                Trace.WriteLine(formatException.Data);
                Trace.WriteLine(formatException.InnerException);
                Trace.WriteLine(formatException.Message);
                Trace.WriteLine(DateTime.Now + "URI format exception occured" + e.Message);
            
            }

            return xmlstring;

        
        }

        public bool UploadMasterConfigFile(string _in_cx_IP)
        {
            // Using cxcli call number 52 we are uploading MasterConfigFile.xml to cx.
            Trace.WriteLine("Entered UploadMasterConfigFile()...");
           
            

            try
            {
                
                string postData1 = "&operation=";

                // xmlData would be send as the value of variable called "xml"
                // This data would then be wrtitten in to a file callied input.xml
                // on CX and then given as argument to the cx_cli.php script.
                int operation = 52;

                if (ReadConfigFile() == false)
                {

                    Trace.WriteLine(DateTime.Now + "\t" + Masterconfigfile + " doesn't exist in " + latestfilepath);
                    Trace.WriteLine(DateTime.Now + "\t" + "Existing..Unable to upload config file");
                    return false;

                }
                try
                {
                    if (Xmlstring.Contains("'"))
                    {
                        Trace.WriteLine(DateTime.Now + "\t Found inverted comma in xml file ");
                        Trace.WriteLine(DateTime.Now + "\t before uploading wr are making that char as [InMagespecialChar]");
                        Xmlstring = Xmlstring.Replace("'", "[InMagespecialChar]");

                    }
                }
                catch (Exception ex)
                {
                    Trace.WriteLine(DateTime.Now + "\t Failed to search for ' " + ex.Message);
                }
                Xmlstring=EncodeString(Xmlstring);

                string postData2 = "&userfile='input.xml'&xml=" + Xmlstring;


                string vCon_guuid = WinTools.GetvContinuumguuid();

                Trace.WriteLine(DateTime.Now + "\t vcon id " + vCon_guuid);

                //MessageBox.Show(_xmlstring);

                // string postData2 = "&userfile='input.xml'&xml=" + _xmlstring;
                string postData = postData1 + operation + "&hostguid=" + vCon_guuid + postData2;

                WinPreReqs win = new WinPreReqs("", "", "", "");
                HttpReply reply = win.ConnectToHttpClient(postData);
                if (String.IsNullOrEmpty(reply.error))
                {
                    // process reply
                    if (!String.IsNullOrEmpty(reply.data))
                    {
                        Debug.WriteLine(reply.data);
                    }
                }
                else
                {

                    Trace.WriteLine(reply.error);
                    return false;
                }
                //string responseFromServer = reply.data; 

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
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: UploadMasterConfigFile " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + e.Message);
                Trace.WriteLine("ERROR___________________________________________");


                Trace.WriteLine("Faied to delete jobs from CX." + _in_cx_IP);
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


            return true;
        
        }

        internal bool ReadConfigFile()
        {
            // This is to know the length of the string ....
            if (File.Exists(latestfilepath + "\\" + Masterconfigfile))
            {
                StreamReader sr1 = new StreamReader(latestfilepath + "\\" + Masterconfigfile);
                Xmlstring = sr1.ReadToEnd();
                sr1.Close();
            }
            else
            {
                return false;            
            }
            return true;    
        
        }

        internal bool WriteConfigFile()
        {
            // with the read information we will write xml file with name MasterConfigFile.xml..
            if (File.Exists(latestfilepath+"\\"+Masterconfigfile))
            {
                File.Delete(Masterconfigfile);
            }

            StreamWriter sr1 = new StreamWriter(latestfilepath + "\\" + Masterconfigfile);
            sr1.Write(Xmlstring);       
            sr1.Close();

            WinTools win = new WinTools();
            win.SetFilePermissions(latestfilepath + "\\"+ Masterconfigfile);
            return true;
        }

        internal bool GetCXIPSFromCxCli()
          {
              // With this call we will get all cxips including the nat ips also...
              // With this call we can avoid issue that we are displaying different cxips in the vCon Wizard...
              string inPutString = "<plan> " + Environment.NewLine + "<failover_protection> " + Environment.NewLine + "                <replication> " + Environment.NewLine + "                <server_ip>" +WinTools.GetcxIP() + "</server_ip>" + Environment.NewLine +
                                     "                </replication>" + Environment.NewLine + " </failover_protection>" + Environment.NewLine + " </plan>";
             
             

              Trace.WriteLine(inPutString);
              try
              {
                 
                  String postData1 = "&operation=";
                  // xmlData would be send as the value of variable called "xml"
                  // This data would then be wrtitten in to a file callied input.xml
                  // on CX and then given as argument to the cx_cli.php script.
                  int operation = 3;
                  String postData2 = "&userfile='input.xml'&xml=" + inPutString;
                  String postData = postData1 + operation + postData2;
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
                  if ((!responseFromServer.Contains(WinTools.GetcxIP())))
                  {                      
                      return false;
                  }
                  // MessageBox.Show(responseFromServer);
                  //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
                  //based on above comment we have added xmlresolver as null
                  XmlDocument xDocument = new XmlDocument();
                  xDocument.XmlResolver = null;
                  //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
                  XmlReaderSettings settings = new XmlReaderSettings();
                  settings.ProhibitDtd = true;
                  settings.XmlResolver = null;

                  using (XmlReader reader1 = XmlReader.Create(latestfilepath + WinPreReqs.tempXml, settings))
                  {
                      xDocument.Load(reader1);
                      //reader1.Close();
                  }
                  XmlNodeList xPNodeList = xDocument.SelectNodes(".//ip_addresses");
                  foreach (XmlNode xNode in xPNodeList)
                  {
                      cxIP = (xNode.InnerText != null ? xNode.InnerText : "");  
                  }         
                 
              }
              //Process_Server_List.Update();
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
                  Trace.WriteLine("This error from GetDetailsFromCxCli() method");
                  Trace.WriteLine(e.ToString());
                  //MessageBox.Show("Server is not pointed to the CX.Please veriry whether agents are installed or not.And they are pointing to " + inCXIP);
                 // h.pointedToCx = false;
                  return false;
              }
              catch (Exception e)
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
             
              return true;

          }
        internal bool RemoveEntryIfAlreadyExists(string masterConfigfile, string inEsxMasterXMLFile)
          {
              try
              {
                // This will remove the already existing node and that too protect to same target esx ...
                  Trace.WriteLine(DateTime.Now + " \t  Entered: RemoveEntryIfAlreadyExists() in MasterConfigFile.cs");
                  //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
                  //based on above comment we have added xmlresolver as null
                  XmlDocument documentEsx = new XmlDocument();
                  documentEsx.XmlResolver = null;
                  XmlDocument documentMasterEsx = new XmlDocument();
                  documentMasterEsx.XmlResolver = null;

                  string esxXmlFile = inEsxMasterXMLFile;
                  StreamReader reader = new StreamReader(esxXmlFile);
                 // string xmlString = reader.ReadToEnd();


                  string esxMasterXmlFile =latestfilepath + "\\"+  masterConfigfile;
                  StreamReader reader1 = new StreamReader(esxMasterXmlFile);
                 // string xmlMasterString = reader1.ReadToEnd();

                  /*RemoveDifferentCxNodes(esxXmlFile);
                  while (removedTheNode == true)
                  {
                      removedTheNode = false;
                      Trace.WriteLine(DateTime.Now + " \t Removed the node");
                      RemoveDifferentCxNodes(esxXmlFile);
                  }*/
                  Trace.WriteLine(DateTime.Now + "\t Prinitng the file names Esx " + esxXmlFile + " master " + esxMasterXmlFile);
                  //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
                  XmlReaderSettings settings = new XmlReaderSettings();
                  settings.ProhibitDtd = true;
                  settings.XmlResolver = null;
                  using (XmlReader reader2 = XmlReader.Create(esxXmlFile, settings))
                  {                      
                      documentEsx.Load(reader2);
                      //reader2.Close();
                  }

                  using (XmlReader reader3 = XmlReader.Create(esxMasterXmlFile, settings))
                  {
                      documentMasterEsx.Load(reader3);
                      
                      //reader3.Close();
                  }
                 // reader1.Close();
                 // reader.Close();
                  XmlNodeList hostNodesEsxXml = null, hostNodesEsxMasterXml = null;
                  hostNodesEsxXml = documentEsx.GetElementsByTagName("host");
                  hostNodesEsxMasterXml = documentMasterEsx.GetElementsByTagName("host");                
                 
                  
                  documentEsx.Save(esxXmlFile);
                  foreach (XmlNode esxnode in hostNodesEsxXml)
                  {
                      foreach (XmlNode esxMasternode in hostNodesEsxMasterXml)
                      {                       
                           if (esxnode.Attributes["display_name"].Value == esxMasternode.Attributes["display_name"].Value && ((esxMasternode.Attributes["targetesxip"].Value == esxnode.Attributes["targetesxip"].Value) || (esxMasternode.Attributes["targetesxip"].Value == esxnode.Attributes["esxIp"].Value)))
                          {
                              Trace.WriteLine(DateTime.Now + " \t Came here to check the display names and esx ips " + esxnode.Attributes["display_name"].Value + " " + esxMasternode.Attributes["display_name"].Value + " ips " + esxMasternode.Attributes["targetesxip"].Value + " \t " + esxnode.Attributes["targetesxip"].Value + " \t  " + esxMasternode.Attributes["targetesxip"].Value + "\t " + esxnode.Attributes["esxIp"].Value);
                              Trace.WriteLine(DateTime.Now + " \t Came here to remove the entry form master config file " + esxnode.Attributes["display_name"].Value);
                              if (esxMasternode.ParentNode.Name == "SRC_ESX")
                              {
                                  XmlNode parentNode = esxMasternode.ParentNode;
                                  esxMasternode.ParentNode.RemoveChild(esxMasternode);
                                  if (!parentNode.HasChildNodes)
                                  {
                                      parentNode.ParentNode.ParentNode.RemoveChild(parentNode.ParentNode);
                                  }
                              }
                              break;
                          }
                      }
                  }
                  documentEsx.Save(esxXmlFile);
                  documentMasterEsx.Save(esxMasterXmlFile);
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
              Trace.WriteLine(DateTime.Now + " \t  Exiting: RemoveEntryIfAlreadyExists() in MasterConfigFile.cs");
              return true;
          }

        internal bool RemoveDifferentCxNodes(string inMasterXMLFile)
          {

             // Ths method is to remove the entriesof different cx......
              //Multiple MTs in the same target esx server and multiple CXs are used...
              //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
              //based on above comment we have added xmlresolver as null
              XmlDocument documentEsx = new XmlDocument();
              documentEsx.XmlResolver = null;
              string esxXmlFile = inMasterXMLFile;


              //StreamReader reader = new StreamReader(esxXmlFile);
              //string xmlString = reader.ReadToEnd();
              //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
              XmlReaderSettings settings = new XmlReaderSettings();
              settings.ProhibitDtd = true;
              settings.XmlResolver = null;
              using (XmlReader reader1 = XmlReader.Create(esxXmlFile, settings))
              {
                  documentEsx.Load(reader1);
                  //reader1.Close();
              }
              //reader.Close();
              XmlNodeList hostNodesEsxXml = null;
              hostNodesEsxXml = documentEsx.GetElementsByTagName("host");    
              //cxIps are already filled using the cxcli method GetCxDetails() with operation 3.
              if (cxIP != null)
              {
                  string[] cxips = cxIP.Split(',');
                  int count = hostNodesEsxXml.Count;
                  
                  foreach (XmlNode esxnode in hostNodesEsxXml)
                  {
                      if (esxnode.ParentNode.Name == "SRC_ESX")
                      {
                          bool ipExists = false;
                          if (esxnode.Attributes["cxIP"] != null && esxnode.Attributes["cxIP"].Value != null)
                          {
                              Trace.WriteLine(DateTime.Now + "\t Does not matched the cx ips ");
                              foreach (string ip in cxips)
                              {
                                  if (ip == esxnode.Attributes["cxIP"].Value)
                                  {
                                      ipExists = true;
                                  }
                              }
                              if (ipExists == false)
                              {
                                  // It comes here only when cx i doesn't matches with teh current cxips to which vcon is pointed to...

                                  if (esxnode.ParentNode.Name == "SRC_ESX")
                                  {
                                      Trace.WriteLine(DateTime.Now + " \t Came here toremove the node ");
                                      XmlNode parentNode = esxnode.ParentNode;

                                      esxnode.ParentNode.RemoveChild(esxnode);
                                      if (!parentNode.HasChildNodes)
                                      {
                                          parentNode.ParentNode.ParentNode.RemoveChild(parentNode.ParentNode);
                                      }
                                      removedTheNode = true;
                                  }
                                  break;
                              }
                          }
                      }
                  }
              }
              else
              {
                  Trace.WriteLine(DateTime.Now + "\t Problem with the cxcli call option 3. Becuase we got cxips as null");
              }
              documentEsx.Save(esxXmlFile); 
              return true;
          }


        internal bool AppendXml(string masterConfigfile, string inMasterxml)
          {
              try
              {
                  Trace.WriteLine(DateTime.Now + " \t  Entered: AppendXml() in MasterConfigFile.cs");
                  //Reading esx.xml for further use.

                  //XmlTextReader myReader = new XmlTextReader(inMasterxml);

                  // Load the source of the XML file into an XmlDocument
                  //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
                  //based on above comment we have added xmlresolver as null
                  XmlDocument mySourceDoc = new XmlDocument();
                  mySourceDoc.XmlResolver = null;
                  //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
                  // Load the source XML file into the first document
                  XmlReaderSettings settings = new XmlReaderSettings();
                  settings.ProhibitDtd = true;
                  settings.XmlResolver = null;

                  using (XmlReader reader1 = XmlReader.Create(inMasterxml, settings))
                  {
                      mySourceDoc.Load(reader1);
                     // reader1.Close();
                  }
                  // Close the reader

                 // myReader.Close();


                  Debug.WriteLine("prepared initial master xml file");

                  // Open the reader with the destination XML file

                  //myReader = new XmlTextReader(latestfilepath+"\\"+masterConfigfile);

                  // Load the source of the XML file into an XmlDocument
                  //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
                  XmlDocument myDestDoc = new XmlDocument();
                  myDestDoc.XmlResolver = null;
                  // Load the destination XML file into the first document
                  using (XmlReader reader2 = XmlReader.Create(latestfilepath + "\\" + masterConfigfile, settings))
                  {
                      myDestDoc.Load(reader2);
                      //reader2.Close();
                  }
                  // Close the reader

                 // myReader.Close();

                  XmlNode rootDest = myDestDoc["start"];


                  // Store the node to be copied into an XmlNode

                  XmlNodeList nodeOrigList = mySourceDoc["start"].ChildNodes;
                  foreach (XmlNode nodeOrig in nodeOrigList)
                  { 
                      
                      // Store the copy of the original node into an XmlNode

                      XmlNode nodeDest = myDestDoc.ImportNode(nodeOrig, true);

                      // Append the node being copied to the root of the destination document
                      //if(nodeDest.Attributes["cx_ip"].Value.ToString() == WinTools.getCxIp())
                      rootDest.AppendChild(nodeDest);
                  }

                  XmlTextWriter myWriter = new XmlTextWriter(latestfilepath + "\\"+masterConfigfile, Encoding.UTF8);

                  // Indented for easy reading

                  myWriter.Formatting = Formatting.Indented;
                   
                  // Write the file

                  myDestDoc.WriteTo(myWriter);

                  // Close the writer

                  myWriter.Close();
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
              Trace.WriteLine(DateTime.Now + " \t  Exiting: AppendXml() in MasterConfigFile.cs");
              return true;

          }
        internal bool AddOrReplaceMasterConfigFile(string masterConfigfile, string inEsxMasterXMLFile)
          {
              // First we will get all the cx ips and nat ips..
              // we will check all the cx ips in the esx_master_targetesxip.xml and then we will remove the diffeent
              // cx nodes and we will remove if duplicate entries and then we will append esx_master_targetesxip.xml to the masterconfile.xml...

              GetCXIPSFromCxCli();
              EsxtargetFile = inEsxMasterXMLFile;
              if (!File.Exists(latestfilepath+"\\"+ masterConfigfile))
              {  //Create New file mastconfig file.xml//
                  XmlWriterSettings settings = new XmlWriterSettings();
                  settings.Indent = true;
                  settings.ConformanceLevel = ConformanceLevel.Fragment;

                  if (File.Exists(inEsxMasterXMLFile))
                  {
                      File.Copy(inEsxMasterXMLFile,latestfilepath + "\\"+ Masterconfigfile, true);
                  }
                  else
                  {
                      Trace.WriteLine(DateTime.Now + " \t "+ inEsxMasterXMLFile+ " doesn't exist");
                      Trace.WriteLine(DateTime.Now + " \t Failed with exist status 1");
                      return false;
                  }
              }
              else
              {
                  
                  if (RemoveEntryIfAlreadyExists(masterConfigfile, inEsxMasterXMLFile) == true)
                  {
                      if (AppendXml(masterConfigfile,inEsxMasterXMLFile) == true)
                      {
                          Trace.WriteLine(DateTime.Now + "\t Updated Masterconfigfile successfully");
                          return true;
                      }
                      else
                      {
                          Trace.WriteLine(DateTime.Now + "\t Failed to update Masterconfigfile successfully");
                          return false;
                      }
                  }
                  else
                  {
                      Trace.WriteLine(DateTime.Now + "\t Failed Remove existing hosts from master config file..");
                      return false;

                  }
              }

              return true;
          }

        public bool UpdateMasterConfigFile(string inPutXml)
        {
            //Input.xml can be esx.xml , recovery.xm and remove.xml. Based on opration ui or scripts will pass this file.
            // Last step is to uplaod masterconfigfile.xml to the cx.
            if (File.Exists(latestfilepath + "\\"+Masterconfigfile))
            {
                File.Delete(latestfilepath + "\\" + Masterconfigfile);
            
            }
            //Step1 download the file from cx.
            DownloadMasterConfigFile(WinTools.GetcxIPWithPortNumber());
            
            //Checking whether downloaded masterConfigFile size 0 or not//

            if (File.Exists(latestfilepath + "\\" + Masterconfigfile))
            {
                FileInfo fInfo = new FileInfo(latestfilepath + "\\" + Masterconfigfile);

                long size = fInfo.Length;
                if(size == 0)
                File.Delete(latestfilepath + "\\" + Masterconfigfile);
                
            }

           
            SolutionConfig solutionConfig = new SolutionConfig();
            solutionConfig.AddOrReplaceMasterXmlFile(inPutXml);
            //AddOrReplaceMasterConfigFile(MASTER_CONFIG_FILE, inMasterXMLfile);
            if (UploadMasterConfigFile(WinTools.GetcxIPWithPortNumber()) == false)
            {
                return false;
            }
            
            
            return true;
        }

        public bool UpdateMasterConfigFiletoLatestVersion(string inPutXml)
        {
            //Input.xml can be esx.xml , recovery.xm and remove.xml. Based on opration ui or scripts will pass this file.
            // Last step is to uplaod masterconfigfile.xml to the cx.
            if (File.Exists(latestfilepath + "\\" + Masterconfigfile))
            {
                File.Delete(latestfilepath + "\\" + Masterconfigfile);

            }
            //Step1 download the file from cx.
            DownloadMasterConfigFile(WinTools.GetcxIPWithPortNumber());
            
            //check if there any empty nodes and remove if exist//
            try
            {
                RemoveEmptyNode(inPutXml);
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Caught exception at RemoveEmptyNode " + ex.Message);
            }

            try
            {
                RemoveOldNodesWhileUpgradetWithTempFile(inPutXml);
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Caught exception at RemoveOldNodes " + ex.Message);
            }
            //Checking whether downloaded masterConfigFile size 0 or not//

            if (File.Exists(latestfilepath + "\\" + Masterconfigfile))
            {
                FileInfo fInfo = new FileInfo(latestfilepath + "\\" + Masterconfigfile);

                long size = fInfo.Length;
                if (size == 0)
                    File.Delete(latestfilepath + "\\" + Masterconfigfile);

            }
            try
            {
                if (File.Exists(inPutXml))
                {
                    FileInfo fInfo = new FileInfo(inPutXml);

                    long size = fInfo.Length;
                    if (size < 30)
                        return true;

                }
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now + "\t Failed to check the size of temp file " + ex.Message);
            }
            AddOrReplaceMasterConfigFile(Masterconfigfile, inPutXml);
            UploadMasterConfigFile(WinTools.GetcxIPWithPortNumber());


            return true;
        }

        //This will remove the empty SRC_ESX nodes along with their parent nodes.
        //First setp we will load the XMl File which is given as an input

        internal bool RemoveEmptyNode(string inPutXml)
        {
            try
            {//Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
                //based on above comment we have added xmlresolver as null
                XmlDocument documentEsx = new XmlDocument();
                documentEsx.XmlResolver = null;
                string esxXmlFile = inPutXml;

                //StreamReader reader = new StreamReader(esxXmlFile);
                //string xmlString = reader.ReadToEnd();
                //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
                XmlReaderSettings settings = new XmlReaderSettings();
                settings.ProhibitDtd = true;
                settings.XmlResolver = null;
                using (XmlReader reader1 = XmlReader.Create(esxXmlFile, settings))
                {
                    documentEsx.Load(reader1);
                    //reader1.Close();
                }
               // reader.Close();
                XmlNodeList hostNodesEsxXml = null;
                int[] indexes;


                hostNodesEsxXml = documentEsx.GetElementsByTagName("SRC_ESX");

                
                
                for (int i = 0; i < hostNodesEsxXml.Count; i++)
                {
                    foreach (XmlNode esxnode in hostNodesEsxXml)
                    {

                        if (esxnode.Name == "SRC_ESX")
                        {
                            if (!esxnode.HasChildNodes)
                            {
                                XmlNode parentNode = esxnode.ParentNode;
                                //parent node is root node
                                string xmlnullvalue = "";
                                parentNode.ParentNode.RemoveChild(esxnode.ParentNode);
                                Trace.WriteLine(DateTime.Now + "\t Removed one node form " + inPutXml);
                                documentEsx.Save(esxXmlFile);
                                hostNodesEsxXml = documentEsx.GetElementsByTagName("SRC_ESX");
                                i = -1;
                                break;
                            }

                        }

                    }

                }
                //test

                documentEsx.Save(esxXmlFile);
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

        /*
         * First we are loading both the file. After that we are checking whetehr we have source_uuid in esx_master_temp.xml
         * If uuid is not null the we will check with uuids or we will go for displaynames.
         * If both the uuid or displaynames matched we will check for the root node configurationversion
         * If version is 1.2 and present is 3.0 we will remove that node from the ESX_Mastet_temp.xml.
         * After that we will save the temp file and check for the length if length is less than 30 we will stop there and give return code true.
         */
        internal bool RemoveOldNodesWhileUpgradetWithTempFile(string inEsxMasterXMLFile)
        {
            try
            {
                // This will remove the already existing node and that too protect to same target esx ...
                Trace.WriteLine(DateTime.Now + " \t  Entered: RemoveOldNodesWhileUpgradetWithTempFile() in MasterConfigFile.cs");
                //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
                //based on above comment we have added xmlresolver as null
                XmlDocument documentEsx = new XmlDocument();
                XmlDocument documentMasterEsx = new XmlDocument();
                documentEsx.XmlResolver = null;
                documentMasterEsx.XmlResolver = null;
                string esxXmlFile = inEsxMasterXMLFile;
                string esxMasterXmlFile = latestfilepath + "\\" + Masterconfigfile;


                //StreamReader reader = new StreamReader(esxXmlFile);
                //string xmlString = reader.ReadToEnd();


                //StreamReader reader1 = new StreamReader(esxMasterXmlFile);
                //string xmlMasterString = reader1.ReadToEnd();

                /*RemoveDifferentCxNodes(esxXmlFile);
                while (removedTheNode == true)
                {
                    removedTheNode = false;
                    Trace.WriteLine(DateTime.Now + " \t Removed the node");
                    RemoveDifferentCxNodes(esxXmlFile);
                }*/
                //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
                Trace.WriteLine(DateTime.Now + "\t Prinitng the file names Esx " + esxXmlFile + " master " + esxMasterXmlFile);
                XmlReaderSettings settings = new XmlReaderSettings();
                settings.ProhibitDtd = true;
                settings.XmlResolver = null;
                using (XmlReader reader1 = XmlReader.Create(esxXmlFile, settings))
                {
                    documentEsx.Load(reader1);
                   // reader1.Close();
                }
                using (XmlReader reader2 = XmlReader.Create(esxMasterXmlFile, settings))
                {
                    documentMasterEsx.Load(reader2);
                    //reader2.Close();
                }
                //reader.Close();
                //reader1.Close();
                XmlNodeList hostNodesEsxXml = null, hostNodesEsxMasterXml = null;
                hostNodesEsxXml = documentEsx.GetElementsByTagName("host");
                hostNodesEsxMasterXml = documentMasterEsx.GetElementsByTagName("host");


                documentEsx.Save(esxXmlFile);
                 foreach (XmlNode esxMasternode in hostNodesEsxMasterXml)
                 {
                     foreach (XmlNode esxnode in hostNodesEsxXml)
                     {
                         if (esxMasternode.Attributes["source_uuid"] != null && esxMasternode.Attributes["source_uuid"].Value.Length != 0 && esxnode.Attributes["source_uuid"] != null && esxnode.Attributes["source_uuid"].Value.Length != 0)
                         {
                             if (((esxnode.Attributes["source_uuid"].Value == esxMasternode.Attributes["source_uuid"].Value) || (esxnode.Attributes["source_uuid"].Value == esxMasternode.Attributes["target_uuid"].Value)) && ((esxMasternode.Attributes["targetesxip"].Value == esxnode.Attributes["targetesxip"].Value) || (esxMasternode.Attributes["targetesxip"].Value == esxnode.Attributes["esxIp"].Value)))
                             {
                                 Trace.WriteLine(DateTime.Now + " \t Came here to check the display names and esx ips " + esxnode.Attributes["display_name"].Value + " " + esxMasternode.Attributes["display_name"].Value + " ips " + esxMasternode.Attributes["targetesxip"].Value + " \t " + esxnode.Attributes["targetesxip"].Value + " \t  " + esxMasternode.Attributes["targetesxip"].Value + "\t " + esxnode.Attributes["esxIp"].Value);
                                 Trace.WriteLine(DateTime.Now + " \t Came here to remove the entry form master config file " + esxnode.Attributes["display_name"].Value);

                                 if (esxnode.ParentNode.Name == "SRC_ESX")
                                 {
                                     XmlNode parentNode = esxnode.ParentNode;

                                     if (esxnode.ParentNode.ParentNode.Attributes["ConfigFileVersion"] != null && esxnode.ParentNode.ParentNode.Attributes["ConfigFileVersion"].Value.Length != 0)
                                     {
                                         if (esxnode.ParentNode.ParentNode.Attributes["ConfigFileVersion"].Value.Contains("1.2") && esxMasternode.ParentNode.ParentNode.Attributes["ConfigFileVersion"].Value.Contains("3.0"))
                                         {
                                             esxnode.ParentNode.RemoveChild(esxnode);
                                             if (!parentNode.HasChildNodes)
                                             {
                                                 parentNode.ParentNode.ParentNode.RemoveChild(parentNode.ParentNode);
                                             }
                                         }
                                     }


                                 }
                                 break;
                             }
                         }
                         else
                         {
                             if (esxnode.Attributes["display_name"].Value == esxMasternode.Attributes["display_name"].Value && ((esxMasternode.Attributes["targetesxip"].Value == esxnode.Attributes["targetesxip"].Value) || (esxMasternode.Attributes["targetesxip"].Value == esxnode.Attributes["esxIp"].Value)))
                             {
                                 Trace.WriteLine(DateTime.Now + " \t Came here to check the display names and esx ips " + esxnode.Attributes["display_name"].Value + " " + esxMasternode.Attributes["display_name"].Value + " ips " + esxMasternode.Attributes["targetesxip"].Value + " \t " + esxnode.Attributes["targetesxip"].Value + " \t  " + esxMasternode.Attributes["targetesxip"].Value + "\t " + esxnode.Attributes["esxIp"].Value);
                                 Trace.WriteLine(DateTime.Now + " \t Came here to remove the entry form master config file " + esxnode.Attributes["display_name"].Value);

                                 if (esxnode.ParentNode.Name == "SRC_ESX")
                                 {
                                     XmlNode parentNode = esxnode.ParentNode;

                                     if (esxnode.ParentNode.ParentNode.Attributes["ConfigFileVersion"] != null && esxnode.ParentNode.ParentNode.Attributes["ConfigFileVersion"].Value.Length != 0)
                                     {
                                         if (esxnode.ParentNode.ParentNode.Attributes["ConfigFileVersion"].Value.Contains("1.2") && esxMasternode.ParentNode.ParentNode.Attributes["ConfigFileVersion"].Value.Contains("3.0"))
                                         {
                                             esxnode.ParentNode.RemoveChild(esxnode);
                                             if (!parentNode.HasChildNodes)
                                             {
                                                 parentNode.ParentNode.ParentNode.RemoveChild(parentNode.ParentNode);
                                             }
                                         }
                                     }


                                 }
                                 break;
                             }
                         }
                     }
                }
                documentEsx.Save(esxXmlFile);
                documentMasterEsx.Save(esxMasterXmlFile);
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
            Trace.WriteLine(DateTime.Now + " \t  Exiting: RemoveOldNodesWhileUpgradetWithTempFile in MasterConfigFile.cs");
            return true;
        }

    }
}
