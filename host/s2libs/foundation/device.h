/*
 * Copyright (c) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : device.cpp
 *
 * Description: 
 */

#ifndef DEVICE__H
#define DEVICE__H

#include <string>
#include "genericfile.h"


class Device: public GenericFile
{
public:
	Device(const Device&);
	Device(const std::string&);
	virtual ~Device();
	int Init();
    const std::string& GetAddress();
    const std::string& GetNumber();
protected:
	Device();
private:
    std::string m_sAddress;
    std::string m_sNumber;

};

#endif

