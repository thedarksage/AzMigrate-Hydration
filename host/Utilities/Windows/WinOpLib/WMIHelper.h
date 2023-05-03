#pragma once
#define _WIN32_DCOM
#include <comdef.h>
#include <Wbemidl.h>

#pragma comment(lib, "wbemuuid.lib")

#ifndef INM_ENTER
#define INM_ENTER()
#endif

#ifndef INM_EXITD
#define INM_EXITD() done:
#endif

#ifndef INM_DONE
#define INM_DONE() goto done;
#endif

class InmWMIHelper
{
	private:
		IWbemLocator *	m_pInmLocator;
		IWbemServices *	m_pInmService;
		bool			m_InmisConnected;

		COAUTHIDENTITY *m_pInmUserIdent;
		void SetUserIdentiry(COAUTHIDENTITY *pUserIdentity)
		{
			if(m_pInmUserIdent)
			{
				delete m_pInmUserIdent;
			}
			m_pInmUserIdent = pUserIdentity;
		}

	public:
		InmWMIHelper(void);
		~InmWMIHelper(void);

		DWORD ConnectWMI(WCHAR *serverName, WCHAR *szDomainName = NULL, WCHAR *szUserName = NULL, WCHAR *szPwd = NULL);

		bool IsWMIConnected() { return m_InmisConnected;}

		DWORD DisconnectWMI();
		DWORD UpdateDnsFromDS(std::vector<const char*> &vListOfZoneName);
};



