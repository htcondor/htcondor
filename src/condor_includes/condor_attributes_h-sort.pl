#!/usr/bin/env perl

# Script for sorting condor_attributes.h.  All you have to do is run
# it in the condor_includes directory.  It will rename the existing
# condor_attributes.h to condor_attributes.h.old, then sort that and
# put the results into condor_attributes.h.  It's smart about the
# distribution-specific attributes that have to be handled by #define
# in addition to the regular strings. 
# Written by Derek Wright <wright@cs.wisc.edu> 2003-11-08

$file = "condor_attributes.h";
$old = "$file.old";

unlink $old || die "Can't unlink $old: $!\n";
rename $file, $old || die "Can't rename $file to $old: $!\n";
open( IN, "<$old") || die "Can't open $old: $!\n";
open( OUT, ">$file") || die "Can't open $file: $!\n";

while( <IN> ) {
    chomp;
    if( /(extern const char|#define) ATTR_.*/ ) {
	$saw_attr = 1;
	if( /#define (\S*)(\s*)(.*)/ ) {
	    $defines{$1}=$3;
	    push @attrs, $1;
	} elsif ( /extern const char (.*)\[\]\;/ ) {
	    push @attrs, $1;
	}
    } else {
	if( $saw_attr ) {
	    push @footer, $_;
	} else {
	    push @header, $_;
	}
    }
}
@sorted_attrs = sort @attrs;

foreach $line ( @header ) {
    print OUT "$line\n";
}

foreach $attr ( @sorted_attrs ) {
    if( $define = $defines{$attr} ) {
	print OUT "#define $attr    $define\n";
    } else {
	print OUT "extern const char $attr\[\]\;\n"; 
    }
}
	
foreach $line ( @footer ) {
    print OUT "$line\n";
}

