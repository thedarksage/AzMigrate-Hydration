#ifndef RESUME_TRACKER_H
#define RESUME_TRACKER_H
#include "ace/OS_NS_sys_stat.h"
#include "icatqueue.h"
#include "defs.h"
#include <string>
#include "connection.h"
#include <list>
#include <map>

enum ICAT_RESUME_TRACKER_MODE 
{ 
	ICAT_TRACKER_MODE_NONE, 
	ICAT_TRACKER_MODE_TRACK, 
	ICAT_TRACKER_MODE_RESUME 
} ;

enum ICAT_RESUME_OBJECT_STATUS 
{
	ICAT_RESOBJ_NONE, 
	ICAT_RESOBJ_SUCCESS, 
	ICAT_RESOBJ_FAILURE, 
	ICAT_RESOBJ_PENDINGDEL, 
	ICAT_RESOBJ_DELETED, 
	ICAT_RESOBJ_DELETE_FAILED,
	ICAT_RESOBJ_STAT_FAILED,
	ICAT_RESOBJ_CANTDELETE_FOLDER
} ;

enum ICAT_RESUME_DELETE_STAT 
{ 
	ICAT_DELOBJ_MORE,  
	ICAT_DELOBJ_NOMORE 
} ;

struct JobStatus
{
	unsigned long long m_configId ;
	unsigned long long m_instanceId ;
	unsigned long long m_contentsrcId ;
	std::string m_destinationPath ;
	std::string m_contentsrcName ;
	time_t m_startTime ;
	time_t m_endTime ;
	ICAT_RESUME_OBJECT_STATUS m_status ;
} ;

struct FileStatus
{
	unsigned long long m_configId; 
	unsigned long long m_instanceId ;
	unsigned long long m_directoryId ;
	std::string m_filename ;
	time_t m_modifiedTime ;
	std::string m_checksum ;
	unsigned long long m_size ;
	ICAT_RESUME_OBJECT_STATUS m_status ;
} ;

struct DirectoryStatus
{
	unsigned long long m_configId ;
	unsigned long long m_instanceId ;
	unsigned long long m_contentsrcId ;
	unsigned long long m_directoryId ;
	std::string m_dirPath ;
	ICAT_RESUME_OBJECT_STATUS m_status ;
} ;

struct ContentSrc
{
	unsigned long long m_configId ;
	unsigned long long m_instanceId ;
	unsigned long long m_contentSrcId ;
	unsigned long long m_contentSrcPath ;
	ICAT_RESUME_OBJECT_STATUS m_status ;
} ;

struct Master
{
	unsigned long long m_configId ;
	std::string m_remoteOffice ;
	std::string m_serverName ;
	std::string m_srcVolume ;
} ;


struct ResumeTrackerObj
{
	unsigned long long m_configId ;
	unsigned long long m_instanceId ;
	unsigned long long m_contentSrcId ;
	unsigned long long m_dirId ;
	std::string m_contentSrcPath ;
	std::string m_dirPath ;
	std::string m_fileName ;
	std::string m_tgtPath ;
	bool m_isDir ;

	ICAT_RESUME_OBJECT_STATUS m_status ;
} ;
struct ResumeValueObject;
struct ResumeValueObject
{
	std::string m_fileName ;
	ICAT_RESUME_OBJECT_STATUS m_status ;
	static bool shouldResume(const std::list<ResumeValueObject> &list, const std::string file )
	{
		bool shouldResume = true ;
		std::list<ResumeValueObject>::const_iterator begin = list.begin() ;
		while( begin != list.end() )
		{
			ResumeValueObject obj = (*begin) ;
			if( obj.m_fileName.compare(file) == 0 && obj.m_status == ICAT_RESOBJ_SUCCESS )
			{
				shouldResume = false ;
				break ;
			}
			begin++ ;
		}
		return shouldResume ;
	}
} ;

typedef std::list<ResumeValueObject> ResumeValueObjList ;


