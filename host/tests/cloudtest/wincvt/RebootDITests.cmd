@echo OFF

:Main
if exist rebootsentinel_2.txt (
  echo %DATE% %TIME%
  echo "RunTests invoking vstest after reboot"

  echo "DI test after Reboot"
  call :VerifyDI

  EXIT /B 0
) else (
  echo %DATE% %TIME%
  echo "RunTests invoking vstest"

  if exist rebootsentinel_1.txt (
    echo "Validate DI after Reboot"
    call :DITestInRebootMode

    echo "Apply data in bitmap mode"
    call :ApplyDataInBitMapMode

    echo Completed > rebootsentinel_2.txt
    EXIT /B 3010
  ) else (
    echo "Apply data in data/metadata mode"
    call :DoInitialSyncAndMatchDI

    echo "Apply data in bitmap mode"
    call :ApplyDataInBitMapMode

    echo Completed > rebootsentinel_1.txt
    EXIT /B 3010
  )

)

:DoInitialSyncAndMatchDI
echo RunTests invoking vstest NoRebootDriverTests.dll:DoInitialSyncAndMatchDI
call "%CloudTestWorkerCustomVstestExe%" NoRebootDriverTests.dll /Tests:DoInitialSyncAndMatchDI /Platform:X86 /Logger:TRX /InIsolation /Settings:test.runsettings 2>&1
EXIT /B 0

:DITestInRebootMode
echo RunTests invoking vstest NoRebootDriverTests.dll:DITestInRebootMode
call "%CloudTestWorkerCustomVstestExe%" NoRebootDriverTests.dll /Tests:DITestInRebootMode /Platform:X86 /Logger:TRX /InIsolation /Settings:test.runsettings 2>&1
EXIT /B 0

:ApplyDataInBitMapMode
echo RunTests invoking vstest NoRebootDriverTests.dll:ApplyDataInBitMapMode
call "%CloudTestWorkerCustomVstestExe%" NoRebootDriverTests.dll /Tests:ApplyDataInBitMapMode /Platform:X86 /Logger:TRX /InIsolation /Settings:test.runsettings 2>&1
EXIT /B 0

:VerifyDI
echo RunTests invoking vstest NoRebootDriverTests.dll:VerifyDI
call "%CloudTestWorkerCustomVstestExe%" NoRebootDriverTests.dll /Tests:VerifyDI /Platform:X86 /Logger:TRX /InIsolation /Settings:test.runsettings 2>&1
EXIT /B 0
