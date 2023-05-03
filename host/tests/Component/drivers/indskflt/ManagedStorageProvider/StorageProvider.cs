using System;
using System.Collections.Generic;
using System.Linq;
using System.Management;
using System.Text;
using System.Threading.Tasks;

using Microsoft.Test.Storages;

/// Summary
/// 
/// Summary

namespace ManagedStorageProvider
{
    public class StorageProvider
    {
        public static IDictionary<int, PhysicalDrive> GetPhysicalDrives(bool bIncludeSystemDisk)
        {
            IDictionary<int, PhysicalDrive> physicalDrives = new SortedDictionary<int, PhysicalDrive>();

            ManagementObjectSearcher searcher = new
                ManagementObjectSearcher("SELECT * FROM Win32_DiskDrive");

            foreach (ManagementObject wmi_HD in searcher.Get())
            {
                int uDiskIndex = int.Parse(wmi_HD["Index"].ToString());
                UInt32 bytesPerSector = UInt32.Parse(wmi_HD["BytesPerSector"].ToString());
                UInt64 diskSize = UInt64.Parse(wmi_HD["Size"].ToString());
                UInt64 totalSectors = UInt64.Parse(wmi_HD["TotalSectors"].ToString());
                if (!bIncludeSystemDisk && IsSystemDisk(uDiskIndex))
                {
                    continue;
                }

                physicalDrives.Add(new KeyValuePair<int,PhysicalDrive> (
                                            uDiskIndex, new PhysicalDrive(diskNumber: uDiskIndex,
                                                                          bytesPerSector: bytesPerSector,
                                                                          size: diskSize,
                                                                          totalSectors: totalSectors)));
            }

            return physicalDrives;
        }

        private static bool IsSystemDisk(int diskIndex)
        {
            return (0 == diskIndex);
        }
    }
}
