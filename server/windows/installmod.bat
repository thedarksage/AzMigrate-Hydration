@echo on

set PATH="%SystemDrive%\WINDOWS";"%SystemDrive%\WINDOWS\system32";"%SystemDrive%\WINDOWS\system32\wbem";"C:\strawberry\perl\bin";C:\strawberry\c\bin;"

echo "Installing the Win32 module" >> C:\thirdparty\perlmoduleinstall.log
cd C:\Temp\Win32-0.39
echo "Win32-0.39 change directory exitcode: %errorlevel%" >> C:\thirdparty\perlmoduleinstall.log
if %errorlevel% NEQ 0 (
	echo "Unable to change directory to Win32-0.39" >> C:\thirdparty\perlmoduleinstall.log
) else (
	echo "Changed the directory to Win32-0.39" >> C:\thirdparty\perlmoduleinstall.log
)
perl Makefile.PL
echo "Makefile generation exitcode: %errorlevel%" >> C:\thirdparty\perlmoduleinstall.log
if exist C:\Temp\Win32-0.39\Makefile (
    echo "C:\Temp\Win32-0.39\Makefile file exists" >> C:\thirdparty\perlmoduleinstall.log
) else (
    echo "C:\Temp\Win32-0.39\Makefile file doesn't exists" >> C:\thirdparty\perlmoduleinstall.log
)
dmake -v -S >> C:\thirdparty\perlmoduleinstall.log
echo "dmake exitcode: %errorlevel%" >> C:\thirdparty\perlmoduleinstall.log
dmake -v -S install >> C:\thirdparty\perlmoduleinstall.log
echo "dmake install exitcode: %errorlevel%" >> C:\thirdparty\perlmoduleinstall.log
if %errorlevel% NEQ 0 (
	echo "Unable to install Win32-0.39 module." >> C:\thirdparty\perlmoduleinstall.log
) else (
echo "Win32 module installation completed successfully." >> C:\thirdparty\perlmoduleinstall.log
)

echo "Installing the Win32-OLE module" >> C:\thirdparty\perlmoduleinstall.log
cd C:\Temp\Win32-OLE-0.1709
echo "Win32-OLE-0.1709 change directory exitcode: %errorlevel%" >> C:\thirdparty\perlmoduleinstall.log
if %errorlevel% NEQ 0 (
	echo "Unable to change directory to Win32-OLE-0.1709" >> C:\thirdparty\perlmoduleinstall.log
) else (
	echo "Changed the directory to Win32-OLE-0.1709" >> C:\thirdparty\perlmoduleinstall.log
)
perl Makefile.PL
echo "Makefile generation exitcode: %errorlevel%" >> C:\thirdparty\perlmoduleinstall.log
if exist C:\Temp\Win32-OLE-0.1709\Makefile (
    echo "C:\Temp\Win32-OLE-0.1709\Makefile file exists" >> C:\thirdparty\perlmoduleinstall.log
) else (
    echo "C:\Temp\Win32-OLE-0.1709\Makefile file doesn't exists" >> C:\thirdparty\perlmoduleinstall.log
)
dmake -v -S >> C:\thirdparty\perlmoduleinstall.log
echo "dmake exitcode: %errorlevel%" >> C:\thirdparty\perlmoduleinstall.log
dmake -v -S install >> C:\thirdparty\perlmoduleinstall.log
echo "dmake install exitcode: %errorlevel%" >> C:\thirdparty\perlmoduleinstall.log
if %errorlevel% NEQ 0 (
	echo "Unable to install Win32-OLE-0.1709 module." >> C:\thirdparty\perlmoduleinstall.log
) else (
echo "Win32-OLE module installation completed successfully." >> C:\thirdparty\perlmoduleinstall.log
)

echo "Installing the Win32-Process module" >> C:\thirdparty\perlmoduleinstall.log
cd C:\Temp\Win32-Process-0.14
echo "Win32-Process-0.14 change directory exitcode: %errorlevel%" >> C:\thirdparty\perlmoduleinstall.log
if %errorlevel% NEQ 0 (
	echo "Unable to change directory to Win32-Process-0.14" >> C:\thirdparty\perlmoduleinstall.log
) else (
	echo "Changed the directory to Win32-Process-0.14" >> C:\thirdparty\perlmoduleinstall.log
)
perl Makefile.PL
echo "Makefile generation exitcode: %errorlevel%" >> C:\thirdparty\perlmoduleinstall.log
if exist C:\Temp\Win32-Process-0.14\Makefile (
    echo "C:\Temp\Win32-Process-0.14\Makefile file exists" >> C:\thirdparty\perlmoduleinstall.log
) else (
    echo "C:\Temp\Win32-Process-0.14\Makefile file doesn't exists" >> C:\thirdparty\perlmoduleinstall.log
)
dmake -v -S >> C:\thirdparty\perlmoduleinstall.log
echo "dmake exitcode: %errorlevel%" >> C:\thirdparty\perlmoduleinstall.log
dmake -v -S install >> C:\thirdparty\perlmoduleinstall.log
echo "dmake install exitcode: %errorlevel%" >> C:\thirdparty\perlmoduleinstall.log
if %errorlevel% NEQ 0 (
	echo "Unable to install Win32-Process-0.14 module." >> C:\thirdparty\perlmoduleinstall.log
) else (
echo "Win32-Process module installation completed successfully." >> C:\thirdparty\perlmoduleinstall.log
)

echo "Installing the Win32-Service module" >> C:\thirdparty\perlmoduleinstall.log
cd C:\Temp\Win32-Service-0.06
echo "Win32-Service-0.06 change directory exitcode: %errorlevel%" >> C:\thirdparty\perlmoduleinstall.log
if %errorlevel% NEQ 0 (
	echo "Unable to change directory to Win32-Service-0.06" >> C:\thirdparty\perlmoduleinstall.log
) else (
	echo "Changed the directory to Win32-Service-0.06" >> C:\thirdparty\perlmoduleinstall.log
)
perl Makefile.PL
echo "Makefile generation exitcode: %errorlevel%" >> C:\thirdparty\perlmoduleinstall.log
if exist C:\Temp\Win32-Service-0.069\Makefile (
    echo "C:\Temp\Win32-Service-0.06\Makefile file exists" >> C:\thirdparty\perlmoduleinstall.log
) else (
    echo "C:\Temp\Win32-Service-0.06\Makefile file doesn't exists" >> C:\thirdparty\perlmoduleinstall.log
)
dmake -v -S >> C:\thirdparty\perlmoduleinstall.log
echo "dmake exitcode: %errorlevel%" >> C:\thirdparty\perlmoduleinstall.log
dmake -v -S install >> C:\thirdparty\perlmoduleinstall.log
echo "dmake install exitcode: %errorlevel%" >> C:\thirdparty\perlmoduleinstall.log
if %errorlevel% NEQ 0 (
	echo "Unable to install Win32-Service-0.06 module." >> C:\thirdparty\perlmoduleinstall.log
) else (
echo "Win32-Service module installation completed successfully." >> C:\thirdparty\perlmoduleinstall.log
)