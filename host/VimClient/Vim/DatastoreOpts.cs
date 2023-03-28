//---------------------------------------------------------------
//  <copyright file="DatastoreOpts.cs" company="Microsoft">
//      Copyright (c) Microsoft Corporation. All rights reserved.
//  </copyright>
//
//  <summary>
//  Implements helper class for managing VMWare ESX/vCenter.
//  </summary>
//
//  History:     28-April-2015   GSinha     Created
//----------------------------------------------------------------

using System;
using System.Collections.Generic;
using System.Collections.Specialized;
using System.Linq;
using System.Text;
using VMware.Vim;
using T = VMware.Client.VimDiagnostics;

namespace VMware.Client
{
    /// <summary>
    /// VMware data store operations.
    /// </summary>
    public class DatastoreOpts
    {
        /// <summary>
        /// The client connection.
        /// </summary>
        private VMware.Vim.VimClient client;

        /// <summary>
        /// The data center operations helper.
        /// </summary>
        private DataCenterOpts dataCenterOpts;

        /// <summary>
        /// Initializes a new instance of the <see cref="DatastoreOpts" /> class.
        /// </summary>
        /// <param name="client">The client connection to use.</param>
        /// <param name="dataCenterOpts">The data center operations helper</param>
        public DatastoreOpts(VMware.Vim.VimClient client, DataCenterOpts dataCenterOpts)
        {
            this.client = client;
            this.dataCenterOpts = dataCenterOpts;
        }

        /// <summary>
        /// Parameter for MoveOrCopyFile method.
        /// </summary>
        public enum MoveOrCopyOp
        {
            /// <summary>
            /// Do move operation.
            /// </summary>
            Move,

            /// <summary>
            /// Do copy operation.
            /// </summary>
            Copy
        }

