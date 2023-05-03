//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : cdpv3bitmap.cpp
//
// Description: 
//

#include "cdpv3bitmap.h"
#include "cdpglobals.h"

#include "logger.h"
#include "portable.h"
#include "portablehelpers.h"
#include "localconfigurator.h"

#include <sstream>
#include <fstream>

#ifdef SV_WINDOWS
#include <winioctl.h>
#endif

#include "inmalertdefs.h"
#include "configwrapper.h"


// bitmap file name prefix and suffix
static const char CDPV3_BM_PREFIX[] = "cdpv30_bm_";
static const char CDPV3_BM_SUFFIX[] = ".bmdat";

// 
// bitmap files store the timestamp to hour granularity for the cdp blocks
// the block size is chosen to be 4096 (4K)
// last written timestamp entry of size 4 bytes [ sizeof(cdp_timestamp_t) ]
// is stored for every 4k block
// in segmented bitmaps each maintaining information for 2MB blocks
// making size of each bitmap = 8MB
// i.e every 8GB of the volume requires 8MB for maintaining timestamp
// information
// space for maintaining timestamps = 0.1% of volume size
//
static const unsigned int  CDP_BM_BLOCK_SIZE = 4096;
static const unsigned int  CDP_BM_ENTRY_SIZE = sizeof(cdp_timestamp_t);
static const unsigned int  CDP_TS_SIZE = sizeof(cdp_timestamp_t);
static const unsigned int  CDP_BM_BLOCKS_PER_SEGMENTED_BITMAP = 2097152; 
static const unsigned long CDP_BM_FILE_SIZE = CDP_BM_BLOCKS_PER_SEGMENTED_BITMAP * CDP_BM_ENTRY_SIZE;

// 
// max memory to cache bitmap information in memory (1M)
// No. of entries to fetch per read/write = 1K (io size = 4K)
//
static const unsigned int CDP_BM_CACHE_SIZE = 1048576;//65536; 
static const unsigned int CDP_BLOCKS_PER_BMENTRY = 1024;
static const unsigned int CDP_BM_BUFFER_SIZE = (CDP_BLOCKS_PER_BMENTRY * CDP_BM_ENTRY_SIZE); 

static const unsigned int CDP_BM_MAXPENDINGIOS = 256;
static const unsigned int CDP_BM_MAXMEMORYFORIO = 1048576; //65536;

cdp_bitmap_t::cdp_bitmap_t()
:iocount_cv(iocount_mutex)
{

	m_iscaching_enabled = true;

#ifdef SV_WINDOWS
	m_directio = true;
#else
	m_directio = false;
#endif

	m_compressed = true;

	m_path = ".";
	m_cachesize = CDP_BM_CACHE_SIZE;
	m_memconsumed = 0;
	m_blocksize = CDP_BM_BLOCK_SIZE;
	m_blocks_per_entry = CDP_BLOCKS_PER_BMENTRY;
	m_buffer_size = CDP_BM_BUFFER_SIZE;
	m_iowrites = 0;
	m_ioreads = 0;
	m_iowrites_dueto_insufficient_memory = 0;
	m_timespent_on_io = 0;

	m_init = false;

	m_asyncio = true;
	m_pendingios = 0;
	m_maxpendingios = CDP_BM_MAXPENDINGIOS;
	m_minpendingios = m_maxpendingios -1;
	m_maxmemory_for_io = CDP_BM_MAXMEMORYFORIO;
	m_error = false;
}

cdp_bitmap_t::~cdp_bitmap_t()
{

	if(!m_bitmap_read_handles.empty())
	{
		close_asyncread_handles();
	}

}

bool cdp_bitmap_t::init()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s \n", FUNCTION_NAME);
	try
	{
		LocalConfigurator localConfigurator;
		m_sector_size = localConfigurator.getVxAlignmentSize();
		m_buffer_size = get_blocks_per_entry() * CDP_BM_ENTRY_SIZE;
		m_aligned_buffer.Reset(m_buffer_size, m_sector_size);
		m_buffer = (cdp_timestamp_t *)m_aligned_buffer.Get();
		m_init = true;
	} 	catch ( ContextualException& ce )
	{
		m_init = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s.\n",
			FUNCTION_NAME, ce.what());
	}
	catch( std::exception const& e )
	{
		m_init = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s.\n",
			FUNCTION_NAME, e.what());
	}
	catch ( ... )
	{
		m_init = false;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered unknown exception.\n",
			FUNCTION_NAME);
	}

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s \n", FUNCTION_NAME);
	return m_init;
}

bool cdp_bitmap_t::get_ts(const unsigned long long & offset, cdp_timestamp_t & ts)
{
	bool rv = true;

	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s offset:" ULLSPEC " \n", 
		FUNCTION_NAME, offset);

	do
	{

		if(!m_init)
		{
			DebugPrintf(SV_LOG_ERROR, "Function:%s not yet initialized. \n",
				FUNCTION_NAME);
			rv = false;
			break;
		}

		memset(&ts, 0, sizeof(cdp_timestamp_t));
		unsigned long long block_no = offset / get_block_size();
		unsigned long long starting_block = (block_no/get_blocks_per_entry()) * get_blocks_per_entry();

		if(!is_caching_enabled())
		{
			rv = get_ts_from_disk_nocaching(block_no, ts);
			break;
		}

		cdp_bmkey_t bmkey(starting_block, get_blocks_per_entry());

		cdp_bmentries_t::iterator bmiter = m_bmentries.find(bmkey);
		if(bmiter == m_bmentries.end())
		{
			rv = get_ts_from_disk_withcaching(block_no, ts);
			break;

		}else
		{
			if(!read_ts_from_cache(block_no, ts))
			{
				rv = false;
				break;
			}
		}

	} while (0);

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s offset:" ULLSPEC " \n", 
		FUNCTION_NAME, offset);

	return rv;
}

bool cdp_bitmap_t::read_ts_from_cache(const unsigned long long &block_no,
									  cdp_timestamp_t & ts)
{
	bool rv = true;
	bool found = false;

	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s block_no:" ULLSPEC " \n", 
		FUNCTION_NAME, block_no);

	do
	{

		unsigned long long starting_block = (block_no/get_blocks_per_entry()) * get_blocks_per_entry();

		cdp_bmkey_t bmkey(starting_block, get_blocks_per_entry());
		cdp_bmentries_t::iterator bmiter = m_bmentries.find(bmkey);

		if(bmiter == m_bmentries.end())
		{
			DebugPrintf(SV_LOG_ERROR, 
				"requested block: " ULLSPEC " is not available in cache.\n",
				starting_block);
			rv = false;
			break;
		}

		cdp_bmentries_byts_t & bmentries_by_ts = bmiter -> second;

		cdp_bmentries_byts_t::iterator bmts_iter = bmentries_by_ts.begin();
		for ( ; bmts_iter != bmentries_by_ts.end(); ++bmts_iter)
		{
			cdp_bm_set_realblocks_t::iterator bm_realblocks_iter = bmts_iter -> second.begin();
			while (bm_realblocks_iter != bmts_iter -> second.end())
			{
				if (block_no >= (bmkey.block_start + bm_realblocks_iter -> relative_block_no) && 
					block_no < (bmkey.block_start + bm_realblocks_iter -> relative_block_no + bm_realblocks_iter -> block_count))
				{
					ts  = bmts_iter -> first;
					rv = true;
					found = true;
					break;
				}

				++bm_realblocks_iter;
			}

			if(found)
				break;
		}

		if(!found)
		{
			memset(&ts, 0, sizeof(cdp_timestamp_t));
			rv = true;
		}

	} while(0);

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s block_no:" ULLSPEC " \n", 
		FUNCTION_NAME, block_no);

	return rv;
}


bool cdp_bitmap_t::get_ts_from_disk_nocaching(const unsigned long long &block_no, cdp_timestamp_t & ts)
{
	bool rv = true;

	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s block:" ULLSPEC " \n", 
		FUNCTION_NAME, block_no);

	do
	{
		std::string filename;

		filename = get_bitmap_filename(block_no);
		rv = read_bmentry_from_disk(filename, block_no, ts);
		break;

	} while (0);

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s block:" ULLSPEC " \n", 
		FUNCTION_NAME, block_no);

	return rv;
}

std::string cdp_bitmap_t::get_bitmap_filename(const unsigned long long & block_no)
{
	unsigned long long first_block = (block_no / CDP_BM_BLOCKS_PER_SEGMENTED_BITMAP) * CDP_BM_BLOCKS_PER_SEGMENTED_BITMAP;
	unsigned long long last_block = (((block_no / CDP_BM_BLOCKS_PER_SEGMENTED_BITMAP) + 1) * CDP_BM_BLOCKS_PER_SEGMENTED_BITMAP) - 1;

	std::stringstream fname_str;
	fname_str << m_path
		<< ACE_DIRECTORY_SEPARATOR_CHAR_A 
		<< CDPV3_BM_PREFIX
		<< first_block
		<< "_"
		<< last_block
		<< CDPV3_BM_SUFFIX;

	return fname_str.str();
}

bool cdp_bitmap_t::read_bmentry_from_disk(std::string const & filename, 
										  const unsigned long long & block_no,
										  cdp_timestamp_t & ts)
{

	bool rv = true;
	ACE_HANDLE handle = ACE_INVALID_HANDLE;
	unsigned long long first_block = 0;
	ACE_LOFF_T read_offset = 0;

	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s filename:%s block:" ULLSPEC " \n", 
		FUNCTION_NAME, filename.c_str(), block_no);

	do
	{
		rv = create_bitmap_file_ifneeded(filename, CDP_BM_FILE_SIZE);
		if(!rv)
			break;

		int mode = O_RDONLY;
		if((handle = ACE_OS::open(getLongPathName(filename.c_str()).c_str(), mode)) == ACE_INVALID_HANDLE)
		{
			DebugPrintf(SV_LOG_ERROR, "Function: %s open %s failed. error:%s \n", 
				FUNCTION_NAME, filename.c_str(), ACE_OS::strerror(ACE_OS::last_error()) );
			rv = false;
			break;
		}

		// first block in the file
		first_block = (block_no / CDP_BM_BLOCKS_PER_SEGMENTED_BITMAP) * CDP_BM_BLOCKS_PER_SEGMENTED_BITMAP;

		// offset from where to read 
		read_offset = (block_no - first_block) * CDP_BM_ENTRY_SIZE;

		if(ACE_OS::pread(handle, &ts, CDP_BM_ENTRY_SIZE, read_offset) < 0)
		{
			std::ostringstream l_stderr;

			l_stderr   << "Error detected  " << "in Function:" << FUNCTION_NAME 
				<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
				<< "Error while reading from " << filename  << "\n"
				<< "offset: " << read_offset
				<< "Error Code: "    << ACE_OS::last_error() << "\n"
				<< "Error Message: " << ACE_OS::strerror(ACE_OS::last_error()) << "\n"
				<< USER_DEFAULT_ACTION_MSG << "\n";

			DebugPrintf(SV_LOG_ERROR,"%s\n", l_stderr.str().c_str());
			rv = false;
			break;
		}

		rv = true;
		m_ioreads++;

	} while (0);

	if(handle != ACE_INVALID_HANDLE)
	{
		if(ACE_OS::close(handle) < 0)
		{
			DebugPrintf(SV_LOG_ERROR, 
				"error %s while closing file %s\n", 
				ACE_OS::strerror(ACE_OS::last_error()), filename.c_str());
			rv = false;
		}
	}

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s filename:%s block:" ULLSPEC " \n", 
		FUNCTION_NAME, filename.c_str(), block_no);

	return rv;
}

