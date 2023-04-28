<?php
class SimulatorHandler extends Handler {
	
	private $response;
	
/*
 *  Base Function Implementation (FunctionHandler)
 *  Returns true if the ParameterGroup are sufficient 
 */
	public function validate($identity, $version, $ParameterGroup) {
		return true;
	}
	
	public function setResponse($response) {
		$this->response = $response;
	}
	
/*
 *  Base Function Implementation (Handler)
 *  Implements the function show return Associative Array or throw an Exception with ID and Message
 *  Overwrite the process
 *  	 
 */
	public function process($identity, $version, $functionName, $ParameterGroup) {
		// Write the requests into a file and read responses from file
		

		$fileName = $functionName . "_" . "Response.xml";
		$fileName = "stubs/$fileName";
		if (file_exists ( $fileName )) {
			$ary = XMLUtil::parseXMLFile ( $fileName );
			return $ary ["children"]["Functionresponse"]["children"];
		}
		return $this->response;
	}

}

?>