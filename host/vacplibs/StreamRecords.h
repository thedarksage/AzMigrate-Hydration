#ifndef _STREAM_RECORDS__H
#define _STREAM_RECORDS__H

//#include "hostagenthelpers_ported.h"
#include "svtypes.h"

//TODO
#ifndef ASSERT
#define ASSERT
#endif 

#define STREAM_REC_TYPE_START_OF_TAG_LIST   0x0001
#define STREAM_REC_TYPE_END_OF_TAG_LIST     0x0002
#define STREAM_REC_TYPE_TIME_STAMP_TAG      0x0003
#define STREAM_REC_TYPE_SVD_HEADER_TAG		0x0004
#define STREAM_REC_TYPE_DRTD_DATA_TAG		0x0005

/********************************************************************************/
#ifndef INVOFLT_STREAM_FUNCTIONS
#define INVOFLT_STREAM_FUNCTIONS
typedef struct _STREAM_REC_HDR_4B
{
    SV_USHORT      usStreamRecType;
    SV_UCHAR       ucFlags;
    SV_UCHAR       ucLength;
} STREAM_REC_HDR_4B, *PSTREAM_REC_HDR_4B;

typedef struct _STREAM_REC_HDR_8B
{
    SV_USHORT      usStreamRecType;
    SV_UCHAR       ucFlags;    // STREAM_REC_FLAGS_LENGTH_BIT bit is set for this record.
    SV_UCHAR       ucReserved;
    SV_ULONG       ulLength;
} STREAM_REC_HDR_8B, *PSTREAM_REC_HDR_8B;


#define FILL_STREAM_HEADER_4B(pHeader, Type, Len)           \
{                                                           \
    ASSERT((SV_ULONG)Len < (SV_ULONG)0xFF);                       \
    ((PSTREAM_REC_HDR_4B)pHeader)->usStreamRecType = Type;  \
    ((PSTREAM_REC_HDR_4B)pHeader)->ucFlags = 0;             \
    ((PSTREAM_REC_HDR_4B)pHeader)->ucLength = Len;          \
}



#define FILL_STREAM_HEADER_8B(pHeader, Type, Len)           \
{                                                           \
    ASSERT((SV_ULONG)Len > (SV_ULONG)0xFF);                       \
    ((PSTREAM_REC_HDR_8B)pHeader)->usStreamRecType = Type;  \
    ((PSTREAM_REC_HDR_8B)pHeader)->ucFlags = STREAM_REC_FLAGS_LENGTH_BIT;             \
    ((PSTREAM_REC_HDR_8B)pHeader)->ucReserved = 0;          \
    ((PSTREAM_REC_HDR_8B)pHeader)->ulLength = Len;          \
}




#define STREAM_REC_FLAGS_LENGTH_BIT         0x01



#define GET_STREAM_LENGTH(pHeader)                              \
    ( (((PSTREAM_REC_HDR_4B)pHeader)->ucFlags & STREAM_REC_FLAGS_LENGTH_BIT) ?      \
                (((PSTREAM_REC_HDR_8B)pHeader)->ulLength) :                         \
                (((PSTREAM_REC_HDR_4B)pHeader)->ucLength))
#endif
/**************************************************************************************/
// Stream Record Tags for ApplicationConsistency
// are define below

