<?php   
   //
   // Configuration
   //
   define("HISTORY_URL", "./Test-history.php?branch=%s&test=%s");
   define("BRANCH_URL", "./Run-condor-branch.php?branch=%s&user=%s");
   define("DETAIL_URL", "./Run-condor-details.php?runid=%s&type=%s");
   define("CONDOR_USER", "cndrauto");
   
   $result_types = Array( "passed", "pending", "failed" );

   require_once "./load_config.inc";
   load_config();

   # get args
   $type = $_REQUEST["type"];
   $runid = (int)$_REQUEST["runid"];
   $user = $_REQUEST["user"];
   $build_id = $runid;
   $branch = "unknown";
   
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

<?php

   $db = mysql_connect(WEB_DB_HOST, DB_READER_USER, DB_READER_PASS) or die ("Could not connect : " . mysql_error());
   mysql_select_db(DB_NAME) or die("Could not select database");

   include "last.inc";

	// 
	// need to have the branch if we get a request for a test history
	// Test-history.php?branch=xxxxx&test=yyyyyy
	//

	$query_branch="
		
		SELECT 
  			LEFT(description,
       		(IF(LOCATE('branch-',description),
         	LOCATE('branch-',description)+5,
         	(IF(LOCATE('trunk-',description),
            LOCATE('trunk-',description)+4,
            CHAR_LENGTH(description)))))) AS branch
		FROM 
  			Run 
		WHERE 
  			runid=$runid";
	
	$result = mysql_query($query_branch) or die ("Query ".$query_branch." failed : " . mysql_error());
	
	$row = mysql_fetch_array($result);
	$branch = $row["branch"];


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

   echo "<h1><a href=\"./Run-condor.php\" class=\"title\">Condor Latest Build/Test Results</a> ".
        ":: ".ucfirst($type)."s for Build ID $runid $branch (".date("m/d/Y", $start).")</h1>\n";

   //
   // Platforms
   //
   // Original platforms we actually built on
   $sql = "SELECT DISTINCT(platform) AS platform ".
          "  FROM Run, Task ".
          " WHERE Task.runid = $runid ".
          "   AND Task.runid = Run.runid ".
          "   AND Run.user = '$user' ".
          "   AND Task.platform != 'local' ".
          " ORDER BY (IF (platform='local', 'zzz', platform))";
   $result = mysql_query($sql) or die ("Query $sql failed : " . mysql_error());
   $platforms = Array();
   while ($row = mysql_fetch_array($result)) {
      $tmpp = $platforms[] = $row["platform"];
		//echo "<H3>$tmpp</H3>";
   }
   mysql_free_result($result);
   
   //
   // Build
   //
   $runids = Array();
   if ($type == 'build') {
      $sql = "SELECT DISTINCT(Task.name) AS name ".
             "  FROM Task ".
             " WHERE Task.runid = $runid ".
             " ORDER BY Task.name ASC";
      $runids[] = $runid;
   //
   // Test
   //
   } elseif ($type == 'test') {
      $sql = "SELECT DISTINCT(Task.name) AS name ".
             "  FROM Task, Method_nmi ".
             " WHERE Task.runid = Method_nmi.runid ".
             "   AND Method_nmi.input_runid = $runid ".
             " ORDER BY Task.name ASC";
             
      //
      // We also need runids
      //
      $runid_sql = "SELECT DISTINCT Method_nmi.runid ".
                   "  FROM Method_nmi, Run ".
                   " WHERE Method_nmi.input_runid = $runid ".
                   "   AND Run.runid = Method_nmi.runid ".
		   		   "  AND (component_version = project_version or ( component_version = 'native'))".
                   "   AND Run.user = '$user' ";
      $result = mysql_query($runid_sql) or die ("Query $runid_sql failed : " . mysql_error());
      while ($row = mysql_fetch_array($result)) {
         $tmp = $runids[] = $row["runid"];
		 //echo "<H3>$tmp</H3>";
		 //echo "<H3>$tmp, $row["component_version"], $row["platform"]</H3>";
      }
      mysql_free_result($result);
      
   //
   // Unknown
   //
   } else {
      die("Unsupported parameter type=$type");
   }
   
   $result = mysql_query($sql) or die ("Query $sql failed : " . mysql_error());
   while ($row = mysql_fetch_array($result)) {
      $task_name = $row["name"];
   
      //
      // Now for each task, get the status from the platforms
      //
      $sql = "SELECT platform, result, runid ".
             "  FROM Task ".
             " WHERE Task.runid IN (".implode(",", $runids).") ".
             "   AND Task.name = '$task_name'";
      $task_result = mysql_query($sql) or die ("Query $sql failed : " . mysql_error());
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

<table border="0" cellspacing="0" >
<tr>
   <td>Name</td>
<?php
	// show link to run directory for each platform
   foreach ($platforms AS $platform) {
      $display = $platform;
      $idx = strpos($display, "_");
      $display[$idx] = " ";
		$filepath = "";
   
   	// have to lookup the file location now
		$loc_query = "SELECT * FROM Run WHERE runid='$platform_runids[$platform]'";
		$loc_query_res = mysql_query($loc_query) or die ("Query failed : " . mysql_error());
		while( $locrow = mysql_fetch_array($loc_query_res) ) {
			$filepath = $locrow["filepath"];
			$mygid = $locrow["gid"];
		}

		$display = "<a href=\"$filepath/$mygid/userdir/$platform/\" ".
                 "title=\"View Run Directory\">$display</a>";
      echo "<td align=\"center\" class=\"".$platform_status[$platform]."\">$display</td>\n";
   } // FOREACH 
?>
<tr>
   <td>Results</td>
<?php
	// show link to results for each platform
   foreach ($platforms AS $platform) {
      $display = $platform;
      $idx = strpos($display, "_");
      $display[$idx] = " ";
		$filepath = "";
   
   	// have to lookup the file location now
		$loc_query = "SELECT * FROM Run WHERE runid='$platform_runids[$platform]'";
		$loc_query_res = mysql_query($loc_query) or die ("Query failed : " . mysql_error());
		while( $locrow = mysql_fetch_array($loc_query_res) ) {
			$filepath = $locrow["filepath"];
			$mygid = $locrow["gid"];
		}

		$display = "<a href=\"$filepath/$mygid/userdir/$platform/results.tar.gz\" ".
                 "title=\"View Run Directory\">click</a>";
      echo "<td align=\"center\" class=\"".$platform_status[$platform]."\">$display</td>\n";
   } // FOREACH 
   foreach ($data AS $task => $arr) {
   	if ($type == 'test') {
			$history_url = sprintf(HISTORY_URL,$branch,rawurlencode($task));
			$history_disp = "<a href=\"$history_url\">".limitSize($task, 30)."</a>";
      	echo "<tr>\n".
           "<td ".($task_status[$task] != PLATFORM_PASSED ? 
                  "class=\"".$task_status[$task]."\"" : "").">".
                  "<span title=\"$task\">$history_disp</span></td>\n";
		} else {
      	echo "<tr>\n".
           "<td ".($task_status[$task] != PLATFORM_PASSED ? 
                  "class=\"".$task_status[$task]."\"" : "").">".
                  "<span title=\"$task\">".limitSize($task, 30)."</span></td>\n";
		}
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
      echo "</td>\n";
   } // FOREACH
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

