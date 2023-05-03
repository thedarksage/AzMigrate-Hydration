#ifndef VOLUME_STREAM__H
#define VOLUME_STREAM__H

#ifdef SV_WINDOWS
#pragma once
#endif

#include <ace/OS.h>
#include <boost/shared_ptr.hpp>

#include "volumechangedata.h"
#include "filestream.h"
#include "genericdevicestream.h"


class VolumeStream :
	public  FileStream
	//public GenericDeviceStream
{
public:
	/// \brief Pointer type
	typedef boost::shared_ptr<VolumeStream> Ptr; ///< volume stream pointer type

	/// \brief VolumeStream constructor: supports stream creation for externally supplied handle
	///
	/// \note: 
	/// \li \c Does not support stream open because handle is external and already open
	/// \li \c Does not close handle on stream close
	VolumeStream(const ACE_HANDLE &h);      ///< externally supplied handle

	virtual ~VolumeStream();
	
	/// \brief Open: always throws not implemented, because handle is external and already open
	virtual int Open(const STREAM_MODE);

    VolumeStream& operator>>(VolumeChangeData&);
    VolumeStream& operator<<(VolumeChangeData&);
	
	virtual int Close();

    void Clear();

    std::string GetLastErrMsg() const
    {
        return m_lastErrMsg;
    }

    void SetLastErrMsg(const std::string lastErrMsg)
    {
        m_lastErrMsg = lastErrMsg;
    }

private:

    VolumeStream& GetMetaData(VolumeChangeData&);
    VolumeStream& GetData(VolumeChangeData&);
	VolumeStream& GetStreamData(VolumeChangeData& Data);
    long int m_uliTotalDataProcessed;

    bool m_bSeek;
    std::string m_lastErrMsg;

};

#endif

