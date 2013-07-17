
import re
import logging
import time

from campus_factory.Parsers import AvailableGlideins
from campus_factory.Parsers import IdleGlideins
from campus_factory.Parsers import IdleJobs
from campus_factory.Parsers import FactoryID
from campus_factory.Parsers import RunningGlideinsJobs
from campus_factory.Parsers import RunningGlideins
from campus_factory.Parsers import IdleLocalJobs
from campus_factory.util.ExternalCommands import RunExternal

from GlideinWMS.condorMonitor import CondorQ, CondorStatus

class Singleton(type):
    def __init__(cls, name, bases, dict):
        super(Singleton, cls).__init__(name, bases, dict)
        cls.instance = None
 
    def __call__(cls, *args, **kw):
        if cls.instance is None:
            cls.instance = super(Singleton, cls).__call__(*args, **kw)
 
        return cls.instance


class CondorConfig:
    """
    Singleton of the condor config (as reported by condor_config_val -dump)
    """
    __metaclass__ = Singleton
    
    def __init__(self):
        self.config_dict = {}

        (stdout, stderr) = RunExternal("condor_config_val -dump")
        if len(stdout) == 0:
            error_msg = "Unable to get any output from condor_config_val.  Is condor binaries in the path?"
            raise EnvironmentError(error_msg)
        
        # Parse the stdout
        line_re = re.compile("([\w|\d]+)\s+=\s+(.*)\Z")
        config_dict = {}
        for line in stdout.split('\n'):
            match = line_re.search(line)
            if match == None:
                continue
            (key, value) = match.groups()
            config_dict[key] = value
        
        logging.info("Got %s values from condor_config_val" % len(config_dict.keys()))
        #self.config_dict = config_dict
        
    def get(self, key):
        """
        @param key: string key
        @return: string - value corresponding to key or "" if key is non-valid
                        
        """
        if self.config_dict.has_key(key):
            return self.config_dict[key]
        else:
            (stdout, stderr) = RunExternal("condor_config_val %s" % key)
            if len(stdout) == 0:
                logging.warning("Unable to get any output from condor_config_val.  Is key = %s correct?" % key)
                logging.warning("Stderr: %s" % stderr)

            self.config_dict[key] = stdout.strip()
            return stdout.strip()

        #if self.config_dict.has_key(key):
        #    return self.config_dict[key]
        #else:
        #    return ""


class ClusterStatus:
    """
    Gather statistics on the cluster status
    
    
    """

    def __init__(self, status_constraint=None, queue_constraint=None):
        """
        Initialize the ClusterStatus
        
        @param constraint: Constraint (str) to apply to all queries for jobs and glideins
        
        """
        # Constraint
        self.status_constraint = status_constraint
        self.queue_constraint = queue_constraint
        
        # Timer to update cluster status
        self.q_refresh_timer = 0
        self.status_refresh_timer = 0
        
        
        # Timer to refresh cached information
        self.refresh_interval = 30
        
        pass

    
    def GetCondorQ(self):
        """
        Returns the current queue.  Refreshes data if necessary
        
        @return: [(ClusterID, ProcID): [classad,...]]
        
        """
        
        # Refresh the condor_q, if necessary
        if self.q_refresh_timer < int(time.time()):
            condorq = CondorQ()
            try:
                self.condor_q = condorq.fetch(constraint=self.queue_constraint)
                self.q_refresh_timer = int(time.time()) + self.refresh_interval
            except:
                # There was an error getting information from condor_q
                self.condor_q = {}
        
        # Return the queue
        return self.condor_q
    
    def GetCondorStatus(self, constraint=None):
        """
        Returns the current condor_status.  Refreshes data if necessary
        
        @return: [(ClusterID, ProcID): [classad,...]]
        
        """
        if constraint == None:
            constraint = self.status_constraint
        
        # Refresh the condor_status, if necessary
        if self.status_refresh_timer < int(time.time()):
            condorstatus = CondorStatus()
            try:
                self.condor_status = condorstatus.fetch(constraint=constraint)
                self.status_refresh_timer = int(time.time()) + self.refresh_interval
            except:
                # There was an error in getting the information from condor_status
                self.condor_status = {}
        
        # Return the queue
        return self.condor_status
    
    def CountDict(self, in_dict, **kwargs):
        """
        Returns the number (int) of entries in the dictionary in_dict with **kwargs true
        
        @return: int - Number of dictionary items with kwargs true
        """
        count = 0
        
        for key in in_dict.keys():
            found = True
            for const_key in kwargs:
                
                if const_key not in in_dict[key].keys():
                    found = False
                    break
                
                if not in_dict[key][const_key] == kwargs[const_key]:
                    found = False
                    break
            if found == True:
                count += 1
        
        return count

    def GetIdleGlideins(self):
        """ 
        Returns the number of glideins idle in the cluster (condor_status -avail) 
        
        @return: int - Number of idle glidein slots.
        """
        
        return self.CountDict(self.GetCondorStatus(), IS_GLIDEIN = True, State = "Unclaimed")


    def GetIdleGlideinJobs(self):
        """ 
        Returns the number of glidein jobs that are idle (condor_q) 
        
        @return: int - Number of glidein jobs submitted but still idle.
        """
        return self.CountDict(self.GetCondorQ(), GlideinJob = True, JobStatus = 1)


    def GetIdleJobs(self, schedds):
        """ 
        Returns the number of idle user jobs (condor_q) 
        
        @param schedds: List [] of schedd names to check for idle jobs.
        @return: dictionary with {schedd_name: {owner: idle, owner: idle...}...}
        """
        sumidlejobs = 0
        schedd_owner_idle = {}
        for schedd in schedds:
            idlejobs = IdleJobs(schedd)
            schedd_idlejobs = idlejobs.GetIdle()
            schedd_owner_idle[schedd] = idlejobs.GetOwnerIdle()
        
        # Also, always query the local schedd
        idle_local_jobs = IdleLocalJobs()
        idle_local_jobs.GetIdle()
        schedd_owner_idle[""] = idle_local_jobs.GetOwnerIdle()

        return schedd_owner_idle


    def GetFactoryID(self):
        """
        Returns the condor ClusterId for the factory.
        
        @return: str - ClusterId of the factory
        """
        factoryID = FactoryID()
        return factoryID.GetId()
    
    def GetRunningGlideinJobs(self):
        """
        @return: int - Number of running glidein jobs
        """
        return self.CountDict(self.GetCondorQ(), GlideinJob = True, JobStatus = 2)

        
    def GetRunningGlideins(self):
        """
        @return: int - Number of running glidein startds
        """
        return self.CountDict(self.GetCondorStatus(), IS_GLIDEIN = True)

    
    def GetHeldGlideins(self):
        """
        @return: int - number of idle glidein startds
        """
        return self.CountDict(self.GetCondorQ(), GlideinJob = True, JobStatus = 5)
    
