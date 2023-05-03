=head
	Package Name : Datastore_Opts.pm
	1. Check for the file at datastores on ESX.
	2. Upload/Download files from datastores.
	3. List all Datastores at ESX.
	4. Creation of folder at ESX level. 
	5. Browse all the datastore.
=cut

package DatastoreOpts;

use strict;
use warnings;
use Common_Utils;
use HostOpts;
use Data::Dumper;
 

#####GetDatastoreInfo#####
##description 		:: Collects the datastore Information. 
##Input 			:: Host View and DataCenter.
##Output 			:: Returns DataStoreInfo in map ON SUCCESS else FAILURE.
#####GetDatastoreInfo#####
sub GetDatastoreInfo
{
	my $hostViews 		= shift;
	my $datacenterViews	= shift;
	my @hostViews 		= @$hostViews;
	my @datastoreInfo	= ();
	
	my @ds_array  		= ();
	foreach ( @$datacenterViews )
	{
		if(  defined $_->datastore )
		{
			@ds_array = ( @ds_array , @{$_->datastore} );
		}
	}
	my $datastoresMOB = Vim::get_views( mo_ref_array => \@ds_array );

	foreach  my $hostView ( @hostViews ) 
	{
		my $subHostView				= $hostView;
		my $hostName				= $hostView->name;
		
		$subHostView				= Common_Utils::UpdateViewProperties( view => $subHostView , properties => [ "config.fileSystemVolume", "config.storageDevice.scsiLun", "config.storageDevice.scsiTopology" ] );
		
		my $hostKeyValue 			= $subHostView->{mo_ref}->{value};
		my $scsiLuns				= Common_Utils::GetViewProperty( $subHostView, 'config.storageDevice.scsiLun');
		my $scsiTopologyAdapters	= Common_Utils::GetViewProperty( $subHostView, 'config.storageDevice.scsiTopology')->adapter;
		my %mapLunIdNumber			= ();
		foreach my $scsiTopologyAdapter ( @$scsiTopologyAdapters )
		{
			if( exists $scsiTopologyAdapter->{target} )
			{
				foreach my $scsiTopAdpTgt ( @{$scsiTopologyAdapter->{target}})
				{
					foreach my $scsiTopAdpTgtLun ( @{ $scsiTopAdpTgt->{lun} } )
					{
						$mapLunIdNumber{ $scsiTopAdpTgtLun->scsiLun }	= $scsiTopAdpTgtLun->lun;
					}
				}
			}
		}
		
		my $hostfilesystemvolume  	= Common_Utils::GetViewProperty( $subHostView, 'config.fileSystemVolume');
		foreach my $mountinformation ( @{$hostfilesystemvolume->{mountInfo}} )
		{
			if( ( ! defined ( $mountinformation->{volume}->{name} ) ) || ( $mountinformation->{volume}->{name} eq "" ) )
			{
				next;
			}
			my %mapDatastoreInfo 				= ();
			$mapDatastoreInfo{vSphereHostName} 	= $subHostView->name;
			$mapDatastoreInfo{name} 			= $mountinformation->{mountInfo}->{path};
			$mapDatastoreInfo{symbolicName} 	= $mountinformation->{volume}->{name};
			$mapDatastoreInfo{blockSize}		= $mountinformation->{volume}->{blockSizeMb};
			$mapDatastoreInfo{fileSystem}		= $mountinformation->{volume}->{version};
			$mapDatastoreInfo{type}				= $mountinformation->{volume}->{type};			
			$mapDatastoreInfo{uuid}				= $mountinformation->{volume}->{uuid};
			Common_Utils::WriteMessage("vSphereHostName = $mapDatastoreInfo{vSphereHostName}, symbolicName = $mapDatastoreInfo{symbolicName}.",2);
			
			my $extents							= $mountinformation->{volume}->{extent};
			foreach my $extent ( @$extents )
			{
				$mapDatastoreInfo{diskName}	.= $extent->diskName . ":" . $extent->partition . ",";
				foreach my $scsiLun ( @$scsiLuns )
				{
					if( $scsiLun->canonicalName eq $extent->diskName )
					{
						$mapDatastoreInfo{vendor}		.= $scsiLun->vendor . ",";
						$mapDatastoreInfo{displayName} 	.= $scsiLun->displayName . ",";
						$mapDatastoreInfo{lunNumber}	.= $mapLunIdNumber{ $scsiLun->key } . ",";
						last;
					}
				}
			}
			if( ( exists $mapDatastoreInfo{diskName} ) && ( rindex($mapDatastoreInfo{diskName},",") == length($mapDatastoreInfo{diskName})-1 ) )
			{
				chop( $mapDatastoreInfo{diskName} );
			}
			if( ( exists $mapDatastoreInfo{displayName} ) && ( rindex($mapDatastoreInfo{displayName},",") == length($mapDatastoreInfo{displayName})-1 ) )
			{
				chop( $mapDatastoreInfo{displayName} );
			}
			if( ( exists $mapDatastoreInfo{vendor} ) && ( rindex($mapDatastoreInfo{vendor},",") == length($mapDatastoreInfo{vendor})-1 ) )
			{
				chop( $mapDatastoreInfo{vendor} );
			}
			if( ( exists $mapDatastoreInfo{lunNumber} ) && ( rindex($mapDatastoreInfo{lunNumber},",") == length($mapDatastoreInfo{lunNumber})-1 ) )
			{
				chop( $mapDatastoreInfo{lunNumber} );
			}

			foreach ( @$datastoresMOB )
			{
				my $hostsInfo	= $_->host;
				foreach my $host ( @$hostsInfo )
				{
					if( ( $host->mountInfo->path ne $mapDatastoreInfo{name} ) || ( $host->key->value ne $hostKeyValue ) )
					{
						next;
					}
					my $GB 						= ( $_->summary->capacity ) / ( 1024*1024*1024 );
					$mapDatastoreInfo{capacity} = sprintf("%.2f",$GB);
					my $freespace_GB 			= ( $_->summary->freeSpace ) / ( 1024*1024*1024 );
					$mapDatastoreInfo{freeSpace}= sprintf("%.2f",$freespace_GB);
					$mapDatastoreInfo{refId}	= $_->{mo_ref}->{value};
					push @datastoreInfo , \%mapDatastoreInfo;
					Common_Utils::WriteMessage("capacity = $mapDatastoreInfo{capacity}, vmfs filesystem = $mapDatastoreInfo{fileSystem}, freeSpace = $mapDatastoreInfo{freeSpace}.",2);
					last;
				}	
			}
		}
	}
	return ( $Common_Utils::SUCCESS , \@datastoreInfo )
}

