<?php

include "CondorQ.php";

define("AUTO_URL", "./nightlies.php");
define("ONEOFFS_URL", "./one-offs.php");
define("QUEUE_DEPTH_URL", "./nmi-status.php");

define("BRANCH_URL", "./branch.php?branch=%s&user=%s");
define("DETAIL_URL", "./run-details.php?runid=%s");
define("CROSS_DETAIL_URL", "./xtest-summary.php?runid=%s&type=%s&user=%s");

define("GITSHA1","http://condor-git.cs.wisc.edu/?p=condor.git;a=commit;h=%s");
define("GITSHA1SHORT","http://condor-git.cs.wisc.edu/?p=condor.git;a=log;h=%s;hp=%s");

// Given a set of runids, return a hash of the platform/host combinations of where the jobs executed
function get_hosts($dash, $runids) {
  $hosts = Array();

  if(!$runids) return $hosts;

  $sql = "SELECT runid, platform, host
          FROM Task
          WHERE runid in (" . implode(",", $runids) . ") AND name='remote_task'";
  
  $results = $dash->db_query($sql);
  foreach ($results as $hostrow) {
    $runid = $hostrow["runid"];
    $platform = preg_replace("/^nmi:/", "", $hostrow["platform"]);
    $host = preg_replace("/\..+$/", "", $hostrow["host"]);

    if(!array_key_exists($runid, $hosts)) {
      $hosts[$runid] = Array();
    }

    $hosts[$runid][$platform] = $host;
  }

  return $hosts;
}

function get_queue_for_nmi_platform($platform, $dash) {
  $platform_without_prefix = preg_replace("/nmi:/", "", $platform);
  
  $condorq = new CondorQ($dash->condor_status());
  $condorq->add_platform($platform_without_prefix);
  $per_platform_queues = $condorq->condor_q();
  
  return $per_platform_queues[$platform_without_prefix]["html-queue"];
}


function get_results($dash, $info, $runid, $user, $run_result) {
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
  $results = $dash->db_query($sql);
  $data["build"] = Array();
  $data["build"]["totals"] = Array("total" => 0, "failed" => 0, "pending" => 0, "passed" => 0);
  $data["build"]["platforms"] = Array();
  foreach ($results as $platforms) {
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
  $results = $dash->db_query($sql);
  $data["test"] = Array();
  $data["test"]["totals"] = Array("total" => 0, "failed" => 0, "pending" => 0, "passed" => 0);
  $data["test"]["platforms"] = Array();
  foreach ($results as $platforms) {
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
  $results = $dash->db_query($sql);
  $data["crosstest"] = Array();
  $data["crosstest"]["totals"] = Array("total" => 0, "failed" => 0, "pending" => 0, "passed" => 0);
  $data["crosstest"]["platforms"] = Array();
  foreach ($results as $platforms) {
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
  
  $pincheck = $dash->db_query($findpin);
  foreach ($pincheck as $pindetail) {
    $pin = $pindetails["archive_results_until"];
    $archived = $pindetails["archived"];
    $info["pin"] = "";
    if( !(is_null($pin))) {
      $info["pin"] = "pin $pin";
    }
    $info["runid"] = "<a href='/nmi/results/details?runID=$runid'>$runid</a>";

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
      $totals["missing"] = $data["build"]["totals"]["passed"] - $totals["total"] - $no_test_cnt;
      if ($totals["missing"] > 0) $color = "FAILED";
      elseif ($totals["missing"] < 0) $totals["missing"] = 0;
    }
    
    if($type == "crosstest") {
      $detail_url = sprintf(CROSS_DETAIL_URL, $runid, $type, $user);
    }
    else {
      $detail_url = sprintf(DETAIL_URL, $runid);
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
