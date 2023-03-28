///
///  \file  devicefiltermajor.h
///
///  \brief header that includes windows specific device filter headers
///

#ifndef DEVICE_FILTER_MAJOR_H
#define DEVICE_FILTER_MAJOR_H

#include "InmFltInterface.h"

/// \brief user space code uses INMAGE_FILTER_DEVICE_NAME for volume filter driver for both unix and windows platforms
#define INMAGE_FILTER_DEVICE_NAME	    INMAGE_FILTER_DOS_DEVICE_NAME

#endif /* DEVICE_FILTER_MAJOR_H */
