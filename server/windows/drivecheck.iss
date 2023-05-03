[Code]

var
	// combo box for drives
	cbDrive : TComboBox ;
	// array fo AnsiString that keep the drive letters
	DrvLetters: array of AnsiString;

function GetDriveType( lpDisk: AnsiString ): Integer;
external 'GetDriveTypeA@kernel32.dll stdcall';

function GetLogicalDriveStrings( nLenDrives: LongInt; lpDrives: AnsiString ): Integer;
external 'GetLogicalDriveStringsA@kernel32.dll stdcall';

const
  DRIVE_UNKNOWN = 0; // The drive type cannot be determined.
  DRIVE_NO_ROOT_DIR = 1; // The root path is invalid. For example, no volume is mounted at the path.
  DRIVE_REMOVABLE = 2; // The disk can be removed from the drive.
  DRIVE_FIXED = 3; // The disk cannot be removed from the drive.
  DRIVE_REMOTE = 4; // The drive is a remote (network) drive.
  DRIVE_CDROM = 5; // The drive is a CD-ROM drive.
  DRIVE_RAMDISK = 6; // The drive is a RAM disk.


// function to convert disk type to AnsiString
function DriveTypeString( dtype: Integer ): AnsiString ;
begin
  case dtype of
    DRIVE_NO_ROOT_DIR : Result := 'Root path invalid';
    DRIVE_REMOVABLE : Result := 'Removable';
    DRIVE_FIXED : Result := 'Fixed';
    DRIVE_REMOTE : Result := 'Network';
    DRIVE_CDROM : Result := 'CD-ROM';
    DRIVE_RAMDISK : Result := 'Ram disk';
  else
    Result := 'Unknown';
  end;
end;

// change folder accordigly to the drive letter selected
procedure cbDriveOnClick(Sender: TObject);
var
	PosNum : Integer;
begin
	PosNum := Pos('C:',DrvLetters[ cbDrive.ItemIndex ]);
	//if PosNum<>0 then
	//	WizardForm.DirEdit.Text := DrvLetters[ cbDrive.ItemIndex ] + (ExpandConstant('{#_AppDirC_}'))
	//else
		WizardForm.DirEdit.Text := DrvLetters[ cbDrive.ItemIndex ] + (ExpandConstant('{#_AppDir_}'));
end;

procedure FillCombo();
var
  n: Integer;
  drivesletters: AnsiString; lenletters: Integer;
  drive: AnsiString;
  disktype, posnull: Integer;
  sd: AnsiString;

begin
  //get the system drive
  sd := UpperCase(ExpandConstant('{sd}'));

  //get all drives letters of system
  drivesletters := StringOfChar( ' ', 64 );
  lenletters := GetLogicalDriveStrings( 63, drivesletters );
  SetLength( drivesletters , lenletters );

  drive := '';
  n := 0;
  while ( (Length(drivesletters) > 0) ) do
  begin
    posnull := Pos( #0, drivesletters );
  	if posnull > 0 then
  	begin
      drive:= UpperCase( Copy( drivesletters, 1, posnull - 1 ) );

      // get number type of disk
      disktype := GetDriveType( drive );

      // add it only if it is not a floppy
      if ( disktype = DRIVE_FIXED ) then
      begin

		    cbDrive.Items.Add( drive )

        SetArrayLength(DrvLetters, N+1);
        DrvLetters[n] := drive;

        // default select C: Drive
        //if ( Copy(drive,1,2) = 'C:' ) then cbDrive.ItemIndex := n;
        // or default to system drive
        if ( Copy(drive,1,2) = sd ) then cbDrive.ItemIndex := n;

        n := n + 1;
      end;

  	  drivesletters := Copy( drivesletters, posnull+1, Length(drivesletters));
  	end
  end;

  cbDriveOnClick( cbDrive );

end;


procedure DriveSelectionPage();
begin
	// create the combo box for drives
	cbDrive:= TComboBox.Create(WizardForm.SelectDirPage);
    with cbDrive do
    begin
      Parent := WizardForm.DirEdit.Parent;

      Left := WizardForm.DirEdit.Left;
      Top := WizardForm.DirEdit.Top + WizardForm.DirEdit.Height * 2;
      Width := WizardForm.DirEdit.Width;

      Style := csDropDownList;
    end;

	// hide the Browse button
	WizardForm.DirBrowseButton.Visible := false;

	// Edit box for folder don't have to be editable
	WizardForm.DirEdit.Enabled := false;

	// fill combo box with Drives
	FillCombo;

	// set the event on combo change
	cbDrive.OnClick := @cbDriveOnClick ;
end;

procedure MyAfterInstall2(FileName: AnsiString);
begin
  MsgBox('Just installed ' + FileName + ' as ' + CurrentFileName + '.', mbInformation, MB_OK);
end;