bool cdp_bitmap_t::create_bitmap_file_ifneeded(const std::string &filename, const unsigned long &size)
{
	bool rv = true;
	ACE_HANDLE handle = ACE_INVALID_HANDLE;

	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s. filename:%s size:%lu \n",
		FUNCTION_NAME, filename.c_str(), size);

	do 
	{
		ACE_stat statbuf;
		if(0 == sv_stat(getLongPathName(filename.c_str()).c_str(), &statbuf))
		{
			rv = true;
			break;
		}

		int mode = O_RDWR | O_CREAT;
		if(is_directio_enabled())
		{
			setdirectmode(mode);
		}

		setumask();

		//Setting appropriate permission on retention file
		mode_t perms;
		setsharemode_for_all(perms);

		handle = ACE_OS::open(getLongPathName(filename.c_str()).c_str(), mode,perms);

		if(handle == ACE_INVALID_HANDLE)
		{
			DebugPrintf(SV_LOG_ERROR,
				"cdp bitmap file %s creation failed. error:%d \n",
				filename.c_str(), ACE_OS::last_error());

			rv = false;
			break;
		}

#ifdef SV_WINDOWS
		if(is_compression_enabled())
		{
			SV_USHORT compression_format = COMPRESSION_FORMAT_DEFAULT;
			SV_ULONG unused_var = 0;
			DeviceIoControl(handle,
				FSCTL_SET_COMPRESSION,
				&compression_format,
				sizeof(SV_USHORT),
				NULL,
				0,
				&unused_var,
				NULL);
		}
#endif


		ACE_OS::last_error(0);
		if( ACE_OS::ftruncate(handle, size) == -1)
		{
			DebugPrintf(SV_LOG_ERROR,
				"cdp bitmap file %s expansion failed. error:%d \n", 
				filename.c_str(), ACE_OS::last_error());
			rv = false;
			break;
		}

		if(!is_directio_enabled())
		{
			if(ACE_OS::fsync(handle) < 0)
			{
				std::ostringstream l_stderr;

				l_stderr   << "Error detected  " << "in Function:" << FUNCTION_NAME 
					<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
					<< "Error while flushing buffers for " << filename  << "\n"
					<< "Error Code: "    << ACE_OS::last_error() << "\n"
					<< "Error Message: " << ACE_OS::strerror(ACE_OS::last_error()) << "\n"
					<< USER_DEFAULT_ACTION_MSG << "\n";

				DebugPrintf(SV_LOG_ERROR,"%s\n", l_stderr.str().c_str());
				rv = false;
				break;
			}
		}

		DebugPrintf(SV_LOG_DEBUG,
			"cdp bitmap file %s of size %lu MB created succesfully\n",
			filename.c_str(), size/1048576);
		rv = true;


	} while (0);

	if(handle != ACE_INVALID_HANDLE)
	{
		ACE_OS::close(handle);
	}

	if(!rv)
		ACE_OS::unlink(getLongPathName(filename.c_str()).c_str());

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s. filename:%s size:%lu \n",
		FUNCTION_NAME, filename.c_str(), size);
	return rv;
}

bool cdp_bitmap_t::get_ts_from_disk_withcaching(const unsigned long long &block_no, cdp_timestamp_t & ts)
{
	bool rv = true;
	std::string filename;
	unsigned long long starting_block = 0;

	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s block:" ULLSPEC " \n", 
		FUNCTION_NAME, block_no);

	do
	{

		filename = get_bitmap_filename(block_no);

		if(!read_bmentries_from_disk(filename, block_no, starting_block, m_buffer))
		{
			rv = false;
			break;
		}

		ts = m_buffer[block_no - starting_block];

		cdp_bmkey_t bmkey(starting_block, get_blocks_per_entry());
		cdp_bmentries_byts_t bmentries_by_ts;

		unsigned short mem_required = 0;
		unsigned short  block_idx = 0;
		while (block_idx < get_blocks_per_entry())
		{
			// skip blocks which were never written
			while ( ( block_idx < get_blocks_per_entry())
				&& (m_buffer[block_idx].year == 0) )
				block_idx++;

			if(block_idx == get_blocks_per_entry())
				break;

			unsigned short relative_blk_no = block_idx;
			while ( (block_idx < get_blocks_per_entry()) && (m_buffer[relative_blk_no] == m_buffer[block_idx]))
			{
				block_idx++;
			}

			cdp_bm_realblocks_t real_blks(relative_blk_no, block_idx - relative_blk_no);
			if(!insert_realblock(bmentries_by_ts[m_buffer[relative_blk_no]], real_blks))
			{
				rv = false;
				break;
			}
		}

		if(!rv)
			break;

		mem_required = bmentries_by_ts.size() * sizeof (std::pair<cdp_timestamp_t, cdp_bm_set_realblocks_t>);
		mem_required += sizeof (std::pair<cdp_bmkey_t, cdp_bmentries_byts_t>);

		cdp_bmentries_byts_t::iterator ts_iter = bmentries_by_ts.begin();
		while (ts_iter != bmentries_by_ts.end())
		{
			mem_required += (ts_iter -> second.size() * sizeof (cdp_bm_realblocks_t));
			++ts_iter;
		}

		m_bmentries.insert(std::make_pair(bmkey, bmentries_by_ts));
		m_memconsumed += mem_required;
		rv = true;

	} while (0);

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s block:" ULLSPEC " \n", 
		FUNCTION_NAME, block_no);

	return rv;
}

bool cdp_bitmap_t::read_bmentries_from_disk(const std::string &filename, 
											const unsigned long long &block_no, 
											unsigned long long &starting_block, 
											cdp_timestamp_t * buffer)
{

	bool rv = true;
	ACE_HANDLE handle = ACE_INVALID_HANDLE;
	unsigned long long first_block = 0;
	ACE_LOFF_T starting_offset = 0;
	unsigned short count = get_blocks_per_entry();

	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s filename:%s block:" ULLSPEC " count:%d \n", 
		FUNCTION_NAME, filename.c_str(), block_no, count);

	do
	{
		rv = create_bitmap_file_ifneeded(filename, CDP_BM_FILE_SIZE);
		if(!rv)
			break;

		int mode = O_RDONLY;
		if(is_directio_enabled())
		{
			setdirectmode(mode);
		}

		setumask();

		//Setting appropriate permission on retention file
		mode_t perms;
		setsharemode_for_all(perms);

		if((handle = ACE_OS::open(getLongPathName(filename.c_str()).c_str(), mode,perms)) == ACE_INVALID_HANDLE)
		{
			DebugPrintf(SV_LOG_ERROR, "Function: %s open %s failed. error:%s \n", 
				FUNCTION_NAME, filename.c_str(), ACE_OS::strerror(ACE_OS::last_error()) );
			rv = false;
			break;
		}

		// first block in the file
		first_block = (block_no / CDP_BM_BLOCKS_PER_SEGMENTED_BITMAP) * CDP_BM_BLOCKS_PER_SEGMENTED_BITMAP;

		// starting block from where to read
		starting_block = ( block_no / get_blocks_per_entry()) * get_blocks_per_entry();
		starting_offset = (starting_block - first_block) * CDP_BM_ENTRY_SIZE;


		if(ACE_OS::pread(handle, buffer, CDP_BM_ENTRY_SIZE * count, starting_offset) < 0)
		{
			std::ostringstream l_stderr;

			l_stderr   << "Error detected  " << "in Function:" << FUNCTION_NAME 
				<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
				<< "Error while reading from " << filename  << "\n"
				<< "offset: " << starting_offset
				<< "Error Code: "    << ACE_OS::last_error() << "\n"
				<< "Error Message: " << ACE_OS::strerror(ACE_OS::last_error()) << "\n"
				<< USER_DEFAULT_ACTION_MSG << "\n";

			DebugPrintf(SV_LOG_ERROR,"%s\n", l_stderr.str().c_str());
			rv = false;
			break;
		}

		rv = true;
		++m_ioreads;

	} while (0);

	if(handle != ACE_INVALID_HANDLE)
	{
		if(ACE_OS::close(handle) < 0)
		{
			DebugPrintf(SV_LOG_ERROR, 
				"error %s while closing file %s\n", 
				ACE_OS::strerror(ACE_OS::last_error()), filename.c_str());
			rv = false;
		}
	}

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s filename:%s block:" ULLSPEC " count:%d \n", 
		FUNCTION_NAME, filename.c_str(), block_no, count);

	return rv;
}

bool cdp_bitmap_t::insert_realblock(cdp_bm_set_realblocks_t & real_blks_set, 
									const cdp_bm_realblocks_t & real_blks)
{
	bool rv = true;
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s real_blks.relative_block_no:%d count:%d \n", 
		FUNCTION_NAME, real_blks.relative_block_no, real_blks.block_count);


	do
	{
		if(!real_blks.block_count)
		{
			rv = true;
			break;
		}

		std::pair<cdp_bm_set_realblocks_t::iterator, bool> realblkset_iter;
		realblkset_iter = real_blks_set.insert(real_blks);

		// could not add to set as it is collides with existing entries (non-unique)
		if(!realblkset_iter.second)
		{
			cdp_bm_set_realblocks_t::iterator it = realblkset_iter.first;

			// case: existing entry contains the new one
			if(it ->relative_block_no <= real_blks.relative_block_no 
				&& it ->relative_block_no  + it ->block_count >= real_blks.relative_block_no + real_blks.block_count)
			{
				rv = true;
				break;
			}

			// case: new one contains existing entry
			if(it ->relative_block_no > real_blks.relative_block_no 
				&& it ->relative_block_no + it ->block_count <= real_blks.relative_block_no + real_blks.block_count)
			{
				real_blks_set.erase(it);
				rv = insert_realblock(real_blks_set, real_blks);
				break;
			}

			// case: overlap, get the new block range
			cdp_bm_realblocks_t extended_blocks = (real_blks.relative_block_no < (it ->relative_block_no)) ? real_blks : *it;
			extended_blocks.block_count = extended_blocks.block_count + abs(( it->relative_block_no + it->block_count) - (real_blks.relative_block_no + real_blks.block_count));
			real_blks_set.erase(it);
			rv = insert_realblock(real_blks_set, extended_blocks);
			break;
		}

	} while(0);

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s real_blks.relative_block_no:%d count:%d \n", 
		FUNCTION_NAME, real_blks.relative_block_no, real_blks.block_count);

	return rv;
}

