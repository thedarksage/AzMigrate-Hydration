//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File:       svdrtd.cpp
//
// Description: 
//

#include "svdrtd.h"

bool SV_DRTD_V3::CompareEndOffset(SvDrtdV3Ptr const & y) const
{
	SV_ULONGLONG x_startoffset = this -> VolOffset;
	SV_ULONGLONG y_startoffset = y -> VolOffset;
	SV_ULONGLONG x_endoffset = (this -> VolOffset + this -> Length -1);
	SV_ULONGLONG y_endoffset = (y -> VolOffset + y -> Length -1);

	return (x_endoffset <  y_endoffset);
}

bool SV_DRTD_V3::CompareStartOffset(SvDrtdV3Ptr const & y) const
{
	SV_ULONGLONG x_startoffset = this -> VolOffset;
	SV_ULONGLONG y_startoffset = y -> VolOffset;
	SV_ULONGLONG x_endoffset = (this -> VolOffset + this -> Length -1);
	SV_ULONGLONG y_endoffset = (y -> VolOffset + y -> Length -1);

	return (x_startoffset <  y_startoffset);
}

bool SV_DRTD_V3::CompareFileId(SvDrtdV3Ptr const & y) const
{
	return ( this -> FileId < y -> FileId );
}