using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Threading;
using LoggerInterface;
using Microsoft.ServiceBus.Messaging;
using RcmAgentCommonLib.ErrorHandling;
using RcmAgentCommonLib.LoggerInterface;
using RcmAgentCommonLib.Utilities;

namespace MarsAgent.LoggerInterface
{
    public class EventHubUploader<TlogContext> : LogUploaderBase
        where TlogContext : RcmAgentCommonLogContext, new()
    {
        /// <summary>
        /// The max log size.
        /// It should be less than 1mb because of EventHub message size limitation.
        /// </summary>
        private const int MaxLogSizeInBytes = 1024 * 1000;

        /// <summary>
        /// Gets or sets the log directory path.
        /// </summary>
        private string EventHubSasUri { get; set; }

        /// <summary>
        /// Gets or sets the kusto table name prefix.
        /// </summary>
        private string KustoTableName { get; set; }

        /// <summary>
        /// Gets or sets the kusto table name prefix.
        /// </summary>
        private string EventVersion { get; set; }

        /// <summary>
        /// Gets or sets the logger instance.
        /// </summary>
        protected RcmAgentCommonLogger<TlogContext> LoggerInstance { get; set; }

        private CancellationToken CancellationToken { get; set; }

        /// <summary>
        /// Initializes a new instance of the <see cref="EventHubLogUploader"/> class.
        /// </summary>
        /// <param name="eventHubSasUri">The event hub SAS URI.</param>
        /// <param name="eventProperties">The service properties object for events.</param>
        public EventHubUploader(
            string eventHubSasUri,
            EventProperties eventProperties,
            RcmAgentCommonLogger<TlogContext> logger,
            CancellationToken ct)
        {
            this.EventHubSasUri = eventHubSasUri;
            this.KustoTableName = eventProperties.KustoTableName;
            this.EventVersion = eventProperties.EventVersion;
            this.CancellationToken = ct;
        }

        /// <summary>
        /// Method to upload log to EventHub.
        /// </summary>
        /// <param name="filePath">The file to be processed.</param>
        /// <param name="isCompressionEnabled">Value indicating whether compression is enabled
        /// for uploaded log folders or not.</param>
        /// <returns>True if log is successfully processed, false otherwise.</returns>
        public sealed override bool ProcessLog(string filePath, bool isCompressionEnabled = false)
        {
            try
            {
                this.CancellationToken.ThrowIfCancellationRequested();

                string logFileContent = File.ReadAllText(filePath);

                if (logFileContent.Length > MaxLogSizeInBytes)
                {
                    this.CancellationToken.ThrowIfCancellationRequested();
                    // Split to smaller size, when log data size is more than 1 mb
                    List<string> dataChunks =
                        LogUploadHelper.SplitLogData(logFileContent, MaxLogSizeInBytes);

                    foreach (string logData in dataChunks)
                    {
                        this.CancellationToken.ThrowIfCancellationRequested();
                        this.SendEventHubMessage(logData);
                    }
                }
                else
                {
                    this.SendEventHubMessage(logFileContent);
                }

                // Log file processed, move to uploaded directory.
                LogUploadHelper.MoveFileToUploadedDirectory(filePath, isCompressionEnabled);

                return true;
            }
            catch (OperationCanceledException) when (this.CancellationToken.IsCancellationRequested)
            {
                // Rethrow operation canceled exception if the service is stopped.
                Logger.Instance.LogVerbose(
                    CallInfo.Site(),
                    "Task cancellation exception is throwed");
                throw;
            }
            catch (ServerBusyException e)
                when (ExceptionExtensions<TlogContext>.IsEventHubThrottled(e))
            {
                // Log the error occured in processing of the logfile.
                //this.LogEventHubUploadFailed(e.ToString(), filePath);

                // In case of server busy exception, we don't want to continue with
                // log upload and stress the server more. So, exit the periodic task
                // and retry the operation in the next iteration.This will mitigate
                // the EventHub throttling error.
                throw;
            }
            catch (Exception e)
            {
                throw e;
            }
        }

        /// <summary>
        /// To indicate any flush semantics for data that is being collected.
        /// </summary>
        public sealed override void Flush()
        {
        }

        /// <summary>
        /// Sends message to EventHub for kusto ingestion.
        /// </summary>
        /// <param name="logData">The data to be part of EventHub message.</param>
        private void SendEventHubMessage(string logData)
        {
            this.CancellationToken.ThrowIfCancellationRequested();

            EventData eventData = new EventData(Encoding.UTF8.GetBytes(logData));

            string tableName = $"{this.KustoTableName}";

            // Dynamic routing properties for the database table and mapping.
            eventData.Properties.Add("Table", tableName);

            string ingestionMappingReference =
                $"{tableName}IngestionMapping{this.EventVersion}";
            eventData.Properties.Add("IngestionMappingReference", ingestionMappingReference);

            eventData.Properties.Add("Format", "csv");

            this.CancellationToken.ThrowIfCancellationRequested();
            var sender = EventHubSender.CreateFromConnectionString(this.EventHubSasUri);
            this.CancellationToken.ThrowIfCancellationRequested();
            sender.Send(eventData);
        }
    }
}
