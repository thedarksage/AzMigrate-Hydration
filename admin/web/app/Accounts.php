<?php
  /*
 *$Header: 
 *================================================================= 
 * FILENAME
 *    Accounts.php
 *
 * DESCRIPTION
 *    This script contains functions related to
 *    Accounts
 *=================================================================
 *                 Copyright (c) InMage Systems                    
 *=================================================================
*/
class Accounts
{
    private $conn_obj;
	private $commonfunctions_obj;

   /*
     * Function Name: Accounts
     *
     * Description:
     *    This is the constructor of the class 
     *    performs the following actions.
     *
     *
     * Parameters: None 
     *
     * Return Value:None
     *
     * Exceptions:None
     *    
     */
	function __construct()
	{
		global $conn_obj;
		
		$this->conn = $conn_obj;
		$this->commonfunctions_obj = new CommonFunctions();
		$this->encryption_key = $this->commonfunctions_obj->getCSEncryptionKey();
	}
	
	public function addAccounts($account_details)
	{
		$sql_args = array();
		$account_name = $account_details["AccountName"];
		$user_name = $account_details["UserName"];
		$domain = $account_details["Domain"];
		$account_type = $account_details["AccountType"];
		$password = $account_details["Password"];
		
		$domain_sql = "IF(? = '', ?,  HEX(AES_ENCRYPT(?, ?)))";
		
		$account_sql = "INSERT INTO 
						`accounts`
						(
							accountName,
							userName,
							password,
							domain,
							accountType
						)
						VALUES
						(
							?,
							HEX(AES_ENCRYPT(?, ?)),
							HEX(AES_ENCRYPT(?, ?)),
							{$domain_sql},
							?
						)";
		array_push($sql_args, $account_name, $user_name, $this->encryption_key, $password, $this->encryption_key, $domain, $domain, $domain, $this->encryption_key, $account_type);
		
		$this->conn->sql_query($account_sql, $sql_args);
	}
	
	public function updateAccounts($account_details)
	{
		$sql_args = array();
		$cond = "";
		
		$account_name = $account_details["AccountName"];
		$user_name = $account_details["UserName"];
		$domain = $account_details["Domain"];
		$password = $account_details["Password"];
		$account_id = $account_details['AccountId'];
		$account_type = $account_details['AccountType'];
		
		if($account_name)
		{
			$cond = "accountName = ?";
			array_push($sql_args, $account_name);
		}
		if($user_name)
		{
			$cond .= ($cond) ? ", userName = HEX(AES_ENCRYPT(?, ?))" : "userName = HEX(AES_ENCRYPT(?, ?))";
			array_push($sql_args, $user_name, $this->encryption_key);
		}
		if($password)
		{
			$cond .= ($cond) ? ", password = HEX(AES_ENCRYPT(?, ?))" : "password = HEX(AES_ENCRYPT(?, ?))";
			array_push($sql_args, $password, $this->encryption_key);
		}
		if($domain)
		{
			$cond .= ($cond) ? ", domain = HEX(AES_ENCRYPT(?, ?))" : "domain = HEX(AES_ENCRYPT(?, ?))";
			array_push($sql_args, $domain, $this->encryption_key);
		}
		if($account_type)
		{
			$cond .= ($cond) ? ", accountType = ?" : "accountType = ?";
			array_push($sql_args, $account_type);
		}	
	
		$account_sql = "UPDATE 
							`accounts` 
						SET 
							$cond
						WHERE 
							accountId = ?";
		
		array_push($sql_args, $account_id);
		$this->conn->sql_query($account_sql, $sql_args);
	}
	
	public function deleteAccounts($account_id)
	{
		$sql = "DELETE FROM accounts WHERE FIND_IN_SET(accountId, ?)";
		$this->conn->sql_query($sql, array($account_id));
	}
	
	public function getAccounts()
	{
		$account_details = $this->conn->sql_get_array("SELECT accountName, accountId FROM accounts","accountId","accountName", array());
		return $account_details;
	}
	
	public function getAccountDetails($account_id = '')
	{
		$sql_args = array($this->encryption_key, $this->encryption_key, $this->encryption_key);
		$cond = 1;
		if($account_id) 
		{
			$cond = "accountId = ?";
			array_push($sql_args, $account_id);
		}
		$account_sql = "SELECT 
							accountId,
							accountName,
							AES_DECRYPT(UNHEX(userName),?) as userName,
							IF (ISNULL(domain) OR domain = '', domain, AES_DECRYPT(UNHEX(domain), ?)) as domain,
							AES_DECRYPT(UNHEX(password),?) as password,
							accountType
						FROM 
							accounts
						WHERE
							$cond";
		$account_details = $this->conn->sql_query($account_sql, $sql_args);
		return $account_details;
	}
	
	public function getAssociatedAccounts()
	{
		$inv_account_ids = $agent_account_ids = array();
		$inv_account_ids = $this->conn->sql_get_array("SELECT accountId FROM infrastructureInventory","accountId","accountId", array());
		$agent_account_ids = $this->conn->sql_get_array("SELECT accountId FROM agentInstallers","accountId","accountId", array());
		
		$account_id_arr = array_merge($inv_account_ids, $agent_account_ids);
		return $account_id_arr;
	}
    
    public function validateAccount($account_id)
    {
        $account_exists = $this->conn->sql_get_value("accounts", "accountId", "accountId = ?", array($account_id));
        return $account_exists;
    }
};
?>
