//-----------------------------------------------------------------------
// <copyright file="IoControlCodes.cs" company="Microsoft">
//     Copyright (c) Microsoft Corporation. All rights reserved.
// </copyright>
//-----------------------------------------------------------------------
namespace Microsoft.Test.IOCTLs
{
    using System;
    using System.Runtime.InteropServices;
    using System.Threading;

    using Microsoft.Win32.SafeHandles;

    /// <summary>  
    /// IO Control Codes as defined in winioctl.h  
    /// </summary>  
    public static class IoControlCodes
    {
        /// <summary>  
        /// Method code defining that I/O buffers will be passed buffered to I/O and FS calls.  
        /// </summary>  
        public const UInt32 METHOD_BUFFERED = 0x00;

        /// <summary>  
        /// Method code defining that input buffers will be passed direct to I/O and FS calls.  
        /// </summary>  
        public const UInt32 METHOD_IN_DIRECT = 0x01;

        /// <summary>  
        /// Method code defining that output buffers will be passed direct to I/O and FS calls.  
        /// </summary>  
        public const UInt32 METHOD_OUT_DIRECT = 0x02;

        /// <summary>  
        /// Method code defining that I/O buffers will be passed direct to I/O and FS calls.  
        /// </summary>  
        public const UInt32 METHOD_NEITHER = 0x03;

        /// <summary>  
        /// Constant defining any access to file or device.  
        /// </summary>  
        public const UInt32 FILE_ANY_ACCESS = 0x00;

        /// <summary>  
        /// Constant defining read access to file or device.  
        /// </summary>  
        public const UInt32 FILE_READ_ACCESS = 0x01;

        /// <summary>  
        /// Constant defining write access to file or device.  
        /// </summary>  
        public const UInt32 FILE_WRITE_ACCESS = 0x02;

        /// <summary>  
        /// Base code for disk IOCTls.  
        /// </summary>  
        public const UInt32 IOCTL_DISK_BASE = FILE_DEVICE_DISK;

        /// <summary>  
        /// Base code for SCSI IOCTLs.  
        /// </summary>  
        public const UInt32 IOCTL_SCSI_BASE = 0x00000004;

        /// <summary>  
        /// Base code for storage IOCTLs.  
        /// </summary>  
        public const UInt32 IOCTL_STORAGE_BASE = 0x0000002d;

        /// <summary>  
        /// Base code for CD-ROM IOCTLs.  
        /// </summary>  
        public const UInt32 IOCTL_CDROM_BASE = 0x00000002;

        /// <summary>  
        /// Base code for volume IOCTLs.  
        /// </summary>  
        public const UInt32 IOCTL_VOLUME_BASE = (UInt32)'V';

        /// <summary>  
        /// Base code for file system IOCTLs.  
        /// </summary>  
        public const UInt32 FILE_DEVICE_FILE_SYSTEM = 0x00000009;

        /// <summary>  
        /// File device type for disk IOCTLs.  
        /// </summary>  
        public const UInt32 FILE_DEVICE_DISK = 0x00000007;

        /// <summary>  
        /// File device type for SCSI IOCTLs.  
        /// </summary>  
        public const UInt32 FILE_DEVICE_SCSI = 0x0000001b;

        /// <summary>  
        /// File device type for InMage IOCTLs.  
        /// </summary>  
        public const UInt32 FILE_DEVICE_INMAGE = 0x00008001;

