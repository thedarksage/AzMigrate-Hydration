/*
 * fragent.cpp
 *
 * This file consists of the functionality that implements file replication
 * services
 *
 * Author    :
 * Revision Information :
 */

/************************************Include header files*********************/
#include <assert.h>
#include <strstream>
#include <sstream>
#include <fstream>
#include <list>
#include <stdio.h>
#include <time.h>
#include <curl/curl.h>
#include <curl/easy.h>
#include <iostream>
#include <stdlib.h>
#include <cctype>
#include "fragent.h" /*This is the File Replication Agent definition file*/
#include "inmsafecapis.h"
#ifdef SV_WINDOWS
#include "../common/hostagenthelpers_ported.h"
#include "../Log/logger.h"
#else
#include "../fr_common/hostagenthelpers_ported.h"
#include "../fr_log/logger.h"
#endif
#include <string>
#include "svutils.h"
#ifndef SV_WINDOWS
#include <unistd.h>
#endif

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

/*********************************Defining Namespace**************************/

using namespace std;

/*******************************Constant declarations*************************/

#define OPTION_FLAG(o,s)  {#o, (SV_ULONG)CFileReplicationAgent::FRJOBCONFIG::OPTIONS::o, \
            &CFileReplicationAgent::FRJOBCONFIG::OPTIONS::Flags, 0, 0, s }
#define OPTION_DWORD(o,s) {#o, 0, &CFileReplicationAgent::FRJOBCONFIG::OPTIONS::o, \
            0, 0, s }
#define OPTION_STR(o,s)   {#o, 0, 0,&CFileReplicationAgent::FRJOBCONFIG::OPTIONS::sz \
            ##o, SV_MAX_PATH, s }

const char JOB_LOG_PATTERN[]                = "inmage_job*.log";

const char INMAGE_JOB_EXCLUDE_FILENAME[]    = "inmage_job_";
const char JOB_EXCLUDE_PATTERN[]            = "inmage_job*.exclude";
int const USER_TERMINATED_EXIT_CODE         = -255;
bool bCX_Not_Reachable = false;
#ifndef SV_WINDOWS
//bool bCX_Not_Reachable=false;
#endif


extern SSH_Configuration ssh_config;
extern bool bEnableEncryption;
extern int RegisterHostIntervel;
time_t lastRegisterHostTime =0;
/***************************** Global declarations****************************/

/* Registry variable*/
char SV_ROOT_KEY_LOCAL[]     = "SOFTWARE\\SV Systems";
char* AppendSlashIfNotPresent( char* InmpszPath );
SVERROR AppendStartTimeToFile( char const* InmpszFilename,
                               char const* InmpszHeaderLine );

std::list<int> JobIdVector;
std::list<int> RunningDemonJobList;
std::list<int> RunningJobList;

/***************************** Structure Declarations*************************/

struct MemoryStruct
{
    char *memory;
    size_t size;
};

static const struct
{
    char const* tag;
    SV_ULONG Flag;
    SV_ULONG CFileReplicationAgent::FRJOBCONFIG::OPTIONS::* pNumber;
    char (CFileReplicationAgent::FRJOBCONFIG::OPTIONS::* psz)[ SV_MAX_PATH ];
    int cch;
    char const* pszOption;
} s_rgDispatcher[] =
{
//    OPTION_DWORD(   Port,           "--port=" ),
//    OPTION_DWORD(   IpAddress,      NULL ),  // client doesn't use this option
    OPTION_STR(     TempDirectory,  "--temp-dir=" ),
    OPTION_STR(     BackupDirectory,"--backup-dir=" ),
    OPTION_STR(     BackupSuffix,   "--suffix=" ),
    OPTION_DWORD(   BlockSize,      "--block-size=" ),
    OPTION_DWORD(   ModifyWindow,   "--modify-window=" ),
    OPTION_STR(     ExcludeFrom,    "--exclude-from=" ),
    OPTION_DWORD(   BandwidthLimit, "--bwlimit=" ),
    OPTION_DWORD(   IoTimeout,      "--timeout=" ),
    OPTION_DWORD(   Verbosity,      NULL ),  // requires special handling
    OPTION_STR(     LogFormat,      "--log-format=" ),
    OPTION_STR(  CatchAll,  "" ),

    OPTION_FLAG(    RECURSIVE,          "-r" ),
    OPTION_FLAG(    COPY_WHOLE_FILES,   "-W" ),
    OPTION_FLAG(    BACKUP_EXISTING,    "-b" ),
    OPTION_FLAG(    COMPRESS,           "-z" ),
    OPTION_FLAG(    HANDLE_SPARSE_FILES,"-S" ),
    OPTION_FLAG(    UPDATE_ONLY,        "-u" ),
    OPTION_FLAG(    UPDATE_EXISTING_ONLY,"--existing" ),
    OPTION_FLAG(    IGNORE_EXISTING,    "--ignore-existing" ),
    OPTION_FLAG(    IGNORE_TIMESTAMP,   "-I" ),
    OPTION_FLAG(    ALWAYS_CHECKSUM,    "-c" ),
    OPTION_FLAG(    CHECK_SIZE_ONLY,    "--size-only" ),
    OPTION_FLAG(    DELETE_NON_EXISTING,"--delete" ),
    OPTION_FLAG(    DELETE_EXCLUDED,    "--delete-excluded" ),
    OPTION_FLAG(    DELETE_AFTER,       "--delete-after" ),
    OPTION_FLAG(    DELETE_ON_ERRORS,   "--ignore-errors" ),
    OPTION_FLAG(    FORCE_DELETION,     "--force" ),
    OPTION_FLAG(    KEEP_PARTIAL,       "--partial" ),
    OPTION_FLAG(    RETAIN_SYM_LINKS,   "-l" ),
    OPTION_FLAG(    COPY_LINK_REFERENTS,"-L" ),
    OPTION_FLAG(    COPY_UNSAFE_LINKS,  "--copy-unsafe-links" ),
    OPTION_FLAG(    COPY_SAFE_LINKS,    "--safe-links" ),
    OPTION_FLAG(    PRESERVE_HARD_LINKS,"-H" ),
#ifdef WIN32
    OPTION_FLAG(    PRESERVE_PERMISSIONS,"-A" ),
#else
    OPTION_FLAG(    PRESERVE_PERMISSIONS,"-p" ),
#endif
    OPTION_FLAG(    PRESERVE_OWNER,     "-o" ),
    OPTION_FLAG(    PRESERVE_GROUP,     "-g" ),
    OPTION_FLAG(    PRESERVE_DEVICES,   "-D" ),
    OPTION_FLAG(    PRESERVE_TIMES,     "-t" ),
    OPTION_FLAG(    DONT_CROSS_FILE_SYSTEM,"-x" ),
    OPTION_FLAG(    BLOCKING_IO,        "--blocking-io" ),
    OPTION_FLAG(    NONBLOCKING_IO,     "--no-blocking-io" ),
    OPTION_FLAG(    PRINT_STATS,        "--stats" ),
    OPTION_FLAG(    PROGRESS,           "--progress" )
};

