#ifndef __MAPDRIVELETTER_H
#define __MAPDRIVELETTER_H

#include "AtlConv.h"
#include<iostream>
#include<fstream>
#include<vector>
#include<string>
#include<map>
#include<algorithm>


//#define _CLUSUTIL_LOG

#define BUFSIZE MAX_PATH
#define OPTION_SOURCE_HOST			"-s"
#define OPTION_TARGET_HOST			"-t"
#define DRIVES_LETTER_FILE_NAME		"Inmage_DriveLetterInfo.info"


typedef struct _VolumeInfo
{
	std::vector<std::string> vMountPoints;
	std::string strVolGuid;
}VolumeInfo;

BOOL MapDriveLetterMain(int argc,char* argv[]);
//BOOL EnumerateVolumes (HANDLE volumeHandle, TCHAR *szBuffer, int iBufSize, BOOL bIncludeRemovableDrives);
BOOL EnumerateVolumes (HANDLE volumeHandle, char *szBuffer, int iBufSize, BOOL bIncludeRemovableDrives);
BOOL PersistDriveLetterInfo();
void ExtractMountPointPaths(LPTSTR lpszMountPointPaths,std::vector<std::string> &strPaths);
BOOL HandleTarget();
BOOL RemoveSystemVolumeFromUnMountList();
BOOL BuildMapFromFile();
BOOL BuildMapFromMemory();
void StripVolumeSpecialChars(const std::string strOld,std::string & strNew);
void AddVolumeSpecialChars(const std::string strOld,std::string & strNew);
void ProperlyEscapeSpecialChars(const std::string strOld,std::string &strNew);
void BuildStackForMounting();
void BuildStackForUnMounting();
BOOL UnMountRecursivelyFromTopDown(LPCTSTR lpszPath,std::string strVolGuid);
BOOL MountRecursivelyFromBottomUp(LPCTSTR lpszPath,std::string strVolGuid);
BOOL MountUnMountTheRootVolume(LPCTSTR lpszPath,BOOL bUnMount);

//#ifdef _CLUSUTIL_LOG
//ofstream ClusUtilLogFile(CLUSUTIL_LOGFILE,ios::out | ios::app);
void CloseLogFile();
int LogAndPrint(char* lpszText);
int LogAndPrint(char* lpszText,int nVal);
int LogAndPrint(char* lpszText, char* lpszMoreText);
int LogAndPrint(char* lpszText, const char* lpszMoreText);
int LogAndPrint(char* lpszText,int nVal,LPCTSTR lpszMoreText);
int LogAndPrint(char* lpszText,char* lpszMoreText, char* lpszEvenMoreText,int nVal);
int LogAndPrint(char* lpszText,char* lpszMoreText, char* lpszEvenMoreText);
int LogAndPrint(std::string strDesc);
int PRINTF(char* lpszText,char* lpszMoreText, int nVal);


//#endif
#endif
