///
/// \file windowsremoteconnection.cpp
///
/// \brief
///


#include <Windows.h>
#include <comutil.h>
#include <comdef.h>
#include <Wbemcli.h>

#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

#include "portable.h"
#include "logger.h"

#include "windowsremoteconnection.h"
#include "converterrortostringminor.h"
#include "errorexception.h"
#include "errorexceptionmajor.h"
#include "setpermissions.h"
#include "scopeguard.h"
#include "supportedplatforms.h"
#include "credentialerrorexception.h"
#include "nonretryableerrorexception.h"
#include "WmiErrorException.h"


// important note:
// when making remote wmi calls, you may see COM-specific error codes returned if network problems cause you to lose the remote connection to Windows Management. 
// On error, you can call the COM function GetErrorInfo to obtain more error information. 
// this has not been implemented yet!

// important note:
// wait routine needs to be revisited
// for now, we are not using this routine from callers.

namespace remoteApiLib {


	void WindowsRemoteConnection::copyFiles(const std::vector<std::string> & filesToCopy, const std::string & folderonRemoteMachine)
	{

		for (size_t i = 0; i < filesToCopy.size(); ++i)
			copyFile(filesToCopy[i], folderonRemoteMachine);

	}

	void WindowsRemoteConnection::copyFile(const std::string & fileToCopy, const std::string & filePathonRemoteMachine)
	{
		netconnect();

		std::string src = fileToCopy;
		std::string dst = std::string("\\\\") + ip + std::string("\\")  + filePathonRemoteMachine;
		boost::replace_all(src, remoteApiLib::pathSeperatorToReplace(), remoteApiLib::pathSeperator());
		boost::replace_all(dst, remoteApiLib::pathSeperatorToReplace(), remoteApiLib::pathSeperator());
		boost::algorithm::trim(src);
		boost::algorithm::trim(dst);
		boost::replace_first(dst, ":", "$");

		if (boost::algorithm::ends_with(dst, remoteApiLib::pathSeperator())) {
			securitylib::setPermissions(dst, securitylib::SET_PERMISSIONS_NAME_IS_DIR);
			std::string::size_type idx = src.find_last_of(remoteApiLib::pathSeperator());
			if (std::string::npos != idx) {
				dst += src.substr(idx + 1);
			}
		}
		securitylib::setPermissions(dst, securitylib::SET_PERMISSIONS_DEFAULT);
		if (TRUE != CopyFileEx(src.c_str(), dst.c_str(), NULL, NULL, FALSE, 0)) {
			std::string err;
			convertErrorToString(lastKnownError(), err);

			switch (lastKnownError())
			{
			case ERROR_DISK_FULL :
				throw WmiErrorException(WmiErrorCode::SpaceNotAvailableOnTarget)
					<< "copy file " << src << " to " << dst << " failed with error : " 
					<< lastKnownError() << "(" << err << ")";
			default :
				throw ERROR_EXCEPTION << "copy file " << src << " to " << dst 
					<< " failed with error : " << lastKnownError() << "(" << err << ")";
			}
		}

	}

	void WindowsRemoteConnection::writeFile(const void * data, int length, const std::string & filePathonRemoteMachine)
	{
		netconnect();
		
		std::string dst = std::string("\\\\") + ip + std::string("\\")  + filePathonRemoteMachine;
		boost::replace_all(dst, remoteApiLib::pathSeperatorToReplace(), remoteApiLib::pathSeperator());
		boost::algorithm::trim(dst);
		boost::replace_first(dst, ":", "$");

		if (boost::algorithm::ends_with(dst, remoteApiLib::pathSeperator())) {
			throw ERROR_EXCEPTION << "writeFile routine is provided with invalid name for destination " << dst;
		}


		std::string::size_type idx = dst.find_last_of(remoteApiLib::pathSeperator());
		if (std::string::npos != idx) {
			std::string folder = dst.substr(0, idx);
			securitylib::setPermissions(folder, securitylib::SET_PERMISSIONS_NAME_IS_DIR);
		}
		

		DWORD dwBytesWritten = 0;
		LPSECURITY_ATTRIBUTES pSA = securitylib::defaultSecurityAttributes();

		HANDLE hFile = CreateFile(dst.c_str(),
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			pSA,
			CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL,
			NULL);

		ON_BLOCK_EXIT(&CloseHandle, hFile);

		if (INVALID_HANDLE_VALUE == hFile)	{
			std::string err;
			convertErrorToString(lastKnownError(), err);
			throw ERROR_EXCEPTION << "create file failed for " << dst << "with error :" << lastKnownError() << " ( " << err << ")";
		}
		
		if ((FALSE == WriteFile(hFile, data, length, &dwBytesWritten, NULL)) || (length != dwBytesWritten))	{
			std::string err;
			convertErrorToString(lastKnownError(), err);
			throw ERROR_EXCEPTION << "write file failed for " << dst << "with error :" << lastKnownError() << " ( " << err << ")";
		}
		
	}

