#include "settingshandler.h"
#include "apinames.h"
#include "inmstrcmp.h"
#include "volumegroupsettings.h"
#include "initialsettings.h"
#include "portablehelpers.h"
#include "inmdefines.h"
#include "xmlizeinitialsettings.h"
#include "xmlizevolumegroupsettings.h"
#include "configurecxagent.h"
#include "confschema.h"
#include "confengine/confschemareaderwriter.h"
#include "confengine/agentconfig.h"
#include "protectionpairobject.h"
#include "inmapihandlerdefs.h"
#include "xmlizesourcesystemconfigsettings.h"
#include "xmlizeretentionsettings.h"
#include "xmlizetransport_settings.h"
#include <stdlib.h>
#include "util.h"
#include "InmsdkGlobals.h"
#include "inmsdkutil.h"
#include "StreamRecords.h"

#define BACKUPEXPRESS_COMPATIBILITY_NO 610000

SettingsHandler::SettingsHandler(void) :
m_pProtectionPairGroup(0), m_pVolumesGroup(0)
{
}

SettingsHandler::~SettingsHandler(void)
{
}

INM_ERROR SettingsHandler::process(FunctionInfo& f)
{
	Handler::process(f);
	INM_ERROR errCode = E_SUCCESS;

	if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName, GET_INITIAL_SETTINGS) == 0)
	{
		errCode = GetInitialSettings(f);
	}
	else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName, GET_CURRENT_INITIAL_SETTINGS_V2) == 0)
	{
		errCode = GetCurrentInitialSettingsV2(f);
	}
	else
	{
		//Through an exception that, the function is not supported.
	}

	return Handler::updateErrorCode(errCode);
}


INM_ERROR SettingsHandler::validate(FunctionInfo& f)
{
	INM_ERROR errCode = Handler::validate(f);
	if(E_SUCCESS != errCode)
		return errCode;

	//TODO: Write hadler specific validation here

	return errCode;
}


INM_ERROR SettingsHandler::GetInitialSettings(FunctionInfo &f)
{
	INM_ERROR errCode = E_UNKNOWN;
    int version = -1;
    
    /* TODO: ignore version */
    if (GetVersion(f, version))
    {
        DebugPrintf(SV_LOG_DEBUG, "The version parameter of get initial settings is %d\n", version);
        if (INITIAL_SETTINGS_VERSION  == version)
        {
            errCode = FillInitialSettings(f);
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "version %d supplied for get initial settings is wrong\n", version);
            errCode = E_WRONG_PARAM;
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "could not find version in get initial settings request parameters\n");
        errCode = E_INSUFFICIENT_PARAMS;
    }
	
	return errCode;
}


bool SettingsHandler::GetVersion(FunctionInfo &f, int &version)
{
    /* index is 2 since "1" is hostid */
    const char * pversionindex = "2";
    bool bfound = false;

    ConstParametersIter_t versioniter = f.m_RequestPgs.m_Params.find(pversionindex);
    if (f.m_RequestPgs.m_Params.end() != versioniter)
    {
        std::stringstream ss(versioniter->second.value);
        ss >> version;
        bfound = true;
    }

    return bfound;
}


INM_ERROR SettingsHandler::FillInitialSettings(FunctionInfo &f)
{
    INM_ERROR errCode = E_UNKNOWN;

        if (SetNeededGroups())
        {
            errCode = UpdateInitialSettings(f);
        }
        else
        {
            errCode = E_INTERNAL;
            DebugPrintf(SV_LOG_ERROR, "needed groups are not present in schema to fill initial settings\n");
        }

    return errCode;
}


bool SettingsHandler::SetNeededGroups()
{
    ConfSchemaReaderWriterPtr confschemaRdrWrtr = ConfSchemaReaderWriter::GetInstance() ;
    m_pProtectionPairGroup = confschemaRdrWrtr->getGroup( m_ProtectionPairObj.GetName() ) ;
    m_pVolumesGroup = confschemaRdrWrtr->getGroup( m_VolumesObj.GetName() ) ;
    bool bfound = false;
    if (m_pProtectionPairGroup)
    {
        DebugPrintf(SV_LOG_DEBUG, "protection pair information is present\n");
        if (m_pVolumesGroup)
        {
            /* loop through the available pairs and form one volume group settings
             * for one pair */
            DebugPrintf(SV_LOG_DEBUG, "volumes information is present\n");
            bfound = true;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "protection pair information is found, but did not find volumes information\n");
        }
    }
    else
    {
        /* case to fill in empty settings */
        DebugPrintf(SV_LOG_DEBUG, "protection pair information is not present\n");
        bfound = true;
    }

    return bfound;
}


INM_ERROR SettingsHandler::UpdateInitialSettings(FunctionInfo &f)
{
    ParameterGroup pg;

    std::pair<ParameterGroupsIter_t, bool> hvgpair = pg.m_ParamGroups.insert(std::make_pair(NS_InitialSettings::hostVolumeSettings, ParameterGroup()));
    ParameterGroupsIter_t &hvgpgiter = hvgpair.first;
    ParameterGroup &hvgpg = hvgpgiter->second;

    std::pair<ParameterGroupsIter_t, bool> cdpspair = pg.m_ParamGroups.insert(std::make_pair(NS_InitialSettings::cdpSettings, ParameterGroup()));
    ParameterGroupsIter_t &cdpspgiter = cdpspair.first;
    ParameterGroup &cdpspg = cdpspgiter->second;

    unsigned long timeoutInSecs = 900;
    insertintoreqresp(pg, cxArg ( timeoutInSecs ), NS_InitialSettings::timeoutInSecs);
    
    pg.m_ParamGroups.insert(std::make_pair(NS_InitialSettings::remoteCxs, ParameterGroup()));

    pg.m_ParamGroups.insert(std::make_pair(NS_InitialSettings::prismSettings, ParameterGroup()));

	std::pair<ParameterGroupsIter_t, bool> cstspair = pg.m_ParamGroups.insert(std::make_pair(NS_InitialSettings::transportSettings,
                                                                                             ParameterGroup()));
    ParameterGroupsIter_t &cstsiter = cstspair.first;
    ParameterGroup &csts = cstsiter->second;
	INM_ERROR errCode = FillCurrentTransportSettings(csts);
     
	if (E_SUCCESS == errCode)
	{
		insertintoreqresp(pg, cxArg ( TRANSPORT_PROTOCOL_UNKNOWN ), NS_InitialSettings::transportProtocol);
		insertintoreqresp(pg, cxArg ( VOLUME_SETTINGS::SECURE_MODE_NONE ), NS_InitialSettings::transportSecureMode);
		insertintoreqresp(pg, cxArg ( Options_t() ), NS_InitialSettings::options);

		errCode = FillHostVolumeGroupAndCDPSettings(hvgpg, cdpspg);
		if (E_SUCCESS != errCode)
		{
			DebugPrintf(SV_LOG_ERROR, "could not fill host volume group / cdp settings\n"); 
		}
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR, "could not fill cs transport settings\n"); 
	}

    if (E_SUCCESS == errCode)
    {
        /* fill the pg as the 1st pg of response */
        f.m_ResponsePgs.m_ParamGroups.insert(std::make_pair("1", pg));
    }

    return errCode;
}


INM_ERROR SettingsHandler::FillHostVolumeGroupAndCDPSettings(ParameterGroup &hvgpg,
                                                             ParameterGroup &cdpspg)
{
    INM_ERROR errCode = E_SUCCESS;

    /* Fill volume group settings */
    std::pair<ParameterGroupsIter_t, bool> vgspair = hvgpg.m_ParamGroups.insert(std::make_pair(NS_HOST_VOLUME_GROUP_SETTINGS::volumeGroups, 
                                                                                               ParameterGroup()));
    ParameterGroupsIter_t &vgsiter = vgspair.first;
    ParameterGroup &vgspg = vgsiter->second;

    if (m_pProtectionPairGroup)
    {
        ConfSchema::Group &pairgroup = *m_pProtectionPairGroup;
        /* if protection pair group is present with no objects, nothing is getting filled */
        ConfSchema::Object_t::size_type i = 0;
        for (ConfSchema::ObjectsIter_t oiter = pairgroup.m_Objects.begin(); oiter != pairgroup.m_Objects.end(); oiter++)
        {
			//ConfSchema::ProtectionPairObject proPairObj ;
			//ConfSchema::AttrsIter_t attrIter =  oiter->second.m_Attrs.find(proPairObj.GetExecutionStateAttrName()) ;
			//if( attrIter  != oiter->second.m_Attrs.end() )
			//{
			//	//We are not sending the pair which  is requested for "Resync+Queued"
			//	if(strcmp(attrIter->second.c_str(),"1") != 0)
			//	{
			//		ConfSchema::Object &o = oiter->second;
			//		errCode = UpdateVolumeGroupAndCDPSettings(i, o, vgspg, cdpspg);
			//		if (E_SUCCESS != errCode)
			//		{
			//			break;
			//		}
			//		i += 2 ;
			//	}
			//}	
			ConfSchema::ProtectionPairObject proPairObj ;
			std::string QueuedStatus,replicationState;
			ConfSchema::AttrsIter_t attrIter =  oiter->second.m_Attrs.find(proPairObj.GetExecutionStateAttrName()) ;
			if( attrIter  != oiter->second.m_Attrs.end() )
			{
				QueuedStatus = attrIter->second;
			}
			attrIter = oiter->second.m_Attrs.find(proPairObj.GetReplStateAttrName());
			if( attrIter  != oiter->second.m_Attrs.end() )
			{
				 replicationState = attrIter->second;
			}
			std::string markedForDeletion = "0"; 
			attrIter =  oiter->second.m_Attrs.find(proPairObj.GetMarkedForDeletionAttrName()) ;
			if (attrIter != oiter->second.m_Attrs.end () )
			{
				markedForDeletion = attrIter->second;
			}
			//We are not sending the pair which  is requested for "Resync+Queued"
			if( (!(strcmp(QueuedStatus.c_str(),boost::lexical_cast<std::string>(PAIR_IS_QUEUED).c_str()) == 0 && 
				 strcmp(replicationState.c_str(),boost::lexical_cast<std::string>(VOLUME_SETTINGS::SYNC_DIRECT).c_str()) == 0)) &&
				 markedForDeletion == "0" )
			{
				ConfSchema::Object &o = oiter->second;
				errCode = UpdateVolumeGroupAndCDPSettings(i, o, vgspg, cdpspg);
				if (E_SUCCESS != errCode)
				{
					break;
				}
				i += 2 ;
			}
        }
    }

    return errCode;
}


