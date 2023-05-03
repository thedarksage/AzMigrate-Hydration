using System;
using System.Diagnostics;
using Logger;

namespace DMSUploader
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
        /// Operation Name.
        /// </summary>
        Operation,

        /// <summary>
        /// DMS required parameters file.
        /// </summary>
        DMSRequiredParametersFile,

        /// <summary>
        /// Download name.
        /// </summary>
        DownloadName,

        /// <summary>
        /// ReleaseId.
        /// </summary>
        ReleaseId,

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
            new CommandlineArgument(CommandlineParameterId.Operation, new ParseArgumentDelegate(ParseOperationArgument), false),
            new CommandlineArgument(CommandlineParameterId.DMSRequiredParametersFile, new ParseArgumentDelegate(ParseDMSRequiredParametersFileArgument), false),
            new CommandlineArgument(CommandlineParameterId.DownloadName, new ParseArgumentDelegate(ParseDownloadNameArgument), null),
            new CommandlineArgument(CommandlineParameterId.ReleaseId, new ParseArgumentDelegate(ParseReleaseIdArgument), null)
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
        /// Show help.
        /// </summary>
        public static string ShowHelp()
        {
            string messageForHelp =
                "USAGE:" + Environment.NewLine + Environment.NewLine +
                "DMSUploader.exe /Operation CreateDownload /DMSRequiredParametersFile <Absolute required parameters file path>" + Environment.NewLine +
                "DMSUploader.exe /Operation DeleteDownload /DownloadName <Downloadname to delete> /ReleaseId <ReleaseId>";

            return messageForHelp;
        }

        /// <summary>
        /// Parses the command line. 
        /// In case of errors, shows the Error dialog and then rethrows the exception.
        /// </summary>
        /// <param name="arguments">Commandling arguments.</param>
        public bool ParseCommandline(string[] arguments)
        {
            int index = 1, iterator = 0;
            bool result = true;

            string usage = ShowHelp();

            try
            {
                // Parse command line arguments
                while (index < arguments.Length)
                {
                    Trc.Log(
                        LogLevel.Always,
                        string.Format(
                            "Main : Parse argument {0}",
                            arguments[index]));

                    if (IsArgument(arguments[index]))
                    {
                        for (iterator = 0; iterator < this.commandlineParameterTable.Length; iterator++)
                        {
                            if (this.commandlineParameterTable[iterator].ParseArgumentDelegate(this.commandlineParameterTable[iterator], arguments, ref index))
                            {
                                Trc.Log(
                                    LogLevel.Always,
                                    "Main : The argument was parsed, move to next one.");
                                break;
                            }
                        }

                        if (iterator >= this.commandlineParameterTable.Length)
                        {
                            Trc.Log(
                                LogLevel.ErrorToUser,
                                string.Format(
                                    "Invalid command line parameters passed {0} {1} {2}",
                                    arguments[index],
                                    Environment.NewLine,
                                    usage));
                            return false;
                        }
                    }
                    else
                    {
                        Trc.Log(
                            LogLevel.ErrorToUser,
                            string.Format(
                                "Invalid command line parameters passed {0} {1} {2}",
                                arguments[index],
                                Environment.NewLine,
                                usage));
                        return false;
                    }
                }

                return result;
            }
            catch (Exception Exception)
            {
                Trc.Log(
                    LogLevel.ErrorToUser,
                    string.Format(
                        "pawnSetup : {0}",
                        Exception.ToString()));
                string message = string.Format("{0}\n\n{1}", Exception.Message, usage);
                Trc.Log(
                    LogLevel.ErrorToUser,
                    string.Format(
                        "Quite mode Error: {0}",
                        message));
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
                        Trc.Log(
                            LogLevel.ErrorToUser,
                            string.Format(
                                "Command line parameter {0} was already specified",
                                argument));
                        return false;
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
                    Trc.Log(
                        LogLevel.ErrorToUser,
                        string.Format(
                            "Command line parameter missing - {0}",
                            arguments[index]));
                    return false;
                }

                index++;
                commandlineArgument.Parameter = arguments[index];

                return true;
            }
            else
            {
                Trc.Log(
                    LogLevel.ErrorToUser,
                    string.Format(
                        "Commandline parameter value not specified for {0}",
                        arguments[index]));
                return false;
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
        /// If we find the Operation argument, then set its value in the commandline argument
        /// </summary>
        /// <param name="commandlineArgument">One complete commandline argument</param>
        /// <param name="arguments">All arguments user passed</param>
        /// <param name="index">the index of current argument</param>
        /// <returns>returns whether we found the Operation argument</returns>
        private static bool ParseOperationArgument(CommandlineArgument commandlineArgument, string[] arguments, ref int index)
        {
            if (ConsumeArgument(commandlineArgument, arguments[index], "Operation"))
            {
                if (FetchValue(commandlineArgument, arguments, index))
                {
                    index++;
                    arguments[index] = (string)commandlineArgument.Parameter;
                    index++;
                    return true;
                }
            }

            return false;
        }

        /// <summary>
        /// If we find the DMSRequiredParametersFile argument, then set its value in the commandline argument
        /// </summary>
        /// <param name="commandlineArgument">One complete commandline argument</param>
        /// <param name="arguments">All arguments user passed</param>
        /// <param name="index">the index of current argument</param>
        /// <returns>returns whether we found the DMSRequiredParametersFile argument</returns>
        private static bool ParseDMSRequiredParametersFileArgument(CommandlineArgument commandlineArgument, string[] arguments, ref int index)
        {
            if (ConsumeArgument(commandlineArgument, arguments[index], "DMSRequiredParametersFile"))
            {
                if (FetchValue(commandlineArgument, arguments, index))
                {
                    index++;
                    arguments[index] = (string)commandlineArgument.Parameter;
                    index++;
                    return true;
                }
            }

            return false;
        }

        /// <summary>
        /// If we find the DownloadName argument, then set its value in the commandline argument
        /// </summary>
        /// <param name="commandlineArgument">One complete commandline argument</param>
        /// <param name="arguments">All arguments user passed</param>
        /// <param name="index">the index of current argument</param>
        /// <returns>returns whether we found the DownloadName argument</returns>   
        private static bool ParseDownloadNameArgument(CommandlineArgument commandlineArgument, string[] arguments, ref int index)
        {
            if (ConsumeArgument(commandlineArgument, arguments[index], "DownloadName"))
            {
                if (FetchValue(commandlineArgument, arguments, index))
                {
                    index++;
                    arguments[index] = (string)commandlineArgument.Parameter;
                    index++;
                    return true;
                }
            }

            return false;
        }

        /// <summary>
        /// If we find the ReleaseId argument, then set its value in the commandline argument
        /// </summary>
        /// <param name="commandlineArgument">One complete commandline argument</param>
        /// <param name="arguments">All arguments user passed</param>
        /// <param name="index">the index of current argument</param>
        /// <returns>returns whether we found the ReleaseId argument</returns>   
        private static bool ParseReleaseIdArgument(CommandlineArgument commandlineArgument, string[] arguments, ref int index)
        {
            if (ConsumeArgument(commandlineArgument, arguments[index], "ReleaseId"))
            {
                if (FetchValue(commandlineArgument, arguments, index))
                {
                    index++;
                    arguments[index] = (string)commandlineArgument.Parameter;
                    index++;
                    return true;
                }
            }

            return false;
        }

        #endregion
    }
}
