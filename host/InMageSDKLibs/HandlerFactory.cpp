#include "HandlerFactory.h"
#include "LicencingHandler.h"
#include "RecoveryHandler.h"
#include "MonitorHandler.h"
#include "VolumeHandler.h"
#include "ProtectionHandler.h"
#include "RepositoryHandler.h"
#include "VmAssociationHandler.h"
#include "HostHandler.h"
#include "StubHandler.h"

#include "appsettingshandler.h"
#include "hostdiscoveryhandler.h"
#include "settingshandler.h"
#include "resynchandler.h"
#include "sourcehandler.h"
#include "targethandler.h"
#include "appsettingshandler.h"
#include "policyhandler.h"
#include "paircreationhandler.h"
#include "apinames.h"
#include "snapshotshandler.h"

#include "inmstrcmp.h"

HandlerFactory::HandlerFactory(void)
{
}

HandlerFactory::~HandlerFactory(void)
{
}

boost::shared_ptr<Handler> HandlerFactory::getHandler(const std::string& id, //Authenticatin ID
		const std::string& functionName,		//Name of the requested API
		const std::string& funcReqId,			//Function requst ID
		const std::string& funcIncludes,		//Includes
		INM_ERROR& errCode)
{
    
	HandlerInfo hInfo;
	hInfo.functionName = functionName;
	hInfo.authId = strToUpper(md5(id));
	hInfo.funcReqId = funcReqId;
	hInfo.funcIncludes = funcIncludes;
	errCode = findHandler(hInfo);
	boost::shared_ptr<Handler> pResHandler ;
	//Failure in findHandler() leads to StubHandler.
	switch(hInfo.funcHandler)
	{
	case HANDLER_LICENSING:
		pResHandler.reset(new LicencingHandler());
		break;
	case HANDLER_MONITOR:
		pResHandler.reset(new MonitorHandler());
		break;
	case HANDLER_PROTECTION:
		pResHandler.reset(new ProtectionHandler());
		break;
	case HANDLER_RECOVERY:
		pResHandler.reset(new RecoveryHandler());
		break;
	case HANDLER_REPOSITORY:
		pResHandler.reset(new RepositoryHandler());
		break;
	case HANDLER_VOLUME:
		pResHandler.reset(new VolumeHandler());
		break;
	// TODO: open this once unix compilation is done
    case HANDLER_HOSTDISCOVERY:
            pResHandler .reset(new HostDiscoveryHandler());
            break;
    case HANDLER_SETTINGS:
            pResHandler.reset(new SettingsHandler());
            break;
	case HANDLER_RESYNC:
            pResHandler .reset(new ResyncHandler());
            break;
    case HANDLER_SOURCE:
            pResHandler .reset(new SourceHandler());
            break;
    case HANDLER_TARGET:
            pResHandler.reset(new TargetHandler());
            break;
    case HANDLER_APP_SETTINGS:
        pResHandler .reset(new AppSettingsHandler());
        break;
    case HANDLER_POLICY:
        pResHandler .reset(new PolicyHandler());
        break; 
    case HANDLER_PAIRCREATION:
        pResHandler .reset(new PairCreationHandler());
        break;
    case HANDLER_SNAPSHOTS:
        pResHandler .reset(new SnapShotHandler());
        break ;
	default:
		pResHandler.reset( new StubHandler());
		break;
	}
	if(pResHandler.get())
	{
		pResHandler->setHandlerInfo(hInfo);
	}
	else
		errCode = E_INTERNAL;
	return pResHandler ;
}
boost::shared_ptr<Handler> HandlerFactory::getHandler(const std::string& functionName,//Name of the requested API
		const std::string& funcReqId,			//Function requst ID
		INM_ERROR& errCode )
{
	HandlerInfo hInfo;
	hInfo.functionName = functionName;
	hInfo.funcReqId = funcReqId;
	errCode = findHandler(hInfo);
	boost::shared_ptr<Handler> pResHandler ;
	//Failure in findHandler() leads to StubHandler.
	switch(hInfo.funcHandler)
	{
	case HANDLER_LICENSING:
		pResHandler.reset(new LicencingHandler());
		break;
	case HANDLER_MONITOR:
		pResHandler.reset(new MonitorHandler());
		break;
	case HANDLER_PROTECTION:
		pResHandler.reset(new ProtectionHandler());
		break;
	case HANDLER_RECOVERY:
		pResHandler.reset(new RecoveryHandler());
		break;
	case HANDLER_REPOSITORY:
		pResHandler.reset(new RepositoryHandler());
		break;
	case HANDLER_VOLUME:
		pResHandler.reset(new VolumeHandler());
		break;
	// TODO: open this once unix compilation is done
	case HANDLER_HOSTDISCOVERY:
        pResHandler.reset(new HostDiscoveryHandler());
        break;
    case HANDLER_SETTINGS:
        pResHandler.reset(new SettingsHandler());
        break;
	case HANDLER_RESYNC:
            pResHandler.reset(new ResyncHandler());
            break;
    case HANDLER_SOURCE:
            pResHandler.reset(new SourceHandler());
            break;
    case HANDLER_TARGET:
            pResHandler.reset(new TargetHandler());
            break;
    case HANDLER_APP_SETTINGS:
        pResHandler.reset(new AppSettingsHandler());
            break;
    case HANDLER_POLICY:
        pResHandler.reset(new PolicyHandler());
            break; 
    case HANDLER_PAIRCREATION:
        pResHandler.reset(new PairCreationHandler());
            break;
    case HANDLER_SNAPSHOTS :
        pResHandler.reset(new SnapShotHandler());
        break ;
	default:
		pResHandler.reset(new StubHandler());
		break;
	}
	if(pResHandler.get())
	{
		(pResHandler)->setHandlerInfo(hInfo);
	}
	else
	{
		errCode = E_INTERNAL;
	}
	return pResHandler ;
}
boost::shared_ptr<Handler> HandlerFactory::getStubHandler(INM_ERROR& errCode )
{
	boost::shared_ptr<Handler> pHandler ;
	pHandler.reset(new StubHandler());
	if(pHandler.get())
	{
		errCode = E_INTERNAL;
	}
	return pHandler ;
}
INM_ERROR HandlerFactory::findHandler(HandlerInfo& hInfo)
{
	if(isProtectionHandler(hInfo))
	{
		hInfo.funcHandler = HANDLER_PROTECTION;
		return E_SUCCESS;
	}
	else if(isRepositoryHandler(hInfo))
	{
		hInfo.funcHandler = HANDLER_REPOSITORY;
		return E_SUCCESS;
	}
	else if(isMonitorHandler(hInfo))
	{
		hInfo.funcHandler = HANDLER_MONITOR;
		return E_SUCCESS;
	}
	else if(isVolumeHandler(hInfo))
	{
		hInfo.funcHandler = HANDLER_VOLUME;
		return E_SUCCESS;
	}
	else if(isRecoveryHandler(hInfo))
	{
		hInfo.funcHandler = HANDLER_RECOVERY;
		return E_SUCCESS;
	}
	else if(isLicencingHandler(hInfo))
	{
		hInfo.funcHandler = HANDLER_LICENSING;
		return E_SUCCESS;
	}
	else if(isVmAssociationHandler(hInfo))
	{
		hInfo.funcHandler = HANDLER_VM_ASSOCIATION;
		return E_SUCCESS;
	}
	else if(isHostHandler(hInfo))
	{
		hInfo.funcHandler = HANDLER_HOST;
		return E_SUCCESS;
	}
	
    else if(isHostDiscoveryHandler(hInfo))
    {
        hInfo.funcHandler = HANDLER_HOSTDISCOVERY;
        return E_SUCCESS;
    }
    else if(isSettingsHandler(hInfo))
    {
        hInfo.funcHandler = HANDLER_SETTINGS;
        return E_SUCCESS;
    }
    else if(isResyncHandler(hInfo))
    {
         hInfo.funcHandler = HANDLER_RESYNC;
        return E_SUCCESS;
    }
    else if(isSourceHandler(hInfo))
    {
        hInfo.funcHandler = HANDLER_SOURCE;
        return E_SUCCESS;
    }
    else if(isTargetHandler(hInfo))
    {
        hInfo.funcHandler = HANDLER_TARGET;
        return E_SUCCESS;
    }
    else if(isAppSettingsHandler(hInfo))
    {
        hInfo.funcHandler = HANDLER_APP_SETTINGS;
        return E_SUCCESS;
    }
    else if(isPolicyHandler(hInfo))
    {
        hInfo.funcHandler = HANDLER_POLICY;
        return E_SUCCESS;
    }
    else if(isPairCreationHandler(hInfo))
    {
        hInfo.funcHandler = HANDLER_PAIRCREATION;
        return E_SUCCESS;
    }
    else if(isSnapshothandler(hInfo))
    {
        hInfo.funcHandler = HANDLER_SNAPSHOTS ;
        return E_SUCCESS ;
    }
	
	return E_UNKNOWN_FUNCTION;
}

