<?php
//
// Configuration
//
define("NUM_SPARK_DAYS", 2);

$one_offs = array_key_exists("oneoffs", $_REQUEST) ? $_REQUEST["oneoffs"] : 0;
$SPARK_DAYS = array_key_exists("days", $_REQUEST) ? $_REQUEST["days"] : NUM_SPARK_DAYS;

include "dashboard.inc";

include "Dashboard.php";
$dash = new Dashboard();
$dash->print_header("Condor Build and Test Dashboard");
$dash->connect_to_db();

$continuous_blacklist = Array("x86_64_rhap_5.3-updated");
?>

</head>
<body>

<?php

$condor_user = $dash->get_condor_user();

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
  user='$condor_user'
GROUP BY 
  branch,
  user,
  run_type 
ORDER BY 
  runid desc
";

$results = $dash->db_query($query_runids);
$continuous_buf = Array();
foreach ($results as $row) {
  $runid      = $row["runid"];
  $branch     = $row["branch"];
  $user       = $row["user"];
  $run_result = $row["result"];

  if(preg_match("/Continuous/", $branch)) {
    $skip = 0;
    foreach ($continuous_blacklist as $blacklisted_platform) {
      if(preg_match("/ $blacklisted_platform$/", $branch)) {
        $skip = 1;
        break;
      }
    }
    if($skip == 1) {
      continue;
    }

    $continuous_buf[$branch] = Array();
    $continuous_buf[$branch]["user"] = $user;
    $continuous_buf[$branch]["results"] = create_sparkline($branch, $user, $SPARK_DAYS);
  }
  else {
    continue;
  }
}

// Create the sidebar
echo "<div id='wrap'>";

// Now create the HTML tables.
echo "<div id='main'>\n";

//
// 1) Continuous builds
//
if(!$one_offs) {  
  echo "<table>\n";

  $branches = array_keys($continuous_buf);
  sort($branches);

  $runs = Array();
  foreach ($branches as $branch) {
    $info = $continuous_buf[$branch];
    foreach (array_keys($info["results"]["build"]) as $key) {
      $runs[$key] = $info["results"]["build"][$key]["sha1"];
    }
  }
  krsort($runs);

  $table = Array();

  foreach ($branches as $branch) {
    $info = $continuous_buf[$branch];
    $branch_url = sprintf(BRANCH_URL, urlencode($branch), $info["user"]);
    $branch_display = preg_replace("/^Continuous Build - /", "", $branch);
    $out = "<tr style='height:8px;width:8px'>\n";

    $counter = 0;
    $commits = Array();
    $last_sha1 = "";
    foreach (array_keys($runs) as $key) {
      $counter += 1;
      $style = "";
      if($runs[$key] != $last_sha1 && $last_sha1 != "") {
        $style .= "border-left-width:4px; border-left-color:black; ";
        array_push($commits, $counter);
        $counter = 0;
      }
      $last_sha1 = $runs[$key];

      if(array_key_exists($key, $info["results"]["build"])) {
        $run = $info["results"]["build"][$key];
        $out .= "  <td onClick=\"document.location='" . $run["url"] .  "';\" class='" . $run["color"] . "' style='$style;height:8px;width:8px;'>";
        $out .= $run["html"] . "</td>\n";
      }
      else {
        $out .= "  <td style='$style;height:8px;width:8px;'></td>\n";
        //$out .= "  <td style='$style'>&nbsp;&nbsp;&nbsp;</td>\n";
      }
    }
    
    $out .= "</tr>\n";
    $out .="<tr>\n";

    $last_sha1 = "";
    foreach (array_keys($runs) as $key) {
      $style = "text-align:center; ";
      if($runs[$key] != $last_sha1 && $last_sha1 != "") {
        $style .= "border-left-width:4px; border-left-color:black; ";
      }
      $last_sha1 = $runs[$key];

      if(array_key_exists($key, $info["results"]["test"])) {
        $run = $info["results"]["test"][$key];
        $out .= "  <td class='" . $run["color"] . "' style='$style;height:8px;width:8px;'>";
        $out .= $run["html"] . "</td>\n";
      }
      else {
        $out .= "  <td style='$style;height:8px;width:8px;'></td>\n";
        //$out .= "  <td style='$style'>&nbsp;&nbsp;&nbsp;</td>\n";
      }
    }

    $out .= "</tr>\n";
    array_push($table, $out);
  }

  // We want to have a little spacing between the sparklines.  If we stack them one on top of
  // the other they look crowded.  But we want the spacing to be minimal, so we'll make it only
  // a few pixels high.  Also, it looks really nice to have the black commit lines continue
  // through the spacing.  It's a little bit more code to generate this "separator" row correctly.
  $sep = "<tr><td style='height:3px'></td>";
  $sum = 1;
  foreach ($commits as $len) {
    $sep .= "<td style='height:3px;' colspan='$len'></td>";
    $sep .= "<td style='height:3px; border-right-width:4px; border-right-color:black;' colspan='$len'></td>";
    $sum += $len;
  }
  $final_cell = sizeof($runs) - $sum;
  //$sep .= "<td style='height:3px;' colspan='$final_cell'></td>";
  $sep .= "</tr>\n";

  echo implode($sep, $table);
  echo "</table>\n";
}




