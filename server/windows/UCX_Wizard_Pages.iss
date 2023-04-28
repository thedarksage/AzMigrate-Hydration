
[Code]
var
  CSMultipleNicsWithPort_AfterServerChoicePage_Http, CSMultipleNicsWithPort_AfterServerChoicePage_Https, CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https : TWizardPage;
  PSMultipleNicsWithoutPort_AfterServerChoicePage : TWizardPage;
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http, CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Https, CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https : TWizardPage; 
  PSMultipleNicsWithoutPort_After_CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage : TWizardPage;
  PSMultipleNicsWithoutPort_AfterAddServerChoicePage : TWizardPage;
  PIDetailsPage_After_CSMultipleNicsWithPort_AfterServerChoicePage : TInputQueryWizardPage;
  PIDetailsPage_After_PSMultipleNicsWithoutPort_After_CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage : TInputQueryWizardPage;
  CSDetailsPage_After_PSMultipleNicsWithoutPort_AfterServerChoicePage_Http, CSDetailsPage_After_PSMultipleNicsWithoutPort_AfterServerChoicePage_Https, GetpassphrasePage_After_CSDetailsPage_PS : TInputQueryWizardPage;
  CSDetailsPage_After_PSMultipleNicsWithoutPort_AfterServerChoicePage : TInputQueryWizardPage;
  CSDetailsPage_After_PSMultipleNicsWithoutPort_AfterAddServerChoicePage : TInputQueryWizardPage;
  PrimaryCXDetailsPage : TInputQueryWizardPage;
  PIDetailsPage_After_PrimaryCXDetailsPage : TInputQueryWizardPage;  
  RoleOfCXServer_AfterServerChoicePage : TInputOptionWizardPage;
 	ServerChoicePage,AddServerChoicePage,CommunicationChoicePage_PS, CommunicationChoicePage_PS_CS : TInputOptionWizardPage;
  PIDetailsPage_After_CSMultipleNicsWithPort_After_PSMultipleNicsWithoutPort_AfterServerChoicePage : TInputQueryWizardPage;
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_CheckListBox, CSMultipleNicsWithPort_AfterServerChoicePage_Https_CheckListBox, CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https_CheckListBox : TNewCheckListBox;
  PSMultipleNicsWithoutPort_AfterServerChoicePage_CheckListBox: TNewCheckListBox;
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_CheckListBox, CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Https_CheckListBox, CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https_CheckListBox : TNewCheckListBox;
  PSMultipleNicsWithoutPort_After_CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_CheckListBox : TNewCheckListBox;
  PSMultipleNicsWithoutPort_AfterAddServerChoicePage_CheckListBox : TNewCheckListBox;
  SQlDetailsPage : TInputQueryWizardPage;
  
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_EditBoxFor_PortValue, CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Https_EditBoxFor_PortValue, CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https_EditBoxFor_PortValue, CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https_EditBox_PortValue: TNewEdit;
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_EditBoxFor_PortValue,CSMultipleNicsWithPort_AfterServerChoicePage_Http_EditBox_PortValue, CSMultipleNicsWithPort_AfterServerChoicePage_Https_EditBoxFor_PortValue, CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https_EditBoxFor_PortValue,CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https_EditBox_PortValue : TNewEdit;
  
  
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_StaticText_NicSelection, CSMultipleNicsWithPort_AfterServerChoicePage_Https_StaticText_NicSelection, CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https_StaticText_NicSelection: TNewStaticText;
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_StaticText_PortSelection, CSMultipleNicsWithPort_AfterServerChoicePage_Http_Static_PortSelection ,CSMultipleNicsWithPort_AfterServerChoicePage_Https_StaticText_PortSelection, CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https_StaticText_PortSelection, CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https_Static_PortSelection  : TNewStaticText;
  PSMultipleNicsWithoutPort_AfterServerChoicePage_StaticText_NicSelection  : TNewStaticText;
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_StaticText_NicSelection, CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Https_StaticText_NicSelection, CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https_StaticText_NicSelection  : TNewStaticText;
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_StaticText_PortSelection, CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Https_StaticText_PortSelection, CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https_StaticText_PortSelection, CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https_Static_PortSelection  : TNewStaticText;
  PSMultipleNicsWithoutPort_After_CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_StaticText_NicSelection  : TNewStaticText;
  PSMultipleNicsWithoutPort_AfterAddServerChoicePage_StaticText_NicSelection  : TNewStaticText;
  CheckBox, CheckBox_AfterRole: TNewCheckBox;
  CS_External_IP, CS_External_Port : Integer;

  a_strTextfile : TArrayOfString;
	iLineCounter : Integer;
	Nic_Section_Name,Nic_Name,IP,IPString,Values,InputStr,Nic_IP_Count : String;
	Nic_StringList : TStringList;
  LoopCount : Integer;    
  Total_IP_Count: integer;
  Strong_Pwd_req_Link : TLabel;
  
  
  
