#include "azureagentcomimpl.h"

#include <sstream>

#include <boost/thread/locks.hpp>

#include "comconversion.h"
#include "errorexception.h"
#include "converterrortostringminor.h"
#include "logger.h"
#include "cdputil.h"

#include "azureresyncrequiredexception.h"
#include "azurebadfileexception.h"
#include "azuremissingfileexception.h"
#include "azurecanceloperationexception.h"
#include "azureinvalidopexception.h"
#include "azureerrortypes.h"
#include "inmalertdefs.h"
#include "configwrapper.h"

namespace AzureAgentInterface
{

    AzureAgentComImpl::AzureAgentComImpl(
        const std::string & vmName,
        const std::string & vmId,
        const std::string & deviceId,
        unsigned long long deviceSize,
        unsigned long long blockSize,
        const std::string & resyncJobId,
        unsigned int maxAttempts,
        unsigned int retryInterval, 
        bool isSignatureCalculationRequired)
        :m_vmNameStr(vmName),
        m_vmIdStr(vmId),
        m_deviceIdStr(deviceId),
        m_resyncJobIdStr(resyncJobId),	
        m_maxAttempts(maxAttempts),
        m_retryInterval(retryInterval),
        m_azureAgentProvider(0),
        m_isSignatureCalculationRequired(isSignatureCalculationRequired)
    {
        DebugPrintf(SV_LOG_DEBUG, "Entered %s args: vmName=%s, vmId=%s, deviceId=%s, resyncJobId=%s\n",
            FUNCTION_NAME, vmName.c_str(), vmId.c_str(), deviceId.c_str(), resyncJobId.c_str());

        BSTR bstrVmName = ComConversion::ToBstr(vmName);
        m_vmName.Attach(bstrVmName);

        BSTR bstrVmId = ComConversion::ToBstr(vmId);
        m_vmId.Attach(bstrVmId);

        BSTR bstrDevId = ComConversion::ToBstr(deviceId);
        m_deviceId.Attach(bstrDevId);


        m_deviceSize = deviceSize;
        m_blockSize = blockSize;

        BSTR bstJobId = ComConversion::ToBstr(resyncJobId);
        m_resyncJobId.Attach(bstJobId);

        initializeCOMWithRetries();

        INM_TRACE_EXIT_FUNC
    }


    /// \brief destructor
    /// tear down the com interface
    AzureAgentComImpl::~AzureAgentComImpl()
    {
        uninitializeCOM();
    }


    /// \brief reinitialize is used to handle scenarios 
    //   where the azure agent is no longer available (crashed)
    void AzureAgentComImpl::reinitialize()
    {
        INM_TRACE_ENTER_FUNC

        boost::unique_lock< boost::shared_mutex > lock(m_mutex);
        // do work here, with exclusive access
        uninitializeCOM();
        initializeCOMWithRetries();

        INM_TRACE_EXIT_FUNC
    }

    bool AzureAgentComImpl::isInitFailed()
    {
        return (m_azureAgentProvider == 0);
    }

