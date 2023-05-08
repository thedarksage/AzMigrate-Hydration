//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File:       cdpv3metadatafile.cpp
//
// Description: 
//

#include "cdpv3metadatafile.h"
#include "cdputil.h"
#include "cdpv3tables.h"
#include "convertorfactory.h"
#include "inmsafeint.h"
#include "inmageex.h"

#include <boost/tokenizer.hpp>
#include <boost/lexical_cast.hpp>

#include <sstream>
#include <fstream>
using namespace std;

void dump_metadata_v3(const cdpv3metadata_t & md)
{
	bool rv = true;
	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME);

	do
	{
		stringstream dmpstr;
		int i =0;
		dmpstr << "METADATA:" << '\n'
			<< md.m_tsfc << ' '
			<< md.m_tsfcseq << ' '
			<< md.m_tslc << ' '
			<< md.m_tslcseq << '\n';

		for ( i = 0; i < md.m_filenames.size() ; ++i )
		{
			dmpstr << md.m_filenames[i] << ' '
				<< md.m_baseoffsets[i] << ' '
				<< md.m_byteswritten[i] << '\n';
		}

		for ( i = 0;  i < md.m_drtds.size() ; ++i)
		{
			dmpstr << "drtd:" << ' '
				<< md.m_drtds[i].get_volume_offset() << ' '
				<< md.m_drtds[i].get_length() << ' '
				<< md.m_drtds[i].get_file_offset() << ' '
				<< md.m_drtds[i].get_fileid() << ' '
				<< md.m_drtds[i].get_timedelta() << ' '
				<< md.m_drtds[i].get_seqdelta() << '\n';
		}

		DebugPrintf(SV_LOG_DEBUG, "%s\n", dmpstr.str().c_str());

#if 0


		std::string debuginfoPath = "C:\\debug\\";


		if(SVMakeSureDirectoryPathExists(debuginfoPath.c_str()).failed())
		{
			DebugPrintf(SV_LOG_ERROR, "%s unable to create %s\n", FUNCTION_NAME, debuginfoPath.c_str());
			rv = false;
			break;
		}

		debuginfoPath += "metadata.txt";

		// PR#10815: Long Path support
		std::ofstream out(getLongPathName(debuginfoPath.c_str()).c_str(), std::ios::app);


		if(!out) 
		{
			DebugPrintf(SV_LOG_ERROR,
				"FAILED: %s:Encountered error while opening %s. @LINE %d in FILE %s\n",
				FUNCTION_NAME, debuginfoPath.c_str(), LINE_NO,FILE_NAME);
			rv = false;
			break;
		}

		out << dmpstr.str() << "\n";
#endif


	} while (0);

	DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME);
}

std::ostream & operator <<( std::ostream & o, const cdpv3metadata_t & md)
{
	unsigned int  i = 0;
	o << "METADATA:" << endl;

	o << "Start Timestamp: " << md.m_tsfc << endl;
	o << "( " ;
	std::string display_time;
	CDPUtil::ToDisplayTimeOnConsole(md.m_tsfc,display_time);
	o << display_time << " )" << endl;
	o << "Start Sequence: " << md.m_tsfcseq << endl;

	o << "End Timestamp: " << md.m_tslc << endl;
	o << "( " ;
	CDPUtil::ToDisplayTimeOnConsole(md.m_tslc,display_time);
	o << display_time << " )" << endl;
	o << "End Sequence: " << md.m_tslcseq << endl;

	o << "Bookmarks: " << endl;
	for ( i = 0; i < md.m_users.size() ; ++i)
	{
		o << *(md.m_users[i]) << endl;
	}

	o << "DRTDS: " << endl;
	for ( i =0; i < md.m_drtds.size() ; ++i)
	{
		o << md.m_drtds[i] << endl;
	}

	o << "Data Files" << endl;
	for ( i =0; i < md.m_filenames.size() ; ++i)
	{
		o << "File Name:     "  << md.m_filenames[i] << endl;
		o << "Base Offset:   "  << md.m_baseoffsets[i] << endl;
		o << "Bytes Written: "  << md.m_byteswritten[i] << endl;
	}

	return o;
}

std::string cdpv3metadatafile_t::getfilepath(const SV_ULONGLONG &ts, const std::string & folder)
{
	SV_TIME svtime;
	if(!CDPUtil::ToSVTime(ts, svtime))
	{
		DebugPrintf(SV_LOG_ERROR, "FUNCTION  %s invalid time " ULLSPEC " \n",
			FUNCTION_NAME, ts);
		return string();
	} 

	stringstream fname_str;
	fname_str << folder
		<< ACE_DIRECTORY_SEPARATOR_CHAR_A
		<< CDPV3_MD_PREFIX  
		<< "y" << (svtime.wYear - CDP_BASE_YEAR)
		<< "m" << svtime.wMonth
		<< "d" << svtime.wDay
		<< "h" << svtime.wHour
		<< CDPV3_MD_SUFFIX ;

	return fname_str.str();
}

// log for maintaining list of data files removed due to coalesce operation
std::string cdpv3metadatafile_t::getlogpath(const SV_ULONGLONG &ts, const std::string & folder)
{
	SV_TIME svtime;
	if(!CDPUtil::ToSVTime(ts, svtime))
	{
		DebugPrintf(SV_LOG_ERROR, "FUNCTION  %s invalid time " ULLSPEC " \n",
			FUNCTION_NAME, ts);
		return string();
	} 

	stringstream fname_str;
	fname_str << folder
		<< ACE_DIRECTORY_SEPARATOR_CHAR_A
		<< CDPV3_COALESCED_PREFIX  
		<< "y" << (svtime.wYear - CDP_BASE_YEAR)
		<< "m" << svtime.wMonth
		<< "d" << svtime.wDay
		<< "h" << svtime.wHour
		<< CDPV3_COALESCED_SUFFIX ;

	return fname_str.str();
}

