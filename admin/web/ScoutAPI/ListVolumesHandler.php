<?php

class ListVolumesHandler extends FunctionHandler {
	
	/*
 *  Base Function Implementation (FunctionHandler)
 *  Returns true if the ParameterGroup are sufficient 
 */
	public function validate($identity, $version, $paramList) {
		return true;
	}
	
	/*
 *  Base Function Implementation (Handler)
 *  Implements the function show return Associative Array or throw an Exception with ID and Message
 *  	 
 */
	public function process($identity, $version, $functionName, $ParameterGroup) {
//		throw new Exception("From ListVolumeHandler Function Handler","123");
		print(identity);
		return Test::TestData ();
	}

}
?>