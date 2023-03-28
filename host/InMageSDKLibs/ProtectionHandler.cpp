#include "ProtectionHandler.h"
#include <algorithm>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/case_conv.hpp>

#include "inmstrcmp.h"
#include "inmsdkutil.h"
#include "localconfigurator.h"
#include "host.h"
#include "confengine/volumeconfig.h"
#include "confengine/scenarioconfig.h"
#include "confengine/confschemareaderwriter.h"
#include "confengine/policyconfig.h"
#include "confengine/protectionpairconfig.h"
#include "util.h"
#include "confengine/agentconfig.h"
#include "cdpplatform.h"
#include "InmsdkGlobals.h"
#include "confengine/alertconfig.h"
#include "confengine/eventqueueconfig.h"
#include "APIController.h"
#include "../inmsafecapis/inmsafecapis.h"

ProtectionHandler::ProtectionHandler(void)
{
}

ProtectionHandler::~ProtectionHandler(void)
{
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;

}

void ProtectionHandler::GetVolumeLevelPg(const std::string& volumeName,
										 INM_ERROR volError,
										 const std::string& msg,
										 ParameterGroup& pg)
{
	ValueType vt ;
	vt.value = volumeName ;
	pg.m_Params.insert(std::make_pair( "VolumeName",  vt  )) ;
	vt.value = boost::lexical_cast<std::string>(volError)  ;

	pg.m_Params.insert(std::make_pair( "ReturnCode", vt)) ;
	vt.value = getErrorMessage(volError) + msg ;
	pg.m_Params.insert(std::make_pair( "ErrorMessage", vt)) ;
}

INM_ERROR ProtectionHandler::GetErrorCodeFromPairState(VOLUME_SETTINGS::PAIR_STATE pairState)
{
	INM_ERROR errCode = E_UNKNOWN ;
	switch( pairState )
	{
	case VOLUME_SETTINGS::PAUSE :
		errCode = E_VOLUME_ALREADY_PAUSED ;
		break ;
	case VOLUME_SETTINGS::PAUSE_TACK_COMPLETED :
		errCode = E_VOLUME_ALREADY_PAUSE_TRACKED ;
		break ;
	case VOLUME_SETTINGS::PAIR_PROGRESS :
		errCode = E_VOLUME_PROTECTION_INTACT ;
		break ;
	case VOLUME_SETTINGS::PAUSE_PENDING :
		errCode = E_VOLUME_ALREADY_PAUSED;
		break;
	case VOLUME_SETTINGS::PAUSE_TACK_PENDING :
		errCode = E_VOLUME_ALREADY_PAUSE_TRACKED;
		break;
	case VOLUME_SETTINGS::DELETE_PENDING :
		errCode = E_VOLUME_NOT_PROTECTED ;
		break ;
	case VOLUME_SETTINGS::CLEANUP_DONE :
		errCode = E_VOLUME_NOT_PROTECTED ;
		break ;
	case VOLUME_SETTINGS::RESUME_PENDING :
		errCode = E_VOLUME_QUEUED_FOR_RESYNC ;
		break; 
	default :
		errCode = E_UNKNOWN ;
	}
	return errCode ;
}

void GetRepositoryName(FunctionInfo& request, std::string& repoName)
{
	ConstParametersIter_t paramIter ;
	paramIter = request.m_RequestPgs.m_Params.find( "RepositoryName") ;
	repoName= paramIter->second.value ;
}

void GetRepositoryDeviceName(FunctionInfo& request, std::string& repoDeviceName)
{
	ConstParametersIter_t paramIter ;
	paramIter = request.m_RequestPgs.m_Params.find( "RepositoryDeviceName") ;
	repoDeviceName = paramIter->second.value ;
}


void GetExcludedVolumes(FunctionInfo& request, std::list<std::string>& excludedvolumes)
{
	ConstParameterGroupsIter_t pgIter ;
	std::set<std::string> excludeSet ;
	std::pair<std::set<std::string> ::iterator, bool> insertItemPos ;
	pgIter = request.m_RequestPgs.m_ParamGroups.find("ExclusionList") ;
	if( pgIter != request.m_RequestPgs.m_ParamGroups.end() )
	{
		ConstParametersIter_t paramIter = pgIter->second.m_Params.begin() ;
		while( paramIter != pgIter->second.m_Params.end() )
		{
			insertItemPos = excludeSet.insert( paramIter->second.value )  ;
			if( insertItemPos.second == true )             
			{
				excludedvolumes.push_back(paramIter->second.value) ;
			}
			paramIter++ ;
		}
	}
}

void GetRetentionDuration( RetentionPolicy& retPolicy, double &dNoOptimizedDuration, double &dOptimizedDuration )
{
	time_t durationInHours ;
	std::string timeIntervalUnit ;
	GetDurationAndTimeIntervelUnit( retPolicy.duration, durationInHours, timeIntervalUnit ) ;
	dNoOptimizedDuration = ( (double)durationInHours ) / 24 ;
	std::list<ScenarioRetentionWindow>::iterator retWindowBegin = retPolicy.retentionWindows.begin() ;
	while( retWindowBegin != retPolicy.retentionWindows.end() )
	{
		GetDurationAndTimeIntervelUnit( retWindowBegin->duration,  durationInHours, timeIntervalUnit ) ;
		dOptimizedDuration +=  ( (double)durationInHours ) / 24 ;
		retWindowBegin++ ;
	}
	return ;
}
void GetCapacityAndFsOccupiedSize( const std::list<std::string>& eligiblevolumes,
								  SV_ULONGLONG& totalCapacity, 
								  SV_ULONGLONG& totalFsOccupiedSize )
{
	SV_ULONGLONG protectedCapacity, eligibleVolumesCapacity, eligibleFsOccupiedSize, protectedFsOccupiedSize ;
	protectedCapacity = eligibleVolumesCapacity = eligibleFsOccupiedSize = protectedFsOccupiedSize  = 0 ;
	totalCapacity = totalFsOccupiedSize = 0 ;
	ScenarioConfigPtr scenarioConfigPtr =  ScenarioConfig::GetInstance() ;
	VolumeConfigPtr volumeConf = VolumeConfig::GetInstance();
	std::list<std::string> protectedVolumes;
	scenarioConfigPtr->GetProtectedVolumes( protectedVolumes ) ;
	GetTotalCapacity(protectedVolumes, protectedCapacity) ;
	GetTotalCapacity(eligiblevolumes, eligibleVolumesCapacity) ;
	GetTotalFileSystemUsedSpace (eligiblevolumes, eligibleFsOccupiedSize) ;
	GetTotalFileSystemUsedSpace (protectedVolumes, protectedFsOccupiedSize ) ;
	totalCapacity = protectedCapacity + eligibleVolumesCapacity ;
	totalFsOccupiedSize = eligibleFsOccupiedSize + protectedFsOccupiedSize ;
}

bool IsEnoughSpaceInRepo (SV_ULONGLONG &repoFreeSpace,std::list<std::string> &eligibleVolumes,
						  SV_ULONGLONG &requiredSpace,SV_ULONGLONG &protectedVolumeOverHead)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool isEnougnSpace = false;
	RetentionPolicy retPolicy;
	SV_ULONGLONG totalCapacity, totalFsOccupiedSize ;
	GetCapacityAndFsOccupiedSize( eligibleVolumes, totalCapacity, totalFsOccupiedSize ) ;
	ScenarioConfigPtr scenarioConfigPtr =  ScenarioConfig::GetInstance() ;
	scenarioConfigPtr->GetRetentionPolicy (retPolicy);
	return IsEnoughSpaceInRepo (repoFreeSpace, totalCapacity, totalFsOccupiedSize, retPolicy, requiredSpace, protectedVolumeOverHead);
}

bool IsEnoughSpaceInRepoV2 (	SV_ULONGLONG &repoFreeSpace,
								std::list<std::string> &eligibleVolumes,
								SV_ULONGLONG &requiredSpace,
								SV_ULONGLONG &protectedVolumeOverHead,
								bool supportSparseFile,
								double compressionBenifits , 
								double sparseSavingsFactor ,
								double changeRate, 
								double ntfsMetaDataoverHead
							)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool isEnougnSpace = false;
	RetentionPolicy retPolicy;
	SV_ULONGLONG totalCapacity, totalFsOccupiedSize ;
	GetCapacityAndFsOccupiedSize( eligibleVolumes, totalCapacity, totalFsOccupiedSize ) ;
	ScenarioConfigPtr scenarioConfigPtr =  ScenarioConfig::GetInstance() ;
	scenarioConfigPtr->GetRetentionPolicy (retPolicy);
	return IsEnoughSpaceInRepoV2 (repoFreeSpace, 
								totalCapacity, 
								totalFsOccupiedSize, 
								retPolicy, 
								requiredSpace, 
								protectedVolumeOverHead,
								supportSparseFile,
								compressionBenifits , 
								sparseSavingsFactor ,
								changeRate, 
								ntfsMetaDataoverHead 
								) ; 

}
bool IsEnoughSpaceInRepo( SV_ULONGLONG& repoFreeSpace, SV_ULONGLONG protectedCapacity, SV_ULONGLONG fileSystemOccupiedSize,
						 RetentionPolicy& retPolicy, SV_ULONGLONG& requiredSpace, SV_ULONGLONG& protectedVolumeOverHead)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	std::string repoVol, repoName ,repoPath  ;
	VolumeConfigPtr volumeConfigPtr = VolumeConfig::GetInstance();
	volumeConfigPtr->GetRepositoryNameAndPath( repoName, repoPath ) ;
	volumeConfigPtr->GetRepositoryVolume(repoName, repoVol) ;

	if (isRepositoryConfiguredOnCIFS (repoPath) == false)
	{
		volumeConfigPtr->GetRepositoryFreeSpace(repoName,repoFreeSpace);
	}
	else 
	{
		SV_ULONGLONG capacity,quota;
		GetDiskFreeSpaceP (repoVol ,&quota ,&capacity,&repoFreeSpace);
	}

	double dNoOptimizedDuration = 0, dOptimizedDuration = 0 ;
	GetRetentionDuration( retPolicy, dNoOptimizedDuration, dOptimizedDuration ) ;
	protectedVolumeOverHead = GetProtectedVolumeOverHead();
	DebugPrintf(SV_LOG_DEBUG, "Exited %s\n", FUNCTION_NAME) ;
	return bCheckSpaceAvailability (repoFreeSpace,protectedCapacity, fileSystemOccupiedSize,requiredSpace,dNoOptimizedDuration,dOptimizedDuration,protectedVolumeOverHead);
}

bool IsEnoughSpaceInRepoV2(	SV_ULONGLONG& repoFreeSpace, 
							SV_ULONGLONG protectedCapacity, 
							SV_ULONGLONG fileSystemOccupiedSize,
							RetentionPolicy& retPolicy, 
							SV_ULONGLONG& requiredSpace, 
							SV_ULONGLONG& protectedVolumeOverHead , 
							bool supportSparseFile,
							double compressionBenifits , 
							double sparseSavingsFactor ,
							double changeRate, 
							double ntfsMetaDataoverHead
						 )
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	std::string repoVol, repoName ,repoPath  ;
	VolumeConfigPtr volumeConfigPtr = VolumeConfig::GetInstance();
	volumeConfigPtr->GetRepositoryNameAndPath( repoName, repoPath ) ;
	volumeConfigPtr->GetRepositoryVolume(repoName, repoVol) ;

	if (isRepositoryConfiguredOnCIFS (repoPath) == false)
	{
		volumeConfigPtr->GetRepositoryFreeSpace(repoName,repoFreeSpace);
	}
	else 
	{
		SV_ULONGLONG capacity,quota;
		GetDiskFreeSpaceP (repoVol ,&quota ,&capacity,&repoFreeSpace);
	}
	
    double dNoOptimizedDuration = 0, dOptimizedDuration = 0 ;
    GetRetentionDuration( retPolicy, dNoOptimizedDuration, dOptimizedDuration ) ;
	protectedVolumeOverHead = GetProtectedVolumeOverHead();
	DebugPrintf(SV_LOG_DEBUG, "Exited %s\n", FUNCTION_NAME) ;
	return bCheckSpaceAvailabilityV2 (repoFreeSpace,
									protectedCapacity, 
									fileSystemOccupiedSize,
									requiredSpace,
									dNoOptimizedDuration,
									dOptimizedDuration,
									protectedVolumeOverHead , 
									supportSparseFile,
									compressionBenifits , 
									sparseSavingsFactor ,
									changeRate, 
									ntfsMetaDataoverHead
									) ; 
}
bool IsEnoughSpaceInRepoWithOutXMLRead ( SV_ULONGLONG& repoFreeSpace, SV_ULONGLONG protectedCapacity, SV_ULONGLONG fileSystemOccupiedSize,
										RetentionPolicy& retPolicy, SV_ULONGLONG& requiredSpace, SV_ULONGLONG& protectedVolumeOverHead)
{	
	DebugPrintf(SV_LOG_DEBUG, "ENTER %s\n", FUNCTION_NAME) ;
	double dNoOptimizedDuration = 0, dOptimizedDuration = 0 ;
	GetRetentionDuration( retPolicy, dNoOptimizedDuration, dOptimizedDuration ) ;
	protectedVolumeOverHead = GetProtectedVolumeOverHead();
	DebugPrintf(SV_LOG_DEBUG, "Exited %s\n", FUNCTION_NAME) ;
	return bCheckSpaceAvailability (repoFreeSpace,protectedCapacity, fileSystemOccupiedSize,requiredSpace,dNoOptimizedDuration,dOptimizedDuration,protectedVolumeOverHead);
}

bool IsEnoughSpaceInRepoWithOutXMLReadV2 (	SV_ULONGLONG& repoFreeSpace, 
											SV_ULONGLONG protectedCapacity, 
											SV_ULONGLONG fileSystemOccupiedSize,
											RetentionPolicy& retPolicy, 
											SV_ULONGLONG& requiredSpace, 
											SV_ULONGLONG& protectedVolumeOverHead , 
											bool supportSparseFile,
											double compressionBenifits , 
											double sparseSavingsFactor ,
											double changeRate, 
											double ntfsMetaDataoverHead
											
										 )
{	
	DebugPrintf(SV_LOG_DEBUG, "ENTER %s\n", FUNCTION_NAME) ;
    double dNoOptimizedDuration = 0, dOptimizedDuration = 0 ;
    GetRetentionDuration( retPolicy, dNoOptimizedDuration, dOptimizedDuration ) ;
	protectedVolumeOverHead = GetProtectedVolumeOverHead();
	DebugPrintf(SV_LOG_DEBUG, "Exited %s\n", FUNCTION_NAME) ;
	return bCheckSpaceAvailabilityV2 (	repoFreeSpace,
										protectedCapacity, 
										fileSystemOccupiedSize,
										requiredSpace,
										dNoOptimizedDuration,
										dOptimizedDuration,
										protectedVolumeOverHead, 
										supportSparseFile,
										compressionBenifits , 
										sparseSavingsFactor ,
										changeRate, 
										ntfsMetaDataoverHead
										
										);
}
bool IsEnoughSpaceInRepo( SV_ULONGLONG& repoFreeSpace, std::map<std::string, PairDetails>& pairDetails, 
						 SV_ULONGLONG& requiredSpace,SV_ULONGLONG & protectedVolumeOverHead )
{
	std::map<std::string, PairDetails>::const_iterator pairDetailIter = pairDetails.begin() ;
	std::list<std::string> eligibleVolumes ;
	RetentionPolicy retPolicy ;
	std::list <std::string> protectedVolumes;
	ScenarioConfigPtr scenarioConfigPtr =  ScenarioConfig::GetInstance() ;
	scenarioConfigPtr->GetProtectedVolumes( protectedVolumes ) ;

	while( pairDetailIter != pairDetails.end() )
	{
		if( std::find( protectedVolumes.begin(), protectedVolumes.end(), pairDetailIter->first) == 
			protectedVolumes.end() )
		{
			eligibleVolumes.push_back( pairDetailIter->first ) ;
		}
		retPolicy = pairDetailIter->second.retPolicy ;
		pairDetailIter++ ;
	}


	SV_ULONGLONG totalCapacity, totalFsOccupiedSize ;
	GetCapacityAndFsOccupiedSize( eligibleVolumes, totalCapacity, totalFsOccupiedSize ) ;
	return IsEnoughSpaceInRepo (repoFreeSpace, totalCapacity,
		totalFsOccupiedSize, retPolicy, requiredSpace, protectedVolumeOverHead);
}

