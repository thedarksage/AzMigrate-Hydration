/*
* Copyright (c) 2005 InMage
* This file contains proprietary and confidential information and
* remains the unpublished property of InMage. Use, disclosure,
* or reproduction is prohibited except as permitted by express
* written license aggreement with InMage.
*
* File       : genericfile.h
*
* Description: 
*/

#ifndef GENERIC_FILE__H
#define GENERIC_FILE__H

#include <string>
#include "basicio.h"
#include "entity.h"

class GenericFile: public Entity
{
public:
    virtual ~GenericFile();
    GenericFile(const GenericFile&);
    GenericFile(const std::string&);
    GenericFile(const char*);

    int Init();
    const std::string& GetName() const;
    void SetName(const std::string&);
    void SetName(const char*);

    // BASIC I/O uses BasicIo class to implement
    virtual SV_INT Open(BasicIo::BioOpenMode mode)                         { return m_Bio.Open(m_sName.c_str(), mode); }

	/// \brief returs the open mode
	virtual BasicIo::BioOpenMode OpenMode(void)                              { return m_Bio.OpenMode(); }
    
	virtual SV_UINT Read(char* buffer, SV_UINT length)                       { return m_Bio.Read(buffer, length); }
    virtual SV_UINT FullRead(char* buffer, SV_UINT length)                   { return m_Bio.FullRead(buffer, length); }
    virtual SV_UINT Write(char const * buffer, SV_UINT length)               { return m_Bio.Write(buffer, length); }
    virtual SV_LONGLONG Seek(SV_LONGLONG offset, BasicIo::BioSeekFrom from)  { return m_Bio.Seek(offset, from); }
    virtual SV_LONGLONG Tell() const                                         { return m_Bio.Tell(); }
    virtual SV_INT Close()                                                   { return m_Bio.Close(); }    
    virtual SV_ULONG LastError() const                                       { return m_Bio.LastError(); }
    virtual bool Good() const                                                { return m_Bio.Good(); }
    virtual bool Eof() const                                                 { return m_Bio.Eof(); }
    virtual void ClearError()                                                { return m_Bio.ClearError(); }
    virtual bool isOpen()                                                    { return m_Bio.isOpen(); }
    virtual void SetLastError(SV_ULONG liError)                              { m_Bio.SetLastError(liError); }
    ACE_HANDLE& GetHandle()                                                  { return m_Bio.Handle(); }
    SV_ULONG SetHandle(const ACE_HANDLE& handle)                             { return m_Bio.SetHandle(handle); }

    void EnableRetry(WaitFunction fWait, int iRetries, int iRetryDelay)      { return m_Bio.EnableRetry(fWait, iRetries, iRetryDelay); }
    void DisableRetry()                                                     { return m_Bio.DisableRetry(); }


protected:
    GenericFile();    

    BasicIo m_Bio; 
private:
    std::string  m_sName;


};

#endif