        /// <summary>
        /// Collects the datastore Information.
        /// </summary>
        /// <param name="hostViews">The host views.</param>
        /// <param name="datacenterViews">The data center views.</param>
        /// <returns>List of DataStoreInfo.</returns>
        public List<DataStoreInfo> GetDataStoreInfo(
            List<HostSystem> hostViews,
            List<Datacenter> datacenterViews)
        {
            T.Tracer().TraceInformation("Discovering data store information.");

            List<DataStoreInfo> dsInfoList = new List<DataStoreInfo>();
            List<ManagedObjectReference> dsArray = new List<ManagedObjectReference>();
            foreach (Datacenter dcView in datacenterViews)
            {
                if (dcView.Datastore != null)
                {
                    dsArray.AddRange(dcView.Datastore);
                }
            }

            if (dsArray.Count == 0)
            {
                T.Tracer().TraceError("No Datastores exists in Datacenters.");
                return dsInfoList;
            }

            List<ViewBase> datastoresMOB = this.client.GetViewsByMorefs(dsArray, null);
            foreach (HostSystem hostView in hostViews)
            {
                try
                {
                    HostSystem subHostView = hostView;
                    string hostName = hostView.Name;

                    string[] properties = new string[]
                    {
                        "config.fileSystemVolume",
                        "config.storageDevice.scsiLun",
                        "config.storageDevice.scsiTopology"
                    };

                    subHostView.UpdateViewData(properties);

                    string hostKeyValue = subHostView.MoRef.Value;
                    ScsiLun[] scsiLuns = subHostView.Config.StorageDevice.ScsiLun;
                    HostScsiTopologyInterface[] scsiTopologyAdapters =
                        subHostView.Config.StorageDevice.ScsiTopology.Adapter;
                    var mapLunIdNumber = new Dictionary<string, int>();
                    foreach (var scsiTopologyAdapter in scsiTopologyAdapters)
                    {
                        if (scsiTopologyAdapter.Target == null)
                        {
                            continue;
                        }

                        foreach (HostScsiTopologyTarget scsiTopAdpTgt in scsiTopologyAdapter.Target)
                        {
                            foreach (HostScsiTopologyLun scsiTopAdpTgtLun in scsiTopAdpTgt.Lun)
                            {
                                mapLunIdNumber[scsiTopAdpTgtLun.ScsiLun] = scsiTopAdpTgtLun.Lun;
                            }
                        }
                    }

                    HostFileSystemVolumeInfo hfsVolumeInfo = subHostView.Config.FileSystemVolume;
                    foreach (HostFileSystemMountInfo mountInformation in hfsVolumeInfo.MountInfo)
                    {
                        if (string.IsNullOrEmpty(mountInformation.Volume.Name))
                        {
                            continue;
                        }

                        var datastoreInfo = new DataStoreInfo();
                        datastoreInfo.VSphereHostName = subHostView.Name;
                        datastoreInfo.Name = mountInformation.MountInfo.Path;
                        datastoreInfo.SymbolicName = mountInformation.Volume.Name;

                        datastoreInfo.Type = mountInformation.Volume.Type;

                        if (mountInformation.Volume is HostVmfsVolume)
                        {
                            HostVmfsVolume vmfsVolume = mountInformation.Volume as HostVmfsVolume;

                            if (vmfsVolume != null)
                            {
                                datastoreInfo.BlockSize = vmfsVolume.BlockSizeMb;
                                datastoreInfo.FileSystem = vmfsVolume.Version;
                                datastoreInfo.Uuid = vmfsVolume.Uuid;

                                HostScsiDiskPartition[] extents = vmfsVolume.Extent;
                                foreach (HostScsiDiskPartition extent in extents)
                                {
                                    datastoreInfo.DiskName =
                                        extent.DiskName + ":" + extent.Partition + ",";
                                    foreach (ScsiLun scsiLun in scsiLuns)
                                    {
                                        if (scsiLun.CanonicalName == extent.DiskName)
                                        {
                                            datastoreInfo.Vendor = scsiLun.Vendor + ",";
                                            datastoreInfo.DisplayName = scsiLun.DisplayName + ",";
                                            if (mapLunIdNumber.ContainsKey(scsiLun.Key))
                                            {
                                                datastoreInfo.LunNumber =
                                                    mapLunIdNumber[scsiLun.Key] + ",";
                                            }
                                            else
                                            {
                                                T.Tracer().TraceWarning(
                                                    "lun number of datastore: {0} on host: " +
                                                    "{1} is not set.",
                                                    datastoreInfo.DisplayName,
                                                    hostName);
                                            }

                                            break;
                                        }
                                    }
                                }
                            }
                            else
                            {
                                T.Tracer().TraceWarning(
                                    "Skipping some of the information of the VMFS " +
                                    "datastore {0} of the type {1}",
                                    mountInformation.Volume.Name,
                                    mountInformation.Volume.Type);
                            }
                        }
                        else
                        {
                            T.Tracer().TraceWarning(
                                "Skipping some of the information of the datastore {0} " +
                                "of the type {1}",
                                mountInformation.Volume.Name,
                                mountInformation.Volume.Type);
                        }

                        var toTrim = new char[] { ',' };
                        if (datastoreInfo.DiskName != null)
                        {
                            datastoreInfo.DiskName = datastoreInfo.DiskName.TrimEnd(toTrim);
                        }

                        if (datastoreInfo.DisplayName != null)
                        {
                            datastoreInfo.DisplayName = datastoreInfo.DisplayName.TrimEnd(toTrim);
                        }

                        if (datastoreInfo.Vendor != null)
                        {
                            datastoreInfo.Vendor = datastoreInfo.Vendor.TrimEnd(toTrim);
                        }

                        if (datastoreInfo.LunNumber != null)
                        {
                            datastoreInfo.LunNumber = datastoreInfo.LunNumber.TrimEnd(toTrim);
                        }

                        foreach (Datastore item in datastoresMOB)
                        {
                            DatastoreHostMount[] hostInfo = item.Host;
                            foreach (DatastoreHostMount host in hostInfo)
                            {
                                if (host.MountInfo.Path != datastoreInfo.Name ||
                                    host.Key.Value != hostKeyValue)
                                {
                                    continue;
                                }

                                double gb = (double)item.Summary.Capacity / (1024 * 1024 * 1024);
                                datastoreInfo.Capacity = string.Format("{0:0.00}", gb);
                                double freeSpaceGb =
                                    (double)item.Summary.FreeSpace / (1024 * 1024 * 1024);
                                datastoreInfo.FreeSpace = string.Format("{0:0.00}", freeSpaceGb);
                                datastoreInfo.RefId = item.MoRef.Value;

                                dsInfoList.Add(datastoreInfo);
                                break;
                            }
                        }
                    }
                }
                catch (Exception ex)
                {
                    // Bug 6165637
                    T.Tracer().TraceException(
                        ex,
                        "Unexpected exception: Failed to discover the datastores under the host {0}.",
                        hostView.Name);
                }
            }

            return dsInfoList;
        }

