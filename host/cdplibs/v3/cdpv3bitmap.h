//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : cdpv3bitmap.h
//
// Description: 
//
#ifndef CDPV3BITMAP__H
#define CDPV3BITMAP__H

#include <string>
#include <vector>
#include <map>
#include <set>
#include <list>

#include <ace/Synch.h>
#include <ace/Proactor.h>
#include <ace/Asynch_IO.h>
#include <ace/Message_Block.h>
#include <ace/Atomic_Op.h>

#include "cdpv3globals.h"
#include "sharedalignedbuffer.h"
#include "svtypes.h"
#include <boost/shared_ptr.hpp>

typedef boost::shared_ptr<ACE_Asynch_Write_File> ACE_Asynch_Write_File_Ptr;

struct cdp_timestamp_t
{
	unsigned year:   6;
	unsigned month:  4;
	unsigned days:   5;
	unsigned hours:  5;
	unsigned events: 12;

    cdp_timestamp_t()
        :year(0),month(0),days(0),hours(0),events(0)
    {
        
    }

	bool operator == (const cdp_timestamp_t & rhs) const
	{

		return (memcmp(&rhs, &(*this), sizeof (rhs)) == 0) ;
	}

	bool operator != (const cdp_timestamp_t & rhs) const
	{
		return !(*this == rhs);
	}

	bool operator < (const cdp_timestamp_t & rhs) const
	{
		// compare for year, followed by month, day and hour.

		if (this ->year < rhs.year)
			return true;
		else if (this ->year > rhs.year)
			return false;

		if (this -> month < rhs.month)
			return true;
		else if (this -> month > rhs.month)
			return false;

		if (this -> days < rhs.days)
			return true;
		else if (this -> days > rhs.days)
			return false;

		if (this -> hours < rhs.hours)
			return true;
		else if (this -> hours > rhs.hours)
			return false;

		if(this -> events < rhs.events)
			return true;
		else if (this -> events > rhs.events)
			return false;

		// they are equal
		return false;
	}
};

struct cdp_timestamp_cmp_t
{
	bool operator()(const cdp_timestamp_t & lhs, const cdp_timestamp_t & rhs) const
	{
		// compare for year, followed by month, day and hour.

		if (lhs.year < rhs.year)
			return true;
		else if (lhs.year > rhs.year)
			return false;

		if (lhs.month < rhs.month)
			return true;
		else if (lhs.month > rhs.month)
			return false;

		if (lhs.days < rhs.days)
			return true;
		else if (lhs.days > rhs.days)
			return false;

		if (lhs.hours < rhs.hours)
			return true;
		else if (lhs.hours > rhs.hours)
			return false;

		if(lhs.events < rhs.events)
			return true;
		else if (lhs.events > rhs.events)
			return false;

		// they are equal
		return false;
	}

};
std::ostream & operator <<( std::ostream & o, const cdp_timestamp_t & ts);

// 
// each bitmap key is range of blocks 
struct cdp_bmkey_t
{
	explicit cdp_bmkey_t(unsigned long long bn = 0, unsigned short count = 0)
	{
		block_start = bn;
		block_count = count;
	}

	unsigned long long block_start;
	unsigned short block_count ;
};

struct cdp_bmkey_cmp_t
{
	bool operator()(const cdp_bmkey_t & lhs, const cdp_bmkey_t & rhs) const
	{
		//if (lhs.block_start + lhs.block_count <= rhs.block_start)
		if (lhs.block_start < rhs.block_start)
			return true;
		return false;
	}
};


// within the block range only few have a timestamp associated with them
// rest of the blocks have never been written
struct  cdp_bm_realblocks_t
{
	explicit cdp_bm_realblocks_t(unsigned short rb = 0, unsigned short count = 0)
	{
		relative_block_no = rb;
		block_count = count;
	}
	unsigned short relative_block_no;
	unsigned short block_count; 
};

struct cdp_bm_realblocks_cmp_t
{
	bool operator()(const cdp_bm_realblocks_t & lhs, const cdp_bm_realblocks_t & rhs) const
	{
		if (lhs.relative_block_no + lhs.block_count < rhs.relative_block_no)
			return true;
		return false;
	}
};

typedef  std::set<cdp_bm_realblocks_t, cdp_bm_realblocks_cmp_t> cdp_bm_set_realblocks_t;
typedef std::map<cdp_timestamp_t, cdp_bm_set_realblocks_t, cdp_timestamp_cmp_t> cdp_bmentries_byts_t;
typedef std::map<cdp_bmkey_t, cdp_bmentries_byts_t, cdp_bmkey_cmp_t> cdp_bmentries_t;