cdp_bmentries_t::iterator cdp_bitmap_t::get_farthest_bmentry(const unsigned long long & block_no)
{
	unsigned long long block_to_free = 0;
	cdp_bmentries_t::iterator rv_iter = m_bmentries.end();

	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s block_no:" ULLSPEC " \n", 
		FUNCTION_NAME, block_no);

	do
	{
		if (m_bmentries.size() <= 1)
		{
			rv_iter = m_bmentries.end();
			break;
		}

		unsigned long distFromBeg = 0 , distFromEnd = 0;
		cdp_bmentries_t::iterator fwditer = m_bmentries.begin();
		cdp_bmentries_t::reverse_iterator reviter = m_bmentries.rbegin();

		while(1)
		{
			if(fwditer == m_bmentries.end())
				break;

			cdp_bm_dirtykey_t search_key(fwditer ->first.block_start, get_blocks_per_entry());
			if(m_dirty_keys.find(search_key) == m_dirty_keys.end())
				break;

			++fwditer;
		}

		while(1)
		{
			if(reviter == m_bmentries.rend())
				break;

			cdp_bm_dirtykey_t search_key(reviter ->first.block_start, get_blocks_per_entry());
			if(m_dirty_keys.find(search_key) == m_dirty_keys.end())
				break;

			++reviter;
		}

		if(fwditer != m_bmentries.end())
			distFromBeg = abs(long(block_no - fwditer ->first.block_start));
		if(reviter != m_bmentries.rend())
			distFromEnd = abs(long(block_no - (reviter->first.block_start)));

		if (distFromBeg > distFromEnd)
		{
			block_to_free  = fwditer ->first.block_start;
			cdp_bmkey_t key_to_free(block_to_free, get_blocks_per_entry());
			rv_iter = m_bmentries.find(key_to_free);
		}else
		{
			if(distFromEnd != 0)
			{
				block_to_free  = reviter ->first.block_start;
				cdp_bmkey_t key_to_free(block_to_free, get_blocks_per_entry());
				rv_iter = m_bmentries.find(key_to_free);
			}
		}


	} while(0);

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s block_no:" ULLSPEC " \n", 
		FUNCTION_NAME, block_no);

	return rv_iter;
}

bool cdp_bitmap_t::bm_flush(const cdp_bmkey_t & bmkey)
{
	bool rv = true;
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s block_no:" ULLSPEC " \n", 
		FUNCTION_NAME, bmkey.block_start);

	do
	{
		cdp_bm_dirtykey_t dirty_bmkey(bmkey.block_start, bmkey.block_count);
		cdp_dirty_bmkeys_t::iterator iter = m_dirty_keys.find(dirty_bmkey);;
		if (iter == m_dirty_keys.end())
		{
			rv = true;
			break;
		}

		if(!read_bmentries_from_cache(bmkey.block_start, m_buffer))
		{
			rv = false;
			break;
		}

		if (!write_bmentries_to_disk(bmkey.block_start, get_blocks_per_entry(), m_buffer))
		{
			rv = false;
			break;
		}

		++m_iowrites;
		++m_iowrites_dueto_insufficient_memory;

		m_dirty_keys.erase(iter);
		rv = true;

	} while (0);

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s block_no:" ULLSPEC " \n", 
		FUNCTION_NAME, bmkey.block_start);

	return rv;
}


bool cdp_bitmap_t::read_bmentries_from_cache(const unsigned long long &starting_block,
											 cdp_timestamp_t * buffer)
{
	bool rv = true;
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s block_no:" ULLSPEC " \n", 
		FUNCTION_NAME, starting_block);

	do
	{

		cdp_bmkey_t bmkey(starting_block, get_blocks_per_entry());
		cdp_bmentries_t::iterator bmiter = m_bmentries.find(bmkey);

		if(bmiter == m_bmentries.end())
		{
			DebugPrintf(SV_LOG_ERROR, 
				"requested block: " ULLSPEC " is not available in cache.\n",
				starting_block);
			rv = false;
			break;
		}

		memset(buffer,0, m_buffer_size);
		cdp_bmentries_byts_t & bmentries_by_ts = bmiter -> second;

		cdp_bmentries_byts_t::iterator bmts_iter = bmentries_by_ts.begin();
		for ( ; bmts_iter != bmentries_by_ts.end(); ++bmts_iter)
		{
			cdp_bm_set_realblocks_t::iterator bm_realblocks_iter = bmts_iter -> second.begin();
			while (bm_realblocks_iter != bmts_iter -> second.end())
			{

				unsigned short count = bm_realblocks_iter -> relative_block_no + bm_realblocks_iter -> block_count;
				for (unsigned short i = bm_realblocks_iter ->relative_block_no; i < count ; ++i)
					buffer[i] = bmts_iter -> first;

				++bm_realblocks_iter;
			}
		}

	} while(0);

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s block_no:" ULLSPEC " \n", 
		FUNCTION_NAME, starting_block);

	return rv;
}

bool cdp_bitmap_t::write_bmentries_to_disk(const unsigned long long & block_no, 
										   unsigned int count, 
										   cdp_timestamp_t * buffer)
{
	bool rv = true;
	std::string filename;
	ACE_HANDLE handle = ACE_INVALID_HANDLE;
	unsigned long long first_block = 0;
	ACE_LOFF_T write_offset = 0;

	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s block_no:" ULLSPEC " count:%d \n", 
		FUNCTION_NAME, block_no, count);

	do
	{

		filename = get_bitmap_filename(block_no);
		rv = create_bitmap_file_ifneeded(filename, CDP_BM_FILE_SIZE);
		if(!rv)
			break;

		int mode = O_WRONLY;
		if(is_directio_enabled())
		{
			setdirectmode(mode);
		}

		setumask();

		//Setting appropriate permission on retention file
		mode_t perms;
		setsharemode_for_all(perms);

		if((handle = ACE_OS::open(getLongPathName(filename.c_str()).c_str(), mode,perms)) == ACE_INVALID_HANDLE)
		{
			DebugPrintf(SV_LOG_ERROR, "Function: %s open %s failed. error:%s \n", 
				FUNCTION_NAME, filename.c_str(), ACE_OS::strerror(ACE_OS::last_error()) );
			rv = false;
			break;
		}

		// first block in the file
		first_block = (block_no / CDP_BM_BLOCKS_PER_SEGMENTED_BITMAP) * CDP_BM_BLOCKS_PER_SEGMENTED_BITMAP;

		// starting offset where to write
		write_offset = (block_no - first_block) * CDP_BM_ENTRY_SIZE;


		if(ACE_OS::pwrite(handle, buffer, CDP_BM_ENTRY_SIZE * count, write_offset) < 0)
		{
			std::ostringstream l_stderr;

			l_stderr   << "Error detected  " << "in Function:" << FUNCTION_NAME 
				<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
				<< "Error while writing to " << filename  << "\n"
				<< "offset: " << write_offset
				<< "Error Code: "    << ACE_OS::last_error() << "\n"
				<< "Error Message: " << ACE_OS::strerror(ACE_OS::last_error()) << "\n"
				<< USER_DEFAULT_ACTION_MSG << "\n";

			DebugPrintf(SV_LOG_ERROR,"%s\n", l_stderr.str().c_str());
			rv = false;
			break;
		}

		if(!is_directio_enabled())
		{
			if(ACE_OS::fsync(handle) < 0)
			{
				std::ostringstream l_stderr;

				l_stderr   << "Error detected  " << "in Function:" << FUNCTION_NAME 
					<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
					<< "Error while flushing buffers for " << filename  << "\n"
					<< "offset: " << write_offset
					<< "Error Code: "    << ACE_OS::last_error() << "\n"
					<< "Error Message: " << ACE_OS::strerror(ACE_OS::last_error()) << "\n"
					<< USER_DEFAULT_ACTION_MSG << "\n";

				DebugPrintf(SV_LOG_ERROR,"%s\n", l_stderr.str().c_str());
				rv = false;
				break;
			}
		}


		rv = true;
		++m_iowrites;

	} while(0);

	if(handle != ACE_INVALID_HANDLE)
	{
		if(ACE_OS::close(handle) < 0)
		{
			DebugPrintf(SV_LOG_ERROR, 
				"error %s while closing file %s\n", 
				ACE_OS::strerror(ACE_OS::last_error()), filename.c_str());
			rv = false;
		}
	}


	DebugPrintf(SV_LOG_DEBUG, "EXITED %s block_no:" ULLSPEC " count:%d \n", 
		FUNCTION_NAME, block_no, count);

	return rv;
}

bool cdp_bitmap_t::set_ts(const unsigned long long &offset, const cdp_timestamp_t & ts)
{
	bool rv = true;

	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s offset:" ULLSPEC " \n", 
		FUNCTION_NAME, offset);

	do
	{


		if(!m_init)
		{
			DebugPrintf(SV_LOG_ERROR, "Function:%s not yet initialized. \n",
				FUNCTION_NAME);
			rv = false;
			break;
		}

		unsigned long long block_no = offset / get_block_size();
		unsigned long long starting_block = (block_no/get_blocks_per_entry()) * get_blocks_per_entry();

		if(!is_caching_enabled())
		{
			rv = set_ts_to_disk_nocaching(block_no, ts);
			break;
		}

		cdp_bmkey_t bmkey(starting_block, get_blocks_per_entry());

		cdp_bmentries_t::iterator bmiter = m_bmentries.find(bmkey);
		if(bmiter == m_bmentries.end())
		{
			rv = set_ts_to_disk_nocaching(block_no, ts);
			break;
		}else
		{
			if(!update_ts_in_cache(block_no, ts))
			{
				rv = false;
				break;
			}
		}

	} while (0);

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s offset:" ULLSPEC " \n", 
		FUNCTION_NAME, offset);

	return rv;
}

bool cdp_bitmap_t::set_ts_to_disk_nocaching(const unsigned long long &block_no, const cdp_timestamp_t& ts)
{
	bool rv = true;

	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s block:" ULLSPEC " \n", 
		FUNCTION_NAME, block_no);

	do
	{
		std::string filename;

		filename = get_bitmap_filename(block_no);
		rv = write_bmentry_to_disk(filename, block_no, ts);
		break;

	} while (0);

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s block:" ULLSPEC " \n", 
		FUNCTION_NAME, block_no);

	return rv;
}


