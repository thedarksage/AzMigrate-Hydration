/*++

Copyright (c) 2014  Microsoft Corporation

Module Name:

LinuxClient.cs

Abstract:

This file provides the Linux Uility functions like getTime, getIP, etc.

Note : LinuxClient provides generic functionality while commnunicating with Linux Remote Machine.
 * Please do not modify the file in case of any Linux flavor/version specific tasks.
 * Instead Create a new class inheriting LinuxClient and override the method there.

Author:

Dharmendra Modi(dmodi) 01-03-2014

--*/

using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;

namespace LinuxCommunicationFramework
{
    /// <summary>
    /// specifies the status code returned by fsck.
    /// </summary>
    [Flags]
    public enum FsckResult
    {
        /// <summary>
        /// 0 - No errors
        /// </summary>
        Success = 0,

        /// <summary>
        /// 1 - File system errors corrected
        /// </summary>
        ErrorsCorrected = 1,

        /// <summary>
        /// 2 - System should be rebooted
        /// </summary>
        RebootRequired = 2,

        /// <summary>
        /// 4 - File system errors left uncorrected
        /// </summary>
        UncorrectedErrors = 4,

        /// <summary>
        /// 8 - Operational error
        /// </summary>
        OperationalError = 8,

        /// <summary>
        /// 16 - Usage or syntax error
        /// </summary>
        UsageError = 16,

        /// <summary>
        /// 32 - Fsck canceled by user request
        /// </summary>
        Cancelled = 32,

        /// <summary>
        /// 128 - Shared library error
        /// </summary>
        LibraryError = 64
    };

    /// <summary>
    /// Linux Process state
    /// </summary>
    public enum LinuxProcessState
    {
        /// <summary>
        /// R    running or runnable (on run queue)
        /// </summary>
        Running = 'R',

        /// <summary>
        /// S    interruptible sleep (waiting for an event to complete)
        /// </summary>
        Suspend = 'S',

        /// <summary>
        /// D    uninterruptible sleep (usually IO)
        /// </summary>
        Blocked = 'D',

        /// <summary>
        /// T    stopped, either by a job control signal or because it is being traced
        /// </summary>
        Stopped = 'T',

        /// <summary>
        /// Z    defunct ("zombie") process, terminated but not reaped by its parent
        /// </summary>
        Exited = 'Z',

        /// <summary>
        /// No corresponding mapping to *NIX indicates process completion
        /// </summary>
        Completed = 'C',

        /// <summary>
        /// Process Does Not Exists
        /// </summary>
        NotExists = -1
    }

    /// <summary>
    /// Supported Linux Filesystem
    /// </summary>
    public enum LinuxFileSystem
    {
        /// <summary>
        /// 
        /// </summary>
        ext2,

        /// <summary>
        /// 
        /// </summary>
        ext3,

        /// <summary>
        /// 
        /// </summary>
        ext4,

        /// <summary>
        /// 
        /// </summary>
        dosfs,

        /// <summary>
        /// 
        /// </summary>
        reiserfs
    }

    /// <summary>
    /// 
    /// </summary>
    public partial class LinuxClient
    {
        private SSHUtils sshClient;

        /// <summary>
        /// This a 5 mins constant timeout for any command executed using LinuxClient
        /// </summary>
        private const int CommandTimeOutInSecs = 5 * 60;

        /// <summary>
        /// Default prefix for relative path on *NIX server.
        /// </summary>
        public string UserHomeDir { get; private set; }

        /// <summary>
        /// Path separator for *NIX systems
        /// </summary>
        public char NixPathSeparator { get; private set; }

        #region public methods

        /// <summary>
        /// Updates the Communication IP for Remote.
        /// Usable in case of DHCP configuration where IP adress might get 
        /// changed due to lease expiration.
        /// Not needed for a static IP configuration
        /// </summary>
        /// <param name="communicationIP">New Ip address to be assigned</param>
        public void UpdateCommunicationIP(string communicationIP)
        {
            sshClient.CommunicationIP = communicationIP;
        }

        /// <summary>
        /// Convert a filepath to Unix Path.
        /// </summary>
        /// <param name="filePath">File Path</param>
        /// <returns>Equivalent file path in *NIX</returns>
        /// <example>
        /// If User Root is /root/xyz 
        /// 1. filePath="C:\abc" resolves to /root/xyz/C:/abc.
        /// 2. filePath="/user/user1" resolves to /user/user1 
        /// </example>
        /// NOTE : Not marking this function as virtual as it is used by almost all the functions in this class.
        /// If this is overridden all the function needs to be overriden by the class Inherting this class.
        public string ResolveNIXFilePath(string filePath)
        {
            UtilityFunctions.LogMethodEntry();

            Guard.RequiresNotNullOrWhiteSpace(filePath, "filePath");

            string resolvedPath;
            //Convert path to *NIX Path
            string transformedFilePath = filePath.Replace(Path.DirectorySeparatorChar, NixPathSeparator);
            if (filePath[0] == NixPathSeparator)
            {
                //Absolute Path
                resolvedPath = transformedFilePath;
            }
            else
            {
                //Relative Path
                //Unix Path can contain ":" hence ignoring the case
                resolvedPath = UserHomeDir + NixPathSeparator + transformedFilePath;
            }

            UtilityFunctions.LogDebug(string.Format(CultureInfo.InvariantCulture, "Resolving Filepath {0} to {1}", filePath, resolvedPath));
            return resolvedPath;
        }

        /// <summary>
        /// Function to check if a file exists on *NIX system
        /// </summary>
        /// <param name="filePath">file path on *NIX system</param>
        /// <param name="executionTimeOutInSec">wait time for completion</param>
        /// <returns>true if exists else false</returns>
        public virtual bool DoesFileExists(string filePath, int executionTimeOutInSec)
        {
            //Use "-f" parameter to test file existence
            return ExecuteTestCommand("-f", filePath, executionTimeOutInSec);
        }

