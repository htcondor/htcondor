<?php
//
// Configuration
//
define("NUM_SPARK_DAYS", 2);

$one_offs = array_key_exists("oneoffs", $_REQUEST) ? $_REQUEST["oneoffs"] : 0;
$SPARK_DAYS = array_key_exists("days", $_REQUEST) ? $_REQUEST["days"] : NUM_SPARK_DAYS;

require_once "./load_config.inc";
load_config();
include "dashboard.inc";

$continuous_blacklist = Array("x86_64_rhap_5.3-updated");
?>
<html>
<head>
<Title>NMI - Condor Latest Build/Test Results</TITLE>
<LINK REL="StyleSheet" HREF="condor.css" TYPE="text/css">
</HEAD>
<body>

<?php
$db = connect_to_mysql();

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
  
  echo "<p style='font-size:-1'>Continuous blacklist: " . implode(", ", $continuous_blacklist) . "</p>";

  echo "<form method='get' action='" . $_SERVER{PHP_SELF} . "'>\n";
  echo "<p>Days:&nbsp;<select name='days'>\n";
  echo "<option selected='selected'>2</option>\n";
  echo "<option>3</option>\n";
  echo "<option>4</option>\n";
  echo "<option>5</option>\n";
  echo "</select><input type='submit' value='Show'></form><br>\n";

  $help_text = "<ul>\n";
  $help_text .= "  <li>The last $SPARK_DAYS days of results are shown for each platform.</li>\n";
  $help_text .= "  <li>The newest results are at the left.</li>\n";
  $help_text .= "  <li>Thick black bars designate commits to the repository between runs.</li>\n";
  $help_text .= "  <li>The number shown in the build line is the hour in which the test ran.</li>\n";
  $help_text .= "  <li>If a number is shown in the test line it is the number of tests that failed.</li>\n";
  $help_text .= "</ul>";
  echo "<span class=\"link\" style=\"font-size:100%\"><a href=\"javascript: void(0)\" style=\"text-decoration:none\">Hover here to have this section explained<span style=\"width:450px;\">$help_text</span></a></span>";

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
    $out = "<tr>\n";
    $out .= "  <td rowspan='2'><a href='$branch_url'>$branch_display</a></td>\n";
    $out .= "  <td class='sparkheader'>Build</td>\n";

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
        $out .= "  <td class='" . $run["color"] . "' style='$style'>";
        $out .= $run["html"] . "</td>\n";
      }
      else {
        $out .= "  <td style='$style'>&nbsp;&nbsp;&nbsp;</td>\n";
      }
    }
    
    $out .= "</tr>\n";
    $out .="<tr>\n";
    $out .= "  <td class='sparkheader'>Test</td>\n";

    $last_sha1 = "";
    foreach (array_keys($runs) as $key) {
      $style = "text-align:center; ";
      if($runs[$key] != $last_sha1 && $last_sha1 != "") {
        $style .= "border-left-width:4px; border-left-color:black; ";
      }
      $last_sha1 = $runs[$key];

      if(array_key_exists($key, $info["results"]["test"])) {
        $run = $info["results"]["test"][$key];
        $out .= "  <td class='" . $run["color"] . "' style='$style'>";
        $out .= $run["html"] . "</td>\n";
      }
      else {
        $out .= "  <td style='$style'>&nbsp;&nbsp;&nbsp;</td>\n";
      }
    }

    $out .= "</tr>\n";
    array_push($table, $out);
  }

  // We want to have a little spacing between the sparklines.  If we stack them one on top of
  // the other they look crowded.  But we want the spacing to be minimal, so we'll make it only
  // a few pixels high.  Also, it looks really nice to have the black commit lines continue
  // through the spacing.  It's a little bit more code to generate this "separator" row correctly.
  $sep = "<tr><td style='height:6px'></td>";
  $sum = 1;
  foreach ($commits as $len) {
    $sep .= "<td style='height:6px; border-right-width:4px; border-right-color:black;' colspan='$len'></td>";
    $sum += $len;
  }
  $final_cell = sizeof($runs) + 2 - $sum;
  $sep .= "<td style='height:6px;' colspan='$final_cell'></td>";
  $sep .= "</tr>\n";

  echo implode($sep, $table);
  echo "</table>\n";
}



//
// 2) cndrauto builds
//
if(!$one_offs) {
  echo "<h2>Auto builds - user '" . CONDOR_USER . "'</h2>\n";
  echo "<table border=1>\n";
  echo "<tr><th>Branch</th><th>Build RunID</th><th>Submitted</th><th>Build Results</th><th>Test Results</th><th>Cross Test Results</th>\n";
  echo "</tr>\n";
  
  $branches = array_keys($cndrauto_buf);
  usort($branches, "cndrauto_sort");

  foreach ($branches as $branch) {
    $info = $cndrauto_buf[$branch];
    $branch_url = sprintf(BRANCH_URL, urlencode($branch), $info["user"]);

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
    $branch_url = sprintf(BRANCH_URL, urlencode($info["branch"]), $info["user"]);
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
         "      DATE_SUB(CURDATE(), INTERVAL $num_spark_days DAY) <= start AND " .
         "      Description = '$branch' " .
         "      ORDER BY runid DESC";
  $result2 = mysql_query($sql) or die ("Query $sql failed : " . mysql_error());

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
    
    $details = "  <table>";
    $details .= "    <tr><td>Status</td><td class=\"$color\">$color</td></tr>";
    $details .= "    <tr><td>NMI RunID</td><td>" . $build["runid"] . "</td></tr>";
    $details .= "    <tr><td>Submitted</td><td><nobr>" . $build["start"] . "</nobr></td></tr>";
    $details .= "    <tr><td>SHA1</td><td>" . substr($build["project_version"], 0, 15) . "</td></tr>";
    $details .= "    <tr><td>Host</td><td>__PUT_HOST_HERE__</td></tr>";
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
  $platform = preg_replace("/Continuous Build - /", "", $branch);
  
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
    
    $details = "<table>";
    $details .= "<tr><td>Status</td><td class=\"$color\">$color</td></tr>";
    $details .= "<tr><td>Start</td><td><nobr>" . $test["start"] . "</nobr></td></tr>";
    $details .= "<tr><td>SHA1</td><td>__PUT_SHA_HERE__</td></tr>";
    $details .= "<tr><td>Host</td><td>__PUT_HOST_HERE__</td></tr>";
    $details .= "<tr><td>Failed tests</td><td>$failed_tests</td></td></tr>";
    $details .= "</table>";
    
    $detail_url = sprintf(DETAIL_URL, $build_runid, "test", $user);
    $popup_html = "<span class=\"link\"><a href=\"$detail_url\" style=\"text-decoration:none\">$hour<span>$details</span></a></span>";
    
    $tests[$build_runid] = Array();
    $tests[$build_runid]["color"] = $color;
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
      $output["test"][$key]["html"]  = "&nbsp;";
    }
  }
  
  return $output;
}

// Define a custom sort that puts trunk ahead of other builds, and NMI immediately
// after the matching core build.
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
