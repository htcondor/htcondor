<?php
$NUM_RUNS = array_key_exists("runs", $_REQUEST) ? $_REQUEST["runs"] : 25;
define("NUM_RUNS", $NUM_RUNS);

define("MAX_TASKS_TO_DISPLAY_IN_POPUP", 10);

include "dashboard.inc";

include "Dashboard.php";
$dash = new Dashboard();
$dash->print_header("Condor Build and Test Dashboard");
$dash->connect_to_db();

$blacklist = Array();
?>

</head>
<body>

<?php


$seen_platforms = Array();

/////////////////////////////////////////////
// Get list of runs
/////////////////////////////////////////////

$runs = get_runs($dash);

/////////////////////////////////////////////
// Get build info
/////////////////////////////////////////////

$results = get_run_info(array_keys($runs), $dash, "build");
  
foreach ($results as $row) {
  // Keep track of every platform that we see, and if it is in the blacklist we
  // will skip it here.
  $platform = preg_replace("/nmi:/", "", $row["platform"]);
  if(in_array($platform, $blacklist)) {
    continue;
  }
  
  // Keep track of what platform we have seen and the total records for each.
  if(array_key_exists($platform, $seen_platforms)) {
    $seen_platforms[$platform] += 1;
  }
  else {
    $seen_platforms[$platform] = 1;
  }

  // PHP does not have autovivification so we need to make the array for each platform
  if(!array_key_exists($platform, $runs[$row["runid"]]["platforms"])) {
    $runs[$row["runid"]]["platforms"][$platform] = Array();
    $runs[$row["runid"]]["platforms"][$platform]["build"] = Array();
    $runs[$row["runid"]]["platforms"][$platform]["test"] = Array();
    $runs[$row["runid"]]["platforms"][$platform]["build"]["bad-tasks"] = Array();
    $runs[$row["runid"]]["platforms"][$platform]["test"]["bad-tasks"] = Array();
  }

  if($row["name"] == "platform_job") {
    // The platform_job task will contain the final result, as well as the length of
    // execution for the entire job.
    $runs[$row["runid"]]["platforms"][$platform]["build"]["result"] = $row["result"];
    $runs[$row["runid"]]["platforms"][$platform]["build"]["duration"] = $row["duration"];
    $runs[$row["runid"]]["platforms"][$platform]["build"]["runid"] = $row["runid"];
  }
  elseif($row["name"] == "remote_pre") {
    // For the remote_pre step, we determine the execution host
    $runs[$row["runid"]]["platforms"][$platform]["build"]["host"] = $row["host"];
  }
  else {
    array_push($runs[$row["runid"]]["platforms"][$platform]["build"]["bad-tasks"], $row["name"]);
  }
}

/////////////////////////////////////////////
// Get test info
/////////////////////////////////////////////

// First, get the list of test run IDs.  For FW builds each platform gets its
// own test ID even if all the platforms are built in one run ID.
$runids = implode(", ", array_keys($runs));
$query = "
SELECT
  gjl_input_from_nmi.run_id AS build_runid, 
  gjl_input.run_id as test_runid
FROM 
  gjl_input 
LEFT OUTER JOIN 
  gjl_input_from_nmi ON gjl_input.id = input_id 
WHERE 
  gjl_input_from_nmi.run_id IN ($runids)";

$results = $dash->db_query($query);

$test_mapping = Array();
foreach ($results as $row) {
  $test_mapping[$row["test_runid"]] = $row["build_runid"];
}

$results = get_run_info(array_keys($test_mapping), $dash, "test");
foreach ($results as $row) {
  // Keep track of every platform that we see, and if it is in the blacklist we
  // will skip it here.
  $platform = preg_replace("/nmi:/", "", $row["platform"]);
  if(in_array($platform, $blacklist)) {
    continue;
  }

  $build_runid = $test_mapping[$row["runid"]];
  
  // PHP does not have autovivification so we need to make the array for each platform
  if(!array_key_exists($platform, $runs[$build_runid]["platforms"])) {
    print "WARNING: tests exist for a run that builds do not exist for.<br>\n";
    print " Test run ID = " . $row["runid"] . "<br>\n";
    print " Build run = $build_runid<br>\n";
    print " Platform = $platform <br>\n";
  }

  if($row["name"] == "platform_job") {
    // The platform_job task will contain the final result, as well as the length of
    // execution for the entire job.
    $runs[$build_runid]["platforms"][$platform]["test"]["result"] = $row["result"];
    $runs[$build_runid]["platforms"][$platform]["test"]["duration"] = $row["duration"];
    $runs[$build_runid]["platforms"][$platform]["test"]["runid"] = $row["runid"];
  }
  elseif($row["name"] == "remote_pre") {
    // For the remote_pre step, we determine the execution host
    $runs[$build_runid]["platforms"][$platform]["test"]["host"] = $row["host"];
  }
  else {
    array_push($runs[$build_runid]["platforms"][$platform]["test"]["bad-tasks"], $row["name"]);
  }
}

