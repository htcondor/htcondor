<?php
   //
   // Configuration
   //
   define("BRANCH_URL", "./Run-condor-branch.php?branch=%s&user=%s");
   define("DETAIL_URL", "./Run-condor-details.php?runid=%s&type=%s&user=%s");
   define("CONDOR_USER", "cndrauto");
   
   $result_types = Array( "passed", "pending", "failed", "missing" );

   require_once "./load_config.inc";
   load_config();

   //
   // These are the platforms that never have tests in the nightly builds
   // This is a hack for now and there's no way to differentiate between different
   // branchs. But you know, life is funny that way -- you never really get what you want
   //
   $no_test_platforms = Array( "ppc_macos_10.4", "x86_macos_10.4" );
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
	      <th>Last build</th>
	      <th>User</th>
	      <th>Build Results</th>
	      <th>Test Results</th>  
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
	   $sql = "SELECT SUM(IF(Task.result = 0, 1, 0)) AS passed, ".
		  "       SUM(IF(Task.result != 0, 1, 0)) AS failed, ".
		  "       SUM(IF(Task.result IS NULL, 1, 0)) AS pending ".
		  "  FROM Task, Run ".
		  " WHERE Run.runid = {$runid}  AND ".
		  "       Task.runid = Run.runid AND ".
		  "       Task.platform != 'local' ".
		  " GROUP BY Task.platform ";
	   $result2 = mysql_query($sql)
			  or die ("Query {$sql} failed : " . mysql_error());
	   $data["build"] = Array();
	   while ($platforms = mysql_fetch_array($result2)) {
	      if (!empty($platforms["failed"])) {
		 $data["build"]["failed"]++;
	      } elseif (!empty($platforms["pending"])) {
		 $data["build"]["pending"]++;
	      } elseif (!empty($platforms["passed"])) {
		 $data["build"]["passed"]++;
	      }
	   } // WHILE
	   mysql_free_result($result2);

	   // --------------------------------
	   // TESTS
	   // --------------------------------
	   $sql = "SELECT SUM(IF(Task.result = 0, 1, 0)) AS passed, ".
		  "       SUM(IF(Task.result != 0, 1, 0)) AS failed, ".
		  "       SUM(IF(Task.result IS NULL, 1, 0)) AS pending ".
		  "  FROM Task, Run, Method_nmi ".
		  " WHERE Method_nmi.input_runid = {$runid} AND ".
		  "       Run.runid = Method_nmi.runid AND ".
		  "       Task.runid = Run.runid AND ".
		  "       Task.platform != 'local' ".
		  " GROUP BY Task.platform ";
	   $result2 = mysql_query($sql)
			  or die ("Query {$sql} failed : " . mysql_error());
	   $data["test"] = Array();
	   while ($platforms = mysql_fetch_array($result2)) {
	      if (!empty($platforms["failed"])) {
		 $data["test"]["failed"]++;
		 $data["test"]["total"]++;
	      } elseif (!empty($platforms["pending"])) {
		 $data["test"]["pending"]++;
		 $data["test"]["total"]++;
	      } elseif (!empty($platforms["passed"])) {
		 $data["test"]["passed"]++;
		 $data["test"]["total"]++;
	      }
	   } // WHILE
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
	   if ($user != CONDOR_USER && !count($data["build"]) && !count($data["test"])) continue;
	   
	   echo <<<EOF
	   <tr>
	      <td><a href="{$branch_url}">{$branch}</a></td>
      <td align="center">{$start}</td>
      <td align="center">{$user}</td>
EOF;

   foreach (Array("build", "test") AS $type) {
      $cur = $data[$type];
      $status = ($cur["failed"] ? "FAILED" :
                ($cur["pending"] ? "PENDING" : "PASSED"));
      $color = $status;

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
         $cur["missing"] = $data["build"]["passed"] - $cur["total"] - $no_test_cnt;
         if ($cur["missing"] > 0) $color = "FAILED";
         elseif ($cur["missing"] < 0) $cur["missing"] = 0;
      }

      $detail_url = sprintf(DETAIL_URL, $runid, $type, $user);
      
      //
      // No results
      //
      if (!count($cur)) {
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
         } else {
            echo "<td>&nbsp;</td>\n";
         }
      //
      // Show Summary
      //
      } else {
                        
         echo <<<EOF
         <td class="{$color}" align="center" valign="top">
            <table cellpadding="1" cellspacing="0" width="100%" class="details">
               <tr>
                  <td colspan="2" class="detail_url"><a href="{$detail_url}">$status</a></td>
               </tr>
EOF;
         //
         // Show the different status tallies for platforms
         //
         foreach ($result_types AS $key) {
            if ($key == "missing" && empty($cur[$key])) continue;
            if ($key == "missing") {
               $prefix = "<B>";
               $postfix = "</B>";
            } else {
               $prefix = $postfix = "";
            }

            echo "<tr>\n".
               "   <td width=\"75%\">{$prefix}".ucfirst($key)."{$postfix}</td>\n".
               "   <td width=\"25%\">{$prefix}".(int)$cur[$key]."{$postfix}</td>\n".
               "</tr>\n";
         } // FOREACH
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
