#include "filelister.h"
#include <ace/OS_Dirent.h>
#include "archivecontroller.h"
#include "icatexception.h"
#include "resumetracker.h"
#include <map>
FileListerValueObj_t FileLister::m_listerValueObjs ;
typedef std::map<std::string, unsigned long long> DirectoryTableMap;
/*
* Function Name: fileLister(const std::list<FileListerValueObj>& obj, ArcQueuePtr queue)
* Arguements: 
*					obj - List of file lister value objects
*					queue - ArcQueuePtr which is a wrapper for ACE_SHARED_MQ_Ptr
* Return Value: None
*
* Description: This creates a file lister object 
* Called by:
* Calls:
* Issues Fixed:
*   Issue:
*   Root Cause:
*   Fix:
*/

FileLister::FileLister(const FileListerValueObj_t& obj, ArcQueuePtr queuePtr)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	DebugPrintf(SV_LOG_DEBUG, "Created a flieLister with %d objects\n", obj.size()) ;
	m_queuePtr = queuePtr ;
	m_objs = obj ;
	m_synch = true ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

/*
* Function Name: fileLister(const FileListerValueObj& obj, ArcQueuePtr queue)
* Arguements: 
*					obj - file lister value object
*					queue - ArcQueuePtr which is a wrapper for ACE_SHARED_MQ_Ptr
* Return Value: None
*
* Description: This creates a file lister object 
* Called by:
* Calls:
* Issues Fixed:
*   Issue:
*   Root Cause:
*   Fix:
*/
FileLister::FileLister(const FileListerValueObj& obj, ArcQueuePtr queuePtr) 
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	m_queuePtr = queuePtr ;
	m_objs.push_back( obj) ;
	m_synch = true ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

/*
* Function Name: fileLister(ArcQueuePtr queue) 
* Arguements: 
*					queue - ArcQueuePtr which is a wrapper for ACE_SHARED_MQ_Ptr
* Return Value: None
*
* Description: This creates a file lister object 
* Called by:
* Calls:
* Issues Fixed:
*   Issue:
*   Root Cause:
*   Fix:
*/
FileLister::FileLister(ArcQueuePtr queuePtr) 
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	m_queuePtr = queuePtr ;
	m_synch = true ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}


int FileLister::doArchive()
{

	ConfigValueObject *conf = ConfigValueObject::getInstance() ;
	std::string FilelistFromSource = conf->getFilelistFromSource(); 
	if(FilelistFromSource.empty() == true)
	{
		FileListerValueObj_t::iterator listerObjsIter = m_objs.begin() ;
		while( listerObjsIter != m_objs.end() )
		{
			FileListerValueObj& obj = (*listerObjsIter) ;
			DebugPrintf(SV_LOG_DEBUG, "The directory traversal started for the directory %s\n", obj.m_dir.c_str()) ;
			if( !this->parseDir(obj.m_contentSrcDir, obj.m_contentSrcId, obj.m_folderId, obj.m_dir, "", obj.m_filter, obj.m_recurse) )
			{
				DebugPrintf(SV_LOG_ERROR, "Failed to parse the directory %s\n", obj.m_dir.c_str()) ;
			}
			DebugPrintf(SV_LOG_DEBUG, "The directory traversal finished for the directory %s\n", obj.m_dir.c_str()) ;
			listerObjsIter ++ ;
		}
        m_objs.clear() ;
	}
	else
	{
	   readFileListFromFile();
    }
	archiveController::getInstance() ->postMsg(ICAT_MSG_FL_DONE, ICAT_MSGPRIO_NORMAL, this) ;
	m_queuePtr->setState(ARC_Q_DONE) ;
	return 0 ;
}

