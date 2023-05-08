///
/// \file unixremoteconnection.cpp
///
/// \brief
///


#include <boost/thread/mutex.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/bind.hpp>

#include <algorithm>


#include "converterrortostringminor.h"
#include "errorexception.h"
#include "errorexceptionmajor.h"
#include "scopeguard.h"
#include "programfullpath.h"
#include "supportedplatforms.h"
#include "unixremoteconnection.h"
#include "credentialerrorexception.h"
#include "nonretryableerrorexception.h"
#include "RemoteApiErrorException.h"
#include "RemoteApiErrorCode.h"

#include "logger.h"
#include "portable.h"

#ifndef WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#endif

#include <sys/types.h> 
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>

#include <algorithm>

#include <iostream>
using namespace std;

namespace remoteApiLib {

	void UnixRemoteConnection::copyFiles(const std::vector<std::string> & filesToCopy, const std::string & folderonRemoteMachine)
	{
		for (size_t i = 0; i < filesToCopy.size(); ++i)
			copyFile(filesToCopy[i], folderonRemoteMachine);
	}

	void UnixRemoteConnection::copyFile(const std::string & fileToCopy, const std::string & filePathonRemoteMachine)
	{
		std::string src = fileToCopy;

		std::string dst;

		if (!boost::algorithm::starts_with(filePathonRemoteMachine, remoteApiLib::pathSeperator(unix_idx))) {
			dst = remoteApiLib::pathSeperator(unix_idx);
		}
		dst += filePathonRemoteMachine;

		boost::replace_all(src, remoteApiLib::pathSeperatorToReplace(), remoteApiLib::pathSeperator());
		boost::replace_all(dst, remoteApiLib::pathSeperator(remoteApiLib::windows_idx), remoteApiLib::pathSeperator(remoteApiLib::unix_idx));
		boost::algorithm::trim(src);
		boost::algorithm::trim(dst);

		if (boost::algorithm::ends_with(dst, remoteApiLib::pathSeperator(unix_idx))) {
			std::string::size_type idx = src.find_last_of(remoteApiLib::pathSeperator(unix_idx));
			if (std::string::npos != idx) {
				dst += src.substr(idx + 1);
			}
		}

		UnixSftp ui(ip, username, password);
		ui.copy(src, dst);
	}

	void UnixRemoteConnection::writeFile(const void * data, int length, const std::string & filePathonRemoteMachine)
	{
		std::string dst;

		if (!boost::algorithm::starts_with(filePathonRemoteMachine, remoteApiLib::pathSeperator(unix_idx))) {
			dst = remoteApiLib::pathSeperator(unix_idx);
		}

		dst += filePathonRemoteMachine;
		boost::replace_all(dst, remoteApiLib::pathSeperator(remoteApiLib::windows_idx), remoteApiLib::pathSeperator(remoteApiLib::unix_idx));
		boost::algorithm::trim(dst);

		UnixSftp ui(ip, username, password);
		ui.writeFile(data, length, dst);
	}

	std::string UnixRemoteConnection::readFile(const std::string & filePathonRemoteMachine, bool & fileNotAvailable)
	{
		std::string dst;

		if(!boost::algorithm::starts_with(filePathonRemoteMachine, remoteApiLib::pathSeperator(unix_idx))) {
			dst = remoteApiLib::pathSeperator(unix_idx);
		}

		dst += filePathonRemoteMachine;
		boost::replace_all(dst, remoteApiLib::pathSeperator(remoteApiLib::windows_idx), remoteApiLib::pathSeperator(remoteApiLib::unix_idx));
		boost::algorithm::trim(dst);

		UnixSftp ui(ip, username, password);
		return ui.readFile(dst,fileNotAvailable);
	}


	void UnixRemoteConnection::run(const std::string & executableonRemoteMachine, unsigned int executionTimeoutInSecs)
	{
		std::string output;
		run(executableonRemoteMachine, executionTimeoutInSecs, output);
	}

