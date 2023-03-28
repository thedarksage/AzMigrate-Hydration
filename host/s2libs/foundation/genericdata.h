/*
 * Copyright (c) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : genericdata.h
 *
 * Description: 
 */

#ifndef GENERIC_DATA__H
#define GENERIC_DATA__H

#include "entity.h"

class GenericData :
	public Entity
{
public:
	GenericData(void);
	virtual ~GenericData(void);
	bool IsEmpty() const;
	
protected:
	int Init();
	bool m_bEmpty;

public: 
	void* GetData(void) const;
};

#endif
