// volstats.cpp : getsDefines the entry point for the console application.
//

#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <windows.h>
#include <atlbase.h>
#include <time.h>

#include "involflt.h"

void PrintErrorMsg(char* str, int err)
{
	char msgBuf[1024];
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		  NULL,
		  err,
		  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		  msgBuf,
		  sizeof(msgBuf),
		  NULL );
    std::cerr << str << ": " << msgBuf << std::endl;
    OutputDebugString("*************");
    OutputDebugString(str);
    OutputDebugString(": ");
    OutputDebugString(msgBuf);
    OutputDebugString("\n");
}

VOLUME_STATS_DATA* BuildVolumeStatsDataBuffer(DWORD* bufferLen)
{
	(*bufferLen) = 0;
	VOLUME_STATS_DATA* volStatsData = NULL;

	DWORD idx = 0;
	DWORD volKeyNameLen;

	char volKeyName [MAX_PATH];

	CRegKey paramKey;

	if (ERROR_SUCCESS == paramKey.Open(HKEY_LOCAL_MACHINE, "System\\CurrentControlSet\\Services\\involflt\\Parameters", KEY_READ)) {
		do {
			volKeyNameLen = sizeof(volKeyName);
			if (ERROR_SUCCESS != paramKey.EnumKey(idx, volKeyName, &volKeyNameLen)) {
				break;
			}
			++idx; 
		} while (true);
		paramKey.Close();
	}

	if (idx > 0) {
		(*bufferLen) = sizeof(VOLUME_STATS_DATA) + (idx * sizeof(VOLUME_STATS));
		volStatsData = (VOLUME_STATS_DATA*) new char[(*bufferLen)];
		if (0 == volStatsData) {
			PrintErrorMsg("memory allocation error", GetLastError());
			(*bufferLen) = 0;
		}
	}

	return volStatsData;
}

void DumpVolumeStatsDataHeaders(std::ofstream& rFile)
{
    rFile << "Date\t"
          << "Total Volumes\t"
          << "Volumes Returned\t"
          << "Dirty Blocks Allocated\t"
          << "Service State\n";
}

void DumpVolumeStatsHeaders(std::ofstream& rFile)
{
    rFile << "Date\t"
          << "Volume GUID\t"
          << "Changes Returned To Service\t"
          << "Changes Read From Bit Map\t"
          << "Changes Written To Bit Map\t"
          << "Changes Queued For Writing\t"
          << "Changes Written To Bit Map\t"
          << "Changes Read From Bit Map\t"
          << "Changes Returned To Service\t"
          << "Changes Pending Commit\t"
          << "Changes Reverted\t"
          << "Pending Changes\t"
          << "Changes Pending Commit\t"
          << "Changes Queued For Writing\t"
          << "Dirty Blocks In Queue\t"
          << "Bit Map Write Errors\t"
          << "Volume Flags\t"
          << "Changes Reverted\t"
          << "Volume's Bit Map State\t"
          << "Volume Size\n";
}

void DumpVolumeStatsData(std::ofstream& rFile, VOLUME_STATS_DATA* volStatsData)
{
    if (0 != volStatsData) {
         time_t curTime = time(NULL);

         tm* tmTime = localtime(&curTime);

         char date[32];

         strftime(date, sizeof(date), "%m/%d/%Y %H:%M:%S", tmTime);

        // applies to all volumes
        rFile << date                                << '\t'                  
			  << volStatsData->ulTotalVolumes        << '\t'
			  << volStatsData->ulVolumesReturned     << '\t'
			  << volStatsData->lDirtyBlocksAllocated << '\t'
			  << volStatsData->eServiceState         << '\n';
    }
}

void DumpVolumeStats(std::ofstream& rFile, VOLUME_STATS_DATA* volStatsData)
{
    VOLUME_STATS* volStats = volStatsData->VolumeArray;
    
    char guid[GUID_SIZE_IN_CHARS + 1];

    memset(guid, 0, sizeof(guid));

    time_t curTime = time(NULL);

    tm* tmTime = localtime(&curTime);

    char date[32];
    char defaultChar = ' ';

    BOOL defaultUsed = false;

    strftime(date, sizeof(date), "%m/%d/%Y %H:%M:%S", tmTime);
    if (0 != volStatsData) {
        // individual volumes        
        for (unsigned long i = 0; i < volStatsData->ulVolumesReturned; ++i) {
            if (0 == WideCharToMultiByte(CP_OEMCP, 0, volStats->VolumeGUID, GUID_SIZE_IN_CHARS, guid, sizeof(guid), NULL, NULL)) {
                    std::cout << "error converting guid: " << GetLastError() << std::endl;
            }
            rFile  << date                                             << '\t'
                   << guid                                             << '\t'
                   << volStats->uliChangesReturnedToService.QuadPart   << '\t'
                   << volStats->uliChangesReadFromBitMap.QuadPart      << '\t'
                   << volStats->uliChangesWrittenToBitMap.QuadPart     << '\t'
                   << volStats->ulicbChangesQueuedForWriting.QuadPart  << '\t'
                   << volStats->ulicbChangesWrittenToBitMap.QuadPart   << '\t'
                   << volStats->ulicbChangesReadFromBitMap.QuadPart    << '\t'
                   << volStats->ulicbChangesReturnedToService.QuadPart << '\t'
                   << volStats->ulicbChangesPendingCommit.QuadPart     << '\t'
                   << volStats->ulicbChangesReverted.QuadPart          << '\t'
                   << volStats->ulPendingChanges                       << '\t'
                   << volStats->ulChangesPendingCommit                 << '\t'
                   << volStats->ulChangesQueuedForWriting              << '\t'
                   << volStats->lDirtyBlocksInQueue                    << '\t'
                   << volStats->lNumBitMapWriteErrors                  << '\t'
                   << volStats->ulVolumeFlags                          << '\t'
                   << volStats->ulChangesReverted                      << '\t'
                   << volStats->eVBitmapState                          << '\t'
                   << volStats->ulVolumeSize.QuadPart                  << '\t'
                   << std::endl;
            ++volStats;
        }
        rFile << std::endl;    
	}
}

