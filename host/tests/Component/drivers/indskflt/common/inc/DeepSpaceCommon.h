/*++

Copyright (c) Microsoft Corporation

Module Name:

    DeepSpaceCommon.h

Abstract:

    ** DO NOT INCLUDE THIS FILE DIRECTLY. USE "DeepSpaceUm.h" OR "DeepSpaceKm.h" INSTEAD **

    DeepSpace is an upper disk filter driver intended to be used in the Spaces tests.

    DeepSpace maintains, for each disk, a set of "action requests" which determine how
    the driver will handle I/O and flush IRPs. Each action request consists of the following
    information:
      - Irp type: read, write, flush or DSM. The kind of IRP with which this action request
        is concerned.
        If the irp type is read, write or DSM, the action request also contains:
          - Extent type: physical or space. DeepSpace can filter I/Os based on physical disk
            offsets or space offsets (the second option is not implemented).
          - Offset and length. Define the region of the disk or space with which this action request is concerned.
        If the irp type is read or write, the action request also contains:
          - Irp tag: If this parameter is zero, the action request will intercept IRPs with any tag.
      - Action: hold, complete, monitor, trace, truncate, tripsense or corrupt.
        If action = hold, IRPs matching the action request criteria will be pended. This type of action request takes
        two additional parameters:
          - Number of IRPs to hold: the maximum number of IRPs to hold for this request.
          - Start releasing when queue is full: This dictates how the driver will handle new IRPs after the number of
            blocked IRPs reaches the maximum. If this parameter is true, the oldest blocked IRP will be released when 
            a new one arrives; if it is false, new IRPs will not be blocked.
        If action = complete, IRPs matching the action request criteria will be completed. This type of action request 
        also takes as parameter the completion status code.
        If action = monitor, the driver will count the number of IRPs matching the action request criteria, as well as 
        record the offset and timestamp of the first and last matching IRP, if it is an I/O request.
        If action = trace, the driver will register information from each matching IRP in a internal buffer, which can 
        be retrieved later.
        If action = truncate and irp type = write, the driver will decrease the length of any write IRP matching the
        action request criteria before forwaring the request. Action truncate is not supported for 'read' and 'flush' requests.
        If action = corrupt and irp type = write, the driver will corrupt the user buffer of any write IRP matching the
        action request criteria before forwaring the request. Action corrupt is not supported for 'read' and 'flush' requests.
        If action = tripSense, the driver will will complete matching IRPs, but will set the SRB information for that IRP for
        smart sense checks. This action takes three additional parameters:
          - Sense Code Key to set in IRP.
          - Additional Sense Code Key to set in IRP.
          - Additonal Sense Code Key qualifier to set in IRP.

        IOCTLs
        ======

        IOCTL_DEEPSPACE_DETECT_DRIVER 
            Always returns STATUS_SUCCESS
            Input: none
            Output: none

        IOCTL_DEEPSPACE_ADD_ACTION_REQUEST: 
            Adds an action request to the disk.
            Input: a variable of type AddActionRequestInput containing the IRP filtering criteria and the action details.
            Output: none

        IOCTL_DEEPSPACE_REMOVE_ACTION_REQUEST: 
            Removes an action request from the disk. If it is a hold action, any blocked IRP will be released.
            Input: a variable of type IrpFilter.
            Output: none

        IOCTL_DEEPSPACE_GET_ACTION_STATUS:
            Gets the status from action requests of type hold and monitor. For hold requests, it returns the number of
            blocked IRPs; for monitor requests, it returns the number of observed IRPs and the offset and timestamp of 
            the first and the last observed IRP.
            Input: a variable of type IrpFilter.
            Output: a variable of type ActionRequestStatus.

        IOCTL_DEEPSPACE_RELEASE_IRPS:
            Releases a number of IRPs blocked by an action request of type hold.
            Input: a variable of type ReleaseIrpsInput containing the IRP filtering criteria and the number of IRPs to release.
            Output: none

        IOCTL_DEEPSPACE_CLEANUP_REQUESTS:
            Removes all action requests from the disk and releases any blocked IRP.
            Input: none
            Output: none

Environment:

    Kernel mode

Revision History:

--*/

