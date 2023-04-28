#!/usr/bin/perl -w
use CGI qw(:standard);
use strict;
#use File::Copy::Recursive qw(fcopy rcopy dircopy fmove rmove dirmove);
use File::Path qw( mkpath );
use File::Basename;
use HTTP::Response;
use HTTP::Status;
use File::Spec;
#http://search.cpan.org/~dmuey/File-Copy-Recursive-0.16/Recursive.pm#pathmk()
#http://tomacorp.com/perl/filemanip.html
#sub value_sanity_check($);
my $CHUNK_DATA_TO_SEND =  65536; #64KB
my $action_key = 'action';
my $message_key = 'message';
my $property_key = 'property';
my $size_key = 'datasize';
my $sort_key = 'sort';
my $destination_key = 'dest';
my $append_key = 'append';
my $SEND_ACTION = 0;
my $RECEIVE_ACTION = 1;
my $LIST_ACTION = 2;
my $REMOVE_ACTION = 3;
my $RENAME_ACTION = 4;
my @actions = qw(send_data receive_data list_data remove_data rename_data);

my %form;
my $action_val;
my $message_val;
my $property_val;
my $size_val;
my $sort_val;
my $destination_val;
my $append_val;
my $retval=0;
my $DEBUGFLAG=1;
my $DEBUGPATH="/tmp/inmage_handler_debug.txt";


