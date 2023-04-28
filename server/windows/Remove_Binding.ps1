Import-Module WebAdministration
Remove-Item -Path "IIS:\SslBindings\*!443";
Get-WebBinding -Name "Default Web Site" | Remove-WebBinding
Get-ChildItem -Path cert:\LocalMachine\My -DnsName "NewCertificate" | Remove-Item
Get-ChildItem -Path cert:\LocalMachine\My -DnsName "Scout" | Remove-Item