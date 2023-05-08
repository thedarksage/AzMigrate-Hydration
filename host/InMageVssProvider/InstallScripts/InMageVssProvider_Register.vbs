Option Explicit

Wscript.Echo "" 
Wscript.Echo "InMageVssProvider_Register.vbs" 
Wscript.Echo "" 

Const VSS_E_OUT_OF_MEMORY = &H7
Const VSS_E_SERVICE_DB_LOCKED = &H8007041F
Const VSS_E_SERVICE_ALREADY_EXISTS = &H80070431
Const VSS_E_SERVICE_MARKED_FOR_DELETION = &H80070430
const VSS_E_ERROR_ACCESS_DENIED = &H80070005
PrintAvailableMemory


Dim InmArgs
Set InmArgs = Wscript.Arguments
If InmArgs.Count < 1 Then 
	PrintHelp
End If

Dim InMageProviderName, InmProviderDLL, ProviderDesc

If InmArgs.Item(0) = "-register" Then 
	If Not InmArgs.Count = 4 Then PrintHelp
	InMageProviderName = InmArgs.Item(1)
	InmProviderDLL = InmArgs.Item(2)
	ProviderDesc = InmArgs.Item(3)
		
	Install_InMageVssProvider

	Wscript.Quit 0
End If 

If InmArgs.Item(0) = "-unregister" Then 
	If Not InmArgs.Count = 2 Then PrintHelp
	InMageProviderName = InmArgs.Item(1)
	Uninstall_InMageVssProvider
	Wscript.Quit 0
End If


PrintHelp

Wscript.Quit 0




Sub PrintHelp


	Wscript.Echo "Correct Usage of Script:" 
	Wscript.Echo "" 
	Wscript.Echo " 1) Registering the InMage VSS Provider as a COM+ application:" 
	Wscript.Echo "    CScript.exe " & Wscript.ScriptName & " -register <Provider_Name> <Provider.DLL>  <Provider_Description>"
	Wscript.Echo "" 
	Wscript.Echo " 2) Un-Registering the COM+ application associated with a InMage VSS Provider:" 
	Wscript.Echo "    CScript.exe " & Wscript.ScriptName & " -unregister <Provider_Name>" 
	Wscript.Echo "" 
	Wscript.Quit 1

End Sub


Sub PrintAvailableMemory
	On Error Resume Next
	
	Dim strInmQueryMem, objInmMemSet, objInmMemItem

	strInmQueryMem = "select * from Win32_PerfFormattedData_PerfOS_Memory"
	set objInmMemSet = GetObject("winmgmts:\\.\root\cimv2").ExecQuery(strInmQueryMem)

	If objInmMemSet.Count = 0 Then
		If Err <> 0 Then
			Wscript.Echo "Failed to run WMI query"
			Wscript.Echo "Error code: " & Err & " [0x" & Hex(Err) & "]"
			Wscript.Echo "Description: " & Err.Description
		End If
	Else
		For Each objInmMemItem in objInmMemSet
			Wscript.Echo ""
			Wscript.Echo "AvailableMBytes: " & objInmMemItem.AvailableMBytes
			Wscript.Echo "CommitLimit: " & objInmMemItem.CommitLimit
			Wscript.Echo "CommittedBytes: " & objInmMemItem.CommittedBytes
			Wscript.Echo ""      
		Next
	End If
	
	On Error GoTo 0
End Sub