SV_ULONGLONG cdpv3metadatafile_t::getmetadatafile_timestamp(std::string mdfilename)
{
	SV_TIME sv;
	sv.wYear = sv.wMonth = sv.wDay = sv.wHour = sv.wMinute = sv.wSecond = sv.wMilliseconds = sv.wMicroseconds = sv.wHundrecNanoseconds = 0;
	SV_ULONGLONG timestamp = 0;
	//Check the file format.
	if(mdfilename.find(CDPV3_MD_PREFIX)!=string::npos && mdfilename.find(CDPV3_MD_SUFFIX)!=string::npos)
	{
		mdfilename.erase(0,strlen(CDPV3_MD_PREFIX.c_str()));
		mdfilename.erase(mdfilename.find("."));

		//The remaining string will be in the format y<A>m<B>d<C>hMD>. Tokenize the string the get
		//the year,month,day and hour.

		typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
		boost::char_separator<char> sep("ymdh");
		tokenizer tokens(mdfilename, sep);
		unsigned temp[4],i=0;

		for (tokenizer::iterator tok_iter = tokens.begin();
			tok_iter != tokens.end(); ++tok_iter)
		{
			temp[i] = boost::lexical_cast<unsigned int>(*tok_iter);
			i++;
		}

		sv.wYear = temp[0] + CDP_BASE_YEAR;
		sv.wMonth = temp[1];
		sv.wDay = temp[2];
		sv.wHour = temp[3];

		if(!CDPUtil::ToFileTime(sv,timestamp))
		{
			DebugPrintf(SV_LOG_ERROR, "FUNCTION  %s Unable to get the file time stamp",
				FUNCTION_NAME);
			timestamp = 0;

		}
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR, "FUNCTION  %s - File %s is not in the expected format",
			FUNCTION_NAME,mdfilename.c_str());
	}

	return timestamp;
}

SV_ULONGLONG cdpv3metadatafile_t::getdellogfile_timestamp(std::string dellogfilename)
{
	SV_TIME sv;
	sv.wYear = sv.wMonth = sv.wDay = sv.wHour = sv.wMinute = sv.wSecond = sv.wMilliseconds = sv.wMicroseconds = sv.wHundrecNanoseconds = 0;
	SV_ULONGLONG timestamp = 0;
	if(dellogfilename.find(CDPV3_COALESCED_PREFIX)!=string::npos && dellogfilename.find(CDPV3_COALESCED_SUFFIX)!=string::npos)
	{
		dellogfilename.erase(0,strlen(CDPV3_COALESCED_PREFIX.c_str()));
		dellogfilename.erase(dellogfilename.find("."));

		//The remaining string will be in the format y<A>m<B>d<C>hMD>. Tokenize the string the get
		//the year,month,day and hour.

		typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
		boost::char_separator<char> sep("ymdh");
		tokenizer tokens(dellogfilename, sep);
		unsigned temp[4],i=0;

		for (tokenizer::iterator tok_iter = tokens.begin();
			tok_iter != tokens.end(); ++tok_iter)
		{
			temp[i] = boost::lexical_cast<unsigned int>(*tok_iter);
			i++;
		}

		sv.wYear = temp[0] + CDP_BASE_YEAR;
		sv.wMonth = temp[1];
		sv.wDay = temp[2];
		sv.wHour = temp[3];

		if(!CDPUtil::ToFileTime(sv,timestamp))
		{
			DebugPrintf(SV_LOG_ERROR, "FUNCTION  %s Unable to get the file time stamp",
				FUNCTION_NAME);
			timestamp = 0;

		}
	}
	else
	{
		DebugPrintf(SV_LOG_ERROR, "FUNCTION  %s - File %s is not in the expected format",
			FUNCTION_NAME,dellogfilename.c_str());
	}

	return timestamp;
}

cdpv3metadatafile_t::cdpv3metadatafile_t(const std::string filepath, 
										 const SV_OFFSET_TYPE & endoffset,
										 const SV_UINT& read_ahead_length,
										 bool ready)
										 :File(filepath,false),m_ready(ready)
{
	m_filepath = filepath;
	m_start = 0;
	m_end = endoffset;

	m_read_ahead_length = read_ahead_length;
	if(m_read_ahead_length)
		m_read_ahead_enabled = true;
	else
		m_read_ahead_enabled = false;

	m_in_mem_start_offset = 0;
	m_in_mem_end_offset = 0;
	m_in_mem_current_offset = 0;
	m_filesize = 0;

	//Initialize io stats parameter
	m_io_count = 0;
	m_io_bytes = 0;

	ACE_stat statbuf;
	if(0 == sv_stat(getLongPathName(m_filepath.c_str()).c_str(),&statbuf))
	{
		// todo: what if open failed?
		if(Open(BasicIo::BioReadExisting |BasicIo::BioShareRW | BasicIo::BioBinary))
		{
			m_filesize = statbuf.st_size;
			m_end = statbuf.st_size;
		}
	}
}

// metadata file object can be read ahead from
// metadatareadahead threads. While metadatareadahaead thread is performing the operation
// we do not want another thread to start accessing the in memory buffer
// to handle this, we have introduced m_ready member variable
// this variable will be set from the metadatareadahead thread
// if readahead is disabled, m_ready will default to true
// mutex and condition variables are used for synchronization/wait till the in memory buffer is ready

void cdpv3metadatafile_t::set_ready_state()
{
	boost::unique_lock<boost::mutex> lock(m_isready_mutex);
	m_ready = true;
	m_isready_cv.notify_all();
}

void cdpv3metadatafile_t::wait_for_ready_state()
{
	boost::unique_lock<boost::mutex> lock(m_isready_mutex);
	while(!m_ready)
	{
		m_isready_cv.wait(lock);
	}
	return;
}


