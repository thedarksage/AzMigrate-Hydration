#include "volumerecoveryhelpersproxy.h"
#include <ace/OS_Dirent.h>
#include "volumerecoveryhelperexception.h"
#include "serializationtype.h"
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include "inmsdkutil.h"
#include "confschemareaderwriter.h"
#include "alertconfig.h"
#ifdef SV_WINDOWS
#include "appcommand.h"
#endif
VolumeRecoveryProxyPtr VolumeRecoveryProxy::m_volrcy_pxyobj;
void emptyCdpProgressCallback(CDPMessage const* msg, void *)
{
}


VolumeRecoveryProxy::VolumeRecoveryProxy(const std::string &ipAddress, 
										int port,
										bool isRescueMode)
{
	m_cxipAddress = ipAddress;
	m_port = port;
	m_repositoryPath.clear();
	m_rescueMode = isRescueMode ;
}
VolumeRecoveryProxy::VolumeRecoveryProxy(std::string const& repositoryPath,
										 bool isRescueMode)
{
	m_repositoryPath = repositoryPath;
	m_cxipAddress.clear();
	m_port = 0;
	m_rescueMode = isRescueMode ;
	m_serializerType = Xmlize ;
	DebugPrintf(SV_LOG_DEBUG, "repo path %s\n", repositoryPath.c_str()) ;
}


VolumeRecoveryProxy::~VolumeRecoveryProxy()
{

}

void VolumeRecoveryProxy::createInstance(const std::string &ipAddress, 
										 int port,
										 bool isRescueMode)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;

    if (ipAddress.empty() || !(1 <= port && port <= 65535)) 
    {
        throw "invalid ip address and/or port";
    }
    try
    {
        m_volrcy_pxyobj.reset( new VolumeRecoveryProxy(ipAddress, port, isRescueMode) );
        if( m_volrcy_pxyobj.get() == NULL )
        {
            throw "failed to allocate memory to VolumeRecoveryProxy object";
        }
    }
    catch(std::exception ex)
    {
        throw "failed to allocate memory to VolumeRecoveryProxy object";
    }         

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}
void VolumeRecoveryProxy::createInstance(std::string repositoryPath, 
										 bool isRescueMode)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;

    if (repositoryPath.empty()) 
    {
        throw "invalid path";
    }

   
#ifdef SV_WINDOWS
    //Removing duplicate slashes and converting the forward to reverse slashes
    boost::algorithm::replace_all(repositoryPath, "/", "\\") ;
    if( repositoryPath.find( "\\\\" ) != 0 )
    {
        boost::algorithm::replace_all(repositoryPath, "\\\\", "\\") ;
    }
    /*
    Making the repository path to look like
    <Drive Letter>:\
    or
    <Drive Letter>:\Mount\
    */
    if( repositoryPath.length() == 1 )
    {
        repositoryPath += ":\\" ;
    }
    else if( repositoryPath.length() == 2 )
    {
        repositoryPath += "\\" ;
    }
    else
    {
        if( repositoryPath[repositoryPath.length() - 1 ] != '\\' )
        {
            repositoryPath += "\\" ;
        }
    }
#else
    //Removing duplicate slashes and converting the reverse to forward slashes
    boost::algorithm::replace_all(repositoryPath, "\\", "/") ;
    boost::algorithm::replace_all(repositoryPath, "//", "/") ;
    if( repositoryPath[repositoryPath.length() - 1 ] != '/' )
    {
        repositoryPath += "/" ;
    }

#endif
    try
    {
        m_volrcy_pxyobj.reset( new VolumeRecoveryProxy(repositoryPath, isRescueMode) );
        if( m_volrcy_pxyobj.get() == NULL )
        {
            throw "failed to allocate memory to VolumeRecoveryProxy object";
        }
    }
    catch(std::exception ex)
    {
        throw "failed to allocate memory to VolumeRecoveryProxy object";
    }         
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}
VolumeRecoveryProxyPtr  VolumeRecoveryProxy::getInstance()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    if(m_volrcy_pxyobj.get() == NULL)
    {
        DebugPrintf(SV_LOG_ERROR, "VolumeRecoveryProxy object is not yet created returning null") ;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
    return m_volrcy_pxyobj;
}

bool VolumeRecoveryProxy::createXmlStream(FunctionInfo& funInfo,std::stringstream& ReqXmlStream)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;

    bool breturn = true;
    XmlRequestValueObject FunRequestObj;
    //Populating xml request with default values while posting it CX
    FunRequestObj.setRequestId("10");
    FunRequestObj.setVersion("1.0");
    FunRequestObj.setAuthenticationAccessKeyID("DDC9525FF275C104EFA1DFFD528BD0145F903CB1");
    FunRequestObj.setRequestFunctionList(funInfo);

    try
    {
        FunRequestObj.XmlRequestSave(ReqXmlStream);
    }
    catch(std::exception ex)
    {
        breturn = false;
        DebugPrintf(SV_LOG_DEBUG, "createXmlStream Failed with exception %s\n", ex.what()) ;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return breturn;
}

bool VolumeRecoveryProxy::Transport(FunctionInfo& funInfo)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;

    bool breturn = false;

    try
    {
        if(m_repositoryPath.empty())
        {
            breturn = ProcessRequestUsingCx(funInfo);
        }
        else
        {
			if( !m_repositoryPath.empty() )
			{
				insertintoreqresp(funInfo.m_RequestPgs,cxArg( m_repositoryPath ),"RepositoryPath" );
			}
			if( !m_agentGuid.empty() )
			{
				insertintoreqresp(funInfo.m_RequestPgs,cxArg( m_agentGuid ),"AgentGUID" );
			}
            breturn = ProcessRequestUsingLocalStore(funInfo);
        }
    }
    catch (TransactionException const& te)
    {
        DebugPrintf(SV_LOG_DEBUG, "Transport Failed with exception %s\n", te.what()) ;
        breturn = false;
    }    
    catch(std::exception ex)
    {
        DebugPrintf(SV_LOG_DEBUG, "Transport Failed with exception %s\n", ex.what()) ;
        breturn = false;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return breturn;
}

bool VolumeRecoveryProxy::ProcessRequestUsingLocalStore(FunctionInfo& funInfo)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;

    bool breturn = true;
    INMAGE_ERROR_CODE errCode = E_SUCCESS;
    APIController controller;
    errCode = controller.processRequest(funInfo);
    if(E_SUCCESS != errCode)
    {
        breturn = false;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return breturn;
}

bool VolumeRecoveryProxy::ProcessRequestUsingCx(FunctionInfo& funInfo)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;

    bool breturn = false;
    std::stringstream ReqXmlStream;

    try
    {
        if(createXmlStream(funInfo,ReqXmlStream))
        {
            std::string ResponseXmlstream;
            char *pszGetBuffer = NULL;
            std::string szposturl = "/ScoutAPI/CXAPI.php";
            DebugPrintf(SV_LOG_DEBUG, "Posting request to Cx %s:\n",ReqXmlStream.str().c_str());
            if(postToCx(m_cxipAddress.c_str(), m_port, szposturl.c_str(), ReqXmlStream.str().c_str(),&pszGetBuffer, 0, 1 ).failed())
            {
                DebugPrintf(SV_LOG_ERROR, "PostToCx Failed with error while posting Xml to CX\n");
            }
            else
            {
                if(pszGetBuffer != NULL)
                {
                    ResponseXmlstream = std::string(pszGetBuffer);
                    if(!ResponseXmlstream.empty())
                    {
                        std::list<FunctionInfo>Funresponselist;
                        FunctionInfo Funresponseobj;
                        std::stringstream ResponseStrm;
                        ResponseStrm.str(ResponseXmlstream);

                        XmlResponseValueObject FunResponseObj;
                        try
                        {
                            DebugPrintf(SV_LOG_DEBUG, "response stream from Cx %s:\n",ResponseStrm.str().c_str());
                            FunResponseObj.InitializeXmlResponseObject(ResponseStrm);
                            breturn = true;
                        }
                        catch(std::exception ex)
                        {
                            DebugPrintf(SV_LOG_DEBUG, "InitializeXmlResponseObject Failed with exception %s\n", ex.what()) ;
                        }

                        if(breturn)
                        {
                            Funresponselist = FunResponseObj.getFunctionList();
                            std::list<FunctionInfo>::const_iterator funInfoIter = Funresponselist.begin();
                            //For now only one functioninfo object will be there in a list 
                            //So no need iterate through the loop
                            if(funInfoIter != Funresponselist.end())
                            {
                                Funresponseobj = *funInfoIter;
                            }

                            //assigning response pgs from response to funinfo obj parameter
                            funInfo.m_ReturnCode = Funresponseobj.m_ReturnCode;
                            funInfo.m_Message = Funresponseobj.m_Message;
                            funInfo.m_ResponsePgs = Funresponseobj.m_ResponsePgs;
                            //or we may need to assign whole object like
                            //funInfo = Funresponseobj;
                        }
                    }
                    else
                    {		
                        DebugPrintf(SV_LOG_DEBUG, "Empty response xml stream returned from CX\n");
                    }
                }
                else
                {		
                    DebugPrintf(SV_LOG_DEBUG, "Empty response xml stream returned from CX\n");
                }
            }
        }
    }
    catch(std::exception ex)
    {
        DebugPrintf(SV_LOG_DEBUG, "ProcessRequestUsingCx Failed with exception %s\n", ex.what()) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return breturn;
}