        /// <summary>  
        /// Function to construct an IoControlCode.  
        /// </summary>  
        /// <param name="DeviceType">Base type of IOCTL.</param>  
        /// <param name="Function">Function number.</param>  
        /// <param name="Method">Method to use for passing buffers.</param>  
        /// <param name="Access">Access required.</param>  
        /// <returns>IoControl Code</returns>  
        public static UInt32 CTL_CODE(UInt32 DeviceType, UInt32 Function, UInt32 Method, UInt32 Access)
        {
            return ((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method);
        }

        #region InMage IOCTLS

        /// <summary>  
        /// To register S2 with driver
        /// </summary>
        public static readonly UInt32 IOCTL_INMAGE_PROCESS_START_NOTIFY           =  CTL_CODE( FILE_DEVICE_INMAGE, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS );

        /// <summary>  
        /// To register svagents with driver
        /// </summary>  
        public static readonly UInt32 IOCTL_INMAGE_SERVICE_SHUTDOWN_NOTIFY        =  CTL_CODE( FILE_DEVICE_INMAGE, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS );

        /// <summary>  
        /// This IOCTL can be sent on control device object name InMageFilter or can be sent on Volume Object.
        /// When sent on Volume device the bitmap to be cleared is implicit on the volume device the IOCTL is sent the bitmap is cleared
        /// When sent on Control device the bitmap volumes GUID has to be sent in InputBuffer.
        /// GUID format is a2345678-b234-c234-d234-e23456789012
        /// </summary> 
        public static readonly UInt32 IOCTL_INMAGE_CLEAR_DIFFERENTIALS            =  CTL_CODE( FILE_DEVICE_INMAGE, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS );

        /// <summary>  
        /// This IOCTL can be sent on control device object name InMageFilter or can be sent on Volume Object.
        /// When sent on Volume device, the device to stop filtering is implicit and the device on which the IOCTL 
        /// is received is stop filtering
        /// When sent on Control device the volumes GUID has to be sent in InputBuffer.
        /// GUID format is a2345678-b234-c234-d234-e23456789012
        /// This IOCTL is considered as nop if IOCTL Is stop filtering and volume is already stopped from filtering
        /// Similarly for Start filtering if volume is actively fitlering START filtering is considered as nop.
        /// </summary>
        public static readonly UInt32 IOCTL_INMAGE_STOP_FILTERING_DEVICE          = CTL_CODE( FILE_DEVICE_INMAGE, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS );

        /// <summary>  
        /// To start tracking the changes
        /// </summary>
        public static readonly UInt32 IOCTL_INMAGE_START_FILTERING_DEVICE         =  CTL_CODE( FILE_DEVICE_INMAGE, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS );

        /// <summary>  
        /// To get current dirty block
        /// </summary>
        public static readonly UInt32 IOCTL_INMAGE_GET_DIRTY_BLOCKS_TRANS         =  CTL_CODE( FILE_DEVICE_INMAGE, 0x806, METHOD_BUFFERED, FILE_ANY_ACCESS );

        /// <summary>  
        /// To commit pending dirty block
        /// </summary> 
        public static readonly UInt32 IOCTL_INMAGE_COMMIT_DIRTY_BLOCKS_TRANS      =  CTL_CODE( FILE_DEVICE_INMAGE, 0x807, METHOD_BUFFERED, FILE_ANY_ACCESS );

        /// <summary>  
        /// On initialization S2 would register an event with driver for each volume. 
        /// S2 would wait on this event to check if there is any data available or not. 
        /// Driver signal this event if there is data available. In case of Linux separate IOCTL would be issued for the same.
        /// </summary>
        public static readonly UInt32 IOCTL_INMAGE_SET_DIRTY_BLOCK_NOTIFY_EVENT   =  CTL_CODE( FILE_DEVICE_INMAGE, 0x80F, METHOD_BUFFERED, FILE_ANY_ACCESS );

        /// <summary>
        /// To get current driver mode
        /// </summary>
        public static readonly UInt32 IOCTL_INMAGE_GET_DATA_MODE_INFO             =  CTL_CODE( FILE_DEVICE_INMAGE, 0x817, METHOD_BUFFERED, FILE_ANY_ACCESS );

        /// <summary>
        /// This IOCTL is used to notify resync start on a disk. It is issued by DP when manual/auto resync request comes. 
        /// Resync Start Notify IOCTL would be issued multiple times if DP fails to communicate Resync Start timestamp and Sequence No to CX.  
        /// Target would need to ignore the files generated before Resync start time.  
        /// Driver closes the current DB upon receiving Resync Start Notify IOCTL. 
        /// If not closed, the dirty block can be spawned across Resync start time.
        /// </summary>
        public static readonly UInt32 IOCTL_INMAGE_RESYNC_START_NOTIFICATION      =  CTL_CODE( FILE_DEVICE_INMAGE, 0x818, METHOD_BUFFERED, FILE_ANY_ACCESS );

        /// <summary>
        /// This IOCTL is used to notify resync end on a disk. 
        /// After syncing both source and target volumes, DP issues Resync End Notify IOCTL to the filter driver. 
        /// Pair now moves from Resync Step I to Resync Step II.  Dataprotection at target uses resync start time to differentiate changes generated before resync and while pair is in resync. 
        /// It deletes all the files whose time stamps are lesser than resync start time stamp. 
        /// And Resync End time stamp would be used to move the pair from resync step II to Differential Sync.
        /// </summary>
        public static readonly UInt32 IOCTL_INMAGE_RESYNC_END_NOTIFICATION        =  CTL_CODE( FILE_DEVICE_INMAGE, 0x819, METHOD_BUFFERED, FILE_ANY_ACCESS );

        /// <summary>  
        /// To get driver version
        /// </summary>  
        public static readonly UInt32 IOCTL_INMAGE_GET_VERSION = CTL_CODE(FILE_DEVICE_INMAGE, 0x820, METHOD_BUFFERED, FILE_ANY_ACCESS);
        
        /// <summary>  
        /// Notify the driver about the current state of the system.
        /// V1 - If the bitmap files are encrypted on the disk.
        /// </summary>  
        public static readonly UInt32 IOCTL_INMAGE_NOTIFY_SYSTEM_STATE = CTL_CODE(FILE_DEVICE_INMAGE, 0x839, METHOD_BUFFERED, FILE_ANY_ACCESS);

        #endregion InMage IOCTLS

    }
}