bool cdpv3metadatafile_t::read_ahead(const SV_OFFSET_TYPE& requested_offset, const SV_ULONGLONG& requested_length)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	bool rv = true;

	if(!m_filesize)
		return false;

	SV_OFFSET_TYPE fileoffset = requested_offset;
	SV_ULONGLONG length = requested_length;

	if(fileoffset < 0)
		fileoffset = 0;
	if(fileoffset + length > m_filesize) {
		INM_SAFE_ARITHMETIC(length = InmSafeInt<SV_ULONGLONG>::Type(m_filesize) - fileoffset, INMAGE_EX(m_filesize)(fileoffset))
    }

	do
	{
		if((m_in_mem_start_offset == m_in_mem_end_offset)
			|| (m_in_mem_start_offset > fileoffset && (fileoffset+length) > m_in_mem_end_offset)
			|| (fileoffset >= m_in_mem_end_offset)
			|| (fileoffset+length <= m_in_mem_start_offset))
		{
			m_in_mem_buffer.reset(new char[length]);

			if(File::Seek(fileoffset,  BasicIo::BioBeg) != fileoffset)
			{
				std::stringstream msg;
				msg << "Seek() to offset " << m_in_mem_end_offset << " in file " << m_filepath << " failed with error: "
					<< ACE_OS::last_error() << std::endl;
				DebugPrintf(SV_LOG_ERROR, msg.str().c_str());
				rv = false;
				break;
			}

			if(File::Read(m_in_mem_buffer.get(), length) != length)
			{
				std::stringstream msg;
				msg << "Read() at offset " << fileoffset << " in file " << m_filepath << " failed with error: "
					<< ACE_OS::last_error() << std::endl;
				DebugPrintf(SV_LOG_ERROR, msg.str().c_str());
				rv = false;
				break;
			}
			++m_io_count;
			m_io_bytes += length;
			
			m_in_mem_start_offset = fileoffset;
			m_in_mem_end_offset = fileoffset + length;
			m_in_mem_current_offset = m_in_mem_start_offset;
			break;
		}

		if(fileoffset < m_in_mem_start_offset && (fileoffset+length) <= m_in_mem_end_offset)
		{
			char *newBuffer = new char[length];
			inm_memcpy_s(newBuffer + (m_in_mem_start_offset - fileoffset), (length - (m_in_mem_start_offset - fileoffset)), m_in_mem_buffer.get(), (fileoffset + length) - m_in_mem_start_offset);
			m_in_mem_buffer.reset(newBuffer);

			if(File::Seek(fileoffset,  BasicIo::BioBeg) != fileoffset)
			{
				std::stringstream msg;
				msg << "Seek() to offset " << fileoffset << " in file " << m_filepath << " failed with error: "
					<< ACE_OS::last_error() << std::endl;
				DebugPrintf(SV_LOG_ERROR, msg.str().c_str());
				rv = false;
				break;
			}

			if(File::Read(m_in_mem_buffer.get(), (m_in_mem_start_offset - fileoffset)) != (m_in_mem_start_offset - fileoffset))
			{
				std::stringstream msg;
				msg << "Read() at offset " << fileoffset << " in file " << m_filepath << " failed with error: "
					<< ACE_OS::last_error() << std::endl;
				DebugPrintf(SV_LOG_ERROR, msg.str().c_str());
				rv = false;
				break;
			}
			++m_io_count;
			m_io_bytes += (m_in_mem_start_offset - fileoffset);
			
			m_in_mem_start_offset = fileoffset;
			m_in_mem_end_offset = fileoffset + length;
			m_in_mem_current_offset = m_in_mem_start_offset;
			break;
		}

		if(m_in_mem_start_offset <= fileoffset && m_in_mem_end_offset < (fileoffset+length))
		{
			char *newBuffer = new char[length];
			inm_memcpy_s(newBuffer, length ,m_in_mem_buffer.get() + (fileoffset - m_in_mem_start_offset), m_in_mem_end_offset - fileoffset);
			m_in_mem_buffer.reset(newBuffer);

			if(File::Seek(m_in_mem_end_offset,  BasicIo::BioBeg) != m_in_mem_end_offset)
			{
				std::stringstream msg;
				msg << "Seek() to offset " << m_in_mem_end_offset << " in file " << m_filepath << " failed with error: "
					<< ACE_OS::last_error() << std::endl;
				DebugPrintf(SV_LOG_ERROR, msg.str().c_str());
				rv = false;
				break;
			}

			if(File::Read(m_in_mem_buffer.get() + (m_in_mem_end_offset - fileoffset), (fileoffset + length - m_in_mem_end_offset))
				!= (fileoffset + length - m_in_mem_end_offset))
			{
				std::stringstream msg;
				msg << "Read() at offset " << m_in_mem_end_offset << " in file " << m_filepath << " failed with error: "
					<< ACE_OS::last_error() << std::endl;
				DebugPrintf(SV_LOG_ERROR, msg.str().c_str());
				rv = false;
				break;
			}
			++m_io_count;
			m_io_bytes += (fileoffset + length - m_in_mem_end_offset);
			
			m_in_mem_start_offset = fileoffset;
			m_in_mem_end_offset = fileoffset + length;
			m_in_mem_current_offset = m_in_mem_start_offset;
			break;
		}

		if(m_in_mem_start_offset <= fileoffset && (fileoffset+length) <= m_in_mem_end_offset)
		{
			m_in_mem_current_offset = m_in_mem_start_offset;
			break;
		}
	}while(false);

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}

SV_LONGLONG cdpv3metadatafile_t::in_mem_seek(const SV_OFFSET_TYPE& offset, const BasicIo::BioSeekFrom& from)
{
	if(from == BasicIo::BioBeg)
	{
		SV_OFFSET_TYPE offset_from_beg = offset;
		if(m_in_mem_start_offset <= offset_from_beg && offset_from_beg <= m_in_mem_end_offset)
		{
			m_in_mem_current_offset = offset_from_beg;
			return m_in_mem_current_offset;
		}

		std::stringstream msg;
		msg << "The requested offset is not available in memory" << std::endl
			<< "Requested offset: " << offset_from_beg << std::endl
			<< "InMemory start offset: " << m_in_mem_start_offset << std::endl
			<< "InMemory end offset: " << m_in_mem_end_offset << std::endl;
		DebugPrintf(SV_LOG_DEBUG, "%s", msg.str().c_str());
		return -1;
	}
	else if(from == BasicIo::BioCur)
	{
		SV_OFFSET_TYPE offset_from_cur = offset;
		if(m_in_mem_start_offset <= (m_in_mem_current_offset + offset_from_cur) &&
			(m_in_mem_current_offset + offset_from_cur) <= m_in_mem_end_offset)
		{
			INM_SAFE_ARITHMETIC(m_in_mem_current_offset += InmSafeInt<SV_OFFSET_TYPE>::Type(offset_from_cur), INMAGE_EX(m_in_mem_current_offset)(offset_from_cur))
			return m_in_mem_current_offset;
		}

		std::stringstream msg;
		msg << "The requested offset is not available in memory" << std::endl
			<< "Requested offset: " << (m_in_mem_current_offset + offset_from_cur) << std::endl
			<< "InMemory start offset: " << m_in_mem_start_offset << std::endl
			<< "InMemory end offset: " << m_in_mem_end_offset << std::endl;
		DebugPrintf(SV_LOG_DEBUG, "%s", msg.str().c_str());
		return -1;
	}
	else if(from == BasicIo::BioEnd)
	{
		SV_OFFSET_TYPE offset_from_end = offset;
		if(m_in_mem_start_offset <= (m_filesize + offset_from_end) &&
			(m_filesize + offset_from_end) <= m_in_mem_end_offset)
		{
			INM_SAFE_ARITHMETIC(m_in_mem_current_offset = InmSafeInt<SV_ULONGLONG>::Type(m_filesize) + offset_from_end, INMAGE_EX(m_filesize)(offset_from_end))
			return m_in_mem_current_offset;
		}

		std::stringstream msg;
		msg << "The requested offset is not available in memory" << std::endl
			<< "Requested offset: " << (m_filesize + offset_from_end) << std::endl
			<< "InMemory start offset: " << m_in_mem_start_offset << std::endl
			<< "InMemory end offset: " << m_in_mem_end_offset << std::endl;
		DebugPrintf(SV_LOG_DEBUG, "%s", msg.str().c_str());
		return -1;
	}
	else
		return -1;
}

