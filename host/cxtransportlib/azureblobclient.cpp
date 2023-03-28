#ifndef AZUREBLOBCLIENT_CPP
#define AZUREBLOBCLIENT_CPP
/*
*
* File       : azureblobclient.cpp
*
* Description:
*/
#include <stdexcept>
#include <iomanip>

#include <boost/thread.hpp>
#include <boost/foreach.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>

#include "RestConstants.h"
#include "azureblobclient.h"
#include "portablehelpers.h"
#include "inmsafecapis.h"

// can change to SV_LOG_ALWAYS for debugging
SV_LOG_LEVEL AzureBlobClient::s_logLevel = SV_LOG_DEBUG;

const std::size_t MAX_BLOB_SIZE_FOR_AGENT = 128 * 1024 * 1024; /// 128 MB

const std::size_t MAX_WRITE_SIZE_FOR_AZURE_PAGE_BLOB = 4 * 1024 * 1024; /// 4 MB

const std::size_t MAX_READ_SIZE_FOR_AZURE_PAGE_BLOB = 8 * 1024 * 1024; /// 8 MB

const std::string MetadataKeyStatus = "Status";
const std::string MetadataStatusDeleted = "Deleted";
const std::string MetadataStatusCompleted = "Completed";
const std::string MetadataKeyBlobContentLength = "BlobContentLength";

const uint32_t MaxResults = 1000;

uint32_t AzureBlobClient::s_id = 0;

AzureBlobClient::AzureBlobClient(const std::string &blobContainerSasUri,
    const std::size_t maxBufferSizeBytes,
    const uint32_t timeout) :
    m_blobContainerSasUri(blobContainerSasUri),
    m_continuationWriteOffset(0),
    m_continuationReadOffset(0),
    m_readFileSize(0),
    m_writeDataSize(0),
    m_maxBufferSizeBytes(maxBufferSizeBytes),
    m_bufferedWriteSize(0),
    m_use_proxy(false),
    m_timeout(timeout),
    m_PtrHttpClient(new HttpClient()),
    m_PtrHttpClientForList(new HttpClient()),
    m_PtrHttpClientForDelete(new HttpClient())
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    s_id++;
    m_id = s_id;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s: %u\n", FUNCTION_NAME, m_id);
}

AzureBlobClient::~AzureBlobClient()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s: %u\n", FUNCTION_NAME, m_id);
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void AzureBlobClient::SetHttpProxy(const std::string& address,
    const std::string& port,
    const std::string& bypasslist)
{
    m_use_proxy = true;
    m_proxy.m_address = address;
    m_proxy.m_port = port;
    m_proxy.m_bypasslist = bypasslist;
    return;
}

void AzureBlobClient::CreateCloudPageBlobObject(const std::string& blobSas)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    m_lastError = 0;

    assert(m_PtrHttpClient);

    try
    {
        m_PtrPageBlob.reset(new CloudPageBlob(blobSas, m_PtrHttpClient, true));
    }
    catch (const std::exception &e)
    {
        throw ERROR_EXCEPTION << "failed to create CloudPageBlob object with exception " << e.what();
    }
    catch (...)
    {
        throw ERROR_EXCEPTION << "failed to create CloudPageBlob object with an unknown exception.";
    }

    if (m_use_proxy)
    {
        m_PtrPageBlob->SetHttpProxy(m_proxy);
    }

    m_PtrPageBlob->SetTimeout(m_timeout);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void AzureBlobClient::CreateCloudPageBlobObjectForList(const std::string& blobSas)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    m_lastError = 0;

    assert(m_PtrHttpClientForList);

    try
    {
        m_PtrPageBlobForList.reset(new CloudPageBlob(blobSas, m_PtrHttpClientForList, true));
    }
    catch (const std::exception &e)
    {
        throw ERROR_EXCEPTION << "failed to create CloudPageBlob object with exception " << e.what();
    }
    catch (...)
    {
        throw ERROR_EXCEPTION << "failed to create CloudPageBlob object with an unknown exception.";
    }

    if (m_use_proxy)
    {
        m_PtrPageBlobForList->SetHttpProxy(m_proxy);
    }

    m_PtrPageBlobForList->SetTimeout(m_timeout);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void AzureBlobClient::CreateCloudPageBlobObjectForDelete(const std::string& blobSas)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    m_lastError = 0;

    assert(m_PtrHttpClientForDelete);

    try
    {
        m_PtrPageBlobForDelete.reset(new CloudPageBlob(blobSas, m_PtrHttpClientForDelete, true));
    }
    catch (const std::exception &e)
    {
        throw ERROR_EXCEPTION << "failed to create CloudPageBlob object with exception " << e.what();
    }
    catch (...)
    {
        throw ERROR_EXCEPTION << "failed to create CloudPageBlob object with an unknown exception.";
    }

    if (m_use_proxy)
    {
        m_PtrPageBlobForDelete->SetHttpProxy(m_proxy);
    }

    m_PtrPageBlobForDelete->SetTimeout(m_timeout);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