        /// <summary>
        /// Function to check if a file exists on *NIX system
        /// </summary>
        /// <param name="filePath">file path on *NIX system</param>
        /// <returns>true if exists else false</returns>
        public virtual bool DoesFileExists(string filePath)
        {
            //Use "-f" parameter to test file existence
            return DoesFileExists(filePath, CommandTimeOutInSecs);
        }

        /// <summary>
        /// Function to check if a directory exists on *NIX system
        /// </summary>
        /// <param name="dirPath">directory path on *NIX system</param>
        /// <param name="executionTimeOutInSec">wait time for completion</param>
        /// <returns>true if exists else false</returns>
        public virtual bool DoesDirectoryExists(string dirPath, int executionTimeOutInSec)
        {
            //Use "-d" parameter to test directory existence
            return ExecuteTestCommand("-d", dirPath, executionTimeOutInSec);
        }

        /// <summary>
        /// Function to check if a directory exists on *NIX system
        /// </summary>
        /// <param name="dirPath">directory path on *NIX system</param>
        /// <returns>true if exists else false</returns>
        public virtual bool DoesDirectoryExists(string dirPath)
        {
            //Use "-d" parameter to test directory existence
            return DoesDirectoryExists(dirPath, CommandTimeOutInSecs);
        }

        /// <summary>
        /// Creates a directory on *NIX machine
        /// </summary>
        /// <param name="dirPath">Remote path to create directory</param>
        /// <param name="executionTimeOutInSec">wait time for completion</param>
        /// <remarks>
        /// If Absolute Path i.e. Path starting with "/" is not given the directory
        /// would be created relative to the User Root Directory. All missing 
        /// directories in the path would be created.
        /// </remarks>
        /// <example>
        /// If User Root is /root/xyz 
        /// 1. dirPath="C:\abc" creates /root/xyz/C:/abc directory
        /// 2. dirPath="/user/user1" creates /user/user1 directory
        /// 3. dirPath="C:\x\y\z\w" creates /root/x/y/z/w.
        /// </example>
        public virtual void CreateDirectory(string dirPath, int executionTimeOutInSec)
        {
            string nixFilePath = ResolveNIXFilePath(dirPath);
            if (!DoesDirectoryExists(dirPath))
            {
                string cmd = String.Format(CultureInfo.InvariantCulture,
                        "{0} \"{1}\"", LinuxCommands.RecursiveMakeDir, nixFilePath);
                try
                {
                    ExecuteCommandUsingSSH(cmd, executionTimeOutInSec);
                }
                catch (Exception ex)
                {
                    string errMsg = String.Format(CultureInfo.InvariantCulture,
                        "Cannot create Directory {0} on Remote Machine Exception {1} \n{2}",
                        dirPath, ex.Message, ex.StackTrace);
                    throw new Exception(errMsg);
                }
            }
        }

        /// <summary>
        /// Creates a directory on *NIX machine
        /// </summary>
        /// <param name="dirPath">Remote path to create directory</param>
        /// <remarks>
        /// If Absolute Path i.e. Path starting with "/" is not given the directory
        /// would be created relative to the User Root Directory. All missing 
        /// directories in the path would be created.
        /// </remarks>
        /// <example>
        /// If User Root is /root/xyz 
        /// 1. dirPath="C:\abc" creates /root/xyz/C:/abc directory
        /// 2. dirPath="/user/user1" creates /user/user1 directory
        /// 3. dirPath="C:\x\y\z\w" creates /root/x/y/z/w.
        /// </example>
        public virtual void CreateDirectory(string dirPath)
        {
            CreateDirectory(dirPath, CommandTimeOutInSecs);
        }

        /// <summary>
        /// Copies a directory recursively to/from remote.
        /// </summary>
        /// <param name="src">source directory name</param>
        /// <param name="dest">destination directory name</param>
        /// <param name="toRemote">True in order to copy to Remote 
        /// machine else false</param>
        /// <param name="overwrite"> Overwrite Destination if exists</param>
        /// <param name="executionTimeOutInSec">wait time for completion</param>
        /// <param name="doCRLFConversion">Takes care of CRLF conversion</param>
        /// <remarks>
        /// When copying directory to Remote location Caller should make sure 
        /// Remote Parent directory exists.
        /// 
        /// When copying directory from Remote location Caller should make sure 
        /// local Parent directory exists.
        /// 
        /// This function will throw an exception containing the error message
        /// generated if any.
        /// </remarks> 
        /// 
        /// <TODO> Create Task for Directory of large size</TODO>
        public void CopyDirectory(string src, string dest, bool toRemote, bool overwrite, int executionTimeOutInSec, bool doCRLFConversion)
        {
            if (!overwrite && IsFileOrDirExists(dest, toRemote))
            {
                throw new Exception("Destination Exists :: " + dest);
            }

            if (toRemote)
            {
                string destPath = ResolveNIXFilePath(dest);

                //parent directory is mandatory for copy directory to succeed
                CreateDirectory(destPath, executionTimeOutInSec);

                sshClient.CopyDirectory(src, destPath, toRemote, executionTimeOutInSec);
                //SSHClient is not taking care of CRLF conversion hence explicitly doing here
                if (doCRLFConversion)
                {
                    int errCode = ConvertDirectoryRecursivelyToUnixFormat(destPath);
                    if (errCode != 0)
                    {
                        try
                        {
                            //Conversion failed Delete the file and return error.
                            RemoveDirectory(destPath, true);
                        }
                        catch
                        {
                            //Suppress exception as it is a cleanup that failed
                        }

                        throw new Exception(string.Format(CultureInfo.InvariantCulture,
                            "Failed to convert directory {0} to Unix Format {1} errCode", src, errCode));
                    }
                }
            }
            else
            {
                sshClient.CopyDirectory(ResolveNIXFilePath(src), dest, toRemote, executionTimeOutInSec);
                if (doCRLFConversion)
                {
                    throw new Exception("CRLF Conversion not supported when copying directory from NIX Machine\n");
                }
            }
        }

