//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : cdputil.h
//
// Description: 
//

#ifndef CDPUTIL__H
#define CDPUTIL__H

#include <string>
#include <vector>

#include "svtypes.h"
#include "cdpglobals.h"

typedef std::vector<std::string> svector_t;

#include <ace/Process_Manager.h>
#include "configurator2.h"
#include "retentioninformation.h"
#include "v3/cdpv3tables.h"
#include "volumegroupsettings.h"

class CDPUtil
{

public:

	static const unsigned int NoLimit = 0;

	static void set_quit_flag(bool flag)  ;
	static svector_t split(const std::string, const std::string, const int numFields = 0);
	static void trim(std::string& s, std::string trim_chars = " \n\b\t\a\r\xc");
	static void WaitForNSecs(int secs);
	static ACE_Atomic_Op<ACE_Thread_Mutex, bool> quitflag; 
	static bool ToDisplayTimeOnConsole(SV_ULONGLONG ts, std::string & display);
	static bool ToDisplayTimeOnCx(SV_ULONGLONG ts, std::string & display);
	static bool InputTimeToFileTime(const std::string & input, SV_ULONGLONG & ts);
	static bool CxInputTimeToFileTime(const std::string & input, SV_ULONGLONG & ts);
	static bool ToSVTime(SV_ULONGLONG ts, SV_TIME &svTime);
	static bool ToTimestampinHrs(const SV_ULONGLONG &in_ts, SV_ULONGLONG & out_ts);
	static bool CXInputTimeToConsoleTime(const std::string & input, std::string & consoleTime);
	static bool ToFileTime(const SV_TIME &svTime,SV_ULONGLONG &fileTime);
	static SVERROR CleanPendingActionFiles(const std::string & DeviceName, std::string & errorMessage);
	static bool CleanCache(const std::string & DeviceName, std::string & errorMessage);
	static bool CleanRetentionLog(const std::string dbname, std::string & errorMessage);
	static bool CleanRetentionPath(const std::string & dbname);
	static bool CleanTargetCksumsFiles(const std::string & DeviceName);

	static bool GetServiceState(const std::string & ServiceName, SV_ULONG & state);

	static bool moveRetentionLog(Configurator& configurator, std::map<std::string,std::string> const& moveparams);
	static bool ShouldOperationRun(Configurator& configurator, const std::string& volume, const std::string& operation, bool & shouldrun);
	static bool CopyRetentionLog(const std::string & oldPathName, const std::string & newPathName);
	static bool UpdateRetentionDataDir(const std::string & new_dbName,const std::string & newRetentionDir);
	static bool sendAndWaitForCxStateToUpdate(Configurator& configurator, const std::string & deviceName,int hosttype,const std::string & responsemsg);
	static bool sendMoveRetentionResponseToCX(Configurator& configurator, const std::string & deviceName,int hosttype, const std::string & responsemsg);
	static bool retentionFileExists(const std::string & fileName);
	static bool CopyFile(const std::string & SourceFile, const std::string & DestinationFile);
	static bool CopyDirectoryContents(const std::string& src, const std::string& dest,bool recursive=true);
	static bool CleanAllDirectoryContent(const std::string & dbpath);


	//static time_t MutexTimeOutInSec() { return MUTEX_TIME_OUT_IN_SEC;}

	static SV_ULONGLONG MaxSpacePerDataFile();
	static SV_ULONGLONG MaxTimeRangePerDataFile();
	static SV_ULONGLONG LookupPageSize()          { return CDP_LOOKUPPAGESIZE; }

	static bool getEventByIdentifier(const std::string & db, 
		std::string & recoverypt,
		const std::string & identifier);

	// Most Recent Consistent Point for app/fs
	static bool MostRecentConsistentPoint(const std::vector<std::string> & dbs,
		CDPEvent & commonevent,
		const SV_EVENT_TYPE & type,
		const SV_ULONGLONG & aftertime = 0,
		const SV_ULONGLONG & beforetime =0
		);

