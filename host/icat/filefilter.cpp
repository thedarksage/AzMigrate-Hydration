#include "filefilter.h"
#include <time.h>
#define FILTERCOUNT 3
#include "configvalueobj.h"
#include "archivecontroller.h"
#include "inmsafecapis.h"
#include "inmsafeint.h"
#include "inmageex.h"

/*
* Function Name: Constuctor (Special member function)
* Arguements:  Parameterised(FilterInfo stucture var)
* Return Value: No return type.
* Description: This will assign the given input to member var m_filterInfo and calls parsePattern().
* Called by: 
* Calls: parsePattern() to parse the given pattern into name and extenstion patterns.
* Issues Fixed:
*/

FileFilter::FileFilter(FilterInfo filterInfo)
{
	m_filterInfo = filterInfo ;
	parsePattern();
	
}

FileFilter::FileFilter()
{

}

/*
* Function Name: parsePattern
* Arguements:  No args..
* Return Value: Nothing ..
* Description: This will parses the given pattern into name and extesion patterns and pushes into namePatternVec.
* Called by: FileFilter 's parameterised constuctor.
* Calls: 
* Issues Fixed:
*/

void FileFilter::parsePattern()
{
	char szBuffer[250] ;
	bool retVal = false;
	size_t startPos = 0; //Strat Position
	std::string pattern;
	NamePatternsStruct tempNamePatternStruct;
	
	inm_strcpy_s(szBuffer,ARRAYSIZE(szBuffer),m_filterInfo.m_NamePattern.c_str()) ;
    
	for( char* pszToken = strtok(szBuffer, "," ); 
		(NULL != pszToken);
		(pszToken = strtok( NULL, "," ))) 
	{
		pattern = pszToken;
        trimSpaces( pattern ) ;
		startPos=pattern.find_last_of('.');                          //We are supporting * only 
		if(startPos != std::string::npos)              
		{			
			tempNamePatternStruct.m_ExtensionPattern = pattern.substr(startPos+1);            
		}
		
		tempNamePatternStruct.m_NamePattern= pattern.substr(0,startPos);        
		m_filterInfo.namePatternVec.push_back(tempNamePatternStruct);			
	}	
}

/*
* Function Name: patternMatchforDir
* Arguements: directory name .
* Return Value: bool value.
* Description:This will test whether directory name maches the given exclude pattern or not.
* Called by: filter() function of this component.
* Calls: patternMatch() which performs actual varivication of maching.
* Issues Fixed:
*/

bool FileFilter::patternMatchforDir(std::string dirName)
{
	char *szBuffer;
	archiveControllerPtr arc_controller = archiveController::getInstance() ;
	bool ReturnValue = true ;
	if( dirName.compare("lost+found") == 0 ||
		dirName.compare("System Volume Information") == 0 ||
		dirName.compare("RECYCLER") == 0 )
	{
		DebugPrintf(SV_LOG_DEBUG, "Pattern Match Result for dirname : %s is false\n", dirName.c_str()) ;
		ReturnValue = false ;
		arc_controller->m_tranferStat.total_Num_Of_Excluded_Dirs++;
        return ReturnValue ;
    }
    size_t szbuflen;
    INM_SAFE_ARITHMETIC(szbuflen = InmSafeInt<size_t>::Type(m_filterInfo.m_ExcludeDirs.size())+1, INMAGE_EX(m_filterInfo.m_ExcludeDirs.size()))
	szBuffer = new char[szbuflen];
    inm_strcpy_s(szBuffer,(m_filterInfo.m_ExcludeDirs.size()+1),m_filterInfo.m_ExcludeDirs.c_str()) ;
    for( char* pszToken = strtok(szBuffer, "," ); 
		(NULL != pszToken);
		(pszToken = strtok( NULL, "," ))) 
	{
        std::string excludedDir = pszToken;
        trimSpaces(excludedDir);
		if( patternMatch( dirName, excludedDir ))
		{
			ReturnValue = false ;
			DebugPrintf(SV_LOG_DEBUG, "Pattern Match Result for dirname : %s is false\n", dirName.c_str()) ;
			arc_controller->m_tranferStat.total_Num_Of_Excluded_Dirs++;
            break;
		}
	}
	delete []szBuffer;
	return ReturnValue;
}

