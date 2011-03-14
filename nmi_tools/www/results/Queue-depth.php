<?php   
   // Configuration
   define("CONDOR_USER", "cndrauto");
   
   require_once "./load_config.inc";
   load_config();

   # get args
   $branch = "trunk";
   $user = "cndrauto";
   $type = "build";
?>
<html>
<head>
<title>NMI - Queue depths for core platforms</title>
<LINK REL="StyleSheet" HREF="condor.css" TYPE="text/css">
</head>
<body>

<?php

$db = mysql_connect(WEB_DB_HOST, DB_READER_USER, DB_READER_PASS) or die ("Could not connect : " . mysql_error());
mysql_select_db(DB_NAME) or die("Could not select database");

include "last.inc";

//
// First get a runid of a recent build.
//
$sql = "SELECT runid
          FROM Run 
         WHERE component='condor' AND 
               project='condor' AND
               run_type='build' AND
               user = '$user' AND
               description LIKE '".$branch."%'".
       " ORDER BY runid DESC ".
       " LIMIT 1";

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
//echo "Found platforms: " . implode(", ", $platforms);
?>
<h2>NMI queue depths:</h2>   
<table border="0" cellspacing="0" >
<tr>

<?php
// show link to run directory for each platform
foreach ($platforms AS $platform) {
  // We will remove 'nmi:' from the front of the platform and also split it 
  // onto two separate lines because the length of the header determines the
  // width of the resulting table column.
  $display = preg_replace("/nmi:/", "", $platform);
  $display = preg_replace("/_/", "_ ", $display, 1);

  $ret = get_queue_for_nmi_platform($platform, $type);
  $depth = $ret[0];
  $queue_depth = $ret[1];
  
  $color = "";
  if($depth == 0) {
    $color = "#0000FF";
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
?>

<?php
echo "</table>";

mysql_close($db);
?>

<p>Legend:
<table>
<tr>
<td style="background-color:#0000FF">Depth 0</td>
<td style="background-color:#00FF00">Depth 1-2</td>
<td style="background-color:#FFFF00">Depth 3-5</td>
<td style="background-color:#FF0000">Depth 6+</td>

</body>
</html>
