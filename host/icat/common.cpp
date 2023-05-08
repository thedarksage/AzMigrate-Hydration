#include "common.h"
#include <time.h>
#include <locale.h>
#include <stdlib.h>
void trimSpaces( std::string& s)
{
	std::string stuff_to_trim = " \n\b\t\a\r\xc" ; 
	
	s.erase( s.find_last_not_of(stuff_to_trim) + 1) ; //erase all trailing spaces
	s.erase(0 , s.find_first_not_of(stuff_to_trim) ) ; //erase all leading spaces
}

bool isValidDate(std::string time)
{
    //yyyy-mm-dd
    //yyyy - 4 digit
    //mm - 2 digit (1 - 12)
    //dd - 2 digit (1 - 31, for 2nd month: can not greater than 28, 29 acceptable only on leap years, 
    //1,3,5,7,8,10,12 -> 31, 4,6,9,11 - 30)
    
    if(time.empty())
    {
        return true;
    }
    std::string::size_type index1 = std::string::npos ;
    std::string::size_type index2 = std::string::npos ;
    std::string tempArray[3] ;
    
    int count = 0 ;
    while( index2 != time.length() )
    {
        index1 = index2 + 1 ;
        index2 = time.find('-', index1 ) ;
        if( index2 == std::string::npos )
            index2 = time.length() ;
        tempArray[count] = time.substr(index1, index2 - index1 );
        trimSpaces( tempArray[count++] ) ;
    }
    
    int year = atoi(tempArray[0].c_str());
    int month = atoi(tempArray[1].c_str());
    int date = atoi(tempArray[2].c_str());
        
    // year validateion
    if(year < 1000 || year > 9999)
    {
        return false;
    }
    
    // month validateion
    if(month < 1 || month >12)
    {
        return false;
    }
    
    // date validateion
    if(month == 1 || month == 3 || month == 5 || month == 7 ||month == 8 || month == 10 || month == 12)
    {
        if(date > 31 || date < 1)
        {
            return false;
        }
    }
    else if(month == 4 || month == 6 || month == 9 || month == 11 )
    {
        if(date > 30 || date < 1)
        {
            return false;
        }
    }
    else
    {
        if(year%400 ==0 || (year%100 != 0 && year%4 == 0))  
        {
            if(date > 29 || date < 1)
            {
            return false;
            }
        }
        else
        {
            if(date > 28 || date < 1)
            {
                return false;
            }
        }
    }
    return true;
}

void sliceAndReplace(std::string& path,std::string findwhat,std::string replacewith)
{
       size_t pos = path.find_first_of(findwhat);
       while(pos != std::string::npos)
        {
            path.replace(pos,1,replacewith);
            pos = path.find_first_of(findwhat,pos+1);
        }
}

void getTimeToString(const std::string fmt, std::string& timeStr, time_t *time_struct)
{
	bool bRet = false ;
	char sztime[80] = {'\0' } ;
	time_t * nowbin = NULL ;
	time_t now;

	if( time_struct == NULL )
	{
		now = time(NULL);
		nowbin = &now ;
	}
	else
		nowbin = time_struct ;
    const struct tm *nowstruct = NULL ;

    (void)setlocale(LC_ALL, "");
    nowstruct = localtime(nowbin);
	strftime(sztime, 80, fmt.c_str(), nowstruct) ;
	timeStr = sztime ;
}

void formatingSrcVolume(std::string& volume)
{
	size_t found = volume.find_first_of(":/\\");
	if(found != std::string::npos)
	{
		found = volume.find_first_of(':');
		if(found != std::string::npos)		
		{	
			volume.erase(found,1);
		}
		found=volume.find_last_of("/\\");
		size_t size = volume.size();
		if(found == size-1)
		{
			volume.erase(found,1);
		}
		found = volume.find_first_of("/\\");
		if(found != std::string::npos)		
		{
			if(found == 0)
			{
				volume.erase(found,1);
			}
			else
			{
				volume.replace(found,1,"/");
			}
		}
	}
}

