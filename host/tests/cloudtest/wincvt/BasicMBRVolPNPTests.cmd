@echo OFF

:Main
call :StopShellHWService
call :PNPTests
EXIT /B 0

:StopShellHWService
net stop shellhwdetection & TIMEOUT /T 20 /NOBREAK & sc queryex shellhwdetection | findstr /i /c:"STATE              : 1  STOPPED"
EXIT /B 0

:PNPTests
call :TestCase CreateLogicalWithOffsetInExtendedPartitonFreespace
call :TestCase RemoveDismountWithoutAnyDriveLetter
call :TestCase RemoveDismountWithOneMountPoint
call :TestCase SetOnlyOnePartitionAsActiveAtaTime
call :TestCase SetInactiveOnAnActivePrimaryPartition
call :TestCase InactiveOnAnActiveLogicalPartition
call :TestCase InactiveOnAnExtendedPartitionTest
call :TestCase InactiveOnAnNonActivePrimaryPartitionTest
call :TestCase ExtendWithoutSizeArgumentTests
call :TestCase ExtendLogicalExtendMultipleTimesWithValidSizeTests
call :TestCase ExtendLogicalExtendWithValidSizeTests
call :TestCase ExtendPrimaryLowerBoundery8Tests
call :TestCase ExtendPrimaryLowerBoundery1Tests
call :TestCase ExtendPrimaryPartitionWithSizeTests
call :TestCase ExtendPrimaryPartitionTests
call :TestCase OEMPartitonDeletionWithOffsetTests
call :TestCase OEMPartitonCreationWithOffsetTests
call :TestCase OEMPartitonDeletionSuccessTests
call :TestCase OEMPartitonDeletionFailureTests
call :TestCase OEMPartitonDeletionFailureTests
call :TestCase CreateLinuxPartitionWithOffset
call :TestCase DeleteLinuxPartitionTests
call :TestCase CreateLinuxPartitionTests
call :TestCase CreateLogicalPartitionTests
call :TestCase DeleteExtendedPartitionTests
call :TestCase CreateExtendedPartitionTests
call :TestCase DeletePrimaryPartitionTests
call :TestCase CreateFourPrimaryPartitionTests
call :TestCase ExtendLogicalWithLowerBoundary
call :TestCase SetPrimaryPartitionAsActive
call :TestCase AssignMultipleMountPoints
call :TestCase RemoveDismountWithOneDriveLetter
EXIT /B 0

:TestCase
echo RunTests invoking vstest ManagedPnpTests.dll:%~1
call "%CloudTestWorkerCustomVstestExe%" ManagedPnpTests.dll /Tests:%~1 /Platform:X86 /Logger:TRX /InIsolation /Settings:test.runsettings 2>&1
EXIT /B 0
