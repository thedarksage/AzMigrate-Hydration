//                                       
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
// 
// File       : dataprotector.h
// 
// Description: 
// 

#ifndef DATAPROTECTOR_H
#define DATAPROTECTOR_H

#include <string>
#include "programargs.h"
#include "volumegroupsettings.h"
#include "differentialsync.h"
#include "fastsync.h"
#include "localdirectsync.h"
#include "cdplock.h"

class DataProtector {
public:    
    DataProtector(ProgramArgs args, 
                                 const VOLUME_GROUP_SETTINGS& volumeGrpSettings)                                                                  
    : m_Args(args),
      m_VolumeGroupSettings(volumeGrpSettings),                
      m_FastSyncJob(NULL),
      m_DiffSyncJob(NULL),
      m_DirectSyncJob(NULL)

    { }

    ~DataProtector() { 
        delete m_FastSyncJob;
        delete m_DiffSyncJob;
        delete m_DirectSyncJob;
    }

    void Create();
    void Start();
    void Stop();
    void UpdateVolumeGroupSettings(VOLUME_GROUP_SETTINGS& volumeGroupSettings);   
	bool HaveSettingsChanged(const HOST_VOLUME_GROUP_SETTINGS &hostVolumeGroupSettings);

protected:   

private:    
    // no copying DataProtector
    DataProtector(const DataProtector& dataProtector);
    DataProtector& operator=(const DataProtector& dataProtector);
    std::vector<CDPLock::Ptr> cdplocks;
    ProgramArgs m_Args;

    VOLUME_GROUP_SETTINGS m_VolumeGroupSettings;          

    FastSync* m_FastSyncJob;

    DifferentialSync* m_DiffSyncJob;

	LocalDirectSync *m_DirectSyncJob;
};

#endif // ifndef DATAPROTECTOR__H
