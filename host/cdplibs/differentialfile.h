//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : differentialfile.h
//
// Description: 
//

#ifndef DIFFERENTIALFILE__H
#define DIFFERENTIALFILE__H

#include "cdpglobals.h"
#include "file.h"
#include "portablehelpers.h"
#include "svdconvertor.h"

class DifferentialFile : public File
{
public:
    DifferentialFile(const std::string & path);
    virtual ~DifferentialFile();

    SV_INT Open(BasicIo::BioOpenMode mode);
    bool     ParseDiff(DiffPtr & diffptr);

    inline void SetBaseline(OS_VAL baseline) { m_baseline = baseline; }
    inline void SetStartOffset(SV_OFFSET_TYPE start) { m_start = start; }
    inline void SetEndOffset(SV_OFFSET_TYPE end)     { m_end = end ; }
    inline SV_OFFSET_TYPE LogicalStart()            { return m_start; }
    inline SV_OFFSET_TYPE LogicalEnd()              { return m_end; }
    inline SV_ULONGLONG LogicalSize()             { return ( m_end - m_start ); }
    inline void SetValidation(bool validate)     { m_validate = validate ; }
	inline void SetSourceVolumeSize(const SV_ULONGLONG & volumesize) { m_volsize = volumesize;}

    void ANNOUNCE_FILE();
    void ANNOUNCE_TAG(const std::string & tagname);

	void set_max_io_size(SV_UINT& );

private:


    SV_OFFSET_TYPE m_start;
    SV_OFFSET_TYPE m_end;
    OS_VAL m_baseline;
	SV_UINT m_max_io_size;

	bool m_validate;
    bool m_v2;
	SV_ULONGLONG m_volsize;

    SVDConvertorPtr m_svdConvertor ;
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

    DifferentialFile(DifferentialFile const & );
    DifferentialFile& operator=(DifferentialFile const & );

};

#endif
