/*
+------------------------------------------------------------------------------------+
Copyright(c) Microsoft Corp.
+------------------------------------------------------------------------------------+
File		:	RcmJob.h

Description	:   RcmJob class represents the job structure received from RCM.

+------------------------------------------------------------------------------------+
*/

#ifndef __RCM_JOB_H__
#define __RCM_JOB_H__

#include <string>
#include <vector>
#include "json_reader.h"
#include "json_writer.h"
#include "json_adapter.h"

namespace PI
{
	class ExtendedError
	{
	public:
		std::string error_name;
		std::map<std::string, std::string> error_params;
		std::string default_message;

		void serialize(JSON::Adapter& adapter)
		{
			JSON::Class root(adapter, "ExtendedError", false);

			JSON_E(adapter, error_name);
			JSON_E(adapter, error_params);
			JSON_T(adapter, default_message);
		}
	};

	class ExtendedErrors
	{
	public:
		std::vector<ExtendedError> errors;

		void serialize(JSON::Adapter& adapter)
		{
			JSON::Class root(adapter, "ExtendedErrors", false);

			JSON_T(adapter, errors);
		}
	};

	class ComponentError
	{
	public:
		std::string ComponentId;
		std::string ErrorCode;
		std::string Message;
		std::string Severity;
		std::string PossibleCauses;
		std::string RecommendedAction;
		std::map<std::string, std::string> MessageParameters;
	};

	class RcmJobError
	{
	public:
		std::string ErrorCode;
		std::string Message;
		std::string PossibleCauses;
		std::string RecommendedAction;
		std::string Severity;
		ComponentError ComponentError;
		std::map<std::string, std::string> MessageParameters;
	};

	class JobContext
	{
	public:
		std::string WorkflowId;
		std::string ActivityId;
		std::string ClientRequestId;
		std::string ContainerId;
		std::string ResourceId;
		std::string ResourceLocation;
		std::string AcceptLanguage;
		std::string ServiceActivityId;
		std::string SrsActivityId;
		std::string SubscriptionId;
		std::string AgentMachineId;
		std::string AgentDiskId;
	};

	class RcmJob
	{
	public:
		std::string Id;
		std::string ConsumerId;
		std::string ComponentId;
		std::string JobType;
		std::string SessionId;
		std::string JobStatus;
		std::string RcmServiceUri;
		std::string InputPayload;
		std::string OutputPayload;
		std::vector<RcmJobError> Errors;
		JobContext Context;
		std::string JobCreationimeUtc;
		std::string JobExpiryTimeUtc;
	};
}

#endif