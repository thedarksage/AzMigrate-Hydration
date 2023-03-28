//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File:       cdpv1globals.h
//
// Description: 
//

#ifndef CDPGLOBALS__H
#define CDPGLOBALS__H

#include <map>
#include <vector>
#include <sstream>
#include <list>
#include <iomanip>

#include "svtypes.h"
#include "svmarker.h"
#include "cdpdrtd.h"
#include "svdparse.h"
#include "hostagenthelpers_ported.h"
#include "retentionsettings.h"
#include "retentioninformation.h"
#include "inmsafecapis.h"

const std::string CDPV1DBNAME = "cdpv1.db";
const std::string CDPV3DBNAME = "cdpv3.db";

#define ACCURACYMODE0   "Exact"
#define ACCURACYMODE1   "Approximate"
#define ACCURACYMODE2   "Not Guaranted"
#define ACCURACYMODE3   "Any"

#define ONE_MS                          10000
#define ONE_SEC                         10000000

#define USER_DEFAULT_ACTION_MSG " Default Action:Please ensure the directory is available and has required permissions."

int const CDP_LOOKUPPAGESIZE =            1048576;

int const RESYNC_MODE  = 0;
int const NONSPLIT_IO  = 1;
int const SPLIT_IO     = 2;
int const OUTOFORDER_TS = 3;
int const OUTOFORDER_SEQ =4;

///
/// constants for naming a data file
///
#define CDPDATAFILEPREFIX      "cdpv"
#define CDPV1DATAFILEPREFIX    "cdpv1_"
#define CDPV2DATAFILEPREFIX    "cdpv2_"

#define CDPRESYNCDATAFILE    "resync_"
#define CDPDIFFSYNCDATAFILE  "diffsync_"
#define CDPNONSPLITIOFILE    "nonsplitio_"
#define CDPSPLITIOFILE    "splitio_"
#define CDPOUTOFORDERTSFILE   "oots_"
#define CDPOUTOFORDERSEQFILE  "ooseq_"
#define CDPOUTOFORDERTSFILEOBS   "outoforderts_"
#define CDPOUTOFORDERSEQFILEOBS  "outoforderseq_"

// differential filename versions
#define DIFFIDVERSION1 1
#define DIFFIDVERSION2 2
#define DIFFIDVERSION3 3

///
/// differential file name prefix
///
#define diffMatchPrefix "completed_ediffcompleted_diff"


enum QUERYOP { CDPEVENT, CDPTIMERANGE, CDPDRTD, CDPAPPLIEDDRTD };
const std::string OPERATION[] = {"Event", "Time Range", "Drtd", "Applied DRTD"};

#define APPLIED_INFO_HDR_MAGIC 			INMAGE_MAKEFOURCC( 'A', 'P', 'I', 'F' )
#define APPLIED_INFO_VERSION 			1
#define APPLIED_INFO_CHAR_ARRAY_LEN		252

#define ANNOUNCE_SVDATA(x)   \
{                            \
	std::stringstream l_stdout;   \
	l_stdout << (x) ;        \
	DebugPrintf(SV_LOG_DEBUG, "%s", l_stdout.str().c_str()); \
}


//
// epoch difference between unix and windows base reference
//
#define SVEPOCHDIFF (static_cast<SV_ULONGLONG>(116444736) * static_cast<SV_ULONGLONG>(1000000000))


class CDPTimeRangeMatchingCndn
{
public:
	CDPTimeRangeMatchingCndn() : 
	  m_mode(ACCURACYANY), m_sortcol("default"), m_sortorder("default"), m_limit(0)
	  {
	  }

	  inline CDPTimeRangeMatchingCndn & mode(SV_USHORT mode) { m_mode = mode; return (*this); }
	  inline CDPTimeRangeMatchingCndn & sortby(const std::string & col) { m_sortcol = col; return (*this); }
	  inline CDPTimeRangeMatchingCndn & sortorder(const std::string & order) { m_sortorder = order; return (*this);}
	  inline CDPTimeRangeMatchingCndn & limit(SV_ULONGLONG num)         { m_limit = num; return (*this); }