Procedure CS_MultipleNics_WithPort_AfterServerChoicePage_Http();
begin

  CSMultipleNicsWithPort_AfterServerChoicePage_Http := CreateCustomPage(SQlDetailsPage.ID, 'Nic Selection For Configuration Server', '');
  
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_StaticText_NicSelection := TNewStaticText.Create(CSMultipleNicsWithPort_AfterServerChoicePage_Http);
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_StaticText_NicSelection.Top := ScaleY(4);
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_StaticText_NicSelection.Caption := 'Please choose a Nic for Configuration Server';
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_StaticText_NicSelection.AutoSize := True;
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_StaticText_NicSelection.Parent := CSMultipleNicsWithPort_AfterServerChoicePage_Http.Surface;
  
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_CheckListBox := TNewCheckListBox.Create(CSMultipleNicsWithPort_AfterServerChoicePage_Http);
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_CheckListBox.Width := CSMultipleNicsWithPort_AfterServerChoicePage_Http.SurfaceWidth;
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_CheckListBox.Top := ScaleY(30)- CSMultipleNicsWithPort_AfterServerChoicePage_Http_StaticText_NicSelection.Top
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_CheckListBox.Height := CSMultipleNicsWithPort_AfterServerChoicePage_Http_CheckListBox.Top + ScaleY(120);
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_CheckListBox.Flat := True;
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_CheckListBox.Parent := CSMultipleNicsWithPort_AfterServerChoicePage_Http.Surface;
  
 LoadStringsFromFile(ExpandConstant ('{sd}\Temp\nicdetails.conf'), a_strTextfile);
   
 for iLineCounter := 0 to GetArrayLength(a_strTextfile)-1 do
 begin
   if (Pos('[NIC', a_strTextfile[iLineCounter]) > 0) then
   begin
          Nic_StringList := TStringList.Create;
          StringChangeEx(a_strTextfile[iLineCounter],'[',',',FALSE);
          StringChangeEx(a_strTextfile[iLineCounter],']',',',FALSE);
          Nic_StringList.CommaText :=a_strTextfile[iLineCounter];
       
          Nic_Section_Name := Nic_StringList.Strings[1]; 
          Nic_Name := GetIniString( Nic_Section_Name, 'NICNAME','', ExpandConstant ('{sd}\Temp\nicdetails.conf'));
          Nic_IP_Count := GetIniString( Nic_Section_Name, 'NIC IP COUNT','', ExpandConstant ('{sd}\Temp\nicdetails.conf'));
          
          if StrToInt(Nic_IP_Count) <> 0 then
           begin
                  CSMultipleNicsWithPort_AfterServerChoicePage_Http_CheckListBox.AddRadioButton(Nic_Name, '', 0, False, True, nil);
                  CSMultipleNicsWithPort_AfterServerChoicePage_Http_CheckListBox.CheckItem(0,coCheck);
           end
           else if StrToInt(Nic_IP_Count) = 0 then
           begin
                  CSMultipleNicsWithPort_AfterServerChoicePage_Http_CheckListBox.AddRadioButton(Nic_Name, '', 0, False, False, nil);
                  CSMultipleNicsWithPort_AfterServerChoicePage_Http_CheckListBox.CheckItem(0,coCheck);
                  
           end;
           
          LoopCount := 0;
      		while (LoopCount <= StrToInt(Nic_IP_Count)) do
      		begin
      			IPString := 'IP' + IntToStr(LoopCount);
            IP := GetIniString( Nic_Section_Name,IPString,'', ExpandConstant('{sd}\Temp\nicdetails.conf'));
            if IP <> '' then
            begin
                CSMultipleNicsWithPort_AfterServerChoicePage_Http_CheckListBox.AddRadioButton(IP, '', 1, False, True, nil);  
            end;
            LoopCount := LoopCount + 1;
          end;
      end;
  end;

  CSMultipleNicsWithPort_AfterServerChoicePage_Http_StaticText_PortSelection := TNewStaticText.Create(CSMultipleNicsWithPort_AfterServerChoicePage_Http);
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_StaticText_PortSelection.Top := CSMultipleNicsWithPort_AfterServerChoicePage_Http_CheckListBox.Top + CSMultipleNicsWithPort_AfterServerChoicePage_Http_CheckListBox.Height + ScaleY(15);
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_StaticText_PortSelection.Caption := 'Please enter a Http Port Number for Configuration Server';
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_StaticText_PortSelection.AutoSize := True;
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_StaticText_PortSelection.Parent := CSMultipleNicsWithPort_AfterServerChoicePage_Http.Surface;
  
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_EditBoxFor_PortValue := TNewEdit.Create(CSMultipleNicsWithPort_AfterServerChoicePage_Http);
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_EditBoxFor_PortValue.Top := CSMultipleNicsWithPort_AfterServerChoicePage_Http_StaticText_PortSelection.Top + CSMultipleNicsWithPort_AfterServerChoicePage_Http_StaticText_PortSelection.Height + ScaleY(5);
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_EditBoxFor_PortValue.Width := CSMultipleNicsWithPort_AfterServerChoicePage_Http.SurfaceWidth - ScaleX(6);
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_EditBoxFor_PortValue.Text := '80';
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_EditBoxFor_PortValue.Parent := CSMultipleNicsWithPort_AfterServerChoicePage_Http.Surface;

end;

Procedure CS_MultipleNics_WithPort_AfterServerChoicePage_Https();
begin

  CSMultipleNicsWithPort_AfterServerChoicePage_Https := CreateCustomPage(SQlDetailsPage.ID, 'Nic Selection For Configuration Server', '');
  
  CSMultipleNicsWithPort_AfterServerChoicePage_Https_StaticText_NicSelection := TNewStaticText.Create(CSMultipleNicsWithPort_AfterServerChoicePage_Https);
  CSMultipleNicsWithPort_AfterServerChoicePage_Https_StaticText_NicSelection.Top := ScaleY(4);
  CSMultipleNicsWithPort_AfterServerChoicePage_Https_StaticText_NicSelection.Caption := 'Please choose a Nic for Configuration Server';
  CSMultipleNicsWithPort_AfterServerChoicePage_Https_StaticText_NicSelection.AutoSize := True;
  CSMultipleNicsWithPort_AfterServerChoicePage_Https_StaticText_NicSelection.Parent := CSMultipleNicsWithPort_AfterServerChoicePage_Https.Surface;
  
  CSMultipleNicsWithPort_AfterServerChoicePage_Https_CheckListBox := TNewCheckListBox.Create(CSMultipleNicsWithPort_AfterServerChoicePage_Https);
  CSMultipleNicsWithPort_AfterServerChoicePage_Https_CheckListBox.Width := CSMultipleNicsWithPort_AfterServerChoicePage_Https.SurfaceWidth;
  CSMultipleNicsWithPort_AfterServerChoicePage_Https_CheckListBox.Top := ScaleY(30)- CSMultipleNicsWithPort_AfterServerChoicePage_Https_StaticText_NicSelection.Top
  CSMultipleNicsWithPort_AfterServerChoicePage_Https_CheckListBox.Height := CSMultipleNicsWithPort_AfterServerChoicePage_Https_CheckListBox.Top + ScaleY(120);
  CSMultipleNicsWithPort_AfterServerChoicePage_Https_CheckListBox.Flat := True;
  CSMultipleNicsWithPort_AfterServerChoicePage_Https_CheckListBox.Parent := CSMultipleNicsWithPort_AfterServerChoicePage_Https.Surface;
  
 LoadStringsFromFile(ExpandConstant ('{sd}\Temp\nicdetails.conf'), a_strTextfile);
   
 for iLineCounter := 0 to GetArrayLength(a_strTextfile)-1 do
 begin
   if (Pos('[NIC', a_strTextfile[iLineCounter]) > 0) then
   begin
          Nic_StringList := TStringList.Create;
          StringChangeEx(a_strTextfile[iLineCounter],'[',',',FALSE);
          StringChangeEx(a_strTextfile[iLineCounter],']',',',FALSE);
          Nic_StringList.CommaText :=a_strTextfile[iLineCounter];
       
          Nic_Section_Name := Nic_StringList.Strings[1]; 
          Nic_Name := GetIniString( Nic_Section_Name, 'NICNAME','', ExpandConstant ('{sd}\Temp\nicdetails.conf'));
          Nic_IP_Count := GetIniString( Nic_Section_Name, 'NIC IP COUNT','', ExpandConstant ('{sd}\Temp\nicdetails.conf'));
          
          if StrToInt(Nic_IP_Count) <> 0 then
           begin
                  CSMultipleNicsWithPort_AfterServerChoicePage_Https_CheckListBox.AddRadioButton(Nic_Name, '', 0, False, True, nil);
                  CSMultipleNicsWithPort_AfterServerChoicePage_Https_CheckListBox.CheckItem(0,coCheck);
           end
           else if StrToInt(Nic_IP_Count) = 0 then
           begin
                  CSMultipleNicsWithPort_AfterServerChoicePage_Https_CheckListBox.AddRadioButton(Nic_Name, '', 0, False, False, nil);
                  CSMultipleNicsWithPort_AfterServerChoicePage_Https_CheckListBox.CheckItem(0,coCheck);
                  
           end;
           
          LoopCount := 0;
      		while (LoopCount <= StrToInt(Nic_IP_Count)) do
      		begin
      			IPString := 'IP' + IntToStr(LoopCount);
            IP := GetIniString( Nic_Section_Name,IPString,'', ExpandConstant('{sd}\Temp\nicdetails.conf'));
            if IP <> '' then
            begin
                CSMultipleNicsWithPort_AfterServerChoicePage_Https_CheckListBox.AddRadioButton(IP, '', 1, False, True, nil);  
            end;
            LoopCount := LoopCount + 1;
          end;
      end;
  end;

  CSMultipleNicsWithPort_AfterServerChoicePage_Https_StaticText_PortSelection := TNewStaticText.Create(CSMultipleNicsWithPort_AfterServerChoicePage_Https);
  CSMultipleNicsWithPort_AfterServerChoicePage_Https_StaticText_PortSelection.Top := CSMultipleNicsWithPort_AfterServerChoicePage_Https_CheckListBox.Top + CSMultipleNicsWithPort_AfterServerChoicePage_Https_CheckListBox.Height + ScaleY(15);
  CSMultipleNicsWithPort_AfterServerChoicePage_Https_StaticText_PortSelection.Caption := 'Please enter a Https Port Number for Configuration Server';
  CSMultipleNicsWithPort_AfterServerChoicePage_Https_StaticText_PortSelection.AutoSize := True;
  CSMultipleNicsWithPort_AfterServerChoicePage_Https_StaticText_PortSelection.Parent := CSMultipleNicsWithPort_AfterServerChoicePage_Https.Surface;
  
  CSMultipleNicsWithPort_AfterServerChoicePage_Https_EditBoxFor_PortValue := TNewEdit.Create(CSMultipleNicsWithPort_AfterServerChoicePage_Https);
  CSMultipleNicsWithPort_AfterServerChoicePage_Https_EditBoxFor_PortValue.Top := CSMultipleNicsWithPort_AfterServerChoicePage_Https_StaticText_PortSelection.Top + CSMultipleNicsWithPort_AfterServerChoicePage_Https_StaticText_PortSelection.Height + ScaleY(5);
  CSMultipleNicsWithPort_AfterServerChoicePage_Https_EditBoxFor_PortValue.Width := CSMultipleNicsWithPort_AfterServerChoicePage_Https.SurfaceWidth - ScaleX(6);
  CSMultipleNicsWithPort_AfterServerChoicePage_Https_EditBoxFor_PortValue.Text := '443';
  CSMultipleNicsWithPort_AfterServerChoicePage_Https_EditBoxFor_PortValue.Parent := CSMultipleNicsWithPort_AfterServerChoicePage_Https.Surface;

