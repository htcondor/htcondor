<?php   
   //
   // Configuration
   //
   define("BRANCH_URL", "./Run-condor-branch.php?branch=%s&user=%s");
   define("CROSS_DETAIL_URL", "./Run-condor-cross-details.php?runid=%s&platform=%s&user=%s");
   define("CONDOR_USER", "cndrauto");
   
   $result_types = Array( "passed", "pending", "failed" );

   require_once "./load_config.inc";
   load_config();

   # get args
   $type = $_REQUEST["type"];
   $runid = (int)$_REQUEST["runid"];
   $user = $_REQUEST["user"];
   $build_id = $runid;
   
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

<table border=1 width="85%">
   <tr>
      <th>Src Binaries</th>
      <th>Cross Test Results</th>
      </tr>

<?php

   $db = mysql_connect(WEB_DB_HOST, DB_READER_USER, DB_READER_PASS) or die ("Could not connect : " . mysql_error());
   mysql_select_db(DB_NAME) or die("Could not select database");

   include "last.inc";

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
        ":: ".ucfirst($type)."s for Build ID $runid (".date("m/d/Y", $start).")</h1>\n";

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

   foreach ($platforms as $source) {
		//echo "<H3>Native build: $source</H3>";
		// make a list of platforms to display together
		// with the native platform before the cross platform
		// tests.
		$xdetail_set = Array();
		$nativerunid;
	
	// --------------------------------
	// CROSS TESTS RESULTS
   // --------------------------------
   $sql = "SELECT SUM(IF(Task.result = 0, 1, 0)) AS passed, ".
          "       SUM(IF(Task.result != 0, 1, 0)) AS failed, ".
          "       SUM(IF(Task.result IS NULL, 1, 0)) AS pending ".
          "  FROM Task, Run, Method_nmi ".
          " WHERE Method_nmi.input_runid = {$runid} AND ".
          "       Run.runid = Method_nmi.runid AND ".
          "       Run.user = '$user'  AND ".
          "       Task.runid = Run.runid AND ".
          "       Task.platform != 'local' AND ".
          "       project_version != component_version AND ".
          "       component_version = '$source' ".
          " GROUP BY Task.platform ";
   $result2 = mysql_query($sql)
                  or die ("Query {$sql} failed : " . mysql_error());
   $data["crosstest"] = Array();
   while ($platforms = mysql_fetch_array($result2)) {
      if (!empty($platforms["failed"])) {
         $data["crosstest"]["failed"]++;
         $data["crosstest"]["total"]++;
      } elseif (!empty($platforms["pending"])) {
         $data["crosstest"]["pending"]++;
         $data["crosstest"]["total"]++;
      } elseif (!empty($platforms["passed"])) {
         $data["crosstest"]["passed"]++;
         $data["crosstest"]["total"]++;
      }
   } // WHILE
   mysql_free_result($result2);

	foreach (Array("crosstest") AS $type) {
      $cur = $data[$type];
      $status = ($cur["failed"] ? "FAILED" :
                ($cur["pending"] ? "PENDING" : "PASSED"));
      $color = $status;


	if($type == "crosstest") {
            $detail_url = sprintf(CROSS_DETAIL_URL, $runid, $source, $user);
        } else {
            $detail_url = sprintf(DETAIL_URL, $runid, $type, $user);
        }

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
   <tr>
      <td>{$source}</td>
EOF;
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
            //echo "<td>&nbsp;</td>\n";
         }
      //
      // Show Summary
      //
      } else {

echo <<<EOF
   <tr>
      <td>{$source}</td>
EOF;
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

   }
   mysql_free_result($result);
   
?>

<?php
?>
</body>
</html>

