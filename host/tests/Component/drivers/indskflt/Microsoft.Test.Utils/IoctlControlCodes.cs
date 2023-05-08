#region Assembly Microsoft.Test.Utils.dll, v1.0.0.0
// C:\work\inmage-pilot\inmage-sources\tests\Component\drivers\indskflt\windows\ManagedStorage\bin\Debug\Microsoft.Test.Utils.dll
#endregion

using System;

namespace Microsoft.Test.Utils.NativeMacros
{
    public static class IoControlCodes
    {
        public const uint FILE_ANY_ACCESS = 0;
        public const uint FILE_DEVICE_DISK = 7;
        public const uint FILE_DEVICE_FILE_SYSTEM = 9;
        public const uint FILE_DEVICE_SCSI = 27;
        public const uint FILE_READ_ACCESS = 1;
        public const uint FILE_WRITE_ACCESS = 2;
        public const uint IOCTL_CDROM_BASE = 2;
        public const uint IOCTL_DISK_BASE = 7;
        public const uint IOCTL_SCSI_BASE = 4;
        public const uint IOCTL_STORAGE_BASE = 45;
        public const uint IOCTL_VOLUME_BASE = 86;
        public const uint METHOD_BUFFERED = 0;
        public const uint METHOD_IN_DIRECT = 1;
        public const uint METHOD_NEITHER = 3;
        public const uint METHOD_OUT_DIRECT = 2;

        /// <summary>              
        /// Gets the partition layout of a disk.              
        /// </summary>  
        public static readonly UInt32 IOCTL_DISK_GET_DRIVE_LAYOUT_EX = CTL_CODE(IOCTL_DISK_BASE, 0x0014, METHOD_BUFFERED, FILE_ANY_ACCESS);

        /// <summary>              
        /// Sets the partition layout of a disk.              
        /// </summary>              
        public static readonly UInt32 IOCTL_DISK_SET_DRIVE_LAYOUT_EX = CTL_CODE(IOCTL_DISK_BASE, 0x0015, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS);

        /// <summary>              
        /// Deletes the partition layout of a disk.              
        /// </summary>              
        public static readonly UInt32 IOCTL_DISK_DELETE_DRIVE_LAYOUT = CTL_CODE(IOCTL_DISK_BASE, 0x0040, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS);

        /// <summary>              
        /// Gets the information about a partition.              
        /// </summary>              
        public static readonly UInt32 IOCTL_DISK_GET_PARTITION_INFO_EX = CTL_CODE(IOCTL_DISK_BASE, 0x0012, METHOD_BUFFERED, FILE_ANY_ACCESS);

        /// <summary>              
        /// Gets the drive geometry of a disk or removable media device.              
        /// </summary>              
        public static readonly UInt32 IOCTL_DISK_GET_DRIVE_GEOMETRY = CTL_CODE(IOCTL_DISK_BASE, 0x0000, METHOD_BUFFERED, FILE_ANY_ACCESS);

        /// <summary>              
        /// Queries a storage device or adapter.              
        /// </summary>              
        public static readonly UInt32 IOCTL_STORAGE_QUERY_PROPERTY = CTL_CODE(IOCTL_STORAGE_BASE, 0x0500, METHOD_BUFFERED, FILE_ANY_ACCESS);

        /// <summary>              
        /// Gets a physical disk mapping from a logical offset.              
        /// </summary>              
        public static readonly UInt32 IOCTL_VOLUME_LOGICAL_TO_PHYSICAL = CTL_CODE(IOCTL_VOLUME_BASE, 0x0008, METHOD_BUFFERED, FILE_ANY_ACCESS);

        /// <summary>              
        /// Gets address information, such as the target ID (TID) and the logical unit number (LUN) of a particular SCSI target.              
        /// </summary>              
        public static readonly UInt32 IOCTL_SCSI_GET_ADDRESS = CTL_CODE(IOCTL_SCSI_BASE, 0x0406, METHOD_BUFFERED, FILE_ANY_ACCESS);

        /// <summary>              
        /// Allows an application to get the attributes for a given disk.              
        /// </summary>              
        public static readonly UInt32 IOCTL_DISK_GET_DISK_ATTRIBUTES = CTL_CODE(IOCTL_DISK_BASE, 0x003c, METHOD_BUFFERED, FILE_ANY_ACCESS);

        /// <summary>              
        /// Allows an application to set the attributes for a given disk.              
        /// </summary>              
        public static readonly UInt32 IOCTL_DISK_SET_DISK_ATTRIBUTES = CTL_CODE(IOCTL_DISK_BASE, 0x003d, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS);  

        public static readonly UInt32 IOCTL_DISK_CREATE_DISK = CTL_CODE(IOCTL_DISK_BASE, 0x0016, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS);

        public static UInt32 CTL_CODE(UInt32 DeviceType, UInt32 Function, UInt32 Method, UInt32 Access)
        {
            return ((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method);
        }  

    }
}
