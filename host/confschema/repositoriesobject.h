#ifndef REPOSITORIES_OBJECT_H
#define REPOSITORIES_OBJECT_H

namespace ConfSchema
{
    class RepositoriesObject
    {
        const char *m_pName;
        const char *m_pNameAttrName;
		const char *m_pTypeAttrName;
		const char *m_pDeviceNameAttrName;
		const char *m_pLabelAttrName;
		const char *m_pMountPointAttrName;
		const char *m_pStatusAttrName;
		const char *m_pDomainAttrName;
		const char *m_pUserNameAttrName;
		const char *m_pPasswordAttrName;
	
	public:
        RepositoriesObject();
        const char * GetName(void);
		const char * GetNameAttrName(void);
		const char * GetTypeAttrName(void);
		const char * GetDeviceNameAttrName(void);
		const char * GetLabelAttrName(void);
		const char * GetMountPointAttrName(void);
		const char * GetStatusAttrName(void);
		const char * GetDomainAttrName(void);
		const char * GetUserNameAttrName(void);
		const char * GetPasswordAttrName(void);
	};
	
	inline const char * RepositoriesObject::GetName(void)
    {
        return m_pName;
    }
	inline const char * RepositoriesObject::GetNameAttrName(void)
	{
		return m_pNameAttrName;
	}
	inline const char * RepositoriesObject::GetTypeAttrName(void)
	{
		return m_pTypeAttrName;
	}
	inline const char * RepositoriesObject::GetLabelAttrName(void)
	{
		return m_pLabelAttrName;
	}
	inline const char * RepositoriesObject::GetDeviceNameAttrName(void)
	{
		return m_pDeviceNameAttrName;
	}
	inline const char * RepositoriesObject::GetMountPointAttrName(void)
	{
		return m_pMountPointAttrName;
	}
	inline const char * RepositoriesObject::GetStatusAttrName(void)
	{
		return m_pStatusAttrName;
	}
	inline const char * RepositoriesObject::GetDomainAttrName(void)
	{
		return m_pDomainAttrName;
	}
	inline const char * RepositoriesObject::GetUserNameAttrName(void)
	{
		return m_pUserNameAttrName;
	}
	inline const char * RepositoriesObject::GetPasswordAttrName(void)
	{
		return m_pPasswordAttrName;
	}

}

#endif /* REPOSITORIES_OBJECT_H */