#$Header: /src/server/tm/pm/Common/DB/Connection.pm,v 1.9.8.2 2018/06/26 15:00:12 prgoyal Exp $
#============================================================================= 
# FILENAME
#   Connection.pm 
#
# DESCRIPTION
#    This perl module is responsible for Db connection & sql execution.
#    
# HISTORY
#     19 March 2009  Pawan    Created.
#
#============================================================================
#                 Copyright (c) InMage Systems                    
#============================================================================

package Common::DB::Connection;

use strict;
use lib($ENV{'ProgramData'}."\\Microsoft Azure Site Recovery\\scripts");
use InstallationPath;
my $lib_path;
BEGIN {$lib_path = InstallationPath::getInstallationPath();}
use lib ("$lib_path/home/svsystems/pm");
use Utilities;
use DBI;
use Common::Log;
use Data::Dumper;
use tmanager;
use Common::Constants;

my $TELEMETRY = Common::Constants::LOG_TO_TELEMETRY;

#
# Description: Constructor used to initialize the object .
#
# Input : None
# Return Value : Returns the object referance to the package 
#
sub new
{
    my ($class) = shift; 
	my $args = shift; 
    my $logging_obj = new Common::Log();
    my $self = {};	
	my $dsn;
    my $amethyst_vars = Utilities::get_ametheyst_conf_parameters();
    my $connection_type = $amethyst_vars->{'DB_CONNECTION_TYPE'};
    my $max_retry_limit = $amethyst_vars->{'MAX_RETRY_LIMIT'};
    $self->{'query_log'} = $amethyst_vars->{'QUERY_LOG'}; 
    $self->{'db_driver'} = $amethyst_vars->{'DB_TYPE'};	
	$self->{'db_user'} = $amethyst_vars->{'DB_USER'};
    $self->{'dbh'} = undef;
	my $retry_counter = 1;
    my $db_connect_status = 'FALSE';
	$self->{'cs_installation_directory'} = Utilities::get_cs_installation_path();
	
	if( $args ne undef)
	{		
		if($args->{'DB_PASSWD'} ne undef && $args->{'DB_NAME'} ne undef && $args->{'DB_HOST'} ne undef)
		{
			$self->{'db_name'} = $args->{'DB_NAME'};
			$self->{'db_host'} = $args->{'DB_HOST'};
			$self->{'db_passwd'} = $args->{'DB_PASSWD'};
			$dsn = "DBI:$self->{'db_driver'}:database=$self->{'db_name'};host=$self->{'db_host'}";
		}
	}
	else
	{
		$self->{'db_name'} = $amethyst_vars->{'DB_NAME'};
		$self->{'db_host'} = $amethyst_vars->{'DB_HOST'};
		$self->{'db_passwd'} = $amethyst_vars->{'DB_PASSWD'};
        $dsn = "DBI:$self->{'db_driver'}:database=$self->{'db_name'};host=$self->{'db_host'}";
	}
     
    while($db_connect_status eq 'FALSE')
    {
		if ($retry_counter <= $max_retry_limit)
		{
			eval
			{
				if($connection_type eq 'PERSISTENT')
				{
					
					$self->{'dbh'} = DBI->connect_cached($dsn, 
									     $self->{'db_user'}, 
									     $self->{'db_passwd'}
								            ) || die "Error creating DB Connection:$!\n";
					$db_connect_status = 'TRUE';					
				}
				else
				{
					$self->{'dbh'} = DBI->connect($dsn, 
								      $self->{'db_user'}, 
								      $self->{'db_passwd'}
							             ) ||  die "Error creating DB Connection:$!\n";
					$db_connect_status = 'TRUE';
				}
			};
			if ($@)
			{
				$db_connect_status = 'FALSE';
                
                # print "Connection exception in Connection.pm\n";

                #
                # For connection exception also,
                # Increment connection counter
                #
				$logging_obj->log ("EXCEPTION","EXCEPTION occured in connection class's constructor !!!".$@."\n");
                
				$tmanager::HA_TRY_COUNT = $retry_counter;
                $retry_counter++;
				sleep 2;
			}
		}
		else
		{
			$logging_obj->log ("ERROR","Cannot connect to ".$self->{'db_name'}." after ".$max_retry_limit." attempts \n");
			$db_connect_status = 'FALSE';
            last;
		}
#		$retry_counter++;
    }
    
    bless ($self, $class);
}



