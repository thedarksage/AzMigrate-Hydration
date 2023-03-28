/*
+------------------------------------------------------------------------------------+
Copyright(c) Microsoft Corp.
+------------------------------------------------------------------------------------+
File		:	CredStoreUnsupportedVersionException.h

Description	:   Thrown when credential store version is not the expected version
				as per decryption algorithm.

+------------------------------------------------------------------------------------+
*/

#ifndef __CREDSTOREVERSIONMISMATCH_EXCEPTION_H__
#define __CREDSTOREVERSIONMISMATCH_EXCEPTION_H__

#include <string>
#include <boost/algorithm/algorithm.hpp>
#include <boost/algorithm/string/join.hpp>

namespace PI
{
	/// \brief thrown when credential store version is not the expected version
	/// as per decyption algorithm
	class CredStoreUnsupportedVersionException : public std::logic_error
	{
	public:
		CredStoreUnsupportedVersionException(const std::string &unsupportedVersion, const std::vector<std::string> &supportedVersions) :
			std::logic_error(
			"Unsupported credential store version found. Version found in cred file : " +
			unsupportedVersion +
			". Expected version : " +
			boost::algorithm::join(supportedVersions, ",")) { };
	};
}

#endif