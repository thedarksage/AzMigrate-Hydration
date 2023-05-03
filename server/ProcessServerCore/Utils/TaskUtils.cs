using Microsoft.Azure.SiteRecovery.ProcessServer.Core.Tracing;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Threading;
using System.Threading.Tasks;

namespace Microsoft.Azure.SiteRecovery.ProcessServer.Core.Utils
{
    public static class TaskUtils
    {
        /// <summary>
        /// Spawns a new Task, which executes the operation asynchronously (usually long running).
        /// </summary>
        /// <param name="traceSource">Trace source to use for logging in this helper</param>
        /// <param name="name">Name of the operation</param>
        /// <param name="ct">Cancellation token that must be associated with the created task</param>
        /// <param name="operation">Asynchronous method containing the operation to be executed</param>
        /// <returns>Spawned wrapper task</returns>
        public static Task RunTaskForAsyncOperation(
            TraceSource traceSource, string name, CancellationToken ct, Func<CancellationToken, Task> operation)
        {
            return Task.Run(async () =>
            {
                try
                {
                    traceSource?.TraceAdminLogV2Message(
                        TraceEventType.Information, "Starting {0} async operation", name);

                    if (operation != null)
                    {
                        await operation(ct).ConfigureAwait(false);
                    }

                    traceSource?.TraceAdminLogV2Message(
                        TraceEventType.Information,
                        "{0} async operation completed successfully", name);
                }
                catch (OperationCanceledException ocEx)
                {
                    traceSource?.TraceAdminLogV2Message(
                        (ct != CancellationToken.None && ocEx.CancellationToken == ct)? TraceEventType.Warning : TraceEventType.Error,
                        "{0} async operation cancelled.", name);

                    throw;
                }
                catch (Exception ex)
                {
                    // Unless logged here, this exception would end up getting
                    // cached by this task and would only show up in the log,
                    // wherever the task is waited upon (generally at the Service
                    // stop).

                    traceSource?.TraceAdminLogV2Message(
                        TraceEventType.Error,
                        "{0} async operation failed.{1}{2}",
                        name, Environment.NewLine, ex);

                    throw;
                }
            }, cancellationToken: ct);
        }

        /// <summary>
        /// Spawns a new Task, which executes the operation asynchronously (usually long running).
        /// </summary>
        /// <param name="traceSource">Trace source to use for logging in this helper</param>
        /// <param name="name">Name of the operation</param>
        /// <param name="ct">Cancellation token that must be associated with the created task</param>
        /// <param name="operation">Synchronous method containing the operation to be executed</param>
        /// <returns>Spawned wrapper task</returns>
        public static Task RunTaskForSyncOperation(
            TraceSource traceSource, string name, CancellationToken ct, Action<CancellationToken> operation)
        {
            return RunTaskForAsyncOperation(
                traceSource,
                name,
                ct,
                (token) => Task.Run(() => operation(token), token));
        }

        /// <summary>
        /// Run the action and ignore the exception
        /// </summary>
        /// <param name="action">Action to be run</param>
        /// <returns>True, if the execution didn't fault</returns>
        public static bool RunAndIgnoreException(Action action)
        {
            return RunAndIgnoreException(action, null);
        }

        /// <summary>
        /// Run the action and ignore the exception
        /// </summary>
        /// <param name="action">Action to be run</param>
        /// <param name="traceSource">TraceSource to be used to log fault (if any). Optional.</param>
        /// <returns>True, if the execution didn't fault</returns>
        public static bool RunAndIgnoreException(Action action, TraceSource traceSource)
        {
            return RunAndIgnoreException(action, traceSource, 0);
        }

        /// <summary>
        /// Run the action and ignore the exception
        /// </summary>
        /// <param name="action">Action to be run</param>
        /// <param name="traceSource">TraceSource to be used to log fault (if any). Optional.</param>
        /// <param name="id">id for logging</param>
        /// <returns>True, if the execution didn't fault</returns>
        public static bool RunAndIgnoreException(Action action, TraceSource traceSource, int id)
        {
            if (action == null)
                throw new ArgumentNullException(nameof(action));

            try
            {
                action();
                return true;
            }
            catch (Exception ex)
            {
                traceSource?.TraceAdminLogV2Message(
                    TraceEventType.Error, 
                    id,
                    "Ignoring exception, while running the action{0}{1}",
                    Environment.NewLine, ex);

                if (Debugger.IsAttached)
                    Debugger.Break();

                return false;
            }
        }
    }
}
