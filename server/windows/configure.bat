@echo off

if %1 equ Not_process_server mkdir %2\SystemMonitorRrds

md %2\var 2> NUL


md %2\var\log 2> NUL
"%2\bin\touch" %2\var\log\wtmp
"%2\bin\touch" %2\var\log\xferlog
"%2\bin\touch" %2\var\log\tls.log
