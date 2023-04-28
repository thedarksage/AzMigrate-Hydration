<?php
/**
 * Base Class for all Handler objects
 */
abstract class Handler {
	/*
 *  Base Function Implementation (Handler)
 *  Implements the function show return Associative Array or throw an Exception with ID and Message
 */
	abstract protected function process($identity, $version, $functionName, $ParameterGroup);
}
?>