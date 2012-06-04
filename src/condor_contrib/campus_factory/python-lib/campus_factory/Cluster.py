
import logging
import os

from campus_factory.ClusterStatus import ClusterStatus
from campus_factory.OfflineAds.OfflineAds import OfflineAds
from campus_factory.ClusterStatus import CondorConfig
from campus_factory.util.ExternalCommands import RunExternal
from campus_factory.util.CampusConfig import get_option, get_option_section

class ClusterPreferenceException(Exception):
    def __init__(self, value):
        self.value = value
    def __str__(self):
        return repr(self.value)


class Cluster:
    
    def __init__(self, cluster_unique, useOffline = False):
        self.cluster_unique = cluster_unique
        
        self.status = ClusterStatus(status_constraint="IsUndefined(Offline) && BOSCOCluster =?= \"%s\"" % self.cluster_unique, queue_constraint = "BOSCOCluster =?= \"%s\"" % self.cluster_unique)
        self.useOffline = useOffline
        if useOffline:
            self.offline = OfflineAds()
        
    def ClusterMeetPreferences(self):
        idleslots = self.status.GetIdleGlideins()
        if idleslots == None:
            logging.info("Received None from idle glideins, going to try later")
            raise ClusterPreferenceException("Received None from idle glideins")
        logging.debug("Idle glideins = %i" % idleslots)
        if idleslots >= int(get_option("MAXIDLEGLIDEINS", "5")):
            logging.info("Too many idle glideins")
            raise ClusterPreferenceException("Too many idle glideins")

        # Check for idle glidein jobs
        idlejobs = self.status.GetIdleGlideinJobs()
        if idlejobs == None:
            logging.info("Received None from idle glidein jobs, going to try later")
            raise ClusterPreferenceException("Received None from idle glidein jobs")
        logging.debug("Queued jobs = %i" % idlejobs)
        if idlejobs >= int(get_option("maxqueuedjobs", "5")):
            logging.info("Too many queued jobs")
            raise ClusterPreferenceException("Too many queued jobs")

        return (idleslots, idlejobs)




    def GetIdleJobs(self):
        if not self.useOffline:
            return 0
        
        # Update the offline cluster information
        toSubmit = self.offline.Update( [self.cluster_unique] )
        
        # Get the delinquent sites
        num_submit = self.offline.GetDelinquentSites([self.cluster_unique])
        logging.debug("toSubmit from offline %s", str(toSubmit))
        logging.debug("num_submit = %s\n", str(num_submit))
            
        if (len(toSubmit) > 0) or num_submit[self.cluster_unique]:
            idleuserjobs = max([ num_submit[self.cluster_unique], 5 ])
            logging.debug("Offline ads detected jobs should be submitted.  Idle user jobs set to %i", idleuserjobs)
        else:
            logging.debug("Offline ads did not detect any matches or Delinquencies.")
            idleuserjobs = 0 
        
        return toSubmit
    
    def _GetClusterSpecificConfig(self, option, default):
        if get_option_section(self.cluster_unique, option):
            return  get_option_section(self.cluster_unique, option)
        elif get_option(option):
            return get_option(option)
        else:
            return default
    
    def SubmitGlideins(self, numSubmit):
        """
        Submit numSubmit glideins.
        
        @param numSubmit: The number of glideins to submit.
        """
        # Substitute values in submit file
        file = os.path.join(get_option("GLIDEIN_DIRECTORY"), "job.submit.template")

        # Submit jobs
        for i in range(numSubmit):
            self.SingleSubmit(file)

        # Delete the submit file

    def SingleSubmit(self, file):
        """
        Submit a single glidein job
        
        @param file: The file (string) to submit
        
        """
        
        # Get the cluster specific information
        # First, the cluster tmp directory
        cluster_tmp = self._GetClusterSpecificConfig("worker_tmp", "/tmp")
        remote_factory_location = self._GetClusterSpecificConfig("remote_factory", "~/bosco/campus_factory")
        
        # If we are submtiting to ourselves, then don't need remote cluster
        if get_option("CONDOR_HOST") == self.cluster_unique:
            remote_cluster = ""
        else:
            remote_cluster = self.cluster_unique
        
        # TODO: These options should be moved to a better location
        options = {"WN_TMP": cluster_tmp, \
                   "GLIDEIN_HOST": get_option("COLLECTOR_HOST"), \
                   "GLIDEIN_Site": self.cluster_unique, \
                   "BOSCOCluster": self.cluster_unique, \
                   "REMOTE_FACTORY": remote_factory_location, \
                   "REMOTE_CLUSTER": remote_cluster }
        
        options_str = ""
        for key in options.keys():
            options_str += " -a %s=\"%s\"" % (key, options[key])
            
        (stdout, stderr) = RunExternal("condor_submit %s %s" % (file, options_str))
        logging.debug("stdout: %s" % stdout)
        logging.debug("stderr: %s" % stderr)
    
    
        
