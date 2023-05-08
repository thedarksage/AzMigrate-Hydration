/*
*
* File       : azureblockblobstream.cpp
*
* Description:
*/
#include <string>
#include <stdexcept>

#include <boost/thread.hpp>

#include "azureblockblobstream.h"
#include "error.h"
#include "portablehelpers.h"
#include "inmsafeint.h"

const uint32_t MaxBlockWriteSize = 4194304000;

AzureBlockBlobStream::AzureBlockBlobStream(std::string& blobContainerSasUri,
    const uint32_t& timeout,
    const uint64_t azureBlockBlobParallelUploadSize,
    const uint64_t azureBlockBlobMaxWriteSize,
    const uint32_t maxParallelUploadThreads,
    const bool bEnableChecksums) :
    AzureBlobStream(blobContainerSasUri, timeout, bEnableChecksums),
    m_azureBlockBlobParallelUploadSize(azureBlockBlobParallelUploadSize),
    m_azureBlockBlobMaxWriteSize(azureBlockBlobMaxWriteSize),
    m_maxParallelUploadThreads(maxParallelUploadThreads)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    if (azureBlockBlobMaxWriteSize > MaxBlockWriteSize)
    {
        std::stringstream ss;
        ss << "azureBlockBlobMaxWriteSize=" << azureBlockBlobMaxWriteSize << "is > Maximum block write size limit " << MaxBlockWriteSize;
        throw INMAGE_EX(ss.str().c_str());
    }

    INM_SAFE_ARITHMETIC(m_NumberOfParallelUploads = InmSafeInt<unsigned long>::Type(azureBlockBlobMaxWriteSize) / azureBlockBlobParallelUploadSize,
        INMAGE_EX(azureBlockBlobMaxWriteSize)(azureBlockBlobParallelUploadSize));

    Init();

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

AzureBlockBlobStream::~AzureBlockBlobStream()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

int AzureBlockBlobStream::Write(const char* sData, unsigned long int uliDataLen, unsigned long int uliOffset, const std::string& sasUri)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    
    blob_size_t nLength = HttpRestUtil::RoundToProperBlobSize(uliDataLen);

    if (uliOffset)
    {
        DebugPrintf(SV_LOG_ERROR,
            "%s: writing at offset %lu is not supported\n",
            FUNCTION_NAME, uliOffset);

        return SV_FAILURE;
    }

    std::string blobName;
    size_t qpos = sasUri.find('?');
    blobName = sasUri.substr(0, qpos);
    
    m_lastError = 0;
    m_responseData.clear();
    m_ptrBlockBlob.reset();

    assert(m_vPtrHttpClient.size() == m_NumberOfParallelUploads);

    try
    {
        m_ptrBlockBlob = boost::make_shared<CloudBlockBlob>(sasUri,
            m_vPtrHttpClient,
            m_NumberOfParallelUploads,
            m_azureBlockBlobParallelUploadSize,
            m_azureBlockBlobMaxWriteSize,
            m_maxParallelUploadThreads);
    }
    catch (const std::exception& e)
    {
        DebugPrintf(SV_LOG_ERROR,
            "%s: Failed to create block blob for log file %s with error %s.\n",
            FUNCTION_NAME, blobName.c_str(), e.what());

        return SV_FAILURE;
    }

    if (m_use_proxy)
        m_ptrBlockBlob->SetHttpProxy(m_proxy);

    m_ptrBlockBlob->SetTimeout(m_timeout);

    if (m_bEnableChecksums)
    {
        if (m_pSha256CtxForChecksums)
        {
            m_pSha256CtxForChecksums.reset();
        }
        m_pSha256CtxForChecksums = boost::make_shared<SHA256_CTX>();
        SHA256_Init(m_pSha256CtxForChecksums.get());
    }

    if (!m_ptrBlockBlob->Write(sData, uliDataLen))
    {
        DebugPrintf(SV_LOG_ERROR, "%s: failed to write %lu bytes to block blob %s\n",
            FUNCTION_NAME, uliDataLen, blobName.c_str());

        m_lastError = m_ptrBlockBlob->GetLastError();
        m_responseData = m_ptrBlockBlob->GetLastHttpResponseData();

        return SV_FAILURE;
    }

    // Keep updating m_pSha256CtxForChecksums on each successful write to same blob
    if (m_bEnableChecksums && m_pSha256CtxForChecksums)
    {
        SHA256_Update(m_pSha256CtxForChecksums.get(), sData, uliDataLen);
    }
    
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return SV_SUCCESS;
}

