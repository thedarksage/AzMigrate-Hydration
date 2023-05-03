#ifdef SV_WINDOWS
#include <Windows.h>
#endif
#include "volumeconfig.h"
#include "confschemamgr.h"
#include "confschemareaderwriter.h"
#include "xmlizeinitialsettings.h"
#include "xmlizevolumegroupsettings.h"
#include "generalupdate.h"
#include "protectionpairconfig.h"
#include "alertconfig.h"
#include "host.h"
#include "inmsdkutil.h"
#include "auditconfig.h"
#include <boost/algorithm/string/replace.hpp>
#include "fileconfigurator.h"
#include "util.h"
#include "agentconfig.h"
#include "portablehelpersmajor.h"

VolumeConfigPtr VolumeConfig::m_volumeConfigPtr ;

VolumeConfigPtr VolumeConfig::GetInstance(bool loadgrp)
{
    if( !m_volumeConfigPtr )
    {
        m_volumeConfigPtr.reset( new VolumeConfig() ) ;
    }
	if( loadgrp )
	{
		m_volumeConfigPtr->loadVolumeGrp() ;
	}
    return m_volumeConfigPtr ;
}
bool VolumeConfig::isModified()
{
    return m_isModified ;
}
void VolumeConfig::loadVolumeGrp()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n",FUNCTION_NAME) ;
    ConfSchemaReaderWriterPtr confSchemaReaderWriter = ConfSchemaReaderWriter::GetInstance() ;

    m_VolumeGroup = confSchemaReaderWriter->getGroup(m_volumeAttrNamesObj.GetName());
    m_RepositoryGroup = confSchemaReaderWriter->getGroup(m_repositoryAttrNamesObj.GetName());
    m_firstRegistration = IsFirsRegistration() ;

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
void VolumeConfig::RemoveVolumeResizeHistory(const std::list<std::string> &volumeList)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n",FUNCTION_NAME) ;

	VolumeConfigPtr volumeConfigPtr = VolumeConfig::GetInstance(); 
	std::list<std::string>::const_iterator volumeListIter = volumeList.begin(); 
	ConfSchema::Object* volumeObj; 
	while (volumeListIter != volumeList.end())
	{
		GetVolumeObjectByVolumeName (volumeListIter->c_str(),&volumeObj); 
		volumeConfigPtr->RemoveVolumeResizeHistory(volumeObj); 
		volumeListIter ++; 
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n",FUNCTION_NAME) ;
	return; 
}
void VolumeConfig::RemoveVolumeResizeHistory(ConfSchema::Object* volumeObj)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n",FUNCTION_NAME) ;
    ConfSchema::Group* volumeResizeHistoryGrp = NULL ;
    ConfSchema::GroupsIter_t grpiter = volumeObj->m_Groups.find( m_volResizeHistoryAttrNamesObj.GetName() ) ;
    if( grpiter != volumeObj->m_Groups.end() )
    {
		grpiter->second.m_Objects.clear();
    }
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n",FUNCTION_NAME) ;
	return; 
}


void VolumeConfig::GetVolumeResizeHistory(ConfSchema::Object* volumeObj,
                                          std::map<time_t, volumeResizeDetails>& volumeResizeHistoryMap)
{
    ConfSchema::Group* volumeResizeHistoryGrp = NULL ;
    ConfSchema::GroupsIter_t grpiter = volumeObj->m_Groups.find( m_volResizeHistoryAttrNamesObj.GetName() ) ;
    if( grpiter != volumeObj->m_Groups.end() )
    {
        volumeResizeHistoryGrp = &(grpiter->second) ;
        ConfSchema::ObjectsIter_t resizeHistoryObjIter = volumeResizeHistoryGrp->m_Objects.begin() ;
        while( resizeHistoryObjIter != volumeResizeHistoryGrp->m_Objects.end() )
        {
            ConfSchema::Object* resizeHistoryObj = &(resizeHistoryObjIter->second) ;
            ConfSchema::Attrs_t& resizeHistoryAttrs = resizeHistoryObj->m_Attrs ;
            volumeResizeDetails details ;
            ConfSchema::AttrsIter_t attrIter ;
            attrIter = resizeHistoryAttrs.find( m_volResizeHistoryAttrNamesObj.GetTimeStampAttrName() );
            details.m_changeTimeStamp = boost::lexical_cast<time_t>( attrIter->second ) ;
            attrIter = resizeHistoryAttrs.find( m_volResizeHistoryAttrNamesObj.GetCurrentCapacityAttrName());
            details.m_newVolumeCapacity = boost::lexical_cast<SV_LONGLONG>( attrIter->second ) ;
            attrIter = resizeHistoryAttrs.find( m_volResizeHistoryAttrNamesObj.GetCurrentRawSizeAttrName()) ;
            details.m_newVolumeRawSize = boost::lexical_cast<SV_LONGLONG>( attrIter->second ) ;
            attrIter = resizeHistoryAttrs.find( m_volResizeHistoryAttrNamesObj.GetOldCapacityAttrName()) ;
            details.m_oldVolumeCapacity = boost::lexical_cast<SV_LONGLONG>( attrIter->second ) ;
            attrIter = resizeHistoryAttrs.find( m_volResizeHistoryAttrNamesObj.GetOldRawSizeAttrName()) ;
            details.m_oldVolumeRawSize = boost::lexical_cast<SV_LONGLONG>( attrIter->second ) ;
			attrIter = resizeHistoryAttrs.find( m_volResizeHistoryAttrNamesObj.GetIsSparseFilesCreatedAttrName());
			if (attrIter != resizeHistoryAttrs.end())
			{
				details.m_IsSparseFilesCreated = attrIter->second ;
			}
			else 
			{
				details.m_IsSparseFilesCreated = "0";
			}
            volumeResizeHistoryMap.insert(std::make_pair( details.m_changeTimeStamp, details)) ;
            resizeHistoryObjIter++ ;
        }
    }
}