    void AzureAgentComImpl::StartResync()
    {
        INM_TRACE_ENTER_FUNC

        unsigned int attempt = 1;
        bool done = false;
        unsigned int delay = m_retryInterval;


        while (!done) {

            try{

                DebugPrintf(SV_LOG_DEBUG, "Attempt : %u %s\n", attempt, FUNCTION_NAME);

                HRESULT hr = StartResync(m_vmName, m_vmId, m_deviceId,
                    m_deviceSize, m_blockSize, m_resyncJobId, m_isSignatureCalculationRequired);

                if (FAILED(hr)) {

                    std::string err;
                    convertAzureErrorToString(hr, err);
                    std::stringstream errMsg;

                    errMsg << "Attempt: " << attempt <<
                        " : Azure StartResync for vmId:" << m_vmIdStr 
                        << " device: " << m_deviceIdStr
                        << " jobid: " << m_resyncJobIdStr
                        << " IsSignatureCalculationRequired: "<< m_isSignatureCalculationRequired
                        << " failed with result " << std::hex << hr << "(" << err << ")";

                    if (HRESULT_CODE(hr) == RPC_S_SERVER_UNAVAILABLE){

                        DebugPrintf(SV_LOG_ERROR, "%s\n", errMsg.str().c_str());
                        DebugPrintf(SV_LOG_ERROR, 
                            "Azure agent COM service is not available. re-initializing\n");

                        reinitialize();
                        ++attempt;
                    }
                    else{

                        if (hr == MT_E_INITIALIZATION_FAILED ||
                            hr == MT_E_REGISTRATION_FAILED)
                        {
                            AzureAuthenticationFailureAlert a(m_vmIdStr, m_deviceIdStr);
                            SendAlertToCx(SV_ALERT_CRITICAL, SV_ALERT_MODULE_GENERAL, a);
                        }

                        ThrowAzureException(errMsg.str(), hr);
                    }
                }
                else {
                    done = true;
                }
            }
            catch (AzureSpecialException &ae) {
                
                ++attempt;

                // retry for m_maxAttempts
                if (attempt <= m_maxAttempts){

                    DebugPrintf(SV_LOG_ERROR, "%s, re-attempt after %u seconds\n", 
                        ae.what(),
                        delay);

                    // check for quit request, 
                    // if quit requested throw exception 
                    // else log the failed attempt and retry
                    // 
                    if (CDPUtil::QuitRequested(delay))
                        throw;

                    delay *= 2;
                }
                else {
                    throw ERROR_EXCEPTION << ae.what();
                }
            }
            catch (AzureCancelOpException & ce) {
                throw;
            }
            catch (AzureInvalidOpException & ie) {
                throw;
            }
            catch (ErrorException & ee) {

                ++attempt;

                if (isInitFailed())
                    throw;

                // retry for m_maxAttempts
                if (attempt <= m_maxAttempts){

                    DebugPrintf(SV_LOG_ERROR, "%s, re-attempt after %u seconds\n", 
                        ee.what(),
                        m_retryInterval);

                    // check for quit request, 
                    // if quit requested throw exception 
                    // else log the failed attempt and retry
                    // 
                    if (CDPUtil::QuitRequested(m_retryInterval))
                        throw;


                }
                else {
                    throw;
                }
            }

        } // while loop

        INM_TRACE_EXIT_FUNC

    }

