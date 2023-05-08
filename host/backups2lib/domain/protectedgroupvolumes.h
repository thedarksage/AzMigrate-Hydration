//#pragma once

#ifndef PROTECTED_GROUP_VOLUMES__H
#define PROTECTED_GROUP_VOLUMES__H

#include <memory>
#include <list>
#include <iterator>
#include <cstddef>
#include <ctime>
#include <ace/Time_Value.h>
#include "entity.h"
#include "event.h"
#include "volumegroupsettings.h"
#include "compress.h" /*Included in the ProtectedGroupVolumes.h file because we need to have a member of ZCompress class.*/
#include "cdpvolume.h"
#include "file.h"
#include "boost/shared_array.hpp"
#include "boost/shared_ptr.hpp"
#include "devicefilterfeatures.h"
#include "validatesvdfile.h"
#include "compressmode.h"
#include "differentialsync.h"
#include "configwrapper.h"
#include "volumestream.h"
#include "filterdrvvolmajor.h"
#include "datacacher.h"
#include "runnable.h"
#include "volumereporterimp.h"
#include "genericprofiler.h"
#include "HealthCollator.h"
#include "json_writer.h"

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/gregorian/gregorian.hpp>
#include <boost/chrono.hpp>

#define STARTTS_MARKER "_S"
#define ENDTS_MARKER "_E"
#define PREVENDTS_MARKER "P"
#define TOKEN_SEP_FILE "_"

struct VOLUME_SETTINGS;

class ProtectedGroupVolumes;
typedef std::list <ProtectedGroupVolumes*> PROTECTED_GROUP_VOLUMES_LIST;
typedef std::list <VOLUME_SETTINGS> ENDPOINTS;
typedef ENDPOINTS::iterator ENDPOINTSITER;
typedef ENDPOINTS::const_iterator ENDPOINTSCONSTITER;

const char FINALPATHS_SEP = ';';

class DeviceStream;
class Event;
struct Compress;
struct Runnable;
struct Entity;
struct VOLUME_SETTINGS;
class VolumeChangeData;
class BaseTransportStream;
class TransportStream;
class MemoryStream;


class PeerAddress;
class CommBaseHandler;
class TransportHandle;

enum CHANGE_TIME_STAMP_POS
{
    FIRST_CHANGE=1,
    LAST_CHANGE=2
};
//typedef std::list<std::string> Checksums_t ;

