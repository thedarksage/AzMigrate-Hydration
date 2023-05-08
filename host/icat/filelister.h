#ifndef FILE_LISTER_H
#define FILE_LISTER_H

#include <string>
#include <ace/Task.h>
#include <ace/Mutex.h>
#include <list>

#include "defs.h"
#include "filesystemobj.h"
#include "filefilter.h"
#include "icatqueue.h"

#include "event.h"
#include "s2libsthread.h"
#include "configvalueobj.h"


struct FileListerValueObj
{
	std::string m_contentSrcDir ;
	std::string m_dir ;
	FileFilter m_filter ;
	unsigned long long m_contentSrcId ;
	unsigned long long m_folderId ;
	bool m_recurse ;
	FileListerValueObj(const std::string& contentSrcDir, unsigned long long contentSrcId , const std::string& dir, unsigned long long folderId, const FileFilter& filter, bool recurse)
	{
		m_contentSrcDir = contentSrcDir ;
		m_dir = dir ;
		m_filter = filter ;
		m_recurse = recurse ;
		m_folderId = folderId ;
		m_contentSrcId = contentSrcId ;
	}
} ;

typedef std::list<FileListerValueObj> FileListerValueObj_t ;

class FileLister : public ACE_Task<ACE_MT_SYNCH>
{
	FileListerValueObj_t m_objs ;
	ArcQueuePtr m_queuePtr ;
	ACE_Mutex m_lock ;
	bool m_synch ;
	ConfigValueObject * m_config ;
	static FileListerValueObj_t m_listerValueObjs ;

	FileLister(const FileListerValueObj_t& obj, ArcQueuePtr queuePtr) ;
	FileLister(const FileListerValueObj& obj, ArcQueuePtr queuePtr) ;
	int doArchive() ;
public:
	FileLister(ArcQueuePtr queuePtr)  ;
	bool parseDir(const std::string& contentSrcDir, unsigned long long contentSrcId , unsigned long long folderId, const std::string& dir, const std::string currentDir, FileFilter& filter, bool recurse) ;
	bool readFileListFromFile();
	virtual int svc()  ;
    virtual int open(void *args = 0);
    virtual int close(u_long flags = 0);

	void addListerObj(FileListerValueObj) ;
	static bool getNextLevelDirs(const std::string& baseDir, const std::string& dir, unsigned long long contentSrcId, std::list<std::string>& dirList,bool& DirListTimeout) ;
	static FileListerValueObj_t& getFileListerValueObjs() ;

	bool isProcessing() ;
	bool shouldProcess() ;
	ArcQueuePtr getQueue() ;
	int fileListerObjsCount()  { return (int) m_objs.size() ; }
} ;

typedef boost::shared_ptr<FileLister> FileListerPtr ;
#endif

