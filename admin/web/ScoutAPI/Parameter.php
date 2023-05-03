<?php
/**
 * Parameter Class
 * @author InMage Systems
 *
 */
class Parameter extends APIObject {
	// Array contains list of name,value pairs 
	private $attrList = array ();
	
	
	// Constructor takes name,value
	public function Parameter($name, $value) {
		$this->setParameterNameValuePair ( $name, $value );
	}
	
	
	/*
	 * Gets the unique key representing the object
	 */
	public function getKey()
	{
		return $this->getName();
	}
	
	
	// Replaces the all the elements including name and value
	public function replace($attrAry) {
		$this->attrList = $attrAry;
	}
	
	/**
	 * Sets the name and value for the given pair
	 * @param String $name
	 * @param String $value
	 */
	public function setParameterNameValuePair($name, $value) {
		$this->attrList ["Name"] = $name;
		$this->attrList ["Value"] = htmlspecialchars($value);
	}
	
	/**
	 * Returns the value for give key
	 * @param String $key
	 * @return String or Object
	 */
	public function getValue() {
		return $this->attrList ["Value"];
	}
	/**
	 * Get Name of the parameter
	 * @return String:
	 */
	public function getName() {
		return $this->attrList ["Name"];
	}
	/**
	 * Add additional attributes of the parameter
	 * @param String $key
	 * @param String $value
	 */
	public function addAdditionalAttribute($key, $value) {
		$this->attrList [$key] = $value;
	}
	
	/**
	 * Returns XML String representating the object
	 * @return string
	 */
	public function getXMLString() {
		$xml = "\n<Parameter";
		foreach ( $this->attrList as $key => $val ) {
			$xml = $xml . " " . $key . "=\"" . $val . "\"";
		}
		$xml = $xml . "/>";
  	return $xml;
  }
  
}
  
?>