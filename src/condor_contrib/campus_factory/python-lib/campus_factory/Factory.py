
import ConfigParser
import sys
import time
import logging
import logging.handlers
import os
import signal

from campus_factory.ClusterStatus import ClusterStatus
from campus_factory.ClusterStatus import CondorConfig
from campus_factory.util.ExternalCommands import RunExternal
from campus_factory.OfflineAds.OfflineAds import OfflineAds
from campus_factory.util.DaemonWrangler import DaemonWrangler
from campus_factory.Cluster import *

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
        self.config = ConfigParser.ConfigParser()
        files_read = self.config.read([self.config_file])

        # check if no files read in
        if len(files_read) < 1:
            sys.stderr.write("No configuration files found.  Location = %s\n" % self.config_file)
            sys.exit(1)
            
        self._SetLogging()
       
        if  self.config.has_option("general", "useoffline") and (self.config.get("general", "useoffline").lower() == "true"):
            self.UseOffline = True
        else:
            self.UseOffline = False
        
        try:
            self.condor_config = CondorConfig()
        except EnvironmentError, inst:
            logging.exception(str(inst))
            raise inst
        
        self.cluster_list = []
        # Get the cluster lists
        if self.config.has_option("general", "clusterlist"):
            logging.debug("Using the cluster list in the campus factory configuration.")
            for cluster_id in self.config.get("general", "clusterlist").split(','):
                self.cluster_list.append(Cluster(cluster_id, self.config, useOffline = self.UseOffline))
        else:
            # Check for the bosco cluster command
            (stdout, stderr) = RunExternal("bosco_cluster -l")
            if len(stdout) != 0:
                logging.debug("Using the cluster list installed with BOSCO")
                for cluster_id in stdout.split("\n"):
                    if len(cluster_id) > 0 and cluster_id != "":
                        self.cluster_list.append(Cluster(cluster_id, self.config, useOffline = self.UseOffline))
            else:
                # Initialize as emtpy, which infers to submit 'here'
                self.cluster_list = [ self.condor_config.get("CONDOR_HOST") ]
        
        # Tar up the executables
        wrangler = DaemonWrangler(self.config)
        wrangler.Package()
            
        
    def _SetLogging(self):
        """
        Setting the logging level and set the logging.
        """
        logging_levels = {'debug': logging.DEBUG,
                          'info': logging.INFO,
                          'warning': logging.WARNING,
                          'error': logging.ERROR,
                          'critical': logging.CRITICAL}

        level = logging_levels.get(self.config.get("general", "loglevel"))
        logdirectory = self.config.get("general", "logdirectory")
        handler = logging.handlers.RotatingFileHandler(os.path.join(logdirectory, "campus_factory.log"),
                        maxBytes=10000000, backupCount=5)
        root_logger = logging.getLogger()
        root_logger.setLevel(level)
        formatter = logging.Formatter("%(asctime)s - %(levelname)s - %(message)s")
        handler.setFormatter(formatter)
        root_logger.addHandler(handler)
        
        
    def Restart(self):
        status = ClusterStatus()
        
        # Get the factory id
        factoryID = status.GetFactoryID()
        
        # Hold then release the factory in the queue
        (stderr, stdout) = RunExternal("condor_hold %s" % factoryID)
        print "Stderr = %s" % stderr.strip()
        #print "Stdout = %s" % stdout.strip()
        
        (stderr, stdout) = RunExternal("condor_release %s" % factoryID)
        print "Stderr = %s" % stderr.strip()
        #print "Stdout = %s" % stdout.strip()
        
    
    
    def Stop(self):
        status = ClusterStatus()
        
        # Get the factory id
        factoryID = status.GetFactoryID()
        
        # Remove the factory job
        (stderr, stdout) = RunExternal("condor_rm %s" % factoryID)
        print "Stderr = %s" % stderr.strip()



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
                if user_idle == None:
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
                except ClusterPreferenceException, e:
                    logging.debug("Received error from ClusterMeetPreferences")
                    logging.debug(e)
                    idleslots = idlejobs = None
                
                # If the cluster preferences weren't met, then move on
                if idleslots == None or idlejobs == None:
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
        sleeptime = int(self.config.get("general", "iterationtime"))
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
        return min([int(self.config.get("general", "maxqueuedjobs")) - idlejobs, \
                    idleuserjobs,\
                    int(self.config.get("general", "MaxIdleGlideins")) - idleslots])
        
           
        
    def GetIdleJobs(self, status):
        """
        Get the number of idle jobs from configured flock from hosts.
        
        @return: { user, int } - Number of idle jobs by user (dictionary)
        """
        # Check for idle jobs to flock from
        if not self.UseOffline:

            schedds = []
            # Get schedd's to query
            if self.config.has_option("general", "FLOCK_FROM"):
                schedds = self.config.get("general", "FLOCK_FROM").split(",")
                logging.debug("Using FLOCK_FROM from the factory config.")
            else:
                schedds_config = self.condor_config.get('FLOCK_FROM')
                if len(schedds_config) >= 0 and schedds_config != '':
                    schedds = schedds_config.strip().split(",")
                    logging.debug("Using FLOCK_FROM from the condor configuration")
            
            schedds.append(self.condor_config.get("CONDOR_HOST"))
                            
            logging.debug("Schedds to query: %s" % str(schedds))
            
            
            idleuserjobs = status.GetIdleJobs(schedds)
            if idleuserjobs == None:
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

