#ifndef UTIL__H_
#define UTIL__H_
#include<map>
#include "confschema.h"
#include "svtypes.h"
#include <boost/filesystem/operations.hpp>
#include <boost/algorithm/string/trim.hpp>
#include "cdpcli.h"
#include "VacpUtil.h"
#include "StreamRecords.h"
#include "InmXmlParser.h"
#define CACHECLEANUPPOS 0
#define PENDINGFILESPOS 2
#define VSNAPCLEANUPPOS 4
#define RETENTIONLOGPOS 6
#define UNLOCKTGTDEVICEPOS 8
const char VOL_PACK_PROVISON[] = "VolumeProvisioning";
const char VOL_PACK_PROVISONV2[] = "VolumeProvisioningV2";
const char UPDATE_POLICY_INPROGRESS[] = "InProgress";
const char UPDATE_POLICY_FAILED[] = "Failed";
const char UPDATE_POLICY_SUCCESS[] = "Success";
const char VOL_PACK_PENDING[] = "Target Volume Provisioning Pending" ;
const char VOL_PACK_SUCCESS[] = "Target Volume Provisioning Successful";
const char VOL_PACK_FAILED[] = "Target Volume Provisioning Failed";
const char VOL_PACK_INPROGRESS[] = "Target Volume Provisioning In Progress";
const char VOL_PACK_DELETION_SUCCESS[] = "Target Volume Un-Provisioning Successful";
const char VOL_PACK_DELETION_FAILED[] = "Target Volume Un-Provisioning Failed";
const char VOL_PACK_DELETION_INPROGRESS[] = "Target Volume Un-Provisioning In Progress";
const char VOL_PACK_DELETION_PENDING[] = "Target Volume Un-Provisioning Pending";
const char SCENARIO_DELETION_PENDING[] = "Scenario Deletion Pending";
const char SCENARIO_DELETION_FAILED[] = "Scenario Deletion Failed" ;
const char UNDER_PROTECTION[] = "Active" ;
const char CONSISTENCY_POLICY[] = "Consistency";
struct ConsistencyOptions
{
    std::string exception ;
    std::string interval ;
    std::string tagName ;
    std::string tagType ;
} ;


struct ScenarioRetentionWindow
{
    int count ;
    std::string duration ;
    std::string granularity ;
    std::string retentionWindowId ;
} ;

struct RetentionPolicy
{
    std::string duration ;
    std::string retentionType ;
    std::string size ;
    std::list<ScenarioRetentionWindow> retentionWindows ;
} ;

struct PairDetails
{
    std::string srcVolumeName ;
    std::string retentionPath ;
    std::string retentionVolume ;
    std::string volumeProvisioningStatus ;
    int rpoThreshold ;
    std::string targetVolume ;
    SV_LONGLONG srcVolRawSize ;
    SV_LONGLONG srcVolCapacity ;
    RetentionPolicy retPolicy ;
} ;

struct AutoResyncOptions
{
    std::string between ;
    std::string state ;
    int waitTime ;
	AutoResyncOptions ()
	{
		between = "00:00-23:59";
		waitTime = 10;
	}
} ;
struct ReplicationOptions
{
    int batchSize ;
    AutoResyncOptions autoResyncOptions ;
	ReplicationOptions ()
	{
		batchSize = 0;
	}
} ;
typedef struct hostDetails
{
	SV_ULONGLONG fileSystemUsedSpace; 
	SV_ULONGLONG protectedCapacity; 
    SV_ULONGLONG repoFreeSpace ;
    SV_ULONGLONG requiredSpace;
	SV_ULONGLONG protectedVolumeOverHeadLocal;
} hostDetails;

typedef struct spaceCheckParameters 
{
	double compressionBenifits;
	double sparseSavingsFactor; 
	double changeRate;
	double ntfsMetaDataoverHead; 
} spaceCheckParameters; 
enum DRIVER_STATE
{
	UNKNOWN = 3,
	STOPPED_FILTERING = 1,
	START_FILTERING = 2 
};

