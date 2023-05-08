/* scsicmdutil.cpp : Defines the entry point for the console application.
This utility issues various scsi commands as per the SPC-3 specifications. 
*/

#include "scsicommands.h"
#include <stddef.h>
#include <strsafe.h>
#include <malloc.h>
#include "terminateonheapcorruption.h"

bool enabledebug = false;

int ParseCommandLine(int argc, _TCHAR* argv[]);
HANDLE OpenDisk(_TCHAR* disknumber);
int IsDiskWritable(HANDLE hFile);

int TestUnitReady(HANDLE hFile);
int RegisterNewReservationId(HANDLE hFile, UCHAR *buf);
int ReadSCSI3PRholderKey(HANDLE hFile, UCHAR *buf);
int ReadAllKeys(HANDLE hFile);
int	ClearSCSI3Reservation(HANDLE hFile,  UCHAR *buf);

int utility_usage()
{
    _tprintf(_T(" scsicmdutil.exe  <debug> [-disk] [disknumber] [op]\n\n"));
    _tprintf(_T(" scsi3clear    - PR OUT service action : Clear\n"));
    _tprintf(_T(" writable      - to verify whether the disk is writable or not\n"));
    _tprintf(_T(" register      - PR OUT service action : Register and ignore existing key\n"));
    _tprintf(_T(" readkeys      - PR IN service action : Read keys\n"));
    _tprintf(_T(" readres       - PR IN service action : Read reservation\n"));	
    _tprintf (_T(" tur          - Test Unit ready\n"  ));
    _tprintf (_T(" scsicmdutil.exe /? - utility usage\n\n"));
    _tprintf (_T(" Debug        - Enable debug logs for scsi commands \n"));
    return 0;
}

int ParseCommandLine(int argc, _TCHAR* argv[])
{
   int iparsed = 0;
   unsigned int disknumber = 0;
   HANDLE hFile = NULL;
   iparsed++;

   while(argc > iparsed) {
      if (0 == _tcsicmp(argv[iparsed], USAGE)) {
          utility_usage();
          return 1;
       } else if (0 == _tcsicmp(argv[iparsed], DEBUG)){
          enabledebug = true;
      } else if (0 == _tcsicmp(argv[iparsed], DISK)) {
           iparsed++;
           if (argc != iparsed) {
               disknumber = _ttol(argv[iparsed]);
               _tprintf(_T(" Selected Disk# : %u \n"), disknumber);			   
               hFile = OpenDisk(argv[iparsed]);
               if (NULL == hFile)
                    return 0;
           } else {
               _tprintf(_T(" Incomplete Command \n"));
               utility_usage();
               return 0;
           }
      }  else if ( (NULL != hFile) && (0 == _tcsicmp(argv[iparsed], REGISTER ))) {
          _tprintf(_T(" Read and Register the PR holder key \n"));
          UCHAR *buf;
          buf = (UCHAR*)malloc(PARAM_DATA_LENGTH * sizeof(UCHAR));
          if (NULL != buf) {
              ReadSCSI3PRholderKey(hFile, buf);
              RegisterNewReservationId(hFile, buf);
          } else {
              _tprintf(_T(" Insufficient resources; Exiting\n"));
              goto failure;
          }
      } else if ((NULL != hFile) &&(0 == _tcsicmp(argv[iparsed], READALL))) {
          _tprintf (_T(" PR IN service action : Read keys \n"));
          ReadAllKeys(hFile);
      } else if ((NULL != hFile) && (0 == _tcsicmp(argv[iparsed], READRES)))  {
          _tprintf (_T(" PR IN service action : Read reservation \n"));
          ReadSCSI3PRholderKey(hFile, NULL);
      } else if ((NULL != hFile) && (0 == _tcsicmp(argv[iparsed], CLEAR))) {
          _tprintf (_T(" PR OUT service action : Clear \n"));
          UCHAR *buf;
          buf = (UCHAR*)malloc(PARAM_DATA_LENGTH * sizeof(UCHAR));
          if (NULL != buf) {
              if (1 == IsDiskWritable(hFile)) {
                  if (1 == ReadSCSI3PRholderKey(hFile, buf)){
                       ClearSCSI3Reservation(hFile, buf);
                  }
              } else {
                  if (1 == ReadSCSI3PRholderKey(hFile, buf)) {
                      if (1 == RegisterNewReservationId(hFile, buf)){
                        ClearSCSI3Reservation(hFile, buf);
                      }
                  }
              }
         } else {
              _tprintf(_T(" Insufficient resources; Exiting \n"));
              goto failure;
         }
      } else if ((NULL != hFile) && (0 == _tcsicmp(argv[iparsed], TUR))) {
          TestUnitReady(hFile);
      } else if ((NULL != hFile) && (0 == _tcsicmp(argv[iparsed] , WRITABLE))) {
          IsDiskWritable(hFile);
      } else {
          _tprintf(_T(" Unexpected command; Either specified disk does not exist or wrong switch \n"));
          goto failure;
      }
      if (enabledebug)
        _tprintf(_T("iparsed : %d\n"), iparsed);
      iparsed++;	   
  } 
 if (NULL != hFile)
        CloseHandle( hFile);
  if (argc == iparsed) {
      _tprintf(_T(" Command Complete \n"));
      return 1;
  }

failure :
   if (NULL != hFile)
        CloseHandle( hFile);
   _tprintf(_T(" ****Incorrect Usage****** \n"));
   utility_usage();
   return 0;
 }

