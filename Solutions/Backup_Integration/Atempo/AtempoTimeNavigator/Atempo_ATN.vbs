Dim AtempoInstDir, InMageInstDir, InMageVsnapDir, Operation, AtempoSrcDir, AtempoTgtDir
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

strKeyPath = "SOFTWARE\Atempo\tina"
strValueName = "Path Installation"
oReg.GetStringValue HKEY_LOCAL_MACHINE,strKeyPath,strValueName,AtempoInstDir

LogDir = InMageInstDir & "\Atempo_tina_logs"

Operation = objArgs(0)
VsnapSrcDir = objArgs(1)
AtempoArgs = objArgs(2)
If objArgs.length = 4 Then
	AtempoRestoreDir = objArgs(3)
End If

If Not ObjFSO.folderexists(LogDir) Then
	ObjFSO.createfolder(LogDir)
End If

' Getting current time

Set objWMIService = GetObject("winmgmts:" _
 & "{impersonationLevel=impersonate}!\\" & strComputer & "\root\cimv2")
Set colItems = objWMIService.ExecQuery("Select * from Win32_LocalTime")

If  Operation = "backup" Then
' Taking Atemp Backup
	Set f = objFSO.OpenTextFile(LogDir & "\Atempobackup.txt", ForAppending, True)
	f.WriteLine Date & " " & Time & " Atempo Backup in progress..."
	Wscript.echo(Date & " " & Time & " Atempo Backup in progress...")
	RetVal = wshShell.Run( """" & AtempoInstDir & "\Bin\tina_backup.exe"" -path " & VsnapSrcDir & " " & AtempoArgs, 0, true)
	If RetVal = 0 Then
	f.WriteLine Date & " " & Time & " Atempo Backup successfully completed"
	Wscript.echo(Date & " " & Time & " Atempo Backup successfully completed")
	Else
	f.WriteLine Date & " " & Time & " Atempo Backup failed for some issue"
	Wscript.echo(Date & " " & Time & " Atempo Backup failed for some issue refer " & LogDir & "\Atempobackup.txt file")
	End If

' Atempo Restore
ElseIf  Operation = "restore" Then
	Set f = objFSO.OpenTextFile(LogDir & "\Atemporestore.txt", ForAppending, True)
	f.WriteLine Date & " " & Time & " Atempo Restore in Progress..."
	Wscript.echo(Date & " " & Time & " Atempo Restore in Progress...")
	RetVal = wshShell.Run("""" & AtempoInstDir & "\Bin\tina_restore.exe"" -path_folder " & VsnapSrcDir & " -path_dest " & AtempoRestoreDir & " " & AtempoArgs, 0, true)
	If RetVal = 0 Then
	f.WriteLine Date & " " & Time & " Atempo Restore successful"
	Wscript.echo(Date & " " & Time & " Atempo Restore successful")
	Else
	f.WriteLine Date & " " & Time & " Atempo Restore failed for some issue"
	Wscript.echo(Date & " " & Time & " Atempo Restore failed for some issue refer " & LogDir & "\Atemporestore.txt file")
	End If

End If

'' SIG '' Begin signature block
'' SIG '' MIIawwYJKoZIhvcNAQcCoIIatDCCGrACAQExCzAJBgUr
'' SIG '' DgMCGgUAMGcGCisGAQQBgjcCAQSgWTBXMDIGCisGAQQB
'' SIG '' gjcCAR4wJAIBAQQQTvApFpkntU2P5azhDxfrqwIBAAIB
'' SIG '' AAIBAAIBAAIBADAhMAkGBSsOAwIaBQAEFAyIFMPl+AVG
'' SIG '' xnCFisFD/HWKThFmoIIVejCCBLswggOjoAMCAQICEzMA
'' SIG '' AABZ1nPNUY7wIsUAAAAAAFkwDQYJKoZIhvcNAQEFBQAw
'' SIG '' dzELMAkGA1UEBhMCVVMxEzARBgNVBAgTCldhc2hpbmd0
'' SIG '' b24xEDAOBgNVBAcTB1JlZG1vbmQxHjAcBgNVBAoTFU1p
'' SIG '' Y3Jvc29mdCBDb3Jwb3JhdGlvbjEhMB8GA1UEAxMYTWlj
'' SIG '' cm9zb2Z0IFRpbWUtU3RhbXAgUENBMB4XDTE0MDUyMzE3
'' SIG '' MTMxNVoXDTE1MDgyMzE3MTMxNVowgasxCzAJBgNVBAYT
'' SIG '' AlVTMQswCQYDVQQIEwJXQTEQMA4GA1UEBxMHUmVkbW9u
'' SIG '' ZDEeMBwGA1UEChMVTWljcm9zb2Z0IENvcnBvcmF0aW9u
'' SIG '' MQ0wCwYDVQQLEwRNT1BSMScwJQYDVQQLEx5uQ2lwaGVy
'' SIG '' IERTRSBFU046RjUyOC0zNzc3LThBNzYxJTAjBgNVBAMT
'' SIG '' HE1pY3Jvc29mdCBUaW1lLVN0YW1wIFNlcnZpY2UwggEi
'' SIG '' MA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDGbE7P
'' SIG '' aFP974De6IvEsfB+B84ePOwMjDWHTOlROry2sJZ3Qvr/
'' SIG '' PM/h2uKJ+m5CAJlbFt0JZDjiUvvfjvqToz27h49uuIfJ
'' SIG '' 0GBPz5yCkW2RG3IQs9hDfFlKYF3GsFXQJ9vy9r3yIMYi
'' SIG '' LJ5riy1s6ngEyUvBcAnnth2vmowGP3hw+nbu0iQUdrKu
'' SIG '' ICiDHKnwSJI/ooX3g8rFUdCVIAN50le8E7VOuLRsVh9T
'' SIG '' HhW7zzA//TsBzV9yaPfK85lmM6hdIo8dbsraZdIrHCSs
'' SIG '' n3ypEIqF4m0uXEr9Sbl7QLFTxt9HubMjTiJHHNPBuUl2
'' SIG '' QnLOkIYOJPXCPLkJNj+oU1xW/l9hAgMBAAGjggEJMIIB
'' SIG '' BTAdBgNVHQ4EFgQUWygas811DWM9/Zn1mxUCSBrLpqww
'' SIG '' HwYDVR0jBBgwFoAUIzT42VJGcArtQPt2+7MrsMM1sw8w
'' SIG '' VAYDVR0fBE0wSzBJoEegRYZDaHR0cDovL2NybC5taWNy
'' SIG '' b3NvZnQuY29tL3BraS9jcmwvcHJvZHVjdHMvTWljcm9z
'' SIG '' b2Z0VGltZVN0YW1wUENBLmNybDBYBggrBgEFBQcBAQRM
'' SIG '' MEowSAYIKwYBBQUHMAKGPGh0dHA6Ly93d3cubWljcm9z
'' SIG '' b2Z0LmNvbS9wa2kvY2VydHMvTWljcm9zb2Z0VGltZVN0
'' SIG '' YW1wUENBLmNydDATBgNVHSUEDDAKBggrBgEFBQcDCDAN
'' SIG '' BgkqhkiG9w0BAQUFAAOCAQEAevAN9EVsNJYOd/DiwEIF
'' SIG '' YfeI03r9iNWn9fd/8gj21f3LynR82wmp39YVB5m/0D6H
'' SIG '' hGJ7wgaOyoto4j3fnlrFjLKpXP5ZYib11/l4tm60CpBl
'' SIG '' ZulRCPF8yaO3BDdGxeeCPihc709xpOexJVrlQ1QzCH+k
'' SIG '' sFUt0YJwSBEgDSaBDu8GJXSrhcPDjOIUX+gFVI+6homq
'' SIG '' lq6UYiX5r2mICgyxUcQJ77iAfFOQebvpj9BI8GLImfFl
'' SIG '' NDv3zrX7Zpi5olmZXT6VnxS/NbT7mHIkKQzzugR6gjn7
'' SIG '' Rs3x4LIWvH0g+Jw5FSWhJi3Wi4G9xr2wnVT+RwtfLU4q
'' SIG '' 9IfqtQ+1t+2SKTCCBOwwggPUoAMCAQICEzMAAADKbNUy
'' SIG '' EjXE4VUAAQAAAMowDQYJKoZIhvcNAQEFBQAweTELMAkG
'' SIG '' A1UEBhMCVVMxEzARBgNVBAgTCldhc2hpbmd0b24xEDAO
'' SIG '' BgNVBAcTB1JlZG1vbmQxHjAcBgNVBAoTFU1pY3Jvc29m
'' SIG '' dCBDb3Jwb3JhdGlvbjEjMCEGA1UEAxMaTWljcm9zb2Z0
'' SIG '' IENvZGUgU2lnbmluZyBQQ0EwHhcNMTQwNDIyMTczOTAw
'' SIG '' WhcNMTUwNzIyMTczOTAwWjCBgzELMAkGA1UEBhMCVVMx
'' SIG '' EzARBgNVBAgTCldhc2hpbmd0b24xEDAOBgNVBAcTB1Jl
'' SIG '' ZG1vbmQxHjAcBgNVBAoTFU1pY3Jvc29mdCBDb3Jwb3Jh
'' SIG '' dGlvbjENMAsGA1UECxMETU9QUjEeMBwGA1UEAxMVTWlj
'' SIG '' cm9zb2Z0IENvcnBvcmF0aW9uMIIBIjANBgkqhkiG9w0B
'' SIG '' AQEFAAOCAQ8AMIIBCgKCAQEAlnFd7QZG+oTLnVu3Rsew
'' SIG '' 4bQROQOtsRVzYJzrp7ZuGjw//2XjNPGmpSFeVplsWOSS
'' SIG '' oQpcwtPcUi8MZZogYUBTMZxsjyF9uvn+E1BSYJU6W7lY
'' SIG '' pXRhQamU4K0mTkyhl3BJJ158Z8pPHnGERrwdS7biD8XG
'' SIG '' J8kH5noKpRcAGUxwRTgtgbRQqsVn0fp5vMXMoXKb9CU0
'' SIG '' mPhU3xI5OBIvpGulmn7HYtHcz+09NPi53zUwuux5Mqnh
'' SIG '' qaxVTUx/TFbDEwt28Qf5zEes+4jVUqUeKPo9Lc/PhJiG
'' SIG '' cWURz4XJCUSG4W/nsfysQESlqYsjP4JJndWWWVATWRhz
'' SIG '' /0MMrSvUfzBAZwIDAQABo4IBYDCCAVwwEwYDVR0lBAww
'' SIG '' CgYIKwYBBQUHAwMwHQYDVR0OBBYEFB9e4l1QjVaGvko8
'' SIG '' zwTop4e1y7+DMFEGA1UdEQRKMEikRjBEMQ0wCwYDVQQL
'' SIG '' EwRNT1BSMTMwMQYDVQQFEyozMTU5NStiNDIxOGYxMy02
'' SIG '' ZmNhLTQ5MGYtOWM0Ny0zZmM1NTdkZmM0NDAwHwYDVR0j
'' SIG '' BBgwFoAUyxHoytK0FlgByTcuMxYWuUyaCh8wVgYDVR0f
'' SIG '' BE8wTTBLoEmgR4ZFaHR0cDovL2NybC5taWNyb3NvZnQu
'' SIG '' Y29tL3BraS9jcmwvcHJvZHVjdHMvTWljQ29kU2lnUENB
'' SIG '' XzA4LTMxLTIwMTAuY3JsMFoGCCsGAQUFBwEBBE4wTDBK
'' SIG '' BggrBgEFBQcwAoY+aHR0cDovL3d3dy5taWNyb3NvZnQu
'' SIG '' Y29tL3BraS9jZXJ0cy9NaWNDb2RTaWdQQ0FfMDgtMzEt
'' SIG '' MjAxMC5jcnQwDQYJKoZIhvcNAQEFBQADggEBAHdc69eR
'' SIG '' Pc29e4PZhamwQ51zfBfJD+0228e1LBte+1QFOoNxQIEJ
'' SIG '' ordxJl7WfbZsO8mqX10DGCodJ34H6cVlH7XPDbdUxyg4
'' SIG '' Wojne8EZtlYyuuLMy5Pbr24PXUT11LDvG9VOwa8O7yCb
'' SIG '' 8uH+J13oxf9h9hnSKAoind/NcIKeGHLYI8x6LEPu/+rA
'' SIG '' 4OYdqp6XMwBSbwe404hs3qQGNafCU4ZlEXcJjzVZudiG
'' SIG '' qAD++DF9LPSMBZ3AwdV3cmzpTVkmg/HCsohXkzUAfFAr
'' SIG '' vFn8/hwpOILT3lKXRSkYTpZbnbpfG6PxJ1DqB5XobTQN
'' SIG '' OFfcNyg1lTo4nNTtaoVdDiIRXnswggW8MIIDpKADAgEC
'' SIG '' AgphMyYaAAAAAAAxMA0GCSqGSIb3DQEBBQUAMF8xEzAR
'' SIG '' BgoJkiaJk/IsZAEZFgNjb20xGTAXBgoJkiaJk/IsZAEZ
'' SIG '' FgltaWNyb3NvZnQxLTArBgNVBAMTJE1pY3Jvc29mdCBS
'' SIG '' b290IENlcnRpZmljYXRlIEF1dGhvcml0eTAeFw0xMDA4
'' SIG '' MzEyMjE5MzJaFw0yMDA4MzEyMjI5MzJaMHkxCzAJBgNV
'' SIG '' BAYTAlVTMRMwEQYDVQQIEwpXYXNoaW5ndG9uMRAwDgYD
'' SIG '' VQQHEwdSZWRtb25kMR4wHAYDVQQKExVNaWNyb3NvZnQg
'' SIG '' Q29ycG9yYXRpb24xIzAhBgNVBAMTGk1pY3Jvc29mdCBD
'' SIG '' b2RlIFNpZ25pbmcgUENBMIIBIjANBgkqhkiG9w0BAQEF
'' SIG '' AAOCAQ8AMIIBCgKCAQEAsnJZXBkwZL8dmmAgIEKZdlNs
'' SIG '' PhvWb8zL8epr/pcWEODfOnSDGrcvoDLs/97CQk4j1XIA
'' SIG '' 2zVXConKriBJ9PBorE1LjaW9eUtxm0cH2v0l3511iM+q
'' SIG '' c0R/14Hb873yNqTJXEXcr6094CholxqnpXJzVvEXlOT9
'' SIG '' NZRyoNZ2Xx53RYOFOBbQc1sFumdSjaWyaS/aGQv+knQp
'' SIG '' 4nYvVN0UMFn40o1i/cvJX0YxULknE+RAMM9yKRAoIsc3
'' SIG '' Tj2gMj2QzaE4BoVcTlaCKCoFMrdL109j59ItYvFFPees
'' SIG '' CAD2RqGe0VuMJlPoeqpK8kbPNzw4nrR3XKUXno3LEY9W
'' SIG '' PMGsCV8D0wIDAQABo4IBXjCCAVowDwYDVR0TAQH/BAUw
'' SIG '' AwEB/zAdBgNVHQ4EFgQUyxHoytK0FlgByTcuMxYWuUya
'' SIG '' Ch8wCwYDVR0PBAQDAgGGMBIGCSsGAQQBgjcVAQQFAgMB
'' SIG '' AAEwIwYJKwYBBAGCNxUCBBYEFP3RMU7TJoqV4ZhgO6gx
'' SIG '' b6Y8vNgtMBkGCSsGAQQBgjcUAgQMHgoAUwB1AGIAQwBB
'' SIG '' MB8GA1UdIwQYMBaAFA6sgmBAVieX5SUT/CrhClOVWeSk
'' SIG '' MFAGA1UdHwRJMEcwRaBDoEGGP2h0dHA6Ly9jcmwubWlj
'' SIG '' cm9zb2Z0LmNvbS9wa2kvY3JsL3Byb2R1Y3RzL21pY3Jv
'' SIG '' c29mdHJvb3RjZXJ0LmNybDBUBggrBgEFBQcBAQRIMEYw
'' SIG '' RAYIKwYBBQUHMAKGOGh0dHA6Ly93d3cubWljcm9zb2Z0
'' SIG '' LmNvbS9wa2kvY2VydHMvTWljcm9zb2Z0Um9vdENlcnQu
'' SIG '' Y3J0MA0GCSqGSIb3DQEBBQUAA4ICAQBZOT5/Jkav629A
'' SIG '' sTK1ausOL26oSffrX3XtTDst10OtC/7L6S0xoyPMfFCY
'' SIG '' gCFdrD0vTLqiqFac43C7uLT4ebVJcvc+6kF/yuEMF2nL
'' SIG '' pZwgLfoLUMRWzS3jStK8cOeoDaIDpVbguIpLV/KVQpzx
'' SIG '' 8+/u44YfNDy4VprwUyOFKqSCHJPilAcd8uJO+IyhyugT
'' SIG '' pZFOyBvSj3KVKnFtmxr4HPBT1mfMIv9cHc2ijL0nsnlj
'' SIG '' VkSiUc356aNYVt2bAkVEL1/02q7UgjJu/KSVE+Traeep
'' SIG '' oiy+yCsQDmWOmdv1ovoSJgllOJTxeh9Ku9HhVujQeJYY
'' SIG '' XMk1Fl/dkx1Jji2+rTREHO4QFRoAXd01WyHOmMcJ7oUO
'' SIG '' jE9tDhNOPXwpSJxy0fNsysHscKNXkld9lI2gG0gDWvfP
'' SIG '' o2cKdKU27S0vF8jmcjcS9G+xPGeC+VKyjTMWZR4Oit0Q
'' SIG '' 3mT0b85G1NMX6XnEBLTT+yzfH4qerAr7EydAreT54al/
'' SIG '' RrsHYEdlYEBOsELsTu2zdnnYCjQJbRyAMR/iDlTd5aH7
'' SIG '' 5UcQrWSY/1AWLny/BSF64pVBJ2nDk4+VyY3YmyGuDVyc
'' SIG '' 8KKuhmiDDGotu3ZrAB2WrfIWe/YWgyS5iM9qqEcxL5rc
'' SIG '' 43E91wB+YkfRzojJuBj6DnKNwaM9rwJAav9pm5biEKgQ
'' SIG '' tDdQCNbDPTCCBgcwggPvoAMCAQICCmEWaDQAAAAAABww
'' SIG '' DQYJKoZIhvcNAQEFBQAwXzETMBEGCgmSJomT8ixkARkW
'' SIG '' A2NvbTEZMBcGCgmSJomT8ixkARkWCW1pY3Jvc29mdDEt
'' SIG '' MCsGA1UEAxMkTWljcm9zb2Z0IFJvb3QgQ2VydGlmaWNh
'' SIG '' dGUgQXV0aG9yaXR5MB4XDTA3MDQwMzEyNTMwOVoXDTIx
'' SIG '' MDQwMzEzMDMwOVowdzELMAkGA1UEBhMCVVMxEzARBgNV
'' SIG '' BAgTCldhc2hpbmd0b24xEDAOBgNVBAcTB1JlZG1vbmQx
'' SIG '' HjAcBgNVBAoTFU1pY3Jvc29mdCBDb3Jwb3JhdGlvbjEh
'' SIG '' MB8GA1UEAxMYTWljcm9zb2Z0IFRpbWUtU3RhbXAgUENB
'' SIG '' MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA
'' SIG '' n6Fssd/bSJIqfGsuGeG94uPFmVEjUK3O3RhOJA/u0afR
'' SIG '' TK10MCAR6wfVVJUVSZQbQpKumFwwJtoAa+h7veyJBw/3
'' SIG '' DgSY8InMH8szJIed8vRnHCz8e+eIHernTqOhwSNTyo36
'' SIG '' Rc8J0F6v0LBCBKL5pmyTZ9co3EZTsIbQ5ShGLieshk9V
'' SIG '' UgzkAyz7apCQMG6H81kwnfp+1pez6CGXfvjSE/MIt1Nt
'' SIG '' UrRFkJ9IAEpHZhEnKWaol+TTBoFKovmEpxFHFAmCn4Tt
'' SIG '' VXj+AZodUAiFABAwRu233iNGu8QtVJ+vHnhBMXfMm987
'' SIG '' g5OhYQK1HQ2x/PebsgHOIktU//kFw8IgCwIDAQABo4IB
'' SIG '' qzCCAacwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQU
'' SIG '' IzT42VJGcArtQPt2+7MrsMM1sw8wCwYDVR0PBAQDAgGG
'' SIG '' MBAGCSsGAQQBgjcVAQQDAgEAMIGYBgNVHSMEgZAwgY2A
'' SIG '' FA6sgmBAVieX5SUT/CrhClOVWeSkoWOkYTBfMRMwEQYK
'' SIG '' CZImiZPyLGQBGRYDY29tMRkwFwYKCZImiZPyLGQBGRYJ
'' SIG '' bWljcm9zb2Z0MS0wKwYDVQQDEyRNaWNyb3NvZnQgUm9v
'' SIG '' dCBDZXJ0aWZpY2F0ZSBBdXRob3JpdHmCEHmtFqFKoKWt
'' SIG '' THNY9AcTLmUwUAYDVR0fBEkwRzBFoEOgQYY/aHR0cDov
'' SIG '' L2NybC5taWNyb3NvZnQuY29tL3BraS9jcmwvcHJvZHVj
'' SIG '' dHMvbWljcm9zb2Z0cm9vdGNlcnQuY3JsMFQGCCsGAQUF
'' SIG '' BwEBBEgwRjBEBggrBgEFBQcwAoY4aHR0cDovL3d3dy5t
'' SIG '' aWNyb3NvZnQuY29tL3BraS9jZXJ0cy9NaWNyb3NvZnRS
'' SIG '' b290Q2VydC5jcnQwEwYDVR0lBAwwCgYIKwYBBQUHAwgw
'' SIG '' DQYJKoZIhvcNAQEFBQADggIBABCXisNcA0Q23em0rXfb
'' SIG '' znlRTQGxLnRxW20ME6vOvnuPuC7UEqKMbWK4VwLLTiAT
'' SIG '' UJndekDiV7uvWJoc4R0Bhqy7ePKL0Ow7Ae7ivo8KBciN
'' SIG '' SOLwUxXdT6uS5OeNatWAweaU8gYvhQPpkSokInD79vzk
'' SIG '' eJkuDfcH4nC8GE6djmsKcpW4oTmcZy3FUQ7qYlw/FpiL
'' SIG '' ID/iBxoy+cwxSnYxPStyC8jqcD3/hQoT38IKYY7w17gX
'' SIG '' 606Lf8U1K16jv+u8fQtCe9RTciHuMMq7eGVcWwEXChQO
'' SIG '' 0toUmPU8uWZYsy0v5/mFhsxRVuidcJRsrDlM1PZ5v6oY
'' SIG '' emIp76KbKTQGdxpiyT0ebR+C8AvHLLvPQ7Pl+ex9teOk
'' SIG '' qHQ1uE7FcSMSJnYLPFKMcVpGQxS8s7OwTWfIn0L/gHkh
'' SIG '' gJ4VMGboQhJeGsieIiHQQ+kr6bv0SMws1NgygEwmKkgk
'' SIG '' X1rqVu+m3pmdyjpvvYEndAYR7nYhv5uCwSdUtrFqPYmh
'' SIG '' dmG0bqETpr+qR/ASb/2KMmyy/t9RyIwjyWa9nR2HEmQC
'' SIG '' PS2vWY+45CHltbDKY7R4VAXUQS5QrJSwpXirs6CWdRrZ
'' SIG '' kocTdSIvMqgIbqBbjCW/oO+EyiHW6x5PyZruSeD3AWVv
'' SIG '' iQt9yGnI5m7qp5fOMSn/DsVbXNhNG6HY+i+ePy5VFmvJ
'' SIG '' E6P9MYIEtTCCBLECAQEwgZAweTELMAkGA1UEBhMCVVMx
'' SIG '' EzARBgNVBAgTCldhc2hpbmd0b24xEDAOBgNVBAcTB1Jl
'' SIG '' ZG1vbmQxHjAcBgNVBAoTFU1pY3Jvc29mdCBDb3Jwb3Jh
'' SIG '' dGlvbjEjMCEGA1UEAxMaTWljcm9zb2Z0IENvZGUgU2ln
'' SIG '' bmluZyBQQ0ECEzMAAADKbNUyEjXE4VUAAQAAAMowCQYF
'' SIG '' Kw4DAhoFAKCBzjAZBgkqhkiG9w0BCQMxDAYKKwYBBAGC
'' SIG '' NwIBBDAcBgorBgEEAYI3AgELMQ4wDAYKKwYBBAGCNwIB
'' SIG '' FTAjBgkqhkiG9w0BCQQxFgQU81siPC04yuKGiao5Frvb
'' SIG '' 4zJcaxgwbgYKKwYBBAGCNwIBDDFgMF6gQIA+AE0AaQBj
'' SIG '' AHIAbwBzAG8AZgB0ACAATQBpAGcAcgBhAHQAaQBvAG4A
'' SIG '' IABBAGMAYwBlAGwAZQByAGEAdABvAHKhGoAYaHR0cDov
'' SIG '' L3d3dy5taWNyb3NvZnQuY29tMA0GCSqGSIb3DQEBAQUA
'' SIG '' BIIBAJOrWdbJ8841Ui2Tu4UEPcJzFWRNLkZ3+m74PjCM
'' SIG '' roxoIwxA0ZJoprJLD+9CAtlOO+ZJKEtWTteCnAgN+wh/
'' SIG '' alzw6YOuK/IbB1LBz9t6lIIscBoTzJ51HbiWLV12TdRZ
'' SIG '' HANpNtyvYNj5I1WhLZqp8BXJuHYx/OIAoMIVPEFHa2CO
'' SIG '' s6gLgRGekwycjnDOFlMVReXDd4kIB7vaZg8137mBFaoc
'' SIG '' vWRIifonkmXwCZyzsnXccIEIOLnQfbsprljU+JYLHwb4
'' SIG '' cBR84DaibhFpHJRvgqh80ocYtfTHDktkXrYoSjcZaOG3
'' SIG '' InxE63iSW6A2fqCyMxjJHY5obrBRXrf6rVZzJfyhggIo
'' SIG '' MIICJAYJKoZIhvcNAQkGMYICFTCCAhECAQEwgY4wdzEL
'' SIG '' MAkGA1UEBhMCVVMxEzARBgNVBAgTCldhc2hpbmd0b24x
'' SIG '' EDAOBgNVBAcTB1JlZG1vbmQxHjAcBgNVBAoTFU1pY3Jv
'' SIG '' c29mdCBDb3Jwb3JhdGlvbjEhMB8GA1UEAxMYTWljcm9z
'' SIG '' b2Z0IFRpbWUtU3RhbXAgUENBAhMzAAAAWdZzzVGO8CLF
'' SIG '' AAAAAABZMAkGBSsOAwIaBQCgXTAYBgkqhkiG9w0BCQMx
'' SIG '' CwYJKoZIhvcNAQcBMBwGCSqGSIb3DQEJBTEPFw0xNDA4
'' SIG '' MjIxNDQ3MTVaMCMGCSqGSIb3DQEJBDEWBBTuAVr/deF+
'' SIG '' Plkaa+gVWLY0BCxtejANBgkqhkiG9w0BAQUFAASCAQB2
'' SIG '' zgG/Gx1rTV3aikM1/TQ+y/g5UpaNusdxqs777tOgZdjH
'' SIG '' ldKyqz9X09IfUII6LJp7AzJfDqcIaZZfygRGE/9E9o2q
'' SIG '' 4U7ZFEGeBLWmOMv3/cg4YN4uhujskKTDnE7+aGiJbTqd
'' SIG '' TE51F614uDx+DEjgbYaXqHMPYdmmfepPy/EPHquvLNM9
'' SIG '' eDAZGYoOs60qt6gP56salemgUQFXcopiJC3F3F2rJ0vr
'' SIG '' VJByIErPHkQQBqPjAOYzEXJ1FVOVXqiDL5vHAERcRaV/
'' SIG '' k4LGbuTzhrTv7ikLr7ESWSlIHPNwVpzObUYUJlFvrKK8
'' SIG '' /ct9EpUd0HrZX7ZM1TEPNPTPsyLRqh51
'' SIG '' End signature block
