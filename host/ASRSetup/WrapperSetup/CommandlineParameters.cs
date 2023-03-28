using System;
using System.Diagnostics;
using WpfResources;
using ASRResources;
using ASRSetupFramework;

namespace UnifiedSetup
{
    /// <summary>
    /// Delegate that parses a command line argument.
    /// </summary>
    /// <param name="commandlineArgument">Commandline parameter.</param>
    /// <param name="arguments">List of command line arguments.</param>
    /// <param name="index">Index of argument.</param>
    /// <returns>Argument parse value.</returns>
    public delegate bool ParseArgumentDelegate(CommandlineArgument commandlineArgument, string[] arguments, ref int index);

    /// <summary>
    /// Enumeration of all command line arguments
    /// </summary>
    public enum CommandlineParameterId
    {
        /// <summary>
        /// Cmdline help.
        /// </summary>
        Help,

        /// <summary>
        /// Upgrade Switch
        /// </summary>
        Upgrade,

        /// <summary>
        /// Show Thirdparty EULA
        /// </summary>
        ShowThirdpartyEULA,

        /// <summary>
        /// Accept Thirdparty EULA
        /// </summary>
        AcceptThirdpartyEULA,

        /// <summary>
        /// Skip Space Check
        /// </summary>
        SkipSpaceCheck,

        /// <summary>
        /// Skip Prerequisites check
        /// </summary>
        SkipPrereqsCheck,

        /// <summary>
        /// Skip TLS check
        /// </summary>
        SkipTLSCheck,

        /// <summary>
        /// Skip Registration
        /// </summary>
        SkipRegistration,

        /// <summary>
        /// Server Mode
        /// </summary>
        ServerMode,

        /// <summary>
        /// Installation Drive
        /// </summary>
        InstallLocation,

        /// <summary>
        /// MySQL Credentials File Path
        /// </summary>
        MySQLCredsFilePath,

        /// <summary>
        /// Vault Credentials File Path
        /// </summary>
        VaultCredsFilePath,

        /// <summary>
        /// Environment Type
        /// </summary>
        EnvType,

        /// <summary>
        /// Process Server IP
        /// </summary>
        PSIP,

        /// <summary>
        /// Azure IP
        /// </summary>
        AZUREIP,

        /// <summary>
        /// Configuration Server IP
        /// </summary>
        CSIP,

        /// <summary>
        /// Passphrase File Path
        /// </summary>
        PassphraseFilePath,

        /// <summary>
        /// Source Config File Path
        /// </summary>
        SourceConfigFilePath,

        /// <summary>
        /// Proxy Settings File Path
        /// </summary>
        ProxySettingsFilePath,

        /// <summary>
        /// Data Transfer Secure Port
        /// </summary>
        DataTransferSecurePort,

        /// <summary>
        /// Total count of params.
        /// </summary>
        Count
    }

    /// <summary>
    /// Represents one command line argument
    /// </summary>
    public class CommandlineArgument
    {
        /// <summary>
        /// Commandline parameter.
        /// </summary>
        private object parameter;

        /// <summary>
        /// If param is valid or not.
        /// </summary>
        private bool valid;

        /// <summary>
        /// CommandlineParam ID.
        /// </summary>
        private CommandlineParameterId commandlineParameterId;

        /// <summary>
        /// Argument parsing delegate.
        /// </summary>
        private ParseArgumentDelegate parseArgumentDelegate;

        /// <summary>
        /// Initializes a new instance of the CommandlineArgument class.
        /// </summary>
        /// <param name="commandlineParameterId">Parameter ID</param>
        /// <param name="parseArgumentDelegate">Delegate to parse the argument passed</param>
        /// <param name="defaultValue">default value of the parameter</param>
        public CommandlineArgument(CommandlineParameterId commandlineParameterId, ParseArgumentDelegate parseArgumentDelegate, object defaultValue)
        {
            this.parameter = defaultValue;
            this.valid = false;
            this.commandlineParameterId = commandlineParameterId;
            this.parseArgumentDelegate = parseArgumentDelegate;
        }

        #region Properties to access the fields

        /// <summary>
        /// Gets commandline parameter ID.
        /// </summary>
        public CommandlineParameterId ParameterId
        {
            get
            {
                return this.commandlineParameterId;
            }
        }

        /// <summary>
        /// Gets or sets parameter value.
        /// </summary>
        public object Parameter
        {
            get
            {
                return this.parameter;
            }

