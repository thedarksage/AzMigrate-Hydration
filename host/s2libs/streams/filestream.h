//#pragma once

#ifndef FILE_STREAM__H
#define FILE_STREAM__H

//namespace inmage {

#include <string>

#include "inputoutputstream.h"
#include "genericstream.h"
#include "file.h"
#include "event.h"


enum FILEPOS
{
	POS_BEGIN	=0,
	POS_CURRENT=1,
	POS_END	=2
};

enum E_PROFILESTREAM
{
    E_PROFILEREAD = 0x1,
    E_PROFILEWRITE = 0x2
};

/// \brief Wrapper class over ACE_HANDLE to have it correctly initialized to ACE_INVALID_HANDLE
class InitializedAceHandle
{
public:
	/// \brief constructor
	InitializedAceHandle() :
		m_Handle(ACE_INVALID_HANDLE)
	{
	}

	/// \brief constructor
	InitializedAceHandle(const ACE_HANDLE &h) ///< handle to hold
		: m_Handle(h)
	{
	}

	/// \brief gets the handle
	ACE_HANDLE Get(void) { return m_Handle; }

	/// \brief sets the handle
	void Set(const ACE_HANDLE &h) { m_Handle = h; }

private:
	ACE_HANDLE m_Handle; ///< handle
};

class FileStream: 
		 public GenericStream,public InputOutputStream
{
public:
	FileStream();
	FileStream(const File&);
	FileStream(const char*);
	FileStream(const std::string&);

	/// \brief constructor: Creates file stream with externally provided already opened handle. 
	/// 
	/// \note 
	/// Call to stream open is not required because already open external handle is provided
	FileStream(const ACE_HANDLE &externalHandle); ///< external handle

	virtual ~FileStream();

	//int Init() ;

	virtual int Open(const STREAM_MODE);
	virtual int Close();
	virtual int Write(const void*,const unsigned long int);
	virtual int Read(void*,const unsigned long int);
    virtual unsigned long int FullRead(void* sOutBuffer,unsigned long int uliBytes);
	virtual SV_LONGLONG Seek(SV_LONGLONG,FILEPOS);
    const SV_LONGLONG Tell();
    void Clear();
    void ProfileRead(void);
    ACE_Time_Value GetTimeForRead(void);

protected:
	GenericFile* m_pGenericFile;
    unsigned int m_ProfileStream;
    ACE_Time_Value m_TimeForRead;
	InitializedAceHandle m_EffectiveHandle; ///< Handle on which all stream operations are done. This is either external handle or handle from m_pGenericFile

private:
	FileStream(const FileStream&);
	FileStream& operator=(const FileStream&);

public:
	void EnableRetry(Event*, int iRetries = 2, int iRetryDelay = 2);
	void DisableRetry();
private:
	int mapToFilePos(FILEPOS position);
	int m_iVolumeRetries, m_iVolumeRetryDelay;
	Event* m_WaitEvent;
	bool m_bRetry;
	bool m_bIsHandleExternal; ///< true if handle is supplied externally to create stream
};

//}

#endif