	void UnixRemoteConnection::run(const std::string & executableonRemoteMachine, unsigned int executionTimeoutInSecs, std::string &  output)
	{
		int rc;
		UnixSsh ui(ip, username, password);

		while ((rc = libssh2_channel_exec(ui.channel, executableonRemoteMachine.c_str())) == LIBSSH2_ERROR_EAGAIN)	{
			ui.select(ui.sock, ui.session);
		}

		if (rc) {
			char *errmsg = NULL;
			libssh2_session_last_error(ui.session, &errmsg, 0, 0);
			throw RemoteApiErrorException(RemoteApiErrorCode::RunFailed)
				<< "libssh2_channel_exec for " << ip
				<< " failed with error :" << libssh2_session_last_errno(ui.session) << " ( " << errmsg << ")";
		}

		unsigned int timeSpent = 0;
		for (;;) {
			// loop until we block 
			do	{
				char buffer[0x1000];
				rc = libssh2_channel_read(ui.channel, buffer, sizeof(buffer));
				if (rc > 0)	{
					output.append(buffer, rc);
				}
			} while (rc > 0);

			if (0 == rc) {
				DebugPrintf(SV_LOG_DEBUG, "command %s  returned %s\n", executableonRemoteMachine.c_str(), output.c_str());
				break;
			}

			// this is due to blocking that would occur otherwise so we loop on this condition
			if (rc == LIBSSH2_ERROR_EAGAIN || (LIBSSH2_ERROR_EAGAIN  == libssh2_session_last_errno(ui.session)))	{

				if (0 == ui.select(ui.sock, ui.session))
					timeSpent += 10;

				if (timeSpent > executionTimeoutInSecs){
					throw NON_RETRYABLE_ERROR_EXCEPTION << " remote job for " << ip << " did not complete even after waiting for " << timeSpent << "secs";
				}
			}
			else {
				char *errmsg = NULL;
				libssh2_session_last_error(ui.session, &errmsg, 0, 0);
				throw RemoteApiErrorException(RemoteApiErrorCode::RunFailed)
					<< "libssh2_channel_read for " << ip
					<< " failed with error :" << libssh2_session_last_errno(ui.session) << " ( " << errmsg << ")";
			}
		}

	}


	std::string UnixRemoteConnection::osName(const std::string & osScript, const std::string & temporaryPathonRemoteMachine)
	{
		std::string scriptname = osScript;
		if (scriptname.empty()) {
			scriptname = osScriptPath();
		}

		std::string dst = temporaryPathonRemoteMachine;
		if (dst.empty()){
			dst = "/tmp/remoteinstall/";
		}

		if (!boost::algorithm::ends_with(dst, remoteApiLib::pathSeperator(unix_idx))){
			dst += remoteApiLib::pathSeperator(unix_idx);
		}

		std::string commandLine =  dst;
		
		std::string::size_type idx = scriptname.find_last_of(remoteApiLib::pathSeperator());
		if (std::string::npos != idx) {
			commandLine += scriptname.substr(idx + 1);
			dst += scriptname.substr(idx + 1);
		}
		
		commandLine += " 1";

		std::string output;
		copyFile(scriptname, dst); 

		//std::string dos2unixConversion = "dos2unix " + dst;
		//std::string dos2unixConversion = "/bin/sed -i $'s/\r$//' " + dst;
		//run(dos2unixConversion);
		// max time for osname script to complete - 10 mins
		run(commandLine, 600,output);
		boost::algorithm::trim(output);
		if (output == "RHEL6U4-64"){
			output = "RHEL6-64";
		}


		return output;
	}




