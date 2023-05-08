using Microsoft.Test.Storages;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace ManagedStorageProvider
{
    public interface IStorageProvider
    {
        // TODO: PhysicalDrive Need to go
        IDictionary<int, PhysicalDrive> GetPhysicalDrives(bool bIncludeSystemDisk);
    }
}
