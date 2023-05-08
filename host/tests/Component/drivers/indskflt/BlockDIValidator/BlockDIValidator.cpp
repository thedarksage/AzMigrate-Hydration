// BlockDIValidator.cpp : Defines the exported functions for the DLL application.
//

#include "BlockDIValidator.h"
#include <stdlib.h>
#include <exception>
#include "InmageTestException.h"

#include <wincrypt.h>
#include <string>

using namespace std;

CBlockDIValidator::CBlockDIValidator(IBlockDevice* sourceDevice, IBlockDevice *targetDevice, ILogger* pLogger) :
m_sourceDevice(sourceDevice), m_targetDevice(targetDevice), m_pLogger(pLogger)
{
    // Get handle to the crypto provider
    if (!CryptAcquireContext(&m_hProv,
        NULL,
        NULL,
        PROV_RSA_FULL,
        CRYPT_VERIFYCONTEXT | CRYPT_NEWKEYSET))
        //CRYPT_VERIFYCONTEXT))
    {
        SV_ULONG dwErr = GetLastError();
        throw CBlockDIValidatorException("CryptAcquireContext failed err: 0x%x", dwErr);
    }

    if (sourceDevice->GetDeviceSize() != targetDevice->GetDeviceSize())
    {
        throw CBlockDIValidatorException("Source device size 0x%x is not equal to target device size 0x%x", 
                                                            sourceDevice->GetDeviceSize(), 
                                                            targetDevice->GetDeviceSize());
    }

    SV_ULONG dwSourceBlockSize = sourceDevice->GetCopyBlockSize();
    SV_ULONG dwTargetBlockSize = targetDevice->GetCopyBlockSize();

    if (dwSourceBlockSize != dwTargetBlockSize)
    {
        throw CBlockDIValidatorException("Source device block size 0x%x is not equal to target device block size 0x%x",
                                                        dwSourceBlockSize,
                                                        dwTargetBlockSize);
    }

    m_dwReadSize = sourceDevice->GetMaxRwSizeInBytes();

    m_sourceBuffer = (void*)malloc(m_dwReadSize);
    if (NULL == m_sourceBuffer)
    {
        throw CBlockDIValidatorException("CBlockDIValidator::CBlockDIValidator failed to allocate memory for m_sourceBuffer");
    }

    m_targetBuffer = (void*)malloc(m_dwReadSize);
    if (NULL == m_targetBuffer)
    {
        free(m_sourceBuffer);
        new CBlockDIValidatorException("CBlockDIValidator::CBlockDIValidator failed to allocate memory for m_targetBuffer");
    }
}

CBlockDIValidator::~CBlockDIValidator()
{
    if (m_hProv)
    {
        CryptReleaseContext(m_hProv, 0);
    }

    SAFE_FREE(m_sourceBuffer);
    SAFE_FREE(m_targetBuffer);
}

bool CBlockDIValidator::Compare(void* source, void* target, SV_ULONG dwBlockSize)
{
    HCRYPTHASH  hSourceHash;
    HCRYPTHASH  hTargetHash;

    PBYTE srcPtr = (PBYTE)source;
    PBYTE tgtPtr = (PBYTE)target;

    if (!CryptCreateHash(m_hProv, CALG_MD5, 0, 0, &hSourceHash))
    {
        throw CBlockDIValidatorException("CryptAcquireContext failed err: 0x%x", GetLastError());
    }

    if (!CryptHashData(hSourceHash, srcPtr, dwBlockSize, 0))
    {
        CryptDestroyHash(hSourceHash);
        throw CBlockDIValidatorException("CryptHashData failed with err=0x%x", GetLastError());
    }

    if (!CryptCreateHash(m_hProv, CALG_MD5, 0, 0, &hTargetHash))
    {
        throw CBlockDIValidatorException("CryptAcquireContext failed err: 0x%x", GetLastError());
    }

    if (!CryptHashData(hTargetHash, tgtPtr, dwBlockSize, 0))
    {
        CryptDestroyHash(hTargetHash);
        throw CBlockDIValidatorException("CryptHashData failed with err=0x%x", GetLastError());
    }

    SV_ULONG cbHash = HASH_LENGTH;
    if (!CryptGetHashParam(hSourceHash, HP_HASHVAL, m_sourceContentHash, &cbHash, 0))
    {
        CryptDestroyHash(hSourceHash);
        CryptDestroyHash(hTargetHash);
        throw CBlockDIValidatorException("CryptGetHashParam Err: 0x%x", GetLastError());

    }

    if (!CryptGetHashParam(hTargetHash, HP_HASHVAL, m_targetContentHash, &cbHash, 0))
    {
        CryptDestroyHash(hSourceHash);
        CryptDestroyHash(hTargetHash);
        throw CBlockDIValidatorException("CryptGetHashParam Err: 0x%x", GetLastError());

    }

    if (0 != memcmp(m_sourceContentHash, m_targetContentHash, cbHash))
    {
        CryptDestroyHash(hSourceHash);
        CryptDestroyHash(hTargetHash);
        return false;
    }

    CryptDestroyHash(hSourceHash);
    CryptDestroyHash(hTargetHash);
    return true;
}

// This is the constructor of a class that has been exported.
// see BlockDIValidator.h for the class definition
bool CBlockDIValidator::Validate()
{
    m_pLogger->LogInfo("Enter %s: %s", __FUNCTION__, m_sourceDevice->GetDeviceId());

    ULONGLONG    ullCurrentOffset = 0;
    ULONGLONG    ullMaxSize = m_sourceDevice->GetDeviceSize();
    SV_ULONG        dwBytes;

    SV_ULONG       dwBytesToRead;
    
    //Reset device to sector 0
    m_sourceDevice->SeekFile(BEGIN, 0);
    m_targetDevice->SeekFile(BEGIN, 0);

    while (ullCurrentOffset < ullMaxSize)
    {
        dwBytesToRead = (ullMaxSize - ullCurrentOffset) > m_dwReadSize ? m_dwReadSize : (SV_ULONG) (ullMaxSize - ullCurrentOffset);

        m_sourceDevice->Read(m_sourceBuffer, dwBytesToRead, dwBytes);
        m_targetDevice->Read(m_targetBuffer, dwBytesToRead, dwBytes);

        if (!Compare(m_sourceBuffer, m_targetBuffer, dwBytesToRead))
        {
            m_pLogger->LogInfo("%s: Validation failed at offset: 0x%I64x size: 0x%x", __FUNCTION__, ullCurrentOffset, dwBytesToRead);
            m_pLogger->Flush();
            return false;
        }
        ullCurrentOffset += dwBytesToRead;
    }

    m_pLogger->Flush();
    return true;
}
