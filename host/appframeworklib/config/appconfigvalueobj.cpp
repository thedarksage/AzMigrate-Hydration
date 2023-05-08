#include "appconfigvalueobj.h"
#include "appparser.h"
#include "inmsafecapis.h"

AppConfigValueObjectPtr AppConfigValueObject::m_cvo_obj ;

AppConfigValueObject::AppConfigValueObject()
{
  
}

AppConfigValueObject::~AppConfigValueObject()    
{
  
}

void AppConfigValueObject::createInstance()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    char tempString[1000];
    if(m_cvo_obj == NULL )
    {
            DebugPrintf(SV_LOG_DEBUG, "config value currently doesnt exist. creating it for the first time\n") ;
            
            try
            {
                m_cvo_obj.reset( new AppConfigValueObject() );
                if( m_cvo_obj.get() == NULL )
				{
                    throw AppException("failed to allocate memory to cvo object");
				}
            }
            catch(InvalidInput inv)
            {
                DebugPrintf(SV_LOG_ERROR, "%s \n",inv.getInvalidInput().c_str()) ;
				inm_sprintf_s(tempString, ARRAYSIZE(tempString), "%s", inv.getInvalidInput().c_str());
                throw AppException(tempString);
            }         
            catch(ParseException ipe)
            {   
                DebugPrintf(SV_LOG_ERROR, "%s \n",ipe.what()) ;
				inm_sprintf_s(tempString, ARRAYSIZE(tempString), "%s", ipe.what());
                throw AppException(tempString);
            }
    }
    else
    {
            DebugPrintf(SV_LOG_DEBUG, "ConvigValueObject is already existed..");                         
    }
        
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;

}
AppConfigValueObjectPtr AppConfigValueObject::getInstance()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    if(m_cvo_obj.get() == NULL)
    {
        DebugPrintf(SV_LOG_ERROR, "ConfigValueObj is not yet created.... returning null") ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
    return m_cvo_obj;

}

