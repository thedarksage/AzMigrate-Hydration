#ifndef __TAGDISCOVERY_H__
#define __TAGDISCOVERY_H__
#include <list>
#include "cdputil.h"

enum ACCURACYEMODE
{
	ACCURACY_EXACT,
	ACCURACY_APPROXIMAYE,
	ACCURACY_NOTGAURANTEED,
	ACCURACY_ALL
};

class TagDiscover 
{
	Configurator* m_vxConfigurator;
	bool m_bconfig;
	SVERROR getRetentionDB( const std::string& volume, std::string& retentionDBPath );
public:
	TagDiscover();
	SVERROR init();
	SVERROR initConfig();
	SVERROR getListOfEvents( const std::list<std::string>& volList,  std::map<std::string, std::vector<CDPEvent> >& eventInfoMap );
	SVERROR getRecentConsistencyPoint( const std::list<std::string>& volList, const std::string& appName, CDPEvent& cdpCommonEvent  );
	SVERROR getRecentCommonConsistencyPoint( const std::list<std::string>& volList, SV_ULONGLONG& eventTime );
	SVERROR getTimeRanges( const std::list<std::string>& volList, const ACCURACYEMODE& mode, std::map< std::string, std::vector<CDPTimeRange> >& timeRangeInfoMap );
	SVERROR printListOfEvents( const std::map<std::string, std::vector<CDPEvent> >& eventInfoMap );
	SVERROR printRecentConsistencyPoint( const std::string& appName, const CDPEvent& cdpCommonEvent );
	SVERROR printRecentCommonConsistencyPoint( const SV_ULONGLONG& eventTime );
	SVERROR printTimeRanges( const std::map< std::string, std::vector<CDPTimeRange> >& timeRangeInfoMap, const int& lastEntries );
	SVERROR getTimeinDisplayFormat( const SV_ULONGLONG& eventTime, std::string& displayTimeForm);
    bool isTagReached(std::list<std::string>& volList,const std::string& tagName);
    std::string getTagType(SV_USHORT eventType);
	~TagDiscover();
};
#endif