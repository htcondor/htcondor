<?php
   //
   // Configuration
   //
   define("BRANCH_URL", "./Run-condor-branch.php?branch=%s&user=%s");
   define("DETAIL_URL", "./Run-condor-details.php?runid=%s&type=%s&user=%s");
   define("CROSS_DETAIL_URL", "./Run-condor-pre-details.php?runid=%s&type=%s&user=%s");
   define("CONDOR_USER", "cndrauto");
   
   $result_types = Array( "passed", "pending", "failed", "missing" );

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
      <th>Branch<br><small>Click to see branch history</small></th>
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
   $sql = "SELECT Task.platform, Task.result" . 
          "  FROM Task, Run ".
          " WHERE Run.runid = {$runid}  AND ".
          "       Task.runid = Run.runid AND ".
          "       Task.platform != 'local' ".
          " GROUP BY Task.platform ";
   $result2 = mysql_query($sql) or die ("Query {$sql} failed : " . mysql_error());
   $data["build"] = Array();
   $data["build"]["totals"] = Array();
   $data["build"]["platforms"] = Array();
   while ($platforms = mysql_fetch_array($result2)) {
     $data["build"]["totals"]["total"]++;
     if($platforms["result"] == NULL) {
       $data["build"]["totals"]["pending"]++;
       $data["build"]["platforms"][$platforms["platform"]] = "pending";
     }
     elseif($platforms["result"] == 0) {
       $data["build"]["totals"]["passed"]++;
       $data["build"]["platforms"][$platforms["platform"]] = "passed";       
     }
     elseif($platforms["result"] != 0) {
       $data["build"]["totals"]["failed"]++;
       $data["build"]["platforms"][$platforms["platform"]] = "failed";
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