class ProtectedGroupVolumes :
    public Entity, public Runnable
{
public:
	
    /// \brief constructor: called when using azure blob transport
    ProtectedGroupVolumes(const DATA_PATH_TRANSPORT_SETTINGS &settings,
        const DeviceFilterFeatures *pDeviceFilterFeatures, ///< device filter features (like per io time stamp support, diff ordering support etc,.)
        const char *pHostId,                               ///< host id 
        const DataCacher::VolumesCache *pVolumesCache,     ///< volumeinfocollector cache
        const size_t *pMetadataReadBufLen                  ///< read buffer length in metadata
        );

    virtual ~ProtectedGroupVolumes();

    CDP_DIRECTION GetProtectionDirection() const;
    const bool IsProtecting() const;
    int Protect();
    int StopProtection(const long int);
    bool operator==(const ProtectedGroupVolumes&);
    int Init();
    int Replicate();

    void SetSourceVolume(const Volume&);
    const std::string& GetSourceVolumeName() const;
    const std::string& GetSourceDiskId() const;
    const std::string& GetSourceDeviceName() const;
    SV_ULONGLONG GetSrcStartOffset(void) const;
    const std::string& GetRawSourceVolumeName() const;
    void SetRawSourceVolumeName(void);
    void SetTransport(void);
    int SendHeaderInfo(const unsigned long int&,const std::string&);
    int SendHeaderAndDirtyBlockInfo(const unsigned long int& ,const std::string& );

    int SendDirtyBlockInfo(const SV_LONGLONG&,const SV_ULONG&,const std::string&);
    int SendDirtyBlockHeaderOffsetAndLength(const SV_LONGLONG&,const SV_ULONG&,const std::string&);
    
    /** Added by BSR for per io time stamps **/
    int SendDirtyBlockInfoV2( const SV_LONGLONG& offset,
                              const SV_ULONG& Length,
                              const SV_ULONG& timeDelta,
                              const SV_ULONG& sequenceDelta,
                              std::string& sDestination ) ;
    /** Modified by BSR for per io timestamps **/
    int SendUserTags(const void*, const std::string&);
    int SendTimeStampInfo(const TIME_STAMP_TAG&,CHANGE_TIME_STAMP_POS,const std::string&);
    
    /** Added by BSR for per io timestamps **/
    int SendTimeStampInfoV2( const TIME_STAMP_TAG_V2&, CHANGE_TIME_STAMP_POS, const std::string& ) ;
    /** End of the change **/

    int SendSizeOfChangesInfo(const SV_LONGLONG&,const std::string&);

    typedef int (ProtectedGroupVolumes::*Transport_t)(const char*,const long int,const std::string&,bool,bool bCreateMissingDirs, bool bIsFull);
    int TransferDataToCX(const char*,const long int,const std::string&,bool,bool bCreateMissingDirs=false, bool bIsFull=false);
    int FlushDataToCx(const char*,const long int,const std::string&,bool,bool bCreateMissingDirs=false, bool bIsFull=false);
    int TransferThroughAccmulation(const char*,const long int,const std::string&,bool,bool bCreateMissingDirs=false, bool bIsFull=false);
    int TransferDirectly(const char*,const long int,const std::string&,bool,bool bCreateMissingDirs=false, bool bIsFull=false);

    /* PR # 6387 : START
     * 1. Changed TransferFileToCX to take TargetFile as arguement 
     * 2. Code to form TargetFile is moved in SetPreTargetDataFileName 
     * PR # 6387 : END */ 
    int TransferFileToCX(const std ::string & LocalFilename, 
					     const std ::string & TargetFile, 
					     const std ::string & RenameFile, 
                         const VolumeChangeData &Changes);
    /* PR # 6387 : START 
     * Added this function to form the Pre TargetFileName in Data File mode */
    int SetPreTargetDataFileName(
                                 VolumeChangeData& Changes,
                                 const std::string &LocalFilename, 
                                 std::string & PreTargetDataFileName,
                                 std::string & RenameFile
                                );
    /* PR # 6387 : END */
    virtual THREAD_FUNC_RETURN Run();
    virtual THREAD_FUNC_RETURN Stop(const long int PARAM=0);
    void Destroy();
    const bool IsCompressionEnabled() const;
    const bool IsThrottled();
    unsigned long int GetVolumeChunkSize();
    bool ReportIfResyncIsRequired(VolumeChangeData&);
    
    void CreateDiffSyncObject();
    void DestroyDiffSyncObj(void);

    bool IsResyncRequired();
    void ResetProtectionState();

    // allows the calling thread to update transport settings
    bool RefreshTransportSettings(DATA_PATH_TRANSPORT_SETTINGS&);

protected:
    const int ThrottleNotifyToDriver();
    bool SvdCheck(
                  std::string const & fileName, 
                  Checksums_t* checksums 
                 );
    
    bool SvdCheck(
        char *buffer,
        const size_t    bufferlen,
        std::string const & fileName,
        Checksums_t* checksums
        );

    void GetWaitForDBNotify();

    enum RemoteNameSetting { SET_FINAL_REMOTE_NAME, SET_PRE_REMOTE_NAME };
    bool CreateVolume();

	/// \brief opens the source volume to perform read
	///
	/// \return true if volume was opened else false
	bool OpenVolume(void);

	/// \brief Gets latest vic cache
	/// 
	/// \return None
	void GetLatestVolumesCacheIfPresent(void);

    bool SendTimestampsAndTags(VolumeChangeData & changes, std::string &preName, std::string &finalName);

    int CreateTransportStream();
    int OpenTransportStream();
    //int CloseTransportStream();
private:
    void SetDefaultNetworkTimeoutValues();
    WAIT_STATE WaitOnQuitEvent(long int seconds, long int milli_seconds = 0);
    const bool IsTagSent() const;
    const bool IsStopRequested() const;
    bool ShouldQuit() const;
	bool ShouldWaitForData(VolumeChangeData &);
	bool ShouldWaitForDataOnLessChanges(VolumeChangeData &);
    int ProcessTimeStampsOnly(VolumeChangeData&);
    int ProcessTagsOnly(VolumeChangeData&);
    int ProcessDataFile(VolumeChangeData&);
    int ProcessDataStream(VolumeStream&,VolumeChangeData&,const std::string&, const std::string&);
    int SendFileHeaders(VolumeChangeData&,unsigned long int,const SV_LONGLONG&,const std::string&);
    bool SetRemoteName(std::string & name, std::string & finalname, VolumeChangeData & changeData, unsigned conintuationId, bool isContinuationEnd);    
    int RenameRemoteFile(std::string const & preName, std::string const & postName);
    int FinishSendingData(std::string const & preName, std::string const & finalName, VolumeChangeData & changeData, unsigned conintuation, 
                          bool isContinuationEnd);
    ProtectedGroupVolumes(const ProtectedGroupVolumes&);
    ProtectedGroupVolumes();
    ProtectedGroupVolumes& operator=(const ProtectedGroupVolumes&);
    void ComputeChangesToSend(VolumeChangeData&,const unsigned long int&,unsigned long int&,SV_LONGLONG& );

    /* PR # 5926 : START
     * Added return type in this function for more error checking 
     * PR # 5926 : END 
     */
    int DeleteInconsistentFileFromCX(std::string const & fileName);

    WAIT_STATE WaitForData();
    int InitTransport();
    unsigned long GetIdleWaitTime();
    unsigned long int GetThresholdDirtyblockSize();    

    /* S2CHKENABLED : Initialize the members when s2 is runned with checks enabled : START */
    void InitializeMembersForS2Checks(void);
    /* S2CHKENABLED : END */

    /* S2CHKENABLED : Checks that need to be done in FinishSendingData if m_bS2ChecksEnabled is true : START */
    int ChecksInFinishSendingData(boost::shared_ptr<Checksums_t> &checksums,  const VolumeChangeData &Changes);
    /* S2CHKENABLED : END */

    /* S2CHKENABLED : Records the data after ranaming of remote file is succeeded : START */
    int RecordPrevDirtyBlockDataAfterRename( const VolumeChangeData & changeData );
	/* S2CHKENABLED : END */

    void PersistFileNamesAndCheckSums( const std::string & finalName,
                                       boost::shared_ptr<Checksums_t> &checksums );

    std::string SupportedFileName(
                                  VolumeChangeData& Changes,
                                  const std::string &LocalFilename 
                                 );

    /* S2CHKENABLED : Out of order and same file name checks : START */
    bool OutOfOrderAndSameFileNameChecks( const VolumeChangeData & Changes );
    /* S2CHKENABLED : END */

    bool VerifyPreviousEndTSAndSeq( const VolumeChangeData & Changes );

    /* S2CHKENABLED : Sets up file name for checks : START */
    bool SetUpFileNamesForChecks( const std::string & preName );
    /* S2CHKENABLED : END */

    /* S2CHKENABLED : Writes to check files in transfer data to cx : START */
    bool WriteToChkFileInTransferDataToCX(const char* sData,const long int uliDataLen);
    /* S2CHKENABLED : END */

    /* S2CHKENABLED : Log renamed files in data file mode : START */
    void LogRenamedFilesInDataFileMode(const std::string &RenameFile,
                                      boost::shared_ptr<Checksums_t> &checksums);
    /* S2CHKENABLED : END */

    /* S2CHKENABLED : runs svdcheck on local file in data file mode : START */
    bool SvdCheckOnDataFile(const std::string &LocalFilename, 
                            boost::shared_ptr<Checksums_t> &checksums, 
                            const VolumeChangeData &Changes);
    /* S2CHKENABLED : END */

    /* S2CHKENABLED : runs svdcheck on local file in data file mode : START */
    bool SvdCheckOnDataStream(
        char *buffer,
        const size_t bufferlen,
        const std::string &LocalFilename,
        boost::shared_ptr<Checksums_t> &checksums,
        const VolumeChangeData &Changes);
    /* S2CHKENABLED : END */
    
    /* S2CHKENABLED : closes files for checks : START */
    void CloseFilesForChecks(void);
    /* S2CHKENABLED : END */

    /* S2CHKENABLED : Uncompresses a compressed file : START */
    bool UncompressIfNeeded(std::string& localFile);
    /* S2CHKENABLED : END */

    /* S2CHKENABLED : Uncompresses a compressed file : START */
    void SetUpLogForAllRenamedFiles(void);
    /* S2CHKENABLED : END */

    std::string GetFinalNameFromPreName(std::string const & preName);
    std::string WriteOrderTag(bool isContinuationEnd, VolumeChangeData &changeData);
	int persistChecksums( Checksums_t& checksums, std::string differentialFileName ) ;
	int setupChecksumStore( const std::string& sourceVolumeName ) ;
    int deletePrevFileIfReq( const std::string& preName );
    int ProcessLastIOATLUNRequest(void);
    int StartFiltering(void);
    void SetUpProfilerLog(void);
    void SetUpDiffsRateProfilerLog(void);
    void Profile(const std::string &diff, VolumeChangeData &Changes);
    void PrintDiffsRate(void);
    void PrintDiffsRateEntry(const time_t &now);
    void ResetPerDiffProfilerValues(void);
    void PrintEndPoints(void);
    void SetFinalPaths(void);
    SV_ULONG GetDiffFileSize(VolumeChangeData& Changes,const SV_LONGLONG& SizeOfChangesToSend);
    SV_ULONG GetStreamDataSize(VolumeChangeData& Data);
    int GetHeaderInfoSize(void);
    int GetTimeStampInfoV2Size(void);
    int GetTimeStampInfoSize(void);
    int GetUserTagsSize(const void* pDirtyBlock);
    int GetSizeOfChangesInfoSize(void);
    bool IsTransportProtocolMemory(void) const;
    int DeleteSentDiffFile(const std::string &sentfile);

    /// \brief handle the resync in the data path by causing OOD
    /// stop filtering will reset the prev timestamp to 0
    int StopFilteringOnResync();

    void WaitOnErrorRetry(int rateFactor, uint64_t counter);
    void WaitOnNetworkErrorRetry(int rateFactor, uint64_t& counter, VolumeChangeData& volumeChangeData);
    void ResetErrorCounters();
    bool UpdateTransportSettings();
    void DecreaseNetworkTimeout();

    void LogDatapathThrottleInfo(const std::string& msg, const SV_ULONGLONG &ttlmsec);
    void HandleDatapathThrottle(const ResponseData &rd, const DATA_SOURCE &SourceOfData);
    void HandleTransportFailure(VolumeChangeData& Changes, const DATA_SOURCE &SourceOfData);
    void InitHealthCollator();
    void SetHealthIssue(const std::string& healthIssueCode);
    bool ResetThrottleHealthIssue();

private:
    bool m_bNeedToRenameDiffs;
    //bool m_bCompletedSendingDirtyBlock;
    unsigned m_continuationId;
    //int m_uiCommittedChanges;
    long int m_liQuitInfo;              //stores the reason for quitting. refer
    std::string m_sProtectedVolumeName; //stores name of the protected volume.
    std::string m_sProtectedDiskId; //stores protected disk id as viewed by control plane.
    std::string m_sRawProtectedVolumeName; //stores name of the raw protected volume.
    std::string m_sProtectedDiskDeviceName; //stores name of the device as represented in OS
    bool m_bProtecting;                 //stores protection status, whether is being protected?
    COMPRESS_MODE m_compressMode;       // sotres compress mode that is in affect
    bool m_bTagSent;                    //specifies if tags are sent for the dirty block being processed
    CDP_DIRECTION m_Direction;          //specifies direction of protection
    CDP_STATUS m_Status;                
    cdp_volume_t::Ptr m_pProtectedVolume;         //pointer to the volume object to maintain state information of protected volume.
    Event m_ProtectEvent;               //used to signal start and stop of protection
    std::string m_TgtVolume;
    SV_ULONGLONG m_SrcCapacity;
    CDP_SETTINGS m_CDPSettings;
    SV_ULONGLONG m_drainLogTransferedBytes; //Bytes
    boost::mutex    m_DBNotifyMutex;
#ifdef SV_WINDOWS
    Event m_DBNotifyEvent;              //signalled by the filter driver when a dirty block is available for processing
#endif
    bool CheckVolumeStatus(int &error, std::string &errorMsg);
    int CreateVolumeStream();

	/// \brief Allocates volume stream object
	/// 
	/// \return SV_SUCCESS if the stream was created else SV_FAILURE
	int AllocateVolumeStream(void);
	
	/// \brief creates device stream for interaction with filter driver
    int CreateDeviceStream(const std::string &driverName); ///< driver name

    VolumeStream::Ptr m_pVolumeStream;  //pointer to the used to perform read on the protected volume
    Compress* m_pCompress;              //used to perform in memory compression of data when source side compression is specified
    boost::shared_ptr<BaseTransportStream> m_pTransportStream;
    DeviceStream* m_pDeviceStream;      //points to the device stream used to interact with the filter driver
    TRANSPORT_CONNECTION_SETTINGS m_TransportSettings;
    bool m_WaitForDBNotify;             //
    unsigned long m_IdleWaitTime;
    boost::shared_ptr<char> m_dataPtr;
    long m_dataBufferLength;
    long m_liTotalLength;
    unsigned long int m_VolumeChunkSize;
    unsigned long int m_ThresholdDirtyblockSize;
    SV_ULONGLONG m_VolumeSize;
    const DeviceFilterFeatures *m_pDeviceFilterFeatures; ///< pointer to device filter features
    const char *m_pHostId;
    SV_ULONGLONG m_SrcStartOffset;
	const DataCacher::VolumesCache *m_pVolumesCache; ///< vic cache to be used to instantiate volume and supply its details to driver
	DataCacher::VolumesCache m_VolumesCache;         ///< vic cache to be used for retries in case the cache supplied externally is incorrect
    const size_t *m_pMetadataReadBufLen;

    /* profiles */
    ACE_Time_Value m_TimeBeforeGetDB;
    ACE_Time_Value m_TimeAfterCommit;
    unsigned long m_DiffSize;
    unsigned long m_CompressedDiffSize;
    ACE_Time_Value m_TimeForTransport;
    FILE *m_fpProfileLog;

    std::string m_FinalPaths;
    /* Added to check if got already sent and committed filename from driver
     * to once again send */
    Transport_t m_Transport[NBOOLS];
	SV_LONGLONG m_BytesToDrainWithoutWait;
   
    /* differential rate profiler variables */
    bool m_bProfileDiffsSize;
    FILE *m_fpDiffsRateProfilerLog;
    SV_ULONGLONG m_TotalDiffsSize;
    SV_ULONGLONG m_TotalCompressedDiffsSize;
    time_t m_LastDiffsRatePrintTime;

	FilterDrvVol_t m_SourceNameForDriverInput; ///< source volume name to be used in driver ioctls

    TRANSPORT_PROTOCOL m_transportProtocol;
	DATA_PATH_TRANSPORT_SETTINGS m_datapathTransportSettings;
    unsigned long m_rpoThreshold;
    VolumeReporter::VolumeReport_t m_VolumeReport;

    bool m_resyncRequired;

    //:For DR Metrics Collection, Profiler object
    Profiler m_DiskReadProfiler;//for Disk Read profling
    Profiler m_DiffFileUploadProfiler;//for Network Transmission profiling

    uint64_t m_driverFailureCount;
    uint64_t m_diskReadFailureCount;
    uint64_t m_networkFailureCount;
    uint64_t m_memAllocFailureCount;

    uint32_t m_networkFailureCode;

    // flag to notify when the transport settings change
    bool            m_bTransportSettingsChanged;
    boost::mutex    m_mutexTransportSettingsChange;

    SV_ULONGLONG m_networkTimeoutFailureCount;
    SV_ULONGLONG m_networkTimeoutFailureMinimumTimeout;
    SV_ULONGLONG m_networkTimeoutFailureCurrentTimeout;
    SV_ULONGLONG m_networkTimeoutFailureMaximumTimeout;
    boost::chrono::time_point<boost::chrono::steady_clock> m_networkTimeoutResetTimestamp;
    SV_UINT m_networkTimeoutResetInterval;

    SV_ULONGLONG m_prevDatapathThrottleLogTime;
    SV_UINT m_datapathThrottleCount;
    SV_ULONGLONG m_cumulativeTimeSpentInThrottleInMsec;
    //\HealthCollator
    HealthCollatorPtr m_healthCollatorPtr;
    bool m_bThrottled;
    std::string m_strThrottleHealthCode;

public:
    std::string previousSentName ;
    bool m_bS2ChecksEnabled;
    bool m_bDICheckEnabled;
    bool m_bSVDCheckEnabled;
    bool m_bHasToCheckTS;
    SV_ULONGLONG prevSentTSFC,
        prevSentTSLC,
        prevSentSEQNOFC,
        prevSentSEQNOLC ;
    SV_ULONG prevSentSeqIDForSplitIO;
    stTimeStampsAndSeqNos_t tsAndSeqnosFile;
    FILE *fpRenamedSuccessfully;
    std::string CurrDiffFileName ;
    FILE *m_fpChecksumFile ;
    boost::shared_ptr<DifferentialSync> m_DiffsyncObj;
    HOST_VOLUME_GROUP_SETTINGS::volumeGroups_t m_EndPointVolGrps;
    SV_ULONGLONG GetDrainLogTransferedBytes();
    FilterDrvVol_t GetSourceNameForDriverInput();
};


#endif