INM_ERROR SettingsHandler::UpdateVolumeGroupAndCDPSettings(const int &srcindex,
                                                           ConfSchema::Object &protectionpairobj,
                                                           ParameterGroup &vgspg, 
                                                           ParameterGroup &cdpspg)
{
    /* fill in the src vg */
    std::stringstream srcss;
    srcss << srcindex;
    std::string pgname = NS_HOST_VOLUME_GROUP_SETTINGS::volumeGroups;
    pgname += "[";
    pgname += srcss.str();
    pgname += "]";
    std::pair<ParameterGroupsIter_t, bool> srcvgpair = vgspg.m_ParamGroups.insert(std::make_pair(pgname, ParameterGroup()));
    ParameterGroupsIter_t &srcvgiter = srcvgpair.first;
    ParameterGroup &srcvg = srcvgiter->second;

    /* fill in the tgt vg */
    std::stringstream tgtss;
    tgtss << (srcindex + 1);
    pgname = NS_HOST_VOLUME_GROUP_SETTINGS::volumeGroups;
    pgname += "[";
    pgname += tgtss.str();
    pgname += "]";
    std::pair<ParameterGroupsIter_t, bool> tgtvgpair = vgspg.m_ParamGroups.insert(std::make_pair(pgname, ParameterGroup()));
    ParameterGroupsIter_t &tgtvgiter = tgtvgpair.first;
    ParameterGroup &tgtvg = tgtvgiter->second;
     
    
    
    /* putting in id */
    const std::string &id  = protectionpairobj.m_Attrs[m_ProtectionPairObj.GetIdAttrName()];
    ValueType vt("int", id);
    srcvg.m_Params.insert(std::make_pair(NS_VOLUME_GROUP_SETTINGS::id, vt));
    tgtvg.m_Params.insert(std::make_pair(NS_VOLUME_GROUP_SETTINGS::id, vt));
     
    /* put direction */
    insertintoreqresp(srcvg, cxArg ( SOURCE ), NS_VOLUME_GROUP_SETTINGS::direction);
    insertintoreqresp(tgtvg, cxArg ( TARGET ), NS_VOLUME_GROUP_SETTINGS::direction);

    /* put status */
    insertintoreqresp(srcvg, cxArg ( PROTECTED ), NS_VOLUME_GROUP_SETTINGS::status);
    /* TODO: is PROTECTED sent for target too ? Need to ask */
    insertintoreqresp(tgtvg, cxArg ( UNPROTECTED ), NS_VOLUME_GROUP_SETTINGS::status);

    /* put volume settings */
    std::pair<ParameterGroupsIter_t, bool> srcvspair = srcvg.m_ParamGroups.insert(std::make_pair(NS_VOLUME_GROUP_SETTINGS::volumes, ParameterGroup()));
    ParameterGroupsIter_t &srcvsiter = srcvspair.first;
    ParameterGroup &srcv = srcvsiter->second;
    std::pair<ParameterGroupsIter_t, bool> tgtvspair = tgtvg.m_ParamGroups.insert(std::make_pair(NS_VOLUME_GROUP_SETTINGS::volumes, ParameterGroup()));
    ParameterGroupsIter_t &tgtvsiter = tgtvspair.first;
    ParameterGroup &tgtv = tgtvsiter->second;

    /* fill volume settings */
    INM_ERROR errCode = UpdateVolumeAndCDPSettings(protectionpairobj, srcv, tgtv, cdpspg);
     
    if (E_SUCCESS == errCode)
    {
        /* fill transport connection settings */
        std::pair<ParameterGroupsIter_t, bool> srctspair = srcvg.m_ParamGroups.insert(std::make_pair(NS_VOLUME_GROUP_SETTINGS::transportSettings,
                                                                                                     ParameterGroup()));
        ParameterGroupsIter_t &srctsiter = srctspair.first;
        ParameterGroup &srcts = srctsiter->second;
        std::pair<ParameterGroupsIter_t, bool> tgttspair = tgtvg.m_ParamGroups.insert(std::make_pair(NS_VOLUME_GROUP_SETTINGS::transportSettings, 
                                                                                                     ParameterGroup()));
        ParameterGroupsIter_t &tgttsiter = tgttspair.first;
        ParameterGroup &tgtts = tgttsiter->second;

        errCode = FillTransportSettings(protectionpairobj, srcts, tgtts);
        if (E_SUCCESS != errCode)
        {
            DebugPrintf(SV_LOG_ERROR, "could not fill transport settings for pair id %s for source / target\n", id.c_str());
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "could not fill volume / cdp settings for pair id %s for source / target\n", id.c_str());
    }
    
    return errCode;
}


INM_ERROR SettingsHandler::FillCurrentTransportSettings(ParameterGroup &pg)
{
    INM_ERROR errCode = E_SUCCESS;

    pg.m_Params.insert(std::make_pair(NS_TRANSPORT_CONNECTION_SETTINGS::ipAddress, ValueType("string", std::string())));
    pg.m_Params.insert(std::make_pair(NS_TRANSPORT_CONNECTION_SETTINGS::port, ValueType("string", std::string())));
    pg.m_Params.insert(std::make_pair(NS_TRANSPORT_CONNECTION_SETTINGS::sslPort, ValueType("string", std::string())));
    pg.m_Params.insert(std::make_pair(NS_TRANSPORT_CONNECTION_SETTINGS::ftpPort, ValueType("string", std::string())));
    pg.m_Params.insert(std::make_pair(NS_TRANSPORT_CONNECTION_SETTINGS::user, ValueType("string", std::string())));
    pg.m_Params.insert(std::make_pair(NS_TRANSPORT_CONNECTION_SETTINGS::password, ValueType("string", std::string())));
    pg.m_Params.insert(std::make_pair(NS_TRANSPORT_CONNECTION_SETTINGS::connectTimeout, ValueType("int", "0")));
    pg.m_Params.insert(std::make_pair(NS_TRANSPORT_CONNECTION_SETTINGS::responseTimeout, ValueType("int", "0")));
    bool activeMode = false;
    insertintoreqresp(pg, cxArg ( activeMode ), NS_TRANSPORT_CONNECTION_SETTINGS::activeMode);

    return errCode;
}


INM_ERROR SettingsHandler::FillTransportSettings(ConfSchema::Object &protectionpairobj,
                                                 ParameterGroup &srcts,
                                                 ParameterGroup &tgtts)
{
    INM_ERROR errCode;

    errCode = FillCurrentTransportSettings(srcts);
    if (E_SUCCESS == errCode)
    {
        errCode = FillCurrentTransportSettings(tgtts);
        if (E_SUCCESS != errCode)
        {
            DebugPrintf(SV_LOG_ERROR, "could not fill source transport settings\n");
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "could not fill source transport settings\n");
    }

    return errCode;
}


