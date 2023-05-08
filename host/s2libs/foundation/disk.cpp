///
///  \file disk.cpp
///
///  \brief implementation for disk class
///

#include "disk.h"
#include "logger.h"
#include "portable.h"

#include "boost/shared_array.hpp"
#include "volumeclusterinfo.h"
#include "volumeclusterinfoimp.h"
#include "sharedalignedbuffer.h"

Disk::Disk(const std::string &name, const VolumeSummaries_t *pVolumeSummariesCache)
: File(name, false),
  m_pVolumeSummariesCache(pVolumeSummariesCache),
  m_nameBasedOn(VolumeSummary::SIGNATURE)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

Disk::Disk(const std::string &name, const std::string &displayName, const VolumeSummary::NameBasedOn & nameBasedOn)
: File(name, false),
m_displayName(displayName),
m_nameBasedOn(nameBasedOn)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

Disk::~Disk()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

FeatureSupportState_t Disk::SetFileSystemCapabilities(const std::string &filesystem)
{
	FeatureSupportState_t fs = E_FEATURECANTSAY;

	if (m_pVolumeClusterInformer)
	{
		fs = m_pVolumeClusterInformer->GetSupportState();
	}
	else
	{
		m_sFileSystem = filesystem;
		m_pVolumeClusterInformer.reset(new (std::nothrow) VolumeClusterInformerImp());
		if (m_pVolumeClusterInformer)
		{
			VolumeClusterInformer::VolumeDetails vd(GetHandle(), m_sFileSystem);
			if (m_pVolumeClusterInformer->Init(vd))
			{
				fs = m_pVolumeClusterInformer->GetSupportState();
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR, "could not initialize volume cluster information for volume %s\n",
					GetName().c_str());
			}
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "could not allocate memory for volume cluster information for volume %s\n",
				GetName().c_str());
		}
	}

	return fs;
}

bool Disk::SetNoFileSystemCapabilities(const SV_LONGLONG size, const SV_LONGLONG startoffset)
{
	bool bisset = false;

	if (m_pVolumeClusterInformer)
	{
		SV_UINT minimumclustersizepossible = GetPhysicalSectorSize();
		if (minimumclustersizepossible)
		{
			bisset = m_pVolumeClusterInformer->InitializeNoFsCluster(size, startoffset, minimumclustersizepossible);
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "SetNoFileSystemCapabilities cannot be done because "
				"physical sector size could not be found for volume %s\n",
				GetName().c_str());
		}
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR, "SetNoFileSystemCapabilities cannot be called without "
			"determining file system support capabilities for volume %s\n",
			GetName().c_str());
	}

	return bisset;
}

SV_ULONGLONG Disk::GetNumberOfClusters(void)
{
	SV_ULONGLONG nclus = 0;
	if (m_pVolumeClusterInformer)
	{
		nclus = m_pVolumeClusterInformer->GetNumberOfClusters();
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR, "For volume %s, file system capabilities is uninitialized but asked\n", GetName().c_str());
	}

	return nclus;
}

bool Disk::GetVolumeClusterInfo(VolumeClusterInformer::VolumeClusterInfo &vci)
{
	bool brval = false;
	if (m_pVolumeClusterInformer)
	{
		brval = m_pVolumeClusterInformer->GetVolumeClusterInfo(vci);
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR, "For volume %s, file system capabilities is uninitialized but asked\n", GetName().c_str());
	}

	return brval;
}

void Disk::PrintVolumeClusterInfo(const VolumeClusterInformer::VolumeClusterInfo &vci)
{
	if (m_pVolumeClusterInformer)
	{
		m_pVolumeClusterInformer->PrintVolumeClusterInfo(vci);
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR, "For volume %s, file system capabilities is uninitialized but asked\n", GetName().c_str());
	}
}

SV_UINT Disk::GetPhysicalSectorSize(void)
{
	const int MINBYTES = 512;
	const int MAXBYTES = 4096;
	SV_UINT s = 0;
#ifdef SV_UNIX
	boost::shared_array<char> maxbuf((char*)valloc(MAXBYTES), free); //aligned buffer for direct io.
#else
	boost::shared_array<char> maxbuf(new (std::nothrow) char[MAXBYTES]);
#endif

	if (0 == maxbuf.get())
	{
		DebugPrintf(SV_LOG_ERROR, "Allocating %d bytes to find physical sector size for volume %s failed\n",
			MAXBYTES, GetName().c_str());
		return 0;
	}

	SV_LONGLONG savepos = Tell();
	Seek(0, BasicIo::BioBeg);
	SV_UINT bytesread = FullRead(maxbuf.get(), MINBYTES);
	if (MINBYTES == bytesread)
	{
		s = bytesread;
	}
	else
	{
		DebugPrintf(SV_LOG_DEBUG, "For volume %s, failed to read %d bytes with error %lu to get physical sector size. "
			"Trying to read %d bytes\n", GetName().c_str(), MINBYTES, LastError(), MAXBYTES);
		bytesread = FullRead(maxbuf.get(), MAXBYTES);
		if (MAXBYTES == bytesread)
		{
			s = bytesread;
		}
		else
		{
			DebugPrintf(SV_LOG_ERROR, "For volume %s, failed to read %d bytes with error %lu to get physical sector size. "
				"volume is offline\n", GetName().c_str(), MAXBYTES, LastError());
		}
	}
	Seek(savepos, BasicIo::BioBeg);

	DebugPrintf(SV_LOG_DEBUG, "physical sector size for volume %s is %u\n", GetName().c_str(), s);
	return s;
}