int _tmain(int argc, _TCHAR* argv[])
{
	TerminateOnHeapCorruption();
    unsigned int cmdlineoptions;
    cmdlineoptions = argc;

    if (cmdlineoptions == 2) {
        if (0 == _tcsicmp(argv[1], USAGE)) {
            utility_usage();
            return 1;
        }
    }

    if (cmdlineoptions > 5 || cmdlineoptions <=3) {
        _tprintf(_T(" ****Incorrect Usage****** \n"));
        utility_usage();
        return 0;
    }
    ParseCommandLine(argc, argv);

    return 1;
}
int TestUnitReady(HANDLE hFile)
{
    SCSI_PASS_THROUGH_WITH_BUFFERS sptwb;
    memset(&sptwb, 0, sizeof(SCSI_PASS_THROUGH_WITH_BUFFERS));

    SCSI_PASS_THROUGH *spt = (SCSI_PASS_THROUGH *)&sptwb.spt;
    
    DWORD breturned = 0;
    BOOL bReturn;

    spt->Length = sizeof(SCSI_PASS_THROUGH);
    spt->CdbLength = 6;
    sptwb.spt.SenseInfoOffset = offsetof(SCSI_PASS_THROUGH_WITH_BUFFERS, ucSenseBuf);
    sptwb.spt.SenseInfoLength = SPT_SENSE_LENGTH;

    spt->DataIn = SCSI_IOCTL_DATA_UNSPECIFIED;

    spt->DataTransferLength = 0;
    spt->TimeOutValue = 2;
    spt->Cdb[0] = 0; // opcode
    
    bReturn = DeviceIoControl(hFile, IOCTL_SCSI_PASS_THROUGH, 
                                          &sptwb, sizeof(SCSI_PASS_THROUGH_WITH_BUFFERS), 
                                          &sptwb, sizeof(SCSI_PASS_THROUGH_WITH_BUFFERS), 
                                          &breturned, NULL);

    if (0 != bReturn) {
        if (spt->ScsiStatus != 0) {
            _tprintf (_T(" TUR status : %x \n"), spt->ScsiStatus);
            if (02 == spt->ScsiStatus) {
                _tprintf(_T(" SenseInfoLength : %d \n"), spt->SenseInfoLength);
                _tprintf(_T("Check Condition \n"));
            } else if(0x18 == spt->ScsiStatus) {
                _tprintf(_T("Reservation conflict \n"));
            }
        } else {
            _tprintf(_T(" IOCTL SCSI PASS THROUGH request with TestUnitReady is successful \n"));
        }
    } else  {
        _tprintf(_T("Failed with status : %d \n"), GetLastError());
        GetLastErrToStr(GetLastError());
        return 0;
    }
    return 1;
}

HANDLE OpenDisk(_TCHAR* disknum)
{
    HANDLE hFile = NULL;
    TCHAR str[32];
    
    StringCbPrintf(str, sizeof(str), _T("\\\\.\\physicaldrive%s"), disknum);
        
    hFile = CreateFile(str, GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);

    if (INVALID_HANDLE_VALUE == hFile) {
        _tprintf(_T(" opening a disk %ws failed with error %x \n"),str, GetLastError());
        DWORD err = GetLastError();
        if (ERROR_FILE_NOT_FOUND  == err)
            _tprintf(_T(" Disk does not exist"));
        GetLastErrToStr(err);
        return NULL;
    } else {
        _tprintf(_T(" Successfully opened a handle< %x > on Disk : %ws \n"), hFile, str);
    }
    return hFile;
}

