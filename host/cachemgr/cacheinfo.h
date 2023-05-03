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

#ifndef CACHEINFO_H
#define CACHEINFO_H

#include <string>
#include <set>
#include <list>

#include <boost/shared_ptr.hpp>
#include <boost/shared_array.hpp>

#include "diffsortcriterion.h"

enum CACHEINFO_STATE { PENDING_DOWNLOAD, READY_TO_WRITE, READY_TO_VERIFY, READY_TO_RENAME, READY_TO_DELETE_FROM_PS, DELETED_FROM_PS };

class CacheInfo 
{
public:

    typedef boost::shared_ptr<CacheInfo> CacheInfoPtr;
    typedef std::list<CacheInfoPtr> CacheInfos_t;

    typedef bool (CacheInfo::*CompareFunction)(CacheInfoPtr const & rhs) const ;
    template <CompareFunction Compare>  class CacheSort 
    {
    public:
        bool operator()(CacheInfoPtr const & lhs, CacheInfoPtr const & rhs) const 
        {
            return (lhs.get()->*Compare)(rhs); 
        }
    };

    CacheInfo(std::string const & name, unsigned long size)
        : m_name(name),
        m_size(size),
        m_state(PENDING_DOWNLOAD),
        m_SortCriterion(name.c_str(), std::string())
    {
    }

    bool CompareEndTime(CacheInfoPtr const & rhs) const
    {
        return m_SortCriterion.Lessthan(rhs->m_SortCriterion);
    }

    bool IsContinuationEnd() const { return m_SortCriterion.IsContinuationEnd(); }
    bool IsSplitIoEnd() const { return m_SortCriterion.IsSplitIoEnd(); }
    bool IsSplitIoStart() const { return m_SortCriterion.IsSplitIoStart(); }
    bool IsSplitIo() const { return m_SortCriterion.IsSplitIo(); }
    bool IsNonSplitIo() const { return m_SortCriterion.IsNonSplitIo(); }


	unsigned long long ResyncCounter() const { return m_SortCriterion.ResyncCounter(); }
	unsigned long long DirtyBlockCounter() const { return m_SortCriterion.DirtyBlockCounter(); }
    unsigned long long CxTime() const { return m_SortCriterion.CxTime(); }
    unsigned long long EndTime() const { return m_SortCriterion.EndTime(); }
    unsigned long long EndTimeSequenceNumber() const { return m_SortCriterion.EndTimeSequenceNumber(); }
    int ContinuationId() const { return m_SortCriterion.ContinuationId(); }


    std::string Name() const { return m_name; }
    unsigned long Size() { return m_size; }
    //CACHEINFO_STATE State() { return m_state ; }

    bool isReadyForDownLoad()
    {  
        return (m_state == PENDING_DOWNLOAD);
    }

    bool isDownLoadRequired()
    {  
        return (m_state == PENDING_DOWNLOAD);
    }

    void SetReadyForWriteToDisk(char * buf)
    {
        // just in case someone called SetReadyForWriteToDisk 
        //  multiple times without releasing previous memory
        m_DataBuffer.reset();

        // use smart pointer to track 
        // need to pass in the deallocator here to
        // make sure it will use free
        // the assumption that te buffer source is allocated using malloc
        // or equivalent
        m_DataBuffer.reset(buf, free);
        m_state = READY_TO_WRITE;
    }

    bool isReadyForWriteToDiskOrVerification()
    {
        return ((m_state == READY_TO_WRITE) ||  (m_state == READY_TO_VERIFY));
    }

    bool isWriteToDiskRequired()
    {
        return ((m_state == PENDING_DOWNLOAD) || (m_state == READY_TO_WRITE));
    }


    void SetReadyToVerify()
    {
        m_state = READY_TO_VERIFY;
    }

    bool isReadyToVerify()
    {
        return (m_state == READY_TO_VERIFY);
    }

    void SetReadyToRename() 
    {  
        m_DataBuffer.reset();
        m_state = READY_TO_RENAME ; 
    }

    bool isReadyForRename()
    {
        return (m_state == READY_TO_RENAME);
    }

    bool isRenameRequired()
    {
       return ((m_state == PENDING_DOWNLOAD) || (m_state == READY_TO_WRITE)
           || (m_state == READY_TO_VERIFY) || (m_state == READY_TO_RENAME));
    }

    void SetReadyToDeleteFromPS() 
    {  
        m_DataBuffer.reset();
        m_state = READY_TO_DELETE_FROM_PS ; 
    }

    bool isReadyForDeletion()
    {
        return (m_state == READY_TO_DELETE_FROM_PS);
    }

    bool isDeletionRequired()
    {
       return ((m_state == PENDING_DOWNLOAD) || (m_state == READY_TO_WRITE)
		   || (m_state == READY_TO_VERIFY)   || (m_state == READY_TO_RENAME)
		   || (m_state == READY_TO_DELETE_FROM_PS));
    }

    void SetDeletedFromPS()
    {
        m_state = DELETED_FROM_PS;
    }

    char * buffer()
    {
        return m_DataBuffer.get();
    }

    bool isCompressed()
    {
        std::string::size_type idx = m_name.rfind(".gz");
        if (std::string::npos == idx || (m_name.length() - idx) > 3) 
        {
            return false; // uncompressed;
        }
        return true;
    }

    DiffSortCriterion const & SortCriterion() { return m_SortCriterion; }

    typedef CacheSort<&CacheInfo::CompareEndTime>   CacheCompareEndTime;

    typedef std::set<CacheInfoPtr, CacheCompareEndTime> CacheInfosOrderedEndTime_t;
    typedef std::vector<CacheInfoPtr> CacheInfosOrderedList_t;
    typedef boost::shared_ptr<CacheInfosOrderedList_t> CacheInfosOrderedListPtr_t;

protected:    

private:

    // no copying CacheInfo
    CacheInfo(const CacheInfo& cacheInfo);
    CacheInfo& operator=(const CacheInfo& cacheInfo);

    std::string m_name;
    unsigned long m_size;

    mutable CACHEINFO_STATE  m_state;
    boost::shared_array<char> m_DataBuffer;

    DiffSortCriterion m_SortCriterion;
};

#endif // ifndef DIFFINFO__H
