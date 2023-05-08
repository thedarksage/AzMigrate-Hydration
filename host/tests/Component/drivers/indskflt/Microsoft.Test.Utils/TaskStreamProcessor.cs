using Microsoft.Test.IOGen.Utils.Collections;
using System;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Microsoft.Test.IOGen.Utils
{
    public class TaskProcessingFaultedEventArgs : EventArgs
    {
        public Exception Exception { get; private set; }
        public Task FaultedTask { get; private set; }

        public TaskProcessingFaultedEventArgs(Exception exception, Task faultedTask)
        {
            this.Exception = exception;
            this.FaultedTask = faultedTask;
        }
    }

    /// <summary>
    /// Manages a stream of tasks and provides error handling for the stream
    /// </summary>
    public class TaskStreamProcessor
    {
        /// <summary>
        /// Number of queued tasks pending completion
        /// </summary>
        public int PendingTaskCount
        {
            get
            {
                return this._pendingTaskCount;
            }
        }

        /// <summary>
        /// Total number of queued tasks that completed processing (both successful and faulted)
        /// </summary>
        public int TotalTasksProcessed
        {
            get { return _totalTasksProcessed; }
        }

        /// <summary>
        /// Has any of the tasks that has been queued faulted
        /// </summary>
        public bool HasAnyTaskProcessingFaulted
        {
            get
            {
                return this.exceptions.Any();
            }
        }

        /// <summary>
        /// Has the processor completed.  If set, further queuing of Tasks would fail.
        /// (Set when user calls CompleteProcessing())
        /// </summary>
        public bool HasCompleted { get; private set; }

        /// <summary>
        /// Triggered everytime when a queued task faults
        /// </summary>
        public event EventHandler<TaskProcessingFaultedEventArgs> TaskProcessingFaulted;

        private ObjectPipe<Exception> exceptions = new ObjectPipe<Exception>();
        private int _pendingTaskCount;
        private int _totalTasksProcessed;

        /// <summary>
        /// Queues a task to the processor
        /// </summary>
        /// <param name="task">Task to be queued</param>
        /// <param name="ignorePreviousFaults">Don't fail queuing, even if previous tasks had faulted</param>
        public void QueueForProcessing(Task task, bool ignorePreviousFaults)
        {
            Interlocked.Increment(ref this._pendingTaskCount);

            try
            {
                if (this.HasCompleted)
                    throw new InvalidOperationException("Processor has been closed");

                if (!ignorePreviousFaults && this.HasAnyTaskProcessingFaulted)
                    throw new InvalidOperationException("One of the processed tasks faulted and so unable to queue further tasks");
            }
            catch
            {
                Interlocked.Decrement(ref this._pendingTaskCount);
                throw;
            }

            // Not awaiting on this task, as the intention is to perform the post processing on the task asynchronously
            // Assigning the task to a temp variable, in order to avoid the compiler warning to use await on this call
            var t = QueueAsync(task);
        }

        /// <summary>
        /// Waits for pending tasks, completes the processing and throws aggregate of all exceptions that occured in the queued tasks
        /// </summary>
        public void CompleteProcessing()
        {
            lock (this)
            {
                if (!this.HasCompleted)
                {
                    const int PENDING_TASK_POLL_TIMEOUT = 20;
                    while (this.PendingTaskCount != 0)
                        Thread.Sleep(PENDING_TASK_POLL_TIMEOUT);

                    this.HasCompleted = true;
                }
            }

            if (this.HasAnyTaskProcessingFaulted)
                throw new AggregateException(this.exceptions);
        }

        private async Task QueueAsync(Task task)
        {
            try
            {
                await task;
            }
            catch (Exception ex)
            {
                this.exceptions.Add(ex);
                if (this.TaskProcessingFaulted != null)
                    this.TaskProcessingFaulted(this, new TaskProcessingFaultedEventArgs(ex, task));
            }
            finally
            {
                Interlocked.Decrement(ref this._pendingTaskCount);
                // The thread could be pre empted in between these two operations and therefore poses a threat of _totalTasksProcessed being less than 1
                // But, this is a low priority counter and also not willing to take unwanted cost of obtaining lock over group updation of these counters
                Interlocked.Increment(ref this._totalTasksProcessed);
            }
        }
    }
}