using System;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;
using System.Threading;
using System.Net;
using System.Net.Sockets;
using Microsoft.Win32;
using System.Management;
using System.Collections.Generic;
using System.Diagnostics;
using System.ServiceProcess;
using System.Security;
using System.Security.Cryptography;
using System.Security.Cryptography.X509Certificates;
using COMAdmin;
using ASRResources;

namespace ASRSetupFramework
{
    public static class ValidationHelper
    {
        /// <summary>
        /// Validates connection passphrase
        /// </summary>
        /// <param name="filePath">connection passphrase file path</param>
        /// <param name="ipAddress">CS IP address</param>
        /// <param name="port">CS Port</param>
        /// <returns>returns ture if connection passphrase is valid, otherwise false</returns>
        public static bool ValidateConnectionPassphrase(string filePath, string ipAddress, string port)
        {
            bool returnCode = false;

            if (File.Exists(filePath))
            {
                try
                {
                    // Create directory before copying passphrase file.
                    string strProgramDataPath =
                        Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData);
                    string dirPath =
                        strProgramDataPath + "\\Microsoft Azure Site Recovery\\private\\";

                    try
                    {
                        // Check whether direcotyr already exists.
                        if (Directory.Exists(dirPath))
                        {
                            Trc.Log(LogLevel.Always, "{0} directory already exists.", dirPath);
                        }
                        else
                        {
                            // Try to create the directory.
                            Directory.CreateDirectory(dirPath);
                            Trc.Log(LogLevel.Always, "{0} directory created successfully.", dirPath);
                        }
                    }
                    catch (Exception e)
                    {
                        Trc.Log(LogLevel.Always, "Could not create {0}", dirPath);
                        Trc.Log(LogLevel.Always, "Exception occurred: {0}", e.ToString());
                        return returnCode;
                    }

                    // Copy passphrase file passed to the installer to destination path.
                    File.Copy(filePath, strProgramDataPath + "\\Microsoft Azure Site Recovery\\private\\connection.passphrase", true);
                }
                catch (Exception e)
                {
                    Trc.Log(LogLevel.Always, "Could not copy connection passphrase file to destination path.");
                    Trc.Log(LogLevel.Always, "Exception occurred: {0}", e.ToString());
                    return returnCode;
                }

                bool isAgentInstalled = true;
                bool queryStatus = PrechecksSetupHelper.QueryInstallationStatus(UnifiedSetupConstants.MS_MTProductGUID, out string version, out isAgentInstalled);
                if (!queryStatus || !isAgentInstalled)
                {
                    // this block will be executed only for unified setup
                    // so don't update drscout.conf here
                    Trc.Log(LogLevel.Always, "Validating passphrase for unified setup installer");
                    returnCode = ValidationHelper.ValidateConnectionPassphraseForCS(filePath, ipAddress, port);
                    Trc.Log(LogLevel.Always, "Exit code for registering with cs is {0}", returnCode);
                    return returnCode;
                }

                AgentInstallRole role = SetupHelper.GetAgentInstalledRole();
                if (role == AgentInstallRole.MS)
                {
                    try
                    {
                        if (!PropertyBagDictionary.Instance.GetProperty<bool>(PropertyBagConstants.IsManualInstall))
                        {
                            Trc.Log(LogLevel.Always, "Command line invocation. Cs type {0}", PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.CSType));
                            // Incase of push install we blindly trust the parameters passed by the push install
                            returnCode = ValidationHelper.ValidateConnectionPassphraseForCS(filePath, ipAddress, port);
                            Trc.Log(LogLevel.Always, "Exit code for registering with cs is {0}", returnCode);
                            SetupHelper.UpdateDrScout(ConfigurationServerType.CSLegacy);
                        }
                        else
                        {
                            // Fresh Manual Installation
                            Trc.Log(LogLevel.Always, "Validating passphrase for manual installation");
                            returnCode = ValidationHelper.ValidateConnectionPassphraseForCS(filePath, ipAddress, port);
                            Trc.Log(LogLevel.Always, "Exit code for registering with cs is {0}", returnCode);
                            SetupHelper.UpdateDrScout(ConfigurationServerType.CSLegacy);

                        }
                    }
                    catch (Exception ex)
                    {
                        returnCode = false;
                        Trc.Log(LogLevel.Error, "Failed to validate passphrase with exception " + ex);
                    }
                }
                else if (role == AgentInstallRole.MT)
                {
                    returnCode = ValidationHelper.ValidateConnectionPassphraseForCS(filePath, ipAddress, port);
                    Trc.Log(LogLevel.Always, "Exit code for registering with cs is {0}", returnCode);
                    SetupHelper.UpdateDrScout(ConfigurationServerType.CSLegacy);
                }
                return returnCode;
            }
            else
            {
                Trc.Log(LogLevel.Always, "Connection passphrase file({0}) passed to the installer does not exist", filePath);
            }
            return returnCode;
        }

        /// <summary>
        /// Validates source config file
        /// </summary>
        /// <param name="sourceConfigFilePath">source config file path</param>
        /// <out="exitCode">exitCode from RcmCli</out>
        /// <returns>returns ture if source config is valid, otherwise false</returns>
        public static bool ValidateSoureConfigFileForCSPrime(string sourceConfigFilePath, out int exitCode)
        {
            bool isSourceConfigValidated = false;
            exitCode = -1;
            string parameters = "--verifyclientauth" + " --configfile " + "\"" + sourceConfigFilePath + "\" " +
                UnifiedSetupConstants.AzureRcmCliErrorFileName +
                $" \"{PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.ValidationsOutputJsonFilePath)}\" " +
                UnifiedSetupConstants.AzureRcmCliLogFileName +
                $" \"{PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.DefaultLogName)}\"";
            string rcmCliPath = Path.Combine(SetupHelper.GetAgentInstalledLocation(),
                UnifiedSetupConstants.AzureRCMCliExeName);

            for (int i = 1; i < 3; i++)
            {
                Trc.Log(LogLevel.Always, "Validating source config file - attempt {0}", i);
                exitCode = CommandExecutor.RunCommand(rcmCliPath, parameters);
                Trc.Log(LogLevel.Always, "Validating source config file Iteration: {0} Exit code: {1}", i, exitCode);
                if (exitCode == 0)
                {
                    Trc.Log(LogLevel.Always, "Successfully validated source config file.");
                    isSourceConfigValidated = true;
                    break;
                }
                else if (exitCode == 148)
                {
                    Trc.Log(LogLevel.Always,
                        "Stopping service - {0}",
                        UnifiedSetupConstants.SvagentsServiceName);
                    if (!ServiceControlFunctions.StopService(
                            UnifiedSetupConstants.SvagentsServiceName))
                    {
                        ServiceControlFunctions.KillServiceProcess(
                            UnifiedSetupConstants.SvagentsServiceName);
                    }
                }
            }
            return isSourceConfigValidated;
        }

        public static bool ValidateConnectionPassphraseForCS(string filePath, string ipAddress, string port)
        {
            bool returnCode = false;
            //Invoke csgetfingerprint.exe for 3 times in the loop to rule out any transient errors
            string parameters = "-i " + ipAddress + " -p " + port;
            string output = "";

            for (int i = 1; i <= 3; i++)
            {
                Trc.Log(LogLevel.Always, "Validating connection passphrase - attempt {0}", i);
                int returnCodeCsgf = CommandExecutor.RunCommand("csgetfingerprint.exe", parameters, out output);
                if (returnCodeCsgf == 0)
                {
                    Trc.Log(LogLevel.Always, "csgetfingerprint.exe command output.");
                    Trc.Log(LogLevel.Always, "{0}", output);
                    Trc.Log(LogLevel.Always, "Successfully validated connection passphrase.");
                    return true;
                }
                else
                {
                    Trc.Log(LogLevel.Always, "csgetfingerprint.exe command output.");
                    Trc.Log(LogLevel.Always, "{0}", output);
                    Trc.Log(LogLevel.Always, "Connection passphrase validation has failed.");
                }

                //sleep for 5 seconds before re-validating
                Thread.Sleep(5000);
            }
            return returnCode;
        }

        /// <summary>
        /// Validates MySQL Credentials File Path and it's contents.
        /// </summary>
        /// <param name="filePath">MySQL Credentials File Path</param>
        /// <returns>true if MySQL credentials file exists and it's contents are valid, otherwise false</returns>
        public static bool ValidateMySQLCredsFilePath(string filePath)
        {
            bool returnCode = false;

            try
            {
                if (File.Exists(filePath))
                {
                    Trc.Log(LogLevel.Always, "MySQL Credentials File Path({0}) passed to the installer exists.", filePath);

                    // Read each line of the credentials file into a string array and excldue empty lines.
                    string[] lines = File.ReadAllLines(filePath).Where(line => !IsNullOrWhiteSpace(line)).ToArray();

                    // Check whether number of lines are 3.
                    if (lines.Length != 3)
                    {
                        Trc.Log(LogLevel.Always, "Number of lines in credentials file are not 3.");
                        return returnCode;
                    }
                    else
                    {
                        Trc.Log(LogLevel.Always, "Number of lines in credentials file are 3.");
                    }

                    // Check whether first line is in expected format
                    if (lines[0] == "[MySQLCredentials]")
                    {
                        Trc.Log(LogLevel.Always, "First line is in expected format.");
                    }
                    else
                    {
                        Trc.Log(LogLevel.Always, "First line is in not in expected format.");
                        return returnCode;
                    }

                    // Check whether second line is in expected format
                    Match match;
                    match = Regex.Match(lines[1], "^MySQLRootPassword[ \t]*=[ \t]*\".*\"$");
                    if (match.Success)
                    {
                        Trc.Log(LogLevel.Always, "Second line is in expected format.");
                    }
                    else
                    {
                        Trc.Log(LogLevel.Always, "Second line is in not in expected format.");
                        return returnCode;
                    }

                    // Check whether third line is in expected format
                    match = Regex.Match(lines[2], "^MySQLUserPassword[ \t]*=[ \t]*\".*\"$");
                    if (match.Success)
                    {
                        Trc.Log(LogLevel.Always, "Third line is in expected format.");
                    }
                    else
                    {
                        Trc.Log(LogLevel.Always, "Third line is in not in expected format.");
                        return returnCode;
                    }

                    string mysqlRootPassword = GrepUtils.GetKeyValueFromFile(filePath, "MySQLRootPassword");
                    string mysqlUserPassword = GrepUtils.GetKeyValueFromFile(filePath, "MySQLUserPassword");

                    // Validate MySQLRootPassword
                    Trc.Log(LogLevel.Always, "Validating MySQLRootPassword.");

                    // Check whether password is empty
                    bool mrpwdIsEmpty = StringIsEmpty(mysqlRootPassword);

                    // Check whether password has required length
                    bool mrpwdHasReqLength = StringHasReqLength(mysqlRootPassword);

                    // Check whether password contains atleast one number
                    bool mrpwdHasNumber = StringHasNumber(mysqlRootPassword);

                    // Check whether password contains atleast one letter
                    bool mrpwdHasLetter = StringHasLetter(mysqlRootPassword);

                    // Check whether password contains any unsupported characters
                    bool mrpwdHasAllowedSplChar = StringHasAllowedSplChar(mysqlRootPassword);

                    // Check whether password contains atleast one allowed special character
                    bool mrpwdHasUnsupportedSplChars = StringHasUnsupportedSplChars(mysqlRootPassword);

                    // Check whether password has spaces
                    bool mrpwdHasSpaces = StringHasSpaces(mysqlRootPassword);

                    // Check whether MySQLRootPassword is not empty, it has required length, it has a number, it has a letter,
                    // it has a allowed special character, it does not have any unsupported special characters and it does not have empty spaces.
                    if (!mrpwdIsEmpty && mrpwdHasReqLength && mrpwdHasNumber && mrpwdHasLetter && mrpwdHasAllowedSplChar && !mrpwdHasUnsupportedSplChars && !mrpwdHasSpaces)
                    {
                        Trc.Log(LogLevel.Always, "MySQLRootPassword is valid.");
                    }
                    else
                    {
                        Trc.Log(LogLevel.Always, "MySQLRootPassword is invalid.");
                        return returnCode;
                    }

                    // Validate MySQLUserPassword
                    Trc.Log(LogLevel.Always, "Validating MySQLUserPassword.");

                    // Check whether password is empty
                    bool mupwdIsEmpty = StringIsEmpty(mysqlUserPassword);

                    // Check whether password has required length
                    bool mupwdHasReqLength = StringHasReqLength(mysqlUserPassword);

                    // Check whether password contains atleast one number
                    bool mupwdHasNumber = StringHasNumber(mysqlUserPassword);

                    // Check whether password contains atleast one letter
                    bool mupwdHasLetter = StringHasLetter(mysqlUserPassword);

                    // Check whether password contains any unsupported characters
                    bool mupwdHasAllowedSplChar = StringHasAllowedSplChar(mysqlUserPassword);

                    // Check whether password contains atleast one allowed special character
                    bool mupwdHasUnsupportedSplChars = StringHasUnsupportedSplChars(mysqlUserPassword);

                    // Check whether password has spaces
                    bool mupwdHasSpaces = StringHasSpaces(mysqlUserPassword);

                    // Check whether MySQLUserPassword is not empty, it has required length, it has a number, it has a letter,
                    // it has a allowed special character, it does not have any unsupported special characters and it does not have empty spaces.
                    if (!mupwdIsEmpty && mupwdHasReqLength && mupwdHasNumber && mupwdHasLetter && mupwdHasAllowedSplChar && !mupwdHasUnsupportedSplChars && !mupwdHasSpaces)
                    {
                        Trc.Log(LogLevel.Always, "MySQLUserPassword is valid.");
                    }
                    else
                    {
                        Trc.Log(LogLevel.Always, "MySQLUserPassword is invalid.");
                        return returnCode;
                    }

                    // If control reaches here, MySQLCredsFilePath and it's contents are valid.
                    return true;
                }
                else
                {
                    Trc.Log(LogLevel.Always, "MySQL Credentials File Path({0}) passed to the installer does not exist.", filePath);
                }
                return returnCode;
            }
            catch (Exception e)
            {
                Trc.Log(LogLevel.Always, "Exception occurred: {0}", e.ToString());
                return returnCode;
            }
        }

        /// <summary>
        ///  Validates install location. Gets install drive from install location and checks whether driver exists or not,
        ///  checks whether drive is fixed NTFS drive and it has >= 600 GB free space.
        /// </summary>
        /// <param name="installDrive">installation location to be validated</param>
        /// <returns>true if drive exists and it is not fixed drive and it is a NFTS drive and it has >=600 GB free space</returns>
        public static bool ValidateInstallLocation(string installLocation, bool spaceCheckRequired)
        {
            bool returnCode = false;

            // Get install drive from install location
            string installDrive = installLocation[0] + ":\\";
            installDrive = installDrive.ToUpper();
            Trc.Log(LogLevel.Always, "InstallLocation value: {0}", installLocation);
            Trc.Log(LogLevel.Always, "InstallDrive value: {0}", installDrive);

            // Get all the drives available on the system and loop through each.
            DriveInfo[] drives = DriveInfo.GetDrives();
            foreach (DriveInfo drive in drives)
            {
                // Check whether drive exists or not.
                if (drive.Name == installDrive)
                {
                    Trc.Log(LogLevel.Always, "InstallDrive({0}) exists.", installDrive);

                    // Check whether drive type is of fixed and it is a NTFS drive.
                    if (drive.DriveType == DriveType.Fixed && drive.DriveFormat.Equals("NTFS", StringComparison.OrdinalIgnoreCase))
                    {
                        Trc.Log(LogLevel.Always, "InstallDrive({0}) is a fixed drive and NTFS drive", installDrive);
                    }
                    else
                    {
                        Trc.Log(LogLevel.Always, "InstallDrive({0}) is not a fixed drive or not a NTFS drive", installDrive);
                        return returnCode;
                    }

                    // Check for free space only when spaceCheckRequired argument is set to true
                    if (spaceCheckRequired)
                    {
                        // Check whether selected drive has more than 600 GB free space.
                        long freeSpace = drive.TotalFreeSpace / (1024 * 1024 * 1024);
                        Trc.Log(LogLevel.Always, "InstallDrive({0}) has {1} GB free space", installDrive, freeSpace);

                        if (freeSpace >= 600)
                        {
                            Trc.Log(LogLevel.Always, "InstallDrive({0}) has more than 600 GB free space", installDrive);
                        }
                        else
                        {
                            Trc.Log(LogLevel.Always, "InstallDrive({0}) has less than 600 GB free space", installDrive);
                            return returnCode;
                        }
                    }

                    // If control reaches here, drive exists, it is not a fixed drive, it a NFTS drive and it has more than 600 GB. So, return true
                    Trc.Log(LogLevel.Always, "InstallDrive({0}) validation has succeeded.", installDrive);
                    return true;
                }
            }
            Trc.Log(LogLevel.Always, "InstallDrive({0}) validation has failed.", installDrive);
            return returnCode;
        }

        /// <summary>
        ///  Validates install location. Gets install drive from install location and checks whether drive is system drive or not.
        /// </summary>
        /// <param name="installLocation">installation location to be validated</param>
        /// <returns>true if drive is non-system drive or a sub-directory </param>
        /// <returns>false if it's a system drive or un-supported installation path.</returns>
        public static bool IsNonSystemDrive(String installLocation)
        {
            bool returnCode = true;
            try
            {
                string installPath = installLocation + @"\";
                string systemDrive = Path.GetPathRoot(Environment.SystemDirectory);

                if (!(installPath.StartsWith(@"\\")) && Path.IsPathRooted(installPath))
                {
                    if (string.IsNullOrEmpty((Path.GetDirectoryName(installPath))))
                    {
                        string installationRootDrive = Path.GetPathRoot(installPath);
                        if (string.Equals(installationRootDrive, systemDrive))
                        {
                            return false;
                        }
                        else
                        {
                            return returnCode;
                        }
                    }
                    return returnCode;
                }
                return false;
            }
            catch (Exception e)
            {
                Trc.Log(LogLevel.Always, "Exception occurred: {0}", e.ToString());
                return false;

            }
        }

        /// <summary>
        /// Validates valult crendentiasl file.
        /// </summary>
        /// <param name="filePath"></param>
        /// <returns>true if file exists and it has .VaultCredentials extension</returns>
        public static bool ValidateVaultCredsFilePath(string filePath)
        {
            bool returnCode = false;

            // Check whether file exists or not.
            if (File.Exists(filePath))
            {
                Trc.Log(LogLevel.Always, "Vault Credentials File Path({0}) passed to the installer exists.", filePath);

                // Get the filename extension.
                string fileExt = Path.GetExtension(filePath);
                string requiredExt = ".VaultCredentials";

                // Comparing the file extensions case-insensitively.
                if (String.Equals(fileExt, requiredExt, StringComparison.OrdinalIgnoreCase))
                {
                    Trc.Log(LogLevel.Always, "Vault Credentials File ({0}) has .VaultCredentials extension.", filePath);
                    return true;
                }
                else
                {
                    Trc.Log(LogLevel.Always, "Vault Credentials File ({0}) does not have .VaultCredentials extension.", filePath);
                }
            }
            else
            {
                Trc.Log(LogLevel.Always, "Vault Credentials File Path({0}) passed to the installer does not exist.", filePath);
            }
            return returnCode;
        }

        /// <summary>
        /// Validates IP address format
        /// </summary>
        /// <param name="ipAddress">IP Address</param>
        /// <returns>ture in case of valid IP address, otherwise false</returns>
        public static bool ValidateIPAddress(string ipAddress)
        {
            bool returnCode = false;

            Match match = Regex.Match(ipAddress, @"^(([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\.){3}([0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])$");
            if (match.Success)
            {
                returnCode = true;
            }
            string status = match.Success ? "succeeded" : "failed";
            Trc.Log(LogLevel.Always, "IP address({0}) validation has {1}.", ipAddress, status);
            return returnCode;
        }

        /// <summary>
        /// Determines whether string has atleast one digit.
        /// </summary>
        /// <param name="word">string to be validated</param>
        /// <returns>true if string has atleast one digit, otherwise false</returns>
        public static bool StringHasNumber(string word)
        {
            bool returnCode = false;
            Match match = Regex.Match(word, "\\d{1}");
            if (match.Success)
            {
                returnCode = true;
            }
            string status = match.Success ? "succeeded" : "failed";
            Trc.Log(LogLevel.Always, "StringHasNumber validation: {0}.", status);
            return returnCode;
        }

        /// <summary>
        /// Determines whether string has atleast one letter.
        /// </summary>
        /// <param name="word">string to be validated</param>
        /// <returns>true if string has atleast one letter, otherwise false</returns>
        public static bool StringHasLetter(string word)
        {
            bool returnCode = false;
            Match match = Regex.Match(word, "[A-Za-z]{1}");
            if (match.Success)
            {
                returnCode = true;
            }
            string status = match.Success ? "succeeded" : "failed";
            Trc.Log(LogLevel.Always, "StringHasLetter validation: {0}.", status);
            return returnCode;
        }

        /// <summary>
        /// Determines whether string has atleast one allowed specical character.
        /// </summary>
        /// <param name="word">string to be validated</param>
        /// <returns>true if string has atleast one allowed specical character, otherwise false</returns>
        private static bool StringHasAllowedSplChar(string word)
        {
            var splList = new[] { "!", "@", "#", "$", "%", "_" };
            if (splList.Any(word.Contains))
            {
                Trc.Log(LogLevel.Always, "StringHasAllowedSplChar validation: succeeded.");
                return true;
            }
            else
            {
                Trc.Log(LogLevel.Always, "StringHasAllowedSplChar validation: failed.");
                return false;
            }
        }

        /// <summary>
        /// Determines whether string has any unsupported specical characters.
        /// </summary>
        /// <param name="word"></param>
        /// <returns>true if string has any unsupported specical characters, otherwise false</returns>
        private static bool StringHasUnsupportedSplChars(string word)
        {
            var UnsupportedsplList = new[] { "~", "'", "^", "&", "*", "(", ")", "+", "=", "-", "/", @"\", ":", ";", "{", "}", "[", "]", "|", "?", "<", ">", ",", ".", "\"" };
            if (UnsupportedsplList.Any(word.Contains))
            {
                Trc.Log(LogLevel.Always, "StringHasUnsupportedSplChars validation: succeeded.");
                return true;
            }
            else
            {
                Trc.Log(LogLevel.Always, "StringHasUnsupportedSplChars validation: failed.");
                return false;
            }
        }

        /// <summary>
        /// Determines whether string length is >=8 and <=16
        /// </summary>
        /// <param name="word">string to be validated</param>
        /// <returns>true if string length is is >=8 and <=16, otherwise false</returns>
        private static bool StringHasReqLength(string word)
        {
            if ((word.Length >= 8) && (word.Length <= 16))
            {
                Trc.Log(LogLevel.Always, "StringHasReqLength validation: succeeded.");
                return true;
            }
            else
            {
                Trc.Log(LogLevel.Always, "StringHasReqLength validation: failed.");
                return false;
            }
        }

        /// <summary>
        /// Determines whether string has any spaces
        /// </summary>
        /// <param name="word">string to be validated</param>
        /// <returns>"true if string has spaces, otherwise false"</returns>
        private static bool StringHasSpaces(string word)
        {
            if (word.Contains(" "))
            {
                Trc.Log(LogLevel.Always, "StringHasSpaces validation: succeeded.");
                return true;
            }
            else
            {
                Trc.Log(LogLevel.Always, "StringHasSpaces validation: failed.");
                return false;
            }
        }

        /// <summary>
        /// Determines whether string is empty
        /// </summary>
        /// <param name="word">string to be validated</param>
        /// <returns>"true if string is empty, otherwise false"</returns>
        private static bool StringIsEmpty(string word)
        {
            if (word.Length == 0)
            {
                Trc.Log(LogLevel.Always, "StringIsEmpty validation: succeeded.");
                return true;
            }
            else
            {
                Trc.Log(LogLevel.Always, "StringIsEmpty validation: failed.");
                return false;
            }
        }

        /// <summary>
        /// Checks whether there is a pending restart before installing Unified Setup/Unified Agent.
        /// </summary>
        /// <param name="reason">Reason of failure or success</param>
        /// <param name="checkForPendingRenamesAndMU">If true, checks are performed for pending file rename operations.</param>
        /// <returns>true if PendingFileRenameOperations and/or UpdateExeVolatile registry keys has some values,
        /// (or) if previous InDskFlt/involflt driver present without agent installation, false otherwise.</returns>
        public static bool RebootNotRequired(out string reason, bool checkForPendingRenamesAndMU = true)
        {
            bool returnCode = false;
            reason = "";
            string pendingFileRenameOperations = "";
            string updateExeVolatile = "";
            string errorReason = @"A Restart from a previous installation is pending. Please restart your server and try installation again. Learn more(https://aka.ms/v2a_reboot_warnings)." + "\n";

            try
            {
                if (checkForPendingRenamesAndMU)
                {
                    Trc.Log(
                        LogLevel.Always,
                        "Checking PendingFileRenameOperations and UpdateExeVolatile registry " +
                        "keys as it is Unified Setup installation.");

                    // Check whether PendingFileRenameOperations key has some value.                
                    pendingFileRenameOperations = Registry.GetValue(
                        @"HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\Session Manager",
                        "PendingFileRenameOperations",
                        string.Empty).ToString();

                    if (string.IsNullOrEmpty(pendingFileRenameOperations))
                    {
                        Trc.Log(LogLevel.Always, "PendingFileRenameOperations registry key does not exist.");
                    }
                    else
                    {
                        // string that constains the reason for check to fail.
                        reason = errorReason;
                        string[] pendingFileRenameOperationsContents = (string[])Registry.GetValue(
                            @"HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\Session Manager",
                            "PendingFileRenameOperations",
                            null);
                        Trc.Log(
                            LogLevel.Always,
                            "PendingFileRenameOperations contents are {0}",
                            string.Join(" ", pendingFileRenameOperationsContents));
                        return returnCode;
                    }

                    // Check whether UpdateExeVolatile key has some value.
                    updateExeVolatile = (string)Registry.GetValue(
                        @"HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Updates",
                        "UpdateExeVolatile",
                        string.Empty);

                    // System restart is not required if UpdateExeVolatile key does not exist or it's value is 0.
                    if (string.IsNullOrEmpty(updateExeVolatile) || updateExeVolatile == "0")
                    {
                        Trc.Log(LogLevel.Always, "UpdateExeVolatile registry key does not exist or it's value is 0.");
                    }
                    else
                    {
                        // string that constains the reason for check to fail.
                        reason = errorReason;
                        Trc.Log(LogLevel.Always, "UpdateExeVolatile registry key value is {0}", updateExeVolatile);
                        return returnCode;
                    }
                }
                else
                {
                    Trc.Log(
                        LogLevel.Always,
                        "Skipping PendingFileRenameOperations and " +
                            "UpdateExeVolatile registry checks as it is Unified Agent installation.");
                }

                // Check whether InDskFlt registry exists or not.                
                RegistryKey idfRegKey = Registry.LocalMachine.OpenSubKey(@"System\CurrentControlSet\Services\InDskFlt");
                if (idfRegKey == null)
                {
                    Trc.Log(LogLevel.Always, "InDskFlt key does not exist.");
                }
                else
                {
                    Trc.Log(LogLevel.Always, "InDskFlt key exists.");
                }

                // Check whether InDskFlt driver is present or not.
                bool idfDrvPst = SetupHelper.IsDriverPresent("InDskFlt");
                if (idfDrvPst)
                {
                    Trc.Log(LogLevel.Always, "InDskFlt driver is present.");
                }
                else
                {
                    Trc.Log(LogLevel.Always, "InDskFlt driver does not present.");
                }

                // Consider system is marked for reboot, if InDskFlt registry key does not exist, but InDskFlt driver is present.
                if (idfRegKey == null && idfDrvPst)
                {
                    // string that constains the reason for check to fail.
                    reason = errorReason;
                    Trc.Log(LogLevel.Always, @"HKEY_LOCAL_MACHINE\System\CurrentControlSet\Services\InDskFlt registry key does not exist, but InDskFlt driver is present.");
                    return returnCode;
                }

                // Check whether involflt registry exists or not.                
                RegistryKey ivfRegKey = Registry.LocalMachine.OpenSubKey(@"System\CurrentControlSet\Services\involflt");
                if (ivfRegKey == null)
                {
                    Trc.Log(LogLevel.Always, "involflt key does not exist.");
                }
                else
                {
                    Trc.Log(LogLevel.Always, "involflt key exists.");
                }

                // Check whether involflt driver is present or not
                bool ivfDrvPst = SetupHelper.IsDriverPresent("involflt");
                if (ivfDrvPst)
                {
                    Trc.Log(LogLevel.Always, "involflt driver is present.");
                }
                else
                {
                    Trc.Log(LogLevel.Always, "involflt driver does not present.");
                }

                // Consider system is marked for reboot, if involflt registry key does not exist, but involflt driver is present.
                if (ivfRegKey == null && ivfDrvPst)
                {
                    // string that constains the reason for check to fail.
                    reason = errorReason;
                    Trc.Log(LogLevel.Always, @"HKEY_LOCAL_MACHINE\System\CurrentControlSet\Services\involflt registry key does not exist, but involflt driver is present.");
                    return returnCode;
                }

                reason = @"Pending restart is not detected on this server." + "\n";
                return true;
            }
            catch (Exception e)
            {
                Trc.Log(LogLevel.Always, "Exception occurred: {0}", e.ToString());
                return returnCode;
            }
        }

        /// <summary>
        /// Checks whether server OS is Windows 2012 R2 and the server is not a domain controller
        /// </summary>
        /// <param name="info">Reason of failure or success</param>
        /// <returns>true if server OS is Windows 2012 R2 and server is not a domain controller, false otherwise</returns>
        public static bool IsWIN2K12R2AndNotDC(out string info)
        {
            info = "";
            bool returnCode = false;
            int prodTypeNum;

            try
            {
                // Check whether server OS is Windows 2012 R2
                bool isServerOSVersion2012R2 = IsServerOSVersion2012R2(out prodTypeNum);
                if (isServerOSVersion2012R2)
                {
                    Trc.Log(LogLevel.Always, "Server OS is Windows Server 2012 R2.");
                    info = StringResources.SupportedWin2K12R2Setup;
                    returnCode = true;
                }
                else
                {
                    if (prodTypeNum == 2)
                    {
                        Trc.Log(LogLevel.Always, "The system is a Domain controller.");
                        info = StringResources.DomainControllerSetup;
                        returnCode = false;
                    }
                    else
                    {
                        Trc.Log(LogLevel.Always, "Server OS is not Windows 2012 R2.");
                        info = StringResources.UnsupportedServerSetup;
                        returnCode = false;
                    }

                }
                return returnCode;
            }
            catch (Exception e)
            {
                Trc.Log(LogLevel.Always, "Exception occurred: {0}", e.ToString());
                return returnCode;
            }
        }

        /// <summary>
        /// Checks if current OS is Windows server 2012 R2.
        /// </summary>
        /// <returns>True is OS is server 2012 R2, false otherwise.</returns>
        public static bool IsServerOSVersion2012R2(out int prodTypeNum)
        {
            bool returnCode = false;
            Trc.Log(LogLevel.Always, "Checking for OS version");
            prodTypeNum = 1;
            const int ProductTypeServer = 3;

            try
            {
                // Query system for Operating System information
                ObjectQuery query = new ObjectQuery(
                    "SELECT version, producttype FROM Win32_OperatingSystem");

                ManagementScope scope = new ManagementScope("\\\\" + SetupHelper.GetFQDN() + "\\root\\cimv2");
                scope.Connect();

                ManagementObjectSearcher searcher = new ManagementObjectSearcher(scope, query);
                foreach (ManagementObject os in searcher.Get())
                {
                    Version winServer2012R2 = new Version("6.3.0");
                    Version currentOSVersion = new Version(os["version"].ToString());

                    Trc.Log(
                        LogLevel.Always,
                        "OS Details Version -> {0}, Type -> {1}",
                        os["version"].ToString(),
                        os["producttype"].ToString());

                    int productType = int.Parse(os["producttype"].ToString());

                    // Check for Win2k12R2 and product type is Server.
                    if (currentOSVersion >= winServer2012R2 && productType == ProductTypeServer)
                    {
                        prodTypeNum = 3;
                        returnCode = true;
                    }
                    // Check for Win2k12R2 and product type is Domain controller.
                    else if (currentOSVersion >= winServer2012R2 && productType == 2)
                    {
                        prodTypeNum = 2;
                        returnCode = false;
                    }
                    else
                    {
                        prodTypeNum = 1;
                        returnCode = false;
                    }
                }
                return returnCode;
            }
            catch (Exception exp)
            {
                Trc.LogException(LogLevel.Always, "Failed to determine OS", exp);
                return returnCode;
            }
        }

        /// <summary>
        /// Validates Proxy Settings File Path and it's contents.
        /// </summary>
        /// <param name="filePath">Proxy Settings File Path</param>
        /// <returns>true if proxy settings file exists and it's contents are valid, false otherwise</returns>
        public static bool ValidateProxySettingsFilePath(string filePath)
        {
            bool returnCode = false;

            try
            {
                if (File.Exists(filePath))
                {
                    Trc.Log(LogLevel.Always, "Proxy Setting File Path({0}) passed to the installer exists.", filePath);

                    // Read each line of the credentials file into a string array and exclude empty lines.
                    string[] lines = File.ReadAllLines(filePath).Where(line => !IsNullOrWhiteSpace(line)).ToArray();

                    // Check whether number of lines are 6.
                    if (lines.Length != 6)
                    {
                        Trc.Log(LogLevel.Always, "Number of lines in proxy settings file are not 6.");
                        return returnCode;
                    }
                    else
                    {
                        Trc.Log(LogLevel.Always, "Number of lines in proxy settings file are 6.");
                    }

                    // Check whether first line is in expected format
                    if (lines[0] == "[ProxySettings]")
                    {
                        Trc.Log(LogLevel.Always, "First line is in expected format.");
                    }
                    else
                    {
                        Trc.Log(LogLevel.Always, "First line is in not in expected format.");
                        return returnCode;
                    }

                    // Check whether second line is in expected format
                    Match match;
                    match = Regex.Match(lines[1], "^ProxyAuthentication[ \t]*=[ \t]*\".*\"$");
                    if (match.Success)
                    {
                        Trc.Log(LogLevel.Always, "Second line is in expected format.");
                    }
                    else
                    {
                        Trc.Log(LogLevel.Always, "Second line is in not in expected format.");
                        return returnCode;
                    }

                    // Check whether third line is in expected format
                    match = Regex.Match(lines[2], "^ProxyIP[ \t]*=[ \t]*\".*\"$");
                    if (match.Success)
                    {
                        Trc.Log(LogLevel.Always, "Third line is in expected format.");
                    }
                    else
                    {
                        Trc.Log(LogLevel.Always, "Third line is in not in expected format.");
                        return returnCode;
                    }

                    // Check whether fourth line is in expected format
                    match = Regex.Match(lines[3], "^ProxyPort[ \t]*=[ \t]*\".*\"$");
                    if (match.Success)
                    {
                        Trc.Log(LogLevel.Always, "Fourth line is in expected format.");
                    }
                    else
                    {
                        Trc.Log(LogLevel.Always, "Fourth line is in not in expected format.");
                        return returnCode;
                    }

                    // Check whether Fifth line is in expected format
                    match = Regex.Match(lines[4], "^ProxyUserName[ \t]*=[ \t]*\".*\"$");
                    if (match.Success)
                    {
                        Trc.Log(LogLevel.Always, "Fifth line is in expected format.");
                    }
                    else
                    {
                        Trc.Log(LogLevel.Always, "Fifth line is in not in expected format.");
                        return returnCode;
                    }

                    // Check whether Sixth line is in expected format
                    match = Regex.Match(lines[5], "^ProxyPassword[ \t]*=[ \t]*\".*\"$");
                    if (match.Success)
                    {
                        Trc.Log(LogLevel.Always, "Sixth line is in expected format.");
                    }
                    else
                    {
                        Trc.Log(LogLevel.Always, "Sixth line is in not in expected format.");
                        return returnCode;
                    }

                    // Validate ProxyAuthentication value
                    string proxyAuthentication = GrepUtils.GetKeyValueFromFile(filePath, "ProxyAuthentication");
                    Trc.Log(LogLevel.Always, "Validating ProxyAuthentication value - {0}", proxyAuthentication);
                    if (proxyAuthentication != "Yes" && proxyAuthentication != "No")
                    {
                        ConsoleUtils.Log(LogLevel.Error, "Invalid ProxyAuthentication value: " + proxyAuthentication + "\n");
                        return false;
                    }
                    else
                    {
                        Trc.Log(LogLevel.Always, "Successfully validated ProxyAuthentication value.");
                    }

                    // Validate ProxyIP value
                    String proxyAddress = GrepUtils.GetKeyValueFromFile(filePath, "ProxyIP");
                    Trc.Log(LogLevel.Always, "Validating ProxyAddress value - {0}", proxyAddress);
                    if (string.IsNullOrEmpty(proxyAddress))
                    {
                        ConsoleUtils.Log(LogLevel.Error, "ProxyAddress value is empty." + "\n");
                        return false;
                    }
                    else if (!SetupHelper.IsAddressHasHTTP(proxyAddress))
                    {
                        ConsoleUtils.Log(LogLevel.Error, "Invalid Proxy address. Enter Proxy address in http://<Proxy> format." + "\n");
                        return false;
                    }
                    else
                    {
                        Trc.Log(LogLevel.Always, "ProxyAddress has some value.");
                        PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.ProxyAddress, proxyAddress);
                    }

                    // Validate ProxyPort value
                    String proxyPort = GrepUtils.GetKeyValueFromFile(filePath, "ProxyPort");
                    Trc.Log(LogLevel.Always, "Validating ProxyPort value - {0}", proxyPort);
                    bool proxyPortValResult = SetupHelper.IsPortValid(proxyPort);
                    if (!proxyPortValResult)
                    {
                        ConsoleUtils.Log(LogLevel.Error, "Invalid ProxyPort value: " + proxyPort + "\n");
                        return false;
                    }
                    else
                    {
                        Trc.Log(LogLevel.Always, "Successfully validated ProxyPort value.");
                        PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.ProxyPort, proxyPort);
                    }

                    // Validate ProxyUserName and ProxyPassword values only if ProxyAuthentication value is set to yes
                    if (proxyAuthentication == "Yes")
                    {
                        // Validate ProxyUserName value
                        String proxyUserName = GrepUtils.GetKeyValueFromFile(filePath, "ProxyUserName");
                        if (String.IsNullOrEmpty(proxyUserName))
                        {
                            ConsoleUtils.Log(LogLevel.Error, "ProxyUserName value is empty." + "\n");
                            return false;
                        }
                        else
                        {
                            Trc.Log(LogLevel.Always, "ProxyUserName has some value.");
                            PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.ProxyUsername, proxyUserName);
                        }

                        // Validate ProxyPassword value
                        String proxyPassword = GrepUtils.GetKeyValueFromFile(filePath, "ProxyPassword");
                        if (String.IsNullOrEmpty(proxyPassword))
                        {
                            ConsoleUtils.Log(LogLevel.Error, "ProxyPassword value is empty." + "\n");
                            return false;
                        }
                        else
                        {
                            Trc.Log(LogLevel.Always, "ProxyPassword has some value.");
                            SecureString spassword = SecureStringHelper.StringToSecureString(proxyPassword);
                            PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.ProxyPassword, spassword);
                        }
                    }
                    else
                    {
                        Trc.Log(LogLevel.Always, "Not validating ProxyUserName and ProxyPassword values as ProxyAuthentication value is set to 'No'.");
                    }

                    // If control reaches here, ProxySettingsFile and it's contents are valid.
                    return true;
                }
                else
                {
                    Trc.Log(LogLevel.Always, "Proxy Setting File Path({0}) passed to the installer does not exist.", filePath);
                }
                return returnCode;
            }
            catch (Exception e)
            {
                Trc.Log(LogLevel.Always, "Exception occurred: {0}", e.ToString());
                return returnCode;
            }
        }

        /// <summary>
        /// Checks whether a 32-bit software is installed or not
        /// </summary>
        /// <param name="name">Name of the software to be checked</param>
        /// <param name="version">Version of the software installed</param>
        /// <returns>true if software is installed, false otherwise</returns>
        public static bool IsSoftwareInstalledWow6432(string name, out string version)
        {
            bool returnCode = false;
            version = "";
            try
            {
                string uninstallKey = UnifiedSetupConstants.RootRegKey + @"Microsoft\Windows\CurrentVersion\Uninstall";
                RegistryKey uninstallRegKey = Registry.LocalMachine.OpenSubKey(uninstallKey);
                string[] uninstallRegSubKeys = uninstallRegKey.GetSubKeyNames();

                // Loop through each subkey, get the version and return true when a match is found.
                foreach (string subkeyName in uninstallRegSubKeys)
                {
                    string displayName = (string)uninstallRegKey.OpenSubKey(subkeyName).GetValue("DisplayName");
                    if (!IsNullOrWhiteSpace(displayName) && displayName.Contains(name))
                    {
                        version = (string)uninstallRegKey.OpenSubKey(subkeyName).GetValue("DisplayVersion");
                        Trc.Log(LogLevel.Always, "Found {0} with version {1}", name, version);
                        return true;
                    }
                }
                return returnCode;
            }
            catch (Exception e)
            {
                Trc.Log(LogLevel.Always, "Exception Occured: {0}", e.ToString());
                return returnCode;
            }
        }

        /// <summary>
        /// Checks whether string is null or having whitespace
        /// </summary>
        /// <param name="value"></param>
        /// <returns>true if string is null or having whitespace, otherwise false</returns>
        public static bool IsNullOrWhiteSpace(this string value)
        {
            if (value == null) return true;
            return string.IsNullOrEmpty(value.Trim());
        }

        /// <summary>
        /// Returns the drive space available.
        /// </summary>
        public static long ReturnFreeSpaceAvail(string driveName)
        {
            DriveInfo driveinfo = new DriveInfo(driveName);
            if (driveinfo.IsReady && driveinfo.Name == driveName)
            {
                // Convert drive space to GB.
                long space_avail = driveinfo.TotalFreeSpace / (1024 * 1024 * 1024);
                return space_avail;
            }
            return -1;
        }

        /// <summary>
        /// Returns the NTFS drive with maximum space available.
        /// </summary>
        public static string LargestNTFSDrive()
        {
            string output = "";
            Dictionary<DriveInfo, long> driveSpaceDictionary = new Dictionary<DriveInfo, long>();
            try
            {
                // Get information of all drives
                DriveInfo[] allDrives = DriveInfo.GetDrives();
                foreach (DriveInfo driveInfo in allDrives)
                {
                    if (driveInfo.DriveType == DriveType.Fixed && driveInfo.DriveFormat.Equals("NTFS", StringComparison.OrdinalIgnoreCase))
                    {
                        // Get the free space available on the drive.
                        long freeSpace = driveInfo.TotalFreeSpace / (1024 * 1024 * 1024);
                        Trc.Log(LogLevel.Always, "{0} has {1} GB free space.", driveInfo, freeSpace);

                        // Adding key-value pair to the dictionary.
                        driveSpaceDictionary.Add(driveInfo, freeSpace);
                    }
                }

                // Get the drive with maximum space.
                DriveInfo largestDrive = driveSpaceDictionary.Aggregate((a, b) => a.Value > b.Value ? a : b).Key;
                output = largestDrive.ToString();
                Trc.Log(LogLevel.Always, "The largest NTFS drive available in the system is {0}.", output);
            }
            catch (Exception e)
            {
                Trc.Log(LogLevel.Always, "Exception occurred: {0}", e.ToString());
            }
            return output;
        }

        /// <summary>
        /// Opens up the log file passed as an argument.
        /// </summary>
        public static void OpenFile(string logPath)
        {
            try
            {
                Trc.Log(LogLevel.Always, "Trying to open {0}", logPath);
                if (File.Exists(logPath))
                {
                    Process.Start(new ProcessStartInfo(logPath));
                }
                else
                {
                    Trc.Log(LogLevel.Always, "Unable to find " + logPath);
                }
            }
            catch (Exception e)
            {
                Trc.Log(LogLevel.Always, "Exception occurred: {0}", e.ToString());
            }
        }

        /// <summary>
        ///  Validates install location. Gets install drive from install location and checks whether driver exists or not,
        ///  checks whether drive is fixed and it has >= 1 GB free space.
        /// </summary>
        /// <param name="installLocation">installation location to be validated</param>
        /// <returns>true if drive exists and it is a fixed drive and it has >=1 GB free space</returns>
        public static bool ValidateInstallLocationUA(string installLocation)
        {
            Trc.Log(LogLevel.Always, "Begin ValidateInstallLocationUA.");
            bool returnCode = false;

            try
            {
                // Get install drive from install location
                string installDrive = installLocation[0] + ":\\";
                installDrive = installDrive.ToUpper();
                Trc.Log(LogLevel.Always, "InstallLocation value: {0}", installLocation);
                Trc.Log(LogLevel.Always, "InstallDrive value: {0}", installDrive);

                // Get all the drives available on the system and loop through each.
                DriveInfo[] drives = DriveInfo.GetDrives();
                foreach (DriveInfo drive in drives)
                {
                    // Check whether drive exists or not.
                    if (drive.Name == installDrive)
                    {
                        Trc.Log(LogLevel.Always, "InstallDrive({0}) exists.", installDrive);

                        // Check whether drive type is of fixed.
                        if (drive.DriveType == DriveType.Fixed)
                        {
                            Trc.Log(LogLevel.Always, "InstallDrive({0}) is a fixed drive.", installDrive);
                        }
                        else
                        {
                            Trc.Log(LogLevel.Always, "InstallDrive({0}) is not a fixed drive.", installDrive);
                            return returnCode;
                        }

                        // Check whether selected drive has more than 1 GB free space.
                        long freeSpace = drive.TotalFreeSpace / (1024 * 1024 * 1024);
                        Trc.Log(LogLevel.Always, "InstallDrive({0}) has {1} GB free space", installDrive, freeSpace);

                        if (freeSpace >= 1)
                        {
                            Trc.Log(LogLevel.Always, "InstallDrive({0}) has more than 1 GB free space", installDrive);
                        }
                        else
                        {
                            Trc.Log(LogLevel.Always, "InstallDrive({0}) has less than 1 GB free space", installDrive);
                            return returnCode;
                        }

                        // If control reaches here, drive exists, it is a fixed drive and it has more than 1 GB freespace. So, return true.
                        Trc.Log(LogLevel.Always, "InstallDrive({0}) validation has succeeded.", installDrive);
                        return true;
                    }
                }
                Trc.Log(LogLevel.Always, "InstallDrive({0}) validation has failed.", installDrive);
                return returnCode;
            }
            catch (Exception e)
            {
                Trc.Log(LogLevel.Always, "Exception Occured: {0}", e.ToString());
                return returnCode;
            }
        }


        /// <summary>
        ///  Checks whether install location characters are within the ASCII characters range(32 to 126) or not.
        /// </summary>
        /// <param name="installation">installation location to be validated</param>
        /// <returns>true if install location characters are within the 32 to 126 ASCII range</returns>
        public static bool IsAsciiCheck(string installLocation)
        {
            Trc.Log(LogLevel.Always, "Begin IsAsciiCheck.");
            bool returnCode = false;

            try
            {
                // ASCII range(32 to 126) validation check
                string installlocStr = installLocation;
                char[] instLocChar = installlocStr.ToCharArray();
                for (int i = 0; i < instLocChar.Length; i++)
                {
                    int t = (int)instLocChar[i];
                    if (t >= 32 && t <= 126)
                    {
                        continue;
                    }
                    else
                    {
                        Trc.Log(LogLevel.Always, "ASCII characters validation with install location ({0}) has failed.", installlocStr);
                        return returnCode;
                    }
                }
                Trc.Log(LogLevel.Always, "ASCII characters validation with install location ({0}) has succeeded.", installlocStr);
                return true;
            }
            catch (Exception e)
            {
                Trc.Log(LogLevel.Always, "Exception Occured while IsAsciiCheck : {0}", e.ToString());
                return returnCode;
            }
        }


        /// <summary>
        /// Checks whether PS exists or not.
        /// </summary>
        /// <returns>true if PS exists, false otherwise.</returns>
        public static bool PSExists()
        {
            try
            {
                // Get PS version and check whether tmansvc service exists or not.
                String psVersion = (string)Registry.GetValue(UnifiedSetupConstants.CSPSRegistryName, "Version", string.Empty);
                bool tmansvcPresent = SetupHelper.IsServicePresent("tmansvc");

                if (!tmansvcPresent && string.IsNullOrEmpty(psVersion))
                {
                    Trc.Log(LogLevel.Always, "Process Server does not exist on this server.");
                    return false;
                }
                else
                {
                    Trc.Log(LogLevel.Always, "tmansvc service present: {0}.", tmansvcPresent);
                    Trc.Log(LogLevel.Always, "Process Server({0}) exists on this server.", psVersion);
                    return true;
                }
            }
            catch (Exception e)
            {
                Trc.Log(LogLevel.Always, "Exception Occured: {0}", e.ToString());
                return false;
            }
        }

        /// <summary>
        /// Fetches service status
        /// </summary>
        /// <param name="serviceName">name of the service</param>
        /// <returns>service status</returns>
        public static string GetServiceStatus(string serviceName)
        {
            try
            {
                ServiceController sc = new ServiceController(serviceName);
                Trc.Log(LogLevel.Always, "{0} service status: {1}", serviceName, sc.Status);

                switch (sc.Status)
                {
                    case ServiceControllerStatus.Running:
                        return "Running";
                    case ServiceControllerStatus.Stopped:
                        return "Stopped";
                    case ServiceControllerStatus.Paused:
                        return "Paused";
                    case ServiceControllerStatus.StopPending:
                        return "Stopping";
                    case ServiceControllerStatus.StartPending:
                        return "Starting";
                    default:
                        return "Status Changing";
                }
            }
            catch (InvalidOperationException ex)
            {
                Trc.LogException(LogLevel.Error, "Service not found", ex);
                return "UnableToGetStatus";
            }
        }

        /// <summary>
        /// Checks whether reboot is required post installation. If InDskFlt driver is unable to track changes, reboot is required.
        /// </summary>
        /// <returns>true if reboot is required post installation, false otherwise</returns>
        public static bool PostAgentInstallationRebootRequired()
        {
            string rebReqRegValue = "";
            bool returnCode = false;

            try
            {
                // Get the value of Reboot_Required
                rebReqRegValue = (string)Registry.GetValue(UnifiedSetupConstants.AgentRegistryName, "Reboot_Required", string.Empty);

                // Retrun true if Reboot_Required reg key is set to 'yes'.
                if (rebReqRegValue == "yes")
                {
                    Trc.Log(LogLevel.Always, "Reboot_Required registry key value is set to yes.");
                    return true;
                }
                else
                {
                    Trc.Log(LogLevel.Always, "Reboot_Required registry key value is set to {0}.", rebReqRegValue);
                    return returnCode;
                }
            }
            catch (Exception exp)
            {
                Trc.LogException(LogLevel.Always, "Failed to check whether post installation reboot is requried or not.", exp);
                return returnCode;
            }
        }

        /// <summary>
        /// Get the value of a key from the specified file.
        /// </summary>
        /// <param name="filePath">Path of the file</param>
        /// <param name="key">Name of the key</param>
        /// <returns>Value of the key</returns>
        public static string GetKeyValueFromFile(string filePath, string key)
        {
            string returnValue = "";
            try
            {
                char[] delimiterChars = { '=' };

                if (File.Exists(filePath))
                {
                    // Add all lines of the file to a string array.
                    string[] lines = File.ReadAllLines(filePath);
                    foreach (string line in lines)
                    {
                        // When the required match is found, split the line with = and remove " characters. Break the loop after first match.
                        if (line.StartsWith(key))
                        {
                            string[] words = line.Split(delimiterChars);
                            returnValue = words[1].Replace("\"", "").Trim();
                            break;
                        }
                    }
                }
                return returnValue;
            }
            catch (Exception ex)
            {
                Trc.Log(LogLevel.Always, "Exception occured: {0}", ex.ToString());
                return returnValue;
            }
        }

        // <summary>
        /// Deletes the directory passed as an argument.
        /// </summary>
        /// <returns>true if directory is deleted, false otherwise</returns>
        public static bool DeleteDirectory(string path)
        {
            bool returnCode = false;

            try
            {
                // Delete the directory if it exists.
                if (Directory.Exists(path))
                {
                    Trc.Log(LogLevel.Always, "\"" + path + "\" directory exists. Deleting it.");
                    Directory.Delete(path, true);
                    return true;
                }
                else
                {
                    Trc.Log(LogLevel.Always, "\"" + path + "\" directory is not available.");
                    return true;
                }
            }
            catch (Exception ex)
            {
                Trc.Log(LogLevel.Always, "Exception occured: {0}", ex.ToString());
                return returnCode;
            }
        }

        /// <summary>
        /// Files to be removed.
        /// </summary>
        public static void DeleteFile(string file)
        {
            try
            {
                // Remove the file if it exists.
                if (File.Exists(file))
                {
                    Trc.Log(LogLevel.Always, "Removing {0}", file);
                    File.Delete(file);
                }
                else
                {
                    Trc.Log(LogLevel.Always, "File {0} doesn't exist.", file);
                }
            }
            catch (Exception e)
            {
                Trc.Log(LogLevel.Always, "Exception occurred: {0}", e.ToString());
            }
        }

        /// <summary>
        /// Sets start-up type to Manual, sets recovery options to 'No Action' and kills a service if it in running/stopping state.
        /// </summary>
        /// <returns>true when service is killed succesfully, false otherwise</returns>
        public static bool KillService(string serviceName)
        {
            Trc.Log(LogLevel.Debug, "Entering KillService service: {0}", serviceName);

            bool result = true;
            ServiceController service = null;
            try
            {
                // Check whether service is installed or not.
                service = ServiceControlFunctions.IsServiceInstalled(serviceName);
                if (null == service)
                {
                    Trc.Log(LogLevel.Always, "{0} service is not installed.", serviceName);
                    return true;
                }

                Trc.Log(LogLevel.Always, "{0} service is installed and its status is: {1}", serviceName, service.Status);

                bool isChanged = ServiceControlFunctions.ChangeServiceStartupType(
                            serviceName,
                            NativeMethods.SERVICE_START_TYPE.SERVICE_DEMAND_START);

                // Set service start-up type to manual.
                if (isChanged)
                {
                    Trc.Log(LogLevel.Always, "Successfully changed {0} service startup type to manual.", serviceName);
                }
                else
                {
                    Trc.Log(LogLevel.Always, "Failed to change {0} service startup type to manual.", serviceName);
                }

                if (ServiceControllerStatus.Stopped == service.Status)
                {
                    Trc.Log(LogLevel.Always, "{0} is already stopped", serviceName);
                    return true;
                }
                ServiceControlFunctions.SetServiceFailureActions(serviceName: serviceName, numRestarts: 0, delayInMS: 0);

                // Kill the service if it is in running/stopping state.
                Process[] proc = Process.GetProcessesByName(serviceName);
                if (proc == null || proc.Length == 0 || proc[0].Id == 0)
                {
                    Trc.Log(LogLevel.Always, "{0} service is not running.", serviceName);
                    return true;
                }

                Trc.Log(LogLevel.Always, "Killing {0} service with id {1}.", serviceName, proc[0].Id);
                proc[0].Kill();
                Trc.Log(LogLevel.Always, "Killed {0} service succesfully.", serviceName);

                // Return true if control reaches here
                return result;
            }
            catch (Exception e)
            {
                // return false in case of exceptions.
                Trc.Log(LogLevel.Always, "Exception occurred: {0}", e.ToString());
                return false;
            }
            finally
            {
                if (null != service)
                {
                    service.Close();
                }
            }
        }

        /// <summary>
        /// Check the Operating System culture, UI culture and current culture of the setup.
        /// </summary>
        /// <returns>true when Operating System culture, UI culture and current culture is English, false otherwise</returns>
        public static bool IsOSEnglish(out string cultureType)
        {
            bool EnglishOSandUICulture = false;
            cultureType = "";
            try
            {
                string culture = System.Globalization.CultureInfo.CurrentCulture.Name;
                string uiCulture = System.Globalization.CultureInfo.CurrentUICulture.Name;
                string installedCulture = System.Globalization.CultureInfo.InstalledUICulture.Name;

                Trc.Log(LogLevel.Always, "Culture installed with Operating System: {0}", installedCulture);
                Trc.Log(LogLevel.Always, "UI Culture : {0}", uiCulture);
                Trc.Log(LogLevel.Always, "Current Culture : {0}", culture);

                Match installedCultureMatch = Regex.Match(installedCulture, "en-*");
                Match uiCultureMatch = Regex.Match(uiCulture, "en-*");
                Match cultureMatch = Regex.Match(culture, "en-*");

                if (installedCultureMatch.Success)
                {
                    Trc.Log(LogLevel.Always, "English operating system.");
                    if ((uiCultureMatch.Success) && (cultureMatch.Success))
                    {
                        Trc.Log(LogLevel.Always, "Current culture and current UI culture are English.");
                        EnglishOSandUICulture = true;
                    }
                    else
                    {
                        // Setting value to cultureType to display individual error message for different culture types.
                        Trc.Log(LogLevel.Always, "Non-English current culture and/or current UI.");
                        cultureType = UnifiedSetupConstants.NonEnglishUICulture;
                    }
                }
                else
                {
                    // Setting value to cultureType to display individual error message for different culture types.
                    Trc.Log(LogLevel.Always, "Non-English Operating System.");
                    cultureType = UnifiedSetupConstants.NonEnglishOSCulture;
                }
                return EnglishOSandUICulture;
            }
            catch (Exception e)
            {
                Trc.Log(LogLevel.Always, "Exception occurred: {0}", e.ToString());
                return EnglishOSandUICulture;
            }
        }

        /// <summary>
        /// Check the service startup type.
        /// </summary>
        /// <returns>false if startup type is set to disabled, true otherwise</returns>
        public static bool IsServiceStartupTypeDisabled(string serviceName)
        {
            bool isServiceStartupTypeDisabled = false;
            try
            {
                Trc.Log(LogLevel.Always, "Checking the " + serviceName + " service startup type.");

                string query = String.Format("SELECT * FROM Win32_Service WHERE Name = '{0}'", serviceName);
                ManagementObjectSearcher searcher = new ManagementObjectSearcher(query);
                if (searcher != null)
                {
                    ManagementObjectCollection services = searcher.Get();
                    foreach (ManagementObject service in services)
                    {
                        string startupType = service.GetPropertyValue("StartMode").ToString();
                        Trc.Log(LogLevel.Always, "The {0} service startup type is : {1}", serviceName, startupType);
                        if (startupType == "Disabled")
                        {
                            isServiceStartupTypeDisabled = true;
                        }
                    }
                }
                else
                {
                    Trc.Log(LogLevel.Always, "{0} service is not installed.", serviceName);
                    isServiceStartupTypeDisabled = false;
                }
                return isServiceStartupTypeDisabled;
            }
            catch (Exception e)
            {
                Trc.Log(LogLevel.Always, "Exception occurred: {0}", e.ToString());
                return isServiceStartupTypeDisabled;
            }
        }

        /// <summary>
        /// Check the enumeration of COM+ applications enumeration.
        /// </summary>
        /// <returns>false if fails to enumerate COM+ applications, true otherwise</returns>
        public static bool COMPlusApplicationsEnumeration()
        {
            try
            {
                Trc.Log(LogLevel.Always, "Enumerating the COM+ applications.");
                ICOMAdminCatalog cat = (ICOMAdminCatalog)Activator.CreateInstance(Type.GetTypeFromProgID("ComAdmin.COMAdminCatalog"));
                ICatalogCollection apps = (ICatalogCollection)cat.GetCollection("Applications");
                apps.Populate();

                Trc.Log(LogLevel.Always, "Enumerating the COM+ applications has been succeeded.");
                return true;
            }
            catch (Exception e)
            {
                Trc.Log(LogLevel.Always, "Failed to enumerate COM+ applications.");
                Trc.Log(LogLevel.Always, "Exception occurred: {0}", e.ToString());
                return false;
            }
        }

        /// <summary>
        ///  Validates log file apth.
        /// </summary>
        /// <param name="logFilePath">log file path to be validated</param>
        /// <returns>true if log file path is valid, false otherwise</returns>
        public static bool ValidateLogFilePath(String logFilePath)
        {
            bool returnCode = false;

            try
            {
                // Get install drive from install location
                Trc.Log(LogLevel.Always, "LogFilePath value: {0}", logFilePath);

                // Check whether log file path is a network path.
                if (logFilePath.StartsWith(@"\\"))
                {
                    Trc.Log(LogLevel.Always, "LogFilePath({0}) is a network path.", logFilePath);
                    return returnCode;
                }

                // Check whehter log file path is an absolute path
                if (!Path.IsPathRooted(logFilePath))
                {
                    Trc.Log(LogLevel.Always, "LogFilePath({0}) is not an absolute path.", logFilePath);
                    return returnCode;
                }

                // Create log file. File.CreateText() overwrites the file if it exists. In case of failure exceptions are caught.
                using (StreamWriter sw = File.CreateText(logFilePath))
                {
                    sw.WriteLine("This log file contains ASRUnifiedAgent.log and UnifiedAgentMSIInstall.log.");
                }
                return true;
            }
            catch (Exception e)
            {
                Trc.Log(LogLevel.Always, "Exception Occured: {0}", e.ToString());
                return returnCode;
            }
        }

        /// <summary>
        /// Check for component installation on the server.
        /// </summary>
        /// <returns>true if component is installed, false otherwise</returns>
        public static bool IsComponentInstalled(string componentName)
        {
            bool iscomponentInstalled = false;
            string installedVersion = "";

            // Validate the component installation on the server.
            if ((componentName == UnifiedSetupConstants.CXTPProductName) ||
                (componentName == UnifiedSetupConstants.CSPSProductName))
            {
                iscomponentInstalled =
                    ValidationHelper.IsSoftwareInstalledWow6432(
                    componentName,
                    out installedVersion);
            }
            else if(componentName == UnifiedSetupConstants.MS_MTProductName)
            {
                iscomponentInstalled = PropertyBagDictionary.Instance.GetProperty<bool>(PropertyBagConstants.IsProductInstalled);
            }
            else
            {
                iscomponentInstalled =
                    SetupHelper.IsInstalled(
                    componentName,
                    out installedVersion);
            }

            if (iscomponentInstalled)
            {
                Trc.Log(LogLevel.Always, "{0} is installed on the server.", componentName);
                iscomponentInstalled = true;
            }
            else
            {
                Trc.Log(LogLevel.Always, "{0} is not installed on the server.", componentName);
            }

            return iscomponentInstalled;
        }

        /// <summary>
        /// Detects .Net Framework version
        /// </summary>
        /// <returns>True is .Net Framework installed, false otherwise.</returns>
        public static bool GetDotNetVersionFromRegistry(string version)
        {
            // Opens the registry key for the .NET Framework entry.
            using (RegistryKey ndpKey = RegistryKey.OpenRemoteBaseKey(RegistryHive.LocalMachine, "").OpenSubKey(@"SOFTWARE\Microsoft\NET Framework Setup\NDP\"))
            {
                foreach (string versionKeyName in ndpKey.GetSubKeyNames())
                {
                    if (versionKeyName.StartsWith("v"))
                    {
                        RegistryKey versionKey = ndpKey.OpenSubKey(versionKeyName);
                        string versionString = (string)versionKey.GetValue("Version", "");
                        string install = versionKey.GetValue("Install", "").ToString();
                        if (versionString != "" && install == "1")
                        {
                            if (versionString.StartsWith(version))
                            {
                                Trc.Log(LogLevel.Always, ".Net Framework version " + versionKeyName + "-" + versionString + " is installed.");
                                return true;
                            }
                        }
                        else
                        {
                            foreach (string subKeyName in versionKey.GetSubKeyNames())
                            {
                                RegistryKey subKey = versionKey.OpenSubKey(subKeyName);
                                versionString = (string)subKey.GetValue("Version", "");
                                install = subKey.GetValue("Install", "").ToString();

                                if (versionString != "" && install == "1")
                                {
                                    if (versionString.StartsWith(version))
                                    {
                                        Trc.Log(LogLevel.Always, ".Net Framework version " + versionKeyName + "-" + subKeyName + "-" + versionString + " is installed.");
                                        return true;
                                    }
                                }
                            }
                        }
                    }
                }
                return false;
            }
        }

        /// <summary>
        /// Gets Agent post-install action status registry.
        /// </summary>
        public static bool IsAgentPostInstallActionSuccess()
        {
            string postInstallStatus = (string)Registry.GetValue(
                UnifiedSetupConstants.AgentRegistryName,
                UnifiedSetupConstants.PostInstallActionsStatusKey,
                string.Empty);
            Trc.Log(LogLevel.Always, "Agent post-install actions status : {0}", postInstallStatus);

            if (string.Equals(
                    postInstallStatus,
                    UnifiedSetupConstants.SuccessStatus,
                    StringComparison.InvariantCultureIgnoreCase))
            {
                return true;
            }
            else
            {
                return false;
            }
        }

        /// <summary>
        /// Validate VMware Tools.
        /// </summary>
        public static bool isVMwareToolsExists()
        {
            bool isVMwareToolsExists = false;
            try
            {
                Trc.Log(LogLevel.Always, "Checking for VMware Tools.");

                string getVMwareToolsVer;
                bool isvSphereCLIInstalled = SetupHelper.IsInstalled("VMware Tools", out getVMwareToolsVer);

                // Check for installed status of VMware Tools on the setup.
                if (isvSphereCLIInstalled)
                {
                    Trc.Log(LogLevel.Always, "VMware Tools version installed: {0}", getVMwareToolsVer);
                    isVMwareToolsExists = true;
                }
                else
                {
                    Trc.Log(LogLevel.Error, "VMware Tools is not installed in the setup.");
                }
            }
            catch (Exception e)
            {
                Trc.LogException(LogLevel.Error, "Unable to fetch VMware Tools details", e);
            }
            return isVMwareToolsExists;
        }

        /// <summary>
        /// Check whether MS/MT only installed.
        /// </summary>
        /// <returns>true if MS/MT only installed, false otherwise</returns>
        public static bool IsAgentOnlyInstalled(out string agentType)
        {
            agentType = "";

            if (!(ValidationHelper.IsComponentInstalled(UnifiedSetupConstants.CXTPProductName) ||
                ValidationHelper.IsComponentInstalled(UnifiedSetupConstants.CSPSProductName)))
            {
                Trc.Log(LogLevel.Always, "CXTP/CS/PS is not installed.");
                if (ValidationHelper.IsComponentInstalled(UnifiedSetupConstants.MS_MTProductName))
                {
                    agentType = SetupHelper.GetAgentInstalledRole().ToString();
                    return true;
                }
            }

            return false;
        }

        /// <summary>
        /// Gets the version of the specified build.
        /// </summary>
        /// <param name="buildName">Name of the build to retrive version.</param>
        /// <returns>Returns the version info.</returns>
        public static Version GetBuildVersion(string buildName)
        {
            // Get absolute path of component exe.
            string currDir =
                new System.IO.FileInfo(System.Reflection.Assembly.GetExecutingAssembly().Location).DirectoryName;
            string componentPath = Path.Combine(currDir, buildName);

            // Get the version of the current installer.
            FileVersionInfo fvi = FileVersionInfo.GetVersionInfo(componentPath);
            Version currentBuildVersion = new Version(fvi.FileVersion);

            return currentBuildVersion;
        }

        /// <summary>
        /// Replaces text in a file.
        /// </summary>
        /// <param name="filePath">Path of the text file.</param>
        /// <param name="initialString">Text to search for.</param>
        /// <param name="replaceString">Text to replace the search text.</param>
        public static bool ReplaceLineinFile(string filePath, string initialString, string replaceString)
        {
            try
            {
                string fileContent = string.Empty;
                using (StreamReader streamReader = new StreamReader(filePath))
                {
                    fileContent = streamReader.ReadToEnd();
                    streamReader.Close();
                }

                fileContent = Regex.Replace(fileContent, initialString + "[^\r\n]+", replaceString);

                using (StreamWriter streamWriter = new StreamWriter(filePath))
                {
                    streamWriter.Write(fileContent);
                    streamWriter.Close();
                }
                return true;
            }
            catch (Exception ex)
            {
                Trc.Log(
                    LogLevel.Error,
                    "Exception occurred while replacing line : {0}", ex.ToString());
                return false;
            }
        }

        /// <summary>
        /// Remove certificates from store.
        /// </summary>
        /// <returns>true if certificate removed, false otherwise</returns>
        public static bool RemoveCertificateFromStore(string certIssuerName)
        {
            string issuerName = null;

            try
            {
                //Set Store object to the local machine store
                X509Store store = new X509Store(StoreName.My, StoreLocation.LocalMachine);
                store.Open(OpenFlags.ReadWrite);

                //Loop through the certs
                foreach (X509Certificate2 cert in store.Certificates)
                {
                    issuerName = cert.Issuer;
                    if (issuerName.Contains(certIssuerName))
                    {
                        store.Remove(cert);
                    }

                }

                return true;
            }
            catch (Exception ex)
            {
                Trc.Log(
                    LogLevel.Error,
                    "Exception occurred while removing certificates from local store : {0}", ex.ToString());
                return false;
            }
        }

        /// <summary>
        /// Validates the file size.
        /// </summary>
        /// <param name="fileSizeInMB">required file size validation in MB</param>
        /// <param name="absoluteFileName">file name with absolute path</param>
        public static bool ValidateFileSize(
            long fileSizeInMB,
            string absoluteFileName)
        {
            Trc.Log(LogLevel.Always,
                "Validation of log - {0} for size - {1} MB.",
                absoluteFileName,
                fileSizeInMB);
            long fileSizeInBytes = fileSizeInMB * 1024 * 1024;
            FileInfo fileInfo = new FileInfo(absoluteFileName);
            bool result = (fileInfo.Length > fileSizeInBytes) ?
                true :
                false;
            return result;
        }

        /// <summary>
        /// Returns the drive space available.
        /// </summary>
        /// <param name="driveName">drive name on which free space validation runs</param>
        /// <returns>available space</returns>
        public static long FreeSpaceAvailOnDrive(string driveName)
        {
            long space_avail = 0;
            try
            {
                DriveInfo driveinfo = new DriveInfo(driveName);
                if (driveinfo.IsReady && driveinfo.Name == driveName)
                {
                    // Convert drive space to GB.
                    space_avail = driveinfo.TotalFreeSpace / (1024 * 1024 * 1024);
                }
                Trc.Log(LogLevel.Always,
                    "Available space on installation drive : {0} - {1} GB",
                    driveName,
                    space_avail.ToString());
            }
            catch (Exception ex)
            {
                Trc.LogErrorException(
                    "Exception occurred at GetAvailableMemory - {0}",
                    ex);
            }

            return space_avail;
        }

        /// <summary>
        /// Validates space on drive.
        /// </summary>
        /// <param name="driveName">drive name on which free space validation runs</param>
        /// <param name="driveName">drive name on which free space validation runs</param>
        /// <returns>true if required space available on drive, false otherwise</returns>
        public static bool ValidateSpaceOnDrive(string installDrive, int minimumSpaceInGB)
        {
            bool returnCode = false;
            try
            {
                int availableFreeSpace = (int)FreeSpaceAvailOnDrive(installDrive);

                if (availableFreeSpace >= minimumSpaceInGB)
                {
                    returnCode = true;
                }
            }
            catch (Exception ex)
            {
                Trc.LogErrorException(
                    "Exception occurred at GetAvailableMemory - {0}",
                    ex);
            }
            return returnCode;
        }

        /// <summary>
        /// Kill the process associated with exe name.
        /// </summary>
        /// <param name="processExeName">Exe name of process to be terminated</param>
        /// <returns>true on sucessfull termination of process, else returns false</returns>
        public static bool KillProcess(string processExeName)
        {
            bool result = false;

            try
            {
                foreach (var process in Process.GetProcessesByName(processExeName))
                {
                    Trc.Log(LogLevel.Always, "Process - {0}", process);
                    process.Kill();
                }

                result = true;
            }
            catch (Win32Exception ex)
            {
                Trc.Log(
                    LogLevel.Debug,
                    "Exception at KillProcess. Message: {0} ErrorCode: {1} StackTrace: {2}",
                    ex.Message,
                    ex.ErrorCode,
                    ex.StackTrace);
            }
            catch (NotSupportedException ex)
            {
                Trc.Log(
                    LogLevel.Debug,
                    "Exception at KillProcess. Message: {0} StackTrace: {1}",
                    ex.Message,
                    ex.StackTrace);
            }
            catch (InvalidOperationException ex)
            {
                Trc.Log(
                    LogLevel.Debug,
                    "Exception at KillProcess. Message: {0} StackTrace: {1}",
                    ex.Message,
                    ex.StackTrace);
            }
            catch (Exception ex)
            {
                Trc.Log(
                    LogLevel.Debug,
                    "Exception at KillProcess. Message: {0} StackTrace: {1}",
                    ex.Message,
                    ex.StackTrace);
            }

            return result;
        }


        # region FunctionsRequiredforPatch
        /// <summary>
        /// Check for required version of the CS/PS installed on the server.
        /// </summary>
        /// <returns>true if required version of the CS/PS is installed, false otherwise</returns>
        public static bool IsBaseVersionforPatchInstalled()
        {
            bool returnCode = false;
            try
            {
                // Check whether required base version of CS/PS is installed on the server.
                string cxVersion = (string)Registry.GetValue(UnifiedSetupConstants.CSPSRegistryName, "Version", string.Empty);
                if (string.IsNullOrEmpty(cxVersion))
                {
                    Trc.Log(LogLevel.Always, "Didn't find CS/PS installation on this server.");
                    return returnCode;
                }
                else
                {
                    Match match;
                    match = Regex.Match(cxVersion, "^9.0.0.0");
                    if (match.Success)
                    {
                        Trc.Log(LogLevel.Always, "9.0 GA version of CS/PS is installed on the server.");
                        return true;
                    }
                    else
                    {
                        Trc.Log(LogLevel.Always, "9.0 GA version of CS/PS is not installed on the server.");
                        return returnCode;
                    }
                }
            }
            catch (Exception e)
            {
                Trc.Log(LogLevel.Always, "Exception Occured: {0}", e.ToString());
                return returnCode;
            }
        }

        /// <summary>
        /// Get the CS/PS installation directory on the server.
        /// </summary>
        /// <returns>CX_TYPE value in amethyst.conf</returns>
        public static string GetCXType()
        {
            try
            {
                // Get CS/PS installation directory from the registry.
                string csInstallDir = (string)Registry.GetValue(UnifiedSetupConstants.CSPSRegistryName, "InstallDirectory", string.Empty);
                Trc.Log(LogLevel.Always, "CS/PS installation directory: {0}", csInstallDir);

                // Get CX_TYPE from amethyst.conf.
                string cxType = GetKeyValueFromFile(csInstallDir + @"\etc\amethyst.conf", "CX_TYPE");
                Trc.Log(LogLevel.Always, "CX_TYPE in drscout.conf: {0}", cxType);

                return cxType;
            }
            catch (Exception e)
            {
                Trc.Log(LogLevel.Always, "Exception Occured: {0}", e.ToString());
                return null;
            }
        }

        /// <summary>
        /// Check for CS installation on the server.
        /// </summary>
        /// <returns>true if CS is installed, false otherwise</returns>
        public static bool IsCSInstalled()
        {
            bool returnCode = false;
            try
            {
                // Calling GetCXType() function.
                string cxType = GetCXType();

                if (cxType == "3")
                {
                    Trc.Log(LogLevel.Always, "CS is installed on the server.");
                    return true;
                }
                else
                {
                    Trc.Log(LogLevel.Always, "CS is not installed on the server.");
                    return returnCode;
                }
            }
            catch (Exception e)
            {
                Trc.Log(LogLevel.Always, "Exception Occured: {0}", e.ToString());
                return returnCode;
            }
        }

        /// <summary>
        /// Check for PS installation on the server.
        /// </summary>
        /// <returns>true if PS is installed, false otherwise</returns>
        public static bool IsPSAloneInstalled()
        {
            bool returnCode = false;
            try
            {
                // Calling GetCXType() function.
                string cxType = GetCXType();

                if (cxType == "2")
                {
                    Trc.Log(LogLevel.Always, "PS alone is installed on the server.");
                    return true;
                }
                else
                {
                    Trc.Log(LogLevel.Always, "PS alone is not installed on the server.");
                    return returnCode;
                }
            }
            catch (Exception e)
            {
                Trc.Log(LogLevel.Always, "Exception Occured: {0}", e.ToString());
                return returnCode;
            }
        }

        /// <summary>
        /// Check for MT installation on the server.
        /// </summary>
        /// <returns>true if MT is installed, false otherwise</returns>
        public static bool IsMTInstalled()
        {
            bool returnCode = false;

            try
            {
                // Check whether MT is installed on the server.
                bool isMTinst = PropertyBagDictionary.Instance.GetProperty<bool>(PropertyBagConstants.IsProductInstalled);
                if (isMTinst)
                {
                    Trc.Log(LogLevel.Always, "MT is installed on this server.");
                    return true;
                }
                else
                {
                    Trc.Log(LogLevel.Always, "Did not find MT installation on this server.");
                    return returnCode;
                }
            }
            catch (Exception e)
            {
                Trc.Log(LogLevel.Always, "Exception Occured: {0}", e.ToString());
                return returnCode;
            }
        }

        /// <summary>
        /// Check for CS type is CSLegacy.
        /// </summary>
        /// <returns>true if CSLegacy, false otherwise</returns>
        public static bool IsCsLegacy()
        {
            return PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.CSType).Equals(ConfigurationServerType.CSLegacy.ToString(), StringComparison.OrdinalIgnoreCase);
        }

        /// <summary>
        /// Check for CS type is CSPrime.
        /// </summary>
        /// <returns>true if CSPrime, false otherwise</returns>
        public static bool IsCsPrime()
        {
            return PropertyBagDictionary.Instance.GetProperty<string>(PropertyBagConstants.CSType).Equals(ConfigurationServerType.CSPrime.ToString(), StringComparison.OrdinalIgnoreCase);
        }

        /// <summary>
        /// Get agent config input
        /// </summary>
        /// <param name="rcmCliPath">File path of the AzureRcmcli</param>
        /// <param name="output">Standard output of the executable</param>
        /// <returns>True if config update generated successfully else returns false</returns>
        public static bool GetAgentConfigInput(string rcmCliPath, out string output)
        {
            bool isConfigInputGenerated = false;
            int exitCode = 0;
            output = "";

            try
            {
                exitCode = CommandExecutor.RunCommand(rcmCliPath, UnifiedSetupConstants.AzureRcmCliAgentConfigInput, out output);
                output = output.Substring(output.LastIndexOf(':') + 1);
                if (exitCode == 0)
                {
                    Trc.Log(LogLevel.Always, "Agent config input: {0}", output);
                    Trc.Log(LogLevel.Always, "Successfully generated agentconfiginput.");
                    isConfigInputGenerated = true;
                }
                else
                {
                    Trc.Log(LogLevel.Always, "Failed to  generate agentconfiginput with exitcode: {0}.", exitCode);
                }
            }
            catch (Exception ex)
            {
                Trc.Log(LogLevel.Always, "Failed to  generate agentconfiginput with Exception: {0}", ex);
            }

            return isConfigInputGenerated;
        }

        # endregion
    }
}