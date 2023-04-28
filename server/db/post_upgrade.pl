#!usr/bin/perl
use lib("/home/svsystems/pm");
use Utilities;

my $AMETHYST_VARS;
BEGIN 
{
  $AMETHYST_VARS = Utilities::get_ametheyst_conf_parameters();
}

my $license_db_update_cmd ;
if (Utilities::isWindows())
{
		$license_db_update_cmd = "c:\\php5nts\\php.exe ".$AMETHYST_VARS->{'WEB_ROOT'}."\\ui\\license_update_db.php";
}
else
{
	if (-e "/usr/bin/php")
	{
		
		$license_db_update_cmd =  "php ".$AMETHYST_VARS->{'WEB_ROOT'}."/ui/license_update_db.php";
	}
	elsif(-e "/usr/bin/php5")
	{
		
		$license_db_update_cmd =  "php5 ".$AMETHYST_VARS->{'WEB_ROOT'}."/ui/license_update_db.php";				
	}
} 

my $data = `$license_db_update_cmd`;