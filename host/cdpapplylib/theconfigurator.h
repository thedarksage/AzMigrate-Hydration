//
// Copyright (c) 2006 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : theconfigurator.h
//
// Description: 
//

#ifndef THECONFIGURATOR__H
#define THECONFIGURATOR__H

#ifdef DP_SYNC_RCM
#include "RcmConfigurator.h"
#endif

#include "configurator2.h"

extern Configurator* TheConfigurator;    // singleton


#endif // ifndef THECONFIGURATOR__H
