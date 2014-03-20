#! /usr/bin/env perl

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

##########################################################################
use strict;
use warnings;
no warnings 'closure';
use Data::Dumper;
use Getopt::Std;

# Global variables. The first four are used internally by &parse, 
# and the %options is set immediately after execution with the command 
# line options for easy access, as specified in &configure.

use vars qw($remaining_text $parameters $current_parameter %cmd_base %options);


# Here we define the syntax rules for the parser. Each character class 
# consists of an array ref of two elements, the first being a regular 
# expression meant to match at least one of those characters, the latter 
# being a string name for the character class.
# These constants will be used in the &parse function. 
use constant {
	WHITESPACE => ['[\r\n\s]+','whitespace'], # TODO: combine these into 
	COMMENTS => ['\#[^\n]+\n', 'comments'],
	LINEBREAK => ['\n\r?','linebreak'],
	SPACES => ['[ ]+','spaces'],
	LINE => ['[^\n]*\n\r?','line'],
};

###############################################################

sub parse {

	# get argument and put into $remaining_text
	$remaining_text = shift;
	# reset current_parameter and parameters variables
	$current_parameter = {};
	$parameters = {};

	#########################################################
	# Main parser logic functions                           #
	#########################################################
	sub error { die ("ERROR! Expected valid ".$_[0]." at '".$_[1]."'\n"); }
	# ignore will ignore the supplied character class
	sub ignore {
		my $regex = $_[0]->[0];
		$remaining_text =~ s/\A$regex//s;
	}
	# next_is will look ahead and return true if the next character(s) match
	# the given chracter class
	sub next_is {
		my $regex = $_[0]->[0];
		$remaining_text =~ /^($regex)/s;
		return $1 && length($1) > 0;
	}
	# not_ignore is used by &accept and &until. It requires that the next characters 
	# be of the supplied regex and will return them, otherwise erroring.
	sub not_ignore {
		my ($regex, $context, $chartype) = @_;
		($context ? $context : $remaining_text) =~ s/$regex//s;
		return (length($1)>0) ? $1 : error($chartype, substr($remaining_text,0,90));
	}
	# accept will require that the next characters be of the supplied character class,
	# returning the matching string.
	sub accept { return not_ignore("^(".$_[0]->[0].")", $_[1], $_[0]->[1]); }
	# until will require that there be somewhere ahead the supplied character class, and 
	# will return the text leading up to that supplied class
	sub until { return not_ignore("^(.*?)(?=".$_[0]->[0].")", $_[1], $_[0]->[1]); }

	##########################################
	# Array building functions here          #
	##########################################
	# add_property will add a property to $current_parameter. It is called with the 
	# property name, the property value, and the dataclass name (only if there is one. 
	# The dataclass name is not specified for normal strings or normal multiline strings).
	sub add_property {
		my ($property, $value) = @_;
		$current_parameter->{$property} = $value;
	}
	# add_parameter is called after a parameter is added. It resets $current_parameter.
	# It then adds $current_parameter to the %parameters hash.
	# If on_the_fly is set to 1, it will call reconstitute on the parameter right away.
	sub add_parameter {
		my ($title) = @_;
		$parameters->{$title} = $current_parameter;
		# reconstitute({"$title"=>$current_parameter}) if $options{on_the_fly};
		$current_parameter = {};
	}
	
	#################################################################
	# Actual parser logic contained here...                         #
	#################################################################

	my $section=0;
	my $section_base=0;
	my $parse_const_int=0;

	### Main loop, through the entire text
	while(length($remaining_text)>1) {
		my $line = &accept(LINE);

		# parse NAMETABLE_DIRECTIVE commands, which control the parsing
		if ( $line =~ /^\s*NAMETABLE_DIRECTIVE:/ ) {
			if ( $line =~ /:BEGIN_SECTION:([a-zA-Z_]*)/ ) {
				$section = $1;
			} elsif ( $line =~ /:END_SECTION:([a-zA-Z_ ]*)/ ) {
				$section = "";
				$parse_const_int = 1;
			} elsif ( $line =~ /:PARSE:([a-zA-Z_ ]*)/ ) {
				if ($1 =~ /const int/) { $parse_const_int = 1; }
			} elsif ( $line =~ /:BASE:([0-9]+)/ ) {
				$section_base = $1;
				$cmd_base{$section} = $1;
			} elsif ( $line =~ /:CLASS:([a-zA-Z_0-9]+)/ ) {
				$options{class} = $1;
			} elsif ( $line =~ /:TABLE:([a-zA-Z_0-9]+)/ ) {
				$options{table} = $1;
			}
		}
		#define CONTINUE_CLAIM		(SCHED_VERS+1)
		elsif ( $line =~ /^(\/*)#define\s+([A-Z_]+)\s+\(([A-Z_]+)\+([0-9]+)\)\s*([^\n]*)/ ) {
			#print "$line\n";
			add_property('unused',$1);
			add_property('base',$3);
			add_property('offset',$4);
			add_property('remain',$5);
			add_parameter($2);
		}
		#const int UPDATE_STARTD_AD		= 0;
		elsif ( $line =~ /^(\/*)\s*const\s+int\s+([A-Z_]+)\s*=\s*([0-9]+)\s*;\s*([^\n]*)/ ) {
			if ($parse_const_int) {
				#print "$line\n";
				add_property('unused',$1);
				add_property('base',$section);
				add_property('offset',$3 - $section_base);
				add_property('remain',$4);
				add_parameter($2);
			}
		}
		#define SCHED_VERS			400
		#define HAD_COMMANDS_BASE                  (700)
		elsif ( $line =~ /\#define\s+([A-Z_]+)\s+\(?([0-9]+)\)?/ ) {
			#print "$1 = $2\n";
			$cmd_base{$1} = $2;
		}
	}

	return $parameters;
}

######################################################################

sub reconstitute {
	my $structure = shift;
	my $default_structure = shift; 
	my $output_filename = $options{output};
	my $table_name = $options{table};
	my $class_name = $options{class};

	###########################################################################
	## All of the actual file output is contained in this section.           ##
	###########################################################################
	sub begin_output {
		open REC_OUT, ($options{append}?'>':'').">$output_filename" unless $options{stdout};
		$options{append} = 1;
	}
	sub continue_output {
		if ($options{stdout}) { print $_[0]; }
		else { print REC_OUT $_[0]; }
	}
	sub end_output {
		close REC_OUT unless $options{stdout};
	}

	my @cmd_ids;
	my @cmd_names;
	my %cmd_output=();
	my %unused_cmd=();
	my %cmd_remain=();
	my %cmd_basename=();
	my %name_map=();
	my %index_map=();

	# Loop through each of the parameters in the structure passed as an argument
	while(my ($cmd_name, $sub_structure) = each %{$structure}) {
		my $unused = ""; if ($sub_structure->{'unused'}) { $unused = "//"; }
		my $base = $cmd_base{$sub_structure->{'base'}};
		my $id = $base + $sub_structure->{'offset'};
		print "$unused$cmd_name=$id ($sub_structure->{'base'} + $sub_structure->{'offset'})\n" if $options{debug};
		print Dumper($sub_structure) if $options{debug};

		push @cmd_ids, $id;
		if (exists $cmd_output{$id}) {
			print " !!! error:  command $id is multiply defined. $cmd_output{$id} == $cmd_name\n";
		}
		$cmd_basename{$id} = $sub_structure->{'base'}; $cmd_basename{$id} =~ s/_BASE$//;
		$cmd_output{$id} = $cmd_name;
		$cmd_remain{$id} = $sub_structure->{'remain'};
		if ($unused) { $unused_cmd{$id} = 1; }
		push @cmd_names, $cmd_name;
		$name_map{$cmd_name} = $id;
	}


	# Here we have the main logic of this function.
	begin_output(); # opening the file, and beginning output
	continue_output("/* !!! DO NOT EDIT THIS FILE !!!\n * generated from: $options{input}\n * by $0\n */\n");
	# continue_output("\nnamespace condor {\n");

	continue_output( "typedef struct ${class_name} {\n   int id;\n   const char * name;\n} ${class_name};\n " );
	continue_output( "\n" );

	continue_output( "const ${class_name} ${table_name}[] = {\n" );
	my $index = 0;
	my $lowest_id;
	for(sort { $a <=> $b } @cmd_ids) {
		my $comment = ""; if ($unused_cmd{$_}) { $comment = "//"; }
		continue_output( "${comment}\t{ /*$_*/ $cmd_output{$_}, \"$cmd_output{$_}\" }, $cmd_remain{$_}\n" );
		$index_map{$cmd_output{$_}} = $index;
		$index += 1 if ( ! $unused_cmd{$_});
		$lowest_id = $_ if ( ! defined $lowest_id);
	}
	continue_output( "};\nconst int ${table_name}_count = $index;\n" );

	# write out positions of the entries for each of the table sections.
	$index = 0;
	my $count = 0;
	my $last_basename = $cmd_basename{$lowest_id};
	continue_output( "// constants that refer to index ranges in the ${table_name} table above.\n" );
	continue_output( "const int ${table_name}_${last_basename}_start = $index;\n" );
	for(sort { $a <=> $b } @cmd_ids) {
		next if ($unused_cmd{$_});
		my $basename = $cmd_basename{$_};
		if ($basename ne $last_basename) {
			continue_output( "const int ${table_name}_${last_basename}_count = $count;\n" );
			continue_output( "const int ${table_name}_${basename}_start = $index;\n" );
			$count = 0;
		}
		$index += 1;
		$count += 1;
		$last_basename = $basename;
	}
	continue_output( "const int ${table_name}_${last_basename}_count = $count;\n" );

	# write out a array of indexes into the above table that is sorted by name.
	continue_output( "\n// the array below indexes ${table_name}[] (see above) by name.\n// it has the same number of elements.\n" );
	continue_output( "const int ${table_name}IndexByName[] = {\n" );
	for(sort { lc($a) cmp lc($b) } @cmd_names) {
		my $id = $name_map{$_};
		my $comment = ""; if ($unused_cmd{$id}) { $comment = "//"; }
		continue_output( "${comment}\t$index_map{$_}, /*$_*/ $cmd_remain{$id}\n" );
	}
	continue_output( "};\n" );

	# wrap things up. 
	# continue_output("\n} //namespace condor\n");
	end_output();
}

# Really simple function that just brutally gets the contents of an entire file into 
# a string.
# If, however, the option stdin is set, then it will instead get input from 
# standard in.
sub file_get_contents {
	my $file_path = shift;
	my @text;
	if ($options{stdin}){
		@text = <STDIN>;
	} else {
		open FILE_NAME, "<$file_path" or die "Cannot find $file_path...";
		@text = <FILE_NAME>;
		close FILE_NAME;
	}
	return join "", @text;
}

############################################################
# Some generic  configuration code...                      #
# This makes adding / removing new switches much easier    # 
# To add new command line options, just add them to the    #
# list @switches contained below.                          #
############################################################
sub configure  {
	my @switches = ( 
		# flag, arg, short name,   default,		            usage description
		['h',	0,   'help',	   0,		                'print this usage information'],
#		['f',	0,   'on_the_fly', 0,		                'output the result as it is parsed'],
		['i',	1,   'input',	   'condor_commands.h',     'input file (default: "condor_commands.h")'],
		['o',	1,   'output',	   'command_name_tables.h', 'output file (default: "command_name_tables.h")'],
		['I',	0,   'stdin',      0,                       'input from standard in instead of file'],
		['O',	0,   'stdout',     0,                       'print to standard out instead of file'],
		['t',	1,   'table',      'CommandNameMap',        'name of the output table'],
		['c',	1,   'class',      'BTranslation',          'class name of the output table'],
		['a',	0,   'append',     0,                       "append: don't clobber output file"],
		['e',	0,   'errors',     0,                       'do not die on some errors'],
		['d',	0,   'debug',	   0,                       0], # 0 makes it hidden on -h
	);
	sub usage {
		my $switches;
		# goes through all of the flags, generating a "help" line for each item
		foreach my $s(@_) { 
			$s->[2]=~s/_/ /g; # replace underscores
			# (the "$switch->[4] and" allows options to be disabled from display by setting the usage description to a false value)
			$s->[4] and $switches .= "\t-".$s->[0].($s->[1]?" [".($s->[2])."]\t":"\t\t")."\t".$s->[4]."\n";
		}
		print << "EOF";
Parameter Parser for Condor

Example usage: 
	perl $0 -i param_table -o output_source.C -f

Full argument list:
$switches

EOF
	}
	sub bomb { usage(@_); exit 0; }
	my %opts;
	getopts(join ('', map { $_->[0].($_->[1]?':':'') } @switches),\%opts); # get CLI options, with ':' properly specifying arguments
	$opts{'h'} and bomb(@switches);
	for my $switch (@switches) {
		if( !defined $opts{$switch->[0]} or $opts{$switch->[0]}=~/^$/ ) { # If argument was not set...
			$options{$switch->[2]} = $switch->[3]; # ...set the options value equal to the default value.
		} else { # Otherwise, set the options value equal to either the argument value, or in the case of...
			$options{$switch->[2]} = $switch->[1] ? $opts{$switch->[0]} : !$switch->[3]; # ...a flag style switch...
		} # ...instead invert the default value.
	}

}


configure();
main();

exit(0);


##########################################################################
# For information on command line options of this script, call it with -h
#
# The logic in this code is divided into two functions. They are parse()
# and reconstitute().
# parse() takes one string as an argument. It will parse the string into
# one giant associative array, and return a reference.
# reconstitute() takes
#
# Because this deals a great deal with specifying meta-information on
# parameters, comments and variable naming can be confusing. So I will
# try to use the term "property" to only refer to the meta-information,
# and the term "parameter" or "params" to refer to those parameters that
# are actually referenced in Condor code, and (will be) actually
# configured by users.


# This main sub simply specifies a default, and calls parse.
# NOTE: $option contains values of command line options. See configure()
# at the bottom of this script if you want more info.
sub main {
	# fetch contents of input file into string $input_contents
	my $input_contents = file_get_contents($options{input});
	# parse contents, and put into associative array $params
	my $params = &parse($input_contents);
	# set defaults
	my $defaults = {
	};
	# call reconstitute on the params to do all of the outputting.
	#$options{debug} = 1;
	reconstitute($params, $defaults)

}