#pragma once
#include <ntstatus.h>

#define IOCTL_DEEPSPACE_BASE                             ((ULONG)0x0000DEEB)
#define IOCTL_DEEPSPACE_DETECT_DRIVER                    CTL_CODE(IOCTL_DEEPSPACE_BASE, 0x0000, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_DEEPSPACE_ADD_ACTION_REQUEST               CTL_CODE(IOCTL_DEEPSPACE_BASE, 0x0001, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_DEEPSPACE_REMOVE_ACTION_REQUEST            CTL_CODE(IOCTL_DEEPSPACE_BASE, 0x0002, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_DEEPSPACE_GET_ACTION_STATUS                CTL_CODE(IOCTL_DEEPSPACE_BASE, 0x0003, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_DEEPSPACE_RELEASE_IRPS                     CTL_CODE(IOCTL_DEEPSPACE_BASE, 0x0004, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_DEEPSPACE_CLEANUP_REQUESTS                 CTL_CODE(IOCTL_DEEPSPACE_BASE, 0x0005, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_DEEPSPACE_FLUSH_DISK                       CTL_CODE(IOCTL_DEEPSPACE_BASE, 0x0006, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_DEEPSPACE_ADD_PERSISTENT_ACTION_REQUEST    CTL_CODE(IOCTL_DEEPSPACE_BASE, 0x0007, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_DEEPSPACE_CLEANUP_PERSISTENT_REQUESTS      CTL_CODE(IOCTL_DEEPSPACE_BASE, 0x0008, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_DEEPSPACE_LOAD_PERSISTENT_REQUESTS         CTL_CODE(IOCTL_DEEPSPACE_BASE, 0x0009, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_DEEPSPACE_WRITE                            CTL_CODE(IOCTL_DEEPSPACE_BASE, 0x000A, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_DEEPSPACE_READ_COPY                        CTL_CODE(IOCTL_DEEPSPACE_BASE, 0x000C, METHOD_BUFFERED, FILE_READ_ACCESS)

#define IOCTL_DEEPSPACE_QUERY_NUMBER_ACTION_REQUESTS     CTL_CODE(IOCTL_DEEPSPACE_BASE, 0x000D, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_DEEPSPACE_QUERY_ACTION_REQUESTS            CTL_CODE(IOCTL_DEEPSPACE_BASE, 0x000E, METHOD_BUFFERED, FILE_READ_ACCESS)


#define DEEPSPACE_MEMTAG 'PSPD' // DPSP

namespace DeepSpace {

    //
    // Enum definitions
    //
    enum EXTENT_TYPE {
        EXTENT_TYPE_PHYSICAL,
        EXTENT_TYPE_SPACE,
        _EXTENT_TYPE_MAX
    };

    enum IRP_TYPE {
        IRP_TYPE_READ,
        IRP_TYPE_WRITE,
        IRP_TYPE_FLUSH,
        IRP_TYPE_DSM,
        IRP_TYPE_PNP,
		IRP_TYPE_IOCTL,
        _IRP_TYPE_MAX
    };

    enum ACTION {
        ACTION_MONITOR,
        ACTION_TRACE,
        ACTION_HOLD,
        ACTION_COMPLETE,
        ACTION_TRUNCATE,
        ACTION_CORRUPT,
	    ACTION_REDIRECT,
        ACTION_TRIPSENSE,
        _ACTION_MAX
    };

    //
    // Input types
    //
    struct IrpFilter {
        IRP_TYPE irpType;          //  4 bytes
        EXTENT_TYPE extentType;    //  4 bytes
        ULONGLONG offset;          //  8 bytes
        ULONGLONG length;          //  8 bytes
        ULONG column;              //  4 bytes
        ULONG copy;                //  4 bytes
        ULONGLONG operationBitSet; //  8 bytes
        ULONGLONG stateBitSet;     //  8 bytes
        ULONGLONG dsmActionBitSet; //  8 bytes
        GUID spaceId;              // 16 bytes
        ULONG owner;               //  4 bytes
		ULONG ioCtlCode;		   //  4 bytes
        ACTION action;             //  4 bytes
    };

