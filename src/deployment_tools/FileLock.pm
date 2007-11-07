##**************************************************************
##
## Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
## University of Wisconsin-Madison, WI.
## 
## Licensed under the Apache License, Version 2.0 (the "License"); you
## may not use this file except in compliance with the License.  You may
## obtain a copy of the License at
## 
##    http://www.apache.org/licenses/LICENSE-2.0
## 
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
##**************************************************************

##*****************************************************************
## Functions to execute a variety of file locks, including 
## diagnostic functions to determine type of locks that will work
## in the given file system.
##*****************************************************************

package FileLock;
require Exporter;
use Fcntl ':flock'; # import LOCK_* constants]
use English;
use Sys::Hostname;
use Socket;

our @ISA = qw(Exporter);
our @EXPORT = qw(AcquireFLock AcquireFLockNB
		 ReleaseFLock TestFLock);  #Symbols to export by default

our $VERSION = 1.01;  #Version number

#***
# Constant Static Variables
#***
my $EXCLUSIVE_LOCK = 'X';
my $SHARED_LOCK = 'S';
my $MAX_BACKOFF = 60;

######################################################################
######################################################################
## WARNING!!!!
## All of the linklock functions are deprecated.  They do not work
## with versions of AFS installed in Linux 2.6.  It may not work on
## all versions of NFS.
## These functions are therefore no longer exported.
## Reexport at your own risk
## -Joe Meehean
######################################################################
######################################################################

#=====================================================================
# Exported Functions
#=====================================================================

#*********************************************************************
# Attempts to acquire a link-lock.  The link-lock will automatically
# expire if this program dies or is exec'ed over top of.
# Returns a "lock handle" that is required to unlock this link-lock.
# WARNING: Due to the inconsistency in implementations of NFS this
# may not work on all versions of NFS.
#*********************************************************************
sub AcquireLinkLock{

    my ($lock_file) = @_;  #Name the parameters

    # Ensure the file exists
    if( !-e $lock_file ){
	die "FAILED: $lock_file lock file does not exist";
    }

    # Create a unique link file name (using pid and IP)
    my $link_file = &AddDynamicPostFix($lock_file);

    # Create memory for the "lock handle" (file handle for link file) 
    my $link_fh = 0;
    
    # Execute in an eval to catch runtime exceptions and clean up
    eval{
	
	# Attemp to get the lock
	&AttemptLinkLock($lock_file, $link_file);
	
	# Setup Link Lock File Semantics
	# Namely that if this program dies, the lock will be released
	$link_fh = &SetupAutoRelease($link_file);
    
    };

    # If runtime failure occured acquiring the lock
    # cleanup and then die
    my $eval_error = $@;
    if($eval_error){

	# Create an error string
	my $error_string = 
	    "FAILED: Could not acquire link lock because [$eval_error].";

	# Attempt to remove the link file
	if( -e $link_file ){
	    unlink $link_file
		or die "$error_string".
		"\n CHAINED FAILURE: Could not unlink $link_file: $!";
	}
	
	# Cleanup is complete, die
	die $error_string;
    }
   
    # Return the "handle" for this lock
    return $link_fh;
}

#*********************************************************************
# Attempts to acquire a link-lock without blocking.  The link-lock 
# will automatically expire if this program dies or is exec'ed over 
# top of. If successful this function returns a "lock handle" that is 
# required to unlock this link-lock.  If the lock was already held 
# this function returns "undef".
# WARNING: Due to the inconsistency in implementations of NFS this
# may not work on all versions of NFS.
#*********************************************************************
sub AcquireLinkLockNB{
    my ($lock_file) = @_;  #Name the parameters

    # Ensure the file exists
    if( !-e $lock_file ){
	die "FAILED: $lock_file lock file does not exist";
    }

    # Create a unique link file name (using pid and IP)
    my $link_file = &AddDynamicPostFix($lock_file);

    # Create memory for the "lock handle" (file handle for link file) 
    my $link_fh;
    
    # Execute in an eval to catch runtime exceptions and clean up
    eval{
	
	# Attemp to get the lock (non-blocking)
	if( &AttemptLinkLockNB($lock_file, $link_file) ){
	    # If successful
	    # Setup Link Lock File Semantics
	    # Namely that if this program dies, the lock will be released
	    $link_fh = &SetupAutoRelease($link_file);
	}
    
    };

    # If runtime failure occured acquiring the lock
    # cleanup and then die
    my $eval_error = $@;
    if($eval_error){

	# Create an error string
	my $error_string = 
	    "FAILED: Could not acquire link lock because [$eval_error].";

	# Attempt to remove the link file
	if( -e $link_file ){
	    unlink $link_file
		or die "$error_string".
		"\n CHAINED FAILURE: Could not unlink $link_file: $!";
	}
	
	# Cleanup is complete, die
	die $error_string;
    }
   
    # Return the "handle" for this lock
    return $link_fh;
}