//Oracle
#define STREAM_REC_TYPE_ORACLE_TAG			0x0100
//SystemFiles
#define STREAM_REC_TYPE_SYSTEMFILES_TAG		0x0101
//Registry
#define STREAM_REC_TYPE_REGISTRY_TAG		0x0102
//SQL
#define STREAM_REC_TYPE_SQL_TAG				0x0103
//EventLog
#define STREAM_REC_TYPE_EVENTLOG_TAG		0x0104
//WMI
#define STREAM_REC_TYPE_WMI_DATA_TAG		0x0105
//COM+REGDB
#define STREAM_REC_TYPE_COM_REGDB_TAG		0x0106
//FS
#define STREAM_REC_TYPE_FS_TAG				0x0107
//USERDEFINED
#define STREAM_REC_TYPE_USERDEFINED_EVENT	0x0108
//EXCHANGE
#define STREAM_REC_TYPE_EXCHANGE_TAG		0x0109
//IISMETABASE
#define STREAM_REC_TYPE_IISMETABASE_TAG		0x010A
//CA - Certificate Authority
#define STREAM_REC_TYPE_CA_TAG				0x010B
//AD - Acitve Directory
#define STREAM_REC_TYPE_AD_TAG				0x010C
//DHCP
#define STREAM_REC_TYPE_DHCP_TAG			0x010D
//BITS
#define STREAM_REC_TYPE_BITS_TAG			0x010E
//WINS Jet Writer 
#define STREAM_REC_TYPE_WINS_TAG			0x010F
//Removable Storage Manager 
#define STREAM_REC_TYPE_RSM_TAG				0x0110
//TermServLicensing
#define STREAM_REC_TYPE_TSL_TAG				0x0111
//FRS
#define STREAM_REC_TYPE_FRS_TAG				0x0112
//BootableState (Win XP)
#define STREAM_REC_TYPE_BS_TAG              0x0113
//Mircrosoft ServiceState (Win XP)
#define STREAM_REC_TYPE_SS_TAG              0x0114
//Cluster Service
#define STREAM_REC_TYPE_CLUSTER_TAG         0x0115
//SQL Server 2005
#define STREAM_REC_TYPE_SQL2005_TAG         0x0116
//Exchange 2007- Exchange Information Store  
#define STREAM_REC_TYPE_EXCHANGEIS_TAG      0x0117
//Exchange 2007- Exchange Replication Service 
#define STREAM_REC_TYPE_EXCHANGEREPL_TAG    0x0118
//SQL Server 2008
#define STREAM_REC_TYPE_SQL2008_TAG         0x0119
//SharePoint 
#define STREAM_REC_TYPE_SHAREPOINT_TAG		0x011A
//Office Search
#define STREAM_REC_TYPE_OSEARCH_TAG			0x011B
//SharePoint Search
#define STREAM_REC_TYPE_SPSEARCH_TAG		0x011C
//Hyper-V 
#define STREAM_REC_TYPE_HYPERV_TAG			0x011D
//Automated System Recovery
#define STREAM_REC_TYPE_ASR_TAG				0x011E
//Shadow Copy Optimzation
#define STREAM_REC_TYPE_SC_OPTIMIZATION_TAG	0x011F
//MS Search 
#define STREAM_REC_TYPE_MSSEARCH_TAG		0x0120
//Task Scheduler Writer
#define STREAM_REC_TYPE_TASK_SCHEDULER_TAG			0x0121
//VSS Express Writer Metadata Store Writer
#define STREAM_REC_TYPE_VSS_METADATA_STORE_TAG		0x0122
//Performance Counters Writer
#define STREAM_REC_TYPE_PERFORMANCE_COUNTER_TAG		0x0123
//IIS Config Writer
#define STREAM_REC_TYPE_IIS_CONFIG_TAG				0x0124
//FSRM Writer
#define STREAM_REC_TYPE_FSRM_TAG					0x0125
//ADAM(instanceN) Writer
#define STREAM_REC_TYPE_ADAM_TAG					0x0126
//Cluster Database Writer
#define STREAM_REC_TYPE_CLUSTER_DB_TAG				0x0127
//Network Policy Server (NPS) VSS Writer
#define STREAM_REC_TYPE_NPS_TAG						0x0128
//TS Gateway Writer
#define STREAM_REC_TYPE_TSG_TAG						0x0129
//Distributed File System Replication 
#define STREAM_REC_TYPE_DFSR_TAG					0x012A
//VMWare Server
#define STREAM_REC_TYPE_VMWARESRV_TAG					0x012B
//SQL Server 2012
#define STREAM_REC_TYPE_SQL2012_TAG         0x012C
// Crash Consistency
#define STREAM_REC_TYPE_CRASH_TAG           0x012D

#define STREAM_REC_TYPE_HYDRATION           0x012E
#define STREAM_REC_TYPE_BASELINE_TAG        0x012F

// Cluster Info Tag
// This tag captures the nodes part of shared disk cluster. 
#define STREAM_REC_TYPE_CLUSTER_INFO_TAG        0x0130

//Revocation Tag - A special tag to revoke/delete/cancel already issued tags 
#define STREAM_REC_TYPE_REVOCATION_TAG 		0x01FF

//TAG GUID, a common unique tag value across multiple volumes for recovery
#define STREAM_REC_TYPE_TAGGUID_TAG		0x02FF

//TAG lifetime, bookmark is dropped from retention when lifetime expires
#define STREAM_REC_TYPE_LIFETIME		0x03FF

//System Level Tag
#define STREAM_REC_TYPE_SYSTEMLEVEL		0x04FF

//Dynamically discovered new VSS Writer
#define STREAM_REC_TYPE_DYNMICALLY_DISCOVERED_NEW_VSSWRITER		0x5FF



#define INM_BOOKMARK_LIFETIME_NOTSPECIFIED 0
#define INM_BOOKMARK_LIFETIME_FOREVER      (unsigned long long)((~(unsigned long long)0) >> 1)

typedef struct _STREAM_GENERIC_REC_HDR
{
	SV_USHORT      usStreamRecType;
    SV_UCHAR       ucFlags;
	SV_UCHAR		ucReserved;
}STREAM_GENERIC_REC_HDR, *PSTREAM_GENERIC_REC_HDR;

#define GET_STREAM_HEADER_LENGTH(pHeader)                              \
    ( (((PSTREAM_REC_HDR_4B)pHeader)->ucFlags & STREAM_REC_FLAGS_LENGTH_BIT) ?      \
                (sizeof(STREAM_REC_HDR_8B)) :                         \
                (sizeof(STREAM_REC_HDR_4B)))

#define CLEAR_STREAM_REC_FLAGS_BIT(pHeader, BitPosition)						\
	(((PSTREAM_REC_HDR_4B)pHeader)->ucFlags &= ~BitPosition)

#endif /* _STREAM_RECORDS_H */

