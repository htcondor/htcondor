<?php   
   //
   // Configuration
   //
	// tabstop=3
   define("BRANCH_URL", "./Run-condor-branch.php?branch=%s&user=%s");
   define("DETAIL_URL", "./Run-condor-details.php?runid=%s&type=%s");
   define("CONDOR_USER", "cndrauto");
   
   $result_types = Array( "passed", "pending", "failed" );

   require_once "./load_config.inc";
   load_config();

   # get args
   $type = "test";
   $user = "cndrauto";
   $test = rawurldecode($_REQUEST["test"]);
   $branch = $_REQUEST["branch"];
	$rows   = ($_REQUEST["rows"] ? $_REQUEST["rows"] : 30);

   define('PLATFORM_PENDING', 'pending');
   define('PLATFORM_FAILED',  'failed');
   define('PLATFORM_PASSED',  'passed');
?>
<html>
<head>
<Title>NMI - Results of <?= $type; ?>s for build ID:<?= $build_id; ?></TITLE>
<LINK REL="StyleSheet" HREF="condor.css" TYPE="text/css">
</HEAD>
<body>

<form method="get" action="<?= $_SERVER{PHP_SELF}; ?>">
<input type="hidden" name="branch" value="<?= $branch; ?>">
<input type="hidden" name="type" value="<?= $type; ?>">
<input type="hidden" name="test" value="<?= $test; ?>">
<input type="hidden" name="user" value="<?= $user; ?>">
<font size="+1"><b>Rows: </b></font>&nbsp;
<select name="rows">
<?php
   $arr = Array("30", "60", "90", "120", "150", "300");
   foreach ($arr AS $val) {
      echo "<option".
           ($val == $rows ? " SELECTED" : "").
           ">$val</option>\n";
   } // FOREACH
 ?>
 </select>
 <input type="submit" value="Show Results">
 </form>

<?php

   $db = mysql_connect(WEB_DB_HOST, DB_READER_USER, DB_READER_PASS) or die ("Could not connect : " . mysql_error());
   mysql_select_db(DB_NAME) or die("Could not select database");

   include "last.inc";
	
	# Create the set of all the runid's we're interested in
	# This is the MAX(runid) i.e. latest run  
	# for each (branch, type) tuple

	echo "<h1>Test History - $branch ($test)</h1>\n";

$sql = "SELECT description,
               convert_tz(start, 'GMT', 'US/Central') as start,
               run_type, 
               runid,
                    archived,
                    archive_results_until,
               result
          FROM Run 
         WHERE component='condor' AND 
               project='condor' AND
               run_type='build' AND
               description LIKE '".$branch."%'".
       (!empty($user) ? " AND user = '$user'" : "").
       " ORDER BY runid DESC ".
       " LIMIT $rows";

// This time through the runids gather ALL Platforms ever used during the period
$results = mysql_query($sql) or die ("Query {$sql} failed : " . mysql_error());

$platforms = Array();
$platform_labels = Array();
$plaformlabel;
$component_version; // native or binaries being tested

while ($runidrow = mysql_fetch_array($results)) {
  	$runid      = $runidrow["runid"];
	// for each runid find all platforms but only insert
	// them into the platforms array if not there yet
	// we include cross test platforms by finding all
	// test run runids which used the build result as
	// input.

   //
   // Platforms
   //
   // Save original platforms we actually built on
   $sql_pls = "SELECT DISTINCT(platform) AS platform, component_version ".
          //"  FROM Run, Task ".
          //" WHERE Task.runid = $runid ".
          //"   AND Task.runid = Run.runid ".
          "  FROM Method_nmi , Task, Run ".
          " WHERE Method_nmi.input_runid = $runid".
          "   AND Task.runid =  Method_nmi.runid ".
          "   AND Task.runid =  Run.runid ".
          //"   AND Run.user = '$user' ".
          "   AND Task.platform != 'local' ".
          " ORDER BY (IF (platform='local', 'zzz', platform))";
   $platform_results = mysql_query($sql_pls) or die ("Query $sql_pls failed : " . mysql_error());
	while ($row = mysql_fetch_array($platform_results)) {
		$component_version = $row["component_version"];
		if( array_key_exists($row["platform"],$platforms)) {
      	//$tmpp = $row["platform"];
			//echo "<H4>skip $tmpp</H4>";
		} else {
      		$tmpp = $platforms[$row["platform"]] = $row["platform"];
			if($component_version == 'native') {
      			$platform_labels[$row["platform"]] = "\nNative\n";
			} else {
      			$platform_labels[$row["platform"]] = "Cross of\n".
				"$component_version"; 
			}
			//echo "<H4>adding $tmpp</H4>";
		}
   }
   mysql_free_result($platform_results);
   
}
mysql_free_result($results);

