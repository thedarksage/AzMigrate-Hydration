//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : inmemorydifffile.h
//
// Description: 
//

#ifndef INMEMORYDIFFFILE__H
#define INMEMORYDIFFFILE__H

#include "cdpglobals.h"
#include "portablehelpers.h"

#include "svdconvertor.h"
class InMemoryDiffFile
{
public:
    InMemoryDiffFile(const std::string & path, const char * source, SV_ULONG sourceLen);
    virtual ~InMemoryDiffFile();

    inline const std::string& GetName() const { return m_path ; }
    bool  ParseDiff(DiffPtr & diffptr);

    inline void SetBaseline(OS_VAL baseline) { m_baseline = baseline; }
    inline void SetStartOffset(SV_OFFSET_TYPE start) { m_start = start; }
    inline void SetEndOffset(SV_OFFSET_TYPE end)     { m_end = end ; }
    inline SV_OFFSET_TYPE LogicalStart()             { return m_start; }
    inline SV_OFFSET_TYPE LogicalEnd()               { return m_end; }
    inline SV_ULONGLONG LogicalSize()                { return ( m_end - m_start ); }

    void ANNOUNCE_FILE();
    void ANNOUNCE_TAG(const std::string & tagname);

    // constants for seeking
    typedef int SeekFrom;  
    static const SeekFrom FromBeg = (SeekFrom)1;
    static const SeekFrom FromCur = (SeekFrom)2;

    // returns number of bytes read/written on success
    SV_UINT Read(char* buffer, SV_UINT length);    

    // convenience function
    // we can eliminate either Read or FullRead. either is sufficient
    SV_ULONG FullRead(char* buffer, SV_ULONG length); 

    // returns the new offset after performing seek
    // negative seeks are not supported
    SV_LONGLONG Seek(SV_LONGLONG offset, SeekFrom from);

    // returns the current offset         
    SV_LONGLONG Tell() const { return m_curpos ;}
	void set_max_io_size(SV_UINT& );
	inline void SetSourceVolumeSize(const SV_ULONGLONG & volumesize) { m_volsize = volumesize;}
private:

    std::string m_path;
    const char * m_source;
    SV_ULONG m_sourceLen;

    SV_OFFSET_TYPE m_start;
    SV_OFFSET_TYPE m_end;
    OS_VAL m_baseline;
	SV_UINT m_max_io_size;

    // current offset;
    SV_OFFSET_TYPE m_curpos;

    bool m_v2;
    SVDConvertorPtr m_svdConvertor ;

    SV_ULONGLONG m_volsize;

    bool ReadSVD1(const SVD_PREFIX & prefix, DiffPtr &diffptr);
    bool ReadSVD2(const SVD_PREFIX & prefix, DiffPtr &diffptr);
    bool ReadTSFC(const SVD_PREFIX & prefix, DiffPtr &diffptr);
    bool ReadTFV2(const SVD_PREFIX & prefix, DiffPtr & diffptr);
    bool ReadUSER(const SVD_PREFIX & prefix, DiffPtr &diffptr, SV_UINT& dataFormatFlags);
    bool ReadLODC(const SVD_PREFIX & prefix, DiffPtr &diffptr);
    bool ReadDRTD(const SVD_PREFIX & prefix, DiffPtr &diffptr);
    bool ReadDRTDV2(const SVD_PREFIX & prefix, DiffPtr & diffptr);
    bool ReadDRTDV3(const SVD_PREFIX & prefix, DiffPtr & diffptr);
    bool ReadTSLC(const SVD_PREFIX & prefix, DiffPtr &diffptr);
    bool ReadTLV2(const SVD_PREFIX & prefix, DiffPtr & diffptr);

    bool ReadDATA(const SVD_PREFIX & prefix, DiffPtr & diffptr);
    bool SkipBytes(const SVD_PREFIX & prefix, DiffPtr & diffptr);
    bool VerifyDataBoundaries(DiffPtr & diffptr);
    bool VerifyIoOrdering(DiffPtr & diffptr);
    bool ParseandVerifyFileName(DiffPtr & diffptr);

    InMemoryDiffFile(InMemoryDiffFile const & );
    InMemoryDiffFile& operator=(InMemoryDiffFile const & );
};

#endif
