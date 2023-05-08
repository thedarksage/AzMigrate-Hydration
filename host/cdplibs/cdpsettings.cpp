//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : cdpsettings.cpp
//
// Description: 
//


#include "cdpsettings.h"
#include "cdpglobals.h"
#include "cdputil.h"
#include "cdpplatform.h"

#include <vector>
using namespace std;



// Constructor for cdp settings
CDP_SETTINGS :: CDP_SETTINGS()
{
	m_state = CDP_DISABLED;
	m_type = CDP_UNDO;
	m_compress = CDP_NOCOMPRESSION;
	m_spacepolicy.bphysicalsize = true;
	m_spacepolicy.size = 0;
	m_timepolicy.m_whentoenforce =CDP_NEVER;
	m_timepolicy.m_time = 0;
	m_deleteoption = CDP_NORMAL ;
    m_metadir.clear();
    m_new_metadir.clear();
    m_dbname.clear();
    m_altmetadir.clear();
    m_datadirs.clear();
	m_dsthreshold = 80;
	m_lsthreshold = 80;
}
/*
// Copy Constructor for cdp settings
CDP_SETTINGS :: CDP_SETTINGS(const CDP_SETTINGS& rhs)
{
	m_state = rhs.m_state;
	m_type = rhs.m_type;
	m_compress = rhs.m_compress;
	m_spacepolicy.bphysicalsize = rhs.m_spacepolicy.bphysicalsize;
	m_spacepolicy.size = rhs.m_spacepolicy.size;
	m_timepolicy.m_whentoenforce = rhs.m_timepolicy.m_whentoenforce;
	m_timepolicy.m_time = rhs.m_timepolicy.m_time;
	m_deleteoption = rhs.m_deleteoption ;
    m_metadir = rhs.m_metadir ;
    m_new_metadir = rhs.m_new_metadir ;
    m_dbname = rhs.m_dbname;
    m_altmetadir = rhs.m_altmetadir ;
    m_datadirs.assign(rhs.m_datadirs.begin(), rhs.m_datadirs.end());
}

// Overloaded Assignment Operator for CDP Settings
CDP_SETTINGS& CDP_SETTINGS :: operator=(const CDP_SETTINGS& rhs)
{
	// Block Self assignment
    if ( this == &rhs)
        return *this;

	m_state = rhs.m_state;
	m_type = rhs.m_type;
	m_compress = rhs.m_compress;
	m_spacepolicy.bphysicalsize = rhs.m_spacepolicy.bphysicalsize;
	m_spacepolicy.size = rhs.m_spacepolicy.size;
	m_timepolicy.m_whentoenforce = rhs.m_timepolicy.m_whentoenforce;
	m_timepolicy.m_time = rhs.m_timepolicy.m_time;
	m_deleteoption = rhs.m_deleteoption ;
    m_metadir = rhs.m_metadir ;
    m_new_metadir = rhs.m_new_metadir ;
    m_dbname = rhs.m_new_metadir;
    m_altmetadir = rhs.m_altmetadir ;
    m_datadirs.assign(rhs.m_datadirs.begin(), rhs.m_datadirs.end());
	return *this;
}
*/

// 
// TODO: 1.Eventually, we would be using configurator for all these settings
//       2. We would be using single query to get all retention settings
//

CDPSettings::CDPSettings(double version, const HTTP_CONNECTION_SETTINGS & cxsettings, 
						 const string & hostid, const string & volumename)
: m_hostid(hostid), m_volumename(volumename), m_cxsettings(cxsettings)
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);

	m_bInit = false;
	m_version = version;
	FillDefaultSettings();

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
}


void CDPSettings::FillDefaultSettings()
{
	m_settings.m_state = CDP_DISABLED;
	m_settings.m_type = CDP_UNDO;
	m_settings.m_compress = CDP_NOCOMPRESSION;
	m_settings.m_spacepolicy.bphysicalsize = true;
	m_settings.m_spacepolicy.size = 0;
	m_settings.m_timepolicy.m_whentoenforce =CDP_NEVER;
	m_settings.m_timepolicy.m_time = 0;
	m_settings.m_deleteoption = CDP_NORMAL ;

	m_settings.m_dbname.clear();
	m_settings.m_metadir.clear();
	m_settings.m_altmetadir.clear();
	m_settings.m_datadirs.clear();
}

	
// check whether retention is enabled or not
bool CDPSettings::CheckEnabled()
{
	string url;
	char * result = NULL ;
	bool rv = true;

	url = m_cxsettings.szUrl;
	url += "?hostid=";
	url += m_hostid;
	url += "&src=";
	url += *(m_volumename.c_str()) ; //TODO: do not be drive letter specific
	url += "&tag=";
	url += GET_RETENTION_OPTION_TAG;

	if( (GetFromSVServer( m_cxsettings.ipAddress, 
						  m_cxsettings.port, url.c_str(), 
						  &result).succeeded()) && (result != 0) )
	{
		(result[0] == '1' )? (m_settings.m_state = CDP_ENABLED):(m_settings.m_state = CDP_DISABLED);
	}
	else
	{
		DebugPrintf(SV_LOG_FATAL, 
			"cx query for retention log enabled on host:%s volume:%s failed\n", 
			m_hostid.c_str(), m_volumename.c_str());
		rv = false;
	}	

	delete [] result;
	result = NULL;
	return rv;
}


