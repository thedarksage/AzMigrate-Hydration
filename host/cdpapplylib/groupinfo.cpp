//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : groupinfo.cpp
//
// Description:
//

#include <sstream>
#include <fstream>

#include "theconfigurator.h"
#include "configurevxagent.h"
#include "groupinfo.h"
#include "dataprotectionexception.h"
#include "cdputil.h"
#include "configwrapper.h"
#include "cdplock.h"
#include "localconfigurator.h"
#include "inmalertdefs.h"
#include "AgentHealthIssueNumbers.h"

using namespace std;


DiffSortCriterion const MIN_DIFF_SORT_CRITERION(DiffSortCriterion::MIN_SORT_VALUE);
DiffSortCriterion const MAX_DIFF_SORT_CRITERION(DiffSortCriterion::MAX_SORT_VALUE);

inline bool IsVolumeVisible(std::vector<std::string> &visibleVolumes, std::string volume)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	std::vector<std::string> :: const_iterator  iter = visibleVolumes.begin();
	std::vector<std::string> :: const_iterator  iterEnd = visibleVolumes.end();

	for( ; iter != iterEnd; iter++)
	{
		if (!strcmp((*iter).c_str(), volume.c_str()))
			return true;
	}

	return false;
}


// --------------------------------------------------------------------------
// sets up all the targets for the volume group
// --------------------------------------------------------------------------
void GroupInfo::SetTargets()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	ResyncTimeSettings resyncStartTime, resyncEndTime;
	bool iscxpatched = TheConfigurator->getVxAgent().getIsCXPatched();

	HTTP_CONNECTION_SETTINGS http = TheConfigurator->getHttpSettings();
	VOLUME_GROUP_SETTINGS::volumes_iterator iter(m_VolumeGroupSettings.volumes.begin());
	VOLUME_GROUP_SETTINGS::volumes_iterator end(m_VolumeGroupSettings.volumes.end());

	for (/* empty*/; iter != end; ++iter) {

		// we will set volumes which are in diffsync and quasidiffsync state as targets
		// not those in resync step1.
		if( !(VOLUME_SETTINGS::SYNC_DIFF == (*iter).second.syncMode 
			|| VOLUME_SETTINGS::SYNC_QUASIDIFF == (*iter).second.syncMode) )
			continue;
		if(!(VOLUME_SETTINGS::PAIR_PROGRESS == (*iter).second.pairState ||
			VOLUME_SETTINGS::FLUSH_AND_HOLD_WRITES_PENDING == (*iter).second.pairState ||
			VOLUME_SETTINGS::FLUSH_AND_HOLD_RESUME_PENDING == (*iter).second.pairState))
			continue;

		if(iscxpatched)
		{
			resyncStartTime.time = (*iter).second.srcResyncStarttime;
			resyncStartTime.seqNo = (*iter).second.srcResyncStartSeq;

			resyncEndTime.time = (*iter).second.srcResyncEndtime;
			resyncEndTime.seqNo = (*iter).second.srcResyncEndSeq;
		}
		else
		{
			resyncStartTime.time = (*iter).second.srcResyncStarttime;
			resyncStartTime.seqNo = 0;
			resyncEndTime.time = (*iter).second.srcResyncEndtime;
			resyncEndTime.seqNo = 0;
		}

		// Bug #7189 - Hide the volume only if the visibility is false
		if(!(*iter).second.visibility)
		{
            bool isvirtualvol = false;
            std::string dev_name((*iter).second.deviceName);
            std::string mntPt((*iter).second.mountPoint);
            std::string sparsefile;
            LocalConfigurator localConfigurator;
            bool isnewvirtualvolume = false;
			bool isAzureTarget = ((*iter).second.isAzureDataPlane());

			if (!isAzureTarget && !IsVolpackDriverAvailable() && (IsVolPackDevice(dev_name.c_str(), sparsefile, isnewvirtualvolume)))
            {
                dev_name = sparsefile;
                mntPt = sparsefile;
                isvirtualvol = true;
            }


			std::string formattedName((*iter).second.deviceName);
			FormatVolumeName(formattedName);


			std::string formattedcxname((*iter).second.deviceName);
			FormatVolumeNameForCxReporting(formattedcxname);
			//Before hiding the target volume acquaring the volume lock to ensure
			//no snapshot,rollback process running
			//checking whether rollback done for target volume
			CDPLock volumeCdpLock(formattedName);
			volumeCdpLock.acquire();

			string RollbackActionsFilePath = CDPUtil::getPendingActionsFilePath(formattedcxname, ".rollback");

			ACE_stat statbuf = {0};
			if ((0 == sv_stat(getLongPathName(RollbackActionsFilePath.c_str()).c_str(), &statbuf))
				&& ( (statbuf.st_mode & S_IFREG) == S_IFREG ) )
			{
				continue;
			}

			if (!isAzureTarget) {

				// for disk, we are not allowing unhide_ro,unhide_rw operations
				// so we can always go ahead and make sure to offline the disk in RW mode.
				if ((*iter).second.GetDeviceType() == VolumeSummary::DISK) {

					cdp_volume_t tgtVol(dev_name, false, cdp_volume_t::CDP_DISK_TYPE, m_pVolumeSummariesCache);
					if (!tgtVol.Hide()){
						TargetVolumeUnmountFailedAlert a(formattedcxname);
						SendAlertToCx(SV_ALERT_CRITICAL, SV_ALERT_MODULE_DIFFERENTIALSYNC, a);
						DebugPrintf(SV_LOG_ERROR, "%s\n", a.GetMessage().c_str());
						continue;
					}

				}
				else {

					VOLUME_STATE VolumeState = GetVolumeState(formattedcxname.c_str());
					if (VolumeState != VOLUME_HIDDEN)
					{
						if (VolumeState == VOLUME_VISIBLE_RW)
						{
                            const std::string &resyncReasonCode = AgentHealthIssueCodes::DiskLevelHealthIssues::ResyncReason::TargetInternalError;
							std::stringstream msg("Target device is unlocked in read-write mode.");
                            msg << ". Marking resync for the device " << dev_name << " with resyncReasonCode=" << resyncReasonCode;
                            DebugPrintf(SV_LOG_ERROR, "%s: %s", FUNCTION_NAME, msg.str().c_str());
                            ResyncReasonStamp resyncReasonStamp;
                            STAMP_RESYNC_REASON(resyncReasonStamp, resyncReasonCode);
                            
                            if (!CDPUtil::store_and_sendcs_resync_required(formattedcxname, msg.str(), resyncReasonStamp))
							{
								continue;
							}
						}

						Volume tgtVol(dev_name, mntPt);
						if (!isvirtualvol)
							tgtVol.SetName(formattedName);
						if (!tgtVol.Hide())
						{
							TargetVolumeUnmountFailedAlert a(formattedcxname);
							SendAlertToCx(SV_ALERT_CRITICAL, SV_ALERT_MODULE_DIFFERENTIALSYNC, a);
							DebugPrintf(SV_LOG_ERROR, "%s\n", a.GetMessage().c_str());
							continue;
						}
						bool status = false;
						bool rv = updateVolumeAttribute((*TheConfigurator), UPDATE_NOTIFY, formattedcxname, VOLUME_HIDDEN, "", status);
						if (!rv || !status)
						{
							DebugPrintf(SV_LOG_ERROR,
								"FAILED: hide operation succeeded for %s but failed to update CX with status.\n",
								formattedcxname.c_str());
						}
					}
				}
			}

			m_Targets.push_back(boost::shared_ptr<VolumeInfo>(new VolumeInfo((*iter).second.deviceName,
				(*iter).second.mountPoint,
				(*iter).second.transportProtocol,
				VOLUME_SETTINGS::SECURE_MODE_SECURE == (*iter).second.secureMode,
				VOLUME_SETTINGS::SYNC_DIFF == (*iter).second.syncMode,
				(*iter).second.visibility,
				m_DiffLocation,
				m_CacheLocation,
				(*iter).second.resyncRequiredFlag,
				(*iter).second.resyncRequiredCause,
				(*iter).second.resyncRequiredTimestamp,
				VOLUME_SETTINGS::SYNC_QUASIDIFF == (*iter).second.syncMode,

				resyncStartTime.time,
				resyncStartTime.seqNo,
				resyncEndTime.time,
				resyncEndTime.seqNo,
				isAzureTarget,
				m_pVolumeSummariesCache)));
		}
	}
}

