using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using SystemRequirementValidator;

using ASRSetupFramework;
using Newtonsoft.Json;

namespace MobilityServiceValidator
{
    class Program
    {
        public class ArgumentNames
        {
            public const string LogFile = @"-log";
            public const string configFile = @"-config";
            public const string outputFile = @"-out";
            public const string summary = @"-summary";
            public const string action = @"-action";
        };

        /// <summary>
        /// Argument parser.
        /// Argument is supposed to be in form of
        ///     <attrib>:<value>
        /// </summary>
        /// <param name="args">command line arguments</param>
        /// <returns>Dictionary that contain values for different parameters.</returns>
        static IDictionary<string, string> ParseArguments(string[] args)
        {
            Console.WriteLine("Entering ParseArguments");

            IDictionary<string, string> argValuePair = new Dictionary<string, string>();

            if (args.Count() % 2 != 0)
            {
                Console.WriteLine(string.Format("Number of arguments is not multiple of 2. args = {0}",
                                    string.Join(",", args)));

                throw new Exception(string.Format("Number of arguments is not multiple of 2. args = {0}",
                                    string.Join(",", args)));
            }

            for (int i = 0; i < args.Count(); i = i + 2)
            {
                if (argValuePair.ContainsKey(args[i]))
                {
                    Console.WriteLine(string.Format("argument {0} exist twice. args = {1}",
                                        args[i], string.Join(",", args)));

                    throw new Exception(string.Format("argument {0} exist twice. args = {1}",
                                        args[i], string.Join(",", args)));

                }

                Console.WriteLine("Argument {0} Value {1}", args[i], args[i + 1]);

                argValuePair.Add(args[i], args[i + 1]);
            }

            Console.WriteLine("Exiting ParseArguments");
            return argValuePair;
        }

        internal static OperationDetails GetOperationDetails(
            Scenario executionScenario,
            OpName operationName,
            OpResult result,
            string errorMessage = null,
            string causes = null,
            string recommendation = null,
            string exception = null,
            int errorCode = 0,
            string errorCodeName = null)
        {
            OperationDetails operationDetails = new OperationDetails()
            {
                ScenarioName = executionScenario,
                OperationName = operationName,
                Result = result,
                Message = errorMessage,
                Causes = causes,
                Recommendation = recommendation,
                Exception = exception,
                ErrorCode = errorCode,
                ErrorCodeName = errorCodeName
            };
            return operationDetails;
        }