        /// <summary>
        /// Copies a directory recursively to/from remote.
        /// </summary>
        /// <param name="src">source directory name</param>
        /// <param name="dest">destination directory name</param>
        /// <param name="toRemote">True in order to copy to Remote 
        /// machine else false</param>
        /// <param name="overwrite"> Overwrite Destination if exists</param>
        /// <remarks>
        /// When copying directory to Remote location Caller should make sure 
        /// Remote Parent directory exists.
        /// 
        /// When copying directory from Remote location Caller should make sure 
        /// local Parent directory exists.
        /// 
        /// This function will throw an exception containing the error message
        /// generated if any.
        /// </remarks>
        /// 
        /// <TODO> Create Task for Directory of large size</TODO>
        public void CopyDirectory(string src, string dest, bool toRemote, bool overwrite)
        {
            CopyDirectory(src, dest, toRemote, overwrite, CommandTimeOutInSecs, true);
        }

        /// <summary>
        /// Copies a file to/from remote.
        /// </summary>
        /// <param name="src">source file name</param>
        /// <param name="dest">destination file name</param>
        /// <param name="toRemote">True in order to copy file to Remote 
        /// machine else false</param>
        /// <param name="overwrite"> Overwrite Destination if exists</param>
        /// <param name="executionTimeOutInSec">wait time for completion</param>
        /// <param name="doCRLFConversion">Takes care of CRLF conversion</param>
        /// <remarks>
        /// When copying file from Remote location Caller should make sure 
        /// Source File (remote file) and Destination folder (local folder)
        /// exists.
        /// 
        /// This function will throw an exception containing the error
        /// message generated if any.
        /// </remarks>
        /// 
        /// <TODO> Create Task for Files of large size</TODO>
        public void CopyFile(string src, string dest, bool toRemote, bool overwrite, int executionTimeOutInSec, bool doCRLFConversion)
        {
            if (!overwrite && IsFileOrDirExists(dest, toRemote))
            {
                throw new Exception("Destination Exists :: " + dest);
            }
            if (toRemote)
            {
                string destPath = ResolveNIXFilePath(dest);

                //parent directory is mandatory for copy file to succeed
                CreateDestinationParent(destPath, toRemote);

                sshClient.CopyFile(src, destPath, toRemote, executionTimeOutInSec);

                //SSHClient is not taking care of CRLF conversion hence explicitly doing here
                if (doCRLFConversion)
                {
                    int errCode = ConvertFileToUnixFormat(destPath);
                    if (errCode != 0)
                    {
                        try
                        {
                            //Conversion failed Delete the file and return error.
                            RemoveFile(destPath);
                        }
                        catch
                        {
                            //Suppress exception as it is a cleanup that failed
                        }

                        throw new Exception(string.Format(CultureInfo.InvariantCulture,
                            "Failed to convert file {0} to Unix Format {1} errCode", src, errCode));
                    }
                }
            }
            else
            {
                sshClient.CopyFile(ResolveNIXFilePath(src), dest, toRemote, executionTimeOutInSec);
                if (doCRLFConversion)
                {
                    throw new Exception("CRLF Conversion not supported when copying files from NIX Machine\n");
                }
            }
        }

        /// <summary>
        /// Copies a file to/from remote.
        /// </summary>
        /// <param name="src">source file name</param>
        /// <param name="dest">destination file name</param>
        /// <param name="toRemote">True in order to copy file to Remote 
        /// machine else false</param>
        /// <param name="overwrite"> Overwrite Destination if exists</param>
        /// <remarks>
        /// When copying file from Remote location Caller should make sure 
        /// Source File (remote file) and Destination folder (local folder)
        /// exists.
        /// 
        /// This function will throw an exception containing the error message generated if any.
        /// </remarks> 
        /// 
        /// <TODO> Create Task for Files of large size</TODO>
        public void CopyFile(string src, string dest, bool toRemote, bool overwrite)
        {
            CopyFile(src, dest, toRemote, overwrite, CommandTimeOutInSecs, true);
        }

        /// <summary>
        /// Copies file/Directory to/from remote. This function makes sure all
        /// the non existing directories in the destination are created.
        /// </summary>
        /// <param name="src">source file or directory</param>
        /// <param name="dest">destination file or directory</param>
        /// <param name="toRemote">True in order to copy file to Remote machine else false</param>
        /// <param name="overwrite"> Overwrite Destination if exists</param>
        /// <param name="executionTimeOutInSec">wait time for completion</param>
        /// <param name="doCRLFConversion">Takes care of CRLF conversion</param>
        /// <TODO> Create Task for File/Directory of large size</TODO>
        public void CopyFileOrDirectory(string src, string dest, bool toRemote, bool overwrite, int executionTimeOutInSec, bool doCRLFConversion)
        {
            if (!overwrite && IsFileOrDirExists(dest, toRemote))
            {
                throw new Exception("Destination Exists :: " + dest);
            }
            if (!IsFileOrDirExists(src, !toRemote))
            {
                throw new Exception(string.Format(CultureInfo.InvariantCulture,
                    "Source {0} Not exists on {1}", src, toRemote ? Environment.MachineName : "Linux Machine"));
            }
            CreateDestinationParent(dest, toRemote);
            if (IsSourceDirectory(src, !toRemote))
            {
                CopyDirectory(src, dest, toRemote, true, executionTimeOutInSec, doCRLFConversion);
            }
            else
            {
                CopyFile(src, dest, toRemote, true, executionTimeOutInSec, doCRLFConversion);
            }
        }

