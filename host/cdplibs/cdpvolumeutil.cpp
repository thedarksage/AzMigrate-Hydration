#include <string>
#include <sstream>
#include <cstring>
#include <algorithm>
#include "svtypes.h"
#include "cdpvolumeutil.h"
#include "simplevolumeapplier.h"
#include "logger.h"
#include "portablehelpers.h"
#include "localconfigurator.h"
#include "file.h"
#include "readinfo.h"
#include "inmsafeint.h"
#include "inmageex.h"


cdp_volume_util::cdp_volume_util()
{
	m_ClusterBitOps[0] = &cdp_volume_util::UnsetClusterBits;
	m_ClusterBitOps[1] = &cdp_volume_util::SetClusterBits;
}

bool cdp_volume_util::IsOffsetLengthCoveredInClusterInfo(const SV_ULONGLONG offsettocheck, const SV_UINT &lengthtocheck, 
                                                         const VolumeClusterInformer::VolumeClusterInfo &vci,
                                                         const SV_UINT &clustersize)
{
    SV_ULONGLONG lengthcovered = vci.m_CountFilled*clustersize;
    SV_ULONGLONG startoffsetcovered = vci.m_Start*clustersize;
    SV_ULONGLONG endoffsetcovered = startoffsetcovered+(lengthcovered?(lengthcovered-1):0);
    SV_ULONGLONG endoffsettocheck = offsettocheck+(lengthtocheck?(lengthtocheck-1):0);

    return (offsettocheck >= startoffsetcovered) && (endoffsettocheck <= endoffsetcovered);
}


bool cdp_volume_util::GetClusterBitmap(const SV_ULONGLONG offset, const SV_UINT &length,
                                       VolumeClusterInformer::VolumeClusterInfo &vci, 
                                       cdp_volume_t *volume,
                                       const SV_UINT &clustersize)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    vci.Reset();
    INM_SAFE_ARITHMETIC(vci.m_Start = InmSafeInt<SV_ULONGLONG>::Type(offset)/clustersize, INMAGE_EX(offset)(clustersize))
    SV_UINT secondopnd;
    INM_SAFE_ARITHMETIC(secondopnd = InmSafeInt<SV_UINT>::Type(clustersize)-1, INMAGE_EX(clustersize))
    SV_ULONGLONG endoffsetinstartcluster;
    INM_SAFE_ARITHMETIC(endoffsetinstartcluster = (clustersize*InmSafeInt<SV_ULONGLONG>::Type(vci.m_Start)) + secondopnd, INMAGE_EX(clustersize)(vci.m_Start)(secondopnd))
    SV_UINT llength = length;

    SV_ULONGLONG endoffsetreqd = offset;
    if (llength)
    {
        endoffsetreqd += (llength-1);
        vci.m_CountRequested = 1;
    }
    if (endoffsetreqd > endoffsetinstartcluster)
    {
        INM_SAFE_ARITHMETIC(llength -= (InmSafeInt<SV_ULONGLONG>::Type(endoffsetinstartcluster)-offset+1), INMAGE_EX(llength)(endoffsetinstartcluster)(offset))
        SV_UINT rem;
        INM_SAFE_ARITHMETIC(rem = InmSafeInt<SV_UINT>::Type(llength)%clustersize, INMAGE_EX(llength)(clustersize))
        SV_UINT add = rem?1:0;
        INM_SAFE_ARITHMETIC(vci.m_CountRequested += ((InmSafeInt<SV_UINT>::Type(llength)/clustersize) + add), INMAGE_EX(vci.m_CountRequested)(llength)(clustersize)(add))
    }

    bool bgot = volume->GetVolumeClusterInfo(vci);
    if (bgot)
    {
        /* DebugPrintf(SV_LOG_DEBUG, "Got cluster bitmap:\n");
        volume->PrintVolumeClusterInfo(vci); */
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return bgot;
}


bool cdp_volume_util::IsAnyClusterUsed(const VolumeClusterInformer::VolumeClusterInfo &vci,
                                       const SV_ULONGLONG offset,
                                       const SV_UINT &length,
                                       const SV_UINT &clustersize)
{
    bool bisused = false;
    SV_ULONGLONG startclusternumber = offset/clustersize;
    SV_UINT c = startclusternumber-vci.m_Start;
    SV_ULONGLONG endoffsetinstartcluster = (clustersize*startclusternumber) + (clustersize-1);
    SV_UINT llength = length;
    SV_UINT nclusters = 0;

    SV_ULONGLONG endoffsetreqd = offset;
    if (llength)
    {
        endoffsetreqd += (llength-1);
        nclusters = 1;
    }
    if (endoffsetreqd > endoffsetinstartcluster)
    {
        llength -= (endoffsetinstartcluster-offset+1);
        nclusters += ((llength/clustersize) + ((llength%clustersize)?1:0));
    }

    for (SV_UINT i = 0; i < nclusters; i++, c++)
    {
        if (vci.m_pBitmap[c / NBITSINBYTE] & (1 << (c % NBITSINBYTE)))
        {
            bisused = true;
            break;
        }
    }
 
    return bisused;
}