void GetVolStats(std::string& outFile, int refreshSeconds)
{		
	DWORD bytesReturned = 0;
	DWORD bufferLen;

	HANDLE hFilterDevice;

	VOLUME_STATS_DATA* volumeStatsData;

    std::ofstream oFile(outFile.c_str());

    if (!oFile) {
        std::cerr << "Couldn't open " << outFile << ": " << GetLastError() << std::endl;
    } else {
        hFilterDevice  = CreateFile(INMAGE_FILTER_DOS_DEVICE_NAME, 
            GENERIC_READ, 
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 
            NULL, 
            OPEN_EXISTING, 
            0, 
            NULL); 

        if (INVALID_HANDLE_VALUE == hFilterDevice) {		
            PrintErrorMsg("open control device error", GetLastError());
        } else {
            volumeStatsData = BuildVolumeStatsDataBuffer(&bufferLen);
            if (NULL != volumeStatsData) {
                int refreshMilliSeconds = refreshSeconds * 1000;            
                if (!DeviceIoControl(hFilterDevice, IOCTL_INMAGE_GET_VOLUME_STATS, NULL, 0, volumeStatsData, bufferLen, &bytesReturned, NULL)) {
                    PrintErrorMsg("DeviceIoControl error", GetLastError());            
                } else {
                    DumpVolumeStatsDataHeaders(oFile);
                    DumpVolumeStatsData(oFile, volumeStatsData);
                    DumpVolumeStatsHeaders(oFile);                                                                                   
                    do {
                        if (0 != refreshSeconds) {
                            // system("cls");
                        }
                        if (!DeviceIoControl(hFilterDevice, IOCTL_INMAGE_GET_VOLUME_STATS, NULL, 0, volumeStatsData, bufferLen, &bytesReturned, NULL)) {
                            PrintErrorMsg("DeviceIoControl error", GetLastError());
                            break;
                        } else {                    
                            DumpVolumeStats(oFile, volumeStatsData);                            
                        }
                        if (0 != refreshSeconds) {
                            //					std::cout << "\n\nRefresh Rate: " << refreshSeconds << " seconds\n\nctrl-c to exit" << std::endl;      
                            Sleep(refreshMilliSeconds);
                            memset(volumeStatsData, 0, bufferLen);
                        }
                    } while (0 != refreshSeconds);
                }
            }
        }

        delete [] volumeStatsData;
        CloseHandle(hFilterDevice);
    }
}
void usage() 
{
	std::cout << "volstats collects volume statistics from the driver and writes\n"
              << "them to a log file using a tab deliminated format that can then\n"
              << "be imported into a spreadsheet application for review.\n\n"
              << "\nusage: volstats [-h] [-f <outfile>] [-t <read stats rate>]\n"
		      << "  -h : diplays usage\n"
			  << "  -f <outfile>: file to write stats into. Default is volstats.log\n"
              << "  -t <read stats rate>: number of seconds to wait between stats reads. default is 5 seconds\n"
              << "\npress Ctrl-c to stop volstats\n"			  
			  << std::endl;
    exit(0);
}

int _tmain(int argc, _TCHAR* argv[])
{
    std::string outFile;    

	int refreshSeconds = 5;

    if (argc > 1) {
        int i = 0;
        // TODO:
        // prevent same option from appearing twice
        while (i < argc) {
            if ('-' == *(argv[i])) {
                switch (argv[i][1]) {
                    case 'h':                    
                        usage();
                        break;
                    case 'f':                    
                        if (0 != argv[i][2]) {
                            outFile = &(argv[i][2]);
                        } else {
                            ++i;
                            outFile = argv[i];
                        }
                        break;
                    case 't':
                        if (0 != argv[i][2]) {
                            refreshSeconds = atoi(&(argv[i][2]));
                        } else {
                            ++i;
                            refreshSeconds = atoi(argv[i]);
                        }
                        break;
                    default:
                        std::cerr << "unknown arg: " << argv[i] << std::endl;
                        usage();
                        break;
                }
            }
            ++i;
        }
    }

    if (outFile.empty()) {
        outFile = argv[0];
        std::string::size_type idx = outFile.find_last_of(".");
        if (std::string::npos != idx) {
            outFile.replace(idx + 1, 3, "log");
        } else {
            outFile += ".log";
        }
    }

    std::cout << "Running volstats with the following\n"
              << " outfile        : " << outFile        << '\n'
              << " read stats rate: " << refreshSeconds << " seconds\n"  
              << "\npress Ctrl-c when you are ready to stop volstats ..."
              << std::endl;

	GetVolStats(outFile, refreshSeconds);

	return 0;

}