	  inline SV_USHORT mode() const { return m_mode; }
	  inline std::string sortby() const { return m_sortorder; }
	  inline std::string sortorder() const { return m_sortcol; }
	  inline SV_ULONGLONG limit()        const { return m_limit; }


private:
	SV_USHORT m_mode;
	std::string m_sortcol;
	std::string m_sortorder;
	SV_ULONGLONG  m_limit;

};
class CDPDRTDsMatchingCndn
{
public:

	CDPDRTDsMatchingCndn(): m_totime(0), m_sequenceid(0),m_fromtime(0)
	{
	}

	inline CDPDRTDsMatchingCndn & toTime(SV_ULONGLONG ts) { m_totime = ts; return (*this); }
	inline CDPDRTDsMatchingCndn & toSequenceId(SV_ULONGLONG seq) { m_sequenceid = seq; return (*this); }
	inline CDPDRTDsMatchingCndn & fromTime(SV_ULONGLONG fromtime) { m_fromtime = fromtime; return (*this); }
	inline CDPDRTDsMatchingCndn & toEvent(std::string event) { m_event = event; return (*this); }

	inline SV_ULONGLONG toTime()	const { return m_totime; }
	inline SV_ULONGLONG fromTime()	const { return m_fromtime; }
	inline SV_ULONGLONG toSequenceId()	const { return m_sequenceid; }
	inline std::string toEvent()	const { return m_event; }

private:

	SV_ULONGLONG	m_fromtime;
	SV_ULONGLONG	m_totime;
	SV_ULONGLONG	m_sequenceid;
	std::string		m_event;

};
class CDPMarkersMatchingCndn
{
public:
	CDPMarkersMatchingCndn()
		:m_type(0), m_attime(0), m_beforetime(0), m_aftertime(0), m_limit(0), m_mode(ACCURACYANY),m_verified(BOTH_VERIFIED_NONVERIFIED_EVENT),m_lockstatus(BOOKMARK_STATE_BOTHLOCKED_UNLOCKED)
	{
		m_value.clear();
		m_identifier.clear();
		m_comment.clear();
		m_sortcol = "default";
		m_sortorder = "default";
	}

	inline CDPMarkersMatchingCndn & type(SV_EVENT_TYPE type)         { m_type   = type; return (*this) ; }
	inline CDPMarkersMatchingCndn & value(const std::string & value)    { m_value  = value; return (*this) ; }
	inline CDPMarkersMatchingCndn & atTime(SV_ULONGLONG attime)    { m_attime = attime; return (*this) ; }
	inline CDPMarkersMatchingCndn & beforeTime(SV_ULONGLONG beforetime) { m_beforetime = beforetime; return (*this) ; }
	inline CDPMarkersMatchingCndn & afterTime(SV_ULONGLONG aftertime) { m_aftertime = aftertime; return (*this) ; }
	inline CDPMarkersMatchingCndn & mode(SV_USHORT mode) { m_mode = mode; return (*this) ; }
	inline CDPMarkersMatchingCndn & sortby(const std::string & col)        { m_sortcol = col ; return (*this) ; }
	inline CDPMarkersMatchingCndn & sortorder(const std::string & order)        { m_sortorder = order ; return (*this) ; }
	inline CDPMarkersMatchingCndn & limit(SV_ULONGLONG num)         { m_limit = num; return (*this); }
	inline CDPMarkersMatchingCndn & identifier(const std::string & value)    { m_identifier  = value; return (*this) ; }
	inline CDPMarkersMatchingCndn & setverified(SV_USHORT verified)    { m_verified  = verified; return (*this) ; }
	inline CDPMarkersMatchingCndn & comment(const std::string & value)    { m_comment  = value; return (*this) ; }
	inline CDPMarkersMatchingCndn & setlockstatus(SV_USHORT lockstatus)    { m_lockstatus  = lockstatus; return (*this) ; }