/*
* Function Name: patternMatchforDir
* Arguements: directory name .
* Return Value: bool value.
* Description:This will test whether directory name maches the given exclude pattern or not.
* Called by: filter() function of this component.
* Calls: patternMatch() which performs actual varivication of maching.
* Issues Fixed:
*/

bool FileFilter::patternMatch(std::string dirName)
{
	char *szBuffer;
	bool ReturnValue = true ;
	if( dirName.compare("lost+found") == 0 ||
		dirName.compare("System Volume Information") == 0 ||
		dirName.compare("RECYCLER") == 0 )
	{
		DebugPrintf(SV_LOG_DEBUG, "Pattern Match Result for dirname : %s is false\n", dirName.c_str()) ;
		ReturnValue = false ;
        return ReturnValue ;
    }
    size_t szbuflen;
    INM_SAFE_ARITHMETIC(szbuflen = InmSafeInt<size_t>::Type(m_filterInfo.m_Dirs.size())+1, INMAGE_EX(m_filterInfo.m_Dirs.size()))
	szBuffer = new char[szbuflen];
    inm_strcpy_s(szBuffer,(m_filterInfo.m_Dirs.size()+1),m_filterInfo.m_Dirs.c_str()) ;
    for( char* pszToken = strtok(szBuffer, "," ); 
		(NULL != pszToken);
		(pszToken = strtok( NULL, "," ))) 
	{
        std::string excludedDir = pszToken;
        trimSpaces(excludedDir);
		if( patternMatch( dirName, excludedDir ))
		{
			ReturnValue = false ;
			DebugPrintf(SV_LOG_DEBUG, "Pattern Match Result for dirname : %s is false\n", dirName.c_str()) ;
	        break;
		}
	}
	delete []szBuffer;
	return ReturnValue;
}



/*
* Function Name: patternMatch
* Arguements: file/dir name and pattern string(either name pattern or extension pattern string),
* Return Value: bool value.
* Description:This will perform actual varivication of maching.
* Called by: either patternMathForDir() or fileter()
* Calls: 
* Issues Fixed:
*/

bool FileFilter::patternMatch(const std::string& filenamePortion,const std::string &pattern)
{
	bool retVal = false;
	size_t startPos; //Strat Position
	size_t endPos;   //End Position
	std::string str;
	std::string cmpstr;
   
	if( pattern.empty() == true ) 
		return true ;

	startPos=pattern.find_first_of('*');                          //We are supporting * only 
	if(startPos != std::string::npos)              
	{
		if(startPos == 0)                                        //String is at start postion
		{
			endPos=pattern.find_first_of('*',startPos+1);       //Find for another wild card i.e *
			if(endPos != std::string::npos)                    //Theres more than one *
			{
				str = pattern.substr (startPos+1,endPos-1);   
				if(filenamePortion.find(str)!= std::string::npos) 
				{
					retVal = true;
				}
			}
			else						                      //only one * 
			{
				str = pattern.substr (startPos+1);

				if(str.empty() || filenamePortion.find(str.c_str(),(filenamePortion.size()- str.size()),str.size() )!= std::string::npos)
				{
					retVal = true;	
				}
            }
		}
		else 
		{
			str = pattern.substr(0,startPos);
			std::string temp = filenamePortion.substr(0,str.size());
			if(temp.compare(str) ==0)
			{
				retVal = true;
			}
		}
	}
	else //Exact match
	{
		if(filenamePortion.compare(pattern)==0)
		{
			retVal = true;	
		}
	}
	return retVal; 	
}

