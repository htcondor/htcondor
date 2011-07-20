#!/usr/bin/perl -w

################################################################
#
# Copyright (C) 2009-2010, Condor Team, Computer Sciences Department,
# University of Wisconsin-Madison, WI.
# 
# Licensed under the Apache License, Version 2.0 (the "License"); you
# may not use this file except in compliance with the License.  You may
# obtain a copy of the License at
# 
#    http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
################################################################


use strict;
use DBI;
use Getopt::Long;
use Carp;


my $condorQCmd		= "condor_q";
my $condorSubmitCmd	= "condor_submit";

my $condorAttrPrefix	= 'CDBQ_';
my $cdbqIdAttr		= "${condorAttrPrefix}ID";
my $cdbqSysAttr		= "${condorAttrPrefix}SYS";


package CondorUserLog;

sub new
{
    my $invocant = shift;

    my $class = ref($invocant) || $invocant;
    my $self = {
		filename	=> undef,
		fh		=> undef,
		buf		=> '',
		bufOffset	=> 0,
		fileOffset	=> 0,
		readChunkSize	=> 1024,
		dev		=> undef,
		ino		=> undef,
		fileErrMsg	=> undef,
		fileErrNum	=> 0,
		lastUpdateTime	=> 0,
		@_
		};

    die "filename required" unless defined $self->{filename};

    if (defined $self->{offset})  {
	$self->{bufOffset} = $self->{fileOffset} = $self->{offset};
	delete $self->{offset};
    }

    bless $self, $class;
    return $self;
}


sub FileExists
{
    my $self = shift;

    return -f $self->{filename};
}


sub Open
{
    my $self = shift;

    if (!defined $self->{fh})  {
	my $fh;
	my $errMsg = '';
	my $errNum = 0;

	my $r = open $fh, "<", $self->{filename};
	if ($r)  {
	    binmode $fh;
	    $r = seek $fh, $self->{fileOffset}, 0;
	    if ($r)  {
		$self->{fh} = $fh;
	    }  else  {
		$errMsg = "seek: $!";
		$errNum = 0 + $!;
		close $fh
	    }
	}  else  {
	    $errMsg = "open: $!";
	    $errNum = 0 + $!;
	}

	$self->{fileErrMsg} = $errMsg;
	$self->{fileErrNum} = $errNum;
    }

    return $self->{fh};
}


sub Close
{
    my $self = shift;

    if (defined $self->{fh})  {
	my $r = close $self->{fh};
	$self->{fh} = undef if $r;
	return $r;
    }

    return 0;
}


sub SetPosition
{
    my $self = shift;
    my $pos = shift;

    $self->{buf} = '';
    $self->{bufOffset} = $self->{fileOffset} = $pos;

    my $r = seek $self->{fh}, $pos, 0;
    if (!$r)  {
	$self->Close;
    }
}


sub DESTROY
{
    my $self = shift;

    $self->Close;
}


sub ReadFileChunk
{
    my $self = shift;

    my $fh = $self->Open;

    return unless defined $fh;

    my $fileSize = -s $fh;
    my $fileOffset = $self->{fileOffset};

    if ($fileOffset > $fileSize)  {
	$self->{fileErrMsg}
		= "File truncated size=$fileSize, last read=$fileOffset";
	$self->{fileErrNum} = -1;
	return;
    }

    my $fileBytesRemaining = $fileSize - $fileOffset;
    my $readSize = $self->{readChunkSize};
    $readSize = $fileBytesRemaining if $fileBytesRemaining < $readSize; 

    return 0 if $readSize == 0;

    my $buf;
    my $bytesRead = read $fh, $buf, $readSize;

    if ($bytesRead != $readSize)  {
	seek $fh, 0, 1;
	$self->{fileErrMsg}
		= "Read truncated wanted=$readSize, read=$bytesRead";
	$self->{fileErrNum} = -1;
	return;
    }

    $self->{buf} .= $buf;
    $self->{fileOffset} += $bytesRead;

    return $bytesRead;
}


#
# returns (record, recordOffset, nextRecordOffset)	 if record found
#         (					 )	 if no record available
#         (undef , recordOffset, undef, errMsg, errNum)  if error
#
sub GetNextRecord
{
    my $self = shift;

    while (1)  {
	my ($r, $delim, $rest) = split /^(\.{3}\r?\n)/m, $self->{buf}, 2;
	my $recordPos = $self->{bufOffset};

	if (defined $delim)  {
	    # found record
	    $self->{buf} = $rest;
	    $self->{bufOffset} += length($r) + length($delim);

	    return ($r, $recordPos, $self->{bufOffset});
	}  else  {
	    my $amount = $self->ReadFileChunk;

	    if (!defined $amount)  {
		# error occurred
		return (undef, $recordPos, undef,
			    $self->{fileErrMsg}, $self->{fileErrNum});
	    }  elsif ($amount == 0)  {
		# no record found
		return;
	    }
	}
    }
}




package CondorWork;


sub new
{
    my $invocant = shift;

    my $class = ref($invocant) || $invocant;
    my $self = {
		workId		=> undef,
		filename	=> undef,
		userLog		=> undef,
		nextPos		=> 0,
		logErrMsg	=> undef,
		logErrNum	=> undef,
		totalJobs	=> 0,
		completeJobs	=> 0,
		jobs		=> {},
		@_
		};

    die "filename required" unless defined $self->{filename};
    
    die "workId required" unless defined $self->{workId};

    $self->{userLog} = new CondorUserLog(
				filename	=> $self->{filename},
				offset		=> $self->{nextPos}
				);

    bless $self, $class;
    return $self;
}


my $eventRE = qr/^(\d+)\s+			# state
		\((\d+)\.(\d+)\.(\d+)\)\s+	# cluster,proc.subproc
		(\d+)\/(\d+)\s+			# MM DD
		(\d+):(\d+):(\d+)\s+		# hh:mm:ss
		(.*)$/sx;			# info

my $normalTermRE = qr/Normal termination \(return value (\d+)\)/;

my $abnormalTermRE = qr/Abnormal termination \(signal (\d+)\)/;

# states in this array should go in the database
# 	true value indicates state is terminal: 5 (terminated) and 9 (removed)
my %states = map {$_ => ($_ == 5 || $_ == 9)} qw( 0 1 2 4 5 7 9 10 11 12 13);


sub Print
{
    my $self = shift;

    print "----- CondorWork -------\n";
    my @keyNames = qw(workId filename nextPos logErrMsg logErrNum
			totalJobs completeJobs);
    foreach my $k (@keyNames)  {
	my $v = $self->{$k};
	$v = "<undef>" unless defined $v;
	printf "%15s  %s\n", $k, $v;
    }
    printf "%15s  %s\n", "jobsSeen", scalar(keys %{$self->{jobs}});
    print "------------------------\n";
}


