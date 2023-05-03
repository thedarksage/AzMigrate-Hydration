using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Diagnostics;
using System.IO;
using System.Threading.Tasks;
using System.Diagnostics.Eventing.Reader;
using Microsoft.Win32;


namespace LogsCollection
{
    class Backend
    {
        public static bool ExecuteCommand(string command, string folderPath)
        {
            try
            {
                ProcessStartInfo pinfo = new ProcessStartInfo("cmd.exe", "/c " + command);
                pinfo.CreateNoWindow = true;
                pinfo.RedirectStandardOutput = true;
                pinfo.UseShellExecute = false;

                Process process = new Process();
                process.StartInfo = pinfo;
                process.Start();

                //Writing the output to textfile
                StreamReader sreader = process.StandardOutput;
                string result = sreader.ReadToEnd();
                StreamWriter sw = File.CreateText(folderPath);
                sw.WriteLine(result);
                sw.Close();
                return true;
            }
            catch (Exception ex)
            {
                Trace.Write(ex.Message);
                return false;
            }
           
        }
        static internal string GetRegPath(string regPath, string key)
        {
            RegistryKey regKey = Registry.LocalMachine.OpenSubKey(regPath);
            if (regKey != null)
            {
                string regValue = (string)regKey.GetValue(key);
                return regValue;
            }
            else
            {
                return null;
            }
        }

        static public void CopyFiles(string sourceFolder, string targetFolder, string searchPattern)
        {
            try
            {
                if (!Directory.Exists(targetFolder))
                {
                    Directory.CreateDirectory(targetFolder);
                }

                string[] files = Directory.GetFiles(sourceFolder, searchPattern);

                foreach (string file in files)
                {
                    try
                    {
                        string fileName = Path.GetFileName(file);
                        File.Copy(file, Path.Combine(targetFolder, fileName));
                    }
                    catch (Exception ex)
                    {

                        Trace.Write("CopyFiles: " + ex.Message);
                    }
                }
                        
            }
            catch (Exception ex)
            {

                Trace.Write("CopyFiles: "+ ex.Message);
            }

        }

        static public void CopyFolder(string sourceFolder, string destFolder)
        {
            if (Directory.Exists(sourceFolder))
            {
                if (!Directory.Exists(destFolder))
                    Directory.CreateDirectory(destFolder);
                string[] files = Directory.GetFiles(sourceFolder);
                foreach (string file in files)
                {
                    string name = Path.GetFileName(file);
                    string dest = Path.Combine(destFolder, name);
                    File.Copy(file, dest);
                }
                string[] folders = Directory.GetDirectories(sourceFolder);
                foreach (string folder in folders)
                {
                    string name = Path.GetFileName(folder);
                    string dest = Path.Combine(destFolder, name);
                    CopyFolder(folder, dest);
                }
            }
        }

        static internal void CollectEventLogs(string logName, string path, string eventVwr)
        {
            try
            {
                EventLogSession els = new EventLogSession();
                string folder = Path.Combine(path, eventVwr);
                els.ExportLogAndMessages(logName, PathType.LogName, "*", folder);
            }
            catch (Exception ex)
            {

                Trace.Write(ex.Message);
            }

        }
        
        internal static string[] CredentialsMasking(string path)
        {
            string maskString = "\"#########\"";
            string[] credentials = new string[2];

            if (File.Exists(path))
            {
                var lines = File.ReadAllLines(path);
                int lineNumber = 0;
                string modifiedLine = null;
                foreach (string linetoEdit in lines)
                {
                    if (linetoEdit != null && (linetoEdit.StartsWith("DB_ROOT_USER") || linetoEdit.StartsWith("DB_ROOT_PASSWD") || linetoEdit.StartsWith("DB_USER") || linetoEdit.StartsWith("DB_PASSWD") || linetoEdit.StartsWith("CXPS_USER_WINDOWS") || linetoEdit.StartsWith("CXPS_PASSWORD_WINDOWS") || linetoEdit.StartsWith("CXPS_USER_LINUX") || linetoEdit.StartsWith("CXPS_PASSWORD_LINUX") || linetoEdit.StartsWith("SSL_CERT_PASSWORD") || linetoEdit.StartsWith("SSL_KEY_PASSWORD")))
                    {
                        string[] lineEditsplit = linetoEdit.Split('=');

                        if (lineEditsplit.Length == 2)
                        {
                            if (linetoEdit.StartsWith("DB_ROOT_USER"))
                            {
                                credentials[0] = lineEditsplit[1].Replace("\"","");
                                credentials[0] = credentials[0].Trim();
                            }
                            else if (linetoEdit.StartsWith("DB_ROOT_PASSWD"))
                            {
                                credentials[1] = lineEditsplit[1].Replace("\"", "");
                                credentials[1] = credentials[1].Trim();
                            }
                            modifiedLine = lineEditsplit[0] + "=" + maskString;
                         
                        }

                    }
                    else
                    {
                        modifiedLine = null;
                    }
                    if (modifiedLine != null)
                    {
                        lines[lineNumber] = modifiedLine;
                        try
                        {
                            File.WriteAllLines(path, lines);
                        }
                        catch (Exception ex)
                        {
                              Trace.WriteLine("\t Failed to Write a file " + ex.Message);
                         }
                     }

                    lineNumber++;
                }

            }
            else
            {
                Trace.WriteLine(path + "\t file not found ");
            }
            return credentials;
        }

     }
}
