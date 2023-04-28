using System;
using System.Collections.Generic;
using System.Text;
using System.Collections;
using System.Threading;
using System.Configuration;
using com.InMage;
using com.InMage.ESX;
using com.InMage.Tools;
using System.Windows.Forms;
using System.Diagnostics;
using System.IO;
using System.Management;



namespace com.InMage
{
    class MainClassLibrary
    {
        
        static void Main(string[] args)
        {

            string domain = "", userName = "", passWord = "", ip = "", operation = "";
            string sourceDir = "", targetDir = "";

            ArrayList ipList = new ArrayList();
            MainClassLibrary mc = new MainClassLibrary();


            if (args.Length < 5)
            {
                Console.WriteLine("Usaage:  " + Application.ExecutablePath + "Operation Domain username password ipList");
                Environment.Exit(-1);
            }
            else
            {
                operation   = args[0];
                domain      = args[1];
                userName    = args[2];
                passWord    = args[3];


               
            }


            switch (operation)
            {
                case "diskinfo":

                    for (int i = 4; i < args.Length; i++)
                    {
                        ipList.Add(args[i]);
                    }
               
                    mc.GetDiskInfo(domain, userName, passWord, ipList);
                    break;

                case "copy":
                    ip = args[4];
                    sourceDir = args[5];
                    targetDir = args[6];

                    Debug.WriteLine( "Copying dir " + sourceDir + " to " + targetDir + "  from local to " +  ip );
                    mc.Copy(domain, userName, passWord, ip, sourceDir, targetDir);
                    break;

                default:
                    break;
            }

         
     

           

        /*
            RemoteWin remote;

            remote = new RemoteWin("10.0.1.1", "qa-domain", "Administrator", "inScott!");
            remote.Connect();

            MainClassLibrary mc = new MainClassLibrary();
            
         
           // mc.GetDriverInfoTest(remote);


            /*
            HostList hList = new HostList();

            Host h1 = new Host();
            h1.hostName = "host1";

            Host h2 = new Host();
            h2.hostName = "host2";

            hList.AddOrReplaceHost(h1);
            hList.AddOrReplaceHost(h2);
         
            hlist.WriteToXml("filename");

            hList.RemoveHost(h2);

            int index=0;

            if( hList.DoesHostExist(h1, ref index ) )
            {
                Debug.WriteLine("Host exists");
             
            }


            hList.Print();

            */


           // RemoteWin remote;


            //PushInstallToWindows windowsPush = new PushInstallToWindows();

           // windowsPush.PushAgent("10.0.1.33", "QA-DOMAIN", "Administrator", "inScott!", "e:\\temp\\esx", "10.0.1.145", "Adminstrator", "inScott!", false, "inScott!");

            //windowsPush.PushAgent("10.0.1.45", "sdfs", "Administrator", "inScott!", "e:\\temp\\esx", "10.0.1.145", "Adminstrator", "inScott!", false, "inScott!");


          // remote = new RemoteWin("10.0.228.34", "qa-domain", "Administrator", "inScott!");
          // remote.Connect();
          // remote.Execute( "notepad.exe" );




            
            
            //Trace.WriteLine("Test");
            //InMageLogger.Dispose();

           // AppConfig app = new AppConfig();
           // Hashtable configInfo = app.GetConfigInfo("ERROR_LOG");
            //app.PrintConfigValues(configInfo);


            /*
            Esx esxInfo = new Esx();

            esxInfo.GetEsxInfo("10.0.1.79", "root", "inScott!", role.SECONDARY);

            esxInfo.PrintVms();
            */
          

         
          
/*
            string command = "perl.exe ";
            string options =   Directory.GetCurrentDirectory() + "\\WriteSourceInfoInXml.pl" + " --server 10.0.1.79 --username root --password inScott! --ostype windows";
            Console.WriteLine("Command = \n" + command + options);
            WinTools.Execute(command, options, 10000);
            */

          //  Esx esxInfo = new Esx( "10.0.1.79", "root", "inScott!", "XML.xml");
           
          //  esxInfo.PrintSourceVms();


            //int hostCount = esxInfo.getSourceEsxHostList().Count;


       
               


         //  Process.Start(command, options);

        

            //xmlc.ReadVirtualSolutionXml(@"XML.xml");





           // ArrayList diskInfo = new ArrayList();

          //  PushInstallToWindows pinstall = new PushInstallToWindows( "10.0.1.34", "dev-domain", "Administrator" );

         /*   string machineIP, string domain, string userName, string passWord,
                                    string pathOfBinary, string cxIP, string userNameFxService, 
                                    string pwFxService, bool restart, string domainUserPassword )
                    
            */
            //RemoteWin remote = new RemoteWin("10.0.1.34", "dev-domain", "Administrator", "inScott!");
            //remote.Connect();
            //remote.CopyFileTo("c:\\temp", "testfile2.txt", "c:\\");






            // CmdArgs would parse all the commands

           //  MainClassLibrary cl = new MainClassLibrary();
           // cl.RemoteExecuteTest(remote);

           // cl.GetSystemInfoTest(remote);


         //   cl.GetPhysicalDiskSizesTest(remote);

          //  string vminfoLine = cl.GetVmInfoEntry( remote, "10.0.1.34");

         //   Console.WriteLine("vmInfo line = " + vminfoLine);

            //CopyUtility.RemoteCopyDir("10.0.0.48", "c:\\test", "10.0.1.34", "c:\\test");
          //  cl.CopyFromRemoteTest(remote, "c:\\temp\\testfile.txt", "c:\\temp\\testfile.txt");


         
           
           // Console.ReadLine();

        }


