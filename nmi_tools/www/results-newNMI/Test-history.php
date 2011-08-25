<?php   

include "dashboard.inc";

include "Dashboard.php";
$dash = new Dashboard();
$dash->print_header("Condor Build and Test Dashboard");
$dash->connect_to_db();

# get args
$type = "test";
$user = $dash->get_condor_user();
$test = rawurldecode($_REQUEST["test"]);
$branch = $_REQUEST["branch"];
$rows   = ($_REQUEST["rows"] ? $_REQUEST["rows"] : 30);

date_default_timezone_set('America/Chicago');
?>

</head>
<body>

<form method="get" action="<?= $_SERVER{'SCRIPT_NAME'}; ?>">
<input type="hidden" name="branch" value="<?= $branch; ?>">
<input type="hidden" name="type" value="<?= $type; ?>">
<input type="hidden" name="test" value="<?= $test; ?>">
<input type="hidden" name="user" value="<?= $user; ?>">
<font size="+1"><b>Rows: </b></font>&nbsp;
<select name="rows">
<?php
  $arr = Array("30", "60", "90", "120", "150", "300");
foreach ($arr AS $val) {
  echo "<option" . ($val == $rows ? " SELECTED" : "") . ">$val</option>\n";
}
?>

</select>
<input type="submit" value="Show Results">
</form>

<?php

// Create the set of all the runid's we're interested in
// This is the MAX(runid) i.e. latest run  
// for each (branch, type) tuple

echo "<h2>Test History - $branch ($test)</h2>\n";

$sql = "SELECT description,
               convert_tz(start, 'GMT', 'US/Central') as start,
               run_type, 
               runid,
                    archived,
                    archive_results_until,
               result
          FROM Run 
         WHERE component='condor' AND 
               project='condor' AND
               run_type='build' AND
               description LIKE '$branch%'".
       (!empty($user) ? " AND user = '$user'" : "").
       " ORDER BY runid DESC ".
       " LIMIT $rows";

// This time through the runids gather ALL Platforms ever used during the period
$results = $dash->db_query($sql);

$platforms = Array();
$platform_labels = Array();
$plaformlabel;
$component_version; // native or binaries being tested

foreach ($results as $runidrow) {
  $runid = $runidrow["runid"];
  // for each runid find all platforms but only insert
  // them into the platforms array if not there yet
  // we include cross test platforms by finding all
  // test run runids which used the build result as
  // input.
  
  //
  // Platforms
  //
  // Save original platforms we actually built on
  $sql_pls = "SELECT DISTINCT(platform) AS platform, component_version ".
    "  FROM Method_nmi , Task, Run ".
    " WHERE Method_nmi.input_runid = $runid".
    "   AND Task.runid =  Method_nmi.runid ".
    "   AND Task.runid =  Run.runid ".
    "   AND Task.platform != 'local' ".
    " ORDER BY (IF (platform='local', 'zzz', platform))";
  $platform_results = $dash->db_query($sql_pls);
  foreach ($platform_results as $row) {
    $component_version = $row["component_version"];
    if( array_key_exists($row["platform"],$platforms)) {
    }
    else {
      $platforms[$row["platform"]] = $row["platform"];
      if($component_version == 'native') {
	$platform_labels[$row["platform"]] = "\nNative\n";
      }
      else {
	$platform_labels[$row["platform"]] = "Cross of\n".
	  "$component_version"; 
      }
    }
  }
}

// This time through the runids show results
$sql4 = "SELECT description,
               convert_tz(start, 'GMT', 'US/Central') as start,
               run_type, 
               runid,
                    archived,
                    archive_results_until,
               result
          FROM Run 
         WHERE component='condor' AND 
               project='condor' AND
               run_type='build' AND
               description LIKE '$branch%'".
       (!empty($user) ? " AND user = '$user'" : "").
       " ORDER BY runid DESC ".
       " LIMIT $rows";

$results = $dash->db_query($sql4);

