<?php   
   //
   // Configuration
   //
   define("HISTORY_URL", "./Test-history.php?branch=%s&test=%s");
   define("BRANCH_URL", "./Run-condor-branch.php?branch=%s&user=%s");
   define("DETAIL_URL", "./Run-condor-details.php?runid=%s&type=%s");
   define("CONDOR_USER", "cndrauto");
   
   $result_types = Array( "passed", "pending", "failed" );

   require_once "./load_config.inc";
   load_config();

   # get args
   $nplatform = $_REQUEST["platform"];
   $runid     = (int)$_REQUEST["runid"];
   $user      = $_REQUEST["user"];
   $type      = $_REQUEST["type"];
   $build_id = $runid;
   $branch = "unknown";
   
   define('PLATFORM_PENDING', 'pending');
   define('PLATFORM_FAILED',  'failed');
   define('PLATFORM_PASSED',  'passed');
?>
<html>
<head>
<Title>NMI - Results of Crosstests for build ID:<?= $build_id; ?> from <?= $nplatform; ?> binaries</TITLE>
<LINK REL="StyleSheet" HREF="condor.css" TYPE="text/css">
</HEAD>
<body>

<?php

$db = mysql_connect(WEB_DB_HOST, DB_READER_USER, DB_READER_PASS) or die ("Could not connect : " . mysql_error());
mysql_select_db(DB_NAME) or die("Could not select database");

include "dashboard.inc";

// 
// need to have the branch if we get a request for a test history
// Test-history.php?branch=xxxxx&test=yyyyyy
//

$query_branch="
        
        SELECT 
            LEFT(description,
            (IF(LOCATE('branch-',description),
            LOCATE('branch-',description)+5,
            (IF(LOCATE('trunk-',description),
            LOCATE('trunk-',description)+4,
            CHAR_LENGTH(description)))))) AS branch
        FROM 
            Run 
        WHERE 
            runid=$runid";
    
$result = mysql_query($query_branch) or die ("Query ".$query_branch." failed : " . mysql_error()); 
$row = mysql_fetch_array($result);
$branch = $row["branch"];

$sql = "SELECT host, gid, UNIX_TIMESTAMP(start) AS start ".
       "  FROM Run ".
       " WHERE Run.runid = $build_id".
       "   AND Run.user = '$user'";

$result = mysql_query($sql) or die ("Query $sql failed : " . mysql_error());
$row = mysql_fetch_array($result);
$host  = $row["host"];
$gid   = $row["gid"];
$start = $row["start"];
mysql_free_result($result);

echo "<h1><a href=\"./Run-condor.php\" class=\"title\">Condor Latest Build/Cross Test Results</a> ".
":: ".ucfirst($nplatform)." binaries from Build ID $runid (".date("m/d/Y", $start).")</h1>\n";

//
// Create array of runids to display results for starting with native
// source binary test on its native platform.
//

$full_runids = Array();
$platforms = Array();
$platforms[] = $nplatform;
$native_runid;

// --------------------------------
// CROSS TESTS RUNIDS
// --------------------------------
// runids we are cross testing
$sql = "SELECT Run.runid as xrunid ".
       "  FROM Run, Method_nmi ".
       " WHERE Method_nmi.input_runid = {$runid} AND ".
       "       Run.runid = Method_nmi.runid AND ".
       "       Run.user = '$user'  AND ".
       "       project_version != component_version AND ".
       "       component_version = '$nplatform' ".
       " ORDER BY Run.runid ";
$result = mysql_query($sql) or die ("Query $sql failed : " . mysql_error());
while ($row = mysql_fetch_array($result)) {
  $tmpp = $xrunids[] = $row["xrunid"];
}
mysql_free_result($result);

// Now find the runids of the native test runs
// --------------------------------
// NATIVE TESTS RUNIDS
// --------------------------------
// runids we are cross testing
$sql = "SELECT Run.runid as nrunid ".
       "  FROM Run, Method_nmi ".
       " WHERE Method_nmi.input_runid = {$runid} AND ".
       "       Run.runid = Method_nmi.runid AND ".
       "       Run.user = '$user'  AND ".
       "       project_version != component_version AND ".
       "       component_version = 'native' ".
       " ORDER BY Run.runid ";
$result = mysql_query($sql) or die ("Query $sql failed : " . mysql_error());
while ($row = mysql_fetch_array($result)) {
  $tmpp = $nrunids[] = $row["nrunid"];
}
mysql_free_result($result);

// find the runid of the native platform tests
// then build the set of runids to test with
// native tests first

foreach ($nrunids as $possible) {
  $sql = "SELECT DISTINCT (Task.platform) AS platform ".
    "  FROM Task ".
    " WHERE Task.runid = $possible ".
    "   AND Task.platform != 'local' ";
  $result = mysql_query($sql) or die ("Query $sql failed : " . mysql_error());
  while ($row = mysql_fetch_array($result)) {
    $tmpp = $row["platform"];
  }
  if($tmpp ==  $nplatform) {
    $native_runid = $full_runids[] = $possible;
    foreach ($xrunids as $testrun) {
      $full_runids[] = $testrun;
    }
  }
  mysql_free_result($result);
}

// for all of the cross tests find platform name 
// and add it to the platform list in same order as runids.
	