/*PR IN : Opcode : 0x5E
Service Action : 01h - Read Reservation
*/
int ReadSCSI3PRholderKey(HANDLE hFile, UCHAR *buf)
{
    SCSI_PASS_THROUGH_WITH_BUFFERS sptwb;

    memset(&sptwb, 0, sizeof(SCSI_PASS_THROUGH_WITH_BUFFERS));

    DWORD breturned = 0;
    BOOL bReturn = 0;
    DWORD length = 0;

    sptwb.spt.Length = sizeof(SCSI_PASS_THROUGH);

    sptwb.spt.DataBufferOffset = offsetof(SCSI_PASS_THROUGH_WITH_BUFFERS, ucDataBuf);
    sptwb.spt.DataTransferLength = SPTWB_DATA_LENGTH;

    sptwb.spt.SenseInfoOffset = offsetof(SCSI_PASS_THROUGH_WITH_BUFFERS, ucSenseBuf);
    sptwb.spt.SenseInfoLength = SPT_SENSE_LENGTH;

    sptwb.spt.DataIn = SCSI_IOCTL_DATA_IN;

    sptwb.spt.TimeOutValue = 2;
    sptwb.spt.CdbLength = 0xa;

    sptwb.spt.Cdb[0] = 0x5E;
    sptwb.spt.Cdb[1] = 01;
    sptwb.spt.Cdb[8] = SPTWB_DATA_LENGTH;
    length =  offsetof(SCSI_PASS_THROUGH_WITH_BUFFERS,ucDataBuf) + sptwb.spt.DataTransferLength;

    bReturn = DeviceIoControl(hFile,  
                              IOCTL_SCSI_PASS_THROUGH, 
                              &sptwb, 
                              sizeof(SCSI_PASS_THROUGH),
                              &sptwb,  
                              length,
                              &breturned, 
                              NULL);
    _tprintf(_T(" Issuing SCSI-3 Read PR Holder key \n"));
    if (0 != bReturn) {
        _tprintf(_T("\n"));        
        if (0 == sptwb.spt.ScsiStatus) {
            _tprintf(_T(" Reading SCSI-3 Reservation holder key is successful\n"));
                // Copy data buffer to acts as input to the CLEAR command
            UCHAR *src = &sptwb.ucDataBuf[0];
            PrintDataBuffer(src, PARAM_DATA_LENGTH);
            if (NULL != buf) {
                memcpy_s((void*)buf, PARAM_DATA_LENGTH, (void*)src, PARAM_DATA_LENGTH);
            }
        }
        else {
            _tprintf(_T(" Reading SCSI-3 Reservation holder key is failed with SCSI status %x, Could not proceed further\n"), sptwb.spt.ScsiStatus);
        }
        if (enabledebug) {
            _tprintf(_T(" Returned Bytes : %d \n"), breturned);
            PrintStatusResults(bReturn,
                               breturned,
                               (PSCSI_PASS_THROUGH_WITH_BUFFERS)&sptwb,
                               length);
        }
        if (0 != sptwb.spt.ScsiStatus)
            return 0;
    } else  {
        _tprintf(_T("IOCTL to get SCSI-3 read reservation is failed with error : %d"), GetLastError());
        GetLastErrToStr(GetLastError());
        return 0;
    }

    return 1;
}