bool VolumeConfig::GetVolumeProperties( std::map<std::string, volumeProperties>& volumePropertiesMap )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    ConfSchema::ObjectsIter_t volumeObjIter = m_VolumeGroup->m_Objects.begin() ;
    while( volumeObjIter != m_VolumeGroup->m_Objects.end() )
    {
        bRet = true ;
        ConfSchema::Object* volumeObj = &(volumeObjIter->second) ;
        ConfSchema::Attrs_t& volumeAttrs = volumeObj->m_Attrs ;
        ConfSchema::AttrsIter_t attrIter ;
        volumeProperties volProps ;
        if( volumeAttrs.find(m_volumeAttrNamesObj.GetCapacityAttrName()) != volumeAttrs.end() )
        {
            volProps.capacity = volumeAttrs.find(m_volumeAttrNamesObj.GetCapacityAttrName())->second ;
        }
        if( volumeAttrs.find(m_volumeAttrNamesObj.GetFileSystemAttrName()) != volumeAttrs.end() )
        {
            volProps.fileSystemType = volumeAttrs.find(m_volumeAttrNamesObj.GetFileSystemAttrName())->second ;
        }
        if( volumeAttrs.find(m_volumeAttrNamesObj.GetFreeSpaceAttrName()) != volumeAttrs.end() )
        {
            volProps.freeSpace = volumeAttrs.find(m_volumeAttrNamesObj.GetFreeSpaceAttrName())->second ;
        }
        if( volumeAttrs.find(m_volumeAttrNamesObj.GetIsSystemVolumeAttrName()) != volumeAttrs.end() )
        {
            volProps.isSystemVolume = volumeAttrs.find(m_volumeAttrNamesObj.GetIsSystemVolumeAttrName())->second ;
        }
        if( volumeAttrs.find(m_volumeAttrNamesObj.GetMountPointAttrName()) != volumeAttrs.end() )
        {
            volProps.mountPoint = volumeAttrs.find(m_volumeAttrNamesObj.GetMountPointAttrName())->second ;
        }
        if( volumeAttrs.find(m_volumeAttrNamesObj.GetNameAttrName()) != volumeAttrs.end() )
        {
            volProps.name = volumeAttrs.find(m_volumeAttrNamesObj.GetNameAttrName())->second ;
        }
        if( volumeAttrs.find(m_volumeAttrNamesObj.GetRawSizeAttrName()) != volumeAttrs.end() )
        {
            volProps.rawSize = volumeAttrs.find(m_volumeAttrNamesObj.GetRawSizeAttrName())->second ;
        }
        if( volumeAttrs.find(m_volumeAttrNamesObj.GetVolumeLabelAttrName()) != volumeAttrs.end() )
        {
            volProps.volumeLabel = volumeAttrs.find(m_volumeAttrNamesObj.GetVolumeLabelAttrName())->second ;
        }
        if( volumeAttrs.find(m_volumeAttrNamesObj.GetVolumeTypeAttrName()) != volumeAttrs.end() )
        {
            volProps.VolumeType = volumeAttrs.find(m_volumeAttrNamesObj.GetVolumeTypeAttrName())->second ;
        }
        if( volumeAttrs.find(m_volumeAttrNamesObj.GetWriteCacheStateAttrName()) != volumeAttrs.end() )
        {
            volProps.writeCacheState = volumeAttrs.find(m_volumeAttrNamesObj.GetWriteCacheStateAttrName())->second ;
        }
        GetVolumeResizeHistory( volumeObj, volProps.volumeResizeHistory ) ;
        volumePropertiesMap.insert( std::make_pair( volProps.name, volProps) ) ;
        volumeObjIter++ ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

void VolumeConfig::GetVolumes(std::list<std::string>& volumes)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::map<std::string, volumeProperties> volumePropertiesMap ;
    GetVolumeProperties( volumePropertiesMap ) ;
    std::map<std::string, volumeProperties>::const_iterator volumePropertiesIter = volumePropertiesMap.begin() ;
    while( volumePropertiesIter != volumePropertiesMap.end() )
    {
        volumes.push_back(volumePropertiesIter->first) ;
        volumePropertiesIter++ ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

bool VolumeConfig::IsTargetVolumeAvailable(const std::string& volumeName)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::map<std::string, volumeProperties> volumePropertiesMap ;
    bool bRet = false ;
    GetVolumeProperties( volumePropertiesMap ) ;
    std::map<std::string, volumeProperties>::const_iterator volpropIter ;
    if( (volpropIter = volumePropertiesMap.find( volumeName)) 
        != volumePropertiesMap.end() )
    {
        if( volpropIter->second.VolumeType == "5" || 
            volpropIter->second.VolumeType == "6" )
        {
            bRet = true ;
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool VolumeConfig::IsVolumeAvailable(const std::string& volumeName)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::map<std::string, volumeProperties> volumePropertiesMap ;
    bool bRet = false ;
    GetVolumeProperties( volumePropertiesMap ) ;
    std::map<std::string, volumeProperties>::const_iterator volpropIter ;
    if( (volpropIter = volumePropertiesMap.find( volumeName)) 
        != volumePropertiesMap.end() )
    {
        if( volpropIter->second.VolumeType == "5" )
        {
            bRet = true ;
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}


std::map<std::string, volumeProperties> VolumeConfig::GetGeneralVolumes(std::list<std::string>& volumes)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::map<std::string, volumeProperties> volumePropertiesMap ;
    GetVolumeProperties( volumePropertiesMap ) ;
    std::map<std::string, volumeProperties>::const_iterator volumePropertiesIter = volumePropertiesMap.begin() ;
    while( volumePropertiesIter != volumePropertiesMap.end() )
    {
        if( volumePropertiesIter->second.VolumeType.compare("5") == 0 )
        {
            volumes.push_back(volumePropertiesIter->first) ;
        }
        volumePropertiesIter++ ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return volumePropertiesMap ;
}

void VolumeConfig::GetVirtualVolumes(std::list<std::string>& volumes)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::map<std::string, volumeProperties> volumePropertiesMap ;
    GetVolumeProperties( volumePropertiesMap ) ;
    std::map<std::string, volumeProperties>::const_iterator volumePropertiesIter = volumePropertiesMap.begin() ;
    while( volumePropertiesIter != volumePropertiesMap.end() )
    {
        if( volumePropertiesIter->second.VolumeType.compare("2") == 0 )
        {
            volumes.push_back(volumePropertiesIter->first) ;
        }
        volumePropertiesIter++ ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void VolumeConfig::GetVolPackVolumes(std::list<std::string>& volumes)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::map<std::string, volumeProperties> volumePropertiesMap ;
    GetVolumeProperties( volumePropertiesMap ) ;
    std::map<std::string, volumeProperties>::const_iterator volumePropertiesIter = volumePropertiesMap.begin() ;
    while( volumePropertiesIter != volumePropertiesMap.end() )
    {
        if( volumePropertiesIter->second.VolumeType.compare("6") == 0 )
        {
            volumes.push_back(volumePropertiesIter->first) ;
        }
        volumePropertiesIter++ ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

SV_ULONGLONG VolumeConfig::GetVolPackVolumeSizeOnDisk (void)
{	
	SV_ULONGLONG volpackCapacity = 0;
	std::list<std::string> volpackVolumes;
	std::list<std::string>::iterator volpackVolumesIter;
	GetVolPackVolumes (volpackVolumes);
	volpackVolumesIter = volpackVolumes.begin();
	while (volpackVolumesIter != volpackVolumes.end())
	{
		SV_LONGLONG sizeOnDisk = 0;
		GetVolumeSizeOnDisk(*volpackVolumesIter, sizeOnDisk);
		DebugPrintf (SV_LOG_DEBUG , "sizeOnDisk is %lld" ,sizeOnDisk );
		volpackCapacity += sizeOnDisk;
		DebugPrintf (SV_LOG_DEBUG , "volpackCapacity is %lld" ,volpackCapacity );

		volpackVolumesIter ++; 
	}
	return volpackCapacity;
}


void VolumeConfig::GetRepositoryVolumes(std::list<std::string>& repoVolumes)
{
    ConfSchema::ObjectsIter_t repoObjectIter = m_RepositoryGroup->m_Objects.begin() ;
    while( repoObjectIter != m_RepositoryGroup->m_Objects.end() )
    {
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = repoObjectIter->second.m_Attrs.find(m_repositoryAttrNamesObj.GetMountPointAttrName()) ;
        repoVolumes.push_back(attrIter->second) ;
        repoObjectIter++ ;
    }
}

void VolumeConfig::GetProtectableVolumes(const std::list<std::string>& excludedVolumes,
                                         std::list<std::string>& protectableVolumes)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    //loadVolumeGrp() ;
    std::list<std::string> volumes, repoVolumes ;
    std::map<std::string, volumeProperties> volPropsMap = GetGeneralVolumes(volumes) ;
    GetRepositoryVolumes(repoVolumes) ;
    std::list<std::string>::const_iterator volIter = volumes.begin() ;
    while( volIter != volumes.end() )
    {
        if( std::find( excludedVolumes.begin(), excludedVolumes.end(), *volIter ) == 
            excludedVolumes.end() )
        {
            if( std::find( repoVolumes.begin(), repoVolumes.end(), *volIter ) == 
                repoVolumes.end() )
            {
                std::map<std::string, volumeProperties>::iterator volPropIterator = volPropsMap.find( *volIter ) ;
                if( volPropIterator != volPropsMap.end() )
                {
                    SV_ULONGLONG ullCapacity = boost::lexical_cast<SV_ULONGLONG>( volPropIterator->second.capacity ) ;
					std::string fileSystemType = ( volPropIterator->second.fileSystemType ) ;
                    if( ullCapacity > 0 && ( fileSystemType == "NTFS" || fileSystemType == "ExtendedFAT" || fileSystemType == "ReFS" ) )
                        protectableVolumes.push_back(*volIter) ;
                }                
            }
        }
        volIter++ ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void VolumeConfig::GetNonProtectedVolumes(const std::list<std::string>& protectedVolumes,
                                          const std::list<std::string>& excludedVolumes,
                                          std::list<std::string>& nonprotectedVolumes)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::list<std::string> volumes, repoVolumes ;
    //loadVolumeGrp() ;
    GetGeneralVolumes(volumes) ;
    GetRepositoryVolumes(repoVolumes) ;
    std::list<std::string>::const_iterator volIter = volumes.begin() ;
    while( volIter != volumes.end() )
    {
        if( std::find( excludedVolumes.begin(), excludedVolumes.end(), *volIter ) == 
            excludedVolumes.end() )
        {
            if( std::find( repoVolumes.begin(), repoVolumes.end(), *volIter ) == 
                repoVolumes.end() )
            {
                if( std::find( protectedVolumes.begin(), protectedVolumes.end(), *volIter ) == protectedVolumes.end() )
                {
                    nonprotectedVolumes.push_back(*volIter) ;
                }
            }
        }
        volIter++ ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void VolumeConfig::GetSystemVolumes(std::list<std::string>& systemVolumes)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::map<std::string, volumeProperties> volumePropertiesMap ;
    GetVolumeProperties( volumePropertiesMap ) ;
    std::map<std::string, volumeProperties>::const_iterator volumePropertiesIter = volumePropertiesMap.begin() ;
    while( volumePropertiesIter != volumePropertiesMap.end() )
    {
        if( volumePropertiesIter->second.isSystemVolume.compare("1") == 0 )
        {
            systemVolumes.push_back(volumePropertiesIter->first) ;
            DebugPrintf (SV_LOG_DEBUG,"System Volume is %s \n",volumePropertiesIter->first.c_str());
        }
        volumePropertiesIter++ ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

bool VolumeConfig::IsSystemVolume(const std::string& volumeName)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::list<std::string> systemVolumes ;
    GetSystemVolumes(systemVolumes) ;
    bool bRet = false ;
    if( std::find( systemVolumes.begin(), systemVolumes.end(), volumeName ) != systemVolumes.end() )
    {
        if( volumeName.compare("C:\\SRV") == 0 )
        {
            bRet = true ;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool VolumeConfig::IsBootVolume(const std::string& volumeName)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    std::list<std::string> systemVolumes ;
    GetSystemVolumes(systemVolumes) ;
    DebugPrintf (SV_LOG_DEBUG, "volumeName in BootVolume is  %s \n ",volumeName.c_str() );
    bool bRet = false ;
    if( std::find( systemVolumes.begin(), systemVolumes.end(), volumeName ) != systemVolumes.end() )
    {
        if( volumeName.compare("C:\\SRV") != 0 )
        {
            bRet = true ;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

std::string VolumeConfig::GetBootVolume( )
{
    ConfSchema::ObjectsIter_t volIter ;
    std::string bootvolume ;
    volIter = m_VolumeGroup->m_Objects.begin() ;
    while( volIter != m_VolumeGroup->m_Objects.end() )
    {
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = volIter->second.m_Attrs.find( m_volumeAttrNamesObj.GetIsSystemVolumeAttrName() ) ;
        if( attrIter->second.compare("1") == 0 )
        {
            attrIter = volIter->second.m_Attrs.find( m_volumeAttrNamesObj.GetNameAttrName() ) ;
            if( attrIter->second.compare( "C:\\SRV" ) != 0 )
            {
                bootvolume = attrIter->second ;
                break ;
            }
        }
        volIter++ ;
    }
    return bootvolume ;
}

bool VolumeConfig::GetVolumeResizeTimeStamp(const std::string& volumeName, 
                                            const time_t& resyncStartTime,  
                                            time_t& resizeTimeStamp)
{
    std::map<std::string, volumeProperties> volumePropertiesMap ;
    GetVolumeProperties( volumePropertiesMap ) ;
    std::map<std::string, volumeProperties>::const_iterator volumePropertiesIter ;
    volumePropertiesIter = volumePropertiesMap.find( volumeName ) ;
    bool bRet = false ;
    DebugPrintf(SV_LOG_DEBUG, "Resync Time Stamp : %ld\n", resyncStartTime) ;
    if( volumePropertiesIter != volumePropertiesMap.end() )
    {
        std::map<time_t, volumeResizeDetails>::const_iterator resizeHistoryIter ;
        resizeHistoryIter = volumePropertiesIter->second.volumeResizeHistory.begin() ;
        while( resizeHistoryIter != volumePropertiesIter->second.volumeResizeHistory.end() )
        {
            if( resizeHistoryIter->second.m_changeTimeStamp > resyncStartTime )
            {
                resizeTimeStamp = resizeHistoryIter->second.m_changeTimeStamp ;
                DebugPrintf(SV_LOG_DEBUG, "Change Time Stamp : %ld\n", resizeTimeStamp ) ;
                bRet = true ;
                break ;
            }
            resizeHistoryIter++ ;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

VolumeConfig::VolumeConfig()
{
    m_isModified = false ;
    m_VolumeGroup = m_RepositoryGroup = NULL ;
}

bool VolumeConfig::isRepositoryConfigured(const std::string& repoName)
{
    bool repoConfigured = false ;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ConfSchema::ObjectsIter_t repoIter ;
    repoIter = find_if(m_RepositoryGroup->m_Objects.begin(),
        m_RepositoryGroup->m_Objects.end(),
        ConfSchema::EqAttr(m_repositoryAttrNamesObj.GetNameAttrName(), 
        repoName.c_str())) ;
    if( repoIter != m_RepositoryGroup->m_Objects.end() )
    {
        repoConfigured = true ;
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return repoConfigured ;
}

bool VolumeConfig::isRepositoryAvailable(const std::string& repoName)
{
    bool repoAvailable = false ;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ConfSchema::ObjectsIter_t repoIter ;
    repoIter = find_if(m_RepositoryGroup->m_Objects.begin(),
        m_RepositoryGroup->m_Objects.end(),
        ConfSchema::EqAttr(m_repositoryAttrNamesObj.GetNameAttrName(), 
        repoName.c_str())) ;
    if( repoIter != m_RepositoryGroup->m_Objects.end() )
    {
        ConfSchema::Object& repoObject = repoIter->second ;
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = repoObject.m_Attrs.find( m_repositoryAttrNamesObj.GetMountPointAttrName()) ;
        if( isRepositoryConfiguredOnCIFS( attrIter->second ) )
        {
			if (m_VolumeGroup->m_Objects.size() > 0 )
			{
            	repoAvailable = true ;
			}
        }
        else
        {
            std::map<std::string, volumeProperties> volumePropertiesMap ;
            if( GetVolumeProperties(volumePropertiesMap) )
            {
                if( volumePropertiesMap.find( attrIter->second ) != volumePropertiesMap.end() )
                {
                    repoAvailable = true ;
                }
            }
        }
    }

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return repoAvailable ;

}

bool VolumeConfig::GetVolumeObjectByVolumeName(const std::string& volumeName, ConfSchema::Object** volObj)
{
    ConfSchema::ObjectsIter_t volumeObjIter ;
    bool bRet = false ;
    volumeObjIter = find_if( m_VolumeGroup->m_Objects.begin(),
        m_VolumeGroup->m_Objects.end(),
        ConfSchema::EqAttr(m_volumeAttrNamesObj.GetNameAttrName(),
        volumeName.c_str())) ;
    if( volumeObjIter != m_VolumeGroup->m_Objects.end() )
    {
        *volObj = &(volumeObjIter->second) ;
        bRet = true ;
    }
    return bRet ;
}
void VolumeConfig::GetRepositoryNameAndPath(std::string& repoName, std::string& repoPath)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ConfSchema::ObjectsIter_t repoIter ;
    if( m_RepositoryGroup->m_Objects.size() )
    {
        repoIter = m_RepositoryGroup->m_Objects.begin() ;
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = repoIter->second.m_Attrs.find( m_repositoryAttrNamesObj.GetNameAttrName()) ;
        repoName = attrIter->second ;
        attrIter = repoIter->second.m_Attrs.find( m_repositoryAttrNamesObj.GetMountPointAttrName()) ;
        repoPath = attrIter->second ;
    }
}


bool VolumeConfig::GetRepositoryVolume(const std::string& repoName, std::string& repoPath)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    ConfSchema::ObjectsIter_t repoIter ;
    repoIter = find_if(m_RepositoryGroup->m_Objects.begin(),
        m_RepositoryGroup->m_Objects.end(),
        ConfSchema::EqAttr(m_repositoryAttrNamesObj.GetNameAttrName(), 
        repoName.c_str())) ;
    if( repoIter != m_RepositoryGroup->m_Objects.end() )
    {
        ConfSchema::Object& repoObject = repoIter->second ;
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = repoObject.m_Attrs.find( m_repositoryAttrNamesObj.GetMountPointAttrName()) ;
        repoPath = attrIter->second ;
        bRet = true ;
    }
    return bRet ;
}

bool VolumeConfig::GetRepositoryFreeSpace(const std::string& repoName, SV_ULONGLONG& freeSpace)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED  %s \n ",FUNCTION_NAME);
    bool bRet = false ;
    std::string repoPath ;
    freeSpace = 0 ;
    if( GetRepositoryVolume(repoName, repoPath) )
    {
        std::map<std::string, volumeProperties> volumePropertiesMap ;
        if( GetVolumeProperties( volumePropertiesMap ) )
        {
            std::map<std::string, volumeProperties>::const_iterator volumePropertiesIter ;
            volumePropertiesIter = volumePropertiesMap.find( repoPath ) ;
            if( volumePropertiesIter != volumePropertiesMap.end() )
            {
                freeSpace = boost::lexical_cast<SV_LONGLONG>(volumePropertiesIter->second.freeSpace) ;
                bRet = true ;
            }
        }
    }
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s \n ",FUNCTION_NAME);
    return bRet ;
}

bool VolumeConfig::GetRepositoryCapacity(const std::string& repoName, SV_ULONGLONG& capacity)
{
    DebugPrintf(SV_LOG_DEBUG,"ENTERED  %s \n ",FUNCTION_NAME);
    bool bRet = false ;
    std::string repoPath ;
    if( GetRepositoryVolume(repoName, repoPath) )
    {
        std::map<std::string, volumeProperties> volumePropertiesMap ;
        if( GetVolumeProperties( volumePropertiesMap ) )
        {
            std::map<std::string, volumeProperties>::const_iterator volumePropertiesIter ;
            volumePropertiesIter = volumePropertiesMap.find( repoPath ) ;
            if( volumePropertiesIter != volumePropertiesMap.end() )
            {
                capacity = boost::lexical_cast<SV_LONGLONG>(volumePropertiesIter->second.capacity) ;
                bRet = true ;
            }
        }
    }

    DebugPrintf(SV_LOG_DEBUG,"EXITED %s \n ",FUNCTION_NAME);
    return bRet ;
}

bool VolumeConfig::UpdateVolumeFreeSpace(const std::string& volName, SV_LONGLONG freeSpace) 
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    ConfSchema::Object* volObj ;
    DebugPrintf(SV_LOG_DEBUG, "Updating the free space for %s\n", volName.c_str()) ;
    if( GetVolumeObjectByVolumeName(volName, &volObj) )
    {
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = volObj->m_Attrs.find( m_volumeAttrNamesObj.GetFreeSpaceAttrName() ) ;
        attrIter->second = boost::lexical_cast<std::string>(freeSpace) ;
        bRet = true ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}


bool VolumeConfig::GetCapacity(const std::string& volumeName, SV_LONGLONG& capacity)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    ConfSchema::Object* volObj ;
    if( GetVolumeObjectByVolumeName(volumeName, &volObj) )
    {
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = volObj->m_Attrs.find( m_volumeAttrNamesObj.GetCapacityAttrName() ) ;
        capacity = boost::lexical_cast<SV_LONGLONG>(attrIter->second) ;
        bRet = true ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool VolumeConfig::GetVolumeSizeOnDisk(const std::string& volumeName, SV_LONGLONG& volumeSizeOnDisk)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    ConfSchema::Object* volObj ;
    if( GetVolumeObjectByVolumeName(volumeName, &volObj) )
    {
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = volObj->m_Attrs.find( m_volumeAttrNamesObj.GetSizeOnDiskAttrName() ) ;
		if (attrIter != volObj->m_Attrs.end () )
		{
			volumeSizeOnDisk = boost::lexical_cast<SV_LONGLONG>(attrIter->second) ;
			bRet = true ;
		}
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}
bool VolumeConfig::GetRawSize(const std::string& volumeName, SV_LONGLONG& rawSize)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    ConfSchema::Object* volObj ;
    if( GetVolumeObjectByVolumeName(volumeName, &volObj) )
    {
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = volObj->m_Attrs.find( m_volumeAttrNamesObj.GetRawSizeAttrName() ) ;
        rawSize = boost::lexical_cast<SV_LONGLONG>(attrIter->second) ;
        bRet = true ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool VolumeConfig::AddRepository(const std::string& repoName, const std::string& repoDev)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    ConfSchema::Object repoObject ;
    repoObject.m_Attrs.insert(std::make_pair(m_repositoryAttrNamesObj.GetNameAttrName(),
        repoName));
    repoObject.m_Attrs.insert(std::make_pair(m_repositoryAttrNamesObj.GetMountPointAttrName(),
        repoDev));
    repoObject.m_Attrs.insert(std::make_pair(m_repositoryAttrNamesObj.GetTypeAttrName(),""));
    repoObject.m_Attrs.insert(std::make_pair(m_repositoryAttrNamesObj.GetDeviceNameAttrName(),""));
    repoObject.m_Attrs.insert(std::make_pair(m_repositoryAttrNamesObj.GetLabelAttrName(),""));
    repoObject.m_Attrs.insert(std::make_pair(m_repositoryAttrNamesObj.GetStatusAttrName(),""));
    repoObject.m_Attrs.insert(std::make_pair(m_repositoryAttrNamesObj.GetDomainAttrName(),""));
    repoObject.m_Attrs.insert(std::make_pair(m_repositoryAttrNamesObj.GetUserNameAttrName(),""));
    repoObject.m_Attrs.insert(std::make_pair(m_repositoryAttrNamesObj.GetPasswordAttrName(),""));

    m_RepositoryGroup->m_Objects.insert(std::make_pair(repoName, repoObject)) ;
    AuditConfigPtr auditConfigPtr = AuditConfig::GetInstance() ;
    auditConfigPtr->LogAudit( "Creating repository " + repoName + " with device " + repoDev ) ;
    bRet = true ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

bool VolumeConfig::IsFirsRegistration()
{
    bool firsttime = false ;
    if( m_VolumeGroup->m_Objects.size() == 0 )
    {
        firsttime = true ;
    }
    return firsttime ;
}

void VolumeConfig::SanitizeVolName(std::string& pathname)
{
    if( !pathname.empty() )
    {
        if( pathname.length() <= 3 )
        {
            pathname.erase(1) ;
        }
        else if( pathname[pathname.length() - 1 ] == '\\' )
        {
            pathname.erase( pathname.length() - 1 ) ;
        }
    }
}

void VolumeConfig::UpdateDynamicInfo(const ParameterGroup& individualVolPg , std::map <std::string,SV_INT> &volumeLockMap)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ConstParametersIter_t paramIter ;
	int locked = 0 ;
    ProtectionPairConfigPtr protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;

    paramIter = individualVolPg.m_Params.find(NS_VolumeDynamicInfo::name);
    std::string volumeName = paramIter->second.value ;
    SanitizeVolName(volumeName) ;
    ConfSchema::ObjectsIter_t objiter ;
    objiter = find_if(m_VolumeGroup->m_Objects.begin(), 
        m_VolumeGroup->m_Objects.end(), 
        ConfSchema::EqAttr(m_volumeAttrNamesObj.GetNameAttrName(), 
        volumeName.c_str()));
    if( objiter != m_VolumeGroup->m_Objects.end() )
    {
        paramIter = individualVolPg.m_Params.find(NS_VolumeDynamicInfo::freeSpace) ;
        DebugPrintf(SV_LOG_DEBUG, "%s --> %s\n", NS_VolumeDynamicInfo::freeSpace, 
            paramIter->second.value.c_str()) ;
        SV_ULONGLONG freeSpace = boost::lexical_cast<SV_ULONGLONG>(paramIter->second.value) ;
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = objiter->second.m_Attrs.find( m_volumeAttrNamesObj.GetFreeSpaceAttrName()) ;
        if( attrIter == objiter->second.m_Attrs.end() )
        {
            objiter->second.m_Attrs.insert(std::make_pair( m_volumeAttrNamesObj.GetFreeSpaceAttrName(), 
                boost::lexical_cast<std::string>(freeSpace))) ;
        }
        else
        {
            attrIter->second = boost::lexical_cast<std::string>(freeSpace) ;
        }	
        if( protectionPairConfigPtr->IsProtectedAsSrcVolume( volumeName ) )
        {
            std::string msg ;
            attrIter = objiter->second.m_Attrs.find( m_volumeAttrNamesObj.GetVolumeTypeAttrName() ) ;
            int volType = boost::lexical_cast<int>(attrIter->second) ;
            attrIter = objiter->second.m_Attrs.find( m_volumeAttrNamesObj.GetIsDeletedVolumeAttrName()) ;
            int isDeleted = boost::lexical_cast<int>(attrIter->second) ;
			std::map <std::string,SV_INT>::iterator volumeLockMapIter = volumeLockMap.begin() ;
			volumeLockMapIter = volumeLockMap.find (volumeName); 
			if (volumeLockMapIter != volumeLockMap.end () )
			{
				locked  = volumeLockMapIter->second ; 
			}
            if( volType == 5 && isDeleted == 0 &&  locked == 0)
            {
                IsEnougFreeSpaceInVolume( volumeName, 10, msg ) ;
            }
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void VolumeConfig::UpdateVolumeHistory( const std::string& volumeName, 
                                       SV_ULONGLONG oldRawCapacity, 
                                       SV_ULONGLONG rawCapacity, 
                                       SV_ULONGLONG oldCapacity, 
                                       SV_ULONGLONG capacity)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ConfSchema::ObjectsIter_t volObjIter ;
    volObjIter = find_if( m_VolumeGroup->m_Objects.begin(),
        m_VolumeGroup->m_Objects.end(),
        ConfSchema::EqAttr(m_volumeAttrNamesObj.GetNameAttrName(),
        volumeName.c_str())) ;
    ConfSchema::GroupsIter_t grpIter = volObjIter->second.m_Groups.find( m_volResizeHistoryAttrNamesObj.GetName() ) ;
    if( grpIter == volObjIter->second.m_Groups.end() )
    {
        volObjIter->second.m_Groups.insert( std::make_pair( m_volResizeHistoryAttrNamesObj.GetName(), ConfSchema::Group())) ;
        grpIter = volObjIter->second.m_Groups.find( m_volResizeHistoryAttrNamesObj.GetName() ) ;
    }
    ConfSchema::Object ResizeDetailsObj ;
    ResizeDetailsObj.m_Attrs.insert(std::make_pair( m_volResizeHistoryAttrNamesObj.GetCurrentCapacityAttrName(), 
        boost::lexical_cast<std::string>(capacity))) ;
    ResizeDetailsObj.m_Attrs.insert(std::make_pair( m_volResizeHistoryAttrNamesObj.GetCurrentRawSizeAttrName(), 
        boost::lexical_cast<std::string>(rawCapacity))) ;
    ResizeDetailsObj.m_Attrs.insert(std::make_pair( m_volResizeHistoryAttrNamesObj.GetOldCapacityAttrName(),
        boost::lexical_cast<std::string>(oldCapacity))) ;
    ResizeDetailsObj.m_Attrs.insert(std::make_pair( m_volResizeHistoryAttrNamesObj.GetOldRawSizeAttrName(),
        boost::lexical_cast<std::string>(oldRawCapacity))) ;
    ResizeDetailsObj.m_Attrs.insert(std::make_pair( m_volResizeHistoryAttrNamesObj.GetIsSparseFilesCreatedAttrName(), "0")) ;
    time_t resizeTs = time(NULL) ;
    ResizeDetailsObj.m_Attrs.insert(std::make_pair( m_volResizeHistoryAttrNamesObj.GetTimeStampAttrName(),
        boost::lexical_cast<std::string>(resizeTs))) ;

    grpIter->second.m_Objects.insert(std::make_pair( boost::lexical_cast<std::string>(resizeTs),
        ResizeDetailsObj)) ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

bool VolumeConfig::CanUpdate(const std::string& volumeName, int locked, int volumeType, SV_ULONGLONG volumeCapacity , SV_ULONGLONG rawCapacity)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;

	/* TODO: make this properly intended;
	 * breaking into functions probably 
	 */
	if (VolumeSummary::DISK == volumeType)
	{
		DebugPrintf(SV_LOG_DEBUG, "%s is disk.Hence currently not updating in store as this is not filterable\n", volumeName.c_str());
		return bRet;
	}

	ConfSchema::AttrsIter_t attrIter ;
    ConfSchema::ObjectsIter_t objIter ;
    objIter = find_if( m_VolumeGroup->m_Objects.begin(),
            m_VolumeGroup->m_Objects.end(),
            ConfSchema::EqAttr(m_volumeAttrNamesObj.GetNameAttrName(),
            volumeName.c_str())) ;
	if (rawCapacity != 0 )
	{
		if( locked != 0 )
		{
			DebugPrintf(SV_LOG_WARNING, "%s is locked..\n", volumeName.c_str()) ;
			if( objIter != m_VolumeGroup->m_Objects.end() )
			{
				attrIter = objIter->second.m_Attrs.find( m_volumeAttrNamesObj.GetIsLockedVolumeAttarName() ) ;
				if( attrIter == objIter->second.m_Attrs.end() )
				{
					objIter->second.m_Attrs.insert( std::make_pair( m_volumeAttrNamesObj.GetIsLockedVolumeAttarName(), "1")) ;
				}
				else
				{
					attrIter->second = "1" ;
				}
			}
		}
		else
		{
			if(volumeType != 5 )
			{
				bRet = true ;
				if( objIter != m_VolumeGroup->m_Objects.end() )
				{
					ProtectionPairConfigPtr protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
					if( protectionPairConfigPtr->IsProtectedAsSrcVolume(volumeName) )
					{
						DebugPrintf(SV_LOG_DEBUG, "Volume type for the protected volume is seen as %d\n", volumeType) ;
						m_removedProtectedVolumes.push_back( volumeName ) ;
						attrIter = objIter->second.m_Attrs.find( m_volumeAttrNamesObj.GetIsDeletedVolumeAttrName() ) ;
						if( attrIter == objIter->second.m_Attrs.end() )
						{
							objIter->second.m_Attrs.insert( std::make_pair(m_volumeAttrNamesObj.GetIsDeletedVolumeAttrName(), "1")) ;
						}
						else
						{
							attrIter->second = "1" ;
						}
						bRet = false ;
					}
				}
				ScenarioConfigPtr scenarioConfigPtr = ScenarioConfig::GetInstance() ;
				scenarioConfigPtr->RemoveVolumeFromExclusionList(volumeName) ;
			}
			else
			{
				DebugPrintf(SV_LOG_WARNING, "%s's capacity is 0 even though its volume type is 5\n", volumeName.c_str()) ;
				ProtectionPairConfigPtr protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
				if( volumeCapacity == 0 )
				{
					if( !protectionPairConfigPtr->IsProtectedAsSrcVolume( volumeName ) ) 
					{
						bRet = true;
					}
					else
					{
						bRet = false ;
					}
				}
				else
				{	
					bRet = true ;
					objIter = m_VolumeGroup->m_Objects.begin() ;
					while( objIter != m_VolumeGroup->m_Objects.end() )
					{
						ConfSchema::AttrsIter_t attrIter ;
						ConfSchema::Attrs_t& volumeObjAttrs = objIter->second.m_Attrs ;
						attrIter = volumeObjAttrs.find(m_volumeAttrNamesObj.GetNameAttrName()) ;
						if( attrIter->second.length() < volumeName.length() )
						{

							if( attrIter->second.find( volumeName ) == 0 )
							{
								//If the volume which is protected and the volume type is seen as non 5 in register host
								//then it is deleted.
								attrIter = volumeObjAttrs.find(m_volumeAttrNamesObj.GetVolumeTypeAttrName()) ;
								int parentVolumeType = boost::lexical_cast<int>(attrIter->second) ;
								if( parentVolumeType != 5 )
								{
									DebugPrintf(SV_LOG_DEBUG, "The volume type of parent of %s is not 5.. Not updating..\n",  volumeName.c_str()) ;
									bRet = false ;
									break ;
								}
							}
						}
						objIter++ ;
					}
				 }
			}
		}
	}
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}
void VolumeConfig::RemoveMissingVolumes(const std::list<std::string>& registeredVolumes)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ProtectionPairConfigPtr protectionPairConfigPtr ;
    protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
    std::list<std::string> protectedVolumes, availableVolumes ;
    protectionPairConfigPtr->GetProtectedVolumes(protectedVolumes) ;
    std::list<std::string>::const_iterator protectedVolIter = protectedVolumes.begin() ;
    //If the protected volume is not found in the registered volume, it is also a missing protected volume.

    while( protectedVolIter != protectedVolumes.end() )
    {
        if( std::find( registeredVolumes.begin(), registeredVolumes.end(), *protectedVolIter)
            == registeredVolumes.end() )
        {
            m_removedProtectedVolumes.push_back( *protectedVolIter ) ;
            ConfSchema::ObjectsIter_t removedVolObj ;
            removedVolObj = find_if( m_VolumeGroup->m_Objects.begin(),
                                              m_VolumeGroup->m_Objects.end(),
                                              ConfSchema::EqAttr(m_volumeAttrNamesObj.GetNameAttrName(),
                                              protectedVolIter->c_str())) ;
            ConfSchema::AttrsIter_t attrIter ;
            attrIter = removedVolObj->second.m_Attrs.find( m_volumeAttrNamesObj.GetIsDeletedVolumeAttrName()) ;
            attrIter->second = "1" ;            
        }
        protectedVolIter++ ;
    }

    GetVolumes(availableVolumes) ;
    std::list<std::string>::const_iterator availableVolIter ;
    availableVolIter = availableVolumes.begin() ;
    while( availableVolIter != availableVolumes.end() )
    {
        bool protectedVolume = false ;
        bool registeredVolume = false ;
        if( std::find( registeredVolumes.begin(), registeredVolumes.end(), *availableVolIter )
            != registeredVolumes.end() )
        {
            registeredVolume = true ;
        }
        if( !registeredVolume )
        {
            //if not a registered volume and the volume is not protected, it can be removed.
            if( std::find( m_removedProtectedVolumes.begin(), m_removedProtectedVolumes.end(),
                *availableVolIter) == m_removedProtectedVolumes.end() )
            {
                ConfSchema::ObjectsIter_t objIter ;
                objIter = find_if( m_VolumeGroup->m_Objects.begin(), 
                    m_VolumeGroup->m_Objects.end(),
                    ConfSchema::EqAttr( m_volumeAttrNamesObj.GetNameAttrName(),
                    availableVolIter->c_str())) ;
                DebugPrintf(SV_LOG_ERROR, "Not an error.. Removing the volume details of %s as it is not in registered volumes\n",
                    availableVolIter->c_str()) ;

                m_VolumeGroup->m_Objects.erase(objIter) ;
                ScenarioConfigPtr scenarioConfigPtr = ScenarioConfig::GetInstance() ;
                scenarioConfigPtr->RemoveVolumeFromExclusionList(*availableVolIter) ;
            }
            else
            {
                DebugPrintf(SV_LOG_ERROR, "Not an error.. Not removing the volume details as the volume is protected.\n") ;
            }
        }
        availableVolIter++ ;
    }
    std::list<std::string>::const_iterator removedVolIter = m_removedProtectedVolumes.begin() ;
    AlertConfigPtr alertConfigPtr = AlertConfig::GetInstance() ;
    while( removedVolIter != m_removedProtectedVolumes.end() )
    {
        std::string alertMsg = "Protected Drive " + *removedVolIter + " is missing on the system. Use Recovery/Rescue Wizard to recover it." ;
        alertConfigPtr->AddAlert("CRITICAL", "CX", SV_ALERT_CRITICAL, SV_ALERT_MODULE_RESYNC, alertMsg) ;  // Modified SV_ALERT_SIMPLE to SV_ALERT_CRITICAL
		protectionPairConfigPtr->DisableAutoResync(*removedVolIter) ;
        removedVolIter++ ;
    }
    m_removedProtectedVolumes.clear() ;
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void VolumeConfig::UpdateStaticInfo(const ParameterGroup& individualVolPg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool newVolume = false ;
    ConstParametersIter_t paramIter ;

    paramIter = individualVolPg.m_Params.find(NS_VolumeSummary::name);
    std::string volumeName = paramIter->second.value ;
    SanitizeVolName(volumeName) ;
    paramIter = individualVolPg.m_Params.find( NS_VolumeSummary::locked) ;

    int locked = boost::lexical_cast<int>(paramIter->second.value) ;
    DebugPrintf(SV_LOG_DEBUG, "%s --> %s\n", NS_VolumeSummary::locked, 
        paramIter->second.value.c_str()) ;

    paramIter = individualVolPg.m_Params.find( NS_VolumeSummary::type) ;
    DebugPrintf(SV_LOG_DEBUG, "%s --> %s\n", NS_VolumeSummary::type, 
        paramIter->second.value.c_str()) ;
    int volumeType = boost::lexical_cast<int>(paramIter->second.value) ;

    paramIter = individualVolPg.m_Params.find( NS_VolumeSummary::capacity ) ;
    SV_ULONGLONG capacity = boost::lexical_cast<SV_ULONGLONG>(paramIter->second.value) ;
    DebugPrintf(SV_LOG_DEBUG, "%s --> %s\n", NS_VolumeSummary::capacity, 
        paramIter->second.value.c_str()) ;

	paramIter = individualVolPg.m_Params.find( NS_VolumeSummary::rawcapacity ) ;
    SV_ULONGLONG rawCapacity = boost::lexical_cast<SV_ULONGLONG>(paramIter->second.value) ;
    DebugPrintf(SV_LOG_DEBUG, "%s --> %s\n", NS_VolumeSummary::rawcapacity, 
        paramIter->second.value.c_str()) ;

    if( CanUpdate(volumeName, locked, volumeType, capacity , rawCapacity) )
    {
        /*
        paramIter = individualVolPg.m_Params.find( NS_VolumeSummary::containPagefile) ;
        DebugPrintf(SV_LOG_DEBUG, "%s --> %s\n", NS_VolumeSummary::containPagefile, 
        paramIter->second.value.c_str()) ;
        */
        paramIter = individualVolPg.m_Params.find( NS_VolumeSummary::fileSystem) ;
        DebugPrintf(SV_LOG_DEBUG, "%s --> %s\n", NS_VolumeSummary::fileSystem, 
            paramIter->second.value.c_str()) ;
        std::string fileSystem = paramIter->second.value ;
        /*
        paramIter = individualVolPg.m_Params.find( NS_VolumeSummary::isCacheVolume) ;
        DebugPrintf(SV_LOG_DEBUG, "%s --> %s\n", NS_VolumeSummary::isCacheVolume, 
        paramIter->second.value.c_str()) ;
        */
        paramIter = individualVolPg.m_Params.find( NS_VolumeSummary::isMounted) ;
        DebugPrintf(SV_LOG_DEBUG, "%s --> %s\n", NS_VolumeSummary::isMounted, 
            paramIter->second.value.c_str()) ;
        int isMounted = boost::lexical_cast<int>(paramIter->second.value) ;
        paramIter = individualVolPg.m_Params.find( NS_VolumeSummary::isSystemVolume ) ;
        DebugPrintf(SV_LOG_DEBUG, "%s --> %s\n", NS_VolumeSummary::isSystemVolume, 
            paramIter->second.value.c_str()) ;
        int isSystemVolume = boost::lexical_cast<int>(paramIter->second.value) ;

        paramIter = individualVolPg.m_Params.find( NS_VolumeSummary::mountPoint) ;
        DebugPrintf(SV_LOG_DEBUG, "%s --> %s\n", NS_VolumeSummary::mountPoint, 
            paramIter->second.value.c_str()) ;
        std::string mountPnt = paramIter->second.value ;
        SanitizeVolName(mountPnt) ;
        paramIter = individualVolPg.m_Params.find( NS_VolumeSummary::rawcapacity) ;
        DebugPrintf(SV_LOG_DEBUG, "%s --> %s\n", NS_VolumeSummary::rawcapacity, 
            paramIter->second.value.c_str()) ;
        SV_ULONGLONG rawCapacity = boost::lexical_cast<SV_ULONGLONG>(paramIter->second.value) ;
        paramIter = individualVolPg.m_Params.find( NS_VolumeSummary::writeCacheState) ;
        DebugPrintf(SV_LOG_DEBUG, "%s --> %s\n", NS_VolumeSummary::writeCacheState, 
            paramIter->second.value.c_str()) ;
        int writeCacheStatus = boost::lexical_cast<int>(paramIter->second.value) ;
        paramIter = individualVolPg.m_Params.find( NS_VolumeSummary::volumeLabel) ;
        DebugPrintf(SV_LOG_DEBUG, "%s --> %s\n", NS_VolumeSummary::volumeLabel, 
            paramIter->second.value.c_str()) ;
        std::string volumelabel = paramIter->second.value ;
		paramIter = individualVolPg.m_Params.find( NS_VolumeSummary::volumeGroup) ;
		std::string volumegroup = paramIter->second.value ;
        ConfSchema::ObjectsIter_t objiter ;
        objiter = find_if(m_VolumeGroup->m_Objects.begin(), 
            m_VolumeGroup->m_Objects.end(), 
            ConfSchema::EqAttr(m_volumeAttrNamesObj.GetNameAttrName(), 
            volumeName.c_str()));

        if( objiter == m_VolumeGroup->m_Objects.end() )
        {
            ConfSchema::Object volumeObj ;
            volumeObj.m_Attrs.insert(std::make_pair( m_volumeAttrNamesObj.GetNameAttrName(), volumeName)) ;
            volumeObj.m_Attrs.insert(std::make_pair( m_volumeAttrNamesObj.GetFileSystemAttrName(),
                fileSystem)) ;

			SV_LONGLONG freeSpace = 0 ;
#ifdef SV_WINDOWS		
			ULARGE_INTEGER uliQuota = {0};
			if( SVGetDiskFreeSpaceEx(mountPnt.c_str(),&uliQuota,NULL,NULL) )
			{
				freeSpace = (unsigned long long)uliQuota.QuadPart ;
			}
#endif
			volumeObj.m_Attrs.insert(std::make_pair( m_volumeAttrNamesObj.GetFreeSpaceAttrName(),
				boost::lexical_cast<std::string>(freeSpace))) ;
            volumeObj.m_Attrs.insert(std::make_pair( m_volumeAttrNamesObj.GetCapacityAttrName(),
                boost::lexical_cast<std::string>(capacity))) ;
            volumeObj.m_Attrs.insert(std::make_pair( m_volumeAttrNamesObj.GetIsSystemVolumeAttrName(),
                boost::lexical_cast<std::string>(isSystemVolume))) ;
            volumeObj.m_Attrs.insert(std::make_pair( m_volumeAttrNamesObj.GetVolumeLabelAttrName(),
                volumelabel)) ;
            volumeObj.m_Attrs.insert(std::make_pair( m_volumeAttrNamesObj.GetMountPointAttrName(), mountPnt)) ;
            volumeObj.m_Attrs.insert(std::make_pair( m_volumeAttrNamesObj.GetRawSizeAttrName(),
                boost::lexical_cast<std::string>(rawCapacity))) ;
            volumeObj.m_Attrs.insert(std::make_pair( m_volumeAttrNamesObj.GetWriteCacheStateAttrName(), 
                boost::lexical_cast<std::string>(writeCacheStatus))) ;

            volumeObj.m_Attrs.insert(std::make_pair( m_volumeAttrNamesObj.GetVolumeTypeAttrName(), 
                boost::lexical_cast<std::string>(volumeType))) ;
            volumeObj.m_Attrs.insert(std::make_pair( m_volumeAttrNamesObj.GetAttachModeAttrName(), "")) ;
            volumeObj.m_Attrs.insert(std::make_pair( m_volumeAttrNamesObj.GetFreeSpaceAttrName(), "0")) ;
            if( !m_firstRegistration && volumeType == 5 && ( fileSystem == "NTFS" ||  fileSystem == "ExtendedFAT" ||  fileSystem == "ReFS" ) )
            {
                newVolume = true ;
            }
			volumeObj.m_Attrs.insert(std::make_pair( m_volumeAttrNamesObj.GetIsDeletedVolumeAttrName(), "0")) ;
            volumeObj.m_Attrs.insert(std::make_pair( m_volumeAttrNamesObj.GetIsLockedVolumeAttarName(), "0")) ;
			volumeObj.m_Attrs.insert(std::make_pair( m_volumeAttrNamesObj.GetVolumeGrpAttrName(), volumegroup)) ;
			
            m_VolumeGroup->m_Objects.insert(std::make_pair(volumeName, volumeObj)) ;            
        }
        else
        {
            ConfSchema::AttrsIter_t attrIter ;
            attrIter = objiter->second.m_Attrs.find(m_volumeAttrNamesObj.GetVolumeTypeAttrName()) ; 
            int oldVolumeType = boost::lexical_cast<int>(attrIter->second) ;
            attrIter->second = boost::lexical_cast<std::string>(volumeType);
            SV_ULONGLONG oldCapacity, oldRawCapacity;
            attrIter = objiter->second.m_Attrs.find(m_volumeAttrNamesObj.GetNameAttrName());
            attrIter->second = volumeName ;
            attrIter = objiter->second.m_Attrs.find(m_volumeAttrNamesObj.GetFileSystemAttrName()) ;
            attrIter->second = fileSystem;
            attrIter = objiter->second.m_Attrs.find(m_volumeAttrNamesObj.GetCapacityAttrName()) ;
            oldCapacity = boost::lexical_cast<SV_ULONGLONG>(attrIter->second) ;
            attrIter->second = boost::lexical_cast<std::string>(capacity) ;
            attrIter = objiter->second.m_Attrs.find(m_volumeAttrNamesObj.GetIsSystemVolumeAttrName()) ;
            attrIter->second = boost::lexical_cast<std::string>(isSystemVolume);
            attrIter = objiter->second.m_Attrs.find(m_volumeAttrNamesObj.GetVolumeLabelAttrName()) ;
            attrIter->second = volumelabel ;
            attrIter = objiter->second.m_Attrs.find(m_volumeAttrNamesObj.GetMountPointAttrName()) ;
            attrIter->second = mountPnt;
            attrIter = objiter->second.m_Attrs.find(m_volumeAttrNamesObj.GetRawSizeAttrName()) ;
            oldRawCapacity = boost::lexical_cast<SV_ULONGLONG>( attrIter->second ) ;

            attrIter->second = boost::lexical_cast<std::string>(rawCapacity);
            attrIter = objiter->second.m_Attrs.find(m_volumeAttrNamesObj.GetWriteCacheStateAttrName());  
            attrIter->second = boost::lexical_cast<std::string>(writeCacheStatus);
            attrIter = objiter->second.m_Attrs.find(m_volumeAttrNamesObj.GetIsDeletedVolumeAttrName()) ;
            if( attrIter == objiter->second.m_Attrs.end() )
            {
                objiter->second.m_Attrs.insert(std::make_pair( m_volumeAttrNamesObj.GetIsDeletedVolumeAttrName(), "0")) ;
            }
            else
            {
                attrIter->second = "0" ;
            }
            attrIter = objiter->second.m_Attrs.find(m_volumeAttrNamesObj.GetIsLockedVolumeAttarName()) ;
            if( attrIter == objiter->second.m_Attrs.end() )
            {
                objiter->second.m_Attrs.insert(std::make_pair( m_volumeAttrNamesObj.GetIsLockedVolumeAttarName(), "0")) ;
            }
            else
            {
                attrIter->second = "0" ;
            }
			attrIter = objiter->second.m_Attrs.find(m_volumeAttrNamesObj.GetVolumeGrpAttrName()) ;
			if( attrIter == objiter->second.m_Attrs.end() )
			{
				objiter->second.m_Attrs.insert(std::make_pair( m_volumeAttrNamesObj.GetVolumeGrpAttrName(), volumegroup)) ;
			}
			else
			{
				attrIter->second = volumegroup ;
			}
			
            attrIter = objiter->second.m_Attrs.find(m_volumeAttrNamesObj.GetAttachModeAttrName()) ;
            attrIter->second = "";
            if( oldVolumeType == 2 && volumeType == 5 && ( fileSystem == "NTFS" || fileSystem == "ExtendedFAT" || fileSystem == "ReFS" ) )
            {
                newVolume = true ;
            }
            if( volumeType == 5 && oldVolumeType == 5 && 
                (oldRawCapacity != rawCapacity) || 
                (oldCapacity  != capacity) )
            {
                ProtectionPairConfigPtr protectionPairConfigPtr ;
                protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
                if( protectionPairConfigPtr->IsProtectedAsSrcVolume(volumeName) )
                {
                    UpdateVolumeHistory( volumeName, oldRawCapacity, rawCapacity, oldCapacity, capacity) ;
                }
            }
        }
        if( newVolume )
        {
            AlertConfigPtr alertConfigPtr = AlertConfig::GetInstance() ;
            std::string alertMsg = "New Volumes Discovered  For " ;
            alertMsg += Host::GetInstance().GetHostName();
            alertMsg += " => ";
            alertMsg += volumeName ;
            alertConfigPtr->AddAlert("NOTICE", "CX", SV_ALERT_SIMPLE, SV_ALERT_MODULE_RESYNC, alertMsg) ;
            AuditConfigPtr auditConfigPtr = AuditConfig::GetInstance() ;
            auditConfigPtr->LogAudit(alertMsg) ;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

void VolumeConfig::GetVolumesWithTypeIDOne(const std::string& volPgName,const ParameterGroup& volumePg,std::list<std::string> &newVolumesWithTypeOne)
{	
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s \n",FUNCTION_NAME);

    for (ParameterGroups_t::size_type i = 0; i < volumePg.m_ParamGroups.size() ;i++)
    {
        std::stringstream ss;
        ss << i;
        std::string eachvolumepgname = volPgName;
        /* TODO: should '[' and ']' be from header ? for now generic enough */
        eachvolumepgname += '[';
        eachvolumepgname += ss.str();
        eachvolumepgname += ']';
        ConstParameterGroupsIter_t iter = volumePg.m_ParamGroups.find(eachvolumepgname);
        if( iter != volumePg.m_ParamGroups.end() )
        {
            const ParameterGroup& individualVolPg =  iter->second ;
            ConstParametersIter_t paramIter = individualVolPg.m_Params.find(NS_VolumeSummary::type);
            if (paramIter != individualVolPg.m_Params.end() ) 
            {
                std::string volumeType = paramIter->second.value ;
                DebugPrintf (SV_LOG_DEBUG,"Volume Type is %s ",volumeType.c_str() );
                if (volumeType == "1")
                {
                    ConstParametersIter_t paramIter = individualVolPg.m_Params.find(NS_VolumeSummary::name);
                    if (paramIter != individualVolPg.m_Params.end() )
                    {
                        std::string volumeName = paramIter->second.value ;
                        DebugPrintf (SV_LOG_DEBUG,"VolumeName in FOR Loop is %s ",volumeName.c_str() );
                        newVolumesWithTypeOne.push_back(volumeName);
                    }
                }
            }
        }
    }
    DebugPrintf (SV_LOG_DEBUG,"EXITED %s \n", FUNCTION_NAME);
    return;
}
void VolumeConfig::CreateRepoObjectIfRequired()
{
    if( !m_RepositoryGroup->m_Objects.size() )
    {
        FileConfigurator config ;
		std::string repositoryName = config.getRepositoryName();
        AddRepository( repositoryName, config.getRepositoryLocation()) ;
    }
}

void VolumeConfig::DeleteRepositoryObject()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
	m_RepositoryGroup->m_Objects.clear();
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
	return ;
}

void VolumeConfig::UpdateVolumes(const std::string& volPgName,
                                 const ParameterGroup& volumePg, 
                                 eHostRegisterType regType)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    CreateRepoObjectIfRequired() ;
    ProtectionPairConfigPtr protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
	AgentConfigPtr agentConfigPtr = AgentConfig::GetInstance() ;
    const char *pname = (DynamicHostRegister == regType) ? NS_VolumeDynamicInfo::name : NS_VolumeSummary::name;
    std::list<std::string> registeredVolumes ;
    for (ParameterGroups_t::size_type i = 0; i < volumePg.m_ParamGroups.size(); i++)
    {
        int volumeFound = 0;
		static std::map <std::string,SV_INT> volumeLockMap; 
		std::map <std::string,SV_INT>::iterator volumeLockMapIter; 
        std::string volumeType;
        std::string volumeName;
        std::stringstream ss;
        ss << i;
        std::string eachvolumepgname = volPgName;
        /* TODO: should '[' and ']' be from header ? for now generic enough */
        eachvolumepgname += '[';
        eachvolumepgname += ss.str();
        eachvolumepgname += ']';
        ConstParameterGroupsIter_t iter = volumePg.m_ParamGroups.find(eachvolumepgname);
        if( iter != volumePg.m_ParamGroups.end() )
        {
            const ParameterGroup& individualVolPg =  iter->second ;
			ConstParametersIter_t paramIter ;
			paramIter = individualVolPg.m_Params.find(NS_VolumeSummary::name);
			if (paramIter != individualVolPg.m_Params.end() )
			{
				std::string volumeName = paramIter->second.value ;
				DebugPrintf(SV_LOG_DEBUG,"VolumeName is %s \n ",volumeName.c_str());
				SanitizeVolName(volumeName) ;
				paramIter = individualVolPg.m_Params.find(NS_VolumeSummary::locked) ;
				if (paramIter != individualVolPg.m_Params.end() )
				{
					DebugPrintf(SV_LOG_DEBUG, "%s --> %s\n", NS_VolumeSummary::locked, 
						paramIter->second.value.c_str()) ;
					SV_INT volumeLock = boost::lexical_cast<SV_INT>(paramIter->second.value) ;
					volumeLockMapIter = volumeLockMap.find (volumeName); 
					if (volumeLockMapIter == volumeLockMap.end() )
					{
						volumeLockMap.insert( std::pair<std::string,SV_INT>(volumeName,volumeLock) );
					}
					else
					{
						volumeLockMap.erase (volumeLockMapIter);
						volumeLockMap.insert( std::pair<std::string,SV_INT>(volumeName,volumeLock) );
					}
				}
			}
            if( DynamicHostRegister == regType )
            {
                UpdateDynamicInfo(individualVolPg,volumeLockMap) ;
            }
            else
            {
				ConstParametersIter_t paramIter ;
                paramIter = individualVolPg.m_Params.find(NS_VolumeSummary::name);
				volumeName = paramIter->second.value ;
                SanitizeVolName(volumeName) ;
                std::list <std::string> newVolumesWithTypeOne;
                GetVolumesWithTypeIDOne(volPgName,volumePg,newVolumesWithTypeOne);
                std::list <std::string>::iterator newVolumesWithTypeOneIter;
                paramIter = individualVolPg.m_Params.find(NS_VolumeSummary::type);
                if (paramIter != individualVolPg.m_Params.end() )
                {
                    volumeType= paramIter->second.value ;
                }
                if (volumeType == "5")
                {
                    if (paramIter != individualVolPg.m_Params.end() )
                    {
                        DebugPrintf (SV_LOG_DEBUG,"VolumeName in Cdp Explorer is %s ",volumeName.c_str());
                        for (newVolumesWithTypeOneIter = newVolumesWithTypeOne.begin() ; 
                            newVolumesWithTypeOneIter != newVolumesWithTypeOne.end(); 
                            newVolumesWithTypeOneIter++ )
                        {
                            std::string newVolumesWithTypeOneString = *newVolumesWithTypeOneIter;
                            DebugPrintf (SV_LOG_DEBUG,"newVolumesWithTypeOneString  is %s ",newVolumesWithTypeOneString.c_str() );
                            if (volumeName.find(newVolumesWithTypeOneString) != std::string::npos && volumeName.length() > 3 )
                            {
                                volumeFound = 1; 		
                            }
                        }
                        if (volumeFound == 1)
                            continue;
                    }
                }
				if( volumeType == "5" || volumeType == "6" )
                {
                    registeredVolumes.push_back( volumeName ) ;
                }

                UpdateStaticInfo(individualVolPg) ;
            }
        }
    }
    if( StaticHostRegister == regType )
    {
        RemoveMissingVolumes(registeredVolumes) ;
        std::list<std::string> targetVolumes ;
        protectionPairConfigPtr->GetProtectedTargetVolumes(targetVolumes) ;
        std::list<std::string>::const_iterator targetVolIter = targetVolumes.begin() ;
		std::string repopath, reponame ;
		GetRepositoryNameAndPath(reponame, repopath) ;
		if( repopath.length() == 1 )
		{
			repopath += ':' ;
		}
		if( repopath[repopath.length() - 1] != '\\' )
		{
			repopath += '\\' ;
		}
		LocalConfigurator lc ;
        while( targetVolIter != targetVolumes.end() )
        {
            if( std::find( registeredVolumes.begin(), registeredVolumes.end(),*targetVolIter ) == 
                registeredVolumes.end() )
            {
				std::string sparsefile = repopath + lc.getHostId() + targetVolIter->substr(targetVolIter->find_last_of('\\')) ;
				boost::algorithm::replace_all( sparsefile, "_virtualvolume", "_sparsefile") ;
				std::string volume = targetVolIter->c_str() ;
                FormatVolumeNameForCxReporting(volume);
#ifdef SV_WINDOWS
                std::transform(volume.begin(), volume.end(), volume.begin(), ::tolower);
#endif
                FormatVolumeName(volume);
                PersistVirtualVolumes(sparsefile, volume.c_str()) ;
				DebugPrintf(SV_LOG_ERROR, "Persisting the missing volpack entry %s and sparsefile is %s\n",volume.c_str(), sparsefile.c_str() ) ;
            }
            targetVolIter++ ;
        }
        AlertOnDiskCachingIfRequired() ;
    }
    else
    {
        std::list<std::string> protectedVolumes; 
        std::string alertmsg ;
        protectionPairConfigPtr->GetProtectedVolumes( protectedVolumes ) ;
        IsEnoughFreeSpaceInSystemDrive( protectedVolumes.size(), 
            1024 * 1024 * 1024 ,
            alertmsg) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME); 
}

void VolumeConfig::AlertOnDiskCachingIfRequired()
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    if( m_RepositoryGroup->m_Objects.size() )
    {
        ConfSchema::AttrsIter_t attrIter ;
        ConfSchema::Object& repoObj = m_RepositoryGroup->m_Objects.begin()->second ;
        attrIter = repoObj.m_Attrs.find( m_repositoryAttrNamesObj.GetMountPointAttrName() ) ;
        std::string& repoDevice = attrIter->second ;
        ConfSchema::ObjectsIter_t repoVolumeObjIter ;
        repoVolumeObjIter = find_if( m_VolumeGroup->m_Objects.begin(),
            m_VolumeGroup->m_Objects.end(),
            ConfSchema::EqAttr( m_volumeAttrNamesObj.GetNameAttrName(), 
            repoDevice.c_str())) ;
		std::string repoName,repoPath;
		GetRepositoryNameAndPath(repoName, repoPath) ;
		if (isRepositoryConfiguredOnCIFS( repoPath ) == true)
		{
			AlertConfigPtr alertConfigPtr = AlertConfig::GetInstance() ;
			std::string alertMsg ;
			alertMsg = "Disk caching status is unknown for the disk on which " ;
			alertMsg += repoDevice ;
			alertMsg += " exists. Disk caching should be disabled as it may cause backup location corruption in case of a unplanned shutdown of the system" ;
			alertConfigPtr->AddAlert("WARNING", "CX", SV_ALERT_CRITICAL, SV_ALERT_MODULE_RESYNC, alertMsg) ; // Modified SV_ALERT_SIMPLE to SV_ALERT_CRITICAL
		}
		else 
		{
			if( repoVolumeObjIter != m_VolumeGroup->m_Objects.end() )
			{
				attrIter = repoVolumeObjIter->second.m_Attrs.find( m_volumeAttrNamesObj.GetWriteCacheStateAttrName()) ;
				VolumeSummary::writecache repoWriteCacheStatus ;
				repoWriteCacheStatus = (VolumeSummary::writecache)boost::lexical_cast<int>( attrIter->second ) ;
				if( repoWriteCacheStatus == VolumeSummary::WRITE_CACHE_DONTKNOW ||
					repoWriteCacheStatus == VolumeSummary::WRITE_CACHE_ENABLED )
				{
					AlertConfigPtr alertConfigPtr = AlertConfig::GetInstance() ;
					std::string alertMsg ;
					alertMsg = "Disk caching status is unknown/enabled for the disk on which " ;
					alertMsg += repoDevice ;
					alertMsg += " exists. Disk caching should be disabled as it may cause backup location corruption in case of a unplanned shutdown of the system" ;
					alertConfigPtr->AddAlert("WARNING", "CX", SV_ALERT_CRITICAL, SV_ALERT_MODULE_RESYNC, alertMsg) ; // Modified SV_ALERT_SIMPLE to SV_ALERT_CRITICAL
				}
			}
		}
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
bool VolumeConfig::GetLatestResizeTs(const std::string& volumeName,
                                     SV_LONGLONG& latestResizeTs)
{
    latestResizeTs = 0 ;
    bool bRet = false ;
    std::map<std::string, volumeProperties> volumePropertiesMap ; 
    std::map<std::string, volumeProperties>::iterator volumePropertisIter ;
    GetVolumeProperties( volumePropertiesMap ) ;
    if( (volumePropertisIter = volumePropertiesMap.find( volumeName )) != 
        volumePropertiesMap.end() )
    {
        bRet = true ;
        std::map<time_t, volumeResizeDetails>::const_iterator resizeHistoryIter ;
        resizeHistoryIter = volumePropertisIter->second.volumeResizeHistory.begin() ;
        while( resizeHistoryIter != volumePropertisIter->second.volumeResizeHistory.end() )
        {
            if( latestResizeTs < resizeHistoryIter->first )
            {
                latestResizeTs = resizeHistoryIter->first ;
            }
            resizeHistoryIter++ ;
        }
    }
    return bRet ;
}

bool VolumeConfig::GetResizeDetails	(const std::string& volumeName,
                                     const SV_LONGLONG &latestResizeTs, 
									 volumeResizeDetails &resizeDetails)
{
    bool bRet = false ;
    std::map<std::string, volumeProperties> volumePropertiesMap ; 
    std::map<std::string, volumeProperties>::iterator volumePropertisIter ;
    GetVolumeProperties( volumePropertiesMap ) ;
    if( (volumePropertisIter = volumePropertiesMap.find( volumeName )) != 
        volumePropertiesMap.end() )
    {
        std::map<time_t, volumeResizeDetails>::const_iterator resizeHistoryIter ;
        resizeHistoryIter = volumePropertisIter->second.volumeResizeHistory.begin() ;
        while( resizeHistoryIter != volumePropertisIter->second.volumeResizeHistory.end() )
        {
            if( latestResizeTs ==  resizeHistoryIter->first )
            {
				bRet = true ;
				resizeDetails = resizeHistoryIter->second ; 
				DebugPrintf (SV_LOG_DEBUG , "New Capacity is %ld \n ",resizeDetails.m_newVolumeCapacity ); 
				DebugPrintf (SV_LOG_DEBUG , "Old Capacity is %ld \n ",resizeDetails.m_oldVolumeCapacity ); 
				DebugPrintf (SV_LOG_DEBUG , "New Raw Size %ld \n ",resizeDetails.m_newVolumeRawSize); 
				DebugPrintf (SV_LOG_DEBUG , "Old Raw Size %ld \n ",resizeDetails.m_oldVolumeRawSize); 
				break; 
            }
            resizeHistoryIter++ ;
        }
    }
    return bRet ;
}

bool VolumeConfig::SetIsSparseFilesCreated(	const std::string& volumeName,
											const SV_LONGLONG &latestResizeTs)
{
	DebugPrintf (SV_LOG_DEBUG, " ENTERED  %s \n ", FUNCTION_NAME ); 
    bool bRet = false ;
	ConfSchema::Object* volumeObj ;
    if( GetVolumeObjectByVolumeName(volumeName, &volumeObj) )
	{
		ConfSchema::Group* volumeResizeHistoryGrp = NULL ;
		ConfSchema::GroupsIter_t grpiter = volumeObj->m_Groups.find( m_volResizeHistoryAttrNamesObj.GetName() ) ;
		if( grpiter != volumeObj->m_Groups.end() )
		{
			volumeResizeHistoryGrp = &(grpiter->second) ;
			ConfSchema::ObjectsIter_t resizeHistoryObjIter = volumeResizeHistoryGrp->m_Objects.begin() ;
			while( resizeHistoryObjIter != volumeResizeHistoryGrp->m_Objects.end() )
			{
				ConfSchema::Object* resizeHistoryObj = &(resizeHistoryObjIter->second) ;
				ConfSchema::Attrs_t& resizeHistoryAttrs = resizeHistoryObj->m_Attrs ;
				ConfSchema::AttrsIter_t attrIter ;
				attrIter = resizeHistoryAttrs.find( m_volResizeHistoryAttrNamesObj.GetTimeStampAttrName()) ;

				if (latestResizeTs == boost::lexical_cast <SV_LONGLONG> ( attrIter->second) ) 
				{
					attrIter = resizeHistoryAttrs.find( m_volResizeHistoryAttrNamesObj.GetIsSparseFilesCreatedAttrName()) ;
					attrIter->second = "1";
					bRet = true; 
					break;
				}
				resizeHistoryObjIter++ ;
			}
		}
    }
	DebugPrintf (SV_LOG_DEBUG, " EXITED  %s \n ", FUNCTION_NAME ); 

    return bRet ;
}

void VolumeConfig::UpdateTargetDiskUsageInfo( const std::string& volumeName,
                                             SV_ULONGLONG sizeOnDisk, 
                                             SV_ULONGLONG spaceSavedByCompression, 
                                             SV_ULONGLONG spaceSavedByThinProvisioning )
{
    ConfSchema::Object* volObj ;
    DebugPrintf(SV_LOG_DEBUG, "Updating the target disk usage for %s\n", volumeName.c_str()) ;
    if( GetVolumeObjectByVolumeName(volumeName, &volObj) )
    {
        volObj->m_Attrs.insert(std::make_pair( m_volumeAttrNamesObj.GetSizeOnDiskAttrName(), "0")) ;
        volObj->m_Attrs.insert(std::make_pair( m_volumeAttrNamesObj.GetDiskSpaceSavedByCompressionAttrName(), "0")) ;
        volObj->m_Attrs.insert(std::make_pair( m_volumeAttrNamesObj.GetDiskSpaceSavedByThinProvisioningAttrName(), "0")) ;
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = volObj->m_Attrs.find( m_volumeAttrNamesObj.GetSizeOnDiskAttrName() ) ;
        attrIter->second = boost::lexical_cast<std::string>( sizeOnDisk ) ; 
        attrIter = volObj->m_Attrs.find( m_volumeAttrNamesObj.GetDiskSpaceSavedByCompressionAttrName() ) ;
        attrIter->second = boost::lexical_cast<std::string>( spaceSavedByCompression ) ; 
        attrIter = volObj->m_Attrs.find( m_volumeAttrNamesObj.GetDiskSpaceSavedByThinProvisioningAttrName() ) ;
        attrIter->second = boost::lexical_cast<std::string>( sizeOnDisk ) ; 
        attrIter->second = boost::lexical_cast<std::string>(spaceSavedByThinProvisioning) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}


void VolumeConfig::ChangePaths( const std::string& existingRepo, const std::string& newRepo ) 
{
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    ConfSchema::ObjectsIter_t volObjsIter = m_VolumeGroup->m_Objects.begin() ;
    /*while( volObjsIter != m_VolumeGroup->m_Objects.end() )
    {
    ConfSchema::Object& volObj = volObjsIter->second ;
    ConfSchema::AttrsIter_t attrIter ;
    attrIter = volObj.m_Attrs.find( m_volumeAttrNamesObj.GetVolumeTypeAttrName() ) ;
    if( attrIter->second == "6" )
    {
    attrIter = volObj.m_Attrs.find( m_volumeAttrNamesObj.GetNameAttrName()) ;
    std::string currentPath = attrIter->second ;
    std::string newPath = newRepo ;
    newPath += currentPath.substr( existingRepo.length() ) ;
    attrIter->second = newPath ;
    attrIter = volObj.m_Attrs.find( m_volumeAttrNamesObj.GetMountPointAttrName() ) ;
    std::string currentMntPnt = attrIter->second ;
    std::string newMntPnt = newRepo ;
    newMntPnt += currentMntPnt.substr( existingRepo.length() ) ;
    attrIter->second = newMntPnt ;
    }
    volObjsIter++ ;
    }*/
    ConfSchema::ObjectsIter_t repoObjIter = m_RepositoryGroup->m_Objects.begin() ;
    while( repoObjIter != m_RepositoryGroup->m_Objects.end() )
    {
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = repoObjIter->second.m_Attrs.find( m_repositoryAttrNamesObj.GetMountPointAttrName() ) ;
        if( attrIter->second.compare( existingRepo ) == 0 )
        {
            attrIter->second = newRepo ;
        }
        repoObjIter++ ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

bool VolumeConfig::GetfreeSpace(const std::string& volume, SV_LONGLONG& freeSpace )
{
    freeSpace = 0 ;
    ConfSchema::Object* volObj ;
    bool bRet = false ;
    if( GetVolumeObjectByVolumeName(volume, &volObj) )
    {
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = volObj->m_Attrs.find( m_volumeAttrNamesObj.GetFreeSpaceAttrName() ) ;
        freeSpace = boost::lexical_cast<SV_LONGLONG>(attrIter->second) ;
        bRet = true ;
    }
    return bRet  ;
}
/* function will give Space used by file system (Capacity - FreeSpace))
*/ 
bool VolumeConfig::GetFileSystemUsedSpace (const std::string &volName , SV_ULONGLONG & usedSpace )
{	
	SV_LONGLONG volumeCapacity;
	SV_LONGLONG volumefreeSpace;
	bool bRet = false;
	if (GetCapacity (volName, volumeCapacity))
	{
		if (GetfreeSpace (volName,volumefreeSpace))
		{
			bRet = true;
			usedSpace = (SV_ULONGLONG) (volumeCapacity - volumefreeSpace);
		}
	}
	return bRet;
}

bool VolumeConfig::IsEnougFreeSpaceInVolume( const std::string& volume, int percentFreeSpace,
                                            std::string& alertMsg )
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    SV_LONGLONG capacity, freeSpace ;
    GetCapacity( volume, capacity ) ;
    GetfreeSpace( volume, freeSpace ) ;
    if( freeSpace > ( capacity / percentFreeSpace ) )
    {
        bRet = true ;
    }
    else
    {
        alertMsg= "There is no enough free space in the protected volume " ;
        alertMsg += volume ;
        std::string alertPrefix = alertMsg ;
        alertMsg += ". Bookmarking the data may fail if it is less than " ;
        alertMsg += boost::lexical_cast<std::string>( percentFreeSpace ) ;
        alertMsg += " percent of the volume capacity.In some cases " ;
        alertMsg += boost::lexical_cast<std::string>( percentFreeSpace ) ;
        alertMsg += " percent free space may not be sufficient and it requires even more free space. " ;
        alertMsg += "Volume capacity is : " ;
        alertMsg += BytesAsString( capacity ) ;
        alertMsg += ". Free space is : " ;
        alertMsg += BytesAsString( freeSpace ) ;
        alertMsg += ". Required free space is at-least : " ;
        alertMsg += BytesAsString( capacity / percentFreeSpace ) ;
        AlertConfigPtr alertConfigPtr = AlertConfig::GetInstance() ;
        alertConfigPtr->AddAlert("WARNING", "CX", SV_ALERT_SIMPLE,   // Change Alert Level from NOTICE to WARNING
            SV_ALERT_MODULE_RESYNC, alertMsg, alertPrefix) ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}


bool VolumeConfig::IsEnoughFreeSpaceInSystemDrive( int volumes, 
                                                  SV_LONGLONG requiredFreeSpacePerVolume,
                                                  std::string& alertMsg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
	if( volumes != 0 )
	{
		DebugPrintf(SV_LOG_DEBUG, "Checking the free space availability for %d volumes\n", volumes) ;
		std::string bootVolume = GetBootVolume( ) ;
		DebugPrintf(SV_LOG_DEBUG, "bootVolume is %s \n", bootVolume.c_str()) ;
		SV_LONGLONG freeSpace ;
		GetfreeSpace( bootVolume, freeSpace ) ;
		if( freeSpace > ( requiredFreeSpacePerVolume *  volumes ) )
		{
			bRet = true ;
		}
		else
		{
			alertMsg= "There is no enough free space in the system volume " ;
			alertMsg += bootVolume ;
			std::string alertPrefix = alertMsg ;
			alertMsg += ". Replicating data on protected volumes to backup location may get affected. " ;
			alertMsg += "Number of volumes in protection : " ;
			alertMsg += boost::lexical_cast<std::string>( volumes ) ;
			alertMsg += ". Current free space : " ;
			alertMsg += BytesAsString( freeSpace ) ;
			alertMsg += " Required free space : " ;
			alertMsg += BytesAsString( requiredFreeSpacePerVolume *  volumes ) ;
			AlertConfigPtr alertConfigPtr = AlertConfig::GetInstance() ;
			alertConfigPtr->AddAlert("WARNING", "CX", SV_ALERT_SIMPLE,    // Change Alert Level from NOTICE to WARNING
				SV_ALERT_MODULE_RESYNC, alertMsg, alertPrefix) ;
		}
	}
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}

void VolumeConfig::ChangeRepoPath(const std::string& repoPath)
{
    bool repoAvailable = false ;
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ConfSchema::ObjectsIter_t repoIter ;
    repoIter = m_RepositoryGroup->m_Objects.begin() ;
        
    if( repoIter != m_RepositoryGroup->m_Objects.end() )
    {
		ConfSchema::AttrsIter_t attrIter ;
        ConfSchema::Object& repoObject = repoIter->second ;
		attrIter = repoObject.m_Attrs.find( m_repositoryAttrNamesObj.GetMountPointAttrName() ) ;
		attrIter->second = repoPath ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}

bool VolumeConfig::IsValidRepository()
{
	std::string repoName, repoPath ;
	if( !ConfSchemaMgr::GetInstance()->ShouldReconstructRepo() )
	{
		GetRepositoryNameAndPath(repoName, repoPath) ;
		if( repoPath.empty() )
		{
			return false ;
		}
	}
	return true ;
}
bool VolumeConfig::isExtendedOrShrunkVolumeGreaterThanSparseSize  (	const std::string &volumeName,
																	SV_LONGLONG &volumeCapcityInExistence )
{
	DebugPrintf (SV_LOG_DEBUG , "ENTERED %s \n ", FUNCTION_NAME); 
	bool bRet = false ; 
	ConfSchema::Object* volumeObj; 
	std::map<time_t, volumeResizeDetails> volumeResizeHistoryMap ; 
	GetVolumeObjectByVolumeName(volumeName,&volumeObj ); 
	GetVolumeResizeHistory(volumeObj,volumeResizeHistoryMap); 
	SV_LONGLONG capacity = 0 ;
	GetCapacity (volumeName,capacity);
	std::map<time_t, volumeResizeDetails>::reverse_iterator mapIter = volumeResizeHistoryMap.rbegin(); 
	bool isSparseFilesCreated = false; 
	while (mapIter != volumeResizeHistoryMap.rend())
	{
		volumeCapcityInExistence = mapIter->second.m_oldVolumeCapacity ; 
		DebugPrintf ( SV_LOG_DEBUG , "New capacity is %lld \n ", mapIter->second.m_newVolumeCapacity ) ; 
		DebugPrintf ( SV_LOG_DEBUG , "Old capacity is %lld \n ", mapIter->second.m_oldVolumeCapacity ) ; 
		DebugPrintf ( SV_LOG_DEBUG , "mapIter->second.m_IsSparseFilesCreated is %s \n ",mapIter->second.m_IsSparseFilesCreated.c_str()) ; 

		if (mapIter->second.m_IsSparseFilesCreated == "1")
		{
			isSparseFilesCreated = true; 

			volumeCapcityInExistence = boost::lexical_cast <SV_LONGLONG> ( mapIter->second.m_newVolumeCapacity );
			DebugPrintf ( SV_LOG_DEBUG , "Enter in to mapIter->second.m_IsSparseFilesCreated == \n ") ; 
			DebugPrintf ( SV_LOG_DEBUG , "volumeCapcityInExistence is%lld \n ", volumeCapcityInExistence ) ; 
			DebugPrintf ( SV_LOG_DEBUG , "capacity is %lld \n ", capacity) ; 

			if (capacity > mapIter->second.m_newVolumeCapacity)
			{
				bRet = true; 
			}
			break; 
		}
		mapIter ++; 
	}
	if (volumeResizeHistoryMap.size() > 0 && isSparseFilesCreated ==  false)
	{
		DebugPrintf (SV_LOG_DEBUG , "	if (volumeResizeHistoryMap.size() ) \n" ); 
		if (capacity > volumeCapcityInExistence)
		{
			bRet = true; 
		}
	}
	DebugPrintf ( SV_LOG_DEBUG , "Volume Name is %s Final volumeCapcityInExistence capacity is %lld and currentCapacity is %lld \n ",volumeName.c_str(),volumeCapcityInExistence, capacity) ; 
	DebugPrintf (SV_LOG_DEBUG , "EXITED %s \n ", FUNCTION_NAME); 
	return bRet; 
}


bool VolumeConfig::isShrunkVolume(const std::string &volumeName )
{
	DebugPrintf (SV_LOG_DEBUG, "ENTERED %s \n ", FUNCTION_NAME); 
	bool bRet = false ; 
	volumeResizeDetails volumeresizeDetails ; 
	SV_LONGLONG latestResizeTs; 
	GetLatestResizeTs(volumeName, latestResizeTs); 
	GetResizeDetails(volumeName, latestResizeTs , volumeresizeDetails ) ; 
	if (volumeresizeDetails.m_newVolumeRawSize < volumeresizeDetails.m_oldVolumeRawSize)
	{
		bRet = true; 
	}
	return bRet ; 
	DebugPrintf (SV_LOG_DEBUG , "EXITED %s \n ", FUNCTION_NAME); 
}

bool VolumeConfig::isVirtualVolumeExtended(const std::string &volumeName )
{
	DebugPrintf (SV_LOG_DEBUG, "ENTERED %s \n ", FUNCTION_NAME); 
	bool bRet = false ;
	SV_LONGLONG rawSize = 0 , virtualVolumeRawSize = 0 ; 
	GetRawSize (volumeName,rawSize); 
	std::string virtualVolumeName = GetCdpClivolumeName(volumeName);
	virtualVolumeRawSize = getVirtualVolumeRawSize (virtualVolumeName); 
	DebugPrintf (SV_LOG_DEBUG , " Virtual Volume Raw Size is %lld " ,virtualVolumeRawSize);
	DebugPrintf (SV_LOG_DEBUG , " Volume Raw Size is  %lld " ,rawSize);
	if (rawSize == virtualVolumeRawSize)
	{
		bRet = true; 
	}
	DebugPrintf (SV_LOG_DEBUG , "EXITED %s \n ", FUNCTION_NAME); 
	return bRet ; 
}


bool VolumeConfig::SetRawSize(const std::string& volumeName, SV_LONGLONG& rawSize)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    bool bRet = false ;
    ConfSchema::Object* volObj ;
    if( GetVolumeObjectByVolumeName(volumeName, &volObj) )
    {
        ConfSchema::AttrsIter_t attrIter ;
        attrIter = volObj->m_Attrs.find( m_volumeAttrNamesObj.GetRawSizeAttrName() ) ;
		attrIter->second = boost::lexical_cast <std::string > ( rawSize ) ; 
        bRet = true ;
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
    return bRet ;
}


bool VolumeConfig::GetVolumeGroupName(const std::string& volumename, std::string& volumegrpname)
{
	bool bret = false ;
	ConfSchema::Object* volObj ;
	if( GetVolumeObjectByVolumeName(volumename, &volObj) )
	{
		volumegrpname = volObj->m_Attrs.find( m_volumeAttrNamesObj.GetVolumeGrpAttrName() )->second ;
		DebugPrintf(SV_LOG_DEBUG, "Volume group name for %s is %s\n", volumename.c_str(), volumegrpname.c_str()) ;
		bret = true ;

	}
	return bret ;
}

bool DoesDiskContainsProtectedVolumes(const std::string& newrepopath, const std::list<std::string>& protectedvolumes, 
									  std::string& diskname, std::string& volume)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool bret = false ;
	if( !isRepositoryConfiguredOnCIFS(newrepopath))
	{
		std::string repovolume = newrepopath ;
		if( repovolume.length() == 3 )
		{
			repovolume = repovolume[0] ;
		}
		else
		{
			if( repovolume[repovolume.length() - 1] == '\\' )
			{
				repovolume.erase(repovolume.length() - 1) ;
			}
		}
		VolumeConfigPtr volumeConfigPtr = VolumeConfig::GetInstance() ;
		std::string repovolumegroupname ;
		if( volumeConfigPtr->GetVolumeGroupName( repovolume,  repovolumegroupname) )
		{
			std::list<std::string>::const_iterator protectedvolIter = protectedvolumes.begin() ;
			while( protectedvolIter != protectedvolumes.end() )
			{
				std::string volumegroupname ;
				if( volumeConfigPtr->GetVolumeGroupName( *protectedvolIter, volumegroupname ) )
				{
					if( volumegroupname == repovolumegroupname )
					{
						diskname = volumegroupname ;
						volume = *protectedvolIter ;
						bret = true ;
						break ;
					}
				}
				else
				{
					DebugPrintf(SV_LOG_WARNING, "Couldnot get the volume group name for %s\n", protectedvolIter->c_str()) ;
				}
				protectedvolIter++ ;
			}
		}
		else
		{
			DebugPrintf(SV_LOG_WARNING, "Couldnot get the volume group name for %s\n", repovolume.c_str()) ;
		}
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return bret ;
}

void VolumeConfig::SetDummyGrps(ConfSchema::Group& volgrp, ConfSchema::Group& repogrp)
{
	m_VolumeGroup = &volgrp; 
	m_RepositoryGroup = &repogrp ;
}
bool VolumeConfig::GetSameVolumeList(const  std::list<std::string>& protectableVolumes,
									std::list<std::list<std::string> >& sameVolumeList)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool bRet = false ;
#ifdef SV_WINDOWS
	std::list<std::string> tmpProtectableVolumes = protectableVolumes ;
	std::list<std::string> volumes = protectableVolumes ;
	std::list<std::string> volList ;
	
	while(tmpProtectableVolumes.size() != 0)
	{
		tmpProtectableVolumes = volumes ;
		std::list<std::string>::const_iterator tmpProtectedvolIter = tmpProtectableVolumes.begin() ;
		if (tmpProtectedvolIter != tmpProtectableVolumes.end())
		{
			char szGuid[MAX_PATH];
			std::string tempPath = *tmpProtectedvolIter ;
			if( tempPath.length() == 1 )
			{
				tempPath += ":\\" ;
			}
			if( tempPath[tempPath.length() - 1 ] != '\\' )
			{
				tempPath += '\\' ;
			}
			GetVolumeNameForVolumeMountPoint(tempPath.c_str(), szGuid, sizeof(szGuid));
			while( tmpProtectedvolIter != tmpProtectableVolumes.end() )
			{
				char szGuid1[MAX_PATH];
				std::string newTmpPath = *tmpProtectedvolIter ;
				if( newTmpPath.length() == 1 )
				{
					newTmpPath += ":\\" ;
				}
				if( newTmpPath[newTmpPath.length() - 1 ] != '\\' )
				{
					newTmpPath += '\\' ;
				}
				GetVolumeNameForVolumeMountPoint(newTmpPath.c_str(), szGuid1, sizeof(szGuid1));
				if ( strcmp(szGuid,szGuid1) == 0 )
				{
					volList.push_back(*tmpProtectedvolIter) ;
					volumes.remove(*tmpProtectedvolIter);
				}
				tmpProtectedvolIter++ ;
			}
			if( volList.size() > 1 )
			{
				sameVolumeList.push_back(volList) ;
				bRet = true ;
			}
			volList.clear() ;
		}
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
#endif
	return bRet ;
}