            set
            {
                this.parameter = value;
                this.valid = true;
            }
        }

        /// <summary>
        /// Gets a value indicating whether user set the parameter.
        /// </summary>
        public bool Valid
        {
            get
            {
                return this.valid;
            }
        }

        /// <summary>
        /// Gets the delegate to parse the argument passed.
        /// </summary>
        public ParseArgumentDelegate ParseArgumentDelegate
        {
            get
            {
                return this.parseArgumentDelegate;
            }
        }
        #endregion
    }

    /// <summary>
    /// Stores the values specified in command line
    /// </summary>
    [System.Diagnostics.CodeAnalysis.SuppressMessage(
    "Microsoft.StyleCop.CSharp.MaintainabilityRules",
    "SA1402:FileMayOnlyContainASingleClass",
    Justification = "Commandline param processing classes in one file.")]
    public class CommandlineParameters
    {
        /// <summary>
        /// Each argument starts with either of these markers
        /// </summary>
        private static readonly string[] CommandlineParameterMarkers = new string[] { "/", "-" };

        /// <summary>
        /// Table used to parse the command line arguments
        /// </summary>
        private CommandlineArgument[] commandlineParameterTable = new CommandlineArgument[]
        {  
            new CommandlineArgument(CommandlineParameterId.Help, new ParseArgumentDelegate(ParseHelpArgument), false),
            new CommandlineArgument(CommandlineParameterId.Upgrade, new ParseArgumentDelegate(ParseUpgradeArgument), false),
            new CommandlineArgument(CommandlineParameterId.ShowThirdpartyEULA, new ParseArgumentDelegate(ParseShowThirdpartyEULAArgument), false),
            new CommandlineArgument(CommandlineParameterId.AcceptThirdpartyEULA, new ParseArgumentDelegate(ParseAcceptThirdpartyEULAArgument), false),
            new CommandlineArgument(CommandlineParameterId.SkipSpaceCheck, new ParseArgumentDelegate(ParseSkipSpaceCheckArgument), false),
            new CommandlineArgument(CommandlineParameterId.SkipPrereqsCheck, new ParseArgumentDelegate(ParseSkipPrereqsCheckArgument), false), 
            new CommandlineArgument(CommandlineParameterId.SkipTLSCheck, new ParseArgumentDelegate(ParseSkipTLSCheckArgument), false), 
            new CommandlineArgument(CommandlineParameterId.SkipRegistration, new ParseArgumentDelegate(ParseSkipRegistrationArgument), false),
            new CommandlineArgument(CommandlineParameterId.ServerMode, new ParseArgumentDelegate(ParseServerModeArgument), null), 
            new CommandlineArgument(CommandlineParameterId.InstallLocation, new ParseArgumentDelegate(ParseInstallLocationArgument), null), 
            new CommandlineArgument(CommandlineParameterId.MySQLCredsFilePath, new ParseArgumentDelegate(ParseMySQLCredsFilePathArgument), null),
            new CommandlineArgument(CommandlineParameterId.VaultCredsFilePath, new ParseArgumentDelegate(ParseVaultCredsFilePathArgument), null),
            new CommandlineArgument(CommandlineParameterId.EnvType, new ParseArgumentDelegate(ParseEnvTypeArgument), null),
            new CommandlineArgument(CommandlineParameterId.PSIP, new ParseArgumentDelegate(ParsePSIPArgument), null),
            new CommandlineArgument(CommandlineParameterId.AZUREIP, new ParseArgumentDelegate(ParseAZUREIPArgument), null),
            new CommandlineArgument(CommandlineParameterId.CSIP, new ParseArgumentDelegate(ParseCSIPArgument), null),
            new CommandlineArgument(CommandlineParameterId.PassphraseFilePath, new ParseArgumentDelegate(ParsePassphraseFilePathArgument), null),
            new CommandlineArgument(CommandlineParameterId.SourceConfigFilePath, new ParseArgumentDelegate(ParseSourceConfigFilePathArgument), null),
            new CommandlineArgument(CommandlineParameterId.ProxySettingsFilePath, new ParseArgumentDelegate(ParseProxySettingsFilePathArgument), null),
            new CommandlineArgument(CommandlineParameterId.DataTransferSecurePort, new ParseArgumentDelegate(ParseDataTransferSecurePortArgument), null)
        };

