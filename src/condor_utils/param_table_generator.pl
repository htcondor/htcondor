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

my $warn_missing_defaults = 0; # set to 1 to generate warnings for non-string params that have no default value

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
	ASSIGNMENT_HEREDOC => ['[@_A-Za-z]+', 'heredoc deliminator'],
	PARAMETER_TITLE => ['[$a-zA-Z0-9_\.]+','parameter title'],
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

# use this template to get actual declarations for the value of a param without a default value (wasteful)
use constant { RECONSTITUTE_TEMPLATE_EMPTY_ORIG =>
'static const nodef_value def_%parameter_var% = { 0 };
'};

# this template emits a comment for the value of a param without a default value
use constant { RECONSTITUTE_TEMPLATE_EMPTY =>
'//static const %typequal%_value def_%parameter_var% = { %default%, PARAM_TYPE_%type%%range_valid%%cooked_values% };
'};

use constant { RECONSTITUTE_TEMPLATE => 
'static const %typequal%_value def_%parameter_var% = { %default%, PARAM_TYPE_%type%%range_valid%%cooked_values% };
'};

use constant { RECONSTITUTE_TEMPLATE_IFDEF => 
'static const %typequal%_value def_%parameter_var% = {
	#if %win32_ifdef%
	 %win32_default%, PARAM_TYPE_%type%%range_valid%%win_cooked_values%
	#else
	 %default%, PARAM_TYPE_%type%%range_valid%%cooked_values%
	#endif
};
'
};

# is this to pick up a value from a compiler macro
use constant { RECONSTITUTE_TEMPLATE_MACRO_STRING =>
'static const %typequal%_value def_%parameter_var% = {
	#ifdef %macro%
	 %macro%, PARAM_TYPE_%type%%range_valid%%cooked_values%
	#else
	 %default%, PARAM_TYPE_%type%%range_valid%%cooked_values%
	#endif
};
'
};

use constant { RECONSTITUTE_TEMPLATE_MACRO_QUOTED =>
'static const %typequal%_value def_%parameter_var% = {
	#ifdef %macro%
	 PARAM_TABLE_QUOTEME(%macro%), PARAM_TYPE_%type%%range_valid%, %macro%
	#else
	 %default%, PARAM_TYPE_%type%%range_valid%%cooked_values%
	#endif
};
'
};

