=head
	ComputeResourceOpts.pm
	1. used to find computeResourceViews.
=cut

package ComputeResourceOpts;

use strict;
use warnings;
use Common_Utils;


#####GetComputeResourceViews#####
##Description		:: Gets the Compute Resource Views of either vCenter/vSphere.
##Input 			:: None.
##Output 			:: Returns view of Compute Resource on SUCCESS else FAILURE.
#####GetComputeResourceViews#####
sub GetComputeResourceViews
{
	my $computeResourceViews 	= Vim::find_entity_views( view_type => 'ComputeResource' );
	
	unless ( @$computeResourceViews )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to get 'ComputeResource' views from vCenter/vSphere.",3);
		return ( $Common_Utils::FAILURE , $computeResourceViews  );
	}
	
	return ( $Common_Utils::SUCCESS , $computeResourceViews );
}

#####GetComputeResourceViewsByProps#####
##Description		:: Gets the Compute Resource Views of either vCenter/vSphere.
##Input 			:: None.
##Output 			:: Returns view of Compute Resource on SUCCESS else FAILURE.
#####GetComputeResourceViewsByProps#####
sub GetComputeResourceViewsByProps
{
	my $computeResourceViews;
	eval
	{
		my @properties = ("name","host","environmentBrowser");
		$computeResourceViews 	= Vim::find_entity_views( view_type => 'ComputeResource', properties => \@properties );
	};
	if( $@ )
	{
		Common_Utils::WriteMessage("Failed to get ComputeResource views by poperties from vCenter/vSphere : \n $@.",3);
		return ( $Common_Utils::FAILURE , $computeResourceViews  );
	}
	
	unless ( @$computeResourceViews )
	{
		Common_Utils::WriteMessage("ERROR :: Failed to get 'ComputeResource' views from vCenter/vSphere.",3);
		return ( $Common_Utils::FAILURE , $computeResourceViews  );
	}
	
	return ( $Common_Utils::SUCCESS , $computeResourceViews );
}

1;