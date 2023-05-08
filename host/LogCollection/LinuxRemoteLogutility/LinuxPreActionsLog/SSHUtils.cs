/*++

Copyright (c) 2014  Microsoft Corporation

Module Name:

SSHUtils.cs

Abstract:

This file provides the basic helper function to communicate with a linux system 
over SSH. The helpers have dependency on plink.exe and pscp.exe for establising
a secure channel with the Linux guest.

Author:

Dharmendra Modi(dmodi) 01-03-2014

--*/

using Microsoft.Win32;
using System;
using System.Diagnostics;
using System.Globalization;
using System.Net;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace LinuxCommunicationFramework
{
    /// <summary>
    /// Authentication Type to be used with the SSH Target. 
    /// Password based or Key based
    /// </summary>
    public enum SSHTargetAuthType
    {
        /// <summary>
        /// Use Password based SSH Authentication
        /// </summary>
        Password = 0,
        /// <summary>
        /// Use Key based SSH Authentication
        /// </summary>
        Key
    }

    /// <summary>
    /// 
    /// </summary>
    public class SSHTargetConnectionParams
    {
        /// <summary>
        /// User Name to be used for establishing the SSH Channel
        /// </summary>
        public string User { get; set; }

        /// <summary>
        /// Authentication mechanism to be used [Password OR SSH Key]
        /// </summary>
        public SSHTargetAuthType AuthType { get; set; }

        /// <summary>
        /// password for SSHTargetAuthType.Password
        /// private key path for SSHTargetAuthType.Key
        /// </summary>
        public string AuthValue { get; set; }

        /// <summary>
        /// IP Address to be used for communication
        /// </summary>
        public string CommunicationIP { get; set; }

        /// <summary>
        /// Port number to be used for communication
        /// </summary>
        public uint CommunicationPort { get; set; }

        /// <summary>
        /// Function to create a clone of the provided property
        /// </summary>
        /// <returns>SSHTargetConnectionParams</returns>
        public SSHTargetConnectionParams Clone()
        {
            var temp = new SSHTargetConnectionParams();
            temp.User = this.User;
            temp.AuthType = this.AuthType;
            temp.AuthValue = this.AuthValue;
            temp.CommunicationIP = this.CommunicationIP;
            temp.CommunicationPort = this.CommunicationPort;

            return temp;
        }
    }

    /// <summary>
    /// 
    /// </summary>
    public class SSHUtils
    {
        private string authParams;
        private string defaultArgs = "-batch";

        //TODO: Move to config
        private const string CopyExe = @"pscp.exe";
        private const string CommandExe = @"plink.exe";
        private const uint SSHDefaultPort = 22;
        private const string RSAKeyType = "rsa2";
        private const string RSAKeyRegKeyName = @"HKEY_CURRENT_USER\Software\SimonTatham\PuTTY\SshHostKeys";

        /// <summary>
        /// Connection Params
        /// </summary>
        public SSHTargetConnectionParams conParams { get; private set; }

        /// <summary>
        /// 
        /// </summary>
        public string CommunicationIP
        {
            get
            {
                return conParams.CommunicationIP;
            }
            set
            {
                IPAddress.Parse(value);
                conParams.CommunicationIP = value;

                //the mapping of ipaddress and RSA will change. Update it
                CacheRSAKey();
            }
        }

        /// <summary>
        /// Constructor
        /// </summary>
        /// <param name="sshTargetConnectionParams"></param>
        public SSHUtils(SSHTargetConnectionParams sshTargetConnectionParams)
        {
            Guard.RequiresNotNullOrWhiteSpace(sshTargetConnectionParams.CommunicationIP, "SSHTargetConnectionParams.CommunicationIP");
            Guard.RequiresNotNullOrWhiteSpace(sshTargetConnectionParams.User, "SSHTargetConnectionParams.User");
            Guard.RequiresNotNullOrWhiteSpace(sshTargetConnectionParams.AuthValue, "SSHTargetConnectionParams.AuthValue");

            StringBuilder errMsg = new StringBuilder();
            if (!CheckOrCopyExe(CommandExe))
            {
                errMsg.AppendFormat(CultureInfo.InvariantCulture, "Cannot find {0} in path\n", CommandExe);
            }
            if (!CheckOrCopyExe(CopyExe))
            {
                errMsg.AppendFormat(CultureInfo.InvariantCulture, "Cannot find {0} in path\n", CopyExe);
            }
            if (errMsg.Length != 0)
            {
                throw new Exception(errMsg.ToString());
            }

            conParams = sshTargetConnectionParams.Clone();
            if (conParams.CommunicationPort == 0)
            {
                conParams.CommunicationPort = SSHDefaultPort;
            }

            // Port need to be set first in Default args
            defaultArgs += GetPortString();

            this.CommunicationIP = sshTargetConnectionParams.CommunicationIP;

            SetAuthParams(sshTargetConnectionParams.AuthType, sshTargetConnectionParams.AuthValue);
        }

        #region public methods

        ///  <summary>
        ///  Executes a command on the remote machine.
        ///  
        ///  
        ///  </summary>
        ///  <param name="command"> command to be executed. Must be a valid Unix Command or a script. </param>
        /// <param name="executionTimeOutInSec">wait time for completion</param>
        ///  <returns>string representing stdout</returns> 
        /// 
        ///  <remarks>
        ///  This function will throw an exception containing the error message generated if any.
        ///  </remarks> 
        public string ExecuteCommand(string command, int executionTimeOutInSec)
        {
            Guard.RequiresNotNullOrWhiteSpace(command, "command");

            string args = string.Format(CultureInfo.InvariantCulture,
                "{0} {1} {2} {3}", defaultArgs, authParams, CommunicationIP, command);

            return StartProcessing(CommandExe, args, executionTimeOutInSec);
        }

        /// <summary>
        /// Copies a file to/from remote.
        /// </summary>
        /// <param name="srcFileName">source file name</param>
        /// <param name="destFileName">destination file name</param>
        /// <param name="toRemote">True in order to copy file to Remote machine else false</param>
        /// <param name="executionTimeOutInSec">wait time for completion</param>
        /// 
        /// <remarks>
        /// When copying file from Remote location Caller should make sure 
        /// Source File (remote file) and Destination folder (local folder)
        /// exists. Destination file will always be overwritten if exists.
        /// 
        /// This function will throw an exception containing the error message generated if any.
        /// </remarks> 
        public void CopyFile(string srcFileName, string destFileName, bool toRemote, int executionTimeOutInSec)
        {
            if (toRemote)
            {
                WindowsHelper.ValidateFilePath(srcFileName);
            }

            Copy(defaultArgs, srcFileName, destFileName, toRemote, executionTimeOutInSec);
        }

        /// <summary>
        /// Copies a directory recursively to/from remote.
        /// </summary>
        /// <param name="srcDir">source directory name</param>
        /// <param name="destDir">destination directory name</param>
        /// <param name="toRemote">True in order to copy to Remote machine else false</param>
        /// <param name="executionTimeOutInSec">wait time for completion</param>
        /// 
        /// <remarks>
        /// When copying directory to Remote location Caller should make sure 
        /// Remote Parent directory exists. If there is a directory with the 
        /// same name as srcDir it will be overwritten.
        /// 
        /// When copying directory from Remote location Caller should make sure 
        /// local Parent directory exists. If there is a directory with the 
        /// same name as destDir it will be overwritten.
        /// 
        /// This function will throw an exception containing the error message generated if any.
        /// </remarks> 
        public void CopyDirectory(string srcDir, string destDir, bool toRemote, int executionTimeOutInSec)
        {
            if (toRemote)
            {
                WindowsHelper.ValidateDirPath(srcDir);
            }
            else
            {
                WindowsHelper.ValidateDirPath(destDir);
            }

            Copy(defaultArgs + " -r ", srcDir, destDir, toRemote, executionTimeOutInSec);
        }
        #endregion public methods

        #region private methods

        private void CacheRSAKey()
        {
            try
            {
                //SSH uses RSA key for authenticating the *NIX system. This key is verified for every
                //communication. Call below is a hack to auto accept and cache the key. 
                //Connection to a *NIX machine machine provides a Machine Signature. SSH Client verifies
                //the signature and stores it in its cache.
                //Sending Y permits the client to cache the key.
                string cmd = string.Format(CultureInfo.InvariantCulture, "{0} {1}", GetPortString(), CommunicationIP);
                StartProcessing(CommandExe, cmd, 10, "Y");
            }
            catch (TimeoutException)
            {
                string keyName = string.Format(CultureInfo.InvariantCulture, @"{0}@{1}:{2}", RSAKeyType,
                    conParams.CommunicationPort, CommunicationIP);

                //verify the host system registry for the *NIX system RSA key
                string rsaKeyVal = (string)Registry.GetValue(RSAKeyRegKeyName, keyName, null);
                if (null == rsaKeyVal)
                    throw new Exception(string.Format(CultureInfo.InvariantCulture,
                        "Not able to found a valid *NIX system key in registry {0}\\{1}", RSAKeyRegKeyName, keyName));
            }
        }

        private void SetAuthParams(SSHTargetAuthType authType, string authValue)
        {
            switch (authType)
            {
                case SSHTargetAuthType.Password:
                    Guard.RequiresNotNullOrWhiteSpace(authValue, "authValue");
                    this.authParams = string.Format(CultureInfo.InvariantCulture,
                        "-l {0} -pw \"{1}\"", conParams.User, authValue);
                    break;
                case SSHTargetAuthType.Key:
                    WindowsHelper.ValidateFilePath(authValue);
                    this.authParams = string.Format(CultureInfo.InvariantCulture,
                        "-l {0} -i \"{1}\"", conParams.User, authValue);
                    break;
                default:
                    throw new Exception("Unsupported Authentication type " + authType);
            }
        }

        /// <summary>
        /// usage To Remote : localSourceFile RemoteHostIP:RemoteFile
        /// usage From Remote : RemoteHostIP:RemoteSourceFile localDestinationFile
        /// </summary>
        /// <returns>string representing the required copy argument</returns>
        private string FormCopyArgs(string source, string destination, bool toRemote)
        {
            if (toRemote)
            {
                return string.Format(CultureInfo.InvariantCulture,
                        "\"{0}\" {1}:\"{2}\"", source, CommunicationIP, destination);
            }
            else
            {
                return string.Format(CultureInfo.InvariantCulture,
                        "{0}:\"{1}\" \"{2}\"", CommunicationIP, source, destination);
            }
        }

        private string StartProcessing(string exePath, string args, int executionTimeOutInSec, string inputToProcess)
        {
            int timeOutInMs = executionTimeOutInSec;
            if (executionTimeOutInSec != Timeout.Infinite)
            {
                timeOutInMs *= 1000;
            }

            using (Process process = new Process())
            {
                //check to make sure we have to pass input to the created process
                bool redirectStdIn = !string.IsNullOrWhiteSpace(inputToProcess);

                process.StartInfo.FileName = exePath;
                process.StartInfo.Arguments = args;
                process.StartInfo.UseShellExecute = false;
                process.StartInfo.RedirectStandardOutput = true;
                process.StartInfo.RedirectStandardError = true;
                process.StartInfo.CreateNoWindow = true;

                if (redirectStdIn)
                {
                    process.StartInfo.RedirectStandardInput = true;
                }

                process.Start();

                //writing the provided input to process standard input.
                if (redirectStdIn)
                {
                    process.StandardInput.WriteLine(inputToProcess);
                }

                //create tasks to read the error and output streams
                Task<string>[] tasks = new Task<string>[2];
                tasks[0] = process.StandardError.ReadToEndAsync();
                tasks[1] = process.StandardOutput.ReadToEndAsync();

                if (!Task.WaitAll(tasks, timeOutInMs))
                {
                    process.Kill();
                    process.WaitForExit();
                    throw new TimeoutException("Waiting for Stream Read to complete");
                }

                if (!process.WaitForExit(timeOutInMs))
                {
                    process.Kill();
                    process.WaitForExit();
                    throw new TimeoutException("Waiting for Process to complete");
                }

                //Read the streams for result

                if (!string.IsNullOrEmpty(tasks[0].Result))
                {
                    throw new Exception(tasks[0].Result);
                }

                return tasks[1].Result;
            }
        }

        private string StartProcessing(string exePath, string args, int executionTimeOutInSec)
        {
            return StartProcessing(exePath, args, executionTimeOutInSec, string.Empty);
        }

        private void Copy(string args, string source, string destination, bool toRemote, int executionTimeOutInSec)
        {
            string copyArgs = FormCopyArgs(source, destination, toRemote);
            args += string.Format(CultureInfo.InvariantCulture,
                " {0} {1}", authParams, copyArgs);

            StartProcessing(CopyExe, args, executionTimeOutInSec);
        }

        private bool CheckOrCopyExe(string exe)
        {
            //Check if exe exists in current directory
            if (WindowsHelper.DoesFileExists(exe))
            {
                return true;
            }
            else
            {
                //TODO : Copy file from share to the current directory
                return false;
            }
        }

        private string GetPortString()
        {
            return " -P " + conParams.CommunicationPort.ToString();
        }

        #endregion private methods
    }
}
