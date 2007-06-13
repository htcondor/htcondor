<html>
<body bgcolor="orange" text="navy" link="#666699"  alink=#000000 vlink="#333333">

<center>
<h2>UW NMI build and test system</h2>

<br>

<table cellspacing=1 cellpadding=3 border=1>
  <tr bgcolor=#999999>
    <td>Host</td>
    <td>Platform</td>
    <td>State</td>
    <td>Activity</td>
    <td>ActivityTime</td>
    <td>User</td>
    <td>RunID</td>
  </tr>

<?php
    putenv('PATH=/nmi/bin:/usr/local/condor/bin:' . getenv('PATH'));
    $handle = popen('nmi_condor_status -ww | grep -v -E "^(Name|----)"', 'r');
    while (!feof($handle)) {
        $buffer = fgets($handle, 1000);
        list($host, $platform, $state, $activity, $activity_time, $user, $run_id) = split(" *\| *", $buffer, 7);
        if ($host != "")
        {
            if ($state == "Unclaimed")
            {
                $color = "#bbffbb";
            }
            else if ($state == "Owner")
            {
                $color = "77bb77";
            }
            else 
            {
                $color = "#ffbbbb";
            }
            print "<tr bgcolor=$color>";
            print "<td>$host</td><td>$platform</td><td>$state</td>";
            print "<td>$activity</td><td align=right>$activity_time</td>";
            print "<td>$user</td>";
            if ($run_id > 0)
            { 
                print "<td><a href=Task-search.php?runid=$run_id&column=platform>$run_id</a></td>";
            }
            else
            {
                print "<td>$run_id</td>";
            }
            print "</tr>\n";
        }
    }
    pclose($handle);
?>
</table>
</body>
</html>