bool IsEnoughSpaceInRepoV2( SV_ULONGLONG& repoFreeSpace, 
							std::map<std::string,PairDetails>& pairDetails, 
							SV_ULONGLONG& requiredSpace,
							SV_ULONGLONG & protectedVolumeOverHead,
							bool supportSparseFile,
							double compressionBenifits , 
							double sparseSavingsFactor ,
							double changeRate, 
							double ntfsMetaDataoverHead
							
						 )
{
	std::map<std::string, PairDetails>::const_iterator pairDetailIter = pairDetails.begin() ;
	std::list<std::string> eligibleVolumes ;
	RetentionPolicy retPolicy ;
	std::list <std::string> protectedVolumes;
	ScenarioConfigPtr scenarioConfigPtr =  ScenarioConfig::GetInstance() ;
	scenarioConfigPtr->GetProtectedVolumes( protectedVolumes ) ;

	while( pairDetailIter != pairDetails.end() )
	{
		if( std::find( protectedVolumes.begin(), protectedVolumes.end(), pairDetailIter->first) == 
							protectedVolumes.end() )
		{
			eligibleVolumes.push_back( pairDetailIter->first ) ;
		}
		retPolicy = pairDetailIter->second.retPolicy ;
		pairDetailIter++ ;
	}


	SV_ULONGLONG totalCapacity = 0 , totalFsOccupiedSize = 0;
	GetCapacityAndFsOccupiedSize( eligibleVolumes, totalCapacity, totalFsOccupiedSize ) ;
	return IsEnoughSpaceInRepoV2 (	repoFreeSpace, 
									totalCapacity,
									totalFsOccupiedSize, 
									retPolicy, 
									requiredSpace, 
									protectedVolumeOverHead,
									supportSparseFile,
									compressionBenifits , 
									sparseSavingsFactor ,
									changeRate, 
									ntfsMetaDataoverHead
									);
}
SV_ULONGLONG GetProtectedVolumeOverHead (void)
{
	SV_ULONGLONG protectedVolumeOverHead = 0;
	std::list<std::string> protectedVolumes;
	VolumeConfigPtr volumeConfigPtr = VolumeConfig::GetInstance() ;
	ScenarioConfigPtr scenarioConfigPtr =  ScenarioConfig::GetInstance() ;
	ProtectionPairConfigPtr protectionPairConf = ProtectionPairConfig::GetInstance();
	scenarioConfigPtr->GetProtectedVolumes( protectedVolumes ) ;
	SV_ULONGLONG sizeondisk = volumeConfigPtr->GetVolPackVolumeSizeOnDisk();
	DebugPrintf (SV_LOG_DEBUG , "after GetVolPackVolumeSizeOnDisk  sizeOnDisk is %llu" ,sizeondisk );
	protectedVolumeOverHead = sizeondisk + protectionPairConf->GetCDPStoreSizeByProtectedVolumes(protectedVolumes) + ( SV_ULONGLONG ) convertGBtoBytes ( GetRepoLogsSpace()) ;
	DebugPrintf ( "protectedVolumeOverHead in GetProtectedVolumeOverHead is %I64d \n " , protectedVolumeOverHead ); 
	return protectedVolumeOverHead;
}
bool bCheckSpaceAvailability (SV_ULONGLONG repoFreeSpace,SV_ULONGLONG totalProtectedCapacity, SV_ULONGLONG fileSystemOccupiedSize,
							  SV_ULONGLONG &requiredSpace,double dNoOptimizedDuration,double dOptimizedDuration,
							  SV_ULONGLONG &protectedVolumeOverHead)
{	
	double dlfileSystemOccupiedSize = boost::lexical_cast<double>(fileSystemOccupiedSize); //8.6; //((fileSystemOccupiedSize /1024) /1024 )/1024;
	DebugPrintf ( "total size used by FileSystem is %lf \n " , dlfileSystemOccupiedSize );
	dlfileSystemOccupiedSize = dlfileSystemOccupiedSize /( 1024 * 1024 * 1024 ) ;  
	double dltotalProtectedCapacity = (double)totalProtectedCapacity / (1024 * 1024 * 1024);
	DebugPrintf ( "totalProtectedCapacity is %lf \n " , dltotalProtectedCapacity );
	double nonOverlappedChangeRate = 0.03 ; 
	double totalChangeRate = (double)7/100 ;
	int sparseSavingFactor = 3 ; 
	double compressionBenifits = (double) 40 /100;

	double inmageSpace = GetRepoLogsSpace ();
	double retentionPolicyDays = GetRetentionPolicyDays (dNoOptimizedDuration,dOptimizedDuration);
	DebugPrintf("retentionPolicyDays is %lf \n " ,retentionPolicyDays);
	double anticipatedNonOverlappedIOSize = GetAnticipatedNonOverlappedIOSize (nonOverlappedChangeRate,dlfileSystemOccupiedSize,retentionPolicyDays);

	SV_UINT compressVolpacks; 
	LocalConfigurator localConfigurator;
	compressVolpacks = localConfigurator.VirtualVolumeCompressionEnabled();
	double volPackSizeWithOutCompression = GetVolpackSizeWithOutCompression (anticipatedNonOverlappedIOSize ,dlfileSystemOccupiedSize ,dltotalProtectedCapacity);
	DebugPrintf ("volPackSize With Out Compression is %lf \n ",volPackSizeWithOutCompression);
	double volPackSize = GetVolpackSize (anticipatedNonOverlappedIOSize,dlfileSystemOccupiedSize,dltotalProtectedCapacity , compressVolpacks);
	DebugPrintf ("volPackSize is %lf \n ",volPackSize);

	double cdpStoreNotOptimisedSpace = GetCDPStoreNotOptimisedSpace (dlfileSystemOccupiedSize, totalChangeRate , dNoOptimizedDuration);
	double cdpStoreOptimisedUsage = GetCDPStoreOptimisedUsage (dlfileSystemOccupiedSize,totalChangeRate,dOptimizedDuration,sparseSavingFactor);

	double totalCDPSpaceNoCompression = GetTotalCDPSpaceNoCompression (cdpStoreNotOptimisedSpace, cdpStoreOptimisedUsage); 
	DebugPrintf ( "totalCDPSpaceWithOut Compression is %lf \n" , totalCDPSpaceNoCompression );
	double totalCDPSpaceCompression = 0; 
	if (compressVolpacks == 1 )
	{
		totalCDPSpaceCompression = GetToatlCDPSpaceWithCompression (totalCDPSpaceNoCompression , compressionBenifits); 
		DebugPrintf ( "totalCDPSpaceWithCompression is %lf \n" , totalCDPSpaceCompression );
	}
	double NTFSMetaDataOverHead = volPackSizeWithOutCompression * 13 /100 ; 
	double totalSize ;
	if (compressVolpacks == 1 )
		totalSize = TotalSpaceRequired (volPackSize,totalCDPSpaceCompression,NTFSMetaDataOverHead,inmageSpace); 
	else 
		totalSize = TotalSpaceRequiredWithCompression (volPackSize,totalCDPSpaceNoCompression,NTFSMetaDataOverHead,inmageSpace);

	requiredSpace = ( SV_ULONGLONG ) convertGBtoBytes (totalSize);

	DebugPrintf (SV_LOG_DEBUG,"requiredSpace  in Bytes is %I64d \n " , requiredSpace ); 

	DebugPrintf ( SV_LOG_DEBUG , "Backup location FreeSpace : %I64d protectedVolumeOverHead %I64d requiredSpace %I64d\n ", repoFreeSpace,protectedVolumeOverHead,requiredSpace) ;

	DebugPrintf ( SV_LOG_DEBUG , "NTFSMetaDataOverHead  in Bytes is %lf \n " , NTFSMetaDataOverHead ); 

	return checkSpaceAvailability (repoFreeSpace,requiredSpace,protectedVolumeOverHead);
}

SV_ULONGLONG convertGBtoBytes(const double totalSize) 
{
	return (SV_ULONGLONG)(totalSize * 1024 * 1024 *1024 ); 
}
bool checkSpaceAvailability (const SV_ULONGLONG repoFreeSpace ,const SV_ULONGLONG requiredSpace , const SV_ULONGLONG &protectedVolumeOverHead)
{
	bool bRet; 
	//Bug 25623 - 2.0>>Scout SSE>Space check calculation should be on 80% of backup location size because we start pruning beyond it
	if ((0.8 * repoFreeSpace + protectedVolumeOverHead ) >  requiredSpace)
		bRet = true;
	else 
		bRet = false;
	return bRet;
}


double GetRetentionPolicyDays (const double dNoOptimizedDuration ,const double dOptimizedDuration)
{
	return (dNoOptimizedDuration + dOptimizedDuration); 
}
double GetAnticipatedNonOverlappedIOSize (const double nonOverlappedChangeRate ,const double  dlfileSystemOccupiedSize ,const double retentionPolicyDays)
{
	return (nonOverlappedChangeRate*dlfileSystemOccupiedSize*retentionPolicyDays);
}
double GetVolpackSizeWithOutCompression (const double anticipatedNonOverlappedIOSize ,const double fileSystemOccupiedSize ,const double dltotalProtectedCapacity)
{
	double volPackSize; 
	if (( anticipatedNonOverlappedIOSize + fileSystemOccupiedSize ) < dltotalProtectedCapacity)
	{
		volPackSize = anticipatedNonOverlappedIOSize + fileSystemOccupiedSize ; 
	}
	else 
	{
		volPackSize = dltotalProtectedCapacity;
	}
	return volPackSize;
}
double GetVolpackSize (const double anticipatedNonOverlappedIOSize ,const double fileSystemOccupiedSize ,const double dltotalProtectedCapacity, SV_UINT compressVolpacks)
{
	double volPackSize; 
	DebugPrintf (SV_LOG_DEBUG, "compressVolpacks value is %d \n", compressVolpacks); 
	volPackSize = GetVolpackSizeWithOutCompression (anticipatedNonOverlappedIOSize,fileSystemOccupiedSize,dltotalProtectedCapacity);	
	if (compressVolpacks == 1)
	{
		double compressionBenifits = (double)20 /100 ;
		volPackSize =  volPackSize - ( volPackSize * compressionBenifits );
	}
	return volPackSize; 
}
double GetAnticipatedNonOverlappedIOSize (	double dNoOptimizedDuration , 
											double dOptimizedDuration , 
											double nonOverlappedChangeRate , 
											double dlfileSystemOccupiedSize ) 
{
	double inmageSpace = GetRepoLogsSpace ();
	double retentionPolicyDays = GetRetentionPolicyDays (dNoOptimizedDuration,dOptimizedDuration);
	DebugPrintf("retentionPolicyDays is %lf \n " ,retentionPolicyDays);
	double anticipatedNonOverlappedIOSize = GetAnticipatedNonOverlappedIOSize (
												nonOverlappedChangeRate,
												dlfileSystemOccupiedSize,
												retentionPolicyDays
											);
	return anticipatedNonOverlappedIOSize ;
}
SV_UINT VirtualVolumeCompressValue ()
{
	SV_UINT compressVolpacks; 
	LocalConfigurator localConfigurator;
	compressVolpacks = localConfigurator.VirtualVolumeCompressionEnabled();
	return compressVolpacks; 
}

double GetTotalCDPSpace (double dlfileSystemOccupiedSize, 
						 double totalChangeRate,
						 double dNoOptimizedDuration,
						 double sparseSavingFactor,
						 double dOptimizedDuration, 
						 double compressionBenifits) 
{
	SV_UINT cdpCompression; 
	double cdpStoreNotOptimisedSpace = GetCDPStoreNotOptimisedSpace 
										(dlfileSystemOccupiedSize, 
										totalChangeRate , 
										dNoOptimizedDuration);

	DebugPrintf ( "dlfileSystemOccupiedSize is %lf \n" , dlfileSystemOccupiedSize );
	DebugPrintf ( "totalChangeRate is %lf \n" , totalChangeRate );
	DebugPrintf ( "dNoOptimizedDuration is %lf \n" , dNoOptimizedDuration );


	double cdpStoreOptimisedUsage = GetCDPStoreOptimisedUsage 
									(dlfileSystemOccupiedSize,
									totalChangeRate,
									dOptimizedDuration,
									sparseSavingFactor);
	DebugPrintf ( "dlfileSystemOccupiedSize is %lf \n" , dlfileSystemOccupiedSize );
	DebugPrintf ( "totalChangeRate is %lf \n" , totalChangeRate );
	DebugPrintf ( "dOptimizedDuration is %lf \n" , dOptimizedDuration );
	DebugPrintf ( "sparseSavingFactor is %lf \n" , sparseSavingFactor );
	DebugPrintf ( "compressionBenifits is %lf \n" , compressionBenifits );


    double totalCDPSpaceNoCompression = GetTotalCDPSpaceNoCompression (
											cdpStoreNotOptimisedSpace, 
											cdpStoreOptimisedUsage
										); 
	DebugPrintf ( "cdpStoreNotOptimisedSpace is %lf \n" , cdpStoreNotOptimisedSpace );
	DebugPrintf ( "cdpStoreOptimisedUsage is %lf \n" , cdpStoreOptimisedUsage );

	DebugPrintf ( "totalCDPSpaceWithOut Compression is %lf \n" , totalCDPSpaceNoCompression );
	double totalCDPSpaceCompression = totalCDPSpaceNoCompression ; 
	LocalConfigurator configurator ;
	cdpCompression = configurator.GetCDPCompression(); // VirtualVolumeCompressValue (); 
	if (cdpCompression == 1 )
	{
		totalCDPSpaceCompression = GetToatlCDPSpaceWithCompression (totalCDPSpaceNoCompression , compressionBenifits); 
		DebugPrintf ( "totalCDPSpaceWithCompression is %lf \n" , totalCDPSpaceCompression );
	}
	return totalCDPSpaceCompression ; 
}
double GetVolpackSizeV2 ( double dlfileSystemOccupiedSize, double dltotalProtectedCapacity , bool supportSparseFile ) 
{
	SV_UINT compressVolpacks = 0; 
	double volPackSize ,volPackSizeWithOutCompression ;
	if(supportSparseFile)
	{
		volPackSizeWithOutCompression = GetVolpackSizeWithOutCompressionV2 (dlfileSystemOccupiedSize);
		DebugPrintf ("volPackSize With Out Compression when sparsefile  support is %lf \n ",dlfileSystemOccupiedSize);
		volPackSize = GetVolpackSizeV2 (dlfileSystemOccupiedSize,compressVolpacks);
	}
	else
	{
		volPackSizeWithOutCompression = GetVolpackSizeWithOutCompressionV2 (dltotalProtectedCapacity);
		DebugPrintf ("volPackSize With Out Compression when sparsefile not support is %lf \n ",dltotalProtectedCapacity);
		volPackSize = GetVolpackSizeV2 (dltotalProtectedCapacity,compressVolpacks);
	}
	
	return volPackSize; 
}
bool CheckSpaceAvailability(std::string& repovolumename,
							RetentionPolicy& retPolicy,
							std::list<std::string>& totalProtectVolumes,
							SV_ULONGLONG &requiredSpace,
							SV_ULONGLONG &repoFreeSpace,
							double compressionBenifits, 
							double sparseSavingsFactor,
							double changeRate, 
							double ntfsMetaDataoverHead )
{
	bool bRet = false ;
	double dNoOptimizedDuration = 0, dOptimizedDuration = 0 ;
    GetRetentionDuration( retPolicy, dNoOptimizedDuration, dOptimizedDuration ) ;
	SV_ULONGLONG	repoCapacity = 0 ,					// RepositoryCapacity 
					fileSystemCapacity = 0 ,			// RepositoryFileSystemCapacity
					fileSystemFreeSpace = 0 
					 ;// fileSystemFreeSpace 
	GetRepoCapacityAndFreeSpace ( repovolumename ,repoCapacity, repoFreeSpace);
	GetFileSystemOccupiedSizeForVolumes (totalProtectVolumes, fileSystemCapacity, fileSystemFreeSpace);

	SV_ULONGLONG totalFsOccupiedSize = fileSystemCapacity - fileSystemFreeSpace ;

	double dlfileSystemOccupiedSize = boost::lexical_cast<double>(totalFsOccupiedSize); 
	dlfileSystemOccupiedSize = dlfileSystemOccupiedSize /( 1024 * 1024 * 1024 ) ;  
	DebugPrintf ( "total size used by FileSystem is %lf \n " , dlfileSystemOccupiedSize );

	double dltotalProtectedCapacity = ( boost::lexical_cast<double>(fileSystemCapacity)) / (1024 * 1024 * 1024);
	DebugPrintf ( "totalProtectedCapacity is %lf \n " , dltotalProtectedCapacity );

	double inmageSpace = GetRepoLogsSpace ();
	SV_UINT compressVolpacks = 0;

	std::string testRepoVolName ;
	//logic to test the new repo supports sparse file or not
	bool supportSparseFile = false ;
	if ( SupportsSparseFiles(repovolumename) )
	{
		DebugPrintf(SV_LOG_DEBUG, "Sparsefileattribute supported from inside CheckSpaceAvailability check\n") ;
		supportSparseFile = true;
	}
	else
	{
		DebugPrintf(SV_LOG_DEBUG, "Sparsefileattribute not supported from inside CheckSpaceAvailability check\n") ;
	}
					
	double volPackSize = GetVolpackSizeV2 (dlfileSystemOccupiedSize,dltotalProtectedCapacity,supportSparseFile);
	DebugPrintf ("volPackSize is %lf \n ",volPackSize);
	double NTFSMetaDataOverHead = volPackSize * 13 /100 ; 
	DebugPrintf (SV_LOG_DEBUG,"NTFSMetaDataOverHead is %lf \n ",NTFSMetaDataOverHead);
	double totalSize = 0;
	double cdpSpace = GetTotalCDPSpace (dlfileSystemOccupiedSize, 
										changeRate,
										dNoOptimizedDuration,
										sparseSavingsFactor,
										dOptimizedDuration,
										compressionBenifits); 
	totalSize = TotalSpaceRequired (volPackSize,cdpSpace,NTFSMetaDataOverHead,inmageSpace); 

	requiredSpace = ( SV_ULONGLONG ) convertGBtoBytes (totalSize);

	DebugPrintf (SV_LOG_DEBUG,"requiredSpace  in Bytes is %I64d \n " , requiredSpace ); 
	DebugPrintf (SV_LOG_DEBUG,"repoFreeSpace  in Bytes is %I64d \n " , repoFreeSpace );
	if (repoFreeSpace > requiredSpace )
		bRet = true ;
	if (bRet)
		DebugPrintf (SV_LOG_DEBUG,"Space Check not failed... \n " , requiredSpace );
	else
		DebugPrintf (SV_LOG_DEBUG,"Space Check got failed... \n " , requiredSpace );
	return bRet;

}