Sub Install_InMageVssProvider
	On Error Resume Next

	Wscript.Echo "Creating a new COM+ application:" 

	Wscript.Echo " Creating the catalogue object "
	Dim InmCat
	Set InmCat = CreateObject("COMAdmin.COMAdminCatalog") 	
	VerifyError 501

	wscript.echo " Get the collection of Applications"
	Dim collInmApps
	Set collInmApps = InmCat.GetCollection("Applications")
	VerifyCollectionError 502, InmCat

	Wscript.Echo " Populate the collection..." 
	collInmApps.Populate 
	VerifyCollectionError 503, InmCat
	
	Dim collInmAppObject
	For  Each collInmAppObject in collInmApps        
	    If collInmAppObject.Name = InMageProviderName Then
			WScript.echo "- Application " & InMageProviderName & " exists. Skipping installation."
			Wscript.Quit 0
	    End If
	Next
	
	Wscript.Echo " Add new application object to the above collection" 
	Dim InmApp
	Set InmApp = collInmApps.Add 
	VerifyCollectionError 504, InmCat

	Wscript.Echo " Set Application Name = " & InMageProviderName & " "
	InmApp.Value("Name") = InMageProviderName
	VerifyObjectError 505, InmCat, InmApp

	Wscript.Echo " Set Application Description = " & ProviderDesc & " "
	InmApp.Value("Description") = ProviderDesc 
	VerifyObjectError 506, InmCat, InmApp


	Wscript.Echo " Set Application accessibility check to true "
	InmApp.Value("ApplicationAccessChecksEnabled") = 1   
	VerifyObjectError 507, InmCat, InmApp


	Wscript.Echo " Set encryption for COM communication to true "
	InmApp.Value("Authentication") = 6	                  
	VerifyObjectError 508, InmCat, InmApp


	Wscript.Echo " Set secure references to true "
	InmApp.Value("AuthenticationCapability") = 2         
	VerifyObjectError 509, InmCat, InmApp


	Wscript.Echo " Set impersonation to false "
	InmApp.Value("ImpersonationLevel") = 2               
	VerifyObjectError 510, InmCat, InmApp

	Wscript.Echo " Save all the changes..."
	collInmApps.SaveChanges
	VerifyCollectionError 511, InmCat

	wscript.echo " Create Windows service running under Local System Account"
	InmCat.CreateServiceForApplication InMageProviderName, InMageProviderName , "SERVICE_DEMAND_START", "SERVICE_ERROR_NORMAL", "", ".\localsystem", "", 0
	VerifyCollectionError 512, InmCat

	wscript.echo " Add the DLL component to the Windows Service"
	InmCat.InstallComponent InMageProviderName, InmProviderDLL , "", ""
        VerifyCollectionError 513, InmCat

	'
	' Add the new role for the Local SYSTEM account
	'

	wscript.echo "Secure the COM+ application:"
	wscript.echo " Get collection of roles "
	Dim collInmRoles
	Set collInmRoles = collInmApps.GetCollection("Roles", InmApp.Key)
	VerifyCollectionError 525, collInmApps

	wscript.echo " Populate the collection..."
	collInmRoles.Populate
	VerifyCollectionError 526, collInmApps

	wscript.echo " Add new role to the collection"
	Dim InmRole
	Set InmRole = collInmRoles.Add
	VerifyCollectionError 527, collInmApps

	wscript.echo " Set Role name as Administrators "
	InmRole.Value("Name") = "Administrators"
	VerifyObjectError 528, collInmApps, InmRole

	wscript.echo " Set description as  Administrators group "
	InmRole.Value("Description") = "Administrators group"
	VerifyObjectError 529, collInmApps, InmRole

	wscript.echo " Save all the changes ..."
	collInmRoles.SaveChanges
	VerifyCollectionError 530, collInmApps
	
	'
	' Add users into InmRole
	'

	wscript.echo "Granting User Permissions:"
	Dim collInmUsersInRole
	Set collInmUsersInRole = collInmRoles.GetCollection("UsersInRole", InmRole.Key)
	VerifyCollectionError 540, collInmRoles

	wscript.echo " Populate..the collection of user roles."
	collInmUsersInRole.Populate
	VerifyCollectionError 541, collInmRoles

	wscript.echo " Add a new user"
	Dim InmUser
	Set InmUser = collInmUsersInRole.Add
	VerifyCollectionError 542, collInmRoles

	wscript.echo " Searching for the Administrators account ..."


	Dim strInmQuery
	strInmQuery = "select * from Win32_Account where SID='S-1-5-32-544' and localAccount=TRUE"
	Dim objInmSet
	set objInmSet = GetObject("winmgmts:\\.\root\cimv2").ExecQuery(strInmQuery)
	VerifyError 543
    If objInmSet.Count = 0 Then
	Set Err.number = 561
	Set Err.Description = "No Administrator Accounts obtained by WMI query."
	VerifyError 561
	End If

	Dim InmObj, InAccount
	for each InmObj in objInmSet
	    set InAccount = InmObj
		exit for
	next

	wscript.echo " Set user name = .\" & InAccount.Name & " "
	InmUser.Value("User") = ".\" & InAccount.Name
	VerifyObjectError 550, collInmRoles, InmUser

	wscript.echo " Add Local SYSTEM user"
	Set InmUser = collInmUsersInRole.Add
	VerifyCollectionError 551, collInmRoles
    
	Dim strInmQuerySys
	strInmQuerySys = "select * from Win32_Account where SID='S-1-5-18' and localAccount=TRUE"
	set objInmSet = GetObject("winmgmts:\\.\root\cimv2").ExecQuery(strInmQuerySys)
	VerifyError 544
    If objInmSet.Count = 0 Then
	Set Err.number = 562
	Set Err.Description = "No Local System Accounts obtained by WMI query."
	VerifyError 562	
	End If

	Dim InAccountSys
	for each InmObj in objInmSet
	    set InAccountSys = InmObj
		exit for
	next

	wscript.echo " Set user name = Local SYSTEM "
	InmUser.Value("User") = InAccountSys.Name
	VerifyObjectError 552, collInmRoles, InmUser

	wscript.echo " Save all the changes..."
	collInmUsersInRole.SaveChanges
	VerifyCollectionError 553, collInmRoles
	
	Set InmApp      = Nothing
	Set InmCat      = Nothing
	Set InmRole     = Nothing
	Set InmUser     = Nothing

	Set collInmApps = Nothing
	Set collInmRoles = Nothing
	Set collInmUsersInRole	= Nothing

	set objInmSet   = Nothing
	set InmObj      = Nothing

	Wscript.Echo "Completed Installation." 

	On Error GoTo 0
