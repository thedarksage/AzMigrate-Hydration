@ECHO OFF
set PATH=%PATH%;%~dp0\..;%~dp0;
SETLOCAL
SETLOCAL ENABLEDELAYEDEXPANSION

set UnFreezeError=0
set FreezeError=0
set vacpError=0
set UnFreezeSuccess=1
set FreezeSuccess=1

if "%~5"=="" (
goto Usage
)

set Action=%1
set DB2Inst=%2
set DB2Databases=%~3
set Tag=%4
set Volumes=%5

echo.
echo Action going to be performed is %Action%
echo DB2Instance received is %DB2Inst%
echo DB2Databases received is %DB2Databases%
echo Tag name chosen is %Tag%
echo Volumes passed are %Volumes%
echo.

if "%Action%"=="freeze" (
goto Freeze
) Else (
if "%Action%"=="unfreeze" (
goto UnFreeze
) Else (
if "%Action%"=="consistency" (
goto Freeze
) Else (
goto Usage
)))

:Freeze

if "%DB2Inst%"=="ALL" (
for /F "delims=" %%i in ('db2ilist.exe') do (
	SET DB2INSTANCE=%%i
	echo Performing Freezing for the Databases under DB2instance %%i
    
	db2cmd.exe -c -w -i db2 list active databases > out.%%i.txt     
	for /F "tokens=4" %%a in ('type out.%%i.txt ^| findstr /C:"Database name" ') do (
		set DBName=%%a
		echo.
		echo Freezing the Database !DBName! 
		echo db2 connect to !DBName! > cmd.bat
		echo db2 set write suspend for database >> cmd.bat
		echo db2 commit >> cmd.bat
		echo db2 disconnect !DBName! >> cmd.bat
		db2cmd.exe -c -w -i cmd.bat > dbout.txt		
		for /F "delims=" %%b in ('type dbout.txt ^| findstr /C:"WRITE SUSPEND command failed"') do (
			for /F "tokens=2 delims==" %%c in ('type dbout.txt ^| findstr /C:"Reason code ="') do (				
				for /F tokens^=2^ delims^=^" %%d in ('echo %%c') do (
					set FreezeError=%%d
					echo.
					echo Freezeing of %%a is failed with error reason code : !FreezeError!
				)
			)
			echo.
			echo Failed to Freeze the Database !DBName!
			echo.
			echo --------
			echo.
			type dbout.txt 
			echo.
			echo --------
			echo.
			
			set FreezeError=1
			del cmd.bat > nul 2>&1
			del dbout.txt > nul 2>&1
			goto UnFreeze		
		)
		for /F "delims=" %%e in ('type dbout.txt ^| findstr /C:"WRITE command completed successfully."') do (	
			echo.
			echo successfully freezed the database !DBName!	
			echo.
			set FreezeSuccess=0
		)
		if not !FreezeSuccess! EQU 0 (
			echo.
			echo Failed to freeze the database !DBName!	
			echo.
			echo --------
			echo.
			type dbout.txt					
			echo.
			echo --------
			echo.			
		)
		set FreezeSuccess=1
		del cmd.bat > nul 2>&1         
		del dbout.txt > nul 2>&1
    )
    del out.%%i.txt > nul 
)) else (
set DB2INSTANCE=%DB2Inst%
echo Performing Freezing for the Databases under DB2instance %DB2Inst% 
If "%DB2Databases%"=="ALL" (
	db2cmd.exe -c -w -i db2 list active databases > out.%DB2Inst%.txt     
	for /F "tokens=4" %%a in ('type out.%DB2Inst%.txt ^| findstr /C:"Database name" ') do (
		set DBName=%%a
		echo.
		echo Freezing the Database !DBName! 
		echo db2 connect to !DBName! > cmd.bat
		echo db2 set write suspend for database >> cmd.bat
		echo db2 commit >> cmd.bat
		echo db2 disconnect !DBName! >> cmd.bat
		db2cmd.exe -c -w -i cmd.bat> dbout.txt
		for /F "delims=" %%b in ('type dbout.txt ^| findstr /C:"WRITE SUSPEND command failed"') do (
			for /F "tokens=2 delims==" %%c in ('type dbout.txt ^| findstr /C:"Reason code ="') do (				
				for /F tokens^=2^ delims^=^" %%d in ('echo %%c') do (
					set FreezeError=%%d
					echo.
					echo Freezeing of %%a is failed with error reason code : !FreezeError!
				)
			)
			echo.
			echo Failed to Freeze the Database !DBName!
			echo.
			echo --------
			echo.
			type dbout.txt 
			echo.
			echo --------
			echo.
			
			set FreezeError=1
			del cmd.bat > nul 2>&1
			del dbout.txt > nul 2>&1
			goto UnFreeze		
		)
		for /F "delims=" %%e in ('type dbout.txt ^| findstr /C:"WRITE command completed successfully."') do (	
			echo.
			echo successfully freezed the database !DBName!	
			echo.
			set FreezeSuccess=0
		)
		if not !FreezeSuccess! EQU 0 (
			echo.
			echo Failed to freeze the database !DBName!	
			echo.
			echo --------
			echo.
			type dbout.txt					
			echo.
			echo --------
			echo.							
		)
		set FreezeSuccess=1		
		del cmd.bat > nul 2>&1 
        del dbout.txt > nul 2>&1
     )
     del out.%DB2Inst%.txt
) Else (	
	call :FreezeParse "%DB2Databases%"
	goto :eos
	
	:FreezeParse
	set list=%1
	set list=%list:"=%

	FOR /f "tokens=1* delims=," %%a IN ("%list%") DO (
		if not "%%a" == "" (
			set DB2Database=%%a
			echo.
			echo Freezing the Database !DB2Database!
			echo db2 connect to !DB2Database! > cmd.bat
			echo db2 set write suspend for database >> cmd.bat
			echo db2 commit >> cmd.bat
			echo db2 disconnect !DB2Database! >> cmd.bat
			db2cmd.exe -c -w -i cmd.bat > dbout.txt
			for /F "delims=" %%b in ('type dbout.txt ^| findstr /C:"WRITE SUSPEND command failed"') do (
				for /F "tokens=2 delims==" %%c in ('type dbout.txt ^| findstr /C:"Reason code ="') do (				
					for /F tokens^=2^ delims^=^" %%d in ('echo %%c') do (
						set FreezeError=%%d
						echo.
						echo Freezeing of !DB2Database! is failed with error reason code : !FreezeError!
						if !FreezeError! EQU 2 (
							echo.
							echo Freezing the Database !DB2Database! failed as there is some backup operation in progress.				
						)
					)
				)				
				set FreezeError=1					
			)
			for /F "delims=" %%e in ('type dbout.txt ^| findstr /C:"WRITE command completed successfully."') do (	
				echo.
				echo successfully freezed the database !DB2Database!	
				echo.
				set FreezeSuccess=0
			)
			if not !FreezeSuccess! EQU 0 (
				echo.
				echo Failed to freeze the database !DB2Database!
				echo.
				echo --------				
				echo.
				type dbout.txt					
				echo.
				echo --------
				echo.			
			)
			set FreezeSuccess=1
			del cmd.bat > nul 2>&1
			del dbout.txt > nul 2>&1
		)  
		if not "%%b" == "" call :FreezeParse "%%b"
	)
	goto :eos		
))

