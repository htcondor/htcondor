<?php

define("AUTO_URL", "./Run-condor.php?oneoffs=0");
define("ONEOFFS_URL", "./Run-condor.php?oneoffs=1");
define("QUEUE_DEPTH_URL", "./Queue-depth-nmi.php");

define("CONDOR_USER", "cndrauto");
define("BRANCH_URL", "./Run-condor-branch.php?branch=%s&user=%s");
define("DETAIL_URL", "./Run-condor-details.php?runid=%s&type=%s&user=%s");
define("CROSS_DETAIL_URL", "./Run-condor-pre-details.php?runid=%s&type=%s&user=%s");

define("GITSHA1","http://condor-git.cs.wisc.edu/?p=condor.git;a=commit;h=%s");
define("GITSHA1SHORT","http://condor-git.cs.wisc.edu/?p=condor.git;a=log;h=%s;hp=%s");

// Connect to MySQL DB
function connect_to_mysql() {
  $db = mysql_connect(WEB_DB_HOST, DB_READER_USER, DB_READER_PASS) or die ("Could not connect : " . mysql_error());
  mysql_select_db(DB_NAME) or die("Could not select database");
  return $db;
}


// Given a set of runids, return a hash of the platform/host combinations of where the jobs executed
function get_hosts($runids) {
  $hosts = Array();
  $sql = "SELECT runid, platform, host
          FROM Task
          WHERE runid in (" . implode(",", $runids) . ") AND name='remote_task'";
  
  $result = mysql_query($sql) or die ("Query $sql failed : " . mysql_error());
  while ($hostrow = mysql_fetch_array($result)) {
    $runid = $hostrow["runid"];
    $platform = preg_replace("/^nmi:/", "", $hostrow["platform"]);
    $host = preg_replace("/\..+$/", "", $hostrow["host"]);

    if(!array_key_exists($runid, $hosts)) {
      $hosts[$runid] = Array();
    }

    $hosts[$runid][$platform] = $host;
  }
  mysql_free_result($result);

  return $hosts;
}

function get_queue_for_nmi_platform($platform, $type) {
  // For the purposes of this script crosstype => type
  if($type == "crosstest") $type = "test";

  $platform_without_prefix = preg_replace("/nmi:/", "", $platform);
  $output = `/usr/local/condor/bin/condor_q -global -const 'nmi_target_platform=="$platform_without_prefix" && nmi_run_type=="$type"' -format '%-2s ' 'ifThenElse(JobStatus==0,"U",ifThenElse(JobStatus==1,"I",ifThenElse(JobStatus==2,"R",ifThenElse(JobStatus==3,"X",ifThenElse(JobStatus==4,"C",ifThenElse(JobStatus==5,"H",ifThenElse(JobStatus==6,"E",string(JobStatus))))))))' -format "%6d " ClusterId -format " %-14s " Owner -format '%-11s ' 'formatTime(QDate,"%0m/%d %H:%M")' -format '%-11s\n' 'formatTime(EnteredCurrentStatus,"%0m/%d %H:%M")'`;
  $queue_contents = split("\n", $output);
  $depth = sizeof($queue_contents) - 1;
  $has_running_job = 0;
  if($depth != 0) {
    $output = "<table><tr><th>State</th><th>ID</th><th>Owner</th><th>Submitted</th><th>Started</th></tr>\n";
    foreach ($queue_contents as $line) {
      $items = preg_split("/\s+/", $line);
      if(sizeof($items) == 7) {
	$style = "background-color:#FFFFAA; text-decoration:none;";
	if($items[0] == "R") {
	  $style = "background-color:#0097C5;";
	  $has_running_job++;
	}
	else {
	  # If the job is not running we don't care about 'EnteredCurrentStatus',
	  # a.k.a. "Start Time"
	  $items[5] = "";
	  $items[6] = "";
	  if($items[0] == "H") {
	    $style = "background-color:#A1A1A1;";
	  }
	}
	$output .= "<tr style=\"$style\"><td style=\"text-align:center\">$items[0]</td><td>$items[1]</td><td>$items[2]</td><td>$items[3]&nbsp;$items[4]</td><td>$items[5]&nbsp;$items[6]</tr>\n";
      }
    }
    $output .= "</table>\n";
  }
  else {
    $output = "No jobs in queue";
  }

  $note = "";
  if($has_running_job == 0 && $depth != 0) {
    $note = "*";
    $output = "<font style=\"color:red\">* No job is running!</font><br />$output";
  }

  $ret = Array();
  $ret[0] = $depth;
  $ret[1] = "<br /><span class=\"link\"><a href=\"javascript: void(0)\" style=\"text-decoration:none;\">Q Depth: $depth$note<span>$output</span></a></span>";
  return $ret;
}

function make_sidebar() {
  echo "<div id='nav'>\n";

  echo "<p><a href='" . AUTO_URL . "'>Auto builds</a>\n";
  echo "<p><a href='" . ONEOFFS_URL . "'>One-off builds</a>\n";
  echo "<p><a href='" . QUEUE_DEPTH_URL . "'>NMI platform queue</a>\n";
  echo "<p><a href='../results-devel/'>Development area</a>\n";

  echo "</div>\n";
}


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
  $data["build"]["totals"] = Array("total" => 0, "failed" => 0, "pending" => 0, "passed" => 0);
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
  $data["test"]["totals"] = Array("total" => 0, "failed" => 0, "pending" => 0, "passed" => 0);
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
         "       Run.user = '$user' AND ".
         "       Task.runid = Run.runid AND ".
         "       Task.platform != 'local' AND ".
         "       project_version != component_version AND ".
         "	 component_version != 'native' ".
         " GROUP BY Task.platform ";
  $result2 = mysql_query($sql) or die ("Query $sql failed : " . mysql_error());
  $data["crosstest"] = Array();
  $data["crosstest"]["totals"] = Array("total" => 0, "failed" => 0, "pending" => 0, "passed" => 0);
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
    $info["pin"] = "";
    if( !(is_null($pin))) {
      $info["pin"] = "pin $pin";
    }
    $info["runid"] = "<a href='Task-search.php?runid=$runid&Submit=Search+by+RunID'>$runid</a>";

    if( $archived == '0' ) {
      $info["runid"] .= "<br><font size='-1'> D </font>";
    }
  }
  
  foreach (Array("build", "test", "crosstest") AS $type) {
    $platforms = $data[$type]["platforms"];
    $totals = $data[$type]["totals"];
    
    // Form a status table
    /*
    $list = Array();
    $list["passed"] = Array();
    $list["pending"] = Array();
    $list["failed"] = Array();
    foreach ($platforms as $platform) {
      array_push($list[$platforms[$platform]], $platform);
    }
    */
    
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
      //if (count($no_test_platforms)) {
      //  $sql = "SELECT count(DISTINCT platform) AS count ".
      //         "  FROM Run, Task ".
      //         " WHERE Run.runid = $runid  AND ".
      //         "       Task.runid = Run.runid AND ".
      //         "       Task.platform IN ('".implode("','", $no_test_platforms)."') ";
      //  $cnt_result = mysql_query($sql) or die ("Query $sql failed : " . mysql_error());
      //  $res = mysql_fetch_array($cnt_result);
      //  $no_test_cnt = $res["count"];
      //}
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

?>
