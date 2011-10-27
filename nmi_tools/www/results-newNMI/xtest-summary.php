<?php   
//
// Configuration
//
define("BRANCH_URL", "./branch.php?branch=%s&user=%s");
define("CROSS_DETAIL_URL", "./xtest-details.php?runid=%s&platform=%s&type=%s&user=%s");

$result_types = Array( "passed", "pending", "failed" );

include "dashboard.inc";

include "Dashboard.php";
$dash = new Dashboard();
$dash->print_header("Condor Build and Test Dashboard");
$dash->connect_to_db();

# get args
$type = $_REQUEST["type"];
$runid = (int)$_REQUEST["runid"];
$user = $_REQUEST["user"];
$build_id = $runid;

define('PLATFORM_PENDING', 'pending');
define('PLATFORM_FAILED',  'failed');
define('PLATFORM_PASSED',  'passed');
?>

</head>
<body>

<table border=1 width="85%">
   <tr>
      <th>Src Binaries</th>
      <th>Cross Test Results</th>
      </tr>

<?php

$sql = "SELECT host, gid, UNIX_TIMESTAMP(start) AS start ".
          "  FROM Run ".
          " WHERE Run.runid = $build_id".
          "   AND Run.user = '$user'";

$results = $dash->db_query($sql);
$host  = $results[0]["host"];
$gid   = $results[0]["gid"];
$start = $results[0]["start"];

echo "<h2>" . ucfirst($type) . "s for Build ID $runid (" . date("m/d/Y", $start) . ")</h2>\n";

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
$results = $dash->db_query($sql);
$platforms = Array();
foreach ($results as $row) {
  $tmpp = $platforms[] = $row["platform"];
  //echo "<H3>$tmpp</H3>";
}

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
  $results = $dash->db_query($sql);
  $data["crosstest"] = Array();
  foreach ($results as $platforms) {
    if (!empty($platforms["failed"])) {
      $data["crosstest"]["failed"]++;
      $data["crosstest"]["total"]++;
    }
    elseif (!empty($platforms["pending"])) {
      $data["crosstest"]["pending"]++;
      $data["crosstest"]["total"]++;
    }
    elseif (!empty($platforms["passed"])) {
      $data["crosstest"]["passed"]++;
      $data["crosstest"]["total"]++;
    }
  } // WHILE

  foreach (Array("crosstest") AS $type) {
    $cur = $data[$type];
    $status = ($cur["failed"] ? "FAILED" : ($cur["pending"] ? "PENDING" : "PASSED"));
    $color = $status;

    if($type == "crosstest") {
      $detail_url = sprintf(CROSS_DETAIL_URL, $runid, $source, $type, $user);
    }
    else {
      $detail_url = sprintf(DETAIL_URL, $runid);
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
        print "<tr>\n";
        print "<td>$source</td>\n";

        print "<td class='$status' align='center'>\n";
        print "<table cellpadding='1' cellspacing='0' width='100%' class='details'>\n";
        print "<tr>\n";
        print "<td colspan='2' class='detail_url'><a href='$detail_url'>$status</a></td>\n";
        print "</tr>\n";
        print "</table>\n";
        print "</td>\n";

      }
      else {
        //echo "<td>&nbsp;</td>\n";
      }
      //
      // Show Summary
      //
      }
    else {

      print "<tr>\n";
      print "<td>$source</td>\n";

      print "<td class='$color' align='center' valign='top'>\n";
      print "<table cellpadding='1' cellspacing='0' width='100%' class='details'>\n";
      print "<tr>\n";
      print "<td colspan='2' class='detail_url'><a href='{$detail_url}'>$status</a></td>\n";
      print "</tr>\n";

      //
      // Show the different status tallies for platforms
      //
      foreach ($result_types AS $key) {
        if ($key == "missing" && empty($cur[$key])) continue;
        if ($key == "missing") {
          $prefix = "<B>";
          $postfix = "</B>";
        }
        else {
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
   
?>

</body>
</html>

