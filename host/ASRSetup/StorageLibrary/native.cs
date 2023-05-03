using System;
using System.IO;
using System.ComponentModel;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;

using NativeFramework;

// We don't want to redocument the entire Win32 API.
#pragma warning disable 1591

namespace StorageDeviceLibrary
{
    /// <summary>
    /// Class containing useful constants defined in the windows SDK headers
    /// </summary>
    public static partial class NativeConstants
    {
        #region File Access and Attribute constants
        /// <summary>
        /// Constant as defined in winnt.h
        /// </summary>
        public const UInt32 GENERIC_READ = 0x80000000;
        /// <summary>
        /// Constant as defined in winnt.h
        /// </summary>
        public const UInt32 GENERIC_WRITE = 0x40000000;
        /// <summary>
        /// Constant as defined in winnt.h
        /// </summary>
        public const UInt32 GENERIC_EXECUTE = 0x20000000;
        /// <summary>
        /// Constant as defined in winnt.h
        /// </summary>
        public const UInt32 GENERIC_ALL = 0x10000000;
        /// <summary>
        /// Constant as defined in winnt.h
        /// </summary>
        public const UInt32 FILE_FLAG_OVERLAPPED = 0x40000000;
        /// <summary>
        /// Constant as defined in winnt.h
        /// </summary>
        public const UInt32 FILE_FLAG_WRITE_THROUGH = 0x80000000;
        /// <summary>
        /// Constant as defined in winnt.h
        /// </summary>
        public const UInt32 FILE_FLAG_NO_BUFFERING = 0x20000000;
        /// <summary>
        /// Constant as defined in winnt.h
        /// </summary>
        public const UInt32 FILE_FLAG_RANDOM_ACCESS = 0x10000000;
        #endregion File Access and Attribute constants
        #region Security constants
        internal const UInt32 STANDARD_RIGHTS_REQUIRED = 0x000F0000;
        internal const UInt32 TOKEN_ASSIGN_PRIMARY = 0x0001;
        internal const UInt32 TOKEN_DUPLICATE = 0x0002;
        internal const UInt32 TOKEN_IMPERSONATE = 0x0004;
        internal const UInt32 TOKEN_QUERY = 0x0008;
        internal const UInt32 TOKEN_QUERY_SOURCE = 0x0010;
        internal const UInt32 TOKEN_ADJUST_PRIVILEGES = 0x0020;
        internal const UInt32 TOKEN_ADJUST_GROUPS = 0x0040;
        internal const UInt32 TOKEN_ADJUST_DEFAULT = 0x0080;
        internal const UInt32 TOKEN_ADJUST_SESSIONID = 0x0100;
        internal const UInt32 TOKEN_ALL_ACCESS  = STANDARD_RIGHTS_REQUIRED |
            TOKEN_ASSIGN_PRIMARY      |
            TOKEN_DUPLICATE           |
            TOKEN_IMPERSONATE         |
            TOKEN_QUERY               |
            TOKEN_QUERY_SOURCE        |
            TOKEN_ADJUST_PRIVILEGES   |
            TOKEN_ADJUST_GROUPS       |
            TOKEN_ADJUST_DEFAULT;
        internal const string SE_MANAGE_VOLUME_NAME = "SeManageVolumePrivilege";
        internal const UInt32 SE_PRIVILEGE_ENABLED = 0x00000002;
        internal const UInt32 SE_PRIVILEGE_REMOVED = 0X00000004;
        #endregion Security constants
        #region DriveType constants
        /// <summary>
        /// The drive type cannot be determined.
        /// </summary>
        public const UInt32 DRIVE_UNKNOWN = 0;
        /// <summary>
        /// The root path is invalid; for example, there is no volume is mounted at the path.
        /// </summary>
        public const UInt32 DRIVE_NO_ROOT_DIR = 1;
        /// <summary>
        /// The drive has removable media; for example, a floppy drive or flash card reader.
        /// </summary>
        public const UInt32 DRIVE_REMOVABLE = 2;
        /// <summary>
        /// The drive has fixed media; for example, a hard drive, flash drive, or thumb drive.
        /// </summary>
        public const UInt32 DRIVE_FIXED = 3;
        /// <summary>
        /// The drive is a remote (network) drive.
        /// </summary>
        public const UInt32 DRIVE_REMOTE = 4;
        /// <summary>
        /// The drive is a CD-ROM drive.
        /// </summary>
        public const UInt32 DRIVE_CDROM = 5;
        /// <summary>
        /// The drive is a RAM disk.
        /// </summary>
        public const UInt32 DRIVE_RAMDISK = 6;
        #endregion DriveType constants
        #region Setup API Constants
        /// <summary>DeviceDesc (R/W)</summary>
        public const UInt32 SPDRP_DEVICEDESC = 0x00000000;
        /// <summary>HardwareID (R/W)</summary>
        public const UInt32 SPDRP_HARDWAREID = 0x00000001;
        /// <summary>CompatibleIDs (R/W)</summary>
        public const UInt32 SPDRP_COMPATIBLEIDS = 0x00000002;
        /// <summary>unused</summary>
        public const UInt32 SPDRP_UNUSED0 = 0x00000003;
        /// <summary>Service (R/W)</summary>
        public const UInt32 SPDRP_SERVICE = 0x00000004;
        /// <summary>unused</summary>
        public const UInt32 SPDRP_UNUSED1 = 0x00000005;
        /// <summary>unused</summary>
        public const UInt32 SPDRP_UNUSED2 = 0x00000006;
        /// <summary>Class (R--tied to ClassGUID)</summary>
        public const UInt32 SPDRP_CLASS = 0x00000007;
        /// <summary>ClassGUID (R/W)</summary>
        public const UInt32 SPDRP_CLASSGUID = 0x00000008;
        /// <summary>Driver (R/W)</summary>
        public const UInt32 SPDRP_DRIVER = 0x00000009;
        /// <summary>ConfigFlags (R/W)</summary>
        public const UInt32 SPDRP_CONFIGFLAGS = 0x0000000A;
        /// <summary>Mfg (R/W)</summary>
        public const UInt32 SPDRP_MFG = 0x0000000B;
        /// <summary>FriendlyName (R/W)</summary>
        public const UInt32 SPDRP_FRIENDLYNAME = 0x0000000C;
        /// <summary>LocationInformation (R/W)</summary>
        public const UInt32 SPDRP_LOCATION_INFORMATION = 0x0000000D;
        /// <summary>PhysicalDeviceObjectName (R)</summary>
        public const UInt32 SPDRP_PHYSICAL_DEVICE_OBJECT_NAME = 0x0000000E;
        /// <summary>Capabilities (R)</summary>
        public const UInt32 SPDRP_CAPABILITIES = 0x0000000F;
        /// <summary>UiNumber (R)</summary>
        public const UInt32 SPDRP_UI_NUMBER = 0x00000010;
        /// <summary>UpperFilters (R/W)</summary>
        public const UInt32 SPDRP_UPPERFILTERS = 0x00000011;
        /// <summary>LowerFilters (R/W)</summary>
        public const UInt32 SPDRP_LOWERFILTERS = 0x00000012;
        /// <summary>BusTypeGUID (R)</summary>
        public const UInt32 SPDRP_BUSTYPEGUID = 0x00000013;
        /// <summary>LegacyBusType (R)</summary>
        public const UInt32 SPDRP_LEGACYBUSTYPE = 0x00000014;
        /// <summary>BusNumber (R)</summary>
        public const UInt32 SPDRP_BUSNUMBER = 0x00000015;
        /// <summary>Enumerator Name (R)</summary>
        public const UInt32 SPDRP_ENUMERATOR_NAME = 0x00000016;
        /// <summary>Security (R/W, binary form)</summary>
        public const UInt32 SPDRP_SECURITY = 0x00000017;
        /// <summary>Security (W, SDS form)</summary>
        public const UInt32 SPDRP_SECURITY_SDS = 0x00000018;
        /// <summary>Device Type (R/W)</summary>
        public const UInt32 SPDRP_DEVTYPE = 0x00000019;
        /// <summary>Device is exclusive-access (R/W)</summary>
        public const UInt32 SPDRP_EXCLUSIVE = 0x0000001A;
        /// <summary>Device Characteristics (R/W)</summary>
        public const UInt32 SPDRP_CHARACTERISTICS = 0x0000001B;
        /// <summary>Device Address (R)</summary>
        public const UInt32 SPDRP_ADDRESS = 0x0000001C;
        /// <summary>UiNumberDescFormat (R/W)</summary>
        public const UInt32 SPDRP_UI_NUMBER_DESC_FORMAT = 0X0000001D;
        /// <summary>Device Power Data (R)</summary>
        public const UInt32 SPDRP_DEVICE_POWER_DATA = 0x0000001E;
        /// <summary>Removal Policy (R)</summary>
        public const UInt32 SPDRP_REMOVAL_POLICY = 0x0000001F;
        /// <summary>Hardware Removal Policy (R)</summary>
        public const UInt32 SPDRP_REMOVAL_POLICY_HW_DEFAULT = 0x00000020;
        /// <summary>Removal Policy Override (RW)</summary>
        public const UInt32 SPDRP_REMOVAL_POLICY_OVERRIDE = 0x00000021;
        /// <summary>Device Install State (R)</summary>
        public const UInt32 SPDRP_INSTALL_STATE = 0x00000022;
        /// <summary>Device Location Paths (R)</summary>
        public const UInt32 SPDRP_LOCATION_PATHS = 0x00000023;
        /// <summary>Upper bound on ordinals</summary>
        public const UInt32 SPDRP_MAXIMUM_PROPERTY = 0x00000024;
        #endregion Setup API Constants
        #region Error Mode constants
        /// <summary>
        /// The system does not display the critical-error-handler message box.
        /// Instead, the system sends the error to the calling process.
        /// </summary>
        public const UInt32 SEM_FAILCRITICALERRORS = 0x0001;
        /// <summary>
        /// The system automatically fixes memory alignment faults and makes them
        /// invisible to the application. It does this for the calling process and
        /// any descendant processes. This feature is only supported by certain
        /// processor architectures.
        /// </summary>
        public const UInt32 SEM_NOALIGNMENTFAULTEXCEPT = 0x0004;
        /// <summary>
        /// The system does not display the general-protection-fault message box.
        /// This flag should only be set by debugging applications that handle
        /// general protection (GP) faults themselves with an exception handler.
        /// </summary>
        public const UInt32 SEM_NOGPFAULTERRORBOX = 0x0002;
        /// <summary>
        /// The system does not display a message box when it fails to find a file.
        /// Instead, the error is returned to the calling process.
        /// </summary>
        public const UInt32 SEM_NOOPENFILEERRORBOX = 0x8000;
        #endregion Error Mode constants
        #region Partition Type Constants
        /// <summary>
        /// Partition Entry unused
        /// </summary>
        public static Guid PARTITION_ENTRY_UNUSED_GUID  = new Guid(0x00000000, 0x0000, 0x0000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);
        /// <summary>
        /// EFI system partition
        /// </summary>
        public static Guid PARTITION_SYSTEM_GUID = new Guid(0xC12A7328, 0xF81F, 0x11D2, 0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B);
        /// <summary>
        /// Microsoft reserved space
        /// </summary>
        public static Guid PARTITION_MSFT_RESERVED_GUID = new Guid(0xE3C9E316, 0x0B5C, 0x4DB8, 0x81, 0x7D, 0xF9, 0x2D, 0xF0, 0x02, 0x15, 0xAE);
        /// <summary>
        /// Basic data partition
        /// </summary>
        public static Guid PARTITION_BASIC_DATA_GUID = new Guid(0xEBD0A0A2, 0xB9E5, 0x4433, 0x87, 0xC0, 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7);
        /// <summary>
        /// Logical Disk Manager metadata partition
        /// </summary>
        public static Guid PARTITION_LDM_METADATA_GUID = new Guid(0x5808C8AA, 0x7E8F, 0x42E0, 0x85, 0xD2, 0xE1, 0xE9, 0x04, 0x34, 0xCF, 0xB3);
        /// <summary>
        /// Logical Disk Manager data partition
        /// </summary>
        public static Guid PARTITION_LDM_DATA_GUID = new Guid(0xAF9B60A0, 0x1431, 0x4F62, 0xBC, 0x68, 0x33, 0x11, 0x71, 0x4A, 0x69, 0xAD);
        /// <summary>
        /// Microsoft recovery partition
        /// </summary>
        public static Guid PARTITION_MSFT_RECOVERY_GUID = new Guid(0xDE94BBA4, 0x06D1, 0x4D40, 0xA1, 0x6A, 0xBF, 0xD5, 0x01, 0x79, 0xD6, 0xAC);
        /// <summary>
        /// Partition Entry unused
        /// </summary>
        public const byte PARTITION_ENTRY_UNUSED        = 0x00;
        /// <summary>
        /// 12-bit FAT partition entries
        /// </summary>
        public const byte PARTITION_FAT_12              = 0x01;
        /// <summary>
        /// Xenix partition entry
        /// </summary>
        public const byte PARTITION_XENIX_1             = 0x02;
        /// <summary>
        /// Xenix partition entry
        /// </summary>
        public const byte PARTITION_XENIX_2             = 0x03;
        /// <summary>
        /// 16-bit FAT entries
        /// </summary>
        public const byte PARTITION_FAT_16              = 0x04;
        /// <summary>
        /// Extended partition entry
        /// </summary>
        public const byte PARTITION_EXTENDED            = 0x05;
        /// <summary>
        /// Huge partition MS-DOS V4
        /// </summary>
        public const byte PARTITION_HUGE                = 0x06;
        /// <summary>
        /// IFS Partition
        /// </summary>
        public const byte PARTITION_IFS                 = 0x07;
        /// <summary>
        /// OS/2 Boot Manager/OPUS/Coherent swap
        /// </summary>
        public const byte PARTITION_OS2BOOTMGR          = 0x0A;
        /// <summary>
        /// FAT32 partition
        /// </summary>
        public const byte PARTITION_FAT32               = 0x0B;
        /// <summary>
        /// FAT32 using extended int13 services
        /// </summary>
        public const byte PARTITION_FAT32_XINT13        = 0x0C;
        /// <summary>
        /// Win95 partition using extended int13 services
        /// </summary>
        public const byte PARTITION_XINT13              = 0x0E;
        /// <summary>
        /// Same as type 5 but uses extended int13 services
        /// </summary>
        public const byte PARTITION_XINT13_EXTENDED     = 0x0F;
        /// <summary>
        /// PowerPC Reference Platform (PReP) Boot Partition
        /// </summary>
        public const byte PARTITION_PREP                = 0x41;
        /// <summary>
        /// Logical Disk Manager partition
        /// </summary>
        public const byte PARTITION_LDM                 = 0x42;
        /// <summary>
        /// Unix partition
        /// </summary>
        public const byte PARTITION_UNIX                = 0x63;
        /// <summary>
        /// NTFT uses high order bits
        /// </summary>
        public const byte VALID_NTFT                    = 0xC0;
        /// <summary>
        /// The high bit of the partition type code indicates that a partition
        /// is part of an NTFT mirror or striped array.
        /// </summary>
        public const byte PARTITION_NTFT                = 0x80;
        /// <summary>
        /// EFI System Partition
        /// </summary>
        public const byte PARTITION_ESP                 = 0xEF;
        #endregion Partition Type Constants

