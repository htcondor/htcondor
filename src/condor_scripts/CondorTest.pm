package CondorTest;

$MAX_CHECKPOINTS = 2;
$HOST_CMD = 'host';
#------------------------------------------------------------------------------
# No user servicable parts below this line
#------------------------------------------------------------------------------
use Condor;



BEGIN {
    $checkpoints = 0;
    $require_checkpoint = 0;
    $ignored = 0;
    $usetestcallback = 0;
    foreach $arg (@ARGV)
    {
	if( $arg eq "-a" )
	{
	    $analyze_only = 1;
	}
	else
	{
	    $analyze_only = 0;
	}
    }
}

$normal = sub
{
    ($cluster, $proc, $return_val, @output, @error) = @_;
    local($args, $locals);
    if($return_val == 0)
    {
	$num_expect = $#expected_out + $#IGNORE + 2;
	$num_actual = $#output + 1;
	if($num_actual != $num_expect)
	{
	    print "$testname: FAILURE (output differs from expected by num lines)\n";
	    print "expected: $num_expect.  actual: $num_actual.\n";
	    exit(1);
	}

      OUTPUT: for ($i = 0; $i <= $#output; ++$i)
	{
	    $linenum = $i + 1;
	    foreach $line (@IGNORE)
	    {
		
		if($linenum == $line)
		{
		    ++$ignored;
		    print "Ignoring line $linenum.\n";
		    next OUTPUT;
		}
	    }
    
	    $expected_index = $i - $ignored;
	    chomp $output[$i]; chomp $expected_out[$expected_index];
	    
	    if( $output[$i] ne $expected_out[$expected_index] )
	    {
		print "actual: $output[$i]\n";
		print "expect: $expected_out[$expected_index]\n";
		print "$testname: FAILURE (output differs at line $linenum.)\n";
		exit(4);
	    }
	}
	if($require_checkpoint == 1 && $checkpoints == 0)
	{
	    print "$testname: FAILURE (did not checkpoint)\n";
	    exit(5);
	}
	if( $usetestcallback == 1  )
	{
	    &Condor::debug("Calling special test handler.\n");
	    $result = &$testcallback(@output, @error);
	    if($result != 0)
	    {
		print "$testname: FAILURE ( special conditions not met ).\n";
		exit(6);
	    }
	}
	print "$testname: SUCCESS ( $checkpoints checkpoints ).\n";
	exit(0);
    }
    else
    {
	print "$testname: FAILURE (return val of " . $return_val . ")\n";
	exit(3);
    }
};

$abnormal = sub
{
    ($cluster, $proc, $signal, $core, @output, @error);
    print "$testname: FAILURE (abnormal term (signal $signal)):\n";
    if($core ne "")
    {
	print "Core file is: $core.\n";
    }
    exit(2);
};

$execute = sub
{
    ($cluster, $proc, $ip, $port) = @_;
    local($output, $junk, $name);
    if($checkpoints > $MAX_CHECKPOINTS)
    {
	return;
    }
    else
    {
	&Condor::debug("Looking up $ip\n");
	
	open(HOST, "$HOST_CMD $ip|");
	$output = <HOST>;
	while ($output !~ /(.*)Host not found(.*)/  &&
	       $output !~ /(.*)Name(.*)/)
	{
	    $output = <HOST>;
	    print $output;
	}
	
	if($output =~ /(.*)Host not found(.*)/)
	{
	    die "Can't find hostname of execte machine!: $ipaddr\n";
	}
	
	($junk, $name) = split(' ',$output);
	sleep 5;
	&Condor::debug("Sending vacate to $name\n");
	&Condor::Vacate($name);
    }
};

$evicted = sub
{
    ($cluster, $proc, $ckpt) = @_;
    $checkpoints += $ckpt;
    &Condor::debug("Job evicted. (checkpointed = $ckpt)\n");
    &Condor::Reschedule();
};

sub RunTest
{
    ($cmd, $testname, @expected_out) = @_;

    if($analyze_only == 0)
    {
	$cluster = Condor::Submit($cmd);
	&Condor::RegisterNormalTerm($cluster, $normal);
	&Condor::RegisterAbnormalTerm($cluster, $abnormal);
	&Condor::RegisterExecute($cluster, $execute);
	&Condor::RegisterEvicted($cluster, $evicted);

	&Condor::Monitor($cluster);
	
	&Condor::Wait();
    }
    else
    {
      ($log, $out, $err, $in) = Condor::ReadCmdFile($cmd);
      open(OUT, $out) || die "Can't open $out: $!.\n";
      @output = <OUT>; # Slurp
      close(OUT);
      
      open(ERR, $err) || die "Can't open $err: $!.\n";
      @error = <ERR>;
      close(ERR);

      &$normal(0, 0, 0, @output, @error);
    }
}

sub IgnoreLine
{
    my($line) = @_;
    push(@IGNORE, $line);
}

sub IgnoreLines
{
    my($line);
    foreach $line (@_)
    {
	push(@IGNORE, $line);
    }
}

sub AddTestCallback
{
    &Condor::debug("Enabling special test callback.\n");
    ($sub) = @_;
    $usetestcallback = 1;
    $testcallback = $sub;
}

sub RequireCheckpoint
{
    $require_checkpoint = 1;
}