	UnixSsh::UnixSsh(const std::string & ip, const std::string & username, const std::string & password)
		:ip(ip), username(username), password(password)
	{

		int rc;
		sock = socket(AF_INET, SOCK_STREAM, 0);

		if (-1 == sock){
			std::string err;
			convertErrorToString(lastKnownError(), err);
			throw ERROR_EXCEPTION << "socket creation " << " failed with error :" << lastKnownError() << " ( " << err << ")";
		}

		struct sockaddr_in sin;
		sin.sin_family = AF_INET;
		sin.sin_port = htons(22);
		sin.sin_addr.s_addr = inet_addr(ip.c_str());

		if (connect(sock, (struct sockaddr*)(&sin), sizeof(struct sockaddr_in)) != 0) {
			std::string err;
			convertErrorToString(lastKnownError(), err);
			throw ERROR_EXCEPTION << "connect to" << ip << " failed with error :" << lastKnownError() << " ( " << err << ")";
		}

		session = libssh2_session_init();
		if (!session) {
			std::string err;
			convertErrorToString(lastKnownError(), err);
			throw ERROR_EXCEPTION << "session initialization for " << ip << " failed with error :" << lastKnownError() << " ( " << err << ")";
		}

		// tell libssh2 we want it all done non-blocking 
		libssh2_session_set_blocking(session, 0);

		while ((rc = libssh2_session_handshake(session, sock)) == LIBSSH2_ERROR_EAGAIN)
			;

		if (rc) {
			char *errmsg = NULL;
			libssh2_session_last_error(session, &errmsg, 0, 0);
			throw ERROR_EXCEPTION << "session initialization for " << ip << " failed with error :" <<
				libssh2_session_last_errno(session) << " ( " << errmsg << ")";
		}

		while ((rc = libssh2_userauth_password(session, username.c_str(), password.c_str())) == LIBSSH2_ERROR_EAGAIN)
			;

		if (rc) {
			char *errmsg = NULL;
			libssh2_session_last_error(session, &errmsg, 0, 0); 
			if (LIBSSH2_ERROR_AUTHENTICATION_FAILED == libssh2_session_last_errno(session)){
				throw CREDENTIAL_ERROR_EXCEPTION << "ssh session authentication for " << ip << " failed with error :" <<
					libssh2_session_last_errno(session) << " ( " << errmsg << ")";
			}
			throw ERROR_EXCEPTION << "ssh session authentication for " << ip << " failed with error :" <<
				libssh2_session_last_errno(session) << " ( " << errmsg << ")";
		}

		// exec non-blocking on the remove host
		while ((channel = libssh2_channel_open_session(session)) == NULL &&
			libssh2_session_last_error(session, NULL, NULL, 0) ==
			LIBSSH2_ERROR_EAGAIN)
		{
			select(sock, session);
		}

		if (channel == NULL)
		{
			char *errmsg = NULL;
			libssh2_session_last_error(session, &errmsg, 0, 0);
			throw ERROR_EXCEPTION << "ssh session channel creation for " << ip << " failed with error :" <<
				libssh2_session_last_errno(session) << " ( " << errmsg << ")";
		}
	}

	UnixSsh::~UnixSsh()
	{
		int rc;

		if (channel) {

			while ((rc = libssh2_channel_close(channel)) == LIBSSH2_ERROR_EAGAIN)
				select(sock, session);

			libssh2_channel_free(channel);
			channel = NULL;
		}

		libssh2_session_disconnect(session, "Normal Shutdown");
		libssh2_session_free(session);
		session = NULL;

#ifdef WIN32
		closesocket(sock);
#else
		close(sock);
#endif
		sock = -1;

	}

	int UnixSsh::select(int socketfd, LIBSSH2_SESSION *ssh2Session)
	{
		struct timeval timeout;
		int rc;
		fd_set fd;
		fd_set *writefd = NULL;
		fd_set *readfd = NULL;
		int dir;

		timeout.tv_sec = 10;
		timeout.tv_usec = 0;

		FD_ZERO(&fd);

		FD_SET(socketfd, &fd);

		//make sure we wait in the correct direction
		dir = libssh2_session_block_directions(ssh2Session);

		if (dir & LIBSSH2_SESSION_BLOCK_INBOUND)
			readfd = &fd;

		if (dir & LIBSSH2_SESSION_BLOCK_OUTBOUND)
			writefd = &fd;

		rc = ::select(socketfd + 1, readfd, writefd, NULL, &timeout);

		return rc;
	}



