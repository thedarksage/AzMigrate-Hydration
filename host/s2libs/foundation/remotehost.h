/*
 * Copyright (c) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : remotehost.h
 *
 * Description: 
 */

#ifndef REMOTE_HOST__H
#define REMOTE_HOST__H

#include "generichost.h"

class RemoteHost : public GenericHost  
{
  public:
      RemoteHost();
      virtual ~RemoteHost();
  protected:
  private:
      RemoteHost(const RemoteHost &right);
      RemoteHost & operator=(const RemoteHost &right);
      int operator==(const RemoteHost &right) const;
      int operator!=(const RemoteHost &right) const;
      int operator<(const RemoteHost &right) const;
      int operator>(const RemoteHost &right) const;
      int operator<=(const RemoteHost &right) const;
      int operator>=(const RemoteHost &right) const;
};

#endif