        /// <summary>
        /// This function parses input file and parses all checks needed.
        ///     -config <config-path> -out <output-json-complete-path> -log <logfile>
        /// </summary>
        /// <param name="args">should have config path output and logfile.</param>
        /// <returns>status</returns>
        static int Main(string[] args)
        {
            IDictionary<string, string> argValuePair;
            IList<OperationDetails> operationDetails = new List<OperationDetails>();

            JsonSerializerSettings SerializerSettings = new JsonSerializerSettings()
            {
                Formatting = Formatting.Indented,
                NullValueHandling = NullValueHandling.Ignore
            };

            try
            {
                bool areAllArgumentsPresent = true;

                if (0 == args.Length)
                {
                    Console.WriteLine("Usage: {0} {1} <logfile> {2} <configfile> {3} <outputfile> {4} <summary-file> {5} <action>", 
                                    Process.GetCurrentProcess().ProcessName,
                                    ArgumentNames.LogFile, ArgumentNames.configFile, ArgumentNames.outputFile, ArgumentNames.summary, ArgumentNames.action);
                    return (int)ExitStatus.ErrorArgumentsMissing;
                }

                argValuePair = ParseArguments(args);
                if (!argValuePair.ContainsKey(ArgumentNames.LogFile))
                {
                    Console.WriteLine("Argument {0} is missing", ArgumentNames.LogFile);
                    areAllArgumentsPresent = false;
                }

                if (!argValuePair.ContainsKey(ArgumentNames.configFile))
                {
                    Console.WriteLine("Argument {0} is missing", ArgumentNames.configFile);
                    areAllArgumentsPresent = false;
                }

                if (!argValuePair.ContainsKey(ArgumentNames.outputFile))
                {
                    Console.WriteLine("Argument {0} is missing", ArgumentNames.outputFile);
                    areAllArgumentsPresent = false;
                }

                if (!argValuePair.ContainsKey(ArgumentNames.summary))
                {
                    Console.WriteLine("Argument {0} is missing", ArgumentNames.summary);
                    areAllArgumentsPresent = false;
                }

                if(!argValuePair.ContainsKey(ArgumentNames.action))
                {
                    Console.WriteLine("Argument {0} is missing", ArgumentNames.action);
                    areAllArgumentsPresent = false;
                }

                if (!areAllArgumentsPresent)
                {
                    Console.WriteLine("All Arguments are not present");
                    return (int)ExitStatus.ErrorArgumentsMissing;
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine("Error: {0}", ex.ToString());
                return (int) ExitStatus.ErrorUnknownException;
            }

            try
            {
                if (!File.Exists(argValuePair[ArgumentNames.configFile]))
                {
                    throw new FileNotFoundException(argValuePair[ArgumentNames.configFile]);
                }
                Trc.Initialize(
                    (LogLevel.Always | LogLevel.Debug | LogLevel.Error | LogLevel.Warn | LogLevel.Info),
                    argValuePair[ArgumentNames.LogFile]);

                var reader = new StreamReader(argValuePair[ArgumentNames.configFile]);
                JsonTextReader jsonReader = new JsonTextReader(reader);

                JsonSerializer serializer = JsonSerializer.Create(SerializerSettings);
                try
                {
                    if (!ErrorLogger.InitializeInstallerOutputJson(argValuePair[ArgumentNames.outputFile]))
                    {
                        Trc.Log(LogLevel.Error, "Failed to Initialise the output json file {0}", ErrorLogger.OutputJsonFileName);
                    }
                    else
                    {
                        Trc.Log(LogLevel.Info, string.Format("Successfully initialised the output json file {0}", ErrorLogger.OutputJsonFileName));
                    }
                }
                catch (Exception ex)
                {
                    Trc.Log(LogLevel.Error, string.Format("Initialising output json file {0} failed with exception {1}",
                        argValuePair[ArgumentNames.outputFile], ex));
                    return (int)ExitStatus.ErrorUnknownException;
                }

                //List for precheck errors
                IList<InstallerException> erroInfoList = new List<InstallerException>();
                //List for precheck warning
                IList<InstallerException> warningInfoList = new List<InstallerException>();
                SourceRequirementHandler SourceRequirementHandler = new SourceRequirementHandler();

                string logFile = argValuePair[ArgumentNames.LogFile];

                var systemRequirememts = serializer.Deserialize<List<SystemRequirementCheck>>(jsonReader);

                foreach (var requirement in systemRequirememts)
                {
                    OpName operation = OpName.InvalidOperation;
                    bool isMandatory = true;
                    if (bool.TryParse(requirement.IsMandatory, out isMandatory))
                    {
                        isMandatory = bool.Parse(requirement.IsMandatory);
                        Trc.Log(LogLevel.Debug, string.Format("Check {0} is {1} mandatory", requirement.CheckName, (isMandatory) ? "" : "not"));
                    }
                    else
                    {
                        Trc.Log(LogLevel.Info, "Failed to parse if the check is mandatory. So setting the isMandatory flag to true");
                        isMandatory = true;
                    }

                    InstallAction installAction = InstallAction.Install;
                    try
                    {
                        if (requirement.InstallAction == null)
                        {
                            Trc.Log(LogLevel.Info, "Install action is null, so setting the action to Both");
                            installAction = InstallAction.All;
                        }
                        else
                        {
                            installAction = (InstallAction)Enum.Parse(typeof(InstallAction), requirement.InstallAction);
                            Trc.Log(LogLevel.Debug, "Install Action Value : {0}", installAction.ToString());
                        }
                    }
                    catch
                    {
                        Trc.Log(LogLevel.Info, "Failed to parse installaction. So setting the action to Both");
                        installAction = InstallAction.All;
                    }

                    try
                    {
                        operation = (OpName)Enum.Parse(typeof(OpName), requirement.OperationName, true);
                    }
                    catch (Exception ex)
                    {
                        Trc.Log(LogLevel.Error, "Failed to parse operation name: {0} error={1}", requirement.OperationName, ex.ToString());
                    }

                    try
                    {
                        Trc.Log(LogLevel.Info, "Validating requirement {0}", requirement.CheckName);

                        if (argValuePair[ArgumentNames.action].Equals(installAction.ToString(), StringComparison.OrdinalIgnoreCase) ||
                            installAction == InstallAction.All)
                        {
                            SourceRequirementHandler.GetHandler(requirement.CheckName)(requirement.Params, logFile);
                            if (operation != OpName.InvalidOperation)
                            {
                                operationDetails.Add(GetOperationDetails(Scenario.PreInstallation, operation, OpResult.Success));
                            }
                            Trc.Log(LogLevel.Info, "Validated {0}", requirement.CheckName);
                        }
                        else
                        {
                            Trc.Log(LogLevel.Debug, "The action passed {0} is not equal to the action {1}", argValuePair[ArgumentNames.action], installAction.ToString());
                        }
                    }
                    catch (OutOfMemoryException)
                    {
                        return (int)ExitStatus.OutofMemoryException;
                    }
                    catch (SystemRequirementNotMet e)
                    {
                        Trc.Log(LogLevel.Error, e.ToString());
                        if (isMandatory)
                        {
                            //Current precheck failed. As isMandatory flag is true, add the error to errorInfoList
                            erroInfoList.Add(e.GetErrorInfo());
                        }
                        else
                        {
                            //Current precheck failed. As isMandatory flag is false, add the error to warningInfoList
                            warningInfoList.Add(e.GetErrorInfo());
                        }

                        if (operation != OpName.InvalidOperation)
                        {
                            operationDetails.Add(GetOperationDetails(
                                        Scenario.PreInstallation,
                                        operation,
                                        OpResult.Failed,
                                        e.GetErrorInfo().default_message,
                                        null,
                                        null,
                                        null,
                                        (int)ExitStatus.ErrorSystemRequirementMismatch,
                                        requirement.CheckName));
                        }
                    }
                    catch (Exception ex)
                    {
                        if (operation != OpName.InvalidOperation)
                        {
                            operationDetails.Add(GetOperationDetails(
                                        Scenario.PreInstallation,
                                        operation,
                                        OpResult.Failed,
                                        ex.Message,
                                        null,
                                        null,
                                        ex.ToString(),
                                        (int)ExitStatus.ErrorUnknownException,
                                        requirement.CheckName));
                        }

                        IDictionary<string, string> errorParams = new Dictionary<string, string>()
                    {
                        {"message", "Generic Error Encountered"}
                    };
                        Trc.Log(LogLevel.Error, ex.ToString());
                        if (isMandatory)
                        {
                            erroInfoList.Add(new InstallerException(SystemRequirementFailure.ASRMobilityServiceGenericError.ToString(), errorParams, ex.Message));
                        }
                        else
                        {
                            warningInfoList.Add(new InstallerException(SystemRequirementFailure.ASRMobilityServiceGenericError.ToString(), errorParams, ex.Message));
                        }
                    }
                }

                if (operationDetails.Count != 0)
                {
                    using (StreamWriter sw = new StreamWriter(argValuePair[ArgumentNames.summary], false))
                    using (JsonWriter writer = new JsonTextWriter(sw))
                    {
                        serializer.Serialize(writer, operationDetails);
                    }
                }

                if (erroInfoList.Count != 0)
                {
                    Trc.Log(LogLevel.Debug, string.Format("Error count : {0}", erroInfoList.Count));
                    ExtensionErrors errors = new ExtensionErrors();
                    errors.Errors = erroInfoList;

                    Trc.Log(LogLevel.Debug, "Writing error info to Output file");
                    ErrorLogger.LogInstallerErrorList(errors);
                }

                if (warningInfoList.Count != 0)
                {
                    Trc.Log(LogLevel.Debug, string.Format("Error count : {0}", warningInfoList.Count));
                    ExtensionErrors warnings = new ExtensionErrors();
                    warnings.Errors = warningInfoList;

                    Trc.Log(LogLevel.Debug, "Writing warning info to Output file");
                    ErrorLogger.LogInstallerErrorList(warnings);
                }

                Trc.Log(LogLevel.Debug, "Exiting main");
                if (erroInfoList.Count != 0)
                {
                    return (int)ExitStatus.ErrorSystemRequirementMismatch;
                }
                else if (warningInfoList.Count != 0)
                {
                    return (int)ExitStatus.WarningSystemRequirementMismatch;
                }
                else
                    return (int)ExitStatus.Success;
            }
            catch(OutOfMemoryException)
            {
                return (int)ExitStatus.OutofMemoryException;
            }
            catch (Exception ex)
            {
                Trc.Log(LogLevel.Error, "Prechecks failed due to exception " + ex);
                return (int)ExitStatus.ErrorUnknownException;
            }
        }
    }
}
