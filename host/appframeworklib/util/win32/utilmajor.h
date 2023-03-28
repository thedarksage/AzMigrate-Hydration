#ifndef __UTILMINOR__H
#define __UTILMINOR__H
#include "appglobals.h"

#define SAMBA_CONF    "/etc/samba/smb.conf"

SVERROR shareFile(const std::string& shareFileName, const std::string& sharePathName);

#endif