/*
* Function Name	: svc
* Arguements		: None 
* Return Value		: int
*
* Description		: File Lister's Service Code. 
* Called by			: Called by Archive controller object's run method or waitforarchiveTasks
* Calls				: ParseDir
* Issues Fixed:
*   Issue:
*   Root Cause:
*   Fix:
*/
int FileLister::svc()
{
	int retVal = 0 ;
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	DebugPrintf(SV_LOG_DEBUG, "Thread started. (File Lister)\n") ;
	m_config = ConfigValueObject::getInstance() ;
	try
	{
		if( m_queuePtr.get() == NULL )
		{
			DebugPrintf(SV_LOG_DEBUG, "No process queue attached to this file lister thread and exiting\n") ;
			throw icatException("FILE LISTER : The file lister's service method called without assigning a proper Queue") ;  
		}
		doArchive() ;
	}
	catch(icatOutOfMemoryException& oome)
	{
		DebugPrintf(SV_LOG_DEBUG, "Exception : %s\n", oome.what()) ;
		archiveController::getInstance()->QuitSignal() ;
		retVal = -1 ;
	}
	catch(icatException &e)
	{
		DebugPrintf(SV_LOG_ERROR, "FILE LISTER : %s\n", e.what()) ;
		archiveController::getInstance()->QuitSignal() ;
		retVal = -1 ;
	}
	DebugPrintf(SV_LOG_DEBUG, "Thread exited. (File Lister)\n") ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	return retVal ;
}

time_t ConvertDatetosecs(std::string dateStr)
{
    struct tm timeinfo = { 0 } ;
    int month,year,day;
	int hour, min, sec ;
    time_t timeVal;
	if( dateStr.compare("0000-00-00 00:00:00") == 0 )
	{
		return 0 ;
	}
	sscanf(dateStr.c_str(),"%d-%d-%d %d:%d:%d",&year,&month,&day, &hour, &min, &sec); 
    timeinfo.tm_year = year - 1900;
    timeinfo.tm_mon = month - 1;
    timeinfo.tm_mday = day;
	timeinfo.tm_hour = hour ;
	timeinfo.tm_min = min ;
	timeinfo.tm_sec = sec ;
    timeVal = mktime ( &timeinfo );
    return timeVal;
}


time_t ConvertDatetotime(std::string dateStr)
{
    struct tm timeinfo = { 0 } ;
    int month,year,day;
    time_t timeVal;
	sscanf(dateStr.c_str(),"%d-%d-%d",&year,&month,&day); 
    timeinfo.tm_year = year - 1900;
    timeinfo.tm_mon = month - 1;
    timeinfo.tm_mday = day;
    timeVal = mktime ( &timeinfo );
    return timeVal;
}

