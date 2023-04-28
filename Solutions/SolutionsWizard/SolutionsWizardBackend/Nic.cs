using System;
using System.Collections.Generic;
using System.Text;
using Com.Inmage.Tools;
using System.Collections;
using System.Management;
using System.Diagnostics;
using System.Windows.Forms;
using System.IO;
using System.Xml;
using System.Net;


namespace Com.Inmage.Esxcalls
{
    [Serializable]
    public class Nic
    {
        public ArrayList list;
      
       
        public Nic()
        {
            list = new ArrayList();
            
        }

        internal bool GetNetworkInfo(RemoteWin remote)
        {


            try
            {
                // In case of p2v ui is getting the nic info...
                // there are so many nics. we are didplaying only nics where mac address is not null and manufacturer is microsoft only...
                ObjectQuery query = new ObjectQuery(

                    "SELECT * FROM Win32_NetworkAdapter where macaddress !=null and manufacturer != 'microsoft' ");


                ManagementObjectSearcher searcher =
                    new ManagementObjectSearcher(remote.scope, query);
                if (searcher.Get().Count == 0)
                {
                    query = new ObjectQuery("SELECT * FROM Win32_NetworkAdapter where  name like 'Microsoft Virtual Machine Bus Network Adapter%' and macaddress !=null");
                    searcher = new ManagementObjectSearcher(remote.scope, query);
                }
                ObjectQuery query1 = new ObjectQuery(

                        "SELECT * FROM  Win32_NetworkAdapterConfiguration where MACaddress != null ");
                ManagementObjectSearcher searcher1 =
                    new ManagementObjectSearcher(remote.scope, query1);
                int nicCount = 1;

                foreach (ManagementObject wmiquery in searcher.Get())
                {                                       
                    
                    foreach (ManagementObject queryObj in searcher1.Get())
                    {
                        
                        if (wmiquery["MACAddress"].ToString() == queryObj["MACAddress"].ToString())
                        {

                            
                            Hashtable nicHash = new Hashtable();




                            if (queryObj["IPAddress"] != null)
                            {

                                String[] ips = (String[])(queryObj["IPAddress"]);


                                string ip = ips[0];
                                nicHash.Add("ip", ip);

                                Trace.WriteLine(DateTime.Now + "\t\t Ipaddresses:" + ip);

                            }
                            else
                            {
                                nicHash.Add("ip", null);

                            }
                            if (queryObj["IPSubnet"] != null)
                            {
                                String[] subnetmaskips = (String[])(queryObj["IPSubnet"]);


                                string subnetmaskip = subnetmaskips[0];
                                nicHash.Add("mask", subnetmaskip);
                                Trace.WriteLine(DateTime.Now + "\t\t NetMask:" + subnetmaskip);
                            }
                            else
                            {
                                nicHash.Add("mask", null);
                            }
                            if (queryObj["DefaultIPGateway"] != null)
                            {
                                String[] gateways = (String[])(queryObj["DefaultIPGateway"]);


                                string gateway = gateways[0];
                                nicHash.Add("gateway", gateway);
                            }
                            else
                            {
                                nicHash.Add("gateway", null);
                            }
                            if (queryObj["DHCPEnabled"] != null)
                            {
                                if (bool.Parse(queryObj["DHCPEnabled"].ToString()) == true)
                                {
                                    nicHash.Add("dhcp", "1");
                                }
                                else
                                {
                                    nicHash.Add("dhcp", "0");
                                }
                            }
                            else
                            {
                                nicHash.Add("dhcp", "0");
                            }
                            if (queryObj["DNSServerSearchOrder"] != null)
                            {
                                String[] dnsips = (String[])(queryObj["DNSServerSearchOrder"]);
                                string dnsip = dnsips[0];
                                nicHash.Add("dnsip", dnsip);
                            }
                            else
                            {
                                nicHash.Add("dnsip", null);
                            }
                            if (queryObj["MACAddress"] != null)
                            {
                                nicHash.Add("nic_mac", queryObj["MACAddress"].ToString());
                            }
                            else
                            {
                                nicHash.Add("nic_mac", null);
                            }

                            // We are taking all new values as null and then when user gives new values 
                            // we are assing them to the particular nics.
                            nicHash.Add("new_ip", null);
                            nicHash.Add("new_mask", null);
                            nicHash.Add("new_dnsip", null);
                            nicHash.Add("new_gateway", null);
                            nicHash.Add("new_network_name", null);
                            nicHash.Add("Selected", "yes");
                            nicHash.Add("network_label", "Network adapter " + nicCount.ToString());
                            nicHash.Add("network_name", null);
                            
                            list.Add(nicHash);
                            nicCount++;

                        }
                    }
                }
                Trace.WriteLine(DateTime.Now + "\t\t Total Nics found on this Machine:" + (--nicCount).ToString());
            }
            catch (ManagementException err)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(err, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + err.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return false;
            }
            catch (System.UnauthorizedAccessException unauthorizedErr)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(unauthorizedErr, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + unauthorizedErr.Message);
                Trace.WriteLine("ERROR___________________________________________");
                Trace.WriteLine("01_001_007: " + "An error occurred while querying for WMI data: " + unauthorizedErr.Message);
                MessageBox.Show("Connection error (user name or password might be incorrect): " + unauthorizedErr.Message);
                return false;
            }
            return true;
        }

    }
}
