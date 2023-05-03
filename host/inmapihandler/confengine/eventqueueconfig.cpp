#include "eventqueueconfig.h"
#include "confschemareaderwriter.h"
#include <boost/lexical_cast.hpp>
EventQueueConfigPtr EventQueueConfig::m_eventqGroupPtr ;
EventQueueConfigPtr EventQueueConfig::GetInstance()
{
    if( !m_eventqGroupPtr )
    {
        m_eventqGroupPtr.reset( new EventQueueConfig() ) ;
    }
    m_eventqGroupPtr->loadEventConfigGroup();
    return m_eventqGroupPtr ;
}

void EventQueueConfig::loadEventConfigGroup()
{
    ConfSchemaReaderWriterPtr confSchemaReaderWriter = ConfSchemaReaderWriter::GetInstance() ;
    m_EventQGroup =  confSchemaReaderWriter->getGroup( m_eventqueuobj.GetName()) ;
}
bool EventQueueConfig::AnyPendingEvents()
{
    if( m_EventQGroup->m_Objects.size() )
    {
        return true ;
    }
    return false ;
}
void EventQueueConfig::ClearPendingEvents()
{
    m_EventQGroup->m_Objects.clear() ;
}
EventQueueConfig::EventQueueConfig()
{
}

void EventQueueConfig::AddPendingEvent(const std::string& operation)
{
	ConfSchema::Object eventObj ;
	eventObj.m_Attrs.insert( std::make_pair( "Operation", operation ) ) ;
	m_EventQGroup->m_Objects.insert( std::make_pair( boost::lexical_cast<std::string>(time(NULL)), eventObj)) ;
}

