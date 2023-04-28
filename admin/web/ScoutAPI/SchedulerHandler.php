<?php
/*
 *$Header: /src/admin/web/ScoutAPI/SchedulerHandler.php,v 1.4.56.1 2017/08/06 15:06:06 srpatnan Exp $
 *=================================================================
 * FILENAME
 *  SchedulerHanlder.php
 *
 * DESCRIPTION
 *  This script executes queries and return result for requests coming from scheduler
 *
 *=================================================================
 *                 Copyright (c) InMage Systems
 *=================================================================
*/
class SchedulerHandler extends ResourceHandler
{
    private $conn;
    private $loggingObj;

	public function __construct()
	{
        global $logging_obj, $LOG_ROOT;

        $this->conn = new Connection();
        $this->loggingObj = $logging_obj;
        $this->loggingObj->global_log_file = $LOG_ROOT . 'schedulerapi.log';
	}
}
?>