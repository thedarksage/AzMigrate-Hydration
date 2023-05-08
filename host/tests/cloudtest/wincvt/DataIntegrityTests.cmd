@echo OFF

:Main
echo RunTests invoking vstest DataIntegrityTest.dll
::call :TestCase DIFullDiskTestof1MSizedWrites
call :DITestLargeIOPattern
call :DITestSizedAlignedIOPattern
EXIT /B 0

:DITestLargeIOPattern
call :TestCase DIFullDiskTestof1MSizedWrites
call :TestCase DIFullDiskTestof2MSizedWrites
call :TestCase DIFullDiskTestof3MSizedWrites
call :TestCase DIFullDiskTestof4MSizedWrites
call :TestCase DIFullDiskTestof5MSizedWrites
call :TestCase DIFullDiskTestof6MSizedWrites
call :TestCase DIFullDiskTestof7MSizedWrites
call :TestCase DIFullDiskTestof8MSizedWrites
call :TestCase DIFullDiskTestof9MSizedWrites
call :TestCase DIFullDiskTestof1MSizedUnalignedWrites
call :TestCase DIFullDiskTestof2MSizedUnalignedWrites
call :TestCase DIFullDiskTestof3MSizedUnalignedWrites
call :TestCase DIFullDiskTestof4MSizedUnalignedWrites
call :TestCase DIFullDiskTestof5MSizedUnalignedWrites
call :TestCase DIFullDiskTestof6MSizedUnalignedWrites
call :TestCase DIFullDiskTestof7MSizedUnalignedWrites
call :TestCase DIFullDiskTestof8MSizedUnalignedWrites
call :TestCase DIFullDiskTestof9MSizedUnalignedWrites
EXIT /B 0

:DITestSizedAlignedIOPattern
call :TestCase DIFullDiskTestof256KSizedWrites
call :TestCase DIFullDiskTestof256KSizedUnalignedWrites
call :TestCase DIFullDiskTestof128KSizedWrites
call :TestCase DIFullDiskTestof128KSizedUnalignedWrites
call :TestCase DIFullDiskTestof64KSizedWrites
call :TestCase DIFullDiskTestof64KSizedUnalignedWrites
call :TestCase DIFullDiskTestof32KSizedWrites
call :TestCase DIFullDiskTestof32KSizedUnalignedWrites
call :TestCase DIFullDiskTestof16KSizedWrites
call :TestCase DIFullDiskTestof16KSizedUnalignedWrites
call :TestCase DIFullDiskTestof8KSizedWrites
call :TestCase DIFullDiskTestof8KSizedUnalignedWrites
call :TestCase DIFullDiskTestof4KSizedWrites
call :TestCase DIFullDiskTestof4KSizedUnalignedWrites
::call :TestCase DIFullDiskTestof1KSizedWrites
::call :TestCase DIFullDiskTestof1KSizedUnalignedWrites
EXIT /B 0

:TestCase
echo RunTests invoking vstest DataIntegrityTests.dll:%~1
call "%CloudTestWorkerCustomVstestExe%" DataIntegrityTest.dll /Tests:%~1 /Platform:X86 /Logger:TRX /InIsolation /Settings:test.runsettings 2>&1
EXIT /B 0