/*******************************Non-Member Function Definitions****************/
/*
 * AppendSlashIfNotPresent
 *
 * Returns the original string with a slash appended, unless slash is
 * the last character. Auto-includes a newline character.
 * Requires that the string have at least one character, or null is
 * returned.
 *
 * pszPath --> the stirng to which slash is to be appended.
 *
 * Return pszPath, the string with appended slash.
 */

char* AppendSlashIfNotPresent( char* InmpszPath )
{
    if(NULL == InmpszPath || 0 == InmpszPath[ 0 ])
    {
        return( NULL );
    }
    else {
        size_t cch = strlen( InmpszPath );
        if( '/' != InmpszPath[ cch-1 ] ) {
            InmpszPath[ cch ]   = '/';
            InmpszPath[ cch+1 ] = 0;
        }
    }

    return( InmpszPath );
}

/*
 * AppendStartTimeToFile
 *
 * Appends a line (containing a %c for timestamp) to a log file.
 * Auto-includes a newline character.
 *
 * pszFilename --> The log file path of the current job.
 *
 * pszHeaderLine --> The header string for the file.
 *
 * Returns SVFAIL on error.
 *
 */
SVERROR AppendStartTimeToFile( char const* InmpszFilename, char const* InmpszHeaderLine )
{
    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
    SVERROR rc;

    FILE* pFile = NULL;

    if( NULL == InmpszFilename )
    {
        rc = SVE_INVALIDARG;
        return rc;
    }
    else
    {
        pFile = fopen( InmpszFilename, "a" );

        if( NULL == pFile )
        {
            DebugPrintf(SV_LOG_ERROR, "Failed to Open the file %s @LINE %d for writing\n", InmpszFilename, __LINE__);
            rc = errno;
        }
        else
        {
            char lInmszLine[ SV_MAX_PATH ];
            time_t lInmcurrentTime;
            time( &lInmcurrentTime );
            struct tm* now = localtime( &lInmcurrentTime );

            strftime( lInmszLine, sizeof( lInmszLine ), InmpszHeaderLine, now );
            fprintf( pFile, "%s\n", lInmszLine );
        }
        SAFE_FCLOSE( pFile );
    }

    DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
    return( rc );
}

/*
 * IncludeQuotesIfRequired
 *
 * Appends quotes to the pre/post script command.
 *
 * Return string with quotes.
 *
 */
char * IncludeQuotesIfRequired(char* InmpszCommand)
{
    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
    static char lInmszCommandLine[ 4*SV_MAX_PATH ];
    char *ptr = InmpszCommand;
    int i = 0;
    if (*ptr == '\0')
        return InmpszCommand;
    if (SVIsFilePathExists(InmpszCommand))
		inm_sprintf_s(lInmszCommandLine, ARRAYSIZE(lInmszCommandLine), "\"%s\"", InmpszCommand);
    else
        while( *ptr ) {
            if ( *ptr == '"') {
                lInmszCommandLine[i] = '\0';
                inm_strcat_s(lInmszCommandLine,ARRAYSIZE(lInmszCommandLine),ptr);
                break;
            }
            else {
                lInmszCommandLine[i++] = '"';
                while (*ptr != ' ' && *ptr != '\0' ) {
                    lInmszCommandLine[i++] = *ptr;
                    ptr++;
                }
                lInmszCommandLine[i++] = '"';
                lInmszCommandLine[i++] = ' ';
                while (*ptr == ' ') ptr++;
            }
            if (*ptr == '\0' )
                lInmszCommandLine[i] = '\0';
        }
    DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
    return lInmszCommandLine;
}
/*
 * IsUNCPath
 *
 * Checks whether the given path is UNC or not.
 *
 * Returns the boolean value
 *
 */
bool IsUNCPath (char *InmszPath)
{
    if (InmszPath[0] == '\\' && InmszPath[1] == '\\')
    {
        char * ptr = InmszPath;
        while (*ptr)
        {
            if (*ptr == '/')
                *ptr = '\\';
            ptr++;
        }
        return true;
    }
    else
        return false;
}

//This function for getting Dirname.It is specific to Fx agent
#ifdef SV_WINDOWS

SVERROR DeleteOlderMatchingFiles( char const* pszPath, char const* pszPattern, time_t timestamp )
{
    /* place holder function */
    return SVS_OK;
}
#endif


/********************************Member Function Definitions*************************/

/*
 * CFileReplicationAgent::CFileReplicationAgent
 *
 * This function is the constructor of CFileReplicationAgent Class.
 * All the required data members are initialized here.
 *
 * Return None
 *
 */

CFileReplicationAgent::CFileReplicationAgent()
{
    ZeroFill( m_sv_host_agent_params );
    m_cJobConfig = 0;
    m_cJobs = 0;
    m_cDaemonJobs = 0;
    ZeroFill( m_FRJobConfig );
    ZeroFill( m_FRJob );
    ZeroFill( m_FRDaemonJob );
    ZeroFill( m_Daemon );
    m_pszConfigPathname = NULL;
    m_LastChecksum = 0;
    m_pDispatcher = NULL;
    m_WaitDelay = WORKER_REINCARNATION_TIME;
}

/*
 * CFileReplicationAgent::Init
 *
 * This function initializes the File Replication Agent
 *
 * pszConfigPathname config file path name.
 *
 * Return Value SVS_OK
 */

SVERROR CFileReplicationAgent::Init( char const* InmpszConfigPathname )
{
    m_pszConfigPathname = InmpszConfigPathname;
    setAgentInstallPath();
    SetLoggerAgentType(FxAgentLogger);

    return( SVS_OK );
}


#if 1

