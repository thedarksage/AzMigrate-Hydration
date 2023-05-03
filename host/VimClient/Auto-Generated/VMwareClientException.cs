// +--------------------------------------------------------------
//  Copyright (c) Microsoft Corporation.  All rights reserved.
//
//  Description: This file contains the VMware Client Exception classes code.
//
//  This is a generated file, any changes made will be lost.
//
// ---------------------------------------------------------------

using System;
using System.Collections.Generic;

namespace VMware.Client
{
   public partial class VMwareClientException
   {
      public static Dictionary<VMwareClientErrorCode, Severity> ErrorCodeToSeverityDict =
         new Dictionary<VMwareClientErrorCode, Severity>()
         {
             { VMwareClientErrorCode.HostNotFound, Severity.Error},
             { VMwareClientErrorCode.DataCenterNotFound, Severity.Error},
             { VMwareClientErrorCode.VirtualMachineNotFound, Severity.Error},
             { VMwareClientErrorCode.FailedToCreateSmallDummyDisks, Severity.Error},
             { VMwareClientErrorCode.MultipleVirtualMachinesNotFound, Severity.Error},
             { VMwareClientErrorCode.NoDevicesFound, Severity.Error},
             { VMwareClientErrorCode.UnexpectedSataControllerType, Severity.Error},
             { VMwareClientErrorCode.UnexpectedScsiControllerType, Severity.Error},
             { VMwareClientErrorCode.CannotPowerOffVmMarkedAsTemplate, Severity.Error},
             { VMwareClientErrorCode.CannotPowerOffVmInvalidPowerState, Severity.Error},
             { VMwareClientErrorCode.CannotPowerOffVmInCurrentState, Severity.Error},
             { VMwareClientErrorCode.CannotPowerOffVm, Severity.Error},
             { VMwareClientErrorCode.PowerOffFailedOnVm, Severity.Error},
             { VMwareClientErrorCode.FailedToShutdownVmTimeout, Severity.Error},
             { VMwareClientErrorCode.FailedToPowerOffMasterTarget, Severity.Error},
             { VMwareClientErrorCode.FailedToFindDisksInMasterTarget, Severity.Error},
             { VMwareClientErrorCode.FailedToPowerOnMasterTarget, Severity.Warning},
             { VMwareClientErrorCode.FailedToUpgradeVmOnRecovery, Severity.Warning},
             { VMwareClientErrorCode.FailedToBringUpRecoveredMachine, Severity.Warning},
             { VMwareClientErrorCode.FailedToDetermineDataCenterForMachine, Severity.Warning},
             { VMwareClientErrorCode.NoDiskInformationFoundForMachine, Severity.Warning},
             { VMwareClientErrorCode.UnableToFindDisksOnMasterTarget, Severity.Warning},
             { VMwareClientErrorCode.UnableToSomeDisksOnMasterTarget, Severity.Warning},
             { VMwareClientErrorCode.FailedToDeatchDisksFromMasterTarget, Severity.Error},
             { VMwareClientErrorCode.FailedToPerformNetworkChanges, Severity.Warning},
             { VMwareClientErrorCode.CannotPowerOnVmMarkedAsTemplate, Severity.Error},
             { VMwareClientErrorCode.CannotPowerOnVmInvalidPowerState, Severity.Error},
             { VMwareClientErrorCode.CannotPowerOnVmInCurrentState, Severity.Error},
             { VMwareClientErrorCode.CannotPowerOnVm, Severity.Error},
             { VMwareClientErrorCode.PowerOnFailedOnVm, Severity.Error},
             { VMwareClientErrorCode.AnswerVmConcurrentAccess, Severity.Error},
             { VMwareClientErrorCode.QuestionIdDoesNotApply, Severity.Error},
             { VMwareClientErrorCode.FailedToAnswerQuestion, Severity.Error},
             { VMwareClientErrorCode.FailedToUpgradeVm, Severity.Warning},
             { VMwareClientErrorCode.FailedToPerformNetworkChangesVimError, Severity.Warning},
             { VMwareClientErrorCode.FailedToFindDisksInMasterTargetVimError, Severity.Error},
             { VMwareClientErrorCode.FailedToPowerOnMasterTargetVimError, Severity.Warning},
             { VMwareClientErrorCode.FailedToUpgradeVmOnRecoveryVimError, Severity.Warning},
             { VMwareClientErrorCode.FailedToBringUpRecoveredMachineVimError, Severity.Warning},
             { VMwareClientErrorCode.ResourcePoolNotFound, Severity.Error},
             { VMwareClientErrorCode.ShadowVmCreationFailed, Severity.Error},
             { VMwareClientErrorCode.VmMustBePoweredOff, Severity.Error},
             { VMwareClientErrorCode.UnableToCommunicateWithHost, Severity.Error},
             { VMwareClientErrorCode.OperationNotAllowedInCurrentState, Severity.Error},
             { VMwareClientErrorCode.NotSupported, Severity.Error},
             { VMwareClientErrorCode.InvalidDatastore, Severity.Error},
             { VMwareClientErrorCode.DataCenterMismatch, Severity.Error},
             { VMwareClientErrorCode.InvalidArgument, Severity.Error},
             { VMwareClientErrorCode.MaximumNumberOfVmsExceeded, Severity.Error},
             { VMwareClientErrorCode.AlreadyExists, Severity.Error},
             { VMwareClientErrorCode.NoFreeSlotInMasterTarget, Severity.Error},
             { VMwareClientErrorCode.MoveCopyOperationFailed, Severity.Error},
             { VMwareClientErrorCode.FileAlreadyExists, Severity.Error},
             { VMwareClientErrorCode.FailedToCreatePath, Severity.Error},
             { VMwareClientErrorCode.ReadinessCheckInvalidInput, Severity.Error},
             { VMwareClientErrorCode.VirtualMachineWithPathNameNotFoundInDatacenter, Severity.Error},
             { VMwareClientErrorCode.MultipleVirtualMachinesWithPathNameFoundInDataCenter, Severity.Error},
             { VMwareClientErrorCode.VirtualMachineNotFoundInDatacenter, Severity.Error},
             { VMwareClientErrorCode.MultipleVirtualMachinesFoundInDataCenter, Severity.Error},
             { VMwareClientErrorCode.VirtualMachineIsRunning, Severity.Error},
             { VMwareClientErrorCode.FailedToGetDisksInformationOfMachine, Severity.Error},
             { VMwareClientErrorCode.DatastoreNotAccessible, Severity.Error},
             { VMwareClientErrorCode.DatastoreIsReadonly, Severity.Error},
             { VMwareClientErrorCode.RdmDiskAlreadyExistsAsVmdk, Severity.Error},
             { VMwareClientErrorCode.RdmDiskAlreadyExistsAsRdm, Severity.Error},
             { VMwareClientErrorCode.RdmDiskIsLargerOnSourceThanTarget, Severity.Error},
             { VMwareClientErrorCode.DiskResizeRequired, Severity.Error},
             { VMwareClientErrorCode.NoVirtualMachineFoundUnderDataCenter, Severity.Error},
             { VMwareClientErrorCode.MultipleVirtualMachinesFoundWithUuid, Severity.Error},
             { VMwareClientErrorCode.InsufficientMasterTargetRamSize, Severity.Error},
             { VMwareClientErrorCode.InvalidScsiControllerForLinux, Severity.Error},
             { VMwareClientErrorCode.InsufficientFreeSlotsOnMasterTarget, Severity.Error},
             { VMwareClientErrorCode.DifferentControllersOnMasterTarget, Severity.Error},
             { VMwareClientErrorCode.IdeDiskProtectionNotSupported, Severity.Error},
             { VMwareClientErrorCode.FolderExistsOnDatastore, Severity.Error},
             { VMwareClientErrorCode.FailedToGetFileAndFolderInfoOnDatastoreVimError, Severity.Error},
             { VMwareClientErrorCode.DiskIsInUseByAnotherVm, Severity.Error},
             { VMwareClientErrorCode.InsufficientFreeSpaceOnDatastore, Severity.Error},
             { VMwareClientErrorCode.MachineWithSameNameExistsOnTargetHost, Severity.Error},
             { VMwareClientErrorCode.MachineWithSameVmPathNameExistsOnTargetHost, Severity.Error},
             { VMwareClientErrorCode.MasterTargetCannotAccessDatastore, Severity.Error},
             { VMwareClientErrorCode.VMHostCannotAccessDatastore, Severity.Error},
             { VMwareClientErrorCode.MasterTargetHasDiskWithDuplicateUuid, Severity.Error},
             { VMwareClientErrorCode.DirectoryCreationFailed, Severity.Error},
         };
   }