bool cdp_bitmap_t::write_bmentry_to_disk(std::string const & filename, 
										 const unsigned long long & block_no,
										 const cdp_timestamp_t & ts)
{

	bool rv = true;
	ACE_HANDLE handle = ACE_INVALID_HANDLE;
	unsigned long long first_block = 0;
	ACE_LOFF_T write_offset = 0;

	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s filename:%s block:" ULLSPEC " \n", 
		FUNCTION_NAME, filename.c_str(), block_no);

	do
	{
		rv = create_bitmap_file_ifneeded(filename, CDP_BM_FILE_SIZE);
		if(!rv)
			break;

		int mode = O_WRONLY;

		setumask();

		//Setting appropriate permission on retention file
		mode_t perms;
		setsharemode_for_all(perms);

		if((handle = ACE_OS::open(getLongPathName(filename.c_str()).c_str(), mode,perms)) == ACE_INVALID_HANDLE)
		{
			DebugPrintf(SV_LOG_ERROR, "Function: %s open %s failed. error:%s \n", 
				FUNCTION_NAME, filename.c_str(), ACE_OS::strerror(ACE_OS::last_error()) );
			rv = false;
			break;
		}

		// first block in the file
		first_block = (block_no / CDP_BM_BLOCKS_PER_SEGMENTED_BITMAP) * CDP_BM_BLOCKS_PER_SEGMENTED_BITMAP;

		// offset from where to read 
		write_offset = (block_no - first_block) * CDP_BM_ENTRY_SIZE;

		if(ACE_OS::pwrite(handle, &ts, CDP_BM_ENTRY_SIZE, write_offset) < 0)
		{
			std::ostringstream l_stderr;

			l_stderr   << "Error detected  " << "in Function:" << FUNCTION_NAME 
				<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
				<< "Error while writing to " << filename  << "\n"
				<< "offset: " << write_offset
				<< "Error Code: "    << ACE_OS::last_error() << "\n"
				<< "Error Message: " << ACE_OS::strerror(ACE_OS::last_error()) << "\n"
				<< USER_DEFAULT_ACTION_MSG << "\n";

			DebugPrintf(SV_LOG_ERROR,"%s\n", l_stderr.str().c_str());
			rv = false;
			break;
		}

		if(ACE_OS::fsync(handle) < 0)
		{
			std::ostringstream l_stderr;

			l_stderr   << "Error detected  " << "in Function:" << FUNCTION_NAME 
				<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
				<< "Error while flushing buffers for " << filename  << "\n"
				<< "offset: " << write_offset
				<< "Error Code: "    << ACE_OS::last_error() << "\n"
				<< "Error Message: " << ACE_OS::strerror(ACE_OS::last_error()) << "\n"
				<< USER_DEFAULT_ACTION_MSG << "\n";

			DebugPrintf(SV_LOG_ERROR,"%s\n", l_stderr.str().c_str());
			rv = false;
			break;
		}

		rv = true;
		m_iowrites++;

	} while (0);

	if(handle != ACE_INVALID_HANDLE)
	{
		if(ACE_OS::close(handle) < 0)
		{
			DebugPrintf(SV_LOG_ERROR, 
				"error %s while closing file %s\n", 
				ACE_OS::strerror(ACE_OS::last_error()), filename.c_str());
			rv = false;
		}
	}

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s filename:%s block:" ULLSPEC " \n", 
		FUNCTION_NAME, filename.c_str(), block_no);

	return rv;
}

bool cdp_bitmap_t::update_ts_in_cache(const unsigned long long &block_no, 
									  const cdp_timestamp_t& setts)
{
	bool rv = true;
	bool found = false;

	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s block:" ULLSPEC " \n", 
		FUNCTION_NAME, block_no);

	do
	{
		unsigned long long first_block = (block_no/get_blocks_per_entry()) * get_blocks_per_entry();
		cdp_timestamp_t ts;

		cdp_bmkey_t bmkey(first_block, get_blocks_per_entry());
		cdp_bm_dirtykey_t dirty_bmkey(first_block, get_blocks_per_entry());
		cdp_bm_realblocks_t real_blks_to_add(block_no - first_block, 1);

		cdp_bmentries_t::iterator bmiter = m_bmentries.find(bmkey);
		if(bmiter == m_bmentries.end())
		{
			DebugPrintf(SV_LOG_ERROR, 
				"requested block: " ULLSPEC " is not available in cache.\n",
				first_block);
			rv = false;
			break;
		}

		cdp_bmentries_byts_t & bmentries_by_ts = bmiter -> second;
		cdp_bmentries_byts_t::iterator bmts_iter = bmentries_by_ts.begin();
		for ( ; bmts_iter != bmentries_by_ts.end(); ++bmts_iter)
		{
			cdp_bm_set_realblocks_t::iterator bm_realblocks_iter = bmts_iter -> second.begin();
			while (bm_realblocks_iter != bmts_iter -> second.end())
			{
				if (block_no >= (bmkey.block_start + bm_realblocks_iter -> relative_block_no) && 
					block_no < (bmkey.block_start + bm_realblocks_iter -> relative_block_no + bm_realblocks_iter -> block_count))
				{
					found = true;
					ts  = bmts_iter -> first;

					// no change in timestamp
					if(setts == ts)
					{
						rv = true;
						break;
					}

					// timestamp changed, no other blocks effected
					// overwrite it by inserting new ts entry with real_blks_to_add
					// erase previous one
					if (bm_realblocks_iter ->block_count == 1)
					{

						cdp_bm_set_realblocks_t &real_blks_set = bmentries_by_ts[setts];
						int real_blks_count = real_blks_set.size();

						if(!real_blks_count)
						{
							m_memconsumed += sizeof(std::pair<cdp_timestamp_t, cdp_bm_set_realblocks_t>);
						}

						if(!insert_realblock(real_blks_set, real_blks_to_add))
						{
							rv = false;
							break;
						}

						m_memconsumed += (real_blks_set.size() - real_blks_count) * (int)sizeof (cdp_bm_realblocks_t);

						if(!insert_into_dirtybitmap(dirty_bmkey))
						{
							rv = false;
							break;
						}

						bmts_iter ->second.erase(bm_realblocks_iter);
						m_memconsumed -= sizeof (cdp_bm_realblocks_t);
						if(bmts_iter ->second.empty())
						{
							bmentries_by_ts.erase(bmts_iter);
							m_memconsumed -= sizeof(std::pair<cdp_timestamp_t, cdp_bm_set_realblocks_t>);
						}

						rv = true;
						break;
					}

					// timestamp changed, there were adjacent blocks
					unsigned short relative_blk_no = block_no - first_block ;
					cdp_bm_realblocks_t rblk1 (bm_realblocks_iter ->relative_block_no, relative_blk_no - bm_realblocks_iter ->relative_block_no);
					cdp_bm_realblocks_t rblk2 (relative_blk_no, 1);
					cdp_bm_realblocks_t rblk3 (relative_blk_no +1, bm_realblocks_iter ->block_count - (relative_blk_no - bm_realblocks_iter ->relative_block_no + 1));

					// delete the original one from set.
					bmts_iter ->second.erase(bm_realblocks_iter);
					m_memconsumed -= sizeof (cdp_bm_realblocks_t);

					// insert all three in to set
					if(rblk1.block_count)
					{
						int real_blks_count = bmts_iter ->second.size();
						if(!insert_realblock(bmts_iter ->second, rblk1))
						{
							rv = false;
							break;
						}
						m_memconsumed += (bmts_iter ->second.size() - real_blks_count) * (int)sizeof (cdp_bm_realblocks_t);
					}

					cdp_bm_set_realblocks_t &real_blks_set = bmentries_by_ts[setts];
					int real_blks_count = real_blks_set.size();

					if(!real_blks_count)
					{
						m_memconsumed += sizeof(std::pair<cdp_timestamp_t, cdp_bm_set_realblocks_t>);
					}

					if(!insert_realblock(real_blks_set, rblk2))
					{
						rv = false;
						break;
					}
					m_memconsumed += (real_blks_set.size() - real_blks_count) * (int)sizeof (cdp_bm_realblocks_t);

					if(rblk3.block_count)
					{
						int real_blks_count = bmts_iter ->second.size();
						if(!insert_realblock(bmts_iter ->second,rblk3))
						{
							rv = false;
							break;
						}
						m_memconsumed += (bmts_iter ->second.size() - real_blks_count) * (int)sizeof (cdp_bm_realblocks_t);
					}

					if(!insert_into_dirtybitmap(dirty_bmkey))
					{
						rv = false;
						break;
					}

					rv = true;
					break;
				}

				++bm_realblocks_iter;
			}

			if(found)
				break;
		}

		// block is written for first time
		if(!found)
		{
			cdp_bm_set_realblocks_t &real_blks_set = bmentries_by_ts[setts];
			int real_blks_count = real_blks_set.size();
			if(!real_blks_count)
			{
				m_memconsumed += sizeof(std::pair<cdp_timestamp_t, cdp_bm_set_realblocks_t>);
			}

			if(!insert_realblock(real_blks_set, real_blks_to_add))
			{
				rv = false;
				break;
			}


			if(!insert_into_dirtybitmap(dirty_bmkey))
			{
				rv = false;
				break;
			}

			m_memconsumed += (real_blks_set.size() - real_blks_count) * (int)sizeof (cdp_bm_realblocks_t);
			rv = true;
			break;
		}

	} while (0);

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s block:" ULLSPEC " \n", 
		FUNCTION_NAME, block_no);

	return rv;
}


bool cdp_bitmap_t::get_and_set_ts(const unsigned long long & offset, 
								  const cdp_timestamp_t & setts, 
								  cdp_timestamp_t &getts)
{
	bool rv = true;

	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s offset:" ULLSPEC " \n", 
		FUNCTION_NAME, offset);

	do
	{

		if(!m_init)
		{
			DebugPrintf(SV_LOG_ERROR, "Function:%s not yet initialized. \n",
				FUNCTION_NAME);
			rv = false;
			break;
		}

		if(!get_ts(offset,getts))
		{
			rv = false;
			break;
		}

		if(!set_ts(offset,setts))
		{
			rv = false;
			break;
		}

	} while(0);

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s offset:" ULLSPEC " \n", 
		FUNCTION_NAME, offset);

	return rv;
}

bool cdp_bitmap_t::flush()
{
	bool rv = true;
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s \n", FUNCTION_NAME);

	do
	{
		if(!m_init)
		{
			DebugPrintf(SV_LOG_ERROR, "Function:%s not yet initialized. \n",
				FUNCTION_NAME);
			rv = false;
			break;
		}

		if(m_dirty_keys.empty())
		{
			rv = true;
			break;
		}

		if(is_asyncio_enabled())
		{
			rv = write_async_bmentries();
		} else
		{
			rv = write_sync_bmentries();
		}

		DebugPrintf(SV_LOG_DEBUG,"MEMUSAGE: %s - m_memconsumed %d\n",FUNCTION_NAME,m_memconsumed);
		if(m_memconsumed < 0)
		{
			DebugPrintf(SV_LOG_ERROR, "MEMUSAGE: m_memconsumed %d, resetting to zero\n",m_memconsumed);
			m_memconsumed = 0;
		}

		if(m_memconsumed > get_cache_size())
		{
			//dump_bmdata_from_cache_to_log();
			free_bmdata_from_cache();
		}

		if(rv)
		{
			m_dirty_keys.clear();
		}

	} while (0);


	DebugPrintf(SV_LOG_DEBUG, "EXITED %s \n", FUNCTION_NAME);
	return rv;
}

// ============================================
// resource management routines
// ============================================

bool cdp_bitmap_t::canissue_nextio()
{
	bool rv = true;
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);


	do
	{
		unsigned long pendingios = m_pendingios;
		if(error())
		{
			rv = false;
			break;
		}

		if(m_pendingios < m_maxpendingios)
		{
			rv = true;
			break;
		} else
		{
			iocount_mutex.acquire();
			while( !error() && (m_pendingios >= m_maxpendingios))
			{
				iocount_cv.wait();
			}

			iocount_mutex.release();

			if(error())
			{
				rv = false;
				break;
			}
		}

	} while (0);

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s \n",FUNCTION_NAME);
	return rv;
}

void cdp_bitmap_t::increment_pending_ios()
{
	iocount_mutex.acquire();
	++m_pendingios;
	iocount_mutex.release();
}

void cdp_bitmap_t::decrement_pending_ios()
{
	iocount_mutex.acquire();
	--m_pendingios;
	iocount_mutex.release();
	if( m_pendingios <= m_minpendingios)
		iocount_cv.signal();
}

// ==========================================
// io routines
// ==========================================

// ==========================================
// asynchronous flush
// ==========================================

