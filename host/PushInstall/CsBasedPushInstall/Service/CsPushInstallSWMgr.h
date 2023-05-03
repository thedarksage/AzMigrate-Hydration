///
/// \file CsPushInstallSWMgr.h
///
/// \brief
///


#ifndef INMAGE_PI_CSPISWMGR_H
#define INMAGE_PI_CSPISWMGR_H

#include "PushInstallSWMgrBase.h"

namespace PI {

	class CsPushInstallSWMgr;
	typedef boost::shared_ptr<CsPushInstallSWMgr> CsPushInstallSWMgrPtr;

	class CsPushInstallSWMgr : public PushInstallSWMgrBase
	{

	public:

		~CsPushInstallSWMgr() {}

		virtual std::string getSoftwareNameFromDownloadUrl(const std::string &csurl);
		virtual void verifySW(const std::string &osname, const std::string &sw);
		
		static CsPushInstallSWMgrPtr Instance();

	protected:

	private:

		CsPushInstallSWMgr();

	};
}

#endif