// ---------------------------------------------------------------------------
// Set up things for next run
// ---------------------------------------------------------------------------
void GroupInfo::SetShouldCheckForCacheDirSpace()
{
	GroupInfo::VolumeInfos_t::iterator iter(m_Targets.begin());
	GroupInfo::VolumeInfos_t::iterator end(m_Targets.end());
	for (/* empty */; iter != end; ++iter) {
		(*iter) -> SetShouldCheckForCacheDirSpace();
	}
}

// --------------------------------------------------------------------------
// asks all the targets to clean their local cache
// --------------------------------------------------------------------------
void GroupInfo::CleanTargetCaches()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	VolumeInfos_t::iterator iter(m_Targets.begin());
	VolumeInfos_t::iterator end(m_Targets.end());

	for (/* empty */; iter != end; ++iter) {
		(*iter)->CleanCache();
	}
}

// ---------------------------------------------------------------------------
// Set up things for next run
// ---------------------------------------------------------------------------
void GroupInfo::reset()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	m_havediffsToApply = false;
	GroupInfo::VolumeInfos_t::iterator iter(m_Targets.begin());
	GroupInfo::VolumeInfos_t::iterator end(m_Targets.end());
	for (/* empty */; iter != end; ++iter) {
		(*iter) -> reset();
	}
}

