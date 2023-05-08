#ifndef PI_TELEMETRY__H
#define PI_TELEMETRY__H

#include "boost/date_time/posix_time/posix_time.hpp"
#include "pushjobdefinition.h"
#include "pushinstallexception.h"
#include "pushinstallfallbackexception.h"
#include "errormessage.h"

namespace PI
{
	enum PushinstallJobStatus
	{
		/// <summary>
		/// Status represents job has completed.
		/// </summary>
		PushInstallSuccess,

		/// <summary>
		/// Status represents job has failed.
		/// </summary>
		PushInstallFailed
	};

	class PushinstallTelemetry
	{

	public:

		PushinstallTelemetry()
		{
			StartTime = boost::posix_time::microsec_clock::local_time();
			EndTime = StartTime;
			Status = PushinstallJobStatus::PushInstallFailed;
			ErrorCode = "EP2001";
			ErrorCodeName = GetErrorNameForErrorcode(ErrorCode);
		}

		void RecordJobStarted()
		{
			StartTime = boost::posix_time::microsec_clock::local_time();
		}

		void RecordJobSucceeded()
		{
			this->RecordJobEndTime();
			Status = PushinstallJobStatus::PushInstallSuccess;
			ErrorCode = "0";
			ErrorCodeName = GetErrorNameForErrorcode(ErrorCode);
			ComponentErrorCode = "";
		}

		void RecordJobFailed(PushInstallException & exception)
		{
			this->RecordJobEndTime();
			Status = PushinstallJobStatus::PushInstallFailed;
			ErrorCode = exception.ErrorCode;
			ErrorCodeName = exception.ErrorCodeName;
			ComponentErrorCode = "";
		}

		void RecordJobFailed(PushInstallFallbackException & exception)
		{
			this->RecordJobEndTime();
			Status = PushinstallJobStatus::PushInstallFailed;
			ErrorCode = exception.ErrorCode;
			ErrorCodeName = exception.ErrorCodeName;
			ComponentErrorCode = exception.ComponentErrorCode;
		}

		void RecordJobFailed(std::exception & exception)
		{
			this->RecordJobEndTime();
			Status = PushinstallJobStatus::PushInstallFailed;
			ErrorCode = "EP2001";
			ErrorCodeName = GetErrorNameForErrorcode(ErrorCode);
			ComponentErrorCode = "";
		}

		void RecordJobFailed()
		{
			this->RecordJobEndTime();
			Status = PushinstallJobStatus::PushInstallFailed;
			ErrorCode = "EP2001";
			ErrorCodeName = GetErrorNameForErrorcode(ErrorCode);
			ComponentErrorCode = "";
		}

		void SendTelemetryData(const PushJobDefinition & jobDefinition);

	private:

		void SendPushinstallTelemetry(const PushJobDefinition & jobDefinition);
		void SendAgentinstallTelemetry(const PushJobDefinition & jobDefinition);
		void WriteTelemetryMetadata(std::string & metadataFilePath, const PushJobDefinition & jobDefinition);

		void RecordJobEndTime()
		{
			EndTime = boost::posix_time::microsec_clock::local_time();
		}

		const char* GetName(PushJobType jobType)
		{
			if (jobType == pushFreshInstall)
			{
				return "pushFreshInstall";
			}
			else if (jobType == pushUpgrade)
			{
				return "pushUpgrade";
			}
			else if (jobType == pushPatchInstall)
			{
				return "pushPatchInstall";
			}
			else if (jobType == pushUninstall)
			{
				return "pushUninstall";
			}
			else if (jobType == fetchDistroDetails)
			{
				return "fetchDistroDetails";
			}
			else if (jobType == pushConfigure)
			{
				return "pushConfigure";
			}
			else
			{
				throw std::runtime_error("Unknown job type.");
			}
		}

		const char* GetName(PushinstallJobStatus status)
		{
			if (status == PushinstallJobStatus::PushInstallSuccess)
			{
				return "Success";
			}
			else if (status == PushinstallJobStatus::PushInstallFailed)
			{
				return "Failed";
			}
			else
			{
				throw std::runtime_error("Unknown job status.");
			}
		}

		const char * GetName(VM_TYPE vmType)
		{
			if (vmType == VMWARE)
			{
				return "VMWARE";
			}
			else if (vmType == PHYSICAL)
			{
				return "PHYSICAL";
			}
			else
			{
				throw std::runtime_error("Unknown VM type.");
			}
		}

		const char * GetOsType(int osIdx)
		{
			if (osIdx == 1)
			{
				return "Windows";
			}
			else if (osIdx == 2)
			{
				return "Linux";
			}
			else
			{
				throw std::runtime_error("Unknown operating system type.");
			}
		}

		boost::posix_time::ptime StartTime;
		boost::posix_time::ptime EndTime;
		PushinstallJobStatus Status;
		std::string ErrorCode;
		std::string ErrorCodeName;
		std::string ComponentErrorCode;
	};
}

#endif
