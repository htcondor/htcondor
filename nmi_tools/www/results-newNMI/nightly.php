<?php

include "dashboard.inc";

include "Dashboard.php";
$dash = new Dashboard();
$dash->print_header("Condor Build and Test Dashboard");
$dash->connect_to_db();

$continuous_blacklist = Array("x86_64_rhap_5.3-updated");
?>

</head>
<body>

<?php

$condor_user = $dash->get_condor_user();

$query = "
SELECT 
  LEFT(description,
       (IF(LOCATE('branch-',description),
         LOCATE('branch-',description)+5,
         (IF(LOCATE('trunk-',description),
            LOCATE('trunk-',description)+4,
            CHAR_LENGTH(description)))))) AS branch,
  MAX(convert_tz(start, 'GMT', 'US/Central')) AS start,
  unix_timestamp(MAX(convert_tz(start, 'GMT', 'US/Central'))) AS start_epoch,
  run_type, 
  max(runid) as runid,
  user,
  archive_results_until,
  result
FROM 
  Run 
WHERE 
  component='condor' AND 
  project='condor' AND
  run_type='build' AND
  description != '' AND 
  DATE_SUB(CURDATE(), INTERVAL 10 DAY) <= start AND
  user='$condor_user'
GROUP BY 
  branch,
  user,
  run_type 
ORDER BY 
  runid desc
";

$results = $dash->db_query($query);
$condorauto_buf = Array();
foreach ($results as $row) {
  $runid      = $row["runid"];
  $branch     = $row["branch"];
  $user       = $row["user"];
  $run_result = $row["result"];

  if(!preg_match("/Continuous/", $branch)) {
    $condorauto_buf[$branch] = Array();
    $condorauto_buf[$branch]["user"] = $user;
    $condorauto_buf[$branch]["start"] = $row["start"];
    $condorauto_buf[$branch]["start_epoch"] = $row["start_epoch"];
    $condorauto_buf[$branch]["results"] = get_results($dash, &$condorauto_buf[$branch], $runid, $user, $row["result"]);
  }
}

// Now create the HTML tables.
echo "<div id='main'>\n";

//
// 2) condorauto builds
//
echo "<table border=1>\n";
echo "<tr><th>Branch</th><th>Build RunID</th><th>Submitted</th><th>Build Results</th><th>Test Results</th><th>Cross Test Results</th>\n";
echo "</tr>\n";

$branches = array_keys($condorauto_buf);
usort($branches, "condorauto_sort");

foreach ($branches as $branch) {
  $info = $condorauto_buf[$branch];
  $branch_url = sprintf(BRANCH_URL, urlencode($branch), $info["user"]);
  
  // Print a "warning" if this branch has not been submitted for >1 day.  This makes it easier to spot old
  // builds from the dashboard.
  $warning = "";
  $diff = time() - $info["start_epoch"];
  if($diff > 60*60*24) {
    $days = floor($diff / (60*60*24));
    $warning = "<p style='font-size:75%; color:red;'>$days+ days old</p>";
  }
  
  $style = "";
  if(preg_match("/^NMI/", $branch)) {
    $style = "style='border-bottom-width:4px;'";
  }
  else {
    $style = "style='border-top-width:4px;'";
  }
  
  echo "<tr>\n";
  echo "  <td $style><a href='$branch_url'>$branch</a><br><font size='-2'>" . $info["pin"] . "</font></td>\n";
  echo "  <td $style align='center'>" . $info["runid"] . "</td>\n";
  echo "  <td $style align='center'>" . $info["start"] . $warning . "</td>\n";
  echo $info["results"];
  echo "</tr>\n";
}
echo "</table>\n";

echo "</div>\n";
echo "<div id='footer'>&nbsp;</div>\n";
echo "</div>\n";


// Define a custom sort that puts trunk ahead of other builds, and NMI immediately
// after the matching core build.
function condorauto_sort($a, $b) {
  $a_is_nmi = preg_match("/^NMI/", $a);
  $b_is_nmi = preg_match("/^NMI/", $b);

  $a = preg_replace("/^NMI Ports - /", "", $a);
  $b = preg_replace("/^NMI Ports - /", "", $b);

  if($a == $b) {
    if($a_is_nmi and !$b_is_nmi) {
      return 1;
    }
    elseif($b_is_nmi and !$a_is_nmi) {
      return -1;
    }
  }

  if(preg_match("/trunk/", $a)) {
    return -1;
  }
  elseif(preg_match("/trunk/", $b)) {
    return 1;
  }

  return strcasecmp($b, $a);
}

?>
</body>
</html>
