<?php
class CsApiJson
{
    const REQUEST_VER_2014_08_01 = '2014-08-01';
    const CS_LOGIN_REPLY = 'csloginreply';
    const CS_CFS_CONNECT_INFO_REPLY = 'cfsconnectinforeply';
    const CS_STATUS_OK = 'ok';

    // public
    public function __construct()
    {
        $this->commonfunctions_obj = new CommonFunctions();
		if ($_SERVER['HTTPS'] == "on") {
            $this->get_cs_fingerprint();
        } else {
            $this->fingerprint = '';
        }
        $this->get_passphrase();
        $this->requestVerCurrent = self::REQUEST_VER_2014_08_01;
        $this->dbConnection = new Connection();
    }

    public function process_request()
    {
        $ar = array();
        // NOTE: the action names need to be kept in sync with
        // host/cxpslib/protocolhandler.h CS_ACTION_* values
        switch (htmlspecialchars($_REQUEST['action'])) {
            case 'cslogin':
                $ar = $this->cs_login();
                break;
            case 'getcfsconnectinfo':
                $ar = $this->get_cfs_connect_info();
                break;
            case 'cfsheartbeat':
                $ar = $this->cfs_heartbeat();
                break;
            case 'cfserror':
                $ar = $this->cfs_error();
                break;
            default:
                $ar = $this->reply_error('invalid request');
                break;
        }
        if (isset($ar)) {
            header('Content-Type: application/json');
            $jsonStr = json_encode($ar) . "\n";
            echo $jsonStr;
        }
    }
	
    // protected
    protected function reply_error($reason, $data = null)
    {
        return array(
            'error' => array(
                'reason' => $reason,
                'data' => $data
            )
        );
    }

    protected function report_validation_error()
    {
        http_response_code(501);
        header('HTTP/1.1 511 Network Authentication Required');
        echo "<html><head></heade><body><<h1>511 Network Authentication Required</h1></body></html>";
    }

    protected function cs_login()
    {
        if ($this->validate_cs_login_id()) {
            $id = htmlspecialchars($_REQUEST['id']);
            $tag = $this->gen_nonce(16, true);
            return array(
                'cs' => array(
                    'status' => self::CS_STATUS_OK,
                    'action' => self::CS_LOGIN_REPLY,
                    'tag' => $tag,
                    'id' => $this->build_cs_login_reply_id($id, $tag)
                )
            );
        }
        return $this->report_validation_error();
    }

    protected function get_cfs_connect_info()
    {
        if ($this->validate_get_cfs_connect_info_id()) 
		{
            $hostId = htmlspecialchars($_REQUEST['host']);
			if (isset($hostId) && $hostId != '')
			{
				$guid_format_status = $this->commonfunctions_obj->is_guid_format_valid($hostId);		
				if ($guid_format_status == false)
				{
					$this->commonfunctions_obj->bad_request_response("Invalid post data for hostId $hostId in get_cfs_connect_info.");
				}
			}
			$id = htmlspecialchars($_REQUEST['id']);			
			
            $sqlQuery = 'select ' .
                '  cfs.id, ' .
                '  cfs.publicIpAddress, ' .
                '  cfs.publicPort, ' .
                '  cfs.publicSslPort, ' .
                '  cfs.privateIpAddress, '.
                '  cfs.privatePort, ' .
                '  cfs.privateSslPort, ' .
                '  cfs.connectivityType ' .
                'from ' .
                '  cfs,  ' .
                '  srcLogicalVolumeDestLogicalVolume sd ' .
                'where ' .
                '  sd.processServerId = \'' . $this->dbConnection->sql_escape_string($hostId) . '\'' .
                '  and sd.destinationHostId = cfs.id ' .
                '  and sd.useCfs = 1';
            $rs = $this->dbConnection->sql_query($sqlQuery, NULL);
            if (!$rs) {
                return $this->reply_error('SQL failed: ' . $this->dbConnection->sql_error());
            }
            $cfsInfo = array('cfsInfo' => array());
            if ($this->dbConnection->sql_num_row($rs) > 0) {
                while ($row = $this->dbConnection->sql_fetch_object($rs)) {		
                    if($row->connectivityType == 'VPN')
                    {
                        $ipAddress = $row->privateIpAddress;
                        $port = $row->privatePort;
                        $sslPort = $row->privateSslPort;
                    }
                    else if($row->connectivityType == 'NON VPN')
                    {
                        $ipAddress = $row->publicIpAddress;
                        $port = $row->publicPort;
                        $sslPort = $row->publicSslPort;					
                    }
                    
                    $cfsInfo['cfsInfo'][] =  array(
                        'cfsId' => (empty($row->id) ? 'na' : $row->id),
                        'publicIpAddress' => (empty($ipAddress) ? 'na' : $ipAddress),
                        'publicPort' => (empty($port) ? '9080' : $port),
                        'publicSslPort' => (empty($sslPort) ? '9443' : $sslPort)                       
                    );					
                }
                $cfsInfo['cfsTag'] = $this->build_get_cfs_connect_info_reply_id($cfsInfo['cfsInfo']);
            }
            return $cfsInfo;
        } else {
            return $this->reply_error('validation failed', print_r($_REQUEST, true));
        }
    }

