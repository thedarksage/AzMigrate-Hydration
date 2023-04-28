<?php
/**
 * Base case for function Level Handler
 */

abstract class FunctionHandler extends Handler {
	/*
 *  Base Function Implementation (FunctionHandler)
 *  Returns true if the ParameterGroup are sufficient 
 */
	abstract protected function validate($identity, $version, $paramList);
}

?>