        /// <summary>
        /// Copies file/Directory to/from remote. This function makes sure all
        /// the non existing directories in the destination are created.
        /// </summary>
        /// <param name="src">source file or directory</param>
        /// <param name="dest">destination file or directory</param>
        /// <param name="toRemote">True in order to copy file to Remote machine else false</param>
        /// <param name="overwrite"> Overwrite Destination if exists</param>
        /// 
        /// <TODO> Create Task for File/Directory of large size</TODO>
        public void CopyFileOrDirectory(string src, string dest, bool toRemote, bool overwrite)
        {
            CopyFileOrDirectory(src, dest, toRemote, overwrite, CommandTimeOutInSecs, true);
        }

        /// <summary>
        /// Executes a command on *NIX machine.
        /// </summary>
        /// <param name="command">*NIX command</param>
        /// <param name="executionTimeOutInSec">wait time for completion</param>
        /// <returns>output of the command if any</returns>
        public virtual string ExecuteCommand(string command, int executionTimeOutInSec)
        {
            return ExecuteCommandUsingSSH(command, executionTimeOutInSec);
        }

        /// <summary>
        /// Executes a command on *NIX machine.
        /// </summary>
        /// <param name="command">*NIX command</param>
        /// <returns>output of the command if any</returns>
        public virtual string ExecuteCommand(string command)
        {
            return ExecuteCommand(command, CommandTimeOutInSecs);
        }

        /// <summary>
        /// Executes a command on *NIX machine and returns the error code.
        /// 0 indicates SUCCESS else FAILURE.
        /// 
        /// This function throws an exception in case the command returns non integer output
        /// </summary>
        /// <param name="command">*NIX command</param>
        /// <param name="executionTimeOutInSec">wait time for completion</param>
        /// <returns>"0" on SUCCESS "Non Zero" FAILURE</returns>
        public virtual int ExecuteAndReturnErrCode(string command, int executionTimeOutInSec)
        {
            return ExecuteCommandAndReturnErrorCode(command, executionTimeOutInSec);
        }

        /// <summary>
        /// Executes a command on *NIX machine and returns the error code.
        /// 0 indicates SUCCESS else FAILURE.
        /// 
        /// This function throws an exception in case the command returns non integer output
        /// </summary>
        /// <param name="command">*NIX command</param>
        /// <returns>"0" on SUCCESS "Non Zero" FAILURE</returns>
        public virtual int ExecuteAndReturnErrCode(string command)
        {
            return ExecuteAndReturnErrCode(command, CommandTimeOutInSecs);
        }

        /// <summary>
        /// Executes a script on *NIX machine.
        /// 
        /// Throws exception containing execution error message if any.
        /// </summary>
        /// <param name="scriptPath">script path on *NIX machine</param>
        /// <param name="executionTimeOutInSec">wait time for completion</param>
        /// <returns>output of the script if any</returns>
        public virtual string ExecuteScript(string scriptPath, int executionTimeOutInSec)
        {
            UtilityFunctions.LogMethodEntry();

            string nixPath = ResolveNIXFilePath(scriptPath);

            UtilityFunctions.LogDebug(string.Format(CultureInfo.InvariantCulture, "Executing Script {0} ", nixPath));

            return ExecuteCommandUsingSSH(nixPath, executionTimeOutInSec);
        }

        /// <summary>
        /// Executes a script on *NIX machine.
        /// 
        /// Throws exception containing execution error message if any.
        /// </summary>
        /// <param name="scriptPath">script path on *NIX machine</param>
        /// <returns>output of the script if any</returns>
        public virtual string ExecuteScript(string scriptPath)
        {
            return ExecuteScript(scriptPath, CommandTimeOutInSecs);
        }

        /// <summary>
        /// Removes a file on *NIX machine
        /// </summary>
        /// <param name="filePath">file path</param>
        /// <param name="executionTimeOutInSec">wait time for completion</param>
        public virtual void RemoveFile(string filePath, int executionTimeOutInSec)
        {
            string nixPath = ResolveNIXFilePath(filePath);
            string command = string.Format(CultureInfo.InvariantCulture,
                "{0} \"{1}\"", LinuxCommands.Remove, nixPath);
            ExecuteCommandUsingSSH(command, executionTimeOutInSec);
        }

        /// <summary>
        /// Removes a file on *NIX machine
        /// </summary>
        /// <param name="filePath">file path</param>
        public virtual void RemoveFile(string filePath)
        {
            RemoveFile(filePath, CommandTimeOutInSecs);
        }

        /// <summary>
        /// Removes a directory recursively on *NIX machine
        /// </summary>
        /// <param name="dirPath">directory path</param>
        /// <param name="recursive">delete directory recursively</param>
        /// <param name="executionTimeOutInSec">wait time for completion</param>
        public virtual void RemoveDirectory(string dirPath, bool recursive, int executionTimeOutInSec)
        {
            string nixPath = ResolveNIXFilePath(dirPath);
            string command;
            if (!recursive)
            {
                command = string.Format(CultureInfo.InvariantCulture, "{0} \"{1}\"", LinuxCommands.RemoveDir, nixPath);
            }
            else
            {
                command = string.Format(CultureInfo.InvariantCulture, "{0} \"{1}\"", LinuxCommands.Remove + " -r", nixPath);
            }
            ExecuteCommandUsingSSH(command, executionTimeOutInSec);
        }

        /// <summary>
        /// Removes a directory recursively on *NIX machine
        /// </summary>
        /// <param name="dirPath">directory path</param>
        /// <param name="recursive">delete directory recursively</param>
        public void RemoveDirectory(string dirPath, bool recursive)
        {
            RemoveDirectory(dirPath, recursive, CommandTimeOutInSecs);
        }

        /// <summary>
        /// Append contents to a file if existing else creates a file with specified contents
        /// </summary>
        /// <param name="filePath">*NIX file path</param>
        /// <param name="contents">contents to be updated</param>
        /// <param name="executionTimeOutInSec">wait time for completion</param>
        public virtual void AppendToFile(string filePath, string contents, int executionTimeOutInSec)
        {
            string nixPath = ResolveNIXFilePath(filePath);
            CreateDestinationParent(filePath, true);
            string command = string.Format(CultureInfo.InvariantCulture, "{0} '{1}' >> {2}", LinuxCommands.Echo, contents, nixPath);
            ExecuteCommand(command, executionTimeOutInSec);
        }

