<?php   
//
// Configuration
//
define("HISTORY_URL", "./Test-history.php?branch=%s&test=%s");

$result_types = Array( "passed", "pending", "failed" );

include "dashboard.inc";

include "Dashboard.php";
$dash = new Dashboard();
$dash->print_header("Condor Build and Test Dashboard");
$dash->connect_to_db();

# get args
$type     = $_REQUEST["type"];
$runid    = (int)$_REQUEST["runid"];
$user     = $_REQUEST["user"];
$timed    = array_key_exists("timed", $_REQUEST) ? $_REQUEST["timed"] : "";
$build_id = $runid;
$branch   = "unknown";

define('PLATFORM_PENDING', 'pending');
define('PLATFORM_FAILED',  'failed');
define('PLATFORM_PASSED',  'passed');
?>

</head>
<body>

<?php

// 
// need to have the branch if we get a request for a test history
// Test-history.php?branch=xxxxx&test=yyyyyy
//

$query_branch="SELECT 
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

$results = $dash->db_query($query_branch);
$branch = $results[0]["branch"];


$sql = "SELECT host, gid, UNIX_TIMESTAMP(start) AS start
        FROM Run
        WHERE Run.runid = $build_id
          AND Run.user = '$user'";

$results = $dash->db_query($sql);
$host  = $results[0]["host"];
$gid   = $results[0]["gid"];
$start = $results[0]["start"];

echo "<h2>" . ucfirst($type) . "s for Build ID $runid $branch (" . date("m/d/Y", $start) . ")</h2>\n";

if(!$timed) {
  echo "<p><a href='http://" . $_SERVER['HTTP_HOST']  . $_SERVER['REQUEST_URI'] . "&timed=1'>Show this page with test times</a>\n";
}

//
// Platforms
//
// Original platforms we actually built on
$sql = "SELECT DISTINCT(platform) AS platform
        FROM Run, Task
        WHERE Task.runid = $runid
          AND Task.runid = Run.runid
          AND Run.user = '$user' ";
//"   AND Task.platform != 'local' ".
//" ORDER BY (IF (platform='local', 'zzz', platform))";
$results = $dash->db_query($sql);
$platforms = Array();
foreach ($results as $row) {
  $tmpp = $platforms[] = $row["platform"];
  //echo "<H3>$tmpp</H3>";
}
   
$runids = Array();
if ($type == 'build') {
  $sql = "SELECT DISTINCT(Task.name) AS name ".
    "  FROM Task ".
    " WHERE Task.runid = $runid ".
    " ORDER BY Task.start ASC";
  $runids[] = $runid;
}
elseif ($type == 'test') {
  $sql = "SELECT DISTINCT(Task.name) AS name ".
    "  FROM Task, Method_nmi ".
    " WHERE Task.runid = Method_nmi.runid ".
    "   AND Method_nmi.input_runid = $runid ".
    " ORDER BY Task.start ASC";
             
  //
  // We also need runids
  //
  $runid_sql = "SELECT DISTINCT Method_nmi.runid ".
    "  FROM Method_nmi, Run ".
    " WHERE Method_nmi.input_runid = $runid ".
    "   AND Run.runid = Method_nmi.runid ".
    "   AND (component_version = project_version or ( component_version = 'native'))".
    "   AND Run.user = '$user' ";
  $results = $dash->db_query($runid_sql);
  foreach ($results as $row) {
    $tmp = $runids[] = $row["runid"];
    //echo "<H3>$tmp</H3>";
    //echo "<H3>$tmp, $row["component_version"], $row["platform"]</H3>";
  }
}
else {
  // Type is unknown
  die("Unsupported parameter type=$type");
}

$results = $dash->db_query($sql);
foreach ($results as $row) {
  $task_name = $row["name"];
  
  //
  // Now for each task, get the status from the platforms
  //
  $sql = "SELECT platform, result, runid, TIME_TO_SEC(TIMEDIFF(Finish, Start)) as length ".
    "  FROM Task ".
    " WHERE Task.runid IN (".implode(",", $runids).") ".
    "   AND Task.name = '$task_name'";
  $task_results = $dash->db_query($sql);
  foreach ($task_results as $task_row) {
    $platform = $task_row["platform"];
    $platform_runids[$platform] = $task_row["runid"];
    $result_value = $task_row["result"];
    $length = $task_row["length"];
    
    if (is_null($result_value)) {
      $result_value = PLATFORM_PENDING;
      $platform_status[$platform] = PLATFORM_PENDING;
      $task_status[$task_name] = PLATFORM_PENDING;
    }
    elseif ($result_value) {
      $platform_status[$platform] = PLATFORM_FAILED;
      $task_status[$task_name] = PLATFORM_FAILED;
    }
    elseif(!array_key_exists($platform, $platform_status)) {
      $platform_status[$platform] = PLATFORM_PASSED;
    }

    if(!array_key_exists($task_name, $task_status)) {
      $task_status[$task_name] = PLATFORM_PASSED;
    }
    $data[$row["name"]][$task_row["platform"]] = Array($result_value, $length);
  }
}