#####CreatePath#####
##Description 			:: Creates specfied path on vSphere/vCenter accessed datastores.
##Input 				:: Path to be created -- which states DatastoreName, Root Folder and sub sequent Folder or File Name.
##Output				:: Returns SUCCESS when successfully Created, 
##						   FAILURE when there is an ERROR.
##						   Returns Different Error Code when Path to be created already Exist.
#####CreatePath#####
sub CreatePath
{
	my $pathToCreate 			= shift;
	my $datacenterName			= shift;
	
	my $serviceContnent 		= Vim::get_service_content();
	my $fileManager				= Vim::get_view( mo_ref => $serviceContnent->fileManager, properties => [] );
	
	( my $returnCode , my $datacenterView )= DatacenterOpts::GetDataCenterView( $datacenterName );
	if ( $Common_Utils::SUCCESS ne $returnCode )
	{
		return ( $Common_Utils::FAILURE );
	}
	
	eval
	{
		$fileManager->MakeDirectory( name => "$pathToCreate" , datacenter => $datacenterView , createParentDirectories => "true" );
	};
	if( $@ )
	{
		if( ref($@->detail) eq 'CannotCreateFile' ) 
		{
			Common_Utils::WriteMessage("ERROR :: $@.",3);
			Common_Utils::WriteMessage("ERROR :: Failed to create path $pathToCreate.",3);
			return ( $Common_Utils::FAILURE );
		}
		elsif( ref($@->detail) eq 'FileAlreadyExists' )
		{
			return ( $Common_Utils::EXIST );
		}
		else
		{
			Common_Utils::WriteMessage("ERROR :: $@.",3);
			Common_Utils::WriteMessage("ERROR :: Failed to create path $pathToCreate.",3);
			return ( $Common_Utils::FAILURE );
		}
	}
	Common_Utils::WriteMessage("Successfully created path $pathToCreate.",2);
	return ( $Common_Utils::SUCCESS );
}

