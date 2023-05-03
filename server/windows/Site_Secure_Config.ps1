Import-Module WebAdministration
Remove-Item -Path "IIS:\SslBindings\*!443";
New-WebBinding -Name "Default Web Site" -IP "*" -Port 443 -Protocol https

# Get SSL certificate  
$SSLthumbprint = (Get-ChildItem Cert:\LocalMachine\My |? {$_.Subject -match "CN=Scout"}).Thumbprint

# Bind certificate to site  
Get-Item Cert:\LocalMachine\my\$SSLthumbprint | New-Item IIS:\SslBindings\*!443