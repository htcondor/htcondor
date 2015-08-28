#!/usr/bin/perl -w

# Specify directory name on command line or
# use default.
$ca_root_dir = shift || "CondorRootCA";

# Create directory for CA files.
if(not -d $ca_root_dir) {
    mkdir $ca_root_dir, 0700 
	or die "Can't create '$ca_root_dir' directory: $!.";
}

# Create directory for certificate files.
if(not -d "$ca_root_dir/ca.db.certs") {
    mkdir "$ca_root_dir/ca.db.certs", 0700
	or die "Can't create '$ca_root_dir/ca.db.certs': $!";
}

# Initialize serial if it isn't already created.
if(not -f "$ca_root_dir/ca.db.serial") {
    open SERIAL, ">$ca_root_dir/ca.db.serial"
	or die "Can't create serial: $!";
    print SERIAL "01\n";
    close SERIAL;
}

# Create index file if not existant.
if(not -f "$ca_root_dir/ca.db.index") {
    open INDEX, ">$ca_root_dir/ca.db.index" 
	or die "Can't create index: $!";
    close INDEX;
} 

# Initialize random number file.
if(not -f "$ca_root_dir/ca.db.rand") {
    open RAND, ">$ca_root_dir/ca.db.rand"
	or die "Can't create randfile: $!";
    my($r) = int(rand(90)+10);
    print RAND "$r\n";
    close RAND;
}