// For every runid of a build
$rowcount = 0;
echo "<table border='0' cellspacing='0' >\n";
foreach ($results as $runidrow) {
  $runid    = $runidrow["runid"];
  $desc     = $runidrow["description"];
  $runstart = $runidrow["start"];
  
  $data = array();
	
  $sql = "SELECT host, gid, UNIX_TIMESTAMP(start) AS start ".
         "  FROM Run ".
         " WHERE Run.runid = $runid".
         "   AND Run.user = '$user'";
  $results2 = $dash->db_query($sql);
  $host  = $results2[0]["host"];
  $gid   = $results2[0]["gid"];
  $start = $results2[0]["start"];

  //
  // Platforms
  //
  // Saved in the very beginning 
  
  $testrunids = Array();
  //
  // Test
  //
  if ($type != 'test') {
    die("Unsupported parameter type=$type");
  }
             
  //
  // We also need testrunids
  //
  $runid_sql = "SELECT DISTINCT Method_nmi.runid ".
               "  FROM Method_nmi, Run ".
               " WHERE Method_nmi.input_runid = $runid ".
               "   AND Run.runid = Method_nmi.runid ".
               "   AND Run.user = '$user' ";
  $results2 = $dash->db_query($runid_sql);
  foreach ($results2 as $row) {
    $testrunids[] = $row["runid"];
  }

  $sql2 = "SELECT DISTINCT(Task.name) AS name ".
          "  FROM Task, Method_nmi ".
          " WHERE Task.runid = Method_nmi.runid ".
          "   AND Task.name = '$test' ".
          "   AND Method_nmi.input_runid = $runid ".
          " ORDER BY Task.name ASC";

  $results2 = $dash->db_query($sql2);
  foreach ($results2 as $row) {
    $task_name = $row["name"];
    
    //
    // Now for each task, get the status from the platforms
    //
    $sql3 = "SELECT platform, result, runid ".
            "  FROM Task ".
            " WHERE Task.runid IN (".implode(",", $testrunids).") ".
            "   AND Task.name = '$task_name'";
    $results3 = $dash->db_query($sql3);
    foreach ($results3 as $tash_result) {
      $platform = $task_row["platform"];
      $platform_runids[$platform] = $task_row["runid"];
      $result_value = $task_row["result"];
      
      if (is_null($result_value)) {
	$result_value = "pending";
	$platform_status[$platform] = "pending";
	$task_status[$task_name] = "pending";
      }
      elseif ($result_value) {
	$platform_status[$platform] = "failed";
	$task_status[$task_name] = "failed";
      }
      elseif (!$platform_status[$platform]) {
	$platform_status[$platform] = "passed";
      }

      if (!$task_status[$task_name]) {
	$task_status[$task_name] = "passed";
      }
      $data[$row["name"]][$task_row["platform"]] = $result_value;
    }
  }
  
  if( sizeof($data) == 0 ) {
    echo "<tr><td></td><td colspan='8'><strong>No Tests on ".date("m/d/Y", $start)." - Runid $runid - </strong></td></tr>\n";
  } 

  foreach ($data AS $task => $arr) {
    // we are focusing on a single test history, skip the rest
    $diff = strcmp($task,$test);
    if($diff ==  0) {
      if(($rowcount % 20) == 0) {
	//echo "<table border='0' cellspacing='0' >\n";
	echo "<tr>\n";
	echo "<td>&nbsp</td>\n";
	foreach ($platforms AS $platform) {
	  $display = $platform;
	  $idx = strpos($display, "_");
	  $display[$idx] = " ";
	  $filepath = "";
	  echo "<td>$display</td>\n";
	} // FOREACH 
	echo "</tr>\n";
	// Cross or Native labels
	//echo "<table border='0' cellspacing='0' >\n";
	echo "<tr>\n";
	echo "<td>&nbsp</td>\n";
	foreach ($platform_labels AS $label) {
	  $display = $label;
	  $idx = strpos($display, "_");
	  $display[$idx] = " ";
	  $filepath = "";
	  echo "<td>$display</td>\n";
	} // FOREACH 
	echo "</tr>\n";
      }
      $gotests = "<a href='task-details.php?runid=$runid&type=test&user=cndrauto'>";
      echo "<tr><td><span title='$task'>$gotests".date("m/d/Y", $start)."</a></span></td>\n";
      
      foreach ($platforms AS $platform) {
	$result = $arr[$platform];
	if ($result == "pending") {
	  echo "<td align='center' class='$result'>-</td>\n";
	}
	else {
	  if ($result == '') {
	    echo "<td align='center'>&nbsp;</td>\n";
	  }
	  else {
	    $display = "<a href='task-details.php?platform=$platform&task=".urlencode($task)."&runid=".$platform_runids[$platform]. "'>$result</a>";
	    $class = ($result == 0) ? "passed" : "failed";
	    echo "<td class='$class'align='center'><B>$display</B></td>\n";
	  }
	}
      }
      $rowcount++;
      echo "</td>\n";
    } 
  } // FOREACH
}

echo "</table>";

?>
</body>
</html>