        /// <summary>
        /// Initializes a new instance of the CommandlineParameters class.
        /// </summary>
        public CommandlineParameters()
        {
            // Sanity check
            for (CommandlineParameterId id = 0; id < CommandlineParameterId.Count; id++)
            {
                Trace.Assert(this.commandlineParameterTable[(int)id].ParameterId == id, "commandlineParameterTable is incorrectly initialized", string.Format("{0} != {1}", this.commandlineParameterTable[(int)id].ParameterId, id));
            }
        }

        /// <summary>
        /// Parses the command line. 
        /// In case of errors, shows the Error dialog and then rethrows the exception.
        /// </summary>
        /// <param name="arguments">Commandling arguments.</param>
        public void ParseCommandline(string[] arguments)
        {
            int index = 1, iterator = 0;

            try
            {
                // Parse command line arguments
                while (index < arguments.Length)
                {
                    Trc.Log(LogLevel.Debug, "Main : Parse argument {0}", arguments[index]);

                    if (IsArgument(arguments[index]))
                    {
                        for (iterator = 0; iterator < this.commandlineParameterTable.Length; iterator++)
                        {
                            if (this.commandlineParameterTable[iterator].ParseArgumentDelegate(this.commandlineParameterTable[iterator], arguments, ref index))
                            {
                                Trc.Log(LogLevel.Debug, "Main : The argument was parsed, move to next one.");
                                break;
                            }
                        }

                        if (iterator >= this.commandlineParameterTable.Length)
                        {
                            throw new Exception(string.Format("{0} {1} - {2}", StringResources.InvalidCommandLine, arguments[index], StringResources.CommandlineUsage));
                        }
                    }
                    else
                    {
                        throw new Exception(string.Format("{0} {1} - {2}", StringResources.InvalidCommandLine, arguments[index], StringResources.CommandlineUsage));
                    }
                }
            }
            catch (Exception backEndErrorException)
            {
                Trc.Log(LogLevel.Debug, "SpawnSetup : {0}", backEndErrorException.ToString());
                string message = string.Format("{0}\n\n{1}", backEndErrorException.Message, StringResources.CommandlineUsage);
                Trc.Log(LogLevel.Error, "Quite mode Error: {0}", message);
                ConsoleUtils.Log(LogLevel.Error, message);
            }
        }

        /// <summary>
        /// Gets the parameter Value based on the ID
        /// </summary>
        /// <param name="commandlineParameterId">Parameter ID</param>
        /// <returns>returns parameter value</returns>
        public object GetParameterValue(CommandlineParameterId commandlineParameterId)
        {
            return this.commandlineParameterTable[(int)commandlineParameterId].Parameter;
        }

        /// <summary>
        /// Whether the parameter is specified based on the parameter ID
        /// </summary>
        /// <param name="commandlineParameterId">Parameter ID</param>
        /// <returns>returns whether parameter is specified or not</returns>
        public bool IsParameterSpecified(CommandlineParameterId commandlineParameterId)
        {
            return this.commandlineParameterTable[(int)commandlineParameterId].Valid;
        }

        #region Argument interpretation helpers

        /// <summary>
        /// Is the argument given by user a valid argument.
        /// </summary>
        /// <param name="argument">Commandline argument.</param>
        /// <returns>returns whether it is valid or not.</returns>
        private static bool IsArgument(string argument)
        {
            foreach (string marker in CommandlineParameterMarkers)
            {
                if (argument.StartsWith(marker))
                {
                    return true;
                }
            }

            return false;
        }

        /// <summary>
        /// Should the calling function consume this argument, is it meant for it
        /// </summary>
        /// <param name="commandlineArgument">One complete commandline argument</param>
        /// <param name="argument">The argument passed by user</param>
        /// <param name="parameter">The paramenter value the calling function represents</param>
        /// <returns>whether this is the argument is for the calling function</returns>
        private static bool ConsumeArgument(CommandlineArgument commandlineArgument, string argument, string parameter)
        {
            foreach (string marker in CommandlineParameterMarkers)
            {
                int comparison = string.Compare(argument, marker + parameter, true);
                if (comparison == 0)
                {
                    if (commandlineArgument.Valid == true)
                    {
                        Trc.Log(LogLevel.Error, "Argument {0} already set", argument);
                        throw new Exception(string.Format(StringResources.CmdlineParameterAlreadySpecified, argument));
                    }

                    return true;
                }
            }

            return false;
        }

