#ifndef INMZPOOL__H_
#define INMZPOOL__H_

#include <string>
#include <sstream>

#define LINVSNAP_SYNC_DRV "/dev/vblkcontrol"
#define CHECK_FREQ_FOR_SYNCDEV 2

bool ExecuteInmCommand(const std::string& fsmount, std::string &outputmsg, std::string& errormsg, int & ecode);
bool OpenLinvsnapSyncDev(int &fd);
bool CloseLinvsnapSyncDev(int &fd);

#endif /* INMZPOOL__H_ */
