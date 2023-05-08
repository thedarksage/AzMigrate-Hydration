/*
+------------------------------------------------------------------------------------+
Copyright(c) Microsoft Corp.
+------------------------------------------------------------------------------------+
File		:	CredStorePassphraseEmptyException.h

Description	:   Thrown when passphrase in the credential store is empty.

+------------------------------------------------------------------------------------+
*/

#ifndef __CREDSTOREPASSPHRASEEMPTY_EXCEPTION_H__
#define __CREDSTOREPASSPHRASEEMPTY_EXCEPTION_H__

#include <string>

namespace PI
{
	/// \brief thrown when passphrase in the credential store is empty.
	class CredStorePassphraseEmptyException : public std::logic_error
	{
	public:
		CredStorePassphraseEmptyException(const std::string &credsFilePath) :
			std::logic_error(
				"Passphrase is empty in the credential store creds file path : " +
				credsFilePath) { };
	};
}

#endif