SV_UINT cdp_volume_util::GetContinuousUsedClusters(const VolumeClusterInformer::VolumeClusterInfo &vci,
                                                   const SV_ULONGLONG offset,
                                                   const SV_UINT &length,
                                                   const SV_UINT &clustersize)
{
    SV_UINT count = 0;
    SV_ULONGLONG startclusternumber = offset/clustersize;
    SV_UINT c = startclusternumber - vci.m_Start;
    SV_ULONGLONG endoffsetinstartcluster = (clustersize*startclusternumber) + (clustersize-1);
    SV_UINT llength = length;
    SV_UINT nclusters = 0;

    SV_ULONGLONG endoffsetreqd = offset;
    if (llength)
    {
        endoffsetreqd += (llength-1);
        nclusters = 1;
    }
    if (endoffsetreqd > endoffsetinstartcluster)
    {
        llength -= (endoffsetinstartcluster-offset+1);
        nclusters += ((llength/clustersize) + ((llength%clustersize)?1:0));
    }

    for (SV_UINT i = 0; i < nclusters; i++, c++)
    {
        if (vci.m_pBitmap[c / NBITSINBYTE] & (1 << (c % NBITSINBYTE)))
        {
            count++;
        }
        else
        {
            break;
        }
    }
 
    return count;
}


SV_UINT cdp_volume_util::GetContinuousClusters(const VolumeClusterInformer::VolumeClusterInfo &vci,
                                               const SV_ULONGLONG offset,
                                               const SV_UINT &length,
                                               const SV_UINT &clustersize, 
                                               bool &bisused)
{
    SV_ULONGLONG startclusternumber = offset/clustersize;
    SV_UINT c = startclusternumber - vci.m_Start;
    SV_ULONGLONG endoffsetinstartcluster = (clustersize*startclusternumber) + (clustersize-1);
    SV_UINT llength = length;
    SV_UINT nclusters = 0;

    SV_ULONGLONG endoffsetreqd = offset;
    if (llength)
    {
        endoffsetreqd += (llength-1);
        nclusters = 1;
    }
    if (endoffsetreqd > endoffsetinstartcluster)
    {
        llength -= (endoffsetinstartcluster-offset+1);
        nclusters += ((llength/clustersize) + ((llength%clustersize)?1:0));
    }

    bisused = vci.m_pBitmap[c/NBITSINBYTE] & (1<<(c%NBITSINBYTE));
    return GetCountOfContinuousClusters(vci, c, nclusters, bisused);
}


SV_UINT cdp_volume_util::GetCountOfContinuousClusters(const VolumeClusterInformer::VolumeClusterInfo &vci,
                                                      const SV_UINT &clusternumber,
                                                      const SV_UINT &nclusters,
                                                      const bool &bisused)
{
    SV_UINT c = clusternumber;
    SV_UINT count = 0;

    /* NOTE: this is redundant to avoid for
     * check for the first cluster being used or not
     * every time for loop executes whether to 
     * increment count or break */
    if (bisused)
    {
        for (SV_UINT i = 0; i < nclusters; i++, c++)
        {
            if (vci.m_pBitmap[c / NBITSINBYTE] & (1 << (c % NBITSINBYTE)))
            {
                count++;
            }
            else
            {
                break;
            }
        }
    }
    else
    {
        for (SV_UINT i = 0; i < nclusters; i++, c++)
        {
            if (vci.m_pBitmap[c / NBITSINBYTE] & (1 << (c % NBITSINBYTE)))
            {
                break;
            }
            else
            {
                count++;
            }
        }
    }

    return count;
}


