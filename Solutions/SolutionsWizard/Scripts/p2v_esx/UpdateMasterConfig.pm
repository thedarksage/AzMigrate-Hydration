=head
	-----UpdateMasterConfig.pm-----
	1. It is used to update master config file with previous information in recovery database file.
		Support protections done with earlier versions of vContinuum which relied on recovery_database_ and ESX_Master files.
	2. It also updates master config file by writing failover = yes  and failback = yes.
		so this will remove dependency on vContinuum.exe.
	3. We can write functions to backup the file as well.
=cut

package UpdateMasterConfig;

use strict;
use warnings;

use Common_Utils;
use HostOpts;
use DatastoreOpts;

#####MergeRecoveryDatabaseIntoMasterConfig#####
##Description		:: It merges all the data of recovery database file to MasterConfig file.
##						1. If Master Config file does not exist in CX, we need to create it.
##						2. If already exists, update it with Recovery database entries, remove file from target Datastores.
##Input 			:: Target Host details( Target Host name, part of datacenter? ).
##Output			:: Returns SUCCESS on merging successfully else FAILURE.
#####MergeRecoveryDatabaseIntoMasterConfig##### 
sub MergeRecoveryDatabaseIntoMasterConfig
{
	my $targetHostName	= shift;
	Common_Utils::WriteMessage("Updating Configuration files in host $targetHostName.",2);
	my( $returnCode , $hostView ) 			= HostOpts::GetSubHostView( $targetHostName );
	if ( $returnCode ne $Common_Utils::SUCCESS )
	{
		return ( $Common_Utils::FAILURE );
	}
	
	( $returnCode , my $datacenterView )	= DatacenterOpts::GetDataCenterViewOfHost( $targetHostName );
	if ( $returnCode ne $Common_Utils::SUCCESS )
	{
		return ( $Common_Utils::FAILURE );
	}
	
	my $datacenterName						= $datacenterView->name;
	Common_Utils::WriteMessage("Datacenter Name where $targetHostName resides = $datacenterName.",2);
	
	my @hostViews							= $hostView;
	my @datacenterViews						= $datacenterView;
	( $returnCode , my $listDatastoreInfo )	= DatastoreOpts::GetDatastoreInfo( \@hostViews , \@datacenterViews );
	if ( $returnCode ne $Common_Utils::SUCCESS ) 
	{
		return ( $Common_Utils::FAILURE );
	}
	#should we validate whether we can access datastores or not?

	( $returnCode ,my $recoveryDatabaseInfo )= DatastoreOpts::GetLatestFileInfo( $hostView , "recovery_database_*" , "fileinfo" , $listDatastoreInfo );
	if ( $returnCode ne $Common_Utils::SUCCESS )
	{
		return ( $Common_Utils::FAILURE );
	}
		
	( $returnCode , my $EsxMasterFilesInfo ) = DatastoreOpts::GetLatestFileInfo( $hostView , "ESX_Master_*" , "fileinfo" , $listDatastoreInfo );
	if ( $returnCode ne $Common_Utils::SUCCESS )
	{
		return ( $Common_Utils::FAILURE );
	}
	
	if ( ( 0 != scalar( keys %$recoveryDatabaseInfo ) ) && ( 0 != scalar( keys %$EsxMasterFilesInfo ) ) )
	{
		Common_Utils::WriteMessage("Found atleast one recovery database and ESX Master files.",2);
		if ( $Common_Utils::SUCCESS ne DatastoreOpts::DownloadFiles( $recoveryDatabaseInfo , $datacenterName ) )
		{
			return ( $Common_Utils::FAILURE );
		}
		
		if ( $Common_Utils::SUCCESS ne DatastoreOpts::DownloadFiles( $EsxMasterFilesInfo , $datacenterName ) )
		{
			return ( $Common_Utils::FAILURE );
		}
		
		foreach my $recoveryDatabaseFileName ( sort keys %$recoveryDatabaseInfo )
		{
			my @fileMapsToIP		= split( /recovery_database_/ , $recoveryDatabaseFileName );
			my $EsxMasterFileName	= "ESX_Master_".$fileMapsToIP[-1].".xml";
			if ( ! exists $$EsxMasterFilesInfo{$EsxMasterFileName} )
			{
				$$recoveryDatabaseInfo{ $recoveryDatabaseFileName }	= "delete";
				next;
			}
			
			( $returnCode , my $updateFile )	= MergeFiles( $recoveryDatabaseFileName , $EsxMasterFileName );
			if ( $Common_Utils::SUCCESS ne $Common_Utils::SUCCESS )
			{
				next;
			}	
			
			if ( $updateFile )
			{
				$$recoveryDatabaseInfo{ $recoveryDatabaseFileName }	= "update";
				next;
			}
			$$recoveryDatabaseInfo{ $recoveryDatabaseFileName }	= "delete";
		}
	}
	
	if ( $Common_Utils::SUCCESS ne UpdateFiles( $hostView , $recoveryDatabaseInfo , $EsxMasterFilesInfo , $listDatastoreInfo , $datacenterName ) )
	{
		#as of now do not report error even if there a failure in download of the file.
	}
	return ( $Common_Utils::SUCCESS );
}

