
///
///  \file azurereagentdummyimpl.h
///
///  \brief wrapper class for interacting with MARS (azure) agent
///
///

#ifndef INMAGE_AZUREAGENTDUMMYIMPL_H
#define INMAGE_AZUREAGENTDUMMYIMPL_H


#include <string>
#include <vector>

#include "azureagentimpl.h"

namespace AzureAgentInterface
{

	/// \brief class for connecting to azure agent using dummy implementation
	///
	/// Note: This class should not be used directly 
	/// instead use AzureAgent passing in implementation type as dummy
	///
	class AzureAgentDummyImpl : public AzureAgentImpl
	{
	public:

		/// \brief constructor
		/// It set up com interface
		/// and stores vmid, deviceId and size needed for further
		// calls to azure agent

		AzureAgentDummyImpl() {}

		/// \brief destructor
		/// tear down the com interface
		~AzureAgentDummyImpl() {}


		/// \brief StartResync is called to initalize sync session
		/// It should be called each time either dataprotection or agent 
		/// is restarted for the same resync job.

		void StartResync() {}

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
			checksums.resize(digestLen);
			memset(&checksums[0], '\0', checksums.size());
		}

		/// \brief ApplyLog
		/// API invoked by resync threads to apply resync log file

		void ApplyLog(const std::string & resyncFilePath) {}


		/// \brief CompleteResync
		/// API invoked by MT when exiting “Resync” mode

		void CompleteResync(bool status) {}

		/// \brief UploadLogs
		/// API invoked by MT during Resync Step 2 and DiffSync.
		/// containsRecoverablePoints will be set to false when Resync Step 2 is in progress / bitmap mode
		///	azureDiffInfos is ordered list of files along with metadata information
		/// Expectation is that the same set of files will be sent on retry / crash.
		/// Return value will indicate target initiated resync where applicable.This needs to be surfaced to CS.

		void UploadLogs(const std::vector<AzureDiffLogInfo> & azureDiffInfos,
			bool containsRecoverablePoints) {}

        /// \brief UploadEvents
        /// API invoked by MT to upload events, logs and telemetries from various
        /// source components into Telemetry service.

        void UploadEvents(const std::vector<std::string> &logFilePaths) {}

#ifdef SV_WINDOWS
		/// \brief CheckMarsAgentHealth is called to update the MARS health.
		/// It should be called periodically by MT to report the health.

		std::list<HRESULT> CheckMarsAgentHealth()
		{
			std::list<HRESULT> hrs;
			return hrs;
		}
#endif

	protected:

	private:

	};

}

#endif