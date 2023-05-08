
//
//  File: AzurePageBlobStream.h
//
//  Description:
//

#ifndef AZUREPAGEBLOBSTREAM_H
#define AZUREPAGEBLOBSTREAM_H

#include <string>

#include "azureblobstream.h"

#define AZURE_PAGE_BLOB_PAGE_SIZE   512

using namespace AzureStorageRest;

class AzurePageBlobStream :
    public AzureBlobStream
{
private:
    uint64_t        m_continuationWriteOffset;
    
    bool            m_bProcessWriteFailed;

    std::vector<boost::shared_ptr<CloudPageBlob> >  m_vPtrPageBlob;

private:
    int Write(uint64_t offset, const char* buffer, uint64_t length);

    void ProcessWrite(uint64_t offset, const char* buffer, uint64_t length, uint8_t threadIndex);

protected:
    virtual int SetMetadata(const metadata_t &metadata);

public:
    AzurePageBlobStream(std::string& blobContainerSasUri,
        const uint32_t& timeout = 300,
        const bool bEnableChecksums = false);

    virtual ~AzurePageBlobStream();
    
    virtual int Write(const char*, unsigned long int, const std::string&, bool bMoreData, COMPRESS_MODE compressMode, bool createDirs = false,
        bool bIsFull = false);

    /// \brief write to azure page blob with page-aligned size (512-bytes)
    virtual int Write(const char* buffer, unsigned long int length, unsigned long int offset, const std::string& sasUri);

    virtual bool NeedAlignedBuffers() { return true; }

    virtual uint64_t AlignedBufferSize() { return AZURE_PAGE_BLOB_PAGE_SIZE; }
};

#endif // AZUREPAGEBLOBSTREAM_H
#pragma once
