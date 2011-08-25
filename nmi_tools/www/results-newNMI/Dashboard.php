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
    print "<script type='text/javascript' src='jquery-1.6.2.js'></script>\n";
    print "<link rel='stylesheet' href='dashboard.css' type='text/css'>\n";
    print "<link rel='stylesheet' href='condor.css' type='text/css'>\n";
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
}

?>