std::string AzureBlobClient::GetRestUriFromSasUri(const std::string& SasUri,
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

void AzureBlobClient::Write(const std::size_t offset, const char *sData, const std::size_t length)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    pbyte_t pData = (pbyte_t)sData;
    std::size_t nLength = length;

    assert(m_PtrPageBlob);

    if (!m_PtrPageBlob->Write(offset, pData, nLength))
    {
        m_lastError = m_PtrPageBlob->GetLastError();
        throw ERROR_EXCEPTION << FUNCTION_NAME
            << " failed to write " << nLength << " bytes at offset " << offset
            << ", error: " << m_lastError
            << ", status code: " << m_PtrPageBlob->GetLastHttpStatusCode();
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void AzureBlobClient::Write(const char *data, const std::size_t dataSize)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    if (!dataSize)
    {
        DebugPrintf(SV_LOG_DEBUG, "%s: EXITED on zero length wirte\n", FUNCTION_NAME);
        return;
    }

    const std::size_t alignedWriteSize = HttpRestUtil::RoundToProperBlobSize(dataSize);

    assert(alignedWriteSize > dataSize);

    std::size_t nLength = alignedWriteSize;

    while (nLength >= MAX_WRITE_SIZE_FOR_AZURE_PAGE_BLOB)
    {
        Write(m_continuationWriteOffset, data, MAX_WRITE_SIZE_FOR_AZURE_PAGE_BLOB);

        data += MAX_WRITE_SIZE_FOR_AZURE_PAGE_BLOB;
        m_continuationWriteOffset += MAX_WRITE_SIZE_FOR_AZURE_PAGE_BLOB;
        nLength -= MAX_WRITE_SIZE_FOR_AZURE_PAGE_BLOB;
    }

    if (nLength)
    {
        Write(m_continuationWriteOffset, data, nLength);
        m_continuationWriteOffset += nLength;
    }

    DebugPrintf(s_logLevel, "%s: total bytes written to the blob %ld, bytes written in this iteration %ld, actual data size %ld.\n",
        FUNCTION_NAME, m_continuationWriteOffset, alignedWriteSize, dataSize);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

/// \brief issue put file request to send data to a remote file
///
/// use to send data to Azure blob. If the total size of the data to be sent
/// is not pre-calculated then multiple put file requests can be used to send the data.
///
/// \exception throws ERROR_EXCEPTION on failure
///
/// \note
/// \li \c if more then 1 put file request is needed to send all the data, then no other
/// requests can be sent until a put file request with moreData set to false is issued
/// \li \c if all the data has been sent before knowing there is no more data (i.e. a
/// putfile request with moreData set to false has not been sent), then 1 additional put
/// file request with moreData set to false still needs to be sent to let the server know
/// all the data has been sent. In this case set dataSize to 0 and data to NULL (0)
void AzureBlobClient::putFile(std::string const& remoteName,        ///< name of remote file to put the data into
    std::size_t dataSize,                                           ///< size of the data being sent in this request
    char const * data,                                              ///< the data to put in the file
    bool moreData,                                                  ///< if there is more data to be sent true: yes, false: no
    COMPRESS_MODE compressMode,                                     ///< compress mode (see volumegroupsettings.h)
    mapHeaders_t const & headers,                                   ///< additional headers to send in the putfile request
    bool createDirs,                                                ///< indicates if missing dirs should be created (true: yes, false: no)
    long long offset                                                ///< offset to write at
)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s: %u\n", FUNCTION_NAME, m_id);
    boost::unique_lock<boost::recursive_mutex> guard(m_contextLock);


    try
    {
        DebugPrintf(s_logLevel, "ENTERED %s: remoteName %s dataSize %ld moreData %d continuationName %s bufferdsize %ld\n",
            FUNCTION_NAME, remoteName.c_str(), dataSize, moreData, m_continuationBlobName.c_str(), m_bufferedWriteSize);

        if (dataSize > MAX_BLOB_SIZE_FOR_AGENT)
        {
            throw ERROR_EXCEPTION << "the write size " << dataSize << " is more than maximum supported " << MAX_BLOB_SIZE_FOR_AGENT;
        }

        if (!dataSize && !m_bufferedWriteSize)
        {
            ResponseData responseData;
            responseData.responseCode = moreData ? CLIENT_RESULT_MORE_DATA : CLIENT_RESULT_OK;
            setResponseData(responseData);

            DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

            return;
        }

        if (!boost::iequals(m_continuationBlobName, remoteName))
        {
            const std::string pageBlobUri = GetRestUriFromSasUri(m_blobContainerSasUri, remoteName);
            CreateCloudPageBlobObject(pageBlobUri);
            if (!m_PtrPageBlob->Create(MAX_BLOB_SIZE_FOR_AGENT))
            {
                m_lastError = m_PtrPageBlob->GetLastError();
                throw ERROR_EXCEPTION << FUNCTION_NAME
                    << " failed to create " << MAX_BLOB_SIZE_FOR_AGENT << " bytes page blob"
                    << ", error: " << m_lastError
                    << ", status code: " << m_PtrPageBlob->GetLastHttpStatusCode();
            }

            m_continuationBlobName = remoteName;
            m_continuationWriteOffset = 0;

            m_writeDataSize = 0;
            m_bufferedWriteSize = 0;
            m_ptrWriteBuffer.reset(new char[Blob::BlobPageSize]);
            if (!m_ptrWriteBuffer.get())
            {
                throw ERROR_EXCEPTION << "failed to allocate memory for buffering non-aligned write.";
            }
            memset(m_ptrWriteBuffer.get(), 0, Blob::BlobPageSize);
        }
        else
        {
            DebugPrintf(s_logLevel, "%s: continuation write for destination %s at offset %ld.\n",
                FUNCTION_NAME, remoteName.c_str(), m_continuationWriteOffset);

            if (!m_PtrPageBlob)
            {
                throw ERROR_EXCEPTION << "CloudPageBlob object not initialized for continuation write.";
            }

            if (!m_ptrWriteBuffer.get())
            {
                throw ERROR_EXCEPTION << "memory buffer for non-aligned write is not allocated.";
            }
        }

        m_writeDataSize += dataSize;

        // there is nothing in the buffer and no more data
        if (!m_bufferedWriteSize && !moreData)
        {
            if (dataSize < Blob::BlobPageSize)
            {
                Write(data, dataSize);
            }
            else
            {
                uint32_t unalignedDataSize = dataSize % Blob::BlobPageSize;

                Write(data, dataSize - unalignedDataSize);

                if (unalignedDataSize)
                {
                    inm_memcpy_s(m_ptrWriteBuffer.get(), Blob::BlobPageSize, data + dataSize - unalignedDataSize, unalignedDataSize);

                    Write(m_ptrWriteBuffer.get(), unalignedDataSize);
                }
            }
        }
        else if (m_bufferedWriteSize && !moreData)
        {
            if (m_bufferedWriteSize + dataSize < Blob::BlobPageSize)
            {
                inm_memcpy_s(m_ptrWriteBuffer.get() + m_bufferedWriteSize, Blob::BlobPageSize - m_bufferedWriteSize, data, dataSize);

                m_bufferedWriteSize += dataSize;

                Write(m_ptrWriteBuffer.get(), m_bufferedWriteSize);

                m_bufferedWriteSize = 0;
            }
            else // (m_bufferedWriteSize + dataSize >= Blob::BlobPageSize)
            {
                // fill rest of the buffer
                uint32_t residualBufferLength = Blob::BlobPageSize - m_bufferedWriteSize;
                inm_memcpy_s(m_ptrWriteBuffer.get() + m_bufferedWriteSize, residualBufferLength, data, residualBufferLength);

                // write buffered data
                Write(m_ptrWriteBuffer.get(), Blob::BlobPageSize);

                m_bufferedWriteSize = 0;

                uint32_t unalignedDataSize = (dataSize - residualBufferLength) % Blob::BlobPageSize;

                // write rest of the data
                Write(data + residualBufferLength, dataSize - residualBufferLength - unalignedDataSize);

                if (unalignedDataSize)
                {
                    inm_memcpy_s(m_ptrWriteBuffer.get(), Blob::BlobPageSize, data + dataSize - unalignedDataSize, unalignedDataSize);

                    Write(m_ptrWriteBuffer.get(), unalignedDataSize);
                }
            }
        }
        else if (!m_bufferedWriteSize && moreData)
        {
            if (dataSize < Blob::BlobPageSize)
            {
                inm_memcpy_s(m_ptrWriteBuffer.get(), Blob::BlobPageSize, data, dataSize);
                m_bufferedWriteSize = dataSize;
            }
            else // (dataSize >= Blob::BlobPageSize)
            {
                uint32_t unalignedDataSize = dataSize % Blob::BlobPageSize;

                Write(data, dataSize - unalignedDataSize);

                inm_memcpy_s(m_ptrWriteBuffer.get(), Blob::BlobPageSize, data + dataSize - unalignedDataSize, unalignedDataSize);

                m_bufferedWriteSize = unalignedDataSize;
            }
        }
        else // (m_bufferedWriteSize && moreData)
        {
            if (dataSize + m_bufferedWriteSize < Blob::BlobPageSize)
            {
                inm_memcpy_s(m_ptrWriteBuffer.get() + m_bufferedWriteSize, Blob::BlobPageSize - m_bufferedWriteSize, data, dataSize);
                m_bufferedWriteSize += dataSize;
            }
            else // (dataSize + m_bufferedWriteSize >= Blob::BlobPageSize)
            {
                // fill rest of the buffer
                uint32_t residualBufferLength = Blob::BlobPageSize - m_bufferedWriteSize;
                inm_memcpy_s(m_ptrWriteBuffer.get() + m_bufferedWriteSize, residualBufferLength, data, residualBufferLength);

                // write buffered data
                Write(m_ptrWriteBuffer.get(), Blob::BlobPageSize);

                m_bufferedWriteSize = 0;

                std::size_t remainingDataSize = dataSize - residualBufferLength;

                if (remainingDataSize < Blob::BlobPageSize)
                {
                    inm_memcpy_s(m_ptrWriteBuffer.get() , Blob::BlobPageSize, data + residualBufferLength, remainingDataSize);
                    m_bufferedWriteSize = remainingDataSize;
                }
                else // (remainingDataSize >= Blob::BlobPageSize)
                {
                    uint32_t unalignedDataSize = remainingDataSize % Blob::BlobPageSize;

                    Write(data + residualBufferLength, remainingDataSize - unalignedDataSize);

                    inm_memcpy_s(m_ptrWriteBuffer.get(), Blob::BlobPageSize, data + dataSize - unalignedDataSize, unalignedDataSize);

                    m_bufferedWriteSize = unalignedDataSize;
                }
            }
        }

        ResponseData responseData;
        responseData.responseCode = moreData ? CLIENT_RESULT_MORE_DATA : CLIENT_RESULT_OK;
        setResponseData(responseData);

        DebugPrintf(s_logLevel, "EXITED %s: remoteName %s dataSize %ld moreData %d continuationName %s bufferdsize %ld\n",
            FUNCTION_NAME, remoteName.c_str(), dataSize, moreData, m_continuationBlobName.c_str(), m_bufferedWriteSize);

    }
    catch (const std::exception & e)
    {
        throw ERROR_EXCEPTION << FUNCTION_NAME << " failed"
            << ", remoteName: " << remoteName
            << ", dataSize: " << dataSize
            << ", moreData: " << moreData
            << ", error: " << e.what();
    }
    catch (...)
    {
        throw ERROR_EXCEPTION << FUNCTION_NAME << " failed with an unknown exception"
            << ", remoteName: " << remoteName
            << ", dataSize: " << dataSize
            << ", moreData: " << moreData;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void AzureBlobClient::putFile(std::string const& remoteName,        ///< name of remote file to put the data into
    std::size_t dataSize,                                           ///< size of the data being sent in this request
    char const * data,                                              ///< the data to put in the file
    bool moreData,                                                  ///< if there is more data to be sent true: yes, false: no
    COMPRESS_MODE compressMode,                                     ///< compress mode (see volumegroupsettings.h)
    bool createDirs,                                                ///< indicates if missing dirs should be created (true: yes, false: no)
    long long offset                                                ///< offset to write at
)
{
    putFile(remoteName, dataSize, data, moreData, compressMode, s_emptyHeader, createDirs, offset);
}

/// \brief issue put file request to send a local file to a remote file
///
/// use to send a local file to a remote file.
///
/// \exception throws ERROR_EXCEPTION on failure
///
/// \note
/// \li \c always uses binary mode
///
void AzureBlobClient::putFile(std::string const& remoteName,                ///< name of remote file to put the data into
    std::string const& localName,                                           ///< name of local file to send
    COMPRESS_MODE compressMode,                                             ///< compress mode (see volumegroupsettings.h)
    mapHeaders_t const & headers,                                           ///< additional headers to send in putfile request
    bool createDirs                                                         ///< indicates if missing dirs should be created (true: yes, false: no)
)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s: %u\n", FUNCTION_NAME, m_id);
    boost::unique_lock<boost::recursive_mutex> guard(m_contextLock);

    try
    {
        DebugPrintf(s_logLevel, "%s: localName %s remoteName %s\n",
            FUNCTION_NAME, localName.c_str(), remoteName.c_str());

        if (boost::iequals(m_continuationBlobName, remoteName))
        {
            DebugPrintf(s_logLevel, "%s: continuation write for destination %s.\n",
                FUNCTION_NAME, remoteName.c_str());
        }
        else
        {
            const std::string pageBlobUri = GetRestUriFromSasUri(m_blobContainerSasUri, remoteName);
            CreateCloudPageBlobObject(pageBlobUri);
            if (!m_PtrPageBlob->Create(MAX_BLOB_SIZE_FOR_AGENT))
            {
                m_lastError = m_PtrPageBlob->GetLastError();
                throw ERROR_EXCEPTION << FUNCTION_NAME
                    << " failed to create " << MAX_BLOB_SIZE_FOR_AGENT << " bytes page blob"
                    << ", error: " << m_lastError
                    << ", status code: " << m_PtrPageBlob->GetLastHttpStatusCode();
            
            }

            m_bufferedWriteSize = 0;
            m_ptrWriteBuffer.reset(new char[Blob::BlobPageSize]);
            if (!m_ptrWriteBuffer.get())
            {
                throw ERROR_EXCEPTION << "failed to allocate memory for buffering non-aligned write.";
            }
            memset(m_ptrWriteBuffer.get(), 0, Blob::BlobPageSize);

            m_continuationBlobName = remoteName;
            m_continuationWriteOffset = 0;
            m_writeDataSize = boost::filesystem::file_size(localName);
        }
        
        if (!m_PtrPageBlob)
        {
            throw ERROR_EXCEPTION << "CloudPageBlob object not initialized.";
        }

        if (!m_PtrPageBlob->UploadFileContent(localName))
        {
            m_lastError = m_PtrPageBlob->GetLastError();
            throw ERROR_EXCEPTION << "failed to upload file content with error: " << m_lastError
                << ", status code: " << m_PtrPageBlob->GetLastHttpStatusCode();
        }

        ResponseData responseData;
        responseData.responseCode = CLIENT_RESULT_OK;
        setResponseData(responseData);
    }
    catch (const std::exception & e)
    {
        throw ERROR_EXCEPTION << FUNCTION_NAME << " failed"
            << ", localName: " << localName
            << ", remoteName: " << remoteName
            << ", error: " << e.what();
    }
    catch (...)
    {
        throw ERROR_EXCEPTION << FUNCTION_NAME << " failed with an unknown exception"
            << ", localName: " << localName
            << ", remoteName: " << remoteName;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void AzureBlobClient::putFile(std::string const& remoteName,                ///< name of remote file to put the data into
    std::string const& localName,                                           ///< name of local file to send
    COMPRESS_MODE compressMode,                                             ///< compress mode (see volumegroupsettings.h)
    bool createDirs                                                         ///< indicates if missing dirs should be created (true: yes, false: no)
)
{
    putFile(remoteName, localName, compressMode, s_emptyHeader, createDirs);
}


/// \brief issue list file request
///
/// \return
/// \li \c CLIENT_RESULT_OK       : on success
/// \li \c CLIENT_RESULT_NOT_FOUND: no matches found
///
/// \exception ERROR_EXCEPTION on failure
ClientCode AzureBlobClient::listFile(std::string const& fileSpec, ///< file specification to list. Use stnadard glob syntax
    std::string & files)                                          ///< receives the list of files each separated by new-line (\\n)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s: %u, filespec %s.\n", FUNCTION_NAME, m_id, fileSpec.c_str());
    boost::unique_lock<boost::recursive_mutex> guard(m_contextLock);

    ClientCode clientCode = CLIENT_RESULT_OK;

    files.clear();

    try
    {
        if (!m_PtrPageBlobForList)
        {
            CreateCloudPageBlobObjectForList(m_blobContainerSasUri);
        }

        const std::string fileSpecToList = (fileSpec[fileSpec.length() - 1] == '*') ?
            fileSpec.substr(0, fileSpec.length() - 1) : fileSpec;

        std::string strListOutputXml;
        std::stringstream ssListOutput;
        if (!m_PtrPageBlobForList->List(fileSpecToList, MaxResults, strListOutputXml))
        {
            m_lastError = m_PtrPageBlobForList->GetLastError();
            throw ERROR_EXCEPTION << FUNCTION_NAME
                << " List failed"
                << ", error: " << m_lastError
                << ", status code: " << m_PtrPageBlobForList->GetLastHttpStatusCode();
        }
        else
        {
            // Expected list response xml schema:
            //  <EnumerationResults>
            //    <Blobs>
            //        <Blob>
            //            <Name>blob name</Name>
            //            <Properties>
            //                <Content-Length><size of blob></Content-Length>
            //            </Properties>
            //            <Metadata>
            //                <key>value</key>
            //            </Metadata>
            //        </Blob>
            //    </Blobs>
            //  </EnumerationResults>


            std::stringstream ss(strListOutputXml);
            boost::property_tree::ptree pt;
            boost::property_tree::read_xml(ss, pt);

            boost::optional<boost::property_tree::ptree &> ptBlobs = pt.get_child_optional("EnumerationResults.Blobs");
            if (ptBlobs)
            {
                BOOST_FOREACH(boost::property_tree::ptree::value_type &ptBlob, pt.get_child("EnumerationResults.Blobs"))
                {
                    const std::string metadataKeyStatus = "Metadata." + MetadataKeyStatus;
                    const std::string metadataKeyBlobContentLength = "Metadata." + MetadataKeyBlobContentLength;
                    try
                    {
                        const std::string metadataKeyStatusValue = ptBlob.second.get<std::string>(metadataKeyStatus);
                        if (boost::iequals(metadataKeyStatusValue, MetadataStatusCompleted))
                        {
                            try
                            {
                                const std::string blobName = ptBlob.second.get<std::string>("Name");
                                const std::size_t blobContentLength = ptBlob.second.get<std::size_t>(metadataKeyBlobContentLength);
                                // CxTransportImpClient assumes files has the following format
                                //    "filename\tfilesize\n"
                                // for each file returned in the list
                                ssListOutput << blobName << '\t' << blobContentLength << '\n';
                            }
                            catch (const std::exception &e)
                            {
                                DebugPrintf(SV_LOG_ERROR, "%s: failed to retrieve blob name and Content-Length with exception %s.\n",
                                    FUNCTION_NAME, e.what());
                            }
                            catch (...)
                            {
                                DebugPrintf(SV_LOG_ERROR, "%s: failed to retrieve blob name and Content-Length with an unknown exception.\n",
                                    FUNCTION_NAME);
                            }
                        }
                        else
                        {
                            DebugPrintf(SV_LOG_DEBUG, "%s: unmatched metadata %s=%s.\n",
                                FUNCTION_NAME, metadataKeyStatus.c_str(), metadataKeyStatusValue.c_str());
                        }
                    }
                    catch (const std::exception &e)
                    {
                        // Log error message and continue
                        std::string blobName; try { blobName = ptBlob.second.get<std::string>("Name"); }
                        catch (...) {};
                        DebugPrintf(SV_LOG_DEBUG, "%s: property tree failed to retrieve metadata \"%s\" for blob \"%s\" with exception %s.\n",
                            FUNCTION_NAME, metadataKeyStatus.c_str(), blobName.c_str(), e.what());
                    }
                    catch (...)
                    {
                        // Log error message and continue
                        std::string blobName; try { blobName = ptBlob.second.get<std::string>("Name"); }
                        catch (...) {};
                        DebugPrintf(SV_LOG_DEBUG, "%s: property tree failed to retrieve metadata \"%s\" for blob \"%s\" with an unknown exception.\n",
                            FUNCTION_NAME, metadataKeyStatus.c_str(), blobName.c_str());
                    }
                }
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "%s: expected XML node EnumerationResults.Blobs not found in results.\n", FUNCTION_NAME);
            }

            if (ssListOutput.str().empty())
            {
                clientCode = CLIENT_RESULT_NOT_FOUND;
                DebugPrintf(s_logLevel, "%s: List result does not contain metadata %s=%s.\n", FUNCTION_NAME,
                    MetadataKeyStatus.c_str(), MetadataStatusCompleted.c_str());
            }
            else
            {
                files = ssListOutput.str();
                DebugPrintf(s_logLevel, "%s: files: %s\n", FUNCTION_NAME, files.c_str());
            }
        }
    }
    catch (const std::exception &e)
    {
        throw ERROR_EXCEPTION << FUNCTION_NAME << " failed"
            << ", fileSpec: " << fileSpec
            << ", error: " << e.what();
    }
    catch (...)
    {
        throw ERROR_EXCEPTION << FUNCTION_NAME << " failed with an unknown exception"
            << ", fileSpec: " << fileSpec;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return clientCode;
}

/// \brief issue rename file request
///
///  \note
///  \li \c finalPaths should be used to tell transport it should perform the "final" rename instead of time shot manager
///  it should be a semi-conlon (';') separated list of the final paths that the file should get a "copy" of the new file.
///  hard links are used to create the "copies" unless transport server was built with RENAME_COPY_ON_FAILED_LINK defined.
///  in that case transport will attempt to copy the file if the hard link fails.
///
/// \exception ERROR_EXCEPTION on failure
/// \return
/// \li \c CLIENT_RESULT_OK       : on success
/// \li \c CLIENT_RESULT_NOT_FOUND: oldName not found
ClientCode AzureBlobClient::renameFile(std::string const& oldName,   ///< the name of the file that is to be renamed
    std::string const& newName,                    ///< the new name to use
    COMPRESS_MODE compressMode,                    ///< compress mode in affect (see compressmode.h)
    mapHeaders_t const& headers,                   ///< additional headers to be sent in renamefile request
    std::string const& finalPaths                  ///< semi-colon (';') separated  list of all paths that should get a "copy" of the renamed file
)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s: %u\n", FUNCTION_NAME, m_id);
    boost::unique_lock<boost::recursive_mutex> guard(m_contextLock);

    DebugPrintf(s_logLevel, "ENTERED %s: oldName %s newName %s\n", FUNCTION_NAME, oldName.c_str(), newName.c_str());

    ClientCode clientCode = CLIENT_RESULT_OK;

    try
    {

        if (boost::starts_with(newName, "corrupt_"))
        {
            DebugPrintf(s_logLevel, "%s: Deleting corrupt_ file %s.\n", FUNCTION_NAME, oldName.c_str());
            clientCode = deleteFile(oldName);

            DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
            return clientCode;
        }

        if (!boost::iequals(m_continuationBlobName, oldName) || !m_PtrPageBlob)
        {
            DebugPrintf(SV_LOG_ERROR, "%s: rename operation on a file name %s other than continuation name %s\n",
                FUNCTION_NAME, oldName.c_str(), m_continuationBlobName.c_str());

            throw ERROR_EXCEPTION << "rename operation on a file name other than continuation name. " << oldName << ", " << m_continuationBlobName;
            //return clientCode;
        }

        AzureStorageRest::metadata_t metadata;
        metadata[MetadataKeyStatus] = MetadataStatusCompleted;
        metadata[MetadataKeyBlobContentLength] = boost::lexical_cast<std::string>(m_writeDataSize);
        if (!m_PtrPageBlob->SetMetadata(metadata))
        {
            m_lastError = m_PtrPageBlob->GetLastError();
            throw ERROR_EXCEPTION << FUNCTION_NAME
                << " failed to set the metadata "
                << MetadataKeyStatus << "=" << MetadataStatusCompleted
                << ", error: " << m_lastError
                << ", status code: " << m_PtrPageBlob->GetLastHttpStatusCode();
        }
        m_continuationWriteOffset = 0;
        m_writeDataSize = 0;
        m_bufferedWriteSize = 0;
    }
    catch (const std::exception &e)
    {
        throw ERROR_EXCEPTION << FUNCTION_NAME << " failed"
            << ", oldName: " << oldName
            << ", newName: " << newName
            << ", error: " << e.what();
    }
    catch (...)
    {
        throw ERROR_EXCEPTION << FUNCTION_NAME << " failed with an unknown exception"
            << ", oldName: " << oldName
            << ", newName: " << newName;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return clientCode;
}

/// \brief issue rename file request
///
///  \note
///  \li \c finalPaths should be used to tell transport it should perform the "final" rename instead of time shot manager
///  it should be a semi-conlon (';') separated list of the final paths that the file should get a "copy" of the new file.
///  hard links are used to create the "copies" unless transport server was built with RENAME_COPY_ON_FAILED_LINK defined.
///  in that case transport will attempt to copy the file if the hard link fails.
///
/// \exception ERROR_EXCEPTION on failure
/// \return
/// \li \c CLIENT_RESULT_OK       : on success
/// \li \c CLIENT_RESULT_NOT_FOUND: oldName not found
ClientCode AzureBlobClient::renameFile(std::string const& oldName,   ///< the name of the file that is to be renamed
    std::string const& newName,                    ///< the new name to use
    COMPRESS_MODE compressMode,                    ///< compress mode in affect (see compressmode.h)
    std::string const& finalPaths                  ///< semi-colon (';') separated  list of all paths that should get a "copy" of the renamed file
)
{
    return renameFile(oldName, newName, compressMode, s_emptyHeader, finalPaths);
}

void AzureBlobClient::Read(const std::size_t offset,   ///< start offset in blob to read
    char * buffer,               ///< points to the read data returned
    const std::size_t length,       ///< size of the buffer
    std::size_t& bytesReturned)  ///< set the the number of bytes returned in data
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s: %u\n", FUNCTION_NAME, m_id);

    bytesReturned = 0;

    assert(m_PtrPageBlob);

    bytesReturned = m_PtrPageBlob->Read(offset, (pbyte_t)buffer, length);
    if ((bytesReturned == 0) ||
        ((m_PtrPageBlob->GetLastHttpStatusCode() != HttpErrorCode::OK) &&
        (m_PtrPageBlob->GetLastHttpStatusCode() != HttpErrorCode::PARTIAL_CONTENT)))
    {
        m_lastError = m_PtrPageBlob->GetLastError();
        throw ERROR_EXCEPTION << FUNCTION_NAME
            << " failed to read " << length << " bytes at offset " << offset
            << ", bytes read: " << bytesReturned
            << ", error: " << m_lastError
            << ", status code: " << m_PtrPageBlob->GetLastHttpStatusCode();
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

void AzureBlobClient::ReadWithSize(std::size_t offset,
    char* buffer,
    const std::size_t length,
    std::size_t& bytesReturned,
    std::size_t& blobContentLength)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s: %u\n", FUNCTION_NAME, m_id);

    bytesReturned = 0;

    blob_properties properties;

    bytesReturned = m_PtrPageBlob->Read(offset, (pbyte_t)buffer, length, properties);

    if ((bytesReturned == 0) ||
        ((m_PtrPageBlob->GetLastHttpStatusCode() != HttpErrorCode::OK) &&
         (m_PtrPageBlob->GetLastHttpStatusCode() != HttpErrorCode::PARTIAL_CONTENT)))
    {
        m_lastError = m_PtrPageBlob->GetLastError();
        throw ERROR_EXCEPTION << FUNCTION_NAME
            << " failed to read " << length << " bytes at offset " << offset
            << ", bytes read: " << bytesReturned
            << ", error: " << m_lastError
            << ", status code: " << m_PtrPageBlob->GetLastHttpStatusCode();
    }

    metadata_t::iterator iter = properties.metadata.find(MetadataKeyBlobContentLength);
    if (iter == properties.metadata.end())
    {
        m_lastError = m_PtrPageBlob->GetLastError();
        throw ERROR_EXCEPTION << FUNCTION_NAME
            << " failed to get size "
            << ", bytes read: " << bytesReturned
            << ", error: " << m_lastError
            << ", status code: " << m_PtrPageBlob->GetLastHttpStatusCode();
    }

    blobContentLength = boost::lexical_cast<std::size_t>(iter->second);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

/// \brief issue get file request to get a remote file to a buffer
///
/// \note
/// \li \c if CLIENT_RESULT_MORE_DATA is returned, you must copy all the data returned
/// in "data" before making the next call as "data" will be overwritten on each call
///
/// \return
/// \li \c CLIENT_RESULT_OK       : on success and no more data to receive
/// \li \c CLIENT_RESULT_MORE_DATA: on success and more data to reveive
/// \li \c CLIENT_RESULT_NOT_FOUND: remote file not found
///
/// \exception ERROR_EXCEPTION on failure
ClientCode AzureBlobClient::getFile(std::string const& name,     ///< file to get
    std::size_t dataSize,        ///< size of the buffer pointed to by data
    char * data,                 ///< points to the get file data returned
    std::size_t& bytesReturned)  ///< set the the number of bytes returned in data
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    ClientCode cc = getFile(name, m_continuationReadOffset, dataSize, data, bytesReturned);

    if (cc == CLIENT_RESULT_OK)
        m_continuationReadOffset = 0;
    else
        m_continuationReadOffset += bytesReturned;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return cc;
}

ClientCode AzureBlobClient::getFile(std::string const& name,     ///< file to get
        std::size_t offset,
        std::size_t dataSize,        ///< size of the buffer pointed to by data
        char * data,                 ///< points to the get file data returned
        std::size_t& bytesReturned)  ///< set the the number of bytes returned in data
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s: %u\n", FUNCTION_NAME, m_id);
    boost::unique_lock<boost::recursive_mutex> guard(m_contextLock);

    ClientCode clientCode = CLIENT_RESULT_OK;
    bytesReturned = 0;
    try
    {
        DebugPrintf(s_logLevel, "ENTERED %s: remoteName %s dataSize %u offset %u\n",
            FUNCTION_NAME, name.c_str(), dataSize, offset);

        if (dataSize > MAX_BLOB_SIZE_FOR_AGENT)
        {
            DebugPrintf(SV_LOG_ERROR, "%s: for file %s read size %ld more than max supported %ld\n",
                FUNCTION_NAME, name.c_str(), dataSize, MAX_BLOB_SIZE_FOR_AGENT);
            throw ERROR_EXCEPTION << "the read size " << dataSize << " is more than maximum supported " << MAX_BLOB_SIZE_FOR_AGENT;
        }

        if (boost::iequals(m_continuationBlobName, name))
        {
            DebugPrintf(s_logLevel, "%s: continuation read for destination %s at offset %ld.\n",
                FUNCTION_NAME, name.c_str(), offset);
        }
        else
        {
            const std::string pageBlobUri = GetRestUriFromSasUri(m_blobContainerSasUri, name);
            CreateCloudPageBlobObject(pageBlobUri);

            offset = 0;
            m_continuationReadOffset = 0;
            m_continuationBlobName = name;

            m_bufferedWriteSize = 0;
            m_ptrWriteBuffer.reset(new char[Blob::BlobPageSize]);
            if (!m_ptrWriteBuffer.get())
            {
                DebugPrintf(SV_LOG_ERROR, "%s: for file %s failed to allocate memory.\n",
                    FUNCTION_NAME, name.c_str());
                throw ERROR_EXCEPTION << "failed to allocate memory for buffering non-aligned write.";
            }
            memset(m_ptrWriteBuffer.get(), 0, Blob::BlobPageSize);
        }

        if (!m_PtrPageBlob)
        {
            DebugPrintf(SV_LOG_ERROR, "%s: for file %s failed to initialize CloudPageBlob.\n",
                FUNCTION_NAME, name.c_str());
            throw ERROR_EXCEPTION << "CloudPageBlob object not initialized.";
        }

        std::size_t nLength = dataSize;

        if (!offset)
        {
            m_readFileSize = 0;

            std::size_t blobContentLength = 0, bytesRead = 0;
            std::size_t bytesRequested = (nLength >= MAX_READ_SIZE_FOR_AZURE_PAGE_BLOB) ? MAX_READ_SIZE_FOR_AZURE_PAGE_BLOB : nLength;

            ReadWithSize(offset, data, bytesRequested, bytesRead, blobContentLength);

            m_readFileSize = blobContentLength;

            DebugPrintf(s_logLevel,
                "%s: name %s file size is %u.\n",
                FUNCTION_NAME, name.c_str(), blobContentLength);

            offset += bytesRead;
            bytesReturned += bytesRead;
            data += bytesRead;
            nLength -= bytesRead;

            // when requesting size more than the blob content length,
            // bytesRead is aligned to page size. So adjust to actual bytes returned
            if (offset >= m_readFileSize)
            {
                nLength = 0;
                bytesReturned -= (offset- m_readFileSize);
            }
        }

        while (nLength > 0)
        {
            std::size_t bytesRead = 0;

            std::size_t bytesRequested = (nLength >= MAX_READ_SIZE_FOR_AZURE_PAGE_BLOB) ? MAX_READ_SIZE_FOR_AZURE_PAGE_BLOB : nLength;

            Read(offset, data, bytesRequested, bytesRead);

            data += bytesRead;
            bytesReturned += bytesRead;
            offset += bytesRead;
            nLength -= bytesRead;

            // when EOF is reached and requested more than the blob content length,
            // bytesRead is aligned to page size. So adjust to actual bytes returned
            if (offset >= m_readFileSize)
            {
                nLength = 0;
                bytesReturned -= (offset - m_readFileSize);
                DebugPrintf(s_logLevel, "%s: total bytes read %u, remaining size %u file size %u offset %u.\n",
                    FUNCTION_NAME, bytesReturned, nLength, m_readFileSize, offset);
                break;
            }
        }

        DebugPrintf(s_logLevel, "EXITED %s: total bytes read %u, remaining size %u.\n",
            FUNCTION_NAME, bytesReturned, nLength);

        ResponseData responseData;
        clientCode = responseData.responseCode = (offset >= m_readFileSize) ? CLIENT_RESULT_OK :CLIENT_RESULT_MORE_DATA;

        setResponseData(responseData);
    }
    catch (const std::exception &e)
    {
        throw ERROR_EXCEPTION << FUNCTION_NAME << " failed"
            << ", getName: " << name
            << ", dataSize: " << dataSize
            << ", error: " << e.what();
    }
    catch (...)
    {
        throw ERROR_EXCEPTION << FUNCTION_NAME << " failed with an unknown exception"
            << ", getName: " << name
            << ", dataSize: " << dataSize;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return clientCode;
}

/// \brief issue get file request to get a remote file to a local file
///
/// \return
/// \li \c CLIENT_RESULT_OK       : on success
/// \li \c CLIENT_RESULT_NOT_FOUND: remote file not found
///
/// \exception ERROR_EXCEPTION on failure
///
/// \note
/// \li \c always uses binary mode
///
ClientCode AzureBlobClient::getFile(std::string const& remoteName,  ///< remote file to get
    std::string const& localName)   ///< local file to put data into
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    std::string checksum;
    
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return getFileAndChecksum(remoteName, localName, checksum, false);
}

/// \brief issue get file request to get a remote file to a local file with SHA256 checksum
///
/// \return
/// \li \c CLIENT_RESULT_OK       : on success
/// \li \c CLIENT_RESULT_NOT_FOUND: remote file not found
///
/// \exception ERROR_EXCEPTION on failure
///
/// \note
/// \li \c always uses binary mode
///
ClientCode AzureBlobClient::getFile(std::string const& remoteName,  ///< remote file to get
    std::string const& localName,   ///< local file to put data into
    std::string& checksum)   ///< buffer to return checksum
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s: %u\n", FUNCTION_NAME, m_id);
    
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return getFileAndChecksum(remoteName, localName, checksum, true);
}

ClientCode AzureBlobClient::getFileAndChecksum(std::string const& remoteName,
    std::string const& localName,
    std::string& checksum,
    bool bNeedChecksum)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    boost::unique_lock<boost::recursive_mutex> guard(m_contextLock);

    ClientCode clientCode = CLIENT_RESULT_OK;

    DebugPrintf(s_logLevel, "ENTERED %s: remoteName %s localName %s\n",
        FUNCTION_NAME, remoteName.c_str(), localName.c_str());

    try {

        const std::string pageBlobUri = GetRestUriFromSasUri(m_blobContainerSasUri, remoteName);
        CreateCloudPageBlobObject(pageBlobUri);

        m_continuationBlobName = remoteName;
        std::size_t offset = 0, blobContentLength = 0, bytesReturned = 0;
        std::vector<char> buffer(m_maxBufferSizeBytes, 0);

        ReadWithSize(offset, &buffer[0], m_maxBufferSizeBytes, bytesReturned, blobContentLength);

        m_readFileSize = blobContentLength;

        DebugPrintf(s_logLevel,
            "%s: remoteName %s size is %u.\n",
            FUNCTION_NAME, remoteName.c_str(), blobContentLength);

        FIO::Fio oFio(ExtendedLengthPath::name(localName).c_str(), FIO::FIO_OVERWRITE);

        SHA256_CTX sha256;
        if (bNeedChecksum) SHA256_Init(&sha256);

        std::size_t nLength = blobContentLength;

        if (blobContentLength)
        {
            std::size_t readSize = (blobContentLength < bytesReturned) ? blobContentLength : bytesReturned;
            if (oFio.write(&buffer[0], readSize) < 0)
            {
                throw ERROR_EXCEPTION << "error writing data to local file: " << oFio.errorAsString();
            }

            if (bNeedChecksum) SHA256_Update(&sha256, &buffer[0], readSize);

            nLength -= readSize;
            offset = readSize;
        }

        while (nLength)
        {
            std::size_t bytesReturned = 0;

            const std::size_t bytesRequested = (nLength > m_maxBufferSizeBytes) ? m_maxBufferSizeBytes :
                nLength;

            clientCode = getFile(remoteName, offset, bytesRequested, &buffer[0], bytesReturned);

            if (bytesReturned)
            {
                if (oFio.write(&buffer[0], bytesReturned) < 0)
                {
                    throw ERROR_EXCEPTION << "error writing data to local file: " << oFio.errorAsString();
                }
                offset += bytesReturned;
                nLength -= bytesReturned;

                if (bNeedChecksum) SHA256_Update(&sha256, &buffer[0], bytesReturned);
            }
        }

        if (WRITE_MODE_FLUSH == writeMode() && !oFio.flushToDisk())
        {
            throw ERROR_EXCEPTION << "error flushing data to local file: " << oFio.errorAsString();
        }

        if (bNeedChecksum) {
            unsigned char mac[SHA256_DIGEST_LENGTH];
            SHA256_Final(mac, &sha256);

            std::stringstream ss;
            for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
                ss << std::hex << std::setfill('0') << std::setw(2) << (int)mac[i];
            }
            checksum = ss.str();
        }

        DebugPrintf(s_logLevel, "EXITED %s: remoteName %s clientCode %u\n",
            FUNCTION_NAME, remoteName.c_str(), clientCode);
    }
    catch (const std::exception &e)
    {
        throw ERROR_EXCEPTION << FUNCTION_NAME << " failed"
            << ", remoteName: " << remoteName
            << ", localName: " << localName
            << ", error: " << e.what();
    }
    catch (...)
    {
        throw ERROR_EXCEPTION << FUNCTION_NAME << " failed with an unknown exception"
            << ", remoteName: " << remoteName
            << ", localName: " << localName;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return clientCode;
}

