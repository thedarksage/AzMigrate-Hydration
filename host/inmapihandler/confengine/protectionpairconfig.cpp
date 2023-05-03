#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/replace.hpp>

#include "protectionpairconfig.h"
#include "svtypes.h"
#include "util.h"
#include "volumegroupsettings.h"
#include "Handler.h"
#include "inmsdkutil.h"
#include "paircreationhandler.h"
#include "confschemareaderwriter.h"
#include "confengine/volumeconfig.h"
#include "confengine/alertconfig.h"
#include "host.h"
#include "auditconfig.h"
#include "scenarioconfig.h"
#include "cdpplatform.h"
#include "confengine/agentconfig.h"
#include "inmsafecapis.h"

ProtectionPairConfigPtr ProtectionPairConfig::m_protectionPairConfigPtr ;
ProtectionPairConfigPtr ProtectionPairConfig::GetInstance()
{
    if( !m_protectionPairConfigPtr )
    {
        m_protectionPairConfigPtr.reset( new ProtectionPairConfig() ) ;
    }
    m_protectionPairConfigPtr->loadProtectionPairGrp() ;
    return m_protectionPairConfigPtr ;
}

ProtectionPairConfig::ProtectionPairConfig()
{
    m_isModified = false ;
    m_ProtectionPairGrp = NULL ;
}

/*
1. Set the group name
2. Read Conf Schema
3. Get the Protection Pair Group
*/
void ProtectionPairConfig::loadProtectionPairGrp()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n",FUNCTION_NAME) ;
    ConfSchemaReaderWriterPtr confSchemaReaderWriter = ConfSchemaReaderWriter::GetInstance() ;
    m_ProtectionPairGrp = confSchemaReaderWriter->getGroup( m_proPairAttrNamesObj.GetName() ) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

/*
checks whether the protection pair group is changed or not.
*/
bool ProtectionPairConfig::isModified()
{
    return m_isModified ;
}

