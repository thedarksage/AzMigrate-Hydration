#include <sstream>
#include <boost/lexical_cast.hpp>
#include "portablehelpers.h"
#include "InmsdkGlobals.h"
#include "confschemareaderwriter.h"
#include "time.h"
#include "auditconfig.h"
#include "inmsdkutil.h"
AuditConfigPtr AuditConfig::m_auditGroupPtr ;

AuditConfigPtr AuditConfig::GetInstance()
{
    if( !m_auditGroupPtr )
    {
        m_auditGroupPtr.reset( new AuditConfig() ) ;
    }
    m_auditGroupPtr->loadAuditConfig() ;
    return m_auditGroupPtr ;
}

void AuditConfig::loadAuditConfig()
{
    ConfSchemaReaderWriterPtr confSchemaReaderWriter = ConfSchemaReaderWriter::GetInstance() ;
    m_auditGroup = confSchemaReaderWriter->getGroup("Audit") ;
}

void AuditConfig::LogAudit(const std::string& msg)
{
    DebugPrintf(SV_LOG_DEBUG, "ENTERED %s\n", FUNCTION_NAME) ;
    ACE_OS::sleep(1) ;
    time_t currentAuditObjectTime = time(NULL) ;
    ConfSchema::Object auditMsgObj ;
    auditMsgObj.m_Attrs.insert( std::make_pair( "Message", msg)) ;
    auditMsgObj.m_Attrs.insert( std::make_pair( "TimeStamp", getTimeString(currentAuditObjectTime)) ) ;
    m_auditGroup->m_Objects.insert( std::make_pair( boost::lexical_cast<std::string>(currentAuditObjectTime),
        auditMsgObj)) ;

	ConfSchema::ObjectsIter_t auditObjIter = m_auditGroup->m_Objects.begin();
	ConfSchema::ObjectsIter_t objectToEraseIter;
	while (auditObjIter != m_auditGroup->m_Objects.end())
	{
		time_t storedAuditObjectTime = boost::lexical_cast <time_t> (auditObjIter->first);
		objectToEraseIter = auditObjIter;
		auditObjIter++;
		if (currentAuditObjectTime - storedAuditObjectTime > 7*24*60*60 )  //7*24*60*60
		{		
			m_auditGroup->m_Objects.erase(objectToEraseIter);
		}
	}

    DebugPrintf(SV_LOG_DEBUG, "EXITED %s\n", FUNCTION_NAME) ;
}
AuditConfig::AuditConfig()
{
    m_isModified = false ;
    m_auditGroup = NULL ;
}

ConfSchema::Group& AuditConfig::GetAuditGroup( ) 
{
    return *m_auditGroup ;
}

void AuditConfig::RemoveAuditMessages()
{
	m_auditGroup->m_Objects.clear();
	return;
}