	// common app/fs event with given  value
	static bool FindCommonEvent(const std::vector<std::string> &  dbs, 
		const SV_EVENT_TYPE & type,
		const std::string & value,
		std::string & identifier,
		SV_ULONGLONG & ts);

	static bool PickLatestEvent(const std::vector<std::string> & dbs,
		const SV_EVENT_TYPE & type,
		const std::string & value,
		std::string & identifier,
		SV_ULONGLONG & ts);

	static bool PickOldestEvent(const std::vector<std::string> & dbs,
		const SV_EVENT_TYPE & type,
		const std::string & value,
		std::string & identifier,
		SV_ULONGLONG & ts);

	// is specified time a common crash consisten point
	static bool isCCP(const std::vector<std::string> &  dbs,
		const SV_ULONGLONG &ts,
		bool & isconsistent,
		bool & isavailable);
	static bool isCCP(std::string const & dbname, const SV_ULONGLONG &ts, bool & isconsistent, bool & isavailable);
	static bool gettimeranges(std::string const & dbname, CDPTimeRangeMatchingCndn const & cndn, std::vector<CDPTimeRange> & timeranges);

	static void TimeRangeIntersect(std::vector<CDPTimeRange> & lhs, std::vector<CDPTimeRange> &rhs, std::vector<CDPTimeRange> & result);

	// most recent crash consistent point
	static bool MostRecentCCP(const std::vector<std::string> &  dbs,
		SV_ULONGLONG & ts,
		SV_ULONGLONG & seq,
		const SV_ULONGLONG & aftertime = 0,
		const SV_ULONGLONG & beforetime =0
		);
	static bool getevents(std::string const & dbname, CDPMarkersMatchingCndn const & cndn, std::vector<CDPEvent> & events);
	static bool printevents(std::string const & dbname, 
		CDPMarkersMatchingCndn const & cndn,
		SV_ULONGLONG & eventCount,
		bool namevaluefomat = false);

    static bool displaybookmarks(std::set<CDPV3BookMark> & bookmarks, bool namevalueformat = false);

	static bool FetchSeqtoRecover(const std::vector<std::string> &  dbs, SV_ULONGLONG & ts,SV_ULONGLONG & seq);
	static bool FetchSeqtoRecover(const std::string & db, SV_ULONGLONG & ts,SV_ULONGLONG & seq);

	static bool getNthEvent(const std::string & db,
		const SV_EVENT_TYPE & type,
		const SV_ULONGLONG & eventnum,
		CDPEvent & event);

	static bool runscript(const std::string & script);
	static bool WaitForProcess(ACE_Process_Manager *, pid_t);

	static void Stop(int);
	static void SetupSignalHandler();
	static bool CliInitQuitEvent(bool skipHandlerInit) ;
	static bool SvcInitQuitEvent();
	static bool InitQuitEvent(bool callerCli = false, bool skipHandlerInit = false) ;
	static void UnInitQuitEvent();
	static bool QuitRequested(int waitTimeinSeconds = 0);
	static bool Quit();
	static void RequestQuit();
	static void SignalQuit();
    static void ClearQuitRequest() ;
	static std::string hash(const std::string & path);
	// added to resolve the bug 5276
	static std::string getPendingActionsDirectory();
	static std::string getPendingActionsFilePath(const std::string & volumename, const std::string & extn);
	static std::string getPendingActionsFileBaseName(const std::string & volumename, const std::string & extn);
	static std::string getCurrentTimeAsString();

	static int disable_signal(int sigmin, int sigmax);

	static std::string get_last_fileapplied_info_location(const std::string& volumeName);
	static bool get_last_fileapplied_information(const std::string& volumeName, CDPLastProcessedFile& lastProcessedInfo);
	static bool update_last_fileapplied_information(const std::string& volumeName, CDPLastProcessedFile& appliedInfo);

