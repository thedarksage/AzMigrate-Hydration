
@echo off

rem This script is for discovering and creating consistency  Tag for Oracle Database on Windows
rem Developed by InMage Systems.
rem Version1.0

del /f  c:\temp\drivelist.txt

set ORACLE_HOME=%~1
set ORACLE_SID=%2
set INSTALLATION_PATH=%3
set str=
for %%f in ("Oracle.bat") do set str=%%~dp$path:f
if     "%str%" == "" set path=%path%;%~3
set OPERATION=%4
set Tagname=%5
set APPLICATION_BOOKMARK_MSG="Consistency_Oracle%random%%random%"

if "%ORACLE_HOME%" == "" GOTO USAGE
if "%ORACLE_SID%" == "" GOTO USAGE
if %INSTALLATION_PATH% == "" GOTO USAGE
if "%OPERATION%" == "" GOTO USAGE

if "%OPERATION%" == "failover" GOTO FAILOVER
if "%OPERATION%" == "consistency" GOTO CONSISTENCY
if "%OPERATION%" == "discovery" GOTO DISCOVEROUTPUT


:CONSISTENCY
	mkdir c:\temp 2>>.file
        rem creating pfile on source and keeping it in failover\data folder
	echo conn /as sysdba; > %INSTALLATION_PATH%\consistency\mytest.sql
	if not exist %ORACLE_HOME%\database\SPFILE%ORACLE_SID%.ORA copy /y %ORACLE_HOME%\database\INIT%ORACLE_SID%.ORA %INSTALLATION_PATH%\failover\data\pfile.tmp
	if exist %ORACLE_HOME%\database\SPFILE%ORACLE_SID%.ORA echo create pfile='c:\temp\pfile.tmp' from spfile; >> %INSTALLATION_PATH%\consistency\mytest.sql
	echo exit;>>%INSTALLATION_PATH%\consistency\mytest.sql
	%ORACLE_HOME%\bin\sqlplus /nolog @%3\consistency\mytest.sql
	if exist %ORACLE_HOME%\database\SPFILE%ORACLE_SID%.ORA copy /y c:\temp\pfile.tmp %INSTALLATION_PATH%\failover\data
        set PASSWORDFILE=%ORACLE_HOME%\database\pwd%ORACLE_SID%.ora
	copy /y %PASSWORDFILE% %INSTALLATION_PATH%\failover\data
	%ORACLE_HOME%\bin\sqlplus /nolog @%3\consistency\oracle.sql	

	echo conn /as sysdba >c:\temp\orabegin.sql
	findstr "alter tablespace * begin backup;" c:\temp\orabegin_temp.sql >>c:\temp\orabegin.sql
	echo alter system archive log current;>>c:\temp\orabegin.sql
	echo exit;>> c:\temp\orabegin.sql
	
	echo conn /as sysdba >c:\temp\oraend.sql
	findstr "alter tablespace * begin backup;" c:\temp\oraend_temp.sql >>c:\temp\oraend.sql
	echo exit;>> c:\temp\oraend.sql
	
	echo ****************************************************
	echo Putting Database in Begin backup mode
	echo ****************************************************
	svsleep 2
	%ORACLE_HOME%\bin\sqlplus /nolog @c:\temp\orabegin.sql
	
	echo ****************************************************
	echo Putting Database in Begin backup mode is Done
	echo ****************************************************
	svsleep 2
	
	
	echo set path=%path%;%3 >c:\temp\vacp.bat
	findstr ":" c:\temp\vacp_temp.bat >> c:\temp\vacp.bat
	
	


	echo ****************************************************
	echo Generating Conistency tag on Oracle volumes
	echo ****************************************************
	svsleep 2
	set custom_tag=%APPLICATION_BOOKMARK_MSG%
	vacp -a oracle -v all -t %custom_tag%

	rem call c:\temp\vacp.bat

	echo ****************************************************
	echo Generating Conistency tag on Oracle volumes is Done
	echo ****************************************************
	
	svsleep 2
	echo ****************************************************
	echo Putting Database in End backup 
	echo ****************************************************
	
	%ORACLE_HOME%\bin\sqlplus /nolog @c:\temp\oraend.sql
	
	echo ****************************************************
	echo Putting Database in End backup is done
	echo ****************************************************
	findstr ":" c:\temp\drivelist_temp.txt > c:\temp\drivelist.txt		
	copy c:\temp\drivelist.txt %INSTALLATION_PATH%\failover\data  >c:\temp\.xfile 
	if "%OPERATION%" == "failover" GOTO BACK2FAILOVER
	GOTO END

