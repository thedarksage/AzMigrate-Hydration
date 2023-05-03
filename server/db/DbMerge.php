<?php
/**
 *$Header: /src/server/db/DbMerge.php,v 1.12.16.1 2017/11/22 09:39:23 srpatnan Exp $
 *================================================================= 
 * FILENAME
 *    DbMerge.php
 *
 * DESCRIPTION
 *    Compare the schemas of two databases and generate the SQL diff.
 *
 * HISTORY
 *    Aug 09, 2012 - Padhi - Created 
 *=================================================================
 *                 Copyright (c) InMage Systems                    
 *=================================================================
 */

 
class DbMerge {

	private $upgrade_sql = array();
	private $upgrade_sql_categorized = array();
	
	public function generateSql($config_source, $config_target, $manual_data_tables='', $sync_triggers=false, $exclude_tables=array()) {
	
		$this->_storeSQL('SET FOREIGN_KEY_CHECKS = 0');
		
		// Generate DDL Statements
		$s_tab = $this->fetchTables($config_source);
		$t_tab = $this->fetchTables($config_target);	
		
		if ( is_array($t_tab) ) {
			foreach ( $t_tab AS $key => $value ) {
				if(!in_array($key, $exclude_tables)) {
					if ( !isset($s_tab[$key]) ) {
						$this->createTable($config_target, $key);
					}
				}
			}
		}
		
		if ( is_array($s_tab) ) {
			foreach ( $s_tab AS $key => $value ) {
				if(!in_array($key, $exclude_tables)) {
					if ( isset($t_tab[$key]) ) {
						$this->alterTable($s_tab[$key], $t_tab[$key]);
					} else {
						$this->dropTable($key);
					}
				}
			}
		}
		
		// Generate DML Statements
		if($manual_data_tables) {

			// For Full-Sync Tables
			$manual_tables_full_sync = $manual_data_tables['full_sync'];
			$manual_tables_full_sync_keys = array_keys($manual_tables_full_sync);
			
			if ( is_array($t_tab) ) {
				foreach ( $t_tab AS $key => $value ) {
					if(!in_array($key, $exclude_tables)) {
						if(in_array($key, $manual_tables_full_sync_keys)) {
							if ( !isset($s_tab[$key]) ) {
								$this->insertAllRecords($config_target, $key);
							} elseif ($this->getTableChecksum($config_source, $key) != $this->getTableChecksum($config_target, $key)) {
								$this->compareAllRecords($config_source, $config_target, $key, $manual_tables_full_sync[$key]['pk'], $manual_tables_full_sync[$key]['fields'], $manual_tables_full_sync[$key]['exclude']);
							}
						}
					}
				}
			}
			
			// For Partial-Sync Tables
			$manual_tables_partial_sync = $manual_data_tables['partial_sync'];
			$manual_tables_partial_sync_keys = array_keys($manual_tables_partial_sync);
			if ( is_array($t_tab) ) {
				foreach ( $t_tab AS $key => $value ) {
					if(!in_array($key, $exclude_tables)) {
						if(in_array($key, $manual_tables_partial_sync_keys)) {
							
							if ( !isset($s_tab[$key]) ) {
								$this->insertAllRecords($config_target, $key);
							} else {
								$this->compareAllRecords($config_source, $config_target, $key, $manual_tables_partial_sync[$key]['pk'], $manual_tables_partial_sync[$key]['fields'], $manual_tables_partial_sync[$key]['exclude']);
							}
						}
					}
				}
			}
		}
		
		// Generate DDL (Trigger) Statements
		if($sync_triggers) {
			$s_trg = $this->fetchTriggers($config_source);
			$t_trg = $this->fetchTriggers($config_target);
									
			if ( is_array($t_trg) ) {
				foreach ( $t_trg AS $key => $value ) {
					if ( !isset($s_trg[$key]) ) {
						$this->createTrigger($value);
					} else {
						
						$s_trig = $t_trig = '';
						foreach($s_trg[$key] as $field) $s_trig .= '_'.$field;
						foreach($t_trg[$key] as $field) $t_trig .= '_'.$field;
						
						if(crc32($s_trig) != crc32($t_trig)) {
							$this->dropTrigger($value);
							$this->createTrigger($value);
						}
					}
				}
			}
			
			if ( is_array($s_trg) ) {
				foreach ( $s_trg AS $key => $value ) {
					if ( !isset($t_trg[$key]) ) {
						$this->dropTrigger($value);
					}
				}
			}
		}		
		
		$this->_storeSQL('SET FOREIGN_KEY_CHECKS = 1');
		
		return array($this->upgrade_sql, $this->upgrade_sql_categorized);
	}
	
