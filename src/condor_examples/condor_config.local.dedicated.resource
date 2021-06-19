######################################################################
##
##  condor_config.local.dedicated.resource
##
##  This is the default local configuration file for any resources
##  that are going to be configured as dedicated resources in your
##  Condor pool.  If you are going to use Condor's dedicated MPI
##  scheduling, you must configure some of your machines as dedicated
##  resources, using the settings in this file.
##
##  PLEASE READ the discussion on "Configuring Condor for Dedicated
##  Scheduling" in the "Setting up Condor for Special Environments"
##  section of the Condor Manual for more details.
##
##  You should copy this file to the appropriate location and
##  customize it for your needs.  The file is divided into three main
##  parts: settings you MUST customize, settings regarding the policy
##  of running jobs on your dedicated resources (you must select a
##  policy and uncomment the corresponding expressions), and settings
##  you should leave alone, but that must be present for dedicated
##  scheduling to work.  Settings that are defined here MUST BE
##  DEFINED, since they have no default value.
##
######################################################################


######################################################################
######################################################################
##  Settings you MUST customize!
######################################################################
######################################################################

##  What is the name of the dedicated scheduler for this resource?
##  You MUST fill in the correct full hostname where you're running
##  the dedicated scheduler, and where users will submit their
##  dedicated jobs.  The "DedicateScheduler@" part should not be
##  changed, ONLY the hostname.
DedicatedScheduler = "DedicatedScheduler@full.host.name"


######################################################################
######################################################################
##  Policy Settings (You MUST choose a policy and uncomment it) 
######################################################################
######################################################################

##  There are three basic options for the policy on dedicated
##  resources: 
##  1) Only run dedicated jobs
##  2) Always run jobs, but prefer dedicated ones
##  3) Always run dedicated jobs, but only allow non-dedicated jobs to
##     run on an opportunistic basis.   
##  You MUST uncomment the set of policy expressions you want to use
##  at your site.

##--------------------------------------------------------------------
## 1) Only run dedicated jobs
##--------------------------------------------------------------------
#START		= Scheduler =?= $(DedicatedScheduler)
#SUSPEND	= False
#CONTINUE	= True
#PREEMPT	= False
#KILL		= False
#WANT_SUSPEND	= False
#WANT_VACATE	= False
#RANK		= Scheduler =?= $(DedicatedScheduler)

##--------------------------------------------------------------------
## 2) Always run jobs, but prefer dedicated ones
##--------------------------------------------------------------------
#START		= True
#SUSPEND	= False
#CONTINUE	= True
#PREEMPT	= False
#KILL		= False
#WANT_SUSPEND	= False
#WANT_VACATE	= False
#RANK		= Scheduler =?= $(DedicatedScheduler)

##--------------------------------------------------------------------
## 3) Always run dedicated jobs, but only allow non-dedicated jobs to
##    run on an opportunistic basis.   
##--------------------------------------------------------------------
##  Allowing both dedicated and opportunistic jobs on your resources
##  requires that you have an opportunistic policy already defined.
##  These are the only settings that need to be modified from your
##  existing policy expressions to allow dedicated jobs to always run
##  without suspending, or ever being preempted (either from activity
##  on the machine, or other jobs in the system).

#SUSPEND	= Scheduler =!= $(DedicatedScheduler) && ($(SUSPEND))
#PREEMPT	= Scheduler =!= $(DedicatedScheduler) && ($(PREEMPT))
#RANK_FACTOR	= 1000000
#RANK	= (Scheduler =?= $(DedicatedScheduler) * $(RANK_FACTOR)) + $(RANK)
#START	= (Scheduler =?= $(DedicatedScheduler)) || ($(START))

##  Note: For everything to work, you MUST set RANK_FACTOR to be a
##  larger value than the maximum value your existing rank expression
##  could possibly evaluate to.  RANK is just a floating point value,
##  so there's no harm in having a value that's very large.


######################################################################
######################################################################
##  Settings you should leave alone, but that must be defined
######################################################################
######################################################################

##  Path to the special version of rsh that's required to spawn MPI
##  jobs under Condor.  WARNING: This is not a replacement for rsh,
##  and does NOT work for interactive use.  Do not use it directly!
MPI_CONDOR_RSH_PATH = $(LIBEXEC)

##  Path to OpenSSH server binary
##  Condor uses this to establish a private SSH connection between execute
##  machines. It is usually in /usr/sbin, but may be in /usr/local/sbin
CONDOR_SSHD = /usr/sbin/sshd

##  Path to OpenSSH keypair generator.
##  Condor uses this to establish a private SSH connection between execute
##  machines. It is usually in /usr/bin, but may be in /usr/local/bin
CONDOR_SSH_KEYGEN = /usr/bin/ssh-keygen

##  This setting puts the DedicatedScheduler attribute, defined above,
##  into your machine's classad.  This way, the dedicated scheduler
##  (and you) can identify which machines are configured as dedicated
##  resources.
##  Note: as of 8.4.1 this setting is automatic
#STARTD_ATTRS = $(STARTD_ATTRS), DedicatedScheduler
