#!/usr/bin/env perl
use strict;
use warnings;

# this perl script take as input a set of classads (presumed to be machine/slot ads)
# and generates C++ data declarations that can be used to (approximately) reproduce those ads
# it was used to generate parse_classad_testdata.hpp from the file formerly known as testdata.txt

my %keys;    # use count of keys indexed by attribute
my %values;  # use count of non-numeric values, indexed by value
my %num_min; # min value for numeric values, indexed by attribute
my %num_max; # min value for numeric values, indexed by attribute
my @ads;
my $ad={};
my %special_string=(
 Machine=>"?machine", Name=>"?name",
 FileSystemDomain=>"?machine", UtsnameNodename=>"?machine", UidDomain=>"?machine",
 CkptServer=>"?machine", UpdatesHistory=>"?hist",
 RemoteUser=>"?user", RemoteOwner=>"?user",
 HardwareAddress=>"?MAC", StartdIpAddr=>"?sin", MyAddress=>"?sin",
 GlobalJobId=>"?gjid", JobId=>"?jid", PublicClaimId=>"?cid",
 UWCMS_CVMFS_Exists=>"?Cvmsexist",
);
my %special_int=(
 LastHeardFrom=>1, RecentDaemonCoreDutyCycle=>1, ServerTime=>1, MyCurrentTime=>1,
 UWCMS_CVMFS_CacheUsedKB=>1, AFSCacheUsed=>1, UWCMS_CVMFS_CacheSizeKB=>1, AFSCacheAvail=>1,
 KeyboardIdle=>1, ConsoleIdle=>1, TotalCondorLoadAvg=>1, CondorLoadAvg=>1, LoadAvg=>1, TotalLoadAvg=>1,
 DaemonStartTime=>1, LastBenchmark=>1, LastUpdate=>1,
 Mips=>1, KFlops=>1,
 Disk=>1, TotalDisk=>1, TotalSlotDisk=>1, ImageSize=>1,
 TotalMemory=>1, DetectedMemory=>1, TotalSlotMemory=>1, Memory=>1, TotalVirtualMemory=>1, VirtualMemory=>1,
 TotalClaimRunTime=>1, TotalTimeClaimedBusy=>1, TotalJobRunTime=>1,
 TotalTimeOwnerIdle=>1, CpuBusyTime=>1, TotalTimeUnclaimedIdle=>1, TotalClaimRunTime=>1,
 UpdateSequenceNumber=>1, UpdatesTotal=>1, UpdatesSequenced=>1,
 ExpectedMachineQuickDrainingBadput=>1, ExpectedMachineQuickDrainingCompletion=>1,
 ExpectedMachineGracefulDrainingBadput=>1, ExpectedMachineGracefulDrainingCompletion=>1,
 TotalMachineDrainingBadput=>1,
 MonitorSelfImageSize=>1, MonitorSelfCPUUsage=>1, MonitorSelfAge=>1, MonitorSelfTime=>1, MonitorSelfResidentSetSize=>1,
);

my %attrs; # hold the position for the attributes
my %rhpool; # holds the position for the right hand sides
my %examples;
my $longest = 0;

for (<>) {
   # print $_;
   if ($_ =~ /^\s*$/) {
      push @ads, $ad;
      $ad = {};
      next;
   }
   if (length($_) > $longest) { $longest = length($_); }
   my ($key,$value) = $_ =~ m{^\s*([a-zA-Z0-9_\$\.]+)\s*=\s*(.*)$};
   if (exists $special_string{$key}) {
      $examples{$special_string{$key}} = $value;
      $value = $special_string{$key};
   }
   #print "$key|$_" if ($key =~ /^\d/);
   $keys{$key} += 1;
   if ($value =~ /^-?[\d\.]+$/) {
      if (exists $num_min{$key}) {
         if ($num_min{$key} > $value) { $num_min{$key} = $value; }
         if ($num_max{$key} < $value) { $num_max{$key} = $value; }
      } else {
         $num_max{$key} = $num_min{$key} = $value;
      }
      $value = "?num";
   } else {
      $values{$value} += 1;
   }
   $ad->{$key} = $value;
}
if (scalar %{$ad}) { push @ads, $ad; }

print "static const size_t longest_kvp=$longest;\n";

print "static const char attrs[] =\n";
my $index = 0;
my $pos = 0;
for my $k (sort keys %keys) {
    print "\t\"$k\\0\" //$index,$pos\n";
    $attrs{$k} = $pos;
    $pos += length($k) + 1;
    $index += 1;
}
print ";\n\n";

while (my ($k,$v) = each %examples) {
   print "// $k = $v\n";
}
print "\n";

print "static const char rhpool[] =\n";
$index = 0;
$pos = 0;
for my $v (sort keys %values) {
    my $q = $v;
    $q =~ s/\"/\\"/g;
    print "\t\"$q\\0\" //$index,$pos\n";
    $rhpool{$v} = $pos;
    $pos += length($v) + 1;
    $index += 1;
}

$pos = ($pos + 0xFFF) & ~0xFFF;
print ";\n\n";
print "static const int rhint_index_base=$pos;\n";

# now write the ranged numeric int values
$index = 0;
print "static struct { long long lo; long long hi; } rhint[] = {\n";
for my $k (sort keys %num_min) {
    next if ($num_min{$k} =~ /\./);
    next if ($num_max{$k} =~ /\./);
    my $n = $num_min{$k} . ',' . $num_max{$k};
    next if (exists $rhpool{$n});
    print "\t{$n}, //$index,$pos\n";
    $rhpool{$n} = $pos;
    $pos += 1;
    $index += 1;
}
print "};\n\n";

$pos = ($pos + 0xFF) & ~0xFF;
print "static const int rhdouble_index_base=$pos;\n";

# now write the ranged numeric double values
$index = 0;
print "static struct { double lo; double hi; } rhdouble[] = {\n";
for my $k (sort keys %num_min) {
    next if (!($num_min{$k} =~ /\./) && !($num_max{$k} =~ /\./));
    my $n = $num_min{$k} . ',' . $num_max{$k};
    next if (exists $rhpool{$n});
    print "\t{$n}, //$index,$pos\n";
    $rhpool{$n} = $pos;
    $pos += 1;
    $index += 1;
}
print "};\n\n";

print "static const unsigned short ad_data[] = {\n";
for $ad (@ads) {
   print "  ";
   while (my ($k,$v) = each %{$ad}) {
      print $attrs{$k} . ',';
      if ($v eq "?num") {
         my $n = $num_min{$k} . ',' . $num_max{$k};
         print $rhpool{$n} . ',  ';
      } else {
         print $rhpool{$v} . ',  ';
      }
   }
   print " 0xFFFE,0xFFFF,\n";
}
print "};\n\n";