/*PR IN : Opcode : 0x5E
Service Action : 00h - Read All keys
*/
int ReadAllKeys(HANDLE hFile)
{
    SCSI_PASS_THROUGH_WITH_BUFFERS sptwb;

    memset(&sptwb, 0, sizeof(SCSI_PASS_THROUGH_WITH_BUFFERS));

    DWORD breturned = 0;
    BOOL bReturn = 0;
    DWORD length = 0;

    sptwb.spt.Length = sizeof(SCSI_PASS_THROUGH);

    sptwb.spt.DataBufferOffset = offsetof(SCSI_PASS_THROUGH_WITH_BUFFERS, ucDataBuf);
    sptwb.spt.DataTransferLength = SPTWB_DATA_LENGTH;

    sptwb.spt.SenseInfoOffset = offsetof(SCSI_PASS_THROUGH_WITH_BUFFERS, ucSenseBuf);
    sptwb.spt.SenseInfoLength = SPT_SENSE_LENGTH;

    sptwb.spt.DataIn = SCSI_IOCTL_DATA_IN;

    sptwb.spt.TimeOutValue = 2;
    sptwb.spt.CdbLength = 0xa;

    sptwb.spt.Cdb[0] = 0x5E;
    sptwb.spt.Cdb[1] = 00; // Read Keys
    sptwb.spt.Cdb[7] = 00;
    sptwb.spt.Cdb[8] = 64;
    length = offsetof(SCSI_PASS_THROUGH_WITH_BUFFERS,ucDataBuf) + sptwb.spt.DataTransferLength;

   bReturn = DeviceIoControl(hFile, 
                             IOCTL_SCSI_PASS_THROUGH, 
                             &sptwb, 
                             length,
                             &sptwb,  
                             length,
                             &breturned, 
                             NULL);
    _tprintf(_T(" Issuing SCSI-3 Read All PR Holder keys \n"));
    if (0 != bReturn) {
       _tprintf(_T("\n"));        
        if (0 == sptwb.spt.ScsiStatus) {
            _tprintf(_T(" SCSI-3 Read All PR holder keys is successful, Enable DEBUG mode to view all keys\n"));             
        }
        else
            _tprintf(_T(" Reading SCSI-3 Reservation holder keys is failed with SCSI status %x\n"), sptwb.spt.ScsiStatus);

        if (enabledebug) {
            _tprintf(_T(" bReturned : %d \n"), breturned);
            PrintStatusResults(bReturn,
                               breturned,
                               (PSCSI_PASS_THROUGH_WITH_BUFFERS)&sptwb,
                               length);
        }
    } else  {
            _tprintf(_T("IOCTL to get SCSI-3 read reservation keys is failed with error  : %d \n"), GetLastError());
            GetLastErrToStr(GetLastError());
            return 0;
    }

    return 1;
}


/*PR OUT : Opcode : 0x5F
Service Action : 03h - Clear - Clear the reservation
Parameter Buffer : Service Action Reservation key is set to the actual reservation key from the I-T nexus which is registered
*/
int ClearSCSI3Reservation(HANDLE hFile, UCHAR *buf)
{
    SCSI_PASS_THROUGH_WITH_BUFFERS sptwb;

    memset(&sptwb, 0, sizeof(SCSI_PASS_THROUGH_WITH_BUFFERS));

    DWORD breturned = 0;
    BOOL bReturn = 0;
    DWORD length = 0;

    sptwb.spt.Length = sizeof(SCSI_PASS_THROUGH);

    sptwb.spt.DataBufferOffset = offsetof(SCSI_PASS_THROUGH_WITH_BUFFERS, ucDataBuf);

    sptwb.spt.SenseInfoOffset = offsetof(SCSI_PASS_THROUGH_WITH_BUFFERS, ucSenseBuf);
    sptwb.spt.SenseInfoLength = SPT_SENSE_LENGTH;

    sptwb.spt.DataTransferLength = PARAM_DATA_LENGTH;
    
    sptwb.spt.DataIn = SCSI_IOCTL_DATA_OUT;

    sptwb.spt.TimeOutValue = 2;
    sptwb.spt.CdbLength = 0xa;

    sptwb.spt.Cdb[0] = 0x5F;
    sptwb.spt.Cdb[1] = 03;
    sptwb.spt.Cdb[8] = PARAM_DATA_LENGTH;

    length = offsetof(SCSI_PASS_THROUGH_WITH_BUFFERS,ucDataBuf) +
       sptwb.spt.DataTransferLength;

    //Copy the reservation key read from PR IN command
    UCHAR *reskey = &sptwb.ucDataBuf[0];
    UCHAR *srckey = &buf[RES_KEY_LENGTH];
    memcpy_s((void*)reskey, RES_KEY_LENGTH, srckey, RES_KEY_LENGTH);

    //PrintDataBuffer(reskey, 8);
    
    _tprintf(_T(" Reservation Type : %x \n\n"), reskey[21]);

    bReturn = DeviceIoControl(hFile, 
                             IOCTL_SCSI_PASS_THROUGH, 
                             &sptwb, 
                             length,
                             &sptwb,  
                             length,
                             &breturned, 
                             NULL);
    _tprintf(_T(" Issuing SCSI-3 CLEAR command \n"));
    if (0 != bReturn) {
        printf ("\n");        
        if (0 == sptwb.spt.ScsiStatus) {
            _tprintf(_T(" Issuing SCSI-3 clear command is successful\n"));
        }
        else
            _tprintf(_T(" Issuing SCSI-3 clear comand is failed with SCSI status %x\n"), sptwb.spt.ScsiStatus);

        if (enabledebug) {
            _tprintf(_T(" bReturned : %d \n"), breturned);
            PrintStatusResults(bReturn,
                               breturned,
                               (PSCSI_PASS_THROUGH_WITH_BUFFERS)&sptwb,
                               length);
        }
        if (0 != sptwb.spt.ScsiStatus)
            return 0;
    } else  {
        _tprintf(_T(" IOCTL to issue SCSI-3 CLEAR is failed with status : %d"), GetLastError());
        GetLastErrToStr(GetLastError());
        return 0;
    }

    return 1;
}

