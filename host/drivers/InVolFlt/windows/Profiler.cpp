#include "Common.h"
#include "InmFltInterface.h"
#include "Profiler.h"

//This Function is called with Dev spin lock held
VOID
InitializeProfiler(PDEVICE_PROFILE DeviceProfile, PDEVICE_PROFILE_INITIALIZER ProfileInitializer)
{
    ULONG ulMaxBuckets = MAX_DC_IO_SIZE_BUCKETS - 1;
    PSET_IO_SIZE_ARRAY_INPUT IoReadArraySizeInput = NULL;

    if(ProfileInitializer->Type == READ_BUCKET_INITIALIZER)
    {
        IoReadArraySizeInput = &ProfileInitializer->Profile.IoReadArraySizeInput;
        DeviceProfile->ReadProfile.MostFrequentlyUsedBucket = 1;

        for(ULONG Index = 0; Index < ulMaxBuckets; Index++) {
            DeviceProfile->ReadProfile.ReadIoSizeArray[Index] =  IoReadArraySizeInput->ulIoSizeArray[Index];
            DeviceProfile->ReadProfile.ReadIoSizeCounters[Index] = 0;
        }

        for (ULONG Index = ulMaxBuckets; Index < MAX_DC_IO_SIZE_BUCKETS; Index++) {
            DeviceProfile->ReadProfile.ReadIoSizeArray[Index] = 0;
            DeviceProfile->ReadProfile.ReadIoSizeCounters[Index] = 0;
        }

    }
}

#if DBG
VOID
LogReadIOSize(ULONG IoSize, PDEVICE_PROFILE DeviceProfile)
{
    ULONG Index = 0;
    
    if(DeviceProfile->ReadProfile.ReadIoSizeArray[DeviceProfile->ReadProfile.MostFrequentlyUsedBucket - 1] < IoSize)
        Index = DeviceProfile->ReadProfile.MostFrequentlyUsedBucket;
    
    for (; Index < MAX_DC_IO_SIZE_BUCKETS; Index++) {
        if (DeviceProfile->ReadProfile.ReadIoSizeArray[Index]) {
            if (IoSize <= DeviceProfile->ReadProfile.ReadIoSizeArray[Index]) {
                DeviceProfile->ReadProfile.ReadIoSizeCounters[Index]++;
                
                if(Index != 0 && DeviceProfile->ReadProfile.ReadIoSizeCounters[Index] > 
                    DeviceProfile->ReadProfile.ReadIoSizeCounters[DeviceProfile->ReadProfile.MostFrequentlyUsedBucket])
                    DeviceProfile->ReadProfile.MostFrequentlyUsedBucket = Index;
                break;
            }
        } else {
            DeviceProfile->ReadProfile.ReadIoSizeCounters[Index]++;
            break;
        }
    }
}
#endif

ValueDistribution::ValueDistribution(ULONG ulMaxBuckets) : 
    m_ulMaxBuckets(0), 
    m_ulBuckets(0),
    m_ulFrequentlyUsedBucket(0),
    m_DistributionArray(NULL), 
    m_DistributionSize(NULL),
    m_bInitialized(false)
{
    AllocateDistribution(ulMaxBuckets);
}

ValueDistribution::ValueDistribution(ULONG ulMaxBuckets, ULONG ulBuckets, PULONG ulSizeArray) : 
    m_ulMaxBuckets(0), 
    m_ulBuckets(0),
    m_ulFrequentlyUsedBucket(0),
    m_DistributionArray(NULL), 
    m_DistributionSize(NULL),
    m_bInitialized(false)
{
    AllocateDistribution(ulMaxBuckets);
    ASSERT(ulMaxBuckets >= ulBuckets);
    SetDistributionSize(ulBuckets, ulSizeArray);
}

ValueDistribution::~ValueDistribution()
{
    if (m_DistributionArray) {
        ExFreePoolWithTag(m_DistributionArray, TAG_VALUE_DISTRIBUTION);
        m_DistributionArray = NULL;
    }

    if (m_DistributionSize) {
        ExFreePoolWithTag(m_DistributionSize, TAG_VALUE_DISTRIBUTION);
        m_DistributionSize = NULL;
    }

    return;
}

