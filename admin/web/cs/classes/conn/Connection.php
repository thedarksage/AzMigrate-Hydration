<?php
/*
 *================================================================= 
 * FILENAME
 *    Connection.php
 *
 * DESCRIPTION
 *    This is the Common DB abstarction class for Host and Fabric
 *    
 *
 * HISTORY
 *     *     21-Jun-2008  Himanshu Patel
 *              
 *=================================================================
 *                 Copyright (c) InMage Systems                    
 *=================================================================
*/
class Connection 
{
    var $_singleton;
    var $_connection;
    var $sql;
    var $sqlmethod;
    var $queryresult;
    var $recordselect;
    var $totalrecords;
    var $db;
    var $dberror;
    var $dbname;
    var $str;
    var $resource;
    var $selectmethod;
    var $enableLog;
    var $logfile;    
    var $rst_type;
    var $offset;
    var $row_number;
	var $enable_exception;
	var $query_result;
	var $arguments;

    /*
	* Function Name: Connection
	* Description
	* 	Connection Class Constructor
	*     Parameters:
	*     Param 1 [IN]:enable sql log, if it is set=1 logs will be enable 
	*     Param 2 [IN]:Mysql Host Name
	*     Param 3 [IN]:Mysql User Name
	*     Param 4 [IN]:Mysql Password
	*     Param 5 [IN]:Mysql Data Base Name
	*     Param 6 [Out]:Return connection resource if TRUE or False
	* Return Value:
	*     Ret Value:TRUE or False
	*
    */
    function __construct($enableLog=1)
    {
		global $MAX_PHP_CONNECTION_RETRY_LIMIT, $enable_storage_monitoring;
		global $DB_HOST,$DB_USER,$DB_PASSWORD,$DB_DATABASE_NAME;
		$retry = 0;
		$select_db_retry = 0;
		$charset_retry = 0;
		$connection_status = false;
		$select_db_status = false;
		$charset_status = false;
        $mysql_error_code = "";
		
		try
		{
			$this->enableLog = $enableLog;
			$this->dbname = $DB_DATABASE_NAME;
			$this->enable_exception = 0;
			
			while (!$connection_status and ($retry < $MAX_PHP_CONNECTION_RETRY_LIMIT)) 
			{
				$this->_connection = @mysqli_connect($DB_HOST, $DB_USER, $DB_PASSWORD );
				
				if (! $this->_connection)
				{
					$mysql_error_code .= mysqli_connect_errno().",";
					if ($retry < $MAX_PHP_CONNECTION_RETRY_LIMIT)
					{
						sleep($enable_storage_monitoring);
						$retry++;
					}
					if ($retry == $MAX_PHP_CONNECTION_RETRY_LIMIT)
					{
						throw new SQLException("MYSQL connection failed with error no ".$mysql_error_code.strlen($DB_HOST).",".strlen($DB_USER).",".strlen($DB_PASSWORD));
					}
				}
				else
				{
					$connection_status = true;
				}
			}
			
			while (!$select_db_status and ($select_db_retry < $MAX_PHP_CONNECTION_RETRY_LIMIT)) 
			{
				if (! @mysqli_select_db($this->_connection, $DB_DATABASE_NAME))
				{
					if ($select_db_retry < $MAX_PHP_CONNECTION_RETRY_LIMIT)
					{
						sleep($enable_storage_monitoring);
						$select_db_retry++;
					}
					if ($select_db_retry == $MAX_PHP_CONNECTION_RETRY_LIMIT)
					{
						throw new SQLException("Can't select database. Check whether database is installed,".strlen($DB_DATABASE_NAME));
					}
				}
				else
				{
					$select_db_status = true;
				}
			}
			
			while (!$charset_status and ($charset_retry < $MAX_PHP_CONNECTION_RETRY_LIMIT)) 
			{
				if (! @mysqli_set_charset($this->_connection, "utf8"))
				{
					if ($charset_retry < $MAX_PHP_CONNECTION_RETRY_LIMIT)
					{
						sleep($enable_storage_monitoring);
						$charset_retry++;
					}
					if ($charset_retry == $MAX_PHP_CONNECTION_RETRY_LIMIT)
					{
						throw new SQLException("Can't set charset to utf8.");
					}	
				}
				else
				{
					$charset_status = true;
				}
			}			
		
		}
        catch(SQLException $exception)
        {
			// Log the exception
			$this->sql_log($exception->getMessage(), 1);
			$this->sql_log($exception->getTraceAsString(),1);
			
			// Erases the output buffer if any.
			ob_clean();
			
			// Send 500 response as no database connection created
			header("HTTP/1.0 500 " . $exception->getMessage(), true);
			
			exit(0);
        }
    }
	
