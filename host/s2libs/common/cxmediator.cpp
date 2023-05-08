#include <string>
#include <sstream>
#include <exception>
#include "transport_settings.h"
#include "configurevxagent.h"
#include "configurator2.h"
#include "resyncrequestor.h"
#include "cdputil.h"
#include "cxmediator.h"
#include "error.h"
#include "inmageex.h"

int CxMediator::UpdateMirrorState(
			           Configurator* cxConfigurator,
			           const std::string &sourceLunId, 
			           const PRISM_VOLUME_INFO::MIRROR_STATE mirrorStateRequested,
                       const PRISM_VOLUME_INFO::MIRROR_ERROR errCode,
                       const std::string &errorString)
{
    int iStatus = SV_FAILURE;
    int rval = -1;

    try
    {
        rval = cxConfigurator->getVxAgent().updateMirrorState( sourceLunId,
                                                               mirrorStateRequested,
                                                               errCode,
                                                               errorString );
    }
    catch( ContextualException& exception )
    {
        DebugPrintf( SV_LOG_ERROR,
                     "Failed to update mirror state. Call failed with: %s\n",
                     exception.what() ) ;
    }
    catch(...)
    {
        DebugPrintf( SV_LOG_ERROR,
                     "update mirror state call failed with Unknown Error\n" ) ;
    }

    iStatus = (0 == rval) ? SV_SUCCESS : SV_FAILURE;
    return iStatus;
}


int CxMediator::SendLastIOTimeStampOnATLUN(Configurator *cxConfigurator,
                                           const std::string &sourceVolumeName,
                                           const ATLUN_STATS_REQUEST &atLunStatsRequest,
                                           const SV_ULONGLONG ts)
{
    int iStatus = SV_FAILURE;
    int rval = -1;

    try
    {
        /* TODO: should the source volume name be added ? 
         * currently AT Lun name is enough */
        rval = cxConfigurator->getVxAgent().sendLastIOTimeStampOnATLUN(sourceVolumeName, ts);
    }
    catch( ContextualException& exception )
    {
        DebugPrintf( SV_LOG_ERROR,
                     "Failed to Send Last IO Start Time Stamp " ULLSPEC 
                     " on AT LUN %s, source volume %s. Call failed with: %s\n",
                     ts, atLunStatsRequest.atLUNName.c_str(), 
                     sourceVolumeName.c_str(), exception.what() ) ;
    }
    catch(...)
    {
        DebugPrintf( SV_LOG_ERROR,
                     "Failed to Send Last IO Start Time Stamp " ULLSPEC 
                     " on AT LUN %s, source volume %s. Call failed with unknown error\n",
                     ts, atLunStatsRequest.atLUNName.c_str(), 
                     sourceVolumeName.c_str() );
    }

    iStatus = (0 == rval) ? SV_SUCCESS : SV_FAILURE;
    return iStatus;
}


int CxMediator::UpdateVolumesPendingChanges(Configurator *cxConfigurator,
                                            const VolumesStats_t &vss,
											const std::list<std::string>& statsNotAvailableVolumes)
{
    int rval = -1;

    try
    {
        rval = cxConfigurator->getVxAgent().updateVolumesPendingChanges(vss, statsNotAvailableVolumes);
    }
    catch( ContextualException& exception )
    {
        DebugPrintf( SV_LOG_ERROR, "Failed to send pending changes count. Call failed with: %s\n", 
                     exception.what() ) ;
    }
    catch(...)
    {
        DebugPrintf( SV_LOG_ERROR, "Failed to send pending changes count. Call failed with unknown error\n");
    }

    int iStatus = (0 == rval) ? SV_SUCCESS : SV_FAILURE;
    return iStatus;
}

