#!/s/std/bin/perl

# Script for sorting condor_attributes.C.  All you have to do is run
# it in the condor_c++_util directory.  It will rename the existing
# condor_attributes.C to condor_attributes.C.old, then sort that and
# put the results into condor_attributes.C.  It's smart about the
# distribution-specific attributes that have to be commented out in
# here, since they are really handled by #defines in
# condor_attributes.h instead of as real strings.  It also gets the
# formatting right automatically, so we don't have to worry about
# that, either.
# Written by Derek Wright <wright@cs.wisc.edu> 2003-11-08

$file = "condor_attributes.C";
$old = "$file.old";

unlink $old || die "Can't unlink $old: $!\n";
rename $file, $old || die "Can't rename $file to $old: $!\n";
open( IN, "<$old") || die "Can't open $old: $!\n";
open( OUT, ">$file") || die "Can't open $file: $!\n";

while( <IN> ) {
    chomp;
    if( /(\/*)const char (ATTR_\S*)(\s*)(.*)/ ) {
	$is_comment = $1;
	$attr_name = $2;
	$attr_val = $4;
	$saw_attr = 1;
	if( $is_comment ) {
	    $comments{$attr_name}=$attr_val;
	    push @attrs, $attr_name;
	} else {
	    $real_attrs{$attr_name}=$attr_val;
	    push @attrs, $attr_name;
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
    if( $val = $comments{$attr} ) {
	printf OUT "//const char %-26s %s\n", $attr, $val;
    } else {
	printf OUT "const char %-28s %s\n", $attr, $real_attrs{$attr};
    }
}
	
foreach $line ( @footer ) {
    print OUT "$line\n";
}

