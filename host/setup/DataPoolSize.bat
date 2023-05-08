@echo off 

cd /d %1

GetDataPoolSize.exe > DatapoolsizeBinaryOutput.txt
set DataInMb=%ERRORLEVEL%
set DefaultValue=0
set UpperLimitValue=4096
if /i %DataInMb% GTR  %UpperLimitValue%  goto :OUT	
if /i %DataInMb% GTR  %DefaultValue%     goto :KB

:OUT
exit

:KB
set /a DataInKb=((%DataInMb%)*80/100)*1024 
reg add HKLM\System\CurrentControlSet\Services\InDskFlt\Parameters /v DataPoolSize /t REG_DWORD /d %DataInMb% /f 
reg add HKLM\System\CurrentControlSet\Services\InDskFlt\Parameters /v MaxDeviceDataSizeLimit /t REG_DWORD /d %DataInKb% /f 