    protected function cfs_heartbeat()
    {
        if ($this->validate_cfs_heartbeat_id()) {
            $hostId = htmlspecialchars($_REQUEST['host']);
			if (isset($hostId) && $hostId != '')
			{
				$guid_format_status = $this->commonfunctions_obj->is_guid_format_valid($hostId);		
				if ($guid_format_status == false)
				{
					$this->commonfunctions_obj->bad_request_response("Invalid post data for hostId $hostId in cfs_heartbeat.");
				}
			}
            $id = htmlspecialchars($_REQUEST['id']);
            $sqlQuery = 'update cfs set heartbeat = UTC_TIMESTAMP where id = ' . "'" . $this->dbConnection->sql_escape_string($hostId) . "'";
            $rs = $this->dbConnection->sql_query($sqlQuery, NULL);
            if (!$rs) {
                return $this->reply_error('SQL failed: ' . $this->dbConnection->sql_error());
            }
            return array('cfsInfo' => array());
        } else {
            return $this->reply_error('validation failed', print_r($_REQUEST, true));
        }
    }

    protected function cfs_error()
    {
        if ($this->validate_cfs_error_id()) 
		{
            // MAYBE: need to issue trap
            return array('cfsInfo' => array());
        } 
		else 
		{
            return $this->reply_error('validation failed', print_r($_REQUEST, true));
        }
    }

    protected function validate_cs_login_id()
    {
        return $this->validate_id($this->build_cs_id(htmlspecialchars($_REQUEST['tag']), $this->fingerprint));
    }

    protected function build_cs_login_reply_id($id, $tag)
    {
        $a1 = '';
        if (strlen($this->fingerprint) > 0) {
            $a1 = hash_hmac('sha256', $this->fingerprint, $this->passphraseHash, false);
        }
        $a2 = htmlspecialchars($id) . ':'
            . htmlspecialchars(self::CS_STATUS_OK) . ':'
            . htmlspecialchars(self::CS_LOGIN_REPLY) . ':'
            . htmlspecialchars($tag) . ':'
            . htmlspecialchars($_REQUEST['ver']);
        $a3 = $a1 . ':' . hash_hmac('sha256', $a2, $this->passphraseHash, false);
        return hash_hmac('sha256', $a3, $this->passphraseHash, false);
    }

    protected function validate_get_cfs_connect_info_id()
    {
        // require tag if post 2014-08-01, but also support tag being sent with 2014-08-01
        if (htmlspecialchars($_REQUEST['ver']) > self::REQUEST_VER_2014_08_01 || !empty($_REQUEST['tag'])) {
            return $this->validate_id($this->build_cs_id(htmlspecialchars($_REQUEST['tag']) . htmlspecialchars($_REQUEST['host'])));
        } else {
            return $this->validate_id($this->build_cs_id( htmlspecialchars($_REQUEST['host'])));
        }
    }

    protected function build_get_cfs_connect_info_reply_id($cfsConnectInfo)
    {
        // the string to sign is
        // tag<nonce>hostid<hostid>publicIpAddress<publicIpAddress>publicPort<publicPort>publicSslPort<publicSslPort>...hostid<hostid>publicIpAddress<publicIpAddress>publicPort<publicPort>publicSslPort<publicSslPort>
        // to make sure both sides build that string the same way, we order it host id
        $cfsTag['tag'] = 'tag' . $this->gen_nonce(16);
        $strToSign = $cfsTag['tag'];
        $tmpArray = array();
        foreach ($cfsConnectInfo as $entry) {
            $tmpArray[$entry['cfsId']] = array('publicIpAddress' . $entry['publicIpAddress'] .
                                               'publicPort' . $entry['publicPort'] .
                                               'publicSslPort' . $entry['publicSslPort']);
        }
        asort($tmpArray);
        foreach ($tmpArray as $key => $val) {
            $strToSign .= 'hostid' . $key . $val[0];
        }
        $cfsTag['id'] = hash_hmac('sha256', $strToSign, $this->passphraseHash, false);
        return $cfsTag;
    }

