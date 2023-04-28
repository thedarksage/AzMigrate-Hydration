@echo off
%windir%\system32\inetsrv\appcmd set config /section:system.webServer/fastCgi /+[fullPath='c:\thirdparty\php5nts\php-cgi.exe']
%windir%\system32\inetsrv\appcmd set config /section:handlers /+[name='PHP_via_FastCGI',path='*.php',modules='FastCgiModule',scriptProcessor='C:\thirdparty\php5nts\php-cgi.exe',verb='*',resourceType='Either']

rem MIME types to the Default Web sites in IIS
%windir%\system32\inetsrv\appcmd.exe set config /section:staticContent /+"[fileExtension='*',mimeType='application/octet-stream']"
%windir%\system32\inetsrv\appcmd.exe set config /section:system.webServer/security/requestFiltering /requestlimits.maxallowedcontentlength:524288000
%windir%\system32\inetsrv\appcmd.exe set apppool /apppool.name:DefaultAppPool /enable32BitAppOnWin64:false
%windir%\system32\inetsrv\appcmd.exe set apppool /apppool.name:DefaultAppPool /managedPipelineMode:Integrated
%windir%\system32\inetsrv\appcmd.exe set apppool /apppool.name:DefaultAppPool /queueLength:10000
%windir%\system32\inetsrv\appcmd.exe set config -section:system.webServer/fastCgi /[fullPath='c:\thirdparty\php5nts\php-cgi.exe'].instanceMaxRequests:10000
%windir%\system32\inetsrv\appcmd.exe set config -section:system.webServer/fastCgi /+"[fullPath='C:\thirdparty\php5nts\php-cgi.exe'].environmentVariables.[name='PHP_FCGI_MAX_REQUESTS',value='10000']"
%windir%\system32\inetsrv\appcmd.exe set config -section:system.webServer/fastCgi /[fullPath='C:\thirdparty\php5nts\php-cgi.exe'].activityTimeout:600
%windir%\system32\inetsrv\appcmd.exe set config -section:system.webServer/fastCgi /[fullPath='C:\thirdparty\php5nts\php-cgi.exe'].requestTimeout:600