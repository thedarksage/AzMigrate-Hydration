#include "retwindowobject.h"

namespace ConfSchema
{
    RetWindowObject::RetWindowObject()
    {
        m_pName = "RetentionWindow";
        m_pIdAttrName = "Id";
        m_pRetWindowIdAttrName = "retentionWindowId";
        m_pStartTimeAttrName = "startTime";
        m_pEndTimeAttrName = "endTime";
        m_pTimeGranularityAttrName = "timeGranularity";
        m_pTimeIntervalUnitAttrName = "timeIntervalUnit";
        m_pGranularityUnitAttrName = "granularityUnit";
    }
}
