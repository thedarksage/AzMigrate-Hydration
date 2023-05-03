/*
 * Copyright (c) 2005 InMage
 * This file contains proprietary and confidential information and
 * remains the unpublished property of InMage. Use, disclosure,
 * or reproduction is prohibited except as permitted by express
 * written license aggreement with InMage.
 *
 * File       : inmageproduct_port.cpp
 *
 * Description: Windows specific port of inmageproduct.cpp
 */

#include <string>
#include <cassert>

#include "inmageproduct.h"
#include "entity.h"
#include "error.h"
#include "portableheaders.h"
#include "hostagenthelpers_ported.h"
#include "synchronize.h"
#include "version.h"

#define FILTER_NAME _T( "involflt" )
#define DISK_FILTER_NAME _T( "InDskFlt" )
#define FILTER_EXTENSION _T( ".sys" )

/*
 * FUNCTION NAME : GetProductVersionFromResource
 *
 * DESCRIPTION : 
 *
 * INPUT PARAMETERS : 
 *
 * OUTPUT PARAMETERS : 
 *
 * NOTES :
 *
 * return value : 
 *
 */
bool InmageProduct::GetProductVersionFromResource(char* pszFile, char* pszBuffer, int pszBufferSize)
{
    assert( NULL != pszFile );
    assert( NULL != pszBuffer );

    //
    // TODO: clean up this code. Make language/region independent
    //
    DWORD dwVerHnd;
    DWORD dwVerInfoSize = GetFileVersionInfoSize( pszFile, &dwVerHnd );
    if( !dwVerInfoSize )
        return FALSE;

    BYTE* pVerInfo = new (std::nothrow) BYTE[ dwVerInfoSize ];
    if( !pVerInfo )
        return FALSE;

    BOOL fRet = GetFileVersionInfo( pszFile, 0L, dwVerInfoSize, pVerInfo );
    if (fRet)
    {
        TCHAR* pVer = NULL;
        UINT ccVer = 0;
        fRet = VerQueryValue(pVerInfo,                            
                _T("\\StringFileInfo\\040904B0\\FileVersion"),
                (void**)&pVer,
                    &ccVer);
        if (fRet && pVer)
			inm_tcscpy_s(pszBuffer, pszBufferSize, pVer);
    }
 
    delete [] pVerInfo;
    return fRet;
}