bool HandlerFactory::isHostHandler(HandlerInfo& hInfo)
{
	if(!InmStrCmp<NoCaseCharCmp>("ListHosts",hInfo.functionName.c_str()))
	{
		//Not supported.
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("GetHostByIP",hInfo.functionName.c_str()))
	{
		//Not supported.
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("GetHostByName",hInfo.functionName.c_str()))
	{
		//Not supported.
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("GetHostBySystemGUID",hInfo.functionName.c_str()))
	{
		//Not supported.
		return true;
	}
	return false;
}
bool HandlerFactory::isLicencingHandler(HandlerInfo& hInfo)
{
	if(!InmStrCmp<NoCaseCharCmp>("UpdateProtectionPolicies",hInfo.functionName.c_str()))
	{
		hInfo.funcSupported = true;
		if(hInfo.authId.compare("F82A8D0F02B0BAC25DCA1C4116A54DCE") == 0)
		{
			hInfo.access = true;
		}
		else if(hInfo.authId.compare("FF97A9FDEDE09EAF6E1C8EC9F6A61DD5") == 0)
		{
			hInfo.access = true;
		}
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("UpdateProtectionPoliciesBySystemGUID",hInfo.functionName.c_str()))
	{
		//Not supported.
		return true;
	}
    else if( InmStrCmp<NoCaseCharCmp>("GetNodeLockString",hInfo.functionName.c_str()) == 0 )
    {
        hInfo.funcSupported = true;
		return true ;
    }
    else if( InmStrCmp<NoCaseCharCmp>("OnLicenseExpiry",hInfo.functionName.c_str())== 0 )
    {
        hInfo.funcSupported = true;
		return true ;
    }
    else if( InmStrCmp<NoCaseCharCmp>("OnLicenseExtension",hInfo.functionName.c_str())== 0 )
    {
        hInfo.funcSupported = true;
		return true ;
    }
    else if( InmStrCmp<NoCaseCharCmp>("IsLicenseExpired",hInfo.functionName.c_str())== 0 )
    {
        hInfo.funcSupported = true;
		return true ;
    }

	return false;
}
bool HandlerFactory::isMonitorHandler(HandlerInfo& hInfo)
{
	if(!InmStrCmp<NoCaseCharCmp>("GetProtectionDetails",hInfo.functionName.c_str()))
	{
		hInfo.funcSupported = true;
		if(hInfo.authId.compare("F82A8D0F02B0BAC25DCA1C4116A54DCE") == 0)
		{
			hInfo.access = true;
		}
		else if(hInfo.authId.compare("FF97A9FDEDE09EAF6E1C8EC9F6A61DD5") == 0)
		{
			hInfo.access = true;
		}
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("GetProtectionDetailsBySystemGUID",hInfo.functionName.c_str()))
	{
		//Not supported.
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("GetErrorLogPath",hInfo.functionName.c_str()))
	{
		hInfo.funcSupported = true;
		if(hInfo.authId.compare("F82A8D0F02B0BAC25DCA1C4116A54DCE") == 0)
		{
			hInfo.access = true;
		}
		else if(hInfo.authId.compare("FF97A9FDEDE09EAF6E1C8EC9F6A61DD5") == 0)
		{
			hInfo.access = true;
		}
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("DownloadAlertsForSystemGUID",hInfo.functionName.c_str()))
	{
		//Not supported.
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("DownloadAlerts",hInfo.functionName.c_str()))
	{
		hInfo.funcSupported = true;
		if(hInfo.authId.compare("F82A8D0F02B0BAC25DCA1C4116A54DCE") == 0)
		{
			hInfo.access = true;
		}
		else if(hInfo.authId.compare("FF97A9FDEDE09EAF6E1C8EC9F6A61DD5") == 0)
		{
			hInfo.access = true;
		}
		return true;
	}
    else if(!InmStrCmp<NoCaseCharCmp>("ListVolumesWithProtectionDetails",hInfo.functionName.c_str()))
    {
        hInfo.funcSupported = true;
        if(hInfo.authId.compare("F82A8D0F02B0BAC25DCA1C4116A54DCE") == 0)
        {
            hInfo.access = true;
        }
        else if(hInfo.authId.compare("FF97A9FDEDE09EAF6E1C8EC9F6A61DD5") == 0)
        {
            hInfo.access = true;
        }
        return true;
    }
	else if(!InmStrCmp<NoCaseCharCmp>("DownloadAudit",hInfo.functionName.c_str()))
	{
		hInfo.funcSupported = true;
		if(hInfo.authId.compare("F82A8D0F02B0BAC25DCA1C4116A54DCE") == 0)
		{
			hInfo.access = true;
		}
		else if(hInfo.authId.compare("FF97A9FDEDE09EAF6E1C8EC9F6A61DD5") == 0)
		{
			hInfo.access = true;
		}
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("HealthStatus",hInfo.functionName.c_str()))
	{
		hInfo.funcSupported = true;
		if(hInfo.authId.compare("F82A8D0F02B0BAC25DCA1C4116A54DCE") == 0)
		{
			hInfo.access = true;
		}
		else if(hInfo.authId.compare("FF97A9FDEDE09EAF6E1C8EC9F6A61DD5") == 0)
		{
			hInfo.access = true;
		}
		return true;
	}
	return false;
}
bool HandlerFactory::isProtectionHandler(HandlerInfo& hInfo)
{
	if(!InmStrCmp<NoCaseCharCmp>("SetupBackupProtection",hInfo.functionName.c_str()))
	{
		hInfo.funcSupported = true;
		if(hInfo.authId.compare("F82A8D0F02B0BAC25DCA1C4116A54DCE") == 0)
		{
			hInfo.access = true;
		}
		else if(hInfo.authId.compare("FF97A9FDEDE09EAF6E1C8EC9F6A61DD5") == 0)
		{
			hInfo.access = true;
		}
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("SetupBackupProtectionBySystemGUID",hInfo.functionName.c_str()))
	{
		//Not supported.
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("SpaceRequired",hInfo.functionName.c_str()))
	{
		hInfo.funcSupported = true;
		if(hInfo.authId.compare("F82A8D0F02B0BAC25DCA1C4116A54DCE") == 0)
		{
			hInfo.access = true;
		}
		else if(hInfo.authId.compare("FF97A9FDEDE09EAF6E1C8EC9F6A61DD5") == 0)
		{
			hInfo.access = true;
		}
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("DeleteBackupProtection",hInfo.functionName.c_str()))
	{
		hInfo.funcSupported = true;
		if(hInfo.authId.compare("F82A8D0F02B0BAC25DCA1C4116A54DCE") == 0)
		{
			hInfo.access = true;
		}
		else if(hInfo.authId.compare("FF97A9FDEDE09EAF6E1C8EC9F6A61DD5") == 0)
		{
			hInfo.access = true;
		}
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("DeleteBackupProtectionBySystemGUID",hInfo.functionName.c_str()))
	{
		//Not supported.
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("ResyncProtection",hInfo.functionName.c_str()))
	{
		hInfo.funcSupported = true;
		if(hInfo.authId.compare("F82A8D0F02B0BAC25DCA1C4116A54DCE") == 0)
		{
			hInfo.access = true;
		}
		else if(hInfo.authId.compare("FF97A9FDEDE09EAF6E1C8EC9F6A61DD5") == 0)
		{
			hInfo.access = true;
		}
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("ResyncProtectionBySystemGUID",hInfo.functionName.c_str()))
	{
		//Not supported.
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("ResumeTracking",hInfo.functionName.c_str()))
	{
		hInfo.funcSupported = true;
		if(hInfo.authId.compare("F82A8D0F02B0BAC25DCA1C4116A54DCE") == 0)
		{
			hInfo.access = true;
		}
		else if(hInfo.authId.compare("FF97A9FDEDE09EAF6E1C8EC9F6A61DD5") == 0)
		{
			hInfo.access = true;
		}
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("ResumeTrackingForVolume",hInfo.functionName.c_str()))
	{
		hInfo.funcSupported = true;
		if(hInfo.authId.compare("F82A8D0F02B0BAC25DCA1C4116A54DCE") == 0)
		{
			hInfo.access = true;
		}
		else if(hInfo.authId.compare("FF97A9FDEDE09EAF6E1C8EC9F6A61DD5") == 0)
		{
			hInfo.access = true;
		}
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("ResumeProtectionWithResyncByAgentGUID",hInfo.functionName.c_str()))
	{
		//Not supported.
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("PauseTracking",hInfo.functionName.c_str()))
	{
		hInfo.funcSupported = true;
		if(hInfo.authId.compare("F82A8D0F02B0BAC25DCA1C4116A54DCE") == 0)
		{
			hInfo.access = true;
		}
		else if(hInfo.authId.compare("FF97A9FDEDE09EAF6E1C8EC9F6A61DD5") == 0)
		{
			hInfo.access = true;
		}
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("PauseTrackingForAgentGUID",hInfo.functionName.c_str()))
	{
		//Not supported.
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("PauseTrackingForVolume",hInfo.functionName.c_str()))
	{
		hInfo.funcSupported = true;
		if(hInfo.authId.compare("F82A8D0F02B0BAC25DCA1C4116A54DCE") == 0)
		{
			hInfo.access = true;
		}
		else if(hInfo.authId.compare("FF97A9FDEDE09EAF6E1C8EC9F6A61DD5") == 0)
		{
			hInfo.access = true;
		}
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("PauseTrackingForVolumeForAgentGUID",hInfo.functionName.c_str()))
	{
		//Not supported.
		return true;
	}
    else if(!InmStrCmp<NoCaseCharCmp>("PauseProtection",hInfo.functionName.c_str()))
	{
		hInfo.funcSupported = true;
		if(hInfo.authId.compare("F82A8D0F02B0BAC25DCA1C4116A54DCE") == 0)
		{
			hInfo.access = true;
		}
		else if(hInfo.authId.compare("FF97A9FDEDE09EAF6E1C8EC9F6A61DD5") == 0)
		{
			hInfo.access = true;
		}
		return true;
	}
    else if(!InmStrCmp<NoCaseCharCmp>("PauseProtectionForVolume",hInfo.functionName.c_str()))
	{
		hInfo.funcSupported = true;
		if(hInfo.authId.compare("F82A8D0F02B0BAC25DCA1C4116A54DCE") == 0)
		{
			hInfo.access = true;
		}
		else if(hInfo.authId.compare("FF97A9FDEDE09EAF6E1C8EC9F6A61DD5") == 0)
		{
			hInfo.access = true;
		}
		return true;
	}
    else if(!InmStrCmp<NoCaseCharCmp>("ResumeProtection",hInfo.functionName.c_str()))
	{
		hInfo.funcSupported = true;
		if(hInfo.authId.compare("F82A8D0F02B0BAC25DCA1C4116A54DCE") == 0)
		{
			hInfo.access = true;
		}
		else if(hInfo.authId.compare("FF97A9FDEDE09EAF6E1C8EC9F6A61DD5") == 0)
		{
			hInfo.access = true;
		}
		return true;
	}
    else if(!InmStrCmp<NoCaseCharCmp>("ResumeProtectionForVolume",hInfo.functionName.c_str()))
	{
		hInfo.funcSupported = true;
		if(hInfo.authId.compare("F82A8D0F02B0BAC25DCA1C4116A54DCE") == 0)
		{
			hInfo.access = true;
		}
		else if(hInfo.authId.compare("FF97A9FDEDE09EAF6E1C8EC9F6A61DD5") == 0)
		{
			hInfo.access = true;
		}
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("IssueConsistency",hInfo.functionName.c_str()))
	{
		hInfo.funcSupported = true;
		if(hInfo.authId.compare("F82A8D0F02B0BAC25DCA1C4116A54DCE") == 0)
		{
			hInfo.access = true;
		}
		else if(hInfo.authId.compare("FF97A9FDEDE09EAF6E1C8EC9F6A61DD5") == 0)
		{
			hInfo.access = true;
		}
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("AddVolumes",hInfo.functionName.c_str()))
	{
		hInfo.funcSupported = true;
		if(hInfo.authId.compare("F82A8D0F02B0BAC25DCA1C4116A54DCE") == 0)
		{
			hInfo.access = true;
		}
		else if(hInfo.authId.compare("FF97A9FDEDE09EAF6E1C8EC9F6A61DD5") == 0)
		{
			hInfo.access = true;
		}
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("ListHosts",hInfo.functionName.c_str()))
	{
		hInfo.funcSupported = true;
		if(hInfo.authId.compare("F82A8D0F02B0BAC25DCA1C4116A54DCE") == 0)
		{
			hInfo.access = true;
		}
		else if(hInfo.authId.compare("FF97A9FDEDE09EAF6E1C8EC9F6A61DD5") == 0)
		{
			hInfo.access = true;
		}
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("ListHostsMinimal",hInfo.functionName.c_str()))
	{
		hInfo.funcSupported = true;
		if(hInfo.authId.compare("F82A8D0F02B0BAC25DCA1C4116A54DCE") == 0)
		{
			hInfo.access = true;
		}
		else if(hInfo.authId.compare("FF97A9FDEDE09EAF6E1C8EC9F6A61DD5") == 0)
		{
			hInfo.access = true;
		}
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("ExportRepositoryOnCIFS",hInfo.functionName.c_str()))
	{
		hInfo.funcSupported = true;
		if(hInfo.authId.compare("F82A8D0F02B0BAC25DCA1C4116A54DCE") == 0)
		{
			hInfo.access = true;
		}
		else if(hInfo.authId.compare("FF97A9FDEDE09EAF6E1C8EC9F6A61DD5") == 0)
		{
			hInfo.access = true;
		}
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("GetExportedRepositoryDetails",hInfo.functionName.c_str() ) || 
            !InmStrCmp<NoCaseCharCmp>("GetExportedRepositoryDetailsByAgentGUID",hInfo.functionName.c_str() ) )
	{
		hInfo.funcSupported = true;
		if(hInfo.authId.compare("F82A8D0F02B0BAC25DCA1C4116A54DCE") == 0)
		{
			hInfo.access = true;
		}
		else if(hInfo.authId.compare("FF97A9FDEDE09EAF6E1C8EC9F6A61DD5") == 0)
		{
			hInfo.access = true;
		}
		return true;
	}
	else if( !InmStrCmp<NoCaseCharCmp>("SetLogRotationInterval",hInfo.functionName.c_str()) )
	{
		return true ;
	}
	return false;
}
bool HandlerFactory::isRecoveryHandler(HandlerInfo& hInfo)
{
	if(!InmStrCmp<NoCaseCharCmp>("ListRestorePoints",hInfo.functionName.c_str()))
	{
		hInfo.funcSupported = true;
		if(hInfo.authId.compare("F82A8D0F02B0BAC25DCA1C4116A54DCE") == 0)
		{
			hInfo.access = true;
		}
		else if(hInfo.authId.compare("FF97A9FDEDE09EAF6E1C8EC9F6A61DD5") == 0)
		{
			hInfo.access = true;
		}
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("GetRestorePointsBySystemGUID",hInfo.functionName.c_str()))
	{
		//Not supported.
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("RecoverVolumesToLCCP",hInfo.functionName.c_str()))
	{
		hInfo.funcSupported = true;
		if(hInfo.authId.compare("F82A8D0F02B0BAC25DCA1C4116A54DCE") == 0)
		{
			hInfo.access = true;
		}
		else if(hInfo.authId.compare("FF97A9FDEDE09EAF6E1C8EC9F6A61DD5") == 0)
		{
			hInfo.access = true;
		}
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("RecoverVolumeToLCT",hInfo.functionName.c_str()))
	{
		hInfo.funcSupported = true;
		if(hInfo.authId.compare("F82A8D0F02B0BAC25DCA1C4116A54DCE") == 0)
		{
			hInfo.access = true;
		}
		else if(hInfo.authId.compare("FF97A9FDEDE09EAF6E1C8EC9F6A61DD5") == 0)
		{
			hInfo.access = true;
		}
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("RecoverVolumeToRestorePoint",hInfo.functionName.c_str()))
	{
		hInfo.funcSupported = true;
		if(hInfo.authId.compare("F82A8D0F02B0BAC25DCA1C4116A54DCE") == 0)
		{
			hInfo.access = true;
		}
		else if(hInfo.authId.compare("FF97A9FDEDE09EAF6E1C8EC9F6A61DD5") == 0)
		{
			hInfo.access = true;
		}
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("RecoverVolumesToLCCPForAgentGUID",hInfo.functionName.c_str()))
	{
		//Not supported.
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("RecoverVolumeToLCTForAgentGUID",hInfo.functionName.c_str()))
	{
		//Not supported.
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("RecoverVolumeToRestorePointForAgentGUID",hInfo.functionName.c_str()))
	{
		//Not supported.
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("SnapshotVolumeToLCP",hInfo.functionName.c_str()))
	{
		hInfo.funcSupported = true;
		if(hInfo.authId.compare("F82A8D0F02B0BAC25DCA1C4116A54DCE") == 0)
		{
			hInfo.access = true;
		}
		else if(hInfo.authId.compare("FF97A9FDEDE09EAF6E1C8EC9F6A61DD5") == 0)
		{
			hInfo.access = true;
		}
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("SnapshotVolumeToLT",hInfo.functionName.c_str()))
	{
		hInfo.funcSupported = true;
		if(hInfo.authId.compare("F82A8D0F02B0BAC25DCA1C4116A54DCE") == 0)
		{
			hInfo.access = true;
		}
		else if(hInfo.authId.compare("FF97A9FDEDE09EAF6E1C8EC9F6A61DD5") == 0)
		{
			hInfo.access = true;
		}
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("SnapshotVolumeTorestorePoint",hInfo.functionName.c_str()))
	{
		hInfo.funcSupported = true;
		if(hInfo.authId.compare("F82A8D0F02B0BAC25DCA1C4116A54DCE") == 0)
		{
			hInfo.access = true;
		}
		else if(hInfo.authId.compare("FF97A9FDEDE09EAF6E1C8EC9F6A61DD5") == 0)
		{
			hInfo.access = true;
		}
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("SnapshotVolumeToLCPForAgentGUID",hInfo.functionName.c_str()))
	{
		//Not supported.
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("SnapshotVolumeToLTForAgentGUID",hInfo.functionName.c_str()))
	{
		//Not supported.
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("SnapshotVolumeToRestorePointForAgentGUID",hInfo.functionName.c_str()))
	{
		//Not supported.
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("GetSnapshotExportedPath",hInfo.functionName.c_str()))
	{
		//Not supported.
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("ImportMBRForAgentGUID",hInfo.functionName.c_str()))
	{
		//Not supported.
		return true;
	}
	return false;
}
bool HandlerFactory::isRepositoryHandler(HandlerInfo& hInfo)
{
	if(!InmStrCmp<NoCaseCharCmp>("ListRepositoryDevices",hInfo.functionName.c_str()))
	{
		hInfo.funcSupported = true;
		if(hInfo.authId.compare("F82A8D0F02B0BAC25DCA1C4116A54DCE") == 0)
		{
			hInfo.access = true;
		}
		else if(hInfo.authId.compare("FF97A9FDEDE09EAF6E1C8EC9F6A61DD5") == 0)
		{
			hInfo.access = true;
		}
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("ListConfiguredRepositories",hInfo.functionName.c_str()))
	{
		hInfo.funcSupported = true;
		if(hInfo.authId.compare("F82A8D0F02B0BAC25DCA1C4116A54DCE") == 0)
		{
			hInfo.access = true;
		}
		else if(hInfo.authId.compare("FF97A9FDEDE09EAF6E1C8EC9F6A61DD5") == 0)
		{
			hInfo.access = true;
		}
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("SetupRepository",hInfo.functionName.c_str()))
	{
		hInfo.funcSupported = true;
		if(hInfo.authId.compare("F82A8D0F02B0BAC25DCA1C4116A54DCE") == 0)
		{
			hInfo.access = true;
		}
		else if(hInfo.authId.compare("FF97A9FDEDE09EAF6E1C8EC9F6A61DD5") == 0)
		{
			hInfo.access = true;
		}
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("UpdateCredentials",hInfo.functionName.c_str()))
	{
		hInfo.funcSupported = true;
		if(hInfo.authId.compare("F82A8D0F02B0BAC25DCA1C4116A54DCE") == 0)
		{
			hInfo.access = true;
		}
		else if(hInfo.authId.compare("FF97A9FDEDE09EAF6E1C8EC9F6A61DD5") == 0)
		{
			hInfo.access = true;
		}
		return true;
	}
	
	else if(!InmStrCmp<NoCaseCharCmp>("SetupRepositoryBySystemGUID",hInfo.functionName.c_str()))
	{
		//Not supported
		return true;
	}
    else if(!InmStrCmp<NoCaseCharCmp>("DeleteRepository",hInfo.functionName.c_str()))
    {
		hInfo.funcSupported = true;
		if(hInfo.authId.compare("F82A8D0F02B0BAC25DCA1C4116A54DCE") == 0)
		{
			hInfo.access = true;
		}
		else if(hInfo.authId.compare("FF97A9FDEDE09EAF6E1C8EC9F6A61DD5") == 0)
		{
			hInfo.access = true;
		}
		return true;
    }
    else if(!InmStrCmp<NoCaseCharCmp>("ReconstructRepo",hInfo.functionName.c_str()))
    {
        hInfo.funcSupported = true;
        if(hInfo.authId.compare("F82A8D0F02B0BAC25DCA1C4116A54DCE") == 0)
        {
            hInfo.access = true;
        }
        else if(hInfo.authId.compare("FF97A9FDEDE09EAF6E1C8EC9F6A61DD5") == 0)
        {
            hInfo.access = true;
        }
        return true;
    }
    else if(!InmStrCmp<NoCaseCharCmp>("MoveRepository",hInfo.functionName.c_str()))
	{
		hInfo.funcSupported = true;
		if(hInfo.authId.compare("F82A8D0F02B0BAC25DCA1C4116A54DCE") == 0)
		{
			hInfo.access = true;
		}
		else if(hInfo.authId.compare("FF97A9FDEDE09EAF6E1C8EC9F6A61DD5") == 0)
		{
			hInfo.access = true;
		}
		return true;
	}
    else if(!InmStrCmp<NoCaseCharCmp>("ArchiveRepository",hInfo.functionName.c_str()))
	{
		hInfo.funcSupported = true;
		if(hInfo.authId.compare("F82A8D0F02B0BAC25DCA1C4116A54DCE") == 0)
		{
			hInfo.access = true;
		}
		else if(hInfo.authId.compare("FF97A9FDEDE09EAF6E1C8EC9F6A61DD5") == 0)
		{
			hInfo.access = true;
		}
		return true;
	}

	else if( !InmStrCmp<NoCaseCharCmp>("SetCompression",hInfo.functionName.c_str()))
	{
		hInfo.funcSupported = true;
		if(hInfo.authId.compare("F82A8D0F02B0BAC25DCA1C4116A54DCE") == 0)
		{
			hInfo.access = true;
		}
		else if(hInfo.authId.compare("FF97A9FDEDE09EAF6E1C8EC9F6A61DD5") == 0)
		{
			hInfo.access = true;
		}
		return true;
	}
	else if( !InmStrCmp<NoCaseCharCmp>("ChangeBackupLocation", hInfo.functionName.c_str()) )
	{
		hInfo.funcSupported = true;
		if(hInfo.authId.compare("F82A8D0F02B0BAC25DCA1C4116A54DCE") == 0)
		{
			hInfo.access = true;
		}
		else if(hInfo.authId.compare("FF97A9FDEDE09EAF6E1C8EC9F6A61DD5") == 0)
		{
			hInfo.access = true;
		}
		return true;
	}
	else if( !InmStrCmp<NoCaseCharCmp>("ShouldMoveOrArchiveRepository", hInfo.functionName.c_str()) )
	{
		hInfo.funcSupported = true;
		if(hInfo.authId.compare("F82A8D0F02B0BAC25DCA1C4116A54DCE") == 0)
		{
			hInfo.access = true;
		}
		else if(hInfo.authId.compare("FF97A9FDEDE09EAF6E1C8EC9F6A61DD5") == 0)
		{
			hInfo.access = true;
		}
		return true;
	}
	return false;
}

