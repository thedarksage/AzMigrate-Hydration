#ifndef _PROFILER_H_
#define _PROFILER_H_

#define MAX_VC_IO_SIZE_BUCKETS          12

typedef enum PROFILE_INITIALIZER_TYPE
{
    READ_BUCKET_INITIALIZER = 0,
    WRITE_BUCKET_INITIALIZER
};

typedef struct _VOLUME_PROFILE_INITIALIZER
{
    PROFILE_INITIALIZER_TYPE Type;
    union
    {
        SET_IO_SIZE_ARRAY_INPUT IoReadArraySizeInput;
        SET_IO_SIZE_ARRAY_INPUT IoWriteArraySizeInput;
    }Profile;
}VOLUME_PROFILE_INITIALIZER, *PVOLUME_PROFILE_INITIALIZER;

typedef struct _VOLUME_READ_PROFILE
{
    ULONGLONG ReadIoSizeCounters[MAX_VC_IO_SIZE_BUCKETS];
    ULONG ReadIoSizeArray[MAX_VC_IO_SIZE_BUCKETS];
    ULONG MostFrequentlyUsedBucket;
}VOLUME_READ_PROFILE, *PVOLUME_READ_PROFILE;

typedef struct _VOLUME_PROFILE
{
    VOLUME_READ_PROFILE ReadProfile;
}VOLUME_PROFILE, *PVOLUME_PROFILE;

VOID
InitializeProfiler(PVOLUME_PROFILE VolumeProfile, PVOLUME_PROFILE_INITIALIZER ProfileInitializer);

VOID
LogReadIOSize(ULONG IoSize, PVOLUME_PROFILE VolumeProfile);

class ValueDistribution {
public:
    ValueDistribution(ULONG ulMaxBuckets);
    ValueDistribution(ULONG ulMaxBuckets, ULONG ulBuckets, PULONG ulSizeArray);
    ~ValueDistribution();

    void * __cdecl operator new(size_t size) {return malloc(size, TAG_VALUE_DISTRIBUTION, NonPagedPool);};

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

    void * __cdecl operator new(size_t size) {return malloc(size, TAG_VALUE_LOG, NonPagedPool);};

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
#endif
