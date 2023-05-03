
//
//  File: dummytransportstream.h
//
//  Description:
//

#ifndef DUMMYTRANSPORTSTREAM_H
#define DUMMYTRANSPORTSTREAM_H

#include <string>

#include "transport_settings.h"
#include "basetransportstream.h"
#include "compressmode.h"

class DummyTransportStream :
    public BaseTransportStream
{
public:
    DummyTransportStream();
    virtual ~DummyTransportStream() ;
    virtual int Write(const char*, unsigned long int, const std::string&, bool bMoreData, COMPRESS_MODE compressMode, bool createDirs = false, 
                      bool bIsFull = false);
    virtual int Write(const std ::string& localFile, const std ::string& targetFile, COMPRESS_MODE compressMode, bool createDirs = false);
    virtual int Write(const std ::string& localFile, const std ::string& targetFile, COMPRESS_MODE compressMode,
                      const std::string& renameFile, std::string const& finalPaths = std::string(), bool createDirs = false);
    virtual int Open(const STREAM_MODE);
    virtual int Close();
    virtual int Abort(char const* pData);
    virtual int DeleteFile(std::string const& names, int mode = FindDelete::FILES_ONLY); 
    virtual int DeleteFile(std::string const& names, std::string const& fileSpec, int mode = FindDelete::FILES_ONLY);
    virtual int Rename(const std::string& sOldFileName, const std::string& sNewFileName, COMPRESS_MODE compressMode, std::string const& finalPaths = std::string());
    virtual int DeleteFiles(const ListString& DeleteFileList);
    virtual int Write(const void*,const unsigned long int);
    virtual bool NeedToDeletePreviousFile(void);
    virtual void SetNeedToDeletePreviousFile(bool bneedtodeleteprevfile);
    virtual int Read(void*,const unsigned long int);
    virtual void SetSizeOfStream(unsigned long int);
    virtual bool heartbeat(bool forceSend=false);
};

#endif // DUMMYTRANSPORTSTREAM_H