/////////////////////////////////////////////
// Get Git log info
/////////////////////////////////////////////
$last_run = end($runs);
$hash1 = $last_run["sha1"];
$first_run = reset($runs);
$hash2 = $first_run["sha1"];
$commit_info = get_git_log($hash1, $hash2);


/////////////////////////////////////////////
// Display the info
/////////////////////////////////////////////

print "<div id='main'>\n";

print "<form method='get' action='" . $_SERVER["PHP_SELF"] . "'>\n";
print "<p>Commits:&nbsp;<select name='runs'>\n";
print "<option selected='selected'>25</option>\n";
print "<option>50</option>\n";
print "<option>100</option>\n";
print "<option>200</option>\n";
print "</select><input type='submit' value='Show'></form>\n";

print "<p style='font-size:-1'>The following platforms are excluded from displaying here: " . implode(", ", $blacklist) . "</p>\n";

// Create the table header
print "<table>\n";
print "<tr>\n";
print "  <th>D</td>\n";
print "  <th>SHA1</th>\n";

// Get the queue lengths
$condorq = new CondorQ($pool_platforms);
foreach (array_keys($seen_platforms) AS $platform) {
  $condorq->add_platform($platform);
}
$queues = $condorq->condor_q();

foreach (array_keys($seen_platforms) as $platform) {
  // Update this if NMI has any other architecture prefixes (such as ia64) 
  if(preg_match("/^x86_64_/", $platform)) {
    $display = preg_replace("/x86_64_/", "x86_64<br>", $platform);
  }
  elseif(preg_match("/ia64_/", $platform)) {
    $display = preg_replace("/ia64_/", "x86<br>", $platform);
  }
  else {
    $display = preg_replace("/x86_/", "x86<br>", $platform);
  }

  print "  <th colspan=2><font size='-3'>$display" . $queues[$platform]["html-queue"] . "</font></th>\n";
  print "  <th></th>\n";
}
print "  <th><font size='-3'># Bad<br>Tasks</font></th>\n";
print "</tr>\n";

// Determine the heights of the days-of-the-week that display on the left
$day_heights = Array();
$last_date = "";
$count = 0;
foreach (array_keys($runs) as $run) {
  $date = preg_replace("/^\d\d\d\d-(\d\d-\d\d).*/", "$1", $runs[$run]["start"]);
  if($last_date == "") {
    // Always mark the first day
    $runs[$run]["day-break"] = 1;
  }
  elseif($date != $last_date) {
    $runs[$run]["day-break"] = 1;
    array_push($day_heights, $count);
    $count = 0;
  }
  $count++;
  $last_date = $date;
}
array_push($day_heights, $count);