#####DeletePath#####
##Description 			:: Deletes specfied path on vSphere/vCenter accessed datastores.
##Input 				:: Path to be deleted -- which states DatastoreName, Root Folder and sub sequent Folder or File Name.
##Output				:: Returns SUCCESS when successfully Created, 
##						   FAILURE when there is an ERROR.
#####DeletePath#####
sub DeletePath
{
	my %args 			= @_;
	
	my $serviceContnent = Vim::get_service_content();
	my $fileManager		= Vim::get_view( mo_ref => $serviceContnent->fileManager, properties => [] );
	
	eval
	{
		$fileManager->DeleteDatastoreFile( name => "$args{pathToDelete}" , datacenter => $args{datacenterView} );
	};
	if( $@ )
	{
		if( $@ =~ /Can't call method \"state\" on an undefined value/i )
		{
			Common_Utils::WriteMessage("ERROR :: Considering this as timing issue and continuing.",2);
			Common_Utils::WriteMessage("ERROR :: Failed to delete path $args{pathToDelete}.",2);	
			Common_Utils::WriteMessage("ERROR :: $@.",2);
			return ( $Common_Utils::SUCCESS );
		}
		if( ref($@->detail) eq 'CannotCreateFile' ) 
		{
			Common_Utils::WriteMessage("ERROR :: $@.",3);
			Common_Utils::WriteMessage("ERROR :: Failed to delete path $args{pathToDelete}.",3);
			return ( $Common_Utils::FAILURE );
		}
		elsif( ref($@->detail) eq 'FileAlreadyExists' )
		{
			return ( $Common_Utils::EXIST );
		}
		else
		{
			Common_Utils::WriteMessage("ERROR :: $@.",3);
			Common_Utils::WriteMessage("ERROR :: Failed to delete path $args{pathToDelete}.",3);
			return ( $Common_Utils::FAILURE );
		}
	}
	Common_Utils::WriteMessage("Successfully deleted Path $args{pathToDelete}",2);
	return ( $Common_Utils::SUCCESS );
}	

#####IsDataStoreAccessible#####
##Description 			:: Checks whether the datastore is accessible by "Searching the dataStore".If datastore is accessible it
##							returns SUCCESS else it FAILS.
##Input 				:: Name of the datastore to check.
##Output 				:: Returns SUCCESS if accessible else FAILURE.
#####IsDataStoreAccessible#####
sub IsDataStoreAccessible
{
	my $hostView			= shift;
	my $datastoreName 		= shift;
	
	my $hostDatastoreBrowser= Vim::get_view (mo_ref => $hostView->datastoreBrowser, properties => [] );
	
	Common_Utils::WriteMessage("Checking accessibility of datastore $datastoreName.",2);
	eval
	{
		$hostDatastoreBrowser->SearchDatastore( datastorePath => "[$datastoreName]" );
	};
	if ( $@ )
	{
		Common_Utils::WriteMessage("$@",3);
		Common_Utils::WriteMessage("ERROR :: Please check with vCenter/ESX user privileges.",3);
		return ( $Common_Utils::FAILURE )
	}
	
	return ( $Common_Utils::SUCCESS );
}

#####RemovePath#####
##Description 			:: Removes the specified from datastore.
##Input 				:: Path to be removed.
##Output 				:: Returns SUCCESS if removed else FAILURE.
#####RemovePath#####
sub RemovePath
{
	my $datacenterName	= shift;
	my $pathToRemove	= shift;
	my $ForceOption		= shift;
	Common_Utils::WriteMessage("Removing path $pathToRemove.",2);
	my $serviceContnent = Vim::get_service_content();
	my $fileManager		= Vim::get_view( mo_ref => $serviceContnent->fileManager, properties => [] );
	
	( my $returnCode , my $datacenterView )= DatacenterOpts::GetDataCenterView( $datacenterName );
	if ( $Common_Utils::SUCCESS ne $returnCode )
	{
		return ( $Common_Utils::FAILURE );
	}
	
	eval
	{
		$fileManager->DeleteDatastoreFile( name => $pathToRemove , datacenter => $datacenterView );
	};
	if ( $@ )
	{
		if( $@ =~ /Can't call method \"state\" on an undefined value/i )
		{
			Common_Utils::WriteMessage("ERROR :: Considering this as timing issue and continuing.",2);
			Common_Utils::WriteMessage("ERROR :: Failed to remove the path \"$pathToRemove\".",2);	
			Common_Utils::WriteMessage("ERROR :: $@.",2);
			return ( $Common_Utils::SUCCESS );
		}		
		Common_Utils::WriteMessage("ERROR :: Failed to remove the path \"$pathToRemove\".",3);	
		Common_Utils::WriteMessage("ERROR :: $@.",3);
		return ( $Common_Utils::FAILURE );
	}
	
	return ( $Common_Utils::SUCCESS );
}							
							
#####ValidateDatastores#####
##Description 			:: Check whether Datastores in the list are accessible? writeable?.
##Input 				:: refernece to the List of DataStore Information.
##Output 				:: Returns Reference to dataStore List with new parameter added(isAccessible).
#####ValidateDatastores#####
sub ValidateDatastores
{
	my $hostView			= shift;
	my $listDatastoreInfo	= shift;
	my $datacenterName 		= shift;
	my @listDatastoreInfo	= @$listDatastoreInfo;
	my $returnCode			= 0;
	my $ReturnCode			= $Common_Utils::SUCCESS;

	for ( my $i = 0 ; $i <= $#listDatastoreInfo ; $i++ )
	{
		$listDatastoreInfo[$i]->{folderFileInfo}	= "";
		if ( $Common_Utils::SUCCESS ne IsDataStoreAccessible( $hostView , $listDatastoreInfo[$i]->{symbolicName} ) )
		{
			$listDatastoreInfo[$i]->{isAccessible} 	= "No";
			$listDatastoreInfo[$i]->{readOnly}		= "Yes";
			next;
		}
		$listDatastoreInfo[$i]->{isAccessible} 		= "Yes";
		
		( $returnCode , my $folderFileInfo , my $fileInfo )	= FindFileInfo( $hostView , "*" , "[$listDatastoreInfo[$i]->{symbolicName}]" , "folderfileinfo" );
		if ( $returnCode eq $Common_Utils::FAILURE )
		{
			$ReturnCode	= $Common_Utils::FAILURE;
		}
		$listDatastoreInfo[$i]->{folderFileInfo}= $folderFileInfo;
				
		my $FolderPath 		= "[$listDatastoreInfo[$i]->{symbolicName}] InMageDummyTemp";
		if( lc($listDatastoreInfo[$i]->{type}) ne "vsan" )
		{
			for( my $j = 0 ; ; $i++ )
			{
				$returnCode 	= CreatePath( $FolderPath , $datacenterName );
				if ( $Common_Utils::EXIST eq $returnCode )
				{
					$FolderPath	= "$FolderPath$j";
					next;
				}
				elsif( $Common_Utils::FAILURE eq $returnCode )
				{
					$listDatastoreInfo[$i]->{readOnly}	= "Yes";
					last;
				}
				$listDatastoreInfo[$i]->{readOnly}		= "No";
				last;
			}
			
			if ( "no" eq lc ( $listDatastoreInfo[$i]->{readOnly} ) )
			{
				$returnCode	= RemovePath( $datacenterName , $FolderPath , 1 );
				if( $Common_Utils::SUCCESS ne $returnCode )
				{
					$ReturnCode	= $Common_Utils::FAILURE;
				}
			}
		}
		else
		{
			my $serviceContnent 		= Vim::get_service_content();
			my $datastoreNamespaceMngr	= Vim::get_view( mo_ref => $serviceContnent->datastoreNamespaceManager, properties => [] );
			
			( my $returnCode , my $datacenterView )= DatacenterOpts::GetDataCenterView( $datacenterName );
			if ( $Common_Utils::SUCCESS ne $returnCode )
			{
				return ( $Common_Utils::FAILURE );
			}
			
			my $dsMoRef	= "";
			if(  defined $datacenterView->datastore )
			{
				foreach my $datastore ( @{$datacenterView->datastore} )
				{
					if( $listDatastoreInfo[$i]->{refId} eq $datastore->value )
					{
						$dsMoRef	= $datastore;
					}
				}
			}
			
			my $datastoreView 	= Vim::get_view( mo_ref => $dsMoRef, properties => [] );
			my $createdPath		= "";
	
			eval
			{
				$createdPath	= $datastoreNamespaceMngr->CreateDirectory( displayName => "InMageDummyTemp" , datastore => $datastoreView  );
			};
			if( $@ )
			{		
				Common_Utils::WriteMessage("ERROR :: $@.",3);
				Common_Utils::WriteMessage("ERROR :: Failed to create path InMageDummyTemp on vSan datastore.",3);
				return ( $Common_Utils::FAILURE );
			}
			Common_Utils::WriteMessage("Successfully created path InMageDummyTemp($createdPath) on vSan datastore.",2);
			
			eval
			{
				$datastoreNamespaceMngr->DeleteDirectory( datastorePath => $createdPath, datacenter => $datacenterView  );
			};
			if( $@ )
			{		
				Common_Utils::WriteMessage("ERROR :: $@.",3);
				Common_Utils::WriteMessage("ERROR :: Failed to delete path InMageDummyTemp on vSan datastore.",3);
				return ( $Common_Utils::FAILURE );
			}
			Common_Utils::WriteMessage("Successfully deleted the path InMageDummyTemp($createdPath) on vSan datastore.",2);			
		}
	}
	return( $ReturnCode, \@listDatastoreInfo );
}

#####FindFileInfo#####
##Description 		:: Searches the datastore for the Specified Path or Lists all the information.
##Input 			:: DataStoreName, hostView and search Criteria.
##Output 			:: Returns SUCCESS and Folder or File Information else FAILURE.
#####FindFileInfo#####
sub FindFileInfo
{
	my $hostView			= shift;
	my $searchFileOrPath 	= shift;
	my $searchInPath		= shift;
	my $searchType			= shift;
	my $ReturnCode			= $Common_Utils::SUCCESS;
	my @folderFileInfo		= ();
	my @fileInfo			= ();
	
	my $datastoreBrowser 	= Vim::get_view ( mo_ref => $hostView->datastoreBrowser, properties => [] ); 
	eval
	{
		my @matchPattern 	= ("$searchFileOrPath");
		my $details			= FileQueryFlags->new( fileOwner => 'True' , fileType => 'True' ,fileSize => 'True' , modification => 'True' );
		my $HDBS 			= HostDatastoreBrowserSearchSpec->new( details => $details , matchPattern => \@matchPattern , sortFoldersFirst => 'True', searchCaseInsensitive => 'False' );
		my $task	 		= $datastoreBrowser->SearchDatastore_Task( datastorePath => $searchInPath , searchSpec => $HDBS );	
		my $taskView 		= Vim::get_view( mo_ref => $task );
		
		while(1)
		{
			my $taskInfo 	= $taskView->info;
			if( $taskInfo->state->val eq 'success' )
			{
				last;
			}
			elsif( $taskInfo->state->val eq 'error' )
			{
				 my $soap_fault 	= SoapFault->new;
	             my $errorMesg 		= $soap_fault->fault_string( $taskInfo->error->localizedMessage );
	             Common_Utils::WriteMessage("ERROR :: Failed to list all the file and folder information on datastore \"$searchInPath\".",3);
	             Common_Utils::WriteMessage("ERROR :: $errorMesg.",3);
	             $ReturnCode		= $Common_Utils::FAILURE;
	             return;
			}
			else
			{
				sleep(5);
			}
			$taskView->ViewBase::update_view_data();
		}
		
		my $result = $taskView->info->result->file;
		if( !defined($result) )
		{
			Common_Utils::WriteMessage("ERROR :: Failed to list all the file and folder information on datastore \"$searchInPath\".",2);
			Common_Utils::WriteMessage("ERROR :: Might be datastore is empty.",2);
			$ReturnCode	= $Common_Utils::NOTFOUND; 
			return;
		}
		
		foreach my $info ( @$result )
		{
			if ( $searchType ne lc( ref($info) ) )
			{
				next;
			}
			push @folderFileInfo , $info->path;
			push @fileInfo	, $info;
		}
	};
	if( $@ )
	{
		Common_Utils::WriteMessage("ERROR :: $@.",3);
		Common_Utils::WriteMessage("ERROR :: Failed to list all the file and folder information on datastore \"$searchInPath\".",3);
		$ReturnCode	= $Common_Utils::FAILURE;
	}
	
	return ( $ReturnCode, \@folderFileInfo, \@fileInfo );
} 

#####GetLatestFileInfo#####
##Description	:: Find latest file which matches with search patter across all datastores.
##Input 		:: Search pattern, list of datastores.
##Output 		:: Returns SUCCESS and Latest File info else FAILURE.
#####GetLatestFileInfo#####
sub GetLatestFileInfo
{
	my $hostView			= shift;
	my $searchPattern 		= shift;
	my $searchType			= shift;
	my $listDatastoreInfo	= shift;
	
	my %fileInfo			= ();
	my %mapFileAndPath		= ();
	my @tempArr				= @$listDatastoreInfo;
	for ( my $i = 0 ; $i <= $#tempArr ; $i++ )
	{
		my $searchInPath	= "[$$listDatastoreInfo[$i]->{symbolicName}]";
		Common_Utils::WriteMessage("Search Pattern = $searchPattern , in path = $searchInPath , of Type = $searchType.",2);
		my( $returnCode , $folderFileInfo , $fileInfo )	= FindFileInfo( $hostView , $searchPattern , $searchInPath , $searchType );
		if ( $returnCode ne $Common_Utils::SUCCESS )
		{
			return ( $Common_Utils::FAILURE );
		}
		
		foreach my $file ( @$fileInfo )
		{
			if( exists $fileInfo{ $file->path } )
			{
				my $fileName				= $file->path;
				my $existingFileModTime		= $fileInfo{ $file->path }->modification;
				my $newFileModTime			= $file->modification;
				
				Common_Utils::WriteMessage("File Name = $fileName , existing time = $existingFileModTime , New Time = $newFileModTime.",2);
				my @existingFileDateAndTime	= split( /T|Z/ , $existingFileModTime );
				my @newFileDateAndTime		= split( /T|Z/ , $newFileModTime );
				
				if ( $existingFileDateAndTime[0] eq $newFileDateAndTime[0] )
				{
					if ( $existingFileDateAndTime[1] lt $newFileDateAndTime[1] )
					{
						Common_Utils::WriteMessage("Time differs.Taken new file in to consideration and Modification time = $newFileModTime.",2);
						$fileInfo{$file->path}			= $file;
						$mapFileAndPath{ $fileName }	= $searchInPath;
					}
				}
				elsif ( $existingFileDateAndTime[0] lt $newFileDateAndTime[0] )
				{
					$mapFileAndPath{ $fileName }		= $searchInPath;
					$fileInfo{$file->path}				= $file;
					Common_Utils::WriteMessage("Date differs.Taken new file in to consideration and Modification time = $newFileModTime.",2);
				}
				next;
			}
			$fileInfo{$file->path }			= $file;
			$mapFileAndPath{ $file->path }	= $searchInPath;
		}
	}

	return ( $Common_Utils::SUCCESS , \%mapFileAndPath );	
}

#####DownloadFiles#####
##Description 	:: Download's list of files from Datastore using DatastoreOpts::do_get(). 
##Input 		:: Reference to a Hash map is passed, where Key = File to be downloaded and Value = Datastore name where file is.
##Output 		:: It return SUCCESS after downloading all file else FAILURE.
#####DownloadFiles#####
sub DownloadFiles
{
	my $mapOfFileAndPath	= shift;
	my $datacenterName		= shift;
	
	foreach my $key ( sort keys %$mapOfFileAndPath )
	{
		my $fileToDownload 	= $$mapOfFileAndPath{$key}." ".$key;
		if ( $Common_Utils::SUCCESS ne do_get( $fileToDownload , "$Common_Utils::vContinuumMetaFilesPath/$key" , $datacenterName ) )
		{
		}
	}
	return ( $Common_Utils::SUCCESS );
}

#####do_get#####
##Description 	:: it get's the files from a datastore over HTTP protocol.
##Input 			:: remote file path ,local file path.
##Output 		:: Returns SUCCESS or FAILURE
#####do_get#####
sub do_get 
{
	my ($remote_source, $local_target , $dcName) 	= @_;
	Common_Utils::WriteMessage("Trying to download file from $remote_source to $local_target.",2);
	my ($mode, $dc, $ds, $filepath) 				= VIExt::parse_remote_path($remote_source);

	# Bug #10191 Need to encode all the special characters used in URLS.
	# Read this link for more information ---> http://www.eskimo.com/~bloo/indexdot/html/topics/urlencoding.htm
	my $orig_filepath = $filepath;

	$filepath =~ s/\%/\%25/g;
	$filepath =~ s/\+/\%2B/g;
	$filepath =~ s/\$/\%24/g;
	$filepath =~ s/\,/\%2C/g;
	$filepath =~ s/ /\%20/g;
	$filepath =~ s/</\%3C/g;
	$filepath =~ s/>/\%3E/g;
	$filepath =~ s/&/\%26/g;
	$filepath =~ s/:/\%3A/g;
	$filepath =~ s/\//\%2F/g;
	$filepath =~ s/;/\%3B/g;
	$filepath =~ s/=/\%3D/g;
	$filepath =~ s/\?/\%3F/g;
	$filepath =~ s/#/\%23/g;

	# Unable to find how to substitute @ character TO DO: Need to fix this later
	#	$filepath =~ s/@/\%40/g;

	#Lets add more of these as they come along
	
	if (-d $local_target)
	{
		my $local_filename 	= $orig_filepath ;
		$local_filename 	=~ s@^.*/([^/]*)$@$1@;
		$local_target 		.= "/" . $local_filename;
	}
    my $resp = VIExt::http_get_file($mode, $filepath, $ds, $dcName, $local_target);

    if ( !$resp->is_success )
	{
		my $errorMesg		= $resp->status_line;
		Common_Utils::WriteMessage("ERROR :: $errorMesg",3);
		Common_Utils::WriteMessage("ERROR :: Downloading file ($orig_filepath) to ($local_target) failed.",3);
		return ( $Common_Utils::FAILURE );
	}

	Common_Utils::WriteMessage("Downloaded file ($orig_filepath) to ($local_target) successfully.",2);
	return ( $Common_Utils::SUCCESS );
}

#####do_put#####
##Description		:: Puts file on datastore.	
#Input 				:: local file path and remote target path.
#Output				:: Return SUCCESS or FAILURE.
#####do_put#####
sub do_put
{
	my ($local_source, $remote_target , $dcName) = @_;
	
	if(! -e $local_source)
	{
		Common_Utils::WriteMessage("ERROR :: Unable to find file \"$local_source\" which has to be up-loaded on to ESX server.",3);
		Common_Utils::WriteMessage("ERROR :: Please rerun after checking for presence of file in local system.",3);
		return ( $Common_Utils::FAILURE );
	}
	my ($mode, $dc, $ds, $filepath) = VIExt::parse_remote_path($remote_target);

	# Bug #10191 Need to encode all the special characters used in URLS.
	# Read this link for more information ---> http://www.eskimo.com/~bloo/indexdot/html/topics/urlencoding.htm
	my $orig_filepath = $filepath;
	$filepath =~ s/\%/\%25/g;
	$filepath =~ s/\+/\%2B/g;
	$filepath =~ s/\$/\%24/g;
	$filepath =~ s/\,/\%2C/g;
	$filepath =~ s/ /\%20/g;
	$filepath =~ s/</\%3C/g;
	$filepath =~ s/>/\%3E/g;
	$filepath =~ s/&/\%26/g;
	$filepath =~ s/:/\%3A/g;
	$filepath =~ s/\//\%2F/g;
	$filepath =~ s/;/\%3B/g;
	$filepath =~ s/=/\%3D/g;
	$filepath =~ s/\?/\%3F/g;
	$filepath =~ s/#/\%23/g;

	# Unable to find how to substitute @ character TO DO: Need to fix this later
	# $filepath =~ s/@/\%40/g;

	# Lets add more of these as they come along
	
	my $resp = VIExt::http_put_file( $mode, $local_source, $filepath, $ds, $dcName );
	if ( ( $resp == 1 ) || ( $resp->is_success ) )
	{
      Common_Utils::WriteMessage("Uploaded file ($local_source) to ($orig_filepath) successfully.",2);
      return ( $Common_Utils::SUCCESS );
   	} 
   	else 
   	{
      Common_Utils::WriteMessage("Uploading of file ($local_source) to ($orig_filepath) Failed.",3);
      return ( $Common_Utils::FAILURE );
  	}
}

#####MoveOrCopyFile#####
##Description	:: Moves/Copies file from remote location to local remote path.
##Input 		:: Operation( Move /Copy ), Source and Target Paths, datacenterView and force option to use.
##Output 		:: Returns SUCCESS or FAILURE.
#####MoveOrCopyFile#####
sub MoveOrCopyFile 
{
	my ( $op, $datacenterName, $source, $target, $force ) = @_;
	my $ReturnCode				= $Common_Utils::SUCCESS;

	my $serviceContnent 		= Vim::get_service_content();
	my $fileManager				= Vim::get_view( mo_ref => $serviceContnent->fileManager, properties => [] );
	
	my ( $returnCode, $datacenterView )= DatacenterOpts::GetDataCenterView( $datacenterName );
	if ( $Common_Utils::SUCCESS ne $returnCode )
	{
		return ( $Common_Utils::FAILURE );
	}
   
    if ($op ne "copy" && $op ne "move")
    {
    	Common_Utils::WriteMessage("ERROR :: Invalid Operation specified.",2);
    	Common_Utils::WriteMessage("ERROR :: Operation has to be either move/copy.",2);
    	return ( $Common_Utils::FAILURE );
   	}

   eval 
   { 
   		my $task 		= "";
   	    if ($op eq "copy")
       	{
       		$task 		= $fileManager->CopyDatastoreFile_Task(
				         										sourceName 				=> $source, 
				         									 	sourceDatacenter		=> $datacenterView,	
				            								 	destinationDatacenter 	=> $datacenterView, 
				            						 			destinationName 		=> $target,
				            						 			force 					=> $force
				            						 		);
        
      } 
      else 
      {
      	  $task 		= $fileManager->MoveDatastoreFile_Task(
				         										sourceName 				=> $source,
				         							 			sourceDatacenter		=> $datacenterView,
				           							 			destinationDatacenter 	=> $datacenterView, 
				            						 			destinationName 		=> $target,
								       						    force 					=> $force
								       						  );
      }
      
      my $task_view 	= Vim::get_view( mo_ref => $task );
	  while ( 1 ) 
	  {
	      my $info 	= $task_view->info;
	      if( $info->state->val eq 'running' )
	      {
	         Common_Utils::WriteMessage("$op of $source to $target is in progress.",2);
	      } 
	      elsif( $info->state->val eq 'success' ) 
	      {
	         Common_Utils::WriteMessage("$op of file from $source to $target is successfull.",2);
	         last;
	      } 
	      elsif( $info->state->val eq 'error' ) 
	      {
	         my $soap_fault = SoapFault->new;
	         my $errorMesg 	= $soap_fault->fault_string($info->error->localizedMessage);
	         Common_Utils::WriteMessage("ERROR :: $op of $source to $target failed with error \"$errorMesg\".",3);
	         $ReturnCode	= $Common_Utils::FAILURE;
	         last;
	      }
	      sleep 2;
	      $task_view->ViewBase::update_view_data();
	  }		
   };
   if ($@) 
   {
   		if( $@ =~ /Can't call method \"state\" on an undefined value/i )
		{
			Common_Utils::WriteMessage("ERROR :: Considering this as timing issue and continuing.",2);
			Common_Utils::WriteMessage("ERROR :: Unable to $op $source to $target.",2);	
			Common_Utils::WriteMessage("ERROR :: $@.",2);
			return ( $Common_Utils::SUCCESS );
		}
   		
   		Common_Utils::WriteMessage("ERROR :: $@.",2);
   		Common_Utils::WriteMessage("ERROR :: Unable to $op $source to $target.",2);
   		$ReturnCode	= $Common_Utils::FAILURE;
   		if(ref($@->detail) eq 'FileAlreadyExists')
   		{
			$ReturnCode	= $Common_Utils::EXIST;      		
   		}     	
   }
   
   return ( $ReturnCode );
}

#####QueryUnresolvedVmfsVolumes#####
##Description 			:: Querying Unresolved Vmfs Volumes(virtual snaps of luns).
##Input 				:: hostview.
##Output 				:: Return host unresolved vmfs on Success or Failure.
#####QueryUnresolvedVmfsVolumes#####
sub QueryUnresolvedVmfsVolumes
{
	my $hostView	= shift;
	my $hostDatastoreSystem	= Vim::get_view( mo_ref => Common_Utils::GetViewProperty( $hostView, 'configManager.datastoreSystem' ), properties => [] );

	my $hostUnResolvedVmfs;
	eval
	{
		$hostUnResolvedVmfs = $hostDatastoreSystem->QueryUnresolvedVmfsVolumes();
	};
	if($@)
	{
		Common_Utils::WriteMessage("ERROR :: unble to get unresovled vmfs volumes: \n $@.",3);
		return ( $Common_Utils::FAILURE );
	}
	
	my @tempArr	= @$hostUnResolvedVmfs;
	if( $#tempArr == -1)
	{
		Common_Utils::WriteMessage("ERROR :: No volumes found for datastore creation.",3);
		Common_Utils::WriteMessage("ERROR :: Please perform manual rescan for new storage device and rerun.",3);
		return ( $Common_Utils::FAILURE );
	}
	
	return ( $Common_Utils::SUCCESS, $hostUnResolvedVmfs );
}

#####ResignatureUnresolvedVmfsVolumes#####
##Description 			:: Resignaturing the Unresolved Vmfs Volumes(virtual snaps of luns).
##Input 				:: hostview and luns device paths.
##Output 				:: Return Resolved Datastore views on Success or Failure.
#####ResignatureUnresolvedVmfsVolumes#####
sub ResignatureUnresolvedVmfsVolumes
{
	my $hostView			= shift;
	my $resignPaths			= shift;
	my @resignPaths			= @$resignPaths;
	my @taskViews			= ();
	my @devicePaths			= ();
	my %mapDeviceDatastore	= ();
	my $ReturnCode			= $Common_Utils::SUCCESS;
	my $noOfTasks			= 5;
	
	if( $noOfTasks > $#resignPaths + 1 )
	{
		$noOfTasks	= $#resignPaths + 1;
	}
	
	my $hostDatastoreSystem	= Vim::get_view( mo_ref => Common_Utils::GetViewProperty( $hostView, 'configManager.datastoreSystem' ), properties => [] );
	
	my $i	= 1;
	while( $i <= $noOfTasks )
	{
		my $devicePath	= shift @resignPaths;
		eval
		{
			my $task = $hostDatastoreSystem->ResignatureUnresolvedVmfsVolume_Task( resolutionSpec => 
											HostUnresolvedVmfsResignatureSpec-> new ( extentDevicePath => [ $devicePath ] ) );
			
			my $taskView 	= Vim::get_view( mo_ref => $task );
			push @taskViews, $taskView;
			push @devicePaths, $devicePath;			
		};
		if($@)
		{
			Common_Utils::WriteMessage("ERROR :: unble to resignature the vmfs lun $devicePath \n $@.",3);
			$ReturnCode	= $Common_Utils::FAILURE;
		}
		
		if( ( $i < $noOfTasks ) && ( -1 != $#resignPaths ) )
		{
			$i++;
			next;
		}
		
		for(my $j = 0; $j <= $#devicePaths ; $j++)
	   	{
	   		my $deviceName = $devicePaths[$j];
	   		eval
			{
		   		while(1)
		   		{
		   			my $taskView 	= $taskViews[$j];
				   	my $info 		= $taskView->info;
			        if( $info->state->val eq 'running' )
			        {
			        	
			        } 
			        elsif( $info->state->val eq 'success' ) 
			        {
			        	my $resingedDsView	= Vim::get_view( mo_ref => $info->result->result, properties => ["name"] );
						my $newDsName		= $resingedDsView->name;
						Common_Utils::WriteMessage("successfully resigned the device \"$deviceName\" mounted datastore \"$newDsName\".",2);
						$mapDeviceDatastore{$deviceName}	= $newDsName;
			            last;
			        }  
			        elsif( $info->state->val eq 'error' ) 
			        {
			        	my $soap_fault 	= SoapFault->new;
			            my $errorMesg 	= $soap_fault->fault_string($info->error->localizedMessage);
			            Common_Utils::WriteMessage("ERROR :: Failed to resign the device \"$deviceName\" due to error \"$errorMesg\".",3);
			            $ReturnCode	= $Common_Utils::FAILURE;
			            last;
			        }
			        $taskView->ViewBase::update_view_data();
		   		}
			};
			if ( $@ )
			{
				Common_Utils::WriteMessage("ERROR :: .\n $@",3);
				$ReturnCode 	= $Common_Utils::FAILURE;
			}
	    }
	    $i 			= 1;
	    @devicePaths= ();
	    @taskViews	= ();
	    if( $noOfTasks > $#resignPaths + 1 )
		{
			$noOfTasks	= $#resignPaths + 1;
		}		
	}
	
	return ( $ReturnCode, \%mapDeviceDatastore );
}

#####RescanVmfs#####
##Description 			:: Rescan the host for new datastores.
##Input 				:: hostview.
##Output 				:: Return Success or Failure.
#####RescanVmfs#####
sub RescanVmfs
{
	my $hostView	= shift;
	my $hostStorageSystem	= Vim::get_view( mo_ref => Common_Utils::GetViewProperty( $hostView, 'configManager.storageSystem' ), properties => [] );
	eval
	{
		$hostStorageSystem->RescanVmfs();
	};
	if($@)
	{
		Common_Utils::WriteMessage("ERROR :: unble to rescan for new datastores: \n $@.",3);
		return $Common_Utils::FAILURE;
	}
	$hostView->ViewBase::update_view_data( HostOpts::GetHostProps() );
	return $Common_Utils::SUCCESS;
}

#####ArrayDatastoresResolvebilityCheck#####
##Description 			:: It will check Host Unresolved Vmfs Volume Resolve Status using for array snap shot.
##Input 				:: host unresolved vmfs volumes and required datastores.
##Output 				:: Return Success or Failure.
#####ArrayDatastoresResolvebilityCheck#####
sub ArrayDatastoresResolvebilityCheck
{
	my $hostUnResolvedVmfs	= shift;
	my $requiredDatastores	= shift;
	my $ReturnCode			= $Common_Utils::SUCCESS;
	
	foreach my $requiredDs ( @$requiredDatastores)
	{
		my $dsFound	= 0;
		my $dsName	= $$requiredDs{datastoreName};
		foreach my $unResVmfsVol ( @$hostUnResolvedVmfs )
		{
			unless( $$requiredDs{uuid} eq $unResVmfsVol->vmfsUuid )
			{
				next;
			}
			
			unless( $unResVmfsVol->resolveStatus->resolvable )
			{				
				if( $unResVmfsVol->resolveStatus->multipleCopies )
				{
					Common_Utils::WriteMessage("ERROR :: Found multilple virtual luns of datastore $dsName.",3);
					$ReturnCode	= $Common_Utils::FAILURE;
				}
				
				if( $unResVmfsVol->resolveStatus->incompleteExtents )
				{
					my @diskNames		= split(",", $$requiredDs{diskName});
					my $noOfExtents		= $#diskNames;
					my $unResVmfsExtents= $unResVmfsVol->extent;					
					for( my $i = 0; $i <= $noOfExtents; $i++ )
					{
						my $found	= 0;
						foreach my $unResVmfsExtent ( @$unResVmfsExtents )
						{
							if($i == $unResVmfsExtent->ordinal)
							{
								$found	= 1;
								last;
							}
						}
						unless($found)
						{
							my $missingLun	= $diskNames[$i];
							Common_Utils::WriteMessage("ERROR :: $missingLun extent virtual lun is not available for datastore $dsName.",3);
						}
					}
					Common_Utils::WriteMessage("ERROR :: some of the extents are not available for datastore $dsName.",3);
					$ReturnCode	= $Common_Utils::FAILURE;
				}
			}
			else
			{
				Common_Utils::WriteMessage("Datastore $dsName virtual luns are ",3);
				my $unResVmfsExtents= $unResVmfsVol->extent;
				my $count	= 1;
				foreach my $unResVmfsExtent ( @$unResVmfsExtents )
				{
					my $deviceName	= $unResVmfsExtent->devicePath;
					Common_Utils::WriteMessage("$count.$deviceName",3);
					$count++;
				}
				$dsFound	= 1;
			}
		}
		unless( $dsFound )
		{
			Common_Utils::WriteMessage("ERROR :: Not found Virtual luns for datastore $dsName.",3);
			$ReturnCode	= $Common_Utils::FAILURE;
		}
	}
	return $ReturnCode;
}

1;