end;

Procedure CS_MultipleNics_WithPort_AfterServerChoicePage_Http_Https();
begin

  CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https := CreateCustomPage(SQlDetailsPage.ID, 'Nic Selection For Configuration Server', '');
  
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https_StaticText_NicSelection := TNewStaticText.Create(CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https);
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https_StaticText_NicSelection.Top := ScaleY(4);
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https_StaticText_NicSelection.Caption := 'Please choose a Nic for Configuration Server';
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https_StaticText_NicSelection.AutoSize := True;
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https_StaticText_NicSelection.Parent := CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https.Surface;
  
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https_CheckListBox := TNewCheckListBox.Create(CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https);
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https_CheckListBox.Width := CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https.SurfaceWidth;
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https_CheckListBox.Top := ScaleY(30)- CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https_StaticText_NicSelection.Top
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https_CheckListBox.Height := CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https_CheckListBox.Top + ScaleY(80);
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https_CheckListBox.Flat := True;
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https_CheckListBox.Parent := CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https.Surface;
  
 LoadStringsFromFile(ExpandConstant ('{sd}\Temp\nicdetails.conf'), a_strTextfile);
   
 for iLineCounter := 0 to GetArrayLength(a_strTextfile)-1 do
 begin
   if (Pos('[NIC', a_strTextfile[iLineCounter]) > 0) then
   begin
          Nic_StringList := TStringList.Create;
          StringChangeEx(a_strTextfile[iLineCounter],'[',',',FALSE);
          StringChangeEx(a_strTextfile[iLineCounter],']',',',FALSE);
          Nic_StringList.CommaText :=a_strTextfile[iLineCounter];
       
          Nic_Section_Name := Nic_StringList.Strings[1]; 
          Nic_Name := GetIniString( Nic_Section_Name, 'NICNAME','', ExpandConstant ('{sd}\Temp\nicdetails.conf'));
          Nic_IP_Count := GetIniString( Nic_Section_Name, 'NIC IP COUNT','', ExpandConstant ('{sd}\Temp\nicdetails.conf'));
          
          if StrToInt(Nic_IP_Count) <> 0 then
           begin
                  CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https_CheckListBox.AddRadioButton(Nic_Name, '', 0, False, True, nil);
                  CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https_CheckListBox.CheckItem(0,coCheck);
           end
           else if StrToInt(Nic_IP_Count) = 0 then
           begin
                  CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https_CheckListBox.AddRadioButton(Nic_Name, '', 0, False, False, nil);
                  CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https_CheckListBox.CheckItem(0,coCheck);
                  
           end;
           
          LoopCount := 0;
      		while (LoopCount <= StrToInt(Nic_IP_Count)) do
      		begin
      			IPString := 'IP' + IntToStr(LoopCount);
            IP := GetIniString( Nic_Section_Name,IPString,'', ExpandConstant('{sd}\Temp\nicdetails.conf'));
            if IP <> '' then
            begin
                CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https_CheckListBox.AddRadioButton(IP, '', 1, False, True, nil);  
            end;
            LoopCount := LoopCount + 1;
          end;
      end;
  end;

  CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https_StaticText_PortSelection := TNewStaticText.Create(CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https);
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https_StaticText_PortSelection.Top := CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https_CheckListBox.Top + CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https_CheckListBox.Height + ScaleY(8);
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https_StaticText_PortSelection.Caption := 'Please enter a Http Port Number for Configuration Server';
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https_StaticText_PortSelection.AutoSize := True;
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https_StaticText_PortSelection.Parent := CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https.Surface;
  
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https_EditBoxFor_PortValue := TNewEdit.Create(CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https);
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https_EditBoxFor_PortValue.Top := CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https_StaticText_PortSelection.Top + CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https_StaticText_PortSelection.Height + ScaleY(3);
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https_EditBoxFor_PortValue.Width := CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https.SurfaceWidth - ScaleX(6);
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https_EditBoxFor_PortValue.Text := '80';
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https_EditBoxFor_PortValue.Parent := CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https.Surface;
  
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https_Static_PortSelection := TNewStaticText.Create(CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https);
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https_Static_PortSelection.Top := CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https_EditBoxFor_PortValue.Top + CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https_EditBoxFor_PortValue.Height + ScaleY(8);
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https_Static_PortSelection.Caption := 'Please enter a Https Port Number for Configuration Server';
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https_Static_PortSelection.AutoSize := True;
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https_Static_PortSelection.Parent := CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https.Surface;
  
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https_EditBox_PortValue := TNewEdit.Create(CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https);
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https_EditBox_PortValue.Top := CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https_Static_PortSelection.Top + CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https_Static_PortSelection.Height + ScaleY(3);
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https_EditBox_PortValue.Width := CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https.SurfaceWidth - ScaleX(6);
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https_EditBox_PortValue.Text := '443';
  CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https_EditBox_PortValue.Parent := CSMultipleNicsWithPort_AfterServerChoicePage_Http_Https.Surface;

end;