void
ValueDistribution::AllocateDistribution(ULONG ulMaxBuckets)
{
    m_DistributionArray = (PULONGLONG) ExAllocatePoolWithTag(NonPagedPool, ulMaxBuckets * sizeof(ULONGLONG), TAG_VALUE_DISTRIBUTION);
    if (NULL == m_DistributionArray) {
        return;
    }

    RtlZeroMemory(m_DistributionArray, ulMaxBuckets * sizeof(ULONGLONG));
    m_DistributionSize = (PULONG) ExAllocatePoolWithTag(NonPagedPool, ulMaxBuckets * sizeof(ULONG), TAG_VALUE_DISTRIBUTION);
    if (NULL == m_DistributionSize) {
        ExFreePoolWithTag(m_DistributionArray, TAG_VALUE_DISTRIBUTION);
        m_DistributionArray = NULL;
        return;
    }

    RtlZeroMemory(m_DistributionSize, ulMaxBuckets * sizeof(ULONG));
    m_ulMaxBuckets = ulMaxBuckets;
}

NTSTATUS
ValueDistribution::SetDistributionSize(ULONG ulBuckets, PULONG ulSizeArray)
{
    ASSERT(m_ulMaxBuckets >= ulBuckets);
    if (!m_ulMaxBuckets) {
        return STATUS_UNSUCCESSFUL;
    }

    if (!ulBuckets) {
        return STATUS_UNSUCCESSFUL;
    }

    if ((NULL == m_DistributionArray) || (NULL == m_DistributionSize)) {
        return STATUS_UNSUCCESSFUL;
    }

    if (ulBuckets > m_ulMaxBuckets) {
        ulBuckets = m_ulMaxBuckets;
    }

    for (unsigned long ul = 0; ul < ulBuckets; ++ul) {
        m_DistributionSize[ul] = ulSizeArray[ul];
    }

    if (!m_DistributionSize[ulBuckets - 1]) {
        // The last size is not zero. That last bucket has to be zero as wild card
        if (m_ulMaxBuckets > ulBuckets) {
            m_DistributionSize[ulBuckets] = 0x00;
            ulBuckets++;
        } else {
            // Convert the last bucket as wild card bucket
            m_DistributionSize[ulBuckets - 1] = 0x00;
        }
    }

    m_ulBuckets = ulBuckets;
    m_bInitialized = true;
    return STATUS_SUCCESS;
}

void
ValueDistribution::AddToDistribution(ULONG ulValue)
{
    if (!m_bInitialized) {
        return;
    }

    ULONG   ulIndex = 0;
    if (m_ulFrequentlyUsedBucket && (ulValue > m_DistributionSize[m_ulFrequentlyUsedBucket - 1])) {
        ulIndex = m_ulFrequentlyUsedBucket;
    }

    for (; ulIndex < m_ulMaxBuckets; ulIndex++) {
        if (m_DistributionSize[ulIndex]) {
            if (ulValue <= m_DistributionSize[ulIndex]) {
                m_DistributionArray[ulIndex]++;

                if (m_DistributionArray[ulIndex] > m_DistributionArray[m_ulFrequentlyUsedBucket])
                    m_ulFrequentlyUsedBucket = ulIndex;
                break;
            }
        } else {
            m_DistributionArray[ulIndex]++;
            break;
        }
    }

    return;
}

void
ValueDistribution::GetDistribution(PULONGLONG pBuckets, ULONG ulBuckets)
{
    if (!pBuckets || !ulBuckets) {
        return;
    }

    if (ulBuckets >= m_ulBuckets) {
        for (unsigned long ul = 0; ul < m_ulBuckets; ++ul) {
            pBuckets[ul] = m_DistributionArray[ul];
        }

        for (unsigned long ul = m_ulBuckets; ul < ulBuckets; ++ul) {
            pBuckets[ul] = 0;
        }
    } else {
        for (unsigned long ul = 0; ul < ulBuckets; ++ul) {
            pBuckets[ul] = m_DistributionArray[ul];
        }

        // Add all other bucket values to last bucket
        for (unsigned long ul = ulBuckets; ul < m_ulBuckets; ++ul) {
            pBuckets[ulBuckets - 1] += m_DistributionArray[ul];
        }
    }

    return;
}

void
ValueDistribution::GetDistributionSize(PULONG pBucketSizes, ULONG ulBuckets)
{
    if (!pBucketSizes || !ulBuckets) {
        return;
    }

    if (ulBuckets >= m_ulBuckets) {
        for (unsigned long ul = 0; ul < m_ulBuckets; ++ul) {
            pBucketSizes[ul] = m_DistributionSize[ul];
        }

        for (unsigned long ul = m_ulBuckets; ul < ulBuckets; ++ul) {
            pBucketSizes[ul] = 0;
        }
    } else {
        for (unsigned long ul = 0; ul < ulBuckets - 1; ++ul) {
            pBucketSizes[ul] = m_DistributionSize[ul];
        }

        // Add all other bucket values to last bucket
        pBucketSizes[ulBuckets - 1] = 0;
    }

    return;
}