bool bCheckSpaceAvailabilityV2 (SV_ULONGLONG repoFreeSpace,
								SV_ULONGLONG totalProtectedCapacity, 
								SV_ULONGLONG fileSystemOccupiedSize,
								SV_ULONGLONG &requiredSpace,
								double dNoOptimizedDuration,
								double dOptimizedDuration,
								SV_ULONGLONG &protectedVolumeOverHead,
								bool supportSparseFile,
								double compressionBenifits, 
								double sparseSavingsFactor,
								double changeRate, 
								double ntfsMetaDataoverHead
								)
{	
	double dlfileSystemOccupiedSize = boost::lexical_cast<double>(fileSystemOccupiedSize); 
	DebugPrintf ( "total size used by FileSystem is %lf \n " , dlfileSystemOccupiedSize );
	dlfileSystemOccupiedSize = dlfileSystemOccupiedSize /( 1024 * 1024 * 1024 ) ;  
	double dltotalProtectedCapacity = (double)totalProtectedCapacity / (1024 * 1024 * 1024);
	DebugPrintf ( "totalProtectedCapacity is %lf \n " , dltotalProtectedCapacity );
	double inmageSpace = GetRepoLogsSpace ();
	
	double anticipatedNonOverlappedIOSize = GetAnticipatedNonOverlappedIOSize (
												dNoOptimizedDuration , 
												dOptimizedDuration , 
												changeRate , 
												dlfileSystemOccupiedSize  
											);

	double volPackSize = GetVolpackSizeV2 (dlfileSystemOccupiedSize,dltotalProtectedCapacity,supportSparseFile);  
	DebugPrintf ("volPackSize is %lf \n ",volPackSize);


	double NTFSMetaDataOverHead = volPackSize * 13 /100 ; 
	double totalSize = 0;
	double cdpSpace = GetTotalCDPSpace (dlfileSystemOccupiedSize, 
										changeRate,
										dNoOptimizedDuration,
										sparseSavingsFactor,
										dOptimizedDuration,
										compressionBenifits); 
	totalSize = TotalSpaceRequired (volPackSize,cdpSpace,NTFSMetaDataOverHead,inmageSpace); 

	requiredSpace = ( SV_ULONGLONG ) convertGBtoBytes (totalSize);

	DebugPrintf (SV_LOG_DEBUG,"requiredSpace  in Bytes is %I64d \n " , requiredSpace ); 

	DebugPrintf ( SV_LOG_DEBUG , "Backup location FreeSpace : %I64d protectedVolumeOverHead %I64d requiredSpace %I64d\n ", repoFreeSpace,protectedVolumeOverHead,requiredSpace) ;

	DebugPrintf ( SV_LOG_DEBUG , "NTFSMetaDataOverHead  in Bytes is %lf \n " , NTFSMetaDataOverHead ); 

    return checkSpaceAvailability (repoFreeSpace,requiredSpace,protectedVolumeOverHead);
}

/* 
	In this version VolPack size is equal to fileSystem size 
*/ 

double GetVolpackSizeWithOutCompressionV2 (const double &dlfileSystemSizeOrProtectedCpacity)
{
	double volPackSize = (double) dlfileSystemSizeOrProtectedCpacity ; 
	return volPackSize;
}

/* 
	In this version VolPack size is equal to fileSystem size 
	We will get maximum of 20% compression benifits on volpack volumes

*/ 

double GetVolpackSizeV2 (const double &dlfileSystemSizeOrProtectedCpacity , const SV_UINT compressVolpacks)
{
	double volPackSize = GetVolpackSizeWithOutCompressionV2 (dlfileSystemSizeOrProtectedCpacity);	
	if (compressVolpacks == 1)
	{
		volPackSize =(  ( 100 - 20 ) / 100 ) * volPackSize; 	
	}
	return volPackSize; 
}

double GetCDPStoreNotOptimisedSpace (const double fileSystemOccupiedSize ,const double totalChangeRate ,const double dNoOptimizedDuration)
{
	return fileSystemOccupiedSize*totalChangeRate*dNoOptimizedDuration; 
}

double GetCDPStoreOptimisedUsage (const double fileSystemOccupiedSize,const double totalChangeRate ,const double dOptimizedDuration ,const int sparseSavingFactor)
{
	return (fileSystemOccupiedSize*totalChangeRate*dOptimizedDuration)/sparseSavingFactor;
}
double GetTotalCDPSpaceNoCompression (const double cdpStoreNotOptimisedSpace,const double cdpStoreOptimisedUsage)
{
	return cdpStoreNotOptimisedSpace + cdpStoreOptimisedUsage; 
}
double GetToatlCDPSpaceWithCompression (const double totalCDPSpaceNoCompression ,const double compressionBenifits)
{
	return 	totalCDPSpaceNoCompression - (compressionBenifits * totalCDPSpaceNoCompression ) ; 
}

double TotalSpaceRequired (const double volPackSize,const double totalCDPSpaceNoCompression,const double ntfsOverHead, const double inmageSpace)
{
	return volPackSize + totalCDPSpaceNoCompression + ntfsOverHead + inmageSpace; 
}

double TotalSpaceRequiredWithCompression (const double volPackSize,const double totalCDPSpaceNoCompression,const double ntfsOverHead, const double inmageSpace)
{
	return volPackSize + totalCDPSpaceNoCompression + ntfsOverHead + inmageSpace; 
}

void GetActionOnNewVolumes(FunctionInfo& request, std::string& actionOnNewVolumes)
{
	ConstParametersIter_t paramIter = request.m_RequestPgs.m_Params.find("NewVolumeDiscovered") ;
	if( paramIter != request.m_RequestPgs.m_Params.end() )
	{
		actionOnNewVolumes = paramIter->second.value ;
	}
	else
	{
		actionOnNewVolumes = "Notify" ;
	}
}

void isSpaceCheckRequired(FunctionInfo& request, std::string& spaceCheckRequired)
{
	ConstParametersIter_t paramIter = request.m_RequestPgs.m_Params.find("OverRideSpaceCheck") ;
	if( paramIter != request.m_RequestPgs.m_Params.end() )
	{
		spaceCheckRequired = paramIter->second.value ;
	}
	else 
	{
		spaceCheckRequired = "No"; 
	}
	return;
}

bool GetReplicationOptionsForUpdateRequest(FunctionInfo& request, ReplicationOptions& replicationOptions)
{
	bool bRet = false; 
	ParameterGroupsIter_t pgIter,autoresyncoptionsPgIter ;
	ConfSchema::Object* replicationOptionsObject ;
	ScenarioConfigPtr scenarioConfigPtr =  ScenarioConfig::GetInstance() ;
	ConfSchema::ReplicationOptionsObject m_replicationOptionsAttrNamesObj ;
	ConfSchema::AutoResyncObject m_autoresyncOptionAttrNamesObj ;
	ConfSchema::AttrsIter_t attrIter ;

	scenarioConfigPtr->GetReplicationOptionsObj(&replicationOptionsObject) ;

	pgIter = request.m_RequestPgs.m_ParamGroups.find("ReplicationOptions") ;
	if( pgIter != request.m_RequestPgs.m_ParamGroups.end() )
	{
		Parameters_t& replicationOptionsParams = pgIter->second.m_Params ;
		ConstParametersIter_t paramIter ;
		if( (paramIter = replicationOptionsParams.find( "BatchResync")) != 
			replicationOptionsParams.end() )
		{
			replicationOptions.batchSize = boost::lexical_cast<int>(paramIter->second.value) ;
		}
		else
		{
			ConfSchema::AttrsIter_t attrIter ;
			attrIter = replicationOptionsObject->m_Attrs.find( m_replicationOptionsAttrNamesObj.GetBatchResyncAttrName() ) ;
			replicationOptions.batchSize = boost::lexical_cast<int>(attrIter->second) ;
		}

		ConfSchema::GroupsIter_t grpIter ;
		grpIter = replicationOptionsObject->m_Groups.find( m_autoresyncOptionAttrNamesObj.GetName() ) ;


		autoresyncoptionsPgIter = pgIter->second.m_ParamGroups.find("AutoResyncPolicy") ;
		if( autoresyncoptionsPgIter != pgIter->second.m_ParamGroups.end() )
		{
			Parameters_t& autoResyncParams = autoresyncoptionsPgIter->second.m_Params ;
			if( (paramIter = autoResyncParams.find("Between")) !=
				autoResyncParams.end() )
			{
				replicationOptions.autoResyncOptions.between = paramIter->second.value ;
			}
			else
			{
				if( grpIter !=  replicationOptionsObject->m_Groups.end() )
				{
					ConfSchema::Group& autoResyncGrp = grpIter->second ;
					if( autoResyncGrp.m_Objects.size() )
					{
						ConfSchema::Object& autoresyncOptions = autoResyncGrp.m_Objects.begin()->second ;
						attrIter = autoresyncOptions.m_Attrs.find( m_autoresyncOptionAttrNamesObj.GetBetweenAttrName() ) ;
						replicationOptions.autoResyncOptions.between = attrIter->second ;
						bRet = true ;
					}
				}

			}

			if( (paramIter = autoResyncParams.find("WaitTime")) !=
				autoResyncParams.end() )
			{
				replicationOptions.autoResyncOptions.waitTime = boost::lexical_cast<int>(paramIter->second.value) ;
			}
			else
			{
				if( grpIter !=  replicationOptionsObject->m_Groups.end() )
				{
					ConfSchema::Group& autoResyncGrp = grpIter->second ;
					if( autoResyncGrp.m_Objects.size() )
					{
						ConfSchema::Object& autoresyncOptions = autoResyncGrp.m_Objects.begin()->second ;
						attrIter = autoresyncOptions.m_Attrs.find( m_autoresyncOptionAttrNamesObj.GetWaitTimeAttrName() ) ;
						replicationOptions.autoResyncOptions.waitTime = boost::lexical_cast<int>(attrIter ->second) ;
						bRet = true ;
					}
				}
			}
		}
		bRet = true;
	}
	return bRet;
}
bool GetReplicationOptions(FunctionInfo& request, ReplicationOptions& replicationOptions)
{
	bool bRet = false; 
	ParameterGroupsIter_t pgIter,autoresyncoptionsPgIter ;
	pgIter = request.m_RequestPgs.m_ParamGroups.find("ReplicationOptions") ;
	if( pgIter != request.m_RequestPgs.m_ParamGroups.end() )
	{
		Parameters_t& replicationOptionsParams = pgIter->second.m_Params ;
		ConstParametersIter_t paramIter ;
		if( (paramIter = replicationOptionsParams.find( "BatchResync")) != 
			replicationOptionsParams.end() )
		{
			replicationOptions.batchSize = boost::lexical_cast<int>(paramIter->second.value) ;
		}
		else
		{
			replicationOptions.batchSize = 0 ;
		}

		autoresyncoptionsPgIter = pgIter->second.m_ParamGroups.find("AutoResyncPolicy") ;
		if( autoresyncoptionsPgIter != pgIter->second.m_ParamGroups.end() )
		{
			Parameters_t& autoResyncParams = autoresyncoptionsPgIter->second.m_Params ;
			if( (paramIter = autoResyncParams.find("Between")) !=
				autoResyncParams.end() )
			{
				replicationOptions.autoResyncOptions.between = paramIter->second.value ;
			}
			else
			{
				replicationOptions.autoResyncOptions.between = "00:00-23:59" ;
			}

			if( (paramIter = autoResyncParams.find("WaitTime")) !=
				autoResyncParams.end() )
			{
				replicationOptions.autoResyncOptions.waitTime = boost::lexical_cast<int>(paramIter->second.value) ;
			}
			else
			{
				replicationOptions.autoResyncOptions.waitTime = 10 ;
			}
		}
		bRet = true;
	}
	return bRet;
}
void GetSourceTgtVolumeMap( const std::map<std::string, PairDetails>& pairDetailsMap,
						   std::map<std::string, std::string>& srcTgtVolumeMap)
{
	std::map<std::string, PairDetails>::const_iterator pairDetailIter = pairDetailsMap.begin() ;
	while( pairDetailIter != pairDetailsMap.end() )
	{
		srcTgtVolumeMap.insert(std::make_pair( pairDetailIter->first, pairDetailIter->second.targetVolume)) ;
		pairDetailIter++ ;
	}
}

std::string constructTargetVolumeFromSourceAndRepo( std::string sourceVolume, const std::string& repoVolume, const std::string& tgtHostId )
{
	std::string targetVolume, tempVolume ;
	targetVolume = repoVolume ;
	tempVolume = sourceVolume ;
	boost::algorithm::replace_all(tempVolume, ":", "_") ;
	boost::algorithm::replace_all(tempVolume, "\\", "_") ; 
	if( repoVolume.length() == 1 )
	{
		targetVolume += ":" ;
	}

	targetVolume += ACE_DIRECTORY_SEPARATOR_CHAR_A ;
	targetVolume += tgtHostId ;
	targetVolume += ACE_DIRECTORY_SEPARATOR_CHAR_A ;
	targetVolume += tempVolume ;

	return targetVolume ;
}

std::string constructRetentionPathFromSourceAndRepo( std::string sourceVolume, const std::string& repoVolume, const std::string& tgtHostId )
{
	std::string retentionPath, tempVolume ;
	retentionPath = repoVolume ;
	tempVolume = sourceVolume ;
	boost::algorithm::replace_all(tempVolume, ":", "_") ;
	boost::algorithm::replace_all(tempVolume, "\\", "_") ; 
	if( repoVolume.length() == 1 )
	{
		retentionPath += ":" ;
	}

	retentionPath += ACE_DIRECTORY_SEPARATOR_CHAR_A ;
	retentionPath += tgtHostId ;
	retentionPath += ACE_DIRECTORY_SEPARATOR_CHAR_A ;
	retentionPath += tempVolume ;
	retentionPath += ACE_DIRECTORY_SEPARATOR_CHAR_A ;
	retentionPath += "Retention" ;
	return retentionPath ;
}

void GetPairDetails( int rpoPolicy,
					RetentionPolicy& retentionPolicy,
					const std::string& repoVolume,
					const std::string& tgtHostId,
					const std::list<std::string>& protectableVolumes,
					const std::map<std::string, volumeProperties>& volumePropertiesMap,
					std::map<std::string, PairDetails>& pairDetailsMap)
{
	std::list<std::string>::const_iterator volIter = protectableVolumes.begin() ;        

	while( volIter != protectableVolumes.end() )
	{
		PairDetails pairDetail ;
		pairDetail.retentionVolume = repoVolume ;
		pairDetail.targetVolume = constructTargetVolumeFromSourceAndRepo( *volIter, repoVolume, tgtHostId ) ;
		pairDetail.retentionPath = constructRetentionPathFromSourceAndRepo( *volIter, repoVolume, tgtHostId ) ;          
		pairDetail.srcVolumeName = *volIter ;
		if( volumePropertiesMap.find( pairDetail.srcVolumeName ) != volumePropertiesMap.end() )
		{
			pairDetail.srcVolRawSize = 
				boost::lexical_cast<SV_LONGLONG>(volumePropertiesMap.find( pairDetail.srcVolumeName )->second.rawSize);
			pairDetail.srcVolCapacity = boost::lexical_cast<SV_LONGLONG>(volumePropertiesMap.find( pairDetail.srcVolumeName )->second.capacity);
		}  
		pairDetail.rpoThreshold = rpoPolicy ;//boost::lexical_cast<int>(rppolicy) ;
		pairDetail.retPolicy = retentionPolicy ;
		pairDetailsMap.insert(std::make_pair(*volIter,pairDetail)) ;
		volIter++ ;
	}   

}
INM_ERROR ProtectionHandler::CanExcludeVolumes(FunctionInfo& request, 
											   const std::list<std::string>& excludedVolumes,
											   std::string& volName)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	INM_ERROR errCode = E_SUCCESS ;
	std::list<std::string>::const_iterator excludeIter ;
	excludeIter = excludedVolumes.begin() ;
	VolumeConfigPtr volumeConfigPtr = VolumeConfig::GetInstance() ;
	AlertConfigPtr alertConfigPtr ;
	alertConfigPtr = AlertConfig::GetInstance() ;

	while( excludeIter != excludedVolumes.end() )
	{
		if( !volumeConfigPtr->IsVolumeAvailable(*excludeIter) )
		{
			errCode = E_EXCLUDED_VOLUME_DOESNT_EXIST_ON_SYSTEM ;
			break ;
		}
		if( volumeConfigPtr->IsSystemVolume(*excludeIter) )
		{
			errCode = E_SYSTEM_VOLUME_CANNOT_EXCLUDE ;
		}
		if( volumeConfigPtr->IsBootVolume(*excludeIter) )
		{
			errCode = E_BOOT_VOLUME_CANNOT_EXCLUDE ;
		}
		excludeIter++ ;
	}
	if( excludeIter != excludedVolumes.end() )
	{
		DebugPrintf(SV_LOG_ERROR, "%s cannot be excluded\n", excludeIter->c_str()) ;
		volName = *excludeIter ;
	}
	if( E_BOOT_VOLUME_CANNOT_EXCLUDE == errCode || 
		E_SYSTEM_VOLUME_CANNOT_EXCLUDE == errCode )
	{
		errCode = E_SYSTEM_RECOVERY_NOT_POSSIBLE ;
		std::string alertMsg = getErrorMessage(E_SYSTEM_RECOVERY_NOT_POSSIBLE);
		alertConfigPtr->AddAlert("ALERT", "CX", SV_ALERT_SIMPLE, SV_ALERT_MODULE_RESYNC, alertMsg) ;
	}

	return errCode ;
}


