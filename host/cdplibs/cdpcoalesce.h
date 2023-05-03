//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : cdpcoalesce.h
//
// Description: 
//

#ifndef CDPCOALESCE__H
#define CDPCOALESCE__H

#include "cdpglobals.h"


// The last bucket has to be always zero.
#define SV_DEFAULT_IO_SIZE_BUCKET_0     0x200    // 512
#define SV_DEFAULT_IO_SIZE_BUCKET_1     0x400    // 1K
#define SV_DEFAULT_IO_SIZE_BUCKET_2     0x800    // 2K
#define SV_DEFAULT_IO_SIZE_BUCKET_3     0x1000   // 4K
#define SV_DEFAULT_IO_SIZE_BUCKET_4     0x2000   // 8K
#define SV_DEFAULT_IO_SIZE_BUCKET_5     0x4000   // 16K
#define SV_DEFAULT_IO_SIZE_BUCKET_6     0x8000   // 32K
#define SV_DEFAULT_IO_SIZE_BUCKET_7     0x10000  // 64K
#define SV_DEFAULT_IO_SIZE_BUCKET_8     0x20000  // 128K
#define SV_DEFAULT_IO_SIZE_BUCKET_9     0x40000  // 256K
#define SV_DEFAULT_IO_SIZE_BUCKET_10     0x80000  // 512K
#define SV_DEFAULT_IO_SIZE_BUCKET_11     0x100000 // 1M
#define SV_DEFAULT_IO_SIZE_BUCKET_12     0x200000 // 2M
#define SV_DEFAULT_IO_SIZE_BUCKET_13     0x400000 // 4M
#define SV_DEFAULT_IO_SIZE_BUCKET_14    0x800000 // 8M
#define SV_DEFAULT_IO_SIZE_BUCKET_15    0x0      // > 8M


typedef struct _LesceStats_ 
{
  SV_ULONGLONG       dups;
  SV_ULONGLONG       dups_len;

  SV_ULONGLONG       overlaps;
  SV_ULONGLONG       overlaps_len;


  SV_ULONGLONG       total_writes;
  SV_ULONGLONG       total_write_len;
  SV_ULONGLONG       total_coalesced_writes;
  SV_ULONGLONG       total_coalesced_writes_len;

  SV_ULONGLONG       _512b_writes;
  SV_ULONGLONG       _512b_1k_writes;
  SV_ULONGLONG       _1k_2k_writes;
  SV_ULONGLONG       _2k_4k_writes;
  SV_ULONGLONG       _4k_8k_writes;
  SV_ULONGLONG       _8k_16k_writes;
  SV_ULONGLONG       _16k_32k_writes;
  SV_ULONGLONG       _32k_64k_writes;
  SV_ULONGLONG       _64k_128k_writes;
  SV_ULONGLONG       _128k_256k_writes;
  SV_ULONGLONG       _256k_512k_writes;
  SV_ULONGLONG       _512k_1M_writes;
  SV_ULONGLONG       _1M_2M_writes;
  SV_ULONGLONG       _2M_4M_writes;
  SV_ULONGLONG       _4M_8M_writes;
  SV_ULONGLONG       _8M_above_writes;

  SV_ULONGLONG       _512b_coaleced_writes;
  SV_ULONGLONG       _512b_1k_coaleced_writes;
  SV_ULONGLONG       _1k_2k_coaleced_writes;
  SV_ULONGLONG       _2k_4k_coaleced_writes;
  SV_ULONGLONG       _4k_8k_coaleced_writes;
  SV_ULONGLONG       _8k_16k_coaleced_writes;
  SV_ULONGLONG       _16k_32k_coaleced_writes;
  SV_ULONGLONG       _32k_64k_coaleced_writes;
  SV_ULONGLONG       _64k_128k_coaleced_writes;
  SV_ULONGLONG       _128k_256k_coaleced_writes;
  SV_ULONGLONG       _256k_512k_coaleced_writes;
  SV_ULONGLONG       _512k_1M_coaleced_writes;
  SV_ULONGLONG       _1M_2M_coaleced_writes;
  SV_ULONGLONG       _2M_4M_coaleced_writes;
  SV_ULONGLONG       _4M_8M_coaleced_writes;
  SV_ULONGLONG       _8M_above_coaleced_writes;

} LesceStats;


struct cdp_lesce_drtd_t
{
    bool CompareEndOffset(boost::shared_ptr<cdp_lesce_drtd_t> const & y) const ;
    bool CompareStartOffset(boost::shared_ptr<cdp_lesce_drtd_t> const & y) const ;
    bool CompareFileId(boost::shared_ptr<cdp_lesce_drtd_t> const & y) const ;

	SV_ULONGLONG  Length;
	SV_OFFSET_TYPE  VolOffset;
	SV_OFFSET_TYPE  FileOffset;
	SV_ULONG        FileId;
};

typedef boost::shared_ptr<cdp_lesce_drtd_t> cdp_lesce_drtdptr_t;
typedef std::vector< cdp_lesce_drtdptr_t >   cdp_lesce_drtdptrs_t;
typedef cdp_lesce_drtdptrs_t::iterator cdp_lesce_drtdptrs_iter_t; 
typedef cdp_lesce_drtdptrs_t::reverse_iterator cdp_lesce_drtdptrs_riter_t; 

typedef bool (cdp_lesce_drtd_t::*CompareFunction)(cdp_lesce_drtdptr_t const & rhs) const ;
template <CompareFunction Compare>  class DrtdSort 
{
public:
	bool operator()(cdp_lesce_drtdptr_t const & lhs, cdp_lesce_drtdptr_t const & rhs) const 
	{
		return (lhs.get()->*Compare)(rhs); 
	}
};

typedef DrtdSort<&cdp_lesce_drtd_t::CompareEndOffset>   DrtdCompareEndOffset;
typedef DrtdSort<&cdp_lesce_drtd_t::CompareStartOffset> DrtdCompareStartOffset;
typedef DrtdSort<&cdp_lesce_drtd_t::CompareFileId>      DrtdCompareFileId;
typedef std::set<cdp_lesce_drtdptr_t, DrtdCompareEndOffset> DrtdsOrderedEndOffset_t;
typedef std::set<cdp_lesce_drtdptr_t, DrtdCompareStartOffset> DrtdsOrderedStartOffset_t;
typedef std::multiset<cdp_lesce_drtdptr_t, DrtdCompareFileId> DrtdsOrderedFileId_t;


bool coalesce_dirty_block(DrtdsOrderedStartOffset_t & final_drtds, 
                           const cdp_lesce_drtd_t & drtdToAdd, 
                           LesceStats & lstats);

bool coalesce_dirty_blocks(Differentials_t & diffs, 
                           DrtdsOrderedFileId_t coalesced_drtds,
                           LesceStats & lstats, 
                           bool retainoldestchange);

#endif

