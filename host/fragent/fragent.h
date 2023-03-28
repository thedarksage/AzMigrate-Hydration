/*
 * fragent.h
 *
 * This file consists of the functionality decalrations that implements file
 * replication services
 *
 * Author				:
 * Revision Information	:
 */

#ifndef FRAGENT__H
#define FRAGENT__H

/************************************Include header files*********************/

#ifdef SV_WINDOWS

#include "../common/hostagenthelpers_ported.h"
#include "../common/portable.h"

#else

#include "../fr_common/hostagenthelpers_ported.h" /* for sv_host_agent_params*/
/*size dependency on CProcess because we call the destructor inline*/
#include "../fr_common/portable.h"

#endif

#include <sstream>
#include <vector>
#include <map>
#include <string>

#include <boost/filesystem/path.hpp>

/*******************************Constant declarations*************************/

/***************************** Global declarations****************************/

/***************************** Structure Declarations*************************/

struct IFileReplicationAgent
{
    virtual SVERROR Init( char const* pszPathname ) = 0;
    virtual SVERROR Shutdown() = 0;
    virtual SVERROR Run() = 0;
};


typedef struct StandByCx_T
{
    char szStandbyServerName[ 260 ];
    char szStandbyNATServerName[260];
    SV_ULONG szStandbyServerHTTPPort;
    SV_ULONG szStandbyServerTimeout;
    SV_ULONG szStandbyServerActive;

}StandByCx;


/*****************************Non-Member Function Declarations****************/


/*****************************Class Declarations******************************/

/*
 * CFileReplicationAgent
 *
 * This class encapsulates the file replication data structures and
 * functionality. It provides an implementaion for the IFileReplicationAgent
 * interface.
 */

class CFileReplicationAgent : public IFileReplicationAgent
{
public:
    //
    // Construction
    //
    CFileReplicationAgent();
    virtual ~CFileReplicationAgent(){ }

public:
    //
    // IFileReplicationAgent methods
    //
    SVERROR Init( char const* InmpszConfigPathname );
    SVERROR Shutdown();
    SVERROR Run();

public:
    /* Structures*/
    enum JOB_STATE { JOB_STATE_NOT_STARTED, JOB_STATE_STARTED,
                     JOB_STATE_STOPPED, JOB_STATE_LOG_UPDATE };
    enum JOB_DIRECTION { JOB_DIRECTION_PUSH, JOB_DIRECTION_PULL };
    enum EXIT_REASON { EXIT_NONE=1, EXIT_USER_CANCEL, EXIT_PROCESS_FINISHED };

    struct FRJOBCONFIG
    {
	SV_ULONG JobId;
	SV_ULONG JobConfigId;
	char szSourceHostId[ SV_MAX_PATH ];
	char szSourcePath[ SV_MAX_PATH ];
	char szDestHostId[ SV_MAX_PATH ];
	char szDestPath[ SV_MAX_PATH ];
	JOB_STATE DaemonState;
	JOB_STATE AssignedState;
	JOB_STATE CurrentState;
	SV_LONGLONG LogOffset;
	JOB_DIRECTION Direction;
	char szPreSourceCommand[ SV_MAX_PATH ];
	char szPostSourceCommand[ SV_MAX_PATH ];
	char szPreTargetCommand[ SV_MAX_PATH ];
	char szPostTargetCommand[ SV_MAX_PATH ];
	SV_INT CpuThrottle;
	EXIT_REASON ExitReason;
	int ExitCode;
	struct SSHOPTIONS sshOptions;
	struct OPTIONS {
            enum { RECURSIVE=1<<0, COPY_WHOLE_FILES=1<<1, BACKUP_EXISTING=1<<2,
                   COMPRESS=1<<3, HANDLE_SPARSE_FILES=1<<4, UPDATE_ONLY=1<<5,
                   UPDATE_EXISTING_ONLY=1<<6, IGNORE_EXISTING=1<<7,
                   IGNORE_TIMESTAMP=1<<8, ALWAYS_CHECKSUM=1<<9,
                   CHECK_SIZE_ONLY=1<<10, DELETE_NON_EXISTING=1<<11,
                   DELETE_EXCLUDED=1<<12, DELETE_AFTER=1<<13,
                   DELETE_ON_ERRORS=1<<14,FORCE_DELETION=1<<15,
                   KEEP_PARTIAL=1<<16, RETAIN_SYM_LINKS=1<<17,
                   COPY_LINK_REFERENTS=1<<18, COPY_UNSAFE_LINKS=1<<19,
                   COPY_SAFE_LINKS=1<<20, PRESERVE_HARD_LINKS=1<<21,
                   PRESERVE_PERMISSIONS=1<<22, PRESERVE_OWNER=1<<23,
                   PRESERVE_GROUP=1<<24, PRESERVE_DEVICES=1<<25,
                   PRESERVE_TIMES=1<<26, DONT_CROSS_FILE_SYSTEM=1<<27,
                   BLOCKING_IO=1<<28, NONBLOCKING_IO=1<<29, PRINT_STATS=1<<30,
                   PROGRESS=1<<31 };

