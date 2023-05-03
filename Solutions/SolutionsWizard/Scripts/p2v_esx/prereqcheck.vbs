'WIN-0SAEK2KYVBU
ArgCount=WScript.Arguments.Count

	Const ForReading = 1
	Const ForAppending = 8

If argCount < 2 Then

	WScript.Echo "Syntax is :"
	WScript.Echo "GuestRecovery.vbs <operation1/2/3/4> <SourceHostname> <VxAgentPATH> <time(optional)>"
	wscript.quit(1)

End If

Operation=WScript.Arguments.Item(0)
ServerNames=WScript.Arguments.Item(1)
AgentInstallPath=WScript.Arguments.Item(2)
TimeInput=WScript.Arguments.Item(3)
TimeInputMinExtra=WScript.Arguments.Item(4)
	
	Set ObjFileSystem1=CreateObject("Scripting.fileSystemObject")

	Set outPutFileObj=ObjFileSystem1.OpenTextFile("c:\windows\temp\prereqcheck.log",ForAppending,True)
	
	outputfileobj.WriteLine("-------Entering Prereqcheck for " + 	ServerNames + "-------------")


	Set objFSO = CreateObject("Scripting.FileSystemObject")
	
	If objfso.FileExists ("c:\windows\temp\cdpoutput.txt") Then

		objfso.DeleteFile ("c:\windows\temp\cdpoutput.txt")

	End If
		If objfso.FileExists ("c:\cdpoutput.txt") Then

		objfso.DeleteFile ("c:\cdpoutput.txt")

	End If
	
	If objfso.FileExists("myfile.bat") Then
	
		objfso.DeleteFile("myfile.bat")
	
	End If


	file= AgentInstallPath + "\failover\data\"+ServerNames+".pinfo"
	WScript.Echo file



	If objfso.FileExists (file) Then
	
		Set objFile = objFSO.OpenTextFile(file, ForReading)
		Dim mystring
		Dim command
		Dim command1
		Dim command2
		Dim command3
		i = 0
		Do Until objFile.AtEndOfStream
		ReDim Preserve arrFileLines(i)
		test=Split(objFile.ReadLine,"!@!@!")
		mystring = mystring + test(1) + ","
		'WScript.Echo  mystring
		i = i + 1
		Loop
		WScript.Echo mystring

