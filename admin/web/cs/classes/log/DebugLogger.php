<?php

class DebugLogger extends Logger
{
    public function DebugLogger() {
        // move to _load_amethyst_config()
        $this->log_directory = "/tmp";
        if (!file_exists ($this->log_directory)) {
            mkdir ($this->log_directory, 0777, 1);
        }
        $this->log_filename = "cs_php_debug.log";
        $this->with_timestamp = 0;
        parent::Logger();
    }

    public function debug_log($str, $file, $line, $function) {
        if (strcmp($function, "")) {
            $msg = basename($file)."(line ".$line."): function ".$function."(): ".$str;
        }
        else {
            $msg = basename($file)."(line ".$line."): ".$str;
        }
        parent::log($msg);
    }
}

?>
