using System;
using System.Collections;
using System.Text;
using System.Diagnostics;
using System.Xml;
using System.Windows.Forms;
using System.IO;
using System.Runtime.Serialization.Formatters.Binary;
using Com.Inmage.Esxcalls;
using Com.Inmage.Tools;
using System.Text.RegularExpressions;





namespace Com.Inmage
{
    public enum Hosttype { Vmwareguest=0, Physical, Xenguest, Amazonguest };
    public enum Role { Primary = 0, Secondary };
    public enum OStype { Windows = 0, Linux =1, Solaris = 2 };
  
    [Serializable]
    public class Host : ICloneable
    {//Error Codes Range: 01_005_

        public string hostname = null;
        public string displayname = null;
        public string ip = null;
        public string[] IPlist;
        public int ipCount = 0;
        public string credentialName;
        public string domain = null;
        public string userName = null;
        public string passWord = null;
        public string osEdition;
        public bool useDomainAdmin = false;
        public bool pushAgent = false;
        public bool rebootAfterPush = false;
        public Hosttype hostType;
        public OStype os;
        public string operatingSystem;
        public Role role = Role.Primary;
        public int numOfDisks;
        public DiskList disks;
        public ArrayList scsiAdapterList;
        public ArrayList vmScsiDiskList;
        public Hashtable vmdkHash;
        public string vmxFile;
        public string resourcePool = null;
        public string datastore = "";
        public string snapshot = "";
        public string folder_path = "";
        public string vmx_path = "";
        public int number_of_disks = 0;
        public bool collectedDetails = false;
        public string poweredStatus;
        public string vmwareTools;
        public Hashtable diskHash;
        public Hashtable nicHash;
        public Hashtable partitionHash;
        public string esxIp = null;
        public string esxUserName;
        public string esxPassword;
        public string retentionPath;
        public string retention_drive = null;
        public string destiNationEsxIp;
        public string destinationMasterIp;
        public string _installPath;
        public string subNetMask = null;
        public string defaultGateWay = null;
        public string dns = null;
        public string tag;
        public string tagType;
        public float totalSizeOfDisks = 0;
        public ArrayList dataStoreList;
        public ArrayList lunList;
        public int recoveryOrder = 0;
        public string retentionLogSize = null;
        public string retentionInDays = null;
        //public string consistency;
        public string consistencyInDays;
        public string consistencyInHours;
        public string consistencyInmins = null;
        public string rdmpDisk = "FALSE";
        public bool convertRdmpDiskToVmdk = true;
        public string processServerIp;
        public string targetDataStore = null;
        public string plan;
        public string masterTargetDisplayName;
        public string masterTargetHostName;
        public string targetESXIp;
        public string machineType;
        public string failOver = "";
        public string newIP = null;
        public string failback = "no";
        public bool pointedToCx = false;
        public bool fxagentInstalled = false;
        public bool vxagentInstalled = false;
        public bool vxlicensed = false;
        public bool fxlicensed = false;
        public bool fx_agent_heartbeat = false;
        public bool vx_agent_heartbeat = false;

        public string fx_agent_path = null;
        public string vx_agent_path = null;

        public bool ranPrecheck = false;
        public bool checkedCredentials = false;
        public bool rdmSelected = false;
        public bool rdmdisk = false;
        public bool skipPushAndPreReqs = false;
        public bool delete = false;
        public bool incrementalDisk = false;
        public bool commonTimeAvaiable = false;
        public bool commonSpecificTimeAvaiable = false;
        public bool commonTagAvaiable = false;
        public bool commonUserDefinedTimeAvailable = false;
        public int recoveryOperation = 0;
        public string userDefinedTime = null;

        public bool recoveryPrereqsPassed = false;
        public bool protectionPreReqsPassed = false;
        public string cluster = "no";
        public bool selectedDisk = false;
        public string ide_count = "1";
        public string natIPAddress = null;
        public bool containsNatIPAddress = false;
        public bool isSourceNatted = false;
        public bool isTargetNatted = false;
        public int batchResync = 0;
        public string cxIP = null;
        public string portNumber = null;
        public int alreadyP2VDisksProtected = 0;
        public bool sourceIsThere = false;
        public bool unInstall = false;
        public string directoryName = null;
        public bool machineInUse = false;
        public Nic nicList;
        public bool keepNicOldValues = true;
        //public ArrayList networkList;
        public ArrayList networkNameList;
        public ArrayList configurationList;
        public ArrayList resourcePoolList;
        public int cpuCount = 0;
        public int memSize = 0;
        public string source_uuid = null;
        public string target_uuid = null;
        public string mt_uuid = null;
        public string vConName = null;
        public string targetvSphereHostName = null;
        public string targetESXUserName = null;
        public string targetESXPassword = null;
        public string timeZone = null;
        public string selectedTimeByUser = null;
        public string vSpherehost = null;
        public string hostversion = null;
        public string datacenter = null;
        public string vmxversion = null;
        public string vCenterProtection = "No";
        public string floppy_device_count = null;
        public string alt_guest_name = null;
        public bool thinProvisioning = true;
        public string resourcepoolgrpname = null;
        public string new_displayname = null;
        public int numberOfEntriesInCX = 1;
        public bool offlineSync = false;
        public bool local_backup = false;
        public string vConversion = null;
        public string vmDirectoryPath = null;
        public string oldVMDirectory = null;
        public string inmage_hostid = null;
        public bool vSan = false;
        public string vsan_folder = null;
        public string old_vm_folder_path = null;
        public string mbr_path = null;
        public string system_volume = null;
        public string cpuVersion = null;
        public string requestId = null;
        public bool cxbasedCompression = false;
        public bool sourceBasedCompression = false;
        public bool noCompression = false;
        public bool secure_src_to_ps = true;
        public bool secure_ps_to_tgt = true;
        //public string disk_type ;
        public string unitsOfWindow = "days";
        public int unitsofTimeIntervelWindow = 0;
        public int unitsofTimeIntervelWindow1Value = 0;
        public int unitsofTimeIntervelWindow2Value = 0;
        public int unitsofTimeIntervelWindow3Value = 0;
        public int bookMarkCountWindow1 = 0;
        public int bookMarkCountWindow2 = 0;
        public int bookMarkCountWindow3 = 0;
        public int window1Range = 0;
        public bool selectedSparce = false;
        public bool sparceDaysSelected = false;
        public bool sparceWeeksSelected = false;
        public bool sparceMonthsSelected = false;
        public bool efi = false;
        public string planid = null;
        public bool runEsxUtilCommand = false;
        public string commandtoExecute = null;
        public string perlCommand = null;
        public string initializingProtectionPlan = null;
        public string downloadingConfigurationfiles = null;
        public string preparingMasterTargetDisks = null;
        public string updatingDiskLayouts = null;
        public string activateProtectionPlan = null;
        public string initializingRecoveryPlan = null;
        public string initializingPhysicalSnapShotOperation = null;
        public string startingPhysicalSnapShotforSelectedVMs = null;
        public string initializingDRDrillPlan = null;
        public string powerOnVMs = null;
        public string srcDir = null;
        public string tardir = null;
        public string startingRecoveryForSelectedVms = null;
        public string resourcepoolgrpname_src = null;
        public string resourcePool_src = null;
        public Hashtable taskHash;
        public ArrayList taskList;
        public OperationType operationType = OperationType.Initialprotection;
        public bool recoveryLater = false;
        public string diskInfo = null;
        public string sourceDataStoreForDrdrill = null;
        public bool unifiedAgent = false;
        public string unifiedagentVersion = null;
        public string cacheDir = null;
        public bool enableLogging = false;
        public string iptoPush = null;
        public string natip = null;
        public int retentionInHours = 0;
        public bool powerOnVM = true;
        public string diskenableuuid = "na";