// Create the table body.  One row for each SHA1
foreach ($runs as $run) {
  print "<tr>\n";

  if(array_key_exists("day-break", $run)) {
    $td_class = "daybreak";
    $rowspan = array_shift($day_heights);

    $dayofweek = $run["dayofweek"];
    if($rowspan < 4) {
      $dayofweek = substr($dayofweek, 0, 3);
    }
    $dayofweek = implode("<br>", str_split($dayofweek, 1));
    print "  <td class=\"$td_class\" rowspan=$rowspan>$dayofweek</td>\n";
  }
  else {
    $td_class = "";
  }

  print "  <td class=\"$td_class\">\n";

  $tmp = substr($run["sha1"], 0, 15) . "<br><font size=\"-2\">" . $run["start"] . "</font>\n";
  $link = sprintf(GITSHA1, $run["sha1"]);  
  print "    <span class=\"link\"><a href=\"$link\" style=\"text-decoration:none;\">$tmp<span style=\"width:300px\">" . $commit_info[$run["sha1"]] . "</span></a></span>";
  print "  </td>\n";

  // Keep track of a summary of the platforms
  $summary = Array();
  $summary["build"] = Array();
  $summary["build"]["passed"] = 0;
  $summary["build"]["pending"] = 0;
  $summary["build"]["failed"] = 0;
  $summary["test"] = Array();
  $summary["test"]["passed"] = 0;
  $summary["test"]["pending"] = 0;
  $summary["test"]["failed"] = 0;

  // Now print the results for each platform
  foreach (array_keys($seen_platforms) as $platform) {
    // There is no guarantee that each platform is in each run.  So check it here
    if(array_key_exists($platform, $run["platforms"])) {

      if($run["platforms"][$platform]["build"]["result"] == NULL) {
	$summary["build"]["pending"] += 1;
      }
      elseif($run["platforms"][$platform]["build"]["result"] != 0) {
	$summary["build"]["failed"] += 1;
      }
      else {
	$summary["build"]["passed"] += 1;
	if($run["platforms"][$platform]["test"]["result"] == NULL) {
	  $summary["test"]["pending"] += 1;
	}
	elseif($run["platforms"][$platform]["test"]["result"] == 0) {
	  $summary["test"]["passed"] += 1;
	}
	else {
	  $summary["test"]["failed"] += 1;
	}
      }


      print make_cell($run, $platform, "build", $td_class);

      if($run["platforms"][$platform]["build"]["result"] != NULL and 
	 $run["platforms"][$platform]["build"]["result"] == 0) {
	print make_cell($run, $platform, "test", $td_class);
      }
      else {
	print " <td class=\"noresults test $td_class\">&nbsp;&nbsp;&nbsp;</td>";
      }
    }    
    else {
      print "  <td class='build $td_class'>&nbsp;</td><td class='test $td_class'>&nbsp;</td>\n";
    }

    print "  <td class='$td_class' style=\"width:10px; font-size:5px;\">&nbsp;</td>\n";
  }

  // Print the summary
  $num = $summary["build"]["pending"] + $summary["build"]["failed"] + $summary["test"]["pending"] + $summary["test"]["failed"];
  if($num == 0) {
    print "<td class='passed $td_class'>$num</td>";
  }
  else {
    print "<td class='$td_class'>$num</td>";
  }

  print "</tr>\n";
}

print "</table>\n";

function get_git_log($hash1, $hash2) {
  $output = `git --git-dir=/home/condorauto/condor.git log --pretty=format:'%H | %an | %s' $hash1..$hash2 2>&1`;
  $commits = explode("\n", $output);

  $commit_info = Array();
  foreach ($commits as $commit) {
    $array = explode(" | ", $commit);
    $commit_info[$array[0]] = "$array[1]<br>$array[2]\n";
  }

  return $commit_info;
}

print "</div>\n";
print "<div id='footer'>&nbsp;</div>\n";
print "</div>\n";

print "</body>\n";
print "</html>\n";


function get_runs($dash) {
  // Each result from this query is one column in the sparklines.
  // We will use this query to get the Run IDs and the order to display the SHAs
  $condor_user = $dash->get_condor_user();
  $query = "
SELECT 
  runid,
  project_version as sha1,
  host,
  result,
  CONVERT_TZ(start, 'GMT', 'US/Central') AS start,
  DAYOFWEEK(CONVERT_TZ(start, 'GMT', 'US/Central')) as dayofweek
FROM 
  Run 
WHERE 
  component='condor' AND 
  project='condor' AND
  run_type='build' AND
  description LIKE 'Continuous Build' AND 
  user = '$condor_user'
ORDER BY 
  runid desc
LIMIT " . NUM_RUNS;


  $results = $dash->db_query($query);
  $runs = Array();
  foreach ($results as $row) {
    // The unique ID for each column will be the NMI Run ID.
    // This will let us sort the runs by time.
    $id = $row["runid"];
    
    $runs[$id] = Array();
    $runs[$id]["runid"]     = $row["runid"];  // Duplicate info, but it will be useful later
    $runs[$id]["start"]     = $row["start"];
    $runs[$id]["sha1"]      = $row["sha1"];
    $runs[$id]["host"]      = $row["host"];
    $runs[$id]["result"]    = $row["result"];
    $runs[$id]["dayofweek"] = day_of_week($row["dayofweek"]);
    $runs[$id]["platforms"] = Array();
  }

  // Bail out in case there are no matching runs
  if(count($runs) == 0) {
    print "<p>No runs found.\n";
    exit;
  }

  return $runs;
}


