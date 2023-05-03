// volopt.cpp: Volume optimizer tool
// 'Optimizes' a volume for volume replication by overwriting all unused blocks with zeros.
//define NOMINMAX to avoid ambiguous definitions for min and max
#define NOMINMAX 
#include <iostream>
#include "hostagenthelpers.h"
#include "portablehelpersmajor.h"
#include "inmsafecapis.h"
#include "inmsafeint.h"
#include "inmageex.h"
#include "terminateonheapcorruption.h"

int  const  VOLOPT_DEF_FREE_SPACE_SAFETY_MARGIN = 80;           // 80 MB
int  const  VOLOPT_WBUF_SZ                      = 1024 * 1024;  // 1 MB
char const  *DEFAULT_FILE_NAME                  = "volopt.dat";

//Bugfix for bug#227
ULONG sectorSize = 0;
ULONG bufSize = 0;
char *bufPtr = NULL;
char *alignBufPtr = NULL;


HRESULT OptimizeVolume( char const* pszLogicalVolume, ULONGLONG freeSpaceMargin, char const* fileName)
{
	char const *defaultFileName = "volopt.dat";

	DebugPrintf( "@ LINE %d in FILE %s \n", __LINE__, __FILE__ );
    DebugPrintf( "ENTERING OptimizeSourceVolume()...\n" );

    if( NULL == pszLogicalVolume || !isalpha( pszLogicalVolume[ 0 ] ) )
    {
        return( E_INVALIDARG );
    }

    char szFilename[ BUFSIZ ];
	if (NULL != fileName) {
        inm_sprintf_s(szFilename, ARRAYSIZE(szFilename), "%s\\%s", pszLogicalVolume, fileName);
	}
	else {
        inm_sprintf_s(szFilename, ARRAYSIZE(szFilename), "%s\\%s", pszLogicalVolume, defaultFileName);
	}

    HRESULT hr = S_OK;
	// PR#10815: Long Path support
    HANDLE hFile = SVCreateFile( szFilename, 
                               GENERIC_WRITE, 
                               FILE_SHARE_READ | FILE_SHARE_WRITE, 
                               NULL, 
                               CREATE_ALWAYS, 
                               FILE_ATTRIBUTE_NORMAL | FILE_FLAG_NO_BUFFERING, 
                               NULL );

    do
    {
        if( INVALID_HANDLE_VALUE == hFile )
        {
            hr = HRESULT_FROM_WIN32( GetLastError() );
            DebugPrintf( "@ LINE %d in FILE %s \n", __LINE__, __FILE__ );
            DebugPrintf( hr, "FAILED CreateFile()... hr = %08X\n", hr );
            break;
        }

        ULARGE_INTEGER ullQuota = { 0 };
        ULARGE_INTEGER ullTotal = { 0 };
        ULARGE_INTEGER ullFree = { 0 };

		// PR#10815: Long Path support
        if( !SVGetDiskFreeSpaceEx( pszLogicalVolume, &ullQuota, &ullTotal, &ullFree ) )
        {
            hr = HRESULT_FROM_WIN32( GetLastError() );
            DebugPrintf( "@ LINE %d in FILE %s \n", __LINE__, __FILE__ );
            DebugPrintf( hr, "FAILED GetDiskFreeSpaceEx()... hr = %08X\n", hr );

			std::cerr << "Failed to get free disk space" << std::endl;

			break;
        }

        ULARGE_INTEGER ullRemaining = { 0 };
        if( ullFree.QuadPart > freeSpaceMargin )
        {
            ullRemaining.QuadPart = ullFree.QuadPart - freeSpaceMargin;
        }

        // The write buffer needs to be disk sector aligned for non-buffered I/O
		// PR#10815: Long Path support
        if(!SVGetDiskFreeSpace(pszLogicalVolume, NULL, &sectorSize, NULL, NULL))
        {
            hr = HRESULT_FROM_WIN32( GetLastError() );
            DebugPrintf( "@ LINE %d in FILE %s \n", __LINE__, __FILE__ );
            DebugPrintf( hr, "FAILED GetDiskFreeSpace()... hr = %08X\n", hr );

			std::cerr << "Failed to get free disk space" << std::endl;

			break;
        }
      
        // Set the buffer size  
        bufSize = VOLOPT_WBUF_SZ;

        // Setup the buffer to be sector-aligned
        size_t buflen;
        INM_SAFE_ARITHMETIC(buflen = InmSafeInt<ULONG>::Type(bufSize) + sectorSize, INMAGE_EX(bufSize)(sectorSize))
        bufPtr = (char *)malloc(buflen);
        alignBufPtr = (char *)(((ULONG)bufPtr + sectorSize ) & ~ (sectorSize -1));

        ZeroMemory( alignBufPtr, bufSize );
 
        // The starting address on the disk also needs to be 
        // sector-align. So, set the file pointer to the beginning
        // of the file - make absolutely sure!
        LONG lowpart = 0;
        LONG highpart = 0;
       
        if( -1 == SetFilePointer( hFile,
                                  lowpart,
                                  &( highpart ),
                                  FILE_BEGIN ) )
        {
            hr = HRESULT_FROM_WIN32( GetLastError() );
            DebugPrintf( "@ LINE %d in FILE %s \n", __LINE__, __FILE__ );
            DebugPrintf( hr, "FAILED SetFilePointer()... hr = %08X\n", hr );

			std::cerr << "Failed to write file" << std::endl;

			break;
        } 

		int i = 0;
        while( ullRemaining.QuadPart > 0 )
        {
			DWORD dwToWrite = std::min( bufSize, (DWORD) ullRemaining.QuadPart );
            DWORD dwWritten = 0;
			i++;

            if( !WriteFile( hFile, alignBufPtr, dwToWrite, &dwWritten, NULL ) )
            {
                hr = HRESULT_FROM_WIN32( GetLastError() );
                DebugPrintf( "@ LINE %d in FILE %s \n", __LINE__, __FILE__ );
                DebugPrintf( hr, "FAILED WriteFile()... hr = %08X\n", hr );

				std::cerr << "Failed to write file" << std::endl;
				
				break;
            }

            ullRemaining.QuadPart -= dwWritten;

			// Check available free space at every safety margin boundary
			// This is to allow for change in FS utilization while the volopt
			// is running. We do not want to accidentally fill up the volume
			if (( (VOLOPT_DEF_FREE_SPACE_SAFETY_MARGIN * 1024 * 1024) / VOLOPT_WBUF_SZ) == i) {
				if( !FlushFileBuffers( hFile ) )
				{
					hr = HRESULT_FROM_WIN32( GetLastError() );
					DebugPrintf( "@ LINE %d in FILE %s \n", __LINE__, __FILE__ );
					DebugPrintf( hr, "FAILED FlushFileBuffers()... hr = %08X\n", hr );

					std::cerr << "Failed to flush buffers to disk" << std::endl;

					break;
				}

				// PR#10815: Long Path support
				if( !SVGetDiskFreeSpaceEx( pszLogicalVolume, &ullQuota, &ullTotal, &ullFree ) )
				{
					hr = HRESULT_FROM_WIN32( GetLastError() );
					DebugPrintf( "@ LINE %d in FILE %s \n", __LINE__, __FILE__ );
					DebugPrintf( hr, "FAILED GetDiskFreeSpaceEx()... hr = %08X\n", hr );

					std::cerr << "Failed to get free disk space" << std::endl;

					break;
				}

				if( ullFree.QuadPart > freeSpaceMargin )
				{
					ullRemaining.QuadPart = ullFree.QuadPart - freeSpaceMargin;
				}
				else {
					ullRemaining.QuadPart = 0;
				}
				i = 0;
			}
		}

    } while( FALSE );

	free(bufPtr);
	CloseHandle( hFile );
    DeleteFile( szFilename );

    return( hr );
}