SVERROR VolumeRecoveryProxy::ImportMBRForAgentGUID(const std::string& repositoryPath, SV_ULONGLONG SelectedtimeStamp, const std::string& agentGUID, std::string& MBRFilepath)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVERROR breturn = SVS_FALSE;
    try
    {
        std::vector<std::string>MbrFileVector;
		std::list<std::string>MbrTimestamplist;
        std::string MbrDirectoryPath ;
        MbrDirectoryPath = repositoryPath ;
        if( isalpha(MbrDirectoryPath[0]) && repositoryPath.length() == 1 )
        {
            MbrDirectoryPath += ":" ;
        }
        MbrDirectoryPath = MbrDirectoryPath + ACE_DIRECTORY_SEPARATOR_CHAR_A + std::string("SourceMBR") + ACE_DIRECTORY_SEPARATOR_CHAR_A + agentGUID;

        if(GetMbrFileList(MbrDirectoryPath,MbrFileVector) == SVS_OK)
        {
            std::vector<std::string>::const_iterator MbrfilesIter = MbrFileVector.begin();
            while(MbrfilesIter != MbrFileVector.end())
            {
                std::string MbrFile = *MbrfilesIter;
				if( MbrFile.find("partition") != std::string::npos )
				{
					DebugPrintf(SV_LOG_DEBUG, "Skipping the partition file\n") ;
					MbrfilesIter++;
					continue ;
				}
                size_t pos = MbrFile.find_first_of("mbrdata_");
                std::string MbrfileTimestamp = MbrFile.substr(pos+8);
                MbrTimestamplist.push_back(MbrfileTimestamp);
                MbrfilesIter++;
            }

            MbrTimestamplist.sort();
            std::list<std::string>::const_iterator timestmpIter = MbrTimestamplist.begin();
			std::string mbrfileforRecovery ;
            while(timestmpIter!=MbrTimestamplist.end())
            {
				SV_ULONGLONG MbrtimeStamp;
				MbrtimeStamp = boost::lexical_cast<SV_ULONGLONG>(timestmpIter->substr(0, 14));
                if(SelectedtimeStamp < MbrtimeStamp)
                    break;
                mbrfileforRecovery = *timestmpIter ;
                timestmpIter++;
            }
            //required time stamp is
            MBRFilepath = MbrDirectoryPath + ACE_DIRECTORY_SEPARATOR_CHAR_A + std::string("mbrdata_")+ mbrfileforRecovery;
            breturn = SVS_OK;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "Error Failed get MBR files list in ImportMBRForAgentGUID \n");
        }
    }
    catch(std::exception ex)
    {
        DebugPrintf(SV_LOG_DEBUG, "ImportMBRForAgentGUID Failed with exception %s\n", ex.what()) ;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return breturn;
}
SVERROR VolumeRecoveryProxy::processAPI( FunctionInfo& functInfo, const std::vector<std::string>& acceptableErrorCodes )
{
    SVERROR svError = SVS_FALSE ;
    if( Transport( functInfo ) )
    {
        std::vector<std::string>::const_iterator errorCodeBegin = acceptableErrorCodes.begin() ;
        std::vector<std::string>::const_iterator errorCodeEnd = acceptableErrorCodes.end() ;
        while( errorCodeBegin != errorCodeEnd )
        {
            if( strcmp( functInfo.m_ReturnCode.c_str(), (*errorCodeBegin).c_str() ) == 0 )
            {
                svError = SVS_OK ;
                break ;        
            }
            errorCodeBegin++ ;
        }
        if(  svError != SVS_OK )
            throw VolumeRecoveryHelperException( functInfo ) ;
    }
    else
    {
        std::stringstream exceptionMessage ;
        exceptionMessage << "The API" << functInfo.m_RequestFunctionName << "failed due to network connection error " << std::endl ;
        throw VolumeRecoveryHelperException( exceptionMessage.str() ) ;
    }
    return svError ;
}

void VolumeRecoveryProxy::RepairConfigStoreIfRequired()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	std::string repaircmdStr ;
	FileConfigurator fc ;
	if( m_rescueMode )
	{
		fc.setRepositoryLocation( m_repositoryPath ) ;
	}
	repaircmdStr = fc.getInstallPath();
#ifndef SV_WINDOWS
	repaircmdStr += ACE_DIRECTORY_SEPARATOR_CHAR ;
	repaircmdStr += "bin" ;
#endif
	std::string repositorypath = m_repositoryPath ;
	if( repositorypath[repositorypath.length()-1] == '\\' )
	{
		repositorypath.erase(repositorypath.length()-1) ;
	}
	repaircmdStr += ACE_DIRECTORY_SEPARATOR_CHAR ;
	repaircmdStr += "bxadmin" ;
	repaircmdStr += " --rebuildconfigstore all " ;
	repaircmdStr += "\"" ;
	repaircmdStr += repositorypath;
	repaircmdStr += "\"" ;
	std::string strLogFile = fc.getInstallPath() + ACE_DIRECTORY_SEPARATOR_STR_A + "bxadmin_stdout.log" ;

	std::string outputmsg, errormsg ;
	int  ecode ;
#ifndef SV_WINDOWS
	RunInmCommand( repaircmdStr, outputmsg, errormsg, ecode ) ;
#else
	SV_ULONG dwExitCode = 0 ;
	AppCommand repairCmd(repaircmdStr, 0, strLogFile ) ;
	std::string stdOutput ;
	bool bProcessActive = true ;
	repairCmd.Run(dwExitCode,stdOutput, bProcessActive, fc.getInstallPath(), false  )  ;
#endif
DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;

}

SVERROR VolumeRecoveryProxy::ListHosts(HostInfoMap& hostinfo)
{
    SVERROR breturn = SVS_FALSE;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    try
    {
        RepairConfigStoreIfRequired() ;
        FunctionInfo funInfo;
        funInfo.m_RequestFunctionName = "ListHosts";
        funInfo.m_ReqstInclude = "No";

        processAPI( funInfo ) ;
        extractfromreqresp(funInfo.m_ResponsePgs,hostinfo,"");
        breturn = SVS_OK;     
    }
    catch( VolumeRecoveryHelperException& ex )
    {
        DebugPrintf( SV_LOG_ERROR, "Failed to list the hosts with exception %s\n", ex.what() );
    }
    catch(ContextualException& ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to list the hosts with exception %s\n", ex.what()) ;
    }
    catch(std::exception& ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to list the hosts with exception %s\n", ex.what()) ;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return breturn;
}

SVERROR VolumeRecoveryProxy::GetRestorePoints(const std::string& repositoryPath, std::string& agentGUID, VolumeRestorePointsMap& volrestoremapobj)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVERROR breturn = SVS_FALSE;
    try
    {
        Serializer sr(m_serializerType);
        sr.Serialize("GetRestorePoints");

        FunctionInfo funInfo = sr.m_FunctionInfo;

        if(Transport(funInfo))
        {
            if(strcmp(funInfo.m_ReturnCode.c_str(),"0")== 0)
            {
                volrestoremapobj =  sr.UnSerialize<VolumeRestorePointsMap>();
                breturn = SVS_OK;
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "Error GetRestorePoints execution failed with:%s\n",funInfo.m_Message.c_str());
            }
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "Error Transport Failed in GetRestorePoints \n");
        }

    }
    catch(std::exception ex)
    {
        DebugPrintf(SV_LOG_DEBUG, "GetRestorePoints Failed with exception %s\n", ex.what()) ;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return breturn;
}