	std::string WindowsRemoteConnection::readFile(const std::string & filePathonRemoteMachine, bool & fileNotAvailable)
	{
		std::string data;

		fileNotAvailable = false;
		netconnect();

		std::string dst = std::string("\\\\") + ip + std::string("\\") + filePathonRemoteMachine;
		boost::replace_all(dst, remoteApiLib::pathSeperatorToReplace(), remoteApiLib::pathSeperator());
		boost::algorithm::trim(dst);
		boost::replace_first(dst, ":", "$");

		if (boost::algorithm::ends_with(dst, remoteApiLib::pathSeperator())) {
			throw ERROR_EXCEPTION << "readFile routine is provided with invalid name for destination " << dst;
		}



		DWORD dwBytesRead = 0;

		HANDLE hFile = CreateFile(dst.c_str(),
			GENERIC_READ ,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			NULL);

		ON_BLOCK_EXIT(&CloseHandle, hFile);

		if (INVALID_HANDLE_VALUE == hFile)	{

			if (ERROR_FILE_NOT_FOUND == lastKnownError()){
				fileNotAvailable = true;
				return std::string();
			}

			std::string err;
			convertErrorToString(lastKnownError(), err);
			throw ERROR_EXCEPTION << "open file failed for " << dst << "with error :" << lastKnownError() << " ( " << err << ")";
		}

		char buffer[0x4000];
		int buflen = 0x4000;

		while (true) {

			if (FALSE == ReadFile(hFile, buffer, buflen, &dwBytesRead, NULL)) {
				std::string err;
				convertErrorToString(lastKnownError(), err);
				throw ERROR_EXCEPTION << "read file failed for " << dst << "with error :" << lastKnownError() << " ( " << err << ")";
			}

			if (0 == dwBytesRead) {
				break;
			}

			data.append(buffer, dwBytesRead);
		}

		return data;
	}


