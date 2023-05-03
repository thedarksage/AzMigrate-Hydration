#ifndef API__CONTROLLER__H
#define API__CONTROLLER__H

#include "InmsdkGlobals.h"
#include "HandlerFactory.h"
#include "InmFunctionInfo.h"
#include <boost/shared_ptr.hpp>
class RepositoryLock
{
	static ACE_Atomic_Op<ACE_Thread_Mutex, bool> m_quit ;
	bool m_locked ;
#ifdef SV_WINDOWS
	HANDLE m_lockhandle ;
	OVERLAPPED o;
#else
	int m_fd;
#endif
	std::string m_lockpath ;
public:
	void Release() ;
	RepositoryLock(const std::string& lockfile) ;
	bool Acquire(bool processanyways=false) ;
	~RepositoryLock() ;
	static void RequestQuit(bool flag) ;
} ;

#ifdef SV_WINDOWS
class __declspec(dllexport) APIController
#else
class APIController
#endif
{
	HandlerFactory m_handlerFactory;
public:
	APIController(void);
	~APIController(void);
		
	INM_ERROR processRequest(FunctionInfo &FunctionData);
	INM_ERROR processRequest(std::list<FunctionInfo> &listFuncReq);
};
void SignalApplicationQuit(bool flag) ;
void ProcessAPI(FunctionInfo &FunctionData) ;
bool IsRepositoryForHostAlreadyLocked(const std::string& lockPath) ;
#endif /* API__CONTROLLER__H */