##################################################################################
# $property_types customizes the type and options of the properties. Each property is 
# pointing toward a hash, containing the following metadata:
#      type      =>   (String specifying the type of that property. Types are defined in 
#                       the $type_subs variable below)
#      required  =>   (Set this to 1 to make this property required.)
#      dont_trim =>   (Set this to 1 to not trim trailing whitespace on value.) 
##################################################################################
my $property_types = {
	parameter_name 	=> { type => "char[]", required => 1 },
	parameter_var 	=> { type => "nodots", required => 1 },
	type 			=> { type => "param_type", def => "STRING" },
	default 		=> { type => "char[]", required => 1, dont_trim => 1 },
	win32_default	=> { type => "char[]", dont_trim => 1 },
	range 			=> { type => "char[]" },
	restart 		=> { type => "restart_type", def => 0 },
	friendly_name 	=> { type => "char[]" },
	usage 			=> { type => "char[]" },
	tags 			=> { type => "char[]" },
	aliases 		=> { type => "char[]" },
	customization	=> { type => "customization_type", def => "SELDOM" },
#	state 			=> { type => "state_type" },
#	is_macro 		=> { type => "is_macro_type" },
#	version 		=> { type => "char[]" },
#	id 				=> { type => "int"}, 
#	daemon_name     => { type => "literal" },
#	url				=> { type => "char[]" }
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
	'char[]'  => sub { return '"'.escape($_[0]).'"'; },
	'literal'  => sub { return $_[0]; },
	'bool'  => sub { return enum($_[0],'true','false'); },
	'int'  => sub { return $_[0]=~/^\d+$/?$_[0]:type_error($_[0], 'int'); },
	'long'  => sub { return $_[0]=~/^\d+$/?$_[0]:type_error($_[0], 'long'); },
	'float'  => sub { return $_[0]=~/^\d+\.\d+$/?$_[0]:type_error($_[0], 'float'); },
	'double'  => sub { return $_[0]=~/^\d+\.\d+$/?$_[0]: type_error($_[0], 'double');},
	'char'  => sub { return ($_[0]=~/^\d+$/ and $_[0]<256) ? $_[0] : type_error($_[0], 'char');},
	'state_type'  => sub {
		my $state = enum($_[0],'USER','AUTODEFAULT','DEFAULT', 'RUNTIME');
		return "STATE_".$state;
	},
	'nodots'  => sub { 
	    my $param_var = $_[0];
	    $param_var =~ s/\./_/g; 
	    return $param_var;
	},
	'param_type' => sub {
		return enum($_[0],'STRING','INT','BOOL', 'DOUBLE', 'LONG', 'PATH');
	},
	'is_macro_type' => sub {
		my $is_macro = enum($_[0],'true','false');
		return ($is_macro =~ /true/) ? 1 : 0;
	},
	'restart_type' => sub {
		return enum($_[0],'false', 'true', 'never');
	},
	'customization_type' => sub {
		my $customization = enum($_[0], 'CONST', 'COMMON', 'NORMAL', 'SELDOM', 'EXPERT', 'DEVEL', 'NEVER');
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
	my $platform = $options{platform};
	print "generating param_info for platform=[$platform]\n";
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
			$context =~ s/$key/$val/g ;
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
		# $i is the metadata of the field (ie, {type=>'char[]', required=1})
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
	continue_output("/* !!! DO NOT EDIT THIS FILE !!!\n * generated from: $options{input}\n * by $0\n */\n");
	continue_output(  "//#undef DOMAIN // because DOMAIN is a param name\n"
					. "#define PARAM_DEFAULTS_SORTED\n"
					. "#define PARAM_TABLE_QUOTEME_(s) #s\n"
					. "#define PARAM_TABLE_QUOTEME(s) PARAM_TABLE_QUOTEME_(s)"
					);
	continue_output("\nnamespace condor_params {\n");
	continue_output("\n"
		. "const int PARAM_FLAGS_TYPE_MASK = 0x0F;\n"
		. "const int PARAM_FLAGS_RANGED = 0x10;\n"
		. "const int PARAM_FLAGS_PATH = 0x20;\n"
		. "const int PARAM_FLAGS_RESTART = 0x1000;\n"
		. "const int PARAM_FLAGS_NORECONFIG = 0x2000;\n"
		. "const int PARAM_FLAGS_CONST = 0x8000;\n"
		. "const int PARAM_CUST_MASK   = 0xF0000;\n"
		. "const int PARAM_CUST_SELDOM = 0x00000;\n"
		. "const int PARAM_CUST_COMMON = 0x10000;\n"
		. "const int PARAM_CUST_NORMAL = 0x20000;\n"
		. "const int PARAM_CUST_EXPERT = 0x30000;\n"
		. "const int PARAM_CUST_DEVEL  = 0x40000;\n"
		. "const int PARAM_CUST_NEVER  = 0x50000;\n"
		. "typedef struct nodef_value { const char * psz; int flags; } nodef_value;\n"
		. "typedef struct string_value { const char * psz; int flags; } string_value;\n"
		. "typedef struct bool_value { const char * psz; int flags; bool val; } bool_value;\n"
		. "typedef struct int_value { const char * psz; int flags; int val; } int_value;\n"
		. "typedef struct long_value { const char * psz; int flags; long long val; } long_value;\n"
		. "typedef struct double_value { const char * psz; int flags; double val; } double_value;\n"
		. "typedef struct ranged_int_value { const char * psz; int flags; int val; int min; int max; } ranged_int_value;\n"
		. "typedef struct ranged_long_value { const char * psz; int flags; long long val; long long min; long long max; } ranged_long_value;\n"
		. "typedef struct ranged_double_value { const char * psz; int flags; double val; double min; double max; } ranged_double_value;\n"
		. "typedef struct key_value_pair { const char * key; const nodef_value * def; } key_value_pair;\n"
		. "typedef struct kvp_value { const char * key; int flags; const key_value_pair * aTable; int cElms; } kvp_value;\n"
		. "typedef struct key_table_pair { const char * key; const key_value_pair * aTable; int cElms; } key_table_pair;\n"
		. "typedef struct ktp_value { const char * label; int flags; const key_table_pair * aTables; int cTables; } ktp_value;\n"
		. "\n");
	
	# everything after this will be inside ifdef PARAM_DECLARE_TABLES.
	continue_output("// declare param defaults and mapping tables\n#ifdef PARAM_DECLARE_TABLES\n\n");
	
	my @var_names;
	my %empty_vars=();
	my %var_daemons=(); # hash of arrays of per-daemon/sybsystem parameters overrides.
	my %var_metas=(); # hash of arrays of metaknobs
	my %var_help=(); # hash of help info
	
	# Loop through each of the parameters in the structure passed as an argument
	while(my ($param_name, $sub_structure) = each %{$structure}){

		# parm names of the form "subsys.param" are set aside into ${var_daemons}{$subsys}
		# rather than being stored in @var_names
		#
		my $daemon_prefix = "";
		if ($param_name =~ /\./) {
			my @aaa = split(/\./, $param_name);
			# print "$aaa[0] of $aaa[1]\n";

			# param subsys is case insensitive, so covert subsys to uppercase before using as a key.
			my $subsys = uc($aaa[0]);
			$daemon_prefix = $subsys.'$';
			$param_name = $aaa[1];
			if ($subsys =~ /^\$(.*)/) {
				# subsystem names that start with $ are really meta knobs.
				# keep track of them in a different hashtable.
				my $meta = $1;
				if (defined $var_metas{$meta}) {
					push @{$var_metas{$meta}}, $param_name;
				} else {
					my @meta_var_names;
					push @meta_var_names, $param_name;
					$var_metas{$meta} = \@meta_var_names;
				}
			} else {
				if (defined $var_daemons{$subsys}) {
					push @{$var_daemons{$subsys}}, $param_name;
				} else {
					my @daemon_var_names;
					push @daemon_var_names, $param_name;
					$var_daemons{$subsys} = \@daemon_var_names;
				}
			}
		} else {
			push @var_names, $param_name;
		}
		
		my $var_name = $daemon_prefix.$param_name;
		my %replace=();
		# Quickly add the pseudo-property "parameter_name" for the name of the 
		# parameter, so that it can be treated just like any other property.
		$sub_structure->{'parameter_name'} = $param_name;
		$sub_structure->{'parameter_var'} = $var_name;
		#if ( ! exists $sub_structure->{type}) { $sub_structure->{type} = $default_structure->{type}; }
		
		# check for platform specific default, right now we support only one of these.
		my $plat = "win32";
		foreach my $key (keys %{$sub_structure}) {
			if ($key =~ /^([a-zA-Z_]+[a-zA-Z0-9_]*)_default/) {
				$plat = $1;
				my $isplat = 0; if ($platform =~ $plat) { $isplat = 1; }
				if ($plat !~ /win32/) {
					print "found platform default key=[$key] plat=[$plat] $isplat def=$sub_structure->{$key}\n";
				}
			}
		}

		my $typequal = "nodef";
		my $cooked_values = "";
		my $win_cooked_values = "";
		my $typequal_ranged = "";
		my $cooked_range = "";
		my $range_max = "";
		my $nix_default = $sub_structure->{'default'};
		my $win_default = $sub_structure->{"${plat}_default"};
		my $def_valid = (defined $nix_default && $nix_default ne "") ? "1" : "0";
		my $win_valid = (defined $win_default && $win_default ne "") ? "1" : "0";
		my $range_valid = "";
		
		
		# print "$var_name has ${plat}_default=$win_default\n" if $win_valid eq "1";
		
		print Dumper($sub_structure) if $options{debug};
		
		# Loop through each of the properties in the hash specifying property 
		# rules. (This hash is defined at the top of this file and it details 
		# how every property should be treated).
		while(my($name, $info) = each %{$property_types}) {
			# generate an error for required properties that are not defined
			unless(defined $sub_structure->{$name} or ! $info->{'required'}){
				param_err ("$param_name does not have required property $name.");
			}
			# Get the property value; procesed, formatted, and ready for insertion
			# by do_one_property().
			if (defined $sub_structure->{$name}) {
				$replace{"%$name%"} = do_one_property($sub_structure,$info,$name);
			} elsif (defined $default_structure->{$name}) {
				$replace{"%$name%"} = $default_structure->{$name};
			} else {
				$replace{"%$name%"} = "";
			}

			# TYPECHECK: certain parameters types must have a non-empty default
			# this is also where we set convert string default value to int or double as needed
			# and decide whether to set the default_valid flag or not.
			if ($name eq "type")
			{
				$typequal = do_one_property($sub_structure,$info,$name); 
				#print "$typequal from do_one_property of $info, $name\n";

				# Integer parameters
				my $typeT = $type_subs->{$info->{type}}(exists $sub_structure->{type} ? $sub_structure->{type} : $default_structure->{type});
				if ($typeT eq "INT" || $typeT eq "LONG")
				{
					$range_max = "INT_MAX";
					$cooked_values = $nix_default;
					if ($cooked_values =~ /^[0-9\-\*\/\(\) \t]*$/) {
						$def_valid = "1";
					} else {
						#print "$param_name default is expression $cooked_values\n";
						$cooked_values = "0";
						$def_valid = "0";
					}

					if (defined $win_default)
					{
						$win_cooked_values = $win_default;
						if ($win_cooked_values =~ /^[0-9\-\*\/\(\) \t]*$/) {
							$win_valid = "1";
						} else {
							#print "$param_name default is expression $win_cooked_values\n";
							$win_cooked_values = "0";
							$win_valid = "0";
						}
					}

					if ($nix_default eq "") {
						print "WARNING: Integer parameter $param_name has no default\n" if ($warn_missing_defaults);
					}
					# print "$var_name cooked is $cooked_values\n";
				}

				# Boolean parameters
				if ($typeT eq "BOOL")
				{
					$cooked_values = $nix_default;
					if ($cooked_values =~ /^[ \t]*TRUE|FALSE|true|false|0|1[ \t]*$/) {
						$def_valid = "1";
					} else {
						#print "$param_name default is expression $cooked_values\n";
						$cooked_values = "0";
						$def_valid = "0";
					}
					if (defined $win_default)
					{
						$win_cooked_values = $win_default;
						if ($win_cooked_values =~ /^[ \t]*TRUE|FALSE|true|false|0|1[ \t]*$/) {
							$win_valid = "1";
						} else {
							#print "$param_name default is expression $win_cooked_values\n";
							$win_cooked_values = "0";
							$win_valid = "0";
						}
					}
					if ($nix_default eq "") {
						print "WARNING: Boolean parameter $param_name has no default\n" if ($warn_missing_defaults);
					}
				}

				# Double parameters
				if ($typeT eq "DOUBLE")
				{
					$range_max = "DBL_MAX";
					$cooked_values = $nix_default;
					if ($cooked_values =~ /^[0-9\.\-eE+\*\/\(\) \t]*$/) {
						$def_valid = "1";
					} else {
						#print "$param_name default is expression $cooked_values\n";
						$cooked_values = "0";
						$def_valid = "0";
					}				    
					if (defined $win_default)
					{
						$win_cooked_values = $nix_default;
						if ($win_cooked_values =~ /^[0-9\.\-eE+\*\/\(\) \t]*$/) {
							$win_valid = "1";
						} else {
							#print "$param_name default is expression $win_cooked_values\n";
							$win_cooked_values = "0";
							$win_valid = "0";
						}				    
					}
					if ($nix_default eq "") {
						print "WARNING: Double parameter $param_name has no default.\n" if ($warn_missing_defaults);
					}
				}

				# Path parameters
				if ($typeT eq "PATH")
				{
					$replace{"%type%"} = "STRING";
					$typequal = "string";
					if ( ! defined $win_default)
					{
						$win_valid   = $def_valid;
						$win_default = $nix_default;
						if (defined $win_default) { $win_default =~ s/\//\\/g; }
					}
					$range_valid = "|PARAM_FLAGS_PATH";
				}
			}

			# convert ranges from string to int or double if we can
			# if range can be set a compile time, then we need to emit a xxx_ranged
			# structure and two aditional data values. plus we set the 
			# range_valid flag.
			#
			if ($name eq "range")
			{
				my $range_raw = ".*";
				if (exists $sub_structure->{'range'}) {
				   $range_raw = $sub_structure->{'range'};
				}
				   
				if ($range_raw ne ".*")
				{
					if ($range_raw =~ /^[0-9\.\-eE+, \t]*$/)
					{
						#print "$param_name range is numeric $range_raw\n";
						$typequal_ranged = "ranged_";	
						$cooked_range = ", ".$range_raw;
						$range_valid = "|PARAM_FLAGS_RANGED";
					}
					else
					{
						#print "$param_name range is expression $range_raw\n";
					}
				}
			}

		} # bake based on %property_types rules 
		
		if (exists $sub_structure->{'restart'}) {
			if ($sub_structure->{'restart'} eq 'true') { $range_valid .= "|PARAM_FLAGS_RESTART"; }
			elsif ($sub_structure->{'restart'} eq 'never') { $range_valid .= "|PARAM_FLAGS_NORECONFIG"; }
		}
		if (exists $sub_structure->{'customization'}) {
			if ($sub_structure->{'customization'} eq 'const') {
				$range_valid .= "|PARAM_FLAGS_CONST";
			} elsif ($sub_structure->{'customization'} eq 'expert') {
				$range_valid .= "|PARAM_CUST_EXPERT";
			} elsif ($sub_structure->{'customization'} eq 'devel') {
				$range_valid .= "|PARAM_CUST_DEVEL";
			} elsif ($sub_structure->{'customization'} eq 'never') {
				$range_valid .= "|PARAM_CUST_NEVER";
			}
		}

		# if cooked_range ends in a , then the max value is missing, so
		# append $range_max
		#
		if ($cooked_range =~ /,$/) {
			$cooked_range = $cooked_range.$range_max;
			#print "$param_name range is $cooked_range\n";
		}
		
		$replace{"%def_valid%"} = $def_valid;
		$replace{"%range_valid%"} = $range_valid;
		$replace{"%cooked_values%"} = "";
		if (exists $sub_structure->{'macro'}) { $replace{"%macro%"} = $sub_structure->{'macro'}; }
		
		my $cooked = $cooked_values.$cooked_range;
		if (length $cooked) { $replace{"%cooked_values%"} = ", ".$cooked; }
		$replace{"%typequal%"} = lc($typequal_ranged.$typequal);

		# Here we actually apply the template and output the parameter.
		if (defined $win_default) {
		    $replace{"%win32_default%"} = '"'.escape($win_default).'"';
			$replace{"%win_cooked_values%"} = "";
			if ($plat =~ /^win32$/i) {
				$replace{"%win32_ifdef%"} = "defined WIN32";
			} elsif ($plat =~ /^x64$/i) {
				$replace{"%win32_ifdef%"} = "defined X86_64";
			} else {
				if ($platform =~ $plat) {
					$replace{"%win32_ifdef%"} = "1 // PLATFORM =~ \"${plat}\"";
				} else {
					$replace{"%win32_ifdef%"} = "0 // PLATFORM =~ \"${plat}\"";
				}
			}
			$cooked = $win_cooked_values.$cooked_range;
			if (length $cooked) { $replace{"%win_cooked_values%"} = ", ".$cooked; }
			continue_output(replace_by_hash(\%replace, RECONSTITUTE_TEMPLATE_IFDEF));
		} elsif (length $replace{"%default%"} > 2) {
			if (defined $replace{"%macro%"}) {
				if ($replace{"%type%"} eq "STRING") {
					continue_output(replace_by_hash(\%replace, RECONSTITUTE_TEMPLATE_MACRO_STRING));
				} else {
					continue_output(replace_by_hash(\%replace, RECONSTITUTE_TEMPLATE_MACRO_QUOTED));
				}
			} else {
				continue_output(replace_by_hash(\%replace, RECONSTITUTE_TEMPLATE));
			}
		} else {
			continue_output(replace_by_hash(\%replace, RECONSTITUTE_TEMPLATE_EMPTY));
			$empty_vars{$var_name} = 1;
		}

		my $descrip = "";   if (exists $sub_structure->{'description'}) { $descrip .= $sub_structure->{'description'}; }
		my $help_tags = ""; if (exists $sub_structure->{'tags'}) { $help_tags .= $sub_structure->{'tags'}; }
		my $usage = ""; 	if (exists $sub_structure->{'usage'}) { $usage .= $sub_structure->{'usage'}; }
		$replace{"%help_strings%"} = $descrip . '\0' . $help_tags . '\0' . $usage;
		$var_help{$var_name} = replace_by_hash(\%replace, 'PARAM_TYPE_%type%%range_valid%, "%help_strings%\0"');
	}

	# output a sorted key/value table with param names and pointers to their default values
	# we will use this to do a binary lookup of the parameter by name.
	#
	continue_output("\n/*======================\n"
					. " * global defaults[]\n"
					. " *=====================*/\n");
	continue_output("const key_value_pair defaults[] = {\n");
	for(sort { lc($a) cmp lc($b) } @var_names) {
		if ($empty_vars{$_}) {
			continue_output("\t{ \"$_\", 0 },\n");
		} else {
			continue_output("\t{ \"$_\", (const nodef_value*)&def_$_ },\n");
		}
	}
	continue_output("}; // defaults[]\n");
	continue_output("const int defaults_count = sizeof(defaults)/sizeof(key_value_pair);\n\n");


	# output per-daemon key/value table
	#
	continue_output("\n/*==========================================\n"
					. " * subsystem overrides of global defaults[]\n"
					. " *==========================================*/\n");
	foreach my $key (sort { lc($a) cmp lc($b) } keys %var_daemons) {
		my $daemon_prefix = $key.'$';
		continue_output("// '$key.param\'\n");
		continue_output("const key_value_pair ${daemon_prefix}defaults[] = {\n");
		foreach (sort { lc($a) cmp lc($b) } @{$var_daemons{$key}}) {
			continue_output("\t{ \"$_\", (const nodef_value*)&def_${daemon_prefix}$_ },\n");
		}
		continue_output("}; // ${daemon_prefix}defaults[]\n");
		continue_output("const int ${daemon_prefix}defaults_count = sizeof(${daemon_prefix}defaults)/sizeof(key_value_pair);\n\n");
	}
	# output a map of per-daemon tables
	continue_output("\n/*==========================================\n"
					. " * map of subsystem override param tables.\n"
					. " *==========================================*/\n");
	continue_output("const key_table_pair subsystems[] = {\n");
	foreach my $key (sort  { lc($a) cmp lc($b) } keys %var_daemons) {
		my $daemon_prefix = $key.'$';
		continue_output("\t{ \"$key\", ${daemon_prefix}defaults, ${daemon_prefix}defaults_count },\n");
	}
	continue_output("}; // subsystems[]\n");
	continue_output("const int subsystems_count = sizeof(subsystems)/sizeof(key_table_pair);\n\n");
	continue_output("const ktp_value def_subsystems = { \"subsys\", PARAM_TYPE_KTP_TABLE, subsystems, subsystems_count };\n\n");

	# output metaknob  key/value table
	#
	continue_output("\n/*==========================================\n"
					. " * metaknob tables \n"
					. " *==========================================*/\n");
	foreach my $key (sort  { lc($a) cmp lc($b) } keys %var_metas) {
		my $meta_prefix = $key.'$';
		continue_output("// '$key.knobs\'\n");
		continue_output("const key_value_pair ${meta_prefix}knobs[] = {\n");
		foreach (sort { lc($a) cmp lc($b) } @{$var_metas{$key}}) {
			continue_output("\t{ \"$_\", (const nodef_value*)&def_\$${meta_prefix}$_ },\n");
		}
		continue_output("}; // ${meta_prefix}knobs[]\n");
		continue_output("const int ${meta_prefix}knobs_count = sizeof(${meta_prefix}knobs)/sizeof(key_value_pair);\n\n");
	}
	# output a map of meta knob tables
	continue_output("\n/*==========================================\n"
					. " * map of metaknob tables.\n"
					. " *==========================================*/\n");
	continue_output("const key_table_pair metaknobsets[] = {\n");
	foreach my $key (sort  { lc($a) cmp lc($b) } keys %var_metas) {
		my $meta_prefix = $key.'$';
		continue_output("\t{ \"$key\", ${meta_prefix}knobs, ${meta_prefix}knobs_count },\n");
	}
	continue_output("}; // metaknobsets[]\n");
	continue_output("const int metaknobsets_count = sizeof(metaknobsets)/sizeof(key_table_pair);\n");
	continue_output("const ktp_value def_metaknobsets = { \"\$\", PARAM_TYPE_KTP_TABLE, metaknobsets, metaknobsets_count };\n\n");
	# output metaknob  key.value source map
	continue_output("\n/*==========================================\n"
					. " * metaknob source table \n"
					. " *==========================================*/\n");
	continue_output("//const key_value_pair metaknobsources[] = {\n");
	foreach my $key (sort  { lc($a) cmp lc($b) } keys %var_metas) {
		foreach (sort { lc($a) cmp lc($b) } @{$var_metas{$key}}) {
			continue_output("//\t{ \"${key}:$_\", (const nodef_value*)&def_\$${key}\$$_ },\n");
		}
	}
	continue_output("//}; // metaknobsources[]\n");
	continue_output("//const int metaknobsources_count = sizeof(metaknobsources)/sizeof(key_value_pair);\n\n");
	continue_output("//const kvp_value def_metaknobsources = { \"\$:\", PARAM_TYPE_KVP_TABLE, metaknobsources, metaknobsources_count };\n\n");

	# end of table declarations, everthing after this should be extern or type declarations.
	#
	continue_output("#endif // if PARAM_DECLARE_TABLES\n\n"
					. "extern const key_value_pair defaults[];\n"
					. "extern const int defaults_count;\n"
					. "extern const key_table_pair subsystems[];\n"
					. "extern const int subsystems_count;\n"
					. "//extern const key_table_pair metaknobsets[];\n"
					. "//extern const int metaknobsets_count;\n"
					. "extern const ktp_value def_metaknobsets;\n"
					. "//extern const key_value_pair metaknobsources[];\n"
					. "//extern const int metaknobsources_count;\n"
					. "//extern const kvp_value def_metaknobsources;\n"
					);

	# output an enum of param indexs into the global defaults table
	#
	continue_output("\n/*==========================================\n"
					. " * param id's. index into global defaults[].\n"
					. " *==========================================*/\n");
	continue_output("enum {\n");
	for(sort { lc($a) cmp lc($b) } @var_names) {
		continue_output("\tix$_,\n");
	}
	continue_output("}; // param id's\n\n");

	# meta info for params
	#
	continue_output("\n#ifdef PARAM_DECLARE_HELP_TABLES\n");
	continue_output("\n/*==========================================\n"
					. " * param help/usage information. indexed by param id.\n"
					. " *==========================================*/\n");

	continue_output("typedef struct paramhelp_entry { int flags; const char * strings; } paramhelp_entry;\n");

	for(sort { lc($a) cmp lc($b) } @var_names) {
		my $help = $var_help{$_};
		continue_output("static const paramhelp_entry help_$_ = { $help };\n");
	}

	continue_output("const paramhelp_entry* paramhelp_table[] = {\n");
	for(sort { lc($a) cmp lc($b) } @var_names) {
		continue_output("\t&help_$_,\n");
	}
	continue_output("}; // paramhelp_table[]\n");
	continue_output("const int paramhelp_table_size = sizeof(paramhelp_table)/sizeof(paramhelp_table[0]);\n\n");
	continue_output("\n#endif // PARAM_DECLARE_HELP_TABLES\n");


	# wrap things up. 
	continue_output("\n} //namespace condor_params\n");
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
	my $banner_charclass = ['\A\#\#+', 'banner'];
	while (&next_is($banner_charclass)) {
		&ignore(COMMENTS);
	}
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
			&ignore(WHITESPACE);
			my $banner_charclass = ['\A\#\#+', 'banner'];
			while (&next_is($banner_charclass)) {
				&ignore(COMMENTS);
			}
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
		['o',	1,   'output',	   'param_info.h',          'output file (default: "param_info.h")'],
		['I',	0,   'stdin',      0,                       'input from standard in instead of file'],
		['O',	0,   'stdout',     0,                       'print to standard out instead of file'],
		['a',	0,   'append',     0,                       "append: don't clobber output file"],
		['e',	0,   'errors',     0,                       'do not die on some errors'],
		['d',	0,   'debug',	   0,                       0], # 0 makes it hidden on -h
		['p',	1,   'platform',   'generic',               'platform'],
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
		version => '7.1.0',
		type => 'STRING',
		param_type => 'STRING',
	};
	# call reconstitute on the params to do all of the outputting.
	#$options{debug} = 1;
	reconstitute($params, $defaults)

}
