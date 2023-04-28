=head
	ResourcePoolOpts
	1. Maps ResourcePool groups to their names.
=cut

package ResourcePoolOpts;

use strict;
use warnings;
use Common_Utils;

my @resourcePoolProps = ("name","parent","owner");

#####GetResourcePoolViews#####
##Description 		:: Gets resourcePool view of vCenter/vSphere.
##Input 			:: None.
##Output 			:: Returns resource pool views on SUCCESS else FAILURE.
#####GetResourcePoolViews#####
sub GetResourcePoolViews
{
	my $resourcePoolViews	= Vim::find_entity_views( view_type => 'ResourcePool' );
	
	unless ( @$resourcePoolViews )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to get Resource pool views from vCenter/vSphere.",3);
		return ( $Common_Utils::FAILURE , $resourcePoolViews );
	}
	return ( $Common_Utils::SUCCESS , $resourcePoolViews );
}

#####GetResourcePoolViewsByProps#####
##Description 		:: Gets resourcePool view of vCenter/vSphere.
##Input 			:: None.
##Output 			:: Returns resource pool views on SUCCESS else FAILURE.
#####GetResourcePoolViewsByProps#####
sub GetResourcePoolViewsByProps
{
	my $resourcePoolViews;
	eval
	{
		$resourcePoolViews	= Vim::find_entity_views( view_type => 'ResourcePool', properties => \@resourcePoolProps );
	};
	if( $@ )
	{
		Common_Utils::WriteMessage("Failed to get Resource pool views by poperties from vCenter/vSphere : \n $@.",3);
		return ( $Common_Utils::FAILURE , $resourcePoolViews );
	}
	unless ( @$resourcePoolViews )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to get Resource pool views from vCenter/vSphere.",3);
		return ( $Common_Utils::FAILURE , $resourcePoolViews );
	}
	return ( $Common_Utils::SUCCESS , $resourcePoolViews );
}

#####MapResourcePool#####
##Description 		:: It maps resource pool group to it's names. At VM level we have only the resource pool group name but not
##						thier names as displayed on vCenter/vSphere.
##input 			:: resourcePoolView.
##Output 			:: Returns Map of resource pool group name and its name displayed at vSphere level on SUCCESS else FAILURE.
#####MapResourcePool#####
sub MapResourcePool
{
	my $resourcePoolView		= shift;
	my %mapResourceGroupName 	= ();
	foreach my $rp ( @$resourcePoolView )
	{
		if ( ! defined $rp->{mo_ref}->{value} ) 
		{
			next;
		}
		my $resourceGroup 						= $rp->{mo_ref}->{value};
		my $resourcePoolName					= $rp->name;
		Common_Utils::WriteMessage("ResourceGroup = $resourceGroup and its Name = $resourcePoolName.",2);
		$mapResourceGroupName{$resourceGroup} 	= $resourcePoolName; 
	}
	return ( $Common_Utils::SUCCESS , \%mapResourceGroupName );
}

#####FindResourcePool#####
##Description 		:: Finds resource pool reference using resource pool group ID.
##Input 			:: Resource Pool Group ID.
##Output 			:: Returns SUCCESS else FAILURE.
#####FindResourcePool#####
sub FindResourcePool
{
	my $resourceGroupId	= shift;
	
	my ( $returnCode , $resPoolViews )	= GetResourcePoolViewsByProps();
	if ( $returnCode ne $Common_Utils::SUCCESS )
	{
		return ( $Common_Utils::FAILURE );
	}	
	
	foreach my $resPool ( @$resPoolViews )
	{
		if ( $resourceGroupId eq $resPool->{mo_ref}->{value} )
		{
			return ( $Common_Utils::SUCCESS , $resPool );
		}
	} 
	return ( $Common_Utils::FAILURE );
}

#####GetResourcePoolViewInDC#####
##Description 		:: Finds the resourcepool view under a datacenter.
##Input 			:: ResourcePoolName and Datacenter View.
##Output 			:: Returns SUCCESS on Finding else FAILURE.
#####GetResourcePoolViewInDC#####
sub GetResourcePoolViewInDC
{
	my $resourcePoolName	= shift;
	my $datacenterView		= shift;
	my $resourcePoolView 	= Vim::find_entity_view( view_type => 'ResourcePool', begin_entity => $datacenterView , 
													properties => \@resourcePoolProps, filter => {'name'=> $resourcePoolName } );
	if ( !$resourcePoolView )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to find Resource Pool View.",3);
		return ( $Common_Utils::FAILURE );
	}
	return ( $Common_Utils::SUCCESS , $resourcePoolView );
}