#####UpdateFiles#####
##Description 		:: It receives the recovery database Info and ESX_Master File info which contain what action to be performed
##						to each.
##Input 			:: Host View, Map of Recovery database file name and action and corresponding ESX Master Map.
##Output 			:: Returns SUCCESS else FAILURE.
#####UpdateFile#####
sub UpdateFiles
{
	my $hostView 			 = shift;
	my $recoveryDatabaseInfo = shift;
	my $EsxMasterFilesInfo	 = shift;
	my $listDatastoreInfo	 = shift;
	my $datacenterName 		 = shift;
	
	foreach my $recoveryDatabaseFileName ( sort keys %$recoveryDatabaseInfo )
	{
		my @fileMapsToIP		= split( /recovery_database_/ , $recoveryDatabaseFileName );
		my $EsxMasterFileName	= "ESX_Master_".$fileMapsToIP[-1].".xml";
		
		if ( "update" eq $$recoveryDatabaseInfo{ $recoveryDatabaseFileName } )
		{
			if ( $Common_Utils::SUCCESS ne UploadFile( $hostView , $EsxMasterFileName , $listDatastoreInfo , $datacenterName ) )
			{
			}
			next;
		}
		
		if ( $Common_Utils::SUCCESS ne DeleteFileFromDatastores( $hostView , $recoveryDatabaseFileName , $listDatastoreInfo , $datacenterName ) )
		{	
		}
		
		if ( ! exists $$EsxMasterFilesInfo{ $EsxMasterFileName } )
		{
			next;
		}
		
		if ( $Common_Utils::SUCCESS ne DeleteFileFromDatastores( $hostView , $EsxMasterFileName , $listDatastoreInfo , $datacenterName ) )
		{	
		}
	}
	
	return ( $Common_Utils::SUCCESS );
} 