	inline SV_EVENT_TYPE  type()	       const { return m_type ; }
	inline std::string       value()        const { return m_value ;   }
	inline SV_ULONGLONG atTime()       const { return m_attime ;  }
	inline SV_ULONGLONG beforeTime()   const { return m_beforetime ; }
	inline SV_ULONGLONG afterTime()    const { return m_aftertime  ; }
	inline SV_USHORT    mode()         const { return m_mode ; }
	inline std::string       sortby()       const { return m_sortcol ; }
	inline std::string       sortorder()    const { return m_sortorder ; }
	inline SV_ULONGLONG limit()        const { return m_limit; }
	inline std::string       identifier()        const { return m_identifier ;   }
	inline SV_USHORT verified() const { return m_verified;}
	inline std::string comment() const { return m_comment;}
	inline SV_USHORT lockstatus() const { return m_lockstatus;}

protected:

private:

	SV_EVENT_TYPE   m_type;
	std::string        m_value;
	SV_ULONGLONG  m_beforetime;
	SV_ULONGLONG  m_aftertime;
	SV_ULONGLONG  m_attime;
	SV_USHORT     m_mode;
	std::string        m_identifier;
	std::string        m_sortcol;
	std::string        m_sortorder;
	SV_ULONGLONG  m_limit;
	std::string		   m_comment;
	SV_USHORT m_verified;
	SV_USHORT m_lockstatus;
};

//
// Differential Structure
// 
struct Differential
{


	Differential()
	{
		ZeroFill(m_start);
		ZeroFill(m_end);
		ZeroFill(m_hdr);
		ZeroFill(m_tsfc);
		ZeroFill(m_tslc);
		ZeroFill(m_lodc);
		m_users.clear();
		m_drtds.clear();
		m_continuationId = 0;
		m_type = RESYNC_MODE;
		m_baseline = SV_WIN_OS;
		m_idVersion = DIFFIDVERSION1;
	}

	void initialize()
	{
		ZeroFill(m_start);
		ZeroFill(m_end);
		ZeroFill(m_hdr);
		ZeroFill(m_tsfc);
		ZeroFill(m_tslc);
		ZeroFill(m_lodc);
		m_users.clear();
		m_drtds.clear();
		m_continuationId = 0;
		m_type = RESYNC_MODE;
		m_baseline = SV_WIN_OS;
		m_idVersion = DIFFIDVERSION1;
	}

	void StartOffset(const SV_OFFSET_TYPE & start) 
	{
		m_start = start ;
	}

	SV_OFFSET_TYPE  StartOffset()
	{ 
		return m_start ; 
	}

	void EndOffset(const SV_OFFSET_TYPE & end)     
	{
		m_end = end ;
	}

	SV_OFFSET_TYPE  EndOffset() 
	{
		return m_end ; 
	}

	SV_ULONGLONG size()  
	{
		return (m_end > m_start)?(m_end - m_start):(m_start - m_end); 
	}

	void Hdr(const SVD_HEADER1 & hdr)
	{
		inm_memcpy_s((char*)(&m_hdr) + SVD_VERSION_SIZE, (sizeof(m_hdr) - SVD_VERSION_SIZE), &hdr, SVD_HEADER1_SIZE);
	}

	void Hdr(const SVD_HEADER_V2 & hdr)
	{
		inm_memcpy_s((char*)(&m_hdr), sizeof(m_hdr), &hdr, SVD_HEADER2_SIZE);
	}

	SVD_HEADER_V2 getHdr()
	{
		return m_hdr;
	}

	void version(const SV_UINT & major,  const SV_UINT & minor)
	{
		m_hdr.Version.Major = SVD_PERIOVERSION_MAJOR;
		m_hdr.Version.Minor = SVD_PERIOVERSION_MINOR;
	}

	SV_UINT version()                 
	{
		return SVD_VERSION(m_hdr.Version.Major, m_hdr.Version.Minor); 
	}

	SV_ULONGLONG ResyncCounter()
	{
		//OOD_DESIGN1//return m_hdr.resyncCtr;
		SV_ULONGLONG resyncCounter = 0;

		resyncCounter = m_hdr.SVId.Data1;
		resyncCounter = (resyncCounter << 16) + m_hdr.SVId.Data2;
		resyncCounter = (resyncCounter << 16) + m_hdr.SVId.Data3;

		return resyncCounter;
	}

	SV_ULONGLONG DirtyBlockCounter()
	{
		//OOD_DESIGN1//return m_hdr.dirtyBlockCtr;
		SV_ULONGLONG dirtyBlockCounter = 0;

		dirtyBlockCounter = (SV_ULONGLONG)m_hdr.SVId.Data4;

		return dirtyBlockCounter;
	}