        /// <summary>
        /// Get the value of the argument if it is valid and sets it in CommandlineArgument
        /// </summary>
        /// <param name="commandlineArgument">One complete commandline argument</param>
        /// <param name="arguments">All arguments user passed</param>
        /// <param name="index">the index of current argument</param>
        /// <returns>returns whether we found the value</returns>
        private static bool FetchValue(CommandlineArgument commandlineArgument, string[] arguments, int index)
        {
            if (index + 1 < arguments.Length)
            {
                if (IsArgument(arguments[index + 1]))
                {
                    Trc.Log(LogLevel.Debug, "The value looks like another argument");

                    throw new Exception(string.Format("{0} - {1}", StringResources.CmdlineParameterMissing, arguments[index]));
                }

                index++;
                commandlineArgument.Parameter = arguments[index];

                return true;
            }
            else
            {
                Trc.Log(LogLevel.Debug, "Commandline parameter value not specified");
                throw new Exception(string.Format("{0} - {1}", StringResources.CmdlineParameterMissing, arguments[index]));
            }
        }

        #endregion

        #region Parsing routines

        /// <summary>
        /// If we find the help argument, then set its value in the commandline argument
        /// </summary>
        /// <param name="commandlineArgument">One complete commandline argument</param>
        /// <param name="arguments">All arguments user passed</param>
        /// <param name="index">the index of current argument</param>
        /// <returns>returns whether we found the help argument</returns>
        private static bool ParseHelpArgument(CommandlineArgument commandlineArgument, string[] arguments, ref int index)
        {
            if (ConsumeArgument(commandlineArgument, arguments[index], "?"))
            {
                commandlineArgument.Parameter = true;
                index++;
                return true;
            }

            return false;
        }

        /// <summary>
        /// If we find the upgrade argument, then set its value in the commandline argument
        /// </summary>
        /// <param name="commandlineArgument">One complete commandline argument</param>
        /// <param name="arguments">All arguments user passed</param>
        /// <param name="index">the index of current argument</param>
        /// <returns>returns whether we found the upgrade argument</returns>
        private static bool ParseUpgradeArgument(CommandlineArgument commandlineArgument, string[] arguments, ref int index)
        {
            if (ConsumeArgument(commandlineArgument, arguments[index], "Upgrade"))
            {
                commandlineArgument.Parameter = true;
                index++;
                return true;
            }

            return false;
        }  

        /// <summary>
        /// If we find the ShowThirdpartyEULA argument, then set its value in the commandline argument
        /// </summary>
        /// <param name="commandlineArgument">One complete commandline argument</param>
        /// <param name="arguments">All arguments user passed</param>
        /// <param name="index">the index of current argument</param>
        /// <returns>returns whether we found the ShowThirdpartyEULA argument</returns>
        private static bool ParseShowThirdpartyEULAArgument(CommandlineArgument commandlineArgument, string[] arguments, ref int index)
        {
            if (ConsumeArgument(commandlineArgument, arguments[index], "ShowThirdpartyEULA"))
            {
                commandlineArgument.Parameter = true;
                index++;
                return true;
            }

            return false;
        }

        /// <summary>
        /// If we find the AcceptThirdpartyEULA argument, then set its value in the commandline argument
        /// </summary>
        /// <param name="commandlineArgument">One complete commandline argument</param>
        /// <param name="arguments">All arguments user passed</param>
        /// <param name="index">the index of current argument</param>
        /// <returns>returns whether we found the AcceptThirdpartyEULA argument</returns>
        private static bool ParseAcceptThirdpartyEULAArgument(CommandlineArgument commandlineArgument, string[] arguments, ref int index)
        {
            if (ConsumeArgument(commandlineArgument, arguments[index], "AcceptThirdpartyEULA"))
            {
                commandlineArgument.Parameter = true;
                index++;
                return true;
            }

            return false;
        }

        /// <summary>
        /// If we find the SkipSpaceCheck argument, then set its value in the commandline argument
        /// </summary>
        /// <param name="commandlineArgument">One complete commandline argument</param>
        /// <param name="arguments">All arguments user passed</param>
        /// <param name="index">the index of current argument</param>
        /// <returns>returns whether we found the SkipSpaceCheck argument</returns>
        private static bool ParseSkipSpaceCheckArgument(CommandlineArgument commandlineArgument, string[] arguments, ref int index)
        {
            if (ConsumeArgument(commandlineArgument, arguments[index], "SkipSpaceCheck"))
            {
                commandlineArgument.Parameter = true;
                index++;
                return true;
            }

            return false;
        }

