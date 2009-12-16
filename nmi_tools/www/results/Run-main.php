<?php

include "last.inc";

?>

<html>
<body bgcolor="#d9d9ff" text="navy" link="#666699"  alink=#000000 vlink="#333333">
<title>NWO run results</title>
<LINK REL="StyleSheet" HREF="condor.css" TYPE="text/css">

<center><h2>UW NMI build and test system</h2><br>
Select a run id from the left column or a run dir from the right to view more details, or use the search options below to refine your view. 

<?php
  
require_once "./load_config.inc";
load_config();
   

  $local_hostname = WEBSERVER;

  # get args, and figure out if we are accessing this page for the first time or not
  $args["gid"] = $_REQUEST["gid"];
  $args["runid"] = $_REQUEST["runid"];
  $args["user"] = $_REQUEST["user"];
  $args["result"] = $_REQUEST["status"];
  $args["run_type"] = $_REQUEST["runtype"];
  $args["project"] = $_REQUEST["project"];
  $args["component"] = $_REQUEST["component"];

  $limit = $_REQUEST["limit"];

  # don't put the date arguments into the args array because they must be manipulated before we can use them
  $arg_day = $_REQUEST["day"];
  $arg_month = $_REQUEST["month"];
  $arg_year = $_REQUEST["year"];   

  $currentDir = "results";
  $basedir = "rundir"; # absolute path is /nmi/run 

  $su = $_REQUEST["search"]; # search button
  $re = $_REQUEST["reset"];  # reset button
  $da = $_REQUEST["today"];  # today button
  
  if ( is_null( $re ) ) {
    # trim down the array - remove keys whose value is ALL or ""
    foreach ($args as $key => $val) {
      if($val == "" || $val == " " || is_null($val) || (strstr($val, "ALL")) ) { 
        unset($args[$key]);
      }
    }
  } else {
    # reset array of search values to null
    foreach ($args as $key => $val) {
      unset($args[$key]);
    }
    unset($arg_day);
    unset($arg_month);
    unset($arg_year);
  }

  $db = mysql_connect(WEB_DB_HOST, DB_READER_USER, DB_READER_PASS) or die ("Could not connect : " . mysql_error());
  mysql_select_db(DB_NAME) or die("Could not select database");

  # find all unique users of the system for the dropdown box
  $query2 = "SELECT DISTINCT user FROM Run";
  $result2 = mysql_query($query2) or die ("Query failed : " . mysql_error());
  while($res2 = mysql_fetch_array($result2)){
    $t_users[] = trim( $res2["user"]);
  }
  $tempusers = array_unique( $t_users );
  foreach ($t_users as $t_user) {
    $users["$t_user"] = "";
  }
  ksort( $users );
  $arg_users = $args["user"];
  if ( is_null( $arg_users ) ) {
    $users["ALL"] = "SELECTED";
    $users["$arg_users"] = "";
  } else {
    $users["ALL"] = "";
    $users["$arg_users"] = "SELECTED";
  }   
 
  # find all possible run types for dropdown box
  $query3 = "SELECT DISTINCT run_type FROM Run";
  $result3 = mysql_query($query3) or die ("Query failed : " . mysql_error());
  while($res3 = mysql_fetch_array($result3)){
    $t_run_types[] = trim( $res3["run_type"] );
  }
  $t_run_types = array_unique( $t_run_types );
  foreach ($t_run_types as $t_run_type) {
    $run_types["$t_run_type"] = "";
  }
  ksort( $run_types );
  $arg_run_type = $args["run_type"];
  if ( is_null( $arg_run_type ) ) {
    $run_types["ALL"] = "SELECTED";
    $run_types["$arg_run_type"] = "";
  } else {
    $run_types["ALL"] = "";
    $run_types["$arg_run_type"] = "SELECTED";
  }

  # find all status possiblities for dropdown box
  $query4 = "SELECT DISTINCT result FROM Run";
  $result4 = mysql_query($query4) or die ("Query failed : " . mysql_error());
  while($res4 = mysql_fetch_array($result4)){
    if (  is_null( $res4["result"] ) ) {
      $t_statuses[] = "NULL";
    } else {
      $t_statuses[] = trim( $res4["result"] );
    }
  }
  $t_statuses = array_unique( $t_statuses );
  foreach ($t_statuses as $t_status) {
    $statuses["$t_status"] = "";
  }
  ksort( $statuses );  
  $arg_result = $args["result"];
  if ( is_null( $arg_result ) ) {
    $statuses["ALL"] = "SELECTED";
    $statuses["$arg_result"] = "";
  } else {
    $statuses["ALL"] = "";
    $statuses["$arg_result"] = "SELECTED";
  }

  # find all project possiblities for dropdown box
  $query5 = "SELECT DISTINCT project FROM Run";
  $result5 = mysql_query($query5) or die ("Query failed : " . mysql_error());
  while($res5 = mysql_fetch_array($result5)){
    $t_projects[] = trim( $res5["project"] );
  }
  $t_projects = array_unique( $t_projects );
  foreach ($t_projects as $t_project) {
    $projects["$t_project"] = "";
  } 
  uksort($projects, "strnatcasecmp");
  $arg_project = $args["project"];
  if ( is_null( $arg_project ) ) {
    $projects["ALL"] = "SELECTED";
    $projects["$arg_project"] = "";
  } else {
    $projects["ALL"] = "";
    $projects["$arg_project"] = "SELECTED";
  }

  # find all status possiblities for dropdown box
  $query6 = "SELECT DISTINCT component FROM Run";
  $result6 = mysql_query($query6) or die ("Query failed : " . mysql_error());
  while($res6 = mysql_fetch_array($result6)){
    $t_components[] = trim( $res6["component"] );
  }
  $t_components = array_unique( $t_components );
  foreach ($t_components as $t_component) {
    $components["$t_component"] = "";
  }
  uksort($components, "strnatcasecmp");
  $arg_component = $args["component"];
  if ( is_null( $arg_component ) ) {
    $components["ALL"] = "SELECTED";
    $components["$arg_component"] = "";
  } else {
    $components["ALL"] = "";
    $components["$arg_component"] = "SELECTED";
  }

  # To get date functions to work, you have to compile PHP with --enable-calendar.
  # find all day possiblities for dropdown box
  $find_date = date('d');

  for ($i = 1; $i <= 31; $i++) {
    $date_array["$i"] = "";
  }

  if ( is_null( $arg_day ) ) {
    $date_array["ALL"] = "SELECTED";
    $date_array["$arg_day"] = "";
  } else if ( !is_null( $da ) ) { # today button pressed
    $date_array["$find_date"]="SELECTED";
    $date_array["ALL"] = "";
  } else {
    $date_array["ALL"]="";
    $date_array["$arg_day"]="SELECTED";
}

  # TODO: Use month name rather than numbers once we have date functions available.
  # find all month possiblities for dropdown box
  $find_date = date('m');
 
  for ($i = 1; $i <= 12; $i++) {
    #$month_array["$i"] = JDMonthName( $i, 0 );
    $month_array["$i"] = "";
  }

  if ( is_null( $arg_month ) ) {
    $month_array["ALL"] = "SELECTED";
    $month_array["$arg_month"] = "";
  } else if ( !is_null( $da ) ) { # today button pressed
    $month_array["$find_date"]="SELECTED";
    $month_array["ALL"] = "";
  } else {
    $month_array["ALL"] = "";
    $month_array["$arg_month"] = "SELECTED";
  }

  # find all year possiblities for dropdown box
  $find_date = date('Y');

  $year_array["2004"]="";
  $year_array["2005"]="";

  if ( is_null( $arg_year ) ) {
    $year_array["ALL"] = "SELECTED";
    $year_array["$arg_year"] = "";
  } else if ( !is_null( $da ) ) { # today button pressed
    $year_array["$find_date"]="SELECTED";
    $year_array["ALL"] = "";
  } else {
    $year_array["ALL"] = "";
    $year_array["$arg_year"] = "SELECTED";
  }

