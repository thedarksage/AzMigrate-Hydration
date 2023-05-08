/*
+------------------------------------------------------------------------------------+
Copyright(c) Microsoft Corp.
+------------------------------------------------------------------------------------+
File		:	CredStoreAccountNotFoundException.h

Description	:   Thrown when the requested account id is not found in credential store

+------------------------------------------------------------------------------------+
*/

#ifndef __CREDSTOREACCOUNTNOTFOUND_EXCEPTION_H__
#define __CREDSTOREACCOUNTNOTFOUND_EXCEPTION_H__

#include <string>

namespace PI
{
	/// \brief thrown when the requested account id is not found in credential store
	class CredStoreAccountNotFoundException : public std::logic_error
	{
	public:
		CredStoreAccountNotFoundException(const std::string &accountId) : 
			std::logic_error("Account not found in credential store with id " + accountId) { };
	};
}

#endif