	private function fetchTableData($config, $tbl) {
		$db = mysqli_connect($config['host'], $config['user'], $config['password']);
		if (!$db) { return null; }
		if (!mysqli_select_db($db, $config['name'])) { return null; }

		$data = array();
		if($result = mysqli_query($db,"SELECT * FROM `{$tbl}`")) {
			while ($row = mysqli_fetch_array($result, MYSQLI_ASSOC)) {
				$data[] = $row;
			}
		}

		return $data;
	}
	
    private function getTableChecksum($config, $tbl) {
		$db = mysqli_connect($config['host'], $config['user'], $config['password']);
		if (!$db) { return null; }
		if (!mysqli_select_db($db,$config['name'])) { return null; }
		
		$result = mysqli_query($db,"CHECKSUM TABLE `{$tbl}`");
		$row = mysqli_fetch_array($result);
		
		return $row['Checksum'];
    }

	private function insertAllRecords($config, $tbl) {
		$db = mysqli_connect($config['host'], $config['user'], $config['password']);
		if (!$db) { return null; }
		if (!mysqli_select_db($db,$config['name'])) { return null; }

		$this->_storeSQL("TRUNCATE TABLE ".$this->_objectName($tbl), $tbl);
		if($result = mysqli_query($db,"SELECT * FROM `{$tbl}`")) {
			while ($row = mysqli_fetch_array($result, MYSQLI_ASSOC)) {

				$data_str = "";
				foreach ( $row AS $fieldname => $fieldvalue ) {
					
					$data_str .= ($data_str) ? ', ' : '';
					$data_str .= $this->_objectName($fieldname)."='".mysqli_real_escape_string($db,$fieldvalue)."'";
				}
				$sql = "INSERT INTO ".$this->_objectName($tbl)." SET ".$data_str;
				$this->_storeSQL($sql, $tbl);
			}
		}
	}
	