    void AzureAgentComImpl::GetChecksums(unsigned long long startingOffset,
        unsigned long long length,
        std::vector<unsigned char> & checksums,
        size_t digestLen)
    {
        DebugPrintf(SV_LOG_DEBUG, "Entered %s args: startingOffset=%llu, length=%llu\n",
            FUNCTION_NAME, startingOffset, length);

        SAFEARRAY * sa = NULL;

        CComSafeArray<unsigned char> csa;


        unsigned int attempt = 1;
        bool done = false;
        unsigned int delay = m_retryInterval;

        while (!done) {

            try{

                DebugPrintf(SV_LOG_DEBUG, "Attempt : %u %s\n", attempt, FUNCTION_NAME);

                HRESULT hr = GetChecksum(m_vmName, m_vmId, m_deviceId,
                    m_resyncJobId, startingOffset, length, &sa);


                if (FAILED(hr)) {

                    std::string err;
                    convertAzureErrorToString(hr, err);
                    std::stringstream errMsg;

                    errMsg << "Attempt: " << attempt <<
                        " : Azure GetChecksums for vmId:" << m_vmIdStr
                        << " device: " << m_deviceIdStr
                        << " jobid: " << m_resyncJobIdStr
                        << " offset:" << startingOffset << " length:" << length
                        << " failed with result " << std::hex << hr << "(" << err << ")";

                    if (HRESULT_CODE(hr) == RPC_S_SERVER_UNAVAILABLE){

                        DebugPrintf(SV_LOG_ERROR, "%s\n", errMsg.str().c_str());
                        DebugPrintf(SV_LOG_ERROR, "Azure agent COM service is not available. re-initializing\n");
                        
                        reinitialize();
                        StartResync();
                        ++attempt;
                    }
                    else if (hr == MT_E_RESTART_RESYNC) {
                        DebugPrintf(SV_LOG_ERROR, "%s\n", errMsg.str().c_str());
                        DebugPrintf(SV_LOG_ERROR, "Azure agent COM service requested restart resync. re-attempting\n");
                        
                        StartResync();
                        ++attempt;
                    }
                    else {
                        
                        if (hr == MT_E_INITIALIZATION_FAILED ||
                            hr == MT_E_REGISTRATION_FAILED)
                        {
                            AzureAuthenticationFailureAlert a(m_vmIdStr, m_deviceIdStr);
                            SendAlertToCx(SV_ALERT_CRITICAL, SV_ALERT_MODULE_GENERAL, a);
                        }

                        ThrowAzureException(errMsg.str(), hr);
                    }
                }
                else {
                    done = true;
                    csa.Attach(sa);
                    ComConversion::ToVector<unsigned char>(csa, checksums);

                    if (checksums.size() != digestLen) {

                        std::stringstream errMsg;

                        errMsg << "Attempt: " << attempt <<
                            " : Azure GetChecksums for vmId:" << m_vmIdStr 
                            << " device: " << m_deviceIdStr
                            << " jobid: " << m_resyncJobIdStr
                            << " offset:" << startingOffset << " length:" << length
                            << " returned checksums of length " << checksums.size()
                            << " , expecting " << digestLen << std::endl;

                        throw ERROR_EXCEPTION << errMsg.str();
                    }
                }
            }
            catch (AzureSpecialException &ae) {

                ++attempt;

                // retry for m_maxAttempts
                if (attempt <= m_maxAttempts){

                    DebugPrintf(SV_LOG_ERROR, "%s, re-attempt after %u seconds\n",
                        ae.what(),
                        delay);

                    // check for quit request, 
                    // if quit requested throw exception 
                    // else log the failed attempt and retry
                    // 
                    if (CDPUtil::QuitRequested(delay))
                        throw;

                    delay *= 2;
                }
                else {
                    throw ERROR_EXCEPTION << ae.what();
                }
            }
            catch (AzureCancelOpException & ce) {
                throw;
            }
            catch (AzureInvalidOpException & ie) {
                throw;
            }
            catch (ErrorException & ee) {
                ++attempt;

                if (isInitFailed())
                    throw;

                if (done)
                    throw;

                // retry for m_maxAttempts
                if (attempt <= m_maxAttempts){


                    DebugPrintf(SV_LOG_ERROR, "%s, re-attempt after %u seconds\n",
                        ee.what(),
                        m_retryInterval);

                    // check for quit request, 
                    // if quit requested throw exception 
                    // else log the failed attempt and retry
                    // 
                    if (CDPUtil::QuitRequested(m_retryInterval))
                        throw;

                }
                else {
                    throw;
                }
            }

        } // while loop

        INM_TRACE_EXIT_FUNC
    }

