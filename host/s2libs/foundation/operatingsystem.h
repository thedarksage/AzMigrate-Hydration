/*
 * Copyright (c) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : operatingsystem.h
 *
 * Description: 
 */

#ifndef OPERATING_SYSTEM__H
#define OPERATING_SYSTEM__H

#include "entity.h"
#include "portablehelpers.h"
#include "volumegroupsettings.h"

class Lockable;

class OperatingSystem: public Entity
{
 
public:
    static OperatingSystem& GetInstance();
    static bool Destroy();

	const Object & GetOsInfo() const;
    const ENDIANNESS & GetEndianness(void) const;
    const OS_VAL & GetOsVal(void) const;

protected:

private:
    OperatingSystem();
    virtual ~OperatingSystem();
    static void Reset();

	static Object m_OsInfo;
	static ENDIANNESS m_Endianness;
	static OS_VAL m_OsVal;
    static  Lockable m_CreateLock;
    static OperatingSystem* theOS;

private:
      OperatingSystem(const OperatingSystem &right);
      OperatingSystem & operator=(const OperatingSystem &right);
	  
      int Init();
	  int SetOsInfo(std::string &errmsg);
	  int SetOsSpecificInfo(std::string &errmsg);
	  int SetOsCommonInfo(std::string &errmsg);
	  /// As of now, this SystemType is required for windows only.
	  /// Need to implement only if the particular host requires.
	  int SetSystemType(std::string &errmsg);
	  void SetEndianness(void);

	  /* TODO: below two should be merged together */
	  int SetOsReadableName(std::string &errmsg);
	  int SetOsVersionBuild(std::string &errmsg);

      int operator==(const OperatingSystem &right) const;
      int operator!=(const OperatingSystem &right) const;
      int operator<(const OperatingSystem &right) const;
      int operator>(const OperatingSystem &right) const;
      int operator<=(const OperatingSystem &right) const;
      int operator>=(const OperatingSystem &right) const;
  private: 
};

#endif