#
# Function name : get_database_handle
# Description:
# Function to return the database handler.
# Input : None
# Return Value : Returns the database handler
#
sub get_database_handle
{
   my $self = shift;
   return $self->{'dbh'};
}

#
# Function name : sql_query
# Description:
# Function to prepare & execute SQL query.
# Input[0] : SQL query as first argument
#      [1] : bind variables as array (optional)
# Return Value   : Returns the statemant handler
#
sub sql_query
{
	my $self = shift;
	my $sql = shift;
	my $param_array_ref = shift;
	my ($package, $caller_file_name, $line) = caller;
	my $query_status;
	my $sth;

	my $ping_status = $self->sql_ping_test();

	if($param_array_ref eq undef)
	{
		if( !$ping_status)
		{
			$sth = $self->{'dbh'}->prepare($sql) or
				$self->sql_error_log(1,$self->{'dbh'}->errstr,$package,$caller_file_name,$line,$sql);
		
			$query_status = $sth->execute() or 
				$self->sql_error_log(2,$self->{'dbh'}->errstr,$package,$caller_file_name,$line,$sql);
		}
		
	}
	else
	{
		if( !$ping_status)
		{
			$sth = $self->{'dbh'}->prepare($sql) or 
				$self->sql_error_log(1,$self->{'dbh'}->errstr,$package,$caller_file_name,$line,$sql);
		
			$query_status = $sth->execute(@$param_array_ref) or 
				$self->sql_error_log(2,$self->{'dbh'}->errstr,$package,$caller_file_name,$line,$sql);
		}
	}
	
	if ($query_status eq undef)
	{
		return undef;
	}
	else
	{
		return $sth;
	}	
}


#
# Function name : sql_fetchrow_array
# Description : 
# Fetches the next row of data and returns it as a list containing the field values
# Input  : Statement handler
# Return Value : Returns the rows of data as array.
#
sub sql_fetchrow_array
{
	my $self = shift;
	my $sth = shift;
	return ($sth->fetchrow_array());
}


#
# Function name : sql_fetchrow_hash_ref
# Description :
# Fetches the next row of data and returns it as a reference to a hash containing 
# field name and field value pairs
# Input  : Statement handler
# Return Value : Returns reference to a hash containing row data
#
sub sql_fetchrow_hash_ref
{
	my $self = shift;
	my $sth = shift;
	return ($sth->fetchrow_hashref());
}


#
# Function name :  sql_fetchrow_array_ref
# Description :
# Fetches the next row of data and returns a reference to an array holding the field values.
# Input  : Statement handler
# Return Value : Returns referance to array that contains row data 
#
sub sql_fetchrow_array_ref
{
	my $self = shift;	
	my $sth = shift;
	return ($sth->fetchrow_arrayref());
}


#
# Function name : sql_fetchall_array_ref
# Description :
# Fetches all the data to be returned from a prepared and executed statement handle. 
# It returns a reference to an array that contains one reference per row.
# Input   : Statement handler
# Return Value : Returns a reference to an array that contains one reference per row
#
sub sql_fetchall_array_ref
{
	my $self = shift;
	my $sth = shift;
	return ($sth->fetchall_arrayref({}));
}


#
# Function name : sql_fetchall_hash_ref
# Description :
# Fetches all the data to be returned from a prepared and executed statement handle.
# It returns a reference to a hash containing a key for each distinct value of the 
# $key_field column that was fetched
# Input  : Statement handler
# Return Value : Returns a reference to a hash containing a key for each distinct value 
# of the $key_field column that was fetche 
#
sub sql_fetchall_hash_ref
{
	my $self = shift;
	my $sth = shift;
	my $key_field = shift;
	return ($sth->fetchall_hashref($key_field));
}


#
# Function name : sql_finish
# Description :
# No more data will be fetched from this statement handle before it is either 
# executed again or destroyed.
# Input  : Statement handler
# Return Value : No
#
sub sql_finish
{
	my $self = shift;
	my $sth = shift;
	$sth->finish();
	return;
}


#
# Function name : sql_num_rows
# Description :
# Returns the number of rows affected by the last row affecting command, 
# or -1 if the number of rows is not known or not available.
# Input : Statement handler
# Return Value : Returns the number of rows affected by the last row affecting command
#
sub sql_num_rows
{
	my $self = shift;
	my $sth = shift;
	my $affected_row = $sth->rows();
	return $affected_row;
}


