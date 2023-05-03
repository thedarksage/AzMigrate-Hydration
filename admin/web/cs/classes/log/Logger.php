<?php

class Logger
{
    protected $log_directory;
    protected $log_filename;
    protected $with_timestamp;

    public function Logger() {
    }

    public function withTimestamp($msg) {
        return (time().": ".$msg);
    }

    public function log($msg) {
        if ($this->with_timestamp) {
            $msg = $this->withTimestamp ($msg);
        }

        $log_file = $this->log_directory."/".$this->log_filename;
        $fh = fopen($log_file, 'a+');
        if($fh) {
            fwrite ($fh, $msg."\n");
            fclose ($fh);
        }
    }

    public function getLogDirectory() {
        return $this->log_directory;
    }

    public function setLogDirectory($dir) {
        if (is_string ($dir)) {
            $this->log_directory = $dir;
        }
    }

    public function getLogFilename() {
        return $this->log_filename;
    }

    public function setLogFilename($filename) {
        if (is_string ($filename)) {
            $this->log_filename = $filename;
        }
    }

    public function isWithTimeStamp() {
        return $this->with_timestamp;
    }

    public function setWithTimeStamp($with_ts) {
        $this->with_timestamp = $with_ts ? 1:0;
    }
}

?>