	void WindowsRemoteConnection::run(const std::string & commandToRun, unsigned int executionTimeoutInSecs)
	{
		unsigned int pid = 0;
		HRESULT hres;

		VARIANT vtProp, vtPid, vtEid;
		WmiInternal wi;
		bool waitForCompletion = true;

		try{
			wmiconnect();
			hres = m_wbemSvc->GetObject(L"Win32_Process", 0, NULL, &wi.pClass, NULL);
			if (FAILED(hres)) {
				throw ERROR_EXCEPTION << " get wmi object Win32_Process failed with error " << std::hex << hres ;
			}

			hres = wi.pClass->GetMethod(L"Create", 0, &wi.pInParamsDefinition, NULL);
			if (FAILED(hres)) {
				throw ERROR_EXCEPTION << " wmi Win32_Process.getMethod:Create failed with error " << std::hex << hres ;
			}

			hres = wi.pInParamsDefinition->SpawnInstance(0, &wi.pClassInstance);
			if (FAILED(hres)) {
				throw ERROR_EXCEPTION << " wmi Win32_Process spawn instance failed with error " << std::hex << hres ;
			}

			vtProp.vt = VT_BSTR;
			vtProp.bstrVal = _bstr_t(commandToRun.c_str());

			DebugPrintf(SV_LOG_ALWAYS, "Command to execute = %s\n", commandToRun.c_str());

			hres = wi.pClassInstance->Put(L"CommandLine", 0, &vtProp, 0);
			if (FAILED(hres)) {
				throw ERROR_EXCEPTION << " wmi method Put failed for " << commandToRun << " with error " << std::hex << hres ;
			}

			hres = m_wbemSvc->ExecMethod(L"Win32_Process", L"Create", 0, NULL, wi.pClassInstance, &wi.pOutParam, NULL);
			if (FAILED(hres)){
				throw ERROR_EXCEPTION << " wmi method Exec failed for " << commandToRun << " with error " << std::hex << hres ;
			}

			if (!wi.pOutParam) {
				throw ERROR_EXCEPTION << " wmi method Exec for " << commandToRun << " did not return expected output. error: internal";
			}

			hres = wi.pOutParam->Get(L"ProcessId", 0, &vtPid, 0, 0);
			if (FAILED(hres)) {
				throw ERROR_EXCEPTION << " wmi method Exec for " << commandToRun << " did not return process id. error: "
					<< std::hex << hres ;
			}

			pid = vtPid.uintVal;

			hres = wi.pOutParam->Get(L"ReturnValue", 0, &vtEid, 0, 0);
			if (FAILED(hres)) {
				throw ERROR_EXCEPTION << " wmi method Exec for " << commandToRun << " did not give output for ReturnValue request. error: "
					<< std::hex << hres ;
			}

			if (vtEid.intVal != 0){
				throw ERROR_EXCEPTION << " wmi method Exec for " << commandToRun << " failed with return code " << vtEid.intVal;
			}
			DebugPrintf(SV_LOG_ALWAYS, "Spawned remote push client process id = %d\n", pid);
			DebugPrintf(SV_LOG_ALWAYS, "Create process exit code = %d\n", vtEid.intVal);
			if (waitForCompletion)
			{
				wait(pid,executionTimeoutInSecs);
			}
		}
		catch (std::exception & )
		{
			throw;
		}
		catch (_com_error &err)
		{
			throw ERROR_EXCEPTION << " wmi exception. error " << "(" << err.ErrorMessage() << ")";
		}


	}

	void WindowsRemoteConnection::wait(unsigned int pid, unsigned int executionTimeoutInSecs)
	{
		int attempt = 1;
		long timeout = executionTimeoutInSecs * 1000;
		long timeSpent = 0;
		bool done = false;

		do{
			DebugPrintf(SV_LOG_ALWAYS, "Attempt %d Checking if remote process %u completed.\n", attempt, pid);

			if (timeSpent > timeout) {
				throw NON_RETRYABLE_ERROR_EXCEPTION << " remote job for " << ip << " did not complete even after waiting for " << timeout << " milliseconds";
			}

			waitonce(pid, done);
			if (!done){
				Sleep(30000);
				timeSpent += 30000;
				++attempt;
			}

		} while (!done);
	}

