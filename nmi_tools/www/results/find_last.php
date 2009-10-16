<?php

include "last.inc";

?>

<html>
<body>
<center>
<h2>UW NMI "Who broke the build?" query. </h2>
</center>

<?php
$id = $_REQUEST['id'];
$db = mysql_connect('mysql.batlab.org', 'nmipublic', 'nmiReadOnly!') or die ("Could not connect : " . mysql_error());
mysql_select_db('nmi_history') or die("Could not select database");
$bonsai = getLastGoodBuild($id);
echo "<br><a href=\"$bonsai\">Click here to see the diffs vs the last successful build.</a><br>\n";
echo "<br>\n";
mysql_close($db);
?>

</body>
</html>
