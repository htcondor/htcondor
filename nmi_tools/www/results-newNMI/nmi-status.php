<?php   
// Configuration

include "Dashboard.php";
$dash = new Dashboard();
$dash->print_header("NMI Pool Status");

?>

</head>
<body>

<?php

// Get the current platforms in the pool
$pool_platforms = $dash->condor_status();

include "CondorQ.php";
$condorq = new CondorQ($pool_platforms);

function make_cell($platform, $info) {
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
  
  return "<td align=\"center\" style=\"background-color:$color\">$platform<br>" . $info["html-queue"] . "</td>\n";
}

echo "<h2>NMI queue depths:</h2>\n";

$roster_file = "/usr/local/nmi/etc/nmi_platform_list";
$handle = fopen($roster_file, "r");
$contents = fread($handle, filesize($roster_file));
fclose($handle);

$platforms = array_filter(explode("\n", $contents));

// Strip nmi: from the front of each platform name
$func = function($platform) {
  return preg_replace("/nmi:/", "", $platform);
};
$platforms = array_map($func, $platforms);
sort($platforms);

echo "<table style='border-width:0px; border-style:none;'>\n";
echo "<tr style='border-width:0px'>\n";



// Get the queue depths.  We do this ahead of time so we can do this in 
// a single condor_q command
foreach ($platforms AS $platform) {
  $condorq->add_platform($platform);
}
$queues = $condorq->condor_q();

$count = 0;
foreach ($platforms AS $platform) {
  if(!preg_match("/\S/", $platform)) {
    continue;
  }

  $count += 1;

  if(!preg_match("/\S/", $platform)) {
    continue;
  }

  // We want to make it obvious if a platform is not in the pool
  $style = "border-width:0px;border-style:none;align:center;";
  if(!$pool_platforms[$platform]) {
    $style .= "background-color:#B8002E";
  }
  
  echo make_cell($platform, $queues[$platform], "Builds");

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

<p>
<h2>Slots</h2>
<table>
<tr>
  <th>Platform</th><th>Total</th><th>Avail.</th>
</tr>

<?php

// Display the number of available slots on each NMI platform

$free_slots = Array();
$total_slots = Array();
foreach ($platforms AS $platform) {
  $free_slots[$platform] = $queues[$platform]["slots"] - $queues[$platform]["running"];
  $total_slots[$platform] = $queues[$platform]["slots"];
}

arsort($free_slots, SORT_NUMERIC);
foreach (array_keys($free_slots) as $platform) {
  print "<tr><td style=\"text-align:left\">$platform</td><td>$total_slots[$platform]</td><td>$free_slots[$platform]</td></tr>\n";
}
?>

</table>
</body>
</html>