        /// <summary>
        /// Creates specfied path on vSphere/vCenter accessed datastores.
        /// </summary>
        /// <param name="pathToCreate">The path to be created which states DatastoreName,
        /// Root Folder and sub sequent Folder or File Name.</param>
        /// <param name="dataCenterName">The data center.</param>
        public void CreatePath(string pathToCreate, string dataCenterName)
        {
            FileManager fileManager = (FileManager)this.client.GetView(
                this.client.ServiceContent.FileManager,
                null);
            Datacenter dataCenterView = this.dataCenterOpts.GetDataCenterView(dataCenterName);
            try
            {
                fileManager.MakeDirectory(
                    pathToCreate,
                    dataCenterView.MoRef,
                    createParentDirectories: true);
            }
            catch (VimException ve)
            {
                if (ve.MethodFault is CannotCreateFile)
                {
                    throw VMwareClientException.FailedToCreatePath(pathToCreate, ve.Message);
                }

                if (ve.MethodFault is FileAlreadyExists)
                {
                    T.Tracer().TraceError("Path {0} already exists.", pathToCreate);
                    throw VMwareClientException.FileAlreadyExists(pathToCreate);
                }

                throw;
            }

            T.Tracer().TraceInformation("Successfully created path {0}.", pathToCreate);
        }

        /// <summary>
        /// Moves/Copies file from remote location to local remote path.
        /// </summary>
        /// <param name="operation">Move or copy.</param>
        /// <param name="dataCenterName">The data center.</param>
        /// <param name="sourceName">The source to move/copy.</param>
        /// <param name="destinationName">The destination.</param>
        /// <param name="force">Force flag.</param>
        public void MoveOrCopyFile(
            MoveOrCopyOp operation,
            string dataCenterName,
            string sourceName,
            string destinationName,
            bool force)
        {
            FileManager fileManager = (FileManager)this.client.GetView(
                this.client.ServiceContent.FileManager,
                null);
            Datacenter dataCenterView = this.dataCenterOpts.GetDataCenterView(dataCenterName);

            try
            {
                ManagedObjectReference task;
                if (operation == MoveOrCopyOp.Copy)
                {
                    task = fileManager.CopyDatastoreFile_Task(
                        sourceName,
                        dataCenterView.MoRef,
                        destinationName,
                        dataCenterView.MoRef,
                        force: true);
                }
                else
                {
                    task = fileManager.MoveDatastoreFile_Task(
                        sourceName,
                        dataCenterView.MoRef,
                        destinationName,
                        dataCenterView.MoRef,
                        force: true);
                }

                Task taskView = (Task)this.client.GetView(task, null);
                bool keepWaiting = true;

                while (keepWaiting)
                {
                    TaskInfo info = taskView.Info;
                    if (info.State == TaskInfoState.running)
                    {
                        T.Tracer().TraceInformation(
                            "{0} of {1} to {2} is in progress.",
                            operation.ToString(),
                            sourceName,
                            destinationName);
                    }
                    else if (info.State == TaskInfoState.success)
                    {
                        T.Tracer().TraceInformation(
                            "{0} of {1} to {2} is successful.",
                            operation.ToString(),
                            sourceName,
                            destinationName);
                        break;
                    }
                    else if (info.State == TaskInfoState.error)
                    {
                        string errorMessage = info.Error.LocalizedMessage;
                        throw VMwareClientException.MoveCopyOperationFailed(
                            sourceName,
                            destinationName,
                            errorMessage);
                    }

                    System.Threading.Thread.Sleep(TimeSpan.FromSeconds(2));
                    taskView.UpdateViewData();
                }
            }
            catch (VimException ve)
            {
                if (ve.MethodFault is FileAlreadyExists)
                {
                    T.Tracer().TraceError("Path {0} already exists.", destinationName);
                    throw VMwareClientException.FileAlreadyExists(destinationName);
                }

                throw;
            }
        }

