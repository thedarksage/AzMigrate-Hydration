<?php
/*
 * $Header: /src/admin/web/app/SwitchConfigurator.php,v 1.41.8.1 2017/08/16 15:57:42 srpatnan Exp $
 *================================================================= 
 * FILENAME
 *    SwitchConfigurator.php
 *
 * DESCRIPTION
 *    This holds all the switch agent back end call specific functions
 *    
 *=================================================================
 *                 Copyright (c) InMage Systems                    
 *=================================================================
 */

if (preg_match('/Windows/i', php_uname()))
{
    include_once "$REPLICATION_ROOT\\admin\\web\\cs\\om_functions.php";
}
else
{
    include_once "$REPLICATION_ROOT/admin/web/om_functions.php";
}

// The following variables will be moved to config.php
$NPORTS = array("table" => "nports", "portWwnCol" => "portWwn");
$APPNPORTS = array("table" => "applianceNports", "portWwnCol" => "appliancePortWwn");
$VIRTUAL_TARGET_BINDINGS = array("table" => "virtualTargetBindings", "portWwnCol" => "virtualTargetBindingPortWwn");
$VIRTUAL_TARGETS = array("table" => "virtualTargets", "portWwnCol" => "virtualTargetPortWwn");
$VIRTUAL_INITIATORS = array("table" => "virtualInitiators", "portWwnCol" => "virtualInitiatorPortWwn");

class SwitchConfigurator
{
    private $conn;

    function __construct()
    {
		global $conn_obj;
        $this->conn = $conn_obj;
        $this->fab_conf = new FabricConfigurator();
		$this->commonfunctions_obj = new CommonFunctions();
    }
}
?>
