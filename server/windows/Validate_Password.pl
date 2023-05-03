#!/usr/bin/perl

#  Script to validate admin password at the time of installation
# Invoke script: perl cx_password.pl "<Password>"
#
# Returns 0 on success
# 1 on Password validation failed
# 2 on DB update failed
# 3 on Unknown error

if (! defined $ARGV[0])
{
 # Password is not supplied
        print 1; 
        exit;
}

$password = $ARGV[0];
if($password and $password =~ /^(?=^.*[a-z])(?=^.*[A-Z])(?=^.*[0-9])(\S{8,16})(?<!\s)$/)
{
 # Success
 print 0; 
 exit(0);
}
else
{
 # Password must contain at least one number, one lower-case, one Upper-case and it should be 8 to 16 characters
 print 1; 
 exit(1);
}