#
# Function name : sql_disconnect
# Description :
# Disconnects the database from the database handle. 
# disconnect is typically only used before exiting the program.
#
sub sql_disconnect
{
	my $self = shift;

	my $ping_status = $self->sql_ping_test();
	if( !$ping_status)
	{
		$self->{'dbh'}->disconnect();
	}	
	$self->{'dbh'} = undef;
	return;
}


#
# Function name : sql_selectrow_array
# Description :
# This method combines "prepare", "execute" and "fetchrow_array" into a single call.
# If called in a list context, it returns the first row of data from the statement.
# Input  : SQL query
# Return Value : Returns first row of data from the statement 
#
sub sql_selectrow_array
{
	my $self = shift;
	my $sql = shift;
	my $row_ary = undef;
	my $ping_status = $self->sql_ping_test();
	if( !$ping_status)
	{
		$row_ary = $self->{'dbh'}->selectrow_array($sql);
	}
	return $row_ary;
}


#
# Function name : sql_selectrow_hash_ref
# This method combines "prepare", "execute" and "fetchrow_hashref" into a single call. 
# It returns the first row of data from the statement.
# Input  : SQL query
# Return Value : Returns the first row of data from the statement
#
sub sql_selectrow_hash_ref
{
	my $self = shift;
	my $sql = shift;
	my $row_ary = undef;
	my $ping_status = $self->sql_ping_test();
	if( !$ping_status)
	{
		$row_ary = $self->{'dbh'}->selectrow_hashref($sql);
	}
	return $row_ary;
}


#
# Function name : sql_selectrow_array_ref
# Description :
# This  method combines "prepare", "execute" and "fetchrow_arrayref" into a single call. 
# It returns the first row of data from the statement.
# Input  : SQL query
# Return Value : Returns the first row of data from the statement 
#
sub sql_selectrow_array_ref
{
	my $self = shift;
	my $sql = shift;
	my $row_ary = undef;
	my $ping_status = $self->sql_ping_test();
	if( !$ping_status)
	{
		$row_ary = $self->{'dbh'}->selectrow_arrayref($sql);
	}
	return $row_ary;
}


#
# Function name : sql_selectall_array_ref
# This method combines "prepare", "execute" and "fetchall_arrayref" into a single call. 
# It returns a reference to an array containing a reference to an array (or hash)
# for each row of data fetched.
# Input  : SQL query
# Return Value : Returns  reference to an array containing a reference to an array 
#
sub sql_selectall_array_ref
{
	my $self = shift;
	my $sql = shift;
	my $ary_ref = undef;
	my $ping_status = $self->sql_ping_test();
	if( !$ping_status)
	{
		$ary_ref = $self->{'dbh'}->selectall_arrayref($sql);
	}
	return $ary_ref;
}


#
# Function name : sql_selectall_hash_ref
# This utility method combines "prepare", "execute" and "fetchall_hashref" into a single call. 
# It returns a reference to a hash containing one entry, at most, for each row,
# as returned by fetchall_hashref().
# Input[0] : SQL qury
#      [1] : Key field
# Return Value  : Returns reference to a hash containing one entry
#
sub sql_selectall_hash_ref
{
	my $self = shift;
	my $sql = shift;
	my $keyfield = shift;
	my $hash_ref = undef;
	my $ping_status = $self->sql_ping_test();
	if( !$ping_status)
	{
		$hash_ref = $self->{'dbh'}->selectall_hashref($sql,$keyfield);
	}
	return $hash_ref;
}


#
# Function name : sql_selectcol_array_ref
# This utility method combines "prepare", "execute", and fetching one column from all the rows, 
# into a single call. It returns a reference to an array containing the values of the 
# first column from each row.
# Input  : SQL query
# Return Value : Returns reference to an array containing the values of the first column from each row.
#
sub sql_selectcol_array_ref
{
	my $self = shift;
	my $sql = shift;
	my $ary_ref = undef;
	my $ping_status = $self->sql_ping_test();
	if( !$ping_status)
	{
		$ary_ref = $self->{'dbh'}->selectcol_arrayref($sql);
	}
	return $ary_ref;
}


