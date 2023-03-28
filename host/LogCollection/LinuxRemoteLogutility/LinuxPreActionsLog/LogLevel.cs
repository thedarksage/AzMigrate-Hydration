using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Diagnostics;
using System.IO;
using LinuxCommunicationFramework;

namespace LinuxPreActionLog
{
    class LogLevel
    {
        internal int LogLevelChange(string ipaddress, string userName, string password, uint port, int loglevel)
        {
            try
            {
                LinuxClient lHelper = null;
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
                string replaceString = "LogLevel = " + loglevel;
                string logLevelCmd = "sed -i 's/^LogLevel.*$/" + replaceString + "/g' /usr/local/drscout.conf";
                int logLevel = lHelper.ExecuteAndReturnErrCode(logLevelCmd);
                if (logLevel != 0)
                {
                    Trace.Write("Unable to update the log level");
                    return Constants.UnableToUpdateLogLevel;
                }
                string serviceCmd = "service vxagent restart";
                lHelper.ExecuteCommand(serviceCmd);
            }
            catch (Exception ex)
            {
                Trace.WriteLine(ex.Message);
            }
            return Constants.SuccessReturnCode;
        }

        internal int ServiceUpdate(string ipaddress, string userName, string password, uint port, string action)
        {
            try
            {
                LinuxClient lHelper = null;
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
                string serviceCmd = "service vxagent" + " " + action;
                lHelper.ExecuteCommand(serviceCmd);
            }
            catch (Exception ex)
            {
                Trace.WriteLine(ex.Message);
            }
            return Constants.SuccessReturnCode;
        }

    }
}