##
#function:Does Sanity Check for the value
#Date: DEC 15,2005
##
sub value_sanity_check {
    my $value = $_[0];
    #print DBUG "doing sanity check for $value\n";
    if (defined $value && length($value) > 0) {
        return 1;
    }
    else {
        return 0;
    }
}
##
#function:File Spec To Regular Expression
#Date: DEC 15,2005
##
sub filespec_to_regexp
{
    local $_ = $_[0];
    my $out = "";

    while (length($_))
    {
	# process the leading part of the input

	if (s/^([^\.\*]+)//)
	{
	    # copy non-wildcard leading characters
	    $out .= $1;
	}
	elsif (s/^\.\.\.//)
	{
	    # convert ... to .*
	    $out .= ".*";
	}
	elsif (s/^\*//)
	{
	    # convert * to [^/]*
	    $out .= "[^/]*";
	}
	elsif (s/^\.//)
	{
	    # convert . to \.
	    $out .= "\\.";
	}
	else
	{
	    die "should never get here: $_\n";
	}
    }

    return $out;
}


##
#function:Prints List
#Date: DEC 15,2005
##
sub print_list {
    my $list_ref = $_[0];
    my $file_name;
    foreach $file_name (@$list_ref) {
        debug("$file_name\n");
    }
}
##
#function:Lists Files in a directory
#Date: DEC 15,2005
##
sub list_files_in_directory {
    my $directory = $_[0];
    my $filespec = $_[1];
    my $out_array = $_[2];
    my $path;
    my $file;
    debug( "File Spec = $filespec\n");
	  if (opendir DIR,$directory) {
		    while ($file = readdir(DIR)) {
            debug("File name = $file");
            if(!(-d $file)) {
                if($file =~ /$filespec/) {
                    $path = File::Spec->catfile($directory,$file);
                    push(@$out_array, $path);
                    debug(" -- Match\n");
                }
                else{
                    debug(" -- No Match\n");
                }
            }
            else {
                debug( " -- Not a file\n");
            }
        }
        closedir(DIR);
    }
    else {
        return -1;
    }
	  return 0;
}

##
#function:Sends Data
#Date: DEC 15,2005
##
sub execute_send_data_action {
    my $dirname;
    my $filename;
    my $type;
    debug("execute send data action called\n");
    $message_val = $_[0];
    $property_val = $_[1];
    $append_val = $_[2];
    if(!value_sanity_check($message_val) || !value_sanity_check($property_val) || !value_sanity_check($append_val)) {
        return -1;
    }
    ($filename,$dirname,$type) = fileparse($property_val);
   # $dirname = dirname($property_val);
    debug("File = $filename, dirname = $dirname\n");
    unless (-d $dirname) {
          debug("Going to execute mkpath for $dirname\n");
          unless (mkpath( [ $dirname ], 0, 0755)) {
            debug("Unable to make directory $dirname $!");
              return -1;
          }
    }
    if($append_val) {
        open(WRITEHANDLE, ">>$property_val") || (debug("Could not open file $!\n" && return -1));
    }
    else {
        open(WRITEHANDLE, ">$property_val") || (debug("Could not open file $!\n" && return -1));
    }
    binmode(WRITEHANDLE);
    print WRITEHANDLE $message_val;
    binmode(STDOUT);
    print STDOUT "Content-type: application/binary\n\n";
    print STDOUT "0";

    return 0;
}
##
#function:Execute Data Action
#Date: DEC 15,2005
##
sub execute_get_data_action {
    my $data;
    my $response;
    my $actual_sent;
    debug("execute get data action called\n");
    $property_val = $_[0];
    $size_val = $_[1];
    if(!value_sanity_check($property_val)) {
        return -1;
    }
    if(defined($size_val) && $size_val <= 0) {
        debug("size val is defined but its size is 0\n");
        return -1;
    }
    debug("size val is OK\n");
    open(READHANDLE, "<$property_val") || (debug("Could not open file $!\n" && return -1));
    binmode(READHANDLE);
    binmode(STDOUT);
    #print STDOUT "HTTP/1.1 200 OK\n";
    print STDOUT "Content-type: application/binary\n\n";
    if(!defined($size_val)) {
        $size_val = 2147483647; #MAX_INT Value
    }
    while(($actual_sent = read (READHANDLE, $data, $CHUNK_DATA_TO_SEND)) > 0 && $size_val > 0 ) {  
        #debug($data);
        print STDOUT $data;
        $size_val -= $actual_sent;
    }
    close(READHANDLE);
    debug("Read Handle closed\n");
    return 0;
}
##
#function:List Data Action
#Date: DEC 15,2005
##
sub execute_list_data_action {
    $property_val = $_[0];
    $sort_val = $_[1];
    my $dir_name;
    my $file_name;
    my $type;
    my $return_code;
    my @file_list;
    my %mtime_map;
    my $mtime;
    my $response;
    my $array_size;
    ($file_name,$dir_name,$type) = fileparse($property_val);
    debug("file_name before regexp conversion= $file_name\n");
    $file_name = filespec_to_regexp($file_name);
    debug("file_name after regexp conversion= $file_name\n");
    $return_code = list_files_in_directory($dir_name,$file_name,\@file_list);
    if($return_code == -1) {
        return -1;
    }
    $array_size = scalar @file_list;
    debug( "Size of returned array from directory $dir_name = $array_size\n");
    foreach $file_name (@file_list) {
        $mtime = (stat($file_name))[9];
        debug( "$mtime --- $file_name\n");
        $mtime_map{$file_name} = $mtime;
    }
    #debug("Size of hash map = keys(%mtime_map)\n");
    if($sort_val) {
        # sort the files by their cached modified times, descending order
        @file_list = sort { $mtime_map{$b} <=> $mtime_map{$a} } keys %mtime_map;
        #debug("Size of returned array after sorting = @file_list\n");
        debug("sorted list =\n");
        print_list(\@file_list);
    }
    #now respond with the list , format - 'filename mtime'
    binmode(STDOUT);
    #print STDOUT "HTTP/1.1 200 OK\n";
    #print STDOUT status_message(RC_OK), "\n";
    print STDOUT "Content-type: application/binary\n\n";
    foreach $file_name (@file_list) {
        print STDOUT "$file_name $mtime_map{$file_name}\n";
        debug("$file_name $mtime_map{$file_name}\n");
    }
    return 0;
}

##
#function:Execute Remove Data
#Date: DEC 15,2005
##
sub execute_remove_data_action {
    $property_val = $_[0];
    if(!value_sanity_check($property_val)) {
        return -1;
    }
    if (unlink($property_val) == 0) {
        debug("File $property_val was not deleted.");
        return -1;
    }
    else {
       debug("File $property_val deleted.");
    }
    binmode(STDOUT);
    print STDOUT "Content-type: application/binary\n\n";
    print STDOUT "0";
    return 0;
}


##
#function:Execute Rename data
#Date: DEC 15,2005
##
sub execute_rename_data_action {
    $property_val = $_[0];
    $destination_val = $_[1];
    if(!value_sanity_check($property_val)) {
        return -1;
    }
    if(!value_sanity_check($destination_val)) {
        return -1;
    }
    if (rename($property_val,$destination_val) == 0) {
        debug("File $property_val was not renamed.");
        return -1;
    }
    else {
        debug("File $property_val renamed.");
    }
    binmode(STDOUT);
    print STDOUT "Content-type: application/binary\n\n";
    print STDOUT "0";
    return 0;
}

sub debug($)
{
		if($DEBUGFLAG > 0)
		{
			open(DBUG, ">>$DEBUGPATH") or (print "COULD NOT OPEN DEBUG FILE" && die "could not open file");
			my ( $msg ) = @_;
			print DBUG $msg;
			close(DBUG);
		}
}


#############################################################################
# 	Main:Entry Point InMage-Handler   			   					    								#
#		Date:DEC 15,2005, InMage Systems 			    															#
#   Ver 1.1								           	                       			  				#
#############################################################################
MAIN:

debug("Started\n");

#execute_list_data_action("g:\\abhai\\docs\\.doc",1);

#foreach my $p (param()) {
#    $form{$p} = param($p);
#    debug( "$p = $form{$p}\n");
#}
$action_val = param($action_key);
#debug("action val = $action_val\n");
if(!value_sanity_check($action_val)) {
exit 1;
}
#debug( "action = $action_val\n");

if($action_val eq $actions[$SEND_ACTION]) {
    $message_val = param($message_key);
    $property_val = param($property_key);
    $append_val = param($append_key);
    $retval = execute_send_data_action($message_val, $property_val,$append_val);
}
elsif($action_val eq $actions[$RECEIVE_ACTION]) {
    $size_val = param($size_key);
    $property_val = param($property_key);
    $retval = execute_get_data_action($property_val, $size_val);
}
elsif($action_val eq $actions[$LIST_ACTION]) {
    $property_val = param($property_key);
    $sort_val = param($sort_key);
    $retval = execute_list_data_action($property_val,$sort_val);
}
elsif($action_val eq $actions[$REMOVE_ACTION]) {
    $property_val = param($property_key);
    $retval = execute_remove_data_action($property_val);
}
elsif($action_val eq $actions[$RENAME_ACTION]) {
    $property_val = param($property_key);
    $destination_val = param($destination_key);
    $retval = execute_rename_data_action($property_val,$destination_val);
}
if($retval != 0) {
    binmode(STDOUT);
    print STDOUT "Content-type: application/binary\n\n";
    print STDOUT "1";
}

exit(0);
