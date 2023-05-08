#ifndef REGISTERMTMAJOR__H
#define REGISTERMTMAJOR__H

#include <string>

namespace MTRegistration
{

	void FetchMTRegistrationDetailsFromCS(const std::string & certPath, const std::string & hivePath);
	void importMTRegistryHive(const std::string & hivePath);
	void importMTCert(const std::string & certPath);

	void writeMTRegistrationContent(const std::string & location, const std::string & content);
	bool SetPrivilege(LPCSTR privilege, bool set);
};

#endif