//template <typename KEY, typename VALUE>
//void FetchValueFromMap(std::map<KEY,VALUE> &m ,const KEY &keyName, VALUE &value); 
void FetchValueFromMap(const std::map<std::string,std::string> &m ,const std::string &keyName, std::string &value); 
void WriteLogMessageIntoFile(const std::string &logMessage);

void getTimeinDisplayFormat( const SV_ULONGLONG& eventTime, std::string& timeInDisplayForm );
int is_leap_year(int year);
bool date_is_valid(int day, int month, int year);
bool lengthValidation (std::string year,std::string month,std::string date,std::string hour,std::string min,std::string sec);
bool emptyDateValidation (std::string year,std::string month , std::string date , std::string hour ,std::string min ,std::string sec );
bool rangeValidation (std::string year,std::string month , std::string date , std::string hour ,std::string min ,std::string sec);
bool dateValidator (std::string year,std::string month , std::string date , std::string hour ,std::string min ,std::string sec );
void GetGranularityUnitAndTimeGranularityUnit(const std::string &granularity,
                                              std::string & granularityUnit,
                                              std::string & timeGranularityUnit) ;
void GetDurationAndTimeIntervelUnit(const std::string& duration, time_t& durationInHours,
                                    std::string& timeIntervalUnit) ;
void GetExcludeFromAndTo(const std::string& exception,
                                       time_t &StartTime, time_t &EndTime) ;
std::string getkeyvaluefromFunInfoPGAttrs(FunctionInfo& request,std::string GroupName,std::string attrKey);
void GetRequiredGroup(ConfSchema::Groups_t &Groups,std::string& groupName,ConfSchema::Group& GroupObj);
std::string getPairStatus(std::string& ReplicationState,std::string& Pairstate);
std::string getPairStatus(ConfSchema::Object& protectionPairObj) ;
std::string getPairStatus(int ReplicationState,int Pairstate) ;
bool getCommonRecoveryRanges(std::map<std::string, std::map<SV_ULONGLONG, CDPTimeRange> >& volumeRecoveryRangeMap, ParameterGroup& CommonRecoveryRangesPG) ;
void insertTimeRanageMapToRRPG( ParameterGroup& arrangesPG, std::map<SV_ULONGLONG, CDPTimeRange>& timeRangeMap, std::vector<std::string>& RecoveryRangeProperties ) ;
void insertRestorePointMapToRPPG( ParameterGroup &arPointsPG, std::map<SV_ULONGLONG, CDPEvent>& restorePointMap,std::vector<std::string>& RestorePntProperties ) ;
void insertRestorePointToRPPG( ParameterGroup &restrpntchilds, const CDPEvent& tempEvent,std::vector<std::string>& RestorePntProperties) ;
void insertTimeRangeToRRPG( ParameterGroup &recvyrangeschilds, const CDPTimeRange& timeRange, std::vector<std::string>& RecoveryRangeProperties )  ;
void insertTimeRanageMapToRRPG( ParameterGroup &arrangesPG, std::map<SV_ULONGLONG, SV_ULONGLONG>& recoveryRangeMap )  ;
void convertTimeStampToCXformat(std::string& ActualFormat,std::string& CxFormat, bool adjustNextSec = false ) ;
bool getRecoveryRangesforVolume(const std::string& actualbasepath,
							    const std::string& newbasedpath,
							    const std::string &retentionPath, 
							    std::map< SV_ULONGLONG, CDPTimeRange >& volumeRRMap, 
								const int& rpoPolicyinHrs, 
								const int& nRangeCount ) ;
bool getRestorePointsforVolume(const std::string& actualbasepath,
							   const std::string& newbasedpath,
							   const std::string &retentionPath, 
							   std::map<SV_ULONGLONG, CDPEvent>& volumeRPMap, 
							   const int& rpoPolicyinHrs, const int& nRestoreCount  ) ;
