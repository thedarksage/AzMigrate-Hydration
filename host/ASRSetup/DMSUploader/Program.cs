using System;
using System.IO;
using Newtonsoft.Json;
using Logger;

namespace DMSUploader
{
    /// <summary>
    /// DMS Uploader Tool.
    /// </summary>
    public static class Program
    {
        /// <summary>
        /// Executable method.
        /// </summary>
        /// <param name="args">Set of command line argument.</param>
        public static void Main(string[] args)
        {
            try
            {
                string currDir =
                    new System.IO.FileInfo(System.Reflection.Assembly.GetExecutingAssembly().Location).DirectoryName;
                string currTime = DateTime.Now.ToString("MM-dd-yy-hh-mm-ss");
                string logFile =
                    Path.Combine(
                        currDir,
                        string.Concat(Constants.DMSUploaderLog, currTime, ".log"));
                string dmsInputParamFilePath = string.Empty;

                try
                {
                    FileStream stream = File.Create(logFile);
                    stream.Close();
                }
                catch (Exception)
                {
                    Trc.Log(
                        LogLevel.ErrorToUser,
                        string.Format(
                            "Unable to write to specified directory '{0}'. Check that the directory has write access.",
                            currDir));
                }

                Trc.Initialize(
                    LogLevel.Always | LogLevel.Debug | LogLevel.Error | LogLevel.Warn | LogLevel.Info | LogLevel.ErrorToUser,
                    logFile,
                    true);

                // Get command line arguments.
                String[] arguments = Environment.GetCommandLineArgs();

                // If command line arguments are more than 1, validate command line arguments.
                if (Environment.GetCommandLineArgs().Length > 1)
                {
                    // Parse command line arguments.
                    CommandlineParameters commandlineParameters = new CommandlineParameters();
                    if (commandlineParameters.ParseCommandline(arguments))
                    {
                        // Validate command line arguments passed.
                        if (!ValidateCommandLineArguments(commandlineParameters))
                        {
                            return;
                        }
                    }
                }
                else
                {
                    Trc.Log(
                        LogLevel.ErrorToUser,
                        CommandlineParameters.ShowHelp());
                    return;
                }

                if (DMSParameters.Instance().Operation == Constants.CreateDownload)
                {
                    dmsInputParamFilePath = DMSParameters.Instance().DMSRequiredParametersFile;

                    if (!File.Exists(dmsInputParamFilePath))
                    {
                       Trc.Log(
                           LogLevel.ErrorToUser,
                           string.Format(
                               "File '{0}' not exists. Please provide valid input parameters file.",
                               dmsInputParamFilePath));
                        return;
                    }

                    using (StreamReader reader = new StreamReader(dmsInputParamFilePath))
                    {
                        string jsonContent = reader.ReadToEnd();
                        if (!string.IsNullOrWhiteSpace(jsonContent))
                        {
                            MandatoryParameters inputParameters = JsonConvert.DeserializeObject<MandatoryParameters>(jsonContent);
                            if (!Uploader.ToLive(inputParameters))
                            {
                                return;
                            }
                            Trc.Log(
                                LogLevel.Always,
                                "Release created successfully.");
                        }
                    }
                }
                else if(DMSParameters.Instance().Operation == Constants.DeleteDownload)
                {
                    if (!Uploader.DeleteDownload(DMSParameters.Instance().DownloadName, DMSParameters.Instance().ReleaseId))
                    {
                        return;
                    }

                    Trc.Log(
                        LogLevel.Always,
                        "Download deleted successfully.");
                }

                Trc.LogSessionEnd(LogLevel.Info, string.Empty);
            }
            catch (Exception ex)
            {
                Trc.Log(
                    LogLevel.ErrorToUser,
                    string.Format(
                        "Download failed with Exception: {0}.",
                        ex));
            }
        }

        