	std::string getSourceHostId()
	{
		std::stringstream originHost;

		//OOD_DESIGN1//unsigned char *ptr = (unsigned char *)&m_hdr.OriginHostId;
		unsigned char *ptr = (unsigned char *)&m_hdr.OriginHost;

		for(int i = 0; i < 4; i++)
			originHost << std::setfill('0') << std::setw(2) << std::hex << (unsigned int)*ptr++;
		originHost << "-";

		for(int i = 0; i < 2; i++)
			originHost << std::setfill('0') << std::setw(2) << std::hex << (unsigned int)*ptr++;
		originHost << "-";

		for(int i = 0; i < 2; i++)
			originHost << std::setfill('0') << std::setw(2) << std::hex << (unsigned int)*ptr++;
		originHost << "-";

		for(int i = 0; i < 2; i++)
			originHost << std::setfill('0') << std::setw(2) << std::hex << (unsigned int)*ptr++;
		originHost << "-";

		for(int i = 0; i < 6; i++)
			originHost << std::setfill('0') << std::setw(2) << std::hex << (unsigned int)*ptr++;

		return originHost.str();
	}

	std::string getSourceVolumeNameHash()
	{
		std::stringstream originVolume;

		/*OOD_DESIGN1
		unsigned char *ptr = (unsigned char *)&m_hdr.OriginVolumeId;

		for(int i = 0; i < sizeof(m_hdr.OriginVolumeId); i++)*/
		unsigned char *ptr = (unsigned char *)&m_hdr.OriginVolume;

		for(int i = 0; i < sizeof(m_hdr.OriginVolume); i++)
		{
			originVolume << (unsigned char)*ptr++;
		}

		return originVolume.str();
	}


	void StartTime(const SVD_TIME_STAMP & tsfc)
	{
		m_tsfc.Header = tsfc.Header;
		m_tsfc.SequenceNumber = tsfc.ulSequenceNumber;
		m_tsfc.TimeInHundNanoSecondsFromJan1601 = tsfc.TimeInHundNanoSecondsFromJan1601;
	}

	void StartTime(const SVD_TIME_STAMP_V2 & tsfc)
	{
		m_tsfc.Header = tsfc.Header;
		m_tsfc.SequenceNumber = tsfc.SequenceNumber;
		m_tsfc.TimeInHundNanoSecondsFromJan1601 = tsfc.TimeInHundNanoSecondsFromJan1601;
	}

	SVD_TIME_STAMP_V2 getStartTime()
	{
		return m_tsfc;
	}

	void StartTime(const SV_ULONGLONG & ts)
	{
		m_tsfc.TimeInHundNanoSecondsFromJan1601 = ts;
	}

	void StartTimeSequenceNumber(const SV_ULONGLONG & seq)
	{
		m_tsfc.SequenceNumber = seq;
	}

	void StartTimeSequenceNumber(const SV_ULONG & seq)
	{
		m_tsfc.SequenceNumber = seq;
	}

	SV_ULONGLONG starttime()               
	{ 
		return m_tsfc.TimeInHundNanoSecondsFromJan1601 ; 
	}

	SV_ULONGLONG StartTimeSequenceNumber() 
	{
		return m_tsfc.SequenceNumber ;
	}

	void EndTime(const SVD_TIME_STAMP & tslc)
	{
		m_tslc.Header = tslc.Header;
		m_tslc.SequenceNumber = tslc.ulSequenceNumber;
		m_tslc.TimeInHundNanoSecondsFromJan1601 = tslc.TimeInHundNanoSecondsFromJan1601;
	}

	void EndTime(const SVD_TIME_STAMP_V2 & tslc)
	{
		m_tslc.Header = tslc.Header;
		m_tslc.SequenceNumber = tslc.SequenceNumber;
		m_tslc.TimeInHundNanoSecondsFromJan1601 = tslc.TimeInHundNanoSecondsFromJan1601;
	}

	SVD_TIME_STAMP_V2 getEndTime()
	{
		return m_tslc;
	}

