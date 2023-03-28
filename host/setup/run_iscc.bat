@echo off
rem Invokes the InnoSetup compiler to build the specified setup script
rem Usage: run_iscc <scriptname.iss>

set ISCC_PATHNAME="C:\Program Files (x86)\Inno Setup 5\ISCC.exe"  

%ISCC_PATHNAME% %1