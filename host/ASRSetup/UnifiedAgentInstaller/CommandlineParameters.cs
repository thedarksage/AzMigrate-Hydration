//---------------------------------------------------------------
//  <copyright file="CommandlineParameters.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Holds logic to parse commandline arguments.
//  </summary>
//
//  History:     03-Jun-2016   ramjsing     Created
//----------------------------------------------------------------

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using ASRResources;
using ASRSetupFramework;

namespace UnifiedAgentInstaller
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
        /// Virtualization platform to support.
        /// </summary>
        Platform,

        /// <summary>
        /// Server Mode
        /// </summary>
        Role,

        /// <summary>
        /// Installation Location
        /// </summary>
        InstallLocation,

        /// <summary>
        /// Silent installation/upgrade
        /// </summary>
        Silent,

        /// <summary>
        /// Skip Prerequisite checks.
        /// </summary>
        SkipPrereqChecks,

        /// <summary>
        /// Path for the combined log file.
        /// </summary>
        LogFilePath,

        /// <summary>
        /// Agent Invoker
        /// </summary>
        Invoker,

        /// <summary>
        /// Path for the summary log file.
        /// </summary>
        SummaryFilePath,

        /// <summary>
        /// Mobiltiy Service validations output json file path.
        /// </summary>
        ValidationsOutputJsonFilePath,

        /// <summary>
        /// Installation type - Install/Upgrade
        /// </summary>
        InstallationType,

        /// <summary>
        /// Cstype - CSLegacy/CSPrime
        /// </summary>
        CSType,

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
            new CommandlineArgument(CommandlineParameterId.Platform, CreateArgumentParser(CommandlineParameterId.Platform), null),
            new CommandlineArgument(CommandlineParameterId.Role, CreateArgumentParser(CommandlineParameterId.Role), null),
            new CommandlineArgument(CommandlineParameterId.InstallLocation, CreateArgumentParser(CommandlineParameterId.InstallLocation), null),
            new CommandlineArgument(CommandlineParameterId.Silent, CreateSwitchArgumentParser(CommandlineParameterId.Silent), null),
            new CommandlineArgument(CommandlineParameterId.SkipPrereqChecks, CreateSwitchArgumentParser(CommandlineParameterId.SkipPrereqChecks), null),
            new CommandlineArgument(CommandlineParameterId.LogFilePath, CreateArgumentParser(CommandlineParameterId.LogFilePath), null),
            new CommandlineArgument(CommandlineParameterId.Invoker, CreateArgumentParser(CommandlineParameterId.Invoker), null),
            new CommandlineArgument(CommandlineParameterId.SummaryFilePath, CreateArgumentParser(CommandlineParameterId.SummaryFilePath), null),
            new CommandlineArgument(CommandlineParameterId.ValidationsOutputJsonFilePath, CreateArgumentParser(CommandlineParameterId.ValidationsOutputJsonFilePath), null),
            new CommandlineArgument(CommandlineParameterId.InstallationType, CreateArgumentParser(CommandlineParameterId.InstallationType), null),
            new CommandlineArgument(CommandlineParameterId.CSType, CreateArgumentParser(CommandlineParameterId.CSType), null)
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
        public bool ParseCommandline(string[] arguments)
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
                            Trc.Log(LogLevel.Debug, "Main : Calling parse function for {0}", this.commandlineParameterTable[iterator].ParameterId);

                            if (this.commandlineParameterTable[iterator].ParseArgumentDelegate(this.commandlineParameterTable[iterator], arguments, ref index))
                            {
                                Trc.Log(LogLevel.Debug, "Main : The argument was parsed, move to next one.");
                                break;
                            }
                        }

                        if (iterator >= this.commandlineParameterTable.Length)
                        {
                            throw new Exception(string.Format("{0} {1} - {2}", StringResources.InvalidCommandLine, arguments[index], StringResources.CommandlineUsageAgent));
                        }
                    }
                    else
                    {
                        throw new Exception(string.Format("{0} {1} - {2}", StringResources.InvalidCommandLine, arguments[index], StringResources.CommandlineUsageAgent));
                    }
                }

                return true;
            }
            catch(OutOfMemoryException)
            {
                throw;
            }
            catch (Exception backEndErrorException)
            {
                Trc.Log(LogLevel.Debug, "SpawnSetup : {0}", backEndErrorException.ToString());
                string message = string.Format("{0}\n\n{1}", backEndErrorException.Message, StringResources.CommandlineUsageAgent);
                Trc.Log(LogLevel.Error, "Quite mode Error: {0}", message);
                ConsoleUtils.Log(LogLevel.Error, message);
                return false;
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
        /// Creates a delegate for the argument, which sets the commandline argument value we find the value of the parameter ID passed.
        /// </summary>
        /// <param name="id">Command line parameter ID.</param>
        /// <returns>Parsing delegate.</returns>
        private static ParseArgumentDelegate CreateArgumentParser(CommandlineParameterId id)
        {
            return delegate(CommandlineArgument commandlineArgument, string[] arguments, ref int index)
            {
                string parmeterName = Enum.GetName(typeof(CommandlineParameterId), id);
                if (ConsumeArgument(commandlineArgument, arguments[index], parmeterName))
                {
                    if (FetchValue(commandlineArgument, arguments, index))
                    {
                        index++;
                        arguments[index] = (string)commandlineArgument.Parameter;

                        Trc.Log(LogLevel.Debug, "{0} received", parmeterName);
                        index++;
                        return true;
                    }
                }

                return false;
            };
        }

        /// <summary>
        /// Creates a delegate for switch argument.
        /// </summary>
        /// <param name="id">Command line parameter ID.</param>
        /// <returns>Parsing delegate.</returns>
        private static ParseArgumentDelegate CreateSwitchArgumentParser(CommandlineParameterId id)
        {
            return delegate(CommandlineArgument commandlineArgument, string[] arguments, ref int index)
            {
                if (ConsumeArgument(commandlineArgument, arguments[index], id.ToString()))
                {
                    commandlineArgument.Parameter = true;
                    index++;
                    return true;
                }

                return false;
            };
        }

        #endregion
    }
}