	function getConnectionHandle()
	{
	  return $this->_connection;
	}
    /*
     	*
	* Function Name:sql_query
	*
	* Description
	*
	* used for executing the mysql_query statement using abstract layer
	* Parameter
	* 
	*    Param 1 [IN] sql query
	*    Param 2 [IN] parametrise arguments
	*    Param 3 [OUT] mysql resource identifier
	
    */
    function sql_query($sql, $args = NULL, $key_col=NULL)
    {
		$this->arguments = $args;
		// $args should at-least contains the value
		if(! isset($args))	return $this->sql_query_noprepare($sql);
		
        try
        {
			// $args should be array
			if(! is_array($args))	throw new SQLException("Invalid argument passed args: $args.");
			
            $this->sql = trim($sql);
            $result = null;

            #
            #   mysqli.reconnect is disabled,so trying to connect with _connect
            #
            if($this->_connection->ping() == FALSE)   throw new SQLException('Re-connection failed');

            if(($stmt = $this->_connection->prepare($this->sql))== FALSE)   throw new SQLException('Prepare Statement failed');
			if(count($args) && call_user_func_array(array($stmt, 'bind_param'), $this->query_bind_params($args)) == FALSE)	throw new SQLException('Bind Parameter failed.');
            if($stmt->execute() == FALSE) throw new SQLException('Query Execution failed');

            $sql_action = strtoupper(substr(trim($this->sql,' ('), 0, 6));
    		$temp_str   = strtoupper(substr(trim($this->sql,' ('), 0, 4));
    		if($temp_str == 'DESC' || $temp_str == 'SHOW')    $sql_action = 'SELECT';

            switch($sql_action)
            {
                case 'SELECT':
                    $stmtRow = array();
                    $metadata = $stmt->result_metadata();

                    while ($field = mysqli_fetch_field($metadata))
                    {
    					$stmtRow[$field->name] = NULL;
                    }

                    $stmt->store_result();
                    mysqli_free_result($metadata);
                    call_user_func_array(array($stmt, 'bind_result'), $this->copy_array_ref($stmtRow));
					$result = array();

                    while($stmt->fetch())
                    {
                        if($key_col) $result[$stmtRow[$key_col]] = $this->copy_array($stmtRow);
                        else $result[] = $this->copy_array($stmtRow);
                    }
                break;

                case 'INSERT':
                    $result = $this->sql_insert_id();
                break;

                case 'UPDATE':
                    $result = $this->sql_affected_row();
                break;

                case 'DELETE':
                    $result = $this->sql_affected_row();
                break;

                default:
                    // treat similarly to select?
                    $result = $stmt;
            }
        }
        catch(SQLException $exception)
        {
			$callstack = $exception->getTrace();
            $this->sql_log(isset($callstack[1]) ? $callstack[1] : $callstack[0], 0, $exception->getMessage());
			if($this->enable_exception == 1)
			{
				$this->raise_exception();
			}
        }

        if(empty($stmt) == FALSE)   $stmt->close();
		
        return $result;
    }
	
	private function sql_query_noprepare($sql)
    {
        $this->sql=$sql;
	    $this->db=$this->_connection;
		$this->queryresult=@mysqli_query($this->db,$this->sql);
		if($this->sql_errno())
		{
			$trace = debug_backtrace();
			$caller = array_shift($trace);
			$this->sql_log($caller);
			
			if($this->enable_exception == 1)
			{
				$this->raise_exception();
			}
			
		}
		return $this->queryresult;
    }
	
	function copy_array_ref(&$row)
    {
		$newrow = array();
		foreach($row as $rowKey => $rowValue)
		{
			$newrow[$rowKey] = &$row[$rowKey];
		}
        return $newrow;
    }

	function query_bind_params($args)
    {
		array_walk_recursive($args, create_function('&$value, $key', '$value = is_null($value) ? "" : $value;'));
        array_unshift($args, str_repeat("s", count($args)));
        return $args;
    }
	
