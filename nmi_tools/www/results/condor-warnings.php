<html>
<body bgcolor="white" text="black" link="#666699"  alink=#000000 vlink="#333333">
<title>Condor Warnings Graph Generator</title>

<center><h2>Condor Warnings Graph Generator</h2>

<?php
  
  # TODO - clean these up and provide a config  
  $base_dir = "/home/cndrauto/warnings"; 
  $branch_data_file = "$base_dir/data/branches";
  $platforms_data_file = "$base_dir/data/platforms";

  $platforms[] = "";
  $fr = fopen ($platforms_data_file, "r");
  if ($fr){
    while ($results = fgets($fr, 4096)){
        $platforms = split(" ", $results);
    }
  }
  fclose($fr);

  $branches[] = "";
  $fr = fopen ($branch_data_file, "r");
  if ($fr){
    while ($results = fgets($fr, 4096)){
        $branches = split(" ", $results);
    }
  }
  fclose($fr);

?> 
<br><br>
  <form method="get" action="condor-warnings-graph.php">
  <table width=600 border=0 bgcolor=white>
  <tr bgcolor=white width=600>

<!-- Branch Area -->
  <td width=200 align=center> 
    <SELECT name="branch">
<?php
    foreach($branches as $branch) {
      echo "<OPTION VALUE=$branch> $branch </OPTION>";
    }
?>
    </SELECT> Branch
  </td>

<!-- Platform Area -->
  <td width=200 align=center>
    <SELECT name="platform">
<?php
    foreach($platforms as $platform) {
      echo "<OPTION VALUE=$platform> $platform </OPTION>";
    }
    echo "<OPTION VALUE=all SELECTED > all </OPTION>";
    echo "<OPTION VALUE=total> total </OPTION>";
    echo "<OPTION VALUE=all,total> all,total </OPTION>";
?>
    </SELECT> Platform
  </td>

<!-- Warning Type Area -->
  <td width=200 align=center>
    <SELECT name="warning_type">
<?php
  
    echo "<OPTION VALUE=total>  total </OPTION>";
    echo "<OPTION VALUE=unique>  unique </OPTION>";
?>
    </SELECT> Warning Type
  </td>
  </tr>
  </table>
    <br>
    <input type="submit" name="search" value="Submit Search"></input>
  </form>
  <hr>

</body>
</html>
