namespace ExchangeValidation
{
    using System;
    using System.Collections.Generic;
    using System.Diagnostics;
    using System.IO;
    using System.Linq;
    using System.Runtime.InteropServices;

    internal class ExchangeLogFlush
    {
        private ExchangeConsistency exchConsObj = new ExchangeConsistency();
        private Exchange exchObj = new Exchange();
        private ExchangeXML exchXmlObj = new ExchangeXML();
        public List<KeyValuePair<string, List<string>>> storageGpForFlush = new List<KeyValuePair<string, List<string>>>();

        private List<KeyValuePair<string, string>> CollectEdbLogRequiredField(string appName, List<KeyValuePair<string, List<string>>> sgDbListForFlush, HashSet<KeyValuePair<string, string>> volumeVsnapSet, string esePath)
        {
            Trace.WriteLine("   DEBUG   ENTERED CollectEdbLogRequiredField()", DateTime.Now.ToString());
            List<KeyValuePair<string, string>> list = new List<KeyValuePair<string, string>>();
            new List<KeyValuePair<string, HashSet<string>>>();
            HashSet<string> set = new HashSet<string>();
            string str = esePath;
            try
            {
                if (string.IsNullOrEmpty(str))
                {
                    return list;
                }
                foreach (KeyValuePair<string, List<string>> pair in sgDbListForFlush)
                {
                    List<string> list2 = new List<string>(pair.Value);
                    foreach (string str2 in list2)
                    {
                        FileInfo info = new FileInfo(str2);
                        foreach (KeyValuePair<string, string> pair2 in volumeVsnapSet)
                        {
                            List<FileInfo> items = new List<FileInfo>();
                            if (this.exchObj.FindFile1(pair2.Value, "*.edb", "", out items))
                            {
                                foreach (FileInfo info2 in items)
                                {
                                    if (string.Compare(info.Name, info2.Name, true) == 0)
                                    {
                                        set.Add(info2.FullName);
                                    }
                                }
                                continue;
                            }
                            Console.WriteLine("Failed to read exchange edb files from the path \"{0}\"", pair2.Value);
                            Trace.WriteLine("Failed to read exchange edb files from the path = " + pair2.Value);
                        }
                    }
                }
                foreach (string str3 in set)
                {
                    FileInfo info3 = new FileInfo(str3);
                    string str4 = this.RunEseutilForLogRequiredCollection(str3, str);
                    list.Add(new KeyValuePair<string, string>(info3.FullName, str4));
                }
            }
            catch (Exception exception)
            {
                Console.WriteLine("Exception in CollectEdbLogRequiredField(). Message = {0}", exception.Message);
                Trace.WriteLine(string.Format("Exception in CollectEdbLogRequiredField(). Message = {0}", exception.Message));
                list.Clear();
            }
            Trace.WriteLine("   DEBUG   EXITED CollectEdbLogRequiredField()", DateTime.Now.ToString());
            return list;
        }