class ResumeTracker 
{
	ICAT_RESUME_TRACKER_MODE  m_mode ;
public :
	unsigned long long m_configId ;
	unsigned long long m_instanceId ;
	std::string m_branchName ; 
	std::string m_serverName ;
	std::string m_volName ;
	std::string m_tgtPath ;
	unsigned int m_maxCopies ;
	unsigned int m_maxDays ;
	std::string m_lastRunTime ;
	std::map<unsigned long long, std::string> m_contentSrcIdMap ;
	std::list<JobStatus> m_deleteList ;
	ResumeTracker() {}
	ResumeTracker(const std::string & branchName, 
							const std::string& serverName, 
							const std::string& volName,
							const std::string& targetPath,
							unsigned int maxCopies,
							unsigned int maxDays,
							ICAT_RESUME_TRACKER_MODE trackMode=ICAT_TRACKER_MODE_NONE) ;
	ICAT_RESUME_TRACKER_MODE  mode() ;

	virtual void init() = 0 ;
	virtual void fetchResumeValueObjList(unsigned long long folderId, ResumeValueObjList & list) = 0 ;
	virtual void addDirectoryStatus(std::string dirName, std::string contentSrcName, unsigned long long contentsrcId, unsigned long long& dirId, ICAT_RESUME_OBJECT_STATUS  status=ICAT_RESOBJ_NONE) = 0 ;
	virtual void addContentSrcStatus(const std::string& contentSrcName, unsigned long long& contentSrcId, ICAT_RESUME_OBJECT_STATUS  status=ICAT_RESOBJ_NONE) = 0 ;
	virtual void updateFileStatus(const std::string& fileName, const ACE_stat& stat, unsigned long long dirId, ICAT_RESUME_OBJECT_STATUS status=ICAT_RESOBJ_NONE) = 0 ;
	virtual void updateFolderStatus(unsigned long long folderId, ICAT_RESUME_OBJECT_STATUS status) = 0 ;
	virtual bool isResumeRequired(unsigned long long contentSrcId) = 0 ;
	virtual void setStartTime() = 0 ;
	virtual void setEndTime() = 0 ;
	virtual std::string getNormalModeBasePath() = 0 ;
	virtual void getLastRunTime(std::string & getLastRunTime) = 0;
	virtual void getDeleteableObjects(std::list<ResumeTrackerObj> &deleteList, int size) = 0 ;
	virtual void updateDeleteStatus(const std::string& fileName, unsigned long long folderId , bool isDir ,ICAT_RESUME_OBJECT_STATUS ) = 0 ;
	virtual bool shouldtryForResume(unsigned long long folderId, std::string fileName) =0 ;
	virtual void addDirectoryAndFileStatus(std::string absPath, unsigned long long contentSrcId, ICAT_RESUME_OBJECT_STATUS  status=ICAT_RESOBJ_NONE) = 0 ;
	virtual void markContentSourceStatus(unsigned long long contentsrcId, ICAT_RESUME_OBJECT_STATUS status) = 0 ;
	virtual void canRunInResume() = 0 ;
	virtual void markDeletePendingToSuccess() = 0 ;
} ;

class SqliteResumeTracker : public ResumeTracker
{
	SqliteConnection m_con ;
	void initMasterInfo() ;
	void initJobStatusInfo() ;
	void initDirStatusInfo() ;
	void initFileStatusInfo() ;
	void initContentSourceInfo() ;
	void initResumeMode() ;
	void initTrackerMode() ;
	void markContentSourceByFolderId(unsigned long long folderId, ICAT_RESUME_OBJECT_STATUS status) ;	
	void finalize() ;
	void prepareTargetPath() ;
	void getInstanceId() ;
	void createInstanceId() ;
	void getConfigId() ;
	void createConfigId() ;
	void setStartTime() ;
	void removeFromFileStatus(unsigned long long folderId, std::string fileName) ;
	void removeFromFolderStatus(unsigned long long folderId) ;	
	void addDirectoryStatus(std::string dirName, unsigned long long contentSrcId, unsigned long long& dirId, ICAT_RESUME_OBJECT_STATUS  status=ICAT_RESOBJ_NONE) ;
	void getDeleteableDirs(std::list<ResumeTrackerObj>& deleteList, int size) ;
	void getDeleteableFiles(std::list<ResumeTrackerObj>& deleteList, int size) ;

public :
	SqliteResumeTracker(const std::string & branchName, 
									const std::string& serverName, 
									const std::string& volName,
									const std::string& targetPath, 
									unsigned int maxCopies,
									unsigned int maxDays,
									ICAT_RESUME_TRACKER_MODE trackMode=ICAT_TRACKER_MODE_NONE) ;


