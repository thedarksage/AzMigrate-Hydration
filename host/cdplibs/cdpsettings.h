//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       :cdpsettings.h
//
// Description: 
//

#ifndef CDPSETTINGS__H
#define CDPSETTINGS__H

#include "transport_settings.h"
#include "cdpglobals.h"
#include "boost/shared_ptr.hpp"

class CDPSettings
{
public:
	
	typedef boost::shared_ptr<CDPSettings> Ptr;

	virtual ~CDPSettings() {}

	bool Good()    { return m_bInit ; }
	bool Enabled() { return ( m_bInit && (m_settings.m_state == CDP_ENABLED)) ; }
	bool Get(CDP_SETTINGS &settings) { if ( m_bInit ) settings = m_settings; return m_bInit; }

	
	static SV_ULONGLONG LookupPageSize()          { return CDP_LOOKUPPAGESIZE; }

	friend class CDPFactory;
protected: 

private:

	CDPSettings(double version, const HTTP_CONNECTION_SETTINGS &, const string & hostid, const string & volumename); 
	bool Init();

	void FillDefaultSettings();
	bool FillRetentionPolicies();
	bool FillLogLocation();
	bool CheckEnabled();

	bool m_bInit;
	string         m_volumename;
	string         m_hostid;
	HTTP_CONNECTION_SETTINGS m_cxsettings;
	CDP_SETTINGS m_settings;
	double m_version;
};

#endif