	UnixSftp::UnixSftp(const std::string & ip, const std::string & username, const std::string & password)
		:ip(ip), username(username), password(password)
	{

		int rc;
		sock = socket(AF_INET, SOCK_STREAM, 0);

		if (-1 == sock){
			std::string err;
			convertErrorToString(lastKnownError(), err);
			throw ERROR_EXCEPTION << "socket creation " << " failed with error :" << lastKnownError() << " ( " << err << ")";
		}

		struct sockaddr_in sin;
		sin.sin_family = AF_INET;
		sin.sin_port = htons(22);
		sin.sin_addr.s_addr = inet_addr(ip.c_str());;

		if (connect(sock, (struct sockaddr*)(&sin), sizeof(struct sockaddr_in)) != 0) {
			std::string err;
			convertErrorToString(lastKnownError(), err);
			throw ERROR_EXCEPTION << "connect to" << ip << " failed with error :" << lastKnownError() << " ( " << err << ")";
		}

		session = libssh2_session_init();
		if (!session) {
			std::string err;
			convertErrorToString(lastKnownError(), err);
			throw ERROR_EXCEPTION << "session initialization for " << ip << " failed with error :" << lastKnownError() << " ( " << err << ")";
		}

		// tell libssh2 we want it all done non-blocking 
		libssh2_session_set_blocking(session, 0);

		while ((rc = libssh2_session_handshake(session, sock)) == LIBSSH2_ERROR_EAGAIN){
			select(sock, session);
		}


		if (rc) {
			char *errmsg = NULL;
			libssh2_session_last_error(session, &errmsg, 0, 0);
			throw ERROR_EXCEPTION << "session initialization for " << ip << " failed with error :" <<
				libssh2_session_last_errno(session) << " ( " << errmsg << ")";
		}

		while ((rc = libssh2_userauth_password(session, username.c_str(), password.c_str())) == LIBSSH2_ERROR_EAGAIN) {
			select(sock, session);
		}

		if (rc) {
			char *errmsg = NULL;
			libssh2_session_last_error(session, &errmsg, 0, 0);
			if (LIBSSH2_ERROR_AUTHENTICATION_FAILED == libssh2_session_last_errno(session)){
				throw CREDENTIAL_ERROR_EXCEPTION << "ssh session authentication for " << ip << " failed with error :" <<
					libssh2_session_last_errno(session) << " ( " << errmsg << ")";
			}
			throw ERROR_EXCEPTION << "ssh session authentication for " << ip << " failed with error :" <<
				libssh2_session_last_errno(session) << " ( " << errmsg << ")";
		}

		while (((sftp_session = libssh2_sftp_init(session)) == NULL) && (libssh2_session_last_errno(session) == LIBSSH2_ERROR_EAGAIN)) {
			select(sock, session);
		}

		if (!sftp_session){
			char *errmsg = NULL;
			libssh2_session_last_error(session, &errmsg, 0, 0);
			throw ERROR_EXCEPTION << "session initialization for " << ip << " failed with error :" <<
				libssh2_session_last_errno(session) << " ( " << errmsg << ")";
		}

	}

	UnixSftp::~UnixSftp()
	{
		int rc;

		if (sftp_session) {

			libssh2_sftp_shutdown(sftp_session);
			sftp_session = NULL;
		}


		libssh2_session_disconnect(session, "Normal Shutdown");
		libssh2_session_free(session);
		session = NULL;

#ifdef WIN32
		closesocket(sock);
#else
		close(sock);
#endif
		sock = -1;

	}