        /// <summary>
        /// Append contents to a file if existing else creates a file with specified contents
        /// </summary>
        /// <param name="filePath">*NIX file path</param>
        /// <param name="contents">contents to be updated</param>
        public virtual void AppendToFile(string filePath, string contents)
        {
            AppendToFile(filePath, contents, CommandTimeOutInSecs);
        }

        /// <summary>
        /// Returns the process current status 
        /// </summary>
        /// <param name="processId">process Id to monitor</param>
        /// <param name="executionTimeOutInSec">wait time for completion</param>
        /// <returns>current status</returns>
        public virtual LinuxProcessState GetProcessStatus(long processId, int executionTimeOutInSec)
        {
            UtilityFunctions.LogMethodEntry();

            string command = string.Format(CultureInfo.InvariantCulture, "{0} -p {1} -o state=",
                                    LinuxCommands.Ps, processId.ToString(CultureInfo.InvariantCulture));
            string res = ExecuteCommand(command, executionTimeOutInSec);
            int statusCode = (int)res[0];
            LinuxProcessState processState;
            if (Enum.IsDefined(typeof(LinuxProcessState), statusCode))
            {
                processState = (LinuxProcessState)statusCode;
            }
            else
            {
                //Cannot deterministically say process completed or invalid process id
                processState = LinuxProcessState.NotExists;
            }

            UtilityFunctions.LogDebug(string.Format(CultureInfo.InvariantCulture, "Process State for process {0} is {1} ", processId, processState));

            return processState;
        }

        /// <summary>
        /// Returns the process current status 
        /// </summary>
        /// <param name="processId">process Id to monitor</param>
        /// <returns>current status</returns>
        public virtual LinuxProcessState GetProcessStatus(long processId)
        {
            return GetProcessStatus(processId, CommandTimeOutInSecs);
        }

        /// <summary>
        /// creates a *NIX process
        /// </summary>
        /// <param name="cmdLine">command line params can be *NIX script path or other command</param>
        /// <param name="waitProcessToExit">wait for process to complete execution</param>
        /// <param name="executionTimeOutInSec">wait time for completion</param>
        /// <param name="outputFilePath">
        /// filePath to redirect standard output and error. The file should be copied on the local machine
        /// for investigation.
        /// </param>
        /// <returns>Process status code in case of blocking call else process id</returns>
        public virtual long CreateProcess(string cmdLine, bool waitProcessToExit, int executionTimeOutInSec, string outputFilePath)
        {
            UtilityFunctions.LogMethodEntry();

            StringBuilder commmandBuilder = new StringBuilder();

            if (string.IsNullOrWhiteSpace(outputFilePath))
            {
                //Standard output and error to null device
                commmandBuilder.AppendFormat(LinuxCommands.RedirectStdOutAndErr, LinuxConstants.Nulldevice);
            }
            else
            {
                //Standard error and output redirected to the specified file
                commmandBuilder.AppendFormat(LinuxCommands.RedirectStdOutAndErr, ResolveNIXFilePath(outputFilePath));
            }

            commmandBuilder.AppendFormat(" {0}", cmdLine);

            if (waitProcessToExit)
            {
                commmandBuilder.AppendFormat(" ; {0}", LinuxCommands.GetLastExitCode);
            }
            else
            {
                //& makes a process run in background
                commmandBuilder.AppendFormat(" & {0}", LinuxCommands.GetLastBgProcessId);
            }

            UtilityFunctions.LogDebug(string.Format(CultureInfo.InvariantCulture, "Creating process {0}", commmandBuilder.ToString()));

            string res = ExecuteCommandUsingSSH(commmandBuilder.ToString(), executionTimeOutInSec);
            res = RemoveNewLine(res);
            return long.Parse(res, CultureInfo.InvariantCulture);
        }

        /// <summary>
        /// creates a *NIX process
        /// </summary>
        /// <param name="cmdLine">command line params can be *NIX script path or other command</param>
        /// <param name="waitProcessToExit">wait for process to complete execution</param>
        /// <param name="executionTimeOutInSec">wait time for completion</param>
        /// <returns>Process status code in case of blocking call else process id</returns>
        public virtual long CreateProcess(string cmdLine, bool waitProcessToExit, int executionTimeOutInSec)
        {
            return CreateProcess(cmdLine, waitProcessToExit, executionTimeOutInSec, null);
        }

        /// <summary>
        /// creates a *NIX process
        /// </summary>
        /// <param name="cmdLine">command line params can be *NIX script path or other command</param>
        /// <param name="waitProcessToExit">wait for process to complete execution</param>
        /// <returns>Process status code in case of blocking call else process id</returns>
        public virtual long CreateProcess(string cmdLine, bool waitProcessToExit)
        {
            return CreateProcess(cmdLine, waitProcessToExit, CommandTimeOutInSecs, null);
        }

        /// <summary>
        /// creates a *NIX process
        /// </summary>
        /// <param name="cmdLine">command line params can be *NIX script path or other command</param>
        /// <returns>Process status code in case of blocking call else process id</returns>
        public virtual long CreateProcess(string cmdLine)
        {
            return CreateProcess(cmdLine, true, CommandTimeOutInSecs, null);
        }