        public bool GetDiskInfo( string inDomain, string inUserName, string inPassWord, ArrayList inIpList )
        {
            HostList primaryHostList = new HostList();

            foreach (string pIp in inIpList)
            {
                PhysicalMachine p = new PhysicalMachine( pIp, inDomain, inUserName, inPassWord );
                Host h = p.GetHostInfo();


                primaryHostList.AddOrReplaceHost(h);

            }

            primaryHostList.WriteToXmlFile("SourceXML.xml", "SRC_ESX");


        }

        public bool Copy(string inDomain, string inUserName, string inPassWord, string inIp, string inSourceDir, string inTargetDir)
        {


        }

        public bool TestXmlWithDummyHosts()
        {

            HostList primaryHostList = new HostList();

            Host h1 = new Host();
            h1.hostName = "host1";

            Hashtable h1drives = new Hashtable();
            Hashtable h2drives = new Hashtable();

            h1drives["Name"] = "disk1";
            h1drives["Size"] = "10003434";
            h2drives["Name"] = "disk2";
            h2drives["Size"] = "10999999434";



            


            ArrayList driveList = new ArrayList();

            driveList.Add(h1drives);
            driveList.Add(h2drives);

            h1.driveList = driveList;

            Host h2 = new Host();
            h2.hostName = "host2";
            h2.driveList = driveList;

            primaryHostList.AddOrReplaceHost(h1);
            primaryHostList.AddOrReplaceHost(h2);



            primaryHostList.AddOrReplaceHost(h1);
            primaryHostList.AddOrReplaceHost(h2);


            primaryHostList.WriteToXmlFile("c:\\temp\\myxml.xml", "root");


            return true;

        }
        public string GetVmInfoEntry( RemoteWin remote, string ip)
        {
            string vmInfoDelimiter="@!@!" , vmdkSizeDelimiter="###", vmdkDelimiter = "|";
            try
            {
                vmInfoDelimiter = ConfigurationManager.AppSettings["ESX_Vminfo_Delimiter"].ToString();
                vmdkSizeDelimiter = ConfigurationManager.AppSettings["ESX_Vmdk_Size_Delimiter"].ToString();
                vmdkDelimiter = ConfigurationManager.AppSettings["ESX_Vmdk_Delimiter"].ToString();
            }
            catch (NullReferenceException err)
            {
                MessageBox.Show("Please provide values for configuration parameters ESX_Vminfo_Delimiter and ESX_Vmdk_Delimiter in the config file" + err.Message);
            }
            
            string vmEntry;
          
            Hashtable sysInfoHash = new Hashtable();

            remote.GetSystemInfo(ref sysInfoHash);
            ArrayList physicalDiskList = new ArrayList();
            remote.GetPhysicalDiskSizes(ref physicalDiskList);

            vmEntry = "DummyResourcePool" + vmInfoDelimiter + sysInfoHash["HostName"] + vmInfoDelimiter + ip + vmInfoDelimiter +  "DummySnapShotDataStore" + vmInfoDelimiter + "SNAPFALSE" + vmInfoDelimiter + sysInfoHash["HostName"] + "/" + vmInfoDelimiter + sysInfoHash["HostName"] + "/" + sysInfoHash["HostName"] + ".vmx" + vmInfoDelimiter;

            int diskNum = 0;
            long diskSizeKBs = 0;
            

            foreach (Hashtable diskHash in physicalDiskList)
            {

                string diskSize = diskHash["Size"].ToString();
                Console.WriteLine("Disk size " + diskSize);

                 //Convert in to Kilo Bytes from Bytes.
                 diskSizeKBs = Int64.Parse(diskSize)/(1024);
                 Console.WriteLine("Disk size " + diskSize + "Disk size in int " + diskSizeKBs);

                string diskEntry = "[DummyDataStore] " + sysInfoHash["HostName"] + "/" + "disk" + diskNum + ".vmdk" + vmdkSizeDelimiter +diskSizeKBs;
                vmEntry = vmEntry +diskEntry + vmdkDelimiter;

                //Increment the disknumbe. So that it will be disk0 disk1 etc
                diskNum++;
            }

            return vmEntry;

        }


       

        

        public void RemoteExecuteTest(RemoteWin remote)
        {
            int processId;
             processId = remote.Execute("mkdir \"c:\\temp\\test5\"");

             while ( remote.ProcessIsRunning(processId) )
             {
                 Console.WriteLine("Process" + processId + " is still running");
                 Thread.Sleep(100);
             }
            
             Console.WriteLine("Process is terminated");
             
        }

        public void GetDriverInfoTest(RemoteWin remote)
        {
            ArrayList diskList = new ArrayList();


            remote.GetDriverInfo(ref diskList);
            

            PrintArrayListOfHashes(ref diskList);
           
       
        }
        public void GetPartitionInfoTest(RemoteWin remote)
        {
            ArrayList diskList = new ArrayList();
     

            remote.GetPartitionInfo(ref diskList);

            PrintArrayListOfHashes(ref diskList);
           

        }

        public void PrintArrayListOfHashes(ref ArrayList refArrayList)
        {

            foreach (Hashtable valHash in refArrayList)
            {
                Console.WriteLine("_____________________________________________________");
                foreach (string key in valHash.Keys)
                {
                    Console.WriteLine(key + "\t=\t" + valHash[key]);
                }


            }

            Console.WriteLine("Total Entries = {0} \n", refArrayList.Count);
        }

      

       

      
        public void GetSCSIAdapterTest(RemoteWin remote)
        {
            
            ArrayList scsiAdapterList = new ArrayList();
            
            remote.GetScsiAdapterList(ref scsiAdapterList);
            Console.WriteLine("Got the scsi data");
            PrintArrayListOfHashes(ref scsiAdapterList);

           
        }

    }
}
