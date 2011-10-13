<?php
include "dashboard.inc";

include "Dashboard.php";
$dash = new Dashboard();
$dash->print_header("Condor Build and Test Dashboard");
$dash->connect_to_db();
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
  user != '$condor_user'
GROUP BY 
  branch,
  user,
  run_type 
ORDER BY 
  runid desc
";

$results = $dash->db_query($query);
$oneoff_buf = Array();
foreach ($results as $row) {
  $runid      = $row["runid"];
  $branch     = $row["branch"];
  $user       = $row["user"];
  $run_result = $row["result"];

  if(!preg_match("/Continuous/", $branch)) {
    // The branch might not be unique for one-off builds so we need to make a more unique key
    $key = "$branch ($user)";
    $oneoff_buf[$key] = Array();
    $oneoff_buf[$key]["branch"]  = $branch;
    $oneoff_buf[$key]["user"]    = $user;
    $oneoff_buf[$key]["start"]   = $row["start"];
    $oneoff_buf[$key]["results"] = get_results($dash, &$oneoff_buf[$key], $runid, $user, $row["result"]);
  }
}

// Now create the HTML tables.
echo "<div id='main'>\n";

  
echo "<table border=1>\n";
echo "<tr><th>Branch</th><th>Runid</th><th>Submitted</th><th>User</th><th>Build Results</th><th>Test Results</th><th>Cross Test Results</th>\n";
echo "</tr>\n";

foreach (array_keys($oneoff_buf) as $key) {
  $info = $oneoff_buf[$key];
  $branch_url = sprintf(BRANCH_URL, urlencode($info["branch"]), $info["user"]);
  echo "<tr>\n";
  echo "  <td><a href='$branch_url'>" . $info["branch"] . "</a><br><font size='-2'>" . $info["pin"] . "</font></td>\n";
  echo "  <td align='center'>" . $info["runid"] . "</td>\n";
  echo "  <td align='center'>" . $info["start"] . "</td>\n";
  echo "  <td align='center'>" . $info["user"] . "</td>\n";
  echo $info["results"];
  echo "</tr>\n";
}
echo "</table>\n";

echo "</div>\n";
echo "<div id='footer'>&nbsp;</div>\n";
echo "</div>\n";






?>
</body>
</html>