    void AzureAgentComImpl::ApplyLog(const std::string & resyncFilePath)
    {
        DebugPrintf(SV_LOG_DEBUG, "Entered %s args: resyncFilePath=%s\n",
            FUNCTION_NAME, resyncFilePath.c_str());

        BSTR bstrResyncFilePath = ComConversion::ToBstr(resyncFilePath);
        CComBSTR cbstr;
        cbstr.Attach(bstrResyncFilePath);

        unsigned int attempt = 1;
        bool done = false;
        unsigned int delay = m_retryInterval;

        while (!done) {

            try{

                DebugPrintf(SV_LOG_DEBUG, "Attempt : %u %s\n", attempt, FUNCTION_NAME);

                HRESULT hr = ApplyLog(m_vmName, m_vmId, m_deviceId, m_resyncJobId, cbstr);

                if (FAILED(hr)) {

                    std::string err;
                    convertAzureErrorToString(hr, err);
                    std::stringstream errMsg;
                    errMsg << "Attempt: " << attempt <<
                        " : Azure ApplyLog for vmId:" << m_vmIdStr 
                        << " device: " << m_deviceIdStr
                        << " jobid: " << m_resyncJobIdStr
                        << " file:" << resyncFilePath
                        << " failed with result " << std::hex << hr << "(" << err << ")";

                    if (HRESULT_CODE(hr) == RPC_S_SERVER_UNAVAILABLE){

                        DebugPrintf(SV_LOG_ERROR, "%s\n", errMsg.str().c_str());
                        DebugPrintf(SV_LOG_ERROR, 
                            "Azure agent COM service is not available. re-initializing\n");

                        reinitialize();
                        StartResync();
                        ++attempt;
                    }
                    else if (hr == MT_E_RESTART_RESYNC) {
                        DebugPrintf(SV_LOG_ERROR, "%s\n", errMsg.str().c_str());
                        DebugPrintf(SV_LOG_ERROR, "Azure agent COM service requested restart resync. re-attempting\n");

                        StartResync();
                        ++attempt;
                    }
                    // Apply failed due to bad file
                    else if (hr == MT_E_LOG_PARSING_ERROR) {

                        std::string err;
                        convertAzureErrorToString(hr, err);

                        throw AZURE_BADFILE_EXCEPTION << errMsg.str();
                    }
                    else{

                        if (hr == MT_E_INITIALIZATION_FAILED ||
                            hr == MT_E_REGISTRATION_FAILED)
                        {
                            AzureAuthenticationFailureAlert a(m_vmIdStr, m_deviceIdStr);
                            SendAlertToCx(SV_ALERT_CRITICAL, SV_ALERT_MODULE_GENERAL, a);
                        }

                        ThrowAzureException(errMsg.str(), hr);
                    }
                }
                else {
                    done = true;
                }
            }
            catch (AzureSpecialException &ae) {

                ++attempt;

                // retry for m_maxAttempts
                if (attempt <= m_maxAttempts){

                    DebugPrintf(SV_LOG_ERROR, "%s, re-attempt after %u seconds\n",
                        ae.what(),
                        delay);

                    // check for quit request, 
                    // if quit requested throw exception 
                    // else log the failed attempt and retry
                    // 
                    if (CDPUtil::QuitRequested(delay))
                        throw;

                    delay *= 2;
                }
                else {
                    throw ERROR_EXCEPTION << ae.what();
                }
            }
            catch (AzureCancelOpException & ce) {
                throw;
            }
            catch (AzureInvalidOpException & ie) {
                throw;
            }
            catch (AzureBadFileException & be) {
                throw;
            }
            catch (ErrorException & ee) {
                ++attempt;

                if (isInitFailed())
                    throw;

                // retry for m_maxAttempts
                if (attempt <= m_maxAttempts){


                    DebugPrintf(SV_LOG_ERROR, "%s, re-attempt after %u seconds\n",
                        ee.what(),
                        m_retryInterval);

                    // check for quit request, 
                    // if quit requested throw exception 
                    // else log the failed attempt and retry
                    // 
                    if (CDPUtil::QuitRequested(m_retryInterval))
                        throw;

                }
                else {
                    throw;
                }
            }

        } // while loop

        INM_TRACE_EXIT_FUNC
    }

    void AzureAgentComImpl::CompleteResync(bool status)
    {
        INM_TRACE_ENTER_FUNC

        unsigned int attempt = 1;
        bool done = false;
        unsigned int delay = m_retryInterval;

        while (!done) {

            try{

                DebugPrintf(SV_LOG_ALWAYS, "%s: JobId : %s, Attempt : %u\n", FUNCTION_NAME, m_resyncJobIdStr.c_str(), attempt);

                HRESULT hr = CompleteResync(m_vmName, m_vmId, m_deviceId, m_resyncJobId, status);
                if (FAILED(hr)) {

                    std::string err;
                    convertAzureErrorToString(hr, err);
                    std::stringstream errMsg;
                    errMsg << "Attempt: " << attempt <<
                        " : Azure CompleteResync for vmId:" << m_vmIdStr 
                        << " device: " << m_deviceIdStr
                        << " jobid: " << m_resyncJobIdStr
                        << " failed with result " << std::hex << hr << "(" << err << ")";

                    if (HRESULT_CODE(hr) == RPC_S_SERVER_UNAVAILABLE){

                        DebugPrintf(SV_LOG_ERROR, "%s\n", errMsg.str().c_str());
                        DebugPrintf(SV_LOG_ERROR,
                            "Azure agent COM service is not available. re-initializing\n");

                        reinitialize();
                        //StartResync();
                        ++attempt;
                    }
                    else if (hr == MT_E_RESTART_RESYNC) {
                        DebugPrintf(SV_LOG_ERROR, "%s\n", errMsg.str().c_str());
                        DebugPrintf(SV_LOG_ERROR, "Azure agent COM service requested restart resync. re-attempting\n");

                        StartResync();
                        ++attempt;
                    }
                    else{

                        if (hr == MT_E_INITIALIZATION_FAILED ||
                            hr == MT_E_REGISTRATION_FAILED)
                        {
                            AzureAuthenticationFailureAlert a(m_vmIdStr, m_deviceIdStr);
                            SendAlertToCx(SV_ALERT_CRITICAL, SV_ALERT_MODULE_GENERAL, a);
                        }

                        ThrowAzureException(errMsg.str(), hr);
                    }
                }
                else {
                    done = true;
                    DebugPrintf(SV_LOG_ALWAYS, "%s: Successfully completed resync with ProtServ for resync JobId %s with result %x\n",
                        FUNCTION_NAME, m_resyncJobIdStr.c_str(), hr);
                }
            }
            catch (AzureSpecialException &ae) {

                ++attempt;

                // retry for m_maxAttempts
                if (attempt <= m_maxAttempts){

                    DebugPrintf(SV_LOG_ERROR, "%s, re-attempt after %u seconds\n",
                        ae.what(),
                        delay);

                    // check for quit request, 
                    // if quit requested throw exception 
                    // else log the failed attempt and retry
                    // 
                    if (CDPUtil::QuitRequested(delay))
                        throw;

                    delay *= 2;
                }
                else {
                    throw ERROR_EXCEPTION << ae.what();
                }
            }
            catch (AzureCancelOpException & ce) {
                throw;
            }
            catch (AzureInvalidOpException & ie) {
                throw;
            }
            catch (ErrorException & ee) {
                ++attempt;

                if (isInitFailed())
                    throw;

                // retry for m_maxAttempts
                if (attempt <= m_maxAttempts){


                    DebugPrintf(SV_LOG_ERROR, "%s, re-attempt after %u seconds\n",
                        ee.what(),
                        m_retryInterval);

                    // check for quit request, 
                    // if quit requested throw exception 
                    // else log the failed attempt and retry
                    // 
                    if (CDPUtil::QuitRequested(m_retryInterval))
                        throw;

                }
                else {
                    throw;
                }
            }

        } // while loop

        INM_TRACE_EXIT_FUNC

    }