sub RestoreJobs
{
    my $self = shift;
    my $jobs = shift;

    %{$self->{jobs}} = %$jobs;
}


sub MakeCondorId
{
    return join '.', @_;
}


sub IsTerminalState
{
    my $state = shift;
    return exists($states{$state}) && $states{$state};
}


sub SetPosition
{
    my $self = shift;
    my $pos = shift;

    $self->{userLog}->SetPosition($pos);
}


sub NumIncompleteJobs
{
    my $self = shift;

    return $self->{totalJobs} - $self->{completeJobs};
}


sub ParseRecordToEvent
{
    my ($record, $recordPos, $nextPos, $logErrMsg, $logErrNum) = @_;
    my %e;

    if (!defined $recordPos)  {
	%e = (
		type		=> 'none'
	    );
    }  elsif (!defined $record)  {
	%e = (
		type		=> 'error',
		recordPos	=> $recordPos,
		errMsg		=> $logErrMsg,
		errNum		=> $logErrNum,
	    );
    }  elsif ($record =~  $eventRE)  {
	%e = (
		type		=> 'event',
		state		=> 0 + $1,
		cluster		=> 0 + $2,
		proc		=> 0 + $3,
		subproc		=> 0 + $4,
		mon		=> 0 + $5,
		day		=> 0 + $6,
		hr		=> 0 + $7,
		min		=> 0 + $8,
		sec		=> 0 + $9,
		info		=> $10,
		exitSignal	=> undef,
		exitCode	=> undef,
		recordPos	=> $recordPos,
		nextPos		=> $nextPos
	    );

	# guess at adjusting the year, since the year isn't in the record :(
	# 	1 month ahead is the future
	# 	everything else is in the past
	my ($mo, $yr) = (localtime time)[4, 5];
	++$mo;
	$yr += 1900;
	++$yr if ($mo == 12 && $e{mo} == 1);
	--$yr if ($mo + 1 < $e{mon});
	$e{yr} = $yr;

	$e{terminal} = IsTerminalState($e{state});

	# get how it terminated if state is 5
	if ($e{state} == 5)  {
	    if ($e{info} =~ $normalTermRE)  {
		$e{exitCode} = 0 + $1;
	    }  elsif ($e{info} =~ $abnormalTermRE)  {
		$e{exitSignal} = 0 + $1;
	    }
	}

	# only certain states get updated in the database
	$e{recordState} = exists $states{$e{state}};

	# create id to use for printing and key values
	$e{id} = MakeCondorId(@e{qw(cluster proc subproc)});

	# create ts in database format
	$e{ts} = "$e{yr}-$e{mon}-$e{day} $e{hr}:$e{min}:$e{sec}";
    }  else  {
	%e = (
		type		=> 'error',
		recordPos	=> $recordPos,
		errMsg		=> "invalid record format at offset $recordPos"
	    );
    }

    return \%e;
}


sub GetNextEvent
{
    my $self = shift;

    my $userLog = $self->{userLog};

    my $event = ParseRecordToEvent($userLog->GetNextRecord());

    return $event;
}


sub GetNewEvents
{
    my $self = shift;

    my $errMsg;
    my $errNum;
    my $nextPos;

    my %jobs;

    while (1)  {
	my $e = $self->GetNextEvent;
	my $type = $e->{type};

	last if $type eq 'none';

	if ($type eq 'event')  {
	    $nextPos = $e->{nextPos};

	    next unless $e->{recordState};

	    my $id = $e->{id};
	    $jobs{$id} = $e unless exists $jobs{$id} && $jobs{$id}->{terminal};
	}  elsif ($type eq 'error')  {
	    if (scalar(keys %jobs) == 0)  {
		$errMsg = $e->{errMsg};
		$errNum = $e->{errNum};
	    }

	    $self->SetPosition($e->{recordPos});

	    last;
	}  else  {
	    die "unknown event type ($type)";
	}
    }

    return (\%jobs, $errMsg, $errNum, $nextPos);
}


sub AddCondorIdsFromLog
{
    my $self = shift;
    my $jobs = shift;

    while (1)  {
	my $e = $self->GetNextEvent;

	if ($e->{type} eq 'event')  {
	    $jobs->{$e->{id}} = 1;
	}  else  {
	    last;
	}
    }
}


sub PrintEvent
{
    my $e = shift;

    my @attrs = qw( type recordPos nextPos state id cluster proc subproc
		    yr mon day hr min sec ts info terminal
		    exitSignal exitCode recordState errMsg errNum );

    print "-------------\n";
    foreach my $k (@attrs)  {
	next unless exists $e->{$k};
	my $v = $e->{$k};
	$v = "<undef>" unless defined $v;
	printf "%-14s: %s\n", $k, $v;
    }
    print "-------------\n";
}


sub RemoveAlreadyTerminatedJobEvents
{
    my $self = shift;
    my $newJobs = shift;
    my $currentJobs = $self->{jobs};

    foreach my $j (keys %$newJobs)  {
	delete $newJobs->{$j} if exists $currentJobs->{$j} && $currentJobs->{$j};
    }
}


sub ApplyJobUpdates
{
    my $self = shift;
    my ($newJobs, $errMsg, $errNum, $nextPos) = @_;
    my $currentJobs = $self->{jobs};
    my $completeJobs = $self->{completeJobs};

    $self->{errMsg} = $errMsg;
    $self->{errNum} = $errNum;
    $self->{nextPos} = $nextPos;

    foreach my $j (keys %$newJobs)  {
	my $jobComplete = $newJobs->{$j}->{terminal};
	$currentJobs->{$j} = $jobComplete;
	++$completeJobs if $jobComplete;
    }

    $self->{completeJobs} = $completeJobs;
}