void ShowUsage()
{
	std::cout << "volopt: Prepares a volume for efficient WAN replication." << std::endl ;
    std::cout << "usage: volopt <Volume> [freespace margin in MiB] [filename]" << std::endl ;
	std::cout << "E.g. volopt e: 1024 volopt_e.file" << std::endl ;
	std::cout << "  ensures that at least 1GB of free space remains on the file system" << std::endl ;
}

int main( int argc, char* argv[] )
{
	TerminateOnHeapCorruption();
    init_inm_safe_c_apis();
    if( 2 != argc && 3 != argc && 4 !=argc)
    {
        ShowUsage();
        return( 0 );
    }

    char const* pszDrive = argv[ 1 ];
    if( !isalpha( pszDrive[ 0 ] ) )
    {
        ShowUsage();
        return( 0 );
    }

	ULONGLONG freeSpaceMargin = VOLOPT_DEF_FREE_SPACE_SAFETY_MARGIN;
	
	if (argc >= 3) {
		freeSpaceMargin = atol(argv[2]);
	}
	if (freeSpaceMargin< VOLOPT_DEF_FREE_SPACE_SAFETY_MARGIN) {
		std::cerr << "Input free space margin too low, defaulting to " << VOLOPT_DEF_FREE_SPACE_SAFETY_MARGIN << " MiB" << std::endl;
		freeSpaceMargin = VOLOPT_DEF_FREE_SPACE_SAFETY_MARGIN;
	}
	freeSpaceMargin = freeSpaceMargin * 1024 * 1024;

	char *fileName = NULL;
	if (argc >=4) {
		fileName = argv[3];
	}

    HRESULT hr = S_OK;
    hr = OptimizeVolume( pszDrive, freeSpaceMargin, fileName);

    if( FAILED( hr ) )
    {
        printf( "Couldn't optimize volume, hr = 0x%08X\n", hr );
    }
    else
    {
		std::cout << "Volume optimization: done" << std::endl;
    }
    return( 0 );
}