    InMageDifferentialLogFileInfo AzureAgentComImpl::COMConvertDiffInfo(const  AzureDiffLogInfo &rhs)
    {
        InMageDifferentialLogFileInfo rv;
        rv.PrevEndTime = rhs.PrevEndTime;
        rv.PrevEndTimeSequenceNumber = rhs.PrevEndTimeSequenceNumber;
        rv.PrevContinuationId = rhs.PrevContinuationId;
        rv.EndTime = rhs.EndTime;
        rv.EndTimeSequenceNumber = rhs.EndTimeSequenceNumber;
        rv.ContinuationId = rhs.ContinuationId;
        rv.FileSize = rhs.FileSize;
        rv.FilePath = ComConversion::ToBstr(rhs.FilePath);

        return rv;
    }

    void AzureAgentComImpl::UploadLogs(const std::vector<AzureDiffLogInfo> & azureDiffInfos,
        bool containsRecoverablePoints)
    {
        DebugPrintf(SV_LOG_DEBUG, "Entered %s args: containsRecoverablePoints=%s\n",
            FUNCTION_NAME, (containsRecoverablePoints)?"true":"false");

        GUID typeLibGuid = LIBID_MTReplicationProviderLib;
        LPSAFEARRAY psa = NULL;

        CComPtr<IRecordInfo> pRecordInfo = ComConversion::GetRecordInfo<InMageDifferentialLogFileInfo>(typeLibGuid);
        ComConversion::SafeArrayUDT<InMageDifferentialLogFileInfo> saUDT(pRecordInfo, azureDiffInfos.size());

        ComConversion::ToSafeArray<AzureDiffLogInfo, InMageDifferentialLogFileInfo>(azureDiffInfos, saUDT, &AzureAgentInterface::AzureAgentComImpl::COMConvertDiffInfo);


        unsigned int attempt = 1;
        bool done = false;
        unsigned int delay = m_retryInterval;

        while (!done) {

            try{

                DebugPrintf(SV_LOG_DEBUG, "Attempt : %u %s\n", attempt, FUNCTION_NAME);

                HRESULT hr = UploadLogs(m_vmName, m_vmId, m_deviceId, *saUDT, containsRecoverablePoints);
                if (FAILED(hr)) {

                    std::string err;
                    convertAzureErrorToString(hr, err);
                    std::stringstream errMsg;

                    errMsg << "Attempt: " << attempt <<
                        " : Azure UploadLogs for vmId: " << m_vmIdStr << " device: " << m_deviceIdStr
                        //<< " diffFileNames:" << diffFileNames
                        << " containsRecoverablePoints " << ((containsRecoverablePoints) ? "true" : "false")
                        << " failed with result " << std::hex << hr << " (" << err << ")";


                    if (HRESULT_CODE(hr) == RPC_S_SERVER_UNAVAILABLE) {

                        DebugPrintf(SV_LOG_ERROR, "%s\n", errMsg.str().c_str());
                        DebugPrintf(SV_LOG_ERROR,
                            "Azure agent COM service is not available. re-initializing\n");

                        reinitialize();
                        ++attempt;
                    }

                    //else if (CO_E_OBJNOTCONNECTED == hr) {

                    //	DebugPrintf(SV_LOG_ERROR, "%s\n", errMsg.str().c_str());
                    //	DebugPrintf(SV_LOG_ERROR,
                    //		"Azure agent COM service is not available. re-initializing\n");

                    //	reinitialize();
                    //	++attempt;
                    //}

                    //upload failed due to missing file on MT
                    else if ((HRESULT_CODE(hr) == ERROR_FILE_NOT_FOUND) || (HRESULT_CODE(hr) == ERROR_PATH_NOT_FOUND)){

                        throw AZURE_MISSINGFILE_EXCEPTION << errMsg.str();
                    }
                    else{

                        if (hr == MT_E_INITIALIZATION_FAILED ||
                            hr == MT_E_REGISTRATION_FAILED)
                        {
                            AzureAuthenticationFailureAlert a(m_vmIdStr, m_deviceIdStr);
                            SendAlertToCx(SV_ALERT_CRITICAL, SV_ALERT_MODULE_GENERAL, a);
                        }

                        ThrowAzureException(errMsg.str(), hr);
                    }
                }
                else {
                    done = true;
                }
            }
            catch (AzureSpecialException &ae) {

                ++attempt;

                // retry for m_maxAttempts
                if (attempt <= m_maxAttempts){

                    DebugPrintf(SV_LOG_ERROR, "%s, re-attempt after %u seconds\n",
                        ae.what(),
                        delay);

                    // check for quit request, 
                    // if quit requested throw exception 
                    // else log the failed attempt and retry
                    // 
                    if (CDPUtil::QuitRequested(delay))
                        throw;

                    delay *= 2;
                }
                else {
                    throw ERROR_EXCEPTION << ae.what();
                }
            }
            catch (AzureMissingFileException & me)
            {
                throw;
            }
            catch (AzureResyncRequiredException & re)
            {
                throw;
            }
            catch (AzureCancelOpException & ce) {
                throw;
            }
            catch (AzureInvalidOpException & ie) {
                throw;
            }
            catch (AzureClientResyncRequiredOpException & cre){
                throw;
            }
            catch (ErrorException & ee) {
                ++attempt;

                if (isInitFailed())
                    throw;

                // retry for m_maxAttempts
                if (attempt <= m_maxAttempts){


                    DebugPrintf(SV_LOG_ERROR, "%s, re-attempt after %u seconds\n",
                        ee.what(),
                        m_retryInterval);

                    // check for quit request, 
                    // if quit requested throw exception 
                    // else log the failed attempt and retry
                    // 
                    if (CDPUtil::QuitRequested(m_retryInterval))
                        throw;

                }
                else {
                    throw;
                }
            }

        } // while loop

        INM_TRACE_EXIT_FUNC

    }