sub ProcessNewEvents
{
    my $self = shift;
    my ($dbh, $insertJobSth, $updateJobSth, $updateWorkJobsSth,
		$updateWorkCompleteSth, $updateWorkJobsErrorSth) = @_;

    my $currentJobs = $self->{jobs};
    my $id = $self->{id};
    my $success = 1;
    my $completeJobs = $self->{completeJobs};

    my ($newJobs, $errMsg, $errNum, $nextPos) = $self->GetNewEvents;

    if (scalar keys %$newJobs)  {
	$self->RemoveAlreadyTerminatedJobEvents($newJobs);
	my $workId = $self->{workId};

	$dbh->begin_work;

	foreach my $jobId (keys %$newJobs)  {
	    my $job = $newJobs->{$jobId};

	    if (defined $currentJobs->{$jobId})  {
		# existing job
		my @attrs = qw(state info ts exitCode exitSignal	
				    cluster proc subproc);

		my $rows = $updateJobSth->execute(@{$job}{@attrs}, $workId);
		if (!defined $rows || $rows != 1)  {
		    my $dbErr = $dbh->errstr;
		    $success = 0;
		    main::LogAndExit("Update work record failed id=$id: $dbErr\n", 1);
		}
	    }  else  {
		# new job
		my @attrs = qw(state info ts ts
				    cluster proc subproc);

		my $rows = $insertJobSth->execute(@{$job}{@attrs}, $workId);
		if (!defined $rows || $rows != 1)  {
		    my $dbErr = $dbh->errstr;
		    $success = 0;
		    main::LogAndExit("Insert work record failed id=$id: $dbErr\n", 1);
		}
	    }

	    $currentJobs->{$jobId} = $job->{terminal};
	    ++$completeJobs if $job->{terminal};
	}

	my $updateWorkSth;
	if ($completeJobs >= $self->{totalJobs})  {
	    # all jobs for work are done
	    $updateWorkSth = $updateWorkCompleteSth;
	}  else  {
	    # still jobs to complete
	    $updateWorkSth = $updateWorkJobsSth;
	}

	my $rows = $updateWorkSth->execute($nextPos, $completeJobs, $workId);
	if (!defined $rows && $rows != 1)  {
	    my $dbErr = $dbh->errstr;
	    $success = 0;
	    main::LogAndExit("update work record failed id=$id: $dbErr\n", 1);
	}

	if ($success)  {
	    $dbh->commit;
	}  else  {
	    $dbh->rollback;
	}
    }  elsif (defined $errMsg || defined $errNum)  {
	my $rows = $updateWorkJobsErrorSth->execute($nextPos, $errMsg, $errNum, $id);
	if (!defined $rows || $rows != 1)  {
	    my $dbErr = $dbh->errstr;
	    $success = 0;
	    main::LogAndExit("Updating work record failed id=$id: $dbErr\n", 1);
	}
    }

    $self->ApplyJobUpdates($newJobs, $errMsg, $errNum, $nextPos);
}



package CondorActiveWork;


sub new
{
    my $invocant = shift;

    my $class = ref($invocant) || $invocant;
    my $self = {
		work			=> {},
		numIncompleteJobs	=> 0
		};

    bless $self, $class;
    return $self;
}


sub AddWork
{
    my $self = shift;
    my ($workId, $filename, $totalJobs, @remain) = @_;

    my $work = new CondorWork(	workId => $workId,
				filename => $filename,
				totalJobs => $totalJobs,
				@remain);

    $self->{work}->{$workId} = $work;
    $self->{numIncompleteJobs} += $totalJobs;
}


sub NumWork
{
    my $self = shift;
    return scalar keys %{$self->{work}};
}


sub NumIncompleteJobs
{
    my $self = shift;
    return $self->{numIncompleteJobs};
}


sub ProcessWork
{
    my $self = shift;
    my @dbArgs = @_;
    my $work = $self->{work};

    foreach my $id (keys %$work)  {
	my $w = $work->{$id};
	my $prevNumIncompleteJobs = $w->NumIncompleteJobs;
	my $r = $w->ProcessNewEvents(@dbArgs);
	my $curNumIncompleteJobs = $w->NumIncompleteJobs;
	$self->{numIncompleteJobs} -= $prevNumIncompleteJobs - $curNumIncompleteJobs;
	delete $work->{$id} if $curNumIncompleteJobs == 0;
    }
}


my $condorQRe = qr/\s*^(?:(\d+)\s+)?(\d+)\s+(\d+)\s*$/;

sub CheckIfWorkSubmitted
{
    my $self = shift;
    my ($workId, $filename) = @_;
    my %jobs;

    my @cmd = (
		$condorQCmd,
		'-format',	'%d ',		'subprocid',
		'-format',	'%d ',		'procid',
		'-format',	'%d\n',		'clusterid',
		'-constraint',	"$cdbqIdAttr == $workId"
		);

    my ($exitCode, $exitSignal, $out, $err) = main::ExecuteCommand(\@cmd, '');
    
    if ($exitCode != 0 || $exitSignal != 0)  {
	$out = '' unless defined $out;
	$err = '' unless defined $err;
	main::LogAndExit("$condorQCmd failed to recover"
			. "$cdbqIdAttr=$workId\n$out\n---\n$err\n", 1);
    }

    foreach my $line (split /\n/, $out)  {
	next if $line =~ /^\s*$/;
	my ($sp, $p, $c) = ($line =~ /$condorQRe/);
	$sp = 0 unless defined $sp;
	main::LogAndExit("$condorQCmd produced invalid line: $line", 1)
		unless defined $c;

	my $condorId = CondorWork::MakeCondorId($c, $p, $sp);
	$jobs{$condorId} = 1;
    }

    my $work = new CondorWork(filename => $filename, workId => $workId);
    $work->AddCondorIdsFromLog(\%jobs);

    return scalar(keys %jobs);
}


sub RecoverChosenWork
{
    my $self = shift;
    my ($getChosenWorkSth, $submitSuccessSth, $revertChosenSth) = @_;

    my %jobsSeen;
    my $numJobs = scalar keys %jobsSeen;
    my $cluster;

    my %work;
    $getChosenWorkSth->execute();
    while (my @data = $getChosenWorkSth->fetchrow_array())  {
	my ($workId, $filename) = @data;
	$work{$workId} = $filename;
    }
    $getChosenWorkSth->finish();

    foreach my $workId (keys %work)  {
	my $numJobs = $self->CheckIfWorkSubmitted($workId, $work{$workId});
	if ($numJobs > 0)  {
	    my $rows = $submitSuccessSth->execute(undef, undef, $numJobs, $workId);
	    if (!defined $rows || $rows != 1)  {
		LogAndExit("Error: Updating db for successful submit", 1);
	    }
	}  else  {
	    my $rows = $revertChosenSth->execute($workId);
	    if (!defined $rows || $rows != 1)  {
		LogAndExit("Error: Updating db for successful submit", 1);
	    }
	}
    }
}


