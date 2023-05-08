#ifndef INM_AUDIT_CONFIG
#define INM_AUDIT_CONFIG
#include <boost/shared_ptr.hpp>
#include "confschema.h"
#include "portable.h"
class AuditConfig;
typedef boost::shared_ptr<AuditConfig> AuditConfigPtr ;
class AuditConfig
{
    ConfSchema::Group* m_auditGroup ;
    bool m_isModified ;
    void loadAuditConfig() ;
    static AuditConfigPtr m_auditGroupPtr ;
public:
    AuditConfig() ;
    static AuditConfigPtr GetInstance() ;
    void LogAudit(const std::string& msg) ;
	ConfSchema::Group& GetAuditGroup( ) ;
	void RemoveAuditMessages();
} ;
#endif