foreach ($xrunids as $targets) {
  $sql = "SELECT DISTINCT (Task.platform) AS platform ".
    "  FROM Task ".
    " WHERE Task.runid = $targets ".
    "   AND Task.platform != 'local' ";
  $result = mysql_query($sql) or die ("Query $sql failed : " . mysql_error());
  while ($row = mysql_fetch_array($result)) {
    $tmpp = $platforms[] = $row["platform"];
  }
}
	
// set of runids to display starting with native test
$plates = implode(',',$platforms);
   
// set of runids to display starting with native test
$finalids = implode(',',$full_runids);
   
// find complete task list
$sql = "SELECT DISTINCT(Task.name) AS name ".
       "  FROM Task,  Method_nmi  ".
       " WHERE Task.runid = Method_nmi.runid ".
       "   AND Method_nmi.input_runid = $runid ".
       " ORDER BY Task.start ASC";
$result = mysql_query($sql) or die ("Query $sql failed : " . mysql_error());
while ($row = mysql_fetch_array($result)) {
  $task_name = $row["name"];
  //
  // Now for each task, get the status from the platforms
  //
  $sql = "SELECT platform, result, runid ".
    "  FROM Task ".
    " WHERE Task.runid IN (".implode(",", $full_runids).") ".
    "   AND Task.name = '$task_name'";
  $task_result = mysql_query($sql) or die ("Query $sql failed : " . mysql_error());
  while ($task_row = mysql_fetch_array($task_result)) {
    $platform = $task_row["platform"];
    $platform_runids[$platform] = $task_row["runid"];
    $result_value = $task_row["result"];
    
    if (is_null($result_value)) {
      $result_value = PLATFORM_PENDING;
      $platform_status[$platform] = PLATFORM_PENDING;
      $task_status[$task_name] = PLATFORM_PENDING;
    }
    elseif ($result_value) {
      $platform_status[$platform] = PLATFORM_FAILED;
      $task_status[$task_name] = PLATFORM_FAILED;
    }
    elseif (!$platform_status[$platform]) {
      $platform_status[$platform] = PLATFORM_PASSED;
    }
    if (!$task_status[$task_name]) {
      $task_status[$task_name] = PLATFORM_PASSED;
    }
    $data[$row["name"]][$task_row["platform"]] = $result_value;
  } // WHILE
  mysql_free_result($task_result);
} // WHILE
mysql_free_result($result);
?>

<table border="0" cellspacing="0" >
<tr>
   <td>Name</td>

<?php
foreach ($platforms AS $platform) {
  // We will remove 'nmi:' from the front of the platform and also split it 
  // onto two separate lines because the length of the header determines the
  // width of the resulting table column.
  $display = preg_replace("/nmi:/", "", $platform);
  $display = preg_replace("/_/", "_ ", $display, 1);
  
  // have to lookup the file location now
  $filepath = "";
  $loc_query = "SELECT * FROM Run WHERE runid='$platform_runids[$platform]'";
  $loc_query_res = mysql_query($loc_query) or die ("Query failed : " . mysql_error());
  while( $locrow = mysql_fetch_array($loc_query_res) ) {
    $filepath = $locrow["filepath"];
  }

  # Get the queue depth for the platform if it is pending
  $queue_depth = "";
  if($platform_status[$platform] == PLATFORM_PENDING) {
    $ret = get_queue_for_nmi_platform($platform, $type);
    $queue_depth = $ret[1];
  }

  $display = "<a href=\"$filepath/$gid/userdir/$platform/\" title=\"View Run Directory\">$display</a>";
  echo "<td align=\"center\" class=\"".$platform_status[$platform]."\">$display $queue_depth</td>\n";
} // FOREACH

foreach ($data AS $task => $arr) {
  // need $branch still
  $history_url = sprintf(HISTORY_URL,urlencode($branch),rawurlencode($task));
  $history_disp = "<a href=\"$history_url\">".limitSize($task, 30)."</a>";
  echo "<tr>\n".
    "<td ".($task_status[$task] != PLATFORM_PASSED ?
	    "class=\"".$task_status[$task]."\"" : "").">".
    //"<span title=\"$task\">".limitSize($task, 30)."</span></td>\n";
    "<span title=\"$task\">$history_disp</span></td>\n";
  
  foreach ($platforms AS $platform) {
    $result = $arr[$platform];
    if ($result == PLATFORM_PENDING) {
      echo "<td align=\"center\" class=\"{$result}\">-</td>\n";
    }
    else {
      if ($result == '') {
	echo "<td align=\"center\">&nbsp;</td>\n";
      }
      else {
	$display = "<a href=\"http://$host/results/Run-condor-taskdetails.php?platform={$platform}&task=".urlencode($task)."&runid=".$platform_runids[$platform]. "\">$result</a>";
	echo "<td class=\"".($result == 0 ? PLATFORM_PASSED : PLATFORM_FAILED)."\"".
	  "align=\"center\"><B>$display</B></td>\n";
      }
    }
  }
  echo "</td>\n";
} // FOREACH
echo "</table>";

function limitSize($str, $cnt) {
  if (strlen($str) > $cnt) {
    $str = substr($str, 0, $cnt - 3)."...";
  }
  return ($str);
}

// done looking up locations.....mysql_close($db);
mysql_close($db);

?>
</body>
</html>

