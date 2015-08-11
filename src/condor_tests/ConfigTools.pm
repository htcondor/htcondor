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

package ConfigTools;

#use CondorTest;
use base 'Exporter';

our @EXPORT = qw(GenerateMassive);

sub GenerateMassive {
	my $argcount = @_;
	my $iterations = shift;
	my $storage = shift;
	print "Current value for storage in GenerateMassive:$storage\n";
	my $counter = 0;
	my $pattern = "x";
	my $unique = "unique";
	my $genunique = "";

	#sleep(10);

	print "GenerateMassive:$iterations iterations\n";
	print "arg count into GenerateMassive:$argcount\n";

	if(defined $storage) {
		"print storing via array reference passed in\n";
	} else {
		"Why are we missing an array reference in:GenerateMassive\n";
	}

	while($counter < $iterations) {
		$genunique = "$unique$counter = $pattern";
		push @{$storage},"$genunique\n";
		#print "Try to add:$genunique to config\n";
		$counter++;
		$pattern = $pattern . "xxxxxxxxxxxx";
	}
};



1;
