#!/usr/bin/env perl
use strict;
use warnings;

# In bytes
my $FILESIZE_LOWER_BOUND = 50_000_000;
my $UNSTRIPPED_FILESIZE_UPPER_BOUND = 600_000_000;
my $STRIPPED_FILESIZE_UPPER_BOUND   = 300_000_000;

# Maximum number of files to print when printing permissions and ownerhsip errors
my $MAX_TO_PRINT = 20;
################

if(@ARGV != 1) {
    print STDERR "ERROR: Usage $0 <tarball to check>\n";
    exit 1;
}

my $file = $ARGV[0];
print "Validating tarball '$file'\n";


#
# Sanity check
#
if(!-e $file) {
    print STDERR "ERROR: tarball '$file' does not exist";
    exit 1;
}

#
# Check file size.  We want it within certain boundaries
#
my $size = (stat($file))[7];

if($size < $FILESIZE_LOWER_BOUND) {
    print STDERR "ERROR: tarball '$file' is too small (< $FILESIZE_LOWER_BOUND bytes)\n";
    print STDERR "If this size threshold is not appropriate modify it in $0\n";
    exit 1;
}

my $FILESIZE_UPPER_BOUND = ($file =~ /stripped/) ? $STRIPPED_FILESIZE_UPPER_BOUND :
                                                   $UNSTRIPPED_FILESIZE_UPPER_BOUND;

if($size > $FILESIZE_UPPER_BOUND) {
    print STDERR "ERROR: tarball '$file' is too large (> $FILESIZE_UPPER_BOUND bytes).\n";
    print STDERR "If this size threshold is not appropriate modify it in $0\n";
    exit 1;
}


#
# Do various checks on the contents of the tarball
#

my @tarfiles = `tar ztvf $file`;

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
    error("No subdirectory entries found inside the tarball");
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
my $desired_group = "root";

my $num_bad_perms_files = 0;
my $num_bad_owner_files = 0;
my @bad_perms_files;
my @bad_owner_files;
foreach my $line (@tarfiles) {
    next if($line =~ /^\s*$/);
    chomp($line);

    # First two fields are permissions then owner/group.  Example:
    # -rw-r--r-- root/root   16468 2011-01-05 10:04:18 lib/gt42/client-config.wsdd
    if($line =~ m|^(\S+)\s+(\S+)/(\S+)\s|) {
        my ($perms, $owner, $group) = ($1, $2, $3);
        if(not check_permissions($perms)) {
            $num_bad_perms_files++;
            push @bad_perms_files, $line;
        }
        if($2 ne "root" or $3 ne $desired_group) {
            $num_bad_owner_files++;
            push @bad_owner_files, $line;
        }
    }
    else {
        print STDERR "Unrecognized line when validating the permissions and owner of the tarball:\n";
        print STDERR "$line\n";
    }
}

if($num_bad_perms_files > 0) {
    my $num = $num_bad_perms_files < $MAX_TO_PRINT ? $num_bad_perms_files : $MAX_TO_PRINT;
    error("ERROR: $num_bad_perms_files files have bad permissions.  All files must be readable by everyone and writable by only the owner.  Here are the first $num entries from the list:\n",
          join("\n", @bad_perms_files[0..$num-1]),
          "\n\n");
}

if($num_bad_owner_files > 0) {
    my $num = $num_bad_owner_files < $MAX_TO_PRINT ? $num_bad_owner_files : $MAX_TO_PRINT;
    error("ERROR: $num_bad_owner_files files have bad owners and/or groups.  Here are the first $num entries from the list:\n",
          join("\n", @bad_owner_files[0..$num-1]),
          "\n\n");
}

if($num_errors > 0) {
    print STDERR "There were $num_errors errors when examining the tarball.";
    exit 1;
}
else {
    print STDERR "Tarball passes all checks in $0\n";
}
exit 0;

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
    print STDERR @_;
    $num_errors++;
}