            SV_ULONG Flags;
            SV_ULONG Port;
            SV_ULONG IpAddress;
            char szTempDirectory[ SV_MAX_PATH ];
            char szBackupDirectory[ SV_MAX_PATH ];
            char szBackupSuffix[ SV_MAX_PATH ];
            SV_ULONG BlockSize;
            SV_ULONG ModifyWindow;
            char szExcludePattern[ 8*SV_MAX_PATH ];
            char szIncludePattern[ 8*SV_MAX_PATH ];
            char szExcludeFrom[ SV_MAX_PATH ];
            SV_ULONG BandwidthLimit;
            SV_ULONG IoTimeout;
            SV_ULONG Verbosity;
            char szLogFormat[ SV_MAX_PATH ];
            char szCatchAll[ SV_MAX_PATH ];
	} options;
    };

    struct FRJOB
    {
	SV_ULONG JobId;
	SV_ULONG JobConfigId;
	JOB_STATE CurrentState;
	JOB_STATE PendingCurrentState;
	JOB_DIRECTION Direction;
	enum FR_PROCESS_TYPE { FR_INVALID, FR_DAEMON, FR_CLIENT,
                               FR_LOCAL_COPY  } ProcessType;
	CProcess* pProcess;
	CProcess* tProcess;
	SV_LONGLONG LogOffset;
	SV_LONGLONG CurrentLogUploaded;
	SV_LONGLONG PendingLogUploaded;
	char szPostCommand[ SV_MAX_PATH ];
	bool bUserTerminated;
	FRJOBCONFIG Config;
	FRJOB() { pProcess = NULL; tProcess = NULL ; }
	~FRJOB(){ }
    };

protected:
    //
    // Helper methods
    //
    SVERROR GetFileReplicationAgentConfiguration();
    SVERROR ActOnFileReplicationConfigurationChanges();
    SVERROR ActOnDeletedFileReplicationConfiguration();
    SVERROR UpdateFileReplicationAgentStatus();
    SVERROR UpdateDaemonJobLog(int jobId);
    void setCxpsInfo(char const* tag, char const* value);
    std::string escapeSpacesInPath(char const* path);
    void parseCxpsInfoIfPresent(char*& buffer);
    void getFxAllowedDirsPath(boost::filesystem::path& fxPath);
    void createFxAllowedDirsFile();
    void removeFxAllowedDirsFiles();
    SVERROR ParseFileReplicationConfiguration( char* InmpszBuffer );
    SVERROR DeleteLogFiles();
    void RemoveStoppedJobs();
    void RemoveStoppedDaemonJobs();
    SVERROR GetStandByCxDetails(std::vector<StandByCx> *myvector,std::string cxip="",SV_ULONG passiveCxPort=0);
    SVERROR CheckPrimaryServerHearbeat(SV_ULONG);
    SVERROR VerifyingPrimaryAndStandByCXStatus(void);
    SVERROR StopAllFxJobs();
    void removeRepeatingSlashBackSlash(std::string& str);
    void removeRepeatingSlashBackSlash(char* str, const size_t strlen);
    SVERROR UpgradeFXInformation();

protected:
    //
    // Helper methods
    //
    FRJOBCONFIG const* GetJobConfig( SV_ULONG dwJobId ) const;
    SVERROR StartJob( FRJOBCONFIG & JobConfig );
    void StopJob( FRJOB& job, bool needToStopProcess = true ); 
    void StopDaemonJob( FRJOB& job );
    void getRunningDaemonJobsList();
    void getRunningJobList();
    void DiscardJob(SV_ULONG JobId );
    void DiscardDaemonJob(SV_ULONG JobId );
    bool FindDuplicateConfigJob(SV_ULONG JobConfigId );
    bool FindDuplicateJob(SV_ULONG JobId );
    SVERROR StartDaemon();
    SVERROR StartDaemonJob( FRJOBCONFIG & JobConfig );
    SVERROR StopDaemon();
    FRJOB const* FindJob( SV_ULONG JobId ) const;
    FRJOB const* FindDaemonJob( SV_ULONG JobId ) const;

