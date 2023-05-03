#ifndef CONFINGUPDATE__H
#define CONFINGUPDATE__H

#include <string>


void GenerateUUID(std::string &hostId);
bool SetHostIdInAmetystConf(const std::string& hostid, std::stringstream& errmsg);
bool SetHostIdInPushConf(const std::string& hostid, std::stringstream& errmsg);
bool CreateAndSaveHostId(std::string& hostid, std::stringstream& errmsg);
bool ReadHostIdFromAmethystconf(std::string& hostid, std::stringstream& errmsg);
bool ReadHostIdFromPushConf(std::string& hostid, std::stringstream& errmsg);
bool ReadAndUpdateHostId(std::stringstream& errmsg);


#endif