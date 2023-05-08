///
/// \file unixremoteconnection.h
///
/// \brief
///


#ifndef INMAGE_UNIX_REMOTE_CONNECTION_H
#define INMAGE_UNIX_REMOTE_CONNECTION_H


#include "remoteconnectionimpl.h"
#include <libssh2.h>
#include <libssh2_sftp.h>

namespace remoteApiLib {

	class UnixRemoteConnection :public RemoteConnectionImpl {

	public:
		UnixRemoteConnection(const std::string &ip,
			const  std::string & username,
			const std::string & password,
			const std::string &rootFolder = std::string())
			:ip(ip), username(username), password(password), rootFolder(rootFolder)
		{
		}

		virtual ~UnixRemoteConnection()
		{
		}

		virtual void copyFiles(const std::vector<std::string> & filesToCopy, const std::string & folderonRemoteMachine);
		virtual void copyFile(const std::string & fileToCopy, const std::string & filePathonRemoteMachine);
		virtual void writeFile(const void * data, int length, const std::string & filePathonRemoteMachine);
		virtual std::string readFile(const std::string & filePathonRemoteMachine, bool & fileNotAvailable);

		virtual void run(const std::string & executableonRemoteMachine, unsigned int executionTimeoutInSecs);
		virtual void remove(const std::string  & pathonRemoteMachine) {}

		virtual std::string osName(const std::string & os_script = std::string(), const std::string & temporaryPathonRemoteMachine = std::string());

	protected:

	private:

		void run(const std::string & executableonRemoteMachine, unsigned int executionTimeoutInSecs, std::string &  output);

		std::string osScriptPath();


		std::string ip;
		std::string username;
		std::string password;
		std::string rootFolder;

	};

	class UnixSsh {

	public:

		friend class UnixRemoteConnection;
		UnixSsh(const std::string & ip, const std::string & username, const std::string & password);
		~UnixSsh();

		int select(int socket_fd, LIBSSH2_SESSION *session);

	private:

		std::string ip;
		std::string username;
		std::string password;

		LIBSSH2_SESSION *session;
		LIBSSH2_CHANNEL *channel;
		int sock;
	};


	class UnixSftp {

	public:

		friend class UnixRemoteConnection;
		UnixSftp(const std::string & ip, const std::string & username, const std::string & password);
		~UnixSftp();


		void copy(const std::string & src, const std::string & dst);
		int select(int socket_fd, LIBSSH2_SESSION *session);



	private:


		void writeFile(const void * data, int length, const std::string & dst);
		std::string readFile(const std::string & filePathonRemoteMachine, bool & fileNotAvailable);

		void closeFtpHandle(LIBSSH2_SFTP_HANDLE ** sftp_handle);
		void closeHandle(FILE *fp);

		std::string ip;
		std::string username;
		std::string password;

		LIBSSH2_SESSION *session;
		LIBSSH2_SFTP *sftp_session;
		int sock;
	};

} // remoteApiLib

#endif