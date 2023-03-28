//#pragma once

#ifndef GENERIC_STREAM__H
#define GENERIC_STREAM__H


#include <cstdio>
#include "entity.h"

enum STREAM
{
	FILE_STRM=0,
	VOLUME_STRM=1,
	DEVICE_STRM=2,
	COMPRESSION_STRM=3,
	HTTP_STRM=4,
    FTP_STRM=5,
    LOCAL_STRM=6
};


typedef unsigned long int STREAM_MODE;

struct GenericStream
{
public:
    GenericStream();
    ~GenericStream();
	virtual int Open(const STREAM_MODE) = 0;
	virtual int Close() = 0;
    virtual int Abort(const void* pData= NULL);
    virtual int Delete(const void* pData = NULL);

    const bool Good() const;
    const bool Bad() const;
    const bool Eof() const;
    const bool Fail() const;
    const bool MoreData() const;
    void Clear();
    const bool IsOpen() const;
    //STREAM_STATE State() const;
public:
    // must specify at least one
    static STREAM_MODE const Mode_Read            = (STREAM_MODE)0x0001;                   // read only  
    static STREAM_MODE const Mode_Write           = (STREAM_MODE)0x0002;                   // write only
    static STREAM_MODE const Mode_RW              = (STREAM_MODE)(Mode_Read | Mode_Write);     // read write (convienence)
    static STREAM_MODE const Mode_Binary          = (STREAM_MODE)0x0004;                   // if not set opens as text
    static STREAM_MODE const Mode_Append          = (STREAM_MODE)0x0008;                   // all writes done at end of file

    // open/creation flags must provide one of the following        
    // note: BioOverwrite and BioTruncate can not be used with any other open/creation flags
    static STREAM_MODE const Mode_Open            = (STREAM_MODE)0x0010;                   // open if exists else fail
    static STREAM_MODE const Mode_Create          = (STREAM_MODE)0x0020;                   // create if it doesn't exist else fail
    static STREAM_MODE const Mode_OpenCreate      = (STREAM_MODE)(Mode_Open | Mode_Create);    // open if exists else create        
    static STREAM_MODE const Mode_Overwrite       = (STREAM_MODE)0x0040;                   // create if not exists, else overwrite existing
    static STREAM_MODE const Mode_Truncate        = (STREAM_MODE)0x0080;                   // open existing and truncate it

    // sharing not sure if these names will be useful on other OSes. the semantics might be different to.)
    // on windows it means you will allow subsequent opens this share access
    static STREAM_MODE const Mode_ShareDelete     = (STREAM_MODE)0x0100;
    static STREAM_MODE const Mode_ShareRead       = (STREAM_MODE)0x0200;
    static STREAM_MODE const Mode_ShareWrite      = (STREAM_MODE)0x0400;               
    static STREAM_MODE const Mode_ShareRW         = (STREAM_MODE)(Mode_ShareRead   | Mode_ShareWrite);
    static STREAM_MODE const Mode_ShareAll        = (STREAM_MODE)(Mode_ShareDelete | Mode_ShareRW);

    // special flags for windows (may be useful on other OSes)
    static STREAM_MODE const Mode_NoBuffer        = (STREAM_MODE)0x1000;
    static STREAM_MODE const Mode_WriteThrough    = (STREAM_MODE)0x2000;

    static STREAM_MODE const  Mode_Synchronous       = (STREAM_MODE)0x4000;
    static STREAM_MODE const  Mode_Asynchronous      = (STREAM_MODE)0x8000;

    // security
    static STREAM_MODE const Mode_Default           = (STREAM_MODE)0x10000;
    static STREAM_MODE const Mode_Secure            = (STREAM_MODE)0x20000;

	//For linux's O_DIRECT flag
	static STREAM_MODE const Mode_Direct			= (STREAM_MODE)0x40000;

    // some (possibly) useful default combinations 
    static STREAM_MODE const Mode_ReadExisting    = (STREAM_MODE)(Mode_Open  | Mode_Read);
    static STREAM_MODE const Mode_WriteNew        = (STREAM_MODE)(Mode_Write | Mode_Create);
    static STREAM_MODE const Mode_WriteAlways     = (STREAM_MODE)(Mode_Write | Mode_OpenCreate);
    static STREAM_MODE const Mode_RWExisitng      = (STREAM_MODE)(Mode_RW    | Mode_Open);
    static STREAM_MODE const Mode_RWNew           = (STREAM_MODE)(Mode_RW    | Mode_Create);
    static STREAM_MODE const Mode_RWAlways        = (STREAM_MODE)(Mode_RW    | Mode_OpenCreate);
protected:
    bool ModeOn(STREAM_MODE chkMode, STREAM_MODE mode); 
    STREAM_MODE m_Mode;
    bool m_bOpen;    
protected:
    enum STREAM_STATE
    {
        STRM_BAD         = 0,
        STRM_GOOD        = 1,
        STRM_EOF         = 2,
        STRM_FAIL        = 3,
        STRM_CLEAR       = 4,
        STRM_MORE_DATA   = 5
    };
    STREAM_STATE m_State;

    void SetState(STREAM_STATE);
};

#endif