SV_LONGLONG cdpv3metadatafile_t::seek(const SV_OFFSET_TYPE& offset, const BasicIo::BioSeekFrom& from)
{
	SV_OFFSET_TYPE newoffset;
	SV_OFFSET_TYPE currentoffset;

	do
	{
		if(m_read_ahead_enabled)
		{
			currentoffset = tell();
			newoffset = in_mem_seek(offset, from);
			if(from == BasicIo::BioBeg && newoffset != offset)
			{
				if(!read_ahead(offset, m_read_ahead_length))
				{
					DebugPrintf(SV_LOG_ERROR, "read_ahead() failed with error: %d for file %s\n",
						ACE_OS::last_error(), m_filepath.c_str());
					newoffset = -1;
					break;
				}
				newoffset = in_mem_seek(offset, from);
			}
			else if(from == BasicIo::BioCur && newoffset != currentoffset + offset)
			{
                SV_OFFSET_TYPE off;
                INM_SAFE_ARITHMETIC(off = InmSafeInt<SV_OFFSET_TYPE>::Type(currentoffset) + offset, INMAGE_EX(currentoffset)(offset))
				if(!read_ahead(off, m_read_ahead_length))
				{
					DebugPrintf(SV_LOG_ERROR, "read_ahead() failed with error: %d for file %s\n",
						ACE_OS::last_error(), m_filepath.c_str());
					newoffset = -1;
					break;
				}
				newoffset = tell();
			}
			else if(from == BasicIo::BioEnd && newoffset != m_filesize + offset)
			{
                SV_OFFSET_TYPE off;
                INM_SAFE_ARITHMETIC(off = InmSafeInt<SV_ULONGLONG>::Type(m_filesize) + offset, INMAGE_EX(m_filesize)(offset))
				if(!read_ahead(off, m_read_ahead_length))
				{
					DebugPrintf(SV_LOG_ERROR, "read_ahead() failed with error: %d for file %s\n",
						ACE_OS::last_error(), m_filepath.c_str());
					newoffset = -1;
					break;
				}
				newoffset = in_mem_seek(offset, from);
			}
		}
		else
		{
			newoffset = File::Seek(offset, from);
		}
	}while(false);

	return newoffset;
}

SV_UINT cdpv3metadatafile_t::in_mem_read(char* buffer, const SV_UINT& length)
{
	if(m_in_mem_current_offset + length <= m_in_mem_end_offset)
	{
		inm_memcpy_s(buffer, length,m_in_mem_buffer.get() + (m_in_mem_current_offset - m_in_mem_start_offset), length);
		INM_SAFE_ARITHMETIC(m_in_mem_current_offset += InmSafeInt<SV_UINT>::Type(length), INMAGE_EX(m_in_mem_current_offset)(length))
		return length;
	}

	std::stringstream msg;
	msg << "The requested length is not available in memory" << std::endl
		<< "InMemory current offset: " << m_in_mem_current_offset << std::endl
		<< "Requested length: " << length << std::endl
		<< "InMemory start offset: " << m_in_mem_start_offset << std::endl
		<< "InMemory end offset: " << m_in_mem_end_offset << std::endl;
	DebugPrintf(SV_LOG_DEBUG, "%s", msg.str().c_str());
	return 0;
}

SV_UINT cdpv3metadatafile_t::read(char* buffer, const SV_UINT& length)
{
	SV_UINT bytes_read = 0;
	SV_UINT bytes_to_read = length;
	do
	{
		if(m_read_ahead_enabled)
		{
			bytes_read = in_mem_read(buffer, length);
			if(length != bytes_read)
			{
				bytes_to_read = max(length, m_read_ahead_length);
				if(!read_ahead(m_in_mem_current_offset, bytes_to_read))
				{
					DebugPrintf(SV_LOG_ERROR, "read_ahead() failed with error: %d for file %s\n",
						ACE_OS::last_error(), m_filepath.c_str());
					bytes_read = 0;
					break;
				}
				bytes_read = in_mem_read(buffer, length);
			}
		}
		else
		{
			bytes_read = File::Read(buffer, length);
		}
	}while(false);

	return bytes_read;
}

SV_LONGLONG cdpv3metadatafile_t::tell()
{
	if(m_read_ahead_enabled)
		return in_mem_tell();
	else
		return File::Tell();
}


cdpv3metadatafile_t::~cdpv3metadatafile_t()
{
	close();
}

void cdpv3metadatafile_t::ANNOUNCE_TAG(const std::string & tagname)
{
	stringstream l_stdout;
	l_stdout << "Got:" << tagname << "\n" ;
	DebugPrintf(SV_LOG_DEBUG, "%s", l_stdout.str().c_str());
}

bool cdpv3metadatafile_t::open(BasicIo::BioOpenMode mode)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	SV_ULONG rv = File::Open(mode);

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

	return (SV_SUCCESS == rv);
}

bool cdpv3metadatafile_t::close()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	SV_ULONG rv = File::Close();
	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);

	return (SV_SUCCESS == rv);
}


// ======================================================
// Metadata File format:
// 
// the below format repeating for each differential
// 
//                SVD_PREFIX (TFV2)
//                SVD_TIME_STAMP
//                SVD_PREFIX (TLV2)
//                SVD_TIME_STAMP
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