bool cdp_bitmap_t::write_async_bmentries()
{
	bool rv = true;
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s()\n", FUNCTION_NAME);

	do
	{
		cdp_dirty_bmkeys_citer_t dirty_keys_iter = m_dirty_keys.begin();


		while(dirty_keys_iter != m_dirty_keys.end())
		{
			SV_UINT max_memory = std::max(m_maxmemory_for_io, m_buffer_size);
			SV_UINT mem_required = 0;

			// get dirty blocks to write based on memory available
			cdp_dirty_bmkeys_citer_t bmentries_to_write_begin;
			cdp_dirty_bmkeys_citer_t bmentries_to_write_end;

			if(!get_bmentries_limited_by_memory(dirty_keys_iter, 
				m_dirty_keys.end(), 
				max_memory, 
				bmentries_to_write_begin,
				bmentries_to_write_end,
				mem_required) )
			{
				rv = false;
				break;
			}


			// allocate the sector aligned buffer of required length
			SharedAlignedBuffer pbuffer(mem_required, m_sector_size);
			if(!read_bmentries_from_cache(pbuffer.Get(),
				bmentries_to_write_begin,bmentries_to_write_end))
			{
				rv = false;
				break;
			}

			if(!write_async_bmentries(pbuffer.Get(),bmentries_to_write_begin, 
				bmentries_to_write_end))
			{
				rv = false;
				break;
			}

			dirty_keys_iter = bmentries_to_write_end;
		}

	} while (0);

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s()\n", FUNCTION_NAME);
	return rv;
}

bool cdp_bitmap_t::get_bmentries_limited_by_memory(const cdp_dirty_bmkeys_citer_t & start, 
												   const cdp_dirty_bmkeys_citer_t & end, 
												   const SV_UINT & max_memory, 
												   cdp_dirty_bmkeys_citer_t & out_bmentries_start,
												   cdp_dirty_bmkeys_citer_t & out_bmentries_end, 
												   SV_UINT &mem_required)
{
	bool rv = true;
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);


	mem_required = 0;
	cdp_dirty_bmkeys_citer_t tmpiter = start;
	out_bmentries_start = tmpiter;

	for( ; tmpiter != end ; ++tmpiter)
	{
		if(mem_required + (tmpiter ->block_count * CDP_BM_ENTRY_SIZE) > max_memory)
		{
			break;
		}          

		mem_required += (tmpiter ->block_count * CDP_BM_ENTRY_SIZE);
	}

	out_bmentries_end = tmpiter;

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}

bool cdp_bitmap_t::read_bmentries_from_cache(char * bm_data,
											 cdp_dirty_bmkeys_citer_t & start,
											 cdp_dirty_bmkeys_citer_t & end)
{
	bool rv = true;
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	SV_UINT bytes_read = 0;
	unsigned long long block_no = 0;
	cdp_dirty_bmkeys_citer_t bm_iter = start;


	for( ; bm_iter != end ; ++bm_iter)
	{
		unsigned long long block_no = bm_iter->block_start;

		if(!read_bmentries_from_cache(block_no,
			(cdp_timestamp_t *)((char*)bm_data + bytes_read)))
		{
			rv = false;
			break;
		}

		bytes_read += (bm_iter->block_count * CDP_BM_ENTRY_SIZE);
	}


	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}

bool cdp_bitmap_t::write_async_bmentries(char * bm_data,
										 const cdp_dirty_bmkeys_citer_t & batch_start, 
										 const cdp_dirty_bmkeys_citer_t & batch_end)
{
	bool rv = true;
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s batch write\n",FUNCTION_NAME);

	std::string bm_filepath;
	SV_UINT bm_length_consumed =0;	
	ACE_HANDLE bm_handle = ACE_INVALID_HANDLE;
	ACE_Asynch_Write_File_Ptr wf_ptr;

	cdp_dirty_bmkeys_citer_t bm_iter = batch_start;
	cdp_dirty_bmkeys_citer_t bm_samefile_iter;
	cdp_dirty_bmkeys_citer_t bm_samefile_end;

	while(bm_iter != batch_end)
	{

		bm_filepath.clear();

		if(!get_bmentries_belonging_to_same_file(bm_iter,batch_end,
			bm_samefile_iter,
			bm_samefile_end,
			bm_filepath))
		{
			rv = false;
			break;
		}

		int mode = O_CREAT |  O_WRONLY | FILE_FLAG_OVERLAPPED;
		if(is_directio_enabled())
		{
			setdirectmode(mode);
		}

		setumask();

		//Setting appropriate permission on retention file
		mode_t perms;
		setsharemode_for_all(perms);

		if((bm_handle = ACE_OS::open(getLongPathName(bm_filepath.c_str()).c_str(), mode,perms)) == ACE_INVALID_HANDLE)
		{
			DebugPrintf(SV_LOG_ERROR, "Function: %s open %s failed. errno:%d \n", 
				FUNCTION_NAME, bm_filepath.c_str(), ACE_OS::last_error());
			rv = false;
			break;
		}

		wf_ptr.reset(new ACE_Asynch_Write_File);

		if(wf_ptr ->open (*this, bm_handle) == -1)
		{
			DebugPrintf(SV_LOG_ERROR, "asynch open %s failed. error no:%d\n",
				bm_filepath.c_str(), ACE_OS::last_error());
			ACE_OS::close(bm_handle);
			bm_handle = ACE_INVALID_HANDLE;
			rv = false;
			break;
		}

		if(!write_async_bmentries(wf_ptr, bm_filepath,bm_data + bm_length_consumed,
			bm_samefile_iter, bm_samefile_end,
			bm_length_consumed))
		{
			rv = false;
			break;
		}
#ifndef SV_WINDOWS
		if(!is_directio_enabled())
		{
#endif
			if(!flush_io(bm_handle, bm_filepath))
			{
				ACE_OS::close(bm_handle);
				bm_handle = ACE_INVALID_HANDLE;
				rv = false;
				break;
			}
#ifndef SV_WINDOWS
		}
#endif

		ACE_OS::close(bm_handle);
		bm_handle = ACE_INVALID_HANDLE;
		wf_ptr.reset();

		bm_iter = bm_samefile_end; 
	}


	DebugPrintf(SV_LOG_DEBUG,"EXITED %s batch_write\n",FUNCTION_NAME);
	return rv;
}

bool cdp_bitmap_t::get_bmentries_belonging_to_same_file(const cdp_dirty_bmkeys_citer_t & start,
														const cdp_dirty_bmkeys_citer_t &end,
														cdp_dirty_bmkeys_citer_t &bmentries_samefile_begin,
														cdp_dirty_bmkeys_citer_t &bmentries_samefile_end,
														std::string & bm_filepath)
{
	bool rv = true;
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	do
	{
		std::string curr_bm_file;
		if(start == end)
		{
			rv = false;
			break;
		}

		bm_filepath.clear();
		bmentries_samefile_begin = start;
		bmentries_samefile_end = start;

		bm_filepath = get_bitmap_filename(bmentries_samefile_begin ->block_start);

		++bmentries_samefile_end;
		while (bmentries_samefile_end != end)
		{

			curr_bm_file = get_bitmap_filename(bmentries_samefile_end ->block_start);
			if(curr_bm_file != bm_filepath)
			{
				break;
			}

			++bmentries_samefile_end;
		}


		if(!rv)
			break;
	}while (0);

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}

bool cdp_bitmap_t::write_async_bmentries(ACE_Asynch_Write_File_Ptr &asynch_handle,
										 const std::string & bm_filepath,
										 char * bm_data,
										 const cdp_dirty_bmkeys_citer_t & batch_start, 
										 const cdp_dirty_bmkeys_citer_t & batch_end,
										 SV_UINT & bm_length_consumed)
{
	bool rv = true;
	unsigned long long first_block = 0;
	unsigned long long block_no = 0;
	unsigned int length_written = 0;
	SV_OFFSET_TYPE write_offset = 0;

	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s filepath:%s \n",
		FUNCTION_NAME, bm_filepath.c_str());

	cdp_dirty_bmkeys_citer_t batch_iter = batch_start;

	for( ;batch_iter != batch_end; ++batch_iter)
	{
		block_no = batch_iter ->block_start;

		if(!canissue_nextio())
		{
			rv = false;
			break;
		}

		// first block in the file
		first_block = (block_no / CDP_BM_BLOCKS_PER_SEGMENTED_BITMAP) * CDP_BM_BLOCKS_PER_SEGMENTED_BITMAP;

		// starting offset where to write
		write_offset = (block_no - first_block) * CDP_BM_ENTRY_SIZE;

		if(!initiate_next_async_write(asynch_handle, 
			bm_filepath, bm_data + length_written,
			m_buffer_size, write_offset))
		{
			rv = false;
			break;
		}

		length_written += m_buffer_size;
		bm_length_consumed += m_buffer_size;
	}

	if(!wait_for_xfr_completion())
		rv = false;


	DebugPrintf(SV_LOG_DEBUG,"EXITED %s filepath:%s \n",
		FUNCTION_NAME, bm_filepath.c_str());
	return rv;
}

bool cdp_bitmap_t::initiate_next_async_write(ACE_Asynch_Write_File_Ptr &asynch_handle, 
											 const std::string & bm_filepath,
											 const char * buffer,
											 const SV_UINT &length,
											 const SV_OFFSET_TYPE& offset)
{
	bool rv = true;
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s on %s offset: " LLSPEC " length %d\n",
		FUNCTION_NAME, bm_filepath.c_str(), offset, length);

	do
	{		

		ACE_Message_Block *mb = new ACE_Message_Block (buffer, length);
		if(!mb)
		{
			std::stringstream l_stderr;
			l_stderr   << "memory allocation(message block) failed "
				<< "while initiating write on " << bm_filepath << ".\n";

			DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
			m_error = true;
			rv = false;
			break;
		}

		mb ->wr_ptr(length);
		increment_pending_ios();
		if(asynch_handle -> write(*mb,length, 
			(unsigned int)(offset), 
			(unsigned int)(offset >> 32)) == -1)
		{
            int errorcode = ACE_OS::last_error();
			std::stringstream l_stderr;
			l_stderr << "Asynch write request for " << bm_filepath
				<< "at offset " << offset << " failed. error no: "
				<< errorcode << std::endl;
			DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());

            RetentionFileWriteFailureAlert a(bm_filepath, offset, length, errorcode);
            SendAlertToCx(SV_ALERT_CRITICAL, SV_ALERT_MODULE_RETENTION, a);

			decrement_pending_ios();
			m_error = true;
			rv = false;
			break;
		}

	} while (0);

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s on %s offset: " LLSPEC " length %d\n",
		FUNCTION_NAME, bm_filepath.c_str(), offset, length);
	return rv;
}

bool cdp_bitmap_t::wait_for_xfr_completion()
{
	bool rv = true;
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	iocount_mutex.acquire();

	while(m_pendingios > 0)
		iocount_cv.wait();

	iocount_mutex.release();
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}

bool cdp_bitmap_t::flush_io(ACE_HANDLE bm_handle, const std::string & bm_filepath)
{
	bool rv = true;   

	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s filename:%s\n",
		FUNCTION_NAME,bm_filepath.c_str());

	//
	// flush is applicable only if
	// the handle was opened in buffered mode
	//
	if(!is_directio_enabled())
	{
		if(ACE_OS::fsync(bm_handle) == -1)
		{
			DebugPrintf(SV_LOG_ERROR, 
				"Flush buffers failed for %s with error %d.\n",
				bm_filepath.c_str(),
				ACE_OS::last_error());
			rv = false;
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s filename:%s\n",
		FUNCTION_NAME,bm_filepath.c_str());
	return rv;
}