bool HandlerFactory::isVmAssociationHandler(HandlerInfo& hInfo)
{
	if(!InmStrCmp<NoCaseCharCmp>("AssociateSystemWithBackupServer",hInfo.functionName.c_str()))
	{
		//Not supported.
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("UploadSystemMetadata",hInfo.functionName.c_str()))
	{
		//Not supported.
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("RetrieveSystemMetadata",hInfo.functionName.c_str()))
	{
		//Not supported.
		return true;
	}
	return false;
}
bool HandlerFactory::isVolumeHandler(HandlerInfo& hInfo)
{
	if(!InmStrCmp<NoCaseCharCmp>("ListVolumes",hInfo.functionName.c_str()))
	{
		hInfo.funcSupported = true;
		if(hInfo.authId.compare("F82A8D0F02B0BAC25DCA1C4116A54DCE") == 0)
		{
			hInfo.access = true;
		}
		else if(hInfo.authId.compare("FF97A9FDEDE09EAF6E1C8EC9F6A61DD5") == 0)
		{
			hInfo.access = true;
		}
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("ListProtectableVolumes",hInfo.functionName.c_str()))
	{
		hInfo.funcSupported = true;
		if(hInfo.authId.compare("F82A8D0F02B0BAC25DCA1C4116A54DCE") == 0)
		{
			hInfo.access = true;
		}
		else if(hInfo.authId.compare("FF97A9FDEDE09EAF6E1C8EC9F6A61DD5") == 0)
		{
			hInfo.access = true;
		}
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("ListAvailableDrives",hInfo.functionName.c_str()))
	{
		hInfo.funcSupported = true;
		if(hInfo.authId.compare("F82A8D0F02B0BAC25DCA1C4116A54DCE") == 0)
		{
			hInfo.access = true;
		}
		else if(hInfo.authId.compare("FF97A9FDEDE09EAF6E1C8EC9F6A61DD5") == 0)
		{
			hInfo.access = true;
		}
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("ListProtectedVolumes",hInfo.functionName.c_str()))
	{
		hInfo.funcSupported = true;
		if(hInfo.authId.compare("F82A8D0F02B0BAC25DCA1C4116A54DCE") == 0)
		{
			hInfo.access = true;
		}
		else if(hInfo.authId.compare("FF97A9FDEDE09EAF6E1C8EC9F6A61DD5") == 0)
		{
			hInfo.access = true;
		}
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("ListVolumesByAgentGUID",hInfo.functionName.c_str()))
	{
		//Not supported.
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("ListProtectableVolumesByAgentGUID",hInfo.functionName.c_str()))
	{
		//Not supported.
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("ListProtectableVolumesBySystemGUID",hInfo.functionName.c_str()))
	{
		//Not supported.
		return true;
	}
	else if(!InmStrCmp<NoCaseCharCmp>("ListVolumesBySystemGUID",hInfo.functionName.c_str()))
	{
		//Not supported.
		return true;
	}
	return false;
}

