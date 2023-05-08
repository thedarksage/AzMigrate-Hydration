#include <libssh2.h>

#include "remoteapi.h"
#include "remoteapimajor.h"

namespace remoteApiLib {

	void global_init()
	{
		libssh2_init(0);
		global_init_major();

	}

	void global_exit()
	{
		global_exit_major();
		libssh2_exit();
	}
}