use Cwd 'abs_path';
use File::Basename;
use lib dirname( abs_path $0 );
use CondorTest;
use CondorPersonal;
use Check::SimpleJob;

use strict;
use warnings;

my $conf = '
RELEASE_DIR		= /usr/local/condor
LOCAL_DIR		= $(TILDE)
LOCAL_CONFIG_DIR	= $(LOCAL_DIR)/config
CONDOR_ADMIN		= root@$(FULL_HOSTNAME)
MAIL			= /bin/mail
UID_DOMAIN		= $(FULL_HOSTNAME)
FILESYSTEM_DOMAIN	= $(FULL_HOSTNAME)
COLLECTOR_NAME 		= My Pool - $(CONDOR_HOST)
FLOCK_FROM = 
FLOCK_TO = 
FLOCK_NEGOTIATOR_HOSTS = $(FLOCK_TO)
FLOCK_COLLECTOR_HOSTS = $(FLOCK_TO)
ALLOW_ADMINISTRATOR = $(CONDOR_HOST), $(IP_ADDRESS)
ALLOW_OWNER = $(FULL_HOSTNAME), $(ALLOW_ADMINISTRATOR)
ALLOW_READ = * 
ALLOW_WRITE = $(FULL_HOSTNAME), $(IP_ADDRESS)
ALLOW_NEGOTIATOR = $(CONDOR_HOST), $(IP_ADDRESS)
ALLOW_NEGOTIATOR_SCHEDD = $(CONDOR_HOST), $(FLOCK_NEGOTIATOR_HOSTS), $(IP_ADDRESS)
ALLOW_WRITE_COLLECTOR = $(ALLOW_WRITE), $(FLOCK_FROM)
ALLOW_WRITE_STARTD    = $(ALLOW_WRITE), $(FLOCK_FROM)
ALLOW_READ_COLLECTOR  = $(ALLOW_READ), $(FLOCK_FROM)
ALLOW_READ_STARTD     = $(ALLOW_READ), $(FLOCK_FROM)
LOCK		= $(LOG)
ALL_DEBUG               =
MAX_COLLECTOR_LOG	= 1000000
COLLECTOR_DEBUG		=
MAX_KBDD_LOG		= 1000000
KBDD_DEBUG		=
MAX_NEGOTIATOR_LOG	= 1000000
NEGOTIATOR_DEBUG	= D_MATCH
MAX_NEGOTIATOR_MATCH_LOG = 1000000
MAX_SCHEDD_LOG		= 1000000
SCHEDD_DEBUG		= D_PID
MAX_SHADOW_LOG		= 1000000
SHADOW_DEBUG		=
MAX_STARTD_LOG		= 1000000
STARTD_DEBUG		= 
MAX_STARTER_LOG		= 1000000
MAX_MASTER_LOG		= 1000000
MASTER_DEBUG		= 
MAX_JOB_ROUTER_LOG      = 1000000
JOB_ROUTER_DEBUG        =
MAX_ROOSTER_LOG         = 1000000
ROOSTER_DEBUG           =
MAX_SHARED_PORT_LOG     = 1000000
SHARED_PORT_DEBUG       =
MAX_HDFS_LOG            = 1000000
HDFS_DEBUG              =
MAX_HAD_LOG		= 1000000
HAD_DEBUG		=
MAX_REPLICATION_LOG	= 1000000
REPLICATION_DEBUG	=
MAX_TRANSFERER_LOG	= 1000000
TRANSFERER_DEBUG	=
MINUTE		= 60
HOUR		= (60 * $(MINUTE))
StateTimer	= (time() - EnteredCurrentState)
ActivityTimer	= (time() - EnteredCurrentActivity)
ActivationTimer = ifThenElse(JobStart =!= UNDEFINED, (time() - JobStart), 0)
LastCkpt	= (time() - LastPeriodicCheckpoint)
STANDARD	= 1
VANILLA		= 5
MPI		= 8
VM		= 13
IsMPI           = (TARGET.JobUniverse == $(MPI))
IsVanilla       = (TARGET.JobUniverse == $(VANILLA))
IsStandard      = (TARGET.JobUniverse == $(STANDARD))
IsVM            = (TARGET.JobUniverse == $(VM))
NonCondorLoadAvg	= (LoadAvg - CondorLoadAvg)
BackgroundLoad		= 0.3
HighLoad		= 0.5
StartIdleTime		= 15 * $(MINUTE)
ContinueIdleTime	=  5 * $(MINUTE)
MaxSuspendTime		= 10 * $(MINUTE)
MaxVacateTime		= 10 * $(MINUTE)
KeyboardBusy		= (KeyboardIdle < $(MINUTE))
ConsoleBusy		= (ConsoleIdle  < $(MINUTE))
CPUIdle			= ($(NonCondorLoadAvg) <= $(BackgroundLoad))
CPUBusy			= ($(NonCondorLoadAvg) >= $(HighLoad))
KeyboardNotBusy		= ($(KeyboardBusy) == False)
BigJob		= (TARGET.ImageSize >= (50 * 1024))
MediumJob	= (TARGET.ImageSize >= (15 * 1024) && TARGET.ImageSize < (50 * 1024))
SmallJob	= (TARGET.ImageSize <  (15 * 1024))
JustCPU			= ($(CPUBusy) && ($(KeyboardBusy) == False))
MachineBusy		= ($(CPUBusy) || $(KeyboardBusy))
WANT_SUSPEND 		= $(UWCS_WANT_SUSPEND)
WANT_VACATE		= $(UWCS_WANT_VACATE)
START			= $(UWCS_START)
SUSPEND			= $(UWCS_SUSPEND)
CONTINUE		= $(UWCS_CONTINUE)
PREEMPT			= $(UWCS_PREEMPT)
KILL			= $(UWCS_KILL)
PERIODIC_CHECKPOINT	= $(UWCS_PERIODIC_CHECKPOINT)
PREEMPTION_REQUIREMENTS	= $(UWCS_PREEMPTION_REQUIREMENTS)
PREEMPTION_RANK		= $(UWCS_PREEMPTION_RANK)
NEGOTIATOR_PRE_JOB_RANK = $(UWCS_NEGOTIATOR_PRE_JOB_RANK)
NEGOTIATOR_POST_JOB_RANK = $(UWCS_NEGOTIATOR_POST_JOB_RANK)
MaxJobRetirementTime    = $(UWCS_MaxJobRetirementTime)
CLAIM_WORKLIFE          = $(UWCS_CLAIM_WORKLIFE)
UWCS_WANT_SUSPEND  = ( $(SmallJob) || $(KeyboardNotBusy) || $(IsVanilla) ) && \
                     ( $(SUSPEND) )
