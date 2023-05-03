//
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
//
// File       : dataprotection.cpp
//
// Description:
//
#include <iostream>
#include <sstream>

#include "dataprotection.h"
#include "dataprotectionexception.h"
#include "theconfigurator.h"
#include "rpcconfigurator.h"
#include "differentialsync.h"
#include "logger.h"
#include "svtypes.h"
#include <ace/OS_NS_unistd.h>
#include <ace/OS_NS_time.h>
#include <ace/OS_NS_sys_time.h>
#include "configurevxagent.h"

int const CALLBACK_WAIT_INTERVAL = 5; // wait 5 seconds at a time for the call back

DataProtection::DataProtection(ProgramArgs& args)
    : m_Args(args),
    m_ReceviedConfiguratorCallback(false),
    m_StopProtectorOnConfiguratorChanges(false),
    m_ConfigurationChanged(false)
{
}

DataProtection::~DataProtection()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    /* Order is important since after all uploads and download finish,
     * stop logging thread */
    if (m_CxTransportXferLogMonitor)
    {
        DebugPrintf(SV_LOG_DEBUG, "Waiting for file tal log monitor\n");
        cxTransportStopXferLogMonitor();
        m_CxTransportXferLogMonitor->join();
    }
    if (NULL != TheConfigurator) {
        TheConfigurator->getHostVolumeGroupSignal().disconnect(this);
        TheConfigurator->getCdpSettingsSignal().disconnect(this);
        TheConfigurator->Stop();
    }
    m_DataProtectors.clear();
}

std::string DataProtection::GetLogDir()
{
    return TheConfigurator->getVxAgent().getLogPathname() + m_Args.getLogDir();
}

void DataProtection::Protect()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    m_bLogFileXfer = TheConfigurator->getVxAgent().getLogFileXfer();
    HOST_VOLUME_GROUP_SETTINGS settings = TheConfigurator->getHostVolumeGroupSettings();
    ProcessHostVolumeGroupSettings( settings );
    m_StopProtectorOnConfiguratorChanges = true;

    // PR #5417
    // On recieving the configurator callback, StopAll is called
    // which in turn sets m_protect to false and set the quit event.
    // But m_protect and quit are not initialized until StartAll is called.
    // To resolve this issue, we have seperated out the dataprotectionsync
    // object creation and dataprotectionsync job invocation.
    // DataprotectionSync Objects are created using CreateAll method
    // and the jobs are invoked using StartAll
    // Callback registration is performed only after the object creation
    // is done
    CreateAll();
    TheConfigurator->getHostVolumeGroupSignal().connect(this, &DataProtection::HostVolumeGroupSettingsCallback);
    TheConfigurator->getCdpSettingsSignal().connect(this,&DataProtection::CdpSettingsCallback);
    StartAll();

}

void DataProtection::HostVolumeGroupSettingsCallback(HOST_VOLUME_GROUP_SETTINGS hostVolumeGroupSettings)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    // TODO: need to check what changed and if it was for the current dataprotector then
    // stop it.
    // eventually should really call DataProtector update and let it decide if it can proceed or not
    if (m_ConfigurationChanged) {
        return;
    }
    if (m_StopProtectorOnConfiguratorChanges && HaveSettingsChangedForDataprotector(hostVolumeGroupSettings)) {
        m_ConfigurationChanged = true;
        StopAll();
    } else {
        m_ReceviedConfiguratorCallback = true;
    }
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}

bool DataProtection::HaveSettingsChangedForDataprotector(const HOST_VOLUME_GROUP_SETTINGS &hostVolumeGroupSettings)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool changed = false;

    DataProtectors_t::iterator endIt(m_DataProtectors.end());
    for (DataProtectors_t::iterator it = m_DataProtectors.begin(); it != endIt; ++it) {
        changed = (*it).second->HaveSettingsChanged(hostVolumeGroupSettings);
		if (changed) {
			DebugPrintf(SV_LOG_DEBUG, "dataprotector settings have changed\n");
			break;
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return changed;
}

void DataProtection::CdpSettingsCallback()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    if (m_ConfigurationChanged) {
        return;
    }
    if (m_StopProtectorOnConfiguratorChanges) {
        m_ConfigurationChanged = true;
        StopAll();
    } else {
        m_ReceviedConfiguratorCallback = true;
    }
}

