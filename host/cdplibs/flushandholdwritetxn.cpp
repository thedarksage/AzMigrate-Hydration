#include <iostream>
#include <sstream>
#include <fstream>
#include <boost/lexical_cast.hpp>
#include "flushandholdwritetxn.h"
#include "cdpglobals.h"
#include "cdputil.h"

#include "sharedalignedbuffer.h"


bool FlushAndHoldTxnFile::parse()
{
	bool rv = true;
	ACE_stat filestat = {0};
	ACE_HANDLE handle = ACE_INVALID_HANDLE;

	do{
		if(sv_stat(m_filename.c_str(),&filestat) == 0)
		{
			int openmode = O_RDONLY;

			handle = ACE_OS::open(getLongPathName(m_filename.c_str()).c_str(),
				openmode);

			if(ACE_INVALID_HANDLE == handle)
			{
				DebugPrintf(SV_LOG_ERROR, "FUNCTION: %s unable to open %s. error:%d\n",
					FUNCTION_NAME, m_filename.c_str(), ACE_OS::last_error());
				rv = false;
				break;
			}

			int bytesread = ACE_OS::read(handle, &m_txn, sizeof(m_txn));
			if(bytesread != sizeof(m_txn))
			{
				DebugPrintf(SV_LOG_ERROR, "FUNCTION: %s read from %s returned %u bytes. error:%d\n",
					FUNCTION_NAME, 
					m_filename.c_str(), 
					bytesread,
					ACE_OS::last_error());
				rv = false;
				break;
			}

			if(m_txn.m_hdr.m_magic == FLUSH_AND_HOLD_TXN_MAGIC)
			{
				rv = true;
				break;
			}else
			{
				DebugPrintf(SV_LOG_ERROR, "FUNCTION: %s filepath: %s magic bytes do not match.\n",
					FUNCTION_NAME,
					m_filename.c_str());
				rv = false;
				break;
			}

			
		}
		else
		{
			m_txn.m_hdr.m_magic = FLUSH_AND_HOLD_TXN_MAGIC;
			m_txn.m_hdr.m_version = FLUSH_AND_HOLD_TXN_VERSION;

			m_txn.m_redolog_status.m_status = NOT_STARTED;
			m_txn.m_redolog_status.m_filecount = 0;

			m_txn.m_undovolume_status.m_status = NOT_STARTED;

			m_txn.m_replaylog_status.m_status = NOT_STARTED;
		}
	}while(0);

	if(handle != ACE_INVALID_HANDLE)
	{
		ACE_OS::close(handle);
	}

	return rv;
}

bool FlushAndHoldTxnFile::is_redolog_creation_complete()
{
	return (m_txn.m_redolog_status.m_status == COMPLETE);
}
bool FlushAndHoldTxnFile::is_redolog_creation_failed()
{
	return (m_txn.m_redolog_status.m_status == FAILED);
}

bool FlushAndHoldTxnFile::is_undo_volume_changes_complete()
{
	return (m_txn.m_undovolume_status.m_status == COMPLETE);
}
bool FlushAndHoldTxnFile::is_undo_volume_changes_failed()
{
	return (m_txn.m_undovolume_status.m_status == FAILED);
}

bool FlushAndHoldTxnFile::is_replaylog_to_volume_complete()
{
	return (m_txn.m_replaylog_status.m_status == COMPLETE);
}

bool FlushAndHoldTxnFile::is_replaylog_to_volume_failed()
{
	return (m_txn.m_replaylog_status.m_status == FAILED);
}

void FlushAndHoldTxnFile::set_redolog_status(SV_UINT status,SV_UINT filecount)
{
	m_txn.m_redolog_status.m_status = status;
	m_txn.m_redolog_status.m_filecount = filecount;
	persist_status();
}

void FlushAndHoldTxnFile::set_undo_volume_status(SV_UINT status)
{
	m_txn.m_undovolume_status.m_status = status;
	persist_status();
}

void FlushAndHoldTxnFile::set_replaylog_status(SV_UINT status)
{
	m_txn.m_replaylog_status.m_status = status;
	persist_status();
}

SV_UINT FlushAndHoldTxnFile::get_cdp_redolog_count()
{
	return m_txn.m_redolog_status.m_filecount;
}

bool FlushAndHoldTxnFile::persist_status()
{
	bool rv = true;
	ACE_HANDLE handle = ACE_INVALID_HANDLE;

	DebugPrintf(SV_LOG_DEBUG, "ENTERED %s filepath: %s\n",
		FUNCTION_NAME, m_filename.c_str());

	do
	{
		int openmode = O_WRONLY | O_CREAT;

		if(m_txn.m_hdr.m_magic != FLUSH_AND_HOLD_TXN_MAGIC)
		{
			DebugPrintf(SV_LOG_ERROR, "FUNCTION: %s filepath: %s unexpected magic bytes in input.\n",
				FUNCTION_NAME,
				m_filename.c_str());
			rv = false;
			break;
		}

		handle = ACE_OS::open(getLongPathName(m_filename.c_str()).c_str(),
			openmode);

		if(ACE_INVALID_HANDLE == handle)
		{
			DebugPrintf(SV_LOG_ERROR, "FUNCTION: %s unable to open %s. error:%d\n",
				FUNCTION_NAME, m_filename.c_str(), ACE_OS::last_error());
			rv = false;
			break;
		}

		ssize_t byteswrote = ACE_OS::write(handle, (char *)&m_txn, sizeof(m_txn));
		if(byteswrote != sizeof(m_txn))
		{
			DebugPrintf(SV_LOG_ERROR, "FUNCTION: %s write to %s returned %u bytes. error:%d\n",
				FUNCTION_NAME, 
				m_filename.c_str(), 
				byteswrote,
				ACE_OS::last_error());
			rv = false;
			break;

        }

        if(ACE_OS::fsync(handle) < 0)
		{
			DebugPrintf(SV_LOG_ERROR, "FUNCTION: %s fsync failed on %s. error:%d\n",
				FUNCTION_NAME, 
				m_filename.c_str(),
				ACE_OS::last_error());
			rv = false;
			break;
		}

	} while (0);

	if(ACE_INVALID_HANDLE != handle)
		ACE_OS::close(handle);


	DebugPrintf(SV_LOG_DEBUG, "EXITED %s filepath: %s\n",
		FUNCTION_NAME, m_filename.c_str());
	return rv;
}

