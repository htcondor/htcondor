<?php   

define("DETAIL_URL", "./run-details.php?runid=%s");

include "Dashboard.php";
$dash = new Dashboard();
$dash->print_header("Condor Build and Test Dashboard");
$dash->connect_to_db();

# get args
$runid = (int)$_REQUEST["runid"];
$test = $_REQUEST["test"];

?>

<script type='text/javascript' src='jquery-1.6.2.min.js'></script>

<script type="text/javascript">
  var toggle = 1;

  $(document).ready(function(){
      $("#toggle").click(function(){
	  if(toggle == 1) {
	    $(".time").show();
	    $(".status").hide();
	    $("#toggle").text("Show task status");
	    toggle = 0;
	  }
	  else {
	    $(".time").hide();
	    $(".status").show();
	    $("#toggle").text("Show task times");
	    toggle = 1;	    
	  }
	});
  });
</script>

<style type="text/css">
<!--
div.status {
  
}
div.time {
  display:none;
}
p.toggle {
  cursor: pointer;
}
-->
</style>

</head>
<body>


<?php

$sql = "
SELECT
  description
FROM
  Run
WHERE
  runid='$runid'";

$results = $dash->db_query($sql);
$branch = $results[0]["description"];

echo "<h2>Historical data for test $test in branch $branch</h2>\n";

$platforms = $dash->get_platform_list_for_runid($runid);

//
// Get build task info   
//
$sql = "SELECT
          runid,
          platform,
          result,
          host,
          start,
          TIME_TO_SEC(TIMEDIFF(Finish, Start)) as length
       FROM
          Task
       WHERE 
          name = '$test'
       ORDER BY
          start DESC
       LIMIT 1000";

$results = $dash->db_query($sql);

$history = Array();
$runids = Array();
foreach ($results as $row) {
  array_push($runids, $row["runid"]);
  if(!array_key_exists($row["runid"], $history)) {
    $history[$row["runid"]] = Array();
  }

  $history[$row["runid"]][$row["platform"]] = Array("result" => $row["result"],
						    "length" => $row["length"],
						    "start"  => $row["start"],
						    "host"   => $row["host"]);
}

$runids_list = implode(",", $runids);
$sql = "
SELECT
  runid, input_runid
FROM
  Method_nmi
WHERE
  runid in ($runids_list) AND input_runid IS NOT NULL";

$results = $dash->db_query($sql);
$builds = Array();
foreach ($results as $row) {
  if(!array_key_exists($row["input_runid"], $builds)) {
    $builds[$row["input_runid"]] = Array($row["runid"]);
  }
  else {
    array_push($builds[$row["input_runid"]], $row["runid"]);
  }
}

print "<p><a id='toggle'>Show task times</a>\n";

print "<table border='0' cellspacing='0'>\n";
print "<tr>\n";
print "   <th>Run IDs</th>\n";


foreach ($platforms AS $platform) {
  $display = preg_replace("/nmi:/", "", $platform);
   
  if(preg_match("/^x86_64_/", $display)) {
    $display = preg_replace("/x86_64_/", "x86_64<br>", $display);
  }
  elseif(preg_match("/ia64_/", $display)) {
    $display = preg_replace("/ia64_/", "x86<br>", $display);
  }
  else {
    $display = preg_replace("/x86_/", "x86<br>", $display);
  }

  $display = "<font style='font-size:75%'>$display</font>";

  print "<td align='center'>$display</td>\n";
}
print "</tr>\n";

// 
// Show build results
//
print $history[0];
krsort($builds);
foreach ($builds as $build_runid => $test_runids) {
  print "<tr>\n";
  $link = sprintf(DETAIL_URL, $build_runid);
  print "  <td><a href='$link'>$build_runid</a></td>\n";

  foreach ($platforms as $platform) {
    $found_task = 0;
    foreach ($test_runids as $test_runid) {
      if(array_key_exists($platform, $history[$test_runid])) {
	  $class = map_result($history[$test_runid][$platform]["result"]);

	  $contents = "<div class='status'>" . $history[$test_runid][$platform]["result"] . "</div>";
	  $contents .= "<div class='time'>" . sec_to_min($history[$test_runid][$platform]["length"]) . "</div>";
	  print "  <td class=\"$class\">$contents</td>\n";
	  $found_task = 1;
	  break;
      }
    }

    if($found_task == 0) {
      print "<td>&nbsp;</td>\n";
    }
  }
  print "</tr>\n";
}

print "</table>";

function limitSize($str, $cnt) {
  if (strlen($str) > $cnt) {
    $str = substr($str, 0, $cnt - 3)."...";
  }
  return ($str);
}

function sec_to_min($sec) {
  $min = floor($sec / 60);
  $sec = $sec % 60;
  if($sec < 10) {
    $sec = "0$sec";
  }
  return "$min:$sec";
}

function map_result($result) {
  if(is_null($result)) {
    return "pending";
  }
  elseif($result == 0) {
    return "passed";
  }
  else {
    return "failed";
  }
}


?>
</body>
</html>