        /// <summary>
        /// If we find the SkipPrereqsCheck argument, then set its value in the commandline argument
        /// </summary>
        /// <param name="commandlineArgument">One complete commandline argument</param>
        /// <param name="arguments">All arguments user passed</param>
        /// <param name="index">the index of current argument</param>
        /// <returns>returns whether we found the SkipPrereqsCheck argument</returns>
        private static bool ParseSkipPrereqsCheckArgument(CommandlineArgument commandlineArgument, string[] arguments, ref int index)
        {
            if (ConsumeArgument(commandlineArgument, arguments[index], "SkipPrereqsCheck"))
            {
                commandlineArgument.Parameter = true;
                index++;
                return true;
            }

            return false;
        }

        /// <summary>
        /// If we find the SkipTLSCheck argument, then set its value in the commandline argument
        /// </summary>
        /// <param name="commandlineArgument">One complete commandline argument</param>
        /// <param name="arguments">All arguments user passed</param>
        /// <param name="index">the index of current argument</param>
        /// <returns>returns whether we found the SkipTLSCheck argument</returns>
        private static bool ParseSkipTLSCheckArgument(CommandlineArgument commandlineArgument, string[] arguments, ref int index)
        {
            if (ConsumeArgument(commandlineArgument, arguments[index], "SkipTLSCheck"))
            {
                commandlineArgument.Parameter = true;
                index++;
                return true;
            }

            return false;
        }

        /// <summary>
        /// If we find the SkipRegistration argument, then set its value in the commandline argument
        /// </summary>
        /// <param name="commandlineArgument">One complete commandline argument</param>
        /// <param name="arguments">All arguments user passed</param>
        /// <param name="index">the index of current argument</param>
        /// <returns>returns whether we found the SkipRegistration argument</returns>
        private static bool ParseSkipRegistrationArgument(CommandlineArgument commandlineArgument, string[] arguments, ref int index)
        {
            if (ConsumeArgument(commandlineArgument, arguments[index], "SkipRegistration"))
            {
                commandlineArgument.Parameter = true;
                index++;
                return true;
            }

            return false;
        }

        /// <summary>
        /// If we find the ServerMode argument, then set its value in the commandline argument
        /// </summary>
        /// <param name="commandlineArgument">One complete commandline argument</param>
        /// <param name="arguments">All arguments user passed</param>
        /// <param name="index">the index of current argument</param>
        /// <returns>returns whether we found the ServerMode argument</returns>
        private static bool ParseServerModeArgument(CommandlineArgument commandlineArgument, string[] arguments, ref int index)
        {
            if (ConsumeArgument(commandlineArgument, arguments[index], "ServerMode"))
            {
                if (FetchValue(commandlineArgument, arguments, index))
                {
                    index++;
                    arguments[index] = (string)commandlineArgument.Parameter;

                    Trc.Log(LogLevel.Always, "{0} received", "ServerMode");
                    index++;
                    return true;
                }
            }

            return false;
        }

        /// <summary>
        /// If we find the InstallLocation argument, then set its value in the commandline argument
        /// </summary>
        /// <param name="commandlineArgument">One complete commandline argument</param>
        /// <param name="arguments">All arguments user passed</param>
        /// <param name="index">the index of current argument</param>
        /// <returns>returns whether we found the InstallLocation argument</returns>
        private static bool ParseInstallLocationArgument(CommandlineArgument commandlineArgument, string[] arguments, ref int index)
        {
            if (ConsumeArgument(commandlineArgument, arguments[index], "InstallLocation"))
            {
                if (FetchValue(commandlineArgument, arguments, index))
                {
                    index++;
                    arguments[index] = (string)commandlineArgument.Parameter;

                    Trc.Log(LogLevel.Always, "{0} received", "InstallLocation");
                    index++;
                    return true;
                }
            }

            return false;
        }

