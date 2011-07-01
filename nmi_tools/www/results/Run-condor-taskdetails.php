<html>
<body>
<title>NWO task results</title>

<?php
include "last.inc";
require_once "./load_config.inc";                                     
load_config(); 

$platform    = $_REQUEST["platform"];
$task        = $_REQUEST["task"];
$description = $_REQUEST["description"];
$runid       = $_REQUEST["runid"];
$type        = $_REQUEST["type"];

# Some CSS and Javascript
echo '
<style type="text/css">
<!--
pre {
   margin-left: 1em;
   margin-right: 1em;
   padding: 0.5em;
   width: 90%;
   background-color: #f0f0f0;
   border: 1px solid black;
   white-space: pre-wrap;       /* css-3 */
   white-space: -moz-pre-wrap !important; /* Mozilla + Firefox */
   white-space: -pre-wrap;      /* Opera 4-6 */
   white-space: -o-pre-wrap;    /* Opera 7 */
   word-wrap: break-word;       /* Internet Explorer 5.5+ */
}

-->
</style>

<script type="text/javascript" language="JavaScript">
<!--
function swap(d) {
    if(document.getElementById(d).style.display == "none") { 
        document.getElementById(d).style.display = "block"; 
    }
    else { 
        document.getElementById(d).style.display = "none"; 
    }
}
-->
</script>
';

echo "<P>";
echo "<B>Tag: </b>$description<BR>";
echo "<B>Task: </b>$task<br>";
echo "<B>Platform: </b>$platform<br>";
echo "</P>";

$query = "
SELECT
  Task.name,
  convert_tz(Task.start, 'GMT', 'US/Central') as start,
  convert_tz(Task.finish, 'GMT', 'US/Central') as finish,
  timediff(Task.finish, Task.start) as duration,
  Task.result,
  Task.host as taskhost,
  Run.host,
  Run.gid,
  Run.runid,
  Run.filepath,
  Task.task_id
FROM
  Task LEFT JOIN Run USING (runid)
WHERE 
  Task.name='".$task."' AND
  Task.platform='".$platform."'
";

if ($runid) {
  $query.=" AND Task.runid=$runid";
}
$query .= "
ORDER BY
  Task.start DESC
";


$db = mysql_connect(WEB_DB_HOST, DB_READER_USER, DB_READER_PASS) 
      or die ("Could not connect : " . mysql_error());
mysql_select_db(DB_NAME) or die("Could not select database");

$result = mysql_query($query) or die ("Query failed : " . mysql_error());

