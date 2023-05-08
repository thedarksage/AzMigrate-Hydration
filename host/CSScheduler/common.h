#ifndef __COMMON_H_
#define __COMMON_H_

template <typename T> inline void SAFE_DELETE( T*& p ) 
{ 
	if(p) 
	{ 
		delete p; 
		p = NULL; 
	}
}

template <typename X> inline void SAFE_SQL_CLOSE( X*& p ) 
{ 
	if(p) 
	{ 
		p->close(); 
		delete p; 
		p = NULL; 
	}
}

template <typename T> inline void SAFE_DELETE_ARRAY(T*& p)
{
	if (p)
	{
		delete[] p;
		p = NULL;
	}
}

enum SCH_STATUS {SCH_SUCCESS,SCH_ERROR,SCH_EAGAIN,SCH_EINVAL,SCH_INSUFFICIENT_RESOURCES,SCH_PENDING,SCH_INVALID_PARAM,SCH_TIMEOUT,SCH_DELETED};

class SCH_CONF
{
	bool m_bUseHttps;
	std::string m_cx_api_url;
	std::string m_cx_host_id;
	static SCH_CONF m_conf;
	SCH_CONF()
		:m_bUseHttps(false) {}
public:
	static bool Initialize();
	static std::string GetCxApiUrl()
	{
		return m_conf.m_cx_api_url;
	}
	static bool UseHttps()
	{
		return m_conf.m_bUseHttps;
	}
	static std::string GetCxHostId()
	{
		return m_conf.m_cx_host_id;
	}
};

#endif

