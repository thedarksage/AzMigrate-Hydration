//                                       
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
// 
// File       : dataprotection.h
// 
// Description: 
// 

#ifndef DATAPROTECTION_H
#define DATAPROTECTION_H

#include <map>
#include <string>

#include "dataprotectionBase.h"
#include "dataprotector.h"
#include "programargs.h"

class DataProtection : public DataProtectionBase {
public:
    typedef boost::shared_ptr<DataProtector> DataProtecterPtr_t;
    typedef std::map<int, DataProtecterPtr_t> DataProtectors_t;
    explicit DataProtection(ProgramArgs& args);
    virtual ~DataProtection();

    virtual void Protect();

    void HostVolumeGroupSettingsCallback(HOST_VOLUME_GROUP_SETTINGS HostVolGrpSettings);
    void CdpSettingsCallback();   

protected:
    void CreateAll();
    void StartAll();
    void StopAll();    
    virtual void ProcessHostVolumeGroupSettings(HOST_VOLUME_GROUP_SETTINGS& HostVolGrpSettings);
    void StopDeletedVolumeGroups(HOST_VOLUME_GROUP_SETTINGS& hostVolumeGroupSettings);
    bool CanCreateFastOffloadAgent(const CDP_DIRECTION &direction, const int &settingsgrpid,
                                   const VOLUME_GROUP_SETTINGS::volumes_iterator &volIt);
    bool HaveSettingsChangedForDataprotector(const HOST_VOLUME_GROUP_SETTINGS &hostVolumeGroupSettings);

    virtual std::string GetLogDir();

private:
    // no copying DataProtection
    DataProtection(const DataProtection& dataProtection);
    DataProtection& operator=(const DataProtection& dataProtection);

    ProgramArgs m_Args;

    DataProtectors_t m_DataProtectors;

    bool m_ReceviedConfiguratorCallback;

    bool m_StopProtectorOnConfiguratorChanges;

    bool m_ConfigurationChanged;
};

void Usage(char const * name);

#endif // ifndef DATAPROTECTION__H
