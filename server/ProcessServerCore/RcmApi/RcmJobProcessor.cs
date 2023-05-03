using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Config;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Tracing;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils;
using Newtonsoft.Json;
using RcmContract;
using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Threading;
using System.Threading.Tasks;
using System.Xml;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.RcmApi
{
    public class RcmJobProcessor
    {
        /// <summary>
        /// Lock that protects the atomicity of entry and exit of jobs.
        /// </summary>
        private readonly object m_stateLock = new object();

        /// <summary>
        /// Contains new jobs received from RCM. The jobs are picked from this
        /// queue in FIFO order, processed and then moved to <see cref="m_completedJobs"/>.
        /// </summary>
        private readonly BlockingCollection<RcmJob> m_receivedJobs =
            new BlockingCollection<RcmJob>(
                new ConcurrentQueue<RcmJob>(),
                Settings.Default.RcmJobProc_QueueLen);

        /// <summary>
        /// Contains the processing completed jobs (either success or failure).
        /// The jobs are picked from this queue in FIFO order, status is updated
        /// with RCM and then moved to <see cref="m_reportedJobs"/>.
        /// </summary>
        private readonly BlockingCollection<RcmJob> m_completedJobs =
            new BlockingCollection<RcmJob>(
                new ConcurrentQueue<RcmJob>(),
                Settings.Default.RcmJobProc_QueueLen);

        /// <summary>
        /// Map of job id to job object, whose completion (either success or
        /// failure has been reported to RCM.
        /// </summary>
        private readonly Dictionary<string, RcmJob> m_reportedJobs =
            new Dictionary<string, RcmJob>();

        /// <summary>
        /// Look up map that contains all the jobs currently being handled by the
        /// <see cref="RcmJobProcessor"/> in a Source -> Disk -> Job mapping.
        /// </summary>
        private readonly Dictionary<string, Dictionary<string, RcmJob>> m_srcToJobsMap =
            new Dictionary<string, Dictionary<string, RcmJob>>();

        private readonly IProcessServerRcmApiStubs m_stubs;

        private static readonly object s_monitoredHostLock
            = new object();

        private static IEnumerable<IProcessServerHostSettings> s_monitoredHost = null;

        public RcmJobProcessor()
        {
            // TODO-SanKumar-1904: Make this lazy, so it's repeatedly attempted
            // to be initialized
            m_stubs = RcmApiFactory.GetProcessServerRcmApiStubs(PSConfiguration.Default);
        }

        public static void SetMonitoredHost(IEnumerable<IProcessServerHostSettings> monitoredHost)
        {
            lock(s_monitoredHostLock)
            {
                if (monitoredHost == null)
                {
                    s_monitoredHost = Enumerable.Empty<IProcessServerHostSettings>();
                    return;
                }
                s_monitoredHost = monitoredHost;
            }
        }

        public void ProcessIncomingJobs(string srcMachineId, List<RcmJob> latestJobsList)
        {
            // Treat null as empty list
            if (latestJobsList == null)
                latestJobsList = new List<RcmJob>();

            lock (m_stateLock)
            {
                // Add empty or get existing jobs map for the Source Machine Id
                if (!m_srcToJobsMap.TryGetValue(srcMachineId, out var jobsMapOfSource))
                    jobsMapOfSource = m_srcToJobsMap[srcMachineId] = new Dictionary<string, RcmJob>();

                // Job Ids of Source that are no longer sent from RCM
                var jobIdsNotFromRcm =
                    jobsMapOfSource.Keys.Except(latestJobsList.Select(job => job.Id)).ToList();

                // Remove all the job Ids within the RcmJobProcessor that are
                // found to be not sent by RCM for the Source
                foreach (var currJobId in jobIdsNotFromRcm)
                {
                    if (jobsMapOfSource.Remove(currJobId))
                    {
                        Tracers.RcmJobProc.TraceAdminLogV2Message(
                            TraceEventType.Information,
                            "Removing Job : {0} of Source : {1} out of tracked jobs, as it's not sent by RCM anymore",
                            currJobId, srcMachineId);
                    }

                    if (m_reportedJobs.Remove(currJobId))
                    {
                        Tracers.RcmJobProc.TraceAdminLogV2Message(
                            TraceEventType.Information,
                            "Removed Job : {0} of Source : {1}, which was reported completion to " +
                            "Rcm, as it's not sent by RCM anymore",
                            currJobId, srcMachineId);
                    }
                    else
                    {
                        Tracers.RcmJobProc.TraceAdminLogV2Message(
                            TraceEventType.Warning,
                            "Job : {0} of Source : {1} isn't yet reported completion to Rcm but " +
                            "has been removed, as it's not sent by RCM anymore",
                            currJobId, srcMachineId);

                        // If the job is not present in the reported jobs, then it
                        // would be in the received or the completed queue. It will be discarded,
                        // when it's picked up from the respective queue.
                    }
                }

                // Queue the newly added jobs from RCM
                foreach (var currJob in latestJobsList)
                {
                    if (jobsMapOfSource.ContainsKey(currJob.Id))
                    {
                        // Already known job
                        Tracers.RcmJobProc.TraceAdminLogV2Message(
                            TraceEventType.Information,
                            "Job : {0} of Source : {1} is already in the tracked jobs",
                            currJob.Id, srcMachineId);

                        continue;
                    }

                    // This is the new job received by PS. Queue it for processing.

                    if (!m_receivedJobs.TryAdd(currJob))
                    {
                        // The received jobs queue seems to be back logged, so
                        // we will attempt again for this and rest of the jobs,
                        // when they come again in the next PS settings refresh.

                        Tracers.RcmJobProc.TraceAdminLogV2Message(
                            TraceEventType.Warning,
                            "Couldn't queue the Job : {0} of Source : {1} to the Job processor due " +
                            "to filled up received jobs queue. Will be attempted again at the next " +
                            "incoming PS settings processing",
                            currJob.Id, srcMachineId);

                        break;
                    }

                    jobsMapOfSource.Add(currJob.Id, currJob);

                    Tracers.RcmJobProc.TraceAdminLogV2Message(
                        TraceEventType.Information,
                        "Successfully queued the Job : {0} of Source : {1} to the Job processor",
                        currJob.Id, srcMachineId);
                }
            }
        }

        public void RemoveAllJobs(IEnumerable<string> srcMachineIds)
        {
            if (srcMachineIds == null)
                throw new ArgumentNullException(nameof(srcMachineIds));

            lock (m_stateLock)
            {
                foreach (var currSrcMachineId in srcMachineIds)
                {
                    if (!m_srcToJobsMap.TryGetValue(currSrcMachineId, out var jobsMapOfSource))
                    {
                        Tracers.RcmJobProc.TraceAdminLogV2Message(
                            TraceEventType.Information,
                            "No tracked jobs were found to be removed for given source : {0}",
                            currSrcMachineId);

                        continue;
                    }

                    foreach (var currJobId in jobsMapOfSource.Keys)
                    {
                        if (m_reportedJobs.Remove(currJobId))
                        {
                            Tracers.RcmJobProc.TraceAdminLogV2Message(
                                TraceEventType.Information,
                                "Removed Job : {0} of Source : {1}, which was reported completion " +
                                "to Rcm, as part of removing all tracked jobs of the source machine",
                                currJobId, currSrcMachineId);
                        }
                        else
                        {
                            Tracers.RcmJobProc.TraceAdminLogV2Message(
                                TraceEventType.Warning,
                                "Job : {0} of Source : {1} isn't yet reported completion to Rcm " +
                                "but has been removed, as part of removing all tracked jobs of the " +
                                "source machine",
                                currJobId, currSrcMachineId);

                            // If the job is not present in the reported jobs, then it
                            // would be in the received or the completed queue. It will be discarded,
                            // when it's picked up from the respective queue.
                        }
                    }

                    m_srcToJobsMap.Remove(currSrcMachineId);
                }
            }
        }

        private int m_isRunning = 0;

        public Task Run(CancellationToken cancellationToken)
        {
            if (Interlocked.CompareExchange(ref m_isRunning, 1, 0) == 1)
                throw new InvalidOperationException($"{nameof(RcmJobProcessor)} is already running");

            var dispatchTask = TaskUtils.RunTaskForAsyncOperation(
                traceSource: Tracers.RcmJobProc,
                name: $"{nameof(RcmJobProcessor)} - Dispatch jobs",
                ct: cancellationToken,
                operation: DispatchJobsAsync);

            var reportTask = TaskUtils.RunTaskForAsyncOperation(
                traceSource: Tracers.RcmJobProc,
                name: $"{nameof(RcmJobProcessor)} - Report job completions",
                ct: cancellationToken,
                operation: ReportJobCompletionAsync);

            return Task.WhenAll(dispatchTask, reportTask);
        }

        private async Task DispatchJobsAsync(CancellationToken cancellationToken)
        {
            foreach (var currRecvdJob in m_receivedJobs.GetConsumingEnumerable(cancellationToken))
            {
                bool jobExecuted = false;

                if (IsJobValid(currRecvdJob))
                {
                    jobExecuted = true;

                    Tracers.RcmJobProc.TraceAdminLogV2Message(
                        TraceEventType.Information,
                        "Executing Job:{0}{1}",
                        Environment.NewLine, currRecvdJob);

                    try
                    {
                        await ExecuteJobAsync(currRecvdJob, cancellationToken).ConfigureAwait(false);

                        Tracers.RcmJobProc.TraceAdminLogV2Message(
                            TraceEventType.Information,
                            "Successfully executed the Job : {0} of Source : {1}",
                            currRecvdJob.Id, currRecvdJob.ConsumerId);
                    }
                    catch (Exception ex)
                    {
                        Tracers.RcmJobProc.TraceAdminLogV2Message(
                            TraceEventType.Error,
                            "Exception in executing the Job : {0} of Source : {1}{2}{3}",
                            currRecvdJob.Id, currRecvdJob.ConsumerId, Environment.NewLine, ex);

                        currRecvdJob.JobStatus = RcmJobStatus.Failed.ToString();
                        currRecvdJob.Errors.Add(BuildRcmJobError(
                            PSRcmJobErrorEnum.JobUnexpectedError,
                            msgParams: new Dictionary<string, string>()
                            {
                                ["JobType"] = currRecvdJob.JobType
                            },
                            activityId: currRecvdJob.Context?.ActivityId,
                            clientRequestId: currRecvdJob.Context?.ClientRequestId));
                    }
                }

                // NOTE-SanKumar-1905: Since we are attempting a blocking add
                // to the completed jobs queue, we don't acquire the lock, which
                // otherwise would block processing further incoming jobs and
                // clearing the orphan jobs. Also, locking is not needed, since
                // this is an intermediate queue and no lookup update is required.
                if (IsJobValid(currRecvdJob))
                {
                    Tracers.RcmJobProc.TraceAdminLogV2Message(
                        TraceEventType.Information,
                        "Moving the {0} Job : {1} of Source : {2} to completed jobs queue",
                        jobExecuted ? "executed" : "NOT executed", currRecvdJob.Id, currRecvdJob.ConsumerId);

                    m_completedJobs.Add(currRecvdJob, cancellationToken);
                }
                else
                {
                    // If a job is not valid anymore, it's simply forgotten (by not
                    // adding it to the next queue).

                    Tracers.RcmJobProc.TraceAdminLogV2Message(
                        TraceEventType.Warning,
                        "Not moving the {0} Job : {1} of Source : {2} to the completed jobs queue, " +
                        "as it's no longer valid",
                        jobExecuted ? "executed" : "NOT executed", currRecvdJob.Id, currRecvdJob.ConsumerId);
                }
            }
        }

        private async Task ReportJobCompletionAsync(CancellationToken cancellationToken)
        {
            foreach (var currCompJob in m_completedJobs.GetConsumingEnumerable(cancellationToken))
            {
                bool updateStatusToRcmSucceeded = false;

                while (!updateStatusToRcmSucceeded)
                {
                    if (!IsJobValid(currCompJob))
                    {
                        Tracers.RcmJobProc.TraceAdminLogV2Message(
                            TraceEventType.Warning,
                            "Not reporting the Job : {0} of Source : {1} to RCM, as it's no longer valid",
                            currCompJob.Id, currCompJob.ConsumerId);

                        break;
                    }

                    RcmOperationContext opContext = new RcmOperationContext();

                    try
                    {
                        Tracers.RcmJobProc.TraceAdminLogV2Message(
                            TraceEventType.Information,
                            opContext,
                            "Reporting the status : {0} of the Job : {1} of Source : {2} to RCM.",
                            currCompJob.JobStatus, currCompJob.Id, currCompJob.ConsumerId);

                        await m_stubs
                            .UpdateAgentJobStatusAsync(opContext, currCompJob, cancellationToken)
                            .ConfigureAwait(false);

                        Tracers.RcmJobProc.TraceAdminLogV2Message(
                            TraceEventType.Information,
                            opContext,
                            "Successfully reported the status : {0} of the Job : {1} of Source : {2} to RCM.",
                            currCompJob.JobStatus, currCompJob.Id, currCompJob.ConsumerId);

                        updateStatusToRcmSucceeded = true;
                    }
                    catch (Exception ex)
                    {
                        Tracers.RcmJobProc.TraceAdminLogV2Message(
                            TraceEventType.Error,
                            opContext,
                            "Failed to report the status : {0} of the Job : {1} of Source : {2} to RCM.{3}Will retry after {4}.{5}{6}",
                            currCompJob.JobStatus, currCompJob.Id, currCompJob.ConsumerId,
                            Environment.NewLine, Settings.Default.RcmJobProc_UpdateJobStatusRetryInterval,
                            Environment.NewLine, ex);

                        await Task.Delay(
                            Settings.Default.RcmJobProc_UpdateJobStatusRetryInterval,
                            cancellationToken)
                            .ConfigureAwait(false);
                    }
                }

                lock (m_stateLock)
                {
                    if (IsJobValid(currCompJob))
                    {
                        Tracers.RcmJobProc.TraceAdminLogV2Message(
                            TraceEventType.Information,
                            "Moving the the status {0} Job : {1} of Source : {2} to the reported jobs queue",
                            updateStatusToRcmSucceeded ? "reported" : "NOT reported", currCompJob.Id, currCompJob.ConsumerId);

                        m_reportedJobs.Add(currCompJob.Id, currCompJob);
                    }
                    else
                    {
                        // If a job is not valid anymore, it's simply forgotten (by not
                        // adding it to the next queue).

                        Tracers.RcmJobProc.TraceAdminLogV2Message(
                            TraceEventType.Warning,
                            "Not moving the status {0} Job : {1} of Source : {2} to the reported jobs queue, " +
                            "as it's no longer valid",
                            updateStatusToRcmSucceeded ? "reported" : "NOT reported", currCompJob.Id, currCompJob.ConsumerId);
                    }
                }
            }
        }

        private bool IsJobValid(RcmJob job)
        {
            // TODO-SanKumar-1905: Change this lock to reader writer slim lock
            // for better performance. Beware of recursive locking in such case,
            // which is OK with lock().
            lock (m_stateLock)
            {
                return
                    m_srcToJobsMap.TryGetValue(job.ConsumerId, out var jobsMapOfSource) &&
                    jobsMapOfSource.ContainsKey(job.Id);
            }
        }

        private static readonly Lazy<XmlDocument> s_rcmJobErrorsXml = new Lazy<XmlDocument>(() =>
        {
            XmlDocument toRet = new XmlDocument();
            Assembly currAssembly = Assembly.GetAssembly(typeof(RcmJobProcessor));
            var resourceName = "Microsoft.Azure.SiteRecovery.ProcessServer.Core.RcmApi.RcmJobErrors.xml";
            Stream xmlStream = currAssembly.GetManifestResourceStream(resourceName);
            toRet.Load(xmlStream);
            return toRet;
        });

        private static RcmJobError BuildRcmJobError(PSRcmJobErrorEnum errorCode, string message = null, Dictionary<string, string> msgParams = null, string activityId = null, string clientRequestId = null)
        {
            try
            {
                if (activityId != null || clientRequestId != null)
                {
                    msgParams = msgParams ?? new Dictionary<string, string>();
                    msgParams.Add("ActivityId", activityId);
                    msgParams.Add("ClientRequestId", clientRequestId);
                }

                XmlNamespaceManager nsManager = new XmlNamespaceManager(s_rcmJobErrorsXml.Value.NameTable);
                nsManager.AddNamespace("srs", "http://schemas.microsoft.com/2012/DisasterRecovery/SRSErrors.xsd");

                XmlNode node = s_rcmJobErrorsXml.Value.SelectSingleNode($"/srs:SRSErrors/srs:Error[srs:EnumName/text()='{errorCode}']", nsManager);

                if (node != null)
                {
                    return new RcmJobError(
                        errorCode: errorCode.ToString(),
                        // If a specific message is given, override the stock message
                        message: message ?? node?["Message"]?.InnerText,
                        possibleCauses: node?["PossibleCauses"]?.InnerText,
                        recommendedAction: node?["RecommendedAction"]?.InnerText,
                        severity: node?["Severity"]?.InnerText,
                        messageParameters: msgParams,
                        componentError: null);
                }
                else
                {
                    Tracers.RcmJobProc.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Couldn't find the mapping for Error code : {0}",
                        errorCode);
                }
            }
            catch (Exception ex)
            {
                Tracers.RcmJobProc.TraceAdminLogV2Message(
                    TraceEventType.Error,
                    "Failed to build RcmJobError for Error code : {0}{1}{2}",
                    errorCode, Environment.NewLine, ex);
            }

            return new RcmJobError(
                errorCode: errorCode.ToString(),
                message: message,
                possibleCauses: null,
                recommendedAction: null,
                severity: Severity.Error.ToString(),
                messageParameters: msgParams,
                componentError: null);
        }

        private static bool ParseInput<T>(RcmJob currRecvdJob, out T input)
        {
            try
            {
                input = JsonConvert.DeserializeObject<T>(currRecvdJob.InputPayload);
                return true;
            }
            catch (Exception ex)
            {
                Tracers.RcmJobProc.TraceAdminLogV2Message(
                    TraceEventType.Error,
                    "Failed to deserialize the input for Job : {0} of type : {1} for Source : {2}{3}{4}{5}{6}",
                    currRecvdJob.Id, currRecvdJob.JobType, currRecvdJob.ConsumerId,
                    Environment.NewLine, currRecvdJob.InputPayload, Environment.NewLine, ex);

                currRecvdJob.JobStatus = RcmJobStatus.Failed.ToString();
                currRecvdJob.Errors.Add(BuildRcmJobError(
                    PSRcmJobErrorEnum.JobInputParseFailure,
                    msgParams: new Dictionary<string, string>()
                    {
                        ["JobType"] = currRecvdJob.JobType,
                        ["Version"] = PSInstallationInfo.Default.GetPSCurrentVersion(),
                        ["Exception"] = ex.ToString()
                    },
                    activityId: currRecvdJob.Context?.ActivityId,
                    clientRequestId: currRecvdJob.Context?.ClientRequestId));

                input = default(T);
                return false;
            }
        }

        private static bool EnsureFolderPathUnderReplicationLogFolderPath(string folderPath, RcmJob currRecvdJob)
        {
            var replicationLogFolderPath = PSInstallationInfo.Default.GetReplicationLogFolderPath();

            try
            {
                FSUtils.EnsureItemUnderParent(
                    itemPath: folderPath,
                    isFile: false,
                    expectedParentFolder: replicationLogFolderPath);

                return true;
            }
            catch (Exception ex)
            {
                Tracers.RcmJobProc.TraceAdminLogV2Message(
                    TraceEventType.Error,
                    "Processed folder : {0} in Job : {1} of type : {2} for Source : {3} is not under the ReplicationLogFolderPath : {4}{5}{6}",
                    folderPath, currRecvdJob.Id, currRecvdJob.JobType, currRecvdJob.ConsumerId,
                    replicationLogFolderPath, Environment.NewLine, ex);

                currRecvdJob.JobStatus = RcmJobStatus.Failed.ToString();
                currRecvdJob.Errors.Add(BuildRcmJobError(
                    PSRcmJobErrorEnum.JobFolderNotUnderReplicationLogFolderPath,
                    msgParams: new Dictionary<string, string>()
                    {
                        ["FolderPath"] = folderPath,
                        ["JobType"] = currRecvdJob.JobType,
                        ["ReplicationLogFolderPath"] = replicationLogFolderPath,
                        ["Exception"] = ex.ToString()
                    },
                    activityId: currRecvdJob.Context?.ActivityId,
                    clientRequestId: currRecvdJob.Context?.ClientRequestId));

                return false;
            }
        }

        private async Task ExecuteJobAsync(RcmJob currRecvdJob, CancellationToken cancellationToken)
        {
            if (currRecvdJob == null)
                throw new ArgumentNullException(nameof(currRecvdJob));

            if (!Enum.TryParse<RcmJobEnum.JobType>(currRecvdJob.JobType, out var parsedJobType))
            {
                Tracers.RcmJobProc.TraceAdminLogV2Message(
                    TraceEventType.Error,
                    "Unknown job type : {0} in Job : {1} of Source : {2}",
                    currRecvdJob.JobType, currRecvdJob.Id, currRecvdJob.ConsumerId);

                currRecvdJob.JobStatus = RcmJobStatus.Failed.ToString();
                currRecvdJob.Errors.Add(BuildRcmJobError(
                    PSRcmJobErrorEnum.UnknownJob,
                    msgParams: new Dictionary<string, string>()
                    {
                        ["JobType"] = currRecvdJob.JobType,
                        ["Version"] = PSInstallationInfo.Default.GetPSCurrentVersion()
                    },
                    activityId: currRecvdJob.Context?.ActivityId,
                    clientRequestId: currRecvdJob.Context?.ClientRequestId));

                return;
            }

            switch (parsedJobType)
            {
                case RcmJobEnum.JobType.ProcessServerPrepareForSync:
                    {
                        if (!ParseInput<ProcessServerPrepareForSyncInput>(currRecvdJob, out var input))
                            return;

                        // NOTE-SanKumar-1905: Cleanup must always be performed
                        // before Create, as per the job definition of RCM.
                        bool isSuccessful =
                            await DeleteReplicationFoldersAsync(input.LogFoldersToCleanup, currRecvdJob, cancellationToken)
                            .ConfigureAwait(false);

                        if (!isSuccessful)
                            return;

                        if (input.LogFoldersToCreate != null)
                        {
                            foreach (var currFolderToCreate in input.LogFoldersToCreate)
                            {
                                if (!EnsureFolderPathUnderReplicationLogFolderPath(currFolderToCreate, currRecvdJob))
                                    return;

                                var longPath =
                                    FSUtils.GetLongPath(
                                        currFolderToCreate,
                                        isFile: false,
                                        optimalPath: Settings.Default.RcmJobProc_LongPathOptimalConversion);

                                Tracers.RcmJobProc.TraceAdminLogV2Message(
                                    TraceEventType.Information,
                                    "Creating folder : {0} using long path : {1}",
                                    currFolderToCreate, longPath);

                                try
                                {

                                    if (Directory.Exists(longPath))
                                    {
                                        Tracers.RcmJobProc.TraceAdminLogV2Message(
                                           TraceEventType.Information,
                                           "Not creating folder : {0} using long path : {1}, as it's already present",
                                           currFolderToCreate, longPath);
                                        
                                    }
                                    else
                                    {
                                        await DirectoryUtils.CreateDirectoryAsync(longPath,
                                            Settings.Default.RcmJobProc_CreateDirMaxRetryCnt,
                                            Settings.Default.RcmJobProc_CreateDirRetryInterval,
                                            cancellationToken);

                                        Tracers.RcmJobProc.TraceAdminLogV2Message(
                                            TraceEventType.Information,
                                            "Successfully created folder : {0} using long path : {1} " +
                                            "with Admin and System only access",
                                            currFolderToCreate, longPath);

                                    }
                                }
                                catch (Exception ex)
                                {
                                    Tracers.RcmJobProc.TraceAdminLogV2Message(
                                        TraceEventType.Error,
                                        "Failed to create folder : {0} using long path : {1}{2}{3}",
                                        currFolderToCreate, longPath, Environment.NewLine, ex);

                                    currRecvdJob.JobStatus = RcmJobStatus.Failed.ToString();
                                    currRecvdJob.Errors.Add(BuildRcmJobError(
                                        PSRcmJobErrorEnum.JobFolderCreationFailed,
                                        msgParams: new Dictionary<string, string>()
                                        {
                                            ["FolderPath"] = currFolderToCreate,
                                            ["JobType"] = currRecvdJob.JobType,
                                            ["Exception"] = ex.ToString()
                                        },
                                        activityId: currRecvdJob.Context?.ActivityId,
                                        clientRequestId: currRecvdJob.Context?.ClientRequestId));

                                    return;
                                }
                            }
                        }
                    }

                    break;

                case RcmJobEnum.JobType.ProcessServerPrepareForDisableProtection:
                    {
                        if (!ParseInput<ProcessServerPrepareForDisableProtectionInput>(currRecvdJob, out var input))
                            return;

                        var isSuccess =
                            await DeleteReplicationFoldersAsync(input.LogFoldersToCleanup, currRecvdJob, cancellationToken)
                            .ConfigureAwait(false);

                        if (!isSuccess)
                            return;

                        // TODO-SanKumar-2004: Handle input.TelemetryFoldersToCleanup. By moving these
                        // contents to an internal folder visible only to EvtCollForw, we could
                        // rescue the logs that are in the PS right around the time of disable protection.
                        // EvtCollForw would then delete these logs after uploading them to Kusto.
                    }
                    
                    break;

               case RcmJobEnum.JobType.ProcessServerPrepareForSwitchAppliance:
                    {
                        if (!ParseInput<ProcessServerPrepareForSwitchApplianceInput>(currRecvdJob, out var input))
                            return;

                        var isSuccess =
                            await IsMonitoringEnabledForHost(input.SourceMachineId, currRecvdJob, cancellationToken)
                            .ConfigureAwait(false);

                        if (!isSuccess)
                            return;

                    }

                    break;

                default:
                    {
                        currRecvdJob.JobStatus = RcmJobStatus.Failed.ToString();
                        currRecvdJob.Errors.Add(BuildRcmJobError(
                            PSRcmJobErrorEnum.UnimplementedJob,
                            msgParams: new Dictionary<string, string>()
                            {
                                ["JobType"] = currRecvdJob.JobType,
                                ["Version"] = PSInstallationInfo.Default.GetPSCurrentVersion()
                            },
                            activityId: currRecvdJob.Context?.ActivityId,
                            clientRequestId: currRecvdJob.Context?.ClientRequestId));

                        return;
                    }
            }

            currRecvdJob.JobStatus = RcmJobStatus.Finished.ToString();
        }

        private static async Task<bool> IsMonitoringEnabledForHost(
            string hostId,
            RcmJob currRecvdJob,
            CancellationToken cancellationToken)
        {
            TimeSpan RetryIntervalInMs = Settings.Default.
                RcmJobProc_HostMonitoringSettingsRetryInterval;

            TimeSpan RetryTimoutInRemaingInMs = Settings.Default.
                RcmJobProc_HostMonitoringSettingsRetryTimeout;

            while (RetryTimoutInRemaingInMs > TimeSpan.Zero
                && !cancellationToken.IsCancellationRequested)
            {
                IProcessServerHostSettings switchedApplianceHost = null;
                lock (s_monitoredHostLock)
                {
                    if (s_monitoredHost != null)
                    {
                        switchedApplianceHost = s_monitoredHost.FirstOrDefault(host => host.HostId == hostId);
                        if (switchedApplianceHost == null)
                            return true;
                    }
                    
                }

                await Task
                    .Delay(RetryIntervalInMs, cancellationToken)
                    .ConfigureAwait(false);
                RetryTimoutInRemaingInMs = RetryTimoutInRemaingInMs - RetryIntervalInMs;
            }

            Tracers.RcmJobProc.TraceAdminLogV2Message(
                                        TraceEventType.Error,
                                        "Host: {0} is still present in PSSettings, and hence being monitored.",
                                        hostId);

            currRecvdJob.JobStatus = RcmJobStatus.Failed.ToString();
            currRecvdJob.Errors.Add(BuildRcmJobError(
                        PSRcmJobErrorEnum.JobProcessServerPrepareForSwitchApplianceFailed,
                        msgParams: new Dictionary<string, string>()
                        {
                            ["JobType"] = currRecvdJob.JobType,
                            ["PSSettings"] = PSInstallationInfo.Default.GetPSCachedSettingsFilePath()
                        },
                        activityId: currRecvdJob.Context?.ActivityId,
                        clientRequestId: currRecvdJob.Context?.ClientRequestId));

            return false;
        }

        private static async Task<bool> DeleteReplicationFoldersAsync(
            IEnumerable<string> foldersToCleanup,
            RcmJob currRecvdJob,
            CancellationToken ct)
        {
            if (foldersToCleanup == null)
                return true;

            foreach (var currFolderToCleanup in foldersToCleanup)
            {
                if (string.IsNullOrEmpty(currFolderToCleanup))
                    continue;

                if (!EnsureFolderPathUnderReplicationLogFolderPath(currFolderToCleanup, currRecvdJob))
                    return false;

                try
                {
                    await DirectoryUtils.DeleteDirectoryAsync(
                        currFolderToCleanup,
                        recursive: true, // TODO-SanKumar-2004: Settings
                        useLongPath: true, // TODO-SanKumar-2004: Settings
                        maxRetryCount: Settings.Default.RcmJobProc_RecDelFolderMaxRetryCnt,
                        retryInterval: Settings.Default.RcmJobProc_RecDelFolderRetryInterval,
                        traceSource: Tracers.RcmJobProc,
                        ct: ct)
                        .ConfigureAwait(false);
                }
                catch (Exception ex)
                {
                    Tracers.RcmJobProc.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "Failed to delete folder : {0} using long path{1}{2}",
                        currFolderToCleanup, Environment.NewLine, ex);

                    if (ex is UnauthorizedAccessException)
                    {
                        currRecvdJob.JobStatus = RcmJobStatus.Finished.ToString();
                    }
                    else
                    {
                        currRecvdJob.JobStatus = RcmJobStatus.Failed.ToString();
                    }
                
                    currRecvdJob.Errors.Add(BuildRcmJobError(
                        PSRcmJobErrorEnum.JobFolderDeletionFailed,
                        msgParams: new Dictionary<string, string>()
                        {
                            ["FolderPath"] = currFolderToCleanup,
                            ["JobType"] = currRecvdJob.JobType
                        },
                        activityId: currRecvdJob.Context?.ActivityId,
                        clientRequestId: currRecvdJob.Context?.ClientRequestId));

                    return false;
                }
            }

            return true;
        }

        public void DeleteDisabledReplicationFolderAsync(
            IEnumerable<string> srcMachineIds)
        {
            if (srcMachineIds == null)
                throw new ArgumentNullException(nameof(srcMachineIds));

            foreach (var currSrcMachineId in srcMachineIds)
            {
                if (string.IsNullOrEmpty(currSrcMachineId))
                    continue;

                string[] currFoldersToCleanup = new[]
                {
                    Path.Combine(PSInstallationInfo.Default.GetReplicationLogFolderPath(), currSrcMachineId),
                    Path.Combine(PSInstallationInfo.Default.GetTelemetryFolderPath(), currSrcMachineId)
                };

                foreach (string folderToCleanup in currFoldersToCleanup)
                {
                    try
                    {
                        DirectoryUtils.DeleteDirectory(
                            folderToCleanup,
                            recursive: true,
                            useLongPath: true,
                            maxRetryCount: Settings.Default.RcmJobProc_DelFolderMaxRetryCnt,
                            retryInterval: Settings.Default.RcmJobProc_RecDelFolderRetryInterval,
                            traceSource: Tracers.RcmJobProc);
                    }
                    catch (Exception ex)
                    {
                        Tracers.RcmJobProc.TraceAdminLogV2Message(
                            TraceEventType.Error,
                            "Failed to delete folder : {0} using long path{1}{2}",
                            folderToCleanup, Environment.NewLine, ex);
                    }
                }
            }
        }
    }
}