	private function compareAllRecords($config_source, $config_target, $tbl, $pk, $fields, $exclude) {

		$config = $config_target;
		$db = mysqli_connect($config['host'], $config['user'], $config['password']);
		if (!$db) { return null; }
		if (!mysqli_select_db($db,$config['name'])) { return null; }
		
		$s_tab_data = $this->fetchTableData($config_source, $tbl);
		$t_tab_data = $this->fetchTableData($config_target, $tbl);
							
		$s_data_checksum = array();
			
		foreach($s_tab_data as $key => $val) {
			$row_data = $row_key = '';
			foreach($pk as $field) $row_key .= '_'.$val[$field];
			foreach($fields as $field) $row_data .= $val[$field];
			$s_data_checksum[$row_key] = crc32($row_data);
			//$s_data_checksum[$row_key] = $row_data;
		}
			
		foreach($t_tab_data as $key => $val) {
		
			$row_data = $row_key = '';
			foreach($pk as $field) $row_key .= '_'.$val[$field];
			foreach($fields as $field) $row_data .= $val[$field];
			$t_data_checksum = crc32($row_data);
			//$t_data_checksum = $row_data;
			
			if(!isset($s_data_checksum[$row_key])) {
			
				$data_str = "";
				foreach ( $val AS $fieldname => $fieldvalue ) {
					
					if(is_array($exclude) && in_array($fieldname, $exclude)) continue;
					
					$data_str .= ($data_str) ? ', ' : '';
					$data_str .= $this->_objectName($fieldname)."='".mysqli_real_escape_string($db,$fieldvalue)."'";
				}
				$sql = "INSERT INTO ".$this->_objectName($tbl)." SET ".$data_str;
				$this->_storeSQL($sql, $tbl);
			
			} elseif($s_data_checksum[$row_key] != $t_data_checksum) {

				$data_str = "";
				$cond_str = "";
				
				foreach ( $val AS $fieldname => $fieldvalue ) {
					
					if(in_array($fieldname, $fields)) {
						$data_str .= ($data_str) ? ', ' : '';
						$data_str .= $this->_objectName($fieldname)."='".mysqli_real_escape_string($db,$fieldvalue)."'";
					}
					
					if(in_array($fieldname, $pk)) {
						$cond_str .= ($cond_str) ? ' AND ' : '';
						$cond_str .= $this->_objectName($fieldname)."='".mysqli_real_escape_string($db,$fieldvalue)."'";
					}
				}
				
				if($cond_str != '') $cond_str = "WHERE ".$cond_str;				
				$sql = "UPDATE ".$this->_objectName($tbl)." SET ".$data_str." ".$cond_str;
								
				$this->_storeSQL($sql, $tbl);
			}
		}
	}
	private function fetchTriggers($config) {

		$db = mysqli_connect($config['host'], $config['user'], $config['password']);
		if (!$db) { return null; }
		if (!mysqli_select_db($db,$config['name'])) { echo "ERROR: ".mysqli_error($db); return null; }
		
		$data = array();
		if($result = mysqli_query($db,"SHOW TRIGGERS from `{$config['name']}`")) {
			while ($row = mysqli_fetch_array($result)) {
				$data[$row["Trigger"]] = array(
					"Trigger" => $row['Trigger'],
					"Timing" => $row["Timing"],
					"Event" => $row["Event"],
					"Table" => $row["Table"],
					"Statement" => $row["Statement"]
				);
			}
		}
	
		return count($data) ? $data : NULL;
	}
	
	private function createTrigger($data) {
	
		$sql = "\ndelimiter ~\n";
		$this->_storeSQL($sql, $data['Table'],'');
		
		$sql = "CREATE TRIGGER ".
				"{$data['Trigger']}\n".
				"{$data['Timing']} ".
				"{$data['Event']} ".
				"ON ".
				"{$data['Table']}\n".
				"FOR EACH ROW\n".
				"{$data['Statement']}";
		$this->_storeSQL($sql, $data['Table'], '~');
		
		$sql = "\ndelimiter ;\n";
		$this->_storeSQL($sql, $data['Table'],'');		
	}
	
	private function dropTrigger($data) {
	
		$sql = "DROP TRIGGER IF EXISTS {$data['Trigger']}";
		$this->_storeSQL($sql, $data['Table']);
	}	
	
	private function fetchTables($config) {

		$db = mysqli_connect($config['host'], $config['user'], $config['password']);
		if (!$db) { return null; }
		if (!mysqli_select_db($db,$config['name'])) { return null; }
		
		$data = array();
		if($result = mysqli_query($db,"SHOW TABLE STATUS FROM `{$config['name']}`")) {
			while ($row = mysqli_fetch_array($result)) {
			
				$constraints = array();
				if ( $row["Engine"] == "InnoDB" ) {

					$cparts = explode("; ", $row["Comment"]);
					$comment = preg_match("/^InnoDB free:/i", $c = trim($cparts[0])) ? "" : $c;

					if ( $result1 = mysqli_query($db,"SHOW CREATE TABLE `{$config['name']}`.`{$row["Name"]}`") ) {
						$row1 = mysqli_fetch_array($result1);

						if ( preg_match_all("/(CONSTRAINT `([A-Z0-9_$]+)` )?(FOREIGN KEY) \(([^)]+)\) REFERENCES `(([A-Z0-9_$]+)(\.([A-Z0-9_$]+))?)` \(([^)]+)\)( ON (DELETE|UPDATE)( (CASCADE|SET NULL|NO ACTION|RESTRICT)))?/i", $row1["Create Table"], $matches, PREG_SET_ORDER) ) {
							
							foreach ( $matches AS $match ) {
								$constraints[$match[4]] = array(
										"full" => $match[0],
										"name" => $match[4],
										"id" => $match[2],
										"type" => $match[3],
										"targetdb" => isset($match[8]) && trim($match[8]) != "" ? $match[6] : $db,
										"targettable" => isset($match[8]) && trim($match[8]) != "" ? $match[8] : $match[6],
										"targetcols" => $match[9],
										"params" => isset($match[10]) ? $match[10] : NULL,
									);
							}
						}
					}
				} else {
					$comment = trim($row["Comment"]);	
				}
				
				$data[$row["Name"]] = array(
					"database" => $config['name'],
					"name" => $row["Name"],
					"engine" => $row["Engine"],
					"options" => $row["Create_options"],
					"auto_incr" => isset($row["Auto_increment"]) ? $row["Auto_increment"] : NULL,
					"comment"=>$comment,
					"fields"=>$this->fetchFields($config, $row["Name"]),
					"idx"=>$this->fetchIndexes($config, $row["Name"]),
					"constraints"=>$constraints
				);
				
				$data[$row["Name"]]["collate"] = ( isset($row["Collation"]) ) ? $row["Collation"] : "";
				
			}
		}
	
		return count($data) ? $data : NULL;
	}
	
