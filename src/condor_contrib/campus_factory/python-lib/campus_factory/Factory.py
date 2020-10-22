
from __future__ import print_function

import ConfigParser
import sys
import time
import logging
import logging.handlers
import os
import signal
import pwd
import shutil

from campus_factory.ClusterStatus import ClusterStatus
from campus_factory.util.ExternalCommands import RunExternal
from campus_factory.OfflineAds.OfflineAds import OfflineAds
from campus_factory.util.DaemonWrangler import DaemonWrangler
from campus_factory.Cluster import *
from campus_factory.util.StreamToLogger import StreamToLogger
from campus_factory.util.CampusConfig import get_option, set_config_file, set_option

BOSCO_CLUSTERLIST = "~/.bosco/.clusterlist"

class Factory:
    """
    The main class of the factory.  Designed to be run inside the condor scheduler.
    
    @author: Derek Weitzel (dweitzel@cse.unl.edu)
    
    """
    def __init__(self, options):
        """
        Initialization function.
        
        @param options: A set of options in the form of an options parser
                Required options: config - location of configuration File
        """
        
        self.options = options

        
    
    def Intialize(self):
        """
        
        Function to initialize the factory's variables such as configuration
        and logging
        """
        # Set the sighup signal handler
        signal.signal(signal.SIGHUP, self.Intialize)
        
        # Read in the configuration file
        self.config_file = self.options.config
        files_read = set_config_file(self.config_file)

        # check if no files read in
        if len(files_read) < 1:
            sys.stderr.write("No configuration files found.  Location = %s\n" % self.config_file)
            sys.exit(1)
            
        self._SetLogging()
        
        if os.getuid() == 0 or get_option("factory_user"):
            logging.info("Detected that factory should change user")
            self._DropPriv()
       
        if  get_option("useoffline", "false").lower() == "true":
            self.UseOffline = True
        else:
            self.UseOffline = False
        
        self.cluster_list = []
        # Get the cluster lists
        if get_option("clusterlist", "") != "":
            logging.debug("Using the cluster list in the campus factory configuration.")
            for cluster_id in get_option("clusterlist").split(','):
                self.cluster_list.append(Cluster(cluster_id, useOffline = self.UseOffline))
        else:
            # Check for the bosco cluster command
            (stdout, stderr) = RunExternal("bosco_cluster -l")
            if len(stdout) != 0 and stdout != "No clusters configured":
                logging.debug("Using the cluster list installed with BOSCO")
                for cluster_id in stdout.split("\n"):
                    if len(cluster_id) > 0 and cluster_id != "":
                        self.cluster_list.append(Cluster(cluster_id, useOffline = self.UseOffline))
            else:
                # Initialize as empty, which infers to submit 'here'
                self.cluster_list = [ Cluster(get_option("CONDOR_HOST"), useOffline = self.UseOffline) ]
        
        # Tar up the executables
        wrangler = DaemonWrangler()
        wrangler.Package()
            

    def _DropPriv(self):
        factory_user = get_option("factory_user")
        current_uid = os.getuid()
        if factory_user is None:
            logging.warning("factory_user is not set in campus factory config file")
            if get_option("CONDOR_IDS"):
                logging.info("CONDOR_IDS is set, will use for dropping privledge")
                (factory_uid, factory_gid) = get_option("CONDOR_IDS").split(".")
                factory_uid = int(factory_uid)
                factory_gid = int(factory_gid)
                factory_user = pwd.getpwuid(factory_uid).pw_name
            elif current_uid == 0:
                logging.error("We are running as root, which can not submit condor jobs.")
                logging.error("Don't know who to drop privledges to.")
                logging.error("I can't do my job!")
                logging.error("Exiting...")
                sys.exit(1)
        else:
            # If factory user is set
            factory_uid = pwd.getpwnam(factory_user).pw_uid
            factory_gid = pwd.getpwnam(factory_user).pw_gid
            logging.debug("Using %i:%i for user:group" % (factory_uid, factory_gid))
        
        # Some parts of bosco need the HOME directory and USER to be defined
        os.environ["HOME"] = pwd.getpwnam(factory_user).pw_dir
        os.environ["USER"] = factory_user
        os.setgid(factory_gid)
        os.setuid(factory_uid)
        
        
    def _SetLogging(self):
        """
        Setting the logging level and set the logging.
        """
        logging_levels = {'debug': logging.DEBUG,
                          'info': logging.INFO,
                          'warning': logging.WARNING,
                          'error': logging.ERROR,
                          'critical': logging.CRITICAL}

        level = logging_levels.get(get_option("loglevel"))
        logdirectory = get_option("logdirectory")
        handler = logging.handlers.RotatingFileHandler(os.path.join(logdirectory, "campus_factory.log"),
                        maxBytes=10000000, backupCount=5)
        root_logger = logging.getLogger()
        root_logger.setLevel(level)
        formatter = logging.Formatter("%(asctime)s - %(levelname)s - %(message)s")
        handler.setFormatter(formatter)
        root_logger.addHandler(handler)
        
        # Send stdout to the log
        stdout_logger = logging.getLogger()
        sl = StreamToLogger(stdout_logger, logging.INFO)
        sys.stdout = sl
 
        stderr_logger = logging.getLogger()
        sl = StreamToLogger(stderr_logger, logging.ERROR)
        sys.stderr = sl
        
        
        
    def Restart(self):
        status = ClusterStatus()
        
        # Get the factory id
        factoryID = status.GetFactoryID()
        
        # Hold then release the factory in the queue
        (stderr, stdout) = RunExternal("condor_hold %s" % factoryID)
        print("Stderr = %s" % stderr.strip())
        #print "Stdout = %s" % stdout.strip()
        
        (stderr, stdout) = RunExternal("condor_release %s" % factoryID)
        print("Stderr = %s" % stderr.strip())
        #print "Stdout = %s" % stdout.strip()
        
    
    
    def Stop(self):
        status = ClusterStatus()
        
        # Get the factory id
        factoryID = status.GetFactoryID()
        
        # Remove the factory job
        (stderr, stdout) = RunExternal("condor_rm %s" % factoryID)
        print("Stderr = %s" % stderr.strip())



    def Start(self):
        """ 
        Start the Factory 
        
        """
        self.Intialize()

        statuses = {}
        status = ClusterStatus(status_constraint="IsUndefined(Offline)")
        offline = OfflineAds()

        # First, daemonize?

        while 1:
            logging.info("Starting iteration...")
            
            # Check if there are any idle jobs
            if not self.UseOffline:
                user_idle = self.GetIdleJobs(ClusterStatus())
                if user_idle is None:
                    logging.info("Received None from idle jobs")
                    self.SleepFactory()
                    continue
                
                idleuserjobs = 0
                for user in user_idle.keys():
                    idleuserjobs += user_idle[user]
                    
                logging.debug("Idle jobs = %i" % idleuserjobs)
                if idleuserjobs < 1:
                    logging.info("No idle jobs")
                    self.SleepFactory()
                    continue


            # For each ssh'd blahp
            for cluster in self.cluster_list:
                idleslots = idlejobs = 0

                if self.UseOffline:
                    idleuserjobs = cluster.GetIdleJobs()
                
                # Check if the cluster is able to submit jobs
                try:
                    (idleslots, idlejobs) = cluster.ClusterMeetPreferences()
                except ClusterPreferenceException as e:
                    logging.debug("Received error from ClusterMeetPreferences")
                    logging.debug(e)
                    idleslots = idlejobs = None
                
                # If the cluster preferences weren't met, then move on
                if idleslots is None or idlejobs is None:
                    continue

                # Get the offline ads to update.
                if self.UseOffline:
                    num_submit = cluster.GetIdleJobs()
                    
    
                
                # Determine how many glideins to submit
                num_submit = self.GetNumSubmit(idleslots, idlejobs, idleuserjobs)
                logging.info("Submitting %i glidein jobs", num_submit)
                cluster.SubmitGlideins(num_submit)

            self.SleepFactory()



    def SleepFactory(self):
        sleeptime = int(get_option("iterationtime"))
        logging.info("Sleeping for %i seconds" % sleeptime)
        time.sleep(sleeptime)
        
        
    def GetNumSubmit(self, idleslots, idlejobs, idleuserjobs):
        """
        Calculate the number of glideins to submit.
        
        @param idleslots: Number of idle startd's
        @param idlejobs: Number of glideins in queue, but not active
        @param idleuserjobs: Number of idle user jobs from FLOCK_FROM
        
        @return: int - Number of glideins to submit
        """
        
        # If we have already submitted enough glideins to fufill the request,
        # don't submit more.
        if max([idlejobs, idleslots]) >= idleuserjobs:
            logging.debug("The number of idlejobs or idleslots fufills the requested idleuserjobs, not submitting any glideins")
            return 0
        
        status = ClusterStatus(status_constraint="IsUndefined(Offline)")
        
        # Check that running glideins are reporting to the collector
        running_glidein_jobs = status.GetRunningGlideinJobs()
        logging.debug("Number of running_glidein_jobs = %i", running_glidein_jobs)
        running_glideins = status.GetRunningGlideins()
        logging.debug("Number of running glideins = %i", running_glideins)
        
        if ((running_glidein_jobs * .9) > running_glideins):
            logging.error("I'm guessing glideins are not reporting to the collector, not submitting")
            return 0
        
        # Ok, so now submit until we can't submit any more, or there are less user jobs
        return min([int(get_option("maxqueuedjobs")) - idlejobs, \
                    idleuserjobs,\
                    int(get_option("MaxIdleGlideins")) - idleslots])
        
           
        
    def GetIdleJobs(self, status):
        """
        Get the number of idle jobs from configured flock from hosts.
        
        @return: { user, int } - Number of idle jobs by user (dictionary)
        """
        # Check for idle jobs to flock from
        if not self.UseOffline:

            schedds = []
            # Get schedd's to query
            if get_option("FLOCK_FROM"):
                schedds = get_option("FLOCK_FROM").strip().split(",")
                            
            logging.debug("Schedds to query: %s" % str(schedds))
            
            
            idleuserjobs = status.GetIdleJobs(schedds)
            if idleuserjobs is None:
                logging.info("Received None from idle user jobs, going to try later")
                return None
            
            # Add all the idle jobs from all the schedds, unique on user (owner)
            user_idle = {}
            for schedd in idleuserjobs.keys():
                for user in idleuserjobs[schedd].keys():
                    if not user_idle.has_key(user):
                        user_idle[user] = 0
                    user_idle[user] += idleuserjobs[schedd][user]

            return user_idle