VolumeErrors VolumeRecoveryProxy::PauseTrackingForVolumeForAgentGUID(PauseResumeDetails& pauseInfo, 
                                                                     const std::string& agentGUID)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    VolumeErrors volErrors;
    try
    {
		FunctionInfo funInfo;
		std::vector<std::string> acceptableErrorCodes ;
		acceptableErrorCodes.push_back( "0" ) ;
		acceptableErrorCodes.push_back( "109" ) ;
		if(m_repositoryPath.empty())
		{
			funInfo.m_RequestFunctionName = "PauseTrackingForVolumeForAgentGUID";
		}
		else
		{
			funInfo.m_RequestFunctionName = "PauseTrackingForVolume";
		}
		if( m_rescueMode )
		{
			pauseInfo.PauseDirectly = true ;
		}
		insertintoreqresp(funInfo.m_RequestPgs,cxArg( pauseInfo ),"" );
		insertintoreqresp(funInfo.m_RequestPgs,cxArg( agentGUID ),"AgentGUID" );
		processAPI( funInfo, acceptableErrorCodes ) ;
		extractfromreqresp(funInfo.m_ResponsePgs, volErrors,"");		
	}
    catch( VolumeRecoveryHelperException& ex )
    {
        DebugPrintf( SV_LOG_ERROR, "Failed to pause the tracking with exception %s\n", ex.what() );   
    }
    catch(ContextualException& ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to pause the tracking with exception %s\n", ex.what()) ;
    }
    catch(std::exception& ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to pause the tracking with exception %s\n", ex.what()) ;
        //throw ex;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return volErrors;
}
VolumeErrors VolumeRecoveryProxy::ResumeTrackingForVolumeForAgentGUID(PauseResumeDetails& resumeInfo, 
                                                                      const std::string& agentGUID)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    VolumeErrors volError;
    try
    {
        FunctionInfo funInfo;
        std::vector<std::string> acceptableErrorCodes ;
        acceptableErrorCodes.push_back( "0" ) ;
        acceptableErrorCodes.push_back( "103" ) ;
        if(m_repositoryPath.empty())
        {
            funInfo.m_RequestFunctionName = "ResumeTrackingForVolumeForAgentGUID";
        }
        else
        {
            funInfo.m_RequestFunctionName = "ResumeTrackingForVolume";
        }
        insertintoreqresp(funInfo.m_RequestPgs,cxArg( resumeInfo ),"" );
        insertintoreqresp(funInfo.m_RequestPgs,cxArg( agentGUID ),"AgentGUID" );
        processAPI( funInfo, acceptableErrorCodes ) ;          
        extractfromreqresp(funInfo.m_ResponsePgs, volError,"");	
    }
    catch( VolumeRecoveryHelperException& ex )
    {
        DebugPrintf( SV_LOG_ERROR, "Failed to resume tracking with exception %s\n", ex.what() );
    }
    catch(ContextualException& ex)
    {
        DebugPrintf(SV_LOG_DEBUG, "Failed to resume tracking with exception %s\n", ex.what()) ;
    }
    catch(std::exception& ex)
    {
        DebugPrintf(SV_LOG_DEBUG, "Failed to resume tracking with exception %s\n", ex.what()) ;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return volError;
}

bool VolumeRecoveryProxy::unmountVirtualVolume(std::string& volpackName)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    bool bret = false;
    if(!volpackName.empty())
    {
        std::vector<std::string> cdpcliArgs ;
        cdpcliArgs.push_back( "cdpcli" ) ;
        cdpcliArgs.push_back( "--virtualvolume" ) ;
        cdpcliArgs.push_back( "--op=unmount" ) ;        
        cdpcliArgs.push_back( "--drivename=" +  volpackName) ;
        cdpcliArgs.push_back( "--deletesparsefile=no" ) ;
        bret = RunCDPCLI( cdpcliArgs ) ;

        if(bret == false ) 
        {
            DebugPrintf(SV_LOG_DEBUG,"volpack mount failed for volpack:%s",volpackName.c_str());
        }
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG,"given volpackname is empty"); 
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return bret;
}
std::string VolumeRecoveryProxy::GetApplicationTagType (const std::string &snapShotSourceVol,const std::string &restoreEvent)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    bool bRet = false;
    std::string applicationType;
    VolumeProtectionRecoveryInfo_t::iterator volProteBegIter = m_ListVolumesWithProtectionDetails.VolumeLevelProtectionInfo.begin() ;
    while( volProteBegIter != m_ListVolumesWithProtectionDetails.VolumeLevelProtectionInfo.end() )
    {
        VolumeRestorePointsMap_t::iterator volumeRestorePointsIter = volProteBegIter->second.VolumeRestorePoints.begin();
        while (volumeRestorePointsIter  != volProteBegIter->second.VolumeRestorePoints.end () )
        {
            std::string sourceVolume = snapShotSourceVol.substr(0,snapShotSourceVol.length()-1);
            DebugPrintf (SV_LOG_DEBUG, " sourceVolume is %s  \n ",sourceVolume.c_str() );
            DebugPrintf (SV_LOG_DEBUG, " volumeRestorePointsIter->second.ProtectionDetails.TargetVolume is %s  \n ",volumeRestorePointsIter->second.ProtectionDetails.TargetVolume.c_str() );
            RestorePointMap_t::iterator volumeRestorePointIter = volumeRestorePointsIter->second.RestorePoints.begin();
            while (volumeRestorePointIter != volumeRestorePointsIter->second.RestorePoints.end())
            {
                RestorePointMap_t restorePoint  = volumeRestorePointsIter->second.RestorePoints;
                RestorePointMap_t::iterator restorePointIter = restorePoint.begin();
                while (restorePointIter  != restorePoint.end () )
                {	
                    if (restoreEvent == restorePointIter->second.RestorePoint)
                    {
                        DebugPrintf (SV_LOG_DEBUG, " restoreEvent is %s \n  ",restoreEvent.c_str() );
                        DebugPrintf (SV_LOG_DEBUG, " restorePointIter->second.RestorePoint %s  \n ",restorePointIter->second.RestorePoint.c_str());
                        applicationType =  restorePointIter->second.ApplicationType; 
                        bRet = true;
                        break;
                    }
                    restorePointIter ++;
                }
                if (bRet == true )
                    break;
                volumeRestorePointIter ++;
            }
            if (bRet == true )
                break;
            volumeRestorePointsIter ++; 
        }
        if (bRet == true )
            break;
        volProteBegIter++ ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return applicationType;
}

bool isReFsVolume(const ListVolumesWithProtectionDetails& lvwp, const std::string& srcvolname)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool isRefs = false ;
	std::map<VolumeGUID_t, VolumeProtectionRecoveryInfo>::const_iterator volprotiter = lvwp.VolumeLevelProtectionInfo.begin() ;
	while( volprotiter != lvwp.VolumeLevelProtectionInfo.end() )
	{
		if( volprotiter->second.VolumeDetails.VolumeName == srcvolname )
		{
			if( volprotiter->second.VolumeDetails.FileSystemType == "ReFS" )
			{
				DebugPrintf(SV_LOG_DEBUG, "%s is a refs volume\n", volprotiter->second.VolumeDetails.VolumeName.c_str()) ;
				isRefs = true ;
			}
			break ;
		}
		volprotiter++ ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return isRefs ;
}

