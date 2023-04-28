<?php
/*
 *=================================================================
 * FILENAME
 *  BatchSqls.php
 *
 * DESCRIPTION
 *
 *  This script executes the Batch sql queries. 
 *  It will commit if all the queries are executed successfully.
 *  It will rollback if anyone of the query execution failed.
 *
 *=================================================================
 *                 Copyright (c) InMage Systems
 *=================================================================
*/

class BatchSqls
{
	private $conn;
	
	public function __construct()
	{
		global $conn_obj;
		
		$this->conn = $conn_obj;
	}
	
	private function constructBatches($batch_update_queries)
	{
		global $logging_obj;

		$multi_query_list = array();
		$insert_batches = array();
		$update_batches = array();
		$delete_batches = array();
		$replace_batches = array();

		foreach($batch_update_queries as $key=>$value)
		{
			switch($key)
			{
				case "insert":
					$insert_batches = $this->getBatchQueries($value);				
				break;
				case "update":
					$update_batches = $this->getBatchQueries($value);
				break;
				case "delete":
					$delete_batches = $this->getBatchQueries($value);
				break;
				case "replace":
					$replace_batches = $this->getBatchQueries($value);
				break;
				default:
				break;			
			}
		}
		
		$multi_query_list = array_merge($insert_batches,$update_batches,$delete_batches, $replace_batches);
		return $multi_query_list;
	}
	
	private function getBatchQueries($multi_queries)
	{
		global $logging_obj;
		$partition_strings = array();
		#8MB defined as allowed data packet size for group queries execution.
		$packet_size = 8388608;

		$multi_queries = array_unique($multi_queries);
		$records_count = count($multi_queries);

		if ($records_count > 0)
		{
			$queries_string = implode(";",$multi_queries);
			$total_records_size = strlen($queries_string);
			$chunk_size = ceil($records_count/($total_records_size/$packet_size));
			$partitions = array_chunk($multi_queries,$chunk_size);
			foreach($partitions as $key=>$value)
			{
				$partition_strings[] = implode(";",$value);
			}
		}
		return $partition_strings;
	}
	
	public function filterAndUpdateBatches($multi_query_list, $caller_info = '')
	{
		global $logging_obj;
		$return_status = 1;
		$transaction_status = FALSE;
				
		$multi_query_list = $this->constructBatches($multi_query_list);
		
		if (is_array($multi_query_list))
		if (count($multi_query_list) > 0)
		{
			try
			{

				$this->conn->sql_multi_query("BEGIN");
				foreach($multi_query_list as $key=>$batch_query)
				{
					$logging_obj->my_error_handler("DEBUG","filterAndUpdateBatches \n Batch query = $batch_query \n");

					if ($transaction_status = $this->conn->sql_multi_query($batch_query)) 
					{
					   do {
						   /* store first result set */
						   if ($result = $this->conn->sql_store_result()) 
						   {
							   //do nothing since there's nothing to handle
							   $this->connect->sql_free_result($result);
						   }
						   
						   /* print divider */
						   if ($this->conn->sql_more_result()) 
						   {								
							   //I just kept this since it seems useful
							   //try removing and see for yourself
						   }
					   } while ($this->conn->sql_more_result() && $this->conn->sql_next_result());
					   
						if($this->conn->sql_error())
						{
							throw new Exception("Query execution failed: ".$this->conn->sql_error()." in query : $batch_query","999");
						}
						
					}
					else
					{
						throw new Exception("Transaction failed while executing the query : $batch_query","999");	
					}
				}		
			}
			catch(Exception $e)
			{
				$logging_obj->my_error_handler("INFO","filterAndUpdateBatches::  Message::".$e->getMessage().", File:: ".$e->getFile().", Line::". $e->getLine()."\n");
				$transaction_status = FALSE;
				$return_status = 1;
			}

			/*Transaction to update the database and based on that send the response as true or false*/
			if($transaction_status)
			{
				$commit_status = $this->conn->sql_multi_query("COMMIT");
				#$logging_obj->my_error_handler("DEBUG","filterAndUpdateBatches \n Commited  with status = $commit_status, Transaction status = $transaction_status, called from : $caller_info \n");
				if ($commit_status)
				{
					$return_status =  0;
				}
			}
			else
			{
				$rollback_status = $this->conn->sql_multi_query("ROLLBACK");
				$logging_obj->my_error_handler("INFO",array("TransactionStatus"=>"ROLLBACK"),Log::BOTH);
				$return_status =  1;
			}
		}
		return (bool)$return_status;
	}
	
	//recoveryBatchUpdates renamed as sequentialBatchUpdates
	public function sequentialBatchUpdates($multi_query_list, $caller_info = '')
	{
		global $logging_obj;
		$return_status = 1;
		$transaction_status = FALSE;
		
		$multi_query_list = $this->getBatchQueries($multi_query_list);

		if (is_array($multi_query_list))
		if (count($multi_query_list) > 0)
		{
			try
			{
				$this->conn->sql_multi_query("BEGIN");
				foreach($multi_query_list as $key=>$batch_query)
				{
					$logging_obj->my_error_handler("DEBUG","sequentialBatchUpdates \n Batch query = $batch_query \n");

					if ($transaction_status = $this->conn->sql_multi_query($batch_query)) 
					{
					   do {
							/* store first result set */
						   if ($result = $this->conn->sql_store_result()) 
						   {
							    //do nothing since there's nothing to handle
							   $this->conn->sql_free_result($result);
						   }
						   /* print divider */
						   if ($this->conn->sql_more_result())
						   {
								//I just kept this since it seems useful
							   //try removing and see for yourself
						   }
					   } while ($this->conn->sql_more_result() && $this->conn->sql_next_result());
						
						if($this->conn->sql_error())
						{
							throw new Exception("Query execution failed: ".$this->conn->sql_error()." in query : $batch_query","999");
						}
						
					}
					else
					{
						throw new Exception("Transaction failed while executing the query : $batch_query","999");	
					}
				}
			}
			catch(Exception $e)
			{
				$logging_obj->my_error_handler("INFO","sequentialBatchUpdates::  Message::".$e->getMessage().", File:: ".$e->getFile().", Line::". $e->getLine()."\n");
				$transaction_status = FALSE;
				$return_status = 0;
			}

			/*Transaction to update the database and based on that send the response as true or false*/
			if($transaction_status)
			{
				$commit_status = $this->conn->sql_multi_query("COMMIT");
				#$logging_obj->my_error_handler("INFO","sequentialBatchUpdates:: Commited  with status $commit_status, Transaction status = $transaction_status, called from : $caller_info \n");
				if ($commit_status)
				{
					$return_status =  1;
				}
			}
			else
			{
				$rollback_status = $this->conn->sql_multi_query("ROLLBACK");
				$logging_obj->my_error_handler("INFO",array("TransactionStatus"=>"ROLLBACK"),Log::BOTH);
				$return_status =  0;
			}
		}
		#$logging_obj->my_error_handler("INFO","sequentialBatchUpdates:: return_status $return_status\n");
		return (bool)$return_status;
	}
		
}

?>
