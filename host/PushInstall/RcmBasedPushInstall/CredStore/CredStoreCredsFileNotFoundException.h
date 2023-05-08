/*
+------------------------------------------------------------------------------------+
Copyright(c) Microsoft Corp.
+------------------------------------------------------------------------------------+
File		:	CredStoreCredsFileNotFoundException.h

Description	:   Thrown when the cred store credentials file is not found in the specified location

+------------------------------------------------------------------------------------+
*/

#ifndef __CREDSTORECREDSFILENOTFOUND_EXCEPTION_H__
#define __CREDSTORECREDSFILENOTFOUND_EXCEPTION_H__

#include <string>

namespace PI
{
	/// \brief thrown when the cred store credentials file is not found in the specified location
	class CredStoreCredsFileNotFoundException : public std::logic_error
	{
	public:
		CredStoreCredsFileNotFoundException(const std::string &filePath) :
			std::logic_error(
				"Credential store creds file is not present in the location : " +
				filePath) { };
	};
}

#endif