        private HashSet<KeyValuePair<string, HashSet<string>>> CollectLogFiles(List<KeyValuePair<string, List<string>>> sgForFlush, HashSet<KeyValuePair<string, string>> volVsnapHash, HashSet<KeyValuePair<string, string>> logPathChkPrefix)
        {
            Trace.WriteLine("   DEBUG   ENTERED CollectLogFiles()", DateTime.Now.ToString());
            HashSet<KeyValuePair<string, HashSet<string>>> set = new HashSet<KeyValuePair<string, HashSet<string>>>();
            HashSet<StorageInfo> set2 = new HashSet<StorageInfo>();
            try
            {
                foreach (KeyValuePair<string, List<string>> pair in sgForFlush)
                {
                    string str = "";
                    string key = "";
                    string str3 = "";
                    foreach (KeyValuePair<string, string> pair2 in volVsnapHash)
                    {
                        if (string.Compare(pair.Key, pair2.Key, true) == 0)
                        {
                            Console.WriteLine("sgvsnap.Key = {0}", pair2.Key);
                            string str4 = this.exchObj.GetVolPathName(pair2.Key).ToString();
                            Console.WriteLine("vol = {0}", str4);
                            int length = str4.Length;
                            int num2 = 0;
                            char ch = str4[length - 1];
                            if (string.Compare(ch.ToString(), @"\", true) == 0)
                            {
                                num2 = 1;
                            }
                            string str5 = pair2.Key.Substring(length - num2);
                            Console.WriteLine("folderpath = {0}", str5);
                            string str6 = pair2.Value;
                            Console.WriteLine("exactVsnapPath = {0}", str6);
                            str = str6 + @"\" + str5;
                            Console.WriteLine("storevsnapPath = {0}", str);
                            key = pair2.Key;
                        }
                    }
                    foreach (KeyValuePair<string, string> pair3 in logPathChkPrefix)
                    {
                        if (string.Compare(pair.Key, pair3.Key, true) == 0)
                        {
                            str3 = pair3.Value;
                        }
                    }
                    StorageInfo item = new StorageInfo();
                    item.storePath = key;
                    item.storeVsnapPath = str;
                    item.logPrefix = str3;
                    set2.Add(item);
                }
                foreach (StorageInfo info2 in set2)
                {
                    List<FileInfo> items = new List<FileInfo>();
                    if (this.exchObj.FindFile1(info2.storeVsnapPath, "*.log", info2.logPrefix, out items))
                    {
                        HashSet<string> set3 = new HashSet<string>();
                        foreach (FileInfo info3 in items)
                        {
                            set3.Add(info3.Name);
                        }
                        //Console.WriteLine("here 1");
                        set.Add(new KeyValuePair<string, HashSet<string>>(info2.storePath, set3));
                        //Console.WriteLine("here 2");
                    }
                }
            }
            catch (Exception exception)
            {
                Console.WriteLine("Exception in CollectLogFiles(). Message = {0}", exception.Message);
                Trace.WriteLine(string.Format("Exception in CollectLogFiles(). Message = {0}", exception.Message));
                set.Clear();
            }
            Trace.WriteLine("   DEBUG   EXITED CollectLogFiles()", DateTime.Now.ToString());
            return set;
        }

        private bool CollectSGListForFlush(List<IntegrityResult> resultList, List<SgDbMap> srcLogPath, out List<KeyValuePair<string, List<string>>> sgForFlush)
        {
            Trace.WriteLine("   DEBUG   ENTERED CollectSGListForFlush()", DateTime.Now.ToString());
            sgForFlush = new List<KeyValuePair<string, List<string>>>();
            HashSet<KeyValuePair<string, string>> set = new HashSet<KeyValuePair<string, string>>();
            bool flag = true;
            try
            {
                foreach (IntegrityResult result in resultList)
                {
                    if (string.Compare(result.logIntegrity, "True", true) == 0)
                    {
                        set.Add(new KeyValuePair<string, string>(result.logFile, result.logIntegrity));
                    }
                    else
                    {
                        Console.WriteLine("[Warning] : \"{0}\" failed integrity test and not applicable for log flush", result.dbFile);
                        Trace.WriteLine("[Warning] : Logs failed integrity test and not applicable for log flush = " + result.dbFile);
                    }
                }
                foreach (KeyValuePair<string, string> pair in set)
                {
                    bool flag2 = true;
                    List<string> list = new List<string>();
                    foreach (IntegrityResult result2 in resultList)
                    {
                        if (string.Compare(pair.Key, result2.logFile, true) == 0)
                        {
                            if (string.Compare(result2.dbIntegrity, "True", true) == 0)
                            {
                                list.Add(result2.dbFile);
                            }
                            else
                            {
                                Console.WriteLine("[Warning] : \"{0}\" failed integrity test and it's corresponding log files [{1}] are not applicable for log flush", result2.dbFile, result2.logFile);
                                Console.WriteLine("[Warning] : Database failed integrity test and it's corresponding log files are not applicable for log flush" + result2.dbFile + "," + result2.logFile);
                                flag2 = false;
                                break;
                            }
                        }
                    }
                    if (flag2)
                    {
                        foreach (SgDbMap map in srcLogPath)
                        {
                            if (string.Compare(map.tgtSgPath, pair.Key, true) == 0)
                            {
                                sgForFlush.Add(new KeyValuePair<string, List<string>>(map.tgtSgPath, list));
                            }
                        }
                        continue;
                    }
                }
            }
            catch (Exception exception)
            {
                Console.WriteLine("Exception in CollectSGListForFlush(). Message = {0}", exception.Message);
                Trace.WriteLine(string.Format("Exception in CollectSGListForFlush(). Message = {0}", exception.Message));
                flag = false;
            }
            Trace.WriteLine("   DEBUG   EXITED CollectSGListForFlush()", DateTime.Now.ToString());
            return flag;
        }

        private HashSet<KeyValuePair<string, HashSet<string>>> CollectValidatedLogFiles(HashSet<KeyValuePair<string, HashSet<string>>> logfilesForFlush, List<KeyValuePair<string, string>> logRequiredList, List<KeyValuePair<string, List<string>>> storageGpForFlush, HashSet<KeyValuePair<string, string>> logPathChkPrefix)
        {
            Trace.WriteLine("   DEBUG   ENTERED CollectValidatedLogFiles()", DateTime.Now.ToString());
            HashSet<KeyValuePair<string, HashSet<string>>> set = new HashSet<KeyValuePair<string, HashSet<string>>>();
            try
            {
                foreach (KeyValuePair<string, HashSet<string>> pair in logfilesForFlush)
                {
                    string key = pair.Key;
                    foreach (KeyValuePair<string, List<string>> pair2 in storageGpForFlush)
                    {
                        string strB = pair2.Key;
                        List<string> list = new List<string>(pair2.Value);
                        if (string.Compare(key, strB, true) == 0)
                        {
                            FileInfo info = new FileInfo(list[0]);
                            foreach (KeyValuePair<string, string> pair3 in logRequiredList)
                            {
                                FileInfo info2 = new FileInfo(pair3.Key);
                                if (string.Compare(info.Name, info2.Name, true) == 0)
                                {
                                    string str3 = pair3.Value.Split(new char[] { '(' })[1].Split(new char[] { 'x', '-' })[1];
                                    string data = pair.Value.First<string>();
                                    data = this.exchObj.TrimData(data, "", 0, 4);
                                    string str5 = "";
                                    foreach (KeyValuePair<string, string> pair4 in logPathChkPrefix)
                                    {
                                        if (string.Compare(pair4.Key, key, true) == 0)
                                        {
                                            str5 = pair4.Value;
                                        }
                                    }
                                    int num = (data.Length - 3) - str3.Length;
                                    for (int i = 0; i < num; i++)
                                    {
                                        str5 = str5 + "0";
                                    }
                                    str5 = str5 + str3 + ".log";
                                    HashSet<string> set2 = new HashSet<string>(pair.Value);
                                    HashSet<string> set3 = new HashSet<string>();
                                    foreach (string str6 in set2)
                                    {
                                        if (string.Compare(str6, str5, true) == 0)
                                        {
                                            break;
                                        }
                                        set3.Add(str6);
                                    }
                                    set.Add(new KeyValuePair<string, HashSet<string>>(key, set3));
                                    break;
                                }
                            }
                            break;
                        }
                    }
                }
            }
            catch (Exception exception)
            {
                Console.WriteLine("Exception in CollectValidatedLogFiles(). Message = {0}", exception.Message);
                Trace.WriteLine(string.Format("Exception in CollectValidatedLogFiles(). Message = {0}", exception.Message));
                set.Clear();
            }
            Trace.WriteLine("   DEBUG   EXITED CollectValidatedLogFiles()", DateTime.Now.ToString());
            return set;
        }

        public void PersistLogFilesForFlush(List<IntegrityResult> resultList, HashSet<KeyValuePair<string, string>> volumeVsnapHash, string appName, string srcServerName, string virServerName, string seqNumber, List<SgDbMap> storageDbList, HashSet<KeyValuePair<string, string>> srcLogPath, HashSet<KeyValuePair<string, string>> logPathChkPrefix, string exchangeEseutilPath)
        {
            Trace.WriteLine("   DEBUG   ENTERED PersistLogFilesForFlush()", DateTime.Now.ToString());
            List<KeyValuePair<string, string>> logRequiredList = new List<KeyValuePair<string, string>>();
            HashSet<KeyValuePair<string, HashSet<string>>> logfilesForFlush = new HashSet<KeyValuePair<string, HashSet<string>>>();
            HashSet<KeyValuePair<string, HashSet<string>>> validatedLogFiles = new HashSet<KeyValuePair<string, HashSet<string>>>();
            string esePath = exchangeEseutilPath;
            Console.WriteLine("\n***** Started Retrieving Exchange Logs for Flush");
            try
            {
                this.CollectSGListForFlush(resultList, storageDbList, out this.storageGpForFlush);
                if (this.storageGpForFlush.Count != 0)
                {
                    logRequiredList = this.CollectEdbLogRequiredField(appName, this.storageGpForFlush, volumeVsnapHash, esePath);
                    if (logRequiredList.Count == 0)
                    {
                        Console.WriteLine("No log files for Flush");
                        Trace.WriteLine("No Log Files For flush");
                    }
                    else
                    {
                        HashSet<KeyValuePair<string, string>> volVsnapHash = new HashSet<KeyValuePair<string, string>>();
                        this.exchConsObj.CreateSgEdbVsnapMap(storageDbList, volumeVsnapHash);
                        volVsnapHash = this.exchConsObj.tgtLogPathVsnapPath;
                        logfilesForFlush = this.CollectLogFiles(this.storageGpForFlush, volVsnapHash, logPathChkPrefix);
                        if (logfilesForFlush.Count != 0)
                        {
                            validatedLogFiles = this.CollectValidatedLogFiles(logfilesForFlush, logRequiredList, this.storageGpForFlush, logPathChkPrefix);
                            if (validatedLogFiles.Count != 0)
                            {
                                string sourceServerName = srcServerName;
                                string evsName = virServerName;
                                string str4 = seqNumber;
                                if (!this.exchXmlObj.PersistLogFilesIntoXML(validatedLogFiles, sourceServerName, evsName, str4, srcLogPath))
                                {
                                    return;
                                }
                            }
                            Trace.WriteLine("   DEBUG   EXITED PersistLogFilesForFlush()", DateTime.Now.ToString());
                        }
                    }
                }
                else
                {
                    Console.WriteLine("\n[Error] : No Log files are applicable for Log Flush");
                    Console.WriteLine("Please issue another tag and try running Exchange Validation");
                    Trace.WriteLine("[Error] : No Log files are applicable for Log Flush");
                }
            }
            catch (Exception exception)
            {
                Console.WriteLine("Exception in PersistLogFilesForFlush(). Message = {0}", exception.Message);
                Trace.WriteLine(string.Format("Exception in PersistLogFilesForFlush(). Message = {0}", exception.Message));
            }
        }

        private string RunEseutilForLogRequiredCollection(string edbFileVsnapPath, string eseutilPath)
        {
            Trace.WriteLine("   DEBUG   ENTERED RunEseutilForLogRequiredCollection()", DateTime.Now.ToString());
            string str = "";
            try
            {
                string procArg = "";
                procArg = procArg + " /mh \"" + edbFileVsnapPath + "\"";
                Trace.WriteLine("   " + edbFileVsnapPath, "Run Eseutil For DB  ");
                string fileName = eseutilPath;
                ProcessStartInfo processInit = this.exchObj.GetProcessInit(fileName, procArg);
                int num = this.exchObj.StartProcess(processInit, true, "./eseutilOut.txt");
                Console.WriteLine("\n*****Eseutil LogRequired Process");
                Console.WriteLine("     EDB Path : {0}", edbFileVsnapPath);
                Trace.WriteLine("   " + edbFileVsnapPath, "EDB Path     ");
                Console.WriteLine("     Exitcode : {0}", num);
                Trace.WriteLine("   " + num, "Exitcode        ");
                if (num != 0)
                {
                    str = "";
                }
                if (File.Exists("./eseutilOut.txt"))
                {
                    foreach (string str4 in File.ReadAllLines("./eseutilOut.txt"))
                    {
                        if (str4.Contains("Log Required"))
                        {
                            str = str4;
                            Console.WriteLine("{0}", str4);
                            Trace.WriteLine("    " + str4);
                        }
                    }
                }
                else
                {
                    Console.WriteLine("\n[Error] : Redirect file to find Log required not found");
                    Trace.WriteLine("[Error] : Redirect file to find Log required not found");
                    str = "";
                }
                if (File.Exists("./eseutilOut.txt"))
                {
                    File.Delete("./eseutilOut.txt");
                }
            }
            catch (Exception exception)
            {
                Console.WriteLine("Exception in RunEseutilForLogRequiredCollection(). Message = {0}", exception.Message);
                Trace.WriteLine(string.Format("Exception in RunEseutilForLogRequiredCollection(). Message = {0}", exception.Message));
                str = "";
            }
            Trace.WriteLine("   DEBUG   EXITED RunEseutilForLogRequiredCollection()", DateTime.Now.ToString());
            return str;
        }
    }
}

