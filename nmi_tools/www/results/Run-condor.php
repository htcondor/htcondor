<?php
//
// Configuration
//
define("AUTO_URL", "./Run-condor.php?oneoffs=0");
define("ONEOFFS_URL", "./Run-condor.php?oneoffs=1");
define("QUEUE_DEPTH_URL", "./Queue-depth.php");
define("BRANCH_URL", "./Run-condor-branch.php?branch=%s&user=%s");
define("DETAIL_URL", "./Run-condor-details.php?runid=%s&type=%s&user=%s");
define("CROSS_DETAIL_URL", "./Run-condor-pre-details.php?runid=%s&type=%s&user=%s");
define("CONDOR_USER", "cndrauto");
define("NUM_SPARK_DAYS", 3);

$one_offs = $_REQUEST["oneoffs"];

require_once "./load_config.inc";
load_config();
include "last.inc";

//
// These are the platforms that never have tests in the nightly builds
// This is a hack for now and there's no way to differentiate between different
// branchs. But you know, life is funny that way -- you never really get what you want
//
//$no_test_platforms = Array( "ppc_macos_10.4", "x86_macos_10.4" );
$no_test_platforms = Array( );
?>
<html>
<head>
<Title>NMI - Condor Latest Build/Test Results</TITLE>
<LINK REL="StyleSheet" HREF="condor.css" TYPE="text/css">
</HEAD>
<body>

<?php
$db = mysql_connect(WEB_DB_HOST, DB_READER_USER, DB_READER_PASS) or die ("Could not connect : " . mysql_error());
mysql_select_db(DB_NAME) or die("Could not select database");

$user_condition = $one_offs ? "user!='" . CONDOR_USER . "'" : "user='" . CONDOR_USER . "'";

$query_runids="
SELECT 
  LEFT(description,
       (IF(LOCATE('branch-',description),
         LOCATE('branch-',description)+5,
         (IF(LOCATE('trunk-',description),
            LOCATE('trunk-',description)+4,
            CHAR_LENGTH(description)))))) AS branch,
  MAX(convert_tz(start, 'GMT', 'US/Central')) AS start,
  unix_timestamp(MAX(convert_tz(start, 'GMT', 'US/Central'))) AS start_epoch,
  run_type, 
  max(runid) as runid,
  user,
  archive_results_until,
  result
FROM 
  Run 
WHERE 
  component='condor' AND 
  project='condor' AND
  run_type='build' AND
  description != '' AND 
  DATE_SUB(CURDATE(), INTERVAL 10 DAY) <= start AND
  $user_condition
GROUP BY 
  branch,
  user,
  run_type 
ORDER BY 
  runid desc
";

$result = mysql_query($query_runids) or die ("Query $query_runids failed : " . mysql_error());
$continuous_buf = Array();
$cndrauto_buf = Array();
$oneoff_buf = Array();
while ($row = mysql_fetch_array($result)) {
  $runid      = $row["runid"];
  $branch     = $row["branch"];
  $user       = $row["user"];
  $run_result = $row["result"];

  if(preg_match("/Continuous/", $branch)) {
    $continuous_buf[$branch] = Array();
    $continuous_buf[$branch]["user"] = $user;
    $continuous_buf[$branch]["results"] = create_sparkline($branch, $user);
  }
  else {
    if($user == CONDOR_USER) {
      $cndrauto_buf[$branch] = Array();
      $cndrauto_buf[$branch]["user"] = $user;
      $cndrauto_buf[$branch]["start"] = $row["start"];
      $cndrauto_buf[$branch]["start_epoch"] = $row["start_epoch"];
      $cndrauto_buf[$branch]["results"] = get_results(&$cndrauto_buf[$branch], $runid, $user, $row["result"]);
    }
    else {
      # The branch might not be unique for one-off builds so we need to make a more unique key
      $key = "$branch ($user)";
      $oneoff_buf[$key] = Array();
      $oneoff_buf[$key]["branch"]  = $branch;
      $oneoff_buf[$key]["user"]    = $user;
      $oneoff_buf[$key]["start"]   = $row["start"];
      $oneoff_buf[$key]["results"] = get_results(&$oneoff_buf[$key], $runid, $user, $row["result"]);
    }
  }
}
mysql_free_result($result);
mysql_close($db);

