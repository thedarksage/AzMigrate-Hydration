@echo off

del /Q Enable_DataFileMode_output.txt 

drvutil.exe --setdriverflags -resetflags -disabledatafiles > Enable_DataFileMode_output.txt 2>&1

drvutil.exe --setvolumeflags %systemdrive% -resetflags -disabledatafiles >> Enable_DataFileMode_output.txt 2>&1

exit


