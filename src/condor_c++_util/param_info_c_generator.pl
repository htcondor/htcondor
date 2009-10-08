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
		version => '7.1.0',
	};
	# call reconstitute on the params to do all of the outputting. 
	reconstitute($params, $default) unless $options{on_the_fly};
	# The reason that it is not called if on_the_fly is set is because
	# on_the_fly will cause &parse to call many reconstitute commands 
	# "on the fly" as it parses the string. If it were called anyway, 
	# then it would end up repeating output.  

	# hack for our build system
	# This #includes param_info_init.c
	`touch param_info.c`;
}
##########################################################################
use strict;
use warnings;
no warnings 'closure';
use Data::Dumper;
use Getopt::Std;

# Global variables. The first three are used internally by &parse, 
# and the %options is set immediately after execution with the command 
# line options for easy access, as specified in &configure.
use vars qw($remaining_text $parameters $current_parameter %options);
# You may be surprised to see $remaining_text, $parameters, and 
# $current_parameter listed here as global variables, even though they 
# are used exclusively by the &parse sub. While it probably isn't as 
# clean as it (c|sh)ould be, it ended up being a step in the simplest 
# solution to making recursive calls to &parse function as expected, 
# due to a variety of subtleties involving scoping in subs contained 

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
	ASSIGNMENT => ['\:?\=?','assignment operator'],
	ASSIGNMENT_EQUALS => ['\=?','assignment equals operator'],
	ASSIGNMENT_COLON => ['\:?','assignment colon operator'],
	ASSIGNMENT_HEREDOC => ['[A-Za-z]+', 'heredoc deliminator'],
	PARAMETER_TITLE => ['[a-zA-Z0-9_\.]+','parameter title'],
	PROPERTY_NAME => ['[a-zA-Z0-9_-]+','property name'],
	PROPERTY_VALUE => ['[^\n]+','property value'],
	DATACLASS_NAME => ['[a-zA-Z0-9_-]+','dataclass name'],
	OPEN_BRACKET => ['\[','open bracket'],
	CLOSE_BRACKET => ['\]','close bracket'],
	OPEN_PARENTHESIS => ['\(', 'open parenthesis'],
	CLOSE_PARENTHESIS => ['\)','close parenthesis'],
};

##################################################################################
# This is the template to be used when substituting for the parameters properties. 
# The string that is substituted is in the format of %property%, where property is 
# the name of the property to be substituted. 
# (property types and names are defined farther below in $property_types) 
##################################################################################
use constant { RECONSTITUTE_TEMPLATE => 
'param_info_insert(%parameter_name%, %aliases%, %default%, %version%, %range%,
                  %state%, %type%, %is_macro%, %reconfig%, %customization%,
				  %friendly_name%, %usage%,
				  %url%,
				  %tags%);
'
};

##################################################################################
# $property_types customizes the type and options of the properties. Each property is 
# pointing toward a hash, containing the following metadata:
#      type      =>   (String specifying the type of that property. Types are defined in 
#                       the $type_subs variable below)
#      optional  =>   (Set this to 1 to make this property optional.)
#      dont_trim =>   (Set this to 1 to not trim trailing whitespace on value.) 
##################################################################################
my $property_types = {
	parameter_name 	=> { type => "char[]" },
	default 		=> { type => "char[]", dont_trim => 1  },
	friendly_name 	=> { type => "char[]" },
	type 			=> { type => "param_type" },
	state 			=> { type => "state_type" },
	version 		=> { type => "char[]",	optional => 1 },
	tags 			=> { type => "char[]" },
	usage 			=> { type => "char[]" },
#	id 				=> { type => "int", optional => 1}, 
	aliases 		=> { type => "char[]", optional => 1 },
	range 			=> { type => "char[]", optional => 1 },
	is_macro 		=> { type => "is_macro_type", optional => 1 },
	reconfig 		=> { type => "reconfig_type", optional => 1 },
	customization	=> { type => "customization_type", optional => 1 },
	url				=> { type => "char[]", optional => 1 }
};