void cdp_bitmap_t::handle_write_file (const ACE_Asynch_Write_File::Result &result)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	try
	{
		do
		{

			// release message block.
			result.message_block ().release ();
			decrement_pending_ios();

			if(error())
				break;

			unsigned long long offset = 
				((unsigned long long)(result.offset_high()) << 32) + (unsigned long long)(result.offset());

			if (result.success ())
			{
				if (result.bytes_transferred () != result.bytes_to_write())
				{
					m_error = true;
					std::stringstream l_stderr;
					l_stderr << "bitmap async write at offset "
						<< offset
						<< " failed with error " 
						<<  result.error()  
						<< "bytes transferred =" << result.bytes_transferred ()
						<< "bytes requested =" << result.bytes_to_write()
						<< "\n";
					DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
					break;
				}

			} else
			{
				m_error = true;
				std::stringstream l_stderr;
				l_stderr << "bitmap async write at "
					<< offset
					<< " failed with error " 
					<<  result.error() << "\n";
				DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
				break;
			}

		} while (0);
	}
	catch ( ContextualException& ce )
	{
		m_error = true;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s.\n",
			FUNCTION_NAME, ce.what());
	}
	catch( std::exception const& e )
	{
		m_error = true;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s.\n",
			FUNCTION_NAME, e.what());
	}
	catch ( ... )
	{
		m_error = true;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered unknown exception.\n",
			FUNCTION_NAME);
	}

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}




// ==========================================
// synchronous flush
// ==========================================


bool cdp_bitmap_t::write_sync_bmentries()
{
	bool rv = true;
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s \n", FUNCTION_NAME);

	do
	{
		cdp_dirty_bmkeys_citer_t dirty_keys_iter = m_dirty_keys.begin();


		while(dirty_keys_iter != m_dirty_keys.end())
		{
			SV_UINT max_memory = std::max(m_maxmemory_for_io,m_buffer_size);
			SV_UINT mem_required = 0;

			// get dirty blocks to write based on memory available
			cdp_dirty_bmkeys_citer_t bmentries_to_write_begin;
			cdp_dirty_bmkeys_citer_t bmentries_to_write_end;

			if(!get_bmentries_limited_by_memory(dirty_keys_iter, 
				m_dirty_keys.end(), 
				max_memory, 
				bmentries_to_write_begin,
				bmentries_to_write_end,
				mem_required) )
			{
				rv = false;
				break;
			}


			// allocate the sector aligned buffer of required length
			SharedAlignedBuffer pbuffer(mem_required, m_sector_size);
			if(!read_bmentries_from_cache(pbuffer.Get(),
				bmentries_to_write_begin,bmentries_to_write_end))
			{
				rv = false;
				break;
			}

			if(!write_sync_bmentries(pbuffer.Get(),bmentries_to_write_begin, 
				bmentries_to_write_end))
			{
				rv = false;
				break;
			}

			dirty_keys_iter = bmentries_to_write_end;
		}

	} while (0);

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s \n", FUNCTION_NAME);
	return rv;
}

bool cdp_bitmap_t::write_sync_bmentries(char * bm_data,
										const cdp_dirty_bmkeys_citer_t & batch_start, 
										const cdp_dirty_bmkeys_citer_t & batch_end)
{
	bool rv = true;
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s batch write\n",FUNCTION_NAME);

	std::string bm_filepath;
	SV_UINT bm_length_consumed =0;	
	ACE_HANDLE bm_handle = ACE_INVALID_HANDLE;

	cdp_dirty_bmkeys_citer_t bm_iter = batch_start;
	cdp_dirty_bmkeys_citer_t bm_samefile_iter;
	cdp_dirty_bmkeys_citer_t bm_samefile_end;

	while(bm_iter != batch_end)
	{

		bm_filepath.clear();

		if(!get_bmentries_belonging_to_same_file(bm_iter,batch_end,
			bm_samefile_iter,
			bm_samefile_end,
			bm_filepath))
		{
			rv = false;
			break;
		}

		int mode = O_CREAT |  O_WRONLY ;
		if(is_directio_enabled())
		{
			setdirectmode(mode);
		}

		setumask();

		//Setting appropriate permission on retention file
		mode_t perms;
		setsharemode_for_all(perms);

		if((bm_handle = ACE_OS::open(getLongPathName(bm_filepath.c_str()).c_str(), mode,perms)) == ACE_INVALID_HANDLE)
		{
			DebugPrintf(SV_LOG_ERROR, "Function: %s open %s failed. errno:%d \n", 
				FUNCTION_NAME, bm_filepath.c_str(), ACE_OS::last_error());
			rv = false;
			break;
		}


		if(!write_sync_bmentries(bm_handle, bm_filepath,bm_data + bm_length_consumed,
			bm_samefile_iter, bm_samefile_end,
			bm_length_consumed))
		{
			rv = false;
			break;
		}

		if(!is_directio_enabled())
		{
			if(!flush_io(bm_handle, bm_filepath))
			{
				ACE_OS::close(bm_handle);
				bm_handle = ACE_INVALID_HANDLE;
				rv = false;
				break;
			}
		}

		ACE_OS::close(bm_handle);
		bm_handle = ACE_INVALID_HANDLE;

		bm_iter = bm_samefile_end; 
	}


	DebugPrintf(SV_LOG_DEBUG,"EXITED %s batch_write\n",FUNCTION_NAME);
	return rv;
}

bool cdp_bitmap_t::write_sync_bmentries(ACE_HANDLE & bm_handle,
										const std::string & bm_filepath,
										char * bm_data,
										const cdp_dirty_bmkeys_citer_t & batch_start, 
										const cdp_dirty_bmkeys_citer_t & batch_end,
										SV_UINT & bm_length_consumed)
{
	bool rv = true;
	unsigned long long first_block = 0;
	unsigned long long block_no = 0;
	unsigned int length_written = 0;
	SV_OFFSET_TYPE write_offset = 0;

	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s filepath:%s \n",
		FUNCTION_NAME, bm_filepath.c_str());

	cdp_dirty_bmkeys_citer_t batch_iter = batch_start;

	for( ;batch_iter != batch_end; ++batch_iter)
	{
		block_no = batch_iter ->block_start;

		// first block in the file
		first_block = (block_no / CDP_BM_BLOCKS_PER_SEGMENTED_BITMAP) * CDP_BM_BLOCKS_PER_SEGMENTED_BITMAP;

		// starting offset where to write
		write_offset = (block_no - first_block) * CDP_BM_ENTRY_SIZE;

		if(ACE_OS::pwrite(bm_handle, bm_data + length_written, m_buffer_size, write_offset) < 0)
		{
			std::ostringstream l_stderr;

			l_stderr   << "Error detected  " << "in Function:" << FUNCTION_NAME 
				<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
				<< "Error while writing to " << bm_filepath  << "\n"
				<< "offset: " << write_offset
				<< "Error Code: "    << ACE_OS::last_error() << "\n"
				<< "Error Message: " << ACE_OS::strerror(ACE_OS::last_error()) << "\n"
				<< USER_DEFAULT_ACTION_MSG << "\n";

			DebugPrintf(SV_LOG_ERROR,"%s\n", l_stderr.str().c_str());
			rv = false;
			break;
		}

		length_written += m_buffer_size;
		bm_length_consumed += m_buffer_size;
	}

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s filepath:%s \n",
		FUNCTION_NAME, bm_filepath.c_str());
	return rv;
}

// ======================================================================================
// Bitmap read optimizations.
// 1) A list of cdp_offset_change_ts_t structures is passed, the structur has offset,
//     oldtime(output) and new time stamp(current time).
// 2) From the given offsets, for all the repeating offsets the last modified time will be
//     current hr.
// 3) Generate a unique list of blocks to be read and issue a async read if the block 
//    is not in thecache.
// 4) After read completes, get and set timestamps for all the unique offsets 
//    for which a read is issued.
// 5) If the cache usage is high, flush dirty blocks based on the unique offsets which 
//     we read.
// ====================================================================================