SV_UINT cdp_volume_util::GetLengthCovered(const SV_UINT &continuousclusters,
                                          const SV_ULONGLONG offsetasked,
                                          const SV_UINT &lengthasked,
                                          const SV_UINT &clustersize)
{
    SV_UINT startclusternumber;
    INM_SAFE_ARITHMETIC(startclusternumber = InmSafeInt<SV_ULONGLONG>::Type(offsetasked)/clustersize, INMAGE_EX(offsetasked)(clustersize))
    SV_UINT secondopnd;
    INM_SAFE_ARITHMETIC(secondopnd = InmSafeInt<SV_UINT>::Type(clustersize)-1, INMAGE_EX(clustersize))
    SV_ULONGLONG endoffsetinstartcluster;
    INM_SAFE_ARITHMETIC(endoffsetinstartcluster = (InmSafeInt<SV_UINT>::Type(clustersize)*startclusternumber) + secondopnd, INMAGE_EX(clustersize)(startclusternumber)(secondopnd))
    SV_UINT length;
    INM_SAFE_ARITHMETIC(length = (InmSafeInt<SV_ULONGLONG>::Type(endoffsetinstartcluster)-offsetasked+1), INMAGE_EX(endoffsetinstartcluster)(offsetasked))

    INM_SAFE_ARITHMETIC(length += ((InmSafeInt<SV_UINT>::Type(continuousclusters)-1) * clustersize), INMAGE_EX(length)(continuousclusters)(clustersize))
    if (length > lengthasked)
    {
        length = lengthasked;
    }
    return length;
}

            
SV_UINT cdp_volume_util::GetClusterSize(cdp_volume_t &volume, const std::string &fstype,
                                        const SV_ULONGLONG totalsize, const SV_ULONGLONG startoffset,
                                        const E_FS_SUPPORT_ACTIONS &efssupportaction,
                                        std::string &errmsg)
{
    if ((E_SET_NOFS_CAPABILITIES_IFNOSUPPORT == efssupportaction) ||
        (E_SET_NO_CAPABILITIES_IFNOSUPPORT == efssupportaction))
    {
        FeatureSupportState_t st = volume.SetFileSystemCapabilities(fstype);
        DebugPrintf(SV_LOG_DEBUG, "For volume %s, file system %s, feature support state is %s\n",
                                  volume.GetName().c_str(), fstype.c_str(), StrFeatureSupportState[st]);
        if (E_FEATURECANTSAY == st)
        {
            errmsg = "cannot set file system capability for ";
            errmsg += volume.GetName();
            errmsg += ". Feature support state is unknown";
            return 0;
        }

        if (E_FEATURENOTSUPPORTED == st)
        {
            if (E_SET_NOFS_CAPABILITIES_IFNOSUPPORT == efssupportaction)
            {
                if (!volume.SetNoFileSystemCapabilities(totalsize, startoffset))
                {
                    errmsg = "cannot set no file system capability for ";
                    errmsg += volume.GetName();
                    return 0;
                }
            }
            else
            {
                DebugPrintf(SV_LOG_DEBUG, "For volume %s, file system %s, file system capabilities is not supported."
                                          "Also set no file system capabilities not asked for\n",
                                          volume.GetName().c_str(), fstype.c_str());
                return 0;
            }
        }
    }
    else if (E_SET_NOFS_CAPABILITIES == efssupportaction)
    {
		std::string nofs = "";
        FeatureSupportState_t st = volume.SetFileSystemCapabilities(nofs);
        DebugPrintf(SV_LOG_DEBUG, "For volume %s, file system %s, feature support state is %s\n",
                                  volume.GetName().c_str(), nofs.c_str(), StrFeatureSupportState[st]);
        if (E_FEATURECANTSAY == st)
        {
            errmsg = "cannot set file system capability for ";
            errmsg += volume.GetName();
            errmsg += ". Feature support state is unknown";
            return 0;
        }

        if (!volume.SetNoFileSystemCapabilities(totalsize, startoffset))
        {
            errmsg = "cannot set no file system capability for ";
            errmsg += volume.GetName();
            return 0;
        }
    }
    else
    {
        std::stringstream ss;
        ss << efssupportaction;
        errmsg = "cannot set no file system capability: ";
        errmsg += ss.str();
        errmsg += " for volume ";
        errmsg += volume.GetName();
        return 0;
    }

    SV_ULONGLONG nClusters = volume.GetNumberOfClusters();
    if (0 == nClusters)
    {
        errmsg = "number of cluster is zero for volume ";
        errmsg += volume.GetName();
        return 0;
    }

    SV_UINT clustersize;
    INM_SAFE_ARITHMETIC(clustersize = InmSafeInt<SV_ULONGLONG>::Type(totalsize) / nClusters, INMAGE_EX(totalsize)(nClusters))

    return clustersize;
}


