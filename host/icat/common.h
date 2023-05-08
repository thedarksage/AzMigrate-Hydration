#ifndef COMMON_H
#define COMMON_H
#include <string>
#include "svtypes.h"

void trimSpaces( std::string& str);
bool isValidDate(std::string date);
std::string getLogFilePath() ;
void sliceAndReplace(std::string& path,std::string, std::string);
SV_ULONGLONG convertStringtoULL( const std::string& strVal ) ;
void getTimeToString(const std::string fmt, std::string& timeStr, time_t *time_struct=NULL) ;
char* getPathSeparator() ;
void formatingSrcVolume(std::string& volume);
#endif