	private function fetchFields($config, $table) {
	
		$db = mysqli_connect($config['host'], $config['user'], $config['password']);
		if (!$db) { return null; }
		if (!mysqli_select_db($db,$config['name'])) { return null; }
		
		$data = NULL;
		if ( $result = mysqli_query($db,"SHOW FULL FIELDS FROM `$table` FROM `{$config['name']}`") ) {
			while ( $row = mysqli_fetch_array($result) ) {
				$field = $row['Field'];
				$data[$field] = array(
					"name" => $row['Field'],
					"type" => $row['Type'],
					"null" => ( isset($row['Null']) && $row['Null'] == "YES" ? 1 : 0 ),
					"default" => ( isset($row['Default']) ? $row['Default'] : NULL ),
					"extra"=> ( isset($row['Extra']) ? $row['Extra'] : NULL ),
				);
				if ( isset($row['Comment']) ) $data[$field]["comment"] = $row['Comment'];
				if ( isset($row['Collation']) && $row->Collation != "NULL" ) $data[$field]["collate"] = $row['Collation'];
			}
		}
		return isset($data) ? $data : NULL;
	}

	private function fetchIndexes($config, $table) {
	
		$db = mysqli_connect($config['host'], $config['user'], $config['password']);
		if (!$db) { return null; }
		if (!mysqli_select_db($db,$config['name'])) { return null; }

		$data = NULL;
		if ( $result = mysqli_query($db,"SHOW INDEX FROM `$table` FROM `{$config['name']}`") ) {
			
			while ( $row = mysqli_fetch_array($result) ) {
				$key_name = $row['Key_name'];
				$data[$key_name]["name"] = $row['Key_name'];				
				$data[$key_name]["unique"] = ($row['Non_unique'] == 0) ? 1 : 0;
				$data[$key_name]["fields"][$row['Column_name']]["name"] = $row['Column_name'];
				$data[$key_name]["type"] = isset($row['Index_type']) ? $row['Index_type'] : "BTREE";
				if ( isset($row['Sub_part']) && $row['Sub_part'] != "" ) $data[$key_name]["fields"][$row['Column_name']]["sub"] = $row['Sub_part'];
			}
		}
		return isset($data) ? $data : NULL;
	}