bool FileLister::readFileListFromFile()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	fstream filestr;
	char buf[256];

	std::string absPath;
	std::string basedir;
	std::string fileName;
	bool RetVal= true;

	ConfigValueObject *temp_obj = ConfigValueObject::getInstance() ;
	
	std::string FileListPath = temp_obj->getFilelistFromSource();
	filestr.open (FileListPath.c_str());
	if(filestr.fail())
	{
		throw ParseException("FAILED to Read  Filelist file\n"); 
	}
	
	int buffer_length = 0;    

	filestr.seekg (0, ios::end);
	buffer_length = filestr.tellg();
	filestr.seekg (0, ios::beg);
	boost::shared_ptr<ResumeTracker>  trackerPtr = archiveController::getInstance()->resumeTrackerInstance() ;

    DirectoryTableMap directoryTable ;
	while( !archiveController::getInstance()->QuitSignalled() && 
	       buffer_length != filestr.tellg())
	{
		filestr.getline(buf,256,'\n');
		absPath = buf;
		std::string DirPath = absPath.substr(0,absPath.find_last_of(ACE_DIRECTORY_SEPARATOR_CHAR_A));
		int index  = DirPath.find_first_of(ACE_DIRECTORY_SEPARATOR_CHAR_A);
        std::string contentSrcDirName;
        if(index != std::string::npos)
        {
#ifdef WIN32
		contentSrcDirName = DirPath.substr(0,index);
		contentSrcDirName.erase( contentSrcDirName.find_last_not_of("\\") + 1 );
		if( contentSrcDirName.length() == 2 )
		{
			contentSrcDirName += "\\" ;
		}
		if( contentSrcDirName.length() == 1 )
		{
			contentSrcDirName += ":\\" ;
		}
#else
		contentSrcDirName ='/';
		if( contentSrcDirName.length() != 1 )
		{
			contentSrcDirName.erase( contentSrcDirName.find_last_not_of("/") + 1);
		}
#endif

       	} 
		unsigned long long contentSrcDirId = 0 ;
        trackerPtr->addContentSrcStatus(contentSrcDirName, contentSrcDirId) ;
		unsigned long long folderId ;
		if( directoryTable.size() > 0 && directoryTable.find( DirPath ) != directoryTable.end() )
		{
			DebugPrintf(SV_LOG_DEBUG, "The Dirpath %s is existing map\n", DirPath.c_str()) ;
			folderId = directoryTable.find( DirPath )->second;
		}
		else
		{
			trackerPtr->addDirectoryStatus(DirPath, contentSrcDirName,contentSrcDirId, folderId, ICAT_RESOBJ_SUCCESS) ;
			std::pair<DirectoryTableMap::iterator, bool> bRet ;
			bRet = directoryTable.insert(std::pair<std::string, unsigned long long>(DirPath,folderId)) ;		
			if( ! bRet.second )
			{
				DebugPrintf(SV_LOG_DEBUG, "The Dirpath %s is already existing map\n", DirPath.c_str()) ;
			}
		}
		
		fileName = absPath.substr(absPath.find_last_of(ACE_DIRECTORY_SEPARATOR_CHAR_A)+1);
		ACE_stat stat ;
		ACE_Message_Block *mb ;
		int statRetVal;
		// PR#10815: Long Path support
		if( (statRetVal = sv_stat( getLongPathName(absPath.c_str()).c_str(), &stat )) != 0 )
		{
			DebugPrintf(SV_LOG_ERROR, "FILE LISTER : ACE_OS::stat for %s is failed with error %d \n", absPath.c_str(), statRetVal) ;
		}
		if( m_config->getResume() )
		{
			if( !trackerPtr->shouldtryForResume(folderId, fileName) )
			{
				DebugPrintf(SV_LOG_DEBUG, "The Value Object %s is skipped as it is succeeded in normal mode\n", absPath.c_str()) ;
				continue ;
			}
		}
		fileSystemObj * msg = new (std::nothrow) fileSystemObj(absPath, 
																	absPath.substr(contentSrcDirName.length()), 
																	fileName, 
																	contentSrcDirId, 
																	folderId,													
																	stat) ;
		if( msg == NULL )
		{
			throw icatOutOfMemoryException("Failed to create the file system object\n") ;
		}
		mb = new (std::nothrow) ACE_Message_Block(sizeof(*msg),ACE_Message_Block::MB_DATA, NULL, (const char*) msg) ;
		if( mb == NULL )
		{
			throw icatOutOfMemoryException("Failed to create message block\n") ;
		}
		m_queuePtr->queue()->enqueue_tail(mb) ;			
	}
		
	filestr.close();
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	return RetVal;
}



