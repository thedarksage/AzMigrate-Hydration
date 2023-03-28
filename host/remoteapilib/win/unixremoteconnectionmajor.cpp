///
/// \file unixremoteconnectionmajor.cpp
///
/// \brief
///

#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string.hpp>

#include  "unixremoteconnection.h"
#include  "programfullpath.h"

namespace remoteApiLib {

	std::string UnixRemoteConnection::osScriptPath()
	{
		std::string fullName;
		std::string osScript;

		getProgramFullPath("", fullName);
		if (!fullName.empty()) {
			
			boost::replace_all(fullName, "\\", "/");
			boost::algorithm::trim(fullName);

			std::string::size_type idx = fullName.find_last_of("/");
			if (std::string::npos != idx) {
				osScript = fullName.substr(0,idx);
			}
		}

		if (!boost::algorithm::ends_with(osScript, "/"))
			osScript += "/";

		osScript += "OS_details.sh";
		return osScript;
	}

	
};