    FRJOB::FR_PROCESS_TYPE GetProcessType( FRJOBCONFIG const& JobConfig ) const;

    bool ShouldStartJob( FRJOBCONFIG & JobConfig );
    bool ShouldStopJob( FRJOB const& Job ) const;
    bool ShouldStopDaemonJob( FRJOB const& Job ) const;
    bool IsDaemonStarted() const {
        return( JOB_STATE_STARTED == m_Daemon.CurrentState );
    }
    bool IsDaemonRequired() const {
        for( int i = 0; i < m_cJobConfig; i++ )
        {
            if( FRJOB::FR_DAEMON == GetProcessType( m_FRJobConfig[i] ) ) {
                return( true );
            }
        }
        return( false );
    }
    void UpdateOption( FRJOBCONFIG& JobConfig, char const* pszOptionTag,
                       char const* pszParam );
    void GetinmsyncOptions( std::string& optionsAsString,
                            FRJOBCONFIG::OPTIONS const& options,
                            bool exp,
                            SV_ULONG jobid,
                            char const* destHostId,
                            char const* secureMode);
    SVERROR StartOFM();
    SVERROR RunScriptCommand( char const
                              (FRJOBCONFIG::*pszCommand) [SV_MAX_PATH],
                              FRJOBCONFIG const& JobConfig,
                              char const* pszLogFilename, FRJOB * pJob = NULL,
                              SV_ULONG* pExitCode = NULL );
    SVERROR CreateIncludeExcludeFile( FRJOBCONFIG const& JobConfig );
    SVERROR updateJobStatus(SV_ULONG jobId);
    SVERROR getJobExitCode(SV_ULONG jobId, SV_ULONG jobInstanceId, int &exitCode);
    SVERROR RegisterFXIfNeeded();
    SVERROR persistRemoteCXInfo(const char* pszRemoteCXInfo);
    SVERROR loadSavedRemoteCXDetails();
    void setAgentInstallPath();

protected:
    /*Transport helper methods*/
    SVERROR GetFromServer( const char* pszHost, SV_INT HttpPort,
                           const char* pszUrl, char** ppszBuffer ) const;
    SVERROR PostToServer( const char* pszHost, SV_INT HttpPort,
                          const char* pszUrl, const char* pszBuffer, SV_ULONG BufferLength,bool response=false,int *ResponseCodeFromCx=0);


private:
    SVERROR update_known_hosts( FRJOBCONFIG );
    SVERROR update_authorized_keys(FRJOBCONFIG &);
    SVERROR generate_key_pair( FRJOBCONFIG&);
    SVERROR getPublicKeys(FRJOBCONFIG&);
    SVERROR setPublicKey(FRJOBCONFIG);
    SVERROR RemPubKeyFromAuthKeysFile( SV_ULONG, const char*, const char* );
    SVERROR cleanup_server_keys(SV_ULONG, SV_ULONG);
    SVERROR cleanup_client_keys(SV_ULONG);
    SVERROR delete_encryption_keys( SV_ULONG );
    /* File Replication Agent state*/
    SV_HOST_AGENT_PARAMS m_sv_host_agent_params;
    enum { MAX_JOBS = 64 };
    int m_cJobConfig;	/* one per job that has us as a source, target, or both*/
    int m_cJobs;	/* one per job that is not a daemon*/
    int m_cDaemonJobs;	/* one for every job that is of type daemon*/

    FRJOBCONFIG m_FRJobConfig[ MAX_JOBS ];
    FRJOB m_FRJob[ MAX_JOBS ];
    FRJOB m_FRDaemonJob[ MAX_JOBS ];
    FRJOB m_Daemon;
    SV_ULONG m_LastChecksum;
    char const* m_pszConfigPathname;
    CDispatchLoop* m_pDispatcher;
    SV_ULONG m_WaitDelay;
    std::vector<StandByCx> m_RemoteCxBackupVector;

    std::string m_cxpsPort;
    std::string m_cxpsSslPort;
    std::string m_cxpsUser;
    std::string m_cxpsPassword;
    std::string m_cxpsAllowDirs;
    std::string m_cxpsClientPem;

    std::string m_agentInstallPath;
};



#endif // FRAGENT__H
