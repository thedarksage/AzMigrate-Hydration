#include "CloudBlockBlob.h"
#include "../common/Trace.h"
#include "../common/AzureRecoveryException.h"
#include "securityutils.h"

#include <boost/assert.hpp>
#include <boost/thread.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/exception/all.hpp>
#include <boost/algorithm/string.hpp>

namespace AzureStorageRest
{
    /*
    Method      :  CloudBlockBlob::CloudBlockBlob

    Description :  CloudBlockBlob constructor

    Parameters  :
                   [in] blobSas: Azure block blob's SAS
                   [in] enableSSLAuthentication : flag to check if SSL authentication be verified
                   [in] azureBlockBlobParallelUploadChunkSize : max data upload size per thread in bytes
                   [in] azureBlockBlobMaxWriteSize : max data size in blob in bytes
                   [in] maxParallelUploadThreads : max number of threads to split and upload data in parallel
    Return Code :  None

    */
    CloudBlockBlob::CloudBlockBlob(const std::string& blobSas,
        const std::vector<boost::shared_ptr<HttpClient> >& vPtrHttpClient,
        const uint8_t numberOfParallelUploads,
        const uint64_t azureBlockBlobParallelUploadSize,
        const uint64_t azureBlockBlobMaxWriteSize,
        const uint32_t maxParallelUploadThreads) :
        m_blob_sas(blobSas),
        m_vPtrHttpClient(vPtrHttpClient),
        m_NumberOfParallelUploads(numberOfParallelUploads),
        m_azureBlockBlobParallelUploadSize(azureBlockBlobParallelUploadSize),
        m_azureBlockBlobMaxWriteSize(azureBlockBlobMaxWriteSize),
        m_maxParallelUploadThreads(maxParallelUploadThreads),
        m_totalWriteSize(0),
        m_timeout(0),
        m_last_error(0),
        m_http_status_code(0),
        m_bWriteBlockFailed(false),
        m_xmldocBlockIds(boost::make_shared<AzureRecovery::XmlDoccument>("BlockList")),
        m_BlockIdsCount(0)
    {
    }

    /*
    Method      :  CloudBlockBlob::WriteBlock

    Description :  Write/upload block data along with corresponding block ID to Azure blok blob

    Parameters  :
                   [in] blockId: block ID for current block data
                   [in] out_buff : buffer holding block data
                   [in] length : buffer length
                   [in] httpClient : a shared_ptr to the HttpClient that should be used to upload block
    Return Code :  None

    */
    void CloudBlockBlob::WriteBlock(const std::string& blockId,
        const char* sData,
        const blob_size_t length,
        boost::shared_ptr<HttpClient> httpClient)
    {
        TRACE_FUNC_BEGIN;
        BOOST_ASSERT(!m_blob_sas.empty());

        try
        {
            pbyte_t out_buff = (pbyte_t)sData;

            Uri blobMetadaUri(m_blob_sas);

            blobMetadaUri.AddQueryParam(Blob::QueryParamComp, Blob::QueryValueBlock);
            blobMetadaUri.AddQueryParam(Blob::QueryValueBlockID, blockId);

            HttpRequest request(blobMetadaUri.ToString());
            if (m_timeout)
            {
                request.SetTimeout(m_timeout);
            }
            request.AddHeader(RestHeader::X_MS_Version, HttpRestUtil::Get_X_MS_Version());
            request.AddHeader(RestHeader::Content_Length, boost::lexical_cast<std::string>(length));

            // Set Http Method
            request.SetHttpMethod(AzureStorageRest::HTTP_PUT);

            // Set upload/write buffer stream
            request.SetRequestBody(out_buff, length);

            HttpResponse response;
            if (!httpClient->GetResponse(request, response))
            {
                boost::unique_lock<boost::mutex> lock(m_mutexLastError);
                m_last_error = response.GetErrorCode();
                m_http_response_data = response.GetResponseData();
                m_http_status_code = response.GetResponseCode();
                m_bWriteBlockFailed = true;
                TRACE_ERROR("%s: Could not initiate put-blob rest operation, error code %lu.\n",
                    FUNCTION_NAME, m_last_error);
            }
            else if (response.GetResponseCode() != HttpErrorCode::CREATED)
            {
                boost::unique_lock<boost::mutex> lock(m_mutexLastError);
                m_last_error = response.GetResponseCode();
                m_http_response_data = response.GetResponseData();
                m_http_status_code = response.GetResponseCode();
                m_bWriteBlockFailed = true;

                //Response body will have more meaningful message about failure.
                response.PrintHttpErrorMsg();

                TRACE_ERROR("%s: Error: put-blob rest api failed, http status code %lu.\n",
                    FUNCTION_NAME, response.GetResponseCode());
            }
        }
        catch (const std::exception& e)
        {
            TRACE_ERROR("%s: Exception: %s\n", FUNCTION_NAME, e.what());
        }
        catch (...)
        {
            TRACE_ERROR("%s: Unknown Exception\n", FUNCTION_NAME);
        }

        TRACE_FUNC_END;
    }

