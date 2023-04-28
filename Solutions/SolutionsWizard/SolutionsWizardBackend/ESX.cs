using System;
using System.Collections.Generic;
using System.Collections;
using System.Text;
using System.IO;
using System.Diagnostics;
using System.Xml;
using Com.Inmage.Tools;
using System.Windows.Forms;
using Microsoft.Win32;
using System.Runtime.Serialization.Formatters.Binary;



namespace Com.Inmage.Esxcalls
{

    public enum OperationType { Initialprotection = 0, Additionofdisk, Offlinesync, Failback, Resume, Drdrill, Recover, V2P, Resize,Removedisk, Removevolume};

    [Serializable]
    public class Esx : ICloneable
    {//Error Codes Range: 01_004_

        private static TraceSwitch esxTrcSwitch = null;

        private string Perlbinary = "perl.exe";
        private string Offlinesyncscript = "OfflineSync.pl";
        private string Failbackresumescript = "Failback_Resume.pl";
        private string ESX_SCRIPT = "GetInfo.pl ";
        private string READINESS_CHECK_SCRIPT = "ReadinessChecks.pl";
        private string CREATE_DUMMY_DISK_SCRIPT = "CreateDummyDisk.pl";
        private string RECOVERY_SCRIPT_FOR_WMI = "Recovery.pl";
        private string SOURCE_XML_FILE = "Info.xml ";
        private string TARGET_XML_FILE = "Info.xml ";
        private string Commandtowritescsiunitsfile = "--scsiunit yes";
        private string SOURCE_ESX_COMMAND = "  --server {0} --ostype {1}";
        private string SOURCE_ESX_COMMAND_FOR_RESIZE = "  --server {0} --resize yes";
        private string SOURCE_ESX_COMMAND_FOR_ONLY_HOSTS = "  --server {0} --ostype {1}  --retrieve {2}";
        private string SOURCE_ESX_COMMAND_FOR_ONLY_LUNS = "  --server {0} --retrieve {1}";
        private string TARGET_ESX_COMMAND = "  --server {0} --ostype {1} --target yes";
        private string TARGET_ESX_COMMAND_FOR_VMDKS = "  --server {0} --ostype {1} --target yes --list_disk yes";
        private string Protectscript = "Protect.pl";
        private string Createtargetguestscommand = "  --server {0} ";
        private string Createguestontargetforofflinesynccommand = "--server {0} -offline_sync_export yes";
        private string Jobautomationscript = "jobautomation.pl";
        private string ESX_UTIL_WIN = "EsxUtilWin.exe";
        private string Jobautomationscriptcommand = "--vconmt {0} ";
        private string DOWNLOAD_ESX_MASTER_XML_COMMAND = "  --server {0}  --get_file {1} ";
        private string UPLOAD_ESX_MASTER_XML_COMMAND = " --server {0} --put_file {1}";
        private string VLIDATE_ESX_COMMAND = " --server {0} --validate";

        private string DOWNLOAD_ESX_XML_COMMAND_P2V = "  --server {0}  --incremental_disk yes --physical_machine yes --ostype Windows";
        private string Createguestforincrementaldisk = "  --server {0} --incremental_disk yes ";
        private string Createguestforresize = "  --server {0} --resize yes ";
        private string Createguestforresume = "  --server {0} --resume yes ";
        private string Createguestfordrdrill = "  --server {0} --dr_drill yes ";
        private string CREATE_GUEST_FOR_DRDRILL_ARRAYBASED = "  --server {0} --dr_drill_array yes ";
        private string CREATE_DATASTORES_FOR_DRDRILL = " --server {0} --array_datastore yes ";
        private string Jobautomationforincrementaldisk = " --incremental_disk yes --vconmt {0}";
        private string Jobautomationfordrdrill = " --esx_ip {0} --recovery {1} --dr_drill yes";
        private string PRE_CHECKS_COMMAND = "--server {0}  ";
        private string POWERED_OFF_COMMAND = " --server {0} --power_off {1} ";
        private string Createguestforfailback = " --server {0} --failback yes ";
        private string jobautomationfailbackcommand = " --failback yes --vconmt {0}";
        private string RECOVERY_OPTIONS = " --recovery yes --esx_ip {0} ";

        private string POWER_ON_COMMAND = "  --server {0} --power_on {3} ";
        private string GENERATE_VMX_FILE = " --server {0} --getvmxfiles yes ";
        private string GENERATE_VMX_FILE_FOR_RESUME_TARGET = " --server {0}  --target yes --resume yes ";
        private string GENERATE_VMX_FILE_FOR_RESUME = " --server {0} --getvmxfiles yes --resume yes ";
        private string GET_FILE_FOR_OFFLINE_SYNC = " --server {0} --get_file {1} --offline_sync_import yes";
        private string PUT_FILE_FOR_OFFLINE_SYNC = "--server {0} --put_file {1}  --datastorename {4}";
        private string TARGET_SCRIPT_COMMAND_FOR_OFFLINE_SYNC = "--server {0} --offline_sync_import yes  ";
        private string DETACH_DISKS_FROM_MASTER_TARGET = "--server {0} --remove yes --directory {1}";
        private string PRE_CHECKS_COMMAND_FOR_FAILBACK = "--server {0} --failback yes";
        private string PRE_CHECKS_COMMAND_FOR_INCREMENTAL_DISK = "--server {0} --incremental_disk yes";
        private string PRE_CHECKS_COMMAND_FOR_OFFLINE_SYNC_IMPORT = "--server {0} --offline_sync_import yes";
        private string PRE_CHECKS_COMMAND_FOR_OFFLINE_SYNC_EXP = "--server {0} --offline_sync_export yes";
        private string PRE_CHECKS_COMMAND_FOR_RESUME = "--server {0} --resume yes";
        private string PRE_CHECKS_COMMAND_FOR_DRDRILL = "--server {0} --dr_drill yes";
        private string PRE_CHECKS_COMMAND_FOR_DRDRILL_ARRAYBASED = "--server {0} --dr_drill_array yes";
        private string PRE_CHECKS_COMMAND_ADD_DISK_TARGET = "--server {0} --incremental_disk yes --target yes";
        private string PRE_CHECKS_COMMAND_RESIZE = "--server {0} --resize yes --target yes";
        private string PRE_CHECKS_COMMAND_AT_CREATING_DATASTORES = "--server {0} --array_datastore yes";
        private string DUMMY_DISKS_FOR_WINDOWS_PROTECTION = "--server {0} ";
        private string DUMMY_DISKS_FOR_WINDOWS_PROTECTION_FOR_DR_DRILL = "--server {0} --dr_drill yes";
        private string WMI_BASED_RECOVERY_COMMAND = " --server {0} --directory {1} --recovery yes --cxpath {2}";
        private string WMI_BASED_DRDRILL_COMMAND = " --server {0} --directory {1} --dr_drill yes";
        private string WMI_BASED_V2PRECOVERY_COMMAND = " --server {0} --directory {1} --recovery_v2p yes ";
        private string installPath = "";
        private static string VERSIONOFPRIMARYVSPHERESERVER = null;
        private static string VERSIONOFSECONDARYVSPHERE_SERVER = null;
        private static string INCREMENTAL_DISK_SOURCE_CONNECTION = "--incremental_disk yes --server {0}";
        private static int Perltimeout = 12000000; //Time out value is in Millseconds. 12000000 corresponds to 12000 seconds corresponds to 200 minute corresponds to 3hrs 20 mins
        //Time out values is 3 hrs 20 mins.
        internal string Esxutilwinfortarget = "-role {0} -d {1}";

        public string ip = "";
        public string userName = "";
        public string passWord = "";
        public ArrayList diskMapping;
        public ArrayList hostList;
        public HostList selectedHosts = new HostList();
        public CredentialsList credList = new CredentialsList();

        public ArrayList dataStoreList = new ArrayList();
        public ArrayList resourcePoolList = new ArrayList();
        public ArrayList lunList = new ArrayList();
        public ArrayList networkList = new ArrayList();
        public ArrayList configurationList = new ArrayList();
        public string scriptsPath = null;
        public string filePath = null;


        // string      _xmlFile;
        Role _role;


        static Esx()
        {

            esxTrcSwitch = new TraceSwitch("EsxFilter", "Esx trace output filter settings");

        }

        public Esx()
        {

            hostList = new ArrayList();

            dataStoreList = new ArrayList();

            selectedHosts = new HostList();

            credList = new CredentialsList();
            resourcePoolList = new ArrayList();
            installPath = WinTools.FxAgentPath() + "\\vContinuum";

            scriptsPath = installPath + "\\Scripts";
            Directory.SetCurrentDirectory(scriptsPath);
            filePath = installPath + "\\Latest";
            lunList = new ArrayList();
            networkList = new ArrayList();
            configurationList = new ArrayList();
        }
        

        public object Clone()
        {

            MemoryStream memoryStream = new MemoryStream();

            BinaryFormatter binaryFormatter = new BinaryFormatter();

            binaryFormatter.Serialize(memoryStream, this);

            memoryStream.Position = 0;

            object obj = binaryFormatter.Deserialize(memoryStream);

            memoryStream.Close();

            return obj;

        }


        public static string Sourceversionnumber
        {

            get
            {
                return VERSIONOFPRIMARYVSPHERESERVER;
            }
            set
            {
                VERSIONOFPRIMARYVSPHERESERVER = value;
            }

        }

        public static string Targetversionnumber
        {
            get
            {
                return VERSIONOFSECONDARYVSPHERE_SERVER;
            }
            set
            {
                VERSIONOFSECONDARYVSPHERE_SERVER = value;
            }
        }

        public int GetEsxInfoForResize(string inIP)
        {

            // this will the source info and target info with the credentials given by the user. 
            // for source we will get sourcexml.xml and for target we will get masterxml.xml....
            // once we will get the file we will read though the ReadEsxConfigFile methid.
            Trace.WriteLine(DateTime.Now + " \t Entered: GetEsxInfo of ESX.cs With ip " + inIP  );

            string cmdOptions = null, xmlFile = null;
            string esxCommand = null;
            int scriptReturnCode = 0;

            // Debug.WriteLine("Entered: GetEsxInfo");
            cmdOptions = ESX_SCRIPT;

           


            // if (inEsxRole == role.PRIMARY)
            {

               
                esxCommand = string.Format(SOURCE_ESX_COMMAND_FOR_RESIZE, inIP);
                // SOURCE_ESX_COMMAND = string.Format(SOURCE_ESX_COMMAND, inIp, inUserName, inPassWord);
                cmdOptions = cmdOptions + "  " + esxCommand;

                xmlFile = SOURCE_XML_FILE;
            }
            /*if (inEsxRole == role.SECONDARY)
            {
               // Debug.WriteLine("once you enter into to method:");
               // Debug.WriteLine(inIp +inUserName +inPassWord);
                esxCommand = string.Format(SOURCE_ESX_COMMAND, inIp, "\"" + inUserName + "\"", "\"" + inPassWord + "\"", osType.ToString());
                cmdOptions = cmdOptions + "  " + esxCommand ;
                //Debug.WriteLine("this is here command options: " +cmdOptions);
                xmlFile = TARGET_XML_FILE;
            }*/



            //If File already exists move it to .bac
            if (File.Exists(xmlFile))
            {
                if (File.Exists(xmlFile + ".bac"))
                {
                    File.Delete(xmlFile + ".bac");
                }
                try
                {
                    File.Move(xmlFile, xmlFile + ".bac");
                }
                catch (IOException e)
                {
                    System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);

                    Trace.WriteLine("ERROR*******************************************");
                    Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                    Trace.WriteLine("Exception  =  " + e.Message);
                    Trace.WriteLine("ERROR___________________________________________");

                    Trace.WriteLine(DateTime.Now + " Couldn't move file " + xmlFile + " to " + xmlFile + ".bac");
                }

            }



            string command = Perlbinary;
            //string  options = _installPath + "\\esx.pl " + "  --server " + inIp + "  --username " + inUserName + "  --password  " + inPassword + " --ostype Windows";


            // Debug.WriteLineIf(_esxTrcSwitch.Level == TraceLevel.Verbose, "Command to execute = " + command + cmdOptions);


            // Debug.WriteLine("Command to execute = " + command + cmdOptions);

            scriptReturnCode = WinTools.Execute(command, cmdOptions, 6000000);

            //Reads the hosts generated by the RCLI script in to a globla host list _hostList
            //ReadEsxConfigFile(xmlFile, role.PRIMARY, inIp, inUserName, inPassWord);

            Trace.WriteLine(DateTime.Now + " \t Exiting: GetEsxInfo of ESX.cs With ip " + inIP);

            return scriptReturnCode;

        }

        public int GetEsxInfo(string inIP, Role inEsxRole, OStype osType)
        {

            // this will the source info and target info with the credentials given by the user. 
            // for source we will get sourcexml.xml and for target we will get masterxml.xml....
            // once we will get the file we will read though the ReadEsxConfigFile methid.
            Trace.WriteLine(DateTime.Now + " \t Entered: GetEsxInfo of ESX.cs With ip " + inIP + " role as " + inEsxRole + " and ostype as " + osType);

            string cmdOptions = null, xmlFile = null;
            string esxCommand = null;
            int scriptReturnCode = 0;

            // Debug.WriteLine("Entered: GetEsxInfo");
            cmdOptions = ESX_SCRIPT;





            // if (inEsxRole == role.PRIMARY)
            {

                
                esxCommand = string.Format(SOURCE_ESX_COMMAND, inIP, osType.ToString());
                // SOURCE_ESX_COMMAND = string.Format(SOURCE_ESX_COMMAND, inIp, inUserName, inPassWord);
                cmdOptions = cmdOptions + "  " + esxCommand;

                xmlFile = SOURCE_XML_FILE;
            }
            /*if (inEsxRole == role.SECONDARY)
            {
               // Debug.WriteLine("once you enter into to method:");
               // Debug.WriteLine(inIp +inUserName +inPassWord);
                esxCommand = string.Format(SOURCE_ESX_COMMAND, inIp, "\"" + inUserName + "\"", "\"" + inPassWord + "\"", osType.ToString());
                cmdOptions = cmdOptions + "  " + esxCommand ;
                //Debug.WriteLine("this is here command options: " +cmdOptions);
                xmlFile = TARGET_XML_FILE;
            }*/



            //If File already exists move it to .bac
            if (File.Exists(xmlFile))
            {
                if (File.Exists(xmlFile + ".bac"))
                {
                    File.Delete(xmlFile + ".bac");
                }
                try
                {
                    File.Move(xmlFile, xmlFile + ".bac");
                }
                catch (IOException e)
                {
                    System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);

                    Trace.WriteLine("ERROR*******************************************");
                    Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                    Trace.WriteLine("Exception  =  " + e.Message);
                    Trace.WriteLine("ERROR___________________________________________");

                    Trace.WriteLine(DateTime.Now + " Couldn't move file " + xmlFile + " to " + xmlFile + ".bac");
                }

            }



            string command = Perlbinary;
            //string  options = _installPath + "\\esx.pl " + "  --server " + inIp + "  --username " + inUserName + "  --password  " + inPassword + " --ostype Windows";


