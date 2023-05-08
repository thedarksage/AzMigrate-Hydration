//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File:       cdpdrtd.h
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


// used by rollback, recovery, vsnap code
struct cdp_rollback_drtd_t
{
    SV_UINT         length;
    SV_OFFSET_TYPE  voloffset;
    SV_OFFSET_TYPE  fileoffset;
	SV_ULONGLONG    timestamp;
	std::string     filename;
};

/*typedef boost::shared_ptr<cdp_rollback_drtd_t> cdp_rollback_drtdptr_t;
typedef std::vector< cdp_rollback_drtdptr_t >   cdp_rollback_drtdptrs_t;
typedef cdp_rollback_drtdptrs_t::iterator cdp_rollback_drtdptrs_iter_t; 
typedef cdp_rollback_drtdptrs_t::reverse_iterator cdp_rollback_drtdptrs_riter_t;*/ 


//struct cdp_drtd_t
//{
//public:
//
//	cdp_drtd_t()
//		:m_length(0),
//		m_voloffset(-1),
//		m_fileoffset(-1)
//	{
//	}
//
//	cdp_drtd_t(const SV_UINT & length,
//				const SV_OFFSET_TYPE & voloffset,
//				const SV_OFFSET_TYPE & fileoffset)
//				:m_length(length),
//				m_voloffset(voloffset),
//				m_fileoffset(fileoffset)
//	{
//	}
//
//
//	inline const SV_OFFSET_TYPE & get_file_offset() const { return m_fileoffset; } 
//	inline const SV_OFFSET_TYPE & get_volume_offset() const { return m_voloffset; }
//	inline const SV_UINT & get_length() const { return m_length; }
//
//	// used for performing write to volume
//	inline const SV_OFFSET_TYPE & get_volume_write_offset() const { return m_voloffset; }
//	inline const SV_UINT & get_volume_write_length() const { return m_length; }
//	
//	// used for performing cow reads
//	inline const SV_OFFSET_TYPE & get_volume_read_offset() const { return m_voloffset ; }
//	inline const SV_UINT & get_volume_read_length() const { return m_length; } 
//
//	
//private:
//
//    SV_UINT         m_length;
//    SV_OFFSET_TYPE  m_voloffset;
//    SV_OFFSET_TYPE  m_fileoffset;
//};
//
//
//typedef boost::shared_ptr<cdp_drtd_t> cdp_drtdptr_t;
//typedef std::vector< cdp_drtdptr_t >  cdp_drtdptrs_t;
//typedef cdp_drtdptrs_t::iterator cdp_drtdptrs_iter_t;
//typedef cdp_drtdptrs_t::reverse_iterator cdp_drtdptrs_riter_t; 
//
//typedef std::vector< cdp_drtd_t >   cdp_drtds_t;
//typedef cdp_drtds_t::iterator cdp_drtds_iter_t; 
//typedef cdp_drtds_t::const_iterator cdp_drtds_constiter_t; 
//typedef cdp_drtds_t::reverse_iterator cdp_drtds_riter_t; 
//typedef cdp_drtds_t::const_reverse_iterator cdp_drtds_constriter_t; 

struct cdp_drtdv2_t
{

public:

	cdp_drtdv2_t()
				:m_length(0),
				m_voloffset(-1),
				m_fileoffset(-1),
				m_seqdelta(0),
				m_timedelta(0),
				m_fileid(0)
	{
	}

	cdp_drtdv2_t(const SV_UINT & length,
				const SV_OFFSET_TYPE & voloffset,
				const SV_OFFSET_TYPE & fileoffset,
				const SV_UINT & seqdelta,
				const SV_UINT & timedelta,
				SV_UINT fileid)
				:m_length(length),
				m_voloffset(voloffset),
				m_fileoffset(fileoffset),
				m_seqdelta(seqdelta),
				m_timedelta(timedelta),
				m_fileid(fileid)
	{
	}