SVERROR cdpv3metadatafile_t::read_metadata_desc(cdpv3metadata_t & metadata, const SV_ULONGLONG & time_to_recover)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s %s\n",FUNCTION_NAME, m_filepath.c_str());

	bool rv = false;
	int  readin = 0;
	SVD_PREFIX  prefix = {0};
	std::string tagname;
	SV_OFFSET_TYPE startoffset = 0;
	SV_OFFSET_TYPE seekoffset = 0;
	SV_OFFSET_TYPE skip = 0;
	SV_UINT dataFormatFlags = 0;
	ConvertorFactory::setDataFormatFlags(dataFormatFlags);

	metadata.clear();
	m_files_to_skip.clear();
	wait_for_ready_state();

	if(m_end == 0)
	{
		DebugPrintf(SV_LOG_DEBUG,"EXITED %s %s\n",FUNCTION_NAME, m_filepath.c_str());
		return SVS_FALSE;
	}

	if(m_end == -1)
	{
		if(!m_filesize)
			return SVS_FALSE;
	}

	do
	{
		INM_SAFE_ARITHMETIC(seekoffset = InmSafeInt<SV_OFFSET_TYPE>::Type(m_end) - (SVD_SOST_SIZE + SVD_PREFIX_SIZE), INMAGE_EX(m_end))
		SV_OFFSET_TYPE newoffset = 0;
		if(m_read_ahead_enabled)
		{
			newoffset = in_mem_seek(seekoffset,  BasicIo::BioBeg);
			if( newoffset != seekoffset )
			{
                SV_OFFSET_TYPE off;
                INM_SAFE_ARITHMETIC(off = InmSafeInt<SV_OFFSET_TYPE>::Type(m_end) - m_read_ahead_length, INMAGE_EX(m_end)(m_read_ahead_length))
				if(!read_ahead(off, m_read_ahead_length))
				{
					DebugPrintf(SV_LOG_ERROR, "read_ahead() failed with error: %d for file %s\n",
						ACE_OS::last_error(), m_filepath.c_str());
					rv = false;
					break;
				}
				newoffset = in_mem_seek(seekoffset,  BasicIo::BioBeg);
			}
		}
		else
		{
			newoffset = File::Seek(seekoffset,  BasicIo::BioBeg);
		}
		if(newoffset != seekoffset)
		{
			DebugPrintf(SV_LOG_ERROR, 
				"FUNCTION: %s seek on %s failed. expected: " LLSPEC " actual: " LLSPEC " \n",
				__FUNCTION__, File::GetName().c_str(), seekoffset, newoffset);
			rv = false;
			break;
		}

		readin = read((char*)&prefix, SVD_PREFIX_SIZE);
		if (SVD_PREFIX_SIZE != readin)
		{
			stringstream l_stdfatal;
			l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
				<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
				<< "\n" << File::GetName() << "is not parseable.\n" 
				<< "Prefix could not be be read at offset " << seekoffset << "\n"
				<< " Expected Read Bytes: " << SVD_PREFIX_SIZE << "\n"
				<< " Actual Read Bytes: "  << readin << "\n"
				<< " Error Code: " << File::LastError() << "\n"
				<< " Error Message: " << Error::Msg(File::LastError()) << "\n";

			DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());
			rv = false;
			break;
		}

		if( m_svdConvertor.get() == NULL )
		{
			SV_UINT FiledataFormat = dataFormatFlags;
			if(prefix.tag != SVD_TAG_SOST)
			{
				if(dataFormatFlags == SVD_TAG_BEFORMAT)
					FiledataFormat = INMAGE_MAKEFOURCC('L','T','R','D');
				else
					FiledataFormat = INMAGE_MAKEFOURCC('B','T','R','D');
			}

			ConvertorFactory::getSVDConvertor( FiledataFormat, m_svdConvertor ) ;
		}

		m_svdConvertor->convertPrefix( prefix ) ;
		if(prefix.tag != SVD_TAG_SOST)
		{

			//__asm int 3;

			stringstream l_stdfatal;
			l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
				<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
				<< "\n" << File::GetName() << "is not parseable.\n" 
				<< "offset: " <<  seekoffset << "\t" << tell() << "\n"
				<< "expecting tag: " << SVD_TAG_SOST_NAME << "\n"
				<< "got: " <<  prefix.tag << "\n" ;


			DebugPrintf(SV_LOG_ERROR, "%s", l_stdfatal.str().c_str());
			rv = false;
			break;
		}

		if(!read_sost(startoffset))
		{
			DebugPrintf(SV_LOG_ERROR, "startoffset = " LLSPEC "\n",
				startoffset);
			rv = false;
			break;
		}

		seekoffset = startoffset;
		if(m_read_ahead_enabled)
		{
			newoffset = in_mem_seek(seekoffset,  BasicIo::BioBeg);
			if( newoffset != seekoffset )
			{
                SV_UINT diff;
                INM_SAFE_ARITHMETIC(diff = InmSafeInt<SV_OFFSET_TYPE>::Type(m_end) - seekoffset, INMAGE_EX(m_end)(seekoffset))
				SV_UINT read_length = max(m_read_ahead_length, diff);
				SV_OFFSET_TYPE read_from;
                INM_SAFE_ARITHMETIC(read_from = InmSafeInt<SV_OFFSET_TYPE>::Type(m_end) - read_length, INMAGE_EX(m_end)(read_length))
				if(m_read_ahead_enabled && !read_ahead(read_from, read_length))
				{
					DebugPrintf(SV_LOG_ERROR, "read_ahead() failed with error: %d for file %s\n",
						ACE_OS::last_error(), m_filepath.c_str());
					rv = false;
					break;
				}
				newoffset = in_mem_seek(seekoffset,  BasicIo::BioBeg);
			}
		}
		else
		{
			newoffset = File::Seek(seekoffset,  BasicIo::BioBeg);
		}
		if(newoffset != seekoffset)
		{
			DebugPrintf(SV_LOG_ERROR, 
				"FUNCTION: %s seek on %s failed. expected: " LLSPEC " actual: " LLSPEC " \n",
				__FUNCTION__, File::GetName().c_str(), seekoffset, newoffset);
			rv = false;
			break;
		}

		while ( seekoffset < m_end )
		{
			readin = read((char*)&prefix, SVD_PREFIX_SIZE);
			m_svdConvertor->convertPrefix( prefix ) ;

			if (SVD_PREFIX_SIZE != readin)
			{
				stringstream l_stdfatal;
				l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
					<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
					<< "\n" << File::GetName() << "is corrupt.\n" 
					<< "Prefix could not be be read at offset " << tell() << "\n"
					<< " Expected Read Bytes: " << SVD_PREFIX_SIZE << "\n"
					<< " Actual Read Bytes: "  << readin << "\n"
					<< " Error Code: " << File::LastError() << "\n"
					<< " Error Message: " << Error::Msg(File::LastError()) << "\n";

				DebugPrintf(SV_LOG_FATAL, "%s", l_stdfatal.str().c_str());
				rv = false;
				break;
			}

			stringstream l_stdfatal;
			switch (prefix.tag)
			{

			case SVD_TAG_TIME_STAMP_OF_FIRST_CHANGE_V2:

				tagname = SVD_TAG_TSFC_V2_NAME;
				ANNOUNCE_TAG(tagname);
				rv = read_tsfc(metadata);
				break;

			case SVD_TAG_TIME_STAMP_OF_LAST_CHANGE_V2:

				tagname = SVD_TAG_TSLC_V2_NAME;
				ANNOUNCE_TAG(tagname);
				rv = read_tslc(metadata);
				break;

			case SVD_TAG_OTHR:

				tagname = SVD_TAG_OTHR_NAME;
				ANNOUNCE_TAG(tagname);
				rv = read_othr(metadata);
				break;

			case SVD_TAG_FOST:

				tagname = SVD_TAG_FOST_NAME;
				ANNOUNCE_TAG(tagname);
				rv = read_fost(prefix, metadata,time_to_recover);
				break;

			case SVD_TAG_CDPD:

				tagname = SVD_TAG_CDPD_NAME;
				ANNOUNCE_TAG(tagname);
				rv = read_cdpd(prefix, metadata);
				break;

			case SVD_TAG_USER:

				tagname = SVD_TAG_USER_NAME;
				ANNOUNCE_TAG(tagname);
				rv = read_user(prefix, metadata,dataFormatFlags);
				break;

			case SVD_TAG_SOST:

				tagname = SVD_TAG_SOST_NAME;
				ANNOUNCE_TAG(tagname);
				rv = read_sost(skip);
				break;

			case SVD_TAG_SKIP:

				tagname = SVD_TAG_SKIP_NAME;
				ANNOUNCE_TAG(tagname);
				rv = skip_bytes(prefix);
				break;

				// Unkown prefix
			default:

				l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
					<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
					<< "\n" << File::GetName()   << "is corrupt.\n" 
					<< "Encountered an unknown tag: " << prefix.tag << " at offset " 
					<< tell() << "\n";

				DebugPrintf(SV_LOG_FATAL, "%s", l_stdfatal.str().c_str());
				rv = false;
				break;
			}

			if ( !rv )
			{
				break;
			}
			seekoffset = tell();		
		}

		if(rv)
			m_end = startoffset;

		if(m_end == 0)
			close();


	} while (0);

	if(rv)
	{
#if 0
		stringstream ss ;
		ss << metadata << endl;
		DebugPrintf(SV_LOG_DEBUG, "%s", ss.str().c_str());		
		dump_metadata_v3(metadata);
#endif
	}

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s %s\n",FUNCTION_NAME, m_filepath.c_str());
	return ((rv)?SVS_OK:SVE_FAIL);
}