void DataProtection::ProcessHostVolumeGroupSettings(HOST_VOLUME_GROUP_SETTINGS& hostVolumeGroupSettings)
{
    bool breakout = false;
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    LockGuard lock(m_Lock);
    if (hostVolumeGroupSettings.volumeGroups.empty()) {
        return;
    }

    HOST_VOLUME_GROUP_SETTINGS::volumeGroups_iterator volumeGroupEndIt(hostVolumeGroupSettings.volumeGroups.end());
    for (HOST_VOLUME_GROUP_SETTINGS::volumeGroups_iterator volumeGroupIt = hostVolumeGroupSettings.volumeGroups.begin();
         (volumeGroupIt != volumeGroupEndIt) && (!breakout);
         ++volumeGroupIt)
    {
        if (m_Args.m_DiffSync)
        {
            if (((*volumeGroupIt).id == m_Args.m_DiffSyncArgs.m_Id) && (TARGET == (*volumeGroupIt).direction))
            {
                DataProtecterPtr_t dataProtector = DataProtecterPtr_t(new DataProtector(m_Args, (*volumeGroupIt)));
                m_DataProtectors.insert(std::make_pair(m_Args.m_DiffSyncArgs.m_Id, dataProtector));
                break;
            }
        }
        else if (m_Args.m_DirectSync)
        {
            VOLUME_GROUP_SETTINGS::volumes_iterator volIt((*volumeGroupIt).volumes.begin());
            VOLUME_GROUP_SETTINGS::volumes_iterator volItEnd((*volumeGroupIt).volumes.end());

            for (/* empty */; volIt != volItEnd; ++volIt)
            {
                // Invoke dp for directsync if the direction is SOURCE
                // as it will issue START_FILTER for source volume.
                // If it for TARGET then it will issue START_FILTER for
                // target.

                /**
                 * 1 TO N sync: comparsion for end point added
                 */
                std::string endpointdevice = (*volIt).second.endpoints.begin()->deviceName;
                if(VOLUME_SETTINGS::SYNC_DIRECT == (*volIt).second.syncMode
                   && SOURCE == (*volumeGroupIt).direction
                   && ((*volIt).second.deviceName ==  m_Args.m_directSyncArgs.m_srcDeviceName )
                   && endpointdevice == m_Args.m_directSyncArgs.m_trgDeviceName) 
                {
                    VOLUME_GROUP_SETTINGS vgs = (*volumeGroupIt);
                    DebugPrintf(SV_LOG_DEBUG, "For direct sync inserted device = %s, endpointdevice = %s\n", 
                                (*volIt).second.deviceName.c_str(),
                                (*volIt).second.endpoints.begin()->deviceName.c_str());
                    DataProtecterPtr_t dataProtector = DataProtecterPtr_t(new DataProtector(m_Args, vgs));
                    m_DataProtectors.insert(std::make_pair((*volumeGroupIt).id, dataProtector));
                    breakout = true;
                    break;
                }
            }
        }
        else
        {
            // TODO: refactor once there are true volume groups for fast/offload sync
            // need to walk the volumes in this group to find the correct volume
            VOLUME_GROUP_SETTINGS::volumes_iterator volIt((*volumeGroupIt).volumes.begin());
            VOLUME_GROUP_SETTINGS::volumes_iterator volItEnd((*volumeGroupIt).volumes.end());
            for (/* empty */; volIt != volItEnd; ++volIt)
            {
                /**
                 * 1 TO N sync: for source, comparing the end point from settings and from command line args
                 */
                if (CanCreateFastOffloadAgent((*volumeGroupIt).direction, (*volumeGroupIt).id, volIt))
                {
                    // TODO: for now we can only have one volume in a volume group for fast sync so we
                    // need to create a new VOLUME_GROUP_SETTINGS to put this volume in
                    VOLUME_GROUP_SETTINGS vgs = (*volumeGroupIt);
                    DebugPrintf(SV_LOG_DEBUG, "For fast/offload sync, inserted device = %s, endpointdevice = %s, grp id = %d\n", 
                                (*volIt).second.deviceName.c_str(),
                                (*volIt).second.endpoints.begin()->deviceName.c_str(), (*volumeGroupIt).id);
                    DataProtecterPtr_t dataProtector = DataProtecterPtr_t(new DataProtector(m_Args, vgs));
                    SetUpFileXferLog(volIt->second);
                    m_DataProtectors.insert(std::make_pair((*volumeGroupIt).id, dataProtector));
                    breakout = true;
                    break;
                }
            }
        }
    }

    StopDeletedVolumeGroups(hostVolumeGroupSettings);
}