/*
* Function Name	: parseDir
* Arguements		: 
*							basedir		: The basedir (eg., volume, mount point that comes from the config parameter)
*							dir1			: The directory that is parsed by the function (this comes from the svc() code and doesnt change
*							currentDir	: The directory that is parsed by this function. It changes for every recursive call this function makes.
*							filter			: filter object 
*							recurse		: should recurse or not( true : in case of recursion of sub directories is required)
* Return Value		: bool
* Description		: File Lister's Service Code. 
* Called by			: Called by Archive controller object's run method or waitforarchiveTasks
* Calls				: own, file filter's functions
* Issues Fixed:
*   Issue:
*   Root Cause:
*   Fix:
*/
bool FileLister::parseDir(const std::string& basedir, 
						  unsigned long long contentSrcId ,
						  unsigned long long folderId, 
						  const std::string& dir1, 
						  const std::string currentDir, 
						  FileFilter &filter, 
						  bool recurse)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	DebugPrintf(SV_LOG_DEBUG, "directory: \"%s\", current dir: \"%s\"\n", dir1.c_str(), currentDir.c_str()) ;
	std::string filelistToTarget = m_config->getFilelistToTarget();
    boost::shared_ptr<ResumeTracker> tracker = archiveController::getInstance()->resumeTrackerInstance() ;
	ACE_stat stat ;
	bool retVal ;
	ACE_DIR * ace_dir = NULL ;
	ACE_DIRENT * dirent = NULL ;
	std::string dir = dir1 ;
	int statRetVal = 0 ;
	
	if( currentDir.compare("") != 0 )
	{
		dir += ACE_DIRECTORY_SEPARATOR_CHAR_A + currentDir ;
	}
	// PR#10815: Long Path support
	if((statRetVal  = sv_stat(getLongPathName(dir.c_str()).c_str(),&stat)) != 0 )
	{
		DebugPrintf(SV_LOG_ERROR, "FILE LISTER :Unable to stat %s. ACE_OS::stat returned %d\n", dir.c_str(), statRetVal) ; 
		tracker->markContentSourceStatus(contentSrcId, ICAT_RESOBJ_STAT_FAILED) ;
		retVal = false;
	}
	else
	{
		ResumeValueObjList valObjList ;
		// PR#10815: Long Path support
		ace_dir = sv_opendir(dir.c_str()) ;
		if( ace_dir != NULL )
		{
			do
			{
				dirent = ACE_OS::readdir(ace_dir) ;
				bool isDir = false ;
				if( dirent != NULL )
				{
					std::string fileName = ACE_TEXT_ALWAYS_CHAR(dirent->d_name);
					if( strcmp(fileName.c_str(), "." ) == 0 || strcmp(fileName.c_str(), ".." ) == 0 )
					{
						continue ;
					}
					std::string absPath = dir + ACE_DIRECTORY_SEPARATOR_CHAR_A + fileName ;
					// PR#10815: Long Path support
					if( (statRetVal = sv_stat( getLongPathName(absPath.c_str()).c_str(), &stat )) != 0 )
					{
						DebugPrintf(SV_LOG_ERROR, "FILE LISTER : ACE_OS::stat for %s is failed with error %d \n", absPath.c_str(), statRetVal) ;
							tracker->markContentSourceStatus(contentSrcId, ICAT_RESOBJ_STAT_FAILED) ;
						continue ;
					}
					if( stat.st_mode & S_IFDIR )
					{
						isDir = true ;
					}

					if( isDir )
					{
						if( !recurse ) 
						{
							DebugPrintf(SV_LOG_DEBUG, "Not recursing into %s\n", absPath.c_str()) ;
							continue ;
						}
					}
					if( !isDir && ConfigValueObject::getInstance()->getFromLastRun() )
					{
						std::string lastRunTimeStr ;
							tracker->getLastRunTime(lastRunTimeStr) ;
						if( stat.st_mtime < ConvertDatetosecs(lastRunTimeStr) )
						{
							std::string fileModTime ;
							getTimeToString("%Y-%m-%d %H:%M:%S", fileModTime, &(stat.st_mtime)) ;
							DebugPrintf(SV_LOG_DEBUG, "File : %s modification Time : %s. Skipping it\n", absPath.c_str(), fileModTime.c_str()) ;
							continue ;
						}
					}
					if( isDir )
					{
						

						bool Rval = filter.patternMatch(absPath);
						if( !Rval)
						{
							DebugPrintf(SV_LOG_DEBUG, "Directory %s is filtered\n", absPath.c_str()) ;
							continue;
						}
					}
								
					bool RetVal = filter.filter( fileName.c_str(), isDir, stat ) ;
					if( !isDir )
					{
						RetVal = filter.m_filterInfo. m_includeFileFilter ? RetVal : !RetVal ;
						if( !RetVal)
						{
							DebugPrintf(SV_LOG_DEBUG, "File %s is filtered\n", absPath.c_str()) ;
							continue ;
						}
					}
					else
					{
						if(!RetVal)
						{
							DebugPrintf(SV_LOG_DEBUG, "Directory %s is filtered\n", absPath.c_str()) ;
							continue ;
						}
						std::string childDir = currentDir ;
						if( childDir.compare("") != 0 )
						{
							childDir += ACE_DIRECTORY_SEPARATOR_CHAR_A ;
						}
						childDir += fileName ;
						unsigned long long childsFolderId ;
							tracker->addDirectoryStatus(absPath, basedir, contentSrcId, childsFolderId) ;
						parseDir(basedir, contentSrcId, childsFolderId, dir1, childDir, filter, recurse);
					}
					if( !isDir )
					{
						if( m_config->getResume() )
						{
								if( !tracker->shouldtryForResume(folderId, fileName.c_str()) )
								{
									DebugPrintf(SV_LOG_DEBUG, "The Value Object %s is skipped as it is succeeded in normal mode\n", absPath.c_str()) ;
									continue ;
								}
						}
						ACE_Message_Block *mb ;
						fileSystemObj * msg; 
						std::string FilelistToTarget = m_config->getFilelistToTarget();
						ICAT_OPERATION opCode = ICAT_OPER_ARCH_FILE ;
						if(FilelistToTarget.empty() == false)
						{
							opCode = ICAT_OPER_LISTINTOFILE ;						
						}	
						msg = new (std::nothrow) fileSystemObj( absPath,
                                                                absPath.substr(basedir.length()+1),
                                                                fileName.c_str(),
                                                                contentSrcId,
                                                                folderId,
                                                                stat,
    															opCode ) ;

						if( msg == NULL )
						{
							throw icatOutOfMemoryException("Failed to create the file system object\n") ;
						}
						mb = new (std::nothrow) ACE_Message_Block(sizeof(*msg),ACE_Message_Block::MB_DATA, NULL, (const char*) msg) ;
						if( mb == NULL )
						{
							throw icatOutOfMemoryException("Failed to create message block\n") ;
						}
						DebugPrintf(SV_LOG_DEBUG, "Enqueued %s to process queue\n", msg->absPath().c_str()) ;
						m_queuePtr->queue()->enqueue_tail(mb) ;
						
					}
				}
			} while ( !archiveController::getInstance()->QuitSignalled() && dirent != NULL  ) ;
			ACE_OS::closedir( ace_dir ) ;
			retVal = true ;
		}
		else
		{
				tracker->addDirectoryStatus(dir, basedir, contentSrcId,  folderId, ICAT_RESOBJ_FAILURE) ;
			DebugPrintf(SV_LOG_ERROR, "ACE_OS::open_dir failed for %s.\n", dir.c_str()) ;
			retVal = false ;		
		}
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	return retVal ;
}

