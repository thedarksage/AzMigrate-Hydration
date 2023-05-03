#!/usr/bin/perl

use strict;
use lib($ENV{'ProgramData'}."\\Microsoft Azure Site Recovery\\scripts");
use InstallationPath;
my $lib_path;
BEGIN {$lib_path = InstallationPath::getInstallationPath();}
use lib ("$lib_path/home/svsystems/pm");
use Net::SMTP;
use Mail::Sender;
use Data::Dumper;

my $post_data;
read(STDIN, $post_data, $ENV{'CONTENT_LENGTH'});


print "Content-type: text/plain\n\n<br>";

my @value_pairs = split (/&/,$post_data);
my %form_results = ();

foreach my $pair (@value_pairs) {                 
    my ($key, $value) = split (/=/,$pair);
    $value =~ tr/+/ /;
    #$value =~ s/%([\dA-Fa-f][\dA-Fa-f])/pack ("C", hex ($1))/eg;
    $form_results{$key} = $value;  # store the key in the results hash
}

#print "Mail Server = ".$form_results{"mail_server"};
#print "Mail Addresses = ".$form_results{"mail_addresses"};
#print "Message = ".$form_results{"message"};

my $mail_server = $form_results{"mail_server"};
my $to_email_address = $form_results{"mail_addresses"};
my $from_email_address = $form_results{"from"};
#my $from_email_address = "MailserverCX\@10.0.1.111";
my $message = $form_results{"message"};
my $subject = $form_results{"subject"};
my $smtp = Net::SMTP->new($mail_server);
if($smtp != undef)
{
	my $sender = new Mail::Sender {smtp => "$mail_server", from => "$from_email_address"};
	
	# with no attachments
	if(!$form_results{"post_dir"})
	{
		$sender->MailMsg({
			to => "$to_email_address",
			subject => $subject,
			ctype => "text/html",
			msg => $message
			});		
	}
	else
	{
		$sender->OpenMultipart({
			to => "$to_email_address",
			subject => $subject,
			ctype => "text/html",
			});

		$sender->Body;
		$sender->SendLine( $message );
		my @files = <$form_results{'post_dir'}/*>;
		foreach my $file (@files) {
	   	  $sender->SendFile({file => $file});
		}
		$sender->Close;	
		foreach my $file (@files) {
			unlink($file);
		}
		my $ret = rmdir($form_results{'post_dir'});
		if($ret)
		{
			#print "Removed the FOLDER\n";			
		}
	}
	$smtp->quit;
}
else
{
	#print "SMTP object did not get created<br>";
}
