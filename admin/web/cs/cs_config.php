<?php
$db_options = null;

$conn_factory = new Connection($DB_TYPE, $DB_HOST, $DB_DATABASE_NAME, $DB_USER, $DB_PASSWORD, $db_options);
function get_connection($domain_id) {
    return $conn_factory->getConnectionByDomain($domain_id);
}

?>
