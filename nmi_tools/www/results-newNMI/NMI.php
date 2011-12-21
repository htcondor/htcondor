<?php

define("NMI_CONFIG_PATH", "/usr/local/nmi/etc/nmi.conf");

class NMI {
  var $config;  // Array containing configuration information from file
  var $db;

  // Constructor
  function NMI() {
    return $this->load_config();
  }

  // Destructor
  function _NMI() {

    // Close MySQL socket if it was opened
    if($db) {
      mysql_close($db);
    }

  }
  
  function load_config() {
    if(!file_exists(NMI_CONFIG_PATH)) {
      echo "ERROR: Config file '" . NMI_CONFIG_PATH . "' does not exist";
      return (false);
    }
    elseif(!is_readable(NMI_CONFIG_PATH)) {
      print "ERROR: Config file '" . NMI_CONFIG_PATH . "' is not readable.";
      return (false);
    }

    $this->config = $this->__parse_config_file(NMI_CONFIG_PATH);
    return true;
  }

  function connect_to_db() {
    $this->db = mysql_connect($this->config["DB_HOST"],
			      $this->config["DB_READER_USER"], 
			      $this->config["DB_READER_PASS"]) or die("Could not connect to DB: " . mysql_error());
    mysql_select_db($this->config["DB_NAME"]) or die("Could not select DB: " . mysql_error());
    return $this->db;
  }


  private function __parse_config_file($file) {
    $contents = preg_split("/\n/", file_get_contents(NMI_CONFIG_PATH, false));

    $config = Array();
    foreach ($contents as $line) {
      # Skip comments and blank lines
      if(preg_match("/^\s*#/", $line)) continue;
      if(preg_match("/^\s*$/", $line)) continue;

      $tmp = preg_split("/=/", $line);
      if(count($tmp) == 2) {
	$config[trim($tmp[0])] = trim($tmp[1]);
      }
    }
    
    return $config;
  }

}

?>