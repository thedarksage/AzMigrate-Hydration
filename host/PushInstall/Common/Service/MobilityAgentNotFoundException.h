/*
+------------------------------------------------------------------------------------+
Copyright(c) Microsoft Corp.
+------------------------------------------------------------------------------------+
File		:	MobilityAgentNotFoundException.h

Description	:   Thrown when mobility agent software is not found for the provided distro or OS

+------------------------------------------------------------------------------------+
*/

#ifndef INMAGE_PI_MOBILITYAGENT_NOT_FOUND_EXCEPTION_H
#define INMAGE_PI_MOBILITYAGENT_NOT_FOUND_EXCEPTION_H

#include <string>

namespace PI
{
	/// \brief Thrown when mobility agent software is not found for the provided distro or OS
	class MobilityAgentNotFoundException : public std::logic_error
	{
	public:
		MobilityAgentNotFoundException(const std::string &osOrDistro)
			: std::logic_error(
				"Mobility agent is not found for distro : " +
				osOrDistro +
				"!") { };
	};
}

#endif