        /// <summary>
        /// If we find the MySQLCredsFilePath argument, then set its value in the commandline argument
        /// </summary>
        /// <param name="commandlineArgument">One complete commandline argument</param>
        /// <param name="arguments">All arguments user passed</param>
        /// <param name="index">the index of current argument</param>
        /// <returns>returns whether we found the MySQLCredsFilePath argument</returns>
        private static bool ParseMySQLCredsFilePathArgument(CommandlineArgument commandlineArgument, string[] arguments, ref int index)
        {
            if (ConsumeArgument(commandlineArgument, arguments[index], "MySQLCredsFilePath"))
            {
                if (FetchValue(commandlineArgument, arguments, index))
                {
                    index++;
                    arguments[index] = (string)commandlineArgument.Parameter;

                    Trc.Log(LogLevel.Always, "{0} received", "MySQLCredsFilePath");
                    index++;
                    return true;
                }
            }

            return false;
        }

        /// <summary>
        /// If we find the VaultCredsFilePath argument, then set its value in the commandline argument
        /// </summary>
        /// <param name="commandlineArgument">One complete commandline argument</param>
        /// <param name="arguments">All arguments user passed</param>
        /// <param name="index">the index of current argument</param>
        /// <returns>returns whether we found the VaultCredsFilePath argument</returns>        
        private static bool ParseVaultCredsFilePathArgument(CommandlineArgument commandlineArgument, string[] arguments, ref int index)
        {
            if (ConsumeArgument(commandlineArgument, arguments[index], "VaultCredsFilePath"))
            {
                if (FetchValue(commandlineArgument, arguments, index))
                {
                    index++;
                    arguments[index] = (string)commandlineArgument.Parameter;

                    Trc.Log(LogLevel.Always, "{0} received", "VaultCredsFilePath");
                    index++;
                    return true;
                }
            }

            return false;
        }

        /// <summary>
        /// If we find the EnvType argument, then set its value in the commandline argument
        /// </summary>
        /// <param name="commandlineArgument">One complete commandline argument</param>
        /// <param name="arguments">All arguments user passed</param>
        /// <param name="index">the index of current argument</param>
        /// <returns>returns whether we found the EnvType argument</returns>   
        private static bool ParseEnvTypeArgument(CommandlineArgument commandlineArgument, string[] arguments, ref int index)
        {
            if (ConsumeArgument(commandlineArgument, arguments[index], "EnvType"))
            {
                if (FetchValue(commandlineArgument, arguments, index))
                {
                    index++;
                    arguments[index] = (string)commandlineArgument.Parameter;

                    Trc.Log(LogLevel.Always, "{0} received", "EnvType");
                    index++;
                    return true;
                }
            }

            return false;
        }

        /// <summary>
        /// If we find the PSIP argument, then set its value in the commandline argument
        /// </summary>
        /// <param name="commandlineArgument">One complete commandline argument</param>
        /// <param name="arguments">All arguments user passed</param>
        /// <param name="index">the index of current argument</param>
        /// <returns>returns whether we found the PSIP argument</returns>   
        private static bool ParsePSIPArgument(CommandlineArgument commandlineArgument, string[] arguments, ref int index)
        {
            if (ConsumeArgument(commandlineArgument, arguments[index], "PSIP"))
            {
                if (FetchValue(commandlineArgument, arguments, index))
                {
                    index++;
                    arguments[index] = (string)commandlineArgument.Parameter;

                    Trc.Log(LogLevel.Always, "{0} received", "PSIP");
                    index++;
                    return true;
                }
            }

            return false;
        }

        /// <summary>
        /// If we find the AZUREIP argument, then set its value in the commandline argument
        /// </summary>
        /// <param name="commandlineArgument">One complete commandline argument</param>
        /// <param name="arguments">All arguments user passed</param>
        /// <param name="index">the index of current argument</param>
        /// <returns>returns whether we found the AZUREIP argument</returns>   
        private static bool ParseAZUREIPArgument(CommandlineArgument commandlineArgument, string[] arguments, ref int index)
        {
            if (ConsumeArgument(commandlineArgument, arguments[index], "AZUREIP"))
            {
                if (FetchValue(commandlineArgument, arguments, index))
                {
                    index++;
                    arguments[index] = (string)commandlineArgument.Parameter;

                    Trc.Log(LogLevel.Always, "{0} received", "AZUREIP");
                    index++;
                    return true;
                }
            }

            return false;
        } 