	void UnixSftp::copy(const std::string & localFile, const std::string & filePathonRemoteMachine)
	{

		int rc = 0;
		size_t nread = 0;
		char * ptr = NULL;

		std::string src = localFile;
		std::string dst = filePathonRemoteMachine;
		std::string dstdir;

		LIBSSH2_SFTP_HANDLE *sftp_handle = NULL;
		FILE *srcFp = NULL;


		ON_BLOCK_EXIT(boost::bind(&UnixSftp::closeFtpHandle, this, &sftp_handle));
		ON_BLOCK_EXIT(boost::bind(&UnixSftp::closeHandle, this, srcFp));
		//boost::shared_ptr<void> dirpGuard(static_cast<void*>(0), boost::bind(fclose, srcFp));

		boost::replace_all(src, remoteApiLib::pathSeperatorToReplace(), remoteApiLib::pathSeperator());
		boost::replace_all(dst, remoteApiLib::pathSeperator(remoteApiLib::windows_idx), remoteApiLib::pathSeperator(remoteApiLib::unix_idx));
		boost::algorithm::trim(src);
		boost::algorithm::trim(dst);

		if (boost::algorithm::ends_with(dst, remoteApiLib::pathSeperator(remoteApiLib::unix_idx))) {
			dstdir = dst;
		}
		else{
			std::string::size_type idx = dst.find_last_of(remoteApiLib::pathSeperator(remoteApiLib::unix_idx));
			if (std::string::npos != idx) {
				dstdir += dst.substr(0, idx);
				dstdir += remoteApiLib::pathSeperator(remoteApiLib::unix_idx);
			}
		}


		if (boost::algorithm::ends_with(dst, remoteApiLib::pathSeperator(remoteApiLib::unix_idx))) {
			std::string::size_type idx = src.find_last_of(remoteApiLib::pathSeperator(remoteApiLib::unix_idx));
			if (std::string::npos != idx) {
				dst += src.substr(idx + 1);
			}
		}


		srcFp = fopen(src.c_str(), "rb");
		if (!srcFp) {
			std::string err;
			convertErrorToString(lastKnownError(), err);
			throw RemoteApiErrorException(RemoteApiErrorCode::CopyFailed)
				<< "open file failed for " << src
				<< "with error :" << lastKnownError() << " ( " << err << ")";
		}

		std::string path_to_create;
		std::string remaining_path = dstdir;
		std::string::size_type idx = remaining_path.find_first_of(remoteApiLib::pathSeperator(remoteApiLib::unix_idx));
		while (std::string::npos != idx) {
			path_to_create += remaining_path.substr(0, idx);
			path_to_create += remoteApiLib::pathSeperator(remoteApiLib::unix_idx);
			remaining_path = remaining_path.substr(idx + 1);
			idx = remaining_path.find_first_of(remoteApiLib::pathSeperator(remoteApiLib::unix_idx));

			// sftp_mkdir
			while (((rc = libssh2_sftp_mkdir(sftp_session, path_to_create.c_str(),
				LIBSSH2_SFTP_S_IRWXU |
				LIBSSH2_SFTP_S_IRGRP |
				LIBSSH2_SFTP_S_IXGRP)) != 0) && libssh2_session_last_error(session, NULL, NULL, 0) == LIBSSH2_ERROR_EAGAIN)	{
				select(sock, session);
			}
		}

		// mkdir operation can fail in following scenarios:
		// 1. insufficient storage - value of rc equals -31 at runtime in this case.
		// 2. directory already exists - value of rc equals -31 at runtime is this case.
		// 3. permissions - already have a pre-req to check the user account to be root.
		// 4. invalid characters in directory - checks not required.
		if (0 != rc && (LIBSSH2_ERROR_SFTP_PROTOCOL != rc)) {
			char *errmsg = NULL;
			libssh2_session_last_error(session, &errmsg, 0, 0);
			throw RemoteApiErrorException(RemoteApiErrorCode::CopyFailed)
				<< "libssh2_sftp_mkdir for host " << ip 
				<< " directory " << dstdir 
				<< " failed with error :" << libssh2_session_last_errno(session) << " ( " << errmsg << ")";
		}

		// open session  for dst

		while ((NULL == (sftp_handle =
			libssh2_sftp_open(sftp_session, dst.c_str(),
			LIBSSH2_FXF_WRITE | LIBSSH2_FXF_CREAT | LIBSSH2_FXF_TRUNC,
			LIBSSH2_SFTP_S_IRWXU |
			LIBSSH2_SFTP_S_IRGRP |
			LIBSSH2_SFTP_S_IXGRP))) && libssh2_session_last_error(session, NULL, NULL, 0) == LIBSSH2_ERROR_EAGAIN)	{
			select(sock, session);
		}

		// mkdir can fail with rc(=-31) in 2 cases, following are sftp_handle values corresponding the rc:
		// 1. directory already exists - sftp open will succeed and return non-null sftp_handle(any failures related to storage will be encountered while writing the file to dst).
		// 2. insufficient storage - directory would not be present and thus sftp open will fail returning a null sftp_handle.
		if (NULL == sftp_handle) {
			char *errmsg = NULL;
			// if sftp_handle==NULL and rc==-31 then directory exists but there is insufficient storage to open a session with target dst.
			// if sftp_handle==NULL and rc!=31 then sftp open would fail due to non-mkdir failures and should throw CopyFailed error code.
			RemoteApiErrorCode errCode = LIBSSH2_ERROR_SFTP_PROTOCOL == rc
				? RemoteApiErrorCode::StorageNotAvailableOnTarget
				: RemoteApiErrorCode::CopyFailed;

			libssh2_session_last_error(session, &errmsg, 0, 0);
			throw RemoteApiErrorException(errCode)
				<< "libssh2_sftp_open for host " << ip
				<< " file " << dst
				<< " failed with error :" << libssh2_session_last_errno(session) << " ( " << errmsg << ")";
		}

		do
		{
			char buffer[0x4000];
			nread = fread(buffer, 1, sizeof(buffer), srcFp);
			if (nread <= 0) {
				// end of file 
				break;
			} 
			ptr = buffer;

			do
			{
				/* write data in a loop until we block */
				rc = libssh2_sftp_write(sftp_handle, ptr, nread);
				if (rc == LIBSSH2_ERROR_EAGAIN)	{
					select(sock, session);
				}
				else if (rc < 0) {
					char *errmsg = NULL;
					libssh2_session_last_error(session, &errmsg, 0, 0);

					RemoteApiErrorCode osNameErrCode = 
						rc == LIBSSH2_ERROR_SFTP_PROTOCOL ?
						RemoteApiErrorCode::StorageNotAvailableOnTarget :
						RemoteApiErrorCode::CopyFailed;

					throw RemoteApiErrorException(osNameErrCode)
						<< "libssh2_sftp_write for " << ip
						<< " failed with error :" << libssh2_session_last_errno(session) << " ( " << errmsg << ")";
				}
				else {
					ptr += rc;
					nread -= rc;
				}

			} while (nread);

		} while (rc > 0);

	}