sub RestoreStateFromDatabase
{
    my $self = shift;

    my $getInBatchWorkSth = shift;
    my $getInBatchJobsSth = shift;
    my $activeWork = $self->{work};

    $getInBatchWorkSth->execute();
    while (my @data = $getInBatchWorkSth->fetchrow_array())  {
	my ($workId, $filename, $nextPos, $logErrMsg, $logErrNum,
		    $totalJobs, $completeJobs) = @data;
	my $work = new CondorWork(
				workId		=> $workId,
				filename	=> $filename,
				nextPos		=> $nextPos,
				logErrMsg	=> $logErrMsg,
				logErrNum	=> $logErrNum,
				totalJobs	=> $totalJobs,
				completeJobs	=> $completeJobs
				);
	$activeWork->{$workId} = $work;
	$self->{numIncompleteJobs} += $totalJobs - $completeJobs;
	print "Restore $workId totalJobs=$totalJobs completeJobs=$completeJobs\n";
    }
    $getInBatchWorkSth->finish();

    my %jobs;
    $getInBatchJobsSth->execute();
    while (my @data = $getInBatchJobsSth->fetchrow_array())  {
	my ($workId, $cluster, $proc, $subproc, $state) = @data;
	my $condorId = CondorWork::MakeCondorId($cluster, $proc, $subproc);
	$jobs{$workId}{$condorId} = CondorWork::IsTerminalState($state);
    }
    $getInBatchJobsSth->finish();

    foreach my $workId (keys %jobs)  {
	print "Restore $workId seen-jobs=", scalar(keys %{$jobs{$workId}}), "\n";
	$activeWork->{$workId}->RestoreJobs($jobs{$workId});
    }
}


sub Recover
{
    my $self = shift;
    my ($getInBatchWorkSth, $getInBatchJobsSth, $getChosenWorkSth,
		$submitSuccessSth, $revertChosenSth) = @_;

    $self->RecoverChosenWork($getChosenWorkSth,
			    $submitSuccessSth, $revertChosenSth);
    $self->RestoreStateFromDatabase($getInBatchWorkSth, $getInBatchJobsSth);
}



package main;


my $tablePrefix = '';
my $workTable = $tablePrefix . 'work';
my $jobsTable = $tablePrefix . 'jobs';

my %options;
my %sqlStmts;

my %defaultDbInfoFiles = (
			admin	=> 'db.admin.conf',
			submit	=> 'db.submit.conf',
			worker	=> 'db.worker.conf'
			);


sub InitializeSqlStmts
{
    %sqlStmts = (

    createWorkTable => qq{
CREATE TABLE $workTable  (
    work_data		TEXT		NOT NULL,
    create_ts		TIMESTAMP	NOT NULL DEFAULT current_timestamp,
    id			BIGINT		PRIMARY KEY,
	    -- DEFAULT NEXTVAL('${workTable}_seq') handled by trigger
    insert_user		TEXT		NOT NULL DEFAULT user,
    state		TEXT		NOT NULL DEFAULT 'initial',
            -- values:  initial  chosen  in_batch  complete  failed
    cdbq_user		TEXT,
    batch_sys		TEXT,
    cmd_stdout		TEXT,
    cmd_stderr		TEXT,
    cmd_exit_code	INTEGER,
    cmd_exit_signal	INTEGER,
    log_file		TEXT,
    next_pos		INTEGER,
    total_jobs		INTEGER,
    complete_jobs	INTEGER,
    log_err_msg		TEXT,
    log_err_num		INTEGER,
    update_ts		TIMESTAMP,
    user_id		INTEGER,
    user_text		TEXT
)
    },

    createJobsTable => qq{
CREATE TABLE $jobsTable  (
    work_id		BIGINT		REFERENCES $workTable ON DELETE CASCADE,
    cluster		INTEGER		NOT NULL,
    proc		INTEGER		NOT NULL,
    subproc		INTEGER		NOT NULL,
    state		INTEGER		NOT NULL,
    info		TEXT		NOT NULL,
    record_ts		TIMESTAMP	NOT NULL,
    create_ts		TIMESTAMP	NOT NULL,
    update_ts		TIMESTAMP	NOT NULL,
    exit_code		INTEGER,
    exit_signal		INTEGER,

    PRIMARY KEY (work_id, cluster, proc, subproc)
)
    },

    createWorkSeq		=> "CREATE SEQUENCE ${workTable}_seq",

    createWorkFunction => qq{
CREATE OR REPLACE FUNCTION ${workTable}_trigger() RETURNS trigger AS
\$\$
BEGIN
    IF TG_OP = 'INSERT' THEN
	NEW.create_ts = current_timestamp;
	NEW.id = NEXTVAL('${workTable}_seq');
	NEW.cdbq_user = NULL;
	NEW.state = 'initial';
	NEW.insert_user = user;
    ELSIF TG_OP = 'UPDATE' THEN
	IF OLD.state = 'initial' THEN
	    NEW.cdbq_user = user;
	    NEW.state = 'chosen';
	ELSIF OLD.cdbq_user <> user THEN
	    RAISE EXCEPTION 'user ''\%\%'' not allowed to update $workTable id=%%',
				    user,                             OLD.id;
	END IF;
	NEW.create_ts = OLD.create_ts;
	NEW.id = OLD.id;
    END IF;

    RETURN NEW;
END
\$\$ LANGUAGE plpgsql
    },

    createWorkTrigger => qq{
CREATE TRIGGER ${workTable}_trigger
    BEFORE INSERT OR UPDATE
    ON $workTable
    FOR EACH ROW EXECUTE PROCEDURE ${workTable}_trigger()
    },


    createJobsFunction => q{
CREATE OR REPLACE FUNCTION jobs_trigger() RETURNS trigger AS
$$
BEGIN
    IF TG_OP = 'DELETE' THEN
	IF OLD.state <> 5 AND OLD.state <> 9 THEN
	    RAISE EXCEPTION 'job ''%%'' still in batch system', OLD.work_id;
	END IF;

	RETURN OLD;
    END IF;

    RETURN NEW;
END
$$ LANGUAGE plpgsql
    },

    createJobsTrigger => qq{
CREATE TRIGGER jobs_trigger
    BEFORE DELETE
    ON $jobsTable
    FOR EACH ROW EXECUTE PROCEDURE jobs_trigger()
    },

    createSubmitUser		=> 'CREATE USER %s PASSWORD \'%s\'',

    grantSubmitUserWorkTable	=> "GRANT SELECT, INSERT ON $workTable TO \%s",

    grantSubmitUserJobsTable	=> "GRANT SELECT ON $jobsTable TO \%s",

    grantSubmitUserWorkSeq	=> "GRANT USAGE on ${workTable}_seq TO \%s",

    createExecuteUser		=> 'CREATE USER %s PASSWORD \'%s\'',

    grantExecuteUserWorkTable	=> "GRANT SELECT, UPDATE ON $workTable TO \%s",

    grantExecuteUserJobsTable	=> "GRANT SELECT, INSERT, UPDATE ON $jobsTable TO \%s",

    revokePublicSchemaCreate => qq(
REVOKE
    CREATE
    ON SCHEMA public
    FROM public
    CASCADE
    ),

    createLangPlpgsql		=> "create language plpgsql",

    updateWorkToChosen => qq{
UPDATE $workTable
    SET batch_sys = ?, state = 'chosen', log_file = ? || '/work.' || id || '.log'
    WHERE id in
	(SELECT id
	    FROM $workTable
	    WHERE state = 'initial'
	    ORDER BY id
	    LIMIT $options{grabamount})
    },

    selectChosenWork => qq{
SELECT id, work_data, log_file
    FROM $workTable
    WHERE state = 'chosen' AND batch_sys = ?
    ORDER BY id
    },

    updateWorkToInBatch => qq{
UPDATE $workTable
    SET state = 'in_batch',
	cmd_exit_code = 0, cmd_exit_signal = 0,
	cmd_stdout = ?, cmd_stderr = ?,
	total_jobs = ?, complete_jobs = 0
    WHERE id = ?
    },

    updateWorkToTmpFailed => qq{
UPDATE $workTable
    SET state = 'initial',
	cmd_exit_code = ?, cmd_exit_signal = 0,
	cmd_stdout = ?, cmd_stderr = ?
    WHERE id = ?
    },

    updateWorkToFailed => qq{
UPDATE $workTable
    SET state = 'failed',
	cmd_exit_code = ?, cmd_exit_signal = ?,
	cmd_stdout = ?, cmd_stderr = ?
    WHERE id = ?
    },

    insertJob => qq{
INSERT INTO $jobsTable
    (state, info, record_ts, create_ts, update_ts,
    cluster, proc, subproc, work_id)
    VALUES  (?, ?, ?, ?, current_timestamp, ?, ?, ?, ?)
    },

    updateJob => qq{
UPDATE $jobsTable
    SET state = ?, info = ?, record_ts = ?, exit_code = ?, exit_signal = ?,
	update_ts = current_timestamp
    WHERE cluster = ? AND proc = ? AND subproc = ? AND work_id = ?
    },

    updateWorkJobs => qq(
UPDATE $workTable
    SET next_pos = ?, complete_jobs = ?, update_ts = current_timestamp
    WHERE id = ?
    ),

    updateWorkComplete => qq(
UPDATE $workTable
    SET state = 'complete', next_pos = ?, complete_jobs = ?,
	update_ts = current_timestamp
    WHERE id = ?
    ),

    updateWorkJobsError => qq(
UPDATE $workTable
    SET next_pos = ?, log_err_msg = ?, log_err_num = ?,
	update_ts = current_timestamp
    WHERE id = ?
    ),

    getChosenWork => qq(
SELECT id, log_file
    FROM $workTable
    WHERE state = 'chosen'
    ),

    updateWorkToInitial => qq(
UPDATE $workTable
    SET state = 'initial'
    WHERE id = ?
    ),

    updateAllChosenWorkToInitial => qq(
UPDATE $workTable
    SET state = 'initial'
    WHERE state = 'chosen'
    ),

    getInBatchWork => qq(
SELECT id, log_file, next_pos, log_err_msg, log_err_num,
	total_jobs, complete_jobs
    FROM $workTable
    WHERE state = 'in_batch'
    ),

    getInBatchJobs => qq(
SELECT $workTable.id, $jobsTable.cluster, proc, subproc, $jobsTable.state
    FROM $workTable, $jobsTable
    WHERE $workTable.state = 'in_batch' AND id = work_id
    ORDER BY id
    )

    );
}


