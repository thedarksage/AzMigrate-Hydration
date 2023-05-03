using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Reflection;
using LoggerInterface;
using Newtonsoft.Json;
using RcmAgentCommonLib.LoggerInterface;
using RcmClientLib;
using RcmContract;
using MarsAgent.ErrorHandling;
using MarsAgent.Service;

namespace MarsAgent.LoggerInterface
{
    /// <summary>
    /// Implements logging interface used by the service.
    /// </summary>
    public sealed class Logger : RcmAgentCommonLogger<LogContext>
    {
        /// <summary>
        /// Singleton instance of <see cref="Logger"/> used by all callers for logging.
        /// </summary>
        public static readonly Logger Instance = new Logger();

        /// <summary>
        /// Start event ID for operation events.
        /// </summary>
        private const int OperationSuccessEventsRangeStart = 2101;

        /// <summary>
        /// End event ID for operation events.
        /// </summary>
        private const int OperationSuccessEventsRangeEnd = 2200;

        /// <summary>
        /// Start event ID for life cycle events.
        /// </summary>
        private const int LifeCycleSuccessEventsRangeStart = 2201;

        /// <summary>
        /// End event ID for life cycle events.
        /// </summary>
        private const int LifeCycleSuccessEventsRangeEnd = 2300;

        /// <summary>
        /// Start event ID for operation events.
        /// </summary>
        private const int OperationFailureEventsRangeStart = 4101;

        /// <summary>
        /// End event ID for operation events.
        /// </summary>
        private const int OperationFailureEventsRangeEnd = 4200;

        /// <summary>
        /// Start event ID for life cycle events.
        /// </summary>
        private const int LifeCycleFailureEventsRangeStart = 4201;

        /// <summary>
        /// End event ID for life cycle events.
        /// </summary>
        private const int LifeCycleFailureEventsRangeEnd = 4300;

        /// <summary>
        /// Initializes a new instance of the <see cref="Logger" /> class.
        /// </summary>
        public Logger()
            : base(ServiceProperties.Instance.EventSourceName)
        {
        }

        /// <summary>
        /// Fetches the service specific event object to be used for logging to MDS.
        /// </summary>
        /// <param name="eventName">The event name to be logged.</param>
        /// <param name="traceLevel">The trace level to be associated with the event.</param>
        /// <returns>The event object to be used for logging.</returns>
        public override EventBase GetEventObject(string eventName, out TraceEventType traceLevel)
        {
            RcmAgentEventId rcmAgentEventId;
            if (!Enum.TryParse(eventName, out rcmAgentEventId))
            {
                throw new ArgumentException($"Invalid EventName '{eventName}'.");
            }

            return this.GetEventObject((int)rcmAgentEventId, out traceLevel);
        }

        /// <summary>
        /// Fetches the service specific event object to be used for logging.
        /// </summary>
        /// <param name="eventId">The event to be logged.</param>
        /// <param name="traceLevel">The trace level to be associated with the event.</param>
        /// <returns>The event object to be used for logging.</returns>
        public override EventBase GetEventObject(int eventId, out TraceEventType traceLevel)
        {
            traceLevel = TraceEventType.Information;
            RcmAgentEventId rcmEventId = (RcmAgentEventId)eventId;
            RcmAgentEventInfo eventInfo;
            if (EventInfoCollection.Collection.TryGetValue(rcmEventId, out eventInfo))
            {
                traceLevel = eventInfo.TraceLevel;
            }

            if ((eventId >= Logger.OperationSuccessEventsRangeStart &&
                eventId <= Logger.OperationSuccessEventsRangeEnd) ||
                (eventId >= Logger.OperationFailureEventsRangeStart &&
                eventId <= Logger.OperationFailureEventsRangeEnd))
            {
                var operationObject = new OperationEvent();
                operationObject.ServiceEventId = eventId;
                operationObject.ServiceEventName = ((RcmAgentEventId)eventId).ToString();
                return operationObject;
            }
            else if ((eventId >= Logger.LifeCycleSuccessEventsRangeStart &&
                eventId <= Logger.LifeCycleSuccessEventsRangeEnd) ||
                (eventId >= Logger.LifeCycleFailureEventsRangeStart &&
                eventId <= Logger.LifeCycleFailureEventsRangeEnd))
            {
                var lifeCycleObject = new LifeCycleEvent();
                lifeCycleObject.ServiceEventId = eventId;
                lifeCycleObject.ServiceEventName = ((RcmAgentEventId)eventId).ToString();
                return lifeCycleObject;
            }
            else
            {
                var eventObject = new DiagnosticEvent();
                eventObject.ServiceEventId = eventId;
                eventObject.ServiceEventName = ((RcmAgentEventId)eventId).ToString();
                return eventObject;
            }
        }

        /// <summary>
        /// Fetches the service specific event object to be used for logging.
        /// </summary>
        /// <param name="eventType">The generic event to be logged.</param>
        /// <returns>The event object to be used for logging.</returns>
        public override EventBase GetEventObject(GenericEvent eventType)
        {
            switch (eventType)
            {
                case GenericEvent.ConfigurationSettingChange:
                case GenericEvent.ConfigurationSettingDecryptionFailure:
                    var lifeCycleObject = new LifeCycleEvent();
                    lifeCycleObject.ServiceEventId = (int)eventType;
                    lifeCycleObject.ServiceEventName = eventType.ToString();
                    return lifeCycleObject;

                default:
                    return new DiagnosticEvent
                    {
                        ServiceEventId = (int)eventType,
                        ServiceEventName = eventType.ToString()
                    };
            }
        }