SVERROR CFileReplicationAgent:: StopAllFxJobs()
{

    SVERROR rc = SVS_OK;

    SV_HOST_AGENT_PARAMS service_params;
    const char * configPath = NULL;
    DebugPrintf( "ENTER StopAllFxJobs()\n" );

    ReadAgentConfiguration(&service_params,m_pszConfigPathname,SV_AGENT_FILE_REPLICATION );



    string lInmcomplete_url;
    string lInmpost_buffer("hostid=");

    string szUpdateJobStateUrl = "/update_file_replication_status.php";

    lInmcomplete_url += szUpdateJobStateUrl;
    lInmcomplete_url += "?hostid=";
    lInmcomplete_url += service_params.szSVHostID;

    rc = postToCx(service_params.szSVServerName,
                  service_params.ServerHttpPort,const_cast<char*>(lInmcomplete_url.c_str()),NULL,NULL,service_params.Https);

    if( rc.failed())
    {
        DebugPrintf(SV_LOG_ERROR, "GetFromServer() Failed @LINE %d, %s\n", __LINE__, rc.toString());

    }



    DebugPrintf(SV_LOG_INFO,"All Fx jobs stopped\n" );
    return( rc );
}



SVERROR CFileReplicationAgent:: GetStandByCxDetails(vector<StandByCx> *myvector,std::string PassiveCxip,SV_ULONG passiveCxPort)
{
    /* place holder function */
    return SVS_OK;
}

SVERROR CFileReplicationAgent::CheckPrimaryServerHearbeat( SV_ULONG TimeoutPeriod)
{
    DebugPrintf(SV_LOG_DEBUG,"Entered %s\n",__FUNCTION__);
    time_t lInmStartTime;
    SV_ULONGLONG lInmDiff = 0;
    time_t lInmEndTime ;
    SV_ULONG lInmwaiting_time =TimeoutPeriod*60;
    time (&lInmStartTime);
    vector<StandByCx> StandByCxVector;
    StandByCx StandByCx;
    SVERROR hr = SVS_OK;

    do {
        StandByCxVector.clear();
        hr = GetStandByCxDetails(&StandByCxVector);
        time (&lInmEndTime);
        lInmDiff = (SV_ULONGLONG)difftime (lInmEndTime,lInmStartTime);
        for(int i=0;i <(int) StandByCxVector.size();i++)
        {
            StandByCx= StandByCxVector[i];
            DebugPrintf(SV_LOG_INFO,"StandByServer = %s,StandByServerPort = %d,StandByServerTimeout = %d\n",StandByCx.szStandbyServerName,StandByCx.szStandbyServerHTTPPort,StandByCx.szStandbyServerTimeout);
        }
        if(hr == SVE_INVALID_FORMAT )
        {
            DebugPrintf(SV_LOG_INFO,"SVE_INVALID_FORMAT \n ");
            break;
        }
        else if (hr.failed())
        {

            if((lInmDiff)  >  lInmwaiting_time)
            {
                DebugPrintf(SV_LOG_INFO,"Primary CX server Failed to reply after %lu Timeout\n",TimeoutPeriod);
                bCX_Not_Reachable=true;
                DebugPrintf(SV_LOG_INFO,"setting bCX_Not_Reachable flag =%d\n",bCX_Not_Reachable);
                hr = SVE_FAIL;
                break;
            }
#ifdef SV_WINDOWS
            Sleep(1000 );
#else
            sleep(1);
#endif

        }
        else
        {


            DebugPrintf(SV_LOG_INFO,"Primary CX Server successfully respond in %ld sec\n",lInmEndTime);
            hr = SVS_OK;
            break;


        }
    }while(true);
    DebugPrintf(SV_LOG_DEBUG,"Exit %s\n",__FUNCTION__);
    return (hr);
}


SVERROR CFileReplicationAgent::VerifyingPrimaryAndStandByCXStatus(void)
{
    /* place holder function */
    return SVS_OK;
}

#endif




/*
 * CFileReplicationAgent::Run
 *
 * This function runs the fx agent.This is invoked by the frsvc service.
 *
 * Return SV_FAIL
 */

SVERROR CFileReplicationAgent::Run()
{
    /* place holder function */
    return SVS_OK;
}

SVERROR CFileReplicationAgent::delete_encryption_keys(SV_ULONG Inmid) {
    SVERROR hr = SVS_OK;
    hr = delete_encryption_keys_impl(m_sv_host_agent_params.szCacheDirectory, Inmid);
    return hr;
}
SVERROR CFileReplicationAgent::ActOnDeletedFileReplicationConfiguration()
{
    /* place holder function */
    return SVS_OK;
}

/*
 * CFileReplicationAgent::GetFileReplicationAgentConfiguration
 *
 * Gets the Fx agent job configurations from CX server\
 *
 * Return SV_FAIL
 */

SVERROR CFileReplicationAgent::GetFileReplicationAgentConfiguration()
{
    /* place holder function */
    return SVS_OK;
}


CFileReplicationAgent::FRJOB::FR_PROCESS_TYPE CFileReplicationAgent::GetProcessType( FRJOBCONFIG const& JobConfig )const
{
    if( JOB_DIRECTION_PULL == JobConfig.Direction ) {
        DebugPrintf(SV_LOG_ERROR, "CFileReplicationAgent::GetProcessType PULL direction not supported. JobConfgId: %lu, JobId: %lu\n",
                    JobConfig.JobConfigId, JobConfig.JobId);
        return FRJOB::FR_INVALID;
    }
    else
    {
        return( 0 == strcmpi( m_sv_host_agent_params.szSVHostID,
                              JobConfig.szDestHostId ))
            ? (( 0 == strcmpi( m_sv_host_agent_params.szSVHostID,
                               JobConfig.szSourceHostId ))
               ? FRJOB::FR_LOCAL_COPY : FRJOB::FR_DAEMON )
            : ((0 == strcmpi( m_sv_host_agent_params.szSVHostID,
                              JobConfig.szSourceHostId ) )
               ? FRJOB::FR_CLIENT : FRJOB::FR_INVALID );
    }
}

/*
 * CFileReplicationAgent::ShouldStartJob
 *
 * This function returns if a job is to be started or not.
 *
 * JobConfig  Fx job config obtained from CX.
 *
 * Return true or false.
 */

inline bool CFileReplicationAgent::ShouldStartJob( FRJOBCONFIG& JobConfig)
{
    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
    FRJOB::FR_PROCESS_TYPE ProcessType = GetProcessType( JobConfig );
    if( FRJOB::FR_CLIENT == ProcessType )
    {
        return( ( JOB_STATE_STARTED == JobConfig.DaemonState
                  && JOB_STATE_STARTED == JobConfig.AssignedState )
                || JOB_STATE_LOG_UPDATE == JobConfig.AssignedState );
    }
    else if( FRJOB::FR_LOCAL_COPY == ProcessType ) {
        return( JOB_STATE_STARTED == JobConfig.AssignedState
                || JOB_STATE_LOG_UPDATE == JobConfig.AssignedState );
    }
    else if( FRJOB::FR_DAEMON == ProcessType ) {
        return( true );

    }
    else {
        return( false );
    }
}