/*
 * FUNCTION NAME : DataProtection::CreateAll
 *
 * DESCRIPTION : create the dataprotectionsync objects via dataprotector create
 *               interface.
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
void DataProtection::CreateAll()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    DataProtectors_t::iterator endIt(m_DataProtectors.end());
    for (DataProtectors_t::iterator it = m_DataProtectors.begin(); it != endIt; ++it) {
        (*it).second->Create();
    }
}

void DataProtection::StartAll()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    DataProtectors_t::iterator endIt(m_DataProtectors.end());
    for (DataProtectors_t::iterator it = m_DataProtectors.begin(); it != endIt; ++it) {
        // TODO: for now since we only have 1 this will not return until it is done
        // in the future when we support more then 1 we will need to start these in their own threads
        (*it).second->Start();
    }
}

void DataProtection::StopAll()
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

    DataProtectors_t::iterator endIt(m_DataProtectors.end());
    for (DataProtectors_t::iterator it = m_DataProtectors.begin(); it != endIt; ++it) {
        (*it).second->Stop();
    }
}

void DataProtection::StopDeletedVolumeGroups(HOST_VOLUME_GROUP_SETTINGS& hostVolumeGroupSettings)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
    DataProtectors_t::iterator endIt(m_DataProtectors.end());
    for (DataProtectors_t::iterator it = m_DataProtectors.begin(); it != endIt; ++it) {
        HOST_VOLUME_GROUP_SETTINGS::volumeGroups_iterator volumeGroupEndIt(hostVolumeGroupSettings.volumeGroups.end());
        HOST_VOLUME_GROUP_SETTINGS::volumeGroups_iterator volumeGroupIt(hostVolumeGroupSettings.volumeGroups.begin());
        // TODO:: for efficency should use a set or map for HOST_VOLUME_GROUP_SETTINGS::volumeGroups_t
        // so that this code does not have to walk the list multiple times
        for ( /* already set */; volumeGroupIt != volumeGroupEndIt; ++volumeGroupIt) {
            if ((*it).first == (*volumeGroupIt).id) {
                break;
            }
        }
        if (volumeGroupIt == volumeGroupEndIt) {
            // no longer a rep job
            (*it).second->Stop();
            // TODO: may want to erase it from the list if so make sure to do it correctly for STL associative containers!
        }
    }
}


bool DataProtection::CanCreateFastOffloadAgent(const CDP_DIRECTION &direction, 
                                               const int &settingsgrpid,
                                               const VOLUME_GROUP_SETTINGS::volumes_iterator &volIt)
{
    bool bcanstart = false;

    //Step 1: devicename should match 
    if ((*volIt).first == m_Args.m_FastOffloadSyncArgs.m_DeviceName)
    {
        //Step 2: pair should not be in step 2 or diff sync
        if ((VOLUME_SETTINGS::SYNC_DIFF != (*volIt).second.syncMode)
            && (VOLUME_SETTINGS::SYNC_QUASIDIFF != (*volIt).second.syncMode))
        {
            //Step 3: If direction from settings from direction is source
            if (direction == SOURCE)
            {
                //Step 4: check if our cmd line args have source
                if (m_Args.m_FastOffloadSyncArgs.m_SourceSide)
                {
                    if (VOLUME_SETTINGS::SYNC_OFFLOAD != (*volIt).second.syncMode)
                    {
                        //If this not offload sync, then only check for end point
                        //This function should never be called for direct or diff sync
                        //TODO: What about SYNC_FAST?
                        const std::string &settingsendptdev = (*volIt).second.endpoints.begin()->deviceName;
                        const std::string &cmdlineendptdev = m_Args.m_FastOffloadSyncArgs.m_EndPointDeviceName;
                        const int &cmdlinegrpid = m_Args.m_FastOffloadSyncArgs.m_GroupId;
                        
                        if ((settingsendptdev == cmdlineendptdev) && (settingsgrpid == cmdlinegrpid))
                        { 
                            bcanstart = true;
                        }
                    }
                    else
                    {
                        bcanstart = true;
                    }
                }
            }
            else if (direction == TARGET)
            {
                if (!m_Args.m_FastOffloadSyncArgs.m_SourceSide)
                {
                    bcanstart = true;
                }
            }
        }
    }
  
    return bcanstart;
}

// --------------------------------------------------------------------------
// usage function
// --------------------------------------------------------------------------
void Usage(char const * name)
{
    std::stringstream msg;
    msg << "usage: \n"
        << "fast sync source   : " << name << " source fast <source volume> <target volume> <volume group id>\n"
        << "fast sync target   : " << name << " target fast <target volume>\n"
        << "offload sync source: " << name << " source offload <target volume>\n"
        << "offload sync target: " << name << " target offload <target volume>\n"
        << "diff sync target   : " << name << " target diff <src dir> <dest dir> <volume group id> <filename prefix> <agent type> <visible drives>\n";

    std::cerr << msg.str();

    //DebugPrintf(SV_LOG_INFO, "%s\n",msg.str().c_str());
}

