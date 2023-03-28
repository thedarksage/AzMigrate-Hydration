using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml.Serialization;
using System.IO.Compression;
using System.IO;
using LinuxCommunicationFramework;
using System.Diagnostics;

namespace RemoteLogCollection_Linux
{
    class AgentLogs
    {
        internal int CollectLogs(string ipaddress, string userName, string password, uint port)
        {
            LinuxClient lHelper = null;
            string supportFilePath = "/var/log/support_material.log";
            bool flag = true;
            try
            {
                try
                {
                    SSHTargetConnectionParams RemoteConn = new SSHTargetConnectionParams();
                    RemoteConn.AuthType = SSHTargetAuthType.Password;
                    RemoteConn.AuthValue = password;
                    RemoteConn.User = userName;
                    RemoteConn.CommunicationIP = ipaddress;
                    RemoteConn.CommunicationPort = port;

                    lHelper = LinuxClient.GetInstance(RemoteConn);
                }
                catch (Exception ex)
                {
                    Trace.WriteLine(ex.Message);
                    return Constants.UnableToConnect;
                }                
                
                string vxBinCmd = "grep INSTALLATION_DIR /usr/local/.vx_version | awk -F'=' '{print $2 }'";
                string vxBinPath = lHelper.ExecuteCommand(vxBinCmd);

                string vxAgenRoleCmd = "grep Role /usr/local/drscout.conf | awk -F'=' '{print $2 }'";
                string vxAgentRole = lHelper.ExecuteCommand(vxAgenRoleCmd);
                string agentRole = vxAgentRole == "Agent" ? "Source" : "MT";
                string hostNameCmd = "hostname";
                string hostName = lHelper.ExecuteCommand(hostNameCmd);
                hostName =  hostName.Trim();

                string zipName = hostName + "-" + agentRole + "-" + DateTime.Now.ToString("dd-MM-HH-mm-ss").ToString();

                string currentDir = Directory.GetCurrentDirectory();
                string folder = Path.Combine(currentDir, zipName);
                if (!Directory.Exists(folder)) Directory.CreateDirectory(folder);

                string zipPath = folder + @".zip";
                
                string command = "/bin/echo  y | " + vxBinPath.Trim() + "/bin/collect_support_materials.sh >/var/log/support_material.log 2>&1";
                lHelper.ExecuteCommand(command);
                if (lHelper.DoesFileExists(supportFilePath))
                {
                    lHelper.CopyFile(supportFilePath, folder, false, true, 300, false);
                    lHelper.RemoveFile(supportFilePath);
                }
                else
                {
                    Trace.WriteLine("Support material collection file path not found");

                }
                string supportLogPath = folder + @"\support_material.log";
                if (File.Exists(supportLogPath))
                {

                    string[] supportcontent = System.IO.File.ReadAllLines(supportLogPath);
                    int length = supportcontent.Length;
                    string filename = supportcontent[length - 1];
                    string[] LineArray = filename.Split(new string[] { " " }, StringSplitOptions.RemoveEmptyEntries);
                    string supportTarFileName = "";
                    if (LineArray.Length == 5)
                        supportTarFileName = LineArray[4];
                    if (filename.StartsWith("Compressed support materials file"))
                    {
                        lHelper.CopyFile(supportTarFileName, folder, false, true, 300, false);
                        lHelper.RemoveFile(supportTarFileName);
                    }

                }
                else
                {
                    Trace.WriteLine("Support material collection file path not found on local machine");

                }

                ZipFile.CreateFromDirectory(folder, zipPath);
                Directory.Delete(folder, flag);
                
            }
            catch (Exception ex)
            {
                Trace.WriteLine(ex.Message);
            }
            return Constants.SuccessReturnCode;
        }
     
    }

}