SVERROR cdpv3metadatafile_t::read_metadata_asc(cdpv3metadata_t & metadata)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s %s\n",FUNCTION_NAME, m_filepath.c_str());

	bool rv = false;
	int  readin = 0;
	SVD_PREFIX  prefix = {0};
	std::string tagname;
	SV_OFFSET_TYPE startoffset = m_start;
	SV_OFFSET_TYPE seekoffset = 0;
	SV_OFFSET_TYPE skip = 0;
	SV_UINT dataFormatFlags = 0;
	bool done = false;
	ConvertorFactory::setDataFormatFlags(dataFormatFlags);

	metadata.clear();
	if(m_end == m_start)
	{
		DebugPrintf(SV_LOG_DEBUG,"EXITED %s %s\n",FUNCTION_NAME, m_filepath.c_str());
		return SVS_FALSE;
	}

	if(m_end == -1)
	{
		ACE_stat statbuf;
		if(0 != sv_stat(getLongPathName(m_filepath.c_str()).c_str(),&statbuf))
		{
			DebugPrintf(SV_LOG_DEBUG,"EXITED %s is not yet created?\n", m_filepath.c_str());
			DebugPrintf(SV_LOG_DEBUG,"EXITED %s %s\n",FUNCTION_NAME, m_filepath.c_str());
			return SVS_FALSE;
		}
		m_filesize = statbuf.st_size;
		m_end = statbuf.st_size;
	}


	do
	{

		if(!Open(BasicIo::BioReadExisting |BasicIo::BioShareRW | BasicIo::BioBinary))
		{
			rv = false;
			break;
		}

		//if(!read_ahead(m_start, m_read_ahead_length))
		//{
		//	DebugPrintf(SV_LOG_ERROR, "read_ahead() failed with error: %d for file %s\n",
		//		ACE_OS::last_error(), m_filepath.c_str());
		//	rv = false;
		//	break;
		//}


		seekoffset = startoffset;
		SV_OFFSET_TYPE newoffset = 0;
		if(m_read_ahead_enabled)
		{
			newoffset = in_mem_seek(seekoffset,  BasicIo::BioBeg);
			if( newoffset != seekoffset )
			{
				if(!read_ahead(m_start, m_read_ahead_length))
				{
					DebugPrintf(SV_LOG_ERROR, "read_ahead() failed with error: %d for file %s\n",
						ACE_OS::last_error(), m_filepath.c_str());
					rv = false;
					break;
				}
				newoffset = in_mem_seek(seekoffset,  BasicIo::BioBeg);
			}
		}
		else
		{
			newoffset = File::Seek(seekoffset,  BasicIo::BioBeg);
		}
		if(newoffset != seekoffset)
		{
			DebugPrintf(SV_LOG_ERROR, 
				"FUNCTION: %s seek on %s failed. expected: " LLSPEC " actual: " LLSPEC " \n",
				__FUNCTION__, File::GetName().c_str(), seekoffset, newoffset);
			rv = false;
			break;
		}

		while ( !done && (seekoffset < m_end) )
		{
			readin = read((char*)&prefix, SVD_PREFIX_SIZE);

			if (SVD_PREFIX_SIZE != readin)
			{
				stringstream l_stdfatal;
				l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
					<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
					<< "\n" << File::GetName() << "is corrupt.\n" 
					<< "Prefix could not be be read at offset " << tell() << "\n"
					<< " Expected Read Bytes: " << SVD_PREFIX_SIZE << "\n"
					<< " Actual Read Bytes: "  << readin << "\n"
					<< " Error Code: " << File::LastError() << "\n"
					<< " Error Message: " << Error::Msg(File::LastError()) << "\n";

				DebugPrintf(SV_LOG_FATAL, "%s", l_stdfatal.str().c_str());
				rv = false;
				break;
			}

			if( m_svdConvertor.get() == NULL )
			{
				SV_UINT FiledataFormat = dataFormatFlags;
				if(prefix.tag != SVD_TAG_TIME_STAMP_OF_FIRST_CHANGE_V2)
				{
					if(dataFormatFlags == SVD_TAG_BEFORMAT)
						FiledataFormat = INMAGE_MAKEFOURCC('L','T','R','D');
					else
						FiledataFormat = INMAGE_MAKEFOURCC('B','T','T','D');
				}

				ConvertorFactory::getSVDConvertor( FiledataFormat, m_svdConvertor ) ;
			}

			m_svdConvertor->convertPrefix( prefix ) ;

			stringstream l_stdfatal;
			switch (prefix.tag)
			{

			case SVD_TAG_TIME_STAMP_OF_FIRST_CHANGE_V2:

				tagname = SVD_TAG_TSFC_V2_NAME;
				ANNOUNCE_TAG(tagname);
				rv = read_tsfc(metadata);
				break;

			case SVD_TAG_TIME_STAMP_OF_LAST_CHANGE_V2:

				tagname = SVD_TAG_TSLC_V2_NAME;
				ANNOUNCE_TAG(tagname);
				rv = read_tslc(metadata);
				break;

			case SVD_TAG_OTHR:

				tagname = SVD_TAG_OTHR_NAME;
				ANNOUNCE_TAG(tagname);
				rv = read_othr(metadata);
				break;

			case SVD_TAG_FOST:

				tagname = SVD_TAG_FOST_NAME;
				ANNOUNCE_TAG(tagname);
				rv = read_fost(prefix, metadata);
				break;

			case SVD_TAG_CDPD:

				tagname = SVD_TAG_CDPD_NAME;
				ANNOUNCE_TAG(tagname);
				rv = read_cdpd(prefix, metadata);
				break;

			case SVD_TAG_USER:

				tagname = SVD_TAG_USER_NAME;
				ANNOUNCE_TAG(tagname);
				rv = read_user(prefix, metadata,dataFormatFlags);
				break;

			case SVD_TAG_SOST:

				tagname = SVD_TAG_SOST_NAME;
				ANNOUNCE_TAG(tagname);
				rv = read_sost(skip);
				done = true;
				break;

			case SVD_TAG_SKIP:

				tagname = SVD_TAG_SKIP_NAME;
				ANNOUNCE_TAG(tagname);
				rv = skip_bytes(prefix);
				break;

				// Unkown prefix
			default:

				l_stdfatal << "Error detected  " << "in Function:" << FUNCTION_NAME 
					<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
					<< "\n" << File::GetName()   << "is corrupt.\n" 
					<< "Encountered an unknown tag: " << prefix.tag << " at offset " 
					<< tell() << "\n";

				DebugPrintf(SV_LOG_FATAL, "%s", l_stdfatal.str().c_str());
				rv = false;
				break;
			}

			if ( !rv )
			{
				break;
			}
			seekoffset = tell();		
		}

		if(rv)
			m_start = seekoffset;

		if(m_end == m_start)
			close();


	} while (0);