INM_ERROR SettingsHandler::UpdateVolumeAndCDPSettings(ConfSchema::Object &protectionpairobj, 
                                                      ParameterGroup &srcv,
                                                      ParameterGroup &tgtv, 
                                                      ParameterGroup &cdpspg)
{
    /* get source and target devices */
    const std::string &srcdevice = protectionpairobj.m_Attrs[m_ProtectionPairObj.GetSrcNameAttrName()];
    const std::string &tgtdevice = protectionpairobj.m_Attrs[m_ProtectionPairObj.GetTgtNameAttrName()];

    /* fill in the volume settings map */
    std::pair<ParameterGroupsIter_t, bool> srcpair = srcv.m_ParamGroups.insert(std::make_pair(srcdevice, ParameterGroup()));
    std::pair<ParameterGroupsIter_t, bool> tgtpair = tgtv.m_ParamGroups.insert(std::make_pair(tgtdevice, ParameterGroup()));
    ParameterGroupsIter_t &srciter = srcpair.first;
    ParameterGroupsIter_t &tgtiter = tgtpair.first;
    ParameterGroup &srcvs = srciter->second;
    ParameterGroup &tgtvs = tgtiter->second;

    /* fill in the cdpsettings map */
    std::pair<ParameterGroupsIter_t, bool> cdpsettingspair = cdpspg.m_ParamGroups.insert(std::make_pair(tgtdevice, ParameterGroup()));
    ParameterGroupsIter_t cdpsettingspairiter = cdpsettingspair.first;
    ParameterGroup &cdpsetting = cdpsettingspairiter->second;

    INM_ERROR errCode = E_SUCCESS;
    /* find source and target in volumes object */
   ConfSchema::ObjectsIter_t srcvoliter = find_if(m_pVolumesGroup->m_Objects.begin(), m_pVolumesGroup->m_Objects.end(), 
                                                  ConfSchema::EqAttr(m_VolumesObj.GetNameAttrName(), srcdevice.c_str()));
    if (m_pVolumesGroup->m_Objects.end() == srcvoliter)
    {
        DebugPrintf(SV_LOG_ERROR, "could not find source device %s, in volumes relation\n", srcdevice.c_str());
        errCode = E_NO_DATA_FOUND;
    }

    ConfSchema::ObjectsIter_t tgtvoliter = find_if(m_pVolumesGroup->m_Objects.begin(), m_pVolumesGroup->m_Objects.end(), 
                                                  ConfSchema::EqAttr(m_VolumesObj.GetNameAttrName(), tgtdevice.c_str()));
    if (m_pVolumesGroup->m_Objects.end() == tgtvoliter)
    {
        DebugPrintf(SV_LOG_ERROR, "could not find target device %s, in volumes relation\n", tgtdevice.c_str());
        errCode = E_NO_DATA_FOUND;
    }
     
    if (E_SUCCESS == errCode)
    {
        errCode = FillVolumeSettings(protectionpairobj, srcvoliter->second, tgtvoliter->second, srcvs, tgtvs); 
        if (E_SUCCESS == errCode)
        {
            errCode = FillCdpSettings(protectionpairobj, tgtvoliter->second, cdpsetting);
            if (E_SUCCESS != errCode)
            {
                DebugPrintf(SV_LOG_ERROR, "could not fill cdp settings for target %s\n", tgtdevice.c_str());
            }
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "could not fill volume settings for source %s, target %s\n", 
                                      srcdevice.c_str(), tgtdevice.c_str());
        }
    }

    return errCode;
}


