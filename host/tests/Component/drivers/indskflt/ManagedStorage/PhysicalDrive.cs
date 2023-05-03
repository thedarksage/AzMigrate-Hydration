//-----------------------------------------------------------------------
// <copyright file="PhysicalDrive.cs" company="Microsoft">
//     Copyright (c) Microsoft Corporation. All rights reserved.
// </copyright>
//-----------------------------------------------------------------------
namespace Microsoft.Test.Storages
{
    using System;
    using System.ComponentModel;
    using System.Globalization;
    using System.Runtime.InteropServices;

    using Microsoft.Test.Utils;
    using Microsoft.Test.Utils.NativeMacros;
    using Microsoft.Win32.SafeHandles;
    using System.IO;
    using System.Threading;

    public class PhysicalDrive : RawIOStorage, IStorageControl
    {
        //protected SafeFileHandle fileHandle;

        public int DiskNumber { get; private set; }
        public UInt32 DiskSignature { get; private set; }
        public NativeStructures.PARTITION_STYLE PartitionStyle { get; private set; }
        public UInt32 BytesPerSector { get; set; }
        public UInt64 Size { get; set; }

        public UInt64 TotalSectors { get; set; }
        #region Constructors

        public PhysicalDrive(int diskNumber)
            : base(GetSpecialFileName(diskNumber), FileFlagsAndAttributes.WriteThrough | FileFlagsAndAttributes.NoBuffering)
        {
            this.DiskNumber = diskNumber;
        }

        public PhysicalDrive(int diskNumber, UInt32 bytesPerSector, UInt64 size, UInt64 totalSectors)
            : this(diskNumber)
        {
            this.BytesPerSector = bytesPerSector;
            this.Size = size;
            this.TotalSectors = totalSectors;
        }

        public PhysicalDrive(int diskNumber, UInt32 bytesPerSector, UInt64 size)
            : this(diskNumber)
        {
            this.BytesPerSector = bytesPerSector;
            this.Size = size;
            NativeStructures.DRIVE_LAYOUT_INFORMATION_EX info = GetDriveLayoutEx();
            this.DiskSignature = info.Mbr.Signature;
            this.PartitionStyle = info.PartitionStyle;
        }

        public PhysicalDrive(int diskNumber, FileFlagsAndAttributes flagsAndAttributes)
            : base(GetSpecialFileName(diskNumber), flagsAndAttributes)
        {
            this.DiskNumber = diskNumber;
        }

        public PhysicalDrive(int diskNumber, ShareModes shareModes, AccessMasks accessMasks, CreationDisposition creationDisposition, FileFlagsAndAttributes flagsAndAttributes)
            : base(GetSpecialFileName(diskNumber), shareModes, accessMasks, creationDisposition, flagsAndAttributes)
        {
            this.DiskNumber = diskNumber;
        }

        #endregion


        private void SetDiskAttributes(bool online, bool readOnly)
        {
            NativeStructures.SET_DISK_ATTRIBUTES attributes = new NativeStructures.SET_DISK_ATTRIBUTES();
            UInt32 size             = 0;  
            UInt32 dwBytes          = 0;  

            attributes.AttributesMask  =   NativeStructures.DiskAttributes.ReadOnly | NativeStructures.DiskAttributes.Offline;  

            attributes.Attributes      =  (online)? 0 : NativeStructures.DiskAttributes.Offline;
            attributes.Attributes |= (readOnly) ? NativeStructures.DiskAttributes.ReadOnly : 0;

            attributes.Version     =   (UInt32)    Marshal.SizeOf(attributes);  
            size                =   (UInt32)    Marshal.SizeOf(attributes);  
            IntPtr buffer       =   Marshal.AllocHGlobal((int) size);  
            Marshal.StructureToPtr(attributes, buffer, false);  
            IntPtr output = IntPtr.Zero;
            unsafe
            {
                if (!NativeWrapper.DeviceIoControl(fileHandle,
                                                   IoControlCodes.IOCTL_DISK_SET_DISK_ATTRIBUTES,
                                                    buffer,
                                                    size,
                                                    output,
                                                    0,
                                                    ref dwBytes,
                                                    null
                    ))
                {
                    string exceptionMessage = string.Format(CultureInfo.InvariantCulture,
                        "Exception occured during IOCTL_DISK_SET_DISK_ATTRIBUTES File() native call for file name : {0}", this.FileName);
                    throw new Exception(exceptionMessage, new Win32Exception(Marshal.GetLastWin32Error()));
                }
                Marshal.FreeHGlobal(buffer);
            }
                
        }

