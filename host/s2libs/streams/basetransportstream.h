//#pragma once

#ifndef BASE_TRANSPORT_STREAM__H
#define BASE_TRANSPORT_STREAM__H

#include <string>
#include <list>

#include "inputoutputstream.h"
#include "genericstream.h"
#include "compressmode.h"
#include "finddelete.h"
#include "ResponseData.h"

typedef std::list<std::string> ListString;

class BaseTransportStream :
    public GenericStream, public InputOutputStream
{
public:
    BaseTransportStream()
    { 
	}
    virtual ~BaseTransportStream() {
    }

    virtual int Write(const char*, unsigned long int, const std::string&, bool bMoreData, COMPRESS_MODE compressMode, bool createDirs = false, 
                      bool bIsFull = false) = 0;
    virtual int Write(const std ::string& localFile, const std ::string& targetFile, COMPRESS_MODE compressMode, bool createDirs = false)=0;
    virtual int Write(const std ::string& localFile, const std ::string& targetFile, COMPRESS_MODE compressMode, const std::string& renameFile, std::string const& finalPaths = std::string(), bool createDirs = false) =0;

    virtual int Open(const STREAM_MODE) = 0;

    virtual int Close() = 0;
    virtual int Abort(char const* pData) = 0;

    virtual int DeleteFile(std::string const& names, int mode = FindDelete::FILES_ONLY) = 0; 
    virtual int DeleteFile(std::string const& names, std::string const& fileSpec, int mode = FindDelete::FILES_ONLY) = 0;
        
    virtual int Rename(const std::string& sOldFileName, const std::string& sNewFileName, COMPRESS_MODE compressMode, std::string const& finalPaths = std::string())= 0;

    virtual int DeleteFiles(const ListString& DeleteFileList) = 0;
    virtual int Write(const void*,const unsigned long int) = 0;
    virtual bool NeedToDeletePreviousFile(void) =0;
    virtual void SetNeedToDeletePreviousFile(bool bneedtodeleteprevfile)= 0;
    virtual int Read(void*,const unsigned long int) = 0;
    virtual void SetSizeOfStream(unsigned long int size) = 0;    
    virtual bool heartbeat(bool forceSend=false) = 0 ;
    virtual bool NeedAlignedBuffers() { return false; }
    
    virtual uint64_t AlignedBufferSize() { return 0; }
    virtual void SetHttpProxy(const std::string& address, const std::string& port, const std::string& bypasslist) {}
    virtual uint32_t GetLastError() { return 0; }
    virtual bool UpdateTransportSettings(const std::string& dest) { return true; }
    virtual void SetTransportTimeout(SV_ULONGLONG timeout) {};

    virtual void SetDiskId(const std::string & diskId) {};
    virtual void SetFileType(int filetype) {};
    virtual void SetAppendSystemTimeUtcOnRename(bool appendTimeOn) {};

    virtual ResponseData GetResponseData() const { return ResponseData(); }

protected:
    

private:

};

#endif
