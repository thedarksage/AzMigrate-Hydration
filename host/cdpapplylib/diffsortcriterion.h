//
// Copyright (c) 2006 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : diffsortcriterion.h
//
// Description: 
//

#ifndef DIFFSORTCRITERION_H
#define DIFFSORTCRITERION_H

#include <string>
#include <set>

// differential filename versions
#define DIFFIDVERSION1 1
#define DIFFIDVERSION2 2
#define DIFFIDVERSION3 3

static const unsigned long long MAX_DIRTY_BLOCK_NUMBER = 18446744073709551615ULL;

class DiffSortCriterion {
public:
    enum SORT_VALUES { MIN_SORT_VALUE, MAX_SORT_VALUE, FROM_FILE_SORT_VALUE };

    typedef bool (DiffSortCriterion::*DiffCompareFunction)(DiffSortCriterion const & rhs) const;

    template <DiffCompareFunction Compare>  class SortBy {
    public:
        bool operator()(DiffSortCriterion const & lhs, DiffSortCriterion const & rhs) const  {
            return (lhs.*Compare)(rhs); 
        }
    };            
    
    DiffSortCriterion(SORT_VALUES sortValue);
    DiffSortCriterion(char const * diffId, std::string const & volumeName);
    DiffSortCriterion(DiffSortCriterion const & rhs);
    
	bool Lessthan(DiffSortCriterion const & rhs) const;
    bool Equal(DiffSortCriterion const & rhs) const ;
    bool IsSplitIoEnd() const { return (1 != m_ContinuationId && m_EndContinuation); }
    bool IsSplitIoStart() const { return (1 == m_ContinuationId && !m_EndContinuation); }
    bool IsSplitIo() const { return !m_EndContinuation; }
    bool IsNonSplitIo() const { return (1 == m_ContinuationId && m_EndContinuation); }

    bool IsContinuationEnd() const { return m_EndContinuation; }

    std::string const & Id() const { return m_Id; }
	int IdVersion() const { return m_IdVersion; }

	unsigned long long ResyncCounter() const { return m_ResyncCounter; }
	unsigned long long DirtyBlockCounter() const { return m_DirtyBlockCounter; }

    unsigned long long CxTime() const { return m_CxTime; }

    unsigned long long PreviousEndTime() const { return m_PrevEndTime; }
    unsigned long long PreviousEndTimeSequenceNumber() const { return m_PrevEndTimeSequenceNumber; }
    int PreviousContinuationId() const { return m_PrevContinuationId; }             

	unsigned long long EndTime() const { return m_EndTime; }
    unsigned long long EndTimeSequenceNumber() const { return m_EndTimeSequenceNumber; }
    int ContinuationId() const { return m_ContinuationId; }             

    std::string const & Mode() const { return m_Mode; }

    typedef SortBy<&DiffSortCriterion::Lessthan> CompareEndTime;

    typedef std::multiset<DiffSortCriterion, CompareEndTime>  OrderedEndTime_t;

protected:
	void ParseIdBasedOnVersion(std::string const& id);
	void ParseIdV3(std::string const & id);
	void ParseIdV2(std::string const & id);
    void ParseIdV1(std::string const & id);

private:
    std::string m_Id;
	
	SORT_VALUES m_SortValue;

	int m_IdVersion;

	unsigned long long m_ResyncCounter;
	unsigned long long m_DirtyBlockCounter;

    unsigned long long m_CxTime;

    unsigned long long m_PrevEndTime;
    unsigned long long m_PrevEndTimeSequenceNumber;    
    unsigned int m_PrevContinuationId;

    unsigned long long m_EndTime;
    unsigned long long m_EndTimeSequenceNumber;    
    unsigned int m_ContinuationId;

    bool m_EndContinuation;

    std::string m_Mode;

    std::string m_VolumeName;
};

#endif // ifndef DIFFSORTCRITERION_H