	void init() ;
	void fetchResumeValueObjList(unsigned long long folderId, ResumeValueObjList & list) ;
	void addDirectoryStatus(std::string dirName, std::string contentSrcName, unsigned long long contentsrcId, unsigned long long& dirId, ICAT_RESUME_OBJECT_STATUS  status=ICAT_RESOBJ_NONE) ;
	void addDirectoryAndFileStatus(std::string absPath, unsigned long long contentSrcId, ICAT_RESUME_OBJECT_STATUS  status=ICAT_RESOBJ_NONE) ;
	void addContentSrcStatus(const std::string& contentSrcName, unsigned long long& contentSrcId, ICAT_RESUME_OBJECT_STATUS  status=ICAT_RESOBJ_NONE) ;
	void updateFileStatus(const std::string& fileName, const ACE_stat& stat, unsigned long long dirId, ICAT_RESUME_OBJECT_STATUS status=ICAT_RESOBJ_NONE) ;
	void updateFolderStatus(unsigned long long folderId, ICAT_RESUME_OBJECT_STATUS status) ;
	void setEndTime() ;
	void getLastRunTime(std::string & getLastRunTime) ;
	std::string getNormalModeBasePath() ;
	bool isResumeRequired(unsigned long long contentSrcId) ;
	void prepareDeleteJobList() ;
	void getDeleteableObjects(std::list<ResumeTrackerObj> &deleteList, int size) ;
	void updateDeleteStatus(const std::string& fileName, unsigned long long folderId , bool isDir, ICAT_RESUME_OBJECT_STATUS ) ;
	bool shouldtryForResume(unsigned long long folderId, std::string fileName) ;
	void canRunInResume() ;
	void markContentSourceStatus(unsigned long long contentsrcId, ICAT_RESUME_OBJECT_STATUS status) ;
    void markDeletePendingToSuccess();
} ;
class EmptyTracker : public  ResumeTracker
{
public:
    void init() { }
    void fetchResumeValueObjList(unsigned long long folderId, ResumeValueObjList & list) {}
    void addDirectoryStatus(std::string dirName, std::string contentSrcName, unsigned long long contentsrcId, unsigned long long& dirId, ICAT_RESUME_OBJECT_STATUS  status=ICAT_RESOBJ_NONE) {}
    void addContentSrcStatus(const std::string& contentSrcName, unsigned long long& contentSrcId, ICAT_RESUME_OBJECT_STATUS  status=ICAT_RESOBJ_NONE) {}
    void updateFileStatus(const std::string& fileName, const ACE_stat& stat, unsigned long long dirId, ICAT_RESUME_OBJECT_STATUS status=ICAT_RESOBJ_NONE) {}
    void updateFolderStatus(unsigned long long folderId, ICAT_RESUME_OBJECT_STATUS status) {}
    bool isResumeRequired(unsigned long long contentSrcId) {return false ; }
    void setStartTime() {}
    void setEndTime() {}
    std::string getNormalModeBasePath() {return "" ; }
    void getLastRunTime(std::string & getLastRunTime){} 
    void getDeleteableObjects(std::list<ResumeTrackerObj> &deleteList, int size) {}
    void updateDeleteStatus(const std::string& fileName, unsigned long long folderId , bool isDir ,ICAT_RESUME_OBJECT_STATUS ){}
    bool shouldtryForResume(unsigned long long folderId, std::string fileName) {return  false ; }
    void addDirectoryAndFileStatus(std::string absPath, unsigned long long contentSrcId, ICAT_RESUME_OBJECT_STATUS  status=ICAT_RESOBJ_NONE){}
    void markContentSourceStatus(unsigned long long contentsrcId, ICAT_RESUME_OBJECT_STATUS status) {}
	void canRunInResume() {}
	void markDeletePendingToSuccess(){} 

};
typedef boost::shared_ptr<SqliteResumeTracker> SqliteResumeTrackerPtr ;
typedef boost::shared_ptr<EmptyTracker> EmptyTrackerPtr;
#endif
