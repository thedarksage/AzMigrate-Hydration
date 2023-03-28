@echo off

rem add need dirs to path

rem make sure to save the old path so it can be reset on uninstall
@echo setx /M PATH "%PATH%" > %systemdrive%\Temp\reset_cxpath.bat

