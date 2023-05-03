/*
 * Copyright (c) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : cxcommunicator.h
 *
 * Description: 
 */

#ifndef CXMEDIATOR__H
#define CXMEDIATOR__H

#include <string>
#include "volumegroupsettings.h"
#include "prismsettings.h"
#include "mirrorerror.h"

struct Configurator;
class CxMediator {
public: 
    int UpdateMirrorState(
			Configurator* cxConfigurator,
			const std::string &sourceLunId, 
			const PRISM_VOLUME_INFO::MIRROR_STATE mirrorStateRequested,
            const PRISM_VOLUME_INFO::MIRROR_ERROR errcode,
            const std::string &errorString);

    int SendLastIOTimeStampOnATLUN(Configurator *cxConfigurator,
                                   const std::string &sourceVolumeName,
                                   const ATLUN_STATS_REQUEST &atLunStatsRequest,
                                   const SV_ULONGLONG ts);

    int UpdateVolumesPendingChanges(Configurator *cxConfigurator, 
                                    const VolumesStats_t &vss,
									const std::list<std::string>& statsNotAvailableVolumes);
};


#endif // ifndef CXMEDIATOR__H
