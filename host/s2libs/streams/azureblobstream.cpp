/*
*
* File       : azureblobstream.cpp
*
* Description:
*/
#include <string>
#include <stdexcept>
#include <iomanip>

#include <boost/thread.hpp>

#include "azureblobstream.h"
#include "error.h"
#include "portablehelpers.h"

#include "diffsortcriterion.h"
#include "dataprotectionexception.h"

#include "securityutils.h"

#include "inmsafeint.h"

const char DIFF_FILE_NAME[] = "DiffFileName";
const char DIFF_FILE_SIZE[] = "DiffFileSize";
const char DIFF_FILE_STATUS[] = "DiffFileStatus";
const char DIFF_FILE_STATUS_COMPLETED[] = "Completed";

const char DIFF_FILE_PREV_IO_TIMESTAMP[] = "PreviousIoTimestamp";
const char DIFF_FILE_PREV_IO_SEQUENCE_NUM[] = "PreviousIoSequenceNumber";
const char DIFF_FILE_PREV_IO_CONTINUATION_ID[] = "PreviousContinuationId";

const char DIFF_FILE_END_IO_TIMESTAMP[] = "EndIoTimestamp";
const char DIFF_FILE_END_IO_SEQUENCE_NUM[] = "EndIoSequenceNumber";
const char DIFF_FILE_END_IO_CONTINUATION_ID[] = "EndContinuationId";

const char DIFF_FILE_TYPE[] = "DiffFileType";
const char DIFF_FILE_EXTENSION[] = "DiffFileExtension";
const char DIFF_FILE_AGENT_TIMESTAMP[] = "AgentTimestamp";
const char DIFF_FILE_UPLOAD_END_TIMESTAMP[] = "AgentUploadEndTime";
const char DIFF_FILE_UPLOAD_START_TIMESTAMP[] = "AgentUploadStartTime";

const char DIFF_FILE_SHA256CHECKSUM[] = "DiffFileSHA256Checksum";

#define DEFAULT_NUMBER_OF_PARALLEL_UPLOADS 2


/// \brief Get the end timestamp sequence number for file sorting
///
/// \returns true on success, false on failure
bool AzureBlobStream::GetEndTimeStampSeqNumForSorting(const std::string &filename,
    std::string &endTimeStampSeqNum)
{
    const std::string FIELD_SEPARATOR("_");
    std::string writeOrderMode;

    try {
        DiffSortCriterion fileNameParser(filename.c_str(), std::string());
        std::stringstream ssEndTimeStampSeqNumber;
        ssEndTimeStampSeqNumber << std::setfill('0') << std::setw(20) << fileNameParser.EndTime();
        ssEndTimeStampSeqNumber << FIELD_SEPARATOR;
        ssEndTimeStampSeqNumber << std::setfill('0') << std::setw(20) << fileNameParser.EndTimeSequenceNumber();
        ssEndTimeStampSeqNumber << FIELD_SEPARATOR;
        ssEndTimeStampSeqNumber << fileNameParser.Mode();
        ssEndTimeStampSeqNumber << (fileNameParser.IsContinuationEnd() ? 'E' : 'C');
        ssEndTimeStampSeqNumber << std::setfill('0') << std::setw(10) << (unsigned int)fileNameParser.ContinuationId();
        ssEndTimeStampSeqNumber << FIELD_SEPARATOR;
        ssEndTimeStampSeqNumber << std::setfill('0') << std::setw(20) << fileNameParser.CxTime();

        endTimeStampSeqNum = ssEndTimeStampSeqNumber.str();
    }
    catch (DataProtectionException &e)
    {
        DebugPrintf(SV_LOG_ERROR,
            "%s : failed to parse file name %s. caught exception %s\n",
            FUNCTION_NAME,
            filename.c_str(),
            e.what());
        return false;
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR,
            "%s : failed to parse file name %s. caught unknown exception\n",
            FUNCTION_NAME,
            filename.c_str());
        return false;
    }

    return true;
}


AzureBlobStream::AzureBlobStream(std::string& blobContainerSasUri,
    const uint32_t& timeout,
    const bool bEnableChecksums) :
    m_blobContainerSasUri(blobContainerSasUri),
    m_timeout(timeout),
    m_bEnableChecksums(bEnableChecksums),
    m_dataSize(0),
    m_use_proxy(false),
    m_NumberOfParallelUploads(DEFAULT_NUMBER_OF_PARALLEL_UPLOADS)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void AzureBlobStream::Init()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    if (!m_NumberOfParallelUploads)
    {
        m_NumberOfParallelUploads = DEFAULT_NUMBER_OF_PARALLEL_UPLOADS;
    }

    for (uint8_t i = 0; i < m_NumberOfParallelUploads; i++)
    {
        m_vPtrHttpClient.push_back(boost::shared_ptr<HttpClient>(new HttpClient()));
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}