	void WindowsRemoteConnection::waitonce(unsigned int pid, bool & done)
	{

		HRESULT hres;
		ULONG uReturn = 0;
		WmiInternal wi;

		wmiconnect();

		std::string query = "SELECT * FROM Win32_Process where Handle = '" 
			+ boost::lexical_cast<std::string>(pid) + "'";
		DebugPrintf(SV_LOG_ALWAYS, "Executing wmi query = %s\n", query.c_str());
		
		hres = m_wbemSvc ->ExecQuery(L"WQL", 
			_bstr_t(query.c_str()), WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_WBEM_COMPLETE,
			NULL, &wi.pEnum);
		DebugPrintf(SV_LOG_ALWAYS, "ExecQuery returned hres = %x\n", hres);
		
		if (FAILED(hres)) {
			throw NON_RETRYABLE_ERROR_EXCEPTION << " wmi method ExecQuery for query " << query << " failed with error: "
				<< std::hex << hres ;
		}

		std::string domain = "WORKGROUP";
		std::string usernameWoDomain = username;

		if (username.find_first_of("\\") != std::string::npos) {
			domain = username.substr(0, username.find_first_of("\\"));
			usernameWoDomain = username.substr(username.find_first_of("\\") + 1);
		}
		else if (username.find_first_of("@") != std::string::npos) {
			usernameWoDomain = username.substr(0, username.find_first_of("@"));
			domain = username.substr(username.find_first_of("@") + 1);
		}

		COAUTHIDENTITY userAuthInfo;
		memset(&userAuthInfo, 0, sizeof(COAUTHIDENTITY));
		userAuthInfo.PasswordLength = password.length();
		userAuthInfo.Password = (USHORT*)password.c_str();
		userAuthInfo.DomainLength = domain.length();
		userAuthInfo.Domain = (USHORT*)domain.c_str();
		userAuthInfo.UserLength = usernameWoDomain.length();
		userAuthInfo.User = (USHORT*)usernameWoDomain.c_str();
		userAuthInfo.Flags = SEC_WINNT_AUTH_IDENTITY_ANSI;

		hres = CoSetProxyBlanket(wi.pEnum, RPC_C_AUTHN_DEFAULT, RPC_C_AUTHZ_DEFAULT, COLE_DEFAULT_PRINCIPAL,
			RPC_C_AUTHN_LEVEL_PKT_PRIVACY, RPC_C_IMP_LEVEL_IMPERSONATE, &userAuthInfo, EOAC_NONE);
		DebugPrintf(SV_LOG_ALWAYS, "CoSetProxyBlanket returned hres = %x\n", hres);
		if (FAILED(hres)){
			std::string err;
			convertErrorToString(lastKnownError(), err);
			throw NON_RETRYABLE_ERROR_EXCEPTION << "create wbem security proxy (CoSetProxyBlanket) for " << domain << "\\" << usernameWoDomain
				<< " failed with result " << std::hex << hres << "(" << err << ")";
		}

		HRESULT hEnum = wi.pEnum->Next(WBEM_NO_WAIT, 1, &wi.pclsObj, &uReturn);
		DebugPrintf(SV_LOG_ALWAYS, "hEnum = %x\n", hEnum);

		if (FAILED(hEnum)){
			throw NON_RETRYABLE_ERROR_EXCEPTION << " wmi method enumeration for query " << query << " failed with error: "
				<< std::hex << hres;
		}

		if (hEnum == WBEM_S_FALSE) {
			DebugPrintf(SV_LOG_ALWAYS, "Remote process %u exited.\n", pid);
			done = true;
		}		

		DebugPrintf(SV_LOG_ALWAYS, "Objects returned %lu\n", uReturn);
	}