/*
 * CFileReplicationAgent::ShouldStopJob
 *
 * This function is used to decide if t he job is to be stopped.
 *
 * Job  The current job Id.
 *
 * Return true or false
 *
 */
inline bool CFileReplicationAgent::ShouldStopJob( FRJOB const& Job ) const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
    FRJOBCONFIG const* pConfig = GetJobConfig( Job.JobId );
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
    return( NULL == pConfig || JOB_STATE_STOPPED == pConfig->AssignedState
            || JOB_STATE_NOT_STARTED == Job.Config.AssignedState /*pConfig->AssignedState*/ );
}

/*
 * CFileReplicationAgent::ShouldStopDaemonJob
 *
 * This is used to decide if the current deamon job is to be stopped.
 *
 * Job  The current job id.
 *
 * Return Value
 * SVERROR  :
 */
inline bool CFileReplicationAgent::ShouldStopDaemonJob( FRJOB const& Job ) const
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__);
    FRJOBCONFIG const* pConfig = GetJobConfig( Job.JobId );
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__);
    return( NULL == pConfig || JOB_STATE_STOPPED == pConfig->CurrentState
            || JOB_STATE_NOT_STARTED == Job.Config.AssignedState /*pConfig->AssignedState*/ );
}

/*
 * CFileReplicationAgent::ActOnFileReplicationConfigurationChanges
 *
 * This function is used to implement the job and its options.
 *
 * Return SV_FAIL on error.
 */

SVERROR CFileReplicationAgent::ActOnFileReplicationConfigurationChanges()
{
    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
    SVERROR hr = SVS_OK;

    do {
        for( int i = 0; i < m_cJobConfig; i++ ) {
            DebugPrintf( "[%d] (AssignedState state %d CurrentState %d DaemonState %d)\n",
                         m_FRJobConfig[ i ].JobId,
                         m_FRJobConfig[ i ].AssignedState,
                         m_FRJobConfig[ i ].CurrentState,
                         m_FRJobConfig[ i ].DaemonState);
        }
        /*for each current daemon job, stop any that have been deleted or stopped*/
        for( int i = 0; i < m_cDaemonJobs; i++ )
        {

            if(ShouldStopDaemonJob( m_FRDaemonJob[i]))
            {
                StopDaemonJob( m_FRDaemonJob[ i ] );
            }
        }

        /* Start or stop the daemon*/
        if(IsDaemonRequired()) {
            hr = StartDaemon();
        }
        else {
            hr = StopDaemon();
        }
        /* for each current job, stop any that have been deleted or stopped*/

        for(int i = 0; i < m_cJobs; i++) {
            if(ShouldStopJob( m_FRJob[ i ])) {
                StopJob( m_FRJob[ i ] );
            }
        }

        if(hr.failed()) {
            DebugPrintf(SV_LOG_ERROR, "Failed to %s @LINE %d daemon, %s \n",
                        IsDaemonRequired() ? "start" : "stop", __LINE__, hr.toString());
            DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
            return hr;
            //break;
        }

        /* foreach job in the configuration not already started, start it*/
        int cFailure = 0;
        for( int i = 0; i < m_cJobConfig; i++ ) {
            if(ShouldStartJob( m_FRJobConfig[i])) {
                hr = StartJob( m_FRJobConfig[ i ] );
                if( hr.failed() ) {
                    DebugPrintf(SV_LOG_ERROR, "Failed start job %u, %s\n",
                                m_FRJobConfig[ i ].JobId, hr.toString());
                    cFailure ++;
                }
            }
            else {
                DebugPrintf(SV_LOG_INFO, "Skipping non starting job %d\n",
                            m_FRJobConfig[ i ].JobId );
            }
        }

        if(cFailure > 0) {
            hr = SVE_FAIL;
        }

    } while( false );
    DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
    return(hr);
}

/*
 * CFileReplicationAgent::UpdateDaemonJobLog
 *
 * This function updates the daemon job status to Cx
 *
 * JobId --> job id of daemon job to report status to the cx.
 *
 * Return SV_FAIL on error
 */
SVERROR CFileReplicationAgent::UpdateDaemonJobLog(int jobId)
{
    /* place holder function */
    return SVS_OK;
}

SVERROR CFileReplicationAgent::cleanup_client_keys(SV_ULONG InmJobConfigId) {
    SVERROR hr = SVS_OK;
    hr = cleanup_client_keys_impl(m_sv_host_agent_params.ssh_config,  m_sv_host_agent_params.szCacheDirectory, InmJobConfigId);
    return hr;

}
SVERROR CFileReplicationAgent::cleanup_server_keys(SV_ULONG InmJobId, SV_ULONG InmJobConfigId)
{
    SVERROR hr = SVS_OK;
    hr = cleanup_server_keys_impl(m_sv_host_agent_params.ssh_config, m_sv_host_agent_params.szCacheDirectory, InmJobId, InmJobConfigId);
    return hr;
}

/*
 * CFileReplicationAgent::UpdateFileReplicationAgentStatus
 *
 * This function updates the job status to Cx
 *
 * Return SV_FAIL on error
 */

SVERROR CFileReplicationAgent::UpdateFileReplicationAgentStatus()
{
    /* place holder function */
    return SVS_OK;
}

SVERROR CFileReplicationAgent::getPublicKeys(FRJOBCONFIG &JobConfig)
{
    /* place holder function */
    return SVS_OK;
}

SVERROR CFileReplicationAgent::setPublicKey(FRJOBCONFIG JobConfig)
{
    /* place holder function */
    return SVS_OK;
}

void CFileReplicationAgent::setCxpsInfo(char const* tag, char const* value)
{
    // NOTE: these need to staty in syc with the tags used by
    // getCxpsInfo function in admin/web/functions.php
    // so if you want to add, remove, or change these, make the same change
    // in getCxpsInfo
    if (0 == strncmp(tag, "port", strlen("port"))) {
        m_cxpsPort = value;
    } else if (0 == strncmp(tag, "sslport", strlen("sslport"))) {
        m_cxpsSslPort = value;
    } else if (0 == strncmp(tag, "user", strlen("user"))) {
        m_cxpsUser = value;
    } else if (0 == strncmp(tag, "password", strlen("password"))) {
        m_cxpsPassword = value;
    } else if (0 == strncmp(tag, "allowdirs", strlen("allowdirs"))) {
        m_cxpsAllowDirs = value;
    }
}