Procedure PI_DetailsPage_After_CS_MultipleNics_WithPort_AfterServerChoicePage();
var
      DomainUsrName,DomainName,UsrName,ForwardSlash : String;
begin
  		DomainName := ExpandConstant(GetEnv('USERDOMAIN'));
      UsrName:= ExpandConstant( +GetUserNameString);
      ForwardSlash := '\';
      DomainUsrName := DomainName + ForwardSlash + UsrName;
    
      PIDetailsPage_After_CSMultipleNicsWithPort_AfterServerChoicePage := CreateInputQueryPage(CSMultipleNicsWithPort_AfterServerChoicePage_Http.ID,'Push Server Details','','Please enter following data and click Next.');
  		PIDetailsPage_After_CSMultipleNicsWithPort_AfterServerChoicePage.Add('Domain_name\User_name ' + #13 + '[Ex:- mydomain\cxadmin - "cxadmin" should have local system admin privileges ]',False);
			PIDetailsPage_After_CSMultipleNicsWithPort_AfterServerChoicePage.Add('Password',True);
			PIDetailsPage_After_CSMultipleNicsWithPort_AfterServerChoicePage.Values[0] :=  DomainUsrName; 
end;

Procedure PS_MultipleNics_WithoutPort_AfterServerChoicePage();
begin

        PSMultipleNicsWithoutPort_AfterServerChoicePage := CreateCustomPage(SQlDetailsPage.ID, 'Nic Selection For Process Server', '');
        
        PSMultipleNicsWithoutPort_AfterServerChoicePage_StaticText_NicSelection := TNewStaticText.Create(PSMultipleNicsWithoutPort_AfterServerChoicePage);
        PSMultipleNicsWithoutPort_AfterServerChoicePage_StaticText_NicSelection.Top := ScaleY(4);
        PSMultipleNicsWithoutPort_AfterServerChoicePage_StaticText_NicSelection.Caption := 'Please choose a Nic for Process Server';
        PSMultipleNicsWithoutPort_AfterServerChoicePage_StaticText_NicSelection.AutoSize := True;
        PSMultipleNicsWithoutPort_AfterServerChoicePage_StaticText_NicSelection.Parent := PSMultipleNicsWithoutPort_AfterServerChoicePage.Surface;
        
        PSMultipleNicsWithoutPort_AfterServerChoicePage_CheckListBox := TNewCheckListBox.Create(PSMultipleNicsWithoutPort_AfterServerChoicePage);
        PSMultipleNicsWithoutPort_AfterServerChoicePage_CheckListBox.Width := PSMultipleNicsWithoutPort_AfterServerChoicePage.SurfaceWidth;
        PSMultipleNicsWithoutPort_AfterServerChoicePage_CheckListBox.Top := ScaleY(30)- PSMultipleNicsWithoutPort_AfterServerChoicePage_StaticText_NicSelection.Top
        PSMultipleNicsWithoutPort_AfterServerChoicePage_CheckListBox.Height := PSMultipleNicsWithoutPort_AfterServerChoicePage_CheckListBox.Top + ScaleY(120);
        PSMultipleNicsWithoutPort_AfterServerChoicePage_CheckListBox.Flat := True;
        PSMultipleNicsWithoutPort_AfterServerChoicePage_CheckListBox.Parent := PSMultipleNicsWithoutPort_AfterServerChoicePage.Surface;
        
       LoadStringsFromFile(ExpandConstant ('{sd}\Temp\nicdetails.conf'), a_strTextfile);
         
       for iLineCounter := 0 to GetArrayLength(a_strTextfile)-1 do
       begin
         if (Pos('[NIC', a_strTextfile[iLineCounter]) > 0) then
         begin
                Nic_StringList := TStringList.Create;
                StringChangeEx(a_strTextfile[iLineCounter],'[',',',FALSE);
                StringChangeEx(a_strTextfile[iLineCounter],']',',',FALSE);
                Nic_StringList.CommaText :=a_strTextfile[iLineCounter];
             
                Nic_Section_Name := Nic_StringList.Strings[1]; 
                Nic_Name := GetIniString( Nic_Section_Name, 'NICNAME','', ExpandConstant ('{sd}\Temp\nicdetails.conf'));
                Nic_IP_Count := GetIniString( Nic_Section_Name, 'NIC IP COUNT','', ExpandConstant ('{sd}\Temp\nicdetails.conf'));
                
                if StrToInt(Nic_IP_Count) <> 0 then
                 begin
                        PSMultipleNicsWithoutPort_AfterServerChoicePage_CheckListBox.AddRadioButton(Nic_Name, '', 0, False, True, nil);
                        PSMultipleNicsWithoutPort_AfterServerChoicePage_CheckListBox.CheckItem(0,coCheck);
                 end
                 else if StrToInt(Nic_IP_Count) = 0 then
                 begin
                        PSMultipleNicsWithoutPort_AfterServerChoicePage_CheckListBox.AddRadioButton(Nic_Name, '', 0, False, False, nil);
                        PSMultipleNicsWithoutPort_AfterServerChoicePage_CheckListBox.CheckItem(0,coCheck);
                 end;
                 
                LoopCount := 0;
            		while (LoopCount <= StrToInt(Nic_IP_Count)) do
            		begin
            			IPString := 'IP' + IntToStr(LoopCount);
                  IP := GetIniString( Nic_Section_Name,IPString,'', ExpandConstant('{sd}\Temp\nicdetails.conf'));
                  if IP <> '' then
                  begin
                      PSMultipleNicsWithoutPort_AfterServerChoicePage_CheckListBox.AddRadioButton(IP, '', 1, False, True, nil);  
                  end;
                  LoopCount := LoopCount + 1;
                end;
            end;
        end;
end;

Procedure CS_DetailsPage_After_PS_MultipleNics_WithoutPort_AfterServerChoicePage_Http();
begin
			CSDetailsPage_After_PSMultipleNicsWithoutPort_AfterServerChoicePage_Http := CreateInputQueryPage(PSMultipleNicsWithoutPort_AfterServerChoicePage.ID,
													'Configuration Server Details',
													'',
													'Please enter following data and click Next.');
			CSDetailsPage_After_PSMultipleNicsWithoutPort_AfterServerChoicePage_Http.Add('Configuration Server IP Address',False);
			CSDetailsPage_After_PSMultipleNicsWithoutPort_AfterServerChoicePage_Http.Add('Configuration Server Http Port Number',False);
			CSDetailsPage_After_PSMultipleNicsWithoutPort_AfterServerChoicePage_Http.Values[1] := '80';
end;

Procedure CS_DetailsPage_After_PS_MultipleNics_WithoutPort_AfterServerChoicePage_Https();
begin
			CSDetailsPage_After_PSMultipleNicsWithoutPort_AfterServerChoicePage_Https := CreateInputQueryPage(PSMultipleNicsWithoutPort_AfterServerChoicePage.ID,
													'Configuration Server Details',
													'',
													'Please enter following data and click Next.');
			CSDetailsPage_After_PSMultipleNicsWithoutPort_AfterServerChoicePage_Https.Add('Configuration Server IP Address',False);
			CSDetailsPage_After_PSMultipleNicsWithoutPort_AfterServerChoicePage_Https.Add('Configuration Server Https Port Number',False);
			CSDetailsPage_After_PSMultipleNicsWithoutPort_AfterServerChoicePage_Https.Values[1] := '443';
end;

Procedure Getpassphrase_Page_After_CSDetailsPage_PS();
begin
			GetpassphrasePage_After_CSDetailsPage_PS := CreateInputQueryPage(CSDetailsPage_After_PSMultipleNicsWithoutPort_AfterServerChoicePage_Http.ID,
													'CS Passphrase Details',
													'',
													'Please enter following data and click Next.');
			GetpassphrasePage_After_CSDetailsPage_PS.Add('Configuration Server Passphrase value',False);
end;

Procedure RoleOfCXServerPage_AfterServerChoicePage();
begin
        RoleOfCXServer_AfterServerChoicePage := CreateInputOptionPage(SQlDetailsPage.ID,
     													'Role Of CX Server',
      													'',
      													'Choose one of the following Roles for the CX server and click Next.',
      													True,False);
   			RoleOfCXServer_AfterServerChoicePage.Add('Primary');
   			RoleOfCXServer_AfterServerChoicePage.Add('Standby');
   			RoleOfCXServer_AfterServerChoicePage.SelectedValueIndex := 0;
end;


Procedure CS_MultipleNics_WithPort_After_RoleOfCXServerPage_AfterServerChoicePage_Http();
begin

  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http := CreateCustomPage(RoleOfCXServer_AfterServerChoicePage.ID, 'Nic Selection For Configuration server', '');
  
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_StaticText_NicSelection := TNewStaticText.Create(CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http);
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_StaticText_NicSelection.Top := ScaleY(4);
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_StaticText_NicSelection.Caption := 'Please choose a Nic for Configuration Server';
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_StaticText_NicSelection.AutoSize := True;
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_StaticText_NicSelection.Parent := CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http.Surface;
  
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_CheckListBox := TNewCheckListBox.Create(CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http);
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_CheckListBox.Width := CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http.SurfaceWidth;
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_CheckListBox.Top := ScaleY(30)- CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_StaticText_NicSelection.Top
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_CheckListBox.Height := CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_CheckListBox.Top + ScaleY(120);
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_CheckListBox.Flat := True;
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_CheckListBox.Parent := CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http.Surface;
  
 LoadStringsFromFile(ExpandConstant ('{sd}\Temp\nicdetails.conf'), a_strTextfile);
   
 for iLineCounter := 0 to GetArrayLength(a_strTextfile)-1 do
 begin
   if (Pos('[NIC', a_strTextfile[iLineCounter]) > 0) then
   begin
          Nic_StringList := TStringList.Create;
          StringChangeEx(a_strTextfile[iLineCounter],'[',',',FALSE);
          StringChangeEx(a_strTextfile[iLineCounter],']',',',FALSE);
          Nic_StringList.CommaText :=a_strTextfile[iLineCounter];
       
          Nic_Section_Name := Nic_StringList.Strings[1]; 
          Nic_Name := GetIniString( Nic_Section_Name, 'NICNAME','', ExpandConstant ('{sd}\Temp\nicdetails.conf'));
          Nic_IP_Count := GetIniString( Nic_Section_Name, 'NIC IP COUNT','', ExpandConstant ('{sd}\Temp\nicdetails.conf'));
          
          if StrToInt(Nic_IP_Count) <> 0 then
           begin
                  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_CheckListBox.AddRadioButton(Nic_Name, '', 0, False, True, nil);
                  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_CheckListBox.CheckItem(0,coCheck);
           end
           else if StrToInt(Nic_IP_Count) = 0 then
           begin
                  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_CheckListBox.AddRadioButton(Nic_Name, '', 0, False, False, nil);
                  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_CheckListBox.CheckItem(0,coCheck);
           end;
           
          LoopCount := 0;
      		while (LoopCount <= StrToInt(Nic_IP_Count)) do
      		begin
      			IPString := 'IP' + IntToStr(LoopCount);
            IP := GetIniString( Nic_Section_Name,IPString,'', ExpandConstant('{sd}\Temp\nicdetails.conf'));
            if IP <> '' then
            begin
                CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_CheckListBox.AddRadioButton(IP, '', 1, False, True, nil);  
            end;
            LoopCount := LoopCount + 1;
          end;
      end;
  end;

  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_StaticText_PortSelection := TNewStaticText.Create(CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http);
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_StaticText_PortSelection.Top := CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_CheckListBox.Top + CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_CheckListBox.Height + ScaleY(15);
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_StaticText_PortSelection.Caption := 'Please enter a Http Port Number for Configuration Server';
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_StaticText_PortSelection.AutoSize := True;
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_StaticText_PortSelection.Parent := CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http.Surface;
  
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_EditBoxFor_PortValue := TNewEdit.Create(CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http);
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_EditBoxFor_PortValue.Top := CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_StaticText_PortSelection.Top + CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_StaticText_PortSelection.Height + ScaleY(5);
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_EditBoxFor_PortValue.Width := CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http.SurfaceWidth - ScaleX(6);
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_EditBoxFor_PortValue.Text := '80';
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_EditBoxFor_PortValue.Parent := CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http.Surface;

end;

Procedure CS_MultipleNics_WithPort_After_RoleOfCXServerPage_AfterServerChoicePage_Https();
begin

  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Https := CreateCustomPage(RoleOfCXServer_AfterServerChoicePage.ID, 'Nic Selection For Configuration server', '');
  
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Https_StaticText_NicSelection := TNewStaticText.Create(CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Https);
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Https_StaticText_NicSelection.Top := ScaleY(4);
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Https_StaticText_NicSelection.Caption := 'Please choose a Nic for Configuration Server';
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Https_StaticText_NicSelection.AutoSize := True;
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Https_StaticText_NicSelection.Parent := CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Https.Surface;
  
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Https_CheckListBox := TNewCheckListBox.Create(CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Https);
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Https_CheckListBox.Width := CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Https.SurfaceWidth;
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Https_CheckListBox.Top := ScaleY(30)- CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Https_StaticText_NicSelection.Top
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Https_CheckListBox.Height := CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Https_CheckListBox.Top + ScaleY(120);
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Https_CheckListBox.Flat := True;
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Https_CheckListBox.Parent := CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Https.Surface;
  
 LoadStringsFromFile(ExpandConstant ('{sd}\Temp\nicdetails.conf'), a_strTextfile);
   
 for iLineCounter := 0 to GetArrayLength(a_strTextfile)-1 do
 begin
   if (Pos('[NIC', a_strTextfile[iLineCounter]) > 0) then
   begin
          Nic_StringList := TStringList.Create;
          StringChangeEx(a_strTextfile[iLineCounter],'[',',',FALSE);
          StringChangeEx(a_strTextfile[iLineCounter],']',',',FALSE);
          Nic_StringList.CommaText :=a_strTextfile[iLineCounter];
       
          Nic_Section_Name := Nic_StringList.Strings[1]; 
          Nic_Name := GetIniString( Nic_Section_Name, 'NICNAME','', ExpandConstant ('{sd}\Temp\nicdetails.conf'));
          Nic_IP_Count := GetIniString( Nic_Section_Name, 'NIC IP COUNT','', ExpandConstant ('{sd}\Temp\nicdetails.conf'));
          
          if StrToInt(Nic_IP_Count) <> 0 then
           begin
                  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Https_CheckListBox.AddRadioButton(Nic_Name, '', 0, False, True, nil);
                  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Https_CheckListBox.CheckItem(0,coCheck);
           end
           else if StrToInt(Nic_IP_Count) = 0 then
           begin
                  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Https_CheckListBox.AddRadioButton(Nic_Name, '', 0, False, False, nil);
                  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Https_CheckListBox.CheckItem(0,coCheck);
           end;
           
          LoopCount := 0;
      		while (LoopCount <= StrToInt(Nic_IP_Count)) do
      		begin
      			IPString := 'IP' + IntToStr(LoopCount);
            IP := GetIniString( Nic_Section_Name,IPString,'', ExpandConstant('{sd}\Temp\nicdetails.conf'));
            if IP <> '' then
            begin
                CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Https_CheckListBox.AddRadioButton(IP, '', 1, False, True, nil);  
            end;
            LoopCount := LoopCount + 1;
          end;
      end;
  end;

  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Https_StaticText_PortSelection := TNewStaticText.Create(CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Https);
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Https_StaticText_PortSelection.Top := CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Https_CheckListBox.Top + CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Https_CheckListBox.Height + ScaleY(15);
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Https_StaticText_PortSelection.Caption := 'Please enter a Https Port Number for Configuration Server';
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Https_StaticText_PortSelection.AutoSize := True;
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Https_StaticText_PortSelection.Parent := CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Https.Surface;
  
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Https_EditBoxFor_PortValue := TNewEdit.Create(CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Https);
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Https_EditBoxFor_PortValue.Top := CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Https_StaticText_PortSelection.Top + CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Https_StaticText_PortSelection.Height + ScaleY(5);
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Https_EditBoxFor_PortValue.Width := CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Https.SurfaceWidth - ScaleX(6);
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Https_EditBoxFor_PortValue.Text := '443';
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Https_EditBoxFor_PortValue.Parent := CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Https.Surface;

end;

Procedure CS_MultipleNics_WithPort_After_RoleOfCXServerPage_AfterServerChoicePage_Http_Https();
begin

  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https := CreateCustomPage(RoleOfCXServer_AfterServerChoicePage.ID, 'Nic Selection For Configuration server', '');
  
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https_StaticText_NicSelection := TNewStaticText.Create(CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https);
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https_StaticText_NicSelection.Top := ScaleY(4);
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https_StaticText_NicSelection.Caption := 'Please choose a Nic for Configuration Server';
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https_StaticText_NicSelection.AutoSize := True;
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https_StaticText_NicSelection.Parent := CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https.Surface;
  
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https_CheckListBox := TNewCheckListBox.Create(CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https);
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https_CheckListBox.Width := CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https.SurfaceWidth;
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https_CheckListBox.Top := ScaleY(30)- CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https_StaticText_NicSelection.Top
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https_CheckListBox.Height := CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https_CheckListBox.Top + ScaleY(80);
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https_CheckListBox.Flat := True;
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https_CheckListBox.Parent := CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https.Surface;
  
 LoadStringsFromFile(ExpandConstant ('{sd}\Temp\nicdetails.conf'), a_strTextfile);
   
 for iLineCounter := 0 to GetArrayLength(a_strTextfile)-1 do
 begin
   if (Pos('[NIC', a_strTextfile[iLineCounter]) > 0) then
   begin
          Nic_StringList := TStringList.Create;
          StringChangeEx(a_strTextfile[iLineCounter],'[',',',FALSE);
          StringChangeEx(a_strTextfile[iLineCounter],']',',',FALSE);
          Nic_StringList.CommaText :=a_strTextfile[iLineCounter];
       
          Nic_Section_Name := Nic_StringList.Strings[1]; 
          Nic_Name := GetIniString( Nic_Section_Name, 'NICNAME','', ExpandConstant ('{sd}\Temp\nicdetails.conf'));
          Nic_IP_Count := GetIniString( Nic_Section_Name, 'NIC IP COUNT','', ExpandConstant ('{sd}\Temp\nicdetails.conf'));
          
          if StrToInt(Nic_IP_Count) <> 0 then
           begin
                  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https_CheckListBox.AddRadioButton(Nic_Name, '', 0, False, True, nil);
                  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https_CheckListBox.CheckItem(0,coCheck);
           end
           else if StrToInt(Nic_IP_Count) = 0 then
           begin
                  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https_CheckListBox.AddRadioButton(Nic_Name, '', 0, False, False, nil);
                  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https_CheckListBox.CheckItem(0,coCheck);
           end;
           
          LoopCount := 0;
      		while (LoopCount <= StrToInt(Nic_IP_Count)) do
      		begin
      			IPString := 'IP' + IntToStr(LoopCount);
            IP := GetIniString( Nic_Section_Name,IPString,'', ExpandConstant('{sd}\Temp\nicdetails.conf'));
            if IP <> '' then
            begin
                CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https_CheckListBox.AddRadioButton(IP, '', 1, False, True, nil);  
            end;
            LoopCount := LoopCount + 1;
          end;
      end;
  end;

  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https_StaticText_PortSelection := TNewStaticText.Create(CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https);
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https_StaticText_PortSelection.Top := CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https_CheckListBox.Top + CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https_CheckListBox.Height + ScaleY(8);
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https_StaticText_PortSelection.Caption := 'Please enter a Http Port Number for Configuration Server';
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https_StaticText_PortSelection.AutoSize := True;
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https_StaticText_PortSelection.Parent := CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https.Surface;
  
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https_EditBoxFor_PortValue := TNewEdit.Create(CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https);
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https_EditBoxFor_PortValue.Top := CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https_StaticText_PortSelection.Top + CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https_StaticText_PortSelection.Height + ScaleY(3);
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https_EditBoxFor_PortValue.Width := CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https.SurfaceWidth - ScaleX(6);
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https_EditBoxFor_PortValue.Text := '80';
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https_EditBoxFor_PortValue.Parent := CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https.Surface;
  
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https_Static_PortSelection := TNewStaticText.Create(CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https);
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https_Static_PortSelection.Top := CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https_EditBoxFor_PortValue.Top + CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https_EditBoxFor_PortValue.Height + ScaleY(8);
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https_Static_PortSelection.Caption := 'Please enter a Https Port Number for Configuration Server';
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https_Static_PortSelection.AutoSize := True;
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https_Static_PortSelection.Parent := CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https.Surface;
  
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https_EditBox_PortValue := TNewEdit.Create(CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https);
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https_EditBox_PortValue.Top := CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https_Static_PortSelection.Top + CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https_Static_PortSelection.Height + ScaleY(3);
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https_EditBox_PortValue.Width := CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https.SurfaceWidth - ScaleX(6);
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https_EditBox_PortValue.Text := '443';
  CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https_EditBox_PortValue.Parent := CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http_Https.Surface;

end;

Procedure PS_MultipleNics_WithoutPort_After_CS_MultipleNics_WithPort_After_RoleOfCXServerPage_AfterServerChoicePage();
begin

        PSMultipleNicsWithoutPort_After_CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage := CreateCustomPage(CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_Http.ID, 'Nic Selection For Process Server', '');
        
        PSMultipleNicsWithoutPort_After_CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_StaticText_NicSelection := TNewStaticText.Create(PSMultipleNicsWithoutPort_After_CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage);
        PSMultipleNicsWithoutPort_After_CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_StaticText_NicSelection.Top := ScaleY(4);
        PSMultipleNicsWithoutPort_After_CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_StaticText_NicSelection.Caption := 'Please choose a Nic for Process Server';
        PSMultipleNicsWithoutPort_After_CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_StaticText_NicSelection.AutoSize := True;
        PSMultipleNicsWithoutPort_After_CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_StaticText_NicSelection.Parent := PSMultipleNicsWithoutPort_After_CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage.Surface;
        
        PSMultipleNicsWithoutPort_After_CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_CheckListBox := TNewCheckListBox.Create(PSMultipleNicsWithoutPort_After_CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage);
        PSMultipleNicsWithoutPort_After_CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_CheckListBox.Width := PSMultipleNicsWithoutPort_After_CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage.SurfaceWidth;
        PSMultipleNicsWithoutPort_After_CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_CheckListBox.Top := ScaleY(30)- PSMultipleNicsWithoutPort_After_CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_StaticText_NicSelection.Top
        PSMultipleNicsWithoutPort_After_CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_CheckListBox.Height := PSMultipleNicsWithoutPort_After_CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_CheckListBox.Top + ScaleY(120);
        PSMultipleNicsWithoutPort_After_CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_CheckListBox.Flat := True;
        PSMultipleNicsWithoutPort_After_CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_CheckListBox.Parent := PSMultipleNicsWithoutPort_After_CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage.Surface;
        
       LoadStringsFromFile(ExpandConstant ('{sd}\Temp\nicdetails.conf'), a_strTextfile);
         
       for iLineCounter := 0 to GetArrayLength(a_strTextfile)-1 do
       begin
         if (Pos('[NIC', a_strTextfile[iLineCounter]) > 0) then
         begin
                Nic_StringList := TStringList.Create;
                StringChangeEx(a_strTextfile[iLineCounter],'[',',',FALSE);
                StringChangeEx(a_strTextfile[iLineCounter],']',',',FALSE);
                Nic_StringList.CommaText :=a_strTextfile[iLineCounter];
             
                Nic_Section_Name := Nic_StringList.Strings[1]; 
                Nic_Name := GetIniString( Nic_Section_Name, 'NICNAME','', ExpandConstant ('{sd}\Temp\nicdetails.conf'));
                Nic_IP_Count := GetIniString( Nic_Section_Name, 'NIC IP COUNT','', ExpandConstant ('{sd}\Temp\nicdetails.conf'));
                
                if StrToInt(Nic_IP_Count) <> 0 then
                 begin
                        PSMultipleNicsWithoutPort_After_CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_CheckListBox.AddRadioButton(Nic_Name, '', 0, False, True, nil);
                        PSMultipleNicsWithoutPort_After_CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_CheckListBox.CheckItem(0,coCheck);
                 end
                 else if StrToInt(Nic_IP_Count) = 0 then
                 begin
                        PSMultipleNicsWithoutPort_After_CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_CheckListBox.AddRadioButton(Nic_Name, '', 0, False, False, nil);
                        PSMultipleNicsWithoutPort_After_CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_CheckListBox.CheckItem(0,coCheck);
                 end;
                 
                LoopCount := 0;
            		while (LoopCount <= StrToInt(Nic_IP_Count)) do
            		begin
            			IPString := 'IP' + IntToStr(LoopCount);
                  IP := GetIniString( Nic_Section_Name,IPString,'', ExpandConstant('{sd}\Temp\nicdetails.conf'));
                  if IP <> '' then
                  begin
                      PSMultipleNicsWithoutPort_After_CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage_CheckListBox.AddRadioButton(IP, '', 1, False, True, nil);  
                  end;
                  LoopCount := LoopCount + 1;
                end;
            end;
        end;
end;

Procedure PI_DetailsPage_After_PS_MultipleNics_WithoutPort_After_CS_MultipleNics_WithPort_After_RoleOfCXServerPage_AfterServerChoicePage();
var
      DomainUsrName,DomainName,UsrName,ForwardSlash : String;
begin
  		DomainName := ExpandConstant(GetEnv('USERDOMAIN'));
      UsrName:= ExpandConstant( +GetUserNameString);
      ForwardSlash := '\';
      DomainUsrName := DomainName + ForwardSlash + UsrName;
    
      PIDetailsPage_After_PSMultipleNicsWithoutPort_After_CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage := CreateInputQueryPage(CSMultipleNicsWithPort_AfterServerChoicePage_Http.ID,'Push Server Details','','Please enter following data and click Next.');
  		PIDetailsPage_After_PSMultipleNicsWithoutPort_After_CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage.Add('Domain_name\User_name ' + #13 + '[Ex:- mydomain\cxadmin - "cxadmin" should have local system admin privileges ]',False);
			PIDetailsPage_After_PSMultipleNicsWithoutPort_After_CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage.Add('Password',True);
			PIDetailsPage_After_PSMultipleNicsWithoutPort_After_CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage.Values[0] :=  DomainUsrName; 
end;

Procedure PI_DetailsPage_After_CS_MultipleNics_WithPort_After_PS_MultipleNics_WithoutPort_AfterServerChoicePage();
var
      DomainUsrName,DomainName,UsrName,ForwardSlash : String;
begin
  		DomainName := ExpandConstant(GetEnv('USERDOMAIN'));
      UsrName:= ExpandConstant( +GetUserNameString);
      ForwardSlash := '\';
      DomainUsrName := DomainName + ForwardSlash + UsrName;
    
      PIDetailsPage_After_CSMultipleNicsWithPort_After_PSMultipleNicsWithoutPort_AfterServerChoicePage := CreateInputQueryPage(CSMultipleNicsWithPort_AfterServerChoicePage_Http.ID,'Push Server Details','','Please enter following data and click Next.');
  		PIDetailsPage_After_CSMultipleNicsWithPort_After_PSMultipleNicsWithoutPort_AfterServerChoicePage.Add('Domain_name\User_name ' + #13 + '[Ex:- mydomain\cxadmin - "cxadmin" should have local system admin privileges ]',False);
			PIDetailsPage_After_CSMultipleNicsWithPort_After_PSMultipleNicsWithoutPort_AfterServerChoicePage.Add('Password',True);
			PIDetailsPage_After_CSMultipleNicsWithPort_After_PSMultipleNicsWithoutPort_AfterServerChoicePage.Values[0] :=  DomainUsrName; 
end;


Procedure PrimaryCX_DetailsPage();
begin
			PrimaryCXDetailsPage := CreateInputQueryPage(PSMultipleNicsWithoutPort_After_CSMultipleNicsWithPort_AfterRoleOfCXServerPage_AfterServerChoicePage.ID,
													'Primary CX Details',
													'',
													'Please enter following data and click Next.');
			PrimaryCXDetailsPage.Add('Primary CX IP Address',False);
			PrimaryCXDetailsPage.Add('Primary CX Port Number',False);
			PrimaryCXDetailsPage.Values[1] := '80';
end;


Procedure PI_DetailsPage_After_PrimaryCX_DetailsPage();
var
      DomainUsrName,DomainName,UsrName,ForwardSlash : String;
begin
  		DomainName := ExpandConstant(GetEnv('USERDOMAIN'));
      UsrName:= ExpandConstant( +GetUserNameString);
      ForwardSlash := '\';
      DomainUsrName := DomainName + ForwardSlash + UsrName;
    
      PIDetailsPage_After_PrimaryCXDetailsPage := CreateInputQueryPage(PrimaryCXDetailsPage.ID,'Push Server Details','','Please enter following data and click Next.');
  		PIDetailsPage_After_PrimaryCXDetailsPage.Add('Domain_name\User_name ' + #13 + '[Ex:- mydomain\cxadmin - "cxadmin" should have local system admin privileges ]',False);
			PIDetailsPage_After_PrimaryCXDetailsPage.Add('Password',True);
			PIDetailsPage_After_PrimaryCXDetailsPage.Values[0] :=  DomainUsrName; 
end;


Procedure PS_MultipleNics_WithoutPort_AfterAddServerChoicePage();
begin

        PSMultipleNicsWithoutPort_AfterAddServerChoicePage := CreateCustomPage(AddServerChoicePage.ID, 'Nic Selection For Process Server', '');
        
        PSMultipleNicsWithoutPort_AfterAddServerChoicePage_StaticText_NicSelection := TNewStaticText.Create(PSMultipleNicsWithoutPort_AfterAddServerChoicePage);
        PSMultipleNicsWithoutPort_AfterAddServerChoicePage_StaticText_NicSelection.Top := ScaleY(4);
        PSMultipleNicsWithoutPort_AfterAddServerChoicePage_StaticText_NicSelection.Caption := 'Please choose a Nic for Process Server';
        PSMultipleNicsWithoutPort_AfterAddServerChoicePage_StaticText_NicSelection.AutoSize := True;
        PSMultipleNicsWithoutPort_AfterAddServerChoicePage_StaticText_NicSelection.Parent := PSMultipleNicsWithoutPort_AfterAddServerChoicePage.Surface;
        
        PSMultipleNicsWithoutPort_AfterAddServerChoicePage_CheckListBox := TNewCheckListBox.Create(PSMultipleNicsWithoutPort_AfterAddServerChoicePage);
        PSMultipleNicsWithoutPort_AfterAddServerChoicePage_CheckListBox.Width := PSMultipleNicsWithoutPort_AfterAddServerChoicePage.SurfaceWidth;
        PSMultipleNicsWithoutPort_AfterAddServerChoicePage_CheckListBox.Top := ScaleY(30)- PSMultipleNicsWithoutPort_AfterAddServerChoicePage_StaticText_NicSelection.Top
        PSMultipleNicsWithoutPort_AfterAddServerChoicePage_CheckListBox.Height := PSMultipleNicsWithoutPort_AfterAddServerChoicePage_CheckListBox.Top + ScaleY(120);
        PSMultipleNicsWithoutPort_AfterAddServerChoicePage_CheckListBox.Flat := True;
        PSMultipleNicsWithoutPort_AfterAddServerChoicePage_CheckListBox.Parent := PSMultipleNicsWithoutPort_AfterAddServerChoicePage.Surface;
        
       LoadStringsFromFile(ExpandConstant ('{sd}\Temp\nicdetails.conf'), a_strTextfile);
         
       for iLineCounter := 0 to GetArrayLength(a_strTextfile)-1 do
       begin
         if (Pos('[NIC', a_strTextfile[iLineCounter]) > 0) then
         begin
                Nic_StringList := TStringList.Create;
                StringChangeEx(a_strTextfile[iLineCounter],'[',',',FALSE);
                StringChangeEx(a_strTextfile[iLineCounter],']',',',FALSE);
                Nic_StringList.CommaText :=a_strTextfile[iLineCounter];
             
                Nic_Section_Name := Nic_StringList.Strings[1]; 
                Nic_Name := GetIniString( Nic_Section_Name, 'NICNAME','', ExpandConstant ('{sd}\Temp\nicdetails.conf'));
                Nic_IP_Count := GetIniString( Nic_Section_Name, 'NIC IP COUNT','', ExpandConstant ('{sd}\Temp\nicdetails.conf'));
                
                if StrToInt(Nic_IP_Count) <> 0 then
                 begin
                        PSMultipleNicsWithoutPort_AfterAddServerChoicePage_CheckListBox.AddRadioButton(Nic_Name, '', 0, False, True, nil);
                        PSMultipleNicsWithoutPort_AfterAddServerChoicePage_CheckListBox.CheckItem(0,coCheck);
                 end
                 else if StrToInt(Nic_IP_Count) = 0 then
                 begin
                        PSMultipleNicsWithoutPort_AfterAddServerChoicePage_CheckListBox.AddRadioButton(Nic_Name, '', 0, False, False, nil);
                        PSMultipleNicsWithoutPort_AfterAddServerChoicePage_CheckListBox.CheckItem(0,coCheck);
                 end;
                 
                LoopCount := 0;
            		while (LoopCount <= StrToInt(Nic_IP_Count)) do
            		begin
            			IPString := 'IP' + IntToStr(LoopCount);
                  IP := GetIniString( Nic_Section_Name,IPString,'', ExpandConstant('{sd}\Temp\nicdetails.conf'));
                  if IP <> '' then
                  begin
                      PSMultipleNicsWithoutPort_AfterAddServerChoicePage_CheckListBox.AddRadioButton(IP, '', 1, False, True, nil);  
                  end;
                  LoopCount := LoopCount + 1;
                end;
            end;
        end;
end;

Procedure CS_DetailsPage_After_PS_MultipleNics_WithoutPort_AfterAddServerChoicePage();
begin
			CSDetailsPage_After_PSMultipleNicsWithoutPort_AfterAddServerChoicePage := CreateInputQueryPage(PSMultipleNicsWithoutPort_AfterAddServerChoicePage.ID,
													'Configuration Server Details',
													'',
													'Please enter following data and click Next.');
			CSDetailsPage_After_PSMultipleNicsWithoutPort_AfterAddServerChoicePage.Add('Configuration Server IP Address',False);
			CSDetailsPage_After_PSMultipleNicsWithoutPort_AfterAddServerChoicePage.Add('Configuration Server Port Number',False);
			CSDetailsPage_After_PSMultipleNicsWithoutPort_AfterAddServerChoicePage.Values[1] := '80';
end;

Procedure CommunicationChoicePagePS();
begin
        CommunicationChoicePage_PS := CreateInputOptionPage(ServerChoicePage.ID,
    																'Communication Mode',
    																'',
    																'Choose one of the following modes for the Communication and click Next.',
    																True,False);
    		CommunicationChoicePage_PS.Add('Http');
    		CommunicationChoicePage_PS.Add('Https');     
    		CommunicationChoicePage_PS.SelectedValueIndex := 1;
end;

Procedure CommunicationChoicePagePSCS();
begin
        CommunicationChoicePage_PS_CS := CreateInputOptionPage(ServerChoicePage.ID,
    																'Communication Mode',
    																'',
    																'Choose one of the following modes for the Communication and click Next.',
    																True,False);
    		CommunicationChoicePage_PS_CS.Add('Http');
    		CommunicationChoicePage_PS_CS.Add('Https'); 
    		CommunicationChoicePage_PS_CS.Add('Both Http and Https');     
    		CommunicationChoicePage_PS_CS.SelectedValueIndex := 2;
end;
