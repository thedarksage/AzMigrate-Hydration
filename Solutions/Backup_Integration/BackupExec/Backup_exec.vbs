Dim Backup_execInstDir, InMageInstDir, InMageVsnapDir, Operation, Backup_execSrcDir, Backup_execTgtDir
Set objArgs = WScript.Arguments
Set wshShell = WScript.CreateObject("WScript.Shell")
Set objFSO = CreateObject("Scripting.FileSystemObject")

Const HKEY_LOCAL_MACHINE = &H80000002
Const ForReading = 1, ForWriting = 2, ForAppending = 8

strComputer = "."

' Getting Registry parameter 
Set oReg=GetObject("winmgmts:{impersonationLevel=impersonate}!\\" & _
    strComputer & "\root\default:StdRegProv")

strKeyPath = "SOFTWARE\SV Systems\VxAgent"
strValueName = "InstallDirectory"
oReg.GetStringValue HKEY_LOCAL_MACHINE,strKeyPath,strValueName,InMageInstDir

strKeyPath = "SOFTWARE\Symantec\Backup Exec For Windows\Backup Exec\12.5\Install"
strValueName = "Path"
oReg.GetStringValue HKEY_LOCAL_MACHINE,strKeyPath,strValueName,Backup_execInstDir

LogDir = InMageInstDir & "\Backup_exec_logs"

Backup_execArgs = ""
For i = 0 To objArgs.length - 6
	Backup_execArgs = Backup_execArgs & objArgs(i) & " "
Next


Backup_execArgs = Replace (Backup_execArgs, Chr(39), Chr(34) )

If Not ObjFSO.folderexists(LogDir) Then
	ObjFSO.createfolder(LogDir)
End If

' Getting current time

Set objWMIService = GetObject("winmgmts:" _
 & "{impersonationLevel=impersonate}!\\" & strComputer & "\root\cimv2")
Set colItems = objWMIService.ExecQuery("Select * from Win32_LocalTime")

' Taking Backup through Backup exec
	Wscript.echo(Date & " " & Time & " Backup through Backup exec in progress...")
	RetVal = wshShell.Run( """" & Backup_execInstDir & "bemcmd.exe"" -r -w -la:""" & LogDir & "\Backup_exec_backup.txt""" & " " & Backup_execArgs, 0, true)
	
	If RetVal = 1 Then
	Wscript.echo(Date & " " & Time & " Backup through Backup exec successfully completed")
	Else
	Wscript.echo(Date & " " & Time & " Backup through Backup exec failed for some issue refer " & LogDir & "\Backup_exec_backup.txt file")
	End If
