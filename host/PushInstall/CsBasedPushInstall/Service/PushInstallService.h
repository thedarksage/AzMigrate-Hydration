#ifndef PUSH_INSTALL_SERVICE_H
#define PUSH_INSTALL_SERVICE_H

#include "PushProxy.h"


namespace PI {

	class PushInstallService
	{
	public:
		void runPushInstaller();
		void stopPushInstaller();
		void deletePushInstaller();

	protected:

	private:
		PushProxyPtr m_pushProxyThread;
	};
}

#endif