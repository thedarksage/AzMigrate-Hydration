@echo OFF

:Main
call :SimpleVolumeTests
call :SpannedVolumeTests
call :StripedVolumeTests
call :MirroredVolumeTests
call :Raid5VolumeTests
call :LayoutConversionTests
call :OnlineOfflineDiskTests
EXIT /B 0

:SimpleVolumeTests
call :TestCase CreateFourSimpleVolumesInDynamicMBRDisk
call :TestCase ShrinkSimpleVolumeTests
call :TestCase ExtendSimpleVolumeTests
call :TestCase ExtendSimpleVolumeWithoutSizeTests
call :TestCase DeleteSimpleVolumeTests
call :TestCase DeleteSimpleVolumesInDynamicMBRDiskTests
call :TestCase ShrinkAndExtendVolumes
call :TestCase CreateSimpleVolWithoutSizeAndAssignDriveLetter
call :TestCase RemoveDriveLetterFromSimpleVolumeTests
call :TestCase RemoveMultipleDriveLetters
call :TestCase AssignMountPointsToMultipleSimpleVolumes
call :TestCase AssignDriveLettersToMultipleSimpleVolumes
call :TestCase MakeDiskOnlineAndCreateVolume
EXIT /B 0

:SpannedVolumeTests
call :TestCase CreateSpannedVolumeTests
call :TestCase ExtendSpannedVolumeTests
call :TestCase ShrinkSpannedVolumeTests
call :TestCase ShrinkSimpleVolumeAndCreateSpannedVolumeTests
call :TestCase DeleteSpannedVolumeTests
EXIT /B 0

:StripedVolumeTests
call :TestCase CreateStripedVolume
call :TestCase DeleteStripedVolume
EXIT /B 0

:MirroredVolumeTests
call :TestCase AddMirrorToExistingSimpleVolume
call :TestCase DeleteMirroredVolumeTests
call :TestCase RemoveAMirrorFromMirroredVolumesTests
call :TestCase ReconnectFailedMirroredDisk
call :TestCase ReplaceFailedMirrorWithNewMirror
call :TestCase BreakMirroredVolumeTests
EXIT /B 0

:Raid5VolumeTests
call :TestCase CreateRaid5Volume
call :TestCase DeleteRaid5Volume
call :TestCase AssignDriveLetterToRaid5VolumeTests
call :TestCase ReactivateRaid5Disk
call :TestCase RepairRaid5Disk
EXIT /B 0

:LayoutConversionTests
call :TestCase CreateDynamicMBRDisk
call :TestCase ConvertBasicMBRWithVolumesToDynamicMBR
EXIT /B 0

:OnlineOfflineDiskTests
call :TestCase OnlineDynamicMBRDiskTests
call :TestCase OfflineDynamicMBRDiskTests
EXIT /B 0

:TestCase
echo RunTests invoking vstest ManagedPnpTests.dll:%~1
call "%CloudTestWorkerCustomVstestExe%" ManagedPnpTests.dll /Tests:%~1 /Platform:X86 /Logger:TRX /InIsolation /Settings:test.runsettings 2>&1
EXIT /B 0