##################################################################################
# $type_subs tells this script how to treat all the different types of parameters
# Each sub takes the value as an argument and returns the properly formatted value.
# It should be formatted such that it can be inserted without problem in the 
# RECONSTITUTE_TEMPLATE.
# Also, it should be in charge of dieing if it encounters a bad value.
# When writing these subs, you have the following subs available:
#  escape( $ ): takes one argument, escapes all potentially problematic characters.
#  enum($, @_ ...):  The first argument should be the input value. The remaining 
#                    arguments should be acceptable values. If will try to 
#                    (case-insensitively) match the user input with the remaining
#                    acceptable values. If it cannot find a match, it will die.
#                    Otherwise, it will correct the capitalization.
#  type_error($, $): Dies with a nice error message. The first argument should be 
#                    the value, the second the type.
##################################################################################
my $type_subs = { 
	'char[]'  => sub { return $_[0]?'"'.escape($_[0]).'"':'""'; },
	'bool'  => sub { return enum($_[0],'true','false'); },
	'int'  => sub { return $_[0]=~/^\d+$/?$_[0]:type_error($_[0], 'int'); },
	'float'  => sub { return $_[0]=~/^\d+\.\d+$/?$_[0]:type_error($_[0], 'float'); },
	'double'  => sub { return $_[0]=~/^\d+\.\d+$/?$_[0]: type_error($_[0], 'double');},
	'char'  => sub { return $_[0]=~/^\d+$/ and $_[0]<256 ? $_[0]:type_error($_[0], 'char');},
	'state_type'  => sub {
		my $state = enum($_[0],'USER','AUTODEFAULT','DEFAULT', 'RUNTIME');
		return "STATE_".$state;
	},
	'param_type' => sub { 
		my $type = enum($_[0],'STRING','INT','BOOL', 'DOUBLE');
		return "PARAM_TYPE_".$type;
	},
	'is_macro_type' => sub {
		my $is_macro = enum($_[0],'true','false');
		return ($is_macro =~ /true/) ? 1 : 0;
	},
	'reconfig_type' => sub {
		my $reconfig = enum($_[0],'true', 'false');
		return ($reconfig =~ /true/) ? 1 : 0;
	},
	'customization_type' => sub {
		my $customization = enum($_[0], 'NORMAL', 'SELDOM', 'EXPERT');
		return "CUSTOMIZATION_".$customization;
	},
};

###############################################################################################
# The reconstitute function takes a hash of parameters as its first argument, and a default 
# parameter structure as its second. The hash of parameters should be in the same format as 
# the one that is generated by the &parse function. The default parameters should be a hash,   
# with the keys being property names and the values being the actual default property 
# values.
# Possible TODO: Allow &parse to load default structure from a magic "[_default]" parameter.
sub reconstitute {
	my $structure = shift;
	my $default_structure = shift; 
	my $output_filename = $options{output};
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
	############################################################################
	# replace_by_hash takes a hash as the first argument and a string as a second.
	# It then replaces all keys contained in the hash by their respective values. 
	sub replace_by_hash {
		my ($replace,$context) = @_;
		while(my($key, $val) = each %{$replace} ) {
			$key =~ s/\W/\\$&/mg;
			$context =~ s/$key/$val/ ;
		}
		return $context;
	}
	# param_err is just a simple wrapper for errors encountered.
	sub param_err {
		print STDERR $_[0]."\n\t\t" and die unless($options{ignore});
	}
	#####################################################################
	# do_one_property
	# This function gets the correct replacement value of one property.
	# It is called with a ref to the parameters substructure, with the 
	# type (ie, char[], int, etc) of the property, and with the name 
	# of the property. If it cannot find the property, it will return 
	# the default value instead.
	sub do_one_property {
		# $s is a ref to the structure of this parameter (ie, {name=>'foo',usage=>'bar'}) 
		# $i is the metadata of the field (ie, {type=>'char[]',optional=1})
		# $p is the name of the property (ie, 'friendly_name')
		my ($s,$i,$p) = @_;
		##############################################################################
		# escape and enum are two functions useful for subs contained in type_subs.  #
		# They assist with common user input formatting needs.                       #
		##############################################################################
		# type_error generates a nice error message for wrong types  
		sub type_error {
			my ($value, $type) = @_;
			param_err("PARAMETER TYPE ERROR: '$value' is not a valid type $type.");
		}
		# escape will escape various control characters from a string so that it 
		# can be safely used in quotes in C code. 
		sub escape {
			my $input = shift;
			return $input unless $input;
			# trim trailing whitespace
			if (exists($i->{dont_trim})) {
				$input =~ s/\s+$// if $i->{dont_trim} != 1;
			}
			$input =~ s/\\/\\\\/g;
			$input =~ s/\n/\\n/g;
			$input =~ s/\t/\\t/g;
			$input =~ s/\r/\\r/g;
			$input =~ s/\f/\\f/g;
			$input =~ s/'/\\\'/g;
			$input =~ s/"/\\\"/g;
			$input =~ s/\?/\\\?/g;
			return $input;
		}
		# The first argument of enum is a user inputted value that is matched 
		# in a case-insensitive manner with the remaining arguments. If there is 
		# a match, then it returns the match, using the capitalization of the 
		# latter argument. If there is not a match, it will explode with an error.
		sub enum {
			my $c = shift;
			my @list = @_;
			foreach (@list) { return $_ if lc($c) eq lc($_); } 
			return param_err("$p isn't valid ".$i->{type}.". Use one of '@_' instead of $c.");
		}
		# All the logic in this function is contained in the line below. It calls the 
		# type_sub for proper type, with either the param's value for that property, 
		# or the default value for that property (if the param does not contain that 
		# property).
		return $type_subs->{$i->{type}}(exists $s->{$p} ? $s->{$p} : $default_structure->{$p} );
	}
	#####################################################################
	
	# Here we have the main logic of this function.
	begin_output(); # opening the file, and beginning output

	# Loop through each of the parameters in the structure passed as an argument
	while(my ($param_name, $sub_structure) = each %{$structure}){

		my %replace=();
		# Quickly add the pseudo-property "parameter_name" for the name of the 
		# parameter, so that it can be treated just like any other property.
		$sub_structure->{'parameter_name'} = $param_name;
		print Dumper($sub_structure) if $options{debug};
		# Loop through each of the properties in the hash specifying property 
		# rules. (This hash is defined at the top of this file and it details 
		# how every property should be treated).
		while(my($name, $info) = each %{$property_types}){
			# unless the $sub_structure contains the property or if that property
			# is optional, summon an error. 
			unless(defined $sub_structure->{$name} or $info->{'optional'}){
				param_err ("$param_name does not have required property $name.");}
			# Get the property value; procesed, formatted, and ready for insertion
			# by do_one_property().
			$replace{"%$name%"}=do_one_property($sub_structure,$info,$name); 

			# TYPECHECK: certain parameters types must have a non-empty default
			if ($name eq "type")
			{
				# Integer parameters
				if ($type_subs->{$info->{type}}(exists $sub_structure->{type} ? $sub_structure->{type} : $default_structure->{type}) eq "PARAM_TYPE_INT")
				{
					if ($sub_structure->{'default'} eq "") {
						print "ERROR: Integer parameter $param_name needs " .
								"a default!\n";
					}
				}

				# Boolean parameters
				if ($type_subs->{$info->{type}}(exists $sub_structure->{type} ? $sub_structure->{type} : $default_structure->{type}) eq "PARAM_TYPE_BOOL")
				{
					if ($sub_structure->{'default'} eq "") {
						print "ERROR: Boolean parameter $param_name needs " .
								"a default!\n";
					}
				}

				# Double parameters
				if ($type_subs->{$info->{type}}(exists $sub_structure->{type} ? $sub_structure->{type} : $default_structure->{type}) eq "PARAM_TYPE_DOUBLE")
				{
					if ($sub_structure->{'default'} eq "") {
						print "ERROR: Double parameter $param_name needs " .
								"a default!\n";
					}
				}
			}
		}

		# Here we actually apply the template and output the parameter.
		continue_output(replace_by_hash(\%replace, RECONSTITUTE_TEMPLATE));
	}

	# wrap things up. 
	end_output();
}

