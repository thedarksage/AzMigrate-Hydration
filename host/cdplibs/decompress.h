//
// Copyright (c) 2006 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : decompress.h
//
// Description: 
//

#ifndef INM_DECOMPRESS__H
#define INM_DECOMPRESS__H

#include <string>
#include "svtypes.h"
#include "zutil.h"

class GzipUncompress
{
public:

	GzipUncompress() {}
	~GzipUncompress() {}
	bool UnCompress(const char * pszInBuffer, const unsigned long & ulInBufferLen, char ** ppOutBuffer, unsigned long& ulOutBufferLen);
	bool UnCompress(const char* pszInBuffer,const unsigned long ulInBufferLen, const std::string & path);
	bool UnCompress(const std::string & inpath, const std::string & outpath);
	bool Verify(const std::string & path);

protected:

private:

};

#endif
