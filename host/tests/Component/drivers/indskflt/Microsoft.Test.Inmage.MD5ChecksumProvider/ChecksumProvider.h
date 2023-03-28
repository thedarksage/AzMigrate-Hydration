#ifndef _MICROSOFTTESTINMAGECHECKSUMPROVIDER_H_
#define _MICROSOFTTESTINMAGECHECKSUMPROVIDER_H_

#include "Logger.h"
#include "IPlatformAPIs.h"
#include "IBlockDevice.h"
#include "inm_md5.h"

#include <bitset>
#include <ctime>

#include "boost/timer.hpp"

#include <string>
#include <sstream>
#include <fstream>
#include <list>
#include <map>
#include <bitset>

#define CHUNK_SIZE			(4*1024*1024)
#define FS_CLUSTER_SIZE		(4*1024)
#define bitmapsize			((CHUNK_SIZE) / (FS_CLUSTER_SIZE))

#ifdef SV_WINDOWS
#ifdef MICROSOFTTESTINMAGECHECKSUMPROVIDER_EXPORTS
#define MICROSOFTTESTINMAGECHECKSUMPROVIDER_API __declspec(dllexport)
#else
#define MICROSOFTTESTINMAGECHECKSUMPROVIDER_API __declspec(dllimport)
#endif
#else
#define MICROSOFTTESTINMAGECHECKSUMPROVIDER_API
#endif

class MICROSOFTTESTINMAGECHECKSUMPROVIDER_API IInmageChecksumProvider {
public:
	virtual void GetCheckSum(IBlockDevice* psrcDevice, std::ofstream& outputFile) = 0;
};


class MICROSOFTTESTINMAGECHECKSUMPROVIDER_API CInmageMD5ChecksumProvider : public IInmageChecksumProvider {
private:
	std::list<ExtentInfo>									m_ignoreExts;
	std::map<unsigned long long, std::bitset<bitmapsize> >	m_chunkBitmap;
	ILogger*												m_pLogger;

public:
	CInmageMD5ChecksumProvider(list<ExtentInfo> ignoreExts, ILogger* pLogger);
	void GetCheckSum(IBlockDevice* psrcDevice, std::ofstream& outputFile);
};

#endif