#*********************************************************************
# Releases a link lock
# WARNING: Due to the inconsistency in implementations of NFS this
# may not work on all versions of NFS.
#*********************************************************************
sub ReleaseLinkLock{
    my ($fh, $file_name) = @_;   #Name the parameters

    close $fh
	or die "FAILED: Lock may have been prematurely released: $!";

    # Stat the file to ensure the link count is flushed
    my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
	$atime,$mtime,$ctime,$blksize,$blocks)
	= stat($file_name);
    
}

#*********************************************************************
# Test to see if the lock is available.
# Return true if it was available, false otherwise.
# WARNING: Due to the inconsistency in implementations of NFS this
# may not work on all versions of NFS.
#*********************************************************************
sub TestLinkLock{
    my ($lock_file) = @_;   #Name the parameters

    # Attempt to get the lock
    my $lock_handle = &AcquireLinkLockNB($lock_file);
    
    # lock succeeded
    my $success = defined $lock_handle;
    if( $success ){
	# unlock
	&ReleaseLinkLock($lock_handle, $lock_file);
    }

    return $success;
    
}

#*********************************************************************
# Acquires a exclusive or shared lock on the given file.  Blocks until
# the lock is acquired.  Returns a handle to the given lock.
#*********************************************************************
sub AcquireFLock{

    my ($lock_file, $lock_type) = @_;  #Name the parameters
    
    # Ensure the file exists
    if( !-e $lock_file ){
	die "FAILED: $lock_file lock file does not exist";
    }

    # Determine the type of lock required
    my $open_type = '';
    my $flock_mode = 0;
    # exclusive
    if( $lock_type eq $EXCLUSIVE_LOCK){
	$open_type = '>>';
	$flock_mode = LOCK_EX;
    }
    # shared
    elsif( $lock_type eq $SHARED_LOCK){
	$open_type = '<';
	$flock_mode = LOCK_SH;
    }
    # error
    else{
	die "FAILED: Unknown lock type[$lock_type]";
    }

    # Open the lock file (read or write)
    open LOCK_FILE, $open_type, $lock_file
	or die "FAILED: Cannot open $lock_file: $!";

    # Attemp to aqcuire the lock
    flock(LOCK_FILE, $flock_mode)
	or die "FAILED: Cannot acquire [$lock_type] lock on $lock_file: $!";

    # Closing the lock file would have the 
    # consequences of releasing the lock
    return( *LOCK_FILE );
} 

#*********************************************************************
# Acquires a exclusive or shared lock on the given file.  Does not 
# block.  Returns the lock_handle if successful and undef if not.
#*********************************************************************
sub AcquireFLockNB{

    my ($lock_file, $lock_type) = @_;  #Name the parameters
    
    # Ensure the file exists
    if( !-e $lock_file ){
	die "FAILED: $lock_file lock file does not exist";
    }

    # Determine the type of lock required
    my $open_type = '';
    my $flock_mode = 0;
    # exclusive
    if( $lock_type eq $EXCLUSIVE_LOCK){
	$open_type = '>>';
	$flock_mode = LOCK_EX;
    }
    # shared
    elsif( $lock_type eq $SHARED_LOCK){
	$open_type = '<';
	$flock_mode = LOCK_SH;
    }
    # error
    else{
	die "FAILED: Unknown lock type[$lock_type]";
    }

    # Open the lock file (read or write)
    open LOCK_FILE, $open_type, $lock_file
	or die "FAILED: Cannot open $lock_file: $!";

    # Attemp to aqcuire the lock (non-blocking)
    my $locked = flock(LOCK_FILE, $flock_mode | LOCK_NB);

    # If we couldn't get the lock
    if( !$locked ){
	# make sure to close the file
	close LOCK_FILE;
	# return failure
	return undef;
    }

    # Closing the lock file would have the 
    # consequences of releasing the lock
    # Return the lock_handle
    return( *LOCK_FILE );
} 

#*********************************************************************
# Release an flock.  Takes the lock handle
#*********************************************************************
sub ReleaseFLock{
    my ($fh) = @_;  #Name the parameters
    
    # Attemp to release the lock
    flock($fh, LOCK_UN)
	or die "FAILED: Cannot release lock on $lock_file: $!";

    # close the lock file
    close $fh;
} 