#if 0

	if(rv)
	{
		stringstream ss ;
		ss << metadata << endl;
		DebugPrintf(SV_LOG_DEBUG, "%s", ss.str().c_str());
	}

#endif


	DebugPrintf(SV_LOG_DEBUG,"EXITED %s %s\n",FUNCTION_NAME, m_filepath.c_str());
	return ((rv)?SVS_OK:SVE_FAIL);
}

bool cdpv3metadatafile_t::get_lowestseq_gr_eq_ts(const SV_ULONGLONG & ts, SV_ULONGLONG & seq)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s %s\n",FUNCTION_NAME, m_filepath.c_str());

	bool found = false;
	SVERROR hr = SVE_FAIL;
	cdpv3metadata_t metadata;

	while((hr = read_metadata_asc(metadata)) == SVS_OK)
	{
		if(metadata.m_tslc < ts)
			continue;

		if(!((metadata.m_type & DIFFTYPE_PERIO_BIT_SET) &&
			(metadata.m_type & DIFFTYPE_DIFFSYNC_BIT_SET) &&
			(metadata.m_type & DIFFTYPE_NONSPLITIO_BIT_SET)))
		{
			seq = 0;
			found = true;
			break;
		}

		if(metadata.m_tsfc >= ts)
		{
			seq = metadata.m_tsfcseq;
			found = true;
			break;
		}


		cdp_drtdv2s_constiter_t drtd_iter = metadata.m_drtds.begin();
		cdp_drtdv2s_constiter_t drtd_end = metadata.m_drtds.end();
		for( ; drtd_iter != drtd_end ; ++drtd_iter)
		{
			if(drtd_iter -> get_timedelta() + metadata.m_tsfc >= ts)
			{
				seq = (drtd_iter ->get_seqdelta() + metadata.m_tsfcseq);
				found = true;
				break;
			}
		}

		if(found)
			break;

		if(metadata.m_tslc >= ts)
		{
			seq = metadata.m_tslcseq;
			found = true;
			break;
		}
	}

	if (hr.succeeded() && !found){
		seq = metadata.m_tslcseq;
		found = true;
	}

	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s %s\n",FUNCTION_NAME, m_filepath.c_str());
	return found;
}

bool cdpv3metadatafile_t::read_tsfc(cdpv3metadata_t & metadata)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool rv = false;
	int readin = 0;

	// Read the timestamp chunk
	SVD_TIME_STAMP_V2 tsfc;
	readin = read((char*)&(tsfc),SVD_TIMESTAMP_V2_SIZE);
	m_svdConvertor->convertTimeStampV2( tsfc );

	if (SVD_TIMESTAMP_V2_SIZE == readin)
	{
		ANNOUNCE_SVDATA(tsfc);
		metadata.m_tsfc = tsfc.TimeInHundNanoSecondsFromJan1601;
		metadata.m_tsfcseq = tsfc.SequenceNumber;
		rv = true;
	}

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}

bool cdpv3metadatafile_t::read_tslc(cdpv3metadata_t & metadata)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool rv = false;
	int readin = 0;

	// Read the timestamp chunk
	SVD_TIME_STAMP_V2 tslc;
	readin = read((char*)&(tslc),SVD_TIMESTAMP_V2_SIZE);
	m_svdConvertor->convertTimeStampV2( tslc );

	if (SVD_TIMESTAMP_V2_SIZE == readin)
	{
		ANNOUNCE_SVDATA(tslc);
		metadata.m_tslc = tslc.TimeInHundNanoSecondsFromJan1601;
		metadata.m_tslcseq = tslc.SequenceNumber;
		rv = true;
	}

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}


