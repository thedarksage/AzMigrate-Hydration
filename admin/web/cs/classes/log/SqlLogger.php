<?php

class SqlLogger extends Logger
{
    public $log_file_size_limit;

    public function SqlLogger() {
        // move to _load_amethyst_config()
        $this->log_directory = "/home/svsystems/var/sync";
        if (!file_exists ($this->log_directory)) {
            mkdir ($this->log_directory, 0777, 1);
        }
        $this->log_filename = time()."_sync.log";
        $this->with_timestamp = 0;
        $this->log_file_size_limit = 2*1024*1024;
        parent::Logger();
    }

    public function getLogFilename() {
        $file_size = filesize ($this->log_filename);
        if ($file_size && ($file_size >= $this->log_file_size_limit))
        {
            $this->log_filename = $this->newLogFilename();
        }
        parent::getLogFilename();
    }

    public function newLogFilename() {
        return (time()."_sync.log");
    }
}

?>

