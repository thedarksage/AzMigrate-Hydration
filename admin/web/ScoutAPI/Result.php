<?php

/**
 * A Wrapper Object for Result
 * @author saav
 *
 */
class Result {
	private $returnCode = 0;
	private $message ="";
	private $request;
	private $response;
	/**
	 * @return the $returnCode
	 */
	public function getReturnCode() {
		return $this->returnCode;
	}

	/**
	 * @return the $message
	 */
	public function getMessage() {
		return $this->message;
	}

	/**
	 * @return the $request
	 */
	public function getRequest() {
		return $this->request;
	}

	/**
	 * @return the $response
	 */
	public function getResponse() {
		return $this->response;
	}

	/**
	 * @param field_type $returnCode
	 */
	public function setReturnCode($returnCode) {
		$this->returnCode = $returnCode;
	}

	/**
	 * @param field_type $message
	 */
	public function setMessage($message) {
		$this->message = $message;
	}

	/**
	 * @param field_type $request
	 */
	public function setRequest($request) {
		$this->request = $request;
	}

	/**
	 * @param field_type $response
	 */
	public function setResponse($response) {
		$this->response = $response;
	}

	
	
	

}

?>