///
/// \file CsPushInstallSWMgr.cpp
///
/// \brief
///

#include "CsPushInstallSWMgr.h"
#include "errorexception.h"
#include "CsCommunicationImpl.h"
#include "SignMgr.h"

namespace PI {

	CsPushInstallSWMgrPtr g_cspushinstallswmgr;
	bool g_cspushinstallswmgrcreated = false;
	boost::mutex g_cspushinstallswmgrMutex;

	CsPushInstallSWMgrPtr CsPushInstallSWMgr::Instance()
	{
		if (!g_cspushinstallswmgrcreated) {
			boost::mutex::scoped_lock guard(g_cspushinstallswmgrMutex);
			if (!g_cspushinstallswmgrcreated) {

				g_cspushinstallswmgr.reset(new CsPushInstallSWMgr());
				g_cspushinstallswmgrcreated = true;
			}
		}

		return g_cspushinstallswmgr;
	}

	CsPushInstallSWMgr::CsPushInstallSWMgr()
		: PushInstallSWMgrBase ((PushInstallCommunicationBasePtr)CsCommunicationImpl::Instance(), (PushConfigPtr)CsPushConfig::Instance())
	{
		
	}

	void CsPushInstallSWMgr::verifySW(const std::string &osname, const std::string &sw)
	{
		if (SWAlreadyVerified(sw))
			return;

		// verify the software
		// we currently support only verifying windows software

		if (osname == "Win") {
			SignMgr verifier;
			if (!verifier.VerifyEmbeddedSignature((TCHAR*)sw.c_str())) {

				throw ERROR_EXCEPTION << "signature check for " << sw << "failed";
			}
		}
	}

	std::string CsPushInstallSWMgr::getSoftwareNameFromDownloadUrl(const std::string &csurl)
	{
		size_t idx = csurl.find_last_of("/");
		if (idx == std::string::npos) {
			throw ERROR_EXCEPTION << "invalid url " << csurl << " for downloading software";
		}

		return csurl.substr(idx + 1);
	}
}