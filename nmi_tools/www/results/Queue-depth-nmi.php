<?php   
// Configuration

require_once "./load_config.inc";
load_config();

include "CondorQ.php";
$condorq_build = &new CondorQ("build");
$condorq_test = &new CondorQ("test");

function make_cell($platform, $depth, $queue, $type) {
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
  
  return "<td align=\"center\" style=\"background-color:$color\">$type $queue</td>\n";
}
?>

<html>
<head>
<title>NMI - Build queue depths for core platforms</title>
<LINK REL="StyleSheet" HREF="condor.css" TYPE="text/css">
</head>
<body>

<?php

echo "<h2>NMI queue depths:</h2>\n";

$roster_file = "/nmi/etc/nmi_platform_list";
$handle = fopen($roster_file, "r");
$contents = fread($handle, filesize($roster_file));
fclose($handle);

$platforms = explode("\n", $contents);

echo "<table border='0' cellspacing='0'>\n";
echo "<tr>\n";



// Get the queue depths.  We do this ahead of time so we can do this in 
// a single condor_q command
foreach ($platforms AS $platform) {
  if(!preg_match("/\S/", $platform)) {
    continue;
  }

  $condorq_build->add_platform($platform);
  $condorq_test->add_platform($platform);
}
$build_queues = $condorq_build->condor_q();
$test_queues = $condorq_test->condor_q();

$count = 0;
foreach ($platforms AS $platform) {
  $count += 1;

  $platform = preg_replace("/nmi:/", "", $platform);
  
  if(!preg_match("/\S/", $platform)) {
    continue;
  }

  $build_depth = $build_queues[$platform][0];
  $build_queue = $build_queues[$platform][1];

  $test_depth = $test_queues[$platform][0];
  $test_queue = $test_queues[$platform][1];

  echo "<td><table><tr><td colspan=2>$platform</td></tr><tr>";
  echo make_cell($platform, $build_depth, $build_queue, "Builds");
  echo make_cell($platform, $test_depth, $test_queue, "Tests");
  echo "</tr></table></td>";

  if($count == 6) {
    $count = 0;
    echo "</tr><tr style='height:6px;'><td colspan=6 style='height:6px;'></td></tr><tr>";
  }
}

echo "</table>\n";

?>

<p>Legend:
<table>
<tr>
<td style="background-color:#00FFFF">Depth 0</td>
<td style="background-color:#00FF00">Depth 1-2</td>
<td style="background-color:#FFFF00">Depth 3-5</td>
<td style="background-color:#FF0000">Depth 6+</td>

</body>
</html>