########################################################################## 
# &parse parses a string. It is totally self-contained, using no outside functions (although 
# it does use the character type constants such as PARAMETER_NAME defined in the top of this 
# file). It accepts a string as its only argument, and returns a hash structure. No attempt 
# is made (in *this* function) to check any of the data; it ONLY parses strings into more 
# readable formats. 
#  The following string...
#   -  -  -  -  -  -  -  -  -  -
#   [TEST_PARAM]
#   # Comment, I am ignored
#   var1 = vala
#   var2 = valb
#
#   [NEXT_PARAM]
#   var1 = blah a
#   var2 : EOF
#   multiline string line 1
#   line 2
#   EOF
#   recursive_structure : (classname) EOF
#       sub_val1 = 1
#       sub_val2 = 2
#   EOF
#   -  -  -  -  -  -  -  -  -  -
#  ...would be parsed into...
#   -  -  -  -  -  -  -  -  -  -
# {
#    NEXT_PARAM => { var1 => 'blah a', var2 => "multiline string line 1\nline 2"
#       recursive_structure=>{ '_dataclass'=>'classname', sub_val1=>'1', sub_val2=>'2' } 
#    },
#    TEST_PARAM => { var1 => 'vala', var2 => 'valb'}
# }
#   -  -  -  -  -  -  -  -  -  -
########################################################################## 
sub parse {
	# TODO:
	# it would be be best if WHITESPACE and COMMENT types were 
	# combined, as anywhere there is a WHITESPACE ignored, comments should be 
	# ignored also.
	
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
		my ($property, $value, $dataclass) = @_;
		if ($dataclass) {
			# TODO: the [FOOBAR] thing is sloppy. It is only added to make 
			# the recursive call to &parse happy with the text and parse it.
			# Actually, this entire block is rather sloppy...
			$value = "[FOOBAR]$value";
			# All of the $h_.+ type variables below are used to temporarily "freeze"
			# the global variables in the execution stack, so that calling &parse 
			# recursively below will function as expected. It's pretty messy, 
			# but it works for now at least...
			# TODO: Very sloppy 
			my $h_remaining_text = $remaining_text;
			my %h_current_parameter = %{$current_parameter};
			my %h_parameters = %{$parameters};
			$remaining_text = $value; # reassigning $remaining_text to equal $value
			$value = parse("$value")->{'FOOBAR'}; # actual parse call
			$value->{'_dataclass'} = $dataclass;
			$remaining_text = $h_remaining_text;
			$current_parameter = \%h_current_parameter;
			$parameters = \%h_parameters;
		}
		$current_parameter->{$property} = $value;
	}
	# add_parameter is called after a parameter is added. It resets $current_parameter.
	# It then adds $current_parameter to the %parameters hash.
	# If on_the_fly is set to 1, it will call reconstitute on the parameter right away.
	sub add_parameter {
		my ($title) = @_;
		$parameters->{$title} = $current_parameter;
		reconstitute({"$title"=>$current_parameter}) if $options{on_the_fly};
		$current_parameter = {};
	}

	#################################################################
	# Actual parser logic contained here...                         #
	#################################################################
	&ignore(WHITESPACE); # First, ignore all whitespace and comments
	&ignore(COMMENTS);
	&ignore(WHITESPACE);
	while(length($remaining_text)>1){ ### Main loop, through the entire text
		# We first get the name of the next parameter, enclosed in brackets
		&accept(OPEN_BRACKET);
		my $parameter_title = &accept(PARAMETER_TITLE);
		&accept(CLOSE_BRACKET);
		&ignore(WHITESPACE);
		&ignore(COMMENTS);
		&ignore(WHITESPACE);
		until(&next_is(OPEN_BRACKET)){
			# Now we get all of its properties, looping through until we hit the 
			# next parameter definition.
			if(length($remaining_text)<1){ last; } # End of file 
			# Get the property name...
			my $property_name = &accept(PROPERTY_NAME);
			&ignore(WHITESPACE);
			my $assignment = &accept(ASSIGNMENT);
			# Get the assignment operator
			my ($property_value, $dataclass_name);
			if($assignment eq '=') {
				# If it is an equals sign (normal assignment)...
				&ignore(SPACES);
				$property_value = "" if &next_is(LINEBREAK);
				$property_value = &accept(PROPERTY_VALUE) unless &next_is(LINEBREAK);
				&ignore(LINEBREAK);
			} else {
				# If it is a colon (multiline and special 
				# dataclass assignment, such as for roles)...
				&ignore(SPACES);
				if(&next_is(OPEN_PARENTHESIS)){
					# This means that it is NOT simply a multiline string, 
					# but rather a dataclass (such as, default : (role) EOF) 
					&accept(OPEN_PARENTHESIS);
					&ignore(SPACES);
					$dataclass_name = &accept(DATACLASS_NAME);
					&ignore(SPACES);
					&accept(CLOSE_PARENTHESIS);
					&ignore(SPACES);
				}
				# This code grabs heredoc delimiter, and then the text until 
				# the heredoc delimiter. It will be used for both multiline 
				# strings and dataclass assignments.
				my $heredoc = &accept(ASSIGNMENT_HEREDOC);
				&ignore(SPACES);
				&accept(LINEBREAK);
				my $heredoc_charclass = ['\r?\n'.$heredoc.'\r?\n', $heredoc];
				$property_value = &until($heredoc_charclass);
				&ignore($heredoc_charclass);
			}
			# add_property will add the newly created property to 
			# @current_parameter. If it is a single or multiline string, it 
			# will simply set the new parameter to equal the string.
			# However, if $dataclass is longer than 0 characters, it will 
			# attempt to parse the string.
			add_property($property_name, $property_value, $dataclass_name);
			ignore(WHITESPACE);
			&ignore(COMMENTS);
			&ignore(WHITESPACE);
			if(length($remaining_text)<1){ last; } # End of file 
		}
		# add_parameter will add @current_parameter (the parameter implicitly 
		# constructed with add_property) to the hash $parameters. If on_the_fly
		# is set, it will also call the reconstruct function on this structure
		# and output the results on the fly.
		add_parameter($parameter_title);
	}
	return $parameters;
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
		['f',	0,   'on_the_fly', 0,		                'output the result as it is parsed'],
		['i',	1,   'input',	   'param_info.in',         'input file (default: "param_info.in")'],
		['o',	1,   'output',	   'param_info_init.c',     'output file (default: "param_info.c")'],
		['I',	0,   'stdin',      0,                       'input from standard in instead of file'],
		['O',	0,   'stdout',     0,                       'print to standard out instead of file'],
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
