#ifndef PLAN__OBJECT__H__
#define PLAN__OBJECT__H__

namespace ConfSchema
{
    class PlanObject
    {
        const char *m_pName;
        const char *m_pPlanIdAttrName;
        const char *m_pPlanNameAttrName;
    public:
        PlanObject();
        const char * GetName(void);
        const char * GetPlandIdAttrName(void);
        const char * GetPlanNameAttrName(void);
    };

    inline const char * PlanObject::GetName(void)
    {
        return m_pName;
    }
    inline const char * PlanObject::GetPlandIdAttrName(void)
    {
        return m_pPlanIdAttrName;
    }
    inline const char * PlanObject::GetPlanNameAttrName(void)
    {
        return m_pPlanNameAttrName;
    }
}

#endif /*PLAN__OBJECT__H__*/
