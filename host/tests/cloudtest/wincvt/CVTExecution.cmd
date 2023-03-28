@echo OFF

:Main
call :DataIntegrityTest
call :DynamicVolPNPTest
call :BasicMBRVolPNPTest
call :RebootDITest
exit 0

:RebootDITest
if not exist RebootDITest_Completed.txt (
  echo Calling RebootDITests
  Call RebootDITests.cmd
  if errorlevel 3010 (
    echo Received Error 3010 from RebootDITests
    exit 3010
  ) else (
    echo Completed > RebootDITest_Completed.txt
  )
)
EXIT /B 0

:DataIntegrityTest
if not exist DataIntegrityTest_Completed.txt (
  echo Calling DataIntegrityTests
  Call DataIntegrityTests.cmd
  echo Completed > DataIntegrityTest_Completed.txt
)
EXIT /B 0

:DynamicVolPNPTest
if not exist DynamicVolPNPTest_Completed.txt (
  echo Calling DynamicVolPNPTests
  Call DynamicVolPNPTests.cmd
  echo Completed > DynamicVolPNPTest_Completed.txt
)
EXIT /B 0

:BasicMBRVolPNPTest
if not exist BasicMBRVolPNPTest_Completed.txt (
  echo Calling BasicMBRVolPNPTests
  Call BasicMBRVolPNPTests.cmd
  echo Completed > BasicMBRVolPNPTest_Completed.txt
)
EXIT /B 0
