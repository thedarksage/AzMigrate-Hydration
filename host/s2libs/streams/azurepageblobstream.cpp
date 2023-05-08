/*
*
* File       : azurepageblobstream.cpp
*
* Description:
*/
#include <string>
#include <stdexcept>

#include <boost/thread.hpp>

#include "azurepageblobstream.h"
#include "error.h"
#include "portablehelpers.h"
#include "inmsafeint.h"

const blob_size_t MAX_BLOB_SIZE_FOR_AGENT = 128 * 1024 * 1024; /// 128 MB

const blob_size_t MAX_WRITE_SIZE_FOR_AZURE_PAGE_BLOB = 4 * 1024 * 1024; /// 4 MB

const blob_size_t WRITE_CHUNK_SIZE_FOR_PARALLEL_UPLOAD = 2 * 1024 * 1024; /// 2 MB


AzurePageBlobStream::AzurePageBlobStream(std::string& blobContainerSasUri,
    const uint32_t& timeout,
    const bool bEnableChecksums) :
    AzureBlobStream(blobContainerSasUri, timeout, bEnableChecksums)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    INM_SAFE_ARITHMETIC(m_NumberOfParallelUploads = InmSafeInt<unsigned long>::Type(MAX_WRITE_SIZE_FOR_AZURE_PAGE_BLOB) / WRITE_CHUNK_SIZE_FOR_PARALLEL_UPLOAD,
        INMAGE_EX(MAX_WRITE_SIZE_FOR_AZURE_PAGE_BLOB)(WRITE_CHUNK_SIZE_FOR_PARALLEL_UPLOAD))
    
    Init();

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}


AzurePageBlobStream::~AzurePageBlobStream()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void AzurePageBlobStream::ProcessWrite(uint64_t offset, const char *sData, uint64_t length, uint8_t index)
{
    pbyte_t pData = (pbyte_t)sData;
    blob_size_t nLength = length;

    assert(m_vPtrPageBlob[index]);

    if (!m_vPtrPageBlob[index]->Write(offset, pData, nLength))
    {
        DebugPrintf(SV_LOG_ERROR,
            "%s: failed to write %llu bytes at offset %llu.\n",
            FUNCTION_NAME,
            nLength,
            offset);

        m_bProcessWriteFailed = true;
        m_lastError = m_vPtrPageBlob[index]->GetLastError();
        m_responseData = m_vPtrPageBlob[index]->GetLastHttpResponseData();
    }
}

int AzurePageBlobStream::Write(uint64_t offset, const char* sData, uint64_t length)
{
    m_bProcessWriteFailed = false;

    boost::thread_group threadGroup;
    boost::thread *jobThread;
    uint8_t threadIndex = 0;

    assert(length <= MAX_WRITE_SIZE_FOR_AZURE_PAGE_BLOB);

    do
    {
        if (length > WRITE_CHUNK_SIZE_FOR_PARALLEL_UPLOAD)
        {
            try {
                jobThread = new boost::thread(boost::bind(&AzurePageBlobStream::ProcessWrite, this, offset, sData, WRITE_CHUNK_SIZE_FOR_PARALLEL_UPLOAD, threadIndex));
                threadGroup.add_thread(jobThread);
            }
            catch (const std::exception &e)
            {
                DebugPrintf(SV_LOG_ERROR,
                    "%s: failed to create thread to prcess write %llu bytes at offset %llu.\n",
                    FUNCTION_NAME,
                    WRITE_CHUNK_SIZE_FOR_PARALLEL_UPLOAD,
                    offset);
                
                m_bProcessWriteFailed = true;
                break;

            }
        }
        else
        {
            AzurePageBlobStream::ProcessWrite(offset, sData, length, threadIndex);
            break;
        }

        length -= WRITE_CHUNK_SIZE_FOR_PARALLEL_UPLOAD;
        offset += WRITE_CHUNK_SIZE_FOR_PARALLEL_UPLOAD;
        sData += WRITE_CHUNK_SIZE_FOR_PARALLEL_UPLOAD;
        threadIndex++;

    } while (true);

    threadGroup.join_all();

    return ((m_bProcessWriteFailed) ? SV_FAILURE : SV_SUCCESS);
}