	void WindowsRemoteConnection::netconnect()
	{
		if (!networkShareconnected) {

			boost::mutex::scoped_lock guard(m_networkShareMutex);
			if (!networkShareconnected) {


				std::string strRemoteName = std::string("\\\\") + ip + std::string("\\") + rootFolder;
				boost::replace_all(strRemoteName, remoteApiLib::pathSeperatorToReplace(), remoteApiLib::pathSeperator());
				boost::algorithm::trim(strRemoteName);
				boost::replace_first(strRemoteName, ":", "$");

				NETRESOURCE netResource;
				memset((void*)&netResource, 0, sizeof (NETRESOURCE));
				netResource.dwType = RESOURCETYPE_ANY;
				netResource.lpLocalName = NULL;
				netResource.lpRemoteName = (LPSTR)(strRemoteName.c_str());
				netResource.lpProvider = NULL;
				DWORD dwFlags = CONNECT_TEMPORARY;

				DWORD dwRetVal = WNetAddConnection2(&netResource, password.c_str(), username.c_str(), dwFlags);
				if (dwRetVal == ERROR_INVALID_PASSWORD || dwRetVal == ERROR_LOGON_FAILURE)
				{
					std::string err;
					convertErrorToString(lastKnownError(), err);
					throw CREDENTIAL_ERROR_EXCEPTION << "network share connection to  path: " << strRemoteName << " failed with error : " << dwRetVal << "(" << err << ")";
				}
				if ((dwRetVal != NO_ERROR) && (dwRetVal != ERROR_SESSION_CREDENTIAL_CONFLICT) ){

					std::string err;
					convertErrorToString(lastKnownError(), err);

					switch (dwRetVal)
					{
					case ERROR_ACCESS_DENIED:
						throw WmiErrorException(WmiErrorCode::AccessDenied)
							<< "network share connection to  path: " << strRemoteName 
							<< " failed with error : " << dwRetVal << "(" << err << ")";
						break;
					case ERROR_ACCOUNT_LOCKED_OUT:
						throw WmiErrorException(WmiErrorCode::LoginAccountLockedOut)
							<< "network share connection to  path: " << strRemoteName
							<< " failed with error : " << dwRetVal << "(" << err << ")";
						break;
					case ERROR_NETLOGON_NOT_STARTED:
						throw WmiErrorException(WmiErrorCode::LogonServiceNotStarted)
							<< "network share connection to  path: " << strRemoteName
							<< " failed with error : " << dwRetVal << "(" << err << ")";
						break;
					case ERROR_BAD_NET_NAME:
						throw WmiErrorException(WmiErrorCode::NetworkNotFound)
							<< "network share connection to  path: " << strRemoteName
							<< " failed with error : " << dwRetVal << "(" << err << ")";
						break;
					case ERROR_NETNAME_DELETED:
						throw WmiErrorException(WmiErrorCode::NetworkNotFound)
							<< "network share connection to  path: " << strRemoteName
							<< " failed with error : " << dwRetVal << "(" << err << ")";
						break;
					case ERROR_TRUSTED_RELATIONSHIP_FAILURE:
						throw WmiErrorException(WmiErrorCode::DomainTrustRelationshipFailed)
							<< "network share connection to  path: " << strRemoteName
							<< " failed with error : " << dwRetVal << "(" << err << ")";
						break;
					case ERROR_BAD_NETPATH:
						throw WmiErrorException(WmiErrorCode::NetworkNotFound)
							<< "network share connection to  path: " << strRemoteName
							<< " failed with error : " << dwRetVal << "(" << err << ")";
						break;
					case ERROR_ACCOUNT_DISABLED:
						throw WmiErrorException(WmiErrorCode::LoginAccountDisabled)
							<< "network share connection to  path: " << strRemoteName
							<< " failed with error : " << dwRetVal << "(" << err << ")";
						break;
					case ERROR_NO_LOGON_SERVERS:
						throw WmiErrorException(WmiErrorCode::LogonServersNotAvailable)
							<< "network share connection to  path: " << strRemoteName
							<< " failed with error : " << dwRetVal << "(" << err << ")";
						break;
					default :
						throw ERROR_EXCEPTION << "network share connection to  path: " 
							<< strRemoteName << " failed with error : " << dwRetVal 
							<< "(" << err << ")";
					}	
				}

				networkShareconnected = true;
			}
		}
	}

	void WindowsRemoteConnection::netdisconnect()
	{
		boost::mutex::scoped_lock guard(m_networkShareMutex);

		std::string strRemoteName = std::string("\\\\") + ip + std::string("\\") + rootFolder;
		boost::replace_all(strRemoteName, remoteApiLib::pathSeperatorToReplace(), remoteApiLib::pathSeperator());
		boost::algorithm::trim(strRemoteName);
		boost::replace_first(strRemoteName, ":", "$");

		DWORD dwRetVal = WNetCancelConnection2((LPCSTR)strRemoteName.c_str(), CONNECT_UPDATE_PROFILE, true);
		if (dwRetVal != NO_ERROR)
		{
			std::string err;
			convertErrorToString(lastKnownError(), err);
			DebugPrintf(SV_LOG_ERROR, "network share disconnection to path: %s failed with error : %s (%lu) ", strRemoteName.c_str(), err.c_str(), dwRetVal);
			
			// commented the below exception as this is called from destructor (which may be called from an exception handler)
			//throw ERROR_EXCEPTION << "network share disconnect to path: " << strRemoteName << " failed with error :" << dwRetVal << "(" << err << ")";
		}
		networkShareconnected = false;
	}

