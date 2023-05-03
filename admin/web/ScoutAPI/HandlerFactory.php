<?php
/**
 * Returns the Handler for function name
 */
class HandlerFactory {
	/*
	 *  Get Handler
	 */
	public static function getHandler($version,$functionName) {
		/*
 	  * 1. Retrieve function name and version
 	  * 2. Check if there is any function handler
 	  * 	2.1 retrieve function handler
 	  * 	2.3 if found, call validate function
 	  * 	2.4 return handler
 	  * 3. If there is no functional handler, check for resource handler
 	  * 	3.1 retrive resource name corresponding the method
 	  *     3.2 Check for resource handler
 	  *     3.3 Check if resource handler, implements the corresponding function
 	  *     3.4 Return the resource handler
 	  * 4. If in simulation mode, return default handler    
 	  */
		
			
		// Get API Data for processing
		 $apidata = new APIProcessData ();
		
		// Check, if it is in forced simulation mode
		if (strcasecmp ( $apidata->getMode (), "ForceSimulation" ) == 0) {
			$simulatorHandler = new SimulatorHandler ();
			return $simulatorHandler;
		}
		
		// Check of Function Handlers
		$functionHandlers = $apidata->getFunctionHandlers ();
		
		// Return function handler, if found 
		if (array_key_exists ( $functionName, $functionHandlers )) {
			$handleClassName = $functionHandlers [$functionName];
			return new $handleClassName ();
		}
		
		// Check of Resource Handlers
		
		$resourceHandlers = $apidata->getResourceHandlers ();
		$handleCode = $apidata->getResource ( $functionName );
		// Return function handler, if found 
		if (array_key_exists ( $handleCode, $resourceHandlers )) {
			$handleClassName = $resourceHandlers [$handleCode];
			$obj = new $handleClassName ();
			if ($obj->isFunctionSupported ( $functionName )) {
				// Only return if the method is supported
				return new $handleClassName ();
			}
		}
		
		// If in simulation mode run simulator
		if (strcasecmp ( $apidata->getMode (), "Simulation" ) == 0) {
			$simulatorHandler = new SimulatorHandler ();
			
			return $simulatorHandler;
		}		
	}
}
?>