int AzurePageBlobStream::Write(const char* sData, unsigned long int uliDataLen, unsigned long int uliOffset, const std::string& sasUri)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    
    blob_size_t nLength = HttpRestUtil::RoundToProperBlobSize(uliDataLen);

    if (uliDataLen != nLength)
    {
        DebugPrintf(SV_LOG_ERROR,
            "%s: The write data size %lu is not blob size aligned.\n",
            FUNCTION_NAME, uliDataLen);

        return SV_FAILURE;
    }

    if (uliDataLen > MAX_BLOB_SIZE_FOR_AGENT)
    {
        DebugPrintf(SV_LOG_ERROR,
            "%s: The write data size %lu is more than maximum supported %llu\n",
            FUNCTION_NAME, uliDataLen, MAX_BLOB_SIZE_FOR_AGENT);

        return SV_FAILURE;
    }

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
    m_vPtrPageBlob.clear();

    assert(m_vPtrHttpClient.size() == m_NumberOfParallelUploads);

    for (uint8_t i = 0; i < m_NumberOfParallelUploads; i++)
    {
        assert(m_vPtrHttpClient[i]);

        try {
            m_vPtrPageBlob.push_back(boost::shared_ptr<CloudPageBlob>(new CloudPageBlob(sasUri, m_vPtrHttpClient[i], true)));
        }
        catch (const std::exception& e)
        {
            DebugPrintf(SV_LOG_ERROR,
                "%s: Failed to create page blob for log file with error %s.\n",
                FUNCTION_NAME, e.what());

            return SV_FAILURE;
        }

        if (m_use_proxy)
            m_vPtrPageBlob[i]->SetHttpProxy(m_proxy);

        m_vPtrPageBlob[i]->SetTimeout(m_timeout);
    }

    if (!m_vPtrPageBlob[0]->Create(MAX_BLOB_SIZE_FOR_AGENT))
    {
        DebugPrintf(SV_LOG_ERROR,
            "%s: Failed to create %llu bytes page blob. blob %s\n",
            FUNCTION_NAME, MAX_BLOB_SIZE_FOR_AGENT, blobName.c_str());

        m_lastError = m_vPtrPageBlob[0]->GetLastError();
        m_responseData = m_vPtrPageBlob[0]->GetLastHttpResponseData();
        return SV_FAILURE;
    }

    m_continuationWriteOffset = 0;

    if (m_bEnableChecksums)
    {
        if (m_pSha256CtxForChecksums)
        {
            m_pSha256CtxForChecksums.reset();
        }
        m_pSha256CtxForChecksums = boost::make_shared<SHA256_CTX>();
        SHA256_Init(m_pSha256CtxForChecksums.get());
    }

    while (nLength > MAX_WRITE_SIZE_FOR_AZURE_PAGE_BLOB)
    {
        if (!Write(m_continuationWriteOffset, sData, MAX_WRITE_SIZE_FOR_AZURE_PAGE_BLOB))
        {
            DebugPrintf(SV_LOG_ERROR, "%s: failed to write %llu bytes at offset %llu to blob %s\n",
                FUNCTION_NAME, MAX_WRITE_SIZE_FOR_AZURE_PAGE_BLOB, m_continuationWriteOffset, blobName.c_str());

            return SV_FAILURE;
        }

        // Keep updating m_pSha256CtxForChecksums on each successful write to same blob
        if (m_bEnableChecksums && m_pSha256CtxForChecksums)
        {
            SHA256_Update(m_pSha256CtxForChecksums.get(), sData, MAX_WRITE_SIZE_FOR_AZURE_PAGE_BLOB);
        }

        sData += MAX_WRITE_SIZE_FOR_AZURE_PAGE_BLOB;
        m_continuationWriteOffset += MAX_WRITE_SIZE_FOR_AZURE_PAGE_BLOB;
        nLength -= MAX_WRITE_SIZE_FOR_AZURE_PAGE_BLOB;
    }

    if (nLength)
    {
        if (!Write(m_continuationWriteOffset, sData, nLength))
        {
            DebugPrintf(SV_LOG_ERROR, "%s: failed to write %llu bytes at offset %llu to blob %s\n",
                FUNCTION_NAME, nLength, m_continuationWriteOffset, blobName.c_str());

            return SV_FAILURE;
        }

        // Keep updating m_pSha256CtxForChecksums on each successful write to same blob
        if (m_bEnableChecksums && m_pSha256CtxForChecksums)
        {
            SHA256_Update(m_pSha256CtxForChecksums.get(), sData, nLength);
        }

        m_continuationWriteOffset += nLength;
    }

    BOOST_ASSERT(m_continuationWriteOffset == uliDataLen);
    
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return SV_SUCCESS;
}

