///
/// \file remoteconnectionmajor.cpp
///
/// \brief
///

#include "remoteconnection.h"
#include "supportedplatforms.h"
#include "errorexception.h"
#include "unixremoteconnection.h"

#include <boost/shared_ptr.hpp>
#include <string>

namespace remoteApiLib {

	RemoteConnection::RemoteConnection(const std::string & remoteplatform,
		const std::string &ip,
		const  std::string & username,
		const std::string & password,
		const std::string &rootFolder)
	{
        if (remoteplatform == unix_platform) {
			m_pimpl.reset(new UnixRemoteConnection(ip, username, password, rootFolder));
		}
		else {
			throw ERROR_EXCEPTION << unsupported_platform << " : " << remoteplatform;
		}
	}

} // remoteApiLib