SVERROR
VolumeRecoveryProxy::ListVolumesWithProtectionDetailsByAgentGUID(ListVolumesWithProtectionDetails &VolProtectionDetails,
                                                                 const std::string &agentGUID)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVERROR bret = SVS_FALSE;
    try
    {
        FunctionInfo funInfo;
        if(m_repositoryPath.empty())
        {
            funInfo.m_RequestFunctionName = "ListVolumesWithProtectionDetailsByAgentGUID";
        }
        else
        {
            funInfo.m_RequestFunctionName = "ListVolumesWithProtectionDetails";
        }
        insertintoreqresp(funInfo.m_RequestPgs,cxArg( agentGUID ),"AgentGUID" ); 
        processAPI( funInfo ) ;
        extractfromreqresp(funInfo.m_ResponsePgs,VolProtectionDetails,"");
        DebugPrintf( SV_LOG_DEBUG, "Volume Protection Details:\n" ) ;
        VolumeProtectionRecoveryInfo_t::iterator volProteBegIter = VolProtectionDetails.VolumeLevelProtectionInfo.begin() ;
        while( volProteBegIter != VolProtectionDetails.VolumeLevelProtectionInfo.end() )
        {
            DebugPrintf( SV_LOG_DEBUG, "Key Name: %s, Volume Name:%s\n", volProteBegIter->first.c_str(), volProteBegIter->second.VolumeDetails.VolumeName.c_str() ) ;
            volProteBegIter++ ;
        }
        m_ListVolumesWithProtectionDetails = VolProtectionDetails;
        bret = SVS_OK;		
    }
    catch( VolumeRecoveryHelperException& ex )
    {
        DebugPrintf( SV_LOG_ERROR, "Failed to list the protection details with exception %s\n", ex.what() );
    }
    catch(ContextualException& ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to list the protection details with exception %s\n", ex.what()) ;
    }
    catch(std::exception& ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to list the protection details with exception %s\n", ex.what()) ;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return bret;
}

SVERROR
VolumeRecoveryProxy::ExportRepositoryOnCIFS(RepoCIFSExportMap_t &repoInfoMap,const std::string &agentGUID)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVERROR bret = SVS_FALSE;
    try
    {
        FunctionInfo funInfo;
        std::vector<std::string> acceptableErrorCodes ;
        acceptableErrorCodes.push_back( "0" ) ;
        acceptableErrorCodes.push_back( "210" ) ;
        funInfo.m_RequestFunctionName = "ExportRepositoryOnCIFS";
        insertintoreqresp(funInfo.m_RequestPgs,cxArg(repoInfoMap),"" );
        processAPI( funInfo, acceptableErrorCodes ) ;		
        bret = SVS_OK ;
    }
    catch( VolumeRecoveryHelperException& ex )
    {
        DebugPrintf( SV_LOG_ERROR, "ExportRepositoryOnCIFS Failed with exception %s\n", ex.what() );
    }
    catch(ContextualException& ex)
    {
        DebugPrintf(SV_LOG_ERROR, "ExportRepositoryOnCIFS Failed with exception %s\n", ex.what()) ;
    }
    catch(std::exception ex)
    {
        DebugPrintf(SV_LOG_ERROR, "ExportRepositoryOnCIFS Failed with exception %s\n", ex.what()) ;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return bret;
}

SVERROR
VolumeRecoveryProxy::ExportRepositoryOnNFS(RepoNFSExportMap_t& repoInfoMap,const std::string &agentGUID)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVERROR bret = SVS_FALSE;
    FunctionInfo funInfo;
    try
    {
        std::vector<std::string> acceptableErrorCodes ;
        acceptableErrorCodes.push_back( "0" ) ;
        acceptableErrorCodes.push_back( "210" ) ;
        funInfo.m_RequestFunctionName = "ExportRepositoryOnNFS";
        insertintoreqresp(funInfo.m_RequestPgs,cxArg(repoInfoMap),"" );
        processAPI( funInfo, acceptableErrorCodes ) ;	
        bret = SVS_OK;		
    }
    catch( VolumeRecoveryHelperException& ex )
    {
        DebugPrintf( SV_LOG_ERROR, "ExportRepositoryOnNFS Failed with exception %s\n", ex.what() );
    }
    catch(ContextualException& ex)
    {
        DebugPrintf(SV_LOG_ERROR, "%s Failed with exception %s\n", funInfo.m_RequestFunctionName.c_str(), ex.what()) ;
    }
    catch(std::exception ex)
    {
        DebugPrintf(SV_LOG_ERROR, "%s Failed with exception %s\n", funInfo.m_RequestFunctionName.c_str(), ex.what()) ;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return bret;
}

ExportedRepoDetails
VolumeRecoveryProxy::GetExportedRepositoryDetailsByAgentGUID(const std::string &repositoryName, 
                                                             const std::string &agentGUID, 
                                                             std::string& localPath)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    SVERROR bret = SVS_FALSE;
    ExportedRepoDetails exprepoobj;
    try
    {
        FunctionInfo funInfo;
        std::vector<std::string> acceptableErrorCodes ;
        acceptableErrorCodes.push_back( "0" ) ;
        acceptableErrorCodes.push_back( "210" ) ;
        funInfo.m_RequestFunctionName = "GetExportedRepositoryDetailsByAgentGUID";
        if(m_repositoryPath.empty())
        {
            insertintoreqresp(funInfo.m_RequestPgs,cxArg(repositoryName),"RepositoryName" );
            insertintoreqresp(funInfo.m_RequestPgs,cxArg(agentGUID),"AgentGUID" );
        }
        else
        {
            funInfo.m_RequestFunctionName = "GetExportedRepositoryDetails";	
        }
        processAPI( funInfo, acceptableErrorCodes ) ;	
        extractfromreqresp(funInfo.m_ResponsePgs,exprepoobj,"");
        if(getlocalpath(exprepoobj,localPath))
        {
            bret = SVS_OK;
        }         
    }
    catch( VolumeRecoveryHelperException& ex )
    {
        DebugPrintf( SV_LOG_ERROR, "GetExportedRepositoryDetailsByAgentGUID Failed with exception %s\n", ex.what() );
    }
    catch(ContextualException& ex)
    {
        DebugPrintf(SV_LOG_DEBUG, "GetExportedRepositoryDetailsByAgentGUID Failed with exception %s\n", ex.what()) ;
    }
    catch(std::exception& ex)
    {
        DebugPrintf(SV_LOG_DEBUG, "GetExportedRepositoryDetailsByAgentGUID Failed with exception %s\n", ex.what()) ;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);

    return exprepoobj;
}

VolumeErrors
VolumeRecoveryProxy::PauseTrackingForAgentGUID(std::string &agentGUID,PauseResumeDetails& pauseInfo)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    VolumeErrors volErrors;
    try
    {
        FunctionInfo funInfo;
        std::vector<std::string> acceptableErrorCodes ;
        acceptableErrorCodes.push_back( "0" ) ;
        acceptableErrorCodes.push_back( "109" ) ;
        if(m_repositoryPath.empty())
        {
            funInfo.m_RequestFunctionName = "PauseTrackingForAgentGUID";
            insertintoreqresp(funInfo.m_RequestPgs,cxArg( agentGUID ),"AgentGUID" ); 
            insertintoreqresp(funInfo.m_RequestPgs,cxArg( pauseInfo ),"" );
        }
        else
        {
            funInfo.m_RequestFunctionName = "PauseTracking";
        }
        processAPI( funInfo, acceptableErrorCodes ) ;
        extractfromreqresp(funInfo.m_ResponsePgs, volErrors,"");	
    }
    catch( VolumeRecoveryHelperException& ex )
    {
        DebugPrintf( SV_LOG_ERROR, "Failed to pause tracking with exception %s\n", ex.what() );
    }
    catch(ContextualException& ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to pause tracking with exception %s\n", ex.what()) ;
    }
    catch(std::exception& ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to pause tracking with exception %s\n", ex.what()) ;
        //throw ex;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return volErrors;
}