function get_run_info($run_ids, $dash, $type) {
  $extra_conditional = "";
  if($type == "test") {
    $extra_conditional = "platform != 'local' AND";
  }
  // Now we run a second query.  This time we will get the result per platform for each run.
  // Additionally, we will gather info on where the jobs ran, etc.
  $runids = implode(", ", $run_ids);
  $query = "
SELECT 
  runid,
  platform,
  result,
  host,
  TIMEDIFF(finish,start) as duration,
  name 
FROM 
  Task
WHERE
  runid in ($runids) AND
  $extra_conditional
  (name in (\"platform_job\", \"remote_pre\") OR result != 0)
";

  return $dash->db_query($query);
}


function make_cell($run, $platform, $run_type, $td_class) {

  $color = "passed";
  if($run["platforms"][$platform][$run_type]["result"] == NULL) {
    $color = "pending";
  }
  elseif($run["platforms"][$platform][$run_type]["result"] != 0) {
    $color = "failed";
  }

  $details = "  <table>";
  $details .= "    <tr><td class='left'>Status</td><td class=\"$color left\">$color</td></tr>";
  $details .= "    <tr><td class='left'><nobr>NMI RunID</nobr></td><td class='left'>" . $run["platforms"][$platform][$run_type]["runid"] . "</td></tr>";
  $details .= "    <tr><td class='left'>Submitted</td><td class='left'><nobr>" . $run["start"] . "</nobr></td></tr>";
  $details .= "    <tr><td class='left'>Host</td><td class='left'>" . $run["platforms"][$platform][$run_type]["host"] . "</td></tr>";

  if($color != "pending") {
    $details .= "    <tr><td class='left'>Duration</td><td class='left'><nobr>" . $run["platforms"][$platform][$run_type]["duration"] . "</nobr></td></tr>";
  }

  if(count($run["platforms"][$platform][$run_type]["bad-tasks"]) == 0) {
    $failed_tasks = "&lt;None&gt;";
  }
  elseif(count($run["platforms"][$platform][$run_type]["bad-tasks"]) <= MAX_TASKS_TO_DISPLAY_IN_POPUP) {
    $failed_tasks = "<nobr>" . implode("</nobr><br><nobr>", $run["platforms"][$platform][$run_type]["bad-tasks"]) . "</nobr>";
  }
  else {
    $failed_tasks = "<nobr>" . implode("</nobr><br><nobr>", array_slice($run["platforms"][$platform][$run_type]["bad-tasks"], 0, MAX_TASKS_TO_DISPLAY_IN_POPUP-1)) . "</nobr>";
    $hidden = count($run["platforms"][$platform][$run_type]["bad-tasks"]) - MAX_TASKS_TO_DISPLAY_IN_POPUP;
    $failed_tasks .= "<br><i>$hidden more...</i>";
  }
  $details .= "    <tr><td class='left'><nobr>Failed tasks</nobr></td><td class='left'>$failed_tasks</td></tr>";
  //$details .= "    <tr><td>Submission Host</td><td>" . $run["host"] . "</td></tr>";

  $details .= "  </table>";

  $detail_url = sprintf(DETAIL_URL, $run["runid"]);

  if(count($run["platforms"][$platform][$run_type]["bad-tasks"]) == 0) {
    $div = "&nbsp;&nbsp;&nbsp;";
  }
  else {
    $div = count($run["platforms"][$platform][$run_type]["bad-tasks"]);
  }

  $popup_html = "  <td class=\"$color $run_type $td_class\"><span class=\"link\"><a href=\"$detail_url\" style=\"text-decoration:none\">$div<span>$details</span></a></span></td>";

  return $popup_html;
}


// We are going to build "sparklines" for the Continuous builds to make
// it easier to visualize their performance over time
function create_sparkline($dash, $branch, $num_spark_days) {

  ///////////////////////////////////////////////////////////
  // Now find the matching tests
  ///////////////////////////////////////////////////////////

  // Query the DB for all tests that used the above builds as an input
  if($runids) {

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

?>