        /// <summary>
        /// Fetches the service specific event object to be used for logging.
        /// </summary>
        /// <param name="ex">The exception to be logged.</param>
        /// <param name="knownException">Set to true if this is a know exception, false otherwise.
        /// </param>
        /// <returns>The event object to be used for logging.</returns>
        public override EventBase GetEventObject(Exception ex, out bool knownException)
        {
            int errorCode;
            string errorCodeName;
            string errorTags = string.Empty;
            if (ex is MarsAgentException)
            {
                knownException = true;
                errorCode = (int)((MarsAgentException)ex).ErrorCode;
                errorCodeName = ((MarsAgentException)ex).ErrorCode.ToString();
                errorTags = JsonConvert.SerializeObject(
                    ((MarsAgentException)ex).ErrorTags,
                    Formatting.Indented);
            }
            else if (ex is RcmException &&
                Enum.TryParse(((RcmException)ex).ErrorCode, out RcmErrorCode rcmErrorCode))
            {
                knownException = true;
                errorCode = (int)rcmErrorCode;
                errorCodeName = rcmErrorCode.ToString();

                var rcmErrorTags = new Dictionary<string, string>();
                rcmErrorTags["Category"] = ((RcmException)ex).ErrorCategory;
                errorTags = JsonConvert.SerializeObject(rcmErrorTags, Formatting.Indented);
            }
            else
            {
                knownException = false;
                errorCode = (int)MarsAgentErrorCode.UnhandledException;
                errorCodeName = MarsAgentErrorCode.UnhandledException.ToString();
            }

            ErrorEvent eventObject = new ErrorEvent();
            eventObject.ErrorCode = (int)errorCode;
            eventObject.ErrorCodeName = errorCodeName;
            eventObject.ErrorTags = errorTags;

            return eventObject;
        }

        /// <summary>
        /// Fetches the service specific event object to be used for logging log context missing
        /// event.
        /// </summary>
        /// <returns>The event object to be used for logging.</returns>
        /// <remarks>This event object is used in case log context is missing in current thread.
        /// GetLogContext should not be called inside this function as it will result in
        /// recursive call.</remarks>
        public override EventBase GetLogContextMissingEventObject()
        {
            // Since this call is coming from GetLogContext(), we should not try to fetch
            // log context here. Else it will result in recursion.
            var eventObject = new DiagnosticEvent(fetchLogContext: false);

            eventObject.ServiceEventId = (int)RcmAgentEventId.LogContextMissingEvent;
            eventObject.ServiceEventName = nameof(
                RcmAgentEventId.LogContextMissingEvent);

            return eventObject;
        }

        /// <summary>
        /// Creates a log context with the specified information and associates it with the
        /// thread.
        /// </summary>
        /// <param name="applianceId">Appliance identifier.</param>
        /// <param name="activityId">The activity ID to be used for logging.</param>
        public void InitializeLogContext(string applianceId, string activityId)
        {
            this.SetLogContext(new LogContext(applianceId, Guid.Parse(activityId)));
        }

        /// <summary>
        /// Gets the properties of all logging events.
        /// </summary>
        /// <returns>Properties of all logging events.</returns>
        public override Dictionary<string, PropertyInfo[]> GetEventProperties()
        {
            var eventProperties = new Dictionary<string, PropertyInfo[]>();

            eventProperties.Add(
                typeof(DiagnosticEvent).Name,
                typeof(DiagnosticEvent).GetProperties(BindingFlags.Instance | BindingFlags.Public));

            eventProperties.Add(
                typeof(OperationEvent).Name,
                typeof(OperationEvent).GetProperties(BindingFlags.Instance | BindingFlags.Public));

            eventProperties.Add(
                typeof(LifeCycleEvent).Name,
                typeof(LifeCycleEvent).GetProperties(BindingFlags.Instance | BindingFlags.Public));

            eventProperties.Add(
                typeof(ErrorEvent).Name,
                typeof(ErrorEvent).GetProperties(BindingFlags.Instance | BindingFlags.Public));

            eventProperties.Add(
                typeof(MetricEvent).Name,
                typeof(MetricEvent).GetProperties(BindingFlags.Instance | BindingFlags.Public));

            return eventProperties;
        }

        /// <summary>
        /// Gets the versions of all logging events.
        /// </summary>
        /// <returns>The versions of all logging events.</returns>
        public override Dictionary<string, string> GetEventVersions()
        {
            var eventVersions = new Dictionary<string, string>();

            eventVersions.Add(
                typeof(DiagnosticEvent).Name,
                new DiagnosticEvent(fetchLogContext: false).GetEventVersion());

            eventVersions.Add(
                typeof(OperationEvent).Name,
                new OperationEvent(fetchLogContext: false).GetEventVersion());

            eventVersions.Add(
                typeof(LifeCycleEvent).Name,
                new LifeCycleEvent(fetchLogContext: false).GetEventVersion());

            eventVersions.Add(
                typeof(ErrorEvent).Name,
                new ErrorEvent(fetchLogContext: false).GetEventVersion());

            eventVersions.Add(
                typeof(MetricEvent).Name,
                new MetricEvent(fetchLogContext: false).GetEventVersion());

            return eventVersions;
        }
    }
}