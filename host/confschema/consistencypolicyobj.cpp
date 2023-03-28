#include "consistencypolicyobj.h"

namespace ConfSchema
{
	ConsistencyPolicyObject::ConsistencyPolicyObject()
	{
		m_pName = "ConsistencyPolicy";
		m_pTagTypeAttrName = "tagType";
		m_pIntervalAttrName = "interval";
		m_pExceptionAttrName = "exception";
		m_pTagNameAttrName = "tagName";
	}
}

