using System;
using System.Collections.Generic;
using System.Collections;
using System.Linq;
using System.Text;
using Microsoft.SharePoint.Administration;
using Microsoft.SharePoint;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Xml;
using System.IO;
using System.Net;
using System.Net.Security;
using System.Security.Cryptography.X509Certificates;
using System.Data.SqlClient;
using System.Diagnostics;
using System.Net.Sockets;
using HttpCommunication;

namespace spapp
{
    class SharepointDiscovery
    {
        public const string SHAREPOINT_2007 = "SharePoint2007";
        public const string SHAREPOINT_2010 = "SharePoint2010";
        public const string SHAREPOINT_2013 = "Sharepoint2013";

        public bool Find_Dbserver_or_Not(String ServerName)
        {
            Trace.WriteLine(DateTime.Now.ToString() + "\t" +  "Entered SharepointDiscovery Find_Dbserver_or_Not");
            bool result = true;
            String dbString = ServerName;
            String connStr = "";
            SqlConnection tempConn;
            connStr = "Data Source=" + dbString + ";Initial Catalog=master; Integrated Security=SSPI";
            tempConn = new SqlConnection(connStr);

            try
            {
                 tempConn.Open();
            }
            catch (Exception e)
            {
                Trace.TraceInformation("{0} in:{1} exception:{2}/nExpected exception (or) Ignore", DateTime.Now.ToString(), "Find_Dbserver_or_Not", e.Message);
                //if it is  not a sql server
                result = false ;     
            }
            finally
            {
                tempConn.Close();
                tempConn.Dispose();
            }
            Trace.WriteLine(DateTime.Now.ToString() + "\t" +  "Exited SharepointDiscovery Find_Dbserver_or_Not");
            return result;
        }

        public static string GetIP(string hostName)
        {
            Trace.WriteLine(DateTime.Now.ToString() + "\t" +  "Entered SharepointDiscovery GetIP");
            String Ipv4Ipaddress = "";

            IPAddress[] ips = { };
         
            try
            {
                ips = Dns.GetHostAddresses(hostName);
            }
            catch(System.Net.Sockets.SocketException e)
            {
                Console.WriteLine("Failed to get the IP Address from Dns for host :" + hostName + "\nPlease check the Dns and try again");
                Console.WriteLine("SocketException caught!!!");
                Console.WriteLine("Source : " + e.Source);
                Console.WriteLine("Message : " + e.Message);
            }
            catch(Exception ex)
            {
               Console.WriteLine("Failed to get the IP Address from Dns for host :" + hostName + "\nPlease check the Dns and try again");
               Console.WriteLine("Exception caught!!!");
               Console.WriteLine("Source : " + ex.Source);
               Console.WriteLine("Message : " + ex.Message);
            }
           
            Trace.WriteLine(DateTime.Now.ToString() + "\t" + "Time taken in SharepointDiscovery GetIP");

            foreach (IPAddress ipaddress in ips)
            {
                String inputIp = ipaddress.ToString();
                IPAddress address;
                if (IPAddress.TryParse(inputIp, out address))
                {
                    switch (address.AddressFamily)
                    {
                        case System.Net.Sockets.AddressFamily.InterNetwork:
                            Ipv4Ipaddress = inputIp;
                            //Console.WriteLine("IPV4 Address: {0}", inputIp);
                            break;
                        case System.Net.Sockets.AddressFamily.InterNetworkV6:
                            //Console.WriteLine("IPV6 Address: {0}", inputIp);
                            break;
                        default:
                            break;
                    }
                }
            }
            Trace.WriteLine(DateTime.Now.ToString() + "\t" +  "Exited SharepointDiscovery GetIP");
            return Ipv4Ipaddress;
        }