?> 
<br><br>
  <table width=750 border=0 bgcolor=#d9d9ff>

<!-- User Area -->
  <tr bgcolor=#d9d9ff width=750 valign=middle>
  <td width=200 align=center valign=middle> 
  <form method="get" action="Run-main.php">
    <SELECT name="user">
<?php
    foreach($users as $user=>$selected) {
      echo "<OPTION VALUE=$user $selected> $user </OPTION>";
    }
?>
    </SELECT> User
  </td>

<!-- Project Area -->
  <td width=300 align=center>
    <SELECT name="project">
<?php
    foreach($projects as $project=>$selected) {
      echo "<OPTION VALUE=$project $selected> $project </OPTION>";
    }
?>
    </SELECT> Project
  </td>

<!-- Status Area -->
  <td width=250 align=center>
    <SELECT name="status">
<?php
    foreach($statuses as $status=>$selected) {
  
      if ( $status == "0" ) {
        echo "<OPTION VALUE='$status' $selected>  0";
      } else if ( $status == "NULL" ) {
        echo "<OPTION VALUE='$status' $selected>  running";
      } else {
        echo "<OPTION VALUE=$status $selected>  $status ";
      }
    }
?>
    </SELECT> Result
  </td></tr>

<!-- RunType Area -->
  <tr bgcolor=#d9d9ff width=750>
  <td width=200 align=center>
    <SELECT name="runtype">