bool cdp_volume_util::GetPhysicalSectorSize(const std::string& volumename, SV_UINT& sectorSize)
{
    bool bRet = false ;
    std::stringstream msg;

    cdp_volume_t volume(volumename);
    volume.Open(BasicIo::BioReadExisting | BasicIo::BioShareAll | BasicIo::BioBinary);
    if (volume.Good())
    {
        SV_UINT s = volume.GetPhysicalSectorSize();
        if (s)
        {
            bRet = true;
            sectorSize = s;
        }
    }
    else
    {
        msg << "Failed to open volume: " 
            << volume.GetName()<< " with error: " << volume.LastError();
        DebugPrintf(SV_LOG_ERROR,"%s\n", msg.str().c_str());
    }

    return bRet ;
}


cdp_volume_t *cdp_volume_util::CreateVolume(const std::string volumename)
{
    std::string dev_name(volumename);
    std::string sparsefile;
    LocalConfigurator localConfigurator;
    bool isnewvirtualvolume = false;
    if(!localConfigurator.IsVolpackDriverAvailable() && (IsVolPackDevice(dev_name.c_str(),sparsefile,isnewvirtualvolume)))
    {
        dev_name = sparsefile;
    }

    return new (std::nothrow) cdp_volume_t(dev_name, isnewvirtualvolume);
}


void cdp_volume_util::DestroyVolume(cdp_volume_t *pvolume)
{
    if (pvolume)
    {
        delete pvolume;
    }
}


VolumeApplier *cdp_volume_util::CreateSimpleVolumeApplier(VolumeApplier::VolumeApplierFormationInputs_t inputs)
{
    return new (std::nothrow) SimpleVolumeApplier(inputs);
}


void cdp_volume_util::DestroySimpleVolumeApplier(VolumeApplier *pvolumeapplier)
{
    if (pvolumeapplier)
    {
        delete pvolumeapplier;
    }
}


void cdp_volume_util::CustomizeClusterBitmap(ClusterBitmapCustomizationInfos_t &infos, 
                                             VolumeClusterInformer::VolumeClusterInfo &vci)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
	ClusterBitmapCustomizationInfos_t delinfos;

    for (ClusterBitmapCustomizationInfosIter_t it = infos.begin(); it != infos.end(); it++)
    {
        ClusterBitmapCustomizationInfo_t &c = *it;
        CustomizeClusterBitmap(c, vci);
		if (0 == c.m_Count)
		{
			delinfos.push_back(c);
		}
    }

	for (ClusterBitmapCustomizationInfosIter_t dit = delinfos.begin(); dit != delinfos.end(); dit++)
	{
		infos.erase(std::remove(infos.begin(), infos.end(), *dit), infos.end());
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}


void cdp_volume_util::CustomizeClusterBitmap(ClusterBitmapCustomizationInfo_t &info,
                                             VolumeClusterInformer::VolumeClusterInfo &vci)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
	SV_UINT nclusterstocustomize = 0;
	SV_ULONGLONG endclusterinvci = vci.m_Start+(vci.m_CountFilled?(vci.m_CountFilled-1):0);

	/* TODO: put in code to check if already a range was 
	* covered; with function pointers so that we avoid
	* check ? */
	if ((info.m_Start >= vci.m_Start) && (info.m_Start <= endclusterinvci))
	{
		SV_UINT c = info.m_Start-vci.m_Start;
		SV_ULONGLONG endclusterininfo = info.m_Start+(info.m_Count?(info.m_Count-1):0);
		if (endclusterininfo <= endclusterinvci)
		{
			nclusterstocustomize = info.m_Count;
		}
		else
		{
			nclusterstocustomize = endclusterinvci-info.m_Start+1;
		}
		ClusterBitOp_t op = m_ClusterBitOps[info.m_Set];
		(this->*op)(c, nclusterstocustomize, vci);
		info.m_Start += nclusterstocustomize;
		info.m_Count -= nclusterstocustomize;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}


void cdp_volume_util::SetClusterBits(const SV_UINT &relativeclusternumber, const SV_UINT &count, VolumeClusterInformer::VolumeClusterInfo &vci)
{
    SV_UINT c = relativeclusternumber;
    for (SV_UINT i = 0; i < count; i++, c++)
    {
        vci.m_pBitmap[c / NBITSINBYTE] |= (1 << (c % NBITSINBYTE));
    }
}


void cdp_volume_util::UnsetClusterBits(const SV_UINT &relativeclusternumber, const SV_UINT &count, VolumeClusterInformer::VolumeClusterInfo &vci)
{
    SV_UINT c = relativeclusternumber;
    for (SV_UINT i = 0; i < count; i++, c++)
    {
        vci.m_pBitmap[c / NBITSINBYTE] &= ~(1 << (c % NBITSINBYTE));
    }
}