bool cdp_bitmap_t::get_and_set_ts(cdp_change_offset_ts_list_t & offsets)
{
	//For each offset find the block number it corresponds to.
	//Add it to the 'to be fetched list' if it is not already added.
	bool rv = true;
	read_blocks_t read_blocks;
	cdp_change_offset_ts_list_t::iterator it = offsets.begin();
	std::set<unsigned long long> unique_offsets;
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s \n",FUNCTION_NAME);
	if(m_asyncio)
	{

		for(; it != offsets.end(); it++)
		{
			if(unique_offsets.find(it->offset) == unique_offsets.end())
			{
				unique_offsets.insert(it->offset);
			}
			else //This is repeating offset. The getts will be equal to the current hr.
			{
				it->repeating = true;
				it->getts = it->setts; //Current hr, as this is a repeating offset.
			}
		}

		std::set<unsigned long long>::iterator unique_offsets_iter = unique_offsets.begin();
		read_blocks.clear();
		for(; unique_offsets_iter != unique_offsets.end(); unique_offsets_iter++)
		{
			unsigned long long block_no = (*unique_offsets_iter) / get_block_size();
			unsigned long long starting_block = (block_no/get_blocks_per_entry()) * get_blocks_per_entry();
			cdp_bmkey_t bmkey(starting_block, get_blocks_per_entry());

			cdp_bmentries_t::iterator bmiter = m_bmentries.find(bmkey);

			/*if(read_blocks.find(block_no) == read_blocks.end() && bmiter == m_bmentries.end())
			{
			read_blocks.insert(block_no);
			}*/
			if(read_blocks.find(starting_block) == read_blocks.end() && bmiter ==  m_bmentries.end())
			{
				read_blocks.insert(starting_block);
			}
		}

		read_blocks_iter_t read_blocks_iterator = read_blocks.begin();

		while(read_blocks_iterator != read_blocks.end())
		{
			read_blocks_iter_t begin_iter;
			read_blocks_iter_t end_iter;
			SV_UINT mem_required = 0;
			SV_UINT length_read = 0;
			if(get_read_blocks_limited_by_memory(read_blocks_iterator,read_blocks.end(),m_maxmemory_for_io,begin_iter,end_iter,mem_required))
			{
				SharedAlignedBuffer pbuffer(mem_required, SV_SECTOR_SIZE);
				if(!read_async_bmentries(pbuffer.Get(),begin_iter,end_iter,length_read))
				{
					DebugPrintf(SV_LOG_DEBUG,"read_async_bmentries failed\n");
					rv = false;
					break;
				}
				//Add entries to bm cache.
				if(!add_bmdata_to_cache(begin_iter, end_iter, pbuffer.Get()))
				{
					DebugPrintf(SV_LOG_DEBUG,"add_bmdata_to_cache failed\n");
					rv = false;
					break;
				}
				read_blocks_iterator = end_iter;
			}
			else
			{
				DebugPrintf(SV_LOG_DEBUG,"get_read_blocks_limited_by_memory failed\n");
				rv = false;
				break;
			}
		}

		//After read is complete, get and set timestamp to the cached blocks
		for(it = offsets.begin(); it != offsets.end(); it++)
		{
			if(!it->repeating)
			{
				if(!get_ts(it->offset,it->getts)) //TODO : Should evaluate to avoid this.
				{
					DebugPrintf(SV_LOG_DEBUG,"Failed to retreive last modified time for offset " ULLSPEC,it->offset);
					rv = false;
					break;
				}
				if(!set_ts(it->offset,it->setts))
				{
					DebugPrintf(SV_LOG_DEBUG,"Failed to set modified time for offset " ULLSPEC,it->offset);
					rv = false;
					break;
				}
			}
		}

	}
	else
	{
		//After read is complete, get and set timestamp to the cached blocks
		for(it = offsets.begin(); it != offsets.end(); it++)
		{
			if(!get_ts(it->offset,it->getts)) 
			{
				DebugPrintf(SV_LOG_DEBUG,"Failed to retreive last modified time for offset " ULLSPEC,it->offset);
				rv = false;
				break;
			}
			if(!set_ts(it->offset,it->setts))
			{
				DebugPrintf(SV_LOG_DEBUG,"Failed to set modified time for offset " ULLSPEC,it->offset);
				rv = false;
				break;
			}
		}
	}

	if(m_asyncio)
	{
		if(!close_asyncread_handles())
		{
			rv = false;
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"EXITING %s \n",FUNCTION_NAME);
	return rv;
}

bool cdp_bitmap_t::add_bmdata_to_cache(read_blocks_iter_t start, read_blocks_iter_t end, char * buffer)
{
	bool rv = true;
	unsigned long long block_no;
	unsigned long long starting_block;
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s \n",FUNCTION_NAME);

	read_blocks_iter_t tempitr = start;
	cdp_timestamp_t * cdp_ts = (cdp_timestamp_t *)buffer;
	SV_UINT block_idx = 0;
	SV_UINT mem_required = 0;

	for(; tempitr!=end; tempitr++)
	{
		block_no = *tempitr;
		starting_block = (block_no/get_blocks_per_entry()) * get_blocks_per_entry();
		cdp_bmkey_t bmkey(starting_block, get_blocks_per_entry());

		cdp_bmentries_byts_t bmentries_by_ts;

		while (block_idx < get_blocks_per_entry())
		{
			// skip blocks which were never written
			while ( ( block_idx < get_blocks_per_entry())
				&& (cdp_ts[block_idx].year == 0) )
				block_idx++;

			if(block_idx == get_blocks_per_entry())
				break;

			unsigned short relative_blk_no = block_idx;
			while ( (block_idx < get_blocks_per_entry()) && (cdp_ts[relative_blk_no] == cdp_ts[block_idx]))
			{
				block_idx++;
			}

			cdp_bm_realblocks_t real_blks(relative_blk_no, block_idx - relative_blk_no);
			if(!insert_realblock(bmentries_by_ts[cdp_ts[relative_blk_no]], real_blks))
			{
				rv = false;
				break;
			}
		}

		mem_required = bmentries_by_ts.size() *  sizeof (std::pair<cdp_timestamp_t, cdp_bm_set_realblocks_t>);
		mem_required += sizeof (std::pair<cdp_bmkey_t, cdp_bmentries_byts_t>);

		cdp_bmentries_byts_t::iterator ts_iter = bmentries_by_ts.begin();
		while (ts_iter != bmentries_by_ts.end())
		{
			mem_required += (ts_iter -> second.size() * sizeof (cdp_bm_realblocks_t));
			++ts_iter;
		}
		//Add to the total mem_consumed.
		m_bmentries.insert(std::make_pair(bmkey, bmentries_by_ts));
		m_memconsumed +=mem_required;
	}
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}

bool cdp_bitmap_t::get_read_blocks_limited_by_memory(const read_blocks_iter_t & start, 
													 const read_blocks_iter_t & end, 
													 const SV_UINT & max_memory, 
													 read_blocks_iter_t & out_readblocks_start,
													 read_blocks_iter_t & out_readblocks_end, 
													 SV_UINT &mem_required)
{
	bool rv = true;
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	mem_required = 0;
	read_blocks_iter_t tmpiter = start;
	out_readblocks_start = tmpiter;

	for( ; tmpiter != end ; ++tmpiter)
	{
		if(mem_required + (get_blocks_per_entry() * CDP_BM_ENTRY_SIZE) > max_memory)
		{
			break;
		}          

		mem_required += (get_blocks_per_entry() * CDP_BM_ENTRY_SIZE); 
	}

	out_readblocks_end = tmpiter;

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}


bool cdp_bitmap_t::read_async_bmentries(char * buffer,
										const read_blocks_iter_t & batch_start, 
										const read_blocks_iter_t & batch_end,
										SV_UINT & bm_length_read)
{
	bool rv = true;
	unsigned long long first_block = 0;
	unsigned long long starting_block = 0;
	unsigned long long block_no = 0;
	unsigned int length_read = 0;
	SV_OFFSET_TYPE read_offset = 0;
	unsigned int rd_length = CDP_BM_ENTRY_SIZE * get_blocks_per_entry();
	std::string filename;
	ACE_Asynch_Read_File_Ptr asynch_handle;
	read_blocks_iter_t batch_iter = batch_start;

	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s \n",FUNCTION_NAME);


	for( ;batch_iter != batch_end; batch_iter++)
	{
		block_no = *batch_iter;

		if(!get_bitmap_filehandle(block_no,asynch_handle,filename))
		{
			rv = false;
			break;
		}

		if(!canissue_nextio())
		{
			rv = false;
			break;
		}

		// first block in the file
		first_block = (block_no / CDP_BM_BLOCKS_PER_SEGMENTED_BITMAP) * CDP_BM_BLOCKS_PER_SEGMENTED_BITMAP;

		// starting block from where to read
		starting_block = ( block_no / get_blocks_per_entry()) * get_blocks_per_entry();
		read_offset = (starting_block - first_block) * CDP_BM_ENTRY_SIZE;

		if(!initiate_next_async_read(asynch_handle, 
			filename, buffer + length_read,
			rd_length, read_offset))
		{
			rv = false;
			break;
		}

		length_read += rd_length;
		bm_length_read += rd_length;
	}

	if(!wait_for_xfr_completion())
		rv = false;

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s \n",FUNCTION_NAME);
	return rv;
}

bool cdp_bitmap_t::get_bitmap_filehandle(const unsigned long long &block_no, ACE_Asynch_Read_File_Ptr &rf_ptr,std::string &filename)
{
	bool rv = false;
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	//Get file name from the block number.
	filename = get_bitmap_filename(block_no);
	cdp_bitmap_asyncreadhandles_t::iterator it = m_bitmap_read_handles.find(filename);
	ACE_HANDLE handle = ACE_INVALID_HANDLE;
	if(it == m_bitmap_read_handles.end())
	{
		//Create bitmap file if required.
		rv = create_bitmap_file_ifneeded(filename,CDP_BM_FILE_SIZE);
		if(rv)
		{
			do
			{
				int openMode = O_CREAT | O_RDONLY | FILE_FLAG_OVERLAPPED;
				if(is_directio_enabled())
				{
					setdirectmode(openMode);
				}

				setumask();
				//Setting appropriate share on retention file
				mode_t share;
				setsharemode_for_all(share);

				// PR#10815: Long Path support
				handle = ACE_OS::open(getLongPathName(filename.c_str()).c_str(),
					openMode, share);

				if(ACE_INVALID_HANDLE == handle)
				{
					DebugPrintf(SV_LOG_ERROR, "open %s failed. error no:%d\n",
						filename.c_str(), ACE_OS::last_error());
					rv = false;
					break;
				}

				rf_ptr.reset(new ACE_Asynch_Read_File);

				if(rf_ptr ->open (*this, handle) == -1)
				{
					DebugPrintf(SV_LOG_ERROR, "asynch open %s failed. error no:%d\n",
						filename.c_str(), ACE_OS::last_error());
					ACE_OS::close(handle);
					handle = ACE_INVALID_HANDLE;
					rv = false;
					break;
				}

				cdp_bitmap_asyncreadhandle_t value(handle, rf_ptr);
				m_bitmap_read_handles.insert(std::make_pair(filename, value));

			}while(0);
		}
	}
	else
	{
		rf_ptr = it-> second.asynch_handle;
		rv = true;
	}
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}

bool cdp_bitmap_t::initiate_next_async_read(ACE_Asynch_Read_File_Ptr &asynch_handle, 
											std::string bm_filepath, char * buffer,	const SV_UINT &length,const SV_OFFSET_TYPE& offset)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s on %s offset: " LLSPEC " length %d\n",
		FUNCTION_NAME, bm_filepath.c_str(),offset, length);
	bool rv = true;

	do
	{		

		ACE_Message_Block *mb = new ACE_Message_Block (buffer, length);
		if(!mb)
		{
			std::stringstream l_stderr;
			l_stderr   << "memory allocation(message block) failed "
				<< "while initiating read on " << bm_filepath << ".\n";

			DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
			m_error = true;
			rv = false;
			break;
		}

		mb ->rd_ptr(length);
		increment_pending_ios();
		if(asynch_handle -> read(*mb,length, 
			(unsigned int)(offset), 
			(unsigned int)(offset >> 32)) == -1)
		{
            int errorcode = ACE_OS::last_error();
			std::stringstream l_stderr;
			l_stderr << "Asynch read request for " << bm_filepath
				<< "at offset " << offset << " failed. error no: "
				<< errorcode << std::endl;
			DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());

            RetentionFileReadFailureAlert a(bm_filepath, offset, length, errorcode);
            SendAlertToCx(SV_ALERT_CRITICAL, SV_ALERT_MODULE_RETENTION, a);

			decrement_pending_ios();
			m_error = true;
			rv = false;
			break;
		}

	} while (0);

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s on %s offset: " LLSPEC " length %d\n",
		FUNCTION_NAME, bm_filepath.c_str(), offset, length);
	return rv;
}

void cdp_bitmap_t::handle_read_file (const ACE_Asynch_Read_File::Result &result)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	try
	{
		do
		{
			// release message block.
			result.message_block ().release ();
			decrement_pending_ios();

			if(error())
				break;

			unsigned long long offset = 
				((unsigned long long)(result.offset_high()) << 32) + (unsigned long long)(result.offset());

			if (result.success ())
			{
				if (result.bytes_transferred () != result.bytes_to_read())
				{
					m_error = true;
					std::stringstream l_stderr;
					l_stderr << "bitmap async read at offset "
						<< offset
						<< " failed with error " 
						<<  result.error()  
						<< "bytes transferred =" << result.bytes_transferred ()
						<< "bytes requested =" << result.bytes_to_read()
						<< "\n";
					DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
					break;
				}

			} else
			{
				m_error = true;
				std::stringstream l_stderr;
				l_stderr << "bitmap async write at "
					<< offset
					<< " failed with error " 
					<<  result.error() << "\n";
				DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
				break;
			}

		} while (0);
	}
	catch ( ContextualException& ce )
	{
		m_error = true;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s.\n",
			FUNCTION_NAME, ce.what());
	}
	catch( std::exception const& e )
	{
		m_error = true;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered exception %s.\n",
			FUNCTION_NAME, e.what());
	}
	catch ( ... )
	{
		m_error = true;
		DebugPrintf(SV_LOG_ERROR, 
			"\n%s encountered unknown exception.\n",
			FUNCTION_NAME);
	}

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}


