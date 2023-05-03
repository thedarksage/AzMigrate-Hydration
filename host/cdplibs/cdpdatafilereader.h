//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : cdpdatafilereader.h
//
// Description: 
//

#ifndef CDPDATAFILEREADER__H
#define CDPDATAFILEREADER__H

#include "cdpglobals.h"
#include "file.h"
#include "portablehelpers.h"
#include <boost/shared_ptr.hpp>

#define TSO_FILE_SIZE 196

class CDPDataFileReader
{
public:

    typedef boost::shared_ptr<CDPDataFileReader> Ptr;
    CDPDataFileReader(const std::string & path);
    CDPDataFileReader(const std::string & path, char * source, SV_ULONG sourceLen);

    virtual ~CDPDataFileReader() {}

    virtual bool Init() = 0;
    virtual bool Parse() = 0;
    virtual void SetBaseline(OS_VAL baseline) = 0;

    //class DRTDReader
    //{
    //public:
    //    virtual SV_UINT read(char* buffer, SV_UINT length) = 0;
    //};

    //virtual bool FetchDRTDReader(const cdp_drtd_t &drtd, boost::shared_ptr<DRTDReader> &) =0;
    //virtual bool FetchDRTDReader(const cdp_drtdv2_t &drtd, boost::shared_ptr<DRTDReader> &) =0;
    virtual void SetStartOffset(SV_OFFSET_TYPE) = 0;
    virtual void SetEndOffset(SV_OFFSET_TYPE) =0;
    virtual SV_ULONGLONG LogicalSize() =0;

    SV_ULONGLONG StartTime();
    SV_ULONGLONG EndTime();
    SV_ULONGLONG StartSeq();
    SV_ULONGLONG EndSeq();
    SV_ULONGLONG PhysicalSize();
    SV_UINT NumEvents();

    inline SV_UINT      NumDiffs()    const   { return (SV_UINT)m_diffs.size(); }
    inline DiffIterator DiffsBegin()          { return m_diffs.begin(); }
    inline DiffIterator DiffsEnd()            { return m_diffs.end(); }
    inline const std::string& GetName() const { return m_path; }
    inline std::string GetBaseName() const	  { return m_basename;	}
    inline bool tsoFile() const               {	return m_btsoFile;	}

protected:

    std::string m_path;
    char * m_source;
    SV_ULONG m_sourceLen;

    std::string m_basename;
    bool m_btsoFile;
    Differentials_t  m_diffs;

private:

    CDPDataFileReader(CDPDataFileReader const & );
    CDPDataFileReader& operator=(CDPDataFileReader const & );
};


#endif

