#ifndef EVENT_Q_OBJ
#define EVENT_Q_OBJ
namespace ConfSchema
{
    class EventQueueObj
    {
        const char *m_pName;
        const char *m_pEventName;
    public:
        EventQueueObj() ;
        const char * GetName(void);
        const char * GetEventNameAttrName(void);
    } ;
    inline const char * EventQueueObj::GetName(void)
    {
        return m_pName;
    }
    inline const char * EventQueueObj::GetEventNameAttrName(void)
    {
        return m_pEventName;
    }
}
#endif