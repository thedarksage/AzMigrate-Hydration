<?php
class xenInfo
{
   /*
     $db_handle: database connection object
   */

    private $commonfunctions_obj;
    private $conn;
    private $db_handle;
	private $volumeProtection_obj;

    /*
     * Function Name: VolumeProtection
     *
     * Description:
     *               Constructor
     *               It initializes the private variable
    */
    function __construct()
    {
		global $conn_obj;
        $this->commonfunctions_obj = new CommonFunctions();
		$this->volumeProtection_obj = new VolumeProtection();
		$this->conn = $conn_obj;
        $this->db_handle = $this->conn->getConnectionHandle();
    }
}
?>