<?php

/**
 * @author Vamsi Indana
 * @copyright 2012
 */

class SecurityException extends Exception{

    function __construct($message, $code = 0){

        parent::__construct($message, $code);
    }

    function displayCompleteMessage(){
        return "File: ".$this->getFile()."\t on Line No: ".$this->getLine().",\t".$this->getMessage();
    }

    function __destruct(){

    }
}
?>