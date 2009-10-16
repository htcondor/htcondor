<html>
<body bgcolor="#ffffff" text="#000033" link="#666699"  alink=#000000 vlink="#333333">

<title>Condor testsuite results</title>
<center><h2>Condor testsuite results </h2>

<?php
  $dbName = "nmi_history";
  $dbUsername = "nmipublic";
  $dbPassword = ""; 
  $currentDir = "results";

  # get args
  $args["gid"] = $_REQUEST["gid"];
  $args["runid"] = $_REQUEST["runid"];
  $args["description"] = $_REQUEST["description"];
  
  $runid = $args["runid"];
  $description = $args["description"];
  echo "for build tag $description";
  printf("<br><br>", "");

  $query1 = "SELECT * FROM Run where description='$description' and run_type='test' ORDER BY runid";
  $platforms = array();
  $runids = array("0" => "");
  $basedir = "rundir";  
 
  $db = mysql_connect("mysql.batlab.org", "$dbUsername", "$dbPassword") or die ("Could not connect : " . mysql_error());
  mysql_select_db("$dbName") or die("Could not select database");
  $result1 = mysql_query($query1) or die ("Query failed : " . mysql_error());

  while( $myrow = mysql_fetch_array($result1) ) {
    $gid = $myrow["gid"];
    $runid = $myrow["runid"];
    $run_type = $myrow["run_type"];
    $description = $myrow["description"];
    $result = $myrow["result"];
    $base = "$basedir/$gid";
    $temprunid = $myrow["runid"];
    $runids["$temprunid"] = "$base";
  }
  echo "<center><table width=400 border=1 bgcolor=ffffff>";
  echo "<tr bgcolor=ffffff>";
  echo "<td width=100>Runid</td>";  
  echo "<td width=200>Platform</td>";  
  echo "<td width=50>Results</td>";  
  echo "<td width=50>run dir</td>";  
  echo "</tr>"; 

  foreach ($runids as $key => $val) {
    if ($key != "0" ) {
      echo "<tr>";
      $total = 0;
      $found = 0;
      $running = 0;
      $query2 = "SELECT * FROM Task where runid=$key";
      $result2 = mysql_query($query2) or die ("Query failed : " . mysql_error());
      while( $myrow = mysql_fetch_array($result2) ) {
        $platform = $myrow["platform"];
        array_push( $platforms, "$platform" );
        $result = $myrow["result"];
        
        $taskname = $myrow["name"]; 
        if ( $taskname == "remote_task") {
          $total = $result;
          $found = 1;
        }
        # find if the run is complete 
        if ( is_null($result) ) {
          $running = 1;
        }
      } 
      $platforms = array_unique( $platforms ); 
      unset($platforms['local']);

      foreach ($platforms as $key1 ) {
        $currPlat = $key1;   
        unset($platforms['$currPlat']);
      }
      echo "<td><a href=\"http://grandcentral.cs.wisc.edu/$currentDir/Task-search.php?runid=$key\">$key</a></td>";
      printf("<td>%s</td>", $currPlat);
     
      if ($found == 0) { # no remote task result, the test run failed miserably 
        printf("<td bgcolor=red>%s</td>", "testsuite failed");
      } else if ( $running == 1) { # tests in progress
        printf("<td bgcolor=yellow>%s</td>", "running");
      } else if ( $total == 0) { # all tests passed
        printf("<td bgcolor=green>%s</td>", $total);
      } else { # some tests failed 
        printf("<td bgcolor=red>%s</td>", $total);
      }
      echo "<td><a href=\"http://grandcentral.cs.wisc.edu/$val\">run dir</a></td>";
      echo "</tr>";
    }
  }
  echo "</table>";
  echo "</center>";

#mysql_free_result($result1);
#mysql_free_result($result2);
mysql_close($db);

?>
</body>
</html>

