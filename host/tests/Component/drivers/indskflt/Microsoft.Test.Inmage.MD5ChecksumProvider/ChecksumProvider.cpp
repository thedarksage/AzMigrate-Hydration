// Microsoft.Test.Inmage.MD5ChecksumProvider.cpp : Defines the exported functions for the DLL application.
//

#include "ChecksumProvider.h"

CInmageMD5ChecksumProvider::CInmageMD5ChecksumProvider(std::list<ExtentInfo> ignoreExts, ILogger *pLogger):
m_pLogger(pLogger),
m_ignoreExts(ignoreExts)
{
	unsigned long long ullIgnoreBlockStartOffset;
	unsigned long long ullIgnoreBlockEndOffset;

	std::list<ExtentInfo>::iterator it;
	for (it = m_ignoreExts.begin(); m_ignoreExts.end() != it; it++)
	{
		ullIgnoreBlockStartOffset = it->m_liStartOffset;
		ullIgnoreBlockEndOffset = it->m_liEndOffset;

		for (unsigned long long offset = ullIgnoreBlockStartOffset; offset <= ullIgnoreBlockEndOffset; offset += FS_CLUSTER_SIZE)
		{
			unsigned long long chunkOffset = ROUND_OFF(offset, CHUNK_SIZE);
			unsigned long long bitno = (offset - chunkOffset) / FS_CLUSTER_SIZE;
			m_chunkBitmap[chunkOffset].set(bitno);
		}
	}

}

void CInmageMD5ChecksumProvider::GetCheckSum(IBlockDevice* psrcDevice, std::ofstream& outputFile)
{
	unsigned long long size = psrcDevice->GetDeviceSize();
	unsigned long long ullBytesRemaining = size;

	unsigned long long ullCurrentStartOffset = 0;
	unsigned long long ullCurrentEndOffset = 0;
	std::string consoleout;
	
	vector<unsigned char> buffer(CHUNK_SIZE);
	vector<unsigned char> digest(16);

	m_pLogger->LogInfo("Entering: %s Disk Id: %s", __FUNCTION__, psrcDevice->GetDeviceId().c_str());

	psrcDevice->SeekFile(BEGIN, 0);

	double chksumTime = 0.0;
	double readTime = 0.0;

	boost::timer	chksumTimer;

	while (ullBytesRemaining > 0)
	{
		
		INM_MD5_CTX	ctx;
		INM_MD5Init(&ctx);
		unsigned long  bytesRead;
		unsigned long long bytesToRead = (ullBytesRemaining > CHUNK_SIZE) ? CHUNK_SIZE : ullBytesRemaining;
		ullBytesRemaining -= bytesToRead;

		chksumTimer.restart();
		psrcDevice->Read(&buffer[0], bytesToRead, bytesRead);
		readTime += chksumTimer.elapsed();

		chksumTimer.restart();

		ullCurrentEndOffset += bytesRead;

		if (m_chunkBitmap.end() != m_chunkBitmap.find(ullCurrentStartOffset))
		{
			for (size_t i = 0; i < m_chunkBitmap[ullCurrentStartOffset].size(); i++)
			{
				if (m_chunkBitmap[ullCurrentStartOffset].test(i)){
					memset(&buffer[(FS_CLUSTER_SIZE)* i], 0, FS_CLUSTER_SIZE);
				}
			}
		}

		INM_MD5Update(&ctx, &buffer[0], bytesRead);
		INM_MD5Final(&digest[0], &ctx);
		
		chksumTime += chksumTimer.elapsed();

		outputFile.write((char*)&digest[0], 16);

		ullCurrentStartOffset = ullCurrentEndOffset;
	}

	m_pLogger->LogInfo("%s: Time spent on reading source: %f secs Calculating Chksum: %f secs",
						__FUNCTION__,
						readTime,
						chksumTime);

	m_pLogger->LogInfo("Exiting: %s", __FUNCTION__);
}