// Create the sidebar
echo "<div id='wrap'>";
make_sidebar();

// Now create the HTML tables.
echo "<div id='main'>\n";

//
// 1) Continuous builds
//
if(!$one_offs) {  
  echo "<h2>Continuous Builds</h2>\n";
  
  $info = "<ul>\n";
  $info .= "  <li>The last " . NUM_SPARK_DAYS . " days of results are shown for each platform.</li>\n";
  $info .= "  <li>The newest results are at the left.</li>\n";
  $info .= "  <li>Thick black bars designate commits to the repository between runs.</li>\n";
  $info .= "  <li>The number shown in the build line is the hour in which the test ran.</li>\n";
  $info .= "  <li>If a number is shown in the test line it is the number of tests that failed.</li>\n";
  $info .= "</ul>";
  echo "<span class=\"link\" style=\"font-size:100%\"><a href=\"javascript: void(0)\" style=\"text-decoration:none\">Hover here to have this section explained<span style=\"width:450px;\">$info</span></a></span>";
  
  echo "<table>\n";
  echo "<tr><th>Platform</th><th>Results</th></tr>\n";

  $branches = array_keys($continuous_buf);
  sort($branches);

  foreach ($branches as $branch) {
    $info = $continuous_buf[$branch];
    $branch_url = sprintf(BRANCH_URL, $branch, $info["user"]);
    $branch_display = preg_replace("/^Continuous Build - /", "", $branch);
    echo "<tr>\n";
    echo "  <td><a href='$branch_url'>$branch_display</a></td>\n";
    echo "  <td>" . $info["results"] . "</td>\n";
    echo "</tr>\n";
  }
  echo "</table>\n";
}



//
// 2) cndrauto builds
//
if(!$one_offs) {
  echo "<h2>Auto builds - user '" . CONDOR_USER . "'</h2>\n";
  echo "<table border=1>\n";
  echo "<tr><th>Branch</th><th>Runid</th><th>Submitted</th><th>Build Results</th><th>Test Results</th><th>Cross Test Results</th>\n";
  echo "</tr>\n";
  
  $branches = array_keys($cndrauto_buf);
  usort($branches, "cndrauto_sort");

  foreach ($branches as $branch) {
    $info = $cndrauto_buf[$branch];
    $branch_url = sprintf(BRANCH_URL, $branch, $info["user"]);

    // Print a "warning" if this branch has not been submitted for >1 day.  This makes it easier to spot old
    // builds from the dashboard.
    $warning = "";
    $diff = time() - $info["start_epoch"];
    if($diff > 60*60*24) {
      $days = floor($diff / (60*60*24));
      $warning = "<p style='font-size:75%; color:red;'>$days+ days old</p>";
    }

    $style = "";
    if(preg_match("/^NMI/", $branch)) {
      $style = "style='border-bottom-width:4px;'";
    }
    else {
      $style = "style='border-top-width:4px;'";
    }

    echo "<tr>\n";
    echo "  <td $style><a href='$branch_url'>$branch</a><br><font size='-2'>" . $info["pin"] . "</font></td>\n";
    echo "  <td $style align='center'>" . $info["runid"] . "</td>\n";
    echo "  <td $style align='center'>" . $info["start"] . $warning . "</td>\n";
    echo $info["results"];
    echo "</tr>\n";
  }
  echo "</table>\n";
}
  
