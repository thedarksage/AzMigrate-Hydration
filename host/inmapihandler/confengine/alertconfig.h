#ifndef ALERT_CONFIG_H
#define ALERT_CONFIG_H
#include <string>
#include <boost/shared_ptr.hpp>
#include "confschema.h"
#include "alertobject.h"
#include "portable.h"

class AlertConfig ;
typedef boost::shared_ptr<AlertConfig> AlertConfigPtr ;

class AlertConfig
{
    ConfSchema::Group* m_alertGroup ;
    ConfSchema::AlertObjcet m_alertAttrNamesObj ;
    bool m_isModified ;
    void loadAlertGroup() ;
    AlertConfig() ;
    static AlertConfigPtr m_alertConfigPtr ;
    void RemoveOldAlerts(const std::string& alertPrefix) ;
public:
	ConfSchema::Group* m_tmpAlertGroup ;
    ConfSchema::AlertObjcet m_tmpAlertAttrNamesObj ;
    bool isModified() ;
    void AddAlert(const std::string  & errLevel,
                  const std::string& agentType,
                  SV_ALERT_TYPE alertType,
                  SV_ALERT_MODULE alertingModule,
                  const std::string& alertMsg,
                  const std::string& alertPrefix="") ;
    void AddAlert(const std::string & timeStr,
                  const std::string  & errLevel,
                  const std::string& agentType,
                  SV_ALERT_TYPE alertType,
                  SV_ALERT_MODULE alertingModule,
                  const std::string& alertMsg,
                  const std::string& alertPrefix="") ;
	void AddTmpAlert(const std::string  & errLevel,
                  const std::string& agentType,
                  SV_ALERT_TYPE alertType,
                  SV_ALERT_MODULE alertingModule,
                  const std::string& alertMsg,
                  const std::string& alertPrefix="") ;

    ConfSchema::Group& GetAlertGroup( ) ;
	void RemoveAlerts();
	void processAlert (std::string &alertMsg,std::string &alertPrefix);
	void SetAlertGrp(ConfSchema::Group& grp) ;
    static AlertConfigPtr GetInstance(bool loadGrp=true) ;
} ;
#endif
