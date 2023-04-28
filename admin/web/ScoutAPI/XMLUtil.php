<?php
class XMLUtil {
	
	/*
	 * Parse XML String passed from Buffer
	 */
	public static function parseXML($requestXML) {
		$returnval = false;
		$xml = simplexml_load_string ( $requestXML );
		if ($xml === false)
		{
			return false;
		}
		return XMLUtil::xml2array ( $xml );
	
	}
	
	/*
	 * Parse XML File
	 */
	public static function parseXMLFile($fileName) {
		$xml = simplexml_load_file ( $fileName );
		if ($xml == false)
			return false;
		return XMLUtil::xml2array ( $xml );
	}
	
	/*
	 * Function: Convert an XML to Associative array with
	 * Parameter and ParameterGroup Object where ever they appear
	 */
	public static function xml2array($xml) {
		$arXML = array ();
		$nameE = ucwords ( strtolower ( trim ( $xml->getName () ) ) );
		$valueE = trim ( ( string ) $xml );
		if ((strcasecmp ( $nameE, "Parameter" ) == 0)) {
			$attrAry = array ();
			
			foreach ( $xml->attributes () as $pk => $pv ) {
				$attrAry [$pk] = trim(( string ) $pv);
			}
			$param = new Parameter ( "Name", "Value" );
			$param->replace ( $attrAry );
			return $param;
		} else if ((strcasecmp ( $nameE, "ParameterGroup" ) == 0)) {
			$attrAry;
			foreach ( $xml->attributes () as $pk => $pv ) {
				$attrAry [$pk] = trim(( string ) $pv);
			}
			$params = new ParameterGroup ( $attrAry ["Id"] );
			// Get Child for parameter objects
			foreach ( $xml->children () as $name => $xmlchild ) {
				$nameC = ucwords ( strtolower ( $name ) );
				if ((strcasecmp ( $nameC, "Parameter" ) == 0) || (strcasecmp ( $nameC, "ParameterGroup" ) == 0)) {
					$pObj = XMLUtil::xml2array ( $xmlchild );
					$params->addChild ( $pObj );
				
				}
			}
			return $params;
		} else if ((strcasecmp ( $nameE, "FunctionRequest" ) == 0)) {
			$attrAry;
			$name = "";
			$id = "";
			$include = False;
			foreach ( $xml->attributes () as $pk => $pv ) {
				$pv = trim($pv);
				if (strtolower ( $pk ) == "id") {
					$id = ( string ) $pv;
				}
				if (strtolower ( $pk ) == "name") {
					$name = ( string ) $pv;
				}
				if (strtolower ( $pk ) == "include") {
					$ic = ( string ) $pv;
					if (strtolower ( $ic ) == "y" || strtolower ( $ic ) == "yes" || strtolower ( $ic ) == "true") {
						$include = TRUE;
					}
				}
			}
			
			// Construct the FunctionRequest Object
			

			$functionRequest = new FunctionRequest ( $id, $name, $include );
			
			// Get Child for parameter objects
			foreach ( $xml->children () as $name => $xmlchild ) {
				$nameC = ucwords ( strtolower ( $name ) );
				if ((strcasecmp ( $nameC, "Parameter" ) == 0) || (strcasecmp ( $nameC, "ParameterGroup" ) == 0)) {
					$pObj = XMLUtil::xml2array ( $xmlchild );
					$functionRequest->addChild ( $pObj );
				}
			}
			return $functionRequest;
		} else {
			$t = array ();
			foreach ( $xml->attributes () as $name => $value )
				$t [ucwords ( strtolower ( $name ) )] = trim ( $value );
			$arXML ['name'] = $nameE;
			$arXML ['value'] = $valueE;
			$arXML ['attr'] = $t;
			$t = array ();
			foreach ( $xml->children () as $name => $xmlchild ) {
				$nameC = ucwords ( strtolower ( $name ) );
				if ((strcasecmp ( $nameC, "Parameter" ) == 0) || (strcasecmp ( $nameC, "ParameterGroup" ) == 0) || (strcasecmp ( $nameC, "FunctionRequest" ) == 0)) {
					$pObj = XMLUtil::xml2array ( $xmlchild );
					//TODO:: If Key exists throw Exception
					$className = get_class ( $pObj );
					$objKey = $pObj->getKey ();
					if (array_key_exists ( $objKey, $t )) {
						throw new Exception ( "Duplicate function ID($objKey) exists, Please change functionID to be unique", "1001" );
					} else {
						$t [$objKey] = $pObj;
					}
				} else {
					$t [$nameC] = XMLUtil::xml2array ( $xmlchild );
				}
			}
			$arXML ['children'] = $t;
		}
		return ($arXML);
	}
}
?>