   public class NamedParameters
   {
       public const string Name = "Name";
       public const string Id = "Id";
       public const string Type = "Type";
       public const string ErrorMessage = "ErrorMessage";
       public const string MtName = "MtName";
       public const string QuestionId = "QuestionId";
       public const string HostName = "HostName";
       public const string Path = "Path";
       public const string Source = "Source";
       public const string Target = "Target";
       public const string InputName = "InputName";
       public const string vmPathName = "vmPathName";
       public const string datacenterName = "datacenterName";
       public const string vmName = "vmName";
       public const string error = "error";
       public const string datastoreName = "datastoreName";
       public const string hostname = "hostname";
       public const string diskName = "diskName";
       public const string targetDiskName = "targetDiskName";
       public const string machineName = "machineName";
       public const string spaceRequired = "spaceRequired";
       public const string Uuid = "Uuid";
       public const string msaterTargetName = "msaterTargetName";
       public const string size = "size";
       public const string masterTargetName = "masterTargetName";
       public const string scsciControllerName = "scsciControllerName";
       public const string freeSlots = "freeSlots";
       public const string requiredSlots = "requiredSlots";
       public const string folderPathName = "folderPathName";
       public const string datastorePath = "datastorePath";
       public const string errorMessage = "errorMessage";
       public const string freeSpace = "freeSpace";
       public const string requiredSpace = "requiredSpace";
       public const string DatastoreName = "DatastoreName";
   }

