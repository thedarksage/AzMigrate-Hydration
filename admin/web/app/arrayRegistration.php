<?
/*
 *=================================================================
 * FILENAME
 *  arrayRegistration.php
 *
 * DESCRIPTION
 *  This script registers the axiom arrays and lun information.
 *
 *=================================================================
 *                 Copyright (c) InMage Systems
 *=================================================================
*/


class arrayRegistration
{
	public $connect;
	public $partition_length;
	public $packet_size;
	public $batch_update_queries;
	public $batch_queries;

	public function __construct()
	{
		#8MB defined as allowed data packet size for group queries execution.
		global $conn_obj;
		$this->packet_size = 8388608;
		$this->batch_update_queries["insert"] = array();
		$this->batch_update_queries["update"] = array();
		$this->batch_update_queries["delete"] = array();
		$this->batch_queries[] = array();

		$this->connect = $conn_obj;
	}
}
?>
