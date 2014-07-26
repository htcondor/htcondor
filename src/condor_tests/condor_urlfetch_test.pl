use Cwd 'abs_path';
use File::Basename;
use lib dirname( abs_path $0 );
use CondorTest;
use CondorPersonal;
use CondorUtils;
use Check::SimpleJob;

use strict;
use warnings;

my $conf = 'URLTEST = 123456 '; #The local config file



my $fileh;
$fileh = File::Temp->new();
my $fname = $fileh->filename;
$fileh->print($conf);

my $forkpid = fork();
if($forkpid == 0)
{
  system("$(LOCAL_DIR)/python miniserver.py &");
} else 
{
  sleep(1);
  local $/=undef;
  open FILE, "($(LOCALDIR)/pythonurl";
  my $url = <FILE> . "/" . $fname;
  close FILE;

  my @convaldump = ();
  runCondorTool("condor_config_val -config", \@convaldump, 2, {emit_output => 1});
  my @lines = split /\n/, @convaldump;
  my $configpath = trim($lines[1]); #$configpath now contains the address of the conf file
  
  my $oldconffh;
  open($oldconffh, '>>', $configpath) or die "Can't open $configpath";
  # May add save old config file in condor_urlfetch_test.saveme, doesn't seem
  # necessary for now
  say $oldconffh "\n LOCAL_CONFIG_FILE = condor_urlfetch _P(SUBSYSTEM) " . $url . " condor_config_file_cache";
  CondorTest::StartCondorWithParams
  (
    condor_name => "urlfetch_test",
    #condorlocalsrc=> ($conf . "\n LOCAL_CONFIG_FILE = condor_urlfetch _P(SUBSYSTEM) " . $url . " condor_config_file_cache"),
    do_not_start => 1,
    fresh_local => "true"
  ); 
  
  my $fetchedfh;
  open($fetchedfh, "$(LOCAL_DIR)/condor_config_file_cache");
  
  use File::Compare;
  if(compare($fileh, $fetchedfh) != 0)
  {
    print "Error in downloading the config file";
    exit("Error in downloading the config file");
  }
#condor_scripts condortests.pm    copy it to condor_tests
}


exit();