'' SIG '' Begin signature block
'' SIG '' MIIaywYJKoZIhvcNAQcCoIIavDCCGrgCAQExCzAJBgUr
'' SIG '' DgMCGgUAMGcGCisGAQQBgjcCAQSgWTBXMDIGCisGAQQB
'' SIG '' gjcCAR4wJAIBAQQQTvApFpkntU2P5azhDxfrqwIBAAIB
'' SIG '' AAIBAAIBAAIBADAhMAkGBSsOAwIaBQAEFCCHkQaRP3JU
'' SIG '' Salfzf2g2yNuwCvuoIIVgjCCBMMwggOroAMCAQICEzMA
'' SIG '' AABMoehNzLR0ezsAAAAAAEwwDQYJKoZIhvcNAQEFBQAw
'' SIG '' dzELMAkGA1UEBhMCVVMxEzARBgNVBAgTCldhc2hpbmd0
'' SIG '' b24xEDAOBgNVBAcTB1JlZG1vbmQxHjAcBgNVBAoTFU1p
'' SIG '' Y3Jvc29mdCBDb3Jwb3JhdGlvbjEhMB8GA1UEAxMYTWlj
'' SIG '' cm9zb2Z0IFRpbWUtU3RhbXAgUENBMB4XDTEzMTExMTIy
'' SIG '' MTEzMVoXDTE1MDIxMTIyMTEzMVowgbMxCzAJBgNVBAYT
'' SIG '' AlVTMRMwEQYDVQQIEwpXYXNoaW5ndG9uMRAwDgYDVQQH
'' SIG '' EwdSZWRtb25kMR4wHAYDVQQKExVNaWNyb3NvZnQgQ29y
'' SIG '' cG9yYXRpb24xDTALBgNVBAsTBE1PUFIxJzAlBgNVBAsT
'' SIG '' Hm5DaXBoZXIgRFNFIEVTTjpDMEY0LTMwODYtREVGODEl
'' SIG '' MCMGA1UEAxMcTWljcm9zb2Z0IFRpbWUtU3RhbXAgU2Vy
'' SIG '' dmljZTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoC
'' SIG '' ggEBALHY+hsGK3eo5JRdfA/meqaS7opUHaT5hHWFl8zL
'' SIG '' XJbQ13Ut2Qj7W9LuLSXGNz71q34aU+VXvmvov8qWCtxG
'' SIG '' 8VoePgLSsuAmjgBke748k/hYMnmH0hpdI7ycUcQPEPoE
'' SIG '' WLUWdm7svMblvvytrMFB26rOefUcsplBp3olK/+reA1Y
'' SIG '' OrFeUN5kTODKFSrfpun+pGYvWxAJCSYh1D8NL23S+HeQ
'' SIG '' A2zeFBKljOc2H/SHpbBBF2/jTXRmwv2icUY1UcxrF1Fj
'' SIG '' +hWUkppfSyi65hZFSekstf6Lh6/8pW1D3KYw+iko75sN
'' SIG '' LFyD3hKNarTbce9cFFoqIyj/gXBX8YwHmhPYKlMCAwEA
'' SIG '' AaOCAQkwggEFMB0GA1UdDgQWBBS5Da2zTfTanxqyJyZV
'' SIG '' DSBE2Jji9DAfBgNVHSMEGDAWgBQjNPjZUkZwCu1A+3b7
'' SIG '' syuwwzWzDzBUBgNVHR8ETTBLMEmgR6BFhkNodHRwOi8v
'' SIG '' Y3JsLm1pY3Jvc29mdC5jb20vcGtpL2NybC9wcm9kdWN0
'' SIG '' cy9NaWNyb3NvZnRUaW1lU3RhbXBQQ0EuY3JsMFgGCCsG
'' SIG '' AQUFBwEBBEwwSjBIBggrBgEFBQcwAoY8aHR0cDovL3d3
'' SIG '' dy5taWNyb3NvZnQuY29tL3BraS9jZXJ0cy9NaWNyb3Nv
'' SIG '' ZnRUaW1lU3RhbXBQQ0EuY3J0MBMGA1UdJQQMMAoGCCsG
'' SIG '' AQUFBwMIMA0GCSqGSIb3DQEBBQUAA4IBAQAJik4Gr+jt
'' SIG '' gs8dB37XKqckCy2vmlskf5RxDFWIJBpSFWPikE0FSphK
'' SIG '' nPvhp21oVYK5KeppqbLV4wza0dZ6JTd4ZxwM+9spWhqX
'' SIG '' OCo5Vkb7NYG55D1GWo7k/HU3WFlJi07bPBWdc1JL63sM
'' SIG '' OsItwbObUi3gNcW5wVez6D2hPETyIxYeCqpZNyfQlVJe
'' SIG '' qH8/VPCB4dyavWXVePb3TDm73eDWNw6RmoeMc+dxZFL3
'' SIG '' PgPYxs1yuDQ0mFuM0/UIput4xlGgDQ5v9Gs8QBpgFiyp
'' SIG '' BlKdHBOQzm8CHup7nLP2+Jdg8mXR0R+HOsF18EKNeu2M
'' SIG '' crJ7+yyKtJFHVOIuacwWVBpZMIIE7DCCA9SgAwIBAgIT
'' SIG '' MwAAAMps1TISNcThVQABAAAAyjANBgkqhkiG9w0BAQUF
'' SIG '' ADB5MQswCQYDVQQGEwJVUzETMBEGA1UECBMKV2FzaGlu
'' SIG '' Z3RvbjEQMA4GA1UEBxMHUmVkbW9uZDEeMBwGA1UEChMV
'' SIG '' TWljcm9zb2Z0IENvcnBvcmF0aW9uMSMwIQYDVQQDExpN
'' SIG '' aWNyb3NvZnQgQ29kZSBTaWduaW5nIFBDQTAeFw0xNDA0
'' SIG '' MjIxNzM5MDBaFw0xNTA3MjIxNzM5MDBaMIGDMQswCQYD
'' SIG '' VQQGEwJVUzETMBEGA1UECBMKV2FzaGluZ3RvbjEQMA4G
'' SIG '' A1UEBxMHUmVkbW9uZDEeMBwGA1UEChMVTWljcm9zb2Z0
'' SIG '' IENvcnBvcmF0aW9uMQ0wCwYDVQQLEwRNT1BSMR4wHAYD
'' SIG '' VQQDExVNaWNyb3NvZnQgQ29ycG9yYXRpb24wggEiMA0G
'' SIG '' CSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQCWcV3tBkb6
'' SIG '' hMudW7dGx7DhtBE5A62xFXNgnOuntm4aPD//ZeM08aal
'' SIG '' IV5WmWxY5JKhClzC09xSLwxlmiBhQFMxnGyPIX26+f4T
'' SIG '' UFJglTpbuVildGFBqZTgrSZOTKGXcEknXnxnyk8ecYRG
'' SIG '' vB1LtuIPxcYnyQfmegqlFwAZTHBFOC2BtFCqxWfR+nm8
'' SIG '' xcyhcpv0JTSY+FTfEjk4Ei+ka6Wafsdi0dzP7T00+Lnf
'' SIG '' NTC67HkyqeGprFVNTH9MVsMTC3bxB/nMR6z7iNVSpR4o
'' SIG '' +j0tz8+EmIZxZRHPhckJRIbhb+ex/KxARKWpiyM/gkmd
'' SIG '' 1ZZZUBNZGHP/QwytK9R/MEBnAgMBAAGjggFgMIIBXDAT
'' SIG '' BgNVHSUEDDAKBggrBgEFBQcDAzAdBgNVHQ4EFgQUH17i
'' SIG '' XVCNVoa+SjzPBOinh7XLv4MwUQYDVR0RBEowSKRGMEQx
'' SIG '' DTALBgNVBAsTBE1PUFIxMzAxBgNVBAUTKjMxNTk1K2I0
'' SIG '' MjE4ZjEzLTZmY2EtNDkwZi05YzQ3LTNmYzU1N2RmYzQ0
'' SIG '' MDAfBgNVHSMEGDAWgBTLEejK0rQWWAHJNy4zFha5TJoK
'' SIG '' HzBWBgNVHR8ETzBNMEugSaBHhkVodHRwOi8vY3JsLm1p
'' SIG '' Y3Jvc29mdC5jb20vcGtpL2NybC9wcm9kdWN0cy9NaWND
'' SIG '' b2RTaWdQQ0FfMDgtMzEtMjAxMC5jcmwwWgYIKwYBBQUH
'' SIG '' AQEETjBMMEoGCCsGAQUFBzAChj5odHRwOi8vd3d3Lm1p
'' SIG '' Y3Jvc29mdC5jb20vcGtpL2NlcnRzL01pY0NvZFNpZ1BD
'' SIG '' QV8wOC0zMS0yMDEwLmNydDANBgkqhkiG9w0BAQUFAAOC
'' SIG '' AQEAd1zr15E9zb17g9mFqbBDnXN8F8kP7Tbbx7UsG177
'' SIG '' VAU6g3FAgQmit3EmXtZ9tmw7yapfXQMYKh0nfgfpxWUf
'' SIG '' tc8Nt1THKDhaiOd7wRm2VjK64szLk9uvbg9dRPXUsO8b
'' SIG '' 1U7Brw7vIJvy4f4nXejF/2H2GdIoCiKd381wgp4Yctgj
'' SIG '' zHosQ+7/6sDg5h2qnpczAFJvB7jTiGzepAY1p8JThmUR
'' SIG '' dwmPNVm52IaoAP74MX0s9IwFncDB1XdybOlNWSaD8cKy
'' SIG '' iFeTNQB8UCu8Wfz+HCk4gtPeUpdFKRhOlludul8bo/En
'' SIG '' UOoHlehtNA04V9w3KDWVOjic1O1qhV0OIhFeezCCBbww
'' SIG '' ggOkoAMCAQICCmEzJhoAAAAAADEwDQYJKoZIhvcNAQEF
'' SIG '' BQAwXzETMBEGCgmSJomT8ixkARkWA2NvbTEZMBcGCgmS
'' SIG '' JomT8ixkARkWCW1pY3Jvc29mdDEtMCsGA1UEAxMkTWlj
'' SIG '' cm9zb2Z0IFJvb3QgQ2VydGlmaWNhdGUgQXV0aG9yaXR5
'' SIG '' MB4XDTEwMDgzMTIyMTkzMloXDTIwMDgzMTIyMjkzMlow
'' SIG '' eTELMAkGA1UEBhMCVVMxEzARBgNVBAgTCldhc2hpbmd0
'' SIG '' b24xEDAOBgNVBAcTB1JlZG1vbmQxHjAcBgNVBAoTFU1p
'' SIG '' Y3Jvc29mdCBDb3Jwb3JhdGlvbjEjMCEGA1UEAxMaTWlj
'' SIG '' cm9zb2Z0IENvZGUgU2lnbmluZyBQQ0EwggEiMA0GCSqG
'' SIG '' SIb3DQEBAQUAA4IBDwAwggEKAoIBAQCycllcGTBkvx2a
'' SIG '' YCAgQpl2U2w+G9ZvzMvx6mv+lxYQ4N86dIMaty+gMuz/
'' SIG '' 3sJCTiPVcgDbNVcKicquIEn08GisTUuNpb15S3GbRwfa
'' SIG '' /SXfnXWIz6pzRH/XgdvzvfI2pMlcRdyvrT3gKGiXGqel
'' SIG '' cnNW8ReU5P01lHKg1nZfHndFg4U4FtBzWwW6Z1KNpbJp
'' SIG '' L9oZC/6SdCnidi9U3RQwWfjSjWL9y8lfRjFQuScT5EAw
'' SIG '' z3IpECgixzdOPaAyPZDNoTgGhVxOVoIoKgUyt0vXT2Pn
'' SIG '' 0i1i8UU956wIAPZGoZ7RW4wmU+h6qkryRs83PDietHdc
'' SIG '' pReejcsRj1Y8wawJXwPTAgMBAAGjggFeMIIBWjAPBgNV
'' SIG '' HRMBAf8EBTADAQH/MB0GA1UdDgQWBBTLEejK0rQWWAHJ
'' SIG '' Ny4zFha5TJoKHzALBgNVHQ8EBAMCAYYwEgYJKwYBBAGC
'' SIG '' NxUBBAUCAwEAATAjBgkrBgEEAYI3FQIEFgQU/dExTtMm
'' SIG '' ipXhmGA7qDFvpjy82C0wGQYJKwYBBAGCNxQCBAweCgBT
'' SIG '' AHUAYgBDAEEwHwYDVR0jBBgwFoAUDqyCYEBWJ5flJRP8
'' SIG '' KuEKU5VZ5KQwUAYDVR0fBEkwRzBFoEOgQYY/aHR0cDov
'' SIG '' L2NybC5taWNyb3NvZnQuY29tL3BraS9jcmwvcHJvZHVj
'' SIG '' dHMvbWljcm9zb2Z0cm9vdGNlcnQuY3JsMFQGCCsGAQUF
'' SIG '' BwEBBEgwRjBEBggrBgEFBQcwAoY4aHR0cDovL3d3dy5t
'' SIG '' aWNyb3NvZnQuY29tL3BraS9jZXJ0cy9NaWNyb3NvZnRS
'' SIG '' b290Q2VydC5jcnQwDQYJKoZIhvcNAQEFBQADggIBAFk5
'' SIG '' Pn8mRq/rb0CxMrVq6w4vbqhJ9+tfde1MOy3XQ60L/svp
'' SIG '' LTGjI8x8UJiAIV2sPS9MuqKoVpzjcLu4tPh5tUly9z7q
'' SIG '' QX/K4QwXaculnCAt+gtQxFbNLeNK0rxw56gNogOlVuC4
'' SIG '' iktX8pVCnPHz7+7jhh80PLhWmvBTI4UqpIIck+KUBx3y
'' SIG '' 4k74jKHK6BOlkU7IG9KPcpUqcW2bGvgc8FPWZ8wi/1wd
'' SIG '' zaKMvSeyeWNWRKJRzfnpo1hW3ZsCRUQvX/TartSCMm78
'' SIG '' pJUT5Otp56miLL7IKxAOZY6Z2/Wi+hImCWU4lPF6H0q7
'' SIG '' 0eFW6NB4lhhcyTUWX92THUmOLb6tNEQc7hAVGgBd3TVb
'' SIG '' Ic6YxwnuhQ6MT20OE049fClInHLR82zKwexwo1eSV32U
'' SIG '' jaAbSANa98+jZwp0pTbtLS8XyOZyNxL0b7E8Z4L5UrKN
'' SIG '' MxZlHg6K3RDeZPRvzkbU0xfpecQEtNP7LN8fip6sCvsT
'' SIG '' J0Ct5PnhqX9GuwdgR2VgQE6wQuxO7bN2edgKNAltHIAx
'' SIG '' H+IOVN3lofvlRxCtZJj/UBYufL8FIXrilUEnacOTj5XJ
'' SIG '' jdibIa4NXJzwoq6GaIMMai27dmsAHZat8hZ79haDJLmI
'' SIG '' z2qoRzEvmtzjcT3XAH5iR9HOiMm4GPoOco3Boz2vAkBq
'' SIG '' /2mbluIQqBC0N1AI1sM9MIIGBzCCA++gAwIBAgIKYRZo
'' SIG '' NAAAAAAAHDANBgkqhkiG9w0BAQUFADBfMRMwEQYKCZIm
'' SIG '' iZPyLGQBGRYDY29tMRkwFwYKCZImiZPyLGQBGRYJbWlj
'' SIG '' cm9zb2Z0MS0wKwYDVQQDEyRNaWNyb3NvZnQgUm9vdCBD
'' SIG '' ZXJ0aWZpY2F0ZSBBdXRob3JpdHkwHhcNMDcwNDAzMTI1
'' SIG '' MzA5WhcNMjEwNDAzMTMwMzA5WjB3MQswCQYDVQQGEwJV
'' SIG '' UzETMBEGA1UECBMKV2FzaGluZ3RvbjEQMA4GA1UEBxMH
'' SIG '' UmVkbW9uZDEeMBwGA1UEChMVTWljcm9zb2Z0IENvcnBv
'' SIG '' cmF0aW9uMSEwHwYDVQQDExhNaWNyb3NvZnQgVGltZS1T
'' SIG '' dGFtcCBQQ0EwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAw
'' SIG '' ggEKAoIBAQCfoWyx39tIkip8ay4Z4b3i48WZUSNQrc7d
'' SIG '' GE4kD+7Rp9FMrXQwIBHrB9VUlRVJlBtCkq6YXDAm2gBr
'' SIG '' 6Hu97IkHD/cOBJjwicwfyzMkh53y9GccLPx754gd6udO
'' SIG '' o6HBI1PKjfpFzwnQXq/QsEIEovmmbJNn1yjcRlOwhtDl
'' SIG '' KEYuJ6yGT1VSDOQDLPtqkJAwbofzWTCd+n7Wl7PoIZd+
'' SIG '' +NIT8wi3U21StEWQn0gASkdmEScpZqiX5NMGgUqi+YSn
'' SIG '' EUcUCYKfhO1VeP4Bmh1QCIUAEDBG7bfeI0a7xC1Un68e
'' SIG '' eEExd8yb3zuDk6FhArUdDbH895uyAc4iS1T/+QXDwiAL
'' SIG '' AgMBAAGjggGrMIIBpzAPBgNVHRMBAf8EBTADAQH/MB0G
'' SIG '' A1UdDgQWBBQjNPjZUkZwCu1A+3b7syuwwzWzDzALBgNV
'' SIG '' HQ8EBAMCAYYwEAYJKwYBBAGCNxUBBAMCAQAwgZgGA1Ud
'' SIG '' IwSBkDCBjYAUDqyCYEBWJ5flJRP8KuEKU5VZ5KShY6Rh
'' SIG '' MF8xEzARBgoJkiaJk/IsZAEZFgNjb20xGTAXBgoJkiaJ
'' SIG '' k/IsZAEZFgltaWNyb3NvZnQxLTArBgNVBAMTJE1pY3Jv
'' SIG '' c29mdCBSb290IENlcnRpZmljYXRlIEF1dGhvcml0eYIQ
'' SIG '' ea0WoUqgpa1Mc1j0BxMuZTBQBgNVHR8ESTBHMEWgQ6BB
'' SIG '' hj9odHRwOi8vY3JsLm1pY3Jvc29mdC5jb20vcGtpL2Ny
'' SIG '' bC9wcm9kdWN0cy9taWNyb3NvZnRyb290Y2VydC5jcmww
'' SIG '' VAYIKwYBBQUHAQEESDBGMEQGCCsGAQUFBzAChjhodHRw
'' SIG '' Oi8vd3d3Lm1pY3Jvc29mdC5jb20vcGtpL2NlcnRzL01p
'' SIG '' Y3Jvc29mdFJvb3RDZXJ0LmNydDATBgNVHSUEDDAKBggr
'' SIG '' BgEFBQcDCDANBgkqhkiG9w0BAQUFAAOCAgEAEJeKw1wD
'' SIG '' RDbd6bStd9vOeVFNAbEudHFbbQwTq86+e4+4LtQSooxt
'' SIG '' YrhXAstOIBNQmd16QOJXu69YmhzhHQGGrLt48ovQ7DsB
'' SIG '' 7uK+jwoFyI1I4vBTFd1Pq5Lk541q1YDB5pTyBi+FA+mR
'' SIG '' KiQicPv2/OR4mS4N9wficLwYTp2OawpylbihOZxnLcVR
'' SIG '' DupiXD8WmIsgP+IHGjL5zDFKdjE9K3ILyOpwPf+FChPf
'' SIG '' wgphjvDXuBfrTot/xTUrXqO/67x9C0J71FNyIe4wyrt4
'' SIG '' ZVxbARcKFA7S2hSY9Ty5ZlizLS/n+YWGzFFW6J1wlGys
'' SIG '' OUzU9nm/qhh6YinvopspNAZ3GmLJPR5tH4LwC8csu89D
'' SIG '' s+X57H2146SodDW4TsVxIxImdgs8UoxxWkZDFLyzs7BN
'' SIG '' Z8ifQv+AeSGAnhUwZuhCEl4ayJ4iIdBD6Svpu/RIzCzU
'' SIG '' 2DKATCYqSCRfWupW76bemZ3KOm+9gSd0BhHudiG/m4LB
'' SIG '' J1S2sWo9iaF2YbRuoROmv6pH8BJv/YoybLL+31HIjCPJ
'' SIG '' Zr2dHYcSZAI9La9Zj7jkIeW1sMpjtHhUBdRBLlCslLCl
'' SIG '' eKuzoJZ1GtmShxN1Ii8yqAhuoFuMJb+g74TKIdbrHk/J
'' SIG '' mu5J4PcBZW+JC33Iacjmbuqnl84xKf8OxVtc2E0bodj6
'' SIG '' L54/LlUWa8kTo/0xggS1MIIEsQIBATCBkDB5MQswCQYD
'' SIG '' VQQGEwJVUzETMBEGA1UECBMKV2FzaGluZ3RvbjEQMA4G
'' SIG '' A1UEBxMHUmVkbW9uZDEeMBwGA1UEChMVTWljcm9zb2Z0
'' SIG '' IENvcnBvcmF0aW9uMSMwIQYDVQQDExpNaWNyb3NvZnQg
'' SIG '' Q29kZSBTaWduaW5nIFBDQQITMwAAAMps1TISNcThVQAB
'' SIG '' AAAAyjAJBgUrDgMCGgUAoIHOMBkGCSqGSIb3DQEJAzEM
'' SIG '' BgorBgEEAYI3AgEEMBwGCisGAQQBgjcCAQsxDjAMBgor
'' SIG '' BgEEAYI3AgEVMCMGCSqGSIb3DQEJBDEWBBQBG+sS2zem
'' SIG '' BnYaAfeE/T6l0ykRpjBuBgorBgEEAYI3AgEMMWAwXqBA
'' SIG '' gD4ATQBpAGMAcgBvAHMAbwBmAHQAIABNAGkAZwByAGEA
'' SIG '' dABpAG8AbgAgAEEAYwBjAGUAbABlAHIAYQB0AG8AcqEa
'' SIG '' gBhodHRwOi8vd3d3Lm1pY3Jvc29mdC5jb20wDQYJKoZI
'' SIG '' hvcNAQEBBQAEggEAE0QaxxUBgk/B9UDZ1mvFXcCEjV6L
'' SIG '' F7ijCzF6IX8F4DVAShNuZQknJoLsL5SR+ORaRj4KWifB
'' SIG '' sAiYVW2QFW3Tyr7tdUmZC55MnCfLpXZKEWDynfr55CaH
'' SIG '' 17EzYRLkxDz+Qx02rLGV3hFKljWnOhn5OS9dMejwG0wb
'' SIG '' vdvlK6YIyZz2zsTleiwmiZhf89vgx/Uwt4WRnLnhUhIK
'' SIG '' I+49Zv/Qe18gpQscDGXgQ4GcqZG1MWPdimB7mXRLi7Rl
'' SIG '' 3rjuHI3IU4cvSzlVuPOE1kgNepw43JggieJ9Lg81Ex5I
'' SIG '' qqZVXIHqCM/z+xb2RgutJ0LovaFUdVKlFX/zwRJhOVIB
'' SIG '' Yxl4RqGCAigwggIkBgkqhkiG9w0BCQYxggIVMIICEQIB
'' SIG '' ATCBjjB3MQswCQYDVQQGEwJVUzETMBEGA1UECBMKV2Fz
'' SIG '' aGluZ3RvbjEQMA4GA1UEBxMHUmVkbW9uZDEeMBwGA1UE
'' SIG '' ChMVTWljcm9zb2Z0IENvcnBvcmF0aW9uMSEwHwYDVQQD
'' SIG '' ExhNaWNyb3NvZnQgVGltZS1TdGFtcCBQQ0ECEzMAAABM
'' SIG '' oehNzLR0ezsAAAAAAEwwCQYFKw4DAhoFAKBdMBgGCSqG
'' SIG '' SIb3DQEJAzELBgkqhkiG9w0BBwEwHAYJKoZIhvcNAQkF
'' SIG '' MQ8XDTE0MDgyMjE0NDcxNVowIwYJKoZIhvcNAQkEMRYE
'' SIG '' FHpN4W48BoPS5NsXEnwib8+aH4bnMA0GCSqGSIb3DQEB
'' SIG '' BQUABIIBADMzf9QgOeV1PGIPjXvNEcFa1FpP8uNCyImh
'' SIG '' KUti8EWJvj3EdUyahupFbIgsZ3H/iMvi9ZjGVt7U7uuR
'' SIG '' 1SPi4eo9SwYfed5ZHAtaHcSDnQVF2wiwPZz9Sdy3yz+e
'' SIG '' ImNnXnmpypDZI6oDNj/BTsJMgi+S7WzL2p+6X92Hpypj
'' SIG '' fFvaAW9rBWVsIgdf47jIURo9sk3Nr55nUJiBQCuMci7c
'' SIG '' QJ9MR3eVSv7VkwxPSwsdfwcWPUcYIx1HCs3/YYMPNQzH
'' SIG '' zZpk0qtOFSbAEMu2syhZeDkfNDQ7gt0j2lR4KC+TOzzS
'' SIG '' bXXoA44hAd6W5lQi45uLDQmd6R7TU9s8Rojwi9GJ4Pc=
'' SIG '' End signature block
