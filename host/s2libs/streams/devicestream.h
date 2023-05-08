//#pragma once

#ifndef DEVICE_STREAM__H
#define DEVICE_STREAM__H



#include "genericdevicestream.h"
#include "filestream.h"
#include "device.h"

class DeviceStream: 
        public FileStream,
	    public GenericDeviceStream
{
public:
	/// \brief Pointer type
	typedef boost::shared_ptr<DeviceStream> Ptr;

	virtual ~DeviceStream();

	virtual int IoControl(unsigned long int,void*,long int,void*,long int);
    virtual int IoControl(unsigned long int,void*,long int);
	virtual int IoControl(unsigned long int,void*);
	virtual int IoControl(unsigned long int);
	virtual int Open(const STREAM_MODE);
	virtual int Close();
    int CancelIO();
    int CancelIOEx();
private:
	DeviceStream();

private:
	bool	m_bSync;
#ifdef SV_WINDOWS
	OVERLAPPED m_IoEvent;
#endif
    int m_iErrorCode; 

private:
	DeviceStream& operator=(const DeviceStream&);
	DeviceStream(const DeviceStream&);

public:
	DeviceStream(const Device& device);
    DeviceStream(const std::string&);
    int GetErrorCode(void);
    void SetErrorCode(int iErrorCode);
    void ResetErrorCode(void);

	/// \brief returns device name
	std::string GetName(void);

    /// \brief waits for the event to be signalled when device
    /// is opened in async mode
    int WaitForEvent(unsigned int milliseconds);
};

#endif