VolumeErrors
VolumeRecoveryProxy::ResumeTrackingForAgentGUID(std::string &agentGUID, PauseResumeDetails &resumeInfo)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    VolumeErrors volErrors;
    try
    {
        FunctionInfo funInfo;
        std::vector<std::string> acceptableErrorCodes ;
        acceptableErrorCodes.push_back( "0" ) ;
        acceptableErrorCodes.push_back( "103" ) ;
        if(m_repositoryPath.empty())
        {
            funInfo.m_RequestFunctionName = "ResumeTrackingForAgentGUID";
            insertintoreqresp(funInfo.m_RequestPgs,cxArg( agentGUID ),"AgentGUID" ); 
            insertintoreqresp(funInfo.m_RequestPgs,cxArg( resumeInfo ),"" );
        }
        else
        {
            funInfo.m_RequestFunctionName = "ResumeTracking";
        }
        processAPI( funInfo, acceptableErrorCodes ) ;
        extractfromreqresp(funInfo.m_ResponsePgs, volErrors,"");			
    }
    catch( VolumeRecoveryHelperException& ex )
    {
        DebugPrintf( SV_LOG_ERROR, "Failed to resume tracking with exception %s\n", ex.what() );
    }
    catch(ContextualException& ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to resume tracking with exception %s\n", ex.what()) ;
    }
    catch(std::exception& ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to resume tracking with exception %s\n", ex.what()) ;
        //	throw ex;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return volErrors;
}

bool
VolumeRecoveryProxy::DisconnectRepository(const std::string &repositoryName,const std::string& repositoryPath,const std::string& ExportType)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    bool breturn = false;

    //incase of windows nothing to unmount 
    //for linux case need to unmount repositorypath(nfs mount)
    if(unMountRepository(repositoryPath))
    {
        try
        {
            FunctionInfo funInfo;
            funInfo.m_RequestFunctionName = "DisconnectRepository";
            insertintoreqresp(funInfo.m_RequestPgs,cxArg(repositoryName),"RepositoryName" );
            insertintoreqresp(funInfo.m_RequestPgs,cxArg(ExportType),"ExportType" );
            processAPI( funInfo ) ;
            breturn = true ;
        }
        catch( VolumeRecoveryHelperException& ex )
        {
            DebugPrintf( SV_LOG_ERROR, "DisconnectRepository Failed with exception %s\n", ex.what() );
        }
        catch(ContextualException& ex)
        {
            DebugPrintf(SV_LOG_ERROR, "DisconnectRepository Failed with exception %s\n", ex.what()) ;
        }
        catch(std::exception& ex)
        {
            DebugPrintf(SV_LOG_ERROR, "DisconnectRepository Failed with exception %s\n", ex.what()) ;
        }
    }
    else
    {
        DebugPrintf(SV_LOG_ERROR, "Error unmounting repository :%s",repositoryName.c_str());	
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return breturn;
}

SVERROR
VolumeRecoveryProxy::GetMbrFileList(const std::string& strSourcePath,std::vector<std::string> &files)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ACE_stat stat ;
    SVERROR retVal = SVS_OK;
    ACE_DIR * ace_dir = NULL ;
    ACE_DIRENT * dirent = NULL ;
    std::string dir = strSourcePath ;
    int statRetVal = 0 ;
    bool recurse = true;
    ACE_HANDLE hd_in, hd_out ;
    hd_in = hd_out = ACE_INVALID_HANDLE ;
    if((statRetVal  = sv_stat(getLongPathName(dir.c_str()).c_str(),&stat)) != 0 )
    {
        DebugPrintf(SV_LOG_ERROR, "FILE LISTER :Unable to stat %s. ACE_OS::stat returned %d\n", dir.c_str(), statRetVal) ; 
        retVal =  SVS_FALSE;
    }
    else
    {
        ace_dir = sv_opendir(dir.c_str()) ;
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
                    std::string absPath = dir + ACE_DIRECTORY_SEPARATOR_CHAR_A + fileName ;

                    if( (statRetVal = sv_stat( getLongPathName(absPath.c_str()).c_str(), &stat )) != 0 )
                    {
                        DebugPrintf(SV_LOG_ERROR, "ACE_OS::stat for %s is failed with error %d \n", absPath.c_str(), statRetVal) ;
                        continue ;
                    }
                    if(stat.st_mode & S_IFREG)
                    {
                        size_t found;
                        if(found = fileName.find("mbrdata")!= std::string::npos)
                        {
                            files.push_back(fileName);
                        }
                    }
                }
            } while (dirent != NULL  ) ;
            ACE_OS::closedir( ace_dir ) ;
        }
        else
        {
            DebugPrintf(SV_LOG_ERROR, "ACE_OS::open_dir failed for %s.\n", dir.c_str()) ;
            retVal =  SVS_FALSE;;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return retVal;
}

VolumeErrorDetails  VolumeRecoveryProxy::PerformPhysicalSnapShot(const std::string& srcvol,
																 const std::string& snapShotSrcVol, const std::string& snapShotTgtVol, 
                                                                 const std::string& retentionPath, const std::string& srcVisibleCapacity, 
                                                                 const RestoreOptions& restoreOptions,
                                                                 CDPCli::cdpProgressCallback_t cdpProgressCallback,
                                                                 void * cdpProgressCallbackContext,
																 bool fsAwareCopy)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    VolumeErrorDetails volerr;
    int recoveryType = 0,start = 1, no_error_code = 0 ;
    std::vector<std::string> cdpcliArgs ;
    cdpcliArgs.push_back( "cdpcli" ) ;
    cdpcliArgs.push_back( "--recover" ) ;
    cdpcliArgs.push_back( "--source=" + snapShotSrcVol ) ;
    cdpcliArgs.push_back(  "--dest=" + snapShotTgtVol ) ;
    cdpcliArgs.push_back( "--db=" + retentionPath) ;
	cdpcliArgs.push_back( "--skip_replication_pause") ;
    SendAlertForRecovery (start , recoveryType, no_error_code , srcvol);

    if( restoreOptions.restoreType == RESTORE_TYPE_TAG )
    {
        cdpcliArgs.push_back( "--event=" +  restoreOptions.restoreEvent ) ;
    }
    else
    {
        cdpcliArgs.push_back( "--at=" + restoreOptions.restorePointInCdpCliFmt ) ;
    }

    ListVolumesWithProtectionDetails VolProtectionDetails;
    const std::string agentGUID;

    std::string applicationType = GetApplicationTagType (snapShotSrcVol,restoreOptions.restoreEvent);
    DebugPrintf (SV_LOG_DEBUG , "ApplicationType is %s \n ", applicationType.c_str() );
    if (applicationType == "FS")
        cdpcliArgs.push_back( "--app=FS" ) ;
    else 
        cdpcliArgs.push_back( "--app=USERDEFINED" ) ;

    cdpcliArgs.push_back( "--force=yes" ) ;
    cdpcliArgs.push_back( "--source_uservisible_capacity=" +  srcVisibleCapacity) ;
    cdpcliArgs.push_back( "--nodirectio" ) ;
    cdpcliArgs.push_back( "--skipchecks" ) ;
	cdpcliArgs.push_back( "--pickoldest" ) ;
	cdpcliArgs.push_back( "--pickoldest" ) ;
	if( fsAwareCopy )
	{
		cdpcliArgs.push_back( "--fsawarecopy=yes" ) ;
	}
	else
	{
		cdpcliArgs.push_back( "--fsawarecopy=no" ) ;
	}
	volerr.VolumeName = snapShotTgtVol ;
    volerr.ReturnCode = RunCDPCLI( cdpcliArgs, cdpProgressCallback, cdpProgressCallbackContext ) ; 
    start = 0;
    SendAlertForRecovery (start , recoveryType , volerr.ReturnCode , srcvol);

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return volerr ;
}