/*PR OUT : Opcode : 0x5F
Service Action : 06h - Register and Ignore existing key
Allocation Lenght has to be set to 24
Parameter Buffer : Register using the key read through ReadSCSI3PRholderKey()
*/
int RegisterNewReservationId(HANDLE hFile, UCHAR *buf)
{
   SCSI_PASS_THROUGH_WITH_BUFFERS sptwb;

    memset(&sptwb, 0, sizeof(SCSI_PASS_THROUGH_WITH_BUFFERS));

    DWORD breturned = 0;
    BOOL bReturn = 0;
    DWORD length = 0;

    sptwb.spt.Length = sizeof(SCSI_PASS_THROUGH);

    sptwb.spt.DataBufferOffset = offsetof(SCSI_PASS_THROUGH_WITH_BUFFERS, ucDataBuf);
    sptwb.spt.DataTransferLength = PARAM_DATA_LENGTH;

    sptwb.spt.SenseInfoOffset = offsetof(SCSI_PASS_THROUGH_WITH_BUFFERS, ucSenseBuf);
    sptwb.spt.SenseInfoLength = SPT_SENSE_LENGTH;

    sptwb.spt.DataIn = SCSI_IOCTL_DATA_OUT;

    sptwb.spt.TimeOutValue = 2;
    sptwb.spt.CdbLength = 0xa;

    sptwb.spt.Cdb[0] = 0x5F;
    sptwb.spt.Cdb[1] = 0x06; //service action : REGISTER and ignore existing key
    sptwb.spt.Cdb[8] = PARAM_DATA_LENGTH;

    length = offsetof(SCSI_PASS_THROUGH_WITH_BUFFERS,ucDataBuf) +
                      sptwb.spt.DataTransferLength;

    //Register with persistent reservation id read using READ_RESERVATION to authorize CLEAR reservation    
    UCHAR *reskey = &sptwb.ucDataBuf[8];
    UCHAR *srckey = &buf[8];

    memcpy_s((void*)reskey, RES_KEY_LENGTH, srckey, RES_KEY_LENGTH);
    _tprintf(_T("Registering with following key \n"));
    PrintDataBuffer(srckey, RES_KEY_LENGTH);
        
    bReturn = DeviceIoControl(hFile, 
                              IOCTL_SCSI_PASS_THROUGH, 
                              &sptwb, 
                              length,
                              &sptwb,  
                              length,
                              &breturned, 
                              NULL);
    _tprintf(_T("Issuing SCSI-3 REGISTER and Ignore existing key command\n"));
    if (0 != bReturn) {
        if (0 == sptwb.spt.ScsiStatus) {
            _tprintf(_T(" Issuing SCSI-3 Register and Ignore existing key is successful\n"));
        }
        else {
            _tprintf(_T(" Issuing SCSI-3 REGISTER and Ignore existing key is failed with SCSI status %x\n"), sptwb.spt.ScsiStatus);            
        }
       if (enabledebug) {
            _tprintf(_T(" bReturned : %d Status : %x \n"), breturned, sptwb.spt.ScsiStatus);
            PrintStatusResults(bReturn,
                               breturned,
                               (PSCSI_PASS_THROUGH_WITH_BUFFERS)&sptwb,
                               length);
       }
       if (0 != sptwb.spt.ScsiStatus)
           return 0;
    } else  {
            _tprintf(_T("IOCTL to issue SCSI-3 REGISTER and Ignore existing key command is failed with status : %d"), GetLastError());
            GetLastErrToStr(GetLastError());
            return 0;
    }

    return 1;
}
/*
Code to find whether the disk is writable or not; This function detemines whether the initiator has write access or not. 
If the initiator has write access, probably this node holds Persistent Reservations.
*/
int IsDiskWritable(HANDLE hFile)
{
  BOOL bret;
  DWORD BytesRet = 0;

  bret = DeviceIoControl( (HANDLE) hFile, IOCTL_DISK_IS_WRITABLE, NULL, 0, NULL, 0, (LPDWORD) &BytesRet, NULL);  
  if (NULL != bret) {
      _tprintf(_T(" Disk is Writable\n"));   
      return 1;
  } else {
       if (ERROR_WRITE_PROTECT == GetLastError())
          _tprintf(_T(" Disk is read-only; probably the disk is reserved by some other node. Status : %d \n"), GetLastError());
       else
           _tprintf(_T(" Disk is not writable; win32 error status: %d \n"), GetLastError());
       GetLastErrToStr(GetLastError());
      return 0;
  }
  return 1;
}
VOID PrintDataBuffer(PUCHAR DataBuffer, ULONG BufferLength)
{
    ULONG Cnt;

    _tprintf(_T("      00  01  02  03  04  05  06  07   08  09  0A  0B  0C  0D  0E  0F\n"));
    _tprintf(_T("      ---------------------------------------------------------------\n"));
    for (Cnt = 0; Cnt < BufferLength; Cnt++) {
       if ((Cnt) % 16 == 0) {
          _tprintf(_T(" %03X  "),Cnt);
          }
       _tprintf(_T("%02X  "), DataBuffer[Cnt]);
       if ((Cnt+1) % 8 == 0) {
          _tprintf(_T(" "));
          }
       if ((Cnt+1) % 16 == 0) {
          _tprintf(_T("\n"));
          }
       }
    _tprintf(_T("\n\n"));
}