	function copy_array($stmtRow)
    {
        foreach($stmtRow as $key => $value)
        {
            $row[$key] = $value;
        }

        return $row;
    }
	
	
	
    /*
	* 
	* New Function Name:sql_fetch_array
	* Description
	*
	* used for exceuting the mysql_fetch_array statement using abstarct layer
	* Parameter
	* 
	*    Param 1 [IN] $result resource
	*    Param 2 [IN] $rst_type off set
	*    Param 3 [OUT] $arr array value based upon the result 
    */
    function sql_fetch_array($result,$rst_type=MYSQLI_BOTH) {	
		$arr = @mysqli_fetch_array($result,$rst_type);
		if($this->sql_errno())
		{
			$trace = debug_backtrace();
			$caller = array_shift($trace);
			$this->sql_log($caller);
			
			if($this->enable_exception == 1)
			{
				$this->raise_exception();
			}
		}
		return $arr;	
    }
	 /*
	* 
	* New Function Name:sql_fetch_assoc
	* Description
	*
	* used for exceuting the mysql_fetch_assoc statement using abstarct layer
	* Parameter
	* 
	*    Param 1 [IN] $result resource
	*    Param 2 [OUT] $assoc array value based upon the result 
    */
    function sql_fetch_assoc($result) {
		$assoc = @mysqli_fetch_assoc($result);
		if($this->sql_errno())
		{
			$trace = debug_backtrace();
			$caller = array_shift($trace);
			$this->sql_log($caller);
			
			if($this->enable_exception == 1)
			{
				$this->raise_exception();
			}
		}
		return $assoc;	
    }
	/*
	* 
	* New Function Name:sql_fetch_row
	* Description
	*
	* used for exceuting the mysql_fetch_row statement using abstarct layer
	* Parameter
	* 
	*    Param 1 [IN] $result resource
	*    Param 2 [OUT] return array value based upon the result 
    */
    function sql_fetch_row($result) {
		$row = @mysqli_fetch_row($result);
		if($this->sql_errno())
		{
			$trace = debug_backtrace();
			$caller = array_shift($trace);
			$this->sql_log($caller);
			
			if($this->enable_exception == 1)
			{
				$this->raise_exception();
			}
		}
		return $row;	
    }
	
	 /*
	* 
	* New Function Name:sql_fetch_object
	* Description
	*
	* used for exceuting the mysql_fetch_object statement using abstarct layer
	* Parameter
	* 
	*    Param 1 [IN] $result resource
	*    Param 2 [OUT] return Object value based upon the result 
    */
    function sql_fetch_object($result) {
		$obj = @mysqli_fetch_object($result);
		if($this->sql_errno())
		{
			$trace = debug_backtrace();
			$caller = array_shift($trace);
			$this->sql_log($caller);
			
			if($this->enable_exception == 1)
			{
				$this->raise_exception();
			}
		}
		return $obj;	
    }
	/*
	 /*
	* 
	* New Function Name:sql_fetch_field
	* Description
	*
	* used for exceuting the sql_fetch_field statement using abstarct layer
	* Parameter
	* 
	*    Param 1 [IN] $result resource
	*    Param 2 [OUT] return Object value based upon the result 
    */
    function sql_fetch_field($result) {
		$obj = @mysqli_fetch_field($result);
		if($this->sql_errno())
		{
			$trace = debug_backtrace();
			$caller = array_shift($trace);
			$this->sql_log($caller);
			
			if($this->enable_exception == 1)
			{
				$this->raise_exception();
			}
		}
		return $obj; 
	}
	
	
	
	function sql_field_name($result) {
		$obj = @mysqli_fetch_field($result);
		if($this->sql_errno())
		{
			$trace = debug_backtrace();
			$caller = array_shift($trace);
			$this->sql_log($caller);
			
			if($this->enable_exception == 1)
			{
				$this->raise_exception();
			}
		}
		return $obj;
	}
	
