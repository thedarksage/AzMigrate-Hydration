///
///  \file resyncazurereagent.h
///
///  \brief wrapper class for interacting with MARS (azure) agent
///         while replication is in resync
///
///

#ifndef INMAGE_AZUREAGENT_H
#define INMAGE_AZUREAGENT_H


#include "azureagentimpl.h"
#include "azureagentdummyimpl.h"
#include "azureagentunsupportedimpl.h"

#ifdef WIN32
#include "azureagentcomimpl.h"
#endif

#include "logger.h"

#include <string>
#include <vector>
#include <boost/shared_ptr.hpp>


namespace AzureAgentInterface
{
	/// \brief wrapper class for communicating with azure
	/// agent while replication is in resync.
	class AzureAgent
	{
	public:

		typedef boost::shared_ptr<AzureAgent> ptr;

		/// \brief constructor
		/// It calls base class to set up com interface
		/// and stores vmid, deviceId and size needed for further
		// calls to azure agent

		AzureAgent(
			const std::string & vmName,
			const std::string & vmId,
			const std::string & deviceId,
			unsigned long long deviceSize,
			unsigned long long blockSize,
			const std::string & resyncJobId,
			const AzureAgentImplTypes & implType = AZURE_COMIMPL,
			unsigned int maxAttempts = AZUREIF_MAX_ATTEMPTS,
			unsigned int retryInterval = AZUREIF_RETRY_INTERVAL_SECS,
			bool isSignatureCalculationRequired = true)
		{
			INM_TRACE_ENTER_FUNC

			switch (implType)
			{
#ifdef WIN32
			case AZURE_COMIMPL:
				m_impl.reset(new AzureAgentComImpl(vmName, vmId, deviceId, deviceSize, blockSize, resyncJobId,
					maxAttempts, retryInterval, isSignatureCalculationRequired));
				break;
#endif

			case AZURE_DUMMYIMPL:
				m_impl.reset(new AzureAgentDummyImpl());
				break;

			case AZURE_NOTSUPPORTEDIMPL:
			default:
				m_impl.reset(new AzureAgentUnsupportedImpl());
				break;
			}

			INM_TRACE_EXIT_FUNC

		}

		/// \brief destructor
		/// nothing to do, all objects are self managed
		~AzureAgent() {}

		/// \brief StartResync is called to initalize sync session
		/// It should be called each time either dataprotection or agent 
		/// is restarted for the same resync job.

		void StartResync()
		{
			m_impl->StartResync();
		}

		// \brief GetChecksums
		/// API invoked by resync threads to get MD5 checksum.
		/// startingOffset -starting offset into the disk
		/// length - length of data being processed.
		///          This is interpreted to figure out how many checksums to return, 
		///          we are not looking at a variable checksum block size.
		/// digestLen - expected length of checksum array
		/// MT will ask for the checksums starting from the beginning and going towards the end,
		/// but need not be strictly sequential since multiple threads might be active.

		void GetChecksums(unsigned long long startingOffset,
			unsigned long long length,
			std::vector<unsigned char> & checksums,
			size_t digestLen)
		{
			m_impl->GetChecksums(startingOffset, length, checksums,digestLen);
		}

		/// \brief ApplyLog
		/// API invoked by resync threads to apply resync log file

		void ApplyLog(const std::string & resyncFilePath)
		{
			m_impl->ApplyLog(resyncFilePath);
		}


		/// \brief CompleteResync
		/// API invoked by MT when exiting “Resync” mode

		void CompleteResync(bool status)
		{
			m_impl->CompleteResync(status);
		}

		/// \brief UploadLogs
		/// API invoked by MT during Resync Step 2 and DiffSync.
		/// containsRecoverablePoints will be set to false when Resync Step 2 is in progress / bitmap mode
		///	azureDiffInfos is ordered list of files along with metadata information
		/// Expectation is that the same set of files will be sent on retry / crash.
		/// Return value will indicate target initiated resync where applicable.This needs to be surfaced to CS.

		void UploadLogs(const std::vector<AzureDiffLogInfo> & azureDiffInfos,
			bool containsRecoverablePoints)
		{
			m_impl->UploadLogs(azureDiffInfos, containsRecoverablePoints);
		}

        /// \brief UploadEvents
        /// API invoked by MT to upload events, logs and telemetries from various
        /// source components into Telemetry service.
        void UploadEvents(const std::vector<std::string> &logFilePaths)
        {
            m_impl->UploadEvents(logFilePaths);
        }

#ifdef SV_WINDOWS
		/// \brief CheckMarsAgentHealth is called to update the MARS health.
		/// It should be called periodically by MT to report the health.

		std::list<HRESULT> CheckMarsAgentHealth()
		{
			return m_impl->CheckMarsAgentHealth();
		}
#endif

	protected:

	private:

		AzureAgentImpl::ptr m_impl;

	};

}

#endif