    /*
    Method      :  CloudBlockBlob::WriteBlockBlobMaxWriteSize

    Description :  Wrapper to WriteBlock to write/upload max write size to Azure block blob

    Parameters  :
                   [in] out_buff : buffer holding block data
                   [in] out_length : buffer length
    Return Code :  true on success otherwise false

    */
    bool CloudBlockBlob::WriteBlockBlobMaxWriteSize(const char* sData, const blob_size_t out_length)
    {
        TRACE_FUNC_BEGIN;

        boost::thread_group threadGroup;
        uint8_t threadIndex = 0;
        blob_size_t length = out_length;

        assert(length <= m_azureBlockBlobMaxWriteSize);
        assert(m_vPtrHttpClient.size() == m_NumberOfParallelUploads);

        for (uint8_t threadIndex = 0; threadIndex < m_NumberOfParallelUploads; threadIndex++)
        {
            std::stringstream ssBlockId;
            ssBlockId << std::setw(5) << std::setfill('0') << m_BlockIdsCount++;
            const std::string &blockId(securitylib::base64Encode(ssBlockId.str().c_str(), ssBlockId.str().length()));
            m_xmldocBlockIds->xAddChild("Latest", blockId);

            if (length > m_azureBlockBlobParallelUploadSize)
            {
                try
                {
                    boost::thread *pJobThread = new boost::thread(boost::bind(&CloudBlockBlob::WriteBlock, this, blockId, sData, m_azureBlockBlobParallelUploadSize, m_vPtrHttpClient[threadIndex]));
                    threadGroup.add_thread(pJobThread);
                }
                catch (const std::exception& e)
                {
                    TRACE_ERROR("%s: failed to create thread to prcess write %lld bytes.\n",
                        FUNCTION_NAME, m_azureBlockBlobParallelUploadSize);

                    m_bWriteBlockFailed = true;
                    break;

                }
            }
            else
            {
                WriteBlock(blockId, sData, length, m_vPtrHttpClient[threadIndex]);
                break;
            }

            sData += m_azureBlockBlobParallelUploadSize;
            length -= m_azureBlockBlobParallelUploadSize;
            
            if (threadGroup.size() && m_maxParallelUploadThreads && threadIndex >= m_maxParallelUploadThreads)
            {
                threadGroup.join_all();
            }
        }

        threadGroup.join_all();

        /// Cleanup all outstanding BlockIds if WriteBlock failed. This way when we successfully upload data in same blob in next atempt.
        /// New set of BlockIds will be committed and old uncommitted BlockIds will be discarded.
        if (m_bWriteBlockFailed)
        {
            m_xmldocBlockIds.reset();
            m_xmldocBlockIds = boost::make_shared<AzureRecovery::XmlDoccument>("BlockList");
        }
        else
        {
            m_totalWriteSize += out_length;
        }

        TRACE_FUNC_END;

        return !m_bWriteBlockFailed;
    }

    /*
    Method      :  CloudBlockBlob::Write

    Description :  Write/upload blob data to Azure block blob

    Parameters  :
                   [in] out_buff : buffer holding block data
                   [in] out_length : buffer length
    Return Code :  true on success otherwise false

    */
    bool CloudBlockBlob::Write(const char* sData, const blob_size_t out_length)
    {
        TRACE_FUNC_BEGIN;

        blob_size_t nLength = out_length;

        while (nLength > m_azureBlockBlobMaxWriteSize)
        {
            if (!WriteBlockBlobMaxWriteSize(sData, m_azureBlockBlobMaxWriteSize))
            {
                TRACE_ERROR("%s: failed to write %lu bytes\n", FUNCTION_NAME, m_azureBlockBlobMaxWriteSize);
                return false;
            }

            sData += m_azureBlockBlobMaxWriteSize;
            nLength -= m_azureBlockBlobMaxWriteSize;
        }

        if (nLength)
        {
            if (!WriteBlockBlobMaxWriteSize(sData, nLength))
            {
                TRACE_ERROR("%s: failed to write %lu bytes\n", FUNCTION_NAME, nLength);
                return false;
            }
        }

        TRACE_FUNC_END;

        return true;
    }