/*
* Function Name: filterBySize
* Arguements: state of the file ,
* Return Value: bool value.
* Description: This will test whether given state is filtered or not based on the comparision ops with respect to the size..
* Called by: fileter()
* Calls: 
* Issues Fixed:
*/
bool FileFilter::filterBySize(ACE_stat stat)
{
	bool retVal = false;
    std::string ops = m_filterInfo.m_SizeOps;

    if(ops.compare(">=") == 0 && stat.st_size >= m_filterInfo.m_FileSize)
    {
        retVal = true;
    }
    else if(ops.compare("<=") == 0 && stat.st_size <= m_filterInfo.m_FileSize)
    {
        retVal = true;
    }
    else if(ops.compare(">") == 0 && stat.st_size > m_filterInfo.m_FileSize)
    {
        retVal = true;
    }
    else if(ops.compare("<") == 0 && stat.st_size < m_filterInfo.m_FileSize)
    {
        retVal = true;
    }
    else if(ops.compare("=") == 0 && stat.st_size == m_filterInfo.m_FileSize)
    {
        retVal = true;
    }
    
 	return retVal;
}

/*
* Function Name: filterByTime
* Arguements: state of the file ,
* Return Value: bool value.
* Description: This will test whether given state is filtered or not based on the comparision ops  with respect to the modified time..
* Called by: fileter()
* Calls: 
* Issues Fixed:
*/

bool FileFilter::filterByTime(ACE_stat stat)
{
	bool retVal = false;
    DebugPrintf(SV_LOG_DEBUG, "filter time value :"ULLSPEC "file time value :"ULLSPEC"\n", m_filterInfo.m_SearchFromTime, stat.st_mtime) ;
    
    std::string ops = m_filterInfo.m_szMtimeOps;

    if(ops.compare(">=") == 0 && stat.st_mtime >= m_filterInfo.m_SearchFromTime)
    {
        retVal = true;
    }
    else if(ops.compare("<=") == 0 && stat.st_mtime <= m_filterInfo.m_SearchFromTime)
    {
        retVal = true;
    }
    else if(ops.compare(">") == 0 && stat.st_mtime > m_filterInfo.m_SearchFromTime)
    {
        retVal = true;
    }
    else if(ops.compare("<") == 0 && stat.st_mtime < m_filterInfo.m_SearchFromTime)
    {
        retVal = true;
    }    
    else if(ops.compare("=") == 0 && 
		(( stat.st_mtime < m_filterInfo.m_SearchFromTime + 60 * 24 * 60 ) && 
		( stat.st_mtime > m_filterInfo.m_SearchFromTime )) )
    {
        retVal = true;
    }

	return retVal;
	
}

/*
* Function Name: filter
* Arguements: 
* Return Value: bool value.
* Description: This will call appropriate function based on given input..
* Called by: 
* Calls: 
* Issues Fixed:
*/

bool FileFilter::filter( const std::string& directoryEntry, bool isDir, const ACE_stat& stat ) 
{
	archiveControllerPtr arc_controller = archiveController::getInstance() ;
	if( isDir )
	{
        arc_controller->m_tranferStat.total_Num_Of_Dirs++;
		return patternMatchforDir(directoryEntry) ;
	}
	else
	{
        arc_controller->m_tranferStat.total_Num_Of_Files++;
		return filter( directoryEntry, stat ) ;
	}
	
}

/*
* Function Name: filter
* Arguements: filename and its state.
* Return Value: bool value.
* Description: This will loop into filterTemplate string list and calls appropriate filetering functions.
* Called by: filelister component 
* Calls: This will call appropriate function based on filterTemplate structure..
* Issues Fixed:
*/

