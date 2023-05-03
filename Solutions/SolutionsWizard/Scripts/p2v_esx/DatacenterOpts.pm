=head
		DatacenterOpts
		--------------
		1. To List DataCenter details on vCenter/vSphere.
		
=cut

package DatacenterOpts;

use strict;
use warnings;
use Common_Utils;
use HostOpts;

my @dcProps = ("name","datastore", "vmFolder");

#####GetDatacenterViews#####
##Description 		:: Gets views of a specific datacenter on vCenter/vSphere.
##Input 			:: None.
##Output			:: Returns DataCenterViews on SUCCESS else FAILURE.
#####GetDataCenterViews#####
sub GetDataCenterViews
{
	my $datacenterViews	= Vim::find_entity_views( view_type => 'Datacenter' ); 
	
	unless ( @$datacenterViews )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to get Datacenter views from vCenter/vSphere.",3);
		return ( $Common_Utils::FAILURE , $datacenterViews );
	}
	return ( $Common_Utils::SUCCESS , $datacenterViews );
}

#####GetDataCenterViewsByProps#####
##Description 		:: Gets views of a specific datacenter on vCenter/vSphere.
##Input 			:: None.
##Output			:: Returns DataCenterViews on SUCCESS else FAILURE.
#####GetDataCenterViewsByProps#####
sub GetDataCenterViewsByProps
{
	my $datacenterViews;
	eval
	{
		$datacenterViews	= Vim::find_entity_views( view_type => 'Datacenter', properties => \@dcProps ); 
	};
	if( $@ )
	{
		Common_Utils::WriteMessage("Failed to get datacenter views by poperties from vCenter/vSphere : \n $@.",3);
		return ( $Common_Utils::FAILURE , $datacenterViews );
	}
	
	unless ( @$datacenterViews )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to get Datacenter views from vCenter/vSphere.",3);
		return ( $Common_Utils::FAILURE , $datacenterViews );
	}
	return ( $Common_Utils::SUCCESS , $datacenterViews );
}

#####GetDatacenterView#####
##Description 		:: Gets view of a specific datacenter on vCenter/vSphere.
##Input 			:: Datacenter Name for which the view has to be found.
##Output			:: Returns DataCenterView on SUCCESS else FAILURE.
#####GetDataCenterView#####
sub GetDataCenterView
{
	my $datacenterName 	= shift;
	
	$datacenterName	= Common_Utils::EncodeNameSpecialChars( $datacenterName );
	my $datacenterView	= Vim::find_entity_view( view_type=> 'Datacenter' , filter => { 'name' => $datacenterName },
													properties => \@dcProps ); 
	
	if ( !$datacenterView )
	{
		Common_Utils::WriteMessage("ERROR :: Unable to find datacenter view for datacenter \"$datacenterName\".",3);
		return ( $Common_Utils::FAILURE );
	}
	return ( $Common_Utils::SUCCESS , $datacenterView );
}

#####MapDataCenterGroupName#####
##Description 		:: It maps resource pool group to it's names. At VM level we have only the resource pool group name but not
##						thier names as displayed on vCenter/vSphere.
##input 			:: resourcePoolView.
##Output 			:: Returns Map of resource pool group name and its name displayed at vSphere level on SUCCESS else FAILURE.
#####MapDataCenterGroupName#####
sub MapDataCenterGroupName
{
	my $datacenterView			= shift;
	my %mapDataCenterGroupName 	= ();
	foreach my $dc ( @$datacenterView )
	{
		my $datacenterGroup 						= $dc->vmFolder->value;
		my $datacenterName							= $dc->name;
		Common_Utils::WriteMessage("ResourceGroup = $datacenterGroup and its Name = $datacenterName.",2);
		$mapDataCenterGroupName{$datacenterGroup} 	= $datacenterName;
	}
	return ( $Common_Utils::SUCCESS , \%mapDataCenterGroupName );
}

#####GetDataCenterViewOfHost#####
##Description		:: It finds datacenter view in which the specified host exists.
##Input 			:: Name of the Host.
##Output 			:: Returns Datacenter view on SUCCESS else FAILURE.
#####GetDataCenterViewOfHost#####
sub GetDataCenterViewOfHost
{
	my $hostName 	= shift;
	
	my $dcViews		= GetDataCenterViews();
	foreach my $dc ( @$dcViews )
	{
		my ( $returnCode , $hostViews )	= HostOpts::GetHostViewsInDatacenter( $dc );
		if ( $returnCode ne $Common_Utils::SUCCESS )
		{
			return ( $Common_Utils::FAILURE );
		}
		
		foreach my $host ( @$hostViews )
		{
			if ( $hostName eq $host->name )
			{
				my $datacenterName 		= $dc->name;
				Common_Utils::WriteMessage("Host $hostName is found under datacenter $datacenterName.",2);
				return ( $Common_Utils::SUCCESS , $dc )
			}
		}
	}
	Common_Utils::WriteMessage("ERROR :: Failed to find datacenter view in which $hostName resides.",3);
	return ( $Common_Utils::FAILURE );
}

#####GetVmDataCenterName#####
##Description 		:: Gets view of a specific datacenter on vCenter/vSphere.
##Input 			:: vmView for which the datacenter has to be found.
##Output			:: Returns DataCenterView on SUCCESS else FAILURE.
#####GetVmDataCenterName#####
sub GetVmDataCenterName
{
	my $vmView			= shift;
	my $datacenterName	= "";
	if ( defined $vmView->parent )
	{
		my $parentView 			= $vmView;
		while( $parentView->parent->type !~ /Datacenter/i )
		{	   
			$parentView 		= Vim::get_view( mo_ref => ($parentView->parent), properties => ["parent"] );
	    }
	   $datacenterName			= Vim::get_view( mo_ref => ($parentView->parent), properties => ["name"] )->name;
	}
	elsif( defined $vmView->parentVApp )
	{
		my $parentView			= $vmView;
		$parentView 			= Vim::get_view( mo_ref => ($parentView->parentVApp), properties => ["parent"] );
		while( $parentView->parent->type !~ /Datacenter/i )
		{	   
			$parentView = Vim::get_view( mo_ref => ($parentView->parent), properties => ["parent"] );
	    }
	    $datacenterName 		= Vim::get_view( mo_ref => ($parentView->parent), properties => ["name"] )->name;
	}
	
	if ( $datacenterName eq "" )
	{
		Common_Utils::WriteMessage("ERROR :: Unable to find datacenter name for virtual machine \"$vmView->{name}\".",3);
		return ( $Common_Utils::FAILURE );
	}
	
	return ( $Common_Utils::SUCCESS , $datacenterName );
}

1;