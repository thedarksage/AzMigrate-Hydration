//
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
//
// File       : dataprotectionsync.cpp
//
// Description:
//

#include "..\dataprotectionsync.h"
#include "dpdrivercomm.h"
#include "inmstrcmp.h"
#include "portablehelpers.h"
#include "portablehelpersmajor.h"
#include "logger.h"
#include "error.h"
#include "filesystem.h"


bool DataProtectionSync::GenerateResyncTimeIfReq(ResyncTimeSettings &rts)
{
    return true;
}


void DataProtectionSync::FillClusterBitmapCustomizationInfos(const bool &isFilterDriverAvailable)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

	/* unify cdpvolumeutil class usage
	 * 1. define only in dataprotection sync class
	 * use in all syncs */
	cdp_volume_util u;
    if (isFilterDriverAvailable && (0==InmStrCmp<NoCaseCharCmp>(Settings().fstype, NTFS)) && IsDrive(Settings().deviceName))
    {
        FileSystem::ClusterRanges_t rgs;
        svector_t pagefiles;
        DpDriverComm driverComm(Settings().isFabricVolume()?
                                Settings().sanVolumeInfo.virtualName :
                                Settings().deviceName, Settings().devicetype);
        DWORD mask = getpagefilevol(&pagefiles);
        int rval;

        for (constsvectoriter_t it = pagefiles.begin(); it != pagefiles.end(); it++)
        {
            const std::string &pagefile = *it;
            if (pagefile[0] == Settings().deviceName[0])
            {
                /* comparing the drive letter is enough as 
                 * page file cannot be on mount point */
                rval = driverComm.GetFilesystemClusters(pagefile, rgs);
                if (SV_SUCCESS == rval)
                {
                    DebugPrintf(SV_LOG_DEBUG, "got lcns for page file %s\n", pagefile.c_str());
                }
                else if (ERR_INVALID_FUNCTION == rval)
                {
                    DebugPrintf(SV_LOG_WARNING, "involflt driver does not support querying lcns for page file. Hence not querying further\n");
                    break;
                }
                else
                {
                    DebugPrintf(SV_LOG_ERROR, "For page file %s, got error %s to query page file\n", pagefile.c_str(), Error::Msg(Error::UserToOS(rval)).c_str());
                }
            }
        }

        DebugPrintf(SV_LOG_DEBUG, "got cluster ranges for page file%s\n", (rgs.size()>1) ? "s" : "");
		for (FileSystem::ConstClusterRangesIter_t it = rgs.begin(); it != rgs.end(); it++)
        {
            it->Print();
			u.CollectClusterBitmapCustomizationInfo(*m_pClusterBitmapCustomizationInfos, *it, false);
        }
    }

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}