INM_ERROR SettingsHandler::FillVolumeSettings(ConfSchema::Object &protectionpairobj,
                                              ConfSchema::Object &srcvolobj,
                                              ConfSchema::Object &tgtvolobj,
                                              ParameterGroup &srcvs,
                                              ParameterGroup &tgtvs)
{
    INM_ERROR errCode = E_SUCCESS;

    /* start filling in volume settings */

    /* get source and target devices */
    const std::string &srcdevice = protectionpairobj.m_Attrs[m_ProtectionPairObj.GetSrcNameAttrName()];
    const std::string &tgtdevice = protectionpairobj.m_Attrs[m_ProtectionPairObj.GetTgtNameAttrName()];

    /* fill the device name */
    srcvs.m_Params.insert(std::make_pair(NS_VOLUME_SETTINGS::deviceName, ValueType("string", srcdevice)));
    tgtvs.m_Params.insert(std::make_pair(NS_VOLUME_SETTINGS::deviceName, ValueType("string", tgtdevice)));

    /* fill mount point */
    const std::string &srcmountpoint = srcvolobj.m_Attrs[m_VolumesObj.GetMountPointAttrName()];
    srcvs.m_Params.insert(std::make_pair(NS_VOLUME_SETTINGS::mountPoint, ValueType("string", srcmountpoint)));
    const std::string &tgtmountpoint = tgtvolobj.m_Attrs[m_VolumesObj.GetMountPointAttrName()];
    tgtvs.m_Params.insert(std::make_pair(NS_VOLUME_SETTINGS::mountPoint, ValueType("string", tgtmountpoint)));
         
    /* fill file system */
    const std::string &filesystem = srcvolobj.m_Attrs[m_VolumesObj.GetFileSystemAttrName()];
    /* TODO: for target, copy the source file system ? */
    srcvs.m_Params.insert(std::make_pair(NS_VOLUME_SETTINGS::fstype, ValueType("string", filesystem)));
    tgtvs.m_Params.insert(std::make_pair(NS_VOLUME_SETTINGS::fstype, ValueType("string", filesystem)));
        
    /* fill host name */
    /* TODO: need to store hostname ? */
    srcvs.m_Params.insert(std::make_pair(NS_VOLUME_SETTINGS::hostname, ValueType("string", std::string())));
    tgtvs.m_Params.insert(std::make_pair(NS_VOLUME_SETTINGS::hostname, ValueType("string", std::string())));
        
    /* fill host name */
    /* TODO: need to store hostid ? */
	srcvs.m_Params.insert(std::make_pair(NS_VOLUME_SETTINGS::hostId, ValueType("string", AgentConfig::GetInstance()->getHostId())));
    tgtvs.m_Params.insert(std::make_pair(NS_VOLUME_SETTINGS::hostId, ValueType("string", AgentConfig::GetInstance()->getHostId())));

    /* fill secure mode */
    /* TODO: no secure mode */
    insertintoreqresp(srcvs, cxArg ( VOLUME_SETTINGS::SECURE_MODE_NONE ), NS_VOLUME_SETTINGS::secureMode);
    insertintoreqresp(tgtvs, cxArg ( VOLUME_SETTINGS::SECURE_MODE_NONE ), NS_VOLUME_SETTINGS::secureMode);

    /* fill secure mode from source to CX */
    /* TODO: no secure mode from source to CX */
    insertintoreqresp(srcvs, cxArg ( VOLUME_SETTINGS::SECURE_MODE_NONE ), NS_VOLUME_SETTINGS::sourceToCXSecureMode);
    insertintoreqresp(tgtvs, cxArg ( VOLUME_SETTINGS::SECURE_MODE_NONE ), NS_VOLUME_SETTINGS::sourceToCXSecureMode);

    /* fill sync mode */
    const std::string &syncMode = protectionpairobj.m_Attrs[m_ProtectionPairObj.GetReplStateAttrName()];
    /* TODO: is replication state directly representing the enum ? */
   // std::string syncMode = boost::lexical_cast<std::string>(VOLUME_SETTINGS::SYNC_DIRECT);
    srcvs.m_Params.insert(std::make_pair(NS_VOLUME_SETTINGS::syncMode, ValueType("int", syncMode)));
    tgtvs.m_Params.insert(std::make_pair(NS_VOLUME_SETTINGS::syncMode, ValueType("int", syncMode)));

    /* fill transprotocol */
    /* TODO: no transprotocol (or) is it file ? memory (3) */
    insertintoreqresp(srcvs, cxArg ( TRANSPORT_PROTOOL_MEMORY ), NS_VOLUME_SETTINGS::transportProtocol);
    insertintoreqresp(tgtvs, cxArg ( TRANSPORT_PROTOOL_MEMORY ), NS_VOLUME_SETTINGS::transportProtocol);

    /* fill visibility */
    /* TODO: no attribute found that stores visibility and certain things like is it only for 
     * target have to be understood */
    insertintoreqresp(srcvs, cxArg ( 1 ), NS_VOLUME_SETTINGS::visibility);
    insertintoreqresp(tgtvs, cxArg ( 0 ), NS_VOLUME_SETTINGS::visibility);

    /* fill source capacity */
    const std::string &srccapacity = srcvolobj.m_Attrs[m_VolumesObj.GetCapacityAttrName()];
    /* for target, copy the source capacity ? */
    srcvs.m_Params.insert(std::make_pair(NS_VOLUME_SETTINGS::sourceCapacity, ValueType("unsigned long long", srccapacity)));
    tgtvs.m_Params.insert(std::make_pair(NS_VOLUME_SETTINGS::sourceCapacity, ValueType("unsigned long long", srccapacity)));

    /* fill resync directory */
    /* TODO: no resync directory ? do not need */
    srcvs.m_Params.insert(std::make_pair(NS_VOLUME_SETTINGS::resyncDirectory, ValueType("string", std::string())));
    tgtvs.m_Params.insert(std::make_pair(NS_VOLUME_SETTINGS::resyncDirectory, ValueType("string", std::string())));

    /* fill rpoThreshold */
    /* TODO: no rpo threshold found in schema; This is used only by s2 to find idle wait time
     * need to check in CX what is it and its generation, significance */
    insertintoreqresp(srcvs, cxArg ( 65 ), NS_VOLUME_SETTINGS::rpoThreshold);
    insertintoreqresp(tgtvs, cxArg ( 65 ), NS_VOLUME_SETTINGS::rpoThreshold);
         
    /* fill resync start time */
    const std::string &resyncstarttime = protectionpairobj.m_Attrs[m_ProtectionPairObj.GetResyncStartTagTimeAttrName()];
    srcvs.m_Params.insert(std::make_pair(NS_VOLUME_SETTINGS::srcResyncStarttime, ValueType("unsigned long long", resyncstarttime)));
    tgtvs.m_Params.insert(std::make_pair(NS_VOLUME_SETTINGS::srcResyncStarttime, ValueType("unsigned long long", resyncstarttime)));

    /* fill resync end time */
    const std::string &resyncendtime = protectionpairobj.m_Attrs[m_ProtectionPairObj.GetResyncEndTagTimeAttrName()];
    srcvs.m_Params.insert(std::make_pair(NS_VOLUME_SETTINGS::srcResyncEndtime, ValueType("unsigned long long", resyncendtime)));
    tgtvs.m_Params.insert(std::make_pair(NS_VOLUME_SETTINGS::srcResyncEndtime, ValueType("unsigned long long", resyncendtime)));

    /* fill the compatibility number */
    /* TODO: need to store compatibilty number */
    SV_ULONG OtherSiteCompatibilityNum = BACKUPEXPRESS_COMPATIBILITY_NO;

    insertintoreqresp(srcvs, cxArg ( OtherSiteCompatibilityNum ), NS_VOLUME_SETTINGS::OtherSiteCompatibilityNum);
    insertintoreqresp(tgtvs, cxArg ( OtherSiteCompatibilityNum ), NS_VOLUME_SETTINGS::OtherSiteCompatibilityNum);

    /* fill the resync required flag */
    /*const std::string &resyncsetts = protectionpairobj.m_Attrs[m_ProtectionPairObj.GetResyncSetTimeStampAttrName()];
    unsigned long long ullresyncsetts = 0;
    unsigned long long ullresyncstartts = 0;
    std::stringstream setss(resyncsetts);
    std::stringstream startss(resyncstarttime);
    TODO: check for convertibility using boost lexical cast
    setss >> ullresyncsetts;
    startss >> ullresyncstartts;*/
	unsigned long long ullresyncsetts = 0;
    //bool bresyncreqd =  (ullresyncsetts >= ullresyncstartts);
	bool bresyncreqd = false ;
	std::string resyncSetFrom = protectionpairobj.m_Attrs[m_ProtectionPairObj.GetShouldResyncSetFromAttrName()] ;
	std::string resyncSetCxTs = protectionpairobj.m_Attrs[m_ProtectionPairObj.GetResyncSetCXtimeStampAttrName()] ;
	std::string resyncSetTs = protectionpairobj.m_Attrs[m_ProtectionPairObj.GetResyncSetTimeStampAttrName()] ;
	if( !resyncSetTs.empty() )
	{
		ullresyncsetts = boost::lexical_cast<unsigned long long>(resyncSetTs) ;
	}
	if( !resyncSetCxTs.empty() )
	{
		ullresyncsetts = boost::lexical_cast<unsigned long long>(resyncSetCxTs) ;
	}
	if( !resyncSetFrom.empty() )
	{
		bresyncreqd = true ;
	}
	else
	{
		bresyncreqd = false ;
	}
	
    insertintoreqresp(srcvs, cxArg ( bresyncreqd ), NS_VOLUME_SETTINGS::resyncRequiredFlag);
    insertintoreqresp(tgtvs, cxArg ( bresyncreqd ), NS_VOLUME_SETTINGS::resyncRequiredFlag);
	insertintoreqresp(srcvs, cxArg ( VOLUME_SETTINGS::RESYNCREQUIRED_BYSOURCE ), NS_VOLUME_SETTINGS::resyncRequiredCause);
	insertintoreqresp(tgtvs, cxArg ( VOLUME_SETTINGS::RESYNCREQUIRED_BYSOURCE ), NS_VOLUME_SETTINGS::resyncRequiredCause);
	if( !resyncSetFrom.empty() && boost::lexical_cast<int>(resyncSetFrom) == 2 )
	{
		insertintoreqresp(srcvs, cxArg ( VOLUME_SETTINGS::RESYNCREQUIRED_BYSOURCE ), NS_VOLUME_SETTINGS::resyncRequiredCause);
		insertintoreqresp(tgtvs, cxArg ( VOLUME_SETTINGS::RESYNCREQUIRED_BYSOURCE ), NS_VOLUME_SETTINGS::resyncRequiredCause);
	}
	else
	{
		insertintoreqresp(srcvs, cxArg ( VOLUME_SETTINGS::RESYNCREQUIRED_BYTARGET ), NS_VOLUME_SETTINGS::resyncRequiredCause);
		insertintoreqresp(tgtvs, cxArg ( VOLUME_SETTINGS::RESYNCREQUIRED_BYTARGET ), NS_VOLUME_SETTINGS::resyncRequiredCause);
	}
    /* TODO: fill resyncRequiredCause after processing; currently
     *       setting as source as there is no enumeration for none; 
     *       does this value needs to be stored in schema ? */
    insertintoreqresp(srcvs, cxArg ( VOLUME_SETTINGS::RESYNCREQUIRED_BYSOURCE ), NS_VOLUME_SETTINGS::resyncRequiredCause);
    insertintoreqresp(tgtvs, cxArg ( VOLUME_SETTINGS::RESYNCREQUIRED_BYSOURCE ), NS_VOLUME_SETTINGS::resyncRequiredCause);

    /* fill resync required time stamp */
    insertintoreqresp(srcvs, cxArg ( ullresyncsetts ), NS_VOLUME_SETTINGS::resyncRequiredTimestamp);
    insertintoreqresp(tgtvs, cxArg ( ullresyncsetts ), NS_VOLUME_SETTINGS::resyncRequiredTimestamp);

    /* fill source os val */
    /* TODO: store the source os val; target should receive source's os */
    OS_VAL srcosval = SV_WIN_OS;
    insertintoreqresp(srcvs, cxArg ( srcosval ), NS_VOLUME_SETTINGS::sourceOSVal);
    insertintoreqresp(tgtvs, cxArg ( srcosval ), NS_VOLUME_SETTINGS::sourceOSVal);

    /* fill compressionEnable */
    /* TODO: compression not presnet ? */
    insertintoreqresp(srcvs, cxArg ( COMPRESS_NONE ), NS_VOLUME_SETTINGS::compressionEnable);
    insertintoreqresp(tgtvs, cxArg ( COMPRESS_NONE ), NS_VOLUME_SETTINGS::compressionEnable);
 
    /* fill jobid */
    /* TODO: no job id >? */
    const std::string &jobId = protectionpairobj.m_Attrs[m_ProtectionPairObj.GetJobIdAttrName()] ;

    srcvs.m_Params.insert(std::make_pair(NS_VOLUME_SETTINGS::jobId, ValueType("string", jobId)));
    tgtvs.m_Params.insert(std::make_pair(NS_VOLUME_SETTINGS::jobId, ValueType("string", jobId)));

    /* fill sanvolume info */
    /* TODO: separate call for target and source ? */
    ParameterGroup sanvolpg;
    FillSanVolumeInfo(sanvolpg);
    srcvs.m_ParamGroups.insert(std::make_pair(NS_VOLUME_SETTINGS::sanVolumeInfo, sanvolpg));
    tgtvs.m_ParamGroups.insert(std::make_pair(NS_VOLUME_SETTINGS::sanVolumeInfo, sanvolpg));

    /* fill pair state */
    /* TODO: does pair state reflect the agent side value in schema ? */
    /* TODO: create only one value type and use for both source and target */
    int srcPairState, tgtPairState ;
    GetPairState(protectionpairobj,srcPairState, tgtPairState) ;
    srcvs.m_Params.insert(std::make_pair(NS_VOLUME_SETTINGS::pairState, 
        ValueType("int", boost::lexical_cast<std::string>(srcPairState))));
    tgtvs.m_Params.insert(std::make_pair(NS_VOLUME_SETTINGS::pairState, 
        ValueType("int", boost::lexical_cast<std::string>(tgtPairState))));

    /* fill cleanup action */
    /* TODO: clean up action needs to be generated and all */
    std::string cleanUpActionStr ;
    if(srcPairState == VOLUME_SETTINGS::DELETE_PENDING || 
       tgtPairState == VOLUME_SETTINGS::DELETE_PENDING )
    {
        cleanUpActionStr = GetCleanUpActionString(protectionpairobj);
    }
    srcvs.m_Params.insert(std::make_pair(NS_VOLUME_SETTINGS::cleanup_action, ValueType("string", std::string())));
    tgtvs.m_Params.insert(std::make_pair(NS_VOLUME_SETTINGS::cleanup_action, ValueType("string", cleanUpActionStr)));

    /* fill diffs pending in CX */
    /* TODO: no use of this */
    SV_ULONGLONG diffsPendingInCX = 0;
    insertintoreqresp(srcvs, cxArg ( diffsPendingInCX ), NS_VOLUME_SETTINGS::diffsPendingInCX);
    insertintoreqresp(tgtvs, cxArg ( diffsPendingInCX ), NS_VOLUME_SETTINGS::diffsPendingInCX);

    /* fill current rpo */
    /* TODO: is the the value of current RPO */
    const std::string &currentrpo = protectionpairobj.m_Attrs[m_ProtectionPairObj.GetCurrentRPOTimeAttrName()];
    srcvs.m_Params.insert(std::make_pair(NS_VOLUME_SETTINGS::currentRPO, ValueType("unsigned long long", currentrpo)));
    tgtvs.m_Params.insert(std::make_pair(NS_VOLUME_SETTINGS::currentRPO, ValueType("unsigned long long", currentrpo)));

    /* fill apply rate */
    /* TODO: apply rate not being stored */
    SV_ULONGLONG applyRate = 0;
    insertintoreqresp(srcvs, cxArg ( applyRate ), NS_VOLUME_SETTINGS::applyRate);
    insertintoreqresp(tgtvs, cxArg ( applyRate ), NS_VOLUME_SETTINGS::applyRate);

    /* fill maintenance action */
    /* TODO: maintenance action needs to be generated and all */
    const std::string & pausePendingPendingState = boost::lexical_cast<std::string>(VOLUME_SETTINGS::PAUSE_PENDING);
    const std::string & pauseTrackPendingState = boost::lexical_cast<std::string>(VOLUME_SETTINGS::PAUSE_TACK_PENDING) ;
    std::string srcMaintenanceActionString, tgtMaintenanceActionString;
    srcMaintenanceActionString = GetMaintainenceActionString(protectionpairobj, true) ;
    tgtMaintenanceActionString = GetMaintainenceActionString(protectionpairobj, false) ;
    srcvs.m_Params.insert(std::make_pair(NS_VOLUME_SETTINGS::maintenance_action, ValueType("string", srcMaintenanceActionString)));
    tgtvs.m_Params.insert(std::make_pair(NS_VOLUME_SETTINGS::maintenance_action, ValueType("string", tgtMaintenanceActionString)));

    /* fill resync start sequence number */
    const std::string &resyncstartseq = protectionpairobj.m_Attrs[m_ProtectionPairObj.GetResyncStartSeqAttrName()];
    srcvs.m_Params.insert(std::make_pair(NS_VOLUME_SETTINGS::srcResyncStartSeq, ValueType("unsigned long long", resyncstartseq)));
    tgtvs.m_Params.insert(std::make_pair(NS_VOLUME_SETTINGS::srcResyncStartSeq, ValueType("unsigned long long", resyncstartseq)));

    /* fill resync end sequence number */
    const std::string &resyncendseq = protectionpairobj.m_Attrs[m_ProtectionPairObj.GetResyncEndSeqAttrName()];
    srcvs.m_Params.insert(std::make_pair(NS_VOLUME_SETTINGS::srcResyncEndSeq, ValueType("unsigned long long", resyncendseq)));
    tgtvs.m_Params.insert(std::make_pair(NS_VOLUME_SETTINGS::srcResyncEndSeq, ValueType("unsigned long long", resyncendseq)));

    /* fill throttle settings */
    /* TODO: separate call for target and source ? */
    ParameterGroup throttlepg;
    FillThrottleSettings(throttlepg);
    srcvs.m_ParamGroups.insert(std::make_pair(NS_VOLUME_SETTINGS::throttleSettings, throttlepg));
    tgtvs.m_ParamGroups.insert(std::make_pair(NS_VOLUME_SETTINGS::throttleSettings, throttlepg));
         
    /* fill resync counter */
    const std::string &resyncctr = protectionpairobj.m_Attrs[m_ProtectionPairObj.GetRestartResyncCounterAttrName()];
    srcvs.m_Params.insert(std::make_pair(NS_VOLUME_SETTINGS::resyncCounter, ValueType("unsigned long long", resyncctr)));
    tgtvs.m_Params.insert(std::make_pair(NS_VOLUME_SETTINGS::resyncCounter, ValueType("unsigned long long", resyncctr)));

    /* fill source raw size */
    const std::string &srcrawcapacity = srcvolobj.m_Attrs[m_VolumesObj.GetRawSizeAttrName()];
    /* for target, copy the source raw capacity ? */
    srcvs.m_Params.insert(std::make_pair(NS_VOLUME_SETTINGS::sourceRawSize, ValueType("unsigned long long", srcrawcapacity)));
    tgtvs.m_Params.insert(std::make_pair(NS_VOLUME_SETTINGS::sourceRawSize, ValueType("unsigned long long", srcrawcapacity)));

    /* fill at lun stats request */
    /* TODO: separate call for target and source ? */
    ParameterGroup atlunstatsreqpg;
    FillATLunStatsReq(atlunstatsreqpg);
    srcvs.m_ParamGroups.insert(std::make_pair(NS_VOLUME_SETTINGS::atLunStatsRequest, atlunstatsreqpg));
    tgtvs.m_ParamGroups.insert(std::make_pair(NS_VOLUME_SETTINGS::atLunStatsRequest, atlunstatsreqpg));

    /* fill source start offset */
    /* TODO: no use of this */
    SV_ULONGLONG srcStartOffset = 0;
    insertintoreqresp(srcvs, cxArg ( srcStartOffset ), NS_VOLUME_SETTINGS::srcStartOffset);
    insertintoreqresp(tgtvs, cxArg ( srcStartOffset ), NS_VOLUME_SETTINGS::srcStartOffset);

    /* fill device type */
    /* TODO: type is not being stored in volumes schema ?
     *       Also should only source side type be sent to
     *       target, ? */
    insertintoreqresp(srcvs, cxArg ( VolumeSummary::LOGICAL_VOLUME ), NS_VOLUME_SETTINGS::devicetype);
    insertintoreqresp(tgtvs, cxArg ( VolumeSummary::LOGICAL_VOLUME ), NS_VOLUME_SETTINGS::devicetype);

	/* build source and target options */
    std::map<std::string, std::string> srcoptions, tgtoptions;
	/* insert resync copy mode: for now filesystem */
	srcoptions.insert(std::make_pair(NsVOLUME_SETTINGS::NAME_RESYNC_COPY_MODE, NsVOLUME_SETTINGS::VALUE_FILESYSTEM));
	tgtoptions.insert(std::make_pair(NsVOLUME_SETTINGS::NAME_RESYNC_COPY_MODE, NsVOLUME_SETTINGS::VALUE_FILESYSTEM));
	/* insert protection direction: for now forward */
	srcoptions.insert(std::make_pair(NsVOLUME_SETTINGS::NAME_PROTECTION_DIRECTION, NsVOLUME_SETTINGS::VALUE_FORWARD));
	tgtoptions.insert(std::make_pair(NsVOLUME_SETTINGS::NAME_PROTECTION_DIRECTION, NsVOLUME_SETTINGS::VALUE_FORWARD));
	/* insert raw volume size */
	const std::string &tgtrawcapacity = tgtvolobj.m_Attrs[m_VolumesObj.GetRawSizeAttrName()];
	srcoptions.insert(std::make_pair(NsVOLUME_SETTINGS::NAME_RAWSIZE, srcrawcapacity));
	tgtoptions.insert(std::make_pair(NsVOLUME_SETTINGS::NAME_RAWSIZE, tgtrawcapacity));
	/* insert options */
    insertintoreqresp(srcvs, cxArg ( srcoptions ), NS_VOLUME_SETTINGS::options);
    insertintoreqresp(tgtvs, cxArg ( tgtoptions ), NS_VOLUME_SETTINGS::options);

    std::pair<ParameterGroupsIter_t, bool> srcendptpair = srcvs.m_ParamGroups.insert(std::make_pair(NS_VOLUME_SETTINGS::endpoints, 
                                                                                                    ParameterGroup()));
    ParameterGroupsIter_t &srcendptiter = srcendptpair.first;
    ParameterGroup &srcendptpg = srcendptiter->second;

    std::pair<ParameterGroupsIter_t, bool> tgtendptpair = tgtvs.m_ParamGroups.insert(std::make_pair(NS_VOLUME_SETTINGS::endpoints, 
                                                                                                    ParameterGroup()));
    ParameterGroupsIter_t &tgtendptiter = tgtendptpair.first;
    ParameterGroup &tgtendptpg = tgtendptiter->second;
    std::string eachendptname = NS_VOLUME_SETTINGS::endpoints;
    eachendptname += "[0]";
    srcendptpg.m_ParamGroups.insert(std::make_pair(eachendptname, tgtvs));
    tgtendptpg.m_ParamGroups.insert(std::make_pair(eachendptname, srcvs));

    return errCode;
}