<?php
    foreach($run_types as $run_type=>$selected) {
      echo "<OPTION VALUE=$run_type $selected> $run_type ";
    }
?>
    </SELECT> Run Type
  </td>

<!-- Component Area -->
  <td width=300 align=center>
    <SELECT name="component">
<?php
    foreach($components as $component=>$selected) {
      echo "<OPTION VALUE='$component' $selected> $component ";
    }
?>
    </SELECT> Component
  </td>

<!-- Size Limit Area -->
  <td width=250 align=center>
    <input type=text name="limit" size=8> Results Limit
  </td></tr>

<!-- Day Area -->
  <tr bgcolor=#d9d9ff width=600>
  <td width="50" align=center> 
    <SELECT name="day">
<?php
    foreach($date_array as $dates=>$selected) {
      echo "<OPTION VALUE='$dates' $selected>$dates ";
    }
?>
    </SELECT> Day 
 </td>

<!-- Month Area -->
  <td width="50" align=center>
    <SELECT name="month">
<?php
    foreach($month_array as $months=>$selected) {
      echo "<option value='$months' $selected>$months ";
    }
?>
    </SELECT> Month       
 </td>

<!-- Year Area -->
  <td width="50" align=center>
  <SELECT name="year">
<?php
    foreach($year_array as $years=>$selected) {
      echo "<option value='$years' $selected>$years ";
    }
?>
    </SELECT> Year       
 </td></tr>

<!-- Button for today's date -->
  <tr bgcolor=#d9d9ff width=600>
  <td width="50" align=center>
    <input type="submit" name="today" value="Today"></input>
  </td>

<!-- Button for this month -->
  <td width="50" align=center>
    <input type="submit" name="thismonth" value="This month"></input>
  </td>
<!-- Button for this year -->
  <td width="50" align=center>
    <input type="submit" name="thisyear" value="This year"></input>
  </td></tr>

  </table>
    <pre>
    <input type="submit" name="search" value="Submit Search"></input>  <input type="submit" name="reset" value="Reset Values"></input>
    </pre>   
  </form>
  <hr>
<!-- ID select Form -->
  <center>
  <form method="get" action="Task-search.php">
      <input type="text" size=50 name="gid"></input>
      <input type="submit" name="submit" value="Search by GID" size=100></input>
  </form>
  <form method="get" action="Task-search.php">
      <input type="text" size=6 name="runid"></input>
      <input type="submit" name="submit" value="Search by RunID" size=100></input>
  </form>
  </center>

  <br>

