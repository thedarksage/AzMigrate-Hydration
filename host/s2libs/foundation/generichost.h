/*
 * Copyright (c) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : generichost.h
 *
 * Description: 
 */
#ifndef GENERIC_HOST__H
#define GENERIC_HOST__H

#include <string>
#include <ostream>
#include <istream>

#include "operatingsystem.h"
#include "entity.h"

class GenericHost : public Entity  
{

public:
    GenericHost();
    virtual ~GenericHost();
    virtual const std::string GetHostName () const = 0;
    virtual const std::string GetIPAddress () const = 0;
    OperatingSystem* GetOperatingSystem ();

protected:

private:
      GenericHost(const GenericHost &right);
      GenericHost & operator=(const GenericHost &right);

        int operator==(const GenericHost &right) const;
      int operator!=(const GenericHost &right) const;
      int operator<(const GenericHost &right) const;
      int operator>(const GenericHost &right) const;
      int operator<=(const GenericHost &right) const;
      int operator>=(const GenericHost &right) const;

      void operator->() const;
      void operator*() const;

      friend std::ostream & operator<<(std::ostream &stream,const GenericHost &right);
      friend std::istream & operator>>(std::istream &stream,GenericHost &object);

  private: 
      OperatingSystem* m_pOperatingSystem;
};
#endif
