using System;
using System.Collections.Generic;
using System.Linq;
using System.Diagnostics;
using System.DirectoryServices.ActiveDirectory;
using System.Net.NetworkInformation;

namespace spapp
{
    class Utility
    {
        public static List<string> GetCurrentDomainControllers()
        {
            List<string> dcList = new List<string>();
            try
            {
                Domain domain = Domain.GetCurrentDomain();
                var domainControllers = domain.FindAllDomainControllers();
                string dcName = String.Empty;
                foreach (DomainController dc in domainControllers)
                {                    
                    if (false == String.IsNullOrEmpty(dc.Name))
                    {
                        dcName = dc.Name.Substring(0, dc.Name.IndexOf('.'));
                    }
                    dcList.Add(dcName);
                    Trace.WriteLine(String.Format("{0}\tRole=DC, ServerName={1}", DateTime.Now.ToString(), dcName));
                }
            }
            catch (Exception e)
            {
                Trace.WriteLine(DateTime.Now.ToString() + "\tFailed to enum Domain Controllers of the local machine. Exception:" + e.ToString());
            }
            return dcList;
        }

        public static string GetCurrentDomainName()
        {
            string domainName = String.Empty;
            try
            {
                Domain domain = Domain.GetCurrentDomain();
                domainName = domain.Name;
                //domainName = System.Net.NetworkInformation.IPGlobalProperties.GetIPGlobalProperties().DomainName;
                Trace.WriteLine(String.Format("{0}\tDomainName={1}", DateTime.Now.ToString() , domainName));
            }
            catch (Exception e)
            {
                Trace.WriteLine(String.Format("{0}\tFailed to get Domain name of the local machine. Exception:{1}", DateTime.Now.ToString() , e.ToString()));
            }
            return domainName;
        }

        public static string GetHostId(string serverName)
        {
            string hostId = Configuration.GetRemoteRegistryValue(serverName, Microsoft.Win32.RegistryHive.LocalMachine, "SOFTWARE\\Wow6432Node\\SV Systems\\", "HostId");

            return hostId;
        }

        public static string GetLocalHost()
        {
            string hostName = String.Empty;
            try
            {
                hostName = System.Net.Dns.GetHostName().ToUpper();
            }
            catch (Exception ex)
            {
                Trace.WriteLine(String.Format("{0}\tFailed to get local host name. Exception:{1}", DateTime.Now.ToString(), ex.ToString()));
            }
            return hostName;
        }

        public static void DisplayDnsAddresses()
        {
            try
            {
                NetworkInterface[] adapters = NetworkInterface.GetAllNetworkInterfaces();
                foreach (NetworkInterface adapter in adapters)
                {
                    IPInterfaceProperties adapterProperties = adapter.GetIPProperties();
                    IPAddressCollection dnsServers = adapterProperties.DnsAddresses;
                    if (dnsServers.Count > 0)
                    {
                        Console.WriteLine(adapter.Description);
                        foreach (System.Net.IPAddress dns in dnsServers)
                        {
                            switch (dns.AddressFamily)
                            {
                                case System.Net.Sockets.AddressFamily.InterNetwork:
                                    Console.WriteLine("  DNS Server ............................. : {0}", dns.ToString());
                                    break;
                                case System.Net.Sockets.AddressFamily.InterNetworkV6:
                                    //Console.WriteLine("  DNS Server IPV6 Address................. : {0}", dns.ToString());                                    
                                    break;
                                default:
                                    break;
                            }
                        }
                        Console.WriteLine();
                    }
                }
            }
            catch (Exception ex)
            {
                Trace.WriteLine(String.Format("{0}\tFailed to get DNS servers. Exception:{1}", DateTime.Now.ToString(), ex.ToString()));
            }
        }

    }
}