        public bool Discover(bool postToCx)
        {
            bool bStatus = false;
            string xmlRequest = String.Empty;
            DiscoveryInfo discoveryInfo = new DiscoveryInfo();
            if (FillSharepointVersion(discoveryInfo) && FillCurrentDomainDetails(discoveryInfo) && DiscoverSPServers(discoveryInfo) && FillSharepointWebApplicationPorts(discoveryInfo))
            {
                discoveryInfo.DiscoveryServer = Utility.GetLocalHost();
                discoveryInfo.HostId = Utility.GetHostId(discoveryInfo.DiscoveryServer);

                // Serializing DiscoveryInfo object to xml
                //xmlRequest = discoveryInfo.ToXml();  (or)
                using (StringWriter Output = new StringWriter(new StringBuilder()))
                {
                    System.Xml.Serialization.XmlSerializer ser = new System.Xml.Serialization.XmlSerializer(typeof(DiscoveryInfo));

                    // To remove XML declaration in the XML request
                    XmlWriter xw = XmlWriter.Create(Output,
                              new XmlWriterSettings()
                              {
                                  OmitXmlDeclaration = true,
                                  //ConformanceLevel = ConformanceLevel.Fragment,
                                  Indent = true
                              });
                    // To remove namespaces in the XML request
                    System.Xml.Serialization.XmlSerializerNamespaces ns = new System.Xml.Serialization.XmlSerializerNamespaces();
                    ns.Add("",""); 
                    ser.Serialize(xw, discoveryInfo, ns);
                    xmlRequest = Output.ToString();
                }                
                
                Console.WriteLine(xmlRequest);
                bStatus = true;
                if (postToCx)
                {
                    bStatus = PostToCx(xmlRequest);
                }
            }

            return bStatus;
        }

        private bool DiscoverSPServers(DiscoveryInfo discoveryInfo)
        {
            String role = "unknown";
            bool retVal = true;
            SPFarm farm = SPFarm.Local;
            int Server_Count = 0;

            try
            {
                SPServerCollection serverCollection = farm.Servers;
                if (serverCollection == null)
                {
                    Trace.WriteLine(DateTime.Now.ToString() + "\tFound no servers in the farm\n");
                }
                else
                {
                    ServerInfo serverInfo = null;
                    foreach (SPServer myServer in serverCollection)
                    {
                        serverInfo = new ServerInfo();
                        switch (myServer.Role)
                        {
                            case SPServerRole.Application:
                                role = "app";
                                break;
                            case SPServerRole.SingleServer:
                                role = "single";
                                break;
                            case SPServerRole.WebFrontEnd:
                                role = "web";
                                break;
                            case SPServerRole.Invalid:
                                if (Find_Dbserver_or_Not(myServer.Name))
                                {
                                    role = "database";
                                    break;
                                }
                                else
                                {
                                    continue;
                                }
                        }

                        if (Server_Count == 1)
                        {
                            role = "single";
                        }
                        serverInfo.Name = myServer.Name;
                        Trace.WriteLine(String.Format("{0}\tRole={1}, ServerName={2}", DateTime.Now.ToString(), role, serverInfo.Name));
                        serverInfo.IP = GetIP(serverInfo.Name);
                        serverInfo.HostId = Utility.GetHostId(serverInfo.Name);
                        if (role == "app")
                        {
                            discoveryInfo.Role.App.Add(serverInfo);
                        }
                        else if (role == "web")
                        {
                            discoveryInfo.Role.Web.Add(serverInfo);
                        }
                        else if (role == "database")
                        {
                            discoveryInfo.Role.Database.Add(serverInfo);
                        }
                        else if (role == "single")
                        {
                            discoveryInfo.Role.Single = serverInfo;
                        }                        
                    }
                }
            }
            catch (Exception e)
            {                
                Trace.WriteLine(DateTime.Now.ToString() + "\tFailed to discover sharepoint farm. Exception:" + e.ToString());
                retVal = false;
            }

            return retVal;
        }

        private bool FillSharepointVersion(DiscoveryInfo discoveryInfo)
        {
            bool retVal = true;
            try
            {
                string appName = String.Empty;
                Microsoft.SharePoint.Administration.SPFarm farm = SPFarm.Local;             
                switch (farm.BuildVersion.Major)
                {
                    case 12:
                        appName = SHAREPOINT_2007;
                        break;
                    case 14:
                        appName = SHAREPOINT_2010;
                        break;
                    case 15:
                        appName = SHAREPOINT_2013;
                        break;
                }
                discoveryInfo.Version = farm.BuildVersion.ToString();
                discoveryInfo.AppName = appName;
                discoveryInfo.FarmId = farm.Id.ToString();
                Trace.WriteLine(String.Format("{0}\tApp:{1}, Version:{2}, Farm ID:{3}", DateTime.Now.ToString(), appName, discoveryInfo.Version, discoveryInfo.FarmId));
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now.ToString() + "\tFailed to determine SharePoint version. Exception:" + ex.ToString());
                retVal = false;
            }
            return retVal;
        }

