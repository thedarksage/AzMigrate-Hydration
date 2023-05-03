#include "Windows.h"
#include "tchar.h"

#define DEFAULT_DATA_POOL_SIZE_64BIT                          (256)
#define DEFAULT_DATA_POOL_SIZE_32BIT                          (64)
#define LOWERLIMIT_DATAPOOL_PERC                              (6)
#define UPPERLIMIT_DATAPOOL_PERC                              (4096)
#define DATA_POOL_SIZE_ALIGN                                  (4)

// Name :- GetDataPoolSize
// Input :- Void
// OutPut :- DataPoolSize in MB
// Describtion :- Get the appropiate value for DataPagaPool considering the system memory and defaults. 
//                Set the registry value before driver get loaded while installing the product.


// Check if running under Wow64
BOOL RunningOn64bitVersion(BOOL &bIsWow64) {
    typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
    LPFN_ISWOW64PROCESS fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(GetModuleHandle(L"kernel32"),"IsWow64Process");
    if (NULL != fnIsWow64Process){
        if(fnIsWow64Process(GetCurrentProcess(),&bIsWow64)){
            return TRUE;
        } else {
            //_tprintf(_T("RunningOn64bitVersion failed as IsWow64Process failed with error : %d\n"), GetLastError());
            return TRUE;
        }
	} else {
        //_tprintf(_T("RunningOn64bitVersion failed as GetProcAddress failed\n"));
        return FALSE;
    }
}


unsigned long _tmain(int argc, _TCHAR* argv[])
{    
   unsigned long ulDataPoolSizeMB = 0;
   unsigned long ulDefaultDataPoolSizeMB = DEFAULT_DATA_POOL_SIZE_32BIT;
	
   BOOL bIsWow64 = FALSE;
   // Check if it is running under WOW64. Set the default accordingly. In error case consider default value of 32bit
   if (RunningOn64bitVersion(bIsWow64)) {
        if (bIsWow64) {
            ulDefaultDataPoolSizeMB = DEFAULT_DATA_POOL_SIZE_64BIT;
            MEMORYSTATUSEX lpBuffer;
            lpBuffer.dwLength = sizeof (lpBuffer);
            if (GlobalMemoryStatusEx(&lpBuffer)) {
                // Set the DataPoolSize to 1/16 i.e 6% of total system memory
                ulDataPoolSizeMB = (unsigned long)((LOWERLIMIT_DATAPOOL_PERC * ((lpBuffer.ullTotalPhys + (1024 * 1024) - 1) / (1024 * 1024))) / 100);
                // Check Allignment and set accordingly
                if (ulDataPoolSizeMB % DATA_POOL_SIZE_ALIGN) {                                 
                    ulDataPoolSizeMB = ((ulDataPoolSizeMB + DATA_POOL_SIZE_ALIGN - 1) / DATA_POOL_SIZE_ALIGN) * DATA_POOL_SIZE_ALIGN;
                }
            }
            /* else {
                _tprintf(_T("GlobalMemoryStatusEx failed\n"));
            } */
        }
    }

    //_tprintf(_T("ulDataPoolSizeMB %u ulDefaultDataPoolSizeMB %u\n"), ulDataPoolSizeMB, ulDefaultDataPoolSizeMB);

    // ulDataPoolSizeMB should not be less than Default.
    if (ulDataPoolSizeMB <= ulDefaultDataPoolSizeMB) {
        ulDataPoolSizeMB = 0;
    }

    if (ulDataPoolSizeMB > UPPERLIMIT_DATAPOOL_PERC){
        ulDataPoolSizeMB = UPPERLIMIT_DATAPOOL_PERC;
    }

    //_tprintf(_T("ulDataPoolSizeMB %u\n"), ulDataPoolSizeMB);

    return ulDataPoolSizeMB;
}
