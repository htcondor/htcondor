<?php   

define("TASK_URL", "task-details.php?platform=%s&task=%s&runid=%s");

include "Dashboard.php";
$dash = new Dashboard();
$dash->print_header("Condor Build and Test Dashboard");
$dash->connect_to_db();

# get args
$runid = (int)$_REQUEST["runid"];
$sha1 = $_REQUEST["sha1"];

// If we get a SHA1 and not a run id, look up the run id
if(!$runid and $sha1) {
  $sql = "SELECT runid FROM Run WHERE project_version='$sha1'";
  $result = $dash->db_query($sql);
  $runid = $result[0]["runid"];
}

print "</head>\n";
print "<body>\n";

echo "<h2>Results for build ID $runid</h2>\n";


//
// Get the platforms, and prepare some data structures
//
$platforms = $dash->get_platform_list_for_runid($runid);

$build_headers = Array();
$test_headers = Array();
foreach ($platforms as $platform) {
  $build_headers[$platform] = "passed";
  $test_headers[$platform] = "passed";
}

//
// Get build task info   
//
$sql = "SELECT
          platform,
          name,
          result,
          host,
          TIME_TO_SEC(TIMEDIFF(Finish, Start)) as length
       FROM
          Task
       WHERE 
          runid = $runid 
       ORDER BY
          start ASC";

$results = $dash->db_query($sql);

foreach ($results as $row) {
  if(!array_key_exists($row["name"], $build_tasks)) {
    $build_tasks[$row["name"]] = Array();
  }
  $build_tasks[$row["name"]][$row["platform"]] = Array($row["result"], $row["length"], $row["host"]);

  $result = map_result($row["result"]);
  if($result == "failed") { 
    $build_headers[$row["platform"]] = "failed";
  }
  elseif($result == "pending" and $build_headers[$row["platform"]] != "failed") {
    $build_headers[$row["platform"]] = "pending";    
  }
}


//
// Get test task info
//
$test_runids = $dash->get_test_runids_for_build($runid);

$sql = "SELECT 
          platform,
          Task.name,
          result,
          host,
          TIME_TO_SEC(TIMEDIFF(Finish, Start)) as length
        FROM
          Task,
          Method_nmi
        WHERE
          Task.runid = Method_nmi.runid AND
          Method_nmi.input_runid = $runid
        ORDER BY
          Task.start ASC";

$results = $dash->db_query($sql);

foreach ($results as $row) {
  if(!array_key_exists($row["name"], $test_tasks)) {
    $test_tasks[$row["name"]] = Array();
  }

  $test_tasks[$row["name"]][$row["platform"]] = Array($row["result"], $row["length"], $row["host"]);

  $result = map_result($row["result"]);
  if($result == "failed") { 
    $test_headers[$row["platform"]] = "failed";
  }
  elseif($result == "pending" and $test_headers[$row["platform"]] != "failed") {
    $test_headers[$row["platform"]] = "pending";    
  }
}


echo "<table border='0' cellspacing='0'>\n";
echo "<tr>\n";
echo "   <th>Build Tasks</th>\n";


// show link to run directory for each platform
foreach ($platforms AS $platform) {
  $display = preg_replace("/nmi:/", "", $platform);
   
  /*  
  # Get the queue depth for the platform if it is pending
  $queue_depth = "";
  if($platform_status[$platform] == PLATFORM_PENDING) {
    $queue_depth = get_queue_for_nmi_platform($platform, $dash);
  }

  */  

  $queue_depth = "";

  if(preg_match("/^x86_64_/", $display)) {
    $display = preg_replace("/x86_64_/", "x86_64<br>", $display);
  }
  elseif(preg_match("/ia64_/", $display)) {
    $display = preg_replace("/ia64_/", "x86<br>", $display);
  }
  else {
    $display = preg_replace("/x86_/", "x86<br>", $display);
  }

  $display = "<font style='font-size:75%'>$display</font>";

  echo "<td align='center' class='".$platform_status[$platform]."'>$display $queue_depth</td>\n";
}


