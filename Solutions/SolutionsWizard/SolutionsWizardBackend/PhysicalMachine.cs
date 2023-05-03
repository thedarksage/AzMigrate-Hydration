using System;
using System.Collections.Generic;
using System.Text;
using Com.Inmage.Tools;
using System.Collections;
using System.Diagnostics;
using System.Windows.Forms;
using System.Management;

namespace Com.Inmage
{
   public class PhysicalMachine
    {
        RemoteWin remote;
        string IP = null;
        string Username = null;
        string Password = null;
        string Domain = null;
        string Datastorename = "datastore";
        private Host host;

        public bool Connected = false;


        public PhysicalMachine(string inIp, string inDomain, string inUserName, string inPassWord)
        {
            try
            {
                bool status;

                string errorMessage = "";
                WmiCode errorCode = WmiCode.Unknown;

                remote = new RemoteWin(inIp, inDomain, inUserName, inPassWord,"");

                status = remote.Connect( errorMessage,  errorCode);

                errorCode = remote.GetWmiCode;
                errorMessage = remote.GetErrorString;

                if (status == true)
                {
                    Connected = true;
                }
                else
                {
                    Connected = false;
                }


                IP = inIp;
                Username = inUserName;
                Password = inPassWord;
                Domain = inDomain;
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
                

            }
           // AppConfig app = new AppConfig();
           // Debug.WriteLine("Calling GetConfigInfo in PhysicalMachine");
          //  _configInfo = app.GetConfigInfo("REMOTE_WIN");
            //app.PrintConfigValues(_configInfo);

        }


        internal class MyDiskOrderCopare : IComparer
        {

            // Calls CaseInsensitiveComparer.Compare with the parameters reversed.
            int IComparer.Compare(Object x, Object y)
            {
                Hashtable disk1 = (Hashtable)x;
                Hashtable disk2 = (Hashtable)y;

                string disk1Name = disk1["Name"].ToString();
                string disk2Name = disk2["Name"].ToString();

                return disk1Name.CompareTo(disk2Name);

            }
        }
        internal class MyNicOrderCopare : IComparer
        {

            // Calls CaseInsensitiveComparer.Compare with the parameters reversed.
            int IComparer.Compare(Object x, Object y)
            {
                Hashtable nic1 = (Hashtable)x;
                Hashtable nic2 = (Hashtable)y;

                string nic11Name = nic1["network_label"].ToString();
                string nic2Name = nic2["network_label"].ToString();

                return nic11Name.CompareTo(nic2Name);

            }
        }


        public bool GetHostInfo()
        {
            host = new Host();
            try
            {

                Hashtable sysInfoHash = new Hashtable();

                if (Connected == false)
                {
                    return false;
                }
                remote.GetSystemInfo( sysInfoHash);

                sysInfoHash = remote.GetSysInfoHash;

                host.hostname = sysInfoHash["HostName"].ToString();






                Trace.WriteLine("Getting disks and partitions for " + host.hostname);

                host.disks.GetDisksAndPartitions(remote);
                host.nicList.GetNetworkInfo(remote);
                IComparer myComparer = new MyDiskOrderCopare();

                host.disks.list.Sort(myComparer);
                // Trying to sort with network lable using icomparer
                try
                {
                    IComparer comparer = new MyNicOrderCopare();
                    host.nicList.list.Sort(comparer);
                }
                catch (Exception ex)
                {
                    Trace.WriteLine(DateTime.Now + "\t Failed to sort nics");
                }
                Trace.WriteLine(DateTime.Now + "\t after sorting the nics ");
                foreach (Hashtable hash in host.nicList.list)
                {
                    Trace.WriteLine(DateTime.Now + "\t network label " + hash["network_label"].ToString());
                }
                for (int i = 0; i < host.disks.list.Count; i++)
                {
                    ((Hashtable)host.disks.list[i])["Name"] = "[DATASTORE_NAME] " + host.hostname + "/" + ((Hashtable)host.disks.list[i])["Name"] + ".vmdk";

                }






                host.ip = IP;
                host.domain = Domain;
                host.userName = Username;
                host.passWord = Password;
                host.role = Role.Primary;
                host.folder_path = host.hostname + "/";
                host.vmx_path = "[DATASTORE_NAME] " + host.folder_path + host.hostname + ".vmx";
                host.displayname = host.hostname;
                host.datastore = Datastorename;
                host.snapshot = "SNAPFALSE";
                host.number_of_disks = host.disks.list.Count;
                //Change this when adding linux
                host.operatingSystem = sysInfoHash["OperatingSystem"].ToString();
                host.os = OStype.Windows;
                host.ide_count = "1";
                host.floppy_device_count = "0";
                host.vmxversion = "vmx-07";
                host.resourcePool = "dummy";
                host.osEdition = sysInfoHash["osEdition"].ToString();
                host.alt_guest_name = host.osEdition;
                if (sysInfoHash["cpucount"] != null)
                {
                    host.cpuCount = int.Parse(sysInfoHash["cpucount"].ToString());

                }
                if (sysInfoHash["memsize"] != null)
                {
                    ulong memsize = (ulong.Parse(sysInfoHash["memsize"].ToString())) / (1024 * 1024);
                    host.memSize = int.Parse(memsize.ToString().Split('.')[0]);

                }
                host.collectedDetails = true;
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


            }
            return true;
        }

        public Host GetHost
        {
            get
            {
                return host;
            }
            set
            {
                value = host;
            }
        }







       internal static void PrintHash(Hashtable inHash)
        {
            foreach (string key in inHash.Keys)
            {
                Console.WriteLine(key + " = " + inHash[key]);
            }
        }





   

        //public static void PrintArrayListOfHashes(ref ArrayList refArrayList)
        //{
        //    try
        //    {
        //        foreach (Hashtable valHash in refArrayList)
        //        {
        //            Console.WriteLine("_____________________________________________________");
        //            foreach (string key in valHash.Keys)
        //            {
        //                Console.WriteLine(key + "\t=\t" + valHash[key]);
        //            }


        //        }
        //        Trace.WriteLine("Total Entries = {0} \n", refArrayList.Count.ToString());
        //    }
        //    catch (Exception ex)
        //    {

        //        System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
        //        System.FormatException formatException = new FormatException("Inner exception", ex);
        //        Trace.WriteLine(formatException.GetBaseException());
        //        Trace.WriteLine(formatException.Data);
        //        Trace.WriteLine(formatException.InnerException);
        //        Trace.WriteLine(formatException.Message);
        //        Trace.WriteLine("ERROR*******************************************");
        //        Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
        //        Trace.WriteLine("Exception  =  " + ex.Message);
        //        Trace.WriteLine("ERROR___________________________________________");


        //    }
            
        //}

        

    }
}