	inline const SV_OFFSET_TYPE & get_file_offset() const { return m_fileoffset; } 
	inline const SV_OFFSET_TYPE & get_volume_offset() const { return m_voloffset; }
	inline const SV_UINT & get_length() const { return m_length; }
	inline const SV_UINT & get_seqdelta() const { return m_seqdelta; }
	inline const SV_UINT & get_timedelta() const { return m_timedelta; }
	inline const SV_UINT & get_fileid() const { return m_fileid; }

private:

    SV_UINT         m_length;
    SV_OFFSET_TYPE  m_voloffset;
    SV_OFFSET_TYPE  m_fileoffset;
    SV_UINT      	m_seqdelta;
    SV_UINT      	m_timedelta;
	SV_UINT         m_fileid;
};

typedef boost::shared_ptr<cdp_drtdv2_t> cdp_drtdv2ptr_t;
typedef std::vector< cdp_drtdv2ptr_t >   cdp_drtdv2ptrs_t;
typedef cdp_drtdv2ptrs_t::iterator cdp_drtdv2ptrs_iter_t; 
typedef cdp_drtdv2ptrs_t::const_iterator cdp_drtdv2ptrs_constiter_t; 
typedef cdp_drtdv2ptrs_t::reverse_iterator cdp_drtdv2ptrs_riter_t; 

typedef std::vector< cdp_drtdv2_t >   cdp_drtdv2s_t;
typedef cdp_drtdv2s_t::iterator cdp_drtdv2s_iter_t; 
typedef cdp_drtdv2s_t::const_iterator cdp_drtdv2s_constiter_t; 
typedef cdp_drtdv2s_t::reverse_iterator cdp_drtdv2s_riter_t; 
typedef cdp_drtdv2s_t::const_reverse_iterator cdp_drtdv2s_constriter_t; 

struct cdp_drtdv2_fileid_t
{

public:

	cdp_drtdv2_fileid_t()
				:m_length(0),
				m_voloffset(-1),
				m_fileoffset(-1),
				m_fileid(0),
				m_drtdno(0)
	{
	}

	cdp_drtdv2_fileid_t(const SV_UINT & length,
				const SV_OFFSET_TYPE & voloffset,
				const SV_OFFSET_TYPE & fileoffset,
				const SV_UINT & fileid,
				const SV_UINT & drtdno)
				:m_length(length),
				m_voloffset(voloffset),
				m_fileoffset(fileoffset),
				m_fileid(fileid),
				m_drtdno(drtdno)
	{
	}


	inline const SV_OFFSET_TYPE & get_file_offset() const { return m_fileoffset; } 
	inline const SV_OFFSET_TYPE & get_volume_offset() const { return m_voloffset; }
	inline const SV_UINT & get_length() const { return m_length; }
	inline const SV_UINT & get_fileid() const { return m_fileid; }
	inline const SV_UINT & get_drtdno() const { return m_drtdno; }

private:

    SV_UINT         m_length;
    SV_OFFSET_TYPE  m_voloffset;
    SV_OFFSET_TYPE  m_fileoffset;
	SV_UINT         m_fileid;
	SV_UINT         m_drtdno;
};


struct cdp_aligned_drtd_t
{

public:

	cdp_aligned_drtd_t()
		:m_write_vollength(0),
		m_write_voloffset(-1),
		m_fileoffset(-1),
		m_read_vollength(0),
		m_read_voloffset(-1),
		m_seqdelta(0),
		m_timedelta(0)
	{
	}

	cdp_aligned_drtd_t(const SV_UINT & write_vollength,
				const SV_OFFSET_TYPE & write_voloffset,
				const SV_OFFSET_TYPE & fileoffset,
				const SV_UINT & read_vollength,
				const SV_OFFSET_TYPE &  read_voloffset,
				const SV_UINT & seqdelta,
				const SV_UINT & timedelta)
				:m_write_vollength(write_vollength),
				m_write_voloffset(write_voloffset),
				m_fileoffset(fileoffset),
				m_read_vollength(read_vollength),
				m_read_voloffset(read_voloffset),
				m_seqdelta(seqdelta),
				m_timedelta(timedelta)
	{
	}