void AppConfigValueObject::UpdateConfFile(std::string filePath)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    ConfigValueParser cvp_obj(filePath);
    try
    {
        cvp_obj.UpdateConfigFile(*m_cvo_obj);    
    }
    catch(InvalidInput inv)
    {
        DebugPrintf(SV_LOG_ERROR, "%s \n",inv.getInvalidInput().c_str()) ;
    }         
    catch(ParseException ipe)
    {   
        DebugPrintf(SV_LOG_ERROR, "%s \n",ipe.what()) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

void AppConfigValueObject::ParseConfFile(std::string filePath)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    ConfigValueParser cvp_obj(filePath);
    try
    {
        cvp_obj.parseConfigFileInput(*m_cvo_obj);   
    }
    catch(InvalidInput inv)
    {
        DebugPrintf(SV_LOG_ERROR, "%s \n",inv.getInvalidInput().c_str()) ;
    }         
    catch(ParseException ipe)
    {   
        DebugPrintf(SV_LOG_ERROR, "%s \n",ipe.what()) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

std::string AppConfigValueObject::getApplicationType() const
{
    return m_VacpOptions.m_ApplicationType;
}
std::list<std::string> AppConfigValueObject::getApplicationVolumeList() const
{
    return m_VacpOptions.m_AppVolumesList;
}
std::list<std::string> AppConfigValueObject::getCustomVolumesList() const
{
    return m_VacpOptions.m_CustomVolumesList;
}
std::string AppConfigValueObject::getVolumeGUID() const
{
    return m_VacpOptions.m_VolumeGUID;
}
std::string AppConfigValueObject::getTagName() const
{
    return m_VacpOptions.m_TagName;
}
std::string AppConfigValueObject::getWriterInstance() const
{
    return m_VacpOptions.m_WriterInstance;
}
bool AppConfigValueObject::getCrashConsistency() const
{
    return m_VacpOptions.m_CrashConsistency;
}
bool AppConfigValueObject::getSkipDriverCheck() const
{
    return m_VacpOptions.m_SkipDriverCheck;
}
bool AppConfigValueObject::getPerformFullBackup() const
{
    return m_VacpOptions.m_PerformFullBackup;
}
std::string AppConfigValueObject::getProviderGUID() const
{
    return m_VacpOptions.m_ProviderGUID;
}
bool AppConfigValueObject::getSynchronousTag() const
{
    return m_VacpOptions.m_SynchronousTag;
}
SV_ULONG AppConfigValueObject::getTimeout() const
{
    return m_VacpOptions.m_Timeout;
}
bool AppConfigValueObject::getRemoteTag() const
{
    return m_VacpOptions.m_RemoteTag;
}
std::string AppConfigValueObject::getServerDevice() const
{
    return m_VacpOptions.m_ServerDevice;
}
std::string AppConfigValueObject::getIPAddress() const
{
    return m_VacpOptions.m_IPAddress;
}
SV_ULONG AppConfigValueObject::getport() const
{
    return m_VacpOptions.m_port;
}
std::string AppConfigValueObject::getContext() const
{
    return m_VacpOptions.m_Context;
}
std::string AppConfigValueObject::getVacpCommand() const
{
    return m_VacpOptions.m_VacpCommand;
}
std::string AppConfigValueObject::getLatestCommonTime() const
{
    return m_RecoveryPoint.m_LatestCommonTime;
}
std::map<std::string,RecoveryOptions> AppConfigValueObject::getRecoveryOptions() const
{
    return m_RecoveryPoint.m_RecoveryOptions;
}
std::list<std::string> AppConfigValueObject::getSQLInstanceNameList() const
{
    return m_SQLInstanceNames.m_InstanceNameList;
}
std::map<std::string,std::string> AppConfigValueObject::getCustomProtectedPair() const
{
    return m_RecoveryPoint.m_CustomProctedPairs;
}
std::map<std::string, std::string> AppConfigValueObject::getPairStatus() const
{
	return m_PairStatus.m_Pairs;
}
void AppConfigValueObject::setApplicationType(std::string const& value)
{
    m_VacpOptions.m_ApplicationType = value;
}
void AppConfigValueObject::setApplicationVolume(std::string const& value)
{
    m_VacpOptions.m_AppVolumesList.push_back(value);
}
void AppConfigValueObject::setCustomVolume(std::string const& value)
{
    m_VacpOptions.m_CustomVolumesList.push_back(value);
}
void AppConfigValueObject::setVolumeGUID(std::string const& value)
{
    m_VacpOptions.m_VolumeGUID = value;
}
void AppConfigValueObject::setTagName(std::string const& value)
{
    m_VacpOptions.m_TagName = value;
}
void AppConfigValueObject::setWriterInstance(std::string const& value)
{
    m_VacpOptions.m_WriterInstance = value;
}
void AppConfigValueObject::setCrashConsistency(bool const& value)
{
    m_VacpOptions.m_CrashConsistency = value;
}
void AppConfigValueObject::setSkipDriverCheck(bool const& value)
{
    m_VacpOptions.m_SkipDriverCheck = value;
}
void AppConfigValueObject::setPerformFullBackup(bool const& value)
{
    m_VacpOptions.m_PerformFullBackup = value;
}
void AppConfigValueObject::setProviderGUID(std::string const& value)
{
    m_VacpOptions.m_ProviderGUID = value;
}
void AppConfigValueObject::setTimeout(SV_ULONG  const& value)
{
    m_VacpOptions.m_Timeout = value;
}
void AppConfigValueObject::setRemoteTag(bool const& value)
{
    m_VacpOptions.m_RemoteTag = value;
}
void AppConfigValueObject::setServerDevice(std::string const& value)
{
    m_VacpOptions.m_ServerDevice = value;
}
void AppConfigValueObject::setSynchronousTag(bool const& value)
{
    m_VacpOptions.m_SynchronousTag = value;
}
void AppConfigValueObject::setIPAddress(std::string const& value)
{
    m_VacpOptions.m_IPAddress = value;
}
void AppConfigValueObject::setport(SV_ULONG const& value)
{
    m_VacpOptions.m_port = value;
}
void AppConfigValueObject::setContext(std::string const& value)
{
    m_VacpOptions.m_Context = value;
}
void AppConfigValueObject::setVacpCommand(std::string const& value)
{
    m_VacpOptions.m_VacpCommand = value;
}
void AppConfigValueObject::setLatestCommonTime(bool const& value)
{
    m_RecoveryPoint.m_LatestCommonTime = value;
}
void AppConfigValueObject::setRecoveryOptions(RecoveryOptions const& options)
{
    m_RecoveryPoint.m_RecoveryOptions.insert(std::make_pair(options.m_VolumeName,options));
}

void AppConfigValueObject::setSQLInstanceNameList( std::string const& instanceName)
{
    m_SQLInstanceNames.m_InstanceNameList.push_back(instanceName);
}

void AppConfigValueObject::setCustomProtectedPair( std::string const& srcvol,std::string const& tgtvol)
{
    m_RecoveryPoint.m_CustomProctedPairs.insert(std::make_pair(srcvol,tgtvol));
}

void AppConfigValueObject::setPairStatus(const std::string &volName, const std::string &repStatus)
{
	m_PairStatus.m_Pairs.insert(std::make_pair(volName,repStatus));
}
void AppConfigValueObject::ClearLists()
{
    m_VacpOptions.m_AppVolumesList.clear();
    m_VacpOptions.m_CustomVolumesList.clear();
    m_SQLInstanceNames.m_InstanceNameList.clear();
    m_RecoveryPoint.m_RecoveryOptions.clear();
    m_RecoveryPoint.m_AppVolumesList.clear();
    m_RecoveryPoint.m_CustomVolumesList.clear();
    m_RecoveryPoint.m_CustomProctedPairs.clear();
	m_PairStatus.m_Pairs.clear();
}