    void AzureAgentComImpl::UploadEvents(const std::vector<std::string> &logFilePaths)
    {
        INM_TRACE_ENTER_FUNC;

        BOOST_ASSERT(!logFilePaths.empty());

        LPSAFEARRAY psa = NULL;
        CComSafeArray<BSTR> csa;
        ComConversion::ToSafeArray(logFilePaths, csa);

        HRESULT hr = UploadEvents(*csa.GetSafeArrayPtr());
        if (FAILED(hr))
        {
            std::string err;
            convertAzureErrorToString(hr, err);
            std::stringstream errMsg;

            errMsg << "Azure UploadEvents failed with result " << std::hex << hr << " - " << err;
            DebugPrintf(SV_LOG_ERROR, "%s.\n", errMsg.str().c_str());

            THROW_ERROR_EXCEPTION(errMsg.str());
        }

        // TODO-SanKumar-1703: Do we need any retries in case of failure? The basic design of EvtCollForw
        // is to be at low priority. So currently the retries are made by the EvtCollForw at the next
        // try.

        INM_TRACE_EXIT_FUNC;
    }

    std::list<HRESULT> AzureAgentComImpl::CheckMarsAgentHealth()
    {
        INM_TRACE_ENTER_FUNC

        std::list<HRESULT> hrs;
        HRESULT hr = CheckAgentHealth();

        if (hr != S_OK)
        {
            hrs.push_back(hr);
        }

        INM_TRACE_EXIT_FUNC

        return hrs;
    }

