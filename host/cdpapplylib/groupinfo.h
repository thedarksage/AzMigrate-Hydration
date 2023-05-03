//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : groupinfo.h
//
// Description: 
//

#ifndef GROUPINFO_H
#define GROUPINFO_H

#include <list>
#include <string>
#include <vector>

#include "boost/shared_ptr.hpp"

#include "volumeinfo.h"
#include "programargs.h"
#include "volumegroupsettings.h"
#include "diffsortcriterion.h"

class GroupInfo { 
public:
    typedef std::list<boost::shared_ptr<VolumeInfo> > VolumeInfos_t;
	typedef std::list<VolumeInfo *> VolumeInfoList_t;
    GroupInfo(int id,
              int agentType,
              std::vector<std::string> visibleVolumes,
              const std::string& programeName,
              const std::string& agentSide,
              const std::string& syncType,
              const std::string& diffsLocation,
              const std::string& cacheLocation,
              const std::string& searchPattern,
              VOLUME_GROUP_SETTINGS const & volumeGroupSettings,
			  const VolumeSummaries_t *pVolumeSummariesCache)
        : m_Id(id),          
          m_AgentType(agentType),
          m_VisibleVolumes(visibleVolumes),
          m_ProgramName(programeName),          
          m_AgentSide(agentSide),
          m_SyncType(syncType),          
          m_DiffLocation(diffsLocation),
          m_CacheLocation(cacheLocation),                                             
          m_SearchPattern(searchPattern),                     
          m_VolumeGroupSettings(volumeGroupSettings),
		  m_havediffsToApply(false),
		  m_pVolumeSummariesCache(pVolumeSummariesCache)
    {} 
   

	GroupInfo(DiffSyncArgs const & args, VOLUME_GROUP_SETTINGS const & volumeGroupSettings,
		const VolumeSummaries_t *pVolumeSummariesCache)
        : m_Id(args.m_Id),          
          m_AgentType(args.m_AgentType),
          m_VisibleVolumes(args.m_VisibleVolumes),
          m_ProgramName(args.m_ProgramName),          
          m_AgentSide(args.m_AgentSide),
          m_SyncType(args.m_SyncType),          
          m_DiffLocation(args.m_DiffLocation),
          m_CacheLocation(args.m_CacheLocation),                                             
          m_SearchPattern(args.m_SearchPattern),                     
          m_VolumeGroupSettings(volumeGroupSettings),
		  m_havediffsToApply(false),
		  m_pVolumeSummariesCache(pVolumeSummariesCache)
    {}   
    
    bool SnapshotOnlyMode() { return (RUN_ONLY_SNAPSHOT_AGENT == m_AgentType); }
    bool DifferentialOnlyMode() { return (RUN_ONLY_OUTPOST_AGENT == m_AgentType); }
    bool SnapshotAndDifferentialMode() { return (RUN_OUTPOST_SNAPSHOT_AGENT == m_AgentType); }
	bool AvailableToProcessDiffs();


    std::string const & SearchPattern() const { return m_SearchPattern; }
    std::string const & DiffLocation() const { return m_DiffLocation; }
    std::string const & CacheLocation() const { return m_CacheLocation; }       

    VolumeInfos_t& Targets() { return m_Targets; }

    void ApplyConsistencyPolicies();
    void SetTargets();
    void CleanTargetCaches();
	bool HaveDiffsToApply() { return m_havediffsToApply;}
	void reset();
	void SetShouldCheckForCacheDirSpace();

protected:            
    void GetCutoffTime(DiffSortCriterion & cutoffTime);   
    void TrimDiffInfosToCutoffTime(DiffSortCriterion const & cutoffTime);

private:
    // no copying GroupInfo
    GroupInfo(const GroupInfo& groupInfo);
    GroupInfo& operator=(const GroupInfo& groupInfo);

    int m_Id;
    int m_AgentType; 

    std::vector<std::string>  m_VisibleVolumes;

    std::string m_ProgramName;
    std::string m_AgentSide;
    std::string m_SyncType;
    std::string m_DiffLocation; 
    std::string m_CacheLocation;       
    std::string m_SearchPattern;
	bool m_havediffsToApply;

    VolumeInfos_t m_Targets;

 

    VOLUME_GROUP_SETTINGS m_VolumeGroupSettings;
	const VolumeSummaries_t *m_pVolumeSummariesCache;
};       

#endif // ifndef GROUPINFO__H