#
# Commit (make permanent) the most recent series of database changes 
# if the database supports transactions and AutoCommit is off.
#
sub sql_commit
{
	my $self = shift;
    my ($package, $caller_file_name, $line) = caller;

	my $ping_status = $self->sql_ping_test();
	if( !$ping_status)
	{
		$self->{'dbh'}->commit or
                        $self->sql_error_log(2,$self->{'dbh'}->errstr,$package,$caller_file_name,$line);
	}

}


#
# Rollback (undo) the most recent series of uncommitted database changes
# if the database supports transactions and AutoCommit is off.
#
sub sql_rollback
{
	my $self = shift;
    my ($package, $caller_file_name, $line) = caller;

	my $ping_status = $self->sql_ping_test();
	if( !$ping_status)
	{
		$self->{'dbh'}->rollback  or
                        $self->sql_error_log(2,$self->{'dbh'}->errstr,$package,$caller_file_name,$line);
	}
}


#
# Enable transactions (by turning AutoCommit off) until the next call to commit 
# or rollback. After the next commit or rollback, AutoCommit will automatically 
# be turned on again.
#
sub sql_begin_work
{
	my $self = shift;
    my ($package, $caller_file_name, $line) = caller;

	my $ping_status = $self->sql_ping_test();
	if( !$ping_status)
	{
		$self->{'dbh'}->begin_work or
                        $self->sql_error_log(2,$self->{'dbh'}->errstr,$package,$caller_file_name,$line);
	}
}


#
# Functon name : sql_bind_col
# Description :
# Binds a Perl variable and/or some attributes to an output column (field) of a 
# SELECT statement.Column numbers count up from 1
# Input [0] : Statement handler
#       [1] : Column number
#       [2] : Variables to bind
#
sub sql_bind_col
{
	my $self = shift;
	my $sth = shift;
	my $column_number = shift;
	my $var_to_bind =  shift;
	$sth->bind_col($column_number, \$var_to_bind);
}


#
# Function name : sql_bind_columns
# Description :
# The list of references should have the same number of elements as the number 
# of columns in the SELECT statement. If it doesn't then bind_columns will bind 
# the elements given, upto the number of columns, and then return an error.
# Input [0] : Statement handler
#       [1] : Array of variables for binding columns
# Return Value : Statement handler
#
sub sql_bind_columns
{
 	my $self = shift;
 	my $sth = shift;
 	my @bind_array = @_;
 	$sth->bind_columns(@bind_array); 
 	return $sth;
} 
 

#
# Used to fetch the data from statement handler which eventually used with bind_columns().
#
sub sql_fetch
{
	my $self = shift;
  	my $sth = shift;
  	$sth->fetch();
}


#
# Function name : is_db_alive
# Description :
# Used to check the database handle is still alived or not.
# Input  : NO
# Output : 1 if database handler is defined and $dbh->ping is also defined
#          or returns 0
#
sub is_db_alive
{
	my $self = shift;
	my $status = 0;
	eval
	{    
		my $ping_status = $self->sql_ping_test();
		if( !$ping_status)
		{
			if ($self->{'dbh'} ne undef)
			{
				if ($self->{'dbh'}->ping ne undef)
				{
					$status = 1;
				}
			}
		}
	};
	if ($@)
	{
		$status = 0;              
	}
	return $status;
}

