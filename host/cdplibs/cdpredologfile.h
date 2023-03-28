//
// Copyright (c) 2005 InMage
// This file contains proprietary and confidential information and
// remains the unpublished property of InMage. Use, disclosure,
// or reproduction is prohibited except as permitted by express
// written license aggreement with InMage.
//
// File       : redologfile.h
//
// Description: 
//

#ifndef CDPREDOLOGFILE__H
#define CDPREDOLOGFILE__H

#include "cdpglobals.h"
#include "file.h"
#include "portablehelpers.h"

#define CDP_REDOLOG_FILENAME_PREFIX "cdp_redolog_"
#define CDP_REDOLOG_FILENAME_SUFFIX ".dat"

class CdpRedoLogFile
{
public:
	CdpRedoLogFile(const std::string & path,SV_UINT sector_size,SV_UINT max_rw_size)
		:m_filename(path),m_sector_size(sector_size),m_max_rw_size(max_rw_size),m_handle(ACE_INVALID_HANDLE)
	{
	}
	virtual ~CdpRedoLogFile()
	{
		if(m_handle != ACE_INVALID_HANDLE)
			ACE_OS::close(m_handle);
	}

	bool Parse();
	bool WriteMetaData(cdp_drtdv2s_t & redo_drtds);
	bool WriteData(char * buffer, SV_UINT length_of_data);
	bool GetData(char * buffer, SV_UINT length_of_data);
	cdp_drtdv2s_t & GetDrtds( );
	static SV_UINT GetSizeOfHeader(){return (SVD_PREFIX_SIZE + SVD_REDOLOG_HEADER_SIZE + SVD_PREFIX_SIZE + SVD_LODC_SIZE);}
	std::string GetName(){return m_filename;}

private:

	bool ReadHeader(ACE_HANDLE handle, const SVD_PREFIX & prefix);
	bool ReadLODC(ACE_HANDLE handle, const SVD_PREFIX & prefix);
	bool ReadDRTD(ACE_HANDLE handle, const SVD_PREFIX & prefix);

	CdpRedoLogFile(CdpRedoLogFile const & );
    CdpRedoLogFile& operator=(CdpRedoLogFile const & );

	SV_ULONGLONG m_number_of_drtds;
	SV_ULONGLONG m_length_of_data_changes;
	cdp_drtdv2s_t m_redo_drtds;
	std::string m_filename;
	SV_UINT m_sector_size;
	SV_UINT m_max_rw_size;
	ACE_HANDLE m_handle;

};

#endif
