//All common functions, error codes, structures for this API here.

#pragma once
#ifndef COMMON__H
#define COMMON__H

//#include <iostream>

#include <sstream>
#include <fstream>
#include <string>
#include <vector>
#include <map>

#include <ace/OS.h>
#include <ace/OS_main.h>
#include <ace/Task.h>
#include <ace/Guard_T.h>
#include <ace/Mutex.h>
#include <ace/Process.h>
#include <ace/Process_Manager.h>

#include <boost/thread.hpp>

#include "dlmapi.h"
#include "portablehelpers.h"
#include "localconfigurator.h"
#include "logger.h"
#include "inm_md5.h"

#ifdef SV_WINDOWS
#include <diskguid.h>
#endif

/*********************************************************************************
MBR BOOT SECTOR INFO (512 bytes : 0 to 511)

Disk Signature : (440-443, 4 bytes)

Partition Table Entries (446-509 , 64 bytes) 
------------------------------------------------------------------------------
Partition   Range(16)   PartitionType(1)    StartingSector(4)   NoOfSectors(4)
------------------------------------------------------------------------------
    1       446-461     450                 454-457             458-461
    2       462-477     466                 470-473             474-477
    3       478-493     482                 486-489             490-493
    4       494-509     498                 502-505             506-509
------------------------------------------------------------------------------
Note: Since '4' bytes is common size for many attributes, define a common constant(say magic count)

MBR Signature : 0xAA55 (510-511, 2 bytes) - in MBR it will be as 55 AA (little endian)
*********************************************************************************/

#define MBR_MAGIC_COUNT                             4
#define MBR_DISK_SIGNATURE_BEGIN                    440
#define MBR_PARTITION_RECORD_SIZE                   16
#define MBR_PARTITION_1_PARTITIONTYPE               450
#define MBR_PARTITION_1_STARTINGSECTOR              454
#define MBR_PARTITION_1_NOOFSECTORS                 458
#define EBR_SECOND_ENTRY_STARTING_INDEX             470     //easy to get while fetching EBR
#define GPT_PARTITION_1_STARTINGSECTOR              1024

#define MBR_SIGNATURE_FIRST_BYTE                    0xAA    
#define MBR_SIGNATURE_SECOND_BYTE                   0x55
#define GPT_PARTITION_TYPE                          0xEE    
#define EXTENDED_PARTITION_TYPE1                    0x05
#define EXTENDED_PARTITION_TYPE2                    0x0F
#define MBR_DYNAMIC_DISK                            0x42

struct GPT_GUID {
    SV_ULONG  Data1;
    SV_USHORT Data2;
    SV_USHORT Data3;
    SV_UCHAR  Data4[8];
};

#ifndef SV_WINDOWS
// Logical Disk Manager (LDM) metadata partition on a dynamic disk for GPT disk.
const GPT_GUID PARTITION_LDM_METADATA_GUID = { 0x5808C8AA,
0x7E8F,
0x42E0,
0x85, 0xD2, 0xE1, 0xE9, 0x04, 0x34, 0xCF, 0xB3 };
#endif

#endif  /* COMMON__H */