        /// <summary>
        /// checks *NIX filesystem for error.
        /// If the specified device is mounted it would be unmounted before running fsck and 
        /// remounted after the check.
        /// </summary>
        /// <param name="devicePath">*NIX device path</param>
        /// <param name="correctErrors">correct file system errors if any</param>
        /// <param name="timeoutInSecs">timeout to wait for completion</param>
        /// <returns></returns>
        public virtual FsckResult CheckFileSystem(string devicePath, bool correctErrors, int timeoutInSecs)
        {
            UtilityFunctions.LogMethodEntry();

            string mountPoint = string.Empty;

            if (!string.IsNullOrWhiteSpace(devicePath))
            {
                if (!IsDeviceExists(devicePath))
                    throw new Exception(string.Format(CultureInfo.InvariantCulture, "Device {0}, does not exists", devicePath));
            }

            if (correctErrors && !string.IsNullOrWhiteSpace(devicePath))
            {
                mountPoint = GetBlockDeviceMountPoint(devicePath);

                if (!string.IsNullOrWhiteSpace(mountPoint))
                {
                    UnmountFileSystem(devicePath);
                }
            }

            //In case no devices are specified checks all the devices
            string command = string.Format(CultureInfo.InvariantCulture, "{0} {1} {2}",
                LinuxCommands.Fsck, devicePath, correctErrors ? "-a" : "-n");
            int status = ExecuteAndReturnErrCode(command, timeoutInSecs);

            if (!string.IsNullOrWhiteSpace(mountPoint))
            {
                MountFileSystem(devicePath, mountPoint);
            }

            UtilityFunctions.LogDebug(string.Format(CultureInfo.InvariantCulture, "FileSystem Check Status for {0} is {1} ", devicePath, status));
            return (FsckResult)status;
        }

        /// <summary>
        /// checks *NIX filesystem for error
        /// </summary>
        /// <param name="devicePath"></param>
        /// <param name="correctErrors"></param>
        /// <returns></returns>
        public virtual FsckResult CheckFileSystem(string devicePath, bool correctErrors)
        {
            const int fsckTimeoutInsecs = 30 * 60;
            return CheckFileSystem(devicePath, correctErrors, fsckTimeoutInsecs);
        }

        /// <summary>
        /// Unmounts a file system
        /// </summary>
        /// <param name="devicePath">device Path</param>
        public virtual void UnmountFileSystem(string devicePath)
        {
            UtilityFunctions.LogMethodEntry();

            Guard.RequiresNotNullOrWhiteSpace(devicePath, "devicePath");

            if (!IsDeviceExists(devicePath))
                throw new Exception(string.Format(CultureInfo.InvariantCulture, "Device {0}, does not exists", devicePath));

            UtilityFunctions.LogDebug(string.Format(CultureInfo.InvariantCulture, "Unmounting FileSystem {0} ", devicePath));

            string command = string.Format(CultureInfo.InvariantCulture, "{0} {1}",
                LinuxCommands.Umount, ResolveNIXFilePath(devicePath));
            int res = ExecuteAndReturnErrCode(command, CommandTimeOutInSecs);
            if (res != 0)
            {
                throw new Exception(string.Format(CultureInfo.InstalledUICulture, "Failed to unmount {0} err {1}",
                    devicePath, res));
            }
        }

        /// <summary>
        /// Mounts a file system
        /// </summary>
        /// <param name="devicePath">device Path</param>
        /// <param name="mountPoint">mount point</param>
        /// <returns>mount point of the filesystem</returns>
        public virtual string MountFileSystem(string devicePath, string mountPoint)
        {
            UtilityFunctions.LogMethodEntry();

            Guard.RequiresNotNullOrWhiteSpace(devicePath, "devicePath");
            if (!IsDeviceExists(devicePath))
                throw new Exception(string.Format(CultureInfo.InvariantCulture, "Device {0}, does not exists", devicePath));

            if (string.IsNullOrWhiteSpace(mountPoint))
            {
                mountPoint = DateTime.Now.TimeOfDay.ToString();
                mountPoint = mountPoint.Replace(":", "");
                try
                {
                    CreateDirectory(mountPoint);
                }
                catch (Exception e)
                {
                    throw new Exception("Failed to Create Mount Point at " + mountPoint + e.Message);
                }
            }

            mountPoint = ResolveNIXFilePath(mountPoint);

            UtilityFunctions.LogDebug(string.Format(CultureInfo.InvariantCulture, "Mounting device {0} at {1} ", devicePath, mountPoint));
            string command = string.Format(CultureInfo.InvariantCulture, "{0} {1} {2}",
                LinuxCommands.Mount, devicePath, mountPoint);
            int res = ExecuteAndReturnErrCode(command, CommandTimeOutInSecs);
            if (res != 0)
            {
                throw new Exception(string.Format(CultureInfo.InstalledUICulture, "Failed to mount {0} at {1} err {2}",
                    devicePath, mountPoint, res));
            }

            return mountPoint;
        }

        /// <summary>
        /// Function retrieves all the block devices and their mount point if any.
        /// </summary>
        /// <returns>Dictionary of block devices and mount points</returns>
        public virtual Dictionary<string, string> GetBlockDevicesList()
        {
            UtilityFunctions.LogMethodEntry();

            Dictionary<string, string> blockDevicesInfo = new Dictionary<string, string>();
            string command = string.Format(CultureInfo.InvariantCulture, "{0} -o NAME,MOUNTPOINT -P",
                LinuxCommands.Lsblk);
            string res = ExecuteCommand(command, CommandTimeOutInSecs);

            string[] devices = res.Split(new char[] { '\n' }, StringSplitOptions.RemoveEmptyEntries);
            foreach (string device in devices)
            {
                //each string is going to be of form NAME="..." MOUNTPOINT="..."
                string[] tempStrings = device.Split(new char[] { '"' }, StringSplitOptions.None);
                blockDevicesInfo.Add(tempStrings[1], tempStrings[3]);
            }

            return blockDevicesInfo;
        }