AzureBlobStream::~AzureBlobStream()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}


int AzureBlobStream::Write(const std::string& localFile, const std::string& targetFile, COMPRESS_MODE compressMode, const std::string& renameFile, std::string const& finalPaths, bool createDirs)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "%s: localFile %s targetFile %s renameFile %s finalPaths %s createDirs %d\n",
        FUNCTION_NAME, localFile.c_str(), targetFile.c_str(), renameFile.c_str(), finalPaths.c_str(), createDirs);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    throw std::runtime_error("not implemented");
}


int AzureBlobStream::Write(const std::string& localFile, const std::string& targetFile, COMPRESS_MODE compressMode, bool createDirs)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "%s: localFile %s targetFile %s createDirs %d\n",
        FUNCTION_NAME, localFile.c_str(), targetFile.c_str(), createDirs);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    throw std::runtime_error("not implemented");
}


int AzureBlobStream::Open(STREAM_MODE Mode)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return SV_SUCCESS;
}


int AzureBlobStream::Close()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return SV_SUCCESS;
}


int AzureBlobStream::Abort(char const* pData)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return SV_SUCCESS;
}


int AzureBlobStream::DeleteFile(std::string const& names, std::string const& fileSpec, int mode)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "%s: names %s fileSpec %s\n", FUNCTION_NAME, names.c_str(), fileSpec.c_str());
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return SV_SUCCESS;
}


int AzureBlobStream::DeleteFile(std::string const& names, int mode)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "%s: names %s \n", FUNCTION_NAME, names.c_str());
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return SV_SUCCESS;
}