bool cdpv3metadatafile_t::read_othr(cdpv3metadata_t & metadata)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool rv = false;
	int readin = 0;

	SVD_OTHR_INFO othr;
	readin = read((char*)&(othr),SVD_OTHR_INFO_SIZE);
	m_svdConvertor->convertOthrInfo( othr );

	if (SVD_OTHR_INFO_SIZE == readin)
	{
		//ANNOUNCE_SVDATA(othr);
		metadata.m_type = othr.type;
		rv = true;
	}

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}

bool cdpv3metadatafile_t::skip_bytes(const SVD_PREFIX & prefix)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool rv = true;
	// skip specified bytes
	seek(prefix.count, BasicIo::BioCur);

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}

bool cdpv3metadatafile_t::read_user(const SVD_PREFIX & prefix, cdpv3metadata_t & metadata, SV_UINT& dataFormatFlags)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool rv = true;
	int readin = 0;

	for( SV_UINT count = 0; count < prefix.count ; ++count)
	{
		boost::shared_array<char> buffer(new (nothrow) char[prefix.Flags]);
		if ( !buffer )
		{
			rv = false;
			break;
		}
		readin = read(buffer.get(), prefix.Flags);
		if ( prefix.Flags != readin )
		{
			rv = false;
			break;
		}
		boost::shared_ptr<SV_MARKER> marker(new (nothrow) SV_MARKER(buffer, prefix.Flags));

		SV_UINT EndianFormat = dataFormatFlags;
		rv = marker ->Parse(EndianFormat);
		if(!rv)
		{
			if(EndianFormat == SVD_TAG_BEFORMAT)
				EndianFormat = SVD_TAG_LEFORMAT;
			else
				EndianFormat = SVD_TAG_BEFORMAT;

			rv = marker ->Parse(EndianFormat);
			if(!rv)
			{
				std::stringstream l_stderr;
				l_stderr << "Error detected in Function: " << FUNCTION_NAME 
					<< " @ Line:" << LINE_NO << " FILE:" << FILE_NAME << "\n\n"
					<< "\nEncountered unparsable bookmark in file " << File::GetName() << "\n";
				DebugPrintf(SV_LOG_ERROR, "%s", l_stderr.str().c_str());
			}
		}

		if(rv)
		{
			metadata.m_users.push_back(marker);
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}

bool cdpv3metadatafile_t::read_fost(const SVD_PREFIX & prefix, cdpv3metadata_t & metadata, const SV_ULONGLONG & time_to_recover)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool rv = true;
	int readin = 0;

	for( SV_UINT count = 0; count < prefix.count ; ++count)
	{

		SVD_FOST_INFO fost;
		readin = read((char*)&(fost),SVD_FOST_INFO_SIZE);
		m_svdConvertor->convertFostInfo( fost );


		if (SVD_FOST_INFO_SIZE == readin)
		{
			//ANNOUNCE_SVDATA(fost);
			metadata.m_filenames.push_back(fost.filename);
			metadata.m_baseoffsets.push_back(fost.startoffset);
			metadata.m_byteswritten.push_back(0);

			// when performing a rollback operation, some of drtd belonging to certains data files
			// can be skipped to perform faster rollback
			// m_files_to_skip holds those data file ids.
			if(time_to_recover)
			{
				SV_TIME datafile_start_svtime = { 0 };
				SV_ULONGLONG datafile_start_ts = 0;
				std::string filename = fost.filename;
				datafile_start_svtime.wYear = atoi(filename.substr(22,2).c_str()) + 2009;
				datafile_start_svtime.wMonth = atoi(filename.substr(24,2).c_str());
				datafile_start_svtime.wDay = atoi(filename.substr(26,2).c_str());
				datafile_start_svtime.wHour = atoi(filename.substr(28,2).c_str());
				if(datafile_start_svtime.wMonth == 0) datafile_start_svtime.wMonth = 1;
				if(datafile_start_svtime.wDay == 0) datafile_start_svtime.wDay = 1;

				if(!CDPUtil::ToFileTime(datafile_start_svtime, datafile_start_ts))
				{
					DebugPrintf(SV_LOG_ERROR, "FUNCTION:%s unable to find the datafile start time for %s.\n",
						FUNCTION_NAME, fost.filename);
					rv = false;
					break;
				}

				if(datafile_start_ts > time_to_recover)
				{
					DebugPrintf(SV_LOG_DEBUG, "skipping %s\n", fost.filename);
					m_files_to_skip.push_back(metadata.m_filenames.size() -1);
				}
			}
		} else
		{
			rv = false;
			break;
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}

bool cdpv3metadatafile_t::read_cdpd(const SVD_PREFIX & prefix, cdpv3metadata_t & metadata)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool rv = true;
	int readin = 0;

	for( SV_UINT count = 0; count < prefix.count ; ++count)
	{

		SVD_CDPDRTD svdrtd;
		readin = read((char*)&(svdrtd),SVD_CDPDRTD_SIZE);
		m_svdConvertor->convertCdpDrtd( svdrtd );

		if (SVD_CDPDRTD_SIZE == readin)
		{

			// when performing a rollback operation, some of drtd belonging to certains data files
			// can be skipped to perform faster rollback
			// m_files_to_skip holds those data file ids.
			if(find(m_files_to_skip.begin(), m_files_to_skip.end(),svdrtd.fileid) == m_files_to_skip.end())
			{

				cdp_drtdv2_t drtd(svdrtd.length,
					svdrtd.voloffset,
					(metadata.m_baseoffsets[svdrtd.fileid] + metadata.m_byteswritten[svdrtd.fileid]),
					svdrtd.uiSequenceNumberDelta,
					svdrtd.uiTimeDelta,
					svdrtd.fileid);

				metadata.m_drtds.push_back(drtd);
				metadata.m_byteswritten[svdrtd.fileid] += svdrtd.length;
			}
		} else
		{
			rv = false;
			break;
		}
	}

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}

bool cdpv3metadatafile_t::read_sost(SV_OFFSET_TYPE & startoffset)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	bool rv = false;
	int readin = 0;

	// fetch the starting offset
	SV_LONGLONG offset;
	readin = read((char*)&(offset),SVD_SOST_SIZE);
	m_svdConvertor-> convertLONGLONG( offset );

	if (SVD_SOST_SIZE == readin)
	{
		ANNOUNCE_SVDATA(offset);
		startoffset = offset;
		rv = true;
	}

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return rv;
}

cdp_io_stats_t cdpv3metadatafile_t::get_io_stats()
{
	cdp_io_stats_t stats;
	
	stats.read_io_count = m_io_count;
	stats.read_bytes_count = m_io_bytes;

	return stats;
}