<?php   
// Configuration

include "Dashboard.php";
$dash = new Dashboard();
$dash->print_header("NMI Pool Status");

// Get the current platforms in the pool
$pool_platforms = $dash->condor_status();

include "CondorQ.php";
$condorq_build = new CondorQ("build", $pool_platforms);
$condorq_test = new CondorQ("test", $pool_platforms);

function make_cell($platform, $info, $type) {
  $queued = $info["depth"] - $info["running"];

  $color = "";
  if($queued == 0) {
    $color = "#00FFFF";
  }
  elseif($queued > 0 and $queued < 3) {
    $color = "#00FF00";
  }
  elseif($queued >= 3 and $queued < 6) {
    $color = "#FFFF00";
  }
  elseif($queued >= 6) {
    $color = "#FF0000";
  }
  
  return "<td align=\"center\" style=\"background-color:$color\">$type " . $info["html-queue"] . "</td>\n";
}
?>

<html>
<head>
<title>NMI - Build queue depths for core platforms</title>
<link rel="StyleSheet" href="condor.css" type="text/css">
</head>
<body>

<?php

echo "<h2>NMI queue depths:</h2>\n";

$roster_file = "/usr/local/nmi/etc/nmi_platform_list";
$handle = fopen($roster_file, "r");
$contents = fread($handle, filesize($roster_file));
fclose($handle);

$platforms = explode("\n", $contents);
sort($platforms);

echo "<table style='border-width:0px; border-style:none;'>\n";
echo "<tr style='border-width:0px'>\n";



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
  if(!preg_match("/\S/", $platform)) {
    continue;
  }

  $count += 1;

  $platform = preg_replace("/nmi:/", "", $platform);
  
  if(!preg_match("/\S/", $platform)) {
    continue;
  }

  // We want to make it obvious if a platform is not in the pool
  $style = "border-width:0px;border-style:none;align:center;";
  if(!$pool_platforms[$platform]) {
    $style .= "background-color:#B8002E";
  }

  echo "<td style=\"$style\"><table><tr><td colspan=2 style='text-align:center'>$platform</td></tr><tr>";
  echo make_cell($platform, $build_queues[$platform], "Builds");
  echo make_cell($platform, $test_queues[$platform], "Tests");
  echo "</tr></table></td>";

  if($count == 6) {
    $count = 0;
    echo "</tr><tr style='height:6px;border-width:0px'><td colspan=6 style='height:6px;border-width:0px;border-style:none;'></td></tr><tr style='border-width:0px'>";
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
</tr>
<tr>
  <td colspan="4" style="height:4px;size=2px;">&nbsp;</td>
</tr>
<tr>
  <td colspan="4" style="background-color:#B8002E">Platform missing from pool</td>
</tr>
</table>

</body>
</html>
