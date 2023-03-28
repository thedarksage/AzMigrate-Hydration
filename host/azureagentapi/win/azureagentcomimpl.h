
///
///  \file azurereagentcominterface.h
///
///  \brief wrapper class for interacting with MARS (azure) agent
///
///

#ifndef INMAGE_AZUREAGENTCOMIMPL_H
#define INMAGE_AZUREAGENTCOMIMPL_H

#include <Windows.h>
#include <atlbase.h>
#include <atlsafe.h>
#include <comutil.h>
#include <comdef.h>

#include <string>
#include <vector>
#include <list>
#include <boost/thread/win32/shared_mutex.hpp>

#include "azureagentimpl.h"

#include "MTReplicationEndpointProvider_h.h"



namespace AzureAgentInterface
{

    /// \brief class for connecting to azure agent using com inteface
    ///
    /// Note: This class should not be used directly 
    /// instead use AzureAgent
    ///
    class AzureAgentComImpl : public AzureAgentImpl
    {
    public:

        /// \brief constructor
        /// It set up com interface
        /// and stores vmid, deviceId and size needed for further
        // calls to azure agent

        AzureAgentComImpl(
			const std::string &vmName,
			const std::string & vmId,
            const std::string & deviceId,
            unsigned long long deviceSize,
			unsigned long long blockSize,
            const std::string & resyncJobId,
            unsigned int maxAttempts = AZUREIF_MAX_ATTEMPTS,
            unsigned int retryInterval = AZUREIF_RETRY_INTERVAL_SECS, 
            bool isSignatureCalculationRequired = true);


        /// \brief destructor
        /// tear down the com interface
        ~AzureAgentComImpl();


        /// \brief StartResync is called to initalize sync session
        /// It should be called each time either dataprotection or agent 
        /// is restarted for the same resync job.

        void StartResync();

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
			size_t digestLen);

        /// \brief ApplyLog
        /// API invoked by resync threads to apply resync log file

        void ApplyLog(const std::string & resyncFilePath);


        /// \brief CompleteResync
        /// API invoked by MT when exiting “Resync” mode

        void CompleteResync(bool status);

        /// \brief UploadLogs
        /// API invoked by MT during Resync Step 2 and DiffSync.
        /// containsRecoverablePoints will be set to false when Resync Step 2 is in progress / bitmap mode
		///	azureDiffInfos is ordered list of files along with metadata information
        /// Expectation is that the same set of files will be sent on retry / crash.
        /// Return value will indicate target initiated resync where applicable.This needs to be surfaced to CS.

		void UploadLogs(const std::vector<AzureDiffLogInfo> & azureDiffInfos,
            bool containsRecoverablePoints);

        /// \brief UploadEvents
        /// API invoked by MT to upload events, logs and telemetries from various
        /// source components into Telemetry service.
        void UploadEvents(const std::vector<std::string> &logFilePaths);

		/// \brief CheckMarsAgentHealth is called to update the MARS health.
		/// It should be called periodically by MT to report the health.

		std::list<HRESULT> CheckMarsAgentHealth();

		static InMageDifferentialLogFileInfo COMConvertDiffInfo(const  AzureDiffLogInfo & diffInfo);

    protected:
        
    private:

        /// \brief reinitialize is used to handle scenarios 
        //   where the azure agent is no longer available (crashed)
        void reinitialize();

        /// \brief isinitFailed is used to handle scenarios 
        ///   where the azure agent is no longer available (crashed)
        ///  and exception has been thrown from reinitialie routine
        /// all attempts to reinitialize have failed
        bool isInitFailed();


        /// \brief StartResync is called to initalize sync session
        /// It should be called each time either dataprotection or agent 
        /// is restarted for the same resync job.

        HRESULT StartResync(
			/* [in] */ BSTR VmName,
            /* [in] */ BSTR VmId,
            /* [in] */ BSTR DiskId,
            /* [in] */ unsigned long long DiskSize,
			/* [in] */ unsigned long long BlockkSize,
            /* [in] */ BSTR JobId,
            /* [in, optional, defaultvalue(TRUE)] */ bool isSignatureCalculationRequired = true);

        // \brief GetChecksums
        /// API invoked by resync threads to get checksum.
        /// startingOffset -starting offset into the disk
        /// length - length of data being processed.
        ///          This is interpreted to figure out how many checksums to return, 
        ///          we are not looking at a variable checksum block size.
        /// MT will ask for the checksums starting from the beginning and going towards the end,
        /// but need not be strictly sequential since multiple threads might be active.

        HRESULT GetChecksum(
			/* [in] */ BSTR VmName,
            /* [in] */ BSTR VmId,
            /* [in] */ BSTR DiskId,
            /* [in] */ BSTR JobId,
            /* [in] */ unsigned long long Offset,
            /* [in] */ unsigned long long Length,
            /* [retval][out] */ SAFEARRAY * *Checksums);

        /// \brief ApplyLog
        /// API invoked by resync threads to apply resync log file

        HRESULT ApplyLog(
			/* [in] */ BSTR VmName,
            /* [in] */ BSTR VmId,
            /* [in] */ BSTR DiskId,
            /* [in] */ BSTR JobId,
            /* [in] */ BSTR ResyncLogFilePath);

        /// \brief CompleteResync
        /// API invoked by MT when exiting Resync mode

        HRESULT CompleteResync(
			/* [in] */ BSTR VmName,
            /* [in] */ BSTR VmId,
            /* [in] */ BSTR DiskId,
            /* [in] */ BSTR JobId,
            /* [in] */ boolean IsSuccess);

        /// \brief UploadLogs
        /// API invoked by MT during Resync Step 2 and DiffSync.
        /// containsRecoverablePoints will be set to false when Resync Step 2 is in progress / bitmap mode
        ///	diffFileNames is ordered list of files
        /// Expectation is that the same set of files will be sent on retry / crash.
        /// Return value will indicate target initiated resync where applicable.This needs to be surfaced to CS.

        HRESULT UploadLogs(
			/* [in] */ BSTR VmName,
            /* [in] */ BSTR VmId,
            /* [in] */ BSTR DiskId,
            /* [in] */ SAFEARRAY * DiffLogFilePaths,
            /* [in] */ boolean IsRecoverable);

        /// \brief UploadEvents
        /// API invoked by MT to upload events, logs and telemetries from various
        /// source components into Telemetry service.

        HRESULT UploadEvents(
            /* [in] */ SAFEARRAY *LogFilePaths);

		/// \brief CheckAgentHealth is called to update the MARS health.
		/// It should be called periodically by MT to report the health.

		HRESULT CheckAgentHealth();

        void initializeCOMWithRetries();
        void uninitializeCOM();
        void initializeCOM();

		/// \brief converts error code to error string
		void convertAzureErrorToString(unsigned long err, std::string& errStr);

		std::string m_vmNameStr;
		std::string m_vmIdStr;
		std::string m_deviceIdStr;
        std::string m_resyncJobIdStr;

		CComBSTR m_vmName;
        CComBSTR m_vmId;
        CComBSTR m_deviceId;
        unsigned long long m_deviceSize;
		unsigned long long m_blockSize;
        CComBSTR m_resyncJobId;
        bool m_isSignatureCalculationRequired;

        unsigned int m_maxAttempts;
        unsigned int m_retryInterval;
        IMTReplicationEndpointProvider * m_azureAgentProvider;
        boost::shared_mutex m_mutex;
    };

}

#endif