UWCS_WANT_VACATE   = ( $(ActivationTimer) > 10 * $(MINUTE) || $(IsVanilla) )

UWCS_START	= ( (KeyboardIdle > $(StartIdleTime)) \
                    && ( $(CPUIdle) || \
                         (State != "Unclaimed" && State != "Owner")) )
UWCS_SUSPEND = ( $(KeyboardBusy) || \
                 ( (CpuBusyTime > 2 * $(MINUTE)) \
                   && $(ActivationTimer) > 90 ) )
UWCS_CONTINUE = ( $(CPUIdle) && ($(ActivityTimer) > 10) \
                  && (KeyboardIdle > $(ContinueIdleTime)) )

UWCS_PREEMPT = ( ((Activity == \"Suspended\") && \
                  ($(ActivityTimer) > $(MaxSuspendTime))) \
		 || (SUSPEND && (WANT_SUSPEND == False)) )
UWCS_MaxJobRetirementTime = 0
MachineMaxVacateTime = $(MaxVacateTime) 
UWCS_KILL = false
UWCS_PERIODIC_CHECKPOINT	= $(LastCkpt) > (3 * $(HOUR) + \
                                  $RANDOM_INTEGER(-30,30,1) * $(MINUTE) )
UWCS_NEGOTIATOR_PRE_JOB_RANK = RemoteOwner =?= UNDEFINED
UWCS_NEGOTIATOR_POST_JOB_RANK = \
 (RemoteOwner =?= UNDEFINED) * (KFlops - SlotID - 1.0e10*(Offline=?=True))
UWCS_PREEMPTION_REQUIREMENTS = ((SubmitterGroup =?= RemoteGroup) \
        && ($(StateTimer) > (1 * $(HOUR))) \
	&& (RemoteUserPrio > TARGET.SubmitterUserPrio * 1.2)) \
        || (MY.NiceUser == True)
UWCS_PREEMPTION_RANK = (RemoteUserPrio * 1000000) - TARGET.ImageSize
TESTINGMODE_WANT_SUSPEND	= False
TESTINGMODE_WANT_VACATE		= False
TESTINGMODE_START			= True
TESTINGMODE_SUSPEND			= False
TESTINGMODE_CONTINUE		= True
TESTINGMODE_PREEMPT			= False
TESTINGMODE_KILL			= False
TESTINGMODE_PERIODIC_CHECKPOINT	= False
TESTINGMODE_PREEMPTION_REQUIREMENTS = False
TESTINGMODE_PREEMPTION_RANK = 0
TESTINGMODE_CLAIM_WORKLIFE = 1200
LOG		= $(LOCAL_DIR)/log
SPOOL		= $(LOCAL_DIR)/spool
EXECUTE		= $(LOCAL_DIR)/execute
BIN		= $(RELEASE_DIR)/bin
LIB		= $(RELEASE_DIR)/lib
INCLUDE		= $(RELEASE_DIR)/include
SBIN		= $(RELEASE_DIR)/sbin
LIBEXEC		= $(RELEASE_DIR)/libexec
HISTORY		= $(SPOOL)/history
COLLECTOR_LOG	= $(LOG)/CollectorLog
KBDD_LOG	= $(LOG)/KbdLog
MASTER_LOG	= $(LOG)/MasterLog
NEGOTIATOR_LOG	= $(LOG)/NegotiatorLog
NEGOTIATOR_MATCH_LOG = $(LOG)/MatchLog
SCHEDD_LOG	= $(LOG)/SchedLog
SHADOW_LOG	= $(LOG)/ShadowLog
STARTD_LOG	= $(LOG)/StartLog
STARTER_LOG	= $(LOG)/StarterLog
JOB_ROUTER_LOG  = $(LOG)/JobRouterLog
ROOSTER_LOG     = $(LOG)/RoosterLog
SHARED_PORT_LOG = $(LOG)/SharedPortLog
HAD_LOG		= $(LOG)/HADLog
REPLICATION_LOG	= $(LOG)/ReplicationLog
TRANSFERER_LOG	= $(LOG)/TransfererLog
HDFS_LOG	= $(LOG)/HDFSLog
SHADOW_LOCK	= $(LOCK)/ShadowLock
COLLECTOR_HOST  = $(CONDOR_HOST)
RESERVED_DISK		= 5
DAEMON_LIST			= MASTER, STARTD, SCHEDD
MASTER				= $(SBIN)/condor_master
STARTD				= $(SBIN)/condor_startd
SCHEDD				= $(SBIN)/condor_schedd
KBDD				= $(SBIN)/condor_kbdd
NEGOTIATOR			= $(SBIN)/condor_negotiator
COLLECTOR			= $(SBIN)/condor_collector
CKPT_SERVER			= $(SBIN)/condor_ckpt_server
STARTER_LOCAL			= $(SBIN)/condor_starter
JOB_ROUTER                      = $(LIBEXEC)/condor_job_router
ROOSTER                         = $(LIBEXEC)/condor_rooster
HDFS				= $(SBIN)/condor_hdfs
SHARED_PORT			= $(LIBEXEC)/condor_shared_port
TRANSFERER			= $(LIBEXEC)/condor_transferer
DEFRAG                          = $(LIBEXEC)/condor_defrag
GANGLIAD                        = $(LIBEXEC)/condor_gangliad
MASTER_ADDRESS_FILE = $(LOG)/.master_address
PREEN				= $(SBIN)/condor_preen
PREEN_ARGS			= -m -r
COLLECTOR_ADDRESS_FILE = $(LOG)/.collector_address
STARTER_LIST = STARTER, STARTER_STANDARD
STARTER			= $(SBIN)/condor_starter
STARTER_STANDARD	= $(SBIN)/condor_starter.std
STARTER_LOCAL		= $(SBIN)/condor_starter
STARTD_ADDRESS_FILE	= $(LOG)/.startd_address
BenchmarkTimer = (time() - LastBenchmark)
RunBenchmarks : (LastBenchmark == 0 ) || ($(BenchmarkTimer) >= (4 * $(HOUR)))
benchmarks_joblist = mips kflops
benchmarks_max_job_load = 1.0
benchmarks_mips_executable = $(LIBEXEC)/condor_mips
benchmarks_mips_job_load = 1.0
benchmarks_kflops_executable = $(LIBEXEC)/condor_kflops
benchmarks_kflops_job_load = 1.0
COLLECTOR_HOST_STRING = "$(COLLECTOR_HOST)"
STARTD_ATTRS = COLLECTOR_HOST_STRING
STARTD_JOB_EXPRS = ImageSize, ExecutableSize, JobUniverse, NiceUser
CKPT_PROBE = $(LIBEXEC)/condor_ckpt_probe
SHADOW_LIST = SHADOW, SHADOW_STANDARD
SHADOW			= $(SBIN)/condor_shadow
SHADOW_STANDARD		= $(SBIN)/condor_shadow.std
SCHEDD_ADDRESS_FILE	= $(SPOOL)/.schedd_address
SCHEDD_DAEMON_AD_FILE = $(SPOOL)/.schedd_classad
QUEUE_SUPER_USERS	= root, condor
WINDOWS_RMDIR = $(SBIN)\condor_rmdir.exe
PROCD = $(SBIN)/condor_procd
PROCD_ADDRESS = $(LOCK)/procd_pipe
PROCD_LOG = $(LOG)/ProcLog
PROCD_MAX_SNAPSHOT_INTERVAL = 60
WINDOWS_SOFTKILL = $(SBIN)/condor_softkill
VALID_SPOOL_FILES	= job_queue.log, job_queue.log.tmp, history, \
                          Accountant.log, Accountantnew.log, \
                          local_univ_execute, .quillwritepassword, \
						  .pgpass, \
			  .schedd_address, .schedd_classad
INVALID_LOG_FILES	= core
JAVA = /usr/bin/java
JAVA_CLASSPATH_DEFAULT = $(LIB) $(LIB)/scimark2lib.jar .
JAVA_CLASSPATH_ARGUMENT = -classpath
JAVA_CLASSPATH_SEPARATOR = :
JAVA_BENCHMARK_TIME = 2
JAVA_EXTRA_ARGUMENTS =
GRIDMANAGER			= $(SBIN)/condor_gridmanager
GT2_GAHP			= $(SBIN)/gahp_server
GRID_MONITOR			= $(SBIN)/grid_monitor
MAX_GRIDMANAGER_LOG	= 1000000
GRIDMANAGER_DEBUG	= 
GRIDMANAGER_LOG = $(LOG)/GridmanagerLog.$(USERNAME)
GRIDMANAGER_LOCK = $(LOCK)/GridmanagerLock.$(USERNAME)
GRIDMANAGER_MAX_JOBMANAGERS_PER_RESOURCE = 10
CRED_MIN_TIME_LEFT		= 120 
CONDOR_GAHP = $(SBIN)/condor_c-gahp
CONDOR_GAHP_WORKER = $(SBIN)/condor_c-gahp_worker_thread
MAX_C_GAHP_LOG	= 1000000
C_GAHP_LOG = /tmp/CGAHPLog.$(USERNAME)
C_GAHP_LOCK = /tmp/CGAHPLock.$(USERNAME)
C_GAHP_WORKER_THREAD_LOG = /tmp/CGAHPWorkerLog.$(USERNAME)
C_GAHP_WORKER_THREAD_LOCK = /tmp/CGAHPWorkerLock.$(USERNAME)
GLITE_LOCATION = $(LIBEXEC)/glite
BATCH_GAHP = $(GLITE_LOCATION)/bin/batch_gahp
UNICORE_GAHP = $(SBIN)/unicore_gahp
NORDUGRID_GAHP = $(SBIN)/nordugrid_gahp
CREAM_GAHP = $(SBIN)/cream_gahp
DELTACLOUD_GAHP = $(SBIN)/deltacloud_gahp
EC2_GAHP = $(SBIN)/ec2_gahp
EC2_GAHP_LOG = /tmp/EC2GahpLog.$(USERNAME)
GRIDMANAGER_MAX_SUBMITTED_JOBS_PER_RESOURCE_EC2 = 20
CREDD				= $(SBIN)/condor_credd
CREDD_ADDRESS_FILE	= $(LOG)/.credd_address
CREDD_PORT			= 9620
CREDD_ARGS			= -p $(CREDD_PORT) -f
CREDD_LOG			= $(LOG)/CredLog
CREDD_DEBUG			= D_FULLDEBUG
MAX_CREDD_LOG		= 4000000
CRED_STORE_DIR = $(LOCAL_DIR)/cred_dir
QUILL = $(SBIN)/condor_quill
QUILL_LOG = $(LOG)/QuillLog
QUILL_ADDRESS_FILE = $(LOG)/.quill_address
DBMSD = $(SBIN)/condor_dbmsd
DBMSD_ARGS = -f
DBMSD_LOG = $(LOG)/DbmsdLog
VM_GAHP_SERVER = $(SBIN)/condor_vm-gahp
VM_GAHP_LOG	= $(LOG)/VMGahpLog
MAX_VM_GAHP_LOG	= 1000000
VMWARE_PERL = perl
VMWARE_SCRIPT = $(SBIN)/condor_vm_vmware
VMWARE_NETWORKING_TYPE = nat
LIBVIRT_XML_SCRIPT = $(LIBEXEC)/libvirt_simple_script.awk
LeaseManager			= $(SBIN)/condor_lease_manager
LeaseManger_ADDRESS_FILE	= $(LOG)/.lease_manager_address
LeaseManager_LOG		= $(LOG)/LeaseManagerLog
LeaseManager_DEBUG		= D_FULLDEBUG
MAX_LeaseManager_LOG		= 1000000
LeaseManager.GETADS_INTERVAL	= 60
LeaseManager.UPDATE_INTERVAL	= 300
LeaseManager.PRUNE_INTERVAL	= 60
LeaseManager.DEBUG_ADS		= False
LeaseManager.CLASSAD_LOG	= $(SPOOL)/LeaseManagerState
KBDD_ADDRESS_FILE = $(LOG)/.kbdd_address
HDFS_NAMENODE = hdfs://example.com:9000
HDFS_NAMENODE_WEB = example.com:8000
HDFS_BACKUPNODE = hdfs://example.com:50100
HDFS_BACKUPNODE_WEB = example.com:50105
HDFS_NODETYPE = HDFS_DATANODE
HDFS_NAMENODE_ROLE = ACTIVE
HDFS_NAMENODE_DIR = /tmp/hadoop_name
HDFS_DATANODE_DIR = /scratch/tmp/hadoop_data
FILETRANSFER_PLUGINS = $(LIBEXEC)/curl_plugin, $(LIBEXEC)/data_plugin
GANGLIAD_METRICS_CONFIG_DIR = /scratch/brandl/master/buildmaster/release_dir/etc/condor/ganglia.d';



my $fh = File::Temp->new();
my $fname = $fh->filename;
$fh->print("URLTEST = 555");

my $forkpid = fork();
if($forkpid == 0)
{
  system("python miniserver.py &");
} else 
{
  sleep(1);
  local $/=undef;
  open FILE, "pythonurl";
  my $url = <FILE>;
  close FILE;

  my @convaldump = ();
  runCondorTool("condor_config_val -config", \@foobar, 2, {emit_output => 1});
  my @lines = split /\n/, $str;
  my $configpath = $lines[2]

  CondorTest::StartCondorWithParams
  (
    #condorlocalsrc=> ($conf . "\n LOCAL_CONFIG_FILE = condor_urlfetch _P(SUBSYSTEM) " . $url . " condor_config_file_cache"),
    do_not_start => 1,
    fresh_local => "true"
  ); 

  
  use File::Compare;
  if(compare($fh, condor_config_file_cache) != 0)
  {
    print "Error in downloading the config file";
    exit("Error in downloading the config file");
  }
#condor_scripts condortests.pm    copy it to condor_tests
}


exit();
