/*++

Copyright (c) Microsoft Corporation

Module Name:

    DeepSpaceUm.h

Abstract:


Environment:

    User mode

Revision History:

--*/

#pragma once

////////////////////////////////////////////////////////////////////////////////
//
// Type definitions from spaceio.h
//

typedef enum _SIO_OPERATION : unsigned char {

    SioOperationUnknown = 0,

    SioOperationSpacePrepare,
    SioOperationSpaceFlush,
    SioOperationSpaceDsm,

    SioOperationTaskPrepare,

    SioOperationSpanIo,

    SioOperationRaidRead,
    SioOperationRaidWrite,
    SioOperationRaidRegenerate,
    SioOperationRaidScrub,
    SioOperationRaidRepair,
    SioOperationRaidDestageRead,
    SioOperationRaidDestageWrite,
    SioOperationRaidReplay,

    SioOperationRaid1ReadUnit,
    SioOperationRaid1PrepareWrite,
    SioOperationRaid1WriteCopies,

    SioOperationRaid5ReadUnit,
    SioOperationRaid5Reconstruct,
    SioOperationRaid5WriteUnit,
    SioOperationRaid5WriteMultiple,
    SioOperationRaid5WriteParity,

    SioOperationDrtRead,
    SioOperationDrtWrite,

    SioOperationLogRead,
    SioOperationLogWrite,

    SioOperationConfigIo

} SIO_OPERATION, *PSIO_OPERATION;


typedef enum _SIO_STATE : unsigned char {

    SioStateUnknown = 0,
    SioStateNormal,
    SioStateNoRecovery,
    SioStateRecover,
    SioStateRepair,
    SioStateWriteback,
    SioStateLog

} SIO_STATE, *PSIO_STATE;

#include "DeepSpaceCommon.h"
