using System;
using System.Runtime.InteropServices;

namespace Microsoft.Test.Utils
{
    public static class NativeStructures
    {

        [Flags]
        public enum DiskAttributes : ulong
        {
            Offline = 1uL,
            ReadOnly = 2uL
        }

        public struct GET_DISK_ATTRIBUTES
        {
            public NativeStructures.DiskAttributes Attributes;
            public uint Reserved1;
            public uint Version;
        }

        public struct SET_DISK_ATTRIBUTES
        {
            public uint Version;

            public byte Persist;

            public byte RelinquishOwnership;

            private byte Reserved1;

            public NativeStructures.DiskAttributes Attributes;

            public NativeStructures.DiskAttributes AttributesMask;

            public Guid Owner;
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
        [StructLayout(LayoutKind.Sequential)]
        public struct CREATE_DISK_MBR
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
        [StructLayout(LayoutKind.Sequential)]
        public struct CREATE_DISK_GPT
        {
            /// <summary>
            /// Unique GUID of this disk.
            /// </summary>
            public Guid DiskId;
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
                return String.Format("ID:{0}, Max Partition Count:{1}",
                    DiskId, MaxPartitionCount);
            }
        }

        /// <summary>
        /// OS Structure as defined in winioctl.h
        /// </summary>
        [StructLayout(LayoutKind.Explicit)]
        public struct CREATE_DISK
        {
            /// <summary>
            /// Format of the partitions on the disk.
            /// </summary>
            [FieldOffset(0)]
            public PARTITION_STYLE PartitionStyle;
            /// <summary>
            /// If the PartitionStyle is <see cref="PARTITION_STYLE.PARTITION_STYLE_MBR"/>
            /// this contains information about an MBR drive layout.  Otherwise this element should not be accessed.
            /// </summary>
            [FieldOffset(4)]
            public CREATE_DISK_MBR Mbr;
            /// <summary>
            /// If the PartitionStyle is <see cref="PARTITION_STYLE.PARTITION_STYLE_GPT"/>
            /// this contains information about an GPT drive layout.  Otherwise this element should not be accessed.
            /// </summary>
            [FieldOffset(4)]
            public CREATE_DISK_GPT Gpt;

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

                return result;
            }
        }

        public enum DRIVER_STATE
        {
            /// <summary>
            /// Data mode
            /// </summary>
            Data,
            /// <summary>
            /// MetsData mode
            /// </summary>
            MetaData,
            /// <summary>
            /// Bitmap mode
            /// </summary>
            BitMap
        }

        [StructLayout(LayoutKind.Sequential)]
        public struct DISK_STATUS
        {
            public int DriverState;
        }
    }
}