sub GetDbConnectionInfo
{
    my ($dbInfoDir, $dbInfoFile, $dbInfoType) = @_;

    my $defaultDbInfoFile = $defaultDbInfoFiles{$dbInfoType};

    die "unknown dbinfotype=$dbInfoType" unless defined $defaultDbInfoFile;

    $dbInfoFile = "$dbInfoDir/$defaultDbInfoFile" if $dbInfoFile eq '';

    if ($dbInfoFile eq '-')  {
	open DBINFOFILE, '-'
		or LogAndExit("open db info file '-' failed: $!", 1);
    }  else  {
	open DBINFOFILE, '<', $dbInfoFile
		or LogAndExit("open db info file '$dbInfoFile' failed: $!", 1);
    }

    my $dataSource = <DBINFOFILE>;
    my $user = <DBINFOFILE>;
    my $password =<DBINFOFILE>;

    close DBINFOFILE or LogAndExit("close db info file '$dbInfoFile' failed: $!", 1);

    LogAndExit("db info file '$dbInfoFile' is empty", 1) unless defined $dataSource;
    chomp $dataSource;
    LogAndExit("db info file '$dbInfoFile' missing user", 1) unless defined $user;
    LogAndExit("db info file '$dbInfoFile' bad user '$user' is non-alphanumeric", 1) unless $user =~ /^\w+$/;
    chomp $user;
    chomp $password if defined $password;

    return ($dataSource, $user, $password);
}


sub ConnectToDb
{
    my ($dbInfoDir, $dbInfoFile, $dbInfoType) = @_;
    my ($dataSource, $user, $password)
		= GetDbConnectionInfo($dbInfoDir, $dbInfoFile, $dbInfoType);

    my %dbAttrs = (AutoCommit => 1, PrintError => 0, PrintWarn => 0);
    my $dbh = DBI->connect($dataSource, $user, $password, \%dbAttrs) or die;

    return $dbh;
}


sub ExecuteDbOperation
{
    my ($dbh, $options, $stmtId, @params) = @_;

    die "unknown db stmt '$stmtId'" unless defined $sqlStmts{$stmtId};

    my $stmt = sprintf $sqlStmts{$stmtId}, @params;

    if ($options->{noexecute})  {
	$stmt =~ s/\s*$//;
	print "\n$stmt;\n";
    }  else  {
	my $rows = $dbh->do($stmt);
	unless (defined $rows)  {
	    my $dbErr = $dbh->errstr;
	    LogAndExit("Db stmt failed: $dbErr\n$stmt", 1);
	}
    }
}


my %preparedStmtCache;

sub GetPreparedStmt
{
    my ($dbh, $stmtId) = @_;
    my $sth;

    if (exists $preparedStmtCache{$stmtId})  {
	$sth = $preparedStmtCache{$stmtId};
    }  else  {
	die "unknown db stmt '$stmtId'" unless defined $sqlStmts{$stmtId};

	my $stmt = $sqlStmts{$stmtId};

	$sth = $dbh->prepare($stmt) or die "prepare failed '$stmt'";
    }

    return $sth;
}