#*********************************************************************
# Test to see if the lock is available (shared or not held).
# Return true if it was available, false otherwise.
#*********************************************************************
sub TestFLock{
    my ($lock_file) = @_;  #Name the parameters

    # Try to get the lock in shared mode
    my $lock_handle = &AcquireFLockNB($lock_file, $SHARED_LOCK);

    # If successful
    my $locked = defined $lock_handle;
    if( $locked ){
	# be sure to release the lock.
	&ReleaseFLock($lock_handle);
    }

    return $locked;
} 

#=====================================================================
# Non-Exported Functions
#=====================================================================

#*********************************************************************
# Attempts to acquire a link-lock on the give lock file using the
# given name for the link file, uses optimistic locking.  
# Blocks until the lock is acquired.
#*********************************************************************
sub AttemptLinkLock(){
    
    my ($lock_file, $link_file) = @_;  #Name the parameters

    # Loop until we receive the lock
    my $locked = 0;
    my $attempts = 0;
    while( !$locked ){

	# Backoff based on the number of attempts
	# Calling backoff with attempts == 0 return immediately
	ExponentialBackoff(1, $attempts, $MAX_BACKOFF);

	# Attempt to get the lock (non-blocking)
	$locked = &AttemptLinkLockNB($lock_file, $link_file);
     
	$attempts++;
    }

}

#*********************************************************************
# Attempts to acquire a link-lock on the give lock file using the
# given name for the link file, uses optimistic locking.  
# Does not block.
# Returns true on success and false if the lock is held
#*********************************************************************
sub AttemptLinkLockNB{
    my ($lock_file, $link_file) = @_;  #Name the parameters

    # Attempt to get the lock
    link $lock_file, $link_file
	or die "FAILED: Could not link $link_file to $lock_file: $!";
    
    # Check to see if we got the lock
    my $locked = 0;
    $nlink = LinkCount($lock_file);

    # case1: lock acquired
    if( $nlink == 2 ){
	$locked = 1;
    }
    
    # case2: lock collision
    elsif( $nlink > 2 ){
	# release the lock
	unlink $link_file
	    or die "FAILED: Cannot unlink $link_file: $!";
    }
    
    # case3: catastrophic failure
    elsif( $nlink < 2 ){
	die "FAILED: Catastrophic lock failure:".
	    " link count for $lock_file is $nlink";
    }

    return( $locked );
}

#*********************************************************************
# Sets the link-lock to expire in the event that this program dies
# or is replaced using an 'exec' call.
#*********************************************************************
sub SetupAutoRelease(){
    my ($link_file) = @_;  #Name the parameters

     # open the link file
    open LINK, $link_file
	or die "FAILED: Could not open $link_file: $!";

    # delete the link file
    my $success = unlink $link_file;
    
    # Failure
    if( !$success ){
	# must close the file to prevent implicitly holding the lock
	close LINK;
	die "FAILED: Could not unlink $link_file: $!";
    }

    return( *LINK );
}

#*********************************************************************
# Creates a dynamic name by appending the IP address and Pid
# of this process to the given name.
#*********************************************************************
sub AddDynamicPostFix(){
    my ($name) = @_;  #name the parameters

    # get the IP Address
    my $hostname = hostname();
    my $IpAddr = gethostbyname( $hostname );
    $IpAddr = inet_ntoa $IpAddr;
    # append the ip address and pid to the name 
    my $dynamic_name = $name.'.'.$IpAddr.'-'.$PID;

    return $dynamic_name;
}

#*********************************************************************
# Sleeps for a random number of seconds based on a base time increment
# the current try, and a maximimum time.  Based on ethernet 
# exponential backoff.
#*********************************************************************
sub ExponentialBackoff(){
    my ($base, $current_try, $max_backoff) = @_;  #Name the parameters

    # Sanity check on the parameters
    if( $current_try < 1 ){
	return;
    }

    # Get the maximum increment range
    my $max_range = 2 ** ($current_try - 1);

    # Get a random number between 0 and max range
    my $backoff_mult = int(rand($max_range));

    # Use the random interval to multiply by the base timeout
    my $timeout = $base * $backoff_mult;
    
    # Ensure the timeout does not exceed the max 
    $timeout = $max_backoff if( $timeout > $max_backoff );

    # Sleep for the timeout
    sleep($timeout);
}

#*********************************************************************
# Returns the number of hardlinks to the given file
#*********************************************************************
sub LinkCount(){
    my ($file_name) = @_; #Name the parameters
    
    # check the status of the lock file
    my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
	$atime,$mtime,$ctime,$blksize,$blocks)
	= stat($file_name);

    return $nlink;
}

# loading module was a success
1;
