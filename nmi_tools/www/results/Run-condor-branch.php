<?php
//
// Configuration
//

$result_types = Array( "passed", "pending", "failed", "missing" );

require_once "./load_config.inc";
load_config();

include "dashboard.inc";

// Cache the committers info in a persistent file on disk
include "Committers.php";
$committers = &new Committers();

//
// Grab GET/POST vars
//
$branch = $_REQUEST["branch"];
$user   = $_REQUEST["user"];
$rows   = array_key_exists("rows", $_REQUEST) ? $_REQUEST["rows"] : 25;

?>
<html>
<head>
   <Title>NMI - Condor <?= $branch; ?> Build/Test Results</TITLE>
   <LINK REL="StyleSheet" HREF="condor.css" TYPE="text/css">
</HEAD>
<body>
<h1><a href="./Run-condor.php" class="title">Condor Latest Build/Test Results</a> :: <?= $branch.(!empty($user) ? " ($user)" : ""); ?></h1>

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
}
?>
</select>
<input type="submit" value="Show Results">
</form>

<table border=1 width="85%">
  <tr>
    <th>Branch</th>
    <th>Build<br>RunID</th>
    <th>Submitted</th>
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

$results = mysql_query($sql) or die ("Query $sql failed : " . mysql_error());

$info = Array();
while ($row = mysql_fetch_array($results)) {
  $tmp = Array();
  
  $tmp["desc"]      = $row["description"];
  $tmp["sha1"]      = $row["project_version"];
  $tmp["start"]     = $row["start"];
  $tmp["pin"]       = $row["archive_results_until"];
  $tmp["archived"]  = $row["archived"];
  $tmp["results"]   = get_results(&$tmp, $row["runid"], $user, $row["result"]);

  array_push($info, $tmp);
}

for ($i=0; $i < sizeof($info); $i++) {
  $run = $info[$i];

  $diffstr = "";
  if( ( $i+1 < sizeof($info) ) && $run["sha1"] != $info[$i+1]["sha1"]) {
    $tmp_url = sprintf(GITSHA1SHORT, $run["sha1"], $info[$i+1]["sha1"]);

    $authors = $committers->get_committers($info[$i+1]["sha1"], $run["sha1"]);
    $names = array_keys($authors);
    sort($names);
    $list = "<table><tr><th>Committer</th><th>Count</th></tr>\n";
    foreach ($names AS $name) {
      $list .= "<tr><td>" . htmlspecialchars($name) . "</td><td style='text-align:center'>" . htmlspecialchars($authors[$name]["count"]) . "</td></tr>";
    }
    $list .= "</table>";

    $diffstr = "| <span class=\"link\" style=\"font-size:100%\"><a href=\"$tmp_url\" style=\"\">Log from previous<span style=\"width:300px;text-decoration:none;\">$list</span></a></span>";
  }
  $full_log_url = sprintf(GITSHA1, $run["sha1"]);  

  echo "<tr>\n";
  echo "  <td>" . $run["desc"] . "<br><font size='-1'>" . $run["sha1"] . "<br><a href='$full_log_url'>Commit info</a> $diffstr</font>\n";
  echo "      <br><font size='-2'>" . $run["pin"] . "</font></td>\n";
  echo "  <td align='center'>" . $run["runid"] . "</td>\n";
  echo "  <td align='center'>" . $run["start"] . "</td>\n";
  echo $run["results"];
  
  echo "</tr>\n";
}
   
echo "</table>";
echo "</center>";

mysql_free_result($results);
mysql_close($db);

?>
</body>
</html>
