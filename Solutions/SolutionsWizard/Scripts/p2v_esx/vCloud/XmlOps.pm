=head
	XML operations
	1. Parses XML response sent.
	2. Constructs XML.
	3. Reconstructs XML file with adding\deleting information.
=cut

package XmlOps;

use strict;
use warnings;

use CommonUtils;
my $SUCCESS = 0;
my $FAILURE = 1;

#####GetPlanNodes#####
##Description 	:: Gets the Plan Node information from the XML file. XML may contain multiple Plan information.
##Input 		:: XML file.
##Output 		:: It returns reference to plan nodes on SUCCESS else FAILURE.
#####GetPlanNodes#####
sub GetPlanNodes
{
	my $file		= shift;
	if ( ! -e $file  )
	{
		syswrite STDOUT ,"ERROR :: Unable to find file \"$file\".";
		syswrite STDOUT ,"ERROR :: Please check whether above file exists or not.";
		return $FAILURE;
	}
	
	my $parser 		= XML::LibXML->new();
	my $tree		= "";
	eval
	{
		$tree 		= $parser->parse_file($file);
	};
	if ( $@ )
	{
		syswrite STDOUT , "ERROR :: $@.";
		return ( $FAILURE );
	}
	
	my $root 		= $tree->getDocumentElement;
	return ( $SUCCESS , $root );
}


1;