/*
* Function Name	: addListerObj
* Arguements		: 
*							obj			: The file lister's value object 
* Return Value		: None
* Description		: This adds file lister value object to the m_objs 
* Called by			: 
* Calls				: own, file filter's functions
* Issues Fixed:
*   Issue:
*   Root Cause:
*   Fix:
*/
void FileLister::addListerObj(FileListerValueObj obj)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	this->m_objs.push_back(obj) ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

FileListerValueObj_t& FileLister::getFileListerValueObjs()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	
	ConfigValueObject *obj = ConfigValueObject::getInstance() ;
	boost::shared_ptr<ResumeTracker> trackerPtr = archiveController::getInstance()->resumeTrackerInstance() ;
	std::list<ContentSource> srcList = obj->getContentSrcList() ;
	int processorQueuesCount = obj->getMaxConnects() ;
	std::string filelistToTarget = obj->getFilelistToTarget();
	
	std::list<ContentSource>::iterator srcIter = srcList.begin() ;
	
	while( srcIter != srcList.end() )
	{
		const ContentSource src = (*srcIter) ;
		srcIter++ ;
        FilterInfo finfo ;
		
		//erase all leading and trailing spaces
		std::string contentSrcDirName = src.m_szDirectoryName ;
#ifdef WIN32
		contentSrcDirName.erase( contentSrcDirName.find_last_not_of("\\") + 1 );
		if( contentSrcDirName.length() == 2 )
		{
			contentSrcDirName += "\\" ;
		}
		if( contentSrcDirName.length() == 1 )
		{
			contentSrcDirName += ":\\" ;
		}
#else
		if( contentSrcDirName.length() != 1 )
		{
			contentSrcDirName.erase( contentSrcDirName.find_last_not_of("/") + 1);
		}
#endif
		unsigned long long contentSrcDirId = 0 ;
			trackerPtr->addContentSrcStatus(contentSrcDirName, contentSrcDirId) ;
			if( obj->getResume() )
			{
				if( !obj->getForceRun() && !trackerPtr->isResumeRequired(contentSrcDirId) )
				{
					DebugPrintf(SV_LOG_DEBUG, "Nothing to resume for content source  %s Id  %I64u\n", contentSrcDirName.c_str(), contentSrcDirId) ;
					continue ;
				}
			}
		finfo.m_NamePattern = src.m_szFilePattern ;
		finfo.m_ExcludeDirs = src.m_szExcludeList ;
		std::string timefilterVal = src.m_szMtime ;
		finfo.m_SearchFromTime = ConvertDatetotime(src.m_szMtime);
		finfo.m_FileSize = src.m_Size ;
		finfo.m_includeFileFilter = src.m_Include;
		finfo.m_szfilterTemplate = src.m_szfilterTemplate;
        finfo.m_szFilePatternOps = src.m_szFilePatternOps;
        finfo.m_szMtimeOps = src.m_szMtimeOps;
        finfo.m_SizeOps = src.m_SizeOps;
		if( src.m_szMtime.compare("") != 0 )
		{
			finfo.m_FilterOp1 = src.m_szFilter1 ;
			finfo.m_FilterOp1.erase( finfo.m_FilterOp1.find_last_not_of(" ") + 1) ; //erase all trailing spaces
			finfo.m_FilterOp1.erase(0 , finfo.m_FilterOp1.find_first_not_of(" ") ) ; //erase all leading spaces
		}
		if( finfo.m_FileSize != -1 )
		{
			finfo.m_FilterOp2 = src.m_szFilter2 ;
			finfo.m_FilterOp2.erase( finfo.m_FilterOp2.find_last_not_of(" ") + 1) ; //erase all trailing spaces
			finfo.m_FilterOp2.erase(0 , finfo.m_FilterOp2.find_first_not_of(" ") ) ; //erase all leading spaces
		}
		FileFilter filter(finfo) ;
		// PR#10815: Long Path support
		ACE_DIR * ace_dir = sv_opendir(contentSrcDirName.c_str()) ;
		unsigned long long folderId = 0 ;
		if( ace_dir != NULL )
		{
				trackerPtr->addDirectoryStatus(contentSrcDirName, contentSrcDirName, contentSrcDirId,  folderId, ICAT_RESOBJ_SUCCESS) ;
			ACE_OS::closedir(ace_dir) ;
		}
		else
		{
				trackerPtr->addDirectoryStatus(contentSrcDirName, contentSrcDirName, contentSrcDirId,  folderId, ICAT_RESOBJ_FAILURE) ;
			DebugPrintf(SV_LOG_ERROR, "Cannot open root directory %s\n", contentSrcDirName.c_str()) ;
			continue ;
		}
		FileListerValueObj valueObj(contentSrcDirName, contentSrcDirId, contentSrcDirName, folderId, filter, true) ;
		m_listerValueObjs.push_back(valueObj) ;
	}
	DebugPrintf(SV_LOG_DEBUG, "Count of file lister value objs before expansion %d\n", m_listerValueObjs.size() ) ;
	
	int lastRunCount = 0 ;
	bool DirListTimeout = false;
	while( (int) m_listerValueObjs.size() < processorQueuesCount )
	{
		if( lastRunCount != (int) m_listerValueObjs.size()) 
		{
			lastRunCount = (int) m_listerValueObjs.size() ;				
		}
		else
		{
			DebugPrintf(SV_LOG_DEBUG, "Can't expand the value object list further. The current Count %d\n", m_listerValueObjs.size()) ;
			break ;
		}

		FileListerValueObj_t::iterator listerValueObjIter = m_listerValueObjs.begin() ;
		
		while( listerValueObjIter != m_listerValueObjs.end() )
		{
			FileListerValueObj& primaryListerValueObj = (*listerValueObjIter) ;
			listerValueObjIter++ ;
			
			if( primaryListerValueObj.m_recurse == true )
			{
				primaryListerValueObj.m_recurse = false ;
				std::string dirName ;
				
				DebugPrintf(SV_LOG_DEBUG, "listerValueObject basedir %s dir %s recurse %s\n", primaryListerValueObj.m_contentSrcDir.c_str(), primaryListerValueObj.m_dir.c_str(), "false") ;
				std::list<std::string> nextLevelDirs ;
				if( getNextLevelDirs(primaryListerValueObj.m_contentSrcDir, primaryListerValueObj.m_dir, primaryListerValueObj.m_contentSrcId, nextLevelDirs,DirListTimeout) )
				{
					std::list<std::string>::iterator dirsIter = nextLevelDirs.begin() ;
					
					
					while( dirsIter != nextLevelDirs.end() )
					{
						std::string dirName = *dirsIter ;
						ACE_stat stat ;
						if(DirListTimeout)
						{
							primaryListerValueObj.m_recurse = true ;
							if(primaryListerValueObj.m_filter.m_filterInfo.m_Dirs.compare("") == 0)
								primaryListerValueObj.m_filter.m_filterInfo.m_Dirs = dirName;
							else
								primaryListerValueObj.m_filter.m_filterInfo.m_Dirs += "," + dirName;
						}
                        if(primaryListerValueObj.m_filter.filter( dirName.substr(dirName.find_last_of(ACE_DIRECTORY_SEPARATOR_CHAR_A)+1),
							true, stat) == false )
						{
							DebugPrintf(SV_LOG_DEBUG, "Directory %s is filtered\n", dirName.c_str()) ;
							dirsIter++  ;
							continue ;
						}

						unsigned long long folderId ;
							trackerPtr->addDirectoryStatus(dirName, primaryListerValueObj.m_contentSrcDir, primaryListerValueObj.m_contentSrcId, folderId, ICAT_RESOBJ_SUCCESS) ;
						FileListerValueObj valueObj(primaryListerValueObj.m_contentSrcDir, primaryListerValueObj.m_contentSrcId , dirName, folderId, primaryListerValueObj.m_filter, true) ;
						DebugPrintf(SV_LOG_DEBUG, "File Lister Value Obj : basedir: \"%s\" dir: \"%s\" recurse: \"%s\"\n", primaryListerValueObj.m_contentSrcDir.c_str(), dirName .c_str(), "true") ;
						m_listerValueObjs.push_back(valueObj) ;
						dirsIter++ ;
					}						
                    if( m_listerValueObjs.size() > obj->getmaxfilelisters() ) 
		            {
			            break ;
		            }
				}
			}
		}	
	}
	DebugPrintf(SV_LOG_DEBUG, "Count of file lister value objs after expansion %d\n", m_listerValueObjs.size() ) ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	return m_listerValueObjs ;
}