	void EndTime(const SV_ULONGLONG & ts)
	{
		m_tslc.TimeInHundNanoSecondsFromJan1601 = ts;
	}

	void EndTimeSequenceNumber(const SV_ULONGLONG & seq)
	{
		m_tslc.SequenceNumber = seq;
	}

	void EndTimeSequenceNumber(const SV_ULONG & seq)
	{
		m_tslc.SequenceNumber = seq;
	}

	SV_ULONGLONG endtime()                 
	{ 
		return m_tslc.TimeInHundNanoSecondsFromJan1601 ; 
	}

	SV_ULONGLONG EndTimeSequenceNumber()
	{ 
		return m_tslc.SequenceNumber ;
	}

	void LengthOfDataChanges(const SV_ULONGLONG & lodc)
	{
		m_lodc.QuadPart = lodc;
	}

	SV_ULONGLONG LengthOfDataChanges()
	{
		return m_lodc.QuadPart;
	}

	void AddBookMark(SvMarkerPtr bookmark)
	{
		m_users.push_back(bookmark);
	}

	bool ContainsMarker()                  
	{ 
		return ( m_users.begin() != m_users.end() ); 
	}

	SV_UINT NumUserTags()         
	{
		return static_cast<SV_UINT>(m_users.size()); 
	}

	UserTagsIterator UserTagsBegin() 
	{
		return m_users.begin(); 
	}

	UserTagsIterator UserTagsEnd()
	{
		return m_users.end(); 
	}

	bool ContainsRevocationTag()
	{
		UserTagsIterator udtsIter = m_users.begin();
		UserTagsIterator udtsEnd = m_users.end();
		for ( /* empty */ ; udtsIter != udtsEnd ; ++udtsIter )
		{
			SvMarkerPtr pMarker = *udtsIter;
			if(pMarker -> IsRevocationTag())
			{
				return true;
			}
		}
		return false;
	}

	bool GetRevocationTag(SvMarkerPtr &ptr)
	{
		UserTagsIterator udtsIter = m_users.begin();
		UserTagsIterator udtsEnd = m_users.end();
		for ( /* empty */ ; udtsIter != udtsEnd ; ++udtsIter )
		{
			SvMarkerPtr pMarker = *udtsIter;
			if(pMarker -> IsRevocationTag())
			{
				ptr = pMarker;
				return true;
			}
		}
		return false;
	}

	void AddDRTD(const cdp_drtdv2_t &drtd)
	{
		m_drtds.push_back(drtd);
	}

	SV_UINT       NumDrtds()               
	{ 
		return static_cast<SV_UINT>(m_drtds.size()); 
	}

	cdp_drtdv2s_iter_t DrtdsBegin()           
	{ 
		return m_drtds.begin(); 
	}

	cdp_drtdv2s_iter_t DrtdsEnd()             
	{
		return m_drtds.end(); 
	}

	cdp_drtdv2s_constiter_t DrtdsBegin()  const    
	{
		return m_drtds.begin(); 
	}

	cdp_drtdv2s_constiter_t DrtdsEnd()    const    
	{
		return m_drtds.end(); 
	}

	cdp_drtdv2s_riter_t DrtdsRbegin()           
	{ 
		return m_drtds.rbegin(); 
	}

	cdp_drtdv2s_riter_t DrtdsRend()             
	{ 
		return m_drtds.rend(); 
	}


	cdp_drtdv2s_constriter_t DrtdsRbegin()   const        
	{ 
		return m_drtds.rbegin(); 
	}

	cdp_drtdv2s_constriter_t DrtdsRend()    const         
	{ 
		return m_drtds.rend(); 
	}

	SV_ULONGLONG DrtdsLength() const
	{
		SV_ULONGLONG length = 0;

		cdp_drtdv2s_constiter_t drtdsIter = m_drtds.begin();
		cdp_drtdv2s_constiter_t drtdsEnd  = m_drtds.end();

		if (drtdsIter == drtdsEnd)
			length = 0;
		else
		{
			for ( /* empty */ ; drtdsIter != drtdsEnd ; ++drtdsIter )
			{
				cdp_drtdv2_t drtd = *drtdsIter;
				length += drtd.get_length();
			}	
		}

		return length;
	}


