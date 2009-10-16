<html>
<body>


<title>NWO task results</title>

<?php
include "last.inc";
require_once "./load_config.inc";                                     
load_config(); 

 
$platform=$_REQUEST["platform"];
$task=$_REQUEST["task"];
$description=$_REQUEST["description"];
$runid=$_REQUEST["runid"];

echo "<P>";
echo "<B>Tag: </b>$description<BR>";
echo "<B>Task: </b>$task<br>";
echo "<B>Platfrom: </b>$platform<br>";
echo "</P>";

$query = "
SELECT
  Task.name,
  convert_tz(Task.start, 'GMT', 'US/Central') as start,
  convert_tz(Task.finish, 'GMT', 'US/Central') as finish,
  timediff(Task.finish, Task.start) as duration,
  Task.result,
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
    $platfrom=$myrow["platform"];
    $hostname=$myrow["host"];
    $gid=$myrow["gid"];
    $base_filepath=$myrow["filepath"];

    # use .out file to determine path - .err file base path is indentical  
    $filepath = "";
    $basedir = $base_filepath; //"rundir";   
    //$local_fs_prefix = "/nmi/nwo/www/html/";

      if ( strstr($taskname, "platform_job") ){
        $file_found = 0;
      # }  else if ( strstr($hostname, THIS_HOST) ){  
        # if ( strstr($platform, "local") ) {
        #   $filepath = "$basedir/$gid/$taskname";
        # } else {
        #   $filepath = "$basedir/$gid/$taskname.$platform";
        # }
	# $file_found=1;
      } else if ( !(strstr($platform, "local")) ) {   
        $filepath = "$basedir/$gid/userdir/$platform/$taskname";
	$file_found = file_exists($local_fs_prefix.$filepath.".out");
      } else {
        $filepath = "$basedir/$gid/userdir/$platform/$taskname";
	$file_found = 1;
      }

		$resultspath = "$basedir/$gid/userdir/$platform/results.tar.gz";
		$resfound = file_exists($local_fs_prefix.$resultspath);

      if (!$file_found) {
	$stdout_url = "N/A";
	$stderr_url = "N/A";
      } else {
	$stdout_url = "<a href=\"http://$hostname/$filepath.out\">".basename($filepath).".out</a>";
	$stderr_url = "<a href=\"http://$hostname/$filepath.err\">".basename($filepath).".err</a>";
      }

		if(!$resfound){
			$results_url = "N/A";
		} else {
			$results_url = "<a href=\"http://$hostname/$resultspath\">".basename($resultspath)."</a>";
		}

	$resultval = $myrow["result"];
	$test_results_url = "<a href=\"http://nmi.cs.wisc.edu/node/552\">".$resultval."</a>";
    echo "<P>";
    echo "<TABLE>";
#    echo "<TR><TD>Run ID:</TD><TD><a href=\"http://$hostname/nmi/?page=results/runDetails&id=".$myrow["runid"] ."\">".$myrow["runid"]."</a></TD></TR>";
    #echo "<TR><TD>Run ID:</TD><TD><a href=\"http://$hostname/nmi/?page=results/runDetails&runid=".$myrow["runid"] 
    echo "<TR><TD>Run ID:</TD><TD><a href=\"./Task-search.php?runid=".$myrow["runid"]."&Submit=Search+by+RunID" 
."\">".$myrow["runid"]."</a></TD></TR>";
    echo "<TR><TD>GID:</TD><TD>".$myrow["gid"] ."</TD></TR>";
    echo "<TR><TD>Task ID:</TD><TD>".$myrow["task_id"] ."</TD></TR>";
    echo "<TR><TD>Start:</TD><TD>".$myrow["start"] ."</TD></TR>";
    echo "<TR><TD>Finish:</TD><TD> ".$myrow["finish"] ."</TD></TR>";
    echo "<TR><TD>Duration:</TD><TD> ".$myrow["duration"] ."</TD></TR>";
    #echo "<TR><TD>Result:</TD><TD> ".$myrow["result"] . "</TD></TR>";
    echo "<TR><TD>Result:</TD><TD> $test_results_url </TD></TR>";
    echo "<TR><TD>Stdout:</TD><TD> $stdout_url </TD></TR>";
    echo "<TR><TD>Stderr:</TD><TD> $stderr_url</a></TD></TR>";
    echo "<TR><TD>Run Results:</TD><TD> $results_url</a></TD></TR>";
    echo "</TABLE>";
    echo "</P>";
  }

mysql_free_result($result);
mysql_close($db);

?>
</body>
</html>