//
// Print build hosts
//
print "<tr>\n";
print "  <td>Hosts</td>\n";
foreach ($platforms as $platform) {
  // Lookup the file location now
  $loc_query = "SELECT filepath,gid FROM Run WHERE runid='$runid'";
  $loc_query_results = $dash->db_query($loc_query);
  foreach ($loc_query_results as $loc_row) {
    $filepath = $loc_row["filepath"];
    $mygid = $loc_row["gid"];
  }

  $class = $build_headers[$platform];
  $remote_host = "";
  if(array_key_exists("remote_task", $build_tasks)) {
    if(array_key_exists($platform, $build_tasks["remote_task"])) {
      $host = preg_replace("/\.batlab\.org/", "", $build_tasks["remote_task"][$platform][2]);
      print "<td class='$class'><font style='font-size:75%'><a href='$filepath/$mygid/userdir/$platform/'>$host</a></font></td>";
      continue;
    }
  }
  print "<td class='$class'>&nbsp;</td>";
}

// 
// Show build results
//
foreach ($build_tasks as $task_name => $results) {
  $totals = Array("passed" => 0, "pending" => 0, "failed" => 0);
  $output = "";  
  foreach ($platforms as $platform) {
    if(array_key_exists($platform, $results)) {
      $class = map_result($results[$platform][0]);
      $totals[$class] += 1;
      $link = sprintf(TASK_URL, $platform, urlencode($task_name), $runid);
      $output .= "  <td class=\"$class\"><a href='$link'>" . $results[$platform][0] . "</a></td>\n";
    }
    else {
      $output .= "<td>&nbsp;</td>\n";
    }
  }

  $class = "";
  if($totals["failed"] > 0) { $class = "failed"; }
  elseif($totals["pending"] > 0) { $class = "pending"; }

  print "<tr>\n";
  print "  <td class=\"left $class\">" . limitSize($task_name,30) . "</td>\n";
  print $output;
  print "</tr>\n";
}


//
// Show test results
//
$num_platforms = count($platforms);
print "<tr><td style='border-bottom-width:0px' colspan=" . ($num_platforms+1) . ">&nbsp;</td></tr>\n";
print "<tr><th>Test Tasks</th><th colspan=$num_platform>&nbsp</th></tr>\n";


//
// Print test hosts
//
print "<tr>\n";
print "  <td>Hosts</td>\n";
foreach ($platforms as $platform) {
  // Lookup the file location now
  $test_runid = $test_runids[$platform];
  $loc_query = "SELECT filepath,gid FROM Run WHERE runid='$test_runid'";
  $loc_query_results = $dash->db_query($loc_query);
  foreach ($loc_query_results as $loc_row) {
    $filepath = $loc_row["filepath"];
    $mygid = $loc_row["gid"];
  }

  $remote_host = "";
  if(array_key_exists("remote_task", $test_tasks)) {
    if(array_key_exists($platform, $test_tasks["remote_task"])) {
      $host = preg_replace("/\.batlab\.org/", "", $test_tasks["remote_task"][$platform][2]);
      $class = $test_headers[$platform];
      print "<td class='$class'><font style='font-size:75%'><a href='$filepath/$mygid/userdir/$platform/'>$host</a></font></td>";
      continue;
    }
  }
  print "<td>&nbsp;</td>";
}

foreach ($test_tasks as $task_name => $results) {  
  $totals = Array("passed" => 0, "pending" => 0, "failed" => 0);
  $output = "";
  foreach ($platforms as $platform) {
    if(array_key_exists($platform, $results)) {
      $class = map_result($results[$platform][0]);
      $totals[$class] += 1;
      $link = sprintf(TASK_URL, $platform, urlencode($task_name), $test_runids[$platform]);
      $output .= "  <td class=\"$class\"><a href='$link'>" . $results[$platform][0] . "</a></td>\n";
    }
    else {
      $output .= "<td>&nbsp;</td>\n";
    }
  }

  $class = "";
  if($totals["failed"] > 0) { $class = "failed"; }
  elseif($totals["pending"] > 0) { $class = "pending"; }

  print "<tr>\n";
  print "  <td class=\"left $class\">" . limitSize($task_name,30) . "</td>\n";
  print $output;
  print "</tr>\n";
}

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

function map_result($result) {
  if(is_null($result)) {
    return "pending";
  }
  elseif($result == 0) {
    return "passed";
  }
  else {
    return "failed";
  }
}


?>
</body>
</html>
