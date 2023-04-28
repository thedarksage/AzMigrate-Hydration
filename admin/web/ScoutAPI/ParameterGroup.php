<?php
class ParameterGroup extends APIContainerObject {
	private $childList = array ();
	private $id;
	/*
	 * Constructor
	 */
	public function ParameterGroup($id) {
		$this->id = $id;
	}
	
	/*
	 * Gets the unique key representing the object
	 */
	public function getKey() {
		return $this->id;
	}
	
	/*
	 * Set the Id of the ParameterGroup object
	 */
	public function setID($id) {
		$this->id = $id;
	}
	
	/*
	 * Get the id of the ParameterGroup Object
	 */
	public function getId() {
		return $this->id;
	}
	
	/*
	 * Returns all the child list
	 */
	
	public function getChildList() {
		return $this->childList;
	}
	
	/*
	 * Set ParameterGroup as child object
	 */
	public function addParameterGroup($obj) {
		$this->addChild ( $obj );
	}
	
	/*
	 * Set parameter as child object
	 */
	public function addParameter($obj) {
		$this->addChild ( $obj );
	}
	
	/*
	 * Set child  ParameterGroup Object
	 */
	public function setParameterNameValuePair($name, $value) {
		
		$parameter = new Parameter ( $name, $value );
		$this->addChild ( $parameter );
	}

	/*
	 * Get value by name in ParameterGroup Object. It is not a recursive function.
	 */
	public function getParameterValue($name) {
		
		$processAry = $this->childList;
		if (count ( $this->childList ) == 0) {
			return false;
		}
		
		foreach ( $processAry as $key => $val ) {
			if (get_class ( $val ) == "ParameterGroup" || $val->getName() != $name)	continue;
			return $val->getValue();
		}

		return false;
	}
	
	/*
	 *  getChild give the key
	 */
	public function getChild($key) {
		return $this->childList [$key];
	}
	
	/*
	 * Returns to XML String using recursive build
	 * Some flattening logic
	 */
	public function getXMLString() {
		// 1. If there is only element in the array and it is of type "ParameterGroup" then you enumerate the child objects are root
		$processAry = $this->childList;
		if (count ( $this->childList ) == 0) {
			return;
		}
		
		$xml = "\n<ParameterGroup Id=\"" . $this->id . "\">";
		foreach ( $processAry as $key => $val ) {
			$xml = $xml . $val->getXMLString ();
		}
		$xml = $xml . "\n</ParameterGroup>";
		return $xml;
	}
	
	/*
	 *  Add a ParameterGroup or Parameter Child Object
	 */
	public function addChild($obj) {
	$className = get_class ( $obj );
		if ( $className == "ParameterGroup" || $className == "Parameter"  ) {
			$objKey = $obj->getKey ();
			if (isset ( $objKey )) {
				if (array_key_exists ( $objKey, $this->childList )) {
					throw new Exception ( "Duplicate $className exists for key ($objKey)", "1001" );
				} else {
					$this->childList [$objKey] = $obj;
				}
			} else {
				throw new Exception ( "$className passed doesn't have a Id/Name", "1002" );
			}
		} else {
			throw new Exception ( "Invalid Object/Element Passed: " . get_class ( $obj ) . " Valid Types are Parameter or ParameterGroup.", "1003" );
		}
	}
}

?>