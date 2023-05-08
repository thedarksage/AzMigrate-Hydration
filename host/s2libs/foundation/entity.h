/*
 * Copyright (c) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : entity.h
 *
 * Description: 
 */

#ifndef ENTITY__H
#define ENTITY__H

struct Entity
{
public:
	int Init(void);
	inline bool IsInitialized(void) 
	{		
		return m_bInitialized; 
	}
	inline Entity():m_bInitialized(false) {}
	inline virtual ~Entity() {}
protected:
	bool m_bInitialized;
};

#endif //_ENTITY_
