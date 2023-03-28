//                                       
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
// 
// File       : dataprotector.cpp
// 
// Description: 
// 

#include <string>

#include "dataprotector.h"
#include "cdplock.h"
#include "theconfigurator.h"
#include "rpcconfigurator.h"
#include "configurevxagent.h"
#include "ace/OS_NS_sys_stat.h"
/* For IsReportingRealNameToCx function */
#include "portablehelpersmajor.h"
#include "inmsafecapis.h"

using namespace std;
extern Configurator* TheConfigurator;

/*
 * FUNCTION NAME : DataProtector::Create
 *
 * DESCRIPTION : create the appropriate dataprotectionsync objects
 *               (dataprotectionsync jobs: FastSync, OffloadSync, DiffSync)
 *
 * INPUT PARAMETERS : none
 *
 * OUTPUT PARAMETERS : none
 *
 * NOTES :
 * 
 *
 * return value : none
 *
 */

void DataProtector::Create()
{           
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    CDPUtil::InitQuitEvent();
    LocalConfigurator lc;
    //std::vector<CDPLock::Ptr> cdplocks;
    bool foundRollbackVolume = false;
    std::string cxFormattedVolumeName = "";
    VOLUME_GROUP_SETTINGS::volumes_iterator volIt(
        m_VolumeGroupSettings.volumes.begin());
    VOLUME_GROUP_SETTINGS::volumes_iterator volItEnd(
        m_VolumeGroupSettings.volumes.end());

    for (/* empty */; volIt != volItEnd; ++volIt) 
    {

        // we will acquire lock on only those targets matching the sync mode
        if(m_Args.m_DiffSync) 
        {
            // acquire lock on only diffsync and quasidiffsync targets
            if( !(VOLUME_SETTINGS::SYNC_DIFF == (*volIt).second.syncMode 
                || VOLUME_SETTINGS::SYNC_QUASIDIFF == (*volIt).second.syncMode) )
                continue;

        }
        else  if (m_Args.m_FastSync)
        {
            // acquire lock on only fast sync targets
            //Fast Sync TBC -- adding new sync mode
            if( VOLUME_SETTINGS::SYNC_FAST_TBC != (*volIt).second.syncMode )
                continue;
        } 
        else  if (m_Args.m_DirectSync)
        {
            // acquire lock on only fast sync targets
            if( VOLUME_SETTINGS::SYNC_DIRECT != (*volIt).second.syncMode )
                continue;
        } 
        else 
        {
            if(VOLUME_SETTINGS::SYNC_OFFLOAD != (*volIt).second.syncMode )
                continue;
        }
        
        //checking whether the pair exist or not
        if(!(VOLUME_SETTINGS::PAIR_PROGRESS == (*volIt).second.pairState ||
            VOLUME_SETTINGS::FLUSH_AND_HOLD_WRITES_PENDING == (*volIt).second.pairState||
            VOLUME_SETTINGS::FLUSH_AND_HOLD_RESUME_PENDING == (*volIt).second.pairState) )//SRM: dataprotection should run for both flush pending and resume 
            return;

        /**
        * 1 TO N sync: 
        * 1. lock is not needed for source
        * 2. lock name for target is target
        * 3. do volume lock only if the pair is in resync-I
        * 4. For directsync the dp is launched only for target,
        *    as the (*volIt).first is the source for directsync, so fetching the endpoint of it for target
        */
        std::string targetname;
        std::string fileLock;
            
        if (((TARGET == m_VolumeGroupSettings.direction) && (!m_Args.m_DiffSync)) ||
             ((SOURCE == m_VolumeGroupSettings.direction) && (m_Args.m_DirectSync)))
        {
            if(m_Args.m_DirectSync)
            {
                VOLUME_SETTINGS::endpoints_iterator endPointsIter = 
                    (*volIt).second.endpoints.begin();
                targetname = endPointsIter->deviceName;
            }
            else
            {
                targetname = (*volIt).first;
            }
            DebugPrintf(SV_LOG_DEBUG, "The lock name going to acquire is %s\n", targetname.c_str());
            
            CDPLock::Ptr cdplock(new CDPLock(targetname));
            if(!cdplock ->try_acquire()) 
            {
                DebugPrintf(SV_LOG_WARNING, 
                    "Exclusive lock denied for %s. Ok if cdpcli is using this volume.cdpcli snapshot should be run only after sucessfully stopping svagent service.\n", 
                    targetname.c_str());
                return;
            }
            cdplocks.push_back(cdplock);
            
            //Delete the 'resyncRequired' file for the target if the pair is in resync mode
            std::string appliedInfoPath = CDPUtil::get_last_fileapplied_info_location(targetname);
            std::string appliedInfoDir = dirname_r(appliedInfoPath.c_str());
            appliedInfoDir += ACE_DIRECTORY_SEPARATOR_CHAR_A;

            std::string resyncRequiredFile = appliedInfoDir + "resyncRequired";
            
            if((ACE_OS::unlink(getLongPathName(resyncRequiredFile.c_str()).c_str()) < 0) && (ACE_OS::last_error() != ENOENT))
            {
                DebugPrintf(SV_LOG_ERROR,
                    "\n%s encountered error (%d) while trying unlink operation for file %s.\n",
                    FUNCTION_NAME,ACE_OS::last_error(), resyncRequiredFile.c_str());
                return;
            }

            std::string sourceSystemCrashedFile = appliedInfoDir + "sourceSystemCrashed";
            if((ACE_OS::unlink(getLongPathName(sourceSystemCrashedFile.c_str()).c_str()) < 0)
                && (ACE_OS::last_error() != ENOENT))
            {
                DebugPrintf(SV_LOG_ERROR,
                    "\n%s encountered error (%d) while trying unlink operation for file %s.\n",
                    FUNCTION_NAME,ACE_OS::last_error(), sourceSystemCrashedFile.c_str());
                return;
            }

            if((ACE_OS::unlink(getLongPathName(appliedInfoPath.c_str()).c_str()) < 0)
                && (ACE_OS::last_error() != ENOENT))
            {
                DebugPrintf(SV_LOG_ERROR,
                    "\n%s encountered error (%d) while trying unlink operation for file %s.\n",
                    FUNCTION_NAME,ACE_OS::last_error(), appliedInfoPath.c_str());
                return;
            }

            CDP_SETTINGS settings = TheConfigurator->getCdpSettings(targetname);
            std::string dbname = settings.Catalogue();
            std::string cdpdir = settings.CatalogueDir();
            if((dbname.size() > CDPV1DBNAME.size()) &&
                (std::equal(dbname.begin() + dbname.size() - CDPV1DBNAME.size(), dbname.end(), CDPV1DBNAME.begin())))
            {
                std::string retTxnFileName = CDPUtil::get_last_retentiontxn_info_location(cdpdir);
                ACE_stat statbuf = {0};
                if ((0 == sv_stat(getLongPathName(retTxnFileName.c_str()).c_str(), &statbuf))
                    && ( (statbuf.st_mode & S_IFREG) == S_IFREG ) )
                {
                    CDPLastRetentionUpdate lastCDPtxn;
                    if(!CDPUtil::get_last_retentiontxn(cdpdir, lastCDPtxn))
                    {
                        DebugPrintf(SV_LOG_ERROR,
                            "%s unable to get last retention transaction details from %s.\n",
                            FUNCTION_NAME, retTxnFileName.c_str());
                        return;
                    }
                    std::string none = "none";
                    memset(&lastCDPtxn, 0, sizeof(lastCDPtxn.partialTransaction));
                    inm_memcpy_s(lastCDPtxn.partialTransaction.diffFileApplied, sizeof(lastCDPtxn.partialTransaction.diffFileApplied), none.c_str(), none.length() + 1);

                    if(!CDPUtil::update_last_retentiontxn(cdpdir, lastCDPtxn))
                    {
                        DebugPrintf(SV_LOG_ERROR,
                            "%s unable to clear last retention transaction details in %s.\n",
                            FUNCTION_NAME, retTxnFileName.c_str());
                        return;
                    }
                }
            }
        }


        if (!m_Args.m_DiffSync)
        {
            cxFormattedVolumeName = (*volIt).first;
            string RollbackActionsFilePath = CDPUtil::getPendingActionsFilePath(cxFormattedVolumeName, ".rollback");

            ACE_stat statbuf = {0};
            if ((0 == sv_stat(getLongPathName(RollbackActionsFilePath.c_str()).c_str(), &statbuf))
                && ( (statbuf.st_mode & S_IFREG) == S_IFREG ) )
            {
                DebugPrintf(SV_LOG_ERROR, 
                    "Note: this is not an error. Replication pair for volume %s is under rollback; resync cannot continue.\n",
                    cxFormattedVolumeName.c_str());
                foundRollbackVolume= true;
            }
        }

        //checking another instance of dp is running or not for the same volume, if it running then exit newly launched instance
                
        if (m_Args.m_DirectSync)
        {
            
            fileLock = m_Args.m_directSyncArgs.m_srcDeviceName + "_" + boost::lexical_cast<std::string>(m_VolumeGroupSettings.id) + "_" + m_Args.m_directSyncArgs.m_trgDeviceName;            
            DebugPrintf(SV_LOG_DEBUG, " The direct sync lock name = %s \n", fileLock.c_str());

        }
        else if (m_Args.m_DiffSync)
        {
            fileLock = (*volIt).first;            
        }
        else if (m_Args.m_FastSync)
        {
            if (TARGET == m_VolumeGroupSettings.direction)
            {
                fileLock = (*volIt).second.deviceName;
                DebugPrintf(SV_LOG_DEBUG, " The fast sync target lock name is %s\n", fileLock.c_str());

            }
            else if (SOURCE == m_VolumeGroupSettings.direction)
            {
                fileLock = (*volIt).second.deviceName + "_" + boost::lexical_cast<std::string>(m_VolumeGroupSettings.id) + "_" + (*volIt).second.endpoints.begin()->deviceName;                
                DebugPrintf(SV_LOG_DEBUG, " The fast sync source lock name  %s \n", fileLock.c_str());
            }
            else
            {
                std::stringstream sserr("Not a valid direction ");
                sserr << m_VolumeGroupSettings.direction << '.';
                throw INMAGE_EX(sserr.str().c_str());
            }
        }
        
        //appending _instance to end of lock name to maintain same lock name for all three sync cases
        fileLock = fileLock + "_instance";
        DebugPrintf(SV_LOG_DEBUG, " The lock name going to acquire is %s\n", fileLock.c_str());
        CDPLock::Ptr cdplock(new CDPLock(fileLock));
        if (cdplock->try_acquire())
        {
            DebugPrintf(SV_LOG_DEBUG, "%s: Lock acquired sucessfully. lock name = %s\n", FUNCTION_NAME, fileLock.c_str());
            cdplocks.push_back(cdplock);
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "%s: Another dataprotection instance is running for same device with lock name = %s. Exiting newly created process %d\n",
                FUNCTION_NAME, fileLock.c_str(), ACE_OS::getpid());
            //newly launched process should exit so return from here
            //wait for 60-90 sec to quit this process,so that immediate launch of same processs can be avoided
            CDPUtil::QuitRequested(lc.getInitialSettingCallInterval());
            return;
        }
        
    }
    if(foundRollbackVolume)
    {        
        return;        
    }

    if (m_Args.m_DiffSync) {            
        m_DiffSyncJob = new DifferentialSync(m_VolumeGroupSettings, m_Args.m_DiffSyncArgs);
    } else if (m_Args.m_FastSync) {
        m_FastSyncJob = new FastSync(m_VolumeGroupSettings, m_Args.m_FastOffloadSyncArgs.m_AgentRestartStatus);
    } else if (m_Args.m_DirectSync){
        m_DirectSyncJob = new LocalDirectSync(m_VolumeGroupSettings, m_Args.m_directSyncArgs.m_srcDeviceName, m_Args.m_directSyncArgs.m_trgDeviceName, m_Args.m_directSyncArgs.m_blockSize, TheConfigurator->getVxAgent().getInstallPath());
    }
}