bool ProtectionPairConfig::IsPairInStepOne(ConfSchema::Object* protectionPairObj)
{    
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    ConfSchema::Attrs_t& proPairAttrs = protectionPairObj->m_Attrs ;
    ConfSchema::AttrsIter_t attrIter ;
    attrIter = proPairAttrs.find( m_proPairAttrNamesObj.GetReplStateAttrName()) ;
    int replState = boost::lexical_cast<int>(attrIter ->second ) ;
    attrIter = proPairAttrs.find( m_proPairAttrNamesObj.GetSrcNameAttrName() ) ;
    std::string srcDevName, tgtDevName ;
    srcDevName = attrIter->second ;
    attrIter = proPairAttrs.find( m_proPairAttrNamesObj.GetTgtNameAttrName() ) ;
    tgtDevName = attrIter->second ;
    DebugPrintf(SV_LOG_DEBUG, "Src Dev Name %s, Tgt Dev Name %s, Repl State %d\n", 
        srcDevName.c_str(), tgtDevName.c_str(), replState) ;
    if( replState == 7 )
    {
        bRet = true ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool ProtectionPairConfig::GetRetentionLogPolicyObject(ConfSchema::Object* protectionPairObj,
                                                       ConfSchema::Object** retLogPolicyObj)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ConfSchema::GroupsIter_t retentionLogGroupIter ;
    bool bRet = false ;
    retentionLogGroupIter = protectionPairObj->m_Groups.find(m_retLogPolicyAttrNamesObj.GetName()) ;
    if( retentionLogGroupIter != protectionPairObj->m_Groups.end() )
    {
        ConfSchema::ObjectsIter_t retObjIter = retentionLogGroupIter->second.m_Objects.begin() ;
        bRet = true ;
        *retLogPolicyObj = &(retObjIter->second) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}
void ProtectionPairConfig::RestartResyncForPair (const std::string &resizedVolume)
{
	ProtectionPairConfigPtr protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
	ScenarioConfigPtr scenarioConfigPtr = ScenarioConfig::GetInstance() ;
	SV_UINT batchSize = scenarioConfigPtr->GetBatchSize(); 
	protectionPairConfigPtr->RestartResyncForPair (resizedVolume,batchSize); 	
	return; 
}
void ProtectionPairConfig::doStopFilteringAndRestartResync (const std::string &resizedVolume)
{
	DebugPrintf (SV_LOG_DEBUG  , "ENTERED %s \n", FUNCTION_NAME); 
	ProtectionPairConfigPtr protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
	DRIVER_STATE stopFilteringStatus = getStopFilteringStatus (resizedVolume);
	bool restartFlag = false; 
	if (stopFilteringStatus == STOPPED_FILTERING)
	{
		restartFlag = true; 
	}
	else 
	{
		if (executeStopFilteringCommand  (resizedVolume) == true)
		{
			restartFlag = true;
		}

	}
	if (restartFlag == true)
	{
		protectionPairConfigPtr->RestartResyncForPair (resizedVolume); 
	}
	DebugPrintf (SV_LOG_DEBUG  , "Exited %s \n", FUNCTION_NAME); 
	return; 
}

void ProtectionPairConfig::validateAndActOnResizedVolume(const std::string &resizedVolume)
{
	DebugPrintf (SV_LOG_DEBUG , "ENTERED %s \n", FUNCTION_NAME); 
	VolumeConfigPtr volumeConfigPtr = VolumeConfig::GetInstance(); 
	ProtectionPairConfigPtr protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
	DebugPrintf (SV_LOG_DEBUG , " ResizedVolume is in validateAndActOnResizedVolume is %s \n " , resizedVolume.c_str()); 
	SV_LONGLONG volumeCapcityInExistence = 0 ;
	bool stopfilteringFlag  = false; 
	protectionPairConfigPtr->IncrementVolumeResizeActionCount(resizedVolume); 
	if	(volumeConfigPtr->isExtendedOrShrunkVolumeGreaterThanSparseSize(resizedVolume , volumeCapcityInExistence ) == true ) 
	{
		if (volumeConfigPtr->isVirtualVolumeExtended(resizedVolume) == false)
		{
			bool cdpCliStatus = executeVolumeResizeCDPCLICommand (resizedVolume); 
			if (cdpCliStatus  == false  )
			{
				// Already Send Alert 
			}
			else 
			{
				stopfilteringFlag  = true; 
			}
		}
		else 
		{
			stopfilteringFlag = true; 
		}
		if (stopfilteringFlag == true) 
		{
			protectionPairConfigPtr->doStopFilteringAndRestartResync(resizedVolume); 
		}
	}
	else 
	{
		// This is shrunk volume 
		SV_LONGLONG capacity = 0 ; 
		volumeConfigPtr->GetCapacity (resizedVolume,capacity); 

		std::string alertMsg; 
		alertMsg	= resizedVolume.c_str(); 
		alertMsg	+= " was successfully protected when capacity was "; 
		alertMsg	+= boost::lexical_cast <std::string> (volumeCapcityInExistence); 
		alertMsg	+= " Bytes. Current capacity is ";
		alertMsg	+= boost::lexical_cast <std::string> (capacity); 
		alertMsg	+= " Bytes. Please remove from the protection and add again. ";
		AlertConfigPtr alertConfigPtr = AlertConfig::GetInstance() ;
		alertConfigPtr->AddAlert("WARNING", "CX", SV_ALERT_SIMPLE, SV_ALERT_MODULE_RESYNC, alertMsg) ;	
	}

	DebugPrintf (SV_LOG_DEBUG  , "Exited %s \n", FUNCTION_NAME); 
	return; 
}

bool ProtectionPairConfig::isVolumePaused (const std::string &volumeName )
{
	DebugPrintf (SV_LOG_DEBUG , "ENTERED %s \n ", FUNCTION_NAME); 
	bool bRet = false; 
	VOLUME_SETTINGS::PAIR_STATE pairState; 
	GetPairState (volumeName , pairState);
	if (pairState == VOLUME_SETTINGS::PAUSE)
	{
		bRet = true; 
	}
	DebugPrintf (SV_LOG_DEBUG, " EXITED %s \n ", FUNCTION_NAME ); 
	return bRet; 
}

bool ProtectionPairConfig::IsPairInStepTwo(ConfSchema::Object* protectionPairObj)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    ConfSchema::Attrs_t& proPairAttrs = protectionPairObj->m_Attrs ;
    ConfSchema::AttrsIter_t attrIter ;
    attrIter = proPairAttrs.find( m_proPairAttrNamesObj.GetReplStateAttrName()) ;
    DebugPrintf(SV_LOG_DEBUG, "Replication State %s\n", attrIter->second.c_str()) ;
    if( attrIter->second.compare("5") == 0 )
    {
        bRet = true ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool ProtectionPairConfig::IsPairInDiffSync(ConfSchema::Object* protectionPairObj)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    ConfSchema::Attrs_t& proPairAttrs = protectionPairObj->m_Attrs ;
    ConfSchema::AttrsIter_t attrIter ;
    attrIter = proPairAttrs.find( m_proPairAttrNamesObj.GetReplStateAttrName()) ;
    DebugPrintf(SV_LOG_DEBUG, "Replication State %s\n", attrIter->second.c_str()) ;
    if( attrIter->second.compare("0") == 0 )
    {
        bRet = true ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool ProtectionPairConfig::IsPairInStepOne(const std::string& srcDeviceName,
                                           const std::string& tgtDeviceName,
                                           const std::string& srcHostId,
                                           const std::string& tgtHostId,
                                           const std::string& jobId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    ConfSchema::Object* protectionPairObj = NULL ;
    if( GetProtectionPairObj(srcDeviceName, tgtDeviceName, &protectionPairObj) )
    {
        bRet = IsPairInStepOne( protectionPairObj ) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool ProtectionPairConfig::IsPairInStepTwo(const std::string& srcDevName,
                                           const std::string& tgtDevName,
                                           const std::string& srcHostId,
                                           const std::string& tgtHostId,
                                           const std::string& jobId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    ConfSchema::Object* protectionPairObj = NULL ;
    if( GetProtectionPairObj(srcDevName, tgtDevName, &protectionPairObj) )
    {
        bRet = IsPairInStepTwo( protectionPairObj ) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool ProtectionPairConfig::IsPairInDiffSync(const std::string& srcDevName,
                                            const std::string& tgtDevName,
                                            const std::string& srcHostId,
                                            const std::string& tgtHostId,
                                            const std::string& jobId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    ConfSchema::Object* protectionPairObj = NULL ;
    if( !srcDevName.empty() && !tgtDevName.empty() )
    {
        bRet = GetProtectionPairObj( srcDevName, tgtDevName, &protectionPairObj ) ;
    }
    else if( !srcDevName.empty() )
    {
        bRet = GetProtectionPairObjBySrcDevName( srcDevName, &protectionPairObj ) ;
    }
    else if( !tgtDevName.empty() )
    {
        bRet = GetProtectionPairObjByTgtDevName( tgtDevName, &protectionPairObj ) ;
    }
    if( bRet )
    {
        bRet = IsPairInDiffSync( protectionPairObj ) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool ProtectionPairConfig::IsValidJobId(ConfSchema::Object* protectionPairObj, 
                                        const std::string& jobId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    if( protectionPairObj != NULL )
    {
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = protectionPairObj->m_Attrs.find( m_proPairAttrNamesObj.GetJobIdAttrName() ) ;
        if( attrIter != protectionPairObj->m_Attrs.end() )
        {
            if( attrIter->second.compare(jobId) == 0 )
            {
                bRet = true ;
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "Job Id MISMATCH. In pair %s. In Request %s\n", 
                    attrIter->second.c_str(), jobId.c_str()) ;
            }
        }
        else
        {
            throw "Job Id attribute not found in the protection pair object\n" ;
        }
    }
    else
    {
        throw "Invalid Protection Pair Object\n" ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool ProtectionPairConfig::GetProtectionPairObj(const std::string& srcDeviceName,
                                                const std::string& tgtDeviceName,
                                                ConfSchema::Object** protectionPairObj,
                                                const std::string& srcHostId,
                                                const std::string& tgtHostId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool isFound = false ;
    ConfSchema::ObjectsIter_t protPairObjIter ;
    protPairObjIter = find_if(m_ProtectionPairGrp->m_Objects.begin(),
        m_ProtectionPairGrp->m_Objects.end(),
        ConfSchema::EqAttr(m_proPairAttrNamesObj.GetSrcNameAttrName(),
        srcDeviceName.c_str())) ;
    if( protPairObjIter != m_ProtectionPairGrp->m_Objects.end() )
    {
        *protectionPairObj = &(protPairObjIter->second) ;
        isFound = true ;
    }
    if( protPairObjIter != m_ProtectionPairGrp->m_Objects.end() && !tgtDeviceName.empty() )
    {
        DebugPrintf(SV_LOG_DEBUG, "Pair with %s as %s found\n", 
            srcDeviceName.c_str(),
            m_proPairAttrNamesObj.GetSrcNameAttrName()) ;

        protPairObjIter = find_if(m_ProtectionPairGrp->m_Objects.begin(),
            m_ProtectionPairGrp->m_Objects.end(),
            ConfSchema::EqAttr(m_proPairAttrNamesObj.GetTgtNameAttrName(),
            tgtDeviceName.c_str())) ;
        if( protPairObjIter != m_ProtectionPairGrp->m_Objects.end()) 
        {
            DebugPrintf(SV_LOG_DEBUG, "Pair with %s as %s found\n", 
                tgtDeviceName.c_str(),
                m_proPairAttrNamesObj.GetTgtNameAttrName()) ;
            *protectionPairObj = &(protPairObjIter->second) ;
            isFound = true ;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return isFound ;
}

bool ProtectionPairConfig::GetProtectionPairObjBySrcDevName(const std::string& srcDevName,
                                                            ConfSchema::Object** protectionPairObj)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool isFound = false ;
    ConfSchema::ObjectsIter_t protPairObjIter ;
    protPairObjIter = find_if(m_ProtectionPairGrp->m_Objects.begin(),
        m_ProtectionPairGrp->m_Objects.end(),
        ConfSchema::EqAttr(m_proPairAttrNamesObj.GetSrcNameAttrName(),
        srcDevName.c_str())) ;

    if( protPairObjIter != m_ProtectionPairGrp->m_Objects.end() )
    {
        DebugPrintf(SV_LOG_DEBUG, "Pair with %s as %s found\n", 
            srcDevName.c_str(),
            m_proPairAttrNamesObj.GetSrcNameAttrName()) ;
        *protectionPairObj = &(protPairObjIter->second) ;
        isFound = true ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return isFound ;
}

bool ProtectionPairConfig::GetProtectionPairObjByTgtDevName(const std::string& tgtDevName,
                                                            ConfSchema::Object** protectionPairObj)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool isFound = false ;
    DebugPrintf (SV_LOG_DEBUG , "    tgtDevName.c_str() is %s : \n ",tgtDevName.c_str());
    ConfSchema::ObjectsIter_t protPairObjIter ;
    protPairObjIter = find_if(m_ProtectionPairGrp->m_Objects.begin(),
        m_ProtectionPairGrp->m_Objects.end(),
        ConfSchema::EqAttr(m_proPairAttrNamesObj.GetTgtNameAttrName(),
        tgtDevName.c_str())) ;

    if( protPairObjIter != m_ProtectionPairGrp->m_Objects.end() )
    {
        DebugPrintf(SV_LOG_DEBUG, "Pair with %s as %s found\n", 
            tgtDevName.c_str(),
            m_proPairAttrNamesObj.GetTgtNameAttrName()) ;
        *protectionPairObj = &(protPairObjIter->second) ;
        isFound = true ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return isFound ;
}

bool ProtectionPairConfig::IsPairInQueuedState(ConfSchema::Object* protectionPairObj)
{
    bool bRet = false; 
    ConfSchema::AttrsIter_t attrIter ;
    attrIter = protectionPairObj->m_Attrs.find( m_proPairAttrNamesObj.GetExecutionStateAttrName()) ;
    int executionState = boost::lexical_cast<int>(attrIter->second) ;
    if( executionState == PAIR_IS_QUEUED )
    {
        bRet = true ;
    }
    return bRet ;
}

bool ProtectionPairConfig::IsDeletionInProgress(ConfSchema::Object* protectionPairObj)
{
    bool bRet = false; 
    ConfSchema::AttrsIter_t attrIter ;
    attrIter = protectionPairObj->m_Attrs.find( m_proPairAttrNamesObj.GetPairStateAttrName()) ;
    int pairState = boost::lexical_cast<int>(attrIter->second) ;
    if( pairState == VOLUME_SETTINGS::DELETE_PENDING || pairState == VOLUME_SETTINGS::CLEANUP_DONE )
    {
        bRet = true ;
    }
    return bRet ;
}

SV_INT ProtectionPairConfig::GetNumberOfPairsProgressingInStepOne ()
{
    DebugPrintf (SV_LOG_DEBUG, "ENTERED ", FUNCTION_NAME );
    SV_INT count = 0; 
    ConfSchema::ObjectsIter_t protectionPairObjIter ;
    protectionPairObjIter = m_ProtectionPairGrp->m_Objects.begin() ;
    while (m_ProtectionPairGrp->m_Objects.end() != protectionPairObjIter)
    {
        ConfSchema::Object *protectionPairObj = &(protectionPairObjIter->second) ;
        VOLUME_SETTINGS::PAIR_STATE pairState ; 
        if( IsPairInStepOne (protectionPairObj) && 
            !IsPairInQueuedState(protectionPairObj) && 
            !IsDeletionInProgress(protectionPairObj))
        {
            count ++;
        }
        protectionPairObjIter++ ;
    }
    DebugPrintf (SV_LOG_DEBUG, "EXITED ", FUNCTION_NAME );
    return count;
}
void ProtectionPairConfig::ActOnQueuedPair()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ConfSchema::ObjectsIter_t protectionPairObjIter ;
    bool proceed = true ;
    ScenarioConfigPtr scenarioConfigPtr =ScenarioConfig::GetInstance();
    SV_INT batchSize = scenarioConfigPtr->GetBatchSize();  // X 
    ProtectionPairConfigPtr protectionPairConfig = ProtectionPairConfig::GetInstance();
    SV_INT volumesInStepOneNonQueued = protectionPairConfig->GetNumberOfPairsProgressingInStepOne();
    if( batchSize == 0 )
    {
        batchSize = 999 ;
    }
    if( ( batchSize - volumesInStepOneNonQueued > 0 ) ) 
    {
        SV_INT noOfPairsTobeMovedToStep1 = batchSize - volumesInStepOneNonQueued ;
        protectionPairObjIter = m_ProtectionPairGrp->m_Objects.begin() ;
        while( m_ProtectionPairGrp->m_Objects.end() != protectionPairObjIter )
        {
            ConfSchema::Object *protectionPairObj = &(protectionPairObjIter->second) ;
            if( IsPairInStepOne(protectionPairObj) )
            {
                VOLUME_SETTINGS::PAIR_STATE pairState ; 
                if( IsPairInQueuedState(protectionPairObj) && 
                    IsResyncProtectionAllowed(protectionPairObj, pairState, false))
                {
                    //Just Remove the Queued State ;
                    ConfSchema::AttrsIter_t attrIter ;
                    attrIter = protectionPairObj->m_Attrs.find( m_proPairAttrNamesObj.GetExecutionStateAttrName() ) ;
                    attrIter->second = boost::lexical_cast<std::string>(PAIR_IS_NONQUEUED) ;
                    attrIter = protectionPairObj->m_Attrs.find(m_proPairAttrNamesObj.GetResyncStartTime()) ;
                    std::string resyncstarttime = boost::lexical_cast<std::string>(time(NULL)) ;
                    attrIter->second = resyncstarttime ;
                    noOfPairsTobeMovedToStep1-- ;
                    if( noOfPairsTobeMovedToStep1 == 0 )
                    {
                        break ;
                    }
                }
            }
            protectionPairObjIter++ ;
        }
        //If there are no pairs in resync queued pairs, 
        //check the queued states of pairs in non-resync.
        protectionPairObjIter = m_ProtectionPairGrp->m_Objects.begin() ;
        if( noOfPairsTobeMovedToStep1 > 0 )
        {
            while( m_ProtectionPairGrp->m_Objects.end() != protectionPairObjIter )
            {
                ConfSchema::Object *protectionPairObj = &(protectionPairObjIter->second) ;
                if( !IsPairInStepOne(protectionPairObj) )
                {
                    VOLUME_SETTINGS::PAIR_STATE pairState ; 
                    if( IsPairInQueuedState(protectionPairObj) && 
                        IsResyncProtectionAllowed (protectionPairObj, pairState, false))
                    {
                        //Perform the restart resync
                        RestartResyncForPair(protectionPairObj, false) ;
                        noOfPairsTobeMovedToStep1-- ;
                        if( noOfPairsTobeMovedToStep1 == 0 )
                        {
                            break ;
                        }
                    }
                }
                protectionPairObjIter++ ;
            }
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

bool ProtectionPairConfig::IsResyncProtectionAllowed (ConfSchema::Object* protectionPairObj, 
                                                      VOLUME_SETTINGS::PAIR_STATE& pairState,
													  bool isManualResync) 
{
    GetPairState( protectionPairObj, pairState );
    bool AutoResyncDisabled = false ;
	if( !isManualResync )
	{
		AutoResyncDisabled = IsAutoResyncDisabled( *protectionPairObj ) ;
	}
	return IsResyncProtectionAllowed (pairState, AutoResyncDisabled);
}

bool ProtectionPairConfig::IsResyncProtectionAllowed (const std::string& srcDevName,
                                                      VOLUME_SETTINGS::PAIR_STATE& pairState,
													  bool isManualResync)
{
    GetPairState( srcDevName, pairState );
	bool AutoResyncDisabled = false ;
	if( !isManualResync )
	{
		AutoResyncDisabled = IsAutoResyncDisabled( srcDevName ) ;
	}
	DebugPrintf(SV_LOG_DEBUG, "Checking Resync is allowed or not for %s\n", srcDevName.c_str()) ;
    return IsResyncProtectionAllowed (pairState, AutoResyncDisabled);
}

bool ProtectionPairConfig::IsResyncProtectionAllowed (VOLUME_SETTINGS::PAIR_STATE& pairState, bool AutoResyncDisabled)
{
	bool bRet = true ;
	if( AutoResyncDisabled )
	{
		bRet = false ;
	}
	else if (pairState == VOLUME_SETTINGS::PAUSE ||
        pairState == VOLUME_SETTINGS::PAUSE_PENDING ||
        pairState == VOLUME_SETTINGS::PAUSE_TACK_COMPLETED ||
        pairState == VOLUME_SETTINGS::PAUSE_TACK_PENDING ||
        pairState == VOLUME_SETTINGS::DELETE_PENDING || 
        pairState == VOLUME_SETTINGS::CLEANUP_DONE )
	{
        bRet = false;
	}
	return bRet; 
}


bool ProtectionPairConfig::MovePairFromStepOneToTwo(const std::string& srcDeviceName,
                                                    const std::string& tgtDeviceName,
                                                    SV_LONGLONG srcVolumeCapacity,
                                                    const std::string& jobId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    ConfSchema::Object* protectionPairObj = NULL ;
    if( GetProtectionPairObj(srcDeviceName, tgtDeviceName, &protectionPairObj) )
    {
        if( IsValidJobId( protectionPairObj,jobId ) )
        {
            bRet = true ;
            ConfSchema::Attrs_t& protectionAttrs = protectionPairObj->m_Attrs ;
            ConfSchema::AttrsIter_t attrIter ;
            attrIter = protectionAttrs.find(m_proPairAttrNamesObj.GetReplStateAttrName());
            if( attrIter->second.compare("7") == 0 )
            {
                attrIter->second = boost::lexical_cast<std::string>(VOLUME_SETTINGS::SYNC_QUASIDIFF) ;
                SV_LONGLONG fastSyncUnmatched = 0;
                attrIter = protectionAttrs.find(m_proPairAttrNamesObj.GetFastResyncUnMatchedAttrName());
                fastSyncUnmatched = boost::lexical_cast<SV_LONGLONG>(attrIter->second) ;
                attrIter = protectionAttrs.find(m_proPairAttrNamesObj.GetFastResyncMatchedAttrName());
                SV_LONGLONG fastSyncMatched = srcVolumeCapacity - fastSyncUnmatched ;
                attrIter->second = boost::lexical_cast<std::string>( fastSyncMatched ) ;
                attrIter = protectionAttrs.find(m_proPairAttrNamesObj.GetShouldResyncAttrName());
                attrIter->second = "0" ;
				attrIter = protectionAttrs.find(m_proPairAttrNamesObj.GetResyncEndTime()) ;
				attrIter->second = boost::lexical_cast<std::string>(time(NULL)) ;
                if (!IsDeletionInProgress (srcDeviceName) )
                {
                    ActOnQueuedPair() ;
                }
				std::string alertMsg = "backup initialized for volume " + srcDeviceName ;
                			
				AlertConfigPtr alertConfigPtr = AlertConfig::GetInstance() ;
				alertConfigPtr->AddAlert("ALERT", "CX", SV_ALERT_SIMPLE, SV_ALERT_MODULE_RESYNC, alertMsg) ;

                AuditConfigPtr auditConfigPtr = AuditConfig::GetInstance() ;
                std::string auditmsg ;
                auditmsg = "Pair " + srcDeviceName + " <--> " + tgtDeviceName ;
                auditmsg += " completed initial sync. Moving to step 2" ;
                auditConfigPtr->LogAudit(auditmsg) ;
            }
            else
            {
                DebugPrintf(SV_LOG_DEBUG, "Replication State %s\n", attrIter->second.c_str()) ;
            }
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool ProtectionPairConfig::IsRestartResyncAllowed(ConfSchema::Object* protectionPairObj)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERER %s\n", FUNCTION_NAME) ;
    bool bRet = true  ;
    ConfSchema::AttrsIter_t attrIter ;
    attrIter = protectionPairObj->m_Attrs.find( m_proPairAttrNamesObj.GetResizePolicyTSAttrName() ) ;
    eVolumeResize resizeStatus ;
    resizeStatus = (eVolumeResize) boost::lexical_cast<int>(attrIter->second) ;
    if( resizeStatus != VOLUME_SIZE_NORMAL )
    {
        bRet = false ;
    }
    DebugPrintf(SV_LOG_DEBUG, "Resize Status %d\n", resizeStatus) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool ProtectionPairConfig::IsRestartResyncAllowed(const std::string& srcDevName,
                                                  const std::string& tgtDevName,
                                                  const std::string& srcHostId,
                                                  const std::string& tgtHostId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERER %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    ConfSchema::Object* protectionPairObj = NULL ;
    if( GetProtectionPairObj(srcDevName, tgtDevName, &protectionPairObj) )
    {
        bRet = IsRestartResyncAllowed(protectionPairObj) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool ProtectionPairConfig::GetResyncStartTagTimeAndSeqNo(const std::string& srcDeviceName,
                                                         const std::string& tgtDeviceName,
                                                         const std::string& jobId,
                                                         std::string& resyncStartTagTime,
                                                         std::string& resyncStartSeqNo)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    ConfSchema::Object* protectionPairObj = NULL ;
    if( GetProtectionPairObj(srcDeviceName, tgtDeviceName, &protectionPairObj) )
    {
        if( IsValidJobId( protectionPairObj,jobId ) )
        {
            bRet = true ;
            ConfSchema::Attrs_t& protectionPairAttrs = protectionPairObj->m_Attrs ;
            ConfSchema::AttrsIter_t attrIter ;
            attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetResyncStartTagTimeAttrName()) ;
            DebugPrintf(SV_LOG_DEBUG, "Attribute : %s , value : %s\n", 
                m_proPairAttrNamesObj.GetResyncStartTagTimeAttrName(),
                attrIter->second.c_str()) ;
            resyncStartTagTime = attrIter->second ;
            attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetResyncStartSeqAttrName()) ;
            DebugPrintf(SV_LOG_DEBUG, "Attribute : %s , value : %s\n", 
                m_proPairAttrNamesObj.GetResyncStartSeqAttrName(),
                attrIter->second.c_str()) ;
            resyncStartSeqNo = attrIter->second ;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool ProtectionPairConfig::GetResyncEndTagTimeAndSeqNo(const std::string& srcDeviceName,
                                                       const std::string& tgtDeviceName,
                                                       const std::string& jobId,
                                                       std::string& resyncEndTagTime,
                                                       std::string& resyncEndSeqNo)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ConfSchema::Object* protectionPairObj = NULL ;
    bool bRet = false ;
    if( GetProtectionPairObj(srcDeviceName, tgtDeviceName, &protectionPairObj) )
    {
        if( IsValidJobId( protectionPairObj,jobId ) )
        {
            bRet = true ;
            ConfSchema::Attrs_t& protectionPairAttrs = protectionPairObj->m_Attrs ;
            ConfSchema::AttrsIter_t attrIter ;
            attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetResyncEndTagTimeAttrName()) ;
            DebugPrintf(SV_LOG_DEBUG, "Attribute : %s , value : %s\n", 
                m_proPairAttrNamesObj.GetResyncEndTagTimeAttrName(),
                attrIter->second.c_str()) ;
            resyncEndTagTime = attrIter->second ;
            attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetResyncEndSeqAttrName()) ;
            DebugPrintf(SV_LOG_DEBUG, "Attribute : %s , value : %s\n", 
                m_proPairAttrNamesObj.GetResyncEndSeqAttrName(),
                attrIter->second.c_str()) ;
            resyncEndSeqNo = attrIter->second ;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool ProtectionPairConfig::SetResyncStartTagTimeAndSeqNo(const std::string& srcDevName,
                                                         const std::string& tgtDevName,
                                                         const std::string& timeStamp,
                                                         const std::string& seqNo,
                                                         const std::string& jobId,
                                                         const std::string& srcHostId,
                                                         const std::string& tgtHostId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    DebugPrintf(SV_LOG_DEBUG, "Src Dev : %s, tgt Dev %s\n", 
        srcDevName.c_str(),
        tgtDevName.c_str()) ;
    bool bRet = false ;
    ConfSchema::Object* protectionPairObj = NULL ;
    if( GetProtectionPairObj(srcDevName, tgtDevName, &protectionPairObj) )
    {
        if( IsValidJobId( protectionPairObj,jobId ) )
        {
            bRet = true ;
            DebugPrintf(SV_LOG_DEBUG, "time stamp : %s, seq no : %s\n", 
                timeStamp.c_str(),
                seqNo.c_str()) ;
            ConfSchema::Attrs_t& protectionPairAttrs = protectionPairObj->m_Attrs ;
            ConfSchema::AttrsIter_t attrIter ;
            attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetResyncStartTagTimeAttrName()) ;
            attrIter->second = timeStamp ;
            attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetResyncStartSeqAttrName()) ;
            attrIter->second = seqNo ;
            AuditConfigPtr auditConfigPtr = AuditConfig::GetInstance() ;
            std::string auditmsg ;
            auditmsg = "Resync start tag time for " ; 
            auditmsg += srcDevName ;
            auditmsg += " <--> " ;
            auditmsg += tgtDevName ;
            auditmsg += " is " ;
            auditmsg += timeStamp ;
            auditConfigPtr->LogAudit(auditmsg) ;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool ProtectionPairConfig::SetResyncEndTagTimeAndSeqNo(const std::string& srcDevName,
                                                       const std::string& tgtDevName,
                                                       const std::string& timeStamp,
                                                       const std::string& seqNo,
                                                       const std::string& jobId,
                                                       const std::string& srcHostId,
                                                       const std::string& tgtHostId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    DebugPrintf(SV_LOG_DEBUG, "Src Dev : %s, tgt Dev %s\n", 
        srcDevName.c_str(),
        tgtDevName.c_str()) ;

    ConfSchema::Object* protectionPairObj = NULL ;
    bool bRet = false ;
    if( GetProtectionPairObj(srcDevName, tgtDevName, &protectionPairObj) )
    {
        if( IsValidJobId( protectionPairObj,jobId ) )
        {
            bRet = true ;
            DebugPrintf(SV_LOG_DEBUG, "time stamp : %s, seq no : %s\n", 
                timeStamp.c_str(),
                seqNo.c_str()) ;
            ConfSchema::Attrs_t& protectionPairAttrs = protectionPairObj->m_Attrs ;
            ConfSchema::AttrsIter_t attrIter ;
            attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetResyncEndTagTimeAttrName()) ;
            attrIter->second = timeStamp ;
            attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetResyncEndSeqAttrName()) ;
            attrIter->second = seqNo ;
            AuditConfigPtr auditConfigPtr = AuditConfig::GetInstance() ;
            std::string auditmsg ;
            auditmsg = "Initial sync/resync completed for " ;
            auditmsg += srcDevName ;
            auditmsg += " <--> " ;
            auditmsg += tgtDevName ;
            auditmsg += " with driver stamp : " ;
            auditmsg += timeStamp ;
            auditmsg += " and sequence number : " ;
            auditmsg += seqNo ;
            auditConfigPtr->LogAudit( auditmsg ) ;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}


bool ProtectionPairConfig::UpdateLastResyncOffset( const std::string& srcDevName,
                                                  const std::string& tgtDevName,
                                                  const std::string& jobId,
                                                  SV_LONGLONG offset,
												  SV_LONGLONG fsunusedbytes,
                                                  const std::string& srcHostId,
                                                  const std::string& tgtHostId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    DebugPrintf(SV_LOG_DEBUG, "Src Dev : %s, tgt Dev %s, job Id %s\n", 
        srcDevName.c_str(),
        tgtDevName.c_str(), jobId.c_str()) ;
    ConfSchema::Object* protectionPairObj = NULL ;
    bool bRet = false ;
    if( GetProtectionPairObj(srcDevName, tgtDevName, &protectionPairObj) )
    {
        if( IsValidJobId( protectionPairObj,jobId ) )
        {
            bRet = true ;
            DebugPrintf(SV_LOG_DEBUG, "Last offset %ld\n", offset) ;
            ConfSchema::Attrs_t& protectionPairAttrs = protectionPairObj->m_Attrs ;
            ConfSchema::AttrsIter_t attrIter ;
            attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetLastOffsetAttrName()) ;
            attrIter->second = boost::lexical_cast<std::string>(offset) ;
			attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetFsUnusedBytes()) ;
            attrIter->second = boost::lexical_cast<std::string>(fsunusedbytes) ;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}


/*
TODO : Need to check what is the use of matched attribute
*/
bool ProtectionPairConfig::UpdateResyncProgress( const std::string& srcDevName,
                                                const std::string& tgtDevName,
                                                const std::string& jobId,
                                                const std::string& bytes,
                                                const std::string& matched,
                                                const std::string& srcHostId,
                                                const std::string& tgtHostId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    DebugPrintf(SV_LOG_DEBUG, "Src Dev : %s, tgt Dev %s, job Id %s\n", 
        srcDevName.c_str(),
        tgtDevName.c_str(), jobId.c_str()) ;
    ConfSchema::Object* protectionPairObj = NULL ;
    bool bRet = false ;
    if( GetProtectionPairObj(srcDevName, tgtDevName, &protectionPairObj) )
    {
        if( IsValidJobId( protectionPairObj,jobId ) )
        {
            bRet = true ;
            ConfSchema::Attrs_t& protectionPairAttrs = protectionPairObj->m_Attrs ;
            ConfSchema::AttrsIter_t attrIter ;
            attrIter = protectionPairAttrs.find( m_proPairAttrNamesObj.GetFastResyncUnMatchedAttrName()) ;
            DebugPrintf(SV_LOG_DEBUG, "fastsync unmatched bytes : %s, recieved unmatched bytes : %s\n",
                attrIter->second.c_str(), bytes.c_str()) ;
            SV_ULONGLONG fastSyncUnmathcedBytes = boost::lexical_cast<SV_ULONGLONG>(attrIter->second) ;
            SV_ULONGLONG receivedUnmatchedBytes = boost::lexical_cast<SV_ULONGLONG>(bytes) ;
            fastSyncUnmathcedBytes += receivedUnmatchedBytes ;
            attrIter->second =  boost::lexical_cast<std::string>(fastSyncUnmathcedBytes) ;
            DebugPrintf(SV_LOG_DEBUG, "fastsync unmatched bytes after update: %s\n",
                attrIter->second.c_str()) ;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

void ProtectionPairConfig::UpdatePendingChanges(const std::string& srcDevName,
                                                SV_LONGLONG pendingChangesInBytes,
                                                double rpo)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    DebugPrintf(SV_LOG_DEBUG, "Src Dev : %s, Pending Changes %ld\n", 
        srcDevName.c_str(), pendingChangesInBytes) ;
    ConfSchema::Object* protectionPairObj = NULL ;
    if( GetProtectionPairObjBySrcDevName(srcDevName,  &protectionPairObj) )
    {
        ConfSchema::Attrs_t& protectionPairAttrs = protectionPairObj->m_Attrs ;
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetPendingChangesSizeAttrName());
        attrIter->second = boost::lexical_cast<std::string>( pendingChangesInBytes ) ;
        attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetCurrentRPOTimeAttrName()) ;
        attrIter->second = boost::lexical_cast<std::string>( rpo ) ;
		protectionPairAttrs.insert(std::make_pair(m_proPairAttrNamesObj.GetIsValidRPOAttrName(), "1")) ;
		attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetIsValidRPOAttrName()) ;
		attrIter ->second = "1" ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

bool ProtectionPairConfig::IsResyncReqdAfterResynStartTagTime( SV_ULONGLONG resyncStartTagTime,
                                                              const std::string& resyncReqdMsg,
                                                              SV_ULONGLONG& resyncReqdStamp, 
                                                              bool& bInNano )
{
    bool bRet = true ;
    bInNano = true ;
    size_t idx1, idx2 ; 

    idx1 = resyncReqdMsg.find("&timestamp=") + strlen("&timestamp=") ;
    idx2 = resyncReqdMsg.find("&err") ;
	resyncReqdStamp = 0 ;
    if( idx1 != std::string::npos && idx2 != std::string::npos )
    {
        resyncReqdMsg.substr( idx1, idx2 - idx1) ;
        resyncReqdStamp = boost::lexical_cast<SV_ULONGLONG>(resyncReqdMsg.substr( idx1, idx2 - idx1)) ;
        if( resyncReqdStamp <= resyncStartTagTime )
        {
            bRet = false ;
        }
        DebugPrintf(SV_LOG_ERROR, "Resync Required TimeStamp " ULLSPEC ". Resync Start TimeStamp " ULLSPEC  ". This is not error", resyncReqdStamp, 
            resyncStartTagTime) ;
    }
    else
    {
        resyncReqdStamp = time(NULL) ;
        bInNano = false ;
    }
    return bRet ;
}

void ProtectionPairConfig::SetResyncRequired(const std::string& devName,
                                             const std::string& errMessage,
                                             int type) 
{
    struct tm * timeinfo;
    char buffer [80];
    bool bproceed = false ;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    DebugPrintf(SV_LOG_DEBUG, "Src Dev : %s\n", 
        devName.c_str()) ;
    ConfSchema::Object* protectionPairObj = NULL ;
    if( type == 2 )
    {
        bproceed = GetProtectionPairObjBySrcDevName(devName, &protectionPairObj) ;
    }
    else if( type == 1 )
    {
        bproceed = GetProtectionPairObjByTgtDevName(devName, &protectionPairObj) ;
    }
    if( bproceed )
    {
        ConfSchema::Attrs_t& protectionPairAttrs = protectionPairObj->m_Attrs ;
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetResyncStartTagTimeAttrName()) ;

        SV_ULONGLONG resyncStartTagTime = boost::lexical_cast<SV_ULONGLONG>(attrIter->second) ;
        SV_ULONGLONG resyncRequiredTs ;
        bool bInNano ; 
        if( IsResyncReqdAfterResynStartTagTime( resyncStartTagTime, errMessage, resyncRequiredTs, bInNano ) )
        {
            attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetShouldResyncAttrName()) ;
            attrIter->second = "1" ;
            attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetShouldResyncSetFromAttrName()) ;
            attrIter->second = boost::lexical_cast<std::string>( type ) ;
            std::string resyncRequiredMessage = "Resync Required is set for the volume " ;
            if( type == 1 )
            {
                attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetResyncSetCXtimeStampAttrName()) ;
            }
            else
            {
                attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetResyncSetTimeStampAttrName()) ;
            }
			if( resyncRequiredTs != 0 )
			{
				attrIter->second = boost::lexical_cast<std::string>(resyncRequiredTs) ;
			}
            attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetSrcNameAttrName()) ;
            resyncRequiredMessage += attrIter->second ;
           
            AlertConfigPtr alertConfigPtr = AlertConfig::GetInstance() ;
            alertConfigPtr->AddAlert("CRITICAL", "CX", SV_ALERT_SIMPLE, SV_ALERT_MODULE_RESYNC, resyncRequiredMessage) ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "The Resync Required Time Stamp is less than the resync Start Tagtime. Not marking the resync required. This is not error. \n") ; 
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void ProtectionPairConfig::SetResyncRequiredBySource(const std::string& srcDevName,
                                                     const std::string& errMessage) 
{
    SetResyncRequired(srcDevName, errMessage, 2) ;
    AuditConfigPtr auditConfigPtr = AuditConfig::GetInstance() ;
    std::string auditmsg ;
    auditmsg = "Resync Required for " + srcDevName + " from source";
    auditConfigPtr->LogAudit(auditmsg) ;

}

void ProtectionPairConfig::SetResyncRequiredByTarget(const std::string& srcDevName,
                                                     const std::string& errMessage) 
{
    SetResyncRequired(srcDevName, errMessage, 1) ;
    AuditConfigPtr auditConfigPtr = AuditConfig::GetInstance() ;
    std::string auditmsg ;
    auditmsg = "Resync Required for " + srcDevName + " from target";
    auditConfigPtr->LogAudit(auditmsg) ;
}


void ProtectionPairConfig::UpdateReplState( const std::string& srcDevName, 
                                           VOLUME_SETTINGS::SYNC_MODE syncMode)
{
    ConfSchema::Object* protectionPairObj = NULL ;
    if( GetProtectionPairObj(srcDevName, "",&protectionPairObj) )
    {
        ConfSchema::Attrs_t& protectionPairAttrs = protectionPairObj->m_Attrs ;
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetReplStateAttrName()) ;
        attrIter->second = boost::lexical_cast<std::string>(syncMode) ;
    }
}

void ProtectionPairConfig::UpdatePairState( const std::string& srcDevName, 
                                           VOLUME_SETTINGS::PAIR_STATE pairState)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ConfSchema::Object* protectionPairObj = NULL ;
    if( GetProtectionPairObj(srcDevName, "",&protectionPairObj) )
    {
        ConfSchema::Attrs_t& protectionPairAttrs = protectionPairObj->m_Attrs ;
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetPairStateAttrName()) ;
        attrIter->second = boost::lexical_cast<std::string>(pairState) ;
    }
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
void ProtectionPairConfig::UpdateQuasiEndState(const std::string& tgtDevName)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    DebugPrintf(SV_LOG_DEBUG, "Tgt Dev : %s\n", 
        tgtDevName.c_str()) ;
    ConfSchema::Object* protectionPairObj = NULL ;
    if( GetProtectionPairObjByTgtDevName(tgtDevName, &protectionPairObj) )
    {
        ConfSchema::Attrs_t& protectionPairAttrs = protectionPairObj->m_Attrs ;
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetReplStateAttrName()) ;
        attrIter->second = boost::lexical_cast<std::string>(VOLUME_SETTINGS::SYNC_DIFF) ;
        AuditConfigPtr auditConfigPtr = AuditConfig::GetInstance() ;
        std::string auditmsg ;

        auditmsg = "Initial/resync step 2 for " + tgtDevName + " is completed.";
        auditConfigPtr->LogAudit(auditmsg) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void ProtectionPairConfig::UpdateVolumeFreeSpace(const std::string& volumeName,
                                                 SV_LONGLONG freeSpace)
{
    VolumeConfigPtr volumeConfigPtr = VolumeConfig::GetInstance() ;
    volumeConfigPtr->UpdateVolumeFreeSpace(volumeName, freeSpace) ;
}

void ProtectionPairConfig::UpdateCurrentLogSize(ConfSchema::Object* protectionPairObj,
                                                SV_ULONGLONG diskSpace)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ConfSchema::Object* retPolObj ;
    if( GetRetentionLogPolicyObject(protectionPairObj, &retPolObj) )
    {
        ConfSchema::Attrs_t& retLogObjAttrs = retPolObj->m_Attrs ;
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = retLogObjAttrs.find(m_retLogPolicyAttrNamesObj.GetCurrLogSizeAttrName()) ;
        attrIter->second = boost::lexical_cast<std::string>(diskSpace) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

SV_ULONGLONG ProtectionPairConfig::GetCDPStoreSizeByProtectedVolumes (std::list<std::string> &protectedVolumes )
{
    SV_ULONGLONG totalCDPSize = 0;
    std::list <std::string>::iterator protectedVolumesIter = protectedVolumes.begin();
    while (protectedVolumesIter != protectedVolumes.end () )
    {
        SV_ULONGLONG logSize = 0 ;
        GetCurrentLogSizeBySrcDevName (*protectedVolumesIter,logSize);
        totalCDPSize += logSize ;
        protectedVolumesIter ++;
    }
    return totalCDPSize;
}
void ProtectionPairConfig::GetCurrentLogSizeBySrcDevName(const std::string& srcDevName,
                                                         SV_ULONGLONG& logSize,
                                                         const std::string& srcHostId,
                                                         const std::string& tgtHostId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    logSize = 0 ;
    ConfSchema::Object* protectionPairObj ;
    if( GetProtectionPairObjBySrcDevName(srcDevName, &protectionPairObj) )
    {
        ConfSchema::Object* retPolObj ;
        if( GetRetentionLogPolicyObject(protectionPairObj, &retPolObj) )
        {
            ConfSchema::Attrs_t& retLogObjAttrs = retPolObj->m_Attrs ;
            ConfSchema::AttrsIter_t attrIter ;
            attrIter = retLogObjAttrs.find(m_retLogPolicyAttrNamesObj.GetCurrLogSizeAttrName()) ;
            logSize = boost::lexical_cast<SV_ULONGLONG>(attrIter->second) ;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void ProtectionPairConfig::UpdateCDPSummary(const std::string& tgtDevName,
                                            const ParameterGroup & cdpSummaryPg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    DebugPrintf(SV_LOG_DEBUG, "Tgt Dev : %s\n", 
        tgtDevName.c_str()) ;
    ConfSchema::Object* protectionPairObj = NULL ;
    if( GetProtectionPairObjByTgtDevName(tgtDevName, &protectionPairObj) )
    {
        const std::string& logsFromUtc = cdpSummaryPg.m_Params.find(NS_CDPSummary::start_ts)->second.value ;
        const std::string& logsToUtc   = cdpSummaryPg.m_Params.find(NS_CDPSummary::end_ts)->second.value ;
        std::string logsFromUtcAsString,  logsToUtcAsString ;
        getTimeinDisplayFormat(boost::lexical_cast<SV_LONGLONG>(logsFromUtc), logsFromUtcAsString) ;
        getTimeinDisplayFormat(boost::lexical_cast<SV_LONGLONG>(logsToUtc), logsToUtcAsString) ;
        ConfSchema::Attrs_t& protectionPairAttrs = protectionPairObj->m_Attrs ;
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = protectionPairAttrs.find( m_proPairAttrNamesObj.GetLogsFromUTCAttrName()) ;
        attrIter->second = logsFromUtc ;

        attrIter = protectionPairAttrs.find( m_proPairAttrNamesObj.GetLogsTOUTCAttrName()) ;
        attrIter->second = logsToUtc ;

        attrIter = protectionPairAttrs.find( m_proPairAttrNamesObj.GetLogsFromAttrName()) ;
        attrIter->second = logsFromUtcAsString ;

        attrIter = protectionPairAttrs.find( m_proPairAttrNamesObj.GetLogsToAttrName()) ;
        attrIter->second = logsToUtcAsString ;
        //UpdateCurrentLogSize(protectionPairObj, cdpSummaryPg) ;

        SV_LONGLONG freeSpace = boost::lexical_cast<SV_LONGLONG>(cdpSummaryPg.m_Params.find(NS_CDPSummary::free_space)->second.value);
        UpdateVolumeFreeSpace(tgtDevName, freeSpace) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void ProtectionPairConfig::GetRetentionWindowDetailsByTgtDevName(const std::string& tgtDevName,
                                                                 ParameterGroup& pg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    DebugPrintf(SV_LOG_DEBUG, "Tgt Dev : %s\n", 
        tgtDevName.c_str()) ;
    ConfSchema::Object* protectionPairObj = NULL ;
    if( GetProtectionPairObjByTgtDevName(tgtDevName, &protectionPairObj) )
    {
        ConfSchema::Attrs_t& protectionPairAttrs = protectionPairObj->m_Attrs ;
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = protectionPairAttrs.find( m_proPairAttrNamesObj.GetLogsFromAttrName() ) ;
        pg.m_Params.insert(std::make_pair(NS_RetentionWindow::m_startTime, ValueType("unsigned int", attrIter->second)));
        attrIter = protectionPairAttrs.find( m_proPairAttrNamesObj.GetLogsFromUTCAttrName() ) ;
        pg.m_Params.insert(std::make_pair(NS_RetentionWindow::m_startTime, ValueType("unsigned int", attrIter->second)));
        attrIter = protectionPairAttrs.find( m_proPairAttrNamesObj.GetLogsToAttrName() ) ;
        pg.m_Params.insert(std::make_pair(NS_RetentionWindow::m_startTime, ValueType("unsigned int", attrIter->second)));
        attrIter = protectionPairAttrs.find( m_proPairAttrNamesObj.GetLogsTOUTCAttrName() ) ;
        pg.m_Params.insert(std::make_pair(NS_RetentionWindow::m_startTime, ValueType("unsigned int", attrIter->second)));

    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}


void ProtectionPairConfig::UpdateCleanUpAction(const std::string& tgtDevName,
                                               const std::string& cleanupAction)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    DebugPrintf(SV_LOG_DEBUG, "Tgt Dev : %s\n", 
        tgtDevName.c_str()) ;
    ConfSchema::Object* protectionPairObj = NULL ;
    if( GetProtectionPairObjByTgtDevName(tgtDevName, &protectionPairObj) )
    {
        ConfSchema::Attrs_t& protectionPairAttrs = protectionPairObj->m_Attrs ;
        ConfSchema::AttrsIter_t attrIter ;
		attrIter = protectionPairAttrs.find( m_proPairAttrNamesObj.GetReplicationCleanupOptionAttrName() ) ;
		std::string cleanUpOptions = attrIter->second ;
		if( !cleanUpOptions.empty() )
		{
			attrIter = protectionPairAttrs.find( m_proPairAttrNamesObj.GetPairStateAttrName()) ;
			if( attrIter->second == boost::lexical_cast<std::string>(VOLUME_SETTINGS::DELETE_PENDING) || 
				attrIter->second == boost::lexical_cast<std::string>(VOLUME_SETTINGS::CLEANUP_DONE ) )
			{
				GetCleanUpOptionsString(cleanupAction, cleanUpOptions) ;
				attrIter = protectionPairAttrs.find( m_proPairAttrNamesObj.GetReplicationCleanupOptionAttrName() ) ;
        		attrIter->second =  cleanUpOptions ;
				attrIter = protectionPairAttrs.find( m_proPairAttrNamesObj.GetCleanupActionAttrName() ) ;
				attrIter->second = cleanupAction ;
				attrIter = protectionPairAttrs.find( m_proPairAttrNamesObj.GetPairStateAttrName() ) ;
				std::string tempStr = cleanUpOptions.substr(RETENTIONLOGPOS, 2) ;
				if(tempStr == "01")
				{
					attrIter->second = boost::lexical_cast<std::string>(VOLUME_SETTINGS::CLEANUP_DONE) ;
					AuditConfigPtr auditconfigPtr = AuditConfig::GetInstance() ;
					std::string auditmsg ;
					auditmsg = "Cleanup done for the " + tgtDevName ;
					auditconfigPtr->LogAudit(auditmsg) ;
				}
			}
			else
			{
				DebugPrintf(SV_LOG_ERROR, "Cleanup options were not empty even when the pair state is not marked for deletion.. Resetting the cleanup action string\n") ;
				attrIter = protectionPairAttrs.find( m_proPairAttrNamesObj.GetCleanupActionAttrName()) ;
				attrIter->second = "" ;
				attrIter = protectionPairAttrs.find( m_proPairAttrNamesObj.GetReplicationCleanupOptionAttrName()) ;
				attrIter->second = "" ;
			}
		}
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void ProtectionPairConfig::ResumeProtectionByTgtVolume(const std::string& tgtdevname)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool bRet = false ;
	ConfSchema::Object* protectionPairObj = NULL ;
	bRet = GetProtectionPairObjByTgtDevName( tgtdevname, &protectionPairObj ) ;
	if( bRet )
	{
		ConfSchema::AttrsIter_t attrIter ;
		attrIter = protectionPairObj->m_Attrs.find( m_proPairAttrNamesObj.GetPairStateAttrName() ) ;
		attrIter->second = boost::lexical_cast<std::string>(VOLUME_SETTINGS::PAIR_PROGRESS) ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
void ProtectionPairConfig::PauseProtectionByTgtVolume(const std::string& tgtdevname)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool bRet = false ;
	ConfSchema::Object* protectionPairObj = NULL ;
	bRet = GetProtectionPairObjByTgtDevName( tgtdevname, &protectionPairObj ) ;
	if( bRet )
	{
		ConfSchema::AttrsIter_t attrIter ;
		attrIter = protectionPairObj->m_Attrs.find( m_proPairAttrNamesObj.GetPairStateAttrName() ) ;
		attrIter->second = boost::lexical_cast<std::string>(VOLUME_SETTINGS::PAUSE) ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void ProtectionPairConfig::PauseProtection(CDP_DIRECTION direction,
										   const std::string& device)
{
	bool bRet ;
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	ConfSchema::Object* protectionPairObj = NULL ;
	if( TARGET == direction )
	{
		bRet = GetProtectionPairObjBySrcDevName( device, &protectionPairObj ) ;
	}
	else
	{
		bRet = GetProtectionPairObjByTgtDevName( device, &protectionPairObj ) ;
	}
	if( bRet )
	{
		ConfSchema::AttrsIter_t attrIter ;
		attrIter = protectionPairObj->m_Attrs.find( m_proPairAttrNamesObj.GetPairStateAttrName() ) ;
		if( boost::lexical_cast<int>(attrIter->second) == VOLUME_SETTINGS::PAUSE_PENDING )
		{
			attrIter->second = boost::lexical_cast<std::string>(VOLUME_SETTINGS::PAUSE) ;
		}
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void ProtectionPairConfig::PauseTrack(CDP_DIRECTION direction,
									 const std::string& device)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	ConfSchema::Object* protectionPairObj = NULL ;
	bool bRet ;
	if( TARGET == direction )
	{
		bRet = GetProtectionPairObjBySrcDevName( device, &protectionPairObj ) ;
	}
	else
	{
		bRet = GetProtectionPairObjByTgtDevName( device, &protectionPairObj ) ;
	}
	if( bRet )
	{
		ConfSchema::AttrsIter_t attrIter ;
		attrIter = protectionPairObj->m_Attrs.find( m_proPairAttrNamesObj.GetPairStateAttrName() ) ;
		if( boost::lexical_cast<int>(attrIter->second) == VOLUME_SETTINGS::PAUSE_TACK_PENDING )
		{
			attrIter->second = boost::lexical_cast<std::string>(VOLUME_SETTINGS::PAUSE_TACK_COMPLETED) ;
		}
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void ProtectionPairConfig::UpdatePauseReplicationStatus( const std::string& devName,
                                                        int hostType,
                                                        const std::string& pauseString )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::string deviceName ;
    DebugPrintf(SV_LOG_DEBUG, "Tgt Dev : %s\n", 
        devName.c_str()) ;
    ConfSchema::Object* protectionPairObj = NULL ;
    bool bRet = false ;
    int bitmapIndex = -1 ;
    if(HOSTSOURCE == hostType) 
    {
        bRet = GetProtectionPairObjBySrcDevName( devName, &protectionPairObj ) ;
        bitmapIndex = 0 ;
    }
    else if (HOSTTARGET == hostType) 
    {
        bRet = GetProtectionPairObjByTgtDevName( devName, &protectionPairObj ) ;
        bitmapIndex = 1 ;
    }
    if( bRet )
    {
        ConfSchema::Attrs_t& protectionPairAttrs = protectionPairObj->m_Attrs ;
        ConfSchema::AttrsIter_t pairStateAttrIter, maintenanceBitMapAttrIter ;
        pairStateAttrIter = protectionPairAttrs.find( m_proPairAttrNamesObj.GetPairStateAttrName() ) ;
        maintenanceBitMapAttrIter = protectionPairAttrs.find( m_proPairAttrNamesObj.GetMaintenanceActionAttrName() ) ;
        if( pauseString.find("pause_components_status=completed") != std::string::npos || 
            pauseString.find("pause_tracking_status=completed") != std::string::npos ) 
        {
            if( maintenanceBitMapAttrIter->second.length() > bitmapIndex ) 
                maintenanceBitMapAttrIter->second[bitmapIndex] = '0' ;
            else
                DebugPrintf(SV_LOG_ERROR, "This should not be here in the first place: %s, \n", maintenanceBitMapAttrIter->second.c_str() ) ;
        }
        if( maintenanceBitMapAttrIter->second.compare("00") == 0 )
        {
            std::string auditmsg ;
            AuditConfigPtr auditConfigPtr = AuditConfig::GetInstance() ;
            auditmsg = "Volume " + devName ;
            switch ( boost::lexical_cast<int>( pairStateAttrIter->second ) )
            {
            case VOLUME_SETTINGS::PAUSE_PENDING :
                pairStateAttrIter->second = boost::lexical_cast<std::string>(VOLUME_SETTINGS::PAUSE) ;
                auditmsg += " paused." ;
                break ;
            case VOLUME_SETTINGS::PAUSE_TACK_PENDING :
                pairStateAttrIter->second = boost::lexical_cast<std::string>(VOLUME_SETTINGS::PAUSE_TACK_COMPLETED) ;
                auditmsg += " pause tracked." ;
                break ;
            }
            auditConfigPtr->LogAudit(auditmsg) ;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void ProtectionPairConfig::GetPairState( ConfSchema::Object* protectionPairObj, VOLUME_SETTINGS::PAIR_STATE& pairState )
{
    ConfSchema::Attrs_t& protectionPairAttrs = protectionPairObj->m_Attrs ;
    ConfSchema::AttrsIter_t attrIter ;
    attrIter = protectionPairAttrs.find( m_proPairAttrNamesObj.GetPairStateAttrName()) ;
    pairState = (VOLUME_SETTINGS::PAIR_STATE) boost::lexical_cast<int>( attrIter->second ) ;
    return ;
}

bool ProtectionPairConfig::GetPairState( const std::string& srcDevName,
                                        VOLUME_SETTINGS::PAIR_STATE& pairState,
                                        const std::string& tgtDevName,
                                        const std::string& sourceHostId,
                                        const std::string& tgtHostId)
{
    DebugPrintf(SV_LOG_DEBUG, "Src Dev : %s, Tgt Dev : %s\n", 
        srcDevName.c_str(), tgtDevName.c_str()) ;
    bool bRet = false ;
    ConfSchema::Object* protectionPairObj = NULL ;
    if( !srcDevName.empty() && !tgtDevName.empty() )
    {
        bRet = GetProtectionPairObj( srcDevName, tgtDevName, &protectionPairObj ) ;
    }
    else if( !srcDevName.empty() )
    {
        bRet = GetProtectionPairObjBySrcDevName( srcDevName, &protectionPairObj ) ;
    }
    else if( !tgtDevName.empty() )
    {
        bRet = GetProtectionPairObjByTgtDevName( tgtDevName, &protectionPairObj ) ;
    }
    if( bRet )
    {
        GetPairState( protectionPairObj, pairState ) ;            
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool ProtectionPairConfig::IsPauseAllowed(const std::string& srcDevName,
                                          const std::string& tgtDevName,
                                          const std::string& srcHostId,
                                          const std::string& tgtHostId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    bool bRet = false ;

    ConfSchema::Object* protectionPairObj = NULL ;
    if( GetProtectionPairObjBySrcDevName( srcDevName, &protectionPairObj ) ) 
    {
		bRet = true ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}
bool ProtectionPairConfig::PauseTracking(const std::string& srcDevName,
                                         bool directPause,
                                         const std::string& tgtDevName,
                                         const std::string& srcHostId,
                                         const std::string& tgtHostId)
{
    DebugPrintf(SV_LOG_DEBUG, "Src Dev : %s, Tgt Dev : %s\n", 
        srcDevName.c_str(), tgtDevName.c_str()) ;
    bool bRet = false ;
    ConfSchema::Object* protectionPairObj = NULL ;
    if( !srcDevName.empty() && !tgtDevName.empty() )
    {
        bRet = GetProtectionPairObj( srcDevName, tgtDevName, &protectionPairObj ) ;
    }
    else if( !srcDevName.empty() )
    {
        bRet = GetProtectionPairObjBySrcDevName( srcDevName, &protectionPairObj ) ;
    }
    else if( !tgtDevName.empty() )
    {
        bRet = GetProtectionPairObjByTgtDevName( tgtDevName, &protectionPairObj ) ;
    }
    if( bRet )
    {
        ConfSchema::Attrs_t& protectionPairAttrs = protectionPairObj->m_Attrs ;
        ConfSchema::AttrsIter_t attrIter ;
        std::string modeStr ;
        if( !directPause )
        {
            attrIter  = protectionPairAttrs.find(m_proPairAttrNamesObj.GetPairStateAttrName()) ;
            attrIter->second = boost::lexical_cast<std::string>(VOLUME_SETTINGS::PAUSE_TACK_PENDING) ;
            attrIter  = protectionPairAttrs.find(m_proPairAttrNamesObj.GetPauseReasonAttrName()) ;
            attrIter->second = "Pause By User" ;
            /*attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetMaintenanceActionAttrName()) ;
            attrIter->second = "11" ; //Only target does maintenance action for pause.
			*/
			attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetMaintenanceActionAttrName()) ;
			attrIter ->second = "" ;
            modeStr = "directly" ;

        }
        else
        {
            attrIter  = protectionPairAttrs.find(m_proPairAttrNamesObj.GetPairStateAttrName()) ;
            attrIter->second = boost::lexical_cast<std::string>(VOLUME_SETTINGS::PAUSE_TACK_COMPLETED) ;
            modeStr = "indirectly" ;
        }
        AuditConfigPtr auditConfigPtr = AuditConfig::GetInstance() ;
        std::string auditmsg ;
        auditmsg = "Pause tracking the changes to" + srcDevName + " "  + modeStr ;
        auditConfigPtr->LogAudit(auditmsg) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool ProtectionPairConfig::PerformPendingResumeTracking()
{
    ConfSchema::ObjectsIter_t protPairObjIter  = m_ProtectionPairGrp->m_Objects.begin() ;
	SV_UINT batchSize ;
	bool bRet = false ;
	ScenarioConfigPtr scenarioConfigPtr = ScenarioConfig::GetInstance() ;
	batchSize = scenarioConfigPtr->GetBatchSize() ;
    while( protPairObjIter != m_ProtectionPairGrp->m_Objects.end() )
    {
        ConfSchema::Object* protectionPairObj = &(protPairObjIter->second) ;
		ConfSchema::AttrsIter_t attrIter = protectionPairObj->m_Attrs.find( m_proPairAttrNamesObj.GetResumeTrackingByService()) ;
		if( attrIter != protectionPairObj->m_Attrs.end() )
		{
			if( attrIter->second.compare("1") == 0 )
			{
				attrIter = protectionPairObj->m_Attrs.find( m_proPairAttrNamesObj.GetSrcNameAttrName() ) ;
				ResumeTracking( attrIter->second, batchSize, false) ;
				attrIter = protectionPairObj->m_Attrs.find( m_proPairAttrNamesObj.GetResumeTrackingByService()) ;
				attrIter->second = "0" ;
				bRet = true ; 
			}
		}
        protPairObjIter++ ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return bRet ;
}

bool ProtectionPairConfig::ResumeTracking( const std::string& srcDevName,
                                          int batchSize,
										  bool isRescueMode,
                                          const std::string& tgtDevName,
                                          const std::string& srcHostId,
                                          const std::string& tgtHostId)
{
    DebugPrintf(SV_LOG_DEBUG, "Src Dev : %s, Tgt Dev : %s\n", 
        srcDevName.c_str(), tgtDevName.c_str()) ;
    bool bRet = false ;
    ConfSchema::Object* protectionPairObj = NULL ;
    if( !srcDevName.empty() && !tgtDevName.empty() )
    {
        bRet = GetProtectionPairObj( srcDevName, tgtDevName, &protectionPairObj ) ;
    }
    else if( !srcDevName.empty() )
    {
        bRet = GetProtectionPairObjBySrcDevName( srcDevName, &protectionPairObj ) ;
    }
    else if( !tgtDevName.empty() )
    {
        bRet = GetProtectionPairObjByTgtDevName( tgtDevName, &protectionPairObj ) ;
    }
    if( bRet )
    {
		std::string auditmsg ;
		AuditConfigPtr auditConfigPtr = AuditConfig::GetInstance() ;
		auditmsg = "Resuming tracking of I/O changes for " + srcDevName + ". " ;
		EnableAutoResync( *protectionPairObj ) ;
		if( isRescueMode )
		{
			ConfSchema::AttrsIter_t attrIter ;
			protectionPairObj->m_Attrs.insert( std::make_pair( m_proPairAttrNamesObj.GetResumeTrackingByService(), "0" )) ;
			attrIter = protectionPairObj->m_Attrs.find( m_proPairAttrNamesObj.GetResumeTrackingByService() ) ;
			attrIter->second = "1" ;
			auditmsg = "Tracking of I/O changes would be resumed when the server booted normally for " ;
			auditmsg += srcDevName ;
			auditmsg += ". " ;

		}
		else
		{
			bool Queued = ShouldQueued(protectionPairObj, batchSize) ;
			RestartResyncForPair( protectionPairObj, Queued ) ;
			if( Queued )
			{
				ConfSchema::AttrsIter_t attrIter ;
				attrIter = protectionPairObj->m_Attrs.find( m_proPairAttrNamesObj.GetPairStateAttrName()) ;
				attrIter->second = boost::lexical_cast<std::string>( VOLUME_SETTINGS::RESUME_PENDING ) ;
				auditmsg += "It's currently in Resume Pending state" ;
			}
		}
		auditConfigPtr->LogAudit(auditmsg) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool ProtectionPairConfig::ResumeProtection()
{
	ConfSchema::ObjectsIter_t protectionpairobjiter = m_ProtectionPairGrp->m_Objects.begin() ;
	while( protectionpairobjiter != m_ProtectionPairGrp->m_Objects.end() )
	{
		ConfSchema::Attrs_t& protectionPairAttrs = protectionpairobjiter->second.m_Attrs ;
        ConfSchema::AttrsIter_t attrIter ;
		attrIter = protectionPairAttrs.find( m_proPairAttrNamesObj.GetVolumeResizedAttrName() ) ;
        eVolumeResize resizeStatus ;
        resizeStatus = (eVolumeResize) boost::lexical_cast<int>(attrIter->second) ;
        if( resizeStatus == VOLUME_SIZE_NORMAL )
        {
			attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetPairStateAttrName());
			attrIter->second = boost::lexical_cast<std::string>(VOLUME_SETTINGS::PAIR_PROGRESS);   
			attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetPauseReasonAttrName());
			attrIter->second = "";
			attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetPauseExtendedReasonAttrName());
			if( attrIter != protectionPairAttrs.end() )
			{
				attrIter->second = "" ;
			}
		}
		protectionpairobjiter++ ;
	}
	return true ;
}

bool ProtectionPairConfig::ResumeProtection( const std::string& srcDevName,
                                            const std::string& tgtDevName,
                                            const std::string& srcHostId,
                                            const std::string& tgtHostId)
{
    DebugPrintf(SV_LOG_DEBUG, "Src Dev : %s, Tgt Dev : %s\n", 
        srcDevName.c_str(), tgtDevName.c_str()) ;
    bool bRet = false ;
    ConfSchema::Object* protectionPairObj = NULL ;
    if( !srcDevName.empty() && !tgtDevName.empty() )
    {
        bRet = GetProtectionPairObj( srcDevName, tgtDevName, &protectionPairObj ) ;
    }
    else if( !srcDevName.empty() )
    {
        bRet = GetProtectionPairObjBySrcDevName( srcDevName, &protectionPairObj ) ;
    }
    else if( !tgtDevName.empty() )
    {
        bRet = GetProtectionPairObjByTgtDevName( tgtDevName, &protectionPairObj ) ;
    }
    if( bRet )
    {
        ConfSchema::Attrs_t& protectionPairAttrs = protectionPairObj->m_Attrs ;
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetPairStateAttrName());
        attrIter->second = boost::lexical_cast<std::string>(VOLUME_SETTINGS::PAIR_PROGRESS);   
        attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetPauseReasonAttrName());
        attrIter->second = "";
		attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetPauseExtendedReasonAttrName());
		if( attrIter != protectionPairAttrs.end() )
		{
			attrIter->second = "" ;
		}
        AuditConfigPtr auditConfigPtr = AuditConfig::GetInstance() ;
        std::string auditmsg ;
        auditmsg = "Resuming protection for " + srcDevName + ". " ;
        auditConfigPtr->LogAudit(auditmsg) ;

    }
    return bRet ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}


bool ProtectionPairConfig::PauseProtection(const std::string& srcDevName,
                                           bool directPause,
										   const std::string& pauseReason, 
										   const std::string& tgtDevName,
                                           const std::string& srcHostId,
                                           const std::string& tgtHostId)
{
    DebugPrintf(SV_LOG_DEBUG, "Src Dev : %s, Tgt Dev : %s\n", 
        srcDevName.c_str(), tgtDevName.c_str()) ;
    bool bRet = false ;
    ConfSchema::Object* protectionPairObj = NULL ;
    if( !srcDevName.empty() && !tgtDevName.empty() )
    {
        bRet = GetProtectionPairObj( srcDevName, tgtDevName, &protectionPairObj ) ;
    }
    else if( !srcDevName.empty() )
    {
        bRet = GetProtectionPairObjBySrcDevName( srcDevName, &protectionPairObj ) ;
    }
    else if( !tgtDevName.empty() )
    {
        bRet = GetProtectionPairObjByTgtDevName( tgtDevName, &protectionPairObj ) ;
    }
    if( bRet )
    {
		
        ConfSchema::Attrs_t& protectionPairAttrs = protectionPairObj->m_Attrs ;
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetPairStateAttrName());
		std::string auditmsg ;
        if( directPause )
		{
			attrIter->second = boost::lexical_cast<std::string>(VOLUME_SETTINGS::PAUSE);   
			auditmsg = "Pausing protection for " + srcDevName + ". " ;
        }
		else
		{
			if(attrIter->second != boost::lexical_cast<std::string>(VOLUME_SETTINGS::PAUSE))
			{
				attrIter->second = boost::lexical_cast<std::string>(VOLUME_SETTINGS::PAUSE_PENDING);   
				attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetPauseReasonAttrName());
				attrIter->second = "Pause By User";   
				attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetMaintenanceActionAttrName()) ;
				attrIter->second = "" ; //Only target does maintenance action for pause.
				auditmsg = "Marking protection for pause for " + srcDevName + ". " ;
			}
        }
        AuditConfigPtr auditConfigPtr = AuditConfig::GetInstance() ;
        auditConfigPtr->LogAudit(auditmsg) ;
		if( !pauseReason.empty() )
		{
			protectionPairAttrs.insert(std::make_pair( m_proPairAttrNamesObj.GetPauseExtendedReasonAttrName(), "")) ;
			if (! IsPairMarkedAsResized (srcDevName))
			{
				attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetPauseExtendedReasonAttrName());
				attrIter->second = pauseReason ;
			}
		}
		attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetCleanupActionAttrName()) ;
		if( !attrIter->second.empty() )
		{
			auditmsg = "Clearing the cleanupaction string from " + attrIter->second ;
			attrIter->second = "" ;
			auditConfigPtr->LogAudit(auditmsg) ;
		}
		attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetReplicationCleanupOptionAttrName()) ;
		if( !attrIter->second.empty() )
		{
			auditmsg = "Clearing the replication cleanup action string from " + attrIter->second ;
			attrIter->second = "" ;
			auditConfigPtr->LogAudit(auditmsg) ;
		}
    }
    return bRet ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

bool ProtectionPairConfig::IsResumeAllowed(const std::string& srcDevName,
                                           const std::string& tgtDevName,
                                           const std::string& srcHostId,
                                           const std::string& tgtHostId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    bool bRet = false ;
    VOLUME_SETTINGS::PAIR_STATE pairState ;
    if( GetPairState( srcDevName, pairState, tgtDevName, srcHostId, tgtHostId ) )
    {
        if( ! IsPairMarkedAsResized (srcDevName) )
		{
			bRet = true ;
		}
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool ProtectionPairConfig::IsPauseTrackingAllowed(const std::string& srcDevName,
                                                  const std::string& tgtDevName,
                                                  const std::string& srcHostId,
                                                  const std::string& tgtHostId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    bool bRet = false ;
    ConfSchema::Object* protectionPairObj = NULL ;
    if( GetProtectionPairObjBySrcDevName( srcDevName, &protectionPairObj ) ) 
    {
		bRet = true ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}


bool ProtectionPairConfig::IsResumeTrackingAllowed(const std::string& srcDevName,
                                                   const std::string& tgtDevName,
                                                   const std::string& srcHostId,
                                                   const std::string& tgtHostId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    bool bRet = false ;
    VOLUME_SETTINGS::PAIR_STATE pairState ;
    if( GetPairState( srcDevName, pairState, tgtDevName, srcHostId, tgtHostId ) )
    {
		bRet = true ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

void ProtectionPairConfig::GetCleanUpOptionsString( const std::string & cleanUpActionStr,
                                                   std::string &cleanUpOptions )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    DebugPrintf(SV_LOG_DEBUG,"CleanUpOptions = %s\n",cleanUpOptions.c_str());
    std::string newCleaUpActionStr;
    size_t sz = cleanUpActionStr.find("log_cleanup_status=32");
    if(sz != std::string::npos)
    {
        newCleaUpActionStr = "01";
        cleanUpOptions.replace(RETENTIONLOGPOS,2,newCleaUpActionStr);  
    }
    sz = cleanUpActionStr.find("log_cleanup_status=-32");
    if(sz != std::string::npos)
    {
        newCleaUpActionStr = "02";
        cleanUpOptions.replace(RETENTIONLOGPOS,2,newCleaUpActionStr);
    }
    sz = cleanUpActionStr.find("cache_dir_del_status=36");
    if(sz != std::string::npos)
    {
        newCleaUpActionStr = "01";
        cleanUpOptions.replace(CACHECLEANUPPOS,2,newCleaUpActionStr);  
    }
    sz = cleanUpActionStr.find("cache_dir_del_status=-36");
    if(sz != std::string::npos)
    {
        newCleaUpActionStr = "02";
        cleanUpOptions.replace(CACHECLEANUPPOS,2,newCleaUpActionStr);
    }
    sz = cleanUpActionStr.find("pending_action_folder_cleanup_status=40");
    if(sz != std::string::npos)
    {
        newCleaUpActionStr = "01";
        cleanUpOptions.replace(PENDINGFILESPOS,2,newCleaUpActionStr);  
    }
    sz = cleanUpActionStr.find("pending_action_folder_cleanup_status=-40");
    if(sz != std::string::npos)
    {
        newCleaUpActionStr = "02";
        cleanUpOptions.replace(PENDINGFILESPOS,2,newCleaUpActionStr);
    }
    sz = cleanUpActionStr.find("vsnap_cleanup_status=38");
    if(sz != std::string::npos)
    {
        newCleaUpActionStr = "01";
        cleanUpOptions.replace(VSNAPCLEANUPPOS,2,newCleaUpActionStr);  
    }
    sz = cleanUpActionStr.find("vsnap_cleanup_status=-38");
    if(sz != std::string::npos)
    {
        newCleaUpActionStr = "02";
        cleanUpOptions.replace(VSNAPCLEANUPPOS,2,newCleaUpActionStr);
    }
    sz = cleanUpActionStr.find("unlock_vol_status=34");
    if(sz != std::string::npos)
    {
        newCleaUpActionStr = "01";
        cleanUpOptions.replace(UNLOCKTGTDEVICEPOS,2,newCleaUpActionStr);  
    }
    sz = cleanUpActionStr.find("unlock_vol_status=-34");
    if(sz != std::string::npos)
    {
        newCleaUpActionStr = "02";
        cleanUpOptions.replace(UNLOCKTGTDEVICEPOS,2,newCleaUpActionStr);
    }
    DebugPrintf(SV_LOG_DEBUG,"CleanUpOptions = %s\n",cleanUpOptions.c_str());
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

void ProtectionPairConfig::PopulateProtectionPairAttrs( const PairDetails& pairDetails,
                                                       ConfSchema::Object& protectionPairObj,
                                                       bool Queued)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ConfSchema::Attrs_t& protectionPairAttrs = protectionPairObj.m_Attrs ;
    protectionPairAttrs.insert(std::make_pair(m_proPairAttrNamesObj.GetSrcNameAttrName(),pairDetails.srcVolumeName));
    protectionPairAttrs.insert(std::make_pair(m_proPairAttrNamesObj.GetTgtNameAttrName(),pairDetails.targetVolume));
    protectionPairAttrs.insert(std::make_pair(m_proPairAttrNamesObj.GetAutoResumeAttrName(),""));
    protectionPairAttrs.insert(std::make_pair(m_proPairAttrNamesObj.GetFullSyncBytesSentAttrName(),"0"));
    protectionPairAttrs.insert(std::make_pair(m_proPairAttrNamesObj.GetLastOffsetAttrName(),"0"));
	protectionPairAttrs.insert(std::make_pair(m_proPairAttrNamesObj.GetFsUnusedBytes(),"0"));

    protectionPairAttrs.insert(std::make_pair(m_proPairAttrNamesObj.GetJobIdAttrName(), boost::lexical_cast<std::string>(time(NULL))));
    ACE_OS::sleep(1) ;
    std::string id =  boost::lexical_cast<std::string>(time(NULL)) ;
    protectionPairAttrs.insert(std::make_pair(m_proPairAttrNamesObj.GetIdAttrName(),id));
    std::string valueStr = boost::lexical_cast<std::string>(VOLUME_SETTINGS::SYNC_DIRECT);
    protectionPairAttrs.insert(std::make_pair(m_proPairAttrNamesObj.GetReplStateAttrName(),valueStr));
    protectionPairAttrs.insert(std::make_pair(m_proPairAttrNamesObj.GetShouldResyncAttrName(),"1"));
    protectionPairAttrs.insert(std::make_pair(m_proPairAttrNamesObj.GetShouldResyncSetFromAttrName(),""));
    protectionPairAttrs.insert(std::make_pair(m_proPairAttrNamesObj.GetFastResyncMatchedAttrName(),"0"));
    protectionPairAttrs.insert(std::make_pair(m_proPairAttrNamesObj.GetFastResyncUnMatchedAttrName(),"0"));
    protectionPairAttrs.insert(std::make_pair(m_proPairAttrNamesObj.GetCurrentRPOTimeAttrName(),"0"));
    protectionPairAttrs.insert(std::make_pair(m_proPairAttrNamesObj.GetLogsFromUTCAttrName(),"0"));
    protectionPairAttrs.insert(std::make_pair(m_proPairAttrNamesObj.GetLogsTOUTCAttrName(),"0"));
    protectionPairAttrs.insert(std::make_pair(m_proPairAttrNamesObj.GetLogsFromAttrName(),""));
    protectionPairAttrs.insert(std::make_pair(m_proPairAttrNamesObj.GetLogsToAttrName(),""));
    protectionPairAttrs.insert(std::make_pair(m_proPairAttrNamesObj.GetResyncSetCXtimeStampAttrName(),""));
    protectionPairAttrs.insert(std::make_pair(m_proPairAttrNamesObj.GetResyncStartSeqAttrName(),"0"));
    protectionPairAttrs.insert(std::make_pair(m_proPairAttrNamesObj.GetResyncStartTagTimeAttrName(),"0"));
    protectionPairAttrs.insert(std::make_pair(m_proPairAttrNamesObj.GetResyncStartTime(), boost::lexical_cast<std::string>(time(NULL))));
    protectionPairAttrs.insert(std::make_pair(m_proPairAttrNamesObj.GetVolumeResizedAttrName(), boost::lexical_cast<std::string>(VOLUME_SIZE_NORMAL))) ;
    protectionPairAttrs.insert(std::make_pair(m_proPairAttrNamesObj.GetResizePolicyTSAttrName(), "0")) ;
    protectionPairAttrs.insert(std::make_pair(m_proPairAttrNamesObj.GetSrcVolCapacityAttrName(), boost::lexical_cast<std::string>(pairDetails.srcVolCapacity))) ;
    protectionPairAttrs.insert(std::make_pair(m_proPairAttrNamesObj.GetSrcVolRawSizeAttrName(), boost::lexical_cast<std::string>(pairDetails.srcVolRawSize))) ;
    protectionPairAttrs.insert(std::make_pair(m_proPairAttrNamesObj.GetResyncEndTime(),"0"));
    protectionPairAttrs.insert(std::make_pair(m_proPairAttrNamesObj.GetMarkedForDeletionAttrName(),"0")) ;
    protectionPairAttrs.insert(std::make_pair(m_proPairAttrNamesObj.GetResyncEndTagTimeAttrName(),"0"));
    protectionPairAttrs.insert(std::make_pair(m_proPairAttrNamesObj.GetResyncEndSeqAttrName(),"0"));
    protectionPairAttrs.insert(std::make_pair(m_proPairAttrNamesObj.GetQuasiDiffStarttimeAttrName(),"0"));
    protectionPairAttrs.insert(std::make_pair(m_proPairAttrNamesObj.GetQuasiDiffEndtimeAttrName(),"0"));
    protectionPairAttrs.insert(std::make_pair(m_proPairAttrNamesObj.GetResyncSetTimeStampAttrName(),"0"));
    protectionPairAttrs.insert(std::make_pair(m_proPairAttrNamesObj.GetPendingChangesSizeAttrName(),""));
    std::string executionState;
    protectionPairAttrs.insert(std::make_pair(m_proPairAttrNamesObj.GetAutoResyncStartTimeAttrName(),valueStr));
    valueStr = boost::lexical_cast<std::string>(VOLUME_SETTINGS::PAIR_PROGRESS);
    protectionPairAttrs.insert(std::make_pair(m_proPairAttrNamesObj.GetPairStateAttrName(),valueStr));
    if( !Queued )
    {
        executionState = boost::lexical_cast<std::string>(PAIR_IS_NONQUEUED) ;
    }
    else
    {
        executionState = boost::lexical_cast<std::string>(PAIR_IS_QUEUED) ;
    }
    protectionPairAttrs.insert(std::make_pair(m_proPairAttrNamesObj.GetExecutionStateAttrName(),executionState));
    protectionPairAttrs.insert(std::make_pair(m_proPairAttrNamesObj.GetResyncStartSeqAttrName(),""));
    protectionPairAttrs.insert(std::make_pair(m_proPairAttrNamesObj.GetPauseReasonAttrName(),""));
    protectionPairAttrs.insert(std::make_pair(m_proPairAttrNamesObj.GetRestartResyncCounterAttrName(),"0"));
    protectionPairAttrs.insert(std::make_pair(m_proPairAttrNamesObj.GetCleanupActionAttrName(),""));
    protectionPairAttrs.insert(std::make_pair(m_proPairAttrNamesObj.GetMaintenanceActionAttrName(),""));
    protectionPairAttrs.insert(std::make_pair(m_proPairAttrNamesObj.GetReplicationCleanupOptionAttrName(),""));
	protectionPairAttrs.insert(std::make_pair(m_proPairAttrNamesObj.GetVolumeResizedActionCountAttrName(),"0"));
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void ProtectionPairConfig::RestartResyncForPair( ConfSchema::Object* protectionPairObj, bool bQueued )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    ConfSchema::AttrsIter_t attrIter ;
    ConfSchema::Attrs_t& protectionPairAttrs = protectionPairObj->m_Attrs ;
    if( bQueued )
    {
        attrIter =  protectionPairAttrs.find(m_proPairAttrNamesObj.GetExecutionStateAttrName()) ;
        attrIter->second = boost::lexical_cast<std::string>(PAIR_IS_QUEUED) ;
        attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetShouldResyncAttrName()) ;
        attrIter->second = "1" ;
        attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetShouldResyncSetFromAttrName()) ;
        attrIter->second = boost::lexical_cast<std::string>( 1 ) ;
    }
    else
    {
        attrIter =  protectionPairAttrs.find(m_proPairAttrNamesObj.GetExecutionStateAttrName()) ;
        attrIter->second = boost::lexical_cast<std::string>(PAIR_IS_NONQUEUED) ;
        attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetReplStateAttrName()) ;
        attrIter->second = boost::lexical_cast<std::string>(VOLUME_SETTINGS::SYNC_DIRECT) ;


        attrIter  = protectionPairAttrs.find(m_proPairAttrNamesObj.GetCleanupActionAttrName()) ;
        attrIter->second = "" ;

        attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetCurrentRPOTimeAttrName()) ;
        attrIter->second = "0" ;

        attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetFastResyncMatchedAttrName()) ;
        attrIter->second = "0" ;

        attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetFastResyncUnMatchedAttrName()) ;
        attrIter->second = "0" ;

        attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetFullSyncBytesSentAttrName()) ;
        attrIter->second = "0" ;

        attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetJobIdAttrName()) ;
        ACE_OS::sleep(1)  ;
        attrIter->second = boost::lexical_cast<std::string>(time(NULL));

        attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetLastOffsetAttrName()) ;
        attrIter->second = "0" ;

        attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetLogsFromUTCAttrName()) ;
        attrIter->second = "" ;

        attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetLogsFromAttrName()) ;
        attrIter->second = "" ;

        attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetLogsToAttrName()) ;
        attrIter->second = "" ;

        attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetLogsTOUTCAttrName()) ;
        attrIter->second = "" ;

        attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetMaintenanceActionAttrName()) ; 
        attrIter->second = "" ;

        attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetPairStateAttrName()) ;
        attrIter->second = boost::lexical_cast<std::string>(0); ;

        attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetPauseReasonAttrName()) ;
        attrIter->second = "" ;

        attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetQuasiDiffEndtimeAttrName()) ;
        attrIter->second = "0" ;

        attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetQuasiDiffStarttimeAttrName()) ;
        attrIter->second = "0" ;

        attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetReplicationCleanupOptionAttrName()) ;
        attrIter->second = "" ;

        attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetRestartResyncCounterAttrName()) ;
        int resyncCount = 0 ;
        try
        {
            resyncCount = boost::lexical_cast<int> (attrIter->second) ;
            resyncCount++ ;

        }
        catch(boost::bad_lexical_cast& be)
        {
            DebugPrintf(SV_LOG_DEBUG,"Exception : %s\n",be.what());
            DebugPrintf(SV_LOG_DEBUG, "failed to cast %s to int. its value is %s\n", 
                m_proPairAttrNamesObj.GetRestartResyncCounterAttrName(), attrIter->second.c_str()) ;
            resyncCount = 2 ;
        }
        attrIter->second = boost::lexical_cast<std::string>(resyncCount) ;

        attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetResyncEndTagTimeAttrName()) ;
        attrIter->second = "0" ;

        attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetResyncStartTagTimeAttrName()) ;
        attrIter->second = "0" ;

        attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetResyncStartTime()) ;
        std::string resyncstarttime = boost::lexical_cast<std::string>(time(NULL)) ;
        attrIter->second = resyncstarttime ;
		attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetResyncEndTime()) ;
		attrIter->second = "0" ;
        attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetVolumeResizedAttrName()) ;
        attrIter->second = boost::lexical_cast<std::string>(VOLUME_SIZE_NORMAL) ;

        attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetResizePolicyTSAttrName()) ;
        attrIter->second = "0";

        attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetShouldResyncSetFromAttrName()) ;
        attrIter->second = "" ;

        attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetResyncSetTimeStampAttrName()) ;
        attrIter->second = "" ;

        attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetResyncEndSeqAttrName()) ;
        attrIter->second = "0" ;

        attrIter =  protectionPairAttrs.find(m_proPairAttrNamesObj.GetResyncStartSeqAttrName()) ;
        attrIter->second = "0" ;

        attrIter =  protectionPairAttrs.find(m_proPairAttrNamesObj.GetPendingChangesSizeAttrName()) ;
        attrIter->second = "0" ;


        attrIter =  protectionPairAttrs.find(m_proPairAttrNamesObj.GetAutoResyncStartTimeAttrName()) ;
        attrIter->second = "" ;

        attrIter =  protectionPairAttrs.find(m_proPairAttrNamesObj.GetAutoResumeAttrName()) ;
        attrIter->second = "" ;

        attrIter =  protectionPairAttrs.find(m_proPairAttrNamesObj.GetResyncSetCXtimeStampAttrName()) ;
        attrIter->second = "" ;
		attrIter =  protectionPairAttrs.find(m_proPairAttrNamesObj.GetFsUnusedBytes()) ;
        attrIter->second = "0" ;
		attrIter =  protectionPairAttrs.find(m_proPairAttrNamesObj.GetAutoResyncDisabledAttrName()) ;
		if( attrIter != protectionPairAttrs.end() )
		{
			attrIter->second = "0" ;
		}
		attrIter =  protectionPairAttrs.find(m_proPairAttrNamesObj.GetVolumeResizedActionCountAttrName()) ;
		if( attrIter != protectionPairAttrs.end() )
		{
			attrIter->second = "0" ;
		}
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}
bool ProtectionPairConfig::isVolumeExists (const std::string& srcVolumeName)
{
	bool bRet = false; 
    ConfSchema::ObjectsIter_t objIter = find_if( m_ProtectionPairGrp->m_Objects.begin(),
        m_ProtectionPairGrp->m_Objects.end(),
        ConfSchema::EqAttr(m_proPairAttrNamesObj.GetSrcNameAttrName(),
        srcVolumeName.c_str())) ;
    if( objIter != m_ProtectionPairGrp->m_Objects.end()) 
	{
		bRet = true; 
	}
	return bRet ; 	
}
bool ProtectionPairConfig::IncrementVolumeResizeActionCount(const std::string& srcVolumeName)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    ConfSchema::Object* protectionPairObj = NULL ;
    if( !srcVolumeName.empty() )
    {
        bRet = GetProtectionPairObjBySrcDevName(srcVolumeName,  &protectionPairObj) ;
    }
    if( bRet )
    {
		ConfSchema::AttrsIter_t attrIter ;
		int volumeResizeActionCount = 0 ;
		attrIter = protectionPairObj->m_Attrs.find( m_proPairAttrNamesObj.GetVolumeResizedActionCountAttrName() ) ;
		if (attrIter != protectionPairObj->m_Attrs.end() ) 
		{
			volumeResizeActionCount = boost::lexical_cast <int> ( attrIter->second ) ; 
			if (volumeResizeActionCount == 2) 
			{
				volumeResizeActionCount = 0 ;
				attrIter->second = boost::lexical_cast <std::string> ( volumeResizeActionCount )  ; 
				bRet = true; 
			}
			else 
			{
				volumeResizeActionCount++  ;
				attrIter->second = boost::lexical_cast <std::string> ( volumeResizeActionCount )  ; 
				bRet = true; 
			}
		}
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return bRet ; 	
}

bool ProtectionPairConfig::GetVolumeResizeActionCount(const std::string& srcVolumeName , int &volumeResizeActionCount)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTER %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    ConfSchema::Object* protectionPairObj = NULL ;
    if( !srcVolumeName.empty() )
    {
        bRet = GetProtectionPairObjBySrcDevName(srcVolumeName,  &protectionPairObj) ;
    }
    if( bRet )
    {
		ConfSchema::AttrsIter_t attrIter ;
		attrIter = protectionPairObj->m_Attrs.find( m_proPairAttrNamesObj.GetVolumeResizedActionCountAttrName() ) ;
		if (attrIter != protectionPairObj->m_Attrs.end() ) 
		{
			volumeResizeActionCount = boost::lexical_cast <int> ( attrIter->second ) ; 
			bRet = true; 
		}
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return bRet ; 	
}

std::string ProtectionPairConfig::GenerateCatalogPath( const std::string& retentionVolume,
                                                      const std::string& srcVolumeName)
{
    std::string catalogPath ;
    ConfSchema::ObjectsIter_t objIter = find_if( m_ProtectionPairGrp->m_Objects.begin(),
        m_ProtectionPairGrp->m_Objects.end(),
        ConfSchema::EqAttr(m_proPairAttrNamesObj.GetSrcNameAttrName(),
        srcVolumeName.c_str())) ;
    if( objIter == m_ProtectionPairGrp->m_Objects.end() || 
        ( objIter != m_ProtectionPairGrp->m_Objects.end() &&
        !GetRetentionPath(&(objIter->second), catalogPath)) )
    {
        catalogPath = retentionVolume ;
        if( catalogPath.length() == 1 )
        {
            catalogPath += ":" ;
        }
        std::string uniqId = boost::lexical_cast<std::string>(time(NULL)) ;
        catalogPath += '\\' ;
		catalogPath += AgentConfig::GetInstance()->getHostId() ;
        catalogPath += '\\' ;
        catalogPath += "catalogue" ;
        catalogPath += '\\' ;
        std::string srcVol = srcVolumeName ;
        boost::algorithm::replace_all(srcVol, "\\", "_") ;
        boost::algorithm::replace_all(srcVol, ":", "_") ;
        catalogPath += srcVol ;
        ACE_DIR * ace_dir = NULL ;
		ACE_DIRENT * dirent = NULL ;
		ACE_stat stat ;
		ace_dir = sv_opendir(catalogPath.c_str()) ;
		if( ace_dir != NULL )
		{
			do
			{
				dirent = ACE_OS::readdir(ace_dir) ;
				bool isDir = false ;
				if( dirent != NULL )
				{
					std::string fileName = ACE_TEXT_ALWAYS_CHAR(dirent->d_name);
					if( strcmp(fileName.c_str(), "." ) == 0 || strcmp(fileName.c_str(), ".." ) == 0 )
					{
						continue ;
					}
					std::string absPath = catalogPath + ACE_DIRECTORY_SEPARATOR_CHAR_A + fileName ;
					int statRetVal = 0 ;
					if( (statRetVal = sv_stat( getLongPathName(absPath.c_str()).c_str(), &stat )) != 0 )
					{
						DebugPrintf(SV_LOG_ERROR, "ACE_OS::stat for %s is failed with error %d \n", absPath.c_str(), statRetVal) ;
						continue ;
					}
					if( stat.st_mode & S_IFDIR )
					{
						uniqId = fileName ;
					}
				}
			}  while ( dirent != NULL  ) ;
			ACE_OS::closedir( ace_dir ) ;
		}
        catalogPath += '\\' ;
		catalogPath += uniqId ;
        catalogPath += '\\' ;
        catalogPath += "cdpv3.db" ;
    }
    return catalogPath ;
}

void ProtectionPairConfig::PopualateRetentionEventPolicies(ConfSchema::Object& defaultRetentionWindowObj, 
                                                           const std::string& retentionPath, 
                                                           const std::string& retentionVolume, 
                                                           const std::string &cataloguePath, 
                                                           SV_UINT tagCount, 
                                                           bool bExisting )
{
    if( bExisting == false )
    {
        ConfSchema::Group retEvtPolicyGrp ;
        ConfSchema::Object retevtPoliciesObj ;
        retevtPoliciesObj.m_Attrs.insert(std::make_pair( m_retevtPolicyAttrNamesObj.GetStoragePathAttrName(), retentionPath)) ;
        retevtPoliciesObj.m_Attrs.insert(std::make_pair( m_retevtPolicyAttrNamesObj.GetStorageDeviceAttrName(), retentionVolume)) ;
        retevtPoliciesObj.m_Attrs.insert(std::make_pair( m_retevtPolicyAttrNamesObj.GetStoragePrunePolicyAttrName(), "0")) ;
        retevtPoliciesObj.m_Attrs.insert(std::make_pair( m_retevtPolicyAttrNamesObj.GetConsistencyTagAttrName(), "")) ;
        retevtPoliciesObj.m_Attrs.insert(std::make_pair( m_retevtPolicyAttrNamesObj.GetAlertThresholdAttrName(), "80")) ;
        retevtPoliciesObj.m_Attrs.insert(std::make_pair( m_retevtPolicyAttrNamesObj.GetTagCountAttrName(), boost::lexical_cast<std::string>(tagCount))) ;
        retevtPoliciesObj.m_Attrs.insert(std::make_pair( m_retevtPolicyAttrNamesObj.GetUserDefinedTagAttrName(), "")) ;
        retevtPoliciesObj.m_Attrs.insert(std::make_pair( m_retevtPolicyAttrNamesObj.GetCatalogPathAttrName(), cataloguePath)) ;
        ACE_OS::sleep(1) ;
        retEvtPolicyGrp.m_Objects.insert(std::make_pair( boost::lexical_cast<std::string>(time(NULL)), 
            retevtPoliciesObj)) ;
        defaultRetentionWindowObj.m_Groups.insert(std::make_pair( m_retevtPolicyAttrNamesObj.GetName(), retEvtPolicyGrp)) ;
    }
    else
    {
        ConfSchema::GroupsIter_t groupsIter = defaultRetentionWindowObj.m_Groups.find( m_retevtPolicyAttrNamesObj.GetName() ) ;
        if( groupsIter != defaultRetentionWindowObj.m_Groups.end() )
        {
            ConfSchema::ObjectsIter_t objectsIter = (groupsIter->second).m_Objects.begin() ;
            ConfSchema::AttrsIter_t attrIter = objectsIter->second.m_Attrs.find(  m_retevtPolicyAttrNamesObj.GetTagCountAttrName() ) ;
            attrIter->second = boost::lexical_cast<std::string>(tagCount) ;
            attrIter = objectsIter->second.m_Attrs.find(  m_retevtPolicyAttrNamesObj.GetCatalogPathAttrName()) ;
            attrIter->second = cataloguePath ;
        }
    }
    return ;
}

void ProtectionPairConfig::PopulateDefaultRetentionWindowObj(ConfSchema::Object& defaultWindowObj,
                                                             const RetentionPolicy& retPolicy, bool bExisting )
{
    time_t startTime = 0;
    ConfSchema::Attrs_t& defaultretWindowAttrs = defaultWindowObj.m_Attrs ;
    time_t durationInHours ;
    std::string timeIntervalUnit ;
    GetDurationAndTimeIntervelUnit( retPolicy.duration, durationInHours, timeIntervalUnit) ;
    time_t endTime = startTime + durationInHours ;

    if( bExisting == false )
    {
        defaultretWindowAttrs.insert(std::make_pair( m_retWindowAttrNamesObj.GetStartTimeAttrName(),  boost::lexical_cast<std::string>(startTime))) ;            
        defaultretWindowAttrs.insert(std::make_pair( m_retWindowAttrNamesObj.GetEndTimeAttrName(),  boost::lexical_cast<std::string>(endTime))) ;
        defaultretWindowAttrs.insert(std::make_pair(m_retWindowAttrNamesObj.GetTimeGranularityAttrName(),  "hour"));
        defaultretWindowAttrs.insert(std::make_pair(m_retWindowAttrNamesObj.GetTimeIntervalUnitAttrName(), timeIntervalUnit));
        defaultretWindowAttrs.insert(std::make_pair(m_retWindowAttrNamesObj.GetGranularityUnitAttrName() ,"0"));
        defaultretWindowAttrs.insert(std::make_pair(m_retWindowAttrNamesObj.GetRetWindowIdAttrName() ,"Default"));
    }
    else
    {
        ConfSchema::AttrsIter_t attrIter = defaultretWindowAttrs.find(  m_retWindowAttrNamesObj.GetEndTimeAttrName() ) ;
        if( attrIter != defaultretWindowAttrs.end() )
        {
            attrIter->second = boost::lexical_cast<std::string>(endTime) ;
        }
    }

    return ;
}

void ProtectionPairConfig::PopulateRetentionWindowObj(ConfSchema::Object& dailyWindowObj,
                                                      const RetentionPolicy& retPolicy,
                                                      const ScenarioRetentionWindow& retWindow,
                                                      const std::string& previousEndTime, bool bExisting  )
{
    time_t startTime = boost::lexical_cast<time_t>(previousEndTime);
    time_t durationInHour; 
    std::string timeIntervelUnit;
    GetDurationAndTimeIntervelUnit(retWindow.duration, durationInHour, timeIntervelUnit);
    time_t endTime = startTime + durationInHour ;
    std::string  granularityUnit;
    std::string  timeGranularityUnit;
    GetGranularityUnitAndTimeGranularityUnit(retWindow.granularity, granularityUnit, 
        timeGranularityUnit);

    if( bExisting == false )
    {
        dailyWindowObj.m_Attrs.insert(std::make_pair(m_retWindowAttrNamesObj.GetStartTimeAttrName(), previousEndTime));
        dailyWindowObj.m_Attrs.insert(std::make_pair(m_retWindowAttrNamesObj.GetEndTimeAttrName(), boost::lexical_cast<std::string>(endTime)));        
        dailyWindowObj.m_Attrs.insert(std::make_pair(m_retWindowAttrNamesObj.GetTimeGranularityAttrName(), timeGranularityUnit));
        dailyWindowObj.m_Attrs.insert(std::make_pair(m_retWindowAttrNamesObj.GetTimeIntervalUnitAttrName(), timeIntervelUnit));
        dailyWindowObj.m_Attrs.insert(std::make_pair(m_retWindowAttrNamesObj.GetGranularityUnitAttrName(), granularityUnit));
        dailyWindowObj.m_Attrs.insert(std::make_pair(m_retWindowAttrNamesObj.GetRetWindowIdAttrName() , retWindow.retentionWindowId )) ;
    }
    else
    {
        ConfSchema::AttrsIter_t attrIter = dailyWindowObj.m_Attrs.find( m_retWindowAttrNamesObj.GetStartTimeAttrName() ) ;
        if( attrIter != dailyWindowObj.m_Attrs.end() )
            attrIter->second = previousEndTime ;

        attrIter = dailyWindowObj.m_Attrs.find( m_retWindowAttrNamesObj.GetEndTimeAttrName() ) ;
        if( attrIter != dailyWindowObj.m_Attrs.end() )
            attrIter->second = boost::lexical_cast<std::string>(endTime) ;

        attrIter = dailyWindowObj.m_Attrs.find( m_retWindowAttrNamesObj.GetGranularityUnitAttrName() ) ;
        if( attrIter != dailyWindowObj.m_Attrs.end() )
            attrIter->second = granularityUnit ;
    }
    return ;
}

void ProtectionPairConfig::PopulateRetentionWindowObjects(ConfSchema::Object& retentionLogObject,
                                                          const PairDetails& pairDetails)
{
    ConfSchema::Object defaultWindowObj ;

    ConfSchema::Group retWindowGrp ;

    PopulateDefaultRetentionWindowObj(defaultWindowObj, pairDetails.retPolicy ) ;
    std::string cataloguePath = GenerateCatalogPath( pairDetails.retentionVolume, pairDetails.srcVolumeName ) ;
    PopualateRetentionEventPolicies( defaultWindowObj, pairDetails.retentionPath, pairDetails.retentionVolume, cataloguePath ) ;
    ConfSchema::AttrsIter_t attrIter ;
    std::string previousEndTime = "0" ;
    attrIter = defaultWindowObj.m_Attrs.find( m_retWindowAttrNamesObj.GetEndTimeAttrName());
    if( attrIter != defaultWindowObj.m_Attrs.end() )
    {
        previousEndTime = attrIter->second ;
    }
    ACE_OS::sleep(1) ;
    retWindowGrp.m_Objects.insert(std::make_pair(boost::lexical_cast<std::string>(time(NULL)) , defaultWindowObj)) ;
    std::list<ScenarioRetentionWindow>::const_iterator retWindowIter = pairDetails.retPolicy.retentionWindows.begin() ;
    while( retWindowIter != pairDetails.retPolicy.retentionWindows.end() )
    {
        ConfSchema::Object dailyWindowObj ;
        PopulateRetentionWindowObj(dailyWindowObj, pairDetails.retPolicy,*retWindowIter, previousEndTime ) ;
        PopualateRetentionEventPolicies(dailyWindowObj,  pairDetails.retentionPath, pairDetails.retentionVolume, cataloguePath, retWindowIter-> count ) ;
        previousEndTime = dailyWindowObj.m_Attrs.find( m_retWindowAttrNamesObj.GetEndTimeAttrName())->second ;
        ACE_OS::sleep(1) ;
        retWindowGrp.m_Objects.insert(std::make_pair( boost::lexical_cast<std::string>(time(NULL)) , 
            dailyWindowObj)) ;

        retWindowIter++ ;
    }
    retentionLogObject.m_Groups.insert(std::make_pair( m_retWindowAttrNamesObj.GetName(), retWindowGrp)) ;
}

void ProtectionPairConfig::PopulateRetentionInformation(const PairDetails& pairDetails, 
                                                        ConfSchema::Object& protectionPairObj)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::list<ScenarioRetentionWindow>::const_iterator scenarioRetWindowIter ;
    scenarioRetWindowIter = pairDetails.retPolicy.retentionWindows.begin() ;
    ConfSchema::Object retentionLogObject ;
    ConfSchema::Attrs_t& retentionLogAttrs = retentionLogObject.m_Attrs ;
    retentionLogAttrs.insert(std::make_pair( m_retLogPolicyAttrNamesObj.GetCurrLogSizeAttrName(), "0")) ;
    retentionLogAttrs.insert(std::make_pair( m_retLogPolicyAttrNamesObj.GetCreatedDateAttrName(), "0")) ;
    retentionLogAttrs.insert(std::make_pair( m_retLogPolicyAttrNamesObj.GetDiskSpaceThresholdAttrName(), "256")) ;
    retentionLogAttrs.insert(std::make_pair( m_retLogPolicyAttrNamesObj.GetIsSparceEnabledAttrName(), "1")) ;
    retentionLogAttrs.insert(std::make_pair( m_retLogPolicyAttrNamesObj.GetModifiedDateAttrName(), "0")) ;
    retentionLogAttrs.insert(std::make_pair( m_retLogPolicyAttrNamesObj.GetRetStateAttrName(), "0")) ;
    retentionLogAttrs.insert(std::make_pair( m_retLogPolicyAttrNamesObj.GetUniqueIdAttrName(), 
        boost::lexical_cast<std::string>(time(NULL)))) ;

    PopulateRetentionWindowObjects( retentionLogObject, pairDetails) ;
    ConfSchema::Group retLogPolicyGrp ;
    ACE_OS::sleep(1) ;
    retLogPolicyGrp.m_Objects.insert(std::make_pair( boost::lexical_cast<std::string>(time(NULL)), 
        retentionLogObject)) ;
    protectionPairObj.m_Groups.insert(std::make_pair( m_retLogPolicyAttrNamesObj.GetName(), retLogPolicyGrp)) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

int ProtectionPairConfig::PairsInResync()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    int pairsInResync = 0 ;
    if( m_ProtectionPairGrp->m_Objects.size() )
    {
        ConfSchema::ObjectsIter_t protectionPairObjIter = m_ProtectionPairGrp->m_Objects.begin() ;
        while( protectionPairObjIter != m_ProtectionPairGrp->m_Objects.end() )
        {
            if( IsPairInStepOne(&(protectionPairObjIter->second) ) )
            {
                pairsInResync++ ;
            }
            protectionPairObjIter++ ;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return pairsInResync ;
}

bool ProtectionPairConfig::UpdatePairWithRetention(  const std::string& srcVolume,
                                                   const std::string& retentionvolume,
                                                   const std::string& retentionpath,
                                                   const std::string& catalogpath,
                                                   const RetentionPolicy& retentionpolicy )
{
    ConfSchema::ObjectsIter_t protectionPairObjIter ;
    protectionPairObjIter = find_if( m_ProtectionPairGrp->m_Objects.begin(),
        m_ProtectionPairGrp->m_Objects.end(),
        ConfSchema::EqAttr( m_proPairAttrNamesObj.GetSrcNameAttrName(),
        srcVolume.c_str())) ;
    ConfSchema::Object *retentionPolicyObj ;
    ConfSchema::Object *protectionPairObj ;
    if( protectionPairObjIter != m_ProtectionPairGrp->m_Objects.end() )
    {
        protectionPairObj = &(protectionPairObjIter->second) ;
        ConfSchema::GroupsIter_t retentionGroupsIter = protectionPairObj->m_Groups.find( m_retLogPolicyAttrNamesObj.GetName() ) ;
        ConfSchema::Group* retentionGroup = &(retentionGroupsIter->second );
        ConfSchema::ObjectsIter_t retIter = retentionGroup->m_Objects.begin() ;
        retentionPolicyObj = &( retIter->second ) ;
        UpdateRetentionObject( *retentionPolicyObj, retentionpolicy, 
            srcVolume, retentionvolume, retentionpath, catalogpath ) ;
    }
    return true ;
}


bool ProtectionPairConfig::UpdatePairsWithRetention( RetentionPolicy& retentionPolicy ) 
{
    ConfSchema::Object *retentionPolicyObj ;
    ConfSchema::Object *protectionPairObj ;
    ConfSchema::ObjectsIter_t pairIterator, retIter  ;

    pairIterator = m_ProtectionPairGrp->m_Objects.begin() ;
    while( pairIterator != m_ProtectionPairGrp->m_Objects.end() )
    {
        protectionPairObj = &(pairIterator->second) ;
        std::string srcVolumeName, retentionVolume, retentionPath ;
        GetSourceVolumeRetentionPathAndVolume( protectionPairObj, srcVolumeName, retentionVolume, retentionPath ) ;
        ConfSchema::GroupsIter_t retentionGroupsIter = protectionPairObj->m_Groups.find( m_retLogPolicyAttrNamesObj.GetName() ) ;
        ConfSchema::Group* retentionGroup = &(retentionGroupsIter->second );
        retIter = retentionGroup->m_Objects.begin() ;
        retentionPolicyObj = &( retIter->second ) ;
        UpdateRetentionObject( *retentionPolicyObj, retentionPolicy, srcVolumeName, retentionVolume, retentionPath ) ;
        pairIterator++ ;
    }
    return true ;
}

bool ProtectionPairConfig::GetRetentionWindowObjectForUpdate( std::string windowType, 
                                                             ConfSchema::Group &retentionWindowGroup, 
                                                             ConfSchema::Object** retentionWindowObject )
{
    bool bRet = true ;

    ConfSchema::ObjectsIter_t retwindowIter = find_if( retentionWindowGroup.m_Objects.begin(), retentionWindowGroup.m_Objects.end(), 
        ConfSchema::EqAttr(m_retWindowAttrNamesObj.GetRetWindowIdAttrName(), windowType.c_str() ) ) ;
    if( retwindowIter == retentionWindowGroup.m_Objects.end() )
    {
        ConfSchema::Object retentionWindow ;
        retentionWindow.m_Attrs.insert( std::make_pair( m_retWindowAttrNamesObj.GetRetWindowIdAttrName(), windowType ) ) ;
        ACE_OS::sleep(1) ;
        retentionWindowGroup.m_Objects.insert( std::make_pair( boost::lexical_cast<std::string>(time(NULL)), 
            retentionWindow ) ) ;
        retwindowIter = find_if( retentionWindowGroup.m_Objects.begin(), retentionWindowGroup.m_Objects.end(), 
            ConfSchema::EqAttr(m_retWindowAttrNamesObj.GetRetWindowIdAttrName(), windowType.c_str() ) ) ;        
        bRet = false ;
    }
    *retentionWindowObject = &(retwindowIter->second ) ;
    return bRet; 
}

bool ProtectionPairConfig::UpdateRetentionObject( ConfSchema::Object &retentionPolicyObj, 
                                                 const RetentionPolicy& retentionPolicy, 
                                                 const std::string& srcVolumeName, 
                                                 const std::string& retentionVolume, 
                                                 const std::string& retentionPath,
                                                 const std::string& cataloguePath)
{
    ConfSchema::Object* retentionWindowObj ;
    bool bRet = true ;
    ConfSchema::GroupsIter_t retentionwindowGroupIter =  retentionPolicyObj.m_Groups.find( m_retWindowAttrNamesObj.GetName() ) ;
    ConfSchema::Group* retentionWindowGroup = &(retentionwindowGroupIter->second ) ;

    bRet = GetRetentionWindowObjectForUpdate( "Default",  *retentionWindowGroup, &retentionWindowObj ) ;
    std::string generatecatalogpath = cataloguePath ;
    if( generatecatalogpath.empty() )
    {

        generatecatalogpath  = GenerateCatalogPath(retentionVolume, srcVolumeName) ;
    }
    PopulateDefaultRetentionWindowObj( *retentionWindowObj, retentionPolicy, bRet ) ; 
    PopualateRetentionEventPolicies( *retentionWindowObj, retentionPath, retentionVolume,  generatecatalogpath, 1, bRet ) ;
    std::string previousEndTime = "0" ;

    std::list<ScenarioRetentionWindow>::const_iterator retWindowIter = retentionPolicy.retentionWindows.begin() ;
    std::map<std::string,std::string>::iterator attrIter ;
    while( retWindowIter != retentionPolicy.retentionWindows.end() )
    {
        attrIter = retentionWindowObj->m_Attrs.find( m_retWindowAttrNamesObj.GetEndTimeAttrName());
        if( attrIter != retentionWindowObj->m_Attrs.end() )
        {
            previousEndTime = attrIter->second ;
            bRet = GetRetentionWindowObjectForUpdate( retWindowIter->retentionWindowId, *retentionWindowGroup, &retentionWindowObj ) ;
            PopulateRetentionWindowObj( *retentionWindowObj, retentionPolicy, *retWindowIter, previousEndTime, bRet ) ;
            PopualateRetentionEventPolicies( *retentionWindowObj, retentionPath, retentionVolume,  generatecatalogpath, retWindowIter->count, bRet  ) ;
        }

        retWindowIter++ ;
    }

    ConfSchema::ObjectsIter_t retwindowOldIter = retentionWindowGroup->m_Objects.begin() ;
    std::map<std::string, std::string>::iterator windowIdIter ;
    std::list<std::string> windowIdList ;
    std::list<std::string>::iterator windowIdListIter ;
    while( retwindowOldIter !=  retentionWindowGroup->m_Objects.end() )
    {
        windowIdIter = retwindowOldIter->second.m_Attrs.find( m_retWindowAttrNamesObj.GetRetWindowIdAttrName() ) ;
        if( windowIdIter->second.compare( "Default" ) != 0 )
        {
            retWindowIter = retentionPolicy.retentionWindows.begin() ;
            while( retWindowIter != retentionPolicy.retentionWindows.end() )
            {
                if( windowIdIter->second.compare( retWindowIter->retentionWindowId ) == 0 )
                {
                    break ;
                }
                retWindowIter++ ;
            }
            if( retWindowIter == retentionPolicy.retentionWindows.end() )
                windowIdList.push_back( retwindowOldIter->first ) ;
        }
        retwindowOldIter++ ;
    }
    windowIdListIter = windowIdList.begin() ;
    while( windowIdListIter !=  windowIdList.end() )
    {
        retentionWindowGroup->m_Objects.erase( *windowIdListIter ) ;
        windowIdListIter++ ;
    }
    return bRet ;
}

void ProtectionPairConfig::AddToProtection( const PairDetails& pairDetails, int batchSize)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ConfSchema::Object protectionPairObj ;
    ConfSchema::Object retentionPolicyObj ;
    bool bQueued = ShouldQueued(NULL, batchSize) ;
    PopulateProtectionPairAttrs(pairDetails, protectionPairObj, bQueued) ;
    PopulateRetentionInformation(pairDetails, protectionPairObj) ;
    ACE_OS::sleep(1) ;
    m_ProtectionPairGrp->m_Objects.insert(std::make_pair( boost::lexical_cast<std::string>(time(NULL)), 
        protectionPairObj)) ;
    AuditConfigPtr auditConfigPtr = AuditConfig::GetInstance() ;
    std::string auditmsg ;
    auditmsg = "Protection settings added for " + pairDetails.srcVolumeName + " <--> " + pairDetails.targetVolume ;
    auditConfigPtr->LogAudit(auditmsg) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void ProtectionPairConfig::RestartResyncForPair( const std::string& srcDevName,
                                                int batchSize,
                                                const std::string& tgtDevName,
                                                const std::string& srcHostId,
                                                const std::string& tgtHostId)
{
    bool bRet = false ;
    ConfSchema::Object* protectionPairObj = NULL ;
    if( !srcDevName.empty() && !tgtDevName.empty() )
    {
        bRet = GetProtectionPairObj( srcDevName, tgtDevName, &protectionPairObj ) ;
    }
    else if( !srcDevName.empty() )
    {
        bRet = GetProtectionPairObjBySrcDevName( srcDevName, &protectionPairObj ) ;
    }
    else if( !tgtDevName.empty() )
    {
        bRet = GetProtectionPairObjByTgtDevName( tgtDevName, &protectionPairObj ) ;
    }
    if( bRet )
    {
        bool queued = ShouldQueued(protectionPairObj, batchSize); 
        RestartResyncForPair( protectionPairObj, queued ) ;
        AuditConfigPtr auditConfigPtr = AuditConfig::GetInstance() ;
        std::string auditmsg ;
        auditmsg = "Requesting the re-syncing of volume " + srcDevName ;
        if( queued )
        {
            auditmsg += ". it is queued now. " ;
        }
        auditConfigPtr->LogAudit(auditmsg) ;

    }
}
void ProtectionPairConfig::GetProtectedTargetVolumes( std::list<std::string>& protectedVolumes)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ConfSchema::ObjectsIter_t pairObjIter = m_ProtectionPairGrp->m_Objects.begin() ;    
    while( pairObjIter != m_ProtectionPairGrp->m_Objects.end() )
    {
        ConfSchema::Object* protectionPairObj = NULL ;
        protectionPairObj = &(pairObjIter->second) ;
        ConfSchema::Attrs_t& protectionPairAttrs = protectionPairObj->m_Attrs ;
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = protectionPairAttrs.find( m_proPairAttrNamesObj.GetTgtNameAttrName()) ;
        protectedVolumes.push_back(attrIter->second) ;
        pairObjIter++ ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;

}

void ProtectionPairConfig::GetProtectedPairs( std::map<std::string, std::string>& pairs ) 
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ConfSchema::ObjectsIter_t pairObjIter = m_ProtectionPairGrp->m_Objects.begin() ; 
    ConfSchema::AttrsIter_t attrIter, attrIterTgt ;
    ConfSchema::Object* protectionPairObj = NULL ;
    while( pairObjIter != m_ProtectionPairGrp->m_Objects.end() )
    {
        protectionPairObj = &(pairObjIter->second) ;
        ConfSchema::Attrs_t& protectionPairAttrs = protectionPairObj->m_Attrs ;
        attrIter = protectionPairAttrs.find( m_proPairAttrNamesObj.GetSrcNameAttrName()) ;
        attrIterTgt = protectionPairAttrs.find( m_proPairAttrNamesObj.GetTgtNameAttrName() ) ;
        pairs.insert(std::make_pair( attrIter->second, attrIterTgt->second ) ) ;
        pairObjIter++ ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
void ProtectionPairConfig::GetProtectedVolumes( std::list<std::string>& protectedVolumes)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ConfSchema::ObjectsIter_t pairObjIter = m_ProtectionPairGrp->m_Objects.begin() ;    
    while( pairObjIter != m_ProtectionPairGrp->m_Objects.end() )
    {
        ConfSchema::Object* protectionPairObj = NULL ;
        protectionPairObj = &(pairObjIter->second) ;
        ConfSchema::Attrs_t& protectionPairAttrs = protectionPairObj->m_Attrs ;
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = protectionPairAttrs.find( m_proPairAttrNamesObj.GetSrcNameAttrName()) ;
        protectedVolumes.push_back(attrIter->second) ;
        pairObjIter++ ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void ProtectionPairConfig::DeletePair( const std::string& srcDevName,
                                      const std::string& tgtDevName,
                                      const std::string& srcHostId,
                                      const std::string& tgtHostId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    DebugPrintf(SV_LOG_DEBUG, "Src Dev : %s, Tgt Dev : %s\n", 
        srcDevName.c_str(), tgtDevName.c_str()) ;
    ConfSchema::ObjectsIter_t protPairObjIter  = m_ProtectionPairGrp->m_Objects.end() ;
    if( !srcDevName.empty() )
    {
        protPairObjIter = find_if(m_ProtectionPairGrp->m_Objects.begin(),
            m_ProtectionPairGrp->m_Objects.end(),
            ConfSchema::EqAttr(m_proPairAttrNamesObj.GetSrcNameAttrName(),
            srcDevName.c_str())) ;

    }
    if( !tgtDevName.empty() )
    {
        protPairObjIter = find_if(m_ProtectionPairGrp->m_Objects.begin(),
            m_ProtectionPairGrp->m_Objects.end(),
            ConfSchema::EqAttr(m_proPairAttrNamesObj.GetTgtNameAttrName(),
            tgtDevName.c_str())) ;

    }
    if( protPairObjIter != m_ProtectionPairGrp->m_Objects.end() )
    {
        m_ProtectionPairGrp->m_Objects.erase(protPairObjIter->first) ;
        AuditConfigPtr auditConfigPtr = AuditConfig::GetInstance() ;
        std::string auditmsg ;
        auditmsg = "Removing the protection settings for " + srcDevName + " <--> " + tgtDevName ;
        auditConfigPtr->LogAudit(auditmsg) ;

    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

bool ProtectionPairConfig::IsAutoResyncAllowed( ConfSchema::Object* protectionPairObj  )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ConfSchema::Attrs_t& protectionPairAttrs = protectionPairObj->m_Attrs ;
    ConfSchema::AttrsIter_t attrIter ;
    bool bRet = false ;
    attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetVolumeResizedAttrName());
    eVolumeResize resizeStatus ;
    resizeStatus = (eVolumeResize) boost::lexical_cast<int>(attrIter->second) ;

    attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetShouldResyncSetFromAttrName());
    std::string shouldResyncSetFrom = attrIter->second ;
    attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetExecutionStateAttrName()) ;
    int executionState = boost::lexical_cast<int>(attrIter->second) ;
    VOLUME_SETTINGS::PAIR_STATE pairState ;
    GetPairState (protectionPairObj , pairState ) ;
    if( resizeStatus == VOLUME_SIZE_NORMAL && 
        executionState == 0 && 
        !shouldResyncSetFrom.empty() &&
        pairState != VOLUME_SETTINGS::DELETE_PENDING &&
        pairState != VOLUME_SETTINGS::CLEANUP_DONE )
    {
        bRet = true ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool ProtectionPairConfig::ShouldQueued(ConfSchema::Object* protectionPairObj, int batchSize)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bQueued = false ;
    if( ( protectionPairObj == NULL ) || 
        ( protectionPairObj != NULL && IsPairInQueuedState (protectionPairObj) ) || 
        ( protectionPairObj != NULL && !IsPairInStepOne(protectionPairObj) )
        ) 
    {
        if( batchSize != 0 )
        {
            int pairsInResync = GetNumberOfPairsProgressingInStepOne(); //PairsInResync() ;
            if( pairsInResync >= batchSize )
            {
                bQueued = true ;
            }
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bQueued ;
}

bool ProtectionPairConfig::IsResyncRequiredSetToYes(ConfSchema::Object* protectionPairObj)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    ConfSchema::AttrsIter_t attrIter ;
    int resyncSetFrom = 0 ;
    attrIter = protectionPairObj->m_Attrs.find( m_proPairAttrNamesObj.GetShouldResyncSetFromAttrName() ) ;
    try
    {
        resyncSetFrom = boost::lexical_cast<int>(attrIter->second) ;
    }
    catch( boost::bad_lexical_cast& bx)
    {
    }
    DebugPrintf(SV_LOG_DEBUG, "ResyncSetFrom Value %d\n", resyncSetFrom) ;
    if( resyncSetFrom != 0 )
    {
        bRet = true ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool ProtectionPairConfig::IsResyncRequiredSetToYes(const std::string& srcDevName,
                                                    const std::string& tgtDevName,
                                                    const std::string& srcHostId,
                                                    const std::string& tgtHostId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    ConfSchema::Object* protectionPairObj = NULL ;
    if( !srcDevName.empty() && !tgtDevName.empty() )
    {
        bRet = GetProtectionPairObj(srcDevName, tgtDevName, &protectionPairObj) ;
    }
    if( !srcDevName.empty() )
    {
        bRet = GetProtectionPairObjBySrcDevName(srcDevName,  &protectionPairObj) ;
    }

    if( !tgtDevName.empty() )
    {
        bRet = GetProtectionPairObjByTgtDevName(tgtDevName, &protectionPairObj) ;
    }
    if( bRet )
    {
        bRet = false ;
        if( IsResyncRequiredSetToYes(protectionPairObj) )
        {
            bRet = true ;
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "Src Dev Name %s, Tgt Dev Name %s\n",
            srcDevName.c_str(), tgtDevName.c_str()) ;
        throw "Cannot find the pair details" ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

void ProtectionPairConfig::PerformAutoResync(int batchSize)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ConfSchema::ObjectsIter_t protPairObjIter  = m_ProtectionPairGrp->m_Objects.begin() ;
    while( protPairObjIter != m_ProtectionPairGrp->m_Objects.end() )
    {
        ConfSchema::Object* protectionPairObj = &(protPairObjIter->second) ;
        VOLUME_SETTINGS::PAIR_STATE pairState;
		pairState = (VOLUME_SETTINGS::PAIR_STATE) boost::lexical_cast<int>( protectionPairObj->m_Attrs.find( m_proPairAttrNamesObj.GetPairStateAttrName() )->second ) ;
		if( IsAutoResyncAllowed( protectionPairObj ) &&
            IsResyncRequiredSetToYes( protectionPairObj ) &&
            IsResyncProtectionAllowed ( protectionPairObj, pairState, false ) )
        {
            std::string srcDevName, tgtDevName ;
            srcDevName = protectionPairObj->m_Attrs.find( m_proPairAttrNamesObj.GetSrcNameAttrName())->second ; 
            tgtDevName = protectionPairObj->m_Attrs.find( m_proPairAttrNamesObj.GetTgtNameAttrName())->second ;
            bool Queued = ShouldQueued(protectionPairObj, batchSize) ;
            RestartResyncForPair( protectionPairObj,  Queued ) ;
            AuditConfigPtr auditConfigPtr = AuditConfig::GetInstance() ;
            std::string auditmsg ;
            auditmsg = "Performing the automatic resync for " + srcDevName + " <--> " + tgtDevName ;
            auditConfigPtr->LogAudit(auditmsg) ;

        }
        protPairObjIter++ ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}


bool ProtectionPairConfig::IsPairExists(const std::string& srcDevName,
                                        const std::string& tgtDevName,
                                        const std::string& srcHostId,
                                        const std::string& tgtHostId)
{
    bool bRet = false ;
    ConfSchema::Object* protectionPairObj = NULL ;
    if( GetProtectionPairObj(srcDevName, tgtDevName, &protectionPairObj) )
    {
        bRet = true ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool ProtectionPairConfig::GetRetentionWindowDetailsByTgtVolName(const std::string& tgtDevName,
                                                                 std::string& logsFrom,
                                                                 std::string& logsTo,
                                                                 std::string& logsFromUtc,
                                                                 std::string& logsToUtc,
                                                                 const std::string& srcHostId,
                                                                 const std::string& tgtHostId)
{
    bool bRet = false ;
    ConfSchema::Object* protectionPairObj = NULL ;
    if( GetProtectionPairObjByTgtDevName(tgtDevName, &protectionPairObj) )
    {
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = protectionPairObj->m_Attrs.find( m_proPairAttrNamesObj.GetLogsFromAttrName()) ;
        logsFrom = attrIter ->second ;
        attrIter = protectionPairObj->m_Attrs.find( m_proPairAttrNamesObj.GetLogsToAttrName()) ;
        logsTo = attrIter ->second ;
        attrIter = protectionPairObj->m_Attrs.find( m_proPairAttrNamesObj.GetLogsFromUTCAttrName()) ;
        logsFromUtc = attrIter ->second ;
        attrIter = protectionPairObj->m_Attrs.find( m_proPairAttrNamesObj.GetLogsTOUTCAttrName()) ;
        logsToUtc = attrIter ->second ;
        bRet = true ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool ProtectionPairConfig::GetRetentionWindowDetailsBySrcVolName(const std::string& srcDevName,
                                                                 std::string& logsFrom,
                                                                 std::string& logsTo,
                                                                 std::string& logsFromUtc,
                                                                 std::string& logsToUtc,
                                                                 const std::string& srcHostId,
                                                                 const std::string& tgtHostId)
{
    bool bRet = false ;
    ConfSchema::Object* protectionPairObj = NULL ;
    if( GetProtectionPairObjBySrcDevName(srcDevName, &protectionPairObj) )
    {
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = protectionPairObj->m_Attrs.find( m_proPairAttrNamesObj.GetLogsFromAttrName()) ;
        logsFrom = attrIter ->second ;
        attrIter = protectionPairObj->m_Attrs.find( m_proPairAttrNamesObj.GetLogsToAttrName()) ;
        logsTo = attrIter ->second ;
        attrIter = protectionPairObj->m_Attrs.find( m_proPairAttrNamesObj.GetLogsFromUTCAttrName()) ;
        logsFromUtc = attrIter ->second ;
        attrIter = protectionPairObj->m_Attrs.find( m_proPairAttrNamesObj.GetLogsTOUTCAttrName()) ;
        logsToUtc = attrIter ->second ;
        bRet = true ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool ProtectionPairConfig::GetPairInsertionTime(const std::string& srcDevName,
                                                SV_LONGLONG& pairInsertionTime)
{
    bool bRet = false ;
    ConfSchema::Object* protectionPairObj = NULL ;
    if( GetProtectionPairObjBySrcDevName(srcDevName, &protectionPairObj) )
    {
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = protectionPairObj->m_Attrs.find(m_proPairAttrNamesObj.GetResyncStartTime()) ;
        pairInsertionTime = boost::lexical_cast<SV_LONGLONG>(attrIter->second) ;
        bRet = true ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;

}

bool ProtectionPairConfig::GetRPOInSeconds(const std::string& srcDevName, double& rpoInSec)
{
    bool bRet = false ;
    ConfSchema::Object* protectionPairObj = NULL ;
    if( GetProtectionPairObjBySrcDevName(srcDevName, &protectionPairObj) )
    {
        GetRPOInSeconds( *protectionPairObj, rpoInSec ) ;
        bRet = true ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

void ProtectionPairConfig::GetRPOInSeconds(ConfSchema::Object& protectionPairObj, double& rpoInSec)
{
    ConfSchema::AttrsIter_t attrIter ;
    attrIter = protectionPairObj.m_Attrs.find( m_proPairAttrNamesObj.GetCurrentRPOTimeAttrName()) ;
    rpoInSec = boost::lexical_cast <double> (attrIter->second);

    if( rpoInSec == 0 && IsPairInStepOne( &protectionPairObj ) )
    {
        attrIter = protectionPairObj.m_Attrs.find( m_proPairAttrNamesObj.GetResyncStartTime()) ;
        rpoInSec = boost::lexical_cast<double>(time(NULL) - boost::lexical_cast <time_t> (attrIter->second)) ;
    }        
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;    
}

void ProtectionPairConfig::MarkPairForDeletionPending(ConfSchema::Object* protectionPairObj)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;

    ConfSchema::AttrsIter_t attrIter ;
    attrIter = protectionPairObj->m_Attrs.find( m_proPairAttrNamesObj.GetPairStateAttrName() ) ;
    attrIter->second = boost::lexical_cast<std::string>( VOLUME_SETTINGS::DELETE_PENDING) ;
    attrIter = protectionPairObj->m_Attrs.find( m_proPairAttrNamesObj.GetReplicationCleanupOptionAttrName() ) ;
    attrIter->second = "0000001000" ;
    attrIter = protectionPairObj->m_Attrs.find( m_proPairAttrNamesObj.GetExecutionStateAttrName());
    if (attrIter != protectionPairObj->m_Attrs.end() )
    {
        std::string queuedState = attrIter->second;
        if ( queuedState == "1")
        {
            attrIter->second = "0";
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

bool ProtectionPairConfig::IsCleanupDone( ConfSchema::Object* protectionPairObj)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    ConfSchema::AttrsIter_t attrIter ;
    attrIter = protectionPairObj->m_Attrs.find( m_proPairAttrNamesObj.GetPairStateAttrName()) ;
    VOLUME_SETTINGS::PAIR_STATE pairState ; 
    pairState = (VOLUME_SETTINGS::PAIR_STATE) boost::lexical_cast<int>( attrIter->second ) ;
    DebugPrintf (SV_LOG_DEBUG , "Pair state in IsCleanupDone is %d " , pairState);
    if( pairState == VOLUME_SETTINGS::CLEANUP_DONE )
    {
        attrIter = protectionPairObj->m_Attrs.find( m_proPairAttrNamesObj.GetMarkedForDeletionAttrName()) ;
        if (attrIter != protectionPairObj->m_Attrs.end() )
        {
            attrIter->second= "1" ;
        }
        else 
        {
            protectionPairObj->m_Attrs.insert(std::make_pair(m_proPairAttrNamesObj.GetMarkedForDeletionAttrName(),"1"));
        }
        bRet = true ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

void ProtectionPairConfig::ClearPairs(std::list<std::string> &list)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::list<std::string>::iterator listIter = list.begin();
    std::string auditmsg ;
    ConfSchema::ObjectsIter_t protPairObjIter ;
    while (listIter != list.end() )
    {
        protPairObjIter = find_if(m_ProtectionPairGrp->m_Objects.begin(),
            m_ProtectionPairGrp->m_Objects.end(),
            ConfSchema::EqAttr(m_proPairAttrNamesObj.GetSrcNameAttrName(),
            listIter->c_str())) ;
        if( protPairObjIter != m_ProtectionPairGrp->m_Objects.end() )
        {
            m_ProtectionPairGrp->m_Objects.erase( protPairObjIter ) ;
        }
        listIter++;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return ;
}

bool ProtectionPairConfig::IsCleanupDoneForPairs( )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    ConfSchema::ObjectsIter_t protectionPairObjIter ;
    protectionPairObjIter = m_ProtectionPairGrp->m_Objects.begin() ;
    while( protectionPairObjIter != m_ProtectionPairGrp->m_Objects.end() )
    {
        if( IsCleanupDone(&(protectionPairObjIter->second) ) )
        {
            bRet = true ;
        }
        protectionPairObjIter++ ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

void ProtectionPairConfig::ClearPairs()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    m_ProtectionPairGrp->m_Objects.clear() ;
    AuditConfigPtr auditConfigPtr = AuditConfig::GetInstance() ;
    std::string auditmsg ;
    auditmsg = "Removing the protection pairs." ;
    auditConfigPtr->LogAudit(auditmsg) ;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;

}

std::string ProtectionPairConfig::getPairStatus(ConfSchema::Object& protectionPairObj)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME); 
    std::string PairStatus ;

    int pairstate, replState, volumeresizeStatus , PairQueuedStatus;
    ConfSchema::ProtectionPairObject proppairObj ;
    ConfSchema::AttrsIter_t attrIter ;
    attrIter = protectionPairObj.m_Attrs.find(proppairObj.GetReplStateAttrName()) ;
    if( protectionPairObj.m_Attrs.end() != attrIter )
    {
        replState = boost::lexical_cast<int>(attrIter->second) ;
    }
    attrIter = protectionPairObj.m_Attrs.find(proppairObj.GetPairStateAttrName()) ;
    if( protectionPairObj.m_Attrs.end() != attrIter )
    {
        pairstate = boost::lexical_cast<int>(attrIter->second) ;
    }
    attrIter = protectionPairObj.m_Attrs.find(proppairObj.GetExecutionStateAttrName()) ;
    if( protectionPairObj.m_Attrs.end() != attrIter )
    {
        PairQueuedStatus = boost::lexical_cast<int>(attrIter->second) ;
    }

    attrIter = protectionPairObj.m_Attrs.find(proppairObj.GetVolumeResizedAttrName()) ;
    if( attrIter != protectionPairObj.m_Attrs.end() )
    {
        volumeresizeStatus = boost::lexical_cast<int>(attrIter->second) ;
    }
    else
    {
        volumeresizeStatus = VOLUME_SIZE_NORMAL ;
    }
    std::string replicationState, pairState, resizeStatus, QueuedStatus ;
    switch( replState )
    {
    case 0 :
        replicationState = PAIR_DIFF_SYNC;
        break ;
    case 5 :
        replicationState = PAIR_RESYNC_STEP2;
        break ;
    case 7 :
        replicationState = PAIR_RESYNC_STEP1;
        break ;
    }
    switch( pairstate )
    {
    case VOLUME_SETTINGS::PAUSE_PENDING:
        pairState = PAIR_PAUSE_PENDING ;
        break ;
    case VOLUME_SETTINGS::RESUME_PENDING:
        pairState = PAIR_RESUME_PENDING ;
        break ;
    case VOLUME_SETTINGS::PAUSE:
        pairState = PAIR_PAUSED ;		
        break ;
    case VOLUME_SETTINGS::PAUSE_TACK_PENDING:
        pairState =  PAIR_PAUSE_TRACKING_PENDING ;
        break ;
    case VOLUME_SETTINGS::PAUSE_TACK_COMPLETED:
        pairState = PAIR_PAUSE_TRACKING_COMPLETED ;
        break ;
    case VOLUME_SETTINGS::DELETE_PENDING:
        pairState = PAIR_DELETE_PENDING  ;
        break ;
    case VOLUME_SETTINGS::CLEANUP_DONE:
        pairState = PAIR_CLEANUP_DONE ;
        break ;
    }
    switch( volumeresizeStatus )
    {
    case VOLUME_RESIZE_PENDING:
    case VOLUME_RESIZE_SHRINK:
    case VOLUME_RESIZE_NOFREESPACE:
    case VOLUME_RESIZE_FAILED:
    case VOLUME_RESIZE_COMPLETED:
        resizeStatus = "Volume Resized" ;
        break ;
    }
    switch( PairQueuedStatus )
    {
    case PAIR_IS_QUEUED:
        QueuedStatus = PAIR_QUEUED ;
    default :
        QueuedStatus = "" ;
    }
    PairStatus = replicationState ;
    PairStatus += " " ;
    if( !pairState.empty() )
    {
        PairStatus += "(" ;
        PairStatus += pairState ;
        PairStatus += ") " ;
    }
    if( PairQueuedStatus == PAIR_IS_QUEUED )
    {
        if( replState == 7 )
        {
            PairStatus = PAIR_QUEUED ;
        }
        else
        {
            PairStatus += "(" ;
            PairStatus += PAIR_QUEUED ; 
            PairStatus +=  ") ";
        }
    }
    if( resizeStatus.compare("Volume Resized") == 0 )
    {
        PairStatus  += "(" ;
        PairStatus  += resizeStatus ;
        PairStatus  += ")" ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return PairStatus ;

}

void ProtectionPairConfig::GetPairState(const std::string& srcDevName,
                                        std::string& PairState,
                                        const std::string& tgtDevName,
                                        const std::string& srcHostId,
                                        const std::string& tgtHostId)
{
    bool bRet = false ;
    ConfSchema::Object* protectionPairObj = NULL ;
    if( !srcDevName.empty() && !tgtDevName.empty() )
    {
        bRet = GetProtectionPairObj( srcDevName, tgtDevName, &protectionPairObj ) ;
    }
    else if( !srcDevName.empty() )
    {
        bRet = GetProtectionPairObjBySrcDevName( srcDevName, &protectionPairObj ) ;
    }
    else if( !tgtDevName.empty() )
    {
        bRet = GetProtectionPairObjByTgtDevName( tgtDevName, &protectionPairObj ) ;
    }
    if( bRet )
    {
        PairState = getPairStatus(*protectionPairObj) ;
    }
}


void ProtectionPairConfig::GetRepositoryNameAndPath(const std::string& srcVolName,
                                                    std::string& RepositoryName,
                                                    std::string& RepositoryPath,
                                                    const std::string& tgtVolName,
                                                    const std::string& srcHostId,
                                                    const std::string& tgtHostId)
{
    VolumeConfigPtr volumeConfigPtr = VolumeConfig::GetInstance() ;
    volumeConfigPtr->GetRepositoryNameAndPath(RepositoryName, RepositoryPath) ;
}

std::string ProtectionPairConfig::ConvertRPOToHumanReadableformat( double& currentRPO )
{
    std::string currentRpoStr ;

    char number [100];
	if (currentRPO < 0.00 )
	{
		currentRpoStr = "N/A";	
	}
	else 
	{
		if (currentRPO >= 0.00 && currentRPO <= 7200.00 )
		{
			inm_sprintf_s(number, ARRAYSIZE(number), "%.2lf", (double) currentRPO / ( 60.00 ) );
			currentRpoStr = boost::lexical_cast <std::string> (number);
			currentRpoStr += " minutes"; 
		}
		else if (currentRPO > 7200.00 && currentRPO <= 172800.00 )
		{
			inm_sprintf_s(number, ARRAYSIZE(number), "%.2lf", (double)currentRPO / (60.00 * 60.00));
			currentRpoStr = boost::lexical_cast <std::string> (number);
			currentRpoStr += " hrs"; 
		}
		else if (currentRPO > 172800.00 && currentRPO <= 1209600.00 )
		{
			inm_sprintf_s(number, ARRAYSIZE(number), "%.2lf", (double)currentRPO / (60.00 * 60.00 * 24.00) );
			currentRpoStr = boost::lexical_cast <std::string> (number);
			currentRpoStr += " days";
		}
		else 
		{
			inm_sprintf_s(number, ARRAYSIZE(number), "%.2lf", (double)currentRPO / (60.00 * 60.00 * 24.00 * 7.00 ) );
			currentRpoStr = boost::lexical_cast <std::string> (number);
			currentRpoStr += " weeks";
		}
	}

    return currentRpoStr ;
}

bool ProtectionPairConfig::IsValidRPO(ConfSchema::Object* protectionPairObj)
{
	bool ret = true ;
	ConfSchema::AttrsIter_t attrIter  ;
	attrIter = protectionPairObj->m_Attrs.find( m_proPairAttrNamesObj.GetIsValidRPOAttrName() ); 
	if( attrIter != protectionPairObj->m_Attrs.end() && attrIter->second == "0" )
	{
		DebugPrintf(SV_LOG_WARNING, "This is not a valid RPO\n") ;
		ret = false ;
	}
	return ret ;
}

void ProtectionPairConfig::NotValidRPO(const std::string& srcDevName)
{
	ConfSchema::Object* protectionPairObj = NULL ;
    GetProtectionPairObjBySrcDevName( srcDevName, &protectionPairObj ) ;
	if( protectionPairObj != NULL )
	{
		protectionPairObj->m_Attrs.insert( std::make_pair(m_proPairAttrNamesObj.GetIsValidRPOAttrName(), "0" ) ) ;
		ConfSchema::AttrsIter_t attrIter  ;
		attrIter = protectionPairObj->m_Attrs.find( m_proPairAttrNamesObj.GetIsValidRPOAttrName() ); 
		attrIter->second = "0" ;
	}
	
}
std::string ProtectionPairConfig::GetCurrentRPO(ConfSchema::Object& protectionPairObj) 
{   
    std::string currentRpoStr ;
	if(!IsValidRPO(&protectionPairObj) )
	{
		currentRpoStr = "N/A" ;
	}
	else
	{
		if(! (IsPairInStepOne( &protectionPairObj ) && IsPairInQueuedState(&protectionPairObj) ) )
		{

			double rpoInSec = 0 ;
			GetRPOInSeconds( protectionPairObj, rpoInSec ) ;
			currentRpoStr = ConvertRPOToHumanReadableformat( rpoInSec ) ;
		}
		else
		{
			currentRpoStr = "N/A" ;
		}
	}
	return currentRpoStr ;
}

void ProtectionPairConfig::GetCurrentRPO(const std::string& srcDevName,
                                         std::string& rpoInHumanRdblFmt,
                                         const std::string& tgtDevName,
                                         const std::string& srcHostId,
                                         const std::string& tgtHostId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    ConfSchema::Object* protectionPairObj = NULL ;
    if( !srcDevName.empty() && !tgtDevName.empty() )
    {
        bRet = GetProtectionPairObj( srcDevName, tgtDevName, &protectionPairObj ) ;
    }
    else if( !srcDevName.empty() )
    {
        bRet = GetProtectionPairObjBySrcDevName( srcDevName, &protectionPairObj ) ;
    }
    else if( !tgtDevName.empty() )
    {
        bRet = GetProtectionPairObjByTgtDevName( tgtDevName, &protectionPairObj ) ;
    }
    if( bRet )
    {
        rpoInHumanRdblFmt = GetCurrentRPO(*protectionPairObj) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

std::string ProtectionPairConfig::GetResyncProgress(ConfSchema::Object& protectionPairObj)
{
    double resyncProgress ;
    std::string resyncProgressStr ;
    if( IsPairInStepOne(&protectionPairObj) && !IsPairInQueuedState(&protectionPairObj) )
    {
        VolumeConfigPtr volumeConfig = VolumeConfig::GetInstance() ;
        ConfSchema::AttrsIter_t attrIter ;
        SV_LONGLONG currentOffset, capacity, freespace, fsusedbytes, fsunusedbytes, copiedbytes ;
		attrIter = protectionPairObj.m_Attrs.find( m_proPairAttrNamesObj.GetSrcNameAttrName() ) ;
		std::string srcName = attrIter->second ;
		std::string modifiedVolume = srcName; 
		if (modifiedVolume.length() == 1 )
		{
			modifiedVolume += ":\\";
		}
		else 
		{
			modifiedVolume += "\\";
		}
		SV_ULONGLONG volumeCapacity,quota,volumeFreeSpace;
		DebugPrintf (SV_LOG_DEBUG , "modifiedVolume is %s ", modifiedVolume.c_str() );
		GetDiskFreeSpaceP (modifiedVolume ,&quota ,&volumeCapacity,&volumeFreeSpace);
		//volumeConfig->GetfreeSpace(srcName, freespace) ;
		DebugPrintf(SV_LOG_DEBUG, "freespace %s\n",boost::lexical_cast<std::string>(volumeFreeSpace).c_str() ) ;
		freespace = volumeFreeSpace;
        attrIter = protectionPairObj.m_Attrs.find( m_proPairAttrNamesObj.GetSrcVolCapacityAttrName() ) ;
		capacity = boost::lexical_cast<SV_LONGLONG>(attrIter->second) ;
		DebugPrintf(SV_LOG_DEBUG, "capacity %s\n",attrIter->second.c_str() ) ;
		attrIter = protectionPairObj.m_Attrs.find( m_proPairAttrNamesObj.GetFsUnusedBytes()) ;	
		fsunusedbytes = boost::lexical_cast<SV_LONGLONG>(attrIter->second) ;
		DebugPrintf(SV_LOG_DEBUG, "fsunusedbytes %s\n",attrIter->second.c_str() ) ;
		attrIter = protectionPairObj.m_Attrs.find( m_proPairAttrNamesObj.GetLastOffsetAttrName() ) ;
		currentOffset = boost::lexical_cast<SV_LONGLONG>(attrIter->second) ;
		DebugPrintf(SV_LOG_DEBUG, "current offset %s\n",attrIter->second.c_str() ) ;
		fsusedbytes = capacity - freespace ; //143022616576 - 59394310144 = 83628306432
		copiedbytes = currentOffset - fsunusedbytes ;//27890384896 - 121167872 = 27769217024
		resyncProgress = boost::lexical_cast<double>(copiedbytes) / 
            (boost::lexical_cast<double>(fsusedbytes)) ;
        DebugPrintf(SV_LOG_DEBUG, "progress %f\n",resyncProgress) ;
		resyncProgress *= 100 ;
		if( resyncProgress < 1 )
		{
			resyncProgress = 0 ;
		}
		if (resyncProgress >= 100 )
		{
			resyncProgress = 100 ;
		}
		resyncProgressStr = boost::lexical_cast<std::string>(resyncProgress) ;
		size_t pos;
		pos = resyncProgressStr.find_first_of('.');
		if(pos != std::string::npos)
		{
			resyncProgressStr = resyncProgressStr.substr(0,pos+3);
		}
		resyncProgressStr += "%" ;
		DebugPrintf(SV_LOG_DEBUG, "resync progress %s\n", resyncProgressStr.c_str()) ;
    }
    else
    {
        resyncProgressStr = "N/A" ;
    }
    return resyncProgressStr ;
}


void ProtectionPairConfig::GetResyncProgress(const std::string& srcDevName,
                                             std::string& resyncProgress,
                                             const std::string& tgtDevName,
                                             const std::string& srcHostId,
                                             const std::string& tgtHostId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    ConfSchema::Object* protectionPairObj = NULL ;
    if( !srcDevName.empty() && !tgtDevName.empty() )
    {
        bRet = GetProtectionPairObj( srcDevName, tgtDevName, &protectionPairObj ) ;
    }
    else if( !srcDevName.empty() )
    {
        bRet = GetProtectionPairObjBySrcDevName( srcDevName, &protectionPairObj ) ;
    }
    else if( !tgtDevName.empty() )
    {
        bRet = GetProtectionPairObjByTgtDevName( tgtDevName, &protectionPairObj ) ;
    }
    if( bRet )
    {
        resyncProgress = GetResyncProgress(*protectionPairObj) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void ProtectionPairConfig::GetEtlForResync(const std::string& srcDevName,
										   std::string& resyncEtl,
                                           const std::string& tgtDevName,
                                           const std::string& srcHostId,
                                           const std::string& tgtHostId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;\
    ConfSchema::Object* protectionPairObj = NULL ;
    if( !srcDevName.empty() && !tgtDevName.empty() )
    {
        bRet = GetProtectionPairObj( srcDevName, tgtDevName, &protectionPairObj ) ;
    }
    else if( !srcDevName.empty() )
    {
        bRet = GetProtectionPairObjBySrcDevName( srcDevName, &protectionPairObj ) ;
    }
    else if( !tgtDevName.empty() )
    {
        bRet = GetProtectionPairObjByTgtDevName( tgtDevName, &protectionPairObj ) ;
    }
    if( bRet )
    {
		std::string resyncStr ;
		resyncStr = GetResyncProgress(*protectionPairObj) ;
		DebugPrintf(SV_LOG_DEBUG, "resync progress %s\n",resyncStr.c_str() ) ;
		if( resyncStr != "N/A" && resyncStr != "0%" )
		{
			ConfSchema::AttrsIter_t attrIter ;
			attrIter = protectionPairObj->m_Attrs.find( m_proPairAttrNamesObj.GetResyncStartTime() ) ;
			SV_LONGLONG resyncstarttime, currenttime ;
			resyncstarttime = boost::lexical_cast<SV_LONGLONG>(attrIter ->second) ;
			currenttime = time(NULL) ;
			resyncStr = resyncStr.substr(0,resyncStr.find('%')) ; 
			DebugPrintf(SV_LOG_DEBUG, "resync progress %s\n",resyncStr.c_str() ) ;
			int resyncProgress = atoi(resyncStr.c_str()) ;
			double etl = boost::lexical_cast<double>((( 100 - resyncProgress ) * ( currenttime - resyncstarttime))/resyncProgress) ;
			DebugPrintf(SV_LOG_DEBUG, "etl %s\n", boost::lexical_cast<std::string>(etl).c_str()) ;
			resyncEtl = ConvertRPOToHumanReadableformat( etl ) ;
		}
		else
		{
			resyncEtl = "N/A" ;
		}
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;

}
std::string ProtectionPairConfig::GetResyncRequiredState(ConfSchema::Object& protectionPairObj)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::string str ;
    if( IsPairInQueuedState(&protectionPairObj) && IsPairInStepOne(&protectionPairObj) )
    {
        str = "N/A" ;
    }
    else if( IsPairInStepOne(&protectionPairObj) )
    {
        str = "Yes" ;
    }
    else if (IsPairInStepTwo(&protectionPairObj) )
    {
        ConfSchema::AttrsIter_t attrIter = protectionPairObj.m_Attrs.find(m_proPairAttrNamesObj.GetShouldResyncSetFromAttrName()) ;
        if( attrIter->second.compare("") == 0 )
        {
            str = "N/A" ;
        }
        else
        {
            str = "Yes" ;
        }
    }
    else
    {
        ConfSchema::AttrsIter_t attrIter = protectionPairObj.m_Attrs.find(m_proPairAttrNamesObj.GetShouldResyncSetFromAttrName()) ;
        if( attrIter->second.compare("") == 0 )
        {
            str = "No" ;
        }
        else
        {
            str = "Yes" ;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return str ;
}


void ProtectionPairConfig::GetResyncRequiredState(const std::string& srcDevName,
                                                  std::string& resyncRequiredState,
                                                  const std::string& tgtDevName,
                                                  const std::string& srcHostId,
                                                  const std::string& tgtHostId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    ConfSchema::Object* protectionPairObj = NULL ;
    if( !srcDevName.empty() && !tgtDevName.empty() )
    {
        bRet = GetProtectionPairObj( srcDevName, tgtDevName, &protectionPairObj ) ;
    }
    else if( !srcDevName.empty() )
    {
        bRet = GetProtectionPairObjBySrcDevName( srcDevName, &protectionPairObj ) ;
    }
    else if( !tgtDevName.empty() )
    {
        bRet = GetProtectionPairObjByTgtDevName( tgtDevName, &protectionPairObj ) ;
    }
    if( bRet )
    {
        resyncRequiredState = GetResyncRequiredState(*protectionPairObj) ;
    }
}

bool ProtectionPairConfig::GetTargetVolumeBySrcVolumeName(const std::string& srcDevName,
                                                          std::string& tgtDevName,
                                                          const std::string& srcHostId,
                                                          const std::string& tgtHostId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    ConfSchema::Object* protectionPairObj  ;
    if( GetProtectionPairObjBySrcDevName( srcDevName, &protectionPairObj ) )
    {
        bRet = true ;
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = protectionPairObj->m_Attrs.find( m_proPairAttrNamesObj.GetTgtNameAttrName()) ;
        tgtDevName = attrIter->second ;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool ProtectionPairConfig::GetRetentionEventPolicies(ConfSchema::Object* retPolObject,
                                                     std::list<ConfSchema::Object>& retEvntPolicyObjs)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    ConfSchema::GroupsIter_t grpIter ;
    grpIter = retPolObject->m_Groups.find( m_retWindowAttrNamesObj.GetName() ) ;
    if( grpIter != retPolObject->m_Groups.end() )
    {
        ConfSchema::Group& retwindowGrp = grpIter->second ;
        ConfSchema::ObjectsIter_t objIter = retwindowGrp.m_Objects.begin() ;
        while( objIter != retwindowGrp.m_Objects.end() )
        {
            grpIter = objIter->second.m_Groups.find( m_retevtPolicyAttrNamesObj.GetName() ) ;
            if( grpIter != objIter->second.m_Groups.end() )
            {
                ConfSchema::Group& retEvtPoliciesGrp = grpIter->second ;
                if( retEvtPoliciesGrp.m_Objects.size() )
                {
                    bRet = true ;
                    ConfSchema::Object& retevntPolicyObj = retEvtPoliciesGrp.m_Objects.begin()->second ;
                    retEvntPolicyObjs.push_back(retevntPolicyObj) ;
                }
            }
            objIter++ ;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool ProtectionPairConfig::GetRetentionPath(std::map<std::string, std::string>& srcVolRetentionPathMap)
{
	ConfSchema::ObjectsIter_t objiter = m_ProtectionPairGrp->m_Objects.begin() ;
	while( objiter != m_ProtectionPairGrp->m_Objects.end() )
	{
		std::string srcvol ;
		std::string retentionpath ;
		ConfSchema::AttrsIter_t attrIter = objiter->second.m_Attrs.find( m_proPairAttrNamesObj.GetSrcNameAttrName()) ;
		if( attrIter != objiter->second.m_Attrs.end() )
		{
			srcvol = attrIter->second ;
			if(GetRetentionPathBySrcDevName(srcvol, retentionpath))
			{
				srcVolRetentionPathMap.insert(std::make_pair( srcvol, retentionpath)) ;
			}
		}
		objiter ++ ;
	}
	return true ;
}

bool ProtectionPairConfig::GetRetentionPath(ConfSchema::Object* protectionPairObj, 
                                            std::string& retentionPath)
{
    ConfSchema::Object* retPolObject ;
    bool bRet = false ;
    if( GetRetentionLogPolicyObject(protectionPairObj, &retPolObject) )
    {
        std::list<ConfSchema::Object> retEvtPolicies ;
        if( GetRetentionEventPolicies(retPolObject, retEvtPolicies ) )
        {
            if( retEvtPolicies.size() )
            {
                bRet = true ;
                ConfSchema::Object& retEvtPolicyObj = *(retEvtPolicies.begin()) ;
                ConfSchema::AttrsIter_t attrIter ;
                attrIter = retEvtPolicyObj.m_Attrs.find( m_retevtPolicyAttrNamesObj.GetCatalogPathAttrName()) ;
                retentionPath = attrIter->second ;
            }
        }
    }
    return bRet ;
}

bool ProtectionPairConfig::GetSourceVolumeRetentionPathAndVolume( ConfSchema::Object* protectionPairObj, std::string& srcVolumeName, std::string& retentionVolume, std::string& retentionPath )
{
    ConfSchema::Object* retPolObject ;
    ConfSchema::AttrsIter_t attrIter ;
    bool bRet = false ;
    attrIter = protectionPairObj->m_Attrs.find( m_proPairAttrNamesObj.GetSrcNameAttrName()) ; 
    if( attrIter != protectionPairObj->m_Attrs.end() )
    {
        srcVolumeName = attrIter->second ;
        if( GetRetentionLogPolicyObject(protectionPairObj, &retPolObject) )
        {
            std::list<ConfSchema::Object> retEvtPolicies ;
            if( GetRetentionEventPolicies(retPolObject, retEvtPolicies ) )
            {
                if( retEvtPolicies.size() )
                {
                    bRet = true ;
                    ConfSchema::Object& retEvtPolicyObj = *(retEvtPolicies.begin()) ;

                    attrIter = retEvtPolicyObj.m_Attrs.find( m_retevtPolicyAttrNamesObj.GetStorageDeviceAttrName()) ;
                    retentionVolume = attrIter->second ;

                    attrIter = retEvtPolicyObj.m_Attrs.find( m_retevtPolicyAttrNamesObj.GetStoragePathAttrName()) ;
                    retentionPath = attrIter->second ;
                }
            }
        }
    }
    return bRet ;
}

bool ProtectionPairConfig::GetRetentionPathByTgtDevName(const std::string& tgtDevName,
                                                        std::string& retentionPath,
                                                        const std::string& srcHostId,
                                                        const std::string& tgtHostId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    ConfSchema::Object* protectionPairObj ;
    if( GetProtectionPairObjByTgtDevName(tgtDevName, &protectionPairObj) )
    {
        if( GetRetentionPath( protectionPairObj, retentionPath ) )
        {
            bRet = true ;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool ProtectionPairConfig::GetRetentionPathBySrcDevName(const std::string& srcDevName,
                                                        std::string& retentionPath,
                                                        const std::string& srcHostId,
                                                        const std::string& tgtHostId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    ConfSchema::Object* protectionPairObj ;
    if( GetProtectionPairObjBySrcDevName(srcDevName, &protectionPairObj) )
    {
        if( GetRetentionPath( protectionPairObj, retentionPath ) )
        {
            bRet = true ;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool ProtectionPairConfig::MarkPairAsResized(const std::string& srcDevName,
                                             const std::string& tgtDevName,
                                             const std::string& srcHostId,
                                             const std::string& tgtHostId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    ConfSchema::Object* protectionPairObj ;
    if( GetProtectionPairObjBySrcDevName(srcDevName, &protectionPairObj) )
    {
        bRet = true ;
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = protectionPairObj->m_Attrs.find( m_proPairAttrNamesObj.GetVolumeResizedAttrName() ) ;
        attrIter->second = boost::lexical_cast<std::string>(VOLUME_RESIZE_PENDING) ;
        std::string errMsg = " Volume Resized on " ;
        errMsg += Host::GetInstance().GetHostName();
        errMsg += " => ";
        errMsg += srcDevName;
        AlertConfigPtr alertConfigPtr = AlertConfig::GetInstance() ;
        alertConfigPtr->AddAlert("CRITICAL", "CX", SV_ALERT_SIMPLE, SV_ALERT_MODULE_RESYNC, errMsg) ;
        SetResyncRequiredBySource(srcDevName, "Resync Required for " + srcDevName ) ;
        AuditConfigPtr auditConfigPtr = AuditConfig::GetInstance() ;
        std::string auditmsg ;
        auditmsg = "Marking the volume " + srcDevName + " as resized." ;
        auditConfigPtr->LogAudit(auditmsg) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool ProtectionPairConfig::IsPairMarkedAsResized(const std::string& srcDevName,
                                                 const std::string& tgtDevName,
                                                 const std::string& srcHostId,
                                                 const std::string& tgtHostId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = true ;
    ConfSchema::Object* protectionPairObj ;
    if( GetProtectionPairObjBySrcDevName(srcDevName, &protectionPairObj) )
    {
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = protectionPairObj->m_Attrs.find( m_proPairAttrNamesObj.GetVolumeResizedAttrName() ) ;
        eVolumeResize resizeStatus ;
        resizeStatus = (eVolumeResize) boost::lexical_cast<int>(attrIter->second) ;
        if( resizeStatus == VOLUME_SIZE_NORMAL )
        {
            bRet = false ;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}


bool ProtectionPairConfig::IsProtectedAsSrcVolume(const std::string& srcDevName,
                                                  const std::string& srcHostId,
                                                  const std::string& tgtHostId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::list<std::string> volumes ;
    bool bret = false ;
    GetProtectedVolumes(volumes) ;
    if( std::find( volumes.begin(), volumes.end(), srcDevName) != volumes.end() )
    {
        bret = true ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bret ;
}

bool ProtectionPairConfig::IsProtectedAsTgtVolume(const std::string& tgtDevName,
                                                  const std::string& srcHostId,
                                                  const std::string& tgtHostId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::list<std::string> tgtVolumes ;
    bool bret = false ;
    GetProtectedTargetVolumes(tgtVolumes) ;
    if( std::find( tgtVolumes.begin(), tgtVolumes.end(), tgtDevName) != tgtVolumes.end() )
    {
        bret = true ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bret; 
}


void ProtectionPairConfig::UpdateCDPRetentionDiskUsage( const std::string& tgtVolume, SV_ULONGLONG startTs, 
                                                       SV_ULONGLONG endTs, SV_ULONGLONG sizeOnDisk, 
                                                       SV_ULONGLONG spaceSavedByCompression, 
                                                       SV_ULONGLONG spaceSavedBySparsePolicy, 
                                                       SV_UINT numFiles)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ConfSchema::Object* protectionPairObj; 
    if( GetProtectionPairObjByTgtDevName(tgtVolume, &protectionPairObj) )
    {
        UpdateCurrentLogSize( protectionPairObj, sizeOnDisk) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void ProtectionPairConfig::ChangePaths( const std::string& existingRepo, const std::string& newRepo ) 
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ConfSchema::Object* retPolObject ;
    bool bRet = false ;
    ConfSchema::ObjectsIter_t protectionPairObjIter = m_ProtectionPairGrp->m_Objects.begin() ;
    while( protectionPairObjIter != m_ProtectionPairGrp->m_Objects.end() )
    {
        if( GetRetentionLogPolicyObject(&(protectionPairObjIter->second), &retPolObject) )
        {
            ConfSchema::AttrsIter_t attrIter ;
            /*attrIter = protectionPairObjIter->second.m_Attrs.find( m_proPairAttrNamesObj.GetTgtNameAttrName() ) ;
            std::string currentTgtPath = attrIter->second ;
            std::string newTgtPath = newRepo ;
            newTgtPath += currentTgtPath.substr( existingRepo.length() ) ;
            attrIter->second = newTgtPath ;
            */
            ConfSchema::GroupsIter_t grpIter ;
            grpIter = retPolObject->m_Groups.find( m_retWindowAttrNamesObj.GetName() ) ;
            if( grpIter != retPolObject->m_Groups.end() )
            {
                ConfSchema::Group& retwindowGrp = grpIter->second ;
                ConfSchema::ObjectsIter_t objIter = retwindowGrp.m_Objects.begin() ;
                while( objIter != retwindowGrp.m_Objects.end() )
                {
                    grpIter = objIter->second.m_Groups.find( m_retevtPolicyAttrNamesObj.GetName() ) ;
                    if( grpIter != objIter->second.m_Groups.end() )
                    {
                        ConfSchema::Group& retEvtPoliciesGrp = grpIter->second ;
                        if( retEvtPoliciesGrp.m_Objects.size() )
                        {
                            ConfSchema::ObjectsIter_t retEvntpolicyObjIter ;
                            retEvntpolicyObjIter = retEvtPoliciesGrp.m_Objects.begin() ;
                            while( retEvntpolicyObjIter != retEvtPoliciesGrp.m_Objects.end() )
                            {
                                ConfSchema::Object& retevntPolicyObj = retEvtPoliciesGrp.m_Objects.begin()->second ;
                                attrIter = retevntPolicyObj.m_Attrs.find( m_retevtPolicyAttrNamesObj.GetCatalogPathAttrName() ) ;
                                std::string currentCatlogPath = attrIter->second ;
                                std::string newcatalogpath = newRepo ;
                                newcatalogpath += currentCatlogPath.substr( existingRepo.length() ) ;
                                attrIter->second = newcatalogpath ;
                                attrIter = retevntPolicyObj.m_Attrs.find( m_retevtPolicyAttrNamesObj.GetStoragePathAttrName() ) ;
                                std::string currentStoragePath = attrIter->second ;
                                std::string newStoragePath = newRepo ;
                                newStoragePath += currentStoragePath.substr( existingRepo.length() ) ;
                                attrIter->second = newStoragePath ;
                                attrIter = retevntPolicyObj.m_Attrs.find( m_retevtPolicyAttrNamesObj.GetStorageDeviceAttrName() ) ;
                                attrIter->second = newRepo ;
                                retEvntpolicyObjIter++ ;
                            }
                        }
                    }
                    objIter++ ;
                }
            }
        }
        protectionPairObjIter++ ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

bool ProtectionPairConfig::IsDeletionInProgress(const std::string& volName)
{
    ConfSchema::Object* protectionPairObj; 
    bool bRet = false ;
    if( GetProtectionPairObjBySrcDevName(volName, &protectionPairObj) )
    {
        ConfSchema::AttrsIter_t attrIter = protectionPairObj->m_Attrs.find( m_proPairAttrNamesObj.GetPairStateAttrName() ) ;
        int pairState = boost::lexical_cast <int>( attrIter->second );
        if( pairState == VOLUME_SETTINGS::DELETE_PENDING || pairState == VOLUME_SETTINGS::CLEANUP_DONE)
        {
            bRet = true ;
        }
    }
    return bRet ;
}

void ProtectionPairConfig::GetVolumesMarkedForDeletion(std::list <std::string> &list)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ConfSchema::ObjectsIter_t protectionPairObjIter ;
    protectionPairObjIter = m_ProtectionPairGrp->m_Objects.begin() ;
    while( protectionPairObjIter != m_ProtectionPairGrp->m_Objects.end() )
    {
        ConfSchema::AttrsIter_t attrIter = protectionPairObjIter->second.m_Attrs.find(m_proPairAttrNamesObj.GetMarkedForDeletionAttrName());
        if (attrIter != protectionPairObjIter->second.m_Attrs.end() )
        {
            DebugPrintf("GetMarkedForDeletionAttrName value is %s \n ",attrIter->second.c_str() ) ;
            if (attrIter->second == "1")
            {
                attrIter = protectionPairObjIter->second.m_Attrs.find(m_proPairAttrNamesObj.GetSrcNameAttrName());
                if (attrIter != protectionPairObjIter->second.m_Attrs.end () )
                {
                    list.push_back (attrIter->second) ; 
                }
            }
        }
        protectionPairObjIter++ ;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return; 
}

bool ProtectionPairConfig::GetSourceListFromTargetList (std::list <std::string> &targetVolumes , std::list<std::string> &sourceVolumes)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    int count = 0 ;
    std::list<std::string>::iterator targetVolumesIter = targetVolumes.begin();
    while (targetVolumesIter  != targetVolumes.end() )
    {
        ConfSchema::ObjectsIter_t protectionPairObjIter ;
        protectionPairObjIter = m_ProtectionPairGrp->m_Objects.begin() ;
        while( protectionPairObjIter != m_ProtectionPairGrp->m_Objects.end() )
        {
            ConfSchema::AttrsIter_t attrIter = protectionPairObjIter->second.m_Attrs.find(m_proPairAttrNamesObj.GetTgtNameAttrName());
            if (attrIter->second == *targetVolumesIter)
            {
                attrIter = protectionPairObjIter->second.m_Attrs.find(m_proPairAttrNamesObj.GetSrcNameAttrName());
                if (attrIter != protectionPairObjIter->second.m_Attrs.end () )
                {
                    DebugPrintf("attrIter is %s  \n ", attrIter->second.c_str() ) ;
                    sourceVolumes.push_back (attrIter->second) ; 
                    count ++; 
                }
            }
            protectionPairObjIter++ ;
        }
        targetVolumesIter++;
    }
    if (count == targetVolumes.size () )
        bRet = true;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet; 

}
void ProtectionPairConfig::MarkPairsForDeletionPending(std::string &volumeName)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ConfSchema::ObjectsIter_t protectionPairObjIter ;
    protectionPairObjIter = m_ProtectionPairGrp->m_Objects.begin() ;
    while( protectionPairObjIter != m_ProtectionPairGrp->m_Objects.end() )
    {
        ConfSchema::AttrsIter_t attrIter = protectionPairObjIter->second.m_Attrs.find(m_proPairAttrNamesObj.GetSrcNameAttrName());
        if (attrIter->second == volumeName )
        {
            MarkPairForDeletionPending( &(protectionPairObjIter->second) ) ;
            ActOnQueuedPair() ;	
            AuditConfigPtr auditConfigPtr = AuditConfig::GetInstance() ;
            std::string auditmsg ;
            std::string srcDevName = protectionPairObjIter->second.m_Attrs.find( m_proPairAttrNamesObj.GetSrcNameAttrName() )->second ;
            auditmsg = "Marking the protection of volume " + srcDevName + " for deletion" ;
            auditConfigPtr->LogAudit(auditmsg) ;
        }
        protectionPairObjIter++ ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
void ProtectionPairConfig::MarkPairsForDeletionPending(std::list <std::string> &list)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::list <std::string>::iterator listIter = list.begin ();
    while (listIter != list.end() )
    {
        ConfSchema::ObjectsIter_t protectionPairObjIter ;
        protectionPairObjIter = m_ProtectionPairGrp->m_Objects.begin() ;
        while( protectionPairObjIter != m_ProtectionPairGrp->m_Objects.end() )
        {
            ConfSchema::AttrsIter_t attrIter = protectionPairObjIter->second.m_Attrs.find(m_proPairAttrNamesObj.GetSrcNameAttrName());
            if (attrIter->second == *listIter )
            {
                MarkPairForDeletionPending( &(protectionPairObjIter->second) ) ;
                ActOnQueuedPair() ;	
                AuditConfigPtr auditConfigPtr = AuditConfig::GetInstance() ;
                std::string auditmsg ;
                std::string srcDevName = protectionPairObjIter->second.m_Attrs.find( m_proPairAttrNamesObj.GetSrcNameAttrName() )->second ;
                auditmsg = "Marking the protection of volume " + srcDevName + " for deletion" ;
                auditConfigPtr->LogAudit(auditmsg) ;
            }
            protectionPairObjIter++ ;
        }
        listIter ++;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void ProtectionPairConfig::ResetValues(std::string& srcVol,
                                       std::string& tgtVol,
                                       VOLUME_SETTINGS::PAIR_STATE pairState,
                                       bool resyncRequiredFlag,
                                       int resyncRequiredCause,
                                       int syncMode,
                                       std::string& jobId,
                                       SV_ULONGLONG resyncStartTagTime,
                                       SV_ULONGLONG resyncStartSeqNo,
                                       SV_ULONGLONG resyncEndTagTime,
                                       SV_ULONGLONG resyncEndSeqNo,
                                       std::string& retentionPath)
{
    ConfSchema::ObjectsIter_t protectionPairIter ;
    protectionPairIter = find_if( m_ProtectionPairGrp->m_Objects.begin(),
        m_ProtectionPairGrp->m_Objects.end(),
        ConfSchema::EqAttr( m_proPairAttrNamesObj.GetSrcNameAttrName(), srcVol.c_str())) ;
    if( protectionPairIter == m_ProtectionPairGrp->m_Objects.end() )
    {
        DebugPrintf(SV_LOG_ERROR, "Cannot create the protection pairsettings for the pair with source volume %s\n",srcVol.c_str()) ;
    }
    else
    {
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = protectionPairIter->second.m_Attrs.find( m_proPairAttrNamesObj.GetPairStateAttrName() ) ;
        attrIter->second = boost::lexical_cast<std::string>(pairState) ;
        attrIter = protectionPairIter->second.m_Attrs.find( m_proPairAttrNamesObj.GetShouldResyncSetFromAttrName()) ;
        attrIter->second = boost::lexical_cast<std::string>(resyncRequiredCause) ;
        attrIter = protectionPairIter->second.m_Attrs.find( m_proPairAttrNamesObj.GetShouldResyncAttrName()) ;
        attrIter->second = boost::lexical_cast<std::string>(resyncRequiredFlag) ;
        if( resyncRequiredCause == 1 )
        {
            attrIter = protectionPairIter->second.m_Attrs.find( m_proPairAttrNamesObj.GetResyncSetCXtimeStampAttrName()) ;
            attrIter->second = "unknown time stamp" ;

        }
        else
        {
            attrIter = protectionPairIter->second.m_Attrs.find( m_proPairAttrNamesObj.GetResyncSetTimeStampAttrName()) ;
            attrIter->second = "unknown time stamp" ;
        }
        attrIter = protectionPairIter->second.m_Attrs.find( m_proPairAttrNamesObj.GetReplStateAttrName()) ;
        attrIter->second = boost::lexical_cast<std::string>(syncMode) ;
        attrIter = protectionPairIter->second.m_Attrs.find( m_proPairAttrNamesObj.GetJobIdAttrName()) ;
        attrIter->second = jobId ;
        attrIter = protectionPairIter->second.m_Attrs.find( m_proPairAttrNamesObj.GetResyncStartTagTimeAttrName()) ;
        attrIter->second = boost::lexical_cast<std::string>(resyncStartTagTime) ;
        attrIter = protectionPairIter->second.m_Attrs.find( m_proPairAttrNamesObj.GetResyncStartSeqAttrName()) ;
        attrIter->second = boost::lexical_cast<std::string>(resyncStartSeqNo) ;
        attrIter = protectionPairIter->second.m_Attrs.find( m_proPairAttrNamesObj.GetResyncEndTagTimeAttrName()) ;
        attrIter->second = boost::lexical_cast<std::string>(resyncEndTagTime) ;
        attrIter = protectionPairIter->second.m_Attrs.find( m_proPairAttrNamesObj.GetResyncEndSeqAttrName()) ;
        attrIter->second = boost::lexical_cast<std::string>(resyncEndSeqNo) ;
        ScenarioConfigPtr scenarioConfigPtr = ScenarioConfig::GetInstance() ;
        RetentionPolicy  retentionPolicy ;
        scenarioConfigPtr->GetRetentionPolicy( retentionPolicy ) ;
        UpdatePairsWithRetention( retentionPolicy ) ;
    }
}
void ProtectionPairConfig::GetTargetVolumes( std::list<std::string>& tgtVolumes)
{
    ConfSchema::ObjectsIter_t protectionPairObjIter = m_ProtectionPairGrp->m_Objects.begin() ;
    while( protectionPairObjIter != m_ProtectionPairGrp->m_Objects.end() )
    {
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = protectionPairObjIter->second.m_Attrs.find( m_proPairAttrNamesObj.GetTgtNameAttrName()) ;
        tgtVolumes.push_back( attrIter->second ) ;
        protectionPairObjIter++; 
    }
}

bool ProtectionPairConfig::SetQueuedState(const std::string& srcVolume, PAIR_QUEUED_STATUS queuedStatus)
{
    ConfSchema::Object* protectionPairObj = NULL ;
    bool ret = false ;
    if( GetProtectionPairObj(srcVolume, "",&protectionPairObj) )
    {
        ConfSchema::Attrs_t& protectionPairAttrs = protectionPairObj->m_Attrs ;
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = protectionPairAttrs.find(m_proPairAttrNamesObj.GetExecutionStateAttrName()) ;
        attrIter->second = boost::lexical_cast<std::string>(queuedStatus) ;
        ret = true ;
    }
    return ret ;
}
void ProtectionPairConfig::GetResyncStartAndEndTime(ConfSchema::Object& protectionPairObj,
														std::string& resyncStartTime,
														std::string& resyncEndTime)
{
	ConfSchema::AttrsIter_t attrIter ;
	attrIter = protectionPairObj.m_Attrs.find(m_proPairAttrNamesObj.GetResyncStartTime()) ;
	if( attrIter != protectionPairObj.m_Attrs.end() ) 
	{
		resyncStartTime = attrIter->second ;
	}
	attrIter = protectionPairObj.m_Attrs.find(m_proPairAttrNamesObj.GetResyncEndTime()) ;
	if( attrIter != protectionPairObj.m_Attrs.end() ) 
	{
		resyncEndTime = attrIter->second ;
	}
}
void ProtectionPairConfig::GetResyncStartAndEndTime(const std::string& srcDevName,
													std::string& resyncStartTime,
													std::string& resyncEndTime,
													const std::string& tgtDevName,
													const std::string& srcHostId,
													const std::string& tgtHostId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool bRet ;
    ConfSchema::Object* protectionPairObj = NULL ;
    if( !srcDevName.empty() && !tgtDevName.empty() )
    {
        bRet = GetProtectionPairObj( srcDevName, tgtDevName, &protectionPairObj ) ;
    }
    else if( !srcDevName.empty() )
    {
        bRet = GetProtectionPairObjBySrcDevName( srcDevName, &protectionPairObj ) ;
    }
    else if( !tgtDevName.empty() )
    {
        bRet = GetProtectionPairObjByTgtDevName( tgtDevName, &protectionPairObj ) ;
    }
    if( bRet )
    {
        GetResyncStartAndEndTime(*protectionPairObj, resyncStartTime, resyncEndTime) ;
		if( resyncStartTime == "" || resyncStartTime == "0" )
		{
			resyncStartTime = "N/A" ;
		}
		if( resyncEndTime == "" || resyncEndTime == "0" )
		{
			resyncEndTime = "N/A" ;
		}

    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void ProtectionPairConfig::GetResyncStartAndEndTimeInHRF(const std::string& srcDevName,
														 std::string& resyncStartTime,
														 std::string& resyncEndTime,
														 const std::string& tgtDevName,
														 const std::string& srcHostId,
														 const std::string& tgtHostId)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ConfSchema::Object* protectionPairObj = NULL ;
	bool bRet ;
    if( !srcDevName.empty() && !tgtDevName.empty() )
    {
        bRet = GetProtectionPairObj( srcDevName, tgtDevName, &protectionPairObj ) ;
    }
    else if( !srcDevName.empty() )
    {
        bRet = GetProtectionPairObjBySrcDevName( srcDevName, &protectionPairObj ) ;
    }
    else if( !tgtDevName.empty() )
    {
        bRet = GetProtectionPairObjByTgtDevName( tgtDevName, &protectionPairObj ) ;
    }
    if( bRet )
    {
        GetResyncStartAndEndTime(*protectionPairObj, resyncStartTime, resyncEndTime) ;
		if( resyncStartTime != "0" && resyncStartTime != "" )
		{
			char buff[20];
			time_t t = boost::lexical_cast<unsigned long>(resyncStartTime) ;
			strftime(buff, 20, "%m-%d-%Y %H:%M:%S", localtime(&t));
			resyncStartTime = buff;
		}
		else
		{
			resyncStartTime = "N/A" ;
		}
		if( resyncEndTime != "0" && resyncEndTime != "" )
		{
			char buff[20];
			time_t t = boost::lexical_cast<unsigned long>(resyncEndTime) ;
			strftime(buff, 20, "%m-%d-%Y %H:%M:%S", localtime(&t));
			resyncEndTime = buff;
		}
		else
		{
			resyncEndTime = "N/A" ;
		}
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;

}

std::string ProtectionPairConfig::GetPauseExtendedReason(ConfSchema::Object& protectionPairObj)
{
	ConfSchema::AttrsIter_t attrIter ;
	attrIter = protectionPairObj.m_Attrs.find( m_proPairAttrNamesObj.GetPauseExtendedReasonAttrName() ) ;
	if( attrIter == protectionPairObj.m_Attrs.end() )
	{
		return "" ;
	}
	return attrIter->second ;
}

void ProtectionPairConfig::GetPauseExtendedReason(const std::string& srcDevName,
                                                  std::string& pauseReason,
                                                  const std::string& tgtDevName,
                                                  const std::string& srcHostId,
                                                  const std::string& tgtHostId)
{
	    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    ConfSchema::Object* protectionPairObj = NULL ;
    if( !srcDevName.empty() && !tgtDevName.empty() )
    {
        bRet = GetProtectionPairObj( srcDevName, tgtDevName, &protectionPairObj ) ;
    }
    else if( !srcDevName.empty() )
    {
        bRet = GetProtectionPairObjBySrcDevName( srcDevName, &protectionPairObj ) ;
    }
    else if( !tgtDevName.empty() )
    {
        bRet = GetProtectionPairObjByTgtDevName( tgtDevName, &protectionPairObj ) ;
    }
    if( bRet )
    {
        pauseReason = GetPauseExtendedReason(*protectionPairObj) ;
    }
}
void ProtectionPairConfig::DisableAutoResync(ConfSchema::Object& protectionPairObj)
{
	ConfSchema::AttrsIter_t attrIter ;
	protectionPairObj.m_Attrs.insert( std::make_pair( m_proPairAttrNamesObj.GetAutoResyncDisabledAttrName(), "0" )) ;
	attrIter = protectionPairObj.m_Attrs.find( m_proPairAttrNamesObj.GetAutoResyncDisabledAttrName()) ;
	attrIter->second = "1" ;
	attrIter = protectionPairObj.m_Attrs.find( m_proPairAttrNamesObj.GetSrcNameAttrName()) ;
	AuditConfigPtr auditconfigptr = AuditConfig::GetInstance() ;
	auditconfigptr->LogAudit("Disabling the auto resync for the volume " + attrIter->second ) ;
	
}

void ProtectionPairConfig::DisableAutoResync(const std::string& srcDevName,
											 const std::string& tgtDevName,
											 const std::string& srcHostId,
											 const std::string& tgtHostId)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool bRet ;
	ConfSchema::Object* protectionPairObj = NULL ;
    if( !srcDevName.empty() && !tgtDevName.empty() )
    {
        bRet = GetProtectionPairObj( srcDevName, tgtDevName, &protectionPairObj ) ;
    }
    else if( !srcDevName.empty() )
    {
        bRet = GetProtectionPairObjBySrcDevName( srcDevName, &protectionPairObj ) ;
    }
    else if( !tgtDevName.empty() )
    {
        bRet = GetProtectionPairObjByTgtDevName( tgtDevName, &protectionPairObj ) ;
    }
    if( bRet )
    {
        DisableAutoResync(*protectionPairObj) ;
    }
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void ProtectionPairConfig::EnableAutoResync(ConfSchema::Object& protectionPairObj)
{
	ConfSchema::AttrsIter_t attrIter ;
	protectionPairObj.m_Attrs.insert( std::make_pair( m_proPairAttrNamesObj.GetAutoResyncDisabledAttrName(), "0" )) ;
	attrIter = protectionPairObj.m_Attrs.find( m_proPairAttrNamesObj.GetAutoResyncDisabledAttrName()) ;
	attrIter->second = "0" ;
	attrIter = protectionPairObj.m_Attrs.find( m_proPairAttrNamesObj.GetSrcNameAttrName()) ;
	AuditConfigPtr auditconfigptr = AuditConfig::GetInstance() ;
	auditconfigptr->LogAudit("Enabling the auto resync for the volume " + attrIter->second ) ;

}

void ProtectionPairConfig::EnableAutoResync(const std::string& srcDevName,
											 const std::string& tgtDevName,
											 const std::string& srcHostId,
											 const std::string& tgtHostId)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool bRet ;
	ConfSchema::Object* protectionPairObj = NULL ;
    if( !srcDevName.empty() && !tgtDevName.empty() )
    {
        bRet = GetProtectionPairObj( srcDevName, tgtDevName, &protectionPairObj ) ;
    }
    else if( !srcDevName.empty() )
    {
        bRet = GetProtectionPairObjBySrcDevName( srcDevName, &protectionPairObj ) ;
    }
    else if( !tgtDevName.empty() )
    {
        bRet = GetProtectionPairObjByTgtDevName( tgtDevName, &protectionPairObj ) ;
    }
    if( bRet )
    {
        EnableAutoResync(*protectionPairObj) ;
    }
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
bool ProtectionPairConfig::IsAutoResyncDisabled(ConfSchema::Object& protectionPairObj)
{
	ConfSchema::AttrsIter_t attrIter ;
	attrIter = protectionPairObj.m_Attrs.find( m_proPairAttrNamesObj.GetAutoResyncDisabledAttrName()) ;
	if( attrIter != protectionPairObj.m_Attrs.end() && attrIter->second == "1" )
	{
		return true ;
	}
	return false ;
}

bool ProtectionPairConfig::IsAutoResyncDisabled(const std::string& srcDevName,
											 const std::string& tgtDevName,
											 const std::string& srcHostId,
											 const std::string& tgtHostId)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool bRet ;ConfSchema::Object* protectionPairObj = NULL ;
    if( !srcDevName.empty() && !tgtDevName.empty() )
    {
        bRet = GetProtectionPairObj( srcDevName, tgtDevName, &protectionPairObj ) ;
    }
    else if( !srcDevName.empty() )
    {
        bRet = GetProtectionPairObjBySrcDevName( srcDevName, &protectionPairObj ) ;
    }
    else if( !tgtDevName.empty() )
    {
        bRet = GetProtectionPairObjByTgtDevName( tgtDevName, &protectionPairObj ) ;
    }
    if( bRet )
    {
        bRet = IsAutoResyncDisabled(*protectionPairObj) ;
    }
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return bRet ;
}


void ProtectionPairConfig::ModifyRetentionBaseDir(const std::string& existRepoBase,
												  const std::string& newRepoBase)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	std::string newRepoBaseDrive, newRepoBasePath ;
	std::string existingRepoBaseDrive, existingRepoBasePath ;
	newRepoBaseDrive = newRepoBase ;
	newRepoBasePath = newRepoBase ;
	if( newRepoBase.length() == 1 )
	{
		newRepoBasePath += ":\\" ;
	}
	existingRepoBaseDrive = existRepoBase ;
	existingRepoBasePath = existRepoBase ;
	if( existRepoBase.length() == 1 )
	{
		existingRepoBasePath += ":\\" ;
	}
	if( newRepoBasePath[newRepoBasePath.length() - 1 ] != '\\' )
	{
		newRepoBasePath += '\\' ;
	}
	if( existingRepoBasePath[existingRepoBasePath.length() - 1 ] != '\\' )
	{
		existingRepoBasePath += '\\' ;
	}
	ConfSchema::ObjectsIter_t protectionPairObjiter = m_ProtectionPairGrp->m_Objects.begin() ;
	while( protectionPairObjiter != m_ProtectionPairGrp->m_Objects.end() )
	{
		ConfSchema::GroupsIter_t groupIter ;
		groupIter = protectionPairObjiter->second.m_Groups.find( m_retLogPolicyAttrNamesObj.GetName() ) ;
		if( groupIter != protectionPairObjiter->second.m_Groups.end() )
		{
			ConfSchema::Object& retLogPolicyObj = groupIter->second.m_Objects.begin()->second ;
			groupIter = retLogPolicyObj.m_Groups.find( m_retWindowAttrNamesObj.GetName() ) ;
			if( groupIter != retLogPolicyObj.m_Groups.end() )
			{
				ConfSchema::ObjectsIter_t retWindowObjIter = groupIter->second.m_Objects.begin() ;
				while( retWindowObjIter != groupIter->second.m_Objects.end() )
				{
					ConfSchema::Object& retWindowObj = retWindowObjIter->second ;
					ConfSchema::GroupsIter_t retevtPolicyGroupIter = retWindowObj.m_Groups.find( m_retevtPolicyAttrNamesObj.GetName() ) ;
					if( retevtPolicyGroupIter != retWindowObj.m_Groups.end() )
					{
						ConfSchema::Object& retEvtPolicyObj = retevtPolicyGroupIter->second.m_Objects.begin()->second ;
						ConfSchema::AttrsIter_t attrIter ;
						attrIter = retEvtPolicyObj.m_Attrs.find( m_retevtPolicyAttrNamesObj.GetStorageDeviceAttrName() ) ;
						attrIter->second = newRepoBaseDrive ; 
						attrIter = retEvtPolicyObj.m_Attrs.find( m_retevtPolicyAttrNamesObj.GetCatalogPathAttrName() ) ;
						attrIter->second = newRepoBasePath + attrIter->second.substr( existingRepoBasePath.length() ) ;
						attrIter = retEvtPolicyObj.m_Attrs.find( m_retevtPolicyAttrNamesObj.GetStoragePathAttrName() ) ;
						attrIter->second = newRepoBasePath + attrIter->second.substr( existingRepoBasePath.length() ) ;
					}
					retWindowObjIter++ ;
				}
			}
		}
		protectionPairObjiter ++ ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}


bool ProtectionPairConfig::MoveOperationInProgress()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool bret = false ;
	ConfSchema::ObjectsIter_t protectionPairObjiter = m_ProtectionPairGrp->m_Objects.begin() ;
	while( protectionPairObjiter != m_ProtectionPairGrp->m_Objects.end() )
	{
		ConfSchema::AttrsIter_t attrIter ;
		attrIter = protectionPairObjiter->second.m_Attrs.find(m_proPairAttrNamesObj.GetPauseExtendedReasonAttrName());						
		
		if( attrIter != protectionPairObjiter->second.m_Attrs.end() && attrIter->second == "Moving backup" )
		{
			attrIter = protectionPairObjiter->second.m_Attrs.find(m_proPairAttrNamesObj.GetSrcNameAttrName()) ;
			DebugPrintf(SV_LOG_DEBUG, "Move operation is in progress for %s\n", attrIter->second.c_str()) ;
			bret = true ;
			break ;
		}
		protectionPairObjiter++ ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return bret ;
}
