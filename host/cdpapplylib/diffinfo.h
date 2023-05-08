//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : diffinfo.h
//
// Description:
//

#ifndef DIFFINFO_H
#define DIFFINFO_H

#include <string>
#include <set>
#include <list>

#include <boost/shared_ptr.hpp>
#include <boost/shared_array.hpp>

#include "hostagenthelpers_ported.h"
#include "transport_settings.h"
#include "volumegroupsettings.h" // FIXME: is this really needed here?
#include "diffsortcriterion.h"

class VolumeInfo;
class DifferentialSync;

class DiffInfo {
public:

    typedef boost::shared_ptr<DiffInfo> DiffInfoPtr;
    typedef std::list<DiffInfoPtr> DiffInfos_t;

    typedef bool (DiffInfo::*DiffCompareFunction)(DiffInfoPtr const & rhs) const ;
    template <DiffCompareFunction Compare>  class DiffSort {
    public:
        bool operator()(DiffInfoPtr const & lhs, DiffInfoPtr const & rhs) const {
            return (lhs.get()->*Compare)(rhs); // need to use get() here to invoke DiffInfo member function
        }
    };

    DiffInfo(VolumeInfo* volumeInfo, std::string const & location, char const * id, long size);

    bool CompareEndTime(DiffInfoPtr const & rhs) const;
    bool CompareStartTime(DiffInfoPtr const & rhs) const;
    bool OpenVolumeExclusive();


    bool IsContinuationEnd() const { return m_SortCriterion.IsContinuationEnd(); }

    bool IsSplitIoEnd() const { return m_SortCriterion.IsSplitIoEnd(); }
    bool IsSplitIoStart() const { return m_SortCriterion.IsSplitIoStart(); }
    bool IsSplitIo() const { return m_SortCriterion.IsSplitIo(); }
    bool IsNonSplitIo() const { return m_SortCriterion.IsNonSplitIo(); }

    bool CutoffTimeGreaterThenEndTime(DiffSortCriterion const & cutoffTime);
    bool EndTimeLessThenCutoffTime(DiffSortCriterion const & cutoffTime);
    bool ReadyToApply() { return m_ReadyToApply; }

    //Added second argument for fixing Bug#6984
	bool ApplyData(DifferentialSync *, bool preAllocate, bool lastFileToProcess);
    bool Secure();
    TRANSPORT_PROTOCOL Protocol();

    void SetReadyToApply(std::string const & cacheLocation);
    void SetReadyToApply(std::string const & cacheLocation, char * buffer, SV_ULONG  buflen);
    void ReleaseBuffer();

    int ContinuationId() const { return m_SortCriterion.ContinuationId(); }
    unsigned long long CxTime() const { return m_SortCriterion.CxTime(); }
    unsigned long long EndTime() const { return m_SortCriterion.EndTime(); }
    unsigned long long EndTimeSequenceNumber() const { return m_SortCriterion.EndTimeSequenceNumber(); }

    long Size() { return m_Size; }

    std::string const & CacheLocation();
    std::string const & FullCacheLocation() const { return m_FullCacheLocation; }
    std::string const & DiffLocation() const { return m_DiffLocation; }
    std::string const & Id() const { return m_Id; }
    DiffSortCriterion const & SortCriterion() { return m_SortCriterion; }
    bool EndsInDotgz();
    bool tsoFile();
    bool IfDataInMemory() { return m_bDataInMemory; }
    bool IfDataOnDisk() { return !m_bDataInMemory; }
    SV_ULONG DataLen() { return m_DataLen; }
    bool Uncompress();
    bool UncompressIfNeeded();

    char const * VolumeInfoName();
    VolumeInfo * VolInfo() { return m_VolumeInfo ;}

    typedef DiffSort<&DiffInfo::CompareEndTime>   DiffCompareEndTime;

    typedef std::multiset<DiffInfoPtr, DiffCompareEndTime> DiffInfosOrderedEndTime_t;

protected:

private:
    // no copying DiffInfo
    DiffInfo(const DiffInfo& diffInfo);
    DiffInfo& operator=(const DiffInfo& diffInfo);

    std::string m_Id;
    std::string m_DiffLocation;
    std::string m_FullCacheLocation;
    boost::shared_array<char> m_DataBuffer;
    SV_ULONG m_DataLen;
    bool m_bDataInMemory;

    long m_Size;

    bool m_ReadyToApply;

    VolumeInfo* m_VolumeInfo;

    DiffSortCriterion m_SortCriterion;
};

#endif // ifndef DIFFINFO__H