/*
 * FUNCTION NAME : DataProtector::Start
 *
 * DESCRIPTION : start the appropriate dataprotectionsync jobs
 *               (dataprotectionsync jobs: FastSync, OffloadSync, DiffSync)
 *
 * INPUT PARAMETERS : none
 *
 * OUTPUT PARAMETERS : none
 *
 * NOTES :
 * 
 * for now since we only have 1 type of job this will not return until it is done
 * in the future when we support more then 1 we will need to start these in their 
 * own threads 
 *
 * return value : none
 *
 */
void DataProtector::Start()
{        
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    if (NULL != m_DiffSyncJob) 
    {            
        m_DiffSyncJob->Start();
    }

    if (NULL != m_FastSyncJob) 
    {
        m_FastSyncJob->Start();
    }

    if (NULL != m_DirectSyncJob)
    {
        m_DirectSyncJob->Start();
    }
}

// --------------------------------------------------------------------------
// stops protecting
// --------------------------------------------------------------------------
void DataProtector::Stop()
{        
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    if (NULL != m_DiffSyncJob) {
        m_DiffSyncJob->Stop();
    }
    if (NULL != m_FastSyncJob) {
        m_FastSyncJob->Stop();
    }
    if (NULL != m_DirectSyncJob)
    {
        m_DirectSyncJob->Stop();
    }
}

