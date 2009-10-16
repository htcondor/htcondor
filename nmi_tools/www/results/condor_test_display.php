<html>
<body bgcolor="#ffffff" text="#000033" link="#666699"  alink=#000000 vlink="#333333">

<title>Condor test results</title>
<center><h2>Condor test results </h2>

<?php

  $dbName = "nmi_history";
  $dbUsername = "nmipublic";
  $dbPassword = ""; 
  $currentDir = "results";
  $limit = 40; 

  # find the last given number of builds,  ordered by the newest builds first 
  $query0 = "SELECT * FROM Run where user='cndrauto' and run_type='build' ORDER BY runid DESC LIMIT $limit"; 
  $db = mysql_connect("mysql.batlab.org", "$dbUsername", "$dbPassword") or die ("Could not connect : " . mysql_error());
  mysql_select_db("$dbName") or die("Could not select database");
  $result0 = mysql_query($query0) or die ("Query failed : " . mysql_error());
  while( $myrow = mysql_fetch_array($result0) ) {
    $myrunid = $myrow["runid"]; 
    $mydesc = $myrow["description"]; 
    $mydesc = trim( $mydesc ); 
    $tags["$myrunid"] = "$mydesc";
  }

?> 
  <form method="get" action="Condor-testsuite-home.php">
    Select a tag from the dropdown list <br><br>
    <SELECT NAME="description">
<?php
      foreach ( $tags as $runid => $desc ) {
        echo "<OPTION VALUE=$desc>  $runid ($desc) </OPTION>";
        echo "<input type=\"hidden\" name=\"description\" value=$desc><br>";
      }
?>     
    </SELECT>
    <input type="submit" name="submit" value="Get Results"></input> 
  </form> 
  <br><br> or enter a build tag <br><br>

  <form method="get" action="Condor-testsuite-home.php">
    <input type="text" name="description" size=40></input>
    <input type="submit" name="submit" value="Get Results"></input>
  </form>

<?php
mysql_close($db);

?>
</body>
</html>