void SettingsHandler::GetPairState(ConfSchema::Object &protectionpairobj, int & srcPairState, int& tgtPairState)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    const std::string &pairstateStr = protectionpairobj.m_Attrs[m_ProtectionPairObj.GetPairStateAttrName()];
    int pairState = boost::lexical_cast<int>(pairstateStr) ;
    const std::string &maintenanceActionBitMap = protectionpairobj.m_Attrs[m_ProtectionPairObj.GetMaintenanceActionAttrName()] ;
    if( maintenanceActionBitMap.empty() )
    {
        srcPairState = tgtPairState = pairState ;
    }
    else
    {
        bool pendingMaintenanceForSrc, pendingMaintenanceForTgt ;
        pendingMaintenanceForSrc = pendingMaintenanceForTgt = false ;
        if( maintenanceActionBitMap[0] == '1' )
        {
            pendingMaintenanceForSrc = true ;
        }
        if( maintenanceActionBitMap.length() > 1 && maintenanceActionBitMap[1] == '1' )
        {
            pendingMaintenanceForTgt = true ;
        }
        switch( pairState )
        {
            case VOLUME_SETTINGS::PAUSE_TACK_PENDING:
                if( pendingMaintenanceForSrc )
                {
                    srcPairState = VOLUME_SETTINGS::PAUSE_TACK_PENDING ;
                }
                else
                {
                    srcPairState = VOLUME_SETTINGS::PAUSE_TACK_COMPLETED ;
                }
                if( pendingMaintenanceForTgt )
                {
                    tgtPairState = VOLUME_SETTINGS::PAUSE_PENDING ;
                }
                else
                {
                    tgtPairState = VOLUME_SETTINGS::PAUSE ;
                }
                break ;
            case VOLUME_SETTINGS::PAUSE_TACK_COMPLETED:
                srcPairState = VOLUME_SETTINGS::PAUSE_TACK_COMPLETED ;
                tgtPairState = VOLUME_SETTINGS::PAUSE ;
                break ;
            case VOLUME_SETTINGS::PAUSE_PENDING:
                //srcPairState = VOLUME_SETTINGS::PAIR_PROGRESS ;
                if( pendingMaintenanceForTgt )
                {
                    tgtPairState = VOLUME_SETTINGS::PAUSE_PENDING ;
                }
                else
                {
                    tgtPairState = VOLUME_SETTINGS::PAUSE ;
                }
                srcPairState = VOLUME_SETTINGS::PAUSE ;
                break ;
            case VOLUME_SETTINGS::PAUSE:
                srcPairState = VOLUME_SETTINGS::PAUSE ;
                tgtPairState = VOLUME_SETTINGS::PAUSE ;
                break ;
            default:
                srcPairState = pairState ;
                tgtPairState = pairState ;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

INM_ERROR SettingsHandler::FillCdpSettings(ConfSchema::Object &protectionpairobj,
                                           ConfSchema::Object &tgtvolobj,
                                           ParameterGroup &cdpsetting)
{
    INM_ERROR errCode = E_UNKNOWN;

    const std::string &tgtname = tgtvolobj.m_Attrs[m_VolumesObj.GetNameAttrName()];
    ConfSchema::GroupsIter_t retlogpolicygrpiter = protectionpairobj.m_Groups.find(m_RetLogPolicyObj.GetName());
    if (retlogpolicygrpiter != protectionpairobj.m_Groups.end())
    {
        DebugPrintf(SV_LOG_DEBUG, "retention log policy information present for target %s\n", tgtname.c_str());
        ConfSchema::Group &retlogpolicygrp = retlogpolicygrpiter->second;

        ConfSchema::Object_t::size_type nlogpolicy = retlogpolicygrp.m_Objects.size();
        /* TODO: does one retlogpolicygroup have one object ? 
         *       what has to be done in all other cases */
        if (nlogpolicy == 1)
        {
            ConfSchema::Object &retlogpolicyobj = (retlogpolicygrp.m_Objects.begin())->second;

            /* From CX, cdp_status sent to agent is negation of retstate */
            /* TODO: should retState map directly to cdp_status */
            const std::string &retState = retlogpolicyobj.m_Attrs[m_RetLogPolicyObj.GetRetStateAttrName()];
            /* TODO: use boost lexical cast ? */
            unsigned int iretState = 0;
            std::stringstream ssretstate(retState);
            ssretstate >> iretState;
            int cdp_status = !iretState;
            if(cdp_status == 0 )
            {
                DebugPrintf(SV_LOG_ERROR, "CDP Status is sent as 0\n") ;
            }
            insertintoreqresp(cdpsetting, cxArg ( cdp_status ), NS_CDP_SETTINGS::cdp_status);

            /* From CX, cdpid is unique id */
            /* TODO: should this be auto increment key number ? */
            const std::string &cdpid = retlogpolicyobj.m_Attrs[m_RetLogPolicyObj.GetUniqueIdAttrName()];
            cdpsetting.m_Params.insert(std::make_pair(NS_CDP_SETTINGS::cdp_id, ValueType("string", cdpid)));

            /* TODO: is this hardcoded to 3; if yes, get this from header */
            const std::string &sissparseenabled = retlogpolicyobj.m_Attrs[m_RetLogPolicyObj.GetIsSparceEnabledAttrName()];
            std::stringstream sssp(sissparseenabled);
            int issparse = 0;
            sssp >> issparse;
            const unsigned int version = issparse ? 3 : 1;
			if( version != 3 )
			{
				DebugPrintf(SV_LOG_ERROR, "Version is sent as %d\n",version ) ;
			}
            insertintoreqresp(cdpsetting, cxArg ( version ), NS_CDP_SETTINGS::cdp_version);

            /* TODO: values coming from retentionEvent have to be filled in 
             *       which are cdp_catalogue, cdp_alert_threshold, 
             *       cdp_onspace_shortage */
           
            /* This value is 0 for time based policy */
            unsigned long long cdp_diskspace = 0;
            insertintoreqresp(cdpsetting, cxArg ( cdp_diskspace ), NS_CDP_SETTINGS::cdp_diskspace);

            /* TODO: cdp_minreservedspace is coming from transbandSettings relation;
             * This may be a configurable value; either schema stores this relation
             * itself (or) the reserved space attribute is added to retLopPolicy */
            /* currently sending as per CX code default value of 1MB 
             * this might be 256 MB default */
            unsigned long long cdp_minreservedspace = 1048576;
            insertintoreqresp(cdpsetting, cxArg ( cdp_minreservedspace ), NS_CDP_SETTINGS::cdp_minreservedspace);
  
            std::pair<ParameterGroupsIter_t, bool> cdppoliciespair = cdpsetting.m_ParamGroups.insert(std::make_pair(NS_CDP_SETTINGS::cdp_policies, 
                                                                                                                    ParameterGroup()));
            ParameterGroupsIter_t cdppoliciesiter = cdppoliciespair.first;
            ParameterGroup &cdppolicypg = cdppoliciesiter->second;
            errCode = FillCDPPoliciesAndConfInfo(tgtname, retlogpolicyobj, cdpsetting, cdppolicypg);
        }
        else
        {
            std::stringstream msg;
            msg << nlogpolicy;
            DebugPrintf(SV_LOG_ERROR, "%s retention log policy information present for target %s\n", msg.str().c_str(), tgtname.c_str());
            errCode = E_INTERNAL;
        }
    }
    else
    {
        /* This is an error */
        DebugPrintf(SV_LOG_ERROR, "retention log policy information is not present for target %s\n", tgtname.c_str());
        errCode = E_NO_DATA_FOUND;
    }
 
    return errCode;
}


INM_ERROR SettingsHandler::FillCDPPoliciesAndConfInfo(const std::string &tgtname,
                                                      ConfSchema::Object &retlogpolicyobj,
                                                      ParameterGroup &cdpsetting,
                                                      ParameterGroup &cdppolicypg)
{
    INM_ERROR errCode = E_UNKNOWN;
     
    ConfSchema::GroupsIter_t retwindowiter = retlogpolicyobj.m_Groups.find(m_RetWindowObj.GetName());
    if (retwindowiter != retlogpolicyobj.m_Groups.end())
    {
        ConfSchema::Group &retwindowgrp = retwindowiter->second;
        errCode = FillCDPWindowAndConfInfo(tgtname, retwindowgrp, cdpsetting, cdppolicypg);
    }
    else
    {
        errCode = E_NO_DATA_FOUND;
        /* as per CX team, there is atleast one window */
        DebugPrintf(SV_LOG_ERROR, "For target volume %s, did not find retention window information\n", tgtname.c_str());
    }

    return errCode;
}


INM_ERROR SettingsHandler::FillCDPWindowAndConfInfo(const std::string &tgtname, 
                                                    ConfSchema::Group &retwindowgrp,
                                                    ParameterGroup &cdpsetting,
                                                    ParameterGroup &cdppolicypg)
{
    INM_ERROR errCode = E_SUCCESS;
    std::string cdp_catalogue;
    std::string cdp_alert_threshold;
    std::string cdp_onspace_shortage;
    unsigned long long cdp_timepolicy;

    cdp_timepolicy = 0;
    if (retwindowgrp.m_Objects.size())
    {
        ConfSchema::Object_t::size_type i = 0;
        for (ConfSchema::ObjectsIter_t oiter = retwindowgrp.m_Objects.begin(); oiter != retwindowgrp.m_Objects.end(); oiter++, i++)
        {
            ConfSchema::Object &retwindowobj = oiter->second;
            const std::string &retwindowid = retwindowobj.m_Attrs[m_RetWindowObj.GetIdAttrName()];
            ConfSchema::GroupsIter_t grpiter = retwindowobj.m_Groups.find(m_RetEvtPoliciesObj.GetName());
            if (grpiter == retwindowobj.m_Groups.end())
            {
                errCode = E_NO_DATA_FOUND;
                DebugPrintf(SV_LOG_ERROR, "For target device %s, window id %s, no retention event information is present\n",
                                          tgtname.c_str(), retwindowid.c_str());
                break;
            }
            ConfSchema::Group &reteventgrp = grpiter->second;
            ConfSchema::Object_t::size_type nreteventobj = reteventgrp.m_Objects.size();
            if (nreteventobj != 1)
            {
                errCode = E_NO_DATA_FOUND;
                std::stringstream ssnretevent;
                ssnretevent << nreteventobj;
                DebugPrintf(SV_LOG_ERROR, "For target device %s, window id %s, %s retention event objects are present\n", 
                                          tgtname.c_str(), retwindowid.c_str(), ssnretevent.str().c_str());
                break;
            }
            ConfSchema::Object &reteventobj = (reteventgrp.m_Objects.begin())->second;
            /* From CX code itself, these values are getting overwritten */
            cdp_catalogue = reteventobj.m_Attrs[m_RetEvtPoliciesObj.GetCatalogPathAttrName()];
            cdp_alert_threshold = reteventobj.m_Attrs[m_RetEvtPoliciesObj.GetAlertThresholdAttrName()];
            cdp_onspace_shortage = "0";//reteventobj.m_Attrs[m_RetEvtPoliciesObj.GetStoragePrunePolicyAttrName()];

            std::stringstream sspolicynumber;
            sspolicynumber << i;
            std::string cdppolicyname = NS_CDP_SETTINGS::cdp_policies;
            cdppolicyname += "[";
            cdppolicyname += sspolicynumber.str();
            cdppolicyname += "]";
            std::pair<ParameterGroupsIter_t, bool> cdppolicypair = cdppolicypg.m_ParamGroups.insert(std::make_pair(cdppolicyname, ParameterGroup()));
            ParameterGroupsIter_t cdppolicyiter = cdppolicypair.first;
            ParameterGroup &cdppolicy = cdppolicyiter->second;
             
            const std::string &start = retwindowobj.m_Attrs[m_RetWindowObj.GetStartTimeAttrName()];
            std::stringstream ssstart(start);
            unsigned long long starttime = 0;
            ssstart >> starttime;
            const std::string &end = retwindowobj.m_Attrs[m_RetWindowObj.GetEndTimeAttrName()];
            unsigned long long endtime = 0;
            std::stringstream ssend(end);
            ssend >> endtime;
            unsigned long long duration = endtime - starttime;
            insertintoreqresp(cdppolicy, cxArg ( starttime ), NS_cdp_policy::start);
            insertintoreqresp(cdppolicy, cxArg ( duration ), NS_cdp_policy::duration);
            cdp_timepolicy += duration;
  
            //const std::string &granularity = retwindowobj.m_Attrs[m_RetWindowObj.GetTimeGranularityAttrName()];
            const std::string &granularity = retwindowobj.m_Attrs[m_RetWindowObj.GetGranularityUnitAttrName()];
            cdppolicy.m_Params.insert(std::make_pair(NS_cdp_policy::granularity, ValueType("unsigned int", granularity)));

            const std::string &tagcount = reteventobj.m_Attrs[m_RetEvtPoliciesObj.GetTagCountAttrName()];
            cdppolicy.m_Params.insert(std::make_pair(NS_cdp_policy::tagcount, ValueType("unsigned int", tagcount)));
             
            /* cx sends 0 for this */
            cdppolicy.m_Params.insert(std::make_pair(NS_cdp_policy::cdp_diskspace_forthiswindow, ValueType("unsigned long long", "0")));
         
            /* TODO: need the os type here; and refine this  
             * below code which is taken from CX as is */
            const std::string &storagepath = reteventobj.m_Attrs[m_RetEvtPoliciesObj.GetStoragePathAttrName()];
            std::string cdp_contentstore;
            /*
            if(agent is windows)
            {
            */
                std::vector<std::string> tokens;
                std::string stp = storagepath;
                Tokenize(stp, tokens, ":");
                if (tokens.size() == 1)
                {
                    if (stp.length() && (stp[stp.length() - 1] != ':'))
                    {
                        stp.push_back(':');
                    }
                }
                cdp_contentstore = stp;
                cdp_contentstore += "\\contentstore";

                /*
                $content_store_arr = split(':',$storagePath);
                if(count($content_store_arr) == 1)
                {
                    $storagePath = $storagePath.":";
                }
                $content_store = "$storagePath\\contentstore";
                */
            /*
            }
            else
            {
                cdp_contentstore = storagePath;
                cdp_contentstore += "/contentstore";
            }
            */
            cdppolicy.m_Params.insert(std::make_pair(NS_cdp_policy::cdp_contentstore, ValueType("string", cdp_contentstore)));
             
            const std::string &consistencytag = reteventobj.m_Attrs[m_RetEvtPoliciesObj.GetConsistencyTagAttrName()]; 
            /* TODO: there is mapping from tag name to the tag enum at 
             *       CX from consistencyTagList relation; need to store this 
             * (or) there is header file in vacp may be having same mapping ? */
            /* CX code:
             * $tag_type = get_tag_type($row_app->consistencyTag);
             * #$logging_obj->my_error_handler("DEBUG","vxstub_get_cdp_settings::tag_type:: $tag_type \n");
             * $tag_value = AppNameToTagType($tag_type);
             * #$logging_obj->my_error_handler("DEBUG","vxstub_get_cdp_settings::tag_value:: $tag_value \n");
             * $arr[7]=$tag_value;
            */
            unsigned int apptype = STREAM_REC_TYPE_FS_TAG; /* getapptypefromtagtype(consistencytag) */;
            insertintoreqresp(cdppolicy, cxArg ( apptype ), NS_cdp_policy::apptype);
 
            /* just replicating CX behaviour */
            std::pair<ParameterGroupsIter_t, bool> userbookmarkspair = cdppolicy.m_ParamGroups.insert(std::make_pair(NS_cdp_policy::userbookmarks, 
                                                                                                                      ParameterGroup()));
            ParameterGroupsIter_t userbookmarksiter = userbookmarkspair.first;
            ParameterGroup &userbookmarkspg = userbookmarksiter->second;
            const std::string &userdefinedtag = reteventobj.m_Attrs[m_RetEvtPoliciesObj.GetUserDefinedTagAttrName()]; 
            std::vector<std::string> tags;
            Tokenize(userdefinedtag, tags, ",");
            std::vector<std::string>::size_type it = 0;
            for (std::vector<std::string>::const_iterator titer = tags.begin(); titer != tags.end(); titer++, it++)
            {
                std::stringstream ssit;
                ssit << it;
                std::string eachbookmarkname = NS_cdp_policy::userbookmarks;
                eachbookmarkname += "[";
                eachbookmarkname += ssit.str();
                eachbookmarkname += "]";
                userbookmarkspg.m_Params.insert(std::make_pair(eachbookmarkname, ValueType("string", *titer)));
            }
        }
        if (E_SUCCESS == errCode)
        {
             cdpsetting.m_Params.insert(std::make_pair(NS_CDP_SETTINGS::cdp_catalogue, ValueType("string", cdp_catalogue)));
             cdpsetting.m_Params.insert(std::make_pair(NS_CDP_SETTINGS::cdp_alert_threshold, ValueType("unsigned int", cdp_alert_threshold)));
             cdpsetting.m_Params.insert(std::make_pair(NS_CDP_SETTINGS::cdp_onspace_shortage, ValueType("unsigned int", cdp_onspace_shortage)));
             insertintoreqresp(cdpsetting, cxArg ( cdp_timepolicy ), NS_CDP_SETTINGS::cdp_timepolicy);
        }
    }
    else
    {
        /* TODO: update errCode everywhere */
        errCode = E_NO_DATA_FOUND;
        /* as per CX team, there is atleast one window */
        DebugPrintf(SV_LOG_ERROR, "For target volume %s, not atleast one window present\n", tgtname.c_str());
    }

    return errCode;
}
                                                     

void SettingsHandler::FillATLunStatsReq(ParameterGroup &pg)
{
    insertintoreqresp(pg, cxArg ( ATLUN_STATS_REQUEST::ATLUN_STATS_NOREQUEST ), NS_ATLUN_STATS_REQUEST::type);
    pg.m_Params.insert(std::make_pair(NS_ATLUN_STATS_REQUEST::atLUNName, ValueType("string", std::string())));
    pg.m_ParamGroups.insert(std::make_pair(NS_ATLUN_STATS_REQUEST::physicalInitiatorPWWNs, ParameterGroup()));
}


void SettingsHandler::FillThrottleSettings(ParameterGroup &pg)
{
    /* TODO: need to store or generate throttle ? */
    pg.m_Params.insert(std::make_pair(NS_THROTTLE_SETTINGS::throttleResync, ValueType("bool", "0")));
    pg.m_Params.insert(std::make_pair(NS_THROTTLE_SETTINGS::throttleSource, ValueType("bool", "0")));
    pg.m_Params.insert(std::make_pair(NS_THROTTLE_SETTINGS::throttleTarget, ValueType("bool", "0")));
}


void SettingsHandler::FillSanVolumeInfo(ParameterGroup &pg)
{
    pg.m_Params.insert(std::make_pair(NS_SAN_VOLUME_INFO::isSanVolume, ValueType("bool", "0")));
    pg.m_Params.insert(std::make_pair(NS_SAN_VOLUME_INFO::physicalDeviceName, ValueType("string", std::string())));
    pg.m_Params.insert(std::make_pair(NS_SAN_VOLUME_INFO::mirroredDeviceName, ValueType("string", std::string())));
    pg.m_Params.insert(std::make_pair(NS_SAN_VOLUME_INFO::virtualDevicename, ValueType("string", std::string())));
    pg.m_Params.insert(std::make_pair(NS_SAN_VOLUME_INFO::virtualName, ValueType("string", std::string())));
    pg.m_Params.insert(std::make_pair(NS_SAN_VOLUME_INFO::physicalLunId, ValueType("string", std::string())));
}


INM_ERROR SettingsHandler::GetCurrentInitialSettingsV2(FunctionInfo &f)
{
    /* TODO: change this to not supported error code */
    return E_UNKNOWN_FUNCTION;
}

std::string SettingsHandler::GetCleanUpActionString(ConfSchema::Object &protectionpairobj)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    std::string cleanUpActionString;
    std::string cleanUpActions = protectionpairobj.m_Attrs[m_ProtectionPairObj.GetReplicationCleanupOptionAttrName()];
    if(!cleanUpActions.empty())
    {
        std::string cleanUpSubStr = cleanUpActions.substr(CACHECLEANUPPOS,2);
        if(cleanUpSubStr == "10")
        {
            cleanUpActionString += "cache_dir_del=yes;cache_dir_del_status=35;cache_dir_del_message=0";

        }
        cleanUpSubStr = cleanUpActions.substr(PENDINGFILESPOS,2);
        if(cleanUpSubStr == "10")
        {
            if(!cleanUpActionString.empty())
                cleanUpActionString += ",";
            cleanUpActionString += "pending_action_folder_cleanup=yes;pending_action_folder_cleanup_status=39;pending_action_folder_cleanup_message=0";

        }
        else if(cleanUpSubStr == "01")
        {
            if(!cleanUpActionString.empty())
                cleanUpActionString += ",";
            cleanUpActionString += "pending_action_folder_cleanup=no;pending_action_folder_cleanup_status=40;pending_action_folder_cleanup_message=0";
        }
        cleanUpSubStr = cleanUpActions.substr(VSNAPCLEANUPPOS,2);
        if(cleanUpSubStr == "10")
        {
            if(!cleanUpActionString.empty())
                cleanUpActionString += ",";
            cleanUpActionString += "vsnap_cleanup=yes;vsnap_cleanup_status=37;vsnap_cleanup_message=0";

        }
        cleanUpSubStr = cleanUpActions.substr(RETENTIONLOGPOS,2);
        DebugPrintf(SV_LOG_DEBUG," cleanUpSubStr %s\n",cleanUpSubStr.c_str());
        if(cleanUpSubStr == "10")
        {
            if(!cleanUpActionString.empty())
                cleanUpActionString += ",";
            cleanUpActionString += "log_cleanup=yes;log_cleanup_status=31;log_cleanup_message=0";

        }
        cleanUpSubStr = cleanUpActions.substr(UNLOCKTGTDEVICEPOS,2);
        if(cleanUpSubStr == "10")
        {
            if(!cleanUpActionString.empty())
                cleanUpActionString += ",";
            cleanUpActionString += "unlock_volume=yes;unlock_vol_status=33;unlock_vol_message=0";

        }
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG,"CleanupAction is empty\n");
    }
    DebugPrintf(SV_LOG_DEBUG, "CleanUpActionString %s\n",cleanUpActionString.c_str());
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
    return cleanUpActionString;
}