    struct ActionParameters {
        union {
            struct {
                ULONG numIrpsToHold;                             // 4 bytes
                BOOLEAN startReleasingWhenQueueIsFull;           // 1 byte
            } Hold;
            struct {
                NTSTATUS completionStatus;                       // 4 bytes
            } Complete;
            struct {
                ULONG fragmentNumerator;                         // 4 bytes
                ULONG fragmentDenominator;                       // 4 bytes
            } Truncate;
            struct {
                ULONG corruptionData;                            // 4 bytes
            } Corrupt;
            struct {
                ULONG column;                                    // 4 bytes
            } Redirect;
            struct {
                UCHAR dataSummaryLength;                         // 1 bytes
            } Trace;
            struct {
                UCHAR SenseKey;                                  // 1 bytes
                UCHAR AdditionalSenseCode;                       // 1 bytes
                UCHAR AdditionalSenseCodeQualifier;              // 1 bytes
            } SenseCode;
            CHAR _padding0[16];                                  // 16 bytes
        } Parameters;
    };

    struct AddActionRequestInput {
        IrpFilter irpFilter;
        ActionParameters actionParameters;
    };

    struct AddPersistentActionRequestInput {
        IrpFilter irpFilter;
        ActionParameters actionParameters;
        BOOLEAN addAfterDriverReinitialization;
    };

    struct ReleaseIrpsInput {
        IrpFilter irpFilter;
        ULONG numIrpsToRelase;
        ULONG _padding0;
    };

    struct WriteInput {
        ULONGLONG offset;
        ULONG length;
        CHAR data[ANYSIZE_ARRAY];
    };

    struct ReadCopyInput {
        ULONGLONG offset;
        ULONG length;
        ULONG copyNumber;
    };

    //
    // Output types
    //

    struct QueryActionRequestOutput {
        IrpFilter irpFilter;
        ActionParameters actionParameters;
    };

    struct ActionRequestStatus {
        ULONG irpCount;         // Number of IRPs detected. Applicable to requests with action = ACTION_HOLD or ACTION_MONITOR.
        ULONG _padding0;
        // The fields below are applicable to requests with action = ACTION_MONITOR and irpType = IRP_TYPE_READ or IRP_TYPE_WRITE
        ULONGLONG firstOffset; // Offset of the first observed I/O. 
        ULONGLONG lastOffset; // Offset of the last observed I/O.
        // The fields below are applicable to requests with action = ACTION_MONITOR and any irpType.
        ULONGLONG firstTimestamp; // Timestamp of the first observed IRP, as a count of 100-nanosecond intervals since January 1, 1601
        ULONGLONG lastTimestamp; // Timestamp of the last observed IRP, as a count of 100-nanosecond intervals since January 1, 1601
        // The fields below are applicable to requests with action = ACTION_MONITOR and irpType = IRP_TYPE_READ, IRP_TYPE_WRITE or IRP_TYPE_DSM
        ULONGLONG amountOfData;  // Number of bytes written to, read from or otherwise affected by the intercepted IRPs
    };

#define DEEPSPACE_ROOT_CONTROL_DEVICE_NAME        L"\\Device\\DeepSpaceControl"
#define DEEPSPACE_ROOT_CONTROL_SYMBOLIC_LINK_NAME L"\\DosDevices\\DeepSpaceControl"

#define GET_BITSET_FROM_VALUE(X) (((ULONGLONG)1)<<(X))
#define EMPTY_BITSET 0
#define COMPLETE_BITSET (MAXULONGLONG)

#define DEFAULT_FILTER_OWNER   0
#define AUTOTRACE_FILTER_OWNER 0xE0000000
#define ANY_FILTER_OWNER       MAXULONG
}