	/*	
	* 
	* New Function Name:sql_num_fields
	* Description
	*
	* used for exceuting the mysql_num_fields statement using abstarct layer
	* Parameter
	* 
	*    Param 1 [IN] mysql resource 
	*    Param 2 [OUT] return integer value based upon the result 
    */
    function sql_num_field($resource){
		$this->resource=$resource;
		$num_fields = @mysqli_num_fields($this->resource);
		if($this->sql_errno())
		{
			$trace = debug_backtrace();
			$caller = array_shift($trace);
			$this->sql_log($caller);
			
			if($this->enable_exception == 1)
			{
				$this->raise_exception();
			}
		}
		return $num_fields;
    }
	 /*
	*
	* Function Name:sql_num_row
	*
	* Description
	*
	* used for exceuting the mysql_num_rows statement using abstarct layer
	* Parameter
	* 
	*    Param 1 [IN] mysql resource 
	*    Param 2 [OUT] return integer value based upon the result 
    */
    function sql_num_row($resource){
	    $this->resource=$resource;
	    $num_rows = @mysqli_num_rows($this->resource);
		if($this->sql_errno())
		{
			$trace = debug_backtrace();
			$caller = array_shift($trace);
			$this->sql_log($caller);
			
			if($this->enable_exception == 1)
			{
				$this->raise_exception();
			}
		}
		return $num_rows;
    }
    
    
	/*
	*
	* Function Name:sql_close
	*
	* Description
	*
	* used for exceuting the mysql_close statement using abstarct layer
	* Parameter
	* 
	*    Param 2 [OUT] return true
    */
    function sql_close(){
	    @mysqli_close($this->_connection);
	    return true;
    }
    /*
	*
	* Function Name:sql_error
	*
	* Description
	*
	* used for exceuting the mysql_error statement using abstarct layer
	* Parameter
	* 
	*    Param 2 [OUT] return error string
    */
    function sql_error(){
	    return @mysqli_error($this->_connection);
    }
     /*
	*
	* Function Name:sql_errno
	*
	* Description
	*
	* used for exceuting the mysql_errno statement using abstarct layer
	* Parameter
	* 
	*    Param 2 [OUT] return error string
    */
    function sql_errno(){
		return @mysqli_errno($this->_connection);
	}
	    /*
	* 
	* Function Name:sql_data_seek
	* Description
	*
	* used for exceuting the mysql_data_seek statement using abstarct layer
	* Parameter
	*    Param 1 [IN]: $resource resource
	*    Param 2 [IN]: $row_number integet
	*    Param 3 [OUT] bool
    */
    function sql_data_seek($resource,$row_number){
	    $data_seek = @mysqli_data_seek($resource,$row_number);
		if($this->sql_errno())
		{
			$trace = debug_backtrace();
			$caller = array_shift($trace);
			$this->sql_log($caller);
			
			if($this->enable_exception == 1)
			{
				$this->raise_exception();
			}
		}
		return $data_seek;
    }
  /*
	*
	* Function Name:sql_escape_string
	*
	* Description
	*
	* used for exceuting the mysql_escape_string statement using abstarct layer
	* Parameter
	* 
	*    Param 1 [IN] string to be escaped 
	*    Param 2 [OUT] return mysql escaped string 
		
    */
    function sql_escape_string($str){
		$this->str=$str;
		return @mysqli_escape_string($this->_connection,$this->str);
    }

	 /*
	* 
	* Function Name:sql_insert_id
	* Description
	*
	* used for exceuting the mysql_insert_id statement using abstarct layer
	* Parameter
	* 
	*    Param 1 [OUT] returns the ID generated for an AUTO_INCREMENT column
	*    by the previous INSERT query
    */
    function sql_insert_id(){
		return @mysqli_insert_id($this->_connection);
    }
	
	function nextRow ($result) 
	{
		$row = mysqli_fetch_array($result);
		if($this->sql_errno())
		{
			$trace = debug_backtrace();
			$caller = array_shift($trace);
			$this->sql_log($caller);
			
			if($this->enable_exception == 1)
			{
				$this->raise_exception();
			}
		}
		return $row;
	}
	

    function execute($sql) 
    {
		$this->sql = $sql;
		$result = mysqli_query($this->_connection,$sql);
		if($this->sql_errno())
		{
			$trace = debug_backtrace();
			$caller = array_shift($trace);
			$this->sql_log($caller);
		}
    
		#$this->checkErrors($sql);

		if ($this->isSqlInsert($sql) || $this->isSqlUpdate($sql) || $this->isSqlDelete($sql)) 
		{
			#$this->log($sql);
		}
		return $result;
    }