int AzurePageBlobStream::Write(const char* sData, unsigned long int uliDataLen, const std::string& sDestination, bool bMoreData, COMPRESS_MODE compressMode, bool createDirs, bool bIsFull)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    DebugPrintf(SV_LOG_DEBUG, "%s: sDestination %s uliDataLen %lu moreData %d createDirs %d\n",
        FUNCTION_NAME, sDestination.c_str(), uliDataLen, bMoreData, createDirs);

    if (m_continuationBlobName == sDestination)
    {
        DebugPrintf(SV_LOG_DEBUG,
            "%s: Continuation write for destination %s at offset %llu.\n",
            FUNCTION_NAME, sDestination.c_str(), m_continuationWriteOffset);
    }

    const blob_size_t writeSize = HttpRestUtil::RoundToProperBlobSize(uliDataLen);

    if ((writeSize != uliDataLen) && (bMoreData))
    {
        DebugPrintf(SV_LOG_ERROR,
            "%s: The write size %lu is not blob page size aligned but has more data.\n",
            FUNCTION_NAME, uliDataLen);

        return SV_FAILURE;
    }

    if ((uliDataLen > MAX_BLOB_SIZE_FOR_AGENT))
    {
        DebugPrintf(SV_LOG_ERROR, "%s: The write data size %lu is more than maximum supported %llu\n",
            FUNCTION_NAME, uliDataLen, MAX_BLOB_SIZE_FOR_AGENT);

        return SV_FAILURE;
    }

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

        m_vPtrPageBlob.clear();
        m_responseData.clear();
        m_lastError = 0;

        assert(m_vPtrHttpClient.size() == m_NumberOfParallelUploads);

        for (uint8_t i = 0; i < m_NumberOfParallelUploads; i++)
        {
            assert(m_vPtrHttpClient[i]);

            try {
                m_vPtrPageBlob.push_back(boost::shared_ptr<CloudPageBlob>(new CloudPageBlob(blobUri, m_vPtrHttpClient[i], true)));
            }
            catch (const std::exception& e)
            {
                DebugPrintf(SV_LOG_ERROR,
                    "%s: Failed to create page blob for log file %s with error %s.\n",
                    FUNCTION_NAME, sDestination.c_str(), e.what());

                return SV_FAILURE;
            }

            if (m_use_proxy)
                m_vPtrPageBlob[i]->SetHttpProxy(m_proxy);

            m_vPtrPageBlob[i]->SetTimeout(m_timeout);
        }

        if (!m_vPtrPageBlob[0]->Create(MAX_BLOB_SIZE_FOR_AGENT))
        {
            DebugPrintf(SV_LOG_ERROR,
                "%s: Failed to create %llu bytes page blob. Log file %s\n",
                FUNCTION_NAME, MAX_BLOB_SIZE_FOR_AGENT, sDestination.c_str());

            m_lastError = m_vPtrPageBlob[0]->GetLastError();
            m_responseData = m_vPtrPageBlob[0]->GetLastHttpResponseData();
            return SV_FAILURE;
        }

        m_continuationBlobName = sDestination;
        m_continuationWriteOffset = 0;
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

        if (!m_vPtrPageBlob[0])
        {
            DebugPrintf(SV_LOG_ERROR,
                "%s: page blob object is not found for continuation write on log file %s\n",
                FUNCTION_NAME, sDestination.c_str());

            return SV_FAILURE;
        }

        m_dataSize += uliDataLen;
    }

    blob_size_t nLength = writeSize;

    while (nLength > MAX_WRITE_SIZE_FOR_AZURE_PAGE_BLOB)
    {
        if (SV_SUCCESS != Write(m_continuationWriteOffset, sData, MAX_WRITE_SIZE_FOR_AZURE_PAGE_BLOB))
        {
            DebugPrintf(SV_LOG_ERROR, "%s: failed to write %llu bytes at offset %llu to file %s.\n",
                FUNCTION_NAME, MAX_WRITE_SIZE_FOR_AZURE_PAGE_BLOB, m_continuationWriteOffset, sDestination.c_str());

            return SV_FAILURE;
	    }
        
	    // Keep updating m_pSha256CtxForChecksums on each successful write to same blob
	    if (m_bEnableChecksums && m_pSha256CtxForChecksums)
	    {
	        SHA256_Update(m_pSha256CtxForChecksums.get(), sData, MAX_WRITE_SIZE_FOR_AZURE_PAGE_BLOB);
	    }

	    sData += MAX_WRITE_SIZE_FOR_AZURE_PAGE_BLOB;
        m_continuationWriteOffset += MAX_WRITE_SIZE_FOR_AZURE_PAGE_BLOB;
        nLength -= MAX_WRITE_SIZE_FOR_AZURE_PAGE_BLOB;
    }

    if (nLength)
    {
        if (SV_SUCCESS != Write(m_continuationWriteOffset, sData, nLength))
        {
            DebugPrintf(SV_LOG_ERROR, "%s: failed to write %llu bytes at offset %llu to blob %s\n",
                FUNCTION_NAME, nLength, m_continuationWriteOffset, sDestination.c_str());

            return SV_FAILURE;
	    }

	    // Keep updating m_pSha256CtxForChecksums on each successful write to same blob
	    if (m_bEnableChecksums && m_pSha256CtxForChecksums)
	    {
	        SHA256_Update(m_pSha256CtxForChecksums.get(), sData, bMoreData? nLength : nLength - (writeSize - uliDataLen));
	    }

	    m_continuationWriteOffset += nLength;
    }

    if (!bMoreData)
    {
        DebugPrintf(SV_LOG_DEBUG, "%s: Total bytes written to the blob %llu.\n", FUNCTION_NAME, m_continuationWriteOffset);

        DebugPrintf(SV_LOG_DEBUG, "%s: Actual data size %llu.\n", FUNCTION_NAME, m_dataSize);
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return SV_SUCCESS;
}

int AzurePageBlobStream::SetMetadata(const metadata_t &metadata)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    
    int status = SV_SUCCESS;
    
    assert(m_vPtrPageBlob.size() == m_NumberOfParallelUploads);

    if (!m_vPtrPageBlob[0])
    {
        DebugPrintf(SV_LOG_ERROR,
            "%s: page blob object is not found for metadata update of log file %s\n",
            FUNCTION_NAME, m_continuationBlobName.c_str());

        return SV_FAILURE;
    }

    if (!m_vPtrPageBlob[0]->SetMetadata(metadata))
    {
        DebugPrintf(SV_LOG_ERROR, "%s: Failed to set the metadata for %s\n", FUNCTION_NAME, m_continuationBlobName.c_str());
        m_lastError = m_vPtrPageBlob[0]->GetLastError();
        m_responseData = m_vPtrPageBlob[0]->GetLastHttpResponseData();
        status = SV_FAILURE;
    }

    m_continuationWriteOffset = 0;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return status;
}