void cdp_volume_util::FormClusterBitmapCustomizationInfos(const svector_t &filenames, const std::string &filesystem, const bool &recurse, 
                                                          const bool &exclude, ClusterBitmapCustomizationInfos_t &cbcustinfos)
{
    FileClusterInformer::FileClusterRanges_t fileclusterranges;
    
    for (constsvectoriter_t it = filenames.begin(); it != filenames.end(); it++)
    {
        const std::string &filename = *it;
        File f(filename);

        f.Open(BasicIo::BioReadExisting | BasicIo::BioShareAll);
        if (!f.Good())
        {
            DebugPrintf(SV_LOG_ERROR, "File %s supplied for %s, is not present with error %lu\n", 
                                      filename.c_str(), exclude ? "exclusion" : "inclusion", f.LastError());
            continue;
        }

        if (!f.SetFilesystemCapabilities(filesystem))
        {
            DebugPrintf(SV_LOG_ERROR, "For file %s supplied for %s, filesystem %s, could not set capabilities\n",
                                       filename.c_str(), exclude ? "exclusion" : "inclusion", filesystem.c_str());
            continue;
        }

        if (!f.GetFilesystemClusters(recurse, fileclusterranges))
        {
            DebugPrintf(SV_LOG_ERROR, "For file %s supplied for %s, filesystem %s, could not get file system clusters\n",
                                       filename.c_str(), exclude ? "exclusion" : "inclusion", filesystem.c_str());
            continue;
        }
	}

    if (fileclusterranges.m_FeatureSupportState == E_FEATURESUPPORTED)
    {
        for (FileSystem::ConstClusterRangesIter_t it = fileclusterranges.m_ClusterRanges.begin(); it != fileclusterranges.m_ClusterRanges.end(); it++)
        {
            const FileSystem::ClusterRange_t &cr = *it;
            CollectClusterBitmapCustomizationInfo(cbcustinfos, cr, false);
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "cluster bitmap customization infos:\n");
    for (ClusterBitmapCustomizationInfosIter_t it = cbcustinfos.begin(); it != cbcustinfos.end(); it++)
    {
        it->Print();
    }
}


void cdp_volume_util::ClusterBitmapCustomizationInfo::Print(void)
{
    DebugPrintf(SV_LOG_DEBUG, "cluster bitmap customization info:\n");
    DebugPrintf(SV_LOG_DEBUG, "start: " ULLSPEC "\n", m_Start);
    DebugPrintf(SV_LOG_DEBUG, "count: %u\n", m_Count);
    DebugPrintf(SV_LOG_DEBUG, "set: %s\n", STRBOOL(m_Set));
}


void cdp_volume_util::CollectClusterBitmapCustomizationInfo(ClusterBitmapCustomizationInfos_t &cbcustinfos,
                                                            const FileSystem::ClusterRange_t &cr,
                                                            const bool &set)
{
    SV_ULONGLONG processcount;
    SV_ULONGLONG count = cr.m_Count;
    SV_ULONGLONG processedcount = 0;
	SV_ULONGLONG svuintmax = SV_UINT_MAX;

    while (count)
    {
        processcount = std::min(svuintmax, count);
        cbcustinfos.push_back(ClusterBitmapCustomizationInfo_t(cr.m_Start+processedcount, processcount, set));
        processedcount += processcount;
        count -= processcount;
    }
}


bool cdp_volume_util::DoesDataMatch(cdp_volume_t &v, const SV_LONGLONG &offset, const SV_UINT &length, const char *datatocompare)
{
	bool matches = false;
	std::stringstream excmsg;

    char *readdata = AllocatePageAlignedMemory(length);
	if (readdata)
	{
		v.Seek(offset, BasicIo::BioBeg);
		SV_UINT count = v.FullRead(readdata, length);
		if (count == length)
		{
			matches = (0 == memcmp(datatocompare, readdata, length));
		}
		else
		{
			excmsg << "failed to read volume :"
				<< v.GetName() 
				<< ", offset " << offset
				<< ", length " << length
				<< ", with error: " << v.LastError();
			DebugPrintf(SV_LOG_ERROR, "In function %s, %s\n", FUNCTION_NAME, excmsg.str().c_str());
		}
        FreePageAlignedMemory(readdata);
	}
	else
	{
		excmsg << "Could not allocate read buffer of " << length << " bytes.";
		DebugPrintf(SV_LOG_ERROR, "In function %s, %s\n", FUNCTION_NAME, excmsg.str().c_str());
	}

	return matches;
}
