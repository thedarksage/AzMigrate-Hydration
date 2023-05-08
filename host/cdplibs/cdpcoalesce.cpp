//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : cdpcoalesce.cpp
//
// Description: 
//

#include "cdpcoalesce.h"
using namespace std;

bool cdp_lesce_drtd_t::CompareEndOffset(cdp_lesce_drtdptr_t const & y) const
{
	SV_ULONGLONG x_startoffset = this -> VolOffset;
	SV_ULONGLONG y_startoffset = y -> VolOffset;
	SV_ULONGLONG x_endoffset = (this -> VolOffset + this -> Length -1);
	SV_ULONGLONG y_endoffset = (y -> VolOffset + y -> Length -1);

	return (x_endoffset <  y_endoffset);
}

bool cdp_lesce_drtd_t::CompareStartOffset(cdp_lesce_drtdptr_t const & y) const
{
	SV_ULONGLONG x_startoffset = this -> VolOffset;
	SV_ULONGLONG y_startoffset = y -> VolOffset;
	SV_ULONGLONG x_endoffset = (this -> VolOffset + this -> Length -1);
	SV_ULONGLONG y_endoffset = (y -> VolOffset + y -> Length -1);

	return (x_startoffset <  y_startoffset);
}

bool cdp_lesce_drtd_t::CompareFileId(cdp_lesce_drtdptr_t const & y) const
{
	return ( this -> FileId < y -> FileId );
}

bool disjoint(cdp_lesce_drtdptr_t x, const cdp_lesce_drtd_t & y)
{
    SV_ULONGLONG x_startoffset = x -> VolOffset;
    SV_ULONGLONG y_startoffset = y.VolOffset;
    SV_ULONGLONG x_endoffset = (x -> VolOffset + x -> Length -1);
    SV_ULONGLONG y_endoffset = (y.VolOffset + y.Length -1);

    return ( (y_endoffset < x_startoffset) || ( y_startoffset > x_endoffset) );
}

bool contains(cdp_lesce_drtdptr_t x, const cdp_lesce_drtd_t & y, SV_ULONGLONG & dup_len)
{

    SV_ULONGLONG x_startoffset = x -> VolOffset;
    SV_ULONGLONG y_startoffset = y.VolOffset;
    SV_ULONGLONG x_endoffset = (x -> VolOffset + x -> Length -1);
    SV_ULONGLONG y_endoffset = (y.VolOffset + y.Length -1);

    if( (x_startoffset <= y_startoffset) && (x_endoffset >= y_endoffset) )
    {
        dup_len = y.Length;
        return true;
    }

    return false;
}


bool r_overlap(cdp_lesce_drtdptr_t x, const cdp_lesce_drtd_t & y,
               SV_ULONGLONG & r_overlap_len, cdp_lesce_drtd_t & r_nonoverlap)
{

    SV_ULONGLONG x_startoffset = x -> VolOffset;
    SV_ULONGLONG y_startoffset = y.VolOffset;
    SV_ULONGLONG x_endoffset = (x -> VolOffset + x -> Length -1);
    SV_ULONGLONG y_endoffset = (y.VolOffset + y.Length -1);

    if( (x_startoffset <= y_startoffset) && (x_endoffset < y_endoffset) )
    {
        r_overlap_len = (x_endoffset - y_startoffset + 1);
        r_nonoverlap.VolOffset = x_endoffset + 1;
        r_nonoverlap.Length = y_endoffset - x_endoffset;
        r_nonoverlap.FileOffset = y.FileOffset + r_overlap_len;
        r_nonoverlap.FileId = y.FileId;
        return true;
    }

    return false;
}

bool l_overlap(cdp_lesce_drtdptr_t x, const cdp_lesce_drtd_t & y,
               SV_ULONGLONG & l_overlap_len, cdp_lesce_drtd_t & l_nonoverlap)
{

    SV_ULONGLONG x_startoffset = x -> VolOffset;
    SV_ULONGLONG y_startoffset = y.VolOffset;
    SV_ULONGLONG x_endoffset = (x -> VolOffset + x -> Length -1);
    SV_ULONGLONG y_endoffset = (y.VolOffset + y.Length -1);

    if( (y_startoffset < x_startoffset) && (x_endoffset >= y_endoffset) )
    {
        l_overlap_len = (y_endoffset - x_startoffset + 1);
        l_nonoverlap.VolOffset = y_startoffset;
        l_nonoverlap.Length = x_startoffset - y_startoffset;
        l_nonoverlap.FileOffset = y.FileOffset;
        l_nonoverlap.FileId = y.FileId;
        return true;
    }

    return false;
}

