#ifndef PAIR__INFO__OBJECT__H__
#define PAIR__INFO__OBJECT__H__

namespace ConfSchema
{
    class PairInfoObject
    {
	    const char *m_pName;
	    const char *m_pTargetHostIdAttrName;
    public:
	    PairInfoObject();
	    const char * GetName(void);
	    const char * GetTargetHostIdAttrName(void);
    };

    inline const char * PairInfoObject::GetName(void)
    {
	    return m_pName;
    }
    inline const char * PairInfoObject::GetTargetHostIdAttrName(void)
    {
	    return m_pTargetHostIdAttrName;
    }
}
#endif /* PAIR__INFO__OBJECT__H__ */
