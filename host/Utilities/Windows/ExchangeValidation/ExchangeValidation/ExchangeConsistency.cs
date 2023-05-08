namespace ExchangeValidation
{
    using System;
    using System.Collections.Generic;
    using System.Diagnostics;
    using System.IO;
    using System.Linq;
    using System.Runtime.InteropServices;
    using System.Text;

    internal class ExchangeConsistency
    {
        public List<KeyValuePair<string, string>> commandList = new List<KeyValuePair<string, string>>();
        public string consistentTag;
        private Exchange exchObj = new Exchange();
        private ExchangeVsnap exvsnapObj = new ExchangeVsnap();
        public HashSet<KeyValuePair<string, string>> proVolRecoveryTag = new HashSet<KeyValuePair<string, string>>();
        public HashSet<KeyValuePair<string, string>> proVolRecoveryTime = new HashSet<KeyValuePair<string, string>>();
        public HashSet<KeyValuePair<string, string>> proVolVsnapPath = new HashSet<KeyValuePair<string, string>>();
        private HashSet<KeyValuePair<string, string>> sgCommonTag = new HashSet<KeyValuePair<string, string>>();
        public HashSet<KeyValuePair<KeyValuePair<string, string>, HashSet<KeyValuePair<string, string>>>> sgdbmap = new HashSet<KeyValuePair<KeyValuePair<string, string>, HashSet<KeyValuePair<string, string>>>>();
        private HashSet<KeyValuePair<string, string>> tgtDbPathVsnapPath = new HashSet<KeyValuePair<string, string>>();
        public HashSet<KeyValuePair<string, string>> tgtLogPathVsnapPath = new HashSet<KeyValuePair<string, string>>();
        public HashSet<KeyValuePair<KeyValuePair<string, string>, HashSet<KeyValuePair<string, string>>>> validSgDbMap = new HashSet<KeyValuePair<KeyValuePair<string, string>, HashSet<KeyValuePair<string, string>>>>();
        public static int VALUE_SIZE = 0x400;
        public HashSet<KeyValuePair<string, string>> volumeTagPairs = new HashSet<KeyValuePair<string, string>>();
        public string workingDirectory = "";

        private List<KeyValuePair<string, string>> AppendvsnapVolumeLOGAndDB(HashSet<string> dbCmdList, HashSet<KeyValuePair<string, string>> logVsnapPath, HashSet<KeyValuePair<string, string>> dbVsnapPath, string exchangeEseutilPath)
        {
            Trace.WriteLine("   DEBUG   ENTERED AppendvsnapVolumeLOGAndDB()", DateTime.Now.ToString());
            List<KeyValuePair<string, string>> list = new List<KeyValuePair<string, string>>();
            try
            {
                foreach (string str in dbCmdList)
                {
                    Trace.WriteLine("*****************dbCmd = " + str);
                    string str2 = " //Nologo";
                    str2 = str2 + str;
                    string key = "";
                    foreach (KeyValuePair<string, string> pair in logVsnapPath)
                    {
                        Trace.WriteLine("str = " + str);
                        Trace.WriteLine("lvsnap.Key = " + pair.Key);
                        if (str.ToLower().Contains(pair.Key.ToLower()))
                        {
                            str2 = str2 + " /vsnapVolumeLOG:\"";
                            str2 = str2 + pair.Value;
                            str2 = str2 + "\"";
                            break;
                        }
                    }
                    Trace.WriteLine("=================================================");
                    foreach (KeyValuePair<string, string> pair2 in dbVsnapPath)
                    {
                        Trace.WriteLine("str = " + str);
                        Trace.WriteLine("dvsnap.Key = " + pair2.Key);
                        if (str.ToLower().Contains(pair2.Key.ToLower()))
                        {
                            key = pair2.Key;
                            str2 = str2 + " /vsnapVolumeDB:\"";
                            str2 = str2 + pair2.Value;
                            str2 = str2 + "\"";
                            break;
                        }
                    }
                    if (!string.IsNullOrEmpty(exchangeEseutilPath))
                    {
                        str2 = str2 + " /exchangeBinPath:\"" + Path.GetDirectoryName(exchangeEseutilPath) + "\"";
                        list.Add(new KeyValuePair<string, string>(key, str2));
                    }
                }
            }
            catch (Exception exception)
            {
                Console.WriteLine("Exception in AppendvsnapVolumeLOGAndDB(). Message = {0}", exception.Message);
                Trace.WriteLine(string.Format("Exception in AppendvsnapVolumeLOGAndDB(). Message = {0}", exception.Message));
                list.Clear();
            }
            Trace.WriteLine("   DEBUG   EXITED AppendvsnapVolumeLOGAndDB()", DateTime.Now.ToString());
            return list;
        }

        private List<KeyValuePair<string, string>> AppendvsnapVolumeLOGAndDB1(HashSet<string> dbCmdList, HashSet<KeyValuePair<string, string>> logVsnapPath, HashSet<KeyValuePair<string, string>> dbVsnapPath, string exchangeEseutilPath)
        {
            Trace.WriteLine("   DEBUG   ENTERED AppendvsnapVolumeLOGAndDB1()", DateTime.Now.ToString());
            List<KeyValuePair<string, string>> list = new List<KeyValuePair<string, string>>();
            try
            {
                char[] separator = new char[] { '/' };
                foreach (string str in dbCmdList)
                {
                    string str2 = " //Nologo";
                    str2 = str2 + str;
                    string key = "";
                    string strA = "";
                    string str5 = "";
                    string str6 = "";
                    bool flag = false;
                    string[] strArray = str.Split(separator);
                    foreach (string str7 in strArray)
                    {
                        str6 = str7.Split(new char[] { ':' })[0];
                        if (string.Compare(str6, "exchangeLogFilePath", true) == 0)
                        {
                            strA = this.exchObj.TrimData(str7, ":", 2, 2);
                        }
                        foreach (KeyValuePair<string, string> pair in logVsnapPath)
                        {
                            if (string.Compare(strA, pair.Key, true) == 0)
                            {
                                str2 = str2 + " /vsnapVolumeLOG:\"";
                                str2 = str2 + pair.Value;
                                str2 = str2 + "\"";
                                flag = true;
                                break;
                            }
                        }
                        if (flag)
                        {
                            break;
                        }
                    }
                    str6 = "";
                    flag = false;
                    foreach (string str8 in strArray)
                    {
                        str6 = str8.Split(new char[] { ':' })[0];
                        if (string.Compare(str6, "exchangeDBFilePath", true) == 0)
                        {
                            str5 = this.exchObj.TrimData(str8, ":", 2, 2);
                        }
                        foreach (KeyValuePair<string, string> pair2 in dbVsnapPath)
                        {
                            if (string.Compare(str5, pair2.Key, true) == 0)
                            {
                                key = pair2.Key;
                                str2 = str2 + " /vsnapVolumeDB:\"";
                                str2 = str2 + pair2.Value;
                                str2 = str2 + "\"";
                                flag = true;
                                break;
                            }
                        }
                        if (flag)
                        {
                            break;
                        }
                    }
                    if (!string.IsNullOrEmpty(exchangeEseutilPath))
                    {
                        str2 = str2 + " /exchangeBinPath:\"" + Path.GetDirectoryName(exchangeEseutilPath) + "\"";
                        list.Add(new KeyValuePair<string, string>(key, str2));
                    }
                }
            }
            catch (Exception exception)
            {
                Console.WriteLine("Exception in AppendvsnapVolumeLOGAndDB1(). Message = {0}", exception.Message);
                Trace.WriteLine(string.Format("Exception in AppendvsnapVolumeLOGAndDB1(). Message = {0}", exception.Message));
                list.Clear();
            }
            Trace.WriteLine("   DEBUG   EXITED AppendvsnapVolumeLOGAndDB1()", DateTime.Now.ToString());
            return list;
        }

        private bool CheckEDBFiles(HashSet<KeyValuePair<string, string>> dbVsnapPath, List<FileInfo> pathToEdbFile)
        {
            Trace.WriteLine("   DEBUG   ENTERED CheckEDBFiles()", DateTime.Now.ToString());
            bool flag = false;
            try
            {
                foreach (KeyValuePair<string, string> pair in dbVsnapPath)
                {
                    FileInfo info = new FileInfo(pair.Key);
                    Trace.WriteLine("   " + info.Name, "Database Name       ");
                    flag = false;
                    foreach (FileInfo info2 in pathToEdbFile)
                    {
                        if (info2.Name.Equals(info.Name))
                        {
                            Trace.WriteLine("   " + info2.FullName, "Database FullPath   ");
                            flag = true;
                            break;
                        }
                    }
                    if (!flag)
                    {
                        flag = false;
                        Console.WriteLine("\n[Error] : Missing Database file \"{0}\" in mounted vsnap", info.Name);
                        Trace.WriteLine("[Error] : Missing Database file in mounted vsnap = " + info.Name);
                        Console.WriteLine("Please verify all exchange affected volumes are protected");
                        return flag;
                    }
                }
            }
            catch (Exception exception)
            {
                Console.WriteLine("Exception in CheckEDBFiles(). Message = {0}", exception.Message);
                Trace.WriteLine(string.Format("Exception in CheckEDBFiles(). Message = {0}", exception.Message));
                flag = false;
            }
            Trace.WriteLine("   DEBUG   EXITED CheckEDBFiles()", DateTime.Now.ToString());
            return flag;
        }

        public bool CreateSGDBMap(List<SgDbMap> sgdbpathdrive, out HashSet<KeyValuePair<KeyValuePair<string, string>, HashSet<KeyValuePair<string, string>>>> sgDatabaseMap)
        {
            Trace.WriteLine("   DEBUG   ENTERED CreateSGDBMap()", DateTime.Now.ToString());
            sgDatabaseMap = new HashSet<KeyValuePair<KeyValuePair<string, string>, HashSet<KeyValuePair<string, string>>>>();
            HashSet<KeyValuePair<string, string>> set = new HashSet<KeyValuePair<string, string>>();
            bool flag = true;
            try
            {
                foreach (SgDbMap map in sgdbpathdrive)
                {
                    set.Add(new KeyValuePair<string, string>(map.tgtSgPath, map.tgtLogDrive));
                }
                foreach (KeyValuePair<string, string> pair in set)
                {
                    HashSet<KeyValuePair<string, string>> set2 = new HashSet<KeyValuePair<string, string>>();
                    foreach (SgDbMap map2 in sgdbpathdrive)
                    {
                        if (string.Compare(map2.tgtSgPath, pair.Key, true) == 0)
                        {
                            set2.Add(new KeyValuePair<string, string>(map2.tgtDbPath, map2.tgtDbDrive));
                        }
                    }
                    sgDatabaseMap.Add(new KeyValuePair<KeyValuePair<string, string>, HashSet<KeyValuePair<string, string>>>(pair, set2));
                }
            }
            catch (Exception exception)
            {
                Console.WriteLine("Exception in CreateSGDBMap(), Message = {0}", exception.Message);
                Trace.WriteLine(string.Format("Exception in CreateSGDBMap(), Message = {0}", exception.Message));
                flag = false;
            }
            Trace.WriteLine("   DEBUG   EXITED CreateSGDBMap()", DateTime.Now.ToString());
            return flag;
        }

        public bool CreateSgEdbVsnapMap(List<SgDbMap> sgdbPathDrive, HashSet<KeyValuePair<string, string>> volumeVsnapPath)
        {
            Trace.WriteLine("   DEBUG   ENTERED CreateSgEdbVsnapMap()", DateTime.Now.ToString());
            bool flag = true;
            try
            {
                string strA = "";
                string tgtDbDrive = "";
                foreach (SgDbMap map in sgdbPathDrive)
                {
                    strA = map.tgtLogDrive;
                    tgtDbDrive = map.tgtDbDrive;
                    foreach (KeyValuePair<string, string> pair in volumeVsnapPath)
                    {
                        if (string.Compare(strA, pair.Key, true) == 0)
                        {
                            this.tgtLogPathVsnapPath.Add(new KeyValuePair<string, string>(map.tgtSgPath, pair.Value));
                        }
                        if (string.Compare(tgtDbDrive, pair.Key, true) == 0)
                        {
                            this.tgtDbPathVsnapPath.Add(new KeyValuePair<string, string>(map.tgtDbPath, pair.Value));
                        }
                    }
                }
            }
            catch (Exception exception)
            {
                Console.WriteLine("Exception in CreateSgEdbVsnapMap(). Message = {0}", exception.Message);
                Trace.WriteLine(string.Format("Exception in CreateSgEdbVsnapMap(). Message = {0}", exception.Message));
                flag = false;
            }
            Trace.WriteLine("   DEBUG   EXITED CreateSgEdbVsnapMap()", DateTime.Now.ToString());
            return flag;
        }

        public bool CreateVsnapAndValidate(string virSrvName, string batchScriptFile, List<SgDbMap> sgdbPathDrive, string appEventTag, string seqNum, bool bUseVsnap, string srcName, HashSet<string> dbCmdList, string exchEseutilPath)
        {
            Trace.WriteLine("   DEBUG   ENTERED CreateVsnapAndValidate()", DateTime.Now.ToString());
            bool flag = true;
            string vsnapLoc = this.exchObj.inMageInstallPath + @"\ExchangeConsistency";
            string exchangeEseutilPath = exchEseutilPath;
            HashSet<string> logdbVsnapPath = new HashSet<string>();
            HashSet<string> set2 = new HashSet<string>();
            List<FileInfo> items = new List<FileInfo>();
            try
            {
                string str3;
                HashSet<string> set3;
                Console.WriteLine("\n***** Checking for mounted vsnaps");
                HashSet<string> protectedVolumes = new HashSet<string>();
                foreach (SgDbMap map in sgdbPathDrive)
                {
                    protectedVolumes.Add(map.tgtLogDrive);
                    protectedVolumes.Add(map.tgtDbDrive);
                }
                this.CreateSGDBMap(sgdbPathDrive, out this.sgdbmap);
                if (!bUseVsnap)
                {
                    goto Label_03E0;
                }
                if (string.IsNullOrEmpty(virSrvName))
                {
                    str3 = srcName;
                }
                else
                {
                    str3 = virSrvName;
                }
                if (this.ReadVsnapConfiguration(str3, protectedVolumes))
                {
                    set3 = new HashSet<string>();
                    if (this.FindCommonConsistenctPoint(this.sgdbmap))
                    {
                        if (this.validSgDbMap.Count == 0)
                        {
                            Console.WriteLine("[Error] Found no common consistent tag across all the volumes");
                            Trace.WriteLine("[Error] Found no common consistent tag across all the volumes");
                            Console.WriteLine("Please select common recovery tag or time to perform consistency validation");
                            return false;
                        }
                        foreach (KeyValuePair<KeyValuePair<string, string>, HashSet<KeyValuePair<string, string>>> pair in this.validSgDbMap)
                        {
                            foreach (string str4 in dbCmdList)
                            {
                                if (str4.ToLower().Contains(pair.Key.Key.ToLower()))
                                {
                                    set2.Add(str4);
                                }
                            }
                        }
                        goto Label_01CF;
                    }
                    Console.WriteLine("[Error] Failed to find common consistent tag across all the volumes");
                    Trace.WriteLine("[Error] Failed to find common consistent tag across all the volumes");
                }
                return false;
            Label_01CF:
                if (string.IsNullOrEmpty(appEventTag))
                {
                    foreach (string str5 in set2)
                    {
                        foreach (KeyValuePair<string, string> pair2 in this.sgCommonTag)
                        {
                            if (str5.ToLower().Contains(pair2.Key.ToLower()))
                            {
                                string item = str5;
                                item = item + " /eventTag:\"" + pair2.Value + "\"";
                                set3.Add(item);
                            }
                        }
                    }
                }
                else
                {
                    foreach (string str7 in set2)
                    {
                        foreach (KeyValuePair<string, string> pair3 in this.sgCommonTag)
                        {
                            string newValue = "";
                            if (str7.ToLower().Contains(pair3.Key.ToLower()))
                            {
                                if (pair3.Value.Contains<char>(':'))
                                {
                                    string[] strArray = this.consistentTag.Split(new char[] { ':', '/' });
                                    this.consistentTag = "";
                                    for (int i = 0; i < strArray.Length; i++)
                                    {
                                        this.consistentTag = this.consistentTag + strArray[i].Trim();
                                        if (i != (strArray.Length - 1))
                                        {
                                            this.consistentTag = this.consistentTag + "_";
                                        }
                                    }
                                    newValue = "\"";
                                    newValue = newValue + this.consistentTag + "\"";
                                }
                                else
                                {
                                    newValue = pair3.Value;
                                }
                                set3.Add(str7.Replace(appEventTag, newValue));
                            }
                        }
                    }
                }
                dbCmdList = set3;
                goto Label_0601;
            Label_03E0:
                foreach (string str9 in protectedVolumes)
                {
                    Console.WriteLine("\nVolume = \"{0}\"", str9);
                    this.exvsnapObj.CheckForMountedVsnapAndUnmount(this.exchObj.inMageCdpCliPath, str9, vsnapLoc, true);
                }
                Console.WriteLine("\n***** Creating Working Directory for Mounting Vsnap");
                if (this.exchObj.CreateDir(vsnapLoc))
                {
                    if (!Directory.Exists(vsnapLoc))
                    {
                        Console.WriteLine("[Error] : Failed to create working directory");
                        Trace.WriteLine("[Error] : Failed to create working directory");
                        return false;
                    }
                    this.workingDirectory = vsnapLoc;
                }
                Console.WriteLine("      Working Directory   = {0}", vsnapLoc);
                Trace.WriteLine("   " + vsnapLoc, "Working Directory   ");
                Console.WriteLine("\n***** Started Creating Vsnap ");
                Console.WriteLine("      Tag used for creating vsnap = {0}", appEventTag);
                Trace.WriteLine("Tag to Create Vsnap = " + appEventTag);
                this.workingDirectory = this.workingDirectory + @"\";
                this.workingDirectory = this.workingDirectory + seqNum;
                this.workingDirectory = this.workingDirectory + @"\ExchangeVsnap\";
                int fileNum = 0;
                foreach (string str10 in protectedVolumes)
                {
                    fileNum++;
                    string vsnapMntDrive = "";
                    Console.WriteLine("\nVolume = \"{0}\"", str10);
                    if (!this.exvsnapObj.MountVsnap(this.exchObj.inMageCdpCliPath, str10, this.workingDirectory, fileNum, appEventTag, ref vsnapMntDrive))
                    {
                        Console.WriteLine("\n[Error] : Failed to mount vsnap for Exchange log target drive : {0}", str10);
                        Trace.WriteLine("[Error] : Failed to mount vsnap for Exchange log target drive : {0}", str10);
                        return false;
                    }
                    this.proVolVsnapPath.Add(new KeyValuePair<string, string>(str10, vsnapMntDrive));
                }
                Console.WriteLine("\nVsnap Details\n=============");
                foreach (KeyValuePair<string, string> pair4 in this.proVolVsnapPath)
                {
                    Console.WriteLine("Volume         = {0}", pair4.Key);
                    Console.WriteLine("Vsnap Location = {0}", pair4.Value);
                }
            Label_0601:
                Console.WriteLine("\n");
                if (!this.CreateSgEdbVsnapMap(sgdbPathDrive, this.proVolVsnapPath))
                {
                    Console.WriteLine("[Error] Failed to map exchange log and database to vsnap Path");
                    Trace.WriteLine("[Error] Failed to map exchange log and database to vsnap Path");
                    return false;
                }
                this.commandList = this.AppendvsnapVolumeLOGAndDB1(dbCmdList, this.tgtLogPathVsnapPath, this.tgtDbPathVsnapPath, exchangeEseutilPath);
                logdbVsnapPath = this.exvsnapObj.CollectVsnapPath(this.tgtLogPathVsnapPath, this.tgtDbPathVsnapPath);
                if (!this.exvsnapObj.CheckVsnap(this.exchObj.inMageCdpCliPath, logdbVsnapPath))
                {
                    Console.WriteLine("\n[Error] One or more vsnaps are not found in the expected path");
                    Trace.WriteLine("[Error] One or more vsnaps are not found in the expected path");
                    Console.WriteLine("Cannot continue validation");
                    return false;
                }
                List<FileInfo> pathToEdbFile = new List<FileInfo>();
                foreach (string str12 in logdbVsnapPath)
                {
                    if (this.exchObj.FindFile(str12, "*.edb", ref items, ""))
                    {
                        foreach (FileInfo info in items)
                        {
                            pathToEdbFile.Add(info);
                        }
                        continue;
                    }
                }
                if (!this.CheckEDBFiles(this.tgtDbPathVsnapPath, pathToEdbFile))
                {
                    Console.WriteLine("[Error] One or more exchange database files are not found in the vsnap");
                    Trace.WriteLine("[Error] One or more exchange database files are not found in the vsnap");
                    return false;
                }
            }
            catch (Exception exception)
            {
                Console.WriteLine("Exception in CreateVsnapAndValidate(). Message = {0}", exception.Message);
                Trace.WriteLine(string.Format("Exception in CreateVsnapAndValidate(). Message = {0}", exception.Message));
                flag = false;
            }
            Trace.WriteLine("   DEBUG   EXITED CreateVsnapAndValidate()", DateTime.Now.ToString());
            return flag;
        }

        ~ExchangeConsistency()
        {
        }

        private bool FindCommonConsistenctPoint(HashSet<KeyValuePair<KeyValuePair<string, string>, HashSet<KeyValuePair<string, string>>>> sgDbMap)
        {
            Trace.WriteLine("   DEBUG   ENTERED FindCommonConsistenctPoint()", DateTime.Now.ToString());
            bool flag = true;
            try
            {
                foreach (KeyValuePair<KeyValuePair<string, string>, HashSet<KeyValuePair<string, string>>> pair in sgDbMap)
                {
                    bool flag2 = true;
                    string strB = pair.Key.Value;
                    new HashSet<string>();
                    foreach (KeyValuePair<string, string> pair2 in this.volumeTagPairs)
                    {
                        if (string.Compare(pair2.Key, strB, true) == 0)
                        {
                            this.consistentTag = pair2.Value;
                            break;
                        }
                    }
                    foreach (KeyValuePair<string, string> pair3 in pair.Value)
                    {
                        foreach (KeyValuePair<string, string> pair4 in this.volumeTagPairs)
                        {
                            if ((string.Compare(pair4.Key, pair3.Value, true) == 0) && (string.Compare(pair4.Value, this.consistentTag, true) != 0))
                            {
                                string recoverytag = "";
                                this.exvsnapObj.ReadRecoveryTagFromMountedVsnap(this.exchObj.inMageCdpCliPath, strB, ref recoverytag, true);
                                this.consistentTag = recoverytag;
                                this.exvsnapObj.ReadRecoveryTagFromMountedVsnap(this.exchObj.inMageCdpCliPath, pair3.Value, ref recoverytag, true);
                                if (string.Compare(this.consistentTag, recoverytag, true) != 0)
                                {
                                    flag2 = false;
                                }
                            }
                        }
                    }
                    if (!flag2)
                    {
                        Console.WriteLine("\nFound no common consistent point");
                        Console.WriteLine("Skipping Storage group volume and its associated database volumes for validation");
                        Console.WriteLine("\tStorage group volume = {0}", pair.Key.Value);
                        Trace.WriteLine("Skipping Storage group volume and its associated database volumes for validation");
                        Trace.WriteLine("Storage group volume = " + pair.Key.Value);
                        foreach (KeyValuePair<string, string> pair5 in pair.Value)
                        {
                            Console.WriteLine("\t     Database volume = {0}", pair5.Value);
                            Trace.WriteLine("Database volume = " + pair5.Value);
                        }
                        continue;
                    }
                    Console.WriteLine("\nFound common consistent point = {0}", this.consistentTag);
                    Trace.WriteLine("Found common consistent point = " + this.consistentTag);
                    Console.WriteLine("\tStorage group volume = {0}", pair.Key.Value);
                    Trace.WriteLine("Storage group volume = " + pair.Key.Value);
                    foreach (KeyValuePair<string, string> pair6 in pair.Value)
                    {
                        Console.WriteLine("\t     Database volume = {0}", pair6.Value);
                        Trace.WriteLine("Database volume = " + pair6.Value);
                    }
                    this.validSgDbMap.Add(pair);
                    this.sgCommonTag.Add(new KeyValuePair<string, string>(pair.Key.Key, this.consistentTag));
                }
            }
            catch (Exception exception)
            {
                Console.WriteLine("Exception in FindCommonConsistenctPoint(). Message = {0}", exception.Message);
                Trace.WriteLine(string.Format("Exception in FindCommonConsistenctPoint(). Message = {0}", exception.Message));
                flag = false;
            }
            Trace.WriteLine("   DEBUG   EXITED FindCommonConsistenctPoint()", DateTime.Now.ToString());
            return flag;
        }

        private bool ReadVsnapConfiguration(string sourceServer, HashSet<string> protectedVolumes)
        {
            Trace.WriteLine("   DEBUG   ENTERED ReadVsnapConfiguration()", DateTime.Now.ToString());
            bool flag = true;
            string section = "EXCHANGEVSNAP";
            char[] separator = new char[] { '=' };
            char[] chArray2 = new char[] { ',' };
            char[] trimChars = new char[] { '\\' };
            char[] chArray4 = new char[] { ':' };
            try
            {
                string fileName = this.exchObj.inMageFailoverFolder + @"\" + sourceServer + "_exchange_vsnap_config.conf";
                VALUE_SIZE = (int) new FileInfo(fileName).Length;
                StringBuilder retVal = new StringBuilder(VALUE_SIZE);
                if (!File.Exists(fileName))
                {
                    Console.WriteLine("[Error] Failed to read snapshot details from the file \"{0}\"", fileName);
                    Trace.WriteLine("[Error] Vsnap configuration file not found" + fileName);
                    Console.WriteLine("File not found. Please check whether the file exists in the expected path");
                    flag = false;
                }
                else
                {
                    NativeMethods.GetPrivateProfileString(section, "SourceVirtualServerName", "", retVal, VALUE_SIZE - 1, fileName);
                    retVal.ToString();
                    NativeMethods.GetPrivateProfileString(section, "VolumeVsnapPath", "", retVal, VALUE_SIZE - 1, fileName);
                    foreach (string str3 in retVal.ToString().Split(chArray2))
                    {
                        string[] strArray = str3.Split(separator);
                        string strA = strArray[0].TrimEnd(trimChars);
                        foreach (string str5 in protectedVolumes)
                        {
                            string strB = str5.TrimEnd(chArray4);
                            if (string.Compare(strA, strB, true) == 0)
                            {
                                string str7 = strArray[1].Trim();
                                if (str7.Length == 1)
                                {
                                    str7 = str7 + ":";
                                }
                                this.proVolVsnapPath.Add(new KeyValuePair<string, string>(str5, str7));
                            }
                        }
                    }
                    if (this.proVolVsnapPath.Count == 0)
                    {
                        Console.WriteLine("[Error] Found no existing snapshot for the protected volumes");
                        Trace.WriteLine("[Error] Found no existing snapshot for the protected volumes");
                        Console.WriteLine("Please check whether snapshot exists for the protected volumes");
                        return false;
                    }
                    string str8 = "";
                    NativeMethods.GetPrivateProfileString(section, "RecoveryTag", "", retVal, VALUE_SIZE - 1, fileName);
                    str8 = retVal.ToString();
                    string str9 = "";
                    NativeMethods.GetPrivateProfileString(section, "RecoveryTime", "", retVal, VALUE_SIZE - 1, fileName);
                    str9 = retVal.ToString();
                    if (!string.IsNullOrEmpty(str8))
                    {
                        foreach (string str10 in str8.Split(chArray2))
                        {
                            string[] strArray2 = str10.Split(separator);
                            string str11 = strArray2[0].TrimEnd(trimChars);
                            foreach (string str12 in protectedVolumes)
                            {
                                string str13 = str12.TrimEnd(chArray4);
                                if (string.Compare(str11, str13, true) == 0)
                                {
                                    string str14 = strArray2[1].Trim();
                                    this.proVolRecoveryTag.Add(new KeyValuePair<string, string>(str12, str14));
                                }
                            }
                        }
                    }
                    if (!string.IsNullOrEmpty(str9))
                    {
                        foreach (string str15 in str9.Split(chArray2))
                        {
                            string[] strArray3 = str15.Split(separator);
                            string str16 = strArray3[0].TrimEnd(trimChars);
                            foreach (string str17 in protectedVolumes)
                            {
                                string str18 = str17.TrimEnd(chArray4);
                                if (string.Compare(str16, str18, true) == 0)
                                {
                                    string str19 = strArray3[1].Trim();
                                    this.proVolRecoveryTime.Add(new KeyValuePair<string, string>(str17, str19));
                                }
                            }
                        }
                    }
                    if (string.IsNullOrEmpty(str8) && string.IsNullOrEmpty(str9))
                    {
                        foreach (string str20 in protectedVolumes)
                        {
                            string recoverytag = "";
                            this.exvsnapObj.ReadRecoveryTagFromMountedVsnap(this.exchObj.inMageCdpCliPath, str20, ref recoverytag, false);
                            this.volumeTagPairs.Add(new KeyValuePair<string, string>(str20, recoverytag));
                        }
                    }
                    foreach (KeyValuePair<string, string> pair in this.proVolVsnapPath)
                    {
                        if (this.proVolRecoveryTag.Count != 0)
                        {
                            foreach (KeyValuePair<string, string> pair2 in this.proVolRecoveryTag)
                            {
                                if (string.Compare(pair.Key, pair2.Key, true) == 0)
                                {
                                    this.volumeTagPairs.Add(new KeyValuePair<string, string>(pair.Key, pair2.Value));
                                }
                            }
                        }
                        if (this.proVolRecoveryTime.Count != 0)
                        {
                            foreach (KeyValuePair<string, string> pair3 in this.proVolRecoveryTime)
                            {
                                if (string.Compare(pair.Key, pair3.Key, true) == 0)
                                {
                                    this.volumeTagPairs.Add(new KeyValuePair<string, string>(pair.Key, pair3.Value));
                                }
                            }
                            continue;
                        }
                    }
                    Console.WriteLine("\nVsnap Details\n=============");
                    foreach (KeyValuePair<string, string> pair4 in this.proVolVsnapPath)
                    {
                        Console.WriteLine("Volume = {0}", pair4.Key);
                        Console.WriteLine("   Vsnap Location = {0}", pair4.Value);
                        if (this.volumeTagPairs.Count != 0)
                        {
                            foreach (KeyValuePair<string, string> pair5 in this.volumeTagPairs)
                            {
                                if (string.Compare(pair5.Key, pair4.Key, true) == 0)
                                {
                                    Console.WriteLine("   Recovery Tag   = {0}", pair5.Value);
                                }
                            }
                            continue;
                        }
                    }
                }
            }
            catch (Exception exception)
            {
                Console.WriteLine("Exception in ReadVsnapConfiguration(). Message = {0}", exception.Message);
                Trace.WriteLine(string.Format("Exception in ReadVsnapConfiguration(). Message = {0}", exception.Message));
                flag = false;
            }
            Trace.WriteLine("   DEBUG   EXITED ReadVsnapConfiguration()", DateTime.Now.ToString());
            return flag;
        }
    }
}

