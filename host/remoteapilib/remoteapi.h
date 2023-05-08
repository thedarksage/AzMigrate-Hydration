///
/// \file remoteapi.h
///
/// \brief
///


#ifndef INMAGE_REMOTE_API_H
#define INMAGE_REMOTE_API_H


#include "remoteconnection.h"


namespace remoteApiLib {

	class RemoteApi {
	public:
		RemoteApi() {}
		~RemoteApi() {}

		void copyFiles(const RemoteConnection & connection, const std::vector<std::string> & filesToCopy, const std::string & folderonRemoteMachine)
		{
			connection.impl()->copyFiles(filesToCopy, folderonRemoteMachine);
		}

		void copyFile(const RemoteConnection & connection, const std::string & fileToCopy, const std::string & filePathonRemoteMachine)
		{
			connection.impl()->copyFile(fileToCopy, filePathonRemoteMachine);
		}

		void writeFile(const RemoteConnection & connection, const void * data, int length, const std::string & filePathonRemoteMachine)
		{
			connection.impl()->writeFile(data, length, filePathonRemoteMachine);
		}


		void run(const RemoteConnection & connection, const std::string & executableonRemoteMachine, unsigned int executionTimeoutInSecs)
		{
			return connection.impl()->run(executableonRemoteMachine, executionTimeoutInSecs);
		}


	protected:

	private:

	};

	void global_init();
	void global_exit();

} // remoteApiLib

#endif