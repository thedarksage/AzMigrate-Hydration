///
///  \file  IBlockDevice.h
///
///  \brief contains interface of  BlockDevice



#ifndef IBLOCKDEVICE_H
#define IBLOCKDEVICE_H

#include "common.h"
#include "DiskDeviceMajor.h"
#include "PlatformAPIs.h"


#define SafeFree(x)		if (NULL != x) {free(x); x = NULL;}

class BLOCKDEVICE_API IBlockDevice
{
public:
    virtual bool			Online(bool readOnly) = 0;
    virtual bool			Offline(bool readonly) = 0;
    virtual SV_ULONG		GetCopyBlockSize() = 0;
    virtual SV_ULONGLONG	GetDeviceSize() = 0;
        
    virtual bool    Read(void* buffer, SV_ULONG ulBytesToRead, SV_ULONG& ulBytesRead) = 0;

    virtual bool    Write(const void* buffer, SV_ULONG ulBytesToWrite, SV_ULONG& ulBytesWritten) = 0;

    virtual bool DeviceIoControlSync(
        _In_		SV_ULONG       ulIoControlCode,
        _In_opt_	void*				lpInBuffer,
        _In_		SV_ULONG       nInBufferSize,
        _Out_opt_	void*				lpOutBuffer,
        _In_		SV_ULONG       nOutBufferSize,
        _Out_opt_	SV_ULONG*	    lpBytesReturned
        ) = 0;

    virtual     SV_ULONG       GetDeviceNumber() = 0;
    virtual 	std::string			GetDeviceName() = 0;
    virtual 	std::string			GetDeviceId() = 0;
    virtual     SV_ULONG   GetMaxRwSizeInBytes() = 0;
    virtual bool    SeekFile(FileMoveMethod ulMoveMethod, SV_ULONGLONG offset) = 0;
};
#endif /* IBLOCKDEVICE_H */
