%windir%\system32\inetsrv\appcmd set config /section:system.webServer/fastCgi /-[fullPath='c:\php5nts\php-cgi.exe']
%windir%\system32\inetsrv\appcmd set config /section:system.webServer/fastCgi /-[fullPath='c:\thirdparty\php5nts\php-cgi.exe']
%windir%\system32\inetsrv\appcmd set config /section:handlers /-[name='PHP_via_FastCGI'] 
%windir%\system32\inetsrv\appcmd set config /section:staticContent /-"[fileExtension='*',mimeType='application/octet-stream']"