//#pragma once

#ifndef GENERIC_DEVICE_STREAM__H
#define GENERIC_DEVICE_STREAM__H





//namespace inmage {


struct GenericDeviceStream
{
public:
#ifdef SV_WINDOWS
	virtual int IoControl(unsigned long int,void*,long int,void*,long int) = 0;
#elif SV_UNIX
    virtual int IoControl(unsigned long int,void* pBuffer,long int liVal) = 0;
#endif
};



//}

#endif