        /// <summary>
        /// returns the mount point of the device. Throw exception in case the device is not found
        /// </summary>
        /// <param name="devicePath">device Path</param>
        /// <returns>mountpoint if any else null</returns>
        public virtual string GetBlockDeviceMountPoint(string devicePath)
        {
            UtilityFunctions.LogMethodEntry();

            Guard.RequiresNotNullOrWhiteSpace(devicePath, "devicePath");

            Dictionary<string, string> blockDevicesInfo = GetBlockDevicesList();
            string deviceName = GetDeviceNameFromPath(devicePath);
            string mountPoint;
            if (!blockDevicesInfo.TryGetValue(deviceName, out mountPoint))
            {
                throw new Exception("Cannot find block device " + devicePath);
            }

            UtilityFunctions.LogDebug(string.Format(CultureInfo.InvariantCulture, "Mount point for device {0} is {1} ", devicePath, mountPoint));

            return mountPoint;
        }

        /// <summary>
        /// Returns the first unmounted disk on a *NIX System. Null if no devices are found
        /// </summary>
        /// <returns>device path of first unmounted block device. Null if no unmounted devices are present</returns>
        public virtual string GetFirstUnmountedBlockDevice()
        {
            UtilityFunctions.LogMethodEntry();

            Dictionary<string, string> blockDevicesInfo = GetBlockDevicesList();
            string blockDevice = (from diskInfo in blockDevicesInfo
                                  where String.IsNullOrWhiteSpace(diskInfo.Value)
                                  where diskInfo.Key.All(Char.IsLetter) && !blockDevicesInfo.ContainsKey(diskInfo.Key + "1")
                                  select LinuxConstants.DeviceFilePrefix + "/" + diskInfo.Key).FirstOrDefault();

            UtilityFunctions.LogDebug(string.Format(CultureInfo.InvariantCulture, "First Unmounted device is {0}", blockDevice));

            return blockDevice;
        }

        /// <summary>
        /// Creates the specified filesystem on the specified device.
        /// In case no file system is specified creates the default ext2 file system
        /// </summary>
        /// <param name="devicePath">device Path</param>
        /// <param name="fstype">file system type</param>
        /// <param name="timeOutInSec">timeout to wait for the operation</param>
        /// <returns></returns>
        public virtual void CreateFileSystem(string devicePath, string fstype, int timeOutInSec)
        {
            UtilityFunctions.LogMethodEntry();

            Guard.RequiresNotNullOrWhiteSpace(devicePath, "devicePath");
            if (!IsDeviceExists(devicePath))
                throw new Exception(string.Format(CultureInfo.InvariantCulture, "Device {0}, does not exists", devicePath));

            if (!string.IsNullOrWhiteSpace(GetBlockDeviceMountPoint(devicePath)))
            {
                throw new Exception("Device Already mounted. Unmount and try again");
            }

            StringBuilder command = new StringBuilder();
            command.Append(LinuxCommands.Mkfs + " -F ");
            if (!string.IsNullOrWhiteSpace(fstype))
            {
                command.Append(" -t " + fstype + " ");
            }
            command.Append(devicePath);

            UtilityFunctions.LogDebug(string.Format(CultureInfo.InvariantCulture, "Creating file system {0} on device {1} ", fstype, devicePath));
            //Due to Bug in MKfs it is writing to stderr some debug input hence not using ExecuteCommand
            int res = ExecuteCommandAndReturnErrorCode(command.ToString(), timeOutInSec);
            if (res != 0)
            {
                throw new Exception(string.Format(CultureInfo.InvariantCulture,
                    "Error creating filesystem {0} on Device {1} error code {2}", fstype, devicePath,
                    res.ToString(CultureInfo.InvariantCulture)));
            }
        }

        /// <summary>
        /// Checks Linux Boot status
        /// </summary>
        /// <returns>true if booted else false</returns>
        public virtual bool IsLinuxBooted()
        {
            string cmd = string.Format(CultureInfo.InvariantCulture, "{0} -C {1}", LinuxCommands.Ps, LinuxConstants.LoginDaemonName);
            int status = ExecuteAndReturnErrCode(cmd);
            if (status != 0)
            {
                //FIXME:: Once All Linux distros are moved to Systemd-logind Call below can be removde.

                //Depending on init process to check boot complete
                //Init process is the first process to be started. It moves to suspend state after the system boots up.
                //Most of the time it is a NO OP and the call below is have no side 
                LinuxProcessState initState = GetProcessStatus(LinuxConstants.InitProcessId);
                if (initState == LinuxProcessState.Suspend)
                {
                    status = 0;
                }
            }

            return status == 0;
        }

        /// <summary>
        /// Shutdown a Linux System
        /// </summary>
        /// <returns>true if call successfull else false</returns>
        public virtual bool ShutDownSystem()
        {
            int status = ExecuteAndReturnErrCode(LinuxCommands.ShutDown);
            return status == 0;
        }

        /// <summary>
        /// Graceful Shutdown a Linux System
        /// </summary>
        /// <returns>true if call successfull else false</returns>
        public virtual bool RestartSystem()
        {
            int status = ExecuteAndReturnErrCode(LinuxCommands.Reboot);
            return status == 0;
        }

        /// <summary>
        /// Converts the file to Unix File Format (CRLF to LF conversion)
        /// </summary>
        /// <param name="filePath">FilePath</param>
        /// <returns>0 sucess else linux error code</returns>
        public virtual int ConvertFileToUnixFormat(string filePath)
        {
            string destPath = ResolveNIXFilePath(filePath);
            return ExecuteAndReturnErrCode(LinuxCommands.Dos2Unix + " " + destPath);
        }

        /// <summary>
        /// Converts files in a directory recursively to Unix File Format (CRLF to LF conversion)
        /// </summary>
        /// <param name="dirPath">FilePath</param>
        /// <returns>0 sucess else linux error code</returns>
        public virtual int ConvertDirectoryRecursivelyToUnixFormat(string dirPath)
        {
            string destPath = ResolveNIXFilePath(dirPath);
            if (!DoesDirectoryExists(dirPath))
            {
                throw new DirectoryNotFoundException("Cannot find directory on remote machine " + dirPath);
            }
            string cmd = string.Format(CultureInfo.InvariantCulture,
                                @"find {0} -type f -exec {1} {{}} \;", dirPath, LinuxCommands.Dos2Unix);

            return ExecuteAndReturnErrCode(cmd);
        }

