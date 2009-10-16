<?php
   //
   // Configuration
   //
   
   $result_types = Array( "passed", "pending", "failed", "missing" );

   require_once "./load_config.inc";
   load_config();

   //
   // Grab GET/POST vars
   //
   $branch = $_REQUEST["branch"];
   $user   = $_REQUEST["user"];
   $rows   = ($_REQUEST["rows"] ? $_REQUEST["rows"] : 25);

   //
   // These are the platforms that never have tests in the nightly builds
   // This is a hack for now and there's no way to differentiate between different
   // branchs. But you know, life is funny that way -- you never really get what you want
   //
   $no_test_platforms = Array( "ppc_macos_10.4", "x86_macos_10.4" );
?>
<html>
<head>
   <Title>NMI - Condor <?= $branch; ?> Build/Test Results</TITLE>
   <LINK REL="StyleSheet" HREF="condor.css" TYPE="text/css">
</HEAD>
<body>
<h1><a href="./Run-condor.php" class="title">Condor Latest Build/Test Results</a> :: <?= $branch.(!empty($user) ? " ({$user})" : ""); ?></h1>

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
   } // FOREACH
 ?>
 </select>
 <input type="submit" value="Show Results">
 </form>

<table border=1 width="85%">
   <tr>
      <th>Branch</th>
      <th>Runid</th>
      <th>Last build</th>
      </tr>
      
<?php

   $db = mysql_connect(WEB_DB_HOST, DB_READER_USER, DB_READER_PASS) or die ("Could not connect : " . mysql_error());
   mysql_select_db(DB_NAME) or die("Could not select database");



# Create the set of all the runid's we're interested in
# This is the MAX(runid) i.e. latest run  
# for each (branch, type) tuple

$sql = "SELECT description,
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
//die($sql);
$results = mysql_query($sql) or die ("Query {$sql} failed : " . mysql_error());
while ($row = mysql_fetch_array($results)) {
  $runid      = $row["runid"];
  $desc       = $row["description"];
  $start      = $row["start"];
  $run_result = $row["result"];
  $pin 		  = $row["archive_results_until"];
	$archived = $row["archived"];

   //
   // HTML Table Row
   //
   
   
   echo <<<EOF
   <tr>
      <td>{$desc}</td>
      <td align="center">{$runid}</td>
      <td align="center">{$start}</td>
EOF;

   
   echo "</tr>";
} // WHILE
echo "</table>";
echo "</center>";

mysql_free_result($results);
mysql_close($db);

?>
</body>
</html>
