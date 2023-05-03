
///
///  \file azurereagenyimpl.h
///
///  \brief abstract base class for interacting with MARS (azure) agent
///
///

#ifndef INMAGE_AZUREAGENTIMPL_H
#define INMAGE_AZUREAGENTIMPL_H


#include <string>
#include <vector>
#include <list>
#include <boost/shared_ptr.hpp>


namespace AzureAgentInterface
{
	const unsigned int AZUREIF_MAX_ATTEMPTS = 10;
	const unsigned int AZUREIF_RETRY_INTERVAL_SECS = 60;

	enum AzureAgentImplTypes { AZURE_COMIMPL, AZURE_DUMMYIMPL, AZURE_NOTSUPPORTEDIMPL };


	struct AzureDiffLogInfo
	{
		// end timestamp of last uploaded file
		unsigned long long PrevEndTime;

		// end sequence of last uploaded file
		unsigned long long PrevEndTimeSequenceNumber;

		// continuation id of last uploaded file
		unsigned int PrevContinuationId;

		// end timestamp of file to be uploaded
		unsigned long long EndTime;

		// end sequence of file to be uploaded
		unsigned long long EndTimeSequenceNumber;

		// continuation id of file to be uploaded
		unsigned int ContinuationId;

		// size in bytes of file to be uploaded
		unsigned long long FileSize;

		// complete path of file to upload"
#ifdef WIN32
		std::wstring FilePath;
#else
		std::string FilePath;
#endif
	};

	/// \brief class for connecting to azure agent using com inteface
	///
	/// Note: This class should not be used directly 
	/// instead use AzureAgent
	///
	class AzureAgentImpl
	{
	public:

		typedef boost::shared_ptr<AzureAgentImpl> ptr;

		/// \brief constructor
		/// It set up com interface
		/// and stores vmid, deviceId and size needed for further
		// calls to azure agent

		AzureAgentImpl() {}

		/// \brief destructor
		/// tear down the com interface
		virtual ~AzureAgentImpl() {}


		/// \brief StartResync is called to initalize sync session
		/// It should be called each time either dataprotection or agent 
		/// is restarted for the same resync job.

		virtual void StartResync() = 0;

		// \brief GetChecksums
		/// API invoked by resync threads to get MD5 checksum.
		/// startingOffset -starting offset into the disk
		/// length - length of data being processed.
		///          This is interpreted to figure out how many checksums to return, 
		///          we are not looking at a variable checksum block size.
		/// digestLen - expected length of checksum array
		/// MT will ask for the checksums starting from the beginning and going towards the end,
		/// but need not be strictly sequential since multiple threads might be active.


		virtual void GetChecksums(unsigned long long startingOffset,
			unsigned long long length,
			std::vector<unsigned char> & checksums,
			size_t digestLen) = 0;

		/// \brief ApplyLog
		/// API invoked by resync threads to apply resync log file

		virtual void ApplyLog(const std::string & resyncFilePath) = 0;


		/// \brief CompleteResync
		/// API invoked by MT when exiting “Resync” mode

		virtual void CompleteResync(bool status) = 0;

		/// \brief UploadLogs
		/// API invoked by MT during Resync Step 2 and DiffSync.
		/// containsRecoverablePoints will be set to false when Resync Step 2 is in progress / bitmap mode
		///	azureDiffInfos is ordered list of files along with metadata information
		/// Expectation is that the same set of files will be sent on retry / crash.
		/// Return value will indicate target initiated resync where applicable.This needs to be surfaced to CS.

		virtual void UploadLogs(const std::vector<AzureDiffLogInfo> & azureDiffInfos,
			bool containsRecoverablePoints) = 0;

        /// \brief UploadEvents
        /// API invoked by MT to upload events, logs and telemetries from various
        /// source components into Telemetry service.

        virtual void UploadEvents(const std::vector<std::string> &logFilePaths) = 0;

#ifdef SV_WINDOWS
		/// \brief CheckMarsAgentHealth is called to update the MARS health.
		/// It should be called periodically by MT to report the health.

		virtual std::list<HRESULT> CheckMarsAgentHealth() = 0;
#endif

	protected:

	private:

	};

}

#endif