        public string initializingResizePlan = null;
        public string resizedownloadfiles = null;
        public string pauseReplicationPairs = null;
        public string resizePreParingMasterTargetDisks = null;
        public string resizeUpdateDiskLayouts = null;
        public string resumeReplicationPairs = null;

        public string initializingRemoveDiskPlan = null;
        public string downloadRemoveDiskFiles = null;
        public string deleteReplicationPairs = null;
        public string removeDisksFromMt = null;
        public string clusterName = null;
        public string clusterNodes = null;
        public string clusterInmageUUIds = null;
        public string clusterMBRFiles = null;

        public bool resizedOrPaused = false;
        public bool https = false;
        public string servicesToDisable = null;
        public bool masterTarget = false;
        public bool template = false;
        public string fxAgentHeartBeat = null;
        public string vxAgentHeartBeat = null;
        public bool restartResync = false;
        public string recoveryPlanId = null;
        public string agentFirstReportedTime = null;
        public Host()
        {

            disks               =       new DiskList();
            _installPath        = WinTools.FxAgentPath() + "\\vContinuum";
            dataStoreList       = new ArrayList();
            lunList             = new ArrayList();
            nicList = new Nic();
            //networkList = new ArrayList();
            configurationList = new ArrayList();
            networkNameList = new ArrayList();
            resourcePoolList = new ArrayList();
            taskHash = new Hashtable();
            taskList = new ArrayList();
        
        }





        internal bool CollectedDetails()
        {
            return collectedDetails;
        }

        // deep copy in separeate memory space

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
        public bool SelectDisk(int diskNumber)
        {
            Trace.WriteLine(DateTime.Now+ " \t Reaches to add the disk to the host");
            return disks.SelectDisk(diskNumber);
        }

        public bool SelectDiskForRemove(int diskNumber)
        {
            Trace.WriteLine(DateTime.Now + " \t Reaches to add the disk to the host");
            return disks.SelectDiskForRemove(diskNumber);
        }

        public bool UnselectDisk(int diskNumber)
        {
            Trace.WriteLine(DateTime.Now+ " \t Reaches to remove the disk to the host");
            return disks.UnselectDisk( diskNumber );
        }

        public bool UnselectDiskForRemove(int diskNumber)
        {
            Trace.WriteLine(DateTime.Now + " \t Reaches to remove the disk to the host");
            return disks.UnselectDiskForRemove(diskNumber);
        }


        public static string GetTitle(string file)
        {
            
                Match m = Regex.Match(file, @"<title>\s*(.+?)\s*</title>");
                if (m.Success)
                {
                    return m.Groups[1].Value;
                }
                else
                {
                    try
                    {
                        m = Regex.Match(file, @"(?<=<TITLE.*>)([\s\S]*)(?=</TITLE>)");
                    }
                    catch (Exception ex)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Failed to check the title " + ex.Message);
                    }
                     if (m.Success)
                     {
                         return m.Groups[1].Value;
                     }
                     else
                     {
                         return "";
                     }
                }
            
            
        }