VOID PrintError(ULONG ErrorCode)
{
    UCHAR errorBuffer[80];
    ULONG count;

    count = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM,
                  NULL,
                  ErrorCode,
                  0,
                  (LPSTR)errorBuffer,
                  sizeof(errorBuffer),
                  NULL
                  );

    if (count != 0) {
        _tprintf(_T("%s\n"), errorBuffer);
    } else {
        _tprintf(_T("Format message failed.  Error: %d\n"), GetLastError());
    }
}

VOID PrintSenseInfo(PSCSI_PASS_THROUGH_WITH_BUFFERS psptwb)
{
    int i;

    _tprintf(_T("Scsi status: %02Xh\n\n"),psptwb->spt.ScsiStatus);
    if (psptwb->spt.SenseInfoLength == 0) {
       return;
       }
    _tprintf(_T("Sense Info -- consult SCSI spec for details\n"));
    _tprintf(_T("-------------------------------------------------------------\n"));
    for (i=0; i < psptwb->spt.SenseInfoLength; i++) {
       _tprintf(_T("%02X "),psptwb->ucSenseBuf[i]);
       }
    _tprintf(_T("\n\n"));
}

VOID PrintStatusResults( BOOL status,DWORD returned,PSCSI_PASS_THROUGH_WITH_BUFFERS psptwb, ULONG length)
{
    ULONG errorCode;

    if (!status ) {
       _tprintf(_T( "Error: %d  "),
           errorCode = GetLastError() );
       PrintError(errorCode);
       return;
       }
    if (psptwb->spt.ScsiStatus) {
       PrintSenseInfo(psptwb);
       return;
       }
    else {
       _tprintf(_T(" Scsi status: %02Xh, Bytes returned: %Xh, "),
          psptwb->spt.ScsiStatus,returned);
       _tprintf(_T(" Data buffer length: %Xh\n\n\n"),
          psptwb->spt.DataTransferLength);
       PrintDataBuffer((PUCHAR)psptwb,length);
       }
}

VOID GetLastErrToStr(DWORD err)
{
    LPTSTR Error = 0;
    if(::FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                        NULL,
                        err,
                        0,
                        (LPTSTR)&Error,
                        0,
                        NULL) == 0)
    {
       _tprintf(_T("Conversion is failed\n"));
    }
    _tprintf(_T(" \n Error String : %S \n"), Error);

    // Free the buffer.
    if( Error )
    {
       ::LocalFree( Error );
       Error = 0;
    }
    return;
}
