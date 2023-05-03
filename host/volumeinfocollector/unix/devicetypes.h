//                                       
// Copyright (c) 2005 InMage.
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use,
// disclosure, or reproduction is prohibited except as permitted
// by express written license agreement with InMage.
// 
// File       : devicetypes.h
// 
// Description: 
// 

#ifndef DEVICETYPES_H
#define DEVICETYPES_H

int const DEVICE_TYPE_UNKNOWN(0);
int const DEVICE_TYPE_FULL(1);
int const DEVICE_TYPE_PARTITION(2);
int const DEVICE_TYPE_EXTENDED_PARTITION(3);
int const DEVICE_TYPE_PARTITION_LOGICAL_VOLUME(4);
int const DEVICE_TYPE_LOGICAL_VOLUME_GROUP_VOLUME(5);
int const DEVICE_TYPE_VSNAP(6);
int const DEVICE_TYPE_VOL_PACK(7);
int const DEVICE_TYPE_DEV_MAPPER_VOLUME(8);
int const DEVICE_TYPE_FULL_HDLM(9);
int const DEVICE_TYPE_PARTITION_HDLM(10);
int const DEVICE_TYPE_MULTI_PATH_HDLM(11);
int const DEVICE_TYPE_SOLARIS_VOLUME_MANAGER_VOLUME(12);
int const DEVICE_TYPE_VERITAS_VOLUME_MANAGER_VOLUME(13);
int const DEVICE_TYPE_CUSTOM(14);

#endif // ifndef DEVICETYPES_H
