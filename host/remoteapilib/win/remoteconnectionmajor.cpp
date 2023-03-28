///
/// \file remoteconnectionmajor.cpp
///
/// \brief
///

#include "remoteconnection.h"
#include "supportedplatforms.h"
#include "errorexception.h"
#include "windowsremoteconnection.h"
#include "unixremoteconnection.h"

#include "portable.h"
#include "logger.h"
#include <boost/shared_ptr.hpp>
#include <string>

namespace remoteApiLib {

	RemoteConnection::RemoteConnection(const std::string & remoteplatform,
		const std::string & ip,
		const  std::string & username,
		const std::string & password,
		const std::string &rootFolder)
	{
		if (remoteplatform == windows_platform) {
			m_pimpl.reset(new WindowsRemoteConnection(ip, username, password, rootFolder));
		}
		else if (remoteplatform == unix_platform) {
			m_pimpl.reset(new UnixRemoteConnection(ip, username, password, rootFolder));
		}
		else {
			throw ERROR_EXCEPTION << unsupported_platform << " : " << remoteplatform;
		}
		DebugPrintf(SV_LOG_DEBUG, "Remote Connection platform :%s , ip:%s , username:*****, password:*****, rootFolder:%s.\n",
			remoteplatform.c_str(),
			ip.c_str(),
			rootFolder.c_str());
	}

} // remoteApiLib

