///
/// \file pushjobmajor.cpp
///
/// \brief
///

#include <windows.h>
#include "InMageSecurity.h"

#include "errorexception.h"
#include "pushjobdefinition.h"
#include "pushjob.h"

//#include "securityutils.h"

namespace PI {


	void PushJob::encryptcredentials(std::string & encrypted_passwd, std::string & encryption_key)
	{
		std::string errmsg;
		// std::string binary_encrypted_key, binary_encrypted_passwd;
		if (TRUE != EncryptCredentials(m_jobdefinition.domainName(), m_jobdefinition.userName(), m_jobdefinition.password, encryption_key, encrypted_passwd, false, errmsg))
		{
			throw ERROR_EXCEPTION << "failed to create encrypted password for job " << m_jobdefinition.jobId() << " with error " << errmsg;
		}

		// encryption_key = securitylib::base64Encode(binary_encrypted_key.c_str(), binary_encrypted_key.length());
		// encrypted_passwd = securitylib::base64Encode(binary_encrypted_passwd.c_str(), binary_encrypted_passwd.length());
	}
}