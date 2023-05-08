//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File:       cdpv3metadatafile.h
//
// Description: 
//

#ifndef CDPV3METADATAFILE__H
#define CDPV3METADATAFILE__H

#include <string>

#include "cdpglobals.h"
#include "file.h"
#include "cdpv3tables.h"
#include "boost/shared_array.hpp"
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>
#include "svdconvertor.h"
//
// CDPV3Metadata Structure
// 
struct cdpv3metadata_t
{
	SV_ULONGLONG          m_tsfc;
	SV_ULONGLONG          m_tsfcseq;
	SV_ULONGLONG          m_tslc;
	SV_ULONGLONG          m_tslcseq;
	SV_UINT               m_type;
	UserTags              m_users;
	cdp_drtdv2s_t         m_drtds;
	std::vector<std::string> m_filenames;
	std::vector<SV_OFFSET_TYPE> m_baseoffsets;
	std::vector<SV_ULONGLONG> m_byteswritten;

	void clear()
	{
		m_tsfc = 0;
		m_tsfcseq = 0;
		m_tslc = 0;
		m_tslcseq = 0;
		m_type = 0;
		m_users.clear();
		m_drtds.clear();
		m_filenames.clear();
		m_baseoffsets.clear();
		m_byteswritten.clear();
	}
};


typedef boost::shared_ptr<cdpv3metadata_t> cdpv3metadataptr_t;
typedef std::vector< cdpv3metadataptr_t > cdpv3metadataptrs_t;
typedef cdpv3metadataptrs_t::iterator cdpv3metadataptrs_iter_t;
typedef cdpv3metadataptrs_t::const_iterator cdpv3metadataptrs_citer_t;
typedef cdpv3metadataptrs_t::reverse_iterator cdpv3metadataptrs_riter_t;

typedef std::vector< cdpv3metadata_t > cdpv3metadatas_t;
typedef cdpv3metadatas_t::iterator cdpv3metadatas_iter_t;
typedef cdpv3metadatas_t::const_iterator cdpv3metadatas_citer_t;
typedef cdpv3metadatas_t::reverse_iterator cdpv3metadatas_riter_t;

std::ostream & operator <<( std::ostream & o, const cdpv3metadata_t & md);

// ======================================================
// Metadata File format:
// 
// the below format repeating for each differential
// 
//                SVD_PREFIX (TFV2)
//                SVD_TIME_STAMP_V2
//                SVD_PREFIX (TLV2)
//                SVD_TIME_STAMP_V2
//                SVD_PREFIX (OTHR)
//                SVD_OTHR_INFO
//
//
//                SVD_PREFIX (FOST)
// these      +--------------------------------------------------+
// repeating  |   SVD_FOST_INFO    {filename, baseoffset}        |
//            +--------------------+ SVD_PREFIX.count times -----+
//
//                SVD_PREFIX (CDPD)
// these      +--------------------------------------------------+
// repeating  |   SVD_CDPDRTD {voloffset,fileid,tsdelta,seqdelta}|
//            +--------------------+ SVD_PREFIX.count times -----+
// 
// these     +----SVD_PREFIX (USER)------------------------------+          
// repeating |--  formatted data of length SVD_PREFIX.flags      | (optional)
//           +---------------------------------------------------+
//
//                SVD_PREFIX (SKIP)
//                white space (\0) to make this metadata block 512 byte multiple
//
//                SVD_PREFIX (SOST)
//                SV_ULONGLONG {pointing to start of this metadata}
//                 
// --------------------------------------------------------------------------

class cdpv3metadatafile_t : public File
{
public:
	
	static std::string getfilepath(const SV_ULONGLONG &ts, const std::string & folder);

    // log for maintaining list of data files removed due to coalesce operation
    static std::string getlogpath(const SV_ULONGLONG &ts, const std::string & folder);

