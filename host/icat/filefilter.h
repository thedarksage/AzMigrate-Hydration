#ifndef FILE_FILTER_H
#define FILE_FILTER_H
#include <vector>
#include <list>
#include <ace/OS_Dirent.h>
#include "filesystemobj.h"
#include "defs.h"
#include "common.h"

typedef struct NamePatterns_t
{
	std::string m_NamePattern;
	std::string m_ExtensionPattern;
}NamePatternsStruct;

struct FilterInfo
{
	std::string m_ExcludeDirs;					//Contains List of dirs need to exclude
	std::string m_NamePattern;				//List of filepatterns like 1* *.* *.exe
	std::string m_szFilePatternOps;
    time_t m_SearchFromTime;			//Fileter Files whose modification time > searchFromTime
    std::string m_szMtimeOps;
	std::string m_FilterOp1;					//This is either "and" OR "or" 
	std::string m_FilterOp2;					//This is either "and" OR "or"  
	long long m_FileSize;
    std::string m_SizeOps;
	bool m_includeFileFilter;
	std::list<std::string> m_szfilterTemplate;
	std::vector<NamePatternsStruct> namePatternVec;
	std::string m_Dirs;
};

class FileFilter
{
public:
	FilterInfo m_filterInfo ;
	bool filter(const std::string& fileName, const ACE_stat& ) ; // returns true if the file needs to be filtered otherwise returns false 
	bool filter( const std::string& directoryEntry, bool isDir, const ACE_stat& stat ) ;
	void parsePattern();
	bool patternMatch(const std::string& fileName,const std::string &pattern);
	bool patternMatch(std::string dirName);
	bool patternMatchforDir(std::string);
	bool filterByTime(ACE_stat);
	bool filterBySize(ACE_stat);
	FileFilter(FilterInfo filter) ;
	FileFilter() ;
} ;
#endif