bool FileLister::getNextLevelDirs(const std::string& baseDir, const std::string& dir, unsigned long long contentSrcId, std::list<std::string>& dirList ,bool& DirListTimeout)
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
	ACE_DIR * ace_dir = NULL ;
	ACE_DIRENT * dirent = NULL ;
	ACE_stat stat ;
	int statRetVal ;
	bool retVal ;
	ACE_Time_Value wait = ACE_OS::gettimeofday();
	int waitSeconds = 10;
	wait.sec (wait.sec () + waitSeconds);
	boost::shared_ptr<ResumeTracker> tracker = archiveController::getInstance()->resumeTrackerInstance() ;
	unsigned long long folderId ;
	// PR#10815: Long Path support
	if(( statRetVal = sv_stat(getLongPathName(dir.c_str()).c_str(), &stat)) != 0)
	{
		DebugPrintf(SV_LOG_ERROR, "FILE LISTER : ACE_OS::stat for %s failed with return value %d\n", dir.c_str(), statRetVal) ; 
		tracker->markContentSourceStatus(contentSrcId, ICAT_RESOBJ_FAILURE) ;
		retVal = false;
	}
	else
	{
		// PR#10815: Long Path support
		ace_dir = sv_opendir(dir.c_str()) ;
		if( ace_dir != NULL )
		{
			do
			{
				dirent = ACE_OS::readdir(ace_dir);
				bool isDir = false;
				if( dirent != NULL )
				{
					std::string fileName = ACE_TEXT_ALWAYS_CHAR(dirent->d_name);
					if( ! strcmp(fileName.c_str(), "." ) || ! strcmp(fileName.c_str(), ".." ) )
					{
						continue ;
					}
					std::string absPath = dir + ACE_DIRECTORY_SEPARATOR_CHAR_A + fileName.c_str() ;
					// PR#10815: Long Path support
					if( (statRetVal = sv_stat( getLongPathName(absPath.c_str()).c_str(), &stat )) != 0 )
					{
						tracker->markContentSourceStatus(contentSrcId, ICAT_RESOBJ_STAT_FAILED) ;
						DebugPrintf(SV_LOG_ERROR, "ACE_OS::stat for %s is failed with return value %d\n", absPath.c_str(), statRetVal) ;
						retVal = false ;
					}
					if( stat.st_mode & S_IFDIR )
					{
						dirList.push_back(absPath) ;
						
					}
					ACE_Time_Value currentTime = ACE_OS::gettimeofday();
					if(currentTime.sec() >wait.sec())
					{
					  DirListTimeout = true;	
        			  break;			
					}  
				}
			} while(dirent != NULL ) ;
			ACE_OS::closedir(ace_dir) ;
			retVal = true ;
		}
		else
		{
			tracker->addDirectoryStatus(dir, baseDir, contentSrcId,  folderId, ICAT_RESOBJ_FAILURE) ;
			DebugPrintf(SV_LOG_ERROR, "Failed to open dir %s\n", dir.c_str()) ;
			retVal = false ;
		}
	}
	return retVal ;
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
}

bool FileLister::isProcessing()
{
	bool retVal = false ;
	retVal = ( m_objs.size() > 0 ) || ( m_queuePtr->queue()->message_count () > 0 );
	return retVal ;
}

int FileLister::open(void *args)
{
	int retVal ;
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", __FUNCTION__) ;
    retVal = this->activate() ;
	if( retVal == -1 )
	{
		throw icatThreadException("Failed to activate the fileLister thread\n") ;
	}
	else if( retVal == 1 )
	{
		throw icatThreadException("The file lister thread already active and activate doesn't work in this case\n") ;
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", __FUNCTION__) ;
	return retVal ;
}

int FileLister::close(u_long flags)
{   
    DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",__FUNCTION__);
    DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",__FUNCTION__);
    return 0;
}

ArcQueuePtr FileLister::getQueue()
{
	return m_queuePtr ;
}