    function isSqlInsert ($sql) 
    {
		$ret = preg_match ("/insert/i", trim($sql)) ? true : false;
		return $ret;
    }

    function isSqlUpdate ($sql) 
    {
		$ret = preg_match ("/update/i", trim($sql)) ? true : false;
		return $ret;
    }

    function isSqlDelete ($sql) 
    {
		$ret = preg_match ("/delete/i", trim($sql)) ? true : false;
		return $ret;
    }
	
	function checkErrors($sql) 
    {
		// Only thing that we need todo is define some variables
		// And ask from RDBMS, if there was some sort of errors.
		$err=mysqli_error();
		$errno=mysqli_errno();

		if($errno)
		{
			// SQL Error occurred. This is FATAL error. Error message and 
			// SQL command will be logged and aplication will teminate immediately.
			$message = "The following SQL command ".$sql." caused Database error: ".$err.".";

			//$message = addslashes("SQL-command: ".$sql." error-message: ".$message);
			//$systemLog->writeSystemSqlError ("SQL Error occurred", $errno, $message);
			print "Unrecowerable error has occurred. All data will be logged.";
			print "Please contact System Administrator for help! \n";
			print "<!-- ".$message." -->\n";
			exit;
		}
		else
		{
			return;
		}
    }
	
	 /*
	*
	* Function Name:sql_affected_row
	*
	* Description
	*
	* used for exceuting the mysql_affected_rows statement using abstarct layer
	* Parameter
	* 
	*    Param 1 [IN] mysql resource identifier
	*    Param 2 [OUT] return integer value based upon the result 
    */
    function sql_affected_row(){
		$affected_rows = @mysqli_affected_rows($this->_connection);
		if($this->sql_errno())
		{
			$trace = debug_backtrace();
			$caller = array_shift($trace);
			$this->sql_log($caller);
			
			if($this->enable_exception == 1)
			{
				$this->raise_exception();
			}
		}
		return $affected_rows;
    }
	/*
	* 
	* New Function Name:sql_real_escape_string
	* Description
	*
	* used for exceuting the mysql_real_escape_string statement using abstarct layer
	* Parameter
	* 
	*    Param 1 [IN] string to be escaped 
	*    Param 2 [OUT] return mysql escaped string 
    */
    
    function sql_real_escape_string($str){
	    $this->str=$str;
	    return @mysqli_real_escape_string($this->_connection,$this->str);
    }
	  /*
	*
	* Function Name:sql_log
	*
	* Description
	*
	* used for creating sql log if logs are enabled 
	* Parameter
	* it will track all the sql operation
	*  Param 1 [IN] sql statemnt
	*  Param 2 [OUT] return true or false
    */
    function sql_log($caller= '', $flag= 0, $customMessage= null)
	{
		global $REPLICATION_ROOT, $commonfunctions_obj, $logging_obj;
		if($flag)
		{
			$ErrorStr = $caller;
		}
		else
		{
			$ErrorStr = basename($caller['file'])."::".$caller['function']."::".$caller['line']."::SQL->".$this->sql."::ErrorStr->".$this->sql_error()." :: CustomMessage-> ".$customMessage;
		}
	
		if (preg_match('/Linux/i', php_uname())) {
	    	$this->logfile="$REPLICATION_ROOT/var/php_sql_error.log";
		}else{
	    	$this->logfile="$REPLICATION_ROOT\\var\\php_sql_error.log";
		}
		
		if ($this->enableLog==1){
			$file_handle = $commonfunctions_obj->file_open_handle($this->logfile, 'a+');
            if(!$file_handle) return;
			$file_handle && fwrite($file_handle, date('Y-m-d h:i:s (T)').' - '.$ErrorStr . "\n\n");
			$file_handle && fclose($file_handle);
	    	    
		}
		
		$GLOBALS['http_header_500'] = TRUE;
		$logging_obj->my_error_handler("INFO",array("Status"=>"Fail","Reason"=>"SqlException","Message"=>date('Y-m-d h:i:s (T)').' - '.$ErrorStr),Log::APPINSIGHTS);
    }
	 /*
	* 
	* Function Name:sql_get_value
	* Description
	*
	* used for get a single value from table
	* Parameter
	*    Param 1 [IN]: $table_name string
	*    Param 2 [IN]: $field_name string
	*    Param 3 [IN]: $where_cond string	
	*    Param 4 [IN]: $debug bool	
	*    Param 3 [OUT] string
    */	
	function sql_get_value($table_name, $field_name, $where_cond='', $args= array(), $debug=0)
	{
		$this->arguments = $args;
	    $sql = 'SELECT '.$field_name.' FROM '.$table_name;
		$sql .= (trim($where_cond)) ? ' WHERE '.$where_cond : '';
		if($debug) echo($sql);
		
		if(empty($args))
		{
			$result = $this->sql_query($sql);
			$arr = $this->sql_fetch_array($result);
			$val = $arr[$field_name];
		}
		else
		{
			$result = $this->sql_query($sql, $args);
			$val = isset($result[0][$field_name]) ? $result[0][$field_name] : null;
		}
		
		if($this->sql_errno())
		{
			$trace = debug_backtrace();
			$caller = array_shift($trace);
			$this->sql_log($caller);
		}
		
	    return $val;
	}
	
