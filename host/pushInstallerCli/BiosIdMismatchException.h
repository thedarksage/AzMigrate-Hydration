///
/// \file BiosIdMismatchException.h
///
/// \brief
///

#ifndef __BIOSIDVALIDATIONFAILED_H
#define __BIOSIDVALIDATIONFAILED_H

#include <string>

namespace PI
{
	/// \brief Exception thrown when there is a mismatch in bios id reported by discovery and on-prem.
	class BiosIdMismatchException : public std::logic_error
	{
	public:
		BiosIdMismatchException(const std::string &biosIdReceived, const std::string &onPremBiosId)
			: std::logic_error(
				"Bios id from discovery " +
				biosIdReceived +
				" did not match the on-premise value: " +
				onPremBiosId +
				".") { };
	};
}

#endif