bool cdp_bitmap_t::close_asyncread_handles()
{
	bool rv = true;

	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	cdp_bitmap_asyncreadhandles_t::const_iterator handle_iter =  m_bitmap_read_handles.begin();
	for( ; handle_iter != m_bitmap_read_handles.end(); ++handle_iter)
	{
		cdp_bitmap_asyncreadhandle_t handle_value = handle_iter ->second;
		ACE_OS::close(handle_value.handle);
	}
	m_bitmap_read_handles.clear();
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}

//memmory consumed is greater than the configured cache size. the method finds out the farthest 
//no dirty entry in the cache and deletes the entry until we free up the excess memory used or all the elements in
//the cache are dirty(recently modified).
//1. Find the non dirty entry in the cache in the forward direction.
//2. Find the non dirty entry in the cache in the reverse direction.
//3. Find the farthest entry from the dirty blocks. delete the entry from cache.
//4. repeat the above until the required memory is freed up.

bool cdp_bitmap_t::free_bmdata_from_cache()
{
	bool rv = true;
	DebugPrintf(SV_LOG_DEBUG, "MEMUSAGE - ENTERED %s %d\n", FUNCTION_NAME,m_memconsumed);

	unsigned long long starting_dirty_block_no = 0;
	unsigned long long ending_dirty_block_no = 0;

	do
	{
		cdp_dirty_bmkeys_citer_t dirtykey_fwd_itr = m_dirty_keys.begin();

		//Not possible...
		if(dirtykey_fwd_itr == m_dirty_keys.end())
		{
			rv = false;
			break;
		}
		starting_dirty_block_no = dirtykey_fwd_itr->block_start;
		DebugPrintf(SV_LOG_DEBUG, "MEMUSAGE starting_dirty_block_no " ULLSPEC "\n", starting_dirty_block_no);

		cdp_dirty_bmkeys_creviter_t dirtykey_rev_itr = m_dirty_keys.rbegin();

		//Not possible...
		if(dirtykey_rev_itr == m_dirty_keys.rend())
		{
			rv = false;
			break;
		}

		ending_dirty_block_no = dirtykey_rev_itr->block_start;
		DebugPrintf(SV_LOG_DEBUG, "MEMUSAGE ending_dirty_block_no " ULLSPEC "\n", ending_dirty_block_no);

		if(m_memconsumed < get_cache_size())
		{
			rv = true;
			break;
		}

		cdp_bmentries_t::iterator fwditer = m_bmentries.begin();
		cdp_bmentries_t::reverse_iterator reviter = m_bmentries.rbegin();

		unsigned int mem_freed  = 0;
		unsigned int mem_to_be_freed = m_memconsumed - get_cache_size();
		std::vector<unsigned long long> blocks_to_free;

		while(mem_freed < mem_to_be_freed
			&& (fwditer != m_bmentries.end()) 
			&& (reviter != m_bmentries.rend()))
		{

			unsigned long long block_to_free = 0;
			unsigned long long distFromBeg = 0 , distFromEnd = 0;

			while(fwditer != m_bmentries.end())
			{

				cdp_bm_dirtykey_t search_key(fwditer ->first.block_start, get_blocks_per_entry());
				DebugPrintf(SV_LOG_DEBUG, "MEMUSAGE search_key " ULLSPEC " %u\n", search_key.block_start, search_key.block_count);
				if(m_dirty_keys.find(search_key) == m_dirty_keys.end())
				{
					if(starting_dirty_block_no > fwditer ->first.block_start)
						distFromBeg = starting_dirty_block_no - fwditer ->first.block_start;
					else
						distFromBeg =  fwditer ->first.block_start - starting_dirty_block_no;

					DebugPrintf(SV_LOG_DEBUG, "MEMUSAGE fwd key " ULLSPEC " %u\n", fwditer ->first.block_start, fwditer ->first.block_count);
					DebugPrintf(SV_LOG_DEBUG, "MEMUSAGE distFromBeg " ULLSPEC " \n", distFromBeg);
					break;
				}

				++fwditer;
			}

			while(reviter != m_bmentries.rend())
			{
				cdp_bm_dirtykey_t search_key(reviter ->first.block_start, get_blocks_per_entry());
				DebugPrintf(SV_LOG_DEBUG, "MEMUSAGE search_key " ULLSPEC " %u\n", search_key.block_start, search_key.block_count);
				if(m_dirty_keys.find(search_key) == m_dirty_keys.end())
				{
					if(ending_dirty_block_no > reviter->first.block_start)
						distFromEnd = ending_dirty_block_no - reviter->first.block_start;
					else
						distFromEnd = reviter->first.block_start - ending_dirty_block_no;

					DebugPrintf(SV_LOG_DEBUG, "MEMUSAGE rev key " ULLSPEC " %u\n", reviter ->first.block_start, reviter ->first.block_count);
					DebugPrintf(SV_LOG_DEBUG, "MEMUSAGE distFromEnd: "  ULLSPEC " \n", distFromEnd);
					break;
				}

				++reviter;
			}


			if (distFromBeg && distFromBeg > distFromEnd)
			{
				block_to_free  = fwditer ->first.block_start;
				blocks_to_free.push_back(block_to_free);

				DebugPrintf(SV_LOG_DEBUG, "MEMUSAGE block_to_free " ULLSPEC "\n", block_to_free);

				mem_freed += sizeof (std::pair<cdp_bmkey_t, cdp_bmentries_byts_t>);
				cdp_bmentries_byts_t & tsentries_to_free = fwditer -> second;
				mem_freed += tsentries_to_free.size() * sizeof (std::pair<cdp_timestamp_t, cdp_bm_set_realblocks_t>);

				cdp_bmentries_byts_t::iterator ts_to_free_iter = tsentries_to_free.begin();
				while (ts_to_free_iter != tsentries_to_free.end())
				{
					mem_freed += (ts_to_free_iter ->second.size() * sizeof (cdp_bm_realblocks_t)); 
					++ts_to_free_iter;
				}

				DebugPrintf(SV_LOG_DEBUG, "MEMUSAGE mem_freed %u\n", mem_freed);


				if(reviter != m_bmentries.rend() && 
					(fwditer ->first.block_start == reviter ->first.block_start))
					++reviter;

				++fwditer;

				if(fwditer != m_bmentries.end())
				{
					DebugPrintf(SV_LOG_DEBUG, "MEMUSAGE fwditer " ULLSPEC " %u\n", 
						fwditer ->first.block_start, fwditer ->first.block_count);
				}

				if(reviter != m_bmentries.rend())
				{
					DebugPrintf(SV_LOG_DEBUG, "MEMUSAGE reviter " ULLSPEC " %u\n",
						reviter ->first.block_start, reviter ->first.block_count);
				}

			}else if(distFromEnd)
			{
				block_to_free  = reviter ->first.block_start;
				blocks_to_free.push_back(block_to_free);
				DebugPrintf(SV_LOG_DEBUG, "MEMUSAGE block_to_free " ULLSPEC "\n", block_to_free);

				mem_freed += sizeof (std::pair<cdp_bmkey_t, cdp_bmentries_byts_t>);
				cdp_bmentries_byts_t & tsentries_to_free = reviter -> second;
				mem_freed += tsentries_to_free.size() * sizeof (std::pair<cdp_timestamp_t, cdp_bm_set_realblocks_t>);

				cdp_bmentries_byts_t::iterator ts_to_free_iter = tsentries_to_free.begin();
				while (ts_to_free_iter != tsentries_to_free.end())
				{
					mem_freed += (ts_to_free_iter ->second.size() * sizeof (cdp_bm_realblocks_t)); 
					++ts_to_free_iter;
				}

				DebugPrintf(SV_LOG_DEBUG, "MEMUSAGE mem_freed %u\n", mem_freed);

				if(fwditer != m_bmentries.end() &&
					(reviter ->first.block_start == fwditer ->first.block_start))
					++fwditer;

				++reviter;

				if(fwditer != m_bmentries.end())
				{
					DebugPrintf(SV_LOG_DEBUG, "MEMUSAGE fwditer " ULLSPEC " %u\n", 
						fwditer ->first.block_start, fwditer ->first.block_count);
				}

				if(reviter != m_bmentries.rend())
				{
					DebugPrintf(SV_LOG_DEBUG, "MEMUSAGE reviter " ULLSPEC " %u\n",
						reviter ->first.block_start, reviter ->first.block_count);
				}
			}
			else
			{
				DebugPrintf(SV_LOG_DEBUG,"%s: Could not find any non dirty entry to free from cache.\n",FUNCTION_NAME);
				rv = true;
				break;
			}
		}


		std::vector<unsigned long long>::iterator blocks_to_free_iterator = blocks_to_free.begin();
		while(blocks_to_free_iterator != blocks_to_free.end())
		{
			unsigned long long block_to_free = *blocks_to_free_iterator;
			cdp_bmkey_t key_to_free(block_to_free, get_blocks_per_entry());
			m_bmentries.erase(key_to_free);
			++blocks_to_free_iterator;
		}

		m_memconsumed -= mem_freed;
		DebugPrintf(SV_LOG_DEBUG, "MEMUSAGE m_memconsumed %d\n", m_memconsumed);

	} while(0);

	DebugPrintf(SV_LOG_DEBUG, "MEMUSAGE - EXITED %s  %d\n", FUNCTION_NAME,m_memconsumed);

	return rv;
}

//Debug utility.
//Traverses the m_bmentries and dumps the data to a log file
void cdp_bitmap_t::dump_bmdata_from_cache_to_log()
{
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s \n", FUNCTION_NAME);
	cdp_bmentries_t::iterator iter = m_bmentries.begin();

	std::string log_path = m_path + ACE_DIRECTORY_SEPARATOR_CHAR_A + "bitmap_cache_data.log";
	//Check if the file is present

	//  ACE_stat statbuf;
	//  if(0 == sv_stat(getLongPathName(log_path.c_str()).c_str(), &statbuf))
	//  {
	//return;
	//  }

	std::ofstream output(log_path.c_str(), std::ios_base::app| std::ios_base::out);

	if(output.is_open())
	{
		output<<"memconused is :"<<m_memconsumed<<std::endl;
		output<<"number of bm_entries :"<<m_bmentries.size()<<std::endl;

		while(iter != m_bmentries.end())
		{
			output<<iter->first.block_start<<"-"<<iter->second.size()<<std::endl;
			cdp_bmentries_byts_t tsentries = iter->second;
			cdp_bmentries_byts_t::iterator ts_iter = tsentries.begin();
			while (ts_iter != tsentries.end())
			{
				output<<"\t"<<ts_iter->first<<"-"<<ts_iter->second.size()<<std::endl;
				cdp_bm_set_realblocks_t::iterator realblk_iter = ts_iter->second.begin();
				while(realblk_iter != ts_iter->second.end())
				{
					output<<"\t\t"<<realblk_iter->relative_block_no<<":"<<realblk_iter->block_count<<std::endl;
					realblk_iter++;
				}
				ts_iter++;
			}
			iter++;
		}
		output.flush();
		output.close();
	}
	DebugPrintf(SV_LOG_DEBUG, "EXITED %s  \n", FUNCTION_NAME);
}

std::ostream & operator <<( std::ostream & o, const cdp_timestamp_t & ts)
{
	o <<ts.hours<<":"<<ts.days<<":"<<ts.month<<":"<<ts.year<<":"<<ts.events;
	return o;
}




