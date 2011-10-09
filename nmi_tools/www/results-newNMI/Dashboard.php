<?php

define("CONDOR_USER", "condorauto");

include "NMI.php";

class Dashboard {

  var $nmi;
  var $db;

  function Dashboard() {
    $this->nmi = new NMI();
    return true;
  }
  
  function print_header($title) {
    print "<html>\n";
    print "<head>\n";
    print "<title>$title</title>\n";
    print "<link rel='stylesheet' href='dashboard.css' type='text/css'>\n";
    flush();
  }

  function connect_to_db() {
    $this->db = $this->nmi->connect_to_db();
    return true;
  }

  function db_query($query) {
    $result = mysql_query($query) or die("Query failed:<br>\n" . $query  . "<br>\n<br>\nError message: " . mysql_error());

    $all = Array();
    while ($row = mysql_fetch_assoc($result)) {
      $all[] = $row;
    }
    
    mysql_free_result($result);

    return $all;
  }

  function get_condor_user() {
    return CONDOR_USER;
  }


  function condor_status() {
    // Determine the list of machines currently in the pool
    $output = `/usr/bin/condor_status -format '%s' nmi_platform -format ' %s\n' Machine | sort | uniq -c`;
    
    $platforms = Array();
    $lines = split("\n", $output);
    foreach ($lines as $line) {
      if(preg_match("/\s*(\d+)\s+(\S+)\s+(.+)$/", $line, $matches)) {
	if(!array_key_exists($matches[2], $platforms)) {
	  $platforms[$matches[2]] = Array();
	}
	array_push($platforms[$matches[2]], array($matches[3], $matches[1]));
      }
    }
    
    return $platforms;
  }

}

function day_of_week($num) {
  
  if($num == 1) { return "Sunday"; }
  elseif($num == 2) { return "Monday"; }
  elseif($num == 3) { return "Tuesday"; }
  elseif($num == 4) { return "Wednesday"; }
  elseif($num == 5) { return "Thursday"; }
  elseif($num == 6) { return "Friday"; } 
  elseif($num == 7) { return "Saturday"; }
  else { return "Unknown"; }
}

?>