sub DoCreateLangPlpgsql
{
    my ($dbh, $options) = @_;

    ExecuteDbOperation($dbh, $options, 'createLangPlpgsql');
}


sub DoRevokePublicSchemaCreate
{
    my ($dbh, $options) = @_;

    ExecuteDbOperation($dbh, $options, 'revokePublicSchemaCreate');
}


sub DoInitDb
{
    my ($dbh, $options) = @_;

    ExecuteDbOperation($dbh, $options, 'createWorkTable');
    ExecuteDbOperation($dbh, $options, 'createJobsTable');
    ExecuteDbOperation($dbh, $options, 'createWorkSeq');

    ExecuteDbOperation($dbh, $options, 'createWorkFunction');
    ExecuteDbOperation($dbh, $options, 'createWorkTrigger');

    ExecuteDbOperation($dbh, $options, 'createJobsFunction');
    ExecuteDbOperation($dbh, $options, 'createJobsTrigger');
}


sub DoCreateSubmitUser
{

    my ($dbh, $options) = @_;

    my $dbInfoFile = $options->{createsubmituser};
    my $dbInfoDir = $options->{dbinfodir};
    my ($dataSource, $user, $password) 
		    = GetDbConnectionInfo($dbInfoDir, $dbInfoFile, 'submit');

    ExecuteDbOperation($dbh, $options, 'createSubmitUser', $user, $password);
    ExecuteDbOperation($dbh, $options, 'grantSubmitUserWorkTable', $user);
    ExecuteDbOperation($dbh, $options, 'grantSubmitUserJobsTable', $user);
    ExecuteDbOperation($dbh, $options, 'grantSubmitUserWorkSeq', $user);
}


sub DoCreateWorkUser
{

    my ($dbh, $options) = @_;

    my $dbInfoFile = $options->{createworkuser};
    my $dbInfoDir = $options->{dbinfodir};
    my ($dataSource, $user, $password) 
		    = GetDbConnectionInfo($dbInfoDir, $dbInfoFile, 'worker');

    ExecuteDbOperation($dbh, $options, 'createExecuteUser', $user, $password);
    ExecuteDbOperation($dbh, $options, 'grantExecuteUserWorkTable', $user);
    ExecuteDbOperation($dbh, $options, 'grantExecuteUserJobsTable', $user);
}


my $progVersion = '1.0b3';

my $progName = $0;
$progName =~ s/.*[\\\/]//;


sub Log
{
    my $data = shift;

    print STDERR "$data\n";
}


sub LogAndExit
{
    my ($data, $code) = @_;

    Log $data;

    exit $code;
}




sub Usage
{
    print STDERR <<EOF;
Usage: $progName [options]...

    --dbinfofile      -f  db info file
    --dbinfodir       -a  db info dir
    --logdir          -d  user log directory
    --sleepamount         number of seconds to sleep between iterations
    --maxwork             maximum number of submitted work (0 is infinite)
    --maxjobs             maximum number of incomplete jobs (0 is infinite)
    --grabamount          number of new work pieces to grab at once
    --id                  id for the queue

    --initdb              initialize database
    --createlang          create plpgsql language in database
    --revokepublic        revoke create access to public schema
    --createsubmituser    add a submit db account from db info file
    --createworkuser      add an execute db account from db info file

    --submit              submit Condor job to db

    --noexecute       -n  just print sql commands

    --help            -h  print this message
    --version         -v  print version number
EOF
}


sub ProcessOptions
{
    %options = (
		    dbinfofile		=> '',
		    dbinfodir		=> '.',
		    logdir		=> '.',
		    sleepamount		=> 10,
		    maxwork		=> 0,
		    maxjobs		=> 0,
		    grabamount		=> 10,
		    id			=> '',
		    initdb		=> 0,
		    createlang		=> 0,
		    revokepublic	=> 0,
		    createsubmituser	=> undef,
		    createworkuser	=> undef,
		    submit		=> undef,
		    noexecute		=> 0,
		    help		=> 0,
		    version		=> 0
		);

    my @options = (
		    "dbinfofile|f=s",
		    "dbinfodir|a=s",
		    "logdir|d=s",
		    "sleepamount=i",
		    "maxwork=i",
		    "maxjobs=i",
		    "grabamount=i",
		    "id=s",
		    "initdb!",
		    "createlang!",
		    "revokepublic!",
		    "createsubmituser:s",
		    "createworkuser:s",
		    "submit=s",
		    "noexecute|n!",
		    "help|h!",
		    "version|v!"
		);

    my $ok = GetOptions(\%options, @options);
    if (!$ok || $options{help})  {
	Usage();
	exit !$ok;
    }

    if ($options{version})  {
	print "$progName: $progVersion\n";
	exit 0;
    }

    $options{id} = `hostname` if $options{id} eq '';
    chomp $options{id};

    if (!-d $options{logdir})  {
	print STDERR "User log directory '$options{logdir}' does not exist\n";
	exit 1;
    }
}


sub Initialize
{
    ProcessOptions();
    InitializeSqlStmts();
}


sub CheckCondorSubmitRunning
{
    my $cmd = "$condorQCmd 2>&1 >/dev/null";
    `$cmd`;

    return $? == 0;
}


sub GetFileContents
{
    my ($filename) = @_;

    local $/;
    if ($filename eq '-')  {
	open INFILE, '-' or LogAndExit("open '-': $!", 1);
    }  else  {
	open INFILE, '<', $filename or LogAndExit("open < '$filename': $!", 1);
    }
    my $data = <INFILE>;
    close INFILE or LogAndExit("close $filename: $!", 1);

    return $data;
}


sub ShellQuoteArg
{
    my $data = shift;

    $data =~ s/'/'\\''/g;
    $data = "'$data'" if $data =~ tr|A-Za-z0-9/-_||c;

    return $data;
}