:eos
if not "%DB2Databases%"=="ALL" (
	endlocal
)
goto EndOfFreeze

:UnFreeze

if "%DB2Inst%"=="ALL" (
for /F "delims=" %%i in ('db2ilist.exe') do (
	SET DB2INSTANCE=%%i
	echo.
	echo Performing UnFreezing for the Databases under DB2instance %%i
	
	db2cmd.exe -c -w -i db2 list db directory > out.%%i.txt	
    for /F "tokens=4" %%a in ('type out.%%i.txt ^| findstr /C:"Database name" ') do (
		set DBName=%%a
		echo db2 get db cfg for !DBName! > cmd.bat
		db2cmd.exe -c -w -i cmd.bat > dbout.txt
		for /F "tokens=2 delims==" %%b in ('type dbout.txt ^| findstr /C:"Database is in write suspend state"') do (
			for /F "tokens=* delims= " %%c in ('echo %%b') do (
				set mode=%%c
			)
			if "!mode!"=="YES" (
				echo.
				echo UnFreezing the Database !DBName! 
				echo db2 connect to !DBName! > cmd.bat
				echo db2 set write resume for database >> cmd.bat
				echo db2 commit >> cmd.bat
				echo db2 disconnect !DBName! >> cmd.bat
				db2cmd.exe -c -w -i cmd.bat > dbout.txt			
				for /F "delims=" %%d in ('type dbout.txt ^| findstr /C:"WRITE RESUME parameter failed"') do (	
					for /F "tokens=2 delims==" %%e in ('type dbout.txt ^| findstr /C:"Reason code ="') do (				
						for /F tokens^=2^ delims^=^" %%f in ('echo %%e') do (
							set UnFreezeError=%%f
							echo.
							echo UnFreezeing of database failed with error reason code : !UnFreezeError!
						)
					)																				
					set UnFreezeError=1						
				)				
				for /F "delims=" %%g in ('type dbout.txt ^| findstr /C:"WRITE command completed successfully."') do (	
					echo.
					echo successfully unfreezed the database !DBName!
					echo.
					set UnFreezeSuccess=0
				)
				if not !UnFreezeSuccess! EQU 0 (
					echo.
					echo Failed to unfreeze the database !DBName!	
					echo.
					echo --------
					echo.
					type dbout.txt	
					echo.
					echo --------
					echo.
					set UnFreezeError=1	
				)
			) else (
				echo.
				echo !DBName! is not in write suspend state, skipping the unfreeze for !DBName!
				echo.
			)	  
		)
		del cmd.bat > nul 2>&1
		del dbout.txt > nul 2>&1
		set UnFreezeSuccess=1
    )	 
    del out.%%i.txt
)) else (
set DB2INSTANCE=%DB2Inst%
echo.
echo Performing UnFreezing for the Databases under DB2instance %DB2Inst%
If "%DB2Databases%"=="ALL" (
    db2cmd.exe -c -w -i db2 list db directory > out.%DB2Inst%.txt	 
    for /F "tokens=4" %%a in ('type out.%DB2Inst%.txt ^| findstr /C:"Database name" ') do (
		set DBName=%%a
		echo db2 get db cfg for !DBName! > cmd.bat
		db2cmd.exe -c -w -i cmd.bat > dbout.txt
		for /F "tokens=2 delims==" %%b in ('type dbout.txt ^| findstr /C:"Database is in write suspend state"') do (	
			for /F "tokens=* delims= " %%c in ('echo %%b') do (
				set mode=%%c
			)			
			if "!mode!"=="YES" (
				echo.
				echo UnFreezing the Database !DBName! 
				echo db2 connect to !DBName! > cmd.bat
				echo db2 set write resume for database >> cmd.bat
				echo db2 commit >> cmd.bat
				echo db2 disconnect !DBName! >> cmd.bat
				db2cmd.exe -c -w -i cmd.bat > dbout.txt
				for /F "delims=" %%d in ('type dbout.txt ^| findstr /C:"WRITE RESUME parameter failed"') do (	
					for /F "tokens=2 delims==" %%e in ('type dbout.txt ^| findstr /C:"Reason code ="') do (				
						for /F tokens^=2^ delims^=^" %%f in ('echo %%e') do (
							set UnFreezeError=%%f
							echo.
							echo UnFreezeing of database failed with error reason code : !UnFreezeError!
							echo.
						)
					)																	
					set UnFreezeError=1					
				)
				for /F "delims=" %%g in ('type dbout.txt ^| findstr /C:"WRITE command completed successfully."') do (	
					echo.
					echo successfully unfreezed the database !DBName!	
					echo.
					set UnFreezeSuccess=0
				)	
				if not !UnFreezeSuccess! EQU 0 (
					echo.
					echo Failed to unfreeze the database !DBName!
					echo.
					echo --------					
					echo.
					type dbout.txt	
					echo.
					echo --------
					echo.
					set UnFreezeError=1	
				)
			)else (
				echo.
				echo !DBName! is not in write suspend state, skipping the unfreeze for !DBName!
				echo.
			)
		) 
		del cmd.bat > nul 2>&1
        del dbout.txt > nul	2>&1	 
		set UnFreezeSuccess=1
    )	 
    del out.%DB2Inst%.txt
) Else (
	call :UnFreezeParse "%DB2Databases%"
	goto :eos1
	
	:UNFreezeParse
	set list=%1
	set list=%list:"=%

	FOR /f "tokens=1* delims=," %%a IN ("%list%") DO (
		if not "%%a" == "" (
			set DB2Database=%%a
			echo.
			echo UnFreezing the Database !DB2Database!
			echo db2 connect to !DB2Database! > cmd.bat
			echo db2 set write resume for database >> cmd.bat
			echo db2 commit >> cmd.bat
			echo db2 disconnect !DB2Database! >> cmd.bat
			db2cmd.exe -c -w -i cmd.bat > dbout.txt
			for /F "delims=" %%b in ('type dbout.txt ^| findstr /C:"WRITE RESUME parameter failed"') do (	
				for /F "tokens=2 delims==" %%c in ('type dbout.txt ^| findstr /C:"Reason code ="') do (				
					for /F tokens^=2^ delims^=^" %%d in ('echo %%c') do (
						set UnFreezeError=%%d
						echo.
						echo UnFreezeing of database !DB2Database! failed with error reason code : !UnFreezeError!
						echo.
					)
				)					
				set UnFreezeError=1	
			)
			for /F "delims=" %%e in ('type dbout.txt ^| findstr /C:"WRITE command completed successfully."') do (
				echo.
				echo successfully unfreezed the database !DB2Database!		
				echo.
				set UnFreezeSuccess=0
			)		
	
			if not !UnFreezeSuccess! EQU 0 (
				echo.
				echo Failed to unfreeze the database !DB2Database!
				echo.
				echo --------
				echo.
				type dbout.txt
				echo.
				echo --------
				echo.
				set UnFreezeError=1	
			)
			set UnFreezeSuccess=1
			del cmd.bat > nul 2>&1     
			del dbout.txt > nul 2>&1
		)  
		if not "%%b" == "" call :UNFreezeParse "%%b"
	)
	goto :eos1	
))