/// \brief issue delete file request
///
///  see documenation for FindDelete for complete details
///
/// \return
/// \li \c CLIENT_RESULT_OK       : on success
/// \li \c CLIENT_RESULT_NOT_FOUND: remote file not found
/// \exception ERROR_EXCEPTION on failure
ClientCode AzureBlobClient::deleteFile(std::string const& names,    ///< semi-colon seprated list of files and/or dirs to delete
    int mode    ///< delete mode to use (FILES_ONLY, RECURSE_DIRS, or can combine both using logical-or ('|'))
)
{
    return deleteFile(names, std::string(), mode);
}

/// \brief issue delete file request
///
/// see documenation for FindDelete for complete details
///
/// \return
/// \li \c CLIENT_RESULT_OK       : on success
/// \li \c CLIENT_RESULT_NOT_FOUND: remote file not found
/// \exception ERROR_EXCEPTION on failure
ClientCode AzureBlobClient::deleteFile(std::string const& names,    ///< semi-colon seprated list of files and/or dirs to delete
    std::string const& fileSpec,         ///< file spec to use to match file/dir names when name in names is a dir
    int mode    ///< delete mode to use (FILES_ONLY, RECURSE_DIRS, or can combine both using logical-or ('|'))
)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s: %u\n", FUNCTION_NAME, m_id);
    boost::unique_lock<boost::recursive_mutex> guard(m_contextLock);

    DebugPrintf(s_logLevel, "ENTERED %s: names %s.\n", FUNCTION_NAME, names.c_str());

    ClientCode clientCode = CLIENT_RESULT_OK;

    try
    {
        typedef boost::tokenizer<boost::char_separator<char> > tokenizer_t;
        boost::char_separator<char> sep(";");
        tokenizer_t blobNameTokens(names, sep);
        tokenizer_t::iterator blobNameTokensItr(blobNameTokens.begin());
        for (blobNameTokensItr; blobNameTokensItr != blobNameTokens.end(); ++blobNameTokensItr)
        {
            std::string fileName = *blobNameTokensItr;
            boost::trim(fileName);
            if (fileName.empty())
            {
                continue;
            }

            const std::string pageBlobUri = GetRestUriFromSasUri(m_blobContainerSasUri, fileName);
            DebugPrintf(s_logLevel, "%s: deleting file %s.\n", FUNCTION_NAME, fileName.c_str());
            CreateCloudPageBlobObjectForDelete(pageBlobUri);

            if (!m_PtrPageBlobForDelete->Delete())
            {
                m_lastError = m_PtrPageBlobForDelete->GetLastError();
                throw ERROR_EXCEPTION << FUNCTION_NAME
                    << " failed to delete blob file " << fileName
                    << ", error: " << m_lastError
                    << ", status code: " << m_PtrPageBlobForDelete->GetLastHttpStatusCode()
                    << ", response data: " << m_PtrPageBlobForDelete->GetLastHttpResponseData();
            }

            m_continuationBlobName.clear();
            m_continuationWriteOffset = 0;
            m_writeDataSize = 0;
        }
    }
    catch (const std::exception &e)
    {
        throw ERROR_EXCEPTION << FUNCTION_NAME << " failed"
            << " names: " << names
            << ", fileSpec: " << fileSpec
            << ", mode: 0x" << std::hex << mode << std::dec
            << ", error: " << e.what();
    }
    catch (...)
    {
        throw ERROR_EXCEPTION << FUNCTION_NAME << " failed with an unknown exception"
            << ", names: " << names
            << ", fileSpec: " << fileSpec
            << " mode: 0x" << std::hex << mode << std::dec;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return clientCode;
}

#endif // AZUREBLOBCLIENT_CPP