	private function alterTable($source, $target) {

		// Checking attributes ...
		$lastfield = NULL;
		foreach ( $target["fields"] AS $vk=>$vf ) {
		
			if ( !isset($source["fields"][$vk]) ) {
				$sql = "ALTER TABLE ".$this->_objectName($target["name"])." ADD ".$this->_fieldString($target["fields"][$vk]).( isset($lastfield) ? " AFTER"." $lastfield" : " ");
				$this->_storeSQL($sql, $target["name"]);
			
			} else if(crc32(serialize($target["fields"][$vk])) != crc32(serialize($source["fields"][$vk]))) {
			
				$sql = "ALTER TABLE ".$this->_objectName($target["name"])." MODIFY ".$this->_fieldString($target["fields"][$vk]);
				$this->_storeSQL($sql, $target["name"]);
			}
			$lastfield = $target["fields"][$vk]["name"];
			
		}

		foreach ( $source["fields"] AS $vk=>$vf ) {
			if ( !isset($target["fields"][$vk]) ) {
			
				$sql = "ALTER TABLE ".$this->_objectName($target["name"])." DROP ".$this->_objectName($vk);
				$this->_storeSQL($sql, $target["name"]);
			}
		}
		
		// Checking keys ...
		if ( isset($target["idx"]) ) {
			foreach ( $target["idx"] AS $vk => $vf ) {
				if ( !isset($source["idx"][$vk])) {
					$sql = "ALTER TABLE ".$this->_objectName($target["name"])." ADD ".$this->_indexString($vf);
					$this->_storeSQL($sql, $target["name"]);
				} else if ($source["idx"][$vk]["fields"] != $target["idx"][$vk]["fields"] || 
							$source["idx"][$vk]["type"] != $target["idx"][$vk]["type"]) {
					if( $vf["unique"] && $vk == "PRIMARY") {
						
						$sql = "ALTER TABLE ".$this->_objectName($target["name"])." DROP PRIMARY KEY, ADD ".$this->_indexString($vf);
						$this->_storeSQL($sql, $target["name"]);	
					} else {
						
						$sql = "ALTER TABLE ".$this->_objectName($target["name"])." DROP INDEX ".$vf["name"].", ADD ". $this->_indexString($vf);
						$this->_storeSQL($sql, $target["name"]);
					}
				}
			}
		}
		

		
		// table engine option (should be before constraints queries) ...
		if ( $source["engine"] != $target["engine"] ) {
			$sql = "ALTER TABLE ".$this->_objectName($target["name"])." ENGINE=".$target["engine"];
			$this->_storeSQL($sql, $target["name"]);
		}		

		// Doing handling of foreign key constraints ...
		if (isset($target["constraints"]) ) {
			foreach ( $target["constraints"] AS $vk => $vf ) {
				if ( !isset($source["constraints"][$vk]) ) {
					$sql = "ALTER TABLE ".$this->_objectName($target["name"])." ".$this->_constraintString($vf, $target["database"], 'CONSTRAINT_ADD');
					$this->_storeSQL($sql, $target["name"]);
				}
			}
		}
		
		if (isset($source["constraints"]) ) {
			foreach ( $source["constraints"] AS $vk => $vf ) {
				if ( !isset($target["constraints"][$vk]) ) {
					$sql = "ALTER TABLE ".$this->_objectName($target["name"])." ".$this->_constraintString($vf, $target["database"], 'CONSTRAINT_DROP');
					$this->_storeSQL($sql, $target["name"]);
				}
			}
		}
		
		if ( isset($source["idx"]) ) {
			foreach ( $source["idx"] AS $vk => $vf ) {
				if ( !isset($target["idx"][$vk] ) ) {
					$sql = "ALTER TABLE ".$this->_objectName($target["name"])." DROP ".(($vf["unique"] && $vk == "PRIMARY") ? "PRIMARY KEY" : "INDEX ".$source["idx"][$vk]['name']);
					$this->_storeSQL($sql, $target["name"]);
				}
			}
		}		

		// Charset ...
		if ( $source["collate"] != $target["collate"] ) {
			$charsetinfo = explode("_", $target["collate"]);

			$charset = "DEFAULT CHARSET=".$charsetinfo[0]." COLLATE=".$target["collate"];
			$sql = "ALTER TABLE ".$this->_objectName($target["name"])." ".$charset;
			$this->_storeSQL($sql, $target["name"]);
		}

		// table options ...

		/*
		if ( $source["options"] != $target["options"] ) {
			$sql = "ALTER TABLE ".$this->_objectName($target["name"])." ".$target["options"];
			$this->_storeSQL($sql, $target["name"]);
		}

		if ( $source["auto_incr"] != $target["auto_incr"] ) {
			$sql = "ALTER TABLE ".$this->_objectName($target["name"])." AUTO_INCREMENT=".$target["auto_incr"];
			$this->_storeSQL($sql, $target["name"]);
		}
		*/

		if ( $source["comment"] != $target["comment"] ) {
			$sql = "ALTER TABLE ".$this->_objectName($target["name"])." COMMENT='".$target["comment"]."'";
			$this->_storeSQL($sql, $target["name"]);
		}
	}