        /// <summary>
        /// Check whether Datastores in the list are accessible? writeable?.
        /// </summary>
        /// <param name="hostView">The target host view.</param>
        /// <param name="datacenterName">The datacenter name.</param>
        /// <param name="dsInfo">Datastore information.</param>
        public void ValidateDataStores(
            HostSystem hostView,
            string datacenterName,
            DataStoreInfo dsInfo)
        {
            if (!this.IsDataStoreAccessible(hostView, dsInfo.SymbolicName))
            {
                dsInfo.IsAccessible = "No";
                dsInfo.ReadOnly = "Yes";
                throw VMwareClientException.DatastoreNotAccessible(
                    dsInfo.DisplayName,
                    hostView.Name);
            }

            dsInfo.IsAccessible = "Yes";
            List<string> folderFileInfo;
            FolderFileInfo fileInfo;
            this.FindFileInfo(
                hostView,
                "*",
                "[" + dsInfo.SymbolicName + "]",
                "folderfileinfo",
                out folderFileInfo,
                out fileInfo);

            dsInfo.FolderFileInfo = folderFileInfo;
            string folderPath = "[" + dsInfo.SymbolicName + "]" + " InMageDummyTemp";
            if (dsInfo.Type.ToLower() != "vsan")
            {
                int j = 0;
                while (true)
                {
                    try
                    {
                        this.CreatePath(folderPath, datacenterName);
                        dsInfo.ReadOnly = "No";
                        break;
                    }
                    catch (Exception ex)
                    {
                        VMwareClientException vmwareException = ex as VMwareClientException;
                        if (vmwareException != null &&
                            vmwareException.ErrorCode == VMwareClientErrorCode.FileAlreadyExists)
                        {
                            folderPath = folderPath + j.ToString();
                        }
                        else
                        {
                            dsInfo.ReadOnly = "Yes";
                            throw VMwareClientException.DatastoreIsReadonly(dsInfo.DisplayName);
                        }
                    }
                }

                if (dsInfo.ReadOnly.Equals("no", StringComparison.OrdinalIgnoreCase))
                {
                    this.RemovePath(datacenterName, folderPath);
                }
            }
            else
            {
                Datastore store = new Datastore(this.client, null);
                DatastoreNamespaceManager datastoreNamespaceMngr =
                    (DatastoreNamespaceManager)this.client.GetView(
                        this.client.ServiceContent.DatastoreNamespaceManager,
                        null);
                Datacenter datacenterView = this.dataCenterOpts.GetDataCenterView(datacenterName);
                foreach (ManagedObjectReference datastore in datacenterView.Datastore)
                {
                    if (dsInfo.RefId == datastore.Value)
                    {
                        Datastore datastoreView = (Datastore)this.client.GetView(datastore, null);
                            
                        // TODO: (priyanp) create directory using DatastoreNamespaceManager.
                    }
                }
            }
        }
       
        /// <summary>
        /// Searches the datastore for the Specified Path or Lists all the information.
        /// </summary>
        /// <param name="hostView">The host view.</param>
        /// <param name="searchFileOrPath">File or path to be searched.</param>
        /// <param name="searchInPath">Path under which search is to be performed.</param>
        /// <param name="searchType">The search type.</param>
        /// <param name="folderFileInfo">The folder file info </param>
        /// <param name="fileInfo">The file info.</param>
        public void FindFileInfo(
            HostSystem hostView,
            string searchFileOrPath,
            string searchInPath,
            string searchType,
            out List<string> folderFileInfo,
            out FolderFileInfo fileInfo)
        {
            folderFileInfo = new List<string>();

            // TODO (priyanp): Not populating fielInfo object. Is it really needed?
            fileInfo = null;
            HostDatastoreBrowser datastoreBrowser =
                (HostDatastoreBrowser)this.client.GetView(hostView.DatastoreBrowser, null);

            HostDatastoreBrowserSearchSpec hdbs = new HostDatastoreBrowserSearchSpec();
            hdbs.Details = new FileQueryFlags()
                {
                    FileOwner = true,
                    FileType = true,
                    FileSize = true,
                    Modification = true
                };
            hdbs.MatchPattern = new string[] { searchFileOrPath };
            hdbs.SortFoldersFirst = true;
            hdbs.SearchCaseInsensitive = false;

            Task taskView = (Task)this.client.GetView(
                datastoreBrowser.SearchDatastore_Task(searchInPath, hdbs),
                null);

            bool keepWaiting = true;
            while (keepWaiting)
            {
                TaskInfo taskInfo = taskView.Info;
                if (taskInfo.State == TaskInfoState.running)
                {
                    // Keep waiting.
                }
                else if (taskInfo.State == TaskInfoState.success)
                {
                    T.Tracer().TraceInformation(
                        "Files and folder info retrieved on Datastore {0}",
                        searchInPath);
                    break;
                }
                else if (taskInfo.State == TaskInfoState.error)
                {
                    string errorMessage = taskInfo.Error.LocalizedMessage;
                    throw VMwareClientException.FailedToGetFileAndFolderInfoOnDatastoreVimError(
                        searchInPath,
                        errorMessage);
                }

                taskView.UpdateViewData();
            }

            var info = 
                taskView.Info.Result as HostDatastoreBrowserSearchResults;
            if (info != null)
            {
                foreach (FileInfo fInfo in info.File)
                {
                    FolderFileInfo ffInfo = fInfo as FolderFileInfo;
                    if (ffInfo != null)
                    {
                        folderFileInfo.Add(fInfo.Path);
                    }
                }
            }
        }