:eos1
if not "%DB2Databases%"=="ALL" (
	endlocal
)
goto EndOfUnfreeze

:EndOfFreeze

if "%Action%"=="consistency" (
	if not %FreezeError% EQU 0 (		
		exit 1
	) 
	vacp.exe -t %Tag% -v %Volumes%	
	if errorlevel 1 (
        echo Failed to issue tags
		set vacpError=1
    )	
	goto UnFreeze	
) else (
	if not %FreezeError% EQU 0 (			
		exit 1
	) 
	exit 0
)

:EndOfUnfreeze

if "%Action%"=="unfreeze" (
	if not %UnFreezeError% EQU 0 (				
		exit 1
	)else (
	 exit 0
	)
) else (    
	if not %FreezeError% EQU 0 (
		exit 1
	) else (
	if not %vacpError% EQU 0 (	
		exit 1
	) else (
	  exit 0
	))	
)
 
:Usage
echo usage: %0 action DBInstance DBNames TagName Volumes
echo.
echo where action can be one of the follwing:
echo consistency		- To issue consistency tag on DB volumes
echo freeze			- To freeze the DBs specified
echo unfreeze		- To unfreeze the DBs specified
echo.
echo DBInstance		- DB2 instance name 
echo				  ALL to freeze databases in all instances
echo DBNames			- DB2 databases 
echo			  	  ALL to freeze all databases in all instances/single instance.
echo			  	  Multiple dbnames can be provides as comma (,) separated and in double quotes.
echo			  	  For example, %0 consistency db2inst1 "testdb1,testdb2" testtag F:
echo TagName			- Name of the tag.
echo				  Should be NONE for freeze and unfreeze actions
echo Volumes			- Volumes for which tag to be issued
echo				  Should be NONE for freeze and unfreeze actions
exit 1