End Sub



' Uninstall the InMage VSS Provider

Sub Uninstall_InMageVssProvider
	On Error Resume Next

	Wscript.Echo "Unregistering the existing application..." 

	wscript.echo " Create the catalogue object"
	Dim InmCat
	Set InmCat = CreateObject("COMAdmin.COMAdminCatalog")
	VerifyError 801
	
	wscript.echo " Get the collection of Applications "
	Dim collInmApps
	Set collInmApps = InmCat.GetCollection("Applications")
	VerifyCollectionError 802, InmCat

	wscript.echo " Populate.the collection of Applications.."
	collInmApps.Populate
	VerifyCollectionError 803, InmCat
	
	wscript.echo " Lookup for " & InMageProviderName & " application..."
	Dim numApps
	numApps = collInmApps.Count
	Dim i
	For i = numApps - 1 To 0 Step -1
        wscript.echo " AppName : " & collInmApps.Item(i).Value("Name")
	    If collInmApps.Item(i).Value("Name") = InMageProviderName Then
	        collInmApps.Remove(i)
            VerifyCollectionError 804, InmCat
            WScript.echo "- Application " & InMageProviderName & " removed!"
	    exit for
	    End If
	Next
	
	wscript.echo " Saving all the changes..."
	collInmApps.SaveChanges
	VerifyCollectionError 805, InmCat

	Set collInmApps = Nothing
	Set InmCat      = Nothing

	Wscript.Echo " Completed UnInstallation." 

	On Error GoTo 0
End Sub




' Sub VerifyError

Sub VerifyError(lInmexitCode)
    If Err = 0 Then Exit Sub
    ShowVBSError lInmexitCode

    Wscript.Quit lInmexitCode
End Sub



' Sub VerifyCollectionError

Sub VerifyCollectionError(lInmExitCode, InmColl)
    If Err = 0 Then Exit Sub
    ShowVBSError lInmExitCode

    ShowComPlusError(InmColl.GetCollection("ErrorInfo"))
 
    Wscript.Quit lInmExitCode
End Sub



' Sub VerifyObjectError

Sub VerifyObjectError(exitCode, coll, object)
    If Err = 0 Then Exit Sub
    ShowVBSError exitCode

    ' ShowComPlusError(coll.GetCollection("ErrorInfo", object.Key))
    ShowComPlusError(coll.GetCollection("ErrorInfo"))

    Wscript.Quit exitCode
End Sub




' Sub ShowVBSError

Sub ShowVBSError(exitCode)
	If Err.number = VSS_E_OUT_OF_MEMORY Then
		exitCode = 514
	ElseIf Err.number = VSS_E_SERVICE_DB_LOCKED Then
		exitCode = 515
	ElseIf Err.number = VSS_E_SERVICE_ALREADY_EXISTS Then
		exitCode = 516
	ElseIf Err.number = VSS_E_SERVICE_MARKED_FOR_DELETION Then
		exitCode = 517
    ElseIf Err.number = VSS_E_ERROR_ACCESS_DENIED Then
		exitCode = 560
	End If
	
	WScript.Echo vbNewLine & "ERROR:"
    WScript.Echo "- Error code: " & Err & " [0x" & Hex(Err) & "]"
    WScript.Echo "- Exit code: " & exitCode
    WScript.Echo "- Description: " & Err.Description
    WScript.Echo "- Source: " & Err.Source
    WScript.Echo "- Help file: " & Err.Helpfile
    WScript.Echo "- Help context: " & Err.HelpContext
End Sub



' Sub ShowComPlusError

Sub ShowComPlusError(errors)

    errors.Populate

    WScript.Echo "COM+ Errors found: (" & errors.Count & ")"

    Dim error
    Dim val
    For val = 0 to errors.Count - 1
	Set error = errors.Item(val)
        WScript.Echo "(COM+ ERROR " & val & ") on " & error.Value("Name")
        WScript.Echo "ErrorCode: " & error.Value("ErrorCode") & " (0x" & Hex(error.Value("ErrorCode")) & ")"
        WScript.Echo "MajorErrorRef: " & error.Value("MajorRef")
        WScript.Echo "MinorErrorRef: " & error.Value("MinorRef")
    Next