        #region REG Constants
        /// <summary>
        ///  Unicode nul terminated string
        /// </summary>
        public const UInt32 REG_SZ                      = 1;
        /// <summary>
        ///  Unicode nul terminated string
        /// </summary>
        public const UInt32 REG_EXPAND_SZ               = 2;
        /// <summary>
        /// 32-bit number (same as REG_DWORD) 
        /// </summary>
        public const UInt32 REG_DWORD                   = 4;
        /// <summary>
        /// // 32-bit number 
        /// </summary>   
        public const UInt32 REG_DWORD_LITTLE_ENDIAN     = 4;
        /// <summary>
        ///  32-bit number
        /// </summary>
        public const UInt32 REG_DWORD_BIG_ENDIAN        = 5;
        #endregion REG constants        
        
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
            /// <summary>
            /// Gets the device number, partition number, and device type of a storage device.
            /// </summary>
            public static readonly UInt32 IOCTL_STORAGE_GET_DEVICE_NUMBER = CTL_CODE(IOCTL_STORAGE_BASE, 0x0420, METHOD_BUFFERED, FILE_ANY_ACCESS);
            /// <summary>
            /// Gets the disk extents of a volume.
            /// </summary>
            public static readonly UInt32 IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS = CTL_CODE(IOCTL_VOLUME_BASE, 0, METHOD_BUFFERED, FILE_ANY_ACCESS);
            /// <summary>
            /// Locks a volume.
            /// </summary>
            public static readonly UInt32 FSCTL_LOCK_VOLUME = CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 6, METHOD_BUFFERED, FILE_ANY_ACCESS);
            /// <summary>
            /// Unlocks a volume.
            /// </summary>
            public static readonly UInt32 FSCTL_UNLOCK_VOLUME = CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 7, METHOD_BUFFERED, FILE_ANY_ACCESS);
            /// <summary>
            /// Dismounts a volume.
            /// </summary>
            public static readonly UInt32 FSCTL_DISMOUNT_VOLUME = CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 8, METHOD_BUFFERED, FILE_ANY_ACCESS);
            /// <summary>
            /// Gets the VCNs for a file.
            /// </summary>
            public static readonly UInt32 FSCTL_GET_RETRIEVAL_POINTERS = CTL_CODE(FILE_DEVICE_FILE_SYSTEM, 28, METHOD_NEITHER, FILE_ANY_ACCESS);
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
            /// Sets the spindle speed of the CD-ROM drive.
            /// </summary>
            public static readonly UInt32 IOCTL_CDROM_SET_SPEED = CTL_CODE(IOCTL_CDROM_BASE, 0x0018, METHOD_BUFFERED, FILE_READ_ACCESS);
            /// <summary>
            /// Enables streaming mode of operation in CD-ROM class driver.
            /// </summary>
            public static readonly UInt32 IOCTL_CDROM_ENABLE_STREAMING = CTL_CODE(IOCTL_CDROM_BASE, 0x001A, METHOD_BUFFERED, FILE_READ_ACCESS);
            /// <summary>
            /// Allows an application to send ATA command to a target device
            /// </summary>
            public static readonly UInt32 IOCTL_ATA_PASS_THROUGH = CTL_CODE(IOCTL_SCSI_BASE, 0x040b, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS);
            /// <summary>
            /// Allows an application to get the attributes for a given disk.
            /// </summary>
            public static readonly UInt32 IOCTL_DISK_GET_DISK_ATTRIBUTES = CTL_CODE(IOCTL_DISK_BASE, 0x003c, METHOD_BUFFERED, FILE_ANY_ACCESS);
            /// <summary>
            /// Allows an application to set the attributes for a given disk.
            /// </summary>
            public static readonly UInt32 IOCTL_DISK_SET_DISK_ATTRIBUTES = CTL_CODE(IOCTL_DISK_BASE, 0x003d, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS);
            /// <summary>
            /// Allows an application to create a diff VHD on top of an online VHD.
            /// </summary>
            public static readonly UInt32 IOCTL_STORAGE_FORK_VIRTUAL_DISK = CTL_CODE(IOCTL_STORAGE_BASE, 0x0655, METHOD_BUFFERED, FILE_ANY_ACCESS);
        }
    }

    namespace NativeStructures {

        /// <summary>
        /// OS Structure as defined in winioctl.h
        /// </summary>
        [StructLayout(LayoutKind.Sequential, Pack=0)]
        public struct STORAGE_DEVICE_NUMBER
        {
            /// <summary>
            /// Specifies one of the system-defined FILE_DEVICE_XXX constants
            /// indicating the type of device (such as FILE_DEVICE_DISK, FILE_DEVICE_KEYBOARD,
            /// and so forth) or a vendor-defined value for a new type of device.
            /// </summary>
            public Int32 DeviceType;
            /// <summary>
            /// Indicates the number of this device.
            /// </summary>
            public Int32 DeviceNumber;
            /// <summary>
            /// Indicates the partition number of the device is returned in this member,
            /// if the device can be partitioned. Otherwise, -1 is returned.
            /// </summary>
            public Int32 PartitionNumber;
            /// <summary>
            /// Retruns a string representation of this structure.
            /// </summary>
            /// <returns>String representation of this structure</returns>
            public override string ToString()
            {
                return String.Format("Type:{0} Number:{1} Partition:{2}",
                    DeviceType, DeviceNumber, PartitionNumber);
            }
        }

        /// <summary>
        /// OS Enumeration as defined in winioctl.h
        /// </summary>
        public enum PARTITION_STYLE
        {
            /// <summary>
            /// Master Boot Record partition.
            /// </summary>
            PARTITION_STYLE_MBR,
            /// <summary>
            /// GUID Partition Table partition.
            /// </summary>
            PARTITION_STYLE_GPT,
            /// <summary>
            /// RAW partition.
            /// </summary>
            PARTITION_STYLE_RAW,
        }
        /// <summary>
        /// OS Structure as defined in winioctl.h
        /// </summary>
        [StructLayout(LayoutKind.Explicit)]
        public struct PARTITION_INFORMATION_EX
        {
            /// <summary>
            /// Format of the partition.
            /// </summary>
            [FieldOffset(0)]
            public PARTITION_STYLE PartitionStyle;
            /// <summary>
            /// Offset in bytes of the beginning of the partition on the physical disk.
            /// </summary>
            [FieldOffset(8)]
            public UInt64 StartingOffset;
            /// <summary>
            /// Length of the partition in bytes.
            /// </summary>
            [FieldOffset(16)]
            public UInt64 PartitionLength;
            /// <summary>
            /// One-based number of the partition.
            /// </summary>
            [FieldOffset(24)]
            public UInt32 PartitionNumber;
            /// <summary>
            /// Flag used to tell the consumer of the structure to rewrite the partition.
            /// </summary>
            [FieldOffset(28)]
            public bool RewritePartition;
            /// <summary>
            /// If the PartitionStyle is <see cref="PARTITION_STYLE.PARTITION_STYLE_MBR"/>
            /// this contains information about an MBR partition.  Otherwise this element should not be accessed.
            /// </summary>
            [FieldOffset(32)]
            public PARTITION_INFORMATION_MBR Mbr;
            /// <summary>
            /// If the PartitionStyle is <see cref="PARTITION_STYLE.PARTITION_STYLE_GPT"/>
            /// this contains information about an GPT partition.  Otherwise this element should not be accessed.
            /// </summary>
            [FieldOffset(32)]
            public PARTITION_INFORMATION_GPT Gpt;
            /// <summary>
            /// Retruns a string representation of this structure.
            /// </summary>
            /// <returns>String representation of this structure</returns>
            public override string ToString()
            {
                string result = String.Format("Partition#:{0} Offset:{1}\nLength:{2} Style:{3}\n",
                    PartitionNumber, StartingOffset, PartitionLength, PartitionStyle);
                if (PartitionStyle == PARTITION_STYLE.PARTITION_STYLE_MBR)
                    result += Mbr;
                else if (PartitionStyle == PARTITION_STYLE.PARTITION_STYLE_GPT)
                    result += Gpt;
                else
                    result += "RAW";
                return result;
            }
        }
        /// <summary>
        /// OS Structure as defined in winioctl.h
        /// </summary>
        [StructLayout(LayoutKind.Explicit)]
        public struct PARTITION_INFORMATION_MBR
        {
            /// <summary>
            /// Type of partition. For a list of values, see <see href="http://msdn.microsoft.com/library/en-us/fileio/fs/disk_partition_types.asp"/>.
            /// </summary>
            [FieldOffset(0)]
            public byte PartitionType;
            /// <summary>
            /// Identifies the partition as the Active boot partition.
            /// </summary>
            [FieldOffset(1)]
            public bool BootIndicator;
            /// <summary>
            /// Identifies that the partition type is recognized.
            /// </summary>
            [FieldOffset(2)]
            public bool RecognizedPartition;
            /// <summary>
            /// Number of hidden sectors allocated on this partition
            /// </summary>
            [FieldOffset(4)]
            public UInt32 HiddenSectors;
            /// <summary>
            /// Retruns a string representation of this structure.
            /// </summary>
            /// <returns>String representation of this structure</returns>
            public override string ToString()
            {
                return String.Format("Type:0x{0:X2} Active:{1} Recognized:{2}\nHidden Sectors:{3}",
                    PartitionType, BootIndicator, RecognizedPartition, HiddenSectors);
            }
        }
        /// <summary>
        /// OS Structure as defined in winioctl.h
        /// </summary>
        /// <seealso href="http://msdn.microsoft.com/library/default.asp?url=/library/en-us/fileio/fs/disk_extent_str.asp"/>
        [StructLayout(LayoutKind.Explicit, CharSet = CharSet.Unicode)]
        public struct PARTITION_INFORMATION_GPT
        {
            /// <summary>
            /// The unique partition type GUID of a partition, as defined in the EFI specification.
            /// </summary>
            [FieldOffset(0)]
            public Guid PartitionType;
            /// <summary>
            /// The unique GUID assigned to this partition.
            /// </summary>
            [FieldOffset(16)]
            public Guid PartitionId;
            /// <summary>
            /// The EFI attributes of the partition.
            /// </summary>
            [FieldOffset(32)]
            public UInt64 Attributes;
            /// <summary>
            /// Name of the partition.
            /// </summary>
            [FieldOffset(40)]
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 36)]
            public string Name;
            /// <summary>
            /// Retruns a string representation of this structure.
            /// </summary>
            /// <returns>String representation of this structure</returns>
            public override string ToString()
            {
                return String.Format("Type:{0}\nID:{1}\nAttributes:0x{2:X16} Name:{3}",
                    PartitionType, PartitionId, Attributes, Name);
            }

        }
        /// <summary>
        /// OS Structure as defined in winioctl.h
        /// </summary>
        [StructLayout(LayoutKind.Explicit)]
        public struct DRIVE_LAYOUT_INFORMATION_EX
        {
            /// <summary>
            /// Format of the partitions on the disk.
            /// </summary>
            [FieldOffset(0)]
            public PARTITION_STYLE PartitionStyle;
            /// <summary>
            /// Number of partitions on the disk.
            /// </summary>
            [FieldOffset(4)]
            public UInt32 PartitionCount;
            /// <summary>
            /// If the PartitionStyle is <see cref="PARTITION_STYLE.PARTITION_STYLE_MBR"/>
            /// this contains information about an MBR drive layout.  Otherwise this element should not be accessed.
            /// </summary>
            [FieldOffset(8)]
            public DRIVE_LAYOUT_INFORMATION_MBR Mbr;
            /// <summary>
            /// If the PartitionStyle is <see cref="PARTITION_STYLE.PARTITION_STYLE_GPT"/>
            /// this contains information about an GPT drive layout.  Otherwise this element should not be accessed.
            /// </summary>
            [FieldOffset(8)]
            public DRIVE_LAYOUT_INFORMATION_GPT Gpt;
            /// <summary>
            /// An array of <see cref="PARTITION_INFORMATION_EX"/> with information about each partition.
            /// </summary>
            [FieldOffset(48)]
            public PARTITION_INFORMATION_EX[] PartitionEntry;
            /// <summary>
            /// Retruns a string representation of this structure.
            /// </summary>
            /// <returns>String representation of this structure</returns>
            public override string ToString()
            {
                string result = String.Format("Style:{0} \n", PartitionStyle);
                if (PartitionStyle == PARTITION_STYLE.PARTITION_STYLE_MBR)
                    result += Mbr;
                else if (PartitionStyle == PARTITION_STYLE.PARTITION_STYLE_GPT)
                    result += Gpt;
                else
                    result += "RAW";
                if (PartitionEntry != null)
                {
                    for (int entry = 0; (entry < PartitionEntry.Length); entry++)
                        result += String.Format("\nPartition[{0}] {1}", entry, PartitionEntry[entry]);
                }
                return result;
            }
        }
        /// <summary>
        /// OS Structure as defined in winioctl.h
        /// </summary>
        [StructLayout(LayoutKind.Sequential)]
        public struct DRIVE_LAYOUT_INFORMATION_GPT
        {
            /// <summary>
            /// Unique GUID of this disk.
            /// </summary>
            public Guid DiskId;
            /// <summary>
            /// Starting byte of the first usable block on the disk.
            /// </summary>
            public UInt64 StartingUsableOffset;
            /// <summary>
            /// Number of usable blocks on the disk.
            /// </summary>
            public UInt64 UsableLength;
            /// <summary>
            /// Number of partitions that can be created on the disk.
            /// </summary>
            public UInt32 MaxPartitionCount;
            /// <summary>
            /// Retruns a string representation of this structure.
            /// </summary>
            /// <returns>String representation of this structure</returns>
            public override string ToString()
            {
                return String.Format("ID:{0} Starting Usable Offset:{1}\nUsable Length:{2} Max Partition Count:{3}",
                    DiskId, StartingUsableOffset, UsableLength, MaxPartitionCount);
            }
        }
        /// <summary>
        /// OS Structure as defined in winioctl.h
        /// </summary>
        [StructLayout(LayoutKind.Sequential)]
        public struct DRIVE_LAYOUT_INFORMATION_MBR
        {
            /// <summary>
            /// Signature of the drive.
            /// </summary>
            public UInt32 Signature;
            /// <summary>
            /// Retruns a string representation of this structure.
            /// </summary>
            /// <returns>String representation of this structure</returns>
            public override string ToString()
            {
                return String.Format("Signature:0x{0:X8}", Signature);
            }
        }
        /// <summary>
        /// OS Structure as defined in winioctl.h
        /// </summary>
        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
        public struct STORAGE_PROPERTY_QUERY
        {
            /// <summary>
            /// Property to query.
            /// </summary>
            public STORAGE_PROPERTY_ID PropertyId;
            /// <summary>
            /// Type of query to issue.
            /// </summary>
            public STORAGE_QUERY_TYPE QueryType;

            /// <summary>
            /// Field for query specific input parameters.
            /// </summary>
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 1)]
            public byte[] AdditionalParameters;
        }
        /// <summary>
        /// OS Enumeration as defined in winioctl.h
        /// </summary>
        public enum STORAGE_QUERY_TYPE
        {
            /// <summary>
            /// Instructs the port driver to report a device descriptor, an adapter descriptor or a unique hardware device ID (DUID).
            /// </summary>
            PropertyStandardQuery = 0,
            /// <summary>
            /// Instructs the port driver to report whether the descriptor is supported.
            /// </summary>
            PropertyExistsQuery,
            /// <summary>
            /// Not supported.
            /// </summary>
            PropertyMaskQuery,
            /// <summary>
            /// Upper limit of query types, do not use.
            /// </summary>
            PropertyQueryMaxDefined
        }
        /// <summary>
        /// Contains various attributes for a disk object.
        /// </summary>
        [Flags]
        public enum
        DiskAttributes : ulong {
            /// <summary>
            /// Indicates that the disk is offline.
            /// </summary>
            Offline     = 0x1,
            /// <summary>
            /// Indicates that the disk is read-only.
            /// </summary>
            ReadOnly    = 0x2
        }
        /// <summary>
        /// OS Enumeration as defined in winioctl.h
        /// </summary>
        public enum STORAGE_PROPERTY_ID
        {
            /// <summary>
            /// Indicates that the caller is querying for the device descriptor.
            /// <see cref="STORAGE_DEVICE_DESCRIPTOR"/>
            /// </summary>
            StorageDeviceProperty = 0,
            /// <summary>
            /// Indicates that the caller is querying for the adapter descriptor.
            /// <see cref="STORAGE_ADAPTER_DESCRIPTOR"/>
            /// </summary>
            StorageAdapterProperty,
            /// <summary>
            /// Indicates that the caller is querying for the device identifiers
            /// provided with the SCSI vital product data pages.
            /// <see cref="STORAGE_DEVICE_ID_DESCRIPTOR"/>
            /// </summary>
            StorageDeviceIdProperty,
            /// <summary>
            /// Indicates that the caller is querying for the unique device identifiers.
            /// The returned stucture is currently unimplemented.  This provides the same
            /// information as issuing a DeviceProperty request and a DeviceId request.
            /// </summary>
            StorageDeviceUniqueIdProperty,
            /// <summary>
            /// Indicates that the caller is querying for the write cache property.
            /// <see cref="STORAGE_WRITE_CACHE_PROPERTY"/>
            /// </summary>
            StorageDeviceWriteCacheProperty,
            /// <summary>
            /// Indicates that the caller is querying for the miniport property.
            /// <see cref="STORAGE_MINIPORT_DESCRIPTOR"/>
            /// </summary>
            StorageMiniportProperty,
            /// <summary>
            /// Indicates that the caller is querying for the access alignment property.
            /// <see cref="STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR"/>
            /// </summary>
            StorageAccessAlignmentProperty,
            StorageDeviceSeekPenaltyProperty,
            StorageDeviceTrimProperty,
            StorageDeviceWriteAggregationProperty,
            StorageDeviceDeviceTelemetryProperty,
            StorageDeviceLBProvisioningProperty,
            StorageDeviceZeroPowerODDProperty,
            StorageDeviceCopyOffloadProperty,
            StorageDeviceResiliencyProperty
        }
        /// <summary>
        /// OS Enumeration as defined in winioctl.h
        /// </summary>
        public enum STORAGE_BUS_TYPE
        {
            /// <summary>
            /// Unknown bus type.
            /// </summary>
            BusTypeUnknown = 0x00,
            /// <summary>
            /// Indicates a small computer system interface (SCSI) bus.
            /// </summary>
            BusTypeScsi,
            /// <summary>
            /// Indicates an AT Attachment Packet Interface (ATAPI) bus.
            /// </summary>
            BusTypeAtapi,
            /// <summary>
            /// Indicates an advanced technology attachment (ATA) bus.
            /// </summary>
            BusTypeAta,
            /// <summary>
            /// Indicates an IEEE 1394 bus.
            /// </summary>
            BusType1394,
            /// <summary>
            /// Indicates a serial storage architecture (SSA) bus.
            /// </summary>
            BusTypeSsa,
            /// <summary>
            /// Indicates a fibre channel bus type.
            /// </summary>
            BusTypeFibre,
            /// <summary>
            /// Indicates a USB bus type.
            /// </summary>
            BusTypeUsb,
            /// <summary>
            /// Indicates a bus for a redundant array of inexpensive disks (RAID).
            /// </summary>
            BusTypeRAID,
            /// <summary>
            /// Indicates an iSCSI bus.
            /// </summary>
            BusTypeiScsi,
            /// <summary>
            /// Indicates a serial attached SCSI bus.
            /// </summary>
            BusTypeSas,
            /// <summary>
            /// Indicates a serial ATA bus.
            /// </summary>
            BusTypeSata,
            /// <summary>
            /// Indicates a Secure Digital (SD) bus.
            /// </summary>
            BusTypeSd,
            /// <summary>
            /// Indicates a MultiMediaCard bus.
            /// </summary>
            BusTypeMmc,
            /// <summary>
            /// Maximum bus type limit.
            /// </summary>
            BusTypeMax,
            /// <summary>
            /// Reserved, do not use.
            /// </summary>
            BusTypeMaxReserved = 0x7F
        }
        /// <summary>
        /// Enumeration representing the type of storage miniports.
        /// </summary>
        public enum STORAGE_PORT_CODE_SET
        {
            /// <summary>
            /// Not used.
            /// </summary>
            StoragePortCodeSetReserved = 0,
            /// <summary>
            /// Adapter is a Storport miniport.
            /// </summary>
            StoragePortCodeSetStorport = 1,
            /// <summary>
            /// Adapter is a Scsiport miniport.
            /// </summary>
            StoragePortCodeSetSCSIport = 2
        };
        /// <summary>
        /// OS Structure as defined in winioctl.h
        /// </summary>
        [StructLayout(LayoutKind.Sequential)]
        public struct STORAGE_DESCRIPTOR_HEADER
        {
            /// <summary>
            /// Indicates version of the data reported.
            /// </summary>
            public UInt32 Version;
            /// <summary>
            /// Indicates the quantity of data reported, in bytes, including this header.
            /// </summary>
            public UInt32 Size;
        }


        /// <summary>
        /// OS Structure as defined in winioctl.h
        /// </summary>
        [StructLayout(LayoutKind.Sequential)]
        public struct DEVICE_LB_PROVISIONING_DESCRIPTOR
        {
            /// <summary>
            /// version
            /// </summary>
            public UInt32 Version;
            /// <summary>
            /// size 
            /// </summary>
            public UInt32 Size;
            /// <summary>
            /// configurations
            /// </summary>
            public UInt64 Configuration;
            /// <summary>
            /// OptimalUnmapGranularity
            /// </summary>
            public UInt64 OptimalUnmapGranularity;
            /// <summary>
            /// Retruns a string representation of this structure.
            /// </summary>
            public UInt64 UnmapGranularityAlignment;
        }

        /// <summary>
        /// OS Structure as defined in winioctl.h
        /// </summary>
        [StructLayout(LayoutKind.Sequential)]
        public struct DEVICE_COPY_OFFLOAD_DESCRIPTOR
        {
            /// <summary>
            /// version
            /// </summary>
            public UInt32 Version;
            /// <summary>
            /// size 
            /// </summary>
            public UInt32 Size;
            /// <summary>
            /// MaximumTokenLifetime
            /// </summary>
            public UInt32 MaximumTokenLifetime;
            /// <summary>
            /// DefaultTokenLifetime
            /// </summary>
            public UInt32 DefaultTokenLifetime;
            /// <summary>
            /// MaximumTransferSize
            /// </summary>
            public UInt64 MaximumTransferSize;
            /// <summary>
            /// OptimalTransferCount
            /// </summary>
            public UInt64 OptimalTransferCount;
            /// <summary>
            /// MaximumDataDescriptors 
            /// </summary>
            public UInt32 MaximumDataDescriptors;
            /// <summary>
            /// MaximumTransferLengthPerDescriptor
            /// </summary>
            public UInt32 MaximumTransferLengthPerDescriptor;
            /// <summary>
            /// OptimalTransferLengthPerDescriptor
            /// </summary>
            public UInt32 OptimalTransferLengthPerDescriptor;
            /// <summary>
            /// OptimalTransferLengthGranularity
            /// </summary>
            public UInt16 OptimalTransferLengthGranularity;
            /// <summary>
            /// Reserved
            /// </summary>
            public UInt16 Reserved;
        }

        /// <summary>
        /// The access alignment descriptor provides information about a
        /// device's sector size and sector allignment.
        /// </summary>
        [StructLayout(LayoutKind.Sequential)]
        public struct STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR {
            /// <summary>
            /// Sizeof(STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR)
            /// </summary>
            public UInt32 Version;
            /// <summary>
            /// Total size of the descriptor, including the space for additional
            /// data and id strings
            /// </summary>
            public UInt32 Size;
            /// <summary>
            /// The number of bytes in a cache line of the device
            /// </summary>
            public UInt32 BytesPerCacheLine;
            /// <summary>
            /// The address offset neccessary for proper cache access alignment in bytes
            /// </summary>
            public UInt32 BytesOffsetForCacheAlignment;
            /// <summary>
            /// The number of bytes in a physical sector of the device
            /// </summary>
            public UInt32 BytesPerLogicalSector;
            /// <summary>
            /// The number of bytes in an addressable logical sector (LBA)of the device
            /// </summary>
            public UInt32 BytesPerPhysicalSector;
            /// <summary>
            /// The address offset neccessary for proper sector access alignment in bytes
            /// </summary>
            public UInt32 BytesOffsetForSectorAlignment;
            /// <summary>
            /// Retruns a string representation of this structure.
            /// </summary>
            /// <returns>String representation of this structure</returns>
            public override string ToString()
            {
                string result = String.Format("Size:{0}\nVersion:{1}\n", Size, Version);
                result += String.Format("BytesPerCacheLine: {0} BytesOffsetForCacheAlignment: {1}\n" +
                    "BytesPerLogicalSector: {2} BytesPerPhysicalSector: {3}\n BytesOffsetForSectorAlignment: {4}",
                    BytesPerCacheLine, BytesOffsetForCacheAlignment, BytesPerLogicalSector,
                    BytesPerPhysicalSector, BytesOffsetForSectorAlignment);
                return result;
            }
        }
        /// <summary>
        /// OS Structure as defined in winioctl.h
        /// </summary>
        [StructLayout(LayoutKind.Explicit)]
        public struct STORAGE_ADAPTER_DESCRIPTOR
        {
            /// <summary>
            /// Indicates version of the data reported.
            /// </summary>
            [FieldOffset(0)]
            public UInt32 Version;
            /// <summary>
            /// Indicates the quantity of data reported, in bytes.
            /// </summary>
            [FieldOffset(4)]
            public UInt32 Size;
            /// <summary>
            /// Maximum number of bytes supported by the adapter in a single operation.
            /// </summary>
            [FieldOffset(8)]
            public UInt32 MaximumTransferLength;
            /// <summary>
            /// Specifies the maximum number of discontinuous physical pages the HBA
            /// can manage in a single transfer (in other words, the extent of its scatter/gather support).
            /// </summary>
            [FieldOffset(12)]
            public UInt32 MaximumPhysicalPages;
            /// <summary>
            /// Specifies the HBA's alignment requirements for transfers. A storage class
            /// driver sets the AlignmentRequirement field in its device objects to this
            /// value. The alignment mask indicates alignment restrictions for buffers
            /// required by the HBA for transfer operations. Valid mask values are also
            /// restricted by characteristics of the memory managers on different versions
            /// of Windows. The mask values that are permitted under Windows 95 or
            /// Windows 98 are 0 (byte aligned), 1 (word aligned) or 3 (DWORD aligned).
            /// Under Windows NT and Windows 2000 the valid mask values are 0 (byte aligned),
            /// 1 (word aligned), 3 (DWORD aligned) and 7 (double DWORD aligned).
            /// </summary>
            [FieldOffset(16)]
            public UInt32 AlignmentMask;
            /// <summary>
            /// Indicates when TRUE that the HBA uses PIO and requires the use of system-space
            /// virtual addresses mapped to physical memory for data buffers. When FALSE,
            /// the HBA does not use PIO.
            /// </summary>
            [FieldOffset(20)]
            public bool AdapterUsesPio;
            /// <summary>
            /// Indicates when TRUE that the HBA scans down for BIOS devices, that is,
            /// the HBA begins scanning with the highest device number rather than the lowest.
            /// When FALSE, the HBA begins scanning with the lowest device number. This member
            /// is reserved for legacy miniport drivers.
            /// </summary>
            [FieldOffset(21)]
            public bool AdapterScansDown;
            /// <summary>
            /// Indicates when TRUE that the HBA supports SCSI tagged queuing and/or
            /// per-logical-unit internal queues, or the non-SCSI equivalent. When FALSE,
            /// the HBA neither supports SCSI-tagged queuing nor per-logical-unit internal queues.
            /// </summary>
            [FieldOffset(22)]
            public bool CommandQueueing;
            /// <summary>
            /// Indicates when TRUE that the HBA supports synchronous transfers as a way of
            /// speeding up I/O. When FALSE, the HBA does not support synchronous transfers
            /// as a way of speeding up I/O.
            /// </summary>
            [FieldOffset(23)]
            public bool AcceleratedTransfer;
            [FieldOffset(24)]
            byte BusType_byte;
            /// <summary>
            /// Specifies a value of type STORAGE_BUS_TYPE that indicates the type of the
            /// bus that connects the HBA to devices.
            /// </summary>
            public STORAGE_BUS_TYPE BusType { get { return (STORAGE_BUS_TYPE)BusType_byte; } }
            /// <summary>
            /// Specifies the major version number, if any, of the HBA.
            /// </summary>
            [FieldOffset(26)]
            public UInt16 BusMajorVersion;
            /// <summary>
            /// Specifies the major version number, if any, of the HBA.
            /// </summary>
            [FieldOffset(28)]
            public UInt16 BusMinorVersion;
            /// <summary>
            /// Retruns a string representation of this structure.
            /// </summary>
            /// <returns>String representation of this structure</returns>
            public override string ToString()
            {
                string result = String.Format("Size:{0}\nVersion:{1}\n", Size, Version);
                result += String.Format("Max Transfer Length:{0}\nMax Physical Pages:{1}\n",
                    MaximumTransferLength, MaximumPhysicalPages);
                result += String.Format("Alignment Mask:{0}\nUses PIO:{1}\nScans Down:{2}\n",
                    AlignmentMask, AdapterUsesPio, AdapterScansDown);
                result += String.Format("Accelerated Transfer:{0}\nBus Type:{1}\n",
                    AcceleratedTransfer, BusType);
                result += String.Format("Bus Major Version:{0}\nBus Minor Version:{1}\n",
                    BusMajorVersion, BusMinorVersion);
                return result;
            }
        }
        /// <summary>
        /// OS Structure as defined in winioctl.h
        /// </summary>
        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi,Pack=0)]
        public struct STORAGE_DEVICE_DESCRIPTOR
        {
            /// <summary>
            /// Indicates version of the data reported.
            /// </summary>
            public UInt32 Version;
            /// <summary>
            /// Indicates the quantity of data reported, in bytes.
            /// </summary>
            public UInt32 Size;
            /// <summary>
            /// Specifies the device type as defined by the
            /// Small Computer Systems Interface (SCSI) specification.
            /// </summary>
            public byte DeviceType;
            /// <summary>
            /// Specifies the device type modifier, if any, as defined
            /// by the Small Computer Systems Interface (SCSI) specification.
            /// If no device type modifier exists, this member is zero.
            /// </summary>
            public byte DeviceTypeModifier;
            /// <summary>
            /// Indicates when TRUE that the device's media (if any) is removable.
            /// If the device has no media, this member should be ignored.
            /// When FALSE the device's media is not removable.
            /// </summary>
            [MarshalAs(UnmanagedType.U1, SizeConst = 1)]
            public bool RemovableMedia;
            /// <summary>
            /// Indicates when TRUE that the device supports multiple outstanding
            /// commands (SCSI tagged queuing or equivalent). When FALSE, the device
            /// does not support SCSI-tagged queuing or the equivalent. The STORPORT
            /// driver is responsible for synchronizing the commands.
            /// </summary>
            [MarshalAs(UnmanagedType.U1, SizeConst = 1)]
            public bool CommandQueueing;
            /// <summary>
            /// Specifies the byte offset from the beginning of the structure to a
            /// NULL-terminated ASCII string that contains the device's vendor ID.
            /// If the device has no vendor ID, this member is zero.
            /// </summary>
            public UInt32 VendorIdOffset;
            /// <summary>
            /// Specifies the byte offset from the beginning of the structure to a
            /// NULL-terminated ASCII string that contains the device's product ID.
            /// If the device has no product ID, this member is zero.
            /// </summary>
            public UInt32 ProductIdOffset;
            /// <summary>
            /// Specifies the byte offset from the beginning of the structure to a
            /// NULL-terminated ASCII string that contains the device's product
            /// revision string. If the device has no product revision string,
            /// this member is zero.
            /// </summary>
            public UInt32 ProductRevisionOffset;
            /// <summary>
            /// Specifies the byte offset from the beginning of the structure to a
            /// NULL-terminated ASCII string that contains the device's serial number.
            /// If the device has no serial number, this member is -1.
            /// </summary>
            public UInt32 SerialNumberOffset;
            /// <summary>
            /// Specifies a value of type STORAGE_BUS_TYPE that indicates the type of the
            /// bus to which the device is connected.
            /// </summary>
            [MarshalAs(UnmanagedType.U4, SizeConst = 4)]
            public STORAGE_BUS_TYPE BusType;
            /// <summary>
            /// Length, in bytes of the bus-specific data contained in the
            /// RawDeviceProperties field.
            /// </summary>

            public UInt32 RawPropertiesLength;
            /// <summary>
            /// Array of bytes containing bus specific property data.
            /// </summary>
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 1)]
            public string RawDeviceProperties;
            /// <summary>
            /// The device's vendor ID. &quot;&quot; if none is reported.
            /// </summary>
            public string VendorId { get { return GetRawDeviceProperty(VendorIdOffset); } }
            /// <summary>
            /// The device's product ID. &quot;&quot; if none is reported.
            /// </summary>
            public string ProductId { get { return GetRawDeviceProperty(ProductIdOffset); } }
            /// <summary>
            /// The device's product revision. &quot;&quot; if none is reported.
            /// </summary>
            public string ProductRevision { get { return GetRawDeviceProperty(ProductRevisionOffset); } }
            /// <summary>
            /// The device's vendor ID. &quot;&quot; if none is reported.
            /// </summary>
            public string SerialNumber { get { return GetRawDeviceProperty(SerialNumberOffset); } }
            private string GetRawDeviceProperty(UInt32 propertyOffset)
            {
                string result = String.Empty;
                int rawPropIndex = (int)propertyOffset - (int)Marshal.OffsetOf(this.GetType(), "RawDeviceProperties"), nullIndex;
                if (rawPropIndex >= 0)
                {
                    nullIndex = RawDeviceProperties.IndexOf('\0', rawPropIndex);
                    result = RawDeviceProperties.Substring(rawPropIndex, nullIndex - rawPropIndex);
                }
                return result;
            }
            /// <summary>
            /// Retruns a string representation of this structure.
            /// </summary>
            /// <returns>String representation of this structure</returns>
            public override string ToString()
            {
                string result = String.Format("Size:{0}\nVersion:{1}\n", Size, Version);
                result += String.Format("Type:{0} Modifier:{1}\nRemovable-media:{2}\nCommand-queueing:{3}\n",
                    DeviceType, DeviceTypeModifier, RemovableMedia, CommandQueueing);
                result += String.Format("VendorIdOffset:{0}\nProductIdOffset:{1}\nProductRevisionOffset:{2}\n",
                    VendorIdOffset, ProductIdOffset, ProductRevisionOffset);
                result += String.Format("SerialNumberOffset:{0}\nRawPropertiesLength:{1}\n",
                    SerialNumberOffset, RawPropertiesLength);
                result += String.Format("Vendor-Id:{0}\nProduct-Id:{1}\nProduct-Rev:{2}\nSerialNumber:{3}\nBus-Type:{4}",
                    VendorId, ProductId, ProductRevision, SerialNumber, BusType);
                return result;
            }
        }
        /// <summary>
        /// OS Structure as defined in winioctl.h
        /// </summary>
        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
        public struct STORAGE_DEVICE_ID_DESCRIPTOR
        {
            /// <summary>
            /// Indicates version of the data reported.
            /// </summary>
            public UInt32 Version;
            /// <summary>
            /// Indicates the quantity of data reported, in bytes.
            /// </summary>
            public UInt32 Size;
            /// <summary>
            /// Number of identifiers in the Identifiers
            /// array.
            /// </summary>
            public UInt32 NumberOfIdentifiers;
            /// <summary>
            /// Variable length array of identification descriptors/
            /// </summary>
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 1)]
            public string Identifiers;
        }
        /// <summary>
        /// OS Enumeration as defined in winioctl.h
        /// </summary>
        public enum WRITE_CACHE_TYPE
        {
            /// <summary>
            /// The system cannot report the type of the write cache.
            /// </summary>
            WriteCacheTypeUnknown,
            /// <summary>
            /// The device does not have a write cache.
            /// </summary>
            WriteCacheTypeNone,
            /// <summary>
            /// The device has a write back cache.
            /// </summary>
            WriteCacheTypeWriteBack,
            /// <summary>
            /// The device has a write through cache.
            /// </summary>
            WriteCacheTypeWriteThrough
        }
        /// <summary>
        /// OS Enumeration as defined in winioctl.h
        /// </summary>
        public enum WRITE_CACHE_ENABLE
        {
            /// <summary>
            /// The system cannot report whether the device's write cache is enabled or disabled.
            /// </summary>
            WriteCacheEnableUnknown,
            /// <summary>
            /// The device's write cache is disabled.
            /// </summary>
            WriteCacheDisabled,
            /// <summary>
            /// The device's write cache is enabled.
            /// </summary>
            WriteCacheEnabled
        }
        /// <summary>
        /// OS Enumeration as defined in winioctl.h
        /// </summary>
        public enum WRITE_CACHE_CHANGE
        {
            /// <summary>
            /// The system cannot report the write cache change capability of the device.
            /// </summary>
            WriteCacheChangeUnknown,
            /// <summary>
            /// Host software cannot change the characteristics of the device's write cache
            /// </summary>
            WriteCacheNotChangeable,
            /// <summary>
            /// Host software can change the characteristics of the device's write cache
            /// </summary>
            WriteCacheChangeable
        }
        /// <summary>
        /// OS Enumeration as defined in winioctl.h
        /// </summary>
        public enum WRITE_THROUGH
        {
            /// <summary>
            /// Indicates that no information is available concerning
            /// the writethrough capabilities of the device.
            /// </summary>
            WriteThroughUnknown,
            /// <summary>
            /// Indicates that the device does not support writethrough operations.
            /// </summary>
            WriteThroughNotSupported,
            /// <summary>
            /// Indicates that the device supports writethrough operations.
            /// </summary>
            WriteThroughSupported
        }
        /// <summary>
        /// OS Structure as defined in winioctl.h
        /// </summary>
        [StructLayout(LayoutKind.Sequential, Pack=0)]
        public struct STORAGE_WRITE_CACHE_PROPERTY
        {
            /// <summary>
            /// Indicates version of the data reported.
            /// </summary>
            public UInt32 Version;
            /// <summary>
            /// Indicates the quantity of data reported, in bytes.
            /// </summary>
            public UInt32 Size;
            /// <summary>
            /// Indicates the current write cache type.
            /// </summary>
            public WRITE_CACHE_TYPE WriteCacheType;
            /// <summary>
            /// Indicates whether the write cache is enabled.
            /// </summary>
            public WRITE_CACHE_ENABLE WriteCacheEnabled;
            /// <summary>
            /// Indicates whether if the host can change the write cache characteristics.
            /// </summary>
            public WRITE_CACHE_CHANGE WriteCacheChangeable;
            /// <summary>
            /// Indicates whether the device supports write-through caching.
            /// </summary>
            public WRITE_THROUGH WriteThroughSupported;
            /// <summary>
            /// A Boolean value that indicates whether the device allows host software
            /// to flush the device cache. If TRUE, the device allows host software to
            /// flush the device cache. If FALSE, host software cannot flush the device cache.
            /// </summary>
            [MarshalAs(UnmanagedType.U1)]
            public bool FlushCacheSupported;
            /// <summary>
            /// A Boolean value that indicates whether a user can configure the device's
            /// power protection characteristics in the registry. If TRUE, a user can
            /// configure the device's power protection characteristics in the registry.
            /// If FALSE, the user cannot configure the device's power protection
            /// characteristics in the registry.
            /// </summary>
            [MarshalAs(UnmanagedType.U1)]
            public bool UserDefinedPowerProtection;

            /// <summary>
            /// A Boolean value that indicates whether the device has a battery backup
            /// for the write cache. If TRUE, the device has a battery backup for the
            /// write cache. If FALSE, the device does not have a battery backup for
            /// the writer cache.
            /// </summary>
            [MarshalAs(UnmanagedType.U1)]
            public bool NVCacheEnabled;
        }
        /// <summary>
        /// Structure representing the type of a storage miniport.
        /// </summary>
        public struct STORAGE_MINIPORT_DESCRIPTOR
        {
            /// <summary>
            /// Sizeof(STORAGE_ACCESS_ALIGNMENT_DESCRIPTOR)
            /// </summary>
            public UInt32 Version;
            /// <summary>
            /// Total size of the descriptor, including the space for additional
            /// data and id strings
            /// </summary>
            public UInt32 Size;
            /// <summary>
            /// Type of miniport.
            /// </summary>
            public STORAGE_PORT_CODE_SET Portdriver;
            /// <summary>
            /// Adapter supports a lun reset.
            /// </summary>
            public bool LUNResetSupported;
            /// <summary>
            /// Adapter supports a target reset.
            /// </summary>
            public bool TargetResetSupported;
            /// <summary>
            /// Retruns a string representation of this structure.
            /// </summary>
            /// <returns>String representation of this structure</returns>
            public override string ToString()
            {
                string result = "";
                if (Portdriver == STORAGE_PORT_CODE_SET.StoragePortCodeSetSCSIport)
                    result += "SCSIPort miniport";
                else if (Portdriver == STORAGE_PORT_CODE_SET.StoragePortCodeSetStorport)
                    result += "StorPort miniport";
                else
                    result += "Unknown port type";
                result += String.Format(" LUN Reset: {0} Target Reset: {1}", LUNResetSupported, TargetResetSupported);
                return result;
            }
        }
        /// <summary>
        /// Structure representing the starting VCN to consider for the
        /// FSCTL_GET_RETRIEVAL_POINTER IoControl function.
        /// </summary>
        [StructLayout(LayoutKind.Sequential)]
        public struct STARTING_VCN_INPUT_BUFFER
        {
            /// <summary>
            /// Starting VCN to examine in the file.
            /// </summary>
            public UInt64 StartingVcn;
        }
        /// <summary>
        /// Structure representing the a LCN of a file returned from the
        /// FSCTL_GET_RETRIEVAL_POINTER IoControl function.
        /// </summary>
        [StructLayout(LayoutKind.Sequential, Pack=0)]
        public struct FILE_EXTENT
        {
            /// <summary>
            /// Next VCN of the file that starts at a non-contiguous LCN.
            /// </summary>
            public UInt64 NextVcn;
            /// <summary>
            /// Logical cluster that this extent begins with.
            /// </summary>
            public UInt64 Lcn;
        }
        /// <summary>
        /// Structure representing the starting the mapping of a file's LCNs
        /// returned from the FSCTL_GET_RETRIEVAL_POINTER IoControl function.
        /// </summary>
        [StructLayout(LayoutKind.Sequential, Pack=0)]
        public struct RETRIEVAL_POINTERS_BUFFER
        {
            /// <summary>
            /// Number of extents spanned by the file.
            /// </summary>
            public UInt32 ExtentCount;
            /// <summary>
            /// Starting VCN that was examined for the file.
            /// </summary>
            public UInt64 StartingVcn;
            /// <summary>
            /// File extents spanned by the file.
            /// </summary>
            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 1)]
            public FILE_EXTENT[] Extents;
        }
        /// <summary>
        /// Represents a mapping volume offset to disk offset.
        /// </summary>
        [StructLayout(LayoutKind.Sequential, Pack=0)]
        public struct VOLUME_PHYSICAL_OFFSET
        {
            /// <summary>
            /// Physical disk number where the volume offset lies.
            /// </summary>
            public UInt32 DiskNumber;
            /// <summary>
            /// Physical offset on DiskNumber.
            /// </summary>
            public Int64 Offset;
        }
        /// <summary>
        /// Represents a collection of logical volume offsets to physical disk offsets.
        /// </summary>
        [StructLayout(LayoutKind.Sequential, Pack=0)]
        public struct VOLUME_PHYSICAL_OFFSETS
        {
            /// <summary>
            /// Total number of physical mappings.
            /// </summary>
            public UInt32 NumberOfPhysicalOffsets;
            /// <summary>
            /// Array of mappings.
            /// </summary>
            [MarshalAs(UnmanagedType.ByValArray, SizeConst=1)]
            public VOLUME_PHYSICAL_OFFSET [] PhysicalOffset;
        }
        /// <summary>
        /// Represents a volume logical offset in bytes.
        /// </summary>
        public struct VOLUME_LOGICAL_OFFSET
        {
            /// <summary>
            /// Volume offset in bytes.
            /// </summary>
            public Int64 LogicalOffset;
        }
        [StructLayout(LayoutKind.Sequential, Pack=4)]
        public struct TOKEN_PRIVILEGES
        {
            public UInt32 PrivilegeCount;
            [MarshalAs(UnmanagedType.ByValArray, SizeConst=1)]
            public LUID_AND_ATTRIBUTES [] Privileges;
        }
        [StructLayout(LayoutKind.Sequential)]
        public struct LUID_AND_ATTRIBUTES
        {
            public UInt64 Luid;
            public UInt32 Attributes;
        }
        /// <summary>
        /// Describes the geometry of a disk devices and media.
        /// </summary>
        [StructLayout(LayoutKind.Sequential)]
        public struct DISK_GEOMETRY
        {
            /// <summary>
            /// Number of cylinders
            /// </summary>
            public UInt64 Cylinders;
            /// <summary>
            /// Type of media
            /// </summary>
            public MEDIA_TYPE MediaType;
            /// <summary>
            /// Number of tracks per cylinder
            /// </summary>
            public UInt32 TracksPerCylinder;
            /// <summary>
            /// Number of sectors per track
            /// </summary>
            public UInt32 SectorsPerTrack;
            /// <summary>
            /// Number of bytes per sector
            /// </summary>
            public UInt32 BytesPerSector;
            /// <summary>
            /// Gets a string representation of the structure.
            /// </summary>
            /// <returns>String representation of the structure.</returns>
            public override string ToString()
            {
                return String.Format("Media: {0}\nCylinders: {1}\nTracks\\Cylinder: {2}\nSectors\\Track: {3}\nBytes\\Sector: {4}\n",
                    MediaType, Cylinders, TracksPerCylinder, SectorsPerTrack, BytesPerSector);
            }
        }

        /// <summary>
        /// Describes the attributes for a disk.
        /// </summary>
        [StructLayout(LayoutKind.Sequential)]
        public struct GET_DISK_ATTRIBUTES {
            /// <summary>
            /// Specifies the size of the structure for versioning.
            /// </summary>
            public uint             Version;
            /// <summary>
            /// For alignment purposes.
            /// </summary>
            public uint             Reserved1;
            /// <summary>
            /// Specifies the attributes associated with the disk.
            /// </summary>
            public DiskAttributes   Attributes;
        }

        /// <summary>
        /// Allows the user to configure attributes for a disk.
        /// </summary>
        [StructLayout(LayoutKind.Sequential)]
        public struct SET_DISK_ATTRIBUTES {
            /// <summary>
            /// Specifies the size of the structure for versioning.
            /// </summary>
            public uint             Version;
            /// <summary>
            /// Indicates whether to remember these settings across reboots or not.
            /// </summary>
            public byte             Persist;
            /// <summary>
            /// Indicates whether the ownership taken earlier is being released.
            /// </summary>
            public byte             RelinquishOwnership;
            /// <summary>
            /// For alignment purposes.
            /// </summary>
            private byte            Reserved1;
            /// <summary>
            /// Specifies the new attributes.
            /// </summary>
            public DiskAttributes   Attributes;
            /// <summary>
            /// Specifies the attributes that are being modified.
            /// </summary>
            public DiskAttributes   AttributesMask;
            /// <summary>
            /// Specifies an identifier to be associated with the caller.  This setting is not persisted across reboots.
            /// </summary>
            public Guid             Owner;
        }

        /// <summary>
        /// Media types reported by IOCTL_GET_DRIVE_GEOMETRY
        /// </summary>
        public enum MEDIA_TYPE
        {
            /// <summary>Format is unknown </summary>
            Unknown,
            /// <summary>A 5.25" floppy, with 1.2MB and 512 bytes/sector. </summary>
            F5_1Pt2_512,
            /// <summary>A 3.5" floppy, with 1.44MB and 512 bytes/sector. </summary>
            F3_1Pt44_512,
            /// <summary>A 3.5" floppy, with 2.88MB and 512 bytes/sector. </summary>
            F3_2Pt88_512,
            /// <summary>A 3.5" floppy, with 20.8MB and 512 bytes/sector. </summary>
            F3_20Pt8_512,
            /// <summary>A 3.5" floppy, with 720KB and 512 bytes/sector. </summary>
            F3_720_512,
            /// <summary>A 5.25" floppy, with 360KB and 512 bytes/sector. </summary>
            F5_360_512,
            /// <summary>A 5.25" floppy, with 320KB and 512 bytes/sector. </summary>
            F5_320_512,
            /// <summary>A 5.25" floppy, with 320KB and 1024 bytes/sector. </summary>
            F5_320_1024,
            /// <summary>A 5.25" floppy, with 180KB and 512 bytes/sector. </summary>
            F5_180_512,
            /// <summary>A 5.25" floppy, with 160KB and 512 bytes/sector. </summary>
            F5_160_512,
            /// <summary>Removable media other than floppy. </summary>
            RemovableMedia,
            /// <summary>Fixed hard disk media. </summary>
            FixedMedia,
            /// <summary>A 3.5" floppy, with 120MB and 512 bytes/sector. </summary>
            F3_120M_512,
            /// <summary>A 3.5" floppy, with 640MB and 512 bytes/sector. </summary>
            F3_640_512,
            /// <summary>A 5.25" floppy, with 640KB and 512 bytes/sector. </summary>
            F5_640_512,
            /// <summary>A 5.25" floppy, with 720KB and 512 bytes/sector. </summary>
            F5_720_512,
            /// <summary>A 3.5" floppy, with 1.2MB and 512 bytes/sector. </summary>
            F3_1Pt2_512,
            /// <summary>A 3.5" floppy, with 1.23MB and 1024 bytes/sector. </summary>
            F3_1Pt23_1024,
            /// <summary>A 5.25" floppy, with 1.23KB and 1024 bytes/sector. </summary>
            F5_1Pt23_1024,
            /// <summary>A 3.5" floppy, with 128MB and 512 bytes/sector. </summary>
            F3_128Mb_512,
            /// <summary>A 3.5" floppy, with 230MB and 512 bytes/sector. </summary>
            F3_230Mb_512,
            /// <summary>An 8" floppy, with 256KB and 128 bytes/sector. </summary>
            F8_256_128,
            /// <summary>A 3.5" floppy, with 200MB and 512 bytes/sector. (HiFD). </summary>
            F3_200Mb_512,
            /// <summary>A 3.5" floppy, with 240MB and 512 bytes/sector. (HiFD). </summary>
            F3_240M_512,
            /// <summary>A 3.5" floppy, with 32MB and 512 bytes/sector. </summary>
            F3_32M_512
        }
    }

    [StructLayout(LayoutKind.Sequential, Pack=0)]
    public struct SYSTEM_BASIC_INFORMATION 
    {
        [MarshalAs(UnmanagedType.ByValArray, ArraySubType=UnmanagedType.U1, SizeConst=24)]
        public Byte[] Reserved1;
        [MarshalAs(UnmanagedType.ByValArray, ArraySubType=UnmanagedType.U1, SizeConst=4)]
        public IntPtr[] Reserved2;
        public char NumberOfProcessors;
    }

    [StructLayout(LayoutKind.Sequential, Pack=0)]
    public struct SYSTEM_BOOT_ENVIRONMENT_INFORMATION 
    {
        public Guid BootIdentifier;
        public FIRMWARE_TYPE FirmwareType;
        public UInt64 BootFlags;
    }

    public enum FIRMWARE_TYPE
    {
        FirmwareTypeUnknown,
        FirmwareTypeBios,
        FirmwareTypeEfi,
        FirmwareTypeMax
    };

    public enum SecurityInformation : uint
    {
        OWNER_SECURITY_INFORMATION = 0x00000001,
        GROUP_SECURITY_INFORMATION = 0x00000002,
        DACL_SECURITY_INFORMATION = 0x00000004,
        SACL_SECURITY_INFORMATION = 0x00000008,
        PROTECTED_DACL_SECURITY_INFORMATION = 0x80000000,
        PROTECTED_SACL_SECURITY_INFORMATION = 0x40000000,
        UNPROTECTED_DACL_SECURITY_INFORMATION = 0x20000000,
        UNPROTECTED_SACL_SECURITY_INFORMATION = 0x10000000,
    };

    public enum SYSTEM_INFORMATION_CLASS
    {
        SystemBasicInformation,
        SystemProcessorInformation,             // obsolete...delete
        SystemPerformanceInformation,
        SystemTimeOfDayInformation,
        SystemPathInformation,
        SystemProcessInformation,
        SystemCallCountInformation,
        SystemDeviceInformation,
        SystemProcessorPerformanceInformation,
        SystemFlagsInformation,
        SystemCallTimeInformation,
        SystemModuleInformation,
        SystemLocksInformation,
        SystemStackTraceInformation,
        SystemPagedPoolInformation,
        SystemNonPagedPoolInformation,
        SystemHandleInformation,
        SystemObjectInformation,
        SystemPageFileInformation,
        SystemVdmInstemulInformation,
        SystemVdmBopInformation,
        SystemFileCacheInformation,
        SystemPoolTagInformation,
        SystemInterruptInformation,
        SystemDpcBehaviorInformation,
        SystemFullMemoryInformation,
        SystemLoadGdiDriverInformation,
        SystemUnloadGdiDriverInformation,
        SystemTimeAdjustmentInformation,
        SystemSummaryMemoryInformation,
        SystemMirrorMemoryInformation,
        SystemPerformanceTraceInformation,
        SystemObsolete0,
        SystemExceptionInformation,
        SystemCrashDumpStateInformation,
        SystemKernelDebuggerInformation,
        SystemContextSwitchInformation,
        SystemRegistryQuotaInformation,
        SystemExtendServiceTableInformation,
        SystemPrioritySeperation,
        SystemVerifierAddDriverInformation,
        SystemVerifierRemoveDriverInformation,
        SystemProcessorIdleInformation,
        SystemLegacyDriverInformation,
        SystemCurrentTimeZoneInformation,
        SystemLookasideInformation,
        SystemTimeSlipNotification,
        SystemSessionCreate,
        SystemSessionDetach,
        SystemSessionInformation,
        SystemRangeStartInformation,
        SystemVerifierInformation,
        SystemVerifierThunkExtend,
        SystemSessionProcessInformation,
        SystemLoadGdiDriverInSystemSpace,
        SystemNumaProcessorMap,
        SystemPrefetcherInformation,
        SystemExtendedProcessInformation,
        SystemRecommendedSharedDataAlignment,
        SystemComPlusPackage,
        SystemNumaAvailableMemory,
        SystemProcessorPowerInformation,
        SystemEmulationBasicInformation,
        SystemEmulationProcessorInformation,
        SystemExtendedHandleInformation,
        SystemLostDelayedWriteInformation,
        SystemBigPoolInformation,
        SystemSessionPoolTagInformation,
        SystemSessionMappedViewInformation,
        SystemHotpatchInformation,
        SystemObjectSecurityMode,
        SystemWatchdogTimerHandler,
        SystemWatchdogTimerInformation,
        SystemLogicalProcessorInformation,
        SystemWow64SharedInformationObsolete,
        SystemRegisterFirmwareTableInformationHandler,
        SystemFirmwareTableInformation,
        SystemModuleInformationEx,
        SystemVerifierTriageInformation,
        SystemSuperfetchInformation,
        SystemMemoryListInformation,
        SystemFileCacheInformationEx,
        SystemThreadPriorityClientIdInformation,
        SystemProcessorIdleCycleTimeInformation,
        SystemVerifierCancellationInformation,
        SystemProcessorPowerInformationEx,
        SystemRefTraceInformation,
        SystemSpecialPoolInformation,
        SystemProcessIdInformation,
        SystemErrorPortInformation,
        SystemBootEnvironmentInformation,
        SystemHypervisorInformation,
        SystemVerifierInformationEx,
        SystemTimeZoneInformation,
        SystemImageFileExecutionOptionsInformation,
        SystemCoverageInformation,
        SystemPrefetchPatchInformation,
        SystemVerifierFaultsInformation,
        SystemSystemPartitionInformation,
        SystemSystemDiskInformation,
        SystemProcessorPerformanceDistribution,
        SystemNumaProximityNodeInformation,
        SystemDynamicTimeZoneInformation,
        SystemCodeIntegrityInformation,
        SystemProcessorMicrocodeUpdateInformation,
        SystemProcessorBrandString,
        SystemVirtualAddressInformation,
        SystemLogicalProcessorAndGroupInformation,
        SystemProcessorCycleTimeInformation,
        SystemStoreInformation,
        SystemRegistryAppendString,
        SystemAitSamplingValue,
        SystemVhdBootInformation,
        SystemCpuQuotaInformation,
        SystemSpare0,
        SystemSpare1,
        SystemLowPriorityIoInformation,
        SystemTpmBootEntropyInformation,
        SystemVerifierCountersInformation,
        SystemPagedPoolInformationEx,
        SystemSystemPtesInformationEx,
        SystemNodeDistanceInformation,
        SystemAcpiAuditInformation,
        SystemBasicPerformanceInformation,
        SystemSessionBigPoolInformation,
        SystemBootGraphicsInformation,
        SystemScrubPhysicalMemoryInformation,
        SystemBadPageInformation,
        SystemProcessorProfileControlArea,
        MaxSystemInfoClass  // MaxSystemInfoClass should always be the last enum
    };
    
    /// <summary>
    /// A NT status value.
    /// </summary>
    public enum NtStatus : uint
    {
        // Success
        Success = 0x00000000,
        Wait0 = 0x00000000,
        Wait1 = 0x00000001,
        Wait2 = 0x00000002,
        Wait3 = 0x00000003,
        Wait63 = 0x0000003f,
        Abandoned = 0x00000080,
        AbandonedWait0 = 0x00000080,
        AbandonedWait1 = 0x00000081,
        AbandonedWait2 = 0x00000082,
        AbandonedWait3 = 0x00000083,
        AbandonedWait63 = 0x000000bf,
        UserApc = 0x000000c0,
        KernelApc = 0x00000100,
        Alerted = 0x00000101,
        Timeout = 0x00000102,
        Pending = 0x00000103,
        Reparse = 0x00000104,
        MoreEntries = 0x00000105,
        NotAllAssigned = 0x00000106,
        SomeNotMapped = 0x00000107,
        OpLockBreakInProgress = 0x00000108,
        VolumeMounted = 0x00000109,
        RxActCommitted = 0x0000010a,
        NotifyCleanup = 0x0000010b,
        NotifyEnumDir = 0x0000010c,
        NoQuotasForAccount = 0x0000010d,
        PrimaryTransportConnectFailed = 0x0000010e,
        PageFaultTransition = 0x00000110,
        PageFaultDemandZero = 0x00000111,
        PageFaultCopyOnWrite = 0x00000112,
        PageFaultGuardPage = 0x00000113,
        PageFaultPagingFile = 0x00000114,
        CrashDump = 0x00000116,
        ReparseObject = 0x00000118,
        NothingToTerminate = 0x00000122,
        ProcessNotInJob = 0x00000123,
        ProcessInJob = 0x00000124,
        ProcessCloned = 0x00000129,
        FileLockedWithOnlyReaders = 0x0000012a,
        FileLockedWithWriters = 0x0000012b,

        // Informational
        Informational = 0x40000000,
        ObjectNameExists = 0x40000000,
        ThreadWasSuspended = 0x40000001,
        WorkingSetLimitRange = 0x40000002,
        ImageNotAtBase = 0x40000003,
        RegistryRecovered = 0x40000009,

        // Warning
        Warning = 0x80000000,
        GuardPageViolation = 0x80000001,
        DatatypeMisalignment = 0x80000002,
        Breakpoint = 0x80000003,
        SingleStep = 0x80000004,
        BufferOverflow = 0x80000005,
        NoMoreFiles = 0x80000006,
        HandlesClosed = 0x8000000a,
        PartialCopy = 0x8000000d,
        DeviceBusy = 0x80000011,
        InvalidEaName = 0x80000013,
        EaListInconsistent = 0x80000014,
        NoMoreEntries = 0x8000001a,
        LongJump = 0x80000026,
        DllMightBeInsecure = 0x8000002b,

        // Error
        Error = 0xc0000000,
        Unsuccessful = 0xc0000001,
        NotImplemented = 0xc0000002,
        InvalidInfoClass = 0xc0000003,
        InfoLengthMismatch = 0xc0000004,
        AccessViolation = 0xc0000005,
        InPageError = 0xc0000006,
        PagefileQuota = 0xc0000007,
        InvalidHandle = 0xc0000008,
        BadInitialStack = 0xc0000009,
        BadInitialPc = 0xc000000a,
        InvalidCid = 0xc000000b,
        TimerNotCanceled = 0xc000000c,
        InvalidParameter = 0xc000000d,
        NoSuchDevice = 0xc000000e,
        NoSuchFile = 0xc000000f,
        InvalidDeviceRequest = 0xc0000010,
        EndOfFile = 0xc0000011,
        WrongVolume = 0xc0000012,
        NoMediaInDevice = 0xc0000013,
        NoMemory = 0xc0000017,
        NotMappedView = 0xc0000019,
        UnableToFreeVm = 0xc000001a,
        UnableToDeleteSection = 0xc000001b,
        IllegalInstruction = 0xc000001d,
        AlreadyCommitted = 0xc0000021,
        AccessDenied = 0xc0000022,
        BufferTooSmall = 0xc0000023,
        ObjectTypeMismatch = 0xc0000024,
        NonContinuableException = 0xc0000025,
        BadStack = 0xc0000028,
        NotLocked = 0xc000002a,
        NotCommitted = 0xc000002d,
        InvalidParameterMix = 0xc0000030,
        ObjectNameInvalid = 0xc0000033,
        ObjectNameNotFound = 0xc0000034,
        ObjectNameCollision = 0xc0000035,
        ObjectPathInvalid = 0xc0000039,
        ObjectPathNotFound = 0xc000003a,
        ObjectPathSyntaxBad = 0xc000003b,
        DataOverrun = 0xc000003c,
        DataLate = 0xc000003d,
        DataError = 0xc000003e,
        CrcError = 0xc000003f,
        SectionTooBig = 0xc0000040,
        PortConnectionRefused = 0xc0000041,
        InvalidPortHandle = 0xc0000042,
        SharingViolation = 0xc0000043,
        QuotaExceeded = 0xc0000044,
        InvalidPageProtection = 0xc0000045,
        MutantNotOwned = 0xc0000046,
        SemaphoreLimitExceeded = 0xc0000047,
        PortAlreadySet = 0xc0000048,
        SectionNotImage = 0xc0000049,
        SuspendCountExceeded = 0xc000004a,
        ThreadIsTerminating = 0xc000004b,
        BadWorkingSetLimit = 0xc000004c,
        IncompatibleFileMap = 0xc000004d,
        SectionProtection = 0xc000004e,
        EasNotSupported = 0xc000004f,
        EaTooLarge = 0xc0000050,
        NonExistentEaEntry = 0xc0000051,
        NoEasOnFile = 0xc0000052,
        EaCorruptError = 0xc0000053,
        FileLockConflict = 0xc0000054,
        LockNotGranted = 0xc0000055,
        DeletePending = 0xc0000056,
        CtlFileNotSupported = 0xc0000057,
        UnknownRevision = 0xc0000058,
        RevisionMismatch = 0xc0000059,
        InvalidOwner = 0xc000005a,
        InvalidPrimaryGroup = 0xc000005b,
        NoImpersonationToken = 0xc000005c,
        CantDisableMandatory = 0xc000005d,
        NoLogonServers = 0xc000005e,
        NoSuchLogonSession = 0xc000005f,
        NoSuchPrivilege = 0xc0000060,
        PrivilegeNotHeld = 0xc0000061,
        InvalidAccountName = 0xc0000062,
        UserExists = 0xc0000063,
        NoSuchUser = 0xc0000064,
        GroupExists = 0xc0000065,
        NoSuchGroup = 0xc0000066,
        MemberInGroup = 0xc0000067,
        MemberNotInGroup = 0xc0000068,
        LastAdmin = 0xc0000069,
        WrongPassword = 0xc000006a,
        IllFormedPassword = 0xc000006b,
        PasswordRestriction = 0xc000006c,
        LogonFailure = 0xc000006d,
        AccountRestriction = 0xc000006e,
        InvalidLogonHours = 0xc000006f,
        InvalidWorkstation = 0xc0000070,
        PasswordExpired = 0xc0000071,
        AccountDisabled = 0xc0000072,
        NoneMapped = 0xc0000073,
        TooManyLuidsRequested = 0xc0000074,
        LuidsExhausted = 0xc0000075,
        InvalidSubAuthority = 0xc0000076,
        InvalidAcl = 0xc0000077,
        InvalidSid = 0xc0000078,
        InvalidSecurityDescr = 0xc0000079,
        ProcedureNotFound = 0xc000007a,
        InvalidImageFormat = 0xc000007b,
        NoToken = 0xc000007c,
        BadInheritanceAcl = 0xc000007d,
        RangeNotLocked = 0xc000007e,
        DiskFull = 0xc000007f,
        ServerDisabled = 0xc0000080,
        ServerNotDisabled = 0xc0000081,
        TooManyGuidsRequested = 0xc0000082,
        GuidsExhausted = 0xc0000083,
        InvalidIdAuthority = 0xc0000084,
        AgentsExhausted = 0xc0000085,
        InvalidVolumeLabel = 0xc0000086,
        SectionNotExtended = 0xc0000087,
        NotMappedData = 0xc0000088,
        ResourceDataNotFound = 0xc0000089,
        ResourceTypeNotFound = 0xc000008a,
        ResourceNameNotFound = 0xc000008b,
        ArrayBoundsExceeded = 0xc000008c,
        FloatDenormalOperand = 0xc000008d,
        FloatDivideByZero = 0xc000008e,
        FloatInexactResult = 0xc000008f,
        FloatInvalidOperation = 0xc0000090,
        FloatOverflow = 0xc0000091,
        FloatStackCheck = 0xc0000092,
        FloatUnderflow = 0xc0000093,
        IntegerDivideByZero = 0xc0000094,
        IntegerOverflow = 0xc0000095,
        PrivilegedInstruction = 0xc0000096,
        TooManyPagingFiles = 0xc0000097,
        FileInvalid = 0xc0000098,
        InstanceNotAvailable = 0xc00000ab,
        PipeNotAvailable = 0xc00000ac,
        InvalidPipeState = 0xc00000ad,
        PipeBusy = 0xc00000ae,
        IllegalFunction = 0xc00000af,
        PipeDisconnected = 0xc00000b0,
        PipeClosing = 0xc00000b1,
        PipeConnected = 0xc00000b2,
        PipeListening = 0xc00000b3,
        InvalidReadMode = 0xc00000b4,
        IoTimeout = 0xc00000b5,
        FileForcedClosed = 0xc00000b6,
        ProfilingNotStarted = 0xc00000b7,
        ProfilingNotStopped = 0xc00000b8,
        NotSameDevice = 0xc00000d4,
        FileRenamed = 0xc00000d5,
        CantWait = 0xc00000d8,
        PipeEmpty = 0xc00000d9,
        CantTerminateSelf = 0xc00000db,
        InternalError = 0xc00000e5,
        InvalidParameter1 = 0xc00000ef,
        InvalidParameter2 = 0xc00000f0,
        InvalidParameter3 = 0xc00000f1,
        InvalidParameter4 = 0xc00000f2,
        InvalidParameter5 = 0xc00000f3,
        InvalidParameter6 = 0xc00000f4,
        InvalidParameter7 = 0xc00000f5,
        InvalidParameter8 = 0xc00000f6,
        InvalidParameter9 = 0xc00000f7,
        InvalidParameter10 = 0xc00000f8,
        InvalidParameter11 = 0xc00000f9,
        InvalidParameter12 = 0xc00000fa,
        MappedFileSizeZero = 0xc000011e,
        TooManyOpenedFiles = 0xc000011f,
        Cancelled = 0xc0000120,
        CannotDelete = 0xc0000121,
        InvalidComputerName = 0xc0000122,
        FileDeleted = 0xc0000123,
        SpecialAccount = 0xc0000124,
        SpecialGroup = 0xc0000125,
        SpecialUser = 0xc0000126,
        MembersPrimaryGroup = 0xc0000127,
        FileClosed = 0xc0000128,
        TooManyThreads = 0xc0000129,
        ThreadNotInProcess = 0xc000012a,
        TokenAlreadyInUse = 0xc000012b,
        PagefileQuotaExceeded = 0xc000012c,
        CommitmentLimit = 0xc000012d,
        InvalidImageLeFormat = 0xc000012e,
        InvalidImageNotMz = 0xc000012f,
        InvalidImageProtect = 0xc0000130,
        InvalidImageWin16 = 0xc0000131,
        LogonServer = 0xc0000132,
        DifferenceAtDc = 0xc0000133,
        SynchronizationRequired = 0xc0000134,
        DllNotFound = 0xc0000135,
        IoPrivilegeFailed = 0xc0000137,
        OrdinalNotFound = 0xc0000138,
        EntryPointNotFound = 0xc0000139,
        ControlCExit = 0xc000013a,
        PortNotSet = 0xc0000353,
        DebuggerInactive = 0xc0000354,
        CallbackBypass = 0xc0000503,
        PortClosed = 0xc0000700,
        MessageLost = 0xc0000701,
        InvalidMessage = 0xc0000702,
        RequestCanceled = 0xc0000703,
        RecursiveDispatch = 0xc0000704,
        LpcReceiveBufferExpected = 0xc0000705,
        LpcInvalidConnectionUsage = 0xc0000706,
        LpcRequestsNotAllowed = 0xc0000707,
        ResourceInUse = 0xc0000708,
        ProcessIsProtected = 0xc0000712,
        VolumeDirty = 0xc0000806,
        FileCheckedOut = 0xc0000901,
        CheckOutRequired = 0xc0000902,
        BadFileType = 0xc0000903,
        FileTooLarge = 0xc0000904,
        FormsAuthRequired = 0xc0000905,
        VirusInfected = 0xc0000906,
        VirusDeleted = 0xc0000907,
        TransactionalConflict = 0xc0190001,
        InvalidTransaction = 0xc0190002,
        TransactionNotActive = 0xc0190003,
        TmInitializationFailed = 0xc0190004,
        RmNotActive = 0xc0190005,
        RmMetadataCorrupt = 0xc0190006,
        TransactionNotJoined = 0xc0190007,
        DirectoryNotRm = 0xc0190008,
        CouldNotResizeLog = 0xc0190009,
        TransactionsUnsupportedRemote = 0xc019000a,
        LogResizeInvalidSize = 0xc019000b,
        RemoteFileVersionMismatch = 0xc019000c,
        CrmProtocolAlreadyExists = 0xc019000f,
        TransactionPropagationFailed = 0xc0190010,
        CrmProtocolNotFound = 0xc0190011,
        TransactionSuperiorExists = 0xc0190012,
        TransactionRequestNotValid = 0xc0190013,
        TransactionNotRequested = 0xc0190014,
        TransactionAlreadyAborted = 0xc0190015,
        TransactionAlreadyCommitted = 0xc0190016,
        TransactionInvalidMarshallBuffer = 0xc0190017,
        CurrentTransactionNotValid = 0xc0190018,
        LogGrowthFailed = 0xc0190019,
        ObjectNoLongerExists = 0xc0190021,
        StreamMiniversionNotFound = 0xc0190022,
        StreamMiniversionNotValid = 0xc0190023,
        MiniversionInaccessibleFromSpecifiedTransaction = 0xc0190024,
        CantOpenMiniversionWithModifyIntent = 0xc0190025,
        CantCreateMoreStreamMiniversions = 0xc0190026,
        HandleNoLongerValid = 0xc0190028,
        NoTxfMetadata = 0xc0190029,
        LogCorruptionDetected = 0xc0190030,
        CantRecoverWithHandleOpen = 0xc0190031,
        RmDisconnected = 0xc0190032,
        EnlistmentNotSuperior = 0xc0190033,
        RecoveryNotNeeded = 0xc0190034,
        RmAlreadyStarted = 0xc0190035,
        FileIdentityNotPersistent = 0xc0190036,
        CantBreakTransactionalDependency = 0xc0190037,
        CantCrossRmBoundary = 0xc0190038,
        TxfDirNotEmpty = 0xc0190039,
        IndoubtTransactionsExist = 0xc019003a,
        TmVolatile = 0xc019003b,
        RollbackTimerExpired = 0xc019003c,
        TxfAttributeCorrupt = 0xc019003d,
        EfsNotAllowedInTransaction = 0xc019003e,
        TransactionalOpenNotAllowed = 0xc019003f,
        TransactedMappingUnsupportedRemote = 0xc0190040,
        TxfMetadataAlreadyPresent = 0xc0190041,
        TransactionScopeCallbacksNotSet = 0xc0190042,
        TransactionRequiredPromotion = 0xc0190043,
        CannotExecuteFileInTransaction = 0xc0190044,
        TransactionsNotFrozen = 0xc0190045,

        MaximumNtStatus = 0xffffffff
    }


    /// <summary>
    /// Native Win32 fuctions interop'ed using DllImport
    /// </summary>
    public static partial class NativeFunctions
    {

        [DllImport("ntdll", CharSet = CharSet.Auto, SetLastError = true)]
        public extern static NtStatus NtQuerySystemInformation(
            [In] SYSTEM_INFORMATION_CLASS SystemInformationClass,
            [In][Out] IntPtr SystemInformation,
            [In] UInt32 SystemInformationLength,
            [Out] UInt32 ReturnLength
        );

        #region Kernel32 Imports

        /// <summary>
        /// As defined in winbase.h. See MSDN documentation.
        /// </summary>
        /// <param name="fileName"></param>
        /// <param name="dwDesiredAccess"></param>
        /// <param name="dwShareMode"></param>
        /// <param name="securityAttrs_MustBeZero">Not supported</param>
        /// <param name="dwCreationDisposition"></param>
        /// <param name="dwFlagsAndAttributes"></param>
        /// <param name="hTemplateFile_MustBeZero">Not supported</param>
        /// <returns>True on success. False otherwise.  Use Marshal.GetLastWin32Error()
        /// to get error status.</returns>
        [DllImport("kernel32", CharSet = CharSet.Auto, SetLastError = true)]
        public extern static SafeFileHandle CreateFile(
            [In] string fileName,
            [In] UInt32 dwDesiredAccess,
            [In] System.IO.FileShare dwShareMode,
            [In] IntPtr securityAttrs_MustBeZero,
            [In] System.IO.FileMode dwCreationDisposition,
            [In] UInt32 dwFlagsAndAttributes,
            [In] IntPtr hTemplateFile_MustBeZero);

        /// <summary>
        /// As defined in winbase.h. See MSDN documentation.
        /// </summary>
        /// <param name="hFile"></param>
        /// <param name="lpBuffer"></param>
        /// <param name="nNumberOfBytesToWrite"></param>
        /// <param name="lpNumberOfBytesWritten">Not supported</param>
        /// <param name="lpOverlapped"></param>
        /// <returns>True on success. False otherwise.  Use Marshal.GetLastWin32Error()
        /// to get error status.</returns>
        [DllImport("kernel32", CharSet = CharSet.Auto, SetLastError = true)]
        public extern static unsafe bool WriteFile(
            [In] SafeFileHandle hFile,
            [In] IntPtr lpBuffer,
            [In] uint nNumberOfBytesToWrite,
            [In][Out] ref UInt32 lpNumberOfBytesWritten,
            [Out] System.Threading.NativeOverlapped* lpOverlapped
       );

        /// <summary>
        /// As defined in winbase.h. See MSDN documentation.
        /// </summary>
        /// <param name="hFile"></param>
        /// <param name="lpBuffer"></param>
        /// <param name="nNumberOfBytesToRead"></param>
        /// <param name="lpNumberOfBytesRead">Not supported</param>
        /// <param name="lpOverlapped"></param>
        /// <returns>True on success. False otherwise.  Use Marshal.GetLastWin32Error()
        /// to get error status.</returns>
        [DllImport("kernel32", CharSet = CharSet.Auto, SetLastError = true)]
        public extern static unsafe bool ReadFile(
            [In] SafeFileHandle hFile,
            [In][Out] IntPtr lpBuffer,
            [In] uint nNumberOfBytesToRead,
            [In][Out] ref UInt32 lpNumberOfBytesRead,
            [Out] System.Threading.NativeOverlapped* lpOverlapped
       );

        /// <summary>
        /// As defined in winbase.h. See MSDN documentation.
        /// </summary>
        /// <param name="hFile"></param>
        /// <param name="liDistanceToMove"></param>
        /// <param name="lpNewFilePointer"></param>
        /// <param name="dwMoveMethod"></param>
        /// <returns>True on success. False otherwise.  Use Marshal.GetLastWin32Error()
        /// to get error status.</returns>
        [DllImport("kernel32", CharSet = CharSet.Auto, SetLastError = true)]
        public extern static unsafe bool SetFilePointerEx(
            [In] SafeFileHandle hFile,
            [In] Int64 liDistanceToMove,
            [In][Out] ref UInt64 lpNewFilePointer,
            [In] uint dwMoveMethod
       );

        /// <summary>
        /// As defined in winbase.h. See MSDN documentation.
        /// </summary>
        /// <param name="hDevice">Obtained through CreateFile</param>
        /// <param name="dwIoControlCode">See NativeConstants.IoControlCodes</param>
        /// <param name="lpInBuffer">Use Marshal.AllocHGlobal to allocate.</param>
        /// <param name="nInBufferSize"></param>
        /// <param name="lpOutBuffer">Use Marshal.AllocHGlobal to allocate.</param>
        /// <param name="nOutBufferSize"></param>
        /// <param name="lpBytesReturned"></param>
        /// <param name="lpOverlapped"></param>
        /// <returns>True on success. False otherwise.  Use Marshal.GetLastWin32Error()
        /// to get error status.</returns>
        [DllImport("kernel32.dll", SetLastError = true)]
        internal static unsafe extern bool DeviceIoControl(
                [In] SafeFileHandle hDevice,
                [In] UInt32 dwIoControlCode,
                [In] IntPtr lpInBuffer,
                [In] uint nInBufferSize,
           [In][Out] IntPtr lpOutBuffer,
                [In] UInt32 nOutBufferSize,
           [In][Out] ref UInt32 lpBytesReturned,
               [Out] System.Threading.NativeOverlapped* lpOverlapped
            );
        /// <summary>
        /// The GetDriveType function determines whether a disk drive is a removable,
        /// fixed, CD-ROM, RAM disk, or network drive
        /// </summary>
        /// <param name="lpRootPathName">The root directory for the drive.</param>
        /// <returns>Type of the drive</returns>
        [DllImport("kernel32.dll", CharSet=CharSet.Auto)]
        internal extern static UInt32 GetDriveType(
            [In] string lpRootPathName
            );

        /// <summary>
        /// The GetLogicalDrives function retrieves a bitmask representing
        /// the currently available disk drives.
        /// </summary>
        /// <returns>If the function succeeds, the return value is a bitmask
        /// representing the currently available disk drives. Bit position 0
        /// (the least-significant bit) is drive A, bit position 1 is drive B,
        /// bit position 2 is drive C, and so on.</returns>
        [DllImport("kernel32.dll", SetLastError = true)]
        internal extern static UInt32 GetLogicalDrives();

        /// <summary>
        /// Controls whether the system will handle the specified types of
        /// serious errors or whether the process will handle them.
        /// </summary>
        /// <param name="uMode">The process error mode.</param>
        /// <returns>Previous state of the error-mode bit flags.</returns>
        [DllImport("kernel32.dll")]
        internal extern static UInt32 SetErrorMode(UInt32 uMode);
        #endregion Kernel Imports
    }

    public static partial class IoControlFunctions
    {
        /// <summary>
        /// Sends IOCTL_DISK_GET_DRIVE_GEOMETRY to <paramref name="dev"/>  and returns the geometry.
        /// </summary>
        /// <param name="dev">Device to target.  Can be a disk or CDRom.</param>
        /// <returns>Geometry of device</returns>
        /// <exception cref="Win32Exception">Thrown on failure.</exception>
        public static NativeStructures.DISK_GEOMETRY IOCTL_DISK_GET_DRIVE_GEOMETRY(string dev)
        {
            SafeFileHandle hDisk = NativeFunctions.CreateFile(dev, NativeConstants.GENERIC_READ,
                FileShare.ReadWrite, IntPtr.Zero, FileMode.Open, 0, IntPtr.Zero);

            if (hDisk.IsInvalid)
            {
                int lastError = Marshal.GetLastWin32Error();
                string exception = string.Format(@"Failed to open device {0} err = {1}", dev, lastError);
                throw new Win32Exception(exception);
            }

            UInt32 bytesReturned = 0;
            NativeStructures.DISK_GEOMETRY geometry = new NativeStructures.DISK_GEOMETRY();
            unsafe
            {
                UInt32 outSize = (UInt32)Marshal.SizeOf(typeof(NativeStructures.DISK_GEOMETRY));
                IntPtr outBuffer = Marshal.AllocHGlobal((int)outSize);

                if (!NativeFunctions.DeviceIoControl(hDisk, NativeConstants.IoControlCodes.IOCTL_DISK_GET_DRIVE_GEOMETRY,
                    IntPtr.Zero, 0, outBuffer, outSize, ref bytesReturned, null))
                {
                    hDisk.Close();
                    string exception = string.Format(@"IOCTL_STORAGE_GET_DEVICE_NUMBER failed with err = {0}", Marshal.GetLastWin32Error());
                    throw new Win32Exception(exception);
                }

                geometry = (NativeStructures.DISK_GEOMETRY)Marshal.PtrToStructure(outBuffer, typeof(NativeStructures.DISK_GEOMETRY));
                Marshal.FreeHGlobal(outBuffer);
                outBuffer = IntPtr.Zero;
            }
            hDisk.Close();
            return geometry;
        }

        /// <summary>
        /// Calls IOCTL_STORAGE_GET_DEVICE_NUMBER on <paramref name="dev"/>
        /// </summary>
        /// <param name="dev">Device to query</param>
        /// <returns>STORAGE_DEVICE_NUMBER structure.</returns>
        /// <exception cref="Win32Exception">Thrown on failure.</exception>
        public static NativeStructures.STORAGE_DEVICE_NUMBER IOCTL_STORAGE_GET_DEVICE_NUMBER(string dev)
        {
            SafeFileHandle hDevice = NativeFunctions.CreateFile(dev, NativeConstants.GENERIC_READ,
                FileShare.ReadWrite, IntPtr.Zero, FileMode.Open, 0, IntPtr.Zero);

            if (hDevice.IsInvalid)
            {
                int lastError = Marshal.GetLastWin32Error();
                string exception = string.Format(@"Failed to open device {0} err = {1}", dev, lastError);
                throw new Win32Exception(exception);
            }

            return IOCTL_STORAGE_GET_DEVICE_NUMBER(hDevice);
        }

        /// <summary>
        /// Calls IOCTL_STORAGE_GET_DEVICE_NUMBER on <paramref name="hDevice"/>
        /// </summary>
        /// <param name="hDevice">Device to query</param>
        /// <returns>STORAGE_DEVICE_NUMBER structure.</returns>
        /// <exception cref="Win32Exception">Thrown on failure.</exception>
        internal static NativeStructures.STORAGE_DEVICE_NUMBER IOCTL_STORAGE_GET_DEVICE_NUMBER(SafeFileHandle hDevice)
        {
            UInt32 bytesReturned = 0;
            NativeStructures.STORAGE_DEVICE_NUMBER result = new NativeStructures.STORAGE_DEVICE_NUMBER();
            result.DeviceNumber = -1;
            unsafe
            {
                IntPtr buffer = IntPtr.Zero;
                buffer = Marshal.AllocHGlobal(Marshal.SizeOf(result));
                Marshal.StructureToPtr(result, buffer, false);
                if (!NativeFunctions.DeviceIoControl(hDevice, NativeConstants.IoControlCodes.IOCTL_STORAGE_GET_DEVICE_NUMBER,
                    IntPtr.Zero, 0, buffer, (UInt32)Marshal.SizeOf(result), ref bytesReturned, null))
                {
                    hDevice.Close();
                    string exception = string.Format(@"IOCTL_STORAGE_GET_DEVICE_NUMBER failed with err = {0}", Marshal.GetLastWin32Error());
                    throw new Win32Exception(exception);
                }

                result = (NativeStructures.STORAGE_DEVICE_NUMBER)Marshal.PtrToStructure(buffer, typeof(NativeStructures.STORAGE_DEVICE_NUMBER));
                Marshal.FreeHGlobal(buffer);
                buffer = IntPtr.Zero;
            }
            hDevice.Close();
            return result;
        }

        /// <summary>
        /// Calls IOCTL_STORAGE_GET_DEVICE_NUMBER on <paramref name="dev"/>
        /// </summary>
        /// <param name="dev">Device to query</param>
        /// <returns>STORAGE_DEVICE_NUMBER structure.</returns>
        /// <exception cref="Win32Exception">Thrown on failure.</exception>
        public static NativeStructures.DRIVE_LAYOUT_INFORMATION_EX IOCTL_DISK_GET_DRIVE_LAYOUT_EX(string dev)
        {
            SafeFileHandle hDevice = NativeFunctions.CreateFile(dev, NativeConstants.GENERIC_READ,
                FileShare.ReadWrite, IntPtr.Zero, FileMode.Open, 0, IntPtr.Zero);

            if (hDevice.IsInvalid)
            {
                int lastError = Marshal.GetLastWin32Error();
                string exception = string.Format(@"Failed to open device {0} err = {1}", dev, lastError);
                throw new Win32Exception(exception);
            }

            return IOCTL_DISK_GET_DRIVE_LAYOUT_EX(hDevice);
        }

        /// <summary>
        /// Calls IOCTL_DISK_GET_DRIVE_LAYOUT_EX on <paramref name="disk"/>
        /// </summary>
        /// <param name="disk">Disk to query</param>
        /// <returns>DRIVE_LAYOUT_INFORMATION_EX structure.  If not supported
        /// DRIVE_LAYOUT_INFORMATION_EX.PartitionCount = 0.</returns>
        /// <exception cref="Win32Exception">Thrown on failure other than
        /// ERROR_INVALID_FUNCTION.</exception>
        internal static NativeStructures.DRIVE_LAYOUT_INFORMATION_EX IOCTL_DISK_GET_DRIVE_LAYOUT_EX(SafeFileHandle hDevice)
        {
            UInt32 bytesReturned = 0;
            UInt32 size = 0;
            NativeStructures.DRIVE_LAYOUT_INFORMATION_EX info = new NativeStructures.DRIVE_LAYOUT_INFORMATION_EX();
            info.PartitionEntry = new NativeStructures.PARTITION_INFORMATION_EX[1];
            bool done = false;

            for (UInt32 numPartitions = 1; (!done); numPartitions++)
            {
                size = (UInt32)Marshal.OffsetOf(typeof(NativeStructures.DRIVE_LAYOUT_INFORMATION_EX), "PartitionEntry") + numPartitions * (UInt32)Marshal.SizeOf(info.PartitionEntry[0]);
                unsafe
                {
                    IntPtr buffer = Marshal.AllocHGlobal((int)size);

                    if (!(done = NativeFunctions.DeviceIoControl(hDevice, NativeConstants.IoControlCodes.IOCTL_DISK_GET_DRIVE_LAYOUT_EX,
                        IntPtr.Zero, 0, buffer, size, ref bytesReturned, null)))
                    {
                        int lastError = Marshal.GetLastWin32Error();
                        if (lastError == Win32ErrorCodes.ERROR_INVALID_FUNCTION)
                            done = true;
                        else if ((lastError != Win32ErrorCodes.ERROR_INSUFFICIENT_BUFFER) &&
                                 (lastError != Win32ErrorCodes.ERROR_MORE_DATA))
                        {
                            hDevice.Close();
                            string exception = string.Format(@"IOCTL_DISK_GET_DRIVE_LAYOUT_EX failed with err = {0}", lastError);
                            throw new Win32Exception(exception);
                        }
                    }
                    else
                        info = MarshallDRIVE_LAYOUT_INFORMATION_EX(buffer, size);

                    Marshal.FreeHGlobal(buffer);
                    buffer = IntPtr.Zero;
                }
            }
            hDevice.Close();
            return info;

        }

        #region Marshalling Functions

        private static unsafe NativeStructures.PARTITION_INFORMATION_EX MarshallPARTITION_INFORMATION_EX(IntPtr buffer, UInt32 size)
        {

            if (size < (UInt32)Marshal.SizeOf(typeof(NativeStructures.PARTITION_INFORMATION_EX)))
                throw new ArgumentException("Insufficient buffer size to marshal PARTITION_INFORMATION_EX.", "buffer");
            NativeStructures.PARTITION_INFORMATION_EX info = (NativeStructures.PARTITION_INFORMATION_EX)Marshal.PtrToStructure(
                buffer, typeof(NativeStructures.PARTITION_INFORMATION_EX));
            //The call to marshall the structure copies the bytes in the pattern of the PARTITION_INFORMATION_GPT data signature
            //I can manually override this by calculating the pointer
            if (info.PartitionStyle == NativeStructures.PARTITION_STYLE.PARTITION_STYLE_MBR)
                info.Mbr = (NativeStructures.PARTITION_INFORMATION_MBR)Marshal.PtrToStructure(
                    ComputeFieldOffset(buffer, typeof(NativeStructures.PARTITION_INFORMATION_EX), "Mbr"),
                    typeof(NativeStructures.PARTITION_INFORMATION_MBR));
            //I have found that this is unneccessary, but I am not sure why the marshall call is choosing the PARTITION_INFORMATION_GPT
            //structure's data signature.  It may switch at some point and I'd prefer to be safe.
            if (info.PartitionStyle == NativeStructures.PARTITION_STYLE.PARTITION_STYLE_GPT)
                info.Gpt = (NativeStructures.PARTITION_INFORMATION_GPT)Marshal.PtrToStructure(
                    ComputeFieldOffset(buffer, typeof(NativeStructures.PARTITION_INFORMATION_EX), "Gpt"),
                    typeof(NativeStructures.PARTITION_INFORMATION_GPT));
            return info;
        }
        private static unsafe NativeStructures.DRIVE_LAYOUT_INFORMATION_EX MarshallDRIVE_LAYOUT_INFORMATION_EX(IntPtr buffer, UInt32 size)
        {
            NativeStructures.DRIVE_LAYOUT_INFORMATION_EX info = new NativeStructures.DRIVE_LAYOUT_INFORMATION_EX();
            info.PartitionStyle = (NativeStructures.PARTITION_STYLE)Marshal.ReadInt32(buffer);
            info.PartitionCount = (UInt32)Marshal.ReadInt32(ComputeFieldOffset(buffer,
                typeof(NativeStructures.DRIVE_LAYOUT_INFORMATION_EX), "PartitionCount"));
            if (size < ((UInt32)Marshal.OffsetOf(typeof(NativeStructures.DRIVE_LAYOUT_INFORMATION_EX), "PartitionEntry") +
                info.PartitionCount * (UInt32)Marshal.SizeOf(typeof(NativeStructures.PARTITION_INFORMATION_EX))))
                throw new ArgumentException("Insufficient buffer size to marshal DRIVE_LAYOUT_INFORMATION_EX.", "buffer");
            if (info.PartitionStyle == NativeStructures.PARTITION_STYLE.PARTITION_STYLE_MBR)
                info.Mbr = (NativeStructures.DRIVE_LAYOUT_INFORMATION_MBR)Marshal.PtrToStructure(
                    ComputeFieldOffset(buffer, typeof(NativeStructures.DRIVE_LAYOUT_INFORMATION_EX), "Mbr"),
                    typeof(NativeStructures.DRIVE_LAYOUT_INFORMATION_MBR));
            if (info.PartitionStyle == NativeStructures.PARTITION_STYLE.PARTITION_STYLE_GPT)
                info.Gpt = (NativeStructures.DRIVE_LAYOUT_INFORMATION_GPT)Marshal.PtrToStructure(
                    ComputeFieldOffset(buffer, typeof(NativeStructures.DRIVE_LAYOUT_INFORMATION_EX), "Gpt"),
                    typeof(NativeStructures.DRIVE_LAYOUT_INFORMATION_GPT));
            info.PartitionEntry = new NativeStructures.PARTITION_INFORMATION_EX[info.PartitionCount];
            size -= (UInt32)Marshal.OffsetOf(typeof(NativeStructures.DRIVE_LAYOUT_INFORMATION_EX), "PartitionEntry");
            for (UInt32 partNumber = 0; partNumber < info.PartitionCount; partNumber++)
            {
                info.PartitionEntry[partNumber] = MarshallPARTITION_INFORMATION_EX(ComputeFieldOffsetAtIndex(buffer,
                    typeof(NativeStructures.DRIVE_LAYOUT_INFORMATION_EX), "PartitionEntry", partNumber), size);
                size -= (UInt32)Marshal.SizeOf(typeof(NativeStructures.PARTITION_INFORMATION_EX));
            }
            return info;
        }
        private static unsafe void MarshallDRIVE_LAYOUT_INFORMATION_EX(NativeStructures.DRIVE_LAYOUT_INFORMATION_EX layout, IntPtr buffer, UInt32 size)
        {
            if (size < ((UInt32)Marshal.OffsetOf(typeof(NativeStructures.DRIVE_LAYOUT_INFORMATION_EX), "PartitionEntry") +
                layout.PartitionCount * (UInt32)Marshal.SizeOf(typeof(NativeStructures.PARTITION_INFORMATION_EX))))
                throw new ArgumentException("Insufficient buffer size to marshal layout.", "buffer");
            Marshal.WriteInt32(buffer, (int)layout.PartitionStyle);
            Marshal.WriteInt32(ComputeFieldOffset(buffer, typeof(NativeStructures.DRIVE_LAYOUT_INFORMATION_EX), "PartitionCount"),
                (int)layout.PartitionCount);
            if (layout.PartitionStyle == NativeStructures.PARTITION_STYLE.PARTITION_STYLE_GPT)
                Marshal.StructureToPtr(layout.Gpt, ComputeFieldOffset(buffer, typeof(NativeStructures.PARTITION_INFORMATION_EX),
                    "Gpt"), false);
            else if (layout.PartitionStyle == NativeStructures.PARTITION_STYLE.PARTITION_STYLE_MBR)
                Marshal.StructureToPtr(layout.Mbr, ComputeFieldOffset(buffer, typeof(NativeStructures.PARTITION_INFORMATION_EX),
                    "Mbr"), false);
            size -= (UInt32)Marshal.OffsetOf(typeof(NativeStructures.DRIVE_LAYOUT_INFORMATION_EX), "PartitionEntry");
            for (UInt32 partNumber = 0; partNumber < layout.PartitionCount; partNumber++)
            {
                NativeStructures.PARTITION_INFORMATION_EX newPart = layout.PartitionEntry[partNumber];
                newPart.RewritePartition = true;
                MarshallPARTITION_INFORMATION_EX(newPart, ComputeFieldOffsetAtIndex(buffer,
                    typeof(NativeStructures.DRIVE_LAYOUT_INFORMATION_EX), "PartitionEntry", partNumber), size);
                size -= (UInt32)Marshal.SizeOf(typeof(NativeStructures.PARTITION_INFORMATION_EX));
            }
            return;
        }
        private static unsafe void MarshallPARTITION_INFORMATION_EX(NativeStructures.PARTITION_INFORMATION_EX info, IntPtr buffer, UInt32 size)
        {
            if (size < (UInt32)Marshal.SizeOf(typeof(NativeStructures.PARTITION_INFORMATION_EX)))
                throw new ArgumentException("Insufficient buffer to marshall info.", "buffer");
            Marshal.StructureToPtr(info, buffer, false);
            if (info.PartitionStyle == NativeStructures.PARTITION_STYLE.PARTITION_STYLE_GPT)
                Marshal.StructureToPtr(info.Gpt, ComputeFieldOffset(buffer,
                    typeof(NativeStructures.PARTITION_INFORMATION_EX), "Gpt"), false);
            if (info.PartitionStyle == NativeStructures.PARTITION_STYLE.PARTITION_STYLE_MBR)
                Marshal.StructureToPtr(info.Mbr, ComputeFieldOffset(buffer,
                    typeof(NativeStructures.PARTITION_INFORMATION_EX), "Mbr"), false);
        }

        private static unsafe IntPtr ComputeFieldOffset(IntPtr buffer, Type parentStructure, string fieldName)
        {
            long offset = (long)Marshal.OffsetOf(parentStructure, fieldName);
            return (IntPtr)((long)buffer + offset);
        }

        private static unsafe IntPtr ComputeFieldOffsetAtIndex(IntPtr buffer, Type parentStructure, string fieldName, UInt32 index)
        {
            Type fieldType = parentStructure.GetField(fieldName).FieldType.GetElementType();
            long elementSize = Marshal.SizeOf(fieldType);
            long offset = (long)Marshal.OffsetOf(parentStructure, fieldName) + elementSize * index;
            return (IntPtr)((long)buffer + offset);
        }
        #endregion Marshalling Functions
    }
}