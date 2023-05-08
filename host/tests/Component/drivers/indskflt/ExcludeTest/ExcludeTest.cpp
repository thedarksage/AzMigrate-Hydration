// ExcludeTest.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "Microsoft.Test.Inmage.Win32ExcludeDiskBlocks.h"
#include "ChecksumProvider.h"
#include "CDiskDevice.h"
#include "S2Agent.h"
#include "DataProcessor.h"

int _tmain(int argc, _TCHAR* argv[])
{
    DWORD       tick1,  tick2;
    int         disknum;
    int         status = 1;
    int         iNumIterations = 1;

    if (argc < 2)
    { 
        wprintf(L"Usage: %s <disk-num> [<num-iterations>]\n", argv[0]);
        return status;
    }

    disknum = _wtol(argv[1]); 

    if (argc > 2) {
        iNumIterations = _wtoi(argv[2]);
        if (iNumIterations < 0) {
            iNumIterations = MAXINT32;
        }
    }

    try
    {
        boost::shared_ptr<CS2Agent>     s2Agent(new CS2Agent(DataReplication, "E:\\tmp"));
        IPlatformAPIs   *pPlatformApis = new CWin32APIs();
        CDiskDevice     disk1(disknum, pPlatformApis);

        tick1 = GetTickCount();
        printf("Enabling replication for disk %d\n", _wtol(argv[1]));
        s2Agent->AddReplicationSettings(disk1.GetDeviceNumber(), "E:\\tmp\\disk0.dsk", "E:\\tmp\\disk0.log");
        s2Agent->StartReplication(0, disk1.GetDeviceSize());
        s2Agent->StartSVAgents(SHUTDOWN_NOTIFY_FLAGS_ENABLE_DATA_FILTERING);
        
        // run you workload for next 2 mins
        //Sleep(120 * 1000);

        int iIteration = 0;

        for (int i = 0; i < iNumIterations; i++) {
            printf("Iteration %d:  Starting validation....\n", iIteration++);
            if (s2Agent->Validate())
            {
                SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_GREEN);
                printf("Iteration %d: Tests Passed\n",iIteration);
                status = 0;
            }
            else
            {
                SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_RED);
                printf("Iteration %d: Tests Failed\n", iIteration);
            }
            SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 0x0f);
        }
    }
    catch (exception& ex)
    {
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), FOREGROUND_RED);
        printf("%s", ex.what());
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 0x0f);
    }

    return status;
}

