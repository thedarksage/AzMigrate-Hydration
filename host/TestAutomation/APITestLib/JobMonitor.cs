using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Diagnostics;
using InMage.APIHelpers;

namespace InMage.Test.API
{
    public enum JobStatus
    {
        Unknown,
        Completed,
        Failed,
        Timedout
    }

    public delegate JobStatus JobCompletionWaitCallback<T>(T input, out OperationStatus status);
    
    /// <summary>
    /// Monitors Asynchronous Operation
    /// </summary>
    public class JobMonitor
    {
        // Minimum timeout for a Job execution is 0 seconds.
        public const int JobTimeoutMinValue = 0;
        // Maximum timeout for a Job execution is 6 hours.
        public const int JobTimeoutMaxValue = 21600;
        // Default Sleep interval in seconds
        public static int SleepIntervalInSec = 5;

        /// <summary>
        /// Waits until the completion of a Job
        /// </summary>
        /// <typeparam name="T">Type of input for the wait callback</typeparam>
        /// <param name="waitCallback">Wait Callback method to be executed to monitor the completion of an operation</param>
        /// <param name="waitCallbackInput">Input parameter of wait callback</param>
        /// <param name="waitTimeInSec">Wait time for the completion of a Job</param>
        /// <returns>Job Status</returns>
        public static JobStatus WaitForJobCompletion<T>(JobCompletionWaitCallback<T> waitCallback, T waitCallbackInput, int waitTimeInSec)
        {
            JobStatus jobStatus = JobStatus.Unknown;
            bool hasTimedOut = false;
            if (waitTimeInSec > JobTimeoutMaxValue)
            {
                waitTimeInSec = JobTimeoutMaxValue;
            }
            int sleepIntervalInMilliSec = SleepIntervalInSec * 1000;
            int executionWaitTimeInSec = 0;
            while (true)
            {
                OperationStatus operationStatus;
                jobStatus = waitCallback(waitCallbackInput, out operationStatus);
                if (JobStatus.Completed == jobStatus)
                {                    
                    hasTimedOut = executionWaitTimeInSec >= waitTimeInSec;
                    if(hasTimedOut)
                    {
                        jobStatus = JobStatus.Timedout;
                        break;
                    }
                    else if (operationStatus == OperationStatus.Completed || operationStatus == OperationStatus.Success)
                    {
                        jobStatus = JobStatus.Completed;
                        break;
                    }
                    else if(operationStatus == OperationStatus.Failed)
                    {
                        jobStatus = JobStatus.Failed;
                        break;
                    }
                }
                else
                {                    
                    break;
                }                
                System.Threading.Thread.Sleep(sleepIntervalInMilliSec);
                executionWaitTimeInSec += SleepIntervalInSec;
            }            
            Logger.Info(String.Format("Time taken to check status of Job {0} is {1} seconds", waitCallback.Method.Name, executionWaitTimeInSec));
           
            return jobStatus;
        }       
    }
}
