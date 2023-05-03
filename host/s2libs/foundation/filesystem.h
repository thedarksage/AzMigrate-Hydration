/*
 * Copyright (c) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : filesystem.h
 *
 * Description: 
 */
//#pragma once

#ifndef FILE_SYSTEM__H
#define FILE_SYSTEM__H

#include <vector>
#include <iterator>
#include "entity.h"
#include "svtypes.h"

class FileSystem :
	public Entity
{
public:
    typedef struct ClusterRange
    {
        SV_ULONGLONG m_Start;
        SV_ULONGLONG m_Count;

        ClusterRange() :
         m_Start(0),
         m_Count(0)
        {
        }

        ClusterRange(const SV_ULONGLONG &start, const SV_ULONGLONG &count) :
         m_Start(start),
         m_Count(count)
        {
        }

        ClusterRange(const ClusterRange &rhs) : 
         m_Start(rhs.m_Start),
         m_Count(rhs.m_Count)
        {
        }

        void Print(void) const;

    } ClusterRange_t;
    typedef std::vector<ClusterRange_t> ClusterRanges_t;
    typedef ClusterRanges_t::const_iterator ConstClusterRangesIter_t;
    typedef ClusterRanges_t::iterator ClusterRangesIter_t;
    
	FileSystem();
	virtual ~FileSystem();
};

#endif