	private function createTable($config, $table) {
	
		$db = mysqli_connect($config['host'], $config['user'], $config['password']);
		if (!$db) { return null; }
		if (!mysqli_select_db($db,$config['name'])) { return null; }
		
		$sql = '';
		if($result = mysqli_query($db,"SHOW CREATE TABLE {$config['name']}.$table")) {
			$row = mysqli_fetch_array($result);
			$sql =  $row["Create Table"];
		}
		$this->_storeSQL($sql, $table);
		
		return $sql;
	}

	private function dropTable($table) {
		$sql = "DROP TABLE IF EXISTS $table";
		$this->_storeSQL($sql, $table);
		
		return $sql;
	}

	private function _constraintString($idx, $targetdb, $what = 'CONSTRAINT_ADD') {
		if ( $what == 'CONSTRAINT_ADD' ) {
			$result = "ADD CONSTRAINT ".$idx["type"] . " (" . $idx["name"] . ") REFERENCES " . $this->_objectName($idx["targettable"]) . " (" . $idx["targetcols"] . ")".( isset($idx["params"]) && trim($idx["params"]) != "" ? $idx["params"] : "" );
		} else if ( $what == 'CONSTRAINT_DROP') {
			$result = "DROP ".$idx["type"]." ".$idx["id"];
		} else if ( $what == 'CONSTRAINT_CREATE' ) {
			$result = "CONSTRAINT ".$idx["type"] . " (" . $idx["name"] . ") REFERENCES " . $this->_objectName($idx["targettable"]) . " (" . $idx["targetcols"] . ")".( isset($idx["params"]) && trim($idx["params"]) != "" ? $idx["params"] : "" );
		} else $result = "";
		return $result;
	}

	private function _fieldString($field) {
		$result = $this->_objectName($field["name"])." ";
		$result .= $field["type"];
		$result .= ($field["null"]) ? " NULL" : " NOT NULL";
		if ( isset($field["default"]) ) {
			if($field["default"] == 'CURRENT_TIMESTAMP') $result .= " DEFAULT ".$field["default"];
			else $result .= " DEFAULT '".$field["default"]."'";
		} 
		if ( isset($field["comment"]) && $field["comment"] != '') {
			$result .= " COMMENT '".$field["comment"]."'";
		}
		/*
		if ( isset($field["collate"]) ) {
			$result .= " COLLATE ".$field["collate"];
		}
		if ( isset($field["extra"]) && $field["extra"]!="" ) {
			$result .= " ".$field["extra"];
		}
		*/

		return $result;
	}

	private function _indexString($idx) {
		$result = ( $idx["type"] == "FULLTEXT" ? "FULLTEXT INDEX" . ( isset($idx["name"]) ? " ".$this->_objectName($idx["name"]) : "" ) : ( $idx["unique"] ? ( $idx["name"]=="PRIMARY" ? "PRIMARY KEY" : "UNIQUE ".$this->_objectName($idx["name"]) ) : "INDEX ".$this->_objectName($idx["name"]) ) )." (";
		$i = 1; $im = count($idx["fields"]);
		foreach ( $idx["fields"] AS $vf ) {
			$result .= $this->_objectName($vf["name"]).( isset($vf["sub"]) ? "(".$vf["sub"].")" : "" ).( $i<$im ? ", " : "" );
			$i++;
		}
		$result .= ")";
		return $result;
	}
	
	private function _objectName($name) {
		return "`".$name."`";
	}	
	
	private function _storeSQL($sql, $table='', $delimiter=';')	{
		if(trim($sql)) {
			$this->upgrade_sql[] = $sql.$delimiter;
			if($table) $this->upgrade_sql_categorized[$table][] = $sql.$delimiter;
		}
	}
};
?>