std::string SettingsHandler::GetMaintainenceActionString(ConfSchema::Object &protectionpairobj, bool source)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    std::string maintainenceActionBitMap = protectionpairobj.m_Attrs[m_ProtectionPairObj.GetMaintenanceActionAttrName()];;
    const std::string &pauseReason = protectionpairobj.m_Attrs[m_ProtectionPairObj.GetPauseReasonAttrName()];
    std::string maintainenceActionString ;
    DebugPrintf(SV_LOG_DEBUG,"Pause Reason %s\n",pauseReason.c_str());
    int pairState = boost::lexical_cast<int>(protectionpairobj.m_Attrs[m_ProtectionPairObj.GetPairStateAttrName()]) ;
    int bitmapIndex = -1 ; 
    if( source )
    {
        bitmapIndex = 0 ;
    }
    else
    {
        bitmapIndex = 1 ;
    }
    if( maintainenceActionBitMap.empty() || maintainenceActionBitMap[bitmapIndex] != '1' )
    {
        return maintainenceActionString ;
    }
    if(pauseReason == "Pause By User")
    {
        switch( pairState )
        {
            case VOLUME_SETTINGS::PAUSE_PENDING :
                if( source ) {
                    maintainenceActionString = "" ;//No maintenance action for the source 
                }
                else {
                    maintainenceActionString =  "pause_components=yes;components=all;pause_components_status=requested;pause_components_message=";
                }
                break ;
            case VOLUME_SETTINGS::PAUSE_TACK_PENDING :
                if( source ) {
                    maintainenceActionString =  "pause_tracking=yes;components=all;pause_tracking_status=requested;pause_tracking_message=";
                }
                else {
                    maintainenceActionString =  "pause_components=yes;components=all;pause_components_status=requested;pause_components_message=";
                }
                break ;
            default:
                DebugPrintf(SV_LOG_DEBUG, "No Maintenance action for %d\n", pairState) ;
        }
    }
    else if(pauseReason == "Move Retention")
    {
        std::stringstream stream;
        const std::string & cataloguePath = protectionpairobj.m_Attrs[m_RetEvtPoliciesObj.GetCatalogPathAttrName()]; //change obj to retention log;

        switch (pairState)
        {
            case VOLUME_SETTINGS::PAUSE_PENDING :
                stream << "pause_components=yes;components=affected;pause_components_status=requested;pause_components_message=;move_retention=yes;";
                stream << "move_retention_old_path=" ;//Set Current Retention path
                stream << cataloguePath;
                stream << ";";
                stream <<"move_retention_path=" ;
                //stream <<  ; From where we need to read new retention path.
                stream <<";";
                stream <<"move_retention_status=requested;";
                stream << "move_retention_message=;";
                maintainenceActionString =  stream.str();
                break ;
            case VOLUME_SETTINGS::PAUSE :
                maintainenceActionString = protectionpairobj.m_Attrs[m_ProtectionPairObj.GetMaintenanceActionAttrName()];
                break ;
        }
    }
    else if(pauseReason == "Insufficient Disk Space")
    {
        std::stringstream stream ;
        switch( pairState )
        {
            case VOLUME_SETTINGS::PAUSE_PENDING:
                stream << "pause_components=yes;components=affected;pause_components_status=requested;pause_components_message=;";
                stream << "insufficient_space_components=yes;" ;
                stream << "insufficient_space_status=requested;";
                stream << "insufficient_space_message=;";
                maintainenceActionString =  stream.str();
                break ;
            case VOLUME_SETTINGS::PAUSE :
                maintainenceActionString = protectionpairobj.m_Attrs[m_ProtectionPairObj.GetMaintenanceActionAttrName()];
                break ;
        }
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG,"Maintainence Action choosen is not valide type\n");
    }
    DebugPrintf(SV_LOG_DEBUG, "maintainenceString %s\n",maintainenceActionString.c_str());
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
    return maintainenceActionString;
}