            // Debug.WriteLineIf(_esxTrcSwitch.Level == TraceLevel.Verbose, "Command to execute = " + command + cmdOptions);


            // Debug.WriteLine("Command to execute = " + command + cmdOptions);

            scriptReturnCode = WinTools.Execute(command, cmdOptions, 6000000);

            //Reads the hosts generated by the RCLI script in to a globla host list _hostList
            ReadEsxConfigFile(xmlFile, inEsxRole, inIP);

            Trace.WriteLine(DateTime.Now + " \t Exiting: GetEsxInfo of ESX.cs With ip " + inIP + " role as " + inEsxRole + " and ostype as " + osType);

            return scriptReturnCode;

        }
        public int GetEsxInfoWithVmdks(string inIP, Role inEsxRole, OStype osType)
        {

            // this will be used at the time of recovery to get masterxml.xml because we need to check whenther the mt or source vm got vmotioned
            // we will check throung the vmdk names. now we are supporting hthe change of display names so thats whe we 
            // are checking for the vmdk names.
            Trace.WriteLine(DateTime.Now + " \t Entered: GetEsxInfo of ESX.cs With ip " + inIP +  " role as " + inEsxRole + " and ostype as " + osType);

            string cmdOptions = null, xmlFile = null;
            string esxCommand = null;
            int scriptReturnCode = 0;

            // Debug.WriteLine("Entered: GetEsxInfo");
            cmdOptions = ESX_SCRIPT;

          


            if (inEsxRole == Role.Secondary)
            {
                // Debug.WriteLine("once you enter into to method:");
                // Debug.WriteLine(inIp +inUserName +inPassWord);
               
                esxCommand = string.Format(SOURCE_ESX_COMMAND, inIP, osType.ToString());
                cmdOptions = cmdOptions + "  " + esxCommand;
                //Debug.WriteLine("this is here command options: " +cmdOptions);
                xmlFile = TARGET_XML_FILE;
            }



            //If File already exists move it to .bac
            if (File.Exists(xmlFile))
            {
                if (File.Exists(xmlFile + ".bac"))
                {
                    File.Delete(xmlFile + ".bac");
                }
                try
                {
                    File.Move(xmlFile, xmlFile + ".bac");
                }
                catch (IOException e)
                {
                    System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);

                    Trace.WriteLine("ERROR*******************************************");
                    Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                    Trace.WriteLine("Exception  =  " + e.Message);
                    Trace.WriteLine("ERROR___________________________________________");

                    Trace.WriteLine(DateTime.Now + " Couldn't move file " + xmlFile + " to " + xmlFile + ".bac");
                }

            }



            string command = Perlbinary;
            //string  options = _installPath + "\\esx.pl " + "  --server " + inIp + "  --username " + inUserName + "  --password  " + inPassword + " --ostype Windows";


            // Debug.WriteLineIf(_esxTrcSwitch.Level == TraceLevel.Verbose, "Command to execute = " + command + cmdOptions);


            // Debug.WriteLine("Command to execute = " + command + cmdOptions);

            scriptReturnCode = WinTools.Execute(command, cmdOptions, 6000000);

            //Reads the hosts generated by the RCLI script in to a globla host list _hostList
            ReadEsxConfigFile(xmlFile, inEsxRole, inIP);

            Trace.WriteLine(DateTime.Now + " \t Exiting: GetEsxInfo of ESX.cs With ip " + inIP + " role as " + inEsxRole + " and ostype as " + osType);

            return scriptReturnCode;
        }


        public int ValidateEsxCredentails(string inIP)
        {

            // this will be used at the time of recovery to get masterxml.xml because we need to check whenther the mt or source vm got vmotioned
            // we will check throung the vmdk names. now we are supporting hthe change of display names so thats whe we 
            // are checking for the vmdk names.
            Trace.WriteLine(DateTime.Now + " \t Entered: GetEsxInfo of ESX.cs With ip " + inIP  );

            string cmdOptions = null, xmlFile = null;
            string esxCommand = null;
            int scriptReturnCode = 0;

            // Debug.WriteLine("Entered: GetEsxInfo");
            cmdOptions = ESX_SCRIPT;

            



           
               
                esxCommand = string.Format(VLIDATE_ESX_COMMAND, inIP);
                cmdOptions = cmdOptions + "  " + esxCommand;
                
                
           



           



            string command = Perlbinary;
           

            scriptReturnCode = WinTools.Execute(command, cmdOptions, 6000000);

            //Reads the hosts generated by the RCLI script in to a globla host list _hostList


            Trace.WriteLine(DateTime.Now + " \t Exiting: GetEsxInfo of ESX.cs With ip " + inIP);
            return scriptReturnCode;
        }


        public int GetEsxInfoWithVmdksForAllOS(string inIP, Role inEsxRole)
        {

            // this will be used at the time of recovery to get masterxml.xml because we need to check whenther the mt or source vm got vmotioned
            // we will check throung the vmdk names. now we are supporting hthe change of display names so thats whe we 
            // are checking for the vmdk names.
            Trace.WriteLine(DateTime.Now + " \t Entered: GetEsxInfo of ESX.cs With ip " + inIP + " role as " + inEsxRole + " and ostype as all" );

            string cmdOptions = null, xmlFile = null;
            string esxCommand = null;
            int scriptReturnCode = 0;

            // Debug.WriteLine("Entered: GetEsxInfo");
            cmdOptions = ESX_SCRIPT;

          



            if (inEsxRole == Role.Secondary)
            {
                // Debug.WriteLine("once you enter into to method:");
                // Debug.WriteLine(inIp +inUserName +inPassWord);
               
                esxCommand = string.Format(SOURCE_ESX_COMMAND, inIP,  "all");
                cmdOptions = cmdOptions + "  " + esxCommand;
                //Debug.WriteLine("this is here command options: " +cmdOptions);
                xmlFile = TARGET_XML_FILE;
            }



            //If File already exists move it to .bac
            if (File.Exists(xmlFile))
            {
                if (File.Exists(xmlFile + ".bac"))
                {
                    File.Delete(xmlFile + ".bac");
                }
                try
                {
                    File.Move(xmlFile, xmlFile + ".bac");
                }
                catch (IOException e)
                {
                    System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);

                    Trace.WriteLine("ERROR*******************************************");
                    Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                    Trace.WriteLine("Exception  =  " + e.Message);
                    Trace.WriteLine("ERROR___________________________________________");

                    Trace.WriteLine(DateTime.Now + " Couldn't move file " + xmlFile + " to " + xmlFile + ".bac");
                }

            }



            string command = Perlbinary;
            //string  options = _installPath + "\\esx.pl " + "  --server " + inIp + "  --username " + inUserName + "  --password  " + inPassword + " --ostype Windows";


            // Debug.WriteLineIf(_esxTrcSwitch.Level == TraceLevel.Verbose, "Command to execute = " + command + cmdOptions);


            // Debug.WriteLine("Command to execute = " + command + cmdOptions);

            scriptReturnCode = WinTools.Execute(command, cmdOptions, 6000000);

            //Reads the hosts generated by the RCLI script in to a globla host list _hostList
            ReadEsxConfigFile(xmlFile, inEsxRole, inIP);

            Trace.WriteLine(DateTime.Now + " \t Exiting: GetEsxInfo of ESX.cs With ip " + inIP +  " role as " + inEsxRole + " and ostype as ");

            return scriptReturnCode;
        }


        public int GetEsxInfoWithVmdksWithOptions(string inIP, Role inEsxRole, OStype osType, string hosts)
        {

            // this will be used at the time of recovery to get masterxml.xml because we need to check whenther the mt or source vm got vmotioned
            // we will check throung the vmdk names. now we are supporting hthe change of display names so thats whe we 
            // are checking for the vmdk names.
            Trace.WriteLine(DateTime.Now + " \t Entered: GetEsxInfo of ESX.cs With ip " + inIP + " role as " + inEsxRole + " and ostype as " + osType);

            string cmdOptions = null, xmlFile = null;
            string esxCommand = null;
            int scriptReturnCode = 0;

            // Debug.WriteLine("Entered: GetEsxInfo");
            cmdOptions = ESX_SCRIPT;

           



            if (inEsxRole == Role.Secondary)
            {
                // Debug.WriteLine("once you enter into to method:");
                // Debug.WriteLine(inIp +inUserName +inPassWord);
               
                esxCommand = string.Format(SOURCE_ESX_COMMAND_FOR_ONLY_HOSTS, inIP, osType.ToString(), "\"" + hosts + "\"");
                cmdOptions = cmdOptions + "  " + esxCommand;
                //Debug.WriteLine("this is here command options: " +cmdOptions);
                xmlFile = TARGET_XML_FILE;
            }



            //If File already exists move it to .bac
            if (File.Exists(xmlFile))
            {
                if (File.Exists(xmlFile + ".bac"))
                {
                    File.Delete(xmlFile + ".bac");
                }
                try
                {
                    File.Move(xmlFile, xmlFile + ".bac");
                }
                catch (IOException e)
                {
                    System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);

                    Trace.WriteLine("ERROR*******************************************");
                    Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                    Trace.WriteLine("Exception  =  " + e.Message);
                    Trace.WriteLine("ERROR___________________________________________");

                    Trace.WriteLine(DateTime.Now + " Couldn't move file " + xmlFile + " to " + xmlFile + ".bac");
                }

            }



            string command = Perlbinary;
            //string  options = _installPath + "\\esx.pl " + "  --server " + inIp + "  --username " + inUserName + "  --password  " + inPassword + " --ostype Windows";


            // Debug.WriteLineIf(_esxTrcSwitch.Level == TraceLevel.Verbose, "Command to execute = " + command + cmdOptions);


            // Debug.WriteLine("Command to execute = " + command + cmdOptions);

            scriptReturnCode = WinTools.Execute(command, cmdOptions, 6000000);

            //Reads the hosts generated by the RCLI script in to a globla host list _hostList
            if (hosts == "networks")
            {
                ReadNetworkNodesFromXmlFile(xmlFile);
            }
            else
            {
                ReadEsxConfigFile(xmlFile, inEsxRole, inIP);
            }

            Trace.WriteLine(DateTime.Now + " \t Exiting: GetEsxInfo of ESX.cs With ip " + inIP +  " role as " + inEsxRole + " and ostype as " + osType);

            return scriptReturnCode;
        }
        public int GetEsxInfoWithVmdksWithOptionsWithoutOS(string inIP, Role inEsxRole, string hosts)
        {

            // this will be used at the time of recovery to get masterxml.xml because we need to check whenther the mt or source vm got vmotioned
            // we will check throung the vmdk names. now we are supporting hthe change of display names so thats whe we 
            // are checking for the vmdk names.
            Trace.WriteLine(DateTime.Now + " \t Entered: GetEsxInfo of ESX.cs With ip " + inIP +  " role as " + inEsxRole + " and ostype as all" );

            string cmdOptions = null, xmlFile = null;
            string esxCommand = null;
            int scriptReturnCode = 0;

            // Debug.WriteLine("Entered: GetEsxInfo");
            cmdOptions = ESX_SCRIPT;

           




            if (inEsxRole == Role.Secondary)
            {
                // Debug.WriteLine("once you enter into to method:");
                // Debug.WriteLine(inIp +inUserName +inPassWord);
                
                esxCommand = string.Format(SOURCE_ESX_COMMAND_FOR_ONLY_HOSTS, inIP, "all", "\"" + hosts + "\"");
                cmdOptions = cmdOptions + "  " + esxCommand;
                //Debug.WriteLine("this is here command options: " +cmdOptions);
                xmlFile = TARGET_XML_FILE;
            }



            //If File already exists move it to .bac
            if (File.Exists(xmlFile))
            {
                if (File.Exists(xmlFile + ".bac"))
                {
                    File.Delete(xmlFile + ".bac");
                }
                try
                {
                    File.Move(xmlFile, xmlFile + ".bac");
                }
                catch (IOException e)
                {
                    System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);

                    Trace.WriteLine("ERROR*******************************************");
                    Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                    Trace.WriteLine("Exception  =  " + e.Message);
                    Trace.WriteLine("ERROR___________________________________________");

                    Trace.WriteLine(DateTime.Now + " Couldn't move file " + xmlFile + " to " + xmlFile + ".bac");
                }

            }



            string command = Perlbinary;
            //string  options = _installPath + "\\esx.pl " + "  --server " + inIp + "  --username " + inUserName + "  --password  " + inPassword + " --ostype Windows";


            // Debug.WriteLineIf(_esxTrcSwitch.Level == TraceLevel.Verbose, "Command to execute = " + command + cmdOptions);


            // Debug.WriteLine("Command to execute = " + command + cmdOptions);

            scriptReturnCode = WinTools.Execute(command, cmdOptions, 6000000);

            //Reads the hosts generated by the RCLI script in to a globla host list _hostList
            if (hosts == "networks")
            {
                ReadNetworkNodesFromXmlFile(xmlFile);
            }
            else
            {
                ReadEsxConfigFile(xmlFile, inEsxRole, inIP);
            }

            Trace.WriteLine(DateTime.Now + " \t Exiting: GetEsxInfo of ESX.cs With ip " + inIP + " role as " + inEsxRole + " and ostype as all" );

            return scriptReturnCode;
        }
        public int GetOnlyLuns(string inIP, string luns)
        {

            // this will be used at the time of recovery to get masterxml.xml because we need to check whenther the mt or source vm got vmotioned
            // we will check throung the vmdk names. now we are supporting hthe change of display names so thats whe we 
            // are checking for the vmdk names.
            Trace.WriteLine(DateTime.Now + " \t Entered: GetEsxInfo of ESX.cs With ip " + inIP  );

            string cmdOptions = null, xmlFile = null;
            string esxCommand = null;
            int scriptReturnCode = 0;

            // Debug.WriteLine("Entered: GetEsxInfo");
            cmdOptions = ESX_SCRIPT;

          



             // Debug.WriteLine("once you enter into to method:");
                // Debug.WriteLine(inIp +inUserName +inPassWord);
                
                esxCommand = string.Format(SOURCE_ESX_COMMAND_FOR_ONLY_LUNS, inIP,  "\"" + luns + "\"");
                cmdOptions = cmdOptions + "  " + esxCommand;
                //Debug.WriteLine("this is here command options: " +cmdOptions);
                xmlFile = TARGET_XML_FILE;
           



            //If File already exists move it to .bac
                if (File.Exists(xmlFile))
                {
                    if (File.Exists(xmlFile + ".bac"))
                    {
                        File.Delete(xmlFile + ".bac");
                    }
                    try
                    {
                        File.Move(xmlFile, xmlFile + ".bac");
                    }
                    catch (IOException e)
                    {
                        System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);

                        Trace.WriteLine("ERROR*******************************************");
                        Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                        Trace.WriteLine("Exception  =  " + e.Message);
                        Trace.WriteLine("ERROR___________________________________________");

                        Trace.WriteLine(DateTime.Now + " Couldn't move file " + xmlFile + " to " + xmlFile + ".bac");
                    }

                }



            string command = Perlbinary;
            //string  options = _installPath + "\\esx.pl " + "  --server " + inIp + "  --username " + inUserName + "  --password  " + inPassword + " --ostype Windows";


            // Debug.WriteLineIf(_esxTrcSwitch.Level == TraceLevel.Verbose, "Command to execute = " + command + cmdOptions);


            // Debug.WriteLine("Command to execute = " + command + cmdOptions);

            scriptReturnCode = WinTools.Execute(command, cmdOptions, 6000000);

            //Reads the hosts generated by the RCLI script in to a globla host list _hostList

            ReadLunNodesFromXmlFile(xmlFile);
            Trace.WriteLine(DateTime.Now + " \t Exiting: GetEsxInfo of ESX.cs With ip " + inIP );

            return scriptReturnCode;
        }

        public bool ReadNetworkNodesFromXmlFile(string xmlFile)
        {
            //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
            //based on above comment we have added xmlresolver as null
            XmlDocument document = new XmlDocument();
            document.XmlResolver = null;
            XmlNodeList networkNodes = null;
           

            string fileFullPath = filePath + "\\" + xmlFile;
           
            if (!File.Exists(fileFullPath))
            {

                Trace.WriteLine("01_002_001: The file " + fileFullPath + " could not be located" + "in dir " + filePath);

                Console.WriteLine("01_002_001: The file " + fileFullPath + " could not be located" + "in dir " + filePath);
                return false;

            }


            try
            {
                //StreamReader reader = new StreamReader(fileFullPath);
                //string xmlString = reader.ReadToEnd();
                //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
                XmlReaderSettings settings = new XmlReaderSettings();
                settings.ProhibitDtd = true;
                settings.XmlResolver = null;
                using (XmlReader reader1 = XmlReader.Create(fileFullPath, settings))
                {
                    document.Load(reader1);
                   // reader1.Close();
                }
                //reader.Close();
            }
            catch (Exception xmle)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(xmle, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + xmle.Message);
                Trace.WriteLine("ERROR___________________________________________");
                Trace.WriteLine(DateTime.Now + "\t " + "ESX.cs:ReadEsxConfigFile caught Exception :" + xmle.Message);
                return false;
            }
            networkNodes = document.GetElementsByTagName("network");
            networkList = new ArrayList();
            foreach (XmlNode node in networkNodes)
            {
                Trace.WriteLine(DateTime.Now + "\t Came here to read network nodes");
                Network network = ReadNetWorkNode(node);
                this.networkList.Add(network);
            }
            return true;
        }



        public bool ReadLunNodesFromXmlFile(string xmlFile)
        {
            //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
            //based on above comment we have added xmlresolver as null
            XmlDocument document = new XmlDocument();
            document.XmlResolver = null;
            XmlNodeList lunNodes = null;


            string fileFullPath = filePath + "\\" + xmlFile;
           
            if (!File.Exists(fileFullPath))
            {

                Trace.WriteLine("01_002_001: The file " + fileFullPath + " could not be located" + "in dir " + filePath);

                Console.WriteLine("01_002_001: The file " + fileFullPath + " could not be located" + "in dir " + filePath);
                return false;

            }


            try
            {
                //StreamReader reader = new StreamReader(fileFullPath);
                //string xmlString = reader.ReadToEnd();
                //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
                XmlReaderSettings settings = new XmlReaderSettings();
                settings.ProhibitDtd = true;
                settings.XmlResolver = null;
                using (XmlReader reader1 = XmlReader.Create(fileFullPath, settings))
                {
                    document.Load(reader1);
                   // reader1.Close();
                }
                //reader.Close();
            }
            catch (Exception xmle)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(xmle, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + xmle.Message);
                Trace.WriteLine("ERROR___________________________________________");
                Trace.WriteLine(DateTime.Now + "\t " + "ESX.cs:ReadEsxConfigFile caught Exception :" + xmle.Message);
                return false;
            }
            lunNodes = document.GetElementsByTagName("lun");
            lunList = new ArrayList();
            foreach (XmlNode node in lunNodes)
            {
                Trace.WriteLine(DateTime.Now + "\t Came here to read network nodes");
                RdmLuns luns = ReadLunNodes(node);
                this.lunList.Add(luns);
            }
            return true;
        }

        public int PowerOnVM(string inIP, string inDisplayName)
        {

            try
            {

                // this command will power on the vm. we need display name to power on the vm along with esx credentials.
                Trace.WriteLine(DateTime.Now + " \t Entered: PowerOnVM() of ESX.cs with IP " + inIP + " and diplayname as " + inDisplayName);

                string cmdOptions = null, xmlFile = null;
                string createTargetGuestsCommand = null;

                //Add quotest at the begining and end of the path so that spaces won't cause issue
                cmdOptions = Protectscript;

              
                createTargetGuestsCommand = string.Format(POWER_ON_COMMAND, inIP,  inDisplayName);
                cmdOptions = cmdOptions + "  " + createTargetGuestsCommand;






                string command = Perlbinary;

                // Debug.WriteLine("Executing below command command " + command + " " + cmdOptions);
                Trace.WriteLine(DateTime.Now + " \t Exiting: PowerOnVM() of ESX.cs with IP " + inIP + " and diplayname as " + inDisplayName);

                return WinTools.Execute(command, cmdOptions, 600000);
            }
            catch (Exception e)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + e.Message);
                Trace.WriteLine("ERROR___________________________________________");
                Trace.WriteLine(DateTime.Now + "Esx.cs: PowerOnVM Caught an exception " + e.Message);
                return -1;
            }


        }


        public static int PerlInstalled()
        {
            // this is a prereg check of vCon box whether perl is installed on not......
            Trace.WriteLine(DateTime.Now + " \t Entered: PerlInstalled to check perl is installed or not in rcli");
            string cmdOptions = null, xmlFile = null;

            return WinTools.Execute("cmd.exe", "/C " + " perl -MVMware::VIRuntime -e \"hello\"", 600000);
        }

        public int GetRcliVersion()
        {
            try
            {
                // we will get the vcli version in the vCOn box using this call.
                string command = Perlbinary;
                string cmdOptions = ESX_SCRIPT + " --version";
                Trace.WriteLine(DateTime.Now + " \t Entered: GetRcliVersion to check  rcli version.");
                int exitCode = WinTools.Execute(command, cmdOptions, 600000);

                return exitCode;
            }
            catch (Exception e)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + e.Message);
                Trace.WriteLine("ERROR___________________________________________");
                Trace.WriteLine(DateTime.Now + "Esx.cs: GetRcliVersion Caught an exception " + e.Message);
                return -1;
            }
        }


        public int TargetScriptForOffliceSyncImport(string inIP)
        {
            // This is the call for the offline sync importwe need tagetdatastore anf vmdk store which is selected by the user.
            Trace.WriteLine(DateTime.Now + " \t Entered: TargetScriptForOffliceSyncImport with IP: " + inIP +  " vSphereHost ");
            string cmdOptions = null, xmlFile = null;
            string createTargetGuestsCommand = null;

            //Add quotest at the begining and end of the path so that spaces won't cause issue
            cmdOptions = Offlinesyncscript;

            
            createTargetGuestsCommand = string.Format(TARGET_SCRIPT_COMMAND_FOR_OFFLINE_SYNC, inIP);


            cmdOptions = cmdOptions + "  " + createTargetGuestsCommand;






            string command = Perlbinary;

            // Debug.WriteLine("Executing below command  " + command + " " + cmdOptions);
            Trace.WriteLine(DateTime.Now + " \t Exiting: TargetScriptForOffliceSyncImport with IP: " + inIP );
            return WinTools.Execute(command, cmdOptions, Perltimeout);

        }

        public int DetachDisksFromMasterTarget(string inIP)
        {
            // This is the call which is used at the time of remove operation..

            Trace.WriteLine(DateTime.Now + " \t Entered: DetachDisksFromMasterTarget() with IP: " + inIP );
            string cmdOptions = null, xmlFile = null;
            string createTargetGuestsCommand = null;

            //Add quotest at the begining and end of the path so that spaces won't cause issue
            cmdOptions = RECOVERY_SCRIPT_FOR_WMI;

           
            createTargetGuestsCommand = string.Format(DETACH_DISKS_FROM_MASTER_TARGET, inIP, "\"" + filePath + "\"");


            cmdOptions = cmdOptions + "  " + createTargetGuestsCommand;





            Trace.WriteLine(DateTime.Now + " \t Exiting: DetachDisksFromMasterTarget() with IP: " + inIP );
            string command = Perlbinary;

            // Debug.WriteLine("Executing below command  " + command + " " + cmdOptions);

            return WinTools.Execute(command, cmdOptions, Perltimeout);


        }
        public int PowerOffVM(string inIP, string inDisplayName)
        {
            // To power of vm we will use this call.
            // we can power off multiple vms at a time...

            try
            {
                Trace.WriteLine(DateTime.Now + " \t Entered: PowerOffVM() with IP: " + inIP +" diaplayname of vm is  " + inDisplayName);
                string cmdOptions = null, xmlFile = null;
                string createTargetGuestsCommand = null;

                //Add quotest at the begining and end of the path so that spaces won't cause issue
                cmdOptions = Protectscript;

                
                createTargetGuestsCommand = string.Format(POWERED_OFF_COMMAND, inIP, inDisplayName);
                cmdOptions = cmdOptions + "  " + createTargetGuestsCommand;




                string command = Perlbinary;

                // Debug.WriteLine("Executing below command command " + command + " " + cmdOptions);
                Trace.WriteLine(DateTime.Now + " \t Exiting: PowerOffVM() with IP: " + inIP +  " diaplayname of vm is  " + inDisplayName);
                return WinTools.Execute(command, cmdOptions, 600000);
            }
            catch (Exception e)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + e.Message);
                Trace.WriteLine("ERROR___________________________________________");
                Trace.WriteLine(DateTime.Now + "Esx.cs: PowerOff Caught an exception " + e.Message);
                return 1;
            }



        }
        public int GenerateVmxFile(string inIP)
        {

            // In case of v2v scripts will generate the vmx file... using esx.xml prepared by ui.
            Trace.WriteLine(DateTime.Now + " \t Entered: GenerateVmxFile() with IP: " + inIP );
            string cmdOptions = null, xmlFile = null;
            string generateVmxFile = null;


            cmdOptions = ESX_SCRIPT;
            
            generateVmxFile = string.Format(GENERATE_VMX_FILE, inIP);
            cmdOptions = cmdOptions + "  " + generateVmxFile;
            string command = Perlbinary;

            // Debug.WriteLine("Executing below command command " + command + " " + cmdOptions);
            Trace.WriteLine(DateTime.Now + " \t Exiting: GenerateVmxFile() with IP: " + inIP );
            return WinTools.Execute(command, cmdOptions, 60000000);


        }
        public int GenerateVmxFileForResume(string inIP)
        {
            //at the time of resume it is different call to prepare the vmx file....
            Trace.WriteLine(DateTime.Now + " \t Entered: GenerateVmxFile() with IP: " + inIP +  " For resume");
            string cmdOptions = null, xmlFile = null;
            string generateVmxFile = null;


            cmdOptions = ESX_SCRIPT;
            
            generateVmxFile = string.Format(GENERATE_VMX_FILE_FOR_RESUME, inIP );
            cmdOptions = cmdOptions + "  " + generateVmxFile;
            string command = Perlbinary;

            // Debug.WriteLine("Executing below command command " + command + " " + cmdOptions);
            Trace.WriteLine(DateTime.Now + " \t Exiting: GenerateVmxFile() with IP: " + inIP +  " For resume");
            return WinTools.Execute(command, cmdOptions, 60000000);


        }
        public int GenerateVmxFileForResumeTarget(string inIP)
        {


            // this is a type of target pre req check at the time of resume...
            Trace.WriteLine(DateTime.Now + " \t Entered: GenerateVmxFileForResumeTarget() with IP: " + inIP );
            string cmdOptions = null, xmlFile = null;
            string generateVmxFile = null;


            cmdOptions = ESX_SCRIPT;
           
            generateVmxFile = string.Format(GENERATE_VMX_FILE_FOR_RESUME_TARGET, inIP);
            cmdOptions = cmdOptions + "  " + generateVmxFile;
            string command = Perlbinary;

            // Debug.WriteLine("Executing below command command " + command + " " + cmdOptions);
            Trace.WriteLine(DateTime.Now + " \t Exiting: GenerateVmxFileForResumeTarget() with IP: " + inIP );
            return WinTools.Execute(command, cmdOptions, 60000000);


        }
        /*
        public Process ExecuteCreateTargetGuestsScript(string inIp, string inUserName, string inPassWord,  string incremenatal)
        {
            Trace.WriteLine(DateTime.Now + " \t Entered: ExecuteCreateTargetGuestsScript() with IP: " + inIp + " username " + inUserName + " incremental or normal " + incremenatal);
            string cmdOptions = null, xmlFile = null;
            string createTargetGuestsCommand = null;

            //Add quotest at the begining and end of the path so that spaces won't cause issue
            cmdOptions = "\"" + _installPath + "\"" + "\\" + CREATE_GUEST_ON_TARGET_SCRIPT;
            if (incremenatal == "no")
            {

                createTargetGuestsCommand = string.Format(CREATE_TARGET_GUESTS_COMMAND, inIp, inUserName, inPassWord);
            }
            else if (incremenatal == "yes")
            {
                createTargetGuestsCommand = string.Format(CREATE_GUEST_FOR_INCREMENTAL_DISK, inIp, inUserName, inPassWord);
            }

            cmdOptions = cmdOptions + "  " + createTargetGuestsCommand;




            Trace.WriteLine(DateTime.Now + " \t Exiting: ExecuteCreateTargetGuestsScript() with IP: " + inIp + " username " + inUserName + " incremental or normal " + incremenatal);

            string command = PERL_BINARY;

           // Debug.WriteLine("Executing below command  " + command + " " + cmdOptions);

            return WinTools.ExecuteNoWait(command, cmdOptions);
            

            
        }
        */
        public int ExecuteCreateTargetGuestScript(string inIP, OperationType operation)
        {

            // To create vms on target side this will be called for all operatons...
            Trace.WriteLine(DateTime.Now + " \t Entered: ExecuteCreateTargetGuestScript with IP: " + inIP );
            string option = "";

            string cmdOptions = null, xmlFile = null;
            string createTargetGuestsCommand = null;

            switch (operation)
            {
                case OperationType.Initialprotection:
                    cmdOptions = Protectscript;
                    option = Createtargetguestscommand;
                    break;
                case OperationType.Additionofdisk:
                    cmdOptions = Protectscript;
                    option = Createguestforincrementaldisk;
                    break;
                case OperationType.Failback:
                    cmdOptions = Failbackresumescript;
                    option = Createguestforfailback;
                    break;
                case OperationType.Offlinesync:
                    cmdOptions = Offlinesyncscript;
                    option = Createguestontargetforofflinesynccommand;
                    break;
                case OperationType.Resume:
                    cmdOptions = Failbackresumescript;
                    option = Createguestforresume;
                    break;
                case OperationType.Drdrill:
                    cmdOptions = Protectscript;
                    option = Createguestfordrdrill;
                    break;
                case OperationType.Resize:
                    cmdOptions = Protectscript;
                    option = Createguestforresize;
                    break;
            }






            //Add quotest at the begining and end of the path so that spaces won't cause issue
           
            createTargetGuestsCommand = string.Format(option, inIP);
            cmdOptions = cmdOptions + "  " + createTargetGuestsCommand;


            string command = Perlbinary;

            // Debug.WriteLine("Executing below command  " + command + " " + cmdOptions);
            Trace.WriteLine(DateTime.Now + " \t Exiting: ExecuteCreateTargetGuestScript() with IP: " + inIP );
            return WinTools.Execute(command, cmdOptions, Perltimeout);

        }
        /*
                public Process ExecuteCreateTargetGuestScriptForOfflineSync(string inIp, string inUserName, string inPassWord)
                {
                    Trace.WriteLine(DateTime.Now + " \t Entered: ExecuteCreateTargetGuestScriptForOfflineSync() with IP: " + inIp + " username " + inUserName);
                    string cmdOptions = null, xmlFile = null;
                    string createTargetGuestsCommand = null;

                    //Add quotest at the begining and end of the path so that spaces won't cause issue
                    cmdOptions = "\"" + _installPath + "\"" + "\\" + CREATE_GUEST_ON_TARGET_SCRIPT;
                    createTargetGuestsCommand = string.Format(CREATE_GUEST_ON_TARGET_FOR_OFFLINE_SYNC_COMMAND, inIp, inUserName, inPassWord);
                    cmdOptions = cmdOptions + "  " + createTargetGuestsCommand;






                    string command = PERL_BINARY;

                  //  Debug.WriteLine("Executing below command  " + command + " " + cmdOptions);
                    Trace.WriteLine(DateTime.Now + " \t Exiting: ExecuteCreateTargetGuestScriptForOfflineSync() with IP: " + inIp + " username " + inUserName);
                    return WinTools.ExecuteNoWait(command, cmdOptions);
            

                }

                public Process ExecuteCreateGuestForFailBack(string inIp, string inUserName, string inPassWord)
                {

                    Trace.WriteLine(DateTime.Now + " \t Entered: ExecuteCreateGuestForFailBack() with IP: " + inIp + " username " + inUserName);
                    string cmdOptions = null, xmlFile = null;
                    string createTargetGuestsCommand = null;

                    //Add quotest at the begining and end of the path so that spaces won't cause issue
                    cmdOptions = "\"" + _installPath + "\"" + "\\" + CREATE_GUEST_ON_TARGET_SCRIPT;

                    createTargetGuestsCommand = string.Format(CREATE_GUEST_FOR_FAIL_BACK, inIp, inUserName, inPassWord);
                    cmdOptions = cmdOptions + "  " + createTargetGuestsCommand;






                    string command = PERL_BINARY;

                  //  Debug.WriteLine("Executing below command  " + command + " " + cmdOptions);
                    Trace.WriteLine(DateTime.Now + " \t Exiting: ExecuteCreateGuestForFailBack() with IP: " + inIp + " username " + inUserName);
                    return WinTools.ExecuteNoWait(command, cmdOptions);
            

                }
                */
        public int ExecuteCreateTargetGuestScriptForArrayDrill(string inIP)
        {

            // To create vms on target side this will be called for all operatons...
            Trace.WriteLine(DateTime.Now + " \t Entered: ExecuteCreateTargetGuestScript  for Array Dr drill with IP: " + inIP );
            string option = "";

            string cmdOptions = null, xmlFile = null;
            string createTargetGuestsCommand = null;


            cmdOptions = Protectscript;
            option = CREATE_GUEST_FOR_DRDRILL_ARRAYBASED;







            //Add quotest at the begining and end of the path so that spaces won't cause issue
           
            createTargetGuestsCommand = string.Format(option, inIP);
            cmdOptions = cmdOptions + "  " + createTargetGuestsCommand;


            string command = Perlbinary;

            // Debug.WriteLine("Executing below command  " + command + " " + cmdOptions);
            Trace.WriteLine(DateTime.Now + " \t Exiting: ExecuteCreateTargetGuestScript() with IP: " + inIP );
            return WinTools.Execute(command, cmdOptions, Perltimeout);

        }
        public int ExecuteDummyDisksScript(string inIP)
        {
            try
            {
                // This is to create dummy disk to the mt before creating vms on target side
                Trace.WriteLine(DateTime.Now + " \t Entered: To create dummy disks with IP: " + inIP );
                string option = "";

                string cmdOptions = null, xmlFile = null;
                string createTargetGuestsCommand = null;


                cmdOptions = CREATE_DUMMY_DISK_SCRIPT;
                
                createTargetGuestsCommand = string.Format(DUMMY_DISKS_FOR_WINDOWS_PROTECTION, inIP);
                cmdOptions = cmdOptions + "  " + createTargetGuestsCommand;


                string command = Perlbinary;

                // Debug.WriteLine("Executing below command  " + command + " " + cmdOptions);
                Trace.WriteLine(DateTime.Now + " \t Exiting: To create dummy disks with IP: " + inIP );
                return WinTools.Execute(command, cmdOptions, Perltimeout);

            }
            catch (Exception e)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: Dummy disks " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + e.Message);
                Trace.WriteLine("ERROR___________________________________________");
                Trace.WriteLine(DateTime.Now + "Esx.cs: PowerOff Caught an exception " + e.Message);
                return 1;
            }

        }

        public int ExecuteDummyDisksScriptForDrdrill(string inIP)
        {
            try
            {
                // This is to create dummy disk to the mt before creating vms on target side
                Trace.WriteLine(DateTime.Now + " \t Entered: To create dummy disks with IP: " + inIP );
                string option = "";

                string cmdOptions = null, xmlFile = null;
                string createTargetGuestsCommand = null;


                cmdOptions = CREATE_DUMMY_DISK_SCRIPT;
                
                createTargetGuestsCommand = string.Format(DUMMY_DISKS_FOR_WINDOWS_PROTECTION_FOR_DR_DRILL, inIP);
                cmdOptions = cmdOptions + "  " + createTargetGuestsCommand;


                string command = Perlbinary;

                // Debug.WriteLine("Executing below command  " + command + " " + cmdOptions);
                Trace.WriteLine(DateTime.Now + " \t Exiting: To create dummy disks with IP: " + inIP );
                return WinTools.Execute(command, cmdOptions, Perltimeout);

            }
            catch (Exception e)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: Dummy disks " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + e.Message);
                Trace.WriteLine("ERROR___________________________________________");
                Trace.WriteLine(DateTime.Now + "Esx.cs: PowerOff Caught an exception " + e.Message);
                return 1;
            }

        }
        public Process PreReqCheck(string inIP, string isFailBack)
        {
            try
            {
                // this is pre reg check for that the source machine are laready there or not....
                // this is different in case of the failback....
                Trace.WriteLine(DateTime.Now + " \t Entered: PreReqCheck() with IP: " + inIP + " isfailback or not " + isFailBack);

                string cmdOptions = null, xmlFile = null;
                string preReqChecksCommand = null;

                cmdOptions = READINESS_CHECK_SCRIPT;
                
                if (isFailBack == "no")
                {
                    Trace.WriteLine("Came here to run preres for normal protection");
                    preReqChecksCommand = string.Format(PRE_CHECKS_COMMAND, inIP);
                }
                else if (isFailBack == "yes")
                {
                    preReqChecksCommand = string.Format(PRE_CHECKS_COMMAND_FOR_FAILBACK, inIP);
                }
                cmdOptions = cmdOptions + "  " + preReqChecksCommand;

                string command = Perlbinary;

                //Debug.WriteLine("Executing below command command " + command + " " + cmdOptions);
                Trace.WriteLine(DateTime.Now + " \t Exiting: PreReqCheck() with IP: " + inIP +  " isfailback or not " + isFailBack);

                return WinTools.ExecuteNoWait(command, cmdOptions);
            }
            catch (Exception e)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + e.Message);
                Trace.WriteLine("ERROR___________________________________________");
                Trace.WriteLine(DateTime.Now + "Esx.cs: PreReqCheck Caught an exception " + e.Message);
                return null;
            }

        }

        public int PreReqCheckForResume(string inIP)
        {
            try
            {
                // thisis pre req check fo the resume......
                Trace.WriteLine(DateTime.Now + " \t Entered: PreReqCheckForResume() with IP: " + inIP + " resume");

                string cmdOptions = null, xmlFile = null;
                string preReqChecksCommand = null;

                cmdOptions = READINESS_CHECK_SCRIPT;
               
                preReqChecksCommand = string.Format(PRE_CHECKS_COMMAND_FOR_RESUME, inIP);

                cmdOptions = cmdOptions + "  " + preReqChecksCommand;

                string command = Perlbinary;

                //Debug.WriteLine("Executing below command command " + command + " " + cmdOptions);
                Trace.WriteLine(DateTime.Now + " \t Exiting: PreReqCheckForResume() with IP: " + inIP + " resume");

                return WinTools.Execute(command, cmdOptions, 600000);
            }
            catch (Exception e)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: PreReqCheckForResume " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + e.Message);
                Trace.WriteLine("ERROR___________________________________________");
                Trace.WriteLine(DateTime.Now + "Esx.cs: PreReqCheck Caught an exception " + e.Message);
                return 1;
            }

        }

        public int PreReqCheckForDRDrill(string inIP, string drdrilltype)
        {
            try
            {
                // thisis pre req check fo the resume......
                Trace.WriteLine(DateTime.Now + " \t Entered: PreReqCheckForDRDrill() with IP: " + inIP +  " resume");

                string cmdOptions = null, xmlFile = null;
                string preReqChecksCommand = null;

                cmdOptions = READINESS_CHECK_SCRIPT;

               
                if (drdrilltype == "dr_drill_array")
                {
                    Trace.WriteLine(DateTime.Now + "\t Readiness check for arraydrdrill ");
                    preReqChecksCommand = string.Format(PRE_CHECKS_COMMAND_FOR_DRDRILL_ARRAYBASED);
                }
                else
                {
                    preReqChecksCommand = string.Format(PRE_CHECKS_COMMAND_FOR_DRDRILL, inIP);

                }
                cmdOptions = cmdOptions + "  " + preReqChecksCommand;

                string command = Perlbinary;

                //Debug.WriteLine("Executing below command command " + command + " " + cmdOptions);
                Trace.WriteLine(DateTime.Now + " \t Exiting: PreReqCheckForDRDrill() with IP: " + inIP +  " resume");

                return WinTools.Execute(command, cmdOptions, 600000);
            }
            catch (Exception e)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: PreReqCheckForResume " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + e.Message);
                Trace.WriteLine("ERROR___________________________________________");
                Trace.WriteLine(DateTime.Now + "Esx.cs: PreReqCheck Caught an exception " + e.Message);
                return 1;
            }

        }

        public int PreReqCheckForAdditionOfDiskTarget(string inIP)
        {
            try
            {
                // thisis pre req check fo the resume......
                Trace.WriteLine(DateTime.Now + " \t Entered: PreReqCheckForAdditionOfDiskTarget() with IP: " + inIP );

                string cmdOptions = null, xmlFile = null;
                string preReqChecksCommand = null;

                cmdOptions = READINESS_CHECK_SCRIPT;
                
                preReqChecksCommand = string.Format(PRE_CHECKS_COMMAND_ADD_DISK_TARGET, inIP);

                cmdOptions = cmdOptions + "  " + preReqChecksCommand;

                string command = Perlbinary;

                //Debug.WriteLine("Executing below command command " + command + " " + cmdOptions);
                Trace.WriteLine(DateTime.Now + " \t Exiting: PreReqCheckForAdditionOfDiskTarget() with IP: " + inIP );

                return WinTools.Execute(command, cmdOptions, 600000);
            }
            catch (Exception e)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: PreReqCheckForResume " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + e.Message);
                Trace.WriteLine("ERROR___________________________________________");
                Trace.WriteLine(DateTime.Now + "Esx.cs: PreReqCheck Caught an exception " + e.Message);
                return 1;
            }

        }

        public Process PreReqCheckForOfflineSync(string inIP)
        {
            try
            {

                // There is a different call for the offline sync import....
                Trace.WriteLine(DateTime.Now + " \t Entered: PreReqCheck() with IP: " + inIP );

                string cmdOptions = null, xmlFile = null;
                string preReqChecksCommand = null;

                cmdOptions = READINESS_CHECK_SCRIPT;
                
                preReqChecksCommand = string.Format(PRE_CHECKS_COMMAND_FOR_OFFLINE_SYNC_IMPORT, inIP);

                cmdOptions = cmdOptions + "  " + preReqChecksCommand;

                string command = Perlbinary;

                //Debug.WriteLine("Executing below command command " + command + " " + cmdOptions);
                Trace.WriteLine(DateTime.Now + " \t Exiting: PreReqCheck() with IP: " + inIP );

                return WinTools.ExecuteNoWait(command, cmdOptions);
            }
            catch (Exception e)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + e.Message);
                Trace.WriteLine("ERROR___________________________________________");
                Trace.WriteLine(DateTime.Now + "Esx.cs: PreReqCheck Caught an exception " + e.Message);
                return null;
            }

        }

        public Process PreReqCheckForOfflineSyncExport(string inIP)
        {
            try
            {// There is a different call for the offline sync export....
                Trace.WriteLine(DateTime.Now + " \t Entered: PreReqCheckForOfflineSyncExport() with IP: " + inIP );

                string cmdOptions = null, xmlFile = null;
                string preReqChecksCommand = null;

                cmdOptions = READINESS_CHECK_SCRIPT;
                
                preReqChecksCommand = string.Format(PRE_CHECKS_COMMAND_FOR_OFFLINE_SYNC_EXP, inIP);

                cmdOptions = cmdOptions + "  " + preReqChecksCommand;


                string command = Perlbinary;

                //Debug.WriteLine("Executing below command command " + command + " " + cmdOptions);
                Trace.WriteLine(DateTime.Now + " \t Exiting: PreReqCheckForOfflineSyncExport() with IP: " + inIP );


                return WinTools.ExecuteNoWait(command, cmdOptions);

            }
            catch (Exception e)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + e.Message);
                Trace.WriteLine("ERROR___________________________________________");
                Trace.WriteLine(DateTime.Now + "Esx.cs: PreReqCheck Caught an exception " + e.Message);
                return null;
            }

        }





        public Process PreReqCheckForIncrementalDisk(string inIP)
        {
            Trace.WriteLine(DateTime.Now + " \t Entered: PreReqCheck() with IP: " + inIP);

            string cmdOptions = null, xmlFile = null;
            string preReqChecksCommand = null;

            cmdOptions = READINESS_CHECK_SCRIPT;

            
            preReqChecksCommand = string.Format(PRE_CHECKS_COMMAND_FOR_INCREMENTAL_DISK, inIP);

            cmdOptions = cmdOptions + "  " + preReqChecksCommand;

            string command = Perlbinary;

            //  Debug.WriteLine("Executing below command command " + command + " " + cmdOptions);
            Trace.WriteLine(DateTime.Now + " \t Exiting: PreReqCheck() with IP: " + inIP );

            return WinTools.ExecuteNoWait(command, cmdOptions);

        }

        public int PreReqCheckForResize(string inIP)
        {
            Trace.WriteLine(DateTime.Now + " \t Entered: PreReqCheck() with IP: " + inIP );

            string cmdOptions = null, xmlFile = null;
            string preReqChecksCommand = null;

            cmdOptions = READINESS_CHECK_SCRIPT;

            
            preReqChecksCommand = string.Format(PRE_CHECKS_COMMAND_RESIZE, inIP);

            cmdOptions = cmdOptions + "  " + preReqChecksCommand;

            string command = Perlbinary;

            //  Debug.WriteLine("Executing below command command " + command + " " + cmdOptions);
            Trace.WriteLine(DateTime.Now + " \t Exiting: PreReqCheck() with IP: " + inIP );

            return WinTools.Execute(command, cmdOptions, 600000000);

        }

        public Process DownLoadXmlFile(string sourceIP)
        {

            // this is a call for the esx.xml at the time of addition of disk....only for v2v
            Trace.WriteLine(DateTime.Now + " \t Entered: DownLoadXmlFile() with sourceESXIP: " + sourceIP );

            string cmdOptions = null, xmlFile = null;
            string downloadEsxCommand = null;

            
            cmdOptions = ESX_SCRIPT;
            downloadEsxCommand = string.Format(INCREMENTAL_DISK_SOURCE_CONNECTION, sourceIP);
            cmdOptions = cmdOptions + "  " + downloadEsxCommand;

            string command = Perlbinary;
            //Debug.WriteLine("Executing below command command " + command + " " + cmdOptions);
            Trace.WriteLine(DateTime.Now + " \t Exiting: DownLoadXmlFile() with sourceESXIP: " + sourceIP );

            return WinTools.ExecuteNoWait(command, cmdOptions);

        }



        public Process DownLoadXmlFileForP2v(string inTargetEsxIP)
        {// this is a call for the esx.xml at the time of addition of disk....only for p2v
            Trace.WriteLine(DateTime.Now + " \t  Entered: DownLoadXmlFileForP2v() of ESX.cs with targetESXIP " + inTargetEsxIP );

            string cmdOptions = null, xmlFile = null;
            string downloadEsxCommand = null;

            
            cmdOptions = ESX_SCRIPT;
            downloadEsxCommand = string.Format(DOWNLOAD_ESX_XML_COMMAND_P2V, inTargetEsxIP);
            cmdOptions = cmdOptions + "  " + downloadEsxCommand;

            string command = Perlbinary;
            // Debug.WriteLine("Executing below command command " + command + " " + cmdOptions);
            Trace.WriteLine(DateTime.Now + " \t  Exiting: DownLoadXmlFileForP2v() of ESX.cs with targetESXIP " + inTargetEsxIP);

            return WinTools.ExecuteNoWait(command, cmdOptions);

        }


        public int ExecuteRecoveryScript(string inIP)
        {
            // This is to set the recovery fx jobs... 

            Trace.WriteLine(DateTime.Now + " \t Entered: ExecuteRecoveryScript() of ESX.cs with path file name   targetESxIP " + inIP );

            string cmdOptions = null, xmlFile = null;
            string recoveryCommand = null;

            //Add quotest at the begining and end of the path so that spaces won't cause issue
            cmdOptions = Jobautomationscript;
           
            
            recoveryCommand = string.Format(RECOVERY_OPTIONS, inIP);
            cmdOptions = cmdOptions + "  " + recoveryCommand;

            string command = Perlbinary;

            //  Debug.WriteLine("Executing below command command " + command + " " + cmdOptions);
            Trace.WriteLine(DateTime.Now + " \t Exiting: ExecuteRecoveryScript() of ESX.cs with path file name targetESxIP " + inIP );

            return WinTools.Execute(command, cmdOptions, Perltimeout);


            // return 0;



        }

        public int ExecuteJobAutomationScriptForDrdrill(string inEsxIP, string filePath)
        {// this is a call for the esx.xml at the time of addition of disk....only for p2v

            try
            {
                Trace.WriteLine(DateTime.Now + " \t  Entered: ExecuteJobAutomationScriptForDrDrill() of ESX.cs with targetESXIP " + inEsxIP +  " \t filepath " + filePath);

                string cmdOptions = null, xmlFile = null;
                string commandForDrdrill = null;

                
                cmdOptions = Jobautomationscript;
                commandForDrdrill = string.Format(Jobautomationfordrdrill, inEsxIP, "\"" + filePath + "\"");
                cmdOptions = cmdOptions + "  " + commandForDrdrill;

                string command = Perlbinary;
                // Debug.WriteLine("Executing below command command " + command + " " + cmdOptions);
                Trace.WriteLine(DateTime.Now + " \t  Exiting: ExecuteJobAutomationScriptForDrDrill() of ESX.cs with targetESXIP " + inEsxIP );
                return WinTools.Execute(command, cmdOptions, Perltimeout);
            }
            catch (Exception e)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);
                System.FormatException nht = new FormatException("Inner exception", e);
                Trace.WriteLine(nht.GetBaseException());
                Trace.WriteLine(nht.Data);
                Trace.WriteLine(nht.InnerException);
                Trace.WriteLine(nht.Message);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + e.Message);
                Trace.WriteLine("ERROR___________________________________________");
                Trace.WriteLine(DateTime.Now + "Esx.cs: ExecuteJobAutomationScriptForDrDrill Caught an exception " + e.Message);
                return 1;
            }



        }

        public int ExecuteJobAutomationScript(OperationType operation, string vconmt)
        { // This is to set the protection fx jobs..
            Trace.WriteLine(DateTime.Now + " \t Entered: ExecuteJobAutomationScript() of ESX.cs with targetESXIP Operation = " + operation.ToString() + "\t VCON " + vconmt);

            string cmdOptions = null;
            string executeJobAutomationScriptCommand = null;
            string option = "";


            switch (operation)
            {
                case OperationType.Initialprotection:
                    option = Jobautomationscriptcommand;
                    break;
                case OperationType.Additionofdisk:
                    option = Jobautomationforincrementaldisk;
                    break;
                case OperationType.Failback:
                    option = jobautomationfailbackcommand;
                    break;
                case OperationType.Offlinesync:
                    option = Jobautomationscriptcommand;
                    break;
                case OperationType.Resume:
                    option = Jobautomationscriptcommand;
                    break;
                case OperationType.Drdrill:
                    option = Jobautomationfordrdrill;
                    break;
                case OperationType.Removedisk:
                    option = Commandtowritescsiunitsfile;
                    break;
            }


            //Add quotest at the begining and end of the path so that spaces won't cause issue
            cmdOptions = Jobautomationscript;

            executeJobAutomationScriptCommand = string.Format(option,vconmt);

            cmdOptions = cmdOptions + "  " + executeJobAutomationScriptCommand;


            string command = Perlbinary;

            // Debug.WriteLine("Executing below command " + command + " " + cmdOptions);
            Trace.WriteLine(DateTime.Now + " \t Exiting: ExecuteJobAutomationScript() of ESX.cs with targetESXIP ");

            return WinTools.Execute(command, cmdOptions, Perltimeout);




        }





        /*
                public Process ExecuteJobAutomationScript(string inIp, string inUserName, string inPassWord, string incremental)
                {
                    Trace.WriteLine(DateTime.Now + " \t Entered: ExecuteJobAutomationScript() of ESX.cs with targetESXIP " + inIp + " username " + inUserName + " is incremental disk " + incremental);

                    string cmdOptions = null;
                    string executeJobAutomationScriptCommand = null;

                    //Add quotest at the begining and end of the path so that spaces won't cause issue
                    cmdOptions = "\"" + _installPath + "\"" + "\\" + JOB_AUTOMATION_SCRIPT;
                    if (incremental == "no")
                    {

                        executeJobAutomationScriptCommand = string.Format(JOB_AUTOMATION_SCRIPT_COMMAND, inIp, inUserName, inPassWord);
                    }
                    else if (incremental == "yes")
                    {
                        executeJobAutomationScriptCommand = string.Format(JOB_AUTOMATION_FOR_INCREMENTAL_DISK, inIp, inUserName, inPassWord,incremental);
                    }
                    cmdOptions = cmdOptions + "  " + executeJobAutomationScriptCommand;

            
                    string command = PERL_BINARY;

                  //  Debug.WriteLine("Executing below command " + command + " " + cmdOptions);
                    Trace.WriteLine(DateTime.Now + " \t Exiting: ExecuteJobAutomationScript() of ESX.cs with targetESXIP " + inIp + " username " + inUserName + " is incremental disk " + incremental);

                    return WinTools.ExecuteNoWait(command, cmdOptions);


                 
                }

                public Process ExecuteJobAutomationScriptForFailBack(string inIp, string inUserName, string inPassWord)
                {
                    Trace.WriteLine(DateTime.Now + " \t Entered: ExecuteJobAutomationScriptForFailBack() of ESX.cs with targetESXIP " + inIp + " username " + inUserName );

                    string cmdOptions = null;
                    string executeJobAutomationScriptCommand = null;

                    //Add quotest at the begining and end of the path so that spaces won't cause issue
                    cmdOptions = "\"" + _installPath + "\"" + "\\" + JOB_AUTOMATION_SCRIPT;


                    executeJobAutomationScriptCommand = string.Format(JOB_AUTOMATION_FAILBACK_COMMAND, inIp, inUserName, inPassWord);
          
           
                    cmdOptions = cmdOptions + "  " + executeJobAutomationScriptCommand;
             
            
                    string command = PERL_BINARY;

                   // Debug.WriteLine("Executing below command " + command + " " + cmdOptions);
                    Trace.WriteLine(DateTime.Now + " \t Exiting: ExecuteJobAutomationScriptForFailBack() of ESX.cs with targetESXIP " + inIp + " username " + inUserName);

                    return WinTools.ExecuteNoWait(command, cmdOptions);



                }

         */
        public int DownloadFile(string inIP, string fileName)
        {

            // this is to get esx_master_targetesxip.xml from the esx box
            Trace.WriteLine(DateTime.Now + " \t Entered: DownloadFile() of ESX.cs with targetESXIP " + inIP +" file name " + fileName);


            string cmdOptions = null, xmlFile = null;
            string downLoadEsxMasterXmlCommand = null;


            //Add quotest at the begining and end of the path so that spaces won't cause issue
            cmdOptions = Protectscript;

           
            
            downLoadEsxMasterXmlCommand = string.Format(DOWNLOAD_ESX_MASTER_XML_COMMAND, inIP, fileName);
            cmdOptions = cmdOptions + "  " + downLoadEsxMasterXmlCommand;


            string command = Perlbinary;

            // Debug.WriteLine("Executing below command command " + command + " " + cmdOptions);
            Trace.WriteLine(DateTime.Now + " \t Exiting: DownloadFile() of ESX.cs with targetESXIP " + inIP +  " file name " + fileName);

            return WinTools.Execute(command, cmdOptions, Perltimeout);

        }

        public int DownLoadOfflineSyncXmlFile(string inIP, string fileName)
        {  // this is to get esx_master_offlinesync.xml from the esx box in case of ofline sync
            Trace.WriteLine(DateTime.Now + " \t Entered: DownLoadOfflineSyncXmlFile() of ESX.cs with targetESXIP " + inIP +  " file name " + fileName);


            string cmdOptions = null, xmlFile = null;
            string downLoadEsxMasterXmlCommand = null;


            //Add quotest at the begining and end of the path so that spaces won't cause issue
            cmdOptions = Protectscript;

           
            
            downLoadEsxMasterXmlCommand = string.Format(GET_FILE_FOR_OFFLINE_SYNC, inIP, fileName);
            cmdOptions = cmdOptions + "  " + downLoadEsxMasterXmlCommand;


            string command = Perlbinary;

            // Debug.WriteLine("Executing below command command " + command + " " + cmdOptions);
            Trace.WriteLine(DateTime.Now + " \t Exiting: DownLoadOfflineSyncXmlFile() of ESX.cs with targetESXIP " + inIP +  " file name " + fileName);


            return WinTools.Execute(command, cmdOptions, Perltimeout);

        }

        public int UploadOfflineSyncXmlFile(string inIP, string fileName, string datastoreName)
        {  // this is to put esx_master_offlincesyc.xml from the esx box in case of offlinsync
            Trace.WriteLine(DateTime.Now + " \t Entered: UploadOfflineSyncXmlFile() of ESX.cs with targetESXIP " + inIP +" file name " + fileName + " datastorename " + datastoreName);



            string cmdOptions = null, xmlFile = null;
            string upLoadEsxMasterXmlCommand = null;

            //Add quotest at the begining and end of the path so that spaces won't cause issue
            cmdOptions = Protectscript;

           
            upLoadEsxMasterXmlCommand = string.Format(PUT_FILE_FOR_OFFLINE_SYNC, inIP, fileName, datastoreName);
            cmdOptions = cmdOptions + "  " + upLoadEsxMasterXmlCommand;


            string command = Perlbinary;
            Trace.WriteLine(DateTime.Now + " \t Entered: UploadOfflineSyncXmlFile() of ESX.cs with targetESXIP " + inIP + " file name " + fileName + " datastorename " + datastoreName);


            // Debug.WriteLine("Executing below command command " + command + " " + cmdOptions);

            return WinTools.Execute(command, cmdOptions, Perltimeout);


        }

        public int UploadFile(string inIP, string fileName)
        {  // this is to put esx_master_targetesxip.xml from the esx box
            Trace.WriteLine(DateTime.Now + " \t Entered: UploadFile() of ESX.cs with targetESXIP " + inIP + " file name " + fileName);

            string cmdOptions = null, xmlFile = null;
            string upLoadEsxMasterXmlCommand = null;

            //Add quotest at the begining and end of the path so that spaces won't cause issue
            cmdOptions = Protectscript;

            
            upLoadEsxMasterXmlCommand = string.Format(UPLOAD_ESX_MASTER_XML_COMMAND, inIP, fileName);
            cmdOptions = cmdOptions + "  " + upLoadEsxMasterXmlCommand;


            string command = Perlbinary;

            // Debug.WriteLine("Executing below command command " + command + " " + cmdOptions);
            Trace.WriteLine(DateTime.Now + " \t Entered: UploadFile() of ESX.cs with targetESXIP " + inIP +  " file name " + fileName);

            return WinTools.Execute(command, cmdOptions, Perltimeout);

        }

        private class MyNicOrderCopare : IComparer
        {

            // Calls CaseInsensitiveComparer.Compare with the parameters reversed.
            int IComparer.Compare(Object x, Object y)
            {
                Hashtable nic1 = (Hashtable)x;
                Hashtable nic2 = (Hashtable)y;
                
                if (nic1["network_label"] != null && nic2["network_label"] != null)
                {
                    string nic1Name = nic1["network_label"].ToString();
                     string nic2Name = nic2["network_label"].ToString();
                    return nic1Name.CompareTo(nic2Name);
                }
                return 0;
                
               
            }
        }


        public bool ReadEsxConfigFile(string inStrFilename, Role inEsxRole, string inIP)
        {

            // Once the sourcexml.xm and masterxml.xml got generated this method will read that files...
            Trace.WriteLine(DateTime.Now + " \t Entered: ReadEsxConfigFile() of ESX.cs with filename " + inStrFilename + " ip " + inIP );
            //Recommended solution is to completely disable external resource resolution by setting any XmlResolver properties to null
            //based on above comment we have added xmlresolver as null
            XmlDocument document = new XmlDocument();
            document.XmlResolver = null;
            XmlNodeList hostNodes = null;
            XmlNodeList dataStoreNodes = null;
            XmlNodeList esxNodes = null;
            XmlNodeList lunNodes = null;
            XmlNodeList resourcePoolNodes = null;
            XmlNodeList nicNodes = null;
            XmlNodeList networkNodes = null;
            XmlNodeList configurationNode = null;


            string fileFullPath = filePath + "\\" + inStrFilename;
            
            if (!File.Exists(fileFullPath))
            {

                Trace.WriteLine("01_002_001: The file " + fileFullPath + " could not be located" + "in dir " + filePath);

                Console.WriteLine("01_002_001: The file " + fileFullPath + " could not be located" + "in dir " + filePath);
                return false;


            }


            try
            {
                //StreamReader reader = new StreamReader(fileFullPath);
                //string xmlString = reader.ReadToEnd();
                XmlReaderSettings settings = new XmlReaderSettings();
                settings.ProhibitDtd = true;
                settings.XmlResolver = null;
                //To solve fx cop issue CA3056 we are using xml reader and adding xmlreadersetting for secure.
                using (XmlReader reader1 = XmlReader.Create(fileFullPath, settings))
                {
                    document.Load(reader1);
                   // reader1.Close();
                }
               // reader.Close();
            }
            catch (SystemException xmle)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(xmle, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + xmle.Message);
                Trace.WriteLine("ERROR___________________________________________");
                Trace.WriteLine(DateTime.Now + "\t " + "ESX.cs:ReadEsxConfigFile caught Exception :" + xmle.Message);
                return false;
            }

            try
            {

                esxNodes = document.GetElementsByTagName("INFO");
                if (inEsxRole == Role.Primary)
                {
                    foreach (XmlNode node in esxNodes)
                    {
                        XmlAttributeCollection attribColl = node.Attributes;
                        if (attribColl.GetNamedItem("host_version") != null)
                        {
                            VERSIONOFPRIMARYVSPHERESERVER = attribColl.GetNamedItem("host_version").Value.ToString();
                            // MessageBox.Show(VERSION_OF_PRIMARY_VSPHERE_SERVER);
                        }
                    }
                }
                //Get IP address of ESX 
                foreach (XmlNode node in esxNodes)
                {
                    XmlAttributeCollection attribColl = node.Attributes;
                    if (attribColl.GetNamedItem("ip") != null)
                    {
                        ip = attribColl.GetNamedItem("ip").Value.ToString();
                    }
                }

                //Only either SOURCE_ESX or TARGET_ESX will be present an a XML file
                esxNodes = document.GetElementsByTagName("INFO");

                if (inEsxRole == Role.Secondary)
                {
                    foreach (XmlNode node in esxNodes)
                    {
                        XmlAttributeCollection attribColl = node.Attributes;
                        if (attribColl.GetNamedItem("host_version") != null)
                        {
                            VERSIONOFSECONDARYVSPHERE_SERVER = attribColl.GetNamedItem("host_version").Value.ToString();
                            //  MessageBox.Show(VERSION_OF_SECONDARY_VSPHERE_SERVER);
                        }
                    }
                }


                hostNodes = document.GetElementsByTagName("host");
                dataStoreNodes = document.GetElementsByTagName("datastore");
                lunNodes = document.GetElementsByTagName("lun");
                nicNodes = document.GetElementsByTagName("nic");
                resourcePoolNodes = document.GetElementsByTagName("resourcepool");
                networkNodes = document.GetElementsByTagName("network");
                configurationNode = document.GetElementsByTagName("config");
                dataStoreList = new ArrayList();
                resourcePoolList = new ArrayList();
                lunList = new ArrayList();
                networkList = new ArrayList();
                configurationList = new ArrayList();
                // Console.WriteLine("Values = {0}, {1}, {2}, {3} \n", host.hostName, host.displayName, host.ip, host.os);
                foreach (XmlNode node in hostNodes)
                {
                    Host host = ReadHostNode(node);
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

                    foreach (XmlNode nodes in esxNodes)
                    {
                        XmlAttributeCollection attribColls = nodes.Attributes;
                        if (attribColls.GetNamedItem("vCenter") != null)
                        {
                            host.vCenterProtection = attribColls.GetNamedItem("vCenter").Value.ToString();
                        }
                    }
                    XmlAttributeCollection attribColl = node.Attributes;
                    if (attribColl.GetNamedItem("source_uuid") != null && attribColl.GetNamedItem("source_uuid").Value.Length != 0)
                    {
                        host.source_uuid = attribColl.GetNamedItem("source_uuid").Value.ToString();
                    }
                    if (attribColl.GetNamedItem("vSpherehostname") != null && attribColl.GetNamedItem("vSpherehostname").Value.Length != 0)
                    {
                        host.vSpherehost = attribColl.GetNamedItem("vSpherehostname").Value.ToString();
                    }
                    if (attribColl.GetNamedItem("hostversion") != null && attribColl.GetNamedItem("hostversion").Value.Length != 0)
                    {
                        host.hostversion = attribColl.GetNamedItem("hostversion").Value.ToString();
                    }
                    if (attribColl.GetNamedItem("datacenter") != null && attribColl.GetNamedItem("datacenter").Value.Length != 0)
                    {
                        host.datacenter = attribColl.GetNamedItem("datacenter").Value.ToString();
                    }
                    if (attribColl.GetNamedItem("vmx_version") != null && attribColl.GetNamedItem("vmx_version").Value.Length != 0)
                    {
                        host.vmxversion = attribColl.GetNamedItem("vmx_version").Value.ToString();
                    }
                    if (attribColl.GetNamedItem("floppy_device_count") != null && attribColl.GetNamedItem("floppy_device_count").Value.Length != 0)
                    {
                        host.floppy_device_count = attribColl.GetNamedItem("floppy_device_count").Value.ToString();
                    }
                    if (attribColl.GetNamedItem("alt_guest_name") != null && attribColl.GetNamedItem("alt_guest_name").Value.Length != 0)
                    {
                        host.alt_guest_name = attribColl.GetNamedItem("alt_guest_name").Value.ToString();
                    }
                    if (attribColl.GetNamedItem("resourcepoolgrpname") != null && attribColl.GetNamedItem("resourcepoolgrpname").Value.Length != 0)
                    {
                        host.resourcepoolgrpname = attribColl.GetNamedItem("resourcepoolgrpname").Value.ToString();
                    }
                    if (attribColl.GetNamedItem("efi") != null && attribColl.GetNamedItem("efi").Value.Length != 0)
                    {
                        host.efi = bool.Parse(attribColl.GetNamedItem("efi").Value.ToString());
                    }
                    if (attribColl.GetNamedItem("template") != null && attribColl.GetNamedItem("template").Value.Length != 0)
                    {
                        host.template = bool.Parse(attribColl.GetNamedItem("template").Value.ToString());
                    }
                    if (attribColl.GetNamedItem("vmx_path") != null && attribColl.GetNamedItem("vmx_path").Value.Length != 0)
                        host.vmx_path = attribColl.GetNamedItem("vmx_path").Value.ToString();
                    if (attribColl.GetNamedItem("resourcepool") != null && attribColl.GetNamedItem("resourcepool").Value.Length != 0)
                        host.resourcePool = attribColl.GetNamedItem("resourcepool").Value.ToString();
                    if (attribColl.GetNamedItem("display_name") != null && attribColl.GetNamedItem("display_name").Value.Length != 0)
                        host.displayname = attribColl.GetNamedItem("display_name").Value.ToString();
                    if (attribColl.GetNamedItem("os_info") != null && attribColl.GetNamedItem("os_info").Value.Length != 0)
                    {
                        string osType = attribColl.GetNamedItem("os_info").Value.ToString();
                        if (osType != "all")
                        {
                            host.os = (OStype)Enum.Parse(typeof(OStype), osType);
                        }
                    }
                    if (attribColl.GetNamedItem("cpucount") != null && attribColl.GetNamedItem("cpucount").Value.Length != 0)
                        host.cpuCount = int.Parse(attribColl.GetNamedItem("cpucount").Value.ToString());
                    if (attribColl.GetNamedItem("memsize") != null && attribColl.GetNamedItem("memsize").Value.Length != 0)
                        host.memSize = int.Parse(attribColl.GetNamedItem("memsize").Value.ToString());
                    if (attribColl.GetNamedItem("powered_status") != null && attribColl.GetNamedItem("powered_status").Value.Length != 0)
                        host.poweredStatus = attribColl.GetNamedItem("powered_status").Value.ToString();
                    if (attribColl.GetNamedItem("ip_address") != null && attribColl.GetNamedItem("ip_address").Value.Length != 0)
                        host.ip = attribColl.GetNamedItem("ip_address").Value.ToString();
                    if (attribColl.GetNamedItem("vmwaretools") != null && attribColl.GetNamedItem("vmwaretools").Value.Length != 0)
                        host.vmwareTools = attribColl.GetNamedItem("vmwaretools").Value.ToString();
                    if (attribColl.GetNamedItem("operatingsystem") != null && attribColl.GetNamedItem("operatingsystem").Value.Length != 0)
                        host.operatingSystem = attribColl.GetNamedItem("operatingsystem").Value.ToString();
                    if (attribColl.GetNamedItem("snapshot") != null && attribColl.GetNamedItem("snapshot").Value.Length != 0)
                        host.snapshot = attribColl.GetNamedItem("snapshot").Value.ToString();
                    if (attribColl.GetNamedItem("vm_in_datastore") != null && attribColl.GetNamedItem("vm_in_datastore").Value.Length != 0)
                        host.datastore = attribColl.GetNamedItem("vm_in_datastore").Value.ToString();
                    if (attribColl.GetNamedItem("folder_path") != null && attribColl.GetNamedItem("folder_path").Value.Length != 0)
                        host.folder_path = attribColl.GetNamedItem("folder_path").Value.ToString();
                    if (attribColl.GetNamedItem("plan") != null && attribColl.GetNamedItem("plan").Value.Length != 0)
                        host.plan = attribColl.GetNamedItem("plan").Value.ToString();
                    if (attribColl.GetNamedItem("esxIp") != null && attribColl.GetNamedItem("esxIp").Value.Length != 0)
                        host.esxIp = attribColl.GetNamedItem("esxIp").Value.ToString();
                    if (attribColl.GetNamedItem("mastertargetdisplayname") != null && attribColl.GetNamedItem("mastertargetdisplayname").Value.Length != 0)
                        host.masterTargetDisplayName = attribColl.GetNamedItem("mastertargetdisplayname").Value.ToString();
                    if (attribColl.GetNamedItem("mastertargethostname") != null && attribColl.GetNamedItem("mastertargethostname").Value.Length != 0)
                        host.masterTargetHostName = attribColl.GetNamedItem("mastertargethostname").Value.ToString();
                    if (attribColl.GetNamedItem("targetesxip") != null && attribColl.GetNamedItem("targetesxip").Value.Length != 0)
                        host.targetESXIp = attribColl.GetNamedItem("targetesxip").Value.ToString();
                    if (attribColl.GetNamedItem("disk_name") != null && attribColl.GetNamedItem("disk_name").Value.Length != 0)
                        host.vmdkHash["disk_name"] = attribColl.GetNamedItem("disk_name").Value.ToString();
                    if (attribColl.GetNamedItem("size") != null && attribColl.GetNamedItem("size").Value.Length != 0)
                    {
                        host.vmdkHash["size"] = attribColl.GetNamedItem("size").Value.ToString();
                        //  diskMapping[0] = attribColl.GetNamedItem("size").Value.ToString();
                    }
                    if (attribColl.GetNamedItem("protected") != null && attribColl.GetNamedItem("protected").Value.Length != 0)
                    {
                        host.vmdkHash["protected"] = attribColl.GetNamedItem("protected").Value.ToString();
                    }

                    if (attribColl.GetNamedItem("rdm") != null && attribColl.GetNamedItem("rdm").Value.Length != 0)
                        host.rdmpDisk = attribColl.GetNamedItem("rdm").Value.ToString();
                    host.esxIp = inIP;
                   
                    if (host != null)
                    {
                        //Trace.WriteLine(DateTime.Now + " \t Printing the hostname of each vm " + host.hostName + " displayname "+  host.displayName + " Power "+host.poweredStatus);
                        hostList.Add(host);
                        //Trace.WriteLine(DateTime.Now + " \t Printing the count of the list " + _hostList.Count.ToString());

                    }
                    //foreach (XmlNode nodes in dataStoreNodes)
                    //{
                    //    DataStore dataStore = ReadDataStoreNode(nodes);
                    //    //   _dataStoreList.Add(dataStore);
                    //    host.dataStoreList.Add(dataStore);
                    //}
                    //foreach (XmlNode nodes in configurationNode)
                    //{
                    //    Configuration config = ReadConfigurationNodes(nodes);
                    //    host.configurationList.Add(config);
                    //}
                    //foreach (XmlNode nodes in lunNodes)
                    //{
                    //    RdmLuns lun = ReadLunNodes(nodes);
                    //    host.lunList.Add(lun);
                    //}
                    //foreach (XmlNode nodes in resourcePoolNodes)
                    //{
                    //    ResourcePool resourcePool = ReadResourcePoolNode(nodes);
                    //    host.resourcePoolList.Add(resourcePool);
                    //}
                    //foreach (XmlNode nodes in networkNodes)
                    //{
                    //    Network network = ReadNetWorkNode(nodes);
                    //    host.networkNameList.Add(network);
                    //}
                }
                //Added for the datastore in the esx.xml    
                
                foreach (XmlNode node in dataStoreNodes)
                {
                    // Trace.WriteLine(DateTime.Now + "\t Came here to read datastroe nodes ");
                    DataStore dataStore = ReadDataStoreNode(node);
                    dataStoreList.Add(dataStore);
                }
                foreach (XmlNode nodes in resourcePoolNodes)
                {
                    ResourcePool resourcePool = ReadResourcePoolNode(nodes);
                    resourcePoolList.Add(resourcePool);
                }
                foreach (XmlNode nodes in lunNodes)
                {
                    RdmLuns lun = ReadLunNodes(nodes);
                    lunList.Add(lun);
                }
                foreach (XmlNode node in networkNodes)
                {
                    //Trace.WriteLine(DateTime.Now + "\t Came here to read the network values");
                    Network network = ReadNetWorkNode(node);
                    networkList.Add(network);
                }
                foreach (XmlNode nodes in configurationNode)
                {
                    Configurations config = ReadConfigurationNodes(nodes);
                    configurationList.Add(config);
                }
                Console.WriteLine();
                Trace.WriteLine(DateTime.Now + " \t Exiting: ReadEsxConfigFile() of ESX.cs with filename " + inStrFilename + " ip " + inIP );
            }
            catch (SystemException e)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: ReadEsxConfigFile " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + e.Message);
                Trace.WriteLine("ERROR___________________________________________");
                Trace.WriteLine(DateTime.Now + "Esx.cs: ReadEsxConfigFile Caught an exception " + e.Message);
                return false;
            }
            return true;
        }

        private DataStore ReadDataStoreNode(XmlNode node)
        {

            // Trace.WriteLine(DateTime.Now + " \t Entered: ReadDataStoreNode() of ESX.cs ");
            // In MasterXml.xml we will read the datastore nodes to display to user.
            XmlNodeReader reader = null;
            DataStore dataStore = new DataStore();
            XmlAttributeCollection attribColl = node.Attributes;
            try
            {
                reader = new XmlNodeReader(node);
                reader.Read();
                if (attribColl.GetNamedItem("datastore_name") != null && attribColl.GetNamedItem("datastore_name").Value.Length > 0)
                {
                    dataStore.name = reader.GetAttribute("datastore_name");
                }
                if (attribColl.GetNamedItem("vSpherehostname") != null && attribColl.GetNamedItem("vSpherehostname").Value.Length > 0)
                {
                    dataStore.vSpherehostname = reader.GetAttribute("vSpherehostname");
                }
                if (attribColl.GetNamedItem("type") != null && attribColl.GetNamedItem("type").Value.Length > 0)
                {
                    dataStore.type = reader.GetAttribute("type");
                }
                if (attribColl.GetNamedItem("total_size") != null && attribColl.GetNamedItem("total_size").Value.Length > 0)
                {
                    dataStore.totalSize = float.Parse(reader.GetAttribute("total_size"));
                }
                if (attribColl.GetNamedItem("free_space") != null && attribColl.GetNamedItem("free_space").Value.Length > 0)
                {
                    dataStore.freeSpace = float.Parse(reader.GetAttribute("free_space"));
                }
                if (attribColl.GetNamedItem("datastore_blocksize_mb") != null && attribColl.GetNamedItem("datastore_blocksize_mb").Value.Length > 0)
                {
                    dataStore.blockSize = int.Parse(reader.GetAttribute("datastore_blocksize_mb"));
                }
                else
                {
                    dataStore.blockSize = 0;
                }
                if (attribColl.GetNamedItem("filesystem_version") != null && attribColl.GetNamedItem("filesystem_version").Value.Length > 0)
                {
                    dataStore.filesystemversion = double.Parse(reader.GetAttribute("filesystem_version"));
                    //Trace.WriteLine(DateTime.Now + " \t Printitng the file system version " + dataStore.filesystemversion.ToString());
                }
            }
            catch (Exception e)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: ReadDataStoreNode " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + e.Message);
                Trace.WriteLine("ERROR___________________________________________");
                Trace.WriteLine(DateTime.Now + "Esx.cs:  Caught an exception " + e.Message);
            }
            // Trace.WriteLine(DateTime.Now + " \t Exiting: ReadDataStoreNode() of ESX.cs ");
            return dataStore;
        }

        private RdmLuns ReadLunNodes(XmlNode node)
        { // In MasterXml.xml we will read the lun nodes to display to user in case of rdm luns in source vms.
            // Trace.WriteLine(DateTime.Now + " \t Entered: ReadLunNodes() of ESX.cs ");
            XmlNodeReader reader = null;
            RdmLuns lun = new RdmLuns();
            XmlAttributeCollection attribColl = node.Attributes;
            reader = new XmlNodeReader(node);
            reader.Read();
            lun.name = reader.GetAttribute("name");
            lun.adapter = reader.GetAttribute("adapter");
            lun.lun = reader.GetAttribute("lun");
            lun.devicename = reader.GetAttribute("devicename");
            if (attribColl.GetNamedItem("vSpherehostname") != null && attribColl.GetNamedItem("vSpherehostname").Value.Length > 0)
            {
                lun.vSpherehostname = reader.GetAttribute("vSpherehostname");
            }
            try
            {
                lun.capacity_in_kb = double.Parse(reader.GetAttribute("capacity_in_kb"));
            }
            catch (Exception e)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: ReadLunNodes " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + e.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
            // Trace.WriteLine(DateTime.Now + " \t Exiting: ReadLunNodes() of ESX.cs ");
            return lun;
        }

        private Configurations ReadConfigurationNodes(XmlNode node)
        { // In MasterXml.xml we will read the config nodes for the cpu count and memsize.
            // Trace.WriteLine(DateTime.Now + " \t Entered: ReadConfigurationNodes() of ESX.cs ");
            XmlNodeReader reader = null;
            Configurations configure = new Configurations();
            XmlAttributeCollection attribColl = node.Attributes;
            try
            {
                reader = new XmlNodeReader(node);
                reader.Read();
                if (attribColl.GetNamedItem("memsize") != null && attribColl.GetNamedItem("memsize").Value.Length > 0)
                {
                    configure.memsize = float.Parse(reader.GetAttribute("memsize").ToString());
                }
                if (attribColl.GetNamedItem("cpucount") != null && attribColl.GetNamedItem("cpucount").Value.Length > 0)
                {
                    configure.cpucount = int.Parse(reader.GetAttribute("cpucount").ToString());
                }
                if (attribColl.GetNamedItem("vSpherehostname") != null && attribColl.GetNamedItem("vSpherehostname").Value.Length > 0)
                {
                    configure.vSpherehostname = reader.GetAttribute("vSpherehostname");
                }
            }
            catch (Exception e)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: ReadConfigurationNodes " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + e.Message);
                Trace.WriteLine("ERROR___________________________________________");
                Trace.WriteLine(DateTime.Now + "Esx.cs:  Caught an exception " + e.Message);
            }
            // Trace.WriteLine(DateTime.Now + " \t Exiting: ReadConfigurationNodes() of ESX.cs ");
            return configure;
        }
        private ResourcePool ReadResourcePoolNode(XmlNode node)
        {

            //Trace.WriteLine(DateTime.Now + " \t Entered: ReadNetWorkNode() of ESX.cs ");

            XmlNodeReader reader = null;
            ResourcePool resourcePool = new ResourcePool();
            XmlAttributeCollection attribColl = node.Attributes;
            try
            {

                reader = new XmlNodeReader(node);
                reader.Read();
                if (attribColl.GetNamedItem("name") != null && attribColl.GetNamedItem("name").Value.Length > 0)
                {
                    resourcePool.name = reader.GetAttribute("name");
                }
                if (attribColl.GetNamedItem("type") != null && attribColl.GetNamedItem("type").Value.Length > 0)
                {
                    resourcePool.type = reader.GetAttribute("type");
                }
                if (attribColl.GetNamedItem("host") != null && attribColl.GetNamedItem("host").Value.Length > 0)
                {
                    resourcePool.host = reader.GetAttribute("host");
                }
                if (attribColl.GetNamedItem("groupId") != null && attribColl.GetNamedItem("groupId").Value.Length > 0)
                {
                    resourcePool.groupId = reader.GetAttribute("groupId");
                }
                if (attribColl.GetNamedItem("owner") != null && attribColl.GetNamedItem("owner").Value.Length > 0)
                {
                    resourcePool.owner = reader.GetAttribute("owner");
                }
                if (attribColl.GetNamedItem("owner_type") != null && attribColl.GetNamedItem("owner_type").Value.Length > 0)
                {
                    resourcePool.ownertype = reader.GetAttribute("owner_type");
                }

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
                Trace.WriteLine(DateTime.Now + "Esx.cs:  Caught an exception " + e.Message);
            }

            //Trace.WriteLine(DateTime.Now + " \t Exiting: ReadNetWorkNode() of ESX.cs ");
            return resourcePool;

        }

        private Network ReadNetWorkNode(XmlNode node)
        {
            //  Trace.WriteLine(DateTime.Now + " \t Entered: ReadNetWorkNode() of ESX.cs ");
            // In MasterXml.xml we will read the datastore nodes to display to user.to selet the network for for the nics at target side
            XmlNodeReader reader = null;
            Network network = new Network();
            XmlAttributeCollection attribColl = node.Attributes;
            try
            {

                reader = new XmlNodeReader(node);
                reader.Read();
                //Trace.WriteLine(DateTime.Now + " \tprinting the node name " + node.Name);
                if (attribColl.GetNamedItem("name") != null && attribColl.GetNamedItem("name").Value.Length > 0)
                {
                    //Trace.WriteLine(DateTime.Now + " \t Printing the values " + reader.GetAttribute("name"));
                    network.Networkname = reader.GetAttribute("name");
                }
                if (attribColl.GetNamedItem("Type") != null && attribColl.GetNamedItem("Type").Value.Length > 0)
                {
                    network.Networktype = reader.GetAttribute("Type");
                }
                if (attribColl.GetNamedItem("vSpherehostname") != null && attribColl.GetNamedItem("vSpherehostname").Value.Length > 0)
                {
                    network.Vspherehostname = reader.GetAttribute("vSpherehostname");
                }
                if (attribColl.GetNamedItem("switchUuid") != null && attribColl.GetNamedItem("switchUuid").Value.Length > 0)
                {
                    network.Switchuuid = reader.GetAttribute("switchUuid");
                }
                if (attribColl.GetNamedItem("portgroupKey") != null && attribColl.GetNamedItem("portgroupKey").Value.Length > 0)
                {
                    network.PortgroupKey = reader.GetAttribute("portgroupKey");
                }

            }
            catch (Exception e)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(e, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + e.Message);
                Trace.WriteLine("ERROR___________________________________________");
                Trace.WriteLine(DateTime.Now + "Esx.cs:  Caught an exception " + e.Message);

            }
            // Trace.WriteLine(DateTime.Now + " \t Exiting: ReadNetWorkNode() of ESX.cs ");
            return network;

        }

        internal bool WriteDataStoreXmlNodes(XmlWriter writer)
        {
            try
            {
                // Trace.WriteLine(DateTime.Now + " \t Entered: WriteDataStoreXmlNodes() of ESX.cs ");
                foreach (DataStore ds in dataStoreList)
                {
                    ds.WriteXmlNode(writer);

                }
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
            // Trace.WriteLine(DateTime.Now + " \t Exiting: WriteDataStoreXmlNodes() of ESX.cs ");
            return true;
        }

        internal bool WriteNetworkNodes(XmlWriter writer)
        {
            try
            {
                //  Trace.WriteLine(DateTime.Now + " \t Entered: WriteNetworkNodes() of ESX.cs ");
                foreach (Network network in networkList)
                {
                    network.WriteXmlNode(writer);

                }
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
            // Trace.WriteLine(DateTime.Now + " \t Exiting: WriteNetworkNodes() of ESX.cs ");
            return true;
        }

        internal bool WriteConfigurationNodes(XmlWriter writer)
        {
            try
            {
                //Trace.WriteLine(DateTime.Now + " \t Entered: WriteConfigurationNodes() of ESX.cs ");
                foreach (Configurations config in configurationList)
                {
                    config.WriteXmlNode(writer);
                    break;
                }
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
            // Trace.WriteLine(DateTime.Now + " \t Exiting: WriteConfigurationNodes() of ESX.cs ");
            return true;
        }
        internal bool WriteLunXmlNodes(XmlWriter writer)
        {
            try
            {
                // Trace.WriteLine(DateTime.Now + " \t Entered: WriteLunXmlNodes() of ESX.cs ");
                foreach (RdmLuns lun in lunList)
                {
                    lun.WriteXmlNode(writer);
                }
                // Trace.WriteLine(DateTime.Now + " \t Exiting: WriteLunXmlNodes() of ESX.cs ");
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
        internal bool WriteResourcePollNodes(XmlWriter writer)
        {
            try
            {
                // Trace.WriteLine(DateTime.Now + " \t Entered: WriteLunXmlNodes() of ESX.cs ");
                foreach (ResourcePool resourcepool in resourcePoolList)
                {
                    resourcepool.WriteXmlNode(writer);
                }
                // Trace.WriteLine(DateTime.Now + " \t Exiting: WriteLunXmlNodes() of ESX.cs ");
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



        private Host ReadHostNode(XmlNode node)
        {


            //Trace.WriteLine(DateTime.Now + " \t Entered: ReadHostNode() of ESX.cs ");
            XmlNodeReader reader = null;
            string numOfDisksString;
            Host host = new Host();
            try
            {
                reader = new XmlNodeReader(node);
                reader.Read();


                host.hostname = reader.GetAttribute("hostname");
                host.displayname = reader.GetAttribute("display_name");
                host.ip = reader.GetAttribute("ip_address");
                if (host.ip != null)
                {
                    host.IPlist = host.ip.Split(',');
                    host.ipCount = host.IPlist.Length;

                }

                XmlAttributeCollection attribColls = node.Attributes;

                if (reader.GetAttribute("os_info") != null )
                {
                    if (attribColls.GetNamedItem("os_info") != null && attribColls.GetNamedItem("os_info").Value.Length > 0)
                    {
                        string osType = reader.GetAttribute("os_info");

                        if (osType != "all")
                        {
                            host.os = (OStype)Enum.Parse(typeof(OStype), osType);
                        }
                    }


                }
                if (reader.GetAttribute("source_uuid") != null)
                {
                    host.source_uuid = reader.GetAttribute("source_uuid");
                }
                if (reader.GetAttribute("datacenter") != null)
                {
                    host.datacenter = reader.GetAttribute("datacenter");
                }
                if (reader.GetAttribute("hostversion") != null)
                {
                    host.hostversion = reader.GetAttribute("hostversion");
                }
                if (reader.GetAttribute("vSpherehostname") != null)
                {
                    host.vSpherehost = reader.GetAttribute("vSpherehostname");
                }
                host.vmxFile = reader.GetAttribute("vmx_path");
                host.poweredStatus = reader.GetAttribute("powered_status");
                host.vmwareTools = reader.GetAttribute("vmwaretools");
                host.folder_path = reader.GetAttribute("folder_path");
                host.datastore = reader.GetAttribute("datastore");
                if (attribColls.GetNamedItem("operatingsystem") != null && attribColls.GetNamedItem("operatingsystem").Value.Length > 0)
                {
                    host.operatingSystem = reader.GetAttribute("operatingsystem");
                }
                if (attribColls.GetNamedItem("diskenableuuid") != null && attribColls.GetNamedItem("diskenableuuid").Value.Length > 0)
                {
                    host.diskenableuuid = reader.GetAttribute("diskenableuuid");
                }
                host.snapshot = reader.GetAttribute("snapshot");
                host.esxIp = reader.GetAttribute("esxIp");
                host.plan = reader.GetAttribute("plan");
                host.masterTargetDisplayName = reader.GetAttribute("mastertargetdisplayname");
                host.masterTargetHostName = reader.GetAttribute("mastertargethostname");
                host.targetESXIp = reader.GetAttribute("targetesxip");
                if (reader.GetAttribute("memsize") != null)
                {
                    host.memSize = int.Parse(reader.GetAttribute("memsize"));
                }
                if (reader.GetAttribute("cpucount") != null)
                {
                    host.cpuCount = int.Parse(reader.GetAttribute("cpucount"));
                }
                if (reader.GetAttribute("cluster") != null)
                {
                    host.cluster = reader.GetAttribute("cluster");
                }
                if (reader.GetAttribute("ide_count") != null)
                {
                    host.ide_count = reader.GetAttribute("ide_count");
                }
                // Console.WriteLine("Values = {0}, {1}, {2}, {3} \n", host.hostName, host.displayName, host.ip, host.os);
                numOfDisksString = reader.GetAttribute("number_of_disks");

                if ((numOfDisksString != null) && (numOfDisksString != string.Empty))
                {
                    host.numOfDisks = Int32.Parse(numOfDisksString);
                }


                if (reader.GetAttribute("role") == "SECONDARY")
                {
                    host.role = Role.Secondary;
                    /*  if (node.HasChildNodes)
                      {


                          XmlNodeList diskList = node.ChildNodes;

                          host.vmdkHash = new Hashtable();

                          foreach (XmlNode diskNode in diskList)
                          {
                              reader = new XmlNodeReader(diskNode);
                              reader.Read();

                              if (diskNode.Name == "disk")
                              {
                                  reader = new XmlNodeReader(diskNode);
                                  reader.Read();
                                  Hashtable diskHash = new Hashtable();
                                  host.diskHash = new Hashtable();

                                  host.diskHash.Add("Name", reader.GetAttribute("disk_name"));
                                  host.diskHash.Add("Size", reader.GetAttribute("size"));
                                  host.diskHash.Add("disk_type", reader.GetAttribute("disk_type"));
                                  host.diskHash.Add("Protected", reader.GetAttribute("protected"));
                                  host.diskHash.Add("disk_mode", reader.GetAttribute("disk_mode"));
                                  host.diskHash.Add("IsThere", "no");
                                  if (reader.GetAttribute("disk_mode").ToString() == "Mapped Raw LUN")
                                  {
                                      host.diskHash.Add("Rdm", "yes");
                                  }
                                  else
                                  {
                                      host.diskHash.Add("Rdm", "no");

                                  }
                                  host.diskHash.Add("ide_or_scsi", reader.GetAttribute("ide_or_scsi"));
                                  host.diskHash.Add("scsi_mapping_vmx", reader.GetAttribute("scsi_mapping_vmx"));
                                  host.diskHash.Add("scsi_mapping_host", reader.GetAttribute("scsi_mapping_host"));
                                  host.diskHash.Add("independent_persistent", reader.GetAttribute("independent_persistent"));
                                  host.diskHash.Add("Selected", "Yes");
                                  host.disks.list.Add(host.diskHash);
                              }
                          }
                      }*/
                }
                else
                {

                    host.role = Role.Primary;
                }
                //Get host type. It can be either ESX, P2V, Xen, Hyperv , Amazon etc
                switch (node.ParentNode.Name)
                {
                    case "ESX":
                        host.hostType = Hosttype.Vmwareguest;
                        break;
                    case "P2V":
                        host.hostType = Hosttype.Physical;
                        break;
                    default:
                        host.hostType = Hosttype.Vmwareguest;
                        break;
                }


                if (node.HasChildNodes)
                {

                    //Trace.WriteLine(DateTime.Now + " Came here to read disk nodes ");
                    XmlNodeList diskList = node.ChildNodes;
                    XmlNodeList nicList = node.ChildNodes;
                    host.vmdkHash = new Hashtable();

                    foreach (XmlNode diskNode in diskList)
                    {
                        reader = new XmlNodeReader(diskNode);
                        reader.Read();

                        if (diskNode.Name == "disk")
                        {
                            reader = new XmlNodeReader(diskNode);
                            reader.Read();
                            Hashtable diskHash = new Hashtable();
                            host.diskHash = new Hashtable();

                            host.diskHash.Add("Name", reader.GetAttribute("disk_name"));
                            host.diskHash.Add("Size", reader.GetAttribute("size"));
                            host.diskHash.Add("disk_type", reader.GetAttribute("disk_type"));
                            host.diskHash.Add("Protected", reader.GetAttribute("protected"));
                            host.diskHash.Add("disk_mode", reader.GetAttribute("disk_mode"));
                            host.diskHash.Add("IsThere", "no");
                            if (reader.GetAttribute("disk_mode").ToString() == "Mapped Raw LUN")
                            {
                                host.diskHash.Add("Rdm", "yes");
                            }
                            else
                            {
                                host.diskHash.Add("Rdm", "no");

                            }
                            host.diskHash.Add("ide_or_scsi", reader.GetAttribute("ide_or_scsi"));
                            host.diskHash.Add("scsi_mapping_vmx", reader.GetAttribute("scsi_mapping_vmx"));
                            host.diskHash.Add("scsi_mapping_host", reader.GetAttribute("scsi_mapping_host"));
                            host.diskHash.Add("independent_persistent", reader.GetAttribute("independent_persistent"));
                            host.diskHash.Add("Selected", "Yes");
                            if (reader.GetAttribute("controller_type") != null)
                            {
                                host.diskHash.Add("controller_type", reader.GetAttribute("controller_type"));
                            }
                            if (reader.GetAttribute("source_disk_uuid") != null)
                            {
                                host.diskHash.Add("source_disk_uuid", reader.GetAttribute("source_disk_uuid"));
                            }
                            if (reader.GetAttribute("controller_mode") != null)
                            {
                                host.diskHash.Add("controller_mode", reader.GetAttribute("controller_mode"));
                               // Trace.WriteLine(DateTime.Now + "\t reading controller_mode valeu " + reader.GetAttribute("controller_mode"));
                            }
                            if (reader.GetAttribute("cluster_disk") != null)
                            {
                                host.diskHash.Add("cluster_disk", reader.GetAttribute("cluster_disk"));
                            }
                            host.disks.list.Add(host.diskHash);
                        }
                    }
                    foreach (XmlNode nic in nicList)
                    {
                        reader = new XmlNodeReader(nic);
                        XmlAttributeCollection attribColl = nic.Attributes;
                        reader.Read();
                        if (nic.Name == "nic")
                        {
                            Hashtable nicHash = new Hashtable();
                            host.nicHash = new Hashtable();
                            if (attribColl.GetNamedItem("network_label") != null && attribColl.GetNamedItem("network_label").Value.Length > 0)
                            {
                                host.nicHash.Add("network_label", reader.GetAttribute("network_label"));
                            }
                            if (attribColl.GetNamedItem("network_name") != null && attribColl.GetNamedItem("network_name").Value.Length > 0)
                            {
                                host.nicHash.Add("network_name", reader.GetAttribute("network_name"));
                            }
                            if (attribColl.GetNamedItem("nic_mac") != null && attribColl.GetNamedItem("nic_mac").Value.Length > 0)
                            {
                                host.nicHash.Add("nic_mac", reader.GetAttribute("nic_mac"));
                            }
                            if (attribColl.GetNamedItem("ip") != null && attribColl.GetNamedItem("ip").Value.Length > 0)
                            {
                                //Commented because now we are going to support multiple ips.
                                //string ips = reader.GetAttribute("ip");
                                //string ip = ips.Split(',')[0];
                                host.nicHash.Add("ip", reader.GetAttribute("ip"));
                            }
                            if (attribColl.GetNamedItem("dhcp") != null && attribColl.GetNamedItem("dhcp").Value.Length > 0)
                            {
                                host.nicHash.Add("dhcp", int.Parse(reader.GetAttribute("dhcp").ToString()));
                            }
                            else
                            {
                                host.nicHash.Add("dhcp", "0");
                            }
                            if (attribColl.GetNamedItem("mask") != null && attribColl.GetNamedItem("mask").Value.Length > 0)
                            {
                                host.nicHash.Add("mask", reader.GetAttribute("mask"));
                            }
                            if (attribColl.GetNamedItem("gateway") != null && attribColl.GetNamedItem("gateway").Value.Length > 0)
                            {
                                host.nicHash.Add("gateway", reader.GetAttribute("gateway"));
                            }
                            if (attribColl.GetNamedItem("dnsip") != null && attribColl.GetNamedItem("dnsip").Value.Length > 0)
                            {
                                // host.nicHash.Add("dnsip", reader.GetAttribute("dnsip"));
                                //string ips = reader.GetAttribute("dnsip");
                                //string ip = ips.Split(',')[0];
                                host.nicHash.Add("dnsip", reader.GetAttribute("dnsip"));
                            }
                            if (attribColl.GetNamedItem("address_type") != null && attribColl.GetNamedItem("address_type").Value.Length > 0)
                            {
                                host.nicHash.Add("address_type", reader.GetAttribute("address_type"));
                            }
                            else
                            {
                                host.nicHash.Add("address_type", null);
                            }
                            if (attribColl.GetNamedItem("adapter_type") != null && attribColl.GetNamedItem("adapter_type").Value.Length > 0)
                            {
                                host.nicHash.Add("adapter_type", reader.GetAttribute("adapter_type"));
                            }
                            else
                            {
                                host.nicHash.Add("adapter_type", null);
                            }
                            host.nicHash.Add("Selected", "yes");
                            host.nicHash.Add("new_ip", null);
                            host.nicHash.Add("new_mask", null);
                            host.nicHash.Add("new_dnsip", null);
                            host.nicHash.Add("new_gateway", null);
                            host.nicHash.Add("new_network_name", null);
                            host.nicHash.Add("new_macaddress", null);
                            host.nicList.list.Add(host.nicHash);
                        }
                    }
                }
                // Trace.WriteLine(DateTime.Now + " \t Exiting: ReadHostNode() of ESX.cs ");
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
                Trace.WriteLine(DateTime.Now + "Caught exception at Method: ReadHostNode " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
            return host;
        }

        public ArrayList GetHostList
        {
            get
            {
                return hostList;
            }
            set
            {
                value = hostList;
            }
        }

        public ArrayList GetTargetEsxDataStoreList
        {
            get
            {
                return dataStoreList;
            }
            set
            {
                value = dataStoreList;
            }
        }

        private ArrayList GetTargetEsxDataStoreNamesList()
        {
            Trace.WriteLine(DateTime.Now + " \t Entered: getTargetEsxDataStoreNamesList() of ESX.cs ");
            ArrayList _dataStoreNamesList = new ArrayList();
            try
            {
                foreach (DataStore ds in dataStoreList)
                {
                    _dataStoreNamesList.Add(ds.name);
                }
            }
            catch (Exception ex)
            {
                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);
                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
            }
            Trace.WriteLine(DateTime.Now + " \t Exiting: getTargetEsxDataStoreNamesList() of ESX.cs ");
            return _dataStoreNamesList;
        }





        internal void Print()
        {
            try
            {
                //Trace.WriteLine(DateTime.Now + " \t Esx ip = " + ip);
                //Trace.WriteLine(DateTime.Now + " \t Esx UserName = " + userName);
                //Trace.WriteLine(DateTime.Now + " \t Esx Password = " + passWord);

                foreach (DataStore ds in dataStoreList)
                {
                    Trace.WriteLine(DateTime.Now + " \t DataStore = " + ds.name);
                }
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");


            }
            PrintVms();


        }
        internal void PrintVms()
        {
            try
            {
                Trace.WriteLine(DateTime.Now + " \t Entered: PrintVms() of ESX.cs ");
                foreach (Host host in hostList)
                {
                    host.Print();
                }

                Trace.WriteLine("*************************************");


                if (dataStoreList != null)
                {
                    foreach (DataStore dataStore in dataStoreList)
                    {
                        dataStore.Print();
                    }
                }
                Trace.WriteLine(DateTime.Now + " \t Exiting: PrintVms() of ESX.cs ");
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");


            }

        }


        public int WmiRecoveryForV2p(string executeIP, string directory, OperationType operation)
        {
            try
            {
                string option = "";
                string cmdOptions = null, xmlFile = null;
                string commandForWMIRecopvery = null;
                switch (operation)
                {
                    case OperationType.Recover:
                        cmdOptions = RECOVERY_SCRIPT_FOR_WMI;
                        option = WMI_BASED_V2PRECOVERY_COMMAND;
                        break;
                    case OperationType.Drdrill:
                        cmdOptions = RECOVERY_SCRIPT_FOR_WMI;
                        option = WMI_BASED_DRDRILL_COMMAND;
                        break;
                }
                cmdOptions = "\"" + scriptsPath + "\"" + "\\" + RECOVERY_SCRIPT_FOR_WMI;
                //string encryptedPassword = Encrypt.Encrypts(password, true);
                commandForWMIRecopvery = string.Format(option, executeIP,  "\"" + directory + "\"");
                cmdOptions = cmdOptions + "  " + commandForWMIRecopvery;
                string command = Perlbinary;

                // Debug.WriteLine("Executing below command command " + command + " " + cmdOptions);
                Trace.WriteLine(DateTime.Now + " \t Exiting: GenerateVmxFile() with IP: " + executeIP + " power on and directory " + directory);
                return WinTools.Execute(command, cmdOptions, 60000000);
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return 1;

            }

        }

        public int WmiRecovery(string executeIP, string directory, string cxPath,OperationType operation)
        {
            try
            {
                string option = "";
                string cmdOptions = null, xmlFile = null;
                string commandForWMIRecopvery = null;
                switch (operation)
                {
                    case OperationType.Recover:
                        cmdOptions = RECOVERY_SCRIPT_FOR_WMI;
                        option = WMI_BASED_RECOVERY_COMMAND;
                        break;
                    case OperationType.Drdrill:
                        cmdOptions = RECOVERY_SCRIPT_FOR_WMI;
                        option = WMI_BASED_DRDRILL_COMMAND;
                        break;
                }
                cmdOptions = "\"" + scriptsPath + "\"" + "\\" + RECOVERY_SCRIPT_FOR_WMI;
               
                commandForWMIRecopvery = string.Format(option, executeIP, "\"" + directory + "\"", "\"" + cxPath + "\"");
                cmdOptions = cmdOptions + "  " + commandForWMIRecopvery;
                string command = Perlbinary;

                // Debug.WriteLine("Executing below command command " + command + " " + cmdOptions);
                Trace.WriteLine(DateTime.Now + " \t Exiting: GenerateVmxFile() with IP: " + executeIP  + " power on and directory " + directory);
                return WinTools.Execute(command, cmdOptions, 60000000);
            }
            catch (Exception ex)
            {

                System.Diagnostics.StackTrace trace = new System.Diagnostics.StackTrace(ex, true);

                Trace.WriteLine("ERROR*******************************************");
                Trace.WriteLine(DateTime.Now + "Caught exception at Method:  " + trace.GetFrame(0).GetMethod().Name + " Line: " + trace.GetFrame(0).GetFileLineNumber() + " Column: " + trace.GetFrame(0).GetFileColumnNumber());
                Trace.WriteLine("Exception  =  " + ex.Message);
                Trace.WriteLine("ERROR___________________________________________");
                return 1;

            }

        }

        public int ExecuteEsxutilWin(string target, string directory)
        { // This is to set the protection fx jobs..
            //Trace.WriteLine(DateTime.Now + " \t Entered: ExecuteJobAutomationScript() of ESX.cs with targetESXIP Operation = " + operation.ToString());

            string cmdOptions = null;
            string command = null;
            string option = "";           
            option = Esxutilwinfortarget;
            //Add quotest at the begining and end of the path so that spaces won't cause issue
            cmdOptions = ESX_UTIL_WIN;
            command = string.Format(option, target, "\"" + directory + "\"");
            

            cmdOptions = cmdOptions + "  " + command;

            string commands = "";

            // Debug.WriteLine("Executing below command " + command + " " + cmdOptions);
            Trace.WriteLine(DateTime.Now + " \t Exiting: ExecuteEsxutilWin() of ESX.cs with targetESXIP ");

            return WinTools.Execute(commands, cmdOptions, Perltimeout);




        }


    }
}