    protected function validate_cfs_heartbeat_id()
    {
        // require tag if post 2014-08-01, but also support tag being sent with 2014-08-01
        if (htmlspecialchars($_REQUEST['ver']) > self::REQUEST_VER_2014_08_01 || !empty($_REQUEST['tag'])) {
            return $this->validate_id($this->build_cs_id(htmlspecialchars($_REQUEST['tag']) . htmlspecialchars($_REQUEST['host'])));
        } else {
            return $this->validate_id($this->build_cs_id( htmlspecialchars($_REQUEST['host'])));
        }
    }

    protected function validate_cfs_error_id()
    {
        // require tag if post 2014-08-01, but also support tag being sent with 2014-08-01
        if (htmlspecialchars($_REQUEST['ver']) > self::REQUEST_VER_2014_08_01 || !empty($_REQUEST['tag'])) {
            return $this->validate_id($this->build_cs_id(htmlspecialchars(htmlspecialchars($_REQUEST['tag']) . $_REQUEST['host']) . htmlspecialchars($_REQUEST['msg'])));
        } else {
            return $this->validate_id($this->build_cs_id(htmlspecialchars($_REQUEST['host']) . htmlspecialchars($_REQUEST['msg'])));
        }


    }

    protected function build_cs_id($params,              // the request specific params all concatenated together
                                   $fingerprint = null)  // set if fingerprint is part of id (really only used for login)
    {
        $a1 = '';
        if (isset($fingerprint)) {
            $a1 = hash_hmac('sha256', $fingerprint, $this->passphraseHash, false);
        }
        $a2 = htmlspecialchars($_SERVER['REQUEST_METHOD']) . ':'
            . htmlspecialchars($_SERVER['PHP_SELF']) . ':'
            . htmlspecialchars($_REQUEST['action']) . ':'
            . $params . ':'
            . htmlspecialchars($_REQUEST['ver']);
        $a3 = $a1 . ':' . hash_hmac('sha256', $a2, $this->passphraseHash, false);
        return hash_hmac('sha256', $a3, $this->passphraseHash, false);
    }

    protected function gen_nonce($bytes, $includeTime = false)
    {
        $nonce = '';
        if ($includeTime) {
            $nonce .= 'ts:' . microtime(true) . '-';
        }
        for ($i = 0; $i < $bytes; $i++) {
            $nonce .= bin2hex(chr(mt_rand(0, 255)));
        }
        return $nonce;
    }

    protected function get_top_level_dir()
    {
        if (preg_match('/Windows/i', php_uname())) {
            $drive = getenv('ProgramData');
            $file = $drive .'/Microsoft Azure Site Recovery';
            if (file_exists($file)) {
                return $file;
            } 
        } else {
            return '/usr/local/InMage';
        }
    }

    protected function read_file($fileName)
    {
        $line = '';
        $file = $this->commonfunctions_obj->file_open_handle($fileName, 'r');
        // return if file handle not available.
        if(!$file) return;
        if ($file) {
            $line = fgets($file);
            fclose($file);
        }
        return trim($line);
    }

    protected function get_passphrase()
    {
        $this->passphrase = $this->read_file($this->get_top_level_dir() . '/private/connection.passphrase');
        $this->passphraseHash = hash('sha256', $this->passphrase, false);
    }

    protected function get_cs_fingerprint()
    {
        $this->fingerprint = strtolower($this->read_file($this->get_top_level_dir() . '/fingerprints/cs.fingerprint'));
    }

    protected function validate_id($id)
    {
        // NOTE: the ids are hashed again to prevent possibly timing attacks
        return (hash_hmac('sha256', htmlspecialchars($_REQUEST['id']), $this->passphraseHash, false) == hash_hmac('sha256', $id, $this->passphraseHash, false));
    }
	
    // private
    private $passphrase;
    private $passphraseHash;
    private $requestVerCurrent;
    private $fingerprint;
    private $dbConnection;
}
?>