        /// <summary>
        /// If we find the CSIP argument, then set its value in the commandline argument
        /// </summary>
        /// <param name="commandlineArgument">One complete commandline argument</param>
        /// <param name="arguments">All arguments user passed</param>
        /// <param name="index">the index of current argument</param>
        /// <returns>returns whether we found the CSIP argument</returns>   
        private static bool ParseCSIPArgument(CommandlineArgument commandlineArgument, string[] arguments, ref int index)
        {
            if (ConsumeArgument(commandlineArgument, arguments[index], "CSIP"))
            {
                if (FetchValue(commandlineArgument, arguments, index))
                {
                    index++;
                    arguments[index] = (string)commandlineArgument.Parameter;

                    Trc.Log(LogLevel.Always, "{0} received", "CSIP");
                    index++;
                    return true;
                }
            }

            return false;
        }

        /// <summary>
        /// If we find the PassphraseFilePath argument, then set its value in the commandline argument
        /// </summary>
        /// <param name="commandlineArgument">One complete commandline argument</param>
        /// <param name="arguments">All arguments user passed</param>
        /// <param name="index">the index of current argument</param>
        /// <returns>returns whether we found the PassphraseFilePath argument</returns>   
        private static bool ParsePassphraseFilePathArgument(CommandlineArgument commandlineArgument, string[] arguments, ref int index)
        {
            if (ConsumeArgument(commandlineArgument, arguments[index], "PassphraseFilePath"))
            {
                if (FetchValue(commandlineArgument, arguments, index))
                {
                    index++;
                    arguments[index] = (string)commandlineArgument.Parameter;

                    Trc.Log(LogLevel.Always, "{0} received", "PassphraseFilePath");
                    index++;
                    return true;
                }
            }

            return false;
        }
  
        /// <summary>
        /// If we find the SourceConfigFilePath argument, then set its value in the commandline argument
        /// </summary>
        /// <param name="commandlineArgument">One complete commandline argument</param>
        /// <param name="arguments">All arguments user passed</param>
        /// <param name="index">the index of current argument</param>
        /// <returns>returns whether we found the SourceConfigFilePath argument</returns>
        private static bool ParseSourceConfigFilePathArgument(CommandlineArgument commandlineArgument, string[] arguments, ref int index)
        {
            if (ConsumeArgument(commandlineArgument, arguments[index], "SourceConfigFilePath"))
            {
                if (FetchValue(commandlineArgument, arguments, index))
                {
                    index++;
                    arguments[index] = (string)commandlineArgument.Parameter;

                    Trc.Log(LogLevel.Always, "{0} received", "SourceConfigFilePath");
                    index++;
                    return true;
                }
            }

            return false;
        }

        /// <summary>
        /// If we find the ProxySettingsFilePath argument, then set its value in the commandline argument
        /// </summary>
        /// <param name="commandlineArgument">One complete commandline argument</param>
        /// <param name="arguments">All arguments user passed</param>
        /// <param name="index">the index of current argument</param>
        /// <returns>returns whether we found the PassphraseFilePath argument</returns>   
        private static bool ParseProxySettingsFilePathArgument(CommandlineArgument commandlineArgument, string[] arguments, ref int index)
        {
            if (ConsumeArgument(commandlineArgument, arguments[index], "ProxySettingsFilePath"))
            {
                if (FetchValue(commandlineArgument, arguments, index))
                {
                    index++;
                    arguments[index] = (string)commandlineArgument.Parameter;

                    Trc.Log(LogLevel.Always, "{0} received", "ProxySettingsFilePath");
                    index++;
                    return true;
                }
            }

            return false;
        }        
 
        /// <summary>
        /// If we find the DataTransferSecurePort argument, then set its value in the commandline argument
        /// </summary>
        /// <param name="commandlineArgument">One complete commandline argument</param>
        /// <param name="arguments">All arguments user passed</param>
        /// <param name="index">the index of current argument</param>
        /// <returns>returns whether we found the PassphraseFilePath argument</returns>   
        private static bool ParseDataTransferSecurePortArgument(CommandlineArgument commandlineArgument, string[] arguments, ref int index)
        {
            if (ConsumeArgument(commandlineArgument, arguments[index], "DataTransferSecurePort"))
            {
                if (FetchValue(commandlineArgument, arguments, index))
                {
                    index++;
                    arguments[index] = (string)commandlineArgument.Parameter;

                    Trc.Log(LogLevel.Always, "{0} received", "DataTransferSecurePort");
                    index++;
                    return true;
                }
            }

            return false;
        }

        #endregion
    }
}
