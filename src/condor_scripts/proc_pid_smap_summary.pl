#!/usr/bin/env perl
use strict;

my @PIDS;
my $verbose=0;
my $noheader=0;

if (@ARGV > 0) { 
   while (@ARGV) { 
      $_ = shift;
      if ($_ =~ /^-/) {
         if ($_ =~ /^-v/) {
            $verbose=1; if ($_ =~ /^-v2/) { $verbose=2; }
         }  elsif ($_ =~ /^-b/) {
            $noheader=1;
         }
      } else {
        push(@PIDS, $_); 
      }
   } 
}

if (@PIDS <= 0) {
   @PIDS = `top -b -n 1 | grep condor_ | awk '{print \$1}'`;
   chomp(@PIDS);
}

if ( ! $noheader) {
   print 'Process                Total Dynamic =(Heap+Stack+Anon) LibRO LibData =(Exec+Utils+Clasad+Globus+System) Clean';
   print "\n";
}
for my $pid (@PIDS) {
   #print "\npid is $pid.\n";
   smap_sum_verbose($pid);
}
print "\n";

# address                           perm  offset   dev  inode              path
# ffffffffff600000-ffffffffff601000 r-xp 00000000 00:00 0                  [vsyscall]
# Size:                  4 kB
# Rss:                   0 kB
# Pss:                   0 kB
# Shared_Clean:          0 kB
# Shared_Dirty:          0 kB
# Private_Clean:         0 kB
# Private_Dirty:         0 kB
# Referenced:            0 kB
# Anonymous:             0 kB
# AnonHugePages:         0 kB
# Swap:                  0 kB
# KernelPageSize:        4 kB
# MMUPageSize:           4 kB
#

sub smap_sum_verbose {

   my $pid = shift;

   my $lib = ""; my $perm; my $inode; my $exe;
   #my $fd; my $addr1; my $addr2;
   my %tot;
   $tot{Heap} = 0;
   $tot{Stack} = 0;
   $tot{LibData} = 0;

   open (SMAP, "</proc/$pid/smaps");
   while (<SMAP>) {
      if ($verbose > 1) { print $_; }

      if ($_ =~ /^([0-9A-Fa-f]+)-([0-9A-Fa-f]+)\s+(r[w-][x-]p)\s+(\S+\s+\S+)\s+(.*)$/) {

         if (defined $inode && $inode != $5) { 
            if ($verbose) {
               # printf "%6d %s %s = %s\n", $tot{Private_Dirty}, $perm, $inode, $lib; 
            }
         }

         $perm = $3; # perm
         $inode = $5; $lib=""; if ($inode =~ /^(\S+)\s+(.*)$/) { $inode = $1; $lib = $2; }
         #$addr1 = $1; $addr2 = $2; $fd = $4;

         if ( ! ($lib =~ /\.so/) && $lib =~ /condor_/) { $exe = $lib; }
         #
      } elsif ($_ =~ /^(\w+):\s+(\d+)/) {

         my $n = $2;
         #printf "%20.20s %5d\n", $1, $2;
         $tot{$1} += $n;
         if ($1 =~ /Private_Dirty/) { 
            if ($perm eq "r--p") { $tot{LibRO} += $n; }
            if ($lib eq "") { 
               $tot{Anon} += $n;
            } else {
               if ($lib eq "[heap]") { $tot{Heap} += $n; }
               elsif ($lib eq "[stack]") { $tot{Stack} += $n; }
               elsif ($perm eq "rw-p") { $tot{LibData} += $n; }
               
               if ($lib =~ /condor_util/) { $tot{Utils} += $n; }
               elsif ($lib =~ /condor_/)  { $tot{Exec} += $n; }
               elsif ($lib =~ /condor/)   { 
                  if ($lib =~ /globus|voms/) { $tot{Globus} += $n; }
                  elsif ($lib =~ /classad/)  { $tot{Classad} += $n; }
                  else                       { $tot{Other} += $n; }
               } elsif ($lib =~ /lib/) { $tot{Other} += $n; }
            }

            if ($verbose && $lib ne "" && $perm eq "rw-p")
               { printf "%-10.10s %5d %5d %5d %s %s\n", $pid, $n, $tot{LibData}, $tot{Private_Dirty}, $perm, $lib; }
         }
         
      }
   } #while

   my $lbl = $pid;
   if ($exe) { 
      my $ver = $exe; $ver =~ s/\/sbin\/condor_.*//; $ver =~ s/.*\///; $ver =~ s/condor//;
      $exe =~ s/.*\///g; $exe =~ s/condor_//;
      $lbl = "$pid:$exe" . $ver;
   }

#   print 'Process                Total Dynamic =(Heap+Stack+Anon) LibRO LibData =(Exec+Utils+Clasad+Globus+System) Clean';
   printf "%-22.22s %5d %7d %6d %5d %5d %5d %7d %6d %5d %6d %6d %6d %6d\n", $lbl, 
          $tot{Private_Dirty}, $tot{Heap}+$tot{Stack}+$tot{Anon}, $tot{Heap}, $tot{Stack}, $tot{Anon},
          $tot{LibRO}, $tot{LibData}, $tot{Exec}, $tot{Utils}, $tot{Classad}, $tot{Globus}, $tot{System}+$tot{Other},
          $tot{Private_Clean};
}
