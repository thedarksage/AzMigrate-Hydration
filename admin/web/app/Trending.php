<?php
/*    
 *$Header: /src/admin/web/app/Trending.php,v 1.149.8.2 2017/08/16 15:57:42 srpatnan Exp $
 *================================================================= 
 * FILENAME
 *   Trending.php
 *
 * DESCRIPTION
 *    This script contains functions related to
 *    Trending
 *
 *=================================================================
 *                 Copyright (c) InMage Systems                    
 *=================================================================
*/
    
class Trending
{
    private $commonfunctions_obj;
    private $installation_dir;
    private $rrdcommand;
    private $nettrends_dir;
	private $conn;
    private $db_handle;
	private $retention_obj;
	private $sparseretention_obj;

    /*
     * Function Name: Trending
     *
     * Description:
     *    This is the constructor of the class 
     *    performs the following actions.
     *
     *    <Give more details here>    
     *
     * Exceptions:
     *     <Specify the type of exception caught>
     */
    function __construct()
    {
        global $INSTALLATION_DIR, $REPLICATION_ROOT, $conn_obj, $CUSTOM_BUILD_TYPE, $RRDTOOL_PATH;
        
        $this->commonfunctions_obj = new CommonFunctions();
        $this->installation_dir = $INSTALLATION_DIR;
		
		if($CUSTOM_BUILD_TYPE != 3)
		{
			if(!$this->commonfunctions_obj->is_linux())
			{
				$this->rrd_install_path = $RRDTOOL_PATH;
				$this->rrd_install_path = addslashes($this->rrd_install_path);
				$this->rrdcommand = $this->rrd_install_path."\\rrdtool\\Release\\rrdtool.exe";
				$this->xml_path = $REPLICATION_ROOT."\\admin\\web\\trends\\xml\\";
			}
			else
			{
				$this->rrdcommand = "rrdtool";
				$this->xml_path = '/home/svsystems/admin/web/trends/xml/';
			}
			if (!is_dir($this->xml_path)) mkdir($this->xml_path);
			 $this->nettrends_dir = '/home/svsystems/admin/web/trends/net';
			if (!is_dir($this->nettrends_dir)) mkdir($this->nettrends_dir);
		}
		$this->conn = $conn_obj;
        $this->db_handle = $this->conn->getConnectionHandle();
    }

	/**
	 * get the cumulative compression and uncompression value
	 * 
	 * @Param: $pid is the valid profilingid
	 *        $type is to check whether cumulative compression or uncompression
	 * 
	 * @return: array (compression , uncompression value)
	 */

	public function getProfilingSummary($pid) 
	{
		global $_;
		global $conn_obj;
		
		$retrunData = array();
		if(!empty($pid)) 
		{
			#Latham & watkins
			$sql = "select count(*) as recordCount, sum(cumulativeCompression) as sumCumulativeComp, sum(cumulativeUnCompression) as sumCumulativeUnComp,avg(cumulativeCompression) as avgCumulativeComp, avg(cumulativeUnCompression) as avgCumulativeUnComp from profilingDetails where profilingId=$pid and forDate>'1970-1-01'";
			$resultSet = $conn_obj->sql_query($sql);
			if (isset($resultSet))
			{
				$obj = $conn_obj->sql_fetch_object($resultSet);
				$retrunData = array('recordCount'=>$obj->recordCount,'sumCumulativeComp'=>$obj->sumCumulativeComp,'sumCumulativeUnComp'=>$obj->sumCumulativeUnComp,'avgCumulativeComp'=>$obj->avgCumulativeComp,'avgCumulativeUnComp'=>$obj->avgCumulativeUnComp);
			}
		}

		return 	$retrunData;
	}

	private function is_cluster($host_id)
	{
		$sql = "SELECT DISTINCT 
					clusterId 
				FROM 
					clusters 
				WHERE 
					hostId='$host_id'";
		$resultSet = $this->conn->sql_query($sql);
		if ($obj = $this->conn->sql_fetch_object($resultSet))
		{
			return $obj->clusterId;						
		}
		else return 0;
	}
};
?>