<?php

  ##############################################################################
  # Create database queries to populate the statistics header and results table.
  ##############################################################################

  # adjust date formats for statistics queries
  $dd .='_________';
  $date_format='%';
  $d_f ="%";

  # Put all date calculations here 
  # TODO: To get PHP's date functions to work, you have to compile PHP with --enable-calendar.
  # TODO: Some of the date calculations are broken.  Fix them when we have date functions available.   
  if ( !is_null($arg_year) && !strstr( $arg_year, "ALL") ) { 
    if ( !is_null($arg_month) && !strstr( $arg_month, "ALL") ) {
      if (!is_null($arg_day) && !strstr( $arg_day, "ALL") ) {
        $date_range_start = "$arg_year-$arg_month-$arg_day 05:00:00";
        $newday = $arg_day + 1; 
        $date_range_finish = "$arg_year-$arg_month-$newday 05:00:00"; 
      } else {
        $date_range_start = "$arg_year-$arg_month-00 05:00:00";
        $newmonth = $arg_month + 1; 
        $date_range_finish = "$arg_year-$newmonth-00 05:00:00"; 
      } 
    } else {
        $date_range_start = "$arg_year-00-00 05:00:00";
        $newyear = $arg_year + 1; 
        $date_range_finish = "$newyear-00-00 05:00:00"; 
    }
  } else { # no user input for dates
    $date_range_start = "2000-00-00 05:00:00";
    $date_range_finish = "2010-00-00 05:00:00";
  }  
         
  $d_f = "$date_range_start";
  $date_format = "$date_range_finish"; 

  # create the SQL query for main page display 
  $arsize = sizeof($args);
  $count = 0;
  $query1 = " FROM Run ";
  $limit1 = " ORDER BY runid DESC ";
  if ($arsize > 0) { # use user entered search criteria 
    $query1 .= " WHERE ";
    # go through arg list
    foreach ($args as $key => $val) {
      if ( $val == "NULL" ) {
        $query1 .= " $key IS NULL ";
      } else {
        $query1 .= " $key='$val' ";
      }
      $count++;
      if ( $count < $arsize ) {
        $query1 .= "AND";
      }
    }

    if ($limit) {
      $limit1 .= " LIMIT $limit";
    } else {
      $limit1 .= " LIMIT 200";
    }
    $connector = " AND";

  } else if ($limit) {
    $query_main = "SELECT * FROM Run ORDER BY runid DESC LIMIT $limit";
    $connector = " WHERE";
  } else { # use default display criteria
    $query_main = "SELECT * FROM Run ORDER BY runid DESC LIMIT 400";
    $connector = " WHERE";
  }
 
  # if we have a complete query, don't bother with adding to it.
  if ( !strstr( $query_main, "SELECT") ) {
    $query_main = "SELECT * " . "$query1" . "and start > '$d_f' and start < '$date_format'". "$limit1";
  }

  #  add the prefix for various statistics queries 
  $query_stat = "select distinct user " . "$query1" . "$connector" . " start > '$d_f' and start < '$date_format'";
  $query_stat1 = "select result as builds1 " . "$query1" . "$connector" . " start > '$d_f' and start < '$date_format'";
  $query_stat2 = "select result" . "$query1" . "$connector" . " start > '$d_f' and start < '$date_format' and result IS NULL";
  $query_stat3 = "select count(result) as builds3" . "$query1" . "$connector" .  " result >'0' and start > '$d_f' and start < '$date_format'";
  $query_stat4 = "select count(result) as builds4" . "$query1" . "$connector" . " result ='-1' and start > '$d_f' and start < '$date_format'";

  # run the statistics queries
  $db = mysql_connect(WEB_DB_HOST, DB_READER_USER , DB_READER_PASS) or die ("Could not connect : " . mysql_error());
  mysql_select_db(DB_NAME) or die("Could not select database");
  $result1 = mysql_query($query_main) or die ("Query failed : " . mysql_error());

  $result_stat = mysql_query($query_stat) or die ("Query failed : " . mysql_error());
  $buildsub = mysql_num_rows($result_stat);

  $result_stat1 = mysql_query($query_stat1) or die ("Query failed : " . mysql_error());
  $builds1 = mysql_num_rows($result_stat1);

  $result_stat2 = mysql_query($query_stat2) or die ("Query failed : " . mysql_error());
  $buidsprog = mysql_num_rows($result_stat2);

  $result_stat3 = mysql_query($query_stat3) or die ("Query failed : " . mysql_error());
  list($builds3) = mysql_fetch_row($result_stat3);
   
  $result_stat4 = mysql_query($query_stat4) or die ("Query failed : " . mysql_error());
  list($builds4) = mysql_fetch_row($result_stat4);

  # perform statistics calculations
  $percentage_multiplier = 100;
  if ($builds1 == '0') {
    $pprog = 0;
    $pfail = 0; 
    $prem = 0;
    $psuc = 0; 
    $ar=0;
  } else {
    $pprog = ($buidsprog/$builds1) * $percentage_multiplier;
    $pprog = round($pprog, 2);
   
    $pfail = ($builds3/$builds1) * $percentage_multiplier;
    $pfail = round($pfail, 2);
  
    $prem = ($builds4/$builds1) * $percentage_multiplier;
    $prem = round($prem, 2);
  
    $ar=$builds1 - ($buidsprog + $builds3 + $builds4);
    $psuc = ($ar/$builds1) * $percentage_multiplier;
    $psuc = round($psuc, 2);
  }
 
  # display the statistics
  echo "<table bgcolor='#cdcdcd' bordercolordark='black'>";
  echo "<tr><td>Total number of distinct users is " .$buildsub ."</td></tr>";
  echo "<tr><td>Total number of runs submitted is " . $builds1 ."</td></tr>";
  echo "<tr><td>Total number of runs in-progress is ".$buidsprog;
  echo "</td><td width='6'></td><td> Percentage of in-progress runs: ".$pprog . "% </td></tr>";
  echo "<tr><td>Total number of runs failed is " . $builds3;
  echo "</td><td width='6'></td><td>Percentage of failed runs: ".$pfail . "% </td></tr>";
  echo "<tr><td>Total number of runs removed is " . $builds4;
  echo "</td><td width='6'></td><td> Percentage of removed runs: ".$prem . "% </td></tr>";
  echo "<tr><td>Total number of runs successful is ".$ar ;
  echo "</td><td width='6'></td><td> Percentage of successful runs: ".$psuc . "% </td></tr></table>";

  # display the results table
  echo "<table width=100% border=1 bgcolor=#d9d9ff>";
  echo "<tr bgcolor=white>";
  echo "<td width=50>RunID</td>";  
  echo "<td width=20>Result</td>";  
  echo "<td width=70>User</td>";  
  echo "<td width=120>Type</td>";  
  echo "<td width=120>Project</td>";  
  echo "<td width=120>Component</td>";  
  echo "<td width=120>Start (CST) </td>";  
  echo "<td width=120>Duration HH:MM:SS</td>";  
  echo "<td width=100>Description</td>";  
  echo "<td width=50>Platforms</td>";  
  echo "<td width=50>Archived</td>";  
  echo "<td width=100>Pin Expire Date</td>";  
  echo "</tr>"; 
 
  while( $myrow = mysql_fetch_array($result1) ) {
    $gid = $myrow["gid"];
    $runid = $myrow["runid"];
    $project = $myrow["project"];
    $component = $myrow["component"];
    $user = $myrow["user"];
    $run_type = $myrow["run_type"];
    $start_ts = strtotime("$myrow[start]");
    $finish_ts = strtotime("$myrow[finish]");
    $start_ts = $start_ts +  date("Z", $start_ts);
    $start = date("m/d/y  H:i:s ", $start_ts);
    $finish_ts = $finish_ts +  date("Z", $finish_ts);
    $finish = date("m/d/y  H:i:s ", $finish_ts);
    $result = $myrow["result"];
    $description = $myrow["description"];
    $archived = $myrow["archived"];
    $archived_until = $myrow["archive_results_until"];
    $base = "$basedir/$gid/userdir";
    $webserver = $myrow["host"];
	 $now_ts = strtotime("now");
	 $pined_ts = strtotime("$myrow[archive_results_until]");
	 $pined = $pined_ts - $now_ts;
 
    # find the duration, if the run is complete
    # if not finished, don't display a finish time
    if ( !( is_null($result)) ) {
      $duration_ts = $finish_ts - $start_ts;
      $duration = $duration_ts;   
    } else {
      $duration = "In progress";
      $finish = "In progress";
    }     
   
    echo "<tr>";
    echo "<td><a href=\"http://$webserver/$currentDir/Task-search.php?runid=$runid&gid=$gid\">$runid</a><br>$archived_until</td>";
    # figure out the result
    if (  is_null($result) ) {
      printf("<td bgcolor=#ffff55>%s</td>", "running");
    } else if ( $result == 0) {
       printf("<td bgcolor=#55ff55>%s</td>", $result);
    } else if ( $result == -1) {
       printf("<td bgcolor=#cd5c5c>%s</td>", "removed");
    } else { 
      if ($component == 'condor' && preg_match('/^BUILD/', $run_type)) {
         $link = getLastGoodBuild($runid, 0);
         if ($link != '') {
           echo "<td bgcolor=#ff5555><a href=\"$link\">$result</a></td>";
         } else {
           printf("<td bgcolor=#ff5555>%s</td>",$result);   
         }
      } else {
         printf("<td bgcolor=#ff5555>%s</td>",$result);   
      }
    }
    printf("<td>%s</td>", $user);
    printf("<td>%s</td>", $run_type);
    printf("<td>%s</td>", $project);
    printf("<td>%s</td>", $component);
    echo "<td> $start  </td>";

    # split up duration into readable units  
    if ($duration != "In progress") {
      $hours = 0; $minutes = 0; $seconds = 0; 
      $hours = floor($duration / 3600);
      $newruntime = $duration % 3600;
      $minutes = floor($newruntime / 60);
      $seconds = $duration % 60;
      echo("<td>$hours : $minutes : $seconds</td>");
    } else {
      printf("<td>%s</td>", $duration);
    }
    printf("<td>%s</td>", $description);

    # figure out if we have one or many platforms included in the runid
    # if there is only one (besides local), display it, otherwise print the number of platforms
    $query7 = "SELECT * FROM Task where runid=$runid";
  
    $db = mysql_connect(WEB_DB_HOST, DB_READER_USER, DB_READER_PASS) or die ("Could not connect : " . mysql_error());
    mysql_select_db(DB_NAME) or die("Could not select database");
    $result7 = mysql_query($query7) or die ("Query failed : " . mysql_error());
 
    $unique_plats = Array();
    while( $res7 = mysql_fetch_array($result7) ) {
      $unique_plats[] = trim( $res7["platform"] );
    }
    $unique_plats = array_unique( $unique_plats );
    # remove "local" and null from the list  
    foreach($unique_plats as $key => $value) {
      if( $value == "" || $value == " " || is_null($value) || $value == "local" ) {
        unset( $unique_plats[$key] );
      } else {
        $myPlat = $value;
      } 
    }    

    if ( sizeof($unique_plats) > 1 ) {
        $tmp = sizeof($unique_plats);
        printf("<td><center>%s</center></td>", "$tmp");
      } else {
        printf("<td><center>%s</center></td>", "$myPlat");
    }
   
    foreach($unique_plats as $key => $value) {
      unset( $unique_plats[$key] );
    }    

 
    if ( $archived == 1 ) { 
        echo "<td bgcolor=#cdcdcd><center><a href=\"http://$local_hostname/$base\">Y</a></center></td>";
        printf("<td>%s</td>", $archived_until);
    } else { 
        printf("<td><center>%s</center></td>", "N");
        printf("<td><center>%s</center></td>", "");
    }
    
    echo "</tr>";
  }
  echo "</table>";
  echo "</center>";

mysql_free_result($result1);
mysql_free_result($result2);
mysql_free_result($result3);
mysql_free_result($result4);
mysql_free_result($result5);
mysql_free_result($result6);
mysql_close($db);

?>
</body>
</html>
