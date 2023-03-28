// +--------------------------------------------------------------
//  Copyright (c) Microsoft Corporation.  All rights reserved.
//
//  Description: This file contains a copy of the Error code enums.
//
//  This is a generated file, any changes made will be lost.
//
// ---------------------------------------------------------------

using System;

namespace VMware.Client
{
   /// <summary>
   /// The list of possible ErrorCode string values that might be returned as part of
   /// details that is sent back. An enum's ToString() value would match the
   /// ErrorCode.
   /// </summary>
   public enum VMwareClientErrorCode
   {
        HostNotFound                                                 = 8000,
        DataCenterNotFound                                           = 8001,
        VirtualMachineNotFound                                       = 8002,
        FailedToCreateSmallDummyDisks                                = 8003,
        MultipleVirtualMachinesNotFound                              = 8004,
        NoDevicesFound                                               = 8005,
        UnexpectedSataControllerType                                 = 8006,
        UnexpectedScsiControllerType                                 = 8007,
        CannotPowerOffVmMarkedAsTemplate                             = 8008,
        CannotPowerOffVmInvalidPowerState                            = 8009,
        CannotPowerOffVmInCurrentState                               = 8010,
        CannotPowerOffVm                                             = 8011,
        PowerOffFailedOnVm                                           = 8012,
        FailedToShutdownVmTimeout                                    = 8013,
        FailedToPowerOffMasterTarget                                 = 8014,
        FailedToFindDisksInMasterTarget                              = 8015,
        FailedToPowerOnMasterTarget                                  = 8016,
        FailedToUpgradeVmOnRecovery                                  = 8017,
        FailedToBringUpRecoveredMachine                              = 8018,
        FailedToDetermineDataCenterForMachine                        = 8019,
        NoDiskInformationFoundForMachine                             = 8020,
        UnableToFindDisksOnMasterTarget                              = 8021,
        UnableToSomeDisksOnMasterTarget                              = 8022,
        FailedToDeatchDisksFromMasterTarget                          = 8023,
        FailedToPerformNetworkChanges                                = 8024,
        CannotPowerOnVmMarkedAsTemplate                              = 8025,
        CannotPowerOnVmInvalidPowerState                             = 8026,
        CannotPowerOnVmInCurrentState                                = 8027,
        CannotPowerOnVm                                              = 8028,
        PowerOnFailedOnVm                                            = 8029,
        AnswerVmConcurrentAccess                                     = 8030,
        QuestionIdDoesNotApply                                       = 8031,
        FailedToAnswerQuestion                                       = 8032,
        FailedToUpgradeVm                                            = 8033,
        FailedToPerformNetworkChangesVimError                        = 8034,
        FailedToFindDisksInMasterTargetVimError                      = 8035,
        FailedToPowerOnMasterTargetVimError                          = 8036,
        FailedToUpgradeVmOnRecoveryVimError                          = 8037,
        FailedToBringUpRecoveredMachineVimError                      = 8038,
        ResourcePoolNotFound                                         = 8039,
        ShadowVmCreationFailed                                       = 8040,
        VmMustBePoweredOff                                           = 8041,
        UnableToCommunicateWithHost                                  = 8042,
        OperationNotAllowedInCurrentState                            = 8043,
        NotSupported                                                 = 8044,
        InvalidDatastore                                             = 8045,
        DataCenterMismatch                                           = 8046,
        InvalidArgument                                              = 8047,
        MaximumNumberOfVmsExceeded                                   = 8048,
        AlreadyExists                                                = 8049,
        NoFreeSlotInMasterTarget                                     = 8050,
        MoveCopyOperationFailed                                      = 8051,
        FileAlreadyExists                                            = 8052,
        FailedToCreatePath                                           = 8053,
        ReadinessCheckInvalidInput                                   = 8054,
        VirtualMachineWithPathNameNotFoundInDatacenter               = 8055,
        MultipleVirtualMachinesWithPathNameFoundInDataCenter         = 8056,
        VirtualMachineNotFoundInDatacenter                           = 8057,
        MultipleVirtualMachinesFoundInDataCenter                     = 8058,
        VirtualMachineIsRunning                                      = 8059,
        FailedToGetDisksInformationOfMachine                         = 8060,
        DatastoreNotAccessible                                       = 8061,
        DatastoreIsReadonly                                          = 8062,
        RdmDiskAlreadyExistsAsVmdk                                   = 8063,
        RdmDiskAlreadyExistsAsRdm                                    = 8064,
        RdmDiskIsLargerOnSourceThanTarget                            = 8065,
        DiskResizeRequired                                           = 8066,
        NoVirtualMachineFoundUnderDataCenter                         = 8067,
        MultipleVirtualMachinesFoundWithUuid                         = 8068,
        InsufficientMasterTargetRamSize                              = 8069,
        InvalidScsiControllerForLinux                                = 8070,
        InsufficientFreeSlotsOnMasterTarget                          = 8071,
        DifferentControllersOnMasterTarget                           = 8072,
        IdeDiskProtectionNotSupported                                = 8073,
        FolderExistsOnDatastore                                      = 8074,
        FailedToGetFileAndFolderInfoOnDatastoreVimError              = 8075,
        DiskIsInUseByAnotherVm                                       = 8076,
        InsufficientFreeSpaceOnDatastore                             = 8077,
        MachineWithSameNameExistsOnTargetHost                        = 8078,
        MachineWithSameVmPathNameExistsOnTargetHost                  = 8079,
        MasterTargetCannotAccessDatastore                            = 8080,
        VMHostCannotAccessDatastore                                  = 8081,
        MasterTargetHasDiskWithDuplicateUuid                         = 8082,
        DirectoryCreationFailed                                      = 8083,
   }
}

