<?php
define("NUM_SPARK_DAYS", 2);

$SPARK_DAYS = array_key_exists("days", $_REQUEST) ? $_REQUEST["days"] : NUM_SPARK_DAYS;

include "dashboard.inc";

include "Dashboard.php";
$dash = new Dashboard();
$dash->print_header("Condor Build and Test Dashboard");
$dash->connect_to_db();

$continuous_blacklist = Array("Fedora", "x86_64_fedora_13");
?>

<script type="text/javascript">
$(document).ready(function(){
  $("#help_link").click(function(){
    $("#help_box").slideToggle("slow");
  });

  $("#help_box").click(function(){
    $("#help_box").slideToggle("slow");
  });
});
</script>


</head>
<body>

<?php

$condor_user = $dash->get_condor_user();
define("CONDOR_USER", $condor_user);

$query = "
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
  user = '$condor_user'
GROUP BY 
  branch,
  user,
  run_type 
ORDER BY 
  runid desc
";

$results = $dash->db_query($query);
$continuous_buf = Array();
foreach ($results as $row) {
  $runid      = $row["runid"];
  $branch     = $row["branch"];
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
    $continuous_buf[$branch]["results"] = create_sparkline($dash, $branch, $SPARK_DAYS);
  }
}

// Now create the HTML tables.
echo "<div id='main'>\n";

//
// 1) Continuous builds
//

/*
echo "<form method='get' action='" . $_SERVER{"SCRIPT_NAME"} . "'>\n";
echo "<p>Days:&nbsp;<select name='days'>\n";
echo "<option selected='selected'>2</option>\n";
echo "<option>3</option>\n";
echo "<option>4</option>\n";
echo "<option>5</option>\n";
echo "</select><input type='submit' value='Show'></form><br>\n";
*/

$help_text = "<ul>\n";
$help_text .= "  <li>The last $SPARK_DAYS days of results are shown for each platform.</li>\n";
$help_text .= "  <li>The newest results are at the left.</li>\n";
$help_text .= "  <li>Thick black bars designate commits to the repository between runs.</li>\n";
$help_text .= "  <li>The number shown in the build line is the hour in which the test ran.</li>\n";
$help_text .= "  <li>If a number is shown in the test line it is the number of tests that failed.</li>\n";
$help_text .= "</ul>";
echo "<div id='help_link' class='help_link'>Click here to have the sparklines explained</div>";
echo "<div id='help_box' class='help_box'>$help_text</div>";

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
  $branch_url = sprintf(BRANCH_URL, $branch, CONDOR_USER);
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

echo "<p style='font-size:-1'>The following platforms are excluded from displaying here: " . implode(", ", $continuous_blacklist) . "</p>";





echo "</div>\n";
echo "<div id='footer'>&nbsp;</div>\n";
echo "</div>\n";






// We are going to build "sparklines" for the Continuous builds to make
// it easier to visualize their performance over time
function create_sparkline($dash, $branch, $num_spark_days) {
  // $output is a buffer for the information/HTML this function creates
  $output = Array();

  // Unfortunately, the only way we know the NMI platform is by looking at the branch name
  $platform = preg_replace("/Continuous Build - /", "", $branch);

  // First get all the recent build runs.  We'll use the runids from the
  // build runs to determine which tests to match to them

  $condor_user = CONDOR_USER;

  $sql = "SELECT runid, result, project_version, 
                 convert_tz(start, 'GMT', 'US/Central') as start
          FROM Run
          WHERE run_type='build' AND
                component='condor' AND
                project='condor' AND
                DATE_SUB(CURDATE(), INTERVAL $num_spark_days DAY) <= start AND
                Description = '$branch' AND
                user = '$condor_user'
                ORDER BY runid DESC";


  $results = $dash->db_query($sql);

  $builds = Array();
  $runids = Array();
  foreach ($results as $build) {
    array_push($runids, $build["runid"]);
    array_push($builds, create_build_sparkline_box($build));
  }

  // Figure out the hostname for each execute node
  $hosts = get_hosts($dash, $runids);
  $output["build"] = create_build_output($builds, $hosts, $platform);


  ///////////////////////////////////////////////////////////
  // Now find the matching tests
  ///////////////////////////////////////////////////////////

  // Query the DB for all tests that used the above builds as an input
  if($runids) {
    $runids_list = implode(", ", $runids);
    $sql = "SELECT
                   gjl_input_from_nmi.run_id AS build_run_id, 
                   gjl_input.run_id as test_run_id
            FROM gjl_input 
            LEFT OUTER JOIN gjl_input_from_nmi ON gjl_input.id = input_id 
            WHERE gjl_input_from_nmi.run_id IN ($runids_list)";

    $results = $dash->db_query($sql);
  }
  else {
    $results = Array();
  }

  $runids = Array();
  $tests = Array();
  foreach ($results as $pair) {
    $build_runid = $pair["build_run_id"];
    $test_runid = $pair["test_run_id"];

    array_push($runids, $pair["test_run_id"]);
    $tests[$build_runid] = create_test_sparkline_box($dash, $test_runid, $build_runid);
  }

  $hosts = get_hosts($dash, $runids);  
  $output["test"] = create_test_output($tests, $builds, $hosts, $platform);
  
  return $output;
}

