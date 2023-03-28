namespace ExchangeValidation
{
    using System;
    using System.Collections.Generic;
    using System.Diagnostics;
    using System.IO;
    using System.Linq;
    using System.Text;

    internal class ExchangeVsnap
    {
        private Exchange exObj = new Exchange();
        public HashSet<KeyValuePair<string, string>> vsnapList = new HashSet<KeyValuePair<string, string>>();

        public bool CheckForMountedVsnapAndUnmount(string cdpPath, string volume, string vsnapLoc, bool unmount)
        {
            Trace.WriteLine("   DEBUG   ENTERED CheckForMountedVsnapAndUnmount()", DateTime.Now.ToString());
            bool flag = true;
            string str = vsnapLoc;
            string str2 = " --vsnap --op=list --target=";
            str2 = str2 + volume;
            try
            {
                ProcessStartInfo processInit = this.exObj.GetProcessInit(cdpPath, str2.ToString());
                int num = this.exObj.StartProcess(processInit, true, "./listvsnap.txt");
                Console.WriteLine("\n      Executing cdpcli cmd = {0}{1}", cdpPath, str2);
                Trace.WriteLine("  " + cdpPath + str2, "Execute cdpcli cmd   ");
                Console.WriteLine("      Listvsnap Volume :  \"{0}\" , Exitcode : {1}", volume, num);
                Trace.WriteLine(string.Concat(new object[] { "  ", volume, " , ", num }), "Exitcode            ");
                if (num != 0)
                {
                    return false;
                }
                if (File.Exists("./listvsnap.txt"))
                {
                    StreamReader reader = new StreamReader("./listvsnap.txt");
                    while (!reader.EndOfStream)
                    {
                        string str3 = reader.ReadLine();
                        if (!string.IsNullOrEmpty(str3))
                        {
                            string[] source = str3.Split(new char[] { ')' }, 2);
                            if (source.Count<string>() > 1)
                            {
                                string str4 = source[1].Trim();
                                if (str4.ToLower().Contains(str.ToLower()))
                                {
                                    Console.WriteLine("      ==> Found Vsnap = {0}", str4);
                                    Trace.WriteLine("   " + str4, "Found Vsnap   ");
                                    if (unmount)
                                    {
                                        Console.WriteLine("      ==> Cleanup Existing Vsnap");
                                        Console.WriteLine("Unmounting exist vsnap for volume \"{0}\"", str4);
                                        if (!this.UnMountVsnap(cdpPath, str4))
                                        {
                                            Console.WriteLine("[Warning] : Failed to unmount existing vsnap");
                                        }
                                    }
                                    this.vsnapList.Add(new KeyValuePair<string, string>(volume, str4));
                                }
                            }
                        }
                    }
                    reader.Close();
                }
                else
                {
                    Console.WriteLine("\n[Error] : Redirect file to check mounted vsnap not found");
                    Trace.WriteLine("[Error] : Redirect file to check mounted vsnap not found");
                }
                if (File.Exists("./listvsnap.txt"))
                {
                    File.Delete("./listvsnap.txt");
                }
                if (this.vsnapList.Count == 0)
                {
                    Console.WriteLine("      Found No Existing vsnap");
                    return flag;
                }
            }
            catch (Exception exception)
            {
                Console.WriteLine("Exception in CheckForMountedVsnapAndUnmount(). Message = {0}", exception.Message);
                Trace.WriteLine(string.Format("Exception in CheckForMountedVsnapAndUnmount(). Message = {0}", exception.Message));
                flag = false;
            }
            Trace.WriteLine("   DEBUG   EXITED CheckForMountedVsnapAndUnmount()", DateTime.Now.ToString());
            return flag;
        }

        public bool CheckVsnap(string cdpPath, HashSet<string> logdbVsnapPath)
        {
            Trace.WriteLine("   DEBUG   ENTERED CheckVsnap()", DateTime.Now.ToString());
            bool flag = true;
            bool flag2 = false;
            Console.WriteLine("\n***** Checking for mounted vsnap");
            try
            {
                string procArg = "";
                procArg = procArg + " --vsnap --op=list\"";
                Trace.WriteLine("   " + cdpPath + procArg, "CheckVsnap Command  ");
                ProcessStartInfo processInit = this.exObj.GetProcessInit(cdpPath, procArg);
                int num = this.exObj.StartProcess(processInit, true, "./checkVsnap.txt");
                Trace.WriteLine("   " + num, "Vsnap result        ");
                if (num != 0)
                {
                    return false;
                }
                if (File.Exists("./checkVsnap.txt"))
                {
                    foreach (string str2 in logdbVsnapPath)
                    {
                        StreamReader reader = new StreamReader("./checkVsnap.txt");
                        while (!reader.EndOfStream)
                        {
                            if (reader.ReadLine().Contains(str2.ToLower()))
                            {
                                Console.WriteLine("     [SUCCESS] {0}{1}", str2, " = FOUND");
                                Trace.WriteLine("   " + str2, "Vsnap Found         ");
                                flag2 = true;
                                break;
                            }
                            flag2 = false;
                        }
                        if (!flag2)
                        {
                            Console.WriteLine("     [FAILURE] {0}{1}", str2, " = NOTFOUND");
                            Trace.WriteLine("   " + str2, "Vsnap NotFound      ");
                            flag = false;
                        }
                        reader.Close();
                    }
                }
                else
                {
                    Console.WriteLine("\n[Error] : Redirect file to check mounted vsnap not found");
                    Trace.WriteLine("[Error] : Redirect file to check mounted vsnap not found");
                }
                File.Delete("./checkVsnap.txt");
            }
            catch (Exception exception)
            {
                Console.WriteLine("Exception in CheckVsnap(). Message = {0}", exception.Message);
                Trace.WriteLine(string.Format("Exception in CheckVsnap(). Message = {0}", exception.Message));
                flag = false;
            }
            Trace.WriteLine("   DEBUG   EXITED CheckVsnap()", DateTime.Now.ToString());
            return flag;
        }

        private bool CollectValidatedSgDbList(List<IntegrityResult> validResult, HashSet<KeyValuePair<string, string>> volumeTagPair, string volume, string path, ref HashSet<KeyValuePair<KeyValuePair<string, string>, string>> validFalseList, bool bIsValidDB)
        {
            bool flag = true;
            try
            {
                foreach (KeyValuePair<string, string> pair in volumeTagPair)
                {
                    if (string.Compare(volume, pair.Key, true) != 0)
                    {
                        continue;
                    }
                    string str = pair.Value;
                    if (bIsValidDB)
                    {
                        foreach (IntegrityResult result in validResult)
                        {
                            if (string.Compare(path, result.dbFile, true) == 0)
                            {
                                if (string.Compare(result.dbIntegrity, "true", true) != 0)
                                {
                                    validFalseList.Add(new KeyValuePair<KeyValuePair<string, string>, string>(new KeyValuePair<string, string>(path, volume), str));
                                }
                                return flag;
                            }
                        }
                        return flag;
                    }
                    foreach (IntegrityResult result2 in validResult)
                    {
                        if (string.Compare(path, result2.logFile, true) == 0)
                        {
                            if (string.Compare(result2.logIntegrity, "true", true) != 0)
                            {
                                validFalseList.Add(new KeyValuePair<KeyValuePair<string, string>, string>(new KeyValuePair<string, string>(path, volume), str));
                            }
                            return flag;
                        }
                    }
                    return flag;
                }
                return flag;
            }
            catch (Exception exception)
            {
                Console.WriteLine("Exception in CollectValidatedSgDbList(). Message = {0}", exception.Message);
                Trace.WriteLine(string.Format("Exception in CollectValidatedSgDbList(). Message = {0}", exception.Message));
                flag = false;
            }
            return flag;
        }

        public HashSet<string> CollectVsnapPath(HashSet<KeyValuePair<string, string>> logVsnap, HashSet<KeyValuePair<string, string>> dbVsnap)
        {
            Trace.WriteLine("   DEBUG   ENTERED CollectVsnapPath()", DateTime.Now.ToString());
            HashSet<string> set = new HashSet<string>();
            try
            {
                foreach (KeyValuePair<string, string> pair in logVsnap)
                {
                    set.Add(pair.Value);
                }
                foreach (KeyValuePair<string, string> pair2 in dbVsnap)
                {
                    set.Add(pair2.Value);
                }
            }
            catch (Exception exception)
            {
                Console.WriteLine("Exception in CollectVsnapPath(). Message = {0}", exception.Message);
                Trace.WriteLine(string.Format("Exception in CollectVsnapPath(). Message = {0}", exception.Message));
                set.Clear();
            }
            Trace.WriteLine("   DEBUG   EXITED CollectVsnapPath()", DateTime.Now.ToString());
            return set;
        }

        private string ConstructMountCommand(string target, string virPath, string tag)
        {
            Trace.WriteLine("   DEBUG   ENTERED ConstructMountCommand()", DateTime.Now.ToString());
            StringBuilder builder = new StringBuilder();
            try
            {
                builder.Append(" --vsnap --op=mount --target=\"");
                builder.Append(target);
                builder.Append("\"");
                builder.Append(" --virtual=\"");
                builder.Append(virPath);
                builder.Append("\"");
                builder.Append(" --flags=rw");
                builder.Append(" --datalogpath=\"datalogpath\"");
                builder.Append(" --event=");
                builder.Append(tag);
                Trace.WriteLine("  " + builder.ToString(), "Mount command       ");
            }
            catch (Exception exception)
            {
                Console.WriteLine("Exception in ConstructMountCommand(). Message = {0}", exception.Message);
                Trace.WriteLine(string.Format("Exception in ConstructMountCommand(). Message = {0}", exception.Message));
            }
            Trace.WriteLine("   DEBUG   EXITED ConstructMountCommand()", DateTime.Now.ToString());
            return builder.ToString();
        }

        private string ConstructUnmountCommand(string virtualDrive)
        {
            string str = " --vsnap --force=yes --virtual=\"";
            return (str + virtualDrive + "\" --op=unmount");
        }

        ~ExchangeVsnap()
        {
        }

        public bool FindValidatedTagAndMarkAsVerified(List<IntegrityResult> validResult, HashSet<KeyValuePair<KeyValuePair<string, string>, HashSet<KeyValuePair<string, string>>>> validSgDbMap, HashSet<KeyValuePair<string, string>> volumeTagPair, List<KeyValuePair<string, List<string>>> validatedSgDbList, string appType)
        {
            Trace.WriteLine("   DEBUG   ENTERED FindValidatedTagAndMarkAsVerified()", DateTime.Now.ToString());
            bool flag = true;
            HashSet<KeyValuePair<KeyValuePair<string, string>, string>> validFalseList = new HashSet<KeyValuePair<KeyValuePair<string, string>, string>>();
            Console.WriteLine("\n***** Finding the verified tag");
            try
            {
                new List<string>();
                HashSet<KeyValuePair<string, string>> set2 = new HashSet<KeyValuePair<string, string>>();
                string volume = "";
                string path = "";
                string str3 = "";
                string key = "";
                foreach (KeyValuePair<KeyValuePair<string, string>, HashSet<KeyValuePair<string, string>>> pair in validSgDbMap)
                {
                    path = pair.Key.Key;
                    volume = pair.Key.Value;
                    set2 = pair.Value;
                    this.CollectValidatedSgDbList(validResult, volumeTagPair, volume, path, ref validFalseList, false);
                    foreach (KeyValuePair<string, string> pair2 in set2)
                    {
                        str3 = pair2.Value;
                        key = pair2.Key;
                        this.CollectValidatedSgDbList(validResult, volumeTagPair, str3, key, ref validFalseList, true);
                    }
                }
                HashSet<string> volumeList = new HashSet<string>();
                this.GetVolumesList(validatedSgDbList, validSgDbMap, ref volumeList);
                HashSet<KeyValuePair<KeyValuePair<string, string>, string>> set4 = new HashSet<KeyValuePair<KeyValuePair<string, string>, string>>();
                foreach (string str5 in volumeList)
                {
                    foreach (KeyValuePair<string, string> pair3 in volumeTagPair)
                    {
                        bool flag2 = true;
                        string str6 = "";
                        if (string.Compare(str5, pair3.Key, true) == 0)
                        {
                            foreach (KeyValuePair<KeyValuePair<string, string>, string> pair4 in validFalseList)
                            {
                                if (string.Compare(pair4.Key.Value, pair3.Key, true) == 0)
                                {
                                    flag2 = false;
                                }
                                else
                                {
                                    flag2 = true;
                                }
                            }
                            if (flag2)
                            {
                                str6 = str6 + pair3.Key + ";";
                                set4.Add(new KeyValuePair<KeyValuePair<string, string>, string>(new KeyValuePair<string, string>(str5, pair3.Value), str6));
                            }
                        }
                    }
                }
                Console.WriteLine("\n***** Started marking the validated tag as verified");
                foreach (KeyValuePair<KeyValuePair<string, string>, string> pair5 in set4)
                {
                    Console.WriteLine("\nVolume = {0}", pair5.Key.Key);
                    Console.WriteLine("Tag = {0}", pair5.Key.Value);
                    if (pair5.Key.Value.Contains(":"))
                    {
                        this.MarkTagVerified(this.exObj.inMageCdpCliPath, pair5.Key.Key, appType, pair5.Key.Value, false, pair5.Value);
                    }
                    else
                    {
                        this.MarkTagVerified(this.exObj.inMageCdpCliPath, pair5.Key.Key, appType, pair5.Key.Value, true, pair5.Value);
                    }
                }
            }
            catch (Exception exception)
            {
                Console.WriteLine("Exception in FindValidatedTagAndMarkAsVerified(). Message = {0}", exception.Message);
                Trace.WriteLine(string.Format("Exception in FindValidatedTagAndMarkAsVerified(). Message = {0}", exception.Message));
                flag = false;
            }
            Trace.WriteLine("   DEBUG   EXITED FindValidatedTagAndMarkAsVerified()", DateTime.Now.ToString());
            return flag;
        }

        private void GetVolumesList(List<KeyValuePair<string, List<string>>> validatedSgDbList, HashSet<KeyValuePair<KeyValuePair<string, string>, HashSet<KeyValuePair<string, string>>>> validSgDbMap, ref HashSet<string> volumeList)
        {
            List<string> list = new List<string>();
            foreach (KeyValuePair<string, List<string>> pair in validatedSgDbList)
            {
                list = pair.Value;
                foreach (KeyValuePair<KeyValuePair<string, string>, HashSet<KeyValuePair<string, string>>> pair2 in validSgDbMap)
                {
                    if (string.Compare(pair.Key, pair2.Key.Key, true) == 0)
                    {
                        volumeList.Add(pair2.Key.Value);
                    }
                    foreach (string str in list)
                    {
                        foreach (KeyValuePair<string, string> pair3 in pair2.Value)
                        {
                            if (string.Compare(str, pair3.Key, true) == 0)
                            {
                                volumeList.Add(pair3.Value);
                            }
                        }
                    }
                }
            }
        }

        public bool MarkCommonTagAsVerified(List<KeyValuePair<string, List<string>>> validatedSgDbList, HashSet<KeyValuePair<KeyValuePair<string, string>, HashSet<KeyValuePair<string, string>>>> validSgDbMap, string appType, string eventTag)
        {
            bool flag = true;
            HashSet<string> volumeList = new HashSet<string>();
            Console.WriteLine("\n***** Started marking the validated tag as verified");
            try
            {
                this.GetVolumesList(validatedSgDbList, validSgDbMap, ref volumeList);
                foreach (string str in volumeList)
                {
                    Console.WriteLine("\nVolume = {0}", str);
                    Console.Write("Tag = {0}", eventTag);
                    this.MarkTagVerified(this.exObj.inMageCdpCliPath, str, appType, eventTag, true, str);
                }
            }
            catch (Exception exception)
            {
                Console.WriteLine("Exception in MarkCommonTagAsVerified(). Message = {0}", exception.Message);
                Trace.WriteLine(string.Format("Exception in MarkCommonTagAsVerified(). Message = {0}", exception.Message));
                flag = false;
            }
            return flag;
        }

        private bool MarkTagVerified(string cdpclipath, string volume, string app, string tag, bool bIsEventTag, string comment)
        {
            Trace.WriteLine("   DEBUG   ENTERED MarkTagVerified()", DateTime.Now.ToString());
            bool flag = true;
            try
            {
                StringBuilder builder = new StringBuilder();
                builder.Append(" --verifiedtag --vol=");
                builder.Append(volume);
                if (bIsEventTag)
                {
                    builder.Append(" --event=");
                }
                else
                {
                    builder.Append(" --time=");
                }
                if(tag.StartsWith("FileSystem"))
                    app = "FS";
                builder.Append(tag);
                builder.Append(" --app=");
                builder.Append(app);
                builder.Append(" --comment=\"Verified integrity of database/log for volumes ");
                builder.Append(comment);
                builder.Append("\"");
                Trace.WriteLine("   " + cdpclipath + builder, "MarkTagverified Command");
                Console.Write("\nExecuting the Command = {0}{1}", cdpclipath, builder);
                ProcessStartInfo processInit = this.exObj.GetProcessInit(cdpclipath, builder.ToString());
                int num = this.exObj.StartProcess(processInit, false, "");
                Trace.WriteLine("   " + num, "MarkTagVerified result");
                Console.WriteLine("Exitcode = {0}", num);
                if (num != 0)
                {
                    return false;
                }
            }
            catch (Exception exception)
            {
                Console.WriteLine("Exception in MarkTagVerified(). Message = {0}", exception.Message);
                Trace.WriteLine(string.Format("Exception in MarkTagVerified(). Message = {0}", exception.Message));
                flag = false;
            }
            Trace.WriteLine("   DEBUG   EXITED MarkTagVerified()", DateTime.Now.ToString());
            return flag;
        }

        public bool MountVsnap(string cdpExePath, string targetDrive, string virtualPath, int fileNum, string tag, ref string vsnapMntDrive)
        {
            Trace.WriteLine("   DEBUG   ENTERED MountVsnap()", DateTime.Now.ToString());
            bool flag = true;
            string fileName = cdpExePath;
            try
            {
                virtualPath = virtualPath + fileNum;
                if (this.exObj.CreateDir(virtualPath))
                {
                    if (!Directory.Exists(virtualPath))
                    {
                        Console.WriteLine("Failed to create directory to mount vsnap");
                        Trace.WriteLine("Failed to create directory to mount vsnap");
                        return false;
                    }
                    vsnapMntDrive = virtualPath;
                }
                else
                {
                    Console.WriteLine("Failed to create directory to mount vsnap");
                    Trace.WriteLine("Failed to create directory to mount vsnap");
                    return false;
                }
                if (string.IsNullOrEmpty(vsnapMntDrive))
                {
                    Console.WriteLine("\n[Error] : Failed to create directories to mount vsnap");
                    Trace.WriteLine("[Error] Failed to create directories to mount vsnap");
                    return false;
                }
                string str2 = this.ConstructMountCommand(targetDrive, vsnapMntDrive, tag);
                Trace.WriteLine("   " + fileName + " " + str2.ToString(), "Mount command       ");
                ProcessStartInfo processInit = this.exObj.GetProcessInit(fileName, str2.ToString());
                int num = this.exObj.StartProcess(processInit, false, "");
                Console.WriteLine("***Mount Vsnap Target Drive : \"{0}\" , Exitcode : {1}", targetDrive, num);
                Trace.WriteLine(string.Concat(new object[] { "   ", targetDrive, " , ", num }), "Mount result        ");
                if (num != 0)
                {
                    flag = false;
                }
            }
            catch (Exception exception)
            {
                Console.WriteLine("Exception in MountVsnap(). Message = {0}", exception.Message);
                Trace.WriteLine(string.Format("Exception in MountVsnap(). Message = {0}", exception.Message));
                flag = false;
            }
            Trace.WriteLine("   DEBUG   EXITED MountVsnap()", DateTime.Now.ToString());
            return flag;
        }

        public bool ReadRecoveryTagFromMountedVsnap(string cdpclipath, string targetvolume, ref string recoverytag, bool bReadTime)
        {
            Trace.WriteLine("   DEBUG   ENTERED ReadRecoveryTagFromMountedVsnap()", DateTime.Now.ToString());
            bool flag = false;
            try
            {
                string procArg = "";
                procArg = procArg + " --vsnap --op=list --verbose --target=\"" + targetvolume;
                Trace.WriteLine("   " + cdpclipath + procArg, "ReadTag Command     ");
                ProcessStartInfo processInit = this.exObj.GetProcessInit(cdpclipath, procArg);
                int num = this.exObj.StartProcess(processInit, true, "./recoveryTag.txt");
                Trace.WriteLine("   " + num, "ReadTag result        ");
                if (num != 0)
                {
                    return false;
                }
                if (File.Exists("./recoveryTag.txt"))
                {
                    StreamReader reader = new StreamReader("./recoveryTag.txt");
                    while (!reader.EndOfStream)
                    {
                        string str2 = reader.ReadLine();
                        if (str2.Contains("Recovery Tag"))
                        {
                            string[] strArray = str2.Split(new char[] { ':' });
                            recoverytag = strArray[1].Trim();
                            flag = true;
                            Trace.WriteLine("   " + recoverytag, "Recovery Tag        ");
                            break;
                        }
                    }
                    reader.Close();
                    if (bReadTime)
                    {
                        flag = false;
                    }
                    if (!flag)
                    {
                        StreamReader reader2 = new StreamReader("./recoveryTag.txt");
                        while (!reader2.EndOfStream)
                        {
                            string data = reader2.ReadLine();
                            if (data.Contains("Recovery Time"))
                            {
                                recoverytag = this.exObj.TrimData(data, ":", 1, 0).Trim();
                                flag = true;
                                Trace.WriteLine("   " + recoverytag, "Recovery Time        ");
                                break;
                            }
                        }
                        reader2.Close();
                    }
                }
                else
                {
                    Console.WriteLine("\n[Error] : Redirect file to read recovery tag not found");
                    Trace.WriteLine("[Error] : Redirect file to read recovery tag not found");
                }
                if (File.Exists("./recoveryTag.txt"))
                {
                    File.Delete("./recoveryTag.txt");
                }
            }
            catch (Exception exception)
            {
                Console.WriteLine("Exception in ReadRecoveryTagFromMountedVsnap(). Message = {0}", exception.Message);
                Trace.WriteLine(string.Format("Exception in ReadRecoveryTagFromMountedVsnap(). Message = {0}", exception.Message));
                flag = false;
            }
            Trace.WriteLine("   DEBUG   EXITED ReadRecoveryTagFromMountedVsnap()", DateTime.Now.ToString());
            return flag;
        }

        public bool UnMountVsnap(string cdpExePath, string virtualDrive)
        {
            Trace.WriteLine("   DEBUG   ENTERED UnMountVsnap()", DateTime.Now.ToString());
            bool flag = true;
            try
            {
                string procArg = this.ConstructUnmountCommand(virtualDrive);
                Console.WriteLine("     Executing the command  : {0}{1}", cdpExePath, procArg.ToString());
                Trace.WriteLine("   " + cdpExePath + procArg.ToString(), "UNMount command     ");
                ProcessStartInfo processInit = this.exObj.GetProcessInit(cdpExePath, procArg);
                int num = this.exObj.StartProcess(processInit, true, "unMount.txt");
                Console.WriteLine("     UnMount Vsnap Exitcode : {0}", num);
                Trace.WriteLine("   " + num, "UNMount result      ");
                if (num != 0)
                {
                    flag = false;
                }
            }
            catch (Exception exception)
            {
                Console.WriteLine("Exception in UnMountVsnap(). Message = {0}", exception.Message);
                Trace.WriteLine(string.Format("Exception in UnMountVsnap(). Message = {0}", exception.Message));
                flag = false;
            }
            Trace.WriteLine("   DEBUG   EXITED UnMountVsnap()", DateTime.Now.ToString());
            return flag;
        }
    }
}