// check whether retention is enabled or not
bool CDPSettings::FillLogLocation()
{
	string url;
	char * result = NULL ;
	bool rv = true;

	url = m_cxsettings.szUrl;
	url += "?hostid=";
	url += m_hostid;
	url += "&src=";
	url += *(m_volumename.c_str()) ; //TODO: do not be drive letter specific
	url += "&tag=";
	url += GET_LOGLOCATION_TAG;

	if( (GetFromSVServer( m_cxsettings.ipAddress, 
						  m_cxsettings.port, url.c_str(), 
						  &result).succeeded()) && (result != 0) )
	{
		string buf  = result ;
		CDPUtil::trim(buf);

		vector<string> tokens = CDPUtil::split(buf, ",");
		if(tokens.size() != 2)
		{
			DebugPrintf(SV_LOG_FATAL, 
				"cx query for retention log directory on host:%s volume:%s failed\n", 
				m_hostid.c_str(), m_volumename.c_str());
			delete [] result;
			result = NULL;
			return false;
		}

		m_settings.m_metadir = tokens[1];
		m_settings.m_metadir += "\\";
		m_settings.m_metadir += m_hostid;
		m_settings.m_metadir += "\\";
		m_settings.m_metadir += *(m_volumename.c_str()); // TODO: do not be drive letter specific

		if(m_version == CDPVER0)
			m_settings.m_dbname = m_settings.m_metadir + ACE_DIRECTORY_SEPARATOR_CHAR + CDPV0DBNAME;
		else if(m_version == CDPVER1)
			m_settings.m_dbname = m_settings.m_metadir + ACE_DIRECTORY_SEPARATOR_CHAR + CDPV1DBNAME;
	}
	else
	{
		DebugPrintf(SV_LOG_FATAL, 
			"cx query for retention log directory on host:%s volume:%s failed\n", 
			m_hostid.c_str(), m_volumename.c_str());
		rv = false;
	}	

	delete [] result;
	result = NULL;
	return rv;
}

// check whether retention is enabled or not
bool CDPSettings::FillRetentionPolicies()
{
	string url;
	char * result = NULL ;
	bool rv = true;

	url = m_cxsettings.szUrl;
	url += "?hostid=";
	url += m_hostid;
	url += "&src=";
	url += *(m_volumename.c_str()) ; //TODO: do not be drive letter specific
	url += "&tag=";
	url += GET_RETENTION_LIMITS_TAG;

	if( (GetFromSVServer( m_cxsettings.ipAddress, 
						  m_cxsettings.port, url.c_str(), 
						  &result).succeeded()) && (result != 0) )
	{
		m_settings.m_spacepolicy.bphysicalsize = true;
		m_settings.m_spacepolicy.size = _atoi64(result);
	}
	else
	{
		DebugPrintf(SV_LOG_FATAL, 
			"cx query for retention policies on host:%s volume:%s failed\n", 
			m_hostid.c_str(), m_volumename.c_str());
		rv = false;
	}

	delete [] result;
	result = NULL;
	return rv;
}


bool CDPSettings::Init()
{
	DebugPrintf(SV_LOG_DEBUG,"ENTERED %s\n",FUNCTION_NAME);
	char * result = NULL;

	do{

		if(m_bInit)
			break;

		if(!CheckEnabled())
			break;

		if(m_settings.m_state == CDP_DISABLED)
		{
			m_bInit = true;
			DeleteRetentionPath(m_volumename);
			break;
		}

		if(!FillLogLocation())
			break;

		if(!FillRetentionPolicies())
			break;


		PersistRetentionPath(m_settings.m_dbname, m_volumename);
		m_bInit = true;
	} while ( false );

	DebugPrintf(SV_LOG_DEBUG,"EXITED %s\n",FUNCTION_NAME);
	return m_bInit;
}

//=======================================================================
//
// #include "queryresult.h"
//
//		QueryResult qs(result);
//		string value;
//		unsigned int i;
//
//		value = qs.Get("LOG_ENABLED");
//		if(!value.empty())
//		{
//			istrstream s(value.c_str());
//			s >> i;
//			if(!s.good())
//				break;
//			m_state = (RETENTION_STATE)i;
//		}
//
//
//		if( m_state == CONFIGURED )
//		{
//			value = qs.Get("LOG_TYPE");
//			if(!value.empty())
//			{
//				istrstream s(value.c_str());
//				s >> i;
//				if(!s.good())
//					break;
//				m_settings.m_type = (RETENTION_LOGTYPE)i;
//			}
//
//			m_settings.m_dir = qs.Get("LOG_DIR");
//
//			value = qs.Get("LOG_COMPRESS");
//			if(!value.empty())
//			{
//				istrstream s(value.c_str());
//				s >> i;
//				if(!s.good())
//					break;
//				m_settings.m_compress = (COMPRESSION_METHOD)i;
//			}
//
//			value = qs.Get("LOG_CAPACITY");
//			if(!value.empty())
//			{
//				istrstream s(value.c_str());
//				s >> m_settings.m_size;
//				if(!s.good())
//					break;
//			}
//
//			value = qs.Get("LOG_PERIOD");
//			if(!value.empty())
//			{
//				istrstream s(value.c_str());
//				s >> m_settings.m_time.wYear;
//				s >> m_settings.m_time.wMonth;
//				s >> m_settings.m_time.wDay;
//				s >> m_settings.m_time.wHour;
//				s >> m_settings.m_time.wMinute;
//				s >> m_settings.m_time.wSecond;
//				s >> m_settings.m_time.wMilliseconds;
//
//				if(!s.good())
//					break;
//			}
//
//
//#if 0      // enable in 3.6 
//			value = qs.Get("LOG_MIN_MARKERS");
//			if(!value.empty())
//			{
//				svector_t values = QueryResult::split(value,",");
//				for(i =0; i < values.size() ; )
//				{
//					// insert into the map
//				}
//			}
//#endif
//
//		} // end if configured
//=======================================================