// --------------------------------------------------------------------------
// updates group settings
// --------------------------------------------------------------------------
void DataProtector::UpdateVolumeGroupSettings(VOLUME_GROUP_SETTINGS& volumeGroupSettings)
{      
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    if (NULL != m_DiffSyncJob) {
        m_DiffSyncJob->UpdateVolumeGroupSettings(volumeGroupSettings);
    }
    if (NULL != m_FastSyncJob) {
        m_FastSyncJob->UpdateVolumeGroupSettings(volumeGroupSettings);
    }
    if(NULL != m_DirectSyncJob)
    {
        m_DirectSyncJob->UpdateVolumeGroupSettings(volumeGroupSettings);
    }
}

bool DataProtector::HaveSettingsChanged(const HOST_VOLUME_GROUP_SETTINGS &hostVolumeGroupSettings)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    bool changed;

    HOST_VOLUME_GROUP_SETTINGS::volumeGroups_constiterator vgsiter = hostVolumeGroupSettings.volumeGroups.begin();
    for ( /* empty */ ; vgsiter != hostVolumeGroupSettings.volumeGroups.end(); vgsiter++) {
        const VOLUME_GROUP_SETTINGS &vgs = *vgsiter;
        if (vgs.id == m_VolumeGroupSettings.id) {
            changed = !(vgs == m_VolumeGroupSettings);
            break;
        }
    }

    /* if volume group id is not found, it is a change */
    if (vgsiter == hostVolumeGroupSettings.volumeGroups.end()) {
        DebugPrintf(SV_LOG_DEBUG, "volume group settings id %d, not found in newer settings\n", m_VolumeGroupSettings.id);
        changed = true;
    }

    if (changed) {
        DebugPrintf(SV_LOG_DEBUG, "settings have changed for volume group settings id %d\n", m_VolumeGroupSettings.id);
    }
    
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
    return changed;
}
