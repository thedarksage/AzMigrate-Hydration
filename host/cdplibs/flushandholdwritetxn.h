#ifndef FLUSHANDHOLDWRITETXN__H
#define FLUSHANDHOLDWRITETXN__H

#include <iostream>
#include <string>
#include "svtypes.h"
#include "cdpglobals.h"

#if defined(SV_SUN) || defined(SV_AIX)
#pragma pack( 1 )
#else
#pragma pack( push, 1 )
#endif


#define FLUSH_AND_HOLD_TXN_MAGIC INMAGE_MAKEFOURCC( 'F', 'A', 'H', 'T')
#define FLUSH_AND_HOLD_TXN_VERSION 1

#define FLUSH_AND_HOLD_TXNFILENAME "flush_and_hold_txn.tdat"

struct FlushAndHoldTxnHeader
{
	unsigned int m_magic;
	unsigned int m_version;
};

struct FlushAndHoldRedoLogStatus
{
	unsigned int m_status;
	unsigned int m_filecount;
};

struct FlushAndHoldUndoVolumeStatus
{
	unsigned int m_status;
};

struct FlushAndHoldReplayLogStatus
{
	unsigned int m_status;
};

struct FlushAndHoldTxn{
	FlushAndHoldTxnHeader m_hdr;
	FlushAndHoldRedoLogStatus m_redolog_status;
	FlushAndHoldUndoVolumeStatus m_undovolume_status;
	FlushAndHoldReplayLogStatus m_replaylog_status;
};

#if defined(SV_SUN) || defined(SV_AIX)
#pragma pack()
#else
#pragma pack( pop )
#endif 

class FlushAndHoldTxnFile{
public:
	enum TxnStatus{
		NOT_STARTED = 0,
		INPROGRESS,
		COMPLETE,
		FAILED
	};
	FlushAndHoldTxnFile(std::string &filename):
		m_filename(filename)
	{
		m_txn.m_hdr.m_magic = FLUSH_AND_HOLD_TXN_MAGIC;
		m_txn.m_hdr.m_version = FLUSH_AND_HOLD_TXN_VERSION;

		m_txn.m_redolog_status.m_status = NOT_STARTED;
		m_txn.m_redolog_status.m_filecount = 0;

		m_txn.m_undovolume_status.m_status = NOT_STARTED;
		m_txn.m_replaylog_status.m_status = NOT_STARTED;
	}

	~FlushAndHoldTxnFile(){}

	bool parse();

	bool is_redolog_creation_complete();
	bool is_redolog_creation_failed();

	bool is_undo_volume_changes_complete();
	bool is_undo_volume_changes_failed();

	bool is_replaylog_to_volume_complete();
	bool is_replaylog_to_volume_failed();

	SV_UINT get_cdp_redolog_count();

	void set_redolog_status(SV_UINT status,SV_UINT file_count);
	void set_undo_volume_status(SV_UINT status);
	void set_replaylog_status(SV_UINT status);

	bool persist_status();
	
private:
	FlushAndHoldTxn m_txn;
	std::string m_filename;
};

#endif
