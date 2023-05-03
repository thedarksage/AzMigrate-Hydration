
//
//  File: azureblobstream.h
//
//  Description:
//

#ifndef AZUREBLOCKBLOBSTREAM_H
#define AZUREBLOCKBLOBSTREAM_H

#include <string>

#include "azureblobstream.h"
#include "CloudBlockBlob.h"

using namespace AzureStorageRest;

class AzureBlockBlobStream :
    public AzureBlobStream
{
private:
    boost::shared_ptr<CloudBlockBlob>   m_ptrBlockBlob;
    const uint32_t                      m_maxParallelUploadThreads;
    const uint64_t                      m_azureBlockBlobMaxWriteSize;
    const uint64_t                      m_azureBlockBlobParallelUploadSize;

protected:
    virtual int SetMetadata(const metadata_t &metadata);

public:
    AzureBlockBlobStream(std::string& blobContainerSasUri,
        const uint32_t& timeout = 300,
        const uint64_t azureBlockBlobParallelUploadSize = AZURE_BLOCK_BLOB_PARALLEL_UPLOAD_SIZE,
        const uint64_t azureBlockBlobMaxWriteSize = AZURE_BLOCK_BLOB_MAX_WRITE_SIZE,
        const uint32_t maxParallelUploadThreads = 0,
        const bool bEnableChecksums = false);
    
    virtual ~AzureBlockBlobStream();
    
    virtual int Write(const char*, unsigned long int, const std::string&, bool bMoreData, COMPRESS_MODE compressMode, bool createDirs = false,
        bool bIsFull = false);

    /// \brief write to azure page blob with page-aligned size (512-bytes)
    int Write(const char* buffer, unsigned long int length, unsigned long int offset, const std::string &sasUri);

    virtual int DeleteFile(std::string const& names, int mode = FindDelete::FILES_ONLY);
};

#endif // AZUREBLOCKBLOBSTREAM_H
