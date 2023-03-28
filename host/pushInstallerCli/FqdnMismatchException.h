///
/// \file FqdnMismatchException.h
///
/// \brief
///

#ifndef __FQDNVALIDATIONFAILED_H
#define __FQDNVALIDATIONFAILED_H

#include <string>

namespace PI
{
	/// \brief Exception thrown when there is a mismatch in host name reported by discovery and on-prem.
	class FqdnMismatchException : public std::logic_error
	{
	public:
		FqdnMismatchException(const std::string &hostnameReceived, const std::string &onPremHostname)
			: std::logic_error(
				"FQDN from discovery " +
				hostnameReceived +
				" did not match the on-premise value: " +
				onPremHostname +
				".") { };
	};
}

#endif