//
// 3) One-off builds
//
if($one_offs) {
  echo "<h2>One-off builds</h2>\n";
  echo "<table border=1>\n";
  echo "<tr><th>Branch</th><th>Runid</th><th>Submitted</th><th>User</th><th>Build Results</th><th>Test Results</th><th>Cross Test Results</th>\n";
  echo "</tr>\n";
  
  foreach (array_keys($oneoff_buf) as $key) {
    $info = $oneoff_buf[$key];
    $branch_url = sprintf(BRANCH_URL, $info["branch"], $info["user"]);
    echo "<tr>\n";
    echo "  <td><a href='$branch_url'>" . $info["branch"] . "</a><br><font size='-2'>" . $info["pin"] . "</font></td>\n";
    echo "  <td align='center'>" . $info["runid"] . "</td>\n";
    echo "  <td align='center'>" . $info["start"] . "</td>\n";
    echo "  <td align='center'>" . $info["user"] . "</td>\n";
    echo $info["results"];
    echo "</tr>\n";
  }
  echo "</table>\n";
}

echo "</div>\n";
echo "<div id='footer'>&nbsp;</div>\n";
echo "</div>\n";

function get_results($info, $runid, $user, $run_result) {
  // We need to get information about the builds, tests, and crosstests for each run
  $html = ""; // the return information

  // --------------------------------
  // BUILDS
  // --------------------------------
  $sql = "SELECT platform," . 
         "       SUM(IF(result != 0, 1, 0)) AS failed," . 
         "       SUM(IF(result IS NULL, 1, 0)) AS pending " . 
         "  FROM Task ".
         " WHERE runid = $runid AND ".
         "       platform != 'local' ".
         " GROUP BY platform ";
  $result2 = mysql_query($sql) or die ("Query $sql failed : " . mysql_error());
  $data["build"] = Array();
  $data["build"]["totals"] = Array();
  $data["build"]["platforms"] = Array();
  while ($platforms = mysql_fetch_array($result2)) {
    $data["build"]["totals"]["total"]++;
    if($platforms["failed"] > 0) {
      $data["build"]["totals"]["failed"]++;
      $data["build"]["platforms"][$platforms["platform"]] = "failed";
    }
    elseif($platforms["pending"] > 0) {
      $data["build"]["totals"]["pending"]++;
      $data["build"]["platforms"][$platforms["platform"]] = "pending";
    }
    else {
      $data["build"]["totals"]["passed"]++;
      $data["build"]["platforms"][$platforms["platform"]] = "passed";       
    }
  }
  mysql_free_result($result2);

  // --------------------------------
  // TESTS
  // --------------------------------
  $sql = "SELECT SUM(IF(Task.result = 0, 1, 0)) AS passed, ".
         "       SUM(IF(Task.result != 0, 1, 0)) AS failed, ".
         "       SUM(IF(Task.result IS NULL, 1, 0)) AS pending, ".
         "       Task.platform" . 
         "  FROM Task, Run, Method_nmi ".
         " WHERE Method_nmi.input_runid = $runid AND ".
         "       Run.runid = Method_nmi.runid AND ".
         "       Run.user = '$user'  AND ".
         "       Task.runid = Run.runid AND ".
         "       Task.platform != 'local' AND ".
         "       ((Run.project_version = Run.component_version)  OR (Run.component_version = 'native' ))".
         " GROUP BY Task.platform ";
  $result2 = mysql_query($sql) or die ("Query $sql failed : " . mysql_error());
  $data["test"] = Array();
  $data["test"]["totals"] = Array();
  $data["test"]["platforms"] = Array();
  while ($platforms = mysql_fetch_array($result2)) {
    $data["test"]["totals"]["total"]++;
    if($platforms["failed"] > 0) {
      $data["test"]["totals"]["failed"]++;
      $data["test"]["platforms"][$platforms["platform"]] = "failed";
    }
    elseif($platforms["pending"] > 0) {
      $data["test"]["totals"]["pending"]++;
      $data["test"]["platforms"][$platforms["platform"]] = "pending";
    }
    elseif($platforms["passed"] > 0) {
      $data["test"]["totals"]["passed"]++;
      $data["test"]["platforms"][$platforms["platform"]] = "passed";
    }
  }
  mysql_free_result($result2);

  // --------------------------------
  // CROSS TESTS
  // --------------------------------
  $sql = "SELECT SUM(IF(Task.result = 0, 1, 0)) AS passed, ".
         "       SUM(IF(Task.result != 0, 1, 0)) AS failed, ".
         "       SUM(IF(Task.result IS NULL, 1, 0)) AS pending, ".
         "       Task.platform " .
         "  FROM Task, Run, Method_nmi ".
         " WHERE Method_nmi.input_runid = $runid AND ".
         "       Run.runid = Method_nmi.runid AND ".
         "       Run.user = '$user'  AND ".
         "       Task.runid = Run.runid AND ".
         "       Task.platform != 'local' AND ".
         "       project_version != component_version AND ".
         "	  component_version != 'native' ".
         " GROUP BY Task.platform ";
  $result2 = mysql_query($sql) or die ("Query $sql failed : " . mysql_error());
  $data["crosstest"] = Array();
  $data["crosstest"]["totals"] = Array();
  $data["crosstest"]["platforms"] = Array();
  while ($platforms = mysql_fetch_array($result2)) {
    $data["crosstest"]["totals"]["total"]++;
    if($platforms["failed"] > 0) {
      $data["crosstest"]["totals"]["failed"]++;
      $data["crosstest"]["platforms"][$platforms["platform"]] = "failed";
    }
    elseif($platforms["pending"] > 0) {
      $data["crosstest"]["totals"]["pending"]++;
      $data["crosstest"]["platforms"][$platforms["platform"]] = "pending";
    }
    elseif($platforms["passed"] > 0) {
      $data["crosstest"]["totals"]["passed"]++;
      $data["crosstest"]["platforms"][$platforms["platform"]] = "passed";
    }
  }
  mysql_free_result($result2);
     
  //
  // If there's nothing at all, just skip
  //
  // We only want to do this for one-off builds, if the nightly build
  // completely crapped out on us, we need to show it
  // Andy - 11/30/2006
  //
  if($user != CONDOR_USER) {
    if(!count($data["build"]["platforms"]) && !count($data["test"]["platforms"])) {
      return "";
    }
  }

  // If this run is pinned we want to display it
  $findpin="SELECT 
                   run_type, 
                   runid,
                   user,
                   archived,
                   archive_results_until
            FROM 
                   Run 
            WHERE 
                   runid = $runid ";
  
  $pincheck = mysql_query($findpin) or die ("Query $findpin failed : " . mysql_error());
  while ($pindetails = mysql_fetch_array($pincheck)) {
    $pin = $pindetails["archive_results_until"];
    $archived = $pindetails["archived"];
    if( !(is_null($pin))) {
      $info["pin"] = "pin $pin";
    }
    $info["runid"] = $runid;
    if( $archived == '0' ) {
      $info["runid"] .= "<br><font size='-1'> D </font>";
    }
  }
  
  foreach (Array("build", "test", "crosstest") AS $type) {
    $platforms = $data[$type]["platforms"];
    $totals = $data[$type]["totals"];
    
    // Form a status table
    $list = Array();
    $list["passed"] = Array();
    $list["pending"] = Array();
    $list["failed"] = Array();
    foreach ($platforms as $platform) {
      $list[$platforms[$platform]] .= $platform;
    }
    
    if($totals["failed"] > 0) {
      $status = "FAILED";
      $color = "FAILED";
    }
    elseif($totals["pending"] > 0) {
      $status = "PENDING";
      $color = "PENDING";
    }
    elseif($totals["passed"] > 0) {
      $status = "PASSED";
      $color = "PASSED";
    }
    else {
      $status = "No Results";
      $color = "NORESULTS";
    }
    
    //
    // Check for missing tests
    // Since we know how many builds have passed and should fire off tests,
    // we can do a simple check to make sure the total number of tests
    // is equal to the the number of builds
    // Andy - 01.09.2007
    //
    if ($type == "test") {
      $no_test_cnt = 0;
      if (count($no_test_platforms)) {
        $sql = "SELECT count(DISTINCT platform) AS count ".
               "  FROM Run, Task ".
               " WHERE Run.runid = $runid  AND ".
               "       Task.runid = Run.runid AND ".
               "       Task.platform IN ('".implode("','", $no_test_platforms)."') ";
        $cnt_result = mysql_query($sql) or die ("Query $sql failed : " . mysql_error());
        $res = mysql_fetch_array($cnt_result);
        $no_test_cnt = $res["count"];
      }
      $totals["missing"] = $data["build"]["totals"]["passed"] - $totals["total"] - $no_test_cnt;
      if ($totals["missing"] > 0) $color = "FAILED";
      elseif ($totals["missing"] < 0) $totals["missing"] = 0;
    }
    
    if($type == "crosstest") {
      $detail_url = sprintf(CROSS_DETAIL_URL, $runid, $type, $user);
    }
    else {
      $detail_url = sprintf(DETAIL_URL, $runid, $type, $user);
    }
    
    //
    // No results
    //
    if (!count($platforms)) {
      //
      // If this is a nightly build, we can check whether it failed and
      // give a failure notice. Without this, the box will just be empty
      // and people won't know what really happended
      //
      if (!empty($run_result) && $type == 'build') {
        $status = "FAILED";
        $html .= "<td class='$status' align='center'>\n";
        $html .= "  <table cellpadding='1' cellspacing='0' width='100%' class='details'>\n";
        $html .= "    <tr><td colspan='2' class='detail_url'><a href='$detail_url'>$status</a></td></tr>\n";
        $html .= "  </table>\n";
        $html .= "</td>\n";
      }
      elseif($type == "test") {
        $html .= "<td class='noresults' align='center'>\n";
        $html .= "  <table cellpadding='1' cellspacing='0' width='100%' class='details'>\n";
        $html .= "    <tr><td colspan='2' class='detail_url'><a href='$detail_url'>None</a></td></tr>\n";
        $html .= "    <tr><td colspan='2'>&nbsp;</td></tr>\n";
        $html .= "    <tr><td colspan='2'>&nbsp;</td></tr>\n";
        $html .= "    <tr><td colspan='2'>&nbsp;</td></tr>\n";
        $html .= "  </table>\n";
        $html .= "</td>\n";

      }
      else {
        $html .= "<td>&nbsp;</td>\n";
      }
      //
      // Show Summary
      //
    }
    else {
      $html .= "<td class='$color' align='center' valign='top'>\n";
      $html .= "  <table cellpadding='1' cellspacing='0' width='100%' class='details'>\n";
      $html .= "    <tr><td colspan='2' class='detail_url'><a href='$detail_url'>$status</a></td></tr>";

      //
      // Show the different status tallies for platforms
      //
      $result_types = Array( "passed", "pending", "failed", "missing" );
      foreach ($result_types AS $key) {
        if ($key == "missing" && empty($totals[$key])) continue;

        $name_display = ucfirst($key);
        $num_display = $totals[$key] ? $totals[$key] : 0;
        if ($key == "missing") {
          $name_display = "<b>$name_display</b>";
          $num_display = "<b>$num_display</b>";
        }
        
        $html .= "<tr>\n";
        $html .= "  <td width=\"75%\">$name_display</td>\n";
        $html .= "  <td width=\"25%\">$num_display</td>\n";
        $html .= "</tr>\n";
      }
      $html .= "</table></td>\n";
    } // RESULTS
  } // Foreach build/test/crosstest
  return $html;
}





