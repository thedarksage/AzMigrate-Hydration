#include "repositoriesobject.h"

namespace ConfSchema
{
	RepositoriesObject::RepositoriesObject()
	{
		m_pName = "Repositories";
		m_pNameAttrName = "name";
		m_pTypeAttrName = "type";
		m_pDeviceNameAttrName = "devicename";
		m_pLabelAttrName = "label";
		m_pMountPointAttrName = "mountpoint";
		m_pStatusAttrName = "status";
		m_pDomainAttrName = "domain";
		m_pUserNameAttrName = "username";
		m_pPasswordAttrName = "password";
	}
}