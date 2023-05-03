@echo on

set PATH="C:\strawberry\perl\bin";C:\strawberry\c\bin;"

echo "Installing the UTCFileTime module"  >> C:\thirdparty\perlmoduleinstall.log
cd C:\Temp\Win32-UTCFileTime-1.50
echo "Win32-UTCFileTime-1.50 change directory exitcode: %errorlevel%" >> C:\thirdparty\perlmoduleinstall.log
if %errorlevel% NEQ 0 (
	echo "Unable to change directory to Win32-UTCFileTime-1.50" >> C:\thirdparty\perlmoduleinstall.log
) else (
	echo "Changed the directory to Win32-UTCFileTime-1.50" >> C:\thirdparty\perlmoduleinstall.log
)
echo y | perl Makefile.PL >> C:\thirdparty\perlmoduleinstall.log
echo "Makefile generation exitcode: %errorlevel%" >> C:\thirdparty\perlmoduleinstall.log
dir >> C:\thirdparty\perlmoduleinstall.log
if exist C:\Temp\Win32-UTCFileTime-1.50\Makefile (
    echo "C:\Temp\Win32-UTCFileTime-1.50\Makefile file exists" >> C:\thirdparty\perlmoduleinstall.log
) else (
    echo "C:\Temp\Win32-UTCFileTime-1.50\Makefile file doesn't exists" >> C:\thirdparty\perlmoduleinstall.log
)
dmake -v -S >> C:\thirdparty\perlmoduleinstall.log
echo "dmake exitcode: %errorlevel%" >> C:\thirdparty\perlmoduleinstall.log
dmake -v -S install >> C:\thirdparty\perlmoduleinstall.log
echo "dmake install exitcode: %errorlevel%" >> C:\thirdparty\perlmoduleinstall.log
if %errorlevel% NEQ 0 (
	echo "Unable to install Win32-UTCFileTime-1.50 module." >> C:\thirdparty\perlmoduleinstall.log
) else (
echo "Win32-UTCFileTime-1.50 module installation completed successfully." >> C:\thirdparty\perlmoduleinstall.log
)