        #endregion public methods

        #region private methods

        private string GetNIXParentDir(string nixPath)
        {
            int i = nixPath.LastIndexOf(NixPathSeparator);
            if (i == 0)
            {
                //Parent of Root is Root on *NIX
                return nixPath;
            }

            if (i == -1)
            {
                throw new Exception("Cannot find parent of " + nixPath);
            }

            string parent = nixPath.Substring(0, i);
            if (String.IsNullOrEmpty(parent))
            {
                return NixPathSeparator.ToString(CultureInfo.InvariantCulture);
            }
            else
            {
                return parent;
            }
        }

        private string GetParentDirectory(string filePath, bool isRemote)
        {
            if (isRemote)
            {
                //We are on *NIX
                return GetNIXParentDir(ResolveNIXFilePath(filePath));
            }
            else
            {
                //We are on Windows
                return WindowsHelper.GetParentDir(filePath);
            }
        }

        private void CreateParentDirectory(string filePath, bool isRemote)
        {
            string parentDir = GetParentDirectory(filePath, isRemote);
            if (!IsFileOrDirExists(parentDir, isRemote))
            {
                if (isRemote)
                {
                    //We are on *NIX
                    CreateDirectory(parentDir);
                }
                else
                {
                    //We are on Windows
                    WindowsHelper.CreateDirectory(parentDir);
                }
            }
        }

        private void CreateDestinationParent(string filePath, bool isRemote)
        {
            CreateParentDirectory(filePath, isRemote);
        }

        private bool IsSourceDirectory(string source, bool isRemote)
        {
            if (isRemote)
            {
                return DoesDirectoryExists(source);
            }
            else
            {
                return WindowsHelper.DoesDirExists(source);
            }
        }

        /// <summary>
        /// Checks if a file/directory exists on remote machine
        /// </summary>
        /// <param name="filePath">file/directory path</param>
        /// <param name="isRemote">input file/directory is at remote machine</param>
        /// <returns>true if exists else false</returns>
        /// <remarks>
        /// Directories are treated as speical files on *NIX systems
        /// </remarks>
        private bool IsFileOrDirExists(string filePath, bool isRemote)
        {
            if (isRemote)
            {
                return DoesFileExists(filePath) || DoesDirectoryExists(filePath);
            }
            else
            {
                return WindowsHelper.DoesFileOrDirectoryExists(filePath);
            }
        }

        /// <summary>
        /// Executes the *NIX test command and returns the execution status
        /// </summary>
        /// <param name="args">argument to test command</param>
        /// <param name="filePath">*NIX filePath</param>
        /// <param name="executionTimeOutInSec">Time out for to wait for completion</param>
        /// <returns></returns>
        private bool ExecuteTestCommand(string args, string filePath, int executionTimeOutInSec)
        {
            string nixFilePath = ResolveNIXFilePath(filePath);
            string cmd = String.Format(CultureInfo.InvariantCulture,
                        "{0} {1} \"{2}\"", LinuxCommands.Test, args, nixFilePath);
            int errCode = ExecuteCommandAndReturnErrorCode(cmd, executionTimeOutInSec);
            return (errCode == 0);
        }

        private int ExecuteCommandAndReturnErrorCode(string command, int executionTimeOutInSec)
        {
            string ignoreStdOutAndErr = string.Format(CultureInfo.InvariantCulture, LinuxCommands.RedirectStdOutAndErr, LinuxConstants.Nulldevice);
            string cmd = String.Format(CultureInfo.InvariantCulture,
                            "{0} {1} ; {2}", ignoreStdOutAndErr, command, LinuxCommands.GetLastExitCode);
            string result = ExecuteCommandUsingSSH(cmd, executionTimeOutInSec);
            return int.Parse(result, CultureInfo.InvariantCulture);
        }

        private int ExecuteCommandAndReturnErrorCode(string command)
        {
            return ExecuteCommandAndReturnErrorCode(command, CommandTimeOutInSecs);
        }

        private string RemoveNewLine(string content)
        {
            return content.Replace("\n", "");
        }

        private string ExecuteCommandUsingSSH(string command, int executionTimeOutInSec)
        {
            return sshClient.ExecuteCommand(command, executionTimeOutInSec);
        }

        private string ExecuteCommandUsingSSH(string command)
        {
            return ExecuteCommandUsingSSH(command, CommandTimeOutInSecs);
        }

        private string GetDeviceNameFromPath(string devicePath)
        {
            return devicePath.Substring(devicePath.LastIndexOf('/') + 1);
        }

        private bool IsDeviceExists(string devicePath)
        {
            string deviceName = GetDeviceNameFromPath(devicePath);
            if (!string.IsNullOrWhiteSpace(deviceName))
            {
                Dictionary<string, string> blockDevicesInfo = GetBlockDevicesList();
                if (blockDevicesInfo.ContainsKey(deviceName))
                    return true;
            }
            return false;
        }

        /// <summary>
        /// Constructor
        /// </summary>
        /// <param>
        /// <param name="sshTargetConnectionParams"></param>
        /// </param>
        private LinuxClient(SSHTargetConnectionParams sshTargetConnectionParams)
        {
            var targetParams = new SSHTargetConnectionParams();
            targetParams = sshTargetConnectionParams.Clone();


            sshClient = new SSHUtils(targetParams);
            NixPathSeparator = Path.AltDirectorySeparatorChar;

            UserHomeDir = ExecuteCommandUsingSSH(LinuxCommands.Pwd);
            //Removing "\n" as the string would be consumed in *NIX Path Construction
            UserHomeDir = RemoveNewLine(UserHomeDir);
        }

        #endregion private methods
    }
}
