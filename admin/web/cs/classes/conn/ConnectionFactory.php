<?php

class ConnectionFactory
{
    private $conn_array = array();

    public function ConnectionFactory ($db_type, $db_host, $db_name, $db_user, $db_passwd, $db_options) {
        $conn = new Connection($db_type, $db_host, $db_name, $db_user, $db_passwd, $db_options);
        $this->conn_array[0] = $conn;

        $domain_dao = new DomainDAO();
        $domains = $domain_dao->findAll($this->conn_array[0]);
        foreach ($domains as $domain) {
            $domain_id = $domain->get_domain_id();
            $conn = new Connection($db_type, $db_host, $db_name.$domain_id, $db_user, $db_passwd, $db_options);
            $this->conn_array[$domain_id] = $conn->get_db_link() ? $conn:null;
        }
    }

    public function getConnectionByDomain ($domain_id) {
        return $this->conn_array[$domain_id];
    }
}

?>