INM_ERROR ProtectionHandler::SpaceCheck(FunctionInfo& request)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	INM_ERROR errCode = E_SUCCESS ;
	std::string repoVol; 
	ParameterGroup volumesPg  ;	
	std::list<std::string> volumeListFromRequest ;
	SV_ULONGLONG requiredSpace = 0 , protectedVolumeOverHead = 0 ,repoFreeSpace = 0 ;
	GetRepositoryDeviceName(request, repoVol) ;
	RetentionPolicy retentionPolicy ;
	LocalConfigurator configurator ;
	GetRetentionPolicyFromRequest( request, retentionPolicy ) ;
	try 
	{
		std::string fp = repoVol ;
		if( fp.length() == 1 )
		{
			fp += ":" ;
		}
		if( isRepositoryConfiguredOnCIFS( fp ) )
		{
			std::string errmsg; 
			AccessRemoteShare(fp, errCode,  errmsg) ;
		}
		if( errCode == E_SUCCESS )
		{
			if (boost::filesystem::exists (fp.c_str())) 
			{
				errCode = E_SUCCESS; 
			}
			else 
			{
				errCode = E_STORAGE_PATH_NOT_FOUND;
			}
		}
	}
	catch (...)
	{
		errCode = E_STORAGE_PATH_NOT_FOUND;
	}	
	if (errCode == E_SUCCESS)
	{
		if( request.m_RequestPgs.m_ParamGroups.find("VolumeList") !=
			request.m_RequestPgs.m_ParamGroups.end() )
		{
			volumesPg = request.m_RequestPgs.m_ParamGroups.find("VolumeList")->second ;
			GetVolumeListFromPg(volumesPg, volumeListFromRequest) ;
		}
		else 
		{
			ProtectionPairConfigPtr protectionPairConfigPtr ;
			protectionPairConfigPtr->GetProtectedVolumes(volumeListFromRequest) ;
			if( volumeListFromRequest.size() == 0 )
			{
				errCode = E_NO_VOLUMES_FOUND_FOR_SYSTEM;
			}
		}
		if (errCode == E_SUCCESS )
		{
			bool compression = true ;
			std::map <std::string,hostDetails> hostDetailsMap; 
			if( request.m_RequestPgs.m_Params.find( "EnableCompression" ) != request.m_RequestPgs.m_Params.end() && 
				request.m_RequestPgs.m_Params.find( "EnableCompression" )->second.value != "Yes" )
			{
				compression = false ;
			}
			configurator.SetCDPCompression( compression ) ;
			configurator.SetVirtualVolumeCompression( false ) ;
			std::string overRideSpaceCheck ; 
			isSpaceCheckRequired (request , overRideSpaceCheck);
			if (overRideSpaceCheck != "Yes")
			{
				spaceCheckParameters spaceParameter ; 
				bool bIOParams = GetIOParams(request, spaceParameter ) ;
				double compressionBenifits = spaceParameter.compressionBenifits; 
				double sparseSavingsFactor = 3.0; 
				double changeRate = spaceParameter.changeRate ;
				double ntfsMetaDataoverHead = 0.13; 
				SV_ULONGLONG repoFreeSpace,requiredSpace ;
				/*bool bSpaceAvailabililty = SpaceCalculationV2 (	repoVol,
																volumeListFromRequest, 
																retentionPolicy , 
																requiredSpace,
																protectedVolumeOverHead , 
																repoFreeSpace ,
																hostDetailsMap , 
																compressionBenifits , 
																sparseSavingsFactor,
																changeRate ,
																ntfsMetaDataoverHead );*/
				bool bSpaceAvailabililty = CheckSpaceAvailability (	repoVol,
																	retentionPolicy ,
																	volumeListFromRequest,
																	requiredSpace,
																	repoFreeSpace,
																	compressionBenifits , 
																	sparseSavingsFactor,
																	changeRate ,
																	ntfsMetaDataoverHead  );

				if( !bSpaceAvailabililty )
				{
					errCode = E_REPO_VOL_HAS_INSUCCICIENT_STORAGE ;
					std::string msg ;
					msg += "About " ;
					msg += convertToHumanReadableFormat (requiredSpace ) ;
					msg += " is recommended, but only " ;
					msg += convertToHumanReadableFormat (repoFreeSpace) ;
					msg += " is available. " ;
					msg += "Please choose a different backup location with atleast " ;
					msg += convertToHumanReadableFormat (requiredSpace) ;
					msg += " free. " ;
					msg += "You can also try reducing the number of days that backup data is retained." ;
					//request.m_Message = FormatSpaceCheckMsg(request.m_RequestFunctionName, hostDetailsMap) ;
					request.m_Message = msg ;
				}		
			}
		}
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return 	errCode;
}

INM_ERROR ProtectionHandler::SetupBackupProtection(FunctionInfo& request)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	INM_ERROR errCode ;
#ifdef SV_WINDOWS
	WSADATA wsaData;
	int err = WSAStartup(MAKEWORD(1,1), &wsaData);
	if (err != 0) 
	{
		DebugPrintf(SV_LOG_DEBUG,"Could not find a usable WINSOCK DLL\n");
	}
#endif
	VolumeConfigPtr volumeConfigPtr = VolumeConfig::GetInstance() ;
	ScenarioConfigPtr scenarioConfigPtr =  ScenarioConfig::GetInstance() ;
	PolicyConfigPtr policyConfigPtr = PolicyConfig::GetInstance() ;
	std::string repoName ;
	std::string recoveryFeature, actionOnNewVolumes ;
	std::string srcHostId, tgtHostId, srcHostName, tgtHostName ;
	LocalConfigurator configurator ;
	srcHostId = tgtHostId = configurator.getHostId() ;
	srcHostName = tgtHostName = Host::GetInstance().GetHostName() ;
	ConsistencyOptions consistencyOptions ;
	std::map<std::string, PairDetails> pairDetailsMap ;
	ReplicationOptions replicationOptions ;
	GetRepositoryName(request, repoName) ;
	GetRecoveryFeature(request, recoveryFeature) ;
	GetActionOnNewVolumes(request, actionOnNewVolumes) ;
	GetConsistencyOptions(request, consistencyOptions) ;
	GetReplicationOptions(request, replicationOptions) ;
	spaceCheckParameters spacecheckparams ;
	GetIOParams(request,spacecheckparams) ;

	if( volumeConfigPtr->isRepositoryConfigured(repoName) )
	{
#ifdef SV_WINDOWS
		AddHostDetailsToRepositoryConf() ;
#endif
		if( volumeConfigPtr->isRepositoryAvailable(repoName) )
		{
			if( !scenarioConfigPtr->ProtectionScenarioAvailable() )
			{
				std::map<std::string, volumeProperties> volumePropertiesMap ;
				volumeConfigPtr->GetVolumeProperties(volumePropertiesMap) ;
				std::list<std::string> excludedVolumes ;
				GetExcludedVolumes(request, excludedVolumes) ;                            

				std::list<std::string> protectableVolumes ;
				std::string cannotExcludeVol ;
				errCode = CanExcludeVolumes(request, excludedVolumes, cannotExcludeVol) ;
				if( errCode == E_SUCCESS || errCode == E_SYSTEM_RECOVERY_NOT_POSSIBLE )
				{

					volumeConfigPtr->GetProtectableVolumes(excludedVolumes, protectableVolumes) ;
					//Check whether the only selected Volume for Protection is being deleted or not
					if( protectableVolumes.empty())
					{
						errCode = E_NO_VOLUME_WITH_THE_NAME ;
						return errCode;
					}
					std::list<std::list<std::string> > listOfVolList ;
					if (volumeConfigPtr->GetSameVolumeList(protectableVolumes,listOfVolList) )
					{
						std::list<std::list<std::string> >::const_iterator listOfListIter = listOfVolList.begin() ;
						if(listOfListIter != listOfVolList.end())
						{
							std::list<std::string>::const_iterator listIter = (*listOfListIter).begin() ;
							request.m_Message += (*listIter) + " is accessible using " ;
							while (listIter != (*listOfListIter).end())
							{
								request.m_Message += (*listIter) + ", " ;
								listIter++ ;
							}
							request.m_Message += " . " ;
						}
						errCode = E_VOLUME_HAVING_MULTIPLE_ACCESS_POINTS ;
						return errCode ;
					}
					std::string repoVol ;
					volumeConfigPtr->GetRepositoryVolume(repoName, repoVol) ;
#ifdef SV_WINDOWS
					std::string volume ;
					std::string diskname ;
					if( DoesDiskContainsProtectedVolumes( repoVol, protectableVolumes, diskname, volume ) )
					{
						errCode = E_SAMEREPOSITORY_PROTECTEDVOL_DISK ;
						request.m_Message = "The chosen backup location " ;
						request.m_Message += repoVol ;
						request.m_Message += " and volume " ;
						request.m_Message += volume ;
						if( "__INMAGE__DYNAMIC__DG__" != diskname )
						{
							request.m_Message += " are on same disk ";
							request.m_Message += diskname;
						}
						else
						{
							request.m_Message += " are on same dynamic disk group ";
						}

						
					}
#endif
					SV_ULONGLONG requiredSpace = 0 ,protectedVolumeOverHead = 0;
					VolumeConfigPtr volumeConf = VolumeConfig::GetInstance();
					SV_ULONGLONG repoFreeSpace = 0;
					std::string rppolicy ; 
					GetRPOPolicy( request, rppolicy ) ;
					RetentionPolicy retentionPolicy ;
					GetRetentionPolicyFromRequest( request, retentionPolicy ) ;
					GetPairDetails(boost::lexical_cast<int>(rppolicy), retentionPolicy, repoVol, tgtHostId, protectableVolumes, volumePropertiesMap, pairDetailsMap) ;
					std::map <std::string,hostDetails> hostDetailsMap; 
					std::string overRideSpaceCheck ; 
					isSpaceCheckRequired (request , overRideSpaceCheck );
					if (overRideSpaceCheck != "Yes")
					{
						double compressionBenifits = spacecheckparams.compressionBenifits; 
						double sparseSavingsFactor = 3.0; 
						double changeRate = spacecheckparams.changeRate ;
						double ntfsMetaDataoverHead = 0.13; 


						SV_ULONGLONG repoFreeSpace,requiredSpace ;
				/*bool bSpaceAvailabililty = SpaceCalculationV2 (	repoVol,
																protectableVolumes, 
																retentionPolicy , 
																requiredSpace,
																protectedVolumeOverHead , 
																repoFreeSpace ,
																hostDetailsMap , 
																compressionBenifits , 
																sparseSavingsFactor,
																changeRate ,
																ntfsMetaDataoverHead );*/
				bool bSpaceAvailabililty = CheckSpaceAvailability (	repoVol,
																	retentionPolicy ,
																	protectableVolumes,
																	requiredSpace,
																	repoFreeSpace,
																	compressionBenifits , 
																	sparseSavingsFactor,
																	changeRate ,
																	ntfsMetaDataoverHead  );

				if( !bSpaceAvailabililty )
				{
					errCode = E_REPO_VOL_HAS_INSUCCICIENT_STORAGE ;
					std::string msg ;
					msg += "About " ;
					msg += convertToHumanReadableFormat (requiredSpace ) ;
					msg += " is recommended, but only " ;
					msg += convertToHumanReadableFormat (repoFreeSpace) ;
					msg += " is available. " ;
					msg += "Please choose a different backup location with atleast " ;
					msg += convertToHumanReadableFormat (requiredSpace) ;
					msg += " free. " ;
					msg += "You can also try reducing the number of days that backup data is retained." ;
					//request.m_Message = FormatSpaceCheckMsg(request.m_RequestFunctionName, hostDetailsMap) ;
					request.m_Message = msg ;


						}
					}
					scenarioConfigPtr =  ScenarioConfig::GetInstance() ;
					policyConfigPtr = PolicyConfig::GetInstance() ;

					if( errCode == E_SUCCESS || errCode == E_SYSTEM_RECOVERY_NOT_POSSIBLE )
					{                        
						std::map<std::string, std::string> srcTgtVolumeMap ;
						GetSourceTgtVolumeMap(pairDetailsMap, srcTgtVolumeMap) ;

						scenarioConfigPtr->CreateProtectionScenario(recoveryFeature,
							actionOnNewVolumes,
							srcHostId,
							tgtHostId,
							srcHostName,
							tgtHostName,
							repoName,
							excludedVolumes,
							consistencyOptions,
							pairDetailsMap,
							replicationOptions) ;
						int scenarioId = scenarioConfigPtr->GetProtectionScenarioId() ;
						policyConfigPtr->CreateConsistencyPolicy(consistencyOptions, protectableVolumes, scenarioId, true, false) ;
						policyConfigPtr->CreateVolumeProvisioningPolicy(srcTgtVolumeMap, volumePropertiesMap, scenarioId) ;
						EventQueueConfigPtr eventqconfigPtr = EventQueueConfig::GetInstance() ;
						eventqconfigPtr->AddPendingEvent( request.m_RequestFunctionName ) ;
						scenarioConfigPtr->UpdateIOParameters(spacecheckparams) ;
						ConfSchemaReaderWriterPtr confschemareaderwriterPtr ;
						confschemareaderwriterPtr = ConfSchemaReaderWriter::GetInstance() ;
						confschemareaderwriterPtr->Sync() ;
					}
				}
			}
			else
			{
				errCode = E_HOST_UNDER_PRETECTION ;
			}
		}
		else
		{
			errCode = E_REPO_CREATION_INPROGRESS ;
		}
	}
	else
	{
		errCode = E_NO_REPO_WITH_THE_NAME ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return errCode;
}

void ProtectionHandler::GetVolumeListFromPg(const ParameterGroup& pg, 
											std::list<std::string>& volumeList)
{
	ConstParametersIter_t paramIter ;
	paramIter = pg.m_Params.begin() ;
	while( paramIter != pg.m_Params.end() )
	{
		if( std::find( volumeList.begin(), volumeList.end(), paramIter->second.value ) ==
			volumeList.end() )
		{
			volumeList.push_back( paramIter->second.value ) ;
		}
		paramIter ++ ;
	}
}

INM_ERROR ProtectionHandler::PauseTracking(const std::list<std::string>& volumesList,
										   bool directPause,
										   FunctionInfo& request)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	ProtectionPairConfigPtr protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
	VolumeConfigPtr volumeConfigPtr = VolumeConfig::GetInstance() ;
	std::list<std::string> protectedVolumes ;
	std::list<std::string> volumesOnSystem ;
	protectionPairConfigPtr->GetProtectedVolumes(protectedVolumes) ;
	volumeConfigPtr->GetGeneralVolumes(volumesOnSystem) ;
	bool requireSync = false ;
	std::list<std::string>::const_iterator volIter ;
	volIter = volumesList.begin() ;
	while( volIter != volumesList.end() )
	{
		INM_ERROR volError = E_UNKNOWN ;
		std::string volumeMsg ;
		ParameterGroup volumePg ;
		if( std::find( volumesOnSystem.begin(), volumesOnSystem.end(), *volIter ) != 
			volumesOnSystem.end() )
		{
			if( std::find( protectedVolumes.begin(), protectedVolumes.end(), *volIter ) !=
				protectedVolumes.end() )
			{
				if( protectionPairConfigPtr->IsPauseTrackingAllowed(*volIter) )
				{
					protectionPairConfigPtr->PauseTracking(*volIter, directPause) ;
					requireSync = true ;
					volError = E_SUCCESS ;
				}
				else
				{
					VOLUME_SETTINGS::PAIR_STATE pairState ;

					protectionPairConfigPtr->GetPairState(*volIter, pairState ) ;
					volError = GetErrorCodeFromPairState( pairState ) ;
				}
			}
			else
			{
				volError = E_VOLUME_NOT_PROTECTED ;
				//volumeMsg = "Volume is not protected" ;
			}
		}
		else
		{
			volError = E_NO_VOLUME_WITH_THE_NAME ;
			//volumeMsg = "Volume is not available on the system" ;
		}
		GetVolumeLevelPg( *volIter, volError, volumeMsg, volumePg) ;
		request.m_ResponsePgs.m_ParamGroups.insert(std::make_pair( *volIter, volumePg)) ;
		volIter++ ;
	}
	if( requireSync )
	{
		EventQueueConfigPtr eventqconfigPtr = EventQueueConfig::GetInstance() ;
		eventqconfigPtr->AddPendingEvent( request.m_RequestFunctionName ) ;
		ConfSchemaReaderWriterPtr confschemaRdrWrtr = ConfSchemaReaderWriter::GetInstance() ;
		confschemaRdrWrtr->Sync() ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return E_SUCCESS ;
}

