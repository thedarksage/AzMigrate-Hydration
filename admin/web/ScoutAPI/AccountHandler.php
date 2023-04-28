<?php
class AccountHandler extends ResourceHandler 
{
	private $conn;
	
	public function AccountHandler() 
	{
        global $conn_obj;
		$this->conn = $conn_obj;
		$this->account_obj = new Accounts();
	}	
	
	public function CreateAccount_validate($identity, $version, $functionName, $parameters)
	{
		try
		{
			// Enable exceptions for DB.
			$this->conn->enable_exceptions();            
            if(isset($parameters['Accounts']))
			{
				$accounts_obj = $parameters['Accounts']->getChildList();
				
				// Validating Accounts parameter group exists or not.
				if((is_array($accounts_obj) === FALSE) || (count($accounts_obj) == 0))	ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "Accounts"));
				
				// Fetching all existing accounts
				$existing_accounts = $this->account_obj->getAccounts();
				foreach($accounts_obj as $account_key => $accounts_data)
				{					
					$account_info_obj = $accounts_data->getChildList();
					
					// In case AccountName parameter does not exist.
					if(!isset($account_info_obj['AccountName']))	ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => $account_key." - AccountName"));
					
					$account_name = trim($account_info_obj['AccountName']->getValue());
					if(!trim($account_name))	ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => $account_key." - AccountName"));
					
					// In case account exist with the requested account name throw error.
					if(in_array($account_name, $existing_accounts))
					ErrorCodeGenerator::raiseError($functionName, 'EC_ACCOUNT_EXIST', array('AccountName' => $account_name));
					
					// In case UserName parameter does not exist
					if(!isset($account_info_obj['UserName']))	ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => $account_key." - UserName"));
					
					$user_name = trim($account_info_obj['UserName']->getValue());
					if(!trim($user_name)) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => $account_key." - UserName"));
					
					if(isset($account_info_obj['Domain']))
					{
						$domain = trim($account_info_obj['Domain']->getValue());
						if(!trim($domain)) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => $account_key." - Domain"));
					}
					
					if(isset($account_info_obj['AccountType']))
					{
						$account_type = trim($account_info_obj['AccountType']->getValue());
						if(!trim($account_type)) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => $account_key." - AccountType"));
					}
					
					// In case Password parameter does not exist
					if(!isset($account_info_obj['Password'])) ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => $account_key." - Password"));
					
					$password = trim($account_info_obj['Password']->getValue());
					if(!trim($password)) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => $account_key." - Password"));
                }
            }
			else
			{
				ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "Accounts"));
			}
			// Disable Exceptions for DB
			$this->conn->disable_exceptions();
		}
		catch(SQLException $sql_excep)
		{
			// Disable Exceptions for DB
			$this->conn->disable_exceptions();			
			ErrorCodeGenerator::raiseError('COMMON', 'EC_DATABASE_ERROR', array(), "Database error occurred");
		}
		catch (APIException $apiException)
	    {
			// Disable Exceptions for DB
			$this->conn->disable_exceptions();           
			throw $apiException;
	    }
		catch(Exception $excep)
		{
			// Disable Exceptions for DB
			$this->conn->disable_exceptions();		
			ErrorCodeGenerator::raiseError('COMMON', 'EC_INTERNAL_ERROR', array(), "Internal server error");
		}
	}
	
	public function CreateAccount($identity, $version, $functionName, $parameters)
	{
		try
		{
			// Enable exceptions for DB.
			$this->conn->enable_exceptions();            
            $this->conn->sql_begin();
			
			$response = new ParameterGroup("Response");
			if(isset($parameters['Accounts']))
			{
				$accounts_obj = $parameters['Accounts']->getChildList();
				$account_details = $account_info = array();
				foreach($accounts_obj as $account_key => $accounts_data)
				{					
					$account_info_obj = $accounts_data->getChildList();
					
					$account_details['AccountName'] = trim($account_info_obj['AccountName']->getValue());
					$account_details['UserName'] = trim($account_info_obj['UserName']->getValue());
					$account_details['Password'] = trim($account_info_obj['Password']->getValue());
					$account_details['Domain'] = (isset($account_info_obj['Domain'])) ? trim($account_info_obj['Domain']->getValue()) : '';
					$account_details['AccountType'] = (isset($account_info_obj['AccountType'])) ? trim($account_info_obj['AccountType']->getValue()) : '';
					
					// Add accounts to CS DB
					$this->account_obj->addAccounts($account_details);
                }
            }
			$response->setParameterNameValuePair($functionName, "Success");
			
            // Disable Exceptions for DB
            $this->conn->sql_commit();
			$this->conn->disable_exceptions();
			return $response->getChildList();
		}
		catch(SQLException $sql_excep)
		{
			// Disable Exceptions for DB
			$this->conn->sql_rollback();
			$this->conn->disable_exceptions();			
			ErrorCodeGenerator::raiseError('COMMON', 'EC_DATABASE_ERROR', array(), "Database error occurred");
		}
		catch (APIException $apiException)
	    {
			// Disable Exceptions for DB
			$this->conn->disable_exceptions();           
			throw $apiException;
	    }
		catch(Exception $excep)
		{
			// Disable Exceptions for DB
			$this->conn->sql_rollback();
			$this->conn->disable_exceptions();		
			ErrorCodeGenerator::raiseError('COMMON', 'EC_INTERNAL_ERROR', array(), "Internal server error");
		}
	}
	
	public function UpdateAccount_validate($identity, $version, $functionName, $parameters)
	{
		try
		{
			// Enable exceptions for DB.
			$this->conn->enable_exceptions();            
            
            if(isset($parameters['Accounts']))
			{
				$accounts_obj = $parameters['Accounts']->getChildList();
				
				// Validating Accounts parameter group exists or not.
				if((is_array($accounts_obj) === FALSE) || (count($accounts_obj) == 0))	ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "Accounts"));
				
				// Fetching all the account information.
				$existing_accounts = $this->account_obj->getAccounts();
				
				foreach($accounts_obj as $account_key => $accounts_data)
				{					
					$account_info_obj = $accounts_data->getChildList();
			
					// In case AccountId parameter does not exist.
					if(!isset($account_info_obj['AccountId']))	ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => $account_key." - AccountId"));
					
					$account_id = trim($account_info_obj['AccountId']->getValue());
					if(!trim($account_id))	ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => $account_key." - AccountId"));
					
					// In case account exist with the requested account name throw error.
					if(!array_key_exists($account_id, $existing_accounts))
					ErrorCodeGenerator::raiseError($functionName, 'EC_NO_ACCOUNT_EXIST', array('AccountId' => $account_id));
					
					// In case AccountName parameter exist.
					if(isset($account_info_obj['AccountName'])) 
					{
						$account_name = trim($account_info_obj['AccountName']->getValue());
						if(!trim($account_name))	ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => $account_key." - AccountName"));
						
						$prev_acct_name = $existing_accounts[$account_id];
						// In case account exist with the requested account name throw error.
						if(in_array($account_name, $existing_accounts) && $prev_acct_name != $account_name)
						ErrorCodeGenerator::raiseError($functionName, 'EC_ACCOUNT_EXIST', array('AccountName' => $account_name));
					}
					
					// In case UserName parameter does not exist
					if(isset($account_info_obj['UserName'])) 
					{
						$user_name = trim($account_info_obj['UserName']->getValue());
						if(!trim($user_name))	ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => $account_key." - UserName"));
					}	
					
					// In case domain parameter  exist
					if(isset($account_info_obj['Domain']))
					{
						$domain = trim($account_info_obj['Domain']->getValue());
						if(!trim($domain))	ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => $account_key." - Domain"));
					}
					
					if(isset($account_info_obj['AccountType']))
					{
						$account_type = trim($account_info_obj['AccountType']->getValue());
						if(!trim($account_type)) ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => $account_key." - AccountType"));
					}
					
					// In case Password parameter exist
					if(isset($account_info_obj['Password']))
					{
						$password = trim($account_info_obj['Password']->getValue());
						if(!trim($password))	ErrorCodeGenerator::raiseError('COMMON', 'EC_NO_DATA', array('Parameter' => $account_key." - Password"));
					}	
                }
            }
			else
			{
				ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "Accounts"));
			}
			// Disable Exceptions for DB
			$this->conn->disable_exceptions();
		}
		catch(SQLException $sql_excep)
		{
			// Disable Exceptions for DB
			$this->conn->disable_exceptions();			
			ErrorCodeGenerator::raiseError('COMMON', 'EC_DATABASE_ERROR', array(), "Database error occurred");
		}
		catch (APIException $apiException)
	    {
			// Disable Exceptions for DB
			$this->conn->disable_exceptions();           
			throw $apiException;
	    }
		catch(Exception $excep)
		{
			// Disable Exceptions for DB
			$this->conn->disable_exceptions();		
			ErrorCodeGenerator::raiseError('COMMON', 'EC_INTERNAL_ERROR', array(), "Internal server error");
		}
	}

	public function UpdateAccount($identity, $version, $functionName, $parameters)
	{
		try
		{
			// Enable exceptions for DB.
			$this->conn->enable_exceptions();            
            $this->conn->sql_begin();
			
			$response = new ParameterGroup("Response");
            if(isset($parameters['Accounts']))
			{
				$accounts_obj = $parameters['Accounts']->getChildList();
				$account_details = array();
				
				// Validating Accounts parameter group exists or not.
				if((is_array($accounts_obj) === FALSE) || (count($accounts_obj) == 0))	ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "Accounts"));
				
				foreach($accounts_obj as $account_key => $accounts_data)
				{					
					$account_info_obj = $accounts_data->getChildList();
					
					$account_details['AccountName'] = (isset($account_info_obj['AccountName'])) ? trim($account_info_obj['AccountName']->getValue()) : '';
					$account_details['UserName'] = (isset($account_info_obj['UserName'])) ? trim($account_info_obj['UserName']->getValue()) : '';
					$account_details['Password'] = (isset($account_info_obj['Password'])) ? trim($account_info_obj['Password']->getValue()) : '';
					$account_details['Domain'] = (isset($account_info_obj['Domain'])) ? trim($account_info_obj['Domain']->getValue()) : '';
					$account_details['AccountType'] = (isset($account_info_obj['AccountType'])) ? trim($account_info_obj['AccountType']->getValue()) : '';
					$account_details['AccountId'] = trim($account_info_obj['AccountId']->getValue());
					
					$this->account_obj->updateAccounts($account_details);								
                }
            }
			
			$response->setParameterNameValuePair($functionName, "Success");
			
            // Disable Exceptions for DB
            $this->conn->sql_commit();
			$this->conn->disable_exceptions();
			return $response->getChildList();
		}
		catch(SQLException $sql_excep)
		{
			// Disable Exceptions for DB
			$this->conn->sql_rollback();
			$this->conn->disable_exceptions();			
			ErrorCodeGenerator::raiseError('COMMON', 'EC_DATABASE_ERROR', array(), "Database error occurred");
		}
		catch (APIException $apiException)
	    {
			// Disable Exceptions for DB
			$this->conn->disable_exceptions();           
			throw $apiException;
	    }
		catch(Exception $excep)
		{
			// Disable Exceptions for DB
			$this->conn->sql_rollback();
			$this->conn->disable_exceptions();		
			ErrorCodeGenerator::raiseError('COMMON', 'EC_INTERNAL_ERROR', array(), "Internal server error");
		}
	}
	
	public function ListAccounts_validate($identity, $version, $functionName, $parameters)
	{
		// No Need to validate any input parameters.
		return;		
	}
	
	public function ListAccounts($identity, $version, $functionName, $parameters)
	{
		try
		{
			// Enable exceptions for DB.
			$this->conn->enable_exceptions();
			
			$response = new ParameterGroup("Response");
			$account_group = new ParameterGroup ("Accounts");
			$account_details = $this->account_obj->getAccountDetails();
            
            if(is_array($account_details))
			{
				$acc_count = 1;
				foreach($account_details as $account_key => $accounts_data)
				{					
					$account_data = new ParameterGroup ("Account".$acc_count);
					
					$account_data->setParameterNameValuePair('AccountId', $accounts_data['accountId']);
					$account_data->setParameterNameValuePair('AccountName', $accounts_data['accountName']);
					$account_data->setParameterNameValuePair('UserName', $accounts_data['userName']);
					$account_data->setParameterNameValuePair('Domain', $accounts_data['domain']);
					$account_data->setParameterNameValuePair('Password', $accounts_data['password']);
					$account_data->setParameterNameValuePair('AccountType', $accounts_data['accountType']);
					
					$account_group->addParameterGroup ($account_data);
					$acc_count++;
                }
            }
			$response->addParameterGroup ($account_group);
			
            // Disable Exceptions for DB
			$this->conn->disable_exceptions();
			return $response->getChildList();
		}
		catch(SQLException $sql_excep)
		{
			// Disable Exceptions for DB
			$this->conn->disable_exceptions();						
			ErrorCodeGenerator::raiseError('COMMON', 'EC_DATABASE_ERROR', array(), "Database error occurred");
		}
		catch (APIException $apiException)
	    {
			// Disable Exceptions for DB
			$this->conn->disable_exceptions();           
			throw $apiException;
	    }
		catch(Exception $excep)
		{
			// Disable Exceptions for DB
			$this->conn->disable_exceptions();		
			ErrorCodeGenerator::raiseError('COMMON', 'EC_INTERNAL_ERROR', array(), "Internal server error");
		}
	}
	
	public function RemoveAccount_validate($identity, $version, $functionName, $parameters)
	{
		try
		{
			// Enable exceptions for DB.
			$this->conn->enable_exceptions();
			
			if(isset($parameters['Accounts']))
			{				
				$account_id_arr = array();
				$accounts_obj = $parameters['Accounts']->getChildList();
				
				// Validating AccountId parameter if Accounts parameters group exists
				if((is_array($accounts_obj) === FALSE) || (count($accounts_obj) == 0))	ErrorCodeGenerator::raiseError('COMMON', 'EC_INSFFCNT_PRMTRS', array('Parameter' => "Accounts"));
				
				$existing_acc_id = $this->account_obj->getAccounts();
				for($i=1; $i<=count($accounts_obj); $i++)
				{
					//Validating AccountId parameter for empty value
					$account_id = $this->validateParameter($accounts_obj,'AccountId'.$i,FALSE);
					$account_id_arr[] = $account_id;
					
					// Validate whether given account exist in DB or not
					if(!array_key_exists($account_id, $existing_acc_id))  ErrorCodeGenerator::raiseError($functionName, 'EC_NO_ACCOUNT_EXIST', array('AccountId' => $account_id));
				}
				
				$associated_acc_id = $this->account_obj->getAssociatedAccounts();
				if(count($associated_acc_id)> 0)
				{
					foreach($account_id_arr as $key => $acc_id)
					{
						// Validate the given account id is associated with discovery or push install
						if(in_array($acc_id, $associated_acc_id))
							ErrorCodeGenerator::raiseError($functionName, 'EC_DATA_EXIST', array('AccountId' => $acc_id));
					}
				}
			}
            
			// Disable Exceptions for DB
			$this->conn->disable_exceptions();
		}
		catch(SQLException $sql_excep)
		{
			// Disable Exceptions for DB
			$this->conn->disable_exceptions();			
			ErrorCodeGenerator::raiseError('COMMON', 'EC_DATABASE_ERROR', array(), "Database error occurred");
		}
		catch (APIException $apiException)
	    {
			// Disable Exceptions for DB
			$this->conn->disable_exceptions();           
			throw $apiException;
	    }
		catch(Exception $excep)
		{
			// Disable Exceptions for DB
			$this->conn->disable_exceptions();		
			ErrorCodeGenerator::raiseError('COMMON', 'EC_INTERNAL_ERROR', array(), "Internal server error");
		}
	}
	
	public function RemoveAccount($identity, $version, $functionName, $parameters)
	{
		try
		{
			// Enable exceptions for DB.
			$this->conn->enable_exceptions();
			$this->conn->sql_begin();
			
			$response = new ParameterGroup("Response");
			
			if(isset($parameters['Accounts']))
			{				
				$account_id_arr = array();
				$accounts_obj = $parameters['Accounts']->getChildList();
				
				for($i=1; $i<=count($accounts_obj); $i++)
				{            
					//Validating AccountId parameter for empty value
					$account_id = trim($accounts_obj['AccountId'.$i]->getValue());
					$account_id_arr[] = $account_id;
				}
				$account_ids = implode(",", $account_id_arr);
				
				$this->account_obj->deleteAccounts($account_ids);
			}
			
            $response->setParameterNameValuePair($functionName, "Success");
			
            // Disable Exceptions for DB
            $this->conn->sql_commit();
			$this->conn->disable_exceptions();
			return $response->getChildList();
		}
		catch(SQLException $sql_excep)
		{
			// Disable Exceptions for DB
			$this->conn->sql_rollback();
			$this->conn->disable_exceptions();			
			ErrorCodeGenerator::raiseError('COMMON', 'EC_DATABASE_ERROR', array(), "Database error occurred");
		}
		catch (APIException $apiException)
	    {
			// Disable Exceptions for DB
			$this->conn->disable_exceptions();           
			throw $apiException;
	    }
		catch(Exception $excep)
		{
			// Disable Exceptions for DB
			$this->conn->sql_rollback();
			$this->conn->disable_exceptions();		
			ErrorCodeGenerator::raiseError('COMMON', 'EC_INTERNAL_ERROR', array(), "Internal server error");
		}
	}
}
?>