//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : cdpv1datafilereader.h
//
// Description: 
//

#ifndef CDPV1DATAFILEREADER__H
#define CDPV1DATAFILEREADER__H

#include "cdpv1globals.h"
#include "cdpdatafilereader.h"
#include "differentialfile.h"
#include "portablehelpers.h"
#include "boost/shared_ptr.hpp"

class CDPV1DataFileReader : public CDPDataFileReader
{
public:
	CDPV1DataFileReader(const std::string & path);
	virtual ~CDPV1DataFileReader();

	virtual bool Init();
	virtual bool Parse();

	void SetBaseline(OS_VAL baseline) { m_file.SetBaseline(baseline); m_baseline = baseline;}
	virtual void SetStartOffset(SV_OFFSET_TYPE start) { m_start = start; }
	virtual void SetEndOffset(SV_OFFSET_TYPE end)     { m_end = end ; }
	virtual SV_ULONGLONG LogicalSize() { return (m_end - m_start); }


	//class CDPV1DRTDReader : public DRTDReader
	//{
	//public:
	//	CDPV1DRTDReader(DifferentialFile & diff_file, const cdp_drtd_t &drtd);
	//	virtual SV_UINT read(char* buffer, SV_UINT length);
 //       
	//private:

	//	DifferentialFile & m_file;
	//	cdp_drtd_t m_drtd;
	//	SV_ULONGLONG m_bytesRead;
	//};

	//virtual bool FetchDRTDReader(const cdp_drtd_t &drtd, boost::shared_ptr<DRTDReader> &);
 //   virtual bool FetchDRTDReader(const cdp_drtdv2_t &drtd, boost::shared_ptr<DRTDReader> &);

protected:

	bool ParseTsoFile();
    
private:
	
	DifferentialFile m_file;
	OS_VAL m_baseline;
	
	SV_OFFSET_TYPE m_start;
	SV_OFFSET_TYPE m_end;

	CDPV1DataFileReader(CDPV1DataFileReader const & );
    CDPV1DataFileReader& operator=(CDPV1DataFileReader const & );
};

#endif