// We are going to build "sparklines" for the Continuous builds to make
// it easier to visualize their performance over time
function create_sparkline($branch, $user, $num_spark_days) {
  // First get all the recent build runs.  We'll use the runids from the
  // build runs to determine which tests to match to them
  $sql = "SELECT runid,result,project_version, " . 
         "       convert_tz(start, 'GMT', 'US/Central') as start " .
         "FROM Run " .
         "WHERE run_type='build' AND " .
         "      component='condor' AND " .
         "      project='condor' AND " .
         "      DATE_SUB(CURDATE(), INTERVAL 1 DAY) <= start AND " .
         "      Description = '$branch' " .
         "      ORDER BY runid DESC LIMIT 10";
  $result2 = mysql_query($sql) or die ("Query $sql failed : " . mysql_error());

  $platform = preg_replace("/Continuous Build - /", "", $branch);

  $output = Array();
  $output["build"] = Array();
  $output["test"]  = Array();

  $builds = Array();
  $runids = Array();
  while ($build = mysql_fetch_array($result2)) {
    array_push($runids, $build["runid"]);

    $color = "passed";
    if($build["result"] == NULL) {
      $color = "pending";
    }
    elseif($build["result"] != 0) {
      $color = "failed";
    }
    
    /*
    $details = "  <table>";
    $details .= "    <tr><td>Status</td><td class=\"$color\">$color</td></tr>";
    $details .= "    <tr><td>Platform</td><td>$platform</td></tr>";
    $details .= "    <tr><td>NMI RunID</td><td>" . $build["runid"] . "</td></tr>";
    $details .= "    <tr><td>Submitted</td><td><nobr>" . $build["start"] . "</nobr></td></tr>";
    $details .= "    <tr><td>SHA1</td><td>" . substr($build["project_version"], 0, 15) . "</td></tr>";
    $details .= "    <tr><td>Host</td><td>__PUT_HOST_HERE__</td></tr>";
    $details .= "  </table>";
    */
    $details = $platform;
    
    $hour = preg_replace("/^.+(\d\d):\d\d:\d\d.*$/", "$1", $build["start"]);
    
    $detail_url = sprintf(DETAIL_URL, $build["runid"], "build", $user);
    //$popup_html = "<span class=\"link\"><a href=\"$detail_url\" style=\"text-decoration:none;size:2px;\">&nbsp;&nbsp;&nbsp;<span>$details</span></a></span>";
    $popup_html = "<span class=\"link\"><a href=\"$detail_url\" style=\"text-decoration:none;size:2px;\"><span>$details</span></a></span>";
    
    $tmp = Array();
    $tmp["runid"] = $build["runid"];
    $tmp["sha1"]  = substr($build["project_version"], 0, 15);
    $tmp["url"]   = $detail_url;
    $tmp["html"]  = $popup_html; 
    $tmp["color"] = $color;
    $tmp["hour"]  = $hour;
    $tmp["start"] = $build["start"];

    // The key is currently formed by using a combination of the date and the SHA1.  Since we run
    // builds every hour but the SHA1 might remain the same we need to use the date to provide
    // uniqueness.  We use the year/month/day and the hour, but we strip off the minute and second
    // because we want to be able to correlate runs across platforms (which will be submitted each
    // hour but at different minutes).  Eventually when we do per-commit builds this should
    // probably just become the SHA1 without the date.
    $tmp["key"] = preg_replace("/:.+$/", "", $build["start"]) . $tmp["sha1"];

    array_push($builds, $tmp);
  }
  mysql_free_result($result2);

  // Figure out the hostname for each execute node
  $hosts = get_hosts($runids);
  
  for ($i=0; $i < count($builds); $i++) {
    $key = $builds[$i]["key"];
    $output["build"][$key] = Array();
    $output["build"][$key]["sha1"]  = $builds[$i]["sha1"];
    $output["build"][$key]["color"] = $builds[$i]["color"];

    $tmp = preg_replace("/__PUT_HOST_HERE__/", $hosts[$builds[$i]["runid"]][$platform], $builds[$i]["html"]);
    $output["build"][$key]["html"] = $tmp;
  }
  
  // And now for the tests.  This is trickier because we have to line it up
  // with the builds above
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
  
  $runids = Array();
  $tests = Array();
  while ($test = mysql_fetch_array($result2)) {
    $build_runid = preg_replace("/Auto-Test Suite for \($platform, (.+)\)/", "$1", $test["description"]);
    $runid = $test["runid"];
    array_push($runids, $runid);
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

      # Add a max number of failures we'll show
      if($hour > $LIMIT) {
        $failed_tests .= ($hour - $LIMIT) . " more...";
      }
      elseif($hour == 0) {
        # We only want to display the failed parents if there were no failed tests to display.
        $failed_tests = $failed_parents;
      }
    }

    /*
    $details = "<table>";
    $details .= "<tr><td>Status</td><td class=\"$color\">$color</td></tr>";
    $details .= "<tr><td>Platform</td><td>$platform</td></tr>";
    $details .= "<tr><td>Start</td><td><nobr>" . $test["start"] . "</nobr></td></tr>";
    $details .= "<tr><td>SHA1</td><td>__PUT_SHA_HERE__</td></tr>";
    $details .= "<tr><td>Host</td><td>__PUT_HOST_HERE__</td></tr>";
    $details .= "<tr><td>Failed tests</td><td>$failed_tests</td></td></tr>";
    $details .= "</table>";
    */
    $details = $platform;
    
    $detail_url = sprintf(DETAIL_URL, $build_runid, "test", $user);
    $popup_html = "<span class=\"link\"><a href=\"$detail_url\" style=\"text-decoration:none\">$hour<span>$details</span></a></span>";
    
    $tests[$build_runid] = Array();
    $tests[$build_runid]["color"] = $color;
    $tests[$build_runid]["url"]   = $detail_url;
    $tests[$build_runid]["html"]  = $popup_html;
    $tests[$build_runid]["runid"] = $runid;
  }
  mysql_free_result($result2);

  $hosts = get_hosts($runids);
  
  for ($i=0; $i < count($builds); $i++) {
    $build = $builds[$i];
    
    $key = $builds[$i]["key"];
    $output["test"][$key] = Array();

    if(array_key_exists($build["runid"], $tests)) {
      $test = $tests[$build["runid"]];
      $tmp = preg_replace("/__PUT_SHA_HERE__/", $build["sha1"], $test["html"]);
      $tmp = preg_replace("/__PUT_HOUR_HERE__/", $build["hour"], $tmp);
      $tmp = preg_replace("/__PUT_HOST_HERE__/", $hosts[$test["runid"]][$platform], $tmp);
      $output["test"][$key]["color"] = $test["color"];
      $output["test"][$key]["html"]  = $tmp;
    }
    else {
      $output["test"][$key]["color"] = "noresults";
      $output["test"][$key]["html"]  = ""; //&nbsp;";
    }
  }
  
  return $output;
}

?>
</body>
</html>
