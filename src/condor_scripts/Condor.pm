
package Condor;

$CONDOR_SUBMIT = 'condor_submit';
$CONDOR_VACATE = 'condor_vacate';
$CONDOR_RESCHD = 'condor_reschedule';

BEGIN {
    $DEBUG = 0;
}


sub Submit
{
    local($cmd) = @_;
    local($rtn, $cluster, @parts);
    local($log, $out, $err, $in) = &ReadCmdFile($cmd);

    debug("Cleaning up old files: $log $out $err\n");
    `rm -f $log $out $err`;

    # Since we handle many different programs running at once,
    # we store everything in a hash table by name.
    
    open (SUBMIT, "$CONDOR_SUBMIT $cmd|") || die "Can't run $CONDOR_SUBMIT: $!\n";
    $line = <SUBMIT>; $line = <SUBMIT>; $line = <SUBMIT>;  # want 3rd line
    
    close(SUBMIT);

    @parts = split(' ', $line);
    $cluster = $parts[$#parts];
    $cluster =~ s/\.//g; # strip off period.
    
    $LogFile{$cluster} = $log;
    $OutFile{$cluster} = $out;
    $ErrFile{$cluster} = $err;
    $CmdFile{$cluster} = $cmd;
    $InFile{$cluster} = $in;

    return $cluster;
}

sub Vacate
{
    ($machine) = @_;
    debug("Vacating $machine.\n");
    `$CONDOR_VACATE $machine`;
}

sub Reschedule
{
    ($machine) = @_;
    debug("Recheduling $machine.\n");
    `$CONDOR_RESCHD $machine`;
}

# Called with ( $cluster, $proc, $ckpt )    
sub RegisterEvicted
{
    ($cluster, $sub) = @_;
    $EvictedCallback{$cluster} = $sub;
    return 0;
}


# Called with ( $cluster, $proc, $return_val, @output, @error )
sub RegisterNormalTerm
{
    ($cluster, $sub) = @_;
    $NormalTermCallback{$cluster} = $sub;
}

# Called with ( $cluster, $proc, $signal_val, $core, @output, @error )
sub RegisterAbnormalTerm
{
    ($cluster, $sub) = @_;
    $AbnormalTermCallback{$cluster} = $sub;
}

# Called with ( $cluster, $proc, $ip, $port )
sub RegisterExecute
{
    ($cluster, $sub) = @_;
    $ExecuteCallback{$cluster} = $sub;
}

# Called with ( $cluster, $proc, $ip, $port )
sub RegisterSubmit
{
    ($cluster, $sub) = @_;
    $SubmitCallback{$cluster} = $sub;
}

sub Wait
{
    while(wait() != -1)
    {
    }
    return;
}

sub Monitor
{
    ($cluster) = @_;
 
    # fork off new monitor
  FORK: {
      if($pid = fork)
      {
	  return;   # parent returns
      }
      elsif(defined $pid)
      {
	  # child
	  open(CLOG, "<$LogFile{$cluster}") || die "Can't open $LogFile{$cluster}\n";
	      
	  local($tcluster, $proc, $temp, @parts, $checkpointed);
	  local($i);
	  
	  debug("Monitoring Cluster $cluster.\n");
	  while(1)
	  {
	      $_ = <CLOG>;
	      if($_ =~ /(.*)Job was evicted(.*)/)    # Job got evicted from a machine
	      {
		  
		  @parts = split(' ', $_);
		  $cluster_proc = $parts[1];
		  ($tcluster, $proc) = getClusterProc($cluster_proc);
		  
		  &debug("Job $tcluster.$proc was evicted.\n");
		  
		  if($tcluster == $cluster)
		  {
		      $_ = <CLOG>;
		      
		      if($_ =~ /(.*)Job was checkpointed(.*)/)  
		      {
			  if( $DEBUG != 0 )
			  {
			      print "Job $tcluster.$proc was checkpointed.\n";
			  }
			  $checkpointed = 1;
		      }
		      else
		      {
			  $checkpointed = 0;
		      }
		      
		      &debug("Job matches.\n");
		      if( exists $EvictedCallback{$cluster} )
		      {
			  &debug("Calling Eviction handler.\n");
			  $temp = $EvictedCallback{$cluster};
			  &$temp($cluster, $proc, $checkpointed);
		      }
		  }
	      }
	      
	      elsif( $_ =~ /(.*)Job terminated(.*)/ )
	      {
		  
		  @parts = split(' ', $_);
		  $cluster_proc = $parts[1];
		  ($tcluster, $proc) = getClusterProc($cluster_proc);
		  
		  if( $DEBUG != 0 )
		  {
		      print "Job $tcluster.$proc terminated.\n";
		  }
		  
		  
		  if($tcluster == $cluster)
		  {
		      
		      open(OUTPUT, "<$OutFile{$cluster}") || ( print "Can't open $OutFile{$cluster}: $!\n" );
		      @out = <OUTPUT>;   # Sluurrrpp.
		      close(OUTPUT);
		      
		      open(ERROR, "<$ErrFile{$cluster}") || ( print "Can't open $ErrFile{$cluster}: $!\n" );
		      @err = <ERROR>;
		      close(ERROR);
		      
		      $line = <CLOG>;
		      if( $line =~ /(.*)Normal(.*)/ )
		      {
			  @parts = split(' ', $line);
			  $return_val = $parts[5];
			  $return_val =~ s/\)//g;
			  if( exists $NormalTermCallback{$cluster} )
			  {
			      $temp = $NormalTermCallback{$cluster};
			      &$temp($cluster, $proc, $return_val, @out, @err);
			  }
		      }
		      else
		      {
			  @parts = split(' ', $line);
			  $signal_val = $parts[4];
			  $signal_val =~ s/\)//g;
			  
			  $line = <CLOG>;
			  if( $line =~ /(.*)No core file(.*)/ )
			  {
			      $core = "";
			  }
			  else
			  {
			      @more_parts = split ' ', $line;
			      $core = $more_parts[3];
			  }
			  
			  if( exists $AbnormalTermCallback{$cluster} )
			  {
			      $temp = $AbnormalTermCallback{$cluster};
			      &$temp($cluster, $proc, $signal_val, $core, @out, @err);
			  }
		      }  
		  }
	      }
	      elsif( $_ =~ /(.*)Job executing on host(.*)/ )
	      {
		  
		  @parts = split(' ', $_);
		  $cluster_proc = $parts[1];
		  ($tcluster, $proc) = getClusterProc($cluster_proc);
		  
		  debug("Job $tcluster.$proc executing.\n");
		  
		  if($tcluster == $cluster)
		  {
		      debug("tcluster matches cluster.\n");
		      $sinful = $parts[$#parts];
		      
		      @ip_port = &strip_sinful($sinful);
		      
		      if( exists $ExecuteCallback{$cluster} )
		      {
			  debug("Calling execute handler.\n");
			  $temp = $ExecuteCallback{$cluster};
			  &$temp($cluster, $proc, $ip_port[0], $ip_port[1]);
		      }
		  }
	      }
	      elsif( $_ =~ /(.*)Job submitted from host(.*)/ )
	      {
		  @parts = split(' ', $_);
		  $cluster_proc = $parts[1];
		  ($tcluster, $proc) = getClusterProc($cluster_proc);
		  
		  if( $DEBUG != 0 )
		  {
		      print "Job $tcluster.$proc submitted.\n";
		  }
		  
		  if($tcluster == $cluster)
		  {
		      
		      $sinful = $parts[$#parts];
		      @ip_port = &strip_sinful($sinful);
		     
			# Call the registered callback if it exists. 
		      if( exists $SubmitCallback{$cluster} )
		      {
			  $temp = $SubmitCallback{$cluster};
			  &$temp($cluster, $cluster, $proc, @ip_port);
		      }
		  }
	      }
	      sleep 1;
	  }
      }
      elsif($! =~ /No more process/)
      {
	  sleep 5;
	  redo FORK;
      }
      else
      {
	  die "Can't fork: $!\n";
      }
      return 0;
  }
}

sub strip_sinful	
{
    ($sinful) = @_;
    $sinful =~ s/<//g;
    $sinful =~ s/>//g;

    @foo = split(/:/, $sinful);
    return @foo;
}
	 
sub getClusterProc
{
    ($cluster_proc) = @_;
    local($cluster, $proc);

    $cluster_proc =~ s/\(//g;
    $cluster_proc =~ s/\)//g;
    ($cluster, $proc, $subproc) = split(/\./, $cluster_proc);
    $cluster +=  0;   # Strip off leading zeros.
    $proc +=  0;
    return ($cluster, $proc);
}

sub debug
{
    ($string) = @_;
    if( $DEBUG != 0 )
    {
	print $string;
    }
}

sub ReadCmdFile
{
    local($cmd) = @_;
    open(CMD, $cmd) || die "Can't open command file: $cmd: $!.\n";
    local($log, $out, $err, $in);

    while(<CMD>)
    {
	if( $_ =~ /^#/) 
	{
	    next;
	}
	elsif( $_ =~ /(.*)=(.*)/)
	{
	    ($var, $val) = split('=', $_);
	    if($var =~ /^error(.*)/i)
	    {
		$err = $val;
	    }
	    elsif($var =~ /^log(.*)/i)
	    {
		$log = $val;
	    }
	    elsif($var =~ /^out(.*)/i)
	    {
		$out = $val;
	    }
	    elsif($var =~ /^in(.*)/i)
	    {
		$in = $val;
	    }
	}
    }
	chomp $log;
	chomp $out;
	chomp $err;
	chomp $in;
    return ($log, $out, $err, $in);
}

sub DebugOn
{
    $DEBUG = 1;
}