    /*
    Method      :  CloudBlockBlob::AddMetadataHeadersToRequest

    Description :  Adds metadata headers to request

    Parameters  :
                   [out] request : HttpRequest to update with metadata
                   [in] metadata : set of metadata
    Return Code :  None

    */
    void CloudBlockBlob::AddMetadataHeadersToRequest(HttpRequest& request, const metadata_t& metadata)
    {
        TRACE_FUNC_BEGIN;
        metadata_const_iter_t iterBegin = metadata.begin(),
            iterEnd = metadata.end();
        for (; iterBegin != iterEnd; iterBegin++)
        {
            std::string key = iterBegin->first;
            std::string value = iterBegin->second;

            boost::trim(key);
            boost::trim(value);
            if (!key.empty() && !value.empty())
            {
                key = RestHeader::X_MS_Meta_Prefix + key;
                request.AddHeader(key, value);
            }
            else
            {
                TRACE_WARNING("%s: Empty metadata key (or) values found. Hence ignoring [%s] -> [%s]\n",
                    FUNCTION_NAME, iterBegin->first.c_str(), iterBegin->second.c_str());
            }
        }
        TRACE_FUNC_END;
    }

    /*
    Method      :  CloudBlockBlob::SetMetadata

    Description :  Commits the metadata along with outstanding block IDs to remote block blob

    Parameters  :
                   [in] metadata : set of metadata
    Return Code :  true on success otherwise false

    */
    bool CloudBlockBlob::SetMetadata(const metadata_t& metadata)
    {
        TRACE_FUNC_BEGIN;
        bool bRet = true;
        BOOST_ASSERT(!m_blob_sas.empty());
        assert(m_vPtrHttpClient.size() > 0);

        try
        {
            do
            {
                Uri blobMetadaUri(m_blob_sas);

                // Set metadata query parameter comp=metadata
                blobMetadaUri.AddQueryParam(Blob::QueryParamComp, Blob::QueryValueBlockList);

                HttpRequest request(blobMetadaUri.ToString());
                if (m_timeout)
                    request.SetTimeout(m_timeout);
                request.AddHeader(RestHeader::X_MS_Version, HttpRestUtil::Get_X_MS_Version());

                // Set block Ids in XML format in request body
                xmlChar* docstr;
                int len;
                m_xmldocBlockIds->xGetXmlDoc(docstr, len);
                // Set upload/write buffer stream
                request.SetRequestBody((pbyte_t)docstr, len);
                request.AddHeader(RestHeader::Content_Length, boost::lexical_cast<std::string>(len));

                AddMetadataHeadersToRequest(request, metadata);

                request.SetHttpMethod(AzureStorageRest::HTTP_PUT);

                HttpResponse response;
                if (!m_vPtrHttpClient[0]->GetResponse(request, response))
                {
                    bRet = false;
                    m_last_error = response.GetErrorCode();
                    TRACE_ERROR("%s: Could not initiate set-blob-metadata rest operation, error code %lu.\n",
                        FUNCTION_NAME, m_last_error);
                }
                else if (response.GetResponseCode() != HttpErrorCode::CREATED)
                {
                    bRet = false;
                    TRACE_ERROR("%s: Error: set-blob-metadata rest api failed, http status code %lu.\n",
                        FUNCTION_NAME, response.GetResponseCode());

                    m_last_error = response.GetResponseCode();

                    // Response body will have more meaningful message about failure.
                    response.PrintHttpErrorMsg();
                }
                m_http_status_code = response.GetResponseCode();
            
            } while (false);
        }
        catch (const std::exception& e)
        {
            TRACE_ERROR("%s: Exception: %s\n", FUNCTION_NAME, e.what());
            bRet = false;
        }
        catch (...)
        {
            TRACE_ERROR("%s: Unknown Exception\n", FUNCTION_NAME);
            bRet = false;
        }

        TRACE_FUNC_END;
        return bRet;
    }

    /*
    Method      :  CloudBlockBlob::ClearBlockIds

    Description :  Clears all current outstanding block IDs

    Parameters  :

    Return Code :  true on success otherwise false

    */
    bool CloudBlockBlob::ClearBlockIds()
    {
        // We need not physically delete the blob on Azure storage but to clear current set of block IDs.
        // This will allow fresh blocks to be uploaded on retry even if new block ID match to existing one and existing the block data wil be replaced with new data.
        // Also uncommitted blocks will be discarded one week after the last successful block upload by storage service.
        m_xmldocBlockIds.reset();
        m_xmldocBlockIds = boost::make_shared<AzureRecovery::XmlDoccument>("BlockList");
        return true;
    }

    /*
    Method      : CloudPageBlob::SetHttpProxy

    Description : configures the proxy settings to be used for accessing Azure blob

    Parameters  : [in]  proxy : HttpProxy settings.

    Return Code : none.
    */
    void CloudBlockBlob::SetHttpProxy(const HttpProxy& proxy)
    {
        for (int i = 0; i < m_vPtrHttpClient.size(); i++)
        {
            m_vPtrHttpClient[i]->SetProxy(proxy);
        }
    }
}