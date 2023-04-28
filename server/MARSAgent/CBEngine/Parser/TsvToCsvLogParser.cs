using LoggerInterface;
using MarsAgent.CBEngine.Constants;
using MarsAgent.CBEngine.Model;
using MarsAgent.CBEngine.Utilities;
using MarsAgent.LoggerInterface;
using MarsAgent.Utilities;
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text.RegularExpressions;
using System.Threading;

namespace MarsAgent.CBEngine.Parser
{
    public class TsvToCsvLogParser
    {
        public TsvToCsvLogParser() { }

        /// <summary>
        /// The delimiter to be used in the CSV log file.
        /// </summary>
        private string csvLogFileDelimiter = ",";

        private char cbengineLogFileDelimiter = '\t';

        private string inputdatetimeFormat = "MM/dd HH:mm:ss.fff";

        private string outputdatetimeFormat = "yyyy-MM-dd HH:mm:ss.fff";

        public string FileParser(string filepath, string biosId, string cbEngineVer, CancellationToken cts)
        {
            if (IsFileAlreadyParsed(filepath))
            {
                return filepath;
            }

            string outfilepath = null;
            try
            {
                object lockObject = new object();

                lock (lockObject)
                {
                    if (!string.IsNullOrEmpty(filepath) && File.Exists(filepath) && !cts.IsCancellationRequested)
                    {
                        string filename = Path.GetFileName(filepath);
                        string dirpath = Path.GetDirectoryName(filepath);
                        string outfilename = string.Concat(
                            filename.Split('.')[0],
                            CBEngineConstants.LogFileNameSeperator,
                            Constants.CBEngineConstants.ParsedLogFileSuffix,
                            Constants.CBEngineConstants.LogFileExtension);
                        outfilepath = Path.Combine(dirpath, outfilename);

                        List<string> outputlogs = new List<string>();

                        using (var f_lock = new FileLock(filepath))
                        {
                            using (StreamReader reader = new StreamReader(filepath))
                            {
                                string headerLine = reader.ReadLine();
                                string line;
                                while ((line = reader.ReadLine()) != null && !cts.IsCancellationRequested)
                                {
                                    string log = ParseLog(line, biosId, cbEngineVer);
                                    if (!string.IsNullOrEmpty(log))
                                    {
                                        outputlogs.Add(log);
                                    }
                                }
                            }
                        }
                        File.WriteAllLines(filepath, outputlogs);
                        File.Move(filepath, outfilepath);

                        return outfilepath;
                    }
                }
            }
            catch (OperationCanceledException) when (cts.IsCancellationRequested)
            {
                // Rethrow operation canceled exception if the service is stopped.
                Logger.Instance.LogVerbose(
                    CallInfo.Site(),
                    "Task cancellation exception is throwed");
                throw;
            }
            catch (Exception e)
            {
                Logger.Instance.LogError(
                    CallInfo.Site(),
                    $"FileParser failed to parse cbengine : '{filepath}'.\n");

                Logger.Instance.LogException(CallInfo.Site(), e);

                throw;
            }
            return outfilepath;
        }

        private bool IsFileAlreadyParsed(string srcFilePath)
        {
            try
            {
                Regex rgx = new Regex(@"^CBEngine([0-9]|[1]\d+)_\d+_parsed\.log");

                string fileName = Path.GetFileName(srcFilePath);

                if (rgx != null && rgx.IsMatch(fileName))
                {
                    return true;
                }
            }
            catch(Exception e)
            {
                Logger.Instance.LogError(
                    CallInfo.Site(),
                    $"FileParser failed to validate if file already parsed: '{srcFilePath}'.\n");

                Logger.Instance.LogException(CallInfo.Site(), e);

                throw;
            }
            return false;
        }

        private string ParseLog(string logString, string biosId, string cbEngineVer)
        {
            string output = null;
            try
            {
                LogContext logContext = Logger.Instance.GetLogContext();
                string[] logValues = logString.Split(cbengineLogFileDelimiter);

                LogEntity logEntity = new LogEntity();

                logEntity.PreciseTimeStamp = DateTime.Now.ToString(outputdatetimeFormat);

                logEntity.ProcessId = logValues.GetValue(0).ToString();
                logEntity.ThreadId = logValues.GetValue(1).ToString();
                logEntity.LogTime = FormatDateTime(logValues[2], logValues[3]);
                logEntity.ComponentCode = logValues.GetValue(4).ToString();
                logEntity.FileNameLineNumber = logValues.GetValue(5).ToString();
                logEntity.This = logValues.GetValue(6).ToString();
                logEntity.TaskId = logValues.GetValue(7).ToString();
                logEntity.VmId = logValues.GetValue(8).ToString();

                logEntity.AgentMachineId = logValues.GetValue(8).ToString();
                logEntity.Message = GetFormattedMessage(logValues.GetValue(10).ToString());
                logEntity.CallerInfo = logValues.GetValue(5).ToString();

                logEntity.ActivityId = logContext.ActivityId.ToString();
                logEntity.ApplianceId = logContext.ApplianceId;
                logEntity.ServiceVersion = logContext.ServiceVersion;
                logEntity.DiskId = logContext.DiskId;
                logEntity.ServiceEventName = LoggerEnum.ErrorSeverityMapping[
                    logValues.GetValue(9).ToString()].ToString();
                logEntity.SRSActivityId = logContext.SrsActivityId;
                logEntity.ClientRequestId = logContext.ClientRequestId;
                logEntity.ComponentName = logContext.ComponentName;
                logEntity.ContainerId = logContext.ContainerId;
                logEntity.SubscriptionId = logContext.SubscriptionId;
                logEntity.ResourceId = logContext.ResourceId;
                logEntity.ResourceLocation = logContext.ResourceLocation;
                logEntity.StampName = logContext.RcmStampName;
                logEntity.MarsVersion = cbEngineVer;
                logEntity.BiosId = biosId;

                output = logEntity.ToCsv(csvLogFileDelimiter);
            }
            catch (Exception e)
            {
                Logger.Instance.LogError(
                    CallInfo.Site(),
                    $"CBEngine FileParsing failed for log  : '{logString}'.\n");

                Logger.Instance.LogException(CallInfo.Site(), e);
            }

            return output;
        }

        /// <summary>
        /// Added quotes around message to consider it as a whole single string.
        /// Formatting message to exclude quotes inside message. 
        /// Example -
        /// Message - FMBlock: No retry policy specified, Params: {ErrorCode = "ConfigurationError"}
        /// Formatted Message - "FMBlock: No retry policy specified, Params: {ErrorCode = ""ConfigurationError""}"
        /// </summary>
        private string GetFormattedMessage(string message)
        {
            if (!string.IsNullOrWhiteSpace(message)) {
                string formattedMsg = message.Replace("\"", "\"\"");
                return string.Concat("\"", formattedMsg, "\"");
            }
            return string.Empty;
        }

        private string FormatDateTime(string date, string time)
        {
            string dateTimeString = date + " " + time;
            try
            {
                string result = DateTime
                                .ParseExact(dateTimeString, inputdatetimeFormat, CultureInfo.InvariantCulture)
                                .ToString(outputdatetimeFormat);
                return result;
            }
            catch (FormatException e)
            {
                Logger.Instance.LogException(CallInfo.Site(), e);
                throw e;
            }
        }
    }
}