#ifndef __USERMGMT__H
#define __USERMGMT__H

#include <string>
#include <list>

bool isUserExists(const std::string& userName) ;

bool isGroupExists(const std::string& groupName) ;

bool isUserAcctLocked(const std::string& userName) ;

std::list<std::string> getUserGroups(const std::string& userName) ;

std::list<std::string> getGroupMembers(const std::string& groupName) ;

#endif