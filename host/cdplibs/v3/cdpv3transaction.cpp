//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File:       cdpv3transaction.cpp
//
// Description: 
//

#include "cdpv3transaction.h"
#include "portablehelpers.h"
#include "sharedalignedbuffer.h"
#include "inmsafecapis.h"

cdpv3txnfile_t::cdpv3txnfile_t(const std::string & filepath)
:m_filepath(filepath)
{

}

cdpv3txnfile_t::~cdpv3txnfile_t()
{

}

bool cdpv3txnfile_t::read(cdpv3transaction_t& txn, SV_ULONGLONG sectorsize, bool useunbufferedio)
{
	//
	// if file does not exist
	//  prepare an empty initialized entry and provide
	// else
	//  read entry from file
	// 

	bool rv = false;
	ACE_HANDLE handle = ACE_INVALID_HANDLE;

	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s filepath: %s\n",
		FUNCTION_NAME, m_filepath.c_str());

	do
	{
		ACE_stat statbuf = {0};

		ACE_OS::last_error(0);
		if( 0 != sv_stat(getLongPathName(m_filepath.c_str()).c_str(), &statbuf) )
		{
			int lasterror = ACE_OS::last_error();
			if(ENOENT == lasterror)
			{
				memset(&txn,'\0',sizeof(cdpv3transaction_t));
				txn.m_hdr.m_magic = CDPV3_TXN_MAGIC;
				txn.m_hdr.m_version = CDPV3_TXN_VERSION;
				rv = true;
				break;
			}else
			{
				DebugPrintf(SV_LOG_ERROR, "stat failed with error %d for %s\n", lasterror, m_filepath.c_str());
				rv = false;
				break;
			}
		}else
		{
			int openmode = O_RDONLY;

			if(useunbufferedio)
			{
				setdirectmode(openmode);
			}

			setumask();

			//Setting appropriate permission on transaction file
			mode_t perms;
			setsharemode_for_all(perms);

			handle = ACE_OS::open(getLongPathName(m_filepath.c_str()).c_str(),
				openmode,perms);

			if(ACE_INVALID_HANDLE == handle)
			{
				DebugPrintf(SV_LOG_ERROR, "FUNCTION: %s unable to open %s. error:%d\n",
					FUNCTION_NAME, m_filepath.c_str(), ACE_OS::last_error());
				rv = false;
				break;
			}

			SharedAlignedBuffer alignedbuf(sizeof(cdpv3transaction_t),sectorsize);

			int bytesread = ACE_OS::read(handle, alignedbuf.Get(), sizeof(cdpv3transaction_t));
			if(bytesread != sizeof(cdpv3transaction_t))
			{
				DebugPrintf(SV_LOG_ERROR, "FUNCTION: %s read from %s returned %u bytes. error:%d\n",
					FUNCTION_NAME, 
					m_filepath.c_str(), 
					bytesread,
					ACE_OS::last_error());
				rv = false;
				break;
			}

			inm_memcpy_s(&txn, sizeof(txn), alignedbuf.Get(),sizeof(cdpv3transaction_t));
			if(txn.m_hdr.m_magic == CDPV3_TXN_MAGIC)
			{
				rv = true;
				break;
			}else
			{
				DebugPrintf(SV_LOG_ERROR, "FUNCTION: %s filepath: %s magic bytes do not match.\n",
					FUNCTION_NAME,
					m_filepath.c_str());
				rv = false;
				break;
			}
		}

	} while (0);

	if(ACE_INVALID_HANDLE != handle)
		ACE_OS::close(handle);


	DebugPrintf(SV_LOG_DEBUG, "EXITED %s filepath: %s\n",
		FUNCTION_NAME, m_filepath.c_str());
	return rv;
}


bool cdpv3txnfile_t::write(const cdpv3transaction_t& txn, SV_ULONGLONG sectorsize, bool useunbufferedio)
{

	bool rv = true;
	ACE_HANDLE handle = ACE_INVALID_HANDLE;

	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s filepath: %s\n",
		FUNCTION_NAME, m_filepath.c_str());

	do
	{

		int openmode = O_WRONLY | O_CREAT;
		if(useunbufferedio)
		{
			setdirectmode(openmode);
		}

		setumask();

		//Setting appropriate permission on transaction file
		mode_t perms;
		setsharemode_for_all(perms);

		if(txn.m_hdr.m_magic != CDPV3_TXN_MAGIC)
		{
			DebugPrintf(SV_LOG_ERROR, "FUNCTION: %s filepath: %s unexpected magic bytes in input.\n",
				FUNCTION_NAME,
				m_filepath.c_str());
			rv = false;
			break;
		}

		handle = ACE_OS::open(getLongPathName(m_filepath.c_str()).c_str(),
			openmode,perms);

		if(ACE_INVALID_HANDLE == handle)
		{
			DebugPrintf(SV_LOG_ERROR, "FUNCTION: %s unable to open %s. error:%d\n",
				FUNCTION_NAME, m_filepath.c_str(), ACE_OS::last_error());
			rv = false;
			break;
		}

		SharedAlignedBuffer alignedbuf(sizeof(cdpv3transaction_t),sectorsize);
		inm_memcpy_s(alignedbuf.Get(), alignedbuf.Size(),&txn, sizeof(cdpv3transaction_t));

		ssize_t byteswrote = ACE_OS::write(handle, alignedbuf.Get(), sizeof(cdpv3transaction_t));
		if(byteswrote != sizeof(cdpv3transaction_t))
		{
			DebugPrintf(SV_LOG_ERROR, "FUNCTION: %s write to %s returned %u bytes. error:%d\n",
				FUNCTION_NAME, 
				m_filepath.c_str(), 
				byteswrote,
				ACE_OS::last_error());
			rv = false;
			break;

        }

        if(!useunbufferedio && (ACE_OS::fsync(handle) < 0))
		{
			DebugPrintf(SV_LOG_ERROR, "FUNCTION: %s fsync failed on %s. error:%d\n",
				FUNCTION_NAME, 
				m_filepath.c_str(),
				ACE_OS::last_error());
			rv = false;
			break;
		}

	} while (0);

	if(ACE_INVALID_HANDLE != handle)
		ACE_OS::close(handle);


	DebugPrintf(SV_LOG_DEBUG, "EXITED %s filepath: %s\n",
		FUNCTION_NAME, m_filepath.c_str());
	return rv;
}

std::string cdpv3txnfile_t::getfilepath(const std::string & folder)
{
	std::stringstream filepath;
	filepath << folder
		<< ACE_DIRECTORY_SEPARATOR_CHAR_A
		<< CDPV3TXNFILENAME;
	return filepath.str();
}