    HRESULT AzureAgentComImpl::StartResync(
        /* [in] */ BSTR VmName,
        /* [in] */ BSTR VmId,
        /* [in] */ BSTR DiskId,
        /* [in] */ unsigned long long DiskSize,
        /* [in] */ unsigned long long BlockSize,
        /* [in] */ BSTR JobId,
        /* [in, optional, defaultvalue(TRUE)] */ bool isSignatureCalculationRequired)
    {
        boost::shared_lock< boost::shared_mutex > lock(m_mutex);
        
        if (!m_azureAgentProvider)
            return HRESULT_FROM_WIN32(RPC_S_SERVER_UNAVAILABLE);

        if (!isSignatureCalculationRequired)
        {
            return m_azureAgentProvider->StartResyncWithoutSignatureCalculations(VmName, VmId, DiskId, DiskSize, BlockSize, JobId);
        } 

        return m_azureAgentProvider->StartResync(VmName,VmId, DiskId, DiskSize, BlockSize, JobId);
    }

    HRESULT AzureAgentComImpl::GetChecksum(
        /* [in] */ BSTR VmName,
        /* [in] */ BSTR VmId,
        /* [in] */ BSTR DiskId,
        /* [in] */ BSTR JobId,
        /* [in] */ unsigned long long Offset,
        /* [in] */ unsigned long long Length,
        /* [retval][out] */ SAFEARRAY * *Checksums)
    {
        boost::shared_lock< boost::shared_mutex > lock(m_mutex);
        // do work here, without anyone having exclusive access

        if (!m_azureAgentProvider)
            return HRESULT_FROM_WIN32(RPC_S_SERVER_UNAVAILABLE);

        return m_azureAgentProvider->GetChecksum(VmName, VmId, DiskId, JobId, Offset, Length, Checksums);
    }

    HRESULT AzureAgentComImpl::ApplyLog(
        /* [in] */ BSTR VmName,
        /* [in] */ BSTR VmId,
        /* [in] */ BSTR DiskId,
        /* [in] */ BSTR JobId,
        /* [in] */ BSTR ResyncLogFilePath)
    {
        boost::shared_lock< boost::shared_mutex > lock(m_mutex);
        // do work here, without anyone having exclusive access

        if (!m_azureAgentProvider)
            return HRESULT_FROM_WIN32(RPC_S_SERVER_UNAVAILABLE);

        return m_azureAgentProvider->ApplyLog(VmName, VmId, DiskId, JobId, ResyncLogFilePath);
    }


    HRESULT AzureAgentComImpl::CompleteResync(
        /* [in] */ BSTR VmName,
        /* [in] */ BSTR VmId,
        /* [in] */ BSTR DiskId,
        /* [in] */ BSTR JobId,
        /* [in] */ boolean IsSuccess)
    {
        boost::shared_lock< boost::shared_mutex > lock(m_mutex);
        // do work here, without anyone having exclusive access

        if (!m_azureAgentProvider)
            return HRESULT_FROM_WIN32(RPC_S_SERVER_UNAVAILABLE);

        return m_azureAgentProvider->CompleteResync(VmName, VmId, DiskId, JobId, IsSuccess);
    }

    HRESULT AzureAgentComImpl::UploadLogs(
        /* [in] */ BSTR VmName,
        /* [in] */ BSTR VmId,
        /* [in] */ BSTR DiskId,
        /* [in] */ SAFEARRAY * DiffLogFilePaths,
        /* [in] */ boolean IsRecoverable)
    {
        boost::shared_lock< boost::shared_mutex > lock(m_mutex);
        // do work here, without anyone having exclusive access

        if (!m_azureAgentProvider)
            return HRESULT_FROM_WIN32(RPC_S_SERVER_UNAVAILABLE);

        return m_azureAgentProvider->UploadLogs(VmName, VmId, DiskId, DiffLogFilePaths, IsRecoverable);
    }