void
ValueDistribution::Clear()
{
    for (unsigned long ul = 0; ul < m_ulMaxBuckets; ++ul) {
        m_DistributionArray[ul] = 0;
    }

    m_ulFrequentlyUsedBucket = 0;
}

void * __cdecl ValueDistribution::operator new(size_t size) {
	return (PVOID)malloc((ULONG32)size, (ULONG32)TAG_VALUE_DISTRIBUTION, NonPagedPool);
}


ValueLog::ValueLog(ULONG ulMaxLogValues) :
    m_LogValues(NULL),
    m_ulMaxValues(0),
    m_ulIndex(0),
    m_ulMaxValueLogged(0),
    m_ulMinValueLogged(0),
    m_bInitialized(false)
{
    SetLogSize(ulMaxLogValues);
}

ValueLog::~ValueLog()
{
    if (NULL != m_LogValues) {
        ExFreePoolWithTag(m_LogValues, TAG_VALUE_LOG);
        m_ulMaxValues = 0;
        m_ulIndex = 0;
        m_ulMaxValueLogged = 0;
        m_ulMinValueLogged = 0;
        m_bInitialized = false;
    }
}

void
ValueLog::SetLogSize(ULONG ulMaxValues)
{
    if (ulMaxValues < m_ulMaxValues) {
        ASSERT(NULL != m_LogValues);
        m_ulMaxValues = ulMaxValues;
        m_ulIndex = 0;
        m_ulMaxValueLogged = 0;
        m_ulMinValueLogged = 0;
        return;
    }

    if (NULL != m_LogValues) {
        ExFreePoolWithTag(m_LogValues, TAG_VALUE_LOG);
        m_bInitialized = false;
        m_ulMaxValues = 0;
        m_ulIndex = 0;
        m_ulMaxValueLogged = 0;
        m_ulMinValueLogged = 0;
    }

    m_LogValues = (PULONG)ExAllocatePoolWithTag(NonPagedPool, ulMaxValues * sizeof(ULONG), TAG_VALUE_LOG);
    if (NULL != m_LogValues) {
        m_ulMaxValues = ulMaxValues;
        m_ulIndex = 0;
        m_bInitialized = true;
        m_ulMaxValueLogged = 0;
        m_ulMinValueLogged = 0;
    }
}

void
ValueLog::AddToLog(ULONG ulValue)
{
    if ((!m_bInitialized) || (NULL == m_LogValues)) {
        return;
    }

    if (ulValue > m_ulMaxValueLogged) {
        m_ulMaxValueLogged = ulValue;
    }

    if ((0 == m_ulMinValueLogged) || (ulValue < m_ulMinValueLogged)) {
        m_ulMinValueLogged = ulValue;
    }

    ULONG ulIndex = m_ulIndex % m_ulMaxValues;

    m_LogValues[ulIndex] = ulValue;
    m_ulIndex++;

    if (m_ulIndex == 2*m_ulMaxValues) {
        m_ulIndex = m_ulMaxValues;
    }
}

ULONG
ValueLog::GetLog(PULONG Log, ULONG ulMaxValues)
{
    if (ulMaxValues > m_ulMaxValues) {
        ulMaxValues = m_ulMaxValues;
    }

    if (ulMaxValues >= m_ulIndex) {
        if (m_ulIndex) {
            ulMaxValues = m_ulIndex;
        } else {
            ulMaxValues = 0;
        }
    }

    for (ULONG ul = 0; ul < ulMaxValues; ul++) {
        ULONG   ulInstance = m_ulIndex + m_ulMaxValues - ul - 1;
        ulInstance = ulInstance % m_ulMaxValues;
        Log[ul] = m_LogValues[ulInstance];
    }

    return ulMaxValues;
}

void
ValueLog::Clear()
{
    m_ulMaxValueLogged = 0;
    m_ulMinValueLogged = 0;
    m_ulIndex = 0;
}

void * __cdecl ValueLog::operator new(size_t size) {
	return (PVOID)malloc((ULONG32)size, (ULONG32)TAG_VALUE_LOG, NonPagedPool);
}

