#!/usr/bin/perl

my $fval = $ARGV[0];

#print $fval."\n";

#On false
my $exit_code = 1;

#Atleast one digit
if($fval =~ /\d/) 
{
	#Atleast one character
	if($fval =~ /[a-zA-Z]/)
	{
		#Atleast one allowed special character
		if($fval =~ /[_!@#\$\%]/) 
		{
			#No spaces
			if($fval !~ /[ ]/)
			{
				#Length minimum 8 and maximum 16 characters
				if ($fval !~ /^[a-zA-Z0-9_!@#\$\%]{8,16}$/)
				{
  					#print "Fail.\n";
  					
				}
				else
				{
					#print "Success.\n";
					$exit_code = 0;
				}
							
			}
		}
	}
}
#print $exit_code;
exit $exit_code;