sub ExecuteCommand
{
    my ($cmdToExec, $inData, $timeout) = @_;

    $inData = '' unless defined $inData;

    use IPC::Open3;
    use Fcntl;
    use Errno qw{:POSIX};

    local $SIG{PIPE} = 'IGNORE';

    local (*CMDIN, *CMDOUT, *CMDERR);

    # The following block of code should just be
    #
    #   my $cmdPid = open3(\*CMDIN, \*CMDOUT, \*CMDERR, @$cmdToExec);
    #
    # A bug in open3 causes die to be called in the event that exec fails
    # which has the side effect of running all the destructors, which
    # closes all of our database handles.  open3 should only ever throw
    # an exception in the child and only if the exec fails, so try and
    # catch it and exit immediately to work around the open3 bug.
    #
    my $cmdPid;
    eval {
	$cmdPid = open3(\*CMDIN, \*CMDOUT, \*CMDERR, @$cmdToExec);
    };
    if ($@)  {
	print STDERR "ExecuteCommand: exec failed: '" . join("', '", @$cmdToExec), "'";
	eval { require POSIX; POSIX::_exit(255); };
	exit 255;
    }

    my $cmdinFlags = fcntl(CMDIN, F_GETFL, 0);
    fcntl(CMDIN, F_SETFL, $cmdinFlags | O_NONBLOCK);

    my $inFileno = fileno(CMDIN);
    my $outFileno = fileno(CMDOUT);
    my $errFileno = fileno(CMDERR);

    my $outData = '';
    my $outLen = 0;
    my $errData = '';
    my $errLen = 0;

    my $inLen = length($inData);
    my $inOffset = 0;

    my $openFds = 3;

    if ($inLen == 0)  {
	$inFileno = -1;
	--$openFds;
	close(CMDIN) or die "close CMDIN: $!";
    }

    while ($openFds > 0)  {
	my $readVec = '';
	my $writeVec = '';

	vec($readVec, $outFileno, 1) = 1 unless $outFileno == -1;
	vec($readVec, $errFileno, 1) = 1 unless $errFileno == -1;
	vec($writeVec, $inFileno, 1) = 1 unless $inFileno == -1;

	my $numFds = select($readVec, $writeVec, undef, $timeout);
	if ($numFds == -1)  {
	    if ($!{EINTR} || $!{EAGAIN})  {
		redo;
	    }  else  {
		die "select failed: $!";
	    }
	}  elsif ($numFds == 0)  {
	    kill 9, $cmdPid;
	    waitpid $cmdPid, 0;
	    die "select timeout expired, retrying";
	}

	if (vec($readVec, $outFileno, 1))  {
	    my $bytesRead = sysread(CMDOUT, $outData, 1024, $outLen);
	    if (defined $bytesRead)  {
		if ($bytesRead > 0)  {
		    $outLen += $bytesRead;
		}  elsif ($bytesRead == 0)  {
		    $outFileno = -1;
		    --$openFds;
		    close(CMDOUT) or die "close CMDOUT: $!";
		}
	    }  else  {
		if (!$!{EINTR} && !$!{EAGAIN})  {
		    kill 9, $cmdPid;
		    waitpid $cmdPid, 0;
		    die "sysread CMDOUT: $!";
		}
	    }
	}

	if (vec($readVec, $errFileno, 1))  {
	    my $bytesRead = sysread(CMDERR, $errData, 1024, $errLen);
	    if (defined $bytesRead)  {
		if ($bytesRead > 0)  {
		    $errLen += $bytesRead;
		}  elsif ($bytesRead == 0)  {
		    $errFileno = -1;
		    --$openFds;
		    close(CMDERR) or die "close CMDERR: $!";
		}
	    }  else  {
		if (!$!{EINTR} && !$!{EAGAIN})  {
		    kill 9, $cmdPid;
		    waitpid $cmdPid, 0;
		    die "sysread CMDERR: $!";
		}
	    }
	}

	if (vec($writeVec, $inFileno, 1))  {
	    my $bytesToWrite = $inLen - $inOffset;
	    my $bytesWritten = syswrite(CMDIN, $inData, $bytesToWrite, $inOffset);
	    if (defined $bytesWritten)  {
		if ($bytesWritten > 0)  {
		    $inOffset += $bytesWritten;
		    if ($inOffset >= $inLen)  {
			$inFileno = -1;
			--$openFds;
			close(CMDIN) or die "close CMDIN: $!";
		    }
		}
	    }  else  {
		if ($!{EPIPE})  {
		    $inFileno = -1;
		    --$openFds;
		    close(CMDIN) or die "close CMDIN: $!";
		}  elsif (!$!{EINTR} && !$!{EAGAIN})  {
		    kill 9, $cmdPid;
		    waitpid $cmdPid, 0;
		    die "sysread CMDERR: $!";
		}
	    }
	}
    }

    waitpid $cmdPid, 0;

    my $exitCode = $? >> 8;
    my $signalValue = $? & 127;

    return ($exitCode, $signalValue, $outData, $errData);
}


my $scheddDownRE = qr/^ERROR: Can't find address of local schedd/m;
my $parseScheddNumAndCluster = qr/^(\d+) job\(s\) submitted to cluster \d+/m;

sub SubmitToCondor
{
    my ($sysId, $workId, $subFile, $logFile) = @_;

    my @cmd = ($condorSubmitCmd,
		'-a', "log = $logFile",
		'-a', "+$cdbqIdAttr = $workId",
		'-a', "+$cdbqSysAttr = \"$sysId\""
		);
    my ($exitCode, $exitSignal, $out, $err) = ExecuteCommand(\@cmd, $subFile);

    my $isTmpErr
	= ($exitSignal == 0 && $exitCode != 0 && $err =~ /$scheddDownRE/);

    my $numJobs = 0;
    while ($out =~ /$parseScheddNumAndCluster/g)  {
	$numJobs += $1;
    }

    return ($exitCode, $exitSignal, $out, $err, $isTmpErr, $numJobs);
}


sub DoAdminCmds
{
    my ($options) = @_;

    my $dbh = ConnectToDb($options->{dbinfodir}, $options->{dbinfofile}, 'admin');

    if ($options->{createlang})  {
	DoCreateLangPlpgsql($dbh, $options);
    }

    if ($options->{revokepublic})  {
	DoRevokePublicSchemaCreate($dbh, $options);
    }

    if ($options->{initdb})  {
	DoInitDb($dbh, $options);
    }

    if (defined $options->{createsubmituser})  {
	DoCreateSubmitUser($dbh, $options);
    }

    if (defined $options->{createworkuser})  {
	DoCreateWorkUser($dbh, $options);
    }

    $dbh->disconnect() or die;
}


sub DoSubmit
{
    my ($options) = @_;

    my $dbh = ConnectToDb($options->{dbinfodir}, $options->{dbinfofile}, 'submit');

    my $submit = GetFileContents($options->{submit});

    if ($options->{noexecute})  {
	$submit =~ s/'/''/g;
	print "\nINSERT INTO $workTable (work_data) VALUES ('$submit');\n";
    }  else  {
	my $stmt = "INSERT INTO $workTable (work_data) VALUES (?);";
	my $rows = $dbh->do($stmt, undef, $submit);
	unless (defined $rows)  {
	    my $dbErr = $dbh->errstr;
	    LogAndExit("Db stmt failed: $dbErr\n$stmt", 1);
	}
    }

    $dbh->disconnect() or die;
}