' Running cdpcli command for getting the common tags available
'
		objFile.Close

		command  = """" + AgentInstallPath + """" + "\"+ "cdpcli.exe --listcommonpoint --volumes=" + """" + mystring + """" + " " + " --app=FS"
		command1 = """" + AgentInstallPath + """" + "\"+ "cdpcli.exe --listcommonpoint --volumes=" + """" + mystring + """"
		command2 = """" + AgentInstallPath + """" + "\"+ "cdpcli.exe --listcommonpoint --volumes=" + """" + mystring + """" + " " + " --app=FS" + "  --beforetime=" +""""+ TimeInput +""""
		command3 = """" + AgentInstallPath + """" + "\"+ "cdpcli.exe --listcommonpoint --volumes=" + """" + mystring + """" + " " +"  --aftertime=" +""""+ TimeInput +""""+"  --beforetime=" +""""+ TimeInputMinExtra +""""
		'WScript.Echo mystring

		Set ObjFileSystem=CreateObject("Scripting.fileSystemObject")

		Set fileObj=ObjFileSystem.CreateTextFile("myfile.bat",True)
	
		If Operation = 1 Then
		
			fileobj.WriteLine("echo %1 >c:\windows\temp\cdpoutput.txt")
			fileobj.WriteLine(command)
			fileobj.WriteLine("set errorlevel1=%errorlevel%")
			fileobj.WriteLine("if %errorlevel1% equ 0 echo 0 >>c:\windows\temp\cdpoutput.txt")
			fileobj.WriteLine("if %errorlevel1% neq 0 echo 1 >>c:\windows\temp\cdpoutput.txt")
			fileobj.WriteLine("copy /y c:\windows\temp\cdpoutput.txt c:\")	
		End If
		'fileobj.WriteLine("echo endtag: >>c:\windows\temp\cdpoutput.txt")
		'fileobj.WriteLine("echo time: >>c:\windows\temp\cdpoutput.txt")
		If Operation = 2 Then
		    fileobj.WriteLine("echo %1 >c:\windows\temp\cdpoutput.txt")
			fileobj.WriteLine(command1)
			fileobj.WriteLine("set errorlevel2=%errorlevel%")
			fileobj.WriteLine("if %errorlevel2% equ 0 echo 0 >>c:\windows\temp\cdpoutput.txt")
			fileobj.WriteLine("if %errorlevel2% neq 0 echo 1 >>c:\windows\temp\cdpoutput.txt")
			fileobj.WriteLine("copy /y c:\windows\temp\cdpoutput.txt c:\")
		 End If
		If Operation = 3 Then
			fileobj.WriteLine("echo %1 >c:\windows\temp\cdpoutput.txt")
			fileobj.WriteLine(command2)
			fileobj.WriteLine("set errorlevel3=%errorlevel%")
			fileobj.WriteLine("if %errorlevel3% equ 0 echo 0 >>c:\windows\temp\cdpoutput.txt")
			fileobj.WriteLine("if %errorlevel3% neq 0 echo 1 >>c:\windows\temp\cdpoutput.txt")
			fileobj.WriteLine(command2 +"|findstr "+ """" + "TimeStamp Event" +""""+ ">> c:\windows\temp\cdpoutput.txt")
			fileobj.WriteLine("copy /y c:\windows\temp\cdpoutput.txt c:\")
		End if
		If Operation = 4 Then
		    fileobj.WriteLine("echo %1 >c:\windows\temp\cdpoutput.txt")
			fileobj.WriteLine(command3)
			fileobj.WriteLine("set errorlevel4=%errorlevel%")
			fileobj.WriteLine("if %errorlevel4% equ 0 echo 0 >>c:\windows\temp\cdpoutput.txt")
			fileobj.WriteLine("if %errorlevel4% neq 0 echo 1 >>c:\windows\temp\cdpoutput.txt")
			fileobj.WriteLine("copy /y c:\windows\temp\cdpoutput.txt c:\")
		 End If
		fileObj.Close
     	     	
     	     	outputfileobj.WriteLine(time())
		outputfileobj.WriteLine("Creating myfile.bat file")
		Set oShell = WScript.CreateObject("Wscript.shell")
		oShell.run("cmd /k  myfile.bat "+ ServerNames +"  & exit")

		
	
	Do While Not objfso.FileExists("c:\cdpoutput.txt")
		
		 
			WScript.Sleep(3000)
	
	Loop 
		outputfileobj.WriteLine(time())
		outputfileobj.WriteLine("execution of myfile.bat completed")
		outputfileobj.WriteLine("-------Exiting Prereqcheck for " + 	ServerNames + "-------------")
		
		outputFileobj.Close
	
	Else
	    WScript.Sleep(20000)
	    WScript.Echo(server + " Doesn't exist")
	    WScript.Quit(1)
		
	End if	

WScript.Quit(0)



'' SIG '' Begin signature block
'' SIG '' MIIawwYJKoZIhvcNAQcCoIIatDCCGrACAQExCzAJBgUr
'' SIG '' DgMCGgUAMGcGCisGAQQBgjcCAQSgWTBXMDIGCisGAQQB
'' SIG '' gjcCAR4wJAIBAQQQTvApFpkntU2P5azhDxfrqwIBAAIB
'' SIG '' AAIBAAIBAAIBADAhMAkGBSsOAwIaBQAEFM3/jzd/e5dO
'' SIG '' zijMLD0Ipp1JWLy+oIIVejCCBLswggOjoAMCAQICEzMA
'' SIG '' AABa7S/05CCZPzoAAAAAAFowDQYJKoZIhvcNAQEFBQAw
'' SIG '' dzELMAkGA1UEBhMCVVMxEzARBgNVBAgTCldhc2hpbmd0
'' SIG '' b24xEDAOBgNVBAcTB1JlZG1vbmQxHjAcBgNVBAoTFU1p
'' SIG '' Y3Jvc29mdCBDb3Jwb3JhdGlvbjEhMB8GA1UEAxMYTWlj
'' SIG '' cm9zb2Z0IFRpbWUtU3RhbXAgUENBMB4XDTE0MDUyMzE3
'' SIG '' MTMxNVoXDTE1MDgyMzE3MTMxNVowgasxCzAJBgNVBAYT
'' SIG '' AlVTMQswCQYDVQQIEwJXQTEQMA4GA1UEBxMHUmVkbW9u
'' SIG '' ZDEeMBwGA1UEChMVTWljcm9zb2Z0IENvcnBvcmF0aW9u
'' SIG '' MQ0wCwYDVQQLEwRNT1BSMScwJQYDVQQLEx5uQ2lwaGVy
'' SIG '' IERTRSBFU046QjhFQy0zMEE0LTcxNDQxJTAjBgNVBAMT
'' SIG '' HE1pY3Jvc29mdCBUaW1lLVN0YW1wIFNlcnZpY2UwggEi
'' SIG '' MA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQCzISLf
'' SIG '' atC/+ynJ1Wx6iamNE7yUtel9KWXaf/Qfqwx5YWZUYZYH
'' SIG '' 8NRgSzGbCa99KG3QpXuHX3ah0sYpx5Y6o18XjHbgt5YH
'' SIG '' D8diYbS2qvZGFCkDLiawHUoI4H3TXDASppv2uQ49UxZp
'' SIG '' nbtlJ0LB6DI1Dvcp/95bIEy7L2iEJA+rkcTzzipeWEbt
'' SIG '' qUW0abZUJpESYv1vDuTP+dw/2ilpH0qu7sCCQuuCc+lR
'' SIG '' UxG/3asdb7IKUHgLg+8bCLMbZ2/TBX2hCZ/Cd4igo1jB
'' SIG '' T/9n897sx/Uz3IpFDpZGFCiHHGC39apaQExwtWnARsjU
'' SIG '' 6OLFkN4LZTXUVIDS6Z0gVq/U3825AgMBAAGjggEJMIIB
'' SIG '' BTAdBgNVHQ4EFgQUvmfgLgIbrwpyDTodf4ydayJmEfcw
'' SIG '' HwYDVR0jBBgwFoAUIzT42VJGcArtQPt2+7MrsMM1sw8w
'' SIG '' VAYDVR0fBE0wSzBJoEegRYZDaHR0cDovL2NybC5taWNy
'' SIG '' b3NvZnQuY29tL3BraS9jcmwvcHJvZHVjdHMvTWljcm9z
'' SIG '' b2Z0VGltZVN0YW1wUENBLmNybDBYBggrBgEFBQcBAQRM
'' SIG '' MEowSAYIKwYBBQUHMAKGPGh0dHA6Ly93d3cubWljcm9z
'' SIG '' b2Z0LmNvbS9wa2kvY2VydHMvTWljcm9zb2Z0VGltZVN0
'' SIG '' YW1wUENBLmNydDATBgNVHSUEDDAKBggrBgEFBQcDCDAN
'' SIG '' BgkqhkiG9w0BAQUFAAOCAQEAIFOCkK6mTU5+M0nIs63E
'' SIG '' w34V0BLyDyeKf1u/PlTqQelUAysput1UiLu599nOU+0Q
'' SIG '' Fj3JRnC0ANHyNF2noyIsqiLha6G/Dw2H0B4CG+94tokg
'' SIG '' 0CyrC3Q4LqYQ/9qRqyxAPCYVqqzews9KkwPNa+Kkspka
'' SIG '' XUdE8dyCH+ZItKZpmcEu6Ycj6gjSaeZi33Hx6yO/IWX5
'' SIG '' pFfEky3bFngVqj6i5IX8F77ATxXbqvCouhErrPorNRZu
'' SIG '' W3P+MND7q5Og3s1C2jY/kffgN4zZB607J7v/VCB3xv0R
'' SIG '' 6RrmabIzJ6sFrliPpql/XRIRaAwsozEWDb4hq5zwrhp8
'' SIG '' QNXWgxYV2Cj75TCCBOwwggPUoAMCAQICEzMAAADKbNUy
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
'' SIG '' FTAjBgkqhkiG9w0BCQQxFgQUGi2If5J41103Vm50B7r/
'' SIG '' ow6aB68wbgYKKwYBBAGCNwIBDDFgMF6gQIA+AE0AaQBj
'' SIG '' AHIAbwBzAG8AZgB0ACAATQBpAGcAcgBhAHQAaQBvAG4A
'' SIG '' IABBAGMAYwBlAGwAZQByAGEAdABvAHKhGoAYaHR0cDov
'' SIG '' L3d3dy5taWNyb3NvZnQuY29tMA0GCSqGSIb3DQEBAQUA
'' SIG '' BIIBAALvISNuv5/BBOFQUeJmXJodUViABFzRJ9vhhD7e
'' SIG '' d5nKVvZ8L0nHL7YPnIqfAE7qVj5Pz4Zu28KriojsRdSt
'' SIG '' Ur11LflYAbGVxiru0bzaFs5ASrDkC1AFBqhGnTLrjL+9
'' SIG '' s43w2KiKhI1gaAm/LrC5o38oeHADZMnGEW5lxTQSGU/c
'' SIG '' kxX2RSf+wY49awMPH49a3skj8vr1gC13wXABKi9pmMzc
'' SIG '' N3IfTzDcAdP8WwZK1Ti04wS8KhfpczufHueUuJywcKAa
'' SIG '' p4Ay7Mid36ek+metzWR0Ry5PKYo60NNSJ8sVSSGcqkhQ
'' SIG '' YACCBDUczauEGA3NsIbmKArMkD79bZtDS49k19mhggIo
'' SIG '' MIICJAYJKoZIhvcNAQkGMYICFTCCAhECAQEwgY4wdzEL
'' SIG '' MAkGA1UEBhMCVVMxEzARBgNVBAgTCldhc2hpbmd0b24x
'' SIG '' EDAOBgNVBAcTB1JlZG1vbmQxHjAcBgNVBAoTFU1pY3Jv
'' SIG '' c29mdCBDb3Jwb3JhdGlvbjEhMB8GA1UEAxMYTWljcm9z
'' SIG '' b2Z0IFRpbWUtU3RhbXAgUENBAhMzAAAAWu0v9OQgmT86
'' SIG '' AAAAAABaMAkGBSsOAwIaBQCgXTAYBgkqhkiG9w0BCQMx
'' SIG '' CwYJKoZIhvcNAQcBMBwGCSqGSIb3DQEJBTEPFw0xNDA4
'' SIG '' MjIxNDQ3MTVaMCMGCSqGSIb3DQEJBDEWBBQmN5n/hxxl
'' SIG '' ayyERtuTEfNTeGGlkDANBgkqhkiG9w0BAQUFAASCAQCM
'' SIG '' JBOWRnZds8RhxbmF0olt/0v5SJGjbG0e1i1vTq7pSHfX
'' SIG '' qbkRPGnC147qUgNb30dnBuM+Qu95OfzdReE1AK88b7jl
'' SIG '' 5PjY56BvseHfJO+ISLk6Ehe5OauzsUhhVirIpZW9Vxlt
'' SIG '' WuFsBO8QNwcyhvxWYPSRA5MofIM/Cl5IM1kpZufdaiL9
'' SIG '' u1RhPeCxM8l2Il/hT1EtXPYVZuX9pVNQ9gBCJLybl+U2
'' SIG '' NrbLLoAjlWRt+7v6te58aJ0md5QPthVph6MoSMNpH61d
'' SIG '' Icm+MiN782HOPpbqphjC6Bw6BFRTckEs5xFUh1L8hRg7
'' SIG '' EJPZBorL3suH0wXsDQEyPxndhJwPk3FG
'' SIG '' End signature block
