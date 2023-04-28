<?php
class FunctionRequest extends  APIContainerObject {
	private $id = "";
	private $name = "";
	private $include = false;
	private $childList = array ();
	
	/*
 * Default Constructor
 */
	public function FunctionRequest($id, $name, $include = false) {
		$this->id = $id;
		$this->name = $name;
		$this->include = $include;
	}
	
	/*
	 * Gets the unique key representing the object
	 */
	public function getKey()
	{
		return $this->id;
	}
	
	
	/*
  * Return function id
  */
	public function getID() {
		return $this->id;
	}
	/*
  *  Returns Function Name
  */
	public function getName() {
		return $this->name;
	}
	
	/*
  *  Return Function Include
  */
	public function getInclude() {
		return $this->include;
	}
	
	/*
  *  Returns ChildList of Parameter or ParameterGroup Objects 
  */
	public function getChildList() {
		return $this->childList;
	}
	
	/*
	 *  Gets XML String representation of the object
	 */
	public function getXMLString() {
		
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