#
# Function name : sql_dbh_test
# Description :
# Used to check the database handle is undef or not
# Input  : NO
# Output : 1 if database handler is undefined else 0
#
sub sql_dbh_test
{
	my $self = shift;
	
	if($self->{'dbh'} eq undef)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

#
# Function name : sql_err
# Description :
# Used to return the $dbh->err string
#
sub sql_err
{
    my $self = shift;
	my $ping_status = $self->sql_ping_test();
	if( !$ping_status)
	{
		return $self->{'dbh'}->err;
	}
	else
	{
		return undef;
	}
			
}

#
# Function name : sql_ping_test
# Description :
# Used to check the database handle alive or not
# Input  : NO
# Output : 0 if database handler is alive else 1
#
sub sql_ping_test
{

    my $self = shift;

	my $return_status = 0;
	if( $self->{'dbh'} ne undef)
	{
		if($self->{'dbh'}->ping eq undef)
		{
			$return_status = 1;
		}
	}
	else
	{
		$return_status = 1;
	}
	return $return_status;
	
}

#
# Function name : sql_error
# Description:
# Function to return the error string.
# Input : None
# Return Value : Returns the error string
#
sub sql_error
{
    my $self = shift;
	my $ping_status = $self->sql_ping_test();
	if( !$ping_status)
	{
		return $self->{'dbh'}->errstr;
	}
	else
	{
		return undef;
	}
}

#
# Function name : sql_tables
# Description :
# Used to return the number of matching tables in the database
#
sub sql_tables
{
	my $self = shift;
	my $ping_status = $self->sql_ping_test();
	if( !$ping_status)
	{
		my @tables = $self->{'dbh'}->tables();
		return $#tables;
	}
	else
	{
		return undef;
	}
}
#
# Function name : set_auto_commit
# Description:
# Function to set AutoCommit && RaiseError values to given values.
# Input : AutoCommit && RaiseError values to set
# Return Value : None
#
sub set_auto_commit
{ 
	my $self = shift;
	my $m = shift;
	my $n = shift;

	my $ping_status = $self->sql_ping_test();
	if( !$ping_status)
	{
		$self->{'dbh'}->{AutoCommit} = $m;
		$self->{'dbh'}->{RaiseError} = $n;
	}
	else
	{
		return undef;
	}
}

#
# Function name : get_auto_commit
# Description:
# Function to get AutoCommit values.
# Input : None
# Return Value :None
#
sub get_auto_commit
{
	
	my $self = shift;

	my $ping_status = $self->sql_ping_test();
	if( !$ping_status)
	{
		return $self->{'dbh'}->{AutoCommit};
	}
	else
	{
		return undef;
	}
}

#
# Function name : get_raise_error
# Description:
# Function to get RaiseError values.
# Input : None
# Return Value : None
#
sub get_raise_error
{
 
	my $self = shift;

	my $ping_status = $self->sql_ping_test();
	if( !$ping_status)
	{
		return $self->{'dbh'}->{RaiseError};
	}
	else
	{
		return undef;
	}
 
}

#
# Prepare and execute a single statement. Returns the number of rows affected or undef on error. 
# A return value of -1 means the number of rows is not known, not applicable, or not available.
#
sub sql_do
{
	my $self = shift;
	my $sql = shift;

	my $ping_status = $self->sql_ping_test();
	if( !$ping_status)
	{
		$self->{'dbh'}->do($sql);
	}
	else
	{
		return undef;
	}
}

#
# Function name : sql_get_value
# Description:
# Forms the SQL based on the input paramters
#  and returns a single value
# Input : Table, Column, Condition, Verbose
# Return Value : Column Value
#
sub sql_get_value
{
	my $self = shift;
	my $table = shift;
	my $select_column = shift;
	my $condition = shift;
	my $debug = shift;
	my ($package, $caller_file_name, $line) = caller;
	my $query_status;
	my ($sth,$return_value);

	my $ping_status = $self->sql_ping_test();	
	$condition = ($condition) ? $condition : 1;
	my $sql = "SELECT ".$select_column." FROM ".$table." WHERE ".$condition;
	if($debug)
	{
		print $sql."\n";
	}
	if( !$ping_status)
	{
		$sth = $self->{'dbh'}->prepare($sql) or 
			$self->sql_error_log(1,$self->{'dbh'}->errstr,$package,$caller_file_name,$line,$sql);
	
		$query_status = $sth->execute() or 
			$self->sql_error_log(2,$self->{'dbh'}->errstr,$package,$caller_file_name,$line,$sql);
	}
	
	if ($query_status eq undef)
	{
		return undef;
	}
	else
	{
		my $ref = sql_fetchrow_hash_ref($self->{'dbh'},$sth);
		$return_value = $ref->{$select_column};
		return $return_value;
	}	
}

#
# Function name : sql_exec
# Description:
# Executes the SQL and returns the result set 
#  in a pre-defined format
# Input : SQL
# Return Value : Column Value
#
sub sql_exec
{
	my $self = shift;
	my $sql = shift;
	my $key_column = shift;
	my $secondary_key_column = shift;
	my ($package, $caller_file_name, $line) = caller;
	my $query_status;
	my ($sth,$return_value);

	my $ping_status = $self->sql_ping_test();
	if( !$ping_status)
	{
		$sth = $self->{'dbh'}->prepare($sql) or 
			$self->sql_error_log(1,$self->{'dbh'}->errstr,$package,$caller_file_name,$line,$sql);
	
		$query_status = $sth->execute() or 
			$self->sql_error_log(2,$self->{'dbh'}->errstr,$package,$caller_file_name,$line,$sql);
	}
	
	if ($query_status eq undef)
	{
		return undef;
	}
	else
	{			
		while(my $ref = sql_fetchrow_hash_ref($self->{'dbh'},$sth))
		{
			my $return_hash;				
			foreach my $key (keys %$ref)
			{					
				$return_hash->{$key} = $ref->{$key};
			}
			if($key_column)
			{
				my $hash_key = $ref->{$key_column};				
				if($secondary_key_column)
				{
					my $sec_hash_key = $ref->{$secondary_key_column};
					$return_value->{$hash_key}->{$sec_hash_key} = $return_hash;					
				}
				else
				{
					$return_value->{$hash_key} = $return_hash;
				}
			}
			else
			{
				push(@{$return_value}, $return_hash);
			}
		}
		return $return_value;
	}	
}

#
# Function name : sql_get_hash
# Description:
# Executes the SQL and returns the result set 
#  in a key value pair format
# Input : SQL, Key(Column Name), Value(Column Name)
# Return Value : Column Value
#
sub sql_get_hash
{
	my $self = shift;
	my $sql = shift;
	my $key_column = shift;
	my $value_column = shift;
	my ($package, $caller_file_name, $line) = caller;
	my $query_status;
	my ($sth,$return_value);

	my $ping_status = $self->sql_ping_test();
	if( !$ping_status)
	{
		$sth = $self->{'dbh'}->prepare($sql) or 
			$self->sql_error_log(1,$self->{'dbh'}->errstr,$package,$caller_file_name,$line,$sql);
	
		$query_status = $sth->execute() or 
			$self->sql_error_log(2,$self->{'dbh'}->errstr,$package,$caller_file_name,$line,$sql);
	}
	
	if ($query_status eq undef)
	{
		return undef;
	}
	else
	{
		while(my $ref = sql_fetchrow_hash_ref($self->{'dbh'},$sth))
		{			
			$return_value->{$ref->{$key_column}} = $ref->{$value_column};			
		}
		return $return_value;
	}	
}

#
# Escape the sql string 
# Escapes the ', ", / and \
# in the provided string
#
sub sql_escape_string
{
	my $self = shift;
	my $sql = shift;
	
	$sql =~ s/"/\"/g;
	$sql =~ s/\\/\\\\/g;
	$sql =~ s/'/\\\'/g;
	
	return $sql;
}

#
## Function name : sql_get_num_rows
## Description :
## Returns the number of rows from the query passed.
## Input : SQL query
## Return Value : Returns the number of rows returned by the query
##
sub sql_get_num_rows
{
        my $self = shift;
        my $sql = shift;
        my $sth = $self->sql_query($sql);
        my $affected_row = $sth->rows();
        return $affected_row;
}


#
# Function name : sql_error_log
# Description : 
# If any exception occurs during execution of any sql query then that will write into logfile
# by using this function
# Input : error , callinfg package name ,calling file name and line number from where that function is called. 
# 
#
sub sql_error_log 
{
  	my ($self) = shift;
  	my ($type,$error_msg,$package,$func_name,$line,$sql) = @_;
	my %log_data;
	
	my $logging_obj = new Common::Log();
	$self->{'cs_installation_directory'} = Utilities::get_cs_installation_path();
	my $log_file = "/home/svsystems/var/perl_sql_error.log";
  	my $query_log = $self->{'query_log'};
	my $content;
	if (Utilities::isWindows())
  	{			
    		$log_file = $self->{'cs_installation_directory'}."\\home\\svsystems\\var\\perl_sql_error.log";
  	}

	if ( $type == 1)
	{
		$content = "Query prepare failed ";	
	}
	else
	{
		$content = "Query execution failed ";
	} 	
	
	if(defined $sql)
	{
		$content = $content.",sql::$sql,";
	}
	

	$content = $content."$error_msg :: in $package package :: file_name $func_name :: line $line";
	
	$log_data{"SqlError"} = $content;
	$logging_obj->log("EXCEPTION",\%log_data,$TELEMETRY);
	
	open(LOGFILE, ">>$log_file") || print "Unable to open log file:$!\n";
	my $entry   = time() . ": " .$content ."\n"; 
	print LOGFILE $entry;
	close(LOGFILE);

}

1;