        public NativeStructures.GET_DISK_ATTRIBUTES GetDiskAttributes()
        {
            UInt32 scratch = 0, size = 0;
            NativeStructures.GET_DISK_ATTRIBUTES attributes = new NativeStructures.GET_DISK_ATTRIBUTES();
            unsafe
            {
                size = (UInt32)Marshal.SizeOf(attributes);
                IntPtr buffer = Marshal.AllocHGlobal((int)size);
                if (!NativeWrapper.DeviceIoControl(fileHandle, IoControlCodes.IOCTL_DISK_GET_DISK_ATTRIBUTES,
                    IntPtr.Zero, 0, buffer, size, ref scratch, null))
                    throw new Win32Exception(Marshal.GetLastWin32Error());

                attributes = (NativeStructures.GET_DISK_ATTRIBUTES)Marshal.PtrToStructure(buffer, typeof(NativeStructures.GET_DISK_ATTRIBUTES));  

                Marshal.FreeHGlobal(buffer);
                buffer = IntPtr.Zero;
            }  

            return attributes;
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

        public void CleanDisk()
        {
            UInt32 bytesReturned = 0;

            unsafe
            {
                if (!NativeWrapper.DeviceIoControl(fileHandle, IoControlCodes.IOCTL_DISK_DELETE_DRIVE_LAYOUT,
                    IntPtr.Zero, 0, IntPtr.Zero, 0, ref bytesReturned, null))
                    throw new Win32Exception();
            }
        }

        internal void SetDriveLayoutEx(NativeStructures.DRIVE_LAYOUT_INFORMATION_EX layout)
        {
            UInt32 bytesReturned = 0;
            unsafe
            {
                int size = (int)Marshal.OffsetOf(typeof(NativeStructures.DRIVE_LAYOUT_INFORMATION_EX), "PartitionEntry") +
                    Marshal.SizeOf(typeof(NativeStructures.PARTITION_INFORMATION_EX)) * layout.PartitionEntry.Length;

                IntPtr buffer = Marshal.AllocHGlobal(size);
                MarshallDRIVE_LAYOUT_INFORMATION_EX(layout, buffer, (UInt32)size);
                if (!NativeWrapper.DeviceIoControl(fileHandle, IoControlCodes.IOCTL_DISK_SET_DRIVE_LAYOUT_EX,
                    buffer, (UInt32)size, IntPtr.Zero, 0, ref bytesReturned, null))
                {
                    Marshal.FreeHGlobal(buffer);
                    buffer = IntPtr.Zero;
                    throw new Win32Exception();
                }
                Marshal.FreeHGlobal(buffer);
                buffer = IntPtr.Zero;
            }
        }

        public void ResetDisk()
        {
            StreamWriter sw = new StreamWriter(@"c:\tmp\layout.log", true);

            NativeStructures.DRIVE_LAYOUT_INFORMATION_EX info = GetDriveLayoutEx();
            sw.WriteLine("Before1: {0} Signature:{1}", DateTime.Now.ToString(), info.Mbr.Signature.ToString());
            info.PartitionCount = 0;
            sw.WriteLine("Before2: {0} Signature:{1}", DateTime.Now.ToString(), info.Mbr.Signature.ToString());

            SetDriveLayoutEx(info);
            Thread.Sleep(1000);
            info = GetDriveLayoutEx();

            sw.WriteLine("After: {0} Signature:{1}", DateTime.Now.ToString(), info.Mbr.Signature.ToString());
            sw.Close();
        }

        public void ResetDiskInfo()
        {
            bool done = false;
            UInt32 bytesReturned = 0;
            UInt32 size = 0;
            NativeStructures.CREATE_DISK info = new NativeStructures.CREATE_DISK();
            info.PartitionStyle = this.PartitionStyle;
            info.Mbr.Signature = this.DiskSignature;

            unsafe
            {
                size = (UInt32)Marshal.SizeOf(info);
                IntPtr buffer = Marshal.AllocHGlobal((int)size);
                Marshal.StructureToPtr(info, buffer, false);

                if (!(done = NativeWrapper.DeviceIoControl(fileHandle, IoControlCodes.IOCTL_DISK_CREATE_DISK,
                        buffer, size, IntPtr.Zero, 0, ref bytesReturned, null)))
                {
                    int lastError = Marshal.GetLastWin32Error();
                    Marshal.FreeHGlobal(buffer);
                    buffer = IntPtr.Zero;
                    throw new Win32Exception();
                }
                else
                {
                    Marshal.FreeHGlobal(buffer);
                    buffer = IntPtr.Zero;
                }
            }

        }

        public NativeStructures.DRIVE_LAYOUT_INFORMATION_EX GetDriveLayoutEx()
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

                    if (!(done = NativeWrapper.DeviceIoControl(fileHandle, IoControlCodes.IOCTL_DISK_GET_DRIVE_LAYOUT_EX,
                        IntPtr.Zero, 0, buffer, size, ref bytesReturned, null)))
                    {
                        int lastError = Marshal.GetLastWin32Error();
                        if (lastError == NativeConstants.Win32ErrorCodes.ERROR_INVALID_FUNCTION)
                            done = true;
                        else if ((lastError != NativeConstants.Win32ErrorCodes.ERROR_INSUFFICIENT_BUFFER) &&
                                 (lastError != NativeConstants.Win32ErrorCodes.ERROR_MORE_DATA))
                            throw new Win32Exception();
                    }
                    else
                        info = MarshallDRIVE_LAYOUT_INFORMATION_EX(buffer, size);

                    Marshal.FreeHGlobal(buffer);
                    buffer = IntPtr.Zero;
                }
            }

            return info;
        }
    
        public void Online(bool readOnly)
        {
            SetDiskAttributes(online : true, readOnly : readOnly);
        }

        public void Offline(bool readOnly)
        {
            SetDiskAttributes(online: false, readOnly: readOnly);
        }

        private static string GetSpecialFileName(int diskNumber)
        {
            return @"\\.\PhysicalDrive" + diskNumber;
        }
    }
}