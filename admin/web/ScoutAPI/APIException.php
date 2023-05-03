<?php

/**
 * @author Vamsi Indana
 * @copyright 2012
 */

class APIException extends Exception{

	private $errResponse;

    function __construct($message, $code = 0, $errResponse){

        parent::__construct($message, $code);
		$this->errResponse = $errResponse;
    }

    function displayCompleteMessage(){
        return "File: ".$this->getFile()."\t on Line No: ".$this->getLine().",\t".$this->getMessage();
    }
	
	function getErrorResponse()
	{
		return $this->errResponse;
	}

    function __destruct(){

    }
}

?>