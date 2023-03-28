///
/// \file windowsremoteconnection.h
///
/// \brief
///


#ifndef INMAGE_WINDOWS_REMOTE_CONNECTION_H
#define INMAGE_WINDOWS_REMOTE_CONNECTION_H

#include <windows.h>
#include <comutil.h>
#include <comdef.h>
#include <Wbemcli.h>

#include <string>
#include <vector>

#include "remoteconnectionimpl.h"




#include <boost/thread/mutex.hpp>

namespace remoteApiLib {
	
	class WindowsRemoteConnection :public RemoteConnectionImpl {

	public:
		WindowsRemoteConnection(const std::string &ip,
			const  std::string & username,
			const std::string & password,
			const std::string &rootFolder = std::string())
			:ip(ip), username(username), password(password), rootFolder(rootFolder), 
			networkShareconnected(false), wmiConnected(false),
			m_wbemLocator(0), m_wbemSvc(0)
		{
			if (this -> rootFolder.empty()) {
				this -> rootFolder = std::string("C$");
			}
		}

		virtual ~WindowsRemoteConnection()
		{
			if (networkShareconnected) {
				netdisconnect();
			}

			if (wmiConnected) {
				wmidisconnect();
			}

		}

		virtual void copyFiles(const std::vector<std::string> & filesToCopy, const std::string & folderonRemoteMachine);
		virtual void copyFile(const std::string & fileToCopy, const std::string & filePathonRemoteMachine);
		virtual void writeFile(const void * data, int length, const std::string & filePathonRemoteMachine);
		virtual std::string readFile(const std::string & filePathonRemoteMachine, bool & fileNotAvailable);

		virtual void run(const std::string & executableonRemoteMachine, unsigned int executionTimeoutInSecs);
		virtual void remove(const std::string  & pathonRemoteMachine) {}

		virtual std::string osName(const std::string & os_script = std::string(), const std::string & temporaryPathonRemoteMachine = std::string()) { return "Win"; }

	protected:

	private:
		void netconnect();
		void netdisconnect();

		void wmiconnect();
		void wmidisconnect();

		void wait(unsigned int pid, unsigned int executionTimeoutInSecs);
		void waitonce(unsigned int pid, bool & done);

		std::string ip;
		std::string username;
		std::string password;
		std::string rootFolder;
		bool networkShareconnected;
		bool wmiConnected;

		IWbemLocator * m_wbemLocator;
		IWbemServices *m_wbemSvc;

		boost::mutex m_networkShareMutex;
		boost::mutex m_wmiMutex;

	};

	class WmiInternal
	{
	public:
		WmiInternal()
			:pClass(NULL), pInParamsDefinition(NULL), pClassInstance(NULL), pOutParam(NULL), pclsObj(NULL), pEnum(NULL)
		{

		}

		~WmiInternal()
		{
			if (pClass){
				pClass->Release();
				pClass = NULL;
			}

			if (pInParamsDefinition){
				pInParamsDefinition->Release();
				pInParamsDefinition = NULL;
			}

			if (pClassInstance){
				pClassInstance->Release();
				pClassInstance = NULL;
			}

			if (pOutParam) {
				pOutParam->Release();
				pOutParam = NULL;
			}


			if (pclsObj) {
				pclsObj->Release();
				pclsObj = NULL;
			}


			if (pEnum) {
				pEnum->Release();
				pEnum = NULL;
			}
		}

	public:

		IWbemClassObject* pClass;
		IWbemClassObject* pInParamsDefinition;
		IWbemClassObject* pClassInstance;
		IWbemClassObject* pOutParam;
		IEnumWbemClassObject *pEnum;
		IWbemClassObject *pclsObj;
	};

} // remoteApiLib

#endif