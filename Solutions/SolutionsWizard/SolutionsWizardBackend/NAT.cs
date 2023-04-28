using System;
using System.Collections.Generic;
using System.Text;
using System.Collections;
using System.Net.NetworkInformation;
using System.Diagnostics;
using System.IO;
using System.Xml;
using System.Net;
using System.Windows.Forms;
using System.Management;
using HttpCommunication;


namespace Com.Inmage.Tools
{
    public class Nat
    {
        public enum Location { Primarysite = 0, Secondarysite = 1 };

        static public Location Vconlocation = Location.Primarysite;
        static public Location Cxlocation = Location.Primarysite;
        static public Location Pslocation = Location.Primarysite;
        public static string CxlocalIP = null;
        public static string CxnatIP = null;
        public static bool IssourceIsnated = false;
        public static bool IstargetIsnated = false;
        internal static string latestPath = WinTools.Vxagentpath() + "\\vContinuum\\Latest";
        internal void NatLogic()
        {

            if (Vconlocation == Location.Secondarysite)
            {
                // 1. Get NAT values of each VM to be protected if we are going to push agent
                //  2. Set target NAT'd = YES/TRUE in ESX.XML

                if (Cxlocation == Location.Primarysite)
                {
                    //Get NAT values for CX
                    //Get local IP of CX
                }
                else
                {
                    //Get NAT IP of CX and use NAT IP in push agent for source VMS
                }

                if (Pslocation == Location.Primarysite)
                {
                    //Get NAT values for PS
                    //Get local IP of PS
                }

            }



        }

        public static bool GetPSLocalNatIP(ref ArrayList psdetails)
        {
            string _xmlString = "";
           
            try
            {
                
                String postData1 = "&operation=";

                // xmlData would be send as the value of variable called "xml"
                // This data would then be wrtitten in to a file callied input.xml
                // on CX and then given as argument to the cx_cli.php script.
                int operation = 2;

                String postData2 = "&userfile='input.xml'&xml=" + _xmlString;


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
                // Close the Stream object.
               
                Trace.WriteLine(responseFromServer);
                Debug.WriteLine("proceeding ...");
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
                   // reader1.Close();
                }
                XmlNodeList xPNodeList = xDocument.SelectNodes(".//process_server");

                
                foreach (XmlNode xNode in xPNodeList)
                {
                    Hashtable hash = new Hashtable();
                    hash["localip"] = xNode.ChildNodes.Item(1).InnerText.ToString();
                    if (xNode.ChildNodes.Item(4).InnerText.Length != 0)
                    {
                        hash["natip"] = xNode.ChildNodes.Item(4).InnerText.ToString();
                    }
                    else
                    {
                        hash["natip"] = null;
                    }
                    psdetails.Add(hash);
                }
            }
            catch (System.Net.WebException e)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + e.Message);
                Trace.WriteLine("ERROR___________________________________________");

                return false;
            }

            catch (System.Xml.XmlException e)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + e.Message);
                Trace.WriteLine("ERROR___________________________________________");


                return false;


            }

            catch (Exception e)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + e.Message);
                Trace.WriteLine("ERROR___________________________________________");


                return false;



            }

            return true;


        }


        public static bool SetPSNatIP(string psLocalIP, string psnatIP)
        {


           
            string cxIP = WinTools.GetcxIPWithPortNumber();

           

            string inPutString = "<pair_details>" + Environment.NewLine +
                                 "   <volume_replication>" + Environment.NewLine +
                                 "	<ps_details>" + Environment.NewLine +
                                 "		<process_server_ip>" + psLocalIP + "</process_server_ip>" + Environment.NewLine +
                                 "		<ps_nat_ip>" + psnatIP + "</ps_nat_ip>" + Environment.NewLine +
                                 "	</ps_details>" + Environment.NewLine +
                                 "	</volume_replication>" + Environment.NewLine +
                                 "</pair_details>";




            try
            {
               
                String postData1 = "&operation=";

                // xmlData would be send as the value of variable called "xml"
                // This data would then be wrtitten in to a file callied input.xml
                // on CX and then given as argument to the cx_cli.php script.
                int operation = 50;

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
                Trace.WriteLine("Printing Response from CX...");
                Trace.WriteLine(responseFromServer);
                Trace.WriteLine("proceeding ...");


            }
            catch (System.Net.WebException e)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + e.Message);
                Trace.WriteLine("ERROR___________________________________________");

                return false;
            }

            catch (System.Xml.XmlException e)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + e.Message);
                Trace.WriteLine("ERROR___________________________________________");


                return false;


            }

            catch (Exception e)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + e.Message);
                Trace.WriteLine("ERROR___________________________________________");


                return false;



            }

            return true;

        }
    }
}