:DISCOVEROUTPUT
	mkdir c:\temp 2>>.file
	%ORACLE_HOME%\bin\sqlplus /nolog @%3\consistency\oracle.sql	
	echo "Discovery process going on..."
	svsleep 5
	findstr ":" c:\temp\drivelist_temp.txt  > c:\temp\drivelist.txt		
	copy c:\temp\drivelist.txt %INSTALLATION_PATH%\failover\data >c:\temp\.xfile
	echo **********************************************************************************
	echo Oracle is Using following drives.....
	more c:\temp\drivelist.txt
	echo **********************************************************************************
	GOTO END
	
:USAGE

	echo *********************************************************************************
	echo USAGE:
	echo "oracle.bat <ORACLE_HOME> <ORACLE_SID> <INMAGE_VX installatin direcotry> <OPERATION(discovery(or)consistency)(or)failover> <tagname>"
	echo "																		"
	echo "If you choose failover, then you need pass tag name also as an argument... and Tagname is applicable only for failover operation"
	echo *********************************************************************************
	GOTO END
:FAILOVER
	if "%Tagname%" == "" goto FAILOVER_SOURCE
	if NOT "%Tagname%" == "" goto FAILOVER_TARGET
	GOTO END
:FAILOVER_SOURCE

        GOTO CONSISTENCY
:BACK2FAILOVER
	rem cd c:\temp
	rem for /f "tokens=6 delims= " %%i in ('findstr vacp vacp.bat') do set tag=%%i
	echo *****************************************************************************
	echo Please run folowing command on target machine in InMage Vx installation directory
	echo oracle.bat %ORACLE_HOME% %ORACLE_SID%  %INSTALLATION_PATH% failover %custom_tag%
	echo *****************************************************************************                                                                 
	rem cd %INSTALLATION_PATH%
	rem echo %tag% >%INSTALLATION_PATH%\failover\data\ORALatest
	GOTO END 

:FAILOVER_TARGET
        rem copying the pfile into appropriate location
	mkdir c:\temp 2>>.file
	SET PFILE=%ORACLE_HOME%\database\INIT%ORACLE_SID%.ora
        copy /Y %INSTALLATION_PATH%\failover\data\pfile.tmp %PFILE%
	set PASSWORDFILE=%INSTALLATION_PATH%\failover\data\pwd%ORACLE_SID%.ora
	copy /y %PASSWORDFILE% %ORACLE_HOME%\database
	rem Installing Oracle service on target
	%ORACLE_HOME%\bin\oradim -new -sid %ORACLE_SID% -pfile %PFILE% >>.files
        findstr dump %INSTALLATION_PATH%\failover\data\pfile.tmp > %INSTALLATION_PATH%\failover\data\dumpdiretory
	findstr recovery_file_dest= %INSTALLATION_PATH%\failover\data\pfile.tmp >>%INSTALLATION_PATH%\failover\data\dumpdiretory
rem	del %INSTALLATION_PATH%\failover\data\dumpdiretory
rem	for /F "eol=; tokens=2,3* delims='" %%i in ('findstr "dump flash" %INSTALLATION_PATH%\failover\data\pfile.tmp') do @echo %%i >> %INSTALLATION_PATH%\failover\data\dumpdiretory
        copy /y %INSTALLATION_PATH%\failover\data\dumpdiretory c:\temp\dumpdirectory
        cscript %INSTALLATION_PATH%\consistency\oraclefailover.vbs %Tagname% %ORACLE_SID% 
	rem starting of the database at the end.
	echo conn /as sysdba; > %INSTALLATION_PATH%\consistency\mytest.sql
	echo startup mount; >> %INSTALLATION_PATH%\consistency\mytest.sql
        echo recover;>> %INSTALLATION_PATH%\consistency\mytest.sql
	echo alter database open;>> %INSTALLATION_PATH%\consistency\mytest.sql
	echo exit;>>%INSTALLATION_PATH%\consistency\mytest.sql
	%ORACLE_HOME%\bin\sqlplus /nolog @%3\consistency\mytest.sql	

	GOTO END
:END	