INM_ERROR ProtectionHandler::PauseTracking(FunctionInfo& request)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	INM_ERROR errCode = E_UNKNOWN ;
	std::list<std::string> protectedVolumes ;
	ProtectionPairConfigPtr protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
	ScenarioConfigPtr scenarioConfigPtr ;
	scenarioConfigPtr = ScenarioConfig::GetInstance() ;
	if( scenarioConfigPtr->ProtectionScenarioAvailable() )
	{
		protectionPairConfigPtr->GetProtectedVolumes(protectedVolumes) ;

		errCode = E_SUCCESS ;
		PauseTracking(protectedVolumes,true, request) ;
	}
	else
	{
		errCode = E_NO_PROTECTIONS ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return errCode ;
}

INM_ERROR ProtectionHandler::PauseTrackingForVolume(FunctionInfo& request)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	INM_ERROR errCode = E_UNKNOWN ;
	ParameterGroup volumesPg  ;
	if( request.m_RequestPgs.m_ParamGroups.find("VolumeList") !=
		request.m_RequestPgs.m_ParamGroups.end() )
	{
		volumesPg = request.m_RequestPgs.m_ParamGroups.find("VolumeList")->second ;
	}
	std::list<std::string>volumeslist ;
	GetVolumeListFromPg(volumesPg, volumeslist) ;
	if( volumeslist.empty() == false )
	{
		bool directPause = false ;
		ParametersIter_t paramIter ;
		paramIter = request.m_RequestPgs.m_Params.find( "PauseDirectly" ) ;
		if( paramIter != request.m_RequestPgs.m_Params.end() )
		{
			directPause = boost::lexical_cast<bool>(paramIter->second.value) ;
		}

		ScenarioConfigPtr scenarioConfigPtr = ScenarioConfig::GetInstance() ;
		if( scenarioConfigPtr->ProtectionScenarioAvailable() )
		{
			errCode = E_SUCCESS ;
			PauseTracking(volumeslist, directPause, request) ;
		}
		else
		{
			errCode = E_NO_PROTECTIONS ;
		}
	}
	else
	{
		errCode = E_INSUFFICIENT_PARAMS ;
	}

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return errCode ;
}

INM_ERROR ProtectionHandler::PauseProtection(const std::list<std::string>& volumes,
											 FunctionInfo& request)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	INM_ERROR errCode = E_SUCCESS ;
	bool requireSync, allPaused;
	requireSync = false ;
	allPaused = true ;
	std::string pauseReason ;

	ParametersIter_t paramIter  ;
	paramIter = request.m_RequestPgs.m_Params.find( "Pause Reason" ) ;
	if( paramIter != request.m_RequestPgs.m_Params.end() )
	{
		pauseReason = paramIter->second.value ;
	}
	bool directPause = false ;
	paramIter = request.m_RequestPgs.m_Params.find( "PauseDirectly" ) ;
	if( paramIter != request.m_RequestPgs.m_Params.end() )
	{
		directPause = boost::lexical_cast<bool>(paramIter->second.value) ;
	}
	ProtectionPairConfigPtr protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
	VolumeConfigPtr volumeConfigPtr = VolumeConfig::GetInstance() ;
	std::list<std::string> volumesOnSystem, protectedVolumes ;
	protectionPairConfigPtr->GetProtectedVolumes( protectedVolumes ) ;
	volumeConfigPtr->GetGeneralVolumes( volumesOnSystem ) ;
	std::list<std::string>::const_iterator volIter = volumes.begin() ;
	if( protectedVolumes.empty() )
		errCode = E_NO_PROTECTIONS ;
	else
	{
		while( volIter != volumes.end() )
		{
			INM_ERROR volError = E_UNKNOWN ;
			std::string volumeMsg ;
			ParameterGroup volumePg ;
			if( std::find( volumesOnSystem.begin(), volumesOnSystem.end(), *volIter ) != 
				volumesOnSystem.end() )
			{
				if( std::find( protectedVolumes.begin(), protectedVolumes.end() , *volIter ) !=
					protectedVolumes.end() )
				{
					if( protectionPairConfigPtr->IsPauseAllowed(*volIter) )
					{
						allPaused = false ;
						protectionPairConfigPtr->PauseProtection(*volIter, directPause, pauseReason) ;
						requireSync = true ;
						volError = E_SUCCESS ;
					}
					else
					{
						ConfSchema::Object* protectionPairObj = NULL ;
						protectionPairConfigPtr->GetProtectionPairObjBySrcDevName( *volIter, &protectionPairObj ) ;
						VOLUME_SETTINGS::PAIR_STATE pairState ;
						protectionPairConfigPtr->GetPairState( protectionPairObj, pairState ) ;
						if( protectionPairConfigPtr->IsPairInQueuedState( protectionPairObj) && protectionPairConfigPtr->IsPairInStepOne( protectionPairObj ) )
							volError = E_QUEUED_VOLUME_NOT_PAUSED ;
						else
							volError = GetErrorCodeFromPairState( pairState ) ;
					}
				}
				else
				{
					volError = E_VOLUME_NOT_PROTECTED ;
				}
			}
			else
			{
				volError = E_NO_VOLUME_WITH_THE_NAME ;
			}
			GetVolumeLevelPg( *volIter, volError, volumeMsg, volumePg) ;
			request.m_ResponsePgs.m_ParamGroups.insert(std::make_pair( *volIter, volumePg)) ;

			volIter++ ;
		}
		if( allPaused )
		{
			errCode  = E_SYSTEM_ALREADY_PAUSED ;
		}
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return errCode ;
}


INM_ERROR ProtectionHandler::PauseProtection(FunctionInfo& f)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	INM_ERROR errCode = E_UNKNOWN ;
	ProtectionPairConfigPtr protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
	ScenarioConfigPtr scenarioConfigPtr = ScenarioConfig::GetInstance() ;
	if( scenarioConfigPtr->ProtectionScenarioAvailable() )
	{
		errCode = E_SUCCESS ;
		std::list<std::string> protectedVolumes ;
		protectionPairConfigPtr->GetProtectedVolumes(protectedVolumes) ;
		errCode = PauseProtection( protectedVolumes, f) ;
	}
	else
	{
		errCode = E_NO_PROTECTIONS ;
	}
	ConstParametersIter_t paramIter = f.m_RequestPgs.m_Params.find( "Pause Reason" ) ;
	std::string pausereason ;
	if( paramIter!= f.m_RequestPgs.m_Params.end() )
	{
		scenarioConfigPtr->SetSystemPausedReason(f.m_RequestPgs.m_Params.find( "Pause Reason" )->second.value) ; 
	}
	std::string systemPauseState = "1";
	scenarioConfigPtr->SetSystemPausedState(systemPauseState) ; 
	EventQueueConfigPtr eventqconfigPtr = EventQueueConfig::GetInstance() ;
	eventqconfigPtr->AddPendingEvent( f.m_RequestFunctionName ) ;

	ConfSchemaReaderWriterPtr confschemareaderWriterPtr ;
	confschemareaderWriterPtr = ConfSchemaReaderWriter::GetInstance() ;
	confschemareaderWriterPtr->Sync() ;

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return errCode ;
}

INM_ERROR ProtectionHandler::PauseProtectionForVolume(FunctionInfo& request)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	INM_ERROR errCode = E_UNKNOWN ;
	ProtectionPairConfigPtr protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
	ScenarioConfigPtr scenarioConfigPtr = ScenarioConfig::GetInstance() ;
	if( scenarioConfigPtr->ProtectionScenarioAvailable() )
	{
		ParameterGroup volumePg ;
		ConstParameterGroupsIter_t pgIter ;
		pgIter = request.m_RequestPgs.m_ParamGroups.find( "VolumeList" ) ;
		if( pgIter != request.m_RequestPgs.m_ParamGroups.end() )
		{
			errCode = E_SUCCESS ;
			volumePg = pgIter->second ;
			std::list<std::string>volumeslist ;
			GetVolumeListFromPg(volumePg, volumeslist) ;
			PauseProtection( volumeslist, request) ;
		}
	}
	else
	{
		errCode = E_NO_PROTECTIONS ;
	}
	EventQueueConfigPtr eventqconfigPtr = EventQueueConfig::GetInstance() ;
	eventqconfigPtr->AddPendingEvent( request.m_RequestFunctionName ) ;

	ConfSchemaReaderWriterPtr confschemareaderWriterPtr ;
	confschemareaderWriterPtr = ConfSchemaReaderWriter::GetInstance() ;
	confschemareaderWriterPtr->Sync() ;

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return errCode ;
}

INM_ERROR ProtectionHandler::ResumeTracking(const std::list<std::string>& volumes,
											bool isRescueMode, 
											FunctionInfo& request)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	ProtectionPairConfigPtr protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
	ScenarioConfigPtr scenarioConfigPtr = ScenarioConfig::GetInstance() ;
	VolumeConfigPtr volumeConfigPtr = VolumeConfig::GetInstance() ;
	bool requireSync = false ;
	std::list<std::string> volumesOnSystem, protectedVolumes ;
	volumeConfigPtr->GetGeneralVolumes( volumesOnSystem ) ;
	protectionPairConfigPtr->GetProtectedVolumes( protectedVolumes ) ;
	SV_UINT batchSize = scenarioConfigPtr->GetBatchSize() ;
	std::list<std::string>::const_iterator volIter = volumes.begin() ;
	while( volIter != volumes.end() )
	{
		INM_ERROR volError = E_UNKNOWN ;
		std::string volumeMsg ;
		ParameterGroup volumePg ;
		if( std::find( volumesOnSystem.begin(), volumesOnSystem.end(), *volIter ) !=
			volumesOnSystem.end() )
		{
			if( std::find( protectedVolumes.begin(), protectedVolumes.end(), *volIter) != 
				protectedVolumes.end() )
			{
				if( protectionPairConfigPtr->IsResumeTrackingAllowed( *volIter ) )
				{
					requireSync = true ;
					protectionPairConfigPtr->ResumeTracking(*volIter, batchSize, isRescueMode) ;
					volError = E_SUCCESS ;
				}
				else
				{
					VOLUME_SETTINGS::PAIR_STATE pairState ;

					protectionPairConfigPtr->GetPairState( *volIter, pairState ) ;

					volError = GetErrorCodeFromPairState( pairState ) ;
				}
			}
			else
			{
				volError = E_VOLUME_NOT_PROTECTED  ;
			}
		}
		else
		{
			volError = E_NO_VOLUME_WITH_THE_NAME ;
		}
		GetVolumeLevelPg( *volIter, volError, volumeMsg, volumePg) ;
		request.m_ResponsePgs.m_ParamGroups.insert(std::make_pair( *volIter, volumePg)) ;
		volIter++ ;
	}
			std::string systemPaused;
			if ( scenarioConfigPtr ->GetSystemPausedState(systemPaused) )
				if ((boost::lexical_cast <int> (systemPaused)) == 1 )
				{
					systemPaused = "0" ;
					scenarioConfigPtr -> SetSystemPausedState(systemPaused);
				}
	if( requireSync )
	{
		EventQueueConfigPtr eventqconfigPtr = EventQueueConfig::GetInstance() ;
		eventqconfigPtr->AddPendingEvent( request.m_RequestFunctionName ) ;

		ConfSchemaReaderWriterPtr confschemareaderwriterptr = ConfSchemaReaderWriter::GetInstance() ;
		confschemareaderwriterptr->Sync() ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return E_SUCCESS ;
}

INM_ERROR ProtectionHandler::ResumeTracking(FunctionInfo& request)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	INM_ERROR errCode = E_UNKNOWN ;
	ProtectionPairConfigPtr protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
	ScenarioConfigPtr scenarioConfigPtr = ScenarioConfig::GetInstance() ;
	if( scenarioConfigPtr->ProtectionScenarioAvailable() )
	{
		errCode = E_SUCCESS ;
		std::list<std::string> protectedVolumes ;
		protectionPairConfigPtr->GetProtectedVolumes(protectedVolumes) ;
		ResumeTracking( protectedVolumes, false, request) ;
	}
	else
	{
		errCode = E_NO_PROTECTIONS ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return errCode ;
}
INM_ERROR ProtectionHandler::ResumeTrackingForVolume(FunctionInfo& request)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	INM_ERROR errCode = E_UNKNOWN ;
	ScenarioConfigPtr scenarioConfigPtr = ScenarioConfig::GetInstance() ;
	ProtectionPairConfigPtr protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
	if( scenarioConfigPtr->ProtectionScenarioAvailable() )
	{
		errCode = E_SUCCESS ;
		ParameterGroup volumePg ;
		ConstParameterGroupsIter_t pgIter ;
		bool isRescueMode = false ;
		ConstParametersIter_t paramIter = request.m_RequestPgs.m_Params.find("RecoveryMode") ;

		if( paramIter != request.m_RequestPgs.m_Params.end() )
		{
			if( paramIter->second.value.compare("Rescue") == 0 )
			{
				isRescueMode = true ;
			}        
		}
		pgIter = request.m_RequestPgs.m_ParamGroups.find( "VolumeList" ) ;
		if( pgIter != request.m_RequestPgs.m_ParamGroups.end() )
		{
			errCode = E_SUCCESS ;
			volumePg = pgIter->second ;
			std::list<std::string>volumeslist ;
			GetVolumeListFromPg(volumePg, volumeslist) ;
			ResumeTracking( volumeslist, isRescueMode, request) ;
		}
	}
	else
	{
		errCode = E_NO_PROTECTIONS ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return errCode ;
}

INM_ERROR ProtectionHandler::ResumeProtection(const std::list<std::string>& volumes,
											  FunctionInfo& request)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	INM_ERROR errCode = E_SUCCESS ;
	VolumeConfigPtr volumeConfigPtr = VolumeConfig::GetInstance() ;
	ScenarioConfigPtr scenarioConfigPtr = ScenarioConfig::GetInstance() ;
	ProtectionPairConfigPtr protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
	bool requireSync = false ;
	bool bSystemProtectionIntact = true ;
	std::list<std::string> protectedVolumes, volumesOnSystem ;
	protectionPairConfigPtr->GetProtectedVolumes( protectedVolumes ) ;
	volumeConfigPtr->GetGeneralVolumes( volumesOnSystem ) ;

	std::list<std::string>::const_iterator volIter = volumes.begin() ;
	if( protectedVolumes.empty() )
		errCode = E_NO_PROTECTIONS ;
	else
	{
		while( volIter != volumes.end() )
		{
			ParameterGroup volumePg ;
			std::string volumeMsg ;
			INM_ERROR volError = E_UNKNOWN ;

			if( std::find( volumesOnSystem.begin(), volumesOnSystem.end(), *volIter ) != 
				volumesOnSystem.end() )
			{
				if( std::find( protectedVolumes.begin(), protectedVolumes.end(), *volIter ) != 
					protectedVolumes.end() )
				{
					if( protectionPairConfigPtr->IsResumeAllowed(*volIter) )
					{
						volError = E_SUCCESS ;
						protectionPairConfigPtr->ResumeProtection( *volIter ) ;
						requireSync = true ;
						bSystemProtectionIntact = false ;
					}
					else
					{
						VOLUME_SETTINGS::PAIR_STATE pairState ;

						protectionPairConfigPtr->GetPairState( *volIter, pairState ) ;
						if (protectionPairConfigPtr->IsPairMarkedAsResized(*volIter) ) 
						{
							volError = 	E_VOLUME_RESIZED ;
						}
						else 
						{
							volError = GetErrorCodeFromPairState( pairState ) ;
						}
					}
				}
				else
				{
					volError = E_VOLUME_NOT_PROTECTED  ;
				}
			}
			else
			{
				volError = E_NO_VOLUME_WITH_THE_NAME ;
			}
			GetVolumeLevelPg( *volIter, volError, volumeMsg, volumePg) ;
			request.m_ResponsePgs.m_ParamGroups.insert(std::make_pair( *volIter, volumePg)) ;
			volIter++ ;
		}
	}
	if( errCode == E_SUCCESS && bSystemProtectionIntact == true )
		errCode = E_SYSTEM_PROTECTION_INTACT ;

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return errCode ;
}


