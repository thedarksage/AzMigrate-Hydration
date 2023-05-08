#pragma once


#define MAX_DC_IO_SIZE_BUCKETS          12

typedef enum 
{
    READ_BUCKET_INITIALIZER = 0,
    WRITE_BUCKET_INITIALIZER
}PROFILE_INITIALIZER_TYPE;

typedef struct _DEVICE_PROFILE_INITIALIZER
{
    PROFILE_INITIALIZER_TYPE Type;
    union
    {
        SET_IO_SIZE_ARRAY_INPUT IoReadArraySizeInput;
        SET_IO_SIZE_ARRAY_INPUT IoWriteArraySizeInput;
    }Profile;
}DEVICE_PROFILE_INITIALIZER, *PDEVICE_PROFILE_INITIALIZER;

typedef struct _DEV_READ_PROFILE
{
    ULONGLONG ReadIoSizeCounters[MAX_DC_IO_SIZE_BUCKETS];
    ULONG ReadIoSizeArray[MAX_DC_IO_SIZE_BUCKETS];
    ULONG MostFrequentlyUsedBucket;
}DEV_READ_PROFILE, *PDEV_READ_PROFILE;

typedef struct _DEVICE_PROFILE
{
    DEV_READ_PROFILE ReadProfile;
}DEVICE_PROFILE, *PDEVICE_PROFILE;

VOID
InitializeProfiler(PDEVICE_PROFILE DeviceProfile, PDEVICE_PROFILE_INITIALIZER ProfileInitializer);

#if DBG
VOID
LogReadIOSize(ULONG IoSize, PDEVICE_PROFILE DeviceProfile);
#endif

class ValueDistribution {
public:
    ValueDistribution(ULONG ulMaxBuckets);
    ValueDistribution(ULONG ulMaxBuckets, ULONG ulBuckets, PULONG ulSizeArray);
    ~ValueDistribution();

    void * __cdecl operator new(size_t size);

    NTSTATUS SetDistributionSize(ULONG ulBuckets, PULONG ulSizeArray);
    void AddToDistribution(ULONG ulValue);

    void GetDistributionSize(PULONG pBucketSizes, ULONG ulBuckets);

    void GetDistribution(PULONGLONG pBuckets, ULONG ulBuckets);
    void Clear();

private:
    // Declare default constructor but do not define
    ValueDistribution();
    void AllocateDistribution(ULONG ulMaxBuckets);

private:
    PULONGLONG  m_DistributionArray;
    PULONG      m_DistributionSize;
    ULONG       m_ulBuckets;
    ULONG       m_ulMaxBuckets;
    ULONG       m_ulFrequentlyUsedBucket;
    bool        m_bInitialized;
};

class ValueLog {
public:
    ValueLog(ULONG ulMaxLogValues);
    ~ValueLog();

    void * __cdecl operator new(size_t size);

    void SetLogSize(ULONG ulMaxValues);
    void AddToLog(ULONG ulValue);
    ULONG GetMaxLogSize() { return m_ulMaxValues; }
    ULONG GetLog(PULONG Log, ULONG ulMaxValues);
    void GetMinAndMaxLoggedValues(ULONG &ulMinValueLogged, ULONG &ulMaxValueLogged)
    {
        ulMinValueLogged = m_ulMinValueLogged;
        ulMaxValueLogged = m_ulMaxValueLogged;
    }
    void Clear();

private:
    // Declate default constructor but do not define
    ValueLog();
    void AllocateLogBuffer(ULONG ulMaxLogValues);

private:
    PULONG  m_LogValues;
    ULONG   m_ulMaxValues;
    ULONG   m_ulIndex;
    bool    m_bInitialized;
    ULONG   m_ulMaxValueLogged;
    ULONG   m_ulMinValueLogged;
};