int AzureBlockBlobStream::Write(const char* sData, unsigned long int uliDataLen, const std::string& sDestination, bool bMoreData, COMPRESS_MODE compressMode, bool createDirs, bool bIsFull)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "%s: sDestination %s uliDataLen %lu moreData %d createDirs %d\n",
        FUNCTION_NAME, sDestination.c_str(), uliDataLen, bMoreData, createDirs);

    if (m_continuationBlobName == sDestination)
    {
        DebugPrintf(SV_LOG_DEBUG,
            "%s: Continuation write for destination %s.\n",
            FUNCTION_NAME, sDestination.c_str());
    }

    const blob_size_t writeSize = HttpRestUtil::RoundToProperBlobSize(uliDataLen);

    if (m_continuationBlobName != sDestination)
    {
        std::string endTimeStampSeqNumber;

        if (!GetEndTimeStampSeqNumForSorting(sDestination, endTimeStampSeqNumber))
        {
            DebugPrintf(SV_LOG_ERROR, "%s: Failed to get the end timestamp for file %s\n", FUNCTION_NAME, sDestination.c_str());
            return SV_FAILURE;
        }

        std::string blobUri, strBlobName;
        std::string::size_type dirPos = sDestination.rfind('/');
        if (dirPos == std::string::npos)
        {
            DebugPrintf(SV_LOG_DEBUG, "%s: No directory name found for file %s\n", FUNCTION_NAME, sDestination.c_str());
        }
        else
        {
            strBlobName = sDestination.substr(0, dirPos + 1);
        }

        strBlobName += endTimeStampSeqNumber;
        blobUri = GetRestUriFromSasUri(m_blobContainerSasUri, strBlobName);

        m_ptrBlockBlob.reset();
        m_responseData.clear();
        m_lastError = 0;

        assert(m_vPtrHttpClient.size() == m_NumberOfParallelUploads);

        try
        {
            m_ptrBlockBlob = boost::make_shared<CloudBlockBlob>(blobUri,
                m_vPtrHttpClient,
                m_NumberOfParallelUploads,
                m_azureBlockBlobParallelUploadSize,
                m_azureBlockBlobMaxWriteSize,
                m_maxParallelUploadThreads);
        }
        catch (const std::exception& e)
        {
            DebugPrintf(SV_LOG_ERROR,
                "%s: Failed to create block blob for log file %s with error %s.\n",
                FUNCTION_NAME, sDestination.c_str(), e.what());

            return SV_FAILURE;
        }

        if (m_use_proxy)
            m_ptrBlockBlob->SetHttpProxy(m_proxy);

        m_ptrBlockBlob->SetTimeout(m_timeout);

        m_continuationBlobName = sDestination;
        m_dataSize = uliDataLen;
        m_blobCreateTime = GetTimeInMilliSecSinceEpoch1970();
        
        if (m_bEnableChecksums)
        {
            if (m_pSha256CtxForChecksums)
            {
                m_pSha256CtxForChecksums.reset();
            }
            m_pSha256CtxForChecksums = boost::make_shared<SHA256_CTX>();
            SHA256_Init(m_pSha256CtxForChecksums.get());
        }
    }
    else
    {
        assert(m_vPtrHttpClient.size() == m_NumberOfParallelUploads);

        if (!m_ptrBlockBlob)
        {
            DebugPrintf(SV_LOG_ERROR,
                "%s: block blob object is not found for continuation write on log file %s\n",
                FUNCTION_NAME, sDestination.c_str());

            return SV_FAILURE;
        }

        m_dataSize += uliDataLen;
    }

    if (!m_ptrBlockBlob->Write(sData, uliDataLen))
    {
        DebugPrintf(SV_LOG_ERROR, "%s: failed to write %lu bytes to block blob %s\n",
            FUNCTION_NAME, uliDataLen, sDestination.c_str());

        m_lastError = m_ptrBlockBlob->GetLastError();
        m_responseData = m_ptrBlockBlob->GetLastHttpResponseData();

        return SV_FAILURE;
    }

    // Keep updating m_pSha256CtxForChecksums on each successful write to same blob
    if (m_bEnableChecksums && m_pSha256CtxForChecksums)
    {   
        SHA256_Update(m_pSha256CtxForChecksums.get(), sData, uliDataLen);
    }

    if (!bMoreData)
    {
        DebugPrintf(SV_LOG_DEBUG, "%s: Total bytes written to the blob %lu.\n", FUNCTION_NAME, uliDataLen);

        DebugPrintf(SV_LOG_DEBUG, "%s: Actual data size %lu.\n", FUNCTION_NAME, m_dataSize);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return SV_SUCCESS;
}

int AzureBlockBlobStream::DeleteFile(std::string const& names, int mode)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "%s: names %s \n", FUNCTION_NAME, names.c_str());

    if (!m_ptrBlockBlob->ClearBlockIds())
    {
        DebugPrintf(SV_LOG_ERROR, "%s: Failed to delete block blob\n", FUNCTION_NAME);
        return SV_FAILURE;
    }
    
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return SV_SUCCESS;
}


int AzureBlockBlobStream::SetMetadata(const metadata_t &metadata)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    int status = SV_SUCCESS;

    assert(m_vPtrPageBlob.size() == m_NumberOfParallelUploads);

    if (!m_ptrBlockBlob)
    {
        DebugPrintf(SV_LOG_ERROR,
            "%s: block blob object is not found for metadata update of log file %s\n",
            FUNCTION_NAME, m_continuationBlobName.c_str());

        return SV_FAILURE;
    }

    if (!m_ptrBlockBlob->SetMetadata(metadata))
    {
        DebugPrintf(SV_LOG_ERROR, "%s: Failed to set the metadata for %s\n", FUNCTION_NAME, m_continuationBlobName.c_str());
        m_lastError = m_ptrBlockBlob->GetLastError();
        m_responseData = m_ptrBlockBlob->GetLastHttpResponseData();
        status = SV_FAILURE;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}