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


  function get_platform_list_for_runid($runid) {
    // Given a runid, get the set of platforms
    $sql = "SELECT DISTINCT(platform) AS platform
        FROM Task
        WHERE runid = $runid";
    
    $results = $this->db_query($sql);
    $platforms = Array();
    foreach ($results as $row) {
      $platforms[] = $row["platform"];
    }
  
    return $platforms;
  }

  function get_test_runids_for_build($runid) {
    $runid_sql = "SELECT DISTINCT Method_nmi.runid ".
      "  FROM Method_nmi, Run ".
      " WHERE Method_nmi.input_runid = $runid ".
      "   AND Run.runid = Method_nmi.runid ".
      "   AND (component_version = project_version or ( component_version = 'native')) ";

    $results = $this->db_query($runid_sql);
    $runids = Array();
    foreach ($results as $row) {
      $runids[] = $row["runid"];
    }

    if(!$runids) {
      return Array();
    }

    $test_runids = implode(",", $runids);
    $platform_sql = "
      SELECT 
         DISTINCT(platform),
         runid
      FROM
         Task
      WHERE
         runid IN ($test_runids) AND
         platform!='local'";

    $results = $this->db_query($platform_sql);
    $runids = Array();
    foreach ($results as $row) {
      $runids[$row["platform"]] = $row["runid"];
    }

    return $runids;
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