function create_sparkline($branch, $user) {
  // We are going to build "sparklines" for the Continuous builds to make
  // it easier to visualize their performance over time
  $num_spark_days = NUM_SPARK_DAYS;
  
  // First get all the recent build runs.  We'll use the runids from the
  // build runs to determine which tests to match to them
  $sql = "SELECT runid,result,project_version, " . 
         "       convert_tz(start, 'GMT', 'US/Central') as start " .
         "FROM Run " .
         "WHERE run_type='build' AND " .
         "      component='condor' AND " .
         "      project='condor' AND " .
         "      DATE_SUB(CURDATE(), INTERVAL $num_spark_days DAY) <= start AND " .
         "      Description = '$branch' " .
         "      ORDER BY runid DESC";
  $result2 = mysql_query($sql) or die ("Query $sql failed : " . mysql_error());

  $spark = "<table><tr>\n";
  $spark .= "<td class='sparkheader'>Build:</td>\n";
  $builds = Array();
  while ($build = mysql_fetch_array($result2)) {
    $color = "passed";
    if($build["result"] == NULL) {
      $color = "pending";
    }
    elseif($build["result"] != 0) {
      $color = "failed";
    }
    
    $details = "  <table>";
    $details .= "    <tr><td>Status</td><td class=\"$color\">$color</td></tr>";
    $details .= "    <tr><td>NMI RunID</td><td>" . $build["runid"] . "</td></tr>";
    $details .= "    <tr><td>Submitted</td><td><nobr>" . $build["start"] . "</nobr></td></tr>";
    $details .= "    <tr><td>SHA1</td><td>" . substr($build["project_version"], 0, 15) . "</td></tr>";
    $details .= "  </table>";
    
    $hour = preg_replace("/^.+(\d\d):\d\d:\d\d.*$/", "$1", $build["start"]);
    
    $detail_url = sprintf(DETAIL_URL, $build["runid"], "build", $user);
    $popup_html = "<span class=\"link\"><a href=\"$detail_url\" style=\"text-decoration:none\">$hour<span>$details</span></a></span>";
    
    $tmp = Array();
    $tmp["runid"] = $build["runid"];
    $tmp["sha1"]  = substr($build["project_version"], 0, 15);
    $tmp["html"]  = $popup_html; 
    $tmp["color"] = $color;
    $tmp["hour"]  = $hour;
    array_push($builds, $tmp);
  }
  mysql_free_result($result2);
  
  for ($i=0; $i < count($builds); $i++) {
    $style = "";
    if($i != 0 && $builds[$i]["sha1"] != $builds[$i-1]["sha1"]) {
      $style .= " border-left-width:4px; border-left-color:black; ";
    }
    if($builds[$i+1] && $builds[$i]["sha1"] != $builds[$i+1]["sha1"]) {
      $style .= " border-right-width:4px; border-right-color:black; ";
    }
    $spark .= "<td class='" . $builds[$i]["color"] . "' style='$style'>";
    $spark .= $builds[$i]["html"] . "</td>\n";
  }
  
  // And now for the tests.  This is trickier because we have to line it up
  // with the builds above
  $platform = $branch;
  $platform = preg_replace("/Continuous Build - /", "", $branch);
  $sql = "SELECT runid,result,description," .
         "       convert_tz(start, 'GMT', 'US/Central') as start " .
         "FROM Run ".
         "WHERE run_type = 'TEST' AND " .
         "      component = 'condor' AND " .
         "      project = 'condor' AND " .
         "      DATE_SUB(CURDATE(), INTERVAL $num_spark_days DAY) <= start AND " .
         "      description LIKE 'Auto-Test Suite for ($platform, %' " .
         "ORDER BY description DESC";
  $result2 = mysql_query($sql) or die ("Query $sql failed : " . mysql_error());
  
  $tests = Array();
  while ($test = mysql_fetch_array($result2)) {
    $build_runid = preg_replace("/Auto-Test Suite for \($platform, (.+)\)/", "$1", $test["description"]);
    $runid = $test["runid"];
    $color = "passed";
    $hour = "&nbsp;&nbsp;&nbsp;";
    $failed_tests = "";
    if($test["result"] == NULL) {
      $color = "pending";
    }
    elseif($test["result"] != 0) {
      $color = "failed";
      
      $sql = "SELECT name
              FROM   Task
              WHERE  runid=$runid AND Result!=0";
      
      $result3 = mysql_query($sql) or die ("Query $sql failed : " . mysql_error());

      $hour = 0;
      $LIMIT = 5; // Cap the number of entries we show in the pop-up box
      $failed_parents = "";
      while ($task = mysql_fetch_array($result3)) {
        if($task["name"] == "platform_job" || $task["name"] == "remote_task") {
          $failed_parents .= "<nobr>" . $task["name"] . "</nobr><br>";
        }
        else {
          $hour += 1;
          if($hour <= $LIMIT) {
            $failed_tests .= "<nobr>" . $task["name"] . "</nobr><br>";
          }
        }
      }

      # Add a 
      if($hour > $LIMIT) {
        $failed_tests .= ($hour - $LIMIT) . " more...";
      }
      elseif($hour == 0) {
        # We only want to display the failed parents if there were no failed tests to display.
        $failed_tests = $failed_parents;
      }
    }
    
    $details = "<table>";
    $details .= "<tr><td>Status</td><td class=\"$color\">$color</td></tr>";
    $details .= "<tr><td>Start</td><td><nobr>" . $test["start"] . "</nobr></td></tr>";
    $details .= "<tr><td>SHA1</td><td>__PUT_SHA_HERE__</td></tr>";
    $details .= "<tr><td>Failed tests</td><td>$failed_tests</td></td></tr>";
    $details .= "</table>";
    
    $detail_url = sprintf(DETAIL_URL, $build_runid, "test", $user);
    $popup_html = "<span class=\"link\"><a href=\"$detail_url\" style=\"text-decoration:none\">$hour<span>$details</span></a></span>";
    
    $tests[$build_runid] = Array();
    $tests[$build_runid]["color"] = $color;
    $tests[$build_runid]["html"]  = $popup_html;
  }
  mysql_free_result($result2);
  
  $spark .= "<tr><td class='sparkheader'>Test:</td>\n";
  for ($i=0; $i < count($builds); $i++) {
    $build = $builds[$i];
    
    $style = "";
    if($i != 0 && $build["sha1"] != $builds[$i-1]["sha1"]) {
      $style .= "border-left-width:4px; border-left-color:black; ";
    }
    if($builds[$i+1] && $build["sha1"] != $builds[$i+1]["sha1"]) {
      $style .= "border-right-width:4px; border-right-color:black; ";
    }
    
    if($tests[$build["runid"]]) {
      $test = $tests[$build["runid"]];
      $tmp = preg_replace("/__PUT_SHA_HERE__/", $build["sha1"], $test["html"]);
      $tmp = preg_replace("/__PUT_HOUR_HERE__/", $build["hour"], $tmp);
      $spark .= "<td class='" . $test["color"] . "' style='$style;text-align:center'>$tmp</td>\n";
    }
    else {
      $spark .= "<td class='noresults' style='$style'>&nbsp;</td>\n";
    }
  }
  
  $spark .= "</tr></table>\n";
  
  return $spark;
}

// Define a custom sort that puts trunk ahead of other builds, and NMI after core builds
function cndrauto_sort($a, $b) {
  $a_is_nmi = preg_match("/^NMI/", $a);
  $b_is_nmi = preg_match("/^NMI/", $b);

  $a = preg_replace("/^NMI Ports - /", "", $a);
  $b = preg_replace("/^NMI Ports - /", "", $b);

  if($a == $b) {
    if($a_is_nmi and !$b_is_nmi) {
      return 1;
    }
    elseif($b_is_nmi and !$a_is_nmi) {
      return -1;
    }
  }

  if(preg_match("/trunk/", $a)) {
    return -1;
  }
  elseif(preg_match("/trunk/", $b)) {
    return 1;
  }

  return strcasecmp($b, $a);
}

?>
</body>
</html>
