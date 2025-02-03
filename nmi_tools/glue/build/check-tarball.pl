#!/usr/bin/env perl
use strict;
use warnings;

my $tar = "tar";

# On BSD and Mac the tar we want is gtar
if($ENV{NMI_PLATFORM} =~ /(macos|freebsd)/i) {
    print "Detected we are on Mac or FreeBSD.  Trying to use gtar...\n";
    if(system("which gtar") == 0) {
        print "gtar was detected in PATH\n";
        $tar = "gtar";
    } else {
		if(system("which gnutar") == 0) {
			print "gnutar was detected in PATH\n";
			$tar = "gnutar";
		} else {
   	     print "WARNING: gtar was not found in path, falling back to tar.  The version of tar might output in a different version than this script expects.  Tar version:\n";
   	     print `tar --version 2>&1`;
    	}
	}
}

# In bytes
my $FILESIZE_LOWER_BOUND = 10_000_000;
my $UNSTRIPPED_FILESIZE_UPPER_BOUND = 600_000_000;
my $STRIPPED_FILESIZE_UPPER_BOUND   =  75_000_000;

# Maximum number of files to print when printing permissions and ownerhsip errors
my $MAX_TO_PRINT = 20;
################

if(@ARGV == 0) {
    print "ERROR: Usage $0 <tarball to check>\n";
    exit 1;
}

my $num_tarballs_with_errors = 0;
foreach my $tarball (@ARGV) {
    $num_tarballs_with_errors += validate_tarball($tarball);
}

if($num_tarballs_with_errors > 0) {
    print "$num_tarballs_with_errors tarballs had errors.\n";
}
else {
    print "All tarballs succesfully passed checks.\n";
}
exit $num_tarballs_with_errors;

sub validate_tarball {
    my $file = shift;
    print "----------------------------\n";
    print "Validating tarball '$file'\n";

    #
    # Sanity check
    #
    if(!-e $file) {
	print "ERROR: tarball '$file' does not exist";
	return 1;
    }

    #
    # Check file size.  We want it within certain boundaries
    #
    my $size = (stat($file))[7];

    if($size < $FILESIZE_LOWER_BOUND) {
	print "ERROR: tarball '$file' is too small ($size < $FILESIZE_LOWER_BOUND bytes)\n";
	print "If this size threshold is not appropriate modify it in $0\n";
	return 1;
    }
    else {
	print "Tarball size ($size) is larger than lower bound ($FILESIZE_LOWER_BOUND)\n";
    }

    my $FILESIZE_UPPER_BOUND = ($file =~ /unstripped/) ? $UNSTRIPPED_FILESIZE_UPPER_BOUND :
                                                         $STRIPPED_FILESIZE_UPPER_BOUND;

    if($size > $FILESIZE_UPPER_BOUND) {
	print "ERROR: tarball '$file' is too large ($size > $FILESIZE_UPPER_BOUND bytes).\n";
	print "If this size threshold is not appropriate modify it in $0\n";
	return 1;
    }
    else {
	print "Tarball size ($size) is smaller than upper bound ($FILESIZE_UPPER_BOUND)\n\n";
    }


    #
    # Do various checks on the contents of the tarball
    #

    my @tarfiles = `$tar ztvf $file`;

    # At this point in the script we want to try to identify all the errors that have
    # occurred and not error out after the first one.  This will be more useful when
    # fixing a tarball that has multiple problems.
    my $num_errors = 0;

    # 
    # Check for the existence of subdirectories in the tarfile
    #
    # At one point cpack failed to put subdirectories in.  This means that when tar
    # unpacks the tarball is does not have metadata about the owner and permission
    # of those subdirectories.  It ends up using the umask of the user which can lead
    # to rather bizarre permissions.
    #
    my $subdirectory_found = 0;
    foreach my $line (@tarfiles) {
	if($line =~ m|/$|) {
	    $subdirectory_found = 1;
	    last;
	}
    }

    if(not $subdirectory_found) {
	$num_errors++;
	error("No subdirectory entries found inside the tarball");
    }
    else {
	print "Subdirectory entries are present in the tarball.\n";
    }


    #
    # Check the number of binaries
    #


    #
    # Check the permissions and ownership of files
    #

    # On Linux we want group root
    # On Mac we want group wheel
    # On AIX we want group system
    my $desired_owner = "0";
    my $desired_group = "0";

    my $num_bad_perms_files = 0;
    my $num_bad_owner_files = 0;
    my @bad_perms_files;
    my @bad_owner_files;
    foreach my $line (@tarfiles) {
	next if($line =~ /^\s*$/);
        # Skip over tokens.d and passwords.d (not world readable)
        next if($line =~ m!/etc/condor/tokens.d/!);
        next if($line =~ m!/etc/condor/passwords.d/!);
	chomp($line);

	# First two fields are permissions then owner/group.  Example:
	# -rw-r--r-- root/root   16468 2011-01-05 10:04:18 lib/gt42/client-config.wsdd
	my @arr = split(' ', $line);
	if(@arr >= 2) {
	    if(not check_permissions($arr[0])) {
		$num_bad_perms_files++;
		push @bad_perms_files, $line;
	    }
	    
	    # On some OSes the owner will print as just '/' instead of '0/0'.  Seems
	    # to me to be a bug in tar, so we'll just work with either one.
	    if($arr[1] ne "/" and $arr[1] ne "$desired_owner/$desired_group") {
		$num_bad_owner_files++;
		push @bad_owner_files, $line;
	    }
	}
	else {
	    print "Unrecognized line when validating the permissions and owner of the tarball:\n";
	    print "$line\n";
	}
    }

    if($num_bad_perms_files > 0) {
	my $num = $num_bad_perms_files < $MAX_TO_PRINT ? $num_bad_perms_files : $MAX_TO_PRINT;
	$num_errors++;
	error("ERROR: $num_bad_perms_files files have bad permissions.  All files must be readable by everyone and writable by only the owner.  Here are the first $num entries from the list:\n",
	      join("\n", @bad_perms_files[0..$num-1]),
	      "\n\n");
    }
    else {
	print "File permissions tests passed.\n";
    }

    if($num_bad_owner_files > 0) {
	my $num = $num_bad_owner_files < $MAX_TO_PRINT ? $num_bad_owner_files : $MAX_TO_PRINT;
	$num_errors++;
	error("ERROR: $num_bad_owner_files files have bad owners and/or groups.  Here are the first $num entries from the list:\n",
	      join("\n", @bad_owner_files[0..$num-1]),
	      "\n\n");
    }
    else {
	print "File ownership tests passed.\n";
    }

    if($num_errors > 0) {
	print "There were $num_errors errors when examining the tarball.\n\n";
	return 1;
    }
    else {
	print "Tarball passes all checks in $0\n\n";
	return 0;
    }
}

sub check_permissions {
    # We want all files to be readable by everyone (owner, group, world) and writable
    # by *only* owner.
    my ($perm) = @_;

    # If it is a symlink then we don't need to check permissions
    if($perm =~ /^lrwxrwxrwx$/) {
        return 1;
    }

    # Readable by all
    # Writable by only owner
    # Execute bits consistent
    if($perm =~ /^.rw(.)r-(.)r-(.)$/) {
        # TODO - can I compare if one of execute bits is 't', 's', etc?
        if( ($1 eq "x" or $1 eq "-") and ($1 eq $2) and ($1 eq $3) ) {
            return 1;
        }
    }

    return 0;
}


sub error {
    print @_;
}
