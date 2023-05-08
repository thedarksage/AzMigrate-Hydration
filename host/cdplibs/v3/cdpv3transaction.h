//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File:       cdpv3transaction.h
//
// Description: 
//

#ifndef CDPV3TRANSACTION__H
#define CDPV3TRANSACTION__H

#include "svdparse.h"
#include <string>

#if defined(SV_SUN) || defined(SV_AIX)
#pragma pack( 1 )
#else
#pragma pack( push, 1 )
#endif 

#define CDPV3TXNHDR_RESERVED_BYTES 16
#define CDPV3TXNLASTUPDATE_RESERVED_BYTES 40
#define CDPV3TXNTIMESTAMP_RESERVED_BYTES 32
#define CDPV3TXN_RESERVED_BYTES 124

#define CDPV3DIFFNAME_SIZE 160
#define CDPV3MDNAME_SIZE 96

#define CDPV3_TXN_MAGIC INMAGE_MAKEFOURCC( 'T', 'X', 'N', '1')
#define CDPV3_TXN_VERSION 1

#define CDPV3TXNFILENAME "cdpv3_txn.tdat"

struct cdpv3txn_hdr_t
{
	unsigned int m_magic;
	unsigned int m_version;
	unsigned char reserved[CDPV3TXNHDR_RESERVED_BYTES];
};

struct cdpv3txn_lastupdate_t
{
	char m_diffname[CDPV3DIFFNAME_SIZE];
	char m_mdfilename[CDPV3MDNAME_SIZE];
	unsigned int m_partno;
	unsigned int m_mdsize;
	unsigned char reserved[CDPV3TXNLASTUPDATE_RESERVED_BYTES];
};

struct cdpv3txn_cdptimestamp_t
{
	unsigned long long m_seq;
	unsigned long long m_ts;
	unsigned char reserved[CDPV3TXNTIMESTAMP_RESERVED_BYTES];
};

struct cdpv3txn_bookmarks_t
{
	unsigned long long m_hr;
	unsigned int m_bkmkcount;
};

struct cdpv3transaction_t
{
	cdpv3txn_hdr_t m_hdr;
	cdpv3txn_lastupdate_t m_lastupdate;
	cdpv3txn_cdptimestamp_t m_ts;
	cdpv3txn_bookmarks_t m_bkmks;
    unsigned char reserved[CDPV3TXN_RESERVED_BYTES];
};

#if defined(SV_SUN) || defined(SV_AIX)
#pragma pack()
#else
#pragma pack( pop )
#endif 

class cdpv3txnfile_t
{
public:
	static std::string getfilepath(const std::string & folder);

	cdpv3txnfile_t(const std::string & filepath);

	~cdpv3txnfile_t();

	bool read(cdpv3transaction_t & txn, SV_ULONGLONG sectorsize, bool useunbufferedio);
	bool write(const cdpv3transaction_t & txn, SV_ULONGLONG sectorsize, bool useunbufferedio);

private:
	std::string m_filepath;
};





#endif