End Sub

'' SIG '' Begin signature block
'' SIG '' MIIaywYJKoZIhvcNAQcCoIIavDCCGrgCAQExCzAJBgUr
'' SIG '' DgMCGgUAMGcGCisGAQQBgjcCAQSgWTBXMDIGCisGAQQB
'' SIG '' gjcCAR4wJAIBAQQQTvApFpkntU2P5azhDxfrqwIBAAIB
'' SIG '' AAIBAAIBAAIBADAhMAkGBSsOAwIaBQAEFG3tJz63bUWj
'' SIG '' ItjNPffNGLjLebnyoIIVgjCCBMMwggOroAMCAQICEzMA
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
'' SIG '' BgEEAYI3AgEVMCMGCSqGSIb3DQEJBDEWBBTvnebUPaM6
'' SIG '' v404qb0NONsz+RgvOTBuBgorBgEEAYI3AgEMMWAwXqBA
'' SIG '' gD4ATQBpAGMAcgBvAHMAbwBmAHQAIABNAGkAZwByAGEA
'' SIG '' dABpAG8AbgAgAEEAYwBjAGUAbABlAHIAYQB0AG8AcqEa
'' SIG '' gBhodHRwOi8vd3d3Lm1pY3Jvc29mdC5jb20wDQYJKoZI
'' SIG '' hvcNAQEBBQAEggEAJHDbwRhUYM+lC7WQ6Jau6GYbINk5
'' SIG '' cQvQ/0DY4bhB2bdjUYBbQ+bPoBNyHTyLzmVMiChVdWWr
'' SIG '' za3NdA9m0M2rMogSn0vd2FOZwgqdfxlt3Gi74TzG2MkG
'' SIG '' Y9ZqPRvJfJ0jWATHNwRr0pEjk4S5lG7gmydVw+6peCbs
'' SIG '' BJJAV+ctYCR1g6TQQRQvaaekT7QuBGCTCmY1MRIoc1Gk
'' SIG '' 1MtXRuZm7IHnnqZKa/5D48XoQjfMmyUvba7zMDz3RJrv
'' SIG '' faa6LDST0QVQUNRovhOdq3RSpGg3xGEw+M//YNXw0YBb
'' SIG '' 5jdkoSnFaNiYCsepPV0cey8jeTsrhOzObwZb28ZG6JQH
'' SIG '' r7e2paGCAigwggIkBgkqhkiG9w0BCQYxggIVMIICEQIB
'' SIG '' ATCBjjB3MQswCQYDVQQGEwJVUzETMBEGA1UECBMKV2Fz
'' SIG '' aGluZ3RvbjEQMA4GA1UEBxMHUmVkbW9uZDEeMBwGA1UE
'' SIG '' ChMVTWljcm9zb2Z0IENvcnBvcmF0aW9uMSEwHwYDVQQD
'' SIG '' ExhNaWNyb3NvZnQgVGltZS1TdGFtcCBQQ0ECEzMAAABM
'' SIG '' oehNzLR0ezsAAAAAAEwwCQYFKw4DAhoFAKBdMBgGCSqG
'' SIG '' SIb3DQEJAzELBgkqhkiG9w0BBwEwHAYJKoZIhvcNAQkF
'' SIG '' MQ8XDTE0MDgyMjE0MzkxNVowIwYJKoZIhvcNAQkEMRYE
'' SIG '' FMkyruuUX3wXB6y40WhR3SIOB/tlMA0GCSqGSIb3DQEB
'' SIG '' BQUABIIBAGPKcFjM/ICjx41x93LKavw/vym7VVc0N1BU
'' SIG '' lqD28F80HbNju9V0vAY8pa4v+ML3LYdDej6NXM17IbBK
'' SIG '' PcyUZM6QRHbvzUuVNbeA7XHR7EArSRj2nibuP3obzfAp
'' SIG '' oy7TXsflc5XZreWrzdzFV+XatiOTKn3KYDDWFBgovNRw
'' SIG '' NIeUhejn1O49fE/nVFulRO5PpJGhtfATCa/iO5/TWdPn
'' SIG '' c5BUQmsedyD7D3mO58ppPqhMiCWfejUBFcgfdYA2xyfP
'' SIG '' mPRIEMKhflfwSfjBtH6zN5Jnh8yqObCsitYNS5F6RBa3
'' SIG '' YcsfdiPTokVC/iukBW8Xj5uXd09tI6ZZFrGm7sUOoXE=
'' SIG '' End signature block
