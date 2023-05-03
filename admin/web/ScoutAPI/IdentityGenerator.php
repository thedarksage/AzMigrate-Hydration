<?php

generateIdValue ( "IntelGR", "PROTECTING GRANDRIVER USING CDP SOLUTION" );
print ("\n-------------------------\n") ;
generateIdValue ( "IntelBR", "PROTECTING BEARRIVER USING CDP SOLUTION" );
print ("\n-------------------------\n") ;
generateIdValue ( "InMage", "SCOUT AN UNIFIED AND MOST COMPREHENSIVE DATA PROTECTION SOLUTION IN THE WORLD" );
print ("\n-------------------------") ;
/**
 * Generates the Identity and Other Credentials
 * @param unknown_type $id
 * @param unknown_type $pp
 */
function generateIdValue($id, $pp) {
	print_r ( "Identifier: " . $id );
	print_r ( "\nAccessKeyID: " . strtoupper ( sha1 ( $id ) ) );
	$pp = strtoupper ( sha1 ( $pp ) );
	print_r ( "\nPassphrase : " . $pp );
}

?>