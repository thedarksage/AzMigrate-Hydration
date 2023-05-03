package SystemJobs;

use strict;
use lib($ENV{'ProgramData'}."\\Microsoft Azure Site Recovery\\scripts");
use InstallationPath;
my $lib_path;
BEGIN {$lib_path = InstallationPath::getInstallationPath();}
use lib ("$lib_path/home/svsystems/pm");

use Common::Log;
use Config;
use Data::Dumper;
use Utilities;

my $logging_obj = new Common::Log();
$logging_obj->set_event_name("SYSTEM_JOB");
my $cs_installation_path = Utilities::get_cs_installation_path();
sub get_content
{
	my $file = "/home/svsystems/var/deploy_config_file.txt";
	
	if(!-f $file || ! -e $file)
	{
		$logging_obj->log("INFO","get_content : File doesn't exist\n");
		return;
	}
	my (%service_hash, %conf_hash);
	
	open(FH,$file) or $logging_obj->log("EXCEPTION","File cannot be opened for reading");

	my @lines = <FH>;
	close(FH);
	foreach my $line (@lines)	
	{
		chomp $line;
		my($key, $value) = split(/=/,$line);
		$conf_hash{$key} = $value;
	}

    # Create the RX Config File for CX registering with RX
    if($conf_hash{'COMPONENT'} && $conf_hash{'COMPONENT'} eq "CX")
    {
        my $rx_conf_file = "/home/svsystems/var/cx_register_conf_file.txt";
        if(-f $rx_conf_file)
        {
            unlink($rx_conf_file);
        }
        if($conf_hash{'RX_IP_ADDRESS'} && $conf_hash{'RX_PORT'})
        {
            open(RX_FH,">$rx_conf_file");
            print RX_FH $conf_hash{'RX_IP_ADDRESS'}.":".$conf_hash{'RX_PORT'}.":".$conf_hash{'RX_PROTOCOL'}.":".$conf_hash{'CUSTOMER_ACCOUNT_ID'};
            close(RX_FH);
        }
    }
	return \%conf_hash;
}

sub updateConfig
{
	my $deploy_conf_file = "/home/svsystems/var/deploy_config_file.txt";
    my $rx_conf_file = "/home/svsystems/var/cx_register_conf_file.txt";
    if((! -f $deploy_conf_file) && ( -f $rx_conf_file))
    {
		if(&EventManager::registerToRx())
		{
        	unlink($rx_conf_file);
		}
		return;
    }
    
	my ($conf_hash) = &get_content();
	
    if(!$conf_hash)
    {
        $logging_obj->log("INFO","updateConfig : No Config Params\n");
		return;
    }
	
	$logging_obj->log("INFO","updateConfig : Entered updateConfig\n");
	$logging_obj->log("INFO","updateConfig : ".Dumper($conf_hash));
        
    my $component = $conf_hash->{'COMPONENT'};
    if(!$component || ($component ne "CX" && $component ne "MT"))
    {
        $logging_obj->log("EXCEPTION","updateConfig : Invalid component type. Component = '".$component."'");
        return; 
    }
	
    my $command = &getConfigCommand($component,$conf_hash);
    if(!$command)
    {
        $logging_obj->log("EXCEPTION","updateConfig : Couldn't get the command : $command");
        return; 
    }	
	
	$logging_obj->log("INFO","updateConfig : Executing the command : $command\n");
	my $output = `$command`;
	if($? == 0)
	{
		$logging_obj->log("INFO","updateConfig : Successfully executed the command\n");
		unlink($deploy_conf_file);
	}
    else
    {
        $logging_obj->log("EXCEPTION","updateConfig : Command Failed. Command : ".$command);
    }
	$logging_obj->log("INFO","updateConfig : Exiting\n");
}

sub getConfigCommand
{
    my ($component,$conf_hash) = @_;
    
    my ($install_dir,$exe_path,$command);
    
    if ($Config{osname} !~ /linux/i)
	{
        if($component eq "MT")
        {
            use if $^O eq 'MSWin32', 'Win32API::Registry';
            my $Register = "SOFTWARE\\SV Systems\\VxAgent";
            
            my (%vals, $hkey);
            if(!$::HKEY_LOCAL_MACHINE->Open( $Register, $hkey))
            {
                $logging_obj->log("EXCEPTION","getConfigCommand : Cannot find the Registry Key $Register");
                return;
            }
            $hkey->GetValues(\%vals);
            $install_dir = $vals{'InstallDirectory'}[2];
            #$install_dir =~ s/\\/\\\\/g;
            $exe_path = $install_dir."\\Cloud\\PostDeployConfigMT.exe";
            if(!$exe_path || (! -f $exe_path))
            {
                $logging_obj->log("EXCEPTION","getConfigCommand : Cannot find the executable at \"$exe_path\"");
                return;
            }
            $command = '"'.$exe_path.'"'." /verysilent /suppressmsgboxes /i ".$conf_hash->{'CS_IP_ADDRESS'}." /p ".$conf_hash->{'PORT'}." /a ".$conf_hash->{'IP_ADDRESS'}." /g ".$conf_hash->{'HOST_GUID'}." /h ".$conf_hash->{'HOSTNAME'};
        }
        elsif($component eq "CX")
        {
            $install_dir = $cs_installation_path."\\home\\svsystems";
            $exe_path = $install_dir."\\Cloud\\PostDeployConfigCX.exe";
            if(!$exe_path || (! -f $exe_path))
            {
                $logging_obj->log("EXCEPTION","getConfigCommand : Cannot find the executable at \"$exe_path\"");
                return;
            }
            $command = '"'.$exe_path.'"'." /verysilent /suppressmsgboxes /i ".$conf_hash->{'IP_ADDRESS'}." /p ".$conf_hash->{'PORT'}." /g ".$conf_hash->{'HOST_GUID'}." /h ".$conf_hash->{'HOSTNAME'};
        }
    }
    elsif(Common::Functions::isLinux())
    {
        if($component eq "MT")
        {
            my $file_op = `grep -w 'INSTALLATION_DIR' /usr/local/.vx_version`;
            $install_dir = $1 if ($file_op =~ /=(.*)/);            
            chomp($install_dir);            
            $exe_path = $install_dir."/Cloud/PostDeployConfigMT.sh";
            if(!$exe_path || (! -f $exe_path))
            {
                $logging_obj->log("EXCEPTION","getConfigCommand : Cannot find the executable at \"$exe_path\"");
                return;
            }
            $command = $exe_path." -i ".$conf_hash->{'CS_IP_ADDRESS'}." -p ".$conf_hash->{'PORT'}." -a ".$conf_hash->{'IP_ADDRESS'}." -g ".$conf_hash->{'HOST_GUID'}." -H ".$conf_hash->{'HOSTNAME'};
        }
        elsif($component eq "CX")
        {
            $install_dir = "/home/svsystems";
            $exe_path = $install_dir."/Cloud/PostDeployConfigCX.sh";
            if(!$exe_path || (! -f $exe_path))
            {
                $logging_obj->log("EXCEPTION","getConfigCommand : Cannot find the executable at \"$exe_path\"");
                return;
            }
            $command = $exe_path." -i ".$conf_hash->{'IP_ADDRESS'}." -p ".$conf_hash->{'PORT'}." -g ".$conf_hash->{'HOST_GUID'}." -H ".$conf_hash->{'HOSTNAME'};
        }
    }    
    return $command;
}

1;