	void WindowsRemoteConnection::wmiconnect()
	{
		if (!wmiConnected) {

			boost::mutex::scoped_lock guard(m_wmiMutex);
			if (!wmiConnected) {

				HRESULT hres = CoInitializeEx(0, COINIT_MULTITHREADED);
				if (FAILED(hres)) {
					std::string err;
					convertErrorToString(lastKnownError(), err);
					throw ERROR_EXCEPTION << "initialize COM library (CoInitializeEx)  failed with result " << std::hex << hres << "(" << err << ")";
				}

				hres = CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_DEFAULT,
					RPC_C_IMP_LEVEL_IDENTIFY, 0, EOAC_NONE, 0);

				// Check if the securiy has already been initialized by someone in the same process.
				if (hres != RPC_E_TOO_LATE) {
					if (FAILED(hres)) {
						std::string err;
						convertErrorToString(lastKnownError(), err);
						throw ERROR_EXCEPTION << "initialize COM security (CoInitializeSecurity)  failed with result " << std::hex << hres << "(" << err << ")";
					}
				}

				hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID *)&m_wbemLocator);
				if (FAILED(hres)) {
					std::string err;
					convertErrorToString(lastKnownError(), err);
					throw ERROR_EXCEPTION << "create wbem instance (CoCreateInstance)  failed with result " << std::hex << hres << "(" << err << ")";
				}

				std::string wmiNameSpace = "\\\\" + ip + "\\" + "\\root\\cimv2";

				hres = m_wbemLocator->ConnectServer(_bstr_t(wmiNameSpace.c_str()),
					_bstr_t(username.c_str()), 
					_bstr_t(password.c_str()),
					0, NULL, 0, 0, &m_wbemSvc);
				if (FAILED(hres)){
					if (E_ACCESSDENIED == hres)
					{
						std::string err;
						convertErrorToString(lastKnownError(), err);
						throw CREDENTIAL_ERROR_EXCEPTION << "wmi connection to " << wmiNameSpace << " with user " << username << " failed with result " << std::hex << hres << "(" << err << ")";
					}
					std::string err;
					convertErrorToString(lastKnownError(), err);
					throw ERROR_EXCEPTION << "wmi connection to " << wmiNameSpace << " with user " << username << " failed with result " << std::hex << hres << "(" << err << ")";
				}
		
				std::string domain = "WORKGROUP";
				std::string usernameWoDomain = username;

				if (username.find_first_of("\\") != std::string::npos) {
					domain = username.substr(0, username.find_first_of("\\"));
					usernameWoDomain = username.substr(username.find_first_of("\\") + 1);
				}
				else if (username.find_first_of("@") != std::string::npos) {
					usernameWoDomain = username.substr(0, username.find_first_of("@"));
					domain = username.substr(username.find_first_of("@") + 1);
				}

				COAUTHIDENTITY userAuthInfo;
				memset(&userAuthInfo, 0, sizeof(COAUTHIDENTITY));
				userAuthInfo.PasswordLength = password.length();
				userAuthInfo.Password = (USHORT*)password.c_str();
				userAuthInfo.DomainLength = domain.length();
				userAuthInfo.Domain = (USHORT*)domain.c_str();
				userAuthInfo.UserLength = usernameWoDomain.length();
				userAuthInfo.User = (USHORT*)usernameWoDomain.c_str();
				userAuthInfo.Flags = SEC_WINNT_AUTH_IDENTITY_ANSI;

				hres = CoSetProxyBlanket(m_wbemSvc, RPC_C_AUTHN_DEFAULT, RPC_C_AUTHZ_DEFAULT, COLE_DEFAULT_PRINCIPAL,
					RPC_C_AUTHN_LEVEL_PKT_PRIVACY, RPC_C_IMP_LEVEL_IMPERSONATE, &userAuthInfo, EOAC_NONE);

				if (FAILED(hres)){
					std::string err;
					convertErrorToString(lastKnownError(), err);
					throw ERROR_EXCEPTION << "create wbem security proxy (CoSetProxyBlanket) for " << domain << "\\" << usernameWoDomain
						<< " failed with result " << std::hex << hres << "(" << err << ")";
				}

				wmiConnected = true;
			}
		}
	}

	void WindowsRemoteConnection::wmidisconnect()
	{
		boost::mutex::scoped_lock guard(m_wmiMutex);

		if (m_wbemSvc) {
			m_wbemSvc->Release();
			m_wbemSvc = NULL;
		}

		if (m_wbemLocator) {
			m_wbemLocator->Release();
			m_wbemLocator = NULL;
		}

		CoUninitialize();
		wmiConnected = false;
	}

} // remoteApiLib

