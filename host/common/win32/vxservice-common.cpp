#include "branding_parameters.h"
#include "vxservice.h"

#ifdef SV_WINDOWS
/*
* FUNCTION NAME : VxService::ConfigureDataPool
*
* DESCRIPTION
*       Configures driver with data pool in MB.
*       It does following,
*           1) Figure RAM Size
*           2) Get min and max DPP Usage Values from config.
*           3) Get Percentage of RAM to be used by driver.
*           4) Get Data Paged Pool Aligment,
*           5) Figure free physical memory
*           6) Get min and max values from config
*           7) Get Percentage of free memory used by driver.
*           8) Get Data non paged pool alignment
*
*       It calcualtes Data Paged Pool usage as follows,
*       1) Calculates current DPP size by multiplying DPP usage percentage
*           with RAM size.
*       2) If current DPP usage is greater than Max DPP Usage,
*           reset current DPP usage to max DPP USage.
*       3) Align DPP usage to alignment boundary.
*       4) If calculated DPP usage is less than minimum DPP usage, reset DPP
*               usage to minimum DPP usage.
*       Post calculation it calls FilterDriverNotifier to configure filter
*           driver with calculated Data Paged Pool.
*
* return value : true:  If data pool is configured correctly
*                false: Otherwise
*
*/
bool VxService::ConfigureDataPool(const LocalConfigurator& lc)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);
    bool bStatus = false;

    SV_ULONG ulDppSizeInPercentage = lc.getDriverDppRamUsageInPercent();
    SV_ULONG ulMaxDppSizeInMB = lc.getDriverMaxDppUsageInMB();
    SV_ULONG ulMinDppSizeInMB = lc.getDriverMinDppUsageInMB();
    SV_ULONG ulDppAlignmentInMB = lc.getDriverDppAlignmentInMB();

    SV_ULONG ulDnpSizeInPercentage = lc.getDriverDNpFreeRamUsageInPercent();
    SV_ULONG ulMaxDnpSizeInMB = lc.getDriverMaxDNpUsageInMB();
    SV_ULONG ulMinDnpSizeInMB = lc.getDriverMinDNpUsageInMB();
    SV_ULONG ulDnpAlignmentInMB = lc.getDriverDNpAlignmentInMB();

    DebugPrintf(SV_LOG_ALWAYS, "%s: ulDppSizeInPercentage = %lu ulMaxDppSizeInMB = %lu ulMinDppSizeInMB = %lu ulDppAlignmentInMB = %lu\n",
        FUNCTION_NAME,
        ulDppSizeInPercentage,
        ulMaxDppSizeInMB,
        ulMinDppSizeInMB,
        ulDppAlignmentInMB);

    MEMORYSTATUSEX  memoryStatusEx = { 0 };
    memoryStatusEx.dwLength = sizeof(memoryStatusEx);

    if (!GlobalMemoryStatusEx(&memoryStatusEx)) {
        DebugPrintf(SV_LOG_ERROR, "%s: Failed to get Ram Size Error = %lu\n", FUNCTION_NAME, GetLastError());
        return false;
    }

    // By default following MEMORYSTATUSEX fields are ULONLONG
    // Once we convert it to MB we reduce total bits to 64-20 = 44 bits
    // With percentage in place we will further reduce bits to less
    // Even for 1PB memory = 1M * 1G. It needs 30 bits
    // So SV_ULONG should be good enough to handle 1PB
    SV_ULONGLONG    ullTotalPhysicalRamInMB = memoryStatusEx.ullTotalPhys / (1024 * 1024);
    SV_ULONGLONG    ullTotalFreePhysicalMemInMB = memoryStatusEx.ullAvailPhys / (1024 * 1024);
    DebugPrintf(SV_LOG_ALWAYS, "%s: Total Physical RAM = %llu MB  Total Free RAM = %llu MB\n", FUNCTION_NAME, ullTotalPhysicalRamInMB, ullTotalFreePhysicalMemInMB);

    SV_ULONG    ulDppSizeInMB = (SV_ULONG) ((ullTotalPhysicalRamInMB * ulDppSizeInPercentage) / 100);
    SV_ULONG    ulDnpSizeInMB = (SV_ULONG) ((ullTotalFreePhysicalMemInMB * ulDnpSizeInPercentage) / 100);

    if (ulDnpSizeInMB > ulMaxDnpSizeInMB) {
        ulDnpSizeInMB = ulMaxDnpSizeInMB;
    }

    // Align Before configuring driver
    ulDppSizeInMB = ((ulDppSizeInMB / ulDppAlignmentInMB) * ulDppAlignmentInMB);
    ulDnpSizeInMB = ((ulDnpSizeInMB / ulDnpAlignmentInMB) * ulDnpAlignmentInMB);

    if (ulDppSizeInMB < ulMinDppSizeInMB) {
        ulDppSizeInMB = ulMinDppSizeInMB;
    }

    if (ulDnpSizeInMB < ulMinDnpSizeInMB) {
        ulDnpSizeInMB = ulMinDnpSizeInMB;
    }

    DebugPrintf(SV_LOG_ALWAYS, "%s: ulDnpSizeInPercentage = %lu ulMaxDnpSizeInMB = %lu ulMinDnpSizeInMB = %lu ulDnpAlignmentInMB = %lu\n",
        FUNCTION_NAME,
        ulDnpSizeInPercentage,
        ulMaxDnpSizeInMB,
        ulMinDnpSizeInMB,
        ulDnpAlignmentInMB);

    try {
        DebugPrintf(SV_LOG_ALWAYS, "%s: Configuring Data Paged Pool size = %lu MB RamSize = %llu MB\n", FUNCTION_NAME, ulDppSizeInMB, ullTotalPhysicalRamInMB);
        DebugPrintf(SV_LOG_ALWAYS, "%s: Configuring Data NonPaged Pool size = %lu MB FreePhysicalMem = %llu MB\n", FUNCTION_NAME, ulDnpSizeInMB, ullTotalFreePhysicalMemInMB);
        m_sPFilterDriverNotifier->ConfigureDataPool(ulDppSizeInMB, ulDnpSizeInMB);
        bStatus = true;
    }
    catch (FilterDriverNotifier::Exception ex) {
        DebugPrintf(SV_LOG_ERROR, "%s: Failed to configure driver err=%s.\n", FUNCTION_NAME, ex.what());
    }
    catch (std::exception& ex) {
        DebugPrintf(SV_LOG_ERROR, "%s: Failed to configure driver err=%s.\n", FUNCTION_NAME, ex.what());
    }
    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
    return bStatus;
}
#endif