bool iscontained(cdp_lesce_drtdptr_t x, const cdp_lesce_drtd_t & y, 
                 SV_ULONGLONG & overlap_len, 
                 cdp_lesce_drtd_t & l_nonoverlap,
                 cdp_lesce_drtd_t & r_nonoverlap)
{

    SV_ULONGLONG x_startoffset = x -> VolOffset;
    SV_ULONGLONG y_startoffset = y.VolOffset;
    SV_ULONGLONG x_endoffset = (x -> VolOffset + x -> Length -1);
    SV_ULONGLONG y_endoffset = (y.VolOffset + y.Length -1);

    if( (x_startoffset > y_startoffset) && (x_endoffset < y_endoffset) )
    {
        overlap_len = x -> Length;

        l_nonoverlap.VolOffset = y_startoffset;
        l_nonoverlap.Length = x_startoffset - y_startoffset;
        l_nonoverlap.FileOffset = y.FileOffset;
        l_nonoverlap.FileId = y.FileId;


        r_nonoverlap.VolOffset = x_endoffset + 1;
        r_nonoverlap.Length = y_endoffset - x_endoffset;
        r_nonoverlap.FileOffset = y.FileOffset + overlap_len + l_nonoverlap.Length;
        r_nonoverlap.FileId = y.FileId;

        return true;
    }

    return false;
}

DrtdsOrderedStartOffset_t::iterator  
get_start_point(DrtdsOrderedStartOffset_t & final_drtds, 
                     const cdp_lesce_drtd_t & drtdToAdd)
{
    DrtdsOrderedStartOffset_t::iterator drtdIter = final_drtds.begin();
    DrtdsOrderedStartOffset_t::iterator drtdEnd = final_drtds.end();
    SV_ULONGLONG curr_endoffset = 0;

    for( ; drtdIter != drtdEnd ; ++drtdIter)
    {
        cdp_lesce_drtdptr_t curr = *drtdIter;
        curr_endoffset = (curr -> VolOffset + curr -> Length -1);
        if( curr_endoffset >= drtdToAdd.VolOffset )
        {
            break;
        }
    }

    return drtdIter;
}



SV_ULONG get_num_records(Differentials_t & diffs)
{
    SV_ULONG numRcds = 0;
    DiffIterator diffsiter = diffs.begin();
    DiffIterator diffsend = diffs.end();
    for( ; diffsiter != diffsend; ++diffsiter)
    {
        DiffPtr diffptr = (*diffsiter);
        numRcds += diffptr -> NumDrtds();
    }

    return numRcds;
}

void fill_iopattern_info(const cdp_lesce_drtd_t & drtd, LesceStats & lstats, bool coalesced)
{
    SV_ULONGLONG len = drtd.Length;

    if(len <= SV_DEFAULT_IO_SIZE_BUCKET_0)
    {
		(coalesced)?(lstats._512b_coaleced_writes++):(lstats._512b_writes++);
    } else if(len <= SV_DEFAULT_IO_SIZE_BUCKET_1)
    {
		(coalesced)?(lstats._512b_1k_coaleced_writes++):(lstats._512b_1k_writes++);
    } else if(len <= SV_DEFAULT_IO_SIZE_BUCKET_2)
    {
		(coalesced)?(lstats._1k_2k_coaleced_writes++):(lstats._1k_2k_writes++);
    } else if(len <= SV_DEFAULT_IO_SIZE_BUCKET_3)
    {
		(coalesced)?(lstats._2k_4k_coaleced_writes++):(lstats._2k_4k_writes++);
    } else if(len <= SV_DEFAULT_IO_SIZE_BUCKET_4)
    {
		(coalesced)?(lstats._4k_8k_coaleced_writes++):(lstats._4k_8k_writes++);
    } else if(len <= SV_DEFAULT_IO_SIZE_BUCKET_5)
    {
		(coalesced)?(lstats._8k_16k_coaleced_writes++):(lstats._8k_16k_writes++);
    } else if(len <= SV_DEFAULT_IO_SIZE_BUCKET_6)
    {
		(coalesced)?(lstats._16k_32k_coaleced_writes++):(lstats._16k_32k_writes++);
    } else if(len <= SV_DEFAULT_IO_SIZE_BUCKET_7)
    {
		(coalesced)?(lstats._32k_64k_coaleced_writes++):(lstats._32k_64k_writes++);
    } else if(len <= SV_DEFAULT_IO_SIZE_BUCKET_8)
    {
		(coalesced)?(lstats._64k_128k_coaleced_writes++):(lstats._64k_128k_writes++);
    } else if(len <= SV_DEFAULT_IO_SIZE_BUCKET_9)
    {
		(coalesced)?(lstats._128k_256k_coaleced_writes++):(lstats._128k_256k_writes++);
    } else if(len <= SV_DEFAULT_IO_SIZE_BUCKET_10)
    {
		(coalesced)?(lstats._256k_512k_coaleced_writes++):(lstats._256k_512k_writes++);
    } else if(len <= SV_DEFAULT_IO_SIZE_BUCKET_11)
    {
		(coalesced)?(lstats._512k_1M_coaleced_writes++):(lstats._512k_1M_writes++);
    } else if(len <= SV_DEFAULT_IO_SIZE_BUCKET_12)
    {
		(coalesced)?(lstats._1M_2M_coaleced_writes++):(lstats._1M_2M_writes++);
    } else if(len <= SV_DEFAULT_IO_SIZE_BUCKET_13)
    {
		(coalesced)?(lstats._2M_4M_coaleced_writes++):(lstats._2M_4M_writes++);
    } else if(len <= SV_DEFAULT_IO_SIZE_BUCKET_14)
    {
		(coalesced)?(lstats._4M_8M_coaleced_writes++):(lstats._4M_8M_writes++);
    } else
    {
		(coalesced)?(lstats._8M_above_coaleced_writes++):(lstats._8M_above_writes++);
    }
}