   public partial class VMwareClientException
   {
      public static Dictionary<VMwareClientErrorCode, List<string>> ErrorCodeToMessageParamsDict =
         new Dictionary<VMwareClientErrorCode, List<string>>()
         {
             {
                 VMwareClientErrorCode.HostNotFound,
                 new List<string>()
                {
                    NamedParameters.Name,
                }
             },
             {
                 VMwareClientErrorCode.DataCenterNotFound,
                 new List<string>()
                {
                    NamedParameters.Name,
                }
             },
             {
                 VMwareClientErrorCode.VirtualMachineNotFound,
                 new List<string>()
                {
                    NamedParameters.Id,
                }
             },
             {
                 VMwareClientErrorCode.FailedToCreateSmallDummyDisks,
                 new List<string>()
             },
             {
                 VMwareClientErrorCode.MultipleVirtualMachinesNotFound,
                 new List<string>()
                {
                    NamedParameters.Id,
                }
             },
             {
                 VMwareClientErrorCode.NoDevicesFound,
                 new List<string>()
                {
                    NamedParameters.Name,
                }
             },
             {
                 VMwareClientErrorCode.UnexpectedSataControllerType,
                 new List<string>()
                {
                    NamedParameters.Type,
                }
             },
             {
                 VMwareClientErrorCode.UnexpectedScsiControllerType,
                 new List<string>()
                {
                    NamedParameters.Type,
                }
             },
             {
                 VMwareClientErrorCode.CannotPowerOffVmMarkedAsTemplate,
                 new List<string>()
                {
                    NamedParameters.Name,
                }
             },
             {
                 VMwareClientErrorCode.CannotPowerOffVmInvalidPowerState,
                 new List<string>()
                {
                    NamedParameters.Name,
                }
             },
             {
                 VMwareClientErrorCode.CannotPowerOffVmInCurrentState,
                 new List<string>()
                {
                    NamedParameters.Name,
                }
             },
             {
                 VMwareClientErrorCode.CannotPowerOffVm,
                 new List<string>()
                {
                    NamedParameters.Name,
                    NamedParameters.ErrorMessage,
                }
             },
             {
                 VMwareClientErrorCode.PowerOffFailedOnVm,
                 new List<string>()
                {
                    NamedParameters.Name,
                    NamedParameters.ErrorMessage,
                }
             },
             {
                 VMwareClientErrorCode.FailedToShutdownVmTimeout,
                 new List<string>()
                {
                    NamedParameters.Name,
                }
             },
             {
                 VMwareClientErrorCode.FailedToPowerOffMasterTarget,
                 new List<string>()
                {
                    NamedParameters.Name,
                    NamedParameters.ErrorMessage,
                }
             },
             {
                 VMwareClientErrorCode.FailedToFindDisksInMasterTarget,
                 new List<string>()
                {
                    NamedParameters.Name,
                    NamedParameters.MtName,
                }
             },
             {
                 VMwareClientErrorCode.FailedToPowerOnMasterTarget,
                 new List<string>()
                {
                    NamedParameters.Name,
                }
             },
             {
                 VMwareClientErrorCode.FailedToUpgradeVmOnRecovery,
                 new List<string>()
                {
                    NamedParameters.Name,
                }
             },
             {
                 VMwareClientErrorCode.FailedToBringUpRecoveredMachine,
                 new List<string>()
                {
                    NamedParameters.Name,
                }
             },
             {
                 VMwareClientErrorCode.FailedToDetermineDataCenterForMachine,
                 new List<string>()
                {
                    NamedParameters.Name,
                    NamedParameters.Id,
                }
             },
             {
                 VMwareClientErrorCode.NoDiskInformationFoundForMachine,
                 new List<string>()
                {
                    NamedParameters.Name,
                }
             },
             {
                 VMwareClientErrorCode.UnableToFindDisksOnMasterTarget,
                 new List<string>()
                {
                    NamedParameters.Name,
                }
             },
             {
                 VMwareClientErrorCode.UnableToSomeDisksOnMasterTarget,
                 new List<string>()
                {
                    NamedParameters.Name,
                }
             },
             {
                 VMwareClientErrorCode.FailedToDeatchDisksFromMasterTarget,
                 new List<string>()
                {
                    NamedParameters.Name,
                    NamedParameters.MtName,
                    NamedParameters.ErrorMessage,
                }
             },
             {
                 VMwareClientErrorCode.FailedToPerformNetworkChanges,
                 new List<string>()
                {
                    NamedParameters.Name,
                }
             },
             {
                 VMwareClientErrorCode.CannotPowerOnVmMarkedAsTemplate,
                 new List<string>()
                {
                    NamedParameters.Name,
                }
             },
             {
                 VMwareClientErrorCode.CannotPowerOnVmInvalidPowerState,
                 new List<string>()
                {
                    NamedParameters.Name,
                }
             },
             {
                 VMwareClientErrorCode.CannotPowerOnVmInCurrentState,
                 new List<string>()
                {
                    NamedParameters.Name,
                }
             },
             {
                 VMwareClientErrorCode.CannotPowerOnVm,
                 new List<string>()
                {
                    NamedParameters.Name,
                    NamedParameters.ErrorMessage,
                }
             },
             {
                 VMwareClientErrorCode.PowerOnFailedOnVm,
                 new List<string>()
                {
                    NamedParameters.Name,
                    NamedParameters.ErrorMessage,
                }
             },
             {
                 VMwareClientErrorCode.AnswerVmConcurrentAccess,
                 new List<string>()
                {
                    NamedParameters.Name,
                }
             },
             {
                 VMwareClientErrorCode.QuestionIdDoesNotApply,
                 new List<string>()
                {
                    NamedParameters.QuestionId,
                    NamedParameters.Name,
                }
             },
             {
                 VMwareClientErrorCode.FailedToAnswerQuestion,
                 new List<string>()
                {
                    NamedParameters.Name,
                    NamedParameters.ErrorMessage,
                }
             },
             {
                 VMwareClientErrorCode.FailedToUpgradeVm,
                 new List<string>()
                {
                    NamedParameters.Name,
                    NamedParameters.ErrorMessage,
                }
             },
             {
                 VMwareClientErrorCode.FailedToPerformNetworkChangesVimError,
                 new List<string>()
                {
                    NamedParameters.Name,
                    NamedParameters.ErrorMessage,
                }
             },
             {
                 VMwareClientErrorCode.FailedToFindDisksInMasterTargetVimError,
                 new List<string>()
                {
                    NamedParameters.Name,
                    NamedParameters.MtName,
                    NamedParameters.ErrorMessage,
                }
             },
             {
                 VMwareClientErrorCode.FailedToPowerOnMasterTargetVimError,
                 new List<string>()
                {
                    NamedParameters.Name,
                    NamedParameters.ErrorMessage,
                }
             },
             {
                 VMwareClientErrorCode.FailedToUpgradeVmOnRecoveryVimError,
                 new List<string>()
                {
                    NamedParameters.Name,
                    NamedParameters.ErrorMessage,
                }
             },
             {
                 VMwareClientErrorCode.FailedToBringUpRecoveredMachineVimError,
                 new List<string>()
                {
                    NamedParameters.Name,
                    NamedParameters.ErrorMessage,
                }
             },
             {
                 VMwareClientErrorCode.ResourcePoolNotFound,
                 new List<string>()
                {
                    NamedParameters.Id,
                }
             },
             {
                 VMwareClientErrorCode.ShadowVmCreationFailed,
                 new List<string>()
                {
                    NamedParameters.Name,
                    NamedParameters.HostName,
                    NamedParameters.ErrorMessage,
                }
             },
             {
                 VMwareClientErrorCode.VmMustBePoweredOff,
                 new List<string>()
                {
                    NamedParameters.Name,
                }
             },
             {
                 VMwareClientErrorCode.UnableToCommunicateWithHost,
                 new List<string>()
                {
                    NamedParameters.Name,
                }
             },
             {
                 VMwareClientErrorCode.OperationNotAllowedInCurrentState,
                 new List<string>()
             },
             {
                 VMwareClientErrorCode.NotSupported,
                 new List<string>()
             },
             {
                 VMwareClientErrorCode.InvalidDatastore,
                 new List<string>()
                {
                    NamedParameters.Path,
                }
             },
             {
                 VMwareClientErrorCode.DataCenterMismatch,
                 new List<string>()
             },
             {
                 VMwareClientErrorCode.InvalidArgument,
                 new List<string>()
             },
             {
                 VMwareClientErrorCode.MaximumNumberOfVmsExceeded,
                 new List<string>()
             },
             {
                 VMwareClientErrorCode.AlreadyExists,
                 new List<string>()
             },
             {
                 VMwareClientErrorCode.NoFreeSlotInMasterTarget,
                 new List<string>()
                {
                    NamedParameters.Name,
                }
             },
             {
                 VMwareClientErrorCode.MoveCopyOperationFailed,
                 new List<string>()
                {
                    NamedParameters.Source,
                    NamedParameters.Target,
                    NamedParameters.ErrorMessage,
                }
             },
             {
                 VMwareClientErrorCode.FileAlreadyExists,
                 new List<string>()
                {
                    NamedParameters.Path,
                }
             },
             {
                 VMwareClientErrorCode.FailedToCreatePath,
                 new List<string>()
                {
                    NamedParameters.Path,
                    NamedParameters.ErrorMessage,
                }
             },
             {
                 VMwareClientErrorCode.ReadinessCheckInvalidInput,
                 new List<string>()
                {
                    NamedParameters.InputName,
                }
             },
             {
                 VMwareClientErrorCode.VirtualMachineWithPathNameNotFoundInDatacenter,
                 new List<string>()
                {
                    NamedParameters.vmPathName,
                    NamedParameters.datacenterName,
                }
             },
             {
                 VMwareClientErrorCode.MultipleVirtualMachinesWithPathNameFoundInDataCenter,
                 new List<string>()
                {
                    NamedParameters.vmPathName,
                    NamedParameters.datacenterName,
                }
             },
             {
                 VMwareClientErrorCode.VirtualMachineNotFoundInDatacenter,
                 new List<string>()
                {
                    NamedParameters.vmName,
                    NamedParameters.datacenterName,
                }
             },
             {
                 VMwareClientErrorCode.MultipleVirtualMachinesFoundInDataCenter,
                 new List<string>()
                {
                    NamedParameters.vmName,
                    NamedParameters.datacenterName,
                }
             },
             {
                 VMwareClientErrorCode.VirtualMachineIsRunning,
                 new List<string>()
                {
                    NamedParameters.vmName,
                }
             },
             {
                 VMwareClientErrorCode.FailedToGetDisksInformationOfMachine,
                 new List<string>()
                {
                    NamedParameters.vmName,
                    NamedParameters.error,
                }
             },
             {
                 VMwareClientErrorCode.DatastoreNotAccessible,
                 new List<string>()
                {
                    NamedParameters.datastoreName,
                    NamedParameters.hostname,
                }
             },
             {
                 VMwareClientErrorCode.DatastoreIsReadonly,
                 new List<string>()
                {
                    NamedParameters.datastoreName,
                }
             },
             {
                 VMwareClientErrorCode.RdmDiskAlreadyExistsAsVmdk,
                 new List<string>()
                {
                    NamedParameters.diskName,
                    NamedParameters.targetDiskName,
                }
             },
             {
                 VMwareClientErrorCode.RdmDiskAlreadyExistsAsRdm,
                 new List<string>()
                {
                    NamedParameters.diskName,
                    NamedParameters.targetDiskName,
                }
             },
             {
                 VMwareClientErrorCode.RdmDiskIsLargerOnSourceThanTarget,
                 new List<string>()
                {
                    NamedParameters.diskName,
                    NamedParameters.targetDiskName,
                }
             },
             {
                 VMwareClientErrorCode.DiskResizeRequired,
                 new List<string>()
                {
                    NamedParameters.targetDiskName,
                    NamedParameters.machineName,
                    NamedParameters.spaceRequired,
                }
             },
             {
                 VMwareClientErrorCode.NoVirtualMachineFoundUnderDataCenter,
                 new List<string>()
                {
                    NamedParameters.datacenterName,
                }
             },
             {
                 VMwareClientErrorCode.MultipleVirtualMachinesFoundWithUuid,
                 new List<string>()
                {
                    NamedParameters.Uuid,
                }
             },
             {
                 VMwareClientErrorCode.InsufficientMasterTargetRamSize,
                 new List<string>()
                {
                    NamedParameters.msaterTargetName,
                    NamedParameters.size,
                }
             },
             {
                 VMwareClientErrorCode.InvalidScsiControllerForLinux,
                 new List<string>()
                {
                    NamedParameters.masterTargetName,
                    NamedParameters.scsciControllerName,
                }
             },
             {
                 VMwareClientErrorCode.InsufficientFreeSlotsOnMasterTarget,
                 new List<string>()
                {
                    NamedParameters.masterTargetName,
                    NamedParameters.freeSlots,
                    NamedParameters.requiredSlots,
                }
             },
             {
                 VMwareClientErrorCode.DifferentControllersOnMasterTarget,
                 new List<string>()
                {
                    NamedParameters.masterTargetName,
                }
             },
             {
                 VMwareClientErrorCode.IdeDiskProtectionNotSupported,
                 new List<string>()
                {
                    NamedParameters.vmName,
                }
             },
             {
                 VMwareClientErrorCode.FolderExistsOnDatastore,
                 new List<string>()
                {
                    NamedParameters.folderPathName,
                    NamedParameters.datastoreName,
                }
             },
             {
                 VMwareClientErrorCode.FailedToGetFileAndFolderInfoOnDatastoreVimError,
                 new List<string>()
                {
                    NamedParameters.datastorePath,
                    NamedParameters.errorMessage,
                }
             },
             {
                 VMwareClientErrorCode.DiskIsInUseByAnotherVm,
                 new List<string>()
                {
                    NamedParameters.diskName,
                    NamedParameters.vmName,
                }
             },
             {
                 VMwareClientErrorCode.InsufficientFreeSpaceOnDatastore,
                 new List<string>()
                {
                    NamedParameters.datastoreName,
                    NamedParameters.freeSpace,
                    NamedParameters.requiredSpace,
                }
             },
             {
                 VMwareClientErrorCode.MachineWithSameNameExistsOnTargetHost,
                 new List<string>()
                {
                    NamedParameters.vmName,
                    NamedParameters.datacenterName,
                }
             },
             {
                 VMwareClientErrorCode.MachineWithSameVmPathNameExistsOnTargetHost,
                 new List<string>()
                {
                    NamedParameters.vmName,
                    NamedParameters.datacenterName,
                }
             },
             {
                 VMwareClientErrorCode.MasterTargetCannotAccessDatastore,
                 new List<string>()
                {
                    NamedParameters.masterTargetName,
                    NamedParameters.vmName,
                }
             },
             {
                 VMwareClientErrorCode.VMHostCannotAccessDatastore,
                 new List<string>()
                {
                    NamedParameters.HostName,
                    NamedParameters.vmName,
                    NamedParameters.DatastoreName,
                }
             },
             {
                 VMwareClientErrorCode.MasterTargetHasDiskWithDuplicateUuid,
                 new List<string>()
                {
                    NamedParameters.masterTargetName,
                    NamedParameters.vmName,
                    NamedParameters.Id,
                }
             },
             {
                 VMwareClientErrorCode.DirectoryCreationFailed,
                 new List<string>()
                {
                    NamedParameters.masterTargetName,
                    NamedParameters.ErrorMessage,
                }
             },
         };
   }