	void UnixSftp::writeFile(const void * data, int length, const std::string & filePathonRemoteMachine)
	{

		int rc = 0;
		size_t nwrite = 0;
		char * ptr = NULL;

		std::string dst = filePathonRemoteMachine;
		std::string dstdir;

		LIBSSH2_SFTP_HANDLE *sftp_handle = NULL;

		ON_BLOCK_EXIT(boost::bind(&UnixSftp::closeFtpHandle, this, &sftp_handle));

		boost::replace_all(dst, remoteApiLib::pathSeperator(remoteApiLib::windows_idx), remoteApiLib::pathSeperator(remoteApiLib::unix_idx));
		boost::algorithm::trim(dst);

		if (boost::algorithm::ends_with(dst, "/")) {
			throw ERROR_EXCEPTION << "invalid path " << dst << " for remote copy. expecting input as a file path (instead of folder)";
		}

		std::string::size_type idx = dst.find_last_of(remoteApiLib::pathSeperator(remoteApiLib::unix_idx));
		if (std::string::npos != idx) {
			dstdir += dst.substr(0, idx);
			dstdir += remoteApiLib::pathSeperator(remoteApiLib::unix_idx);
		}

		std::string path_to_create;
		std::string remaining_path = dstdir;
		idx = remaining_path.find_first_of(remoteApiLib::pathSeperator(remoteApiLib::unix_idx));
		while (std::string::npos != idx) {
			path_to_create += remaining_path.substr(0, idx);
			path_to_create += remoteApiLib::pathSeperator(remoteApiLib::unix_idx);
			remaining_path = remaining_path.substr(idx + 1);
			idx = remaining_path.find_first_of(remoteApiLib::pathSeperator(remoteApiLib::unix_idx));

			// sftp_mkdir
			while (((rc = libssh2_sftp_mkdir(sftp_session, path_to_create.c_str(),
				LIBSSH2_SFTP_S_IRWXU |
				LIBSSH2_SFTP_S_IRGRP |
				LIBSSH2_SFTP_S_IXGRP)) != 0) && libssh2_session_last_error(session, NULL, NULL, 0) == LIBSSH2_ERROR_EAGAIN)	{
				select(sock, session);
			}
		}


		if (0 != rc && (LIBSSH2_ERROR_SFTP_PROTOCOL != rc))  {
			char *errmsg = NULL;
			libssh2_session_last_error(session, &errmsg, 0, 0);
			throw ERROR_EXCEPTION << "libssh2_sftp_mkdir for host " << ip << " directory " << dstdir << " failed with error :" <<
				libssh2_session_last_errno(session) << " ( " << errmsg << ")";
		}

		// open session  for dst

		while ((NULL == (sftp_handle =
			libssh2_sftp_open(sftp_session, dst.c_str(),
			LIBSSH2_FXF_WRITE | LIBSSH2_FXF_CREAT | LIBSSH2_FXF_TRUNC,
			LIBSSH2_SFTP_S_IRWXU |
			LIBSSH2_SFTP_S_IRGRP |
			LIBSSH2_SFTP_S_IXGRP))) && libssh2_session_last_error(session, NULL, NULL, 0) == LIBSSH2_ERROR_EAGAIN)	{
			select(sock, session);
		}

		if (NULL == sftp_handle) {
			char *errmsg = NULL;
			libssh2_session_last_error(session, &errmsg, 0, 0);
			throw ERROR_EXCEPTION << "libssh2_sftp_open for host " << ip << " file " << dst << " failed with error :" <<
				libssh2_session_last_errno(session) << " ( " << errmsg << ")";
		}


		ptr = (char *)data;
		nwrite = length;
		do
		{
			/* write data in a loop until we block */
			rc = libssh2_sftp_write(sftp_handle, ptr, nwrite);
			if (rc == LIBSSH2_ERROR_EAGAIN)	{
				select(sock, session);
			}
			else if (rc < 0) {
				char *errmsg = NULL;
				libssh2_session_last_error(session, &errmsg, 0, 0);
				throw ERROR_EXCEPTION << "libssh2_sftp_write for " << ip << " file " << dst << " failed with error :" <<
					libssh2_session_last_errno(session) << " ( " << errmsg << ")";
			}
			else {
				ptr += rc;
				nwrite -= rc;
			}

		} while (nwrite);
	}