std::string CFileReplicationAgent::escapeSpacesInPath(char const* path)
{
    std::string escapedPath;
    char const* ch = path;
    bool prevIsBackSlash = false;
    while (*ch) {
        if ('\\' == *ch) {
            prevIsBackSlash = true;
        } else {
            if (' ' == *ch) {
                if (!prevIsBackSlash) {
                    // only add \\ if one not already there
                    escapedPath.append(1, '\\');
                }
            }
            prevIsBackSlash = false;
        }
        escapedPath.append(1, *ch);
        ++ch;
    }
    return escapedPath;
}

void CFileReplicationAgent::parseCxpsInfoIfPresent(char*& buffer)
{
    // if the first line begins with cxpsinfo then have newer frjobconfig that supports
    // returning cxps info in the first line. That line will be
    // "cxpsinfo&port=<port>&sslport=<sslport>&user=<user>&password=<password>\n"
    char* cxpsInfoTag = "cxpsinfo";
    size_t tagLen = strlen(cxpsInfoTag);
    if (strlen(buffer) > tagLen && 0 == memcmp(buffer, "cxpsinfo", tagLen)) {
        char* newLine = strchr(buffer, '\n');
        if (0 != newLine) {
            *newLine = '\0'; // only want to deal with the first line that has cxps info
            char* ampersand = strchr(buffer, '&'); // gets to & of cxpsinfo&
            if (0 != ampersand) {
                // now process all &tag=value
                while (0 != ampersand) {
                    char* tag = ++ampersand; // skip & to get to start of tag=value
                    ampersand = strchr(ampersand, '&'); // gets to next &tag=value will return null for last tag=value
                    if (0 != ampersand) {
                        *ampersand = '\0';
                    }
                    char* value = strchr(tag, '=');
                    if (0 != value) {
                        *value = '\0';
                        setCxpsInfo(tag, ++value);
                    }
                }
            }
            // adjust buffer past the cxpsinfo
            buffer = newLine + 1;
        }
        m_cxpsClientPem = m_sv_host_agent_params.AgentInstallPath;
        bool backSlashSeparator = '\\' == m_cxpsClientPem[m_cxpsClientPem.size() - 1];
        m_cxpsClientPem += "transport";
        m_cxpsClientPem += (backSlashSeparator ? "\\" : "//");
        m_cxpsClientPem += "client.pem";
    }
}

/*
 * CFileReplicationAgent::ParseFileReplicationConfiguration
 *
 * pszBuffer Buffer containing the Fx configuration to be parsed.
 *
 * Return SV_FAIL on error.
 */

SVERROR CFileReplicationAgent::ParseFileReplicationConfiguration( char* InmpszBuffer )
{
    /* place holder function */
    return SVS_OK;
}

/*
 * CFileReplicationAgent::GetJobConfig
 *
 * This function takes the job id and returns the job configuration.
 *
 * JobId  The current job id.
 *
 * Return pConfig pointer of type CFileReplicationAgent::FRJOBCONFIG.
 *
 */