bool getRestorePointsforVolume(const std::string &retentionPath, std::map<SV_ULONGLONG, CDPEvent>& volumeRPMap, const int& rpoPolicyInHrs = 0, const int& nRestoreCount = 0 ) ;
void GetAvailableRestorePointsPg(const std::string& actualbasepath,
								 const std::string& newbasepath,
								 const std::string& retentionPath,
								 SV_UINT rpoThreshold,
                                 ParameterGroup& availableRestorePointsPg, 
								 std::map<SV_ULONGLONG, CDPEvent>& restorePointMap, 
								 const int& nRestoreCount   ) ;
void GetAvailableRecoveryRangesPg(const std::string& actualbasepath,
								  const std::string& newbasepath,
								  const std::string& retentionpath,
								  SV_UINT rpoThreshold,
								  ParameterGroup& availableRecoveryRangesPg, 
								  std::map<SV_ULONGLONG, CDPTimeRange> &recoveryRangeMap, 
								  const int& nRestoreCount ) ;
void FillCommonRestorePointInfo(ParameterGroup& commonrestoreinfoPg,
                                    std::map< std::string, std::map<SV_ULONGLONG, CDPEvent> >& volumeRestorePointsMap,
                                    std::map<std::string, std::map<SV_ULONGLONG, CDPTimeRange> >&  volumeRecoveryRangeMap) ;
bool getCDPEventPropertyVector(const std::string &retentionPath, std::vector<CDPEvent>& eventVector, int numberofevents) ;
bool getCommonRestorePoints(std::map< std::string, std::map<SV_ULONGLONG, CDPEvent> >& volumeRestorePointsMap, ParameterGroup& commonRestorePointsPg ,
							std::vector<std::string>& CmnRestorePntProperties) ;
bool GetRecoveryFeature(FunctionInfo& request, std::string& recoveryFeature) ;
bool GetConsistencyOptions(FunctionInfo& request, ConsistencyOptions& consistencyOptions) ;
bool GetRPOPolicy( FunctionInfo& request,std::string& recoveryFeature) ;
bool GetRetentionPolicyFromRequest(FunctionInfo& request, RetentionPolicy& retPolicy ) ;
bool FillRetentionDetailsFromPG( ParameterGroup& retWindowPG, std::string windowId, std::string durationType, std::string granularityType, ScenarioRetentionWindow& retenWindow ) ;
bool RemoveDir(const std::string& dir) ;
std::string BytesAsString(SV_ULONGLONG bytes, int precision=3) ;
std::string convertToHumanReadableFormat (SV_ULONGLONG requiredSpace);
void convertPath( const std::string& actualbp, const std::string& actualpath,  
				 const std::string& newbp, std::string& newpath);
void sanitizeWindowspath(std::string& path);
void sanitizeunixpath(std::string& path);
double GetSpaceMultiplicationFactorFromRequest (FunctionInfo& request); 
bool SpaceCalculation ( std::string repovolumename,std::list <std::string> &protectableVolumes , RetentionPolicy& retentionPolicy ,SV_ULONGLONG &requiredSpace , SV_ULONGLONG &existingOverHead , SV_ULONGLONG &repoFreeSpace ,double  spaceMultiplicationFactor,std::map <std::string, hostDetails> &hostDetails);

void GetRepoCapacityAndFreeSpace ( std::string &repovolumename , SV_ULONGLONG &capacity , SV_ULONGLONG &repoFreeSpace);

void GetFileSystemOccupiedSizeForVolumes (std::list <std::string> &protectableVolumes, 
										SV_ULONGLONG &fileSystemCapacity, SV_ULONGLONG &fileSystemFreeSpace);
void SpaceCheckForLocalHost (std::string repovolumename,std::list <std::string> &protectableVolumes , 
							 RetentionPolicy& retentionPolicy ,SV_ULONGLONG &requiredSpace , SV_ULONGLONG &existingOverHead , SV_ULONGLONG &repoFreeSpace , std::map <std::string,hostDetails> &hostDetailsMap) ;

void SpaceCheckForRemoteHosts(std::string &repositoryPath,SV_ULONGLONG &requiredSpace , SV_ULONGLONG &existingOverHead , 
							  SV_ULONGLONG &repoFreeSpace , std::map <std::string,hostDetails> &hostDetailsMap);
