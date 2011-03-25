<?php
   //
   // Configuration
   //
   define("BRANCH_URL", "./Run-condor-branch.php?branch=%s&user=%s");
   define("DETAIL_URL", "./Run-condor-details.php?runid=%s&type=%s&user=%s");
   define("CROSS_DETAIL_URL", "./Run-condor-pre-details.php?runid=%s&type=%s&user=%s");
   define("CONDOR_USER", "cndrauto");
   
   $result_types = Array( "passed", "pending", "failed", "missing" );
   $num_spark_days = 2;

   require_once "./load_config.inc";
   load_config();

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

<h1><a href="./Run-condor.php" class="title">Condor Latest Build/Test Results</a></h1>
<table border=1 width="85%">
   <tr>
      <th>Branch</th>
      <th>Runid</th>
      <th>Submitted</th>
      <th>User</th>
      <th>Build Results</th>
      <th>Test Results</th>  
      <th>Cross Test Results</th>  
      </tr>

<?php
$db = mysql_connect(WEB_DB_HOST, DB_READER_USER, DB_READER_PASS) or die ("Could not connect : " . mysql_error());
mysql_select_db(DB_NAME) or die("Could not select database");

$query_runids="
SELECT 
  LEFT(description,
       (IF(LOCATE('branch-',description),
         LOCATE('branch-',description)+5,
         (IF(LOCATE('trunk-',description),
            LOCATE('trunk-',description)+4,
            CHAR_LENGTH(description)))))) AS branch,
  MAX(convert_tz(start, 'GMT', 'US/Central')) as start,
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
  DATE_SUB(CURDATE(), INTERVAL 10 DAY) <= start 
GROUP BY 
  branch,
  user,
  run_type 
ORDER BY 
  IF(user = '".CONDOR_USER."',0,1), runid desc
";