	static std::string get_last_retentiontxn_info_location(const std::string& dbdir);
	static bool get_last_retentiontxn(const std::string& dbdir, CDPLastRetentionUpdate& lastRetentionUpdate);
	static bool update_last_retentiontxn(const std::string& dbdir, CDPLastRetentionUpdate& lastRetentionUpdate);


	static bool getRetentionInfo(const std::string &dbname,
		const SV_ULONGLONG& endTimeinNSecs, RetentionInformation& retInfo, bool dump);


	static int cdp_busy_handler(void * arg, int retry_count);
	static void UnmountVsnapsAssociatedWithInode(SV_ULONGLONG,const std::string& volname);
	static void UnmountVsnapsWithLaterRecoveryTime(SV_ULONGLONG,const std::string& volname);
	static void UnmountVsnapsInTimeRange(const SV_ULONGLONG & start_ts,const SV_ULONGLONG & end_ts, 
									const std::string& volname, const std::vector<CDPV3BookMark>& bookmarks_to_preserve);
	//#15949 :
	static std::string getPairStateAsString(int ps);
	static void printVolumeSettings(VOLUME_GROUP_SETTINGS &, CDPSETTINGS_MAP &);
	static bool printCleanupStatus(std::string &);
	static bool printMaintainenceStatus(std::string & maintainence_actions);
	static std::string convertTimeToDiplayableString(unsigned int timeInHrs);
    static std::string format_time_for_display(const unsigned long long &time_hns);
    static bool parse_and_get_specifiedcolumn(const std::string& filename, const std::string& delim,const int& column,std::vector<std::string>&);
	static bool get_filenames_withpattern(const std::string& dirname, const std::string& pattern, std::vector<std::string>& filelist, unsigned int limit = CDPUtil::NoLimit);

	static bool validate_cdp_settings(std::string volume_name,CDP_SETTINGS &);
	static bool get_lowspace_value(const std::string &retentiondir,SV_ULONGLONG &trigger);


	static std::string get_resync_required_path(const std::string & volume_name);
	static bool store_and_sendcs_resync_required(const std::string & volume_name, const std::string& msg,
        const ResyncReasonStamp &resyncReasonStamp, bool skip_cs_reporting = false, long errorcode = RESYNC_REPORTED_BY_MT);
	static void persist_resync_required(const std::string & resync_required_path);
	static void store_resync_required_history(const std::string &volume_name, const std::string & resync_history, const std::string& msg);
	static bool send_resyncrequired_to_cs(const std::string & volume_name, const std::string &msg,
        const std::map<std::string, std::string> &reason, long errorcode);

	static const std::string CDPRESERVED_FILE;
};


std::ostream & operator <<( std::ostream & o, const SV_GUID & guid);
std::ostream & operator <<( std::ostream & o, const SVD_HEADER1 & hdr);
std::ostream & operator <<( std::ostream & o, const SVD_HEADER_V2 & hdr);
std::ostream & operator <<( std::ostream & o, const SVD_TIME_STAMP & ts);
std::ostream & operator <<( std::ostream & o, const SVD_TIME_STAMP_V2 & ts);
std::ostream & operator <<( std::ostream & o, const SV_ULARGE_INTEGER & lodc);
std::ostream & operator <<( std::ostream & o, const SV_MARKER & marker);
//std::ostream & operator <<( std::ostream & o, const cdp_drtd_t & drtd);
std::ostream & operator <<( std::ostream & o, const cdp_drtdv2_t & drtd);
std::ostream & operator <<( std::ostream & o, const SVD_DATA_INFO & data);
std::ostream & operator <<( std::ostream & o, const CDPV3BookMark & bkmk);
std::ostream & operator <<( std::ostream & o, const CDPV3RecoveryRange & timerange);

std::ostream & operator <<(std::ostream & o, const CsJob &job);
std::ostream & operator <<(std::ostream & o, const CsJobContext &jobContext);
std::ostream & operator <<(std::ostream & o, const std::list<CsJobError> & errors);
std::ostream & operator <<(std::ostream & o, const CsJobError &error);

#endif
