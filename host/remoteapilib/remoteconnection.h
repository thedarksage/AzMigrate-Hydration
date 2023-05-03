///
/// \file remoteconnection.h
///
/// \brief
///


#ifndef INMAGE_REMOTE_CONNECTION_H
#define INMAGE_REMOTE_CONNECTION_H


#include <string>
#include <boost/shared_ptr.hpp>

#include "remoteconnectionimpl.h"

namespace remoteApiLib {

	class RemoteConnection {
	public:
		RemoteConnection(const std::string & remoteplatform, 
			const std::string &ip, 
			const  std::string & username,
			const std::string & password,
			const std::string &rootFolder = std::string());

		~RemoteConnection() { m_pimpl.reset();  }

		boost::shared_ptr<RemoteConnectionImpl> impl() const { return m_pimpl; }

	protected:

	private:
		boost::shared_ptr<RemoteConnectionImpl> m_pimpl;
	};

} // remoteApiLib

#endif