void fillHostDetails (const SV_ULONGLONG &capacity, const SV_ULONGLONG &totalFsOccupiedSize,
					  const SV_ULONGLONG &repoFreeSpace,const SV_ULONGLONG &requiredSpace, 
					  const SV_ULONGLONG &protectedVolumeOverHeadLocal, hostDetails &hostDetail);
void WriteIntoAuditLog (std::map <std::string,hostDetails> &hostDetailsMap);
std::string FormatSpaceCheckMsg(const std::string& apiname,
								const std::map <std::string,hostDetails>& hostDetailsMap) ;
bool checkRepositoryPathexistance (const std::string &repositoryPath);

std::string getLocalTimeString (); 
std::string addOneSecondToDate (std::string &year_s , std::string &month_s , std::string &date_s , std::string &hour_s , std::string &min_s , std::string &sec_s );
int isLeapYear (int yr);
void GetCapacityForVolumes (std::list <std::string> &protectableVolumes, SV_ULONGLONG &fileSystemCapacity); 
bool copyDir(boost::filesystem::path const & source,boost::filesystem::path const & destination );
bool SpaceCalculationV2 (	std::string repovolumename,
							std::list <std::string> &protectableVolumes , 
							RetentionPolicy& retentionPolicy ,
							SV_ULONGLONG &requiredSpace , 
							SV_ULONGLONG &existingOverHead , 
							SV_ULONGLONG &repoFreeSpace ,
							std::map <std::string,hostDetails> &hostDetailsMap,
							double compressionBenifits = 0.4, 
							double sparseSavingsFactor = 3 ,
							double changeRate = 0.03, 
							double ntfsMetaDataoverHead = 0.13); 

void SpaceCheckForLocalHostV2 (	std::string repovolumename,
								std::list <std::string> &protectableVolumes , 
								RetentionPolicy& retentionPolicy ,
								SV_ULONGLONG &requiredSpace , 
								SV_ULONGLONG &existingOverHead , 
								SV_ULONGLONG &repoFreeSpace , 
								std::map <std::string,hostDetails> &hostDetailsMap,
								bool supportSparseFile ,
								double compressionBenifits = 0.4, 
								double sparseSavingsFactor = 3 ,
								double changeRate = 0.03, 
								double ntfsMetaDataoverHead = 0.13); 

void SpaceCheckForRemoteHostsV2 (std::string &repositoryPath , 
								 SV_ULONGLONG &totalrequiredSpaceCIFS, 
								 SV_ULONGLONG &existingOverHeadCIFS , 
								 SV_ULONGLONG &repoFreeSpace , 
								 std::map <std::string,hostDetails> &hostDetailsMap, 
								 bool supportSparseFile ,
								 double compressionBenifits = 0.4, 
 								 double sparseSavingsFactor = 3,
								 double changeRate = 0.03, 
								 double ntfsMetaDataoverHead = 0.13
								); 

void constructVolumeResizeCDPCLICommand (const std::string &volumeName , 
										 std::string &resizeCommand); 

void constructStopFilteringCommand(const std::string &volumeName , 
										std::string &stopFilteringCmd ); 

bool executeVolumeResizeCDPCLICommand (const std::string &volumeName); 

bool executeStopFilteringCommand (const std::string &volumeName); 

std::string GetCdpClivolumeName(const std::string& sourceName ); 

bool SpaceCalculationForResizedVolumeV2 ( 	std::map <std::string,hostDetails> &hostDetailsMap );
bool CheckSpaceAvailabilityForResizedVolume(std::string volume,std::string& msg) ;


void validateResizedVolume (const std::string &voumeName);

DRIVER_STATE getStopFilteringStatus (const std::string &volumeName); 

void constructIsStopFilteringSuccess(const std::string &volumeName , 
								   std::string &isStopFilteringSuccess); 

bool GetIOParams(FunctionInfo& request, spaceCheckParameters& spacecheckparams) ;

SV_LONGLONG   getVirtualVolumeRawSize  (const std::string &virtualVolumeName) ; 

bool isVsnapsMounted( ) ; 

#endif