VolumeErrorDetails VolumeRecoveryProxy::PerformVirtualSnapShot(std::string& SrcVol, 
															   std::string& snapShotSrcVol, 
                                                               std::string& snapShotTgtVol, 
                                                               std::string& retentionPath, 
                                                               const std::string& srcVisibleCapacity, 
                                                               const std::string& rawSize,
                                                               const RestoreOptions& restoreOptions) 
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    VolumeErrorDetails volerr;
	if( isReFsVolume(m_ListVolumesWithProtectionDetails,  SrcVol ) )
	{
		volerr.ErrorMessage = "Recover Files operation is not supported for volumes that are having ReFS as file system" ;
		volerr.ReturnCode = 100 ;
		return volerr ;	
	}
    int recoveryType = 1,start = 1 , no_error_code = 0 ;

    std::string formattedSnapShotSrcVol, formattedRetentionPath ;
    formattedSnapShotSrcVol = snapShotSrcVol ;
    formattedRetentionPath = retentionPath ;
    Configurator* TheConfigurator = 0;
    std::replace(formattedSnapShotSrcVol.begin(), formattedSnapShotSrcVol.end(), '/', '\\') ;
    std::replace(formattedRetentionPath.begin(), formattedRetentionPath.end(), '/', '\\') ;

    DebugPrintf( SV_LOG_DEBUG, "Snapshot source volume: %s\n, target Volume: %s\n, retention path: %s\n", formattedSnapShotSrcVol.c_str(), snapShotTgtVol.c_str(), formattedRetentionPath.c_str() ) ;
    std::string volpackMountPath ;
    SendAlertForRecovery (start , recoveryType, no_error_code , SrcVol);		
	bool alreadyMounted = false ;
    if( mountVirtualVolume(formattedSnapShotSrcVol, snapShotTgtVol, volpackMountPath, alreadyMounted) )
    {
        std::vector<std::string> cdpcliArgs ;
        cdpcliArgs.push_back( "cdpcli" ) ;
        cdpcliArgs.push_back( "--vsnap" ) ;
        cdpcliArgs.push_back( "--target=" + volpackMountPath ) ;
        cdpcliArgs.push_back( "--virtual=" + snapShotTgtVol ) ;
        cdpcliArgs.push_back( "--db=" + formattedRetentionPath) ;
        if( restoreOptions.restoreType == RESTORE_TYPE_TAG )
        {
            cdpcliArgs.push_back( "--event=" + restoreOptions.restoreEvent ) ;
        }
        else
        {
            cdpcliArgs.push_back( "--at=" + restoreOptions.restorePointInCdpCliFmt) ;
        }	    

        cdpcliArgs.push_back( "--op=mount" ) ;
        cdpcliArgs.push_back( "--flags=rw" ) ;
        cdpcliArgs.push_back( "--skipchecks" ) ;
        cdpcliArgs.push_back( "--source_uservisible_capacity=" +  srcVisibleCapacity) ;
        cdpcliArgs.push_back( "--source_raw_capacity=" +  rawSize ) ;
		cdpcliArgs.push_back("--datalogpath=" + formattedRetentionPath.substr(0, formattedRetentionPath.find_last_of('\\'))) ;
        int errorCode = RunCDPCLI( cdpcliArgs ) ;
        DebugPrintf(SV_LOG_DEBUG, "Command Exit Code %d\n", errorCode) ;
        SV_ALERT_TYPE AlertType;

        if(errorCode == 1)
        {
            volerr.VolumeName = snapShotTgtVol ;
            volerr.ReturnCode = 1;
            volerr.ErrorMessage = "Success";
            AlertType = SV_ALERT_SIMPLE;
        }
        else
        {
            volerr.VolumeName = snapShotTgtVol ;
            volerr.ReturnCode = 0;
            volerr.ErrorMessage = "failed";
            DeleteVirtualSnapShot(snapShotTgtVol);
            AlertType = SV_ALERT_CRITICAL;
        }
        start = 0;
        SendAlertForRecovery (start , recoveryType, errorCode , SrcVol);		
    }
	/*
	*   The volpack need to be keep mounted until the vsnap is mounted.
	*/
	/*if( !alreadyMounted )
	{
		if( unmountVirtualVolume(volpackMountPath) )
		{
			DebugPrintf( SV_LOG_DEBUG,  "Successfully unmounted the volpack. drivename is : %s\n", volpackMountPath.c_str() ) ;
		}
		else
		{
			DebugPrintf( SV_LOG_ERROR,  "Failed to unmount the volpack. drivename is :: %s\n", volpackMountPath.c_str() ) ;
		}
	}*/
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return volerr ;
}
void VolumeRecoveryProxy::SendAlert(SV_ALERT_TYPE alertType,
									const std::string& alertMsg)
{
	if( m_serializerType == Xmlize )
	{
		try
		{
			time_t ltime;
			struct tm *today;

			time( &ltime );
			today = gmtime( &ltime );

			char szGmtTimeBuff[100]; /* To hold the time in  buffer form */

			// initialize 100 bytes of the character string to 0
			memset(szGmtTimeBuff,0,100);

			inm_sprintf_s(szGmtTimeBuff, ARRAYSIZE(szGmtTimeBuff), "(20%02d-%02d-%02d %02d:%02d:%02d):",
				today->tm_year - 100,
				today->tm_mon + 1,
				today->tm_mday,		
				today->tm_hour,
				today->tm_min,
				today->tm_sec
				);
			VolumeErrors volErrors;

			FunctionInfo funInfo;        
			std::vector<std::string> acceptableErrorCodes ;
			acceptableErrorCodes.push_back( "0" ) ;
			funInfo.m_RequestFunctionName = "updateAgentLog";
			insertintoreqresp(funInfo.m_RequestPgs,cxArg( m_agentGuid ), "1" ); 
			insertintoreqresp(funInfo.m_RequestPgs,cxArg( szGmtTimeBuff ), "2" ); 

			if (alertType == SV_ALERT_CRITICAL)
				insertintoreqresp(funInfo.m_RequestPgs,cxArg( "CRITICAL" ), "3" ); 
			else 
				insertintoreqresp(funInfo.m_RequestPgs,cxArg( "ALERT" ), "3" );
			 
			insertintoreqresp(funInfo.m_RequestPgs,cxArg( "VX" ), "4" ); 
			insertintoreqresp(funInfo.m_RequestPgs,cxArg( alertMsg ), "5" ); 
			insertintoreqresp(funInfo.m_RequestPgs,cxArg(SV_ALERT_MODULE_SNAPSHOT ), "6" ); 
			insertintoreqresp(funInfo.m_RequestPgs,cxArg( alertType ), "7" ); 
			processAPI( funInfo, acceptableErrorCodes ) ;
			//extractfromreqresp(funInfo.m_ResponsePgs, volErrors,"");		
		}
		catch( VolumeRecoveryHelperException& ex )
		{
			DebugPrintf( SV_LOG_ERROR, "Sending alert failed with exception %s\n", ex.what() );
		}
		catch(ContextualException& ex)
		{
			DebugPrintf(SV_LOG_ERROR, "Sending alert failed with exception %s\n", ex.what()) ;
		}
		catch(std::exception& ex)
		{
			DebugPrintf(SV_LOG_ERROR, "Sending alert failed with exception %s\n", ex.what()) ;
			//throw ex;
		}
		DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
	}
	else
	{
	   Configurator* TheConfigurator = 0;
		std::string configcachepath ;
		if( m_serializerType == Xmlize )
		{
			configcachepath = getConfigCachePath(m_repositoryPath, m_agentGuid) ;
		}
		if(InitializeConfigurator(&TheConfigurator, USE_ONLY_CACHE_SETTINGS, m_serializerType,configcachepath))
		{
			DebugPrintf (SV_LOG_DEBUG , "InitializeConfigurator is success ") ;
		}	
		SendAlertToCx(alertType , SV_ALERT_MODULE_SNAPSHOT, alertMsg.c_str());
	}
}