// TODO: open this once unix compilation is done

bool HandlerFactory::isHostDiscoveryHandler(HandlerInfo& hInfo)
{
    if (0 == InmStrCmp<NoCaseCharCmp>(REGISTER_HOST_STATIC_INFO,hInfo.functionName))
    {
        return true;
    }
    else if (0 == InmStrCmp<NoCaseCharCmp>(REGISTER_HOST_DYNAMIC_INFO,hInfo.functionName))
    {
        return true;
    }
    return false;
}

bool HandlerFactory::isSettingsHandler(HandlerInfo& hInfo)
{
    if (0 == InmStrCmp<NoCaseCharCmp>(GET_INITIAL_SETTINGS,hInfo.functionName))
    {
        return true;
    }
    else if (0 == InmStrCmp<NoCaseCharCmp>(GET_CURRENT_INITIAL_SETTINGS_V2,hInfo.functionName))
    {
        return true;
    }
    return false;
}

bool HandlerFactory::isResyncHandler(HandlerInfo& hInfo)
{
    if (InmStrCmp<NoCaseCharCmp>(SET_RESYNC_TRANSITION,hInfo.functionName)  == 0  ||
        InmStrCmp<NoCaseCharCmp>(GET_RESYNC_START_TIMESTAMP_FASTSYNC,hInfo.functionName) == 0 ||
        InmStrCmp<NoCaseCharCmp>(SEND_RESYNC_START_TIMESTAMP_FASTSYNC,hInfo.functionName) == 0 ||
        InmStrCmp<NoCaseCharCmp>(GET_RESYNC_END_TIMESTAMP_FASTSYNC,hInfo.functionName) == 0 ||
        InmStrCmp<NoCaseCharCmp>(SEND_RESYNC_END_TIMESTAMP_FASTSYNC,hInfo.functionName) == 0 ||
        InmStrCmp<NoCaseCharCmp>(SET_LAST_RESYNC_OFFSET_DIRECTSYNC,hInfo.functionName) == 0 ||
        InmStrCmp<NoCaseCharCmp>(GET_CLEAR_DIFFS,hInfo.functionName) == 0 ||
        InmStrCmp<NoCaseCharCmp>(SET_RESYNC_PROGRESS_FASTSYNC,hInfo.functionName) == 0 )
    {
        return true;
    }
    return false;
}

