=head
	CommonUtils for Cloud project.
	1. Debug Writer.
	2. CommonVariables.
	3. Generic functions which are not specific to any.
=cut

package CommonUtils;

use strict;
use warnings;

if( $^O =~ /MSWin32/i )
{
	require Win32::Registry;
}

my $SUCCESS 	= 0;
my $FAILURE 	= 1;


#####FindCXIP#####
##Description 		:: Finds the CX IP and Its Port Number.
##Input 			:: None.
##Output 			:: Returns SUCCESS and CxIp:PortNumber else FAILURE.
#####FindCXIP#####
sub FindCXIP
{
	my $os_type 			= $^O;
	my $am_I_a_linux_box 	= 0;
	my $CxIp 				= "";
	my $CxPortNumber		= "";
	
	syswrite STDOUT , "THE OS TYPE of vContinuum machine = $os_type.";

	if(0 == $am_I_a_linux_box)
	{
		my %RegType = (
					0 => 'REG_0',
					1 => 'REG_SZ',
					2 => 'REG_EXPAND_SZ',
					3 => 'REG_BINARY',
					4 => 'REG_DWORD',
					5 => 'REG_DWORD_BIG_ENDIAN',
					6 => 'REG_LINK',
					7 => 'REG_MULTI_SZ',
					8 => 'REG_RESOURCE_LIST',
					9 => 'REG_FULL_RESOURCE_DESCRIPTION',
					10 => 'REG_RESSOURCE_REQUIREMENT_MAP');

		my $Register = "SOFTWARE\\SV Systems";
		my $RegType;
		my $RegValue;
		my $RegKey;
		my $value;
		my %vals;
		my @key_list;
		my $hkey;

		if( ! $::HKEY_LOCAL_MACHINE->Open( $Register, $hkey ) )
		{
			syswrite STDOUT , "ERROR :: Cannot find the Registry Key $Register..Cannot find the CX Ip Address.";
			return ( $FAILURE );
		}

		$hkey->GetValues(\%vals);

		foreach $value (keys %vals)
		{
			$RegType 	= $vals{$value}->[1];
			$RegValue 	= $vals{$value}->[2];
			$RegKey 	= $vals{$value}->[0];
			$RegKey 	= 'Default' if ($RegKey eq '');
			
			if($RegKey eq "ServerName")
			{
				if(1 != $RegType)
				{
					syswrite STDOUT , "ERROR : Registry Key \"ServerName\" is not of type REG_SZ.Considering this as an ERROR.";
					return ( $FAILURE );
				}
				$CxIp = $RegValue;
			}
						
			if($RegKey eq "ServerHttpPort")
			{
				if(4 != $RegType)
				{
					syswrite STDOUT , "ERROR : Registry Key \"ServerHttpPort\" is not of type REG_DWORD.Considering this as an ERROR.";
					return ( $FAILURE );
				}
				$CxPortNumber = $RegValue;				
			}
		}
		$hkey->Close();
		syswrite STDOUT ,"CX Ip and Port number: $CxIp:$CxPortNumber .";
	}
	my $CxIpPortNumber = "$CxIp:$CxPortNumber";
	return ( $SUCCESS , $CxIpPortNumber );
}



1;