INM_ERROR ProtectionHandler::ResumeProtection(FunctionInfo& f)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	INM_ERROR errCode = E_UNKNOWN ;
	ScenarioConfigPtr scenarioConfigPtr = ScenarioConfig::GetInstance() ;
	ProtectionPairConfigPtr protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
	if( scenarioConfigPtr->ProtectionScenarioAvailable() )
	{
		errCode = E_SUCCESS ;
		std::list<std::string> protectedVolumes ;
		protectionPairConfigPtr->GetProtectedVolumes( protectedVolumes ) ;
		errCode = ResumeProtection( protectedVolumes, f) ;
	}
	else
	{
		errCode = E_NO_PROTECTIONS ;
	}
	std::string systemPauseState = "0" ;
	scenarioConfigPtr->SetSystemPausedState(systemPauseState) ;
	std::string pausereason = "" ;
	scenarioConfigPtr->SetSystemPausedReason(pausereason) ;
	EventQueueConfigPtr eventqconfigPtr = EventQueueConfig::GetInstance() ;
	eventqconfigPtr->AddPendingEvent( f.m_RequestFunctionName ) ;

	ConfSchemaReaderWriterPtr confschemareaderwriterPtr ;
	confschemareaderwriterPtr = ConfSchemaReaderWriter::GetInstance() ;
	confschemareaderwriterPtr->Sync() ;

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return errCode ;
}

INM_ERROR ProtectionHandler::ResumeProtectionForVolume(FunctionInfo& request)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
	INM_ERROR errCode = E_UNKNOWN ;
	ScenarioConfigPtr scenarioConfigPtr = ScenarioConfig::GetInstance() ;
	if( scenarioConfigPtr->ProtectionScenarioAvailable() )
	{
		errCode = E_SUCCESS ;
		ParameterGroup volumePg ;
		ConstParameterGroupsIter_t pgIter ;
		pgIter = request.m_RequestPgs.m_ParamGroups.find( "VolumeList" ) ;
		if( pgIter != request.m_RequestPgs.m_ParamGroups.end() )
		{
			volumePg = pgIter->second ;
			std::list<std::string>volumeslist ;
			GetVolumeListFromPg(volumePg, volumeslist) ;
			ResumeProtection( volumeslist, request) ;
			std::string systemPaused;
			if ( scenarioConfigPtr ->GetSystemPausedState(systemPaused) )
				if ((boost::lexical_cast <int> (systemPaused)) == 1 )
				{
					systemPaused = "0" ;
					scenarioConfigPtr -> SetSystemPausedState(systemPaused);
				}
		}
	}
	else
	{
		errCode = E_NO_PROTECTIONS ;
	}
	EventQueueConfigPtr eventqconfigPtr = EventQueueConfig::GetInstance() ;
	eventqconfigPtr->AddPendingEvent( request.m_RequestFunctionName ) ;

	ConfSchemaReaderWriterPtr confschemareaderwriterPtr ;
	confschemareaderwriterPtr = ConfSchemaReaderWriter::GetInstance() ;
	confschemareaderwriterPtr->Sync() ;

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return errCode ;
}
void ProtectionHandler::getVolumesForDeletion(FunctionInfo& request , std::list<std::string> &volumeList)
{
	ScenarioConfigPtr scenarioConfigPtr = ScenarioConfig::GetInstance() ;
	ParameterGroupsIter_t paramGroupIter = request.m_RequestPgs.m_ParamGroups.find("VolumeList");
	if ( paramGroupIter != request.m_RequestPgs.m_ParamGroups.end()) 
	{
		GetVolumeListFromPg( paramGroupIter->second, volumeList) ;
	}
	else 
	{
		scenarioConfigPtr->GetProtectedVolumes(volumeList);
	}
	return;
}

INM_ERROR ProtectionHandler::IsDeletionAllowed( std::list <std::string> &volumeList )
{
	INM_ERROR errCode = E_SUCCESS ;
	std::string executionStatus;
	ScenarioConfigPtr scenarioConfigPtr = ScenarioConfig::GetInstance() ;
	ProtectionPairConfigPtr protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
	scenarioConfigPtr->GetExecutionStatus( executionStatus );
	if( InmStrCmp<NoCaseCharCmp>( executionStatus.c_str(), SCENARIO_DELETION_PENDING) == 0 )
	{
		errCode = E_DELETE_PROTECTION_INPROGRESS ;
	}
	else if( InmStrCmp<NoCaseCharCmp>( executionStatus.c_str(), "Recovery in progress") == 0 )
	{
		errCode = E_RECOVERY_INPROGRESS ;
	}
	else if (scenarioConfigPtr->GetTargetVolumeProvisioningStatus (volumeList) != true  )
	{
		errCode = E_ADD_VOLUME_PROTECTION_INPROGRESS;
	}
	if (errCode == E_ADD_VOLUME_PROTECTION_INPROGRESS || errCode == E_SUCCESS)
	{
		std::list<std::string> protectedVolumes ;
		scenarioConfigPtr->GetProtectedVolumes(protectedVolumes) ;
		std::list<std::string>::const_iterator volIter = volumeList.begin() ;
		VolumeConfigPtr volumeConfigPtr = VolumeConfig::GetInstance();

		while( volIter != volumeList.end() )
		{
			if( std::find( protectedVolumes.begin(), protectedVolumes.end(), *volIter) == 
				protectedVolumes.end() )
			{
				errCode = E_VOLUME_NOT_PROTECTED ;
				break ;
			}
			if( volIter != volumeList.end() )
			{
				if( protectionPairConfigPtr->IsDeletionInProgress( *volIter ) )
				{
					errCode = E_DELETE_PROTECTION_INPROGRESS ;
					break ;
				}
			}
			volIter++ ;
		}
	}	
	return errCode ;
}
void ProtectionHandler::DeleteVolpackEntry (std::string &volumeName)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
	LocalConfigurator configurator ;
	char regName[26];
	int counter = configurator.getVirtualVolumesId();
	std::string lowerCaseVolumeName = volumeName; 
	boost::algorithm::to_lower ( lowerCaseVolumeName ) ; 
	std::string sparseFile = lowerCaseVolumeName  + "_sparsefile";
	//use the function InmStrCmp<NoCaseCharCmp> rather than converting to lower case etc..

	while(counter != 0)
	{
		inm_sprintf_s(regName, ARRAYSIZE(regName), "VirVolume%d", counter);
		std::string virtualVolumePath = configurator.getVirtualVolumesPath(regName);
		DebugPrintf (SV_LOG_DEBUG," virtualVolumePath is  %s \n ", virtualVolumePath.c_str()  ); 
		if (virtualVolumePath.find(sparseFile) != std::string::npos)
		{
			configurator.setVirtualVolumesPath(regName,"");
			break; 
		}
		counter--;
	}
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
	return; 
}

bool ProtectionHandler::DeleteSparseFiles (std::string &volumeName , std::string &errMsg)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	std::string sparsefilePath;
	std::string configStorePath =  getConfigStorePath(); 
	sparsefilePath = configStorePath ; 
	std::string srcVolume = volumeName;
	if (srcVolume.length() == 1)
	{
		sparsefilePath += srcVolume ; 
	}
	else 
	{
		srcVolume.replace(1,2,"__");
		sparsefilePath += srcVolume;
	}
	sparsefilePath += "_sparsefile"; 
	sparsefilePath += ".filepart";
	SV_UINT  index = 0;
	bool proceed = false ;
	try 
	{
		while (1)
		{
			std::string sparsefile = sparsefilePath ; 
			sparsefile += boost::lexical_cast <std::string> ( index ) ; 
			DebugPrintf (SV_LOG_DEBUG, "sparsefile is %s \n ",sparsefile.c_str()); 
			if (boost::filesystem::exists(sparsefile.c_str())) 
			{
				if ( 0 == ACE_OS::unlink(getLongPathName( sparsefile.c_str()).c_str() ) )
				{
					DebugPrintf (SV_LOG_DEBUG, "sparsefile Deleted Successfully %s \n ",sparsefile.c_str());
					index++ ; 
				}
				else 
				{
					DebugPrintf (SV_LOG_ERROR, "Unable to delete the sparse file %s. error %ld\n ",sparsefile.c_str(),ACE_OS::last_error()) ;
#ifdef SV_WINDOWS
					
					std::string msg = GetErrorMessage (ACE_OS::last_error());  
					errMsg = msg.c_str(); 
#endif

					proceed = false; 
					break;
				}
			}
			else 
			{
				proceed = true ;
				DebugPrintf(SV_LOG_DEBUG, "%s doesn't exist\n", sparsefile.c_str()) ;
				break;
			}
		}
	}
	catch (boost::filesystem::filesystem_error &e)
	{
		DebugPrintf (SV_LOG_ERROR, "file system error : %s \n", e.what ());  
		errMsg = e.what();
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return proceed; 
}

INM_ERROR ProtectionHandler::DeleteBackupProtection(FunctionInfo& request)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	INM_ERROR errCode = E_UNKNOWN ;
	ScenarioConfigPtr scenarioConfigPtr = ScenarioConfig::GetInstance() ;
	PolicyConfigPtr policyConfigPtr = PolicyConfig::GetInstance() ;
	ProtectionPairConfigPtr protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
	VolumeConfigPtr volumeConfigPtr = VolumeConfig::GetInstance() ;
	std::string executionStatus ;
	std::list <std::string> deleteVolumeList;
	std::list<std::string> protectedVolumes ;
	if( scenarioConfigPtr->ProtectionScenarioAvailable() )
	{
		protectionPairConfigPtr->GetProtectedVolumes(protectedVolumes) ;
		getVolumesForDeletion(request,deleteVolumeList);
		if ( ( errCode = IsDeletionAllowed (deleteVolumeList)) == E_SUCCESS ) 
		{
			errCode = E_SUCCESS ;
			std::map<std::string, std::string> srcTgtVolMap ;
			std::map<std::string, volumeProperties> volumePropertiesMap ;
			volumeConfigPtr->GetVolumeProperties( volumePropertiesMap );
			int scenarioId = scenarioConfigPtr->GetProtectionScenarioId() ;
			std::list<std::string>::iterator deleteVolumeListIter; 
			deleteVolumeListIter = deleteVolumeList.begin(); 
			std::list <std::string> backupRepositoriesVolumes; 
			scenarioConfigPtr->GetProtectedVolumes (backupRepositoriesVolumes);
			bool deleteallvolumes = false; 
			if (backupRepositoriesVolumes.size() == deleteVolumeList.size())
			{
				deleteallvolumes = true; 
			}
			while (deleteVolumeListIter != deleteVolumeList.end())
			{
				std::string volumeProvisioningStatus; 
				std::string volumeName = deleteVolumeListIter->c_str(); 
				scenarioConfigPtr->GetVolumeProvisioningStatus (volumeName,volumeProvisioningStatus);
				if (volumeProvisioningStatus == VOL_PACK_FAILED)
				{
					std::string errMsg; 
					if ( DeleteSparseFiles (volumeName, errMsg) ) 
					{
						DeleteVolpackEntry (volumeName); 
						scenarioConfigPtr->removePairs(volumeName); 
						scenarioConfigPtr->removevolumeFromRepo (volumeName); 
					}
					else 
					{
						request.m_Message = errMsg ; 
						errCode = E_INTERNAL ;
					}
				}
				else if (volumeProvisioningStatus == VOL_PACK_SUCCESS)
				{
					scenarioConfigPtr->GetSourceTargetVoumeMapping( volumeName, srcTgtVolMap) ;
					protectionPairConfigPtr->MarkPairsForDeletionPending(volumeName) ;
				}
				deleteVolumeListIter++;	
			}	

			if (errCode == E_SUCCESS )
			{
				std::string executionStatus; 
				scenarioConfigPtr->GetExecutionStatus(executionStatus) ;
				if (deleteallvolumes == true)
				{
					scenarioConfigPtr->SetExecutionStatus(SCENARIO_DELETION_PENDING) ;							
					RemoveHostIDEntry () ;
				}
				else if ( executionStatus == VOL_PACK_FAILED ) 
				{
					scenarioConfigPtr->SetExecutionStatustoLeastVolumeProvisioningStatus( );
				}

				scenarioConfigPtr->GetProtectedVolumes (protectedVolumes); 
				if (protectedVolumes.size() == 0 )
				{
					scenarioConfigPtr->ClearPlan(); 
				}
			}
			EventQueueConfigPtr eventqconfigPtr = EventQueueConfig::GetInstance() ;
			eventqconfigPtr->AddPendingEvent( request.m_RequestFunctionName ) ;
			ConfSchemaReaderWriterPtr confschemareaderwriterPtr ;
			confschemareaderwriterPtr = ConfSchemaReaderWriter::GetInstance() ;
			confschemareaderwriterPtr->Sync() ;
		}
	}
	else
	{
		errCode = E_NO_PROTECTIONS ;
	}

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return errCode ;
}

INM_ERROR ProtectionHandler::ResyncProtection(const std::list<std::string>& explicitSyncList,
											  const std::list<std::string>& outofSyncList,
											  FunctionInfo& request)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	INM_ERROR errCode = E_UNKNOWN ;
	bool syncRequired = false ;
	ProtectionPairConfigPtr protectionPairConfigPtr ;
	ScenarioConfigPtr scenarioConfigPtr ;
	protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
	scenarioConfigPtr = ScenarioConfig::GetInstance() ;
	VolumeConfigPtr volumeConfigPtr = VolumeConfig::GetInstance() ;
	SV_UINT batchSize = scenarioConfigPtr->GetBatchSize() ;
	std::list<std::string>::const_iterator volIter ;
	volIter = explicitSyncList.begin() ;
	while( volIter != explicitSyncList.end() )
	{
		INM_ERROR volError = E_SUCCESS ;
		if (volumeConfigPtr->IsVolumeAvailable(*volIter))
		{
			if( protectionPairConfigPtr->IsProtectedAsSrcVolume( *volIter ) )
			{
				VOLUME_SETTINGS::PAIR_STATE pairState;
				if (protectionPairConfigPtr->IsResyncProtectionAllowed (*volIter,pairState, true) ) 
				{
					protectionPairConfigPtr->RestartResyncForPair(*volIter, batchSize) ;
					syncRequired = true ;
				}
				else 
				{
					volError = GetErrorCodeFromPairState (pairState);
				}
			}
			else
			{
				volError = E_VOLUME_NOT_PROTECTED ;
			}
		}
		else 
		{
			volError = E_NO_VOLUME_WITH_THE_NAME;	
		}
		ParameterGroup pg ;
		GetVolumeLevelPg(*volIter, volError, "",pg) ; 
		request.m_ResponsePgs.m_ParamGroups.insert(std::make_pair( *volIter, pg)) ;
		volIter++ ;
	}
	volIter = outofSyncList.begin() ;
	while( volIter != outofSyncList.end() )
	{
		INM_ERROR volError = E_SUCCESS ;
		std::string msg ;
		VOLUME_SETTINGS::PAIR_STATE pairState;
		if (protectionPairConfigPtr->IsResyncProtectionAllowed (*volIter,pairState, true) ) 
		{
			if( protectionPairConfigPtr->IsResyncRequiredSetToYes(*volIter) )
			{
				syncRequired = true ;
				protectionPairConfigPtr->RestartResyncForPair(*volIter, batchSize) ;
			}
			else
			{
				volError = E_VOLUME_NOT_OUTOFSYNC ;
				//msg = "Volume is not out of sync" ;
			}
		}
		else
		{
			volError = GetErrorCodeFromPairState (pairState);
		}

		ParameterGroup pg ;
		GetVolumeLevelPg(*volIter, volError, msg,pg) ; 
		request.m_ResponsePgs.m_ParamGroups.insert(std::make_pair( *volIter, pg)) ;
		volIter++ ;
	}
	if( syncRequired )
	{
		errCode = E_SUCCESS ;
		EventQueueConfigPtr eventqconfigPtr = EventQueueConfig::GetInstance() ;
		eventqconfigPtr->AddPendingEvent( request.m_RequestFunctionName ) ;

		ConfSchemaReaderWriterPtr confschemareaderwriterPtr ;
		confschemareaderwriterPtr = ConfSchemaReaderWriter::GetInstance() ;
		confschemareaderwriterPtr->Sync() ;
	}
	else
	{
		errCode = E_NO_VOLUME_OUTOFSYNC ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return errCode ;
}