    HRESULT AzureAgentComImpl::UploadEvents(
        /* [in] */ SAFEARRAY *LogFilePaths)
    {
        boost::shared_lock< boost::shared_mutex > lock(m_mutex);
        // do work here, without anyone having exclusive access

        if (!m_azureAgentProvider)
            return HRESULT_FROM_WIN32(RPC_S_SERVER_UNAVAILABLE);

        return m_azureAgentProvider->UploadEvents(LogFilePaths);
    }

    HRESULT AzureAgentComImpl::CheckAgentHealth()
    {
        boost::shared_lock< boost::shared_mutex > lock(m_mutex);

        if (!m_azureAgentProvider)
            return HRESULT_FROM_WIN32(RPC_S_SERVER_UNAVAILABLE);

        return m_azureAgentProvider->CheckAgentHealth();
    }

    void AzureAgentComImpl::initializeCOM()
    {
        INM_TRACE_ENTER_FUNC

        HRESULT hres = CoInitializeEx(0, COINIT_MULTITHREADED);
        if (FAILED(hres)) {
            std::string err;
            convertAzureErrorToString(hres, err);
            throw ERROR_EXCEPTION << " initializeCOM COM library (CoInitializeEx)  failed with result "
                    << std::hex << hres << "(" << err << ")";
        }

        
        hres = CoCreateInstance(CLSID_MTReplicationEndpointProvider,
            0, CLSCTX_LOCAL_SERVER, IID_IMTReplicationEndpointProvider,
            (LPVOID *)&m_azureAgentProvider);

        if (FAILED(hres) || !m_azureAgentProvider) {

            if (m_azureAgentProvider) {
                m_azureAgentProvider->Release();
                m_azureAgentProvider = NULL;
            }

            CoUninitialize();
            std::string err;
            convertAzureErrorToString(hres, err);
            throw ERROR_EXCEPTION << "create azure agent interface (CoCreateInstance)  failed with result "
                    << std::hex << hres << "(" << err << ")";
        }

        INM_TRACE_EXIT_FUNC
    }

    void AzureAgentComImpl::initializeCOMWithRetries()
    {
        INM_TRACE_ENTER_FUNC

        unsigned int attempt = 1;
        bool initialized = false;

        while (!initialized) {

            try
            {
                initializeCOM();
                initialized = true;
            }
            catch (ErrorException & ee) {

                // retry for m_maxAttempts
                if (attempt <= m_maxAttempts){

                    //check for quit request, 
                    //if quit requested throw exception 
                    //else log the failed attempt and retry

                    DebugPrintf(SV_LOG_ERROR, "Attempt: %u : %s, re-attempt after %u seconds", 
                        attempt, 
                        ee.what(),
                        m_retryInterval);

                    ++attempt;
                    if (CDPUtil::QuitRequested(m_retryInterval))
                        throw;


                }
                else {
                    throw;
                }
            }
        } 

        INM_TRACE_EXIT_FUNC
    }

    void AzureAgentComImpl::uninitializeCOM()
    {
        INM_TRACE_ENTER_FUNC

        if (m_azureAgentProvider) {
            m_azureAgentProvider->Release();
            m_azureAgentProvider = NULL;
        }

        CoUninitialize();

        INM_TRACE_EXIT_FUNC
    }

    /// \brief converts error code to error string
    void AzureAgentComImpl::convertAzureErrorToString(unsigned long err, std::string& errStr)
    {
        std::stringstream str;
        AzureErrorCategory errorType = AzureErrorType(err);
        if (errorType == AZURE_RETRYABLE_ERROR ||
            errorType == AZURE_RETRYABLE_ERROR_EXPONENTIAL_DELAY ||
            errorType == AZURE_NONRETRYABLE_ERROR_CANCELOP ||
            errorType == AZURE_NONRETRYABLE_ERROR_INVALIDOP ||
            errorType == AZURE_NONRETRYABLE_ERROR_RESYNCREQUIRED){

            errStr = AzureErrorCodeToString(err);
        }
        else{
            convertErrorToString(err, errStr);
        }
    }
}