// keys to be flushed to disk
struct cdp_bm_dirtykey_t
{
	explicit cdp_bm_dirtykey_t(unsigned long long bn = 0, unsigned short count = 0)
	{
		block_start = bn;
		block_count = count;
	}

	unsigned long long block_start;
	unsigned short block_count ;
};

struct cdp_bm_dirtykey_cmp_t
{
	bool operator()(const cdp_bm_dirtykey_t & lhs, const cdp_bm_dirtykey_t & rhs) const
	{
		if (lhs.block_start < rhs.block_start)
			return true;
		return false;
	}
};

typedef std::set<cdp_bm_dirtykey_t, cdp_bm_dirtykey_cmp_t> cdp_dirty_bmkeys_t;
typedef cdp_dirty_bmkeys_t::const_iterator cdp_dirty_bmkeys_citer_t;
typedef cdp_dirty_bmkeys_t::const_reverse_iterator cdp_dirty_bmkeys_creviter_t;


//Structure for offset, time stamp of last modified (output), time

struct cdp_offset_change_ts_t{
	unsigned long long offset;
	bool repeating;
	cdp_timestamp_t setts; //Current hr times stamp.
	cdp_timestamp_t getts; //Last modified time stamp for the offset.
};

typedef std::list<cdp_offset_change_ts_t > cdp_change_offset_ts_list_t; 

typedef boost::shared_ptr<ACE_Asynch_Read_File> ACE_Asynch_Read_File_Ptr;
struct cdp_bitmap_asyncreadhandle_t
{
    cdp_bitmap_asyncreadhandle_t(ACE_HANDLE &handle, ACE_Asynch_Read_File_Ptr ptr)
        :handle(handle), asynch_handle(ptr)
    {
    }

    ACE_HANDLE handle;
    ACE_Asynch_Read_File_Ptr asynch_handle;
};

typedef std::map<std::string, cdp_bitmap_asyncreadhandle_t> cdp_bitmap_asyncreadhandles_t;

typedef std::set<unsigned long long> read_blocks_t;
typedef std::set<unsigned long long>::iterator read_blocks_iter_t;

class cdp_bitmap_t : public ACE_Handler
{

public:

	cdp_bitmap_t();
	~cdp_bitmap_t();

	bool init();
	bool get_ts(const unsigned long long & offset, cdp_timestamp_t & ts);
	bool set_ts(const unsigned long long &offset, const cdp_timestamp_t & ts);
	bool get_and_set_ts(const unsigned long long & offset, const cdp_timestamp_t & setts, cdp_timestamp_t &getts);
	bool get_and_set_ts(cdp_change_offset_ts_list_t & offsets);
	bool flush();

	inline const std::string & get_path() { return m_path;}
	inline bool setpath(const std::string & path) { m_path = path; return true; }


	//
	// TODO: caching can be enabled/disabled before first call to get_ts/set_ts
	//       and not later
	// 
	inline bool is_caching_enabled() { return m_iscaching_enabled;  }
	inline void enable_caching()     { m_iscaching_enabled = true;  }
	inline void disable_caching()    { m_iscaching_enabled = false; }

	// 
	// TODO: directio can be enabled/disabled before first call to get_ts/set_ts
	//       and not later
	// Note: we did not add this to constructor because we do not
	//       have same default value across Oses
	//       for windows, we want direct i/o to be enabled
	//       for linux, it should be disabled
	//
	inline bool is_directio_enabled() { return (m_directio == true); }
	inline void enable_directio()     { m_directio = true; }
	inline void disable_directio()    { m_directio = false; }

	// 
	// TODO: block size should be intialized from configuration file 
	//       under retention (catalogue) folder. 
	//       the value in the configuration will come either from 
	//       local config or through cx
	//       for now, we are initializing using a constant defined above
	//
	inline unsigned int get_block_size()                            { return m_blocksize; }
	inline void set_block_size(unsigned int blocksize)              { m_blocksize = blocksize; }

	inline unsigned int get_blocks_per_entry()                      { return m_blocks_per_entry; }
	inline void set_blocks_per_entry(unsigned int blocks)           { m_blocks_per_entry = blocks; }

	inline bool is_compression_enabled() { return (m_compressed == true); }
	inline void enable_compression()     { m_compressed = true; }
	inline void disable_compression()    { m_compressed = false; }