	cdp_drtdv2_t  drtd(SV_UINT drtdNo)
	{
		return m_drtds[drtdNo];
	}

	void SetContinuationId(SV_UINT ctid) 
	{
		m_continuationId = ctid;
	}

	SV_UINT ContinuationId() 
	{ 
		return m_continuationId;
	}

	void type(SV_UINT type)
	{
		m_type = type;
	}

	SV_UINT type()                    
	{ 
		return m_type ; 
	}

	void Baseline(OS_VAL baseline)
	{
		m_baseline = baseline;
	}

	OS_VAL Baseline()
	{
		return m_baseline;
	}

	void IdVersion(SV_UINT version)
	{
		m_idVersion = version;
	}

	SV_UINT IdVersion()  
	{ 
		return m_idVersion; 
	}

	SV_ULONGLONG actualStartTime()
	{
		if((SV_WIN_OS != m_baseline) && (OS_UNKNOWN != m_baseline))
		{
			return (m_tsfc.TimeInHundNanoSecondsFromJan1601 - SVEPOCHDIFF);
		}
		else
		{
			return m_tsfc.TimeInHundNanoSecondsFromJan1601;
		}
	}

	SV_ULONGLONG actualEndTime()
	{
		if((SV_WIN_OS != m_baseline) && (OS_UNKNOWN != m_baseline))
		{
			return (m_tslc.TimeInHundNanoSecondsFromJan1601 - SVEPOCHDIFF);
		}
		else
		{
			return m_tslc.TimeInHundNanoSecondsFromJan1601;
		}
	}

private:

	SV_OFFSET_TYPE        m_start;
	SV_OFFSET_TYPE        m_end;
	SVD_HEADER_V2         m_hdr;
	SVD_TIME_STAMP_V2     m_tsfc;
	SVD_TIME_STAMP_V2     m_tslc;
	SV_ULARGE_INTEGER     m_lodc;
	UserTags              m_users;
	cdp_drtdv2s_t             m_drtds;
	SV_UINT               m_continuationId;
	SV_UINT               m_type; // split io/non split io
	OS_VAL                m_baseline;

	//diff filename format version
	//0 means - completed_ediffcompleted_diff_<source timestamp>_[tso_]<cx timestamp>.dat[.gz]
	//1 means - completed_[ediffcompleted_]diff_[tso_]S<time>__<n>_E<time>_<n>_<MC|ME|WC|WE><continuation number>_[<cx timestamp>].dat[.gz]
	//2 means - completed_[ediffcompleted_]diff_[tso_]S<time>__<n>_E<time>_<n>_<MC|ME|WC|WE><continuation number>_O<dirtyBlockCounter>_R<ResyncCounter>[_<cx timestamp>].dat[.gz]
	SV_UINT               m_idVersion;


};

typedef boost::shared_ptr<Differential> DiffPtr;
typedef std::vector< DiffPtr > Differentials_t;
typedef Differentials_t::iterator DiffIterator;
typedef Differentials_t::reverse_iterator DiffRevIterator;



struct CDPLastProcessedFile
{
	struct Header
	{
		unsigned int magicBytes;
		unsigned int version;
	}hdr;

	struct PreviousDiff
	{
		char filename[APPLIED_INFO_CHAR_ARRAY_LEN];
		unsigned int SVDVersion;//tells if this diff contains PerIOTimeStamps
	}previousDiff;

	char reserved[248];
};

struct CDPLastRetentionUpdate
{
	struct Header
	{
		unsigned int magicBytes;
		unsigned int version;
	}hdr;

	struct PartialTransaction
	{
		char diffFileApplied[APPLIED_INFO_CHAR_ARRAY_LEN];
		unsigned int noOfChangesApplied;
	}partialTransaction;

	struct RetentionEndTimeStamp
	{
		unsigned long long SequenceNumber;
		unsigned long long TimeInHundNanoSecondsFromJan1601;
		unsigned int ContinuationId;
		char reserved[4];
	}retEndTimeStamp;

	char reserved[224];
};

typedef struct cdp_io_stats
{
	SV_UINT read_io_count;
	SV_ULONGLONG read_bytes_count;
}cdp_io_stats_t;

#endif