        public bool CopyDriversFromOScd(string selectedPath, string operatingSystem)
        {
            try
            {

                string tempDirPath = "";
                string destinationDir = "";

                Array readmeFileList = Directory.GetFiles(selectedPath, "README.htm", SearchOption.AllDirectories);

              
                if (readmeFileList.Length <= 0)
                {
                   // readmeFileList = Directory.GetFiles(selectedPath, "READ1ST.HTM", SearchOption.AllDirectories);
                   // if (readmeFileList.Length <= 0)
                    {
                        Trace.WriteLine(DateTime.Now + "\t\tUnable to find README.htm file in browsed location. Hence exiting...");
                        MessageBox.Show("Could not find the OS type. Please provide a Windows CD");
                        return false;
                    }
                }


                bool correctReadMeFile = false;
                foreach (string file in readmeFileList)
                {

                    Trace.WriteLine(DateTime.Now + "\t file path " + file);
                    //Console.WriteLine(file);

                    string html = File.ReadAllText(file);
                    Console.WriteLine(GetTitle(html));


                    string osTitle = GetTitle(html);
                    if (osTitle != null)
                    {

                        if (osTitle.Contains("Windows Server 2003, Enterprise x64 Edition"))
                        {
                            tempDirPath = @"c:\windows\temp\win2k3_64\";
                            destinationDir = "Windows_2003_64";

                            correctReadMeFile = true;

                        }
                        else if (osTitle.Contains("Windows Server 2003, Enterprise Edition"))
                        {
                            tempDirPath = @"C:\windows\temp\win2k3_32\";
                            destinationDir = "Windows_2003_32";
                            correctReadMeFile = true;
                        }
                        else if (osTitle.Contains("Windows Server 2003, Standard x64 Edition"))
                        {
                            tempDirPath = @"c:\windows\temp\win2k3_64\";
                            destinationDir = "Windows_2003_64";
                            correctReadMeFile = true;
                        }
                        else if (osTitle.Contains("Windows Server 2003, Standard Edition"))
                        {
                            tempDirPath = @"C:\windows\temp\win2k3_32\";
                            destinationDir = "Windows_2003_32";
                            correctReadMeFile = true;
                        }
                        else if (osTitle.Contains("2003"))
                        {
                            if (osTitle.Contains("64"))
                            {
                                tempDirPath = @"c:\windows\temp\win2k3_64\";
                                destinationDir = "Windows_2003_64";
                                correctReadMeFile = true;
                            }
                            else
                            {
                                tempDirPath = @"C:\windows\temp\win2k3_32\";
                                destinationDir = "Windows_2003_32";
                                correctReadMeFile = true;
                            }
                        }
                        else if (osTitle.ToUpper().Contains("XP"))
                        {
                            Trace.WriteLine(DateTime.Now + "\t XP ");
                            if (osTitle.Contains("64"))
                            {
                                Trace.WriteLine(DateTime.Now + "\t XP  64 bit");
                                tempDirPath = @"c:\windows\temp\Windows_XP_64\";
                                destinationDir = "Windows_XP_64";
                                correctReadMeFile = true;
                            }
                            else
                            {
                                Trace.WriteLine(DateTime.Now + "\t XP  32 bit");
                                tempDirPath = @"C:\windows\temp\Windows_XP_32\";
                                destinationDir = "Windows_XP_32";
                                correctReadMeFile = true;
                            }
                        }
                    }
                    /*if(GetTitle(html).Contains("Windows Server 2003") && (GetTitle(html).Contains("x64 Edition")))
                    {
                        tempDirPath = @"c:\windows\temp\win2k3_64\";
                        destinationDir = "Windows_2003_64";
                        correctReadMeFile = true;

                    }*/
                    
                }
                if (operatingSystem != destinationDir)
                {
                    MessageBox.Show("Please insert right OS CD of " + operatingSystem);

                }

                Debug.WriteLine(DateTime.Now + " \t Value of the vl=alue of the correct read me file " + correctReadMeFile);
                if (correctReadMeFile)
                    CopyExtractDrivers(tempDirPath, destinationDir, selectedPath);
                else
                    MessageBox.Show("Could not find needed driver files in the cd. Please try again.");

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



        public bool CopyExtractDrivers(string tempDirectoryPath, string destinationDir, string selectedPath)
        {

            bool driversNotFound = true;

            try
            {
                DirectoryInfo directoryInof = new System.IO.DirectoryInfo(tempDirectoryPath);
                if (!directoryInof.Exists)
                {

                    Directory.CreateDirectory(tempDirectoryPath);
                }

                Array filePaths = Directory.GetFiles(selectedPath, "symmpi.sy_", SearchOption.AllDirectories);

                if (filePaths.Length <= 0)
                {
                    try
                    {
                        filePaths = Directory.GetFiles(selectedPath, "SYMMPI.SYS", SearchOption.AllDirectories);
                    }
                    catch (Exception ex)
                    {
                        Trace.WriteLine(DateTime.Now + "\t Failed to find symmpi.sys " + ex.Message);
                    }
                    if (filePaths.Length <= 0)
                    {
                        MessageBox.Show("Could not find the OS type. Please provide a Windows CD");
                        return false;
                    }
                }


                foreach (string fileName in filePaths)
                {
                    //string fileName = filePaths
                    string newfile = "symmpi.sy_";
                    // allServersForm.openFileDialog.FileName = fileName;
                    //openFileDialog.ShowDialog();
                    Console.WriteLine("+++++++++++++++++++++++++ file name" + fileName);


                    if (File.Exists(tempDirectoryPath + newfile))
                    {
                        File.SetAttributes(tempDirectoryPath + newfile, FileAttributes.Normal);
                        File.Delete(tempDirectoryPath + newfile);

                    }

                    File.Copy(fileName, tempDirectoryPath + newfile, true);

                    WinTools.ExpandFile(tempDirectoryPath + "symmpi.sy_ ", tempDirectoryPath + "\\symmpi.sys");

                    DirectoryInfo directory = new DirectoryInfo(_installPath + "\\windows");

                    if (!Directory.Exists(_installPath + "\\windows" + "\\" + destinationDir))
                    {

                        Directory.CreateDirectory(_installPath + "\\windows" + "\\" + destinationDir);
                        // directory.CreateSubdirectory("Win2k3_32");
                    }

                    if (File.Exists(tempDirectoryPath + "\\symmpi.sys"))
                    {
                        File.Copy(tempDirectoryPath + "\\symmpi.sys", _installPath + "\\windows" + "\\" + destinationDir + "\\symmpi.sys", true);
                        driversNotFound = false;
                        break;
                    }
                    else
                    {
                        driversNotFound = true;
                        //MessageBox.Show("Could not find needed driver files in the cd. Please try again.");
                    }
                }

                if (driversNotFound == true)
                {
                    MessageBox.Show("Could not find needed driver files in the cd. Please try again.");
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

        public void Print()
        {
            try
            {
                Trace.WriteLine("*********************");
                Trace.WriteLine("\t HostName =  " + hostname + "OS = " + operatingSystem + "DisplayName = " + displayname + "power_status = " + poweredStatus + " Vmware toolds  = " + vmwareTools + "targetDisplayName " + masterTargetDisplayName);
                Trace.WriteLine("hostType = " + hostType + " role =  " + role + " numOfDisks = " + numOfDisks + "CredentialName = " + credentialName + " Total Size = " + totalSizeOfDisks + "Push Agent = " + pushAgent);
                Trace.WriteLine("\t Domain =  " + domain + " UserName = " + userName);
                Trace.WriteLine("\t vmx_path = " + vmx_path + " os_type = " + os + " resourcePool =  " + resourcePool + " number of disks = " + number_of_disks + " rdmp disks = " + rdmpDisk);

                Trace.WriteLine("\t EsxIP = " + esxIp + " EsxUserName  = " + esxUserName + " RetentionPath = " + retentionPath + " folder Path = " + folder_path);
                Trace.WriteLine("\t DefaultGateWay = " + defaultGateWay + "Subnet Mask = " + subNetMask + " dns = " + dns + " Tag Type = " + tagType + " Tag = " + tag + "datastore = " + datastore);
                Trace.WriteLine("\t RetentionPath = " + retentionPath + "RetentionLogSize = " + retentionLogSize + "retention = " + retentionInDays);
                Trace.WriteLine(DateTime.Now + "\t ip " + ip + " vspherehost " + vSpherehost);





                if (vmScsiDiskList != null)
                {
                    foreach (string scsiDisk in vmScsiDiskList)
                    {
                        Trace.WriteLine("SCSI:{0}", scsiDisk);
                    }
                }



                if (disks.list != null)
                {

                    foreach (Hashtable valHash in disks.list)
                    {
                        Trace.WriteLine("Name = " + valHash["Name"]);
                        Trace.WriteLine("Size = " + valHash["Size"]);
                        Trace.WriteLine("Selected = " + valHash["Selected"]);

                        // totalSizeOfDisks = totalSizeOfDisks + int.Parse(valHash["Size"].ToString());
                        /*
                        foreach (string key in valHash.Keys)
                        {
                            Trace.WriteLine("\t" + key + "\t=\t" + valHash[key]);
                        }
                         */
                    }
                }
                else
                {
                    Debug.WriteLine(DateTime.Now + " \t Disk list is empty");
                }


                if (disks.partitionList != null)
                {
                    foreach (Hashtable hash in disks.partitionList)
                    {
                        Trace.WriteLine(DateTime.Now + " \t Volume Name is:" + hash["Name"]);
                        Trace.WriteLine(DateTime.Now + " \t Volume Free size is:" + hash["FreeSpace"]);
                    }




                }
                else
                {
                    Trace.WriteLine(DateTime.Now + " \t ********************partition Hash is Empty**************");

                }

                if (vmdkHash != null)
                {
                    foreach (string diskName in vmdkHash.Keys)
                    {

                        Trace.WriteLine(DateTime.Now + " \t  \t \t VMDK : " + diskName + "  Size =  " + vmdkHash[diskName]);

                    }
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
                

            }
            Trace.WriteLine(DateTime.Now + " \t *********************");


        }



        internal bool WriteXmlNode(XmlWriter writer)
        {

            try
            {

                if (writer == null)
                {
                    Trace.WriteLine(DateTime.Now + " \t Host:WriteXmlNode: Got a null writer  value ");
                    MessageBox.Show("Host:WriteXmlNode: Got a null writer  value ", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return false;

                }
                number_of_disks = disks.list.Count;
                writer.WriteStartElement("host");
                writer.WriteAttributeString("ip_address", ip);
                writer.WriteAttributeString("hostname", hostname);
                writer.WriteAttributeString("display_name", displayname);
                writer.WriteAttributeString("new_displayname", new_displayname);
                if (vmxversion != null)
                {
                    writer.WriteAttributeString("vmx_version", vmxversion);
                }
                else
                {
                    writer.WriteAttributeString("vmx_version", "");
                }
                if (floppy_device_count != null)
                {
                    writer.WriteAttributeString("floppy_device_count", floppy_device_count);
                }
                else
                {
                    writer.WriteAttributeString("floppy_device_count", "");
                }
                if (alt_guest_name != null)
                {
                    writer.WriteAttributeString("alt_guest_name", alt_guest_name);
                }
                else
                {
                    writer.WriteAttributeString("alt_guest_name", "");
                }
                if (resourcepoolgrpname != null)
                {
                    writer.WriteAttributeString("resourcepoolgrpname", resourcepoolgrpname);
                }
                else
                {
                    writer.WriteAttributeString("resourcepoolgrpname", "");
                }
                if (resourcepoolgrpname_src != null)
                {
                    writer.WriteAttributeString("resourcepoolgrpname_src", resourcepoolgrpname_src);
                }
                else
                {
                    writer.WriteAttributeString("resourcepoolgrpname_src", "");
                }
                if (resourcePool_src != null)
                {
                    writer.WriteAttributeString("resourcePool_src", resourcePool_src);
                }
                else
                {
                    writer.WriteAttributeString("resourcePool_src", "");
                }
                writer.WriteAttributeString("ide_count", ide_count);
                writer.WriteAttributeString("hostversion", hostversion);
                writer.WriteAttributeString("vSpherehostname", vSpherehost);
                writer.WriteAttributeString("machinetype", machineType);
                writer.WriteAttributeString("folder_path", folder_path);
                writer.WriteAttributeString("vmx_path", vmx_path);
                if (sourceBasedCompression == true)
                {
                    writer.WriteAttributeString("compression", "source_base");
                }
                else if (noCompression == true)
                {
                    writer.WriteAttributeString("compression", "");
                }
                else
                {
                    writer.WriteAttributeString("compression", "cx_base");
                }

                writer.WriteAttributeString("secure_src_to_ps", secure_src_to_ps.ToString());
                writer.WriteAttributeString("secure_ps_to_tgt", secure_ps_to_tgt.ToString());
                if (cpuCount != 0)
                {
                    writer.WriteAttributeString("cpucount", cpuCount.ToString());
                }
                else
                {
                    writer.WriteAttributeString("cpucount", "1");
                }
                if (memSize <= 4)
                {
                    writer.WriteAttributeString("memsize", "512");
                }
                else
                {
                    writer.WriteAttributeString("memsize", memSize.ToString());
                }
                writer.WriteAttributeString("resourcepool", resourcePool);
                writer.WriteAttributeString("powered_status", poweredStatus);
                writer.WriteAttributeString("role", role.ToString());
                writer.WriteAttributeString("number_of_disks", number_of_disks.ToString());
                writer.WriteAttributeString("plan", plan);
                writer.WriteAttributeString("esxIp", esxIp);
                writer.WriteAttributeString("mastertargetdisplayname", masterTargetDisplayName);
                writer.WriteAttributeString("mastertargethostname", masterTargetHostName);
                writer.WriteAttributeString("targetesxip", targetESXIp);
                // We added this because of when remove operation is done we are writing defaulty no value
                // If already user recoverd vm also we are calling the detach script. there it is detaching the existing disks.
                if (SolutionConfig.Isremovexml == true)
                {
                    writer.WriteAttributeString("failover", failOver);
                }
                else
                {
                    writer.WriteAttributeString("failover", "no");
                }
                writer.WriteAttributeString("failback", failback);
                writer.WriteAttributeString("diskenableuuid", diskenableuuid);
                writer.WriteAttributeString("os_info", os.ToString());
                writer.WriteAttributeString("operatingsystem", operatingSystem);
                writer.WriteAttributeString("efi", efi.ToString());
                writer.WriteAttributeString("convert_rdm_to_vmdk", convertRdmpDiskToVmdk.ToString());
                writer.WriteAttributeString("rdm", rdmpDisk);
                writer.WriteAttributeString("cluster", cluster.ToString());
                writer.WriteAttributeString("directoryName", "null");
                writer.WriteAttributeString("cxIP", WinTools.GetcxIP());
                writer.WriteAttributeString("datastore", datastore);
                writer.WriteAttributeString("targetvSphereHostName", targetvSphereHostName);
                writer.WriteAttributeString("offlineSync", offlineSync.ToString());
                writer.WriteAttributeString("keepNicOldValues", keepNicOldValues.ToString());
                writer.WriteAttributeString("recoveryOrder", recoveryOrder.ToString());
                writer.WriteAttributeString("tag", tag);
                writer.WriteAttributeString("tagType", tagType);
                writer.WriteAttributeString("source_uuid", source_uuid);
                writer.WriteAttributeString("target_uuid", target_uuid);
                writer.WriteAttributeString("mt_uuid", mt_uuid);
                writer.WriteAttributeString("local_backup", local_backup.ToString());
                writer.WriteAttributeString("selectedSparce", selectedSparce.ToString());
                writer.WriteAttributeString("sparceDaysSelected", sparceDaysSelected.ToString());
                writer.WriteAttributeString("sparceWeeksSelected", sparceWeeksSelected.ToString());
                writer.WriteAttributeString("sparceMonthsSelected", sparceMonthsSelected.ToString());
                string versionNumber = WinTools.VersionNumber();
                writer.WriteAttributeString("vConversion", versionNumber.Substring(0, 3));
                if (vConName != null)
                {
                    //Trace.WriteLine(DateTime.Now + "\t came here beacuse vConname is not empty  " + vConName);
                    writer.WriteAttributeString("vconname", vConName);
                }
                else
                {
                    Trace.WriteLine(DateTime.Now + "\t came here beacuse vConname is empty");
                    writer.WriteAttributeString("vconname", Environment.MachineName);
                }
                writer.WriteAttributeString("vsan", vSan.ToString());
                if (vsan_folder != null)
                {
                    writer.WriteAttributeString("vsan_folder", vsan_folder);
                }
                if (old_vm_folder_path != null)
                {
                    writer.WriteAttributeString("old_vm_folder_path", old_vm_folder_path);
                }
                writer.WriteAttributeString("mbr_path", mbr_path);
                writer.WriteAttributeString("system_volume", system_volume);
                writer.WriteAttributeString("inmage_hostid", inmage_hostid);
                writer.WriteAttributeString("timezone", timeZone);
                writer.WriteAttributeString("isSourceNatted", isSourceNatted.ToString());
                writer.WriteAttributeString("isTargetNatted", isTargetNatted.ToString());
                writer.WriteAttributeString("batchresync", batchResync.ToString());
                writer.WriteAttributeString("datacenter", datacenter);
                writer.WriteAttributeString("vCenterProtection", vCenterProtection);
                writer.WriteAttributeString("vmDirectoryPath", vmDirectoryPath);
                writer.WriteAttributeString("power_on_vm", powerOnVM.ToString());
                 writer.WriteAttributeString("clusternodes_inmageguids", clusterInmageUUIds);
                writer.WriteAttributeString("cluster_name", clusterName);
                writer.WriteAttributeString("clusternodes_mbrfiles", clusterMBRFiles);
                writer.WriteAttributeString("cluster_nodes",clusterNodes);
                writer.WriteAttributeString("recovery_plan_id", recoveryPlanId);
                string service = null;
                service = WinTools.ServicesToManual();
                if (service != null)
                {
                    writer.WriteAttributeString("servicestomanual", service);
                    Trace.WriteLine(DateTime.Now + "\t Service " + service);
                }
                else
                {
                    Trace.WriteLine(DateTime.Now + "\t Service to manaul null" );
                }
                //writer.WriteAttributeString("rdm", rdmpDisk.ToString());
                if (disks.list != null)
                {
                    // totalSizeOfDisks = 0;
                    Trace.WriteLine(DateTime.Now + "\t Count of disks " + disks.list.Count.ToString());
                    foreach (Hashtable disk in disks.list)
                    {
                        if (disk["Selected"].ToString() == "Yes" || (disk["Protected"] != null && disk["Protected"].ToString() == "yes"))
                        {
                            Debug.WriteLine(DateTime.Now + " \t Writing for disk " + disk["Name"].ToString());
                            writer.WriteStartElement("disk");
                            writer.WriteAttributeString("disk_name", disk["Name"].ToString());
                            if (disk["src_devicename"] != null)
                            {
                                writer.WriteAttributeString("src_devicename", disk["src_devicename"].ToString());
                            }
                            writer.WriteAttributeString("size", disk["Size"].ToString());
                            writer.WriteAttributeString("disk_type", disk["disk_type"].ToString());
                            if (disk["disk_mode"] != null)
                            {
                                writer.WriteAttributeString("disk_mode", disk["disk_mode"].ToString());
                            }
                            else
                            {
                                writer.WriteAttributeString("disk_mode", "");
                            }

                            if (disk["DiskScsiId"] != null)
                            {
                                writer.WriteAttributeString("disk_scsi_id", disk["DiskScsiId"].ToString());
                            }
                            if (disk["volumelist"] != null)
                            {
                                writer.WriteAttributeString("volumelist", disk["volumelist"].ToString());
                                try
                                {
                                    string[] count = disk["volumelist"].ToString().Split(',');
                                    int volumecount = count.Length;
                                    volumecount= volumecount + 1;
                                    writer.WriteAttributeString("volume_count", volumecount.ToString());
                                }
                                catch (Exception ex)
                                {
                                    Trace.WriteLine(DateTime.Now + "\t Failed to check count " + ex.Message);
                                }
                            }
                            if (disk["disk_type_os"] != null)
                            {
                                writer.WriteAttributeString("disk_type_os", disk["disk_type_os"].ToString());
                            }

                            if (disk["DiskLayout"] != null)
                            {
                                writer.WriteAttributeString("disk_layout", disk["DiskLayout"].ToString());
                            }
                            if (disk["controller_type"] != null)
                            {
                                writer.WriteAttributeString("controller_type", disk["controller_type"].ToString());
                            }
                            else
                            {
                                if (operatingSystem != null && (machineType != null && machineType == "VirtualMachine"))
                                {
                                    if (operatingSystem.Contains("2003"))
                                    {
                                        writer.WriteAttributeString("controller_type", "VirtualLsiLogicController");
                                    }
                                    else if (operatingSystem.Contains("2008"))
                                    {
                                        writer.WriteAttributeString("controller_type", "VirtualLsiLogicSASController");
                                    }
                                    else if (operatingSystem.Contains("2012"))
                                    {
                                        writer.WriteAttributeString("controller_type", "VirtualLsiLogicSASController");
                                    }
                                }
                                else
                                {
                                    if (cluster == "yes")
                                    {
                                        if (operatingSystem.Contains("2008"))
                                        {
                                            writer.WriteAttributeString("controller_type", "VirtualLsiLogicSASController");
                                        }
                                        else if (operatingSystem.Contains("2012"))
                                        {
                                            writer.WriteAttributeString("controller_type", "VirtualLsiLogicSASController");
                                        }
                                        else
                                        {
                                            writer.WriteAttributeString("controller_type", "VirtualLsiLogicController");
                                        }
                                    }
                                    else
                                    {
                                        if (operatingSystem.Contains("2012"))
                                        {
                                            writer.WriteAttributeString("controller_type", "VirtualLsiLogicSASController");
                                        }
                                        else
                                        {
                                            writer.WriteAttributeString("controller_type", "VirtualLsiLogicController");
                                        }
                                    }

                                }
                            }
							if (disk["selected_for_protection"] != null)
                            {
                                writer.WriteAttributeString("selected", disk["selected_for_protection"].ToString());
                            }

                            if (disk["controller_mode"] != null)
                            {
                                writer.WriteAttributeString("controller_mode", disk["controller_mode"].ToString());
                                Trace.WriteLine(DateTime.Now + "\t Writing controller_mode valeu " + disk["controller_mode"].ToString());
                            }
                            else
                            {
                                if (machineType.ToUpper() == "PHYSICALMACHINE")
                                {
                                    if (cluster == "yes")
                                    {
                                        writer.WriteAttributeString("controller_mode", "virtualSharing");
                                    }
                                    else
                                    {
                                        writer.WriteAttributeString("controller_mode", "noSharing");
                                    }
                                }
                                else
                                {
                                    writer.WriteAttributeString("controller_mode", "noSharing");
                                }
                            }
                            if (disk["cluster_disk"] != null)
                            {
                                writer.WriteAttributeString("cluster_disk", disk["cluster_disk"].ToString());

                            }
                            else
                            {
                                if (machineType.ToUpper() == "PHYSICALMACHINE")
                                {
                                    if (cluster == "yes")
                                    {
                                        writer.WriteAttributeString("cluster_disk", "yes");
                                    }
                                    else
                                    {
                                        writer.WriteAttributeString("cluster_disk", "no");
                                    }
                                }
                                else
                                {
                                    writer.WriteAttributeString("cluster_disk", "no");
                                }

                            }

                            if (disk["scsi_id_onmastertarget"] != null)
                            {
                                writer.WriteAttributeString("scsi_id_onmastertarget", disk["scsi_id_onmastertarget"].ToString());
                            }
                            if (disk["lun_name"] != null)
                            {
                                writer.WriteAttributeString("name", disk["lun_name"].ToString());
                                writer.WriteAttributeString("adapter", disk["adapter"].ToString());
                                writer.WriteAttributeString("lun", disk["lun"].ToString());
                                writer.WriteAttributeString("devicename", disk["devicename"].ToString());
                                writer.WriteAttributeString("capacity_in_kb", disk["capacity_in_kb"].ToString());
                            }
                            if (disk["ide_or_scsi"] != null)
                            {
                                writer.WriteAttributeString("ide_or_scsi", disk["ide_or_scsi"].ToString());
                            }
                            else
                            {
                                writer.WriteAttributeString("ide_or_scsi", "");
                            }
                            writer.WriteAttributeString("datastore_selected", targetDataStore);
                            if (disk["Protected"] != null)
                            {
                                writer.WriteAttributeString("protected", disk["Protected"].ToString());
                            }
                            else
                            {
                                writer.WriteAttributeString("protected", "no");
                            }
                            if (disk["scsi_mapping_host"] != null)
                            {
                                writer.WriteAttributeString("scsi_mapping_host", disk["scsi_mapping_host"].ToString());

                            }
                            else
                            {

                                if (os == OStype.Windows && machineType == "PhysicalMachine")
                                {
                                    writer.WriteAttributeString("scsi_mapping_host", "100:100:100:100");
                                }
                            }

                            if (disk["remove"] != null)
                            {
                                if (disk["remove"].ToString().ToUpper() == "TRUE")
                                {
                                    writer.WriteAttributeString("remove", "True");
                                }
                                else
                                {
                                    writer.WriteAttributeString("remove", "false");
                                }
                            }
                            else
                            {
                                writer.WriteAttributeString("remove", "false");
                            }


                            if (disk["scsi_mapping_vmx"] != null)
                            {
                                writer.WriteAttributeString("scsi_mapping_vmx", disk["scsi_mapping_vmx"].ToString());
                            }
                            if (disk["independent_persistent"] != null)
                            {
                                writer.WriteAttributeString("independent_persistent", disk["independent_persistent"].ToString());
                            }
                            else
                            {
                                writer.WriteAttributeString("independent_persistent", "persistent");

                            }
                            if (disk["source_disk_uuid"] != null)
                            {
                                writer.WriteAttributeString("source_disk_uuid", disk["source_disk_uuid"].ToString());
                            }
                            else
                            {
                                writer.WriteAttributeString("source_disk_uuid", null);
                            }
                            if (disk["target_disk_uuid"] != null)
                            {
                                writer.WriteAttributeString("target_disk_uuid", disk["target_disk_uuid"].ToString());
                            }
                            else
                            {
                                writer.WriteAttributeString("target_disk_uuid", null);
                            }
                            if (disk["SectorsPerTrack"] != null && disk["TotalHeads"] != null)
                            {
                                if (os == OStype.Windows)
                                {
                                    if (long.Parse(disk["Size"].ToString()) > 2097152 && (int.Parse(disk["TotalHeads"].ToString()) != 255 || long.Parse(disk["SectorsPerTrack"].ToString()) != 63))
                                    {
                                        writer.WriteAttributeString("sectors", disk["SectorsPerTrack"].ToString());

                                        // We have seen one issue with physical machien with total heads as 256. When they try to power on whole cluster got corrupted. So we are reducing to 255.
                                        if (int.Parse(disk["TotalHeads"].ToString()) > 255)
                                        {
                                            writer.WriteAttributeString("heads", "255");
                                        }
                                        else
                                        {
                                            writer.WriteAttributeString("heads", disk["TotalHeads"].ToString());
                                        }
                                        writer.WriteAttributeString("updatechs", "yes");
                                    }
                                    else
                                    {
                                        writer.WriteAttributeString("sectors", disk["SectorsPerTrack"].ToString());

                                        writer.WriteAttributeString("heads", disk["TotalHeads"].ToString());
                                        writer.WriteAttributeString("updatechs", "no");
                                    }
                                }
                            }

                            if (disk["media_type"] != null)
                            {
                                writer.WriteAttributeString("media_type", disk["media_type"].ToString());
                            }
                            if (disk["disk_signature"] != null)
                            {
                                writer.WriteAttributeString("disk_signature", disk["disk_signature"].ToString());
                            }
                            writer.WriteAttributeString("convert_rdm_to_vmdk", convertRdmpDiskToVmdk.ToString());
                            writer.WriteAttributeString("thin_disk", thinProvisioning.ToString());
                            // totalSizeOfDisks = totalSizeOfDisks + int.Parse(disk["Size"].ToString());
                            // writer.WriteAttributeString("Selected",disk["Selected"].ToString());
                            writer.WriteEndElement();
                        }
                    }

                    foreach (Hashtable nic in nicList.list)
                    {
                        if (nic["Selected"].ToString() == "yes")
                        {
                            writer.WriteStartElement("nic");

                            Trace.WriteLine(DateTime.Now + "\t This nic is selected for protection/Recovery/Drdrill");
                            if (nic["network_label"] != null)
                            {
                                writer.WriteAttributeString("network_label", nic["network_label"].ToString());

                            }
                            else
                            {
                                writer.WriteAttributeString("network_label", null);
                            }
                            if (nic["network_name"] != null)
                            {
                                writer.WriteAttributeString("network_name", nic["network_name"].ToString());
                            }
                            else
                            {
                                writer.WriteAttributeString("network_name", "VM Network");
                            }
                            if (nic["nic_mac"] != null)
                            {
                                writer.WriteAttributeString("nic_mac", nic["nic_mac"].ToString());
                            }
                            else
                            {
                                writer.WriteAttributeString("nic_mac", null);
                            }
                            if (nic["ip"] != null)
                            {
                                writer.WriteAttributeString("ip", nic["ip"].ToString());
                            }
                            else
                            {
                                writer.WriteAttributeString("ip", null);
                            }
                            if (nic["dhcp"] != null)
                            {
                                writer.WriteAttributeString("dhcp", nic["dhcp"].ToString());
                            }
                            else
                            {
                                writer.WriteAttributeString("dhcp", "0");
                            }
                            if (nic["mask"] != null)
                            {
                                writer.WriteAttributeString("mask", nic["mask"].ToString());
                            }
                            else
                            {
                                writer.WriteAttributeString("mask", null);
                            }
                            if (nic["gateway"] != null)
                            {
                                writer.WriteAttributeString("gateway", nic["gateway"].ToString());
                            }
                            else
                            {
                                writer.WriteAttributeString("gateway", null);
                            }
                            if (nic["dnsip"] != null)
                            {
                                writer.WriteAttributeString("dnsip", nic["dnsip"].ToString());
                            }
                            else
                            {
                                writer.WriteAttributeString("dnsip", null);

                            }
                            if (nic["address_type"] != null)
                            {
                                writer.WriteAttributeString("address_type", nic["address_type"].ToString());

                            }
                            else
                            {
                                writer.WriteAttributeString("address_type", "generated");
                            }
                            if (nic["adapter_type"] != null)
                            {
                                writer.WriteAttributeString("adapter_type", nic["adapter_type"].ToString());

                            }
                            else
                            {
                                writer.WriteAttributeString("adapter_type", "VirtualE1000");
                            }
                            if (nic["new_ip"] != null)
                            {
                                writer.WriteAttributeString("new_ip", nic["new_ip"].ToString());

                            }
                            else
                            {
                                writer.WriteAttributeString("new_ip", null);
                            }
                            if (nic["new_mask"] != null)
                            {
                                writer.WriteAttributeString("new_mask", nic["new_mask"].ToString());
                            }
                            else
                            {
                                writer.WriteAttributeString("new_mask", null);
                            }
                            if (nic["new_dnsip"] != null)
                            {
                                writer.WriteAttributeString("new_dnsip", nic["new_dnsip"].ToString());
                            }
                            else
                            {
                                writer.WriteAttributeString("new_dnsip", null);
                            }
                            if (nic["new_gateway"] != null)
                            {
                                writer.WriteAttributeString("new_gateway", nic["new_gateway"].ToString());
                            }
                            else
                            {
                                writer.WriteAttributeString("new_gateway", null);
                            }
                            if (nic["new_network_name"] != null)
                            {
                                writer.WriteAttributeString("new_network_name", nic["new_network_name"].ToString());
                            }
                            else
                            {
                                writer.WriteAttributeString("new_network_name", null);
                            }
                            if (nic["new_macaddress"] != null)
                            {
                                writer.WriteAttributeString("new_macaddress", nic["new_macaddress"].ToString());
                            }

                            if (nic["virtual_ips"] != null)
                            {
                                writer.WriteAttributeString("virtual_ips", nic["virtual_ips"].ToString());
                            }
                            else
                            {
                                writer.WriteAttributeString("virtual_ips", null);
                            }


                            writer.WriteEndElement();
                        }
                        else
                        {
                            Trace.WriteLine(DateTime.Now + "\t This nic is not selected for protection ");
                        }
                    }

                    //writer.WriteStartElement("host");
                    //writer.WriteAttributeString("Total_size", totalSizeOfDisks.ToString());
                    //writer.WriteEndElement();
                }
                else
                {
                    Debug.WriteLine(DateTime.Now + " \t  +++++++++++++++++++++++++++ Disk List is empty");
                }
                writer.WriteStartElement("process_server");
                writer.WriteAttributeString("IP", processServerIp);
                writer.WriteEndElement();
                // writer.WriteStartElement("host");
                //writer.WriteAttributeString("Total_size", totalSizeOfDisks.ToString());
                writer.WriteStartElement("retention");
                writer.WriteAttributeString("retain_change_MB", retentionLogSize);

                if (retentionInHours != 0)
                {
                    writer.WriteAttributeString("retain_change_hours", retentionInHours.ToString());
                }
                else
                {
                    writer.WriteAttributeString("retain_change_hours", "");
                }
                if (retentionInDays != null)
                {
                    writer.WriteAttributeString("retain_change_days", retentionInDays);
                }
                else
                {
                    writer.WriteAttributeString("retain_change_days", "");
                }
                writer.WriteAttributeString("log_data_dir", retentionPath);
                if (retention_drive != null)
                {
                    writer.WriteAttributeString("retention_drive", retention_drive);
                }
                writer.WriteEndElement();
                if (selectedSparce == true)
                {
                    writer.WriteStartElement("time");
                    if (sparceDaysSelected == true)
                    {
                        writer.WriteStartElement("time_interval_window1");
                        writer.WriteAttributeString("unit", "days");
                        writer.WriteAttributeString("value", unitsofTimeIntervelWindow1Value.ToString());
                        writer.WriteAttributeString("bookmark_count", bookMarkCountWindow1.ToString());
                        writer.WriteAttributeString("range", unitsofTimeIntervelWindow.ToString());
                        writer.WriteEndElement();
                    }
                    if (sparceWeeksSelected == true)
                    {
                        writer.WriteStartElement("time_interval_window2");
                        writer.WriteAttributeString("unit", "weeks");
                        writer.WriteAttributeString("value", unitsofTimeIntervelWindow2Value.ToString());
                        writer.WriteAttributeString("bookmark_count", bookMarkCountWindow2.ToString());
                        writer.WriteEndElement();
                    }
                    if (sparceMonthsSelected == true)
                    {
                        writer.WriteStartElement("time_interval_window3");
                        writer.WriteAttributeString("unit", "months");
                        writer.WriteAttributeString("value", unitsofTimeIntervelWindow3Value.ToString());
                        writer.WriteAttributeString("bookmark_count", bookMarkCountWindow3.ToString());
                        writer.WriteEndElement();
                    }
                    writer.WriteEndElement();
                }
                else
                {
                    Trace.WriteLine(DateTime.Now + "\t User has not selected sparce retetnion for this protection");
                }
                writer.WriteStartElement("consistency_schedule");
                //string[] splitConsistency = consistency.Split(':');
                writer.WriteAttributeString("days", consistencyInDays);
                writer.WriteAttributeString("hours", consistencyInHours);
                writer.WriteAttributeString("mins", consistencyInmins);

                writer.WriteEndElement();
                if (role == Role.Secondary)
                {
                    if (networkNameList.Count != 0)
                    {

                        foreach (Network net in networkNameList)
                        {
                            writer.WriteStartElement("network");
                            writer.WriteAttributeString("vSpherehostname", net.Vspherehostname);
                            writer.WriteAttributeString("name", net.Networkname);
                            writer.WriteAttributeString("Type", net.Networktype);
                            writer.WriteAttributeString("switchUuid", net.Switchuuid);
                            writer.WriteAttributeString("portgroupKey", net.PortgroupKey);
                            writer.WriteEndElement();
                        }
                    }
                    if (configurationList.Count != 0)
                    {
                        foreach (Configurations config in configurationList)
                        {
                            writer.WriteStartElement("config");
                            writer.WriteAttributeString("vSpherehostname", config.vSpherehostname);
                            writer.WriteAttributeString("cpucount", config.cpucount.ToString());
                            writer.WriteAttributeString("memsize", config.memsize.ToString());
                            writer.WriteEndElement();
                        }
                    }
                }
                writer.WriteEndElement();
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
    }
}