	//
	// TODO: cache size can be set before first call to get_ts/set_ts
	//       and not later
	//  
	inline unsigned int get_cache_size()                            { return m_cachesize; }
	inline void  set_cache_size(unsigned int cachesize)             { m_cachesize = cachesize; }
	inline unsigned int get_mem_consumed()                          { return m_memconsumed; }

	inline bool is_asyncio_enabled() { return (m_asyncio == true); }
	inline void enable_asyncio()     { m_asyncio = true; }
	inline void disable_asyncio()    { m_asyncio = false; }

	inline unsigned int get_maxios()                                { return m_maxpendingios; }
	inline void set_maxios(unsigned int maxios)                     { m_maxpendingios = maxios; }
	inline unsigned int get_pendingios()                            { return m_pendingios; }
	
	inline unsigned int get_maxmem_forio()                          { return m_maxmemory_for_io; }
	inline void set_maxmem_forio(unsigned int maxmem)               { m_maxmemory_for_io = maxmem; }


	inline unsigned long get_iowrites()     { return m_iowrites; }
	inline void reset_iowrites()            { m_iowrites = 0;    }
	inline unsigned long get_ioreads()      { return m_ioreads;  }
	inline void reset_ioreads()             { m_ioreads = 0;     }

	inline unsigned long get_iowrites_dueto_insufficient_mem() { return m_iowrites_dueto_insufficient_memory; }
	inline void reset_iowrites_dueto_insufficient_mem()        { m_iowrites_dueto_insufficient_memory = 0; }

	inline unsigned long long get_timespent_on_io() { return m_timespent_on_io; }


private:

	bool read_ts_from_cache(const unsigned long long &block_no,cdp_timestamp_t & ts);

	bool get_ts_from_disk_nocaching(const unsigned long long &block_no, cdp_timestamp_t & ts);
	std::string get_bitmap_filename(const unsigned long long &block_no);
	bool read_bmentry_from_disk(const std::string &filename, const unsigned long long &block_no, cdp_timestamp_t &ts);
	bool create_bitmap_file_ifneeded(const std::string &filename, const unsigned long &size);

	bool get_ts_from_disk_withcaching(const unsigned long long &block_no, cdp_timestamp_t & ts);
	bool read_bmentries_from_disk(const std::string &filename, const unsigned long long &block_no, unsigned long long &starting_block, cdp_timestamp_t * buffer);
	bool insert_realblock(cdp_bm_set_realblocks_t & real_blks_set, const cdp_bm_realblocks_t & real_blks);
	cdp_bmentries_t::iterator get_farthest_bmentry(const unsigned long long & block_no);
	bool bm_flush(const cdp_bmkey_t & bmkey);

	bool read_bmentries_from_cache(const unsigned long long &starting_block, cdp_timestamp_t * buffer);
	bool write_bmentries_to_disk(const unsigned long long & block_no, unsigned int count, cdp_timestamp_t * buffer);

	bool set_ts_to_disk_nocaching(const unsigned long long &block_no, const cdp_timestamp_t& ts);
	bool write_bmentry_to_disk(const std::string &filename, const unsigned long long &block_no, const cdp_timestamp_t &ts);

	bool update_ts_in_cache(const unsigned long long &block_no, const cdp_timestamp_t& ts);
	
	inline bool insert_into_dirtybitmap(const cdp_bm_dirtykey_t & dirty_bmkey)
	{
		m_dirty_keys.insert(dirty_bmkey);
		return true;
	}

	// ============================================
	// resource management routines
	// ============================================

	bool canissue_nextio();
	void increment_pending_ios();
	void decrement_pending_ios();
	bool error() { return m_error ;} 

	// ==========================================
	// io routines
	// ==========================================
	
	// ==========================================
	// asynchronous flush
	// ==========================================
	
	bool write_async_bmentries();

	bool get_bmentries_limited_by_memory(const cdp_dirty_bmkeys_citer_t & start, 
		const cdp_dirty_bmkeys_citer_t & end, 
		const SV_UINT & max_memory, 
		cdp_dirty_bmkeys_citer_t & out_bmentries_start,
		cdp_dirty_bmkeys_citer_t & out_bmentries_end, 
		SV_UINT &mem_required);

	bool read_bmentries_from_cache(char * bm_data,
		cdp_dirty_bmkeys_citer_t & start,
		cdp_dirty_bmkeys_citer_t & end);

