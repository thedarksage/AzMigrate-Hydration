///
/// \file RcmPushInstallSWMgr.h
///
/// \brief
///


#ifndef INMAGE_PI_RCMPISWMGR_H
#define INMAGE_PI_RCMPISWMGR_H

#include <boost/thread/mutex.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/lexical_cast.hpp>

#include "PushInstallSWMgrBase.h"

namespace PI {

	class RcmPushInstallSWMgr;
	typedef boost::shared_ptr<RcmPushInstallSWMgr> RcmPushInstallSWMgrPtr;

	class RcmPushInstallSWMgr : public PushInstallSWMgrBase
	{

	public:

		~RcmPushInstallSWMgr() 
		{

		}

		virtual void verifySW(const std::string &osname, const std::string &sw);
		virtual std::string getSoftwareNameFromDownloadUrl(const std::string &csurl);

		static RcmPushInstallSWMgrPtr Instance();

	protected:

	private:

		RcmPushInstallSWMgr();

	};
}

#endif