// This time through the runids show results
$sql4 = "SELECT description,
               convert_tz(start, 'GMT', 'US/Central') as start,
               run_type, 
               runid,
                    archived,
                    archive_results_until,
               result
          FROM Run 
         WHERE component='condor' AND 
               project='condor' AND
               run_type='build' AND
               description LIKE '".$branch."%'".
       (!empty($user) ? " AND user = '$user'" : "").
       " ORDER BY runid DESC ".
       " LIMIT $rows";

$results = mysql_query($sql4) or die ("Query {$sql4} failed : " . mysql_error());

// Ideally start table now
// reproduce platform row every 10-15 rows
// make missed test runs a <tr><td>Missed tests x/y/z</td></tr>
// end table when all done

// For every runid of a build
$rowcount = 0;
echo "<table border=\"0\" cellspacing=\"0\" >\n";
while ($runidrow = mysql_fetch_array($results)) {
  	$runid      = $runidrow["runid"];
  	$desc       = $runidrow["description"];
  	$runstart      = $runidrow["start"];

	$data = array();
	
	//echo "<h1>Start - ".date("m/d/Y", $start)."</h1>\n";
   $build_id = $runid;
	
   $sql = "SELECT host, gid, UNIX_TIMESTAMP(start) AS start ".
          "  FROM Run ".
          " WHERE Run.runid = $build_id".
          "   AND Run.user = '$user'";
   $result = mysql_query($sql) or die ("Query $sql failed : " . mysql_error());
   $row = mysql_fetch_array($result);
   $host  = $row["host"];
   $gid   = $row["gid"];
   $start = $row["start"];
   mysql_free_result($result);
   //echo "<h1><a href=\"./Run-condor.php\" class=\"title\">Condor Latest Build/Test Results</a> ".
        //":: ".ucfirst($type)."s for Build ID $runid (".date("m/d/Y", $start).")</h1>\n";

   //
   // Platforms
   //
   // Saved in the very beginning 
   
   $testrunids = Array();
   //
   // Test
   //
   if ($type == 'test') {
      $sql2 = "SELECT DISTINCT(Task.name) AS name ".
             "  FROM Task, Method_nmi ".
             " WHERE Task.runid = Method_nmi.runid ".
			 //"   AND Task.name = '$test' ".
			 "   AND Task.name = '$test' ".
             "   AND Method_nmi.input_runid = $runid ".
             " ORDER BY Task.name ASC";
             
      //
      // We also need testrunids
      //
      $runid_sql = "SELECT DISTINCT Method_nmi.runid ".
                   "  FROM Method_nmi, Run ".
                   " WHERE Method_nmi.input_runid = $runid ".
                   "   AND Run.runid = Method_nmi.runid ".
		   		   // "  AND (component_version = project_version or ( component_version = 'native'))".
                   "   AND Run.user = '$user' ";
      $result = mysql_query($runid_sql) or die ("Query $runid_sql failed : " . mysql_error());
      while ($row = mysql_fetch_array($result)) {
         $tmp = $testrunids[] = $row["runid"];
		 	//echo "<H3>$tmp</H3>";
		 	//echo "<H3>$tmp, $row["component_version"], $row["platform"]</H3>\n";
      }
	  //echo "<H3>done</H3>";
      mysql_free_result($result);
      
   //
   // Unknown
   //
   } else {
      die("Unsupported parameter type=$type");
   }
   
   $result = mysql_query($sql2) or die ("Query $sql2 failed : " . mysql_error());
   while ($row = mysql_fetch_array($result)) {
      $task_name = $row["name"];
   
      //
      // Now for each task, get the status from the platforms
      //
      $sql3 = "SELECT platform, result, runid ".
             "  FROM Task ".
             " WHERE Task.runid IN (".implode(",", $testrunids).") ".
             "   AND Task.name = '$task_name'";
      $task_result = mysql_query($sql3) or die ("Query $sql3 failed : " . mysql_error());


      while ($task_row = mysql_fetch_array($task_result)) {
         $platform = $task_row["platform"];
         $platform_runids[$platform] = $task_row["runid"];
         $result_value = $task_row["result"];
         
         if (is_null($result_value)) {
            $result_value = PLATFORM_PENDING;
            $platform_status[$platform] = PLATFORM_PENDING;
            $task_status[$task_name] = PLATFORM_PENDING;
         } elseif ($result_value) {
            $platform_status[$platform] = PLATFORM_FAILED;
            $task_status[$task_name] = PLATFORM_FAILED;
         } elseif (!$platform_status[$platform]) {
            $platform_status[$platform] = PLATFORM_PASSED;
         }

         if (!$task_status[$task_name]) {
            $task_status[$task_name] = PLATFORM_PASSED;
         }
         $data[$row["name"]][$task_row["platform"]] = $result_value;
      } // WHILE
      mysql_free_result($task_result);
   } // WHILE
   mysql_free_result($result);
   // need to lookup location later.....mysql_close($db);
   
?>

<?php
	if( sizeof($data) == 0 ) {
		echo "<tr><td></td><td colspan=\"8\"><strong>No Tests on ".date("m/d/Y", $start)." - Runid $runid - </strong></td></tr>\n";
	} 
   foreach ($data AS $task => $arr) {
		// we are focusing on a single test history, skip the rest
		$diff = strcmp($task,$test);
		if($diff ==  0) {
			if(($rowcount % 20) == 0) {
				//echo "<table border=\"0\" cellspacing=\"0\" >\n";
				echo "<tr>\n";
				echo "<td>&nbsp</td>\n";
   				foreach ($platforms AS $platform) {
      				$display = $platform;
      				$idx = strpos($display, "_");
      				$display[$idx] = " ";
						$filepath = "";
      				echo "<td>$display</td>\n";
   				} // FOREACH 
      			echo "</tr>\n";
				// Cross or Native labels
				//echo "<table border=\"0\" cellspacing=\"0\" >\n";
				echo "<tr>\n";
				echo "<td>&nbsp</td>\n";
   				foreach ($platform_labels AS $label) {
      				$display = $label;
      				$idx = strpos($display, "_");
      				$display[$idx] = " ";
						$filepath = "";
      				echo "<td>$display</td>\n";
   				} // FOREACH 
      			echo "</tr>\n";
			}
		$gotests = "<a href=\"http://$host/results/Run-condor-details.php?runid=$runid&type=test&user=cndrauto\">";
      	echo "<tr>\n".
				"<td>".
 			//"<td ".($task_status[$task] != PLATFORM_PASSED ? 
 				//"class=\"".$task_status[$task]."\"" : "").">".
 				"<span title=\"$task\">$gotests".date("m/d/Y", $start)."</a></span></td>\n";
 				//"<span title=\"$task\">$runstart</span></td>\n";
      
      	foreach ($platforms AS $platform) {
         	$result = $arr[$platform];
         	if ($result == PLATFORM_PENDING) {
            		echo "<td align=\"center\" class=\"{$result}\">-</td>\n";
         	} else {
					if ($result == '') {
						echo "<td align=\"center\">&nbsp;</td>\n";
					} else {
						$display = "<a href=\"http://$host/results/Run-condor-taskdetails.php?platform={$platform}&task=".urlencode($task)."&runid=".$platform_runids[$platform]. "\">$result</a>";
						echo "<td class=\"".($result == 0 ? PLATFORM_PASSED : PLATFORM_FAILED)."\" ".
							"align=\"center\"><B>$display</B></td>\n";
					}
         	}
      	}
			$rowcount++;
      	echo "</td>\n";
   		//echo "</table>";
		} 
   } // FOREACH

}
echo "</table>";
   function limitSize($str, $cnt) {
      if (strlen($str) > $cnt) {
         $str = substr($str, 0, $cnt - 3)."...";
      }
      return ($str);
   }

   // done looking up locations.....mysql_close($db);
   mysql_close($db);
?>
</body>
</html>

