
///
///  \file azurereagentunsupportedimpl.h
///
///  \brief wrapper class for interacting with MARS (azure) agent
///
///

#ifndef INMAGE_AZUREAGENTUNSUPPORTEDIMPL_H
#define INMAGE_AZUREAGENTUNSUPPORTEDIMPL_H


#include <string>
#include <vector>

#include "errorexception.h"
#include "azureagentimpl.h"

namespace AzureAgentInterface
{

	/// \brief class for connecting to azure agent using unsupported exception implementation
	///
	/// Note: This class should not be used directly 
	/// instead use AzureAgent passing in implementation type as unsupported
	///
	class AzureAgentUnsupportedImpl : public AzureAgentImpl
	{
	public:

		/// \brief constructor
		/// It set up com interface
		/// and stores vmid, deviceId and size needed for further
		// calls to azure agent

		AzureAgentUnsupportedImpl() {}


		/// \brief destructor
		/// tear down the com interface
		~AzureAgentUnsupportedImpl() {}


		/// \brief StartResync is called to initalize sync session
		/// It should be called each time either dataprotection or agent 
		/// is restarted for the same resync job.

		void StartResync() 
		{
			throw ERROR_EXCEPTION << "not implemented";
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
			throw ERROR_EXCEPTION << " not implemented";
		}

		/// \brief ApplyLog
		/// API invoked by resync threads to apply resync log file

		void ApplyLog(const std::string & resyncFilePath) 
		{
			throw ERROR_EXCEPTION << " not implemented";
		}


		/// \brief CompleteResync
		/// API invoked by MT when exiting “Resync” mode

		void CompleteResync(bool status) 
		{
			throw ERROR_EXCEPTION << " not implemented";
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
			throw ERROR_EXCEPTION << " not implemented";
		}

        /// \brief UploadEvents
        /// API invoked by MT to upload events, logs and telemetries from various
        /// source components into Telemetry service.

        void UploadEvents(const std::vector<std::string> &logFilePaths)
        {
            throw ERROR_EXCEPTION << " not implemented";
        }

#ifdef SV_WINDOWS
		/// \brief CheckMarsAgentHealth is called to update the MARS health.
		/// It should be called periodically by MT to report the health.

		std::list<HRESULT> CheckMarsAgentHealth()
		{
			throw ERROR_EXCEPTION << " not implemented";
		}
#endif

	protected:

	private:

	};

}

#endif