sub DoGrabWork
{
    my ($options, $dbh, $sth) = @_;

    my $id = $options->{id};
    my $logDir = $options->{logdir};

    my $rows = $sth->execute($id, $logDir);
    if (!defined $rows)  {
	LogAndExit("GrabWork failed", 1);
    }
}


sub DoSubmitWork
{
    my ($activeWork, $options, $dbh, $chosenWorkSth, $submitSuccessSth,
			    $submitFailSth, $submitTmpFailSth,
			    $updateAllChosenWorkToInitialSth) = @_;

    my $id = $options->{id};
    my $maxJobs = $options->{maxjobs};
    my $maxWork = $options->{maxwork};

    my @work;

    $chosenWorkSth->execute($id);

    while (my @data = $chosenWorkSth->fetchrow_array())  {
	my $w = {work_id => $data[0], work_data => $data[1], log_file => $data[2]};

	push @work, $w;
    }
    $chosenWorkSth->finish();

    while (@work)  {
	my $w = shift @work;
	my $workId = $w->{work_id};
	my $workData = $w->{work_data};
	my $logFile = $w->{log_file};

	if (($maxJobs > 0 && $activeWork->NumIncompleteJobs >= $maxJobs)
		    || ($maxWork > 0 && $activeWork->NumWork >= $maxWork))  {
	    my $rows = $updateAllChosenWorkToInitialSth->execute();
	    print "revert rows = $rows\n";
	    if (!defined $rows || $rows == 0)  {
		LogAndExit("Error: Reverting all chosen work to initial", 1);
	    }
	    last;
	}

	# set a temporary error here
	die "$logFile exists" if -e $logFile;

	my ($exitCode, $exitSignal, $out, $err, $isTmpErr, $numJobs)
			= SubmitToCondor($id, $workId, $workData, $logFile);

	$err .= "Unable to extract number of jobs from submission output\n"
		    unless $exitSignal != 0 || $exitCode != 0 || defined $numJobs;

	if ($exitSignal == 0 && $exitCode == 0 && defined $numJobs)  {
	    my $rows = $submitSuccessSth->execute($out, $err, $numJobs, $workId);
	    if (!defined $rows || $rows != 1)  {
		LogAndExit("Error: Updating db for successful submit", 1);
	    }
	    $activeWork->AddWork($workId, $logFile, $numJobs);
	}  elsif ($isTmpErr)  {
	    my $rows = $submitTmpFailSth->execute($exitCode,
						    $out, $err, $workId);
	    if (!defined $rows || $rows != 1)  {
		LogAndExit("Error: Updating db for temporary failed submit", 1);
	    }
	}  else  {
	    my $rows = $submitFailSth->execute($exitCode, $exitSignal,
						    $out, $err, $workId);
	    if (!defined $rows || $rows != 1)  {
		print $dbh->errstr, "\n";
		LogAndExit("Error: Updating db for failed submit", 1);
	    }
	}
    }
}


sub DoProcessQueue
{
    my ($options) = @_;

    my $id = $options{id};
    my $sleepAmount = $options{sleepamount};
    my $maxWork = $options{maxwork};
    my $maxJobs = $options{maxjobs};

    my $dbh = ConnectToDb($options->{dbinfodir}, $options->{dbinfofile}, 'worker');

    my $grabSth			= GetPreparedStmt($dbh, 'updateWorkToChosen');
    my $chosenWorkSth		= GetPreparedStmt($dbh, 'selectChosenWork');
    my $submitSuccessSth	= GetPreparedStmt($dbh, 'updateWorkToInBatch');
    my $submitFailSth		= GetPreparedStmt($dbh, 'updateWorkToFailed');
    my $submitTmpFailSth	= GetPreparedStmt($dbh, 'updateWorkToTmpFailed');
    my $updateAllChosenWorkToInitialSth	= GetPreparedStmt($dbh, 'updateAllChosenWorkToInitial');
    my $insertJobSth		= GetPreparedStmt($dbh, 'insertJob');
    my $updateJobSth		= GetPreparedStmt($dbh, 'updateJob');
    my $updateWorkJobsSth	= GetPreparedStmt($dbh, 'updateWorkJobs');
    my $updateWorkCompleteSth	= GetPreparedStmt($dbh, 'updateWorkComplete');
    my $updateWorkJobsErrorSth	= GetPreparedStmt($dbh, 'updateWorkJobsError');
    my $getInBatchWorkSth	= GetPreparedStmt($dbh, 'getInBatchWork');
    my $getInBatchJobsSth	= GetPreparedStmt($dbh, 'getInBatchJobs');
    my $getChosenWorkSth	= GetPreparedStmt($dbh, 'getChosenWork');
    my $revertChosenSth		= GetPreparedStmt($dbh, 'updateWorkToInitial');

    my $activeWork = new CondorActiveWork;

    $activeWork->Recover($getInBatchWorkSth, $getInBatchJobsSth,
			$getChosenWorkSth, $submitSuccessSth, $revertChosenSth);

    my $done = 0;
    while (!$done)  {
	if (($maxJobs == 0 || $activeWork->NumIncompleteJobs < $maxJobs)
		    && ($maxWork == 0 || $activeWork->NumWork < $maxWork))  {
	    DoGrabWork($options, $dbh, $grabSth);

	    DoSubmitWork($activeWork, $options, $dbh, $chosenWorkSth,
			    $submitSuccessSth, $submitFailSth, $submitTmpFailSth,
			    $updateAllChosenWorkToInitialSth);
	}

	$activeWork->ProcessWork($dbh,
				$insertJobSth, $updateJobSth, $updateWorkJobsSth,
				$updateWorkCompleteSth, $updateWorkJobsErrorSth);

	sleep $sleepAmount;
    }

    $dbh->disconnect() or die;
}


sub Main
{
    my $processQueue = 1;

    Initialize();

    if (       $options{initdb}
	    || $options{createlang}
	    || $options{revokepublic}
	    || defined $options{createsubmituser}
	    || defined $options{createworkuser}
	    )  {
	DoAdminCmds(\%options);
	$processQueue = 0;
    }

    if ($options{submit})  {
	DoSubmit(\%options);
	$processQueue = 0;
    }

    if ($processQueue)  {
	DoProcessQueue(\%options);
    }
}


Main();
