Param
(
	[parameter(Mandatory=$true)]
	[String]$MajorBuildVersion = "9",
	[parameter(Mandatory=$true)]
	[String]$MinorBuildVersion = "25",
	[parameter(Mandatory=$true)]
	[String]$xmlFileName = "",
	[parameter(Mandatory=$true)]
	[String]$credsFileName = ""
)

[xml]$xmlDoc = Get-Content $xmlFileName
[xml]$creds = Get-Content $credsFileName

$Date = Get-Date -UFormat %d
$Month = Get-Date -UFormat %m
$MonthAsString = Get-Date -UFormat %b
$Year = Get-Date -UFormat %Y

$xmlDoc.Configuration.ImageConfig.ImageName = "Microsoft-Azure-Site-Recovery-Process-Server-V2-" + $Year + $Month + "." + $Date
$xmlDoc.Configuration.ImageConfig.ImageVersion = $Year + $Month + "." + $Date + ".00"

$xmlDoc.Configuration.PSConfig.PSVersion = $MajorBuildVersion + "." + $MinorBuildVersion
$xmlDoc.Configuration.PSConfig.PSFullVersion = $MajorBuildVersion + "." + $MinorBuildVersion + ".0.0"
$xmlDoc.Configuration.PSConfig.BuildDate = $Date + "_" + $MonthAsString + "_" + $Year

$xmlDoc.Configuration.ImageConfig.Username = $creds.Credential.Username
$xmlDoc.Configuration.ImageConfig.Password = $creds.Credential.Password

$xmlDoc.Configuration.AzureEnvironment.ApplicationId = $creds.Credential.ApplicationId
$xmlDoc.Configuration.AzureEnvironment.TenantId = $creds.Credential.TenantId

$xmlDoc.Save($xmlFileName)
