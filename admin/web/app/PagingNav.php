<?php
/*
 *$Header: /src/admin/web/app/PagingNav.php,v 1.3.56.1 2017/08/16 15:57:41 srpatnan Exp $
 *================================================================= 
 * FILENAME
 *    PagingNav.php
 *
 * DESCRIPTION
 *    This script contains methods related to
 *    database record pagination on UI
 *=================================================================
 *                 Copyright (c) InMage Systems                    
 *=================================================================
*/

class PagingNav 
{
    private $page_url;
    private $page_id;
    private $page_limit;
    private $page_count;
    private $total_records;
    private $extra_records;
    private $sql_limit_start;
    private $sql_limit_end;

	/*
    * Initialization Method, which sets current page id, total records and page limit for pagination
    */    
	public function __construct($page_id, $total_records, $page_limit=null)
	{
        $this->page_id = ($page_id) ? $page_id : 1;
		$this->total_records = $total_records;
        $this->page_url = $_SERVER['PHP_SELF'];
        
        $this->setPageLimit($page_limit);
	}

	/*
    * Method to set the number of records to display in one view
    */    
	private function setPageLimit($page_limit)
	{
        $this->page_limit = ($page_limit) ? $page_limit : 25;
		$this->page_count = floor($this->total_records / $this->page_limit);
        $this->extra_records = $this->total_records - ($this->page_count * $this->page_limit);
        
        $this->sql_limit_start = ($this->page_limit * $this->page_id) - $this->page_limit;
        $this->sql_limit_end = ($this->page_id == $this->page_count + 1) ? $this->extra_records : $this->page_limit;
	}  
 }
?>