while( $myrow = mysql_fetch_array($result) ) {
  $taskname=$myrow["name"];
  $hostname=$myrow["host"];
  $gid=$myrow["gid"];
  $base_filepath=$myrow["filepath"];
  #echo "<h1>$hostname/$base_filepath/$gid/$taskname</h1>";

  # use .out file to determine path - .err file base path is indentical  
  $filepath = "";
  $basedir = $base_filepath; //"rundir";   
  //$local_fs_prefix = "/nmi/nwo/www/html/";

  if ( strstr($taskname, "platform_job") ){
    $filepath = "$basedir/$gid/$taskname.$platform";
    $file_found=file_exists($filepath.".out");;
    #echo "<h1>$filepath</h1>";
  }
  else if ( strstr($taskname, "platform_post") ){
    $filepath = "$basedir/$gid/$taskname.$platform";
    $file_found=file_exists($filepath.".out");;
    #echo "<h1>$filepath</h1>";
  }
  else if ( strstr($taskname, "platform_pre") ){
    $filepath = "$basedir/$gid/$taskname.$platform";
    $file_found=file_exists($filepath.".out");;
    #echo "<h1>$filepath</h1>";
  }
  else if (strstr($platform, "local"))  {   
    $filepath = "$basedir/$gid/$taskname";
    $file_found = file_exists($filepath.".out");
    #echo "<h1>$filepath</h1>";
  }
  else {   
    $filepath = "$basedir/$gid/userdir/$platform/$taskname";
    $file_found = file_exists($filepath.".out");
    #echo "<h1>$filepath</h1>";
  }

  if (!$file_found) {
    $stdout_url = "N/A";
    $stderr_url = "N/A";
  }
  else {
    $stat = stat("$filepath.out");
    $stdout_size = $stat['size'];
    $stdout_size_display = number_format($stdout_size);
    
    $stat = stat("$filepath.err");
    $stderr_size = $stat['size'];
    $stderr_size_display = number_format($stderr_size);
    
    $stdout_url = "<a href=\"http://$hostname/$filepath.out\">".basename($filepath).".out</a>";
    $stderr_url = "<a href=\"http://$hostname/$filepath.err\">".basename($filepath).".err</a>";
  }
  
  $resultspath = "$basedir/$gid/userdir/$platform/results.tar.gz";
  $resfound = file_exists($local_fs_prefix.$resultspath);
  
  $num_warnings = `grep -c -i warning $filepath.out`;
  $num_warnings_stderr = 0;
  # STDERR is usually empty so don't do a shell callout unless stderr size is > 0
  if($stderr_size > 0) {
    $num_warnings_stderr = `grep -c -i warning $filepath.err`;
  }

  if(!$resfound) {
    $results_url = "N/A";
  }
  else {
    $results_url = "<a href=\"http://$hostname/$resultspath\">".basename($resultspath)."</a>";
  }
  
  $resultval = $myrow["result"];
  $test_results_url = "<a href=\"http://nmi.cs.wisc.edu/node/552\">".$resultval."</a>";
  echo "<p>";
  echo "<table>";
#    echo "<tr><td>Run ID:</td><td><a href=\"http://$hostname/nmi/?page=results/runDetails&id=".$myrow["runid"] ."\">".$myrow["runid"]."</a></td></tr>";
    #echo "<tr><td>Run ID:</td><td><a href=\"http://$hostname/nmi/?page=results/runDetails&runid=".$myrow["runid"] 
  echo "<tr><td>Run ID:</td><td><a href=\"./Task-search.php?runid=".$myrow["runid"]."&Submit=Search+by+RunID" 
    ."\">".$myrow["runid"]."</a></td></tr>";
  echo "<tr><td>Hostname:</td><td>" . $myrow["taskhost"] . "</td></tr>";
  echo "<tr><td>GID:</td><td>".$myrow["gid"] ."</td></tr>";
  echo "<tr><td>Task ID:</td><td>".$myrow["task_id"] ."</td></tr>";
  echo "<tr><td>Start:</td><td>".$myrow["start"] ."</td></tr>";
  echo "<tr><td>Finish:</td><td> ".$myrow["finish"] ."</td></tr>";
  echo "<tr><td>Duration:</td><td> ".$myrow["duration"] ."</td></tr>";
  echo "<tr><td>Result:</td><td> $test_results_url </td></tr>";
  echo "<tr><td>Stdout:</td><td> $stdout_url - (size: $stdout_size_display bytes) </td></tr>";
  echo "<tr><td>Stderr:</td><td> $stderr_url - (size: $stderr_size_display bytes) </td></tr>";
  echo "<tr><td>Run Results:</td><td> $results_url</a></td></tr>";
  echo "<tr><td># warnings STDOUT:</td><td>$num_warnings</td></tr>";
  echo "<tr><td># warnings STDERR:</td><td>$num_warnings_stderr</td></tr>";
  echo "</table>";
  echo "</p>";

  if($file_found) {
    if($stdout_size > 0) {
      show_file_content("STDOUT", "$filepath.out");
    }
    
  }
}

mysql_free_result($result);
mysql_close($db);

function show_file_content($header, $file) {
  echo "<hr>\n";
  echo "<h3>$header:</h3>";

  if($_REQUEST["type"] == "build" && preg_match("/_win/", $_REQUEST["platform"])) {
    // For windows we have a script that does some smarter parsing
    $lines = `./parse-windows-build.pl $file`;
    echo "<pre>$lines</pre>\n";
  }
  else {
    // For linux show some lines from the bottom of the file
    $count = 20;
    $lines = `tail -$count $file`;

    // Do some nice highlighting to make warnings/errors more obvious
    $lines = preg_replace("/(error)/i", "<font style='background-color:red'>$1</font>", $lines);
    echo "<p style=\"font-size: 80%;\">Last $count lines of $header:\n";
    echo "<pre>$lines</pre>";
  }

}

?>
</body>
</html>