#####FindResourcePoolInfo#####
##Description		:: Finds Resource Pool information like pool name , group name, type of pool and its type and also maps to its host.
##Input 			:: ResourcePool views and HostViews..
##Output 			:: Returns a list of ResourcePoolInfo on SUCCESS else FAILURE.
#####FindResourcePoolInfo#####
sub FindResourcePoolInfo
{
	my $resourcePoolViews	= shift;
	my $hostViews 			= shift;
	my @resourcePoolInfo	= ();
	
	my %mapHostandParent	= ();
	foreach my $hostView ( @$hostViews )
	{
		$mapHostandParent{ $hostView->name }	= $hostView->parent->value;
	}
	
	my %vAppPoolMap			= ();
	foreach my $resourcePool( @$resourcePoolViews )
	{
		if ( (! defined ( $resourcePool->{mo_ref}) ) || ( "virtualapp" ne lc( $resourcePool->{mo_ref}->type ) ) )
		{
			next;
		}
		$vAppPoolMap{ $resourcePool->{mo_ref}->value } = $resourcePool->{mo_ref}->type;
	}
	
	foreach my $resourcePool ( @$resourcePoolViews )
	{
		my %resourcePoolInfo		 = ();
		$resourcePoolInfo{groupName} = $resourcePool->{mo_ref}->value;
		$resourcePoolInfo{name}		 = $resourcePool->name;
		$resourcePoolInfo{owner}	 = $resourcePool->owner->value;
		$resourcePoolInfo{ownerType} = $resourcePool->owner->type;
	 	$resourcePoolInfo{resPoolType}= $resourcePool->{mo_ref}->type;
	 	
	 	my $parentPool 				 = $resourcePool;
	 	while ( 'resourcepool' eq lc ( $parentPool->parent->type ) )
	 	{
	 		$parentPool				 = Vim::get_view( mo_ref => $parentPool->parent );
	 	}
	 	if ( ( exists $vAppPoolMap{$parentPool->{mo_ref}->value} ) || ( exists $vAppPoolMap{$parentPool->parent->value} ) )
	 	{
	 		next;
	 	}
	 	
	 	foreach my $host ( sort keys %mapHostandParent )
		{
			if ( $mapHostandParent{$host} eq $resourcePoolInfo{owner}  )
			{
				$resourcePoolInfo{inHost}		= $host;
				Common_Utils::WriteMessage("ResourcePool name = $resourcePoolInfo{name}, groupName = $resourcePoolInfo{groupName}, pool type = $resourcePoolInfo{resPoolType}, owner = $resourcePoolInfo{owner}, owner type = $resourcePoolInfo{ownerType}, host = $resourcePoolInfo{inHost}.",2);
				if ( 'clustercomputeresource' eq lc( $resourcePoolInfo{ownerType} ) )
				{
					my %clustResourcePoolInfo	= %resourcePoolInfo;
					push @resourcePoolInfo , \%clustResourcePoolInfo;
					next;
				}
				push @resourcePoolInfo , \%resourcePoolInfo;
				last;
			}
		}
	}
	
	Common_Utils::WriteMessage("Successfully discovered resource pool information.",2);
	return ( $Common_Utils::SUCCESS , \@resourcePoolInfo );
}

#####FindResourcePoolOfVm#####
##Description		:: Finds the resourePool of a VM.
##Input 			:: It sends the vmView and ResourcePool views.
##Output 			:: returns SUCCESS and ResourcePool Name else FAILURE.	
#####FindResourcePoolOfVm#####
sub FindResourcePoolOfVm
{
	my $vmView 				= shift;
	my $resourcePoolViews	= shift;
	my $resourcePoolName	= "";
	my( $returnCode , $mapResourceGroupName ) = MapResourcePool( $resourcePoolViews ); 
	if ( $Common_Utils::SUCCESS  != $returnCode )
	{
		return ( $Common_Utils::FAILURE );
	}
	
	if ( ! exists  $$mapResourceGroupName{ $vmView->resourcePool->value } )
	{
		return ( $Common_Utils::FAILURE );
	}
	
	$resourcePoolName 	= $$mapResourceGroupName{ $vmView->resourcePool->value };
	return ( $Common_Utils::SUCCESS , $resourcePoolName );	
}

#####MoveVmToResourcePool#####
##Description		:: Moves the Vm To ResourcePool.
##Input 			:: vmView and ResourcePool view.
##Output 			:: not return anything.	
#####MoveVmToResourcePool#####
sub MoveVmToResourcePool
{
	my $resourcePoolView	= shift;
	my $vmView 				= shift;
	eval
	{	
		$resourcePoolView->MoveIntoResourcePool(list => [$vmView]);
	};	
	if( $@ )
   	{
   		Common_Utils::WriteMessage("Unable to move virtual machine to resourcepool, due to : $@ .",2);
   		return ( $Common_Utils::FAILURE );
	}
	return ( $Common_Utils::SUCCESS );
}
1;	