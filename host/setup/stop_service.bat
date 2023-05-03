@REM First verify whether service is already started or not
@net stop %1 > net_stop_output 2>&1
@find /I "is not started" net_stop_output > nul
@if %ERRORLEVEL% EQU 0 goto GETOUT

@REM Most likely svagents will not stop immediately as it might have work
@REM to do still. Let's keep polling it and see when it dies out...
@REM For this, let's create an on-the-fly Windows scripting file...
@setlocal
@echo Wscript.Sleep 5000 > sleep.vbs
@set NO_OF_WAIT_LOOPS=0

@echo.
@echo Attempting to stop the Sentinel/Outpost service...

:WAIT_FOR_SMOOTH_EXIT
@REM Call "net stop svagents"
@net stop %1 > net_stop_output 2>&1

@REM Keep polling the output of net stop to see if svagents exits
@set /A NO_OF_WAIT_LOOPS=%NO_OF_WAIT_LOOPS%+1 > nul
@if %NO_OF_WAIT_LOOPS% GTR 36 goto GIVE_UP
@start /w wscript.exe sleep.vbs
@find /I "is not started" net_stop_output > nul
@if %ERRORLEVEL% NEQ 0 goto WAIT_FOR_SMOOTH_EXIT
@if %ERRORLEVEL% EQU 0 goto SMOOTH_EXIT

:GETOUT
del net_stop_output
exit 0

:SMOOTH_EXIT
@echo.
@echo Sentinel/Outpost service has been stopped successfully...
@echo Wscript.Sleep 2000 > sleep.vbs
@start /w wscript.exe sleep.vbs
@del /F net_stop_output sleep.vbs
@exit 0

:GIVE_UP
@echo.
@echo Sentinel/Outpost service has not exited gracefully even after 180 seconds. Giving up...
@echo Wscript.Sleep 2000 > sleep.vbs
@start /w wscript.exe sleep.vbs
@del /F net_stop_output sleep.vbs
@exit 1