int AzureBlobStream::Rename(const std::string& sOldFileName, const std::string& sNewFileName, COMPRESS_MODE compressMode, std::string const& finalPaths)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "oldFile %s newFile %s finalPaths %s\n",
        sOldFileName.c_str(), sNewFileName.c_str(), finalPaths.c_str());

    if (m_continuationBlobName != sOldFileName)
    {
        DebugPrintf(SV_LOG_ERROR,"%s: Received rename for file %s while current file is %s\n",
            FUNCTION_NAME, sOldFileName.c_str(), m_continuationBlobName.c_str());
        return SV_FAILURE;
    }

    int status = SV_SUCCESS;
    AzureStorageRest::metadata_t metadata;

    try {
        // find file type diff | tso | tag
        // find the previous timestap position _P
        std::string::size_type typeEndPos = sOldFileName.rfind("_P");
        if (typeEndPos == std::string::npos)
        {
            DebugPrintf(SV_LOG_ERROR, "%s : Previous end timestamp not found for file %s\n",
                FUNCTION_NAME, sOldFileName.c_str());
            return SV_FAILURE;
        }

        // find the first '_' from "_P" in reverse
        std::string::size_type typeStartPos = sOldFileName.rfind("_", typeEndPos - 1);
        if (typeStartPos == std::string::npos)
        {
            DebugPrintf(SV_LOG_ERROR, "%s : prefix not found for file %s\n",
                FUNCTION_NAME, sOldFileName.c_str());
            return SV_FAILURE;
        }

        // remove leading '_'
        typeStartPos++;
        
        std::string fileType;
        fileType = sOldFileName.substr(typeStartPos, typeEndPos - typeStartPos);

        metadata[DIFF_FILE_NAME] = sOldFileName;
        metadata[DIFF_FILE_STATUS] = DIFF_FILE_STATUS_COMPLETED;
        metadata[DIFF_FILE_SIZE] = boost::lexical_cast<std::string>(m_dataSize);
        metadata[DIFF_FILE_TYPE] = fileType;

        DiffSortCriterion fileNameParser(sOldFileName.c_str(), std::string());
        metadata[DIFF_FILE_AGENT_TIMESTAMP] = boost::lexical_cast<std::string>(fileNameParser.CxTime());

        std::string endContId(fileNameParser.Mode());
        endContId += (fileNameParser.IsContinuationEnd() ? "E" : "C");
        endContId += boost::lexical_cast<std::string>((unsigned int)fileNameParser.ContinuationId());

        metadata[DIFF_FILE_END_IO_CONTINUATION_ID] = endContId;
        metadata[DIFF_FILE_END_IO_SEQUENCE_NUM] = boost::lexical_cast<std::string>(fileNameParser.EndTimeSequenceNumber());
        metadata[DIFF_FILE_END_IO_TIMESTAMP] = boost::lexical_cast<std::string>(fileNameParser.EndTime());

        metadata[DIFF_FILE_PREV_IO_CONTINUATION_ID] = boost::lexical_cast<std::string>((unsigned int)fileNameParser.PreviousContinuationId());
        metadata[DIFF_FILE_PREV_IO_SEQUENCE_NUM] = boost::lexical_cast<std::string>(fileNameParser.PreviousEndTimeSequenceNumber());
        metadata[DIFF_FILE_PREV_IO_TIMESTAMP] = boost::lexical_cast<std::string>(fileNameParser.PreviousEndTime());

        metadata[DIFF_FILE_UPLOAD_END_TIMESTAMP] = boost::lexical_cast<std::string>(GetTimeInMilliSecSinceEpoch1970());
        metadata[DIFF_FILE_UPLOAD_START_TIMESTAMP] = boost::lexical_cast<std::string> (m_blobCreateTime);

        if (m_bEnableChecksums && m_pSha256CtxForChecksums)
        {
            std::vector<unsigned char> mac(SHA256_DIGEST_LENGTH, '\0');
            SHA256_Final(&mac[0], m_pSha256CtxForChecksums.get());


            metadata[DIFF_FILE_SHA256CHECKSUM] = securitylib::base64Encode((const char*)&mac[0], SHA256_DIGEST_LENGTH);
        }

    }
    catch (DataProtectionException &e)
    {
        DebugPrintf(SV_LOG_ERROR,
            "%s : failed to parse file name %s. caught exception %s\n",
            FUNCTION_NAME, sOldFileName.c_str(), e.what());
        return SV_FAILURE;
    }
    catch (boost::bad_lexical_cast &le)
    {
        DebugPrintf(SV_LOG_ERROR,
            "%s : failed to parse file name %s. caught exception %s\n",
            FUNCTION_NAME, sOldFileName.c_str(), le.what());
        return SV_FAILURE;
    }
    catch (...)
    {
        DebugPrintf(SV_LOG_ERROR,
            "%s : failed to parse file name %s. caught unknown exception\n",
            FUNCTION_NAME, sOldFileName.c_str());
        return SV_FAILURE;
    }

    status = SetMetadata(metadata);

    m_dataSize = 0;
    m_continuationBlobName.clear();

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}


int AzureBlobStream::DeleteFiles(const ListString& DeleteFileList)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return SV_SUCCESS;
}


int AzureBlobStream::Write(const void*, const unsigned long int)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    throw std::runtime_error("not implemented");
}


bool AzureBlobStream::NeedToDeletePreviousFile(void)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return false;
}


void AzureBlobStream::SetNeedToDeletePreviousFile(bool bneedtodeleteprevfile)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}


int AzureBlobStream::Read(void*, const unsigned long int)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
        throw std::runtime_error("not implemented");
}


void AzureBlobStream::SetSizeOfStream(unsigned long int uliSize)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}


bool AzureBlobStream::heartbeat(bool forceSend)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return true;
}


std::string AzureBlobStream::GetRestUriFromSasUri(const std::string& SasUri,
    const std::string& rest_suffix)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    std::string URL, params, restUri;

    int qpos = SasUri.find('?');

    URL = SasUri.substr(0, qpos);

    params = SasUri.substr(qpos + 1);

    if (rest_suffix.find('?') != 0)
    {
        restUri = URL + "/" + rest_suffix + "?" + params;
    }
    else
    {
        restUri = URL + "/" + rest_suffix + "&" + params;
    }
    
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return restUri;
}

void AzureBlobStream::SetHttpProxy(const std::string& address,
    const std::string& port,
    const std::string& bypasslist)
{
    m_use_proxy = true;
    m_proxy.m_address = address;
    m_proxy.m_port = port;
    m_proxy.m_bypasslist = bypasslist;
    return;
}

bool AzureBlobStream::UpdateTransportSettings(const std::string& sasUri)
{
    m_blobContainerSasUri = sasUri;
    return true;
}

void AzureBlobStream::SetTransportTimeout(SV_ULONGLONG timeout)
{
    m_timeout = timeout;
}
