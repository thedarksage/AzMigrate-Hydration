/*
+------------------------------------------------------------------------------------+
Copyright(c) Microsoft Corp.
+------------------------------------------------------------------------------------+
File		:	PushClientNotFoundException.h

Description	:   Thrown when push client software is not found for the provided distro or OS

+------------------------------------------------------------------------------------+
*/

#ifndef INMAGE_PI_PUSHCLIENT_NOT_FOUND_EXCEPTION_H
#define INMAGE_PI_PUSHCLIENT_NOT_FOUND_EXCEPTION_H

#include <string>

namespace PI
{
	/// \brief Thrown when push client software is not found for the provided distro or OS
	class PushClientNotFoundException : public std::logic_error
	{
	public:
		PushClientNotFoundException(const std::string &osOrDistro)
			: std::logic_error(
				"Push client is not found for distro : " +
				osOrDistro +
				"!") { };
	};
}

#endif