$last_user = CONDOR_USER;
$result = mysql_query($query_runids) or die ("Query ".$query_runids." failed : " . mysql_error());
while ($row = mysql_fetch_array($result)) {
   $runid      = $row["runid"];
   $branch     = $row["branch"];
   $start      = $row["start"];
   $user       = $row["user"];
   $run_result = $row["result"];
   $pin        = $row["archive_results_until"];
   
   //
   // Apply a break for nightly builds
   //
   if ($last_user == CONDOR_USER && $user != CONDOR_USER) {
      echo "<tr>\n".
           "   <td colspan=\"5\"><B><i>One-off Builds</i></b></td>\n".
           "</tr>\n";
   }

   // --------------------------------
   // BUILDS
   // --------------------------------
   $sql = "SELECT platform," . 
          "       SUM(IF(result != 0, 1, 0)) AS failed," . 
          "       SUM(IF(result IS NULL, 1, 0)) AS pending " . 
          "  FROM Task ".
          " WHERE runid = ${runid} AND ".
          "       platform != 'local'  ".
          " GROUP BY platform ";
   $result2 = mysql_query($sql) or die ("Query {$sql} failed : " . mysql_error());
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
          " WHERE Method_nmi.input_runid = {$runid} AND ".
          "       Run.runid = Method_nmi.runid AND ".
          "       Run.user = '$user'  AND ".
          "       Task.runid = Run.runid AND ".
          "       Task.platform != 'local' AND ".
          "       ((Run.project_version = Run.component_version)  OR (Run.component_version = 'native' ))".
          " GROUP BY Task.platform ";
   $result2 = mysql_query($sql) or die ("Query {$sql} failed : " . mysql_error());
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
          " WHERE Method_nmi.input_runid = {$runid} AND ".
          "       Run.runid = Method_nmi.runid AND ".
          "       Run.user = '$user'  AND ".
          "       Task.runid = Run.runid AND ".
          "       Task.platform != 'local' AND ".
          "       project_version != component_version AND ".
          "	  component_version != 'native' ".
          " GROUP BY Task.platform ";
   $result2 = mysql_query($sql) or die ("Query {$sql} failed : " . mysql_error());
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
   // HTML Table Row
   //
   $branch_url = sprintf(BRANCH_URL, $branch, $user);
   
   //
   // If there's nothing at all, just skip
   //
   // We only want to do this for one-off builds, if the nightly build
   // completely crapped out on us, we need to show it
   // Andy - 11/30/2006
   //
   if ($user != CONDOR_USER && !count($data["build"]["platforms"]) && !count($data["test"]["platforms"])) continue;

   // Is this top level run pinned or not(probably not but could be one of a kind)
   
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

   $pincheck = mysql_query($findpin) or die ("Query {$findpin} failed : " . mysql_error());
   while ($pindetails = mysql_fetch_array($pincheck)) {
     $pin = $pindetails["archive_results_until"];
     $archived = $pindetails["archived"];
     if( !(is_null($pin))) {
       $pinstr = "pin ". "$pin";
     }
     else {
       $pinstr = "";
     }
     if( $archived == '0' ) {
       $archivedstr = "$runid". "<br><font size=\"-1\"> D </font>";
     }
     else {
       $archivedstr = "$runid";
     }
   }
   
   echo <<<EOF
   <tr>
      <td><a href="{$branch_url}">{$branch}</a><br><font size="-2">$pinstr</font></td>
      <td align="center">{$archivedstr}</td>
      <td align="center">{$start}</td>
      <td align="center">{$user}</td>
EOF;

   foreach (Array("build", "test", "crosstest") AS $type) {
     if(preg_match("/Continuous/", $branch)) {
       if($type == "test" || $type == "crosstest") {
	 continue;
       }

       // We are going to build "sparklines" for the Continuous builds to make
       // it easier to visualize their performance over time

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
       $result2 = mysql_query($sql) or die ("Query {$sql} failed : " . mysql_error());

       $spark = "<p style=\"font-size:75%\">Last $num_spark_days days of results: newest at left<table><tr>\n";
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
	 
	 $details = "<table>";
	 $details .= "<tr><td>Status</td><td class=\"$color\">$color</td></tr>";
	 $details .= "<tr><td>NMI RunID</td><td>" . $build["runid"] . "</td></tr>";
	 $details .= "<tr><td>Submitted</td><td><nobr>" . $build["start"] . "</nobr></td></tr>";
	 $details .= "<tr><td>SHA1</td><td>" . substr($build["project_version"], 0, 15) . "</td></tr>";
	 $details .= "</table>";

	 $detail_url = sprintf(DETAIL_URL, $build["runid"], "build", $user);
	 $box_html = "<span class=\"link\"><a href=\"$detail_url\" style=\"text-decoration:none\">&nbsp;&nbsp;<span>$details</span></a></span>";

	 $tmp = Array();
	 $tmp["runid"] = $build["runid"];
	 $tmp["sha1"]  = substr($build["project_version"], 0, 15);
	 $tmp["html"]  = $box_html; 
	 $tmp["color"] = $color;
	 array_push($builds, $tmp);
       }
       mysql_free_result($result2);
       
       for ($i=0; $i < count($builds); $i++) {
	 $style = "";
	 if($i != 0 && $builds[$i]["sha1"] == $builds[$i-1]["sha1"]) {
	   $style .= "border-left-style:dashed;";
	 }
	 if($builds[$i+1] && $builds[$i]["sha1"] == $builds[$i+1]["sha1"]) {
	   $style .= "border-right-style:dashed;";
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
	      "      DATE_SUB(CURDATE(), INTERVAL 2 DAY) <= start AND " .
	      "      description LIKE 'Auto-Test Suite for ($platform, %' " .
	      "ORDER BY description DESC";
       $result2 = mysql_query($sql) or die ("Query {$sql} failed : " . mysql_error());

       $tests = Array();
       while ($test = mysql_fetch_array($result2)) {
	 $build_runid = preg_replace("/Auto-Test Suite for \($platform, (.+)\)/", "$1", $test["description"]);
	 $color = "passed";
	 if($test["result"] == NULL) {
	   $color = "pending";
	 }
	 elseif($test["result"] != 0) {
	   $color = "failed";
	 }

	 $details = "<table>";
	 $details .= "<tr><td>Status</td><td class=\"$color\">$color</td></tr>";
	 $details .= "<tr><td>Start</td><td><nobr>" . $test["start"] . "</nobr></td></tr>";
	 $details .= "<tr><td>SHA1</td><td>_PUT_SHA_HERE_</td></tr>";
	 $details .= "</table>";

	 $detail_url = sprintf(DETAIL_URL, $build_runid, "test", $user);
	 $box_html = "<span class=\"link\"><a href=\"$detail_url\" style=\"text-decoration:none\">&nbsp;&nbsp;<span>$details</span></a></span>";

	 $tests[$build_runid] = Array();
	 $tests[$build_runid]["color"] = $color;
	 $tests[$build_runid]["html"]  = $box_html;
       }
       mysql_free_result($result2);

       $spark .= "<tr><td class='sparkheader'>Test:</td>\n";
       for ($i=0; $i < count($builds); $i++) {
	 $build = $builds[$i];

	 $style = "";
	 if($i != 0 && $build["sha1"] == $builds[$i-1]["sha1"]) {
	   $style .= "border-left-style:dashed;";
	 }
	 if($builds[$i+1] && $build["sha1"] == $builds[$i+1]["sha1"]) {
	   $style .= "border-right-style:dashed;";
	 }

	 if($tests[$build["runid"]]) {
	   $test = $tests[$build["runid"]];
	   $tmp = preg_replace("/_PUT_SHA_HERE_/", $build["sha1"], $test["html"]);
	   $spark .= "<td class='" . $test["color"] . "' style='$style'>$tmp</td>\n";
	 }
	 else {
	   $spark .= "<td class='noresults' style='$style'>&nbsp;</td>\n";
	 }
       }
       
       $spark .= "</tr></table>\n";

       echo "<td colspan='3'>$spark</td>";
     }
     else {
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

      ##
      ## Check for missing tests
      ## Since we know how many builds have passed and should fire off tests,
      ## we can do a simple check to make sure the total number of tests
      ## is equal to the the number of builds
      ## Andy - 01.09.2007
      ##
      if ($type == "test") {
         $no_test_cnt = 0;
         if (count($no_test_platforms)) {
            $sql = "SELECT count(DISTINCT platform) AS count ".
                   "  FROM Run, Task ".
                   " WHERE Run.runid = {$runid}  AND ".
                   "       Task.runid = Run.runid AND ".
                   "       Task.platform IN ('".implode("','", $no_test_platforms)."') ";
            $cnt_result = mysql_query($sql)
               or die ("Query {$sql} failed : " . mysql_error());
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
	    echo <<<EOF
	      <td class="{$status}" align="center">
	      <table cellpadding="1" cellspacing="0" width="100%" class="details">
	      <tr>
	      <td colspan="2" class="detail_url"><a href="{$detail_url}">$status</a></td>
	      </tr>
	      </table>
	      </td>
EOF;
         //
         // Just display an empty cell
         //
         }
	 elseif($type == "test") {
	    echo <<<EOF
	      <td class="noresults" align="center">
	      <table cellpadding="1" cellspacing="0" width="100%" class="details">
	      <tr><td colspan="2" class="detail_url"><a href="{$detail_url}">No Completed Builds</a></td></tr>
	      <tr><td colspan="2">&nbsp;</td></tr>
	      <tr><td colspan="2">&nbsp;</td></tr>
	      <tr><td colspan="2">&nbsp;</td></tr>
	      </table>
	      </td>
EOF;
	 }
	 else {
            echo "<td>&nbsp;</td>\n";
         }
      //
      // Show Summary
      //
      }
      else {
                        
         echo <<<EOF
         <td class="{$color}" align="center" valign="top">
            <table cellpadding="1" cellspacing="0" width="100%" class="details">
               <tr>
	          <td colspan="2" class="detail_url">
                    <a href="{$detail_url}">$status</a>
	          </td>
               </tr>
EOF;
         //
         // Show the different status tallies for platforms
         //
         foreach ($result_types AS $key) {
            if ($key == "missing" && empty($totals[$key])) continue;
	    $prefix = $postfix = "";
            if ($key == "missing") {
               $prefix = "<B>";
               $postfix = "</B>";
            }

            echo "<tr>\n".
               "   <td width=\"75%\">{$prefix}".ucfirst($key)."{$postfix}</td>\n".
               "   <td width=\"25%\">{$prefix}".(int)$totals[$key]."{$postfix}</td>\n".
               "</tr>\n";
         }
         echo "</table></td>\n";
      } // RESULTS
     }
   } // FOREACH
   
   echo "</tr>";
   $last_user = $user;
} // WHILE
mysql_free_result ($result);

echo "</table>";
echo "</center>";

mysql_close($db);

?>
</body>
</html>
