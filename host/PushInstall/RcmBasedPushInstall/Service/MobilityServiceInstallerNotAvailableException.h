/*
+------------------------------------------------------------------------------------+
Copyright(c) Microsoft Corp.
+------------------------------------------------------------------------------------+
File		:	MobilityServiceInstallerNotAvailableException.h

Description	:   Exception thrown when Mobility service installer software is not available
				for a specific operating system flavour.

+------------------------------------------------------------------------------------+
*/

#ifndef __MOBILITYSERVCE_INSTALLER_NOTAVAILABLE_H
#define __MOBILITYSERVCE_INSTALLER_NOTAVAILABLE_H

#include <string>

namespace PI
{
	/// \brief Exception thrown when Mobility service installer software is not available
	/// for a specific operating system flavour.
	class MobilityServiceInstallerNotAvailableException : public std::logic_error
	{
	public:
		MobilityServiceInstallerNotAvailableException(const std::string &operatingSystemVersion) 
			: std::logic_error(
			"Mobility service installer for " +
			operatingSystemVersion +
			" is not available.") { };
	};
}

#endif