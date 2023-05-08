
//
//  File: memorystream.h
//
//  Description:
//

#ifndef MEMORYSTREAM_H
#define MEMORYSTREAM_H

#ifndef MEMORY_STREAM__H
#define MEMORY_STREAM__H

#include <string>
#include <stdexcept>

#include "transport_settings.h"
#include "basetransportstream.h"
#include "cxtransport.h"
#include "compressmode.h"
#include "cdpapply.h"

class MemoryStream :
    public BaseTransportStream
{
public:
    MemoryStream(TRANSPORT_PROTOCOL protocol,
                             TRANSPORT_CONNECTION_SETTINGS const& transportSettings,
                             const std::string & volumename,
                             const SV_ULONGLONG & source_capacity,
                             const CDP_SETTINGS & settings,
                             DifferentialSync * diffSync);
        

    virtual ~MemoryStream() ;
    
    virtual int Write(const char*, unsigned long int, const std::string&, bool bMoreData, COMPRESS_MODE compressMode, bool createDirs = false, 
                      bool bIsFull = false);
    virtual int Write(const std ::string& localFile, const std ::string& targetFile, COMPRESS_MODE compressMode, bool createDirs = false);
    virtual int Write(const std ::string& localFile, const std ::string& targetFile, COMPRESS_MODE compressMode, const std::string& renameFile, std::string const& finalPaths = std::string(), bool createDirs = false);

    virtual int Open(const STREAM_MODE);
	void init() ;
    virtual int Close();
    virtual int Abort(char const* pData);

    virtual int DeleteFile(std::string const& names, int mode = FindDelete::FILES_ONLY); 
    virtual int DeleteFile(std::string const& names, std::string const& fileSpec, int mode = FindDelete::FILES_ONLY);
        
    virtual int Rename(const std::string& sOldFileName, const std::string& sNewFileName, COMPRESS_MODE compressMode, std::string const& finalPaths = std::string());

    virtual int DeleteFiles(const ListString& DeleteFileList);
    bool NeedToDeletePreviousFile(void)
    {
        return false;
    }
    void SetNeedToDeletePreviousFile(bool bneedtodeleteprevfile)
    {
    }
    virtual int Write(const void*,const unsigned long int) {
        throw std::runtime_error("not implemented");
    }

    virtual int Read(void*,const unsigned long int) {
        throw std::runtime_error("not implemented");
    }
    virtual void SetSizeOfStream(unsigned long int);
	int MemoryLeakIncrease();
	int MemoryLeakDecrease(); 
protected:
    int AllocateMemoryForStream(unsigned long int uliSize);

	int AppendToBuffer(std::string const& remoteName,const char * pszData,unsigned long int uliLength,bool bMoreData);
	int ApplyFullBuffer(std::string const& remoteName,const char * pszData,unsigned long int uliLength,bool bMoreData);

    int AppendToBuffer( std::string const& remoteName, std::string const& localFile ) ;
	int ApplyChanges(const std::string& remoteName, const std::string& localName,const char* pszData=NULL,size_t size=0);
    int ReAllocateMemoryForStream(unsigned long int uliSize);
    bool IsMemoryAvailable(unsigned long int uliSize);
    //void SetSizeOfMemoryStream(unsigned long int uliSize);
    bool IsMemoryAllocated(void);
    bool RenameFile(std::string const& oldName, std::string const& newName);
	std::string GetFileNameToApply(const std::string &difffile);
    virtual bool heartbeat(bool forceSend=false) ;
private:
    typedef int (MemoryStream::*ProcessIncomingBuffer_t)(std::string const& remoteName,const char * pszData,unsigned long int uliLength,bool bMoreData);
    unsigned long int m_uliBufferSize;
    unsigned long int m_uliBufferTopPos;
    TRANSPORT_PROTOCOL m_protocol;

    TRANSPORT_CONNECTION_SETTINGS m_transportSettings;
    bool m_bMemoryAllocated;
    CDPApply::ApplyPtr m_CdpApplyPtr;
    DifferentialSync * m_DiffsyncObj;
    char* m_pszBuffer;
    std::string m_FileName;
    bool m_secure;
    std::string m_TgtVolume;
    SV_ULONGLONG m_SrcCapacity;
    CDP_SETTINGS m_CDPSettings;
    ProcessIncomingBuffer_t m_ProcessIncomingBuffer[NBOOLS];
	static unsigned int MemoryStreamBlocksCount;
	static unsigned long int m_defaultSize ;
	static int m_strictModeEnabled ;
	static std::string m_installpath ;
	static  bool m_init ;
};

#endif



#endif // TRANSPORTSTREAM_H
