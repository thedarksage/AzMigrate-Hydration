using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Config;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.CSApi;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Tracing;
using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils;
using Newtonsoft.Json;
using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Reflection;
using System.Threading;
using System.Threading.Tasks;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.TaskHandlers
{
    public class PSTaskManager
    {
        private static readonly JsonSerializerSettings s_jsonSerSettings =
            JsonUtils.GetStandardSerializerSettings(
                indent: false,
                converters: JsonUtils.GetAllKnownConverters());

        private static string s_jsonData;

        public PSTaskManager(string jsonData)
        {
            s_jsonData = jsonData;
        }

        public async Task RunTasks(CancellationToken ct)
        {
            var taskTriggerTime = new ConcurrentDictionary<TaskName, DateTime>();

            Stopwatch taskManagerSW = new Stopwatch();

            ProcessServerSettings psSettings = null;

            while (true)
            {
                taskManagerSW.Restart();

                var lowestTaskTimeSpan = TimeSpan.Zero;
                var taskList = new List<Task>();

                TaskUtils.RunAndIgnoreException(() =>
                {
                    ct.ThrowIfCancellationRequested();

                    psSettings = GetPSSettings();

                    var psTasks = GetPSTasks();

                    if (psTasks != null)
                    {
                        foreach (var psTask in psTasks)
                        {
                            TaskUtils.RunAndIgnoreException(() =>
                            {
                                ct.ThrowIfCancellationRequested();

                                var (objType, methodInfo) = ValidateAndGetProperties(psTask, ct);

                                var taskName = psTask.TaskName;

                                var taskInterval = TimeSpan.FromSeconds(
                                    Convert.ToDouble(psTask.TaskInterval));

                                if (!taskTriggerTime.TryGetValue(taskName, out DateTime lastTriggerdTime))
                                    lastTriggerdTime = DateTime.MinValue;

                                var currTime = DateTime.UtcNow;

                                Tracers.TaskMgr.TraceAdminLogV2Message(TraceEventType.Verbose,
                                    "TaskName : {0}, TaskInterval : {1} secs," +
                                    " CurrentTime : {2}, LastTriggeredTime : {3}",
                                    taskName, taskInterval.TotalSeconds, currTime, lastTriggerdTime);

                                // TODO : Trigger On-Demand tasks
                                if (lastTriggerdTime == DateTime.MinValue ||
                                    lastTriggerdTime > currTime ||
                                    lastTriggerdTime + taskInterval <= currTime)
                                {
                                    ct.ThrowIfCancellationRequested();

                                    taskTriggerTime.AddOrUpdate(
                                        taskName, lastTriggerdTime, (x, y) => DateTime.UtcNow);

                                    Tracers.TaskMgr.TraceAdminLogV2Message(
                                        TraceEventType.Information,
                                        "Adding {0} task in tasklist as configured interval of {1} secs elapsed",
                                        taskName, taskInterval.TotalSeconds);

                                    var taskManagerTask = TaskUtils.RunTaskForAsyncOperation(
                                            traceSource: Tracers.TaskMgr,
                                            name: $"{nameof(RunTasks)}",
                                            ct: ct,
                                            operation: async (taskCancToken) =>
                                            await RunTaskAsync(
                                                taskName: taskName,
                                                taskInterval: taskInterval,
                                                objType: objType,
                                                methodInfo: methodInfo,
                                                psSettings: psSettings,
                                                ct: taskCancToken).ConfigureAwait(false));

                                    taskList.Add(taskManagerTask);
                                }
                            }, Tracers.TaskMgr);
                        }

                        lowestTaskTimeSpan = GetLowestTaskInterval(psTasks);
                    }
                }, Tracers.TaskMgr);

                TimeSpan toWaitTimeSpan = Settings.Default.TaskMgr_MainIterWaitInterval;

                try
                {
                    taskList = taskList.Where(t => t != null).ToList();

                    // Waiting till any one of the task completes or the next closest task time elapses
                    if (taskList != null && taskList.Count > 0)
                    {
                        await Task.WhenAny(taskList).ConfigureAwait(false);

                        toWaitTimeSpan -= taskManagerSW.Elapsed;

                        if (toWaitTimeSpan > lowestTaskTimeSpan)
                        {
                            toWaitTimeSpan = lowestTaskTimeSpan;
                        }

                        if (toWaitTimeSpan < TimeSpan.Zero)
                        {
                            toWaitTimeSpan = TimeSpan.Zero;
                        }
                    }
                }
                catch (OperationCanceledException) when (ct.IsCancellationRequested)
                {
                    // Rethrow operation canceled exception if the service is stopped.
                    Tracers.TaskMgr.DebugAdminLogV2Message("Cancelling monitor health service Task");
                    throw;
                }
                catch (Exception ex)
                {
                    // Ignoring the exception, will attempt to run tasks in next interval
                    Tracers.TaskMgr.TraceAdminLogV2Message(TraceEventType.Warning,
                        "Ignoring the error, will attempt to run tasks in the next interval." +
                        " Failed to run tasks with error : {0}{1}",
                        Environment.NewLine,
                        ex);
                }

                await Task.Delay(toWaitTimeSpan, ct).ConfigureAwait(false);
            }
        }

        private static TimeSpan GetLowestTaskInterval(IEnumerable<PSTask> psTasks)
        {
            var lowestInterval = TimeSpan.Zero;

            TaskUtils.RunAndIgnoreException(() =>
            {
                var interval = psTasks?.Select(
                    psTask => Convert.ToInt32(psTask?.TaskInterval)).Min();

                lowestInterval = TimeSpan.FromSeconds(
                                    Convert.ToDouble(interval));
            });

            return lowestInterval;
        }

        private static IEnumerable<PSTask> GetPSTasks()
        {
            List<PSTask> psTasks;

            if (string.IsNullOrWhiteSpace(s_jsonData))
                throw new ArgumentNullException(nameof(s_jsonData));

            try
            {
                psTasks =
                    JsonConvert.DeserializeObject<List<PSTask>>(s_jsonData, s_jsonSerSettings);
            }
            catch(Exception ex)
            {
                Tracers.TaskMgr.TraceAdminLogV2Message(
                    TraceEventType.Error,
                    "Failed to deserialize PS Task Settings with error : {0}{1}",
                    Environment.NewLine,
                    ex);

                return null;
            }

            if (psTasks == null)
            {
                throw new ArgumentNullException(nameof(psTasks));
            }

            if (!psTasks.Any(currTask => currTask.TaskExecutionState == TaskExecutionState.ON))
            {
                Tracers.TaskMgr.TraceAdminLogV2Message(
                    TraceEventType.Information, "None of the tasks are in ON state in the PS Tasks List");

                return null;
            }

            psTasks = psTasks.Where(
                currTask => currTask.TaskExecutionState == TaskExecutionState.ON).ToList();

            return psTasks;
        }

        private static (Type, MethodInfo) ValidateAndGetProperties(
            PSTask psTask,
            CancellationToken cancellationToken)
        {
            cancellationToken.ThrowIfCancellationRequested();

            if (psTask == null)
                throw new ArgumentNullException(nameof(psTask));

            if (!psTask.IsValidInfo())
                throw new ArgumentException(
                    $"{nameof(psTask)} contains invalid properties");

            var taskAttributes = psTask.TaskAttributes;
            if (taskAttributes == null)
                throw new ArgumentNullException(nameof(taskAttributes));

            if (!taskAttributes.IsValidInfo())
                throw new ArgumentException(
                    $"{nameof(taskAttributes)} contains invalid properties");

            string methodName = taskAttributes.MethodName;
            string namespaceName = taskAttributes.NameSpaceName;
            string assemblyName = taskAttributes.AssemblyName;
            string typeName = taskAttributes.ClassName;

            Type objType = Type.GetType(namespaceName + "." + typeName + "," + assemblyName);
            if (objType == null)
                throw new Exception(String.Format("Type {0} not found", typeName));

            if (!objType.IsClass)
                throw new Exception("Invalid Class");

            MethodInfo methodInfo = objType.GetMethod(
                methodName,
                BindingFlags.Static | BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance);

            if (methodInfo == null)
                throw new Exception(String.Format("Method {0} not found", methodName));

            return (objType, methodInfo);
        }

        private static async Task RunTaskAsync(
            TaskName taskName,
            TimeSpan taskInterval,
            Type objType,
            MethodInfo methodInfo,
            ProcessServerSettings psSettings,
            CancellationToken ct)
        {
            Stopwatch runTaskSW = new Stopwatch();

            try
            {
                runTaskSW.Start();

                CancellationTokenSource taskCts = new CancellationTokenSource(
                    Settings.Default.TaskMgr_StuckTaskCancellationInterval);

                CancellationTokenSource combinedCts = null;
                CancellationToken combinedCt = taskCts.Token;
                if (ct.CanBeCanceled)
                {
                    combinedCts = CancellationTokenSource.CreateLinkedTokenSource(ct, taskCts.Token);
                    combinedCt = combinedCts.Token;
                }

                combinedCt.ThrowIfCancellationRequested();
                var instance = Activator.CreateInstance(objType);

                Tracers.TaskMgr.TraceAdminLogV2Message(
                    TraceEventType.Information, "Triggering Task {0}", taskName);

                using (combinedCts)
                {
                    try
                    {
                        combinedCt.ThrowIfCancellationRequested();

                        dynamic awaitTask = Task.CompletedTask;

                        ParameterInfo[] parameters = methodInfo.GetParameters();
                        if (parameters.Length > 0)
                        {
                            // TODO: Check if params can be passed based on the datatype and add validations for parameter types.
                            if (parameters.Length == 1 || taskName == TaskName.RegisterPS)
                            {
                                awaitTask = methodInfo.Invoke(instance, new object[] { combinedCts.Token });
                            }
                            else
                            {
                                if (psSettings != null)
                                {
                                    awaitTask = methodInfo.Invoke(instance, new object[] { psSettings, combinedCts.Token });
                                }
                            }

                            await awaitTask.ConfigureAwait(false);
                        }
                    }
                    // Task time-out exception
                    catch (OperationCanceledException) when (taskCts.IsCancellationRequested)
                    {
                        Tracers.TaskMgr.TraceAdminLogV2Message(
                            TraceEventType.Warning, "Cancelling stuck task {0} due to timeout", taskName);
                    }
                    // Re-Throw service stop exception
                    catch (OperationCanceledException) when (ct.IsCancellationRequested)
                    {
                        Tracers.TaskMgr.TraceAdminLogV2Message(
                            TraceEventType.Verbose, "Cancelling task {0} on service stop", taskName);

                        throw;
                    }
                }
            }
            catch (Exception ex)
            {
                // Ignoring the exception, failed task will be attempted
                // to spawn again when it's interval elapses.
                Tracers.TaskMgr.TraceAdminLogV2Message(
                    TraceEventType.Error,
                    "Failed to run task {0} with error {1}{2}",
                    taskName, Environment.NewLine, ex);
            }
            finally
            {
                runTaskSW.Stop();
                Tracers.TaskMgr.TraceAdminLogV2Message(
                    TraceEventType.Information,
                    "Time taken to execute {0} task is {1} ms",
                    taskName,
                    runTaskSW.Elapsed.TotalMilliseconds);
            }
        }

        private static ProcessServerSettings GetPSSettings()
        {
            ProcessServerSettings psSettings;

            try
            {
                psSettings = ProcessServerSettings.GetCachedSettings();
            }
            catch (Exception ex)
            {
                throw new Exception("Failed to get PS cache settings", ex);
            }

            if (psSettings == null)
            {
                Tracers.TaskMgr.TraceAdminLogV2Message(
                        TraceEventType.Information,
                        "Couldn't get cached PS settings");

                return psSettings;
            }

            var pairs = psSettings.Pairs;

            if (pairs == null)
            {
                Tracers.TaskMgr.TraceAdminLogV2Message(
                        TraceEventType.Information,
                        "Couldn't get pair settings from CS");
            }
            else if (!pairs.Any())
            {
                Tracers.TaskMgr.TraceAdminLogV2Message(
                        TraceEventType.Verbose,
                        "ProcessServer doesn't contain any replication pairs");
            }

            return psSettings;
        }
    }
}
