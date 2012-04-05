#
# File for taring up necessary daemons for glidein to run properly
#
# Note, we still need glidein_startup

import tarfile
import logging
import os
import sys
import shutil
import tempfile
from campus_factory.ClusterStatus import CondorConfig


DEFAULT_GLIDEIN_DAEMONS = [ 'condor_master', 'condor_procd', 'condor_startd', \
                            'condor_starter' ]

class DaemonWrangler:
    def __init__(self, config, daemons=None):
        """
        @param daemons: A list of daemons that will be included in the package
        """
        if daemons == None:
            self.daemons = DEFAULT_GLIDEIN_DAEMONS
        else:
            self.daemons = daemons
            
        self.config = config
        self.campus_dir = os.environ['CAMPUSFACTORY_DIR']
            
    def Package(self, name=""): 
        if name == "":
            name = os.path.join(self.campus_dir,"share/glidein_jobs/glideinExec.tar.gz")
        
        # See if the daemons exist
        daemon_paths = self._CheckDaemons()
        if daemon_paths == None:
            logging.error("Unable to read all daemons, not packaging...")
            raise Exception("Unable to check all daemons")
        
        # Copy the daemons to a special directory to tar them
        tmpdir = tempfile.mkdtemp()
        target_dir = os.path.join(tmpdir, "glideinExec")
        os.mkdir(target_dir)
        for daemon_path in daemon_paths:
            shutil.copy(daemon_path, target_dir)
        
        # Add any other files that should go into the tar file
        shutil.copy(os.path.join(self.campus_dir, "share/glidein_jobs/glidein_startup"), target_dir)
        
        tfile = None
        try:
            tfile = tarfile.open(name, mode='w:gz')
            tfile.add(target_dir, arcname="glideinExec")
            tfile.close()
        except IOError, e:
            logging.error("Unable to open package file %s" % name)
            logging.error(str(e))
            shutil.rmtree(tmpdir)
            raise e
        
        # Clean up the temporary file
        shutil.rmtree(tmpdir)
        
        
        
    

    def _CheckDaemons(self):
        """
        Make sure that the daemons that are supposed to be packaged are
        available and readable.
        """
        condor_config = CondorConfig()
        condor_sbin = condor_config.get("SBIN")
        logging.debug("Found SBIN directory = %s" % condor_sbin)
        daemon_paths = []
        for daemon in self.daemons:
            daemon_path = os.path.join(condor_sbin, daemon)
            logging.debug("Looking for daemon at: %s" % daemon_path)
            
            # Look for the daemons in the condor sbin directory.
            if os.path.exists(daemon_path):
                # Try opening the file
                fp = None
                try:
                    fp = open(daemon_path)
                    fp.close()
                    daemon_paths.append(daemon_path)
                except IOError, e:
                    logging.error("Unable to open file: %s" % daemon_path)
                    logging.error(str(e))
                    return None
            else:
                # If the file doesn't exist
                return None
        
        # Done checking all the daemons
        return daemon_paths