echo "<table border='0' cellspacing='0'>\n";
echo "<tr>\n";
echo "   <td>Name</td>\n";

if($type == "build") {
  $hosts = get_hosts(Array($runid));
}
elseif($type == "test") {
  $hosts = get_hosts(array_values($platform_runids));
}

// show link to run directory for each platform
foreach ($platforms AS $platform) {
  $display = preg_replace("/nmi:/", "", $platform);
   
  // have to lookup the file location now
  $filepath = "";
  $loc_query = "SELECT * FROM Run WHERE runid='$platform_runids[$platform]'";
  $loc_query_results = $dash->db_query($loc_query);
  foreach ($loc_query_results as $loc_row) {
    $filepath = $loc_row["filepath"];
    $mygid = $loc_row["gid"];
  }
  
  # Get the queue depth for the platform if it is pending
  $queue_depth = "";
  if($platform_status[$platform] == PLATFORM_PENDING) {
    $queue_depth = get_queue_for_nmi_platform($platform, $type);
  }

  $remote_host = "";
  $true_runid = $platform_runids[$platform];
  if(array_key_exists($display, $hosts[$true_runid])) {
    $remote_host = "<br><font style='font-size:75%'>" . $hosts[$true_runid][$display] . "</font>";
  }

  $display = "<a href='$filepath/$mygid/userdir/$platform/' title='View Run Directory'>$display</a>";
  echo "<td align='center' class='".$platform_status[$platform]."'>$display $remote_host $queue_depth</td>\n";
}

?>
<tr>
   <td>Results</td>
<?php
// show link to results for each platform
foreach ($platforms AS $platform) {
  // have to lookup the file location now
  $filepath = "";
  $loc_query = "SELECT * FROM Run WHERE runid='$platform_runids[$platform]'";
  $loc_query_results = $dash->db_query($loc_query);
  foreach ($loc_query_results as $loc_row) {
    $filepath = $loc_row["filepath"];
    $mygid = $loc_row["gid"];
  }

  $display = "<a href='$filepath/$mygid/userdir/$platform/results.tar.gz' title='View Run Directory'>click</a>";
  echo "<td align='center' class='".$platform_status[$platform]."'>$display</td>\n";
}

foreach ($data AS $task => $arr) {
  if ($type == 'test') {
    $history_url = sprintf(HISTORY_URL,$branch,rawurlencode($task));
    $history_disp = "<a href=\"$history_url\">".limitSize($task, 30)."</a>";
    echo "<tr>\n".
      "<td ".($task_status[$task] != PLATFORM_PASSED ? 
              "class=\"".$task_status[$task]."\"" : "").">".
      "<span title=\"$task\">$history_disp</span></td>\n";
  }
  else {
    echo "<tr>\n".
      "<td ".($task_status[$task] != PLATFORM_PASSED ? 
              "class=\"".$task_status[$task]."\"" : "").">".
      "<span title=\"$task\">".limitSize($task, 30)."</span></td>\n";
  }
  foreach ($platforms AS $platform) {
    if(!array_key_exists($platform, $arr)) {
      echo "<td align='center'>&nbsp;</td>\n";
      continue;
    }

    $result = $arr[$platform][0];
    $length = $arr[$platform][1];
    if ($result == PLATFORM_PENDING) {
      echo "<td align='center' class='$result'>-</td>\n";
    }
    else {
      if ($result == '') {
        echo "<td align='center'>&nbsp;</td>\n";
      }
      else {
        $height = "";
        if($timed) {
          if($task != "platform_job" and $task != "remote_task") {
            # Keep the font from getting too small or too large
            $size = ($length < 50) ? 50 : (($length > 1000) ? 1000 : $length);
            $height = "font-size:$size%;";
          }
        }

        $display = "";
        if($timed) {
          $display .= sec_to_min($length) . " (";
        }

        $display .= "<a href=\"task-details.php?platform=$platform&task=".urlencode($task)."&type=".$type."&runid=".$platform_runids[$platform]. "\">$result</a>";

        if($timed) {
          $display .= ")";
        }

        echo "<td class='".($result == 0 ? PLATFORM_PASSED : PLATFORM_FAILED)."' style='text-align:center;$height'>$display</td>\n";
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

function sec_to_min($sec) {
  $min = floor($sec / 60);
  $sec = $sec % 60;
  if($sec < 10) {
    $sec = "0$sec";
  }
  return "$min:$sec";
}

?>
</body>
</html>