bool coalesce_dirty_block(DrtdsOrderedStartOffset_t & final_drtds, 
                           const cdp_lesce_drtd_t & drtdToAdd, 
                           LesceStats & lstats)
{

    DrtdsOrderedStartOffset_t::iterator drtdsIter = get_start_point(final_drtds,drtdToAdd);
    DrtdsOrderedStartOffset_t::iterator drtdsEnd = final_drtds.end();
    
    std::list<cdp_lesce_drtd_t> drtdsToAdd;

    cdp_lesce_drtd_t r_nonoverlap;
    cdp_lesce_drtd_t l_nonoverlap;
    SV_ULONGLONG r_overlap_len = 0;
    SV_ULONGLONG l_overlap_len = 0;
    SV_ULONGLONG dup_len = 0;
    cdp_lesce_drtd_t drtdToMatch;



    drtdToMatch.VolOffset = drtdToAdd.VolOffset;
    drtdToMatch.Length = drtdToAdd.Length;
    drtdToMatch.FileOffset = drtdToAdd.FileOffset;
    drtdToMatch.FileId = drtdToAdd.FileId;

    fill_iopattern_info(drtdToAdd, lstats, false);
    lstats.total_writes++;
    lstats.total_write_len += drtdToAdd.Length;

    for( ; drtdsIter != drtdsEnd ; ++drtdsIter)
    {
        cdp_lesce_drtdptr_t currdrtd = *drtdsIter;
        if(disjoint(currdrtd, drtdToMatch))
        {
            drtdsToAdd.push_back(drtdToMatch);
            fill_iopattern_info(drtdToMatch, lstats, true);
            lstats.total_coalesced_writes++;
            lstats.total_coalesced_writes_len += drtdToMatch.Length;
            break;
        }

        if(contains(currdrtd, drtdToMatch, dup_len))
        {
            lstats.dups++;
            lstats.dups_len += dup_len;
            break;
        }

        if(l_overlap(currdrtd, drtdToMatch, l_overlap_len, l_nonoverlap))
        {
            lstats.overlaps++;
            lstats.overlaps_len += l_overlap_len;

            drtdsToAdd.push_back(l_nonoverlap);
            fill_iopattern_info(l_nonoverlap, lstats, true);
            lstats.total_coalesced_writes++;
            lstats.total_coalesced_writes_len += l_nonoverlap.Length;
            break;
        }

        if(r_overlap(currdrtd, drtdToMatch, r_overlap_len, r_nonoverlap))
        {
            lstats.overlaps++;
            lstats.overlaps_len += r_overlap_len;

            drtdToMatch.VolOffset = r_nonoverlap.VolOffset;
            drtdToMatch.Length = r_nonoverlap.Length;
            drtdToMatch.FileOffset = r_nonoverlap.FileOffset;
            drtdToMatch.FileId =  r_nonoverlap.FileId;
            continue;
        }

        if(iscontained(currdrtd, drtdToMatch, l_overlap_len, l_nonoverlap, r_nonoverlap))
        {
            lstats.overlaps++;
            lstats.overlaps_len += l_overlap_len;

            drtdsToAdd.push_back(l_nonoverlap);
            fill_iopattern_info(l_nonoverlap, lstats, true);
            lstats.total_coalesced_writes++;
            lstats.total_coalesced_writes_len += l_nonoverlap.Length;

            drtdToMatch.VolOffset = r_nonoverlap.VolOffset;
            drtdToMatch.Length = r_nonoverlap.Length;
            drtdToMatch.FileOffset = r_nonoverlap.FileOffset;
            drtdToMatch.FileId =  r_nonoverlap.FileId;
            continue;
        }
    }
  
    if(drtdsIter == drtdsEnd)
    {
        drtdsToAdd.push_back(drtdToMatch);
        fill_iopattern_info(drtdToMatch, lstats, true);
        lstats.total_coalesced_writes++;
        lstats.total_coalesced_writes_len += drtdToMatch.Length;
    }

    std::list<cdp_lesce_drtd_t>::iterator  drtdstoAddIter = drtdsToAdd.begin();
    for( ; drtdstoAddIter != drtdsToAdd.end(); ++drtdstoAddIter)
    {
        cdp_lesce_drtdptr_t nonoverlap_drtd(new (nothrow) cdp_lesce_drtd_t(*drtdstoAddIter) );
        final_drtds.insert(nonoverlap_drtd);
    }

    return true;
}