bool FileFilter::filter(const std::string& fileName,const ACE_stat& stat)
{
	bool filtersArray[FILTERCOUNT] = {true, true, true};
    std::string filtesrOps[2] = {"and", "and"};
    bool ReturnValue = false;
	bool matchVale = false;
	std::string pattern;
	std::string namePattern;
	std::string extensionPattern;
	std::list<std::string>::iterator it;
	std::string temp;
	int count = 0;
	int index = 0;
    size_t startPos = fileName.find_last_of('.');  
	if(startPos != std::string::npos)
	{
		namePattern = fileName.substr(0,startPos);
		extensionPattern = fileName.substr(startPos+1);
	}
    else
    {
        namePattern = fileName;
    }
   
    int filterIndex = 0 ;
	for ( it=m_filterInfo.m_szfilterTemplate.begin() ; it != m_filterInfo.m_szfilterTemplate.end(); count++,it++ )
	{
		temp = *it;
        filterIndex = count / 2 ;
		if(count%2 != 0)
		{
            filtesrOps[filterIndex]  = temp;			
		}
        else
        {
		    if(temp.compare("pattern") == 0)
		    {
			    for(unsigned int i =0;i<m_filterInfo.namePatternVec.size();i++)
			    {
				    if( patternMatch(namePattern, m_filterInfo.namePatternVec[i].m_NamePattern))
				    {
					    if( patternMatch(extensionPattern, m_filterInfo.namePatternVec[i].m_ExtensionPattern))
					    {
						    filtersArray[filterIndex] = true;
						    break;
					    }
					    else 
					    {
						    filtersArray[filterIndex] = false;
							DebugPrintf(SV_LOG_DEBUG, "%s. Match by extension and file name pattern is false\n", fileName.c_str()) ;
					    }
				    }
                    else
                    {
                        filtersArray[filterIndex] = false;
						DebugPrintf(SV_LOG_DEBUG, "%s. Match by extension and file name pattern is false\n", fileName.c_str()) ;
                    }
			     }
                std::string ops =m_filterInfo.m_szFilePatternOps;
                    
                if( ops.compare("!=") == 0 )
                {
                    filtersArray[filterIndex] = !filtersArray[filterIndex] ;        
                }
            }
		    else if (temp.compare("date") == 0)
		    {
				if( ConfigValueObject::getInstance()->getFromLastRun() )
				{
					filtersArray[filterIndex] = true ;
				}
				else
				{
					filtersArray[filterIndex] = filterByTime(stat);
					if( !filtersArray[filterIndex] )
					{
						std::string fileModTime ;						
						DebugPrintf(SV_LOG_DEBUG, "%s. File Modification time "ULLSPEC". Match by date and time is false\n", fileName.c_str(), stat.st_mtime) ;
					}
				}
		    }
		    else if (temp.compare("size") == 0)
		    {
			    filtersArray[filterIndex] = filterBySize(stat);
				if( !filtersArray[filterIndex] )
				{
					DebugPrintf(SV_LOG_DEBUG, "%s. File Size " ULLSPEC "Match by size is false\n", fileName.c_str(), stat.st_size) ;
				}
		    }
        }
	}

	if(filtesrOps[0].compare("and")==0) 
	{
		if(filtesrOps[1].compare("and")==0)
		{
			if(filtersArray[0] && filtersArray[1] && filtersArray[2])
			{
				ReturnValue = true ;
			}
		}
		else if(filtesrOps[1].compare("or")==0)
		{
			if(filtersArray[0] && filtersArray[1] || filtersArray[2])
			{
				ReturnValue = true ;
			}
		}
	}
	else if(filtesrOps[0].compare("or")==0) 
	{
		if(filtesrOps[1].compare("and")==0)
		{
			if(filtersArray[0] || filtersArray[1] && filtersArray[2])
			{
				ReturnValue = true ;
			}
		}
		else if(filtesrOps[1].compare("or")==0)
		{
			if(filtersArray[0] || filtersArray[1] || filtersArray[2])
			{
				ReturnValue = true;
			}
		}
	}

	return ReturnValue;
}