	static SV_ULONGLONG getmetadatafile_timestamp(std::string mdfilename);
	static SV_ULONGLONG getdellogfile_timestamp(std::string mdfilename);

	cdpv3metadatafile_t(const std::string filepath, 
		const SV_OFFSET_TYPE & endoffset = -1, const SV_UINT& read_ahead_length = 4194304, bool ready = true);
	~cdpv3metadatafile_t();

	SVERROR read_metadata_desc(cdpv3metadata_t & metadata, const SV_ULONGLONG & time_to_recover = 0);
	SVERROR read_metadata_asc(cdpv3metadata_t & metadata);
	bool get_lowestseq_gr_eq_ts(const SV_ULONGLONG & ts, SV_ULONGLONG & seq);

	bool open(BasicIo::BioOpenMode mode);
	bool close();
    bool read_ahead(const SV_OFFSET_TYPE& fileoffset, const SV_ULONGLONG& length);
    SV_ULONGLONG getfilesize() { return m_filesize;}

    void set_ready_state();
    void wait_for_ready_state();

	cdp_io_stats_t get_io_stats();
    

protected:

private:

	
	SV_LONGLONG seek(const SV_OFFSET_TYPE& offset, const BasicIo::BioSeekFrom& from);
	SV_LONGLONG in_mem_seek(const SV_OFFSET_TYPE& offset, const BasicIo::BioSeekFrom& from);
	SV_UINT read(char* buffer, const SV_UINT& length);
	SV_UINT in_mem_read(char* buffer, const SV_UINT& length);
	SV_LONGLONG tell();
	SV_LONGLONG in_mem_tell(){	return m_in_mem_current_offset;}


    bool read_tsfc(cdpv3metadata_t & metadata);
    bool read_tslc(cdpv3metadata_t & metadata);
	bool read_othr(cdpv3metadata_t & metadata);
	bool read_fost(const SVD_PREFIX & prefix, cdpv3metadata_t & metadata, const SV_ULONGLONG & time_to_recover = 0);
	bool read_cdpd(const SVD_PREFIX & prefix, cdpv3metadata_t & metadata);
	bool read_sost(SV_OFFSET_TYPE & startoffset);
	bool read_user(const SVD_PREFIX & prefix, cdpv3metadata_t & metadata,SV_UINT& dataFormatFlags);
	bool skip_bytes(const SVD_PREFIX & prefix);

	void ANNOUNCE_TAG(const std::string & tagname);

	std::string m_filepath;
	SV_ULONGLONG m_filesize;
	SV_OFFSET_TYPE m_start;
	SV_OFFSET_TYPE m_end;

	bool m_read_ahead_enabled;
	SV_UINT m_read_ahead_length;
	SV_OFFSET_TYPE m_in_mem_start_offset;
	SV_OFFSET_TYPE m_in_mem_end_offset;
	SV_OFFSET_TYPE m_in_mem_current_offset;
	boost::shared_array<char> m_in_mem_buffer;

	SVDConvertorPtr m_svdConvertor ;

	// metadatafile length and IOs
	SV_UINT m_io_count;
	SV_ULONGLONG m_io_bytes;

    // when performing a rollback operation, some of drtd belonging to certains data files
    // can be skipped to perform faster rollback
    // m_files_to_skip holds those data file ids.
    std::vector<unsigned int> m_files_to_skip;

    // metadata file object can be read ahead from
    // metadatareadahead threads. While metadatareadahaead thread is performing the operation
    // we do not want another thread to start accessing the in memory buffer
    // to handle this, we have introduced m_ready member variable
    // this variable will be set from the metadatareadahead thread
    // if readahead is disabled, m_ready will default to true
    // mutex and condition variables are used for synchronization/wait till the in memory buffer is ready

    bool m_ready;
    boost::mutex m_isready_mutex;
    boost::condition_variable m_isready_cv;
};

typedef boost::shared_ptr<cdpv3metadatafile_t> cdpv3metadatafileptr_t;
#endif
