///
/// \file RcmPushInstallSWMgr.cpp
///
/// \brief
///

#include "RcmPushInstallSWMgr.h"
#include "errorexception.h"
#include "RcmCommunicationImpl.h"
#include "SignMgr.h"

namespace PI {

	RcmPushInstallSWMgrPtr g_rcmpushinstallswmgr;
	bool g_rcmpushinstallswmgrcreated = false;
	boost::mutex g_rcmpushinstallswmgrMutex;

	RcmPushInstallSWMgrPtr RcmPushInstallSWMgr::Instance()
	{
		if (!g_rcmpushinstallswmgrcreated) {
			boost::mutex::scoped_lock guard(g_rcmpushinstallswmgrMutex);
			if (!g_rcmpushinstallswmgrcreated) {

				g_rcmpushinstallswmgr.reset(new RcmPushInstallSWMgr());
				g_rcmpushinstallswmgrcreated = true;
			}
		}

		return g_rcmpushinstallswmgr;
	}

	RcmPushInstallSWMgr::RcmPushInstallSWMgr()
		: PushInstallSWMgrBase((PushInstallCommunicationBasePtr)(RcmCommunicationImpl::Instance()), (PushConfigPtr)(RcmPushConfig::Instance()))
	{

	}

	void RcmPushInstallSWMgr::verifySW(const std::string &osname, const std::string &sw)
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

	std::string RcmPushInstallSWMgr::getSoftwareNameFromDownloadUrl(const std::string &csurl)
	{
		// TODO (rovemula) - Implement this when DLC link format is available.

		return "";
	}
}