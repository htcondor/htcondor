<?php
   //
   // Configuration
   //
   define("DETAIL_URL", "./Run-condor-details.php?runid=%s&type=%s&user=%s");
   define("CROSS_DETAIL_URL", "./Run-condor-pre-details.php?runid=%s&type=%s&user=%s");
   define("GITSHA1","http://bonsai.cs.wisc.edu/gitweb/gitweb.cgi?p=CONDOR_SRC.git;a=commit;h=%s");
   
   $result_types = Array( "passed", "pending", "failed", "missing" );

   require_once "./load_config.inc";
   load_config();

   //
   // Grab GET/POST vars
   //
   $branch = $_REQUEST["branch"];
   $user   = $_REQUEST["user"];
   $rows   = ($_REQUEST["rows"] ? $_REQUEST["rows"] : 25);

   //
   // These are the platforms that never have tests in the nightly builds
   // This is a hack for now and there's no way to differentiate between different
   // branchs. But you know, life is funny that way -- you never really get what you want
   //
   $no_test_platforms = Array( "ppc_macos_10.4", "x86_macos_10.4" );
?>
<html>
<head>
   <Title>NMI - Condor <?= $branch; ?> Build/Test Results</TITLE>
   <LINK REL="StyleSheet" HREF="condor.css" TYPE="text/css">
</HEAD>
<body>
<h1><a href="./Run-condor.php" class="title">Condor Latest Build/Test Results</a> :: <?= $branch.(!empty($user) ? " ({$user})" : ""); ?></h1>

<form method="get" action="<?= $_SERVER{PHP_SELF}; ?>">
<input type="hidden" name="branch" value="<?= $branch; ?>">
<input type="hidden" name="user" value="<?= $user; ?>">
<font size="+1"><b>Rows: </b></font>&nbsp;
<select name="rows">
<?php
   $arr = Array("10", "25", "50", "100", "200", "500");
   foreach ($arr AS $val) {
      echo "<option".
           ($val == $rows ? " SELECTED" : "").
           ">$val</option>\n";
   } // FOREACH
 ?>
 </select>
 <input type="submit" value="Show Results">
 </form>

<table border=1 width="85%">
   <tr>
      <th>Branch</th>
      <th>Runid</th>
      <th>Last build</th>
      <th>Build Results</th>
      <th>Test Results</th>  
      <th>Cross Test Results</th>  
      </tr>
      
<?php

   $db = mysql_connect(WEB_DB_HOST, DB_READER_USER, DB_READER_PASS) or die ("Could not connect : " . mysql_error());
   mysql_select_db(DB_NAME) or die("Could not select database");



# Create the set of all the runid's we're interested in
# This is the MAX(runid) i.e. latest run  
# for each (branch, type) tuple

$sql = "SELECT description,
			   project_version,
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
//die($sql);
$results = mysql_query($sql) or die ("Query {$sql} failed : " . mysql_error());
while ($row = mysql_fetch_array($results)) {
  $runid      = $row["runid"];
  $desc       = $row["description"];
  $projectversion = $row["project_version"];
  $start      = $row["start"];
  $run_result = $row["result"];
  $pin 		  = $row["archive_results_until"];
	$archived = $row["archived"];

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
          //"       Run.project_version = Run.component_version AND ".
		  "				Run.user = '$user' AND ".
          "       Task.runid = Run.runid AND ".
          "       Task.platform != 'local' AND".
			 "       ((Run.project_version = Run.component_version)  OR (Run.component_version = 'native' ))".
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
   
   // --------------------------------
   // CROSS TESTS
   // --------------------------------
   $sql = "SELECT SUM(IF(Task.result = 0, 1, 0)) AS passed, ".
          "       SUM(IF(Task.result != 0, 1, 0)) AS failed, ".
          "       SUM(IF(Task.result IS NULL, 1, 0)) AS pending ".
          "  FROM Task, Run, Method_nmi ".
          " WHERE Method_nmi.input_runid = {$runid} AND ".
          "       Run.runid = Method_nmi.runid AND ".
					"				Run.user = '$user' AND ".
          "       Task.runid = Run.runid AND ".
          "       Task.platform != 'local' AND".
			    "       project_version != component_version AND ".
				         "        component_version != 'native' ".

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

   //
   // HTML Table Row
   //
   
   //
   // If there's nothing at all, just skip
   // Not sure if I want to do this...
   //
   //if (!count($data["build"]) && !count($data["test"])) continue;
	if( !(is_null($pin))) {
		$pinstr = "pin ". "$pin";
	} else {
		$pinstr = "";
	}

	if( $archived == '0' ) {
		$archivedstr = "$runid". "<br><font size=\"-1\"> D </font>";
	} else {
		$archivedstr = "$runid";
	}
	$sha1_url = sprintf(GITSHA1, $projectversion);
   
   echo <<<EOF
   <tr>
      <td>{$desc}<br><font size="-1"><a href="{$sha1_url}">{$projectversion}</a><font><br><font size="-2">$pinstr<font></td>
      <td align="center">{$archivedstr}</td>
      <td align="center">{$start}</td>
EOF;

   foreach (Array("build", "test", "crosstest") AS $type) {
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

		if($type == "crosstest") {
      	$detail_url = sprintf(CROSS_DETAIL_URL, $runid, $type, $user);
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
} // WHILE
echo "</table>";
echo "</center>";

mysql_free_result($results);
mysql_close($db);

?>
</body>
</html>
