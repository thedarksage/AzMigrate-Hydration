///
/// \file PushInstallSWMgrBase.h
///
/// \brief
///


#ifndef INMAGE_PI_SWMGR_H
#define INMAGE_PI_SWMGR_H

#include <string>
#include <vector>
#include <map>

#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/shared_ptr.hpp>

#include "pushconfig.h"
#include "PushInstallCommunicationBase.h"

namespace PI {

	class PushInstallSWMgrBase;
	typedef boost::shared_ptr<PushInstallSWMgrBase> PushInstallSWMgrBasePtr;

	class PushInstallSWMgrBase {

	public:
		PushInstallSWMgrBase(PushInstallCommunicationBasePtr & proxy, PushConfigPtr & config);
		virtual ~PushInstallSWMgrBase() 
		{

		}

		std::string downloadUA(const std::string & osname, const std::string & csurl, bool verifysignature);
		std::string downloadPushClient(const std::string & osname, const std::string & csurl, bool verifysignature);
		std::string downloadPatch(const std::string & osname, const std::string & csurl, bool verifysignature);

		void verifyUA(const std::string & osname, const std::string & csurl);
		void verifyPushClient(const std::string & osname, const std::string & csurl);
		void verifyPatch(const std::string & osname, const std::string & csurl);

		std::string defaultUAPath(const std::string & osname);
		std::string defaultPushClientPath(const std::string &osname);

	protected:
		virtual void verifySW(const std::string &osname, const std::string &sw) = 0;
		virtual std::string getSoftwareNameFromDownloadUrl(const std::string &url) = 0;

		bool SWAlreadyVerified(const std::string & sw);

	private:

		std::string uaFolder;
		std::string pushclientFolder;
		std::string patchFolder;

		std::vector<std::string> localRepositorySoftwares;
		std::vector<std::string> downloadsinProgress;
		std::vector<std::string> verifiedSofwares;

		boost::condition_variable m_monitordownloadCv; ///< used to notify when a download is done
		boost::mutex m_monitordownloadMutex; ///< mutex used in conjunction with condition variable

		boost::mutex m_swVerificationMutex;

		PushConfigPtr m_config;
		PushInstallCommunicationBasePtr m_proxy;

		void SetFolderPermissions(const std::string & folderPath, int flags);
		void InitializeLocalRepositorySoftwares();
		std::string localUAPath(const std::string &osname, const std::string & csurl);
		std::string localPushClientPath(const std::string &osname, const std::string & csurl);
		std::string localPatchPath(const std::string & csurl);

		void downloadSW(const std::string & url, const std::string & localPath);
		bool swAvailable(const std::string & swPath);
		bool downloadInProgress(const std::string & csurl);
		void CheckAndDownloadSW(const std::string & csurl, const std::string & localPath);
		void RemoveFromAvailableSwList(const std::string &swPath);
		void getCommonDefaultUAandPushClientPath(const std::string & osname, std::string & pushClientPath, std::string & uaPath);
	};
}

#endif