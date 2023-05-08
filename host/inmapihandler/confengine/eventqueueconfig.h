#ifndef INM_EVENTQUEUE_CONFIG
#define INM_EVENTQUEUE_CONFIG
#include <boost/shared_ptr.hpp>
#include "confschema.h"
#include "portable.h"
class EventQueueConfig;
#include "eventqueueobj.h"
typedef boost::shared_ptr<EventQueueConfig> EventQueueConfigPtr ;
class EventQueueConfig
{
    ConfSchema::EventQueueObj m_eventqueuobj ;
    ConfSchema::Group* m_EventQGroup ;
    bool m_isModified ;
    void loadEventConfigGroup() ;
    static EventQueueConfigPtr m_eventqGroupPtr ;
public:
    EventQueueConfig() ;
    bool AnyPendingEvents() ;
	void AddPendingEvent(const std::string& operation) ;
    void ClearPendingEvents() ;
    static EventQueueConfigPtr GetInstance() ;
} ;
#endif