<?php   
// Configuration
define("CONDOR_USER", "cndrauto");

require_once "./load_config.inc";
load_config();

# get args
$branches = array("trunk", "NMI Ports - trunk");
$user = "cndrauto";

if($_REQUEST["type"] != "") {
  $type = $_REQUEST["type"];
}
else {
  $type = "build";
}

include "CondorQ.php";
$condorq = &new CondorQ($type);
?>

<html>
<head>
<title>NMI - Build queue depths for core platforms</title>
<LINK REL="StyleSheet" HREF="condor.css" TYPE="text/css">
</head>
<body>

<?php

$db = mysql_connect(WEB_DB_HOST, DB_READER_USER, DB_READER_PASS) or die ("Could not connect : " . mysql_error());
mysql_select_db(DB_NAME) or die("Could not select database");

include "dashboard.inc";

// Create the sidebar
echo "<div id='wrap'>";
make_sidebar();

// Now create the HTML tables.
echo "<div id='main'>\n";

echo "<h2>NMI build queue depths:</h2>\n";
echo "<p>This page contains depth information for jobs of type \"build\" only</p>\n";


foreach ($branches as $branch) {
  //
  // First get a runid of a recent build.
  //
  $sql = "SELECT runid
          FROM Run 
          WHERE component='condor' AND 
                project='condor' AND
                run_type='$type' AND
                user = '$user' AND
                description LIKE '$branch%'
          ORDER BY runid DESC
          LIMIT 1";

  $result = mysql_query($sql) or die ("Query {$sql} failed : " . mysql_error());
  while ($row = mysql_fetch_array($result)) {
    $runid = $row["runid"];
    //echo "Using RunID: $runid";
  }
  mysql_free_result($result);
  
  //
  // Then get the platform list using that runid
  //
  $sql = "SELECT DISTINCT(platform) AS platform
          FROM Run, Task
          WHERE Task.runid = $runid
          AND Task.runid = Run.runid
          AND Run.user = '$user' 
          AND Task.platform != 'local' ";

  $result = mysql_query($sql) or die ("Query $sql failed : " . mysql_error());
  $platforms = Array();
  while ($row = mysql_fetch_array($result)) {
    array_push($platforms, $row["platform"]);
  }
  mysql_free_result($result);

  echo "<h3>Branch - $branch</h3>\n";
  echo "<table border='0' cellspacing='0'>\n";
  echo "<tr>\n";

  // Get the queue depths.  We do this ahead of time so we can do this in 
  // a single condor_q command
  foreach ($platforms AS $platform) {
    $condorq->add_platform($platform);
  }
  $queues = $condorq->condor_q();

  // show link to run directory for each platform
  foreach ($platforms AS $platform) {
    $display = preg_replace("/nmi:/", "", $platform);

    $depth = $queues[$display][0];
    $queue_depth = $queues[$display][1];
    
    $color = "";
    if($depth == 0) {
      $color = "#00FFFF";
    }
    elseif($depth > 0 and $depth < 3) {
      $color = "#00FF00";
    }
    elseif($depth >= 3 and $depth < 6) {
      $color = "#FFFF00";
    }
    elseif($depth >= 6) {
      $color = "#FF0000";
    }
    
    echo "<td align=\"center\" style=\"background-color:$color\">$display $queue_depth</td>\n";
  }

  echo "</table>\n";
}
mysql_close($db);


?>

<p>Legend:
<table>
<tr>
<td style="background-color:#00FFFF">Depth 0</td>
<td style="background-color:#00FF00">Depth 1-2</td>
<td style="background-color:#FFFF00">Depth 3-5</td>
<td style="background-color:#FF0000">Depth 6+</td>

</div>
<div id='footer'>&nbsp;</div>
</div>

</body>
</html>
