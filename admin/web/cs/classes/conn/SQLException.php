<?php

/**
 * @author Vamsi Indana
 * @copyright 2014
 */

class SQLException extends Exception{

    function __construct($message, $code = 0){

        parent::__construct($message, $code);
    }

    function getCompleteMessage(){
        return "File: ".$this->getFile()."\t on Line No: ".$this->getLine().",\t".$this->getMessage();
    }

    function getMsgArray(){
        return array('StatusCode' => $this->getCode(), 'Message' => $this->getMessage());
    }

    function __destruct(){

    }
}
?>