	// used when performing write to volume
	inline const SV_OFFSET_TYPE & get_file_offset() const { return m_fileoffset; } 
	inline const SV_OFFSET_TYPE & get_volume_write_offset() const { return m_write_voloffset; }
	inline const SV_UINT & get_volume_write_length() const { return m_write_vollength; }
	
	// used for performing cow reads
	inline const SV_OFFSET_TYPE & get_volume_read_offset() const { return m_read_voloffset ; }
	inline const SV_UINT & get_volume_read_length() const { return m_read_vollength; } 
	
	inline const SV_UINT & get_seqdelta() const { return m_seqdelta; }
	inline const SV_UINT & get_timedelta() const { return m_timedelta; }

private:

	// write to volume
	SV_UINT         m_write_vollength;
	SV_OFFSET_TYPE  m_write_voloffset;
	SV_OFFSET_TYPE  m_fileoffset;

	// cow reads from volume 
	// sparse retention: (read_voloffset = 4k aligned, read_length = fixed 4k)
	// non sparse retention: (read_voloffset and read_vollength are same as
	//                        write_voloffset and write_vollength)
	//
	SV_UINT         m_read_vollength;
	SV_OFFSET_TYPE  m_read_voloffset;

	SV_UINT      	m_seqdelta;
	SV_UINT      	m_timedelta;
};

typedef boost::shared_ptr<cdp_aligned_drtd_t> cdp_aligned_drtdptr_t;
typedef std::vector< cdp_aligned_drtdptr_t >   cdp_aligned_drtdptrs_t;
typedef cdp_aligned_drtdptrs_t::iterator cdp_aligned_drtdptrs_iter_t; 
typedef cdp_aligned_drtdptrs_t::const_iterator cdp_aligned_drtdptrs_constiter_t; 
typedef cdp_aligned_drtdptrs_t::reverse_iterator cdp_aligned_drtdptrs_riter_t; 

typedef std::vector< cdp_aligned_drtd_t >   cdp_aligned_drtds_t;
typedef cdp_aligned_drtds_t::iterator cdp_aligned_drtds_iter_t; 
typedef cdp_aligned_drtds_t::const_iterator cdp_aligned_drtds_constiter_t; 
typedef cdp_aligned_drtds_t::reverse_iterator cdp_aligned_drtds_riter_t; 
typedef cdp_aligned_drtds_t::const_reverse_iterator cdp_aligned_drtds_constriter_t; 



struct cdp_drtd_sparsefile_t
{
    public:

	cdp_drtd_sparsefile_t()
				:m_length(0),
                m_voloffset(-1),
				m_fileoffset(-1)
	{
	}

	cdp_drtd_sparsefile_t(const SV_UINT & length,
                const SV_OFFSET_TYPE & voloffset,
				const SV_OFFSET_TYPE & fileoffset,
                const std::string & filename)
				:m_length(length),
                m_voloffset(voloffset),
				m_fileoffset(fileoffset),
				m_filename(filename)
	{
	}

	inline const SV_OFFSET_TYPE & get_vol_offset() const { return m_voloffset; } 
	inline const SV_OFFSET_TYPE & get_file_offset() const { return m_fileoffset; } 
	inline const SV_UINT & get_length() const { return m_length; }
    inline const std::string & get_filename() const { return m_filename; }

private:

    SV_UINT         m_length;
    SV_OFFSET_TYPE  m_voloffset;
    SV_OFFSET_TYPE  m_fileoffset;
    std::string     m_filename;
};

typedef std::vector<cdp_drtd_sparsefile_t> cdp_drtds_sparsefile_t;

#endif