INM_ERROR ProtectionHandler::ResyncProtection(FunctionInfo& request)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	INM_ERROR errCode = E_UNKNOWN ;
	ProtectionPairConfigPtr protectionPairConfig = ProtectionPairConfig::GetInstance() ;
	ScenarioConfigPtr scenarioConfigPtr = ScenarioConfig::GetInstance() ;
	if( scenarioConfigPtr->ProtectionScenarioAvailable() )
	{
		ConstParameterGroupsIter_t pgIter ;
		bool onlyWhenOutOfSync = true ;
		pgIter = request.m_RequestPgs.m_ParamGroups.find( "VolumeList" ) ;
		std::list<std::string> outOfSyncList, explicitSyncList ;
		ParameterGroup volumePg ;
		if( pgIter != request.m_RequestPgs.m_ParamGroups.end() )
		{
			volumePg = pgIter->second ;
			GetVolumeListFromPg(volumePg, explicitSyncList) ;
		}
		ConstParametersIter_t paramIter ;
		paramIter = request.m_RequestPgs.m_Params.find("AllVolumes") ;
		if( paramIter != request.m_RequestPgs.m_Params.end() && InmStrCmp<NoCaseCharCmp>( paramIter->second.value,  "Yes" ) == 0 )
		{
			onlyWhenOutOfSync = false ;
		}
		std::list<std::string> protectedVolumes ;
		protectionPairConfig->GetProtectedVolumes(protectedVolumes) ;
		std::list<std::string>::const_iterator protectedVolIter ;

		protectedVolIter = protectedVolumes.begin() ;
		while( protectedVolIter != protectedVolumes.end() )
		{
			if( !onlyWhenOutOfSync )
			{
				if( std::find( explicitSyncList.begin(), explicitSyncList.end(), *protectedVolIter) ==
					explicitSyncList.end() )
				{
					explicitSyncList.push_back( *protectedVolIter ) ;
				}
			}

			if( std::find( explicitSyncList.begin(), explicitSyncList.end(), *protectedVolIter) ==
				explicitSyncList.end() )
			{
				outOfSyncList.push_back( *protectedVolIter ) ;
			}

			protectedVolIter++ ;
		}
		if (explicitSyncList.size() != 0)
		{
			outOfSyncList.clear() ;
		}
		errCode = ResyncProtection(explicitSyncList, outOfSyncList, request) ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return errCode ;
}
std::string GetTagName (FunctionInfo &request)
{
	std::string tagPrefix;
	ParametersIter_t attrtIter = request.m_RequestPgs.m_Params.find("TagPrefix");
	if (attrtIter != request.m_RequestPgs.m_Params.end())
	{
		tagPrefix = attrtIter->second.value;
	}
	return tagPrefix;
}
INM_ERROR ProtectionHandler::IssueConsistency(FunctionInfo& request)
{	
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	INM_ERROR errCode = E_UNKNOWN;    
	ScenarioConfigPtr scenarioConfigptr = ScenarioConfig::GetInstance() ;
	int diffPairCount = 0 ;
	std::list<std::string> protectedVolumes ;
	if( scenarioConfigptr->ProtectionScenarioAvailable() )
	{
		ProtectionPairConfigPtr protectionPairConfigPtr ;
		protectionPairConfigPtr = ProtectionPairConfig::GetInstance() ;
		protectionPairConfigPtr->GetProtectedVolumes(protectedVolumes) ;
		std::list<std::string>::const_iterator protectedVolIter ;
		protectedVolIter = protectedVolumes.begin() ;

		while( protectedVolIter != protectedVolumes.end() )
		{            
			if( protectionPairConfigPtr->IsPairInDiffSync(*protectedVolIter) )
			{
				diffPairCount++ ;
				break ;
			}                      
			protectedVolIter++ ;
		}
		if( diffPairCount == 0 )
		{
			errCode = E_NO_VOLUME_ELIGIBLE_FOR_ISSUE_CONSISTENCY ;
		}
	}
	else
	{
		errCode = E_NO_PROTECTIONS ;
	}
	if( diffPairCount > 0 )
	{
		errCode = E_SUCCESS ;
		PolicyConfigPtr policyConfigPtr = PolicyConfig::GetInstance() ;
		SV_UINT scenarioId = scenarioConfigptr->GetProtectionScenarioId() ;
		ConsistencyOptions consistencyOptions ;
		scenarioConfigptr->GetConsistencyOptions( consistencyOptions ) ;
		std::string tagPrefix = GetTagName (request);
		if (!tagPrefix.empty())
		{
			consistencyOptions.tagName = tagPrefix;
		}
		policyConfigPtr->CreateConsistencyPolicy( consistencyOptions, protectedVolumes, scenarioId, false ) ;
		EventQueueConfigPtr eventqconfigPtr = EventQueueConfig::GetInstance() ;
		eventqconfigPtr->AddPendingEvent( request.m_RequestFunctionName ) ;

		ConfSchemaReaderWriterPtr  schemaWriter = ConfSchemaReaderWriter::GetInstance() ;
		schemaWriter->Sync() ;
	}
	return errCode ;
}
INM_ERROR ProtectionHandler::process(FunctionInfo& request)
{
	Handler::process(request);
	INM_ERROR errCode = E_FUNCTION_NOT_EXECUTED;

	int systemPaused;
	if ( !GetSystemPausedState(systemPaused) )
		systemPaused = 0;
	if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName.c_str(),"DeleteBackupProtection") == 0 && systemPaused != 1)
	{
		errCode = DeleteBackupProtection(request);
	}
	else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName.c_str(),"ResumeTracking") == 0 &&  systemPaused != 1)
	{
		errCode = ResumeTracking(request);
	}
	else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName.c_str(),"ResumeTrackingForVolume") == 0/* &&  systemPaused != 1*/)
	{
		errCode = ResumeTrackingForVolume(request) ;
	}
	else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName.c_str(),"ResyncProtection") == 0 &&  systemPaused != 1)
	{
		errCode = ResyncProtection(request);
	}
	else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName.c_str(),"ResumeProtection") == 0)
	{
		errCode = ResumeProtection(request);
	}
	else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName.c_str(),"ResumeProtectionForVolume") == 0 /* &&  systemPaused != 1 */)
	{
		errCode = ResumeProtectionForVolume(request);
	}
	else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName.c_str(),"SetupBackupProtection") == 0 &&  systemPaused != 1)
	{
		errCode = SetupBackupProtection(request);
	}
	else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName.c_str(),"SpaceRequired") == 0 &&  systemPaused != 1)
	{
		errCode = SpaceCheck(request);
	}
	else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName.c_str(),"PauseTracking") == 0 &&  systemPaused != 1)
	{
		errCode = PauseTracking(request);
	}
	else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName.c_str(),"PauseTrackingForVolume") == 0 /* &&  systemPaused != 1*/)
	{
		errCode = PauseTrackingForVolume(request);
	}
	else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName.c_str(),"PauseProtection") == 0)
	{
		errCode = PauseProtection(request);
	}
	else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName.c_str(),"PauseProtectionForVolume") == 0 /* &&  systemPaused != 1 */)
	{
		errCode = PauseProtectionForVolume(request);
	}
	else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName.c_str(),"IssueConsistency") == 0 &&  systemPaused != 1)
	{
		errCode = IssueConsistency(request);
	}
	else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName.c_str(),"AddVolumes") == 0 &&  systemPaused != 1)
	{
		errCode = AddVolumes(request);
	}
	else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName.c_str(),"ListHosts") == 0)
	{
		errCode = ListHosts(request);
	}
	else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName.c_str(),"ExportRepositoryOnCIFS") == 0)
	{
		//errCode = ExportRepositoryOnCIFS(request);
	}
	else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName.c_str(),"GetExportedRepositoryDetails") == 0 || 
		InmStrCmp<NoCaseCharCmp>(m_handler.functionName.c_str(),"GetExportedRepositoryDetailsByAgentGUID") == 0 )
	{
		//errCode = GetExportedRepositoryDetails(request);
	}
	else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName.c_str(),"ListHostsMinimal") == 0 )
	{
		errCode = ListHostsMinimal(request);
	}
	else if(InmStrCmp<NoCaseCharCmp>(m_handler.functionName.c_str(),"SetLogRotationInterval") == 0 )
	{
		errCode = SetLogRotationInterval(request);
	}
	return Handler::updateErrorCode(errCode);
}

INM_ERROR ProtectionHandler::SetLogRotationInterval(FunctionInfo& request)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	LocalConfigurator lc ;
	SV_UINT backupIntervalInSec =  24 * 60 * 60 ;
	bool fullbackupRequired = true ;
	if( request.m_RequestPgs.m_Params.find("RequiredFullBackup") != request.m_RequestPgs.m_Params.end() )
	{
		std::string value = request.m_RequestPgs.m_Params.find("RequiredFullBackup")->second.value ;
		if( value == "Yes" )
		{
			fullbackupRequired = true ;
		}
		else
		{
			fullbackupRequired = false ;
		}
	}
	if( fullbackupRequired )
	{
		if( request.m_RequestPgs.m_Params.find("FullBackupIntervalInSec") != request.m_RequestPgs.m_Params.end() )
		{
			backupIntervalInSec = boost::lexical_cast<SV_UINT>(request.m_RequestPgs.m_Params.find("FullBackupIntervalInSec")->second.value) ;
		}
		else
		{
			fullbackupRequired = false ;
		}
	}
	if( fullbackupRequired )
	{
		lc.SetFullBackupInterval(backupIntervalInSec) ;
	}
	lc.SetFullBackupRequired(fullbackupRequired) ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return E_SUCCESS ;
}
INM_ERROR ProtectionHandler::validate(FunctionInfo& request)
{
	INM_ERROR errCode = Handler::validate(request);
	if(E_SUCCESS != errCode)
		return errCode;

	//TODO: Write hadler specific validation here

	return errCode;
}

INM_ERROR ProtectionHandler::ListHostsMinimal(FunctionInfo& request)
{
	std::list<std::map<std::string, std::string> >hostInfoList ;
	GetHostDetailsToRepositoryConf(hostInfoList) ;
	std::list<std::map<std::string, std::string> >::const_iterator hostInfoIter ;
	hostInfoIter = hostInfoList.begin() ;
	int i =0 ;
	while( hostInfoIter != hostInfoList.end() )
	{
		const std::map<std::string, std::string>& propertyPairs = *hostInfoIter ;
		i++ ;
		ParameterGroup systemGuidPg ;
		ValueType vt ;
		std::string hostname, ipaddress, agentGuid, recoveryFeaturePolicy ;
		if( propertyPairs.find("hostname") != propertyPairs.end() )
		{
			vt.value = propertyPairs.find("hostname")->second ;
		}
		systemGuidPg.m_Params.insert(std::make_pair("HostName", vt)) ;
		if( propertyPairs.find("ipaddress") != propertyPairs.end() )
		{
			vt.value = propertyPairs.find("ipaddress")->second ;
		}
		systemGuidPg.m_Params.insert(std::make_pair("HostIP", vt)) ;
		if( propertyPairs.find("hostid") != propertyPairs.end() )
		{
			agentGuid = propertyPairs.find("hostid")->second ;
		}
		vt.value =  agentGuid ;
		systemGuidPg.m_Params.insert(std::make_pair("HostAgentGUID", vt)) ;
		vt.value = "" ;
		systemGuidPg.m_Params.insert(std::make_pair("SystemGUID", vt)) ;
		request.m_ResponsePgs.m_ParamGroups.insert(std::make_pair("System GUID" + boost::lexical_cast<std::string>(i), 
			systemGuidPg)) ;
		hostInfoIter ++ ;
	}
	return E_SUCCESS ;
}