        public bool FillSharepointWebApplicationPorts(DiscoveryInfo discoveryInfo)
        {
            bool retVal = true;
            try
            {
                SPAdministrationWebApplication centralAdmininstrationApp = SPAdministrationWebApplication.Local;
                int port = centralAdmininstrationApp.GetResponseUri(SPUrlZone.Default).Port;
                discoveryInfo.WebApplications.Add(new WebApplication("Central Administration", port));
                Trace.WriteLine(String.Format("{0}\tWeb Application Name: Central Administration, Port:{1}", DateTime.Now.ToString(), port));

                SPWebApplicationCollection mySPWebAppCollection = SPWebService.ContentService.WebApplications;
                if (mySPWebAppCollection != null)
                {
                    foreach (SPWebApplication mySPWebApp in mySPWebAppCollection)
                    {
                        port = mySPWebApp.GetResponseUri(SPUrlZone.Default).Port;
                        discoveryInfo.WebApplications.Add(new WebApplication(mySPWebApp.Name, port));
                        Trace.WriteLine(String.Format("{0}\tWeb Application Name: {1}, Port:{2}", DateTime.Now.ToString(), mySPWebApp.Name, port));
                    }
                }
            }
            catch (Exception e)
            {
                Trace.WriteLine(DateTime.Now.ToString() + "\tFailed to get SharePoint Web Application ports. Exception:" + e.ToString());
                retVal = false;
            }
            return retVal;
        }

        private bool FillCurrentDomainDetails(DiscoveryInfo discoveryInfo)
        {
            bool retVal = true;
            try
            {
                discoveryInfo.DomainName = Utility.GetCurrentDomainName();                
                var dcList = Utility.GetCurrentDomainControllers();
                ServerInfo serverInfo = null;
                foreach (string dcName in dcList)
                {                                       
                    if (!String.IsNullOrEmpty(dcName))
                    {
                        serverInfo = new ServerInfo();
                        serverInfo.Name = dcName;
                        serverInfo.IP = GetIP(serverInfo.Name);
                        serverInfo.HostId = Utility.GetHostId(dcName);
                        discoveryInfo.Role.DC.Add(serverInfo);
                    }                   
                }
            }
            catch (Exception e)
            {
                Trace.WriteLine(DateTime.Now.ToString() + "\tFailed to fill the Domain Controller details. Exception:" + e.ToString());
                retVal = false;
            }
            return retVal;
        }

        public bool PostToCx(string xmlRequest)
        {
            bool bStatus = false;

            try
            {
                Trace.WriteLine(DateTime.Now.ToString() + "\tPosting the discovery info to CX");

                // Create WebRequest Object
                string absoluteUriPath = "/sharepoint_discovery.php";
                Uri uri = Configuration.GetCxUri(absoluteUriPath);                

                if (uri == null)
                {
                    Console.WriteLine("Couldn't post discovery information to CX. Error: Failed to get CX configuration");
                }
                else
                {                    
                    Trace.WriteLine(DateTime.Now.ToString() + "\tPost URL:" + uri.AbsoluteUri);
                    int timeOut = 300000;
                    bool useSsl = uri.Scheme == Uri.UriSchemeHttps ? true : false;
                    HttpClient client = new HttpClient(uri.Host, uri.Port, useSsl, timeOut, null);
                    HttpReply reply = client.sendHttpRequest("POST", absoluteUriPath, xmlRequest, "application/x-www-form-urlencoded", null);

                    // Extract response XML
                    if (String.IsNullOrEmpty(reply.error))
                    {                        
                        Trace.WriteLine(DateTime.Now.ToString() + "\tPosted discovery information successfully to CX.");
                        Console.WriteLine("Posted discovery information successfully to CX.");
                        bStatus = true;
                    }
                    else
                    {
                        Trace.WriteLine(DateTime.Now.ToString() + "\tFailed to post discovery info. Error: " + reply.error);
                        Console.WriteLine("Failed to post discovery information to CX. Error: {0}", reply.error);
                    }
                }
            }
            catch (WebException ex)
            {
                Trace.WriteLine(String.Format("{0}\tFailed to post discovery info. Exception:{1}", DateTime.Now.ToString(), ex.ToString()));
                Trace.WriteLine("Status Code: " + ex.Status);
                Console.WriteLine("Failed to post discovery information to CX. Error: {0}", ex.Message);
            }
            catch (Exception ex)
            {
                Trace.WriteLine(DateTime.Now.ToString() + "\tFailed to post discovery info to CX. Exception:" + ex.ToString());
                Console.WriteLine("Failed to post discovery information to CX. Error: {0}", ex.Message);
            }

            return bStatus;
        }        

    }
}