bool coalesce_dirty_blocks(Differentials_t & diffs, 
                           DrtdsOrderedFileId_t coalesced_drtds,
                           LesceStats & lstats, 
                           bool retainoldestchange)
{
    bool rv = true;

    SV_UINT percent_complete = 0;
    SV_UINT prev_percent_wmark = 0;
    SV_LONG ChgsProcessed = 0;
    SV_ULONG NumChgs = 0;
    SV_ULONG FileId =0;
    DrtdsOrderedStartOffset_t final_drtds;

    NumChgs = get_num_records(diffs);

    if(retainoldestchange)
    {

        DiffIterator diffsiter = diffs.begin();
        DiffIterator diffsend = diffs.end();

        for( ; diffsiter != diffsend; ++diffsiter)
        {
            DiffPtr diffptr = (*diffsiter);
            
            cdp_drtdv2s_iter_t drtdsIter = diffptr -> DrtdsBegin();
            cdp_drtdv2s_iter_t drtdsEnd = diffptr -> DrtdsEnd();
            for ( ; drtdsIter != drtdsEnd ; ++drtdsIter)
            {

                cdp_drtdv2_t drtdv2 = *drtdsIter;
                cdp_lesce_drtd_t drtdv3;

				drtdv3.VolOffset  = drtdv2.get_volume_offset();
				drtdv3.Length     = drtdv2.get_length();
				drtdv3.FileOffset = drtdv2.get_file_offset();
                drtdv3.FileId     = FileId;

                coalesce_dirty_block(final_drtds, drtdv3, lstats);
                ChgsProcessed++;
                percent_complete = (ChgsProcessed*100)/(NumChgs);            
            }

            ++FileId;
        }
    } 
    else
    {

        DiffRevIterator diffsiter = diffs.rbegin();
        DiffRevIterator diffsend = diffs.rend();

        for( ; diffsiter != diffsend; ++diffsiter)
        {
            DiffPtr diffptr = (*diffsiter);

            cdp_drtdv2s_riter_t drtdsIter = diffptr -> DrtdsRbegin();
            cdp_drtdv2s_riter_t drtdsEnd = diffptr -> DrtdsRend();
            for ( ; drtdsIter != drtdsEnd ; ++drtdsIter)
            {

                cdp_drtdv2_t drtdv2 = *drtdsIter;
                cdp_lesce_drtd_t drtdv3;

                drtdv3.VolOffset  = drtdv2.get_volume_offset();
				drtdv3.Length     = drtdv2.get_length();
				drtdv3.FileOffset = drtdv2.get_file_offset();
                drtdv3.FileId     = FileId;


                coalesce_dirty_block(final_drtds, drtdv3, lstats);
                ChgsProcessed++;
                percent_complete = (ChgsProcessed*100)/(NumChgs);            
            }

            ++FileId;
        }
    }

    DrtdsOrderedStartOffset_t::iterator drtdsIter = final_drtds.begin();
    DrtdsOrderedStartOffset_t::iterator drtdsEnd = final_drtds.end();
    for( ; drtdsIter != drtdsEnd ; ++drtdsIter)
    {
        coalesced_drtds.insert(*drtdsIter);
    }

    return true;
}

