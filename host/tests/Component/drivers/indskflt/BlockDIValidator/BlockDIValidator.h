#ifndef _BLOCKDI_VALIDATOR_H
#define _BLOCKDI_VALIDATOR_H

#include "Logger.h"
#include "PlatformAPIs.h"
#include "IBlockDevice.h"
#include <Wincrypt.h>

// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the BLOCKDIVALIDATOR_EXPORTS
// symbol defined on the command line. This symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// BLOCKDIVALIDATOR_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifdef BLOCKDIVALIDATOR_EXPORTS
#define BLOCKDIVALIDATOR_API __declspec(dllexport)
#else
#define BLOCKDIVALIDATOR_API __declspec(dllimport)
#endif

#define SAFE_FREE(x) if (NULL != x) {free(x); x = NULL;}
#define HASH_LENGTH     16

// This class is exported from the BlockDIValidator.dll
class BLOCKDIVALIDATOR_API CBlockDIValidator 
{
private:
	void*			m_sourceBuffer;
	void*			m_targetBuffer;
	IBlockDevice*	m_sourceDevice;
	IBlockDevice*	m_targetDevice;
	SV_ULONG			m_dwReadSize;
    HCRYPTPROV      m_hProv = 0;
    HCRYPTHASH      m_hHash = 0;
    BYTE            m_sourceContentHash[HASH_LENGTH];
    BYTE            m_targetContentHash[HASH_LENGTH];
    ILogger*        m_pLogger;
	bool Compare(void* source, void* target, SV_ULONG dwBlockSize);
public:
	CBlockDIValidator(IBlockDevice* sourceDevice, IBlockDevice *targetDevice, ILogger* pLogger);
	~CBlockDIValidator();
	bool Validate();
};

#endif