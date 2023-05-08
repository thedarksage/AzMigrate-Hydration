//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File:       svdrtd.h
//
// Description: 
//

#ifndef SVDRTD__H
#define SVDRTD__H


#include <vector>
#include <set>
#include <string>
#include <boost/shared_ptr.hpp>

#include "svtypes.h"


struct SV_DRTD
{
    SV_ULONGLONG  Length;
    SV_OFFSET_TYPE  VolOffset;
    SV_OFFSET_TYPE  FileOffset;
	std::string FileName;
};

typedef boost::shared_ptr<SV_DRTD> SvDrtdPtr;
typedef std::vector< SvDrtdPtr >   Drtds_t;
typedef Drtds_t::iterator DrtdsIterator; 
typedef Drtds_t::reverse_iterator DrtdsRevIter; 


struct SV_DRTD_V2
{
    SV_ULONGLONG  Length;
    SV_OFFSET_TYPE  VolOffset;
    SV_OFFSET_TYPE  FileOffset;
    SV_UINT      	SequenceNumberDelta;
    SV_UINT      	TimeDelta;
};

typedef boost::shared_ptr<SV_DRTD_V2> SvDrtdV2Ptr;
typedef std::vector< SvDrtdV2Ptr >   DrtdsV2_t;
typedef DrtdsV2_t::iterator DrtdsV2Iterator; 
typedef DrtdsV2_t::const_iterator DrtdsV2ConstIterator; 
typedef DrtdsV2_t::reverse_iterator DrtdsV2RevIter; 


struct SV_DRTD_V3
{
    bool CompareEndOffset(boost::shared_ptr<SV_DRTD_V3> const & y) const ;
    bool CompareStartOffset(boost::shared_ptr<SV_DRTD_V3> const & y) const ;
    bool CompareFileId(boost::shared_ptr<SV_DRTD_V3> const & y) const ;

	SV_ULONGLONG  Length;
	SV_OFFSET_TYPE  VolOffset;
	SV_OFFSET_TYPE  FileOffset;
	SV_ULONG        FileId;
};

typedef boost::shared_ptr<SV_DRTD_V3> SvDrtdV3Ptr;
typedef std::vector< SvDrtdV3Ptr >   DrtdsV3_t;
typedef DrtdsV3_t::iterator DrtdsV3Iterator; 
typedef DrtdsV3_t::reverse_iterator DrtdsV3RevIter; 

typedef bool (SV_DRTD_V3::*CompareFunction)(SvDrtdV3Ptr const & rhs) const ;
template <CompareFunction Compare>  class DrtdSort 
{
public:
	bool operator()(SvDrtdV3Ptr const & lhs, SvDrtdV3Ptr const & rhs) const 
	{
		return (lhs.get()->*Compare)(rhs); 
	}
};

typedef DrtdSort<&SV_DRTD_V3::CompareEndOffset>   DrtdCompareEndOffset;
typedef DrtdSort<&SV_DRTD_V3::CompareStartOffset> DrtdCompareStartOffset;
typedef DrtdSort<&SV_DRTD_V3::CompareFileId>      DrtdCompareFileId;
typedef std::set<SvDrtdV3Ptr, DrtdCompareEndOffset> DrtdsOrderedEndOffset_t;
typedef std::set<SvDrtdV3Ptr, DrtdCompareStartOffset> DrtdsOrderedStartOffset_t;
typedef std::multiset<SvDrtdV3Ptr, DrtdCompareFileId> DrtdsOrderedFileId_t;


#endif