    public partial class VMwareClientException : Exception
    {
        public static VMwareClientException HostNotFound(string Name)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.HostNotFound,
                new Dictionary<string, string>()
                {
                    { NamedParameters.Name, Name },
                });
        }

        public static VMwareClientException DataCenterNotFound(string Name)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.DataCenterNotFound,
                new Dictionary<string, string>()
                {
                    { NamedParameters.Name, Name },
                });
        }

        public static VMwareClientException VirtualMachineNotFound(string Id)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.VirtualMachineNotFound,
                new Dictionary<string, string>()
                {
                    { NamedParameters.Id, Id },
                });
        }

        public static VMwareClientException FailedToCreateSmallDummyDisks()
        {
            return new VMwareClientException(VMwareClientErrorCode.FailedToCreateSmallDummyDisks, null);
        }

        public static VMwareClientException MultipleVirtualMachinesNotFound(string Id)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.MultipleVirtualMachinesNotFound,
                new Dictionary<string, string>()
                {
                    { NamedParameters.Id, Id },
                });
        }

        public static VMwareClientException NoDevicesFound(string Name)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.NoDevicesFound,
                new Dictionary<string, string>()
                {
                    { NamedParameters.Name, Name },
                });
        }

        public static VMwareClientException UnexpectedSataControllerType(string Type)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.UnexpectedSataControllerType,
                new Dictionary<string, string>()
                {
                    { NamedParameters.Type, Type },
                });
        }

        public static VMwareClientException UnexpectedScsiControllerType(string Type)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.UnexpectedScsiControllerType,
                new Dictionary<string, string>()
                {
                    { NamedParameters.Type, Type },
                });
        }

        public static VMwareClientException CannotPowerOffVmMarkedAsTemplate(string Name)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.CannotPowerOffVmMarkedAsTemplate,
                new Dictionary<string, string>()
                {
                    { NamedParameters.Name, Name },
                });
        }

        public static VMwareClientException CannotPowerOffVmInvalidPowerState(string Name)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.CannotPowerOffVmInvalidPowerState,
                new Dictionary<string, string>()
                {
                    { NamedParameters.Name, Name },
                });
        }

        public static VMwareClientException CannotPowerOffVmInCurrentState(string Name)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.CannotPowerOffVmInCurrentState,
                new Dictionary<string, string>()
                {
                    { NamedParameters.Name, Name },
                });
        }

        public static VMwareClientException CannotPowerOffVm(string Name, string ErrorMessage)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.CannotPowerOffVm,
                new Dictionary<string, string>()
                {
                    { NamedParameters.Name, Name },
                    { NamedParameters.ErrorMessage, ErrorMessage },
                });
        }

        public static VMwareClientException PowerOffFailedOnVm(string Name, string ErrorMessage)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.PowerOffFailedOnVm,
                new Dictionary<string, string>()
                {
                    { NamedParameters.Name, Name },
                    { NamedParameters.ErrorMessage, ErrorMessage },
                });
        }

        public static VMwareClientException FailedToShutdownVmTimeout(string Name)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.FailedToShutdownVmTimeout,
                new Dictionary<string, string>()
                {
                    { NamedParameters.Name, Name },
                });
        }

        public static VMwareClientException FailedToPowerOffMasterTarget(string Name, string ErrorMessage)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.FailedToPowerOffMasterTarget,
                new Dictionary<string, string>()
                {
                    { NamedParameters.Name, Name },
                    { NamedParameters.ErrorMessage, ErrorMessage },
                });
        }

        public static VMwareClientException FailedToFindDisksInMasterTarget(string Name, string MtName)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.FailedToFindDisksInMasterTarget,
                new Dictionary<string, string>()
                {
                    { NamedParameters.Name, Name },
                    { NamedParameters.MtName, MtName },
                });
        }

        public static VMwareClientException FailedToPowerOnMasterTarget(string Name)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.FailedToPowerOnMasterTarget,
                new Dictionary<string, string>()
                {
                    { NamedParameters.Name, Name },
                });
        }

        public static VMwareClientException FailedToUpgradeVmOnRecovery(string Name)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.FailedToUpgradeVmOnRecovery,
                new Dictionary<string, string>()
                {
                    { NamedParameters.Name, Name },
                });
        }

        public static VMwareClientException FailedToBringUpRecoveredMachine(string Name)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.FailedToBringUpRecoveredMachine,
                new Dictionary<string, string>()
                {
                    { NamedParameters.Name, Name },
                });
        }

        public static VMwareClientException FailedToDetermineDataCenterForMachine(string Name, string Id)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.FailedToDetermineDataCenterForMachine,
                new Dictionary<string, string>()
                {
                    { NamedParameters.Name, Name },
                    { NamedParameters.Id, Id },
                });
        }

        public static VMwareClientException NoDiskInformationFoundForMachine(string Name)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.NoDiskInformationFoundForMachine,
                new Dictionary<string, string>()
                {
                    { NamedParameters.Name, Name },
                });
        }

        public static VMwareClientException UnableToFindDisksOnMasterTarget(string Name)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.UnableToFindDisksOnMasterTarget,
                new Dictionary<string, string>()
                {
                    { NamedParameters.Name, Name },
                });
        }

        public static VMwareClientException UnableToSomeDisksOnMasterTarget(string Name)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.UnableToSomeDisksOnMasterTarget,
                new Dictionary<string, string>()
                {
                    { NamedParameters.Name, Name },
                });
        }

        public static VMwareClientException FailedToDeatchDisksFromMasterTarget(string Name, string MtName, string ErrorMessage)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.FailedToDeatchDisksFromMasterTarget,
                new Dictionary<string, string>()
                {
                    { NamedParameters.Name, Name },
                    { NamedParameters.MtName, MtName },
                    { NamedParameters.ErrorMessage, ErrorMessage },
                });
        }

        public static VMwareClientException FailedToPerformNetworkChanges(string Name)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.FailedToPerformNetworkChanges,
                new Dictionary<string, string>()
                {
                    { NamedParameters.Name, Name },
                });
        }

        public static VMwareClientException CannotPowerOnVmMarkedAsTemplate(string Name)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.CannotPowerOnVmMarkedAsTemplate,
                new Dictionary<string, string>()
                {
                    { NamedParameters.Name, Name },
                });
        }

        public static VMwareClientException CannotPowerOnVmInvalidPowerState(string Name)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.CannotPowerOnVmInvalidPowerState,
                new Dictionary<string, string>()
                {
                    { NamedParameters.Name, Name },
                });
        }

        public static VMwareClientException CannotPowerOnVmInCurrentState(string Name)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.CannotPowerOnVmInCurrentState,
                new Dictionary<string, string>()
                {
                    { NamedParameters.Name, Name },
                });
        }

        public static VMwareClientException CannotPowerOnVm(string Name, string ErrorMessage)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.CannotPowerOnVm,
                new Dictionary<string, string>()
                {
                    { NamedParameters.Name, Name },
                    { NamedParameters.ErrorMessage, ErrorMessage },
                });
        }

        public static VMwareClientException PowerOnFailedOnVm(string Name, string ErrorMessage)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.PowerOnFailedOnVm,
                new Dictionary<string, string>()
                {
                    { NamedParameters.Name, Name },
                    { NamedParameters.ErrorMessage, ErrorMessage },
                });
        }

        public static VMwareClientException AnswerVmConcurrentAccess(string Name)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.AnswerVmConcurrentAccess,
                new Dictionary<string, string>()
                {
                    { NamedParameters.Name, Name },
                });
        }

        public static VMwareClientException QuestionIdDoesNotApply(string QuestionId, string Name)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.QuestionIdDoesNotApply,
                new Dictionary<string, string>()
                {
                    { NamedParameters.QuestionId, QuestionId },
                    { NamedParameters.Name, Name },
                });
        }

        public static VMwareClientException FailedToAnswerQuestion(string Name, string ErrorMessage)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.FailedToAnswerQuestion,
                new Dictionary<string, string>()
                {
                    { NamedParameters.Name, Name },
                    { NamedParameters.ErrorMessage, ErrorMessage },
                });
        }

        public static VMwareClientException FailedToUpgradeVm(string Name, string ErrorMessage)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.FailedToUpgradeVm,
                new Dictionary<string, string>()
                {
                    { NamedParameters.Name, Name },
                    { NamedParameters.ErrorMessage, ErrorMessage },
                });
        }

        public static VMwareClientException FailedToPerformNetworkChangesVimError(string Name, string ErrorMessage)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.FailedToPerformNetworkChangesVimError,
                new Dictionary<string, string>()
                {
                    { NamedParameters.Name, Name },
                    { NamedParameters.ErrorMessage, ErrorMessage },
                });
        }

        public static VMwareClientException FailedToFindDisksInMasterTargetVimError(string Name, string MtName, string ErrorMessage)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.FailedToFindDisksInMasterTargetVimError,
                new Dictionary<string, string>()
                {
                    { NamedParameters.Name, Name },
                    { NamedParameters.MtName, MtName },
                    { NamedParameters.ErrorMessage, ErrorMessage },
                });
        }

        public static VMwareClientException FailedToPowerOnMasterTargetVimError(string Name, string ErrorMessage)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.FailedToPowerOnMasterTargetVimError,
                new Dictionary<string, string>()
                {
                    { NamedParameters.Name, Name },
                    { NamedParameters.ErrorMessage, ErrorMessage },
                });
        }

        public static VMwareClientException FailedToUpgradeVmOnRecoveryVimError(string Name, string ErrorMessage)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.FailedToUpgradeVmOnRecoveryVimError,
                new Dictionary<string, string>()
                {
                    { NamedParameters.Name, Name },
                    { NamedParameters.ErrorMessage, ErrorMessage },
                });
        }

        public static VMwareClientException FailedToBringUpRecoveredMachineVimError(string Name, string ErrorMessage)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.FailedToBringUpRecoveredMachineVimError,
                new Dictionary<string, string>()
                {
                    { NamedParameters.Name, Name },
                    { NamedParameters.ErrorMessage, ErrorMessage },
                });
        }

        public static VMwareClientException ResourcePoolNotFound(string Id)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.ResourcePoolNotFound,
                new Dictionary<string, string>()
                {
                    { NamedParameters.Id, Id },
                });
        }

        public static VMwareClientException ShadowVmCreationFailed(string Name, string HostName, string ErrorMessage)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.ShadowVmCreationFailed,
                new Dictionary<string, string>()
                {
                    { NamedParameters.Name, Name },
                    { NamedParameters.HostName, HostName },
                    { NamedParameters.ErrorMessage, ErrorMessage },
                });
        }

        public static VMwareClientException VmMustBePoweredOff(string Name)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.VmMustBePoweredOff,
                new Dictionary<string, string>()
                {
                    { NamedParameters.Name, Name },
                });
        }

        public static VMwareClientException UnableToCommunicateWithHost(string Name)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.UnableToCommunicateWithHost,
                new Dictionary<string, string>()
                {
                    { NamedParameters.Name, Name },
                });
        }

        public static VMwareClientException OperationNotAllowedInCurrentState()
        {
            return new VMwareClientException(VMwareClientErrorCode.OperationNotAllowedInCurrentState, null);
        }

        public static VMwareClientException NotSupported()
        {
            return new VMwareClientException(VMwareClientErrorCode.NotSupported, null);
        }

        public static VMwareClientException InvalidDatastore(string Path)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.InvalidDatastore,
                new Dictionary<string, string>()
                {
                    { NamedParameters.Path, Path },
                });
        }

        public static VMwareClientException DataCenterMismatch()
        {
            return new VMwareClientException(VMwareClientErrorCode.DataCenterMismatch, null);
        }

        public static VMwareClientException InvalidArgument()
        {
            return new VMwareClientException(VMwareClientErrorCode.InvalidArgument, null);
        }

        public static VMwareClientException MaximumNumberOfVmsExceeded()
        {
            return new VMwareClientException(VMwareClientErrorCode.MaximumNumberOfVmsExceeded, null);
        }

        public static VMwareClientException AlreadyExists()
        {
            return new VMwareClientException(VMwareClientErrorCode.AlreadyExists, null);
        }

        public static VMwareClientException NoFreeSlotInMasterTarget(string Name)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.NoFreeSlotInMasterTarget,
                new Dictionary<string, string>()
                {
                    { NamedParameters.Name, Name },
                });
        }

        public static VMwareClientException MoveCopyOperationFailed(string Source, string Target, string ErrorMessage)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.MoveCopyOperationFailed,
                new Dictionary<string, string>()
                {
                    { NamedParameters.Source, Source },
                    { NamedParameters.Target, Target },
                    { NamedParameters.ErrorMessage, ErrorMessage },
                });
        }

        public static VMwareClientException FileAlreadyExists(string Path)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.FileAlreadyExists,
                new Dictionary<string, string>()
                {
                    { NamedParameters.Path, Path },
                });
        }

        public static VMwareClientException FailedToCreatePath(string Path, string ErrorMessage)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.FailedToCreatePath,
                new Dictionary<string, string>()
                {
                    { NamedParameters.Path, Path },
                    { NamedParameters.ErrorMessage, ErrorMessage },
                });
        }

        public static VMwareClientException ReadinessCheckInvalidInput(string InputName)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.ReadinessCheckInvalidInput,
                new Dictionary<string, string>()
                {
                    { NamedParameters.InputName, InputName },
                });
        }

        public static VMwareClientException VirtualMachineWithPathNameNotFoundInDatacenter(string vmPathName, string datacenterName)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.VirtualMachineWithPathNameNotFoundInDatacenter,
                new Dictionary<string, string>()
                {
                    { NamedParameters.vmPathName, vmPathName },
                    { NamedParameters.datacenterName, datacenterName },
                });
        }

        public static VMwareClientException MultipleVirtualMachinesWithPathNameFoundInDataCenter(string vmPathName, string datacenterName)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.MultipleVirtualMachinesWithPathNameFoundInDataCenter,
                new Dictionary<string, string>()
                {
                    { NamedParameters.vmPathName, vmPathName },
                    { NamedParameters.datacenterName, datacenterName },
                });
        }

        public static VMwareClientException VirtualMachineNotFoundInDatacenter(string vmName, string datacenterName)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.VirtualMachineNotFoundInDatacenter,
                new Dictionary<string, string>()
                {
                    { NamedParameters.vmName, vmName },
                    { NamedParameters.datacenterName, datacenterName },
                });
        }

        public static VMwareClientException MultipleVirtualMachinesFoundInDataCenter(string vmName, string datacenterName)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.MultipleVirtualMachinesFoundInDataCenter,
                new Dictionary<string, string>()
                {
                    { NamedParameters.vmName, vmName },
                    { NamedParameters.datacenterName, datacenterName },
                });
        }

        public static VMwareClientException VirtualMachineIsRunning(string vmName)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.VirtualMachineIsRunning,
                new Dictionary<string, string>()
                {
                    { NamedParameters.vmName, vmName },
                });
        }

        public static VMwareClientException FailedToGetDisksInformationOfMachine(string vmName, string error)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.FailedToGetDisksInformationOfMachine,
                new Dictionary<string, string>()
                {
                    { NamedParameters.vmName, vmName },
                    { NamedParameters.error, error },
                });
        }

        public static VMwareClientException DatastoreNotAccessible(string datastoreName, string hostname)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.DatastoreNotAccessible,
                new Dictionary<string, string>()
                {
                    { NamedParameters.datastoreName, datastoreName },
                    { NamedParameters.hostname, hostname },
                });
        }

        public static VMwareClientException DatastoreIsReadonly(string datastoreName)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.DatastoreIsReadonly,
                new Dictionary<string, string>()
                {
                    { NamedParameters.datastoreName, datastoreName },
                });
        }

        public static VMwareClientException RdmDiskAlreadyExistsAsVmdk(string diskName, string targetDiskName)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.RdmDiskAlreadyExistsAsVmdk,
                new Dictionary<string, string>()
                {
                    { NamedParameters.diskName, diskName },
                    { NamedParameters.targetDiskName, targetDiskName },
                });
        }

        public static VMwareClientException RdmDiskAlreadyExistsAsRdm(string diskName, string targetDiskName)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.RdmDiskAlreadyExistsAsRdm,
                new Dictionary<string, string>()
                {
                    { NamedParameters.diskName, diskName },
                    { NamedParameters.targetDiskName, targetDiskName },
                });
        }

        public static VMwareClientException RdmDiskIsLargerOnSourceThanTarget(string diskName, string targetDiskName)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.RdmDiskIsLargerOnSourceThanTarget,
                new Dictionary<string, string>()
                {
                    { NamedParameters.diskName, diskName },
                    { NamedParameters.targetDiskName, targetDiskName },
                });
        }

        public static VMwareClientException DiskResizeRequired(string targetDiskName, string machineName, string spaceRequired)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.DiskResizeRequired,
                new Dictionary<string, string>()
                {
                    { NamedParameters.targetDiskName, targetDiskName },
                    { NamedParameters.machineName, machineName },
                    { NamedParameters.spaceRequired, spaceRequired },
                });
        }

        public static VMwareClientException NoVirtualMachineFoundUnderDataCenter(string datacenterName)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.NoVirtualMachineFoundUnderDataCenter,
                new Dictionary<string, string>()
                {
                    { NamedParameters.datacenterName, datacenterName },
                });
        }

        public static VMwareClientException MultipleVirtualMachinesFoundWithUuid(string Uuid)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.MultipleVirtualMachinesFoundWithUuid,
                new Dictionary<string, string>()
                {
                    { NamedParameters.Uuid, Uuid },
                });
        }

        public static VMwareClientException InsufficientMasterTargetRamSize(string msaterTargetName, string size)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.InsufficientMasterTargetRamSize,
                new Dictionary<string, string>()
                {
                    { NamedParameters.msaterTargetName, msaterTargetName },
                    { NamedParameters.size, size },
                });
        }

        public static VMwareClientException InvalidScsiControllerForLinux(string masterTargetName, string scsciControllerName)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.InvalidScsiControllerForLinux,
                new Dictionary<string, string>()
                {
                    { NamedParameters.masterTargetName, masterTargetName },
                    { NamedParameters.scsciControllerName, scsciControllerName },
                });
        }

        public static VMwareClientException InsufficientFreeSlotsOnMasterTarget(string masterTargetName, string freeSlots, string requiredSlots)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.InsufficientFreeSlotsOnMasterTarget,
                new Dictionary<string, string>()
                {
                    { NamedParameters.masterTargetName, masterTargetName },
                    { NamedParameters.freeSlots, freeSlots },
                    { NamedParameters.requiredSlots, requiredSlots },
                });
        }

        public static VMwareClientException DifferentControllersOnMasterTarget(string masterTargetName)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.DifferentControllersOnMasterTarget,
                new Dictionary<string, string>()
                {
                    { NamedParameters.masterTargetName, masterTargetName },
                });
        }

        public static VMwareClientException IdeDiskProtectionNotSupported(string vmName)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.IdeDiskProtectionNotSupported,
                new Dictionary<string, string>()
                {
                    { NamedParameters.vmName, vmName },
                });
        }

        public static VMwareClientException FolderExistsOnDatastore(string folderPathName, string datastoreName)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.FolderExistsOnDatastore,
                new Dictionary<string, string>()
                {
                    { NamedParameters.folderPathName, folderPathName },
                    { NamedParameters.datastoreName, datastoreName },
                });
        }

        public static VMwareClientException FailedToGetFileAndFolderInfoOnDatastoreVimError(string datastorePath, string errorMessage)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.FailedToGetFileAndFolderInfoOnDatastoreVimError,
                new Dictionary<string, string>()
                {
                    { NamedParameters.datastorePath, datastorePath },
                    { NamedParameters.errorMessage, errorMessage },
                });
        }

        public static VMwareClientException DiskIsInUseByAnotherVm(string diskName, string vmName)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.DiskIsInUseByAnotherVm,
                new Dictionary<string, string>()
                {
                    { NamedParameters.diskName, diskName },
                    { NamedParameters.vmName, vmName },
                });
        }

        public static VMwareClientException InsufficientFreeSpaceOnDatastore(string datastoreName, string freeSpace, string requiredSpace)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.InsufficientFreeSpaceOnDatastore,
                new Dictionary<string, string>()
                {
                    { NamedParameters.datastoreName, datastoreName },
                    { NamedParameters.freeSpace, freeSpace },
                    { NamedParameters.requiredSpace, requiredSpace },
                });
        }

        public static VMwareClientException MachineWithSameNameExistsOnTargetHost(string vmName, string datacenterName)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.MachineWithSameNameExistsOnTargetHost,
                new Dictionary<string, string>()
                {
                    { NamedParameters.vmName, vmName },
                    { NamedParameters.datacenterName, datacenterName },
                });
        }

        public static VMwareClientException MachineWithSameVmPathNameExistsOnTargetHost(string vmName, string datacenterName)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.MachineWithSameVmPathNameExistsOnTargetHost,
                new Dictionary<string, string>()
                {
                    { NamedParameters.vmName, vmName },
                    { NamedParameters.datacenterName, datacenterName },
                });
        }

        public static VMwareClientException MasterTargetCannotAccessDatastore(string masterTargetName, string vmName)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.MasterTargetCannotAccessDatastore,
                new Dictionary<string, string>()
                {
                    { NamedParameters.masterTargetName, masterTargetName },
                    { NamedParameters.vmName, vmName },
                });
        }

        public static VMwareClientException VMHostCannotAccessDatastore(string HostName, string vmName, string DatastoreName)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.VMHostCannotAccessDatastore,
                new Dictionary<string, string>()
                {
                    { NamedParameters.HostName, HostName },
                    { NamedParameters.vmName, vmName },
                    { NamedParameters.DatastoreName, DatastoreName },
                });
        }

        public static VMwareClientException MasterTargetHasDiskWithDuplicateUuid(string masterTargetName, string vmName, string Id)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.MasterTargetHasDiskWithDuplicateUuid,
                new Dictionary<string, string>()
                {
                    { NamedParameters.masterTargetName, masterTargetName },
                    { NamedParameters.vmName, vmName },
                    { NamedParameters.Id, Id },
                });
        }

        public static VMwareClientException DirectoryCreationFailed(string masterTargetName, string ErrorMessage)
        {
            return new VMwareClientException(
                VMwareClientErrorCode.DirectoryCreationFailed,
                new Dictionary<string, string>()
                {
                    { NamedParameters.masterTargetName, masterTargetName },
                    { NamedParameters.ErrorMessage, ErrorMessage },
                });
        }

   }
}