function create_build_sparkline_box($build) {

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
  $hour = "&nbsp&nbsp&nbsp";

  $detail_url = sprintf(DETAIL_URL, $build["runid"], "build", CONDOR_USER);
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

  return $tmp;
}

function create_build_output($builds, $hosts, $platform) {

  $buffer = Array();
  for ($i=0; $i < count($builds); $i++) {
    $key = $builds[$i]["key"];
    $buffer[$key] = Array();
    $buffer[$key]["sha1"]  = $builds[$i]["sha1"];
    $buffer[$key]["color"] = $builds[$i]["color"];

    $tmp = preg_replace("/__PUT_HOST_HERE__/", $hosts[$builds[$i]["runid"]][$platform], $builds[$i]["html"]);
    $buffer[$key]["html"] = $tmp;
  }

  return $buffer;
}


function create_test_sparkline_box($dash, $test_runid, $build_runid) {

  $sql = "SELECT
                result, start_date
          FROM 
                gjl_run
          WHERE
                id=$test_runid";
  $results=$dash->db_query($sql);

  $result = $results[0]["result"];
  $start = $results[0]["start_date"];

  $color = "passed";
  $hour = "&nbsp;&nbsp;&nbsp;";
  $failed_tests = "";
  if($result == NULL) {
    $color = "pending";
  }
  elseif($result != 0) {
    $color = "failed";
      
    $sql = "SELECT name
            FROM   Task
            WHERE  runid=$test_runid AND Result!=0";
      
    $results = $dash->db_query($sql);

    $hour = 0;
    $LIMIT = 5; // Cap the number of entries we show in the pop-up box
    $failed_parents = "";
    foreach ($results as $task) {
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
    
    // Add a max number of failures we'll show
    if($hour > $LIMIT) {
      $failed_tests .= ($hour - $LIMIT) . " more...";
    }
    elseif($hour == 0) {
      // We only want to display the failed parents if there were no failed tests to display.
      $failed_tests = $failed_parents;
    }
  }
    
  $details = "<table>";
  $details .= "<tr><td>Status</td><td class=\"$color\">$color</td></tr>";
  $details .= "<tr><td>Start</td><td><nobr>$start</nobr></td></tr>";
  $details .= "<tr><td>SHA1</td><td>__PUT_SHA_HERE__</td></tr>";
  $details .= "<tr><td>Host</td><td>__PUT_HOST_HERE__</td></tr>";
  $details .= "<tr><td>Failed tests</td><td>$failed_tests</td></td></tr>";
  $details .= "</table>";
    
  $detail_url = sprintf(DETAIL_URL, $build_runid, "test", CONDOR_USER);
  $popup_html = "<span class=\"link\"><a href=\"$detail_url\" style=\"text-decoration:none\">$hour<span>$details</span></a></span>";

  $tmp = Array();
  $tmp["color"] = $color;
  $tmp["html"]  = $popup_html;
  $tmp["runid"] = $runid;

  return $tmp;
}

function create_test_output($tests, $builds, $hosts, $platform) {

  $buffer = Array();

  for ($i=0; $i < count($builds); $i++) {
    $build = $builds[$i];
    
    $key = $builds[$i]["key"];
    $buffer[$key] = Array();

    if(array_key_exists($build["runid"], $tests)) {
      $test = $tests[$build["runid"]];
      $tmp = preg_replace("/__PUT_SHA_HERE__/", $build["sha1"], $test["html"]);
      $tmp = preg_replace("/__PUT_HOST_HERE__/", $hosts[$test["runid"]][$platform], $tmp);
      $buffer[$key]["color"] = $test["color"];
      $buffer[$key]["html"]  = $tmp;
    }
    else {
      $buffer[$key]["color"] = "noresults";
      $buffer[$key]["html"]  = "&nbsp;";
    }
  }

  return $buffer;
}

?>
</body>
</html>