	std::string UnixSftp::readFile(const std::string & filePathonRemoteMachine, bool & fileNotAvailable)
	{

		int rc = 0;
		std::string dst = filePathonRemoteMachine;
		std::string result;
		fileNotAvailable = false;

		LIBSSH2_SFTP_HANDLE *sftp_handle = NULL;

		ON_BLOCK_EXIT(boost::bind(&UnixSftp::closeFtpHandle, this, &sftp_handle));

		boost::replace_all(dst, remoteApiLib::pathSeperator(remoteApiLib::windows_idx), remoteApiLib::pathSeperator(remoteApiLib::unix_idx));
		boost::algorithm::trim(dst);

		if (boost::algorithm::ends_with(dst, "/")) {
			throw ERROR_EXCEPTION << "invalid path " << dst << " . expecting input as a file path (instead of folder)";
		}

		// open session  for dst

		while ((NULL == (sftp_handle =
			libssh2_sftp_open(sftp_session, dst.c_str(),
			LIBSSH2_FXF_READ,
			LIBSSH2_SFTP_S_IRWXU |
			LIBSSH2_SFTP_S_IRGRP |
			LIBSSH2_SFTP_S_IXGRP))) && libssh2_session_last_error(session, NULL, NULL, 0) == LIBSSH2_ERROR_EAGAIN)	{
			select(sock, session);
		}

		if (NULL == sftp_handle) {

			if (LIBSSH2_ERROR_SFTP_PROTOCOL == libssh2_session_last_errno(session)) {
				fileNotAvailable = true;
				return std::string();
			}

			char *errmsg = NULL;
			libssh2_session_last_error(session, &errmsg, 0, 0);
			throw ERROR_EXCEPTION << "libssh2_sftp_open for host " << ip << " file " << dst << " failed with error :" <<
				libssh2_session_last_errno(session) << " ( " << errmsg << ")";
		}


		char buffer[0x4000];
		int buflen = 0x4000;

		do
		{
			/* read data in a loop until we block */
			rc = libssh2_sftp_read(sftp_handle, buffer, buflen);
			if (rc == LIBSSH2_ERROR_EAGAIN)	{
				select(sock, session);
			}
			else if (rc < 0) {
				char *errmsg = NULL;
				libssh2_session_last_error(session, &errmsg, 0, 0);
				throw ERROR_EXCEPTION << "libssh2_sftp_read for " << ip << " file " << dst << " failed with error :" <<
					libssh2_session_last_errno(session) << " ( " << errmsg << ")";
			}
			else if (rc == 0) {
				break;
			}
			else {
				result.append(buffer, rc);
			}

		} while (true);

		return result;
	}



