using Newtonsoft.Json;
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;

namespace ASRSetupFramework
{
    /// <summary>
    /// Logs the errors in output json file
    /// </summary>
    public static class ErrorLogger
    {
        private static readonly object lockobject = new object();

        private static JsonSerializer serializer;

        public static string OutputJsonFileName { get; private set; }

        /// <summary>
        /// Sets the output json file name.
        /// </summary>
        /// <param name="jsonFileName">Path of the json file</param>
        /// <param name="overwrite">If true, deletes the old json file</param>
        /// <returns></returns>
        public static bool InitializeInstallerOutputJson(string jsonFileName, bool overwrite = false)
        {
            Trc.Log(LogLevel.Debug, "Initialising output json file");
            lock (lockobject)
            {
                try
                {
                    OutputJsonFileName = GetInstallerErrorJsonFileLocation(jsonFileName);
                    Trc.Log(LogLevel.Debug, "Output error json filename: " + OutputJsonFileName);
                    FileInfo fileInfo = new FileInfo(OutputJsonFileName);
                    if (!Directory.Exists(fileInfo.DirectoryName))
                    {
                        Directory.CreateDirectory(fileInfo.DirectoryName);
                    }

                    if (overwrite && File.Exists(OutputJsonFileName))
                    {
                        File.Delete(OutputJsonFileName);
                    }
                    return true;
                }
                catch (Exception ex)
                {
                    Trc.Log(LogLevel.Error, string.Format("Failed to initialize file {0} due to following exception {1}", OutputJsonFileName, ex));
                }
            }
            return false;
        }

        /// <summary>
        /// Gets the output json file location
        /// </summary>
        /// <param name="jsonFileName">json file location, can be null/empty</param>
        /// <returns></returns>
        private static string GetInstallerErrorJsonFileLocation(string jsonFileName)
        {
            string outputJsonPath = string.Empty;
            if (!jsonFileName.IsNullOrWhiteSpace())
            {
                outputJsonPath = jsonFileName;
                Trc.Log(LogLevel.Debug, string.Format("Output json file name {0} specified as command line parameters.",outputJsonPath));
            }
            else
            {
                outputJsonPath = Path.Combine(
                    Path.Combine(
                        Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData),
                        UnifiedSetupConstants.LogFolder),
                    UnifiedSetupConstants.MobilityServiceValidatorDefaultOutputJsonFile);
                Trc.Log(LogLevel.Debug, string.Format("Output json file name {0} taken as default filename", outputJsonPath));
                PropertyBagDictionary.Instance.SafeAdd(PropertyBagConstants.ValidationsOutputJsonFilePath, outputJsonPath);
            }

            return outputJsonPath;
        }

        /// <summary>
        /// Initializes the serialization settings for Newtonsoft.json serializer
        /// </summary>
        private static void InitializeJsonSettings()
        {
            JsonSerializerSettings SerializerSettings = new JsonSerializerSettings()
            {
                Formatting = Formatting.Indented,
                NullValueHandling = NullValueHandling.Ignore
            };

            serializer = JsonSerializer.Create(SerializerSettings);
        }

        /// <summary>
        /// Reads all the installer errors from the output json file
        /// </summary>
        /// <returns>Extension error object</returns>
        private static ExtensionErrors GetInstallerErrorsFromJsonFile()
        {
            InitializeJsonSettings();

            if (!File.Exists(OutputJsonFileName))
            {
                Trc.Log(LogLevel.Debug, "The output json file " + OutputJsonFileName + " does not exist");
                return new ExtensionErrors()
                {
                    Errors = new List<InstallerException>()
                };
            }

            StreamReader sr = new StreamReader(OutputJsonFileName);
            JsonTextReader jsonReader = new JsonTextReader(sr);
            var extensionErrors = serializer.Deserialize<ExtensionErrors>(jsonReader);
            jsonReader.Close();
            return extensionErrors;

        }

        /// <summary>
        /// Logs single Extension error in output json file
        /// </summary>
        /// <param name="extError">Extension errors to be logged.</param>
        public static void LogInstallerErrorList(ExtensionErrors extError)
        {
            lock (lockobject)
            {

                ExtensionErrors extensionErrors = GetInstallerErrorsFromJsonFile();
                extensionErrors.Errors = extensionErrors.Errors.Concat(extError.Errors).ToList();

                string errorString = string.Empty;
                foreach (var error in extError.Errors)
                    errorString = errorString + error.ToString();
                Trc.Log(LogLevel.Always, "Writing installer errors list to output json file");
                Trc.Log(LogLevel.Always, errorString);
                using (StreamWriter sw = new StreamWriter(OutputJsonFileName, false))
                {
                    using (JsonWriter writer = new JsonTextWriter(sw))
                    {
                        serializer.Serialize(writer, extensionErrors);
                    }
                }
            }
        }

        /// <summary>
        /// Logs single Installer Exception in output json file.
        /// </summary>
        /// <param name="ie">The exception to be logged</param>
        public static void LogInstallerError(InstallerException ie)
        {
            lock (lockobject)
            {
                ExtensionErrors extensionErrors = GetInstallerErrorsFromJsonFile();
                extensionErrors.Errors.Add(ie);

                Trc.Log(LogLevel.Always, "Writing installer errors to output json file");
                Trc.Log(LogLevel.Always, ie.ToString());
                using (StreamWriter sw = new StreamWriter(OutputJsonFileName, false))
                {
                    using (JsonWriter writer = new JsonTextWriter(sw))
                    {
                        serializer.Serialize(writer, extensionErrors);
                    }
                }
            }
        }

        public static void ConsolidateErrors()
        {
            lock (lockobject)
            {
                ExtensionErrors extensionErrors = GetInstallerErrorsFromJsonFile();
                if (extensionErrors == null || extensionErrors.Errors.Count <= 0)
                    return;

                extensionErrors.Errors = extensionErrors.Errors.
                    GroupBy(err => err.error_name).
                    Select(g => g.First()).
                    ToList();

                Trc.Log(LogLevel.Always, "Consolidating errors to output json file");
                using (StreamWriter sw = new StreamWriter(OutputJsonFileName, false))
                {
                    using (JsonWriter writer = new JsonTextWriter(sw))
                    {
                        serializer.Serialize(writer, extensionErrors);
                    }
                }
            }
        }
    }
}