CFileReplicationAgent::FRJOBCONFIG const* CFileReplicationAgent::GetJobConfig(
    SV_ULONG JobId ) const
{
    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
    FRJOBCONFIG const* pConfig = NULL;

    for( int i = 0; i < m_cJobConfig; i++ ) {
        if( m_FRJobConfig[ i ].JobId == JobId ) {
            pConfig = &m_FRJobConfig[ i ];
            break;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
    return( pConfig );
}

/*
 * CFileReplicationAgent::StopJob
 * Also used to stop main daemon job
 *
 * job The job id of the current job.
 *
 * Return None
 */

void CFileReplicationAgent::StopJob( FRJOB& job, bool needToStopProcess )
{
    /* place holder function */
}

/*
 * CFileReplicationAgent::StopDaemonJob
 *
 * job The job id of the current job.
 *
 * Return Value
 *
 */
void CFileReplicationAgent::StopDaemonJob( FRJOB& Job )
{
    /* place holder function */
}

/*
 * CFileReplicationAgent::RemoveStoppedJobs
 *
 * Return None
 */
void CFileReplicationAgent::RemoveStoppedJobs()
{
    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
    int lInmidxRow = 0;

    for( int i = 0; i < m_cJobs; i++ ) {
        DebugPrintf( "[%d] (CurrentState %d )\n", m_FRJob[ i ].JobId,m_FRJob[ i ].CurrentState);
        if( JOB_STATE_STOPPED !=  m_FRJob[ i ].CurrentState
            && JOB_STATE_NOT_STARTED !=  m_FRJob[ i ].CurrentState ) {
            m_FRJob[ lInmidxRow ] = m_FRJob[ i ];
            lInmidxRow ++;
        }
        else {
            assert( JOB_STATE_STOPPED !=  m_FRJob[ i ].CurrentState
                    || NULL == m_FRJob[ i ].pProcess );
        }
    }

    m_cJobs = lInmidxRow;
    DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
}

/*
 * CFileReplicationAgent::RemoveStoppedDaemonJobs
 *
 * Return none
 */
void CFileReplicationAgent::RemoveStoppedDaemonJobs()
{
    /* place holder function */
}


void CFileReplicationAgent::DiscardJob(SV_ULONG JobId)
{
    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
    int lInmidxRow = 0;

    for( int i = 0; i < m_cJobs; i++ ) {
        if( m_FRJob[ i ].JobId != JobId) {
            m_FRJob[ lInmidxRow ] = m_FRJob[ i ];
            lInmidxRow ++;
        }

    }
    m_cJobs = lInmidxRow;
    DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
}

void CFileReplicationAgent::DiscardDaemonJob(SV_ULONG JobId )
{
    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
    int lInmidxRow = 0;
    std::list<int>::iterator SearchIterator;
    for( int i = 0; i < m_cDaemonJobs; i++ ) {
        if( m_FRDaemonJob[ i ].JobId != JobId) {
            m_FRDaemonJob[ lInmidxRow ] = m_FRDaemonJob[ i ];
            lInmidxRow ++;
        }
        else
        {
            for(SearchIterator = JobIdVector.begin();SearchIterator != JobIdVector.end();SearchIterator++ )
            {
                if (*SearchIterator  == m_FRDaemonJob[ i ].JobId)
                {
                    JobIdVector.erase( SearchIterator );
                    DebugPrintf(SV_LOG_DEBUG,"Job id = %d is removed from vector\n",m_FRDaemonJob[ i ].JobId);
                    break ;
                }

            }
        }
    }
    m_cDaemonJobs = lInmidxRow;
    DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
}

/*
 * CFileReplicationAgent::createFxAllowedDirsFile
 * 
 * creates a FX allowed dirs files with the current list of allowed dirs
 */
void CFileReplicationAgent::createFxAllowedDirsFile()
{
    boost::filesystem::path fxAllowedDirsFile;
    getFxAllowedDirsPath(fxAllowedDirsFile);
    if (!boost::filesystem::exists(fxAllowedDirsFile)) {
        if (!boost::filesystem::create_directory(fxAllowedDirsFile)) {
            DebugPrintf(SV_LOG_ERROR, "Failed to create directory  %s @LINE %d: %d\n", fxAllowedDirsFile.string().c_str(), __LINE__, errno);
        }
    }
    std::string name("fx-");
    name += boost::posix_time::to_iso_string(boost::posix_time::microsec_clock::local_time());
    fxAllowedDirsFile /= name;
    std::ofstream outFile(fxAllowedDirsFile.string().c_str());
    if (outFile.good()) {
        removeRepeatingSlashBackSlash(m_cxpsAllowDirs);
        outFile << m_cxpsAllowDirs;
    } else {
        DebugPrintf(SV_LOG_ERROR, "Failed to Open the file %s @LINE %d for writing: %d\n", fxAllowedDirsFile.string().c_str(), __LINE__, errno);
    }
}

/*
 * CFileReplicationAgent::dremoveFxAllowedDirsFiles
 * 
 * creates a FX allowed dirs files with the current list of allowed dirs
 */
void CFileReplicationAgent::removeFxAllowedDirsFiles()
{
    boost::filesystem::path fxAllowedDirsPath;
    getFxAllowedDirsPath(fxAllowedDirsPath);
    if (boost::filesystem::exists(fxAllowedDirsPath)) {
        boost::filesystem::directory_iterator iterEnd;
        boost::filesystem::directory_iterator iter(fxAllowedDirsPath);
        for (/* empty */; iter != iterEnd; ++iter) {
            if (boost::filesystem::is_regular_file(iter->status())) {
                boost::filesystem::remove((*iter).path());
            }
        }
    }    
}

/*
 * CFileReplicationAgent::StartDaemon
 *
 * Return none
 */
SVERROR CFileReplicationAgent::StartDaemon()
{
    /* place holder function */
    return SVS_OK;
}

/*
 * CFileReplicationAgent::updateJobStatus
 *
 * jobId The current job id.
 *
 * Returns SV_FAIL on error.
 */

SVERROR CFileReplicationAgent::updateJobStatus(SV_ULONG jobId)
{
    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);

    SVERROR lInmrc;

    SVERROR lInmhr;
    DebugPrintf(SV_LOG_INFO, "Calling ReadAgentConfiguration\n");
    lInmhr = ReadAgentConfiguration(&m_sv_host_agent_params, m_pszConfigPathname,
                                    SV_AGENT_FILE_REPLICATION );
    if ( lInmhr.failed() )
    {
        DebugPrintf(SV_LOG_ERROR, "ReadAgentConfiguration Failed @LINE %d\n", __LINE__);
    }


    string lInmcomplete_url;
    string lInmszUpdateJobStateUrl = "/update_file_replication_status.php";
    char lInmjobIdStr[8];

    lInmcomplete_url += lInmszUpdateJobStateUrl;
    lInmcomplete_url += "?jobid=";
	inm_sprintf_s(lInmjobIdStr, ARRAYSIZE(lInmjobIdStr), "%d", jobId);
    lInmcomplete_url += lInmjobIdStr;

    lInmrc = postToCx(m_sv_host_agent_params.szSVServerName,
                      m_sv_host_agent_params.ServerHttpPort,const_cast<char*>(lInmcomplete_url.c_str()),NULL,NULL,m_sv_host_agent_params.Https);

    if( lInmrc.failed())
    {
        DebugPrintf(SV_LOG_ERROR, "postToCx() Failed @LINE %d, %s\n", __LINE__, lInmrc.toString());

    }




    DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
    return( lInmrc );
}

/*
 * CFileReplicationAgent::StopDaemon()
 *
 * Return SVS_OK or SVS_FALSE
 */
SVERROR CFileReplicationAgent::StopDaemon()
{
    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
    DebugPrintf(SV_LOG_DEBUG, "CFileReplicationAgent::StopDaemon\n");
    SVERROR lInmhr = SVS_OK;
    if( JOB_STATE_STARTED != m_Daemon.CurrentState ) {
        lInmhr = SVS_FALSE;
    }
    else
    {
        DebugPrintf(SV_LOG_DEBUG, "Stopping Daemon Job\n");
        // no need to stop but still need to update state
        StopJob( m_Daemon, false );
    }
    DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
    return( lInmhr );
}

/*
 * CFileReplicationAgent::FindJob
 *
 * JobId the current job id.
 *
 * Return Vpointer to FRJob
 */
CFileReplicationAgent::FRJOB const* CFileReplicationAgent::FindJob(SV_ULONG JobId ) const
{
    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
    FRJOB const* pJob = NULL;

    for( int i = 0; i < m_cJobs; i++ ) {
        if( m_FRJob[ i ].JobId == JobId) {
            pJob = &m_FRJob[ i ];
            break;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
    return( pJob );
}

/*
 * CFileReplicationAgent::FindDaemonJob
 *
 * JobId The current job id.
 *
 * Return pointer to FRJOB
 */

CFileReplicationAgent::FRJOB const* CFileReplicationAgent::FindDaemonJob(SV_ULONG JobId ) const
{
    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
    FRJOB const* pJob = NULL;

    for( int i = 0; i < m_cDaemonJobs; i++ ) {
        if( m_FRDaemonJob[ i ].JobId == JobId) {
            pJob = &m_FRJob[ i ];
            break;
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
    return( pJob );
}
void CFileReplicationAgent::getRunningDaemonJobsList()
{
    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
    FRJOB const* pJob = NULL;

    for( int i = 0; i < m_cDaemonJobs; i++ )
    {
        if(m_FRDaemonJob[ i ].CurrentState == JOB_STATE_STARTED )
        {
            RunningDemonJobList.push_back(m_FRDaemonJob[i].JobId);
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);

}
void CFileReplicationAgent::getRunningJobList()
{
    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
    for( int i = 0; i < m_cJobs; i++ )
    {
        if( m_FRJob[ i ].CurrentState == JOB_STATE_STARTED )
        {
            RunningJobList.push_back(m_FRJob[ i ].JobId);
        }
    }
    DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);

}

bool CFileReplicationAgent::FindDuplicateConfigJob(SV_ULONG JobConfigId )
{
    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
    int lInmcount = 0;
    bool lInmbRetval = false;
    for( int i = 0; i < m_cJobConfig; i++ )
    {
        if( m_FRJobConfig[ i ].JobConfigId == JobConfigId)
        {
            lInmcount++;
        }
    }
    if(lInmcount >= 2)
    {
        lInmbRetval = true;
    }
    DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
    return lInmbRetval;

}
bool CFileReplicationAgent::FindDuplicateJob(SV_ULONG JobId )
{
    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
    int lInmcount = 0;
    bool lInmbRetval = false;
    for( int i = 0; i < m_cJobConfig; i++ )
    {
        if( m_FRJobConfig[ i ].JobId == JobId)
        {
            lInmcount++;
        }
    }
    if(lInmcount >= 2)
    {
        lInmbRetval = true;
    }
    DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
    return lInmbRetval;
}

SVERROR CFileReplicationAgent::update_authorized_keys( FRJOBCONFIG &JobConfig )
{
    /* place holder function */
    return SVS_OK;
}


SVERROR CFileReplicationAgent::generate_key_pair(FRJOBCONFIG &JobConfig)
{
    /* place holder function */
    return SVS_OK;
}

/*
 * CFileReplicationAgent::StartJob
 *
 * JobConfig The job configuration structure.
 *
 * Return SV_FAIL on error.
 */

SVERROR CFileReplicationAgent::StartJob( CFileReplicationAgent::FRJOBCONFIG &
                                         JobConfig )
{
    /* plcae holder function */
    return SVS_OK;
}

/*
 * CFileReplicationAgent::StartDaemonJob
 *
 * JobConfig
 *
 * Return SV_FAIL on error
 */
SVERROR CFileReplicationAgent::StartDaemonJob( CFileReplicationAgent::FRJOBCONFIG &
                                               JobConfig )
{
    /* place holder function */
    return SVS_OK;
}

/*
 * CFileReplicationAgent::UpdateOption
 *
 * Updates the config with a named option along with its value. This
 * handles string and integer options, not flags
 *
 * Parameters
 * None  :
 *
 * Return Value
 * void  :
 */
void CFileReplicationAgent::UpdateOption( FRJOBCONFIG& JobConfig,
                                          char const* pszOptionTag, char const* pszParam )
{
    /* placeholder function */
}

/*
 * CFileReplicationAgent::GetinmsyncOptions
 *
 * Builds the inmsync options from the job Configuration.
 *
 * psz <-- inmsync options
 *
 * cch -->
 *
 * options -->
 *
 * Return None.
 */
void CFileReplicationAgent::GetinmsyncOptions( std::string& optionsAsString,
                                               FRJOBCONFIG::OPTIONS const& options,
                                               bool exp,
                                               SV_ULONG jobid,
                                               char const* destHostId,
                                               char const* secureMode)
{
    /* plcae holder function */
}

/*
 * CFileReplicationAgent::Shutdown
 *
 * Return Value SVE_NOTIMPL
 *
 */
SVERROR CFileReplicationAgent::Shutdown()
{
    return(SVE_NOTIMPL);
}

/*
 * CFileReplicationAgent::DeleteLogFiles
 *
 * Deletes the log files.of all the jobs.
 *
 * Return SV_FAIL
 *
 */
SVERROR CFileReplicationAgent::DeleteLogFiles()
{
    DebugPrintf(SV_LOG_DEBUG, "Entering %s\n", __FUNCTION__);
    SVERROR hr;
    int const SECONDS_PER_DAY = 86400;
    time_t  lInmOneWeekAgo = time( NULL ) - 7 * SECONDS_PER_DAY;
    do
    {
        hr = DeleteOlderMatchingFiles( m_sv_host_agent_params.szCacheDirectory,
                                       JOB_LOG_PATTERN,  lInmOneWeekAgo );

        if( hr.failed() ) {
            break;
        }

        hr = DeleteOlderMatchingFiles( m_sv_host_agent_params.szCacheDirectory,
                                       JOB_EXCLUDE_PATTERN,  lInmOneWeekAgo );
    } while( false );
    DebugPrintf(SV_LOG_DEBUG, "Exiting %s\n", __FUNCTION__);
    return( hr );
}

/* CFileReplicationAgent::RunScriptCommand
 *
 * Creates include exclude file options depending on the job config.
 *
 * FRJOBCONFIG::*pszCommandArg)[SV_MAX_PATH]--> JobConfiguration obtained from CX.
 *
 * JobConfig --> JobConfiguration obtained from CX.
 *
 * pszLogFilename --> Log file of the current job(exit status is uploaded).
 *
 * pJob --> Current Job for which the script is being run.
 *
 * pExitCode --> exit code of the pre/post script.

 * Return Value  SVFAIL on error.
 */

SVERROR CFileReplicationAgent::RunScriptCommand(char const (FRJOBCONFIG::*pszCommandArg)[SV_MAX_PATH],
                                                FRJOBCONFIG const& JobConfig, char const* pszLogFilename, FRJOB * pJob,
                                                SV_ULONG* pExitCode )
{
    /* placeholder function */
    return SVS_OK;
}

/* CFileReplicationAgent::CreateIncludeExcludeFile
 *
 * Creates include exclude file options depending on the job config.
 *
 * JobConfig --> JobConfiguration obtained from CX.
 *
 * Return Value  SVFAIL on error.
 */

SVERROR CFileReplicationAgent::CreateIncludeExcludeFile( FRJOBCONFIG const& JobConfig )
{
    SVERROR rc;
    FILE* pExcludeFile = NULL;
    do
    {
        if( 0 == JobConfig.options.szExcludeFrom[ 0 ] ) {
            break;
        }

        assert( JobConfig.options.szExcludePattern[ 0 ]
                || JobConfig.options.szIncludePattern[ 0 ] );

        pExcludeFile = fopen( JobConfig.options.szExcludeFrom, "w" );
        if( NULL == pExcludeFile )
        {
            rc = errno;
            break;
        }

        assert( sizeof( JobConfig.options.szExcludePattern )
                == sizeof( JobConfig.options.szIncludePattern ) );
        char lInmszDelimeters[] = ";";
        char lInmszBuffer[ sizeof( JobConfig.options.szIncludePattern ) ];

        /* Include patterns come first, then exclude patterns. Thus the includes
           take precedence.*/

        inm_strcpy_s( lInmszBuffer,ARRAYSIZE(lInmszBuffer), JobConfig.options.szIncludePattern );
        for( char* pszToken = strtok( lInmszBuffer, lInmszDelimeters ); NULL != pszToken;
             pszToken = strtok( NULL, lInmszDelimeters ) ) {
            if( fprintf( pExcludeFile, "+ %s\n", pszToken ) < 0 ) {
                rc = errno;
                break;
            }
        }

        if( rc.failed() ) {
            rc = errno;
            break;
        }

        inm_strcpy_s( lInmszBuffer, ARRAYSIZE(lInmszBuffer),JobConfig.options.szExcludePattern );
        for( char* pszToken = strtok( lInmszBuffer, lInmszDelimeters ); NULL != pszToken;
             pszToken = strtok( NULL, lInmszDelimeters ) ) {
            if( fprintf( pExcludeFile, "- %s\n", pszToken ) < 0 ) {
                rc = errno;
                break;
            }
        }

        if( rc.failed() ) {
            break;
        }
    } while( false );
    SAFE_FCLOSE( pExcludeFile );

    return( rc );
}


SVERROR CFileReplicationAgent::getJobExitCode(SV_ULONG jobId, SV_ULONG jobInstanceId, int &exitCode)
{
    /* place holder function */
    return SVS_OK;
}
SVERROR CFileReplicationAgent::RegisterFXIfNeeded()
{
    DebugPrintf( SV_LOG_DEBUG,"ENTERED: RegisterFXIfNeeded\n" );
    SVERROR hr = SVS_OK;
    SV_ULONG HostIDSize = 64;
    time_t lInmcurrentTime = time(NULL);
    if( (lInmcurrentTime - lastRegisterHostTime) >= RegisterHostIntervel )
    {
        DebugPrintf( SV_LOG_DEBUG,"RegisterHost in RegisterFXIfNeeded \n" );
        DebugPrintf( SV_LOG_DEBUG,"RegisterHost in RegisterFXIfNeeded https value= %d\n",m_sv_host_agent_params.Https );
        hr = RegisterFX( m_sv_host_agent_params.szSVHostAgentRegKey,
                         m_sv_host_agent_params.szSVServerName,
                         m_sv_host_agent_params.ServerHttpPort,
                         m_sv_host_agent_params.szSVRegisterURL,
                         m_sv_host_agent_params.AgentInstallPath,
                         m_sv_host_agent_params.szSVHostID,
                         &HostIDSize,m_sv_host_agent_params.Https);
        if(hr.failed())
        {

            bCX_Not_Reachable=true;
            DebugPrintf(SV_LOG_INFO,"setting bCX_Not_Reachable flag =%d\n",bCX_Not_Reachable);
            DebugPrintf(SV_LOG_ERROR, "RegisterHost() Failed @LINE %d, %s\n", __LINE__, hr.toString());
            hr =  SVS_FALSE;
        }
        lastRegisterHostTime = lInmcurrentTime;
    }
    DebugPrintf( SV_LOG_DEBUG,"EXITED: RegisterFXIfNeeded\n" );
    return hr;
}

SVERROR CFileReplicationAgent::persistRemoteCXInfo(const char *pszRemoteCXInfo)
{
    DebugPrintf(SV_LOG_DEBUG,"Entered %s\n",__FUNCTION__);
    SVERROR hr = SVE_FAIL;
    if(NULL == pszRemoteCXInfo)
    {
        DebugPrintf(SV_LOG_ERROR,"Error persistRemoteCXInfo(). RemoteCXInfo should not be null\n");
        return hr;
    }
#ifdef SV_WINDOWS
    hr = SetFXRegStringValue("RemoteCXInfo",pszRemoteCXInfo);
#else
    hr = SetConfigParam(m_pszConfigPathname, "RemoteCXInfo",pszRemoteCXInfo);
#endif
    DebugPrintf(SV_LOG_DEBUG,"Exited %s\n",__FUNCTION__);
    return hr;
}

SVERROR CFileReplicationAgent::loadSavedRemoteCXDetails()
{
    /* place holder function */
    return SVS_OK;
}

void CFileReplicationAgent::setAgentInstallPath()
{
    // slight hack as non-windows does not set the agent install dir
    // but the config file is in the install dir so just strip off
    // the base name to get the agent intall dir
    if (0 != m_pszConfigPathname) {
        m_agentInstallPath = m_pszConfigPathname;
        std::string::size_type lastSlash = m_agentInstallPath.find_last_of("/\\");
        if (std::string::npos != lastSlash) {
            m_agentInstallPath.erase(lastSlash + 1);
        }
    }
}

void CFileReplicationAgent::removeRepeatingSlashBackSlash(std::string& str)
{
    std::vector<char> vbuffer(str.size() + 1, '\0');
    vbuffer.insert(vbuffer.begin(), str.begin(), str.end());
    removeRepeatingSlashBackSlash(&vbuffer[0], vbuffer.size());
    str = vbuffer.data();
}

void CFileReplicationAgent::removeRepeatingSlashBackSlash(char* str, const size_t strlen)
{
    size_t curIdx = 0;
    size_t lstIdx = 0;
    const size_t &endIdx = strlen;
    while (curIdx < endIdx) {
        if (curIdx > lstIdx) {
            str[lstIdx] = str[curIdx];
        }
        if ('/' == str[curIdx] || '\\' == str[curIdx]) {
            ++lstIdx;
            do {
                ++curIdx;
            } while (curIdx < endIdx && ('\\' == str[curIdx] || '/' == str[curIdx]));
        } else {
            ++lstIdx;
            ++curIdx;
        }
    }
    if (lstIdx < endIdx) {
        str[lstIdx] = '\0';
    }
}

SVERROR CFileReplicationAgent::UpgradeFXInformation()
{
    DebugPrintf(SV_LOG_DEBUG,"Entered %s\n",__FUNCTION__);
    SVERROR rc = SVS_OK;
    SVERROR hr = SVS_OK;

    string resetval = "0";
    SV_HOST_AGENT_PARAMS service_params;
    const char * configPath = NULL;


    ReadAgentConfiguration(&service_params,m_pszConfigPathname,SV_AGENT_FILE_REPLICATION );


    if(service_params.Fx_Upgraded == 1)
    {
        string lInmcomplete_url;
        string lInmpost_buffer("hostid=");

        string szUpgradeUrl = "/fx_compatability_update.php";

        lInmcomplete_url += szUpgradeUrl;
        lInmcomplete_url += "?hostid=";
        lInmcomplete_url += service_params.szSVHostID;

        rc = postToCx(service_params.szSVServerName,
                      service_params.ServerHttpPort,const_cast<char*>(lInmcomplete_url.c_str()),NULL,NULL,service_params.Https);

        if( rc.succeeded())
        {
            DebugPrintf(SV_LOG_INFO,"postToCx is Success for fx_compatability_update.php call\n");
#ifdef SV_WINDOWS
            DWORD resetupgradedval=0;
            hr = SetFXRegDWordValue("IsFxUpgraded",resetupgradedval);
#else
            hr = SetConfigParam(m_pszConfigPathname, "IsFxUpgraded",resetval);
#endif

        }
    }



    DebugPrintf(SV_LOG_DEBUG,"Exited %s\n",__FUNCTION__);
    return( rc );
}