	int UnixSftp::select(int socketfd, LIBSSH2_SESSION *ssh2Session)
	{
		struct timeval timeout;
		int rc;
		fd_set fd;
		fd_set *writefd = NULL;
		fd_set *readfd = NULL;
		int dir;

		timeout.tv_sec = 10;
		timeout.tv_usec = 0;

		FD_ZERO(&fd);

		FD_SET(socketfd, &fd);

		//make sure we wait in the correct direction
		dir = libssh2_session_block_directions(ssh2Session);

		if (dir & LIBSSH2_SESSION_BLOCK_INBOUND)
			readfd = &fd;

		if (dir & LIBSSH2_SESSION_BLOCK_OUTBOUND)
			writefd = &fd;

		rc = ::select(socketfd + 1, readfd, writefd, NULL, &timeout);

		return rc;
	}

	void UnixSftp::closeFtpHandle(LIBSSH2_SFTP_HANDLE ** psftp_handle)
	{
		int rc;
		std::cout << "close ftp handle\n";
		if (psftp_handle && *psftp_handle) {

			while ((rc = libssh2_sftp_close(*psftp_handle)) == LIBSSH2_ERROR_EAGAIN) {
				select(sock, session);
			}

			*psftp_handle = NULL;
		}
	}

	void UnixSftp::closeHandle(FILE * fp)
	{
		std::cout << "close handle\n";
		if (fp){
			fclose(fp);
		}

	}



} // remoteApiLib