bool HandlerFactory::isSourceHandler(HandlerInfo& hInfo)
{
    if (InmStrCmp<NoCaseCharCmp>(UPDATE_VOLUMES_PENDING_CHANGES,hInfo.functionName) == 0 ||
        InmStrCmp<NoCaseCharCmp>(SET_SOURCE_RESYNC_REQUIRED,hInfo.functionName) == 0 ||
		InmStrCmp<NoCaseCharCmp>(BACKUPAGENT_PAUSE,hInfo.functionName) == 0 ||
		InmStrCmp<NoCaseCharCmp>(BACKUPAGENT_PAUSE_TRACK,hInfo.functionName) == 0 )
    {
        return true;
    }
    return false;
}

bool HandlerFactory::isTargetHandler(HandlerInfo& hInfo)
{
    if (InmStrCmp<NoCaseCharCmp>(NOTIFY_CX_DIFFS_DRAINED,hInfo.functionName)  == 0  ||
        InmStrCmp<NoCaseCharCmp>(CDP_STOP_REPLICATION,hInfo.functionName) == 0 ||
        InmStrCmp<NoCaseCharCmp>(SEND_END_QUASI_STATE_REQUEST,hInfo.functionName) == 0 ||
        InmStrCmp<NoCaseCharCmp>(SET_TARGET_RESYNC_REQUIRED,hInfo.functionName)  == 0  ||
        InmStrCmp<NoCaseCharCmp>(UPDATE_VOLUME_ATTRIBUTE,hInfo.functionName) == 0 ||
        InmStrCmp<NoCaseCharCmp>(UPDATE_CDP_INFORMATIONV2,hInfo.functionName)  == 0  ||
        InmStrCmp<NoCaseCharCmp>(UPDATE_CDP_DISKUSAGE,hInfo.functionName)  == 0  ||
        InmStrCmp<NoCaseCharCmp>(GET_CURRENT_VOLUME_ATTRIBUTE,hInfo.functionName) == 0 ||
        InmStrCmp<NoCaseCharCmp>(UPDATE_REPLICATION_CLEANUP_STATUS,hInfo.functionName)  == 0  ||
        InmStrCmp<NoCaseCharCmp>(UPDATE_RETENTION_INFO,hInfo.functionName)  == 0  ||
        InmStrCmp<NoCaseCharCmp>(SEND_ALERT_TO_CX,hInfo.functionName) == 0 ||
        InmStrCmp<NoCaseCharCmp>(SET_PAUSE_REPLICATION_STATUS,hInfo.functionName) == 0 || 
		InmStrCmp<NoCaseCharCmp>(UPDATE_FLUSH_AND_HOLD_WRITES_PENDING,hInfo.functionName) == 0 || 
		InmStrCmp<NoCaseCharCmp>(UPDATE_FLUSH_AND_HOLD_RESUME_PENDING,hInfo.functionName) == 0 || 
		InmStrCmp<NoCaseCharCmp>(GET_FLUSH_AND_HOLD_WRITES_REQUEST_SETTINGS,hInfo.functionName) == 0 ||
		InmStrCmp<NoCaseCharCmp>(PAUSE_REPLICATION_FROM_HOST,hInfo.functionName) == 0 ||
		InmStrCmp<NoCaseCharCmp>(RESUME_REPLICATION_FROM_HOST,hInfo.functionName) == 0 )
    {
        return true;
    }
    return false;

}
bool HandlerFactory::isAppSettingsHandler(HandlerInfo& hInfo)
{
    if (0 == InmStrCmp<NoCaseCharCmp>(GET_APP_SETTINGS,hInfo.functionName))
    {
        return true;
    }
    return false;
}