        /// <summary>
        /// Validate command line arguments.
        /// </summary>
        /// <param name="commandlineParameters">commandlineParameters object</param>
        /// <returns>true if no errors</returns>
        private static bool ValidateCommandLineArguments(CommandlineParameters commandlineParameters)
        {
            bool result = true;

            // Display usage when /? argument is passed and bail out the installation.
            if ((bool)commandlineParameters.GetParameterValue(CommandlineParameterId.Help))
            {

                Trc.Log(
                    LogLevel.ErrorToUser,
                    CommandlineParameters.ShowHelp());
                return false;
            }

            // Get operation parameter value
            string operation = "";
            string dmsRequiredParametersFile = "";
            string downloadName = "";
            Guid releaseId;
            if (commandlineParameters.IsParameterSpecified(CommandlineParameterId.Operation))
            {
                operation = (string)commandlineParameters.GetParameterValue(CommandlineParameterId.Operation);
                DMSParameters.Instance().Operation = operation.ToUpper();
                Trc.Log(
                    LogLevel.Always,
                    string.Format(
                        "Operation was specified as {0}",
                        operation));
            }
            else
            {
                Trc.Log(
                    LogLevel.ErrorToUser,
                    "/Operation is a mandatory argument. Please pass the argument and retry.");
                return false;
            }

            // Validate Operation parameter value
            if ((operation.ToUpper() != Constants.CreateDownload) &&
                (operation.ToUpper() != Constants.DeleteDownload))
            {
                Trc.Log(
                    LogLevel.ErrorToUser,
                    string.Format(
                        "Invalid Operation argument value: {0} {1} {2}",
                        operation,
                        Environment.NewLine,
                        CommandlineParameters.ShowHelp()));
                return false;
            }
            else
            {
                Trc.Log(
                    LogLevel.Always,
                    string.Format(
                        "Successfully validated Operation {0} argument value.",
                        operation));
            }

            if (operation.ToUpper() == Constants.CreateDownload)
            {
                if (commandlineParameters.IsParameterSpecified(CommandlineParameterId.DMSRequiredParametersFile))
                {
                    dmsRequiredParametersFile = (string)commandlineParameters.GetParameterValue(CommandlineParameterId.DMSRequiredParametersFile);
                    DMSParameters.Instance().DMSRequiredParametersFile = dmsRequiredParametersFile;

                    if (String.IsNullOrEmpty(dmsRequiredParametersFile) || !File.Exists(dmsRequiredParametersFile))
                    {
                        Trc.Log(
                            LogLevel.ErrorToUser,
                            string.Format(
                                "File - {0} not exists. Please pass the correct file.",
                                dmsRequiredParametersFile));
                        return false;
                    }

                    Trc.Log(
                        LogLevel.Always,
                        string.Format(
                            "DMSRequiredParametersFile was specified as {0}",
                            dmsRequiredParametersFile));
                }
                else
                {
                    Trc.Log(
                        LogLevel.ErrorToUser,
                        "/DMSRequiredParametersFile is a mandatory argument.Please pass the argument and retry.");
                    return false;
                }
            }
            else if (operation.ToUpper() == Constants.DeleteDownload)
            {
                if (commandlineParameters.IsParameterSpecified(CommandlineParameterId.DownloadName))
                {
                    downloadName = (string)commandlineParameters.GetParameterValue(CommandlineParameterId.DownloadName);
                    DMSParameters.Instance().DownloadName = downloadName;

                    if (String.IsNullOrEmpty(downloadName))
                    {
                        Trc.Log(
                            LogLevel.ErrorToUser,
                            string.Format(
                                "Please provide valid download name.",
                                dmsRequiredParametersFile));
                        return false;
                    }

                    Trc.Log(
                        LogLevel.Always,
                        string.Format(
                            "Downloadname was specified as {0}",
                            downloadName));
                }
                else
                {
                    Trc.Log(
                        LogLevel.ErrorToUser,
                        "/DownloadName is a mandatory argument. Please pass the argument and retry.");
                    return false;
                }

                if (commandlineParameters.IsParameterSpecified(CommandlineParameterId.ReleaseId))
                {
                    releaseId = Guid.Parse((string)commandlineParameters.GetParameterValue(CommandlineParameterId.ReleaseId));
                    DMSParameters.Instance().ReleaseId = releaseId;

                    if (releaseId == null)
                    {
                        Trc.Log(
                            LogLevel.ErrorToUser,
                            string.Format(
                                "Please provide valid releaseId.",
                                dmsRequiredParametersFile));
                        return false;
                    }

                    Trc.Log(
                        LogLevel.Always,
                        string.Format(
                            "ReleaseId was specified as {0}",
                            releaseId));
                }
                else
                {
                    Trc.Log(
                        LogLevel.ErrorToUser,
                        "/ReleaseId is a mandatory argument. Please pass the argument and retry.");
                    return false;
                }
            }

            return result;
        }
    }
}