	/*
	*
	* Function Name:sql_commit
	*
	* Description
	*
	* used for exceuting the mysql_close statement using abstarct layer
	* Parameter
	* 
	*    Param 2 [OUT] return bool
    */
    function sql_commit(){
	    $commit_result = @mysqli_commit($this->_connection);
		if($this->sql_errno())
		{
			$trace = debug_backtrace();
			$caller = array_shift($trace);
			$this->sql_log($caller);
			
			if($this->enable_exception == 1)
			{
				$this->raise_exception();
			}
		}
		return $commit_result;
    }	
	
	/*
	*
	* Function Name:sql_autocommit
	*
	* Description
	*
	* used for exceuting the mysqli_autocommit statement using abstarct layer
	* Parameter
	* 
	*    Param 1 [IN]: $mode bool
	*    Param 2 [OUT] return bool
    */
    function sql_autocommit($mode){
	    $autocommit_result = @mysqli_autocommit($this->_connection,$bool);
		if($this->sql_errno())
		{
			$trace = debug_backtrace();
			$caller = array_shift($trace);
			$this->sql_log($caller);
			
			if($this->enable_exception == 1)
			{
				$this->raise_exception();
			}
		}
		return $autocommit_result;
    }

	/*
	*
	* Function Name:sql_commit
	*
	* Description
	*
	* used for exceuting the mysql_close statement using abstarct layer
	* Parameter
	* 
	*    Param 2 [OUT] return bool
    */
    function sql_rollback(){
	    $rollback_result = @mysqli_rollback($this->_connection);
		if($this->sql_errno())
		{
			$trace = debug_backtrace();
			$caller = array_shift($trace);
			$this->sql_log($caller);
		}
		return $rollback_result;
    }

	function sql_begin(){
		$sql = "BEGIN";
		$this->sql_query($sql);
	}
	

    function sql_multi_query($query){
		$this->sql = $query;
	    $multi_query_result = @mysqli_multi_query($this->_connection,$query);
		if($this->sql_errno())
		{
			$trace = debug_backtrace();
			$caller = array_shift($trace);
			$this->sql_log($caller);
		}
		return $multi_query_result;
    }
	
	function sql_store_result()
	{
		$store_result = mysqli_store_result($this->_connection);
		if($this->sql_errno())
		{
			$trace = debug_backtrace();
			$caller = array_shift($trace);
			$this->sql_log($caller);
		}
		return $store_result;
	}
	
	function sql_more_result()
	{
		$more_results = mysqli_more_results($this->_connection);
		if($this->sql_errno())
		{
			$trace = debug_backtrace();
			$caller = array_shift($trace);
			$this->sql_log($caller);
		}
		return $more_results;
	}

	function sql_next_result()
	{
		$next_result = mysqli_next_result($this->_connection);
		if($this->sql_errno())
		{
			$trace = debug_backtrace();
			$caller = array_shift($trace);
			$this->sql_log($caller);
		}
		return $next_result;
	}

	function sql_free_result($result)
	{
		$free_result = mysqli_free_result($result);
		if($this->sql_errno())
		{
			$trace = debug_backtrace();
			$caller = array_shift($trace);
			$this->sql_log($caller);
		}
		return $free_result;
	}
	