bool HandlerFactory::isPolicyHandler(HandlerInfo& hInfo)
{
    if (0 == InmStrCmp<NoCaseCharCmp>(POLICY_UPDATE,hInfo.functionName))
    {
        return true;
    }
    return false;
}

bool HandlerFactory::isPairCreationHandler(HandlerInfo& hInfo)
{

	if (0 == InmStrCmp<NoCaseCharCmp>(ENABLE_VOLUME_UNPROVISIONING,hInfo.functionName))
    {
        return true;
    }

	if (0 == InmStrCmp<NoCaseCharCmp>(MONITOR_EVENTS,hInfo.functionName))
	{
		return true;
	}
	if( 0 == InmStrCmp<NoCaseCharCmp>(PENDING_EVENTS,hInfo.functionName))
	{
		return true ;
	}
    return false;
}

bool HandlerFactory::isSnapshothandler(HandlerInfo& hInfo)
{
    if (0 == InmStrCmp<NoCaseCharCmp>(GET_SRR_JOBS,hInfo.functionName))
    {
        return true;
    }
    if (0 == InmStrCmp<NoCaseCharCmp>(UPDATE_SNAPSHOT_STATUS,hInfo.functionName))
    {
        return true;
    }
    
    if (0 == InmStrCmp<NoCaseCharCmp>(UPDATE_SNAPSHOT_PROGRESS,hInfo.functionName))
    {
        return true;
    }
	if (0 == InmStrCmp<NoCaseCharCmp>(UPDATE_SNAPSHOT_CREATION,hInfo.functionName))
    {
        return true;
    }
	if (0 == InmStrCmp<NoCaseCharCmp>(UPDATE_SNAPSHOT_DELETION,hInfo.functionName))
    {
        return true;
    }
    if (0 == InmStrCmp<NoCaseCharCmp>(MAKE_ACTIVE_SNAPSHOT_INSTANCE,hInfo.functionName))
    {
        return true;
    }
    
    if (0 == InmStrCmp<NoCaseCharCmp>(DELETE_VIRTUAL_SNAPSHOT,hInfo.functionName))
    {
        return true;
    }
    return false;
}