	bool write_async_bmentries(char * bm_data,
		const cdp_dirty_bmkeys_citer_t & batch_start, 
		const cdp_dirty_bmkeys_citer_t & batch_end);

	bool get_bmentries_belonging_to_same_file(
		const cdp_dirty_bmkeys_citer_t & start,
		const cdp_dirty_bmkeys_citer_t &end,
		cdp_dirty_bmkeys_citer_t &bmentries_samefile_iter,
		cdp_dirty_bmkeys_citer_t &bmentries_samefile_end,
		std::string & bm_filepath);

	bool write_async_bmentries(ACE_Asynch_Write_File_Ptr &asynch_handle,
		const std::string & bm_filepath,
		char * bm_data,
		const cdp_dirty_bmkeys_citer_t & batch_start, 
		const cdp_dirty_bmkeys_citer_t & batch_end,
		SV_UINT & bm_length_consumed);

	bool initiate_next_async_write(ACE_Asynch_Write_File_Ptr &asynch_handle, 
		const std::string & bm_filepath,
		const char * buffer,
		const SV_UINT &length,
		const SV_OFFSET_TYPE& offset);

	bool wait_for_xfr_completion();
	bool flush_io(ACE_HANDLE bm_handle, const std::string & bm_filepath);

	void handle_write_file (const ACE_Asynch_Write_File::Result &result);  

	//Performance Impr
	void handle_read_file (const ACE_Asynch_Read_File::Result &result);  
	
	bool get_read_blocks_limited_by_memory(const read_blocks_iter_t & start, 
		const read_blocks_iter_t & end, 
		const SV_UINT & max_memory, 
		read_blocks_iter_t & out_bmentries_start,
		read_blocks_iter_t & out_bmentries_end, 
		SV_UINT &mem_required);

	bool initiate_next_async_read(ACE_Asynch_Read_File_Ptr &asynch_handle, 
		std::string filename,
		char * buffer,
		const SV_UINT &length,
		const SV_OFFSET_TYPE& offset);

	bool get_bitmap_filehandle(const unsigned long long &block_no, ACE_Asynch_Read_File_Ptr &rf_ptr, std::string &filename);
	bool read_async_bmentries(char * buffer,
							  const read_blocks_iter_t & batch_start, 
							  const read_blocks_iter_t & batch_end,
							  SV_UINT & bm_length_read);

	bool add_bmdata_to_cache(read_blocks_iter_t start, read_blocks_iter_t end, char *buffer);
	bool close_asyncread_handles();

	bool free_bmdata_from_cache();
	void dump_bmdata_from_cache_to_log();
	// ==========================================
	// synchronous flush
	// ==========================================

	bool write_sync_bmentries();

	bool write_sync_bmentries(char * bm_data,
		const cdp_dirty_bmkeys_citer_t & batch_start, 
		const cdp_dirty_bmkeys_citer_t & batch_end);

	bool write_sync_bmentries(ACE_HANDLE & bm_handle,
		const std::string & bm_filepath,
		char * bm_data,
		const cdp_dirty_bmkeys_citer_t & batch_start, 
		const cdp_dirty_bmkeys_citer_t & batch_end,
		SV_UINT & bm_length_consumed);

	std::string m_path;

	bool m_iscaching_enabled;
	bool m_directio;
	bool m_compressed;
	
	unsigned int m_cachesize;
	int m_memconsumed;
	unsigned int m_blocksize;
	unsigned int m_blocks_per_entry;
	unsigned int m_buffer_size;

	unsigned long m_iowrites;
	unsigned long m_ioreads;
	unsigned long m_iowrites_dueto_insufficient_memory;
	unsigned long long m_timespent_on_io;

	bool m_init;

	cdp_bmentries_t m_bmentries;
	cdp_dirty_bmkeys_t m_dirty_keys;
	SharedAlignedBuffer m_aligned_buffer;
	cdp_timestamp_t * m_buffer;

	// variables used for flush io
	bool m_asyncio;
	ACE_Thread_Mutex iocount_mutex;
	ACE_Condition<ACE_Thread_Mutex> iocount_cv;
	volatile unsigned long m_pendingios;
	unsigned long m_minpendingios;
	unsigned long m_maxpendingios;
	SV_UINT m_maxmemory_for_io;
	bool m_error;
	SV_ULONGLONG m_sector_size;

	cdp_bitmap_asyncreadhandles_t m_bitmap_read_handles;
};

#endif
