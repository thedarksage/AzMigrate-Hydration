#ifndef __UTILMINOR__H
#define __UTILMINOR__H
#include "appglobals.h"

#define SAMBA_CONF    "/etc/samba/smb.conf"
#define SAMBA_CONF_COPY    "/etc/samba/smb.conf_copy"
#define LABEL_CMD "e2label"
#define MAKE_FS_CMD "mkfs -t"
#define MOUNTNFS_CMD "mount -t nfs"
#define MOUNTCIFS_CMD "mount -t cifs"

SVERROR shareFile(const std::string& shareFileName, const std::string& sharePathName);
bool applyLabel(const std::string& deviceName, const std::string& label, const std::string& outPutFileName, std::string& statusMessage);
bool makeFileSystem(const std::string& deviceName, const std::string& fileSystemType, const std::string& outPutFileName, std::string& statusMessage);
bool mountNFS(const std::string& shareName, const std::string& mountPointName, const std::string& outPutFileName, std::string&  statusMessage);
bool mountCIFS(const std::string& shareName, const std::string& mountPointName, const std::string& username, const std::string& passwd, const std::string& domainname, const std::string& outPutFileName, std::string&  statusMessage);
bool isValidNfsPath(const std::string& nfsPath);
bool isValidCIFSPath(const std::string& vfsPath);

#endif