	/*
	*
	* Function Name:sql_exec
	*
	* Description:
	*
	* Used to execute mysql query and returns the result based on the query type
	* SELECT :: If we execute selecte query it will return actual selected data in the form of array instead of returning query result
	* INSERT :: If we execute insert query it will return last insert id
	* UPDATE/DELETE :: If we execute update/delete query it will return the number of effected rows for the current update/delete operation 
	* If any of the above operation does not matches then it will return the query result
	* Parameter
	* 	 Param 1 [IN]  sql statement
	*    Param 2 [OUT] return query result
    */
	function sql_exec($sql) {
        $this->sql = trim($sql);
		
        $sql_action = strtoupper(substr(trim($sql,' ('),0,6));
		$temp_str = strtoupper(substr(trim($sql,' ('),0,4));
		if($temp_str == 'DESC' || $temp_str == 'SHOW') $sql_action = 'SELECT';
        $this->query_result = @mysqli_query($this->_connection,$this->sql);
		
		if($this->sql_errno())
		{
			$trace = debug_backtrace();
			$caller = array_shift($trace);
			$this->sql_log($caller);
			return false;
		}

		switch($sql_action)
		{
			case 'SELECT':
				return $this->sql_records();
			break;
			
			case 'INSERT':
				return $this->sql_insert_id();
			break;
			
			case 'UPDATE':
				return $this->sql_affected_row();
			break;
			
			case 'DELETE':
				return $this->sql_affected_row();
			break;

			default:
				return $this->query_result;
		}
    }
	function sql_records() {
	
		$select_records = array();
		$num_records = mysqli_num_rows($this->query_result);
		if ($num_records > 0)
		{
			while ($row = mysqli_fetch_array($this->query_result,MYSQLI_ASSOC))
			{				
				$select_records[] = $row;
			}
			mysqli_free_result($this->query_result);
		}
		
		return $select_records;
	}
	/*
	*
	* Function Name:sql_get_array
	*
	* Description:
	*
	* Used to get the select query result in the form of array key and value pairs if we want to have key and value as specific fields in the table
	*
	* Parameter
	* 	 Param 1 [IN]  sql statement
	* 	 Param 1 [IN]  field1 which is key for the hash
	* 	 Param 1 [IN]  field2 which is value for the hash
	*    Param 2 [OUT] return hash
    */
	function sql_get_array($sql, $field1, $field2, $args = array())
	{
		$this->arguments = $args;
		$infoArr = array();
		if(empty($args))
		{
			$result = $this->sql_exec($sql);
		}
		else
		{
			$result = $this->sql_query($sql, $args);
		}
		
		if($this->sql_errno())
		{
			$trace = debug_backtrace();
			$caller = array_shift($trace);
			$this->sql_log($caller);
		}

		for($j=0;isset($result[$j]);$j++)
		{
			$key = $result[$j][$field1];
			$value = $result[$j][$field2];
			$infoArr[$key] = stripslashes($value);
		}

		return $infoArr;
	}
	
	/*
		Enable Throwing of exception if any sql operation fails
	*/
	function enable_exceptions()
	{
		$this->enable_exception = 1;
	}
	
	/*
		Disable Throwing of exception if any sql operation fails
	*/
	function disable_exceptions()
	{
		$this->enable_exception = 0;
	}
	
	/*
		Raise Exception if any sql operation fails
	*/
	function raise_exception($message='', $error_code=0)
	{
		if (
			((preg_match("/insert/i", $sql) === 1) AND (preg_match("/accounts/i", $sql) === 1) AND (preg_match("/values/i", $sql) === 1))
			||
			((preg_match("/update/i", $sql) === 1) AND (preg_match("/accounts/i", $sql) === 1) AND (preg_match("/set/i", $sql) === 1))
			) 
		{
			//Stop logging credential details in case of SQL query execution failure.
		}
		else
		{
			$args_data = $this->arguments;
			if (is_array($args_data))
			{
				$args_data = print_r($args_data, TRUE);
			}
		}
		$message = "SQLException::\n";
		$message = $message ."SQL:: ".$this->sql."\nArgs:: ".$args_data."\nSQL Error:: ".$this->sql_error()."\nSQL ErrorNo::".$this->sql_errno()."\n";
		$e = new SQLException($message, $this->sql_errno());
		$message = $message ."BackTrace:: ".$e->getTraceAsString()."\n";
		$this->sql_log($message,1);
		throw $e;
	}

}
	
?>