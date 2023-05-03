//-----------------------------------------------------------------------
// <copyright file="VirtualHardDisk.cs" company="Microsoft">
//     Copyright (c) Microsoft Corporation. All rights reserved.
// </copyright>
//-----------------------------------------------------------------------
namespace Microsoft.Test.Storages
{
    using System;
    using System.IO;
    using System.Threading;
    using System.Threading.Tasks;

    using Microsoft.Test.Utils.NativeMacros;

    public class VirtualHardDisk : IReadableStorage, IWritableStorage
    {
        public string VhdPath { get; private set; }

        // TO DO : Encapsulate this object inside Lazy<T> to delay the initialization, until the first access of the object.
        // By implementing that way, we can ensure that service is always initialized on demand and also avoid unexpected errors such as
        // the static object creation failing during app domain loading (which would lead to crash of application).  In lazy case,
        // we can handle the exceptions during creation of service object without application crash.
        //private static VMImageManagementService VMImageMgmtService = new VMImageManagementService(Environment.MachineName);
        private PhysicalDrive mountedPhysicalDisk;

        #region Constructors

        // TO DO : Use OpenVirtualDisk() Win32 API call instead of using PhysicalDrive object, so that the dependency MS.Virt.Test.dll could be removed
        public VirtualHardDisk(string vhdPath)
        {
            int diskNumber = this.MountVhd(vhdPath);
            this.mountedPhysicalDisk = new PhysicalDrive(diskNumber);
        }

        public VirtualHardDisk(string vhdPath, FileFlagsAndAttributes flagsAndAttributes)
        {
            int diskNumber = this.MountVhd(vhdPath);
            this.mountedPhysicalDisk = new PhysicalDrive(diskNumber, flagsAndAttributes);
        }

        public VirtualHardDisk(string vhdPath, ShareModes shareModes, AccessMasks accessMasks, CreationDisposition creationDisposition, FileFlagsAndAttributes flagsAndAttributes)
        {
            int diskNumber = this.MountVhd(vhdPath);
            this.mountedPhysicalDisk = new PhysicalDrive(diskNumber, shareModes, accessMasks, creationDisposition, flagsAndAttributes);
        }

        private int MountVhd(string vhdPath)
        {
            this.VhdPath = Path.GetFullPath(vhdPath);
            int diskNumber = -1; //TODO: VMImageMgmtService.Mount(this.VhdPath);
            return diskNumber;
        }

        #endregion

        public bool IsInAsyncMode
        {
            get { return this.mountedPhysicalDisk.IsInAsyncMode; }
        }

        #region IWritableStorage members

        public uint Write(byte[] buffer, uint numberOfBytesToWrite, ulong storageOffset)
        {
            return this.mountedPhysicalDisk.Write(buffer, numberOfBytesToWrite, storageOffset);
        }

        public Task<uint> WriteAsync(byte[] buffer, uint numberOfBytesToWrite, ulong storageOffset)
        {
            return this.mountedPhysicalDisk.WriteAsync(buffer, numberOfBytesToWrite, storageOffset);
        }

        public Task<uint> WriteAsync(byte[] buffer, uint numberOfBytesToWrite, ulong storageOffset, CancellationToken ct)
        {
            return this.mountedPhysicalDisk.WriteAsync(buffer, numberOfBytesToWrite, storageOffset, ct);
        }

        #endregion

        #region IReadableStorage members

        public uint Read(byte[] buffer, uint numberOfBytesToRead, ulong storageOffset)
        {
            return this.mountedPhysicalDisk.Read(buffer, numberOfBytesToRead, storageOffset);
        }

        public Task<uint> ReadAsync(byte[] buffer, uint numberOfBytesToRead, ulong storageOffset)
        {
            return this.mountedPhysicalDisk.ReadAsync(buffer, numberOfBytesToRead, storageOffset);
        }

        public Task<uint> ReadAsync(byte[] buffer, uint numberOfBytesToRead, ulong storageOffset, CancellationToken ct)
        {
            return this.mountedPhysicalDisk.ReadAsync(buffer, numberOfBytesToRead, storageOffset, ct);
        }

        #endregion

        #region Dispose Pattern

        private bool isDisposed = false;
        private object disposingLock = new Object();
        protected virtual void Dispose(bool isDisposing)
        {
            lock (this.disposingLock)
            {
                if (this.isDisposed)
                    return;

                try
                {
                    /*TODO
                    if (VMImageMgmtService != null && this.VhdPath != null)
                        VMImageMgmtService.Unmount(this.VhdPath);
                     */ 
                }
                catch
                {
                    // Any exception in Dispose should be omitted
                    // TO DO : Log the exception
                }

                // Though the above call invalidates the physical drive handle,
                // including this for completeness and safety (if the previous call failed, this will act as a retry)
                if (this.mountedPhysicalDisk != null)
                    this.mountedPhysicalDisk.Dispose();

                this.isDisposed = true;
            }
        }

        public void Close()
        {
            this.Dispose();
        }

        public void Dispose()
        {
            this.Dispose(true);
            GC.SuppressFinalize(this);
        }

        ~VirtualHardDisk()
        {
            this.Dispose(false);
        }

        #endregion
    }
}