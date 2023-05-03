///
/// \file remoteconnectionimpl.h
///
/// \brief
///


#ifndef INMAGE_REMOTE_CONNECTIONIMPL_H
#define INMAGE_REMOTE_CONNECTIONIMPL_H

#include <string>
#include <vector>

namespace remoteApiLib {

	class RemoteConnectionImpl {
	public:
		RemoteConnectionImpl() {}

		virtual ~RemoteConnectionImpl() {}

		virtual void copyFiles(const std::vector<std::string> & filesToCopy, const std::string & folderonRemoteMachine) = 0;
		virtual void copyFile(const std::string & fileToCopy, const std::string & filePathonRemoteMachine) = 0;
		virtual void writeFile(const void * data, int length, const std::string & filePathonRemoteMachine) = 0;
		virtual std::string readFile(const std::string & filePathonRemoteMachine, bool & fileNotAvailable) = 0;

		virtual void run(const std::string & executableonRemoteMachine, unsigned int executionTimeoutInSecs) = 0;
		virtual void remove(const std::string  & pathonRemoteMachine) = 0;

		virtual std::string osName(const std::string & os_script = std::string(), const std::string & temporaryPathonRemoteMachine = std::string()) = 0;


	protected:

	private:
	};

} // remoteApiLib

#endif