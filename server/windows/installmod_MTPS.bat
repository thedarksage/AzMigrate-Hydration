@echo off

set PATH="C:\WINDOWS";"C:\WINDOWS\system32";"C:\WINDOWS\system32\wbem";"C:\strawberry\perl\bin";C:\strawberry\c\bin;C:\home\svsystems\bin;"

ECHO "Installing the Process module"
cd c:\Temp\Win32-Process-0.14
perl Makefile.PL
dmake
dmake install


ECHO "Installing the Service module"
cd c:\Temp\Win32-Service-0.06
perl Makefile.PL
dmake
dmake install

ECHO "Installing the Registry module"
cd c:\Temp\Win32-Registry-0.12
perl Makefile.PL
dmake
dmake install