        /// <summary>
        /// Removes the specified path from datastore.
        /// </summary>
        /// <param name="datacenterName">The datacenter name.</param>
        /// <param name="pathToRemove">The path to be removed.</param>
        private void RemovePath(string datacenterName, string pathToRemove)
        {
            T.Tracer().TraceInformation("Removing path {0}.", pathToRemove);
            FileManager fileManager =
                (FileManager)this.client.GetView(this.client.ServiceContent.FileManager, null);
            Datacenter datacenterView = this.dataCenterOpts.GetDataCenterView(datacenterName);
            fileManager.DeleteDatastoreFile(pathToRemove, datacenterView.MoRef);
        }
        
        /// <summary>
        /// Checks if data store is accessible.
        /// </summary>
        /// <param name="hostView">The target host view.</param>
        /// <param name="datastoreName">The datastore.</param>
        /// <returns>True is datastore is accessible, false otherwise.</returns>
        private bool IsDataStoreAccessible(HostSystem hostView, string datastoreName)
        {
            HostDatastoreBrowser hostDatastoreBrowser =
                (HostDatastoreBrowser)this.client.GetView(hostView.DatastoreBrowser, null);
            T.Tracer().TraceInformation("Checking accessibility of datastore {0}", datastoreName);

            try
            {
                HostDatastoreBrowserSearchResults result =
                    hostDatastoreBrowser.SearchDatastore("[" + datastoreName + "]", null);
            }
            catch (VimException ve)
            {
                T.Tracer().TraceError(
                    "Hit issue during SearchDatastore. Please check with vCenter/ESX user " +
                    "privileges. Message: {0}.",
                    ve.Message);
                return false;
            }

            return true;
        }

        /// <summary>
        /// The data store information that is gathered.
        /// </summary>
        public class DataStoreInfo
        {
            /// <summary>
            /// Gets or sets the block size.
            /// </summary>
            public int BlockSize { get; set; }

            /// <summary>
            /// Gets or sets the capacity.
            /// </summary>
            public string Capacity { get; set; }

            /// <summary>
            /// Gets or sets the disk name.
            /// </summary>
            public string DiskName { get; set; }

            /// <summary>
            /// Gets or sets the display name.
            /// </summary>
            public string DisplayName { get; set; }

            /// <summary>
            /// Gets or sets the file system.
            /// </summary>
            public string FileSystem { get; set; }

            /// <summary>
            /// Gets or sets the free space in GB.
            /// </summary>
            public string FreeSpace { get; set; }

            /// <summary>
            /// Gets or sets the lun number.
            /// </summary>
            public string LunNumber { get; set; }

            /// <summary>
            /// Gets or sets the name.
            /// </summary>
            public string Name { get; set; }

            /// <summary>
            /// Gets or sets the reference id.
            /// </summary>
            public string RefId { get; set; }

            /// <summary>
            /// Gets or sets the symbolic name.
            /// </summary>
            public string SymbolicName { get; set; }

            /// <summary>
            /// Gets or sets the type.
            /// </summary>
            public string Type { get; set; }

            /// <summary>
            /// Gets or sets the uuid.
            /// </summary>
            public string Uuid { get; set; }

            /// <summary>
            /// Gets or sets the vendor.
            /// </summary>
            public string Vendor { get; set; }

            /// <summary>
            /// Gets or sets the vSphere host name.
            /// </summary>
            public string VSphereHostName { get; set; }

            /// <summary>
            /// Gets or sets a value indicating whether datastore is accessible.
            /// </summary>
            public string IsAccessible { get; set; }

            /// <summary>
            /// Gets or sets a value indicating whether datastore is readonly.
            /// </summary>
            public string ReadOnly { get; set; }

            /// <summary>
            /// Gets or sets the folder file info.
            /// </summary>
            public List<string> FolderFileInfo { get; set; }

            /// <summary>
            /// Gets or sets the required space.
            /// </summary>
            public long? RequiredSpace { get; set; }
        }
    }
}
