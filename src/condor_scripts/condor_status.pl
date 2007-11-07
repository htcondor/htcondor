#!/usr/bin/env perl

##**************************************************************
##
## Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
## University of Wisconsin-Madison, WI.
## 
## Licensed under the Apache License, Version 2.0 (the "License"); you
## may not use this file except in compliance with the License.  You may
## obtain a copy of the License at
## 
##    http://www.apache.org/licenses/LICENSE-2.0
## 
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
##**************************************************************



BEGIN
{
   $sorted = ['Machine Type','Priority'];
   $avail = ['All','Running Jobs','Submitting Jobs','Available'];
   $display = ['In detail','Summary Only'];
}

use CGI;

open(POOLFILE, "pool.dat");
$poolCnt = 0;

while(<POOLFILE>)
{
	if($_=~/\S+/)
	{
		($pool, $col) = split(' ',$_);
		@poolName[$poolCnt] = $pool;
		$poolCol{$pool} = $col;
		$poolCnt ++;
	}
}

close(POOLFILE);

$q = new CGI;

print $q->header;
print $q->start_html('Condor Status','yadong@cs.wisc.edu');



unless($q->param)
{
   print $q->start_form('Condor Status Query');
   print "<H1 align=center>Enter Your Choice of Hosts</H1><hr>";
   print "<p><h4 align=center>Select Condor Pool<p>", $q->popup_menu('pool',\@poolName);
   print "<P><align=center>Sorted by:<p>", $q->popup_menu('sorted',$sorted);
   print "<p>Select Host Type:<p>", $q->popup_menu('avail',$avail);
   print "<p>Disply Info:<p>", $q->popup_menu('display',$display);
   print "<p>",$q->checkbox('Show')," server disk and swap space";

   print "</h4>";
   print "<hr>";
   print "<h3 align=center>",$q->submit('submit');
   print ' ', $q->reset('reset'),'</h3>';
   print "<h4 align = center>This cgi utility is written by <a
href=\"http://uwscat.radiology.wisc.edu/li\">Yadong Li</a></h4>";
   print $q->end_form;
}
else
{
   $sorted = $q->param('sorted');
   $avail = $q->param('avail');
   $display = $q->param('display');
   $disk = $q->param('Show');
   $pool = $q->param('pool');
   $col = $poolCol{$pool};
   $colp = "-pool $col";
   $sortp = '';
   $sortp = " -prio" if ($sorted eq 'Priority');
   $availp = '';
   $availp = " -avail" if ($avail eq 'Available');
   $availp = " -run" if ($avail eq 'Running Jobs');
   $availp = " -submittors" if($avail eq 'Submitting Jobs');
   $disp = '';
   $disp = " -total" if ($display eq 'Summary Only');
   $diskp = '';
   $diskp = " -server" unless ($disk eq '');
   $cmd = "/unsup/condor/bin/condor_status";
   $cmdall = "$cmd $colp $sortp $availp $disp $diskp";
   $errall = "($cmdall>>/dev/null) 2>&1";

   @CondorQ = `$cmdall`;
   
   if (@CondorQ < 2)
	{
		print "Pool $pool is not available now";
		goto EXITT;
	}
#   @errorMsg = `$errall`;

   $title = shift @CondorQ;
   @titleField = split('\s+',$title);
   print "<H2>Condor Status for Pool $pool:</H2>";
   $sort = 'machine types' if ($sort eq '');
#   print "<h4>Your choice of sort method is: $sort</h4>";
#   print "<h4>Your choice of hosts type is: $avail</h4>";
#   print "<h4>Your choice of information is: $display</h4>";
#   if ($diskp eq " -server")
#   {
#      $dskmsg = "You choose to display disk and swap sapce info of the servers";
#   }
#   else
#   {
#      $dskmsg = "You choose not to display disk and swap space info of servers";
#   }
#   print "<h4>$dskmsg</h4>";
#   print "<h3><font color=\"#0000c0\">Click on host names
#         to see a detailed info of that host</font></h3><hr>";
   goto LABEL if ($disp eq " -total");

   if ($availp eq " -run")
   {
        $tmp = @titleField[0];
        @titleField[0] = @titleField[1];
        @titleField[1] = $tmp;
   }

   print "<table border=3><tr>";
   while (@titleField)
   {
        $cells = shift @titleField ;
        print "<th>$cells" ;
   }
   print "</tr>";

   while (@CondorQ)
   {
        $line = shift @CondorQ;
        $line =~ s/ham-and-cheese/ham-and-cheese /;
        @cellField = split('\s+',$line);

        goto SUMTABLE unless (@cellField[0]=~/\S+/);

        if ($availp eq " -run")
        {
            $tmp = @cellField[0];
            @cellField[0] = @cellField[1];
            @cellField[1] = $tmp;
        }

        print "<tr>";
        $machine = shift @cellField;
        print "<td>$machine</td>";
        while (@cellField)
        {
            $cell = shift @cellField;
            print "<td>$cell</td>";
        }
        print "</tr>";
   };
   SUMTABLE:
   {
        print "</table>\n" ;
   }

#   print "<hr>";
   print "<H2>Summary for Pool $pool:</H2>";

   $title = shift @CondorQ;
   while(!($title=~/\S+/))
   {
      $title = shift @CondorQ;
   }

   ($prix,$collector) = split(' : ',$title);
   goto ENDOFCGI if ($prix=~/^Collect/);

   @titleCell = split (' ',$title);
   print "<table border = 3><tr>";
   while (@titleCell)
   {
      $cell = shift @titleCell;
      print "<th>$cell";
   }
   print "</tr>";

   while(@CondorQ)
   {
      $line = shift @CondorQ;
      unless ($line=~/\w+/)
      {
         print "</table><p>";
         goto LABEL;
      }
      @linea = split(' ',$line);
      print "<tr>";
      while (@linea)
      {
         $cell = shift @linea;
         print "<td>$cell</td>";
      }
      print "</tr>";
   }

   LABEL:
   {
      while (@CondorQ)
      {
         $title = shift @CondorQ;
         ($prix, $collector) = split(' : ',$title);
         goto ENDOFCGI if ($prix=~/^Collect/);
      }
   }

   ENDOFCGI:
   {
      $title = shift @CondorQ;
      ($prix, $date) = split(' : ',$title);
   };

   $title = shift @CondorQ;

   while(!($title=~/^ARCH/))
   {
      $title = shift @CondorQ;
   }

   $title=~s/\// /g;
   $title=~s/\|/ /g;
   $title=~s/TOTAL/ /g;
   $title=~s/AVG/ /g;
   $title=~s/AVERAGE/ /g;

   @titleCell = split('\s+',$title);
   print "<table border = 3>";
   print "<tr>";
   while(@titleCell)
   {
      $cell = shift @titleCell;
      print "<th>$cell";
   }
   print "</tr>";

   SUMM1:
   while(@CondorQ)
   {
      $line = shift @CondorQ;
      goto SUMM1 unless ($line=~/\w+/);
      $line=~s/\// /g;
      $line=~s/\|/ /g;

      $line=~s/Total/Total ------/;
      @linea = split('\s+',$line);
      print "<tr>";
      while(@linea)
      {
         $cell = shift @linea;
         print "<td>$cell</td>";
      }
      print "</tr>";
   }

   print "</table>";

#   print $errorMsg;
#   print "<hr>";
#   print "Information collected by $collector at $date";
EXITT:
	{
#	   print "<p>Output generated by command: $cmdall";
#   	   print "<a href=\"condor_status.pl\"><h3>
#         condor_status</h3></a>";
	}
}