void VolumeRecoveryProxy::SendAlertForRecovery (int start, 
												int recoveryType, 
												int recoveryStatus, 
												std::string volumeName)
{
    DebugPrintf (SV_LOG_DEBUG ," volumeName is in SendAlertForRecovery %s ",volumeName.c_str() );
    SV_ALERT_TYPE AlertType;
    Configurator* TheConfigurator = 0;
    std::string alertMsg ;
    if (recoveryType == 0 ) // recover/clone Volumes
    {
        alertMsg = "Recovery/Clone Operation ";
    }
    else if (recoveryType == 1) // Recover Files
    {
        alertMsg = "File Recovery Operation "; 
    }
    if (start)
    {
        alertMsg += "Started For Volume ";
        AlertType = SV_ALERT_SIMPLE;
    }
    else 
    {
        if (recoveryStatus) 
        {
            alertMsg += "Completed For Volume ";
            AlertType = SV_ALERT_SIMPLE;
        }
        else 
        {
            alertMsg += " Failed/Aborted For Volume ";	
            AlertType = SV_ALERT_CRITICAL;
        }
    }
    alertMsg += volumeName ; 
	SendAlert(AlertType, alertMsg) ;
    return;	
}

std::string getSourceVolume (std::string &formattedSnapShotSrcVol)
{
    DebugPrintf (SV_LOG_DEBUG ," formattedSnapShotSrcVol is %s \n ",formattedSnapShotSrcVol.c_str() );
    size_t found = formattedSnapShotSrcVol.rfind("_virtualvolume");
    std::string volumeName;

    if (found == std::string::npos)
    {
        found = formattedSnapShotSrcVol.rfind("_sparsefile");
    }
    if (found != std::string::npos ) 
    {
        std::string stringWithOut__virtualvolume = formattedSnapShotSrcVol.substr(0,found);
        size_t found1 = stringWithOut__virtualvolume.find_last_of("/\\");
        if (found1  != std::string::npos )
        {
            volumeName = formattedSnapShotSrcVol.substr(found1+1,(found - (found1+1) ));
            std::transform(volumeName.begin(), volumeName.end(), volumeName.begin(), ::toupper);
        }
    }
	if (found != std::string::npos)
	{
		size_t found = formattedSnapShotSrcVol.find("VirtualVolume_");
		if (found != std::string::npos)
		{
			volumeName = formattedSnapShotSrcVol.substr ( found + strlen ("VirtualVolume_"), 1 );
			std::transform(volumeName.begin(), volumeName.end(), volumeName.begin(), ::toupper);
		}
	}
    DebugPrintf (SV_LOG_DEBUG ," volumeName is %s \n ",volumeName.c_str() );
    return volumeName;
}
VolumeErrorDetails VolumeRecoveryProxy::DeleteVirtualSnapShot(const std::string& VirtualSnapShotVolume)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    VolumeErrorDetails volerr ;

    std::vector<std::string> cdpcliArgs ;
    cdpcliArgs.push_back( "cdpcli" ) ;
    cdpcliArgs.push_back( "--vsnap" ) ;
    cdpcliArgs.push_back( "--virtual=" + VirtualSnapShotVolume ) ;
    cdpcliArgs.push_back( "--op=unmount" ) ;
    cdpcliArgs.push_back( "--force=Y" ) ;
    bool bRet = RunCDPCLI( cdpcliArgs ) ;
    if( bRet == false ) 
    {	
        volerr.VolumeName = VirtualSnapShotVolume ;
        volerr.ReturnCode = 0;
        volerr.ErrorMessage = "failed";
    }
    else
    {
        volerr.VolumeName = VirtualSnapShotVolume ;
        volerr.ReturnCode = 1;
        volerr.ErrorMessage = "Success";
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return volerr ;
}

void VolumeRecoveryProxy::BringVolumesOnline()
{
	LocalConfigurator localConfig ;
    std::string installpath = localConfig.getInstallPath() ;
	BringVolumesOnlineUsingDiskPart(installpath) ;
}
SV_ULONG VolumeRecoveryProxy::EnableAutoMount() 
{
    LocalConfigurator localConfig ;
    std::string installpath = localConfig.getInstallPath() ;
    return EnableAutoMountUsingDiskPart( installpath, localConfig.getDoNotRunDiskPart() ) ;
}

SV_ULONG VolumeRecoveryProxy::DisableAutoMount() 
{
    LocalConfigurator localConfig ;
    std::string installpath = localConfig.getInstallPath() ;
    return DisableAutoMountUsingDiskPart(installpath, localConfig.getDoNotRunDiskPart() ) ;
}

VolumeErrorDetails  VolumeRecoveryProxy::RecoverVolume(std::string& srcVol,
													   std::string& snapShotSrcVol, 
                                                       std::string& snapShotTgtVol, 
                                                       std::string& retentionPath, 
                                                       const std::string& srcVisibleCapacity, 
                                                       const RestoreOptions& restoreOptions,
                                                       CDPCli::cdpProgressCallback_t cdpProgressCallback,
                                                       void * cdpProgressCallbackContext,
													   bool fsAwareCopy)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    DebugPrintf(SV_LOG_DEBUG,"Before formatting paths snapShotSrcVol=%s\n snapShotTgtVol=%s\n retentionPath=%s\n",
        snapShotSrcVol.c_str(),snapShotTgtVol.c_str(),retentionPath.c_str());


    bool bRet = false ;
    std::string volpackMountPath ;
    VolumeErrorDetails volerr ;
    std::string formattedSnapShotSrcVol, formattedRetentionPath ;
    formattedSnapShotSrcVol = snapShotSrcVol ;
    formattedRetentionPath = retentionPath ;
#ifdef SV_WINDOWS
    std::replace(formattedSnapShotSrcVol.begin(), formattedSnapShotSrcVol.end(), '/', '\\') ;
    std::replace(formattedRetentionPath.begin(), formattedRetentionPath.end(), '/', '\\') ;
#else
    std::replace(formattedSnapShotSrcVol.begin(), formattedSnapShotSrcVol.end(), '\\', '/') ;
    std::replace(formattedRetentionPath.begin(), formattedRetentionPath.end(), '\\', '/') ;
    if( !m_repositoryPath.empty() )
    {
        //Removing duplicate forward slashes
        boost::algorithm::replace_all(formattedSnapShotSrcVol, "//", "/") ;
        boost::algorithm::replace_all(formattedRetentionPath, "//", "/") ;
        LocalConfigurator configurator ;
        std::string hostId = configurator.getHostId() ;
        std::string hostIdInLowerCase = hostId; 
        boost::algorithm::to_lower(hostIdInLowerCase); 
        boost::algorithm::replace_all(formattedSnapShotSrcVol, hostIdInLowerCase, hostId) ;
    }
#endif
	DebugPrintf(SV_LOG_DEBUG, "snapshot source volume %s\n", snapShotSrcVol.c_str()) ;
	DebugPrintf(SV_LOG_DEBUG, "formatted snapshot source volume %s\n", formattedSnapShotSrcVol.c_str()) ;
	DebugPrintf(SV_LOG_DEBUG, "formatted snapshot source volume %s\n", formattedSnapShotSrcVol.c_str()) ;
	DebugPrintf(SV_LOG_DEBUG, "retention path %s\n", retentionPath.c_str()) ;
	DebugPrintf(SV_LOG_DEBUG, "formatted retention path %s\n", formattedRetentionPath.c_str()) ;
	bool alreadyMounted = false ;
	if( mountVirtualVolume(formattedSnapShotSrcVol, snapShotTgtVol, volpackMountPath, alreadyMounted) )
	{
		DebugPrintf( SV_LOG_DEBUG, "MountedsnapShotSrcVol Volpack Volume : %s\n Access Point after Mounting the volpack : %s\n", 
			snapShotSrcVol.c_str(), volpackMountPath.c_str() ) ;
		StopShellHWDetectionService() ;
		DisableAutoMount() ;
		volerr = PerformPhysicalSnapShot(srcVol, volpackMountPath, snapShotTgtVol, formattedRetentionPath, 
			srcVisibleCapacity, restoreOptions, cdpProgressCallback, cdpProgressCallbackContext, fsAwareCopy) ;
		StartShellHWDetactionService() ;
		EnableAutoMount() ;
		if( !alreadyMounted )
		{
			if( unmountVirtualVolume(volpackMountPath) )
			{
				DebugPrintf( SV_LOG_DEBUG,  "Successfully unmounted the volpack. drivename is : %s\n", volpackMountPath.c_str() ) ;
			}
			else
			{
				DebugPrintf( SV_LOG_ERROR,  "Failed to unmount the volpack. drivename is :: %s\n", volpackMountPath.c_str() ) ;
			}
		}
	}	
	DebugPrintf(SV_LOG_DEBUG, "volpack mount path %s\n", volpackMountPath.c_str()) ;
	
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return volerr ;
}

