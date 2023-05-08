/*
* Copyright (c) 2005 InMage
* This file contains proprietary and confidential information and
* remains the unpublished property of InMage. Use, disclosure,
* or reproduction is prohibited except as permitted by express
* written license aggreement with InMage.
*
* File       : dummytransportstream.cpp
*
* Description: 
*/
#include <string>
#include <stdexcept>
#include "dummytransportstream.h"
#include "error.h"


DummyTransportStream::DummyTransportStream()
{
}


DummyTransportStream::~DummyTransportStream()
{
}


int DummyTransportStream::Write(const char* sData, unsigned long int uliDataLen, const std::string& sDestination, bool bMoreData, COMPRESS_MODE compressMode, bool createDirs, bool bIsFull)
{
    return SV_SUCCESS;
}


int DummyTransportStream::Write(const std::string& localFile, const std::string& targetFile, COMPRESS_MODE compressMode, const std::string& renameFile, std::string const& finalPaths, bool createDirs)
{
    return SV_SUCCESS;
}


int DummyTransportStream::Write(const std::string& localFile, const std::string& targetFile, COMPRESS_MODE compressMode, bool createDirs)
{
    return SV_SUCCESS;
}


int DummyTransportStream::Open(STREAM_MODE Mode)
{
    return SV_SUCCESS;
}


int DummyTransportStream::Close()
{
    return SV_SUCCESS;
}


int DummyTransportStream::Abort(char const* pData)
{
    return SV_SUCCESS;
}


int DummyTransportStream::DeleteFile(std::string const& names, std::string const& fileSpec, int mode)
{
    return SV_SUCCESS;
}


int DummyTransportStream::DeleteFile(std::string const& names, int mode)
{
    return SV_SUCCESS;
}


int DummyTransportStream::Rename(const std::string& sOldFileName,const std::string& sNewFileName, COMPRESS_MODE compressMode, std::string const& finalPaths)
{
    return SV_SUCCESS;
}


int DummyTransportStream::DeleteFiles(const ListString& DeleteFileList)
{
    return SV_SUCCESS;
}           


int DummyTransportStream::Write(const void*,const unsigned long int) 
{
    throw std::runtime_error("not implemented");
}


bool DummyTransportStream::NeedToDeletePreviousFile(void)
{
    return false;
}


void DummyTransportStream::SetNeedToDeletePreviousFile(bool bneedtodeleteprevfile)
{
}


int DummyTransportStream::Read(void*,const unsigned long int) 
{
    throw std::runtime_error("not implemented");
}


void DummyTransportStream::SetSizeOfStream(unsigned long int uliSize)
{
}


bool DummyTransportStream::heartbeat( bool forceSend )
{
    return true;
}
