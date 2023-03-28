// inmwinzip.cpp : Defines the entry point for the console application.
//
#include "stdafx.h"
#include <windows.h>
#include <Shldisp.h>
#include <tlhelp32.h>
#include <iostream>

int _tmain(int argc, _TCHAR* argv[])
{
    HRESULT          hResult;
    IShellDispatch *pISD;
    Folder                *zipArchive = NULL;
    VARIANT          vArchive, vSourceItem, vCpHereOptions;
    unsigned long  ulReturnCode = 0 ;
    if( argc != 3 ) 
        return 6 ;
    OSVERSIONINFOEX osvi ;
    ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX) ; 
    if( !GetVersionEx( (OSVERSIONINFO *)&osvi ) || osvi.dwMajorVersion != 6 )
        return 7 ;

    CoInitialize(NULL);
     unsigned char emptyzip[] = {80,75,5,6,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0};

     HANDLE h = CreateFile( argv[2],                // file to open
                       GENERIC_READ,          // open for reading
                       FILE_SHARE_READ,       // share for reading
                       NULL,                  // default security
                       CREATE_ALWAYS,         // existing file only
                       FILE_ATTRIBUTE_NORMAL, // normal file
                       NULL); 
    DWORD bytesWritten = 0 ;
    WriteFile( h, emptyzip, 22, &bytesWritten, NULL ) ;
    CloseHandle( h ) ;

    hResult = CoCreateInstance(CLSID_Shell, NULL, CLSCTX_INPROC_SERVER,IID_IShellDispatch, (void **)&pISD);

    if  (SUCCEEDED(hResult))
    {
        VariantInit(&vArchive);
        vArchive.vt = VT_BSTR;
        vArchive.bstrVal = argv[2];
        hResult = pISD->NameSpace(vArchive, &zipArchive);
        
        if  (SUCCEEDED(hResult))
        {
            VariantInit(&vSourceItem);
            vSourceItem.vt = VT_BSTR;
            vSourceItem.bstrVal = argv[1] ;

            VariantInit(&vCpHereOptions);           
        
            hResult = zipArchive->CopyHere(vSourceItem, vCpHereOptions);
            if (SUCCEEDED( hResult ) ) 
            {
                HANDLE hThrd[64]; 
                HANDLE snapHandle = CreateToolhelp32Snapshot(TH32CS_SNAPALL ,0);  //TH32CS_SNAPMODULE, 0);
                DWORD NUM_THREADS = 0;
                if (snapHandle != INVALID_HANDLE_VALUE) 
               {
                    THREADENTRY32 te;
                    te.dwSize = sizeof(te);
                    if (Thread32First(snapHandle, &te)) 
                    {
                        do 
                        {
                            if (te.dwSize >= (FIELD_OFFSET(THREADENTRY32, th32OwnerProcessID) + sizeof(te.th32OwnerProcessID)) && 
                                (te.th32OwnerProcessID == GetCurrentProcessId()) && (te.th32ThreadID != GetCurrentThreadId())) 
                            {
                                hThrd[NUM_THREADS] = OpenThread(THREAD_ALL_ACCESS, FALSE, te.th32ThreadID);
                                NUM_THREADS++;
                                if( NUM_THREADS == 64 )
                                    break ;
                            }
                            te.dwSize = sizeof(te);
                        } while (Thread32Next(snapHandle, &te) && NUM_THREADS < 64 );
                    }
                    else
                        ulReturnCode = 1 ;
                    CloseHandle(snapHandle);
                    WaitForMultipleObjects(NUM_THREADS, hThrd , TRUE , INFINITE);
                    for ( DWORD i = 0; i < NUM_THREADS ; i++ )
                    {
                        CloseHandle( hThrd[i] );
                    }
                } //if invalid handle
                else
                    ulReturnCode = 2 ;
            } //if CopyHere() hResult is S_OK
            else
                ulReturnCode = 3 ;
            zipArchive->Release();
          //  SysFreeString( wchSourceitem ) ;
        }
        else
            ulReturnCode = 4 ;
        //SysFreeString( wchZipArchive ) ;
        pISD->Release();
    }
    else
        ulReturnCode = 5 ;
    CoUninitialize();
    return ulReturnCode;
} 

