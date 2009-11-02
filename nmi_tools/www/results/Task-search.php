<html>
<body bgcolor="#d9d9ff" text="navy" link="#666699"  alink=#000000 vlink="#333333">

<center><h2>UW NMI build and test system</h2>
<title>NWO task results</title>
<LINK REL="StyleSheet" HREF="condor.css" TYPE="text/css">

<?php

require_once "./load_config.inc";
load_config();

include ('Task.php');
 
  
  
  

# $server_hostname = WEBSERVER;
#$main_server = MAIN_WEBSERVER;
$main_server = WEBSERVER;

  $gid = $_REQUEST["gid"];
  $runid = $_REQUEST["runid"];
  $column = $_REQUEST["column"];
  $order = $_REQUEST["order"];

  $currentDir = "results";

  # default to order by the column id 
  if ($column == "" || $column == " " || is_null($column) ) {
    $column = "platform";
  }

 # default to order the column in ascending order 
  if ($order == "" || $order == " " || is_null($order) ) {
    $order = "ASC";
  }

  # display link back to entry point
  echo "<a href=\"http://$main_server/$currentDir/Run-main.php\">back to Run page</a><br><br>";  
 
  # if we don't have the runid or gid, find it
  # but sometimes we only have one or the other depending on where the page is accessed from   
  if ($runid == "") {
    $query2 = "SELECT * FROM Run WHERE gid='$gid'";
  } else if ($gid == "") { 
    $query2 = "SELECT * FROM Run WHERE runid='$runid'";
  } else { # default to use the runid if both are present
    $query2 = "SELECT * FROM Run WHERE runid='$runid'";
  }
  $db = mysql_connect(WEB_DB_HOST, DB_READER_USER, DB_READER_PASS) or die ("Could not connect : " . mysql_error());
  mysql_select_db(DB_NAME) or die("Could not select database");
  $result2 = mysql_query($query2) or die ("Query failed : " . mysql_error());
  while( $myrow = mysql_fetch_array($result2) ) {
    $args["gid"] = $myrow["gid"];
    $args["runid"] = $myrow["runid"];
    $args["user"] = $myrow["user"];
    $args["result"] = $myrow["result"];
    $args["run_type"] = $myrow["run_type"];
    $args["project"] = $myrow["project"];
    $args["component"] = $myrow["component"];
    $args["description"] = $myrow["description"];
    $start_ts = strtotime($myrow["start"]);
    $start_ts = $start_ts + date("Z", $start_ts);
    $args["start"] = date("m/d/y  H:i:s", $start_ts);
    $finish_ts = strtotime("$myrow[finish]");
    $finish_ts = $finish_ts + date("Z", $finish_ts);
    $args["finish"] = date("m/d/y  H:i:s", $finish_ts);
    $storage_hostname = $myrow['host'];
    $args["filepath"] = $myrow["filepath"];
  }

  $gid = $args["gid"];
  $runid = $args["runid"];
  $myresult = $args["result"];
  $basedir = $args["filepath"]; #"rundir"; # absolute path is /nmi/run 
  $base = "$basedir/$gid"; 
 
  # find the duration, if the run is complete
  # if not finished, don't display a finish time
  if ( !( is_null($myresult)) ) {
    $duration_ts = $finish_ts - $start_ts;
    $duration = $duration_ts;   
  } else {
    $duration = "In progress";
    $args["duration"] = "In progress";
    $args["finish"] = "In progress";
  }     

  # split up duration into readable units  
  if ($duration != "In progress") {
    $hours = 0; $minutes = 0; $seconds = 0; 
    $hours = floor($duration / 3600);
    $newruntime = $duration % 3600;
    $minutes = floor($newruntime / 60);
    $seconds = $duration % 60;
    $args["duration"] = "$hours : $minutes : $seconds";
  }
    
  # display the heading  - make this more condensed in V2.0
  foreach ($args as $key => $val) {
    echo "$key : $val <br>";
  }
  echo "<td><a href=\"http://$storage_hostname/$base\">run directory</a></td><br><br>";
 
 
  # get all rows for main page display 
  $db = mysql_connect(WEB_DB_HOST, DB_READER_USER, DB_READER_PASS) or die ("Could not connect : " . mysql_error());
  mysql_select_db(DB_NAME) or die("Could not select database");
  $query1 = "SELECT * FROM Task WHERE runid='$runid' ORDER BY $column $order";
  $result1 = mysql_query($query1) or die ("Query failed : " . mysql_error());

  # set up main table headings  
  echo "<table width=100%  bgcolor=#d9d9ff>";
  echo "<tr bgcolor=white>";
  echo "<td width=70><a href=\"http://$storage_hostname/$currentDir/Task-search.php?gid=$gid&runid=$runid&order=ASC&column=task_id\">task id</a></td>";  
  echo "<td width=20><a href=\"http://$storage_hostname/$currentDir/Task-search.php?gid=$gid&runid=$runid&order=DESC&column=result\">result</a></td>";  
  echo "<td width=20>out</td>";  
  echo "<td width=20>err</td>";  
  echo "<td width=100><a href=\"http://$storage_hostname/$currentDir/Task-search.php?gid=$gid&runid=$runid&order=ASC&column=host\">hostname</a></td>";  
  echo "<td width=100><a href=\"http://$storage_hostname/$currentDir/Task-search.php?gid=$gid&runid=$runid&order=ASC&column=platform\">platform</a></td>";  
  echo "<td width=100><a href=\"http://$storage_hostname/$currentDir/Task-search.php?gid=$gid&runid=$runid&order=ASC&column=name\">name</a></td>";  
  echo "<td width=70><a href=\"http://$storage_hostname/$currentDir/Task-search.php?gid=$gid&runid=$runid&order=ASC&column=start\">start (CST)</a></td>";  
  echo "<td width=70>duration HH:MM:SS</td>";  
  echo "</tr>"; 
  $tasks = Array();
  while( $myrow = mysql_fetch_array($result1) ) {
  
    $task_id = $myrow["task_id"];
#echo "<pre>\n"; var_dump($task_id); echo "</pre><BR>\n";
    # read everything into objects, so we can compare all necessary fields
    $tasks["$task_id"] = new Task( $gid, $myrow["task_id"], $runid, $myrow["platform"], $myrow["name"], $myrow["exe"], $myrow["args"], $myrow["host"], $myrow["start"], $myrow["finish"], $myrow["result"], $args["user"], $args["filepath"] );
#echo "<pre>\n"; var_dump($tasks[$task_id]); echo "</pre><BR>\n";
  }

  # TODO sort array - by any of the column headings - taskid, result, platform, hostname, etc 
  # by applying a function to the array
  $remote_task_list = array();
  foreach ( $tasks as $key_task_id => $currTaskObj ) {
    $temptask = $currTaskObj->getTaskname();
    $tempplat = $currTaskObj->getPlatform();
    if ( "$temptask" == "remote_task") {
      array_push( $remote_task_list, "$tempplat" );
    }
  }

  foreach ( $tasks as $key_task_id => $currTaskObj ) {
  
    echo "<tr>";
    printf( "<td>%s</td>", $currTaskObj->getTaskId() );

    # set platform_job status == Queued if !results for platform_job, and remote_task !running 
    if ( is_null($currTaskObj->getResult() ) ) {
      $tempplat = $currTaskObj->getPlatform();
      if ( in_array( "$tempplat", $remote_task_list ) ) {  # $remote_task is running )
        printf("<td bgcolor=#ffff55>%s</td>", "running");
      } else {
        printf("<td bgcolor=brown>%s</td>", "queued");
      }
    } else if ( $currTaskObj->getResult() == 0) {
      printf("<td bgcolor=#55ff55>%s</td>", $currTaskObj->getResult() );
    }  else {  # result is non-zero and therefore failed
      printf("<td bgcolor=#ff5555>%s</td>", $currTaskObj->getResult() );
    }
 
    $baseDir = "";
    $actualBaseDir = RUNDIR;

    # logic to determine how to display links to .out and .err files  
    if ( $currTaskObj->getFilePath() == "" ) { # no files found 
        printf("<td>%s</td>", "N/A" );
        printf("<td>%s</td>", "N/A" );
    } else {
        $mytypestring= "type=\"text/plain\"";
        $myFilePath = $currTaskObj->getFilePath();
        $actualFilePath = "$actualBaseDir/$myFilePath.out";
        if ( file_exists( $actualFilePath ) && filesize( $actualFilePath ) == 0 ) {
          printf("<td>%s</td>", "0byte" );
        } else {
          $myFile = "$baseDir/$myFilePath.out";
          echo "<td bgcolor=aaaaaa><a $mytypestring href=\"http://$storage_hostname/$myFile\">out</a></td>";
        }
        $actualFilePath = "$actualBaseDir/$myFilePath.err";
        if ( file_exists( $actualFilePath ) && filesize( $actualFilePath ) == 0 ) {
          printf("<td>%s</td>", "0byte" );
        } else {
          $myFile = "$baseDir/$myFilePath.err";
          echo "<td bgcolor=aaaaaa><a $mytypestring href=\"http://$storage_hostname/$myFile\">err</a></td>";
        }
    } 

    printf( "<td>%s</td>", $currTaskObj->getHostname() );
    printf( "<td>%s</td>", $currTaskObj->getPlatform() );
    printf( "<td>%s</td>", $currTaskObj->getTaskname() );
    printf( "<td>%s</td>", $currTaskObj->getStart() );
    printf( "<td>%s</td>", $currTaskObj->getDuration() );
    echo "</tr>";
  }
  echo "</table>";
  echo "</center>";

mysql_free_result($result1);
mysql_free_result($result2);
mysql_close($db);

?>
</body>
</html>