INM_ERROR ProtectionHandler::ListHosts(FunctionInfo& request)
{
#ifdef SV_WINDOWS
	WSADATA wsaData;
	int err = WSAStartup(MAKEWORD(1,1), &wsaData);
	if (err != 0) 
	{
		DebugPrintf(SV_LOG_DEBUG,"Could not find a usable WINSOCK DLL\n");
	}
#endif
	INM_ERROR errCode ;
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	std::list<std::map<std::string, std::string> >hostInfoList ;
	GetHostDetailsToRepositoryConf(hostInfoList) ;
	std::list<std::map<std::string, std::string> >::const_iterator itB, itE ;
	itB = hostInfoList.begin() ;
	itE = hostInfoList.end() ;
	int i = 0 ;
	std::string repositoryPath = ConfSchemaMgr::GetInstance()->getRepoLocation();
	while( itB != itE )
	{
		const std::map<std::string, std::string>& propertyPairs = *itB ;
		i++ ;
		ParameterGroup systemGuidPg ;
		ValueType vt ;
		std::string hostname, ipaddress, agentGuid, recoveryFeaturePolicy, repopath ;
		if( propertyPairs.find("hostname") != propertyPairs.end() )
		{
			vt.value = propertyPairs.find("hostname")->second ;
			hostname = vt.value ;
		}
		systemGuidPg.m_Params.insert(std::make_pair("HostName", vt)) ;
		if( propertyPairs.find("ipaddress") != propertyPairs.end() )
		{
			vt.value = propertyPairs.find("ipaddress")->second ;
		}
		systemGuidPg.m_Params.insert(std::make_pair("HostIP", vt)) ;
		if( propertyPairs.find("hostid") != propertyPairs.end() )
		{
			agentGuid = propertyPairs.find("hostid")->second ;
		}
		vt.value =  agentGuid ;
		systemGuidPg.m_Params.insert(std::make_pair("HostAgentGUID", vt)) ;
		if( propertyPairs.find("repopath") != propertyPairs.end() )
		{
			repopath = propertyPairs.find("repopath")->second ;
		}
		vt.value =  repopath ;
		systemGuidPg.m_Params.insert(std::make_pair("RepoPath", vt)) ;

		vt.value = "" ;
		systemGuidPg.m_Params.insert(std::make_pair("SystemGUID", vt)) ;

		std::string repoLocationForHost = constructConfigStoreLocation( repositoryPath, agentGuid ) ;
		LocalConfigurator lc ;
		ScenarioConfigPtr scenarioConf ;
		AgentConfigPtr agentConfigPtr ;
		std::string lockPath = getLockPath(repoLocationForHost) ;
		RepositoryLock repolock( lockPath ) ;
		if( lc.getHostId() != agentGuid )
		{
			repolock.Acquire() ;
		}
		ConfSchemaReaderWriter::CreateInstance( request.m_RequestFunctionName, repositoryPath,  repoLocationForHost, false) ;
		scenarioConf = ScenarioConfig::GetInstance() ;
		agentConfigPtr = AgentConfig::GetInstance() ;
		vt.value = agentConfigPtr->GetAgentVersion() ;
		if( vt.value.empty() )
		{
			DebugPrintf(SV_LOG_WARNING, "Couldn't find the details for host %s.. Skipping it from hosts list\n", hostname.c_str()) ;
			itB++;
			continue ;
		}
		systemGuidPg.m_Params.insert(std::make_pair( "AgentVersion", vt)) ;
		scenarioConf->GetRecoveryFeaturePolicy( recoveryFeaturePolicy ) ;
		vt.value = recoveryFeaturePolicy ;
		systemGuidPg.m_Params.insert(std::make_pair( "RecoveryFeaturePolicy", vt ) ) ;    
		std::map<std::string, std::string> osInfoAttrMap ;
		std::list<std::map<std::string, std::string> > cpuInfos ;
		agentConfigPtr->GetOSInfo( osInfoAttrMap ) ;
		agentConfigPtr->GetCPUInfo( cpuInfos ) ;
		std::string systemdir = agentConfigPtr->GetSystemVol() ;
		vt.value = systemdir ;
		systemGuidPg.m_Params.insert(std::make_pair( "SystemDir", vt)) ;
		ParameterGroup CPUInfoListPg ;
		std::list<std::map<std::string, std::string> >::const_iterator cpuInfoiter = cpuInfos.begin() ;
		while( cpuInfoiter != cpuInfos.end() )
		{
			int i = 0 ;
			const std::map<std::string, std::string>& cpuattrs = *cpuInfoiter ;
			ParameterGroup CPUInfoPg ;
			std::map<std::string, std::string>::const_iterator  cpuAttrIter ; 
			cpuAttrIter = cpuattrs.begin() ;
			std::string cpuid ;
			while( cpuattrs.end() != cpuAttrIter )
			{
				vt.value = cpuAttrIter->second ;
				CPUInfoPg.m_Params.insert( std::make_pair( cpuAttrIter->first, vt) ) ;
				cpuAttrIter ++ ;
			}
			std::string cpuinfolistidx ;
			cpuinfolistidx = "CPUInfoList" ;
			cpuinfolistidx += "[" ;
			cpuinfolistidx += boost::lexical_cast<std::string>(i++) ;
			cpuinfolistidx += "]" ;
			CPUInfoListPg.m_ParamGroups.insert( std::make_pair( cpuinfolistidx, 
				CPUInfoPg)) ;
			cpuInfoiter++ ;
		}
		systemGuidPg.m_ParamGroups.insert( std::make_pair( "CPUInfoList", CPUInfoListPg)); 
		ParameterGroup osInfoPg ;

		std::map<std::string, std::string>::const_iterator osattrIter ;
		osattrIter = osInfoAttrMap.begin() ;
		while( osattrIter != osInfoAttrMap.end() )
		{
			vt.value = osattrIter->second ;
			osInfoPg.m_Params.insert( std::make_pair( osattrIter->first, vt ) ) ;
			osattrIter++ ;
		}
		systemGuidPg.m_ParamGroups.insert( std::make_pair( "osInfo", osInfoPg)); 
		DebugPrintf( SV_LOG_DEBUG, "Config Store Location: %s, Recovery Feature: %s\n", repoLocationForHost.c_str(), recoveryFeaturePolicy.c_str() ) ;
		request.m_ResponsePgs.m_ParamGroups.insert(std::make_pair("System GUID" + boost::lexical_cast<std::string>(i), 
			systemGuidPg)) ;
		itB++ ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	if( i != 0 )
	{
		errCode = E_SUCCESS ; 
	}
	else
		errCode = E_NO_HOSTS_FOUND ;
	return errCode ;
}

bool ProtectionHandler::GetHostDetailsToRepositoryConf(std::list<std::map<std::string, std::string> >& hostInfoList)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool ok = false ;
	std::string smbConfFile ;
	ACE_Configuration_Heap m_inifile ;
	std::string repoLocation = ConfSchemaMgr::GetInstance()->getRepoLocation() ;
#ifdef SV_WINDOWS
	if( repoLocation.length() == 1 )
	{
		repoLocation += ":" ;
	}
#endif
	if( repoLocation[repoLocation.length() - 1 ] != ACE_DIRECTORY_SEPARATOR_CHAR_A )
	{
		repoLocation += ACE_DIRECTORY_SEPARATOR_CHAR_A ;
	}

	std::string repoConfigPath = repoLocation + "repository.ini" ;
	if( m_inifile.open() == 0 )
	{
		ACE_Ini_ImpExp importer( m_inifile );

		if( importer.import_config(ACE_TEXT_CHAR_TO_TCHAR(repoConfigPath.c_str())) == 0 )
		{
			ACE_Configuration_Section_Key sectionKey; 

			ACE_TString value;
			ACE_Configuration_Section_Key shareSectionKey;
			int sectionIndex = 0 ;
			ACE_TString tStrsectionName;
			while( m_inifile.enumerate_sections(m_inifile.root_section(), sectionIndex, tStrsectionName) == 0 )
			{
				std::map<std::string, std::string> hostpropertyMap ;
				m_inifile.open_section(m_inifile.root_section(), tStrsectionName.c_str(), 0, sectionKey) ;
				std::string hostname = ACE_TEXT_ALWAYS_CHAR(tStrsectionName.c_str());

				ACE_TString tStrKeyName, tStrValue;
				ACE_Configuration::VALUETYPE valueType = ACE_Configuration::STRING ;
				m_inifile.get_string_value(sectionKey, ACE_TEXT_CHAR_TO_TCHAR("hostid"), tStrValue) ; 
				std::string hostId = ACE_TEXT_ALWAYS_CHAR(tStrValue.c_str());
				m_inifile.get_string_value(sectionKey, ACE_TEXT_CHAR_TO_TCHAR("ipaddress"), tStrValue) ; 
				std::string ipaddress = ACE_TEXT_ALWAYS_CHAR(tStrValue.c_str());
				m_inifile.get_string_value(sectionKey, ACE_TEXT_CHAR_TO_TCHAR("repopath"), tStrValue) ; 
				std::string repopath = ACE_TEXT_ALWAYS_CHAR(tStrValue.c_str());

				hostpropertyMap.insert(std::make_pair("hostid", hostId)) ;
				hostpropertyMap.insert(std::make_pair("hostname", hostname)) ;
				hostpropertyMap.insert(std::make_pair("ipaddress", ipaddress)) ;
				hostpropertyMap.insert(std::make_pair("repopath", repopath)) ;
				hostInfoList.push_back(hostpropertyMap) ;
				sectionIndex++; 
			}
		}
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return ok ;
}



INM_ERROR ProtectionHandler::AddVolumes(FunctionInfo& request)
{
	DebugPrintf( SV_LOG_DEBUG, "ENTERED: %s", FUNCTION_NAME ) ;
#ifdef SV_WINDOWS
	WSADATA wsaData;
	int err = WSAStartup(MAKEWORD(1,1), &wsaData);
	if (err != 0) 
	{
		DebugPrintf(SV_LOG_DEBUG,"Could not find a usable WINSOCK DLL\n");
	}
#endif

#ifdef SV_WINDOWS
	AddHostDetailsToRepositoryConf() ;
#endif	
	INM_ERROR errorCode = E_SUCCESS ;
	std::string repoCapacityStr;
	SV_ULONGLONG totalRequiredCapacity = 0;
	ParameterGroup volumesPg  ;	
	VolumeConfigPtr volumeConfigPtr = VolumeConfig::GetInstance() ;
	ScenarioConfigPtr scenarioConfigPtr =  ScenarioConfig::GetInstance() ;
	ProtectionPairConfigPtr protectionPairConfigPtr = ProtectionPairConfig::GetInstance();

	std::list<std::string> volumeListFromRequest ;
	if( request.m_RequestPgs.m_ParamGroups.find("VolumeList") !=
		request.m_RequestPgs.m_ParamGroups.end() )
	{
		volumesPg = request.m_RequestPgs.m_ParamGroups.find("VolumeList")->second ;
		GetVolumeListFromPg(volumesPg, volumeListFromRequest) ;
	}
	std::list<std::string> alreadyprotectedVolumes; 
	scenarioConfigPtr->GetProtectedVolumes (alreadyprotectedVolumes);
	std::list<std::string> listOfBothProtectedAndNewVolumes ;
	listOfBothProtectedAndNewVolumes.insert(listOfBothProtectedAndNewVolumes.end(), alreadyprotectedVolumes.begin(), alreadyprotectedVolumes.end());
	listOfBothProtectedAndNewVolumes.insert(listOfBothProtectedAndNewVolumes.end(), volumeListFromRequest.begin(), volumeListFromRequest.end());
	std::list<std::list<std::string> > listOfVolList ;
	if (volumeConfigPtr->GetSameVolumeList(listOfBothProtectedAndNewVolumes,listOfVolList) )
	{
		std::list<std::list<std::string> >::const_iterator listOfListIter = listOfVolList.begin() ;
		if(listOfListIter != listOfVolList.end())
		{
			std::list<std::string>::const_iterator listIter = (*listOfListIter).begin() ;
			request.m_Message += (*listIter) + " is accessible using " ;
			while (listIter != (*listOfListIter).end())
			{
				request.m_Message += (*listIter) + ", " ;
				listIter++ ;
			}
			request.m_Message += " . " ;
		}
		errorCode = E_VOLUME_HAVING_MULTIPLE_ACCESS_POINTS ;
		return errorCode ;
	}
	std::list<std::string>::iterator reqVolumeIterator = volumeListFromRequest.begin() ;
	double spaceMultiplicationFactor = GetSpaceMultiplicationFactorFromRequest (request);
	if( reqVolumeIterator != volumeListFromRequest.end() )
	{
		std::list<std::string> registeredVolumeList, eligibleVolumes, repoVolumes, protectedVolumes ;
		std::map<std::string, volumeProperties> volumePropertiesMap ;
		std::map<std::string, volumeProperties>::iterator volumePropertyIterator ;

		volumePropertiesMap = volumeConfigPtr->GetGeneralVolumes( registeredVolumeList ) ;
		volumeConfigPtr->GetRepositoryVolumes( repoVolumes ) ;

		scenarioConfigPtr->GetProtectedVolumes( protectedVolumes ) ;
		while( reqVolumeIterator != volumeListFromRequest.end() )
		{
			INM_ERROR localErrorCode = E_SUCCESS ;
			if( std::find( registeredVolumeList.begin(), registeredVolumeList.end(), *reqVolumeIterator ) != registeredVolumeList.end() )
			{
				if( std::find( repoVolumes.begin(), repoVolumes.end(), *reqVolumeIterator ) == repoVolumes.end() )
				{
					if( std::find( protectedVolumes.begin(), protectedVolumes.end(), *reqVolumeIterator ) == protectedVolumes.end() )
					{
						volumePropertyIterator = volumePropertiesMap.find( *reqVolumeIterator ) ;
						if( volumePropertyIterator != volumePropertiesMap.end() &&  boost::lexical_cast<SV_ULONGLONG>( volumePropertyIterator->second.capacity ) > 0 )
						{
							eligibleVolumes.push_back( *reqVolumeIterator ) ; 
						}
						else
							localErrorCode = E_VOLUME_RAW ;
					}
					else
						localErrorCode = E_VOLUME_ALREADY_PROTECTED ;                                           
				}
				else
					localErrorCode = E_REPOSITORY_VOLUME_CANNOTBE_PROTECTED ;                
			}
			else
				localErrorCode = E_NO_VOLUME_WITH_THE_NAME ;                
			if( localErrorCode != E_SUCCESS )
			{  
				ParameterGroup volumeErrorPg ;
				GetVolumeLevelPg( *reqVolumeIterator, localErrorCode, "",  volumeErrorPg ) ;
				request.m_ResponsePgs.m_ParamGroups.insert( std::make_pair( *reqVolumeIterator, volumeErrorPg ) ) ;
			}
			reqVolumeIterator++ ;
		}
		RetentionPolicy retentionPolicy ; 
		SV_ULONGLONG requiredSpace = 0;
		if( scenarioConfigPtr->GetRetentionPolicy( "", retentionPolicy )  )
		{
			std::string repoName, repoPath ;
			std::list <std::string> volumesSpaceCalculation;
			volumeConfigPtr->GetRepositoryNameAndPath( repoName, repoPath ) ;
			SV_ULONGLONG repoFreeSpace = 0,protectedVolumeOverHead = 0;
#ifdef SV_WINDOWS
			std::string volume ;
			std::string diskname ;
			if( DoesDiskContainsProtectedVolumes( repoPath, eligibleVolumes, diskname, volume ) )
			{
				errorCode = E_SAMEREPOSITORY_PROTECTEDVOL_DISK ;
				request.m_Message = "The chosen backup location " ;
				request.m_Message += repoPath ;
				request.m_Message += " and volume " ;
				request.m_Message += volume ;
				if( "__INMAGE__DYNAMIC__DG__" != diskname )
				{
					request.m_Message += " are on same disk ";
					request.m_Message += diskname;
				}
				else
				{
					request.m_Message += " are on same dynamic disk group ";
				}
				return errorCode ;
			}
#endif
			std::list<std::string> protectedVolumes; 
			scenarioConfigPtr->GetProtectedVolumes (protectedVolumes);
			volumesSpaceCalculation.insert(volumesSpaceCalculation.end(),eligibleVolumes.begin(),eligibleVolumes.end());
			volumesSpaceCalculation.insert(volumesSpaceCalculation.end(), protectedVolumes.begin(), protectedVolumes.end());
			std::map <std::string,hostDetails> hostDetailsMap; 

			std::string overRideSpaceCheck ; 
			isSpaceCheckRequired (request , overRideSpaceCheck);
			bool bSpaceAvailability = true; 
			SV_ULONGLONG requiredSpace ;
			if (overRideSpaceCheck != "Yes")
			{
				spaceCheckParameters spaceParameter ; 
				scenarioConfigPtr->GetIOParameters (spaceParameter); 
				double compressionBenifits = spaceParameter.compressionBenifits; 
				double sparseSavingsFactor = 3.0; 
				double changeRate = spaceParameter.changeRate ;
				double ntfsMetaDataoverHead = 0.13; 
				/*bSpaceAvailability = SpaceCalculationV2 (	repoPath,
														volumesSpaceCalculation, 
														retentionPolicy , 
														requiredSpace,
														protectedVolumeOverHead , 
														repoFreeSpace, 
														hostDetailsMap, 
														compressionBenifits,
														sparseSavingsFactor,
														changeRate,
														ntfsMetaDataoverHead);*/
				
				
					bSpaceAvailability = CheckSpaceAvailability (	repoPath,
																	retentionPolicy ,
																	eligibleVolumes,
																	requiredSpace,
																	repoFreeSpace,
																	compressionBenifits , 
																	sparseSavingsFactor,
																	changeRate ,
																	ntfsMetaDataoverHead  );
			}

			volumeConfigPtr = VolumeConfig::GetInstance() ;
			scenarioConfigPtr =  ScenarioConfig::GetInstance() ;
			protectionPairConfigPtr = ProtectionPairConfig::GetInstance();

			volumeConfigPtr->GetRepositoryFreeSpace(repoName,repoFreeSpace);
			if( bSpaceAvailability )
			{
				PolicyConfigPtr policyConf = PolicyConfig::GetInstance() ;

				std::list<std::string>::iterator eligibleVolIterator = eligibleVolumes.begin() ;
				LocalConfigurator configurator ;
				std::string hostId = configurator.getHostId() ;
				std::map<std::string, std::string> srcTgtMap ;
				SV_UINT rpoThreshold ;
				scenarioConfigPtr->GetRpoThreshold( rpoThreshold ) ;
				
				while ( eligibleVolIterator != eligibleVolumes.end() )
				{
					volumePropertyIterator = volumePropertiesMap.find( *eligibleVolIterator ) ;
					std::string targetVolume = constructTargetVolumeFromSourceAndRepo( *eligibleVolIterator, *(repoVolumes.begin()), hostId ) ;                
					srcTgtMap.insert( std::make_pair( *eligibleVolIterator, targetVolume ) ) ;               
					scenarioConfigPtr->RemoveVolumeFromExclusionList( *eligibleVolIterator ) ;
					scenarioConfigPtr->AddVolumeToProtectedVolumesList( *eligibleVolIterator ) ;
					protectedVolumes.push_back( *eligibleVolIterator ) ;
					ParameterGroup volumeErrorPg ;
					INM_ERROR localErrorCode = E_SUCCESS ;
					GetVolumeLevelPg( *eligibleVolIterator, localErrorCode, "",  volumeErrorPg ) ;
					request.m_ResponsePgs.m_ParamGroups.insert( std::make_pair( *eligibleVolIterator, volumeErrorPg ) ) ;
					eligibleVolIterator++ ;
				}
				if( eligibleVolumes.size() )
				{
					std::map<std::string, PairDetails> pairDetailsMap ;
					GetPairDetails( rpoThreshold, retentionPolicy, repoPath, hostId, eligibleVolumes, volumePropertiesMap, pairDetailsMap ) ;
					policyConf->CreateVolumeProvisioningPolicy(srcTgtMap,  volumePropertiesMap,  scenarioConfigPtr->GetProtectionScenarioId() ) ;
					scenarioConfigPtr->AddPairs( pairDetailsMap ) ;
					EventQueueConfigPtr eventqconfigPtr = EventQueueConfig::GetInstance() ;
					eventqconfigPtr->AddPendingEvent( request.m_RequestFunctionName ) ;

					ConfSchemaReaderWriterPtr csReaderWriter = ConfSchemaReaderWriter::GetInstance() ;
					csReaderWriter->Sync() ;
				}
			}
			else
			{
				errorCode = E_REPO_VOL_HAS_INSUCCICIENT_STORAGE ;
				/*request.m_Message = FormatSpaceCheckMsg(request.m_RequestFunctionName,hostDetailsMap);*/
				std::string msg ;
					msg += "About " ;
					msg += convertToHumanReadableFormat (requiredSpace ) ;
					msg += " is recommended, but only " ;
					msg += convertToHumanReadableFormat (repoFreeSpace) ;
					msg += " is available. " ;
					msg += "Please choose a different backup location with atleast " ;
					msg += convertToHumanReadableFormat (requiredSpace) ;
					msg += " free. " ;
					msg += "You can also try reducing the number of days that backup data is retained." ;
					request.m_Message = msg ;

			}
		}
		else
		{
			errorCode = E_NO_PROTECTIONS ;
		}
	}
	else
	{
		errorCode = E_INSUFFICIENT_PARAMS ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return errorCode ;
}

double GetRepoLogsSpace ( )  
{
	return 0.5;  // Space for Repo Logs 512 MB
}

bool GetTotalCapacity(const std::list<std::string> &addVolumeList , SV_ULONGLONG &protectedCapacity)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
	bool bRet = true;
	VolumeConfigPtr volumeConfigPtr = VolumeConfig::GetInstance() ;
	std::list<std::string>::const_iterator addVolumeListIter = addVolumeList.begin();
	while (addVolumeListIter != addVolumeList.end () )
	{	
		SV_LONGLONG capacity;		
		volumeConfigPtr->GetCapacity (*addVolumeListIter,capacity);
		protectedCapacity += boost::lexical_cast <SV_ULONGLONG> (capacity);
		addVolumeListIter ++;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;

	return bRet;
}

bool GetTotalFileSystemUsedSpace(const std::list<std::string> &eligibleVolumes,
								 SV_ULONGLONG &fileSystemUsedSpace)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;

	bool bRet = true;
	VolumeConfigPtr volumeConfigPtr = VolumeConfig::GetInstance() ;
	std::list <std::string>::const_iterator eligibleVolumesIter = eligibleVolumes.begin();
	while (eligibleVolumesIter != eligibleVolumes.end() )
	{
		SV_ULONGLONG volumeFileSystemUsedSpace = 0;
		volumeConfigPtr->GetFileSystemUsedSpace (*eligibleVolumesIter,volumeFileSystemUsedSpace);
		fileSystemUsedSpace += boost::lexical_cast <SV_ULONGLONG> (volumeFileSystemUsedSpace);
		eligibleVolumesIter ++ ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
	return bRet;
}