VolumeErrors
VolumeRecoveryProxy::PauseProtectionForVolumeByAgentGUID(PauseResumeDetails& pauseProInfo,const std::string& agentGUID)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
	VolumeErrors volErrors;
    try
    {
		FunctionInfo funInfo;        
		std::vector<std::string> acceptableErrorCodes ;
		acceptableErrorCodes.push_back( "0" ) ;
		acceptableErrorCodes.push_back( "109" ) ;
		if(m_repositoryPath.empty())
		{
			funInfo.m_RequestFunctionName = "PauseProtectionForVolumeByAgentGUID";
		}
		else
		{
			funInfo.m_RequestFunctionName = "PauseProtectionForVolume";
		}
		insertintoreqresp(funInfo.m_RequestPgs,cxArg( agentGUID ),"AgentGUID" ); 
		insertintoreqresp(funInfo.m_RequestPgs,cxArg( pauseProInfo ),"" );
		insertintoreqresp(funInfo.m_RequestPgs,cxArg( "Clone Operation" ),"Pause Reason" );
		processAPI( funInfo, acceptableErrorCodes ) ;
		extractfromreqresp(funInfo.m_ResponsePgs, volErrors,"");		
    }
    catch( VolumeRecoveryHelperException& ex )
    {
        DebugPrintf( SV_LOG_ERROR, "Failed to pause protection with exception %s\n", ex.what() );
    }
    catch(ContextualException& ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to pause protection with exception %s\n", ex.what()) ;
    }
    catch(std::exception& ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to pause protection with exception %s\n", ex.what()) ;
        //throw ex;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return volErrors;
}

VolumeErrors
VolumeRecoveryProxy::ResumeProtectionForVolumeByAgentGUID(PauseResumeDetails& resumeProInfo,const std::string& agentGUID)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

    VolumeErrors volErrors;
    try
    {
        FunctionInfo funInfo;        
        std::vector<std::string> acceptableErrorCodes ;
        acceptableErrorCodes.push_back( "0" ) ;
        acceptableErrorCodes.push_back( "103" ) ;
        if(m_repositoryPath.empty())
        {
            funInfo.m_RequestFunctionName = "ResumeProtectionForVolumeByAgentGUID";
        }
        else
        {
            funInfo.m_RequestFunctionName = "ResumeProtectionForVolume";
        }
        insertintoreqresp(funInfo.m_RequestPgs,cxArg( agentGUID ),"AgentGUID" ); 
        insertintoreqresp(funInfo.m_RequestPgs,cxArg( resumeProInfo ),"" );

        processAPI( funInfo, acceptableErrorCodes ) ;
        extractfromreqresp(funInfo.m_ResponsePgs, volErrors,"");		
    }
    catch( VolumeRecoveryHelperException& ex )
    {
        DebugPrintf( SV_LOG_ERROR, "Failed to resume protection with exception %s\n", ex.what() );
    }
    catch(ContextualException& ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to resume protection with exception %s\n", ex.what()) ;
    }
    catch(std::exception& ex)
    {
        DebugPrintf(SV_LOG_ERROR, "Failed to resume protection with exception %s\n", ex.what()) ;
        //throw ex;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return volErrors;
}


bool VolumeRecoveryProxy::GetFreeDriveLettersForRepoMount(std::list<std::string>& driveList, 
														  const std::string& agentGuid,
														  const std::string& repoPath)
{
	FunctionInfo funInfo;
	AvailableDriveLetters availableDriveLetters ;
	funInfo.m_RequestFunctionName = "ListAvailableDrives";
	ValueType vt ;
	vt.value = agentGuid ;
	funInfo.m_RequestPgs.m_Params.insert( std::make_pair( "AgentGuid", vt)) ;
	vt.value = repoPath ;
	funInfo.m_RequestPgs.m_Params.insert( std::make_pair( "RepoPath", vt)) ;
	bool bret = false ;
	std::vector<std::string> acceptableErrorCodes ;
	acceptableErrorCodes.push_back( "0" ) ;
	try
	{
		processAPI( funInfo, acceptableErrorCodes ) ;
		extractfromreqresp(funInfo.m_ResponsePgs,availableDriveLetters,"");
		driveList = availableDriveLetters.availableDrives ;
		bret = true ;
	}
	catch( VolumeRecoveryHelperException& ex )
	{
		DebugPrintf( SV_LOG_ERROR, "Failed to find the free drive letters with exception %s\n", ex.what() );
	}
	catch(ContextualException& ex)
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to find the free drive letters with exception %s\n", ex.what()) ;
	}
	catch(std::exception& ex)
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to find the free drive letters with exception %s\n", ex.what()) ;
		//throw ex;
	}

	return bret ;

}

void VolumeRecoveryProxy::setConfigStoreLocation(const std::string& agentguid)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", agentguid.c_str()) ;
	m_agentGuid = agentguid ;
}


void VolumeRecoveryProxy::GetAllowedOperations(LicenseInfo& licenseInfo)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	FunctionInfo funInfo;
	AvailableDriveLetters availableDriveLetters ;
	funInfo.m_RequestFunctionName = "IsLicenseExpired";
	ValueType vt ;
	bool retvalue ;
	licenseInfo.m_cloneVolumesEnabled = true ;
	licenseInfo.m_hostToTargetEnabled = true ;
	licenseInfo.m_recoverFilesEnabled = true ;
	licenseInfo.m_recoverVolumesEnabled = true ;
	std::vector<std::string> acceptableErrorCodes ;
	acceptableErrorCodes.push_back("0") ;
	try
	{
		processAPI( funInfo, acceptableErrorCodes ) ;
		extractfromreqresp(funInfo.m_ResponsePgs,retvalue,"1");
		if( retvalue )
		{
			licenseInfo.m_cloneVolumesEnabled = false ;
			licenseInfo.m_hostToTargetEnabled = false ;
			licenseInfo.m_recoverFilesEnabled = false ;
			licenseInfo.m_recoverVolumesEnabled = false ;
		}
	}
	catch( VolumeRecoveryHelperException& ex )
	{
		DebugPrintf( SV_LOG_ERROR, "Failed to find allowed operations with exception %s\n", ex.what() );
	}
	catch(ContextualException& ex)
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to find allowed operations with exception %s\n", ex.what()) ;
	}
	catch(std::exception& ex)
	{
		DebugPrintf(SV_LOG_ERROR, "Failed to find allowed operations with exception %s\n", ex.what()) ;
		//throw ex;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

bool RegisterSecurityHandle(const std::string& fqdnusername, 
							const std::string& password,
							std::string& errormsg)
{

	INM_ERROR error ;
	if( fqdnusername.empty() )
	{
		return true ;
	}
	std::string username, domain ;
    if( fqdnusername.find("\\") != std::string::npos )
    {
        domain = fqdnusername.substr(0, fqdnusername.find("\\") ) ;
        username = fqdnusername.substr(fqdnusername.find("\\") + 1) ;
    }
    else
    {
        domain = ".";
        username = fqdnusername ;
    }

	return ImpersonateAccessToShare( username, password, domain, error, errormsg) ; 
}