// --------------------------------------------------------------------------
// gets the cutoff time that is used to determine which diffs to apply
// basically it determines the oldest end time of all the volumes most recent
// end time that is a continuation end
// (skips over volumes that are resyncing)
// --------------------------------------------------------------------------
void GroupInfo::GetCutoffTime(DiffSortCriterion & cutoffTime)
{    
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	GroupInfo::VolumeInfos_t::iterator iter(m_Targets.begin());
	GroupInfo::VolumeInfos_t::iterator end(m_Targets.end());
	for (/* empty */; iter != end; ++iter) {
		if ((*iter)->OkToProcessDiffs()) {
			if ((*iter)->DiffInfos().empty()) {
				// each volume in diff sync mode needs at least 1 diff
				// otherwise don't apply anything unless a delta policy says it's ok
				cutoffTime = MIN_DIFF_SORT_CRITERION;
				break;
			}
			DiffInfo::DiffInfosOrderedEndTime_t::reverse_iterator rIter((*iter)->DiffInfos().rbegin());

			//check for availability of ME(end of split io) file before applying MC files is removed
			//for fixing the bug #7351

			if ((*rIter)->EndTimeLessThenCutoffTime(cutoffTime)) {
				cutoffTime = (*rIter)->SortCriterion();
			}
		}
	}
}

// --------------------------------------------------------------------------
// check if we have atleast one volume available to process diffs
// --------------------------------------------------------------------------
bool GroupInfo::AvailableToProcessDiffs()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool rv = false;

	GroupInfo::VolumeInfos_t::iterator iter(m_Targets.begin());
	GroupInfo::VolumeInfos_t::iterator end(m_Targets.end());
	for (/* empty */; iter != end; ++iter) {
		if ((*iter)->OkToProcessDiffs()) {
			rv = true;
			break;
		}
	}

	return rv;
}


// --------------------------------------------------------------------------
// puts all the diffs whose end time is <= cutoff time into m_ApplyDiffs
// --------------------------------------------------------------------------
//void GroupInfo::OrderApplyInfos()
//{
//    GroupInfo::VolumeInfos_t::iterator iter(m_Targets.begin());
//    GroupInfo::VolumeInfos_t::iterator end(m_Targets.end());
//    for (/* empty */; iter != end; ++iter) {
//        if ((*iter)->OkToProcessDiffs()) {
//            DiffInfo::DiffInfosOrderedEndTime_t::iterator infoIter((*iter)->DiffInfos().begin());
//            DiffInfo::DiffInfosOrderedEndTime_t::iterator infoEnd((*iter)->DiffInfos().end());
//            for (/* empty */; infoIter != infoEnd; ++infoIter) {
//                // NOTE: for now we allow duplicates but this maybe an issue
//                m_ApplyDiffs.insert(*infoIter);    
//            }
//        }
//    }
//}

// --------------------------------------------------------------------------
// trims each volume's DiffInfo list down to just the ones that will
// be applied so as not to transfer extra data files that won't be applied
// --------------------------------------------------------------------------
void GroupInfo::TrimDiffInfosToCutoffTime(DiffSortCriterion const & cutoffTime)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	GroupInfo::VolumeInfos_t::iterator iter(m_Targets.begin());
	GroupInfo::VolumeInfos_t::iterator end(m_Targets.end());
	for (/* empty */; iter != end; ++iter) {
		if ((*iter)->OkToProcessDiffs() && !(*iter)->DiffInfos().empty()) {              
			DiffInfo::DiffInfosOrderedEndTime_t::reverse_iterator rIter((*iter)->DiffInfos().rbegin());
			DiffInfo::DiffInfosOrderedEndTime_t::reverse_iterator rEnd((*iter)->DiffInfos().rend());

			for (/* empty */; rIter != rEnd; ++rIter) {               
				//check for availability of ME(end of split io) file before applying MC files is removed
				//for fixing the bug #7351
				if ((*rIter)->CutoffTimeGreaterThenEndTime(cutoffTime)) {
					break;
				}                            
			}

			if (rIter == rEnd) {
				(*iter)->DiffInfos().clear();
			} else {
				DiffInfo::DiffInfosOrderedEndTime_t::iterator eraseIter = rIter.base();
				(*iter)->DiffInfos().erase(eraseIter, (*iter)->DiffInfos().end());
			}
		}
	}
}

// --------------------------------------------------------------------------
// applies consitency polices
// --------------------------------------------------------------------------
void GroupInfo::ApplyConsistencyPolicies()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	DiffSortCriterion cutoffTime(MAX_DIFF_SORT_CRITERION); 

	GetCutoffTime(cutoffTime);    

	// TODO: apply delta policies so if it has been a while since we applied
	// anything we can still do it even if 1 or more volumes don't have any
	// data to apply

	if (cutoffTime.Equal(MIN_DIFF_SORT_CRITERION) || 
		cutoffTime.Equal(MAX_DIFF_SORT_CRITERION) ) {
			// at least one of the volumes in the volume group
			// do not have any data to apply. so we cannot proceed
			m_havediffsToApply = false;
			return;
	}

	TrimDiffInfosToCutoffTime(cutoffTime);
	m_havediffsToApply = true;
	string cutoffpt;
	CDPUtil::ToDisplayTimeOnConsole(cutoffTime.EndTime(), cutoffpt);
	DebugPrintf(SV_LOG_DEBUG, "CUTOFFPOINT: %s\n", cutoffpt.c_str());

	// We are now having per volume apply thread
	// so we do not need to compute order of apply infos
	// for the whole volume group
	//OrderApplyInfos();
}
