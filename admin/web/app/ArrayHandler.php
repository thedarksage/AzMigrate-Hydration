<?php
/*
 * Base Class for all Handler objects
 */
abstract class ArrayHandler {
/*
 *  Base Function Implementation (Handler)
 */
	
	//abstract protected function listTreeView();
	
	public function setConnectionObject()
	{
		global $conn_obj;
		if(!isset($this->conn) || !is_object($this->conn))
		{
			$this->conn = $conn_obj;
		}
	}
}
?>