#ifndef CONSISTENCY_POLICY_OBJ_H
#define CONSISTENCY_POLICY_OBJ_H

namespace ConfSchema
{
	class ConsistencyPolicyObject
	{
		const char *m_pName;
		const char *m_pTagTypeAttrName;
		const char *m_pIntervalAttrName;
		const char *m_pExceptionAttrName;
		const char *m_pTagNameAttrName;
	public:
		ConsistencyPolicyObject();
		const char * GetName(void);
		const char * GetTagTypeAttrName(void);
		const char * GetIntervalAttrName(void);
		const char * GetExceptionAttrName(void);
		const char * GetTagNameAttrName(void);
	};

	inline const char * ConsistencyPolicyObject::GetName(void)
	{
		return m_pName;
	}

	inline const char * ConsistencyPolicyObject::GetTagTypeAttrName(void)
	{
		return m_pTagTypeAttrName;
	}

	inline const char * ConsistencyPolicyObject::GetIntervalAttrName(void)
	{
		return m_pIntervalAttrName;
	}

	inline const char * ConsistencyPolicyObject::GetExceptionAttrName(void)
	{
		return m_pExceptionAttrName;
	}

	inline const char * ConsistencyPolicyObject::GetTagNameAttrName(void)
	{
		return m_pTagNameAttrName;
	}
}

#endif