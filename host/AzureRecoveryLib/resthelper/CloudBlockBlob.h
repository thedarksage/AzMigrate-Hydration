#ifndef AZURE_REST_STORAGE_CLOUD_BLOCKBLOB_H
#define AZURE_REST_STORAGE_CLOUD_BLOCKBLOB_H

#include "HttpClient.h"

#include <cstdio>
#include <fstream>
#include <ace/OS.h>

#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include "../config/LibXmlUtil.h"

namespace AzureStorageRest
{
    const blob_size_t AZURE_BLOCK_BLOB_MAX_WRITE_SIZE = 4 * 1024 * 1024; /// 4 MB

    const blob_size_t AZURE_BLOCK_BLOB_PARALLEL_UPLOAD_SIZE = 2 * 1024 * 1024; /// 2 MB

    class CloudBlockBlob
    {
    private:
        const std::string m_blob_sas;

        const std::vector<boost::shared_ptr<HttpClient> > & m_vPtrHttpClient;
        
        const uint8_t m_NumberOfParallelUploads;

        // http request timeout in seconds
        uint32_t    m_timeout;

        // http or curl error codes as returned by below layers for upper layer consumption
        uint32_t    m_last_error;

        // http status code as returned by Azure REST API
        uint32_t    m_http_status_code;

        std::string m_http_response_data;

        boost::mutex m_mutexLastError;

        bool        m_bWriteBlockFailed;

        boost::shared_ptr<AzureRecovery::XmlDoccument> m_xmldocBlockIds;

        uint32_t    m_BlockIdsCount;

        uint64_t    m_totalWriteSize;

        const uint32_t m_maxParallelUploadThreads;

        const uint64_t m_azureBlockBlobMaxWriteSize;

        const uint64_t m_azureBlockBlobParallelUploadSize;

        boost::shared_ptr<HttpProxy>   m_pProxy;

    private:
        void WriteBlock(const std::string& blockId,
            const char* sData,
            const blob_size_t length,
            boost::shared_ptr<HttpClient> httpClient);

        bool WriteBlockBlobMaxWriteSize(const char* sData, const blob_size_t out_length);

        void AddMetadataHeadersToRequest(HttpRequest& request, const metadata_t& metadata);

    public:

        CloudBlockBlob(const std::string& blobSas,
            const std::vector<boost::shared_ptr<HttpClient> >& vPtrHttpClient,
            const uint8_t numberOfParallelUploads,
            const uint64_t azureBlockBlobParallelUploadSize = AZURE_BLOCK_BLOB_PARALLEL_UPLOAD_SIZE,
            const uint64_t azureBlockBlobMaxWriteSize = AZURE_BLOCK_BLOB_MAX_WRITE_SIZE,
            const uint32_t maxParallelUploadThreads = 0);

        ~CloudBlockBlob() {}

        bool Write(const char* sData, const blob_size_t out_length);

        bool SetMetadata(const metadata_t& metadata); // TODO: override

        bool ClearBlockIds();

        void SetHttpProxy(const HttpProxy& proxy); // TODO: override

        void SetTimeout(const uint32_t& timeout) { m_timeout = timeout; }; // TODO: override

        uint32_t GetLastError() { return m_last_error; } // TODO: override

        uint32_t GetLastHttpStatusCode() { return m_http_status_code; } // TODO: override

        std::string GetLastHttpResponseData() { return m_http_response_data; } // TODO: override
    };

}

#endif // ~AZURE_REST_STORAGE_CLOUD_BLOCKBLOB_H
#pragma once