#####UploadFile#####
##Description		:: Uploads file on to all datastores.
##Input 			:: File Name to be uploaded , datastore List, and Datacenter Name.
##Output 			:: Returns SUCCESS else FAILURE.
#####UploadFile#####
sub UploadFile
{
	my $hostView			= shift;
	my $fileName 			= shift;
	my $listDatastoreInfo	= shift;
	my $datacenterName		= shift;
	my $initialFilePath		= "";
	
	my @tempArr	= @$listDatastoreInfo;
	for ( my $i = 0 ; $i <= $#tempArr; $i++ )
	{
		my $datastoreName	= $$listDatastoreInfo[$i]->{symbolicName};
		if ( ( "no" eq lc( $$listDatastoreInfo[$i]->{isAccessible} ) ) || ( "yes" eq lc ( $$listDatastoreInfo[$i]->{readOnly} ) ) )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to update files on datastore $datastoreName as it is either un-accessible or in read only state.",2);
			next;
		}
		
		if ( ( !defined ( $initialFilePath) ) || ( "" eq $initialFilePath ) )
		{
			my( $returnCode , $folderFileInfo , $fileInfo )	= DatastoreOpts::FindFileInfo( $hostView ,"*" , "[$datastoreName]" , "folderfileinfo" );
			if ( $returnCode ne $Common_Utils::SUCCESS )
			{
				return ( $Common_Utils::FAILURE );
			}
			
			my @tempArr2	= @$folderFileInfo;
			if ( -1 == $#tempArr2 )
			{
				next;
			}
			
			my $source 		= $Common_Utils::vContinuumMetaFilesPath."/$fileName";
			my $target		= "[$datastoreName] $$folderFileInfo[0]/$fileName";
			Common_Utils::WriteMessage("Uploading file $source to $target.",2);
			if ( $returnCode ne DatastoreOpts::do_put( $source, $target, $datacenterName ) )
			{
				next;
			}
			$initialFilePath= $target;
		}
		
		my $target			= "[$datastoreName] $fileName";
		if ( $Common_Utils::SUCCESS ne DatastoreOpts::MoveOrCopyFile( "copy" , $datacenterName , $initialFilePath , $target , 1 ) )
		{
			next;
		}
	}
	
	if ( ( defined ( $initialFilePath) ) || ( "" ne $initialFilePath ) )
	{
		my $returnCode		= DatastoreOpts::RemovePath( $datacenterName , $initialFilePath , 1 );
		if( $Common_Utils::SUCCESS ne $returnCode )
		{
			return ( $Common_Utils::FAILURE );
		}
	}
	return ( $Common_Utils::SUCCESS );
} 

#####DeleteFileFromDatastores#####
##Description 		:: Deletes file from datastores. First it checks for presence of file then if exists it deletes.
##Input 			:: File Name to check and delete , to be deleted from datastores and datacenterName.
##Output			:: Return SUCCESS else FAILURE.
#####DeleteFileFromDatastores#####
sub DeleteFileFromDatastores
{
	my $hostView 			= shift;
	my $fileName			= shift;
	my $listDatastoreInfo	= shift;
	my $datacenterName		= shift;
	
	my @tempArr	= @$listDatastoreInfo;
	for ( my $i = 0 ; $i <= $#tempArr ; $i++ )
	{
		my $searchInPath	= "[$$listDatastoreInfo[$i]->{symbolicName}]";
		my $searchType		= "file";
		my( $returnCode , $folderFileInfo , $fileInfo )	= DatastoreOpts::FindFileInfo( $hostView , $fileName , $searchInPath , $searchType );
		if ( $returnCode ne $Common_Utils::SUCCESS )
		{
			return ( $Common_Utils::FAILURE );
		}
		
		foreach my $file ( @$fileInfo )
		{
			if ( $file->path ne $fileName )
			{
				next;
			}
			my $filePath 	= "$searchInPath $fileName";
			$returnCode		= DatastoreOpts::RemovePath( $datacenterName , $filePath , 1 );
			if( $Common_Utils::SUCCESS ne $returnCode )
			{
				return ( $Common_Utils::FAILURE );
			}
		}
	}
}

#####MergeFiles#####
##Description		:: It merges recovery database file and Master Config File.
##						1. checks for Master Config file in CX , if exists will download , else we will Transform ESX_Master_*
##							to MasterConfig.
##						2. Merges all entries in recovery database to MasterConfig and removes same entries in ESX_Master_* also.
##						3. If all entries are updated We need to set a parameter to delete it from Target?
##Input 			:: Recovery Database Files Info and ESX_Master files.
##Ouput 			:: Return SUCCESS, will list the failure cases.
#####MergeFiles#####
sub MergeFiles
{
	my $recoveryDatabaseFileName= shift;
	my $EsxMasterFileName		= shift;
	my @HostNodesInCX			= ();
	
	Common_Utils::WriteMessage("Trying to merge file $recoveryDatabaseFileName.",2);
	if ( ( ! -e "$Common_Utils::vContinuumMetaFilesPath/$recoveryDatabaseFileName" ) || ( ! -f "$Common_Utils::vContinuumMetaFilesPath/$recoveryDatabaseFileName" ) )
	{
		Common_Utils::WriteMessage("Mesg :: $recoveryDatabaseFileName is not merged with MasterConfig file, because it is not found in path $Common_Utils::vContinuumMetaFilesPath.",2);
		return ( $Common_Utils::FAILURE );
	}
		
	if ( ( ! -e "$Common_Utils::vContinuumMetaFilesPath/$EsxMasterFileName" ) || ( ! -f "$Common_Utils::vContinuumMetaFilesPath/$EsxMasterFileName" ) )
	{
		Common_Utils::WriteMessage("Mesg :: $recoveryDatabaseFileName is not merged with MasterConfig file, because corresponding Esx_Master config file is not found in path $Common_Utils::vContinuumMetaFilesPath.",2);
		return ( $Common_Utils::FAILURE );
	}
		
	if ( ( 0 == -s "$Common_Utils::vContinuumMetaFilesPath/$recoveryDatabaseFileName" ) || ( 0 == -s "$Common_Utils::vContinuumMetaFilesPath/$EsxMasterFileName" ) )
	{
		Common_Utils::WriteMessage("Mesg :: $recoveryDatabaseFileName is not merged with MasterConfig file, because either recovery database or Esx_Master file sizes are 0.",2);
		return ( $Common_Utils::FAILURE );
	}
		
	my ( $returnCode , $recoveryDatabaseData )	= ReadRecoveryDatabaseFile( "$Common_Utils::vContinuumMetaFilesPath/$recoveryDatabaseFileName" );
	if ( $returnCode ne $Common_Utils::SUCCESS )
	{
		Common_Utils::WriteMessage("Mesg :: $recoveryDatabaseFileName is not merged with MasterConfig file. Failed to read $recoveryDatabaseFileName.",2);
		return ( $Common_Utils::FAILURE );
	}
		
	if ( -1 == $#HostNodesInCX )
	{
		my $cxIpAndPortNum	= "";
		
		( $returnCode , $cxIpAndPortNum )	= Common_Utils::FindCXIP();
		if ( $returnCode ne $Common_Utils::SUCCESS )
		{
			return ( $Common_Utils::FAILURE );
		}
		
		( $returnCode ) 	= Common_Utils::GetHostNodesInCX( $cxIpAndPortNum , \@HostNodesInCX );
		if ( $returnCode ne $Common_Utils::SUCCESS )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to get list of Host Nodes registered with CX.",2);
			Common_Utils::WriteMessage("Mesg :: $recoveryDatabaseFileName is not merged with MasterConfig file.",2);
			return ( $Common_Utils::FAILURE );
		}
	}
		
	if ( -1 == $#HostNodesInCX )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to find hosts in CX.",2);
		return ( $Common_Utils::FAILURE );
	}
		
	foreach my $db ( @$recoveryDatabaseData )
	{
		foreach my $host ( @HostNodesInCX )
		{
			if ( lc( $$db{hostName} ) eq lc ( $$host{hostName} ) )
			{
				$$db{existsInCX}	= "yes";
				my $hostName		= $$db{hostName};
				Common_Utils::WriteMessage("$hostName exists in CX.",2);
			}
		}
	}
	
	( $returnCode )		= XmlFunctions::PrepareTempEsxMasterFile( $recoveryDatabaseData, "$Common_Utils::vContinuumMetaFilesPath/$EsxMasterFileName" );
	if ( $returnCode ne $Common_Utils::SUCCESS )
	{
		return ( $Common_Utils::FAILURE );
	}
		
	( $returnCode )		= XmlFunctions::UpdateEsxMasterFile( $recoveryDatabaseData, "$Common_Utils::vContinuumMetaFilesPath/$EsxMasterFileName" );
	if ( $returnCode ne $Common_Utils::SUCCESS )
	{
		return ( $Common_Utils::FAILURE );
	}
		
	if ( -f "$Common_Utils::vContinuumMetaFilesPath/Esx_Master_Temp.xml" )
	{
		if ( $Common_Utils::SUCCESS ne XmlFunctions::UpdateTempMasterXmlCX( fileName => "$Common_Utils::vContinuumMetaFilesPath/Esx_Master_Temp.xml" ) )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to merge $recoveryDatabaseFileName with CX MasterConfig file.",2);
			return ( $Common_Utils::FAILURE );
		}
	}
	
	my $updateFile		= 0;
	foreach my $recDb ( @$recoveryDatabaseData )
	{
		if ( ( defined $$recDb{update} ) && ( "yes" eq lc $$recDb{update} ) )
		{
			$updateFile	= 1;
			last;
		}
	}	
	return ( $Common_Utils::SUCCESS , $updateFile );
}

#####ReadRecoveryDatabaseFile#####
##Description 	:: Reads Recovery database file.
##Input 		:: Path to the File. 
##Output 		:: Returns SUCCESS when file is read else FAILURE.
#####ReadRecoveryDatabaseFile#####
sub ReadRecoveryDatabaseFile
{
	my $file 		= shift;
	my @recDbdata	= ();
	
	if ( ! open( RD  , $file ) )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to access file $file. Check whether permission are set for user to read this file.",2);
		return ( $Common_Utils::FAILURE );
	}
	
	while (<RD>)
	{
		my %recdata				= ();
		my @data				= split ( /@!@!/ , $_ );

		if ( "NULL" eq $data[0] || "NULL" eq $data[2] )
		{
			next;
		}
		$recdata{hostName}		= $data[0];
		$recdata{displayName}	= $data[1];
		$recdata{mtHostName}	= $data[2];
		$recdata{diskIds}		= "";
		for ( my $i = 3 ; $i <= $#data  ; $i++ )
		{
			if ( "" eq $recdata{diskIds} )
			{
				$recdata{diskIds}= $data[$i];
				next;
			}
			$data[$i] 			=~ s/\s+$//;
			$recdata{diskIds}	= $recdata{diskIds}."@!@!".$data[$i];
		}
		push @recDbdata , \%recdata;
	}
	return ( $Common_Utils::SUCCESS , \@recDbdata );
}

1;

#new procedure for upgrade..
#Get the recovery database file and ESX Master Config file.
#read recovery database file and see which of these hosts are there in ESX Master file.
#update Master Config file with entries which are found in both the files.
#what are the situations where entries will not be there in MasterConfig File but entries will be there in ESX_Master_IP.xml file.
#Suppose there are couple of CX boxes.
#we need to write up a document on how vContinuum is working and how to debug it.
#need to correct the way we are handling.
#need to strengthen readiness checks...cross checking if there is any thing which can cause failure..







